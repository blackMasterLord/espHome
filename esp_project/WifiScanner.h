#pragma once
#include <Arduino.h>
#include <WiFi.h>

enum class wifiScanResult : int16_t {
    SCAN_DONE = 0,
    SCAN_RUNNING = -1,
    SCAN_FAILED = -2,
};

class WifiScannerClass {
    typedef std::function<void()> ScannerCallback;
    typedef std::function<void(uint16_t numNetworks)> ScannerResultCallback;

public:
    void scanNetworks() {
        WiFi.scanDelete();
        WiFi.scanNetworks(true);
        setResult(wifiScanResult::SCAN_RUNNING);
        scan_done = false;
    }

    wifiScanResult getResult() const {
        return result;
    }

    int16_t getNumNetworks() const {
        return numNetworks;
    }

    void tick() {
        if (!scan_done) {
            numNetworks = WiFi.scanComplete();
            if (numNetworks == WIFI_SCAN_RUNNING) { setResult(wifiScanResult::SCAN_RUNNING); }
            if (numNetworks == WIFI_SCAN_FAILED) { setResult(wifiScanResult::SCAN_FAILED); scan_done = true; }
            else if (numNetworks >= 0) { setResult(wifiScanResult::SCAN_DONE); scan_done = true; }
        }
    }

    void onStart(ScannerCallback cb) {
        _run_cb = cb;
    }

    void onFailed(ScannerCallback cb) {
        _fail_cb = cb;
    }

    void onComplete(ScannerResultCallback cb) {
        _done_cb = cb;
    }
private:
    bool scan_done = true;
    int16_t numNetworks = WIFI_SCAN_FAILED;
    wifiScanResult result = wifiScanResult::SCAN_FAILED;

    ScannerCallback _run_cb = nullptr;
    ScannerCallback _fail_cb = nullptr;
    ScannerResultCallback _done_cb = nullptr;

    void setResult(wifiScanResult newResult) {
        if (result == newResult) {
            return;
        }

        switch (newResult)
        {
        case wifiScanResult::SCAN_DONE:
            if (_done_cb) _done_cb(numNetworks);
            break;
        case wifiScanResult::SCAN_RUNNING:
            if (_run_cb) _run_cb();
            break;
        case wifiScanResult::SCAN_FAILED:
            if (_fail_cb) _fail_cb();
            break;
        default:
            break;
        }

        result = newResult;
    }
};

extern WifiScannerClass WifiScanner;