/*
  Eurorack to MS-20 converter

  CV: V/oct in -> Hz/V out, exp mapping.
  Trigger: V-trig in -> S-trig out (LOW = short to GND).

  --Pin assign---
  POT1  A0  Offset (Hz/V calibration)
  POT2  A1  Scale / gain (Hz/V range)
  POT3  A2  Trigger length (S-trig pulse width, ~2-100 ms)
  F1    A3  CV IN1  V/oct in (Eurorack, analog)
  F2    A4  CV IN2  V-trig in (trigger input; analog read threshold or digital)
  F3    D10 OUT1   Hz/V CV out (PWM)
  F4    D11 OUT2   S-trig out (trigger output, short to GND when active)
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
const int vTrigInPin     = A4;   // F2 = CV IN2 jack, V-trig in
const int sTrigOutPin    = 11;   // F4 = OUT2
const int ledPin         = 3;

bool lastVTrigState = LOW;
unsigned long trigOutStart = 0;
unsigned long ledOffTime = 0;
byte trigOutState = 2;  // 0=start pulse, 1=pulse on, 2=idle
const unsigned long TRIG_DEBOUNCE_MS = 5;
const unsigned long LED_FLASH_MS = 40;

void setup() {
  pinMode(vTrigInPin, INPUT);
  pinMode(sTrigOutPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  pinMode(cvOutPin, OUTPUT);
  digitalWrite(sTrigOutPin, HIGH);
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

  float vOct = (float)adc / 1023.0f * 5.0f;
  float expVal = pow(2.0f, vOct);
  int out = (int)(255.0f * (expVal - 1.0f) / 31.0f + 0.5f);
  if (out < 0) out = 0;
  if (out > 255) out = 255;

  int scale = map(scaleRaw, 0, 1023, 128, 255);
  int offset = map(offsetRaw, 0, 1023, -64, 64);
  out = (out * scale) >> 8;
  out += offset;
  if (out < 0) out = 0;
  if (out > 255) out = 255;
  analogWrite(cvOutPin, (uint8_t)out);

  unsigned long pulseMs = (unsigned long)map(trigLenRaw, 0, 1023, 2, 100);
  if (pulseMs < 2) pulseMs = 2;

  int vTrigReading = digitalRead(vTrigInPin);
  if (vTrigReading == HIGH && lastVTrigState == LOW) {
    if (trigOutState == 2 && (now - trigOutStart) > TRIG_DEBOUNCE_MS) {
      trigOutStart = now;
      trigOutState = 0;
      ledOffTime = now + LED_FLASH_MS;
      digitalWrite(ledPin, HIGH);
    }
  }
  lastVTrigState = vTrigReading;

  if (trigOutState == 0) {
    digitalWrite(sTrigOutPin, LOW);
    trigOutState = 1;
  } else if (trigOutState == 1 && (now - trigOutStart) >= pulseMs) {
    digitalWrite(sTrigOutPin, HIGH);
    trigOutState = 2;
  }

  if (ledOffTime && now >= ledOffTime) {
    digitalWrite(ledPin, LOW);
    ledOffTime = 0;
  }
}
