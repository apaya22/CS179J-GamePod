#include "tetris.h"
#include "../home_os/home_config.h"
#include "../include/controller/controller.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

extern Adafruit_ILI9341 tft;

// Satisfy extern declaration
TetrisGame tetrisGame;

// =====================
// LAYOUT CONSTANTS
// =====================
static const int HEADER_H   = 16;   // px reserved for score bar
static const int CELL       = 8;    // pixels per cell (same as snake)
// Board is 10 cols x 20 rows = 80 x 160 px
// Center the board horizontally on a 320-wide screen
static const int BOARD_X    = (SCREEN_W - TETRIS_COLS * CELL) / 2;  // left edge
static const int BOARD_Y    = HEADER_H;                              // top edge
static const int BOARD_W    = TETRIS_COLS * CELL;
static const int BOARD_H    = TETRIS_ROWS * CELL;

// Sidebar for next-piece preview
static const int PREVIEW_X  = BOARD_X + BOARD_W + 12;
static const int PREVIEW_Y  = HEADER_H + 20;

// Timing
static const uint32_t BASE_DROP_MS   = 500;  // ms per gravity drop at level 0
static const uint32_t INPUT_DELAY_MS = 120;  // ms between repeated inputs
static const uint32_t SOFT_DROP_MS   = 50;   // faster drop when holding down

// =====================
// PIECE DATA  (SRS-style offsets)
// =====================
// 7 tetrominoes, 4 rotations each, 4 blocks per rotation
// Offsets are {col, row} relative to piece origin
static const int8_t PIECE_DATA[7][4][4][2] = {
  // I
  {
    {{ 0,1},{ 1,1},{ 2,1},{ 3,1}},
    {{ 2,0},{ 2,1},{ 2,2},{ 2,3}},
    {{ 0,2},{ 1,2},{ 2,2},{ 3,2}},
    {{ 1,0},{ 1,1},{ 1,2},{ 1,3}}
  },
  // O
  {
    {{ 0,0},{ 1,0},{ 0,1},{ 1,1}},
    {{ 0,0},{ 1,0},{ 0,1},{ 1,1}},
    {{ 0,0},{ 1,0},{ 0,1},{ 1,1}},
    {{ 0,0},{ 1,0},{ 0,1},{ 1,1}}
  },
  // T
  {
    {{ 0,1},{ 1,1},{ 2,1},{ 1,0}},
    {{ 1,0},{ 1,1},{ 1,2},{ 2,1}},
    {{ 0,1},{ 1,1},{ 2,1},{ 1,2}},
    {{ 1,0},{ 1,1},{ 1,2},{ 0,1}}
  },
  // S
  {
    {{ 1,0},{ 2,0},{ 0,1},{ 1,1}},
    {{ 1,0},{ 1,1},{ 2,1},{ 2,2}},
    {{ 1,1},{ 2,1},{ 0,2},{ 1,2}},
    {{ 0,0},{ 0,1},{ 1,1},{ 1,2}}
  },
  // Z
  {
    {{ 0,0},{ 1,0},{ 1,1},{ 2,1}},
    {{ 2,0},{ 1,1},{ 2,1},{ 1,2}},
    {{ 0,1},{ 1,1},{ 1,2},{ 2,2}},
    {{ 1,0},{ 0,1},{ 1,1},{ 0,2}}
  },
  // L
  {
    {{ 0,1},{ 1,1},{ 2,1},{ 2,0}},
    {{ 1,0},{ 1,1},{ 1,2},{ 2,2}},
    {{ 0,1},{ 1,1},{ 2,1},{ 0,2}},
    {{ 0,0},{ 1,0},{ 1,1},{ 1,2}}
  },
  // J
  {
    {{ 0,0},{ 0,1},{ 1,1},{ 2,1}},
    {{ 1,0},{ 1,1},{ 1,2},{ 2,0}},
    {{ 0,1},{ 1,1},{ 2,1},{ 2,2}},
    {{ 0,2},{ 1,0},{ 1,1},{ 1,2}}
  }
};

static const uint16_t PIECE_COLORS[7] = {
  0x07FF,  // I - Cyan
  0xFFE0,  // O - Yellow
  0xA81F,  // T - Purple
  0x07E0,  // S - Green
  0xF800,  // Z - Red
  0xFD20,  // L - Orange
  0x001F   // J - Blue
};

// =====================
// HELPERS
// =====================
static void drawBoardCell(int col, int row, uint16_t color) {
  tft.fillRect(BOARD_X + col * CELL, BOARD_Y + row * CELL, CELL, CELL, color);
  if (color != ILI9341_BLACK) {
    // thin border to give blocks definition
    tft.drawRect(BOARD_X + col * CELL, BOARD_Y + row * CELL, CELL, CELL, 0x0000);
  }
}

static void drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, GAMEPOD_DARK);
  tft.setTextColor(GAMEPOD_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("TETRIS  Score: ");
  tft.print(tetrisGame.score);
  tft.print("  Lv: ");
  tft.print(tetrisGame.level);
}

static void drawBoardBorder() {
  tft.drawRect(BOARD_X - 1, BOARD_Y - 1, BOARD_W + 2, BOARD_H + 2, GAMEPOD_WHITE);
}

static int randomPiece() {
  return random(0, 7);
}

static void drawPreview() {
  // Clear preview area
  tft.fillRect(PREVIEW_X, PREVIEW_Y, CELL * 4, CELL * 4, ILI9341_BLACK);
  tft.setTextColor(GAMEPOD_WHITE);
  tft.setTextSize(1);
  tft.setCursor(PREVIEW_X, PREVIEW_Y - 12);
  tft.print("NEXT");

  int p = tetrisGame.nextPiece;
  uint16_t color = PIECE_COLORS[p];
  for (int i = 0; i < 4; i++) {
    int c = PIECE_DATA[p][0][i][0];
    int r = PIECE_DATA[p][0][i][1];
    tft.fillRect(PREVIEW_X + c * CELL, PREVIEW_Y + r * CELL, CELL, CELL, color);
    tft.drawRect(PREVIEW_X + c * CELL, PREVIEW_Y + r * CELL, CELL, CELL, 0x0000);
  }
}

// =====================
// COLLISION DETECTION
// =====================
static bool pieceFits(int piece, int rot, int col, int row) {
  for (int i = 0; i < 4; i++) {
    int c = col + PIECE_DATA[piece][rot][i][0];
    int r = row + PIECE_DATA[piece][rot][i][1];
    if (c < 0 || c >= TETRIS_COLS) return false;
    if (r < 0 || r >= TETRIS_ROWS) return false;
    if (tetrisGame.board[r][c] != 0) return false;
  }
  return true;
}

// =====================
// DRAW / ERASE ACTIVE PIECE
// =====================
static void drawPiece(uint16_t color) {
  int p = tetrisGame.currentPiece;
  int rot = tetrisGame.rotation;
  for (int i = 0; i < 4; i++) {
    int c = tetrisGame.pieceCol + PIECE_DATA[p][rot][i][0];
    int r = tetrisGame.pieceRow + PIECE_DATA[p][rot][i][1];
    if (r >= 0 && r < TETRIS_ROWS && c >= 0 && c < TETRIS_COLS) {
      drawBoardCell(c, r, color);
    }
  }
}

static void erasePiece() {
  drawPiece(ILI9341_BLACK);
}

// =====================
// LOCK PIECE & CLEAR LINES
// =====================
static void lockPiece() {
  int p = tetrisGame.currentPiece;
  int rot = tetrisGame.rotation;
  uint16_t color = PIECE_COLORS[p];
  for (int i = 0; i < 4; i++) {
    int c = tetrisGame.pieceCol + PIECE_DATA[p][rot][i][0];
    int r = tetrisGame.pieceRow + PIECE_DATA[p][rot][i][1];
    if (r >= 0 && r < TETRIS_ROWS && c >= 0 && c < TETRIS_COLS) {
      tetrisGame.board[r][c] = color;
    }
  }
}

static int clearLines() {
  int cleared = 0;
  for (int r = TETRIS_ROWS - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < TETRIS_COLS; c++) {
      if (tetrisGame.board[r][c] == 0) { full = false; break; }
    }
    if (full) {
      cleared++;
      // Shift everything above down by one
      for (int rr = r; rr > 0; rr--) {
        for (int c = 0; c < TETRIS_COLS; c++) {
          tetrisGame.board[rr][c] = tetrisGame.board[rr - 1][c];
        }
      }
      // Top row becomes empty
      for (int c = 0; c < TETRIS_COLS; c++) {
        tetrisGame.board[0][c] = 0;
      }
      r++;  // recheck this row since we shifted down
    }
  }
  return cleared;
}

static void redrawBoard() {
  for (int r = 0; r < TETRIS_ROWS; r++) {
    for (int c = 0; c < TETRIS_COLS; c++) {
      drawBoardCell(c, r, tetrisGame.board[r][c]);
    }
  }
}

// Scoring: classic NES style
static void addScore(int lines) {
  static const int SCORE_TABLE[5] = {0, 40, 100, 300, 1200};
  if (lines > 0 && lines <= 4) {
    tetrisGame.score += SCORE_TABLE[lines] * (tetrisGame.level + 1);
  }
  tetrisGame.linesCleared += lines;
  tetrisGame.level = tetrisGame.linesCleared / 10;
}

// =====================
// SPAWN NEW PIECE
// =====================
static bool spawnPiece() {
  tetrisGame.currentPiece = tetrisGame.nextPiece;
  tetrisGame.nextPiece = randomPiece();
  tetrisGame.rotation = 0;
  tetrisGame.pieceCol = (TETRIS_COLS - 4) / 2;  // roughly centered
  tetrisGame.pieceRow = 0;

  if (!pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                 tetrisGame.pieceCol, tetrisGame.pieceRow)) {
    return false;  // game over
  }
  drawPreview();
  return true;
}

// =====================
// PUBLIC API
// =====================
void initTetrisGame() {
  randomSeed(esp_random());

  // Clear board
  for (int r = 0; r < TETRIS_ROWS; r++)
    for (int c = 0; c < TETRIS_COLS; c++)
      tetrisGame.board[r][c] = 0;

  tetrisGame.score = 0;
  tetrisGame.level = 0;
  tetrisGame.linesCleared = 0;
  tetrisGame.gameOver = false;

  tetrisGame.nextPiece = randomPiece();
  // spawnPiece will move nextPiece to current and pick a new next
}

// =====================
// MAIN GAME LOOP
// =====================
void runTetrisGame() {
  initTetrisGame();

  // Draw initial screen
  tft.fillScreen(ILI9341_BLACK);
  drawHeader();
  drawBoardBorder();
  drawPreview();

  if (!spawnPiece()) return;  // shouldn't happen on empty board
  drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);

  uint32_t lastDrop  = millis();
  uint32_t lastInput = millis();
  bool running = true;

  while (running) {
    uint32_t now = millis();

    // --- INPUT ---
    if (now - lastInput >= INPUT_DELAY_MS) {
      JoyDir joy = joystickDirection();
      bool moved = false;

      if (joy == LEFT) {
        if (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                      tetrisGame.pieceCol - 1, tetrisGame.pieceRow)) {
          erasePiece();
          tetrisGame.pieceCol--;
          moved = true;
        }
      }
      else if (joy == RIGHT) {
        if (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                      tetrisGame.pieceCol + 1, tetrisGame.pieceRow)) {
          erasePiece();
          tetrisGame.pieceCol++;
          moved = true;
        }
      }
      else if (joy == DOWN) {
        // Soft drop — advance one row immediately
        if (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                      tetrisGame.pieceCol, tetrisGame.pieceRow + 1)) {
          erasePiece();
          tetrisGame.pieceRow++;
          moved = true;
          lastDrop = now;  // reset gravity timer
          tetrisGame.score += 1;  // reward soft drop
        }
      }
      else if (joy == UP) {
        // Rotate clockwise
        int newRot = (tetrisGame.rotation + 1) % 4;
        if (pieceFits(tetrisGame.currentPiece, newRot,
                      tetrisGame.pieceCol, tetrisGame.pieceRow)) {
          erasePiece();
          tetrisGame.rotation = newRot;
          moved = true;
        }
        // Basic wall kick: try shifting left or right by 1
        else if (pieceFits(tetrisGame.currentPiece, newRot,
                           tetrisGame.pieceCol - 1, tetrisGame.pieceRow)) {
          erasePiece();
          tetrisGame.rotation = newRot;
          tetrisGame.pieceCol--;
          moved = true;
        }
        else if (pieceFits(tetrisGame.currentPiece, newRot,
                           tetrisGame.pieceCol + 1, tetrisGame.pieceRow)) {
          erasePiece();
          tetrisGame.rotation = newRot;
          tetrisGame.pieceCol++;
          moved = true;
        }
      }

      // Hard drop on Button A
      if (buttonPressed(BTN_A)) {
        erasePiece();
        while (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                         tetrisGame.pieceCol, tetrisGame.pieceRow + 1)) {
          tetrisGame.pieceRow++;
          tetrisGame.score += 2;  // reward hard drop
        }
        moved = true;
        lastDrop = now - BASE_DROP_MS;  // force immediate lock on next gravity tick
      }

      if (moved) {
        drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);
        lastInput = now;
      }
    }

    // --- GRAVITY ---
    uint32_t dropInterval = BASE_DROP_MS - (uint32_t)(tetrisGame.level * 30);
    if (dropInterval < 80) dropInterval = 80;  // speed cap

    if (now - lastDrop >= dropInterval) {
      lastDrop = now;

      if (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                    tetrisGame.pieceCol, tetrisGame.pieceRow + 1)) {
        erasePiece();
        tetrisGame.pieceRow++;
        drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);
      } else {
        // Lock the piece
        lockPiece();
        int lines = clearLines();
        if (lines > 0) {
          addScore(lines);
          redrawBoard();
          drawHeader();
        }

        // Spawn next piece
        if (!spawnPiece()) {
          running = false;  // game over
          break;
        }
        drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);
        drawHeader();  // update score display
      }
    }

    // --- PAUSE on Button B ---
    if (buttonPressed(BTN_B)) {
      tft.setTextColor(GAMEPOD_WHITE);
      tft.setTextSize(2);
      int16_t x1, y1;
      uint16_t w, h;
      tft.getTextBounds("PAUSED", 0, 0, &x1, &y1, &w, &h);
      int cx = SCREEN_W / 2;
      int cy = SCREEN_H / 2;
      tft.fillRect(cx - (int)w / 2 - 4, cy - (int)h / 2 - 4, w + 8, h + 8, GAMEPOD_DARK);
      tft.setCursor(cx - (int)w / 2, cy - (int)h / 2);
      tft.print("PAUSED");
      // Wait until button released, then pressed again
      while (buttonPressed(BTN_B)) delay(10);
      delay(200);
      while (!buttonPressed(BTN_B)) delay(10);
      while (buttonPressed(BTN_B)) delay(10);
      // Redraw board
      tft.fillScreen(ILI9341_BLACK);
      drawHeader();
      drawBoardBorder();
      redrawBoard();
      drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);
      drawPreview();
      lastDrop = millis();
    }
  }

  // =====================
  // GAME OVER SCREEN
  // =====================
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2;
  tft.fillRect(cx - 85, cy - 28, 170, 56, GAMEPOD_DARK);
  tft.setTextColor(GAMEPOD_RED);
  tft.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds("GAME OVER", 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - (int)w / 2, cy - 18);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(GAMEPOD_WHITE);
  char buf[32];
  snprintf(buf, sizeof(buf), "Score: %d  Lines: %d", tetrisGame.score, tetrisGame.linesCleared);
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - (int)w / 2, cy + 8);
  tft.print(buf);
  delay(3000);
}