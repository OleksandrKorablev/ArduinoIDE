#include <ModbusRTUMaster.h>

#define MODBUS_BAUD 9600
#define MODBUS_CONFIG SERIAL_8N1
#define MODBUS_UNIT_ID 1

ModbusRTUMaster modbus(Serial2, 13);

const uint8_t numRegisters = 6;
uint16_t sensorData[numRegisters] = {0};

void setup() {
  Serial.begin(9600);
  
  Serial2.begin(MODBUS_BAUD, MODBUS_CONFIG, 16, 17);
  modbus.begin(MODBUS_BAUD, MODBUS_CONFIG);
  
  Serial.println("ESP32 Modbus Master Initialized");
}

void loop() {
  uint8_t error = modbus.readHoldingRegisters(MODBUS_UNIT_ID, 0, sensorData, numRegisters);
  if (error == 0) {
    Serial.println("Data received successfully:");
    
    Serial.print("Temperature: ");
    Serial.print(sensorData[0] / 10.0);
    Serial.println(" C");
    
    Serial.print("Humidity: ");
    Serial.print(sensorData[1] / 10.0);
    Serial.println(" %");
    
    Serial.print("CO ppm: ");
    Serial.print(sensorData[2] / 100.0);
    Serial.println(" ppm");
    
    Serial.print("CH4 ppm: ");
    Serial.print(sensorData[3] / 100.0);
    Serial.println(" ppm");
    
    Serial.print("Motion: ");
    Serial.println(sensorData[4]);
    
    Serial.print("Device ID: ");
    Serial.println(sensorData[5]);
  }
  else {
    Serial.print("Error reading Modbus data, code: ");
    Serial.println(error);
  }
  delay(2000);
}
