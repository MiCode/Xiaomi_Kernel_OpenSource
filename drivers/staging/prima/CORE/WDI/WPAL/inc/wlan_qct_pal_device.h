/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef WLAN_QCT_PAL_DEVICE_H
#define WLAN_QCT_PAL_DEVICE_H
/* ====================================================================================================================

   @file   wlan_qct_pal_device.h

   @brief
    This file contains the external API exposed by WLAN PAL Device specific functionalities
    Copyright (c) 2011 Qualcomm Incorporated. All Rights Reserved
    Qualcomm Confidential and Properietary

 * ==================================================================================================================*/

/* ====================================================================================================================
                  EDIT HISTORY FOR FILE

   This section contains comments describing changes made to the module.
   Notice that changes are listed in reverse chronological order

   When             Who                 What, Where, Why
   ---------        --------            -------------------------------------------------------------------------------
   FEB/07/11        sch                 Create module
 * ==================================================================================================================*/

/* ====================================================================================================================
                  INCLUDE FILES FOR MODULES
 * ==================================================================================================================*/
#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
#include "wlan_qct_pal_trace.h"

/* ====================================================================================================================
                  PREPROCESSORS AND DEFINITIONS
 * ==================================================================================================================*/
#define     DXE_INTERRUPT_TX_COMPLE      0x02
#define     DXE_INTERRUPT_RX_READY       0x04
#define     WPAL_ISR_CLIENT_MAX          0x08

#define     WPAL_SMSM_WLAN_TX_ENABLE          0x00000400
#define     WPAL_SMSM_WLAN_TX_RINGS_EMPTY     0x00000200

typedef enum
{
   WPAL_DEBUG_START_HEALTH_TIMER = 1<<0,
   WPAL_DEBUG_TX_DESC_RESYNC     = 1<<1,
} WPAL_DeviceDebugFlags;
/* ====================================================================================================================
  @  Function Name 
      wpalIsrType

  @  Description 
      DXE ISR functio prototype
      DXE should register ISR function into platform

  @  Parameters
      pVoid                       pDXEContext : DXE module control block

  @  Return
      NONE
 * ==================================================================================================================*/
typedef void (* wpalIsrType)(void *usrCtxt);

/* ====================================================================================================================
                  GLOBAL FUNCTIONS
 * ==================================================================================================================*/
/* ====================================================================================================================
   @ Function Name

   @ Description

   @ Arguments

   @ Return value

   @ Note

 * ==================================================================================================================*/
wpt_status wpalDeviceInit
(
   void                 *deviceCB
);

/* ====================================================================================================================
   @ Function Name

   @ Description

   @ Arguments

   @ Return value

   @ Note

 * ==================================================================================================================*/
wpt_status wpalDeviceClose
(
   void                 *deviceC
);

/* ==========================================================================
                  CLIENT SERVICE EXPOSE FUNCTIONS GENERIC
 * =========================================================================*/
/**
  @brief wpalRegisterInterrupt provides a mechansim for client
         to register support for a given interrupt

  The DXE interface supports two interrupts, TX Complete and RX
  Available.  This interface provides the mechanism whereby a client
  can register to support one of these.  It is expected that the core
  DXE implementation will invoke this API twice, once for each interrupt.
  
  @param  intType:          Enumeration of the interrupt type (TX or RX)
  @param  callbackFunction: ISR function pointer
  @param  usrCtxt:          User context passed back whenever the
                            callbackFunction is invoked

  @return SUCCESS if the registration was successful
*/
wpt_status wpalRegisterInterrupt
(
   wpt_uint32                           intType,
   wpalIsrType                          callbackFunction,
   void                                *usrCtxt
);

/**
  @brief wpalUnRegisterInterrupt provides a mechansim for client
         to un-register for a given interrupt

  When DXE stop, remove registered information from PAL
  
  @param  intType:          Enumeration of the interrupt type (TX or RX)

  @return NONE
*/

void wpalUnRegisterInterrupt
(
   wpt_uint32      intType
);

/**
  @brief wpalEnableInterrupt provides a mechansim for a client
         to request that a given interrupt be enabled

  The DXE interface supports two interrupts, TX Complete and RX
  Available.  This interface provides the mechanism whereby a client
  can request that the platform-specific adaptation layer allows a
  given interrupt to occur.  The expectation is that if a given
  interrupt is not enabled, if the interrupt occurs then the APPS CPU
  will not be interrupted.
  
  @param  intType:          Enumeration of the interrupt type (TX or RX)

  @return SUCCESS if the interrupt was enabled
*/
wpt_status wpalEnableInterrupt
(
   wpt_uint32                          intType
);

/**
  @brief wpalDisableInterrupt provides a mechansim for a client
         to request that a given interrupt be disabled

  The DXE interface supports two interrupts, TX Complete and RX
  Available.  This interface provides the mechanism whereby a client
  can request that the platform-specific adaptation layer not allow a
  given interrupt to occur.  The expectation is that if a given
  interrupt is not enabled, if the interrupt occurs then the APPS CPU
  will not be interrupted.
  
  @param  intType:          Enumeration of the interrupt type (TX or RX)

  @return SUCCESS if the interrupt was disabled
*/
wpt_status wpalDisableInterrupt
(
   wpt_uint32                           intType
);

/**
  @brief wpalWriteRegister provides a mechansim for a client
         to write data into a hardware data register

  @param  address:  Physical memory address of the register
  @param  data:     Data value to be written

  @return SUCCESS if the data was successfully written
*/
wpt_status wpalReadRegister
(
   wpt_uint32                           address,
   wpt_uint32                          *data
);

/**
  @brief wpalReadRegister provides a mechansim for a client
         to read data from a hardware data register

  @param  address:  Physical memory address of the register
  @param  data:     Return location for value that is read

  @return SUCCESS if the data was successfully read
*/
wpt_status wpalWriteRegister
(
   wpt_uint32                           address,
   wpt_uint32                           data
);

/**
  @brief wpalReadDeviceMemory provides a mechansim for a client
         to read data from the hardware address space

  @param  address:  Start address of physical memory to be read
  @param  d_buffer: Virtual destination address to which the
                    data will be written
  @param  len:      Number of bytes of data to be read

  @return SUCCESS if the data was successfully read
*/
wpt_status wpalReadDeviceMemory
(
   wpt_uint32                            address,
   wpt_uint8                            *DestBuffer,
   wpt_uint32                            len
);

/**
  @brief wpalWriteDeviceMemory provides a mechansim for a client
         to write data into the hardware address space

  @param  address:  Start address of physical memory to be written
  @param  s_buffer: Virtual source address from which the data will
                    be read
  @param  len:      Number of bytes of data to be written

  @return SUCCESS if the data was successfully written
*/
wpt_status wpalWriteDeviceMemory
(
   wpt_uint32                            address,
   wpt_uint8                            *srcBuffer,
   wpt_uint32                            len
);

/**
  @brief wpalNotifySmsm provides a mechansim for a client to 
         notify SMSM to start DXE engine and/or condition of Tx
         ring buffer

  @param  clrSt:   bit(s) to be cleared on the MASK 
  @param  setSt:   bit(s) to be set on the MASK

  @return SUCCESS if the operation is successful
*/
wpt_status wpalNotifySmsm
(
   wpt_uint32                            clrSt,
   wpt_uint32                            setSt
);

/**
  @brief wpalActivateRxInterrupt activates wpalRxIsr

  @param  NONE

  @return NONE
*/
void wpalActivateRxInterrupt(void);

/**
  @brief wpalInactivateRxInterrupt inactivates wpalRxIsr

  @param  NONE

  @return NONE
*/
void wpalInactivateRxInterrupt(void);

#endif /* WLAN_QCT_PAL_DEVICE_H*/
