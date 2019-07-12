#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "config.h"

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

Ticker readTimer;
Ticker waterTimer;

struct {
    uint8_t moisture = 80;
    uint8_t duration = 10;
} state;

StaticJsonDocument<JSON_OBJECT_SIZE(2)> jsonDocument;
char sendBuffer[JSON_OBJECT_SIZE(2)];

void water(bool stop = false) {
    if (stop) {
        digitalWrite(config::PUMP_PIN, LOW);

        mqttClient.publish("plant/room1-plant/watered", 0, false, "true");
    } else {
        digitalWrite(config::PUMP_PIN, HIGH);

        waterTimer.once(state.duration, water, true);
    }
}

void read(bool sendConfigState = false) {
    auto moisture = (uint8_t) (
            100 - map(
                    analogRead(A0),
                    config::SENSOR_CALIBRATION_MIN,
                    config::SENSOR_CALIBRATION_MAX,
                    0,
                    100
            )
    );

    Serial.print(F("Moisture: "));
    Serial.print(analogRead(A0));
    Serial.print(" = ");
    Serial.print(moisture);
    Serial.println("%");

    if (moisture <= state.moisture) {
        water();
    }

    String payload = "{\"moisture\":";
    payload += moisture;

    if (sendConfigState) {
        payload += ",\"minMoisture\":";
        payload += state.moisture;
        payload += ",\"duration\":";
        payload += state.duration;
    }

    payload += "}";

    mqttClient.publish("plant/room1-plant", 0, false, payload.c_str());
}

void connectToWifi() {
    Serial.println(F("Connecting to Wi-Fi..."));
    WiFi.begin(config::WIFI_SSID, config::WIFI_PASSWORD);
}

void connectToMqtt() {
    Serial.println(F("Connecting to MQTT..."));
    mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP &) {
    connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event) {
    Serial.print(F("Disconnected from Wi-Fi: "));
    Serial.println(event.reason);
    digitalWrite(LED_BUILTIN, LOW);

    mqttReconnectTimer.detach();
    wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool) {
    Serial.println(F("Connected to MQTT."));
    digitalWrite(LED_BUILTIN, HIGH);

    // Subscribe to topics:
    mqttClient.subscribe("plant/room1-plant/water", 0);
    mqttClient.subscribe("plant/room1-plant/set", 0);

    // Send current state
    read(true);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.print(F("Disconnected from MQTT. Reason: "));
    Serial.println((int) reason);

    digitalWrite(LED_BUILTIN, LOW);

    if (WiFi.isConnected()) {
        mqttReconnectTimer.once(2, connectToMqtt);
    }
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties, size_t length, size_t, size_t) {
    if (strcmp(topic, "plant/room1-plant/water") == 0) {
        water();
    } else {
        strncpy(sendBuffer, payload, length);
        sendBuffer[length] = '\0';

        deserializeJson(jsonDocument, payload, length);

        state.moisture = (uint8_t) jsonDocument["minMoisture"];
        state.duration = (uint8_t) jsonDocument["duration"];

        read(true);     // Read & send applied parameters

        // Save state to the memory
        EEPROM.put(0, state);
        EEPROM.commit();
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println();

    pinMode(config::PUMP_PIN, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    digitalWrite(LED_BUILTIN, LOW);

    // Load state from the memory
    EEPROM.begin(sizeof(state));
    EEPROM.get(0, state);

    // Save default values if was not set
    if (state.moisture == 0) {
        state.moisture = 80;
    }

    if (state.duration == 0) {
        state.moisture = 10;
    }

    EEPROM.put(0, state);
    EEPROM.commit();

    Serial.println(F("Loaded state from memory: "));
    Serial.print(F("moisture: "));
    Serial.println(state.moisture);
    Serial.print(F("duration: "));
    Serial.println(state.duration);

    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.setServer(config::MQTT_HOST, config::MQTT_PORT);
    mqttClient.setClientId(config::MQTT_ID);
    mqttClient.setCredentials("device", config::MQTT_PASSWORD);

    readTimer.attach(config::SENSOR_READ_INTERVAL, read, false);

    connectToWifi();
}

void loop() {}