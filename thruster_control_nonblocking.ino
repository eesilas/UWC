#include <Servo.h>

// ESC signal pins for all 8 thrusters
#define ESC_PIN_1 2
#define ESC_PIN_2 3
#define ESC_PIN_3 4
#define ESC_PIN_4 5
#define ESC_PIN_5 6
#define ESC_PIN_6 7
#define ESC_PIN_7 8
#define ESC_PIN_8 9

#define NUM_THRUSTERS 8
#define TRANSITION_STEP 10    // microseconds per update
#define TRANSITION_INTERVAL 20 // ms between steps

struct Thruster {
  Servo esc;
  int pin;
  int targetPulse;
  int currentPulse;
  unsigned long lastUpdate;
};

Thruster thrusters[NUM_THRUSTERS] = {
  {Servo(), ESC_PIN_1, 1500, 1500, 0},
  {Servo(), ESC_PIN_2, 1500, 1500, 0},
  {Servo(), ESC_PIN_3, 1500, 1500, 0},
  {Servo(), ESC_PIN_4, 1500, 1500, 0},
  {Servo(), ESC_PIN_5, 1500, 1500, 0},
  {Servo(), ESC_PIN_6, 1500, 1500, 0},
  {Servo(), ESC_PIN_7, 1500, 1500, 0},
  {Servo(), ESC_PIN_8, 1500, 1500, 0}
};

int stopPulse = 1500;

void setup() {
  Serial.begin(115200);
  Serial.println("Arduino Mega 2560 Pro - 8 Thruster Control (Non-blocking) Initialized");
  for (int i = 0; i < NUM_THRUSTERS; i++) {
    thrusters[i].esc.attach(thrusters[i].pin, 1000, 2000);
    thrusters[i].esc.writeMicroseconds(stopPulse);
    thrusters[i].targetPulse = stopPulse;
    thrusters[i].currentPulse = stopPulse;
    thrusters[i].lastUpdate = millis();
  }
  Serial.println("Input format:");
  Serial.println("  [thruster 1-8] [speed -100 to 100] - Control individual thruster");
  Serial.println("  all [speed -100 to 100] - Control all thrusters");
  Serial.println("  c - Calibrate stop point (1480-1520)");
}

void setAllThrusters(int pulse) {
  for (int i = 0; i < NUM_THRUSTERS; i++) {
    thrusters[i].targetPulse = pulse;
  }
}

void setThrusterTarget(int thruster, int pulse) {
  if (thruster >= 0 && thruster < NUM_THRUSTERS) {
    thrusters[thruster].targetPulse = pulse;
  }
}

void updateThrusters() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_THRUSTERS; i++) {
    if (thrusters[i].currentPulse != thrusters[i].targetPulse) {
      if (now - thrusters[i].lastUpdate >= TRANSITION_INTERVAL) {
        int diff = thrusters[i].targetPulse - thrusters[i].currentPulse;
        if (abs(diff) < TRANSITION_STEP) {
          thrusters[i].currentPulse = thrusters[i].targetPulse;
        } else {
          thrusters[i].currentPulse += (diff > 0 ? TRANSITION_STEP : -TRANSITION_STEP);
        }
        thrusters[i].esc.writeMicroseconds(thrusters[i].currentPulse);
        thrusters[i].lastUpdate = now;
      }
    }
  }
}

void processSerialInput() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.equalsIgnoreCase("c")) {
      Serial.println("Enter stop pulse (1480-1520):");
      while (!Serial.available()) {}
      String calibInput = Serial.readStringUntil('\n');
      stopPulse = calibInput.toInt();
      stopPulse = constrain(stopPulse, 1480, 1520);
      setAllThrusters(stopPulse);
      Serial.print("Stop point set to: ");
      Serial.println(stopPulse);
    } else {
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String thrusterStr = input.substring(0, spaceIndex);
        String speedStr = input.substring(spaceIndex + 1);
        int speed = speedStr.toInt();
        speed = constrain(speed, -100, 100);
        int targetPulse;
        if (speed == 0) {
          targetPulse = stopPulse;
        } else {
          targetPulse = map(abs(speed), 0, 100, stopPulse, speed >= 0 ? 2000 : 1000);
        }
        if (thrusterStr.equalsIgnoreCase("all")) {
          setAllThrusters(targetPulse);
          Serial.print("All thrusters set to speed: ");
          Serial.print(speed);
          Serial.print("%, PWM: ");
          Serial.println(targetPulse);
        } else {
          int thruster = thrusterStr.toInt() - 1;
          if (thruster >= 0 && thruster < NUM_THRUSTERS) {
            setThrusterTarget(thruster, targetPulse);
            Serial.print("Thruster ");
            Serial.print(thruster + 1);
            Serial.print(" set to speed: ");
            Serial.print(speed);
            Serial.print("%, PWM: ");
            Serial.println(targetPulse);
          } else {
            Serial.println("Invalid thruster number (1-8)");
          }
        }
      } else {
        Serial.println("Invalid input format. Use '[thruster] [speed]' or 'all [speed]'");
      }
    }
  }
}

void loop() {
  processSerialInput();
  updateThrusters();
}
