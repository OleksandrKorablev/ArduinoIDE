#include <ArduinoModbus.h>
#include <DHT.h>
#include <math.h>

#define DHT_PIN         10
#define DHT_TYPE        DHT22

#define MQ9_ANALOG_PIN  A0
#define MQ9_DIGITAL_PIN 12

#define PIR_PIN         11

#define RS485_ENABLE_PIN 13

#define DEVICE_ID       101
#define NB_REG          6

DHT dht(DHT_PIN, DHT_TYPE);

float getCOppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  return pow((voltage / 5.0), -1.5) * 100;
}

float getCH4ppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  return pow((voltage / 5.0), -1.8) * 80;
}

void preTransmission() {
  digitalWrite(RS485_ENABLE_PIN, HIGH);
}

void postTransmission() {
  digitalWrite(RS485_ENABLE_PIN, LOW);
}

void setup() {
  Serial.begin(9600);
  Serial.println("Modbus RTU Server starting...");
  
  pinMode(RS485_ENABLE_PIN, OUTPUT);
  digitalWrite(RS485_ENABLE_PIN, LOW);
  pinMode(PIR_PIN, INPUT);
  
  if (!ModbusRTUServer.begin(1, 9600)) {
    Serial.println("Failed to start Modbus RTU Server!");
    while (1);
  }
  
  if (!ModbusRTUServer.configureHoldingRegisters(0, NB_REG)) {
    Serial.println("Failed to configure holding registers!");
    while (1);
  }
  
  dht.begin();
  ModbusRTUServer.holdingRegisterWrite(5, DEVICE_ID);

  ModbusRTUServer.onPreTransmission(preTransmission);
  ModbusRTUServer.onPostTransmission(postTransmission);
  
  Serial.println("Modbus RTU Server (slave) started, holding registers configured.");
}

void loop() {
  float temperature = dht.readTemperature();
  if (isnan(temperature)) {
    temperature = 0;
  }
  
  float humidity = dht.readHumidity();
  if (isnan(humidity)) {
    humidity = 0;
  }
  
  int mqValue = analogRead(MQ9_ANALOG_PIN);
  float CO_ppm = getCOppm(mqValue);
  float CH4_ppm = getCH4ppm(mqValue);
  int motion = digitalRead(PIR_PIN);
  
  ModbusRTUServer.holdingRegisterWrite(0, (uint16_t)(temperature * 10));
  ModbusRTUServer.holdingRegisterWrite(1, (uint16_t)(humidity * 10));
  ModbusRTUServer.holdingRegisterWrite(2, (uint16_t)(CO_ppm * 100));
  ModbusRTUServer.holdingRegisterWrite(3, (uint16_t)(CH4_ppm * 100));
  ModbusRTUServer.holdingRegisterWrite(4, (uint16_t)(motion ? 1 : 0));
  
  ModbusRTUServer.poll();
  delay(1000);
}
