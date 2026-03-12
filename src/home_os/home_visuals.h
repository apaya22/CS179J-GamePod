#ifndef HOME_VISUALS_H
#define HOME_VISUALS_H

#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include "home_config.h"

// Exported global variables
extern Adafruit_ILI9341 tft;
extern bool darkModeEnabled;
extern AppState currentState;
extern int selectedGameIndex;

// Function declarations
void setup();
void loop();
void handleHomeMenuInput();
void startTronGame();
void startSnakeGame();

#endif // HOME_VISUALS_H
