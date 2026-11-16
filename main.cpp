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
void setup() {
  Serial.begin(115200);
  delay(100);

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

  // Все моторы выключены
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

  client.setInsecure();
  Serial.println("Бот готов к работе");
}

// Остановка моторов
void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
// Измерение расстояния и отправка в Telegram
void ey(String chat_id) {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);
  int distance = duration / 58;

  if (duration == 0 || distance >= 400) {
    bot.sendMessage(chat_id, "Расстояние превышено (более 4 м).", "");
  } else {
    bot.sendMessage(chat_id, "До препятствия: " + String(distance) + " см", "");
  }
}
// Обработка входящих сообщений
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    text.trim();

    Serial.println("Получено сообщение: '" + text + "' от chat_id: " + chat_id);

    // Отделяем число после пробела
    String command = text;
    String args = "";
    int spacePos = command.indexOf(' ');
    if (spacePos > 0) {
      args = command.substring(spacePos + 1);
      command = command.substring(0, spacePos);
    }

    // Убираем имя бота после @
    int atPos = command.indexOf('@');
    if (atPos > 0) {
      command = command.substring(0, atPos);
    }
    args.trim();
    command.trim();

    // HELP
    if (command == "/help") {
      String helpMessage =
        "Команды:\n"
        "/up X (движение вперёд X секунд)\n"
        "/down X (назад)\n"
        "/left X (влево)\n"
        "/right X (вправо)\n"
        "/arc L 3 300 (дуга: L или R, время сек, разница скоростей)\n"
        "Пример: /up 2";
      bot.sendMessage(chat_id, helpMessage, "");
    }
// ВПЕРЕД
    else if (command == "/up") {
      int ti = args.toInt() * 1000;
      if (ti <= 0) {
        bot.sendMessage(chat_id, "Укажите положительное время (сек), например /up 2", "");
        continue;
      }

      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);

      analogWrite(ENA, 700);
      analogWrite(ENB, 700);
      delay(ti);
      stopMotors();
      ey(chat_id);
    }
// НАЗАД
    else if (command == "/down") {
      int ti = args.toInt() * 1000;
      if (ti <= 0) {
        bot.sendMessage(chat_id, "Укажите положительное время (сек), например /down 2", "");
        continue;
      }

      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);

      analogWrite(ENA, 700);
      analogWrite(ENB, 700);
      delay(ti);
      stopMotors();
      ey(chat_id);
    }

    // ВПРАВО
    else if (command == "/right") {
      int ti = args.toInt() * 1000;
      if (ti <= 0) {
        bot.sendMessage(chat_id, "Укажите положительное время (сек), например /right 1", "");
        continue;
      }

      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);

      analogWrite(ENA, 700);
      analogWrite(ENB, 700);
      delay(ti);
      stopMotors();
      ey(chat_id);
    }

    // ВЛЕВО
    else if (command == "/left") {
      int ti = args.toInt() * 1000;
      if (ti <= 0) {
        bot.sendMessage(chat_id, "Укажите положительное время (сек), например /left 1", "");
        continue;
      }

      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);

      analogWrite(ENA, 700);
      analogWrite(ENB, 700);
      delay(ti);
      stopMotors();
      ey(chat_id);
    }
// ДУГА
    else if (command == "/arc") {
      char dirStr[10];
      int timeSec = 0;
      int diff = 0;

      int parsed = sscanf(args.c_str(), "%9s %d %d", dirStr, &timeSec, &diff);

      if (parsed != 3) {
        String errMsg = "Ошибка формата!\nКонтроллер увидел аргументы: '" + args + "'\nПример: /arc L 3 300";
        bot.sendMessage(chat_id, errMsg, "");
        continue;
      }

      char direction = dirStr[0];

      if (timeSec <= 0 || diff < 0) {
        bot.sendMessage(chat_id, "Время должно быть больше нуля, а разница скоростей — не отрицательной", "");
        continue;
      }

      int ti = timeSec * 1000;
      int baseSpeed = 1023;
      int leftSpeed = baseSpeed;
      int rightSpeed = baseSpeed;

      if (direction == 'L' || direction == 'l') {
        leftSpeed = baseSpeed - diff;
      } else if (direction == 'R' || direction == 'r') {
        rightSpeed = baseSpeed - diff;
      } else {
        bot.sendMessage(chat_id, "Направление должно быть L или R", "");
        continue;
      }

      leftSpeed = constrain(leftSpeed, 0, 1023);
      rightSpeed = constrain(rightSpeed, 0, 1023);

      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);

      analogWrite(ENA, leftSpeed);
      analogWrite(ENB, rightSpeed);
      delay(ti);
      stopMotors();
      ey(chat_id);
    }

    else {
      bot.sendMessage(chat_id, "Неизвестная команда. /help", "");
    }
  }
}
