/**
 * @file EEPROMUtils.h
 * @brief EEPROMUtils Class definition
 * @author MPro
 * @version 1.0
 * @copyright SPDX-FileCopyrightText: Copyright 2025 Michal Protasowicki
 * @license SPDX-License-Identifier: MIT
 */

#pragma once

#include "definitions.h"

/**
 *  @brief States used in the configuration structure to mark its initialization
 *  [0xFF is the default value in cleared EEPROM].
**/
typedef enum : uint8_t
{
    IST_INITIALIZED = 0xA5,
    IST_FACTORY     = 0xFF
}initState_t;

/**
 *  @brief Structure that stores device configuration parameters.
**/
struct ConfigStruct
{
    uint32_t        cpuFrequency;                                   // CPU Frequency used for calculations
    initState_t     state;                                          // after the first start of the device and saving the default configuration, it is set to 'true'
    uint16_t        CRC;
};

/**
 *  @brief EEPROMUtils Class definitions.
**/
class EEPROMUtils
{
public:
    void eraseEEPROM();
    uint16_t calculateCRC(void* data, uint8_t size);
    uint16_t calculateConfigCRC(ConfigStruct& conf);
    void updateConfigInEEPROM(ConfigStruct& conf);
    void getConfigFromEEPROM(ConfigStruct& conf);
    void initializeEEPROMVariables(ConfigStruct& conf);
};
