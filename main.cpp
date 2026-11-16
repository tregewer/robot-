#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* ssid = "";
const char* password = "";
#define BOT_TOKEN ""

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
void setup() {
  Serial.begin(115200);
  delay(100); // небольшая задержка для стабильности
  // Настройка ШИМ
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  analogWriteRange(1023);
  analogWriteFreq(1000);
  // Пины направления моторов
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  // Инициализация: все моторы выключены, направления сброшены
  stopMotors();
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  // Пины ультразвука
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  // Подключение к Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Подключение к Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi подключен");
  client.setInsecure(); // для работы с Telegram без проверки сертификата
  Serial.println("Бот готов к работе");
}
