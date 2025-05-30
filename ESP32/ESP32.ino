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

void setup() {
  Serial.begin(115200);
  RS485.begin(9600);

  pinMode(RS485_CONTROL, OUTPUT);
  digitalWrite(RS485_CONTROL, LOW);  // Початково у режимі прийому

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

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
  receiveRS485Data();
}

// ==== Функція для прийому даних через RS485 ====
void receiveRS485Data() {
  digitalWrite(RS485_CONTROL, LOW);  // У режим прийому
  
  if (RS485.available()) {
    String receivedData = RS485.readStringUntil('\n');
    Serial.println("Received data:");
    Serial.println(receivedData);

    saveToSDCard(receivedData);
  }
}

// ==== Функція для збереження даних у SD-карту ====
void saveToSDCard(String data) {
  String deviceID, temperature, humidity, CO, CH4, movement;
  
  // Розбір рядка на окремі значення
  int idStart = data.indexOf("ID: ") + 4;
  int tempStart = data.indexOf("/ Temperature: ") + 14;
  int humStart = data.indexOf("/ Humidity: ") + 12;
  int ch4Start = data.indexOf("/ CH₄: ") + 7;
  int coStart = data.indexOf("/ CO: ") + 6;
  int movStart = data.indexOf("/ Movement: ") + 11;
  
  deviceID = data.substring(idStart, tempStart - 15);
  temperature = data.substring(tempStart, humStart - 13);
  humidity = data.substring(humStart, ch4Start - 8);
  CH4 = data.substring(ch4Start, coStart - 7);
  CO = data.substring(coStart, movStart - 12);
  movement = data.substring(movStart);

  // Отримання поточного часу
  String timestamp = getCurrentTimestamp();

  // Формування імені файлу
  String fileName = "/System/DataFromMicrocontrollers/" + deviceID + "__" + timestamp + ".json";

  // Запис у JSON
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

// ==== Функція для отримання поточного часу ====
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

// ==== Функція для оновлення index.JSON ====
void updateIndexJSON(String newFileName) {
  String indexFilePath = "/System/DataFromMicrocontrollers/index.json";
  File indexFile = SD.open(indexFilePath, FILE_READ);
  String indexData;

  if (indexFile) {
    while (indexFile.available()) {
      indexData += (char)indexFile.read();
    }
    indexFile.close();
  }
  
  int filesPos = indexData.indexOf("\"files\": [");
  if (filesPos != -1) {
    int insertPos = filesPos + 9;
    indexData = indexData.substring(0, insertPos) + "\n    \"" + newFileName + "\", " + indexData.substring(insertPos);
  } else {
    Serial.println("Error reading index.JSON structure.");
    return;
  }

  indexFile = SD.open(indexFilePath, FILE_WRITE);
  if (indexFile) {
    indexFile.println(indexData);
    indexFile.close();
    Serial.println("index.JSON updated with: " + newFileName);
  } else {
    Serial.println("Error updating index.JSON.");
  }
}
