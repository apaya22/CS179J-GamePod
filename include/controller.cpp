#include "controller.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

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


void setup() {
    Serial.begin(115200);
    delay(1000);  // Wait for serial to be ready
    Serial.println("System starting...");
    initController();
    SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, TFT_CS);
    tft.begin();
    tft.setRotation(TFT_ROT);
}

void loop() {
    if (buttonPressed(BTN_A)) {
        tft.fillScreen(ILI9341_BLUE);
    }
    else if (buttonPressed(BTN_B)) {
        tft.fillScreen(ILI9341_RED);
    }
    else if (buttonPressed(BTN_C)) {
        tft.fillScreen(ILI9341_GREENYELLOW);
    }
    else if (buttonPressed(BTN_D)) {
        tft.fillScreen(ILI9341_PINK);
    }

    JoyDir d = joystickDirection();

switch (d) {
  case UP:    tft.fillScreen(ILI9341_GREEN); break;
  case DOWN:  tft.fillScreen(ILI9341_RED); break;
  case LEFT:  tft.fillScreen(ILI9341_BLUE); break;
  case RIGHT: tft.fillScreen(ILI9341_YELLOW); break;
  default:    tft.fillScreen(ILI9341_BLACK); break;
}
delay(80);


    delay(100);
}
