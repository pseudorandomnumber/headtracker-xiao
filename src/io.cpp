/*
 * HeadTracker for Seeed XIAO ESP32C6
 * io.cpp - Button + Joystick input handling
 */

#include "io.h"
#include "trackersettings.h"

// ─── Exported state ───────────────────────────────────────────────────────────
volatile int8_t joyX = 0;
volatile int8_t joyY = 0;
volatile bool   btnPressed    = false;
volatile bool   btnShortEvent = false;
volatile bool   btnLongEvent  = false;

// ─── Internal state ───────────────────────────────────────────────────────────
static bool     lastBtnState  = HIGH;  // Pull-up, active LOW
static unsigned long btnPressMs = 0;
static bool     longFired     = false;

#define LONG_PRESS_MS  800
#define JOY_CENTER     2048   // ADC midpoint (0–4095)
#define JOY_DEADZONE   150    // ADC counts around center to ignore

// ─── Init ─────────────────────────────────────────────────────────────────────
void io_init() {
    // Center button: internal pull-up, active LOW
    pinMode(PIN_CENTER_BTN, INPUT_PULLUP);

    // Joystick ADC pins (ESP32C6 ADC)
    pinMode(PIN_JOY_X, INPUT);
    pinMode(PIN_JOY_Y, INPUT);

    Serial.println("[IO] Button + Joystick initialized");
}

// ─── Update (call from main loop ~50Hz) ──────────────────────────────────────
void io_update() {
    unsigned long now = millis();

    // ── Center button ────────────────────────────────────────────────────────
    bool currentBtn = digitalRead(PIN_CENTER_BTN);  // LOW = pressed

    if (lastBtnState == HIGH && currentBtn == LOW) {
        // Falling edge – button pressed
        btnPressed  = true;
        btnPressMs  = now;
        longFired   = false;
    }

    if (currentBtn == LOW && !longFired) {
        // Still held – check for long press
        if ((now - btnPressMs) >= LONG_PRESS_MS) {
            btnLongEvent = true;
            longFired    = true;
        }
    }

    if (lastBtnState == LOW && currentBtn == HIGH) {
        // Rising edge – button released
        btnPressed = false;
        if (!longFired) {
            // Only fire short event if long was NOT triggered
            btnShortEvent = true;
        }
    }

    lastBtnState = currentBtn;

    // ── Joystick ─────────────────────────────────────────────────────────────
    int rawX = analogRead(PIN_JOY_X);
    int rawY = analogRead(PIN_JOY_Y);

    // Map ADC 0–4095 to -100..+100 with deadzone
    auto adcToPercent = [](int raw) -> int8_t {
        int delta = raw - JOY_CENTER;
        if (delta > -JOY_DEADZONE && delta < JOY_DEADZONE) return 0;
        // Map ±(4096/2) to ±100
        int pct = delta * 100 / 2048;
        if (pct >  100) pct =  100;
        if (pct < -100) pct = -100;
        return (int8_t)pct;
    };

    joyX = adcToPercent(rawX);
    joyY = adcToPercent(rawY);
}
