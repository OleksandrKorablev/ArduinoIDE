#include <ModbusADU.h>
#include <ModbusSlaveLogic.h>
#include <ModbusRTUComm.h>
#define MODBUS_SERIAL Serial
#define MODBUS_BAUD 9600
#define MODBUS_CONFIG SERIAL_8N1
#define MODBUS_UNIT_ID 1
const int16_t dePin = 13;

ModbusRTUComm rtuComm(MODBUS_SERIAL, dePin);
ModbusSlaveLogic modbusLogic;
const uint8_t numHoldingRegisters = 2;
uint16_t holdingRegisters[numHoldingRegisters];

void setup(){
  MODBUS_SERIAL.begin(MODBUS_BAUD, MODBUS_CONFIG);
  pinMode(dePin, OUTPUT);
  digitalWrite(dePin, LOW);
  uint32_t msg = 1001001UL;
  holdingRegisters[0] = (uint16_t)(msg >> 16);
  holdingRegisters[1] = (uint16_t)(msg & 0xFFFF);
  modbusLogic.configureHoldingRegisters(holdingRegisters, numHoldingRegisters);
  rtuComm.begin(MODBUS_BAUD, MODBUS_CONFIG);
}

void loop(){
  ModbusADU adu;
  uint8_t error = rtuComm.readAdu(adu);
  if(error){
    Serial.println("Data not sent");
    delay(10);
    return;
  }
  uint8_t uid = adu.getUnitId();
  if(uid != MODBUS_UNIT_ID && uid != 0){
    Serial.println("Data not sent");
    return;
  }
  modbusLogic.processPdu(adu);
  if(uid != 0) rtuComm.writeAdu(adu);
  delay(10);
}
