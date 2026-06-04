/*
 * HeadTracker for Seeed XIAO ESP32C6
 * Inspired by https://github.com/headtracker/HeadTracker (GPL-3.0)
 *
 * trackersettings.cpp - Settings management implementation
 */

#include "trackersettings.h"
#include <Preferences.h>

// Global singleton
TrackerSettings trkset;

static Preferences prefs;
static const char* NVS_NS = "headtracker";

// ─── Constructor ─────────────────────────────────────────────────────────────
TrackerSettings::TrackerSettings() {
    // Tilt defaults
    _tltMin  = DEF_MIN_PWM;
    _tltMax  = DEF_MAX_PWM;
    _tltCnt  = DEF_CNT_PWM;
    _tltGain = DEF_GAIN;
    _tltCh   = DEF_TILT_CHANNEL;
    _tltRev  = false;

    // Roll defaults
    _rllMin  = DEF_MIN_PWM;
    _rllMax  = DEF_MAX_PWM;
    _rllCnt  = DEF_CNT_PWM;
    _rllGain = DEF_GAIN;
    _rllCh   = DEF_ROLL_CHANNEL;
    _rllRev  = false;

    // Pan defaults
    _panMin  = DEF_MIN_PWM;
    _panMax  = DEF_MAX_PWM;
    _panCnt  = DEF_CNT_PWM;
    _panGain = DEF_GAIN;
    _panCh   = DEF_PAN_CHANNEL;
    _panRev  = false;

    _outputMode = OUTPUT_PPM | OUTPUT_BLE_HID;

    strlcpy(_apSSID, "HeadTracker", sizeof(_apSSID));

    _rollOffset = 0;
    _tiltOffset = 0;
    _panOffset  = 0;

    // All channels default to center
    for (int i = 0; i <= PPM_CHANNELS; i++) {
        chanOut[i] = DEF_CNT_PWM;
    }
}

// ─── Persistence ─────────────────────────────────────────────────────────────
void TrackerSettings::saveToFlash() {
    prefs.begin(NVS_NS, false);
    prefs.putInt("TltMin",  _tltMin);
    prefs.putInt("TltMax",  _tltMax);
    prefs.putInt("TltCnt",  _tltCnt);
    prefs.putFloat("TltGain", _tltGain);
    prefs.putInt("TltCh",   _tltCh);
    prefs.putBool("TltRev", _tltRev);

    prefs.putInt("RllMin",  _rllMin);
    prefs.putInt("RllMax",  _rllMax);
    prefs.putInt("RllCnt",  _rllCnt);
    prefs.putFloat("RllGain", _rllGain);
    prefs.putInt("RllCh",   _rllCh);
    prefs.putBool("RllRev", _rllRev);

    prefs.putInt("PanMin",  _panMin);
    prefs.putInt("PanMax",  _panMax);
    prefs.putInt("PanCnt",  _panCnt);
    prefs.putFloat("PanGain", _panGain);
    prefs.putInt("PanCh",   _panCh);
    prefs.putBool("PanRev", _panRev);

    prefs.putInt("OutMode", _outputMode);
    prefs.putString("APSSID", _apSSID);

    prefs.putFloat("RllOff", _rollOffset);
    prefs.putFloat("TltOff", _tiltOffset);
    prefs.putFloat("PanOff", _panOffset);

    prefs.end();
    Serial.println("[Settings] Saved to flash");
}

void TrackerSettings::loadFromFlash() {
    prefs.begin(NVS_NS, true); // read-only

    _tltMin  = prefs.getInt("TltMin",  DEF_MIN_PWM);
    _tltMax  = prefs.getInt("TltMax",  DEF_MAX_PWM);
    _tltCnt  = prefs.getInt("TltCnt",  DEF_CNT_PWM);
    _tltGain = prefs.getFloat("TltGain", DEF_GAIN);
    _tltCh   = prefs.getInt("TltCh",   DEF_TILT_CHANNEL);
    _tltRev  = prefs.getBool("TltRev", false);

    _rllMin  = prefs.getInt("RllMin",  DEF_MIN_PWM);
    _rllMax  = prefs.getInt("RllMax",  DEF_MAX_PWM);
    _rllCnt  = prefs.getInt("RllCnt",  DEF_CNT_PWM);
    _rllGain = prefs.getFloat("RllGain", DEF_GAIN);
    _rllCh   = prefs.getInt("RllCh",   DEF_ROLL_CHANNEL);
    _rllRev  = prefs.getBool("RllRev", false);

    _panMin  = prefs.getInt("PanMin",  DEF_MIN_PWM);
    _panMax  = prefs.getInt("PanMax",  DEF_MAX_PWM);
    _panCnt  = prefs.getInt("PanCnt",  DEF_CNT_PWM);
    _panGain = prefs.getFloat("PanGain", DEF_GAIN);
    _panCh   = prefs.getInt("PanCh",   DEF_PAN_CHANNEL);
    _panRev  = prefs.getBool("PanRev", false);

    _outputMode = prefs.getInt("OutMode", OUTPUT_PPM | OUTPUT_BLE_HID);

    String ssid = prefs.getString("APSSID", "HeadTracker");
    strlcpy(_apSSID, ssid.c_str(), sizeof(_apSSID));

    _rollOffset = prefs.getFloat("RllOff", 0.0f);
    _tiltOffset = prefs.getFloat("TltOff", 0.0f);
    _panOffset  = prefs.getFloat("PanOff", 0.0f);

    prefs.end();
    Serial.println("[Settings] Loaded from flash");
}

// ─── JSON serialization (HeadTracker-configurator protocol) ──────────────────
// Parameter names match the original project's serial protocol.
// See: https://github.com/headtracker/HeadTracker
void TrackerSettings::toJSON(JsonDocument& doc) const {
    doc["Cmd"] = "Set";

    // Tilt
    doc["TltMin"]  = _tltMin;
    doc["TltMax"]  = _tltMax;
    doc["TltCnt"]  = _tltCnt;
    doc["TltGain"] = _tltGain;
    doc["TltCh"]   = _tltCh;
    doc["TltRev"]  = _tltRev;

    // Roll
    doc["RllMin"]  = _rllMin;
    doc["RllMax"]  = _rllMax;
    doc["RllCnt"]  = _rllCnt;
    doc["RllGain"] = _rllGain;
    doc["RllCh"]   = _rllCh;
    doc["RllRev"]  = _rllRev;

    // Pan
    doc["PanMin"]  = _panMin;
    doc["PanMax"]  = _panMax;
    doc["PanCnt"]  = _panCnt;
    doc["PanGain"] = _panGain;
    doc["PanCh"]   = _panCh;
    doc["PanRev"]  = _panRev;

    doc["OutMode"] = _outputMode;
    doc["APSSID"]  = _apSSID;

    // Center offsets (informational)
    doc["RllOff"]  = _rollOffset;
    doc["TltOff"]  = _tiltOffset;
    doc["PanOff"]  = _panOffset;
}

void TrackerSettings::fromJSON(const JsonDocument& doc) {
    if (doc["TltMin"].is<int>())   setTltMin(doc["TltMin"]);
    if (doc["TltMax"].is<int>())   setTltMax(doc["TltMax"]);
    if (doc["TltCnt"].is<int>())   setTltCnt(doc["TltCnt"]);
    if (doc["TltGain"].is<float>()) setTltGain(doc["TltGain"]);
    if (doc["TltCh"].is<int>())    setTltCh(doc["TltCh"]);
    if (doc["TltRev"].is<bool>())  setTltRev(doc["TltRev"]);

    if (doc["RllMin"].is<int>())   setRllMin(doc["RllMin"]);
    if (doc["RllMax"].is<int>())   setRllMax(doc["RllMax"]);
    if (doc["RllCnt"].is<int>())   setRllCnt(doc["RllCnt"]);
    if (doc["RllGain"].is<float>()) setRllGain(doc["RllGain"]);
    if (doc["RllCh"].is<int>())    setRllCh(doc["RllCh"]);
    if (doc["RllRev"].is<bool>())  setRllRev(doc["RllRev"]);

    if (doc["PanMin"].is<int>())   setPanMin(doc["PanMin"]);
    if (doc["PanMax"].is<int>())   setPanMax(doc["PanMax"]);
    if (doc["PanCnt"].is<int>())   setPanCnt(doc["PanCnt"]);
    if (doc["PanGain"].is<float>()) setPanGain(doc["PanGain"]);
    if (doc["PanCh"].is<int>())    setPanCh(doc["PanCh"]);
    if (doc["PanRev"].is<bool>())  setPanRev(doc["PanRev"]);

    if (doc["OutMode"].is<int>())   setOutputMode(doc["OutMode"]);
    if (doc["APSSID"].is<const char*>()) setApSSID(doc["APSSID"]);
}
