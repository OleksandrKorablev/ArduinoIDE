#include <ModbusMaster.h>
#include <DHT.h>
#include <math.h>

// ----- Налаштування для RS485/Modbus -----
#define MODBUS_DIR_PIN     13       // Пін для керування напрямком (DE/RE) модуля MAX485
#define MODBUS_SERIAL_BAUD 9600     // Швидкість передачі

// ----- Налаштування сенсорів -----
#define DHTPIN     10              // DHT22: DATA підключено до D10
#define DHTTYPE    DHT22           // Тип DHT: DHT22
DHT dht(DHTPIN, DHTTYPE);

#define PIRPIN     11              // PIR (HC-SR501): OUT підключено до D11
#define MQ9Analog  A0              // MQ-9: аналоговий вихід AO підключено до A0
#define MQ9Digital 12              // MQ-9: цифровий вихід (якщо потрібен)

// ----- Унікальний ID цього мікроконтролера -----
// Це значення буде передаватися разом із даними для ідентифікації джерела.
#define MC_ID 101

// ----- Об'єкт ModbusMaster -----
// Зауваж, що тут ми задаємо ID цільового слейва (пристрій, до якого звертаємось).
// Наприклад, якщо ESP32 налаштована як слейв з ID = 1, то:
ModbusMaster modbusMaster;

// Для керування напрямком передачі через MAX485 потрібні callback-функції:
void preTransmission() {
  digitalWrite(MODBUS_DIR_PIN, HIGH); // Увімкнути режим передачі
}

void postTransmission() {
  digitalWrite(MODBUS_DIR_PIN, LOW);  // Увімкнути режим прийому
}

// Кількість регістрів, які ми будемо записувати в слейв-пристрій.
// Регістр 0: температура (°C * 10)
// Регістр 1: вологість (% * 10)
// Регістр 2: значення CO (ppm)
// Регістр 3: значення CH₄ (ppm)
// Регістр 4: стан руху (0 або 1)
// Регістр 5: ID мікроконтролера
const uint16_t NUM_REGS = 6;

void setup() {
  // Налаштовуємо сенсорні піни
  pinMode(PIRPIN, INPUT);
  pinMode(MQ9Digital, INPUT);
  pinMode(MODBUS_DIR_PIN, OUTPUT);
  digitalWrite(MODBUS_DIR_PIN, LOW);  // Спочатку встановлюємо режим прийому

  // Ініціалізуємо апаратний порт для RS485
  Serial.begin(MODBUS_SERIAL_BAUD);
  // Ініціалізуємо DHT22
  dht.begin();

  // Налаштовуємо ModbusMaster:
  // Тут ми задаємо ID віддаленого слейва; наприклад, якщо ESP32 налаштована як слейв з ID = 1:
  modbusMaster.begin(1, Serial);
  modbusMaster.preTransmission(preTransmission);
  modbusMaster.postTransmission(postTransmission);
}

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

void loop() {
  uint8_t result;

  // Зчитуємо дані з сенсорів
  float temperature = dht.readTemperature();  // °C
  float humidity    = dht.readHumidity();       // %
  int mq9Raw = analogRead(MQ9Analog);             // сире значення з MQ-9

  // Обчислюємо концентрації газів
  float CO_ppm   = getCOppm(mq9Raw);
  float CH4_ppm  = getCH4ppm(mq9Raw);
  int motion     = (digitalRead(PIRPIN) == HIGH) ? 1 : 0;

  // Масштабуємо значення для передачі:
  // Для збереження десятинної точності температуру та вологість множимо на 10.
  uint16_t reg0 = (uint16_t)(temperature * 10); // Наприклад, 10.8°C → 108
  uint16_t reg1 = (uint16_t)(humidity * 10);    // Наприклад, 48.5% → 485
  uint16_t reg2 = (uint16_t)CO_ppm;
  uint16_t reg3 = (uint16_t)CH4_ppm;
  uint16_t reg4 = (uint16_t)motion;
  uint16_t reg5 = MC_ID;  // Додаємо ID мікроконтролера

  // Розміщуємо дані в буфері для передачі:
  modbusMaster.setTransmitBuffer(0, reg0);
  modbusMaster.setTransmitBuffer(1, reg1);
  modbusMaster.setTransmitBuffer(2, reg2);
  modbusMaster.setTransmitBuffer(3, reg3);
  modbusMaster.setTransmitBuffer(4, reg4);
  modbusMaster.setTransmitBuffer(5, reg5);

  // Виконуємо запис кількох регістрів (починаючи з адреси 0, кількість - 6) до слейва
  result = modbusMaster.writeMultipleRegisters(0, NUM_REGS);

  if (result == modbusMaster.ku8MBSuccess) {
    Serial.println("Modbus write successful.");
  } else {
    Serial.print("Modbus write failed. Error: ");
    Serial.println(result, HEX);
  }

  delay(1000); // Оновлення даних кожну секунду
}
