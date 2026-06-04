/*
 * HeadTracker for Seeed XIAO ESP32C6
 * webserver.h - WiFi AP + AsyncWebServer + WebSocket config interface
 *
 * Connect phone/PC to WiFi AP "HeadTracker-XXXX"
 * Then open http://192.168.4.1 in a browser.
 * Uses the same JSON protocol as the USB Serial interface.
 */
#pragma once
#include <Arduino.h>

void webserver_init();
void webserver_update();   // Call periodically (handles WS broadcasts)

// Broadcast a JSON string to all connected WebSocket clients
void webserver_broadcastJSON(const char* json);
