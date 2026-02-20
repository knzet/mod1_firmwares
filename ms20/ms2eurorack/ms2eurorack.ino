/*
  MS-20 to Eurorack converter

  CV: Hz/V in -> V/oct out, log mapping.
  Trigger: S-trig in -> V-trig out (HIGH pulse).

  Calibration notes:
  - MS-20 CV outputs: typically 0-5V (some modules 0-8V)
  - Hz/V scaling: frequency is linear with voltage (f = k * V)
  - V/oct scaling: frequency is exponential (f = f0 * 2^V)
  - Conversion: V_out = log2(V_in) + constant
  
  Tuning:
  - POT1 (Offset): Sets zero point / root note (-1.25V to +1.25V range)
  - POT2 (Scale): Adjusts octave range to match your oscillator (0.5x to 2.0x gain)
  - Start with Scale at middle, play a known note on MS-20, adjust Offset to match pitch
  - Then adjust Scale so octaves track correctly across the range

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

  // MS-20 CV output: typically 0-5V (ADC 0-1023 if Vcc=5V)
  // Hz/V: frequency = k * voltage (linear)
  // V/oct: frequency = f0 * 2^V_out (exponential)
  // So: f0 * 2^V_out = k * V_in  =>  V_out = log2(k*V_in/f0) = log2(V_in) + constant
  
  // Convert ADC to voltage (assuming 0-5V range, Vcc=5V)
  // ADC 0 = 0V, ADC 1023 = 5V
  float voltageIn = (float)adc * 5.0f / 1023.0f;
  
  // Avoid log(0) - use minimum voltage threshold
  // MS-20 CV typically starts around 0V, but we need a small offset for log
  const float MIN_VOLTAGE = 0.01f;  // 10mV minimum to avoid log(0)
  if (voltageIn < MIN_VOLTAGE) voltageIn = MIN_VOLTAGE;
  
  // Log conversion: V_out ∝ log2(V_in)
  // Map 0.01V to 5V -> log2(0.01) to log2(5) ≈ -6.64 to 2.32
  // Normalize to 0-255 range for PWM output (0-5V V/oct)
  float log2v = log(voltageIn) / log(2.0f);
  float logMin = log(MIN_VOLTAGE) / log(2.0f);  // ≈ -6.64
  float logMax = log(5.0f) / log(2.0f);          // ≈ 2.32
  float logRange = logMax - logMin;              // ≈ 8.96
  
  // Normalize log value to 0-1 range, then scale to 0-255
  float normalized = (log2v - logMin) / logRange;
  int logVal = (int)(normalized * 255.0f + 0.5f);
  if (logVal < 0) logVal = 0;
  if (logVal > 255) logVal = 255;

  // Apply user calibration: scale and offset
  // Scale pot: 0-1023 -> 0.5x to 2.0x gain (128-512 in 8.8 fixed point)
  // Use this to match MS-20 Hz/V scaling to your V/oct oscillator's range
  int scale = map(scaleRaw, 0, 1023, 128, 512);  // 0.5x to 2.0x
  // Offset pot: 0-1023 -> -64 to +64 PWM steps (roughly -1.25V to +1.25V at 5V range)
  // Use this to set the zero point / root note
  int offset = map(offsetRaw, 0, 1023, -64, 64);
  
  // Use long to avoid overflow: logVal (0-255) * scale (128-512) can exceed int16 range
  long out = ((long)logVal * (long)scale) >> 8;  // Apply scale (8.8 fixed point)
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
