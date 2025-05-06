#include <AsyncTCP.h>
#include <Arduino.h>
#include <GTimer.h>
#include <GyverDBFile.h>
#include <LittleFS.h>
#include <SettingsAsyncWS.h>
#include <WiFiConnector.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "CustomJS.h"

enum class WifiIndicatorStatus : uint8_t {
    OFF = 0,

    CONNECTED = 0,
    CONNECTING,
    DISCONNECTED,
    SCAN,
};

class WifiStatusIndicator {
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
        bool connected = WiFiConnector.connected();
        bool connecting = WiFiConnector.connecting();
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

        Serial.println("[StatusIndicator] setEnabled = " + String(_enabled));

        if (!_enabled) {
            digitalWrite(RGB_BUILTIN, LOW);
            changeStatus(WifiIndicatorStatus::OFF);
        }
    }

    void setBrightness(int brightness) {
        _brightness = constrain(brightness, 0, 100);

        Serial.println("[StatusIndicator] setBrightness = " + String(_brightness));
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

WifiStatusIndicator StatusIndicator;

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
        scan_done = false;
    }

    wifiScanResult getResult() {
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

WifiScannerClass WifiScanner;

GyverDBFile db(&LittleFS, "/data.db");
SettingsAsyncWS sett("esphome.local", &db);

DB_KEYS(kk,
    wifi_ssid,
    wifi_pass,
    select_wifi,
    apply,
    add_vevice,
    scan_wifi,

    status_indicator_sw,
    status_indicator_bright,

    voice,

    devices_list,

    confirm
);

enum class Tab : uint8_t {
    MAIN = 0,
    SETTINGS,

    TABS_SIZE,
    NONE,
};

enum class State : uint8_t {
    IDLE = 0,
    LOADING,

    WIFI_SCANNING,
    WIFI_CONNECTING,
};

struct Menu {
    void setTab(Tab newTab, State newState = State::IDLE) {
        if (tab != newTab) {
            tab = newTab;
            need_reload = true;
        }
        if (states[int(tab)] != newState) {
            states[int(tab)] = newState;
            need_reload = true;
        }

        if (need_reload) {
            need_reload = false;
            sett.reload();
        }
    }

    Tab getTab() const {
        return tab;
    }

    void setState(State newState, Tab forTab = Tab::NONE) {
        int targetTab = forTab == Tab::NONE ? int(tab) : int(forTab);
        if (states[targetTab] != newState) {
            states[targetTab] = newState;
            need_reload = true;
        }

        if (need_reload) {
            need_reload = false;
            sett.reload();
        }
    }

    State getState(Tab forTab = Tab::NONE) const {
        int targetTab = forTab == Tab::NONE ? int(tab) : int(forTab);
        return states[targetTab];
    }

    bool isLocked() const {
        return states[int(tab)] == State::WIFI_SCANNING || states[int(tab)] == State::WIFI_CONNECTING;
    }

private:
    bool need_reload = false;
    Tab tab = Tab::MAIN;
    State states[int(Tab::TABS_SIZE)] = { State::IDLE, State::IDLE };
} menu;

static void callAlert(su::Text text) {
    sett.updater().alert(text);
}

static void callNotice(su::Text text) {
    sett.updater().notice(text);
}

static void callConfirm(su::Text text) {
    sett.updater().update(kk::confirm, text);
}

static void scanWifi() {
    db.set(kk::wifi_ssid, "");
    db.set(kk::wifi_pass, "");
    db.set(kk::select_wifi, 0);
    WifiScanner.scanNetworks();
    menu.setTab(Tab::SETTINGS, State::WIFI_SCANNING);
}

static void build(sets::Builder& _b) {
    CustomBuilder b(_b);
    
    static int selected_tab = int(menu.getTab());
    if (!menu.isLocked() && b.Tabs("Главное меню;Настройки", &selected_tab)) {
        menu.setTab(Tab(selected_tab));
    }

    if (menu.getTab() == Tab::MAIN) {
        drawMainMenu(b);
    }

    if (menu.getTab() == Tab::SETTINGS) {
        drawWifiMenu(b);
        drawStatusIndicatorMenu(b);
        drawAddDeviceMenu(b);
    }
}

static void drawLoadingMenu(CustomBuilder& b) {
    b.Loader("Загрузка...");
    b.Voice(kk::voice, "Здесь VoiceWidget");
}

static void drawMainMenu(CustomBuilder& b) {
    if (b.beginGroup("Главное меню")) {
        if (menu.getState() == State::IDLE) {
            drawLoadingMenu(b);
        }
        b.endGroup();
    }
}

static void drawWifiMenu(CustomBuilder& b) {
    if (b.beginGroup("Настройки WiFi")) {
        if (menu.getState() == State::WIFI_CONNECTING) {
            b.Loader("Подключение к " + String(db[kk::wifi_ssid]));
        }
        else if (menu.getState() == State::WIFI_SCANNING) {
            b.Loader("Поиск...");
        }
        else if (menu.getState() == State::IDLE) 
        {
            if (WiFiConnector.connected()) {
                b.Paragraph("Подключено к " + String(db[kk::wifi_ssid]));
                
                if (b.Button(kk::apply, "Отключиться от " + String(db[kk::wifi_ssid]))) {
                    callConfirm("Вы уверены, что хотите отключиться от WiFi сети " + String(db[kk::wifi_ssid]) + "?");
                }
                
                bool conf_res;
                if (b.Confirm(kk::confirm, "", &conf_res) && conf_res) {
                    WiFiConnector.connect("", "");
                    scanWifi();
                }
            }
            else {
                if (WifiScanner.getResult() == wifiScanResult::SCAN_FAILED) {
                    b.Paragraph("WiFi сети не найдены");
                }
                else if (WifiScanner.getResult() == wifiScanResult::SCAN_DONE) {
                    String ssid_values = "Не выбрана;";
                    for (int i = 0; i < WifiScanner.getNumNetworks(); i++) {
                        ssid_values += WiFi.SSID(i) + ";";
                    }

                    if (b.Select(kk::select_wifi, "WiFi сеть", ssid_values)) {
                        db[kk::wifi_ssid] = db[kk::select_wifi] == 0 ? "" : WiFi.SSID(uint8_t(db[kk::select_wifi]) - 1);
                        sett.reload();
                    }

                    if (db[kk::select_wifi] > 0) {
                        b.Pass(kk::wifi_pass, "Пароль");
                        if (b.Button(kk::apply, "Подключиться к " + String(db[kk::wifi_ssid]))) {
                            menu.setState(State::WIFI_CONNECTING);
                            WiFiConnector.connect(db[kk::wifi_ssid], db[kk::wifi_pass]);
                        }
                    }
                }

                if (b.Button(kk::scan_wifi, "Повторить поиск")) {
                    scanWifi();
                }
            }
        }
        b.endGroup();
    }
}

static void drawStatusIndicatorMenu(CustomBuilder& b) {
    if (menu.getState() == State::IDLE && b.beginGroup("Настройки индикации WiFi")) {

        if (b.Switch(kk::status_indicator_sw, "Индикация")) {
            StatusIndicator.setEnabled(db[kk::status_indicator_sw].toBool());
        }

        if (b.Slider(kk::status_indicator_bright, "Яркость", 0, 100, 1, "%")) {
            StatusIndicator.setBrightness(db[kk::status_indicator_bright].toInt());
        }

        b.endGroup();
    }
}

class DeviceData {
public:
    DeviceData(String newName = "Без названия", uint8_t pin = 0, bool enabled = false) : name(newName), hash(H(newName)), pin(pin), enabled(enabled) {}

    String getName() const { return name; }
    size_t getHash() const { return hash; }
    uint8_t getPin() const { return pin; }
    bool getEnabled() const { return enabled; }
    void setEnabled(bool newEnabled) { enabled = newEnabled; }

    void save(JsonObject obj) const {
        obj["name"] = name;
        obj["hash"] = hash;
        obj["pin"] = pin;
        obj["enabled"] = enabled;
    }

    bool load(JsonObject obj) {
        if (!obj.containsKey("name") || !obj.containsKey("hash") || !obj.containsKey("pin") || !obj.containsKey("enabled")) {
            return false;
        }

        name = obj["name"].as<String>();
        hash = obj["hash"].as<size_t>();
        pin = obj["pin"].as<uint8_t>();
        enabled = obj["enabled"].as<bool>();

        if (hash != H(name)) {
            return false;
        }

        return true;
    }

private:
    String name;
    size_t hash;
    uint8_t pin;
    bool enabled;
};

class Devices {
public:
    size_t getNumOfDevices() {
        return devices_list.size();
    }

    DeviceData& getDevice(size_t i) {
        return devices_list.at(i);
    }

    DeviceData getDeviceByHash(size_t hash) {
        auto it = std::find_if(devices_list.begin(), devices_list.end(),
            [hash](const DeviceData& d) { return d.getHash() == hash; });
        if (it != devices_list.end()) {
            return *it;
        }
        return DeviceData("Not Found");
    }

    bool addNewDevice(String baseName = "Без названия") {
        size_t baseHash = H(baseName);

        bool exists = std::any_of(devices_list.begin(), devices_list.end(),
            [baseHash](const DeviceData& d) { return d.getHash() == baseHash; });

        if (!exists) {
            devices_list.emplace_back(baseName);
            return true;
        }

        int counter = 1;
        String newName;
        size_t newHash;

        do {
            newName = baseName + " " + String(counter++);
            newHash = H(newName);
        } while (std::any_of(devices_list.begin(), devices_list.end(),
            [newHash](const DeviceData& d) { return d.getHash() == newHash; }));

        devices_list.emplace_back(newName);
        return true;
    }

    String save() {
        Serial.println("[DevicesDB] save");
        
        StaticJsonDocument<1024> doc;
        JsonArray array = doc.to<JsonArray>();

        for (const auto& device : devices_list) {
            JsonObject obj = array.createNestedObject();
            device.save(obj);
        }

        String jsonStr;
        serializeJson(doc, jsonStr);
        return jsonStr;
    }

    bool load(String data) {
        Serial.println("[DevicesDB] load");
        
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, data);
        if (error) {
            return false;
        }

        devices_list.clear();

        JsonArray array = doc.as<JsonArray>();
        for (JsonObject obj : array) {
            DeviceData device;
            if (device.load(obj)) {
                devices_list.push_back(device);
            }
        }

        return true;
    }

private:
    std::vector<DeviceData> devices_list{};
};

Devices devices;

static void drawAddDeviceMenu(CustomBuilder& b) {
    if (menu.getState() == State::IDLE && b.beginGroup("Устройства")) {

        if (b.Button(kk::add_vevice, "Добавить устройство") && devices.addNewDevice()) {
            db[kk::devices_list] = devices.save();
            sett.reload();
        }

        b.Label("Всего устройств: " + String(devices.getNumOfDevices()));

        if (devices.getNumOfDevices() > 0) {
            for (size_t i = 0; i < devices.getNumOfDevices(); i++) {
                auto& device = devices.getDevice(i);
                b.Paragraph(device.getName(), "Тип устройства");
            }
        }

        b.endGroup();
    }
}

static GTimer<millis> disconnect_timer(1000, false, GTMode::Timeout);
void setup() {
    Serial.begin(115200);
    Serial.println();

    LittleFS.begin(true);

    db.begin();
    //db.clear();

    db.init(kk::wifi_ssid, "");
    db.init(kk::wifi_pass, "");
    db.init(kk::select_wifi, 0);
    db.init(kk::status_indicator_sw, true);
    db.init(kk::status_indicator_bright, 50);
    db.init(kk::devices_list, "");

    StatusIndicator.init();

    WiFiConnector.onConnect([]() {
        Serial.print("[CONNECTOR] Connected! ");
        Serial.print(WiFi.localIP());
        if (!MDNS.begin("esphome")) {
            Serial.println("Error setting up MDNS responder!");
        }
        else {
            Serial.println(" or esphome.local");
        }
        callNotice("Подключение успешно, можете подключаться к WiFi сети " + String(db[kk::wifi_ssid]));
        menu.setState(State::IDLE, Tab::SETTINGS);
        disconnect_timer.start();
    });
    WiFiConnector.onConnecting([]() {
        Serial.println("[CONNECTOR] Try to connecting! ");
    });
    WiFiConnector.onError([]() {
        Serial.print("[CONNECTOR] Error! start AP ");
        Serial.println(WiFi.softAPIP());
        if (!db[kk::wifi_ssid].toString().isEmpty()) {
            callAlert("Ошибка подключения к " + String(db[kk::wifi_ssid]));
        }
        menu.setState(State::IDLE, Tab::SETTINGS);
    });

    WiFiConnector.connect(db[kk::wifi_ssid], db[kk::wifi_pass]);

    WifiScanner.onStart([]() {
        Serial.println("[WIFI] Scan start!");
        menu.setState(State::WIFI_SCANNING, Tab::SETTINGS);
    });

    WifiScanner.onComplete([](uint16_t numNetworks) {
        Serial.println("[WIFI] Scan done, " + String(numNetworks) + " networks found!");
        callNotice("Найдено WiFi сетей: " + String(numNetworks));
        menu.setState(State::IDLE, Tab::SETTINGS);
    });

    WifiScanner.onFailed([]() {
        Serial.println("[WIFI] Scan failed!");
        callAlert("Ошибка поиска WiFi сетей!");
        menu.setState(State::IDLE, Tab::SETTINGS);
    });

    sett.begin();
    sett.onBuild(build);
    sett.getServer();
    sett.setCustom(custom, strlen_P(custom), false);

    StatusIndicator.setEnabled(db[kk::status_indicator_sw].toBool());
    StatusIndicator.setBrightness(db[kk::status_indicator_bright].toInt());

    devices.load(db[kk::devices_list].toString());
}

void loop() {
    WiFiConnector.tick();
    sett.tick();
    WifiScanner.tick();
    StatusIndicator.tick();

    if (disconnect_timer) {
        Serial.println("[WIFI] SoftAP disconnect!");
        WiFi.softAPdisconnect(true);
    }
}
