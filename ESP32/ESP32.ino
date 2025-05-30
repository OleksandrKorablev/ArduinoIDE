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

  // Підключення до Wi-Fi (режим станції)
  Serial.print("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
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

  // Перевірка наявності ключових файлів
  if (SD.exists("/System/Authorization.html")) {
    Serial.println("File /System/Authorization.html found.");
  } else {
    Serial.println("File /System/Authorization.html not found.");
  }
  if (SD.exists("/System/DataControllers.html")) {
    Serial.println("File /System/DataControllers.html found.");
  } else {
    Serial.println("File /System/DataControllers.html not found.");
  }

  // Налаштування статичних маршрутів для ресурсів (CSS, JS, бібліотеки, дані)
  server.serveStatic("/styles", SD, "/System/styles");
  server.serveStatic("/js", SD, "/System/js");
  server.serveStatic("/libs", SD, "/System/libs");
  server.serveStatic("/Login_and_Password", SD, "/System/Login_and_Password");
  server.serveStatic("/DataFromMicrocontrollers", SD, "/System/DataFromMicrocontrollers");

  // Маршрут для Authorization.html (при запиті до кореня "/")
  server.on("/", HTTP_GET, []() {
    if (SD.exists("/System/Authorization.html")) {
      File authFile = SD.open("/System/Authorization.html", FILE_READ);
      if (authFile) {
        Serial.println("Streaming /System/Authorization.html...");
        server.streamFile(authFile, "text/html");
        authFile.close();
      } else {
        Serial.println("Error opening /System/Authorization.html!");
        server.send(500, "text/html", "<h1>Error opening Authorization.html!</h1>");
      }
    } else {
      server.send(404, "text/html", "<h1>Authorization page not found!</h1>");
    }
  });

  // Маршрут для DataControllers.html (при запиті до "/DataControllers.html")
  server.on("/DataControllers.html", HTTP_GET, []() {
    if (SD.exists("/System/DataControllers.html")) {
      File dcFile = SD.open("/System/DataControllers.html", FILE_READ);
      if (dcFile) {
        Serial.println("Streaming /System/DataControllers.html...");
        server.streamFile(dcFile, "text/html");
        dcFile.close();
      } else {
        Serial.println("Error opening /System/DataControllers.html!");
        server.send(500, "text/html", "<h1>Error opening DataControllers.html!</h1>");
      }
    } else {
      server.send(404, "text/html", "<h1>DataControllers page not found!</h1>");
    }
  });

  // Запуск HTTP-сервера
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient(); // Постійна обробка HTTP-запитів
}
