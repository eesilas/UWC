const int relayPin = 7; // Relay control pin 

void setup() {
  Serial.begin(115200); // Initialize serial 
  pinMode(relayPin, OUTPUT); // Set relay pin as output
  digitalWrite(relayPin, LOW); // initialize state: relay open to stop motor
  Serial.println("Please input '1' Start motor，'0' Stop motor");
}

void loop() {
  if (Serial.available() > 0) { // check if any serial input
    char input = Serial.read(); // please take input character
    if (input == '0') { // input '1' start motor
      digitalWrite(relayPin, HIGH); // close relay low level trigger
      Serial.println("motor stop");
    } else if (input == '1') { // input '0' stop motor
      digitalWrite(relayPin, LOW); // open relay
      Serial.println("motor start");
    }
    // clear serial buffer area to avoid repeat read
    while (Serial.available() > 0) {
      Serial.read();
    }
  }
}
