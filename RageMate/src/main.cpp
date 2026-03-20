/*
 * RageMate Maxima V2
 * Changes from Maxima V1:
 *   - Joystick sensitivity button (+) added to left panel (panelFocus 4)
 *   - Virtual slider UI: 1–100%, default 75%; analog response toggle checkbox
 *   - Analog response mode: cursor speed scales with joystick deflection distance
 *   - Cursor redesigned: double-outline (outer bright, inner sqBg gap) on ALL
 *     squares, not just squares with pieces — always visible on every theme
 *   - Panel layout tightened to fit new button; phase indicator preserved
 *   - All Maxima V1 changes retained
 */

#include <Arduino.h>
#include <SPI.h>
#include <math.h>
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

// ─── RGB565 helper ──────────────────────────────────────────────────────────
#define C(r,g,b) ((uint16_t)(((uint16_t)(r)<<11)|((uint16_t)(g)<<5)|(uint16_t)(b)))

// ─── Theme struct ────────────────────────────────────────────────────────────
// Each theme supplies every colour used by every screen so that a single call
// to refreshTheme() updates ALL active colour variables at once.
struct Theme {
  const char* name;
  // Board
  uint16_t bg, darkSq, lightSq, border, cursor, label;
  uint16_t wFill, wTxt, bFill, bTxt;
  // Selection / legal dots / last-move
  uint16_t sel, dot, lastMove, cursorLastMove;
  // Buttons / panel
  uint16_t btnBg, btnHl, pbEmpty, pbBorder;
  // Game-over text colour
  uint16_t gameOver;
  // Lose / resign / checkmate screens
  uint16_t lBg, lGrid, lAccent, lAccent2, lTitle, lSub, lCrown, lCrownHi;
  // Keypad
  uint16_t kBg, kHdr, kHdrTxt, kKey, kKeyBdr, kKeyTxt, kSelKey, kSelBdr, kConfirm, kDigBdr, kDigTxt;
  // Intro
  uint16_t iBg, iBox, iBoxBdr, iTitle, iTitle2, iSub, iPrompt;
  // Panel (sidebar)
  uint16_t panelBg;   // full-height strip behind buttons
  uint16_t btnFace;   // individual button face colour
  uint16_t sideAccent;// accent/text colour inside panel (phase letter, swatch highlight)
};

// ─── 38-theme table (PROGMEM) ────────────────────────────────────────────────
// Field order: name | bg darkSq lightSq border cursor label |
//              wFill wTxt bFill bTxt | sel dot lastMove cursorLastMove |
//              btnBg btnHl pbEmpty pbBorder | gameOver |
//              lBg lGrid lAccent lAccent2 lTitle lSub lCrown lCrownHi |
//              kBg kHdr kHdrTxt kKey kKeyBdr kKeyTxt kSelKey kSelBdr kConfirm kDigBdr kDigTxt |
//              iBg iBox iBoxBdr iTitle iTitle2 iSub iPrompt |
//              panelBg btnFace sideAccent
const Theme themes[] PROGMEM = {

// 00 WOOD
{ "WOOD",
  C( 5, 6, 1),C(12,16, 3),C(27,44,14),C(20,26, 5),C(31,46, 0),C(22,34, 8),
  C(29,50,18),C( 4, 5, 1),C( 8,10, 2),C(26,42,13),
  C(31,54,10),C(24,36, 9),C(18,24, 5),C(27,40,10),
  C(12,16, 3),C(20,26, 5),C( 8,12, 2),C(18,24, 5),
  C(31,46, 0),
  C( 5, 6, 1),C( 8,10, 2),C(20,26, 5),C(12,16, 3),C(28,44,14),C(22,36, 9),C(16,22, 4),C(28,48,16),
  C( 5, 6, 1),C(12,16, 3),C(31,46, 0),C( 8,10, 2),C(16,22, 4),C(24,38,12),C(20,26, 5),C(28,40,12),C(16,22, 4),C(20,28, 6),C(28,44,14),
  C( 5, 6, 1),C(10,14, 3),C(18,24, 5),C(31,46, 0),C(28,44,14),C(20,30, 7),C(24,38,12),
  C( 8,10, 2),C(16,20, 4),C(31,46, 0) },

// 01 CERAMIC
{ "CERAMIC",
  C(22,42,26),C(10,22,24),C(27,54,30),C( 8,20,22),C( 0,30,28),C(10,24,26),
  C(29,58,31),C( 4,10,12),C( 6,14,18),C(28,56,30),
  C( 0,40,30),C( 0,28,26),C( 8,18,22),C( 0,36,28),
  C( 8,18,22),C(12,26,28),C( 6,14,18),C(10,22,24),
  C( 0,30,28),
  C(22,42,26),C( 6,14,18),C( 8,20,22),C(10,22,24),C(27,54,30),C( 0,30,28),C( 8,18,22),C(28,56,30),
  C(22,42,26),C( 8,18,22),C( 0,30,28),C(10,22,24),C(14,30,30),C(20,44,28),C( 8,18,22),C( 0,30,28),C(10,20,22),C(14,28,28),C(22,46,28),
  C(22,42,26),C(10,22,24),C( 8,18,22),C( 0,30,28),C(20,44,28),C(14,30,28),C( 0,24,26),
  C(10,20,24),C(12,22,26),C( 0,30,28) },

// 02 STONE
{ "STONE",
  C( 6, 8, 7),C(12,14,12),C(24,30,18),C(16,18,14),C(28,36, 4),C(18,22,14),
  C(26,34,20),C( 3, 4, 3),C( 8,10, 8),C(24,30,18),
  C(28,40, 6),C(22,28,12),C(14,16,12),C(26,32,14),
  C(12,14,12),C(16,20,14),C( 8,10, 8),C(14,18,12),
  C(28,36, 4),
  C( 6, 8, 7),C( 8,10, 8),C(14,16,12),C(12,14,12),C(24,30,18),C(18,22,12),C(12,14,12),C(26,32,18),
  C( 6, 8, 7),C(12,14,12),C(28,36, 4),C(10,12,10),C(16,20,14),C(22,28,16),C(14,18,12),C(20,26,14),C(12,14,12),C(18,22,14),C(24,30,18),
  C( 6, 8, 7),C(14,18,12),C(10,12,10),C(28,36, 4),C(24,30,18),C(16,20,12),C(20,26,14),
  C( 8,10, 9),C(14,16,13),C(28,36, 4) },

// 03 MEDIEVAL
{ "MEDIEVAL",
  C( 6, 7, 2),C(11,13, 4),C(25,40,12),C(22,24, 4),C(30,38, 0),C(20,28, 6),
  C(28,46,14),C( 4, 5, 1),C( 7, 9, 2),C(26,40,12),
  C(30,44, 4),C(22,32, 8),C(16,20, 4),C(28,40,10),
  C(11,13, 4),C(18,22, 5),C( 7, 9, 2),C(14,18, 4),
  C(30,38, 0),
  C( 6, 7, 2),C( 7, 9, 2),C(18,20, 4),C(11,13, 4),C(25,40,12),C(22,30, 6),C(11,14, 3),C(28,44,13),
  C( 6, 7, 2),C(11,13, 4),C(30,38, 0),C( 9,11, 3),C(16,20, 4),C(24,36,10),C(14,18, 4),C(22,28, 6),C( 9,11, 3),C(16,22, 5),C(25,40,12),
  C( 6, 7, 2),C(12,16, 4),C( 9,11, 3),C(30,38, 0),C(25,40,12),C(18,26, 6),C(22,34,10),
  C( 8, 9, 3),C(14,16, 4),C(30,38, 0) },

// 04 BAR
{ "BAR",
  C( 3, 5, 1),C( 4,14, 2),C( 8,28, 6),C(16,20, 4),C(31,62,31),C(10,26, 6),
  C(26,44,10),C( 1, 2, 0),C( 2, 8, 1),C(22,40, 8),
  C(20,50,12),C(10,28, 6),C( 8,18, 4),C(16,36, 8),
  C( 4,14, 2),C( 8,20, 5),C( 2, 8, 1),C( 6,16, 4),
  C(31,62,31),
  C( 3, 5, 1),C( 2, 8, 1),C( 8,18, 4),C( 4,14, 2),C( 8,28, 6),C(31,62,31),C( 4,14, 2),C(20,36, 8),
  C( 3, 5, 1),C( 4,14, 2),C(31,62,31),C( 4,12, 2),C( 8,20, 5),C(20,36, 8),C( 8,20, 5),C(16,30, 7),C( 4,12, 2),C( 8,22, 5),C(16,40, 9),
  C( 3, 5, 1),C( 6,18, 4),C( 4,12, 2),C(31,62,31),C(16,40, 9),C(10,28, 6),C(20,40,10),
  C( 3, 8, 1),C( 6,16, 3),C(31,62,31) },

// 05 CLUB
{ "CLUB",
  C( 5, 2, 2),C(14, 4, 5),C(26,36,14),C(24,22, 2),C(28,26, 2),C(22,20, 4),
  C(28,42,16),C( 4, 2, 1),C( 8, 2, 3),C(26,36,14),
  C(30,30, 4),C(22,22, 4),C(16, 8, 4),C(26,26, 4),
  C(14, 4, 5),C(18,14, 3),C( 8, 2, 3),C(12, 6, 4),
  C(28,26, 2),
  C( 5, 2, 2),C( 8, 2, 3),C(18,10, 4),C(14, 4, 5),C(26,36,14),C(28,26, 2),C(10, 3, 4),C(28,30,10),
  C( 5, 2, 2),C(14, 4, 5),C(28,26, 2),C(10, 3, 4),C(18,14, 3),C(24,32,12),C(16, 8, 4),C(22,18, 4),C(10, 3, 4),C(16,10, 4),C(24,32,12),
  C( 5, 2, 2),C(14,10, 4),C(10, 3, 4),C(28,26, 2),C(24,32,12),C(18,18, 4),C(22,22, 4),
  C( 8, 3, 3),C(16, 6, 5),C(28,26, 2) },

// 06 DISCO
{ "DISCO",
  C( 4, 2, 8),C(10, 4,18),C(24,24,28),C(28, 8,22),C(31,10,26),C(22, 8,20),
  C(28,28,30),C( 4, 2, 8),C( 6, 2,14),C(28,10,24),
  C(31,16,28),C(24,10,22),C(18, 6,20),C(28,14,26),
  C(10, 4,18),C(18, 8,22),C( 6, 2,14),C(12, 4,18),
  C(31,10,26),
  C( 4, 2, 8),C( 6, 2,14),C(22, 6,20),C(10, 4,18),C(24,24,28),C(31,10,26),C( 8, 3,16),C(28,20,28),
  C( 4, 2, 8),C(10, 4,18),C(31,10,26),C( 8, 3,16),C(18, 8,22),C(24,20,26),C(16, 6,22),C(22,10,24),C( 8, 3,16),C(16, 8,22),C(24,24,28),
  C( 4, 2, 8),C(12, 6,20),C( 8, 3,16),C(31,10,26),C(24,24,28),C(18,10,22),C(20,12,24),
  C( 6, 3,12),C(12, 5,20),C(31,10,26) },

// 07 CYBERPUNK
{ "CYBERPUNK",
  C( 1, 0, 2),C( 6, 2,10),C(12, 4,18),C( 0,44,28),C( 0,48,28),C( 0,38,22),
  C(28, 4,22),C(31,62,31),C( 2,16,16),C(31, 8,28),
  C( 0,52,30),C( 0,40,26),C( 6, 2,10),C( 0,46,28),
  C( 6, 2,10),C(18, 4,20),C( 4, 2, 8),C( 8, 2,12),
  C( 0,48,28),
  C( 1, 0, 2),C( 4, 2, 8),C(14, 2,14),C( 6, 2,10),C(12, 4,18),C( 0,48,28),C( 4, 2, 8),C(31, 8,28),
  C( 1, 0, 2),C( 6, 2,10),C( 0,48,28),C( 6, 2,10),C(18, 4,20),C(26, 6,26),C(14, 4,16),C(20, 4,22),C( 6, 2,10),C(12, 4,18),C(31, 8,28),
  C( 1, 0, 2),C( 8, 2,12),C( 4, 2,10),C( 0,48,28),C(31, 8,28),C(18, 4,20),C( 0,40,26),
  C( 3, 1, 5),C( 8, 3,12),C( 0,48,28) },

// 08 DARK MODE
{ "DARK MODE",
  C( 1, 2, 3),C( 4, 8,10),C( 9,18,20),C( 8,14,18),C( 0,44,31),C(10,20,24),
  C(14,24,28),C(31,62,31),C( 3, 6, 8),C(10,22,28),
  C( 0,48,31),C( 0,36,28),C( 6,12,16),C( 0,42,30),
  C( 4, 8,10),C(10,20,24),C( 3, 6, 8),C( 6,12,16),
  C( 0,44,31),
  C( 1, 2, 3),C( 3, 6, 8),C(10,16,20),C( 4, 8,10),C( 9,18,20),C( 0,44,31),C( 3, 6, 8),C(14,28,31),
  C( 1, 2, 4),C( 5,10,14),C( 0,44,31),C( 4, 8,12),C(10,20,24),C(14,28,31),C( 8,18,22),C(12,24,26),C( 4, 8,12),C(10,24,28),C(14,38,31),
  C( 1, 2, 4),C( 6,14,20),C( 3, 8,12),C( 0,44,31),C(14,38,31),C(10,26,28),C( 0,32,26),
  C( 2, 4, 6),C( 6,10,13),C( 0,44,31) },

// 09 AUTUMN
{ "AUTUMN",
  C( 6, 4, 1),C(18, 8, 1),C(29,40, 2),C(24,18, 2),C(31,52, 0),C(26,28, 2),
  C(30,46, 4),C( 4, 2, 0),C(12, 5, 1),C(29,42, 3),
  C(31,56, 2),C(26,30, 2),C(20,12, 2),C(28,42, 2),
  C(18, 8, 1),C(22,16, 2),C(10, 4, 1),C(16,10, 2),
  C(31,52, 0),
  C( 6, 4, 1),C(12, 5, 1),C(20,12, 2),C(18, 8, 1),C(29,40, 2),C(31,52, 0),C(14, 6, 1),C(28,38, 3),
  C( 6, 4, 1),C(18, 8, 1),C(31,52, 0),C(14, 6, 1),C(22,16, 2),C(28,38, 3),C(18,10, 1),C(24,22, 2),C(14, 6, 1),C(20,16, 2),C(28,42, 3),
  C( 6, 4, 1),C(16,10, 2),C(12, 6, 1),C(31,52, 0),C(28,42, 3),C(22,26, 2),C(26,36, 3),
  C(10, 5, 1),C(20,10, 2),C(31,52, 0) },

// 10 SPRING
{ "SPRING",
  C(22,28,14),C( 6,18, 6),C(30,24,24),C( 8,24, 8),C(22,14,14),C( 8,22, 8),
  C(31,28,26),C( 3, 8, 3),C( 4,12, 4),C(30,22,22),
  C(28,18,16),C(14,22,12),C( 6,18, 6),C(24,18,16),
  C( 6,18, 6),C(10,22,10),C( 4,12, 4),C( 6,16, 6),
  C(22,14,14),
  C(22,28,14),C( 4,12, 4),C( 8,20, 8),C( 6,18, 6),C(30,24,24),C(22,14,14),C( 6,16, 6),C(20,32,18),
  C(22,28,14),C( 6,18, 6),C(22,14,14),C( 6,16, 6),C(10,22,10),C(20,32,18),C(10,22,10),C(16,28,14),C( 6,16, 6),C(10,24,12),C(22,34,18),
  C(22,28,14),C(10,22,10),C( 6,16, 6),C(22,14,14),C(28,24,24),C(14,26,14),C(16,28,14),
  C(12,20,10),C( 8,20, 8),C(22,14,14) },

// 11 TOURNAMENT
{ "TOURNAMENT",
  C( 0, 2, 8),C( 2, 6,18),C(28,46,20),C(28,44, 0),C(31,52, 0),C(24,40, 0),
  C(30,50,22),C( 0, 3,10),C( 1, 4,14),C(28,46, 0),
  C(31,56, 2),C(24,42, 0),C(16,24, 0),C(28,48, 0),
  C( 2, 6,18),C(18,28, 0),C( 1, 4,14),C( 4,10,16),
  C(31,52, 0),
  C( 0, 2, 8),C( 1, 4,14),C(14,20, 0),C( 2, 6,18),C(28,46,20),C(31,52, 0),C( 2, 6,16),C(26,42, 0),
  C( 0, 2, 8),C( 2, 6,18),C(31,52, 0),C( 2, 6,16),C(18,28, 0),C(26,42, 0),C( 4,12,24),C(12,22, 0),C( 2, 6,16),C(16,26, 0),C(28,46,20),
  C( 0, 2, 8),C( 4,10,16),C( 2, 4,16),C(31,52, 0),C(26,42, 0),C(18,30, 0),C(22,36, 0),
  C( 1, 4,12),C( 3, 8,20),C(31,52, 0) },

// 12 ROYAL
{ "ROYAL",
  C( 4, 1, 8),C( 9, 3,18),C(22,28, 8),C(28,44, 0),C(31,50, 0),C(22,34, 0),
  C(26,36,10),C( 3, 1, 6),C( 5, 2,12),C(26,34, 6),
  C(31,54, 2),C(24,36, 0),C(14, 6,22),C(28,46, 0),
  C( 9, 3,18),C(18,24, 0),C( 5, 2,12),C( 8, 4,16),
  C(31,50, 0),
  C( 4, 1, 8),C( 5, 2,12),C(16, 8,22),C( 9, 3,18),C(22,28, 8),C(31,50, 0),C( 7, 3,16),C(24,34, 6),
  C( 4, 1, 8),C( 9, 3,18),C(31,50, 0),C( 7, 2,14),C(18,24, 0),C(24,34, 6),C(12,10,22),C(20,18,26),C( 7, 2,14),C(12,10,22),C(22,28, 8),
  C( 4, 1, 8),C(10, 6,18),C( 7, 2,14),C(31,50, 0),C(24,34, 6),C(18,22, 0),C(20,26, 2),
  C( 6, 2,12),C(11, 4,20),C(31,50, 0) },

// 13 MARBLE
{ "MARBLE",
  C(20,24,22),C( 2, 2, 2),C(30,60,30),C(16,20,18),C(22,28,20),C(14,18,16),
  C(31,62,31),C( 2, 2, 2),C( 2, 2, 2),C(30,60,30),
  C(28,32,24),C(18,22,18),C( 8, 8, 8),C(24,28,22),
  C( 6, 6, 6),C(16,20,18),C( 2, 2, 2),C( 8, 8, 8),
  C(22,28,20),
  C(20,24,22),C( 2, 2, 2),C( 8, 8, 8),C( 6, 6, 6),C(30,60,30),C(22,28,20),C( 6, 6, 6),C(26,52,26),
  C(20,24,22),C( 6, 6, 6),C(22,28,20),C( 8, 8, 8),C(16,20,18),C(26,52,26),C(12,14,12),C(18,20,16),C( 8, 8, 8),C(14,16,14),C(28,56,28),
  C(20,24,22),C(14,18,14),C( 8, 8, 8),C(22,28,20),C(28,56,28),C(16,20,16),C(18,24,18),
  C(10,12,11),C( 8, 8, 8),C(22,28,20) },

// 14 NEON
{ "NEON",
  C( 0, 0, 1),C( 8, 0,14),C(18, 2,26),C(31,10,24),C( 0,56,28),C(24, 8,22),
  C(22, 2,28),C(31,62,31),C( 2,18,20),C(31,10,24),
  C( 0,60,30),C( 0,44,24),C(12, 2,18),C( 0,52,28),
  C( 8, 0,14),C(18, 4,22),C( 4, 0, 8),C( 6, 0,12),
  C( 0,56,28),
  C( 0, 0, 1),C( 4, 0, 8),C(14, 2,20),C( 8, 0,14),C(18, 2,26),C( 0,56,28),C( 6, 0,12),C(31,10,24),
  C( 0, 0, 1),C( 8, 0,14),C( 0,56,28),C( 6, 0,12),C(18, 4,22),C(24, 6,26),C(14, 2,22),C(20, 4,24),C( 6, 0,12),C(14, 2,22),C(18, 2,26),
  C( 0, 0, 1),C( 8, 2,14),C( 6, 0,12),C( 0,56,28),C(31,10,24),C(16, 4,22),C( 0,44,26),
  C( 3, 0, 6),C(10, 1,16),C( 0,56,28) },

// 15 LAVA
{ "LAVA",
  C( 2, 1, 0),C( 8, 3, 0),C(22,12, 0),C(28,18, 0),C(31,24, 0),C(24,16, 0),
  C(26,16, 0),C(31,62,20),C( 5, 2, 0),C(28,22, 0),
  C(31,28, 0),C(24,18, 0),C(14, 6, 0),C(28,22, 0),
  C( 8, 3, 0),C(18,10, 0),C( 5, 2, 0),C( 8, 4, 0),
  C(31,24, 0),
  C( 2, 1, 0),C( 5, 2, 0),C(14, 6, 0),C( 8, 3, 0),C(22,12, 0),C(31,24, 0),C( 6, 3, 0),C(26,18, 0),
  C( 2, 1, 0),C( 8, 3, 0),C(31,24, 0),C( 6, 3, 0),C(18,10, 0),C(26,18, 0),C(14, 8, 0),C(20,12, 0),C( 6, 3, 0),C(12, 6, 0),C(22,12, 0),
  C( 2, 1, 0),C( 8, 4, 0),C( 6, 3, 0),C(31,24, 0),C(26,18, 0),C(18,10, 0),C(22,14, 0),
  C( 4, 2, 0),C(10, 4, 0),C(31,24, 0) },

// 16 OCEAN
{ "OCEAN",
  C( 0, 4, 8),C( 0, 8,16),C( 4,30,28),C( 0,32,26),C( 0,50,30),C( 2,26,24),
  C( 6,36,30),C( 0, 3, 6),C( 0, 6,14),C( 6,34,28),
  C( 0,54,31),C( 0,40,28),C( 0,14,20),C( 0,48,30),
  C( 0, 8,16),C( 0,24,22),C( 0, 6,14),C( 0,12,18),
  C( 0,50,30),
  C( 0, 4, 8),C( 0, 6,14),C( 0,14,22),C( 0, 8,16),C( 4,30,28),C( 0,50,30),C( 0, 8,14),C( 4,32,28),
  C( 0, 4, 8),C( 0, 8,16),C( 0,50,30),C( 0, 8,14),C( 0,24,22),C( 4,32,28),C( 0,16,20),C( 0,28,26),C( 0, 8,14),C( 0,14,20),C( 4,30,28),
  C( 0, 4, 8),C( 0,12,18),C( 0, 8,14),C( 0,50,30),C( 4,32,28),C( 0,28,26),C( 0,22,22),
  C( 0, 5,11),C( 0,10,18),C( 0,50,30) },

// 17 FOREST
{ "FOREST",
  C( 1, 4, 1),C( 2, 8, 2),C(14,30, 6),C( 8,22, 4),C(28,38, 2),C( 8,20, 4),
  C(18,34, 8),C( 0, 2, 0),C( 1, 6, 1),C(16,32, 6),
  C(30,42, 4),C(20,28, 4),C( 4,12, 2),C(26,36, 4),
  C( 2, 8, 2),C( 6,16, 3),C( 1, 6, 1),C( 2, 6, 2),
  C(28,38, 2),
  C( 1, 4, 1),C( 1, 6, 1),C( 4,12, 2),C( 2, 8, 2),C(14,30, 6),C(28,38, 2),C( 2, 6, 1),C(14,28, 6),
  C( 1, 4, 1),C( 2, 8, 2),C(28,38, 2),C( 2, 6, 1),C( 6,16, 3),C(14,28, 6),C( 6,16, 3),C(10,22, 4),C( 2, 6, 1),C( 4,12, 2),C(14,30, 6),
  C( 1, 4, 1),C( 4,10, 2),C( 2, 6, 1),C(28,38, 2),C(14,30, 6),C( 8,20, 4),C(12,24, 5),
  C( 1, 5, 1),C( 3,10, 2),C(28,38, 2) },

// 18 DESERT
{ "DESERT",
  C( 8,10, 2),C(18,20, 4),C(28,40,10),C(26,24, 4),C(31,36, 0),C(24,26, 4),
  C(30,46,12),C( 4, 5, 1),C(12,14, 2),C(28,42,10),
  C(31,40, 2),C(24,28, 6),C(18,18, 3),C(28,34, 4),
  C(18,20, 4),C(22,24, 4),C(12,14, 2),C(16,18, 3),
  C(31,36, 0),
  C( 8,10, 2),C(12,14, 2),C(18,18, 3),C(18,20, 4),C(28,40,10),C(31,36, 0),C(14,16, 3),C(26,38, 8),
  C( 8,10, 2),C(18,20, 4),C(31,36, 0),C(14,16, 3),C(22,24, 4),C(26,38, 8),C(18,20, 4),C(22,26, 5),C(14,16, 3),C(18,20, 4),C(28,40,10),
  C( 8,10, 2),C(16,18, 3),C(14,16, 3),C(31,36, 0),C(26,38, 8),C(20,24, 4),C(22,28, 5),
  C(11,13, 3),C(20,22, 4),C(31,36, 0) },

// 19 MIDNIGHT
{ "MIDNIGHT",
  C( 0, 1, 6),C( 2, 4,14),C( 6,10,22),C( 8,18,28),C(28,56,31),C( 8,18,26),
  C(12,22,30),C(28,56,31),C( 1, 3,10),C(16,34,31),
  C(28,60,31),C(20,40,30),C( 4, 8,18),C(24,52,31),
  C( 2, 4,14),C( 6,14,24),C( 1, 3,10),C( 4, 8,16),
  C(28,56,31),
  C( 0, 1, 6),C( 1, 3,10),C( 6, 8,18),C( 2, 4,14),C( 6,10,22),C(28,56,31),C( 2, 6,16),C(14,30,31),
  C( 0, 1, 6),C( 2, 4,14),C(28,56,31),C( 2, 4,12),C( 6,14,24),C(14,30,31),C( 6,14,24),C(10,22,28),C( 2, 4,12),C( 4,10,20),C( 6,10,22),
  C( 0, 1, 6),C( 4, 8,16),C( 2, 4,12),C(28,56,31),C(14,30,31),C(10,20,28),C(12,24,30),
  C( 1, 2, 9),C( 3, 5,16),C(28,56,31) },

// 20 ROSE
{ "ROSE",
  C( 8, 4, 5),C(18, 8,10),C(28,42,24),C(24,14,14),C(28,16,16),C(22,12,12),
  C(30,48,26),C( 6, 3, 4),C(12, 5, 7),C(28,40,22),
  C(30,20,18),C(22,16,14),C(16, 8,10),C(26,18,16),
  C(18, 8,10),C(20,12,12),C(12, 5, 7),C(14, 6, 8),
  C(28,16,16),
  C( 8, 4, 5),C(12, 5, 7),C(16, 8,10),C(18, 8,10),C(28,42,24),C(28,16,16),C(14, 6, 8),C(26,38,20),
  C( 8, 4, 5),C(18, 8,10),C(28,16,16),C(14, 6, 8),C(20,12,12),C(26,38,20),C(18,12,12),C(22,14,14),C(14, 6, 8),C(18,12,12),C(26,38,20),
  C( 8, 4, 5),C(14, 8,10),C(12, 5, 7),C(28,16,16),C(26,38,20),C(18,14,12),C(20,16,14),
  C(11, 5, 6),C(20,10,11),C(28,16,16) },

// 21 STEAMPUNK
{ "STEAMPUNK",
  C( 4, 4, 2),C(10,10, 3),C(22,18, 4),C(24,20, 2),C(28,56,28),C(20,18, 4),
  C(26,22, 4),C( 2, 2, 1),C( 7, 7, 2),C(24,20, 4),
  C(28,58,28),C(20,20, 4),C(12,10, 3),C(24,22, 4),
  C(10,10, 3),C(16,14, 3),C( 7, 7, 2),C(10, 8, 3),
  C(28,56,28),
  C( 4, 4, 2),C( 7, 7, 2),C(12,10, 3),C(10,10, 3),C(22,18, 4),C(28,56,28),C( 8, 8, 2),C(22,18, 4),
  C( 4, 4, 2),C(10,10, 3),C(28,56,28),C( 8, 8, 2),C(16,14, 3),C(22,18, 4),C(14,12, 3),C(18,16, 4),C( 8, 8, 2),C(12,10, 3),C(22,18, 4),
  C( 4, 4, 2),C(12,10, 3),C( 8, 8, 2),C(28,56,28),C(22,18, 4),C(16,14, 3),C(18,16, 4),
  C( 6, 6, 2),C(12,12, 3),C(28,56,28) },

// 22 VAMPIRE
{ "VAMPIRE",
  C( 2, 0, 0),C( 6, 0, 0),C(26,22,20),C(22, 2, 2),C(28, 4, 4),C(20, 2, 2),
  C(28,26,24),C( 4, 0, 0),C( 4, 0, 0),C(26,22,20),
  C(30, 8, 6),C(22,18,16),C( 8, 1, 1),C(26, 6, 4),
  C( 6, 0, 0),C(14, 2, 2),C( 4, 0, 0),C( 6, 0, 0),
  C(28, 4, 4),
  C( 2, 0, 0),C( 4, 0, 0),C( 8, 1, 1),C( 6, 0, 0),C(26,22,20),C(28, 4, 4),C( 4, 0, 0),C(24,20,18),
  C( 2, 0, 0),C( 6, 0, 0),C(28, 4, 4),C( 4, 0, 0),C(14, 2, 2),C(24, 4, 4),C(10, 1, 1),C(16, 2, 2),C( 4, 0, 0),C( 8, 1, 1),C(22,20,18),
  C( 2, 0, 0),C( 8, 2, 2),C( 4, 0, 0),C(28, 4, 4),C(22,20,18),C(16, 2, 2),C(18, 4, 4),
  C( 3, 0, 0),C( 8, 1, 1),C(28, 4, 4) },

// 23 ARCADE
{ "ARCADE",
  C( 0, 0, 0),C( 2, 6, 0),C( 6,20, 0),C(12,40, 0),C(16,52, 0),C( 8,32, 0),
  C( 8,28, 0),C(31,62,31),C( 1, 4, 0),C(10,36, 0),
  C(16,56, 0),C(12,40, 0),C( 4,14, 0),C(14,48, 0),
  C( 2, 6, 0),C( 6,22, 0),C( 1, 4, 0),C( 2, 8, 0),
  C(16,52, 0),
  C( 0, 0, 0),C( 1, 4, 0),C( 4,14, 0),C( 2, 6, 0),C( 6,20, 0),C(16,52, 0),C( 2, 6, 0),C(10,36, 0),
  C( 0, 0, 0),C( 2, 6, 0),C(16,52, 0),C( 2, 6, 0),C( 6,22, 0),C(10,36, 0),C( 6,22, 0),C(10,32, 0),C( 2, 6, 0),C( 4,14, 0),C( 6,20, 0),
  C( 0, 0, 0),C( 4, 8, 0),C( 2, 6, 0),C(16,52, 0),C(10,36, 0),C( 6,22, 0),C( 8,28, 0),
  C( 1, 3, 0),C( 3, 8, 0),C(16,52, 0) },

// 24 GRAFFITI
{ "GRAFFITI",
  C( 6, 6, 6),C(10,10,10),C(20,20,18),C(28,22, 0),C( 0,28,30),C(24,18, 0),
  C(22,22,20),C( 2, 2, 2),C( 8, 8, 8),C(28,22, 0),
  C( 0,34,30),C( 0,26,28),C(10,10,10),C( 0,30,28),
  C(10,10,10),C(16,16,14),C( 8, 8, 8),C(10,10,10),
  C( 0,28,30),
  C( 6, 6, 6),C( 8, 8, 8),C(12,12,12),C(10,10,10),C(20,20,18),C( 0,28,30),C( 8, 8, 8),C(22,22,20),
  C( 6, 6, 6),C(10,10,10),C( 0,28,30),C( 8, 8, 8),C(16,16,14),C(22,22,20),C(12,12,12),C(16,16,14),C( 8, 8, 8),C(10,10,10),C(20,20,18),
  C( 6, 6, 6),C(12,12,12),C( 8, 8, 8),C( 0,28,30),C(20,20,18),C(14,14,14),C(16,16,14),
  C( 7, 7, 7),C(12,12,12),C( 0,28,30) },

// 25 NORDIC
{ "NORDIC",
  C( 2, 6,12),C( 4,10,18),C(24,46,50),C( 8,20,28),C(28,54,58),C( 8,22,30),
  C(26,50,54),C( 2, 6,10),C( 3, 8,16),C(22,44,48),
  C(28,56,58),C(22,44,48),C( 4,12,20),C(26,52,54),
  C( 4,10,18),C( 8,18,26),C( 3, 8,16),C( 4,10,18),
  C(28,54,58),
  C( 2, 6,12),C( 3, 8,14),C( 6,12,22),C( 4,10,18),C(24,46,50),C(28,54,58),C( 4,10,18),C(22,44,48),
  C( 2, 6,12),C( 4,10,18),C(28,54,58),C( 4,10,18),C( 8,18,26),C(22,44,48),C( 8,18,26),C(14,26,34),C( 4,10,18),C( 6,14,22),C(24,46,50),
  C( 2, 6,12),C( 6,12,20),C( 4,10,18),C(28,54,58),C(22,44,48),C(12,24,32),C(14,26,34),
  C( 3, 7,14),C( 5,12,20),C(28,54,58) },

// 26 SAHARA
{ "SAHARA",
  C( 7, 6, 2),C(16,14, 2),C(28,34, 4),C(26,22, 2),C(31,46, 0),C(24,22, 2),
  C(30,38, 6),C( 4, 3, 0),C(10, 9, 1),C(28,34, 4),
  C(31,50, 2),C(24,24, 2),C(18,16, 2),C(28,34, 4),
  C(16,14, 2),C(20,18, 2),C(10, 9, 1),C(14,12, 2),
  C(31,46, 0),
  C( 7, 6, 2),C(10, 9, 1),C(16,14, 2),C(16,14, 2),C(28,34, 4),C(31,46, 0),C(12,10, 2),C(26,30, 4),
  C( 7, 6, 2),C(16,14, 2),C(31,46, 0),C(12,10, 2),C(20,18, 2),C(26,30, 4),C(16,14, 2),C(22,20, 2),C(12,10, 2),C(16,14, 2),C(28,34, 4),
  C( 7, 6, 2),C(14,12, 2),C(12,10, 2),C(31,46, 0),C(26,30, 4),C(20,20, 2),C(22,22, 2),
  C(10, 9, 2),C(18,16, 3),C(31,46, 0) },

// 27 VOID
{ "VOID",
  C( 0, 0, 0),C( 2, 2, 2),C(28,28,28),C(14,14,14),C(31,62,31),C(14,14,14),
  C(30,30,30),C( 0, 0, 0),C( 2, 2, 2),C(28,28,28),
  C(28,32,28),C(18,18,18),C( 6, 6, 6),C(24,24,24),
  C( 4, 4, 4),C(12,12,12),C( 2, 2, 2),C( 4, 4, 4),
  C(31,62,31),
  C( 0, 0, 0),C( 2, 2, 2),C( 6, 6, 6),C( 4, 4, 4),C(28,28,28),C(31,62,31),C( 4, 4, 4),C(24,24,24),
  C( 0, 0, 0),C( 4, 4, 4),C(31,62,31),C( 4, 4, 4),C(12,12,12),C(24,24,24),C(10,10,10),C(14,14,14),C( 4, 4, 4),C( 6, 6, 6),C(24,24,24),
  C( 0, 0, 0),C( 6, 6, 6),C( 4, 4, 4),C(31,62,31),C(24,24,24),C(14,14,14),C(16,16,16),
  C( 2, 2, 2),C( 5, 5, 5),C(31,62,31) },

// 28 CORAL REEF
{ "CORAL REEF",
  C( 2,10,10),C( 4,16,16),C(28,36,22),C( 0,32,28),C(28,18, 6),C( 0,28,24),
  C(30,40,24),C( 2, 6, 6),C( 2,10,12),C(28,36,22),
  C(30,22, 8),C(22,30,18),C( 4,16,16),C(26,20, 8),
  C( 4,16,16),C( 0,24,22),C( 2,10,12),C( 4,14,14),
  C(28,18, 6),
  C( 2,10,10),C( 2,10,12),C( 4,16,16),C( 4,16,16),C(28,36,22),C(28,18, 6),C( 4,14,14),C(26,34,20),
  C( 2,10,10),C( 4,16,16),C(28,18, 6),C( 4,14,14),C( 0,24,22),C(26,34,20),C( 0,20,20),C( 0,26,24),C( 4,14,14),C( 0,18,18),C(28,36,22),
  C( 2,10,10),C( 4,12,14),C( 3,12,12),C(28,18, 6),C(26,34,20),C( 0,24,22),C( 4,22,20),
  C( 3,12,12),C( 5,18,18),C(28,18, 6) },

// 29 NOIR
{ "NOIR",
  C( 3, 4, 2),C( 8, 9, 6),C(22,24,18),C(16,18,13),C(28,30,22),C(14,16,11),
  C(24,26,20),C( 2, 2, 1),C( 5, 6, 4),C(22,24,18),
  C(28,32,22),C(20,22,16),C( 8, 9, 6),C(24,26,18),
  C( 8, 9, 6),C(12,14,10),C( 5, 6, 4),C( 8, 8, 6),
  C(28,30,22),
  C( 3, 4, 2),C( 5, 6, 4),C( 8, 9, 6),C( 8, 9, 6),C(22,24,18),C(28,30,22),C( 6, 7, 5),C(20,22,16),
  C( 3, 4, 2),C( 8, 9, 6),C(28,30,22),C( 6, 7, 5),C(12,14,10),C(22,24,18),C(10,12, 8),C(14,16,12),C( 6, 7, 5),C( 8, 9, 6),C(22,24,18),
  C( 3, 4, 2),C(10,12, 8),C( 6, 7, 5),C(28,30,22),C(22,24,18),C(14,16,12),C(16,18,13),
  C( 5, 6, 3),C(10,11, 7),C(28,30,22) },

// 30 EMBER
{ "EMBER",
  C( 3, 2, 1),C( 8, 6, 4),C(20,12, 4),C(24,14, 4),C(31,20, 4),C(20,12, 3),
  C(24,16, 4),C(31,62,31),C( 6, 4, 2),C(22,14, 4),
  C(31,24, 4),C(22,14, 4),C( 8, 6, 4),C(28,18, 4),
  C( 8, 6, 4),C(14,10, 4),C( 6, 4, 2),C( 8, 6, 4),
  C(31,20, 4),
  C( 3, 2, 1),C( 6, 4, 2),C( 8, 6, 4),C( 8, 6, 4),C(20,12, 4),C(31,20, 4),C( 6, 4, 2),C(20,14, 4),
  C( 3, 2, 1),C( 8, 6, 4),C(31,20, 4),C( 6, 4, 2),C(14,10, 4),C(22,16, 4),C(12, 8, 3),C(16,10, 4),C( 6, 4, 2),C( 8, 6, 4),C(20,12, 4),
  C( 3, 2, 1),C( 8, 6, 4),C( 6, 4, 2),C(31,20, 4),C(20,14, 4),C(14,10, 4),C(16,12, 4),
  C( 5, 4, 2),C(10, 8, 4),C(31,20, 4) },

// 31 NEON JUNGLE
{ "NEON JUNGLE",
  C( 0, 2, 0),C( 2,10, 2),C( 6,26, 4),C(24, 2,20),C(28, 4,24),C(18, 2,16),
  C(10,34, 6),C( 0, 1, 0),C( 1, 6, 1),C(24, 2,20),
  C(28, 6,24),C(18, 4,18),C( 2,10, 2),C(24, 4,22),
  C( 2,10, 2),C(12, 2,10),C( 1, 6, 1),C( 2, 8, 2),
  C(28, 4,24),
  C( 0, 2, 0),C( 1, 6, 1),C( 2, 8, 2),C( 2,10, 2),C( 6,26, 4),C(28, 4,24),C( 2, 8, 2),C(10,30, 6),
  C( 0, 2, 0),C( 2,10, 2),C(28, 4,24),C( 2, 8, 2),C(12, 2,10),C(18, 2,16),C( 8,20, 6),C(12, 2,12),C( 2, 8, 2),C( 4,14, 4),C( 6,26, 4),
  C( 0, 2, 0),C( 4,10, 4),C( 2, 8, 2),C(28, 4,24),C(10,30, 6),C( 8,22, 4),C(10,26, 6),
  C( 1, 5, 1),C( 3,12, 3),C(28, 4,24) },

// 32 BRONZE
{ "BRONZE",
  C( 5, 6, 2),C(12,14, 3),C(24,28, 8),C(22,20, 4),C( 4,24,16),C(18,18, 4),
  C(26,30, 9),C( 3, 3, 1),C( 8,10, 2),C(24,28, 8),
  C( 6,28,18),C(18,20, 4),C(12,14, 3),C( 4,26,16),
  C(12,14, 3),C(16,16, 4),C( 8,10, 2),C(10,12, 3),
  C( 4,24,16),
  C( 5, 6, 2),C( 8,10, 2),C(12,12, 3),C(12,14, 3),C(24,28, 8),C( 4,24,16),C(10,12, 2),C(22,26, 7),
  C( 5, 6, 2),C(12,14, 3),C( 4,24,16),C(10,12, 2),C(16,16, 4),C(24,26, 8),C(12,14, 3),C(16,18, 4),C(10,12, 2),C(12,12, 3),C(24,28, 8),
  C( 5, 6, 2),C(12,12, 3),C(10,12, 2),C( 4,24,16),C(22,26, 7),C(14,14, 3),C(16,16, 4),
  C( 7, 8, 2),C(14,16, 4),C( 4,24,16) },

// 33 CIRCUIT
{ "CIRCUIT",
  C( 0, 5, 2),C( 1,10, 4),C( 3,20, 7),C( 8,34,10),C(22,18, 4),C( 5,22, 7),
  C( 5,24, 8),C(28,56,28),C( 0, 7, 3),C( 6,28, 9),
  C(24,20, 6),C(14,28, 8),C( 2,12, 4),C(20,18, 5),
  C( 1,10, 4),C( 4,18, 6),C( 0, 7, 3),C( 2,10, 4),
  C(22,18, 4),
  C( 0, 5, 2),C( 0, 7, 3),C( 2,12, 4),C( 1,10, 4),C( 3,20, 7),C(22,18, 4),C( 1, 8, 3),C( 6,26, 8),
  C( 0, 5, 2),C( 1,10, 4),C(22,18, 4),C( 1, 8, 3),C( 4,18, 6),C( 8,32, 9),C( 4,18, 6),C( 6,22, 7),C( 1, 8, 3),C( 2,12, 4),C( 3,20, 7),
  C( 0, 5, 2),C( 2,10, 4),C( 1, 8, 3),C(22,18, 4),C( 8,32, 9),C( 4,18, 6),C( 6,24, 8),
  C( 0, 7, 3),C( 2,12, 5),C(22,18, 4) },

// 34 GOLD
{ "GOLD",
  C( 2, 2, 0),C( 6, 6, 0),C(26,28, 0),C(22,20, 0),C(31,46, 0),C(20,20, 0),
  C(28,30, 2),C( 0, 0, 0),C( 4, 4, 0),C(26,28, 0),
  C(31,50, 2),C(22,22, 0),C(10,10, 0),C(28,30, 0),
  C( 6, 6, 0),C(16,16, 0),C( 4, 4, 0),C( 6, 6, 0),
  C(31,46, 0),
  C( 2, 2, 0),C( 4, 4, 0),C( 8, 8, 0),C( 6, 6, 0),C(26,28, 0),C(31,46, 0),C( 6, 6, 0),C(24,26, 0),
  C( 2, 2, 0),C( 6, 6, 0),C(31,46, 0),C( 6, 6, 0),C(16,16, 0),C(24,26, 0),C(12,12, 0),C(18,18, 0),C( 6, 6, 0),C(12,12, 0),C(26,28, 0),
  C( 2, 2, 0),C( 8, 8, 0),C( 6, 6, 0),C(31,46, 0),C(24,26, 0),C(16,16, 0),C(18,18, 0),
  C( 3, 3, 0),C( 8, 8, 0),C(31,46, 0) },

// 35 THUNDER
{ "THUNDER",
  C( 3, 3, 3),C( 8, 8, 8),C(28,32, 0),C(20,24, 0),C(31,62,31),C(18,22, 0),
  C(30,34, 2),C( 1, 1, 0),C( 5, 5, 5),C(26,30, 0),
  C(31,62,20),C(22,26, 0),C( 8, 8, 8),C(28,34, 0),
  C( 8, 8, 8),C(14,16, 0),C( 5, 5, 5),C( 8, 8, 6),
  C(31,62,31),
  C( 3, 3, 3),C( 5, 5, 5),C( 8, 8, 6),C( 8, 8, 8),C(28,32, 0),C(31,62,31),C( 6, 6, 6),C(24,28, 0),
  C( 3, 3, 3),C( 8, 8, 8),C(31,62,31),C( 6, 6, 6),C(14,16, 0),C(24,28, 0),C(12,14, 0),C(18,20, 0),C( 6, 6, 6),C(10,10, 0),C(28,32, 0),
  C( 3, 3, 3),C(10,10, 6),C( 6, 6, 6),C(31,62,31),C(24,28, 0),C(16,18, 0),C(18,22, 0),
  C( 5, 5, 5),C(10,10,10),C(31,62,31) },

// 36 NATURE
{ "NATURE",
  C( 4, 6, 1),C(14,12, 3),C(10,28, 4),C(12,20, 3),C(22,40, 4),C(10,22, 4),
  C(16,32, 6),C( 2, 3, 0),C( 8, 8, 2),C(14,28, 5),
  C(24,44, 6),C(14,26, 4),C(10,10, 2),C(20,36, 5),
  C(14,12, 3),C(14,20, 4),C( 8, 8, 2),C(10,10, 2),
  C(22,40, 4),
  C( 4, 6, 1),C( 8, 8, 2),C(10,10, 2),C(14,12, 3),C(10,28, 4),C(22,40, 4),C(10,10, 2),C(16,30, 6),
  C( 4, 6, 1),C(14,12, 3),C(22,40, 4),C(10,10, 2),C(14,20, 4),C(18,34, 6),C(12,18, 4),C(14,22, 4),C(10,10, 2),C(12,14, 3),C(10,28, 4),
  C( 4, 6, 1),C(10,12, 3),C(10,10, 2),C(22,40, 4),C(16,30, 6),C(12,20, 4),C(14,22, 4),
  C( 7, 8, 2),C(16,14, 4),C(22,40, 4) },

// 37 HACKER
// Dark terminal aesthetic: black background, phosphor green accents.
// Serious and readable — not the bright lime of ARCADE, but a cooler,
// dimmer green that reads like a real CRT monitor in a dim room.
// bg=near-black | darkSq=very dark green | lightSq=dim terminal green
// border/cursor/label = mid-green | pieces: white fill / black text, dark fill / green text
{ "HACKER",
  C( 0, 1, 0),C( 0, 5, 0),C( 2,12, 1),C( 4,22, 2),C( 6,32, 3),C( 3,18, 1),
  C( 4,20, 2),C(31,62,31),C( 0, 3, 0),C( 4,26, 2),
  C( 6,36, 3),C( 4,28, 2),C( 1, 8, 0),C( 5,30, 2),
  C( 0, 5, 0),C( 2,16, 1),C( 0, 3, 0),C( 0, 6, 0),
  C( 6,32, 3),
  C( 0, 1, 0),C( 0, 3, 0),C( 1, 8, 0),C( 0, 5, 0),C( 2,12, 1),C( 6,32, 3),C( 0, 4, 0),C( 4,22, 2),
  C( 0, 1, 0),C( 0, 5, 0),C( 6,32, 3),C( 0, 4, 0),C( 2,16, 1),C( 4,26, 2),C( 2,14, 1),C( 4,20, 2),C( 0, 4, 0),C( 1,10, 0),C( 4,22, 2),
  C( 0, 1, 0),C( 1, 8, 0),C( 0, 4, 0),C( 6,32, 3),C( 4,26, 2),C( 2,16, 1),C( 3,20, 1),
  C( 0, 2, 0),C( 1, 6, 0),C( 6,32, 3) },

}; // end themes[]

#define NUM_THEMES 38

// ─── Active colour variables (refreshed by refreshTheme) ─────────────────────
// These replace the old hardcoded #define constants.  All existing draw code
// reads these variables so only refreshTheme() needs to change when the theme does.
uint8_t  themeIndex = 0;

// Board / general
uint16_t C_BG, C_DARK_SQ, C_LIGHT_SQ, C_BORDER, C_HOVER, C_SEL, C_BOTH;
uint16_t C_DOT, C_LASTMOVE, C_CURSOR_LASTMOVE, C_GAMEOVER;
uint16_t C_BTN_BG, C_BTN_HL, C_PB_EMPTY, C_PB_BORDER;
uint16_t C_W_FILL, C_W_TXT, C_B_FILL, C_B_TXT, C_LABEL;
// Lose / resign / checkmate screens
uint16_t C_L_BG, C_L_GRID, C_L_ACCENT, C_L_ACCENT2;
uint16_t C_L_TITLE, C_L_SUB, C_L_CROWN, C_L_CROWN_HI;
// Keypad
uint16_t C_K_BG, C_K_HDR, C_K_HDR_TXT, C_K_KEY, C_K_KEY_BDR;
uint16_t C_K_KEY_TXT, C_K_SEL, C_K_SEL_BDR, C_K_CONFIRM, C_K_DIG_BDR, C_K_DIG_TXT;
// Intro
uint16_t C_I_BG, C_I_BOX, C_I_BOX_BDR, C_I_TITLE, C_I_TITLE2, C_I_SUB, C_I_PROMPT;
// Panel (sidebar)
uint16_t C_PANEL_BG;    // full-height strip behind buttons
uint16_t C_BTN_FACE;    // individual button face colour
uint16_t C_SIDE_ACCENT; // accent text colour inside panel

// Helper: read one uint16_t field from a PROGMEM Theme struct.
// 'offset' is the field index (0-based, counting only uint16_t fields after the name pointer).
static inline uint16_t themeField(uint8_t ti, uint8_t offset) {
  // Each Theme: 4-byte name ptr + offset*2 bytes into uint16_t fields.
  const uint8_t* base = (const uint8_t*)&themes[ti];
  return pgm_read_word(base + 4 + offset * 2);
}

// Read the name pointer from PROGMEM Theme.
static inline const char* themeName(uint8_t ti) {
  return (const char*)pgm_read_dword(&themes[ti].name);
}

void refreshTheme() {
  uint8_t ti = themeIndex;
  uint8_t i  = 0;
  // Board
  C_BG              = themeField(ti, i++); // bg
  C_DARK_SQ         = themeField(ti, i++); // darkSq
  C_LIGHT_SQ        = themeField(ti, i++); // lightSq
  C_BORDER          = themeField(ti, i++); // border
  C_HOVER           = themeField(ti, i++); // cursor
  C_LABEL           = themeField(ti, i++); // label
  C_W_FILL          = themeField(ti, i++); // wFill
  C_W_TXT           = themeField(ti, i++); // wTxt
  C_B_FILL          = themeField(ti, i++); // bFill
  C_B_TXT           = themeField(ti, i++); // bTxt
  // Selection / dots / last-move
  C_SEL             = themeField(ti, i++); // sel
  C_DOT             = themeField(ti, i++); // dot
  C_LASTMOVE        = themeField(ti, i++); // lastMove
  C_CURSOR_LASTMOVE = themeField(ti, i++); // cursorLastMove
  // Buttons / panel
  C_BTN_BG          = themeField(ti, i++); // btnBg
  C_BTN_HL          = themeField(ti, i++); // btnHl
  C_PB_EMPTY        = themeField(ti, i++); // pbEmpty
  C_PB_BORDER       = themeField(ti, i++); // pbBorder
  // Game-over text
  C_GAMEOVER        = themeField(ti, i++); // gameOver
  // Lose screen
  C_L_BG            = themeField(ti, i++); // lBg
  C_L_GRID          = themeField(ti, i++); // lGrid
  C_L_ACCENT        = themeField(ti, i++); // lAccent
  C_L_ACCENT2       = themeField(ti, i++); // lAccent2
  C_L_TITLE         = themeField(ti, i++); // lTitle
  C_L_SUB           = themeField(ti, i++); // lSub
  C_L_CROWN         = themeField(ti, i++); // lCrown
  C_L_CROWN_HI      = themeField(ti, i++); // lCrownHi
  // Keypad
  C_K_BG            = themeField(ti, i++); // kBg
  C_K_HDR           = themeField(ti, i++); // kHdr
  C_K_HDR_TXT       = themeField(ti, i++); // kHdrTxt
  C_K_KEY           = themeField(ti, i++); // kKey
  C_K_KEY_BDR       = themeField(ti, i++); // kKeyBdr
  C_K_KEY_TXT       = themeField(ti, i++); // kKeyTxt
  C_K_SEL           = themeField(ti, i++); // kSelKey
  C_K_SEL_BDR       = themeField(ti, i++); // kSelBdr
  C_K_CONFIRM       = themeField(ti, i++); // kConfirm
  C_K_DIG_BDR       = themeField(ti, i++); // kDigBdr
  C_K_DIG_TXT       = themeField(ti, i++); // kDigTxt
  // Intro
  C_I_BG            = themeField(ti, i++); // iBg
  C_I_BOX           = themeField(ti, i++); // iBox
  C_I_BOX_BDR       = themeField(ti, i++); // iBoxBdr
  C_I_TITLE         = themeField(ti, i++); // iTitle
  C_I_TITLE2        = themeField(ti, i++); // iTitle2
  C_I_SUB           = themeField(ti, i++); // iSub
  C_I_PROMPT        = themeField(ti, i++); // iPrompt
  // Panel
  C_PANEL_BG        = themeField(ti, i++); // panelBg
  C_BTN_FACE        = themeField(ti, i++); // btnFace
  C_SIDE_ACCENT     = themeField(ti, i++); // sideAccent
  // Derived aliases (keep old names working in draw code)
  C_BOTH            = 0xFFFF;             // sel+hover overlap always white
}

// Progress bar — left column, fills from bottom up
#define PB_X   130
#define PB_Y    12
#define PB_IW   12
#define PB_IH   96
// Evaluation bar — right column, same height as progress bar
#define EB_X   145
#define EB_Y    12
#define EB_IW   12
#define EB_IH   96

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

// MAX_HISTORY: must cover the longest realistic game.
// A chess game can last 200+ half-moves in theory; 300 comfortably covers
// any practical game while staying within ESP32 SRAM limits.
// HistoryEntry is ~20 bytes, so 300 entries = ~6 KB.
#define MAX_HISTORY 300
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

// ─── UI state ───────────────────────────────────────────────────────────────
int  cursorCol = 4;
int  cursorRow = 4;
int  selSq = -1;
bool gameOver = false;
char gameOverMsg[20] = "";
bool engineThinking = false;
bool checkmatePending = false;  // true when player is checkmated, waiting for 2s hold
uint8_t themeMenuSel = 0;       // remembered scroll position in theme menu

// ─── FreeRTOS search task ──────────────────────────────────────────────────
volatile bool     searchRunning  = false;
volatile bool     searchDone     = false;
volatile int      searchPct      = 0;
volatile int      searchDepth    = 0;   // current iterative-deepening depth, shown above progress bar
volatile int      evalScore      = 0;   // last completed search score (centipawns, white-relative) for eval bar
Move              searchResult   = {0,0,0,0};
SemaphoreHandle_t searchSem      = nullptr;

// Legal moves cache for selected piece
Move legalCache[64];
int  legalCount = 0;

int engineFrom = -1, engineTo = -1;
int  panelFocus = 0;
int  fullMoveCount = 0;

unsigned long lastJoyMove = 0;
#define JOY_DELAY 180

// ─── Joystick sensitivity & analog response ───────────────────────────────────
// joySensitivity: 1–100. Default 75.
//   100  = 2× the base delay  (fastest)
//   75   = base delay (JOY_DELAY = 180 ms)
//   1    = 4× the base delay  (slowest)
// Formula: effectiveDelay = JOY_DELAY * (100 - joySensitivity + 25) / 100
//   at 75:  180 * 100/100 = 180 ms  (neutral)
//   at 100: 180 *  25/100 =  45 ms  (2× faster — half the delay)
//   at 1:   180 * 124/100 = ~720 ms (~4× slower)
// analogResponse: when true, cursor repeat rate also scales with how far
//   the joystick is pushed from centre (0–2047 from centre on each axis).
//   The raw axis reading is 0–4095; centre ≈ 2048.
//   Deflection fraction = (distance from centre) / 2048, clamped 0–1.
//   Effective delay = baseDelay * (1 - 0.6 * deflection).
//   At full deflection the delay drops an additional 60% vs the base.
int  joySensitivity  = 75;    // 1–100, default 75%
bool analogResponse  = false; // analog speed scaling enabled

// Compute effective joystick delay from current sensitivity + raw axis readings.
// rawX, rawY: latest analogRead values (only used if analogResponse is on).
static inline unsigned long joyEffectiveDelay(int rawX, int rawY) {
  // Base delay from sensitivity (linear mapping)
  // sens=75 → factor=100/100=1.0 → delay=180
  // sens=100→ factor=25/100=0.25 → delay=45
  // sens=1  → factor=124/100=1.24→ delay=223... capped so ×4 max = 720
  int factor = (100 - joySensitivity) + 25;   // 25 at 100%, 100 at 75%, 124 at 1%
  unsigned long base = (unsigned long)JOY_DELAY * factor / 100;
  if(base < 20)  base = 20;   // hard floor — never faster than 20 ms
  if(base > 720) base = 720;  // hard ceiling — never slower than 720 ms

  if(!analogResponse) return base;

  // Deflection from centre (2048 midpoint on 12-bit ADC)
  int dx = abs(rawX - 2048);
  int dy = abs(rawY - 2048);
  int deflect = max(dx, dy);  // use dominant axis
  if(deflect > 2048) deflect = 2048;
  // Scale: at full deflection reduce delay by 60%
  unsigned long scaled = base - (base * 6UL * deflect) / (10UL * 2048);
  if(scaled < 20) scaled = 20;
  return scaled;
}

// ─── Piece-square tables ─────────────────────────────────────────────────────
// Written from WHITE's perspective, rank 0 = white's back rank (a1..h1).
// Black pieces are automatically flipped by pstValue().
// Values are BONUSES on top of base piece value — keep magnitudes modest
// so PST never overrides a 1-pawn material difference (~100cp).
//
// PST_PAWN: rank 0 (back rank) and rank 7 (promotion) are always 0 — pawns
// can't exist there. Rank 1 (just pushed off back rank) gets a tiny bonus.
// Central files get increasing bonuses toward the 4th/5th rank.
const int8_t PST_PAWN[64] PROGMEM = {
   0,  0,  0,  0,  0,  0,  0,  0,   // rank 0 — impossible, always 0
  -5, -3,  0,  0,  0,  0, -3, -5,   // rank 1 — just left home, slight rim penalty
  -4, -2,  4, 12, 12,  4, -2, -4,   // rank 2 — modest bonus for central files
   0,  0,  8, 22, 22,  8,  0,  0,   // rank 3 — good central advance
   6,  6, 14, 28, 28, 14,  6,  6,   // rank 4 — strong central control
  18, 20, 24, 30, 30, 24, 20, 18,   // rank 5 — advanced, very strong
  40, 44, 48, 52, 52, 48, 44, 40,   // rank 6 — one step from promotion
   0,  0,  0,  0,  0,  0,  0,  0    // rank 7 — impossible, always 0
};

// PST_KNIGHT: strong central preference, heavy rim/corner penalty.
const int8_t PST_KNIGHT[64] PROGMEM = {
  -50,-40,-30,-30,-30,-30,-40,-50,   // rank 0 — back rank, terrible
  -40,-20,  0,  2,  2,  0,-20,-40,   // rank 1
  -30,  2, 10, 14, 14, 10,  2,-30,   // rank 2
  -20,  6, 14, 18, 18, 14,  6,-20,   // rank 3 — outpost territory
  -20,  4, 14, 18, 18, 14,  4,-20,   // rank 4
  -30,  2, 10, 14, 14, 10,  2,-30,   // rank 5
  -40,-20,  0,  2,  2,  0,-20,-40,   // rank 6
  -50,-40,-30,-20,-20,-30,-40,-50    // rank 7 — back rank
};

// PST_BISHOP: long diagonal activity, moderate central bonus.
// Notably NOT rewarding rank 5+ to avoid early sorties.
const int8_t PST_BISHOP[64] PROGMEM = {
  -20,-10,-10,-10,-10,-10,-10,-20,   // rank 0 — back rank
  -10,  4,  0,  0,  0,  0,  4,-10,   // rank 1 — fianchetto squares bonus
  -10,  6,  8, 10, 10,  8,  6,-10,   // rank 2
  -10,  2, 10, 12, 12, 10,  2,-10,   // rank 3
  -10,  2, 10, 12, 12, 10,  2,-10,   // rank 4 — symmetric, no forward bias
  -10,  4,  6,  8,  8,  6,  4,-10,   // rank 5 — deliberately lower than rank 3/4
  -10,  4,  0,  0,  0,  0,  4,-10,   // rank 6
  -20,-10,-10,-10,-10,-10,-10,-20    // rank 7
};

// PST_ROOK: reward 7th rank (rank 6 from white = rank 1 in PST after flip for black),
// penalise back rank slightly. No bonus for rank 5/6 to stop early rook sorties.
const int8_t PST_ROOK[64] PROGMEM = {
   0,  0,  0,  2,  2,  0,  0,  0,   // rank 0 — back rank, neutral
  -4,  0,  0,  0,  0,  0,  0, -4,   // rank 1
  -4,  0,  0,  0,  0,  0,  0, -4,   // rank 2
  -4,  0,  0,  0,  0,  0,  0, -4,   // rank 3
  -4,  0,  0,  0,  0,  0,  0, -4,   // rank 4
  -4,  0,  0,  0,  0,  0,  0, -4,   // rank 5
   6, 10, 10, 10, 10, 10, 10,  6,   // rank 6 — 7th rank, very strong
   0,  0,  0,  4,  4,  0,  0,  0    // rank 7 — back rank, centre files slight bonus
};

// PST_QUEEN: very modest positional bonuses. The queen's value comes from
// tactics, not from PST. Strong central penalties on ranks 5–7 prevent
// premature development. Slightly negative on rim.
const int8_t PST_QUEEN[64] PROGMEM = {
  -20,-12,-12, -8, -8,-12,-12,-20,   // rank 0 — back rank
  -12, -4,  0,  0,  0,  0, -4,-12,   // rank 1
  -12,  0,  4,  4,  4,  4,  0,-12,   // rank 2
   -8,  0,  4,  6,  6,  4,  0, -8,   // rank 3
   -8,  0,  4,  6,  6,  4,  0, -8,   // rank 4
  -12, -2,  0,  2,  2,  0, -2,-12,   // rank 5 — less bonus, disincentivise forward queen
  -14, -6, -4, -4, -4, -4, -6,-14,   // rank 6 — penalty for early queen sortie
  -20,-12,-12, -8, -8,-12,-12,-20    // rank 7
};

// PST_KING_MID: king wants to castle and stay sheltered behind pawns.
// Castled positions (g1/c1 for white) rewarded. Centre is lethal.
const int8_t PST_KING_MID[64] PROGMEM = {
  -30,-40,-40,-50,-50,-40,-40,-30,   // rank 0 — back rank, exposed centre punished
  -30,-40,-40,-50,-50,-40,-40,-30,   // rank 1
  -30,-40,-40,-50,-50,-40,-40,-30,   // rank 2
  -30,-40,-40,-50,-50,-40,-40,-30,   // rank 3
  -20,-30,-30,-40,-40,-30,-30,-20,   // rank 4
  -10,-20,-20,-30,-30,-20,-20,-10,   // rank 5
   10, 12,  0,-10,-10,  0, 12, 10,   // rank 6 — castled positions OK
   22, 28, 16, -4, -4, 16, 28, 22    // rank 7 — g1/b1 castled squares rewarded
};

// PST_KING_END: king should centralise and become active.
const int8_t PST_KING_END[64] PROGMEM = {
  -50,-30,-20,-10,-10,-20,-30,-50,
  -30,-10,  8, 12, 12,  8,-10,-30,
  -20,  8, 24, 28, 28, 24,  8,-20,
  -10, 12, 28, 36, 36, 28, 12,-10,
  -10, 12, 28, 36, 36, 28, 12,-10,
  -20,  8, 24, 28, 28, 24,  8,-20,
  -30,-10,  8, 12, 12,  8,-10,-30,
  -50,-30,-20,-10,-10,-20,-30,-50
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
void drawSquare(int sq);
void drawLeftPanel();
// drawPhaseIndicator is embedded in drawLeftPanel — no separate declaration needed
void drawEvalBar(int evalCp);
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
int  alphaBeta(int depth, int alpha, int beta, bool maximizing, unsigned long startMs, int lastKnightSq=-1);
int  quiesce(int alpha, int beta);
Move getBotMove();
bool isSquareAttacked(int sq, bool byWhite);
int  pstValue(int piece, int sq);
int  pstValue(int piece, int sq, bool endgame);
inline int findKing(bool white);
void doRestart();
void doUndo();
Move getBookMove(int blackMoveNum);
char pieceChar(int p);
void drawPieceAt(int sx, int sy, int piece);
void clearSquarePixels(int sq);
int  gamePhase();
bool isEndgame();
static int IRAM_ATTR lvaSquare(int sq, int attColor);
int  seeCaptures(int sq, int attColor);
int  seeMoveGain(int fromSq, int toSq, int piece, int attColor);
bool insufficientMaterial();
bool isRepetition();
bool selectSquare(int sq);

void showThemeMenu();
void showSensitivityMenu();

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

// ─── Search depth/buffer constants (must be declared before any arrays that
//     reference them, including hashHistory below) ───────────────────────────
// MAX_DEPTH: killer/history table depth — must exceed search depth + extensions.
#define MAX_DEPTH     12
// AB_MAX_LEVELS: maximum alphaBeta recursion levels (depth 9 + 3 extensions + 1).
#define AB_MAX_LEVELS 13
// Q_MAX_LEVELS: maximum quiescence recursion levels.
#define Q_MAX_LEVELS  10

// hashHistory must hold: all game moves ever played (up to MAX_HISTORY)
// PLUS one entry per alphaBeta recursion level (up to AB_MAX_LEVELS=13).
// Previous size MAX_HISTORY+10 = 110 was too small: a 100-move game +
// 12-ply search = 112 writes → out-of-bounds write and crash.
// New size: MAX_HISTORY + AB_MAX_LEVELS + 4 = 121 (extra 4 for safety margin).
uint32_t hashHistory[MAX_HISTORY + AB_MAX_LEVELS + 4];
int hashHistoryCount = 0;

// ─── History heuristic table ──────────────────────────────────────────────────
int16_t histHeuristic[64][64];

// ─── Search globals ───────────────────────────────────────────────────────────
// The engine runs iterative deepening within a 5-second wall-clock budget.
// searchAborted is set when the time limit is hit, ending the current depth
// cleanly and returning the best move found so far.
// Per-depth best moves are always recorded, so the shallow-depth fallback
// logic has full data regardless of which depth was the last to complete.
bool searchAborted = false;
unsigned long searchStart;
uint32_t nodeCount = 0;
#define SEARCH_TIME_MS 5000UL   // 5-second hard budget

// ─── Development tracking ─────────────────────────────────────────────────────
uint64_t bPieceMoved = 0;
int      lastBlackFrom = -1;
int      lastBlackTo   = -1;

// ─── Phase cache ──────────────────────────────────────────────────────────────
int  cachedPhase      = -1;
uint32_t cachedPhaseHash = 0xFFFFFFFF;

// ─── Helpers ─────────────────────────────────────────────────────────────────
inline int rankOf(int sq)  { return sq >> 3; }
inline int fileOf(int sq)  { return sq & 7; }
inline int sqOf(int r, int f) { return (r<<3)|f; }
inline bool inBounds(int r, int f) { return ((unsigned)r < 8) & ((unsigned)f < 8); }
inline bool squareOnBoard(int sq)  { return (unsigned)sq < 64; }

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
      return endgame ? (int8_t)pgm_read_byte(&PST_KING_END[idx])
                     : (int8_t)pgm_read_byte(&PST_KING_MID[idx]);
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
  fullMoveCount=0;
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
void IRAM_ATTR makeMove(Move& m, HistoryEntry* he) {
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

void IRAM_ATTR unmakeMove(HistoryEntry& he) {
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

bool IRAM_ATTR isLegal(Move& m) {
  bool side = whiteToMove;
  HistoryEntry he;
  makeMove(m, &he);
  bool inChk = isInCheck(side);
  unmakeMove(he);
  return !inChk;
}

// ─── Repetition ───────────────────────────────────────────────────────────────
// Three-fold repetition: the same position must appear 3 times in hashHistory.
// When called from checkGameOver, the current position is already in hashHistory,
// so we count >= 3. (The search uses >= 2 because it checks before pushing.)
bool isRepetition() {
  uint32_t h = currentHash;
  int cnt = 0;
  for(int i=0;i<hashHistoryCount;i++) if(hashHistory[i]==h) cnt++;
  return cnt >= 3;
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
  return gamePhase() == 2;
}

int gamePhase() {
  if(cachedPhase >= 0 && cachedPhaseHash == currentHash) return cachedPhase;

  int total = 0;
  bool bQueenOnBoard = false;
  bool wQueenOnBoard = false;
  for(int s=0;s<64;s++) {
    int p=board[s]; if(!p) continue;
    int t=TYPE(p);
    if(t!=KING) total += PIECE_VAL[t];
    if(p==B_Q) bQueenOnBoard=true;
    if(p==W_Q) wQueenOnBoard=true;
  }

  int phase;
  if(total < 1800) {
    phase = 2;
  } else {
    // Only declare opening if BOTH queens are still present.
    // If either queen is gone the opening phase has ended.
    if(!bQueenOnBoard || !wQueenOnBoard) {
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
static int IRAM_ATTR lvaSquare(int sq, int attColor) {
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

int IRAM_ATTR seeCaptures(int sq, int attColor) {
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

int IRAM_ATTR seeMoveGain(int fromSq, int toSq, int piece, int attColor) {
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
        for(int pr = rr + 1; pr <= r - 1; pr++)
          if(board[sqOf(pr, adjF)] != EMPTY) { clear = false; break; }
        if(!clear) continue;
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
        for(int pr = rr - 1; pr >= r + 1; pr--)
          if(board[sqOf(pr, adjF)] != EMPTY) { clear = false; break; }
        if(!clear) continue;
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

// ─── Fast pawn-threat check (move ordering only — no SEE simulation) ─────────
// Returns a penalty magnitude (always positive). Caller decides sign.
// Does NOT call SEE — safe to call inside moveScore hot path.
// Checks: direct kick (enemy pawn already attacks sq), imminent kick (1 push away).
// Does NOT distinguish outposts — that requires SEE (done in evaluate only).
static inline int fastPawnThreat(int sq, int pieceColor) {
  int r = rankOf(sq), f = fileOf(sq);
  int enemyPawn = (pieceColor == 2) ? W_P : B_P;
  int atkOff    = (pieceColor == 2) ? -1 : 1; // rank offset of attacking pawn

  // Direct kick
  for(int df = -1; df <= 1; df += 2) {
    int pr = r + atkOff, pf = f + df;
    if(inBounds(pr, pf) && board[sqOf(pr, pf)] == enemyPawn) return 70;
  }
  // Imminent kick (pawn one push away, path clear)
  int imR = r + atkOff * 2;
  for(int df = -1; df <= 1; df += 2) {
    int pf = f + df;
    if(!inBounds(imR, pf)) continue;
    if(board[sqOf(imR, pf)] != enemyPawn) continue;
    int midR = r + atkOff;
    if(inBounds(midR, pf) && board[sqOf(midR, pf)] == EMPTY) return 45;
  }
  return 0;
}

// ─── Full pawn-threat penalty (evaluate only — uses SEE for outpost test) ────
// Returns white-relative score delta: positive = bad for black, negative = bad for white.
int pieceThreatPenalty(int sq, int piece) {
  int tp = TYPE(piece);
  if(tp != KNIGHT && tp != BISHOP) return 0;

  int r = rankOf(sq), f = fileOf(sq);
  int pieceColor = COLOR(piece);
  int enemyPawn  = (pieceColor == 2) ? W_P : B_P;
  int atkOff     = (pieceColor == 2) ? -1 : 1;
  bool opening   = isOpeningPhase();
  int sign       = (pieceColor == 2) ? 1 : -1;

  // Direct kick
  bool directKick = false;
  for(int df = -1; df <= 1; df += 2) {
    int pr = r + atkOff, pf = f + df;
    if(inBounds(pr, pf) && board[sqOf(pr, pf)] == enemyPawn) { directKick = true; break; }
  }

  bool outpost = isOutpostSquare(sq, pieceColor);

  if(directKick) {
    if(outpost) {
      bool central = (f >= 2 && f <= 5 && r >= 3 && r <= 5);
      return central ? -sign * 12 : 0;
    }
    return sign * (opening ? 70 : 45);
  }

  if(!outpost) {
    int imR = r + atkOff * 2;
    for(int df = -1; df <= 1; df += 2) {
      int pf = f + df;
      if(!inBounds(imR, pf)) continue;
      if(board[sqOf(imR, pf)] != enemyPawn) continue;
      int midR = r + atkOff;
      if(inBounds(midR, pf) && board[sqOf(midR, pf)] == EMPTY)
        return sign * (opening ? 50 : 32);
    }
  }
  return 0;
}

// ─── Evaluation ───────────────────────────────────────────────────────────────
// White-relative: positive = good for white, negative = good for black.
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

  // ── King safety (middlegame only) ─────────────────────────────────────────
  // Each open/semi-open file adjacent to the king costs 25cp (was 10).
  // An exposed king is a strategic catastrophe worth ~75cp penalty minimum.
  if(!endgame) {
    if(wKsq >= 0) {
      int kf = fileOf(wKsq);
      for(int df = -1; df <= 1; df++) {
        int f2 = kf+df; if(f2<0||f2>7) continue;
        bool sh = false;
        for(int r = rankOf(wKsq)+1; r < 8; r++) if(board[sqOf(r,f2)]==W_P){sh=true;break;}
        if(!sh) score -= 25;
      }
    }
    if(bKsq >= 0) {
      int kf = fileOf(bKsq);
      for(int df = -1; df <= 1; df++) {
        int f2 = kf+df; if(f2<0||f2>7) continue;
        bool sh = false;
        for(int r = rankOf(bKsq)-1; r >= 0; r--) if(board[sqOf(r,f2)]==B_P){sh=true;break;}
        if(!sh) score += 25;
      }
    }
  }

  // ── King activity in endgame ──────────────────────────────────────────────
  if(endgame) {
    if(wKsq >= 0) { int kr=rankOf(wKsq), kf=fileOf(wKsq);
      score += (8 - (abs(kr-3)+abs(kr-4)+abs(kf-3)+abs(kf-4))) * 4; }
    if(bKsq >= 0) { int kr=rankOf(bKsq), kf=fileOf(bKsq);
      score -= (8 - (abs(kr-3)+abs(kr-4)+abs(kf-3)+abs(kf-4))) * 4; }
  }

  // ── Piece mobility ────────────────────────────────────────────────────────
  // Count reachable squares for knights, bishops, rooks, queens.
  // Trapped or restricted pieces get a direct score penalty.
  // Uses fast square-counting — no SEE, no simulation, O(moves) per piece.
  {
    const int Ndx[8][2]={{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
    const int Bdx[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
    const int Rdx[4][2]={{1,0},{-1,0},{0,1},{0,-1}};

    for(int s=0;s<64;s++) {
      int p=board[s]; if(!p) continue;
      int t=TYPE(p); if(t==PAWN||t==KING) continue;
      int mob=0;
      int myColor=COLOR(p);

      if(t==KNIGHT) {
        for(auto& d:Ndx){int r2=rankOf(s)+d[0],f2=fileOf(s)+d[1];
          if(inBounds(r2,f2)&&COLOR(board[sqOf(r2,f2)])!=myColor) mob++;}
        // Knight mobility scale: 0sq=-30, 1=-20, 2=-10, 3=0, 4..8=+2 each
        int bonus = (mob<=2) ? (mob-3)*10 : (mob-3)*2;
        if(IS_WHITE(p)) score+=bonus; else score-=bonus;
      }
      else if(t==BISHOP) {
        for(auto& d:Bdx){int r2=rankOf(s)+d[0],f2=fileOf(s)+d[1];
          while(inBounds(r2,f2)){
            if(board[sqOf(r2,f2)]){if(COLOR(board[sqOf(r2,f2)])!=myColor)mob++;break;}
            mob++;r2+=d[0];f2+=d[1];}}
        // Bishop: 0-2sq = penalty, 3-5 = neutral, 6+ = small bonus
        int bonus = (mob<=2) ? (mob-3)*8 : (mob<=5) ? 0 : (mob-5)*3;
        if(IS_WHITE(p)) score+=bonus; else score-=bonus;
      }
      else if(t==ROOK) {
        for(auto& d:Rdx){int r2=rankOf(s)+d[0],f2=fileOf(s)+d[1];
          while(inBounds(r2,f2)){
            if(board[sqOf(r2,f2)]){if(COLOR(board[sqOf(r2,f2)])!=myColor)mob++;break;}
            mob++;r2+=d[0];f2+=d[1];}}
        // Rook: <4sq = penalised (blocked rook), 7+ = bonus
        int bonus = (mob<4) ? (mob-4)*6 : (mob>=7) ? (mob-6)*3 : 0;
        if(IS_WHITE(p)) score+=bonus; else score-=bonus;
      }
      else if(t==QUEEN) {
        for(auto& d:Bdx){int r2=rankOf(s)+d[0],f2=fileOf(s)+d[1];
          while(inBounds(r2,f2)){
            if(board[sqOf(r2,f2)]){if(COLOR(board[sqOf(r2,f2)])!=myColor)mob++;break;}
            mob++;r2+=d[0];f2+=d[1];}}
        for(auto& d:Rdx){int r2=rankOf(s)+d[0],f2=fileOf(s)+d[1];
          while(inBounds(r2,f2)){
            if(board[sqOf(r2,f2)]){if(COLOR(board[sqOf(r2,f2)])!=myColor)mob++;break;}
            mob++;r2+=d[0];f2+=d[1];}}
        // Queen: small bonus per square beyond 10, penalty below 5
        int bonus = (mob<5) ? (mob-5)*4 : (mob>10) ? (mob-10)*2 : 0;
        if(IS_WHITE(p)) score+=bonus; else score-=bonus;
      }
    }
  }

  // ── Opening-specific penalties ────────────────────────────────────────────
  if(opening) {
    // Undeveloped minor piece penalty — each piece costs more the longer it stays home
    if(board[sqOf(7,1)]==B_N) score += 30; if(board[sqOf(7,6)]==B_N) score += 30;
    if(board[sqOf(7,2)]==B_B) score += 25; if(board[sqOf(7,5)]==B_B) score += 25;
    if(board[sqOf(0,1)]==W_N) score -= 30; if(board[sqOf(0,6)]==W_N) score -= 30;
    if(board[sqOf(0,2)]==W_B) score -= 25; if(board[sqOf(0,5)]==W_B) score -= 25;

    // Castling rights: a small bonus for still having the option.
    // We do NOT penalise losing rights because losing them via castling is good,
    // not bad — king safety is evaluated separately via the pawn shield check.
    if(bCastle_K||bCastle_Q) score -= 15;  // black has options (good for black)
    if(wCastle_K||wCastle_Q) score += 15;  // white has options (good for white)

    // Premature queen development — only penalise if the queen is still alive
    // but has left its home square early (not if it was captured).
    { int wMH=0,bMH=0;
      if(board[sqOf(0,1)]==W_N)wMH++;if(board[sqOf(0,6)]==W_N)wMH++;
      if(board[sqOf(0,2)]==W_B)wMH++;if(board[sqOf(0,5)]==W_B)wMH++;
      if(board[sqOf(7,1)]==B_N)bMH++;if(board[sqOf(7,6)]==B_N)bMH++;
      if(board[sqOf(7,2)]==B_B)bMH++;if(board[sqOf(7,5)]==B_B)bMH++;
      // Check if queen is on board but not on home square
      bool wQueenAlive=false, bQueenAlive=false;
      for(int s=0;s<64;s++){if(board[s]==W_Q)wQueenAlive=true;if(board[s]==B_Q)bQueenAlive=true;}
      if(wQueenAlive && board[sqOf(0,3)]!=W_Q && wMH>=2) score -= 40;
      if(bQueenAlive && board[sqOf(7,3)]!=B_Q && bMH>=2) score += 40;
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

  // ── Pawn threat on knights/bishops (fast, no SEE) ─────────────────────────
  // Use fastPawnThreat only — the full SEE-backed pieceThreatPenalty is too
  // expensive to call at every leaf node (it calls seeCaptures inside a loop).
  for(int s = 0; s < 64; s++) {
    int p = board[s]; if(!p) continue;
    if(TYPE(p) != KNIGHT && TYPE(p) != BISHOP) continue;
    int pen = fastPawnThreat(s, COLOR(p));
    if(pen > 0) {
      if(IS_WHITE(p)) score -= pen; else score += pen;
    }
  }

  return score;
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

  // ── 3. Pawn threat on landing square — fast check, no SEE ───────────────
  // Use fastPawnThreat (no SEE simulation) for move ordering hot path.
  // Full SEE-backed version only runs in evaluate().
  if(cap == EMPTY && (TYPE(piece)==KNIGHT || TYPE(piece)==BISHOP)) {
    uint8_t sTo = board[m.to], sFrom = board[m.from];
    board[m.to] = piece; board[m.from] = EMPTY;
    int pen = fastPawnThreat(m.to, COLOR(piece));
    board[m.to] = sTo; board[m.from] = sFrom;
    if(pen > 0) s -= pen;  // bad landing for any piece — lower its move score
  }

  // ── 4. Black opening heuristics (only in opening, only for black) ─────────
  if(!isBlack || !opening) return s;

  int undevMinors = 0;
  if(board[sqOf(7,1)]==B_N) undevMinors++;
  if(board[sqOf(7,6)]==B_N) undevMinors++;
  if(board[sqOf(7,2)]==B_B) undevMinors++;
  if(board[sqOf(7,5)]==B_B) undevMinors++;

  bool canCastle = (bCastle_K || bCastle_Q);

  // ── Rook moves: banned until ALL minors are developed AND castling is done.
  // A rook shuffle (Ra8→Rb8 etc.) is never useful in the opening.
  // Even on an open file, the bonus (+200) must never beat this penalty.
  if(TYPE(piece)==ROOK) {
    if(undevMinors > 0) {
      s -= 2000;   // absolute ban while any minor is undeveloped
    } else if(canCastle) {
      s -= 1200;   // still heavily discouraged until king is safe
    } else {
      // Post-castling: small penalty for lateral shuffle, bonus for open file / 7th rank
      if(m.to == lastBlackFrom && cap == EMPTY) s -= 350;
      bool openFile = true;
      for(int r=0;r<8;r++) if(board[sqOf(r,toF)]==B_P){openFile=false;break;}
      if(openFile) s += 80;
      if(toR==1)   s += 120;
    }
    return s;  // return early — no other opening bonuses apply to rooks
  }

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
    if(cap && TYPE(cap)==PAWN && pawnSEE>=0) s += 180;
  }

  // Queen: heavy penalties for premature development.
  // Additional flat penalty if king hasn't castled yet — queen activity
  // before king safety is a strategic inaccuracy regardless of undev count.
  if(TYPE(piece)==QUEEN) {
    if(undevMinors>=3) s -= 2000;
    else if(undevMinors==2) s -= 1600;
    else if(undevMinors==1) s -= 900;
    if(canCastle) s -= 500;   // flat castling-urgency penalty
    if(toR>=3&&toR<=5&&toF>=2&&toF<=5) s -= 400;
    if(cap!=EMPTY && undevMinors>=1) s -= 500;
  }

  // King: strong penalty for moving without castling
  if(TYPE(piece)==KING && m.flags==0) s -= 1500;

  // Development bonus: first move of a home-rank minor piece
  if(frR==7 && (TYPE(piece)==KNIGHT||TYPE(piece)==BISHOP)) s += 400;

  // Re-move penalties
  if(m.from == lastBlackTo && cap == EMPTY) s -= 800;
  if((bPieceMoved & (1ULL << m.from)) && cap == EMPTY)
    s -= 400 + undevMinors * 120;

  // Extra penalty for moving a knight that is currently on a "retreat square"
  // (i.e. it moved backward on its last move — a sign it was just kicked).
  // Moving it again instead of developing another piece wastes another tempo.
  if(TYPE(piece)==KNIGHT && cap==EMPTY) {
    // Retreat squares are ranks 6-7 for black (home territory).
    // A knight that came back to rank 6 or 7 after having moved is a retreated knight.
    if(frR >= 6 && (bPieceMoved & (1ULL << m.from))) {
      s -= 500 + undevMinors * 150; // very heavy: develop something else first
    }
  }

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
        if(board[psq]==B_P && seeCaptures(psq,1)>0){s+=400;break;}}
    }
    // Deep kick-trap check: penalise landing squares where a white pawn can
    // reach an attacking square within 2 pushes. Fires throughout opening phase
    // so the search avoids walk-into-kick sequences from move 4 onwards.
    // This is the search equivalent of isKickTrap2 in the book.
    if(opening && cap==EMPTY) {
      for(int df=-1;df<=1;df+=2) {
        int adjF=toF+df; if(adjF<0||adjF>7) continue;
        int atkR=toR-1; if(!inBounds(atkR,adjF)) continue;
        for(int pr=0;pr<atkR;pr++) {
          if(board[sqOf(pr,adjF)]!=W_P) continue;
          int pushes=atkR-pr; if(pushes>2) break;
          bool clear=true;
          for(int rr=pr+1;rr<=atkR;rr++) if(board[sqOf(rr,adjF)]!=EMPTY){clear=false;break;}
          if(!clear) break;
          int aSq=sqOf(atkR,adjF);
          uint8_t sA=board[aSq],sP=board[sqOf(pr,adjF)];
          board[aSq]=W_P; board[sqOf(pr,adjF)]=EMPTY;
          int safety=seeCaptures(aSq,2);
          board[aSq]=sA; board[sqOf(pr,adjF)]=sP;
          if(safety<=0) { s -= 600; break; } // pawn push safe → heavy penalty
          break;
        }
      }
    }
  }

  // Bishop positional bonuses/penalties.
  // Extra penalty for moving to an aggressive forward square (rank 3-4)
  // while the king hasn't castled — bishop sorties before king safety
  // are the classic source of "bishop check" inaccuracies.
  if(TYPE(piece)==BISHOP) {
    if((toR==6&&toF==1)||(toR==6&&toF==6)) s += 80;
    if(toF==0||toF==7) s -= 120;
    if(board[sqOf(7,1)]==B_N||board[sqOf(7,6)]==B_N) s -= 100;
    if((toF==3||toF==4)&&toR==5&&board[sqOf(6,toF)]==B_P) s -= 100;
    // Penalise aggressive bishop sorties (rank ≤4 from black's side = ranks 0-3)
    // while king hasn't castled and minors are still at home
    if(canCastle && toR <= 3 && undevMinors >= 1) s -= 350;
  }

  // General centrality bonus for knights and bishops
  if(TYPE(piece)==KNIGHT||TYPE(piece)==BISHOP) {
    int cd = abs(toF-3)+abs(toF-4)+abs(toR-3)+abs(toR-4);
    s += (8 - cd) * 12;
    if(m.to == lastBlackFrom && cap == EMPTY) s -= 220;
  }

  // Backward move penalty — skip in opening to allow retreat after kicks
  if(!opening && toR > frR && cap == EMPTY) s -= 100;

  // Castling: reward if safe
  if(m.flags==2||m.flags==3) {
    int maxHang = 0;
    for(int sq=0;sq<64;sq++) {
      if(!board[sq]||!IS_BLACK(board[sq])||TYPE(board[sq])==KING) continue;
      int loss=seeCaptures(sq,1); if(loss>maxHang) maxHang=loss;
    }
    s += (maxHang==0) ? 600 : (maxHang<=100) ? 200 : -800;
  }

  return s;
}


void sortMoves(Move* moves, int count) {
  // Insertion sort with scores — good for small count (chess moves rarely >60)
  static int scores[128];  // static: avoids 512B stack alloc per call; safe — single search task
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
// MAX_DEPTH, AB_MAX_LEVELS and Q_MAX_LEVELS are defined in the globals section
// above (before hashHistory) so that the array size expression compiles.
Move killers[MAX_DEPTH][2];

// ─── Per-depth move buffers ───────────────────────────────────────────────────
Move abMoveBuf[AB_MAX_LEVELS][128];
Move qMoveBuf [Q_MAX_LEVELS ][64];
Move rootMoveBuf[128];
int  abDepthLevel = 0;
int  qDepthLevel  = 0;

// ─── TT score clamping helper ─────────────────────────────────────────────────
// int16_t holds ±32767. Alpha/beta sentinels are ±99999 which overflows int16_t.
// Mate scores (±19000) and normal evals are within range; only sentinels need
// clamping. Applied at every TT store site to prevent corrupted cutoffs.
static inline int16_t clampTTScore(int s) {
  if(s >  32767) return  32767;
  if(s < -32767) return -32767;
  return (int16_t)s;
}

// ─── Quiescence search ───────────────────────────────────────────────────────
// Searches all captures (and losing-SEE captures filtered out) until the
// position is quiet. No time-based abort — always completes. Progress bar
// is updated periodically from node count for UI responsiveness.
int quiesce(int alpha, int beta) {
  if(searchAborted) return 0;
  nodeCount++;
  if((nodeCount & 255) == 0) {
    unsigned long elapsed = millis() - searchStart;
    if(elapsed >= SEARCH_TIME_MS) {
      searchAborted = true;
      return 0;
    }
    int _pct = (int)((elapsed * 99UL) / SEARCH_TIME_MS);
    if(_pct > 99) _pct = 99;
    if(_pct > (int)searchPct) searchPct = (int)_pct;
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
// Pruning techniques:
//   - Transposition table (exact / lower / upper bound)
//   - Repetition detection → draw score
//   - Null-move pruning (adaptive R: 2 at depth<5, 3 at depth≥5), skipped in
//     endgame zugzwang risk or when side has only pawns+king
//   - Futility pruning at depth==1 (static eval + margin ≤ alpha)
//   - Late Move Reductions (LMR): i≥3, depth≥2, quiet non-killer non-check moves
//     isQuiet is saved PRE-makeMove to avoid reading stale board state.
//   - Killer move ordering, history heuristic
//   - Check extension: +1 ply when in check
//   - Knight pawn-kick lookahead: prune black knight moves to squares where
//     a white pawn can safely advance to attack within 2 pushes (opening only)
//
// ANTI-SACRIFICE TRAP: LMR always re-searches at full depth when the reduced
// score beats alpha — no tactical sequence can slip past the horizon.
int alphaBeta(int depth, int alpha, int beta, bool maximizing, unsigned long startMs, int lastKnightSq) {
  if(searchAborted) return 0;

  int myLevel = abDepthLevel++;
  if(myLevel >= AB_MAX_LEVELS) {
    // Recursion depth cap hit — return a quiescence eval rather than 0,
    // which would corrupt the search score completely.
    abDepthLevel = myLevel;
    return quiesce(alpha, beta);
  }

  // Time check every 256 nodes — abort and update progress bar.
  // Progress = elapsed / SEARCH_TIME_MS * 100, clamped to 99.
  // This is monotonically increasing and matches real wall-clock time exactly.
  nodeCount++;
  if((nodeCount & 255) == 0) {
    unsigned long elapsed = millis() - searchStart;
    if(elapsed >= SEARCH_TIME_MS) {
      abDepthLevel = myLevel;
      searchAborted = true;
      return 0;
    }
    int _pct = (int)((elapsed * 99UL) / SEARCH_TIME_MS);
    if(_pct > 99) _pct = 99;
    if(_pct > (int)searchPct) searchPct = (int)_pct;
  }

  bool inCheck = isInCheck(whiteToMove);
  if(inCheck && depth > 0) depth++;   // check extension

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

  // ── Null-move pruning ────────────────────────────────────────────────────
  // Skipped: in check, at depth<3, or when side has only pawns+king
  // (zugzwang risk in endgame). Safe to apply — the opponent's moves at ALL
  // nodes are explored fully regardless of this pruning.
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
      int nullScore = -alphaBeta(depth-1-R, -beta, -beta+1, !maximizing, startMs, -1);

      whiteToMove = savedWTM;
      enPassantSq = savedEP;
      currentHash = savedHash;

      if(nullScore >= beta && !searchAborted) { abDepthLevel = myLevel; return beta; }
    }
  }

  // ── Futility pruning (depth==1 only) ─────────────────────────────────────
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

  // Pre-compute opening phase and undev count once for the pruning check below
  bool inOpening = isOpeningPhase();

  // Pre-compute static eval for depth-1 futility pruning (avoid per-move re-eval)
  int futilityEval = 0;
  bool doFutility = (depth == 1 && !inCheck);
  if(doFutility) futilityEval = whiteToMove ? evaluate() : -evaluate();

  for(int i=0;i<count;i++) {
    Move& m = moves[i];
    if(!isLegal(m)) continue;

    // ── Knight pawn-kick lookahead pruning ───────────────────────────────
    // Applies to BLACK's knight moves in the opening (we are the engine
    // playing black). After a black knight lands on square X, we check
    // whether any white pawn can be pushed within 2 moves to attack X.
    // "Attack X" means the pawn reaches rank(X)-1 on file(X)±1.
    //
    // The check is:
    //   For each adjacent file adjF = fileOf(X) ± 1:
    //     atkRank = rankOf(X) - 1          (white pawn attacks from below)
    //     Is there a white pawn on adjF at some rank pr < atkRank,
    //     with a clear path to atkRank (no pieces in the way),
    //     in at most 2 pushes (atkRank - pr <= 2),
    //     AND the pawn landing on atkRank is safe (SEE ≤ 0 for black)?
    //   If yes → knight is kickable → prune this branch.
    //
    // Safety guard: never prune if it's the only legal move so far
    // (legalCount2 == 0), and never prune when in check.
    //
    // Companion heuristic: if the engine itself is choosing a knight move
    // for black (!whiteToMove) and the destination IS kick-safe, we also
    // check whether advancing the h- or a-pawn to rank 5 (the 6th rank
    // from black's perspective, rank index 5) would shield the knight on
    // the adjacent flank. This is handled in moveScore() via the existing
    // deep-kick-trap penalty; we only prune here, not boost.
    if(inOpening && !whiteToMove && !inCheck
       && TYPE(board[m.from]) == KNIGHT
       && board[m.to] == EMPTY) {
      bool isBlackKnight = IS_BLACK(board[m.from]);
      if(isBlackKnight) {
        // Destination square for the black knight
        int kR = rankOf(m.to), kF = fileOf(m.to);
        int atkRank = kR - 1; // white pawns attack from rank(X)-1
        bool kickable = false;
        if(atkRank >= 0) {
          for(int df = -1; df <= 1; df += 2) {
            int adjF = kF + df;
            if(adjF < 0 || adjF > 7) continue;
            // Look for a white pawn on adjF below atkRank, reachable in ≤2 pushes
            for(int pr = 0; pr < atkRank; pr++) {
              if(board[sqOf(pr, adjF)] != W_P) continue;
              int pushes = atkRank - pr;
              if(pushes > 2) break; // too far away
              // Check path is clear
              bool clear = true;
              for(int rr = pr + 1; rr <= atkRank; rr++) {
                if(board[sqOf(rr, adjF)] != EMPTY) { clear = false; break; }
              }
              if(!clear) break;
              // Simulate pawn on atkRank and test if it survives (SEE)
              int aSq = sqOf(atkRank, adjF);
              uint8_t sA = board[aSq], sP = board[sqOf(pr, adjF)];
              board[aSq] = W_P; board[sqOf(pr, adjF)] = EMPTY;
              int safety = seeCaptures(aSq, 2); // can black recapture?
              board[aSq] = sA; board[sqOf(pr, adjF)] = sP;
              if(safety <= 0) { kickable = true; break; } // pawn advance safe → knight gets kicked
              break;
            }
            if(kickable) break;
          }
        }
        if(kickable && legalCount2 > 0) continue;
      }
    }

    legalCount2++;

    if(doFutility && board[m.to]==EMPTY && !m.promo) {
      if(futilityEval + 150 <= alpha) continue;
    }

    // Compute the knight tracking square for child nodes.
    bool movingKnight = (!whiteToMove && TYPE(board[m.from]) == KNIGHT);
    // Save whether this is a quiet move BEFORE makeMove corrupts board[m.to].
    // LMR must only apply to quiet non-capture moves; checking board[m.to]
    // after makeMove would always see a piece there (the one that just moved).
    bool isQuiet = (board[m.to] == EMPTY) && !m.promo;

    HistoryEntry he;
    makeMove(m, &he);
    // Bounds guard: never write past hashHistory array
    if(hashHistoryCount < MAX_HISTORY + AB_MAX_LEVELS + 3)
      hashHistory[hashHistoryCount++] = currentHash;
    else
      hashHistoryCount++;  // still increment so decrement is balanced

    // childKnightSq: keep passing the landing square of a black knight move
    // forward so any future re-entry of this pruning check at deeper nodes
    // can also fire on the same knight if it lands somewhere kickable.
    int childKnightSq = -1;
    if(movingKnight) {
      childKnightSq = m.to;
    } else if(whiteToMove) {
      childKnightSq = lastKnightSq;
    }

    int score;
    int depthIdx = min(depth, MAX_DEPTH-1);
    bool isKiller = (m.from==killers[depthIdx][0].from && m.to==killers[depthIdx][0].to) ||
                    (m.from==killers[depthIdx][1].from && m.to==killers[depthIdx][1].to);

    // ── Late Move Reductions (LMR) ────────────────────────────────────────
    // Only reduce quiet non-killer moves at move index ≥ 3 and depth ≥ 2.
    // isQuiet was captured before makeMove — the only correct place to read it.
    if(i >= 3 && depth >= 2 && isQuiet && !inCheck && !isKiller) {
      int R = (i >= 6 && depth >= 4) ? 2 : 1;
      score = -alphaBeta(depth-1-R, -alpha-1, -alpha, !maximizing, startMs, childKnightSq);
      if(score > alpha && !searchAborted) {
        score = -alphaBeta(depth-1, -beta, -alpha, !maximizing, startMs, childKnightSq);
      }
    } else {
      score = -alphaBeta(depth-1, -beta, -alpha, !maximizing, startMs, childKnightSq);
    }

    hashHistoryCount--;
    unmakeMove(he);
    if(searchAborted) { abDepthLevel = myLevel; return 0; }

    if(score >= beta) {
      if(board[m.to]==EMPTY) {
        killers[min(depth, MAX_DEPTH-1)][1]=killers[min(depth, MAX_DEPTH-1)][0];
        killers[min(depth, MAX_DEPTH-1)][0]=m;
        { int hh = histHeuristic[m.from][m.to] + depth*depth; histHeuristic[m.from][m.to] = (int16_t)(hh > 16000 ? 16000 : hh); }
      }
      tte.hashHi=(h>>16); tte.score=clampTTScore(score); tte.depth=depth;
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
    tte.hashHi=(h>>16); tte.score=clampTTScore(alpha); tte.depth=depth;
    tte.flags=(alpha>origAlpha)?1:3;
    tte.from=bestMove.from; tte.to=bestMove.to;
  }

  abDepthLevel = myLevel;
  return alpha;
}

// ─── Opening Book ─────────────────────────────────────────────────────────────
// Covers black's first THREE moves. Every candidate is verified against the
// actual legal move list before being returned — no illegal move is ever possible.
//
// Move-1 coverage: ALL realistic white first moves (e4,d4,c4,f4,g3,b3,Nf3,
//   Nc3,f3,e3,d3,h4,g4,b4,a4 and everything else).
// Move-2/3 coverage: principled development avoiding kick traps.
//
// isKickTrap2: checks whether a square will be attacked by a white pawn
//   within TWO pushes (not just one). This catches the 4-ply kick pattern:
//   knight lands on f6, white plays g4-g5, knight kicked.
Move getBookMove(int blackMoveNum) {
  Move none = {0,0,0,0};

  Move legal[128]; int legalCnt = 0;
  generateMoves(false, legal, legalCnt, false);
  Move trueLegal[128]; int trueCnt = 0;
  for(int i = 0; i < legalCnt; i++)
    if(isLegal(legal[i])) trueLegal[trueCnt++] = legal[i];
  if(trueCnt == 0) return none;

  auto tryMove = [&](int from, int to) -> Move {
    if(!squareOnBoard(from) || !squareOnBoard(to)) return none;
    if(!board[from] || !IS_BLACK(board[from])) return none;
    if(board[to] && IS_BLACK(board[to])) return none;
    for(int i = 0; i < trueCnt; i++)
      if(trueLegal[i].from == from && trueLegal[i].to == to) return trueLegal[i];
    return none;
  };

  // ── Deep kick-trap check (2 white pawn pushes) ───────────────────────────
  // Returns true if a white pawn can safely reach a square that attacks `sq`
  // within 1 or 2 pushes. This catches both immediate and one-move-away kicks.
  auto isKickTrap2 = [&](int sq) -> bool {
    int r = rankOf(sq), f = fileOf(sq);
    // Check each adjacent file for a white pawn that can reach an attack square
    for(int df = -1; df <= 1; df += 2) {
      int adjF = f + df;
      if(adjF < 0 || adjF > 7) continue;
      // Attack square = one rank below sq (white pawns attack upward)
      int atkR = r - 1;
      if(!inBounds(atkR, adjF)) continue;
      // Look for a white pawn on adjF at ranks 0..atkR-1 that can reach atkR
      for(int pr = 0; pr < atkR; pr++) {
        if(board[sqOf(pr, adjF)] != W_P) continue;
        // Can this pawn reach atkR? Path must be clear, at most 2 pushes away.
        int pushes = atkR - pr;
        if(pushes > 2) break; // pawn is too far away even with 2 pushes
        bool pathClear = true;
        for(int rr = pr + 1; rr <= atkR; rr++)
          if(board[sqOf(rr, adjF)] != EMPTY) { pathClear = false; break; }
        if(!pathClear) break;
        // Simulate pawn on attack square — is the push safe for white?
        int aSq = sqOf(atkR, adjF);
        uint8_t sA = board[aSq], sP = board[sqOf(pr, adjF)];
        board[aSq] = W_P; board[sqOf(pr, adjF)] = EMPTY;
        int safety = seeCaptures(aSq, 2); // can black recapture the pawn?
        board[aSq] = sA; board[sqOf(pr, adjF)] = sP;
        if(safety <= 0) return true; // pawn push is safe → this square is a kick trap
        break;
      }
    }
    return false;
  };

  // ── Board state reads ─────────────────────────────────────────────────────
  // White pawns present
  bool wE4  = (board[sqOf(3,4)] == W_P);
  bool wD4  = (board[sqOf(3,3)] == W_P);
  bool wC4  = (board[sqOf(3,2)] == W_P);
  bool wF4  = (board[sqOf(3,5)] == W_P);
  bool wG4  = (board[sqOf(3,6)] == W_P);
  bool wH4  = (board[sqOf(3,7)] == W_P);
  bool wB4  = (board[sqOf(3,1)] == W_P);
  bool wA4  = (board[sqOf(3,0)] == W_P);
  bool wE5  = (board[sqOf(4,4)] == W_P);
  bool wD5  = (board[sqOf(4,3)] == W_P);
  // White pieces not on home squares (white has developed)
  bool wNf3 = (board[sqOf(2,5)] == W_N); // Nf3
  bool wNc3 = (board[sqOf(2,2)] == W_N); // Nc3
  bool wNh3 = (board[sqOf(2,7)] == W_N); // Nh3
  bool wNa3 = (board[sqOf(2,0)] == W_N); // Na3
  bool wG3P = (board[sqOf(2,6)] == W_P); // g3 pawn advance
  bool wB3P = (board[sqOf(2,1)] == W_P); // b3 pawn advance
  bool wF3P = (board[sqOf(2,5)] == W_P); // f3 pawn advance
  bool wE3P = (board[sqOf(2,4)] == W_P); // e3 pawn advance
  bool wD3P = (board[sqOf(2,3)] == W_P); // d3 pawn advance
  // White's first-rank pawns still home (helps detect flank openings)
  bool wE2home = (board[sqOf(1,4)] == W_P);
  bool wD2home = (board[sqOf(1,3)] == W_P);

  // Black pieces
  bool bE5 = (board[sqOf(4,4)] == B_P);
  bool bD5 = (board[sqOf(4,3)] == B_P);
  bool bC5 = (board[sqOf(4,2)] == B_P);
  bool bF5 = (board[sqOf(4,5)] == B_P);
  bool bD6 = (board[sqOf(5,3)] == B_P);
  bool bE6 = (board[sqOf(5,4)] == B_P);

  // Named squares for black pieces
  int bNb8=sqOf(7,1), bNg8=sqOf(7,6);
  int bBc8=sqOf(7,2), bBf8=sqOf(7,5);
  int bPe7=sqOf(6,4), bPd7=sqOf(6,3), bPc7=sqOf(6,2), bPf7=sqOf(6,5);
  int bPg7=sqOf(6,6), bPh7=sqOf(6,7), bPb7=sqOf(6,1), bPa7=sqOf(6,0);

  // Common destination squares
  int nf6=sqOf(5,5), nc6=sqOf(5,2), nd7=sqOf(6,3);
  int e6=sqOf(5,4), d6=sqOf(5,3), c6=sqOf(5,2);
  int e5=sqOf(4,4), d5=sqOf(4,3), c5=sqOf(4,2);
  int g6=sqOf(5,6), f5=sqOf(4,5);

  int v = chosenOpening % 4;
  Move candidate = none;

  // ═══════════════════════════════════════════════════════════════════════════
  // MOVE 1
  // ═══════════════════════════════════════════════════════════════════════════
  if(blackMoveNum == 1) {

    if(wE4) {
      // 1.e4 — most common. Respond e5 or c5 depending on variation.
      if(v <= 1) candidate = tryMove(bPe7, e5);       // 1...e5
      else       candidate = tryMove(bPc7, c5);       // 1...c5 Sicilian
      if(!candidate.from) candidate = tryMove(bNg8, nf6); // fallback Nf6

    } else if(wD4) {
      // 1.d4 — Queen's pawn. d5 or Nf6 (King's Indian setup)
      if(v <= 1) candidate = tryMove(bPd7, d5);       // 1...d5
      else if(v==2) candidate = tryMove(bNg8, nf6);   // 1...Nf6 KID
      else       candidate = tryMove(bPf7, f5);       // 1...f5 Dutch
      if(!candidate.from) candidate = tryMove(bPd7, d5);

    } else if(wC4) {
      // 1.c4 English — e5 or c5
      if(v <= 1) candidate = tryMove(bPe7, e5);
      else       candidate = tryMove(bPc7, c5);
      if(!candidate.from) candidate = tryMove(bNg8, nf6);

    } else if(wF4) {
      // 1.f4 Bird's — d5 counters in the centre
      candidate = tryMove(bPd7, d5);
      if(!candidate.from) candidate = tryMove(bPe7, e5); // From's Gambit style

    } else if(wG4) {
      // 1.g4 Grob — d5, occupy centre immediately
      candidate = tryMove(bPd7, d5);
      if(!candidate.from) candidate = tryMove(bPe7, e5);

    } else if(wH4) {
      // 1.h4 — just claim the centre
      candidate = tryMove(bPe7, e5);
      if(!candidate.from) candidate = tryMove(bPd7, d5);

    } else if(wB4) {
      // 1.b4 Polish — e5 or d5
      candidate = tryMove(bPe7, e5);
      if(!candidate.from) candidate = tryMove(bPd7, d5);

    } else if(wA4) {
      // 1.a4 — e5
      candidate = tryMove(bPe7, e5);
      if(!candidate.from) candidate = tryMove(bPd7, d5);

    } else if(wNf3) {
      // 1.Nf3 — knight on f3 attacks e5 and d4.
      // Knight-opening rule: push the center pawn adjacent to the attacked
      // central square. Nf3 attacks d4 → the d-pawn (d5) is adjacent to d4.
      // d5 contests the square the knight controls and is the principled reply.
      // v0: d5 (solid QP reply, contests d4), v1: e5 (occupy attacked e5),
      // v2: Nf6 mirror, v3: c5 Sicilian-style.
      if(v == 0) {
        candidate = tryMove(bPd7, d5);        // 1...d5 — principled QP reply
        if(!candidate.from) candidate = tryMove(bNg8, nf6);
      } else if(v == 1) {
        candidate = tryMove(bPe7, e5);        // 1...e5 — occupy the attacked square
        if(!candidate.from) candidate = tryMove(bPd7, d5);
      } else if(v == 2) {
        candidate = tryMove(bNg8, nf6);       // 1...Nf6 — mirror development
        if(!candidate.from) candidate = tryMove(bPd7, d5);
      } else {
        candidate = tryMove(bPc7, c5);        // 1...c5 — Sicilian-style flank
        if(!candidate.from) candidate = tryMove(bPd7, d5);
      }

    } else if(wNc3) {
      // 1.Nc3 — knight on c3 attacks d5 and e4.
      // The center pawn NEXT TO the square the knight attacks is the d-pawn
      // (knight attacks d5 → push d5 to occupy/contest that square directly).
      // All variations push d5 as first priority; the knight opening rule is:
      // push the center pawn adjacent to the attacked central square.
      // v0/v1: d5 (occupy attacked square), v2/v3: Nf6 develop then d5 fallback.
      if(v == 0 || v == 1) {
        candidate = tryMove(bPd7, d5);        // 1...d5 — correct: occupy the attacked square
        if(!candidate.from) candidate = tryMove(bNg8, nf6);
      } else {
        candidate = tryMove(bNg8, nf6);       // 1...Nf6 — develop
        if(!candidate.from) candidate = tryMove(bPd7, d5);
      }

    } else if(wNh3) {
      // 1.Nh3 — useless flank knight, attacks g5 and f4. Neither is central.
      // No central square is under threat — take the centre freely.
      // d5 is completely safe (Nh3 doesn't touch it at all).
      candidate = tryMove(bPd7, d5);
      if(!candidate.from) candidate = tryMove(bPe7, e5);

    } else if(wNa3) {
      // 1.Na3 — useless flank knight, attacks b5 and c4. Neither is central.
      // Just claim the centre — e5 or d5, both completely safe.
      if(v <= 1) candidate = tryMove(bPe7, e5);
      else       candidate = tryMove(bPd7, d5);
      if(!candidate.from) candidate = tryMove(bNg8, nf6);

    } else if(wG3P) {
      // 1.g3 fianchetto — d5 or e5, both claim centre before bishop appears
      if(v <= 1) candidate = tryMove(bPd7, d5);
      else       candidate = tryMove(bPe7, e5);
      if(!candidate.from) candidate = tryMove(bNg8, nf6);

    } else if(wB3P) {
      // 1.b3 Larsen's — e5, take the centre
      candidate = tryMove(bPe7, e5);
      if(!candidate.from) candidate = tryMove(bPd7, d5);

    } else if(wF3P) {
      // 1.f3 — e5, punish the weakening immediately
      candidate = tryMove(bPe7, e5);
      if(!candidate.from) candidate = tryMove(bPd7, d5);

    } else if(wE3P) {
      // 1.e3 — d5
      candidate = tryMove(bPd7, d5);
      if(!candidate.from) candidate = tryMove(bPe7, e5);

    } else if(wD3P) {
      // 1.d3 — e5
      candidate = tryMove(bPe7, e5);
      if(!candidate.from) candidate = tryMove(bPd7, d5);

    } else {
      // Anything else (Na3, Nh3, h3, a3, b3 edge case, etc.) — e5 or d5
      if(v <= 1) candidate = tryMove(bPe7, e5);
      else       candidate = tryMove(bPd7, d5);
      if(!candidate.from) candidate = tryMove(bNg8, nf6);
    }

  // ═══════════════════════════════════════════════════════════════════════════
  // MOVE 2
  // ═══════════════════════════════════════════════════════════════════════════
  } else if(blackMoveNum == 2) {

    // Helper: try knight development, preferring the square that isn't a kick trap.
    // If BOTH squares are kick traps, play a pawn move instead.
    auto safeKnight = [&](int sq1, int from1, int sq2, int from2, int pawnSq, int pawnFrom) -> Move {
      Move m = none;
      if(!isKickTrap2(sq1)) m = tryMove(from1, sq1);
      if(!m.from && !isKickTrap2(sq2)) m = tryMove(from2, sq2);
      if(!m.from) m = tryMove(pawnFrom, pawnSq);  // pawn fallback — never a kick trap
      return m;
    };

    if(wE4 || wE5) {
      if(bE5) {
        // 1.e4 e5 — develop Nc6 or Nf6, not to kick-trapped squares
        candidate = safeKnight(nc6,bNb8, nf6,bNg8, d6,bPd7);
      } else if(bC5) {
        // Sicilian — Nc6 or d6
        candidate = safeKnight(nc6,bNb8, nf6,bNg8, d6,bPd7);
      } else if(bD5) {
        // Scandinavian — Nf6 or e6
        candidate = safeKnight(nf6,bNg8, nc6,bNb8, e6,bPe7);
      } else {
        // Some other black first move after e4 — safe Nf6
        candidate = safeKnight(nf6,bNg8, nc6,bNb8, d6,bPd7);
      }

    } else if(wD4 || wD5) {
      if(bD5) {
        // QGD style — Nf6 or e6
        candidate = safeKnight(nf6,bNg8, nc6,bNb8, e6,bPe7);
      } else if(bF5) {
        // Dutch — Nf6 or e6
        candidate = safeKnight(nf6,bNg8, nc6,bNb8, e6,bPe7);
      } else {
        // KID setup — Nf6 then fianchetto later
        candidate = safeKnight(nf6,bNg8, nc6,bNb8, d5,bPd7);
      }

    } else if(wC4) {
      candidate = safeKnight(nc6,bNb8, nf6,bNg8, e6,bPe7);

    } else if(wF4) {
      // After 1.f4 d5 — Nf6 or g6 (prepare fianchetto)
      candidate = safeKnight(nf6,bNg8, nc6,bNb8, g6,bPg7);

    } else if(wG4) {
      // After 1.g4 — Nf6, develop fast
      candidate = safeKnight(nf6,bNg8, nc6,bNb8, e5,bPe7);

    } else if(wNf3) {
      // After 1.Nf3 — develop a knight safely, supporting whatever pawn we played
      candidate = safeKnight(nf6,bNg8, nc6,bNb8, d5,bPd7);

    } else if(wNc3) {
      // After 1.Nc3 — if we played d5, support with Nf6; if e5, play Nc6 mirror
      if(bD5) candidate = safeKnight(nf6,bNg8, nc6,bNb8, e6,bPe7);
      else    candidate = safeKnight(nc6,bNb8, nf6,bNg8, d5,bPd7);

    } else if(wNh3 || wNa3) {
      // After useless flank knight — develop centrally, Nf6 is always safe
      candidate = safeKnight(nf6,bNg8, nc6,bNb8, d5,bPd7);

    } else if(wG3P || wB3P) {
      // Flank pawn openings — develop Nf6 toward centre
      candidate = safeKnight(nf6,bNg8, nc6,bNb8, d5,bPd7);

    } else {
      // Universal fallback — safe knight to best available square
      candidate = safeKnight(nf6,bNg8, nc6,bNb8, e5,bPe7);
    }

  // ═══════════════════════════════════════════════════════════════════════════
  // MOVE 3 — new: extend book by one more move
  // ═══════════════════════════════════════════════════════════════════════════
  } else if(blackMoveNum == 3) {

    // Count undeveloped black minors — tells us what's still needed
    int undev = 0;
    bool nG8home = (board[bNg8] == B_N);
    bool nB8home = (board[bNb8] == B_N);
    bool bF8home = (board[bBf8] == B_B);
    bool bC8home = (board[bBc8] == B_B);
    if(nG8home) undev++;
    if(nB8home) undev++;
    if(bF8home) undev++;
    if(bC8home) undev++;

    // Priority order for move 3:
    // 1. If both knights are still home, develop a knight (safe square only)
    // 2. If one knight is home, develop it
    // 3. If both knights are out, consider castling prep (e6/d6) or bishop dev

    if(nG8home && !isKickTrap2(nf6)) {
      candidate = tryMove(bNg8, nf6);
    }
    if(!candidate.from && nB8home && !isKickTrap2(nc6)) {
      candidate = tryMove(bNb8, nc6);
    }
    // Both knights developed or both kick traps — play structural pawn
    if(!candidate.from) {
      // Choose between e6, d6, Bc5-style development based on pawn structure
      if(bE5 && !bD6) {
        // After e5 — d6 solidifies the centre
        candidate = tryMove(bPd7, d6);
      } else if(bC5 && !bD6) {
        // Sicilian — d6
        candidate = tryMove(bPd7, d6);
      } else if(bD5 && !bE6) {
        // After d5 — e6 supports
        candidate = tryMove(bPe7, e6);
      } else if(!bE6 && !bE5) {
        candidate = tryMove(bPe7, e6);
      } else {
        // Last resort — if everything else fails, develop bishop
        // Only to a safe non-forward square
        if(bF8home && !isKickTrap2(sqOf(6,4))) {
          candidate = tryMove(bBf8, sqOf(6,4)); // Be7
        }
        if(!candidate.from && bC8home) {
          candidate = tryMove(bBc8, sqOf(5,3)); // Bd7 — solid, no aggression
        }
      }
    }
  }

  return candidate;
}

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
    searchTask, "chess_search", 32768, nullptr, 1, nullptr, 0
  );

  int lastDrawnPct   = -1;
  int lastDrawnDepth = -1;
  while(!searchDone) {
    int p = (int)searchPct;
    int d = (int)searchDepth;

    // Depth label ABOVE the progress bar (y=3..10, same x as bar)
    // Erase and redraw only when depth changes
    if(d != lastDrawnDepth) {
      tft.fillRect(PB_X-1, 2, PB_IW+4, 9, C_BG);
      if(d > 0) {
        tft.setTextSize(1);
        tft.setTextColor(C_HOVER);
        // "D9" = 2 chars = 12px — centre in bar width (PB_IW+2 = 14px)
        tft.setCursor(PB_X + (PB_IW+2 - 12)/2, 3);
        tft.print('D');
        tft.print(d);
      }
      lastDrawnDepth = d;
    }

    if(p != lastDrawnPct) {
      drawProgressBar(p);
      lastDrawnPct = p;
    }
    vTaskDelay(10);
    yield();
  }
  // Final: draw at 99, erase depth label, clean label zone, draw eval bar
  drawProgressBar(99);
  tft.fillRect(PB_X-1, 2, PB_IW+4, 9, C_BG);
  tft.fillRect(PB_X-3, PB_Y+PB_IH+3, PB_IW+7, 9, C_BG);
  drawEvalBar((int)evalScore);
  return searchResult;
}

// ─── Bot move: always depth-9 iterative deepening + shallow-depth fallback ────
//
// DESIGN
// ──────
// 1. Opening book is checked first (moves 1-2 for black). If a book move is
//    available, it is returned immediately — no search needed.
//
// 2. Iterative deepening from depth 1 to SEARCH_DEPTH (9).
//    At each depth d we record:
//      depthBestMove[d]  — the best root move found at that depth
//      depthBestScore[d] — its score (from black's perspective, negamax-style)
//
// 3. After depth 9 completes, the SHALLOW-DEPTH FALLBACK is applied:
//    If the depth-9 score is not significantly better than the score at some
//    shallower depth, and a shallower depth had a better-scoring move, then
//    play that shallower move instead.
//
//    Rationale: a deep search can sometimes be fooled by horizon effects at
//    exactly ply 9 (e.g. a piece hanging just beyond the horizon). Using the
//    best score across all depths avoids this without giving up deep analysis
//    — we always search to 9 first, and only fall back if the shallower
//    result is strictly better.
//
//    Threshold: FALLBACK_MARGIN centipawns. If the best score seen at depths
//    1-8 exceeds the depth-9 score by more than this margin, the shallowest
//    depth that achieved that peak score is used instead.
//    A move from depth d_shallow is only preferred if:
//      peakScore - depth9Score > FALLBACK_MARGIN
//
// 4. Opening system integration: the opening book produces legal, verified
//    moves. The search (with the full evaluation + pruning + PSTs) takes over
//    from move 3 onwards. Both systems share the same board representation
//    and evaluation, so there is no inconsistency.
//
// 5. Anti-sacrifice trap: LMR always re-searches at full depth when the
//    reduced score beats alpha. Quiescence search runs after depth 9 in all
//    branches. Together these ensure a 4-ply sacrifice (or any tactical
//    sequence) cannot be missed regardless of pruning.

#define SEARCH_DEPTH     9     // maximum depth — time limit governs which depths complete
#define FALLBACK_MARGIN  15    // centipawns: prefer shallower if it scores this much better

Move getBotMove() {
  // ── Opening book (moves 1-3) ─────────────────────────────────────────────
  // The book produces principled development moves, but it has NO tactical
  // awareness. Before accepting a book move we run a lightweight safety check:
  //
  //   1. The candidate move itself must not walk into a losing capture (SEE < 0
  //      on the destination square after the move).
  //   2. No currently undefended black piece must be under immediate attack
  //      by a white piece — if so, we must find a defensive move ourselves
  //      rather than blindly developing.
  //   3. No white piece must be en-prise (hanging) that the book would ignore
  //      — if there is a winning capture available (SEE > 0) the book is
  //      bypassed so the search can take it.
  //
  // If any of these conditions fire, openingMoveIndex is advanced past 3 so
  // the book is never consulted again and full search handles everything.
  if(openingMoveIndex < 3) {
    // Condition 3: is there a winning capture available right now?
    bool winningCaptureAvailable = false;
    {
      Move capMoves[128]; int capCnt = 0;
      generateMoves(false, capMoves, capCnt, false);
      for(int i = 0; i < capCnt && !winningCaptureAvailable; i++) {
        if(!board[capMoves[i].to]) continue; // not a capture
        if(!isLegal(capMoves[i])) continue;
        if(seeMoveGain(capMoves[i].from, capMoves[i].to, board[capMoves[i].from], 2) > 0)
          winningCaptureAvailable = true;
      }
    }

    // Condition 2: is any undefended black piece currently under attack?
    bool blackPieceHanging = false;
    if(!winningCaptureAvailable) {
      for(int sq = 0; sq < 64 && !blackPieceHanging; sq++) {
        int p = board[sq];
        if(!p || !IS_BLACK(p) || TYPE(p) == KING) continue;
        // Is this square attacked by white?
        if(seeCaptures(sq, 1) > 0) blackPieceHanging = true;
      }
    }

    if(!winningCaptureAvailable && !blackPieceHanging) {
      Move bm = getBookMove(openingMoveIndex + 1);
      if(bm.from || bm.to) {
        // Condition 1: does the book move itself walk into a losing capture?
        int piece = board[bm.from];
        int seeVal = seeMoveGain(bm.from, bm.to, piece, 2);
        bool safe = (seeVal >= 0);  // >= 0: even exchanges are acceptable
        if(safe) {
          openingMoveIndex++;
          return bm;
        }
      }
    }
    // Any safety check failed — abandon book entirely, use full search
    openingMoveIndex = 3;
  }

  // ── Search initialisation ─────────────────────────────────────────────────
  memset(killers, 0, sizeof(killers));
  memset(histHeuristic, 0, sizeof(histHeuristic));
  searchAborted = false;
  nodeCount     = 0;
  abDepthLevel  = 0;
  qDepthLevel   = 0;
  searchStart   = millis();
  searchPct     = 0;
  searchDepth   = 0;

  // Per-depth tracking (index 0 unused; 1..SEARCH_DEPTH used)
  Move  depthBestMove [SEARCH_DEPTH+1];
  int   depthBestScore[SEARCH_DEPTH+1];
  memset(depthBestMove,  0, sizeof(depthBestMove));
  for(int d=0;d<=SEARCH_DEPTH;d++) depthBestScore[d] = -99999;

  Move* moves = rootMoveBuf;
  int count;
  generateMoves(false, moves, count, false);
  sortMoves(moves, count);

  // Fallback: guarantee we always have at least one legal move ready
  Move completedBestMove = {0,0,0,0};
  int  completedBestScore = -99999;
  for(int i=0;i<count;i++) {
    if(isLegal(moves[i])) { completedBestMove = moves[i]; break; }
  }

  // ── Iterative deepening: depth 1 → SEARCH_DEPTH (time-limited) ──────────
  for(int depth = 1; depth <= SEARCH_DEPTH; depth++) {
    if(searchAborted) break;
    searchDepth = depth;   // tell the UI which depth we are currently searching

    Move  iterBestMove  = {0,0,0,0};
    int   iterBestScore = -99999;

    // Aspiration windows (active from depth 3 onwards)
    int aspDelta = 50;
    int windowLo, windowHi;
    if(depth >= 3 && completedBestScore > -90000) {
      windowLo = completedBestScore - aspDelta;
      windowHi = completedBestScore + aspDelta;
    } else {
      windowLo = -99999;
      windowHi =  99999;
    }

    // Move the previous iteration's best move to front for better ordering
    if(completedBestMove.from || completedBestMove.to) {
      for(int i=0;i<count;i++) {
        if(moves[i].from==completedBestMove.from && moves[i].to==completedBestMove.to) {
          Move tmp=moves[0]; moves[0]=moves[i]; moves[i]=tmp;
          break;
        }
      }
    }

    // Aspiration re-search loop
    while(true) {
      iterBestMove  = {0,0,0,0};
      iterBestScore = -99999;
      int loopAlpha = windowLo;

      for(int i=0; i<count; i++) {
        if(!isLegal(moves[i])) continue;
        if(searchAborted) break;

        HistoryEntry he;
        makeMove(moves[i], &he);
        if(hashHistoryCount < MAX_HISTORY + AB_MAX_LEVELS + 3)
          hashHistory[hashHistoryCount++] = currentHash;
        else
          hashHistoryCount++;
        int score = -alphaBeta(depth-1, -windowHi, -loopAlpha, true, searchStart, -1);
        hashHistoryCount--;
        unmakeMove(he);

        if(searchAborted) break;

        if(score > iterBestScore) {
          iterBestScore = score;
          iterBestMove  = moves[i];
          if(score > loopAlpha) loopAlpha = score;
        }
      }

      if(searchAborted) break;

      // Widen window on fail-high / fail-low
      if(iterBestScore <= windowLo && depth >= 3) {
        windowLo = max(-99999, windowLo - aspDelta);
        aspDelta *= 2;
      } else if(iterBestScore >= windowHi && depth >= 3) {
        windowHi = min(99999, windowHi + aspDelta);
        aspDelta *= 2;
      } else {
        break;   // score within window — accept
      }
      if(aspDelta > 800) { windowLo = -99999; windowHi = 99999; }
    }

    // Record this depth's result
    if(!searchAborted && (iterBestMove.from || iterBestMove.to)) {
      depthBestMove [depth] = iterBestMove;
      depthBestScore[depth] = iterBestScore;
      completedBestMove  = iterBestMove;
      completedBestScore = iterBestScore;

      // Move best move to front of list for next iteration's ordering
      for(int i=0;i<count;i++) {
        if(moves[i].from==completedBestMove.from && moves[i].to==completedBestMove.to) {
          Move tmp=moves[0]; moves[0]=moves[i]; moves[i]=tmp;
          break;
        }
      }
    }

    // Early exit if forced mate found
    if(completedBestScore > 18000) break;
  }

  searchPct = 99;

  // ── Shallow-depth fallback selection ─────────────────────────────────────
  // After completing all depths, check if a shallower depth found a strictly
  // better-scored move than depth 9. If the peak shallower score exceeds the
  // depth-9 score by more than FALLBACK_MARGIN, we trust the shallower result.
  //
  // This handles the case where depth-9 encounters a horizon-effect artefact
  // (e.g. a position looks good just because the search ends right before an
  // opponent recapture). The engine noted every depth's best move; we now
  // pick the globally best one.
  //
  // We do NOT prefer a shallower move if its score is merely equal or slightly
  // better — the depth-9 result is inherently more reliable for close scores.
  //
  // Special case: if depth 9 was never completed (forced mate found earlier),
  // completedBestMove already holds the correct result.
  {
    int d9score = depthBestScore[SEARCH_DEPTH];
    int peakScore = d9score;
    int peakDepth = SEARCH_DEPTH;

    // Scan depths 1..SEARCH_DEPTH-1 for a better score
    for(int d = 1; d < SEARCH_DEPTH; d++) {
      if(!(depthBestMove[d].from || depthBestMove[d].to)) continue;
      if(depthBestScore[d] > peakScore) {
        peakScore = depthBestScore[d];
        peakDepth = d;
      }
    }

    // Only fall back if:
    //   (a) depth-9 actually completed (d9score != -99999)
    //   (b) the shallower score beats depth-9 by more than FALLBACK_MARGIN
    //   (c) the shallower move is valid
    if(d9score != -99999
       && peakDepth < SEARCH_DEPTH
       && peakScore - d9score > FALLBACK_MARGIN
       && (depthBestMove[peakDepth].from || depthBestMove[peakDepth].to))
    {
      completedBestMove  = depthBestMove[peakDepth];
      completedBestScore = peakScore;
    } else if(depthBestMove[SEARCH_DEPTH].from || depthBestMove[SEARCH_DEPTH].to) {
      // Depth-9 result is best or equivalent — use it
      completedBestMove  = depthBestMove[SEARCH_DEPTH];
      completedBestScore = d9score;
    }
    // else: completedBestMove already holds the last completed depth's result
    //       (set during iterative deepening above)
  }

  // Publish final eval score for the eval bar.
  // completedBestScore is negamax from black's POV (positive = good for black).
  // Convert to white-relative: white-relative = -completedBestScore.
  // Clamp mate scores so they don't overflow the bar mapping.
  {
    int ws = -completedBestScore;
    if(ws >  19000) ws =  800;
    if(ws < -19000) ws = -800;
    evalScore = ws;
  }

  return completedBestMove;
}
inline int sqX(int sq) { return BOARD_OX + fileOf(sq)*SQ_W; }
inline int sqY(int sq) { return BOARD_OY + (7-rankOf(sq))*SQ_H; }
inline uint16_t sqColor(int sq) {
  return ((rankOf(sq)+fileOf(sq))%2==0) ? C_DARK_SQ : C_LIGHT_SQ;
}

void drawPieceRainbowBorder(int sx, int sy, int sq) {
  // Use 4 theme-derived shades cycling by diagonal position
  uint16_t spec[4] = { C_HOVER, C_BTN_HL, C_SEL, C_BORDER };
  int diag = ((sq/8) + (sq%8)) % 4;
  uint16_t cTop   = spec[diag];
  uint16_t cRight = spec[(diag+1) % 4];
  uint16_t cBot   = spec[(diag+2) % 4];
  uint16_t cLeft  = spec[(diag+3) % 4];
  tft.drawFastHLine(sx+1,      sy+1,      SQ_W-2, cTop);
  tft.drawFastHLine(sx+1,      sy+SQ_H-2, SQ_W-2, cBot);
  tft.drawFastVLine(sx+1,      sy+1,      SQ_H-2, cLeft);
  tft.drawFastVLine(sx+SQ_W-2, sy+1,      SQ_H-2, cRight);
}

void drawPieceAt(int sx, int sy, int piece) {
  if(IS_WHITE(piece)) {
    tft.fillRect(sx+2, sy+2, SQ_W-4, SQ_H-4, C_W_FILL);
    tft.setTextColor(C_W_TXT);
  } else {
    tft.fillRect(sx+2, sy+2, SQ_W-4, SQ_H-4, C_B_FILL);
    tft.setTextColor(C_B_TXT);
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
  // Gradient border using theme shades
  const uint16_t brc[8] = {
    C_BTN_BG, C_BTN_BG, C_BORDER, C_BTN_HL,
    C_HOVER,  C_SEL,    C_BTN_HL, C_HOVER
  };
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
  tft.drawRect(BOARD_OX-1, BOARD_OY-1, 8*SQ_W+2, 8*SQ_H+2, C_BTN_HL);

  for(int sq=0;sq<64;sq++) drawSquare(sq);

  tft.fillRect(14, 4, 7, 104, C_BG);
  for(int r=0;r<8;r++) {
    tft.setTextColor(C_LABEL);
    tft.setTextSize(1);
    tft.setCursor(15, BOARD_OY + (7-r)*SQ_H + 3);
    tft.print((char)('1'+r));
  }
  tft.fillRect(22, 112, 106, 10, C_BG);
  for(int f=0;f<8;f++) {
    tft.setTextColor(C_LABEL);
    tft.setTextSize(1);
    tft.setCursor(BOARD_OX + f*SQ_W + 4, 113);
    tft.print((char)('a'+f));
  }
}

void drawLastMoveOutlines(int from, int to) {
  if(from<0||to<0) return;
  // Two outlines, both strictly inside the square (no pixel bleed outside).
  // Outer rect: theme cursor colour (bright accent, clearly visible).
  // Inner rect (1px inset): theme bg colour — creates a crisp "gap" that makes
  // the accent ring pop without looking harsh.
  auto markSq = [](int sq) {
    int sx = sqX(sq), sy = sqY(sq);
    uint16_t sqBg = ((rankOf(sq)+fileOf(sq))%2==0) ? C_DARK_SQ : C_LIGHT_SQ;
    tft.drawRect(sx,   sy,   SQ_W,   SQ_H,   C_HOVER);   // outer accent
    tft.drawRect(sx+1, sy+1, SQ_W-2, SQ_H-2, sqBg);      // inner gap (erase inner)
  };
  markSq(from);
  markSq(to);
}

void drawLegalDots() {
  for(int i=0;i<legalCount;i++) {
    int sq = legalCache[i].to;
    int sx = sqX(sq)+SQ_W/2;
    int sy = sqY(sq)+SQ_H/2;
    tft.fillCircle(sx, sy, 3, C_BTN_BG);
    tft.fillCircle(sx, sy, 2, C_DOT);
  }
}

void clearLegalDots() {
  for(int i=0;i<legalCount;i++) drawSquare(legalCache[i].to);
}

void drawCursorAt(int col, int row) {
  if(col < 0) return;
  int sq = sqOf(row, col);
  int sx = sqX(sq), sy = sqY(sq);

  // Determine outer cursor colour based on context
  uint16_t outerC;
  if(selSq == sq)                                        outerC = C_BOTH;
  else if(engineFrom >= 0 && (sq==engineFrom||sq==engineTo)) outerC = C_CURSOR_LASTMOVE;
  else                                                   outerC = C_HOVER;

  // Inner gap colour — always the square's background so the gap is invisible
  // against the square but creates a crisp separation ring that makes the
  // outer outline pop on any theme, light or dark.
  uint16_t sqBg = ((rankOf(sq)+fileOf(sq))%2==0) ? C_DARK_SQ : C_LIGHT_SQ;

  // Double-outline: outer rect in accent colour, inner rect in sqBg (gap ring)
  tft.drawRect(sx,   sy,   SQ_W,   SQ_H,   outerC);
  tft.drawRect(sx+1, sy+1, SQ_W-2, SQ_H-2, sqBg);

  // Re-apply sel outline on top if this square is selected (sel draws over gap)
  if(selSq == sq)
    tft.drawRect(sx+1, sy+1, SQ_W-2, SQ_H-2, C_SEL);
}

void redrawSquareWithArrow(int sq, int cursorSq=-1) {
  drawSquare(sq);
  if(engineFrom >= 0 && engineTo >= 0) {
    if(sq == engineFrom || sq == engineTo) {
      int sx = sqX(sq), sy = sqY(sq);
      uint16_t sqBg = ((rankOf(sq)+fileOf(sq))%2==0) ? C_DARK_SQ : C_LIGHT_SQ;
      tft.drawRect(sx,   sy,   SQ_W,   SQ_H,   C_HOVER);
      tft.drawRect(sx+1, sy+1, SQ_W-2, SQ_H-2, sqBg);
    }
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

// ─── UI-3: Draw red flag icon for reset button ────────────────────────────────
// Draws a pixel-art flag: a vertical pole with a red triangular flag on top
// x,y = top-left corner of the icon area (fits in 13x34 panel space)
void drawFlagIcon(int bx, int by) {
  // Button face already filled. Button is 10px wide (bx..bx+9).
  // Pole at bx+1 (shifted 2px left), flag triangle right of pole.
  int poleX   = bx + 1;
  int poleTop = by + 3;
  int poleBot = by + 26;
  tft.drawFastVLine(poleX, poleTop, poleBot - poleTop + 1, C_BORDER);
  int flagH = 7;  // 7-row triangle fits cleanly in 10px button
  for(int row = 0; row < flagH; row++) {
    int rowY = poleTop + row;
    int w    = flagH - row;
    for(int p = 0; p < w; p++) {
      int fx = poleX + 1 + p;
      if(fx < bx + 8) {
        uint16_t c = (row == 0) ? 0xFBA0 : (p == w-1) ? 0x8000 : 0xD000;
        tft.drawPixel(fx, rowY, c);
      }
    }
  }
}

void drawLeftPanel() {
  // ── Panel background strip — 13px wide, runs full height ─────────────────
  tft.fillRect(0, 0, 13, 128, C_PANEL_BG);

  // Panel layout (squeezed to fit 4 buttons + phase indicator):
  //   Flag       y  6..31  (26px button)
  //   UND        y 36..61  (26px button)
  //   Theme      y 66..78  (13px button)
  //   Sens (+)   y 82..94  (13px button)
  //   Phase      y 99..108 (indicator)

  // ── Flag (resign) button — y 6..31 ───────────────────────────────────────
  tft.fillRoundRect(2, 6, 10, 26, 2, C_BTN_FACE);
  drawFlagIcon(2, 6);
  if(panelFocus==1) tft.drawRect(1, 5, 11, 28, C_HOVER);

  // ── UND (undo) button — y 36..61 ─────────────────────────────────────────
  tft.fillRoundRect(2, 36, 10, 26, 2, C_BTN_FACE);
  if(panelFocus==2) tft.drawRect(1, 35, 11, 28, C_HOVER);
  tft.setTextSize(1);
  tft.setTextColor(C_BTN_HL);      tft.setCursor(4, 39); tft.print('U');
  tft.setTextColor(C_BORDER);      tft.setCursor(4, 47); tft.print('N');
  tft.setTextColor(C_SIDE_ACCENT); tft.setCursor(4, 55); tft.print('D');

  // ── Palette (theme) button — y 66..78 ────────────────────────────────────
  tft.fillRoundRect(2, 66, 10, 13, 2, C_BTN_FACE);
  if(panelFocus==3) tft.drawRect(1, 65, 11, 15, C_HOVER);
  tft.fillRect(3, 69, 4, 7, C_DARK_SQ);
  tft.fillRect(7, 69, 4, 7, C_LIGHT_SQ);

  // ── Sensitivity (+) button — y 82..94 ────────────────────────────────────
  tft.fillRoundRect(2, 82, 10, 13, 2, C_BTN_FACE);
  if(panelFocus==4) tft.drawRect(1, 81, 11, 15, C_HOVER);
  tft.setTextSize(1);
  tft.setTextColor(C_BTN_HL);
  tft.setCursor(4, 85); tft.print('+');

  // ── Phase indicator — y 99..108 ──────────────────────────────────────────
  {
    int ph = gamePhase();
    char letter = (ph==0) ? 'O' : (ph==1) ? 'M' : 'E';
    uint16_t col = (ph==0) ? C_SIDE_ACCENT : (ph==1) ? C_BTN_HL : C_GAMEOVER;
    tft.fillRect(0, 98, 13, 10, C_PANEL_BG);
    tft.setTextSize(1);
    tft.setTextColor(col);
    tft.setCursor(4, 99);
    tft.print(letter);
  }
}

void drawProgressBar(int pct) {
  // Cap at 99 — bar never shows 100
  if(pct > 99) pct = 99;
  if(pct == dispPct) return;
  int oldPct = dispPct;
  dispPct = pct;

  // Erase the entire right-side gutter including any stale pixels from old code
  // (covers PB_X-3 to PB_X+PB_IW+3, y=PB_Y to PB_Y+PB_IH+12)
  // This eliminates the orange rectangle and any orange dots definitively.
  if(pct == 0 || oldPct < 0) {
    tft.fillRect(PB_X-3, PB_Y-1, PB_IW+7, PB_IH+12, C_BG);
    tft.drawRect(PB_X, PB_Y, PB_IW+2, PB_IH+2, C_BORDER);
    tft.fillRect(PB_X+1, PB_Y+1, PB_IW, PB_IH, C_PB_EMPTY);
    return;
  }

  int oldH = (PB_IH * max(0, oldPct)) / 100;
  int newH = (PB_IH * pct)            / 100;

  if(newH > oldH) {
    int yStart = PB_Y + 1 + PB_IH - newH;
    int yEnd   = PB_Y + 1 + PB_IH - oldH - 1;
    if(yStart <= yEnd)
      tft.fillRect(PB_X+1, yStart, PB_IW, yEnd - yStart + 1, C_HOVER);
  } else if(newH < oldH) {
    int yStart = PB_Y + 1 + PB_IH - oldH;
    int yEnd   = PB_Y + 1 + PB_IH - newH - 1;
    if(yStart <= yEnd)
      tft.fillRect(PB_X+1, yStart, PB_IW, yEnd - yStart + 1, C_PB_EMPTY);
  }

  // % label — erase full label zone then print centred, no leading zero
  tft.fillRect(PB_X-3, PB_Y+PB_IH+3, PB_IW+7, 9, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_HOVER);
  // "99%" = 3 chars = 18px, "9%" = 2 chars = 12px
  int labelW = (pct >= 10) ? 18 : 12;
  int labelX = PB_X + (PB_IW+2 - labelW) / 2;
  tft.setCursor(labelX, PB_Y+PB_IH+4);
  tft.print(pct);
  tft.print('%');
  // Depth is drawn above the bar by runSearchOnCore0 — NOT here
}

// ─── Evaluation bar ───────────────────────────────────────────────────────────
// Two-colour bar: top = black portion (C_B_FILL), bottom = white portion (C_W_FILL).
// Logarithmic scaling: 0cp=50/50, queen sac≈70/30, dominant≈88/12, mate=100/0.
void drawEvalBar(int evalCp) {
  // Logarithmic split. splitH = pixels from top for black region.
  // Clamp: always show at least 1px of each colour unless it's a mate line.
  bool isMate = (evalCp >= 19000 || evalCp <= -19000);
  const float LOG_DENOM = 1.9459f; // log(7)
  float absCp = (float)(evalCp < 0 ? -evalCp : evalCp);
  if(absCp > 19000.0f) absCp = 19000.0f;
  float logVal = logf(1.0f + absCp / 150.0f) / LOG_DENOM;
  if(logVal > 1.0f) logVal = 1.0f;
  int half   = EB_IH / 2;
  int splitH;
  if(evalCp >= 0)
    splitH = half - (int)(half * logVal + 0.5f);
  else
    splitH = half + (int)(half * logVal + 0.5f);
  // Issue 5: always keep at least 1px of each colour unless it is a true mate
  if(!isMate) {
    if(splitH < 1)       splitH = 1;
    if(splitH > EB_IH-1) splitH = EB_IH-1;
  } else {
    if(splitH < 0)     splitH = 0;
    if(splitH > EB_IH) splitH = EB_IH;
  }

  // Border + fill
  tft.drawRect(EB_X, EB_Y, EB_IW+2, EB_IH+2, C_BORDER);
  if(splitH > 0)
    tft.fillRect(EB_X+1, EB_Y+1,        EB_IW, splitH,        C_B_FILL);
  if(splitH < EB_IH)
    tft.fillRect(EB_X+1, EB_Y+1+splitH, EB_IW, EB_IH-splitH, C_W_FILL);

  // ── Eval number overlays ─────────────────────────────────────────────────
  // Build digit arrays for integer and fractional parts.
  // The decimal point is drawn manually as a 2x2 gray pixel block so it
  // touches both neighbours with zero gap, staying horizontally centred.
  // Layout: [whole digits | dot 2px | tenths digit], all flush.
  tft.setTextSize(1);
  int absVal = evalCp < 0 ? -evalCp : evalCp;
  if(absVal > 9999) absVal = 9999;
  int pawns  = absVal / 100;
  int tenths = (absVal % 100) / 10;

  // whole-part string (1 or 2 chars)
  char wBuf[3]; int wLen = 0;
  if(pawns >= 10) { wBuf[wLen++] = '0' + pawns/10; wBuf[wLen++] = '0' + pawns%10; }
  else              wBuf[wLen++] = '0' + pawns;
  wBuf[wLen] = 0;
  char tBuf[2] = { (char)('0' + tenths), 0 };

  // Total pixel width: wLen*6 + 2 (dot) + 6 (tenths digit)
  int totalPx = wLen * 6 + 2 + 6;
  int textX   = EB_X - 1 + (EB_IW + 2 - totalPx) / 2;
  if(textX < 0) textX = 0;
  if(textX + totalPx > 160) textX = 160 - totalPx;

  int dotX    = textX + wLen * 6;   // dot starts right after whole digits
  int tenthsX = dotX + 2;           // tenths digit starts right after dot

  // Helper lambda: draw the three components in a given text + dot colour
  // dotCol is always gray; textCol varies by region
  auto drawEvalNumber = [&](int ty, uint16_t textCol) {
    // Whole digits
    tft.setTextColor(textCol);
    tft.setCursor(textX, ty);
    tft.print(wBuf);
    // Decimal point: 2x2 block at font baseline (font is 7px tall, dot at row 5)
    uint16_t gray = 0x8410; // mid-gray in RGB565
    int dotY = ty + 5; // baseline dot position
    tft.fillRect(dotX, dotY, 2, 2, gray);
    // Tenths digit
    tft.setTextColor(textCol);
    tft.setCursor(tenthsX, ty);
    tft.print(tBuf);
  };

  // Black region (top): white text near top
  if(splitH >= 9) {
    drawEvalNumber(EB_Y + 2, 0xFFFF);
  }
  // White region (bottom): black text near bottom
  if(EB_IH - splitH >= 9) {
    int ty = EB_Y + 1 + splitH + (EB_IH - splitH) - 9;
    drawEvalNumber(ty, 0x0000);
  }

  // ── +/- signs at split line ───────────────────────────────────────────────
  if(splitH > 5 && splitH < EB_IH - 5) {
    int splitY = EB_Y + 1 + splitH;
    if(evalCp >= 0) {
      tft.setTextColor(0x0000); tft.setCursor(EB_X+1, splitY+1); tft.print('+');
      tft.setTextColor(0xFFFF); tft.setCursor(EB_X+1, splitY-9); tft.print('-');
    } else {
      tft.setTextColor(0xFFFF); tft.setCursor(EB_X+1, splitY-9); tft.print('+');
      tft.setTextColor(0x0000); tft.setCursor(EB_X+1, splitY+1); tft.print('-');
    }
  }
}

// ─── Phase indicator is drawn inside drawLeftPanel (below palette button) ────
void drawGameOver() {
  tft.fillRect(22, 119, 104, 9, C_BG);
  tft.setTextSize(1);
  int len = strlen(gameOverMsg);
  int tw  = len * 6;
  int tx  = 22 + (104 - tw) / 2;
  for(int i=0; i<len; i++) {
    tft.setTextColor((i % 2 == 0) ? C_GAMEOVER : C_BTN_HL);
    tft.setCursor(tx + i*6, 120);
    tft.print(gameOverMsg[i]);
  }
}

void drawAll() {
  tft.fillScreen(C_BG);
  drawBoard();
  drawLeftPanel();        // panel bg, buttons, phase indicator
  drawProgressBar(0);
  drawEvalBar(evalScore);
  if(engineFrom>=0) drawLastMoveOutlines(engineFrom, engineTo);
  if(selSq>=0) {
    tft.drawRect(sqX(selSq), sqY(selSq), SQ_W, SQ_H, C_SEL);
    drawLegalDots();
  }
  if(cursorCol>=0) drawCursorAt(cursorCol, cursorRow);
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

// Draw a single keypad key using theme colours
void drawKeypadKey(int col, int row, bool selected) {
  int kx, ky, kw;
  if(row == 4) {
    kx = KP_X; ky = KP_Y + 4*(KP_KH+KP_GAP); kw = 3*KP_KW+2*KP_GAP;
    if(col != 0) return;
  } else {
    kx = KP_X + col*(KP_KW+KP_GAP); ky = KP_Y + row*(KP_KH+KP_GAP); kw = KP_KW;
  }
  char label = kpLabels[row][col];
  uint16_t bgCol, borderCol, textCol;
  if(row == 4) {
    bgCol     = selected ? C_K_SEL     : C_K_CONFIRM;
    borderCol = selected ? C_K_SEL_BDR : C_K_KEY_BDR;
    textCol   = selected ? 0xFFFF      : C_K_KEY_TXT;
  } else if(label == 'B') {
    bgCol     = selected ? C_K_SEL     : C_K_KEY;
    borderCol = selected ? C_K_SEL_BDR : C_K_KEY_BDR;
    textCol   = selected ? 0xFFFF      : C_K_KEY_TXT;
  } else if(label == '<') {
    // Delete key — always red (danger), regardless of theme
    bgCol     = selected ? 0x2800 : 0x1000;
    borderCol = selected ? 0xF800 : 0x6000;
    textCol   = selected ? 0xFFFF : 0xF800;
  } else {
    bgCol     = selected ? C_K_SEL     : C_K_KEY;
    borderCol = selected ? C_K_SEL_BDR : C_K_KEY_BDR;
    textCol   = selected ? 0xFFFF      : C_K_KEY_TXT;
  }
  tft.fillRoundRect(kx, ky, kw, KP_KH, 2, bgCol);
  tft.drawRoundRect(kx, ky, kw, KP_KH, 2, borderCol);
  if(!selected) tft.drawFastHLine(kx+2, ky+1, kw-4, C_K_HDR);
  tft.setTextSize(1); tft.setTextColor(textCol);
  if(row == 4) {
    const char* ok = "CONFIRM"; int tl = strlen(ok);
    tft.setCursor(kx+(kw-tl*6)/2, ky+(KP_KH-8)/2); tft.print(ok);
  } else if(label == 'B') {
    tft.setCursor(kx+(kw-4*6)/2, ky+(KP_KH-8)/2); tft.print("BACK");
  } else if(label == '<') {
    tft.setCursor(kx+(kw-2*6)/2, ky+(KP_KH-8)/2); tft.print("<-");
  } else {
    tft.setCursor(kx+(kw-6)/2, ky+(KP_KH-8)/2); tft.print(label);
  }
}

// Redraw only the digit display row
void drawKeypadDigits() {
  tft.fillRect(0, KP_DIG_Y-4, 160, KP_DIG_H+10, C_K_BG);
  for(int i = 0; i < 6; i++) {
    int bx2 = KP_DIG_X + i*(KP_DIG_W+KP_DIG_GAP);
    tft.fillRoundRect(bx2, KP_DIG_Y, KP_DIG_W, KP_DIG_H, 2, C_K_KEY);
    uint16_t bdrC = (i == kpDigitCount) ? C_K_DIG_BDR : C_K_KEY_BDR;
    tft.drawRoundRect(bx2, KP_DIG_Y, KP_DIG_W, KP_DIG_H, 2, bdrC);
    if(i < kpDigitCount) {
      tft.setTextSize(1); tft.setTextColor(C_K_DIG_TXT);
      tft.setCursor(bx2+(KP_DIG_W-6)/2, KP_DIG_Y+(KP_DIG_H-8)/2);
      tft.print('*');
    } else if(i == kpDigitCount) {
      tft.drawFastHLine(bx2+6, KP_DIG_Y+KP_DIG_H-3, KP_DIG_W-12, C_K_DIG_BDR);
    }
  }
}

void drawKeypad(bool redrawAll) {
  if(redrawAll) {
    tft.fillScreen(C_K_BG);
    tft.fillRect(0, 0, 160, 28, C_K_HDR);
    // Gradient top edge
    { int sw = 160/6;
      uint16_t segs[6] = {C_K_KEY, C_K_KEY_BDR, C_K_HDR_TXT, C_HOVER, C_K_HDR_TXT, C_K_KEY_BDR};
      for(int i=0;i<6;i++) tft.drawFastHLine(i*sw, 0, (i==5)?160-i*sw:sw, segs[i]); }
    tft.drawFastHLine(0, 27, 160, C_K_KEY_BDR);
    // Padlock icon
    tft.drawFastVLine(5,  5, 5, C_K_HDR_TXT);
    tft.drawFastVLine(12, 5, 5, C_K_HDR_TXT);
    tft.drawFastHLine(5,  5, 8, C_K_HDR_TXT);
    tft.fillRoundRect(2, 10, 14, 12, 2, C_K_SEL);
    tft.drawRoundRect(2, 10, 14, 12, 2, C_K_HDR_TXT);
    tft.fillRect(7, 13, 4, 6, C_K_BG); tft.fillCircle(9, 13, 2, C_K_BG);
    tft.setTextSize(1);
    tft.setTextColor(C_K_HDR_TXT); tft.setCursor(22,  6); tft.print("ENTER PASSWORD");
    tft.setTextColor(C_K_KEY_BDR); tft.setCursor(22, 16); tft.print("6 digits required");
    tft.drawFastHLine(8, KP_DIG_Y-2, 144, C_K_KEY_BDR);
  }
  drawKeypadDigits();
  tft.drawFastHLine(4, KP_Y-2, 152, C_K_KEY_BDR);
  for(int r = 0; r < 5; r++)
    for(int c = 0; c < 3; c++) {
      if(r==4 && c!=0) continue;
      drawKeypadKey(c, r, (c==kpCurCol && r==kpCurRow));
    }
}

// "WRONG PASSWORD" flash — always red X (danger), theme bg
void drawWrongPassword() {
  tft.fillScreen(C_K_BG);
  tft.fillRoundRect(60, 20, 40, 40, 5, 0x8000);
  tft.drawRoundRect(60, 20, 40, 40, 5, 0xF800);
  tft.drawFastHLine(65, 60, 30, 0x6000);
  for(int i = 0; i < 20; i++) {
    tft.drawPixel(65+i,   28+i, 0xF800); tft.drawPixel(65+i+1, 28+i, 0xF800);
    tft.drawPixel(84-i,   28+i, 0xF800); tft.drawPixel(85-i,   28+i, 0xF800);
  }
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF); tft.setCursor(34, 70); tft.print("WRONG PASSWORD");
  tft.setTextColor(0xF800); tft.setCursor(46, 83); tft.print("Try again!");
  tft.fillRect(0, 115, 160, 13, C_K_HDR);
  tft.drawFastHLine(0, 115, 160, C_K_HDR_TXT);
  tft.setTextColor(C_K_HDR_TXT); tft.setCursor(22, 118); tft.print("Returning in 1s...");
}

// Show "UNDO OK" confirmation flash
void drawUndoOK() {
  tft.fillScreen(C_BG);
  tft.fillRoundRect(60, 20, 40, 40, 5, C_BTN_BG);
  tft.drawRoundRect(60, 20, 40, 40, 5, C_HOVER);
  tft.drawFastHLine(65, 60, 30, C_BTN_HL);
  // Checkmark strokes
  for(int i = 0; i < 8; i++) {
    tft.drawPixel(67+i, 44+i,   C_HOVER);
    tft.drawPixel(67+i+1, 44+i, C_HOVER);
  }
  for(int i = 0; i < 15; i++) {
    tft.drawPixel(74+i, 52-i,   C_HOVER);
    tft.drawPixel(74+i+1, 52-i, C_HOVER);
  }
  tft.setTextSize(1);
  tft.setTextColor(C_SEL);   tft.setCursor(52, 70); tft.print("UNDO OK!");
  tft.setTextColor(C_BTN_HL);tft.setCursor(28, 83); tft.print("Move undone.");
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
  tft.fillScreen(C_L_BG);
  for(int gy = 0; gy < 128; gy += 8)
    for(int gx = 0; gx < 160; gx += 8)
      if(((gx/8 + gy/8) & 1) == 0) tft.fillRect(gx, gy, 8, 8, C_L_GRID);

  tft.fillRect(0, 0, 160, 3, C_L_ACCENT);
  tft.fillRect(0, 3, 160, 1, C_L_CROWN);

  int cx = 80, cy = 28;
  tft.fillRect(cx-10, cy+4, 20, 6, C_L_CROWN);
  tft.fillRect(cx-10, cy,   3,  5, C_L_CROWN);
  tft.fillRect(cx-2,  cy-2, 5,  7, C_L_CROWN_HI);
  tft.fillRect(cx+7,  cy,   3,  5, C_L_CROWN);
  tft.fillRect(cx-2,  cy-6, 5,  5, C_L_CROWN_HI);
  tft.fillRect(cx-5,  cy-3, 11, 2, C_L_CROWN_HI);
  tft.drawFastHLine(cx-14, cy+10, 28, C_L_ACCENT2);
  tft.fillRect(cx-14, cy+11, 28, 2, C_L_BG);

  { tft.setTextSize(1);
    const char* t1 = "YOU RESIGNED";
    int l = strlen(t1), x0 = (160 - l*6) / 2;
    for(int i = 0; i < l; i++) {
      tft.setTextColor((i%2==0) ? C_L_TITLE : C_L_CROWN);
      tft.setCursor(x0 + i*6, 52); tft.print(t1[i]);
    }
  }
  tft.drawFastHLine(20, 63, 120, C_L_ACCENT2);
  tft.setTextColor(C_L_TITLE); tft.setTextSize(1);
  { const char* go = "GAME OVER"; int gl=strlen(go);
    tft.setCursor((160-gl*6)/2, 68); tft.print(go); }
  tft.setTextColor(C_L_SUB);
  { const char* yl = "You lost."; int yll=strlen(yl);
    tft.setCursor((160-yll*6)/2, 80); tft.print(yl); }

  tft.fillRect(0, 108, 160, 1, C_L_ACCENT2);
  tft.fillRect(0, 109, 160, 19, C_L_GRID);
  const char pieceChars[6] = {'K','Q','R','B','N','P'};
  for(int i = 0; i < 6; i++) {
    tft.setTextColor((i%2==0) ? C_L_TITLE : C_L_CROWN);
    tft.setCursor(14 + i*22, 111); tft.print(pieceChars[i]);
  }
  tft.drawFastHLine(0, 127, 160, C_L_ACCENT);

  while(digitalRead(JOY_SW) == LOW)  { yield(); delay(10); }
  while(digitalRead(JOY_SW) != LOW)  { yield(); delay(20); }
  while(digitalRead(JOY_SW) == LOW)  { yield(); delay(10); }
}


// ═══════════════════════════════════════════════════════════════════════════════
// CHECKMATE "YOU LOST" SCREEN
// ═══════════════════════════════════════════════════════════════════════════════
void showCheckmateScreen() {
  tft.fillScreen(C_L_BG);
  for(int gy = 0; gy < 128; gy += 8)
    for(int gx = 0; gx < 160; gx += 8)
      if(((gx/8 + gy/8) & 1) == 0) tft.fillRect(gx, gy, 8, 8, C_L_GRID);

  // Top accent — always red for checkmate (danger signal), theme accent on right
  tft.fillRect(0,  0, 80, 2, 0xD000);
  tft.fillRect(80, 0, 80, 2, C_L_ACCENT);
  tft.fillRect(0,  2, 160, 1, C_L_CROWN);
  tft.fillRect(0,  3, 160, 1, C_L_BG);

  // Fallen king — theme crown colours
  int cx = 80, cy = 28;
  tft.fillRect(cx-10, cy+4, 20, 6, C_L_CROWN);
  tft.fillRect(cx-10, cy,   3,  5, C_L_CROWN);
  tft.fillRect(cx-2,  cy-2, 5,  7, C_L_CROWN_HI);
  tft.fillRect(cx+7,  cy,   3,  5, C_L_CROWN);
  tft.fillRect(cx-2,  cy-6, 5,  5, C_L_CROWN_HI);
  tft.fillRect(cx-5,  cy-3, 11, 2, C_L_CROWN_HI);
  tft.drawFastHLine(cx-14, cy+10, 28, C_L_ACCENT2);
  tft.fillRect(cx-14, cy+11, 28, 2, C_L_BG);

  // "YOU LOST" — large, alternating theme title colours
  { tft.setTextSize(2);
    const char* t1 = "YOU LOST"; int tlen = strlen(t1);
    int tx0 = (160 - tlen*12)/2;
    for(int i = 0; i < tlen; i++) {
      tft.setTextColor((i%2==0) ? C_L_TITLE : C_L_CROWN_HI);
      tft.setCursor(tx0 + i*12, 48); tft.print(t1[i]);
    }
  }

  // Divider — left red, right theme accent
  tft.drawFastHLine(20, 68, 60, 0x8000);
  tft.drawFastHLine(80, 68, 60, C_L_ACCENT2);

  tft.setTextColor(C_L_SUB); tft.setTextSize(1);
  { const char* go = "CHECKMATE"; int gl=strlen(go);
    tft.setCursor((160-gl*6)/2, 73); tft.print(go); }
  tft.setTextColor(C_L_TITLE);
  { const char* bw = "Black wins."; int bwl=strlen(bw);
    tft.setCursor((160-bwl*6)/2, 85); tft.print(bw); }

  // Bottom bar — left red, right theme
  tft.fillRect(0,  108, 80, 1, 0x8000);
  tft.fillRect(80, 108, 80, 1, C_L_ACCENT2);
  tft.fillRect(0,  109, 80, 19, C_L_GRID);
  tft.fillRect(80, 109, 80, 19, C_L_GRID);
  const char pieceChars2[6] = {'K','Q','R','B','N','P'};
  for(int i = 0; i < 6; i++) {
    tft.setTextColor((i < 3) ? C_L_TITLE : C_L_CROWN);
    tft.setCursor(14 + i*22, 111); tft.print(pieceChars2[i]);
  }
  tft.drawFastHLine(0,  127, 80, 0xD000);
  tft.drawFastHLine(80, 127, 80, C_L_ACCENT);

  delay(1500);
}


// ═══════════════════════════════════════════════════════════════════════════════
// UI-4: INTRO CUTSCENE  — RageMate Maxima V1
//
// HACKER palette (hardcoded — runs before theme is applied to game board):
//   HK_BG      near-black green    C(0,1,0)
//   HK_DIM     very dark green     C(0,4,0)
//   HK_MID     dim terminal green  C(2,12,1)
//   HK_BRIGHT  phosphor green      C(6,32,3)
//   HK_HOT     peak phosphor       C(8,44,4)
//   HK_WHITE   white               0xFFFF
//
// Sequence:
//   1. Black → scan-line flood from top, drawing faint horizontal grid lines
//   2. "RageMate" materialises one character at a time with a flicker effect
//   3. Horizontal rule sweeps out from centre (double line)
//   4. MODEL slot machine: scrolls through name list, lands on "Maxima"
//   5. VERSION slot machine: scrolls digits 1-9, lands on "1"
//   6. Border pulses bright then settles
//   7. Blink "> INITIALISING... <" prompt, wait for button
//   8. Glitch-wipe exit: random scanlines, then hard cut to black
// ═══════════════════════════════════════════════════════════════════════════════
void playIntroCutscene() {
  // Hardcoded HACKER palette — independent of themeIndex
  const uint16_t HK_BG     = C( 0, 1, 0);
  const uint16_t HK_DIM    = C( 0, 4, 0);
  const uint16_t HK_MID    = C( 2,12, 1);
  const uint16_t HK_BRIGHT = C( 6,32, 3);
  const uint16_t HK_HOT    = C( 8,44, 4);
  const uint16_t HK_WHITE  = 0xFFFF;

  tft.fillScreen(0x0000);

  // ── 1. Scan-line flood from top, with faint horizontal grid every 8px ────
  for(int y = 0; y < 128; y++) {
    uint16_t lineCol = ((y % 8) == 0) ? HK_DIM : HK_BG;
    tft.drawFastHLine(0, y, 160, lineCol);
    if(y % 6 == 0) delay(3);
  }
  delay(30);

  // ── 2. "RageMate" materialises with per-char flicker ─────────────────────
  // Each character flickers 3 times (dim → off → bright) then stays on
  tft.setTextSize(2);
  const char* title  = "RageMate";
  int tlen   = strlen(title);
  int tWidth = tlen * 12;
  int tx0    = (160 - tWidth) / 2;
  int ty     = 20;

  for(int i = 0; i < tlen; i++) {
    int cx = tx0 + i * 12;
    uint16_t finalCol = (i % 2 == 0) ? HK_HOT : HK_BRIGHT;
    // Flicker: dim, off, dim, final
    uint16_t flick[4] = { HK_MID, HK_BG, HK_MID, finalCol };
    for(int f = 0; f < 4; f++) {
      // Erase
      tft.setTextColor(HK_BG);
      tft.setCursor(cx, ty);
      tft.print(title[i]);
      // Draw
      tft.setTextColor(flick[f]);
      tft.setCursor(cx, ty);
      tft.print(title[i]);
      delay(f == 3 ? 0 : 22);
    }
    delay(38);
  }

  delay(60);

  // ── 3. Double horizontal rule sweeps out from centre ─────────────────────
  int divY = 46;
  for(int d = 0; d <= 72; d++) {
    tft.drawPixel(80 - d, divY,     HK_BRIGHT);
    tft.drawPixel(80 + d, divY,     HK_BRIGHT);
    tft.drawPixel(80 - d, divY + 2, HK_DIM);
    tft.drawPixel(80 + d, divY + 2, HK_DIM);
    if(d % 5 == 0) delay(2);
  }
  delay(20);

  // ── 4. MODEL slot machine ─────────────────────────────────────────────────
  // Label
  tft.setTextSize(1);
  tft.setTextColor(HK_DIM);
  tft.setCursor(16, 54);
  tft.print("MODEL");

  const char* models[] = {
    "Enigma ","Mecha  ","Vendetta","Mamba  ",
    "Katana ","Valkyra","Medusa ","Hydra  ",
    "Chimera","Ultra  ","Maxima "
  };
  int modelCount = 11;
  int mSlotX = 68;
  int mSlotY = 54;
  int mSlotW = 75;

  // Spin fast first, then slow down
  int delays[] = { 30,30,30,30,40,40,55,55,75,100,130,0 };
  for(int i = 0; i < modelCount; i++) {
    // Erase slot
    tft.fillRect(mSlotX, mSlotY, mSlotW, 9, HK_BG);
    // Draw current name — brighter as we approach final
    uint16_t c = (i < modelCount - 3) ? HK_MID : (i < modelCount - 1) ? HK_BRIGHT : HK_HOT;
    tft.setTextColor(c);
    tft.setCursor(mSlotX, mSlotY);
    tft.print(models[i]);
    delay(delays[i]);
  }
  // Final: re-draw "Maxima" in HK_HOT, lock it
  tft.fillRect(mSlotX, mSlotY, mSlotW, 9, HK_BG);
  tft.setTextColor(HK_HOT);
  tft.setCursor(mSlotX, mSlotY);
  tft.print("Maxima");

  delay(40);

  // ── 5. VERSION slot machine ───────────────────────────────────────────────
  tft.setTextSize(1);
  tft.setTextColor(HK_DIM);
  tft.setCursor(16, 68);
  tft.print("VERSION");

  int vSlotX = 80;
  int vSlotY = 68;
  int vSlotW = 14;

  int vDelays[] = { 25,25,28,32,38,48,62,80,110,0 };
  for(int d = 1; d <= 9; d++) {
    tft.fillRect(vSlotX, vSlotY, vSlotW, 9, HK_BG);
    uint16_t c = (d < 7) ? HK_MID : (d < 9) ? HK_BRIGHT : HK_HOT;
    tft.setTextColor(c);
    tft.setCursor(vSlotX, vSlotY);
    tft.print(d);
    delay(vDelays[d - 1]);
  }
  // Land on 1 — re-draw
  tft.fillRect(vSlotX, vSlotY, vSlotW, 9, HK_BG);
  tft.setTextColor(HK_HOT);
  tft.setCursor(vSlotX, vSlotY);
  tft.print("1");

  delay(60);

  // ── 6. Border pulses bright then settles ─────────────────────────────────
  for(int pulse = 0; pulse < 3; pulse++) {
    uint16_t pc = (pulse == 2) ? HK_HOT : HK_BRIGHT;
    tft.drawRect(2, 2, 156, 124, pc);
    delay(55);
    if(pulse < 2) { tft.drawRect(2, 2, 156, 124, HK_BG); delay(35); }
  }
  // Settled border: outer bright + inner dim
  tft.drawRect(2, 2, 156, 124, HK_BRIGHT);
  tft.drawRect(4, 4, 152, 120, HK_DIM);

  delay(50);

  // ── 7. Blinking "> INITIALISING... <" prompt, wait for button press ───────
  const char* prompt = "> INITIALISING... <";
  int plen   = strlen(prompt);
  int pWidth = plen * 6;
  int px0    = (160 - pWidth) / 2;
  int py     = 100;

  auto drawPrompt = [&](bool on) {
    tft.setTextSize(1);
    tft.setTextColor(on ? HK_BRIGHT : HK_BG);
    tft.setCursor(px0, py);
    tft.print(prompt);
  };

  // Fast-blink 5 times, then wait
  for(int b = 0; b < 5; b++) {
    drawPrompt(true);  delay(160);
    drawPrompt(false); delay(100);
  }
  drawPrompt(true);

  while(digitalRead(JOY_SW) == LOW) { yield(); delay(10); }
  unsigned long lastBlink = millis();
  bool promptOn = true;
  while(digitalRead(JOY_SW) != LOW) {
    if(millis() - lastBlink > 350) {
      promptOn = !promptOn;
      drawPrompt(promptOn);
      lastBlink = millis();
    }
    yield(); delay(10);
  }
  while(digitalRead(JOY_SW) == LOW) { yield(); delay(10); }

  // ── 8. Glitch-wipe exit: random scanlines scramble, then hard black ───────
  // Draw ~30 random horizontal lines in HK_BRIGHT/HK_DIM to simulate glitch
  unsigned long seed = millis();
  for(int g = 0; g < 40; g++) {
    seed = seed * 1664525UL + 1013904223UL; // LCG
    int gy = (int)(seed >> 17) & 127;
    uint16_t gc = ((seed >> 24) & 1) ? HK_BRIGHT : HK_DIM;
    tft.drawFastHLine(0, gy, 160, gc);
    delay(4);
  }
  delay(20);
  tft.fillScreen(0x0000);
  delay(20);
  // Small white flash
  tft.fillScreen(HK_WHITE);
  delay(25);
  tft.fillScreen(0x0000);
}


// ═══════════════════════════════════════════════════════════════════════════════
// JOYSTICK HANDLING
// ═══════════════════════════════════════════════════════════════════════════════
void handleJoystick() {
  int vx = analogRead(JOY_VRX);
  int vy = analogRead(JOY_VRY);

  unsigned long effDelay = joyEffectiveDelay(vx, vy);
  if(millis() - lastJoyMove < effDelay) return;

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
      // Map board row to panel focus:
      // rows 6-7→flag(1), rows 4-5→undo(2), rows 2-3→theme(3), rows 0-1→sensitivity(4)
      panelFocus = (newRow >= 6) ? 1 : (newRow >= 4) ? 2 : (newRow >= 2) ? 3 : 4;
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
      // dy=1 = joystick UP → move toward flag (panel 1, top)
      // dy=-1 = joystick DOWN → move toward sensitivity (panel 4, bottom)
      // Order: 1(flag) → 2(undo) → 3(theme) → 4(sensitivity)
      int newFocus = panelFocus + (dy > 0 ? -1 : 1);
      if(newFocus < 1) newFocus = 1;
      if(newFocus > 4) newFocus = 4;
      panelFocus = newFocus;
      cursorRow = (panelFocus == 1) ? 7 : (panelFocus == 2) ? 5 : (panelFocus == 3) ? 2 : 0;
      drawLeftPanel();
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HOLD BUTTON SYSTEM (modified for resign + password undo)
// ═══════════════════════════════════════════════════════════════════════════════
#define HOLD_CONFIRM_MS      2000   // 2 second hold for resign / undo
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
    // panelFocus 3 = theme selector — single press, no hold
    if(panelFocus == 3) {
      if(millis()-lastPress < 300) return;
      lastPress = millis();
      showThemeMenu();
      return;
    }
    // panelFocus 4 = sensitivity — single press, no hold
    if(panelFocus == 4) {
      if(millis()-lastPress < 300) return;
      lastPress = millis();
      showSensitivityMenu();
      return;
    }
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
        // Safety: if history is full, declare draw rather than corrupt state
        if(historyCount >= MAX_HISTORY - 2) {
          gameOver = true;
          strcpy(gameOverMsg, "Draw: Move Limit");
          drawGameOver();
          return;
        }
        HistoryEntry he;
        makeMove(legalCache[i], &he);
        history[historyCount++] = he;
        if(hashHistoryCount < MAX_HISTORY + AB_MAX_LEVELS + 3)
          hashHistory[hashHistoryCount++] = currentHash;
        else
          hashHistoryCount++;
        fullMoveCount++;

        clearLegalDots();
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

        drawLeftPanel();   // updates phase indicator
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
          if(hashHistoryCount < MAX_HISTORY + AB_MAX_LEVELS + 3)
            hashHistory[hashHistoryCount++] = currentHash;
          else
            hashHistoryCount++;
          // openingMoveIndex already updated inside getBotMove — do not increment here
          lastBlackFrom = botMove.from;
          lastBlackTo   = botMove.to;
          bPieceMoved &= ~(1ULL << botMove.from);
          bPieceMoved |=  (1ULL << botMove.to);

          // Erase old engine outlines first — draw old squares clean BEFORE
          // updating engineFrom/engineTo so redrawSquareWithArrow won't re-outline them
          if(engineFrom >= 0) {
            int oldFrom = engineFrom, oldTo = engineTo;
            engineFrom = -1; engineTo = -1;   // clear so redrawSquareWithArrow skips outline
            drawSquare(oldFrom);
            drawSquare(oldTo);
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

          drawLeftPanel();   // updates phase indicator
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
  Move allMoves[128];
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

// ═══════════════════════════════════════════════════════════════════════════════
// UI-6: SENSITIVITY MENU
// ═══════════════════════════════════════════════════════════════════════════════
// Layout:
//   Header (top 16px): "SENSITIVITY"
//   Slider bar (y 24): filled rect showing current %, label "XX%"
//   Analog response checkbox (y 52): outlined box + label
//   Back button prompt (y 108)
//
// Controls (FIXED joystick speed — always uses neutral 180ms delay):
//   LEFT/RIGHT: adjust sensitivity 1–100
//   UP/DOWN: move focus between slider and analog-response checkbox
//   BTN: toggle analogResponse when checkbox is focused / confirm slider value
//   HOLD LEFT: exit back to board
//
// The joystick in THIS menu always uses JOY_DELAY (180ms) regardless of the
// current sensitivity setting, so the menu remains controllable at any value.
// ═══════════════════════════════════════════════════════════════════════════════
void showSensitivityMenu() {
  // ── Full redraw ───────────────────────────────────────────────────────────
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, 160, 16, C_BTN_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_HOVER);  tft.setCursor(4, 4);  tft.print("SENSITIVITY");
  tft.setTextColor(C_BORDER); tft.setCursor(90,4);  tft.print("BTN=select");
  tft.drawFastHLine(0, 16, 160, C_BORDER);

  // Footer
  tft.drawFastHLine(0, 118, 160, C_BORDER);
  tft.setTextColor(C_BORDER); tft.setCursor(4, 120); tft.print("hold left to go back");

  // Working copy — don't commit until exit
  int   workSens    = joySensitivity;
  bool  workAnalog  = analogResponse;
  // focusItem: 0 = slider, 1 = analog checkbox
  int   focusItem   = 0;

  // ── Slider draw helper ────────────────────────────────────────────────────
  // Slider: x 4..155 (152px wide), y 30..39 (10px tall)
  const int SL_X = 4, SL_Y = 30, SL_W = 152, SL_H = 10;

  auto drawSlider = [&]() {
    // Background track
    tft.fillRect(SL_X, SL_Y, SL_W, SL_H, C_BTN_BG);
    tft.drawRect(SL_X, SL_Y, SL_W, SL_H, (focusItem==0) ? C_HOVER : C_BORDER);
    // Filled portion — map 1–100 to 0–SL_W pixels
    int filled = (workSens - 1) * (SL_W - 2) / 99;
    if(filled > 0)
      tft.fillRect(SL_X+1, SL_Y+1, filled, SL_H-2, C_HOVER);
    // Slider label above bar
    tft.fillRect(SL_X, SL_Y - 12, SL_W, 10, C_BG);
    tft.setTextSize(1);
    tft.setTextColor((focusItem==0) ? C_HOVER : C_BTN_HL);
    tft.setCursor(SL_X, SL_Y - 10);
    tft.print("Cursor Speed");
    // Percentage value to the right
    tft.fillRect(130, SL_Y - 10, 30, 8, C_BG);
    tft.setTextColor(C_HOVER);
    tft.setCursor(132, SL_Y - 10);
    if(workSens < 10)  tft.print(' ');
    if(workSens < 100) tft.print(' ');
    tft.print(workSens); tft.print('%');
  };

  // ── Analog response checkbox draw helper ──────────────────────────────────
  // Checkbox: 8×8 box at (4, 56), label to the right
  const int CB_X = 4, CB_Y = 56, CB_S = 8;

  auto drawCheckbox = [&]() {
    tft.fillRect(0, CB_Y - 12, 160, 30, C_BG);
    tft.setTextSize(1);
    tft.setTextColor((focusItem==1) ? C_HOVER : C_BTN_HL);
    tft.setCursor(CB_X, CB_Y - 10);
    tft.print("Analog Response");
    // Outer border — bright when focused
    tft.drawRect(CB_X, CB_Y, CB_S, CB_S, (focusItem==1) ? C_HOVER : C_BORDER);
    // Fill when checked
    if(workAnalog)
      tft.fillRect(CB_X+2, CB_Y+2, CB_S-4, CB_S-4, C_HOVER);
    else
      tft.fillRect(CB_X+2, CB_Y+2, CB_S-4, CB_S-4, C_BG);
    // Description line
    tft.setTextColor(C_BORDER);
    tft.setCursor(CB_X + CB_S + 4, CB_Y);
    tft.print(workAnalog ? "ON " : "OFF");
    tft.setCursor(CB_X, CB_Y + CB_S + 3);
    tft.print("Speed scales with push");
  };

  drawSlider();
  drawCheckbox();

  // ── Input loop — FIXED delay (JOY_DELAY, always 180ms) ───────────────────
  unsigned long lastJoy2  = 0;
  unsigned long holdLeft  = 0;
  bool          joyWas    = false;

  while(true) {
    yield();

    // Hold-left progress bar to exit
    int vxR = analogRead(JOY_VRX);
    int vyR = analogRead(JOY_VRY);
    bool joyLeft = (vxR < 1000);
    if(joyLeft) {
      if(holdLeft == 0) holdLeft = millis();
      unsigned long held = millis() - holdLeft;
      int pct = (int)((held * 100UL) / 1200UL);
      if(pct > 100) pct = 100;
      // Draw tiny back-progress bar at bottom
      tft.fillRect(TM_PB_X, TM_PB_Y, TM_PB_W, TM_PB_H, C_BTN_BG);
      if(pct > 0) tft.fillRect(TM_PB_X, TM_PB_Y, (TM_PB_W * pct) / 100, TM_PB_H, C_HOVER);
      if(held >= 1200) {
        // Commit values and return
        joySensitivity = workSens;
        analogResponse = workAnalog;
        drawAll();
        return;
      }
    } else {
      if(holdLeft != 0) {
        holdLeft = 0;
        tft.fillRect(TM_PB_X, TM_PB_Y, TM_PB_W, TM_PB_H, C_BG);
      }
    }

    // Joystick movement — always use fixed JOY_DELAY
    if(millis() - lastJoy2 < (unsigned long)JOY_DELAY) continue;

    int dxJ = 0, dyJ = 0;
    if(vxR < 1000) dxJ = -1;
    else if(vxR > 3000) dxJ = 1;
    if(vyR < 1000) dyJ = 1;
    else if(vyR > 3000) dyJ = -1;

    if(dxJ != 0 || dyJ != 0) {
      lastJoy2 = millis();

      if(dyJ != 0) {
        // UP/DOWN: switch focus between slider (0) and checkbox (1)
        int newFocus = focusItem + (dyJ > 0 ? -1 : 1);
        if(newFocus < 0) newFocus = 0;
        if(newFocus > 1) newFocus = 1;
        if(newFocus != focusItem) {
          focusItem = newFocus;
          drawSlider();
          drawCheckbox();
        }
      }

      if(dxJ != 0 && focusItem == 0) {
        workSens += dxJ;
        if(workSens < 1)   workSens = 1;
        if(workSens > 100) workSens = 100;
        drawSlider();
      }
    }

    // Button press: toggle checkbox or confirm slider
    bool pressed = (digitalRead(JOY_SW) == LOW);
    if(pressed && !joyWas) {
      if(focusItem == 1) {
        workAnalog = !workAnalog;
        drawCheckbox();
      }
      // On slider: button does nothing extra (slider value is always live)
    }
    joyWas = pressed;
  }
}
// ═══════════════════════════════════════════════════════════════════════════════
// Layout: header (top 18px) + scrollable list (10 rows × 11px) + footer (bottom 9px)
// Each row: index (2 chars) + name (up to 12 chars) + 3-colour swatch (14px wide)
// Controls: joystick UP/DN scroll | BTN select | joystick LEFT = back
#define TM_ROWS_VIS  9
#define TM_ROW_H    11
#define TM_ROW_Y0   19

void drawThemeMenuRow(int idx, int slotY, bool selected) {
  if(selected)
    tft.fillRect(0, slotY, 160, TM_ROW_H-1, C_BTN_BG);
  else
    tft.fillRect(0, slotY, 160, TM_ROW_H-1, C_BG);

  // Index number
  tft.setTextSize(1);
  tft.setTextColor(selected ? C_HOVER : C_BORDER);
  tft.setCursor(2, slotY+2);
  if(idx < 10) tft.print('0');
  tft.print(idx);

  // Theme name from PROGMEM
  tft.setTextColor(selected ? C_HOVER : C_BTN_HL);
  tft.setCursor(16, slotY+2);
  const char* nm = themeName(idx);
  // print up to 13 chars to avoid overflowing swatch area
  char buf[14]; strncpy_P(buf, nm, 13); buf[13]=0;
  tft.print(buf);

  // 3-colour swatch: darkSq | lightSq | cursor — rightmost 14px
  uint16_t ds = themeField(idx, 1); // darkSq
  uint16_t ls = themeField(idx, 2); // lightSq
  uint16_t cu = themeField(idx, 4); // cursor
  tft.fillRect(144,  slotY, 5, TM_ROW_H-1, ds);
  tft.fillRect(149,  slotY, 5, TM_ROW_H-1, ls);
  tft.fillRect(154,  slotY, 6, TM_ROW_H-1, cu);

  // Tick mark for currently active theme
  if(idx == themeIndex) {
    tft.setTextColor(C_HOVER);
    tft.setCursor(138, slotY+2);
    tft.print('>');
  }
}

// ─── Theme menu constants ────────────────────────────────────────────────────
#define TM_BACK_MS     1200   // hold-left duration to exit (ms)
#define TM_PB_X        2      // back-progress bar left x
#define TM_PB_Y        121    // back-progress bar top y
#define TM_PB_W        156    // back-progress bar max width
#define TM_PB_H        4      // back-progress bar height
#define TM_REPEAT_INIT  400   // ms before auto-repeat kicks in
#define TM_REPEAT_FAST   80   // ms between scroll steps during auto-repeat

void showThemeMenu() {
  // ── Initial full draw ─────────────────────────────────────────────────────
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, 160, 18, C_BTN_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_HOVER);   tft.setCursor(4, 3);  tft.print("COLOUR THEME");
  tft.setTextColor(C_BORDER);  tft.setCursor(4, 11); tft.print("BTN=select  HOLD LEFT=back");
  tft.drawFastHLine(0, 18, 160, C_BORDER);

  // Footer
  tft.drawFastHLine(0, 118, 160, C_BORDER);
  tft.fillRect(0, 119, 160, 9, C_BG);
  tft.setTextColor(C_BORDER); tft.setTextSize(1);
  tft.setCursor(4, 120); tft.print("hold left to go back");

  uint8_t sel = themeMenuSel;
  if(sel >= NUM_THEMES) sel = 0;

  int winStart = (int)sel - TM_ROWS_VIS/2;
  if(winStart < 0) winStart = 0;
  if(winStart > NUM_THEMES - TM_ROWS_VIS) winStart = NUM_THEMES - TM_ROWS_VIS;
  if(winStart < 0) winStart = 0;

  for(int slot = 0; slot < TM_ROWS_VIS; slot++) {
    int idx = winStart + slot;
    if(idx >= NUM_THEMES) break;
    drawThemeMenuRow(idx, TM_ROW_Y0 + slot*TM_ROW_H, idx == sel);
  }

  // ── State ─────────────────────────────────────────────────────────────────
  unsigned long lastScrollTime = 0;   // when we last moved the cursor
  bool          inRepeat       = false;
  unsigned long holdDownStart  = 0;   // when UP/DN was first pressed
  int           lastDir        = 0;   // +1 down, -1 up, 0 none

  unsigned long leftHoldStart  = 0;   // when joystick-left was first pushed
  int8_t        lastPbFill     = -1;  // last drawn progress bar fill width

  bool btnWas = false;

  auto drawBackBar = [&](int fillW) {
    // Draw the hold-left progress bar in the footer
    tft.fillRect(TM_PB_X, TM_PB_Y, TM_PB_W, TM_PB_H, C_BTN_BG);
    tft.drawRect(TM_PB_X, TM_PB_Y, TM_PB_W, TM_PB_H, C_BORDER);
    if(fillW > 0)
      tft.fillRect(TM_PB_X+1, TM_PB_Y+1, fillW, TM_PB_H-2, C_HOVER);
    lastPbFill = fillW;
  };

  // ── Input loop ────────────────────────────────────────────────────────────
  while(true) {
    unsigned long now   = millis();
    bool pressed        = (digitalRead(JOY_SW) == LOW);
    int  vx             = analogRead(JOY_VRX);
    int  vy             = analogRead(JOY_VRY);
    bool jU = (vy < 1000), jD = (vy > 3000), jL = (vx < 1000);

    // ── Hold-left: back with progress bar ────────────────────────────────
    if(jL) {
      if(leftHoldStart == 0) leftHoldStart = now;
      unsigned long held = now - leftHoldStart;
      int fillW = (int)((held * (long)(TM_PB_W-2)) / TM_BACK_MS);
      if(fillW > TM_PB_W-2) fillW = TM_PB_W-2;
      if((int8_t)fillW != lastPbFill) drawBackBar(fillW);
      if(held >= TM_BACK_MS) {
        // Exit without changing theme
        themeMenuSel = sel;
        drawAll();
        return;
      }
    } else {
      // Released left — cancel and wipe bar if it was started
      if(leftHoldStart != 0) {
        leftHoldStart = 0;
        tft.fillRect(TM_PB_X, TM_PB_Y, TM_PB_W, TM_PB_H+1, C_BG);
        lastPbFill = -1;
      }
    }

    // ── Dynamic hold-scroll ───────────────────────────────────────────────
    int dir = jU ? -1 : jD ? 1 : 0;

    if(dir == 0) {
      // No direction held — reset scroll state
      lastDir      = 0;
      inRepeat     = false;
      holdDownStart = 0;
    } else {
      if(dir != lastDir) {
        // New direction: move immediately on first frame
        lastDir       = dir;
        inRepeat      = false;
        holdDownStart = now;
        // Immediate single step
        uint8_t prevSel = sel;
        if(dir == -1 && sel > 0)             sel--;
        else if(dir == 1 && sel < NUM_THEMES-1) sel++;
        lastScrollTime = now;

        if(sel != prevSel) {
          int newWin = (int)sel - TM_ROWS_VIS/2;
          if(newWin < 0) newWin = 0;
          if(newWin > NUM_THEMES - TM_ROWS_VIS) newWin = NUM_THEMES - TM_ROWS_VIS;
          if(newWin < 0) newWin = 0;
          if(newWin != winStart) {
            winStart = newWin;
            for(int s = 0; s < TM_ROWS_VIS; s++) {
              int idx = winStart + s; if(idx >= NUM_THEMES) break;
              drawThemeMenuRow(idx, TM_ROW_Y0 + s*TM_ROW_H, idx == sel);
            }
          } else {
            int os = (int)prevSel - winStart, ns2 = (int)sel - winStart;
            if(os >= 0 && os < TM_ROWS_VIS)
              drawThemeMenuRow(prevSel, TM_ROW_Y0 + os*TM_ROW_H, false);
            if(ns2 >= 0 && ns2 < TM_ROWS_VIS)
              drawThemeMenuRow(sel,     TM_ROW_Y0 + ns2*TM_ROW_H, true);
          }
        }
      } else {
        // Same direction still held
        unsigned long heldMs = now - holdDownStart;
        if(!inRepeat && heldMs >= TM_REPEAT_INIT) {
          inRepeat = true;
          lastScrollTime = now;
        }
        if(inRepeat && (now - lastScrollTime) >= TM_REPEAT_FAST) {
          lastScrollTime = now;
          uint8_t prevSel = sel;
          if(dir == -1 && sel > 0)              sel--;
          else if(dir == 1 && sel < NUM_THEMES-1) sel++;

          if(sel != prevSel) {
            int newWin = (int)sel - TM_ROWS_VIS/2;
            if(newWin < 0) newWin = 0;
            if(newWin > NUM_THEMES - TM_ROWS_VIS) newWin = NUM_THEMES - TM_ROWS_VIS;
            if(newWin < 0) newWin = 0;
            if(newWin != winStart) {
              winStart = newWin;
              for(int s = 0; s < TM_ROWS_VIS; s++) {
                int idx = winStart + s; if(idx >= NUM_THEMES) break;
                drawThemeMenuRow(idx, TM_ROW_Y0 + s*TM_ROW_H, idx == sel);
              }
            } else {
              int os = (int)prevSel - winStart, ns2 = (int)sel - winStart;
              if(os >= 0 && os < TM_ROWS_VIS)
                drawThemeMenuRow(prevSel, TM_ROW_Y0 + os*TM_ROW_H, false);
              if(ns2 >= 0 && ns2 < TM_ROWS_VIS)
                drawThemeMenuRow(sel,     TM_ROW_Y0 + ns2*TM_ROW_H, true);
            }
          }
        }
      }
    }

    // ── Button: select and apply theme ────────────────────────────────────
    if(pressed && !btnWas) {
      themeIndex   = sel;
      themeMenuSel = sel;
      refreshTheme();
      drawAll();
      return;
    }

    btnWas = pressed;
    yield(); delay(8);
  }
}
void doRestart() {
  initBoard();
  // Do NOT reset themeIndex — player keeps their chosen theme across games
  evalScore   = 0;    // reset eval bar to 50/50
  dispPct     = -1;
  searchDepth = 0;
  searchPct   = 0;
  panelFocus  = 0;
  cursorCol   = 4;
  cursorRow   = 4;
  selSq       = -1;
  legalCount  = 0;
  checkmatePending = false;
  fullMoveCount    = 0;
  memset(tt, 0, sizeof(tt));
  memset(histHeuristic, 0, sizeof(histHeuristic));
  drawAll();  // drawAll calls drawEvalBar(evalScore) which is now 0
}

void doUndo() {
  if(historyCount < 2) return;
  if(gameOver) {
    gameOver=false;
    gameOverMsg[0]=0;
    checkmatePending=false;
  }

  if(historyCount >= 2 && historyCount-1 < MAX_HISTORY)
    unmakeMove(history[historyCount-1]);
  historyCount--;
  if(hashHistoryCount>0) hashHistoryCount--;
  engineFrom=-1; engineTo=-1;

  if(historyCount >= 1 && historyCount-1 < MAX_HISTORY)
    unmakeMove(history[historyCount-1]);
  historyCount--;
  if(hashHistoryCount>0) hashHistoryCount--;

  openingMoveIndex = max(0, openingMoveIndex-1);

  // Rebuild bPieceMoved, lastBlackFrom, lastBlackTo from remaining history.
  // These are tracking helpers for the opening heuristic — if we can't
  // perfectly reconstruct them we reset to safe defaults, which is harmless
  // (at worst the engine misses one opening penalty for one move).
  bPieceMoved  = 0;
  lastBlackFrom = -1;
  lastBlackTo   = -1;
  for(int i = 1; i < historyCount; i++) {
    Move& mv = history[i].mv;
    // Only track black moves (odd indices in history: 0=white, 1=black, 2=white...)
    // Black moves are at odd indices since white always moves first.
    if(i % 2 == 1) {
      bPieceMoved &= ~(1ULL << mv.from);
      bPieceMoved |=  (1ULL << mv.to);
      lastBlackFrom = mv.from;
      lastBlackTo   = mv.to;
    }
  }

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

  // Initialise theme colours before any drawing
  themeIndex = 0;
  refreshTheme();

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