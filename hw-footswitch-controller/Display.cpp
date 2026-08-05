#include "Display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Display display;

namespace {
  Adafruit_SSD1306 oled(128, 64, &Wire, -1);

  // Discrete step scroll: moves SCROLL_STEP_PX every SCROLL_INTERVAL_MS,
  // rather than a continuously-interpolated smooth scroll - matches a
  // classic stepped marquee look. Timing is based on elapsed time, not
  // how often this function happens to get called, so the visual speed
  // stays consistent regardless of loop timing.
  const int SCROLL_STEP_PX = 1;
  const unsigned long SCROLL_INTERVAL_MS = 100;
  const int SCROLL_GAP_PX = 24; // blank gap before the text repeats

  String scrollingText = "";
  unsigned long scrollStartMs = 0;

  void drawScrollingText(const String& text, int y, uint8_t textSize) {
    oled.setTextSize(textSize);

    if (text.length() == 0) {
      scrollingText = "";
      return;
    }

    int16_t x1, y1;
    uint16_t w, h;
    oled.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    if ((int)w <= 128) {
      oled.setCursor(0, y);
      oled.print(text);
      scrollingText = ""; // not scrolling - reset so a later long name starts fresh
      return;
    }

    if (text != scrollingText) {
      scrollingText = text;
      scrollStartMs = millis();
    }

    int totalWidth = (int)w + SCROLL_GAP_PX;
    unsigned long elapsed = millis() - scrollStartMs;
    unsigned long stepsElapsed = elapsed / SCROLL_INTERVAL_MS;
    int offsetPx = (int)((stepsElapsed * SCROLL_STEP_PX) % (unsigned long)totalWidth);

    int x = -offsetPx;
    oled.setCursor(x, y);
    oled.print(text);
    oled.setCursor(x + totalWidth, y); // second copy, so it loops seamlessly
    oled.print(text);
  }
}

bool Display::begin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[Display] SSD1306 init failed - check wiring/address (0x3C vs 0x3D)");
    return false;
  }
  // Adafruit_GFX defaults to auto-wrapping text that overflows the
  // screen width onto new lines - exactly wrong for a scrolling
  // marquee, where overflowing text should just run off the edge for
  // the scroll offset to reveal. Without this, a long preset name was
  // wrapping onto extra lines instead of scrolling, producing a
  // garbled, static-looking mess rather than a clean scroll.
  oled.setTextWrap(false);
  oled.clearDisplay();
  oled.display();
  return true;
}

void Display::showBoot() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(10, 20);
  oled.print("PULZE FW");
  oled.setTextSize(1);
  oled.setCursor(20, 44);
  oled.print("starting up...");
  oled.display();
}

void Display::showPerformance(AmpConnState ampState, uint16_t bank, uint8_t offset,
                               uint16_t totalBanks, const char* presetName) {
  (void)offset;      // no longer used here - the 4 LEDs indicate the active footswitch now
  (void)totalBanks;  // no longer shown - just "PAGE <n>" per the new design

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // ---- status line (unchanged) ----
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  switch (ampState) {
    case AmpConnState::CONNECTED:    oled.print("AMP OK"); break;
    case AmpConnState::CONNECTING:   oled.print("AMP: connecting"); break;
    case AmpConnState::SCANNING:     oled.print("AMP: scanning"); break;
    case AmpConnState::DISCONNECTED: oled.print("AMP: --"); break;
  }

  oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // ---- PAGE badge: biggest text on screen, inverted (solid fill,
  // black text, simulated bold) - centered horizontally below the
  // divider. "Bold" is faked by drawing the text twice with a 1px
  // offset, since Adafruit_GFX's built-in font has no bold weight. ----
  String pageText = "PAGE " + String(bank + 1);
  oled.setTextSize(2);
  int16_t bx1, by1;
  uint16_t bw, bh;
  oled.getTextBounds(pageText, 0, 0, &bx1, &by1, &bw, &bh);

  const int padX = 6;
  const int padY = 4;
  int badgeW = (int)bw + padX * 2 + 1; // +1 to fit the bold-simulation offset
  int badgeH = (int)bh + padY * 2;
  int badgeX = (128 - badgeW) / 2;
  if (badgeX < 0) badgeX = 0;
  if (badgeX + badgeW > 128) badgeW = 128 - badgeX;
  int badgeY = 14;

  oled.fillRect(badgeX, badgeY, badgeW, badgeH, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setCursor(badgeX + padX, badgeY + padY - by1);
  oled.print(pageText);
  oled.setCursor(badgeX + padX + 1, badgeY + padY - by1); // bold simulation
  oled.print(pageText);
  oled.setTextColor(SSD1306_WHITE); // restore for anything drawn after

  // ---- preset name: smaller, scrolls automatically if it doesn't fit ----
  int nameY = badgeY + badgeH + 6;
  drawScrollingText(presetName ? String(presetName) : String(""), nameY, 2);

  oled.display();
}

void Display::showTransferWaiting() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("TRANSFER MODE");
  oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  oled.setCursor(0, 24);
  oled.println("Waiting for");
  oled.println("web app...");
  oled.setCursor(0, 54);
  oled.print("Hold both banks to exit");
  oled.display();
}

void Display::showReceiving() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("TRANSFER MODE");
  oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(10, 28);
  oled.println("Receiving data");
  oled.setCursor(10, 40);
  oled.println("from app...");
  oled.display();
}

void Display::showTransferProgress(uint16_t received, uint16_t total) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("TRANSFER MODE");
  oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(10, 28);
  oled.print(received);
  oled.print(" / ");
  oled.print(total);
  oled.display();
}

void Display::showTransferDone(uint16_t count) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("TRANSFER COMPLETE");
  oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(0, 28);
  oled.print(count);
  oled.setTextSize(1);
  oled.setCursor(0, 50);
  oled.print("preset(s) saved");
  oled.display();
}

void Display::showError(const char* message) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("ERROR");
  oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  oled.setCursor(0, 20);
  oled.println(message);
  oled.display();
}
