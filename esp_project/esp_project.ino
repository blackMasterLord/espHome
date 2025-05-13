#include <Arduino.h>
#include <LittleFS.h>
#include <GTimer.h>
#include <GyverDBFile.h>
#include <SettingsGyverWS.h>
#include <ESPmDNS.h>

#include "WiFiStatusIndicator.h"
#include "WifiScanner.h"
#include "WiFiConnectorCustom.h"
#include "Devices.h"

GyverDBFile db(&LittleFS, "/data.db");
SettingsGyverWS sett("esphome.local", &db);
Devices devices;

DB_KEYS(kk,
    wifi_ssid,
    wifi_pass,
    select_wifi,

    connect,
    disconnect,
    scan,
    settings,
    add_device,
    clear_devices,

    status_indicator_sw,
    status_indicator_bright,

    devices_list,
    confirm
);

enum class Tab : uint8_t {
    MAIN = 0,
    SETTINGS,

    TABS_SIZE,
    NONE,
};

struct Menu {
    void setTab(Tab newTab) {
        if (tab != newTab) {
            tab = newTab;
            need_reload = true;
        }

        if (need_reload) {
            need_reload = false;
            update();
        }
    }

    void update() {
        sett.reload();
    }

    Tab getTab() const {
        return tab;
    }

    bool isLocked() const {
        return locked;
    }

    void lock() {
        locked = true;
    }

    void unlock() {
        locked = false;
    }

private:
    bool locked = false;
    bool need_reload = false;
    Tab tab = Tab::MAIN;
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

//TODO
//static GTimerCb<millis> upd_timer;
//static void HighLightText(Text findText = "") {
//    upd_timer.startTimeout(1000, [findText]() {
//        BSON p;
//        p["findText"] = findText;
//        sett.updater().update(kk::highlight, p);
//    });
//}

static void scanWifi() {
    db.set(kk::wifi_ssid, "");
    db.set(kk::wifi_pass, "");
    db.set(kk::select_wifi, 0);
    WifiScanner.scanNetworks();
    menu.lock();
    menu.setTab(Tab::SETTINGS);
    menu.update();
}

static int selected_tab;
static void build(sets::Builder& b) {

    selected_tab = int(menu.getTab());
    if (!menu.isLocked() && b.Tabs("Главное меню;Настройки", &selected_tab)) {
        menu.setTab(Tab(selected_tab));
    }

    if (menu.getTab() == Tab::MAIN) {
        drawMainMenu(b);
    }

    if (menu.getTab() == Tab::SETTINGS) {
        drawWifiMenu(b);
        
        if (!menu.isLocked()) {
            drawStatusIndicatorMenu(b);
            drawAddDeviceMenu(b);
        }
    }
}

static void drawMainMenu(sets::Builder& b) {
    if (b.beginGroup("Главное меню")) {
        if (!WiFiConnectorCustom.connected()) {
            if (b.Button(kk::connect, "Подключиться к WiFi")) {
                menu.setTab(Tab::SETTINGS);
                //HighLightText("Настройки WiFi");
            }
        }
        else {
            devices.load(db[kk::devices_list].toString());
            if (devices.getNumOfDevices() == 0) {
                b.Paragraph("Нет добавленных устройств.");
                if (b.Button(kk::settings, "Перейти в настройки")) {
                    menu.setTab(Tab::SETTINGS);
                    //HighLightText("Устройства");
                }
            }
            else {
                drawDevicesButtonsMenu(b);
            }
        }
        b.endGroup();
    }
}

static void drawDevicesButtonsMenu(sets::Builder& b) {
    for (size_t i = 0; i < devices.getNumOfDevices(); i++) {
        auto& device = devices.getDevice(i);
        if (b.Switch(device.getName(), &device.enabled)) {
            device.setEnabled(device.enabled);
            db[kk::devices_list] = devices.save();
        }
    }
}

static void drawWifiMenu(sets::Builder& b) {
    if (b.beginGroup("Настройки WiFi"))  {
        if (WifiScanner.getResult() == wifiScanResult::SCAN_RUNNING) {
            b.Loader("Поиск...");
        }
        else if (WiFiConnectorCustom.connecting()) {
            b.Loader("Подключение к " + String(db[kk::wifi_ssid]));
        }
        else if(WiFiConnectorCustom.connected()) {
            b.Paragraph("Подключено к " + String(db[kk::wifi_ssid]));

            if (b.Button(kk::disconnect, "Отключиться от " + String(db[kk::wifi_ssid]))) {
                callConfirm("Вы уверены, что хотите отключиться от WiFi сети " + String(db[kk::wifi_ssid]) + "?");
            }

            bool conf_res;
            if (b.Confirm(kk::confirm, "", &conf_res) && conf_res) {
                scanWifi();
                WiFiConnectorCustom.connect("", "");
            }
        }
        else if (WifiScanner.getResult() == wifiScanResult::SCAN_FAILED) {
            b.Paragraph("WiFi сети не найдены");
            if (b.Button(kk::scan, "Повторить поиск")) { scanWifi(); }
        }
        else if (WifiScanner.getResult() == wifiScanResult::SCAN_DONE) {
            String ssid_values = "Не выбрана;";
            for (int i = 0; i < WifiScanner.getNumNetworks(); i++) {
                ssid_values += WiFi.SSID(i) + ";";
            }

            if (b.Select(kk::select_wifi, "WiFi сеть", ssid_values)) {
                db[kk::wifi_ssid] = db[kk::select_wifi] == 0 ? "" : WiFi.SSID(uint8_t(db[kk::select_wifi]) - 1);
                menu.update();
            }

            if (db[kk::select_wifi] > 0) {
                b.Pass(kk::wifi_pass, "Пароль");
                if (b.Button(kk::connect, "Подключиться к " + String(db[kk::wifi_ssid]))) {
                    WiFiConnectorCustom.connect(db[kk::wifi_ssid], db[kk::wifi_pass]);
                }
            }

            if (b.Button(kk::scan, "Повторить поиск")) { scanWifi(); }
        }
        b.endGroup();
    }
}

static void drawStatusIndicatorMenu(sets::Builder& b) {
    if (b.beginGroup("Настройки индикации WiFi")) {

        if (b.Switch(kk::status_indicator_sw, "Индикация")) {
            WiFiStatusIndicator.setEnabled(db[kk::status_indicator_sw].toBool());
        }

        if (b.Slider(kk::status_indicator_bright, "Яркость", 0, 100, 1, "%")) {
            WiFiStatusIndicator.setBrightness(db[kk::status_indicator_bright].toInt());
        }

        b.endGroup();
    }
}

static void drawAddDeviceMenu(sets::Builder& b) {
    if (WiFiConnectorCustom.connected() && b.beginGroup("Устройства")) {

        if (b.Button(kk::add_device, "Добавить устройство") && devices.addNewDevice()) {
            db[kk::devices_list] = devices.save();
            menu.update();
        }

        if (devices.getNumOfDevices() > 0 && b.Button(kk::clear_devices, "Удалить все")) {
            devices.clear();
            db[kk::devices_list] = devices.save();
            menu.update();
        }

        b.Label("Всего устройств: " + String(devices.getNumOfDevices()));

        if (devices.getNumOfDevices() > 0 && b.beginScroll()) {
            for (size_t i = 0; i < devices.getNumOfDevices(); i++) {
                auto& device = devices.getDevice(i);
                String f = String((i + 1)) + ". " + device.getDeviceTypeName() + " " + device.getName();
                if (b.beginDropdown(f)) {
                    if (b.Input("Название:", &device.name)) {
                        device.setName(device.name);
                        db[kk::devices_list] = devices.save();
                        menu.update();
                    }

                    if (b.Select("Вывод:", devices.getAvailPins(device.getHash()), &device.pin)) {
                        device.setPin(device.pin);
                        device.setEnabled(false);
                        db[kk::devices_list] = devices.save();
                        //menu.update();
                    }

                    if (b.Button("Удалить", sets::Colors::Red)) {
                        device.setEnabled(false);
                        devices.removeDeviceByHash(device.getHash());
                        db[kk::devices_list] = devices.save();
                        menu.update();
                    }
                    b.endDropdown();
                }
            }
            b.endScroll();
        }

        b.endGroup();
    }
}

static GTimer<millis> disconnect_timer(1000, false, GTMode::Timeout);
void setup() {
    Serial.begin(115200);
    Serial.println();

    WiFiStatusIndicator.init();

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount Failed");
        return;
    }

    db.begin();
    db.clear();

    db.init(kk::wifi_ssid, "");
    db.init(kk::wifi_pass, "");
    db.init(kk::select_wifi, 0);
    db.init(kk::status_indicator_sw, true);
    db.init(kk::status_indicator_bright, 50);
    db.init(kk::devices_list, "");

    WiFiConnectorCustom.onConnect([]() {
        Serial.print("[CONNECTOR] Connected! ");
        Serial.print(WiFi.localIP());
        
        if (!MDNS.begin("esphome")) {
            Serial.println("Error setting up MDNS responder!");
        }
        else {
            Serial.println(" or esphome.local");
        }
        
        callNotice("Подключение успешно, можете подключаться к WiFi сети " + String(db[kk::wifi_ssid]));
        menu.unlock();
        menu.update();
        disconnect_timer.start();
    });
    WiFiConnectorCustom.onConnecting([]() {
        Serial.println("[CONNECTOR] Try to connecting! ");
        menu.lock();
        menu.update();
    });
    WiFiConnectorCustom.onError([]() {
        Serial.print("[CONNECTOR] Error! start AP ");
        Serial.println(WiFi.softAPIP());
        
        if (!MDNS.begin("esphome")) {
            Serial.println("Error setting up MDNS responder!");
        }
        else {
            Serial.println(" or esphome.local");
        }
        
        if (!db[kk::wifi_ssid].toString().isEmpty()) {
            callAlert("Ошибка подключения к " + String(db[kk::wifi_ssid]));
        }
        menu.unlock();
        menu.update();
    });

    WiFiConnectorCustom.connect(db[kk::wifi_ssid], db[kk::wifi_pass]);

    WifiScanner.onStart([]() {
        Serial.println("[WIFI] Scan start!");
        menu.lock();
        menu.update();
    });
    WifiScanner.onComplete([](uint16_t numNetworks) {
        Serial.println("[WIFI] Scan done, " + String(numNetworks) + " networks found!");
        callNotice("Найдено WiFi сетей: " + String(numNetworks));
        menu.unlock();
        menu.update();
    });
    WifiScanner.onFailed([]() {
        Serial.println("[WIFI] Scan failed!");
        callAlert("Ошибка поиска WiFi сетей!");
        menu.unlock();
        menu.update();
    });

    sett.begin();
    sett.onBuild(build);

    WiFiStatusIndicator.setEnabled(db[kk::status_indicator_sw].toBool());
    WiFiStatusIndicator.setBrightness(db[kk::status_indicator_bright].toInt());
}

void loop() {
    WiFiConnectorCustom.tick();
    sett.tick();
    WifiScanner.tick();
    WiFiStatusIndicator.tick();

    if (disconnect_timer) {
        Serial.println("[WIFI] SoftAP disconnect!");
        WiFi.softAPdisconnect(true);
    }
}
