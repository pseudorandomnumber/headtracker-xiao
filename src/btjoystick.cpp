/*
 * HeadTracker for Seeed XIAO ESP32C6
 * btjoystick.cpp — BLE HID Joystick using built-in ESP32 BLE Arduino library
 *
 * 8-channel 16-bit joystick (channels mapped from PPM µs values).
 * Appears as "HeadTracker" in EdgeTX Bluetooth Joystick trainer list.
 *
 * Report structure:
 *   Report ID 1: 8 × int16_t axes (CH1-CH8), signed (-32768..32767)
 *   µs mapping: 1000 → -32768, 1500 → 0, 2000 → +32767
 */

#include "btjoystick.h"
#include "trackersettings.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>

// ─── HID Report Descriptor — 8 axes, 16-bit each ─────────────────────────────
static const uint8_t kJoystickHIDReportDesc[] = {
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x05,        // Usage (Gamepad)
  0xA1, 0x01,        // Collection (Application)

  0x85, 0x01,        //   Report ID (1)

  // 8 × 16-bit signed axes  (CH1-CH8)
  0x09, 0x01,        //   Usage (Pointer)
  0xA1, 0x00,        //   Collection (Physical)
  0x09, 0x30,        //     Usage (X)
  0x09, 0x31,        //     Usage (Y)
  0x09, 0x32,        //     Usage (Z)
  0x09, 0x33,        //     Usage (Rx)
  0x09, 0x34,        //     Usage (Ry)
  0x09, 0x35,        //     Usage (Rz)
  0x09, 0x36,        //     Usage (Slider)
  0x09, 0x37,        //     Usage (Dial)
  0x16, 0x00, 0x80,  //     Logical Minimum (-32768)
  0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
  0x75, 0x10,        //     Report Size (16 bits)
  0x95, 0x08,        //     Report Count (8)
  0x81, 0x02,        //     Input (Data, Var, Abs)
  0xC0,              //   End Collection (Physical)

  0xC0               // End Collection (Application)
};

// ─── BLE objects ──────────────────────────────────────────────────────────────
static BLEHIDDevice*       hidDevice    = nullptr;
static BLECharacteristic*  inputReport  = nullptr;
static BLEServer*          bleServer    = nullptr;
static bool                bleConnected = false;

// ─── Connection callbacks ─────────────────────────────────────────────────────
class BtCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* svr) override {
        bleConnected = true;
        Serial.println("[BLE] Client connected");
    }
    void onDisconnect(BLEServer* svr) override {
        bleConnected = false;
        Serial.println("[BLE] Client disconnected — advertising again");
        BLEDevice::startAdvertising();
    }
};

// ─── Public API ───────────────────────────────────────────────────────────────
void btJoystick_init() {
    BLEDevice::init("HeadTracker");

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new BtCallbacks());

    hidDevice = new BLEHIDDevice(bleServer);
    hidDevice->manufacturer()->setValue("HeadTracker");
    hidDevice->pnp(0x02, 0x045E, 0x0719, 0x0100);  // sig, vid, pid, version
    hidDevice->hidInfo(0x00, 0x01);                  // country, flags

    // Register HID report descriptor
    hidDevice->reportMap((uint8_t*)kJoystickHIDReportDesc,
                         sizeof(kJoystickHIDReportDesc));

    // Create input report characteristic (Report ID 1)
    inputReport = hidDevice->inputReport(1);

    // Enable notifications
    BLE2902* desc = new BLE2902();
    desc->setNotifications(true);
    inputReport->addDescriptor(desc);

    hidDevice->startServices();

    // Advertising
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->setAppearance(HID_GAMEPAD);
    adv->addServiceUUID(hidDevice->hidService()->getUUID());
    adv->start();

    Serial.println("[BLE] HID Joystick advertising as 'HeadTracker'");
}

void btJoystick_update() {
    if (!bleConnected || inputReport == nullptr) return;

    // Build 8-channel report (16 bytes)
    int16_t axes[8];
    for (int i = 0; i < 8; i++) {
        uint16_t us = trkset.chanOut[i + 1];   // 1-indexed, clamp to 1000-2000
        if (us < 1000) us = 1000;
        if (us > 2000) us = 2000;
        // Map 1000-2000 → -32768..+32767
        axes[i] = (int16_t)(((int32_t)(us - 1500)) * 32767 / 500);
    }

    inputReport->setValue((uint8_t*)axes, sizeof(axes));
    inputReport->notify();
}

bool btJoystick_connected() {
    return bleConnected;
}
