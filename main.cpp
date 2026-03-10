#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* ssid = "";
const char* password = "";
#define BOT_TOKEN ""

WiFiClientSecure client; //создаем защищенный HTTPS клиент
UniversalTelegramBot bot(BOT_TOKEN, client); //инициализация бота с токеном

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

// Константы скоростей
#define SPEED_NORMAL 700    // обычная скорость
#define SPEED_TURN_FAST 900  // быстрое колесо при повороте
#define SPEED_TURN_SLOW 500  // медленное колесо при повороте
#define MAX_SPEED 1023       // максимальная скорость

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

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Подключение к Wi-Fi...");
  }
  Serial.println("Подключено к Wi-Fi!");

// Очищаем очередь старых сообщений
    WiFiClientSecure client2;
  client2.setInsecure();
  if (client2.connect("api.telegram.org", 443)) {
    String request = "GET /bot";
    request += BOT_TOKEN;
    request += "/deleteWebhook?drop_pending_updates=true HTTP/1.1\r\n";
    request += "Host: api.telegram.org\r\n";
    request += "Connection: close\r\n\r\n";
    client2.print(request);
    // Ждём ответа (необязательно, можно просто закрыть)
    while (client2.connected()) {
      String line = client2.readStringUntil('\n');
      if (line == "\r") break;
    }
    client2.stop();
    Serial.println("Очередь старых сообщений очищена");
  } else {
    Serial.println("Не удалось очистить очередь");
  }

  client.setInsecure();
}

// Функция остановки моторов
void stopMotors() {
  digitalWrite(ENA, 0);
  digitalWrite(ENB, 0);
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
    text.trim();

    // Отладочный вывод в Serial
    Serial.println("Получено сообщение: '" + text + "' от chat_id: " + chat_id);

    // Удаляем возможное имя бота (часть после @)
    String command = text;
    int atPos = command.indexOf('@');
    if (atPos > 0) {
      command = command.substring(0, atPos);
    }

    // Отделяем аргументы (всё после первого пробела)
    String args = "";
    int spacePos = command.indexOf(' ');
    if (spacePos > 0) {
      args = command.substring(spacePos + 1);
      command = command.substring(0, spacePos);
    }
    args.trim();
    command.trim();

      // Команда HELP
    if (text == "/help" || text.equalsIgnoreCase("/help")) {
      String helpMessage = 
        "Список команд:\n\n"
        "/up X - движение вперед на X секунд\n"
        "/down X - движение назад на X секунд\n"
        "/left X - поворот влево на X секунд\n"
        "/right X - поворот вправо на X секунд\n"
        "/turn90 left/right - поворот на 90 градусов\n"
        "/arc L 3 300 (дуга: L или R, время сек, разница скоростей)\n"
        "/help - показать список команд\n\n";
      bot.sendMessage(chat_id, helpMessage, "");
      continue;  
    }
    
    // ВПЕРЕД
    else if (command == "/up") {
      int ti = args.toInt() * 1000; // переводим секунды в миллисекунды
      if (ti <= 0) {
        bot.sendMessage(chat_id, "Укажите положительное время (сек), например /up 2", "");
        return;
      }

      unsigned long duration = sec * 1000UL;
      unsigned long start = millis();

      // Левое колесо вперёд, правое назад
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);
      analogWrite(ENA, SPEED_NORMAL);
      analogWrite(ENB, SPEED_NORMAL);

      bool aborted = false;
      while (millis() - start < duration) {
        int dist = getDistance();
        if (dist <= 10) {
          stopMotors();
          bot.sendMessage(chat_id, " Препятствие ближе 10 см! Поворот вправо остановлен.", "");
          aborted = true;
          break;
        }
        delay(50);
      }

      if (!aborted) {
        stopMotors();
        ey(chat_id);
      }
    }

      // Направление: оба мотора вперёд
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);

      analogWrite(ENA, SPEED_NORMAL);
      analogWrite(ENB, SPEED_NORMAL);
      delay(ti);
      stopMotors();
      ey(chat_id); // измеряем расстояние после движения
    }
// НАЗАД
    else if (command == "/down") {
      int ti = args.toInt() * 1000;
      if (ti <= 0) {
        bot.sendMessage(chat_id, "Укажите положительное время (сек), например /down 2", "");
        return;
      }

      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);

      analogWrite(ENA, SPEED_NORMAL);
      analogWrite(ENB, SPEED_NORMAL);

      delay(ti);
      stopMotors();
      ey(chat_id);
    }

    // ВПРАВО (поворот на месте)
    else if (command == "/right") {
      int ti = args.toInt() * 1000;
      if (ti <= 0) {
        bot.sendMessage(chat_id, "Укажите положительное время (сек), например /right 1", "");
        return;
      }

      // Левое колесо вперёд, правое назад
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);

      analogWrite(ENA, SPEED_NORMAL);
      analogWrite(ENB, SPEED_NORMAL);

      delay(ti);
      stopMotors();
      ey(chat_id);
    }

    // ВЛЕВО (поворот на месте)
    else if (command == "/left") {
      int ti = args.toInt() * 1000;
      if (ti <= 0) {
        bot.sendMessage(chat_id, "Укажите положительное время (сек), например /left 1", "");
        return;
      }

      // Левое назад, правое вперёд
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);

      analogWrite(ENA, SPEED_NORMAL);
      analogWrite(ENB, SPEED_NORMAL);

      delay(ti);
      stopMotors();
      ey(chat_id);
    }
//ПОВОРОТ НА 90 ГРАДУСОВ
else if (command == "/turn90") {
    String direction = args;
    direction.toLowerCase();
    
    if (direction != "left" && direction != "right") {
        bot.sendMessage(chat_id, "Укажите направление: /turn90 left или /turn90 right", "");
        return;
    }

    int turnTime = 1500; // 1.5 секунды 
    int speedDiff = 400; // разница скоростей для поворота
    
    bot.sendMessage(chat_id, "Поворачиваю " + direction + " на 90° (передний привод)", "");
    
    if (direction == "left") {
        // Поворот налево: правое быстрее левого
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        
        analogWrite(ENA, SPEED_TURN_SLOW );  // левое медленнее
        analogWrite(ENB, SPEED_TURN_FAST );  // правое быстрее
    } 
    else {
        // Поворот направо: левое быстрее правого
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        
        analogWrite(ENA, SPEED_TURN_FAST);  // левое быстрее
        analogWrite(ENB, SPEED_TURN_SLOW );  // правое медленнее
    }
    
    delay(turnTime);
    stopMotors();
    ey(chat_id);
}
  // ДУГА
    else if (command == "/arc") {
      char dirStr[10]; // ОБЯЗАТЕЛЬНО объявляем переменную-строку
      int timeSec = 0;
      int diff = 0;

      // Используем %s (строка), и перед dirStr НЕ ставим знак &, так как это массив
      int parsed = sscanf(args.c_str(), "%s %d %d", dirStr, &timeSec, &diff);

      // Если не удалось прочитать все 3 параметра
      if (parsed != 3) {
        String errMsg = "Ошибка формата!\nКонтроллер увидел аргументы: '" + args + "'\nПример: /arc L 3 300";
        bot.sendMessage(chat_id, errMsg, "");
        return;
      }

      // Забираем первую букву из полученного текста
      char direction = dirStr[0];

      if (timeSec <= 0 || diff < 0) {
        bot.sendMessage(chat_id, "Время должно быть больше нуля, а разница скоростей — не отрицательной", "");
        return;
      }

      int ti = timeSec * 1000;
      int baseSpeed = MAX_SPEED; // Максимальная мощность для поворота
      int leftSpeed = baseSpeed;
      int rightSpeed = baseSpeed;

      // Определяем, какое колесо притормаживает
      if (direction == 'L' || direction == 'l') {
        leftSpeed = baseSpeed - diff;
      } else if (direction == 'R' || direction == 'r') {
        rightSpeed = baseSpeed - diff;
      } else {
        bot.sendMessage(chat_id, "Направление должно быть L или R", "");
        return;
      }

      // Защита от выхода за пределы ШИМ
      leftSpeed = constrain(leftSpeed, 0, MAX_SPEED);
      rightSpeed = constrain(rightSpeed, 0, MAX_SPEED);

      // Направление: оба мотора вперёд
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);

      // Скорость
      analogWrite(ENA, leftSpeed);
      analogWrite(ENB, rightSpeed);

      // Ждем, останавливаем и меряем расстояние
      delay(ti);
      stopMotors();
      ey(chat_id);
    }
    else {
      bot.sendMessage(chat_id, "Неизвестная команда. /help", "");
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
