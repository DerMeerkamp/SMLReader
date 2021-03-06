#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"
#include "Sensor.h"

const char *VERSION = "2.1.5";

// Modifying the config version will probably cause a loss of the existig configuration.
// Be careful!
const char *CONFIG_VERSION = "1.0.2";

const char *WIFI_AP_SSID = "SMLReader";
const char *WIFI_AP_DEFAULT_PASSWORD = "";

static const SensorConfig SENSOR_CONFIGS[] = {
    {.pin = D2,
     .name = "1",
     .type = "SML",
     .numeric_only = false,
     .status_led_enabled = true,
     .status_led_inverted = true,
     .status_led_pin = LED_BUILTIN},
     {.pin = D5,
      .name = "2",
      .type = "AM2302",
      .numeric_only = false,
      .status_led_enabled = false,
      .status_led_inverted = false,
      .status_led_pin = LED_BUILTIN}};

const uint8_t NUM_OF_SENSORS = sizeof(SENSOR_CONFIGS) / sizeof(SensorConfig);

#endif
