#pragma once
#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

class WiFiConnectorClass {
    typedef std::function<void()> ConnectorCallback;

public:
    // имя AP, пароль AP, таймаут в секундах, отключать AP при успешном подключении к STA (иначе AP_STA)
    WiFiConnectorClass(const String& APname = String("ESPHome"),
        const String& APpass = String("123456789"),
        uint16_t timeout = 20,
        bool closeAP = false) : _APname(APname),
        _APpass(APpass),
        _tout(timeout * 1000ul),
        _closeAP(closeAP) {
    }

    // установить имя AP
    void setName(const String& name) {
        _APname = name;
    }

    // установить пароль AP
    void setPass(const String& pass) {
        _APpass = pass;
    }

    // установить таймаут в секундах
    void setTimeout(uint16_t timeout) {
        _tout = timeout * 1000ul;
    }

    // автоматически отключать AP при подключении к STA (умолч. вкл)
    void closeAP(bool close) {
        _closeAP = close;
    }

    // подключить обработчик успешного подключения
    void onConnect(ConnectorCallback cb) {
        _conn_cb = cb;
    }

    // подключить обработчик попытки подключения
    void onConnecting(ConnectorCallback cb) {
        _conng_cb = cb;
    }

    // подключить обработчик ошибки подключения, вызовется после старта AP
    void onError(ConnectorCallback cb) {
        _err_cb = cb;
    }

    // подключиться. Вернёт false если ssid не задан, будет запущена AP
    bool connect(const String& ssid, const String& pass = emptyString) {
        if (ssid.length()) {
            _tryConnect = true;
            if (_conng_cb) _conng_cb();
            WiFi.mode(WIFI_AP_STA);
            _startAP();
            WiFi.begin(ssid.c_str(), pass.c_str());
            _tmr = millis();
            return 1;

        }
        else {
            _tryConnect = false;
            WiFi.disconnect();
            WiFi.mode(WIFI_AP);
            _startAP();
            if (_err_cb) _err_cb();
        }
        return 0;
    }

    // вызывать в loop. Вернёт true при смене состояния
    bool tick() {
        if (_tryConnect) {
            if (WiFi.status() == WL_CONNECTED) {
                _tryConnect = false;
                if (_conn_cb) _conn_cb();
                if (_closeAP) WiFi.softAPdisconnect(true);
                return 1;

            }
            else if (millis() - _tmr >= _tout) {
                _tryConnect = false;
                WiFi.disconnect();
                WiFi.mode(WIFI_AP);
                _startAP();
                if (_err_cb) _err_cb();
                return 1;
            }
        }
        return 0;
    }

    // состояние подключения. true - подключен, false - запущена АР
    bool connected() {
        return WiFi.status() == WL_CONNECTED;
    }

    // в процессе подключения
    bool connecting() {
        return _tryConnect;
    }

    // подключить обработчик таймаута подключения
    void onTimeout(ConnectorCallback cb) __attribute__((deprecated)) {
        _err_cb = cb;
    }

private:
    String _APname, _APpass;
    uint32_t _tmr = 0, _tout;
    bool _closeAP = false;
    bool _tryConnect = false;

    ConnectorCallback _conn_cb = nullptr;
    ConnectorCallback _conng_cb = nullptr;
    ConnectorCallback _err_cb = nullptr;

    void _startAP() const {
        WiFi.softAP(_APname.c_str(), _APpass.c_str());
    }
};

extern WiFiConnectorClass WiFiConnectorCustom;