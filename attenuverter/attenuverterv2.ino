/*
knzet EURORACK DUAL INVERTING CV ATTENUATOR, for HAGIWO's MOD1

Channel 1:
  A0 = Pot 1 (attenuation 0.0 → 1.0)
  A3 = CV IN1
  D10 = OUT1 (PWM)

Channel 2:
  A1 = Pot 2 (attenuation 0.0 → 1.0)
  A4 = CV IN2
  D11 = OUT2 (PWM)

Output = Inverted Input × Knob Position
*/

#include <Arduino.h>

// ----------------------------
// Pin Definitions
// ----------------------------
#define POT1_PIN   A0
#define POT2_PIN   A1

#define CV1_PIN    A3   // CV IN1
#define CV2_PIN    A4   // CV IN2

#define OUT1_PIN   10   // OUT1 PWM (Timer1 OCR1B)
#define OUT2_PIN   11   // OUT2 PWM (Timer2 OCR2A)

// ----------------------------
// Setup
// ----------------------------
void setup() {
  pinMode(OUT1_PIN, OUTPUT);
  pinMode(OUT2_PIN, OUTPUT);

  pinMode(POT1_PIN, INPUT);
  pinMode(POT2_PIN, INPUT);

  pinMode(CV1_PIN, INPUT);
  pinMode(CV2_PIN, INPUT);

  // ----------------------------
  // High Frequency PWM Setup
  // ----------------------------

  // Timer1 → D10 (OCR1B)
  // Fast PWM 8-bit, no prescaler (~62.5kHz)
  TCCR1A = 0;
  TCCR1B = 0;

  TCCR1A = (1 << WGM10) | (1 << COM1B1);
  TCCR1B = (1 << WGM12) | (1 << CS10);

  OCR1B = 0;

  // Timer2 → D11 (OCR2A)
  // Fast PWM, no prescaler (~62.5kHz)
  TCCR2A = 0;
  TCCR2B = 0;

  TCCR2A = (1 << WGM20) | (1 << WGM21) | (1 << COM2A1);
  TCCR2B = (1 << CS20);

  OCR2A = 0;
}

// ----------------------------
// Main Loop
// ----------------------------
void loop() {

  // -------- Channel 1 --------

  // Read knob (0–1023)
  int pot1 = analogRead(POT1_PIN);

  // Convert knob to gain (0.0–1.0)
  float gain1 = pot1 / 1023.0;

  // Read CV input (0–1023)
  int cv1 = analogRead(CV1_PIN);

  // Normalize CV input (0.0–1.0)
  float in1 = cv1 / 1023.0;

  // Invert input
  float inverted1 = 1.0 - in1;

  // Apply attenuation
  float out1 = inverted1 * gain1;

  // Convert to PWM (0–255)
  int pwm1 = (int)(out1 * 255.0);

  // Output to D10
  OCR1B = pwm1;


  // -------- Channel 2 --------

  int pot2 = analogRead(POT2_PIN);
  float gain2 = pot2 / 1023.0;

  int cv2 = analogRead(CV2_PIN);

  float in2 = cv2 / 1023.0;
  float inverted2 = 1.0 - in2;

  float out2 = inverted2 * gain2;

  int pwm2 = (int)(out2 * 255.0);

  // Output to D11
  OCR2A = pwm2;
}
