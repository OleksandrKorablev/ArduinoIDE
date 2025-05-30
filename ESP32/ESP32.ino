#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <time.h>
#include <ArduinoJson.h>

// ======= Pin Configuration =======
#define SD_CS_PIN 2            // Chip Select for SD card
#define RS485_RX_PIN 16        // RS485 Receive pin (RO)
#define RS485_TX_PIN 17        // RS485 Transmit pin (DI)
#define RS485_RE_DE_PIN 13     // RS485 mode control pin (RE + DE together)

// ======= Wi-Fi Credentials =======
const char* ssid = "zakatov";       
const char* password = "zaqxsw228";  

// ======= HTTP Server =======
WebServer server(80);

// ======= Function to Format Timestamp =======
String getFormattedTimestamp() {
  time_t now = time(nullptr);
  struct tm *timeinfo = gmtime(&now);
  char buffer[21];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
  return String(buffer);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // ======= Connecting to Wi-Fi =======
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
  }
  
  // ======= Synchronizing Time via NTP =======
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Synchronizing time...");
  
  // ======= Initializing SD Card =======
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully.");
  if (!SD.exists("/System/DataFromMicrocontrollers")) {
    Serial.println("Creating folder /System/DataFromMicrocontrollers...");
    SD.mkdir("/System/DataFromMicrocontrollers");
    Serial.println("Folder created.");
  }
  
  // ======= Setting Up RS485 Communication Using Hardware Serial (Serial2) =======
  Serial.println("Configuring RS485 on Serial2...");
  Serial2.begin(115200, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(RS485_RE_DE_PIN, OUTPUT);
  digitalWrite(RS485_RE_DE_PIN, LOW); // LOW: receive mode
  Serial.println("RS485 is ready to receive data.");
  
  // ======= Checking for Authorization.html on SD Card =======
  Serial.println("Checking for Authorization.html file...");
  if (SD.exists("/System/Authorization.html")) {
    Serial.println("Authorization.html file found.");
  } else {
    Serial.println("Authorization.html file missing!");
  }
  
  // ======= Setting Up HTTP Server =======
  server.on("/", HTTP_GET, []() {
    if (SD.exists("/System/Authorization.html")) {
      File file = SD.open("/System/Authorization.html", FILE_READ);
      String html = "";
      while (file.available()) {
        html += (char)file.read();
      }
      file.close();
      server.send(200, "text/html", html);
    } else {
      server.send(404, "text/html", "<h1>Authorization page not found!</h1>");
    }
  });
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
  
  // ======= Receiving Data via RS485 Using Serial2 =======
  if (Serial2.available()) {
    String incomingData = Serial2.readStringUntil('\n');
    incomingData.trim();
    if (incomingData.length() > 0) {
      Serial.println("Received from Arduino via RS485:");
      Serial.println(incomingData);
      
      // ======= Parsing Data =======
      int idx = incomingData.indexOf(" /");
      String deviceID = (idx != -1) ? incomingData.substring(0, idx) : "UNKNOWN";
      String sensorData = (idx != -1) ? incomingData.substring(idx + 3) : incomingData;
      
      // ======= Creating File Name with Timestamp =======
      String timestamp = getFormattedTimestamp();
      String fileName = "/System/DataFromMicrocontrollers/" + deviceID + "_" + timestamp + ".json";
      
      // ======= Creating JSON Document for Sensor Data =======
      DynamicJsonDocument jsonDoc(256);
      jsonDoc["deviceID"] = deviceID;
      jsonDoc["timestamp"] = timestamp;
      
      // Parse sensor data (assuming a predefined format)
      int tempIdx = sensorData.indexOf("Temperature: ");
      int humIdx = sensorData.indexOf(" / Humidity: ");
      int motionIdx = sensorData.indexOf(" / Motion: ");
      int coIdx = sensorData.indexOf(" / CO: ");
      int ch4Idx = sensorData.indexOf(" / CH₄: ");
      if (tempIdx != -1 && humIdx != -1 && motionIdx != -1 && coIdx != -1 && ch4Idx != -1) {
        jsonDoc["temperature"] = sensorData.substring(tempIdx + 12, humIdx).toFloat();
        jsonDoc["humidity"] = sensorData.substring(humIdx + 12, motionIdx).toFloat();
        jsonDoc["motion"] = (sensorData.substring(motionIdx + 7, coIdx) == "Yes");
        jsonDoc["CO"] = sensorData.substring(coIdx + 6, ch4Idx).toFloat();
        jsonDoc["CH4"] = sensorData.substring(ch4Idx + 6).toFloat();
      }
      
      // ======= Writing JSON Data to SD Card =======
      File dataFile = SD.open(fileName, FILE_WRITE);
      if (dataFile) {
        serializeJson(jsonDoc, dataFile);
        dataFile.close();
        Serial.println("Data saved to file:");
        Serial.println(fileName);
      } else {
        Serial.println("ERROR writing to file!");
      }
      
      // ======= Updating index.json =======
      File indexFile = SD.open("/System/DataFromMicrocontrollers/index.json", FILE_READ);
      DynamicJsonDocument indexDoc(1024);
      if (indexFile) {
        deserializeJson(indexDoc, indexFile);
        indexFile.close();
      }
      indexDoc["files"].add(fileName.substring(String("/System/DataFromMicrocontrollers/").length()));
      
      indexFile = SD.open("/System/DataFromMicrocontrollers/index.json", FILE_WRITE);
      serializeJson(indexDoc, indexFile);
      indexFile.close();
      Serial.println("index.json updated!");
    }
  }
}
