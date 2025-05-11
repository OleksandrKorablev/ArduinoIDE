#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

// ===================== Pin Configuration =====================
#define SD_CS_PIN 2  // CS pin for microSD
#define RS485_RX_PIN 16  // RX pin for RS485
#define RS485_TX_PIN 17  // TX pin for RS485
#define RS485_RE_DE_PIN 13 // RS485 mode control (Receive/Transmit)

// ========================== Wi-Fi Settings ============================
const char* ssid     = "YOUR_SSID";       // Replace with your Wi-Fi SSID
const char* password = "YOUR_PASSWORD";   // Replace with your Wi-Fi password

// ===================== RS485 Communication ===========================
HardwareSerial RS485(2); // Use UART2 for RS485

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
  Serial.print("Local IP Address: ");
  Serial.println(WiFi.localIP());

  // Initializing microSD card
  Serial.println("Checking microSD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card NOT detected!");
    return;
  } else {
    Serial.println("SD card successfully initialized.");
  }

  // Setting up RS485
  Serial.println("Configuring RS485...");
  RS485.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(RS485_RE_DE_PIN, OUTPUT);
  digitalWrite(RS485_RE_DE_PIN, LOW); // Set RS485 to receive mode
  Serial.println("RS485 is ready to receive data.");
}

void loop() {
  // Receiving data from RS485 (Arduino Pro Mini 328)
  if (RS485.available()) {
    String receivedData = RS485.readStringUntil('\n'); // Read incoming data
    receivedData.trim(); // Remove extra spaces/new lines

    if (receivedData.length() > 0) {
      Serial.println("Received data from Arduino:");
      Serial.println(receivedData); // Print the received data
    }
  }

  delay(1000); // Wait before next loop iteration
}
