#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <time.h>
#include <ArduinoJson.h>

// ===================== Налаштування пінів =====================
#define SD_CS    5    // Chip Select
#define SD_SCK   18   // Clock
#define SD_MOSI  23   // Master Out Slave In
#define SD_MISO  19   // Master In Slave Out

#define RS485_RX_PIN 16        // RX для RS485
#define RS485_TX_PIN 17        // TX для RS485
#define RS485_RE_DE_PIN 13     // Управління режимом RS485 (прийом/передача)

// ===================== Налаштування Wi-Fi =====================
const char* ssid     = "YOUR_SSID";       // Назва Wi-Fi мережі
const char* password = "YOUR_PASSWORD";   // Пароль Wi-Fi

// ===================== Створення HTTP серверу =====================
WebServer server(80);
HardwareSerial RS485(2);  // Використовуємо апаратний серіал для RS485

// Функція для отримання форматованої часової мітки (UTC)
String getFormattedTimestamp() {
  time_t now = time(nullptr);
  struct tm *timeinfo = gmtime(&now);
  char buffer[21];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
  return String(buffer);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Підключення до Wi-Fi
  Serial.print("Підключення до Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi підключено!");
  Serial.print("Локальна IP адреса ESP32: ");
  Serial.println(WiFi.localIP());

  // Синхронізація часу через NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Очікування синхронізації часу...");

  // Ініціалізація SD карти з використанням вказаних пінів
  Serial.println("Перевірка SD карти...");
  SPIClass spiSD(HSPI);
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("SD карта не підключена!");
    return;
  } else {
    Serial.println("SD карта успішно ініціалізована.");
    
    // Перевірка наявності папки /System
    if (!SD.exists("/System")) {
      Serial.println("Папку /System не знайдено!");
    } else {
      Serial.println("Папка /System знайдена.");
    }
    
    // Перевірка та створення папки /System/DataFromMicrocontrollers
    if (!SD.exists("/System/DataFromMicrocontrollers")) {
      Serial.println("Створення папки /System/DataFromMicrocontrollers...");
      SD.mkdir("/System/DataFromMicrocontrollers");
      Serial.println("Папка створена!");
    } else {
      Serial.println("Папка /System/DataFromMicrocontrollers вже існує.");
    }
  }

  // Налаштування RS485
  Serial.println("Конфігурація RS485...");
  RS485.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(RS485_RE_DE_PIN, OUTPUT);
  digitalWrite(RS485_RE_DE_PIN, LOW);
  Serial.println("RS485 готово для прийому даних.");

  // Перевірка файлу Authorization.html
  Serial.println("Перевірка файлу Authorization.html...");
  if (SD.exists("/System/Authorization.html")) {
    Serial.println("Файл Authorization.html знайдено.");
  } else {
    Serial.println("Файл Authorization.html ВІДСУТНІЙ!");
  }
  
  // Налаштування кореневого маршруту HTTP сервера
  server.on("/", HTTP_GET, []() {
    if (SD.exists("/System/Authorization.html")) {
      File file = SD.open("/System/Authorization.html", FILE_READ);
      String html = "";
      while (file.available()) {
        html += (char)file.read();
      }
      file.close();
      Serial.println("Сторінку Authorization.html завантажено.");
      server.send(200, "text/html", html);
    } else {
      server.send(404, "text/html", "<h1>Сторінку авторизації не знайдено!</h1>");
    }
  });
  
  server.begin();
  Serial.println("HTTP сервер запущено.");
}

void loop() {
  server.handleClient();

  // Прийом даних через RS485
  if (RS485.available()) {
    String receivedData = RS485.readStringUntil('\n');
    receivedData.trim();
    if (receivedData.length() > 0) {
      Serial.println("Дані отримано від Arduino:");
      Serial.println(receivedData);

      // Витягування ID пристрою
      int idx = receivedData.indexOf(" /");
      String deviceID = (idx != -1) ? receivedData.substring(0, idx) : "UNKNOWN";
      String sensorData = (idx != -1) ? receivedData.substring(idx + 3) : receivedData;

      // Отримання поточної часової мітки
      String timestamp = getFormattedTimestamp();
      String fileName = "/System/DataFromMicrocontrollers/" + deviceID + "_" + timestamp + ".json";

      // Формування JSON об'єкта з даними пристрою
      DynamicJsonDocument jsonDoc(256);
      jsonDoc["deviceID"] = deviceID;
      jsonDoc["timestamp"] = timestamp;

      // Парсинг даних сенсорів
      int tempIdx = sensorData.indexOf("Temperature: ");
      int humIdx = sensorData.indexOf(" / Humidity: ");
      int motionIdx = sensorData.indexOf(" / Motion: ");
      int coIdx = sensorData.indexOf(" / CO: ");
      int ch4Idx = sensorData.indexOf(" / CH₄: ");

      jsonDoc["temperature"] = sensorData.substring(tempIdx + 12, humIdx).toFloat();
      jsonDoc["humidity"] = sensorData.substring(humIdx + 12, motionIdx).toFloat();
      jsonDoc["motion"] = (sensorData.substring(motionIdx + 7, coIdx) == "Yes");
      jsonDoc["CO"] = sensorData.substring(coIdx + 6, ch4Idx).toFloat();
      jsonDoc["CH4"] = sensorData.substring(ch4Idx + 6).toFloat();

      // Запис даних у JSON файл
      File dataFile = SD.open(fileName, FILE_WRITE);
      if (dataFile) {
        serializeJson(jsonDoc, dataFile);
        dataFile.close();
        Serial.println("Дані збережено у файл:");
        Serial.println(fileName);
      } else {
        Serial.println("ПОМИЛКА запису у файл!");
      }

      // Оновлення index.json
      File indexFile = SD.open("/System/DataFromMicrocontrollers/index.json", FILE_READ);
      DynamicJsonDocument indexDoc(1024);
      if (indexFile) {
        deserializeJson(indexDoc, indexFile);
        indexFile.close();
      }
      indexDoc["files"].add(fileName.substring(24));

      indexFile = SD.open("/System/DataFromMicrocontrollers/index.json", FILE_WRITE);
      serializeJson(indexDoc, indexFile);
      indexFile.close();
      Serial.println("index.json оновлено!");
    }
  }
}
