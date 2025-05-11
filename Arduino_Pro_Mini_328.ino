#include <SoftwareSerial.h>
#include <DHT.h>

#define DHTPIN 10       // Підключення DATA DHT22 до D10
#define DHTTYPE DHT22   // Тип датчика DHT22

#define PIRPIN 11       // Підключення OUT HC-SR501 до D11

#define MQ9Analog A0    // Аналоговий вихід MQ-9 до A0
#define MQ9Digital 12   // Цифровий вихід MQ-9 до D12

#define RE_DE 13        // Контроль RE та DE на Max485

DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial RS485(2, 3); // RX, TX для Max485

const String DEVICE_ID = "ROOM_1"; // Унікальний ID пристрою

// Функція для визначення рівня CO у ppm
float getCOppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  float CO_ppm = pow((voltage / 5.0), -1.5) * 100; // Калібрувальна функція для CO
  return CO_ppm;
}

// Функція для визначення рівня CH₄ у ppm
float getCH4ppm(int sensorValue) {
  float voltage = (sensorValue / 1023.0) * 5.0;
  float CH4_ppm = pow((voltage / 5.0), -1.8) * 80; // Калібрувальна функція для CH₄
  return CH4_ppm;
}

bool checkDHT22() {
  float testTemp = dht.readTemperature();
  float testHum = dht.readHumidity();
  return !isnan(testTemp) && !isnan(testHum);
}

bool checkHC_SR501() {
  int testPIR = digitalRead(PIRPIN);
  return (testPIR == HIGH || testPIR == LOW);
}

bool checkMQ9() {
  int testAnalog = analogRead(MQ9Analog);
  int testDigital = digitalRead(MQ9Digital);
  return (testAnalog >= 0 && testAnalog <= 1023) && (testDigital == HIGH || testDigital == LOW);
}

void setup() {
  Serial.begin(9600);
  RS485.begin(9600);
  dht.begin();
  pinMode(PIRPIN, INPUT);
  pinMode(MQ9Digital, INPUT);
  pinMode(RE_DE, OUTPUT);
  digitalWrite(RE_DE, LOW); // Режим прийому за замовчуванням

  Serial.println("🔍 Перевірка підключення датчиків...");

  // Перевірка підключення датчиків
  bool dhtStatus = checkDHT22();
  bool pirStatus = checkHC_SR501();
  bool mq9Status = checkMQ9();

  if (!dhtStatus) {
    Serial.println("❌ Датчик DHT22 НЕ підключено або не відповідає!");
  } else {
    Serial.println("✅ Датчик DHT22 успішно підключений.");
  }

  if (!pirStatus) {
    Serial.println("❌ Датчик HC-SR501 НЕ підключено або не відповідає!");
  } else {
    Serial.println("✅ Датчик HC-SR501 успішно підключений.");
  }

  if (!mq9Status) {
    Serial.println("❌ Датчик MQ-9 НЕ підключено або не відповідає!");
  } else {
    Serial.println("✅ Датчик MQ-9 успішно підключений.");
  }
}

void loop() {
  // Зчитування даних з датчика DHT22
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Зчитування даних з HC-SR501
  bool motionDetected = digitalRead(PIRPIN);

  // Зчитування даних з MQ-9
  int sensorValue = analogRead(MQ9Analog);
  float CO_ppm = getCOppm(sensorValue);
  float CH4_ppm = getCH4ppm(sensorValue);

  // Формування рядка даних для передачі (додаємо ID)
  String data = DEVICE_ID + " / Температура: " + temperature +
                " / Вологість: " + humidity +
                " / Рух: " + (motionDetected ? "Так" : "Ні") +
                " / CO: " + CO_ppm + " ppm" +
                " / CH₄: " + CH4_ppm + " ppm";

  Serial.println("📊 Поточні показники:");
  Serial.println(data);

  // Передача даних через RS485
  digitalWrite(RE_DE, HIGH);
  delay(1);  
  RS485.println(data);
  delay(1); 
  digitalWrite(RE_DE, LOW); // Повертаємося у режим прийому

  delay(1000); 
}
