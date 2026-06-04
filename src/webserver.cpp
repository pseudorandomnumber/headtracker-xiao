/*
 * HeadTracker for Seeed XIAO ESP32C6
 * Inspired by https://github.com/headtracker/HeadTracker (GPL-3.0)
 *
 * webserver.cpp - WiFi AP + AsyncWebServer + WebSocket
 *
 * Serves the web UI from LittleFS and exposes a WebSocket endpoint
 * at ws://192.168.4.1/ws using the same JSON protocol as the serial port.
 *
 * WiFi AP: SSID = "HeadTracker-XXXX" (XXXX = last 2 bytes of MAC)
 *          IP   = 192.168.4.1
 *          No password
 */

#include "webserver.h"
#include "trackersettings.h"
#include "sense.h"
#include "PPMOut.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_system.h>

// ─── Server instances ─────────────────────────────────────────────────────────
static AsyncWebServer server(80);
static AsyncWebSocket  ws("/ws");

// ─── Data stream flags (per WebSocket) ───────────────────────────────────────
static bool wsStreamPan  = false;
static bool wsStreamTilt = false;
static bool wsStreamRoll = false;
static unsigned long wsLastDataMs = 0;

// ─── Forward declarations ─────────────────────────────────────────────────────
static void handleWsMessage(const String& msg);
static void sendDataStream();

// ─── WebSocket event handler ──────────────────────────────────────────────────
static void onWsEvent(AsyncWebSocket* srv, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Client #%u connected from %s\n",
                      client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len) {
            // Single-frame text message
            if (info->opcode == WS_TEXT) {
                String msg;
                msg.concat((char*)data, len);
                handleWsMessage(msg);
            }
        }
    } else if (type == WS_EVT_ERROR) {
        Serial.printf("[WS] Error #%u: %u\n", client->id(), *((uint16_t*)arg));
    }
}

// ─── Process incoming WebSocket JSON command ──────────────────────────────────
// Uses the same logic as serial.cpp — same protocol over WebSocket
static void handleWsMessage(const String& msg) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (err) {
        Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* cmd = doc["Cmd"] | "";

    // Helper lambda to send reply
    auto sendReply = [&](JsonDocument& reply) {
        String out;
        serializeJson(reply, out);
        ws.textAll(out);
    };

    if (strcmp(cmd, "Get") == 0) {
        JsonDocument reply;
        trkset.toJSON(reply);
        sendReply(reply);
        return;
    }

    if (strcmp(cmd, "Set") == 0) {
        trkset.fromJSON(doc);
        JsonDocument reply;
        trkset.toJSON(reply);
        sendReply(reply);
        return;
    }

    if (strcmp(cmd, "FW") == 0) {
        JsonDocument reply;
        reply["Cmd"]  = "FW";
        reply["Vers"] = FW_VERSION;
        reply["Hard"] = HW_VERSION;
        reply["Git"]  = GIT_VERSION;
        sendReply(reply);
        return;
    }

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
        pins["PPM"]   = PIN_PPM_OUT;
        pins["BTN"]   = PIN_CENTER_BTN;
        pins["JOY_X"] = PIN_JOY_X;
        pins["JOY_Y"] = PIN_JOY_Y;
        pins["SDA"]   = PIN_I2C_SDA;
        pins["SCL"]   = PIN_I2C_SCL;
        sendReply(reply);
        return;
    }

    if (strcmp(cmd, "RstCnt") == 0) {
        sense_setCenter();
        JsonDocument reply;
        reply["Cmd"] = "RstCnt";
        reply["OK"]  = true;
        sendReply(reply);
        return;
    }

    if (strcmp(cmd, "Flash") == 0) {
        trkset.saveToFlash();
        JsonDocument reply;
        reply["Cmd"] = "Flash";
        reply["OK"]  = true;
        sendReply(reply);
        return;
    }

    if (strcmp(cmd, "Reboot") == 0) {
        ws.textAll("{\"Cmd\":\"Reboot\",\"OK\":true}");
        delay(300);
        esp_restart();
        return;
    }

    if (strcmp(cmd, "RD") == 0) {
        if (doc["panout"].is<bool>())  wsStreamPan  = doc["panout"];
        if (doc["tiltout"].is<bool>()) wsStreamTilt = doc["tiltout"];
        if (doc["rollout"].is<bool>()) wsStreamRoll = doc["rollout"];
        return;
    }

    if (strcmp(cmd, "D--") == 0) {
        wsStreamPan = wsStreamTilt = wsStreamRoll = false;
        return;
    }
}

// ─── Periodic data stream ─────────────────────────────────────────────────────
static void sendDataStream() {
    if (ws.count() == 0) return;
    if (!wsStreamPan && !wsStreamTilt && !wsStreamRoll) return;

    unsigned long now = millis();
    if (now - wsLastDataMs < 50) return;
    wsLastDataMs = now;

    JsonDocument doc;
    doc["Cmd"] = "Data";
    if (wsStreamPan)  doc["panout"]  = trkset.chanOut[trkset.panCh()];
    if (wsStreamTilt) doc["tiltout"] = trkset.chanOut[trkset.tltCh()];
    if (wsStreamRoll) doc["rollout"] = trkset.chanOut[trkset.rllCh()];
    doc["panoff"]  = (float)sensePan;
    doc["tiltoff"] = (float)senseTilt;
    doc["rolloff"] = (float)senseRoll;
    doc["calSys"]  = calSys;
    doc["calGyr"]  = calGyro;
    doc["calAcc"]  = calAccel;
    doc["calMag"]  = calMag;

    String out;
    serializeJson(doc, out);
    ws.textAll(out);
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void webserver_init() {
    // ── Build AP SSID: HeadTracker-XXXX ─────────────────────────────────────
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "HeadTracker-%02X%02X", mac[4], mac[5]);
    trkset.setApSSID(ssid);

    // ── Start WiFi Access Point ──────────────────────────────────────────────
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid); // No password
    Serial.printf("[WiFi] AP started: SSID=%s  IP=%s\n",
                  ssid, WiFi.softAPIP().toString().c_str());

    // ── LittleFS ─────────────────────────────────────────────────────────────
    if (!LittleFS.begin(true)) {
        Serial.println("[WiFi] LittleFS mount failed - web UI unavailable");
    } else {
        Serial.println("[WiFi] LittleFS mounted OK");
    }

    // ── WebSocket ─────────────────────────────────────────────────────────────
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // ── Static file serving from LittleFS ────────────────────────────────────
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // ── 404 handler ──────────────────────────────────────────────────────────
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.println("[WiFi] Web server started on http://192.168.4.1");
}

// ─── Update (call from main loop) ─────────────────────────────────────────────
void webserver_update() {
    ws.cleanupClients();
    sendDataStream();
}

// ─── Broadcast helper ─────────────────────────────────────────────────────────
void webserver_broadcastJSON(const char* json) {
    if (ws.count() > 0) {
        ws.textAll(json);
    }
}
