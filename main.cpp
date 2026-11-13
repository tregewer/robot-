#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ---------- Сеть и бот ----------
const char* WIFI_SSID     = "";
const char* WIFI_PASSWORD = "";
#define BOT_TOKEN ""

// Кому можно управлять: chat_id через запятую, например "123456789,987654321".
// Пусто — управлять может любой (свой chat_id бот пришлёт в ответ на /start).
const char* ALLOWED_IDS = "";

// ---------- Пины (как в исходной схеме) ----------
#define ENA  D1   // ШИМ левого мотора
#define ENB  D3   // ШИМ правого мотора
#define IN1  D5   // направление левого мотора
#define IN2  D6
#define IN3  D7   // направление правого мотора
#define IN4  D8
#define TRIG D4   // HC-SR04: запуск замера
#define ECHO D2   // HC-SR04: эхо (через делитель 5 В -> 3,3 В)

// ---------- Параметры ----------
#define PWM_MAX          1023   // максимум ШИМ
#define MIN_SPEED_PCT    30     // ниже моторы обычно не трогаются
#define MAX_MOVE_SEC     10.0   // максимум для одной команды движения
#define STOP_DIST_CM     20     // ближе — аварийная остановка
#define FAR_CM           400    // эхо не вернулось = свободно минимум на 4 м
#define SENSOR_PERIOD_MS 60     // пауза между замерами HC-SR04
#define ECHO_TIMEOUT_US  25000  // ждать эхо не дольше 25 мс (~4 м)
#define STUCK_MS         800    // едем вперёд, а расстояние не меняется столько мс — застряли
#define RAMP_MS          250    // время плавного разгона
#define BRAKE_MS         80     // время активного торможения
#define AUTO_SEGMENT_MS  2000   // автопилот: длина одного отрезка вперёд
#define AUTO_MAX_SEC     300    // автопилот: максимум по времени
#define ROUTE_MAX_STEPS  20     // маршрут: максимум шагов
#define POLL_MS          1000   // как часто спрашивать Telegram

// Кнопки под полем ввода в Telegram
const char* KEYBOARD =
  "[[\"/left 0.5\",\"/up 1\",\"/right 0.5\"],"
  "[\"/turn -90\",\"/down 1\",\"/turn 90\"],"
  "[\"/dist\",\"/auto 60\",\"/stop\"],"
  "[\"/status\",\"/help\"]]";

// Меню команд (кнопка «Меню» слева от поля ввода)
const char* COMMANDS_MENU =
  "[{\"command\":\"start\",\"description\":\"Кнопки управления\"},"
  "{\"command\":\"help\",\"description\":\"Список команд\"},"
  "{\"command\":\"status\",\"description\":\"Состояние робота\"},"
  "{\"command\":\"dist\",\"description\":\"Расстояние до препятствия\"},"
  "{\"command\":\"auto\",\"description\":\"Автопилот\"},"
  "{\"command\":\"stop\",\"description\":\"Стоп\"}]";

const char* HELP_TEXT =
  "🤖 Команды робота\n\n"
  "Движение (секунды, можно дробные):\n"
  "/up 1.5 — вперёд\n"
  "/down 1 — назад\n"
  "/left 0.5 — разворот влево на месте\n"
  "/right 0.5 — разворот вправо на месте\n"
  "/turn 90 — поворот на угол (минус — влево)\n"
  "/turn90 left|right — поворот на 90°\n"
  "/arc L 3 300 — дуга: сторона, секунды, разница ШИМ\n"
  "/route F2 R90 F1 L45 B0.5 — маршрут\n"
  "   F/B — вперёд/назад (сек), L/R — влево/вправо (градусы)\n\n"
  "Режимы:\n"
  "/auto 60 — автопилот с объездом препятствий\n"
  "/stop — стоп\n\n"
  "Настройки и данные:\n"
  "/speed 70 — скорость 30–100 %\n"
  "/calib 600 — сколько мс длится поворот на 90°\n"
  "/dist — расстояние до препятствия\n"
  "/status — состояние робота\n\n"
  "Вперёд робот сам тормозит, если до препятствия меньше 20 см.";

// ---------- Объекты и состояние ----------
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

int speedPct     = 70;    // скорость движения, %
int turnSpeedPct = 75;    // скорость разворота на месте, %
int turn90Ms     = 600;   // время разворота на 90° (меняется командой /calib)
int lastDistance = -1;    // последний замер, см (-1 — ещё не мерили)
unsigned long commandsDone = 0;
unsigned long lastPoll = 0;

bool autoMode = false;              // автопилот включён
unsigned long autoStarted = 0;      // когда включили
unsigned long autoDurationMs = 0;   // на сколько включили
int autoAvoided = 0;                // сколько препятствий объехал
String autoChat = "";               // кому прислать отчёт
bool wifiLostDuringAuto = false;    // автопилот остановлен из-за потери Wi-Fienum MoveResult { MOVE_OK, MOVE_OBSTACLE, MOVE_BLOCKED, MOVE_STUCK };

// ================== Моторы ==================

// Направление колеса: 1 — вперёд, -1 — назад, 0 — отпущено
void setLeft(int dir) {
  digitalWrite(IN1, dir < 0 ? HIGH : LOW);
  digitalWrite(IN2, dir > 0 ? HIGH : LOW);
}

void setRight(int dir) {
  digitalWrite(IN3, dir > 0 ? HIGH : LOW);
  digitalWrite(IN4, dir < 0 ? HIGH : LOW);
}

void setPwm(int left, int right) {
  analogWrite(ENA, constrain(left, 0, PWM_MAX));
  analogWrite(ENB, constrain(right, 0, PWM_MAX));
}

int pctToPwm(int pct) {
  return map(constrain(pct, 0, 100), 0, 100, 0, PWM_MAX);
}

// Остановка. brake = true: сначала короткое торможение (у L298N при IN1=IN2 и
// включённом EN обмотка замыкается и мотор резко встаёт), потом моторы отпускаются.
void stopMotors(bool brake = true) {
  if (brake) {
    setLeft(0);
    setRight(0);
    setPwm(PWM_MAX, PWM_MAX);
    delay(BRAKE_MS);
  }
  setPwm(0, 0);
  setLeft(0);
  setRight(0);
}

// ================== Дальномер ==================

// Один замер, см. -1 — эхо не вернулось (дальше ~4 м или датчик не ответил).
int readDistanceOnce() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  unsigned long us = pulseIn(ECHO, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return -1;
  return (int)(us / 58);
}

// Медиана нескольких замеров: случайные выбросы датчика отбрасываются.
int readDistance(int samples = 5) {
  int v[9];
  samples = constrain(samples, 1, 9);
  for (int i = 0; i < samples; i++) {
    int d = readDistanceOnce();
    v[i] = (d > 0) ? d : FAR_CM;
    if (i < samples - 1) delay(SENSOR_PERIOD_MS);
  }
  for (int i = 1; i < samples; i++) {        // сортировка вставками
    int x = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > x) {
      v[j + 1] = v[j];
      j--;
    }
    v[j + 1] = x;
  }
  lastDistance = min(v[samples / 2], FAR_CM);
  return lastDistance;
}

String distText(int d) {
  if (d < 0) return "нет данных";
  if (d >= FAR_CM) return "больше 4 м";
  return String(d) + " см";
}

// ================== Движение ==================

// Едет ms миллисекунд. ldir/rdir — направления колёс, lpwm/rpwm — ШИМ.
// Если оба колеса крутятся вперёд, каждые 60 мс смотрит вперёд: тормозит перед
// препятствием и замечает застревание (упёрся боком в угол, которого датчик не видит).
// Если препятствие уже вплотную, вперёд не едет совсем.
MoveResult drive(int ldir, int rdir, int lpwm, int rpwm, unsigned long ms) {
  bool checkFront = (ldir > 0 && rdir > 0);
  if (checkFront) {
    int d = readDistance(3);
    if (d < STOP_DIST_CM) return MOVE_BLOCKED;
  }
  setLeft(ldir);
  setRight(rdir);
  unsigned long start = millis();
  unsigned long lastCheck = start;
  int closeHits = 0;
  int refD = -1;                     // опорное расстояние для детектора застревания
  unsigned long refTime = start;     // когда оно последний раз заметно менялось
  while (millis() - start < ms) {
    unsigned long t = millis() - start;
    // Плавный разгон: мощность растёт с 40% до 100% за RAMP_MS
    float k = (t < RAMP_MS) ? 0.4f + 0.6f * t / RAMP_MS : 1.0f;
    setPwm((int)(lpwm * k), (int)(rpwm * k));
    if (checkFront && millis() - lastCheck >= SENSOR_PERIOD_MS) {
      lastCheck = millis();
      int d = readDistanceOnce();
      closeHits = (d > 0 && d < STOP_DIST_CM) ? closeHits + 1 : 0;
      if (closeHits >= 2) {              // два замера подряд — не случайный выброс
        stopMotors();
        lastDistance = d;
        return MOVE_OBSTACLE;
      }
      if (d > 0) {                       // детектор застревания
        if (refD < 0 || abs(d - refD) >= 4) {
          refD = d;
          refTime = millis();
        } else if (millis() - refTime > STUCK_MS) {
          stopMotors();
          lastDistance = d;
          return MOVE_STUCK;
        }
      }
    }
    delay(5);
  }
  stopMotors();
  return MOVE_OK;
}// Разворот на месте: deg > 0 — вправо, deg < 0 — влево.
// Время считается из калибровки: turn90Ms мс на каждые 90°.
void turnDegrees(int deg) {
  if (deg == 0) return;
  unsigned long ms = (unsigned long)abs(deg) * turn90Ms / 90;
  int pwm = pctToPwm(turnSpeedPct);
  if (deg > 0) drive(1, -1, pwm, pwm, ms);
  else         drive(-1, 1, pwm, pwm, ms);
}

// ================== Telegram: вспомогательное ==================

void reply(const String& chat, const String& text) {
  bot.sendMessage(chat, text, "");
}

bool isAllowed(const String& chatId) {
  String list = ALLOWED_IDS;
  list.replace(" ", "");
  if (list.length() == 0) return true;
  return ("," + list + ",").indexOf("," + chatId + ",") >= 0;
}

// Слово номер index из строки (слова разделены пробелами). "" — если его нет.
String word(const String& s, int index) {
  int len = s.length();
  int pos = 0;
  int n = 0;
  while (pos < len) {
    while (pos < len && s[pos] == ' ') pos++;
    if (pos >= len) break;
    int end = s.indexOf(' ', pos);
    if (end < 0) end = len;
    if (n == index) return s.substring(pos, end);
    n++;
    pos = end;
  }
  return "";
}

// Секунды из текста ("1.5" или "1,5") в миллисекунды. Пусто — 1 с. Ошибка — 0.
unsigned long parseMs(String arg) {
  if (arg.length() == 0) return 1000;
  arg.replace(",", ".");
  float s = arg.toFloat();
  if (s < 0.1 || s > MAX_MOVE_SEC) return 0;
  return (unsigned long)(s * 1000);
}

// Почему движение прервалось — текстом
String reasonText(MoveResult r) {
  if (r == MOVE_BLOCKED)  return "впереди препятствие, " + distText(lastDistance) + ", не поехал";
  if (r == MOVE_OBSTACLE) return "остановился перед препятствием, " + distText(lastDistance);
  if (r == MOVE_STUCK)    return "похоже, застрял: колёса крутятся, а расстояние не меняется. Остановился";
  return "";
}

// Ответ после движения: чем закончилось и сколько до препятствия
void reportMove(const String& chat, MoveResult r, const String& what) {
  if (r == MOVE_OK)
    reply(chat, "✅ " + what + ". До препятствия: " + distText(readDistance(5)) + ".");
  else
    reply(chat, "⚠️ " + what + ": " + reasonText(r) + ".");
}

// Выключает автопилот и присылает отчёт
void stopAuto(const String& reason) {
  if (!autoMode) return;
  autoMode = false;
  stopMotors();
  unsigned long sec = (millis() - autoStarted) / 1000;
  reply(autoChat, "🤖 Автопилот выключен (" + reason + "). Ехал " + String(sec) +
                  " с, объехал препятствий: " + String(autoAvoided) + ".");
}

// ================== Команды ==================

void handleCommand(const String& chat, const String& name, const String& cmd, const String& args) {
  commandsDone++;
  int pwm = pctToPwm(speedPct);

  if (cmd == "/start") {
    String msg = "Привет, " + name + "! Ваш chat_id: " + chat + "\n";
    if (String(ALLOWED_IDS).length() == 0)
      msg += "⚠️ Сейчас управлять может любой. Впишите этот id в ALLOWED_IDS.\n";
    msg += "\n";
    msg += HELP_TEXT;
    bot.sendMessageWithReplyKeyboard(chat, msg, "", KEYBOARD, true);
    return;
  }

  if (cmd == "/help") {
    reply(chat, HELP_TEXT);
    return;
  }

  if (cmd == "/up"  cmd == "/down"  cmd == "/left" || cmd == "/right") {
    unsigned long ms = parseMs(args);
    if (ms == 0) {
      reply(chat, "Время от 0.1 до 10 с, например " + cmd + " 1.5");
      return;
    }
    stopAuto("ручное управление");
    MoveResult r;
    String what;
    if (cmd == "/up")        { r = drive(1, 1, pwm, pwm, ms);   what = "Вперёд"; }
    else if (cmd == "/down") { r = drive(-1, -1, pwm, pwm, ms); what = "Назад"; }
    else if (cmd == "/left") { r = drive(-1, 1, pwm, pwm, ms);  what = "Разворот влево"; }
    else                     { r = drive(1, -1, pwm, pwm, ms);  what = "Разворот вправо"; }
    reportMove(chat, r, what + " " + String(ms / 1000.0, 1) + " с");
    return;
  }if (cmd == "/turn" || cmd == "/turn90") {
    int deg = 0;
    if (cmd == "/turn90") {
      String a = args;
      a.toLowerCase();
      if (a == "left") deg = -90;
      else if (a == "right") deg = 90;
    } else {
      deg = args.toInt();
    }
    if (deg == 0 || abs(deg) > 360) {
      reply(chat, "Пример: /turn 90 (вправо), /turn -45 (влево), /turn90 left");
      return;
    }
    stopAuto("ручное управление");
    turnDegrees(deg);
    reply(chat, "↻ Повернул на " + String(deg) + "°. Впереди: " + distText(readDistance(5)) + ".");
    return;
  }

  if (cmd == "/arc") {
    String side = word(args, 0);
    side.toUpperCase();
    unsigned long ms = parseMs(word(args, 1));
    String diffStr = word(args, 2);
    int diff = diffStr.toInt();
    if ((side != "L" && side != "R")  diffStr.length() == 0  ms == 0  diff < 0  diff > PWM_MAX) {
      reply(chat, "Формат: /arc L|R секунды разница(0–1023). Пример: /arc L 3 300");
      return;
    }
    stopAuto("ручное управление");
    int l = PWM_MAX;
    int r = PWM_MAX;
    if (side == "L") l -= diff;     // притормаживаем колесо с той стороны, куда поворачиваем
    else             r -= diff;
    reportMove(chat, drive(1, 1, l, r, ms), "Дуга " + side);
    return;
  }

  if (cmd == "/route") {
    char type[ROUTE_MAX_STEPS];
    float value[ROUTE_MAX_STEPS];
    int n = 0;
    for (int i = 0; ; i++) {
      String w = word(args, i);
      if (w.length() == 0) break;
      if (n >= ROUTE_MAX_STEPS) {
        reply(chat, "Слишком длинный маршрут: максимум " + String(ROUTE_MAX_STEPS) + " шагов.");
        return;
      }
      char t = toupper(w[0]);
      String num = w.substring(1);
      num.replace(",", ".");
      float v = num.toFloat();
      bool ok = false;
      if (t == 'F' || t == 'B') ok = (v >= 0.1 && v <= MAX_MOVE_SEC);
      if (t == 'L' || t == 'R') ok = (v >= 1 && v <= 360);
      if (!ok) {
        reply(chat, "Ошибка в шаге «" + w + "». Пример: /route F2 R90 F1 L45 B0.5");
        return;
      }
      type[n] = t;
      value[n] = v;
      n++;
    }
    if (n == 0) {
      reply(chat, "Пример: /route F2 R90 F1 L45 B0.5");
      return;
    }
    stopAuto("ручное управление");
    reply(chat, "🗺 Маршрут: " + String(n) + " шагов, поехал.");
    for (int i = 0; i < n; i++) {
      MoveResult r = MOVE_OK;
      if (type[i] == 'F')      r = drive(1, 1, pwm, pwm, (unsigned long)(value[i] * 1000));
      else if (type[i] == 'B') r = drive(-1, -1, pwm, pwm, (unsigned long)(value[i] * 1000));
      else turnDegrees(type[i] == 'L' ? -(int)value[i] : (int)value[i]);
      if (r != MOVE_OK) {
        reply(chat, "⚠️ Маршрут прерван на шаге " + String(i + 1) + " из " + String(n) +
                    ": " + reasonText(r) + ".");
        return;
      }
    }
    reply(chat, "🏁 Маршрут пройден. Впереди: " + distText(readDistance(5)) + ".");
    return;
  }

  if (cmd == "/auto") {
    int sec = (args.length() > 0) ? args.toInt() : 60;
    if (sec < 5 || sec > AUTO_MAX_SEC) {
      reply(chat, "Время автопилота от 5 до 300 с, например /auto 60");
      return;
    }
    autoMode = true;
    autoChat = chat;
    autoStarted = millis();
    autoDurationMs = (unsigned long)sec * 1000;
    autoAvoided = 0;
    reply(chat, "🤖 Автопилот на " + String(sec) + " с: еду вперёд и объезжаю препятствия. /stop — остановить.");
    return;
  }

  if (cmd == "/stop") {
    stopMotors();
    if (autoMode) stopAuto("команда /stop");
    else reply(chat, "⏹ Стою.");
    return;
  }

  if (cmd == "/speed") {
    if (args.length() == 0) {
      reply(chat, "Скорость: " + String(speedPct) + "%. Изменить: /speed 30–100");
      return;
    }
    int p = args.toInt();
    if (p < MIN_SPEED_PCT || p > 100) {
      reply(chat, "Скорость от 30 до 100 %, например /speed 80");
      return;
    }
    speedPct = p;
    reply(chat, "⚡ Скорость: " + String(speedPct) + "%");
    return;
  }if (cmd == "/calib") {
    if (args.length() == 0) {
      reply(chat, "Поворот на 90° длится " + String(turn90Ms) + " мс.\n"
                  "Подбор: /calib 650, затем /turn 90 — и так, пока угол не станет ровным.");
      return;
    }
    int ms = args.toInt();
    if (ms < 100 || ms > 3000) {
      reply(chat, "Значение от 100 до 3000 мс, например /calib 650");
      return;
    }
    turn90Ms = ms;
    reply(chat, "🎯 Калибровка: 90° = " + String(turn90Ms) + " мс");
    return;
  }

  if (cmd == "/dist") {
    reply(chat, "📏 До препятствия: " + distText(readDistance(5)));
    return;
  }

  if (cmd == "/status") {
    unsigned long s = millis() / 1000;
    char uptime[24];
    snprintf(uptime, sizeof(uptime), "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
    String msg = "📊 Статус\n";
    msg += "Режим: " + String(autoMode ? "автопилот" : "ручной") + "\n";
    msg += "Скорость: " + String(speedPct) + "%, поворот 90° = " + String(turn90Ms) + " мс\n";
    msg += "Последний замер: " + distText(lastDistance) + "\n";
    msg += "Wi-Fi: " + String(WiFi.RSSI()) + " dBm, IP " + WiFi.localIP().toString() + "\n";
    msg += "Работает: " + String(uptime) + "\n";
    msg += "Свободная память: " + String(ESP.getFreeHeap()) + " байт\n";
    msg += "Команд выполнено: " + String(commandsDone);
    reply(chat, msg);
    return;
  }

  reply(chat, "Неизвестная команда: " + cmd + "\n/help — список команд");
}

void handleNewMessages(int count) {
  for (int i = 0; i < count; i++) {
    String chat = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    text.trim();
    Serial.println("<- " + chat + ": " + text);
    if (text.length() == 0) continue;                // стикер, фото и т.п.
    if (!isAllowed(chat)) {
      reply(chat, "⛔ Нет доступа. Ваш chat_id: " + chat);
      continue;
    }
    if (text[0] != '/') text = "/" + text;           // "up 2" тоже сработает
    String cmd = text;
    String args = "";
    int sp = cmd.indexOf(' ');
    if (sp > 0) {
      args = cmd.substring(sp + 1);
      cmd = cmd.substring(0, sp);
    }
    int at = cmd.indexOf('@');                       // "/up@my_bot" -> "/up"
    if (at > 0) cmd = cmd.substring(0, at);
    cmd.toLowerCase();
    args.trim();
    handleCommand(chat, bot.messages[i].from_name, cmd, args);
  }
}

// ================== Автопилот ==================

// Один шаг: отрезок вперёд. Если упёрся — отъезжает, смотрит влево и вправо
// и поворачивает туда, где свободнее. Между шагами loop() опрашивает Telegram.
void autoStep() {
  if (millis() - autoStarted >= autoDurationMs) {
    stopAuto("время вышло");
    return;
  }
  int pwm = pctToPwm(speedPct);
  if (drive(1, 1, pwm, pwm, AUTO_SEGMENT_MS) == MOVE_OK) return;

  autoAvoided++;
  drive(-1, -1, pwm, pwm, 400);            // немного назад
  turnDegrees(-60);
  int leftD = readDistance(3);             // что слева
  turnDegrees(120);
  int rightD = readDistance(3);            // что справа (сейчас смотрим на 60° вправо)
  if (leftD > rightD) turnDegrees(-120);   // слева свободнее — поворачиваемся туда
  if (max(leftD, rightD) < STOP_DIST_CM * 2) turnDegrees(180);   // тупик — разворот
}

// ================== Wi-Fi, setup, loop ==================

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Подключение к Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Wi-Fi подключён, IP: " + WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  analogWriteRange(PWM_MAX);
  analogWriteFreq(1000);
  stopMotors(false);
  digitalWrite(TRIG, LOW);connectWiFi();
  client.setInsecure();               // без проверки сертификата Telegram
  bot.getUpdates(-1);                 // забыть команды, присланные пока робот был выключен
  bot.setMyCommands(COMMANDS_MENU);   // меню команд в Telegram
  Serial.println("Бот запущен");
}

void loop() {
  // Нет Wi-Fi — автопилот выключаем (иначе его не остановить) и ждём переподключения
  if (WiFi.status() != WL_CONNECTED) {
    if (autoMode) {
      autoMode = false;
      stopMotors();
      wifiLostDuringAuto = true;
    }
    delay(500);
    return;
  }
  if (wifiLostDuringAuto) {
    wifiLostDuringAuto = false;
    reply(autoChat, "📶 Пропадал Wi-Fi — автопилот остановлен для безопасности.");
  }

  // В автопилоте Telegram опрашивается между отрезками, иначе — раз в секунду
  if (autoMode || millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();
    int n = bot.getUpdates(bot.last_message_received + 1);
    while (n > 0) {
      handleNewMessages(n);
      n = bot.getUpdates(bot.last_message_received + 1);
    }
  }

  if (autoMode) autoStep();
  else delay(10);
}
