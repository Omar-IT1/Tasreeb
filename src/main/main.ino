#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// 1. WiFi and Telegram
const char* ssid = "";      // Enter your WiFi network SSID
const char* pass = "";     // Enter your WiFi network password
const char* BOT_TOKEN = "";       // Enter your Telegram bot token from BotFather
const char* CHAT_ID = "";       // Enter your group/user CHAT_ID (include minus sign '-' if applicable)

// 2. ESP32 pins
const int LEAK_SENSOR = 27;  // Sensor digital output (DO)
const int BUZZER = 26;       // Active buzzer

// Change HIGH to LOW if your sensor outputs LOW when it detects water
const int WATER_DETECTED_LEVEL = HIGH;

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

bool alertActive = false;
bool startupMessageSent = false;
int lastReportedState = -1;  // -1 = no status sent yet
unsigned long lastReconnectAttempt = 0;

void setup() {
  Serial.begin(115200);

  pinMode(LEAK_SENSOR, INPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  client.setInsecure();  // Allows Telegram connection without a CA certificate

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.println("Connecting to WiFi...");
}

void loop() {
  // Detect water even when WiFi is unavailable
  alertActive = (digitalRead(LEAK_SENSOR) == WATER_DETECTED_LEVEL);

  // Local alarm
  if (alertActive) {
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    delay(200);
  } else {
    digitalWrite(BUZZER, LOW);
    delay(400);
  }

  // Try to reconnect without stopping the leak alarm
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnectAttempt >= 10000) {
      lastReconnectAttempt = millis();
      Serial.println("Reconnecting to WiFi...");
      WiFi.reconnect();
    }
    return;
  }

  // Send a startup message once
  if (!startupMessageSent) {
    if (bot.sendMessage(CHAT_ID,
                        "نظام كشف تسرب المياه من السقف جاهز للعمل.", "")) {
      startupMessageSent = true;
      Serial.println("Startup message sent.");
    }
  }

  // Send a message when the sensor state changes.
  // If sending fails, retry on the next loop.
  int currentState = alertActive ? 1 : 0;

  if (currentState != lastReportedState) {
    String message;

    if (alertActive) {
      message = "⚠️ تنبيه خطر: تم اكتشاف تسرب مياه من السقف!";
    } else if (lastReportedState == 1) {
      message = "تحديث: توقف اكتشاف المياه، الوضع عاد للطبيعي.";
    } else {
      // No need to announce a dry sensor on first startup
      lastReportedState = 0;
      return;
    }

    if (bot.sendMessage(CHAT_ID, message, "")) {
      lastReportedState = currentState;
      Serial.println("Status message sent.");
    } else {
      Serial.println("Telegram message failed; will retry.");
    }
  }
}