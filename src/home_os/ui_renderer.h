#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <Arduino.h>
#include <Adafruit_ILI9341.h>

// Forward declare global variables
extern Adafruit_ILI9341 tft;
extern bool darkModeEnabled;

void renderHome();
void renderStatusBar();
void renderGameSelector(int index, const char* name, bool isSelected);

#endif // UI_RENDERER_H
