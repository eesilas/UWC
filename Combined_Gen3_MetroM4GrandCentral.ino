#include <Servo.h>
#include <Adafruit_BNO08x.h>
#include <Wire.h>  // 标准 I2C for BNO (pins 20/21, SERCOM3)
#include <Adafruit_NeoPixel.h>
#include <math.h> // 新增: 計算角度
#include "wiring_private.h"  // 新增: for pinPeripheral

// ====================== 開關 ======================
const int switch1Pin = 50;
const int switch2Pin = 51;
int state1 = HIGH, state2 = HIGH;

// ====================== 推進器 ======================
byte servoPins[8] = {2, 3, 4, 5, 6, 7, 8, 9};
Servo servos[8];

// ====================== 刷電機 ======================
#define IN1_M1 22
#define IN2_M1 23
#define IN1_M2 24
#define IN2_M2 25
#define IN1_M3 26
#define IN2_M3 27
#define IN1_M4 28
#define IN2_M4 29
#define PWM_M1 10
#define PWM_M2 11
#define PWM_M3 12
#define PWM_M4 13

// ====================== 超聲波 ======================
float sensorDist[3] = {-1, -1, -1};
const byte triggerCmd = 0x55;
unsigned char buf1[4] = {0}, buf2[4] = {0}, buf3[4] = {0};

// 定义自定义 UART 对象（Mega 兼容引脚，正确 SERCOM/PAD）
Uart sensorSerial1(&sercom4, 19, 18, SERCOM_RX_PAD_1, UART_TX_PAD_0);  // 原 Serial1: pins 18 TX / 19 RX, SERCOM4
Uart sensorSerial2(&sercom1, 17, 16, SERCOM_RX_PAD_3, UART_TX_PAD_2);  // 原 Serial2: pins 16 TX / 17 RX, SERCOM1
Uart sensorSerial3(&sercom5, 15, 14, SERCOM_RX_PAD_1, UART_TX_PAD_0);  // 原 Serial3: pins 14 TX / 15 RX, SERCOM5

// SERCOM1 中断处理 (for sensorSerial2) - #ifndef 避免冲突
#ifndef SERCOM1_0_Handler
void SERCOM1_0_Handler() { sensorSerial2.IrqHandler(); }
#endif
#ifndef SERCOM1_1_Handler
void SERCOM1_1_Handler() { sensorSerial2.IrqHandler(); }
#endif
#ifndef SERCOM1_2_Handler
void SERCOM1_2_Handler() { sensorSerial2.IrqHandler(); }
#endif
#ifndef SERCOM1_3_Handler
void SERCOM1_3_Handler() { sensorSerial2.IrqHandler(); }
#endif

// SERCOM4 中断处理 (for sensorSerial1)
#ifndef SERCOM4_0_Handler
void SERCOM4_0_Handler() { sensorSerial1.IrqHandler(); }
#endif
#ifndef SERCOM4_1_Handler
void SERCOM4_1_Handler() { sensorSerial1.IrqHandler(); }
#endif
#ifndef SERCOM4_2_Handler
void SERCOM4_2_Handler() { sensorSerial1.IrqHandler(); }
#endif
#ifndef SERCOM4_3_Handler
void SERCOM4_3_Handler() { sensorSerial1.IrqHandler(); }
#endif

// SERCOM5 中断处理 (for sensorSerial3)
#ifndef SERCOM5_0_Handler
void SERCOM5_0_Handler() { sensorSerial3.IrqHandler(); }
#endif
#ifndef SERCOM5_1_Handler
void SERCOM5_1_Handler() { sensorSerial3.IrqHandler(); }
#endif
#ifndef SERCOM5_2_Handler
void SERCOM5_2_Handler() { sensorSerial3.IrqHandler(); }
#endif
#ifndef SERCOM5_3_Handler
void SERCOM5_3_Handler() { sensorSerial3.IrqHandler(); }
#endif

// ====================== BNO085 ====================== (标准 Wire, pins 20/21)
Adafruit_BNO08x bno;
sh2_SensorValue_t sv;
float ax=0, ay=0, az=0;
float quatW=1, quatX=0, quatY=0, quatZ=0;
float yaw=0, pitch=0, roll=0;

// ====================== 繼電器狀態（新增: 用於 toggle 邏輯） ======================
bool relayStates[3] = {false, false, false};  // false = OFF

// ====================== 繼電器 + NeoPixel ======================
const int relay1Pin = 30;
const int relay2Pin = 31;
const int relay3Pin = 32;
#define NEO_PIN 33
#define NEO_NUM 16
Adafruit_NeoPixel neo = Adafruit_NeoPixel(NEO_NUM, NEO_PIN, NEO_RGB + NEO_KHZ800);

void setup() {
  Serial.begin(9600);
  pinMode(switch1Pin, INPUT_PULLUP);
  pinMode(switch2Pin, INPUT_PULLUP);

  // 推進器
  for (int i = 0; i < 8; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].writeMicroseconds(1500);
  }
  delay(7000);

  // 刷電機
  pinMode(IN1_M1, OUTPUT); pinMode(IN2_M1, OUTPUT);
  pinMode(IN1_M2, OUTPUT); pinMode(IN2_M2, OUTPUT);
  pinMode(IN1_M3, OUTPUT); pinMode(IN2_M3, OUTPUT);
  pinMode(IN1_M4, OUTPUT); pinMode(IN2_M4, OUTPUT);
  pinMode(PWM_M1, OUTPUT); pinMode(PWM_M2, OUTPUT);
  pinMode(PWM_M3, OUTPUT); pinMode(PWM_M4, OUTPUT);
  stopAll();

  // 初始化自定义串口 + 引脚复用 (PIO_SERCOM / ALT 根据 PAD)
  sensorSerial1.begin(115200);
  pinPeripheral(18, PIO_SERCOM);
  pinPeripheral(19, PIO_SERCOM);
  sensorSerial2.begin(115200);
  pinPeripheral(16, PIO_SERCOM_ALT);
  pinPeripheral(17, PIO_SERCOM_ALT);
  sensorSerial3.begin(115200);
  pinPeripheral(14, PIO_SERCOM);
  pinPeripheral(15, PIO_SERCOM);
  delay(1000);

  // BNO 使用标准 Wire (SERCOM3, pins 20/21)
  if (!bno.begin_I2C()) {
    Serial.println("BNO08x not found!");
    while (1) delay(10);
  }
  bno.enableReport(SH2_ACCELEROMETER, 100000);
  bno.enableReport(SH2_ROTATION_VECTOR, 100000);

  // 繼電器初始關閉 (新增: active-high 邏輯，OFF 時 LOW? 等待，根據交換: OFF 時 LOW)
  pinMode(relay1Pin, OUTPUT); digitalWrite(relay1Pin, LOW);  // 初始 OFF (LOW)
  pinMode(relay2Pin, OUTPUT); digitalWrite(relay2Pin, LOW);
  pinMode(relay3Pin, OUTPUT); digitalWrite(relay3Pin, LOW);

  // NeoPixel 初始關閉
  neo.begin();
  neo.setBrightness(50);
  neo.clear();
  neo.show();
}

void loop() {
  // 開關去抖
  int r1 = digitalRead(switch1Pin);
  int r2 = digitalRead(switch2Pin);
  if (r1 != state1) { delay(50); if (r1 == digitalRead(switch1Pin)) state1 = r1; }
  if (r2 != state2) { delay(50); if (r2 == digitalRead(switch2Pin)) state2 = r2; }

  // 超聲波 (使用自定义串口)
  readSensor(sensorSerial1, buf1, 0);
  readSensor(sensorSerial2, buf2, 1);
  readSensor(sensorSerial3, buf3, 2);

  // IMU (只處理加速度計 + 四元數)
  while (bno.getSensorEvent(&sv)) {
    switch (sv.sensorId) {
      case SH2_ACCELEROMETER:
        ax = sv.un.accelerometer.x;
        ay = sv.un.accelerometer.y;
        az = sv.un.accelerometer.z;
        break;
      case SH2_ROTATION_VECTOR:
        quatW = sv.un.rotationVector.real;
        quatX = sv.un.rotationVector.i;
        quatY = sv.un.rotationVector.j;
        quatZ = sv.un.rotationVector.k;
        // 從四元數計算 Euler 角度 (yaw, pitch, roll)
        yaw = atan2(2.0 * (quatX * quatZ + quatW * quatY), (quatW * quatW - quatX * quatX - quatY * quatY + quatZ * quatZ)) * 180.0 / PI;
        pitch = asin(2.0 * (quatX * quatY - quatW * quatZ)) * 180.0 / PI;
        roll = atan2(2.0 * (quatY * quatZ + quatW * quatX), (quatW * quatW + quatZ * quatZ - quatX * quatX - quatY * quatY)) * 180.0 / PI;
        break;
    }
  }

  // 命令處理
  while (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    // 推進器
    if (s.indexOf(':') > 0) {
      int c = s.indexOf(':');
      int n = s.substring(0, c).toInt();
      int v = s.substring(c + 1).toInt();
      if (n >= 1 && n <= 8 && v >= 1100 && v <= 1900)
        servos[n - 1].writeMicroseconds(v);
    }
    // 刷電機
    else if (s.startsWith("M") || s.startsWith("m")) {
      int p1 = s.indexOf(' ', 1);
      if (p1 < 0) continue;
      int motor = s.substring(1, p1).toInt();
      int p2 = s.indexOf(' ', p1 + 1);
      String dir;
      int spd = 0;
      if (p2 > 0) { dir = s.substring(p1 + 1, p2); spd = s.substring(p2 + 1).toInt(); }
      else dir = s.substring(p1 + 1);
      dir.trim();
      if (motor >= 1 && motor <= 4) controlBrush(motor, dir, spd);
    }
    // 繼電器 (修改: 支持 "open" 命令觸發 toggle)
    else if (s.startsWith("R") || s.startsWith("r")) {
      int p1 = s.indexOf(' ', 1);
      if (p1 < 0) p1 = s.length();
      int relay = s.substring(1, p1).toInt();
      String state = (p1 < s.length()) ? s.substring(p1 + 1) : "open";  // 默認 "open"
      state.trim();
      state.toLowerCase();  // 忽略大小寫
      bool newOn;
      if (state == "open") {
        // Toggle 邏輯
        relayStates[relay - 1] = !relayStates[relay - 1];
        newOn = relayStates[relay - 1];
      } else if (state == "on") {
        newOn = true;
        relayStates[relay - 1] = true;
      } else if (state == "off") {
        newOn = false;
        relayStates[relay - 1] = false;
      } else {
        continue;  // 無效命令
      }
      if (relay >= 1 && relay <= 3) controlRelay(relay, newOn);
    }
    // NeoPixel
    else if (s.startsWith("N") || s.startsWith("n")) {
      int p1 = s.indexOf(' ', 1);
      String state = (p1 > 0) ? s.substring(p1 + 1) : "OFF";
      state.trim();
      controlNeoPixel(state == "ON");
    }
  }

  // 輸出（新增四元數 + 角度）
  Serial.print("开关1:");
  Serial.print(!state1);
  Serial.print(", 开关2:");
  Serial.print(!state2);
  Serial.print(" | S1:");
  Serial.print(sensorDist[0], 0);
  Serial.print(",S2:");
  Serial.print(sensorDist[1], 0);
  Serial.print(",S3:");
  Serial.print(sensorDist[2], 0);
  Serial.print(" | AccelX:");
  Serial.print(ax, 2);
  Serial.print(",AccelY:");
  Serial.print(ay, 2);
  Serial.print(",AccelZ:");
  Serial.print(az, 2);
  Serial.print(" | QuatW:");
  Serial.print(quatW, 2);
  Serial.print(",QuatX:");
  Serial.print(quatX, 2);
  Serial.print(",QuatY:");
  Serial.print(quatY, 2);
  Serial.print(",QuatZ:");
  Serial.print(quatZ, 2);
  Serial.print(" | Yaw:");
  Serial.print(yaw, 2);
  Serial.print(",Pitch:");
  Serial.print(pitch, 2);
  Serial.print(",Roll:");
  Serial.println(roll, 2);
  delay(100);
}

void readSensor(Uart &p, unsigned char* b, int idx) {
  p.write(triggerCmd);
  delay(20);
  if (p.available() >= 4 && p.read() == 0xFF) {
    b[0] = 0xFF; b[1] = p.read(); b[2] = p.read(); b[3] = p.read();
    if (b[3] == ((b[0] + b[1] + b[2]) & 0xFF)) {
      int mm = (b[1] << 8) + b[2];
      sensorDist[idx] = mm / 10.0;
      if (sensorDist[idx] < 5 || sensorDist[idx] > 600) sensorDist[idx] = -1;
      return;
    }
  }
  sensorDist[idx] = -1;
}

void controlBrush(int m, String d, int s) {
  int i1, i2, p;
  switch (m) {
    case 1: i1 = IN1_M1; i2 = IN2_M1; p = PWM_M1; break;
    case 2: i1 = IN1_M2; i2 = IN2_M2; p = PWM_M2; break;
    case 3: i1 = IN1_M3; i2 = IN2_M3; p = PWM_M3; break;
    case 4: i1 = IN1_M4; i2 = IN2_M4; p = PWM_M4; break;
    default: return;
  }
  if (d == "CW") { digitalWrite(i1, HIGH); digitalWrite(i2, LOW); }
  else if (d == "CCW") { digitalWrite(i1, LOW); digitalWrite(i2, HIGH); }
  else { digitalWrite(i1, LOW); digitalWrite(i2, LOW); s = 0; }
  analogWrite(p, s);
}

// 修改: 交換 HIGH/LOW 邏輯 (ON 時 HIGH, OFF 時 LOW)
void controlRelay(int relay, bool on) {
  int pin;
  switch (relay) {
    case 1: pin = relay1Pin; break;
    case 2: pin = relay2Pin; break;
    case 3: pin = relay3Pin; break;
    default: return;
  }
  digitalWrite(pin, on ? HIGH : LOW);  // 交換: ON 時 HIGH
}

void controlNeoPixel(bool on) {
  if (on) {
    for (int i = 0; i < NEO_NUM; i++) {
      neo.setPixelColor(i, neo.Color(255, 255, 255));
    }
  } else {
    neo.clear();
  }
  neo.show();
}

void stopAll() {
  analogWrite(PWM_M1, 0); analogWrite(PWM_M2, 0);
  analogWrite(PWM_M3, 0); analogWrite(PWM_M4, 0);
  digitalWrite(IN1_M1, LOW); digitalWrite(IN2_M1, LOW);
  digitalWrite(IN1_M2, LOW); digitalWrite(IN2_M2, LOW);
  digitalWrite(IN1_M3, LOW); digitalWrite(IN2_M3, LOW);
  digitalWrite(IN1_M4, LOW); digitalWrite(IN2_M4, LOW);
}
