#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <time.h>
#include <ArduinoJson.h> // Для JSON

// ===================== Налаштування пінів =====================
#define SD_CS_PIN 2     // CS -> SD2
#define RS485_RX_PIN 16
#define RS485_TX_PIN 17
#define RS485_RE_DE_PIN 13

// ========================== Wi-Fi ============================
const char* ssid     = "YOUR_SSID";       
const char* password = "YOUR_PASSWORD";   

// ===================== HTTP-Сервер ===========================
WebServer server(80);
HardwareSerial RS485(2);

// =================== Форматування часу =======================
String getFormattedTimestamp() {
  time_t now = time(nullptr);
  struct tm *timeinfo = gmtime(&now);
  char buffer[21];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
  return String(buffer);
}

// =============================== SETUP ========================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  // 1️⃣ Підключення до Wi-Fi
  Serial.print("Підключення до Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi підключено!");
  Serial.print("Локальна IP ESP32: ");
  Serial.println(WiFi.localIP());

  // 2️⃣ Синхронізація NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("⏳ Очікуємо отримання часу...");

  // 3️⃣ Ініціалізація SD-карти
  Serial.println("🔍 Перевірка SD-карти...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("❌ SD-карта НЕ підключена!");
    return;
  } else {
    Serial.println("✅ SD-карта успішно підключена.");
    
    // Перевірка `/System`
    if (!SD.exists("/System")) {
      Serial.println("❌ Папка /System НЕ знайдена!");
    } else {
      Serial.println("✅ Папка /System знайдена.");
    }

    // Перевірка `/System/DataFromMicrocontrollers`
    if (!SD.exists("/System/DataFromMicrocontrollers")) {
      Serial.println("🛠 Створюємо папку /System/DataFromMicrocontrollers...");
      SD.mkdir("/System/DataFromMicrocontrollers");
      Serial.println("✅ Папку створено!");
    } else {
      Serial.println("✅ Папка /System/DataFromMicrocontrollers вже існує.");
    }
  }

  // 4️⃣ Налаштування RS485
  Serial.println("🔧 Налаштування RS485...");
  RS485.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(RS485_RE_DE_PIN, OUTPUT);
  digitalWrite(RS485_RE_DE_PIN, LOW);
  Serial.println("✅ RS485 готовий до прийому даних.");

  // 5️⃣ HTTP-сервер (перевірка Authorization.html)
  Serial.println("🔍 Перевіряємо файл Authorization.html...");
  if (SD.exists("/System/Authorization.html")) {
    Serial.println("✅ Файл Authorization.html знайдено.");
  } else {
    Serial.println("❌ Файл Authorization.html ВІДСУТНІЙ!");
  }
  
  server.on("/", HTTP_GET, []() {
    if (SD.exists("/System/Authorization.html")) {
      File file = SD.open("/System/Authorization.html", FILE_READ);
      String html = "";
      while (file.available()) {
        html += (char)file.read();
      }
      file.close();
      Serial.println("✅ Веб-сторінка Authorization.html завантажена.");
      server.send(200, "text/html", html);
    } else {
      server.send(404, "text/html", "<h1>Сторінку авторизації не знайдено!</h1>");
    }
  });

  server.begin();
  Serial.println("✅ HTTP сервер запущено.");
}

// =============================== LOOP ========================
void loop() {
  server.handleClient();

  // Прийом даних з RS485
  if (RS485.available()) {
    String receivedData = RS485.readStringUntil('\n');
    receivedData.trim();
    if (receivedData.length() > 0) {
      Serial.println("📥 Отримано дані з Arduino:");
      Serial.println(receivedData);

      // Витягуємо ID пристрою
      int idx = receivedData.indexOf(" /");
      String deviceID = (idx != -1) ? receivedData.substring(0, idx) : "UNKNOWN";
      String sensorData = (idx != -1) ? receivedData.substring(idx + 3) : receivedData;

      // Отримання поточного часу
      String timestamp = getFormattedTimestamp(); 
      String fileName = "/System/DataFromMicrocontrollers/" + deviceID + "_" + timestamp + ".json";

      // Формування JSON-структури
      DynamicJsonDocument jsonDoc(256);
      jsonDoc["deviceID"] = deviceID;
      jsonDoc["timestamp"] = timestamp;

      // Розбір сенсорних даних
      int tempIdx = sensorData.indexOf("Температура: ");
      int humIdx = sensorData.indexOf(" / Вологість: ");
      int motionIdx = sensorData.indexOf(" / Рух: ");
      int coIdx = sensorData.indexOf(" / CO: ");
      int ch4Idx = sensorData.indexOf(" / CH₄: ");

      jsonDoc["temperature"] = sensorData.substring(tempIdx + 12, humIdx).toFloat();
      jsonDoc["humidity"] = sensorData.substring(humIdx + 12, motionIdx).toFloat();
      jsonDoc["motion"] = (sensorData.substring(motionIdx + 7, coIdx) == "Так");
      jsonDoc["CO"] = sensorData.substring(coIdx + 6, ch4Idx).toFloat();
      jsonDoc["CH4"] = sensorData.substring(ch4Idx + 6).toFloat();

      // Запис у JSON-файл
      File dataFile = SD.open(fileName, FILE_WRITE);
      if (dataFile) {
        serializeJson(jsonDoc, dataFile);
        dataFile.close();
        Serial.println("✅ Дані збережено у файл:");
        Serial.println(fileName);
      } else {
        Serial.println("❌ ПОМИЛКА запису у файл!");
      }

      // Оновлення `index.json`
      File indexFile = SD.open("/System/DataFromMicrocontrollers/index.json", FILE_READ);
      DynamicJsonDocument indexDoc(1024);
      if (indexFile) {
        deserializeJson(indexDoc, indexFile);
        indexFile.close();
      }
      indexDoc["files"].add(fileName.substring(24));

      indexFile = SD.open("/System/DataFromMicrocontrollers/index.json", FILE_WRITE);
      serializeJson(indexDoc, indexFile);
      indexFile.close();
      Serial.println("📄 `index.json` оновлено!");
    }
  }
}

