#include "config.h"   // WHEEL_MAC lives here (git-ignored); copy config.example.h
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include "esp_sleep.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID charUUID("0000ffe1-0000-1000-8000-00805f9b34fb");

static const unsigned long NO_SIGNAL_MS = 60000UL;
static unsigned long searchStartedMs = 0;

static void goToSleep() {
  Serial.println("No Lynx for 60s — deep sleep. Press RESET to wake.");
  Serial.flush();
  delay(50);
  esp_deep_sleep_start();
}

static const size_t BUF_MAX = 256;
static uint8_t frameBuf[BUF_MAX];
static size_t frameLen = 0;
static uint8_t expectTotal = 0;  // LEN + 4 once LEN known

static int16_t rd_i16_be(const uint8_t* p) {
  return (int16_t)((p[0] << 8) | p[1]);
}
static uint16_t rd_u16_be(const uint8_t* p) {
  return (uint16_t)((p[0] << 8) | p[1]);
}

void showHeader() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Lynx HUD");
  display.display();
}

void showHud(float pwmPct, float mph, float kmh, float volts) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.printf("PWM %3.0f%%", pwmPct);
  display.setCursor(0, 22);
  display.printf("%5.1f mph", mph);
  display.setTextSize(1);
  display.setCursor(0, 48);
  display.printf("%5.1f km/h  %5.1f V", kmh, volts);
  display.display();
}

void parseTelemetry(const uint8_t* b, size_t n) {
  // Need at least classic header through PWM (offset 35)
  if (n < 36) return;
  if (b[0] != 0xDC || b[1] != 0x5A || b[2] != 0x5C) return;

  float voltage = rd_u16_be(b + 4) / 100.0f;   // V
  float speedKmh = rd_i16_be(b + 6) / 10.0f;  // 0.1 km/h
  float speedMph = speedKmh * 0.621371f;
  float pwmPct = rd_u16_be(b + 34) / 100.0f;  // 0..100%

  // (model still in the frame if you need it later — just not on the HUD)
  // uint16_t ver = rd_u16_be(b + 28);
  // uint16_t model = ver / 1000;

  static unsigned long lastPrint = 0;
  unsigned long now = millis();
  if (now - lastPrint < 250) return;  // ~4 Hz
  lastPrint = now;

  Serial.println();
  Serial.printf("PWM %3.0f%%\n", pwmPct);
  Serial.printf("%5.1f mph | %5.1f km/h\n", speedMph, speedKmh);
  Serial.printf("%5.1f V\n", voltage);
  showHud(pwmPct, speedMph, speedKmh, voltage);
}

static void onBytes(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    uint8_t byte = data[i];

    // Resync hunt for magic
    if (frameLen == 0) {
      if (byte == 0xDC) { frameBuf[frameLen++] = byte; }
      continue;
    }
    if (frameLen == 1) {
      if (byte == 0x5A) frameBuf[frameLen++] = byte;
      else frameLen = (byte == 0xDC) ? 1 : 0;
      continue;
    }
    if (frameLen == 2) {
      if (byte == 0x5C) frameBuf[frameLen++] = byte;
      else frameLen = (byte == 0xDC) ? 1 : 0;
      continue;
    }
    if (frameLen == 3) {
      frameBuf[frameLen++] = byte;  // LEN
      expectTotal = (uint8_t)(byte + 4);
      if (expectTotal < 8 || expectTotal > BUF_MAX) {
        frameLen = 0;
        expectTotal = 0;
      }
      continue;
    }

    frameBuf[frameLen++] = byte;
    if (expectTotal && frameLen >= expectTotal) {
      parseTelemetry(frameBuf, frameLen);
      frameLen = 0;
      expectTotal = 0;
    }
    if (frameLen >= BUF_MAX) {
      frameLen = 0;
      expectTotal = 0;
    }
  }
}

static void notifyCB(BLERemoteCharacteristic*, uint8_t* p, size_t n, bool) {
  onBytes(p, n);
}

BLEAdvertisedDevice* target = nullptr;
BLEScan* pScan = nullptr;

class Callbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) {
    String mac = d.getAddress().toString().c_str();
    mac.toLowerCase();
    if (mac == WHEEL_MAC ||
        (d.haveName() && String(d.getName().c_str()) == "LK10991")) {
      Serial.println("*** TARGET FOUND ***");
      target = new BLEAdvertisedDevice(d);
      pScan->stop();
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Lynx speed + PWM");
  BLEDevice::init("");
  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new Callbacks());
  pScan->setActiveScan(true);
  pScan->setDuplicateFilter(false);
  searchStartedMs = millis();
  Serial.println("Scan window: 60s, then sleep");

  Wire.begin(SDA, SCL);  // board's default D4/D5
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Lynx HUD");
    display.display();
  }
}

void loop() {
  if (millis() - searchStartedMs >= NO_SIGNAL_MS) {
    goToSleep();
  }
  target = nullptr;
  frameLen = 0;
  expectTotal = 0;

  Serial.println("--- scanning ---");
  pScan->start(10, false);
  pScan->clearResults();
  if (!target) {
    Serial.println("not found (wake wheel)");
    delay(2000);
    return;
  }

  delay(200);
  BLEClient* client = BLEDevice::createClient();
  if (!client->connect(target)) {
    Serial.println("connect failed");
    delete target;
    delay(2000);
    return;
  }
  Serial.println("CONNECTED — streaming speed/PWM (roll the wheel to see speed)");

  BLERemoteService* svc = client->getService(serviceUUID);
  BLERemoteCharacteristic* ch = svc ? svc->getCharacteristic(charUUID) : nullptr;
  if (!ch || !ch->canNotify()) {
    Serial.println("ffe1 notify missing");
    client->disconnect();
    delete target;
    delay(3000);
    return;
  }
  ch->registerForNotify(notifyCB);

  // Stay connected; reconnect loop if dropped
  while (client->isConnected()) {
    delay(50);
  }
  Serial.println("disconnected");
  showHeader();
  delete target;
  target = nullptr;
  searchStartedMs = millis();  // new 60s window to find the wheel again
  delay(1000);
}