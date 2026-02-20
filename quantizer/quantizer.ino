/*
  Two-channel CV Quantizer

  - Quantizes whatever CV is present on the two inputs and sends quantized
    versions to the two outputs.
  - Uses fast PWM (~62.5 kHz) on D10 and D11 for CV outputs

  Pin assignment
  --------------
  POT1  A0  Scale selection (major, minor, etc.)
  POT2  A1  Portamento time
  POT3  A2  Transpose (e.g. -12 .. +12 semitones)
  CVIN1 A3  CV input channel 1
  CVIN2 A4  CV input channel 2
  BUTTON D4 Bypass toggle (INPUT_PULLUP)
  LED    D3 Scale index indication (blink count on change)
  CVOUT1 D10 Quantized CV output channel 1 (Timer1 / OC1B)
  CVOUT2 D11 Quantized CV output channel 2 (Timer2 / OC2A)
*/

// -------- Pin definitions --------

const int potScalePin      = A0;
const int potPortamentoPin = A1;
const int potTransposePin  = A2;

const int cvIn1Pin = A3;
const int cvIn2Pin = A4;

const int buttonPin = 4;   // momentary, wired to GND, use INPUT_PULLUP
const int ledPin    = 3;

const int cvOut1Pin = 10;  // OC1B (Timer1)
const int cvOut2Pin = 11;  // OC2A (Timer2)

// -------- Quantizer configuration --------

// Approximate range: 0..5 V -> 0..60 semitones (5 octaves at 1 V/oct).
const int   MAX_SEMITONES_INT = 60;
const float MAX_SEMITONES     = 60.0f;

struct ChannelState {
  float currentSemi;  // current output semitone (after portamento)
  float targetSemi;   // target semitone (quantized + transpose)
};

ChannelState ch1 = { 0.0f, 0.0f };
ChannelState ch2 = { 0.0f, 0.0f };

// -------- Scale tables --------

// Each scale is represented as a 12-element mask for the 12 semitones.
// 1 = in scale, 0 = not in scale.

enum ScaleIndex {
  SCALE_MAJOR = 0,
  SCALE_NAT_MINOR,
  SCALE_HARM_MINOR,
  SCALE_PENT_MAJOR,
  SCALE_PENT_MINOR,
  SCALE_DORIAN,
  SCALE_PHRYGIAN,
  NUM_SCALES
};

// Order of masks corresponds to the ScaleIndex enum above.
const uint8_t SCALE_MASKS[NUM_SCALES][12] = {
  // Major: 0,2,4,5,7,9,11
  { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 },
  // Natural minor: 0,2,3,5,7,8,10
  { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 },
  // Harmonic minor: 0,2,3,5,7,8,11
  { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1 },
  // Pentatonic major: 0,2,4,7,9
  { 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0 },
  // Pentatonic minor: 0,3,5,7,10
  { 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0 },
  // Dorian: 0,2,3,5,7,9,10
  { 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0 },
  // Phrygian: 0,1,3,5,7,8,10
  { 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0 }
};

int currentScaleIndex = 0;

// -------- Bypass button state --------

bool bypass = false;

int lastButtonReading = HIGH;
unsigned long lastButtonMillis = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 30;

// -------- LED scale index blink state --------

bool ledOnPhase        = false;
bool ledInSequence     = false;
uint8_t ledBlinkCount  = 0;      // how many ON phases remain
unsigned long ledLastMillis      = 0;
const unsigned long LED_INTERVAL_MS = 120;

// -------- Helper functions --------

static inline float adcToSemitone(int adc) {
  if (adc < 0) {
    adc = 0;
  } else if (adc > 1023) {
    adc = 1023;
  }
  return (adc * MAX_SEMITONES) / 1023.0f;
}

static inline int semitoneToPwm(float semi) {
  if (semi < 0.0f) {
    semi = 0.0f;
  } else if (semi > MAX_SEMITONES) {
    semi = MAX_SEMITONES;
  }
  int value = (int)((semi / MAX_SEMITONES) * 255.0f + 0.5f);
  if (value < 0) {
    value = 0;
  } else if (value > 255) {
    value = 255;
  }
  return value;
}

static inline bool noteInScale(int semitone, const uint8_t *mask) {
  int deg = semitone % 12;
  if (deg < 0) {
    deg += 12;
  }
  return mask[deg] != 0;
}

// Given a continuous semitone value, find the nearest semitone that
// belongs to the current scale (within +/- 12 semitones search window).
static int quantizeToScale(float semi, const uint8_t *mask) {
  int note = (int)(semi + (semi >= 0.0f ? 0.5f : -0.5f));  // round to nearest

  if (note < 0) {
    note = 0;
  } else if (note > MAX_SEMITONES_INT) {
    note = MAX_SEMITONES_INT;
  }

  for (int d = 0; d <= 12; ++d) {
    int down = note - d;
    if (down >= 0 && noteInScale(down, mask)) {
      return down;
    }

    int up = note + d;
    if (up <= MAX_SEMITONES_INT && noteInScale(up, mask)) {
      return up;
    }
  }

  // Fallback (shouldn't normally happen).
  if (note < 0) {
    note = 0;
  } else if (note > MAX_SEMITONES_INT) {
    note = MAX_SEMITONES_INT;
  }
  return note;
}

// Portamento: move current toward target. The portamento pot controls
// how many "steps" it should roughly take to reach the target.
static float applyPortamento(float current, float target, int portRaw) {
  // Very low values: treat as no glide (snappy).
  if (portRaw < 10) {
    return target;
  }

  // Map 0..1023 -> 1..33 steps (higher = slower).
  int steps = 1 + (portRaw / 32);
  float delta = target - current;
  return current + (delta / steps);
}

// Start LED blink sequence: number of blinks = scaleIndex + 1.
static void startScaleBlink(int scaleIndex) {
  uint8_t count = (uint8_t)(scaleIndex + 1);
  if (count == 0) {
    ledInSequence = false;
    digitalWrite(ledPin, LOW);
    return;
  }

  ledBlinkCount = count;
  ledInSequence = true;
  ledOnPhase    = false;
  ledLastMillis = millis();
  digitalWrite(ledPin, LOW);
}

// Update LED blink state machine. Each blink is an ON interval followed by OFF.
static void updateLedBlink() {
  if (!ledInSequence) {
    return;
  }

  unsigned long now = millis();
  if (now - ledLastMillis < LED_INTERVAL_MS) {
    return;
  }
  ledLastMillis = now;

  if (!ledOnPhase) {
    // Start ON phase.
    digitalWrite(ledPin, HIGH);
    ledOnPhase = true;
  } else {
    // End ON phase (one blink completed).
    digitalWrite(ledPin, LOW);
    ledOnPhase = false;

    if (ledBlinkCount > 0) {
      --ledBlinkCount;
    }
    if (ledBlinkCount == 0) {
      ledInSequence = false;
    }
  }
}

// Debounced button read; toggles global bypass on falling edge.
static void updateButton() {
  int reading = digitalRead(buttonPin);
  unsigned long now = millis();

  if (now - lastButtonMillis > BUTTON_DEBOUNCE_MS) {
    if (lastButtonReading == HIGH && reading == LOW) {
      // Falling edge: toggle bypass.
      bypass = !bypass;

      if (!bypass) {
        // When re-enabling quantization, sync current to target to avoid a big slide.
        ch1.currentSemi = ch1.targetSemi;
        ch2.currentSemi = ch2.targetSemi;
      }

      lastButtonMillis = now;
    }
  }

  lastButtonReading = reading;
}

// -------- Arduino setup / loop --------

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  pinMode(cvOut1Pin, OUTPUT);
  pinMode(cvOut2Pin, OUTPUT);

  digitalWrite(ledPin, LOW);

  // Configure Timer1 for fast PWM, 8-bit, no prescaling (D10 = OC1B).
  // WGM10=1, WGM12=1 -> fast PWM 8-bit; CS10=1 -> prescaler 1.
  TCCR1A = (1 << WGM10) | (1 << COM1B1);
  TCCR1B = (1 << WGM12) | (1 << CS10);

  // Configure Timer2 for fast PWM, 8-bit, no prescaling (D11 = OC2A).
  TCCR2A = (1 << WGM21) | (1 << WGM20) | (1 << COM2A1);
  TCCR2B = (1 << CS20);

  // Initialize channel state.
  ch1.currentSemi = 0.0f;
  ch1.targetSemi  = 0.0f;
  ch2.currentSemi = 0.0f;
  ch2.targetSemi  = 0.0f;

  currentScaleIndex = 0;
  startScaleBlink(currentScaleIndex);
}

void loop() {
  // Read control pots.
  int scalePot      = analogRead(potScalePin);
  int portamentoRaw = analogRead(potPortamentoPin);
  int transposeRaw  = analogRead(potTransposePin);

  // Map scale pot to one of the defined scales.
  int newScaleIndex = map(scalePot, 0, 1023, 0, NUM_SCALES - 1);
  if (newScaleIndex < 0) {
    newScaleIndex = 0;
  } else if (newScaleIndex >= NUM_SCALES) {
    newScaleIndex = NUM_SCALES - 1;
  }

  if (newScaleIndex != currentScaleIndex) {
    currentScaleIndex = newScaleIndex;
    startScaleBlink(currentScaleIndex);
  }

  const uint8_t *scaleMask = SCALE_MASKS[currentScaleIndex];

  // Map transpose pot to semitones, e.g. -12 .. +12.
  int transpose = map(transposeRaw, 0, 1023, -12, 12);

  // Update button / bypass state.
  updateButton();

  // Read CV inputs.
  int cv1Adc = analogRead(cvIn1Pin);
  int cv2Adc = analogRead(cvIn2Pin);

  if (bypass) {
    // Pass-through (unquantized, re-scaled to 0..255 PWM).
    int out1 = map(cv1Adc, 0, 1023, 0, 255);
    int out2 = map(cv2Adc, 0, 1023, 0, 255);

    if (out1 < 0) {
      out1 = 0;
    } else if (out1 > 255) {
      out1 = 255;
    }
    if (out2 < 0) {
      out2 = 0;
    } else if (out2 > 255) {
      out2 = 255;
    }

    analogWrite(cvOut1Pin, out1);
    analogWrite(cvOut2Pin, out2);
  } else {
    // Convert ADC to semitone values.
    float semi1 = adcToSemitone(cv1Adc);
    float semi2 = adcToSemitone(cv2Adc);

    // Quantize to scale.
    int q1 = quantizeToScale(semi1, scaleMask);
    int q2 = quantizeToScale(semi2, scaleMask);

    // Apply transpose.
    int q1Trans = q1 + transpose;
    int q2Trans = q2 + transpose;

    if (q1Trans < 0) {
      q1Trans = 0;
    } else if (q1Trans > MAX_SEMITONES_INT) {
      q1Trans = MAX_SEMITONES_INT;
    }
    if (q2Trans < 0) {
      q2Trans = 0;
    } else if (q2Trans > MAX_SEMITONES_INT) {
      q2Trans = MAX_SEMITONES_INT;
    }

    ch1.targetSemi = (float)q1Trans;
    ch2.targetSemi = (float)q2Trans;

    // Apply portamento (same setting for both channels).
    ch1.currentSemi = applyPortamento(ch1.currentSemi, ch1.targetSemi, portamentoRaw);
    ch2.currentSemi = applyPortamento(ch2.currentSemi, ch2.targetSemi, portamentoRaw);

    // Convert back to PWM and output.
    int pwm1 = semitoneToPwm(ch1.currentSemi);
    int pwm2 = semitoneToPwm(ch2.currentSemi);

    analogWrite(cvOut1Pin, pwm1);
    analogWrite(cvOut2Pin, pwm2);
  }

  // Handle LED scale index indication.
  updateLedBlink();
}

