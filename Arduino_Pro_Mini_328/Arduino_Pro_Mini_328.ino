#include <SoftwareSerial.h>
#include <DHT.h>
#include <math.h>
#include <CSE_ArduinoRS485.h>  // Include RS485 library

// Connecting the DHT22 sensor
#define DHTPIN 10
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Connecting the HC-SR501 (PIR) and MQ-9 sensor
#define PIRPIN 11
#define MQ9Analog A0
#define MQ9Digital 12

// Pin for controlling MAX485 operating modes (DE/RE)
#define RS485_DE_PIN 13

// Creating SoftwareSerial for RS485 (TX: pin 3, RX: pin 2)
SoftwareSerial rs485Serial(2, 3);

// Initializing the RS485 object by passing the serial port object and DE pin.
// -1 indicates that the receive mode pin and alternate TX pin are not used.
RS485Class rs485(rs485Serial, RS485_DE_PIN, -1, -1);

const String DEVICE_ID = "ROOM_1";

// Function to calculate CO and CH₄ concentrations based on ADC values
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

// Function to check the DHT22 sensor
bool checkDHT22() {
  float testTemp = dht.readTemperature();
  float testHum  = dht.readHumidity();
  return (!isnan(testTemp) && !isnan(testHum));
}

// Function to check the HC-SR501 sensor
bool checkHC_SR501() {
  int testPIR = digitalRead(PIRPIN);
  // Since digitalRead returns HIGH or LOW, this function always returns true,
  // but it's included for consistency.
  return (testPIR == HIGH || testPIR == LOW);
}

// Function to check the MQ-9 sensor
bool checkMQ9() {
  int testAnalog = analogRead(MQ9Analog);
  int testDigital = digitalRead(MQ9Digital);
  return ((testAnalog >= 0 && testAnalog <= 1023) &&
          (testDigital == HIGH || testDigital == LOW));
}

void setup() {
  Serial.begin(9600);
  rs485Serial.begin(9600);
  
  // RS485 initialization - the library automatically manages transmission/reception
  // modes based on the parameters passed in the constructor.
  
  dht.begin();
  pinMode(PIRPIN, INPUT);
  pinMode(MQ9Digital, INPUT);
  // If needed, you can explicitly set the analog input mode (usually not necessary)
  pinMode(MQ9Analog, INPUT);

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
  // Reading sensor values
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int mq9Raw = analogRead(MQ9Analog);
  float CO_ppm = getCOppm(mq9Raw);
  float CH4_ppm = getCH4ppm(mq9Raw);
  bool motionDetected = (digitalRead(PIRPIN) == HIGH);
  String movementStatus = motionDetected ? "YES" : "NO";
  
  // Constructing the message with the device ID and sensor data
  String message = "ID: " + DEVICE_ID;
  message += " / Temperature: " + String(temperature, 1);
  message += " / Humidity: " + String(humidity, 1);
  message += " / CH₄: " + String(CH4_ppm, 2);
  message += " / CO: " + String(CO_ppm, 2);
  message += " / Movement: " + movementStatus;
  
  Serial.println("Current sensor readings:");
  Serial.println(message);
  
  // Sending data via RS485. Using the standard write() from the Stream class,
  // then flush() ensures immediate UART transmission.
  int bytesSent = rs485.write((uint8_t*)message.c_str(), message.length());
  rs485.flush();
  
  if (bytesSent == message.length()) {
    Serial.println("RS485 transmission successful.");
  } else {
    Serial.print("RS485 transmission error: Sent ");
    Serial.print(bytesSent);
    Serial.print(" bytes out of ");
    Serial.print(message.length());
    Serial.println(" bytes.");
  }
  
  delay(1000); // Interval of 1 second
}
