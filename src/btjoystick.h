/*
 * HeadTracker for Seeed XIAO ESP32C6
 * btjoystick.h — BLE HID Joystick (8 channels, 16-bit axes)
 *
 * Uses the built-in ESP32 BLE Arduino library (arduino-esp32 3.x).
 * No extra library dependency.
 *
 * Appears as "HeadTracker" to EdgeTX BT Joystick trainer input.
 */

#pragma once
#include <Arduino.h>

// Initialise BLE HID Joystick (call once)
void btJoystick_init();

// Send current channel values over BLE HID.
// channels[]: 1-indexed array of µs values (1000–2000), index 0 unused.
// Maps µs → signed 16-bit: 1000→-32768  1500→0  2000→32767
void btJoystick_update();

// True if a BLE central is connected
bool btJoystick_connected();
