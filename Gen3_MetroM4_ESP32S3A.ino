// First Adafruit Metro ESP32-S3 - Responsible for 2 Underwater Ultrasonic Sensors (U1 & U2)

#define TRIGGER_CMD 0x55
const int baudRate = 115200;

HardwareSerial SerialUltra1(1);  // RX=4, TX=5
HardwareSerial SerialUltra2(2);  // RX=6, TX=7

struct UltrasonicData {
  int current_raw_mm = 0;
  int stable_mm = 0;
  unsigned long stableStartTime = 0;
  bool firstValid = true;
};

UltrasonicData ultra1, ultra2;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SerialUltra1.begin(baudRate, SERIAL_8N1, 4, 5);
  SerialUltra2.begin(baudRate, SERIAL_8N1, 6, 7);

  Serial.println("ESP32-S3 #1 System Ready - Handling U1 and U2");
  Serial.println();
}

void loop() {
  SerialUltra1.write(TRIGGER_CMD);
  SerialUltra2.write(TRIGGER_CMD);

  delay(20);

  readWithDebounce(SerialUltra1, ultra1);
  readWithDebounce(SerialUltra2, ultra2);

  // Line 1: U1
  Serial.print("Ultrasonic1: ");
  Serial.print(ultra1.stable_mm);
  if (ultra1.stable_mm == 0) Serial.print(" mm (no signal)");
  else Serial.print(" mm");
  Serial.println();

  // Line 2: U2
  Serial.print("Ultrasonic2: ");
  Serial.print(ultra2.stable_mm);
  if (ultra2.stable_mm == 0) Serial.print(" mm (no signal)");
  else Serial.print(" mm");
  Serial.println();

  Serial.println();  // Blank line separator
  delay(100);
}

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
