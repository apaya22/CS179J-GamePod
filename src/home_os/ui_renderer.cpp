#include "ui_renderer.h"
#include "home_config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

extern Adafruit_ILI9341 tft;
extern bool darkModeEnabled;

static inline void drawCenteredText(const char* txt, int y, uint8_t size, uint16_t color) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_W / 2) - (w / 2);
  tft.setCursor(x, y);
  tft.print(txt);
}

// static inline void drawGameBoyLogo(int centerX, int centerY, uint16_t headerBgColor) {
//   tft.setTextSize(1);
//   tft.setTextColor(GAMEPOD_WHITE);
//   const char* text = "GAME POD";
//   int16_t x1, y1; uint16_t w, h;
//   tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
//   tft.setCursor(centerX - (w / 2), centerY - (h / 2) + 2);
//   tft.print(text);
// }

void renderStatusBar() {
  uint16_t bgColor = darkModeEnabled ? DARK_BG : GAMEPOD_WHITE;
  uint16_t textColor = darkModeEnabled ? DARK_TEXT : GAMEPOD_DARK;

  tft.fillRect(0, SCREEN_H - 16, SCREEN_W, 16, bgColor);

  tft.setTextSize(1);
  tft.setTextColor(textColor);
  tft.setCursor(4, SCREEN_H - 13);
  tft.print("WIFI STATUS: TODO");
}

void renderHome() {

  // select colors based on display mode
  uint16_t bgColor = darkModeEnabled ? DARK_BG : GAMEPOD_GREY;
  uint16_t headerColor = darkModeEnabled ? 0x2104 : GAMEPOD_WHITE;
  uint16_t textColor = darkModeEnabled ? DARK_TEXT : GAMEPOD_DARK;

  tft.fillScreen(bgColor);

  // Header bar
  tft.fillRect(0, 0, SCREEN_W, 44, headerColor);
  //drawGameBoyLogo(SCREEN_W / 2, 12, headerColor);

  // Main welcome card
  const int cardX = 30;
  const int cardY = 90;
  const int cardW = 180;
  const int cardH = 140;

  uint16_t shadowColor = darkModeEnabled ? 0x4208 : 0x8410;
  uint16_t cardBgColor = darkModeEnabled ? 0x1082 : GAMEPOD_WHITE;
  uint16_t cardBorderColor = GAMEPOD_BLUE;

  tft.fillRoundRect(cardX + 4, cardY + 4, cardW, cardH, 18, shadowColor);
  tft.fillRoundRect(cardX, cardY, cardW, cardH, 18, cardBgColor);
  tft.drawRoundRect(cardX, cardY, cardW, cardH, 18, cardBorderColor);

  // Welcome text
  tft.fillCircle(SCREEN_W / 2, cardY + 55, 34, GAMEPOD_BLUE);
  tft.setTextColor(GAMEPOD_WHITE);
  tft.setTextSize(4);
  tft.setCursor((SCREEN_W/2) - 28, cardY + 40);
  tft.print("A");

  drawCenteredText("Hey!", cardY + cardH + 18, 2, textColor);

  tft.setTextSize(1);
  tft.setTextColor(textColor);
  tft.setCursor(20, 56);
  tft.print("GamePod V1");

  renderStatusBar();
}

void renderGameSelector(int index, const char* name) {
  uint16_t bgColor = darkModeEnabled ? DARK_BG : GAMEPOD_GREY;
  uint16_t textColor = darkModeEnabled ? DARK_TEXT : GAMEPOD_DARK;
  uint16_t accent = GAMEPOD_BLUE;

  tft.fillScreen(bgColor);
  // Header
  tft.fillRect(0, 0, SCREEN_W, 36, GAMEPOD_WHITE);
  tft.setTextSize(1);
  tft.setTextColor(textColor);
  tft.setCursor(8, 12);
  tft.print("Select Game");

  // draw arrows
  tft.setTextSize(4);
  tft.setTextColor(accent);
  tft.setCursor(8, SCREEN_H/2 - 12);
  tft.print("<");
  tft.setCursor(SCREEN_W - 32, SCREEN_H/2 - 12);
  tft.print(">");

  // show game name centered
  tft.setTextSize(3);
  tft.setTextColor(textColor);
  int16_t x1,y1; uint16_t w,h;
  tft.getTextBounds(name, 0, 0, &x1, &y1, &w, &h);
  int cx = (SCREEN_W - (int)w)/2;
  tft.setCursor(cx, SCREEN_H/2 - 18);
  tft.print(name);

  // instruction
  tft.setTextSize(1);
  tft.setTextColor(textColor);
  tft.setCursor(12, SCREEN_H - 18);
  tft.print("Move joystick left/right, press A to select");

  renderStatusBar();
}
