#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// 感測器通信參數
#define COM 0x55
#define BAUD_RATE 115200
#define TIMEOUT_MS 500    // 增加超時時間至500ms以確保接收數據
#define MAX_DISTANCE_CM 600  // 最大距離：600 cm

// 第一個感測器（Front，使用UART2）
#define RXD2 16  // GPIO16 (RX2)
#define TXD2 17  // GPIO17 (TX2)
HardwareSerial SerialSensorFront(2);  // UART2

// 第二個感測器（Back，使用UART1）
#define RXD1 9   // GPIO9 (RX1)
#define TXD1 10  // GPIO10 (TX1)
HardwareSerial SerialSensorBack(1);  // UART1

// Wi-Fi設置（連接到ESP32 #1的熱點）
const char* ssid = "ESP32_Webserver";
const char* password = "12345678";

// WebServer地址
const char* serverName = "http://192.168.40.1/update";

// 感測器數據
int currentDistanceFront = -1;
int currentDistanceBack = -1;

// 感測器數據緩衝區
unsigned char buffer_RTTFront[4] = {0};
unsigned char buffer_RTTBack[4] = {0};

// 發送數據計時
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // 每1秒發送一次

// 函數宣告
bool readSensorData(HardwareSerial &sensor, unsigned char *buffer);
bool validateChecksum(unsigned char *buffer);
int extractDistance(unsigned char *buffer);
void clearSerialBuffer(HardwareSerial &sensor);
void debugRawData(HardwareSerial &sensor, const char *sensorName);

void setup() {
  Serial.begin(BAUD_RATE);
  SerialSensorFront.begin(BAUD_RATE, SERIAL_8N1, RXD2, TXD2);
  SerialSensorBack.begin(BAUD_RATE, SERIAL_8N1, RXD1, TXD1);

  // 連接到Wi-Fi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  delay(1000); // 確保感測器啟動完成（啟動時間≤500ms）
}

void loop() {
  // 處理Front感測器
  SerialSensorFront.write(COM);
  if (readSensorData(SerialSensorFront, buffer_RTTFront)) {
    if (validateChecksum(buffer_RTTFront)) {
      int newDistance = extractDistance(buffer_RTTFront);
      if (newDistance >= 0 && newDistance <= MAX_DISTANCE_CM) {
        currentDistanceFront = newDistance;
      } else {
        currentDistanceFront = -1; // 重置為無效值
      }
    } else {
      currentDistanceFront = -1; // 重置為無效值
    }
  } else {
    clearSerialBuffer(SerialSensorFront);
    currentDistanceFront = -1; // 重置為無效值
  }

  // 處理Back感測器
  SerialSensorBack.write(COM);
  if (readSensorData(SerialSensorBack, buffer_RTTBack)) {
    if (validateChecksum(buffer_RTTBack)) {
      int newDistance = extractDistance(buffer_RTTBack);
      if (newDistance >= 0 && newDistance <= MAX_DISTANCE_CM) {
        currentDistanceBack = newDistance;
      } else {
        currentDistanceBack = -1; // 重置為無效值
      }
    } else {
      currentDistanceBack = -1; // 重置為無效值
    }
  } else {
    clearSerialBuffer(SerialSensorBack);
    currentDistanceBack = -1; // 重置為無效值
  }

  // 在串口監視器中顯示當前距離
  Serial.print("Front: ");
  Serial.print(currentDistanceFront);
  Serial.print(" cm, Back: ");
  Serial.print(currentDistanceBack);
  Serial.println(" cm");

  // 每1秒發送數據到ESP32 #1
  if (millis() - lastSendTime >= sendInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverName);
      http.addHeader("Content-Type", "application/json");

      StaticJsonDocument<200> doc;
      doc["front"] = currentDistanceFront;
      doc["back"] = currentDistanceBack;
      String requestBody;
      serializeJson(doc, requestBody);

      int httpResponseCode = http.POST(requestBody);
      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
      WiFi.reconnect();
    }
    lastSendTime = millis();
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

// 清空串口緩衝區，防止數據堆積
void clearSerialBuffer(HardwareSerial &sensor) {
  while (sensor.available()) {
    sensor.read();
  }
}