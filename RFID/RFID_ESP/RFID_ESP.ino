#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>

// ================= WiFi =================
const char* ssid     = "MO";
const char* password = "10000000@#";

IPAddress local_IP(192, 168, 0, 101);   // Gate 1  101         Gate 2  102
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

IPAddress serverIP(192, 168, 0, 100);   // PC IP
const uint16_t serverPort = 5005;

// ================= Pins =================
#define SS_PIN      4
#define RST_PIN     5
#define BUZZER_PIN  25

MFRC522 rfid(SS_PIN, RST_PIN);
WiFiClient client;

const uint32_t GATE_ID = 1;   // ← Change to 2 on second board

unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 5000;  // 5 seconds

// ================= Buzzer =================
void beepSuccess() {
  digitalWrite(BUZZER_PIN, HIGH); delay(400); digitalWrite(BUZZER_PIN, LOW);
}
void beepFail() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delay(120); digitalWrite(BUZZER_PIN, LOW); delay(100);
  }
}
void beepError() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delay(80); digitalWrite(BUZZER_PIN, LOW); delay(80);
  }
}

// ================= Keep connection alive =================
bool ensureConnected() {
  if (client.connected()) return true;

  Serial.print("Connecting to server...");
  if (client.connect(serverIP, serverPort)) {
    Serial.println(" OK");
    return true;
  } else {
    Serial.println(" FAILED");
    return false;
  }
}

// ================= Send 9-byte packet =================
uint8_t sendPacket(uint32_t rfidValue) {
  if (!ensureConnected()) {
    return 0;
  }

  uint8_t packet[9];

  // Gate ID
  packet[0] = (GATE_ID >> 24) & 0xFF;
  packet[1] = (GATE_ID >> 16) & 0xFF;
  packet[2] = (GATE_ID >>  8) & 0xFF;
  packet[3] = (GATE_ID      ) & 0xFF;

  // RFID
  packet[4] = (rfidValue >> 24) & 0xFF;
  packet[5] = (rfidValue >> 16) & 0xFF;
  packet[6] = (rfidValue >>  8) & 0xFF;
  packet[7] = (rfidValue      ) & 0xFF;

  uint32_t var1 = GATE_ID;
  uint32_t var2 = rfidValue;
  // Checksum
  // uint8_t cs = 0;
  // for (int i = 0; i < 8; i++) cs += packet[i];
  // packet[8] = cs;
  packet[8] = (var1 ^ var2) & 0xFF;
  client.write(packet, 9);
  client.flush();

  // Only wait for reply when it's a real card
  if (rfidValue != 0) {
    unsigned long start = millis();
    uint8_t buf[9];
    int idx = 0;

    while (millis() - start < 2000) {
      while (client.available() && idx < 9) {
        buf[idx++] = client.read();
      }
      if (idx == 9) break;
      delay(5);
    }

    if (idx == 9) {
      uint8_t calc = 0;
      for (int i = 0; i < 8; i++) calc += buf[i];
      if (calc == buf[8]) {
        uint32_t status = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
        return status;  // 1 = OK, 2 = Not Found
      }
    }
    return 0; // no valid reply
  }

  return 1; // heartbeat sent
}

void setup() {
  Serial.begin(115200);
  delay(800);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  SPI.begin();
  rfid.PCD_Init();

  WiFi.config(local_IP, gateway, subnet, dns);
  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());

  // First connection + heartbeat
  if (ensureConnected()) {
    sendPacket(0);               // RFID = 0 → "I'm live"
    lastHeartbeat = millis();
    Serial.println("First heartbeat sent");
  }

  Serial.println("Ready");
}

void loop() {
  // Reconnect WiFi if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  // Keep TCP connection alive + send heartbeat
  if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    if (sendPacket(0)) {
      Serial.println("Heartbeat OK");
    } else {
      Serial.println("Heartbeat failed - will retry");
      client.stop();          // force reconnect next time
    }
    lastHeartbeat = millis();
  }

  // Read RFID card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  uint32_t rfidValue = 0;
  for (byte i = 0; i < 4; i++) {
    rfidValue = (rfidValue << 8) | rfid.uid.uidByte[i];
  }

  Serial.printf("Card: %08X\n", rfidValue);

  uint8_t result = sendPacket(rfidValue);

  if (result == 1) {
    Serial.println("→ SUCCESS");
    beepSuccess();
  } else if (result == 2) {
    Serial.println("→ NOT FOUND");
    beepFail();
  } else {
    Serial.println("→ ERROR");
    beepError();
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1200);
}