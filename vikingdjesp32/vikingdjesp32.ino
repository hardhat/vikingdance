/**
 * @file vikingdjesp32
 * @brief [BSD License] See the LICENSE file for details.
 *
 * This is a sketch for an ESP32 attached by USB to a DJ laptop.  
 * It controls the other ESP8266 based necklaces via ESP-NOW.
 * It communciates with the DJ laptop via serial commands.
 *
 */
#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h>
#include <SPI.h>

typedef struct struct_command {
  uint8_t mode;
  uint8_t pattern;
  uint8_t primary[3];
  uint8_t secondary[3];
  uint8_t bpm;
  uint8_t flags;
  uint32_t timestamp;
} struct_command;

struct_command currentCommand;
uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
TFT_eSPI tft = TFT_eSPI();
String lastStatus = "";

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  char macStr[18];
  const uint8_t *mac = info->src_addr;
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("[NECKLACE] From: ");
  Serial.print(macStr);
  Serial.print(" | Bytes: ");
  Serial.print(len);
  Serial.print(" | Data: ");
  for (int i = 0; i < len; i++) {
    Serial.printf("%02X ", incomingData[i]);
  }
  Serial.println();
  // Optionally, parse known message types (e.g., RSSI, join, etc.)
}

void setup() {
  Serial.begin(115200);
  Serial.println("Hey, what's up?");
  WiFi.mode(WIFI_STA);

  tft.init();
  tft.setRotation(1); // Landscape, USB at top
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2); //2
  tft.setCursor(10, 10);
  tft.println("Viking DJ Booth");
  tft.setTextSize(1); //1
  tft.setCursor(10, 35);
  tft.println("ESP-NOW Ready");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(10, 55);
    tft.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddr, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(10, 70);
    tft.println("Failed to add peer");
    return;
  }

  Serial.println("DJ Booth ready. Type commands:");
  Serial.println("Example: SET 255 0 0 0 0 255 120 1");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 90);
  tft.println("Ready for commands");
}

void updateDisplay(const String& status) {
  tft.fillRect(0, 105, 240, 30, TFT_BLACK); // Clear lower part (y=105 to y=134)
  tft.setCursor(10, 110);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print("Last: ");
  tft.println(status);
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    parseAndSend(line);
  }
}

void parseAndSend(String line) {
  line.trim();
  if (line.startsWith("SET")) {
    int vals[9];
    int i = 0;
    char *token = strtok((char*)line.c_str(), " ");
    while (token && i < 9) {
      token = strtok(NULL, " ");
      if (token) vals[i++] = atoi(token);
    }
    currentCommand.mode = 1;
    currentCommand.pattern = vals[0];
    currentCommand.primary[0] = vals[1];
    currentCommand.primary[1] = vals[2];
    currentCommand.primary[2] = vals[3];
    currentCommand.secondary[0] = vals[4];
    currentCommand.secondary[1] = vals[5];
    currentCommand.secondary[2] = vals[6];
    currentCommand.bpm = vals[7];
    currentCommand.flags = vals[8];
    currentCommand.timestamp = millis();
    esp_now_send(broadcastAddr, (uint8_t *)&currentCommand, sizeof(currentCommand));
    Serial.println("Broadcast command sent");
    lastStatus = "SET sent";
    updateDisplay(line);
  } else if (line == "DARK") {
    currentCommand.mode = 0;
    esp_now_send(broadcastAddr, (uint8_t *)&currentCommand, sizeof(currentCommand));
    Serial.println("Go dark command sent");
    lastStatus = "DARK sent";
    updateDisplay(line);
  } else if (line == "RSSI") {
    currentCommand.mode = 2;
    esp_now_send(broadcastAddr, (uint8_t *)&currentCommand, sizeof(currentCommand));
    Serial.println("RSSI scan request sent");
    lastStatus = "RSSI sent";
    updateDisplay(line);
  } else if (line == "SLEEP") {
    currentCommand.mode = 3;
    esp_now_send(broadcastAddr, (uint8_t *)&currentCommand, sizeof(currentCommand));
    Serial.println("Sleep command sent");
    lastStatus = "SLEEP sent";
    updateDisplay(line);
  } else {
    Serial.println("Unknown command. Use SET, DARK, RSSI, or SLEEP.");
    lastStatus = "Unknown command";
    updateDisplay(line);
  }
}
