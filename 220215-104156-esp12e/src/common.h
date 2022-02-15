#ifndef COMMON_H
#define DEBUG_MODE 0
#define COMMON_H
#include <SPI.h>

#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #define PIN_USER_INPUT 34
  #define MAX_INPUT_VALUE 4096
  #define KEY_IDLE_MIN_VALUE  1970
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #define PIN_USER_INPUT A0
  #define PIN_CONFIG_ENABLE D1
  #define PIN_GND_RELAY_ENABLE D6
  #define PIN_BRAKE_BYPASS_RELAY D2
  #define MAX_INPUT_VALUE 1024
  #define KEY_IDLE_MIN_VALUE  MAX_INPUT_VALUE
#endif

#define STATE_KEY_DOWN 1
#define STATE_KEY_UP 0
#define SERIAL_BAUDRATE 115200

#define INVALID_COMMAND 0xFF
#define KEY_INPUT_TOLERANCE 50
#define MAX_KEYNAME_LENGTH 15
#define HU_KEY_PRESS_WIDTH 1000

#define WORDLSBYTE(varInt16) (*((unsigned char*)&varInt16))
#define WORDMSBYTE(varInt16) (*((unsigned char*)&(varInt16)+1))

#endif
