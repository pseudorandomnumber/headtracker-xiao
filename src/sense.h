/*
 * HeadTracker for Seeed XIAO ESP32C6
 * sense.h - BNO055 sensor reading + angle → channel mapping
 */
#pragma once
#include <Arduino.h>

// Raw and processed sensor angles (degrees)
extern volatile float senseRoll;
extern volatile float senseTilt;
extern volatile float sensePan;

// Is the sensor initialized and running?
extern volatile bool senseReady;

// BNO055 calibration status (0–3 each)
extern volatile uint8_t calSys, calGyro, calAccel, calMag;

void sense_init();

// Call from sense task: reads BNO055, applies offsets, maps to channels
void sense_update();

// Set current pose as center (zero offset)
void sense_setCenter();
