#include <EtherCard.h>
#include <Adafruit_NeoPixel.h>

// ========== DISPLAY IDENTITY ==========
#define DISPLAY_ID      1          // ID of this 7-segment display (1..255). ID 0 in a command = all displays

// ========== LED STRIPS ==========
#define LEFT_PIN    3
#define RIGHT_PIN   2
#define NUM_LEDS    30
#define BRIGHTNESS  255

Adafruit_NeoPixel leftStrip(NUM_LEDS, LEFT_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rightStrip(NUM_LEDS, RIGHT_PIN, NEO_GRB + NEO_KHZ800);

// ========== COLORS ==========
#define COLOR_OFF         leftStrip.Color(0, 0, 0)
#define COLOR_RED         leftStrip.Color(255, 0, 0)
#define COLOR_GREEN       leftStrip.Color(0, 255, 0)
#define COLOR_BLUE        leftStrip.Color(0, 0, 255)
#define COLOR_WHITE       leftStrip.Color(255, 255, 255)
#define COLOR_YELLOW      leftStrip.Color(255, 255, 0)
#define COLOR_CYAN        leftStrip.Color(0, 255, 255)
#define COLOR_MAGENTA     leftStrip.Color(255, 0, 255)
#define COLOR_ORANGE      leftStrip.Color(255, 100, 0)
#define COLOR_PURPLE      leftStrip.Color(128, 0, 255)
#define COLOR_PINK        leftStrip.Color(255, 20, 147)
#define COLOR_LIME        leftStrip.Color(50, 255, 50)
#define COLOR_GOLD        leftStrip.Color(255, 215, 0)

uint32_t currentColor = COLOR_RED;
int shownValue = -1;               // -1 = nothing / all-on / off

// Segments
const int segG[] = {1,5}, segB[] = {5,9}, segA[] = {9,13}, segF[] = {13,17};
const int segE[] = {18,22}, segD[] = {22,26}, segC[] = {26,30};

const byte digitPatterns[10] = {
  0b1111110, 0b0110000, 0b1101101, 0b1111001, 0b0110011,
  0b1011011, 0b1011111, 0b1110000, 0b1111111, 0b1111011
};

// ========== NETWORK CONFIG ==========
static byte mymac[] = { 0x74, 0x69, 0x69, 0x2D, 0x30, 0x31 };   // change last byte if you use several Arduinos
static byte myip[]  = { 192, 168, 1, 177 };    // Arduino IP (unique per display)
static byte gwip[]  = { 192, 168, 1, 1 };      // Gateway
static byte mask[]  = { 255, 255, 255, 0 };    // Netmask

// PC settings
static byte pcip[]  = { 192, 168, 1, 100 };    // PC IP (used when HB_BROADCAST = 0)
static byte bcast[] = { 255, 255, 255, 255 };  // broadcast address

#define PC_PORT          5000                  // PC listens here for heartbeats
#define LISTEN_PORT      5001                  // Arduino listens here for commands
#define HB_SRC_PORT      5002                  // source port for heartbeats
#define HB_INTERVAL      1000UL                // heartbeat every 1 second

// 1 = heartbeat sent as broadcast (no ARP needed, PC app discovers the Arduino automatically)
// 0 = heartbeat sent directly to pcip
#define HB_BROADCAST     1

byte Ethernet::buffer[400];
unsigned long lastHb = 0;

// ========== LED FUNCTIONS ==========
void lightSegment(Adafruit_NeoPixel &strip, const int range[], bool on) {
  uint32_t color = on ? currentColor : COLOR_OFF;
  for (int i = range[0]; i < range[1]; i++) strip.setPixelColor(i, color);
}

void showDigit(Adafruit_NeoPixel &strip, int num) {
  if (num < 0 || num > 9) return;
  byte p = digitPatterns[num];
  strip.setPixelColor(0, COLOR_OFF);
  strip.setPixelColor(17, COLOR_OFF);
  lightSegment(strip, segA, p & 0b1000000);
  lightSegment(strip, segB, p & 0b0100000);
  lightSegment(strip, segC, p & 0b0010000);
  lightSegment(strip, segD, p & 0b0001000);
  lightSegment(strip, segE, p & 0b0000100);
  lightSegment(strip, segF, p & 0b0000010);
  lightSegment(strip, segG, p & 0b0000001);
  strip.show();
}

void showNumber(int number) {
  if (number < 0) number = 0;
  if (number > 99) number = 99;
  shownValue = number;
  showDigit(leftStrip, number / 10);
  showDigit(rightStrip, number % 10);
}

void allOn() {
  for (int i = 1; i < 17; i++) {
    leftStrip.setPixelColor(i, currentColor);
    rightStrip.setPixelColor(i, currentColor);
  }
  for (int i = 18; i < 30; i++) {
    leftStrip.setPixelColor(i, currentColor);
    rightStrip.setPixelColor(i, currentColor);
  }
  leftStrip.setPixelColor(0, COLOR_OFF);  leftStrip.setPixelColor(17, COLOR_OFF);
  rightStrip.setPixelColor(0, COLOR_OFF); rightStrip.setPixelColor(17, COLOR_OFF);
  leftStrip.show(); rightStrip.show();
  shownValue = -1;
}

void clearAll() {
  leftStrip.clear(); rightStrip.clear();
  leftStrip.show(); rightStrip.show();
  shownValue = -1;
}

// ========== COMMAND HANDLER ==========
// cmd examples: "42" (show number), "A" (all on), "S" (all off), "R"/"G"/"B"... (color)
void processCommand(const char* cmd) {
  String input = String(cmd);
  input.trim();
  if (input.length() == 0) return;

  if (input.equalsIgnoreCase("A")) {
    allOn();
    Serial.println(F("All ON"));
  }
  else if (input.equalsIgnoreCase("S")) {
    clearAll();
    Serial.println(F("All OFF"));
  }
  else if (input.length() == 1 && !isDigit(input.charAt(0))) {
    char ch = toupper(input.charAt(0));
    if      (ch == 'R') currentColor = COLOR_RED;
    else if (ch == 'G') currentColor = COLOR_GREEN;
    else if (ch == 'B') currentColor = COLOR_BLUE;
    else if (ch == 'W') currentColor = COLOR_WHITE;
    else if (ch == 'Y') currentColor = COLOR_YELLOW;
    else if (ch == 'C') currentColor = COLOR_CYAN;
    else if (ch == 'M') currentColor = COLOR_MAGENTA;
    else if (ch == 'O') currentColor = COLOR_ORANGE;
    else if (ch == 'P') currentColor = COLOR_PURPLE;
    else if (ch == 'K') currentColor = COLOR_PINK;
    else if (ch == 'L') currentColor = COLOR_LIME;
    else if (ch == 'Q') currentColor = COLOR_GOLD;
    Serial.print(F("Color changed to ")); Serial.println(ch);
    if (shownValue >= 0) showNumber(shownValue);   // redraw number in the new color
  }
  else if (isDigit(input.charAt(0))) {
    int num = input.toInt();
    showNumber(num);
    Serial.print(F("Showing: ")); Serial.println(num);
  }
}

// Message format from PC: "<id>,<command>"   e.g. "1,42"  "1,R"  "0,S" (0 = all displays)
void handleMessage(char* msg) {
  char* comma = strchr(msg, ',');
  if (!comma) return;
  *comma = 0;
  int id = atoi(msg);
  if (id != DISPLAY_ID && id != 0) return;         // not for me
  processCommand(comma + 1);
}

// ========== UDP RECEIVE CALLBACK ==========
// (EtherCard versions with src_port in the callback)
void onUdp(uint16_t dest_port, uint8_t src_ip[IP_LEN], uint16_t src_port, const char *data, uint16_t len) {
  char buf[24];
  uint16_t n = (len < sizeof(buf) - 1) ? len : sizeof(buf) - 1;
  memcpy(buf, data, n);
  buf[n] = 0;
  Serial.print(F("RX: ")); Serial.println(buf);
  handleMessage(buf);
}

// ========== HEARTBEAT ==========
// Message: "HB,<id>,<value>"   value = currently shown number or -1
void sendHeartbeat() {
  char msg[20];
  int n = snprintf(msg, sizeof(msg), "HB,%d,%d", DISPLAY_ID, shownValue);
#if HB_BROADCAST
  ether.sendUdp(msg, n, HB_SRC_PORT, bcast, PC_PORT);
#else
  ether.sendUdp(msg, n, HB_SRC_PORT, pcip, PC_PORT);
#endif
}

// ========== SETUP / LOOP ==========
void setup() {
  Serial.begin(9600);
  Serial.println(F("\n=== Arduino UDP Display ==="));
  Serial.print(F("Display ID: ")); Serial.println(DISPLAY_ID);

  leftStrip.begin();
  rightStrip.begin();
  leftStrip.setBrightness(BRIGHTNESS);
  rightStrip.setBrightness(BRIGHTNESS);
  clearAll();

  // LED self-test
  allOn();
  delay(500);
  showNumber(88);
  delay(500);
  clearAll();

  if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0) {
    Serial.println(F("ENC28J60 not found"));
    while (1);
  }
  Serial.println(F("ENC28J60 OK"));

  ether.staticSetup(myip, gwip, 0, mask);          // 0 = no DNS
  ether.copyIp(ether.hisip, pcip);

  ether.printIp("My IP: ", ether.myip);
  ether.printIp("Mask: ", ether.netmask);
  ether.printIp("Gateway: ", ether.gwip);
  Serial.print(F("Heartbeat -> port ")); Serial.println(PC_PORT);
  Serial.print(F("Listening on port ")); Serial.println(LISTEN_PORT);

  // Resolve the gateway MAC (ARP) before the first send
  Serial.println(F("Resolving gateway..."));
  unsigned long t = millis();
  while (ether.clientWaitingGw()) {
    ether.packetLoop(ether.packetReceive());
    if (millis() - t > 5000) {
      Serial.println(F("Gateway ARP timeout"));
      break;
    }
  }
  if (!ether.clientWaitingGw()) Serial.println(F("Gateway resolved"));

  ether.udpServerListenOnPort(&onUdp, LISTEN_PORT);
  lastHb = millis();
}

void loop() {
  ether.packetLoop(ether.packetReceive());         // incoming UDP handled by onUdp()

  if (millis() - lastHb >= HB_INTERVAL) {
    lastHb += HB_INTERVAL;
    sendHeartbeat();
  }
}
