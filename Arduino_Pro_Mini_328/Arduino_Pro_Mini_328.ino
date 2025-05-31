#include <ArduinoRS485.h>    
#include <ArduinoModbus.h>
#include <SoftwareSerial.h>
#include <DHT.h>
#include <math.h>

#define DHT_PIN         10
#define DHT_TYPE        DHT22

#define MQ9_ANALOG_PIN  A0
#define MQ9_DIGITAL_PIN 12

#define PIR_PIN         11

#define RS485_ENABLE_PIN 13

#define RS485_RX_PIN     4
#define RS485_TX_PIN     5

#define DEVICE_ID       101
#define NB_REG          6

DHT dht(DHT_PIN, DHT_TYPE);
SoftwareSerial RS485Serial(RS485_RX_PIN, RS485_TX_PIN);

float getCOppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  return pow((voltage / 5.0), -1.5) * 100;
}

float getCH4ppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  return pow((voltage / 5.0), -1.8) * 80;
}

void setup() {
  Serial.begin(9600);
  pinMode(RS485_ENABLE_PIN, OUTPUT);
  digitalWrite(RS485_ENABLE_PIN, LOW);
  pinMode(PIR_PIN, INPUT);

  RS485Serial.begin(9600);
  
  if (!ModbusRTUServer.begin(1, 9600)) {
    Serial.println("Failed to start Modbus RTU Server!");
    while (1);
  }
  
  if (!ModbusRTUServer.configureHoldingRegisters(0, NB_REG)) {
    Serial.println("Failed to configure holding registers!");
    while (1);
  }
  ModbusRTUServer.holdingRegisterWrite(5, DEVICE_ID);
  
  Serial.println("Modbus RTU Server (slave) started, waiting for master requests...");
  dht.begin();
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
  
  int packetReceived = ModbusRTUServer.poll();
  if (packetReceived)
    Serial.println("Data transmitted successfully!");
  
  delay(1000);
}
