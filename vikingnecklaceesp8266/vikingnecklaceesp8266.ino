/**
 * @file
 * @brief [BSD License] See the LICENSE file for details.
 *
 * This is a sketch for the esp8266 esp-01s based necklace with
 * a chain of WS2812B leds on GPIO2 and a control button on GPIO0.
 * It communicates with the DJ booth over ESP NOW.
 */

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <FastLED.h>

#define NUM_LEDS 5
#define DATA_PIN 2  // GPIO2
#define BUTTON_PIN 0  // GPIO0
#define LONG_PRESS_MS 5000
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
unsigned long buttonPressStart = 0;
bool isButtonHeld = false;
bool localMode = false;

// Replace this with your DJ booth MAC or use broadcast for global
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Command structure
typedef struct struct_command {
  uint8_t mode;         // 0 = go dark, 1 = pattern, 2 = RSSI, 3 = sleep
  uint8_t pattern;      // 0 = chase, 1 = flash, etc.
  uint8_t primary[3];   // RGB
  uint8_t secondary[3];
  uint8_t bpm;
  uint8_t flags;        // Reserved for future use
  uint32_t timestamp;
} struct_command;

struct_command currentCommand;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // GPIO0
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  delay(100);

  // Initialize LEDs
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  // Wi-Fi and ESP-NOW init
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceive);

  // Send join packet
  sendJoin();

  // Start in dark mode until command or local mode takes over
  setDarkMode();
}

void loop() {
  handleButton();

  if (localMode) {
    runLocalPatternCycle();  // Placeholder
  } else {
    runPattern(currentCommand);  // Apply active command
  }

  delay(20);  // Main loop delay
}

void onReceive(uint8_t* mac, uint8_t* data, uint8_t len) {
  if (len == sizeof(currentCommand)) {
    memcpy(&currentCommand, data, len);

    switch (currentCommand.mode) {
      case 0:
        setDarkMode();
        break;
      case 1:
        // Pattern command received
        break;
      case 2:
        sendRSSI(mac); // Pass requester MAC
        break;
      case 3:
        enterDeepSleep();
        break;
    }
  }
}

void sendJoin() {
  // Optionally send a "join" packet with this device's MAC
  uint8_t joinMsg[6];
  WiFi.macAddress(joinMsg);
  esp_now_send(broadcastAddress, joinMsg, sizeof(joinMsg));
}

void handleButton() {
  static bool lastState = HIGH;
  bool currentState = digitalRead(BUTTON_PIN);

  if (lastState == HIGH && currentState == LOW) {
    buttonPressStart = millis();
  }

  if (currentState == LOW && millis() - buttonPressStart > LONG_PRESS_MS) {
    isButtonHeld = true;
    enterDeepSleep();
  }

  if (lastState == LOW && currentState == HIGH && !isButtonHeld) {
    localMode = !localMode;  // Toggle local mode
    Serial.println(localMode ? "Local mode ON" : "Local mode OFF");
  }

  lastState = currentState;
}

void enterDeepSleep() {
  Serial.println("Entering deep sleep...");
  FastLED.clear(); FastLED.show();
  ESP.deepSleep(0);  // Sleep until reset
}

void sendRSSI(uint8_t* destMac) {
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found");
    return;
  }
  struct {
    uint8_t mac[6];
    int8_t rssi;
    uint8_t channel;
  } results[8];
  int count = min(n, 8);
  int idx[32];
  for (int i = 0; i < n; i++) idx[i] = i;
  for (int i = 0; i < n-1; i++) {
    for (int j = i+1; j < n; j++) {
      if (WiFi.RSSI(idx[j]) > WiFi.RSSI(idx[i])) {
        int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
      }
    }
  }
  for (int i = 0; i < count; i++) {
    WiFi.BSSID(idx[i], results[i].mac);
    results[i].rssi = WiFi.RSSI(idx[i]);
    results[i].channel = WiFi.channel(idx[i]);
  }
  esp_now_send(destMac, (uint8_t*)results, count * sizeof(results[0]));
  WiFi.scanDelete();
}

void runLocalPatternCycle() {
  static uint8_t hue = 0;
  fill_rainbow(leds, NUM_LEDS, hue++, 7);
  FastLED.show();
  delay(50);
}

void runPattern(struct_command cmd) {
  switch (cmd.pattern) {
    case 0: // Chase
      static uint8_t chasePos = 0;
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i == chasePos) {
          leds[i] = CRGB(cmd.primary[0], cmd.primary[1], cmd.primary[2]);
        } else {
          leds[i] = CRGB(cmd.secondary[0], cmd.secondary[1], cmd.secondary[2]);
        }
      }
      chasePos = (chasePos + 1) % NUM_LEDS;
      break;
    case 1: // Flash
      static bool flashOn = false;
      flashOn = !flashOn;
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = flashOn ? CRGB(cmd.primary[0], cmd.primary[1], cmd.primary[2])
                         : CRGB(cmd.secondary[0], cmd.secondary[1], cmd.secondary[2]);
      }
      break;
    case 2: // Twinkle
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = (random(2) == 0) ? CRGB(cmd.primary[0], cmd.primary[1], cmd.primary[2])
                                  : CRGB(cmd.secondary[0], cmd.secondary[1], cmd.secondary[2]);
      }
      break;
    case 3: // Fade
      static uint8_t fadeVal = 0;
      static int fadeDir = 1;
      fadeVal += fadeDir * 8;
      if (fadeVal == 0 || fadeVal >= 255) fadeDir = -fadeDir;
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(CRGB(cmd.primary[0], cmd.primary[1], cmd.primary[2]),
                        CRGB(cmd.secondary[0], cmd.secondary[1], cmd.secondary[2]), fadeVal);
      }
      break;
    default: // Solid primary
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(cmd.primary[0], cmd.primary[1], cmd.primary[2]);
      }
      break;
  }
  FastLED.show();
}

void setDarkMode() {
  FastLED.clear();
  FastLED.show();
}
