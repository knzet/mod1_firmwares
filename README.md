firmware for Hagiwo's MOD1 open source digital eurorack module

https://note.com/solder_state/n/nc05d8e8fd311
https://www.youtube.com/watch?v=1raYsBepOF0
https://www.patreon.com/posts/mod1-arduino-cv-118769416



### feel free to open an issue or message with questions/bugs

## steplength euclidian
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


## attenuverter 
(this is a CV attenuverter, obviously it won't work with audio)

Channel 1:
  A0 = Pot 1 (attenuation 0.0 → 1.0)
  A3 = CV IN1
  D10 = OUT1 (PWM)

Channel 2:
  A1 = Pot 2 (attenuation 0.0 → 1.0)
  A4 = CV IN2
  D11 = OUT2 (PWM)

Output = Inverted Input × Knob Position