#include <ArduinoModbus.h>
#include <DHT.h>
#include <math.h>

#define DHTPIN 10
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define PIRPIN 11
#define MQ9Analog A0
#define MQ9Digital 12

#define NB_REG 5

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
  while (!Serial) { }
  Serial.println("Hello, world!");
  
  dht.begin();
  pinMode(PIRPIN, INPUT);
  pinMode(MQ9Analog, INPUT);
  pinMode(MQ9Digital, INPUT);
  
  if (!ModbusRTUServer.begin(1, 9600)) {
    Serial.println("Failed to start Modbus RTU Server!");
    while (1);
  }
  
  ModbusRTUServer.configureHoldingRegisters(0, NB_REG);
  Serial.println("Modbus RTU Server (slave) started, waiting for master requests...");
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int mq9Raw = analogRead(MQ9Analog);
  float CO_ppm = getCOppm(mq9Raw);
  float CH4_ppm = getCH4ppm(mq9Raw);
  bool motion = (digitalRead(PIRPIN) == HIGH);
  
  ModbusRTUServer.holdingRegisterWrite(0, (uint16_t)(temperature * 10));
  ModbusRTUServer.holdingRegisterWrite(1, (uint16_t)(humidity * 10));
  ModbusRTUServer.holdingRegisterWrite(2, (uint16_t)(CO_ppm * 100));
  ModbusRTUServer.holdingRegisterWrite(3, (uint16_t)(CH4_ppm * 100));
  ModbusRTUServer.holdingRegisterWrite(4, motion ? 1 : 0);
  delay(1000);
  Serial.println("====== Current Sensor Readings ======");
  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");
  Serial.print("CO: ");
  Serial.print(CO_ppm, 2);
  Serial.println(" ppm");
  Serial.print("CH4: ");
  Serial.print(CH4_ppm, 2);
  Serial.println(" ppm");
  Serial.print("Motion: ");
  Serial.println(motion ? "YES" : "NO");
  Serial.println("Data successfully updated in holding registers.");
  Serial.println("------------------------------------------");
  
  ModbusRTUServer.poll();
  delay(1000);
}
