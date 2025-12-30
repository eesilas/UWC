// 第二块 Metro ESP32-S3 - 1个刷板(电机1&2) + 1个水下超声
#define TRIGGER_CMD 0x55
const int baudRate = 115200;

#define IN1_BOARD1 2
#define IN2_BOARD1 3
#define ENA_BOARD1 6
#define IN3_BOARD1 4
#define IN4_BOARD1 5
#define ENB_BOARD1 7

HardwareSerial SerialUltra3(1);

struct UltrasonicData {
  int current_raw_mm = 0;
  int stable_mm = 0;
  unsigned long stableStartTime = 0;
  bool firstValid = true;
};

UltrasonicData ultra3;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(IN1_BOARD1, OUTPUT); pinMode(IN2_BOARD1, OUTPUT); pinMode(ENA_BOARD1, OUTPUT);
  pinMode(IN3_BOARD1, OUTPUT); pinMode(IN4_BOARD1, OUTPUT); pinMode(ENB_BOARD1, OUTPUT);
  stopAll();

  SerialUltra3.begin(baudRate, SERIAL_8N1, 10, 11);

  Serial.println("ESP32-S3 #2 Ready: Brush + 1 Ultrasonic");
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    processBrushCommand(command);
  }

  SerialUltra3.write(TRIGGER_CMD);
  delay(20);
  readWithDebounce(SerialUltra3, ultra3);

  Serial.print("U3: "); Serial.print(ultra3.stable_mm);
  if (ultra3.stable_mm == 0) Serial.print(" mm (no)");
  else Serial.print(" mm");
  Serial.println();

  delay(100);
}

void processBrushCommand(String command) {
  int firstSpace = command.indexOf(' ', 2);
  int secondSpace = command.indexOf(' ', firstSpace + 1);

  if (firstSpace == -1) return;

  int motorNum = command.substring(1, firstSpace).toInt();
  String dir;
  int speed = 0;

  if (secondSpace != -1) {
    dir = command.substring(firstSpace + 1, secondSpace);
    speed = command.substring(secondSpace + 1).toInt();
  } else {
    dir = command.substring(firstSpace + 1);
  }
  dir.trim();

  if (motorNum >= 1 && motorNum <= 2) {
    controlMotor(motorNum, dir, speed);
    Serial.print("Executed: "); Serial.println(command);
  }
}

void controlMotor(int motor, String dir, int speed) {
  int in1, in2, ena;
  if (motor == 1) { in1 = IN1_BOARD1; in2 = IN2_BOARD1; ena = ENA_BOARD1; }
  else if (motor == 2) { in1 = IN3_BOARD1; in2 = IN4_BOARD1; ena = ENB_BOARD1; }
  else return;

  if (dir == "CW") { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); analogWrite(ena, speed); }
  else if (dir == "CCW") { digitalWrite(in1, LOW); digitalWrite(in2, HIGH); analogWrite(ena, speed); }
  else if (dir == "STOP") { digitalWrite(in1, LOW); digitalWrite(in2, LOW); analogWrite(ena, 0); }
}

void stopAll() {
  digitalWrite(IN1_BOARD1, LOW); digitalWrite(IN2_BOARD1, LOW); analogWrite(ENA_BOARD1, 0);
  digitalWrite(IN3_BOARD1, LOW); digitalWrite(IN4_BOARD1, LOW); analogWrite(ENB_BOARD1, 0);
}

void readWithDebounce(HardwareSerial &port, UltrasonicData &data) {
  int newRaw = 0;
  if (port.available() >= 4) {
    if (port.read() == 0xFF) {
      uint8_t buf[3];
      if (port.readBytes(buf, 3) == 3) {
        uint8_t checksum = 0xFF + buf[0] + buf[1];
        if ((checksum & 0xFF) == buf[2]) newRaw = (buf[0] << 8) + buf[1];
      }
      while (port.available()) port.read();
    }
  }

  data.current_raw_mm = newRaw;

  if (newRaw > 0) {
    if (data.firstValid || newRaw != data.stable_mm) {
      data.stableStartTime = millis();
      if (data.firstValid) data.stable_mm = newRaw;
      data.firstValid = false;
    }
    if (millis() - data.stableStartTime >= 500) data.stable_mm = newRaw;
  } else {
    data.stable_mm = 0;
    data.firstValid = true;
  }
}
