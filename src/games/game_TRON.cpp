#include "game_tron.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>

#include "tron_visuals.h"
#include "controller/controller.h"
#include "network/tcp_client.h"
#include "secrets.h"

// ============================================
//  CONFIGURATION
//  Flash each ESP32 with a different PLAYER_ID:
//    Board 1 → PLAYER_ID 1  (CYAN  bike)
//    Board 2 → PLAYER_ID 2  (RED   bike)
//    Board 3 → PLAYER_ID 3  (GREEN bike)
// ============================================

#define PLAYER_ID            3   // change this per programmed board
#define RECONNECT_DELAY_MS   3000
#define MAX_BIKES            3
#define JOY_SEND_INTERVAL_MS 100

#if PLAYER_ID < 1 || PLAYER_ID > 3
  #error "PLAYER_ID must be 1, 2, or 3"
#endif

// ============================================
//  BIKE STATE
// ============================================

struct BikeState {
  int     x, y;
  uint8_t dir;
  bool    alive;
  bool    valid;
};

static BikeState bikes[MAX_BIKES] = {};
static bool wallsDrawn = false;
static bool gameOver   = false;

// Death animation state per bike
static int  deathFrame[MAX_BIKES]    = { -1, -1, -1 };  // -1 = not dying
static bool deathDone[MAX_BIKES]     = { false, false, false };
static uint32_t lastDeathFrameMs[MAX_BIKES] = { 0, 0, 0 };
static const uint32_t DEATH_FRAME_INTERVAL_MS = 120;  // ms between death frames

// ============================================
//  COLORS
//  Own bike is drawn WHITE
// ============================================

static const uint16_t BIKE_COLORS[MAX_BIKES] = {
  ILI9341_CYAN,   // Player 1
  ILI9341_RED,    // Player 2
  ILI9341_GREEN   // Player 3
};

static const uint16_t OWN_BIKE_COLOR = ILI9341_WHITE;

// ============================================
//  HELPERS
// ============================================

static void drawWalls(int left, int right, int top, int bottom) {
  tft.drawRect(left, top, (right - left) + 1, (bottom - top) + 1, ILI9341_WHITE);
}

// idk why they're flipped
static uint8_t dirToInt(const char* dir) {
  if (strcmp(dir, "UP")    == 0) return 1;  // visual DOWN sprite
  if (strcmp(dir, "DOWN")  == 0) return 0;  // visual UP sprite
  if (strcmp(dir, "LEFT")  == 0) return 3;  // visual RIGHT sprite
  if (strcmp(dir, "RIGHT") == 0) return 2;  // visual LEFT sprite
  return 2;
}

// Reset display-side state (FIXME: issues with reconnection)
static void resetDisplayState() {
  resetScreenBeforeStart();
  memset(trailMap, 0, sizeof(trailMap));
  for (int i = 0; i < MAX_BIKES; i++) {
    bikes[i] = { 0, 0, 3, true, false };
  }
  wallsDrawn = false;
  gameOver   = false;
  for (int i = 0; i < MAX_BIKES; i++) {
    deathFrame[i] = -1;
    deathDone[i]  = false;
    lastDeathFrameMs[i] = 0;
  }
  Serial.println("[Tron] Display state reset");
}

static void sendDir(const char* dir) {
  if (!tcp_is_connected()) return;
  tcp_send_direction(PLAYER_ID, dir);
}

static void connectToServer() {
  Serial.println("[Tron] Connecting to WiFi...");
  while (!wifi_connect(WIFI_SSID, WIFI_PASS)) {
    Serial.println("[Tron] WiFi failed, retrying...");
    delay(RECONNECT_DELAY_MS);
  }

  Serial.println("[Tron] Connecting to Tron server...");
  while (!tcp_connect(SERVER_IP, SERVER_PORT)) {
    Serial.println("[Tron] TCP failed, retrying...");
    delay(RECONNECT_DELAY_MS);
  }

  tcp_send_join(PLAYER_ID);
  Serial.printf("[Tron] Joined as Player %d\n", PLAYER_ID);
}

// ============================================
//  PACKET PROCESSING
// ============================================

static bool prevGameOver = false;

static void processPacket(const String& json) {

  StaticJsonDocument<2048> doc;

  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.print("[Tron] JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  bool curGameOver = doc["game_over"] | false;

  // if round reset
  if (prevGameOver && !curGameOver) {
    resetDisplayState();
  }
  prevGameOver = curGameOver;

  // boarder redrawn per game
  if (!wallsDrawn) {
    drawWalls(
      doc["wall_left"]   | 0,
      doc["wall_right"]  | 319,
      doc["wall_top"]    | 0,
      doc["wall_bottom"] | 239
    );
    wallsDrawn = true;
  }

  // game over & winner text
  if (curGameOver && !gameOver) {
    gameOver = true;
    const char* wt = doc["winner_text"] | "GAME OVER";
    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setCursor(60, 110);
    tft.print(wt[0] ? wt : "GAME OVER");
    Serial.printf("[Tron] Game over: %s\n", wt);
  }

  // Update every bike in the packet
  JsonArray bikesArr = doc["bikes"].as<JsonArray>();
  for (JsonObject bike : bikesArr) {
    int id = bike["id"] | 0;
    if (id < 1 || id > MAX_BIKES) continue;

    int      idx    = id - 1;
    int      newX   = bike["col"] | 0;
    int      newY   = bike["row"] | 0;
    uint8_t  newDir = dirToInt(bike["direction"] | "RIGHT");
    bool     alive  = bike["alive"] | true;

    uint16_t color  = (id == PLAYER_ID) ? OWN_BIKE_COLOR : BIKE_COLORS[idx];

    // Skip bikes that are already done with their death animation
    if (deathDone[idx]) continue;

    // Skip unchanged positions (but still allow death anim to trigger)
    if (bikes[idx].valid &&
        bikes[idx].x == newX &&
        bikes[idx].y == newY &&
        bikes[idx].dir == newDir &&
        bikes[idx].alive == alive) {
      continue;
    }

    // Detect alive→dead transition: start death animation
    if (bikes[idx].valid && bikes[idx].alive && !alive) {
      bikes[idx].alive = false;
      deathFrame[idx] = 0;
      lastDeathFrameMs[idx] = millis();
      playDeathFrame(bikes[idx].x, bikes[idx].y, bikes[idx].dir, 0, BIKE_COLORS);
      Serial.printf("[Tron] Player %d died – starting death animation\n", id);
      continue;
    }

    // If this bike is mid-death-animation, don't update its position
    if (deathFrame[idx] >= 0) continue;

    // Clear old sprite, restore any trail underneath
    if (bikes[idx].valid) {
      clearCharacter(bikes[idx].x, bikes[idx].y, bikes[idx].dir);
      restoreTrail(bikes[idx].x, bikes[idx].y, bikes[idx].dir, BIKE_COLORS);
    }

    bikes[idx] = { newX, newY, newDir, alive, true };

    if (alive) {
      drawCharacter(newX, newY, newDir, color, id);
    }
  }
}

// ============================================
//  DEATH ANIMATION TICKER
//  Call every loop iteration to advance frames
// ============================================

static void tickDeathAnimations() {
  uint32_t now = millis();
  for (int i = 0; i < MAX_BIKES; i++) {
    if (deathFrame[i] < 0 || deathDone[i]) continue;

    if (now - lastDeathFrameMs[i] >= DEATH_FRAME_INTERVAL_MS) {
      deathFrame[i]++;
      lastDeathFrameMs[i] = now;

      if (deathFrame[i] >= DEATH_FRAME_COUNT) {
        // Animation complete
        deathDone[i] = true;
        deathFrame[i] = -1;
        Serial.printf("[Tron] Player %d death animation complete\n", i + 1);
      } else {
        playDeathFrame(bikes[i].x, bikes[i].y, bikes[i].dir,
                       deathFrame[i], BIKE_COLORS);
      }
    }
  }
}

// ============================================
//  MAIN GAME FUNCTION
// ============================================

void runTronGame() {
  // Reset all game state (clears screen too)
  prevGameOver = false;
  resetDisplayState();

  // ── SPLASH SCREEN (visible while TCP connects) ────────────────────────────
  int16_t x1, y1; uint16_t w, h;

  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(4);
  tft.getTextBounds("TRON", 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((Width_Sreen - (int)w) / 2, 65);
  tft.print("TRON");

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  const char* connStatus = "Connecting to server...";
  tft.getTextBounds(connStatus, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((Width_Sreen - (int)w) / 2, 135);
  tft.print(connStatus);

  tft.setTextColor(0x8410);
  const char* hint = "Press B to return home";
  tft.getTextBounds(hint, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((Width_Sreen - (int)w) / 2, 200);
  tft.print(hint);

  // Connect while the splash screen stays visible
  connectToServer();

  // ── PLAYER ID SCREEN ──────────────────────────────────────────────────────
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(BIKE_COLORS[PLAYER_ID - 1]);
  tft.setTextSize(3);
  tft.getTextBounds("PLAYER X", 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((Width_Sreen - (int)w) / 2, 90);
  tft.printf("PLAYER %d", PLAYER_ID);

  tft.setTextColor(0x8410);
  tft.setTextSize(1);
  tft.getTextBounds(hint, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((Width_Sreen - (int)w) / 2, 160);
  tft.print(hint);

  delay(1500);

  // Clear for game arena
  resetScreenBeforeStart();

  JoyDir   lastJoyDir    = CENTER;
  uint32_t lastJoySendMs = 0;

  bool lastB = buttonPressed(BTN_B);

  String tcpBuffer;

  while (true) {
    // ── Return to home  ──────────────────────────────────────────
    bool curB = buttonPressed(BTN_B);
    if (curB && !lastB) {
      Serial.println("[Tron] Returning to home menu...");
      tcp_disconnect();
      wifi_disconnect();
      resetScreenBeforeStart();
      return;
    }
    lastB = curB;

    // ── Reconnect ──────────────────────────────────────
    if (!wifi_is_connected() || !tcp_is_connected()) {
      Serial.println("[Tron] Lost connection, reconnecting...");
      tcp_disconnect();
      wifi_disconnect();
      delay(RECONNECT_DELAY_MS);
      resetDisplayState();
      connectToServer();
      continue;
    }

    uint32_t now = millis();

    // ── Joystick  ───────────────────────────────
    JoyDir dir = joystickDirection();

    if (dir != CENTER && dir != lastJoyDir && (now - lastJoySendMs >= JOY_SEND_INTERVAL_MS)) {
      switch (dir) {
        case UP:    sendDir("UP");    break;
        case DOWN:  sendDir("DOWN");  break;
        case LEFT:  sendDir("RIGHT"); break;  // axes inverted vs server (need to fix hardcode for now)
        case RIGHT: sendDir("LEFT");  break;
        default: break;
      }
      lastJoyDir    = dir;
      lastJoySendMs = now;
    }
    if (dir == CENTER) lastJoyDir = CENTER;

    // ── Drain ALL available TCP bytes  ────────────────────────────────
    while (tcp_available() > 0) {
      char c = (char)tcp_read();
      if (c == '\n') {
        if (tcpBuffer.length() > 0) {
          processPacket(tcpBuffer);
          tcpBuffer = "";
        }
      } else if (c != '\r') {
        tcpBuffer += c;
      }
    }

    // ── Advance death animations ─────────────────────────────────────
    tickDeathAnimations();
  }
}