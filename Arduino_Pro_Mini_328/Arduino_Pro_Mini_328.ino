#include <DHT.h>
#include <SoftwareSerial.h>
#include <math.h>

// ==== Sensor Setup ====
// DHT22 підключений до D10
#define DHTPIN 10       
#define DHTTYPE DHT22   
DHT dht(DHTPIN, DHTTYPE);

// MQ‑9: аналоговий вихід на A0; (цифровий на D12 можна підключати, але в цьому прикладі використовується аналоговий)
#define MQ9_ANALOG A0

// PIR (HC-SR501): підключено до D11
#define PIR_PIN 11       

// ==== RS485 Setup ====
// Arduino Pro Mini підключена до MAX485 таким чином:
// - HW Serial для дебагу не використовується для RS485,
// - Для RS485 застосовуємо SoftwareSerial на пінах 2 (RX) і 3 (TX)
// - Пін RS485_CONTROL (RE/DE) підключено до D13
#define RS485_CONTROL 13  
SoftwareSerial RS485(2, 3); // RX, TX для RS485

// ==== Device ID ====
const String DEVICE_ID = "ROOM_1";

// ==== Gas level estimation ====
// Функції для розрахунку концентрацій газів (довільні коефіцієнти)
float getCOppm(int sensorValue) {
  // Тут використовується просте співвідношення; реальна функція може бути складнішою
  float voltage = (sensorValue / 1023.0) * 5.0;
  // Довільне масштабування (наприклад, 0.20)
  return voltage * 0.20;
}

float getCH4ppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  // Довільне масштабування (наприклад, 0.25)
  return voltage * 0.25;
}

// ==== Додаткові функції RS485: управління режимом передачі/прийому ====
void RS485_setTransmit() {
  digitalWrite(RS485_CONTROL, HIGH); // У режим передачі
}

void RS485_setReceive() {
  digitalWrite(RS485_CONTROL, LOW);  // У режим прийому
}

// ==== Функції перевірки підключення датчиків ====

// Перевірка DHT22 (якщо при зчитуванні температури чи вологості отримуємо NaN, то сенсор не працює)
bool checkDHT22() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  return !(isnan(t) || isnan(h));
}

// Перевірка MQ‑9: якщо аналогове значення менше порогового (наприклад, 10), вважаємо, що сенсор не підключено
bool checkMQ9() {
  int val = analogRead(MQ9_ANALOG);
  return (val > 10);
}

// Перевірка PIR: провести тест протягом 5 секунд, і якщо хоча б один раз зчитано HIGH, вважаємо, що PIR працює.
// Якщо протягом тесту HIGH не з’явився, виводимо помилку.
bool checkPIR() {
  unsigned long startTime = millis();
  bool detected = false;
  while (millis() - startTime < 5000) {
    if (digitalRead(PIR_PIN) == HIGH) {
      detected = true;
      break;
    }
  }
  return detected;
}

void setup() {
  // Ініціалізація серійного порту для дебагу
  Serial.begin(9600);
  
  // Ініціалізація RS485 SoftwareSerial
  RS485.begin(9600);
  
  // Ініціалізація DHT22
  dht.begin();
  
  // Налаштування входів для PIR і MQ‑9
  pinMode(PIR_PIN, INPUT);
  pinMode(MQ9_ANALOG, INPUT);
  
  // Налаштування RS485 CONTROL
  pinMode(RS485_CONTROL, OUTPUT);
  RS485_setReceive(); // Спочатку у режимі прийому
  
  Serial.println("=== SYSTEM START ===");
  
  // Перевірка підключення датчиків
  if (!checkDHT22()) {
    Serial.println("Error: DHT22 sensor NOT connected or not responding!");
  } else {
    Serial.println("DHT22 sensor OK.");
  }
  
  if (!checkMQ9()) {
    Serial.println("Error: MQ-9 sensor NOT connected or reading too low!");
  } else {
    Serial.println("MQ-9 sensor OK.");
  }
  
  if (!checkPIR()) {
    Serial.println("Error: HC-SR501 (PIR) sensor NOT connected or not responding!");
  } else {
    Serial.println("PIR sensor OK.");
  }
}

void loop() {
  // Перевірка підключення (щоб переконатися, що датчики продовжують працювати)
  bool dhtStatus = checkDHT22();
  bool mq9Status = checkMQ9();
  bool pirStatus = checkPIR(); // Оскільки PIR може бути неактивним, тут використаємо його поточне зчитування десь окремо
  
  if (!dhtStatus || !mq9Status || !pirStatus) {
    // Якщо хоча б один сенсор не відповідає, інформуємо і пропускаємо передачу
    if (!dhtStatus) { Serial.println("Polling error: DHT22 not responding."); }
    if (!mq9Status) { Serial.println("Polling error: MQ-9 reading too low."); }
    if (!pirStatus) { Serial.println("Polling error: PIR sensor not detecting any motion (check connection)."); }
    Serial.println("Skipping data transmission.");
    delay(3000);
    return;
  }
  
  // ---- Опитування датчиків ----
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  int mq9Raw = analogRead(MQ9_ANALOG);
  float CO_ppm = getCOppm(mq9Raw);
  float CH4_ppm = getCH4ppm(mq9Raw);
  
  // Для PIR: якщо сигнал HIGH – рух є; якщо LOW – руху немає.
  bool motion = digitalRead(PIR_PIN) == HIGH;
  const char* movement = motion ? "YES" : "NO";
  
  // ---- Формування повідомлення ----
  String message = "ID: " + DEVICE_ID;
  message += " / Temperature: " + String(temperature, 1);
  message += " / Humidity: " + String(humidity, 1);
  message += " / CH₄: " + String(CH4_ppm, 2);
  message += " / CO: " + String(CO_ppm, 2);
  message += " / Movement: " + String(movement);
  
  // Вивід даних у серійний монітор для перевірки
  Serial.println("Collected Data: " + message);
  
  // ---- Передача даних через RS485 ----
  RS485_setTransmit(); // У режим передачі
  delay(2);            // Коротка затримка для стабілізації
  
  // Передаємо дані через SoftwareSerial RS485
  int bytesSent = RS485.write(message.c_str(), message.length());
  RS485.flush(); // Чекаємо завершення передачі
  
  if (bytesSent == message.length()) {
    Serial.println("RS485 Transmission successful.");
  } else {
    Serial.print("RS485 Transmission error: Only ");
    Serial.print(bytesSent);
    Serial.print(" bytes sent out of ");
    Serial.print(message.length());
    Serial.println(" bytes.");
  }
  
  RS485_setReceive(); // Повертаємо RS485 у режим прийому
  
  delay(3000); // Затримка між опитуваннями
}
