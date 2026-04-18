// ============================================================
//  PUMP Control Node — Single ESP8266 (NodeMCU)
//  Listens to Serial Commands to toggle Pump ON/OFF
// ============================================================

// NodeMCU Pin Mapping: D1 = GPIO 5
#define RELAY_PIN D1     // Connect Pump Relay IN to D1

#define RELAY_OFF HIGH
#define RELAY_ON  LOW

bool pumpIsRunning = false;

void setup() {
  Serial.begin(74880);
  delay(10);
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  
  Serial.println("\n=== NodeMCU PUMP ONLY BOOT ===");
  Serial.println("System Ready. Waiting for commands...");
}

void loop() {
  // Listen for manual commands from dashboard
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    // Pump Commands
    if (cmd == "PUMP_ON") {
      pumpIsRunning = true;
      digitalWrite(RELAY_PIN, RELAY_ON);
      Serial.println(">>> PUMP TURNED ON");
    } else if (cmd == "PUMP_OFF") {
      pumpIsRunning = false;
      digitalWrite(RELAY_PIN, RELAY_OFF);
      Serial.println(">>> PUMP TURNED OFF");
    } 
  }

  // Every second, report status for dashboard to parse
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    Serial.print("Pump: ");
    Serial.println(pumpIsRunning ? "ON" : "OFF");
  }
}
