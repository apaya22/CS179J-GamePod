#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// =====================
// TODO Please Read
// =====================
/*
 * still need to tweak some bugs with left over pixels from turning and when and where does the colision actually happen
 * from the bike to the trail, the trail could also be thicker so we shoul decide and actually map out the game
 * for example do we want a trail to be 3 pixels thick or keep it as one, no matter what it needs to be a odd number
 * our bike will also reflect off that and we should always have the head of the bike be the start of colision which is not 
 * happening right now but that is an easy fix. Lastly if the bike turns and its edges clip a trial how do we want to flush that out
 * these are all questions that should be answered before the next steps
 */

/*
 * Tron test game for ILI9341 (240x320)
 * Same logic as the old 128x128 ST7735 version.
 * Adds a bike-shaped head sprite that rotates with direction.
 */

// =====================
// DISPLAY CONFIGURATION
// =====================
static const int W = 320;
static const int H = 240;
static const uint8_t TFT_ROT = 1; //1 is for horzontal depending on 0/1/2/3 u will need to switch the w and h

// =====================
// DISPLAY PIN CONFIGURATION (your new wiring)
// =====================
#define TFT_CS    4
#define TFT_DC    15
#define TFT_RST   9
#define SCLK_PIN  12
#define MOSI_PIN  11
#define MISO_PIN  13

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

enum Dir : uint8_t { UP=0, RIGHT=1, DOWN=2, LEFT=3 };

struct Player {
  int x, y;
  Dir dir;
  uint16_t color;
  bool alive;
};

Player p1;

// Occupancy grid: W*H bits
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

// =====================
// BIKE SPRITE (procedural)
// =====================
// We treat (x,y) as the "center" of the bike. We draw a small bike silhouette using
// rectangles/pixels. This is fast and avoids bitmaps.
static const int BIKE_HALF_W = 5; // sprite ~11 wide when horizontal
static const int BIKE_HALF_H = 3; // sprite ~7 high when horizontal

// erase old sprite by clearing its bounding box, then restore the trail center pixel
void eraseBikeSprite(int x, int y, Dir d, uint16_t trailColor) {
  // bounding box depends on orientation
  int x0, y0, w, h;
  if (d == LEFT || d == RIGHT) {
    x0 = x - BIKE_HALF_W;  y0 = y - BIKE_HALF_H;
    w  = 2*BIKE_HALF_W + 1; h = 2*BIKE_HALF_H + 1;
  } else {
    x0 = x - BIKE_HALF_H;  y0 = y - BIKE_HALF_W;
    w  = 2*BIKE_HALF_H + 1; h = 2*BIKE_HALF_W + 1;
  }

  // Redraw background/trail using occupancy grid (so trail stays intact)
  for (int yy = y0; yy < y0 + h; yy++) {
    if (yy < 0 || yy >= H) continue;
    for (int xx = x0; xx < x0 + w; xx++) {
      if (xx < 0 || xx >= W) continue;

      // If the trail exists here, draw it; otherwise draw background.
      tft.drawPixel(xx, yy, occGet(xx, yy) ? trailColor : ILI9341_BLACK);
    }
  }
}

void drawBikeSprite(int x, int y, Dir d, uint16_t c) {
  // A tiny "bike": cockpit + body + two wheels.
  // Drawn differently by direction.

  if (d == RIGHT) {
    // wheels
    tft.drawPixel(x-4, y-2, c);
    tft.drawPixel(x-4, y+2, c);
    tft.drawPixel(x+4, y-2, c);
    tft.drawPixel(x+4, y+2, c);

    // body
    tft.drawFastHLine(x-3, y, 7, c);      // main line
    tft.drawFastHLine(x-2, y-1, 5, c);    // thickness
    tft.drawFastHLine(x-2, y+1, 5, c);

    // cockpit nose
    tft.drawPixel(x+5, y, c);
    tft.drawPixel(x+5, y-1, c);
    tft.drawPixel(x+5, y+1, c);

  } else if (d == LEFT) {
    // wheels
    tft.drawPixel(x-4, y-2, c);
    tft.drawPixel(x-4, y+2, c);
    tft.drawPixel(x+4, y-2, c);
    tft.drawPixel(x+4, y+2, c);

    // body
    tft.drawFastHLine(x-3, y, 7, c);
    tft.drawFastHLine(x-2, y-1, 5, c);
    tft.drawFastHLine(x-2, y+1, 5, c);

    // cockpit nose (to the left)
    tft.drawPixel(x-5, y, c);
    tft.drawPixel(x-5, y-1, c);
    tft.drawPixel(x-5, y+1, c);

  } else if (d == UP) {
    // wheels
    tft.drawPixel(x-2, y-4, c);
    tft.drawPixel(x+2, y-4, c);
    tft.drawPixel(x-2, y+4, c);
    tft.drawPixel(x+2, y+4, c);

    // body
    tft.drawFastVLine(x,   y-3, 7, c);
    tft.drawFastVLine(x-1, y-2, 5, c);
    tft.drawFastVLine(x+1, y-2, 5, c);

    // cockpit nose (up)
    tft.drawPixel(x,   y-5, c);
    tft.drawPixel(x-1, y-5, c);
    tft.drawPixel(x+1, y-5, c);

  } else { // DOWN
    // wheels
    tft.drawPixel(x-2, y-4, c);
    tft.drawPixel(x+2, y-4, c);
    tft.drawPixel(x-2, y+4, c);
    tft.drawPixel(x+2, y+4, c);

    // body
    tft.drawFastVLine(x,   y-3, 7, c);
    tft.drawFastVLine(x-1, y-2, 5, c);
    tft.drawFastVLine(x+1, y-2, 5, c);

    // cockpit nose (down)
    tft.drawPixel(x,   y+5, c);
    tft.drawPixel(x-1, y+5, c);
    tft.drawPixel(x+1, y+5, c);
  }

  // ensure center is always on
  tft.drawPixel(x, y, c);
}

// Fake input (same as old)
void fakeInput(Player &p) {
  static uint32_t lastTurn = 0;
  if (millis() - lastTurn > 1000) {
    lastTurn = millis();

    int r = random(0, 3); // 0 straight, 1 left, 2 right
    if (r == 1) p.dir = (Dir)((p.dir + 3) & 3);
    else if (r == 2) p.dir = (Dir)((p.dir + 1) & 3);
  }
}

void resetGame() {
  gameOver = false;
  occClearAll();
  tft.fillScreen(ILI9341_BLACK);

  p1.x = W / 4;
  p1.y = H / 2;
  p1.dir = RIGHT;
  p1.color = ILI9341_CYAN;
  p1.alive = true;

  occSet(p1.x, p1.y);
  tft.drawPixel(p1.x, p1.y, p1.color);

  // draw initial bike head
  drawBikeSprite(p1.x, p1.y, p1.dir, p1.color);
}

void drawGameOver() {
  int sw = tft.width();
  int sh = tft.height();

  tft.fillRect(0, sh/2 - 20, sw, 40, ILI9341_BLACK);
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(2);

  const char *msg = "GAME OVER";
  int16_t x1, y1;
  uint16_t tw, th;
  tft.getTextBounds(msg, 0, 0, &x1, &y1, &tw, &th);

  tft.setCursor((sw - (int)tw)/2, (sh - (int)th)/2);
  tft.print(msg);
}

void stepPlayer(Player &p) {
  if (!p.alive) return;

  int oldx = p.x, oldy = p.y;
  Dir oldDir = p.dir;

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

  // trail collision (trail is 1px wide in occ[])
  if (occGet(nx, ny)) {
    p.alive = false;
    gameOver = true;
    return;
  }

  // erase old bike sprite and restore trail pixel at its center
  eraseBikeSprite(oldx, oldy, oldDir, p.color);

  // then move and draw new trail + bike
  p.x = nx; p.y = ny;
  occSet(p.x, p.y);
  tft.drawPixel(p.x, p.y, p.color);
  drawBikeSprite(p.x, p.y, p.dir, p.color);
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, TFT_CS);
  tft.begin();
  tft.setRotation(TFT_ROT);

  resetGame();
}

void loop() {
  if (gameOver) {
    static bool shown = false;
    if (!shown) { drawGameOver(); shown = true; }

    static uint32_t overAt = 0;
    if (overAt == 0) overAt = millis();
    if (millis() - overAt > 2000) { overAt = 0; shown = false; resetGame(); }

    delay(10);
    return;
  }

  uint32_t now = millis();
  if (now - lastTick < TICK_MS) return;
  lastTick = now;

  fakeInput(p1);
  stepPlayer(p1);
}
