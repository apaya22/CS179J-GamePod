#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <Arduino.h>

// ----------------- Buttons -----------------
#define BTN_A 40
#define BTN_B 41
#define BTN_C 42
#define BTN_D 2

// ----------------- Joystick ADC pins -----------------
#define JOY_X 8
#define JOY_Y 18

// ----------------- Direction enum -----------------
enum JoyDir : uint8_t { CENTER=0, UP=1, RIGHT=2, DOWN=3, LEFT=4 };

// ----------------- Internal calibration state -----------------
// (header-only static state is fine if included once)
static int joy_cx = 2048;
static int joy_cy = 2048;

static int joy_min_x = 4095, joy_max_x = 0;
static int joy_min_y = 4095, joy_max_y = 0;

static bool joy_calibrated = false;
static uint32_t joy_calib_start_ms = 0;
static uint32_t joy_last_update_ms = 0;

// Tunables
static const uint16_t JOY_ADC_MAX = 4095;
static const uint16_t CALIBRATION_MS = 800;   // keep stick centered for ~0.8s after boot
static const int DEADZONE_BASE = 250;         // minimum deadzone
static const int DEADZONE_PCT = 12;           // % of full travel used as deadzone (auto-sized)
static const bool INVERT_Y = false;           // set true if UP/DOWN swapped

inline void initController() {
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_C, INPUT_PULLUP);
  pinMode(BTN_D, INPUT_PULLUP);

  // ADC settings
  analogReadResolution(12); // 0..4095
  // analogSetAttenuation(ADC_11db); // optional (ESP32 core dependent)

  joy_calib_start_ms = millis();
  joy_last_update_ms = millis();
  joy_calibrated = false;

  // start min/max in sane state
  joy_min_x = 4095; joy_max_x = 0;
  joy_min_y = 4095; joy_max_y = 0;
}

// pressed = LOW with INPUT_PULLUP
inline bool buttonPressed(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

inline int readJoyXRaw() { return analogRead(JOY_X); }
inline int readJoyYRaw() { return analogRead(JOY_Y); }

// Call often (each loop). It will:
// - calibrate center for the first CALIBRATION_MS while you keep stick released
// - keep min/max updated over time (so thresholds fit your actual stick)
inline void joystickUpdate() {
  int x = readJoyXRaw();
  int y = readJoyYRaw();

  // Track min/max always (slow drift OK)
  if (x < joy_min_x) joy_min_x = x;
  if (x > joy_max_x) joy_max_x = x;
  if (y < joy_min_y) joy_min_y = y;
  if (y > joy_max_y) joy_max_y = y;

  // During startup, average the center
  uint32_t now = millis();
  if (!joy_calibrated) {
    // simple running average during calibration window
    static long sumx = 0, sumy = 0;
    static int n = 0;

    sumx += x; sumy += y; n++;

    if (now - joy_calib_start_ms >= CALIBRATION_MS) {
      joy_cx = (int)(sumx / max(1, n));
      joy_cy = (int)(sumy / max(1, n));
      joy_calibrated = true;

      // reset sums for safety if called again
      sumx = 0; sumy = 0; n = 0;
    }
  }

  joy_last_update_ms = now;
}

// Compute a deadzone based on observed travel + a base minimum
inline int joystickDeadzoneX() {
  int range = max(1, joy_max_x - joy_min_x);
  int dz = (range * DEADZONE_PCT) / 100;
  return max(DEADZONE_BASE, dz);
}
inline int joystickDeadzoneY() {
  int range = max(1, joy_max_y - joy_min_y);
  int dz = (range * DEADZONE_PCT) / 100;
  return max(DEADZONE_BASE, dz);
}

// Returns direction using dominant axis (no diagonals)
inline JoyDir joystickDirection() {
  joystickUpdate();

  int x = readJoyXRaw();
  int y = readJoyYRaw();

  int dx = x - joy_cx;
  int dy = y - joy_cy;
  if (INVERT_Y) dy = -dy;

  int dzx = joystickDeadzoneX();
  int dzy = joystickDeadzoneY();

  // If within deadzone on both axes -> CENTER
  if (abs(dx) < dzx && abs(dy) < dzy) return CENTER;

  // Pick the axis pushed more
  if (abs(dx) > abs(dy)) {
    return (dx > 0) ? RIGHT : LEFT;
  } else {
    // typical: pushing up makes y smaller on many sticks
    // Here we treat dy < 0 as UP if not inverted
    return (dy > 0) ? DOWN : UP;
  }
}

// Optional: normalized values in [-1000, 1000] for smoother control
inline int joystickNormX() {
  joystickUpdate();
  int x = readJoyXRaw();
  int dx = x - joy_cx;
  int dz = joystickDeadzoneX();
  if (abs(dx) < dz) return 0;

  // scale to [-1000,1000] based on observed travel
  int leftSpan  = max(1, joy_cx - joy_min_x);
  int rightSpan = max(1, joy_max_x - joy_cx);
  if (dx < 0) return (dx * 1000) / leftSpan;
  else        return (dx * 1000) / rightSpan;
}

inline int joystickNormY() {
  joystickUpdate();
  int y = readJoyYRaw();
  int dy = y - joy_cy;
  if (INVERT_Y) dy = -dy;

  int dz = joystickDeadzoneY();
  if (abs(dy) < dz) return 0;

  int upSpan   = max(1, joy_cy - joy_min_y);
  int downSpan = max(1, joy_max_y - joy_cy);
  if (dy < 0) return (dy * 1000) / upSpan;
  else        return (dy * 1000) / downSpan;
}

#endif // CONTROLLER_H
