#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <time.h>
#include <ArduinoJson.h>

// ===================== Pin Configuration =====================
#define SD_CS_PIN 2     // CS pin for SD card
#define RS485_RX_PIN 16 // RX pin for RS485
#define RS485_TX_PIN 17 // TX pin for RS485
#define RS485_RE_DE_PIN 13 // RS485 mode control (Receive/Transmit)

// ========================== Wi-Fi Settings ============================
const char* ssid     = "YOUR_SSID";       // Wi-Fi network name
const char* password = "YOUR_PASSWORD";   // Wi-Fi password

// ===================== Creating HTTP Server ===========================
WebServer server(80);
HardwareSerial RS485(2); // Using hardware serial for RS485

// =================== Formatting Time (UTC) =======================
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

  // Connecting to Wi-Fi
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("Local IP ESP32: ");
  Serial.println(WiFi.localIP());

  // Synchronizing time via NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for time synchronization...");

  // Initializing SD Card
  Serial.println("Checking SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card NOT connected!");
    return;
  } else {
    Serial.println("SD card successfully initialized.");

    // Checking for /System folder
    if (!SD.exists("/System")) {
      Serial.println("Folder /System NOT found!");
    } else {
      Serial.println("Folder /System found.");
    }

    // Checking and creating /System/DataFromMicrocontrollers folder
    if (!SD.exists("/System/DataFromMicrocontrollers")) {
      Serial.println("Creating folder /System/DataFromMicrocontrollers...");
      SD.mkdir("/System/DataFromMicrocontrollers");
      Serial.println("Folder created!");
    } else {
      Serial.println("Folder /System/DataFromMicrocontrollers already exists.");
    }
  }

  // Setting up RS485
  Serial.println("Configuring RS485...");
  RS485.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(RS485_RE_DE_PIN, OUTPUT);
  digitalWrite(RS485_RE_DE_PIN, LOW);
  Serial.println("RS485 is ready for data reception.");

  // Checking for Authorization.html file
  Serial.println("Checking for Authorization.html file...");
  if (SD.exists("/System/Authorization.html")) {
    Serial.println("Authorization.html file found.");
  } else {
    Serial.println("Authorization.html file MISSING!");
  }
  
  server.on("/", HTTP_GET, []() {
    if (SD.exists("/System/Authorization.html")) {
      File file = SD.open("/System/Authorization.html", FILE_READ);
      String html = "";
      while (file.available()) {
        html += (char)file.read();
      }
      file.close();
      Serial.println("Authorization.html page loaded.");
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

  // Receiving data via RS485
  if (RS485.available()) {
    String receivedData = RS485.readStringUntil('\n');
    receivedData.trim();
    if (receivedData.length() > 0) {
      Serial.println("Data received from Arduino:");
      Serial.println(receivedData);

      // Extracting device ID
      int idx = receivedData.indexOf(" /");
      String deviceID = (idx != -1) ? receivedData.substring(0, idx) : "UNKNOWN";
      String sensorData = (idx != -1) ? receivedData.substring(idx + 3) : receivedData;

      // Getting current timestamp
      String timestamp = getFormattedTimestamp(); 
      String fileName = "/System/DataFromMicrocontrollers/" + deviceID + "_" + timestamp + ".json";

      // Creating JSON structure
      DynamicJsonDocument jsonDoc(256);
      jsonDoc["deviceID"] = deviceID;
      jsonDoc["timestamp"] = timestamp;

      // Parsing sensor data
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

      // Writing data to JSON file
      File dataFile = SD.open(fileName, FILE_WRITE);
      if (dataFile) {
        serializeJson(jsonDoc, dataFile);
        dataFile.close();
        Serial.println("Data saved to file:");
        Serial.println(fileName);
      } else {
        Serial.println("ERROR writing to file!");
      }

      // Updating index.json
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
      Serial.println("index.json updated!");
    }
  }
}
