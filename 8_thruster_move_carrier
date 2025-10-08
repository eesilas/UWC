#include <Servo.h>

// ESC signal pins for all 8 thrusters
#define ESC_PIN_TR1 2   // Up/Down vector
#define ESC_PIN_UR2 3   // Forth/Back vector
#define ESC_PIN_LR3 4   // Forth/Back vector
#define ESC_PIN_BR4 5   // Up/Down vector
#define ESC_PIN_TL5 6   // Up/Down vector
#define ESC_PIN_UL6 7   // Forth/Back vector
#define ESC_PIN_LL7 8   // Forth/Back vector
#define ESC_PIN_BL8 9   // Up/Down vector

#define NUM_THRUSTERS 8
#define TRANSITION_STEP 10    // microseconds per update
#define TRANSITION_INTERVAL 20 // ms between steps
#define SWIPE_COUNT_MAX 10

enum Movement {
  STOP,
  UP,
  DOWN,
  LEFT,
  RIGHT,
  FRONT,
  BACK,
  SWIPE,
  ROTATE_CW
};

struct Thruster {
  Servo esc;
  int pin;
  int targetPulse;
  int currentPulse;
  unsigned long lastUpdate;
  String name;
  String vectorType;
};

Thruster thrusters[NUM_THRUSTERS] = {
  {Servo(), ESC_PIN_TR1, 1500, 1500, 0, "TR1", "Up/Down"},
  {Servo(), ESC_PIN_UR2, 1500, 1500, 0, "UR2", "Forth/Back"},
  {Servo(), ESC_PIN_LR3, 1500, 1500, 0, "LR3", "Forth/Back"},
  {Servo(), ESC_PIN_BR4, 1500, 1500, 0, "BR4", "Up/Down"},
  {Servo(), ESC_PIN_TL5, 1500, 1500, 0, "TL5", "Up/Down"},
  {Servo(), ESC_PIN_UL6, 1500, 1500, 0, "UL6", "Forth/Back"},
  {Servo(), ESC_PIN_LL7, 1500, 1500, 0, "LL7", "Forth/Back"},
  {Servo(), ESC_PIN_BL8, 1500, 1500, 0, "BL8", "Up/Down"}
};

int stopPulse = 1500;
Movement currentMovement = STOP;
unsigned long swipeStartTime = 0;
int swipeCount = 0;
bool brushOn = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Arduino Mega 2560 Pro - 8 Thruster Control Initialized");
  
  // Initialize all thrusters
  for (int i = 0; i < NUM_THRUSTERS; i++) {
    thrusters[i].esc.attach(thrusters[i].pin, 1000, 2000);
    thrusters[i].esc.writeMicroseconds(stopPulse);
    thrusters[i].targetPulse = stopPulse;
    thrusters[i].currentPulse = stopPulse;
    thrusters[i].lastUpdate = millis();
    
    Serial.print("Thruster ");
    Serial.print(thrusters[i].name);
    Serial.print(" (");
    Serial.print(thrusters[i].vectorType);
    Serial.println(") initialized");
  }
  
  Serial.println("\nAvailable commands:");
  Serial.println("  up, down, left, right, front, back - Movement commands");
  Serial.println("  swipe - Start swiping operation");
  Serial.println("  rotate - Rotate CW to scan next wall");
  Serial.println("  brush [on/off] - Control brush");
  Serial.println("  stop - Stop all thrusters");
  Serial.println("  c - Calibrate stop point (1480-1520)");
  Serial.println("  status - Show current thruster status");
}

void setThrusterPulse(int thrusterIndex, int pulse) {
  if (thrusterIndex >= 0 && thrusterIndex < NUM_THRUSTERS) {
    thrusters[thrusterIndex].targetPulse = pulse;
  }
}

void setMovement(Movement movement) {
  currentMovement = movement;
  
  switch(movement) {
    case STOP:
      for (int i = 0; i < NUM_THRUSTERS; i++) {
        thrusters[i].targetPulse = stopPulse;
      }
      Serial.println("All thrusters stopped");
      break;
      
    case UP:
      // UP (TR1:CCW, BR4:CCW, TL5:CCW, BL8:CCW)
      setThrusterPulse(0, 1400); // TR1 CCW
      setThrusterPulse(3, 1400); // BR4 CCW
      setThrusterPulse(4, 1400); // TL5 CCW
      setThrusterPulse(7, 1400); // BL8 CCW
      // Stop other thrusters
      setThrusterPulse(1, stopPulse);
      setThrusterPulse(2, stopPulse);
      setThrusterPulse(5, stopPulse);
      setThrusterPulse(6, stopPulse);
      Serial.println("Moving UP");
      break;
      
    case DOWN:
      // DN (TR1:CW, BR4:CW, TL5:CW, BL8:CW)
      setThrusterPulse(0, 1600); // TR1 CW
      setThrusterPulse(3, 1600); // BR4 CW
      setThrusterPulse(4, 1600); // TL5 CW
      setThrusterPulse(7, 1600); // BL8 CW
      // Stop other thrusters
      setThrusterPulse(1, stopPulse);
      setThrusterPulse(2, stopPulse);
      setThrusterPulse(5, stopPulse);
      setThrusterPulse(6, stopPulse);
      Serial.println("Moving DOWN");
      break;
      
    case LEFT:
      // LEFT (TR1:CCW, BR4:CW, TL5:CW, BL8:CCW)
      setThrusterPulse(0, 1400); // TR1 CCW
      setThrusterPulse(3, 1600); // BR4 CW
      setThrusterPulse(4, 1600); // TL5 CW
      setThrusterPulse(7, 1400); // BL8 CCW
      // Stop other thrusters
      setThrusterPulse(1, stopPulse);
      setThrusterPulse(2, stopPulse);
      setThrusterPulse(5, stopPulse);
      setThrusterPulse(6, stopPulse);
      Serial.println("Moving LEFT");
      break;
      
    case RIGHT:
      // RIGHT (TR1:CW, BR4:CCW, TL5:CCW, BL8:CW)
      setThrusterPulse(0, 1600); // TR1 CW
      setThrusterPulse(3, 1400); // BR4 CCW
      setThrusterPulse(4, 1400); // TL5 CCW
      setThrusterPulse(7, 1600); // BL8 CW
      // Stop other thrusters
      setThrusterPulse(1, stopPulse);
      setThrusterPulse(2, stopPulse);
      setThrusterPulse(5, stopPulse);
      setThrusterPulse(6, stopPulse);
      Serial.println("Moving RIGHT");
      break;
      
    case FRONT:
      // FRONT (UR2:CW, LR3:CCW, UL6:CW, LL7:CCW)
      setThrusterPulse(1, 1600); // UR2 CW
      setThrusterPulse(2, 1400); // LR3 CCW
      setThrusterPulse(5, 1600); // UL6 CW
      setThrusterPulse(6, 1400); // LL7 CCW
      // Stop other thrusters
      setThrusterPulse(0, stopPulse);
      setThrusterPulse(3, stopPulse);
      setThrusterPulse(4, stopPulse);
      setThrusterPulse(7, stopPulse);
      Serial.println("Moving FRONT");
      break;
      
    case BACK:
      // BACK (UR2:CCW, LR3:CW, UL6:CCW, LL7:CW)
      setThrusterPulse(1, 1400); // UR2 CCW
      setThrusterPulse(2, 1600); // LR3 CW
      setThrusterPulse(5, 140
