/**
 * @file LMeter.ino
 * @brief Inductance measurement device using LC oscillator
 * @author MPro
 * @version 1.0
 * @copyright SPDX-FileCopyrightText: Copyright 2025 Michal Protasowicki
 * @license SPDX-License-Identifier: MIT
 * 
 * This program measures inductance by:
 * 1. Generating oscillations in an LC Colpitts circuit
 * 2. Measuring the oscillation frequency
 * 3. Calculating inductance using the known capacitance value
 * 4. Providing calibration and CPU frequency measurement functions
 */

#include "src/definitions.h"

// Global objects
SSD1306AsciiWire    oled;                                           // OLED display controller
EEPROMUtils         utils;                                          // EEPROM configuration manager
ConfigStruct        config;                                         // Device configuration parameters

// Volatile variables (modified in ISRs)
volatile bool       blink               {false};                    // LED blink request flag
volatile bool       timer2Started       {false};                    // Timer2 active status
volatile bool       fCpuCalibEnable     {false};                    // CPU frequency calibration enable
volatile uint8_t    ovfCount            {0};                        // Timer1 overflow counter
volatile uint16_t   captureCount        {0};                        // Input capture event counter
volatile uint16_t   captureCountLast    {0};                        // Last captured event count
volatile int32_t    totalCount          {0};                        // Total timer cycles (current)
volatile int32_t    totalCountLast      {0};                        // Total timer cycles (previous)
volatile uint32_t   fCpuOvfCount        {0};                        // Timer2 overflow counter
volatile uint8_t    ppsCount            {0};                        // PPS pulse counter
volatile state_t    state               {ST_IDLE};                  // Current system state

// State management variables
state_t             lastActState        {ST_IDLE};                  // Last active state
uint32_t            adjustedCpuFreq     {0};                        // Calibrated CPU frequency
uint32_t            currentMillis       {0};                        // Current time (ms)
uint32_t            previousMillis      {0};                        // Previous event time (ms)
uint32_t            fCpuCount           {0};                        // Calculated CPU frequency
uint8_t             ledState            {LOW};                      // Current LED state

// Measurement variables
float               measuredFreq;                                   // Measured oscillation frequency
float               measuredInduct;                                 // Calculated inductance
float               zeroCalib           {ZERO_INDUCTANCE};          // Zero calibration value
float               adjustedInduct;                                 // Calibrated inductance value

/**
 * @brief Timer1 overflow interrupt handler
 * 
 * Counts overflows and triggers "no inductance" state when overflow limit exceeded
 */
ISR(TIMER1_OVF_vect)
{
    ovfCount++;
    totalCount += 0x10000;                                          // Add 65536 (16-bit max + 1)
    if ((ovfCount > OVF_COUNT_MAX) && (ST_FCPU_CALIB != state))
    {
        state = ST_NO_INDUCT;
    }
}

/**
 * @brief Timer1 input capture interrupt handler
 * 
 * Captures timer value on signal edge and triggers measurement state
 */
ISR(TIMER1_CAPT_vect)
{
    captureCount++;
    if (ovfCount >= OVF_COUNT_MIN)
    {
        // Combine high and low bytes of capture register
        uint16_t timer_value {(uint16_t)ICR1L | (ICR1H << 8)};

        totalCountLast = totalCount + timer_value;
        totalCount = (int32_t)(0 - (int32_t)timer_value);
        captureCountLast = captureCount;
        captureCount = 0;
        ovfCount = 0;
        if (ST_FCPU_CALIB != state)
        {
            state = ST_MEASURE;
        }
    }
}

/**
 * @brief External interrupt 0 handler (PPS input)
 * 
 * Manages CPU frequency calibration process when enabled
 */
ISR(INT0_vect)
{
    if (true == fCpuCalibEnable)
    {
        if (false == timer2Started)
        {
            // Initialize Timer2 for frequency measurement
            TCNT2 = 0x00;                                           // Reset counter
            TCCR2B = _BV(CS20);                                     // Enable clock (no prescaling)
            TIMSK2 = _BV(TOIE2);                                    // Enable overflow interrupt
            ppsCount = 0;
            fCpuOvfCount = 0;
            timer2Started = true;
        }
        else
        {
            ppsCount++;
            if (ppsCount >= PPS_COUNT_MAX)
            {
                // Stop Timer2 after required pulse count
                TCCR2B = (0 << CS22) | (0 << CS21) | (0 << CS20);
                TIMSK2 = (0 << TOIE2);
                timer2Started = false;
            }
        }
        blink = true;
        state = ST_FCPU_CALIB;
    }
}

/**
 * @brief Timer2 overflow interrupt handler
 * 
 * Counts overflows during CPU frequency calibration
 */
ISR(TIMER2_OVF_vect)
{
    fCpuOvfCount++;
}

/**
 * @brief Display splash screen on OLED
 */
inline void showSplash(void)
{
    oled.clear();
    oled.setCursor(20, 0);
    oled.println(F("LMeter v1.0"));
    oled.setCursor(25, 4);
    oled.println(F("\xA9 by MPro"));
    delay(SPLASH_DELAY);
    oled.clear();
    oled.setCursor(0, 0);
    oled.print(F("CPU Freq:"));
    oled.setCursor(0, 4);
    oled.print(config.cpuFrequency);
    oled.setCursor(100, 4);
    oled.print(F("Hz"));
    delay(SPLASH_DELAY);
    oled.clear();
}

/**
 * @brief Display "no inductance detected" message
 * @param action Display action (SHOW or UPDATE)
 */
inline void showNoInduct(action_t action)
{
    bool isNanoHenry {abs(zeroCalib) < 1.0};

    if (ACTION_SHOW == action)
    {
        oled.setCursor(0, 0);
        oled.clearToEOL();
        oled.print(F("No Inductance"));
        oled.setCursor(0, 4);
        oled.clearToEOL();
        oled.print(F("Calib:"));
        oled.setCursor(100, 4);
        oled.print(isNanoHenry ? "nH" : "\xB5H");
    }

    oled.setCursor(40, 4);
    oled.print(isNanoHenry ? (zeroCalib * 1000.0) : zeroCalib);
}

/**
 * @brief Display inductance measurement results
 * @param action Display action (SHOW or UPDATE)
 */
inline void showMeasurement(action_t action)
{
    bool isNanoHenry {abs(adjustedInduct) < 1.0};
    bool isMiliHenry {abs(adjustedInduct) >= 1000.0};

    if (ACTION_SHOW == action)
    {
        oled.setCursor(0, 0);
        oled.clearToEOL();
        oled.print(F("L:"));
        oled.setCursor(0, 4);
        oled.clearToEOL();
        oled.print(F("F:"));
        oled.setCursor(100, 4);
        oled.print(F("kHz"));
    }

    oled.clear(15, 99, 0, 7);
    oled.setCursor(15, 4);
    oled.print(measuredFreq / 1000.0);
    oled.setCursor(15, 0);
    oled.print(isNanoHenry ? (adjustedInduct * 1000) : 
             (isMiliHenry ? (adjustedInduct / 1000) : adjustedInduct));
    oled.setCursor(100, 0);
    oled.clearToEOL();
    oled.print(isNanoHenry ? F("n") : (isMiliHenry ? F("m") : F("\xB5")));
    oled.print(F("H"));
}

/**
 * @brief Perform CPU frequency calibration
 * 
 * Uses external 1PPS signal to calibrate internal clock frequency
 */
inline void doFcpuCalibration(void)
{
    if (true == blink)
    {
        // Update display with calibration data
        ledState = HIGH;
        digitalWrite(LED_BUILTIN, ledState);
        previousMillis = currentMillis;
        blink = false;

        oled.setCursor(0, 0);
        oled.clearToEOL();
        oled.print(F("Cnt:"));
        oled.print(ppsCount);

        if ((ppsCount > 0))
        {
            fCpuCount = fCpuOvfCount << 8;                          // Multiply by 256
            if (false == timer2Started)
            {
                fCpuCount += TCNT2;                                 // Add current timer value
                oled.setCursor(85, 0);
                oled.print(F("SAVE"));
            }
            fCpuCount = fCpuCount / ppsCount;
            if (false == timer2Started)
            {
                // Save calibrated frequency
                adjustedCpuFreq = fCpuCount * SHIELD_FREQ_DIVIDER;
                config.cpuFrequency = fCpuCount;
                utils.updateConfigInEEPROM(config);
            }
        }
        else
        {
            fCpuCount = 0;
        }
        
        oled.setCursor(0, 4);
        oled.clearToEOL();
        oled.print(F("F:"));
        oled.print(fCpuCount);
    }
    
    // Handle calibration timeout
    if (currentMillis - previousMillis >= CALIB_FAIL_TIMEOUT)
    {
        TCCR2B = (0 << CS22) | (0 << CS21) | (0 << CS20);
        TIMSK2 = (0 << TOIE2);
        timer2Started = false;
        lastActState = ST_IDLE;
        state = ST_IDLE;
    }
}

/**
 * @brief Initialize device configuration
 * 
 * Loads configuration from EEPROM or initializes default values
 */
void initializeConfig(void)
{
    utils.getConfigFromEEPROM(config);
    if ((config.state == IST_FACTORY) || 
        (config.CRC != utils.calculateConfigCRC(config)))
    {
        utils.initializeEEPROMVariables(config);
    }
}

/**
 * @brief Arduino setup function
 * 
 * Initializes hardware peripherals and system configuration
 */
void setup()
{
    // Initialize I2C
    Wire.begin();
    Wire.setClock(I2C_SPEED);

    // Initialize OLED display
    oled.begin(&SH1106_128x64, I2C_ADDRESS);
    oled.setFont(Antonio32);
    oled.setLetterSpacing(2);

    // Configure I/O pins
    pinMode(BUTTON_PIN, INPUT);
    pinMode(COUNTER_PIN, INPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    // Factory reset if button pressed
    if (LOW == digitalRead(BUTTON_PIN))
    {
        utils.initializeEEPROMVariables(config);
    }

    // Load configuration and show splash
    initializeConfig();
    showSplash();

    // Calculate adjusted CPU frequency
    adjustedCpuFreq = config.cpuFrequency * SHIELD_FREQ_DIVIDER;

    // Configure Timer1
    TCCR1A = 0x00;                                                  // Normal port operation
    TCCR1B = _BV(CS10);                                             // No prescaling, falling edge trigger
    TIMSK1 = _BV(ICIE1) | _BV(TOIE1);                               // Enable capture and overflow interrupts

    // Configure external interrupt
    EICRA = _BV(ISC01) | _BV(ISC00);                                // Rising edge trigger
    EIMSK = _BV(INT0);                                              // Enable INT0 interrupt

    // Configure Timer2
    TCCR2A = 0x00;                                                  // Normal port operation
    TCCR2B = 0x00;                                                  // Clock disabled
    TIFR2 = 0x00;                                                   // Clear interrupt flags
    TIMSK2 = 0x00;                                                  // Disable overflow interrupt

    // Enable CPU frequency calibration
    fCpuCalibEnable = true;
}

/**
 * @brief Arduino main loop
 * 
 * Handles state machine and measurement processing
 */
void loop()
{
    currentMillis = millis();

    // State machine processing
    switch (state)
    {
        case ST_FCPU_CALIB:
            doFcpuCalibration();
            break;
            
        case ST_MEASURE:
            // Calculate frequency: (pulses * CPU freq) / timer cycles
            measuredFreq = (float)(((int64_t)captureCountLast * adjustedCpuFreq) / totalCountLast);
            
            // Calculate inductance: L = 1 / [(2πf)^2 * C]
            measuredInduct = measuredFreq * TWO_PI_SQRT_CAP;                            // 2π√C
            measuredInduct = MICROHENRY_DIVIDER / (measuredInduct * measuredInduct);    // L = 1/((2πf)^2 * C)

            // Zero calibration when button pressed
            if (LOW == digitalRead(BUTTON_PIN))
            {
                zeroCalib = measuredInduct;
            }
            
            // Apply calibration
            adjustedInduct = measuredInduct - zeroCalib;
            
            // Update display
            showMeasurement((ST_MEASURE == lastActState) ? ACTION_UPDATE : ACTION_SHOW);
            lastActState = ST_MEASURE;
            state = ST_IDLE;
            break;
            
        case ST_NO_INDUCT:
            showNoInduct((ST_NO_INDUCT == lastActState) ? ACTION_UPDATE : ACTION_SHOW);
            lastActState = ST_NO_INDUCT;
            state = ST_IDLE;
            break;
            
        default:
            break;
    }

    // LED blink timeout handling
    if (HIGH == ledState)
    {
        if (currentMillis - previousMillis >= LED_BLINK_TIME)
        {
            ledState = LOW;
            digitalWrite(LED_BUILTIN, ledState);
        }
    }
}
