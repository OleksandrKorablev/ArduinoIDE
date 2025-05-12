#include <SPI.h>
#include <SD.h>

#define SD_CS    5    // Chip Select
#define SD_SCK   18   // Serial Clock
#define SD_MOSI  23   // Master Out Slave In
#define SD_MISO  19   // Master In Slave Out

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("Початок ініціалізації SD-картки...");

  SPIClass spi = SPIClass(VSPI);
  spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, spi)) {
    Serial.println("Не вийшло під’єднати SD-картку.");
    return;
  }

  Serial.println("Успішне під’єднання до SD-картки.");

  if (!SD.exists("/System")) {
    Serial.println("Папка /System не знайдена.");
    return;
  }

  File file = SD.open("/System/connect.json", FILE_WRITE);
  if (file) {
    file.println("{\"status\": \"connected\"}");
    file.close();
    Serial.println("Файл connect.json створено у /System.");
  } else {
    Serial.println("Помилка створення файлу.");
  }
}

void loop() {

}
