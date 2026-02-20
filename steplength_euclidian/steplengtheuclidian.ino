/*
knzet euclidian sequencer for MOD1, based on HAGIWO's euclidian sequencer code
5, 6, 7, 8, 12, or 16 step sequencer. Adjustable rhythm rotation and number of hits.

--Pin assign---
POT1  A0  number of hits
POT2  A1  rhythm rotation
POT3  A2  step length (5, 6, 7, 8, 12, 16)
F1    D17  reset step in
F2    D9  clock in
F3    D10  number of hits CV
F4    D11 Trigger outpu 
BUTTON    reset step
LED       Trigger output
EEPROM    N/A

[Difference from HAGIWO's version]
v1.0.0  - Add: Rhythm rotation control via pot2, step length modes (5,6,7,8,12,16), removed hit probability
*/

// Definition of Euclidean rhythms arrays for different step lengths
const int euclidean_rhythm_5[6][5] = {
    {0, 0, 0, 0, 0}, // Hits: 0
    {1, 0, 0, 0, 0}, // Hits: 1
    {1, 0, 1, 0, 0}, // Hits: 2
    {1, 0, 1, 0, 1}, // Hits: 3
    {1, 1, 1, 0, 1}, // Hits: 4
    {1, 1, 1, 1, 1}, // Hits: 5
};

const int euclidean_rhythm_6[7][6] = {
    {0, 0, 0, 0, 0, 0}, // Hits: 0
    {1, 0, 0, 0, 0, 0}, // Hits: 1
    {1, 0, 0, 1, 0, 0}, // Hits: 2
    {1, 0, 1, 0, 1, 0}, // Hits: 3
    {1, 1, 0, 1, 1, 0}, // Hits: 4
    {1, 1, 1, 1, 1, 0}, // Hits: 5
    {1, 1, 1, 1, 1, 1}, // Hits: 6
};

const int euclidean_rhythm_7[8][7] = {
    {0, 0, 0, 0, 0, 0, 0}, // Hits: 0
    {1, 0, 0, 0, 0, 0, 0}, // Hits: 1
    {1, 0, 0, 1, 0, 0, 0}, // Hits: 2
    {1, 0, 1, 0, 1, 0, 0}, // Hits: 3
    {1, 0, 1, 0, 1, 0, 1}, // Hits: 4
    {1, 1, 0, 1, 1, 0, 1}, // Hits: 5
    {1, 1, 1, 1, 1, 1, 0}, // Hits: 6
    {1, 1, 1, 1, 1, 1, 1}, // Hits: 7
};

const int euclidean_rhythm[9][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0}, // Hits: 0
    {1, 0, 0, 0, 0, 0, 0, 0}, // Hits: 1
    {1, 0, 0, 0, 1, 0, 0, 0}, // Hits: 2
    {1, 0, 1, 0, 0, 1, 0, 0}, // Hits: 3
    {1, 0, 1, 0, 1, 0, 1, 0}, // Hits: 4
    {1, 1, 0, 1, 1, 0, 1, 0}, // Hits: 5
    {1, 1, 1, 0, 1, 1, 1, 0}, // Hits: 6
    {1, 1, 1, 1, 1, 1, 1, 0}, // Hits: 7
    {1, 1, 1, 1, 1, 1, 1, 1}, // Hits: 8
};

const int euclidean_rhythm_12[13][12] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Hits: 0
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Hits: 1
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}, // Hits: 2
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, // Hits: 3
    {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0}, // Hits: 4
    {1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0}, // Hits: 5
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}, // Hits: 6
    {1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0}, // Hits: 7
    {1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0}, // Hits: 8
    {1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0}, // Hits: 9
    {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0}, // Hits: 10
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}, // Hits: 11
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // Hits: 12
};

const int euclidean_rhythm_16[17][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Hits: 0
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Hits: 1
    {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}, // Hits: 2
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}, // Hits: 3
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, // Hits: 4
    {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0}, // Hits: 5
    {1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0}, // Hits: 6
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0}, // Hits: 7
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}, // Hits: 8
    {1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0}, // Hits: 9
    {1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0}, // Hits: 10
    {1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0}, // Hits: 11
    {1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0}, // Hits: 12
    {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0}, // Hits: 13
    {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0}, // Hits: 14
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}, // Hits: 15
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // Hits: 16
};

// Pin definitions and timing settings
const int resetInputPin = 17; // Pin for step reset
const int resetButtonPin = 4; // Pin for reset button
const int outputPin = 11; // Trigger output pin
const int hitCVPin = A5; // Trigger CV input pin
const int extraLedPin = 3; // Extra LED connected to D3 pin
const int triggerInputPin = 9; // Trigger input pin
const int potPin = A0; // Potentiometer pin
const int stepModePin = A2; // Analog pin for selecting step mode
const unsigned long triggerTime = 10; // Trigger duration in milliseconds

// State management variables
unsigned long triggerStartMillis = 0; // Trigger start time
int currentStep = 0; // Current step index
bool isTriggering = false; // Triggering state flag
bool lastTriggerInputState = false; // Previous trigger input state
static int currentStepLength = 8; // Current step length (5, 6, 7, 8, 12, or 16)
static int lastStepLength = 8; // Last step length state
unsigned long modeChangeLedStartMillis = 0; // LED start time for mode change
bool isModeChangeLedOn = false; // LED state for mode change
bool disableOutputLed = false; // Flag to disable output LED during mode change

// Debounce variables
unsigned long lastResetDebounceTime = 0; // Last debounce time for reset button
const unsigned long resetDebounceDelay = 50; // Debounce delay in milliseconds

// Function to map analog value to step length
int getStepLength(int analogValue) {
  // Map A2 analog value (0-1023) to discrete step lengths
  // 8 should be at 12:00 position (~511)
  if (analogValue < 170) return 5;      // 0-170: 5 steps
  else if (analogValue < 340) return 6; // 171-340: 6 steps
  else if (analogValue < 511) return 7; // 341-511: 7 steps
  else if (analogValue < 682) return 8; // 512-682: 8 steps (center)
  else if (analogValue < 853) return 12; // 683-853: 12 steps
  else return 16;                        // 854-1023: 16 steps
}

void setup() {
  lastStepLength = currentStepLength; // Initialize last mode state
  pinMode(resetInputPin, INPUT); // Set step reset pin as input
  pinMode(resetButtonPin, INPUT_PULLUP); // Set reset button pin as input with pull-up
  pinMode(outputPin, OUTPUT);
  pinMode(extraLedPin, OUTPUT);
  pinMode(triggerInputPin, INPUT);
  pinMode(potPin, INPUT);
  digitalWrite(outputPin, LOW);
}

void loop() {
  // Read A2 value to determine step length
  int stepModeValue = min(analogRead(stepModePin)+5, 1023); // v1.1 FIX
  currentStepLength = getStepLength(stepModeValue);

  // Detect step length change and trigger LED indicator
  if (currentStepLength != lastStepLength) {
    lastStepLength = currentStepLength;
    isModeChangeLedOn = true;
    modeChangeLedStartMillis = millis();
    disableOutputLed = true;
    digitalWrite(extraLedPin, HIGH);
  }

  // Turn off LED after the appropriate time
  if (isModeChangeLedOn) {
    unsigned long ledDuration = (currentStepLength >= 12) ? 1000 : 500;
    if (millis() - modeChangeLedStartMillis >= ledDuration) {
      digitalWrite(extraLedPin, LOW);
      isModeChangeLedOn = false;
      disableOutputLed = false; // Re-enable output LED
    }
  }

  // Read potentiometer value to select Hits
  int potValue = min(analogRead(potPin)+5 + analogRead(hitCVPin), 1023);//v1.1 FIX

  // Explicitly set range for Hits selection based on current step length
  int selectedHits = map(potValue, 0, 1023, 0, currentStepLength);

  // Read rotation value from pot2 (A1) and map to rotation steps
  int rotationValue = min(analogRead(A1)+5, 1023); // v1.1 FIX
  int rotation = map(rotationValue, 0, 1023, 0, currentStepLength);

  // Check trigger input
  bool triggerInput = digitalRead(triggerInputPin) == HIGH;
  if (triggerInput && !lastTriggerInputState) {
    // Advance step on LOW to HIGH transition of trigger input

    // Calculate rotated step index
    int rotatedStep = (currentStep - rotation + currentStepLength) % currentStepLength;

    // Output based on current step with rotation applied
    if (!disableOutputLed) { // Skip output LED if mode change LED is active
      bool shouldTrigger = false;
      
      // Access pattern array based on current step length
      if (currentStepLength == 5) {
        if (selectedHits <= 5 && euclidean_rhythm_5[selectedHits][rotatedStep] == 1) {
          shouldTrigger = true;
        }
      } else if (currentStepLength == 6) {
        if (selectedHits <= 6 && euclidean_rhythm_6[selectedHits][rotatedStep] == 1) {
          shouldTrigger = true;
        }
      } else if (currentStepLength == 7) {
        if (selectedHits <= 7 && euclidean_rhythm_7[selectedHits][rotatedStep] == 1) {
          shouldTrigger = true;
        }
      } else if (currentStepLength == 8) {
        if (selectedHits <= 8 && euclidean_rhythm[selectedHits][rotatedStep] == 1) {
          shouldTrigger = true;
        }
      } else if (currentStepLength == 12) {
        if (selectedHits <= 12 && euclidean_rhythm_12[selectedHits][rotatedStep] == 1) {
          shouldTrigger = true;
        }
      } else if (currentStepLength == 16) {
        if (selectedHits <= 16 && euclidean_rhythm_16[selectedHits][rotatedStep] == 1) {
          shouldTrigger = true;
        }
      }

      if (shouldTrigger) {
        digitalWrite(outputPin, HIGH); // Start trigger
        digitalWrite(extraLedPin, HIGH); // Turn on extra LED
        triggerStartMillis = millis();
        isTriggering = true;
      }
      
      // Advance to next step
      currentStep = (currentStep + 1) % currentStepLength;
    }
  }

  // Check reset input or reset button with debounce
  bool resetInput = digitalRead(resetInputPin) == HIGH;
  bool resetButton = digitalRead(resetButtonPin) == LOW; // Button is active LOW
  static bool lastResetInputState = false; // Previous reset input state

  if ((resetInput || resetButton) && !lastResetInputState) {
    if (millis() - lastResetDebounceTime > resetDebounceDelay) {
      lastResetDebounceTime = millis();
      currentStep = 0; // Reset to first step
    }
  }
  lastResetInputState = resetInput || resetButton;

  // Update trigger input state
  lastTriggerInputState = triggerInput;

  // Handle triggering process
  if (isTriggering) {
    if (millis() - triggerStartMillis >= triggerTime) {
      digitalWrite(outputPin, LOW); // End trigger
      if (!disableOutputLed) {
        digitalWrite(extraLedPin, LOW); // Turn off extra LED
      }
      isTriggering = false;
    }
  }
}
