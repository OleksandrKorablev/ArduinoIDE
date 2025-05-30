#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include "time.h"

// ==== Wi-Fi Settings ====
const char* ssid = "zakatov";
const char* password = "zaqxsw228";

// ==== RS485 Settings ====
#define RS485_CONTROL 13  
HardwareSerial Serial2(2);  

// ==== SD Card Configuration ====
#define SD_CS 5
WebServer server(80);

// ==== Time & NTP Settings ====
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;  
const int   daylightOffset_sec = 0;

// ==== Web Server Handlers ====
void handleRoot() {
  if (SD.exists("/System/Authorization.html")) {
    File authFile = SD.open("/System/Authorization.html", FILE_READ);
    if (authFile) {
      Serial.println("Streaming /System/Authorization.html...");
      server.streamFile(authFile, "text/html");
      authFile.close();
    } else {
      Serial.println("Error opening /System/Authorization.html!");
      server.send(500, "text/html", "<h1>Error opening Authorization.html!</h1>");
    }
  } else {
    server.send(404, "text/html", "<h1>Authorization page not found!</h1>");
  }
}

void handleDataControllers() {
  if (SD.exists("/System/DataControllers.html")) {
    File dcFile = SD.open("/System/DataControllers.html", FILE_READ);
    if (dcFile) {
      Serial.println("Streaming /System/DataControllers.html...");
      server.streamFile(dcFile, "text/html");
      dcFile.close();
    } else {
      Serial.println("Error opening /System/DataControllers.html!");
      server.send(500, "text/html", "<h1>Error opening DataControllers.html!</h1>");
    }
  } else {
    server.send(404, "text/html", "<h1>DataControllers page not found!</h1>");
  }
}

// ==== Time Synchronization ====
void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
    Serial.println("Time synchronized");
  } else {
    Serial.println("Failed to obtain time");
  }
}

// ==== Get current timestamp ====
String getCurrentTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Unknown_Time";
  }
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d_%02d-%02d-%02d",
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);
  return String(buffer);
}

// ==== RS485 Helper Functions ====
void RS485_setTransmit() {
  digitalWrite(RS485_CONTROL, HIGH);
}

void RS485_setReceive() {
  digitalWrite(RS485_CONTROL, LOW);
}

// ==== Update index.json using ArduinoJson ====
void updateIndexJSON(String newFileName) {
  String indexFilePath = "/System/DataFromMicrocontrollers/index.json";
  String jsonString = "";
  
  File indexFile = SD.open(indexFilePath, FILE_READ);
  if (indexFile) {
    while (indexFile.available()) {
      jsonString += (char)indexFile.read();
    }
    indexFile.close();
  }
  
  // Створимо документ для index.json
  StaticJsonDocument<1024> doc;
  
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    // Якщо не вдалося прочитати index.json, ініціалізуємо порожній документ
    doc["files"] = JsonArray();
  }
  
  JsonArray files = doc["files"].as<JsonArray>();
  
  // Прибрати шлях із newFileName для запису в індекс (залишаємо тільки ім'я файлу)
  int slashPos = newFileName.lastIndexOf('/');
  String simpleName = (slashPos != -1) ? newFileName.substring(slashPos + 1) : newFileName;
  
  files.add(simpleName);
  
  // Записуємо оновлений документ назад
  File outFile = SD.open(indexFilePath, FILE_WRITE);
  if (outFile) {
    serializeJsonPretty(doc, outFile);
    outFile.close();
    Serial.println("index.json updated with: " + simpleName);
  } else {
    Serial.println("Error updating index.json");
  }
}

// ==== Save received RS485 data to SD as JSON using ArduinoJson ====
void saveToSDCard(String data) {
  // Очікуваний формат даних:
  // "ID: ROOM_1 / Temperature: 10.8 / Humidity: 48.5 / CH₄: 99.35 / CO: 119.79 / Movement: NO"
  // Розберемо рядок вручну:
  String deviceID, temperature, humidity, CO, CH4, movement;
  int idStart = data.indexOf("ID: ") + 4;
  int tempStart = data.indexOf("/ Temperature: ") + 14;
  int humStart = data.indexOf("/ Humidity: ") + 12;
  int ch4Start = data.indexOf("/ CH₄: ") + 7;
  int coStart = data.indexOf("/ CO: ") + 6;
  int movStart = data.indexOf("/ Movement: ") + 11;
  
  deviceID = data.substring(idStart, tempStart - 14);
  temperature = data.substring(tempStart, humStart - 12);
  humidity = data.substring(humStart, ch4Start - 8);
  CH4 = data.substring(ch4Start, coStart - 7);
  CO = data.substring(coStart, movStart - 12);
  movement = data.substring(movStart);
  
  String timestamp = getCurrentTimestamp();
  
  // Формуємо ім’я файлу без шляху для запису в index.json
  String simpleFileName = deviceID + "__" + timestamp + ".json";
  // А повний шлях для запису у SD:
  String filePath = "/System/DataFromMicrocontrollers/" + simpleFileName;
  
  // Створюємо JSON документ для даних
  StaticJsonDocument<256> doc;
  doc["deviceID"] = deviceID;
  doc["timestamp"] = timestamp;
  doc["temperature"] = temperature.toFloat();
  doc["humidity"] = humidity.toFloat();
  // Якщо рядок з рухом дорівнює "YES" (регістр чутливий), тоді true, інакше false.
  doc["motion"] = (movement == "YES");
  doc["CO"] = CO.toFloat();
  doc["CH4"] = CH4.toFloat();
  
  // Запис у файл
  File dataFile = SD.open(filePath, FILE_WRITE);
  if (dataFile) {
    serializeJsonPretty(doc, dataFile);
    dataFile.close();
    Serial.println("File created: " + filePath);
    updateIndexJSON(filePath);
  } else {
    Serial.println("Error writing to SD card.");
  }
}

// ==== RS485 Data Reception ====
void receiveRS485Data() {  
  RS485_setReceive();
  if (Serial2.available()) {  
    String receivedData = Serial2.readStringUntil('\n');  
    receivedData.trim();
    if (receivedData.length() > 0) {
      Serial.println("Received data:");
      Serial.println(receivedData);
      saveToSDCard(receivedData);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600);  

  pinMode(RS485_CONTROL, OUTPUT);
  RS485_setReceive();
  
  Serial.print("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");
  Serial.print("ESP32 Local IP Address: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully.");
  
  initTime();
  
  // Налаштування статичних маршрутів для веб-сервера
  server.serveStatic("/styles", SD, "/System/styles");
  server.serveStatic("/js", SD, "/System/js");
  server.serveStatic("/libs", SD, "/System/libs");
  server.serveStatic("/Login_and_Password", SD, "/System/Login_and_Password");
  server.serveStatic("/DataFromMicrocontrollers", SD, "/System/DataFromMicrocontrollers");
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/DataControllers.html", HTTP_GET, handleDataControllers);
  
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
  receiveRS485Data();
}
