#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>

// ===================== Wi-Fi Settings =====================
const char* ssid = "zakatov";         // Ім'я мережі
const char* password = "zaqxsw228";   // Пароль мережі

// ===================== SD Card Configuration =====================
#define SD_CS 5  // Пін, до якого підключено Chip Select SD модуля

// ===================== Create HTTP Server =====================
WebServer server(80); // HTTP-сервер на порті 80

void setup() {
  Serial.begin(115200);
  delay(100);

  // Підключення до Wi-Fi
  Serial.print("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA); // Встановлюємо режим як станція (STA)
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

  // Налаштування статичних маршрутів для ресурсів, що використовуються у сторінці
  server.serveStatic("/styles", SD, "/System/styles");
  server.serveStatic("/js", SD, "/System/js");
  server.serveStatic("/libs", SD, "/System/libs");
  server.serveStatic("/Login_and_Password", SD, "/System/Login_and_Password");
  server.serveStatic("/DataFromMicrocontrollers", SD, "/System/DataFromMicrocontrollers");

  // Налаштування кореневого маршруту для прямого відкриття Authorization.html
  server.on("/", HTTP_GET, []() {
    if (SD.exists("/System/Authorization.html")) {
      File authFile = SD.open("/System/Authorization.html", FILE_READ);
      if (authFile) {
        Serial.println("Streaming /System/Authorization.html...");
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
  server.handleClient(); // Постійна обробка HTTP-запитів
}
