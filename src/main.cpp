#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

#define WIFI_SSID "Solomaha"
#define WIFI_PASSWORD "solomakha21"

#define MQTT_HOST IPAddress(192, 168, 1, 230)
#define MQTT_PORT 1883
#define MQTT_ID "room1-plant"
#define MQTT_PASSWORD "hjdfhukfunecagngnfefrvjkhmlumiyervgdf"

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

Ticker readTimer;
Ticker waterTimer;

const uint8_t pumpPin = D1;

struct {
    uint8_t moisture = 80;
    uint8_t duration = 10;
} config;

StaticJsonDocument<JSON_OBJECT_SIZE(2)> jsonDocument;
char sendBuffer[JSON_OBJECT_SIZE(2)];

void water(int stop = 0) {
    if (stop) {
        digitalWrite(pumpPin, LOW);

        mqttClient.publish("plant/room1-plant/watered", 0, false, "true");
    } else {
        digitalWrite(pumpPin, HIGH);

        waterTimer.once(config.duration, water, 1);
    }
}

void read(int sendConfig = 0) {
    Serial.println(analogRead(A0));
    auto moisture = (uint8_t) (100 - (analogRead(A0) / 1023.0) * 100.0);
    Serial.println(moisture);

    if (moisture <= config.moisture) {
        water(0);
    }

    String payload = "{\"moisture\":";
    payload += moisture;

    if (sendConfig) {
        payload += ",\"minMoisture\":";
        payload += config.moisture;
        payload += ",\"duration\":";
        payload += config.duration;
    }

    payload += "}";

    if (mqttClient.connected()) {
        mqttClient.publish("plant/room1-plant", 0, false, payload.c_str());
    }
}

void connectToWifi() {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP &event) {
    Serial.println("Connected to Wi-Fi.");
    digitalWrite(LED_BUILTIN, LOW);

    connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event) {
    Serial.print("Disconnected from Wi-Fi: ");
    Serial.println(event.reason);
    digitalWrite(LED_BUILTIN, HIGH);

    mqttReconnectTimer.detach();
    wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool) {
    Serial.println("Connected to MQTT.");
    digitalWrite(LED_BUILTIN, LOW);

    // Subscribe to topics:
    mqttClient.subscribe("plant/room1-plant/water", 0);
    mqttClient.subscribe("plant/room1-plant/set", 0);
    mqttClient.subscribe("device/room1-plant", 0);

    // Send current state
    read(1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.print("Disconnected from MQTT. Reason: ");
    Serial.println((int) reason);
    digitalWrite(LED_BUILTIN, HIGH);

    if (WiFi.isConnected()) {
        mqttReconnectTimer.once(2, connectToMqtt);
    }
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties, size_t length, size_t, size_t) {
    if (strcmp(topic, "plant/room1-plant/water") == 0) {
        water(0);
    } else {
        strncpy(sendBuffer, payload, length);
        sendBuffer[length] = '\0';

        deserializeJson(jsonDocument, payload, length);

        config.moisture = (uint8_t)jsonDocument["minMoisture"];
        config.duration = (uint8_t)jsonDocument["duration"];

        mqttClient.publish("plant/room1-plant", 0, false, sendBuffer);

        // Save config to the memory
        EEPROM.put(0, config);
        EEPROM.commit();
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println();

    pinMode(pumpPin, OUTPUT);

    // Load config from the memory
    EEPROM.begin(sizeof(config));
    EEPROM.get(0, config);

    // Save default values if was not set
    if (config.moisture == 0) {
        config.moisture = 80;
    }

    if (config.duration == 0) {
        config.moisture = 10;
    }

    EEPROM.put(0, config);
    EEPROM.commit();

    Serial.println("Loaded config: ");
    Serial.print("moisture: ");
    Serial.println(config.moisture);
    Serial.print("duration: ");
    Serial.println(config.duration);

    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setClientId(MQTT_ID);
    mqttClient.setCredentials("device", MQTT_PASSWORD);

    readTimer.attach(300, read, 0);

    connectToWifi();
}

void loop() {}