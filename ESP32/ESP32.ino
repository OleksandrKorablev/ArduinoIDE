#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <time.h>
#include <ArduinoJson.h>
#include <string.h>

// ===================== Pin Configuration =====================
#define SD_CS    5    // Chip Select for SD card
#define SD_SCK   18   // Clock
#define SD_MOSI  23   // Master Out Slave In
#define SD_MISO  19   // Master In Slave Out

#define RS485_RX_PIN 16       // RS485 RX pin
#define RS485_TX_PIN 17       // RS485 TX pin
#define RS485_RE_DE_PIN 13    // RS485 mode control (Receive/Transmit)

// ===================== Wi-Fi Settings =====================
const char* ssid = "zakatov";
const char* password = "zaqxsw228";

// Глобальні змінні для шляхів до папок – присвоєння буде виконане після ініціалізації SD карти
String systemFolder;
String dataFolder;

// ===================== Create HTTP Server =====================
WebServer server(80);
HardwareSerial RS485(2);  // Using hardware serial for RS485

// Function to get a formatted timestamp (UTC)
String getFormattedTimestamp() {
  time_t now = time(nullptr);
  struct tm *timeinfo = gmtime(&now);
  char buffer[21];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
  return String(buffer);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Connecting to Wi-Fi
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");
  Serial.print("ESP32 Local IP Address: ");
  Serial.println(WiFi.localIP());

  // Synchronizing time via NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for time synchronization...");

  // Initialize SD card using the specified SPI pins
  Serial.println("Checking SD card...");
  SPIClass spiSD(HSPI);
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("SD card not found!");
    return;
  } else {
    Serial.println("SD card successfully initialized.");

    // Тепер, після успішної ініціалізації, присвоюємо значення для шляхів
    systemFolder = "/System";
    dataFolder   = "/System/DataFromMicrocontrollers/";

    // Check existence of /System folder
    if (!SD.exists(systemFolder.c_str())) {
      Serial.println("'/System' folder not found!");
    } else {
      Serial.println("'/System' folder found.");
    }

    // Check and create /System/DataFromMicrocontrollers folder if necessary
    if (!SD.exists(dataFolder.c_str())) {
      Serial.println("Creating '/System/DataFromMicrocontrollers' folder...");
      SD.mkdir(dataFolder.c_str());
      Serial.println("Folder created!");
    } else {
      Serial.println("'/System/DataFromMicrocontrollers' folder already exists.");
    }

    // Ensure that index.json exists; if not, create it with initial structure.
    String indexPath = dataFolder + "index.json";
    if (!SD.exists(indexPath.c_str())) {
      Serial.println("Creating index.json file...");
      File initFile = SD.open(indexPath.c_str(), FILE_WRITE);
      if (initFile) {
        DynamicJsonDocument initDoc(1024);
        initDoc["files"] = JsonArray();  // Create an empty array
        if (serializeJson(initDoc, initFile) == 0) {
          Serial.println("Failed to write to index.json");
        }
        initFile.close();
        Serial.println("index.json created.");
      } else {
        Serial.println("Failed to open index.json for writing.");
      }
    }
  }

  // RS485 Setup
  Serial.println("Configuring RS485...");
  RS485.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(RS485_RE_DE_PIN, OUTPUT);
  digitalWrite(RS485_RE_DE_PIN, LOW);
  Serial.println("RS485 ready for data reception.");

  // Check for Authorization.html file
  String authPath = systemFolder + "/Authorization.html";
  Serial.println("Checking for Authorization.html file...");
  if (SD.exists(authPath.c_str())) {
    Serial.println("Authorization.html file found.");
  } else {
    Serial.println("Authorization.html file NOT FOUND!");
  }

  // Setup HTTP server root route.
  server.on("/", HTTP_GET, [authPath]() {
    if (SD.exists(authPath.c_str())) {
      File authFile = SD.open(authPath.c_str(), FILE_READ);
      if (authFile) {
        Serial.println("Streaming Authorization.html page...");
        // Stream the file content without loading it entirely into memory
        server.streamFile(authFile, "text/html");
        authFile.close();
      } else {
        Serial.println("Error opening Authorization.html file.");
        server.send(500, "text/html", "<h1>Error opening Authorization.html file!</h1>");
      }
    } else {
      server.send(404, "text/html", "<h1>Authorization page not found!</h1>");
    }
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();

  // Receiving data via RS485
  if (RS485.available()) {
    String receivedData = RS485.readStringUntil('\n');
    receivedData.trim();
    if (receivedData.length() > 0) {
      Serial.println("Data received from Arduino:");
      Serial.println(receivedData);

      // Extract device ID and sensor data.
      int idx = receivedData.indexOf(" /");
      String deviceID = (idx != -1) ? receivedData.substring(0, idx) : "UNKNOWN";
      String sensorData = (idx != -1) ? receivedData.substring(idx + 3) : receivedData;

      // Get current timestamp.
      String timestamp = getFormattedTimestamp();
      String fileName = dataFolder + deviceID + "_" + timestamp + ".json";

      // Create JSON document with device data.
      DynamicJsonDocument jsonDoc(256);
      jsonDoc["deviceID"] = deviceID;
      jsonDoc["timestamp"] = timestamp;

      // Parse sensor data.
      int tempIdx = sensorData.indexOf("Temperature: ");
      int humIdx = sensorData.indexOf(" / Humidity: ");
      int motionIdx = sensorData.indexOf(" / Motion: ");
      int coIdx = sensorData.indexOf(" / CO: ");
      int ch4Idx = sensorData.indexOf(" / CH₄: ");

      jsonDoc["temperature"] = sensorData.substring(tempIdx + 12, humIdx).toFloat();
      jsonDoc["humidity"] = sensorData.substring(humIdx + 12, motionIdx).toFloat();
      jsonDoc["motion"] = (sensorData.substring(motionIdx + 7, coIdx) == "Yes");
      jsonDoc["CO"] = sensorData.substring(coIdx + 6, ch4Idx).toFloat();
      jsonDoc["CH4"] = sensorData.substring(ch4Idx + 6).toFloat();

      // Write JSON document to file.
      File dataFile = SD.open(fileName.c_str(), FILE_WRITE);
      if (dataFile) {
        serializeJson(jsonDoc, dataFile);
        dataFile.close();
        Serial.println("Data saved to file:");
        Serial.println(fileName);
      } else {
        Serial.println("ERROR writing to file!");
      }

      // Update index.json.
      String indexPath = dataFolder + "index.json";
      File indexFile = SD.open(indexPath.c_str(), FILE_READ);
      DynamicJsonDocument indexDoc(1024);
      if (indexFile) {
        deserializeJson(indexDoc, indexFile);
        indexFile.close();
      }
      if (!indexDoc.containsKey("files")) {
        indexDoc["files"] = JsonArray();
      }
      size_t folderLength = dataFolder.length();
      indexDoc["files"].add(fileName.substring(folderLength));

      indexFile = SD.open(indexPath.c_str(), FILE_WRITE);
      serializeJson(indexDoc, indexFile);
      indexFile.close();
      Serial.println("index.json updated!");
    }
  }
}
