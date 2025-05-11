#include <SPI.h>
#include <SD.h>

#define SD_CS    10   // Chip Select
#define SD_SCK   13   // Serial Clock
#define SD_MOSI  11   // Master Out Slave In
#define SD_MISO  12   // Master In Slave Out

void setup() {
  Serial.begin(9600);  // Початок серійного з’єднання
  delay(1000);
  Serial.println("Початок ініціалізації SD-картки...");

  // Ініціалізація SPI (без параметрів)
  SPI.begin();

  if (!SD.begin(SD_CS)) {  // Перевірка на підключення SD картки
    Serial.println("Не вийшло під’єднати SD-картку.");
    return;
  }

  Serial.println("Успішне під’єднання до SD-картки.");

  if (!SD.exists("/System")) {  // Перевірка наявності папки
    Serial.println("Папка /System не знайдена.");
    return;
  }

  // Створення або відкриття файлу
  File file = SD.open("/System/connect.json", FILE_WRITE);
  if (file) {
    file.println("{\"status\": \"connected\"}");  // Запис даних у файл
    file.close();
    Serial.println("Файл connect.json створено у /System.");
  } else {
    Serial.println("Помилка створення файлу.");
  }
}

void loop() {
  // Нічого не робимо в циклі
}
