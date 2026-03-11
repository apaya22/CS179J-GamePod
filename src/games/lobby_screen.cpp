#include "lobby_screen.h"
#include "../home_os/home_config.h"
#include "../include/controller/controller.h"
#include "../include/network/tcp_client.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>

extern Adafruit_ILI9341 tft;

// ============================================
//  LAYOUT CONSTANTS
// ============================================

#define MAX_SLOTS 3

// Horizontal center of each player column (3 equal thirds of 320px)
static const int COL_CX[MAX_SLOTS] = { 53, 160, 267 };

// Colors per player slot (index 0=P1, 1=P2, 2=P3)
static const uint16_t SLOT_COLORS[MAX_SLOTS] = {
    ILI9341_CYAN,   // P1
    ILI9341_RED,    // P2
    ILI9341_GREEN,  // P3
};

static const uint16_t INACTIVE_CLR = 0x4208;   // dark grey

// ============================================
//  TRACKED STATE  (only redraw when changed)
// ============================================

struct SlotState { bool active; bool ready; };

static SlotState g_slots[MAX_SLOTS];
static char      g_state[16];       // "waiting" | "countdown"
static int       g_readySecs;       // integer seconds left in ready window
static int       g_cdSecs;          // integer seconds left in countdown
static bool      g_isSpectating;    // track if current player is spectating

// ============================================
//  DRAW HELPERS
// ============================================

static void centerTextAt(const char* txt, int y, uint8_t sz, uint16_t color) {
    tft.setTextSize(sz);
    tft.setTextColor(color);
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((SCREEN_W - (int)w) / 2, y);
    tft.print(txt);
}

// Draw one player column
// cx = horizontal center, idx = 0-based slot index.
static void drawSlot(int idx) {
    int cx = COL_CX[idx];
    bool active = g_slots[idx].active;
    bool ready  = g_slots[idx].ready;

    uint16_t bikeClr   = active ? SLOT_COLORS[idx] : INACTIVE_CLR;
    uint16_t bubbleClr = !active ? INACTIVE_CLR : (ready ? ILI9341_GREEN : ILI9341_RED);

    // ── Bike rectangle ──────────────────────────────────────────
    tft.fillRect(cx - 22, 50, 44, 36, ILI9341_BLACK);          // erase
    tft.fillRoundRect(cx - 20, 52, 40, 32, 5, bikeClr);        // bike body

    // Player label inside the box
    char lbl[4];
    snprintf(lbl, sizeof(lbl), "P%d", idx + 1);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_BLACK);
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(lbl, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(cx - (int)w / 2, 62);
    tft.print(lbl);

    // ── Ready bubble ────────────────────────────────────────────
    tft.fillRect(cx - 10, 92, 20, 20, ILI9341_BLACK);
    tft.fillCircle(cx, 101, 9, bubbleClr);

    // ── Status text ─────────────────────────────────────────────
    tft.fillRect(cx - 34, 116, 68, 12, ILI9341_BLACK);
    const char* statusTxt  = !active ? "---" : (ready ? "READY" : "NOT RDY");
    uint16_t    statusColor = !active ? INACTIVE_CLR : (ready ? ILI9341_GREEN : ILI9341_RED);
    tft.setTextSize(1);
    tft.setTextColor(statusColor);
    tft.getTextBounds(statusTxt, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(cx - (int)w / 2, 117);
    tft.print(statusTxt);
}

static void drawTimer() {
    tft.fillRect(0, 138, SCREEN_W, 28, ILI9341_BLACK);

    int activeCount = 0;
    for (int i = 0; i < MAX_SLOTS; i++) if (g_slots[i].active) activeCount++;

    if (strcmp(g_state, "countdown") == 0) {
        char buf[28];
        snprintf(buf, sizeof(buf), "Starting in  %d", g_cdSecs);
        centerTextAt(buf, 142, 2, ILI9341_YELLOW);
    } else if (activeCount < 2) {
        centerTextAt("Waiting for players...", 142, 2, 0x8410);
    } else if (g_readySecs > 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Ready up!   %ds", g_readySecs);
        centerTextAt(buf, 142, 2, ILI9341_WHITE);
    } else {
        centerTextAt("Press A to ready up", 142, 2, ILI9341_WHITE);
    }
}

static void drawFull() {
    tft.fillScreen(ILI9341_BLACK);
    centerTextAt("TRON", 10, 3, ILI9341_CYAN);

    for (int i = 0; i < MAX_SLOTS; i++)
        drawSlot(i);

    drawTimer();
    centerTextAt("A: Ready / Unready", 178, 1, ILI9341_WHITE);
    centerTextAt("C: Toggle Spectate", 185, 1, ILI9341_YELLOW);
    centerTextAt("B: Return home",     193, 1, 0x8410);
}

// ============================================
//  MAIN LOBBY LOOP
// ============================================

bool runLobbyScreen(int playerId, String& pendingPacket) {
    // Reset local state
    for (int i = 0; i < MAX_SLOTS; i++) g_slots[i] = { false, false };
    strncpy(g_state, "waiting", sizeof(g_state) - 1);
    g_readySecs = 10;
    g_cdSecs    = 5;
    g_isSpectating = false;

    drawFull();

    bool   lastB   = buttonPressed(BTN_B);
    bool   lastA   = buttonPressed(BTN_A);
    bool   lastC   = buttonPressed(BTN_C);
    String tcpBuf;

    while (true) {

        // ── B button: exit to home ───────────────────────────────
        bool curB = buttonPressed(BTN_B);
        if (curB && !lastB) {
            if (tcp_is_connected()) {
                char msg[16];
                snprintf(msg, sizeof(msg), "LEAVE:%d", playerId);
                tcp_send(msg);
            }
            return false;
        }
        lastB = curB;

        // ── A button: toggle ready (edge-detect) ─────────────────
        bool curA = buttonPressed(BTN_A);
        if (curA && !lastA && tcp_is_connected()) {
            char msg[16];
            snprintf(msg, sizeof(msg), "READY:%d", playerId);
            tcp_send(msg);
        }
        lastA = curA;

        // ── X button: toggle spectate mode (edge-detect) ──────────
        bool curC = buttonPressed(BTN_C);
        if (curC && !lastC && tcp_is_connected()) {
            char msg[32];
            snprintf(msg, sizeof(msg), "TOGGLE_SPECTATE:%d", playerId);
            tcp_send(msg);
        }
        lastC = curC;

        // ── Drain TCP ────────────────────────────────────────────
        while (tcp_available() > 0) {
            char c = (char)tcp_read();
            if (c == '\n') {
                if (tcpBuf.length() == 0) continue;

                StaticJsonDocument<512> doc;
                deserializeJson(doc, tcpBuf);
                const char* type = doc["type"] | "";

                // Non-lobby packet → game is starting
                if (strcmp(type, "lobby") != 0) {
                    bool isGameOver = doc["game_over"] | false;
                    if (!isGameOver) {
                        isGameOver = tcpBuf.indexOf("\"game_over\":true")  >= 0 ||
                                     tcpBuf.indexOf("\"game_over\": true") >= 0;
                    }
                    if (!isGameOver) {
                        pendingPacket = tcpBuf;
                        return true;
                    }
                    tcpBuf = "";
                    continue;
                }

                // ── Parse lobby packet ───────────────────────────
                const char* st     = doc["state"]         | "waiting";
                float       rdyF   = doc["ready_secs"]    | 0.0f;
                float       cdF    = doc["countdown_secs"] | 0.0f;
                int         rdyInt = (int)rdyF + 1;   // ceil for display
                int         cdInt  = (int)cdF + 1;

                bool stateChanged = (strcmp(st, g_state) != 0);

                // Update slot states, track if anything changed
                bool slotsChanged = false;
                JsonArray arr = doc["slots"].as<JsonArray>();
                for (JsonObject slot : arr) {
                    int  id     = (slot["id"] | 0) - 1;   // convert to 0-based
                    if (id < 0 || id >= MAX_SLOTS) continue;
                    bool active = slot["active"] | false;
                    bool ready  = slot["ready"]  | false;

                    // Track if current player is spectating
                    if (id + 1 == playerId) {
                        bool wasSpectating = g_isSpectating;
                        g_isSpectating = !active;
                        if (wasSpectating != g_isSpectating) {
                            slotsChanged = true;  // force redraw if spectating status changed
                        }
                    }

                    if (active != g_slots[id].active || ready != g_slots[id].ready) {
                        g_slots[id] = { active, ready };
                        slotsChanged = true;
                    }
                }

                bool timerChanged = false;
                if (strcmp(st, "countdown") == 0) {
                    if (cdInt != g_cdSecs) { g_cdSecs = cdInt; timerChanged = true; }
                } else {
                    if (rdyInt != g_readySecs) { g_readySecs = rdyInt; timerChanged = true; }
                }

                if (stateChanged) {
                    strncpy(g_state, st, sizeof(g_state) - 1);
                    drawFull();   // full repaint on state transition
                } else {
                    if (slotsChanged) {
                        for (int i = 0; i < MAX_SLOTS; i++)
                            drawSlot(i);
                    }

                    // update timer on count down or if player slots status change
                    if (timerChanged || slotsChanged)
                        drawTimer();
                }

                tcpBuf = "";
            } else if (c != '\r') {
                tcpBuf += c;
            }
        }
    }
}
