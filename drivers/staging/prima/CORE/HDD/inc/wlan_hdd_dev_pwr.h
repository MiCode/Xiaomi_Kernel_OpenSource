/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_DEV_PWR_H
#define __WLAN_HDD_DEV_PWR_H

#include <wlan_hdd_includes.h>
#include <wlan_hdd_power.h>
#include <vos_sched.h>
#include <vos_api.h>

/*----------------------------------------------------------------------------

   @brief Registration function.
        Register suspend, resume callback functions with platform driver. 

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Registration Success
        VOS_STATUS_E_FAILURE     Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddRegisterPmOps(hdd_context_t *pHddCtx);

/*----------------------------------------------------------------------------

   @brief De-registration function.
        Deregister the suspend, resume callback functions with platform driver

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       De-Registration Success
        VOS_STATUS_E_FAILURE     De-Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDeregisterPmOps(hdd_context_t *pHddCtx);

/*----------------------------------------------------------------------------

   @brief TM Level Change handler
          Received Tm Level changed notification

   @param dev : Device context
          changedTmLevel : Changed new TM level

   @return 

----------------------------------------------------------------------------*/
void hddDevTmLevelChangedHandler(struct device *dev, int changedTmLevel);

/*----------------------------------------------------------------------------

   @brief Register function
        Register Thermal Mitigation Level Changed handle callback function

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Registration Success
        VOS_STATUS_E_FAILURE     Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDevTmRegisterNotifyCallback(hdd_context_t *pHddCtx);

/*----------------------------------------------------------------------------

   @brief Un-Register function
        Un-Register Thermal Mitigation Level Changed handle callback function

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Un-Registration Success
        VOS_STATUS_E_FAILURE     Un-Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDevTmUnregisterNotifyCallback(hdd_context_t *pHddCtx);

#endif
