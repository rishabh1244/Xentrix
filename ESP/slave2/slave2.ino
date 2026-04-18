// ============================================================
//  SLAVE 2  —  ESP32 Tank 2 Sensor Node
//  Connects to router briefly to get Wi-Fi channel,
//  then uses ESP-NOW on that channel to talk to Master
// ============================================================

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ── Wi-Fi (used only to detect router channel) ──
const char* WIFI_SSID     = "TPO-27";   // 2.4GHz band only!
const char* WIFI_PASSWORD = "tpo@2022";

// ── Master ESP32 MAC Address ──
uint8_t masterMAC[] = {0xB0, 0xCB, 0xD8, 0x03, 0x8D, 0xF4};

#define TANK_ID        2
#define TRIG_PIN       27    // Safe GPIO - no strapping conflicts
#define ECHO_PIN       26    // Safe GPIO - no strapping conflicts
#define TANK_HEIGHT_CM 14.0
#define BUFFER_CM      5.0

// ── Data packet — must match Master struct exactly ──
typedef struct __attribute__((packed)) {
  uint8_t  tank_id;
  float    percentage;
  float    waterLevel_cm;
  float    distance_cm;
} TankData;

TankData myData;
esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print(" [Delivery: ");
  Serial.print(status == ESP_NOW_SEND_SUCCESS ? "Success]" : "Fail]");
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== ESP32 SLAVE 2 BOOT ===");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // ── Step 1: Set Wi-Fi to Station Mode and use Channel 1 ──
  WiFi.mode(WIFI_STA);
  // Do NOT use WiFi.disconnect() here, it causes the STA interface to sleep/fail on ESP32!
  delay(100);
  WiFi.setSleep(false); // Keep radio ON
  uint8_t routerChannel = 1; // Force Channel 1 to match Master

  Serial.print("Slave 2 MAC: ");
  Serial.println(WiFi.macAddress());

  // ── Step 2: Init ESP-NOW ──
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed! Rebooting...");
    delay(3000);
    ESP.restart();
  }
  esp_now_register_send_cb(reinterpret_cast<esp_now_send_cb_t>(OnDataSent));
  Serial.println("[ESP-NOW] Initialized OK");

  // ── Step 3: Lock radio to same channel as router ──
  esp_wifi_set_channel(routerChannel, WIFI_SECOND_CHAN_NONE);

  uint8_t ch; wifi_second_chan_t sc;
  esp_wifi_get_channel(&ch, &sc);
  Serial.print("Locked to Channel: ");
  Serial.println(ch);

  // ── Step 4: Register Master as peer ──
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, masterMAC, 6);
  peerInfo.channel = routerChannel;
  peerInfo.encrypt = false;
  peerInfo.ifidx   = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ERROR: Failed to add Master peer! Rebooting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Master peer registered OK");
  Serial.println("Ready! Sending data every 1s...\n");
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

  esp_err_t result = esp_now_send(masterMAC, (uint8_t *) &myData, sizeof(myData));

  Serial.print("Tank 2 | Dist: ");    Serial.print(dist_cm);
  Serial.print(" cm | Level: ");      Serial.print(waterLevel);
  Serial.print(" cm | Fill: ");       Serial.print(pct);
  Serial.print(" % | Send: ");        Serial.println(result == ESP_OK ? "OK" : "FAIL");

  delay(1000);
}
