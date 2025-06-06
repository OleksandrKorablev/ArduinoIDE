#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <ModbusRTUMaster.h>
#include <time.h>
#include <ArduinoJson.h>

// ===================== Wi-Fi Credentials =====================
const char* ssid = "zakatov";
const char* password = "zaqxsw228";

// ===================== SD Card Settings & Web Server =====================
#define SD_CS 5
WebServer server(80);

// ===================== RS485 and Modbus Settings =====================
#define RS485_RE_DE_PIN 13
#define MODBUS_BAUD 9600
#define MODBUS_CONFIG SERIAL_8N1
#define SLAVE_ID 1

ModbusRTUMaster modbus(Serial2, RS485_RE_DE_PIN);

// ===================== File Paths =====================
const char* DATA_FOLDER = "/System/DataFromMicrocontrollers";
const char* INDEX_FILE  = "/System/DataFromMicrocontrollers/index.JSON";

// ===================== NTP Time Settings =====================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;
const int daylightOffset_sec = 3600;

// ===================== Global Variables for Modbus Data =====================
const uint8_t numberOfRegisters = 6;
uint16_t sensorData[numberOfRegisters] = {0};

// ===================== Function: Get Formatted Current Time =====================
String getCurrentFormattedTime() {
  time_t now;
  struct tm timeInfo;
  int retry = 0;
  const int maxRetries = 10;
  while ((now = time(nullptr)) < 1000000000 && retry < maxRetries) {
    delay(500);
    retry++;
  }
  localtime_r(&now, &timeInfo);
  char timeString[25];
  sprintf(timeString, "%04d-%02d-%02d_%02d_%02d_%02d",
          timeInfo.tm_year + 1900,
          timeInfo.tm_mon + 1,
          timeInfo.tm_mday,
          timeInfo.tm_hour,
          timeInfo.tm_min,
          timeInfo.tm_sec);
  return String(timeString);
}

// ===================== Function: Update Index File =====================
void updateIndexFile(const String &newFileName) {
  String indexContent = "";
  if (SD.exists(INDEX_FILE)) {
    File indexFile = SD.open(INDEX_FILE, FILE_READ);
    if (indexFile) {
      while (indexFile.available()) {
        indexContent += (char)indexFile.read();
      }
      indexFile.close();
    }
  }
  if (indexContent == "" || indexContent.indexOf("files") < 0) {
    indexContent = "{\"files\": []}";
  }
  int pos = indexContent.lastIndexOf("]");
  if (pos < 0) {
    indexContent = "{\"files\": []}";
    pos = indexContent.lastIndexOf("]");
  }
  bool emptyArray = (indexContent.substring(indexContent.indexOf("[") + 1, pos)).length() == 0;
  String insertText = "\"" + newFileName + "\"";
  if (!emptyArray) {
    insertText = ", " + insertText;
  }
  indexContent = indexContent.substring(0, pos) + insertText + indexContent.substring(pos);
  File outFile = SD.open(INDEX_FILE, FILE_WRITE);
  if (outFile) {
    outFile.print(indexContent);
    outFile.close();
    Serial.println("Index file updated with: " + newFileName);
  } else {
    Serial.println("Error opening index file for writing!");
  }
}

// ===================== Setup for Wi-Fi, SD & Web Server =====================
void setupWebAndStorage() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");
  Serial.print("ESP32 Local IP: ");
  Serial.println(WiFi.localIP());
  
  SPI.begin(18, 19, 23, 5);
  delay(100);
  
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully.");
  
  if (SD.exists("/System/Authorization.html"))
    Serial.println("File /System/Authorization.html found.");
  else
    Serial.println("File /System/Authorization.html not found!");
  
  if (SD.exists("/System/DataControllers.html"))
    Serial.println("File /System/DataControllers.html found.");
  else
    Serial.println("File /System/DataControllers.html not found!");
  
  server.serveStatic("/styles", SD, "/System/styles");
  server.serveStatic("/js", SD, "/System/js");
  server.serveStatic("/libs", SD, "/System/libs");
  server.serveStatic("/Login_and_Password", SD, "/System/Login_and_Password");
  server.serveStatic("/DataFromMicrocontrollers", SD, DATA_FOLDER);
  
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

// ===================== Setup for NTP Time Synchronization =====================
void setupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for time synchronization...");
  String currentTime = getCurrentFormattedTime();
  Serial.println("Current time: " + currentTime);
}

// ===================== Setup for RS485 and Modbus =====================
void setupModbus() {
  Serial2.begin(MODBUS_BAUD, MODBUS_CONFIG, 16, 17);
  modbus.begin(MODBUS_BAUD, MODBUS_CONFIG);
  Serial.println("Modbus RTU master initialized on RS485.");
}

// ===================== Main Setup =====================
void setup() {
  Serial.begin(115200);
  delay(100);
  setupWebAndStorage();
  setupTime();
  setupModbus();
}

// ===================== Main Loop =====================
void loop() {
  server.handleClient();
  
  uint8_t error = modbus.readHoldingRegisters(SLAVE_ID, 0, sensorData, numberOfRegisters);
  
  if (error == 0) {
    Serial.println("Successfully read data from Holding Registers");
    float temperature = sensorData[0] / 1000.0;
    float humidity    = sensorData[1] / 10.0;
    float CO          = sensorData[2] / 10000.0;
    float CH4         = sensorData[3] / 10000.0;
    uint16_t motion   = sensorData[4];
    uint16_t devID    = sensorData[5];
    String device = "NODE_" + String(devID);
    
    // ===================== Create JSON Data =====================
    StaticJsonDocument<256> doc;
    doc["deviceID"] = device;
    doc["timestamp"] = getCurrentFormattedTime();
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["motion"] = (motion > 0) ? true : false;
    doc["CO"] = CO;
    doc["CH4"] = CH4;
    
    String jsonData;
    serializeJson(doc, jsonData);
    
    // ===================== Save Data to SD Card =====================
    String currentTime = getCurrentFormattedTime();
    String fileName = String(DATA_FOLDER) + "/ROOM_" + String(deviceID) + "__" + currentTime + ".json";
    
    File dataFile = SD.open(fileName.c_str(), FILE_WRITE);
    if (dataFile) {
      dataFile.println(jsonData);
      dataFile.close();
      Serial.println("Data saved to file: " + fileName);
      int lastSlash = fileName.lastIndexOf("/");
      String shortName = fileName.substring(lastSlash + 1);
      updateIndexFile(shortName);
    } else {
      Serial.println("Error opening file for data write!");
    }
  } else {
    Serial.print("Error reading Modbus data, code: ");
    Serial.println(error);
  }
  
  delay(2000);
}
