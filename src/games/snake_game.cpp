#include "snake_game.h"
#include "../home_os/home_config.h"
#include "../home_os/input_handler.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

extern Adafruit_ILI9341 tft;

// Satisfy extern declarations in snake_game.h
SnakeGame snakeGame;
int lastScore = -1;
bool gameOverScreenRendered = false;
bool snakeGameNeedsFullRedraw = true;

// =====================
// LAYOUT CONSTANTS
// =====================
static const int HEADER_H = 16;   // px reserved for score bar
static const int GRID     = 8;    // pixels per cell
static const int COLS     = SCREEN_W / GRID;               // 40
static const int ROWS     = (SCREEN_H - HEADER_H) / GRID;  // 28
static const uint32_t MOVE_MS = 150;                        // ms per snake step

// =====================
// HELPERS
// =====================
static void drawCell(int col, int row, uint16_t color) {
  tft.fillRect(col * GRID, HEADER_H + row * GRID, GRID, GRID, color);
}

static void drawSnakeHeader(int score) {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, GAMEPOD_DARK);
  tft.setTextColor(GAMEPOD_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("SNAKE  Score: ");
  tft.print(score);
}

static void spawnApple() {
  int col, row;
  bool onSnake;
  do {
    col = random(0, COLS);
    row = random(0, ROWS);
    onSnake = false;
    int px = col * GRID;
    int py = HEADER_H + row * GRID;
    for (int i = 0; i < snakeGame.snakeLength; i++) {
      if (snakeGame.snake[i].x == px && snakeGame.snake[i].y == py) {
        onSnake = true;
        break;
      }
    }
  } while (onSnake);
  snakeGame.appleX = col * GRID;
  snakeGame.appleY = HEADER_H + row * GRID;
}

// =====================
// PUBLIC API
// =====================
void initSnakeGame() {
  randomSeed(esp_random());

  snakeGame.snakeLength = 3;
  snakeGame.dirX = 1;
  snakeGame.dirY = 0;
  snakeGame.nextDirX = 1;
  snakeGame.nextDirY = 0;
  snakeGame.score = 0;
  snakeGame.updateCounter = 0;
  snakeGame.updateRate = 5;

  int startCol = COLS / 2;
  int startRow = ROWS / 2;
  for (int i = 0; i < 3; i++) {
    snakeGame.snake[i].x = (startCol - i) * GRID;
    snakeGame.snake[i].y = HEADER_H + startRow * GRID;
  }

  spawnApple();
  lastScore = -1;
  gameOverScreenRendered = false;
  snakeGameNeedsFullRedraw = true;
}

void updateSnakeGame() {
  // Logic is inlined in runSnakeGame(); stub kept for header compatibility.
}

bool checkSnakeCollision() {
  int hx = snakeGame.snake[0].x;
  int hy = snakeGame.snake[0].y;
  if (hx < 0 || hx >= SCREEN_W || hy < HEADER_H || hy >= SCREEN_H) return true;
  for (int i = 1; i < snakeGame.snakeLength; i++) {
    if (hx == snakeGame.snake[i].x && hy == snakeGame.snake[i].y) return true;
  }
  return false;
}

// =====================
// MAIN GAME LOOP
// =====================
void runSnakeGame() {
  initSnakeGame();

  tft.fillScreen(ILI9341_BLACK);
  drawSnakeHeader(0);
  for (int i = 0; i < snakeGame.snakeLength; i++) {
    drawCell(snakeGame.snake[i].x / GRID,
             (snakeGame.snake[i].y - HEADER_H) / GRID,
             ILI9341_GREEN);
  }
  drawCell(snakeGame.appleX / GRID, (snakeGame.appleY - HEADER_H) / GRID, ILI9341_RED);

  uint32_t lastMove = millis();
  bool running = true;

  while (running) {
    // Input - prevent 180-degree reversals
    JoyDir joy = joystickDirection();
    if (joy == UP    && snakeGame.dirY != -1)  { snakeGame.nextDirX =  0; snakeGame.nextDirY = 1; }
    if (joy == DOWN  && snakeGame.dirY != 1) { snakeGame.nextDirX =  0; snakeGame.nextDirY =  -1; }
    if (joy == LEFT  && snakeGame.dirX != 1)  { snakeGame.nextDirX = -1; snakeGame.nextDirY =  0; }
    if (joy == RIGHT && snakeGame.dirX != -1) { snakeGame.nextDirX =  1; snakeGame.nextDirY =  0; }

    uint32_t now = millis();
    if (now - lastMove < MOVE_MS) continue;
    lastMove = now;

    snakeGame.dirX = snakeGame.nextDirX;
    snakeGame.dirY = snakeGame.nextDirY;

    int newHX = snakeGame.snake[0].x + snakeGame.dirX * GRID;
    int newHY = snakeGame.snake[0].y + snakeGame.dirY * GRID;

    // Wall collision
    if (newHX < 0 || newHX >= SCREEN_W || newHY < HEADER_H || newHY >= SCREEN_H) break;

    // Self collision
    for (int i = 0; i < snakeGame.snakeLength; i++) {
      if (newHX == snakeGame.snake[i].x && newHY == snakeGame.snake[i].y) {
        running = false;
        break;
      }
    }
    if (!running) break;

    bool ateApple = (newHX == snakeGame.appleX && newHY == snakeGame.appleY);

    // Erase tail only when not growing
    if (!ateApple) {
      drawCell(snakeGame.snake[snakeGame.snakeLength - 1].x / GRID,
               (snakeGame.snake[snakeGame.snakeLength - 1].y - HEADER_H) / GRID,
               ILI9341_BLACK);
    }

    // Shift body
    for (int i = snakeGame.snakeLength - 1; i > 0; i--) {
      snakeGame.snake[i] = snakeGame.snake[i - 1];
    }
    snakeGame.snake[0].x = newHX;
    snakeGame.snake[0].y = newHY;

    drawCell(newHX / GRID, (newHY - HEADER_H) / GRID, ILI9341_GREEN);

    if (ateApple) {
      snakeGame.snakeLength++;
      snakeGame.score += 10;
      spawnApple();
      drawCell(snakeGame.appleX / GRID, (snakeGame.appleY - HEADER_H) / GRID, ILI9341_RED);
      drawSnakeHeader(snakeGame.score);
    }
  }

  // Game Over screen
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
  char buf[24];
  snprintf(buf, sizeof(buf), "Score: %d", snakeGame.score);
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - (int)w / 2, cy + 8);
  tft.print(buf);
  delay(2000);
}
