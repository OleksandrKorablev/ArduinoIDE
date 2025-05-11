#include <SoftwareSerial.h>
#include <DHT.h>

#define DHTPIN 10       // Connect DATA of DHT22 to D10
#define DHTTYPE DHT22   // Sensor type DHT22

#define PIRPIN 11       // Connect OUT of HC-SR501 to D11

#define MQ9Analog A0    // Analog output of MQ-9 to A0
#define MQ9Digital 12   // Digital output of MQ-9 to D12

#define RE_DE 13        // Control RE and DE on Max485

DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial RS485(2, 3); // RX, TX for Max485

const String DEVICE_ID = "ROOM_1"; // Unique device ID

// Function to determine CO level in ppm
float getCOppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  float CO_ppm = pow((voltage / 5.0), -1.5) * 100; // Calibration function for CO
  return CO_ppm;
}

// Function to determine CH₄ level in ppm
float getCH4ppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  float CH4_ppm = pow((voltage / 5.0), -1.8) * 80; // Calibration function for CH₄
  return CH4_ppm;
}

bool checkDHT22() {
  float testTemp = dht.readTemperature();
  float testHum = dht.readHumidity();
  return !isnan(testTemp) && !isnan(testHum);
}

bool checkHC_SR501() {
  int testPIR = digitalRead(PIRPIN);
  return (testPIR == HIGH || testPIR == LOW);
}

bool checkMQ9() {
  int testAnalog = analogRead(MQ9Analog);
  int testDigital = digitalRead(MQ9Digital);
  return (testAnalog >= 0 && testAnalog <= 1023) && (testDigital == HIGH || testDigital == LOW);
}

void setup() {
  Serial.begin(9600);
  RS485.begin(9600);
  dht.begin();
  pinMode(PIRPIN, INPUT);
  pinMode(MQ9Digital, INPUT);
  pinMode(RE_DE, OUTPUT);
  digitalWrite(RE_DE, LOW); // Default mode is receiving

  Serial.println("Checking sensor connections...");

  // Checking sensor connections
  bool dhtStatus = checkDHT22();
  bool pirStatus = checkHC_SR501();
  bool mq9Status = checkMQ9();

  if (!dhtStatus) {
    Serial.println("DHT22 sensor NOT connected or not responding!");
  } else {
    Serial.println("DHT22 sensor successfully connected.");
  }

  if (!pirStatus) {
    Serial.println("HC-SR501 sensor NOT connected or not responding!");
  } else {
    Serial.println("HC-SR501 sensor successfully connected.");
  }

  if (!mq9Status) {
    Serial.println("MQ-9 sensor NOT connected or not responding!");
  } else {
    Serial.println("MQ-9 sensor successfully connected.");
  }
}

void loop() {
  // Reading data from DHT22 sensor
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Reading data from HC-SR501 sensor
  bool motionDetected = digitalRead(PIRPIN);

  // Reading data from MQ-9 sensor
  int sensorValue = analogRead(MQ9Analog);
  float CO_ppm = getCOppm(sensorValue);
  float CH4_ppm = getCH4ppm(sensorValue);

  // Forming data string for transmission (adding ID)
  String data = DEVICE_ID + " / Temperature: " + temperature +
                " / Humidity: " + humidity +
                " / Motion detected: " + (motionDetected ? "Yes" : "No") +
                " / CO: " + CO_ppm + " ppm" +
                " / CH₄: " + CH4_ppm + " ppm";

  Serial.println("Current sensor readings:");
  Serial.println(data);

  // Transmitting data via RS485
  digitalWrite(RE_DE, HIGH);
  delay(1);  
  RS485.println(data);
  delay(1); 
  digitalWrite(RE_DE, LOW); // Returning to receive mode

  delay(1000); 
}
