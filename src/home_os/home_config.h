#ifndef HOME_CONFIG_H
#define HOME_CONFIG_H

#include <Arduino.h>

// ========================
// DISPLAY CONFIGUARTION
// ========================
static const uint8_t TFT_ROT = 1;  // Rotated 90 degrees right for horizontal
static const uint8_t TS_ROT  = 1;
static const int SCREEN_W = 320;   // Swapped for horizontal orientation
static const int SCREEN_H = 240;

// =====================
// DISPLAY PIN CONFIGURATION
// =====================
#define TFT_CS    4
#define TFT_DC    15
#define TFT_RST   9
#define SCLK_PIN  12
#define MOSI_PIN  11
#define MISO_PIN  13

// ====================================
// GAMEPAD PIN CONFIGURATION (TODO:)
// ====================================

// =====================
// WIFI SETTINGS (TODO:)
// =====================


// =====================
// UI STATES
// =====================
enum AppState {
  STATE_BOOT = 0,      // Boot screen
  STATE_BOOT_FADE = 4, // Boot screen fade-out transition
  STATE_HOME = 1,      // Home menu screen
  STATE_TRON = 2,      // TRON game
  STATE_SNAKE = 3,     // Snake game
  STATE_TETRIS = 5     // Tetris game
};

// Shared UI state that persists across home and games.
extern bool darkModeEnabled;

// =====================
// GAME DEFINITIONS
// =====================
struct Game {
  const char* name;
  AppState state;
};

static const Game GAMES[] = {
  {"TRON", STATE_TRON},
  {"SNAKE", STATE_SNAKE},
  {"TETRIS", STATE_TETRIS}
};
static const int NUM_GAMES = 3;

// =====================
// UI THEME - COLORS
// =====================
#define GAMEPOD_GREY   0xD6BA
#define GAMEPOD_BLUE   0x033F
#define GAMEPOD_DARK   0x2104
#define GAMEPOD_WHITE  0xFFFF
#define GAMEPOD_RED    0xF800
#define DARK_BG        0x0000
#define DARK_TEXT      0xFFFF

// =====================
// GAME LIST (TODO:)
// =====================


#endif // HOME_CONFIG_H