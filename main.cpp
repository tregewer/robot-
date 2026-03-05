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

  client.setInsecure();
}

// Функция остановки моторов
void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
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
        "/help - показать список команд\n\n";
      
      bot.sendMessage(chat_id, helpMessage, "");
      continue;  
    }
    
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
    if (text.startsWith("/left ")) {
      int ti = text.substring(6).toInt() * 1000;

      digitalWrite(D6,HIGH);
      digitalWrite(D8,HIGH);
      analogWrite(S2, 512);
      analogWrite(S3, 512); //1023
      delay(ti);
      digitalWrite(D6,LOW);
      digitalWrite(D8,LOW);
      ey(chat_id);
      }
    else {
      // Если команда не распознана, можно отправить подсказку
      bot.sendMessage(chat_id, "Неизвестная команда. Введите /help для списка команд.", "");
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
