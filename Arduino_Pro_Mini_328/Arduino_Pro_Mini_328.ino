#include <SoftwareSerial.h>
#include <DHT.h>
#include <math.h>

#define DHTPIN 10
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define PIRPIN 11
#define MQ9Analog A0
#define MQ9Digital 12

#define RE_DE 13
SoftwareSerial RS485(2, 3);

const String DEVICE_ID = "ROOM_1";

float getCOppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  float CO_ppm = pow((voltage / 5.0), -1.5) * 100;
  return CO_ppm;
}

float getCH4ppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  float CH4_ppm = pow((voltage / 5.0), -1.8) * 80;
  return CH4_ppm;
}

bool checkDHT22() {
  float testTemp = dht.readTemperature();
  float testHum = dht.readHumidity();
  return (!isnan(testTemp) && !isnan(testHum));
}

bool checkHC_SR501() {
  int testPIR = digitalRead(PIRPIN);
  return (testPIR == HIGH || testPIR == LOW);
}

bool checkMQ9() {
  int testAnalog = analogRead(MQ9Analog);
  int testDigital = digitalRead(MQ9Digital);
  return ((testAnalog >= 0 && testAnalog <= 1023) && (testDigital == HIGH || testDigital == LOW));
}

void RS485_setTransmit() {
  digitalWrite(RE_DE, HIGH);
}

void RS485_setReceive() {
  digitalWrite(RE_DE, LOW);
}

void setup() {
  Serial.begin(9600);
  RS485.begin(9600);
  dht.begin();
  pinMode(PIRPIN, INPUT);
  pinMode(MQ9Digital, INPUT);
  pinMode(MQ9Analog, INPUT);
  pinMode(RE_DE, OUTPUT);
  RS485_setReceive();
  Serial.println("Checking sensor connections...");
  bool dhtStatus = checkDHT22();
  bool pirStatus = checkHC_SR501();
  bool mq9Status = checkMQ9();
  if (!dhtStatus)
    Serial.println("DHT22 sensor NOT connected or not responding!");
  else
    Serial.println("DHT22 sensor successfully connected.");
  if (!pirStatus)
    Serial.println("HC-SR501 sensor NOT connected or not responding!");
  else
    Serial.println("HC-SR501 sensor successfully connected.");
  if (!mq9Status)
    Serial.println("MQ-9 sensor NOT connected or not responding!");
  else
    Serial.println("MQ-9 sensor successfully connected.");
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int mq9Raw = analogRead(MQ9Analog);
  float CO_ppm = getCOppm(mq9Raw);
  float CH4_ppm = getCH4ppm(mq9Raw);
  bool motionDetected = (digitalRead(PIRPIN) == HIGH);
  const char* movementStatus = motionDetected ? "YES" : "NO";
  String message = "ID: " + DEVICE_ID;
  message += " / Temperature: " + String(temperature, 1);
  message += " / Humidity: " + String(humidity, 1);
  message += " / CH₄: " + String(CH4_ppm, 2);
  message += " / CO: " + String(CO_ppm, 2);
  message += " / Movement: " + String(movementStatus);
  Serial.println("Current sensor readings:");
  Serial.println(message);
  RS485_setTransmit();
  delay(2);
  int bytesSent = RS485.write(message.c_str(), message.length());
  RS485.flush();
  if (bytesSent == message.length())
    Serial.println("RS485 Transmission successful.");
  else {
    Serial.print("RS485 Transmission error: Only ");
    Serial.print(bytesSent);
    Serial.print(" bytes sent out of ");
    Serial.print(message.length());
    Serial.println(" bytes.");
  }
  RS485_setReceive();
  delay(1000);
}
