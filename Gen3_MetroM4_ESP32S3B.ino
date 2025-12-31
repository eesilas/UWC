// Second Adafruit Metro ESP32-S3
// Updated: Motor control command format unified with Grand Central (motor1:cw:255, motor1:stop, etc.)

#define TRIGGER_CMD 0x55
const int baudRate = 115200;

// Brush board pins (Motors 1 & 2)
#define IN1_MOTOR1 2
#define IN2_MOTOR1 3
#define ENA_MOTOR1 6
#define IN1_MOTOR2 4
#define IN2_MOTOR2 5
#define ENA_MOTOR2 7

HardwareSerial SerialUltra3(1);  // RX=10, TX=11

// Ultrasonic data structure
struct UltrasonicData {
  int current_raw_mm = 0;
  int stable_mm = 0;
  unsigned long stableStartTime = 0;
  bool firstValid = true;
};

UltrasonicData ultra3;

// Motor status variables (for display)
String motor1_status = "STOP";
int motor1_speed = 0;
String motor2_status = "STOP";
int motor2_speed = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Brush board initialization
  pinMode(IN1_MOTOR1, OUTPUT); pinMode(IN2_MOTOR1, OUTPUT); pinMode(ENA_MOTOR1, OUTPUT);
  pinMode(IN1_MOTOR2, OUTPUT); pinMode(IN2_MOTOR2, OUTPUT); pinMode(ENA_MOTOR2, OUTPUT);
  stopAllMotors();

  // Ultrasonic initialization
  SerialUltra3.begin(baudRate, SERIAL_8N1, 10, 11);

  Serial.println("ESP32-S3 #2 System Ready - Handling Brush Board (Motors 1&2) + U3");
  Serial.println("Motor commands: motor1:cw:255, motor1:ccw:150, motor1:stop (same for motor2)");
  Serial.println();
}

void loop() {
  // Handle motor commands (new unified format)
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    processMotorCommand(command);
  }

  // Read ultrasonic U3
  SerialUltra3.write(TRIGGER_CMD);
  delay(20);
  readWithDebounce(SerialUltra3, ultra3);

  // Line 1: Ultrasonic3
  Serial.print("Ultrasonic3: ");
  Serial.print(ultra3.stable_mm);
  if (ultra3.stable_mm == 0) Serial.print(" mm (no signal)");
  else Serial.print(" mm");
  Serial.println();

  // Line 2: Motor1 status
  Serial.print("Motor1: ");
  if (motor1_status == "STOP") Serial.print("STOP");
  else { Serial.print(motor1_status); Serial.print(" "); Serial.print(motor1_speed); }
  Serial.println();

  // Line 3: Motor2 status
  Serial.print("Motor2: ");
  if (motor2_status == "STOP") Serial.print("STOP");
  else { Serial.print(motor2_status); Serial.print(" "); Serial.print(motor2_speed); }
  Serial.println();

  Serial.println();  // Blank line separator
  delay(100);
}

// New unified motor command processing
void processMotorCommand(String cmd) {
  if (cmd.startsWith("motor1:") || cmd.startsWith("motor2:")) {
    int motorNum = cmd.startsWith("motor1:") ? 1 : 2;
    String &status = (motorNum == 1) ? motor1_status : motor2_status;
    int &speed_var = (motorNum == 1) ? motor1_speed : motor2_speed;

    String rest = cmd.substring(cmd.indexOf(':') + 1);
    rest.trim();

    if (rest == "stop") {
      controlMotor(motorNum, "STOP", 0);
      status = "STOP";
      speed_var = 0;
      Serial.print(">>> Motor "); Serial.print(motorNum); Serial.println(" STOP");
      return;
    }

    int colonPos = rest.indexOf(':');
    if (colonPos == -1) {
      Serial.println(">>> Invalid motor command (missing speed)");
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
      String dirU = (dir == "cw") ? "CW" : "CCW";
      controlMotor(motorNum, dirU, speed);
      status = dirU;
      speed_var = speed;
      Serial.print(">>> Motor "); Serial.print(motorNum);
      Serial.print(" "); Serial.print(dirU);
      Serial.print(" speed: "); Serial.println(speed);
    } else {
      Serial.println(">>> Direction must be cw or ccw");
    }
  } else if (cmd.length() > 0) {
    Serial.println(">>> Unknown command. Use motor1:cw:255, motor1:stop, etc.");
  }
}

// Control single motor
void controlMotor(int motor, String dir, int speed) {
  int in1, in2, ena;
  if (motor == 1) { in1 = IN1_MOTOR1; in2 = IN2_MOTOR1; ena = ENA_MOTOR1; }
  else if (motor == 2) { in1 = IN1_MOTOR2; in2 = IN2_MOTOR2; ena = ENA_MOTOR2; }
  else return;

  if (dir == "CW") {
    digitalWrite(in1, HIGH); digitalWrite(in2, LOW); analogWrite(ena, speed);
  } else if (dir == "CCW") {
    digitalWrite(in1, LOW); digitalWrite(in2, HIGH); analogWrite(ena, speed);
  } else if (dir == "STOP") {
    digitalWrite(in1, LOW); digitalWrite(in2, LOW); analogWrite(ena, 0);
  }
}

// Stop all motors on startup or emergency
void stopAllMotors() {
  controlMotor(1, "STOP", 0);
  controlMotor(2, "STOP", 0);
  motor1_status = "STOP"; motor1_speed = 0;
  motor2_status = "STOP"; motor2_speed = 0;
}

// Ultrasonic read with debounce (unchanged)
void readWithDebounce(HardwareSerial &port, UltrasonicData &data) {
  int newRaw = 0;

  if (port.available() >= 4) {
    if (port.read() == 0xFF) {
      uint8_t buf[3];
      if (port.readBytes(buf, 3) == 3) {
        uint8_t checksum = 0xFF + buf[0] + buf[1];
        if ((checksum & 0xFF) == buf[2]) {
          newRaw = (buf[0] << 8) + buf[1];
        }
      }
      while (port.available()) port.read();
    }
  }

  data.current_raw_mm = newRaw;

  if (newRaw > 0) {
    if (data.firstValid || newRaw != data.stable_mm) {
      data.stableStartTime = millis();
      if (data.firstValid) {
        data.stable_mm = newRaw;
        data.firstValid = false;
      }
    }
    if (millis() - data.stableStartTime >= 500) {
      data.stable_mm = newRaw;
    }
  } else {
    data.stable_mm = 0;
    data.firstValid = true;
  }
}
