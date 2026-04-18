// ============================================================
//  MASTER  —  ESP32 Hub (Dual Tank + Relay + Servo)
//  - Receives ESP-NOW from Slave 1 (ESP8266) and Slave 2 (ESP32)
//  - Streams live data to the Web Dashboard via USB Serial
//  - Accepts commands from Dashboard (PUMP_ON, PUMP_OFF, etc)
// ============================================================

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESP32Servo.h>

// ── Hardware Pins ──
#define RELAY_PIN   4
#define SERVO_PIN   18

// ── Relay Logic ──
#define RELAY_OFF HIGH
#define RELAY_ON  LOW

// ── Servo Positions ──
#define SERVO_TANK1  40
#define SERVO_TANK2  90

// ── Auto Thresholds ──
#define FILL_START_PCT   20.0
#define FILL_STOP_PCT    95.0
#define DRY_RUN_SECS     60
#define DRY_RUN_RISE_CM  0.5

// ── Data Packet (must match both slaves exactly) ──
typedef struct __attribute__((packed)) {
  uint8_t  tank_id;
  float    percentage;
  float    waterLevel_cm;
  float    distance_cm;
} TankData;

// ── State ──
TankData tank1 = {1, 0.0, 0.0, 100.0};
TankData tank2 = {2, 0.0, 0.0, 100.0};
bool got_tank1 = false;
bool got_tank2 = false;

bool pumpIsRunning  = false;
bool autoMode       = false;
bool dryRunDetected = false;
int  servoAngle     = SERVO_TANK1;
int  activeTank     = 0;

float    levelWhenPumpStarted = 0.0;
unsigned long pumpStartTime   = 0;

Servo waterServo;

// ── ESP-NOW Receive Callback ──
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
  TankData received;
  memcpy(&received, incomingData, sizeof(received));
  if      (received.tank_id == 1) { tank1 = received; got_tank1 = true; }
  else if (received.tank_id == 2) { tank2 = received; got_tank2 = true; }
}

void pumpOn(int forTank) {
  if (pumpIsRunning) return;
  pumpIsRunning        = true;
  dryRunDetected       = false;
  activeTank           = forTank;
  pumpStartTime        = millis();
  levelWhenPumpStarted = (forTank == 1) ? tank1.waterLevel_cm : tank2.waterLevel_cm;
  servoAngle           = (forTank == 1) ? SERVO_TANK1 : SERVO_TANK2;
  waterServo.write(servoAngle);
  delay(400);
  digitalWrite(RELAY_PIN, RELAY_ON);
  Serial.print(">>> PUMP ON → Tank "); Serial.println(forTank);
}

void pumpOff(const char* reason) {
  pumpIsRunning = false;
  activeTank    = 0;
  digitalWrite(RELAY_PIN, RELAY_OFF);
  Serial.print(">>> PUMP OFF — "); Serial.println(reason);
}

void runAutoLogic() {
  if (!autoMode || dryRunDetected) return;
  if (pumpIsRunning) {
    float currentLevel = (activeTank == 1) ? tank1.waterLevel_cm : tank2.waterLevel_cm;
    float elapsed = (millis() - pumpStartTime) / 1000.0;
    if (elapsed >= DRY_RUN_SECS) {
      if ((currentLevel - levelWhenPumpStarted) < DRY_RUN_RISE_CM) {
        dryRunDetected = true;
        pumpOff("DRY RUN DETECTED");
        return;
      }
      pumpStartTime = millis();
      levelWhenPumpStarted = currentLevel;
    }
  }
  if (!pumpIsRunning) {
    if      (got_tank1 && tank1.percentage < FILL_START_PCT) pumpOn(1);
    else if (got_tank2 && tank2.percentage < FILL_START_PCT) pumpOn(2);
  } else {
    float pct = (activeTank == 1) ? tank1.percentage : tank2.percentage;
    if (pct >= FILL_STOP_PCT) pumpOff("Tank full");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  waterServo.attach(SERVO_PIN);
  waterServo.write(SERVO_TANK1);

  Serial.println("\n=== ESP32 MASTER BOOT ===");

  // ── Wi-Fi Station mode (no router — pure ESP-NOW) ──
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("╔══════════════════════════════════╗");
  Serial.print  ("║  MASTER MAC: ");
  Serial.print  (WiFi.macAddress());
  Serial.println("  ║");
  Serial.println("╚══════════════════════════════════╝");

  // ── Init ESP-NOW FIRST, then lock channel ──
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed! Rebooting...");
    delay(3000); ESP.restart();
  }

  // Lock to Channel 1 AFTER init (prevents init from resetting it)
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  uint8_t ch; wifi_second_chan_t sc;
  esp_wifi_get_channel(&ch, &sc);
  Serial.print("ESP-NOW listening on Channel: ");
  Serial.println(ch);

  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("System Ready! Waiting for slaves...\n");
}

void loop() {
  // ── Dashboard Serial Commands ──
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if      (cmd == "PUMP_ON")  { dryRunDetected = false; autoMode = false; pumpOn(servoAngle == SERVO_TANK1 ? 1 : 2); }
    else if (cmd == "PUMP_OFF") { autoMode = false; pumpOff("Manual OFF"); }
    else if (cmd == "AUTO_ON")  { autoMode = true;  dryRunDetected = false; Serial.println(">>> AUTO ON"); }
    else if (cmd == "AUTO_OFF") { autoMode = false; pumpOff("Auto Disabled"); }
    else if (cmd == "SERVO_T1") { servoAngle = SERVO_TANK1; waterServo.write(servoAngle); Serial.println(">>> SERVO → Tank 1"); }
    else if (cmd == "SERVO_T2") { servoAngle = SERVO_TANK2; waterServo.write(servoAngle); Serial.println(">>> SERVO → Tank 2"); }
    else if (cmd.startsWith("SERVO:")) { 
      int customAngle = cmd.substring(6).toInt();
      if (customAngle >= 40 && customAngle <= 90) {
        servoAngle = customAngle;
        waterServo.write(servoAngle);
        Serial.print(">>> SERVO → "); Serial.print(servoAngle); Serial.println(" °");
      }
    }
  }

  runAutoLogic();

  // ── Stream to Dashboard every 1 second ──
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    Serial.print("Tank1: ");      Serial.print(got_tank1 ? tank1.percentage    : 0.0);
    Serial.print(" % | Level1: "); Serial.print(got_tank1 ? tank1.waterLevel_cm : 0.0);
    Serial.print(" cm | Tank2: "); Serial.print(got_tank2 ? tank2.percentage    : 0.0);
    Serial.print(" % | Level2: "); Serial.print(got_tank2 ? tank2.waterLevel_cm : 0.0);
    Serial.print(" cm | Pump: ");  Serial.print(pumpIsRunning ? "ON" : "OFF");
    Serial.print(" | Auto: ");     Serial.print(autoMode ? "ON" : "OFF");
    Serial.print(" | Servo: ");    Serial.print(servoAngle);
    Serial.print(" | DryRun: ");   Serial.println(dryRunDetected ? "YES" : "NO");
  }
}
