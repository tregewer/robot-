#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* ssid = "ASF";
const char* password = "12345678";
#define BOT_TOKEN "8599691946:AAGEw8Oemk4joTCStf_uvMSJib9Q7G4--0A"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Пины для управления моторами (L298N)
#define ENA D1   // ШИМ скорость левого мотора
#define ENB D3   // ШИМ скорость правого мотора
#define IN1 D5   // Направление левого мотора
#define IN2 D6   // Направление левого мотора
#define IN3 D7   // Направление правого мотора
#define IN4 D8   // Направление правого мотора

// Пины для ультразвукового датчика HC-SR04
#define TRIG D4
#define ECHO D2
