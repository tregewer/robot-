#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* ssid = "";
const char* password = "";
#define BOT_TOKEN ""

WiFiClientSecure client; //создаем защищенный HTTPS клиент
UniversalTelegramBot bot(BOT_TOKEN, client); //инициализация бота с токеном

void setup() {
  Serial.begin(115200);
  pinMode(10, OUTPUT);
  pinMode(9, OUTPUT); //скорость
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
  pinMode(D8, OUTPUT);
  pinMode(D4, OUTPUT); // назначаем trigPin, как выход
  pinMode(D2, INPUT); // назначаем echoPin, как вход

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Подключение к Wi-Fi...");
  }
  Serial.println("Подключено к Wi-Fi!");

  client.setInsecure();
}

void ey(String chat_id){
  int cm,du;
  digitalWrite(D4, LOW); // изначально датчик не посылает сигнал
  delayMicroseconds(2); // ставим задержку в 2 ммикросекунд
  digitalWrite(D4, HIGH); // посылаем сигнал
  delayMicroseconds(10); // ставим задержку в 10 микросекунд
  digitalWrite(D4, LOW); // выключаем сигнал

  du = pulseIn(D2, HIGH); // включаем прием сигнала

  cm = du / 58; // вычисляем расстояние в сантиметрах

  Serial.print(cm); // выводим расстояние в сантиметрах
  bot.sendMessage(chat_id, "До препятствия: " + String(cm) + " см", "");
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {

    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    // Команда HELP
    if (text == "/help") {
      String helpMessage = 
        "Список команд:\n\n"
        "/up X - движение вперед на X секунд\n"
        "/down X - движение назад на X секунд\n"
        "/left X - поворот влево на X секунд\n"
        "/right X - поворот вправо на X секунд\n"
        "/help - показать список команд\n\n"
        "/stop - мгновенная остановка";

      bot.sendMessage(chat_id, helpMessage, "");
      continue;   // чтобы дальше код не выполнялся
    }

    // МГНОВЕННАЯ ОСТАНОВКА 
    if (text == "/stop") {

      digitalWrite(D5, LOW);
      digitalWrite(D6, LOW);
      digitalWrite(D7, LOW);
      digitalWrite(D8, LOW);

      analogWrite(S2, 0);
      analogWrite(S3, 0);

      bot.sendMessage(chat_id, "Моторы остановлены", "");
      continue;
    }
    // Движение вперед 
    if (text.startsWith("/up ")) {
      int ti = text.substring(4).toInt() * 1000;

      digitalWrite(D7, HIGH);
      digitalWrite(D6, HIGH);
      analogWrite(S2, 512);
      analogWrite(S3, 512);

      delay(ti);

      digitalWrite(D7, LOW);
      digitalWrite(D6, LOW);

      ey(chat_id);
    }

    // Назад 
    if (text.startsWith("/down ")) {
      int ti = text.substring(6).toInt() * 1000;

      digitalWrite(D8,HIGH);
      digitalWrite(D5,HIGH);
      analogWrite(S2, 512);
      analogWrite(S3, 512);

      delay(ti);

      digitalWrite(D8,LOW);
      digitalWrite(D5,LOW);

      ey(chat_id);
    }

    // Вправо 
    if (text.startsWith("/right ")) {
      int ti = text.substring(7).toInt() * 1000;

      digitalWrite(D7,HIGH);
      digitalWrite(D5,HIGH);
      analogWrite(S2, 512);
      analogWrite(S3, 512);

      delay(ti);

      digitalWrite(D7,LOW);
      digitalWrite(D5,LOW);

      ey(chat_id);
    }

    // Влево 
    if (text.startsWith("/left ")) {
      int ti = text.substring(6).toInt() * 1000;

      digitalWrite(D6,HIGH);
      digitalWrite(D8,HIGH);
      analogWrite(S2, 512);
      analogWrite(S3, 512);

      delay(ti);

      digitalWrite(D6,LOW);
      digitalWrite(D8,LOW);

      ey(chat_id);
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
