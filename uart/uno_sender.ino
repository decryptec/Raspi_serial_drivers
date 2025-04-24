const int buttonPin = 2;     // Pushbutton pin
const int ledPin = 13;       // Built-in LED pin

bool ledState = false;
bool lastButtonState = LOW;

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
  while (!Serial);  // Wait for Serial to be ready
}

void loop() {
  // Check for message from Raspberry Pi
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "ON") {
      ledState = true;
      digitalWrite(ledPin, HIGH);
    } else if (input == "OFF") {
      ledState = false;
      digitalWrite(ledPin, LOW);
    }
  }

  // Only send button state if LED is on
  if (ledState) {
    bool currentButtonState = digitalRead(buttonPin);
    if (currentButtonState != lastButtonState) {
      lastButtonState = currentButtonState;
      if (currentButtonState == HIGH) {
        Serial.println("PRESSED");
      } else {
        Serial.println("RELEASED");
      }
    }
  }

  delay(50); // Debounce & pacing
}
