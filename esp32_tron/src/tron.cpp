#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// --- Your working pinout ---
static const int TFT_SCK  = 36;
static const int TFT_MOSI = 35;
static const int TFT_MISO = -1;
static const int TFT_CS   = 10;
static const int TFT_RST  = 8;
static const int TFT_DC   = 9;

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// --- Tron game config ---
static const int W = 128;
static const int H = 128;

enum Dir : uint8_t { UP=0, RIGHT=1, DOWN=2, LEFT=3 };

struct Player {
  int x, y;
  Dir dir;
  uint16_t color;
  bool alive;
};

Player p1;

// Occupancy grid: 128*128 bits = 2048 bytes
static uint8_t occ[(W * H) / 8];

inline int idx(int x, int y) { return y * W + x; }
inline bool inBounds(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }

inline bool occGet(int x, int y) {
  int i = idx(x, y);
  return (occ[i >> 3] >> (i & 7)) & 1;
}
inline void occSet(int x, int y) {
  int i = idx(x, y);
  occ[i >> 3] |= (1 << (i & 7));
}
inline void occClearAll() { memset(occ, 0, sizeof(occ)); }

static const uint32_t TICK_MS = 40; // 25 updates/sec
uint32_t lastTick = 0;
bool gameOver = false;

// Replace later with buttons/joystick/network.
// For now: change direction automatically so you can SEE it doing something.
void fakeInput(Player &p) {
  static uint32_t lastTurn = 0;
  if (millis() - lastTurn > 1000) {
    lastTurn = millis();

    // Random choice: 0 = straight, 1 = left, 2 = right
    int r = random(0, 3);

    if (r == 1) {
      // turn left
      p.dir = (Dir)((p.dir + 3) & 3);
    } 
    else if (r == 2) {
      // turn right
      p.dir = (Dir)((p.dir + 1) & 3);
    }
    // r == 0 → go straight
  }
}


void resetGame() {
  gameOver = false;
  occClearAll();
  tft.fillScreen(ST77XX_BLACK);

  p1.x = W / 4;
  p1.y = H / 2;
  p1.dir = RIGHT;
  p1.color = ST77XX_CYAN;
  p1.alive = true;

  occSet(p1.x, p1.y);
  tft.drawPixel(p1.x, p1.y, p1.color);
}

void drawGameOver() {
  tft.fillRect(0, 50, 128, 30, ST77XX_BLACK);
  tft.setCursor(20, 60);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.print("GAME OVER");
}

void stepPlayer(Player &p) {
  if (!p.alive) return;

  int nx = p.x;
  int ny = p.y;

  switch (p.dir) {
    case UP:    ny--; break;
    case RIGHT: nx++; break;
    case DOWN:  ny++; break;
    case LEFT:  nx--; break;
  }

  // wall collision
  if (!inBounds(nx, ny)) {
    p.alive = false;
    gameOver = true;
    return;
  }

  // trail collision
  if (occGet(nx, ny)) {
    p.alive = false;
    gameOver = true;
    return;
  }

  // commit move + draw trail
  p.x = nx; p.y = ny;
  occSet(p.x, p.y);
  tft.drawPixel(p.x, p.y, p.color);
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());


  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(0);

  resetGame();
}

void loop() {
  if (gameOver) {
    static bool shown = false;
    if (!shown) { drawGameOver(); shown = true; }
    // optional: auto reset after 2 seconds
    static uint32_t overAt = 0;
    if (overAt == 0) overAt = millis();
    if (millis() - overAt > 2000) { overAt = 0; shown = false; resetGame(); }
    delay(10);
    return;
  }

  uint32_t now = millis();
  if (now - lastTick < TICK_MS) return;
  lastTick = now;

  fakeInput(p1);     // replace with real input later
  stepPlayer(p1);
}
