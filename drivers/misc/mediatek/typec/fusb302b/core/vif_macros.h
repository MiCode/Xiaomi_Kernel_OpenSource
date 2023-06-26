/*******************************************************************************
 * @file     vif_macros.h
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
#ifndef VENDOR_MACROS_H
#define VENDOR_MACROS_H

#include "PD_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PORT_SUPPLY_FIRST_FIXED(idx)\
   {.FPDOSupply = {\
                      .MaxCurrent        = Src_PDO_Max_Current##idx,\
                      .Voltage           = Src_PDO_Voltage##idx,\
                      .PeakCurrent       = Src_PDO_Peak_Current##idx,\
                      .DataRoleSwap      = DR_Swap_To_UFP_Supported,\
                      .USBCommCapable    = USB_Comms_Capable,\
                      .ExternallyPowered = Unconstrained_Power,\
                      .USBSuspendSupport = SWAP(USB_Suspend_May_Be_Cleared),\
                      .DualRolePower     = Accepts_PR_Swap_As_Src,\
                      .SupplyType        = Src_PDO_Supply_Type##idx,\
    }}

#define PORT_SUPPLY_TYPE_0(idx)\
   {.FPDOSupply = {\
                      .MaxCurrent        = Src_PDO_Max_Current##idx,\
                      .Voltage           = Src_PDO_Voltage##idx,\
                      .PeakCurrent       = Src_PDO_Peak_Current##idx,\
                      .SupplyType        = Src_PDO_Supply_Type##idx,\
    }}

#define PORT_SUPPLY_TYPE_1(idx)\
    { .BPDO = {\
                 .MaxPower   = Src_PDO_Max_Power##idx,\
                 .MinVoltage = Src_PDO_Min_Voltage##idx,\
                 .MaxVoltage = Src_PDO_Max_Voltage##idx,\
                 .SupplyType = Src_PDO_Supply_Type##idx,\
    }}

#define PORT_SUPPLY_TYPE_2(idx)\
    { .VPDO = {\
                  .MaxCurrent = Src_PDO_Max_Current##idx,\
                  .MinVoltage = Src_PDO_Min_Voltage##idx,\
                  .MaxVoltage = Src_PDO_Max_Voltage##idx,\
                  .SupplyType = Src_PDO_Supply_Type##idx,\
    }}

#define PORT_SUPPLY_TYPE_3(idx)\
    { .PPSAPDO = {\
                     .MaxCurrent = Src_PDO_Max_Current##idx,\
                     .MinVoltage = Src_PDO_Min_Voltage##idx,\
                     .MaxVoltage = Src_PDO_Max_Voltage##idx,\
                     .SupplyType = Src_PDO_Supply_Type##idx,\
     }}

#define PORT_SUPPLY_TYPE_(idx, type)      PORT_SUPPLY_TYPE_##type(idx)
#define CREATE_SUPPLY_PDO(idx, type)      PORT_SUPPLY_TYPE_(idx, type)
#define CREATE_SUPPLY_PDO_FIRST(idx)      PORT_SUPPLY_FIRST_FIXED(idx)

#define PORT_SINK_TYPE_0(idx)\
    { .FPDOSink = {\
                       .OperationalCurrent = Snk_PDO_Op_Current##idx,\
                       .Voltage            = Snk_PDO_Voltage##idx,\
                       .DataRoleSwap       = DR_Swap_To_DFP_Supported,\
                       .USBCommCapable     = USB_Comms_Capable,\
                       .ExternallyPowered  = Unconstrained_Power,\
                       .HigherCapability   = Higher_Capability_Set,\
                       .DualRolePower      = Accepts_PR_Swap_As_Snk,\
                       .SupplyType         = pdoTypeFixed,\
    }}

#define PORT_SINK_TYPE_1(idx)\
    { .BPDO = {\
                 .MaxPower   = Snk_PDO_Op_Power##idx,\
                 .MinVoltage = Snk_PDO_Min_Voltage##idx,\
                 .MaxVoltage = Snk_PDO_Max_Voltage##idx,\
                 .SupplyType = Snk_PDO_Supply_Type3##idx,\
    }}

#define PORT_SINK_TYPE_2(idx)\
    { .VPDO = {\
                  .MaxCurrent = Snk_PDO_Op_Current##idx,\
                  .MinVoltage = Snk_PDO_Min_Voltage##idx,\
                  .MaxVoltage = Snk_PDO_Max_Voltage##idx,\
                  .SupplyType = Snk_PDO_Supply_Type##idx,\
    }}

#define PORT_SINK_TYPE_3(idx)\
    { .PPSAPDO = {\
                     .MaxCurrent          = Snk_PDO_Op_Current##idx,\
                     .MinVoltage          = Snk_PDO_Min_Voltage##idx,\
                     .MaxVoltage          = Snk_PDO_Max_Voltage##idx,\
                     .SupplyType          = Snk_PDO_Supply_Type##idx,\
     }}

#define PORT_SINK_TYPE_(idx, type)      PORT_SINK_TYPE_##type(idx)
#define CREATE_SINK_PDO(idx, type)      PORT_SINK_TYPE_(idx, type)

/******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************/
extern FSC_U8 gCountry_codes[];

void VIF_InitializeSrcCaps(doDataObject_t *src_caps);
void VIF_InitializeSnkCaps(doDataObject_t *snk_caps);

/** Helpers **/
#define YES 1
#define NO 0
#define SWAP(X) ((X) ? 0 : 1)

#ifdef __cplusplus
}
#endif

#endif /* VENDOR_MACROS_H */

