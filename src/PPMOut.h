/*
 * HeadTracker for Seeed XIAO ESP32C6
 * PPMOut.h - PPM signal output via ESP32 hardware timer
 *
 * PPM signal: 8 channels, ~20ms frame, active-high pulses
 * Connects to trainer jack tip (3.5mm). Sleeve = GND.
 */
#pragma once
#include <Arduino.h>

void ppmOut_init();
void ppmOut_enable(bool en);
bool ppmOut_isEnabled();

// Update channel values (1-indexed, microseconds, 1000–2000)
// Called automatically from the PPM ISR using trkset.chanOut[]
