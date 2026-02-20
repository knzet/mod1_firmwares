/*
  MS-20 to Eurorack converter

  CV: Hz/V in -> V/oct out, log mapping.
  Trigger: S-trig in -> V-trig out (HIGH pulse).

  --Pin assign---
  POT1  A0  Offset (V/oct calibration)
  POT2  A1  Scale / gain (octave range)
  POT3  A2  Trigger length (pulse width, ~2-100 ms)
  F1    A3  CV IN1  Hz/V in (MS-20, analog)
  F2    A4  CV IN2  S-trig in (trigger input, short to GND; used as digital)
  F3    D10 OUT1   V/oct CV out (PWM)
  F4    D11 OUT2   V-trig out (trigger output)
  BUTTON D4 Push button (internal pull-up)
  LED   D3  Input trigger indicator
  EEPROM  N/A
*/

#include <math.h>

const int potOffsetPin   = A0;
const int potScalePin    = A1;
const int potTrigLenPin  = A2;
const int cvInPin        = A3;   // F1 = CV IN1
const int cvOutPin       = 10;   // F3 = OUT1
const int sTrigInPin     = A4;   // F2 = CV IN2 jack, S-trig in (digital read)
const int vTrigOutPin    = 11;   // F4 = OUT2
const int ledPin         = 3;

bool lastSTrigState = HIGH;
unsigned long trigOutStart = 0;
unsigned long ledOffTime = 0;
byte trigOutState = 2;  // 0=start pulse, 1=pulse on, 2=idle
const unsigned long TRIG_DEBOUNCE_MS = 5;
const unsigned long LED_FLASH_MS = 40;

void setup() {
  pinMode(sTrigInPin, INPUT_PULLUP);
  pinMode(vTrigOutPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  pinMode(cvOutPin, OUTPUT);
  digitalWrite(vTrigOutPin, LOW);
  digitalWrite(ledPin, LOW);

  TCCR1A = (1 << WGM10) | (1 << COM1B1);
  TCCR1B = (1 << WGM12) | (1 << CS10);
}

void loop() {
  unsigned long now = millis();

  int offsetRaw = analogRead(potOffsetPin);
  int scaleRaw = analogRead(potScalePin);
  int trigLenRaw = analogRead(potTrigLenPin);
  int adc = analogRead(cvInPin);
  if (adc < 0) adc = 0;
  if (adc > 1023) adc = 1023;

  float x = (float)(adc + 1);
  float log2x = log(x) / log(2.0f);
  int logVal = (int)(255.0f * log2x / 10.0f + 0.5f);
  if (logVal < 0) logVal = 0;
  if (logVal > 255) logVal = 255;

  int scale = map(scaleRaw, 0, 1023, 128, 255);
  int offset = map(offsetRaw, 0, 1023, -64, 64);
  int out = (logVal * scale) >> 8;
  out += offset;
  if (out < 0) out = 0;
  if (out > 255) out = 255;
  analogWrite(cvOutPin, (uint8_t)out);

  unsigned long pulseMs = (unsigned long)map(trigLenRaw, 0, 1023, 2, 100);
  if (pulseMs < 2) pulseMs = 2;

  int sTrigReading = digitalRead(sTrigInPin);
  if (sTrigReading == LOW && lastSTrigState == HIGH) {
    if (trigOutState == 2 && (now - trigOutStart) > TRIG_DEBOUNCE_MS) {
      trigOutStart = now;
      trigOutState = 0;
      ledOffTime = now + LED_FLASH_MS;
      digitalWrite(ledPin, HIGH);
    }
  }
  lastSTrigState = sTrigReading;

  if (trigOutState == 0) {
    digitalWrite(vTrigOutPin, HIGH);
    trigOutState = 1;
  } else if (trigOutState == 1 && (now - trigOutStart) >= pulseMs) {
    digitalWrite(vTrigOutPin, LOW);
    trigOutState = 2;
  }

  if (ledOffTime && now >= ledOffTime) {
    digitalWrite(ledPin, LOW);
    ledOffTime = 0;
  }
}
