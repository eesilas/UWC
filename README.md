# UWC
program name: underwater_motor.ino

This program is designed to control an underwater thruster (motor) using an ESC (Electronic Speed Controller) connected to an ESP32 microcontroller. 


Rewrite the program code from 
1. use Arduino ESP32 to use Arduino Mega 2560 Pro
2. change to control 8 underwater thruster (motor) using an ESC (Electronic Speed Controller) connected to an Arduino Mega 2560 Pro microcontroller, originally is control 1 underwater thruster (motor) using an ESC (Electronic Speed Controller) connected microcontroller.

3. orginal code
4. #include <ESP32Servo.h>

#define ESC_PIN 16 // ESC signal pin

Servo esc;
int stopPulse = 1500; // stop pulse which is adjustable 
int currentPulse = 1500; // current pulse

void setup() {
  // initalize serial communication
  Serial.begin(115200);
  Serial.println("ESP32 Thruster Control Initialized");

  // add ESC and set PWM range 1000 to 2000 µs
  esc.attach(ESC_PIN, 1000, 2000);

  // Initalize ESC set to stop
  esc.writeMicroseconds(stopPulse);
  delay(2000); // wait ESC initialize

  Serial.println("Input speed (-100 到 100）or 'c' calibrate stop point - negative value represents backard，0 represents stop");
}

void smoothTransition(int targetPulse) {
  // graduatelly adjust PWM to reduce vibration
  while (currentPulse != targetPulse) {
    if (currentPulse < targetPulse) {
      currentPulse += 10;
      if (currentPulse > targetPulse) currentPulse = targetPulse;
    } else {
      currentPulse -= 10;
      if (currentPulse < targetPulse) currentPulse = targetPulse;
    }
    esc.writeMicroseconds(currentPulse);
    delay(20); // every time adjust time between 20ms
  }
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // remove redunt space
    if (input.equalsIgnoreCase("c")) {
      // enter stop point
      Serial.println("enter stop pulse（1480-1520）：");
      while (!Serial.available()) {}
      String calibInput = Serial.readStringUntil('\n');
      stopPulse = calibInput.toInt();
      stopPulse = constrain(stopPulse, 1480, 1520); // limiting range
      currentPulse = stopPulse;
      esc.writeMicroseconds(stopPulse);
      Serial.print("stop point set to：");
      Serial.println(stopPulse);
    } else {
      // handling input speed
      int speed = input.toInt();
      speed = constrain(speed, -100, 100); // range of speed limit
      // calculating target PWM value
      int targetPulse;
      if (speed == 0) {
        targetPulse = stopPulse; // use calibration stop point
      } else {
        targetPulse = map(abs(speed), 0, 100, stopPulse, speed >= 0 ? 2000 : 1000);
      }
      // smooth transition to target PWM
      smoothTransition(targetPulse);
      // printing status
      Serial.print("speed: ");
      Serial.print(speed);
      Serial.print("%, PWM: ");
      Serial.println(targetPulse);
    }
  }
}
   
5. Here’s the new overview of its functionality:

Main Features
Thruster Control via Serial Input

The program listens for commands from the serial interface (USB/Serial monitor) at 115200 baud rate.
You can input a speed value (from -100 to 100) or send the letter 'c' to calibrate the stop position.

Speed Setting
Positive values (1 to 100): Move the thruster forward.
Negative values (-1 to -100): Move the thruster backward.
Zero (0): Stop the thruster (no movement).
The speed value is mapped to a PWM pulse width (1000–2000 microseconds), which is how ESCs control motor speed and direction:
2000 µs for full forward,
1000 µs for full reverse,
stopPulse (default 1500 µs, adjustable) for stop.

Calibration Mode
If you send 'c' over serial, the program lets you set the stop pulse (between 1480–1520 µs) to calibrate the exact pulse width that stops your specific ESC/motor.

Smooth Transition
When changing speed, the program gradually adjusts the PWM pulse in steps of 10 µs every 20 ms to minimize vibration and mechanical stress.

How It Works
On startup, the ESC is attached and initialized at the stop position.
The main loop checks for serial input:
If a speed value is received, it calculates the corresponding PWM pulse and smoothly transitions to that value.
If 'c' is received, it enters calibration mode for the stop pulse.
It prints status updates to the serial monitor for user feedback.

In summary:
This program allows you to control an underwater motor's speed and direction through an ESC using simple serial commands. It supports forward/reverse motion, includes a calibration mode for precise stopping, and implements smooth speed transitions for safer motor operation.

