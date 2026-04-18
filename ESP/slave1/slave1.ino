// ============================================================
//  SLAVE 1  —  ESP8266 (NodeMCU) Tank 1 Sensor Node
//  Connects to router briefly to get Wi-Fi channel,
//  then uses ESP-NOW on that channel to talk to Master
// ============================================================

#include <ESP8266WiFi.h>
#include <espnow.h>

// ── Wi-Fi (used only to detect router channel) ──
const char* WIFI_SSID     = "TPO-27";   // 2.4GHz band only!
const char* WIFI_PASSWORD = "tpo@2022";

// ── Master ESP32 MAC Address ──
uint8_t masterMAC[] = {0xB0, 0xCB, 0xD8, 0x03, 0x8D, 0xF4};

#define TANK_ID        1
#define TRIG_PIN       D1   // GPIO 5
#define ECHO_PIN       D2   // GPIO 4
#define TANK_HEIGHT_CM 14.0
#define BUFFER_CM      5.5

// Data packet — must match Master struct exactly
typedef struct __attribute__((packed)) {
  uint8_t  tank_id;
  float    percentage;
  float    waterLevel_cm;
  float    distance_cm;
} TankData;

TankData myData;

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("[ESP-NOW] Send: ");
  Serial.println(sendStatus == 0 ? "OK" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== NodeMCU SLAVE 1 BOOT ===");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // ── Step 1: Set Wi-Fi to Station Mode and use Channel 1 ──
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Keep radio ON
  uint8_t routerChannel = 1; // Force Channel 1 to match Master

  Serial.print("Slave 1 MAC: ");
  Serial.println(WiFi.macAddress());

  // ── Step 2: Init ESP-NOW ──
  if (esp_now_init() != 0) {
    Serial.println("ERROR: ESP-NOW init failed! Rebooting...");
    delay(3000);
    ESP.restart();
  }
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);

  // ── Step 3: Register Master peer on the router's exact channel ──
  if (esp_now_add_peer(masterMAC, ESP_NOW_ROLE_SLAVE, routerChannel, NULL, 0) != 0) {
    Serial.println("ERROR: Failed to add master peer! Rebooting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Ready! Sending to Master on Channel " + String(routerChannel) + " every 1s.\n");
}

void loop() {
  // ── Take 5 samples and average ──
  float dist_cm = 0;
  int validSamples = 0;
  for (int i = 0; i < 5; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration > 0) {
      dist_cm += (duration * 0.034 / 2.0);
      validSamples++;
    }
    delay(10);
  }
  
  if (validSamples > 0) {
    dist_cm = dist_cm / validSamples;
  } else {
    // If all fail, assume 0 distance (failsafe = 100% full to prevent overflow)
    dist_cm = 0;
  }

  // ── Apply buffer offset ──
  float waterLevel = TANK_HEIGHT_CM - (dist_cm - BUFFER_CM);
  if (waterLevel < 0)               waterLevel = 0;
  if (waterLevel > TANK_HEIGHT_CM)  waterLevel = TANK_HEIGHT_CM;
  float pct = (waterLevel / TANK_HEIGHT_CM) * 100.0;

  // ── Build & send ──
  myData.tank_id       = TANK_ID;
  myData.percentage    = pct;
  myData.waterLevel_cm = waterLevel;
  myData.distance_cm   = dist_cm;
  esp_now_send(masterMAC, (uint8_t *) &myData, sizeof(myData));

  Serial.print("Tank 1 | Dist: "); Serial.print(dist_cm);
  Serial.print(" cm | Level: "); Serial.print(waterLevel);
  Serial.print(" cm | Fill: "); Serial.print(pct);
  Serial.println(" %");

  delay(1000);
}
