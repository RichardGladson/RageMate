/*
 * Medusa Beta Chess Engine — v3 UI Edition
 * Changes vs v2:
 *  UI-1. Display flipped 180° (setRotation 3)
 *  UI-2. Undo requires 6-digit password via on-screen keypad
 *  UI-3. Reset button replaced with drawn red flag icon; hold=resign screen
 *  UI-4. Intro cutscene: "Richard presents" → "RageMate <name> <version>"
 *        with scrolling name/version animations
 *  UI-5. Top-notch UI quality throughout all new screens
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
#define SEARCH_TIME_MS 3000

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
  bool bQueenOnBoard = false;
  for(int s=0;s<64;s++) {
    int p=board[s]; if(!p) continue;
    int t=TYPE(p);
    if(t!=KING) total += PIECE_VAL[t];
    if(p==B_Q) bQueenOnBoard=true;
  }

  int phase;
  if(total < 1800) {
    phase = 2;
  } else {
    if(!bQueenOnBoard) {
      phase = 1;
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

// ═══════════════════════════════════════════════════════════════════════════════
// MEDUSA POSITIONAL INTELLIGENCE SYSTEM
// ─────────────────────────────────────────────────────────────────────────────
// Layers:
//  1. Positional Genome Scanner  — multi-axis fingerprint of the position
//  2. Gambit Signature Matcher   — detects known opening patterns, loads doctrine
//  3. Thermodynamic Pawn Map     — heat/cold zones, identifies breaks
//  4. Style Reactor              — 5-personality blend driven by genome
//  5. Evaluation delta           — all above applied as additive score adjustments
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Layer 1: Positional Genome ───────────────────────────────────────────────
struct PositionalGenome {
  int pawn_tension;       // number of direct pawn confrontations
  int king_exposure;      // (black king attackers) - (white king attackers): >0 = black unsafe
  int piece_activity;     // black mobility - white mobility (legal square count proxy)
  int center_control;     // control of e4/d4/e5/d5: >0 = white dominates
  int open_file_pressure; // open/semi-open files with our rooks vs theirs
  int sacrifice_signal;   // material imbalance relative to initiative factors
  int tempo_debt;         // undev white minors - undev black minors (>0 = white behind)
};

// Count squares a piece on `sq` can move to (fast mobility proxy, no legality check)
static int mobilityProxy(int sq) {
  int p = board[sq]; if(!p) return 0;
  int r = rankOf(sq), f = fileOf(sq), mob = 0;
  switch(TYPE(p)) {
    case KNIGHT: {
      const int kd[8][2]={{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
      for(auto& d : kd) if(inBounds(r+d[0],f+d[1])) mob++;
      break;
    }
    case BISHOP: {
      const int bd[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
      for(auto& d : bd) {
        int rr=r+d[0], ff=f+d[1];
        while(inBounds(rr,ff)) { mob++; if(board[sqOf(rr,ff)]) break; rr+=d[0]; ff+=d[1]; }
      }
      break;
    }
    case ROOK: {
      const int rd[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
      for(auto& d : rd) {
        int rr=r+d[0], ff=f+d[1];
        while(inBounds(rr,ff)) { mob++; if(board[sqOf(rr,ff)]) break; rr+=d[0]; ff+=d[1]; }
      }
      break;
    }
    case QUEEN: {
      const int qd[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
      for(auto& d : qd) {
        int rr=r+d[0], ff=f+d[1];
        while(inBounds(rr,ff)) { mob++; if(board[sqOf(rr,ff)]) break; rr+=d[0]; ff+=d[1]; }
      }
      break;
    }
    default: mob = 1; break;
  }
  return mob;
}

PositionalGenome computeGenome() {
  PositionalGenome g = {0,0,0,0,0,0,0};

  // Pawn tension: pairs of opposing pawns facing each other diagonally
  for(int s=0;s<64;s++) {
    if(board[s]==W_P) {
      int r=rankOf(s), f=fileOf(s);
      for(int df=-1;df<=1;df+=2) {
        if(inBounds(r+1,f+df) && board[sqOf(r+1,f+df)]==B_P) g.pawn_tension++;
      }
    }
  }

  // King exposure: attacker count around each king
  int wKsq=-1, bKsq=-1;
  for(int s=0;s<64;s++) {
    if(board[s]==W_K) wKsq=s;
    if(board[s]==B_K) bKsq=s;
  }
  auto countKingAttackers = [&](int ksq, bool byWhite) -> int {
    if(ksq<0) return 0;
    int cnt=0;
    int kr=rankOf(ksq), kf=fileOf(ksq);
    for(int dr=-2;dr<=2;dr++) for(int df=-2;df<=2;df++) {
      if(dr==0&&df==0) continue;
      if(!inBounds(kr+dr,kf+df)) continue;
      int p=board[sqOf(kr+dr,kf+df)];
      if(p && (byWhite ? IS_WHITE(p) : IS_BLACK(p)) && TYPE(p)!=KING) cnt++;
    }
    return cnt;
  };
  g.king_exposure = countKingAttackers(bKsq,true) - countKingAttackers(wKsq,false);

  // Piece activity: mobility differential (black - white, so positive = black more active)
  int wMob=0, bMob=0;
  for(int s=0;s<64;s++) {
    if(!board[s]) continue;
    int m=mobilityProxy(s);
    if(IS_WHITE(board[s])) wMob+=m; else bMob+=m;
  }
  g.piece_activity = bMob - wMob;  // positive = black more active (bad for us as black)

  // Center control: attacks on e4/d4/e5/d5 (white squares = sq 27,28,35,36)
  // Positive = white dominates center
  const int centerSqs[4] = {sqOf(3,3),sqOf(3,4),sqOf(4,3),sqOf(4,4)};
  for(int cs : centerSqs) {
    if(isSquareAttacked(cs,true))  g.center_control++;
    if(isSquareAttacked(cs,false)) g.center_control--;
    int p=board[cs];
    if(p) {
      if(IS_WHITE(p)) g.center_control++;
      else            g.center_control--;
    }
  }

  // Open file pressure
  for(int f=0;f<8;f++) {
    bool wp=false, bp=false, wr=false, br=false;
    for(int r=0;r<8;r++) {
      int p=board[sqOf(r,f)];
      if(p==W_P) wp=true; if(p==B_P) bp=true;
      if(p==W_R||p==W_Q) wr=true; if(p==B_R||p==B_Q) br=true;
    }
    if(!bp && wr) g.open_file_pressure++;   // black has open file with our rook
    if(!wp && br) g.open_file_pressure--;   // white has open file with their rook
  }

  // Sacrifice signal: material imbalance — if we're up material significantly vs initiative
  int wMat=0, bMat=0;
  for(int s=0;s<64;s++) {
    int p=board[s]; if(!p||TYPE(p)==KING) continue;
    if(IS_WHITE(p)) wMat+=PIECE_VAL[TYPE(p)];
    else            bMat+=PIECE_VAL[TYPE(p)];
  }
  g.sacrifice_signal = wMat - bMat;  // positive = white up material

  // Tempo debt: undeveloped minor pieces (positive = white behind in development)
  int wUndev=0, bUndev=0;
  if(board[sqOf(0,1)]==W_N) wUndev++;
  if(board[sqOf(0,6)]==W_N) wUndev++;
  if(board[sqOf(0,2)]==W_B) wUndev++;
  if(board[sqOf(0,5)]==W_B) wUndev++;
  if(board[sqOf(7,1)]==B_N) bUndev++;
  if(board[sqOf(7,6)]==B_N) bUndev++;
  if(board[sqOf(7,2)]==B_B) bUndev++;
  if(board[sqOf(7,5)]==B_B) bUndev++;
  g.tempo_debt = wUndev - bUndev;

  return g;
}

// ─── Layer 2: Gambit Signature Matcher ───────────────────────────────────────
// Doctrine IDs — how the engine should respond to detected opening
#define DOCTRINE_NONE         0
#define DOCTRINE_NEUTRALIZE   1   // Solid response, no panic, consolidate
#define DOCTRINE_SUBVERT      2   // Exploit tempo debt, punish premature attacks
#define DOCTRINE_FORTRESS     3   // Lock down king safety, build pawn wall
#define DOCTRINE_COUNTER_ATK  4   // Direct queenside/center counter-attack
#define DOCTRINE_STRANGLE     5   // Space control, slow suffocation
#define DOCTRINE_ACCEPT_GAMBIT 6  // Accept material, trade down to endgame

struct GambitDoctrine {
  int  id;
  int  personality_strangler;   // 0-10 blend weight
  int  personality_pyromaniac;
  int  personality_fortress;
  int  personality_subverter;
  // Evaluation nudges (applied as score deltas for black)
  int  center_break_bonus;      // bonus for d5/e5 breaks
  int  king_safety_premium;     // extra weight on king shelter
  int  piece_trade_bonus;       // bonus for trading pieces (simplify into endgame)
  int  initiative_cancel_bonus; // bonus for moves that neutralize opponent tempo
};

// Detect what opening white is playing and return a doctrine
GambitDoctrine detectAndLoadDoctrine() {
  GambitDoctrine d;
  d.id = DOCTRINE_NONE;
  d.personality_strangler = 2;
  d.personality_pyromaniac = 2;
  d.personality_fortress = 2;
  d.personality_subverter = 2;
  d.center_break_bonus = 0;
  d.king_safety_premium = 0;
  d.piece_trade_bonus = 0;
  d.initiative_cancel_bonus = 0;

  if(historyCount < 2) return d;  // too early

  // Board pattern shortcuts
  bool wE4 = (board[sqOf(3,4)]==W_P);
  bool wD4 = (board[sqOf(3,3)]==W_P);
  bool wC4 = (board[sqOf(3,2)]==W_P);
  bool wF4 = (board[sqOf(3,5)]==W_P);
  bool bE5 = (board[sqOf(4,4)]==B_P);
  bool bD5 = (board[sqOf(4,3)]==B_P);
  bool bNf6= (board[sqOf(5,5)]==B_N);
  bool bC6 = (board[sqOf(5,2)]==B_P);
  bool wNf3= (board[sqOf(2,5)]==W_N);
  bool wBc4= (board[sqOf(2,2)]==W_B);
  bool wBf4= (board[sqOf(2,5)]==W_B);   // London bishop
  bool wNd2= (board[sqOf(1,3)]==W_N);
  bool wPf4= (board[sqOf(3,5)]==W_P);   // f4 push

  // ── King's Gambit: 1.e4 e5 2.f4 ─────────────────────────────────────────
  // White sacrifices f-pawn for rapid development and f-file attack.
  // Counter-doctrine: Subverter+Pyromaniac — accept if safe, build counterplay,
  // use the extra pawn to win the endgame.
  if(wE4 && bE5 && wPf4) {
    d.id = DOCTRINE_ACCEPT_GAMBIT;
    d.personality_subverter   = 7;
    d.personality_pyromaniac  = 5;
    d.personality_fortress    = 4;
    d.personality_strangler   = 1;
    d.center_break_bonus      = 60;  // d5 counter-break is critical
    d.king_safety_premium     = 40;  // king is exposed — shelter first
    d.piece_trade_bonus       = 35;  // trade down — extra pawn wins ending
    d.initiative_cancel_bonus = 50;  // cancel white's f-file initiative
    return d;
  }

  // ── Queen's Gambit: 1.d4 d5 2.c4 ────────────────────────────────────────
  // White pressures d5 pawn. Don't accept carelessly.
  // Counter-doctrine: Strangler — hold d5, develop solidly, fight for center.
  if(wD4 && bD5 && wC4) {
    d.id = DOCTRINE_STRANGLE;
    d.personality_strangler   = 8;
    d.personality_subverter   = 4;
    d.personality_fortress    = 3;
    d.personality_pyromaniac  = 1;
    d.center_break_bonus      = 50;  // c5 break is the thermal release valve
    d.king_safety_premium     = 20;
    d.piece_trade_bonus       = 20;
    d.initiative_cancel_bonus = 30;
    return d;
  }

  // ── London System: 1.d4 + Bf4/Nf3 + e3 pawn chain ───────────────────────
  // White builds a solid boring structure. Fight for same space, create asymmetry.
  // Counter-doctrine: Strangler — contest the c4 and e5 squares, avoid symmetry.
  if(wD4 && wNf3 && wBf4) {
    d.id = DOCTRINE_STRANGLE;
    d.personality_strangler   = 9;
    d.personality_subverter   = 3;
    d.personality_fortress    = 2;
    d.personality_pyromaniac  = 1;
    d.center_break_bonus      = 45;  // e5 or c5 break creates asymmetry
    d.king_safety_premium     = 15;
    d.piece_trade_bonus       = -20; // avoid trades — London wants symmetry, we want life
    d.initiative_cancel_bonus = 25;
    return d;
  }

  // ── Italian Game / Giuoco Piano: 1.e4 e5 2.Nf3 Nc6 3.Bc4 ────────────────
  // Slow buildup. Counter with d6/d5 central break.
  if(wE4 && bE5 && wNf3 && wBc4) {
    d.id = DOCTRINE_COUNTER_ATK;
    d.personality_subverter   = 5;
    d.personality_strangler   = 4;
    d.personality_pyromaniac  = 3;
    d.personality_fortress    = 3;
    d.center_break_bonus      = 40;
    d.king_safety_premium     = 20;
    d.piece_trade_bonus       = 10;
    d.initiative_cancel_bonus = 20;
    return d;
  }

  // ── Scandinavian Defense mirror: if white pushes e4 and we played d5 ─────
  // Black already challenged center. Continue pressure on d-file.
  if(wE4 && bD5) {
    d.id = DOCTRINE_COUNTER_ATK;
    d.personality_pyromaniac  = 6;
    d.personality_subverter   = 5;
    d.personality_fortress    = 2;
    d.personality_strangler   = 2;
    d.center_break_bonus      = 55;
    d.king_safety_premium     = 25;
    d.piece_trade_bonus       = 15;
    d.initiative_cancel_bonus = 35;
    return d;
  }

  // ── English Opening: 1.c4 ────────────────────────────────────────────────
  // Flank control without occupying center. Fight back through center directly.
  if(wC4 && !wD4 && !wE4) {
    d.id = DOCTRINE_COUNTER_ATK;
    d.personality_strangler   = 5;
    d.personality_subverter   = 5;
    d.personality_pyromaniac  = 3;
    d.personality_fortress    = 2;
    d.center_break_bonus      = 50;
    d.king_safety_premium     = 15;
    d.piece_trade_bonus       = 10;
    d.initiative_cancel_bonus = 20;
    return d;
  }

  // ── Bird's Opening: 1.f4 ─────────────────────────────────────────────────
  // White weakens kingside. Attack it directly with From's Gambit idea (e5).
  if(wF4 && !wE4 && !wD4) {
    d.id = DOCTRINE_COUNTER_ATK;
    d.personality_pyromaniac  = 8;
    d.personality_subverter   = 5;
    d.personality_fortress    = 2;
    d.personality_strangler   = 1;
    d.center_break_bonus      = 65;  // e5 is the classic counter
    d.king_safety_premium     = 10;
    d.piece_trade_bonus       = 0;
    d.initiative_cancel_bonus = 45;
    return d;
  }

  // ── Default: solid, balanced ─────────────────────────────────────────────
  d.id = DOCTRINE_NEUTRALIZE;
  d.personality_strangler   = 3;
  d.personality_subverter   = 3;
  d.personality_fortress    = 3;
  d.personality_pyromaniac  = 2;
  d.center_break_bonus      = 20;
  d.king_safety_premium     = 20;
  d.piece_trade_bonus       = 0;
  d.initiative_cancel_bonus = 15;
  return d;
}

// ─── Layer 3: Thermodynamic Pawn Map ─────────────────────────────────────────
// For each file, compute how "hot" (contested/breakthrough potential) it is.
// Returns a heat value per file: positive = white pressure, negative = black pressure.
// Used to steer evaluation toward the correct flank.
void computePawnHeatMap(int heatMap[8]) {
  for(int f=0;f<8;f++) heatMap[f]=0;

  for(int s=0;s<64;s++) {
    int p=board[s]; if(!p) continue;
    if(TYPE(p)!=PAWN) continue;
    int f=fileOf(s), r=rankOf(s);
    if(IS_WHITE(p)) {
      // Forward heat: rank advancement = more pressure upward
      heatMap[f] += r * 2;
      // Diagonal coverage
      if(f>0) heatMap[f-1] += r;
      if(f<7) heatMap[f+1] += r;
    } else {
      // Black pawn pushes downward
      int br = 7 - r;
      heatMap[f] -= br * 2;
      if(f>0) heatMap[f-1] -= br;
      if(f<7) heatMap[f+1] -= br;
    }
  }
}

// ─── Layer 4: Style Reactor ───────────────────────────────────────────────────
// Given a genome and a doctrine, compute evaluation deltas for the position.
// All deltas are from Black's perspective (positive = good for Black).
int styleReactorDelta(const PositionalGenome& g, const GambitDoctrine& doc,
                      int bKsq, int wKsq) {
  int delta = 0;

  // ── The Strangler ──────────────────────────────────────────────────────────
  // Thrives when center control is being contested. Rewards space restriction.
  {
    int w = doc.personality_strangler;
    if(w > 0) {
      // Reward controlling center as black
      delta += (-g.center_control) * w * 3;
      // Reward having less pawn tension (locked = strangled)
      if(g.pawn_tension <= 1) delta += w * 5;
      // Reward open file pressure in our favor
      if(g.open_file_pressure < 0) delta += (-g.open_file_pressure) * w * 2;
    }
  }

  // ── The Pyromaniac ────────────────────────────────────────────────────────
  // Loves open files, piece activity, sacrifices with compensation.
  {
    int w = doc.personality_pyromaniac;
    if(w > 0) {
      // Reward piece activity
      if(g.piece_activity < 0) delta += (-g.piece_activity) * w * 2;
      // Reward open file pressure
      if(g.open_file_pressure < 0) delta += (-g.open_file_pressure) * w * 3;
      // Slight bonus for pawn tension (creates open lines)
      delta += g.pawn_tension * w;
    }
  }

  // ── The Fortress ──────────────────────────────────────────────────────────
  // Activates when the black king is at risk. Locks down the position.
  {
    int w = doc.personality_fortress;
    if(w > 0 && bKsq >= 0) {
      // If black king is exposed (many white attackers near king)
      if(g.king_exposure > 2) {
        delta -= g.king_exposure * w * 8;  // heavily penalize king exposure
      }
      // Reward king safety premium from doctrine
      delta += doc.king_safety_premium * w / 3;
    }
  }

  // ── The Subverter ─────────────────────────────────────────────────────────
  // Exploits white's tempo debt (undeveloped pieces).
  {
    int w = doc.personality_subverter;
    if(w > 0) {
      // If white is behind in development (positive tempo_debt = white behind)
      if(g.tempo_debt > 0) delta += g.tempo_debt * w * 10;
      // Reward initiative cancellation
      delta += doc.initiative_cancel_bonus * w / 5;
    }
  }

  // ── Doctrine-specific bonuses ─────────────────────────────────────────────
  // Piece trade bonus (simplified endgame)
  if(doc.piece_trade_bonus != 0) {
    // Count pieces traded (fewer pieces = more applicable)
    int totalPieces = 0;
    for(int s=0;s<64;s++) {
      if(board[s] && TYPE(board[s])!=KING && TYPE(board[s])!=PAWN) totalPieces++;
    }
    // Bonus scales: fewer pieces = more relevant (endgame approaching)
    int tradeRelevance = max(0, 14 - totalPieces);
    delta += doc.piece_trade_bonus * tradeRelevance / 5;
  }

  return delta;
}

// ─── Layer 5: Thermodynamic Break Bonus ──────────────────────────────────────
// Reward black for controlling the "thermal release valve" — the break square
// identified by the pawn heat map. Applied inside evaluate().
int thermodynamicBreakBonus(const GambitDoctrine& doc, const int heatMap[8]) {
  int bonus = 0;
  if(doc.center_break_bonus <= 0) return 0;

  // Find the hottest contested file (where white has heat pressure)
  // The "thermal release valve" for black is breaking on that file or adjacent
  int hottestFile = 3;  // default to d-file
  int hottestHeat = -9999;
  for(int f=2;f<=5;f++) {  // only center files
    if(heatMap[f] > hottestHeat) { hottestHeat = heatMap[f]; hottestFile = f; }
  }

  // Check if black has a pawn that can break on the hottest file or adjacent
  for(int df=-1;df<=1;df++) {
    int tf = hottestFile + df;
    if(tf<0||tf>7) continue;
    for(int r=3;r<=5;r++) {
      if(board[sqOf(r,tf)]==B_P) {
        // Pawn in position to create tension on the hot file
        bonus += doc.center_break_bonus;
        break;
      }
    }
  }

  // Extra reward if we've already executed the break (pawn on rank 4 in center)
  for(int f=2;f<=5;f++) {
    if(board[sqOf(3,f)]==B_P) bonus += doc.center_break_bonus / 2;
  }

  return bonus;
}

// ─── Master PIS integration function ─────────────────────────────────────────
// Called from evaluate(). Returns total delta to add to score (black perspective).
int positionalIntelligenceDelta() {
  if(!isOpeningPhase()) return 0;  // only fires in opening/early middlegame

  PositionalGenome genome = computeGenome();
  GambitDoctrine doctrine = detectAndLoadDoctrine();

  int bKsq=-1, wKsq=-1;
  for(int s=0;s<64;s++) {
    if(board[s]==W_K) wKsq=s;
    if(board[s]==B_K) bKsq=s;
  }

  int heatMap[8];
  computePawnHeatMap(heatMap);

  int delta = 0;
  delta += styleReactorDelta(genome, doctrine, bKsq, wKsq);
  delta += thermodynamicBreakBonus(doctrine, heatMap);

  // Clamp to prevent runaway evaluation swings
  if(delta >  250) delta =  250;
  if(delta < -250) delta = -250;

  return delta;
}

// ─── PIS move ordering bonus ─────────────────────────────────────────────────
// Called from moveScore() for black moves in opening. Returns extra score.
int pisMoveBonus(Move& m) {
  if(!isOpeningPhase()) return 0;
  if(!IS_BLACK(board[m.from])) return 0;

  GambitDoctrine doc = detectAndLoadDoctrine();
  if(doc.id == DOCTRINE_NONE) return 0;

  int bonus = 0;
  int toR = rankOf(m.to), toF = fileOf(m.to);
  int piece = board[m.from];

  // Center break bonus: prioritize pawns advancing to break squares
  if(TYPE(piece) == PAWN) {
    bool isCenterFile = (toF >= 2 && toF <= 5);
    bool isBreakRank  = (toR >= 3 && toR <= 4);
    if(isCenterFile && isBreakRank) bonus += doc.center_break_bonus;

    // King's Gambit: d5 counter is the #1 priority response
    if(doc.id == DOCTRINE_ACCEPT_GAMBIT && toF == 3 && toR == 4) bonus += 150;
    // Queen's Gambit: c5 break is the release valve
    if(doc.id == DOCTRINE_STRANGLE && toF == 2 && toR == 4) bonus += 120;
    // Bird's counter: e5 push
    if(doc.id == DOCTRINE_COUNTER_ATK && toF == 4 && toR == 4) bonus += 130;
  }

  // Subverter: moves that exploit white's undeveloped pieces get a boost
  if(doc.personality_subverter >= 5 && TYPE(piece) == KNIGHT) {
    // Knight to outpost squares (e4, d4 area — rank 3 for black = rank index 3)
    if(toR == 3 && toF >= 2 && toF <= 5) bonus += doc.initiative_cancel_bonus;
  }

  // Fortress: if doctrine calls for king safety, boost castling heavily
  if(doc.personality_fortress >= 4 && m.flags == 2) bonus += 200;

  // Accept Gambit doctrine: trade pieces to simplify
  if(doc.id == DOCTRINE_ACCEPT_GAMBIT && board[m.to] != EMPTY) {
    bonus += doc.piece_trade_bonus;
  }

  return bonus;
}

// ═══════════════════════════════════════════════════════════════════════════════
// END MEDUSA POSITIONAL INTELLIGENCE SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Evaluation ───────────────────────────────────────────────────────────────
int evaluate() {
  int score = 0;
  bool endgame = isEndgame();
  bool opening = isOpeningPhase();

  int wKsq = -1, bKsq = -1;
  int wb = 0, bb = 0;

  for(int s=0;s<64;s++) {
    int p = board[s];
    if(!p) continue;
    int val = PIECE_VAL[TYPE(p)] + pstValue(p, s, endgame);
    if(IS_WHITE(p)) {
      score += val;
      if(TYPE(p)==BISHOP) wb++;
      if(TYPE(p)==KING)   wKsq = s;
    } else {
      score -= val;
      if(TYPE(p)==BISHOP) bb++;
      if(TYPE(p)==KING)   bKsq = s;
    }
  }

  // Doubled pawn penalty
  for(int f=0;f<8;f++) {
    int wp=0, bp=0;
    for(int r=0;r<8;r++) {
      int sq=sqOf(r,f);
      if(board[sq]==W_P) wp++;
      if(board[sq]==B_P) bp++;
    }
    if(wp>1) score -= 20*(wp-1);
    if(bp>1) score += 20*(bp-1);
  }

  // Passed pawn bonus (larger in endgame)
  int passedScale = endgame ? 2 : 1;
  for(int s=0;s<64;s++) {
    int p=board[s];
    if(TYPE(p)!=PAWN) continue;
    int r=rankOf(s), f=fileOf(s);
    if(IS_WHITE(p)) {
      bool passed=true;
      for(int rr=r+1;rr<8&&passed;rr++) {
        for(int ff=f-1;ff<=f+1;ff++) {
          if(ff<0||ff>7) continue;
          if(board[sqOf(rr,ff)]==B_P) { passed=false; break; }
        }
      }
      if(passed) score += passedScale * (20 + r*10);
    } else {
      bool passed=true;
      for(int rr=r-1;rr>=0&&passed;rr--) {
        for(int ff=f-1;ff<=f+1;ff++) {
          if(ff<0||ff>7) continue;
          if(board[sqOf(rr,ff)]==W_P) { passed=false; break; }
        }
      }
      if(passed) score -= passedScale * (20 + (7-r)*10);
    }
  }

  // Bishop pair bonus
  if(wb>=2) score += 30;
  if(bb>=2) score -= 30;

  // King activity in endgame: reward king proximity to center
  if(endgame) {
    if(wKsq>=0) {
      int kr=rankOf(wKsq), kf=fileOf(wKsq);
      int centerDist = abs(kr-3) + abs(kr-4) + abs(kf-3) + abs(kf-4);
      score += (8 - centerDist) * 4;
    }
    if(bKsq>=0) {
      int kr=rankOf(bKsq), kf=fileOf(bKsq);
      int centerDist = abs(kr-3) + abs(kr-4) + abs(kf-3) + abs(kf-4);
      score -= (8 - centerDist) * 4;
    }
  }

  if(!endgame) {
    // King safety — pawn shield
    if(wKsq>=0) {
      int kf=fileOf(wKsq);
      for(int df=-1;df<=1;df++) {
        int f2=kf+df; if(f2<0||f2>7) continue;
        bool shielded=false;
        for(int r=rankOf(wKsq)+1;r<8;r++) {
          if(board[sqOf(r,f2)]==W_P) { shielded=true; break; }
        }
        if(!shielded) score -= 10;
      }
    }
    if(bKsq>=0) {
      int kf=fileOf(bKsq);
      for(int df=-1;df<=1;df++) {
        int f2=kf+df; if(f2<0||f2>7) continue;
        bool shielded=false;
        for(int r=rankOf(bKsq)-1;r>=0;r--) {
          if(board[sqOf(r,f2)]==B_P) { shielded=true; break; }
        }
        if(!shielded) score += 10;
      }
    }

    if(opening) {
      // ── Undeveloped minor piece penalty ────────────────────────────────────
      // Penalize side that leaves knights/bishops on starting squares
      if(board[sqOf(7,1)]==B_N) score += 30;
      if(board[sqOf(7,6)]==B_N) score += 30;
      if(board[sqOf(7,2)]==B_B) score += 25;
      if(board[sqOf(7,5)]==B_B) score += 25;
      if(board[sqOf(0,1)]==W_N) score -= 30;
      if(board[sqOf(0,6)]==W_N) score -= 30;
      if(board[sqOf(0,2)]==W_B) score -= 25;
      if(board[sqOf(0,5)]==W_B) score -= 25;

      // ── Castling rights: REWARD still having the option, penalize losing it ─
      // (fixed: was inverted for Black — having rights is GOOD, losing them is BAD)
      if(!(bCastle_K||bCastle_Q)) score += 30;  // Black lost castling rights → good for White
      if(!(wCastle_K||wCastle_Q)) score -= 30;  // White lost castling rights → good for Black
      // Bonus for still having rights (incentivizes preserving the option)
      if(bCastle_K||bCastle_Q) score -= 10;     // Black still has rights → slight bonus for Black
      if(wCastle_K||wCastle_Q) score += 10;     // White still has rights → slight bonus for White

      // ── Early queen development penalty ────────────────────────────────────
      // If queen left starting square while 2+ minor pieces are still home,
      // it's a premature queen sally — penalize it heavily (Issue 3)
      {
        int wMinorsHome = 0;
        if(board[sqOf(0,1)]==W_N) wMinorsHome++;
        if(board[sqOf(0,6)]==W_N) wMinorsHome++;
        if(board[sqOf(0,2)]==W_B) wMinorsHome++;
        if(board[sqOf(0,5)]==W_B) wMinorsHome++;
        // If White queen has left d1 while >=2 minors are still home → penalize
        if(board[sqOf(0,3)]!=W_Q && wMinorsHome >= 2) score -= 40;

        int bMinorsHome2 = 0;
        if(board[sqOf(7,1)]==B_N) bMinorsHome2++;
        if(board[sqOf(7,6)]==B_N) bMinorsHome2++;
        if(board[sqOf(7,2)]==B_B) bMinorsHome2++;
        if(board[sqOf(7,5)]==B_B) bMinorsHome2++;
        // If Black queen has left d8 while >=2 minors are still home → penalize Black
        // (fixed: was rewarding Black for this, now correctly penalizes it — Issue 3)
        if(board[sqOf(7,3)]!=B_Q && bMinorsHome2 >= 2) score += 40;
      }

      // ── Piece-under-pawn-attack tempo penalty ───────────────────────────────
      // A bishop/knight on a square attacked by an enemy pawn loses tempo when
      // kicked — this already exists as the pawn-threat block below, but add
      // an extra opening-specific sting to mirror "c3 hits the Bb4" (Issue 1)
      // (handled more heavily in combined pass below — see pawnthreat += 30 → 40)
    }

    int brPos[2]; int brc=0;
    for(int s=0;s<64&&brc<2;s++) if(board[s]==B_R) brPos[brc++]=s;
    if(brc==2) {
      if(rankOf(brPos[0])==rankOf(brPos[1])) score -= 15;
    }

    for(int s=0;s<64;s++) {
      if(board[s]==B_N && (fileOf(s)==0||fileOf(s)==7)) score += 25;
      if(board[s]==W_N && (fileOf(s)==0||fileOf(s)==7)) score -= 25;
    }
  }

  // Bishop mobility penalty + minor piece pawn threat — one combined pass
  // Pawn-threat penalty is extra-steep in opening to discourage premature
  // piece placements that hand opponent free tempo with a pawn kick (Issue 1)
  {
    const int bDirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    for(int s = 0; s < 64; s++) {
      int p = board[s];
      if(!p) continue;
      int tp = TYPE(p);
      if(tp == BISHOP) {
        int mob = 0;
        int r0 = rankOf(s), f0 = fileOf(s);
        for(auto& d : bDirs) {
          int r = r0+d[0], f = f0+d[1];
          while(inBounds(r,f)) {
            mob++;
            if(board[sqOf(r,f)] != EMPTY) break;
            r += d[0]; f += d[1];
          }
        }
        int mobPenalty = 0;
        if(mob == 0)      mobPenalty = 50;
        else if(mob == 1) mobPenalty = 35;
        else if(mob == 2) mobPenalty = 20;
        else if(mob == 3) mobPenalty = 8;
        if(IS_WHITE(p)) score -= mobPenalty;
        else            score += mobPenalty;
      }
      if(tp == KNIGHT || tp == BISHOP) {
        int r = rankOf(s), f = fileOf(s);
        // Extra sting in opening: being kicked by a pawn loses a full tempo
        int threatPenalty = opening ? 45 : 30;
        if(IS_BLACK(p)) {
          bool pawnthreat = false;
          for(int df = -1; df <= 1; df += 2) {
            int pr = r - 1, pf = f + df;
            if(inBounds(pr, pf) && board[sqOf(pr, pf)] == W_P) { pawnthreat = true; break; }
          }
          if(pawnthreat) score += threatPenalty;
        } else {
          bool pawnthreat = false;
          for(int df = -1; df <= 1; df += 2) {
            int pr = r + 1, pf = f + df;
            if(inBounds(pr, pf) && board[sqOf(pr, pf)] == B_P) { pawnthreat = true; break; }
          }
          if(pawnthreat) score -= threatPenalty;
        }
      }
    }
  }

  // ── Isolated pawn penalty ─────────────────────────────────────────────────
  // A pawn with no friendly pawns on adjacent files is isolated and creates
  // a hole on the square in front of it (Issue 4 — weak squares / holes)
  {
    bool wFile[8] = {}, bFile[8] = {};
    for(int f=0;f<8;f++) for(int r=0;r<8;r++) {
      if(board[sqOf(r,f)]==W_P) wFile[f]=true;
      if(board[sqOf(r,f)]==B_P) bFile[f]=true;
    }
    for(int f=0;f<8;f++) {
      bool wLeft  = (f>0) && wFile[f-1];
      bool wRight = (f<7) && wFile[f+1];
      if(wFile[f] && !wLeft && !wRight) score -= 20;  // White isolated pawn

      bool bLeft  = (f>0) && bFile[f-1];
      bool bRight = (f<7) && bFile[f+1];
      if(bFile[f] && !bLeft && !bRight) score += 20;  // Black isolated pawn
    }
  }

  // ── Backward pawn penalty ─────────────────────────────────────────────────
  // A pawn that can't be supported by other pawns and is behind its neighbors
  // creates a hole on the square in front of it (Issue 4)
  {
    for(int s=0;s<64;s++) {
      int p = board[s];
      if(TYPE(p) != PAWN) continue;
      int r = rankOf(s), f = fileOf(s);
      if(IS_WHITE(p)) {
        // Check if any white pawn on adjacent files is on same or lower rank
        bool supported = false;
        for(int df=-1;df<=1;df+=2) {
          int af = f+df;
          if(af<0||af>7) continue;
          for(int rr=0;rr<=r;rr++) {
            if(board[sqOf(rr,af)]==W_P) { supported=true; break; }
          }
          if(supported) break;
        }
        // Square in front must be attacked by enemy pawn to be truly backward
        bool frontAttacked = false;
        int fr = r+1;
        for(int df=-1;df<=1;df+=2) {
          int af = f+df;
          if(inBounds(fr,af) && board[sqOf(fr,af)]==B_P) { frontAttacked=true; break; }
        }
        if(!supported && frontAttacked) score -= 15;
      } else {
        bool supported = false;
        for(int df=-1;df<=1;df+=2) {
          int af = f+df;
          if(af<0||af>7) continue;
          for(int rr=7;rr>=r;rr--) {
            if(board[sqOf(rr,af)]==B_P) { supported=true; break; }
          }
          if(supported) break;
        }
        bool frontAttacked = false;
        int fr = r-1;
        for(int df=-1;df<=1;df+=2) {
          int af = f+df;
          if(inBounds(fr,af) && board[sqOf(fr,af)]==W_P) { frontAttacked=true; break; }
        }
        if(!supported && frontAttacked) score += 15;
      }
    }
  }

  // Rook on open/semi-open file bonus
  for(int s=0;s<64;s++) {
    int p=board[s];
    if(TYPE(p)!=ROOK) continue;
    int f=fileOf(s);
    bool wPawn=false, bPawn=false;
    for(int r=0;r<8;r++) {
      if(board[sqOf(r,f)]==W_P) wPawn=true;
      if(board[sqOf(r,f)]==B_P) bPawn=true;
    }
    if(IS_WHITE(p)) {
      if(!wPawn && !bPawn) score += 20;       // open file
      else if(!wPawn && bPawn) score += 10;   // semi-open file
    } else {
      if(!bPawn && !wPawn) score -= 20;
      else if(!bPawn && wPawn) score -= 10;
    }
  }

  // ── Medusa Positional Intelligence System delta ───────────────────────────
  // Subtracts from score (evaluate is white-relative; PIS delta is black-relative)
  score -= positionalIntelligenceDelta();

  return score;
}

// ─── Stalemate double-check ───────────────────────────────────────────────────
int countAllLegalMoves(bool white) {
  Move buf[128];
  int count;
  generateMoves(white, buf, count, false);
  int legal = 0;
  for(int i=0;i<count;i++) {
    if(isLegal(buf[i])) legal++;
  }
  return legal;
}

// ─── Draw score ───────────────────────────────────────────────────────────────
int getDrawScore() {
  int wMat = 0, bMat = 0;
  for(int s=0;s<64;s++) {
    int p=board[s]; if(!p) continue;
    int t=TYPE(p);
    if(t==KING) continue;
    if(IS_WHITE(p)) wMat+=PIECE_VAL[t];
    else            bMat+=PIECE_VAL[t];
  }
  int diff  = wMat - bMat;
  bool endg = (wMat+bMat) < 1800;
  if(diff > 400 && endg) {
    int bonus = min(300, diff / 3);
    return bonus;
  } else if(diff > 400) {
    return 50;
  } else if(diff > 100) {
    return -80;
  } else if(diff < -100) {
    return -150;
  } else {
    return -30;
  }
}

// ─── Move ordering ────────────────────────────────────────────────────────────
int moveScore(Move& m) {
  int s = 0;
  uint8_t cap = board[m.to];
  int piece = board[m.from];
  int toR = rankOf(m.to), toF = fileOf(m.to);
  int frR = rankOf(m.from), frF = fileOf(m.from);

  if(cap) {
    int seeNet = seeMoveGain(m.from, m.to, piece, IS_BLACK(piece) ? 2 : 1);
    if(seeNet > 0) {
      s += 7000 + seeNet;
    } else if(seeNet == 0) {
      s += 5000;
    } else {
      s += 1000 + seeNet;
    }
  }

  if(m.promo) {
    s += 3000 + PIECE_VAL[m.promo];
  }

  if(m.flags==1) s += 2000;

  bool opening = isOpeningPhase();
  if(!IS_BLACK(piece) || !opening) return s;

  int undevMinors = 0;
  if(board[sqOf(7,1)]==B_N) undevMinors++;
  if(board[sqOf(7,6)]==B_N) undevMinors++;
  if(board[sqOf(7,2)]==B_B) undevMinors++;
  if(board[sqOf(7,5)]==B_B) undevMinors++;

  if(TYPE(piece)==PAWN) {
    int pawnSEE = seeMoveGain(m.from, m.to, piece, 2);
    bool pawnSafe = (pawnSEE >= 0);

    if(pawnSafe) {
      if((frF==3||frF==4) && frR==6 && toR==4) s += 400;
      if((frF==3||frF==4) && toR==5) s += 250;
      if((frF==3||frF==4) && frR==5 && toR==4) s += 200;
      if((frF==2||frF==5) && (toR==4||toR==5)) s += 120;
      if(toR<=4 && (toF>=2&&toF<=5)) s += 50;
    } else {
      s -= pawnSEE * 4;
    }
    if(frF==0||frF==7) s -= 200;
    for(int r2=0;r2<8;r2++) {
      if(board[sqOf(r2,toF)]==B_P) { s -= 70; break; }
    }
    if(frF>=5 && toR<6) s -= 80;
    if(cap && TYPE(board[m.to]) == PAWN && pawnSafe) s += 180;
  }

  if(TYPE(piece)==QUEEN) {
    if(undevMinors >= 3) s -= 1500;
    else if(undevMinors == 2) s -= 1200;
    else if(undevMinors == 1) s -= 600;
    if(toR>=3 && toR<=5 && toF>=2 && toF<=5) s -= 400;
    if(bCastle_K || bCastle_Q) s -= 300;
    if(cap != EMPTY && undevMinors >= 1) s -= 500;
  }

  if(TYPE(piece)==KING && m.flags==0) s -= 1500;

  if(frR==7 && (TYPE(piece)==KNIGHT||TYPE(piece)==BISHOP)) {
    s += 400;
  }

  if(m.from == lastBlackTo && cap == EMPTY) {
    s -= 600;
  }

  if((bPieceMoved & (1ULL << m.from)) && cap == EMPTY && undevMinors > 0) {
    s -= 500;
  }

  if(TYPE(piece)==KNIGHT) {
    if(toF==2||toF==5) s += 100;
    if(toF==3||toF==4) s += 80;
    if(toF==0||toF==7) s -= 150;
    if(board[sqOf(7,1)]==B_N && board[sqOf(7,6)]==B_N) s += 80;

    for(int df = -1; df <= 1; df += 2) {
      int pr = toR - 1, pf = toF + df;
      if(inBounds(pr, pf) && board[sqOf(pr, pf)] == W_P) { s -= 250; break; }
      int pr2 = toR - 2, pf2 = toF + df;
      if(inBounds(pr2, pf2) && board[sqOf(pr2, pf2)] == W_P) { s -= 80; }
    }

    const int katt[8][2]={{1,2},{1,-2},{-1,2},{-1,-2},{2,1},{2,-1},{-2,1},{-2,-1}};
    for(auto& d : katt) {
      int pr = toR+d[0], pf = toF+d[1];
      if(inBounds(pr,pf)) {
        int psq = sqOf(pr,pf);
        if(board[psq]==B_P && seeCaptures(psq, 1) > 0) {
          s += 400;
          break;
        }
      }
    }
  }

  if(TYPE(piece)==BISHOP) {
    if((toR==6&&toF==1)||(toR==6&&toF==6)) s += 80;
    if(toF==0||toF==7) s -= 120;
    if(board[sqOf(7,1)]==B_N||board[sqOf(7,6)]==B_N) s -= 100;
    if((toF==3||toF==4) && toR==5 && board[sqOf(6,toF)]==B_P) s -= 100;
  }

  if(TYPE(piece)==KNIGHT||TYPE(piece)==BISHOP) {
    int cd = abs(toF-3)+abs(toF-4)+abs(toR-3)+abs(toR-4);
    s += (8 - cd) * 12;
  }

  if(toR > frR && cap == EMPTY) s -= 100;

  if(m.flags==2||m.flags==3) {
    int maxHang = 0;
    for(int sq=0; sq<64; sq++) {
      if(!board[sq] || !IS_BLACK(board[sq]) || TYPE(board[sq])==KING) continue;
      int loss = seeCaptures(sq, 1);
      if(loss > maxHang) maxHang = loss;
    }
    if(maxHang == 0)        s += 600;
    else if(maxHang <= 100) s += 200;
    else                    s -= 800;
  }

  if(TYPE(piece)==ROOK) {
    if(m.to==lastBlackFrom && cap==EMPTY) s -= 350;
    if(undevMinors>=2) s -= 500;
    if(undevMinors==1) s -= 200;
    bool openFile=true;
    for(int r=0;r<8;r++) if(board[sqOf(r,toF)]==B_P){openFile=false;break;}
    if(openFile) s += 80;
    if(toR==1) s += 120;
    if(frR==7) {
      for(int f2=0;f2<8;f2++) if(f2!=toF&&board[sqOf(7,f2)]==B_R){s+=50;break;}
    }
  }
  if((TYPE(piece)==KNIGHT||TYPE(piece)==BISHOP)&&m.to==lastBlackFrom&&cap==EMPTY)
    s -= 220;

  // Medusa PIS move ordering bonus
  s += pisMoveBonus(m);

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
      int nullScore = -alphaBeta(depth-1-R, -beta, -beta+1, !maximizing, startMs);

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
Move getBookMove(int blackMoveNum) {
  Move m = {0,0,0,0};
  int v = chosenOpening % 4;
  bool wE4=(board[sqOf(3,4)]==W_P), wD4=(board[sqOf(3,3)]==W_P);
  bool wC4=(board[sqOf(3,2)]==W_P), wF4=(board[sqOf(3,5)]==W_P);
  bool bE5=(board[sqOf(4,4)]==B_P), bC5=(board[sqOf(4,2)]==B_P);
  bool bD5=(board[sqOf(4,3)]==B_P), bF5=(board[sqOf(4,5)]==B_P);
  if(blackMoveNum==1) {
    if(wE4) {
      if(v<=1){m.from=sqOf(6,4);m.to=sqOf(4,4);}
      else   {m.from=sqOf(6,2);m.to=sqOf(4,2);}
    } else if(wD4) {
      if(v<=2){m.from=sqOf(6,3);m.to=sqOf(4,3);}
      else   {m.from=sqOf(6,5);m.to=sqOf(4,5);}
    } else if(wC4) {
      if(v<=1){m.from=sqOf(6,4);m.to=sqOf(4,4);}
      else   {m.from=sqOf(6,2);m.to=sqOf(4,2);}
    } else if(wF4) {
      m.from=sqOf(6,3);m.to=sqOf(4,3);
    } else {
      if(v<=1){m.from=sqOf(6,4);m.to=sqOf(4,4);}
      else   {m.from=sqOf(6,3);m.to=sqOf(4,3);}
    }
  } else {
    if(wE4) {
      if(bE5)      {m.from=sqOf(7,1);m.to=sqOf(5,2);}
      else if(bC5) {m.from=sqOf(6,3);m.to=sqOf(5,3);}
      else         {m.from=sqOf(7,6);m.to=sqOf(5,5);}
    } else if(wD4) {
      if(bD5)      {m.from=sqOf(6,4);m.to=sqOf(5,4);}
      else if(bF5) {m.from=sqOf(7,6);m.to=sqOf(5,5);}
      else         {m.from=sqOf(7,6);m.to=sqOf(5,5);}
    } else if(wC4) {
      if(bE5||bC5) {m.from=sqOf(7,1);m.to=sqOf(5,2);}
      else         {m.from=sqOf(7,6);m.to=sqOf(5,5);}
    } else if(wF4) {
      m.from=sqOf(7,6);m.to=sqOf(5,5);
    } else {
      if(bE5||bD5)   {m.from=sqOf(7,6);m.to=sqOf(5,5);}
      else if(v%2==0){m.from=sqOf(6,4);m.to=sqOf(4,4);}
      else           {m.from=sqOf(6,3);m.to=sqOf(4,3);}
    }
  }
  if(m.from==0&&m.to==0) return m;
  if(board[m.from]==EMPTY||!IS_BLACK(board[m.from])){m.from=0;m.to=0;return m;}
  Move lm=m; if(!isLegal(lm)){m.from=0;m.to=0;}
  return m;
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
      if(iterBestScore <= alpha - aspDelta && depth >= 3) {
        // Fail low — widen
        alpha = max(-99999, alpha - aspDelta*2);
        aspDelta *= 2;
      } else if(iterBestScore >= beta && depth >= 3) {
        // Fail high — widen
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

// Helper: draw a single centered text line with optional color
void cutCenteredText(int y, const char* txt, uint16_t col, int sz = 1) {
  tft.setTextSize(sz);
  int len = strlen(txt);
  int tw  = len * 6 * sz;
  tft.setTextColor(col);
  tft.setCursor((160 - tw) / 2, y);
  tft.print(txt);
}

// Draw subtle scanline pattern for CRT feel (every 3 rows, very faint)
void drawScanlines(int y0, int y1) {
  // Draw a sparse dark overlay using 1px-high horizontal rects every 3 rows
  for(int y = y0; y < y1; y += 3) {
    tft.drawFastHLine(0, y, 160, 0x0000);
  }
}

// Scrolling name animation for 2 seconds
// names[] = list of strings, nameCount = how many, finalName = name to settle on
// Returns after animation + settling
void scrollNameAnimation(int cx, int cy, int fieldW, int fieldH,
                          const char* names[], int nameCount,
                          const char* finalName, uint16_t finalColor) {
  unsigned long start = millis();
  unsigned long dur   = 2000;

  int idx = 0;
  unsigned long lastScroll = 0;
  int scrollInterval = 90;

  // Field border — dim green frame
  tft.fillRect(cx, cy, fieldW, fieldH, 0x0060);
  tft.drawRect(cx-1, cy-1, fieldW+2, fieldH+2, 0x03A0);

  while(millis() - start < dur) {
    unsigned long now = millis();
    float progress = (float)(now - start) / dur;

    // Slow → fast → settling
    if(progress > 0.75f)      scrollInterval = 50;
    else if(progress > 0.45f) scrollInterval = 70;

    if(now - lastScroll >= (unsigned long)scrollInterval) {
      lastScroll = now;

      // Background flicker: dim green with slight variation
      uint16_t bgFlicker = (idx & 1) ? 0x0060 : 0x0080;
      tft.fillRect(cx, cy, fieldW, fieldH, bgFlicker);

      // Color: starts dim, brightens as it accelerates
      uint16_t scrollColor;
      if(progress < 0.3f)      scrollColor = 0x0300;
      else if(progress < 0.6f) scrollColor = 0x03E0;
      else                     scrollColor = 0x07E0;

      tft.setTextSize(1);
      tft.setTextColor(scrollColor);
      const char* nm = names[idx % nameCount];
      int nlen = strlen(nm);
      int nx = cx + (fieldW - nlen*6)/2;
      tft.setCursor(nx, cy + (fieldH-8)/2);
      tft.print(nm);
      idx++;
    }
    yield();
    delay(5);
  }

  // Settle: flash bright then hold
  for(int flash = 0; flash < 3; flash++) {
    tft.fillRect(cx, cy, fieldW, fieldH, (flash&1) ? 0x0300 : 0x0020);
    tft.setTextSize(1);
    tft.setTextColor((flash&1) ? 0x07E0 : 0x03E0);
    int nlen = strlen(finalName);
    int nx = cx + (fieldW - nlen*6)/2;
    tft.setCursor(nx, cy + (fieldH-8)/2);
    tft.print(finalName);
    delay(60);
  }

  // Final locked display
  tft.fillRect(cx, cy, fieldW, fieldH, 0x0020);
  tft.drawRect(cx-1, cy-1, fieldW+2, fieldH+2, finalColor);
  tft.setTextSize(1);
  tft.setTextColor(finalColor);
  int nlen = strlen(finalName);
  int nx = cx + (fieldW - nlen*6)/2;
  tft.setCursor(nx, cy + (fieldH-8)/2);
  tft.print(finalName);
}

void playIntroCutscene() {
  // ═══════════════════════════════════════════════════════════════
  // SCENE 1: "RICHARD PRESENTS" — Matrix rain feel
  // ═══════════════════════════════════════════════════════════════
  tft.fillScreen(0x0000);

  // Matrix-style sparse pixel field: random green dots of varying brightness
  const uint16_t dotPalette[4] = {0x0060, 0x0180, 0x03E0, 0x07E0};
  for(int i = 0; i < 60; i++) {
    int sx = (esp_random() % 156) + 2;
    int sy = (esp_random() % 120) + 4;
    tft.drawPixel(sx, sy, dotPalette[esp_random() % 4]);
  }

  // Top scan line — bright green → dim
  {
    const uint16_t segs[6] = {0x07E0,0x03E0,0x0300,0x03E0,0x07E0,0x47E4};
    int sw = 160/6;
    for(int i=0;i<6;i++) tft.drawFastHLine(i*sw, 18, (i==5)?160-i*sw:sw, segs[i]);
    tft.drawFastHLine(0, 19, 160, 0x0180);
  }
  // Bottom scan line — dim green
  {
    const uint16_t segs[6] = {0x0300,0x03E0,0x07E0,0x47E4,0x07E0,0x03E0};
    int sw = 160/6;
    for(int i=0;i<6;i++) tft.drawFastHLine(i*sw, 108, (i==5)?160-i*sw:sw, segs[i]);
    tft.drawFastHLine(0, 109, 160, 0x0180);
  }

  // "RICHARD" — spaced, bright green
  cutCenteredText(30, "R I C H A R D", 0x07E0, 1);

  // Thin green rule below name
  tft.drawFastHLine(40, 42, 80, 0x03E0);
  tft.drawFastHLine(40, 43, 80, 0x0180);

  // "PRESENTS" — size 2, letter-by-letter brightness gradient
  {
    const char* pword = "PRESENTS";
    int plen = strlen(pword);
    int px0 = (160 - plen*12)/2;
    // Dim→bright→dim green gradient
    const uint16_t pgrad[8] = {0x0300,0x03E0,0x07E0,0x47E4,0x47E4,0x07E0,0x03E0,0x0300};
    tft.setTextSize(2);
    for(int i = 0; i < plen; i++) {
      tft.setTextColor(pgrad[i]);
      tft.setCursor(px0 + i*12, 50);
      tft.print(pword[i]);
    }

    // Subtle underline ticks
    for(int i = 0; i < plen; i++) {
      tft.drawPixel(px0 + i*12 + 5, 67, pgrad[i]);
    }

    // CRT flicker: briefly erase and redraw PRESENTS
    delay(350);
    tft.fillRect(px0-2, 48, plen*12+4, 22, 0x0000);
    delay(35);
    tft.setTextSize(2);
    for(int i = 0; i < plen; i++) {
      tft.setTextColor(pgrad[i]);
      tft.setCursor(px0 + i*12, 50);
      tft.print(pword[i]);
    }
    delay(50);
    tft.fillRect(px0-2, 48, plen*12+4, 22, 0x0000);
    delay(20);
    tft.setTextSize(2);
    for(int i = 0; i < plen; i++) {
      tft.setTextColor(pgrad[i]);
      tft.setCursor(px0 + i*12, 50);
      tft.print(pword[i]);
    }
  }

  // Scanline overlay for CRT effect
  drawScanlines(20, 108);

  // Blinking prompt at bottom
  tft.setTextSize(1);
  tft.setTextColor(0x03E0);
  cutCenteredText(116, "> press to continue <", 0x03E0, 1);

  while(digitalRead(JOY_SW) == LOW) { yield(); delay(10); }
  while(digitalRead(JOY_SW) != LOW) { yield(); delay(20); }
  while(digitalRead(JOY_SW) == LOW) { yield(); delay(10); }

  // ═══════════════════════════════════════════════════════════════
  // WIPE TRANSITION: green scan line sweep downward
  // ═══════════════════════════════════════════════════════════════
  for(int y = 0; y < 128; y += 2) {
    tft.drawFastHLine(0, y,   160, 0x07E0);  // bright sweep line
    tft.drawFastHLine(0, y+1, 160, 0x07E0);
    delay(4);
    tft.drawFastHLine(0, y,   160, 0x0000);
    tft.drawFastHLine(0, y+1, 160, 0x0000);
  }

  // ═══════════════════════════════════════════════════════════════
  // SCENE 2: "RageMate" title + engine/version slot machine
  // ═══════════════════════════════════════════════════════════════
  tft.fillScreen(0x0000);

  // Header band: dark green with green shimmer top edge
  tft.fillRect(0, 0, 160, 36, 0x0060);
  tft.drawFastHLine(0, 0, 160, 0x07E0);
  tft.drawFastHLine(0, 1, 160, 0x03E0);
  tft.drawFastHLine(0, 35, 160, 0x03A0);

  // Chess-square micro-pattern inside header band (very subtle)
  for(int gx = 0; gx < 160; gx += 4) {
    for(int gy = 3; gy < 35; gy += 4) {
      if(((gx/4 + gy/4) & 1) == 0)
        tft.fillRect(gx, gy, 4, 4, 0x0080);
    }
  }

  // "RageMate" — bold, green shimmer gradient
  {
    tft.setTextSize(2);
    const char* title = "RageMate";
    int tlen = strlen(title);
    int tx0 = (160 - tlen*12)/2;
    // Shimmer: dim→bright→pale
    const uint16_t tgrad[8] = {0x03E0,0x05E0,0x03A0,0x05E0,0x03E0,0x05E0,0x03A0,0x03E0};
    for(int i = 0; i < tlen; i++) {
      tft.setTextColor(tgrad[i % 8]);
      tft.setCursor(tx0 + i*12, 8);
      tft.print(title[i]);
    }
    // Underline: green dash per letter
    for(int i = 0; i < tlen; i++) {
      tft.drawFastHLine(tx0 + i*12, 26, 10, tgrad[i % 8]);
    }
  }

  // Separator below header
  tft.fillRect(0, 36, 160, 1, 0x03E0);

  // Background below header: near-black with very faint dot matrix
  for(int i = 0; i < 30; i++) {
    int dx = (esp_random() % 156) + 2;
    int dy = (esp_random() % 84) + 40;
    tft.drawPixel(dx, dy, 0x0080);
  }

  // ── Engine name slot ──────────────────────────────────────────
  tft.setTextSize(1);
  tft.setTextColor(0x03A0);
  tft.setCursor(8, 47);
  tft.print("ENGINE");

  // Small ">" arrow in bright green
  tft.setTextColor(0x07E0);
  tft.setCursor(48, 47);
  tft.print(":");

  const char* engineNames[] = {
    "Enigma","Chimera","Hydra","Mamba","Katana",
    "Vendetta","Mecha","Ignita","Anima","Persona","Medusa","Valkyria"
  };
  int engineCount = 12;

  scrollNameAnimation(55, 43, 97, 14, engineNames, engineCount, "Medusa", 0x07E0);

  // ── Version slot ──────────────────────────────────────────────
  tft.setTextSize(1);
  tft.setTextColor(0x03A0);
  tft.setCursor(8, 65);
  tft.print("VERSION");

  tft.setTextColor(0x07E0);
  tft.setCursor(48, 65);
  tft.print(":");

  const char* versionNames[] = {
    "Alpha","Beta","Gamma","Delta","Zeta","Theta","Sigma","Omega"
  };
  int versionCount = 8;

  scrollNameAnimation(55, 61, 97, 14, versionNames, versionCount, "Beta", 0x47E4);

  // ── Final locked display ──────────────────────────────────────
  delay(150);

  // Outer glow: draw extra rect around both slots
  tft.drawRect(53, 41, 101, 36, 0x03E0);
  tft.drawRect(52, 40, 103, 38, 0x0180);

  // Separator before bottom summary band
  tft.fillRect(0, 82, 160, 1, 0x03E0);
  tft.fillRect(0, 83, 160, 28, 0x0020);

  // "Medusa | Beta" centered in summary band
  tft.setTextSize(1);
  tft.setTextColor(0x07E0);
  tft.setCursor(36, 90);
  tft.print("Medusa");
  tft.setTextColor(0x03A0);
  tft.setCursor(80, 90);
  tft.print("|");
  tft.setTextColor(0x47E4);
  tft.setCursor(88, 90);
  tft.print("Beta");

  // Thin rule
  tft.drawFastHLine(10, 101, 140, 0x03A0);

  // Bottom prompt
  tft.setTextColor(0x07E0);
  tft.setCursor(28, 108);
  tft.print("> press to begin <");

  // Bottom edge accent
  tft.drawFastHLine(0, 127, 160, 0x03E0);

  while(digitalRead(JOY_SW) == LOW) { yield(); delay(10); }
  while(digitalRead(JOY_SW) != LOW) { yield(); delay(20); }
  while(digitalRead(JOY_SW) == LOW) { yield(); delay(10); }

  // Wipe transition: vertical green flash columns
  for(int y = 0; y < 128; y += 2) {
    tft.drawFastHLine(0, y,   160, 0x0000);
    tft.drawFastHLine(0, y+1, 160, 0x0000);
    delay(3);
  }
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
          openingMoveIndex = min(openingMoveIndex+1, 2);

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