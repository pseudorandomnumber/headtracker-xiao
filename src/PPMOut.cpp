/*
 * HeadTracker for Seeed XIAO ESP32C6
 * Inspired by https://github.com/headtracker/HeadTracker (GPL-3.0)
 *
 * PPMOut.cpp - PPM output using ESP32 hardware timer
 *
 * Uses Arduino-ESP32 3.x timer API (required for ESP32C6 / IDF 5.x).
 *
 * Generates an 8-channel PPM signal on PIN_PPM_OUT.
 * Positive PPM: low sync pulse (300µs), then high channel pulse.
 * Total frame period = ~20ms (50Hz).
 *
 * PPM frame structure:
 *   [300µs LOW] [ch1_hi µs HIGH] [300µs LOW] [ch2_hi µs HIGH] ... 
 *   [300µs LOW] [frame_gap HIGH]
 *
 * Where ch_hi = chanUs - 300  (so total slot = chanUs µs)
 * Frame gap fills remaining time to 20000µs total.
 */

#include "PPMOut.h"
#include "trackersettings.h"

// ─── PPM timing constants (µs) ────────────────────────────────────────────────
#define PPM_SYNC_US     300      // Sync pulse width (µs)
#define PPM_FRAME_US    20000    // Total frame period (µs)
#define PPM_CH_COUNT    PPM_CHANNELS

// ─── Timer (Arduino-ESP32 3.x API) ───────────────────────────────────────────
static hw_timer_t* ppmTimer = nullptr;

// ─── State ────────────────────────────────────────────────────────────────────
static volatile bool     ppmEnabled  = false;
static volatile int      ppmChannel  = 0;     // 0-indexed channel
static volatile int      ppmPhase    = 0;     // 0 = sync LOW, 1 = channel HIGH
static volatile uint32_t ppmFrameUs  = 0;     // accumulated frame time (µs)

portMUX_TYPE ppmMux = portMUX_INITIALIZER_UNLOCKED;

// ─── ISR ─────────────────────────────────────────────────────────────────────
void IRAM_ATTR ppmTimerISR() {
    if (!ppmEnabled) {
        digitalWrite(PIN_PPM_OUT, LOW);
        return;
    }

    uint32_t nextUs;

    if (ppmPhase == 0) {
        // ── Sync pulse: output LOW for 300µs ────────────────────────────────
        digitalWrite(PIN_PPM_OUT, LOW);
        nextUs = PPM_SYNC_US;
        ppmFrameUs += PPM_SYNC_US;
        ppmPhase = 1;

    } else {
        // ── High phase ───────────────────────────────────────────────────────
        if (ppmChannel < PPM_CH_COUNT) {
            // Channel pulse: HIGH for (chanUs - 300) µs
            uint32_t chUs = trkset.chanOut[ppmChannel + 1]; // 1-indexed
            uint32_t hiUs = (chUs > PPM_SYNC_US) ? (chUs - PPM_SYNC_US) : 100;
            hiUs = (hiUs > 2200) ? 2200 : hiUs;

            digitalWrite(PIN_PPM_OUT, HIGH);
            nextUs = hiUs;
            ppmFrameUs += hiUs;
            ppmChannel++;
            ppmPhase = 0; // next: sync pulse
        } else {
            // Frame gap: HIGH for remaining time
            uint32_t gapUs = (ppmFrameUs < PPM_FRAME_US)
                             ? (PPM_FRAME_US - ppmFrameUs)
                             : PPM_SYNC_US;
            digitalWrite(PIN_PPM_OUT, HIGH);
            nextUs = gapUs;
            // Reset frame
            ppmChannel = 0;
            ppmFrameUs = 0;
            ppmPhase   = 0; // next: sync pulse
        }
    }

    // Re-arm timer for next interval (Arduino-ESP32 3.x: one-shot via timerAlarm)
    timerAlarm(ppmTimer, nextUs, false, 0);
}

// ─── Public API ───────────────────────────────────────────────────────────────
void ppmOut_init() {
    pinMode(PIN_PPM_OUT, OUTPUT);
    digitalWrite(PIN_PPM_OUT, LOW);

    // Arduino-ESP32 3.x: timerBegin(frequency_hz)
    // 1 000 000 Hz = 1µs resolution
    ppmTimer = timerBegin(1000000);
    timerAttachInterrupt(ppmTimer, &ppmTimerISR);
    // Don't arm yet — wait for ppmOut_enable(true)

    Serial.println("[PPMOut] Initialized on GPIO" + String(PIN_PPM_OUT));
}

void ppmOut_enable(bool en) {
    portENTER_CRITICAL(&ppmMux);
    ppmEnabled  = en;
    ppmChannel  = 0;
    ppmFrameUs  = 0;
    ppmPhase    = 0;
    portEXIT_CRITICAL(&ppmMux);

    if (en) {
        // Start with first sync pulse
        timerAlarm(ppmTimer, PPM_SYNC_US, false, 0);
        Serial.println("[PPMOut] Enabled");
    } else {
        timerStop(ppmTimer);
        digitalWrite(PIN_PPM_OUT, LOW);
        Serial.println("[PPMOut] Disabled");
    }
}

bool ppmOut_isEnabled() {
    return ppmEnabled;
}
