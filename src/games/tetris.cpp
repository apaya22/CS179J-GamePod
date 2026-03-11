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
static const int HEADER_H   = 16;
static const int CELL       = 8;
static const int BOARD_X    = (SCREEN_W - TETRIS_COLS * CELL) / 2;
static const int BOARD_Y    = HEADER_H;
static const int BOARD_W    = TETRIS_COLS * CELL;
static const int BOARD_H    = TETRIS_ROWS * CELL;

static const int PREVIEW_X  = BOARD_X + BOARD_W + 12;
static const int PREVIEW_Y  = HEADER_H + 20;

// Timing
static const uint32_t BASE_DROP_MS   = 500;
static const uint32_t INPUT_DELAY_MS = 120;

// =====================
// PIECE DATA  (SRS-style offsets)
// =====================
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

// Try to rotate with basic wall kicks (shift 0, -1, +1)
static bool tryRotate(int newRot) {
  if (pieceFits(tetrisGame.currentPiece, newRot,
                tetrisGame.pieceCol, tetrisGame.pieceRow)) {
    tetrisGame.rotation = newRot;
    return true;
  }
  if (pieceFits(tetrisGame.currentPiece, newRot,
                tetrisGame.pieceCol - 1, tetrisGame.pieceRow)) {
    tetrisGame.rotation = newRot;
    tetrisGame.pieceCol--;
    return true;
  }
  if (pieceFits(tetrisGame.currentPiece, newRot,
                tetrisGame.pieceCol + 1, tetrisGame.pieceRow)) {
    tetrisGame.rotation = newRot;
    tetrisGame.pieceCol++;
    return true;
  }
  return false;
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
      for (int rr = r; rr > 0; rr--) {
        for (int c = 0; c < TETRIS_COLS; c++) {
          tetrisGame.board[rr][c] = tetrisGame.board[rr - 1][c];
        }
      }
      for (int c = 0; c < TETRIS_COLS; c++) {
        tetrisGame.board[0][c] = 0;
      }
      r++;  // recheck this row
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
  tetrisGame.pieceCol = (TETRIS_COLS - 4) / 2;
  tetrisGame.pieceRow = 0;

  if (!pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                 tetrisGame.pieceCol, tetrisGame.pieceRow)) {
    return false;
  }
  drawPreview();
  return true;
}

// =====================
// PUBLIC API
// =====================
void initTetrisGame() {
  randomSeed(esp_random());

  for (int r = 0; r < TETRIS_ROWS; r++)
    for (int c = 0; c < TETRIS_COLS; c++)
      tetrisGame.board[r][c] = 0;

  tetrisGame.score = 0;
  tetrisGame.level = 0;
  tetrisGame.linesCleared = 0;
  tetrisGame.gameOver = false;
  tetrisGame.nextPiece = randomPiece();
}

// =====================
// MAIN GAME LOOP
// =====================
// Controls:
//   Joystick LEFT/RIGHT  = move piece
//   Joystick DOWN         = soft drop
//   Button A              = rotate clockwise
//   Button B              = rotate counter-clockwise
//   Button C              = hard drop (instant)
//   Button D              = pause

void runTetrisGame() {
  initTetrisGame();

  tft.fillScreen(ILI9341_BLACK);
  drawHeader();
  drawBoardBorder();
  drawPreview();

  if (!spawnPiece()) return;
  drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);

  uint32_t lastDrop  = millis();
  uint32_t lastInput = millis();
  bool lastBtnA = buttonPressed(BTN_A);
  bool lastBtnB = buttonPressed(BTN_B);
  bool lastBtnC = buttonPressed(BTN_C);
  bool lastBtnD = buttonPressed(BTN_D);
  bool running = true;

  while (running) {
    uint32_t now = millis();

    // --- INPUT ---
    if (now - lastInput >= INPUT_DELAY_MS) {
      JoyDir joy = joystickDirection();
      bool moved = false;

      // Joystick LEFT — move left
      if (joy == LEFT) {
        if (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                      tetrisGame.pieceCol - 1, tetrisGame.pieceRow)) {
          erasePiece();
          tetrisGame.pieceCol--;
          moved = true;
        }
      }
      // Joystick RIGHT — move right
      else if (joy == RIGHT) {
        if (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                      tetrisGame.pieceCol + 1, tetrisGame.pieceRow)) {
          erasePiece();
          tetrisGame.pieceCol++;
          moved = true;
        }
      }
      // Joystick DOWN — soft drop
      else if (joy == UP) {
        if (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                      tetrisGame.pieceCol, tetrisGame.pieceRow + 1)) {
          erasePiece();
          tetrisGame.pieceRow++;
          moved = true;
          lastDrop = now;
          tetrisGame.score += 1;
        }
      }

      // Button A — rotate clockwise (edge-triggered)
      bool btnA = buttonPressed(BTN_A);
      if (btnA && !lastBtnA) {
        int newRot = (tetrisGame.rotation + 1) % 4;
        erasePiece();
        if (tryRotate(newRot)) {
          moved = true;
        }
      }
      lastBtnA = btnA;

      // Button B — rotate counter-clockwise (edge-triggered)
      bool btnB = buttonPressed(BTN_B);
      if (btnB && !lastBtnB) {
        int newRot = (tetrisGame.rotation + 3) % 4;  // +3 mod 4 == -1 mod 4
        erasePiece();
        if (tryRotate(newRot)) {
          moved = true;
        }
      }
      lastBtnB = btnB;

      // Button C — hard drop (edge-triggered)
      bool btnC = buttonPressed(BTN_C);
      if (btnC && !lastBtnC) {
        erasePiece();
        while (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                         tetrisGame.pieceCol, tetrisGame.pieceRow + 1)) {
          tetrisGame.pieceRow++;
          tetrisGame.score += 2;
        }
        moved = true;
        lastDrop = now - BASE_DROP_MS;  // force immediate lock
      }
      lastBtnC = btnC;

      if (moved) {
        drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);
        lastInput = now;
      }
    }

    // --- GRAVITY ---
    uint32_t dropInterval = BASE_DROP_MS - (uint32_t)(tetrisGame.level * 30);
    if (dropInterval < 80) dropInterval = 80;

    if (now - lastDrop >= dropInterval) {
      lastDrop = now;

      if (pieceFits(tetrisGame.currentPiece, tetrisGame.rotation,
                    tetrisGame.pieceCol, tetrisGame.pieceRow + 1)) {
        erasePiece();
        tetrisGame.pieceRow++;
        drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);
      } else {
        lockPiece();
        int lines = clearLines();
        if (lines > 0) {
          addScore(lines);
          redrawBoard();
          drawHeader();
        }

        if (!spawnPiece()) {
          running = false;
          break;
        }
        drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);
        drawHeader();
      }
    }

    // --- PAUSE on Button D (edge-triggered) ---
    bool btnD = buttonPressed(BTN_D);
    if (btnD && !lastBtnD) {
      tft.setTextColor(GAMEPOD_WHITE);
      tft.setTextSize(2);
      int16_t x1, y1;
      uint16_t w, h;
      tft.getTextBounds("PAUSED", 0, 0, &x1, &y1, &w, &h);
      int pcx = SCREEN_W / 2;
      int pcy = SCREEN_H / 2;
      tft.fillRect(pcx - (int)w / 2 - 4, pcy - (int)h / 2 - 4, w + 8, h + 8, GAMEPOD_DARK);
      tft.setCursor(pcx - (int)w / 2, pcy - (int)h / 2);
      tft.print("PAUSED");
      // Wait for release, then next press to unpause
      while (buttonPressed(BTN_D)) delay(10);
      delay(200);
      while (!buttonPressed(BTN_D)) delay(10);
      while (buttonPressed(BTN_D)) delay(10);
      // Redraw everything
      tft.fillScreen(ILI9341_BLACK);
      drawHeader();
      drawBoardBorder();
      redrawBoard();
      drawPiece(PIECE_COLORS[tetrisGame.currentPiece]);
      drawPreview();
      lastDrop = millis();
    }
    lastBtnD = btnD;
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