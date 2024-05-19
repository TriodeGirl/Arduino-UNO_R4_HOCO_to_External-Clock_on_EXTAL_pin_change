/*  Arduino UNO R4 code for switching to external-clock e.g. from 10MHz Frequency Standard
 *  ... USB clock remains on HCHO 48MHz
 *
 *  Sucessfully run at 80MHz with this simple code test, without any changes to the normal RA4M1 clock setups,
 *  ... also reduce PLL Multiplier to 2x (data-sheet minimum says 8x) to run at 5MHz (with /4 divide).
 *
 *  Uses Wake-From-Interrupt to remove unneeded loop() spinning when in PLL mode.
 *
 *  Take care about maximum signal levels on EXTAL pin input:
 *  ... my source is 1.6V P/P with 50 ohm terminatino - then A/C coupled to EXTAL pin with 330pF cap.
 *
 *  WARNING - Using higher clock frequencies is HIGHLY EXPERIMENTAL - PLL for 80MHz max!!!
 *
 *  The DAC output with a sine generated signal can be used to check code operation. 
 *  ... On my Minima the program runs at 85MHz but the sine generation shows misplaced levels for several points
 *
 *  If in Wake-From-Interrupt mode will need to Double-Press R4's Reset Button to put into bootloader mode when updating code.
 *
 *  Susan Parker - 6th May 2024.
 *    Tested on Minima
 *    USB Serial module remains on HOCO clock source... 
 *      so non-standard frequencies can be used in the range 4 MHz to 12.5 MHz
 *      e.g. 12.288 MHZ - Adjust PLL settings to match.
 *
 *  Susan Parker - 18/19th May 2024.
 *    Startup remains with HCHO clocking
 *    Extended code for start and stoping PLL mode
 *    Add /2 or /4 divider options
 *
 * This code is "AS IS" without warranty or liability. 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

// ARM-developer - Accessing memory-mapped peripherals
// https://developer.arm.com/documentation/102618/0100

// ==== System & Clock Generation ====
#define SYSTEM 0x40010000 // ICU Base - See 13.2.6 page 233

// Register Write Protection - See section 12
// PRC0 Registers related to the clock generation circuit:
//   SCKDIVCR, SCKSCR, PLLCR, PLLCCR2, MEMWAIT, MOSCCR, HOCOCR, MOCOCR, CKOCR, TRCKCR,
//   OSTDCR, OSTDSR, SLCDSCKCR, EBCKOCR, MOCOUTCR, HOCOUTCR, MOSCWTCR, MOMCR, SOSCCR,
//   SOMCR, LOCOCR, LOCOUTCR, HOCOWTCR, USBCKCR
//
//   *SYSTEM_PRCR = 0xA501;     // Enable writing to the clock registers
//   *SYSTEM_PRCR = 0xA500;     // Disable writing to the clock registers

#define SYSTEM_PRCR  ((volatile unsigned short *)(SYSTEM + 0xE3FE))    // Protect Register
#define PRCR_PRC0           0   // Enables or disables writing to clock generation registers
#define PRCR_PRC1           1   // En/Dis writing to the low power modes and battery backup function registers
#define PRCR_PRC3           3   // Enables or disables writing to the registers related to the LVD
#define PRCR_PRKEY_7_0      8   // Control write access to the PRCR register.
#define PRCR_PRKEY       0xA5   // PRC Key Code - write to the upper 8 bits

#define SYSTEM_SCKDIVCR  ((volatile unsigned int *)(SYSTEM + 0xE020))  // System Clock Division Control Register
                                // SYSTEM_SCKDIVCR = 100010100 
#define SCKDIVCR_PCKD_2_0   0   // Peripheral Module Clock D           = 4; 1/16
#define SCKDIVCR_PCKC_2_0   4   // Peripheral Module Clock C           = 1; 1/2
#define SCKDIVCR_PCKB_2_0   8   // Peripheral Module Clock B           = 1; 1/2
#define SCKDIVCR_PCKA_2_0  12   // Peripheral Module Clock A           = 0
#define SCKDIVCR_ICK_2_0   24   // System Clock (ICLK) Select          = 0
#define SCKDIVCR_FCK_2_0   28   // Flash Interface Clock (FCLK) Select = 0
#define SYSTEM_SCKSCR  ((volatile unsigned char *)(SYSTEM + 0xE026))  // System Clock Source Control Register
#define SCKSCR_CKSEL_2_0    0   // Clock Source Select - See section 8.2.2
#define SYSTEM_PLLCR   ((volatile unsigned char *)(SYSTEM + 0xE02A))  // PLL Control Register
#define PLLCR_PLLSTP        0   // PLL Stop Control; 0: PLL is operating, 1: PLL is stopped
#define SYSTEM_PLLCCR2 ((volatile unsigned char *)(SYSTEM + 0xE02B))  // PLL Clock Control Register 2
#define PLLCCR2_PLLMUL_4_0  0   // PLL Frequency Multiplication Factor Select
#define PLLCCR2_PLODIV_1_0  6   // PLL Output Frequency Division Ratio Select
#define SYSTEM_MEMWAIT ((volatile unsigned char *)(SYSTEM + 0xE031))  // Memory Wait Cycle Control Register
#define MEMWAIT_MEMWAIT     0   // Memory Wait Cycle Select; 0: No wait, 1: Wait
#define SYSTEM_MOSCCR   ((volatile unsigned char *)(SYSTEM + 0xE032))  // Main Clock Oscillator Control Register
#define MOSCCR_MOSTP        0   // Main Clock Oscillator Stop; 0: Main clock oscillator is operating, 1: MCO is stopped
#define SYSTEM_HOCOCR   ((volatile unsigned char *)(SYSTEM + 0xE036))  // High-Speed On-Chip Oscillator Control Register
#define HOCOCR_HCSTP        0   // HOCO Stop; 0: HOCO is operating, 1: HOCO is stopped
#define SYSTEM_MOCOCR   ((volatile unsigned char *)(SYSTEM + 0xE038))  // Middle-Speed On-Chip Oscillator Control Register
#define MOCOCR_MCSTP        0   // MOCO Stop; 0: MOCO is operating, 1: MOCO is stopped
#define SYSTEM_OSCSF    ((volatile unsigned char *)(SYSTEM + 0xE03C))  // Oscillation Stabilization Flag Register
#define OSCSF_HOCOSF        0   // HOCO Clock Oscillation Stabilization Flag; 0: The HOCO clock is stopped or not stable, 1: The clock is stable
#define OSCSF_MOSCSF        3   // Main Clock Oscillation Stabilization Flag; 0: The Main clock is stopped or not stable, 1: The clock is stable
#define OSCSF_PLLSF         5   // PLL  Clock Oscillation Stabilization Flag; 0: The PLL  clock is stopped or not stable, 1: The clock is stable
#define SYSTEM_CKOCR    ((volatile unsigned char *)(SYSTEM + 0xE03E))  // Clock Out Control Register
#define CKOCR_CKOSEL_2_0    0   // Clock Out Source Select; 000: HOCO, 001: MOCO, 010: LOCO, 011: MOSC, 100: SOSC
#define CKOCR_CKODIV_2_0    4   // Clock Out Input Frequency Division Select; 000: ×1, 001: /2, 010: /4, ... , 111: /128
#define CKOCR_CKOEN         7   // Clock Out Enable; 0: Disable clock out, 1: Enable clock out
#define SYSTEM_TRCKCR   ((volatile unsigned char *)(SYSTEM + 0xE03F))  // Trace Clock Control Register
#define TRCKCR_TRCK_3_0     0   // Trace Clock Operation Frequency Select; 0000: /1, 0001: /2, 0010: /4 ( /2 = value after reset )
#define TRCKCR_TRCKEN       7   // Trace Clock Operating Enable; 0: Disable clock, 1: Enable clock

#define SYSTEM_OSTDCR   ((volatile unsigned char *)(SYSTEM + 0xE040))  // Oscillation Stop Detection Control Register
#define OSTDCR_OSTDIE       0   // Oscillation Stop Detection Interrupt Enable; 0: Disable oscillation stop detection interrupt, 1: Enable OSDI
#define OSTDCR_OSTDE        7   // Oscillation Stop Detection Function Enable; 0: Disable the oscillation stop detection function, 1: Enable OSDF
#define SYSTEM_OSTDSR   ((volatile unsigned char *)(SYSTEM + 0xE041))  // Oscillation Stop Detection Control Register
#define OSTDSR_OSTDF        0   // Oscillation Stop Detection Flag; 0: Main clock oscillation stop not detected, 1: Main clock oscillation stop detected

#define SYSTEM_SLCDSCKCR    ((volatile unsigned char *)(SYSTEM + 0xE050))  // Segment LCD Source Clock Control Register
#define SLCDSCKCR_LCDSCKSEL_2_0  0  // LCD Source Clock Select; 000: LOCO, 001: SOSC, 010: MOSC, 100: HOCO
#define SLCDSCKCR_LCDSCKEN       7  // LCD Source Clock Out Enable; 0: LCD source clock out disabled, 1: LCD source clock out enabled

#define SYSTEM_MOCOUTCR   ((volatile unsigned char *)(SYSTEM + 0xE061))  // MOCO User Trimming Control Register
#define MOCOUTCR_MOCOUTRM_7_0   0  // MOCO User Trimming - See: 8.2.21
#define SYSTEM_HOCOUTCR   ((volatile unsigned char *)(SYSTEM + 0xE062))  // HOCO User Trimming Control Register
#define HOCOUTCR_HOCOUTRM_7_0   0  // HOCO User Trimming - See: 8.2.21

#define SYSTEM_MOSCWTCR ((volatile unsigned char *)(SYSTEM + 0xE0A2))  // Main Clock Oscillator Wait Control Register
#define MOSCWTCR_MSTS_3_0   0   // Main Clock Oscillator Wait Time Setting
#define SYSTEM_HOCOWTCR ((volatile unsigned char *)(SYSTEM + 0xE0A5))  // High-Speed On-Chip Oscillator Wait Control Register
#define HOCOWTCR_MSTS_2_0   0   // HOCO Wait Time Setting

#define SYSTEM_USBCKCR  ((volatile unsigned char *)(SYSTEM + 0xE0D0))  // USB Clock Control Register
#define USBCKCR_USBCLKSEL   0   // USB Clock Source Select; 0: PLL (value after reset), 1: HOCO

#define SYSTEM_MOMCR    ((volatile unsigned char *)(SYSTEM + 0xE413))  // Main Clock Oscillator Mode Oscillation Control Register
#define MOMCR_MODRV1        3   // Main Clock Oscillator Drive Capability 1 Switching; 0: 10 MHz to 20 MHz, 1: 1 MHz to 10 MHz
#define MOMCR_MOSEL         6   // Main Clock Oscillator Switching; 0: Resonator, 1: External clock input

#define SYSTEM_SOSCCR   ((volatile unsigned char *)(SYSTEM + 0xE480))  // Sub-Clock Oscillator Control Register
#define SOSCCR_SOSTP        0   // Sub-Clock Oscillator Stop; 0: Operate the sub-clock oscillator, 1: Stop the sub-clock osc
#define SYSTEM_SOMCR    ((volatile unsigned char *)(SYSTEM + 0xE481))  // Sub-Clock Oscillator Mode Control Register
#define SOMCR_SODRV_1_0     0   // Sub-Clock Oscillator Drive Capability Switching; 00: Normal, 01: Low-power 1, 10: Low-power 2, 11: Low-power 3

#define SYSTEM_LOCOCR   ((volatile unsigned char *)(SYSTEM + 0xE490))  // Low-Speed On-Chip Oscillator Control Register
#define LOCOCR_LCSTP        0   // LOCO Stop; 0: Operate the LOCO clock, 1: Stop the LOCO clock
#define SYSTEM_LOCOUTCR ((volatile unsigned char *)(SYSTEM + 0xE492))  // LOCO User Trimming Control Register
#define LOCOUTCR_LOCOUTRM_7_0   0  // LOCO User Trimming - See: 8.2.20

#define SYSTEM_RSTSR0   ((volatile unsigned char *)(SYSTEM + 0xE410))  // Reset Status Register 0
#define RSTSR0_PORF         0    // Power-On Reset Detect Flag
#define RSTSR0_LVD0RF       1    // Voltage Monitor 0 Reset Detect Flag
#define RSTSR0_LVD1RF       2    // Voltage Monitor 1 Reset Detect Flag
#define RSTSR0_LVD2RF       3    // Voltage Monitor 2 Reset Detect Flag
#define SYSTEM_RSTSR1   ((volatile unsigned char *)(SYSTEM + 0xE0C0))  // Reset Status Register 1
#define RSTSR1_IWDTRF       0    // Independent Watchdog Timer Reset Detect Flag
#define RSTSR1_WDTRF        1    // Watchdog Timer Reset Detect Flag
#define RSTSR1_SWRF         2    // Software Reset Detect Flag
#define SYSTEM_RSTSR2   ((volatile unsigned char *)(SYSTEM + 0xE411))  // Reset Status Register 2
#define RSTSR2_CWSF         0    // Cold/Warm Start Determination Flag - 0: Cold start, 1: Warm start

// Low Power Mode Control - See datasheet section 10
#define SYSTEM_SBYCR   ((volatile unsigned short *)(SYSTEM + 0xE00C))      // Standby Control Register
#define SYSTEM_MSTPCRA ((volatile unsigned int   *)(SYSTEM + 0xE01C))      // Module Stop Control Register A

#define MSTP 0x40040000 // Module Registers
#define MSTP_MSTPCRB   ((volatile unsigned int   *)(MSTP + 0x7000))      // Module Stop Control Register B
#define MSTPB2   2 // CAN0
#define MSTPB8   8 // IIC1
#define MSTPB9   9 // IIC0
#define MSTPB18 18 // SPI1
#define MSTPB19 19 // SPI0
#define MSTPB22 22 // SCI9
#define MSTPB29 29 // SCI2
#define MSTPB30 30 // SCI1
#define MSTPB31 31 // SCI0

#define MSTP_MSTPCRC   ((volatile unsigned int   *)(MSTP + 0x7004))      // Module Stop Control Register C
#define MSTP_MSTPCRD   ((volatile unsigned int   *)(MSTP + 0x7008))      // Module Stop Control Register D
#define MSTPD2   2 // AGT1   - Asynchronous General Purpose Timer 1 Module
#define MSTPD3   3 // AGT0   - Asynchronous General Purpose Timer 0 Module
#define MSTPD5   5 // GPT320 and GPT321 General 32 bit PWM Timer Module
#define MSTPD6   6 // GPT162 to GPT167 General 16 bit PWM Timer Module
#define MSTPD14 14 // POEG   - Port Output Enable for GPT Module Stop
#define MSTPD16 16 // ADC140 - 14-Bit A/D Converter Module
#define MSTPD19 19 // DAC8   -  8-Bit D/A Converter Module
#define MSTPD20 20 // DAC12  - 12-Bit D/A Converter Module
#define MSTPD29 29 // ACMPLP - Low-Power Analog Comparator Module
#define MSTPD31 31 // OPAMP  - Operational Amplifier Module

// The Mode Control bits are read as 1, the write value should be 1.
// Bit value 0: Cancel the module-stop state 
// Bit value 1: Enter the module-stop state.

// =========== Ports ============
// 19.2.5 Port mn Pin Function Select Register (PmnPFS/PmnPFS_HA/PmnPFS_BY) (m = 0 to 9; n = 00 to 15)

// 32 bits - Use for setting pin functions to other than default pin I/O

#define P000PFS 0x0800  // Port 0 Pin Function Select Register
#define PFS_P000PFS ((volatile unsigned int *)(PORTBASE + P000PFS))            // A1 / D15 - AN00 - AMP0+ - IRQ6
#define PFS_P001PFS ((volatile unsigned int *)(PORTBASE + P000PFS + ( 1 * 4))) // A2 / D16 - AN01 - AMP0- - IRQ7
#define PFS_P002PFS ((volatile unsigned int *)(PORTBASE + P000PFS + ( 2 * 4))) // A3 / D17 - AN03 - AMP0O - IRQ2
#define PFS_P003PFS ((volatile unsigned int *)(PORTBASE + P000PFS + ( 3 * 4))) // N/C - AN03
#define PFS_P004PFS ((volatile unsigned int *)(PORTBASE + P000PFS + ( 4 * 4))) // N/C - AN04 - IRQ3
#define PFS_P005PFS ((volatile unsigned int *)(PORTBASE + P000PFS + ( 5 * 4))) // N/A - AN11 - IRQ10
#define PFS_P006PFS ((volatile unsigned int *)(PORTBASE + P000PFS + ( 6 * 4))) // N/A - AN12
#define PFS_P007PFS ((volatile unsigned int *)(PORTBASE + P000PFS + ( 7 * 4))) // N/A - AN13
#define PFS_P008PFS ((volatile unsigned int *)(PORTBASE + P000PFS + ( 8 * 4))) // N/A - AN14
// #define PFS_P009PFS ((volatile unsigned int *)(PORTBASE + P000PFS + ( 9 * 4))) // Does not exist
#define PFS_P010PFS ((volatile unsigned int *)(PORTBASE + P000PFS + (10 * 4))) // N/A - AN05
#define PFS_P011PFS ((volatile unsigned int *)(PORTBASE + P000PFS + (11 * 4))) // N/C - AN06 - IRQ15
#define PFS_P012PFS ((volatile unsigned int *)(PORTBASE + P000PFS + (12 * 4))) // TxLED - AN07
#define PFS_P013PFS ((volatile unsigned int *)(PORTBASE + P000PFS + (13 * 4))) // RxLED - AN08
#define PFS_P014PFS ((volatile unsigned int *)(PORTBASE + P000PFS + (14 * 4))) // A0 / D14 - AN09 - DAC0
#define PFS_P015PFS ((volatile unsigned int *)(PORTBASE + P000PFS + (15 * 4))) // N/C - AN10 - IRQ7

#define P100PFS 0x0840  // Port 1 Pin Function Select Register
#define PFS_P100PFS ((volatile unsigned int *)(PORTBASE + P100PFS))            // A5 / D19 - MISOA - SCL1 - IRQ2
#define PFS_P101PFS ((volatile unsigned int *)(PORTBASE + P100PFS + ( 1 * 4))) // A4 / D18 - MOSIA - SDA1 - IRQ1
#define PFS_P102PFS ((volatile unsigned int *)(PORTBASE + P100PFS + ( 2 * 4))) // D5 - RSPCKA
#define PFS_P103PFS ((volatile unsigned int *)(PORTBASE + P100PFS + ( 3 * 4))) // D4 - SSLA0
#define PFS_P104PFS ((volatile unsigned int *)(PORTBASE + P100PFS + ( 4 * 4))) // D3 - IRQ1
#define PFS_P105PFS ((volatile unsigned int *)(PORTBASE + P100PFS + ( 5 * 4))) // D2 - IRQ0
#define PFS_P106PFS ((volatile unsigned int *)(PORTBASE + P100PFS + ( 6 * 4))) // D6
#define PFS_P107PFS ((volatile unsigned int *)(PORTBASE + P100PFS + ( 7 * 4))) // D7
#define PFS_P108PFS ((volatile unsigned int *)(PORTBASE + P100PFS + ( 8 * 4))) // SWD p2 SWDIO
#define PFS_P109PFS ((volatile unsigned int *)(PORTBASE + P100PFS + ( 9 * 4))) // D11 / MOSI
#define PFS_P110PFS ((volatile unsigned int *)(PORTBASE + P100PFS + (10 * 4))) // D12 / MISO - IRQ3
#define PFS_P111PFS ((volatile unsigned int *)(PORTBASE + P100PFS + (11 * 4))) // D13 / SCLK - IRQ4
#define PFS_P112PFS ((volatile unsigned int *)(PORTBASE + P100PFS + (12 * 4))) // D10 / CS
#define PFS_P113PFS ((volatile unsigned int *)(PORTBASE + P100PFS + (13 * 4))) // N/C
#define PFS_P114PFS ((volatile unsigned int *)(PORTBASE + P100PFS + (14 * 4))) // N/A
#define PFS_P115PFS ((volatile unsigned int *)(PORTBASE + P100PFS + (15 * 4))) // N/A

#define P200PFS 0x0880  // Port 2 Pin Function Select Register
#define PFS_P200PFS ((volatile unsigned int *)(PORTBASE + P200PFS))           // NMI
#define PFS_P201PFS ((volatile unsigned int *)(PORTBASE + P200PFS + (1 * 4))) // MD
#define PFS_P202PFS ((volatile unsigned int *)(PORTBASE + P200PFS + (2 * 4))) // N/A
#define PFS_P203PFS ((volatile unsigned int *)(PORTBASE + P200PFS + (3 * 4))) // N/A
#define PFS_P204PFS ((volatile unsigned int *)(PORTBASE + P200PFS + (4 * 4))) // LOVE (Heart Pad on underside of board)
#define PFS_P205PFS ((volatile unsigned int *)(PORTBASE + P200PFS + (5 * 4))) // N/C - IRQ1 - CLKOUT
#define PFS_P206PFS ((volatile unsigned int *)(PORTBASE + P200PFS + (6 * 4))) // N/C - IRQ0
// Pins P212, P213, P214, and P215 are Crystal functions, or N/C

#define PORTBASE 0x40040000 // Port Base 
#define PFS_P100PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843))   // 8 bits - A5
#define PFS_P101PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + ( 1 * 4))) // A4
#define PFS_P102PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + ( 2 * 4))) // D5
#define PFS_P103PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + ( 3 * 4))) // D4
#define PFS_P104PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + ( 4 * 4))) // D3
#define PFS_P105PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + ( 5 * 4))) // D2
#define PFS_P106PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + ( 6 * 4))) // D6
#define PFS_P107PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + ( 7 * 4))) // D7
#define PFS_P108PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + ( 8 * 4))) // SWDIO
#define PFS_P109PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + ( 9 * 4))) // D11 / MOSI
#define PFS_P110PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + (10 * 4))) // D12 / MISO
#define PFS_P111PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + (11 * 4))) // D13 / SCLK
#define PFS_P112PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + (12 * 4))) // D10 / CS
#define PFS_P300PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x08C3))            // SWCLK (P300)
#define PFS_P301PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x08C3 + (01 * 4))) // D0 / RxD (P301)
#define PFS_P302PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x08C3 + (02 * 4))) // D1 / TxD (P302) 
#define PFS_P303PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x08C3 + (03 * 4))) // D9
#define PFS_P304PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x08C3 + (04 * 4))) // D8

// ====  Asynchronous General Purpose Timer (AGT) =====
#define AGTBASE 0x40084000
#define AGT0_AGT    ((volatile unsigned short *)(AGTBASE))         // AGT Counter Register
#define AGT1_AGT    ((volatile unsigned short *)(AGTBASE + 0x100))
#define AGT0_AGTCMA ((volatile unsigned short *)(AGTBASE + 0x002)) // AGT Compare Match A Register
#define AGT1_AGTCMA ((volatile unsigned short *)(AGTBASE + 0x102))
#define AGT0_AGTCMB ((volatile unsigned short *)(AGTBASE + 0x004)) // AGT Compare Match B Register
#define AGT1_AGTCMB ((volatile unsigned short *)(AGTBASE + 0x104))

// 8 bit registers
#define AGT0_AGTCR    ((volatile unsigned char  *)(AGTBASE + 0x008))  // AGT Control Register
#define AGT1_AGTCR    ((volatile unsigned char  *)(AGTBASE + 0x108))  //
#define AGTCR_TSTART 0  // R/W - AGT Count Start; 1: Count starts, 0: Count stops
#define AGTCR_TCSTF  1  // R   - AGT Count Status Flag; 1: Count in progress, 0: Count is stopped
#define AGTCR_TSTOP  2  // W   - AGT Count Forced Stop; 1: The count is forcibly stopped, 0: Writing 0 is invalid!!!
#define AGT0_AGTMR1   ((volatile unsigned char  *)(AGTBASE + 0x009))  // AGT Mode Register 1
#define AGT1_AGTMR1   ((volatile unsigned char  *)(AGTBASE + 0x109))  //
#define AGT0_AGTMR2   ((volatile unsigned char  *)(AGTBASE + 0x00A))  // AGT Mode Register 2
#define AGT1_AGTMR2   ((volatile unsigned char  *)(AGTBASE + 0x10A))  //
#define AGT0_AGTIOC   ((volatile unsigned char  *)(AGTBASE + 0x00C))  // AGT I/O Control Register
#define AGT1_AGTIOC   ((volatile unsigned char  *)(AGTBASE + 0x10C))  //
#define AGTIOC_TOE   2  // AGTOn Output Enable
#define AGT0_AGTISR   ((volatile unsigned char  *)(AGTBASE + 0x00D))  // AGT Event Pin Select Register
#define AGT1_AGTISR   ((volatile unsigned char  *)(AGTBASE + 0x10D))  //
#define AGT0_AGTCMSR  ((volatile unsigned char  *)(AGTBASE + 0x00E))  // AGT Compare Match Function Select Register
#define AGT1_AGTCMSR  ((volatile unsigned char  *)(AGTBASE + 0x10E))  //
#define AGT0_AGTIOSEL ((volatile unsigned char  *)(AGTBASE + 0x00F))  // AGT Pin Select Register
#define AGT1_AGTIOSEL ((volatile unsigned char  *)(AGTBASE + 0x10F))  //

// 12-Bit D/A Converter
#define DACBASE 0x40050000          // DAC Base - DAC output on A0 (P014 AN09 DAC)
#define DAC12_DADR0    ((volatile unsigned short *)(DACBASE + 0xE000))      // D/A Data Register 0 
#define DAC12_DACR     ((volatile unsigned char  *)(DACBASE + 0xE004))      // D/A Control Register
#define DAC12_DADPR    ((volatile unsigned char  *)(DACBASE + 0xE005))      // DADR0 Format Select Register
#define DAC12_DAADSCR  ((volatile unsigned char  *)(DACBASE + 0xE006))      // D/A A/D Synchronous Start Control Register
#define DAC12_DAVREFCR ((volatile unsigned char  *)(DACBASE + 0xE007))      // D/A VREF Control Register


// === Local Defines

#define MAIN_CLOCK_MHz      50.00        // Set in multiples of 5 MHz i.e. 45, 50, 55, 60
#define EXT_CLOCK_MHz       10.00        // Set to external input or XTAL frequency

#define AGT0_RELOAD_48MHz   2999      // AGT0 value to give 1mS / 1kHz - with 48.0MHz/2 PCLKB
#define AGT0_RELOAD_50MHz   3124      // AGT0 value to give 1mS / 1kHz - with 50.0MHz/2 PCLKB
#define AGT0_RELOAD_55MHz   3436      // AGT0 value to give 1mS / 1kHz - with 55.0MHz/2 PCLKB - Note runs sligtly fast
#define AGT0_RELOAD_60MHz   3749      // AGT0 value to give 1mS / 1kHz - with 60.0MHz/2 PCLKB

// #define AGT0_RELOAD  AGT0_RELOAD_60MHz
#define AGT0_RELOAD  (int)((62.5 * MAIN_CLOCK_MHz ) - 1)

#define CLOCK_PLL      // Use with external clock
#define CLKOUT         // Enable clock_monitor on D11 to confirm external-clock source is active

#define DAC_RIGHT_JUST
#define FREQ_UPDATE_INTERVAL 0x04000000   // use a 0x01, 02, 04, 08 value for MSB - 0x04 gives c. 15.6 Hz 

#define MIN_PLL_MULTI   1
#define MAX_PLL_MULTI  31

uint32_t currentMillis  = 0x0;
uint32_t previousMillis = 0x0;
uint32_t lastMillis = 0x0;
uint32_t print_interval_secs = 60;

uint32_t secondsCount = 0;
bool minute_flag = false;

uint16_t agt_reload_val = AGT0_RELOAD;
char pll_multiplier = 9;
char pll_divider = 2;
static bool ext_clk_good = false;
static bool wake_from_int_flag = false;

void setup()
  {
  Serial.begin(115200);      // The interrupts for the USB serial are already in place before setup() starts
  while (!Serial){};         // 
  clock_pll_commands();

  setup_dac();

#ifdef CLKOUT
  sys_clock_monitor(ext_clk_good);
#endif

  previousMillis = lastMillis = currentMillis = millis();
  }

void loop()
  {
  *PFS_P111PFS_BY = 0x04;        // D13 - CLEAR LED
  if(wake_from_int_flag == true)
    asm volatile("wfi");           // Stop here and wait for an interrupt
  *PFS_P111PFS_BY = 0x05;        // D13 - Set LED

  currentMillis = millis();

  if((currentMillis - lastMillis) >= 1)  // Only do an update once per mS
    {
   	lastMillis = currentMillis;
    calc_sine();
    }

  if((currentMillis - previousMillis) >= 1000) 
    {
   	previousMillis = currentMillis;
    secondsCount++;
    Serial.print(".");          // print a "." once per second - Proof of Life !
    if(secondsCount >= 60)  // 
      {
      secondsCount = 0;
      minute_flag = true;       // Change flag to Blip a LED
      if(ext_clk_good == true)
        {
        Serial.print("\nClock = "); 
        if(pll_divider == 2)
          {
          Serial.println((float)(pll_multiplier + 1) * (float)(EXT_CLOCK_MHz / 2));
          }
        else
          {
          Serial.println((float)(pll_multiplier + 1) * (float)(EXT_CLOCK_MHz / 4));
          }
        }
      else
        {
        Serial.println("\nHCHO Clock"); 
        }

      minute_flag = false;
      }
    }

  if(Serial.available() > 0)
    {
    char input_char = Serial.read();  // get one byte from serial buffer
    Serial.println(input_char);

    switch(input_char)
      {
      case '2':
        {
        if(pll_divider == 4)
          {
          pll_divider = 2;
          pll_multiplier = (pll_multiplier >> 1);   // As in /2
          set_sys_clock_hoco();                     // Set to HCOC first before changing PLL settings
          ext_clk_good = sys_clock_pll_setup(pll_multiplier, pll_divider);
          secondsCount = 0;
          if(ext_clk_good == true)
            {
            wake_from_int_flag = true;
            sys_clock_monitor(ext_clk_good);
            } 
          }
        break;
        }
      case '4':
        {
        if(pll_divider == 2)
          {
          pll_divider = 4;
          pll_multiplier = (pll_multiplier << 1);   // As in *2
          set_sys_clock_hoco();                     // Set to HCOC first before changing PLL settings
          ext_clk_good = sys_clock_pll_setup(pll_multiplier, pll_divider);
          secondsCount = 0;
          if(ext_clk_good == true)
            {
            wake_from_int_flag = true;
            sys_clock_monitor(ext_clk_good);
            } 
          }
        break;
        }
      case '+':
        {
        pll_multiplier++;
        if(pll_multiplier > MAX_PLL_MULTI)
          pll_multiplier = MAX_PLL_MULTI;
        set_sys_clock_hoco();              // Set to HCOC first before changing PLL settings
        ext_clk_good = sys_clock_pll_setup(pll_multiplier, pll_divider);
        secondsCount = 0;
        if(ext_clk_good == true)
          {
          wake_from_int_flag = true;
          sys_clock_monitor(ext_clk_good);
          } 
        break;
        }
      case '-':
        {
        pll_multiplier--;
        if(pll_multiplier < MIN_PLL_MULTI)
          pll_multiplier = MIN_PLL_MULTI;
        set_sys_clock_hoco();             // Set to HCOC first before changing PLL settings
        ext_clk_good = sys_clock_pll_setup(pll_multiplier, pll_divider);
        secondsCount = 0;
        if(ext_clk_good == true)
          {
          wake_from_int_flag = true;
          sys_clock_monitor(ext_clk_good);
          } 
        break;
        }
      case 's':
        {
        pll_multiplier = (char)((MAIN_CLOCK_MHz / (float)(EXT_CLOCK_MHz / 2)) - 1); 
        pll_divider = 2;
        ext_clk_good = sys_clock_pll_setup(pll_multiplier, pll_divider);
        if(ext_clk_good == true)
          {
          Serial.println("Ext Clock Running.");
          sys_clock_monitor(ext_clk_good);
          wake_from_int_flag = true; 
          }
        else
          {
          Serial.println("No Ext Clock.");
          sys_clock_monitor(ext_clk_good);
          }   
        secondsCount = 0;
        break;
        }
      case 'r':
        {
        set_sys_clock_hoco();             // Change back to HCOC
        Serial.println("\nHOCO System clock");
        pll_multiplier = (int)((MAIN_CLOCK_MHz / (float)(EXT_CLOCK_MHz / 2)) - 1);  // Return to base PLL setting
        pll_divider = 2;
        *AGT0_AGT = AGT0_RELOAD_48MHz;
        secondsCount = 0;
        ext_clk_good = false;
        sys_clock_monitor(ext_clk_good);
        wake_from_int_flag = false;       // Disable WFI, otherwsie double tap on Reset-Button needed
        break;
        }
      case '?':
        {
        clock_pll_commands();
        break;
        }
      default:
        break;
      }
    }
  }

void clock_pll_commands(void)
  {
  Serial.println("\nPLL Clock Test");
  Serial.println("Code starts with Internal HCHO Clock");
  Serial.println("PLL Clocking uses Wake-From-Interrupt");
  Serial.println("Single Char Clock Update PLL Commands");
  Serial.println("s = Start PLL Clocking with /2 & *10 settings");
  Serial.println("r = Return to HCHO 48MHz Clocking");
  Serial.println("+ = Increase PLL Multiplier by 1");
  Serial.println("- = Decrease PLL Multiplier by 1");
  Serial.println("2 = Use /2 PLL Divider");
  Serial.println("4 = Use /4 PLL Divider");
  Serial.println("? = This Help List");
  }

void calc_sine()
  {
          int32_t out, a , b;
          int32_t result_taylor_sine;
   static int32_t sine_amp_local = 0x7FFFFFFF;
  static uint32_t freq_time_accumulate = 0;
//  static uint32_t freq_time_delta = 0x1 << 24;  //  
  static uint32_t freq_time_delta = 0x1 << 18;  // Using a shift makes sure 0x1 value gives static repeating waveform
          int16_t result_sine;

  int32_t angle, sum, p1, p2, p3, p5, p7, p9, p11, term; 
  uint32_t ph;
  uint16_t analog_write_value;


  freq_time_accumulate += FREQ_UPDATE_INTERVAL;   // Core DDS frequency-synthesis incriment

  if (freq_time_accumulate >= 0xC0000000u || freq_time_accumulate < 0x40000000u)
    {
    *PFS_P103PFS_BY = 0x05;      // Pulse on D4 to trigger scope 
    angle = (int32_t)freq_time_accumulate; // valid from -90 to +90 degrees
    } 
  else
    {
    *PFS_P103PFS_BY = 0x04;      // Each Port Output bit clear or set takes c. 83nS 
    angle = (int32_t)(0x7FFFFFFFu - freq_time_accumulate);
    }
  term = angle << 1;
	asm volatile("smmulr %0, %1, %2" : "=r" (p1) : "r" (term), "r" (1686629713));
	asm volatile("smmulr %0, %1, %2" : "=r" (term) : "r" (p1), "r" (p1));
  p2 = term << 3;
	asm volatile("smmulr %0, %1, %2" : "=r" (term) : "r" (p2), "r" (p1));
  p3 = term << 3;
  term = p1 << 1;
	asm volatile("smmlsr %0, %2, %3, %1" : "=r" (sum) : "r" (term), "r" (p3), "r" (1431655765));
	asm volatile("smmulr %0, %1, %2" : "=r" (term) : "r" (p3), "r" (p2));
  p5 = term << 1;
	asm volatile("smmlar %0, %2, %3, %1" : "=r" (sum) : "r" (sum), "r" (p5), "r" (286331153));
	asm volatile("smmulr %0, %1, %2" : "=r" (p7) : "r" (p5), "r" (p2));
	asm volatile("smmlsr %0, %2, %3, %1" : "=r" (sum) : "r" (sum), "r" (p7), "r" (54539267));
/*  
// Uncommet this for 25bit precision calculation
	asm volatile("smmulr %0, %1, %2" : "=r" (p9) : "r" (p7), "r" (p2));
	asm volatile("smmlar %0, %2, %3, %1" : "=r" (sum) : "r" (sum), "r" (p9), "r" (6059919));
	asm volatile("smmulr %0, %1, %2" : "=r" (p11) : "r" (p9), "r" (p2));
	asm volatile("smmlsr %0, %2, %3, %1" : "=r" (sum) : "r" (sum), "r" (p11), "r" (440721));
*/

  result_taylor_sine = sum << 1;   

#ifdef DAC_RIGHT_JUST
  result_sine = (int16_t)(result_taylor_sine >> 20);   // 32 - 12 = 20
  *DAC12_DADR0 = result_sine + 2048;
#else
  result_sine = (int16_t)(result_taylor_sine >> 16);   // 
  *DAC12_DADR0 = result_sine + 32767;
#endif
  }

void setup_dac(void)       // Note make sure ADC is stopped before setup DAC
  {
  *MSTP_MSTPCRD &= (0xFFFFFFFF - (0x01 << MSTPD20));  // Enable DAC12 module
#ifdef DAC_RIGHT_JUST
  *DAC12_DADPR    = 0x00;        // DADR0 Format Select Register - Set right-justified format
#else
  *DAC12_DADPR    = 0x80;        // DADR0 Format Select Register - Set left-justified format i.e. 16 bit format, 4 LSBs not used
#endif
//  *DAC12_DAADSCR  = 0x80;        // D/A A/D Synchronous Start Control Register - Enable
  *DAC12_DAADSCR  = 0x00;        // D/A A/D Synchronous Start Control Register - Default
// 36.3.2 Notes on Using the Internal Reference Voltage as the Reference Voltage
  *DAC12_DAVREFCR = 0x00;        // D/A VREF Control Register - Write 0x00 first - see 36.2.5
  *DAC12_DADR0    = 0x0000;      // D/A Data Register 0 
   delayMicroseconds(10);        
  *DAC12_DAVREFCR = 0x01;        // D/A VREF Control Register - Select AVCC0/AVSS0 for Vref
//  *DAC12_DAVREFCR = 0x03;        // D/A VREF Control Register - Select Internal reference voltage/AVSS0
//  *DAC12_DAVREFCR = 0x06;        // D/A VREF Control Register - Select External Vref; set VREFH&L pins used for LEDs
  *DAC12_DACR     = 0x5F;        // D/A Control Register - 
   delayMicroseconds(5);         // 
  *DAC12_DADR0    = 0x0800;      // D/A Data Register 0 
  *PFS_P014PFS   = 0x00000000;   // Make sure all cleared
  *PFS_P014PFS  |= (0x1 << 15);  // Port Mode Control - Used as an analog pin
  }


void set_sys_clock_hoco(void)         // Set to HCOC first before changing PLL settings
  {
  *SYSTEM_PRCR       = 0xA501;        // Enable writing to the clock registers
  *SYSTEM_SCKSCR     = 0x00;          // Select HOCO as the system clock 
  *SYSTEM_PRCR       = 0xA500;        // Disable writing to the clock registers
  }

bool sys_clock_pll_setup(char pll_set_val, char pll_div_val)
  {
  Serial.println("Setup MOSC - External Clock & PLL"); 
  *SYSTEM_PRCR     = 0xA501;          // Enable writing to the clock registers
  *SYSTEM_MOSCCR   = 0x01;            // Make sure MOSC is stopped - 8.2.6
//  The External Input EXTAL mode does not work, leave as resonator.
//  *SYSTEM_MOMCR    = 0x40;            // MOSEL - Main Clock Oscillator Switching - 1: External clock input - 8.2.17
  *SYSTEM_MOMCR    = 0x00;            // MOSEL - Main Clock Oscillator Switching - 0: Resonator - 8.2.17
  *SYSTEM_MOSCWTCR = 0x00;            // Set stability timeout period - With External Clock not required
	asm volatile("dsb");                // Data bus Synchronization instruction
  *SYSTEM_MOSCCR   = 0x00;            // Enable External Clock
  delay(100);                         // wait for XTAL to stabilise  
  char enable_ok = *SYSTEM_MOSCCR;    // Check bit 
  Serial.print("SYSTEM_MOSCCR = "); 
  if((int)enable_ok == 0)
    Serial.println("0: Main clock oscillator is operating");
  else
    {
    Serial.println("1: Main clock oscillator is stopped");
    return(false);
    }

// PLL Setup - See Datasheet 8.2.3 and 8.2.4
  *SYSTEM_PLLCR    = 0x01;              // Disable PLL
  delayMicroseconds(4);                 // wait for PLL to stop
  *SYSTEM_PLLCCR2  = pll_div_val << 5;              // Setup PLLCCR2_PLODIV_1_0 PLL Output Frequency Division to /2
  Serial.print("PLL Divider = ");                   // 00000010 << 5 = 01000000
  Serial.println((*SYSTEM_PLLCCR2 & 0xC0) >> 5); 
  *SYSTEM_PLLCCR2 |= pll_set_val;       // Setup PLLCCR2_PLLMUL_4_0 PLL PLL Frequency Multiplication
  Serial.print("PLL Frequency Multiplication = "); 
  Serial.println((*SYSTEM_PLLCCR2 & 0x1F)+1); 
  delayMicroseconds(1);               // wait for PLL config change
  *SYSTEM_PLLCR    = 0x00;            // Enable PLL
  delayMicroseconds(1000);            // wait for PLL to stabilise
  Serial.print("Change System Clock source from "); 
  Serial.print(*SYSTEM_SCKSCR); 
  Serial.print(" to "); 
  *SYSTEM_SCKSCR   = 0x05;            // Select PLL as the system clock 
  Serial.println(*SYSTEM_SCKSCR); 

  enable_ok = *SYSTEM_OSTDSR;    // Check bit 
  Serial.print("SYSTEM_OSTDSR = "); 
  if((int)enable_ok == 0)
    Serial.println("0: Main clock oscillation stop not detected");
  else
    Serial.println("1: Main clock oscillation stop detected");

  *SYSTEM_PRCR     = 0xA500;          // Disable writing to the clock registers

  Serial.print("Clock = "); 
  if(pll_div_val == 2)
    {
    Serial.println((float)(pll_multiplier + 1) * (float)(EXT_CLOCK_MHz / 2));
    }
  else
    {
    Serial.println((float)(pll_multiplier + 1) * (float)(EXT_CLOCK_MHz / 4));
    }

  Serial.print("AGT0_RELOAD = ");
  if(pll_div_val == 2)                //  Calculate value for AGT0 mS counter 1mS update rate
    {
    agt_reload_val = (short int)((62.5 * ((pll_multiplier + 1) * (float)(EXT_CLOCK_MHz / 2)) ) - 1);
    *AGT0_AGT = agt_reload_val;
    Serial.println(agt_reload_val);  // Print value NOT register as this will aleady have decrimented
    }
  else
    {
    agt_reload_val = (short int)((62.5 * ((pll_multiplier + 1) * (float)(EXT_CLOCK_MHz / 4)) ) - 1);
    *AGT0_AGT = agt_reload_val; 
    Serial.println(agt_reload_val);
    }

  return(true);
  }

void sys_clock_monitor(bool source_flag)
  {
#ifdef CLKOUT
  Serial.println("System Clock Monitor on D11"); 
  *SYSTEM_PRCR     = 0xA501;          // Enable writing to the clock registers
  if(source_flag == true)
    *SYSTEM_CKOCR  =  0b00000011;     // Divide by 0, MOSC source
  else
    *SYSTEM_CKOCR  =  0b00010000;     // Divide by 4, HOCO source
  *PFS_P109PFS   = (0b01001 << 24);   // Select PSEL[4:0] for CLKOUT - See Table 19.7
  *SYSTEM_CKOCR |=  0b10000000;       // Enable Output
  *PFS_P109PFS  |= (0x1 << 16);       // Port Mode Control - Used as an I/O port for peripheral function
  *SYSTEM_PRCR     = 0xA500;          // Disable writing to the clock registers
#endif
  }
