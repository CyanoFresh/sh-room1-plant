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
    mqttClient.subscribe("plant/room1-plant/settings", 0);
    mqttClient.subscribe("device/room1-plant", 0);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.print("Disconnected from MQTT. Reason: ");
    Serial.println((int) reason);
    digitalWrite(LED_BUILTIN, HIGH);

    if (WiFi.isConnected()) {
        mqttReconnectTimer.once(2, connectToMqtt);
    }
}

void water(bool turnOff = false) {
    if (turnOff) {
        digitalWrite(pumpPin, LOW);

        mqttClient.publish("plant/room1-plant/watered", 0, false, "true");
    } else {
        digitalWrite(pumpPin, HIGH);

        waterTimer.once(config.duration, water, true);
    }
}

void read() {
    Serial.println(analogRead(A0));
    auto moisture = (uint) (100 - (analogRead(A0) / 1023) * 100);
    Serial.println(moisture);

    if (moisture <= config.moisture) {
        water();
    }

    String payload = "{\"moisture\":";
    payload += moisture;
    payload += "}";

    mqttClient.publish("plant/room1-plant", 0, false, payload.c_str());
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties, size_t length, size_t, size_t) {
    if (strcmp(topic, "plant/room1-plant/water") == 0) {
        water();
    } else {
        deserializeJson(jsonDocument, payload, length);

        config.moisture = jsonDocument["moisture"];
        config.duration = jsonDocument["duration"];

        strcpy(sendBuffer, payload);
        sendBuffer[length] = '\n';
        mqttClient.publish("plant/room1-plant", 0, false, sendBuffer);

        // Save config to the memory
        EEPROM.put(0, config);
        EEPROM.commit();
    }
}

void setup() {
    EEPROM.begin(sizeof(config));

    Serial.begin(115200);
    Serial.println();
    Serial.println();

    pinMode(pumpPin, OUTPUT);

    // Load config from the memory
    EEPROM.get(0, config);

    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setClientId(MQTT_ID);
    mqttClient.setCredentials("device", MQTT_PASSWORD);

    readTimer.attach(60, read);

    connectToWifi();
}

void loop() {}