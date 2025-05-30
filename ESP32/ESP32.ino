#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>

// ==== Wi-Fi Settings ====
const char* ssid = "zakatov";
const char* password = "zaqxsw228";

// ==== RS485 Settings ====
#define RS485_RX 16
#define RS485_TX 17
#define RS485_CONTROL 13
SoftwareSerial RS485(RS485_RX, RS485_TX);

// ==== SD Card Configuration ====
#define SD_CS 5
WebServer server(80);

// ==== Time Setup ====
// Для getLocalTime() переконайтеся, що NTP клієнт налаштований (ESP32 має вбудовану підтримку SNTP)
#include "time.h"
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; // Змінити для вашого регіону, наприклад, +1 година
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

// ==== Time synchronization function ====
void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
    Serial.println("Time synchronized");
  } else {
    Serial.println("Failed to obtain time");
  }
}

// ==== Function to get current timestamp ====
String getCurrentTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
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

// ==== Function to update index.json ====
void updateIndexJSON(String newFileName) {
  String indexFilePath = "/System/DataFromMicrocontrollers/index.json";
  File indexFile = SD.open(indexFilePath, FILE_READ);
  String indexData;
  if (indexFile) {
    while (indexFile.available()) {
      indexData += (char)indexFile.read();
    }
    indexFile.close();
  } else {
    // Якщо index.json не існує, створимо новий.
    indexData = "{\n  \"files\": [\n  ]\n}";
  }
  
  int filesPos = indexData.indexOf("\"files\": [");
  if (filesPos != -1) {
    int insertPos = indexData.indexOf("]", filesPos);
    // Якщо список не порожній, додаємо кому.
    String addition = "";
    if (indexData.charAt(insertPos - 1) != '[') {
      addition = ",\n    \"" + newFileName + "\"";
    } else {
      addition = "\n    \"" + newFileName + "\"";
    }
    indexData = indexData.substring(0, insertPos) + addition + "\n  " + indexData.substring(insertPos);
  } else {
    Serial.println("Error reading index.JSON structure.");
    return;
  }
  
  indexFile = SD.open(indexFilePath, FILE_WRITE);
  if (indexFile) {
    indexFile.print(indexData);
    indexFile.close();
    Serial.println("index.JSON updated with: " + newFileName);
  } else {
    Serial.println("Error updating index.JSON.");
  }
}

// ==== Function to save received RS485 data to SD card as JSON ====
void saveToSDCard(String data) {
  // Очікуємо, що data має формат: 
  // "ID: ROOM_1 / Temperature: 10.8 / Humidity: 48.5 / CH₄: 99.35 / CO: 119.79 / Movement: NO"
  
  // Для простоти будемо розбирати дані за допомогою substring() та indexOf().
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
  
  // Формуємо ім'я файлу
  String fileName = "/System/DataFromMicrocontrollers/" + deviceID + "__" + timestamp + ".json";
  
  File dataFile = SD.open(fileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println("{");
    dataFile.println("  \"deviceID\": \"" + deviceID + "\",");
    dataFile.println("  \"timestamp\": \"" + timestamp + "\",");
    dataFile.println("  \"temperature\": " + temperature + ",");
    dataFile.println("  \"humidity\": " + humidity + ",");
    dataFile.println("  \"motion\": " + (movement == "YES" ? "true" : "false") + ",");
    dataFile.println("  \"CO\": " + CO + ",");
    dataFile.println("  \"CH4\": " + CH4);
    dataFile.println("}");
    dataFile.close();
    Serial.println("File created: " + fileName);
    updateIndexJSON(fileName);
  } else {
    Serial.println("Error writing to SD card.");
  }
}

// ==== Function for RS485 data reception ====
void receiveRS485Data() {
  digitalWrite(RS485_CONTROL, LOW);
  if (RS485.available()) {
    String receivedData = RS485.readStringUntil('\n');
    Serial.println("Received data:");
    Serial.println(receivedData);
    saveToSDCard(receivedData);
  }
}

void setup() {
  Serial.begin(115200);
  RS485.begin(9600);
  
  pinMode(RS485_CONTROL, OUTPUT);
  digitalWrite(RS485_CONTROL, LOW);
  
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
  
  // Ініціалізація часу через NTP
  initTime();
  
  // Налаштування веб-сервера і статичних маршрутів
  server.serveStatic("/styles", SD, "/System/styles");
  server.serveStatic("/js", SD, "/System/js");
  server.serveStatic("/libs", SD, "/System/libs");
  server.serveStatic("/Login_and_Password", SD, "/System/Login_and_Password");
  server.serveStatic("/DataFromMicrocontrollers", SD, "/System/DataFromMicrocontrollers");
  
  server.on("/", HTTP_GET, []() {
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
  });
  
  server.on("/DataControllers.html", HTTP_GET, []() {
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
  });
  
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
  receiveRS485Data();
}
