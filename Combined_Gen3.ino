#include <Servo.h>
#include <Adafruit_BNO08x.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>  // 新增: 計算角度

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

// ====================== BNO085 ======================
Adafruit_BNO08x bno;
sh2_SensorValue_t sv;
float ax=0, ay=0, az=0;
float quatW=1, quatX=0, quatY=0, quatZ=0;
float yaw=0, pitch=0, roll=0;

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

  TCCR1A = 0b00000001;
  TCCR1B = 0b00000001;

  stopAll();

  Serial1.begin(115200); Serial2.begin(115200); Serial3.begin(115200);
  delay(1000);

  if (bno.begin_I2C()) {
    bno.enableReport(SH2_ACCELEROMETER, 100000);      // 只保留加速度計
    bno.enableReport(SH2_ROTATION_VECTOR, 100000);    // 新增: 四元數報告
  }

  // 繼電器初始關閉
  pinMode(relay1Pin, OUTPUT); digitalWrite(relay1Pin, HIGH);
  pinMode(relay2Pin, OUTPUT); digitalWrite(relay2Pin, HIGH);
  pinMode(relay3Pin, OUTPUT); digitalWrite(relay3Pin, HIGH);

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

  // 超聲波
  readSensor(Serial1, buf1, 0);
  readSensor(Serial2, buf2, 1);
  readSensor(Serial3, buf3, 2);

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

    // 繼電器
    else if (s.startsWith("R") || s.startsWith("r")) {
      int p1 = s.indexOf(' ', 1);
      if (p1 < 0) p1 = s.length();
      int relay = s.substring(1, p1).toInt();
      String state = (p1 < s.length()) ? s.substring(p1 + 1) : "OFF";
      state.trim();
      if (relay >= 1 && relay <= 3) controlRelay(relay, state == "ON");
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
  Serial.print(quatX, 2);  // i 分量
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

void readSensor(HardwareSerial &p, unsigned char* b, int idx) {
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

void controlRelay(int relay, bool on) {
  int pin;
  switch (relay) {
    case 1: pin = relay1Pin; break;
    case 2: pin = relay2Pin; break;
    case 3: pin = relay3Pin; break;
    default: return;
  }
  digitalWrite(pin, on ? LOW : HIGH);
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
