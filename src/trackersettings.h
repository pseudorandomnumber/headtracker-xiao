/*
 * HeadTracker for Seeed XIAO ESP32C6
 * Inspired by https://github.com/headtracker/HeadTracker (GPL-3.0)
 *
 * trackersettings.h - Central settings management
 * Mirrors the JSON parameter names from the original project for
 * compatibility with the HeadTracker-configurator serial protocol.
 */

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// ─── Defaults ────────────────────────────────────────────────────────────────
#define DEF_MIN_PWM         1000
#define DEF_MAX_PWM         2000
#define DEF_CNT_PWM         1500
#define DEF_GAIN            1.0f
#define DEF_TILT_CHANNEL    1
#define DEF_ROLL_CHANNEL    2
#define DEF_PAN_CHANNEL     3
#define PPM_CHANNELS        8

// Output modes (bit flags)
#define OUTPUT_PPM          (1 << 0)
#define OUTPUT_BLE_HID      (1 << 1)

// ─── Pin Definitions (XIAO ESP32C6) ──────────────────────────────────────────
#define PIN_PPM_OUT         2    // D0 / GPIO2
#define PIN_CENTER_BTN      3    // D1 / GPIO3
#define PIN_JOY_X           4    // D2 / GPIO4  (ADC)
#define PIN_JOY_Y           5    // D3 / GPIO5  (ADC)
#define PIN_I2C_SDA         6    // D4 / GPIO6
#define PIN_I2C_SCL         7    // D5 / GPIO7

// Firmware info
#define FW_VERSION          "1.0.0"
#define HW_VERSION          "XIAO-ESP32C6"
#define GIT_VERSION         "custom"

class TrackerSettings {
public:
    TrackerSettings();

    // ── Per-axis settings ───────────────────────────────────────────────────
    // Tilt
    int   tltMin()  const { return _tltMin; }
    int   tltMax()  const { return _tltMax; }
    int   tltCnt()  const { return _tltCnt; }
    float tltGain() const { return _tltGain; }
    int   tltCh()   const { return _tltCh; }
    bool  tltRev()  const { return _tltRev; }

    void setTltMin(int v)    { _tltMin  = constrain(v, 500, 2500); }
    void setTltMax(int v)    { _tltMax  = constrain(v, 500, 2500); }
    void setTltCnt(int v)    { _tltCnt  = constrain(v, 500, 2500); }
    void setTltGain(float v) { _tltGain = v; }
    void setTltCh(int v)     { _tltCh   = constrain(v, 1, PPM_CHANNELS); }
    void setTltRev(bool v)   { _tltRev  = v; }

    // Roll
    int   rllMin()  const { return _rllMin; }
    int   rllMax()  const { return _rllMax; }
    int   rllCnt()  const { return _rllCnt; }
    float rllGain() const { return _rllGain; }
    int   rllCh()   const { return _rllCh; }
    bool  rllRev()  const { return _rllRev; }

    void setRllMin(int v)    { _rllMin  = constrain(v, 500, 2500); }
    void setRllMax(int v)    { _rllMax  = constrain(v, 500, 2500); }
    void setRllCnt(int v)    { _rllCnt  = constrain(v, 500, 2500); }
    void setRllGain(float v) { _rllGain = v; }
    void setRllCh(int v)     { _rllCh   = constrain(v, 1, PPM_CHANNELS); }
    void setRllRev(bool v)   { _rllRev  = v; }

    // Pan
    int   panMin()  const { return _panMin; }
    int   panMax()  const { return _panMax; }
    int   panCnt()  const { return _panCnt; }
    float panGain() const { return _panGain; }
    int   panCh()   const { return _panCh; }
    bool  panRev()  const { return _panRev; }

    void setPanMin(int v)    { _panMin  = constrain(v, 500, 2500); }
    void setPanMax(int v)    { _panMax  = constrain(v, 500, 2500); }
    void setPanCnt(int v)    { _panCnt  = constrain(v, 500, 2500); }
    void setPanGain(float v) { _panGain = v; }
    void setPanCh(int v)     { _panCh   = constrain(v, 1, PPM_CHANNELS); }
    void setPanRev(bool v)   { _panRev  = v; }

    // ── Output settings ─────────────────────────────────────────────────────
    int  outputMode()     const { return _outputMode; }
    void setOutputMode(int v)   { _outputMode = v; }

    bool ppmEnabled() const { return (_outputMode & OUTPUT_PPM) != 0; }
    bool bleEnabled() const { return (_outputMode & OUTPUT_BLE_HID) != 0; }

    // WiFi AP SSID
    const char* apSSID() const { return _apSSID; }
    void setApSSID(const char* v) { strlcpy(_apSSID, v, sizeof(_apSSID)); }

    // ── Calibration center offsets ──────────────────────────────────────────
    float rollOffset()  const { return _rollOffset; }
    float tiltOffset()  const { return _tiltOffset; }
    float panOffset()   const { return _panOffset; }

    void setRollOffset(float v) { _rollOffset = v; }
    void setTiltOffset(float v) { _tiltOffset = v; }
    void setPanOffset(float v)  { _panOffset  = v; }

    void resetCenter() {
        _rollOffset = 0;
        _tiltOffset = 0;
        _panOffset  = 0;
    }

    // ── Persistence ─────────────────────────────────────────────────────────
    void loadFromFlash();
    void saveToFlash();

    // ── JSON protocol (HeadTracker-configurator compatible) ─────────────────
    // Serialize all settings to a JSON doc (for "Set" response / "Get" reply)
    void toJSON(JsonDocument& doc) const;
    // Apply settings from a JSON doc (for incoming "Set" command)
    void fromJSON(const JsonDocument& doc);

    // ── Channel output values (written by sense thread, read by outputs) ────
    uint16_t chanOut[PPM_CHANNELS + 1]; // 1-indexed, [0] unused

private:
    // Tilt
    int   _tltMin,  _tltMax,  _tltCnt;
    float _tltGain;
    int   _tltCh;
    bool  _tltRev;

    // Roll
    int   _rllMin,  _rllMax,  _rllCnt;
    float _rllGain;
    int   _rllCh;
    bool  _rllRev;

    // Pan
    int   _panMin,  _panMax,  _panCnt;
    float _panGain;
    int   _panCh;
    bool  _panRev;

    int  _outputMode;
    char _apSSID[32];

    float _rollOffset, _tiltOffset, _panOffset;
};

// Global singleton (defined in trackersettings.cpp)
extern TrackerSettings trkset;
