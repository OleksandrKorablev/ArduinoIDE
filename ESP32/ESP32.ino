#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

// ======= Pin Configuration =======
#define SD_CS_PIN 2            // Chip Select for SD card
#define RS485_RX_PIN 16        // RS485 Receive pin (RO)
#define RS485_TX_PIN 17        // RS485 Transmit pin (DI)
#define RS485_RE_DE_PIN 13     // RS485 mode control pin (RE + DE together)

// ======= Wi-Fi Credentials =======
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// ======= RS485 Serial =======
HardwareSerial RS485(2);  // Using UART2

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ==== Connect to Wi-Fi ====
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
  }

  // ==== Initialize SD card ====
  Serial.println("Checking SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card initialization failed!");
  } else {
    Serial.println("SD card initialized successfully.");
  }

  // ==== Setup RS485 for Receiving ====
  RS485.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(RS485_RE_DE_PIN, OUTPUT);
  digitalWrite(RS485_RE_DE_PIN, LOW); // LOW to enable receive mode
  Serial.println("RS485 is ready to receive data.");
}

void loop() {
  // Read and display RS485 data
  if (RS485.available()) {
    String incomingData = RS485.readStringUntil('\n');
    incomingData.trim();
    if (incomingData.length() > 0) {
      Serial.println("Received from Arduino via RS485:");
      Serial.println(incomingData);
    }
  }
}
