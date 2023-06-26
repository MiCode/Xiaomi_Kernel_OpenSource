/*******************************************************************************
 * @file     platform.h
 * @author   USB PD Firmware Team
 *
 * Copyright 2018 ON Semiconductor. All rights reserved.
 *
 * This software and/or documentation is licensed by ON Semiconductor under
 * limited terms and conditions. The terms and conditions pertaining to the
 * software and/or documentation are available at
 * http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software").
 *
 * DO NOT USE THIS SOFTWARE AND/OR DOCUMENTATION UNLESS YOU HAVE CAREFULLY
 * READ AND YOU AGREE TO THE LIMITED TERMS AND CONDITIONS. BY USING THIS
 * SOFTWARE AND/OR DOCUMENTATION, YOU AGREE TO THE LIMITED TERMS AND CONDITIONS.
 ******************************************************************************/
#ifndef _FSC_PLATFORM_H_
#define _FSC_PLATFORM_H_

/*
 * PLATFORM_NONE
 *
 * This is a set of stubs for no platform in particular, or a starting point
 * for a new platform.
 */
#ifdef PLATFORM_NONE
#include "../Platform_None/FSCTypes.h"
#endif /* PLATFORM_NONE */

/*
 * PLATFORM_PIC32
 *
 * This platform is for the Microchip PIC32 microcontroller.
 */
#ifdef PLATFORM_PIC32
#include "../Platform_PIC32/GenericTypeDefs.h"
#include "../Platform_PIC32/Timing.h"
#endif /* PLATFORM_PIC32 */

/* PLATFORM_ARM
 *
 * This platform is for the ARM M0.
 */
#ifdef PLATFORM_ARM
#include "../Platform_ARM/src/FSCTypes.h"

#define TICK_SCALE_TO_MS    1000
#endif /* PLATFORM_ARM */

/* PLATFORM_ARM_M7
 *
 * This platform is for the ARM M7.
 */
#ifdef PLATFORM_ARM_M7
#include "../Platform_ARM_M7/app/FSCTypes.h"
#endif /* PLATFORM_ARM_M7 */

/* FSC_PLATFORM_LINUX
 *
 * This platform is for the Linux kernel driver.
 */
#ifdef FSC_PLATFORM_LINUX
#include "../Platform_Linux/FSCTypes.h"

#define TICK_SCALE_TO_MS    1000
#endif /* FSC_PLATFORM_LINUX */

#ifndef TICK_SCALE_TO_MS
#define TICK_SCALE_TO_MS    1   /* Fallback for time multiplier */
#endif /* TICK_SCALE_TO_MS */

#ifndef NUM_PORTS
#define NUM_PORTS           1   /* Number of ports in this system */
#endif /* NUM_PORTS */

/**
 * VBus switch levels
 */
typedef enum
{
    VBUS_LVL_5V,
    VBUS_LVL_HV,
    VBUS_LVL_ALL
} VBUS_LVL;


/**
 * Current port connection state
 */
typedef enum
{
    SINK = 0,
    SOURCE
} SourceOrSink;

/**
 * Events that platform uses to notify modules listening to the events.
 * The subscriber to event signal can subscribe to individual events or
 * a event in group.
 */
typedef enum
{
    CC1_ORIENT         =       0x00000001,
    CC2_ORIENT         =       0x00000002,
    CC_NO_ORIENT       =       0x00000004,
    CC_ORIENT_ALL      =       CC1_ORIENT | CC2_ORIENT | CC_NO_ORIENT,
    PD_NEW_CONTRACT    =       0x00000008,
    PD_NO_CONTRACT     =       0x00000010,
    PD_CONTRACT_ALL    =       PD_NEW_CONTRACT | PD_NO_CONTRACT,
    PD_STATE_CHANGED   =       0x00000020,
    ACC_UNSUPPORTED    =       0x00000040,
    BIST_DISABLED      =       0x00000100,
    BIST_ENABLED       =       0x00000200,
    BIST_ALL           =       BIST_ENABLED | BIST_DISABLED,
    ALERT_EVENT        =       0x00000400,
    POWER_ROLE         =       0x00004000,
    DATA_ROLE          =       0x00008000,
    AUDIO_ACC          =       0x00010000,
    CUSTOM_SRC         =       0x00020000,
    EVENT_ALL          =       0xFFFFFFFF,
} Events_t;

/**
 * @brief Set or return the current vbus voltage level as implemented by
 *        the platform (i.e. supply control, gpio switches, etc.)
 *
 * @param port ID for multiple port controls
 * @param level enumeration
 * @param enable TRUE = ON
 * @param disableOthers Disable other sources in make-before-break fashion
 * @return None or state of vbus.
 */
void platform_set_vbus_lvl_enable(FSC_U8 port, VBUS_LVL level, FSC_BOOL enable,
                                  FSC_BOOL disableOthers);

/**
 * @brief Check if the VBUS voltage is enabled
 * @param level VBUS level to check
 * @return TRUE if enabled
 */
FSC_BOOL platform_get_vbus_lvl_enable(FSC_U8 port, VBUS_LVL level);

/**
 * @brief Set or return programmable supply (PPS) voltage and current limit.
 *
 * @param port ID for multiple port controls
 * @param mv Voltage in millivolts
 * @return None or Value in mv/ma.
 */
void platform_set_pps_voltage(FSC_U8 port, FSC_U32 mv);

/**
 * @brief The function gets the current VBUS level supplied by PPS supply
 *
 * If VBUS is not enabled by the PPS supply the return type is undefined.
 *
 * @param port ID for multiple port controls
 * @return VBUS level supplied by PPS in milivolt resolution
 */
FSC_U16 platform_get_pps_voltage(FSC_U8 port);

/**
 * @brief Set the maximum current that can be supplied by PPS source
 * @param port ID for multiple port controls
 * @param ma Current in milliamps
 * @return None
 */
void platform_set_pps_current(FSC_U8 port, FSC_U32 ma);

/**
 * @brief Get the maximum current that the PPS supply is configured to provide
 *
 * If the PPS supply is not currently supplying current the return value is
 * undefined.
 *
 * @param port ID for multiple port controls
 * @return Current in milliamps
 */
FSC_U16 platform_get_pps_current(FSC_U8 port);

/**
 * @brief Enable/Disable VBus discharge path
 *
 * @param port ID for multiple port controls
 * @param enable TRUE = discharge path ON.
 * @return None
 */
void platform_set_vbus_discharge(FSC_U8 port, FSC_BOOL enable);

/**
 * @brief Enable/Disable VConn path
 *
 * Optional for platforms with separate VConn switch
 *
 * @param port ID for multiple port controls
 * @param enable TRUE = VConn path ON.
 * @return None
 */
void platform_set_vconn(FSC_U8 port, FSC_BOOL enable);

/**
 * @brief The current state of the device interrupt pin
 *
 * @param port ID for multiple port controls
 * @return TRUE if interrupt condition present.  Note: pin is active low.
 */
FSC_BOOL platform_get_device_irq_state_fusb302(FSC_U8 port);

/**
 * @brief Write a char buffer to the I2C peripheral.
 *
 * Assumes a single I2C bus.  If multiple buses are used, map based on
 * I2C address in the platform code.
 *
 * @param SlaveAddress - Slave device bus address
 * @param RegAddrLength - Register Address Byte Length
 * @param DataLength - Length of data to transmit
 * @param PacketSize - Maximum size of each transmitted packet
 * @param IncSize - Number of bytes to send before incrementing addr
 * @param RegisterAddress - Internal register address
 * @param Data - Buffer of char data to transmit
 * @return TRUE - success, FALSE otherwise
 */
FSC_BOOL platform_i2c_write(FSC_U8 SlaveAddress,
                            FSC_U8 RegAddrLength,
                            FSC_U8 DataLength,
                            FSC_U8 PacketSize,
                            FSC_U8 IncSize,
                            FSC_U32 RegisterAddress,
                            FSC_U8* Data);

/**
 * @brief Read char data from the I2C peripheral.
 *
 * Assumes a single I2C bus.  If multiple buses are used, map based on
 * I2C address in the platform code.
 *
 * @param SlaveAddress - Slave device bus address
 * @param RegAddrLength - Register Address Byte Length
 * @param DataLength - Length of data to attempt to read
 * @param PacketSize - Maximum size of each received packet
 * @param IncSize - Number of bytes to recv before incrementing addr
 * @param RegisterAddress - Internal register address
 * @param Data - Buffer for received char data
 * @return TRUE - success, FALSE otherwise
 */
FSC_BOOL platform_i2c_read( FSC_U8 SlaveAddress,
                            FSC_U8 RegAddrLength,
                            FSC_U8 DataLength,
                            FSC_U8 PacketSize,
                            FSC_U8 IncSize,
                            FSC_U32 RegisterAddress,
                            FSC_U8* Data);

/**
 * @brief Perform a blocking delay.
 *
 * @param delayCount - Number of 10us delays to wait
 * @return None
 */
void platform_delay_10us(FSC_U32 delayCount);

/**
 * @brief Perform a blocking delay.
 *
 * @param delayCount - Number of us delays to wait
 * @return None
 */
void platform_delay(FSC_U32 uSec);
/**
 * @brief Return a system timestamp for use with core timers.
 *
 * @param None
 * @return System time value in units of (milliseconds / TICK_SCALE_TO_MS)
 */
FSC_U32 platform_get_system_time(void);

/**
 * @brief Return a system timestamp for use with logging functions
 *
 * @param None
 * @return Packed timestamp - format: Upper 16: seconds, Lower 16: 0.1ms.
 */
FSC_U32 platform_get_log_time(void);


#ifdef FSC_HAVE_DP
/******************************************************************************
 * Function:        platform_dp_enable_pins
 * Input:           enable - If false put dp pins to safe state and config is
 *                           don't care. When true configure the pins with valid
 *                           config.
 *                  config - 32-bit port partner config. Same as type in
 *                  DisplayPortConfig_t in display_port_types.h.
 * Return:          TRUE - pin config succeeded, FALSE - pin config failed
 * Description:     enable/disable display port pins. If enable is true, check
 *                  the configuration bits[1:0] and the pin assignment
 *                  bits[15:8] to decide the appropriate configuration.
 ******************************************************************************/
FSC_BOOL platform_dp_enable_pins(FSC_BOOL enable, FSC_U32 config);

/******************************************************************************
 * Function:        platform_dp_status_update
 * Input:           status - 32-bit status value. Same as DisplayPortStatus_t
 *                  in display_port_types.h
 * Return:          None
 * Description:     Called when new status is available from port partner
 ******************************************************************************/
void platform_dp_status_update(FSC_U32 status);
#endif

#endif /* _FSC_PLATFORM_H_ */
