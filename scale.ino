#include <Adafruit_SSD1306.h>
#include <splash.h>

#include <HX711.h>

const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 4;
const int BUTTON_PIN = 23;
const float CALIBRATION_WEIGHT = 16.621f;

const long CALIBRATE_BUTTON_PRESS_MILLIS = 5000;  // 5s

HX711 scale;

const uint8_t DISPLAY_WIDTH = 128;
const uint8_t DISPLAY_HEIGHT = 64;
const uint8_t SCREEN_ADDR = 0x3C;
Adafruit_SSD1306 display{ DISPLAY_WIDTH, DISPLAY_HEIGHT };

enum State {
  SCALE = 0,
  TARE = 1,
  CALIBRATE = 2,
  CALIBRATE2 = 3,
};
State state;

void setup() {
  Serial.begin(9600);
  Serial.println("HX711 Scale");
  state = SCALE;

  // Pullup -> connect button pin to GND.
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    for (;;) {
      Serial.println("Init display failed!");
    }
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);

  Serial.println("Init done");
}

long weight = -1;
const long BUTTON_PRESS_MILLIS_MAX = 0xFFFFFFFF;
long button_press_millis = BUTTON_PRESS_MILLIS_MAX;
bool calibrated = false;

/*

State      | Button
SCALE      | 0       -> weigh, SCALE
SCALE      | 1       -> 0, TARE
TARE       | 0       -> tare, SCALE
TARE       | 1       -> 0, CALIBRATE   # if this state persists!
CALIBRATE  | 0       -> begin calibrate, CALIBRATE
CALIBRATE  | 1       -> begin calibrate, CALIBRATE2
CALIBRATE2 | *       -> calibrate, SCALE

*/

int last = -1;
int tmp = -1;
bool printed_calibrate = false;

void loop() {
  if (!scale.is_ready()) {
    //Serial.println("waiting for scale to become ready");
    if (!scale.wait_ready_timeout()) {
      Serial.println("ERROR: Scale not ready after timeout!");
      // Should we have an INIT state instead?
      return;
    }
  }

  if (state == SCALE) {
    if (calibrated == false) {
      state = CALIBRATE;
      Serial.println("Scale not calibrated! Entering calibration mode");
    } else {
      const long v = scale.get_units();
      if (v != weight) {
        weight = v;
        Serial.println("Weight: ");
        Serial.println(v);
        display.println(v);
        display.display();
      }

      tmp++;
      if (tmp > 1000) {
        Serial.println("yoyoyo: ");
        Serial.println(v);
        tmp = 0;
      }

      if (digitalRead(BUTTON_PIN) == LOW) {
        state = TARE;
        button_press_millis = millis();
      }
    }
  } else if (state == TARE) {
    Serial.println("Tare");
    scale.tare();
    if (digitalRead(BUTTON_PIN) == HIGH) {
      state = SCALE;
      button_press_millis = BUTTON_PRESS_MILLIS_MAX;
    } else {
      if (millis() - button_press_millis > CALIBRATE_BUTTON_PRESS_MILLIS) {
        state = CALIBRATE;
        button_press_millis = BUTTON_PRESS_MILLIS_MAX;
        while (digitalRead(BUTTON_PIN) == LOW) {
          Serial.println("Let go of button!!!");
        }
      }
    }
  } else if (state == CALIBRATE) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      state = CALIBRATE2;
    } else if (!printed_calibrate) {
      Serial.println("Place 50g weight and then press button");
      display.println("CALIB. BTTN.");
      display.display();
      printed_calibrate = true;
    }
  } else if (state == CALIBRATE2) {
    Serial.println("Calibration starts!");
    long calval_calib = scale.read_average(20);
    Serial.println("Calibration value for 50g:");
    Serial.println(calval_calib);
    calibrated = true;
    state = SCALE;
    scale.set_scale(calval_calib / CALIBRATION_WEIGHT);
    display.println("CALIB. DONE");
    display.display();
    printed_calibrate = false;
  } else {
    Serial.println("Corrupted state!");
    Serial.println(state);
  }
}
