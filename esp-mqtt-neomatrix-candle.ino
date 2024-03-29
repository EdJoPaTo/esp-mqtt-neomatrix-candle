#include <Adafruit_NeoMatrix.h>
#include <credentials.h>
#include <EspMQTTClient.h>

#define CLIENT_NAME "espMatrixCandle"
const bool MQTT_RETAINED = true;

EspMQTTClient client(
    WIFI_SSID,
    WIFI_PASSWORD,
    MQTT_SERVER,
    MQTT_USERNAME, // Can be omitted if not needed
    MQTT_PASSWORD, // Can be omitted if not needed
    CLIENT_NAME,   // Client name that uniquely identify your device
    1883           // The MQTT port, default to 1883. this line can be omitted
);

#define BASIC_TOPIC CLIENT_NAME "/"
#define BASIC_TOPIC_SET BASIC_TOPIC "set/"
#define BASIC_TOPIC_STATUS BASIC_TOPIC "status/"

const int PIN_MATRIX = 13; // D7
const int PIN_ON = 5;      // D1

// MATRIX DECLARATION:
// Parameter 1 = width of NeoPixel matrix
// Parameter 2 = height of matrix
// Parameter 3 = pin number (most are valid)
// Parameter 4 = matrix layout flags, add together as needed:
//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; pick two, e.g.
//     NEO_MATRIX_TOP + NEO_MATRIX_LEFT for the top-left corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; pick one or the other.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; pick one.
//   See example below for these values in action.
// Parameter 5 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)

// Example for NeoPixel Shield.  In this application we'd like to use it
// as a 5x8 tall matrix, with the USB port positioned at the top of the
// Arduino.  When held that way, the first pixel is at the top right, and
// lines are arranged in columns, progressive order.  The shield uses
// 800 KHz (v2) pixels that expect GRB color data.
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 32, PIN_MATRIX,
  NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT +
  NEO_MATRIX_ROWS    + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

const int CANDLE_HEIGHT = 5;
const int HEIGHT_MAX = 32 - CANDLE_HEIGHT;
const float HEIGHT_PERCENTAGE_FACTOR = 100.0 / HEIGHT_MAX;

uint16_t hue = 0;
uint8_t sat = 100;
uint8_t bri = 15;
uint8_t height = 10;
boolean lit = true;
boolean on = true;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // Initialize the BUILTIN_LED pin as an output
  pinMode(PIN_ON, OUTPUT);
  Serial.begin(115200);
  matrix.begin();

  client.enableDebuggingMessages();
  client.enableHTTPWebUpdater();
  client.enableOTA();
  client.enableLastWillMessage(BASIC_TOPIC "connected", "0", MQTT_RETAINED);
}

float calc_height_to_percentage() { return height * HEIGHT_PERCENTAGE_FACTOR; }
uint8_t calc_height_from_percentage(float percentage) { return percentage / HEIGHT_PERCENTAGE_FACTOR; }

void onConnectionEstablished() {
  client.subscribe(BASIC_TOPIC_SET "hue", [](const String &payload) {
    int parsed = strtol(payload.c_str(), 0, 10);
    uint16_t newValue = parsed % 360;
    if (hue != newValue) {
      hue = newValue;
      client.publish(BASIC_TOPIC_STATUS "hue", String(hue), MQTT_RETAINED);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "sat", [](const String &payload) {
    int parsed = strtol(payload.c_str(), 0, 10);
    uint8_t newValue = max(0, min(100, parsed));
    if (sat != newValue) {
      sat = newValue;
      client.publish(BASIC_TOPIC_STATUS "sat", String(sat), MQTT_RETAINED);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "bri", [](const String &payload) {
    int parsed = strtol(payload.c_str(), 0, 10);
    uint8_t newValue = max(0, min(255, parsed));
    if (bri != newValue) {
      bri = newValue;
      matrix.setBrightness(bri * on);
      client.publish(BASIC_TOPIC_STATUS "bri", String(bri), MQTT_RETAINED);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "on", [](const String &payload) {
    boolean newValue = payload != "0";
    if (on != newValue) {
      on = newValue;
      matrix.setBrightness(bri * on);
      client.publish(BASIC_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "lit", [](const String &payload) {
    boolean newValue = payload != "0";
    if (lit != newValue) {
      lit = newValue;
      client.publish(BASIC_TOPIC_STATUS "lit", lit ? "1" : "0", MQTT_RETAINED);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "height", [](const String &payload) {
    int parsed = strtol(payload.c_str(), 0, 10);
    uint8_t newValue = max(0, min(HEIGHT_MAX, parsed));
    if (height != newValue) {
      height = newValue;
      client.publish(BASIC_TOPIC_STATUS "height", String(height), MQTT_RETAINED);
      client.publish(BASIC_TOPIC_STATUS "height-percentage", String(calc_height_to_percentage()), MQTT_RETAINED);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "height-percentage", [](const String &payload) {
    float parsed = strtod(payload.c_str(), NULL);
    float percentage = max(0.0f, min(100.0f, parsed));
    uint8_t newValue = calc_height_from_percentage(percentage);
    if (height != newValue) {
      height = newValue;
      client.publish(BASIC_TOPIC_STATUS "height", String(height), MQTT_RETAINED);
      client.publish(BASIC_TOPIC_STATUS "height-percentage", String(calc_height_to_percentage()), MQTT_RETAINED);
    }
  });

  client.publish(BASIC_TOPIC "connected", "2", MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "hue", String(hue), MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "sat", String(sat), MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "bri", String(bri), MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "height", String(height), MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "height-percentage", String(calc_height_to_percentage()), MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "lit", lit ? "1" : "0", MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "on", on ? "1" : "0", MQTT_RETAINED);
}

void drawHorizontalLine(int16_t y, int16_t x_start, int16_t x_end, uint16_t color) {
  for (auto x = x_start; x <= x_end; x++) {
    matrix.drawPixel(x, y, color);
  }
}

void drawCandle() {
  auto candle_color = ColorHSV(hue * 182, sat * 2.55, 255);

  for (uint16_t y = 0; y < height; y++) {
    drawHorizontalLine(y, 0, 7, candle_color);
  }

  if (lit) {
    auto flame_color = ColorHSV(40 * 182, 255, 255);
    int flame_height = millis() % CANDLE_HEIGHT;
    int flame_direction = millis() % 4;
    int flame_move_height = millis() % 3;

    for (int i = 0; i <= flame_height; i++) {
      int left = 3;

      if (i > flame_move_height) {
        if (flame_direction == 0) {
          left -= 1;
        } else if (flame_direction == 1) {
          left += 1;
        }
      }

      drawHorizontalLine(height + i, left, left + 1, flame_color);
    }
  }
}

void loop() {
  client.loop();
  digitalWrite(LED_BUILTIN, client.isConnected() ? HIGH : LOW);
  digitalWrite(PIN_ON, on ? HIGH : LOW);

  matrix.fillScreen(0);
  drawCandle();

  matrix.show();
  delay(40);
}

uint16_t ColorHSV(uint16_t hue, uint8_t sat, uint8_t val) {
  uint8_t r, g, b, r2, g2, b2;

  // Remap 0-65535 to 0-1529. Pure red is CENTERED on the 64K rollover;
  // 0 is not the start of pure red, but the midpoint...a few values above
  // zero and a few below 65536 all yield pure red (similarly, 32768 is the
  // midpoint, not start, of pure cyan). The 8-bit RGB hexcone (256 values
  // each for red, green, blue) really only allows for 1530 distinct hues
  // (not 1536, more on that below), but the full unsigned 16-bit type was
  // chosen for hue so that one's code can easily handle a contiguous color
  // wheel by allowing hue to roll over in either direction.
  hue = (hue * 1530L + 32768) / 65536;

  // Convert hue to R,G,B (nested ifs faster than divide+mod+switch):
  if (hue < 510) {         // Red to Green-1
    b = 0;
    if (hue < 255) {       //   Red to Yellow-1
      r = 255;
      g = hue;             //     g = 0 to 254
    } else {               //   Yellow to Green-1
      r = 510 - hue;       //     r = 255 to 1
      g = 255;
    }
  } else if (hue < 1020) { // Green to Blue-1
    r = 0;
    if (hue < 765) {       //   Green to Cyan-1
      g = 255;
      b = hue - 510;       //     b = 0 to 254
    } else {               //   Cyan to Blue-1
      g = 1020 - hue;      //     g = 255 to 1
      b = 255;
    }
  } else if (hue < 1530) { // Blue to Red-1
    g = 0;
    if (hue < 1275) {      //   Blue to Magenta-1
      r = hue - 1020;      //     r = 0 to 254
      b = 255;
    } else {               //   Magenta to Red-1
      r = 255;
      b = 1530 - hue;      //     b = 255 to 1
    }
  } else {                 // Last 0.5 Red (quicker than % operator)
    r = 255;
    g = b = 0;
  }

  // Apply saturation and value to R,G,B
  uint32_t v1 = 1 + val;  // 1 to 256; allows >>8 instead of /255
  uint16_t s1 = 1 + sat;  // 1 to 256; same reason
  uint8_t s2 = 255 - sat; // 255 to 0

  r2 = ((((r * s1) >> 8) + s2) * v1) >> 8;
  g2 = ((((g * s1) >> 8) + s2) * v1) >> 8;
  b2 = ((((b * s1) >> 8) + s2) * v1) >> 8;
  return matrix.Color(r2, g2, b2);
}
