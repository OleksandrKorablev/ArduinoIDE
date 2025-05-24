#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>

// ===================== Wi-Fi Settings =====================
const char* ssid = "zakatov";         // Ім'я мережі
const char* password = "zaqxsw228";   // Пароль мережі

// ===================== SD Card configuration =====================
#define SD_CS 5  // Пін, до якого підключено Chip Select SD модуля

// ===================== Create HTTP Server =====================
WebServer server(80); // HTTP-сервер на порті 80

void setup() {
  Serial.begin(115200);
  delay(100);

  // Підключення до Wi-Fi
  Serial.print("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA); // Встановлюємо режим станції
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

  // Налаштування маршруту для кореневого запиту "/"
  // Цей маршрут повертає просту HTML-сторінку з повідомленням та кнопкою
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32 Status</title></head><body>";
    html += "<h1>Everything works!</h1>";
    html += "<p><a href='/auth'><button style='font-size:20px;padding:10px;'>Proceed to Authorization</button></a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  // Налаштування маршруту "/auth"
  // Цей маршрут стрімить вміст файлу /System/Authorization.html
  server.on("/auth", HTTP_GET, []() {
    if (SD.exists("/System/Authorization.html")) {
      File authFile = SD.open("/System/Authorization.html", FILE_READ);
      if (authFile) {
        Serial.println("Streaming Authorization.html file...");
        server.streamFile(authFile, "text/html");
        authFile.close();
      } else {
        Serial.println("Error opening /System/Authorization.html file.");
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
  server.handleClient(); // Обробка вхідних HTTP-запитів
}
