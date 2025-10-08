# UWC
program name: underwater_motor.ino

This program is designed to control an underwater thruster (motor) using an ESC (Electronic Speed Controller) connected to an ESP32 microcontroller. 


Rewrite the program code from 
1. use Arduino ESP32 to use Arduino Mega 2560 Pro
2. change to control 8 underwater thruster (motor) using an ESC (Electronic Speed Controller) connected to an Arduino Mega 2560 Pro microcontroller, originally is control 1 underwater thruster (motor) using an ESC (Electronic Speed Controller) connected microcontroller.

3. orginal code
   
Here’s the new overview of its functionality:

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

