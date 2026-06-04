/*
 * HeadTracker for Seeed XIAO ESP32C6
 * Inspired by https://github.com/headtracker/HeadTracker (GPL-3.0)
 *
 * serial.cpp - USB Serial JSON protocol handler
 *
 * Protocol (HeadTracker-configurator compatible):
 *   Each message is a single-line JSON object terminated with '\n'.
 *
 * Incoming commands (Cmd field):
 *   "Get"    → reply with all current settings as {"Cmd":"Set", ...}
 *   "Set"    → apply settings from same message
 *   "FW"     → reply with firmware/hw version
 *   "FE"     → reply with features + pin info
 *   "RstCnt" → reset center position
 *   "Flash"  → save settings to flash
 *   "Reboot" → reboot ESP32
 *   "RD"     → enable/disable real-time data stream
 *   "D--"    → stop all data streams
 */

#include "serial.h"
#include "trackersettings.h"
#include "sense.h"
#include "PPMOut.h"

#include <ArduinoJson.h>
#include <esp_system.h>

// ─── State ────────────────────────────────────────────────────────────────────
static String rxBuffer;
static bool streamPan  = false;
static bool streamTilt = false;
static bool streamRoll = false;

// Throttle data stream to ~50ms
static unsigned long lastDataMs = 0;

// ─── Internal: send data stream ──────────────────────────────────────────────
static void sendDataStream() {
    if (!streamPan && !streamTilt && !streamRoll) return;
    unsigned long now = millis();
    if (now - lastDataMs < 50) return;
    lastDataMs = now;

    JsonDocument doc;
    doc["Cmd"] = "Data";
    if (streamPan)  doc["panout"]  = trkset.chanOut[trkset.panCh()];
    if (streamTilt) doc["tiltout"] = trkset.chanOut[trkset.tltCh()];
    if (streamRoll) doc["rollout"] = trkset.chanOut[trkset.rllCh()];
    doc["panoff"]  = (float)sensePan;
    doc["tiltoff"] = (float)senseTilt;
    doc["rolloff"] = (float)senseRoll;
    doc["calSys"]  = calSys;
    doc["calGyr"]  = calGyro;
    doc["calAcc"]  = calAccel;
    doc["calMag"]  = calMag;

    String out;
    serializeJson(doc, out);
    Serial.println(out);
}

// ─── Internal: process a complete JSON line ───────────────────────────────────
static void processLine(const String& line) {
    if (line.length() < 2) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        Serial.printf("[Serial] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* cmd = doc["Cmd"] | "";

    // ── Get: reply with all settings ──────────────────────────────────────────
    if (strcmp(cmd, "Get") == 0) {
        JsonDocument reply;
        trkset.toJSON(reply);
        String out;
        serializeJson(reply, out);
        Serial.println(out);
        return;
    }

    // ── Set: apply incoming settings ──────────────────────────────────────────
    if (strcmp(cmd, "Set") == 0) {
        trkset.fromJSON(doc);
        // Echo back the full settings
        JsonDocument reply;
        trkset.toJSON(reply);
        String out;
        serializeJson(reply, out);
        Serial.println(out);
        return;
    }

    // ── FW: firmware version ──────────────────────────────────────────────────
    if (strcmp(cmd, "FW") == 0) {
        JsonDocument reply;
        reply["Cmd"]  = "FW";
        reply["Vers"] = FW_VERSION;
        reply["Hard"] = HW_VERSION;
        reply["Git"]  = GIT_VERSION;
        String out;
        serializeJson(reply, out);
        Serial.println(out);
        return;
    }

    // ── FE: features + pins ───────────────────────────────────────────────────
    if (strcmp(cmd, "FE") == 0) {
        JsonDocument reply;
        reply["Cmd"] = "FE";
        JsonArray feat = reply["FEAT"].to<JsonArray>();
        feat.add("PPM_OUT");
        feat.add("BLE_HID");
        feat.add("WIFI_CFG");
        feat.add("SSD1306");
        feat.add("BNO055");
        JsonObject pins = reply["PINS"].to<JsonObject>();
        pins["PPM"]    = PIN_PPM_OUT;
        pins["BTN"]    = PIN_CENTER_BTN;
        pins["JOY_X"]  = PIN_JOY_X;
        pins["JOY_Y"]  = PIN_JOY_Y;
        pins["SDA"]    = PIN_I2C_SDA;
        pins["SCL"]    = PIN_I2C_SCL;
        String out;
        serializeJson(reply, out);
        Serial.println(out);
        return;
    }

    // ── RstCnt: reset center ──────────────────────────────────────────────────
    if (strcmp(cmd, "RstCnt") == 0) {
        sense_setCenter();
        JsonDocument reply;
        reply["Cmd"] = "RstCnt";
        reply["OK"]  = true;
        String out;
        serializeJson(reply, out);
        Serial.println(out);
        return;
    }

    // ── Flash: save to flash ──────────────────────────────────────────────────
    if (strcmp(cmd, "Flash") == 0) {
        trkset.saveToFlash();
        JsonDocument reply;
        reply["Cmd"] = "Flash";
        reply["OK"]  = true;
        String out;
        serializeJson(reply, out);
        Serial.println(out);
        return;
    }

    // ── Reboot ────────────────────────────────────────────────────────────────
    if (strcmp(cmd, "Reboot") == 0) {
        Serial.println("{\"Cmd\":\"Reboot\",\"OK\":true}");
        delay(200);
        esp_restart();
        return;
    }

    // ── RD: enable/disable data streams ───────────────────────────────────────
    if (strcmp(cmd, "RD") == 0) {
        if (doc["panout"].is<bool>())  streamPan  = doc["panout"];
        if (doc["tiltout"].is<bool>()) streamTilt = doc["tiltout"];
        if (doc["rollout"].is<bool>()) streamRoll = doc["rollout"];
        return;
    }

    // ── D--: stop all streams ─────────────────────────────────────────────────
    if (strcmp(cmd, "D--") == 0) {
        streamPan = streamTilt = streamRoll = false;
        return;
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────
void serial_init() {
    // USB CDC Serial is started in main.cpp
    rxBuffer.reserve(256);
    Serial.println("[Serial] Protocol handler ready");
}

void serial_update() {
    // Read incoming bytes into buffer
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (rxBuffer.length() > 0) {
                processLine(rxBuffer);
                rxBuffer = "";
            }
        } else {
            rxBuffer += c;
            if (rxBuffer.length() > 1024) rxBuffer = ""; // safety flush
        }
    }

    // Send data stream if active
    sendDataStream();
}

void serial_sendJSON(const char* json) {
    Serial.println(json);
}
