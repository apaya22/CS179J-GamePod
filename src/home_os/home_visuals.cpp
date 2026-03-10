#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

#include "home_config.h"
#include "ui_renderer.h"
#include "../include/controller/controller.h"
#include "../games/snake_game.h"
#include "../games/game_tron.h"

// =====================
// GLOBAL VARIABLES
// =====================
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST); // PINS DECLARATION

bool darkModeEnabled = false;

// State machine variables
AppState currentState = STATE_HOME;
int selectedGameIndex = 0;
unsigned long lastFrameTime = 0;

// Input state tracking - detect state changes, not continuous state
JoyDir lastJoyDir = CENTER;
bool lastButtonAPressed = false;
bool lastButtonBPressed = false;

static inline void syncHomeButtonState() {
  lastButtonAPressed = buttonPressed(BTN_A);
  lastButtonBPressed = buttonPressed(BTN_B);
}

// Rendering state - only redraw when something changes
int lastRenderedGameIndex = -1;
bool lastRenderedDarkMode = false;
AppState lastRenderedState = static_cast<AppState>(-1);

// =====================
// INPUT SYSTEM
// =====================


// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("System starting...");

  // Initialize input handler (controller pins)
  initController();

  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN);

  tft.begin();
  tft.setRotation(TFT_ROT);

  // Start at home screen
  currentState = STATE_HOME;
  selectedGameIndex = 0;
  syncHomeButtonState();
  renderHome();
}


// =====================
// INPUT HANDLING
// =====================
void handleHomeMenuInput() {
  // Update joystick state
  joystickUpdate();
  JoyDir joyDir = joystickDirection();

  // Only process joystick input when direction changes
  if (joyDir != lastJoyDir) {
    if (joyDir == LEFT) {
      // Scroll left through games
      selectedGameIndex = (selectedGameIndex - 1 + NUM_GAMES) % NUM_GAMES;
      Serial.print("Selected game: ");
      Serial.println(GAMES[selectedGameIndex].name);
    } else if (joyDir == RIGHT) {
      // Scroll right through games
      selectedGameIndex = (selectedGameIndex + 1) % NUM_GAMES;
      Serial.print("Selected game: ");
      Serial.println(GAMES[selectedGameIndex].name);
    }
    lastJoyDir = joyDir;
  }

  // DEBUG: print raw pin reads every ~500ms
  static uint32_t lastDebugPrint = 0;
  uint32_t nowDbg = millis();
  if (nowDbg - lastDebugPrint >= 500) {
    lastDebugPrint = nowDbg;
    Serial.print("BTN_A("); Serial.print(BTN_A); Serial.print(")=");
    Serial.print(digitalRead(BTN_A));
    Serial.print("  BTN_B("); Serial.print(BTN_B); Serial.print(")=");
    Serial.println(digitalRead(BTN_B));
  }

  // Check for button A press - only on state change (press event)
  bool buttonAPressed = buttonPressed(BTN_A);
  if (buttonAPressed && !lastButtonAPressed) {
    Serial.print("Starting game: ");
    Serial.println(GAMES[selectedGameIndex].name);
    currentState = GAMES[selectedGameIndex].state;
  }
  lastButtonAPressed = buttonAPressed;

  // Check for button B press - only on state change (press event)
  bool buttonBPressed = buttonPressed(BTN_B);
  if (buttonBPressed && !lastButtonBPressed) {
    darkModeEnabled = !darkModeEnabled;
    Serial.println(darkModeEnabled ? "Dark mode ON" : "Dark mode OFF");
  }
  lastButtonBPressed = buttonBPressed;
}

// =====================
// GAME LAUNCHER FUNCTIONS
// =====================
void startTronGame() {
  Serial.println("Launching TRON...");
  runTronGame();
}

void startSnakeGame() {
  Serial.println("Launching SNAKE...");
  runSnakeGame();
}

// =====================
// MAIN LOOP
// =====================
void loop() {
  unsigned long now = millis();
  if (now - lastFrameTime < 33) {
    return;
  }
  lastFrameTime = now;

  // State machine
  switch (currentState) {
    case STATE_HOME:
      // Handle input and render home menu
      handleHomeMenuInput();

      // Only redraw if something changed (game selection or dark mode)
      if (selectedGameIndex != lastRenderedGameIndex ||
          darkModeEnabled != lastRenderedDarkMode ||
          currentState != lastRenderedState) {
        renderGameSelector(selectedGameIndex, GAMES[selectedGameIndex].name, true);
        lastRenderedGameIndex = selectedGameIndex;
        lastRenderedDarkMode = darkModeEnabled;
        lastRenderedState = currentState;
      }
      break;

    case STATE_TRON:
      startTronGame();  // blocks until user presses B
      currentState = STATE_HOME;
      selectedGameIndex = 0;
      syncHomeButtonState();
      break;

    case STATE_SNAKE:
      startSnakeGame();  // blocks until game over (includes 2s game-over delay)
      currentState = STATE_HOME;
      selectedGameIndex = 0;
      syncHomeButtonState();
      break;

    default:
      currentState = STATE_HOME;
      break;
  }
}
