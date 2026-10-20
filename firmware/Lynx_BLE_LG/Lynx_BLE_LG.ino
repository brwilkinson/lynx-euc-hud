#include <LilyGo_AMOLED.h>
#include <TFT_eSPI.h>

// 0 = fake telemetry (no wheel); 1 = real Lynx BLE
#define USE_BLE 0

LilyGo_Class amoled;
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

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

  // Left-bottom: mph + yellow unit, packed toward the divider
  char sbuf[16];
  snprintf(sbuf, sizeof(sbuf), "%.1f", mph);
  const int sFont = 6;
  const int uFont = 2;
  int tw = spr.textWidth(sbuf, sFont);
  int uw = spr.textWidth("mph", uFont);
  int total = tw + 4 + uw;
  int sx = mx - pad - total;
  if (sx < pad) sx = pad;
  int sy = my + my / 2;

  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(sbuf, sx, sy, sFont);
  spr.setTextColor(TFT_YELLOW, TFT_BLACK);
  spr.drawString("mph", sx + tw + 4, sy, uFont);

  // Right: font 8 PWM, % fixed on the right edge
  char pbuf[8];
  snprintf(pbuf, sizeof(pbuf), "%.0f", pwmPct);
  int pw = spr.textWidth(pbuf, 8);
  int pctW = spr.textWidth("%", 4);
  int pctX = w - pad - pctW;
  int px = pctX - 4 - pw;
  if (px < mx + pad) px = mx + pad;
  int py = h / 2;

  spr.setTextColor(TFT_YELLOW, TFT_BLACK);
  spr.drawString(pbuf, px, py, 8);
  spr.drawString("%", pctX, py, 4);

  spr.setTextDatum(TL_DATUM);
  amoled.pushColors(0, 0, w, h, (uint16_t *)spr.getPointer());
}

#if USE_BLE
// Lynx BLE goes here next: ffe0/ffe1 notify, DC 5A 5C frames,
// parseTelemetry calls drawHud(pwmPct, speedMph, voltage)
#endif

void setup() {
  Serial.begin(115200);
  if (!amoled.beginAMOLED_147()) {
    while (true) delay(1000);
  }
  amoled.setBrightness(180);
  spr.createSprite(amoled.width(), amoled.height());
  spr.setSwapBytes(1);

#if !USE_BLE
  Serial.println("DEMO mode, no BLE");
#endif

  drawHud(88.0f, 34.0f, 147.0f);  // layout check frame
  delay(1500);
}

void loop() {
#if USE_BLE
  // BLE reconnect / notify handling goes here
#else
  static uint32_t t0 = millis();
  float t = (millis() - t0) / 1000.0f;
  float pwm   = 50.0f + 50.0f * sin(t * 0.4f);   // 0 to 100
  float mph   = 20.0f + 15.0f * sin(t * 0.25f);  // 5 to 35
  float volts = 147.0f + 2.0f * sin(t * 0.1f);
  drawHud(pwm, mph, volts);
  delay(100);  // about 10 frames a second
#endif
}