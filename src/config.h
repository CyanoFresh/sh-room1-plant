#ifndef ROOM1_SECONDARY_LIGHT_CONFIG_H
#define ROOM1_SECONDARY_LIGHT_CONFIG_H

#include <Arduino.h>

namespace config {
    const char WIFI_SSID[] = "Solomaha";
    const char WIFI_PASSWORD[] = "solomakha21";

    const auto MQTT_HOST = IPAddress(192, 168, 1, 230);
    const uint16_t MQTT_PORT = 1883;
    const char MQTT_ID[] = "room1-plant";
    const char MQTT_PASSWORD[] = "hjdfhukfunecagngnfefrvjkhmlumiyervgdf";

    const uint8_t PUMP_PIN = D5;

    const uint8_t SENSOR_READ_INTERVAL = 60 * 3;    // 3 minutes

    const uint16_t SENSOR_CALIBRATION_MIN = 361;
    const uint16_t SENSOR_CALIBRATION_MAX = 797;
}

#endif
