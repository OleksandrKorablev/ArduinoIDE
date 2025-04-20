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

  // Перевірка підключення датчиків
  bool dhtStatus = checkDHT22();
  bool pirStatus = checkHC_SR501();
  bool mq9Status = checkMQ9();

  if (!dhtStatus) {
    Serial.println("Помилка: Датчик DHT22 не підключено або не відповідає!");
  }
  if (!pirStatus) {
    Serial.println("Помилка: Датчик HC-SR501 не підключено або не відповідає!");
  }
  if (!mq9Status) {
    Serial.println("Помилка: Датчик MQ-9 не підключено або не відповідає!");
  }
}

void loop() {
  // Зчитування даних з датчика DHT22
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Зчитування даних з HC-SR501
  bool motionDetected = digitalRead(PIRPIN);

  // Зчитування даних з MQ-9
  int gasAnalog = analogRead(MQ9Analog);

  // Формування рядка даних для передачі
  String data = String("Температура: ") + temperature +
                " / Вологість: " + humidity +
                " / Рух: " + (motionDetected ? "Так" : "Ні") +
                " / CO: " + gasAnalog +
                " / CH₄: " + gasAnalog;

  
  Serial.println(data);// Вивід даних у Serial для перевірки

  // Передача даних через RS485
  digitalWrite(RE_DE, HIGH);
  delay(1);  
  RS485.println(data);
  delay(1); 
  digitalWrite(RE_DE, LOW); // Повертаємося у режим прийому

  delay(1000); 
}
