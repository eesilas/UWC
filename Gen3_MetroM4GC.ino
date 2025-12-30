// Adafruit Metro M4 Grand Central - 最终完整版本
// 功能：3继电器 + IMU + 3激光开关（低电平触发） + 漏水 + 电机3/4控制 + 8推进器控制及状态显示

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
const int laserPin2 = 40;
const int laserPin3 = 53;

// 漏水传感器引脚（高电平触发）
const int leakPin = 44;

// 刷板电机3 & 电机4 引脚
#define IN1_MOTOR3 22
#define IN2_MOTOR3 23
#define ENA_MOTOR3 10
#define IN1_MOTOR4 24
#define IN2_MOTOR4 25
#define ENB_MOTOR4 11

// 8个推进器引脚及对象
byte thrusterPins[8] = {2, 3, 4, 5, 6, 7, 8, 9};  // D2 到 D9
Servo thrusters[8];
int thrusterPWM[8] = {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};  // 当前PWM值（初始停止）

// 电机3/4状态
String motor3_status = "STOP";
int motor3_speed = 0;
String motor4_status = "STOP";
int motor4_speed = 0;

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // 继电器初始化
  pinMode(relayPin1, OUTPUT); digitalWrite(relayPin1, LOW);
  pinMode(relayPin2, OUTPUT); digitalWrite(relayPin2, LOW);
  pinMode(relayPin3, OUTPUT); digitalWrite(relayPin3, LOW);

  // 输入传感器
  pinMode(laserPin1, INPUT);
  pinMode(laserPin2, INPUT);
  pinMode(laserPin3, INPUT);
  pinMode(leakPin, INPUT);

  // 电机3/4初始化
  pinMode(IN1_MOTOR3, OUTPUT); pinMode(IN2_MOTOR3, OUTPUT); pinMode(ENA_MOTOR3, OUTPUT);
  pinMode(IN1_MOTOR4, OUTPUT); pinMode(IN2_MOTOR4, OUTPUT); pinMode(ENB_MOTOR4, OUTPUT);
  stopAllMotors();

  // 推进器初始化
  for (int i = 0; i < 8; i++) {
    thrusters[i].attach(thrusterPins[i]);
    thrusters[i].writeMicroseconds(1500);
  }
  delay(3500);  // 关键：等待ESC完成初始化

  // IMU初始化
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip!");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("Metro M4 Grand Central 系统已就绪");
  Serial.println("命令格式：");
  Serial.println("  relay1:1 / relay1:0 等");
  Serial.println("  motor3:cw:150 / motor3:stop 等");
  Serial.println("  T1:1600 / T5:1300 等（推进器PWM 1100-1900）");
}

void loop() {
  // 处理串口命令
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    processCommand(command);
  }

  // 读取状态
  bool laser1 = (digitalRead(laserPin1) == LOW);   // 低电平触发
  bool laser2 = (digitalRead(laserPin2) == LOW);
  bool laser3 = (digitalRead(laserPin3) == LOW);
  bool leakDetected = (digitalRead(leakPin) == HIGH);
  bool r1 = digitalRead(relayPin1);
  bool r2 = digitalRead(relayPin2);
  bool r3 = digitalRead(relayPin3);

  // 第一行：激光 + 继电器 + 漏水
  Serial.print("Laser1: "); Serial.print(laser1 ? "Trig" : "No");
  Serial.print(" | Laser2: "); Serial.print(laser2 ? "Trig" : "No");
  Serial.print(" | Laser3: "); Serial.print(laser3 ? "Trig" : "No");
  Serial.print(" | Relay1: "); Serial.print(r1 ? "ON" : "OFF");
  Serial.print(" | Relay2: "); Serial.print(r2 ? "ON" : "OFF");
  Serial.print(" | Relay3: "); Serial.print(r3 ? "ON" : "OFF");
  Serial.print(" | Leak: "); Serial.print(leakDetected ? "Detected" : "Normal");
  Serial.println();

  // 第二行：IMU
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  Serial.print("IMU Acc: X=");
  Serial.print(a.acceleration.x, 1);
  Serial.print(" Y=");
  Serial.print(a.acceleration.y, 1);
  Serial.print(" Z=");
  Serial.print(a.acceleration.z, 1);
  Serial.print(" g | Gyro: X=");
  Serial.print(g.gyro.x, 1);
  Serial.print(" Y=");
  Serial.print(g.gyro.y, 1);
  Serial.print(" Z=");
  Serial.print(g.gyro.z, 1);
  Serial.println(" deg/s");

  // 第三行：电机3/4状态
  Serial.print("Motor3: ");
  if (motor3_status == "STOP") Serial.print("STOP");
  else { Serial.print(motor3_status); Serial.print(" "); Serial.print(motor3_speed); }
  Serial.print(" | Motor4: ");
  if (motor4_status == "STOP") Serial.print("STOP");
  else { Serial.print(motor4_status); Serial.print(" "); Serial.print(motor4_speed); }
  Serial.println();

  // 第四行：所有推进器PWM值（新增）
  Serial.print("Thrusters: ");
  for (int i = 0; i < 8; i++) {
    Serial.print("T");
    Serial.print(i + 1);
    Serial.print(":");
    Serial.print(thrusterPWM[i]);
    if (i < 7) Serial.print(" ");
  }
  Serial.println();

  Serial.println();  // 空行分隔

  delay(500);
}

// 统一命令处理
void processCommand(String cmd) {
  if (cmd.startsWith("relay")) {
    handleRelay(cmd);
  }
  else if (cmd.startsWith("motor3:") || cmd.startsWith("motor4:")) {
    handleMotor(cmd);
  }
  else if (cmd.startsWith("t") && cmd.indexOf(':') != -1) {
    handleThruster(cmd);
  }
  else if (cmd.length() > 0) {
    Serial.println(">>> Unknown command");
  }
}

// 继电器处理
void handleRelay(String cmd) {
  int num = cmd[5] - '0';
  int pin = (num == 1) ? relayPin1 : (num == 2) ? relayPin2 : relayPin3;
  if (cmd.endsWith(":1")) {
    digitalWrite(pin, HIGH);
    Serial.print(">>> Relay "); Serial.print(num); Serial.println(" ON");
  } else if (cmd.endsWith(":0")) {
    digitalWrite(pin, LOW);
    Serial.print(">>> Relay "); Serial.print(num); Serial.println(" OFF");
  }
}

// 电机3/4处理（保持完整逻辑）
void handleMotor(String cmd) {
  int motorNum = cmd.startsWith("motor3:") ? 3 : 4;
  String &status = (motorNum == 3) ? motor3_status : motor4_status;
  int &speed_var = (motorNum == 3) ? motor3_speed : motor4_speed;

  String rest = cmd.substring(cmd.indexOf(':') + 1);
  rest.trim();

  if (rest == "stop") {
    controlSingleMotor(motorNum, "STOP", 0);
    status = "STOP";
    speed_var = 0;
    Serial.print(">>> Motor "); Serial.print(motorNum); Serial.println(" STOP");
    return;
  }

  int colonPos = rest.indexOf(':');
  if (colonPos == -1) {
    Serial.println(">>> Invalid motor command");
    return;
  }

  String dir = rest.substring(0, colonPos);
  dir.trim();
  int speed = rest.substring(colonPos + 1).toInt();

  if (speed < 0 || speed > 255) {
    Serial.println(">>> Speed must be 0-255");
    return;
  }

  if (dir == "cw" || dir == "ccw") {
    String dirUpper = (dir == "cw") ? "CW" : "CCW";
    controlSingleMotor(motorNum, dirUpper, speed);
    status = dirUpper;
    speed_var = speed;
    Serial.print(">>> Motor "); Serial.print(motorNum);
    Serial.print(" "); Serial.print(dirUpper);
    Serial.print(" speed: "); Serial.println(speed);
  } else {
    Serial.println(">>> Direction must be cw or ccw");
  }
}

// 单个电机控制
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

// 停止所有电机3/4
void stopAllMotors() {
  controlSingleMotor(3, "STOP", 0);
  controlSingleMotor(4, "STOP", 0);
  motor3_status = "STOP"; motor3_speed = 0;
  motor4_status = "STOP"; motor4_speed = 0;
}

// 推进器命令处理（并更新显示数组）
void handleThruster(String cmd) {
  cmd.toUpperCase();  // 统一为大写 T
  int colonIndex = cmd.indexOf(':');
  if (colonIndex == -1) {
    Serial.println(">>> Invalid thruster format");
    return;
  }

  String numStr = cmd.substring(1, colonIndex);
  int pwmVal = cmd.substring(colonIndex + 1).toInt();
  int thrusterNum = numStr.toInt();

  if (thrusterNum >= 1 && thrusterNum <= 8 && pwmVal >= 1100 && pwmVal <= 1900) {
    int index = thrusterNum - 1;
    thrusters[index].writeMicroseconds(pwmVal);
    thrusterPWM[index] = pwmVal;  // 更新显示值
    Serial.print(">>> Thruster "); Serial.print(thrusterNum);
    Serial.print(" set to "); Serial.println(pwmVal);
  } else {
    Serial.println(">>> Invalid thruster number or PWM (1100-1900)");
  }
}
