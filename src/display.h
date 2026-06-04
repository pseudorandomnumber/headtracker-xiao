/*
 * HeadTracker for Seeed XIAO ESP32C6
 * display.h - SSD1306 OLED status display
 *
 * Supports 0.91" (128×32) and 0.66" (64×48) SSD1306 displays.
 * Both are I2C, address 0x3C.
 * Shared I2C bus with BNO055 (0x28).
 *
 * Display cycles through screens:
 *   0: Live angles + channel outputs
 *   1: BNO055 calibration status
 *   2: Output mode + connection status
 */
#pragma once
#include <Arduino.h>

void display_init();
void display_update();   // Call at ~10Hz

// Cycle to next screen (called by button long-press or joystick)
void display_nextScreen();

// Force screen index
void display_setScreen(int n);
int  display_getScreen();
