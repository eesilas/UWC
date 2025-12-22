# UWC
program name: underwater_motor.ino

This program is designed to control an underwater thruster (motor) using an ESC (Electronic Speed Controller) connected to an ESP32 microcontroller. 

There are 8 thruster motor and name of 8 Thrusters layout 

TR1 : up and down vector 
UR2 : forth and back vector
LR3 : forth and back vector
BR4 : up and down vector
TL5 : up and down vector
UL6 : forth and back vector
LL7 : forth and back vector
UL8 : up and down vector

Movement UP DN LEFT RIGHT corresponding to (rotation CW is clock wise, and CCW is counter clockwise) of each of the 8 thruster motors

sub-carrier UP (TR1:CCW, BR4:CCW, TL5:CCW, BL8:CCW)
sub-carrier DN (TR1:CW, BR4:CW, TL5:CW,  BL8:CW)
sub-carrier LEFT (TR1:CCW, BR4:CW, TL5:CW, BL8:CCW)
sub-carrier RIGHT (TR1:CW, BR4:CCW, TL5:CCW, BL8:CW)
sub-carrier FRONT (UR2:CW, LR3:CCW, UL6:CW, LL7:CCW)
sub-carrier BACK  (UR2:CW, LR3:CCW, UL6:CW, LL7:CCW)

Operation
Start:
sub-carrier vertical move to top left 
brush turn on
Swip:
sub-carrier move right 1.4 meter with speed:
sub-carrier move down 0.2 meter with speed:
sub-carrier move left 1.4 meter with speed:
sub-carrier move down 0.2 meter with speed:
if not count > 10 Goto Swip
end swip 1th wall
rotate CW to scan second wall

Rewrite the program code from
1. use Arduino ESP32 to use Arduino Mega 2560 Pro
2. change to control 8 underwater thruster (motor) using an ESC (Electronic Speed Controller) connected to an Arduino Mega 2560 Pro microcontroller, originally is control 1 underwater thruster (motor) using an ESC (Electronic Speed Controller) connected microcontroller.

Here’s the new overview of its functionality:

Main Features
Thruster Control via Serial Input

The program listens for commands from the serial interface (USB/Serial monitor) at 115200 baud rate.
You can input a speed value (from -100 to 100) or send the letter 'c' to calibrate the stop position.

Speed Setting
Positive values (1 to 100): Move the thruster forward.
Negative values (-1 to -100): Move the thruster backward.
Zero (0): Stop the thruster (no movement).
The speed value is mapped to a PWM pulse width (1000–2000 microseconds), which is how ESCs electronic speed control motor's speed and direction:
1900 µs for full forward,
1100 µs for full reverse,
stopPulse (default 1500 µs, adjustable) for stop.

Calibration Mode
If you send 'c' over serial, the program lets you set the stop pulse (between 1480–1520 µs) to calibrate the exact pulse width that stops your specific ESC/motor, where ESC, electronic speed control is driving the brushless motor by physically three wire.

Smooth Transition
When changing speed, the program gradually adjusts the PWM pulse in steps of 10 µs every 20 ms to minimize vibration and mechanical stress.

How It Works
On startup, the ESC is attached and initialized at the stop position.
The main loop checks for serial input:
If a speed value is received, it calculates the corresponding PWM pulse and smoothly transitions to that value.
If 'c' is received, it enters calibration mode for the stop pulse.
It prints status updates to the serial monitor for user feedback.

Table corresponding motor number(left col) and 8 thruster rotation and speed (top row):
Motor Forward Backward Left Right Up Down
1    0     0     1     -1      -1     1;
2    0     0     1     -1       1     -1;
3     0    0     1     -1       1     -1;
4    0    0      -1     1       1     -1;
5    -1    1     -0.2   -0.2    0      0;
6    -1    1     -0.2   -0.2    0      0;
7     1    -1     0.2    0.2    0      0;
8     1    -1     0.2    0.2    0      0;

thurster signal from 1900 to 1100, where 1900 is CW and 1100 is CCW, where the middle is 1500 stop (number in microsecond)

Router card Deco, SIM card CLS number 64494166
Hand handler       shift 
                                 up, spin
RPi 192.168.68.57
Router 192.168.68.1 (default by distribute 50 to 250)
Laptop (red computer) 192.168.68.50
User\cloud led\python client.py
Press A by handler to start, and A again to stop
Use Android phone to Deco icon, it could fix IP and confirm IP to particular value 

Use PowerShell > ssh 192.168.68.57

Procedures to run demo of sub-carrier:
AP 4G card HK with Deco router green lite up
Acer Nitro computer (red computer) with USB mouse
Browser 192.168.68.57:8080
browser display control panel 192.168.68.57:5000 (control panel)
Yellow cable roll to router: router one to yellow cable roll and router one to Acer nitro PC with 2 RJ45 cable
Sub-carrier's RPi is turn on 
                               
In summary:
This program allows you to control an underwater motor's speed and direction through an ESC using simple serial commands. It supports forward/reverse motion, includes a calibration mode for precise stopping, and implements smooth speed transitions for safer motor operation.
_________________________
Client.py functionality
_________________________
This Python program is designed to use a joystick as an input device, sending data about its states and inputs to a Raspberry Pi (RPi) server using the SocketIO protocol. The application uses the socketio and pygame libraries to enable communication between the client (this script) and the server.

Structure and Key Components
The program has three main functional areas:

Socket.IO Client Initialization and Events:

Sets up the communication channel between the client and the server using the socketio library.
Defines events for connecting to and disconnecting from the server.
Initialization of Pygame and Joystick:

Configures the pygame library to interact with the joystick input device.
Ensures that a joystick is connected before proceeding.
Main Loop for Input Reading and Emission:

Continuously reads the state of the joystick (axes, buttons).
Forms and sends input data (surge, sway, heave, yaw, and button states) to the server at a regular interval.
Concepts and Functions
1. Socket.IO Client Initialization
The socketio.Client class is used to enable communication with the server over the SocketIO protocol. Here’s the structure:

Reconnect Logic:
The socketio.Client is configured with automatic reconnection logic using the reconnection=True and related parameters (reconnection_attempts, reconnection_delay) in case the connection to the server is lost.
Server URL:
SERVER_URL specifies the IP address and port of the server, which in this case seems to be hosted at http://192.168.68.59:5000.
Event Handlers:
@sio.event(namespace='/') is a decorator for attaching event-handling functions. These functions (connect, disconnect) are triggered when the client connects to or disconnects from the server.
2. Pygame Initialization
The pygame library is used for interacting with the joystick. The steps are:

Library Initialization:
pygame.init() initializes pygame.
pygame.joystick.init() initializes the joystick subsystem.
Joystick Detection and Configuration:
The script validates that at least one joystick is connected (pygame.joystick.get_count() > 0).
A reference to the first joystick device is obtained (joystick = pygame.joystick.Joystick(0)).
The joystick is initialized for interaction (joystick.init()).
3. Main Loop
The heart of the program lies in the while True loop where joystick inputs are continuously read, processed, and sent to the server.

Input Reading:
The script reads the state of the joystick’s axes (representing motion) and buttons. Axes and button functionality mapping:
Axes:
surge: Derived from vertical motion of the left stick (joystick.get_axis(1)).
sway: Derived from horizontal motion of the left stick (joystick.get_axis(0)).
heave: Derived from vertical motion of the right stick (joystick.get_axis(3)).
yaw: Derived from horizontal motion of the right stick (joystick.get_axis(2)).
Buttons:
The status of all buttons is captured in a list using joystick.get_button(i) for all available buttons.
Data Transmission:
Data is sent to the server using sio.emit(). The joystick state (surge, sway, heave, yaw, buttons) is transmitted in a dictionary to the namespace '/' on the server.
Interval:
A sleep interval (time.sleep(INTERVAL), where INTERVAL = 0.05) controls the frequency of the input reading and transmission loop.
Key Functionalities (Walkthrough)
Connection to the Server:

The program attempts to connect to the server at the URL hardcoded in SERVER_URL.
If the connection fails, the script prints an error message and exits.
Joystick Input Initialization:

The program ensures that at least one joystick is connected, initializing it for communication. Otherwise, it aborts execution with an error.
Input Capture and Emission:

Using the pygame.event.pump() function, all pending joystick events are processed.
Axes and button states are polled and converted into a dictionary that represents the current state of the joystick.
The state is transmitted to the server using sio.emit(), which sends it as a JSON-like message.
Reconnection and Fault Tolerance:

The script uses built-in reconnection and error handling, so the client can attempt to re-establish the connection to the server automatically.
Feedback:

The script logs debug information to the console, including data about joystick inputs (surge, sway, heave, yaw, A_button).
Key Points to Note
Namespace-Specific Events: The namespace '/' is explicitly defined for connection, disconnection, and emission events.

Execution Safety: The code checks for joystick presence and handles connection errors, ensuring the program doesn’t crash unexpectedly.

Hardcoded Values: SERVER_URL and joystick axes/button mappings are hardcoded, making the script less flexible for other servers or input devices.

Efficient Retry Attempts: The reconnection attempts with custom intervals (1 second) and a maximum of 5 tries ensure the program doesn't excessively retry connections.

Potential Improvements
Dynamic Configuration:

Allow SERVER_URL and INTERVAL to be dynamically configurable, e.g., using command-line arguments or a configuration file.
Joystick Multiplexing:

Add support for dynamically selecting and managing multiple joysticks if more than one is connected.
Namespace Customization:

Support configurable namespaces for Socket.IO events ('/' is used as the default namespace here).
Improved Fault Handling:

More advanced error handling could be added, such as different actions based on specific exceptions.
This program would serve as a communication link between a joystick-controlled client and Raspberry Pi server in scenarios like robotics or remote-control applications.

