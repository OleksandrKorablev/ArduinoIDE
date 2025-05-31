#include <ModbusADU.h>
#include <ModbusSlaveLogic.h>
#include <ModbusRTUComm.h>
#include <DHT.h>
#include <math.h>

#define DHT_PIN         10
#define DHT_TYPE        DHT22

#define MQ9_ANALOG_PIN  A0
#define PIR_PIN         11
#define DEVICE_ID       101

#define RS485_ENABLE_PIN 13
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

#define MODBUS_SERIAL   Serial
#define MODBUS_BAUD     9600
#define MODBUS_CONFIG   SERIAL_8N1
#define MODBUS_UNIT_ID  1

const int16_t dePin = RS485_ENABLE_PIN;

ModbusRTUComm rtuComm(MODBUS_SERIAL, dePin);
ModbusSlaveLogic modbusLogic;

const uint8_t numHoldingRegisters = NB_REG;
uint16_t holdingRegisters[numHoldingRegisters];

void setup() {
  Serial.begin(9600);
  
  pinMode(RS485_ENABLE_PIN, OUTPUT);
  digitalWrite(RS485_ENABLE_PIN, LOW);
  pinMode(PIR_PIN, INPUT);
  
  dht.begin();
  holdingRegisters[5] = DEVICE_ID;
  modbusLogic.configureHoldingRegisters(holdingRegisters, numHoldingRegisters);
  
  MODBUS_SERIAL.begin(MODBUS_BAUD, MODBUS_CONFIG);
  rtuComm.begin(MODBUS_BAUD, MODBUS_CONFIG);
}

void loop() {
  float temperature = dht.readTemperature();
  if (isnan(temperature)) temperature = 0;
  
  float humidity = dht.readHumidity();
  if (isnan(humidity)) humidity = 0;
  
  int mqValue = analogRead(MQ9_ANALOG_PIN);
  float CO_ppm = getCOppm(mqValue);
  float CH4_ppm = getCH4ppm(mqValue);
  int motion = digitalRead(PIR_PIN);
  
  holdingRegisters[0] = (uint16_t)(temperature * 10);
  holdingRegisters[1] = (uint16_t)(humidity * 10);
  holdingRegisters[2] = (uint16_t)(CO_ppm * 100);
  holdingRegisters[3] = (uint16_t)(CH4_ppm * 100);
  holdingRegisters[4] = (uint16_t)(motion ? 1 : 0);
  
  ModbusADU adu;
  uint8_t error = rtuComm.readAdu(adu);
  if (error) { delay(10); return; }
  
  uint8_t unitId = adu.getUnitId();
  if (unitId != MODBUS_UNIT_ID && unitId != 0) return;
  
  modbusLogic.processPdu(adu);
  if (unitId != 0) rtuComm.writeAdu(adu);
  
  delay(1000);
}
