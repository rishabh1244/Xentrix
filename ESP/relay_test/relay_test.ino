// ============================================================
//  RELAY BLINK TEST
//  Toggles the relay ON and OFF every 2 seconds automatically.
//  You should hear a CLICK sound from the relay every 2 seconds.
//  No dashboard needed. No slave needed.
// ============================================================

#define RELAY_PIN 4

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  Serial.println("=== RELAY BLINK TEST ===");
  Serial.println("You should hear a CLICK every 2 seconds.");
}

void loop() {
  // Turn relay ON
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Relay ON  (pin LOW)");
  delay(2000);

  // Turn relay OFF
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("Relay OFF (pin HIGH)");
  delay(2000);
}
