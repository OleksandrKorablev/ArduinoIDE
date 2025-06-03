#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <ModbusMaster.h>
#include <time.h>
#include <ArduinoJson.h>

// ===================== Wi-Fi Settings =====================
const char* ssid = "zakatov";        
const char* password = "zaqxsw228";   

// ===================== SD Card Configuration =====================
#define SD_CS 5  // Pin for SD module Chip Select

// ===================== RS485 & Modbus Settings =====================
#define RS485_DE_RE 13  // Pin for controlling DE/RE (MAX485)
#define RS485_RX 16     // ESP32 pin connected to MAX485 RO
#define RS485_TX 17     // ESP32 pin connected to MAX485 DI
#define MODBUS_BAUD 9600
#define MODBUS_UNIT_ID 1  // ID of Arduino Pro Mini (slave)

// ===================== HTTP Server =====================
WebServer server(80);

// ===================== Modbus & RS485 =====================
ModbusMaster modbusMaster;
HardwareSerial RS485Serial(2); 

// Callback functions for RS485 transmission control
void preTransmission() {
  digitalWrite(RS485_DE_RE, HIGH);
}
void postTransmission() {
  digitalWrite(RS485_DE_RE, LOW);
}

// Function to obtain current time as a formatted string ("YYYY-MM-DD_HH_MM_SS")
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "0000-00-00_00_00_00";
  }
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H_%M_%S", &timeinfo);
  return String(buffer);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // ===================== Wi-Fi =====================
  Serial.print("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("ESP32 Local IP Address: ");
  Serial.println(WiFi.localIP());

  // ===================== Obtain Time via NTP =====================
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.print("Current time: ");
    Serial.println(getFormattedTime());
  } else {
    Serial.println("Unable to get current time");
  }

  // ===================== SD Card =====================
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully.");

  // Check for essential files
  if (SD.exists("/System/Authorization.html"))
    Serial.println("File /System/Authorization.html found.");
  else
    Serial.println("File /System/Authorization.html not found.");
  if (SD.exists("/System/DataControllers.html"))
    Serial.println("File /System/DataControllers.html found.");
  else
    Serial.println("File /System/DataControllers.html not found.");

  // ===================== HTTP Server =====================
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
        server.send(500, "text/html", "<h1>Error opening Authorization page!</h1>");
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

  // ===================== RS485 and Modbus =====================
  pinMode(RS485_DE_RE, OUTPUT);
  digitalWrite(RS485_DE_RE, LOW); // Default to receive mode
  RS485Serial.begin(MODBUS_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);

  modbusMaster.begin(MODBUS_UNIT_ID, RS485Serial);
  modbusMaster.preTransmission(preTransmission);
  modbusMaster.postTransmission(postTransmission);

  // Test Modbus connection with Arduino Pro Mini
  Serial.println("Attempting Modbus connection with Arduino Pro Mini...");
  uint8_t result = modbusMaster.readHoldingRegisters(0, 6);
  if (result == modbusMaster.ku8MBSuccess)
    Serial.println("Successfully connected to Arduino Pro Mini.");
  else {
    Serial.print("Failed to connect to Arduino Pro Mini. Error code: ");
    Serial.println(result, DEC);
  }
}

void loop() {
  server.handleClient();  

  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  // Poll every 15 seconds (15000 ms)
  if (currentMillis - previousMillis >= 15000) {
    previousMillis = currentMillis;

    uint8_t result = modbusMaster.readHoldingRegisters(0, 6);
    if (result == modbusMaster.ku8MBSuccess) {
      Serial.println("Data successfully read from Holding Registers");

      // Read raw data from registers
      uint16_t rawTemperature = modbusMaster.getResponseBuffer(0);
      uint16_t rawHumidity    = modbusMaster.getResponseBuffer(1);
      uint16_t rawCO          = modbusMaster.getResponseBuffer(2);
      uint16_t rawCH4         = modbusMaster.getResponseBuffer(3);
      uint16_t rawMotion      = modbusMaster.getResponseBuffer(4);
      uint16_t deviceId       = modbusMaster.getResponseBuffer(5);

      // Normalize data (restore actual values)
      float temperature = rawTemperature / 10.0; 
      float humidity    = rawHumidity / 10.0;     
      float CO_ppm      = rawCO / 100.0;        
      float CH4_ppm     = rawCH4 / 100.0;       
      int motion = rawMotion; 

      Serial.print("Temperature: "); Serial.println(temperature);
      Serial.print("Humidity: "); Serial.println(humidity);
      Serial.print("CO (ppm): "); Serial.println(CO_ppm);
      Serial.print("CH4 (ppm): "); Serial.println(CH4_ppm);
      Serial.print("Motion: "); Serial.println(motion);
      Serial.print("Device ID: "); Serial.println(deviceId);

      // Get current time for filename
      String timestamp = getFormattedTime();

      // Create JSON object (using ArduinoJson library)
      StaticJsonDocument<256> doc;
      doc["deviceID"]    = deviceId;
      doc["timestamp"]   = timestamp;
      doc["temperature"] = temperature;
      doc["humidity"]    = humidity;
      doc["CO_ppm"]      = CO_ppm;
      doc["CH4_ppm"]     = CH4_ppm;
      doc["motion"]      = motion;

      String jsonString;
      serializeJson(doc, jsonString);

      // Form filename "NODE_<deviceID>__<timestamp>.json"
      String fileName = "/System/DataFromMicrocontrollers/NODE_" + String(deviceId) + "__" + timestamp + ".json";

      // Write JSON data to file on SD card
      File dataFile = SD.open(fileName, FILE_WRITE);
      if (dataFile) {
        dataFile.println(jsonString);
        dataFile.close();
        Serial.println("Data successfully saved to file " + fileName);
      } else {
        Serial.println("Failed to open file for writing: " + fileName);
      }

      // Update index.json which contains the list of created files
      String indexPath = "/System/DataFromMicrocontrollers/index.json";
      DynamicJsonDocument indexDoc(512);

      // If index.json exists, read its contents
      if (SD.exists(indexPath)) {
        File indexFile = SD.open(indexPath, FILE_READ);
        if (indexFile) {
          DeserializationError error = deserializeJson(indexDoc, indexFile);
          if (error) {
            Serial.print("Error deserializing index.json: ");
            Serial.println(error.f_str());
            indexDoc.clear();
            indexDoc["files"] = JsonArray();
          }
          indexFile.close();
        }
      } else {
        indexDoc["files"] = JsonArray();
      }

      // Add new filename to the array
      JsonArray files = indexDoc["files"].as<JsonArray>();
      int lastSlash = fileName.lastIndexOf('/');
      String shortFileName = fileName.substring(lastSlash + 1);  
      files.add(shortFileName);

      // Write the updated index.json to SD card
      File indexFile = SD.open(indexPath, FILE_WRITE);
      if (indexFile) {
        serializeJsonPretty(indexDoc, indexFile);
        indexFile.close();
        Serial.println("index.json updated.");
      } else {
        Serial.println("Failed to open index.json for writing.");
      }
    } else {
      Serial.print("Failed to read data from Holding Registers. Error code: ");
      Serial.println(result, DEC);
    }
  }
}
