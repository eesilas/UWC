// Adafruit Metro M4 Grand Central - 最终完整版本
// 功能：继电器 + IMU + 激光开关(低电平触发) + 漏水 + 电机3/4 + 8推进器控制及显示

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Servo.h>

// 继电器引脚
const int relayPin1 = 30;
const int relayPin2 = 31;
const int relayPin3 = 32;

// 激光开关引脚（低电平触发）
const int laserPin1 = 45;
const int laserPin2 = 52;
const int laserPin3 = 53;

// 漏水传感器引脚（高电平触发）
const int leakPin = 44;

// 电机3 & 电机4 引脚
#define IN1_MOTOR3 22
#define IN2_MOTOR3 23
#define ENA_MOTOR3 10
#define IN1_MOTOR4 24
#define IN2_MOTOR4 25
#define ENB_MOTOR4 11

// 8个推进器引脚及对象
byte thrusterPins[8] = {2, 3, 4, 5, 6, 7, 8, 9};
Servo thrusters[8];
int thrusterPWM[8] = {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};

// 电机3/4状态
String motor3_status = "STOP";
int motor3_speed = 0;
String motor4_status = "STOP";
int motor4_speed = 0;

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(relayPin1, OUTPUT); digitalWrite(relayPin1, LOW);
  pinMode(relayPin2, OUTPUT); digitalWrite(relayPin2, LOW);
  pinMode(relayPin3, OUTPUT); digitalWrite(relayPin3, LOW);

  pinMode(laserPin1, INPUT);
  pinMode(laserPin2, INPUT);
  pinMode(laserPin3, INPUT);
  pinMode(leakPin, INPUT);

  pinMode(IN1_MOTOR3, OUTPUT); pinMode(IN2_MOTOR3, OUTPUT); pinMode(ENA_MOTOR3, OUTPUT);
  pinMode(IN1_MOTOR4, OUTPUT); pinMode(IN2_MOTOR4, OUTPUT); pinMode(ENB_MOTOR4, OUTPUT);
  stopAllMotors();

  for (int i = 0; i < 8; i++) {
    thrusters[i].attach(thrusterPins[i]);
    thrusters[i].writeMicroseconds(1500);
  }
  delay(7000);  // ESC初始化延时

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050!");
    while (1);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("Grand Central 系统就绪");
  Serial.println("命令：relay1:1、motor3:cw:150、T1:1600 等");
}

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    processCommand(cmd);
  }

  bool laser1 = (digitalRead(laserPin1) == LOW);
  bool laser2 = (digitalRead(laserPin2) == LOW);
  bool laser3 = (digitalRead(laserPin3) == LOW);
  bool leak = (digitalRead(leakPin) == HIGH);
  bool r1 = digitalRead(relayPin1);
  bool r2 = digitalRead(relayPin2);
  bool r3 = digitalRead(relayPin3);

  // 第一行
  Serial.print("Laser1: "); Serial.print(laser1 ? "Trig" : "No");
  Serial.print(" | Laser2: "); Serial.print(laser2 ? "Trig" : "No");
  Serial.print(" | Laser3: "); Serial.print(laser3 ? "Trig" : "No");
  Serial.print(" | Relay1: "); Serial.print(r1 ? "ON" : "OFF");
  Serial.print(" | Relay2: "); Serial.print(r2 ? "ON" : "OFF");
  Serial.print(" | Relay3: "); Serial.print(r3 ? "ON" : "OFF");
  Serial.print(" | Leak: "); Serial.print(leak ? "Detected" : "Normal");
  Serial.println();

  // 第二行 IMU
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  Serial.print("IMU Acc: X="); Serial.print(a.acceleration.x, 1);
  Serial.print(" Y="); Serial.print(a.acceleration.y, 1);
  Serial.print(" Z="); Serial.print(a.acceleration.z, 1);
  Serial.print(" g | Gyro: X="); Serial.print(g.gyro.x, 1);
  Serial.print(" Y="); Serial.print(g.gyro.y, 1);
  Serial.print(" Z="); Serial.print(g.gyro.z, 1);
  Serial.println(" deg/s");

  // 第三行 电机3/4
  Serial.print("Motor3: ");
  if (motor3_status == "STOP") Serial.print("STOP");
  else { Serial.print(motor3_status); Serial.print(" "); Serial.print(motor3_speed); }
  Serial.print(" | Motor4: ");
  if (motor4_status == "STOP") Serial.print("STOP");
  else { Serial.print(motor4_status); Serial.print(" "); Serial.print(motor4_speed); }
  Serial.println();

  // 第四行 8个推进器PWM
  Serial.print("Thrusters: ");
  for (int i = 0; i < 8; i++) {
    Serial.print("T"); Serial.print(i+1); Serial.print(":"); Serial.print(thrusterPWM[i]);
    if (i < 7) Serial.print(" ");
  }
  Serial.println();

  Serial.println();  // 分隔
  delay(100);
}

void processCommand(String cmd) {
  if (cmd.startsWith("relay")) handleRelay(cmd);
  else if (cmd.startsWith("motor3:") || cmd.startsWith("motor4:")) handleMotor(cmd);
  else if (cmd.startsWith("t") && cmd.indexOf(':') != -1) handleThruster(cmd);
  else if (cmd.length() > 0) Serial.println(">>> Unknown command");
}

void handleRelay(String cmd) {
  int num = cmd[5] - '0';
  int pin = (num == 1) ? relayPin1 : (num == 2) ? relayPin2 : relayPin3;
  if (cmd.endsWith(":1")) { digitalWrite(pin, HIGH); Serial.print(">>> Relay "); Serial.print(num); Serial.println(" ON"); }
  else if (cmd.endsWith(":0")) { digitalWrite(pin, LOW); Serial.print(">>> Relay "); Serial.print(num); Serial.println(" OFF"); }
}

void handleMotor(String cmd) {
  int motorNum = cmd.startsWith("motor3:") ? 3 : 4;
  String &status = (motorNum == 3) ? motor3_status : motor4_status;
  int &speed_var = (motorNum == 3) ? motor3_speed : motor4_speed;

  String rest = cmd.substring(cmd.indexOf(':') + 1);
  rest.trim();

  if (rest == "stop") {
    controlSingleMotor(motorNum, "STOP", 0);
    status = "STOP"; speed_var = 0;
    Serial.print(">>> Motor "); Serial.print(motorNum); Serial.println(" STOP");
    return;
  }

  int colonPos = rest.indexOf(':');
  if (colonPos == -1) { Serial.println(">>> Invalid motor command"); return; }

  String dir = rest.substring(0, colonPos); dir.trim();
  int speed = rest.substring(colonPos + 1).toInt();

  if (speed < 0 || speed > 255) { Serial.println(">>> Speed 0-255"); return; }

  if (dir == "cw" || dir == "ccw") {
    String dirU = (dir == "cw") ? "CW" : "CCW";
    controlSingleMotor(motorNum, dirU, speed);
    status = dirU; speed_var = speed;
    Serial.print(">>> Motor "); Serial.print(motorNum); Serial.print(" "); Serial.print(dirU);
    Serial.print(" speed: "); Serial.println(speed);
  } else Serial.println(">>> Direction cw or ccw");
}

void controlSingleMotor(int motor, String dir, int speed) {
  if (motor == 3) {
    if (dir == "CW") { digitalWrite(IN1_MOTOR3, HIGH); digitalWrite(IN2_MOTOR3, LOW); analogWrite(ENA_MOTOR3, speed); }
    else if (dir == "CCW") { digitalWrite(IN1_MOTOR3, LOW); digitalWrite(IN2_MOTOR3, HIGH); analogWrite(ENA_MOTOR3, speed); }
    else if (dir == "STOP") { digitalWrite(IN1_MOTOR3, LOW); digitalWrite(IN2_MOTOR3, LOW); analogWrite(ENA_MOTOR3, 0); }
  } else if (motor == 4) {
    if (dir == "CW") { digitalWrite(IN1_MOTOR4, HIGH); digitalWrite(IN2_MOTOR4, LOW); analogWrite(ENB_MOTOR4, speed); }
    else if (dir == "CCW") { digitalWrite(IN1_MOTOR4, LOW); digitalWrite(IN2_MOTOR4, HIGH); analogWrite(ENB_MOTOR4, speed); }
    else if (dir == "STOP") { digitalWrite(IN1_MOTOR4, LOW); digitalWrite(IN2_MOTOR4, LOW); analogWrite(ENB_MOTOR4, 0); }
  }
}

void stopAllMotors() {
  controlSingleMotor(3, "STOP", 0);
  controlSingleMotor(4, "STOP", 0);
  motor3_status = "STOP"; motor3_speed = 0;
  motor4_status = "STOP"; motor4_speed = 0;
}

void handleThruster(String cmd) {
  cmd.toUpperCase();
  int colonIndex = cmd.indexOf(':');
  if (colonIndex == -1) { Serial.println(">>> Invalid thruster format"); return; }

  String numStr = cmd.substring(1, colonIndex);
  int pwmVal = cmd.substring(colonIndex + 1).toInt();
  int num = numStr.toInt();

  if (num >= 1 && num <= 8 && pwmVal >= 1100 && pwmVal <= 1900) {
    int idx = num - 1;
    thrusters[idx].writeMicroseconds(pwmVal);
    thrusterPWM[idx] = pwmVal;
    Serial.print(">>> Thruster "); Serial.print(num); Serial.print(" set to "); Serial.println(pwmVal);
  } else Serial.println(">>> Invalid thruster or PWM (1100-1900)");
}
