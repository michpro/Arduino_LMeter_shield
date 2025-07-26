/**
 * @file definitions.h
 * @brief Global definitions and constants for LMeter project
 * @author MPro
 * @version 1.0
 * @copyright SPDX-FileCopyrightText: Copyright 2025 Michal Protasowicki
 * @license SPDX-License-Identifier: MIT
 * 
 * Contains hardware pin definitions, configuration constants, 
 * and system enumerations used throughout the project.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "SSD1306Ascii/SSD1306AsciiWire.h"
#include "SSD1306Ascii/SSD1306Ascii.h"
#include "EEPROMUtils.h"

// Hardware Configuration
const uint8_t       BUTTON_PIN              {3};                    // Pushbutton input pin (active low)
const uint8_t       COUNTER_PIN             {8};                    // Frequency counter input pin
const uint8_t       I2C_ADDRESS             {0x3C};                 // OLED display I2C address
const uint8_t       EE_ADDR_BASE            {0};                    // EEPROM base address for configuration storage

// Measurement Parameters
const uint8_t       OVF_COUNT_MIN           {64};                   // Minimum overflow count for valid measurement
const uint8_t       OVF_COUNT_MAX           {128};                  // Maximum overflow count before error state
const uint8_t       PPS_COUNT_MAX           {240};                  // PPS pulses for CPU calibration
const uint32_t      I2C_SPEED               {400000};               // I2C bus speed (400kHz)
const uint32_t      SPLASH_DELAY            {1500};                 // Splash screen display time (ms)
const uint32_t      CALIB_FAIL_TIMEOUT      {1500};                 // Calibration timeout duration (ms)
const uint32_t      LED_BLINK_TIME          {100};                  // LED PPS blink time (ms)
const uint32_t      DEFAULT_CPU_FREQUENCY   {16000000};             // Default CPU frequency (16MHz)
const uint32_t      SHIELD_FREQ_DIVIDER     {256};                  // Frequency divider
const uint32_t      MICROHENRY_DIVIDER      {1000000};              // µH divider
const float         ZERO_INDUCTANCE         {1.0};                  // Default zero calibration value (µH)
const float         CAPACITANCE             {0.0000000005};         // Generator circuit capacitance (500pF) => 1nF (C2) and 1nF (C3) in series
const float         TWO_PI_SQRT_CAP         {2 * PI * sqrt(CAPACITANCE)};

/**
 * @brief System state enumeration
 * 
 * Defines possible operating states of the L-Meter
 */
enum state_t : uint8_t
{
    ST_IDLE,                                                        // Idle state (waiting for events)
    ST_MEASURE,                                                     // Active measurement state
    ST_NO_INDUCT,                                                   // No inductance detected state
    ST_FCPU_CALIB,                                                  // CPU frequency calibration state
};

/**
 * @brief Display action enumeration
 * 
 * Controls how measurement results are displayed on OLED
 */
enum action_t : uint8_t
{
    ACTION_UPDATE,                                                  // Update existing display content
    ACTION_SHOW,                                                    // Show new display content (full refresh)
};
