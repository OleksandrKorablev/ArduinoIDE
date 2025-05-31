// Додаємо слабке визначення lwip_hook_ip6_input для вирішення лінкувальної помилки,
// що виникає через IPv6. (Цей підхід вимикає обробку IPv6)
extern "C" void lwip_hook_ip6_input(void) { }

#include <HardwareSerial.h>
#include <CSE_ArduinoRS485.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "time.h"

// ===== RS485 Settings =====
#define RS485_DE_PIN 13   // Pin for controlling DE/RE signals

// ===== SD Card Settings =====
#define SD_CS 5           // Chip select pin for SD card

// ===== Data Folder & Index File =====
#define DATA_FOLDER "/System/DataFromMicrocontrollers"
#define INDEX_FILE  "/System/DataFromMicrocontrollers/index.JSON"

// ===== NTP Settings for Time Synchronization =====
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 0;

// ===== Time Initialization =====
void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Synchronizing time...");
  delay(2000);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  } else {
    Serial.printf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
}

// Returns timestamp for file naming in format "YYYY-MM-DD_HH_MM_SS"
String getCurrentTimestampForFilename() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { return "Unknown_Time"; }
  char buffer[25];
  sprintf(buffer, "%04d-%02d-%02d_%02d_%02d_%02d",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buffer);
}

// Returns timestamp for JSON data in format "YYYY MM DD HH:MM:SS"
String getCurrentTimestampForJSON() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { return "Unknown Time"; }
  char buffer[25];
  sprintf(buffer, "%04d %02d %02d %02d:%02d:%02d",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buffer);
}

// ===== Processing Incoming RS485 Data =====
// Expected input format:
// "ID: ROOM_1 / Temperature: 22.5 / Humidity: 45 / CH₄: 1.2 / CO: 0.03 / Movement: NO"
void processIncomingData(String data) {
  data.trim();
  if (data.length() == 0) return;

  Serial.println("Received data:");
  Serial.println(data);
  
  // Split the string into tokens by " / "
  const int expectedTokens = 6;
  String tokens[expectedTokens];
  int startIndex = 0;
  int tokenIndex = 0;
  
  while (tokenIndex < expectedTokens) {
    int delimIndex = data.indexOf(" / ", startIndex);
    if (delimIndex == -1) {
      tokens[tokenIndex] = data.substring(startIndex);
      break;
    } else {
      tokens[tokenIndex] = data.substring(startIndex, delimIndex);
      startIndex = delimIndex + 3;  // Skip past the delimiter " / "
      tokenIndex++;
    }
  }
  
  // Basic check: expect exactly 6 tokens
  if (tokenIndex < expectedTokens - 1) {
    Serial.println("Error: Incomplete data received.");
    return;
  }
  
  // Parse each token assuming the format "Key: Value"
  String deviceID    = tokens[0].substring(tokens[0].indexOf(": ") + 2);
  float temperature  = tokens[1].substring(tokens[1].indexOf(": ") + 2).toFloat();
  float humidity     = tokens[2].substring(tokens[2].indexOf(": ") + 2).toFloat();
  float ch4          = tokens[3].substring(tokens[3].indexOf(": ") + 2).toFloat();
  float co           = tokens[4].substring(tokens[4].indexOf(": ") + 2).toFloat();
  String movementStr = tokens[5].substring(tokens[5].indexOf(": ") + 2);
  bool motion        = (movementStr.indexOf("YES") != -1);
  
  // Create a JSON document with sensor data and the current timestamp
  // Використовуємо StaticJsonDocument для цього розділу:
  StaticJsonDocument<256> doc;
  doc["deviceID"]    = deviceID;
  doc["timestamp"]   = getCurrentTimestampForJSON();
  doc["temperature"] = temperature;
  doc["humidity"]    = humidity;
  doc["motion"]      = motion;
  doc["CO"]          = co;
  doc["CH4"]         = ch4;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Generate a filename in the format: "DEVICEID__YYYY-MM-DD_HH_MM_SS.json"
  String fileName = deviceID + "__" + getCurrentTimestampForFilename() + ".json";
  String filePath = String(DATA_FOLDER) + "/" + fileName;
  
  // Save JSON data to the file on the SD card
  File dataFile = SD.open(filePath.c_str(), FILE_WRITE);
  if (dataFile) {
    dataFile.println(jsonString);
    dataFile.close();
    Serial.print("Data saved to file: ");
    Serial.println(filePath);
  } else {
    Serial.print("Error opening file for writing: ");
    Serial.println(filePath);
  }
  
  // ===== Update the Index File =====
  String indexPath = INDEX_FILE;
  DynamicJsonDocument indexDoc(1024);
  
  if (SD.exists(indexPath.c_str())) {
    File indexFile = SD.open(indexPath.c_str(), FILE_READ);
    if (indexFile) {
      DeserializationError error = deserializeJson(indexDoc, indexFile);
      indexFile.close();
      if (error) {
        Serial.println("Failed to read index JSON, creating new index.");
        indexDoc.clear();
        // Використаємо createNestedArray() з ключем "files"
        indexDoc.createNestedArray("files");
      }
    }
  } else {
    // Create a new index document if the file does not exist
    indexDoc.createNestedArray("files");
  }
  
  JsonArray filesArray = indexDoc["files"].as<JsonArray>();
  filesArray.add(fileName);
  
  SD.remove(indexPath.c_str()); // Remove the old index file
  File newIndexFile = SD.open(indexPath.c_str(), FILE_WRITE);
  if (newIndexFile) {
    serializeJson(indexDoc, newIndexFile);
    newIndexFile.close();
    Serial.println("Index file updated.");
  } else {
    Serial.println("Error writing index file.");
  }
}

// ===== RS485 Reception Settings =====
// Using ESP32 hardware serial (e.g., Serial2) with CSE_ArduinoRS485
HardwareSerial RS485Serial(2);
// Create the RS485 object with the proper constructor
RS485Class rs485(RS485Serial, RS485_DE_PIN, -1, -1);

void setup() {
  Serial.begin(115200);
  
  // Initialize RS485 serial using given RX and TX pins (example: RX – pin 16, TX – pin 17)
  RS485Serial.begin(9600, SERIAL_8N1, 16, 17);
  // (Бібліотека отримала параметри через конструктор – додатковий виклик rs485.begin() не потрібен)
  
  // Synchronize time via NTP
  initTime();
  
  // Initialize SD card
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized successfully.");
  
  // Ensure the data folder exists on the SD card, otherwise create it
  if (!SD.exists(DATA_FOLDER)) {
    SD.mkdir(DATA_FOLDER);
    Serial.print("Created folder: ");
    Serial.println(DATA_FOLDER);
  }
}

void loop() {
  // Use the RS485 library's receive() function (without arguments)
  rs485.receive();
  
  // Check available data and read it as a String
  if (rs485.available() > 0) {
    String receivedData = "";
    while (rs485.available() > 0) {
      receivedData += (char)rs485.read();
    }
    receivedData.trim();
    if (receivedData.length() > 0) {
      processIncomingData(receivedData);
    }
  }
  
  delay(100);  // Small delay to avoid saturating the loop
}
