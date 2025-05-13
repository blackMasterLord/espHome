#pragma once
#include <Arduino.h>
#include <WiFiType.h>
#include <GTimer.h>
#include "WiFiConnectorCustom.h"


enum class WifiIndicatorStatus : uint8_t {
    OFF = 0,

    CONNECTED = 0,
    CONNECTING,
    DISCONNECTED,
    SCAN,
};

class WifiStatusIndicatorClass {
public:
    WifiIndicatorStatus status;

    void init() {
        setEnabled(false);
    }

    void changeStatus(WifiIndicatorStatus s) {
        if (s == status) return;
        status = s;
        current_step = 0;
        switch (status) {
        case WifiIndicatorStatus::CONNECTED: R = 0, G = 255, B = 0; break;
        case WifiIndicatorStatus::CONNECTING: R = 255, G = 165, B = 0; break;
        case WifiIndicatorStatus::DISCONNECTED: R = 255, G = 0, B = 0; break;
        case WifiIndicatorStatus::SCAN: R = 0, G = 0, B = 255; break;
        default: R = 0, G = 0, B = 0; break;
        }
    }

    void tick() {
        if (!_enabled) return;

        static GTimer<millis> led_timer(int(1000 / steps), true);
        if (led_timer) {
            current_step++;
            if (current_step > steps) {
                current_step = 0;
            }
            uint8_t rm = map(current_step, 0, steps, 0, calcBrightness(R));
            uint8_t gm = map(current_step, 0, steps, 0, calcBrightness(G));
            uint8_t bm = map(current_step, 0, steps, 0, calcBrightness(B));
            rgbLedWrite(RGB_BUILTIN, rm, gm, bm);
        }

        static GTimer<millis> status_timer(100, true);
        if (status_timer) {
            check_status();
        }
    }

    void check_status() {
        bool connected = WiFiConnectorCustom.connected();
        bool connecting = WiFiConnectorCustom.connecting();
        bool scan = WiFi.scanComplete() == WIFI_SCAN_RUNNING;
        if (scan)
            changeStatus(WifiIndicatorStatus::SCAN);
        else if (connected)
            changeStatus(WifiIndicatorStatus::CONNECTED);
        else if (!connected && connecting)
            changeStatus(WifiIndicatorStatus::CONNECTING);
        else if (!connected && !connecting)
            changeStatus(WifiIndicatorStatus::DISCONNECTED);
        else {
            changeStatus(WifiIndicatorStatus::OFF);
        }

    }

    void setEnabled(bool enabled) {
        _enabled = enabled;
        if (!_enabled) {
            changeStatus(WifiIndicatorStatus::OFF);
            digitalWrite(RGB_BUILTIN, LOW);
        }
    }

    void setBrightness(int brightness) {
        _brightness = constrain(brightness, 0, 100);
    }

private:
    uint8_t R, G, B;
    uint8_t current_step = 0;
    uint8_t steps = 33;

    bool _enabled = true;
    int _brightness = 50;

    uint8_t calcBrightness(uint8_t color) const {
        long temp = (long)color * _brightness + 50;
        return constrain(temp / 100, 0, 255);
    }
};

extern WifiStatusIndicatorClass WiFiStatusIndicator;