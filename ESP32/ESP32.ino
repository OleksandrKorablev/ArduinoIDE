#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>

// ===================== Wi-Fi Settings =====================
const char* ssid = "zakatov";         // Змініть на ім'я вашої мережі
const char* password = "zaqxsw228"; // Змініть на ваш пароль

// ===================== SD Card configuration =====================
#define SD_CS 5  // Пін, до якого підключено Chip Select SD модуля

// ===================== Create HTTP Server =====================
WebServer server(80); // HTTP-сервер, що слухає порт 80

void setup() {
  Serial.begin(115200);
  delay(100);

  // Підключення до Wi-Fi
  Serial.print("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA); // Встановлюємо режим як станція
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");
  Serial.print("ESP32 Local IP Address: ");
  Serial.println(WiFi.localIP());

  // Ініціалізація SD-карти через SPI
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully.");

  // Перевірка наявності файлу /System/Authorization.html
  if (SD.exists("/System/Authorization.html")) {
    Serial.println("File /System/Authorization.html found.");
  } else {
    Serial.println("File /System/Authorization.html not found.");
  }

  // Налаштування кореневого маршруту HTTP-сервера
  server.on("/", HTTP_GET, []() {
    if (SD.exists("/System/Authorization.html")) {
      File authFile = SD.open("/System/Authorization.html", FILE_READ);
      if (authFile) {
        Serial.println("Streaming Authorization.html file...");
        // Стрімимо вміст файлу без необхідності зберігати його повністю в оперативній пам'яті
        server.streamFile(authFile, "text/html");
        authFile.close();
      } else {
        Serial.println("Error opening /System/Authorization.html!");
        server.send(500, "text/html", "<h1>Error opening Authorization.html file!</h1>");
      }
    } else {
      server.send(404, "text/html", "<h1>Authorization page not found!</h1>");
    }
  });

  // Запуск HTTP-сервера
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
