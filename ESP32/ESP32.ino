#include <ModbusMaster.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <time.h>
#include <WiFi.h>  

// ===================== Wi-Fi Settings =====================
const char* ssid = "zakatov";         
const char* password = "zaqxsw228";    

// ===================== Modbus та SD =====================
#define MAX485_DE_RE 13
#define RXD2 16
#define TXD2 17
#define SD_CS 5

ModbusMaster node;

void preTransmission() {
  digitalWrite(MAX485_DE_RE, HIGH);
}

void postTransmission() {
  digitalWrite(MAX485_DE_RE, LOW);
}

String getCurrentTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", timeinfo);
  return String(buffer);
}

String getFormattedTimestampForJson() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y %m %d %H:%M:%S", timeinfo);
  return String(buffer);
}

void saveJsonToFile(String deviceID, float temperature, float humidity, bool motion, float CO, float CH4) {
  String fileTimestamp = getCurrentTimestamp();
  String jsonTimestamp = getFormattedTimestampForJson();

  String fileName = deviceID + "__" + fileTimestamp + ".json";
  String fullPath = "/System/DataFromMicrocontrollers/" + fileName;

  StaticJsonDocument<256> doc;
  doc["deviceID"] = deviceID;
  doc["timestamp"] = jsonTimestamp;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["motionDetected"] = motion;
  doc["CO"] = CO;
  doc["CH4"] = CH4;

  File file = SD.open(fullPath, FILE_WRITE);
  if (file) {
    serializeJsonPretty(doc, file);
    file.close();
    Serial.println("Saved JSON to: " + fullPath);
  } else {
    Serial.println("Failed to save JSON to: " + fullPath);
  }
}

void readModbusData() {
  uint8_t result = node.readHoldingRegisters(0x00, 6);
  if (result != node.ku8MBSuccess) {
    Serial.println("Modbus read error");
    return;
  }

  float temperature = node.getResponseBuffer(0) / 10.0;
  float humidity = node.getResponseBuffer(1) / 10.0;
  float CO = node.getResponseBuffer(2) / 100.0;
  float CH4 = node.getResponseBuffer(3) / 100.0;
  bool motion = node.getResponseBuffer(4) == 1;
  uint16_t deviceID_num = node.getResponseBuffer(5);
  String deviceID = "NODE_" + String(deviceID_num);

  Serial.printf("Received data from %s:\n", deviceID.c_str());
  Serial.printf("  Temperature: %.1f °C\n", temperature);
  Serial.printf("  Humidity: %.1f %%\n", humidity);
  Serial.printf("  CO: %.2f ppm\n", CO);
  Serial.printf("  CH4: %.2f ppm\n", CH4);
  Serial.printf("  Motion detected: %s\n", motion ? "YES" : "NO");
  Serial.println("-------------------------------");

  saveJsonToFile(deviceID, temperature, humidity, motion, CO, CH4);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Initializing ESP32 Modbus RTU Master...");

  // ===================== Wi-Fi підключення =====================
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

  // ===================== Modbus Init =====================
  pinMode(MAX485_DE_RE, OUTPUT);
  digitalWrite(MAX485_DE_RE, LOW);

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  node.begin(1, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // ===================== SD карта =====================
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized.");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for time sync...");
  delay(3000);

  if (!SD.exists("/System")) {
    SD.mkdir("/System");
  }
  if (!SD.exists("/System/DataFromMicrocontrollers")) {
    SD.mkdir("/System/DataFromMicrocontrollers");
  }
}

void loop() {
  readModbusData();
  delay(10000);
}
