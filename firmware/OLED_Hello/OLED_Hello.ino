#include <LilyGo_AMOLED.h>
#include <TFT_eSPI.h>

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

  // Left-top: voltage (big)
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  char vbuf[16];
  snprintf(vbuf, sizeof(vbuf), "%.1f", volts);
  spr.drawString(vbuf, mx / 2, my / 2, 6);

  // Left-bottom: mph on one line, packed toward the divider (moved right)
  char sbuf[16];
  snprintf(sbuf, sizeof(sbuf), "%.1f", mph);
  const int sFont = 6;
  const int uFont = 2;
  int tw = spr.textWidth(sbuf, sFont);
  int uw = spr.textWidth("mph", uFont);
  int total = tw + 4 + uw;
  int sx = mx - pad - total;   // right edge stays left of the line
  if (sx < pad) sx = pad;
  int sy = my + my / 2;

  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(sbuf, sx, sy, sFont);
  spr.setTextColor(TFT_YELLOW, TFT_BLACK);
  spr.drawString("mph", sx + tw + 4, sy, uFont);

  // Right: font 8 digits, % fixed on the right edge
  char pbuf[8];
  snprintf(pbuf, sizeof(pbuf), "%.0f", pwmPct);

  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TFT_YELLOW, TFT_BLACK);

  int pw = spr.textWidth(pbuf, 8);
  int pctW = spr.textWidth("%", 4);
  int pctX = w - pad - pctW;
  int px = pctX - 4 - pw;
  if (px < mx + pad) px = mx + pad;
  int py = h / 2;

  spr.drawString(pbuf, px, py, 8);
  spr.drawString("%", pctX, py, 4);

  spr.setTextDatum(TL_DATUM);
  amoled.pushColors(0, 0, w, h, (uint16_t *)spr.getPointer());
}

void setup() {
  Serial.begin(115200);
  if (!amoled.beginAMOLED_147()) {
    while (true) delay(1000);
  }
  amoled.setBrightness(180);
  spr.createSprite(amoled.width(), amoled.height());
  spr.setSwapBytes(1);

  // Parked-looking placeholder (real Lynx numbers next session)
  drawHud(88.0f, 34.0f, 147.0f);
}

void loop() {}