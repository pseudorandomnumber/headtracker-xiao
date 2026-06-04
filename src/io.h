/*
 * HeadTracker for Seeed XIAO ESP32C6
 * io.h - Button + Joystick input handling
 *
 * Hardware:
 *   Center button: PIN_CENTER_BTN (D1/GPIO3), active LOW with internal pull-up
 *   Joystick VRX:  PIN_JOY_X (D2/GPIO4), ADC 0–4095 → -100..+100%
 *   Joystick VRY:  PIN_JOY_Y (D3/GPIO5), ADC 0–4095 → -100..+100%
 *   (No SW button on Deek-Robot joystick)
 *
 * Button behaviour:
 *   Short press  (< 800ms): Set center (zero all offsets)
 *   Long press   (≥ 800ms): Cycle OLED screen
 */
#pragma once
#include <Arduino.h>

void io_init();
void io_update();   // Call from main loop / task

// Joystick values: -100 to +100 (0 = center)
extern volatile int8_t joyX;
extern volatile int8_t joyY;

// Center button state
extern volatile bool btnPressed;       // True while held
extern volatile bool btnShortEvent;    // Set true on short-press release
extern volatile bool btnLongEvent;     // Set true on long-press trigger
