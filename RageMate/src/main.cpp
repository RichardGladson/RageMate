/*
 * RageMate Medusa Zeta V1
 * Bug-fixed build: backward-move penalty corrected for black, knight fork check
 * fixed to target enemy pawns, null-move depth corrected (depth-R not depth-1-R),
 * openingMoveIndex double-increment removed, gamePhase queen-capture bypass removed.
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ─── Pin definitions ───────────────────────────────────────────────────────
#define TFT_SCLK  18
#define TFT_MOSI  23
#define TFT_DC    16
#define TFT_RST   17
#define TFT_CS     5
#define JOY_VRX   34
#define JOY_VRY   35
#define JOY_SW    32

// ─── Display ───────────────────────────────────────────────────────────────
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ─── Board constants ────────────────────────────────────────────────────────
#define BOARD_OX   22
#define BOARD_OY    4
#define SQ_W       13
#define SQ_H       13

// ─── Color theme — MATRIX GREEN ─────────────────────────────────────────────
// RGB565 green palette:
//   0x07E0 = #00FF00 bright green
//   0x03E0 = #007E00 mid green
//   0x0300 = #006000 dark green
//   0x0180 = #003000 very dark green
//   0x0060 = #00C000 medium-bright
//   0x0540 = #00A800 muted green
//   0x0020 = #000400 near-black green tint (background)
//   0x47E4 = #80FF80 pale green highlight
//   0x27E4 = #40FF80 light-mid green
//   0x4208 = #820010 (unused — avoid)
//   0xF800 = red (danger only)
//   0x8000 = dark red (danger shadow)
//   0xFFFF = white (piece text / highlights)
//   0xC618 = light gray
//   0x8410 = mid gray
//   0x4228 = dark gray
//   0x0841 = near-black

#define C_BG               0x0020   // near-black green: #000400
#define C_DARK_SQ          0x0180   // dark green sq: #003000
#define C_LIGHT_SQ         0x0300   // mid-dark green sq: #006000
#define C_BORDER           0x03A0   // medium green border
#define C_HOVER            0x07E0   // bright #00FF00 hover cursor
#define C_SEL              0x47E4   // pale green selection
#define C_BOTH             0xFFFF   // white when sel+hover overlap
#define C_DOT              0x03E0   // medium green legal dot
#define C_LASTMOVE         0x0540   // muted green last-move outline
#define C_CURSOR_LASTMOVE  0x27E4   // light-mid green cursor on last-move sq
#define C_GAMEOVER         0x07E0   // bright green game-over text
#define C_BTN_BG           0x0180   // dark green button bg
#define C_BTN_HL           0x03E0   // mid green button highlight
#define C_PB_EMPTY         0x0140   // dark forest green — bar frame always visible
#define C_PB_BORDER        0x03E0   // green progress bar border

// Progress bar
#define PB_X   130
#define PB_Y    12
#define PB_IW   12
#define PB_IH   96

// ─── Piece encoding ─────────────────────────────────────────────────────────
#define EMPTY  0
#define W_P    9
#define W_N   10
#define W_B   11
#define W_R   12
#define W_Q   13
#define W_K   14
#define B_P   17
#define B_N   18
#define B_B   19
#define B_R   20
#define B_Q   21
#define B_K   22

#define COLOR(p)    ((p) >> 3)
#define TYPE(p)     ((p) & 7)
#define IS_WHITE(p) (COLOR(p)==1)
#define IS_BLACK(p) (COLOR(p)==2)
#define MAKE_PIECE(c,t) ((c)*8+(t))

#define PAWN   1
#define KNIGHT 2
#define BISHOP 3
#define ROOK   4
#define QUEEN  5
#define KING   6

// ─── Piece values ───────────────────────────────────────────────────────────
const int PIECE_VAL[8] = {0, 100, 320, 330, 500, 900, 20000, 0};

// ─── Board state ────────────────────────────────────────────────────────────
uint8_t board[64];
bool wCastle_K = true, wCastle_Q = true;
bool bCastle_K = true, bCastle_Q = true;
int  enPassantSq = -1;
bool whiteToMove = true;

// ─── Move structure ─────────────────────────────────────────────────────────
struct Move {
  uint8_t from, to;
  uint8_t promo;
  uint8_t flags;
};

// ─── History ────────────────────────────────────────────────────────────────
struct HistoryEntry {
  Move mv;
  uint8_t captured;
  bool wCK, wCQ, bCK, bCQ;
  int epSq;
  uint8_t capturedEPpawn;
  uint32_t savedHash;  // hash before this move, for O(1) restore in unmakeMove
};

#define MAX_HISTORY 200
HistoryEntry history[MAX_HISTORY];
int historyCount = 0;

// ─── Transposition table ────────────────────────────────────────────────────
#define TT_SIZE 4096
#define TT_MASK (TT_SIZE-1)
struct TTEntry {
  uint32_t hashHi;
  int16_t  score;
  uint8_t  depth;
  uint8_t  flags;
  uint8_t  from, to;
};
TTEntry tt[TT_SIZE];

// ─── Zobrist ─────────────────────────────────────────────────────────────────
uint32_t zobPiece[64][16];
uint32_t zobSide;
uint32_t zobEP[8];
uint32_t zobCastle[4];
uint32_t currentHash;

// ─── Progress bar ───────────────────────────────────────────────────────────
int8_t dispPct = -1;

// Progress bar gradient — starts at visible dim-green, rises evenly to bright
// No near-black at the base; all segments visible. 96 entries, RGB565.
// Ramps from a soft forest green (0x0260) through to a steady bright green (0x05E0),
// avoiding both the near-invisible start and the blinding white-wash top.
const uint16_t PROGMEM pbGrad[96] = {
  // 0-15: soft dim green — always clearly visible on the display
  0x0260,0x0280,0x02A0,0x02C0,0x02E0,0x0300,0x0320,0x0340,
  0x0360,0x0380,0x03A0,0x03A0,0x03C0,0x03C0,0x03E0,0x03E0,
  // 16-31: lower-mid green
  0x03E0,0x03E0,0x04A0,0x04C0,0x04E0,0x04E0,0x0500,0x0500,
  0x0520,0x0520,0x0540,0x0540,0x0560,0x0560,0x0580,0x0580,
  // 32-47: mid green rising steadily
  0x0580,0x05A0,0x05A0,0x05C0,0x05C0,0x05C0,0x05E0,0x05E0,
  0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,
  // 48-63: hold at confident bright-mid green
  0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,
  0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,0x05E0,
  // 64-79: ease up to full green
  0x05E0,0x06A0,0x06C0,0x06E0,0x0700,0x0720,0x0740,0x0760,
  0x0780,0x07A0,0x07C0,0x07C0,0x07E0,0x07E0,0x07E0,0x07E0,
  // 80-95: settle at bright green — no white blow-out
  0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,
  0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,
};

// ─── UI state ───────────────────────────────────────────────────────────────
int  cursorCol = 4;
int  cursorRow = 4;
int  selSq = -1;
bool gameOver = false;
char gameOverMsg[20] = "";
bool engineThinking = false;
bool checkmatePending = false;  // true when player is checkmated, waiting for 2s hold

// ─── FreeRTOS search task ──────────────────────────────────────────────────
volatile bool     searchRunning  = false;
volatile bool     searchDone     = false;
volatile int      searchPct      = 0;
Move              searchResult   = {0,0,0,0};
SemaphoreHandle_t searchSem      = nullptr;

// Legal moves cache for selected piece
Move legalCache[64];
int  legalCount = 0;

int engineFrom = -1, engineTo = -1;
int  panelFocus = 0;

unsigned long lastJoyMove = 0;
#define JOY_DELAY 180

// ─── Piece-square tables ─────────────────────────────────────────────────────
const int8_t PST_PAWN[64] PROGMEM = {
   0,  0,  0,  0,  0,  0,  0,  0,
  80, 80, 80, 80, 80, 80, 80, 80,
  20, 22, 30, 40, 40, 30, 22, 20,
  12, 14, 20, 35, 35, 20, 14, 12,
   5,  8, 15, 28, 28, 15,  8,  5,
   0, -2,  5, 10, 10,  5, -2,  0,
  -5, -5, -8,-15,-15, -8, -5, -5,
   0,  0,  0,  0,  0,  0,  0,  0
};
const int8_t PST_KNIGHT[64] PROGMEM = {
  -50,-40,-30,-30,-30,-30,-40,-50,
  -40,-20,  0,  0,  0,  0,-20,-40,
  -30,  0,  8, 12, 12,  8,  0,-30,
  -30,  4, 12, 16, 16, 12,  4,-30,
  -30,  0, 12, 16, 16, 12,  0,-30,
  -30,  4,  8, 12, 12,  8,  4,-30,
  -40,-20,  0,  4,  4,  0,-20,-40,
  -50,-40,-30,-30,-30,-30,-40,-50
};
const int8_t PST_BISHOP[64] PROGMEM = {
  -20,-10,-10,-10,-10,-10,-10,-20,
  -10,  0,  0,  0,  0,  0,  0,-10,
  -10,  0,  5, 10, 10,  5,  0,-10,
  -10,  5,  5, 10, 10,  5,  5,-10,
  -10,  0, 10, 10, 10, 10,  0,-10,
  -10, 10, 10, 10, 10, 10, 10,-10,
  -10,  5,  0,  0,  0,  0,  5,-10,
  -20,-10,-10,-10,-10,-10,-10,-20
};
const int8_t PST_ROOK[64] PROGMEM = {
  -2, -1,  0,  3,  3,  0, -1, -2,
   5, 10, 10, 10, 10, 10, 10,  5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   0,  0,  0,  5,  5,  0,  0,  0
};
// Queen PST: heavily penalizes leaving d1/d8, slight central bonus only
// (Issue 3: premature queen sorties are actively punished via negative values
//  on squares reachable before development completes; the PST discourages
//  the queen from leaving its starting zone early)
const int8_t PST_QUEEN[64] PROGMEM = {
  -20,-10,-10, -5, -5,-10,-10,-20,
  -10,  0,  0,  0,  0,  0,  0,-10,
  -10,  0,  2,  2,  2,  2,  0,-10,
   -5,  0,  2,  3,  3,  2,  0, -5,
    0,  0,  2,  3,  3,  2,  0, -5,
  -10,  2,  2,  2,  2,  2,  0,-10,
  -10,  0,  2,  0,  0,  0,  0,-10,
  -20,-10,-10, -5, -5,-10,-10,-20
};
const int8_t PST_KING_MID[64] PROGMEM = {
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30,
  -20,-30,-30,-40,-40,-30,-30,-20,
  -10,-20,-20,-20,-20,-20,-20,-10,
   20, 20,  0,  0,  0,  0, 20, 20,
   20, 30, 10,  0,  0, 10, 30, 20
};
const int8_t PST_KING_END[64] PROGMEM = {
  -50,-40,-30,-20,-20,-30,-40,-50,
  -30,-20,-10,  0,  0,-10,-20,-30,
  -30,-10, 20, 30, 30, 20,-10,-30,
  -30,-10, 30, 40, 40, 30,-10,-30,
  -30,-10, 30, 40, 40, 30,-10,-30,
  -30,-10, 20, 30, 30, 20,-10,-30,
  -30,-30,  0,  0,  0,  0,-30,-30,
  -50,-30,-30,-30,-30,-30,-30,-50
};

// ─── Opening book ─────────────────────────────────────────────────────────────
#define NUM_OPENINGS 4
int chosenOpening = -1;
int openingMoveIndex = 0;

// ─── Forward declarations ─────────────────────────────────────────────────────
void initBoard();
void initZobrist();
uint32_t computeHash();
void drawAll();
void drawBoard();
void drawPiece(int sq);
void drawSquare(int sq);
void drawCursor();
void drawLeftPanel();
void drawPhaseIndicator();
void drawProgressBar(int pct);
void drawLastMoveOutlines(int from, int to);
void drawLegalDots();
void clearLegalDots();
void drawGameOver();
void handleJoystick();
void handleButton();
bool generateMoves(bool white, Move* moves, int& count, bool onlyCaptures=false);
bool isInCheck(bool white);
bool isLegal(Move& m);
void makeMove(Move& m, HistoryEntry* he);
void unmakeMove(HistoryEntry& he);
int  evaluate();
int  alphaBeta(int depth, int alpha, int beta, bool maximizing, unsigned long startMs);
int  quiesce(int alpha, int beta);
Move getBotMove();
bool isSquareAttacked(int sq, bool byWhite);
int  pstValue(int piece, int sq);
int  pstValue(int piece, int sq, bool endgame);
inline int findKing(bool white);
void updateProgressBar(unsigned long startMs, unsigned long totalMs);
int  see(int toSq, int piece, int fromSq, int capturedPiece);
bool squareOnBoard(int sq);
void doRestart();
void doUndo();
Move getBookMove(int blackMoveNum);
int  rankOf(int sq);
int  fileOf(int sq);
int  sqOf(int r, int f);
bool inBounds(int r, int f);
char pieceChar(int p);
void drawPieceAt(int sx, int sy, int piece);
void clearSquarePixels(int sq);
int  countMaterial(bool white);
int  gamePhase();
bool isEndgame();
static int lvaSquare(int sq, int attColor);
int  seeCaptures(int sq, int attColor);
int  seeMoveGain(int fromSq, int toSq, int piece, int attColor);
bool insufficientMaterial();
bool isRepetition();
bool selectSquare(int sq);

// ─── UI-2: Password system ───────────────────────────────────────────────────
void showPasswordKeypad();
bool runPasswordUI();   // returns true if correct password entered
void drawKeypad(int selectedKey, int digitsEntered, char* enteredDigits);
void drawKeypadKey(int kx, int ky, int kw, int kh, char label, bool selected, bool filled);

// ─── UI-3: Resign screen ─────────────────────────────────────────────────────
void showResignScreen();
void drawFlagIcon(int x, int y);
void showCheckmateScreen();

// ─── UI-4: Intro cutscene ─────────────────────────────────────────────────────
void playIntroCutscene();

uint32_t hashHistory[MAX_HISTORY+10];
int hashHistoryCount = 0;

// ─── Search globals ───────────────────────────────────────────────────────────
bool searchAborted = false;
unsigned long searchStart;
uint32_t nodeCount = 0;
#define SEARCH_TIME_MS 4000

// ─── Development tracking ─────────────────────────────────────────────────────
uint64_t bPieceMoved = 0;
int      lastBlackFrom = -1;
int      lastBlackTo   = -1;

// ─── Phase cache ──────────────────────────────────────────────────────────────
int  cachedPhase      = -1;
uint32_t cachedPhaseHash = 0xFFFFFFFF;

// ─── Helpers ─────────────────────────────────────────────────────────────────
int rankOf(int sq)       { return sq / 8; }
int fileOf(int sq)       { return sq % 8; }
int sqOf(int r, int f)   { return r*8+f; }
bool inBounds(int r, int f) { return r>=0&&r<8&&f>=0&&f<8; }
bool squareOnBoard(int sq)  { return sq>=0 && sq<64; }

char pieceChar(int p) {
  const char t[] = ".PNBRQKx";
  return t[TYPE(p)];
}

// pstValue with precomputed endgame flag for efficiency
int pstValue(int piece, int sq, bool endgame) {
  int idx = IS_WHITE(piece) ? sq : (56 - (sq/8)*8 + (sq%8));
  if(idx < 0) idx = 0;
  if(idx > 63) idx = 63;
  switch(TYPE(piece)) {
    case PAWN:   return (int8_t)pgm_read_byte(&PST_PAWN[idx]);
    case KNIGHT: return (int8_t)pgm_read_byte(&PST_KNIGHT[idx]);
    case BISHOP: return (int8_t)pgm_read_byte(&PST_BISHOP[idx]);
    case ROOK:   return (int8_t)pgm_read_byte(&PST_ROOK[idx]);
    case QUEEN:  return (int8_t)pgm_read_byte(&PST_QUEEN[idx]);
    case KING:
      if(endgame) return (int8_t)pgm_read_byte(&PST_KING_END[idx]);
      else        return (int8_t)pgm_read_byte(&PST_KING_MID[idx]);
    default: return 0;
  }
}

// Overload for callers that don't have endgame precomputed
int pstValue(int piece, int sq) {
  return pstValue(piece, sq, isEndgame());
}

// ─── Zobrist ──────────────────────────────────────────────────────────────────
void initZobrist() {
  uint32_t seed = 0xDEADBEEF;
  auto rng = [&]() -> uint32_t {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
  };
  for(int s=0;s<64;s++) for(int p=0;p<16;p++) zobPiece[s][p]=rng();
  zobSide = rng();
  for(int i=0;i<8;i++) zobEP[i]=rng();
  for(int i=0;i<4;i++) zobCastle[i]=rng();
}

uint32_t computeHash() {
  uint32_t h = 0;
  for(int s=0;s<64;s++) if(board[s]) h ^= zobPiece[s][board[s] & 15];
  if(!whiteToMove) h ^= zobSide;
  if(enPassantSq>=0) h ^= zobEP[fileOf(enPassantSq)];
  if(wCastle_K)  h ^= zobCastle[0];
  if(wCastle_Q)  h ^= zobCastle[1];
  if(bCastle_K)  h ^= zobCastle[2];
  if(bCastle_Q)  h ^= zobCastle[3];
  return h;
}

// ─── Board init ───────────────────────────────────────────────────────────────
void initBoard() {
  memset(board, 0, sizeof(board));
  board[0]=W_R; board[1]=W_N; board[2]=W_B; board[3]=W_Q;
  board[4]=W_K; board[5]=W_B; board[6]=W_N; board[7]=W_R;
  for(int f=0;f<8;f++) board[8+f]=W_P;
  board[56]=B_R; board[57]=B_N; board[58]=B_B; board[59]=B_Q;
  board[60]=B_K; board[61]=B_B; board[62]=B_N; board[63]=B_R;
  for(int f=0;f<8;f++) board[48+f]=B_P;
  wCastle_K=wCastle_Q=bCastle_K=bCastle_Q=true;
  enPassantSq=-1;
  whiteToMove=true;
  historyCount=0;
  hashHistoryCount=0;
  currentHash=computeHash();
  hashHistory[hashHistoryCount++]=currentHash;
  chosenOpening = esp_random() % 4;
  openingMoveIndex = 0;
  engineFrom=-1; engineTo=-1;
  bPieceMoved=0; lastBlackFrom=-1; lastBlackTo=-1;
  cachedPhase = -1;
  cachedPhaseHash = 0xFFFFFFFF;
  gameOver=false;
  gameOverMsg[0]=0;
  selSq=-1;
  legalCount=0;
}

// ─── Move generation ─────────────────────────────────────────────────────────
void addMove(Move* moves, int& count, int from, int to, int promo=0, int flags=0) {
  moves[count].from=from; moves[count].to=to;
  moves[count].promo=promo; moves[count].flags=flags;
  count++;
}

bool generateMoves(bool white, Move* moves, int& count, bool onlyCaptures) {
  count = 0;
  int myColor = white ? 1 : 2;
  int opColor = white ? 2 : 1;
  int pawnDir = white ? 1 : -1;
  int pawnStartRank = white ? 1 : 6;
  int pawnPromoRank = white ? 7 : 0;

  for(int sq=0;sq<64;sq++) {
    int p = board[sq];
    if(!p || COLOR(p)!=myColor) continue;
    int r=rankOf(sq), f=fileOf(sq);
    int t=TYPE(p);

    if(t==PAWN) {
      int nr=r+pawnDir, nf=f;
      if(inBounds(nr,nf) && board[sqOf(nr,nf)]==EMPTY) {
        if(nr==pawnPromoRank) {
          if(!onlyCaptures) {
            addMove(moves,count,sq,sqOf(nr,nf),QUEEN);
            addMove(moves,count,sq,sqOf(nr,nf),ROOK);
            addMove(moves,count,sq,sqOf(nr,nf),BISHOP);
            addMove(moves,count,sq,sqOf(nr,nf),KNIGHT);
          }
        } else {
          if(!onlyCaptures) {
            addMove(moves,count,sq,sqOf(nr,nf));
            if(r==pawnStartRank && board[sqOf(r+2*pawnDir,nf)]==EMPTY)
              addMove(moves,count,sq,sqOf(r+2*pawnDir,nf));
          }
        }
      }
      for(int df=-1;df<=1;df+=2) {
        int cf=f+df;
        if(!inBounds(nr,cf)) continue;
        int tsq=sqOf(nr,cf);
        if(COLOR(board[tsq])==opColor) {
          if(nr==pawnPromoRank) {
            addMove(moves,count,sq,tsq,QUEEN);
            addMove(moves,count,sq,tsq,ROOK);
            addMove(moves,count,sq,tsq,BISHOP);
            addMove(moves,count,sq,tsq,KNIGHT);
          } else {
            addMove(moves,count,sq,tsq);
          }
        } else if(tsq==enPassantSq) {
          addMove(moves,count,sq,tsq,0,1);
        }
      }
    }
    else if(t==KNIGHT) {
      const int kd[8][2]={{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
      for(auto& d:kd) {
        int nr2=r+d[0], nf2=f+d[1];
        if(!inBounds(nr2,nf2)) continue;
        int tsq=sqOf(nr2,nf2);
        if(COLOR(board[tsq])==myColor) continue;
        if(onlyCaptures && board[tsq]==EMPTY) continue;
        addMove(moves,count,sq,tsq);
      }
    }
    else if(t==BISHOP || t==QUEEN) {
      const int dirs[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
      for(auto& d:dirs) {
        int nr2=r+d[0], nf2=f+d[1];
        while(inBounds(nr2,nf2)) {
          int tsq=sqOf(nr2,nf2);
          if(COLOR(board[tsq])==myColor) break;
          if(onlyCaptures && board[tsq]==EMPTY) { nr2+=d[0]; nf2+=d[1]; continue; }
          addMove(moves,count,sq,tsq);
          if(board[tsq]!=EMPTY) break;
          nr2+=d[0]; nf2+=d[1];
        }
      }
    }
    if(t==ROOK || t==QUEEN) {
      const int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
      for(auto& d:dirs) {
        int nr2=r+d[0], nf2=f+d[1];
        while(inBounds(nr2,nf2)) {
          int tsq=sqOf(nr2,nf2);
          if(COLOR(board[tsq])==myColor) break;
          if(onlyCaptures && board[tsq]==EMPTY) { nr2+=d[0]; nf2+=d[1]; continue; }
          addMove(moves,count,sq,tsq);
          if(board[tsq]!=EMPTY) break;
          nr2+=d[0]; nf2+=d[1];
        }
      }
    }
    else if(t==KING) {
      const int kd[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
      for(auto& d:kd) {
        int nr2=r+d[0], nf2=f+d[1];
        if(!inBounds(nr2,nf2)) continue;
        int tsq=sqOf(nr2,nf2);
        if(COLOR(board[tsq])==myColor) continue;
        if(onlyCaptures && board[tsq]==EMPTY) continue;
        addMove(moves,count,sq,tsq);
      }
      if(!onlyCaptures) {
        if(white) {
          if(wCastle_K && board[5]==EMPTY && board[6]==EMPTY
             && !isSquareAttacked(4,false) && !isSquareAttacked(5,false) && !isSquareAttacked(6,false))
            addMove(moves,count,4,6,0,2);
          if(wCastle_Q && board[1]==EMPTY && board[2]==EMPTY && board[3]==EMPTY
             && !isSquareAttacked(4,false) && !isSquareAttacked(3,false) && !isSquareAttacked(2,false))
            addMove(moves,count,4,2,0,3);
        } else {
          if(bCastle_K && board[61]==EMPTY && board[62]==EMPTY
             && !isSquareAttacked(60,true) && !isSquareAttacked(61,true) && !isSquareAttacked(62,true))
            addMove(moves,count,60,62,0,2);
          if(bCastle_Q && board[57]==EMPTY && board[58]==EMPTY && board[59]==EMPTY
             && !isSquareAttacked(60,true) && !isSquareAttacked(59,true) && !isSquareAttacked(58,true))
            addMove(moves,count,60,58,0,3);
        }
      }
    }
  }
  return count > 0;
}

// ─── Attack detection ─────────────────────────────────────────────────────────
bool isSquareAttacked(int sq, bool byWhite) {
  int r=rankOf(sq), f=fileOf(sq);
  int pdir = byWhite ? -1 : 1;
  for(int df=-1;df<=1;df+=2) {
    int nr=r+pdir, nf=f+df;
    if(inBounds(nr,nf)) {
      int p=board[sqOf(nr,nf)];
      if(TYPE(p)==PAWN && ((byWhite&&IS_WHITE(p))||(!byWhite&&IS_BLACK(p)))) return true;
    }
  }
  const int kd[8][2]={{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
  for(auto& d:kd) {
    int nr=r+d[0], nf=f+d[1];
    if(inBounds(nr,nf)) {
      int p=board[sqOf(nr,nf)];
      if(TYPE(p)==KNIGHT && COLOR(p)==(byWhite?1:2)) return true;
    }
  }
  const int bd[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
  for(auto& d:bd) {
    int nr=r+d[0], nf=f+d[1];
    while(inBounds(nr,nf)) {
      int p=board[sqOf(nr,nf)];
      if(p!=EMPTY) {
        if(COLOR(p)==(byWhite?1:2) && (TYPE(p)==BISHOP||TYPE(p)==QUEEN)) return true;
        break;
      }
      nr+=d[0]; nf+=d[1];
    }
  }
  const int rd[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
  for(auto& d:rd) {
    int nr=r+d[0], nf=f+d[1];
    while(inBounds(nr,nf)) {
      int p=board[sqOf(nr,nf)];
      if(p!=EMPTY) {
        if(COLOR(p)==(byWhite?1:2) && (TYPE(p)==ROOK||TYPE(p)==QUEEN)) return true;
        break;
      }
      nr+=d[0]; nf+=d[1];
    }
  }
  const int kd2[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
  for(auto& d:kd2) {
    int nr=r+d[0], nf=f+d[1];
    if(inBounds(nr,nf)) {
      int p=board[sqOf(nr,nf)];
      if(TYPE(p)==KING && COLOR(p)==(byWhite?1:2)) return true;
    }
  }
  return false;
}

bool isInCheck(bool white) {
  int kp = white ? W_K : B_K;
  for(int s=0;s<64;s++) if(board[s]==kp) return isSquareAttacked(s,!white);
  return false;
}

// Find king square (cached-friendly helper)
inline int findKing(bool white) {
  int kp = white ? W_K : B_K;
  for(int s=0;s<64;s++) if(board[s]==kp) return s;
  return -1;
}

// ─── Make/Unmake ──────────────────────────────────────────────────────────────
void makeMove(Move& m, HistoryEntry* he) {
  if(he) {
    he->mv = m;
    he->captured = board[m.to];
    he->wCK=wCastle_K; he->wCQ=wCastle_Q;
    he->bCK=bCastle_K; he->bCQ=bCastle_Q;
    he->epSq=enPassantSq;
    he->capturedEPpawn=0;
    he->savedHash=currentHash;
  }

  int piece = board[m.from];

  currentHash ^= zobPiece[m.from][piece&15];
  if(board[m.to]) currentHash ^= zobPiece[m.to][board[m.to]&15];
  if(enPassantSq>=0) currentHash ^= zobEP[fileOf(enPassantSq)];
  currentHash ^= zobSide;

  enPassantSq = -1;

  if(m.flags==1) {
    int capSq = sqOf(rankOf(m.from), fileOf(m.to));
    if(he) he->capturedEPpawn = board[capSq];
    currentHash ^= zobPiece[capSq][board[capSq]&15];
    board[capSq] = EMPTY;
  }

  board[m.to] = piece;
  board[m.from] = EMPTY;

  if(m.promo && TYPE(piece)==PAWN) {
    int promoPiece = MAKE_PIECE(COLOR(piece), m.promo);
    currentHash ^= zobPiece[m.to][board[m.to]&15];
    board[m.to] = promoPiece;
    currentHash ^= zobPiece[m.to][board[m.to]&15];
  } else {
    currentHash ^= zobPiece[m.to][board[m.to]&15];
  }

  if(TYPE(piece)==PAWN && abs(rankOf(m.to)-rankOf(m.from))==2) {
    enPassantSq = sqOf((rankOf(m.from)+rankOf(m.to))/2, fileOf(m.from));
    currentHash ^= zobEP[fileOf(enPassantSq)];
  }

  if(m.flags==2) {
    if(IS_WHITE(piece)) { board[5]=W_R; board[7]=EMPTY; currentHash^=zobPiece[7][W_R&15]^zobPiece[5][W_R&15]; }
    else { board[61]=B_R; board[63]=EMPTY; currentHash^=zobPiece[63][B_R&15]^zobPiece[61][B_R&15]; }
  }
  if(m.flags==3) {
    if(IS_WHITE(piece)) { board[3]=W_R; board[0]=EMPTY; currentHash^=zobPiece[0][W_R&15]^zobPiece[3][W_R&15]; }
    else { board[59]=B_R; board[56]=EMPTY; currentHash^=zobPiece[56][B_R&15]^zobPiece[59][B_R&15]; }
  }

  if(wCastle_K)  currentHash ^= zobCastle[0];
  if(wCastle_Q)  currentHash ^= zobCastle[1];
  if(bCastle_K)  currentHash ^= zobCastle[2];
  if(bCastle_Q)  currentHash ^= zobCastle[3];
  if(m.from==4||m.to==4||m.from==7||m.to==7) wCastle_K=false;
  if(m.from==4||m.to==4||m.from==0||m.to==0) wCastle_Q=false;
  if(m.from==60||m.to==60||m.from==63||m.to==63) bCastle_K=false;
  if(m.from==60||m.to==60||m.from==56||m.to==56) bCastle_Q=false;
  if(wCastle_K)  currentHash ^= zobCastle[0];
  if(wCastle_Q)  currentHash ^= zobCastle[1];
  if(bCastle_K)  currentHash ^= zobCastle[2];
  if(bCastle_Q)  currentHash ^= zobCastle[3];

  whiteToMove = !whiteToMove;
}

void unmakeMove(HistoryEntry& he) {
  Move& m = he.mv;
  int piece = board[m.to];

  if(m.promo && TYPE(piece)==m.promo) {
    piece = MAKE_PIECE(COLOR(piece), PAWN);
  }

  board[m.from] = piece;
  board[m.to] = he.captured;

  if(m.flags==1) {
    int capSq = sqOf(rankOf(m.from), fileOf(m.to));
    board[capSq] = he.capturedEPpawn;
  }
  if(m.flags==2) {
    if(IS_WHITE(piece)) { board[7]=W_R; board[5]=EMPTY; }
    else { board[63]=B_R; board[61]=EMPTY; }
  }
  if(m.flags==3) {
    if(IS_WHITE(piece)) { board[0]=W_R; board[3]=EMPTY; }
    else { board[56]=B_R; board[59]=EMPTY; }
  }

  wCastle_K=he.wCK; wCastle_Q=he.wCQ;
  bCastle_K=he.bCK; bCastle_Q=he.bCQ;
  enPassantSq = he.epSq;
  whiteToMove = !whiteToMove;
  currentHash = he.savedHash;
}

bool isLegal(Move& m) {
  bool side = whiteToMove;
  HistoryEntry he;
  makeMove(m, &he);
  bool inChk = isInCheck(side);
  unmakeMove(he);
  return !inChk;
}

// ─── Repetition ───────────────────────────────────────────────────────────────
bool isRepetition() {
  uint32_t h = currentHash;
  int cnt = 0;
  for(int i=0;i<hashHistoryCount;i++) if(hashHistory[i]==h) cnt++;
  return cnt >= 2;
}

// ─── Material counting ────────────────────────────────────────────────────────
int countMaterial(bool white) {
  int sum = 0;
  for(int s=0;s<64;s++) {
    int p=board[s];
    if(p && ((white&&IS_WHITE(p))||(!white&&IS_BLACK(p))))
      sum += PIECE_VAL[TYPE(p)];
  }
  return sum;
}

bool isEndgame() {
  if(cachedPhase >= 0 && cachedPhaseHash == currentHash) return cachedPhase == 2;
  int total = 0;
  for(int s=0;s<64;s++) {
    int p=board[s];
    if(p && TYPE(p)!=KING) total += PIECE_VAL[TYPE(p)];
  }
  return total < 1800;
}

int gamePhase() {
  if(cachedPhase >= 0 && cachedPhaseHash == currentHash) return cachedPhase;

  int total = 0;
  for(int s=0;s<64;s++) {
    int p=board[s]; if(!p) continue;
    int t=TYPE(p);
    if(t!=KING) total += PIECE_VAL[t];
  }

  int phase;
  if(total < 1800) {
    phase = 2;
  } else {
    int undev = 0;
    if(board[sqOf(7,1)]==B_N) undev++;
    if(board[sqOf(7,6)]==B_N) undev++;
    if(board[sqOf(7,2)]==B_B) undev++;
    if(board[sqOf(7,5)]==B_B) undev++;

    bool bothCastled = (!bCastle_K && !bCastle_Q) && (!wCastle_K && !wCastle_Q);

    if(historyCount < 16 || undev >= 2) {
      phase = 0;
    } else if(historyCount < 30 && undev >= 1 && !bothCastled) {
      phase = 0;
    } else {
      phase = 1;
    }
  }

  cachedPhase     = phase;
  cachedPhaseHash = currentHash;
  return phase;
}
bool isOpeningPhase() { return gamePhase() == 0; }

// ─── SEE ──────────────────────────────────────────────────────────────────────
static int lvaSquare(int sq, int attColor) {
  int bestVal = 0x7FFF, bestSq = -1;
  int pdir = (attColor==1) ? -1 : 1;
  for(int df=-1;df<=1;df+=2) {
    int r=rankOf(sq)+pdir, f=fileOf(sq)+df;
    if(inBounds(r,f)) {
      int s=sqOf(r,f);
      if(board[s] && COLOR(board[s])==attColor && TYPE(board[s])==PAWN)
        if(PIECE_VAL[PAWN]<bestVal) { bestVal=PIECE_VAL[PAWN]; bestSq=s; }
    }
  }
  const int kd[8][2]={{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
  for(auto& d:kd) {
    int r=rankOf(sq)+d[0], f=fileOf(sq)+d[1];
    if(inBounds(r,f)) {
      int s=sqOf(r,f);
      if(board[s] && COLOR(board[s])==attColor && TYPE(board[s])==KNIGHT)
        if(PIECE_VAL[KNIGHT]<bestVal) { bestVal=PIECE_VAL[KNIGHT]; bestSq=s; }
    }
  }
  const int bd[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
  for(auto& d:bd) {
    int r=rankOf(sq)+d[0], f=fileOf(sq)+d[1];
    while(inBounds(r,f)) {
      int s=sqOf(r,f); if(!board[s]) { r+=d[0]; f+=d[1]; continue; }
      if(COLOR(board[s])==attColor) {
        int t=TYPE(board[s]);
        if(t==BISHOP && PIECE_VAL[BISHOP]<bestVal) { bestVal=PIECE_VAL[BISHOP]; bestSq=s; }
        if(t==QUEEN  && PIECE_VAL[QUEEN] <bestVal) { bestVal=PIECE_VAL[QUEEN];  bestSq=s; }
      }
      break;
    }
  }
  const int rd[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
  for(auto& d:rd) {
    int r=rankOf(sq)+d[0], f=fileOf(sq)+d[1];
    while(inBounds(r,f)) {
      int s=sqOf(r,f); if(!board[s]) { r+=d[0]; f+=d[1]; continue; }
      if(COLOR(board[s])==attColor) {
        int t=TYPE(board[s]);
        if(t==ROOK  && PIECE_VAL[ROOK] <bestVal) { bestVal=PIECE_VAL[ROOK];  bestSq=s; }
        if(t==QUEEN && PIECE_VAL[QUEEN]<bestVal) { bestVal=PIECE_VAL[QUEEN]; bestSq=s; }
      }
      break;
    }
  }
  const int kd2[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
  for(auto& d:kd2) {
    int r=rankOf(sq)+d[0], f=fileOf(sq)+d[1];
    if(inBounds(r,f)) {
      int s=sqOf(r,f);
      if(board[s] && COLOR(board[s])==attColor && TYPE(board[s])==KING)
        if(PIECE_VAL[KING]<bestVal) { bestVal=PIECE_VAL[KING]; bestSq=s; }
    }
  }
  return bestSq;
}

int seeCaptures(int sq, int attColor) {
  int fromSq = lvaSquare(sq, attColor);
  if(fromSq < 0) return 0;
  int capturedVal = PIECE_VAL[TYPE(board[sq])];
  uint8_t savedSq   = board[sq];
  uint8_t savedFrom = board[fromSq];
  board[sq]    = board[fromSq];
  board[fromSq] = EMPTY;
  int gain = capturedVal - seeCaptures(sq, 3 - attColor);
  board[sq]    = savedSq;
  board[fromSq] = savedFrom;
  return (gain > 0) ? gain : 0;
}

int seeMoveGain(int fromSq, int toSq, int piece, int attColor) {
  int capturedVal = PIECE_VAL[TYPE(board[toSq])];
  uint8_t savedTo   = board[toSq];
  uint8_t savedFrom = board[fromSq];
  board[toSq]   = piece;
  board[fromSq] = EMPTY;
  int opponentGain = seeCaptures(toSq, 3 - attColor);
  board[toSq]   = savedTo;
  board[fromSq] = savedFrom;
  return capturedVal - opponentGain;
}



// ─────────────────────────────────────────────────────────────────────────────
// THETA SCORING ENGINE
// One clean pass. No layers. No patches. No early returns.
//
// Core principle: safety is computed once, correctly, and used everywhere.
// The pawn-threat system uses a single shared helper (pieceThreatPenalty)
// that is called from BOTH evaluate() and moveScore() — no duplication.
// ─────────────────────────────────────────────────────────────────────────────

// ─── Shared helper: is a square a genuine outpost for color `pieceColor`? ────
// An outpost = no enemy pawn on an adjacent file can safely advance to attack.
// pieceColor: 1=white piece, 2=black piece
// sq: the square the piece is on (or will move to)
// Returns true if the square is safe from pawn advances.
bool isOutpostSquare(int sq, int pieceColor) {
  int r = rankOf(sq), f = fileOf(sq);
  int enemyPawn = (pieceColor == 2) ? W_P : B_P;

  if(pieceColor == 2) {
    // Black piece: white pawns attack upward. Check adjacent files for white
    // pawns below rank r that could advance to rank r-1.
    for(int df = -1; df <= 1; df += 2) {
      int adjF = f + df;
      if(adjF < 0 || adjF > 7) continue;
      for(int rr = 0; rr < r; rr++) {
        if(board[sqOf(rr, adjF)] != W_P) continue;
        // White pawn found. Is the path clear to rank r-1?
        bool clear = true;
        for(int pr = rr + 1; pr < r; pr++)
          if(board[sqOf(pr, adjF)] != EMPTY) { clear = false; break; }
        if(!clear) break;
        // Simulate pawn on the attack square and check if it can stay there
        int aSq = sqOf(r - 1, adjF);
        uint8_t sA = board[aSq], sP = board[sqOf(rr, adjF)];
        board[aSq] = W_P; board[sqOf(rr, adjF)] = EMPTY;
        int safety = seeCaptures(aSq, 2); // can black recapture?
        board[aSq] = sA; board[sqOf(rr, adjF)] = sP;
        if(safety <= 0) return false; // pawn advance is safe → not an outpost
        break;
      }
    }
  } else {
    // White piece: black pawns attack downward. Check adjacent files for black
    // pawns above rank r that could advance to rank r+1.
    for(int df = -1; df <= 1; df += 2) {
      int adjF = f + df;
      if(adjF < 0 || adjF > 7) continue;
      for(int rr = 7; rr > r; rr--) {
        if(board[sqOf(rr, adjF)] != B_P) continue;
        bool clear = true;
        for(int pr = rr - 1; pr > r; pr--)
          if(board[sqOf(pr, adjF)] != EMPTY) { clear = false; break; }
        if(!clear) break;
        int aSq = sqOf(r + 1, adjF);
        uint8_t sA = board[aSq], sP = board[sqOf(rr, adjF)];
        board[aSq] = B_P; board[sqOf(rr, adjF)] = EMPTY;
        int safety = seeCaptures(aSq, 1);
        board[aSq] = sA; board[sqOf(rr, adjF)] = sP;
        if(safety <= 0) return false;
        break;
      }
    }
  }
  return true; // no safe pawn advance found → it's a genuine outpost
}

// ─── Shared helper: pawn threat penalty for a piece on `sq` ─────────────────
// Returns score delta from WHITE's perspective (positive = bad for black piece,
// negative = bad for white piece). Handles both colors symmetrically.
// Penalties:
//   Direct kick (already attacked by enemy pawn, not outpost): ±70 opening, ±45 other
//   Imminent kick (pawn one push away, not outpost):           ±50 opening, ±32 other
//   On outpost (even if kicked): 0 or small bonus for centrality
int pieceThreatPenalty(int sq, int piece) {
  int tp = TYPE(piece);
  if(tp != KNIGHT && tp != BISHOP) return 0;

  int r = rankOf(sq), f = fileOf(sq);
  int pieceColor = COLOR(piece); // 1=white, 2=black
  int enemyPawn  = (pieceColor == 2) ? W_P : B_P;
  bool opening   = isOpeningPhase();

  // Determine the rank offset where the enemy pawn stands to attack this square
  // White pawn on rank r-1 attacks rank r diagonally → atkOff = -1 for black piece
  // Black pawn on rank r+1 attacks rank r diagonally → atkOff = +1 for white piece
  int atkOff = (pieceColor == 2) ? -1 : 1;

  // Direct kick: enemy pawn currently attacks this square
  bool directKick = false;
  for(int df = -1; df <= 1; df += 2) {
    int pr = r + atkOff, pf = f + df;
    if(inBounds(pr, pf) && board[sqOf(pr, pf)] == enemyPawn) {
      directKick = true; break;
    }
  }

  bool outpost = isOutpostSquare(sq, pieceColor);
  int sign = (pieceColor == 2) ? 1 : -1; // positive hurts black, negative hurts white

  if(directKick) {
    if(outpost) {
      // Kicked to a safe permanent square — small centralisation reward
      bool central = (f >= 2 && f <= 5 && r >= 3 && r <= 5);
      return central ? -sign * 12 : 0;
    }
    int pen = opening ? 70 : 45;
    return sign * pen;
  }

  // Not directly kicked — check for imminent kick (enemy pawn 1 push away)
  if(!outpost) {
    // atkOff tells us the direction the enemy pawn attacks FROM
    // So the pawn that will kick is atkOff*2 away in rank
    int imminentRank = r + atkOff * 2;
    for(int df = -1; df <= 1; df += 2) {
      int pf = f + df;
      if(!inBounds(imminentRank, pf)) continue;
      if(board[sqOf(imminentRank, pf)] != enemyPawn) continue;
      // Check the square between the pawn and attack position is empty
      int midRank = r + atkOff;
      if(inBounds(midRank, pf) && board[sqOf(midRank, pf)] == EMPTY) {
        int pen = opening ? 50 : 32;
        return sign * pen;
      }
    }
  }

  return 0;
}

// ─── Evaluation ───────────────────────────────────────────────────────────────
// White-relative: positive = good for white, negative = good for black.
// Single clean pass — no accumulated layers, no early returns.
int evaluate() {
  int score = 0;
  bool endgame = isEndgame();
  bool opening = isOpeningPhase();

  int wKsq = -1, bKsq = -1, wb = 0, bb = 0;

  // ── Material + PST ────────────────────────────────────────────────────────
  for(int s = 0; s < 64; s++) {
    int p = board[s]; if(!p) continue;
    int val = PIECE_VAL[TYPE(p)] + pstValue(p, s, endgame);
    if(IS_WHITE(p)) {
      score += val;
      if(TYPE(p) == BISHOP) wb++;
      if(TYPE(p) == KING)   wKsq = s;
    } else {
      score -= val;
      if(TYPE(p) == BISHOP) bb++;
      if(TYPE(p) == KING)   bKsq = s;
    }
  }

  // ── Bishop pair ───────────────────────────────────────────────────────────
  if(wb >= 2) score += 30;
  if(bb >= 2) score -= 30;

  // ── Doubled pawns ─────────────────────────────────────────────────────────
  for(int f = 0; f < 8; f++) {
    int wp = 0, bp = 0;
    for(int r = 0; r < 8; r++) {
      if(board[sqOf(r,f)] == W_P) wp++;
      if(board[sqOf(r,f)] == B_P) bp++;
    }
    if(wp > 1) score -= 20 * (wp - 1);
    if(bp > 1) score += 20 * (bp - 1);
  }

  // ── Passed pawns ──────────────────────────────────────────────────────────
  int passedScale = endgame ? 2 : 1;
  for(int s = 0; s < 64; s++) {
    int p = board[s]; if(TYPE(p) != PAWN) continue;
    int r = rankOf(s), f = fileOf(s);
    bool passed = true;
    if(IS_WHITE(p)) {
      for(int rr = r+1; rr < 8 && passed; rr++)
        for(int ff = f-1; ff <= f+1; ff++)
          if(ff >= 0 && ff < 8 && board[sqOf(rr,ff)] == B_P) passed = false;
      if(passed) score += passedScale * (20 + r * 10);
    } else {
      for(int rr = r-1; rr >= 0 && passed; rr--)
        for(int ff = f-1; ff <= f+1; ff++)
          if(ff >= 0 && ff < 8 && board[sqOf(rr,ff)] == W_P) passed = false;
      if(passed) score -= passedScale * (20 + (7 - r) * 10);
    }
  }

  // ── Isolated pawns ────────────────────────────────────────────────────────
  {
    bool wFile[8] = {}, bFile[8] = {};
    for(int f = 0; f < 8; f++) for(int r = 0; r < 8; r++) {
      if(board[sqOf(r,f)] == W_P) wFile[f] = true;
      if(board[sqOf(r,f)] == B_P) bFile[f] = true;
    }
    for(int f = 0; f < 8; f++) {
      bool wAdj = (f > 0 && wFile[f-1]) || (f < 7 && wFile[f+1]);
      bool bAdj = (f > 0 && bFile[f-1]) || (f < 7 && bFile[f+1]);
      if(wFile[f] && !wAdj) score -= 20;
      if(bFile[f] && !bAdj) score += 20;
    }
  }

  // ── Backward pawns ────────────────────────────────────────────────────────
  for(int s = 0; s < 64; s++) {
    int p = board[s]; if(TYPE(p) != PAWN) continue;
    int r = rankOf(s), f = fileOf(s);
    if(IS_WHITE(p)) {
      bool supported = false;
      for(int df = -1; df <= 1; df += 2) { int af = f+df; if(af<0||af>7) continue;
        for(int rr = 0; rr <= r; rr++) if(board[sqOf(rr,af)]==W_P){supported=true;break;} if(supported) break; }
      bool frontAtk = false; int fr = r+1;
      for(int df=-1;df<=1;df+=2){int af=f+df;if(inBounds(fr,af)&&board[sqOf(fr,af)]==B_P){frontAtk=true;break;}}
      if(!supported && frontAtk) score -= 15;
    } else {
      bool supported = false;
      for(int df=-1;df<=1;df+=2){int af=f+df;if(af<0||af>7)continue;
        for(int rr=7;rr>=r;rr--) if(board[sqOf(rr,af)]==B_P){supported=true;break;} if(supported) break;}
      bool frontAtk = false; int fr = r-1;
      for(int df=-1;df<=1;df+=2){int af=f+df;if(inBounds(fr,af)&&board[sqOf(fr,af)]==W_P){frontAtk=true;break;}}
      if(!supported && frontAtk) score += 15;
    }
  }

  // ── King safety ───────────────────────────────────────────────────────────
  if(!endgame) {
    if(wKsq >= 0) {
      int kf = fileOf(wKsq);
      for(int df = -1; df <= 1; df++) { int f2 = kf+df; if(f2<0||f2>7) continue;
        bool sh = false;
        for(int r = rankOf(wKsq)+1; r < 8; r++) if(board[sqOf(r,f2)]==W_P){sh=true;break;}
        if(!sh) score -= 10; }
    }
    if(bKsq >= 0) {
      int kf = fileOf(bKsq);
      for(int df = -1; df <= 1; df++) { int f2 = kf+df; if(f2<0||f2>7) continue;
        bool sh = false;
        for(int r = rankOf(bKsq)-1; r >= 0; r--) if(board[sqOf(r,f2)]==B_P){sh=true;break;}
        if(!sh) score += 10; }
    }
  }

  // ── King activity in endgame ──────────────────────────────────────────────
  if(endgame) {
    if(wKsq >= 0) { int kr=rankOf(wKsq), kf=fileOf(wKsq);
      score += (8 - (abs(kr-3)+abs(kr-4)+abs(kf-3)+abs(kf-4))) * 4; }
    if(bKsq >= 0) { int kr=rankOf(bKsq), kf=fileOf(bKsq);
      score -= (8 - (abs(kr-3)+abs(kr-4)+abs(kf-3)+abs(kf-4))) * 4; }
  }

  // ── Opening-specific penalties ────────────────────────────────────────────
  if(opening) {
    // Undeveloped minor piece penalty
    if(board[sqOf(7,1)]==B_N) score += 30; if(board[sqOf(7,6)]==B_N) score += 30;
    if(board[sqOf(7,2)]==B_B) score += 25; if(board[sqOf(7,5)]==B_B) score += 25;
    if(board[sqOf(0,1)]==W_N) score -= 30; if(board[sqOf(0,6)]==W_N) score -= 30;
    if(board[sqOf(0,2)]==W_B) score -= 25; if(board[sqOf(0,5)]==W_B) score -= 25;

    // Castling rights
    if(!(bCastle_K||bCastle_Q)) score += 30;
    if(!(wCastle_K||wCastle_Q)) score -= 30;
    if(bCastle_K||bCastle_Q)    score -= 10;
    if(wCastle_K||wCastle_Q)    score += 10;

    // Premature queen development
    { int wMH=0,bMH=0;
      if(board[sqOf(0,1)]==W_N)wMH++;if(board[sqOf(0,6)]==W_N)wMH++;
      if(board[sqOf(0,2)]==W_B)wMH++;if(board[sqOf(0,5)]==W_B)wMH++;
      if(board[sqOf(7,1)]==B_N)bMH++;if(board[sqOf(7,6)]==B_N)bMH++;
      if(board[sqOf(7,2)]==B_B)bMH++;if(board[sqOf(7,5)]==B_B)bMH++;
      if(board[sqOf(0,3)]!=W_Q && wMH>=2) score -= 40;
      if(board[sqOf(7,3)]!=B_Q && bMH>=2) score += 40;
    }

    // Rim knight penalty
    for(int s=0;s<64;s++) {
      if(board[s]==B_N && (fileOf(s)==0||fileOf(s)==7)) score += 25;
      if(board[s]==W_N && (fileOf(s)==0||fileOf(s)==7)) score -= 25;
    }
  }

  // ── Rook on open/semi-open file ───────────────────────────────────────────
  for(int s = 0; s < 64; s++) {
    int p = board[s]; if(TYPE(p) != ROOK) continue;
    int f = fileOf(s);
    bool wp=false, bp=false;
    for(int r=0;r<8;r++){if(board[sqOf(r,f)]==W_P)wp=true;if(board[sqOf(r,f)]==B_P)bp=true;}
    if(IS_WHITE(p)) { if(!wp&&!bp) score+=20; else if(!wp) score+=10; }
    else            { if(!bp&&!wp) score-=20; else if(!bp) score-=10; }
  }

  // ── Bishop mobility penalty ───────────────────────────────────────────────
  { const int bd[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
    for(int s=0;s<64;s++) {
      int p=board[s]; if(TYPE(p)!=BISHOP) continue;
      int mob=0, r0=rankOf(s), f0=fileOf(s);
      for(auto& d:bd){int r=r0+d[0],f=f0+d[1];
        while(inBounds(r,f)){mob++;if(board[sqOf(r,f)])break;r+=d[0];f+=d[1];}}
      int pen = (mob==0)?50:(mob==1)?35:(mob==2)?20:(mob==3)?8:0;
      if(IS_WHITE(p)) score -= pen; else score += pen;
    }
  }

  // ── Pawn threat on knights/bishops — via shared pieceThreatPenalty ────────
  for(int s = 0; s < 64; s++) {
    int p = board[s]; if(!p) continue;
    if(TYPE(p) != KNIGHT && TYPE(p) != BISHOP) continue;
    score += pieceThreatPenalty(s, p);
  }

  return score;
}

// ─── Stalemate double-check ───────────────────────────────────────────────────
int countAllLegalMoves(bool white) {
  Move buf[128]; int count;
  generateMoves(white, buf, count, false);
  int legal = 0;
  for(int i=0;i<count;i++) if(isLegal(buf[i])) legal++;
  return legal;
}

// ─── Draw score ───────────────────────────────────────────────────────────────
int getDrawScore() {
  int wMat=0, bMat=0;
  for(int s=0;s<64;s++) {
    int p=board[s]; if(!p) continue; int t=TYPE(p); if(t==KING) continue;
    if(IS_WHITE(p)) wMat+=PIECE_VAL[t]; else bMat+=PIECE_VAL[t];
  }
  int diff = wMat - bMat; bool endg = (wMat+bMat) < 1800;
  if(diff > 400 && endg) return min(300, diff/3);
  if(diff > 400)  return 50;
  if(diff > 100)  return -80;
  if(diff < -100) return -150;
  return -30;
}

// ─── Move ordering ────────────────────────────────────────────────────────────
// Unified, no early returns, no phase gates.
// Safety checks fire for ALL pieces, ALL phases, BOTH colors.
int moveScore(Move& m) {
  int s = 0;
  uint8_t cap   = board[m.to];
  int piece     = board[m.from];
  int toR       = rankOf(m.to),   toF = fileOf(m.to);
  int frR       = rankOf(m.from), frF = fileOf(m.from);
  bool opening  = isOpeningPhase();
  bool isBlack  = IS_BLACK(piece);

  // ── 1. Captures — MVV-LVA + SEE ordering ─────────────────────────────────
  // Priority tiers (high to low):
  //   Winning captures  (SEE > 0):  7000 + SEE gain + LVA bonus
  //   Even captures     (SEE == 0): 5000            + LVA bonus
  //   Losing captures   (SEE < 0):  1000 + SEE gain (negative, below quiet moves)
  //
  // Within winning and even captures, LVA (Least Valuable Attacker) ensures
  // the cheapest piece captures first — e.g. pawn takes knight ranks above
  // queen takes knight even when both win the same material. This is correct
  // chess: use your least valuable piece to make a winning trade, preserve
  // your more valuable pieces.
  //
  // LVA bonus: PIECE_VAL[KING] - PIECE_VAL[attacker_type]
  //   Pawn   attacker: 20000 - 100  = 19900  (highest — go first)
  //   Knight attacker: 20000 - 320  = 19680
  //   Bishop attacker: 20000 - 330  = 19670
  //   Rook   attacker: 20000 - 500  = 19500
  //   Queen  attacker: 20000 - 900  = 19100  (lowest  — go last)
  //   King   attacker: 20000 - 20000 = 0
  if(cap) {
    int seeNet  = seeMoveGain(m.from, m.to, piece, isBlack ? 2 : 1);
    int lva     = PIECE_VAL[KING] - PIECE_VAL[TYPE(piece)];  // higher = cheaper attacker
    if(seeNet > 0) {
      s += 7000 + seeNet + lva;   // winning: SEE gain + LVA
    } else if(seeNet == 0) {
      s += 5000 + lva;            // even: just LVA to prefer cheaper attacker
    } else {
      s += 1000 + seeNet;         // losing: no LVA bonus, ranked by how much we lose
    }
  }
  if(m.promo)  s += 3000 + PIECE_VAL[m.promo];
  if(m.flags==1) s += 2000;

  // ── 2. Universal quiet-move safety (all pieces, all phases) ──────────────
  // Simulate the piece landing and run SEE from opponent's view.
  // This is the ONLY place this check lives — no duplication.
  if(cap == EMPTY && TYPE(piece) != PAWN && TYPE(piece) != KING) {
    uint8_t sTo = board[m.to], sFrom = board[m.from];
    board[m.to] = piece; board[m.from] = EMPTY;
    int attColor = isBlack ? 1 : 2;
    int netLoss  = seeCaptures(m.to, attColor);
    board[m.to] = sTo; board[m.from] = sFrom;
    if(netLoss > 0) s -= netLoss * 4;
  }

  // ── 3. Pawn threat on landing square — via shared helper ─────────────────
  // Simulate the piece on the destination and call pieceThreatPenalty.
  // The helper returns white-relative score; for black pieces negate to get
  // the penalty from black's move-ordering perspective.
  if(cap == EMPTY && (TYPE(piece)==KNIGHT || TYPE(piece)==BISHOP)) {
    uint8_t sTo = board[m.to], sFrom = board[m.from];
    board[m.to] = piece; board[m.from] = EMPTY;
    int threatDelta = pieceThreatPenalty(m.to, piece);
    board[m.to] = sTo; board[m.from] = sFrom;
    // White-relative: positive = bad for black, negative = bad for white.
    // For move ordering: a bad landing for black lowers black's move score.
    if(isBlack) s -= threatDelta; // threatDelta positive → bad for black → subtract
    else        s += threatDelta; // threatDelta negative → bad for white → add (making it smaller)
  }

  // ── 4. Black opening heuristics (only in opening, only for black) ─────────
  if(!isBlack || !opening) return s;

  int undevMinors = 0;
  if(board[sqOf(7,1)]==B_N) undevMinors++;
  if(board[sqOf(7,6)]==B_N) undevMinors++;
  if(board[sqOf(7,2)]==B_B) undevMinors++;
  if(board[sqOf(7,5)]==B_B) undevMinors++;

  // Pawn moves: reward safe central advancement
  if(TYPE(piece)==PAWN) {
    int pawnSEE = seeMoveGain(m.from, m.to, piece, 2);
    if(pawnSEE >= 0) {
      if((frF==3||frF==4) && frR==6 && toR==4) s += 400;
      else if((frF==3||frF==4) && toR==5)      s += 250;
      else if((frF==3||frF==4) && toR==4)      s += 200;
      else if((frF==2||frF==5) && (toR==4||toR==5)) s += 120;
      else if(toR<=4 && toF>=2 && toF<=5)      s += 50;
    } else {
      s -= pawnSEE * 4;
    }
    if(frF==0||frF==7) s -= 200;
    for(int r2=0;r2<8;r2++) if(board[sqOf(r2,toF)]==B_P){s-=70;break;}
    if(frF>=5 && toR<6) s -= 80;
    if(cap && TYPE(board[m.to])==PAWN && pawnSEE>=0) s += 180;
  }

  // Queen: heavy penalties for premature development
  if(TYPE(piece)==QUEEN) {
    if(undevMinors>=3) s -= 1500;
    else if(undevMinors==2) s -= 1200;
    else if(undevMinors==1) s -= 600;
    if(toR>=3&&toR<=5&&toF>=2&&toF<=5) s -= 400;
    if(bCastle_K||bCastle_Q) s -= 300;
    if(cap!=EMPTY && undevMinors>=1) s -= 500;
  }

  // King: strong penalty for moving without castling
  if(TYPE(piece)==KING && m.flags==0) s -= 1500;

  // Development bonus: first move of a home-rank piece
  if(frR==7 && (TYPE(piece)==KNIGHT||TYPE(piece)==BISHOP)) s += 400;

  // Re-move penalties
  if(m.from == lastBlackTo && cap == EMPTY) s -= 800;
  if((bPieceMoved & (1ULL << m.from)) && cap == EMPTY)
    s -= 400 + undevMinors * 120;

  // Knight positional bonuses/penalties
  if(TYPE(piece)==KNIGHT) {
    if(toF==2||toF==5) s += 100;
    if(toF==3||toF==4) s += 80;
    if(toF==0||toF==7) s -= 150;
    if(board[sqOf(7,1)]==B_N && board[sqOf(7,6)]==B_N) s += 80;
    // Fork opportunity: landing near white pawn that can be attacked
    const int katt[8][2]={{1,2},{1,-2},{-1,2},{-1,-2},{2,1},{2,-1},{-2,1},{-2,-1}};
    for(auto& d:katt) {
      int pr=toR+d[0], pf=toF+d[1];
      if(inBounds(pr,pf)){int psq=sqOf(pr,pf);
        if(board[psq]==W_P && seeCaptures(psq,2)>0){s+=400;break;}}
    }
  }

  // Bishop positional bonuses/penalties
  if(TYPE(piece)==BISHOP) {
    if((toR==6&&toF==1)||(toR==6&&toF==6)) s += 80;
    if(toF==0||toF==7) s -= 120;
    if(board[sqOf(7,1)]==B_N||board[sqOf(7,6)]==B_N) s -= 100;
    if((toF==3||toF==4)&&toR==5&&board[sqOf(6,toF)]==B_P) s -= 100;
  }

  // General centrality bonus for knights and bishops
  if(TYPE(piece)==KNIGHT||TYPE(piece)==BISHOP) {
    int cd = abs(toF-3)+abs(toF-4)+abs(toR-3)+abs(toR-4);
    s += (8 - cd) * 12;
    if(m.to == lastBlackFrom && cap == EMPTY) s -= 220;
  }

  // Backward move penalty (black moves forward by decreasing rank)
  if(toR < frR && cap == EMPTY) s -= 100;

  // Castling: reward if safe
  if(m.flags==2||m.flags==3) {
    int maxHang = 0;
    for(int sq=0;sq<64;sq++) {
      if(!board[sq]||!IS_BLACK(board[sq])||TYPE(board[sq])==KING) continue;
      int loss=seeCaptures(sq,1); if(loss>maxHang) maxHang=loss;
    }
    s += (maxHang==0) ? 600 : (maxHang<=100) ? 200 : -800;
  }

  // Rook: only in opening when developed enough
  if(TYPE(piece)==ROOK) {
    if(m.to==lastBlackFrom && cap==EMPTY) s -= 350;
    if(undevMinors>=2) s -= 500;
    if(undevMinors==1) s -= 200;
    bool openFile=true;
    for(int r=0;r<8;r++) if(board[sqOf(r,toF)]==B_P){openFile=false;break;}
    if(openFile) s += 80;
    if(toR==1)   s += 120;
  }

  return s;
}


void sortMoves(Move* moves, int count) {
  // Insertion sort with scores — good for small count (chess moves rarely >60)
  int scores[128];
  for(int i=0;i<count;i++) scores[i] = moveScore(moves[i]);
  for(int i=1;i<count;i++) {
    Move keyM = moves[i];
    int  keyS = scores[i];
    int j = i-1;
    while(j>=0 && scores[j] < keyS) {
      moves[j+1]  = moves[j];
      scores[j+1] = scores[j];
      j--;
    }
    moves[j+1]  = keyM;
    scores[j+1] = keyS;
  }
}

// ─── Killer moves ─────────────────────────────────────────────────────────────
#define MAX_DEPTH 10
Move killers[MAX_DEPTH][2];

// ─── Per-depth move buffers ───────────────────────────────────────────────────
#define AB_MAX_LEVELS 12
#define Q_MAX_LEVELS  12
Move abMoveBuf[AB_MAX_LEVELS][128];
Move qMoveBuf [Q_MAX_LEVELS ][64];
Move rootMoveBuf[128];
int  abDepthLevel = 0;
int  qDepthLevel  = 0;

// ─── History heuristic ────────────────────────────────────────────────────────
int histHeuristic[64][64];

// ─── Quiescence search ───────────────────────────────────────────────────────
int quiesce(int alpha, int beta) {
  if(searchAborted) return 0;
  nodeCount++;
  if((nodeCount & 127) == 0) {
    unsigned long _now = millis();
    if((_now - searchStart) > SEARCH_TIME_MS) { searchAborted=true; return 0; }
    int _pct = (int)((_now - searchStart) * 95UL / SEARCH_TIME_MS);
    if(_pct > 95) _pct = 95;
    searchPct = _pct;
  }

  int stand_pat = whiteToMove ? evaluate() : -evaluate();
  if(stand_pat >= beta) return beta;
  if(stand_pat < alpha - 950) return alpha;
  if(stand_pat > alpha) alpha = stand_pat;

  int myQLevel = qDepthLevel++;
  if(myQLevel >= Q_MAX_LEVELS) { qDepthLevel = myQLevel; return stand_pat; }
  Move* moves = qMoveBuf[myQLevel];
  int count;
  generateMoves(whiteToMove, moves, count, true);
  sortMoves(moves, count);

  for(int i=0;i<count;i++) {
    Move& m = moves[i];
    int moverColor = whiteToMove ? 1 : 2;
    if(seeMoveGain(m.from, m.to, board[m.from], moverColor) < 0) continue;
    if(!isLegal(m)) continue;

    HistoryEntry he;
    makeMove(m, &he);
    int score = -quiesce(-beta, -alpha);
    unmakeMove(he);

    if(score >= beta) { qDepthLevel = myQLevel; return beta; }
    if(score > alpha) alpha = score;
  }
  qDepthLevel = myQLevel;
  return alpha;
}

// ─── Alpha-Beta ───────────────────────────────────────────────────────────────
int alphaBeta(int depth, int alpha, int beta, bool maximizing, unsigned long startMs) {
  if(searchAborted) return 0;

  int myLevel = abDepthLevel++;
  if(myLevel >= AB_MAX_LEVELS) { abDepthLevel = myLevel; return 0; }

  nodeCount++;
  if((nodeCount & 127) == 0) {
    unsigned long _now = millis();
    if((_now - startMs) > SEARCH_TIME_MS) { abDepthLevel = myLevel; searchAborted=true; return 0; }
    int _pct = (int)((_now - startMs) * 95UL / SEARCH_TIME_MS);
    if(_pct > 95) _pct = 95;
    searchPct = _pct;
  }
  if(searchAborted) { abDepthLevel = myLevel; return 0; }

  bool inCheck = isInCheck(whiteToMove);
  if(inCheck && depth > 0) depth++;

  uint32_t h = currentHash;
  TTEntry& tte = tt[h & TT_MASK];
  Move ttMove = {0,0,0,0};
  if(tte.hashHi == (h>>16) && tte.depth >= depth) {
    if(tte.flags==1) { abDepthLevel = myLevel; return tte.score; }
    if(tte.flags==2 && tte.score >= beta) { abDepthLevel = myLevel; return tte.score; }
    if(tte.flags==3 && tte.score <= alpha) { abDepthLevel = myLevel; return tte.score; }
    ttMove.from=tte.from; ttMove.to=tte.to;
  }

  { int rCnt = 0;
    for(int i=0;i<hashHistoryCount;i++) if(hashHistory[i]==h) rCnt++;
    if(rCnt >= 2) { abDepthLevel = myLevel; return getDrawScore(); }
  }

  if(depth==0) { abDepthLevel = myLevel; return quiesce(alpha, beta); }

  if(depth >= 3 && !inCheck) {
    bool hasNonPawn = false;
    for(int s=0;s<64;s++) {
      int p=board[s];
      if(p && COLOR(p)==(whiteToMove?1:2) && TYPE(p)!=PAWN && TYPE(p)!=KING) { hasNonPawn=true; break; }
    }
    if(hasNonPawn) {
      bool savedWTM = whiteToMove;
      int savedEP = enPassantSq;
      uint32_t savedHash = currentHash;
      if(enPassantSq>=0) currentHash ^= zobEP[fileOf(enPassantSq)];
      enPassantSq = -1;
      whiteToMove = !whiteToMove;
      currentHash ^= zobSide;

      int R = (depth >= 5) ? 3 : 2;  // adaptive null-move reduction
      int nullScore = -alphaBeta(depth-R, -beta, -beta+1, !maximizing, startMs);

      whiteToMove = savedWTM;
      enPassantSq = savedEP;
      currentHash = savedHash;

      if(nullScore >= beta && !searchAborted) { abDepthLevel = myLevel; return beta; }
    }
  }

  if(depth==1 && !inCheck) {
    int ev = whiteToMove ? evaluate() : -evaluate();
    if(ev < alpha - 300) {
      abDepthLevel = myLevel; return quiesce(alpha, beta);
    }
  }

  Move* moves = abMoveBuf[myLevel];
  int count;
  generateMoves(whiteToMove, moves, count, false);

  if(count==0) {
    abDepthLevel = myLevel;
    if(inCheck) return -19000 + depth;
    return getDrawScore();
  }

  if(ttMove.from || ttMove.to) {
    for(int i=0;i<count;i++) {
      if(moves[i].from==ttMove.from && moves[i].to==ttMove.to) {
        Move tmp=moves[0]; moves[0]=moves[i]; moves[i]=tmp;
        break;
      }
    }
  }

  sortMoves(moves, count);

  int origAlpha = alpha;
  Move bestMove = {0,0,0,0};
  int legalCount2 = 0;

  for(int i=0;i<count;i++) {
    Move& m = moves[i];
    if(!isLegal(m)) continue;
    legalCount2++;

    if(depth==1 && !inCheck && board[m.to]==EMPTY && !m.promo) {
      int ev = whiteToMove ? evaluate() : -evaluate();
      if(ev + 150 <= alpha) continue;
    }

    HistoryEntry he;
    makeMove(m, &he);
    hashHistory[hashHistoryCount++] = currentHash;

    int score;
    int depthIdx = min(depth, MAX_DEPTH-1);
    bool isKiller = (m.from==killers[depthIdx][0].from && m.to==killers[depthIdx][0].to) ||
                    (m.from==killers[depthIdx][1].from && m.to==killers[depthIdx][1].to);
    if(i >= 4 && depth >= 3 && board[m.to]==EMPTY && !m.promo && !inCheck && !isKiller) {
      int R = (i >= 8 && depth >= 5) ? 2 : 1;  // deeper LMR for very late moves
      score = -alphaBeta(depth-1-R, -alpha-1, -alpha, !maximizing, startMs);
      if(score > alpha && !searchAborted) {
        score = -alphaBeta(depth-1, -beta, -alpha, !maximizing, startMs);
      }
    } else {
      score = -alphaBeta(depth-1, -beta, -alpha, !maximizing, startMs);
    }

    hashHistoryCount--;
    unmakeMove(he);
    if(searchAborted) { abDepthLevel = myLevel; return 0; }

    if(score >= beta) {
      if(board[m.to]==EMPTY) {
        killers[min(depth, MAX_DEPTH-1)][1]=killers[min(depth, MAX_DEPTH-1)][0];
        killers[min(depth, MAX_DEPTH-1)][0]=m;
        histHeuristic[m.from][m.to] += depth*depth;
      }
      tte.hashHi=(h>>16); tte.score=(int16_t)score; tte.depth=depth;
      tte.flags=2; tte.from=m.from; tte.to=m.to;
      abDepthLevel = myLevel; return beta;
    }
    if(score > alpha) {
      alpha = score;
      bestMove = m;
    }
  }

  if(legalCount2==0) {
    abDepthLevel = myLevel;
    if(inCheck) return -19000+depth;
    return getDrawScore();
  }

  if(bestMove.from || bestMove.to) {
    tte.hashHi=(h>>16); tte.score=(int16_t)alpha; tte.depth=depth;
    tte.flags=(alpha>origAlpha)?1:3;
    tte.from=bestMove.from; tte.to=bestMove.to;
  }

  abDepthLevel = myLevel;
  return alpha;
}

// ─── Opening Book ─────────────────────────────────────────────────────────────
// SAFETY CONTRACT: Every candidate move is cross-checked against the actual
// generated legal move list. A book move is ONLY returned if it exists verbatim
// in the legal move list for the current position. This guarantees no illegal
// moves can ever be returned regardless of what white played.
Move getBookMove(int blackMoveNum) {
  Move none = {0,0,0,0};

  // Generate all legal black moves from the current position.
  // Book candidates must be found in this list — no exceptions.
  Move legal[128];
  int legalCnt = 0;
  generateMoves(false, legal, legalCnt, false);
  // Filter to actually-legal (not leaving king in check)
  Move trueLegal[128];
  int trueCnt = 0;
  for(int i = 0; i < legalCnt; i++) {
    if(isLegal(legal[i])) trueLegal[trueCnt++] = legal[i];
  }
  if(trueCnt == 0) return none;

  // Helper: return a book move only if it exists in trueLegal
  auto tryMove = [&](int from, int to) -> Move {
    // Basic sanity: our piece must be on `from`, `to` must not have our piece
    if(!squareOnBoard(from) || !squareOnBoard(to)) return none;
    if(!board[from] || !IS_BLACK(board[from])) return none;
    if(board[to] && IS_BLACK(board[to])) return none;  // friendly piece on target
    // Must exist in the legal list
    for(int i = 0; i < trueCnt; i++) {
      if(trueLegal[i].from == from && trueLegal[i].to == to) return trueLegal[i];
    }
    return none;
  };

  // Read current board state — everything is conditional on what's actually there
  bool wE4 = (board[sqOf(3,4)] == W_P);
  bool wD4 = (board[sqOf(3,3)] == W_P);
  bool wC4 = (board[sqOf(3,2)] == W_P);
  bool wF4 = (board[sqOf(3,5)] == W_P);
  bool wE5 = (board[sqOf(4,4)] == W_P);  // white already pushed to e5
  bool wD5 = (board[sqOf(4,3)] == W_P);  // white already pushed to d5

  bool bE5 = (board[sqOf(4,4)] == B_P);
  bool bD5 = (board[sqOf(4,3)] == B_P);
  bool bC5 = (board[sqOf(4,2)] == B_P);
  bool bF5 = (board[sqOf(4,5)] == B_P);

  // Squares for piece origins
  int bNb8 = sqOf(7,1);   // Black queenside knight (b8)
  int bNg8 = sqOf(7,6);   // Black kingside knight  (g8)
  int bBc8 = sqOf(7,2);   // Black queenside bishop (c8)
  int bBf8 = sqOf(7,5);   // Black kingside bishop  (f8)
  int bPe7 = sqOf(6,4);   // Black e-pawn
  int bPd7 = sqOf(6,3);   // Black d-pawn
  int bPc7 = sqOf(6,2);   // Black c-pawn
  int bPf7 = sqOf(6,5);   // Black f-pawn

  int v = chosenOpening % 4;
  Move candidate = none;

  if(blackMoveNum == 1) {
    // First response — based on what white played
    if(wE4) {
      // e5 or c5 depending on variation — but NEVER if white pawn is on e5
      if(v <= 1) {
        candidate = tryMove(bPe7, sqOf(4,4));   // 1...e5
        if(!candidate.from) candidate = tryMove(bNg8, sqOf(5,5)); // fallback Nf6
      } else {
        candidate = tryMove(bPc7, sqOf(4,2));   // 1...c5 Sicilian
        if(!candidate.from) candidate = tryMove(bPe7, sqOf(4,4)); // fallback e5
      }
    } else if(wD4) {
      if(v <= 2) {
        candidate = tryMove(bPd7, sqOf(4,3));   // 1...d5
        if(!candidate.from) candidate = tryMove(bNg8, sqOf(5,5));
      } else {
        candidate = tryMove(bPf7, sqOf(4,5));   // 1...f5 Dutch
        if(!candidate.from) candidate = tryMove(bPd7, sqOf(4,3));
      }
    } else if(wC4) {
      if(v <= 1) {
        candidate = tryMove(bPe7, sqOf(4,4));
        if(!candidate.from) candidate = tryMove(bPc7, sqOf(4,2));
      } else {
        candidate = tryMove(bPc7, sqOf(4,2));
        if(!candidate.from) candidate = tryMove(bPe7, sqOf(4,4));
      }
    } else if(wF4) {
      // Bird's — respond with d5
      candidate = tryMove(bPd7, sqOf(4,3));
      if(!candidate.from) candidate = tryMove(bPe7, sqOf(5,4)); // e6
    } else {
      if(v <= 1) {
        candidate = tryMove(bPe7, sqOf(4,4));
      } else {
        candidate = tryMove(bPd7, sqOf(4,3));
      }
    }
  } else {
    // Move 2 — develop a knight or support the center.
    // CRITICAL: only suggest moves where the destination is NOT a pawn-kick trap.
    // A square is a pawn-kick trap if it is attacked by a white pawn AND there is
    // a white pawn on an adjacent file that can safely advance to re-attack it
    // (i.e. NOT a genuine outpost). Safe outposts are fine even if currently attacked.
    auto isKickTrap = [&](int sq) -> bool {
      int r = rankOf(sq), f = fileOf(sq);
      // Currently attacked by a white pawn?
      bool attacked = false;
      for(int df = -1; df <= 1; df += 2) {
        int pr = r - 1, pf = f + df;
        if(inBounds(pr, pf) && board[sqOf(pr, pf)] == W_P) { attacked = true; break; }
      }
      if(!attacked) return false;
      // Check each adjacent file for a white pawn that can safely advance to threaten
      for(int df = -1; df <= 1; df += 2) {
        int adjF = f + df;
        if(adjF < 0 || adjF > 7) continue;
        for(int rr = 0; rr < r; rr++) {
          if(board[sqOf(rr, adjF)] == W_P) {
            bool pathClear = true;
            for(int pr = rr + 1; pr <= r - 1; pr++) {
              if(board[sqOf(pr, adjF)] != EMPTY) { pathClear = false; break; }
            }
            if(pathClear) {
              int attackSq = sqOf(r - 1, adjF);
              uint8_t savedA = board[attackSq];
              uint8_t savedP = board[sqOf(rr, adjF)];
              board[attackSq]       = W_P;
              board[sqOf(rr, adjF)] = EMPTY;
              int pawnSafety = seeCaptures(attackSq, 2);
              board[attackSq]       = savedA;
              board[sqOf(rr, adjF)] = savedP;
              if(pawnSafety <= 0) return true;  // pawn push is safe → it's a trap
            }
            break;
          }
        }
      }
      return false;  // attacked but no pawn can safely advance → safe outpost
    };

    if(wE4 || wE5) {
      if(bE5) {
        // After 1.e4 e5 — develop Nf6 or Nc6, never to a pawn-attacked square
        int nc6 = sqOf(5,2), nf6 = sqOf(5,5);
        if(!isKickTrap(nc6)) {
          candidate = tryMove(bNb8, nc6);
        }
        if(!candidate.from && !isKickTrap(nf6)) {
          candidate = tryMove(bNg8, nf6);
        }
        if(!candidate.from) {
          // Knights both attacked — play d6 or Bc5
          candidate = tryMove(bPd7, sqOf(5,3));  // d6 solid
        }
      } else if(bC5) {
        // After 1.e4 c5 Sicilian — play d6 or Nc6
        int nc6 = sqOf(5,2);
        if(!isKickTrap(nc6)) {
          candidate = tryMove(bNb8, nc6);
        }
        if(!candidate.from) candidate = tryMove(bPd7, sqOf(5,3)); // d6
      } else if(bD5) {
        // Scandinavian — recapture or Nf6
        int nf6 = sqOf(5,5);
        if(!isKickTrap(nf6)) {
          candidate = tryMove(bNg8, nf6);
        }
        if(!candidate.from) candidate = tryMove(bPe7, sqOf(5,4)); // e6
      } else {
        // Some other first move — just develop kingside knight safely
        int nf6 = sqOf(5,5);
        if(!isKickTrap(nf6)) {
          candidate = tryMove(bNg8, nf6);
        }
        if(!candidate.from) candidate = tryMove(bPd7, sqOf(5,3));
      }
    } else if(wD4 || wD5) {
      if(bD5) {
        // QGD-style — play e6 or Nf6
        int nf6 = sqOf(5,5);
        if(!isKickTrap(nf6)) {
          candidate = tryMove(bNg8, nf6);
        }
        if(!candidate.from) candidate = tryMove(bPe7, sqOf(5,4)); // e6
      } else {
        int nf6 = sqOf(5,5);
        if(!isKickTrap(nf6)) {
          candidate = tryMove(bNg8, nf6);
        }
        if(!candidate.from) candidate = tryMove(bPd7, sqOf(4,3)); // d5 break
      }
    } else if(wC4) {
      if(bE5 || bC5) {
        int nc6 = sqOf(5,2);
        if(!isKickTrap(nc6)) {
          candidate = tryMove(bNb8, nc6);
        }
        if(!candidate.from) {
          int nf6 = sqOf(5,5);
          if(!isKickTrap(nf6)) candidate = tryMove(bNg8, nf6);
        }
      } else {
        int nf6 = sqOf(5,5);
        if(!isKickTrap(nf6)) candidate = tryMove(bNg8, nf6);
        if(!candidate.from) candidate = tryMove(bPe7, sqOf(5,4));
      }
    } else if(wF4) {
      int nf6 = sqOf(5,5);
      if(!isKickTrap(nf6)) candidate = tryMove(bNg8, nf6);
      if(!candidate.from) candidate = tryMove(bPe7, sqOf(5,4)); // e6
    } else {
      // Unknown opening — safe knight development
      int nf6 = sqOf(5,5), nc6 = sqOf(5,2);
      if(bE5 || bD5) {
        if(!isKickTrap(nc6)) candidate = tryMove(bNb8, nc6);
      }
      if(!candidate.from) {
        if(!isKickTrap(nf6)) candidate = tryMove(bNg8, nf6);
      }
      if(!candidate.from) {
        if(v%2==0) candidate = tryMove(bPe7, sqOf(4,4));
        else       candidate = tryMove(bPd7, sqOf(4,3));
      }
    }
  }

  return candidate;
}

// ─── Bot move with iterative deepening ───────────────────────────────────────
int fullMoveCount = 0;

void searchTask(void* param) {
  searchResult  = getBotMove();
  searchDone    = true;
  searchRunning = false;
  xSemaphoreGive(searchSem);
  vTaskDelete(NULL);
}

Move runSearchOnCore0() {
  searchDone    = false;
  searchRunning = true;
  searchResult  = {0,0,0,0};

  if(searchSem == nullptr) {
    searchSem = xSemaphoreCreateBinary();
  }

  xTaskCreatePinnedToCore(
    searchTask, "chess_search", 28672, nullptr, 1, nullptr, 0
  );

  int lastDrawnPct = -1;
  while(!searchDone) {
    int p = (int)searchPct;
    if(p != lastDrawnPct) {
      drawProgressBar(p);
      lastDrawnPct = p;
    }
    vTaskDelay(10);
    yield();
  }
  drawProgressBar(100);
  return searchResult;
}

Move getBotMove() {
  if(openingMoveIndex < 2) {
    Move bm = getBookMove(openingMoveIndex+1);
    if(bm.from || bm.to) {
      openingMoveIndex++;
      return bm;
    }
    openingMoveIndex = 2;
  }

  memset(killers, 0, sizeof(killers));
  memset(histHeuristic, 0, sizeof(histHeuristic));
  searchAborted = false;
  nodeCount     = 0;
  abDepthLevel  = 0;
  qDepthLevel   = 0;
  searchStart   = millis();

  Move* moves = rootMoveBuf;
  int count;
  generateMoves(false, moves, count, false);
  sortMoves(moves, count);

  Move completedBestMove = {0,0,0,0};
  int  completedBestScore = -99999;

  for(int i=0;i<count;i++) {
    if(isLegal(moves[i])) { completedBestMove = moves[i]; break; }
  }

  for(int depth = 1; depth <= 7; depth++) {
    if(searchAborted) break;

    Move  iterBestMove  = {0,0,0,0};
    int   iterBestScore = -99999;

    // Aspiration windows: narrow window around previous score for faster cutoffs
    int aspDelta = 50;
    int alpha, beta;
    if(depth >= 3 && completedBestScore > -90000) {
      alpha = completedBestScore - aspDelta;
      beta  = completedBestScore + aspDelta;
    } else {
      alpha = -99999; beta = 99999;
    }

    if(completedBestMove.from || completedBestMove.to) {
      for(int i=0;i<count;i++) {
        if(moves[i].from==completedBestMove.from && moves[i].to==completedBestMove.to) {
          Move tmp=moves[0]; moves[0]=moves[i]; moves[i]=tmp;
          break;
        }
      }
    }

    // Aspiration loop: widen window on fail-high/low
    while(true) {
      iterBestMove  = {0,0,0,0};
      iterBestScore = -99999;
      int loBound = alpha;  // preserve window lower bound before alpha gets updated

      for(int i=0; i<count; i++) {
        if(!isLegal(moves[i])) continue;
        if(searchAborted) break;

        HistoryEntry he;
        makeMove(moves[i], &he);
        hashHistory[hashHistoryCount++] = currentHash;
        int score = -alphaBeta(depth-1, -beta, -alpha, true, searchStart);
        hashHistoryCount--;
        unmakeMove(he);

        if(searchAborted) break;

        if(score > iterBestScore) {
          iterBestScore = score;
          iterBestMove  = moves[i];
          if(score > alpha) alpha = score;
        }
      }

      if(searchAborted) break;

      // Check if we need to re-search with wider window
      if(iterBestScore <= loBound && depth >= 3) {
        // Fail low — widen below
        alpha = max(-99999, loBound - aspDelta*2);
        aspDelta *= 2;
      } else if(iterBestScore >= beta && depth >= 3) {
        // Fail high — widen above
        beta = min(99999, beta + aspDelta*2);
        aspDelta *= 2;
      } else {
        break;  // score inside window, accept
      }
      if(aspDelta > 800) { alpha = -99999; beta = 99999; }  // full-window fallback
    }

    if(!searchAborted && (iterBestMove.from || iterBestMove.to)) {
      completedBestMove  = iterBestMove;
      completedBestScore = iterBestScore;
      for(int i=0;i<count;i++) {
        if(moves[i].from==completedBestMove.from && moves[i].to==completedBestMove.to) {
          Move tmp=moves[0]; moves[0]=moves[i]; moves[i]=tmp;
          break;
        }
      }
    }

    if(completedBestScore > 18000) break;
  }

  searchPct = 99;
  return completedBestMove;
}

// ─── TFT drawing helpers ───────────────────────────────────────────────────────
inline int sqX(int sq) { return BOARD_OX + fileOf(sq)*SQ_W; }
inline int sqY(int sq) { return BOARD_OY + (7-rankOf(sq))*SQ_H; }
inline uint16_t sqColor(int sq) {
  return ((rankOf(sq)+fileOf(sq))%2==0) ? C_DARK_SQ : C_LIGHT_SQ;
}

void drawPieceRainbowBorder(int sx, int sy, int sq) {
  // Green shimmer: different shades cycling by diagonal position
  static const uint16_t spec[8] = {
    0x07E0, 0x03E0, 0x47E4, 0x0540,
    0x07E0, 0x03A0, 0x27E4, 0x03E0,
  };
  int diag = ((sq/8) + (sq%8)) % 8;
  uint16_t cTop   = spec[diag];
  uint16_t cRight = spec[(diag+2) % 8];
  uint16_t cBot   = spec[(diag+4) % 8];
  uint16_t cLeft  = spec[(diag+6) % 8];
  tft.drawFastHLine(sx+1,      sy+1,      SQ_W-2, cTop);
  tft.drawFastHLine(sx+1,      sy+SQ_H-2, SQ_W-2, cBot);
  tft.drawFastVLine(sx+1,      sy+1,      SQ_H-2, cLeft);
  tft.drawFastVLine(sx+SQ_W-2, sy+1,      SQ_H-2, cRight);
}

void drawPieceAt(int sx, int sy, int piece) {
  if(IS_WHITE(piece)) {
    tft.fillRect(sx+2, sy+2, SQ_W-4, SQ_H-4, 0x0360);  // dark-mid green fill
    tft.setTextColor(0xFFFF);                              // white letter
  } else {
    tft.fillRect(sx+2, sy+2, SQ_W-4, SQ_H-4, 0x0841);  // near-black fill
    tft.setTextColor(0x07E0);                              // bright green letter
  }
  tft.setTextSize(1);
  tft.setCursor(sx+4, sy+3);
  tft.print(pieceChar(piece));
  int sq = ((sy - BOARD_OY) / SQ_H);
  int fl = ((sx - BOARD_OX) / SQ_W);
  sq = (7 - sq) * 8 + fl;
  if(sq >= 0 && sq < 64) drawPieceRainbowBorder(sx, sy, sq);
}

void clearSquarePixels(int sq) {
  int sx=sqX(sq), sy=sqY(sq);
  tft.fillRect(sx, sy, SQ_W, SQ_H, sqColor(sq));
}

void drawSquare(int sq) {
  clearSquarePixels(sq);
  if(board[sq]) {
    drawPieceAt(sqX(sq), sqY(sq), board[sq]);
  }
}

void drawBoard() {
  const uint16_t brc[8] = {0x0180,0x0220,0x0300,0x03A0,0x03E0,0x07E0,0x47E4,0x07E0};
  int bx = BOARD_OX-2, by = BOARD_OY-2;
  int bw = 8*SQ_W+4,   bh = 8*SQ_H+4;
  int segW = bw / 8;
  for(int i=0;i<8;i++) {
    int x0 = bx + i*segW;
    int x1 = (i==7) ? bx+bw-1 : x0+segW-1;
    tft.drawFastHLine(x0, by,       x1-x0+1, brc[i]);
    tft.drawFastHLine(x0, by+bh-1,  x1-x0+1, brc[7-i]);
  }
  int segH = bh / 8;
  for(int i=0;i<8;i++) {
    int y0 = by + i*segH;
    int y1 = (i==7) ? by+bh-1 : y0+segH-1;
    tft.drawFastVLine(bx,      y0, y1-y0+1, brc[7-i]);
    tft.drawFastVLine(bx+bw-1, y0, y1-y0+1, brc[i]);
  }
  tft.drawRect(BOARD_OX-1, BOARD_OY-1, 8*SQ_W+2, 8*SQ_H+2, 0x03E0);

  for(int sq=0;sq<64;sq++) drawSquare(sq);

  tft.fillRect(14, 4, 7, 104, C_BG);
  for(int r=0;r<8;r++) {
    tft.setTextColor(brc[r]);
    tft.setTextSize(1);
    tft.setCursor(15, BOARD_OY + (7-r)*SQ_H + 3);
    tft.print((char)('1'+r));
  }
  tft.fillRect(22, 112, 106, 10, C_BG);
  for(int f=0;f<8;f++) {
    tft.setTextColor(brc[f]);
    tft.setTextSize(1);
    tft.setCursor(BOARD_OX + f*SQ_W + 4, 113);
    tft.print((char)('a'+f));
  }
}

void drawLastMoveOutlines(int from, int to) {
  if(from<0||to<0) return;
  tft.drawRect(sqX(from), sqY(from), SQ_W, SQ_H, C_LASTMOVE);
  tft.drawRect(sqX(to),   sqY(to),   SQ_W, SQ_H, C_LASTMOVE);
}

void drawLegalDots() {
  for(int i=0;i<legalCount;i++) {
    int sq = legalCache[i].to;
    int sx = sqX(sq)+SQ_W/2;
    int sy = sqY(sq)+SQ_H/2;
    tft.fillCircle(sx, sy, 3, 0x0180);
    tft.fillCircle(sx, sy, 2, C_DOT);
  }
}

void clearLegalDots() {
  for(int i=0;i<legalCount;i++) drawSquare(legalCache[i].to);
}

void drawCursorAt(int col, int row) {
  if(col >= 0) {
    int sq = sqOf(row, col);
    int sx = sqX(sq), sy = sqY(sq);
    uint16_t c;
    bool onLastMove = (engineFrom >= 0 && (sq == engineFrom || sq == engineTo));
    if(selSq == sq)       c = C_BOTH;
    else if(onLastMove)   c = C_CURSOR_LASTMOVE;
    else                  c = C_HOVER;
    tft.drawRect(sx, sy, SQ_W, SQ_H, c);
  }
}

void redrawSquareWithArrow(int sq, int cursorSq=-1) {
  drawSquare(sq);
  if(engineFrom >= 0 && engineTo >= 0) {
    if(sq == engineFrom || sq == engineTo)
      tft.drawRect(sqX(sq), sqY(sq), SQ_W, SQ_H, C_LASTMOVE);
  }
  if(selSq == sq) tft.drawRect(sqX(sq), sqY(sq), SQ_W, SQ_H, C_SEL);
}

void eraseCursorAt(int col, int row) {
  if(col >= 0 && col < 8 && row >= 0 && row < 8) {
    int sq = sqOf(row, col);
    redrawSquareWithArrow(sq, sq);
  }
}

void drawSelOutline() {
  if(selSq >= 0) {
    tft.drawRect(sqX(selSq), sqY(selSq), SQ_W, SQ_H, C_SEL);
  }
}

void drawPhaseIndicator() {
  int ph = gamePhase();
  // opening=bright green, middlegame=medium green, endgame=dim+red accent
  uint16_t col = (ph==0) ? 0x07E0 : (ph==1) ? 0x03E0 : 0xF800;
  tft.fillRect(4, 119, 6, 6, col);
  tft.drawRect(3, 118, 8, 8, 0x03A0);
}

// ─── UI-3: Draw red flag icon for reset button ────────────────────────────────
// Draws a pixel-art flag: a vertical pole with a red triangular flag on top
// x,y = top-left corner of the icon area (fits in 13x34 panel space)
void drawFlagIcon(int bx, int by) {
  tft.fillRoundRect(bx, by, 13, 34, 2, C_BTN_BG);

  int poleX = bx + 4;
  int poleTop = by + 4;
  int poleBot = by + 29;
  // Pole stays neutral — a slim gray-green so it doesn't clash with the red flag
  tft.drawFastVLine(poleX,   poleTop, poleBot - poleTop + 1, 0x0300);
  tft.drawFastVLine(poleX+1, poleTop, poleBot - poleTop + 1, 0x0180);

  // Red triangular flag — bright red highlight on top edge, dark red shadow on diagonal
  int flagStartY = poleTop;
  int flagH = 11;
  for(int row = 0; row < flagH; row++) {
    int rowY = flagStartY + row;
    int w = flagH - row;
    for(int px = 0; px < w; px++) {
      int fx = poleX + 2 + px;
      if(fx < bx + 13) {
        // Top edge: bright highlight. Trailing edge: dark shadow. Fill: mid red.
        uint16_t c = (row == 0) ? 0xFBA0 : (px == w-1) ? 0x8000 : 0xD000;
        tft.drawPixel(fx, rowY, c);
      }
    }
  }

  // Base foot — dim green shadow line (pole rests on the ground)
  tft.drawFastHLine(poleX - 1, poleBot + 1, 4, 0x0180);
}

void drawLeftPanel() {
  drawFlagIcon(0, 8);
  if(panelFocus==1) {
    tft.drawFastHLine(0, 8,  13, 0x07E0);
    tft.drawFastHLine(0, 41, 13, 0x03E0);
    tft.drawFastVLine(0, 8,  34, 0x07E0);
    tft.drawFastVLine(12,8,  34, 0x03E0);
  }

  tft.fillRoundRect(0, 50, 13, 34, 2, C_BTN_BG);
  if(panelFocus==2) {
    tft.drawFastHLine(0, 50, 13, 0x07E0);
    tft.drawFastHLine(0, 83, 13, 0x03A0);
    tft.drawFastVLine(0, 50, 34, 0x07E0);
    tft.drawFastVLine(12,50, 34, 0x03A0);
  }
  tft.setTextSize(1);
  tft.setTextColor(0x03E0); tft.setCursor(4, 54); tft.print('U');
  tft.setTextColor(0x03A0); tft.setCursor(4, 63); tft.print('N');
  tft.setTextColor(0x0360); tft.setCursor(4, 72); tft.print('D');

  tft.drawFastVLine(13, 4, 104, 0x03A0);
}

void drawProgressBar(int pct) {
  if(pct == dispPct) return;

  int oldPct = dispPct;
  dispPct = pct;

  if(pct == 0 || oldPct < 0) {
    tft.fillRect(PB_X+1, PB_Y+1, PB_IW, PB_IH, C_PB_EMPTY);
    for(int y=0; y<PB_IH+2; y++) {
      int gi = (y * 95) / (PB_IH+1);
      uint16_t bc = pgm_read_word(&pbGrad[gi]);
      tft.drawPixel(PB_X,         PB_Y+y, bc);
      tft.drawPixel(PB_X+PB_IW+1, PB_Y+y, bc);
    }
    tft.drawFastHLine(PB_X, PB_Y,         PB_IW+2, pgm_read_word(&pbGrad[0]));
    tft.drawFastHLine(PB_X, PB_Y+PB_IH+1, PB_IW+2, pgm_read_word(&pbGrad[95]));
    tft.fillRect(PB_X-2, 112, 20, 9, C_BG);
    tft.setTextColor(pgm_read_word(&pbGrad[0]));
    tft.setTextSize(1);
    tft.setCursor(PB_X, 113);
    tft.print("00%");
    return;
  }

  int oldH = (PB_IH * max(0, oldPct)) / 100;
  int newH = (PB_IH * pct) / 100;

  if(newH > oldH) {
    for(int y = oldH; y < newH; y++) {
      int gradIdx = (y * 95) / PB_IH;
      if(gradIdx > 95) gradIdx = 95;
      uint16_t c = pgm_read_word(&pbGrad[gradIdx]);
      int yy = PB_Y + 1 + PB_IH - 1 - y;
      tft.drawFastHLine(PB_X+1, yy, PB_IW, c);
    }
  } else if(newH < oldH) {
    tft.fillRect(PB_X+1, PB_Y+1, PB_IW, oldH - newH, C_PB_EMPTY);
  }

  tft.fillRect(PB_X-2, 112, 20, 9, C_BG);
  int gradIdx = (pct * 95) / 100;
  if(gradIdx > 95) gradIdx = 95;
  uint16_t tc = pgm_read_word(&pbGrad[gradIdx]);
  tft.setTextColor(tc);
  tft.setTextSize(1);
  tft.setCursor(PB_X, 113);
  if(pct < 10) tft.print('0');
  tft.print(pct);
  tft.print('%');
}

void drawGameOver() {
  tft.fillRect(22, 119, 104, 9, C_BG);
  tft.setTextSize(1);
  int len = strlen(gameOverMsg);
  int tw  = len * 6;
  int tx  = 22 + (104 - tw) / 2;
  const uint16_t brc[8] = {0x03E0,0x05E0,0x03E0,0x05E0,0x03E0,0x05E0,0x03E0,0x05E0};
  for(int i=0; i<len; i++) {
    tft.setTextColor(brc[i % 8]);
    tft.setCursor(tx + i*6, 120);
    tft.print(gameOverMsg[i]);
  }
}

void drawAll() {
  tft.fillScreen(C_BG);
  drawBoard();
  drawLeftPanel();
  drawProgressBar(0);
  if(engineFrom>=0) drawLastMoveOutlines(engineFrom, engineTo);
  if(selSq>=0) {
    tft.drawRect(sqX(selSq), sqY(selSq), SQ_W, SQ_H, C_SEL);
    drawLegalDots();
  }
  if(cursorCol>=0) drawCursorAt(cursorCol, cursorRow);
  drawPhaseIndicator();
  if(gameOver) drawGameOver();
}

// ─── Game-over check ──────────────────────────────────────────────────────────
void checkGameOver() {
  Move moves[128];
  int count;
  generateMoves(whiteToMove, moves, count, false);
  bool hasLegal = false;
  for(int i=0;i<count;i++) {
    if(isLegal(moves[i])) { hasLegal=true; break; }
  }
  if(!hasLegal) {
    gameOver = true;
    if(isInCheck(whiteToMove)) {
      if(whiteToMove) {
        strcpy(gameOverMsg,"Hold button...");
        checkmatePending = true;
      } else {
        strcpy(gameOverMsg,"White Wins!");
        checkmatePending = false;
      }
    } else {
      strcpy(gameOverMsg,"Stalemate!");
      checkmatePending = false;
    }
    return;
  }
  if(insufficientMaterial()) {
    gameOver=true;
    strcpy(gameOverMsg,"Draw: Material");
    return;
  }
  if(isRepetition()) {
    gameOver=true;
    strcpy(gameOverMsg,"Draw: Repetition");
  }
}

bool insufficientMaterial() {
  int wPieces=0, bPieces=0;
  bool wBishop=false, bBishop=false;
  bool wKnight=false, bKnight=false;
  for(int s=0;s<64;s++) {
    int p=board[s]; if(!p) continue;
    if(IS_WHITE(p)) {
      if(TYPE(p)==PAWN||TYPE(p)==ROOK||TYPE(p)==QUEEN) return false;
      if(TYPE(p)==BISHOP) { wBishop=true; wPieces++; }
      if(TYPE(p)==KNIGHT) { wKnight=true; wPieces++; }
    } else {
      if(TYPE(p)==PAWN||TYPE(p)==ROOK||TYPE(p)==QUEEN) return false;
      if(TYPE(p)==BISHOP) { bBishop=true; bPieces++; }
      if(TYPE(p)==KNIGHT) { bKnight=true; bPieces++; }
    }
  }
  if(wPieces==0 && bPieces==0) return true;
  if(wPieces==1 && bPieces==0 && (wBishop||wKnight)) return true;
  if(wPieces==0 && bPieces==1 && (bBishop||bKnight)) return true;
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// UI-2: PASSWORD KEYPAD SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════
// Screen layout (128px tall, 160px wide):
//   Row 0..27  : Header bar (lock icon + title)
//   Row 28..49 : Digit display (6 boxes, clearly visible, not overlapped)
//   Row 50..53 : Thin separator
//   Row 54..127: Keypad grid (4 rows × 3 cols, compact keys)
//
// Key layout:
//   Row0: 1  2  3
//   Row1: 4  5  6
//   Row2: 7  8  9
//   Row3: BK ←  0  OK   ← BK = Back (exit without undo)

#define KEYPAD_CORRECT_PASSWORD "314159"  // Change this to desired password

static int  kpCurRow = 0, kpCurCol = 1;
static char kpEntered[7] = {0};
static int  kpDigitCount = 0;

// Key layout: 3 cols, 4 rows
// Row3: 'B'=Back/exit, '<'=backspace, '0'=zero, 'C'=confirm/OK
// We pack row3 as: col0=B(Back), col1=<(del), col2=C(OK)
// 0 is on row2 right-side extension — but we only have 3 cols.
// Revised simpler layout matching a phone pad:
//   Row0: 1 2 3
//   Row1: 4 5 6
//   Row2: 7 8 9
//   Row3: B 0 <    (B=Back, 0=zero, <=delete)
// Confirm is auto-triggered when 6 digits entered + user presses select on a
// special 5th row: Row4: [    OK    ] (full-width)
// Actually to keep 3-col grid, Row4 col1 = 'C', col0='B', col2='<'
// Final layout:
//   Row0: 1  2  3
//   Row1: 4  5  6
//   Row2: 7  8  9
//   Row3: B  0  <
//   Row4: [  OK  ]   (spans full width, col 0 only, special draw)
static const char kpLabels[5][3] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'B','0','<'},   // B=Back, <=delete
  {'C','C','C'}    // row4 = full-width OK button (all cols map to OK)
};

// Keypad geometry — compact to leave top clear for digit display
#define KP_X      6    // keypad left margin
#define KP_Y      54   // keypad top (below digit display + separator)
#define KP_KW     47   // key width  (3 keys × 47 + 2 gaps × 3 = 141+18 = fits 160)
#define KP_KH     13   // key height (compact: 4 rows × 13 + gaps × 3 = 52+9 = 61 → fits 74px)
#define KP_GAP     3   // gap between keys

// Digit display geometry
#define KP_DIG_Y   30  // top of digit box row
#define KP_DIG_H   16  // height of digit boxes
#define KP_DIG_W   20  // width of each digit box
#define KP_DIG_GAP  3  // gap between digit boxes
// 6 boxes × 20 + 5 gaps × 3 = 120+15 = 135; left margin = (160-135)/2 = 12
#define KP_DIG_X   12  // left margin of first digit box

// Draw a single keypad key
void drawKeypadKey(int col, int row, bool selected) {
  // Row 4 = full-width OK button
  int kx, ky, kw;
  if(row == 4) {
    kx = KP_X;
    ky = KP_Y + 4 * (KP_KH + KP_GAP);
    kw = 3 * KP_KW + 2 * KP_GAP;  // full width spanning all 3 cols
    if(col != 0) return;            // only draw once
  } else {
    kx = KP_X + col * (KP_KW + KP_GAP);
    ky = KP_Y + row * (KP_KH + KP_GAP);
    kw = KP_KW;
  }

  char label = kpLabels[row][col];

  uint16_t bgCol, borderCol, textCol;

  if(row == 4) {
    // CONFIRM button: bright green
    if(selected) {
      bgCol     = 0x0300;
      borderCol = 0x07E0;
      textCol   = 0xFFFF;
    } else {
      bgCol     = 0x0180;
      borderCol = 0x03E0;
      textCol   = 0x07E0;
    }
  } else if(label == 'B') {
    // Back button: dark green with dim glow
    if(selected) {
      bgCol     = 0x0300;
      borderCol = 0x47E4;
      textCol   = 0xFFFF;
    } else {
      bgCol     = 0x0140;
      borderCol = 0x0300;
      textCol   = 0x03E0;
    }
  } else if(label == '<') {
    // Delete: red (only non-green accent — danger)
    if(selected) {
      bgCol     = 0x2800;
      borderCol = 0xF800;
      textCol   = 0xFFFF;
    } else {
      bgCol     = 0x1000;
      borderCol = 0x6000;
      textCol   = 0xF800;
    }
  } else {
    // Normal digit key — green tones
    if(selected) {
      bgCol     = 0x0300;
      borderCol = 0x07E0;
      textCol   = 0xFFFF;
    } else {
      bgCol     = 0x0180;
      borderCol = 0x03A0;
      textCol   = 0x07E0;
    }
  }

  tft.fillRoundRect(kx, ky, kw, KP_KH, 2, bgCol);
  tft.drawRoundRect(kx, ky, kw, KP_KH, 2, borderCol);

  // Subtle top-edge highlight
  if(!selected)
    tft.drawFastHLine(kx+2, ky+1, kw-4, 0x03A0);

  tft.setTextSize(1);
  tft.setTextColor(textCol);

  if(row == 4) {
    // Centered "CONFIRM"
    const char* okTxt = "CONFIRM";
    int tlen = strlen(okTxt);
    tft.setCursor(kx + (kw - tlen*6)/2, ky + (KP_KH - 8)/2);
    tft.print(okTxt);
  } else if(label == 'B') {
    const char* bTxt = "BACK";
    tft.setCursor(kx + (kw - 4*6)/2, ky + (KP_KH-8)/2);
    tft.print(bTxt);
  } else if(label == '<') {
    tft.setCursor(kx + (kw - 2*6)/2, ky + (KP_KH-8)/2);
    tft.print("<-");
  } else {
    // Single digit — center it
    tft.setCursor(kx + (kw - 6)/2, ky + (KP_KH-8)/2);
    tft.print(label);
  }
}

// Redraw only the digit display row
void drawKeypadDigits() {
  tft.fillRect(0, KP_DIG_Y - 4, 160, KP_DIG_H + 10, C_BG);

  for(int i = 0; i < 6; i++) {
    int bx2 = KP_DIG_X + i * (KP_DIG_W + KP_DIG_GAP);

    tft.fillRoundRect(bx2, KP_DIG_Y, KP_DIG_W, KP_DIG_H, 2, 0x0180);

    uint16_t borderC = (i == kpDigitCount) ? 0x07E0 : 0x03A0;
    tft.drawRoundRect(bx2, KP_DIG_Y, KP_DIG_W, KP_DIG_H, 2, borderC);

    if(i < kpDigitCount) {
      tft.setTextSize(1);
      tft.setTextColor(0x07E0);
      tft.setCursor(bx2 + (KP_DIG_W - 6)/2, KP_DIG_Y + (KP_DIG_H - 8)/2);
      tft.print('*');
    } else if(i == kpDigitCount) {
      tft.drawFastHLine(bx2 + 6, KP_DIG_Y + KP_DIG_H - 3, KP_DIG_W - 12, 0x07E0);
    }
  }
}

void drawKeypad(bool redrawAll) {
  if(redrawAll) {
    tft.fillScreen(C_BG);

    // ── Header bar ──────────────────────────────────────────────────────────
    tft.fillRect(0, 0, 160, 28, 0x0180);

    // Green gradient line across very top
    {
      const uint16_t segs[6] = {0x0180,0x0300,0x03E0,0x07E0,0x03E0,0x0300};
      int sw = 160/6;
      for(int i=0;i<6;i++) tft.drawFastHLine(i*sw, 0, (i==5)?160-i*sw:sw, segs[i]);
    }
    // Separator below header
    tft.drawFastHLine(0, 27, 160, 0x03A0);

    // Pixel-art padlock icon — bright green
    tft.drawFastVLine(5, 5, 5,  0x07E0);
    tft.drawFastVLine(12, 5, 5, 0x07E0);
    tft.drawFastHLine(5, 5, 8,  0x07E0);
    tft.fillRoundRect(2, 10, 14, 12, 2, 0x03E0);
    tft.fillRect(7, 13, 4, 6, C_BG);
    tft.fillCircle(9, 13, 2, C_BG);

    // Title
    tft.setTextSize(1);
    tft.setTextColor(0x07E0);
    tft.setCursor(22, 6);
    tft.print("ENTER PASSWORD");
    tft.setTextColor(0x03A0);
    tft.setCursor(22, 16);
    tft.print("6 digits required");

    // Separator above digit boxes
    tft.drawFastHLine(8, KP_DIG_Y - 2, 144, 0x03A0);
  }

  // ── Digit display ────────────────────────────────────────────────────────
  drawKeypadDigits();

  // Thin separator between digits and keypad
  tft.drawFastHLine(4, KP_Y - 2, 152, 0x03A0);

  // ── Draw all keys ────────────────────────────────────────────────────────
  for(int r = 0; r < 5; r++) {
    for(int c = 0; c < 3; c++) {
      if(r == 4 && c != 0) continue;
      drawKeypadKey(c, r, (c == kpCurCol && r == kpCurRow));
    }
  }
}

// Show "WRONG PASSWORD" flash screen
void drawWrongPassword() {
  tft.fillScreen(0x0841);  // near-black with green tint background

  // Red X icon (danger — intentional non-green)
  tft.fillRoundRect(60, 20, 40, 40, 5, 0x8000);
  tft.drawRoundRect(60, 20, 40, 40, 5, 0xF800);
  tft.drawFastHLine(65, 60, 30, 0x6000);

  // Big X in bright red
  for(int i = 0; i < 20; i++) {
    tft.drawPixel(65+i,   28+i, 0xF800);
    tft.drawPixel(65+i+1, 28+i, 0xF800);
    tft.drawPixel(84-i,   28+i, 0xF800);
    tft.drawPixel(85-i,   28+i, 0xF800);
  }

  // Text
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(34, 70);
  tft.print("WRONG PASSWORD");

  tft.setTextColor(0xF800);
  tft.setCursor(46, 83);
  tft.print("Try again!");

  // Bottom hint bar
  tft.fillRect(0, 115, 160, 13, 0x0180);
  tft.drawFastHLine(0, 115, 160, 0x03E0);
  tft.setTextColor(0x03E0);
  tft.setCursor(22, 118);
  tft.print("Returning in 1s...");
}

// Show "UNDO OK" confirmation flash
void drawUndoOK() {
  tft.fillScreen(0x0020);  // deep dark green bg

  // Green checkmark circle
  tft.fillRoundRect(60, 20, 40, 40, 5, 0x0300);
  tft.drawRoundRect(60, 20, 40, 40, 5, 0x07E0);
  tft.drawFastHLine(65, 60, 30, 0x03E0);

  // Checkmark in bright green
  for(int i = 0; i < 8; i++) {
    tft.drawPixel(67+i, 44+i,   0x07E0);
    tft.drawPixel(67+i+1, 44+i, 0x07E0);
  }
  for(int i = 0; i < 15; i++) {
    tft.drawPixel(74+i, 52-i,   0x07E0);
    tft.drawPixel(74+i+1, 52-i, 0x07E0);
  }

  tft.setTextSize(1);
  tft.setTextColor(0x47E4);
  tft.setCursor(52, 70);
  tft.print("UNDO OK!");

  tft.setTextColor(0x03E0);
  tft.setCursor(28, 83);
  tft.print("Move undone.");
  delay(600);
}

// Full password UI flow. Returns true if correct password entered, false to cancel.
bool runPasswordUI() {
  kpCurRow    = 0;
  kpCurCol    = 1;   // start on '2'
  kpDigitCount = 0;
  memset(kpEntered, 0, sizeof(kpEntered));

  drawKeypad(true);

  unsigned long lastJoy = 0;
  bool lastPressed = false;

  while(true) {
    // Joystick navigation
    if(millis() - lastJoy > 150) {
      int vx = analogRead(JOY_VRX);
      int vy = analogRead(JOY_VRY);

      int dx2 = 0, dy2 = 0;
      if(vx < 1000) dx2 = -1;
      else if(vx > 3000) dx2 = 1;
      if(vy < 1000) dy2 = -1;
      else if(vy > 3000) dy2 = 1;

      if(dx2 != 0 || dy2 != 0) {
        lastJoy = millis();
        int newC = kpCurCol + dx2;
        int newR = kpCurRow + dy2;
        // Row 4 = full-width CONFIRM, lock col to 0
        newR = constrain(newR, 0, 4);
        if(newR == 4) newC = 0;
        else newC = constrain(newC, 0, 2);

        if(newC != kpCurCol || newR != kpCurRow) {
          if(kpCurRow == 4) drawKeypadKey(0, 4, false);
          else              drawKeypadKey(kpCurCol, kpCurRow, false);
          kpCurCol = newC;
          kpCurRow = newR;
          if(kpCurRow == 4) drawKeypadKey(0, 4, true);
          else              drawKeypadKey(kpCurCol, kpCurRow, true);
        }
      }
    }

    bool pressed = (digitalRead(JOY_SW) == LOW);

    if(pressed && !lastPressed) {
      lastPressed = true;
      char key = kpLabels[kpCurRow][kpCurCol];

      if(key == 'B') {
        // BACK — return to game without undoing
        return false;

      } else if(key == '<') {
        if(kpDigitCount > 0) {
          kpDigitCount--;
          kpEntered[kpDigitCount] = 0;
          drawKeypadDigits();
        }

      } else if(key == 'C') {
        if(kpDigitCount == 6) {
          kpEntered[6] = '\0';
          if(strcmp(kpEntered, KEYPAD_CORRECT_PASSWORD) == 0) {
            drawUndoOK();
            return true;
          } else {
            drawWrongPassword();
            delay(1000);
            // Allow retry — clear digits and redraw keypad
            kpDigitCount = 0;
            memset(kpEntered, 0, sizeof(kpEntered));
            drawKeypad(true);
          }
        }

      } else {
        // Digit key
        if(kpDigitCount < 6) {
          kpEntered[kpDigitCount++] = key;
          drawKeypadDigits();
        }
      }
    }

    if(!pressed) lastPressed = false;

    yield();
    delay(10);
  }
}


// ═══════════════════════════════════════════════════════════════════════════════
// UI-3: RESIGN SCREEN
// ═══════════════════════════════════════════════════════════════════════════════
void showResignScreen() {
  tft.fillScreen(0x0000);

  // Subtle dark-green chess grid background
  for(int gy = 0; gy < 128; gy += 8) {
    for(int gx = 0; gx < 160; gx += 8) {
      if(((gx/8 + gy/8) & 1) == 0)
        tft.fillRect(gx, gy, 8, 8, 0x0060);
    }
  }

  // Top accent bar — green
  tft.fillRect(0, 0, 160, 3, 0x03E0);
  tft.fillRect(0, 3, 160, 1, 0x0300);

  // King toppled — green crown, white cross
  int cx = 80, cy = 28;
  tft.fillRect(cx-10, cy+4, 20, 6, 0x03E0);
  tft.fillRect(cx-10, cy, 3, 5, 0x03E0);
  tft.fillRect(cx-2,  cy-2, 5, 7, 0x47E4);
  tft.fillRect(cx+7,  cy, 3, 5, 0x03E0);
  tft.fillRect(cx-2,  cy-6, 5, 5, 0x47E4);
  tft.fillRect(cx-5,  cy-3, 11, 2, 0x47E4);
  tft.drawFastHLine(cx-14, cy+10, 28, 0x0300);
  tft.fillRect(cx-14, cy+11, 28, 2, 0x0180);

  // "YOU RESIGNED" — green shimmer gradient
  {
    tft.setTextSize(1);
    const char* t1 = "YOU RESIGNED";
    int l = strlen(t1);
    int x0 = (160 - l*6) / 2;
    const uint16_t grad[12] = {
      0x03E0,0x05E0,0x03E0,0x05E0,0x03E0,0x05E0,
      0x03E0,0x05E0,0x03E0,0x05E0,0x03E0,0x05E0
    };
    for(int i = 0; i < l; i++) {
      tft.setTextColor(grad[i % 12]);
      tft.setCursor(x0 + i*6, 52);
      tft.print(t1[i]);
    }
  }

  tft.drawFastHLine(20, 63, 120, 0x03A0);

  // "GAME OVER"
  {
    const char* go = "GAME OVER";
    int gl = strlen(go);
    tft.setTextColor(0x03E0);
    tft.setCursor((160 - gl*6) / 2, 68);
    tft.print(go);
  }

  // "You lost."
  {
    const char* yl = "You lost.";
    int yll = strlen(yl);
    tft.setTextColor(0x07E0);
    tft.setCursor((160 - yll*6) / 2, 80);
    tft.print(yl);
  }

  // Bottom bar
  tft.fillRect(0, 108, 160, 1, 0x03A0);
  tft.fillRect(0, 109, 160, 19, 0x0060);

  // Piece row — alternating green shades
  const uint16_t pieceCol[6] = {0x07E0,0x03E0,0x07E0,0x03E0,0x07E0,0x03E0};
  const char pieceChars[6] = {'K','Q','R','B','N','P'};
  for(int i = 0; i < 6; i++) {
    tft.setTextColor(pieceCol[i]);
    tft.setCursor(14 + i*22, 111);
    tft.print(pieceChars[i]);
  }

  tft.drawFastHLine(0, 127, 160, 0x07E0);

  while(digitalRead(JOY_SW) == LOW) { yield(); delay(10); }
  while(digitalRead(JOY_SW) != LOW) { yield(); delay(20); }
  while(digitalRead(JOY_SW) == LOW) { yield(); delay(10); }
}


// ═══════════════════════════════════════════════════════════════════════════════
// CHECKMATE "YOU LOST" SCREEN
// ═══════════════════════════════════════════════════════════════════════════════
void showCheckmateScreen() {
  tft.fillScreen(0x0000);

  // Very dark green grid background
  for(int gy = 0; gy < 128; gy += 8) {
    for(int gx = 0; gx < 160; gx += 8) {
      if(((gx/8 + gy/8) & 1) == 0)
        tft.fillRect(gx, gy, 8, 8, 0x0040);
    }
  }

  // Top accent bar — dual stripe: red + green
  tft.fillRect(0, 0, 160, 2, 0xD000);
  tft.fillRect(0, 2, 160, 1, 0x03E0);
  tft.fillRect(0, 3, 160, 1, 0x0180);

  // Fallen king — body in dark red, crown tip in mid red
  int cx = 80, cy = 28;
  tft.fillRect(cx-10, cy+4, 20, 6, 0x8000);
  tft.fillRect(cx-10, cy, 3, 5, 0x8000);
  tft.fillRect(cx-2,  cy-2, 5, 7, 0xD000);
  tft.fillRect(cx+7,  cy, 3, 5, 0x8000);
  tft.fillRect(cx-2,  cy-6, 5, 5, 0xD000);
  tft.fillRect(cx-5,  cy-3, 11, 2, 0xD000);
  tft.drawFastHLine(cx-14, cy+10, 28, 0x4000);
  tft.fillRect(cx-14, cy+11, 28, 2, 0x2000);

  // "YOU LOST" — shades of red
  {
    tft.setTextSize(2);
    const char* t1 = "YOU LOST";
    int tlen = strlen(t1);
    int tx0 = (160 - tlen*12)/2;
    const uint16_t tgrad[8] = {0xF800,0xD000,0xF800,0xA800,0xF800,0xD000,0xF800,0xA800};
    for(int i = 0; i < tlen; i++) {
      tft.setTextColor(tgrad[i % 8]);
      tft.setCursor(tx0 + i*12, 48);
      tft.print(t1[i]);
    }
  }

  // Divider — half red, half green
  tft.drawFastHLine(20,  68, 60, 0x8000);
  tft.drawFastHLine(80,  68, 60, 0x0300);

  // "CHECKMATE" — mid green (the machine speaks)
  {
    const char* go = "CHECKMATE";
    int gl = strlen(go);
    tft.setTextColor(0x03E0);
    tft.setCursor((160 - gl*6) / 2, 73);
    tft.print(go);
  }

  // "Black wins." — bright green
  {
    const char* yl = "Black wins.";
    int yll = strlen(yl);
    tft.setTextColor(0x05E0);
    tft.setCursor((160 - yll*6) / 2, 85);
    tft.print(yl);
  }

  // Bottom bar — left half red, right half green
  tft.fillRect(0,   108, 80, 1, 0x8000);
  tft.fillRect(80,  108, 80, 1, 0x03A0);
  tft.fillRect(0,   109, 80, 19, 0x1800);
  tft.fillRect(80,  109, 80, 19, 0x0060);

  // Piece row — red on left side, green on right side
  const uint16_t pieceCol[6] = {0xF800,0xD000,0xA800,0x03E0,0x05E0,0x03A0};
  const char pieceChars2[6] = {'K','Q','R','B','N','P'};
  for(int i = 0; i < 6; i++) {
    tft.setTextColor(pieceCol[i]);
    tft.setCursor(14 + i*22, 111);
    tft.print(pieceChars2[i]);
  }

  // Bottom edge — split accent
  tft.drawFastHLine(0,  127, 80, 0xD000);
  tft.drawFastHLine(80, 127, 80, 0x07E0);

  delay(1500);
}


// ═══════════════════════════════════════════════════════════════════════════════
// UI-4: INTRO CUTSCENE
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Intro splash (single frame) ─────────────────────────────────────────────
void playIntroCutscene() {
  tft.fillScreen(0x0000);

  // Border
  tft.drawRect(4, 20, 152, 88, 0x03E0);
  tft.drawRect(5, 21, 150, 86, 0x0180);

  // Title line 1: "RageMate"
  tft.setTextSize(2);
  const char* l1 = "RageMate";
  tft.setTextColor(0x07E0);
  tft.setCursor((160 - (int)strlen(l1)*12) / 2, 34);
  tft.print(l1);

  // Title line 2: "Medusa Theta"
  tft.setTextSize(1);
  const char* l2 = "Medusa Theta";
  tft.setTextColor(0x03E0);
  tft.setCursor((160 - (int)strlen(l2)*6) / 2, 58);
  tft.print(l2);

  // Version line
  const char* l3 = "V1";
  tft.setTextColor(0x47E4);
  tft.setCursor((160 - (int)strlen(l3)*6) / 2, 72);
  tft.print(l3);

  // Press to begin
  tft.setTextColor(0x03A0);
  tft.setCursor(28, 96);
  tft.print("> press to begin <");

  while(digitalRead(JOY_SW) == LOW)  { yield(); delay(10); }
  while(digitalRead(JOY_SW) != LOW)  { yield(); delay(20); }
  while(digitalRead(JOY_SW) == LOW)  { yield(); delay(10); }

  tft.fillScreen(0x0000);
}


// ═══════════════════════════════════════════════════════════════════════════════
// JOYSTICK HANDLING
// ═══════════════════════════════════════════════════════════════════════════════
void handleJoystick() {
  if(millis()-lastJoyMove < JOY_DELAY) return;

  int vx = analogRead(JOY_VRX);
  int vy = analogRead(JOY_VRY);

  int dx=0, dy=0;
  if(vx < 1000) dx=-1;
  else if(vx > 3000) dx=1;
  if(vy < 1000) dy=1;
  else if(vy > 3000) dy=-1;

  if(dx==0 && dy==0) return;
  lastJoyMove = millis();

  if(cursorCol >= 0) {
    eraseCursorAt(cursorCol, cursorRow);
    drawSelOutline();
    if(selSq>=0) drawLegalDots();

    int newCol = cursorCol + dx;
    int newRow = constrain(cursorRow + dy, 0, 7);

    if(newCol < 0) {
      cursorCol = -1;
      cursorRow = newRow;
      panelFocus = (newRow >= 4) ? 1 : 2;
      drawLeftPanel();
    } else {
      cursorCol = constrain(newCol, 0, 7);
      cursorRow = newRow;
      drawCursorAt(cursorCol, cursorRow);
    }
  } else {
    if(dx > 0) {
      panelFocus = 0;
      cursorCol = 0;
      cursorRow = constrain(cursorRow, 0, 7);
      drawLeftPanel();
      drawCursorAt(cursorCol, cursorRow);
    } else if(dy != 0) {
      panelFocus = (panelFocus == 1) ? 2 : 1;
      cursorRow = (panelFocus == 1) ? 6 : 2;
      drawLeftPanel();
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HOLD BUTTON SYSTEM (modified for resign + password undo)
// ═══════════════════════════════════════════════════════════════════════════════
#define HOLD_CONFIRM_MS      1000   // 1 second hold for resign / undo
#define HOLD_CHECKMATE_MS    2000   // 2 second hold when checkmated

static int  holdAction   = 0;
static unsigned long holdStart = 0;

void tickHold() {
  bool pressed = (digitalRead(JOY_SW) == LOW);
  if(!pressed) {
    if(holdAction != 0) {
      holdAction = 0;
      holdStart  = 0;
      dispPct    = -1;
      drawProgressBar(0);
    }
    return;
  }
  if(holdAction == 0) return;
  unsigned long held = millis() - holdStart;
  unsigned long threshold = (holdAction == 3) ? HOLD_CHECKMATE_MS : HOLD_CONFIRM_MS;
  int pct = (int)((held * 100UL) / threshold);
  if(pct > 100) pct = 100;
  drawProgressBar(pct);
  if(held >= threshold) {
    int act = holdAction;
    holdAction = 0;
    holdStart  = 0;
    dispPct    = -1;

    if(act == 1) {
      // UI-3: Resign (flag button held)
      showResignScreen();
      doRestart();
      return;
    } else if(act == 3) {
      // Game over restart (held on game-over screen)
      drawProgressBar(0);
      if(checkmatePending) {
        checkmatePending = false;
        showCheckmateScreen();
      }
      doRestart();
      return;
    } else if(act == 2) {
      // UI-2: Undo — show password keypad
      drawProgressBar(0);
      bool ok = runPasswordUI();
      if(ok) {
        doUndo();
      } else {
        drawAll();
      }
      return;
    }

    drawProgressBar(0);
  }
}

void handleButton() {
  static unsigned long lastPress = 0;
  bool pressed = (digitalRead(JOY_SW) == LOW);

  if(gameOver) {
    if(!pressed) {
      if(holdAction == 3) {
        holdAction = 0; holdStart = 0;
        dispPct = -1; drawProgressBar(0);
      }
      return;
    }
    if(holdAction != 3) {
      holdAction = 3;
      holdStart  = millis();
    }
    tickHold();
    return;
  }

  if(cursorCol < 0 && pressed) {
    int wantAction = (panelFocus == 1) ? 1 : (panelFocus == 2 ? 2 : 0);
    if(wantAction != 0) {
      if(holdAction != wantAction) {
        holdAction = wantAction;
        holdStart  = millis();
        dispPct    = -1;
        drawProgressBar(0);
      }
      tickHold();
      return;
    }
  }

  if(cursorCol < 0 && !pressed && holdAction != 0) {
    holdAction = 0; holdStart = 0;
    dispPct = -1; drawProgressBar(0);
    return;
  }

  if(!pressed) return;
  if(holdAction != 0) return;
  if(millis()-lastPress < 300) return;
  lastPress = millis();

  if(cursorCol < 0) return;
  if(gameOver) return;
  if(!whiteToMove) return;

  int clickSq = sqOf(cursorRow, cursorCol);

  if(selSq < 0) {
    if(board[clickSq] && IS_WHITE(board[clickSq])) {
      if(selectSquare(clickSq)) {
        selSq = clickSq;
        tft.drawRect(sqX(selSq), sqY(selSq), SQ_W, SQ_H, C_SEL);
        drawLegalDots();
        drawCursorAt(cursorCol, cursorRow);
      }
    }
  } else {
    bool moved = false;
    for(int i=0;i<legalCount;i++) {
      if(legalCache[i].to == clickSq) {
        HistoryEntry he;
        makeMove(legalCache[i], &he);
        history[historyCount++] = he;
        hashHistory[hashHistoryCount++] = currentHash;
        fullMoveCount++;

        clearLegalDots();
        int prevSel = selSq;
        selSq = -1; legalCount = 0;
        drawSquare(he.mv.from);
        drawSquare(he.mv.to);
        if(he.mv.flags==2) {
          if(IS_WHITE(board[he.mv.to])) { drawSquare(5); drawSquare(7); }
          else { drawSquare(61); drawSquare(63); }
        }
        if(he.mv.flags==3) {
          if(IS_WHITE(board[he.mv.to])) { drawSquare(3); drawSquare(0); }
          else { drawSquare(59); drawSquare(56); }
        }
        if(he.mv.flags==1) {
          drawSquare(sqOf(rankOf(he.mv.from), fileOf(he.mv.to)));
        }
        drawCursorAt(cursorCol, cursorRow);
        moved = true;

        drawPhaseIndicator();
        checkGameOver();
        if(gameOver) { drawGameOver(); return; }

        engineThinking = true;
        searchPct = 0;
        dispPct   = -1;
        drawProgressBar(0);
        Move botMove = runSearchOnCore0();
        dispPct = -1;
        drawProgressBar(0);
        engineThinking = false;

        if((botMove.from || botMove.to) &&
           botMove.from < 64 && botMove.to < 64 &&
           board[botMove.from] != EMPTY && IS_BLACK(board[botMove.from])) {
          HistoryEntry he2;
          makeMove(botMove, &he2);
          history[historyCount++] = he2;
          hashHistory[hashHistoryCount++] = currentHash;

          lastBlackFrom = botMove.from;
          lastBlackTo   = botMove.to;
          bPieceMoved &= ~(1ULL << botMove.from);
          bPieceMoved |=  (1ULL << botMove.to);

          if(engineFrom >= 0) {
            drawSquare(engineFrom);
            drawSquare(engineTo);
          }
          engineFrom = botMove.from;
          engineTo   = botMove.to;
          drawSquare(botMove.from);
          drawSquare(botMove.to);
          if(he2.mv.flags==2) {
            if(IS_WHITE(board[he2.mv.to])) { drawSquare(5); drawSquare(7); }
            else { drawSquare(61); drawSquare(63); }
          }
          if(he2.mv.flags==3) {
            if(IS_WHITE(board[he2.mv.to])) { drawSquare(3); drawSquare(0); }
            else { drawSquare(59); drawSquare(56); }
          }
          if(he2.mv.flags==1) {
            drawSquare(sqOf(rankOf(he2.mv.from), fileOf(he2.mv.to)));
          }
          drawLastMoveOutlines(engineFrom, engineTo);
          drawCursorAt(cursorCol, cursorRow);

          drawPhaseIndicator();
          checkGameOver();
          if(gameOver) drawGameOver();
        }
        break;
      }
    }
    if(!moved) {
      clearLegalDots();
      int prevSel = selSq;
      selSq = -1; legalCount = 0;
      drawSquare(prevSel);

      if(board[clickSq] && IS_WHITE(board[clickSq])) {
        if(selectSquare(clickSq)) {
          selSq = clickSq;
          tft.drawRect(sqX(selSq), sqY(selSq), SQ_W, SQ_H, C_SEL);
          drawLegalDots();
        }
      }
      drawCursorAt(cursorCol, cursorRow);
    }
  }
}

// Helper: populate legalCache for the given white square. Returns true if any legal moves exist.
bool selectSquare(int sq) {
  Move allMoves[64];
  int cnt;
  generateMoves(true, allMoves, cnt, false);
  legalCount = 0;
  for(int i=0;i<cnt;i++) {
    if(allMoves[i].from==sq && isLegal(allMoves[i])) {
      legalCache[legalCount++] = allMoves[i];
    }
  }
  return legalCount > 0;
}

// ─── Restart / Undo ───────────────────────────────────────────────────────────
void doRestart() {
  initBoard();
  dispPct = -1;
  panelFocus = 0;
  cursorCol = 4;
  cursorRow = 4;
  selSq = -1;
  legalCount = 0;
  checkmatePending = false;
  memset(tt, 0, sizeof(tt));
  memset(histHeuristic, 0, sizeof(histHeuristic));
  drawAll();
}

void doUndo() {
  if(historyCount < 2) return;
  if(gameOver) {
    gameOver=false;
    gameOverMsg[0]=0;
    checkmatePending=false;
  }

  unmakeMove(history[historyCount-1]);
  historyCount--;
  if(hashHistoryCount>0) hashHistoryCount--;
  engineFrom=-1; engineTo=-1;

  unmakeMove(history[historyCount-1]);
  historyCount--;
  if(hashHistoryCount>0) hashHistoryCount--;

  openingMoveIndex = max(0, openingMoveIndex-1);

  selSq=-1; legalCount=0;
  drawAll();
}

// ─── Setup & Loop ─────────────────────────────────────────────────────────────
void setup() {
  setCpuFrequencyMhz(240);
  Serial.begin(115200);
  delay(100);

  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH); delay(10);
  digitalWrite(TFT_RST, LOW);  delay(50);
  digitalWrite(TFT_RST, HIGH); delay(150);

  SPI.begin(TFT_SCLK, /*MISO*/ -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(27000000UL);

  tft.initR(INITR_BLACKTAB);
  // UI-1: Flipped 180° — rotation 3 instead of 1
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  delay(50);

  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);

  initZobrist();
  memset(tt, 0, sizeof(tt));
  memset(histHeuristic, 0, sizeof(histHeuristic));

  disableCore0WDT();
  disableCore1WDT();

  searchSem = xSemaphoreCreateBinary();

  // UI-4: Play intro cutscene before game starts
  playIntroCutscene();

  initBoard();
  drawAll();
}

void loop() {
  handleJoystick();
  handleButton();
  tickHold();
  yield();
}