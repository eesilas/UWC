#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

// 感測器通信參數
#define COM 0x55
#define BAUD_RATE 115200
#define TIMEOUT_MS 500    // 增加超時時間至500ms以確保接收數據
#define MAX_DISTANCE_CM 600  // 最大距離：600 cm
#define OLED_REFRESH_MS 1000  // OLED刷新間隔：1秒

// 第一個感測器（Left，使用UART2）
#define RXD2 16  // GPIO16 (RX2)
#define TXD2 17  // GPIO17 (TX2)
HardwareSerial SerialSensorLeft(2);  // UART2

// 第二個感測器（Right，使用UART1）
#define RXD1 9   // GPIO9 (RX1)
#define TXD1 10  // GPIO10 (TX1)
HardwareSerial SerialSensorRight(1);  // UART1

// OLED顯示屏參數
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Wi-Fi設置（Access Point模式）
const char* ssid = "ESP32_Webserver";
const char* password = "12345678";
IPAddress local_IP(192, 168, 40, 1);
IPAddress gateway(192, 168, 40, 1);
IPAddress subnet(255, 255, 255, 0);

// WebServer設置
WebServer server(80);

// 感測器數據
int currentDistanceLeft = -1;
int currentDistanceRight = -1;
int currentDistanceUnder = -1;
int currentDistanceFront = -1;
int currentDistanceBack = -1;

// 感測器數據緩衝區
unsigned char buffer_RTTLeft[4] = {0};
unsigned char buffer_RTTRight[4] = {0};

// OLED刷新計時
unsigned long lastRefreshTime = 0;

// 函數宣告
bool readSensorData(HardwareSerial &sensor, unsigned char *buffer);
bool validateChecksum(unsigned char *buffer);
int extractDistance(unsigned char *buffer);
void updateDisplay();
void clearSerialBuffer(HardwareSerial &sensor);
void debugRawData(HardwareSerial &sensor, const char *sensorName);

// HTML網頁內容（優化後的設計）
const char* index_html = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Underwater Distance Sensors</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: 'Roboto', sans-serif;
      background: linear-gradient(135deg, #1e1e2f, #2e2e4f);
      color: #ffffff;
      text-align: center;
      margin: 0;
      padding: 20px;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      justify-content: center;
    }
    h2 {
      font-size: 2.5em;
      margin-bottom: 20px;
      text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.5);
    }
    .sensor-container {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      gap: 20px;
    }
    .sensor {
      background: rgba(255, 255, 255, 0.1);
      border-radius: 15px;
      padding: 20px;
      width: 200px;
      box-shadow: 0 4px 15px rgba(0, 0, 0, 0.3);
      transition: transform 0.3s ease, box-shadow 0.3s ease;
    }
    .sensor:hover {
      transform: translateY(-5px);
      box-shadow: 0 6px 20px rgba(0, 0, 0, 0.5);
    }
    .sensor h3 {
      font-size: 1.2em;
      margin-bottom: 10px;
      color: #a1c4fd;
    }
    .distance {
      font-size: 1.8em;
      color: #c3e88d;
      opacity: 0;
      animation: fadeIn 0.5s forwards;
    }
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(10px); }
      to { opacity: 1; transform: translateY(0); }
    }
  </style>
</head>
<body>
  <h2>Underwater Distance Sensors</h2>
  <div class="sensor-container">
    <div class="sensor">
      <h3>Left Sensor</h3>
      <p class="distance" id="left">N/A</p>
    </div>
    <div class="sensor">
      <h3>Right Sensor</h3>
      <p class="distance" id="right">N/A</p>
    </div>
    <div class="sensor">
      <h3>Under Sensor</h3>
      <p class="distance" id="under">N/A</p>
    </div>
    <div class="sensor">
      <h3>Front Sensor</h3>
      <p class="distance" id="front">N/A</p>
    </div>
    <div class="sensor">
      <h3>Back Sensor</h3>
      <p class="distance" id="back">N/A</p>
    </div>
  </div>
  <script>
    function updateSensorData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('left').innerText = data.left !== -1 ? data.left + ' cm' : 'N/A';
          document.getElementById('right').innerText = data.right !== -1 ? data.right + ' cm' : 'N/A';
          document.getElementById('under').innerText = data.under !== -1 ? data.under + ' cm' : 'N/A';
          document.getElementById('front').innerText = data.front !== -1 ? data.front + ' cm' : 'N/A';
          document.getElementById('back').innerText = data.back !== -1 ? data.back + ' cm' : 'N/A';
        })
        .catch(error => console.error('Error:', error));
    }
    setInterval(updateSensorData, 1000); // 每1秒更新數據
    updateSensorData(); // 初次加載時更新
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleData() {
  StaticJsonDocument<200> doc;
  doc["left"] = currentDistanceLeft;
  doc["right"] = currentDistanceRight;
  doc["under"] = currentDistanceUnder;
  doc["front"] = currentDistanceFront;
  doc["back"] = currentDistanceBack;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleUpdate() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (!error) {
      if (doc.containsKey("under")) {
        currentDistanceUnder = doc["under"];
      }
      if (doc.containsKey("front")) {
        currentDistanceFront = doc["front"];
      }
      if (doc.containsKey("back")) {
        currentDistanceBack = doc["back"];
      }
      server.send(200, "text/plain", "Data updated");
    } else {
      server.send(400, "text/plain", "Invalid JSON");
    }
  } else {
    server.send(400, "text/plain", "No data received");
  }
}

void setup() {
  Serial.begin(BAUD_RATE);
  SerialSensorLeft.begin(BAUD_RATE, SERIAL_8N1, RXD2, TXD2);
  SerialSensorRight.begin(BAUD_RATE, SERIAL_8N1, RXD1, TXD1);

  // 初始化OLED顯示屏
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 initialization failed");
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Initializing..."));
  display.display();

  // 設置Wi-Fi為Access Point模式
  Serial.println("Setting AP (Access Point)...");
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password, 1); // 設置通道為1，減少干擾

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // 設置WebServer路由
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/update", HTTP_POST, handleUpdate);

  // 啟動WebServer
  server.begin();
  Serial.println("WebServer started");

  delay(1000); // 確保感測器啟動完成（啟動時間≤500ms）
}

void loop() {
  server.handleClient(); // 處理WebServer請求

  // 處理Left感測器
  SerialSensorLeft.write(COM);
  if (readSensorData(SerialSensorLeft, buffer_RTTLeft)) {
    if (validateChecksum(buffer_RTTLeft)) {
      int newDistance = extractDistance(buffer_RTTLeft);
      if (newDistance >= 0 && newDistance <= MAX_DISTANCE_CM) {
        currentDistanceLeft = newDistance;
      } else {
        currentDistanceLeft = -1; // 重置為無效值
      }
    } else {
      Serial.println("Left: Checksum error");
      debugRawData(SerialSensorLeft, "Left");
      currentDistanceLeft = -1; // 重置為無效值
    }
  } else {
    Serial.println("Left: No complete data received");
    debugRawData(SerialSensorLeft, "Left");
    clearSerialBuffer(SerialSensorLeft);
    currentDistanceLeft = -1; // 重置為無效值
  }

  // 處理Right感測器
  SerialSensorRight.write(COM);
  if (readSensorData(SerialSensorRight, buffer_RTTRight)) {
    if (validateChecksum(buffer_RTTRight)) {
      int newDistance = extractDistance(buffer_RTTRight);
      if (newDistance >= 0 && newDistance <= MAX_DISTANCE_CM) {
        currentDistanceRight = newDistance;
      } else {
        currentDistanceRight = -1; // 重置為無效值
      }
    } else {
      Serial.println("Right: Checksum error");
      debugRawData(SerialSensorRight, "Right");
      currentDistanceRight = -1; // 重置為無效值
    }
  } else {
    Serial.println("Right: No complete data received");
    debugRawData(SerialSensorRight, "Right");
    clearSerialBuffer(SerialSensorRight);
    currentDistanceRight = -1; // 重置為無效值
  }

  // 在串口監視器中顯示所有距離
  Serial.print("Left: ");
  Serial.print(currentDistanceLeft);
  Serial.print(" cm, Right: ");
  Serial.print(currentDistanceRight);
  Serial.print(" cm, Under: ");
  Serial.print(currentDistanceUnder);
  Serial.print(" cm, Front: ");
  Serial.print(currentDistanceFront);
  Serial.print(" cm, Back: ");
  Serial.print(currentDistanceBack);
  Serial.println(" cm");

  // 每1秒刷新OLED顯示屏
  if (millis() - lastRefreshTime >= OLED_REFRESH_MS) {
    updateDisplay();
    lastRefreshTime = millis();
  }

  // 非阻塞延時，確保感測器響應時間（14ms）
  unsigned long start = millis();
  while (millis() - start < 100) {
    // 可在此添加其他任務
  }
}

// 從感測器讀取數據
bool readSensorData(HardwareSerial &sensor, unsigned char *buffer) {
  unsigned long startTime = millis();
  while (sensor.available() < 4 && millis() - startTime < TIMEOUT_MS) {
    // 等待數據或超時
  }

  if (sensor.available() >= 4) {
    if (sensor.read() == 0xFF) {
      buffer[0] = 0xFF;
      for (int i = 1; i < 4; i++) {
        buffer[i] = sensor.read();
      }
      return true;
    } else {
      return false; // 第一個字節不是0xFF
    }
  }
  return false; // 數據不足
}

// 驗證校驗和
bool validateChecksum(unsigned char *buffer) {
  uint8_t cs = buffer[0] + buffer[1] + buffer[2];
  return buffer[3] == cs;
}

// 提取距離值（單位：厘米）
int extractDistance(unsigned char *buffer) {
  int distance_mm = (buffer[1] << 8) + buffer[2]; // 原始距離（毫米）
  return distance_mm / 10; // 轉換為厘米
}

// 更新OLED顯示屏，顯示所有感測器的距離
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);

  // 顯示Left
  display.setCursor(0, 0);
  display.print("Left: ");
  if (currentDistanceLeft == -1) {
    display.print("N/A");
  } else {
    display.print(currentDistanceLeft);
    display.print(" cm");
  }

  // 顯示Right
  display.setCursor(0, 10);
  display.print("Right: ");
  if (currentDistanceRight == -1) {
    display.print("N/A");
  } else {
    display.print(currentDistanceRight);
    display.print(" cm");
  }

  // 顯示Under
  display.setCursor(0, 20);
  display.print("Under: ");
  if (currentDistanceUnder == -1) {
    display.print("N/A");
  } else {
    display.print(currentDistanceUnder);
    display.print(" cm");
  }

  // 顯示Front
  display.setCursor(0, 30);
  display.print("Front: ");
  if (currentDistanceFront == -1) {
    display.print("N/A");
  } else {
    display.print(currentDistanceFront);
    display.print(" cm");
  }

  // 顯示Back
  display.setCursor(0, 40);
  display.print("Back: ");
  if (currentDistanceBack == -1) {
    display.print("N/A");
  } else {
    display.print(currentDistanceBack);
    display.print(" cm");
  }

  display.display();
}

// 清空串口緩衝區，防止數據堆積
void clearSerialBuffer(HardwareSerial &sensor) {
  while (sensor.available()) {
    sensor.read();
  }
}

// 調試：顯示感測器的原始數據
void debugRawData(HardwareSerial &sensor, const char *sensorName) {
  Serial.print(sensorName);
  Serial.print(" Raw Data: ");
  if (sensor.available()) {
    while (sensor.available()) {
      Serial.print(sensor.read(), HEX);
      Serial.print(" ");
    }
  } else {
    Serial.print("No data");
  }
  Serial.println();
}3