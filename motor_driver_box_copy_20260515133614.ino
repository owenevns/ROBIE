/*********************************************************************
  Pot controls stepper speed (A4988) and shows LOW / MEDIUM / HIGH
  on SH1106 OLED, only redrawing the text area.

  NEW: Motor STOP handled using ENABLE pin (D7).
*********************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define i2c_Address 0x3c
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----- Pins -----
const int potPin   = A0;
const int stepPin  = 4;
const int dirPin   = 3;
const int enablePin = 7;   // << ENABLE pin for A4988

// ----- Step timing range -----
int stepDelayLower_us = 650;
int stepDelayUpper_us = 5000;

// Bottom 2% stop threshold
const int stopThreshold = int(0.02 * 1023);   // ≈ 20

// Stepper timing
unsigned long lastStepMicros  = 0;
unsigned long stepInterval_us = 2000;

// Display timing
unsigned long lastOledUpdateMs   = 0;
const unsigned long oledPeriodMs = 100;

// Display range states
enum LevelRange { RANGE_LOW = 0, RANGE_MEDIUM, RANGE_HIGH, RANGE_STOP };
int currentRange = -1;

const int labelX = 10;
const int labelY = 24;
const int labelW = 100;
const int labelH = 24;

void setup() {
  Serial.begin(9600);

  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enablePin, OUTPUT);

  digitalWrite(dirPin, HIGH);     // Set direction
  digitalWrite(enablePin, LOW);   // Enable driver ON at startup

  delay(250);
  display.begin(i2c_Address, true);
  Wire.setClock(400000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("Pot level:");
  display.display();
}

void loop() {

  // Read potentiometer
  int rawValue = analogRead(potPin);
  int percent  = map(rawValue, 0, 1023, 0, 100);

  // Decide range including STOP zone
  int newRange;
  if (rawValue < stopThreshold) {
    newRange = RANGE_STOP;
  }
  else if (percent < 34) {
    newRange = RANGE_LOW;
  }
  else if (percent < 67) {
    newRange = RANGE_MEDIUM;
  }
  else {
    newRange = RANGE_HIGH;
  }

  // Step interval (only valid when motor is active)
  if (newRange != RANGE_STOP) {
    stepInterval_us = map(rawValue, stopThreshold, 1023,
                          stepDelayUpper_us, stepDelayLower_us);
  }

  // ---- ENABLE / DISABLE MOTOR ----
  if (newRange == RANGE_STOP) {
    digitalWrite(enablePin, HIGH);   // Disable A4988 output
  } else {
    digitalWrite(enablePin, LOW);    // Enable driver
  }

  // ---- STEPPER PULSING ----
  if (newRange != RANGE_STOP) {
    unsigned long nowMicros = micros();
    if (nowMicros - lastStepMicros >= stepInterval_us) {
      lastStepMicros = nowMicros;

      digitalWrite(stepPin, HIGH);
      delayMicroseconds(3);
      digitalWrite(stepPin, LOW);
    }
  }

  // ---- OLED UPDATE (only if changed) ----
  unsigned long nowMs = millis();
  if (nowMs - lastOledUpdateMs >= oledPeriodMs) {
    lastOledUpdateMs = nowMs;

    if (newRange != currentRange) {
      currentRange = newRange;

      display.fillRect(labelX, labelY, labelW, labelH, SH110X_BLACK);
      display.setTextSize(2);
      display.setCursor(labelX, labelY);

      if (currentRange == RANGE_STOP)      display.print("STOP");
      else if (currentRange == RANGE_LOW)  display.print("LOW");
      else if (currentRange == RANGE_MEDIUM) display.print("MED");
      else if (currentRange == RANGE_HIGH) display.print("HIGH");

      display.display();
    }
  }
}
