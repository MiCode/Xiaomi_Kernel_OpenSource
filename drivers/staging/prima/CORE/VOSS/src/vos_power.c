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

/*!
  @file
  vos_power.c

  @brief
  This is the interface to VOSS power APIs using for power management 
  of the WLAN Libra module from the MSM PMIC. These implementation of 
  these APIs is very target dependent, also these APIs should only be
  used when the WLAN Libra module is powered from the MSM PMIC and not
  from an external independent power source

*/

/*===========================================================================

  Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved

  Qualcomm Proprietary

  Export of this technology or software is regulated by the U.S. Government.
  Diversion contrary to U.S. law prohibited.

  All ideas, data and information contained in or disclosed by
  this document are confidential and proprietary information of
  QUALCOMM Incorporated and all rights therein are expressly reserved.
  By accepting this material the recipient agrees that this material
  and the information contained therein are held in confidence and in
  trust and will not be used, copied, reproduced in whole or in part,
  nor its contents revealed in any manner to others without the express
  written permission of QUALCOMM Incorporated.

===========================================================================*/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

$Header: $

when       who     what, where, why
--------   ---     ----------------------------------------------------------

===========================================================================*/

/*===========================================================================

                           INCLUDE FILES

===========================================================================*/
#include <vos_power.h>

#ifdef MSM_PLATFORM
#include <mach/mpp.h>
#include <mach/vreg.h>
#include <linux/err.h>
#include <linux/delay.h>
#endif //MSM_PLATFORM

#include <vos_sched.h>

//To work around issues of fail to turn WIFI back on after turning it off
#define VOS_PWR_WIFI_ON_OFF_HACK
#ifdef VOS_PWR_WIFI_ON_OFF_HACK
#define VOS_PWR_SLEEP(ms) msleep(ms)
#else
#define VOS_PWR_SLEEP(ms)
#endif

/*===========================================================================

                        DEFINITIONS AND TYPES

===========================================================================*/

#define CHIP_POWER_ON         1
#define CHIP_POWER_OFF        0

// SDIO Config Cycle Clock Frequency
#define WLAN_LOW_SD_CONFIG_CLOCK_FREQ 400000

/*===========================================================================

                    FUNCTION PROTOTYPES

===========================================================================*/

/**
  @brief vos_chipPowerUp() - This API will power up the Libra chip

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  The Libra SDIO core will have been initialized if the operation completes
  successfully

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipPowerUp
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipPowerDown() - This API will power down the Libra chip

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipPowerDown
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipReset() - This API will reset the Libra chip

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  A hard reset will involve a powerDown followed by a PowerUp; a soft reset
  can potentially be accomplished by writing to some device registers

  The Libra SDIO core will have been initialized if the operation completes
  successfully

  @param status [out] : whether this operation will complete sync or async
  @param soft [in] : VOS_TRUE if a soft reset is desired 
                     VOS_FALSE for a hard reset i.e. powerDown followed by powerUp
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_NOSUPPORT - soft reset asked for but not supported
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipReset
(
  vos_call_status_type* status,
  v_BOOL_t              soft,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data,
  vos_chip_reset_reason_type   reason
)
{
   VOS_STATUS vstatus;
   vstatus = vos_watchdog_chip_reset(reason);
   return vstatus;
}


/**
  @brief vos_chipVoteOnPASupply() - This API will power up the PA supply

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOnPASupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}


/**
  @brief vos_chipVoteOffPASupply() - This API will vote to turn off the 
  PA supply. Even if we succeed in voting, there is a chance PA supply will not 
  be turned off. This will be treated the same as a failure.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOffPASupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}


/**
  @brief vos_chipAssertDeepSleep() - This API will assert the deep 
  sleep signal to Libra

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipAssertDeepSleep
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}


/**
  @brief vos_chipDeAssertDeepSleep() - This API will de-assert the deep sleep
  signal to Libra

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipDeAssertDeepSleep
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipExitDeepSleepVREGHandler() - This API will initialize the required VREG
  after exit from deep sleep.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipExitDeepSleepVREGHandler
(
   vos_call_status_type* status,
   vos_power_cb_type     callback,
   v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOnRFSupply() - This API will power up the RF supply

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOnRFSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOffRFSupply() - This API will vote to turn off the 
  RF supply. Even if we succeed in voting, there is a chance RF supply will not 
  be turned off as RF rails could be shared with other modules (outside WLAN)

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOffRFSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOnBBAnalogSupply() - This API will power up the I/P voltage
  used by Base band Analog.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOnBBAnalogSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOffBBAnalogSupply() - This API will vote off the BB Analog supply.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOffBBAnalogSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOnXOBuffer() - This API will vote to turn on the XO buffer from
  PMIC. This API will be used when Libra uses the TCXO from PMIC on the MSM

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOnXOBuffer
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOffXOBuffer() - This API will vote off PMIC XO buffer.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOffXOBuffer
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteXOCore - This API will vote PMIC XO Core.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with
  @param force_enable [in] : Force enable XO CORE or not

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteXOCore
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data,
  v_BOOL_t              force_enable
)
{

   return VOS_STATUS_SUCCESS;
}


/**
  @brief vos_chipVoteFreqFor1p3VSupply() - This API will vote for frequency for 1.3V RF supply.
  
  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  EVM issue is observed with 1.6Mhz freq for 1.3V supply in wlan standalone case.
  During concurrent operation (e.g. WLAN and WCDMA) this issue is not observed. 
  To workaround, wlan will vote for 3.2Mhz during startup and will vote for 1.6Mhz
  during exit.
   
  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with
  @param freq [in]     :  Frequency for 1.3V Supply for which WLAN driver needs to vote for.
  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteFreqFor1p3VSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data,
  v_U32_t               freq
)
{
    return VOS_STATUS_SUCCESS;
}
