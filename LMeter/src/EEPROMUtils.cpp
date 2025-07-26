/**
 * @file EEPROMUtils.cpp
 * @brief EEPROMUtils Class implementation
 * @author MPro
 * @version 1.0
 * @copyright SPDX-FileCopyrightText: Copyright 2025 Michal Protasowicki
 * @license SPDX-License-Identifier: MIT
 */

#include <util/crc16.h>
#include <EEPROM.h>
#include <Arduino.h>

#include "EEPROMUtils.h"

/**
 *  @brief Free function calling device reset.
**/
void(* ResetFunc) (void) = 0x00;

/**
 *  @brief A method that clears the entire EEPROM of the device.
**/
void EEPROMUtils::eraseEEPROM()
{
    for (uint16_t idx = 0; idx < EEPROM.length(); idx++)
    {
        EEPROM.update(idx, 0xFF);
    }
    delay(500);
    ResetFunc();
}

/**
 *  @brief Auxiliary method for calculating CRC16.
 *  @param data pointer to data to calculate CRC.
 *  @param size data size (in bytes) for CRC calculation.
 *  @return CRC16 value.
**/
uint16_t EEPROMUtils::calculateCRC(void* data, uint8_t size)
{
    uint16_t crc {0x0000};

    if ((data != nullptr) && (size > 0))
    { 
        crc = 0xFEED;
        for (uint8_t idx = 0; idx < size; idx++)
        {
            crc = _crc16_update(crc, *(static_cast<uint8_t*>(data) + idx));
        }
    }

    return crc;
}

/**
 *  @brief A method to calculate the CRC16 of the configuration structure.
 *  @param conf A reference to a structure containing configuration data.
 *  @return CRC16 value.
**/
uint16_t EEPROMUtils::calculateConfigCRC(ConfigStruct& conf)
{
    return calculateCRC(&conf, (sizeof(ConfigStruct) - sizeof(uint16_t)));
}

/**
 *  @brief Updates configuration data stored in the EEPROM memory.
 *  @param conf A reference to a structure containing configuration data.
**/
void EEPROMUtils::updateConfigInEEPROM(ConfigStruct& conf)
{
    conf.CRC = calculateConfigCRC(conf);
    EEPROM.put(EE_ADDR_BASE, conf);
}

/**
 *  @brief Reference to the structure into which configuration data will be read from the EEPROM memory.
 *  @param conf A reference to a structure containing configuration data.
**/
void EEPROMUtils::getConfigFromEEPROM(ConfigStruct& conf)
{
    EEPROM.get(EE_ADDR_BASE, conf);
}

/**
 *  @brief Initialization with default values ​​of the structure that stores the device configuration parameters.
 *  @param conf A reference to a structure containing configuration data.
 *  @param i2cSpeed The default clock speed of the I2C bus.
**/
void EEPROMUtils::initializeEEPROMVariables(ConfigStruct& conf)
{
    conf.cpuFrequency           = DEFAULT_CPU_FREQUENCY;
    conf.state                  = IST_INITIALIZED;
    updateConfigInEEPROM(conf);
}
