/*
 * HeadTracker for Seeed XIAO ESP32C6
 * display.cpp - SSD1306 OLED status display
 *
 * Screen layout (128×32 default):
 *
 *  Screen 0 – Live data:
 *   Pan:  +12.4  1612µs
 *   Tlt:  -5.1   1398µs
 *   Rol:  +2.3   1523µs
 *
 *  Screen 1 – Calibration:
 *   BNO055 Cal
 *   Sys:3 Gyr:3
 *   Acc:3 Mag:2
 *
 *  Screen 2 – Status:
 *   HeadTracker
 *   PPM:ON BLE:OK
 *   WiFi:192.168.4.1
 */

#include "display.h"
#include "trackersettings.h"
#include "sense.h"
#include "btjoystick.h"
#include "PPMOut.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ─── Display configuration ────────────────────────────────────────────────────
// Change to 64, 48 if using the 0.66" display
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  32
#define OLED_ADDR      0x3C
#define OLED_RESET     -1    // no reset pin

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static bool displayReady = false;
static int  currentScreen = 0;
static const int NUM_SCREENS = 3;

// ─── Init ─────────────────────────────────────────────────────────────────────
void display_init() {
    // Wire already begun in sense_init()
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[Display] SSD1306 not found – display disabled");
        displayReady = false;
        return;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("HeadTracker");
    display.println("Initializing...");
    display.display();
    displayReady = true;
    Serial.println("[Display] SSD1306 initialized");
}

// ─── Screen renderers ─────────────────────────────────────────────────────────
static void drawScreen0() {
    // Live angles + channel µs values
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);

    char buf[32];
    snprintf(buf, sizeof(buf), "Pan:%+6.1f %4d", (float)sensePan,  trkset.chanOut[trkset.panCh()]);
    display.println(buf);

    snprintf(buf, sizeof(buf), "Tlt:%+6.1f %4d", (float)senseTilt, trkset.chanOut[trkset.tltCh()]);
    display.println(buf);

    snprintf(buf, sizeof(buf), "Rol:%+6.1f %4d", (float)senseRoll, trkset.chanOut[trkset.rllCh()]);
    display.println(buf);

    // Show tiny status on last line (only on 128×32 fits 4 lines)
    if (SCREEN_HEIGHT >= 32) {
        display.setCursor(0, 24);
        display.setTextSize(1);
        const char* ppm = ppmOut_isEnabled()          ? "PPM " : "--- ";
        const char* ble = btJoystick_connected()      ? "BLE " : "ble ";
        const char* sns = senseReady                  ? "CAL" : "ERR";
        display.printf("%s%s%s %d/%d/%d/%d",
                        ppm, ble, sns, calSys, calGyro, calAccel, calMag);
    }

    display.display();
}

static void drawScreen1() {
    // Calibration status
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("BNO055 Calibration");
    char buf[32];
    snprintf(buf, sizeof(buf), "Sys:%d  Gyro:%d", calSys, calGyro);
    display.println(buf);
    snprintf(buf, sizeof(buf), "Acc:%d  Mag:%d",  calAccel, calMag);
    display.println(buf);

    // Show hint on 32px displays
    if (SCREEN_HEIGHT >= 32) {
        display.setCursor(0, 24);
        if (calSys < 3 || calMag < 2) {
            display.print("Move head slowly");
        } else {
            display.print("Cal OK!");
        }
    }
    display.display();
}

static void drawScreen2() {
    // Status / info
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("HeadTracker v" FW_VERSION);

    char buf[32];
    snprintf(buf, sizeof(buf), "PPM:%s BLE:%s",
             ppmOut_isEnabled()       ? "ON"  : "OFF",
             btJoystick_connected() ? "CON" : "---");
    display.println(buf);

    display.println("WiFi:192.168.4.1");

    if (SCREEN_HEIGHT >= 32) {
        display.setCursor(0, 24);
        display.print(trkset.apSSID());
    }
    display.display();
}

// ─── Update (call at ~10Hz) ───────────────────────────────────────────────────
void display_update() {
    if (!displayReady) return;

    switch (currentScreen) {
        case 0: drawScreen0(); break;
        case 1: drawScreen1(); break;
        case 2: drawScreen2(); break;
        default: currentScreen = 0; drawScreen0(); break;
    }
}

// ─── Screen navigation ────────────────────────────────────────────────────────
void display_nextScreen() {
    currentScreen = (currentScreen + 1) % NUM_SCREENS;
}

void display_setScreen(int n) {
    if (n >= 0 && n < NUM_SCREENS) currentScreen = n;
}

int display_getScreen() {
    return currentScreen;
}
