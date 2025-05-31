#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "time.h"
#include <ArduinoModbus.h>

// ==== Wi-Fi Settings ====
const char* ssid = "zakatov";
const char* password = "zaqxsw228";

// ==== RS485 & SD Settings ====
#define RS485_CONTROL 13         // Пін для керування RS485 (при потребі)
#define SD_CS 5                  // CS для SD-карти

// ==== NTP / Часові налаштування ====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;        // GMT+1, наприклад
const int daylightOffset_sec = 0; 

// ==== Ініціалізація часу через NTP ====
void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Synchronizing time...");
  delay(2000); // невелика затримка для синхронізації
}

String getCurrentTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Unknown_Time";
  }
  char buffer[25];
  // Формат: YYYY-MM-DD_HH-MM-SS
  sprintf(buffer, "%04d-%02d-%02d_%02d-%02d-%02d",
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);
  return String(buffer);
}

// ==== Функція запису JSON-файлу на SD-карту ====
bool writeDataToSD(String folder, String filename, String jsonContent) {
  String fullPath = folder + "/" + filename;
  File file = SD.open(fullPath, FILE_WRITE);
  if (!file) {
    Serial.print("Failed to open file for writing: ");
    Serial.println(fullPath);
    return false;
  }
  file.print(jsonContent);
  file.close();
  Serial.print("Data written to ");
  Serial.println(fullPath);
  return true;
}

// ==== Функція оновлення index.JSON файлу ====
bool updateIndexFile(String folder, String newFilename) {
  String indexPath = folder + "/index.JSON";
  DynamicJsonDocument doc(2048);
  File file;
  bool exists = SD.exists(indexPath);
  
  if (exists) {
    file = SD.open(indexPath, FILE_READ);
    if (!file) {
      Serial.println("Failed to open index.JSON for reading.");
      return false;
    }
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
      Serial.println("Failed to parse index.JSON, creating new document.");
      doc.clear();
    }
  }
  
  // Якщо ключ "files" відсутній або не є масивом – створюємо масив
  if (!doc.containsKey("files") || !doc["files"].is<JsonArray>()) {
    doc["files"] = JsonArray();
  }
  JsonArray files = doc["files"];
  files.add(newFilename);
  
  // Записуємо оновлений index.JSON
  file = SD.open(indexPath, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open index.JSON for writing.");
    return false;
  }
  serializeJsonPretty(doc, file);
  file.close();
  Serial.println("Index file updated.");
  return true;
}

#define NUM_REGS 6  // Маємо 6 holding-регістрів

// Таймер для періодичного зчитування даних (наприклад, кожні 5000 мс)
unsigned long previousMillis = 0;
unsigned long interval = 5000; 

void setup() {
  Serial.begin(115200);
  
  // Налаштування RS485: використаємо Serial2 для Modbus RTU (RS485)
  Serial2.begin(9600, SERIAL_8N1);
  pinMode(RS485_CONTROL, OUTPUT);
  digitalWrite(RS485_CONTROL, LOW); // Встановлюємо режим прийому
  
  // ==== Підключення до Wi‑Fi ====
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Ініціалізація SD картки
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized successfully.");
  
  // Ініціалізація часу через NTP
  initTime();
  
  // ==== Ініціалізація Modbus RTU слейва ====
  // ESP32 налаштована як модбас‑слейв з ID = 1, приймає дані через Serial2
  if (!ModbusRTUServer.begin(1, Serial2)) {
    Serial.println("Failed to start Modbus RTU Slave!");
    while (1);
  }
  // Реєструємо holding-регістри, що відповідають даним від Arduino Pro Mini
  for (int i = 0; i < NUM_REGS; i++) {
    ModbusRTUServer.addHreg(i, 0);
  }
  Serial.println("Modbus RTU Slave is running...");
}

void loop() {
  // Обробка запитів Modbus
  ModbusRTUServer.task();
  
  // Кожні interval мілісекунд читаємо holding-регістри та створюємо JSON
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    // Зчитуємо дані з holding-регістрів (адреса 0–5)
    uint16_t reg0 = ModbusRTUServer.hreg(0);
    uint16_t reg1 = ModbusRTUServer.hreg(1);
    uint16_t reg2 = ModbusRTUServer.hreg(2);
    uint16_t reg3 = ModbusRTUServer.hreg(3);
    uint16_t reg4 = ModbusRTUServer.hreg(4);
    uint16_t reg5 = ModbusRTUServer.hreg(5);
    
    // Перетворюємо отримані значення:
    // Температура і вологість були масштабовані на 10
    float temperature = reg0 / 10.0;
    float humidity    = reg1 / 10.0;
    float CO          = reg2;  // ppm
    float CH4         = reg3;  // ppm
    bool motion       = (reg4 == 1);
    int deviceIDNum   = reg5;  // наприклад, 101
    
    // Формуємо рядкове представлення ID, наприклад, "NODE_101"
    String deviceIDStr = "ROOM_" + String(deviceIDNum);
    
    // Отримуємо поточний часовий штамп
    String timestamp = getCurrentTimestamp();
    
    // Створюємо JSON-об’єкт із даними:
    DynamicJsonDocument jsonDoc(512);
    jsonDoc["deviceID"]    = deviceIDStr;
    jsonDoc["timestamp"]   = timestamp;
    jsonDoc["temperature"] = temperature;
    jsonDoc["humidity"]    = humidity;
    jsonDoc["motion"]      = motion;
    jsonDoc["CO"]          = CO;
    jsonDoc["CH4"]         = CH4;
    
    String jsonOutput;
    serializeJsonPretty(jsonDoc, jsonOutput);
    Serial.println("JSON data:");
    Serial.println(jsonOutput);
    
    // Формуємо ім'я файлу, наприклад: "NODE_101__2025-05-15_12-20-00.json"
    String filename = deviceIDStr + "__" + timestamp + ".json";
    
    // Встановлюємо папку для збереження файлів
    String folder = "/System/DataFromMicrocontrollers";
    if (!SD.exists(folder)) {
      SD.mkdir(folder);
    }
    
    // Записуємо JSON-дані у файл
    if (writeDataToSD(folder, filename, jsonOutput)) {
      // Оновлюємо файл-індекс
      updateIndexFile(folder, filename);
    }
  }
}
