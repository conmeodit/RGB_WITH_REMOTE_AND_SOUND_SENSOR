#include <IRremote.hpp>         // Thư viện cho IRremote 4.x
#include <Adafruit_NeoPixel.h>
#include <math.h>

#define IR_RECEIVE_PIN 7        // Chân kết nối với module IR (1838T – MH-R38)
#define LED_PIN 3               // Chân dữ liệu cho NeoPixel
#define LED_COUNT 12            // Số LED

// Định nghĩa cho code 2
#define AUDIO_PIN A0
#define KNOB_PIN A1
#define BUTTON_1 6
#define BUTTON_2 5
#define BUTTON_3 4
#define SAMPLE_WINDOW 10
#define INPUT_FLOOR 50
#define INPUT_CEILING 400
#define PEAK_HANG 15
#define PEAK_FALL 6
#define LED_TOTAL LED_COUNT
#define LED_HALF LED_COUNT/2
#define VISUALS 6

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Biến toàn cục cho code 1
bool ledOn = false;
uint8_t brightness = 150;
bool animationMode = false;
bool animationPlaying = false;
int currentEffect = -1;
int previousEffect = -1;
int colorShift = 0;
const int numColors = 7;
uint32_t colorSequence[numColors] = {
  0,                            // 0: Rainbow mode
  strip.Color(255, 0, 0),       // 1: Red
  strip.Color(0, 255, 0),       // 2: Green
  strip.Color(0, 0, 255),       // 3: Blue
  strip.Color(255, 255, 0),     // 4: Yellow
  strip.Color(0, 255, 255),     // 5: Cyan
  strip.Color(255, 0, 255)      // 6: Magenta
};
int currentColorIndex = 0;
uint32_t currentColor = 0;

// Biến cho hiệu ứng Test
bool testEffectActive = false;
unsigned long testEffectStart = 0;
unsigned long lastTestUpdate = 0;
int testEffectIndex = 0;
uint32_t testColors[4] = {
  strip.Color(255,255,255),
  strip.Color(255,0,0),
  strip.Color(0,255,0),
  strip.Color(0,0,255)
};

// Biến toàn cục cho code 2
uint16_t gradient = 0;
uint16_t thresholds[] = {1529, 1019, 764, 764, 764, 1274};
uint8_t palette = 0;
uint8_t visual = 0;
uint8_t volume = 0;
uint8_t last = 0;
float maxVol = 1;
float knob = 1023.0;
float avgBump = 0;
float avgVol = 0;
float shuffleTime = 0;
bool shuffle = true;
bool bump = false;
int8_t pos[LED_COUNT] = { -2};
uint8_t rgb[LED_COUNT][3] = {0};
bool left = false;
int8_t dotPos = 0;
float timeBump = 0;
float avgTime = 0;
byte peak = 20;
byte dotCount = 0;
byte dotHangCount = 0;
byte volCount = 0;
unsigned int sample;

// Biến để theo dõi chế độ hiện tại
bool mode = true; // true: code 1, false: code 2

// Hàm tiện ích & hiển thị cơ bản
uint32_t Wheel(byte pos) {
  pos = 255 - pos;
  if (pos < 85) {
    return strip.Color(255 - pos * 3, 0, pos * 3);
  }
  if (pos < 170) {
    pos -= 85;
    return strip.Color(0, pos * 3, 255 - pos * 3);
  }
  pos -= 170;
  return strip.Color(pos * 3, 255 - pos * 3, 0);
}

void displayRainbowBase() {
  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t wheelPos = (i * 256 / LED_COUNT) & 0xFF;
    strip.setPixelColor(i, Wheel(wheelPos));
  }
  strip.show();
}

void setAllLEDs(uint32_t color) {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void turnOffLEDs() {
  setAllLEDs(0);
}

void updateStaticDisplay() {
  if (!ledOn) {
    turnOffLEDs();
    return;
  }
  if (currentColorIndex == 0) {
    displayRainbowBase();
  } else {
    setAllLEDs(currentColor);
  }
}

// Các hàm hỗ trợ cho code 2
float fscale(float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve) {
  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  boolean invFlag = 0;

  if (curve > 10) curve = 10;
  if (curve < -10) curve = -10;
  curve = (curve * -.1);
  curve = pow(10, curve);

  if (inputValue < originalMin) inputValue = originalMin;
  if (inputValue > originalMax) inputValue = originalMax;

  OriginalRange = originalMax - originalMin;
  if (newEnd > newBegin) NewRange = newEnd - newBegin;
  else {
    NewRange = newBegin - newEnd;
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal = zeroRefCurVal / OriginalRange;
  if (originalMin > originalMax) return 0;

  if (invFlag == 0) {
    rangedValue = (pow(normalizedCurVal, curve) * NewRange) + newBegin;
  } else {
    rangedValue = newBegin - (pow(normalizedCurVal, curve) * NewRange);
  }
  return rangedValue;
}

void drawLine(uint8_t from, uint8_t to, uint32_t c) {
  uint8_t fromTemp;
  if (from > to) {
    fromTemp = from;
    from = to;
    to = fromTemp;
  }
  for (int i = from; i <= to; i++) {
    strip.setPixelColor(i, c);
  }
}

void fade(float damper) {
  for (int i = 0; i < strip.numPixels(); i++) {
    uint32_t col = strip.getPixelColor(i);
    if (col == 0) continue;
    float colors[3];
    for (int j = 0; j < 3; j++) colors[j] = split(col, j) * damper;
    strip.setPixelColor(i, strip.Color(colors[0], colors[1], colors[2]));
  }
}

uint8_t split(uint32_t color, uint8_t i) {
  if (i == 0) return color >> 16;
  if (i == 1) return color >> 8;
  if (i == 2) return color >> 0;
  return -1;
}

uint32_t ColorPalette(float num) {
  switch (palette) {
    case 0: return (num < 0) ? Rainbow(gradient) : Rainbow(num);
    case 1: return (num < 0) ? Sunset(gradient) : Sunset(num);
    case 2: return (num < 0) ? Ocean(gradient) : Ocean(num);
    case 3: return (num < 0) ? PinaColada(gradient) : PinaColada(num);
    case 4: return (num < 0) ? Sulfur(gradient) : Sulfur(num);
    case 5: return (num < 0) ? NoGreen(gradient) : NoGreen(num);
    default: return Rainbow(gradient);
  }
}

uint32_t Rainbow(unsigned int i) {
  if (i > 1529) return Rainbow(i % 1530);
  if (i > 1274) return strip.Color(255, 0, 255 - (i % 255));
  if (i > 1019) return strip.Color((i % 255), 0, 255);
  if (i > 764) return strip.Color(0, 255 - (i % 255), 255);
  if (i > 509) return strip.Color(0, 255, (i % 255));
  if (i > 255) return strip.Color(255 - (i % 255), 255, 0);
  return strip.Color(255, i, 0);
}

uint32_t Sunset(unsigned int i) {
  if (i > 1019) return Sunset(i % 1020);
  if (i > 764) return strip.Color((i % 255), 0, 255 - (i % 255));
  if (i > 509) return strip.Color(255 - (i % 255), 0, 255);
  if (i > 255) return strip.Color(255, 128 - (i % 255) / 2, (i % 255));
  return strip.Color(255, i / 2, 0);
}

uint32_t Ocean(unsigned int i) {
  if (i > 764) return Ocean(i % 765);
  if (i > 509) return strip.Color(0, i % 255, 255 - (i % 255));
  if (i > 255) return strip.Color(0, 255 - (i % 255), 255);
  return strip.Color(0, 255, i);
}

uint32_t PinaColada(unsigned int i) {
  if (i > 764) return PinaColada(i % 765);
  if (i > 509) return strip.Color(255 - (i % 255) / 2, (i % 255) / 2, (i % 255) / 2);
  if (i > 255) return strip.Color(255, 255 - (i % 255), 0);
  return strip.Color(128 + (i / 2), 128 + (i / 2), 128 - i / 2);
}

uint32_t Sulfur(unsigned int i) {
  if (i > 764) return Sulfur(i % 765);
  if (i > 509) return strip.Color(i % 255, 255, 255 - (i % 255));
  if (i > 255) return strip.Color(0, 255, i % 255);
  return strip.Color(255 - i, 255, 0);
}

uint32_t NoGreen(unsigned int i) {
  if (i > 1274) return NoGreen(i % 1275);
  if (i > 1019) return strip.Color(255, 0, 255 - (i % 255));
  if (i > 764) return strip.Color((i % 255), 0, 255);
  if (i > 509) return strip.Color(0, 255 - (i % 255), 255);
  if (i > 255) return strip.Color(255 - (i % 255), 255, i % 255);
  return strip.Color(255, i, 0);
}

// Các hàm updateEffect0 đến updateEffect10 từ code 1 (giữ nguyên, không thay đổi)
void updateEffect0() {
  static uint16_t j = 0;
  static unsigned long lastJUpdate = 0;
  static unsigned long lastShow = 0;
  unsigned long now = millis();

  if(now - lastJUpdate >= 15) {
    j++;
    lastJUpdate = now;
  }

  if(now - lastShow >= 70) {
    for (int i = 0; i < LED_COUNT; i++){
      if (currentColor == 0){
        uint8_t pos = ((i * 256 / LED_COUNT) + j) & 0xFF;
        strip.setPixelColor(i, Wheel(pos));
      } else {
        strip.setPixelColor(i, currentColor);
      }
    }
    strip.show();
    lastShow = now;
  }
  IrReceiver.resume();
}

void updateEffect1() {
  static unsigned long lastShow = 0;
  unsigned long now = millis();
  
  float phase = (now % 2000) / 2000.0 * 2 * PI;
  float factor = (sin(phase) + 1) / 2.0;
  
  for (int i = 0; i < LED_COUNT; i++){
    if (currentColor == 0) {
      uint8_t basePos = (i * 256 / LED_COUNT) & 0xFF;
      uint32_t baseColor = Wheel(basePos);
      uint8_t r = ((baseColor >> 16) & 0xFF) * factor;
      uint8_t g = ((baseColor >> 8) & 0xFF) * factor;
      uint8_t b = (baseColor & 0xFF) * factor;
      strip.setPixelColor(i, strip.Color(r, g, b));
    } else {
      uint8_t r = (((currentColor >> 16) & 0xFF)) * factor;
      uint8_t g = (((currentColor >> 8) & 0xFF)) * factor;
      uint8_t b = ((currentColor & 0xFF)) * factor;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
  }
  
  if(now - lastShow >= 30) {
    strip.show();
    lastShow = now;
  }
}

void updateEffect2() {
  static bool state = false;
  static unsigned long lastToggle = 0;
  static unsigned long lastShow = 0;
  unsigned long now = millis();

  if (now - lastToggle >= 350) {
    state = !state;
    lastToggle = now;
    
    for (int i = 0; i < LED_COUNT; i++) {
      if ((i % 2 == 0 && state) || (i % 2 == 1 && !state)) {
        if (currentColor == 0) {
          uint8_t pos = (i * 256 / LED_COUNT) & 0xFF;
          strip.setPixelColor(i, Wheel(pos));
        } else {
          strip.setPixelColor(i, currentColor);
        }
      } else {
        strip.setPixelColor(i, 0);
      }
    }
  }

  if (now - lastShow >= 70) {
    strip.show();
    lastShow = now;
  }
  
  IrReceiver.resume();
}

void updateEffect3() {
  static int pos = 0;
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  if (now - lastUpdate >= 50) {
    for (int i = 0; i < LED_COUNT; i++){
      uint32_t col = strip.getPixelColor(i);
      uint8_t r = ((col >> 16) & 0xFF) * 0.4;
      uint8_t g = ((col >> 8) & 0xFF) * 0.4;
      uint8_t b = (col & 0xFF) * 0.65;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    if (currentColor == 0) {
      uint8_t colorWheelPos = (pos * 256 / LED_COUNT) & 0xFF;
      strip.setPixelColor(pos, Wheel(colorWheelPos));
    } else {
      strip.setPixelColor(pos, currentColor);
    }
    
    strip.show();
    pos = (pos + 1) % LED_COUNT;
    lastUpdate = now;
  }
  IrReceiver.resume();
}

void updateEffect4() {
  static bool phase = false;
  static int currentLED = 0;
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  const unsigned long interval = 100;

  if (now - lastUpdate >= interval) {
    if (!phase) {
      if (currentColor == 0) {
        uint8_t colorIndex = (currentLED * 256 / LED_COUNT) & 0xFF;
        strip.setPixelColor(currentLED, Wheel(colorIndex));
      } else {
        strip.setPixelColor(currentLED, currentColor);
      }
      currentLED++;
      if (currentLED >= LED_COUNT) {
        phase = true;
        currentLED = 0;
      }
    } else {
      strip.setPixelColor(currentLED, 0);
      currentLED++;
      if (currentLED >= LED_COUNT) {
        phase = false;
        currentLED = 0;
      }
    }
    strip.show();
    lastUpdate = now;
  }
}

void updateEffect5() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  
  if (now - lastUpdate >= 120) {
    for (int i = 0; i < LED_COUNT; i++) {
      if (random(100) < 40) {
        if (currentColor == 0) {
          uint8_t randomColorPos = random(256);
          uint32_t randomColor = Wheel(randomColorPos);
          strip.setPixelColor(i, randomColor);
        } else {
          strip.setPixelColor(i, currentColor);
        }
      } else {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
    lastUpdate = now;
  }
}

void updateEffect6() {
  static unsigned long lastUpdate = 0;
  static int step = 0;
  static int direction = 1;
  unsigned long now = millis();
  
  if (now - lastUpdate >= 100) {
    for (int i = 0; i < LED_COUNT; i++) {
      uint32_t col = strip.getPixelColor(i);
      uint8_t r = ((col >> 16) & 0xFF) * 0.325;
      uint8_t g = ((col >> 8) & 0xFF) * 0.325;
      uint8_t b = (col & 0xFF) * 0.325;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }

    int centerLeft = LED_COUNT / 2 - 1;
    int centerRight = LED_COUNT / 2;
    
    uint32_t color;
    if (currentColor == 0) {
      uint8_t colorPos = (step * 256 / (LED_COUNT / 2)) & 0xFF;
      color = Wheel(colorPos);
    } else {
      color = currentColor;
    }

    if (step == 0) {
      strip.setPixelColor(centerLeft, color);
      strip.setPixelColor(centerRight, color);
    } else {
      int left = centerLeft - step;
      int right = centerRight + step;
      if (left >= 0) {
        strip.setPixelColor(left, color);
      }
      if (right < LED_COUNT) {
        strip.setPixelColor(right, color);
      }
    }
    
    strip.show();
    
    step += direction;
    if (step >= centerLeft + 1) {
      step = centerLeft + 1;
      direction = -1;
    } else if (step <= 0) {
      step = 0;
      direction = 1;
    }
    
    lastUpdate = now;
  }
}

void updateEffect7() {
  static bool state = false;
  static unsigned long lastToggle = 0;
  unsigned long now = millis();

  if(now - lastToggle >= 300) {
    state = !state;
    lastToggle = now;
    strip.show();
  }

  for (int i = 0; i < LED_COUNT; i++){
    if(i < LED_COUNT / 2) {
      if(state) {
        if(currentColor == 0) {
          uint8_t pos = (i * 256 / LED_COUNT) & 0xFF;
          strip.setPixelColor(i, Wheel(pos));
        } else {
          strip.setPixelColor(i, currentColor);
        }
      }
      else {
        strip.setPixelColor(i, 0);
      }
    } else {
      if(!state) {
        if(currentColor == 0) {
          uint8_t pos = (i * 256 / LED_COUNT) & 0xFF;
          strip.setPixelColor(i, Wheel(pos));
        } else {
          strip.setPixelColor(i, currentColor);
        }
      }
      else {
        strip.setPixelColor(i, 0);
      }
    }
  }
}

void updateEffect8() {
  static bool ledActive[LED_COUNT] = {false};
  static unsigned long lastRandomize = 0;
  static int activeLedCount = 0;
  static unsigned long lastShow = 0;
  unsigned long now = millis();

  if (now % 4000 == 2000 || activeLedCount == 0) {
    memset(ledActive, 0, sizeof(ledActive));
    activeLedCount = random(6, 12);
    int selectedCount = 0;
    while (selectedCount < activeLedCount) {
      int randomLed = random(0, LED_COUNT);
      if (!ledActive[randomLed]) {
        ledActive[randomLed] = true;
        selectedCount++;
      }
    }
    lastRandomize = now;
  }

  float phase = (now % 4000) / 4000.0 * 2 * PI;
  float factor = (cos(phase) + 1) / 2.0;

  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t basePos = (i * 256 / LED_COUNT) & 0xFF;
    uint32_t baseColor = Wheel(basePos);

    if (ledActive[i]) {
      if (currentColor == 0) {
        uint8_t basePos = (i * 256 / LED_COUNT) & 0xFF;
        uint32_t baseColor = Wheel(basePos);
        uint8_t r = ((baseColor >> 16) & 0xFF) * factor;
        uint8_t g = ((baseColor >> 8) & 0xFF) * factor;
        uint8_t b = (baseColor & 0xFF) * factor;
        strip.setPixelColor(i, strip.Color(r, g, b));
      } else {
        uint8_t r = (((currentColor >> 16) & 0xFF)) * factor;
        uint8_t g = (((currentColor >> 8) & 0xFF)) * factor;
        uint8_t b = ((currentColor & 0xFF)) * factor;
        strip.setPixelColor(i, strip.Color(r, g, b));
      }
    } else {
      strip.setPixelColor(i, 0);
    }
  }

  if (now - lastShow >= 70) {
    strip.show();
    lastShow = now;
  }
}

void updateEffect9() {
  static float progress = 0.0f;
  static int direction = 1;
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  
  if (now - lastUpdate >= 30) {
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, 0);
    }
    
    float posA = 2.0f - (2.0f * progress);
    float posB = 3.0f + (2.0f * progress);
    float posC = 8.0f - (2.0f * progress);
    float posD = 9.0f + (2.0f * progress);
    
    for (int i = 0; i < LED_COUNT; i++) {
      float intensity = 0.0f;
      intensity = max(intensity, 1.0f - abs(i - posA));
      intensity = max(intensity, 1.0f - abs(i - posB));
      intensity = max(intensity, 1.0f - abs(i - posC));
      intensity = max(intensity, 1.0f - abs(i - posD));
      intensity = min(1.0f, intensity);
      
      if (intensity > 0.0f) {
        if (currentColor == 0) {
          uint8_t basePos = (i * 256 / LED_COUNT) & 0xFF;
          uint32_t baseColor = Wheel(basePos);
          uint8_t r = ((baseColor >> 16) & 0xFF) * intensity;
          uint8_t g = ((baseColor >> 8) & 0xFF) * intensity;
          uint8_t b = (baseColor & 0xFF) * intensity;
          strip.setPixelColor(i, strip.Color(r, g, b));
        } else {
          uint8_t r = (((currentColor >> 16) & 0xFF)) * intensity;
          uint8_t g = (((currentColor >> 8) & 0xFF)) * intensity;
          uint8_t b = ((currentColor & 0xFF)) * intensity;
          strip.setPixelColor(i, strip.Color(r, g, b));
        }
      }
    }
    
    progress += direction * 0.05f;
    if (progress >= 1.0f) {
      progress = 1.0f;
      direction = -1;
    } else if (progress <= 0.0f) {
      progress = 0.0f;
      direction = 1;
    }
    
    strip.show();
    lastUpdate = now;
  }
}

void updateEffect10() {
  static unsigned long lastUpdate = 0;
  static int fromIndex = 0;
  static int toIndex = 1;
  static float progress = 0.0;
  const unsigned long transitionTime = 2000;
  const unsigned long updateInterval = 30;
  
  unsigned long now = millis();
  if (now - lastUpdate >= updateInterval) {
    float delta = (now - lastUpdate) / (float)transitionTime;
    progress += delta;
    if (progress >= 1.0) {
      progress = 0.0;
      fromIndex = toIndex;
      toIndex = (toIndex + 1) % numColors;
    }
    
    uint32_t newColor = blendColors(colorSequence[fromIndex], colorSequence[toIndex], progress);
    
    setAllLEDs(newColor);
    strip.show();
    lastUpdate = now;
  }
}

uint32_t blendColors(uint32_t baseColor, uint32_t targetColor, float ratio) {
  uint8_t r1 = (baseColor >> 16) & 0xFF;
  uint8_t g1 = (baseColor >> 8) & 0xFF;
  uint8_t b1 = baseColor & 0xFF;
  
  uint8_t r2 = (targetColor >> 16) & 0xFF;
  uint8_t g2 = (targetColor >> 8) & 0xFF;
  uint8_t b2 = targetColor & 0xFF;
  
  uint8_t r = (uint8_t)(r1 * (1 - ratio) + r2 * ratio);
  uint8_t g = (uint8_t)(g1 * (1 - ratio) + g2 * ratio);
  uint8_t b = (uint8_t)(b1 * (1 - ratio) + b2 * ratio);
  
  return strip.Color(r, g, b);
}

void updateAnimation() {
  if (!ledOn) return;
  if (animationMode && animationPlaying) {
    switch (currentEffect) {
      case 0: updateEffect0(); break;
      case 1: updateEffect1(); break;
      case 2: updateEffect2(); break;
      case 3: updateEffect3(); break;
      case 4: updateEffect4(); break;
      case 5: updateEffect5(); break;
      case 6: updateEffect6(); break;
      case 7: updateEffect7(); break;
      case 8: updateEffect8(); break;
      case 9: updateEffect9(); break;
      case 10: updateEffect10(); break;
      default: break;
    }
  }
}

void changeColor() {
  currentColorIndex = (currentColorIndex + 1) % numColors;
  if (currentColorIndex == 0) {
    currentColor = 0;
  } else {
    currentColor = colorSequence[currentColorIndex];
  }
}

void reverseColor() {
  currentColorIndex = (currentColorIndex - 1) % numColors;
  if (currentColorIndex == 0) {
    currentColor = 0;
  } else {
    currentColor = colorSequence[currentColorIndex];
  }
}

void updateTestEffect() {
  unsigned long now = millis();
  if (now - testEffectStart < 2000) {
    if (now - lastTestUpdate >= 200) {
      lastTestUpdate = now;
      testEffectIndex = (testEffectIndex + 1) % 4;
      setAllLEDs(testColors[testEffectIndex]);
    }
  } else {
    testEffectActive = false;
    updateStaticDisplay();
  }
}

void handleIR() {
  if (testEffectActive) return;
  
  static unsigned long lastIRTime = 0;
  unsigned long now = millis();
  
  if (now - lastIRTime < 150) {
    return;
  }
  
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
      IrReceiver.resume();
      return;
    }
    
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    Serial.print("Raw Data (HEX): 0x");
    Serial.println(code, HEX);
    Serial.print("Protocol: ");
    Serial.println(getProtocolString(IrReceiver.decodedIRData.protocol));
    
    switch (code) {
      case 0xB847FF00:  // Menu button: chuyển đổi chế độ
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
          IrReceiver.resume();
          break;
        }
        mode = !mode;
        if (mode) {
          // Chuyển sang chế độ code 1
          animationMode = true;
          animationPlaying = true;
          updateStaticDisplay();
        } else {
          gradient = 0;
          avgVol = 0;
          maxVol = 1;
          // Chuyển sang chế độ code 2
          turnOffLEDs();
          delay(200);    // Thêm debounce 200ms để tránh chuyển đổi quá nhanh
          IrReceiver.resume();
        }
        break;
      case 0xBA45FF00:  // Power button: bật/tắt đèn
        ledOn = !ledOn;
        if (ledOn) {
          if (animationMode)
            animationPlaying = true;
          else
            updateStaticDisplay();
        } else {
          turnOffLEDs();
        }
        break;
      case 0xBB44FF00:  // Test button: nhấp nháy 4 màu
        testEffectActive = true;
        testEffectStart = millis();
        lastTestUpdate = millis();
        testEffectIndex = 0;
        break;
      case 0xBF40FF00:  // Plus button: tăng độ sáng
        if (brightness < 255) {
          brightness = min(brightness + 25, 255);
          strip.setBrightness(brightness);
          if (!animationMode)
            updateStaticDisplay();
        }
        break;
      case 0xE619FF00:  // Minus button: giảm độ sáng
        if (brightness > 0) {
          brightness = max(brightness - 25, 0);
          strip.setBrightness(brightness);
          if (!animationMode)
            updateStaticDisplay();
        }
        break;
      case 0xF609FF00:  // Right button: Chuyển màu
        changeColor();
        break;
      case 0xF807FF00:  // Left button: chuyển màu ngược
        reverseColor();
        break;
      case 0xEA15FF00:  // Play button: tạm dừng / tiếp tục animation
        if (animationMode) {
          animationPlaying = !animationPlaying;
          Serial.print("Animation playing: ");
          Serial.println(animationPlaying ? "Resumed" : "Paused");
        }
        break;
      case 0xBC43FF00:  // Back button: đặt LED thành màu trắng
        animationMode = false;
        animationPlaying = false;
        currentColor = strip.Color(255,255,255);
        currentColorIndex = 1;
        setAllLEDs(currentColor);
        break;
      case 0xE916FF00:  // Button 0: Rainbow Cycle
        previousEffect = currentEffect;
        currentEffect = 0;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xF30CFF00:  // Button 1: Breathing effect
        previousEffect = currentEffect;
        currentEffect = 1;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xE718FF00:  // Button 2: Alternating flashing
        previousEffect = currentEffect;
        currentEffect = 2;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xA15EFF00:  // Button 3: Moving trail effect
        previousEffect = currentEffect;
        currentEffect = 3;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xF708FF00:  // Button 4: Wave effect
        previousEffect = currentEffect;
        currentEffect = 4;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xE31CFF00:  // Button 5: Sparkle effect
        previousEffect = currentEffect;
        currentEffect = 5;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xA55AFF00:  // Button 6: Fireworks effect
        previousEffect = currentEffect;
        currentEffect = 6;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xBD42FF00:  // Button 7: Alternate half flashing
        previousEffect = currentEffect;
        currentEffect = 7;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xAD52FF00:  // Button 8: Moving from ends
        previousEffect = currentEffect;
        currentEffect = 8;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xB54AFF00:  // Button 9: Simultaneous movement
        previousEffect = currentEffect;
        currentEffect = 9;
        animationMode = true;
        animationPlaying = true;
        break;
      case 0xF20DFF00:  // C button: Hiệu ứng chuyển màu dần đều
        previousEffect = currentEffect;
        currentEffect = 10;
        animationMode = true;
        animationPlaying = true;
        break;
      default:
        break;
    }
    
    Serial.println("-------------------");
    lastIRTime = now;
    IrReceiver.resume();
  }
}

void Visualize() {
  if (IrReceiver.decode()) { 
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    // Giả sử mã nút menu là 0xB847FF00
    if (code == 0xB847FF00) {
      mode = true;      // Chuyển sang chế độ IR
      IrReceiver.resume();
      return;           // Thoát khỏi hàm ngay
    }
    IrReceiver.resume();
  }
  switch (visual) {
    case 0: VU(); break;
    case 1: VUdot(); break;
    case 2: VU1(); break;
    case 3: VU2(); break;
    case 4: Pulse(); break;
    case 5: Traffic(); break;
    default: PaletteDance(); break;
  }
}

void Pulse() {
  fade(0.75);
  if (bump) gradient += thresholds[palette] / 24;
  if (volume > 0) {
    uint32_t col = ColorPalette(-1);
    int start = LED_HALF - (LED_HALF * (volume / maxVol));
    int finish = LED_HALF + (LED_HALF * (volume / maxVol)) + strip.numPixels() % 2;
    for (int i = start; i < finish; i++) {
      float damp = sin((i - start) * PI / float(finish - start));
      damp = pow(damp, 2.0);
      uint32_t col2 = strip.getPixelColor(-1);
      uint8_t colors[3];
      float avgCol = 0, avgCol2 = 0;
      for (int k = 0; k < 3; k++) {
        colors[k] = split(col, k) * damp * knob * pow(volume / maxVol, 2);
        avgCol += colors[k];
        avgCol2 += split(col2, k);
      }
      avgCol /= 3.0, avgCol2 /= 3.0;
      if (avgCol > avgCol2) strip.setPixelColor(i, strip.Color(colors[0], colors[1], colors[2]));
    }
  }
  strip.show();
}

void VU() {
  unsigned long startMillis = millis();
  float peakToPeak = 0;
  unsigned int signalMax = 0;
  unsigned int signalMin = 1023;
  unsigned int c, y;
  while (millis() - startMillis < SAMPLE_WINDOW) {
    sample = analogRead(AUDIO_PIN) * 2;
    if (sample < 1024) {
      if (sample > signalMax) signalMax = sample;
      else if (sample < signalMin) signalMin = sample;
    }
  }
  peakToPeak = signalMax - signalMin;
  for (int i = 0; i <= strip.numPixels() - 1; i++) {
    strip.setPixelColor(i, Wheel(map(i, 0, strip.numPixels() - 1, 10, 200)));
  }
  c = fscale(INPUT_FLOOR, INPUT_CEILING, strip.numPixels(), 0, peakToPeak, 2);
  if (c <= strip.numPixels()) {
    drawLine(strip.numPixels(), strip.numPixels() - c, strip.Color(0, 0, 0));
  }
  y = strip.numPixels() - peak;
  strip.setPixelColor(y - 1, Wheel(map(y, 0, strip.numPixels() - 1, 10, 200)));
  strip.show();
  if (dotHangCount > PEAK_HANG) {
    if (++dotCount >= PEAK_FALL) {
      peak++;
      dotCount = 0;
    }
  } else {
    dotHangCount++;
  }
}

void VUdot() {
  unsigned long startMillis = millis();
  float peakToPeak = 0;
  unsigned int signalMax = 0;
  unsigned int signalMin = 1023;
  unsigned int c, y;
  while (millis() - startMillis < SAMPLE_WINDOW) {
    sample = analogRead(AUDIO_PIN) * 3;
    if (sample < 1024) {
      if (sample > signalMax) signalMax = sample;
      else if (sample < signalMin) signalMin = sample;
    }
  }
  peakToPeak = signalMax - signalMin;
  for (int i = 0; i <= strip.numPixels() - 1; i++) {
    strip.setPixelColor(i, Wheel(map(i, 0, strip.numPixels() - 1, 10, 200)));
  }
  c = fscale(INPUT_FLOOR, INPUT_CEILING, strip.numPixels(), 0, peakToPeak, 2);
  if (c < peak) {
    peak = c;
    dotHangCount = 0;
  }
  if (c <= strip.numPixels()) {
    drawLine(strip.numPixels(), strip.numPixels() - c, strip.Color(0, 0, 0));
  }
  y = strip.numPixels() - peak;
  strip.setPixelColor(y - 1, Wheel(map(y, 0, strip.numPixels() - 1, 10, 200)));
  strip.show();
  if (dotHangCount > PEAK_HANG) {
    if (++dotCount >= PEAK_FALL) {
      peak++;
      dotCount = 0;
    }
  } else {
    dotHangCount++;
  }
}

void VU1() {
  unsigned long startMillis = millis();
  float peakToPeak = 0;
  unsigned int signalMax = 0;
  unsigned int signalMin = 1023;
  unsigned int c, y;
  while (millis() - startMillis < SAMPLE_WINDOW) {
    sample = analogRead(AUDIO_PIN) * 3;
    Serial.println(sample);
    if (sample < 1024) {
      if (sample > signalMax) signalMax = sample;
      else if (sample < signalMin) signalMin = sample;
    }
  }
  peakToPeak = signalMax - signalMin;
  for (int i = 0; i <= LED_HALF - 1; i++) {
    uint32_t color = Wheel(map(i, 0, -1, 30, 150));
    strip.setPixelColor(LED_TOTAL - i, color);
    strip.setPixelColor(0 + i, color);
  }
  c = fscale(INPUT_FLOOR, INPUT_CEILING, LED_HALF, 0, peakToPeak, 2);
  if (c < peak) {
    peak = c;
    dotHangCount = 0;
  }
  if (c <= strip.numPixels()) {
    drawLine(LED_HALF, LED_HALF - c, strip.Color(0, 0, 0));
    drawLine(LED_HALF, LED_HALF + c, strip.Color(0, 0, 0));
  }
  y = LED_HALF - peak;
  uint32_t color1 = Wheel(map(y, 0, LED_HALF - 1, 30, 150));
  strip.setPixelColor(y - 1, color1);
  y = LED_HALF + peak;
  strip.setPixelColor(y, color1);
  strip.show();
  if (dotHangCount > PEAK_HANG) {
    if (++dotCount >= PEAK_FALL) {
      peak++;
      dotCount = 0;
    }
  } else {
    dotHangCount++;
  }
}

void VU2() {
  unsigned long startMillis = millis();
  float peakToPeak = 0;
  unsigned int signalMax = 0;
  unsigned int signalMin = 1023;
  unsigned int c, y;
  while (millis() - startMillis < SAMPLE_WINDOW) {
    sample = analogRead(AUDIO_PIN) * 3;
    Serial.println(sample);
    if (sample < 1024) {
      if (sample > signalMax) signalMax = sample;
      else if (sample < signalMin) signalMin = sample;
    }
  }
  peakToPeak = signalMax - signalMin;
  for (int i = 0; i <= LED_HALF - 1; i++) {
    uint32_t color = Wheel(map(i, 0, LED_HALF - 1, 30, 150));
    strip.setPixelColor(LED_HALF - i - 1, color);
    strip.setPixelColor(LED_HALF + i, color);
  }
  c = fscale(INPUT_FLOOR, INPUT_CEILING, LED_HALF, 0, peakToPeak, 2);
  if (c < peak) {
    peak = c;
    dotHangCount = 0;
  }
  if (c <= LED_TOTAL) {
    drawLine(LED_TOTAL, LED_TOTAL - c, strip.Color(0, 0, 0));
    drawLine(0, 0 + c, strip.Color(0, 0, 0));
  }
  y = LED_TOTAL - peak;
  strip.setPixelColor(y - 1, Wheel(map(LED_HALF + y, 0, LED_HALF - 1, 30, 150)));
  y = 0 + peak;
  strip.setPixelColor(y, Wheel(map(LED_HALF - y, 0, LED_HALF + 1, 30, 150)));
  strip.show();
  if (dotHangCount > PEAK_HANG) {
    if (++dotCount >= PEAK_FALL) {
      peak++;
      dotCount = 0;
    }
  } else {
    dotHangCount++;
  }
}

void Traffic() {
  fade(0.8);
  if (bump) {
    int8_t slot = 0;
    for (slot; slot < sizeof(pos); slot++) {
      if (pos[slot] < -1) break;
      else if (slot + 1 >= sizeof(pos)) {
        slot = -3;
        break;
      }
    }
    if (slot != -3) {
      pos[slot] = (slot % 2 == 0) ? -1 : strip.numPixels();
      uint32_t col = ColorPalette(-1);
      gradient += thresholds[palette] / 24;
      for (int j = 0; j < 3; j++) {
        rgb[slot][j] = split(col, j);
      }
    }
  }
  if (volume > 0) {
    for (int i = 0; i < sizeof(pos); i++) {
      if (pos[i] < -1) continue;
      pos[i] += (i % 2) ? -1 : 1;
      if (pos[i] >= strip.numPixels()) pos[i] = -2;
      strip.setPixelColor(pos[i], strip.Color(
        float(rgb[i][0]) * pow(volume / maxVol, 2.0) * knob,
        float(rgb[i][1]) * pow(volume / maxVol, 2.0) * knob,
        float(rgb[i][2]) * pow(volume / maxVol, 2.0) * knob)
      );
    }
  }
  strip.show();
}

void PaletteDance() {
  if (bump) left = !left;
  if (volume > avgVol) {
    for (int i = 0; i < strip.numPixels(); i++) {
      float sinVal = abs(sin(
        (i + dotPos) * (PI / float(strip.numPixels() / 1.25))
      ));
      sinVal *= sinVal;
      sinVal *= volume / maxVol;
      sinVal *= knob;
      unsigned int val = float(thresholds[palette] + 1) * (float(i + map(dotPos, -1 * (strip.numPixels() - 1), strip.numPixels() - 1, 0, strip.numPixels() - 1)) / float(strip.numPixels())) + (gradient);
      val %= thresholds[palette];
      uint32_t col = ColorPalette(val);
      strip.setPixelColor(i, strip.Color(
        float(split(col, 0)) * sinVal,
        float(split(col, 1)) * sinVal,
        float(split(col, 2)) * sinVal)
      );
    }
    dotPos += (left) ? -1 : 1;
  } else fade(0.8);
  strip.show();
  if (dotPos < 0) dotPos = strip.numPixels() - strip.numPixels() / 6;
  else if (dotPos >= strip.numPixels() - strip.numPixels() / 6) dotPos = 0;
}

void CyclePalette() {
  if (shuffle && millis() / 1000.0 - shuffleTime > 38 && gradient % 2) {
    shuffleTime = millis() / 1000.0;
    palette++;
    if (palette >= sizeof(thresholds) / 2) palette = 0;
    gradient %= thresholds[palette];
    maxVol = avgVol;
  }
}

void CycleVisual() {
  if (!digitalRead(BUTTON_2)) {
    delay(10);
    shuffle = false;
    visual++;
    gradient = 0;
    if (visual > VISUALS) {
      shuffle = true;
      visual = 0;
    }
    if (visual == 1) memset(pos, -2, sizeof(pos));
    if (visual == 2 || visual == 3) {
      randomSeed(analogRead(0));
      dotPos = random(strip.numPixels());
    }
    delay(350);
    maxVol = avgVol;
  }
  if (shuffle && millis() / 1000.0 - shuffleTime > 38 && !(gradient % 2)) {
    shuffleTime = millis() / 1000.0;
    visual++;
    gradient = 0;
    if (visual > VISUALS) visual = 0;
    if (visual == 1) memset(pos, -2, sizeof(pos));
    if (visual == 2 || visual == 3) {
      randomSeed(analogRead(0));
      dotPos = random(strip.numPixels());
    }
    maxVol = avgVol;
  }
}

void setup() {
  Serial.begin(9600);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  strip.begin();
  strip.setBrightness(brightness);
  strip.show();

  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);
  pinMode(BUTTON_3, INPUT);
  digitalWrite(BUTTON_1, HIGH);
  digitalWrite(BUTTON_2, HIGH);
  digitalWrite(BUTTON_3, HIGH);
}

void loop() {
  if (mode) {
    // Chế độ code 1
    handleIR();
    if (testEffectActive) {
      updateTestEffect();
    }
    if (ledOn) {
      if (animationMode && animationPlaying) {
        updateAnimation();
      }
    }
  } else {
    handleIR();
    // Chế độ code 2
    volume = (analogRead(AUDIO_PIN) - 126);
    knob = 0.9;
    if (volume < avgVol / 2.0 || volume < 15) volume = 0;
    else avgVol = (avgVol + volume) / 2.0;
    if (volume > maxVol) maxVol = volume;
    CyclePalette();
    CycleVisual();
    if (gradient > thresholds[palette]) {
      gradient %= thresholds[palette] + 1;
      maxVol = (maxVol + volume) / 2.0;
    }
    if (volume - last > 10) avgBump = (avgBump + (volume - last)) / 2.0;
    bump = (volume - last > avgBump * .9);
    if (bump) {
      avgTime = (((millis() / 1000.0) - timeBump) + avgTime) / 2.0;
      timeBump = millis() / 1000.0;
    }
    Visualize();
    gradient++;
    last = volume;
    delay(16);
  }
}