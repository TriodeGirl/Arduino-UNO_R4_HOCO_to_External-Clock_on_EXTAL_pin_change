# Arduino-UNO_R4_HOCO_to_External-Clock_on_EXTAL_pin_change
Arduino UNO R4 RA4M1 HOCO to External-Clock on EXTAL pin change setup code, with over-clocking to 60MHz

This is an update to the previous code to get accurate clocking of the RA4M1 processor  using an external crystal, to use an external frequency-standard source.

Version 2 has the ability to start at 50MHz and then use the keypad + or - keys to increment or decrement the clock frequency, with an adjustment to the AGT time to keep millis() accurate. This version uses Wake-on-Interrupt for the loop() function, and also does some hardware floating-point maths and outputs a sine-wave with the DAC on A0.


![Halcyon-Electronics-OFS1-Frequency-Standard-10MHz-Lock-1_DxO](https://github.com/TriodeGirl/Arduino-UNO_R4_HOCO_to_External-Clock_on_EXTAL_pin_change/assets/139503623/26268804-e499-4f0e-8f62-81c4bc64ee68)
