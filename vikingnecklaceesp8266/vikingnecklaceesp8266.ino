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
unsigned long powerBudget = 35;  // In mA
unsigned long ledValueScale = 10;  // 1/10 for max brightness per channel
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

unsigned long lastPatternUpdate = 0;
unsigned long patternInterval = 50; // Default ms

bool rssiScanRequested = false;
uint8_t rssiScanDestMac[6];

// Power management constants
const uint8_t MAX_CURRENT_PER_LED_mA = 60;  // Maximum current per LED at full white
const uint8_t BASE_CURRENT_mA = 20;         // ESP8266 base current consumption

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // GPIO0
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  //Serial.begin(115200);
  // Serial.begin(74880);
  delay(100);

  // Initialize LEDs
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  // Wi-Fi and ESP-NOW init
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    // Serial.println("ESP-NOW init failed");
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

  // Set pattern interval based on BPM
  if (currentCommand.bpm > 0) {
    patternInterval = 60000UL / currentCommand.bpm;
  } else {
    patternInterval = 50; // fallback default
  }

  unsigned long now = millis();
  if (localMode) {
    if (now - lastPatternUpdate >= patternInterval) {
      runLocalPatternCycle();
      lastPatternUpdate = now;
    }
  } else {
    if (now - lastPatternUpdate >= patternInterval) {
      runPattern(currentCommand);
      lastPatternUpdate = now;
    }
  }

  // Handle asynchronous RSSI scan
  int scanStatus = WiFi.scanComplete();
  if (rssiScanRequested && scanStatus >= 0) {
    int n = scanStatus;
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
      uint8_t* bssid = WiFi.BSSID(idx[i]);
      memcpy(results[i].mac, bssid, 6);
      results[i].rssi = WiFi.RSSI(idx[i]);
      results[i].channel = WiFi.channel(idx[i]);
    }
    esp_now_send(rssiScanDestMac, (uint8_t*)results, count * sizeof(results[0]));
    WiFi.scanDelete();
    rssiScanRequested = false;
  } else if (rssiScanRequested && scanStatus == -2) {
    // Scan not started, start it now
    WiFi.scanNetworks(true);
  }

  delay(20);  // Main loop delay
}

void onReceive(uint8_t* mac, uint8_t* data, uint8_t len) {
  if (len == sizeof(currentCommand)) {
    struct_command *cmd = (struct_command*)data;
    switch (cmd->mode) {
      case 0:
        setDarkMode();
        currentCommand.mode = 0;
        break;
      case 1:
        // Pattern command received
        memcpy(&currentCommand, data, len);
        runPattern(currentCommand);
        break;
      case 2:
        memcpy(rssiScanDestMac, mac, 6);
        rssiScanRequested = true;
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
    if (!localMode) {
      setDarkMode();
    }
    // Serial.println(localMode ? "Local mode ON" : "Local mode OFF");
  }

  lastState = currentState;
}

void enterDeepSleep() {
  //Serial.println("Entering deep sleep...");
  FastLED.clear(); FastLED.show();
  ESP.deepSleep(0);  // Sleep until reset
}

void runLocalPatternCycle() {
  static uint8_t hue = 0;
  fill_rainbow(leds, NUM_LEDS, hue++, 7);
  // Apply power budget-aware brightness
  uint8_t safeBrightness = calculateSafeBrightness();
  FastLED.setBrightness(safeBrightness);
  FastLED.show();
  delay(50);
}

// Calculate actual current draw of current LED state
uint16_t calculateCurrentDraw() {
  uint32_t totalBrightness = 0;
  
  for (int i = 0; i < NUM_LEDS; i++) {
    // Calculate brightness as sum of R+G+B values
    uint16_t ledBrightness = leds[i].r + leds[i].g + leds[i].b;
    totalBrightness += ledBrightness;
  }
  
  // Calculate current: (total brightness / max possible brightness) * max current
  uint16_t maxPossibleBrightness = NUM_LEDS * 765; // 765 = 255*3 (max R+G+B)
  uint16_t ledCurrent = (totalBrightness * (NUM_LEDS * MAX_CURRENT_PER_LED_mA)) / maxPossibleBrightness;
  
  return BASE_CURRENT_mA + ledCurrent;
}

// Calculate safe brightness level to stay within power budget
uint8_t calculateSafeBrightness() {
  // First, calculate what current draw would be at full brightness
  uint16_t fullBrightnessCurrent = calculateCurrentDraw();
  
  if (fullBrightnessCurrent <= powerBudget) {
    return 255; // Full brightness is safe
  }
  
  // Calculate scaling factor to stay within budget
  uint16_t availableCurrent = powerBudget - BASE_CURRENT_mA;
  uint16_t ledCurrent = fullBrightnessCurrent - BASE_CURRENT_mA;
  
  if (ledCurrent == 0) return 255;
  
  uint16_t scaleFactor = (availableCurrent * 255) / ledCurrent;
  return (scaleFactor > 255) ? 255 : (uint8_t)scaleFactor;
}

void runPattern(struct_command cmd) {
  if(cmd.mode == 0) return;
  
  switch (cmd.pattern) {
    case 0: // Chase
      static uint8_t chasePos = 0;
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i == chasePos) {
          leds[i] = CRGB(cmd.primary[0] / ledValueScale, cmd.primary[1] / ledValueScale, cmd.primary[2] / ledValueScale);
        } else {
          leds[i] = CRGB(cmd.secondary[0] / ledValueScale, cmd.secondary[1] / ledValueScale, cmd.secondary[2] / ledValueScale);
        }
      }
      chasePos = (chasePos + 1) % NUM_LEDS;
      break;
    case 1: // Flash
      static bool flashOn = false;
      flashOn = !flashOn;
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = flashOn ? CRGB(cmd.primary[0] / ledValueScale, cmd.primary[1] / ledValueScale, cmd.primary[2] / ledValueScale)
                         : CRGB(cmd.secondary[0] / ledValueScale, cmd.secondary[1] / ledValueScale, cmd.secondary[2] / ledValueScale);
      }
      break;
    case 2: // Twinkle
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = (random(2) == 0) ? CRGB(cmd.primary[0] / ledValueScale, cmd.primary[1] / ledValueScale, cmd.primary[2] / ledValueScale)
                                  : CRGB(cmd.secondary[0] / ledValueScale, cmd.secondary[1] / ledValueScale, cmd.secondary[2] / ledValueScale);
      }
      break;
    case 3: // Fade
      static uint8_t fadeVal = 0;
      static int fadeDir = 1;
      fadeVal += fadeDir * 8;
      if (fadeVal == 0 || fadeVal >= 255) fadeDir = -fadeDir;
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(
          CRGB(cmd.primary[0] / ledValueScale, cmd.primary[1] / ledValueScale, cmd.primary[2] / ledValueScale),
          CRGB(cmd.secondary[0] / ledValueScale, cmd.secondary[1] / ledValueScale, cmd.secondary[2] / ledValueScale),
          fadeVal);
      }
      break;
    default: // Solid primary
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(cmd.primary[0] / ledValueScale, cmd.primary[1] / ledValueScale, cmd.primary[2] / ledValueScale);
      }
      break;
  }
  // Apply power budget-aware brightness
  uint8_t safeBrightness = calculateSafeBrightness();
  FastLED.setBrightness(safeBrightness);
  FastLED.show();
  
  // Optional: Debug output
  // if (Serial) {
  //   uint16_t currentDraw = calculateCurrentDraw();
  //   Serial.printf("Current draw: %dmA, Budget: %dmA, Brightness: %d/255\n", 
  //                 currentDraw, (int)powerBudget, safeBrightness);
  // }
}

void setDarkMode() {
  FastLED.clear();
  FastLED.show();
}

// Add function to set power budget via serial or ESP-NOW
void setPowerBudget(uint16_t budgetmA) {
  powerBudget = constrain(budgetmA, BASE_CURRENT_mA + 10, 1000); // Reasonable limits
  // Serial.printf("Power budget set to %dmA\n", (int)powerBudget);
}
