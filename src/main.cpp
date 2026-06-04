/*
 * HeadTracker for Seeed XIAO ESP32C6
 * Inspired by https://github.com/headtracker/HeadTracker (GPL-3.0)
 *
 * main.cpp - Entry point + FreeRTOS task management
 *
 * Hardware:
 *   MCU:      Seeed XIAO ESP32C6
 *   IMU:      GY-BNO055 (I2C @ 0x28)
 *   Display:  SSD1306 OLED 0.91" 128×32 (I2C @ 0x3C)
 *   Button:   Center/Zero button (GPIO3, active LOW)
 *   Joystick: Deek-Robot dual-axis (VRX=GPIO4, VRY=GPIO5, no SW)
 *   PPM out:  GPIO2 → 3.5mm trainer jack tip
 *
 * FreeRTOS Tasks (all on core 0 — ESP32C6 is single-core RISC-V):
 *   senseTask    (highest prio): reads BNO055 at ~100Hz, maps channels
 *   outputTask:                  updates PPM + BLE HID at ~50Hz
 *   ioTask:                      reads button/joystick, handles events
 *   displayTask:                 updates OLED at ~10Hz
 *   serialTask:                  processes USB Serial commands
 *   webTask:                     WebSocket data stream updates
 */

#include <Arduino.h>
#include "trackersettings.h"
#include "sense.h"
#include "PPMOut.h"
#include "serial.h"
#include "webserver.h"
#include "btjoystick.h"
#include "display.h"
#include "io.h"

// ─── Task stack sizes ─────────────────────────────────────────────────────────
#define STACK_SENSE    4096
#define STACK_OUTPUT   3072
#define STACK_IO       2048
#define STACK_DISPLAY  3072
#define STACK_SERIAL   3072
#define STACK_WEB      3072

// ─── Task priorities (higher = more urgent) ───────────────────────────────────
#define PRIO_SENSE    5
#define PRIO_OUTPUT   4
#define PRIO_IO       3
#define PRIO_SERIAL   2
#define PRIO_WEB      2
#define PRIO_DISPLAY  1

// ─── Task handles ─────────────────────────────────────────────────────────────
static TaskHandle_t senseTaskHandle   = nullptr;
static TaskHandle_t outputTaskHandle  = nullptr;
static TaskHandle_t ioTaskHandle      = nullptr;
static TaskHandle_t displayTaskHandle = nullptr;
static TaskHandle_t serialTaskHandle  = nullptr;
static TaskHandle_t webTaskHandle     = nullptr;

// ─── Task: Sensor reading + channel mapping (~100Hz) ─────────────────────────
void senseTask(void*) {
    for (;;) {
        sense_update();
        vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz
    }
}

// ─── Task: Outputs (PPM + BLE HID) (~50Hz) ───────────────────────────────────
void outputTask(void*) {
    for (;;) {
        if (trkset.bleEnabled()) {
            btJoystick_update();
        }
        // PPM is driven by hardware timer ISR – nothing to call here
        // except keeping the output mode in sync with settings
        static bool lastPpmEnabled = false;
        bool shouldPpm = trkset.ppmEnabled();
        if (shouldPpm != lastPpmEnabled) {
            ppmOut_enable(shouldPpm);
            lastPpmEnabled = shouldPpm;
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz
    }
}

// ─── Task: IO events (~50Hz) ──────────────────────────────────────────────────
void ioTask(void*) {
    for (;;) {
        io_update();

        // Short press → set center
        if (btnShortEvent) {
            btnShortEvent = false;
            sense_setCenter();
            Serial.println("[IO] Center set");
        }

        // Long press → cycle OLED screen
        if (btnLongEvent) {
            btnLongEvent = false;
            display_nextScreen();
        }

        // Joystick Y → cycle screens when pushed significantly
        static int8_t lastJoyY = 0;
        if (lastJoyY == 0 && joyY > 70) {
            display_nextScreen();
        }
        if (lastJoyY == 0 && joyY < -70) {
            // Reverse cycle
            int s = display_getScreen() - 1;
            if (s < 0) s = 2;
            display_setScreen(s);
        }
        lastJoyY = joyY;

        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz
    }
}

// ─── Task: Display update (~10Hz) ────────────────────────────────────────────
void displayTask(void*) {
    for (;;) {
        display_update();
        vTaskDelay(pdMS_TO_TICKS(100)); // 10Hz
    }
}

// ─── Task: Serial JSON protocol ───────────────────────────────────────────────
void serialTask(void*) {
    for (;;) {
        serial_update();
        vTaskDelay(pdMS_TO_TICKS(5)); // ~200Hz poll (mostly idle)
    }
}

// ─── Task: WebSocket data stream ──────────────────────────────────────────────
void webTask(void*) {
    for (;;) {
        webserver_update();
        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz
    }
}

// ─── Arduino setup ───────────────────────────────────────────────────────────
void setup() {
    // USB CDC Serial
    Serial.begin(115200);
    delay(500); // Let CDC connect
    Serial.println("\n\n== HeadTracker v" FW_VERSION " ==");
    Serial.println("Board: " HW_VERSION);

    // ── Load settings ────────────────────────────────────────────────────────
    trkset.loadFromFlash();

    // ── IO init ──────────────────────────────────────────────────────────────
    io_init();

    // ── Sensor init (also sets up I2C) ───────────────────────────────────────
    sense_init();

    // ── Display init (shares I2C) ─────────────────────────────────────────────
    display_init();

    // ── PPM output ────────────────────────────────────────────────────────────
    ppmOut_init();
    if (trkset.ppmEnabled()) {
        ppmOut_enable(true);
    }

    // ── WiFi AP + WebSocket (before BLE — WiFi must claim the radio first) ───
    webserver_init();

    // ── BLE HID joystick (after WiFi — coexistence mode already active) ──────
    if (trkset.bleEnabled()) {
        btJoystick_init();
    }

    // ── Serial protocol ───────────────────────────────────────────────────────
    serial_init();

    Serial.println("[Main] Starting FreeRTOS tasks...");

    // ── Create FreeRTOS tasks ─────────────────────────────────────────────────
    // ESP32C6 is single-core RISC-V — use xTaskCreate (no core pinning)
    xTaskCreate(senseTask,   "sense",   STACK_SENSE,   nullptr, PRIO_SENSE,   &senseTaskHandle);
    xTaskCreate(outputTask,  "output",  STACK_OUTPUT,  nullptr, PRIO_OUTPUT,  &outputTaskHandle);
    xTaskCreate(ioTask,      "io",      STACK_IO,      nullptr, PRIO_IO,      &ioTaskHandle);
    xTaskCreate(displayTask, "display", STACK_DISPLAY, nullptr, PRIO_DISPLAY, &displayTaskHandle);
    xTaskCreate(serialTask,  "serial",  STACK_SERIAL,  nullptr, PRIO_SERIAL,  &serialTaskHandle);
    xTaskCreate(webTask,     "web",     STACK_WEB,     nullptr, PRIO_WEB,     &webTaskHandle);

    Serial.println("[Main] All tasks started. Running.");
}

// ─── Arduino loop (unused — all work done in tasks) ──────────────────────────
void loop() {
    // Yield to FreeRTOS scheduler
    vTaskDelay(pdMS_TO_TICKS(1000));
}
