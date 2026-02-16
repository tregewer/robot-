#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* ssid = "";
const char* password = "";
#define BOT_TOKEN ""

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

void setup() {
  Serial.begin(115200);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
  pinMode(D8, OUTPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Подключение к Wi-Fi...");
  }
  Serial.println("Подключено к Wi-Fi!");

  client.setInsecure();
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text; //перебор сообщений

    if (text == "/up") {
      digitalWrite(D8,HIGH);
      digitalWrite(D6,HIGH);
      delay(5000); //!!!!!!!!!!!!!!!!!
      digitalWrite(D8,LOW);
      digitalWrite(D6,LOW);
    }
    if (text == "/down") {
      digitalWrite(D7,HIGH);
      digitalWrite(D5,HIGH);
      delay(5000); //!!!!!!!!!!!!!!!!!
      digitalWrite(D7,LOW);
      digitalWrite(D5,LOW);
    }
    if (text == "/right") {
      digitalWrite(D8,HIGH);
      digitalWrite(D5,HIGH);
      delay(5000); //!!!!!!!!!!!!!!!!!
      digitalWrite(D8,LOW);
      digitalWrite(D5,LOW);
    }
    if (text == "/left") {
      digitalWrite(D6,HIGH);
      digitalWrite(D7,HIGH);
      delay(5000); //!!!!!!!!!!!!!!!!!
      digitalWrite(D6,LOW);
      digitalWrite(D7,LOW);
    }
  }
}

void loop() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1); //id сообщения

  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  } 
  delay(1000);
}
