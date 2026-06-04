/*
 * HeadTracker for Seeed XIAO ESP32C6
 * serial.h - USB Serial JSON protocol handler
 *
 * Compatible with HeadTracker-configurator (Web Serial API).
 * Protocol: newline-delimited JSON objects.
 * Commands: Get, Set, FW, FE, RstCnt, Flash, Reboot, RD, D--
 */
#pragma once
#include <Arduino.h>

void serial_init();
void serial_update();  // Call from main loop / task

// Send a JSON string to the serial port (adds newline)
void serial_sendJSON(const char* json);
