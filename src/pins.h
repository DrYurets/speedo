#pragma once
#include <Arduino.h>

// Распиновка ESP32-C3 Super Mini — см. AGENTS.md
// Strapping-пины (2, 8, 9) не используются для внешней нагрузки,
// кроме GPS TX→GPIO2 (GPS не драйвит линию при сбросе) и LED на GPIO8.

constexpr uint8_t PIN_OLED_SDA = 7;   // I2C data
constexpr uint8_t PIN_OLED_SCL = 6;   // I2C clock

constexpr uint8_t PIN_GPS_RX   = 3;   // ESP RX <- GPS TX (NMEA 9600)
constexpr uint8_t PIN_GPS_TX   = 2;   // ESP TX -> GPS RX (опц., команды CASIC)
constexpr uint8_t PIN_GPS_PPS  = 10;  // опционально, сейчас не используется

constexpr uint8_t PIN_BTN_MODE = 4;   // кнопка на корпусе, на GND
constexpr uint8_t PIN_BTN_SET  = 1;   // кнопка на корпусе, на GND
constexpr uint8_t PIN_BTN_BACK = 5;   // кнопка на корпусе, на GND

constexpr uint8_t PIN_BAT_ADC  = 0;   // ADC1_CH0, делитель 100k/22k
constexpr uint8_t PIN_LED_FIX  = 8;   // штатный LED платы, активен LOW
