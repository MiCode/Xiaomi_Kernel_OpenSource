/*******************************************************************************
 * @file     TypeC_Types.h
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
#ifndef __FSC_TYPEC_TYPES_H__
#define __FSC_TYPEC_TYPES_H__

/**
 * @brief UUT Device Type specifies the device type from a compliance standpoint
 */
typedef enum {
    UUT_Consumer = 0,
    UUT_ConsumerProvider,
    UUT_ProviderConsumer,
    UUT_Provider,
    UUT_DRP,
    UUT_Cable,
    UUT_TypeCOnly,
    UUT_Undefined = 99
} UUTDeviceType;

/**
 * @brief Port type defines whether the port is Source, Sink, or DRP
 */
typedef enum {
    USBTypeC_Sink = 0,
    USBTypeC_Source,
    USBTypeC_DRP,
    USBTypeC_Debug,
    USBTypeC_UNDEFINED = 99
} USBTypeCPort;

/**
 * CC pin orientation
 */
typedef enum
{
    CCNone,
    CC1,
    CC2
} CCOrientation;

/**
 * @brief Type-C state machine state enum
 */
typedef enum {
    Disabled = 0,
    ErrorRecovery,
    Unattached,
    AttachWaitSink,
    AttachedSink,
    AttachWaitSource,
    AttachedSource,
    TrySource,
    TryWaitSink,
    TrySink,
    TryWaitSource,
    AudioAccessory,
    DebugAccessorySource,
    AttachWaitAccessory,
    PoweredAccessory,
    UnsupportedAccessory,
    DelayUnattached,
    UnattachedSource,
    DebugAccessorySink,
    AttachWaitDebSink,
    AttachedDebSink,
    AttachWaitDebSource,
    AttachedDebSource,
    TryDebSource,
    TryWaitDebSink,
    UnattachedDebSource,
    IllegalCable,
} ConnectionState;

/**
 * @brief Defines possible CC pin terminations (from either side)
 */
typedef enum {
    CCTypeOpen = 0,
    CCTypeRa,
    CCTypeRdUSB,
    CCTypeRd1p5,
    CCTypeRd3p0,
    CCTypeUndefined
} CCTermType;

/**
 * @brief Defines pins available in the connector
 */
typedef enum {
    TypeCPin_None = 0,
    TypeCPin_GND1,
    TypeCPin_TXp1,
    TypeCPin_TXn1,
    TypeCPin_VBUS1,
    TypeCPin_CC1,
    TypeCPin_Dp1,
    TypeCPin_Dn1,
    TypeCPin_SBU1,
    TypeCPin_VBUS2,
    TypeCPin_RXn2,
    TypeCPin_RXp2,
    TypeCPin_GND2,
    TypeCPin_GND3,
    TypeCPin_TXp2,
    TypeCPin_TXn2,
    TypeCPin_VBUS3,
    TypeCPin_CC2,
    TypeCPin_Dp2,
    TypeCPin_Dn2,
    TypeCPin_SBU2,
    TypeCPin_VBUS4,
    TypeCPin_RXn1,
    TypeCPin_RXp1,
    TypeCPin_GND4
} TypeCPins_t;

/**
 * @brief Defines the possible source current advertisements
 */
typedef enum {
    utccNone = 0,
    utccDefault,
    utcc1p5A,
    utcc3p0A,
    utccInvalid,
} USBTypeCCurrent;

#endif /* __FSC_TYPEC_TYPES_H__ */

