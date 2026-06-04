/*
 * HeadTracker for Seeed XIAO ESP32C6
 * btjoystick.h — BLE HID Joystick (8 channels, 16-bit axes)
 *
 * Uses NimBLE-Arduino 2.x (h2zero/NimBLE-Arduino @ ^2.1.0).
 * NimBLE is the only BLE stack supported on ESP32C6 — Bluedroid is not available.
 *
 * Appears as "HeadTracker" BLE HID gamepad to EdgeTX BT Joystick trainer input.
 */

#pragma once
#include <Arduino.h>

// Initialise BLE HID Joystick (call once from setup, after WiFi)
void btJoystick_init();

// Send current channel values over BLE HID.
// Reads trkset.chanOut[1..8] (µs), maps 1000→-32768 / 1500→0 / 2000→+32767
void btJoystick_update();

// True if a BLE central is currently connected
bool btJoystick_connected();
