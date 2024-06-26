# Arduino-UNO_R4_HOCO_to_External-Clock_on_EXTAL_pin_change
Arduino UNO R4 RA4M1 HOCO to External-Clock on EXTAL pin change setup code. 
With 10.00MHz external input; over-clocking to 80MHz, and under-clocking down to 5MHz.

This is an update to the previous code to get accurate clocking of the RA4M1 processor  using an external crystal, to use an external frequency-standard source.

Version 2 has the ability to start at 50MHz and then use the keypad + or - keys to increment or decrement the clock frequency, with an adjustment to the AGT time to keep millis() accurate. This version uses Wake-on-Interrupt for the loop() function, and also does some hardware floating-point maths and outputs a sine-wave with the DAC on A0.

I am getting stable operation at 80MHz with the V2 sine-calc code. NEW: V2c has additional commands plus /2 and /4 options.


With Off-Air Frequency-Standard
![Halcyon-Electronics-OFS1-Frequency-Standard-10MHz-Lock-1_DxO](https://github.com/TriodeGirl/Arduino-UNO_R4_HOCO_to_External-Clock_on_EXTAL_pin_change/assets/139503623/26268804-e499-4f0e-8f62-81c4bc64ee68)


With GPS-Locked Frequency-Generator
![240519_Susan_Arduino_UNO-R4_Minima_RA4M1_external_pll-clocking_setup_1](https://github.com/TriodeGirl/Arduino-UNO_R4_HOCO_to_External-Clock_on_EXTAL_pin_change/assets/139503623/904144c5-a1bc-41aa-bdb2-77fe523b96db)
