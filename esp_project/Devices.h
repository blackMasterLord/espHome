#include <Arduino.h>
#include <ArduinoJson.h>
#include <utils/hash.h>
#include <set>

enum class DeviceType {
    SWITCH = 0
};

std::vector<uint8_t> gpio_pins;

String trim(const String& str) {
    if (str.length() == 0)
        return str;

    size_t start = 0;
    while (start < str.length() && str[start] == ' ')
        ++start;

    size_t end = str.length() - 1;
    while (end > start && str[end] == ' ')
        --end;

    return str.substring(start, end - start + 1);
}

class DeviceData {
public:
    DeviceData(String newName = "Без названия", uint8_t pin = 0, bool enabled = false, DeviceType type = DeviceType::SWITCH) : name(newName), hash(su::hash(newName.c_str(), newName.length())), pin(pin), selected_pin(-1), enabled(enabled), type(type) {}

    String getName() const { return name; }
    void setName(String newName) {
        name = trim(newName);
        hash = su::hash(newName.c_str(), newName.length());
    }
    uint8_t getPin() const { return pin; }
    void setPin(uint8_t newPin) { 
        if (newPin > 0) {
            if(selected_pin >= 0) digitalWrite(gpio_pins[selected_pin], LOW);
            selected_pin = newPin - 1;
            pinMode(gpio_pins[selected_pin], OUTPUT);
        }
        pin = newPin;
    }
    size_t getHash() const { return hash; }
    bool getEnabled() const { return enabled; }
    void setEnabled(bool newEnabled) { 
        enabled = newEnabled; 
        if (selected_pin >= 0) {
            digitalWrite(gpio_pins[selected_pin], uint8_t(newEnabled));
        }
    }

    String getDeviceTypeName() {
        switch (type)
        {
        case DeviceType::SWITCH:
            return "Переключатель";
        default:
            break;
        }
    }

    void save(JsonObject obj) const {
        obj["name"] = name;
        obj["hash"] = hash;
        obj["pin"] = pin;
        obj["enabled"] = enabled;
        obj["type"] = int(type);
    }

    bool load(JsonObject obj) {
        if (!obj.containsKey("name") || 
            !obj.containsKey("hash") || 
            !obj.containsKey("pin") || 
            !obj.containsKey("enabled") ||
            !obj.containsKey("type")) {
            return false;
        }

        hash = obj["hash"].as<size_t>();
        name = obj["name"].as<String>();
        if (hash != su::hash(name.c_str(), name.length())) {
            return false;
        }

        setPin(obj["pin"].as<uint8_t>());
        setEnabled(obj["enabled"].as<bool>());
        type = DeviceType(obj["type"].as<int>());

        return true;
    }

    bool enabled;
    String name;
    uint8_t pin;
private:
    size_t hash;
    uint8_t selected_pin;
    DeviceType type;
};

class Devices {
public:
    Devices() {

#if defined(CONFIG_IDF_TARGET_ESP32S3)
int s3_pins[] = { 0, 2, 4, 5, 6, 15, 16, 17, 18, 19, 20, 21, 26, 27, 28, 29, 30, 31,
                 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 };
gpio_pins.assign(std::begin(s3_pins), std::end(s3_pins));

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
int c3_pins[] = { 4, 5, 6, 7, 8, 9, 10, 18, 19, 20, 21 };
gpio_pins.assign(std::begin(c3_pins), std::end(c3_pins));

#elif defined(ESP8266)
int esp8266_pins[] = { 0, 2, 4, 5, 12, 13, 14, 15, 16 };
gpio_pins.assign(std::begin(esp8266_pins), std::end(esp8266_pins));

#else
#error "Платформа не поддерживается!"
#endif

    }
    
    String getAvailPins(size_t selectedHash) {
        //TODO
        /*std::set<int> used_pins;
        bool has_selected = false;
        int selected_pin = -1;

        for (const auto& dev : devices_list) {
            if (dev.getHash() == selectedHash && dev.pin >= 0) {
                selected_pin = dev.pin;
                has_selected = true;
            }
            else {
                used_pins.insert(dev.pin);
            }
        }*/

        String result = "Не выбран;";
        for (int pin : gpio_pins) {
            //TODO
            /*if (pin == selected_pin || used_pins.find(pin) == used_pins.end()) {
                result += String(pin) + ";";
            }*/
            result += String(pin) + ";";
        }

        return result;
    }

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
        size_t baseHash = su::hash(baseName.c_str(), baseName.length());

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
            newHash = su::hash(newName.c_str(), newName.length());
        } while (std::any_of(devices_list.begin(), devices_list.end(),
            [newHash](const DeviceData& d) { return d.getHash() == newHash; }));

        devices_list.emplace_back(newName);
        return true;
    }

    bool removeDeviceByIndex(size_t index) {
        if (index >= devices_list.size()) {
            return false;
        }

        devices_list.erase(devices_list.begin() + index);
        return true;
    }

    bool removeDeviceByHash(size_t targetHash) {
        auto it = std::find_if(devices_list.begin(), devices_list.end(),
            [targetHash](const DeviceData& device) {
                return device.getHash() == targetHash;
            });

        if (it != devices_list.end()) {
            devices_list.erase(it);
            return true;
        }

        return false;
    }

    String save() {
        for (size_t i = 0; i < devices_list.size(); ++i) {
            DeviceData& currentDevice = devices_list[i];
            String originalName = currentDevice.getName();

            int count = 0;

            for (size_t j = 0; j < i; ++j) {
                if (devices_list[j].getName() == originalName) {
                    ++count;
                }
            }

            if (count > 0) {
                String newName = originalName + " " + String(count);
                currentDevice.setName(newName);
            }
        }
        
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

    void clear() {
        for (size_t i = 0; i < getNumOfDevices(); i++) {
            getDevice(i).setEnabled(false);
        }
        devices_list.clear();
    }

private:
    std::vector<DeviceData> devices_list{};
};