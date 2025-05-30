#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "time.h"

// ==== Wi-Fi Settings ====
const char* ssid = "zakatov";
const char* password = "zaqxsw228";

// ==== RS485 Settings ====
#define RS485_CONTROL 13

// ==== SD Card Configuration ====
#define SD_CS 5
WebServer server(80);

// ==== Time & NTP Settings ====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;

// ==== Ініціалізація та вивід часу ====
void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Synchronizing time...");
  delay(2000); 
  printLocalTime();
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.print("Current time: ");
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n", 
                timeinfo.tm_year + 1900, 
                timeinfo.tm_mon + 1, 
                timeinfo.tm_mday, 
                timeinfo.tm_hour, 
                timeinfo.tm_min, 
                timeinfo.tm_sec);
}

// ==== Get Current Timestamp ====
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

// ==== RS485 Helper Functions ====
void RS485_setTransmit() {
  digitalWrite(RS485_CONTROL, HIGH);
}

void RS485_setReceive() {
  digitalWrite(RS485_CONTROL, LOW);
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
      // Тут можна додати обробку/збереження на SD, якщо потрібно
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600);  
  pinMode(RS485_CONTROL, OUTPUT);
  RS485_setReceive();

  // Wi-Fi
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

  // Час
  initTime();   

  // SD-карта
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully.");

  // Перевірка наявності HTML-файлів
  if (SD.exists("/System/Authorization.html")) {
    Serial.println("File /System/Authorization.html found.");
  } else {
    Serial.println("File /System/Authorization.html not found.");
  }
  if (SD.exists("/System/DataControllers.html")) {
    Serial.println("File /System/DataControllers.html found.");
  } else {
    Serial.println("File /System/DataControllers.html not found.");
  }

  // Static resources
  server.serveStatic("/styles", SD, "/System/styles");
  server.serveStatic("/js", SD, "/System/js");
  server.serveStatic("/libs", SD, "/System/libs");
  server.serveStatic("/Login_and_Password", SD, "/System/Login_and_Password");
  server.serveStatic("/DataFromMicrocontrollers", SD, "/System/DataFromMicrocontrollers");

  // Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/DataControllers.html", HTTP_GET, handleDataControllers);

  // Start server
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
  receiveRS485Data();
}
