/*
 * HeadTracker for Seeed XIAO ESP32C6
 * Inspired by https://github.com/headtracker/HeadTracker (GPL-3.0)
 *
 * sense.cpp - BNO055 sensor reading + angle → channel mapping
 *
 * BNO055 Mounting orientation: FLAT, CHIP-FACE UP (on top of FatShark goggles)
 * Axis remapping: P1 mode (default) — adjust if needed for your mount.
 *
 * Channel mapping formula (same as original HeadTracker project):
 *   output_us = center + (angle * gain)
 *   output_us = constrain(output_us, min, max)
 *   If reversed: output_us = center - (angle * gain) → same as negating gain
 */

#include "sense.h"
#include "trackersettings.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

// ─── BNO055 instance ──────────────────────────────────────────────────────────
// I2C address 0x28 (ADR pin to GND)
static Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);

// ─── Exported globals ─────────────────────────────────────────────────────────
volatile float senseRoll  = 0.0f;
volatile float senseTilt  = 0.0f;
volatile float sensePan   = 0.0f;
volatile bool  senseReady = false;
volatile uint8_t calSys = 0, calGyro = 0, calAccel = 0, calMag = 0;

// ─── Init ─────────────────────────────────────────────────────────────────────
void sense_init() {
    // I2C on XIAO ESP32C6: SDA=GPIO6, SCL=GPIO7
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!bno.begin()) {
        Serial.println("[Sense] ERROR: BNO055 not found! Check wiring.");
        senseReady = false;
        return;
    }

    // Use external crystal for better accuracy
    bno.setExtCrystalUse(true);

    /*
     * Axis remapping for "flat face-up" mount on top of goggles:
     *
     * When the BNO055 is mounted flat, face-up:
     *   - Euler X = Roll  (goggle roll left/right)
     *   - Euler Y = Pitch (goggle tilt up/down)
     *   - Euler Z = Yaw   (goggle pan left/right)
     *
     * BNO055 default orientation (P1):
     *   - Euler.x = heading (yaw)   → maps to Pan
     *   - Euler.y = roll            → maps to Roll
     *   - Euler.z = pitch           → maps to Tilt
     *
     * We use the raw Euler angles from the BNO055 NDOF fusion mode.
     * Adjust the axis assignments below if your physical mount is different.
     */

    Serial.println("[Sense] BNO055 initialized");
    senseReady = true;
}

// ─── Channel mapping helper ────────────────────────────────────────────────────
// Maps an angle (degrees) to a PWM pulse width (µs)
// center_us ± (angle_deg * gain) clamped to [min_us, max_us]
static uint16_t angleToUs(float angle, int minUs, int maxUs, int cntUs, float gain, bool reversed) {
    float direction = reversed ? -1.0f : 1.0f;
    float us = (float)cntUs + direction * angle * gain;
    return (uint16_t)constrain((int)us, minUs, maxUs);
}

// ─── Main update (called from sense FreeRTOS task) ────────────────────────────
void sense_update() {
    if (!senseReady) return;

    // Read calibration status (use local non-volatile vars, then copy)
    uint8_t _sys = 0, _gyr = 0, _acc = 0, _mag = 0;
    bno.getCalibration(&_sys, &_gyr, &_acc, &_mag);
    calSys = _sys; calGyro = _gyr; calAccel = _acc; calMag = _mag;

    // Read Euler angles (degrees) from BNO055 hardware fusion
    imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);

    /*
     * BNO055 Euler vector in NDOF mode (P1 default mount):
     *   euler.x() = heading  0..360  (yaw / pan)
     *   euler.y() = roll    -90..90
     *   euler.z() = pitch   -180..180 (tilt)
     *
     * We convert heading to a signed value centred on 0:
     *   heading > 180 → heading - 360 (so range is -180..+180)
     */
    float rawPan  = euler.x();   // heading 0..360
    float rawRoll = euler.y();   // roll
    float rawTilt = euler.z();   // pitch

    // Convert heading to signed -180..+180
    if (rawPan > 180.0f) rawPan -= 360.0f;

    // Apply center offsets
    float pan  = rawPan  - trkset.panOffset();
    float roll = rawRoll - trkset.rollOffset();
    float tilt = rawTilt - trkset.tiltOffset();

    // Normalise pan to -180..+180 after offset
    if (pan >  180.0f) pan -= 360.0f;
    if (pan < -180.0f) pan += 360.0f;

    // Store for display / telemetry
    sensePan  = pan;
    senseRoll = roll;
    senseTilt = tilt;

    // Map angles to RC channel pulse widths
    uint16_t tltUs = angleToUs(tilt, trkset.tltMin(), trkset.tltMax(), trkset.tltCnt(),
                                trkset.tltGain(), trkset.tltRev());
    uint16_t rllUs = angleToUs(roll, trkset.rllMin(), trkset.rllMax(), trkset.rllCnt(),
                                trkset.rllGain(), trkset.rllRev());
    uint16_t panUs = angleToUs(pan,  trkset.panMin(), trkset.panMax(), trkset.panCnt(),
                                trkset.panGain(), trkset.panRev());

    // Write to the assigned output channels (1-indexed)
    // Unused channels remain at their last value (default 1500)
    int tch = trkset.tltCh();
    int rch = trkset.rllCh();
    int pch = trkset.panCh();

    if (tch >= 1 && tch <= PPM_CHANNELS) trkset.chanOut[tch] = tltUs;
    if (rch >= 1 && rch <= PPM_CHANNELS) trkset.chanOut[rch] = rllUs;
    if (pch >= 1 && pch <= PPM_CHANNELS) trkset.chanOut[pch] = panUs;
}

// ─── Set center ───────────────────────────────────────────────────────────────
void sense_setCenter() {
    if (!senseReady) return;

    imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);

    float rawPan = euler.x();
    if (rawPan > 180.0f) rawPan -= 360.0f;

    trkset.setPanOffset(rawPan);
    trkset.setRollOffset(euler.y());
    trkset.setTiltOffset(euler.z());

    Serial.printf("[Sense] Center set: Pan=%.1f Roll=%.1f Tilt=%.1f\n",
                  rawPan, euler.y(), euler.z());
}
