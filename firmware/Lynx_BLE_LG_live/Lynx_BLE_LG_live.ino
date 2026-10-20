#include "config.h"   // WHEEL_MAC lives here (git-ignored); copy config.example.h
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include "esp_sleep.h"
#include "esp_system.h"
#include "driver/rtc_io.h"
#include "driver/usb_serial_jtag.h"
#include <LilyGo_AMOLED.h>
#include <TFT_eSPI.h>

LilyGo_Class amoled;
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID charUUID("0000ffe1-0000-1000-8000-00805f9b34fb");

static const unsigned long NO_SIGNAL_MS = 60000UL;
static unsigned long searchStartedMs = 0;

static const size_t BUF_MAX = 256;
static uint8_t frameBuf[BUF_MAX];
static size_t frameLen = 0;
static uint8_t expectTotal = 0;

static int16_t rd_i16_be(const uint8_t* p) { return (int16_t)((p[0] << 8) | p[1]); }
static uint16_t rd_u16_be(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }

// ---------- HUD battery (the LiPo in the case, not the wheel) ----------
int  gBattPct = -1;
bool gCharging = false;

void readHudBattery() {
  static uint32_t last = 0;
  if (millis() - last < 1000 && gBattPct >= 0) return;
  last = millis();
  if (!amoled.isBatteryConnect()) { gBattPct = -1; return; }
  gBattPct  = amoled.getBatteryPercent();
  gCharging = amoled.isCharging();
}

void drawBattBadge() {
  readHudBattery();
  if (gBattPct < 0) return;               // no battery, draw nothing
  char b[8];
  snprintf(b, sizeof(b), "%s%d%%", gCharging ? "+" : "", gBattPct);
  uint16_t c = gBattPct > 50 ? TFT_GREEN : gBattPct > 20 ? TFT_YELLOW : TFT_RED;
  spr.setTextDatum(TR_DATUM);             // anchor at top-right
  spr.setTextColor(c, TFT_BLACK);
  spr.drawString(b, amoled.width() - 4, 4, 2);
}

// ---------- Display ----------
void drawHud(float pwmPct, float mph, float volts) {
  const int w = amoled.width();
  const int h = amoled.height();
  const int mx = w / 2;
  const int my = h / 2;
  const int pad = 4;

  spr.fillSprite(TFT_BLACK);
  spr.drawFastVLine(mx, 0, h, TFT_DARKGREY);
  spr.drawFastHLine(0, my, mx, TFT_DARKGREY);

  // Left-top: voltage
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  char vbuf[16];
  snprintf(vbuf, sizeof(vbuf), "%.1f", volts);
  spr.drawString(vbuf, mx / 2, my / 2, 6);

  // Left-bottom: mph + yellow unit
  char sbuf[16];
  snprintf(sbuf, sizeof(sbuf), "%.1f", mph);
  int tw = spr.textWidth(sbuf, 6);
  int uw = spr.textWidth("mph", 2);
  int sx = mx - pad - (tw + 4 + uw);
  if (sx < pad) sx = pad;
  int sy = my + my / 2;

  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(sbuf, sx, sy, 6);
  spr.setTextColor(TFT_YELLOW, TFT_BLACK);
  spr.drawString("mph", sx + tw + 4, sy, 2);

  // Right: font 8 PWM, % fixed on the right edge, colour by level
  char pbuf[8];
  snprintf(pbuf, sizeof(pbuf), "%.0f", pwmPct);
  int pw = spr.textWidth(pbuf, 8);
  int pctW = spr.textWidth("%", 4);
  int pctX = w - pad - pctW;
  int px = pctX - 4 - pw;
  if (px < mx + pad) px = mx + pad;
  int py = h / 2;

  uint16_t pwmColor = TFT_YELLOW;
  if (pwmPct >= 80)      pwmColor = TFT_RED;
  else if (pwmPct >= 60) pwmColor = TFT_ORANGE;
  spr.setTextColor(pwmColor, TFT_BLACK);
  spr.drawString(pbuf, px, py, 8);
  spr.drawString("%", pctX, py, 4);

  spr.setTextDatum(TL_DATUM);
  drawBattBadge();
  amoled.pushColors(0, 0, w, h, (uint16_t *)spr.getPointer());
}

void drawStatus(const char* msg) {
  const int w = amoled.width();
  const int h = amoled.height();
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  spr.drawString("Lynx HUD", w / 2, h / 2 - 20, 4);
  spr.setTextColor(TFT_YELLOW, TFT_BLACK);
  spr.drawString(msg, w / 2, h / 2 + 20, 4);
  spr.setTextDatum(TL_DATUM);
  drawBattBadge();
  amoled.pushColors(0, 0, w, h, (uint16_t *)spr.getPointer());
}

// ---------- Telemetry ----------
volatile float gPwm = 0, gMph = 0, gVolts = 0;
volatile bool gNewData = false;

void parseTelemetry(const uint8_t* b, size_t n) {
  if (n < 36) return;
  if (b[0] != 0xDC || b[1] != 0x5A || b[2] != 0x5C) return;
  gVolts = rd_u16_be(b + 4) / 100.0f;
  gMph   = rd_i16_be(b + 6) / 10.0f * 0.621371f;
  gPwm   = rd_u16_be(b + 34) / 100.0f;
  gNewData = true;   // main loop draws it
}

static void onBytes(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    uint8_t byte = data[i];
    if (frameLen == 0) { if (byte == 0xDC) frameBuf[frameLen++] = byte; continue; }
    if (frameLen == 1) { if (byte == 0x5A) frameBuf[frameLen++] = byte; else frameLen = (byte == 0xDC) ? 1 : 0; continue; }
    if (frameLen == 2) { if (byte == 0x5C) frameBuf[frameLen++] = byte; else frameLen = (byte == 0xDC) ? 1 : 0; continue; }
    if (frameLen == 3) {
      frameBuf[frameLen++] = byte;
      expectTotal = (uint8_t)(byte + 4);
      if (expectTotal < 8 || expectTotal > BUF_MAX) { frameLen = 0; expectTotal = 0; }
      continue;
    }
    frameBuf[frameLen++] = byte;
    if (expectTotal && frameLen >= expectTotal) {
      parseTelemetry(frameBuf, frameLen);
      frameLen = 0; expectTotal = 0;
    }
    if (frameLen >= BUF_MAX) { frameLen = 0; expectTotal = 0; }
  }
}

static void notifyCB(BLERemoteCharacteristic*, uint8_t* p, size_t n, bool) {
  onBytes(p, n);
}

BLEAdvertisedDevice* target = nullptr;
BLEScan* pScan = nullptr;
BLEClient* client = nullptr;

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

// ---------- Sleep ----------
void goToSleep() {
  if (usb_serial_jtag_is_connected()) {
    Serial.println("USB connected, staying awake");
    return;
  }
  Serial.println("Sleeping now...");
  Serial.flush();
  if (client && client->isConnected()) { client->disconnect(); delay(200); }
  if (pScan) pScan->stop();

  spr.fillSprite(TFT_BLACK);
  amoled.pushColors(0, 0, amoled.width(), amoled.height(), (uint16_t *)spr.getPointer());
  amoled.setBrightness(0);
  amoled.sleep(true);   // screen + its power off, sensors and measuring off, touch asleep
  delay(50);

  rtc_gpio_pullup_en(GPIO_NUM_0);
  rtc_gpio_pulldown_dis(GPIO_NUM_0);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  esp_deep_sleep_start();
}

// Hold BOOT for 2 s to sleep. Returns true if it tried (USB kept it awake).
bool checkBootButton() {
  static uint32_t downSince = 0;
  if (digitalRead(0) == HIGH) { downSince = 0; return false; }
  if (downSince == 0) downSince = millis();
  if (millis() - downSince < 2000) return false;
  drawStatus("Release to sleep");
  uint32_t upSince = millis();
  while (millis() - upSince < 500) {
    if (digitalRead(0) == LOW) upSince = millis();
    delay(5);
  }
  downSince = 0;
  goToSleep();
  return true;
}

// ---------- Setup / loop ----------
void setup() {
  Serial.begin(115200);
  rtc_gpio_deinit(GPIO_NUM_0);      // undo sleep's pin setup
  pinMode(0, INPUT_PULLUP);         // BOOT: hold to sleep, tap to wake
  pinMode(21, INPUT_PULLUP);        // IO21: spare, for pages later

  if (!amoled.beginAMOLED_147()) {
    while (true) delay(1000);
  }
  amoled.setBrightness(180);
  spr.createSprite(amoled.width(), amoled.height());
  spr.setSwapBytes(1);

  esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
  int rr = (int)esp_reset_reason();
  Serial.printf("Wake cause %d, reset reason %d\n", (int)why, rr);
  const char* msg;
  if (why == ESP_SLEEP_WAKEUP_EXT0)      msg = "Woke up";
  else if (rr == ESP_RST_POWERON)        msg = "Powered on";
  else if (rr == ESP_RST_USB)            msg = "Updated";
  else if (rr == ESP_RST_BROWNOUT)       msg = "Low power!";
  else                                   msg = "Restarted";
  drawStatus(msg);
  delay(2000);

  drawStatus("Scanning...");
  BLEDevice::init("");
  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new Callbacks());
  pScan->setActiveScan(true);
  pScan->setDuplicateFilter(false);
  searchStartedMs = millis();
}

void loop() {
  checkBootButton();
  if (millis() - searchStartedMs >= NO_SIGNAL_MS) {
    goToSleep();
    searchStartedMs = millis();   // on USB: restart the timer instead of spamming
  }

  target = nullptr;
  frameLen = 0;
  expectTotal = 0;

  drawStatus("Scanning...");
  pScan->start(5, false);          // shorter scan so the button responds sooner
  pScan->clearResults();
  if (!target) {
    Serial.println("not found (wake wheel)");
    for (int i = 0; i < 40; i++) { checkBootButton(); delay(50); }
    return;
  }

  drawStatus("Connecting...");
  delay(200);
  if (!client) client = BLEDevice::createClient();
  if (!client->connect(target)) {
    Serial.println("connect failed");
    delete target; target = nullptr;
    delay(2000);
    return;
  }

  BLERemoteService* svc = client->getService(serviceUUID);
  BLERemoteCharacteristic* ch = svc ? svc->getCharacteristic(charUUID) : nullptr;
  if (!ch || !ch->canNotify()) {
    Serial.println("ffe1 notify missing");
    client->disconnect();
    delete target; target = nullptr;
    delay(3000);
    return;
  }
  Serial.println("CONNECTED, streaming");
  ch->registerForNotify(notifyCB);

  uint32_t lastDraw = 0, lastPrint = 0;
  while (client->isConnected()) {
    if (checkBootButton()) drawStatus("USB: staying awake");
    uint32_t now = millis();
    if (gNewData && now - lastDraw >= 100) {   // about 10 frames a second
      gNewData = false;
      lastDraw = now;
      drawHud(gPwm, gMph, gVolts);
      if (now - lastPrint >= 1000) {
        lastPrint = now;
        Serial.printf("PWM %3.0f%% | %5.1f mph | %5.1f V\n", gPwm, gMph, gVolts);
      }
    }
    delay(10);
  }

  Serial.println("disconnected");
  drawStatus("Lost link");
  delete target; target = nullptr;
  searchStartedMs = millis();
  delay(1000);
}