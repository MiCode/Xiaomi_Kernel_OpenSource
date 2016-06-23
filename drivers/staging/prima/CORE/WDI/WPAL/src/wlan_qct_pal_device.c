/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**=========================================================================
  
  @file  wlan_qct_pal_device.c
  
  @brief 
               
  This file implements the device specific HW access interface
  required by the WLAN Platform Abstraction Layer (WPAL)

  Copyright (c) 2011 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


  when        who  what, where, why
  ----------  ---  -----------------------------------------------------------
  2011-03-01  jtj  Initial version for Linux/Android with Wcnss

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#ifdef EXISTS_MSM_SMSM
#include <mach/msm_smsm.h>
#else
#include <soc/qcom/smsm.h>
#endif
#include "wlan_qct_pal_api.h"
#include "wlan_qct_pal_device.h"
#include "wlan_hdd_main.h"
#include "linux/wcnss_wlan.h"
#include <linux/ratelimit.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

typedef struct {
   struct resource *wcnss_memory;
   void __iomem    *mmio;
   int              tx_irq;
   wpalIsrType      tx_isr;
   void            *tx_context;
   int              rx_irq;
   wpalIsrType      rx_isr;
   void            *rx_context;
   int              rx_registered;
   int              tx_registered;
   u8               rx_isr_enabled;
   u64              *rx_disable_return;
   u64              *rx_enable_return;
   u8               rx_isr_enable_failure;
   u8               rx_isr_enable_partial_failure;
} wcnss_env;

static wcnss_env  gEnv;
static wcnss_env *gpEnv = NULL;

#define WPAL_READ_REGISTER_RATELIMIT_INTERVAL 20*HZ
#define WPAL_READ_REGISTER_RATELIMIT_BURST    1

static DEFINE_RATELIMIT_STATE(wpalReadRegister_rs, \
        WPAL_READ_REGISTER_RATELIMIT_INTERVAL,     \
        WPAL_READ_REGISTER_RATELIMIT_BURST);
/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/**
  @brief wpalTxIsr is the interrupt service routine which handles
         the DXE TX Complete interrupt
  
  wpalTxIsr is registered with the Operating System to handle the
  DXE TX Complete interrupt during system initialization.  When a DXE
  TX Complete interrupt occurs, it is dispatched to the handler which
  had previously been registered via wpalRegisterInterrupt.
  
  @param  irq:    Enumeration of the interrupt that occurred
  @param  dev_id: User-supplied data passed back via the ISR

  @see    wpalRegisterInterrupt

  @return IRQ_HANDLED since it is a dedicated interrupt
*/
static irqreturn_t wpalTxIsr
(
   int irq,
   void *dev_id
)
{
   if ((NULL != gpEnv) && (NULL != gpEnv->tx_isr)) {
      gpEnv->tx_isr(gpEnv->tx_context);
   }
   return IRQ_HANDLED;
}


/**
  @brief wpalRxIsr is the interrupt service routine which handles
         the DXE RX Available interrupt
  
  wpalRxIsr is registered with the Operating System to handle the
  DXE RX Available interrupt during system initalization.  When a DXE
  RX Available interrupt occurs, it is dispatched to the handler which
  had previously been registered via wpalRegisterInterrupt.
  
  @param  irq:    Enumeration of the interrupt that occurred
  @param  dev_id: User-supplied data passed back via the ISR

  @see    wpalRegisterInterrupt

  @return IRQ_HANDLED since it is a dedicated interrupt
*/
static irqreturn_t wpalRxIsr
(
   int irq,
   void *dev_id
)
{
   if ((NULL != gpEnv) && (NULL != gpEnv->rx_isr)) {
      gpEnv->rx_isr(gpEnv->rx_context);
   }
   return IRQ_HANDLED;
}

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/


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
   wpt_uint32      intType,
   wpalIsrType     callbackFunction,
   void            *usrCtxt
)
{
   if (NULL == gpEnv) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: invoked before subsystem initialized",
                 __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if (NULL == callbackFunction) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: invoked with NULL callback",
                 __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   switch (intType) {

   case DXE_INTERRUPT_TX_COMPLE:
      if (NULL != gpEnv->tx_isr) {
         /* TX complete handler already registered */
         WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: TX interrupt handler already registered",
                 __func__);
         /* fall though and accept the new values */
      }
      gpEnv->tx_isr = callbackFunction;
      gpEnv->tx_context = usrCtxt;
      break;

   case DXE_INTERRUPT_RX_READY:
      if (NULL != gpEnv->rx_isr) {
         /* RX complete handler already registered */
         WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: RX interrupt handler already registered",
                 __func__);
         /* fall though and accept the new values */
      }
      gpEnv->rx_isr = callbackFunction;
      gpEnv->rx_context = usrCtxt;
      break;

   default:
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: Unknown interrupt type [%u]",
                 __func__, intType);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   return eWLAN_PAL_STATUS_SUCCESS;
}

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
)
{
   if (NULL == gpEnv) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: invoked before subsystem initialized",
                 __func__);
      return;
   }

   switch (intType) {

   case DXE_INTERRUPT_TX_COMPLE:
      disable_irq_nosync(gpEnv->tx_irq);
      if (gpEnv->tx_registered)
      {
         free_irq(gpEnv->tx_irq, gpEnv);
         gpEnv->tx_registered = 0;
      }
      gpEnv->tx_isr = NULL;
      gpEnv->tx_context = NULL;
      break;

   case DXE_INTERRUPT_RX_READY:
      disable_irq_nosync(gpEnv->rx_irq);
      if (gpEnv->rx_registered)
      {
         free_irq(gpEnv->rx_irq, gpEnv);
         gpEnv->rx_registered = 0;
      }
      gpEnv->rx_isr = NULL;
      gpEnv->rx_context = NULL;
      break;

   default:
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: Unknown interrupt type [%u]",
                 __func__, intType);
      return;
   }

   return;
}

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
   wpt_uint32    intType
)
{
   int ret;
   
   switch (intType) 
   {
   case DXE_INTERRUPT_RX_READY:
      gpEnv->rx_enable_return = VOS_RETURN_ADDRESS;
      if (!gpEnv->rx_registered) 
      {
         gpEnv->rx_registered = 1;
         ret = request_irq(gpEnv->rx_irq, wpalRxIsr, IRQF_TRIGGER_HIGH,
                     "wcnss_wlan", gpEnv);
         if (ret) {
            WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                       "%s: RX IRQ request failure",
                       __func__);
           gpEnv->rx_isr_enable_failure = 1;
           break;
         }
      
        
         ret = enable_irq_wake(gpEnv->rx_irq);
         if (ret) {
            WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                       "%s: enable_irq_wake failed for RX IRQ",
                       __func__);
           gpEnv->rx_isr_enable_partial_failure = 1;
            /* not fatal -- keep on going */
         }
         gpEnv->rx_isr_enabled = 1;
      }
      else
      {
         enable_irq(gpEnv->rx_irq);
         gpEnv->rx_isr_enabled = 1;
      }
      break;
   case DXE_INTERRUPT_TX_COMPLE:
      if (!gpEnv->tx_registered) 
      {
         gpEnv->tx_registered = 1;
         ret = request_irq(gpEnv->tx_irq, wpalTxIsr, IRQF_TRIGGER_HIGH,
                           "wcnss_wlan", gpEnv);
         if (ret) {
            WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                       "%s: TX IRQ request failure",
                       __func__);
            break;
         }
   
   
         ret = enable_irq_wake(gpEnv->tx_irq);
         if (ret) {
            WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                       "%s: enable_irq_wake failed for TX IRQ",
                       __func__);
            /* not fatal -- keep on going */
         }
      }
      else
      {
         enable_irq(gpEnv->tx_irq);
      }
      break;
   default:
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                    "%s: unknown interrupt: %d",
                    __func__, (int)intType);
      break;
   }
   /* on the integrated platform there is no platform-specific
      interrupt control */
   return eWLAN_PAL_STATUS_SUCCESS;
}

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
   wpt_uint32    intType
)
{
   switch (intType) 
   {
   case DXE_INTERRUPT_RX_READY:
      gpEnv->rx_disable_return = VOS_RETURN_ADDRESS;
      disable_irq_nosync(gpEnv->rx_irq);
      gpEnv->rx_isr_enabled = 0;
      break;
   case DXE_INTERRUPT_TX_COMPLE:
      disable_irq_nosync(gpEnv->tx_irq);
      break;
   default:
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                    "%s: unknown interrupt: %d",
                    __func__, (int)intType);
      break;
   }

   /* on the integrated platform there is no platform-specific
      interrupt control */
   return eWLAN_PAL_STATUS_SUCCESS;
}

/**
  @brief wpalWriteRegister provides a mechansim for a client
         to write data into a hardware data register

  @param  address:  Physical memory address of the register
  @param  data:     Data value to be written

  @return SUCCESS if the data was successfully written
*/
wpt_status wpalWriteRegister
(
   wpt_uint32   address,
   wpt_uint32   data
)
{
   /* if SSR is in progress, and WCNSS is not out of reset (re-init
    * not invoked), then do not access WCNSS registers */
   if (NULL == gpEnv || wcnss_device_is_shutdown() ||
        (vos_is_logp_in_progress(VOS_MODULE_ID_WDI, NULL) &&
            !vos_is_reinit_in_progress(VOS_MODULE_ID_WDI, NULL))) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: invoked before subsystem initialized",
                 __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   address = (address | gpEnv->wcnss_memory->start);

   if ((address < gpEnv->wcnss_memory->start) ||
       (address > gpEnv->wcnss_memory->end)) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: Register address 0x%0x out of range 0x%0x - 0x%0x",
                 __func__, address,
                 (u32) gpEnv->wcnss_memory->start,
                 (u32) gpEnv->wcnss_memory->end);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if (0 != (address & 0x3)) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: Register address 0x%0x is not word aligned",
                 __func__, address);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   wmb();
   writel_relaxed(data, gpEnv->mmio + (address - gpEnv->wcnss_memory->start));

   return eWLAN_PAL_STATUS_SUCCESS;
}

/**
  @brief wpalReadRegister provides a mechansim for a client
         to read data from a hardware data register

  @param  address:  Physical memory address of the register
  @param  data:     Return location for value that is read

  @return SUCCESS if the data was successfully read
*/
wpt_status wpalReadRegister
(
   wpt_uint32   address,
   wpt_uint32  *data
)
{
   /* if SSR is in progress, and WCNSS is not out of reset (re-init
    * not invoked), then do not access WCNSS registers */
   if (NULL == gpEnv || wcnss_device_is_shutdown() ||
        (vos_is_logp_in_progress(VOS_MODULE_ID_WDI, NULL) &&
            !vos_is_reinit_in_progress(VOS_MODULE_ID_WDI, NULL))) {
       /* Ratelimit wpalReadRegister failure messages which
        * can flood serial console during improper system
        * initialization or wcnss_device in shutdown state.
        * wpalRegisterInterrupt() call to wpalReadRegister is
        * likely to cause flooding. */
       if (__ratelimit(&wpalReadRegister_rs)) {
           WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                   "%s: invoked before subsystem initialized",
                   __func__);
       }
       return eWLAN_PAL_STATUS_E_INVAL;
   }

   address = (address | gpEnv->wcnss_memory->start);

   if ((address < gpEnv->wcnss_memory->start) ||
       (address > gpEnv->wcnss_memory->end)) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: Register address 0x%0x out of range 0x%0x - 0x%0x",
                 __func__, address,
                 (u32) gpEnv->wcnss_memory->start,
                 (u32) gpEnv->wcnss_memory->end);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if (0 != (address & 0x3)) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: Register address 0x%0x is not word aligned",
                 __func__, address);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   *data = readl_relaxed(gpEnv->mmio + (address - gpEnv->wcnss_memory->start));
   rmb();

   return eWLAN_PAL_STATUS_SUCCESS;
}

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
  wpt_uint32 address,
  wpt_uint8* s_buffer,
  wpt_uint32 len
)
{
   /* if SSR is in progress, and WCNSS is not out of reset (re-init
    * not invoked), then do not access WCNSS registers */
   if (NULL == gpEnv || wcnss_device_is_shutdown() ||
        (vos_is_logp_in_progress(VOS_MODULE_ID_WDI, NULL) &&
            !vos_is_reinit_in_progress(VOS_MODULE_ID_WDI, NULL))) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: invoked before subsystem initialized",
                 __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   address = (address | gpEnv->wcnss_memory->start);

   if ((address < gpEnv->wcnss_memory->start) ||
       ((address + len) > gpEnv->wcnss_memory->end)) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: Memory address 0x%0x len %d out of range 0x%0x - 0x%0x",
                 __func__, address, len,
                 (u32) gpEnv->wcnss_memory->start,
                 (u32) gpEnv->wcnss_memory->end);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   vos_mem_copy(gpEnv->mmio + (address - gpEnv->wcnss_memory->start),
                                                            s_buffer, len);
   wmb();

   return eWLAN_PAL_STATUS_SUCCESS;
}

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
  wpt_uint32 address,
  wpt_uint8* d_buffer,
  wpt_uint32 len
)
{
   /* if SSR is in progress, and WCNSS is not out of reset (re-init
    * not invoked), then do not access WCNSS registers */
   if (NULL == gpEnv || wcnss_device_is_shutdown() ||
        (vos_is_logp_in_progress(VOS_MODULE_ID_WDI, NULL) &&
            !vos_is_reinit_in_progress(VOS_MODULE_ID_WDI, NULL))) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: invoked before subsystem initialized",
                 __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   address = (address | gpEnv->wcnss_memory->start);

   if ((address < gpEnv->wcnss_memory->start) ||
       ((address + len) > gpEnv->wcnss_memory->end)) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: Memory address 0x%0x len %d out of range 0x%0x - 0x%0x",
                 __func__, address, len,
                 (u32) gpEnv->wcnss_memory->start,
                 (u32) gpEnv->wcnss_memory->end);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   vos_mem_copy(d_buffer,
                   gpEnv->mmio + (address - gpEnv->wcnss_memory->start), len);
   rmb();

   return eWLAN_PAL_STATUS_SUCCESS;
}

/**
  @brief wpalDeviceInit provides a mechanism to initialize the DXE
         platform adaptation

  @param  deviceCB:  Implementation-specific device control block

  @see    wpalDeviceClose

  @return SUCCESS if the DXE abstraction was opened
*/
wpt_status wpalDeviceInit
(
   void * devHandle
)
{
   struct device *wcnss_device = (struct device *)devHandle;
   struct resource *wcnss_memory;
   int tx_irq;
   int rx_irq;

   if (NULL != gpEnv) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: invoked  after subsystem initialized",
                 __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if (NULL == wcnss_device) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: invalid device",
                 __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   wcnss_memory = wcnss_wlan_get_memory_map(wcnss_device);
   if (NULL == wcnss_memory) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: WCNSS memory map unavailable",
                 __func__);
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   tx_irq = wcnss_wlan_get_dxe_tx_irq(wcnss_device);
   if (0 > tx_irq) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: WCNSS TX IRQ unavailable",
                 __func__);
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   rx_irq = wcnss_wlan_get_dxe_rx_irq(wcnss_device);
   if (0 > rx_irq) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: WCNSS RX IRQ unavailable",
                 __func__);
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   gpEnv = &gEnv;
   if (NULL == gpEnv) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: memory allocation failure",
                 __func__);
      return eWLAN_PAL_STATUS_E_NOMEM;
   }

   memset(gpEnv, 0, sizeof(*gpEnv));

   gpEnv->wcnss_memory = wcnss_memory;
   gpEnv->tx_irq = tx_irq;
   gpEnv->rx_irq = rx_irq;

   /* note the we don't invoke request_mem_region().
      the memory described by wcnss_memory encompases the entire
      register space (including BT and FM) and we do not want
      exclusive access to that memory */

   gpEnv->mmio = ioremap(wcnss_memory->start, resource_size(wcnss_memory));

   if (NULL == gpEnv->mmio) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: memory remap failure",
                 __func__);
      goto err_ioremap;
   }

   gpEnv->tx_registered = 0;
   gpEnv->rx_registered = 0;

   /* successfully allocated environment, memory and IRQs */
   return eWLAN_PAL_STATUS_SUCCESS;

 err_ioremap:
   gpEnv = NULL;

   return eWLAN_PAL_STATUS_E_FAILURE;

}


/**
  @brief wpalDeviceClose provides a mechanism to deinitialize the DXE
         platform adaptation

  @param  deviceCB:  Implementation-specific device control block

  @see    wpalDeviceOpen

  @return SUCCESS if the DXE abstraction was closed
*/
wpt_status wpalDeviceClose
(
   void * deviceCB
 )
{
   if (NULL == gpEnv) {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: invoked before subsystem initialized",
                 __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if (gpEnv->rx_registered)
   {
      free_irq(gpEnv->rx_irq, gpEnv);
   }
   if (gpEnv->tx_registered)
   {
      free_irq(gpEnv->tx_irq, gpEnv);
   }
   iounmap(gpEnv->mmio);
   gpEnv = NULL;

   return eWLAN_PAL_STATUS_SUCCESS;
}

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
)
{
   int rc;
   rc = smsm_change_state(SMSM_APPS_STATE, clrSt, setSt);
   if(0 != rc) 
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: smsm_change_state failed",
                 __func__);
      return eWLAN_PAL_STATUS_E_FAILURE;
   }
   return eWLAN_PAL_STATUS_SUCCESS;
}

