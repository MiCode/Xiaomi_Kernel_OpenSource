/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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

/*
 * logApi.cc - Handles log messages for all the modules.
 * Author:        Kevin Nguyen
 * Date:          02/27/02
 * History:-
 * 02/11/02       Created.
 * 03/12/02       Rearrange logDebug parameter list and add more params.
 * --------------------------------------------------------------------
 *
 */

#include <sirCommon.h>
#include <sirDebug.h>
#include <utilsApi.h>
#include <wlan_qct_wda.h>

#include <stdarg.h>
#include "utilsGlobal.h"
#include "macInitApi.h"
#include "palApi.h"

#include "vos_trace.h"

#ifdef ANI_OS_TYPE_ANDROID
#include <linux/kernel.h>
#endif


// ---------------------------------------------------------------------
/**
 * logInit()
 *
 * FUNCTION:
 * This function is called to prepare the logging utility.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param tpAniSirGlobal Sirius software parameter strucutre pointer
 * @return None
 */
tSirRetStatus
logInit(tpAniSirGlobal pMac)
{
    tANI_U32    i;

    // Add code to initialize debug level from CFG module
    // For now, enable all logging
    for (i = 0; i < LOG_ENTRY_NUM; i++)
    {
#ifdef SIR_DEBUG
        pMac->utils.gLogEvtLevel[i] = pMac->utils.gLogDbgLevel[i] = LOG1;
#else
        pMac->utils.gLogEvtLevel[i] = pMac->utils.gLogDbgLevel[i] = LOGW;
#endif
    }
    return eSIR_SUCCESS;

} /*** logInit() ***/

void
logDeinit(tpAniSirGlobal pMac)
{
    return;
}

/**
 * logDbg()
 *
 *FUNCTION:
 * This function is called to log a debug message.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * None.
 *
 *NOTE:
 *
 * @param tpAniSirGlobal Sirius software parameter strucutre pointer
 * @param ModId        8-bit modID
 * @param debugLevel   debugging level for this message
 * @param pStr         string parameter pointer
 * @return None
 */


void logDbg(tpAniSirGlobal pMac, tANI_U8 modId, tANI_U32 debugLevel, const char *pStr,...)
{
#ifdef WLAN_DEBUG
    if ( debugLevel > pMac->utils.gLogDbgLevel[LOG_INDEX_FOR_MODULE( modId )] )
        return;
    else
    {
        va_list marker;

        va_start( marker, pStr );     /* Initialize variable arguments. */

        logDebug(pMac, modId, debugLevel, pStr, marker);

        va_end( marker );              /* Reset variable arguments.      */
    }
#endif
}

VOS_TRACE_LEVEL getVosDebugLevel(tANI_U32 debugLevel)
{
    switch(debugLevel)
    {
        case LOGP:
            return VOS_TRACE_LEVEL_FATAL;
        case LOGE:
            return VOS_TRACE_LEVEL_ERROR;
        case LOGW:
            return VOS_TRACE_LEVEL_WARN;
        case LOG1:
            return VOS_TRACE_LEVEL_INFO;
        case LOG2:
            return VOS_TRACE_LEVEL_INFO_HIGH;
        case LOG3:
            return VOS_TRACE_LEVEL_INFO_MED;
        case LOG4:
            return VOS_TRACE_LEVEL_INFO_LOW;
        default:
            return VOS_TRACE_LEVEL_INFO_LOW;
    }
}

static inline VOS_MODULE_ID getVosModuleId(tANI_U8 modId)
{
    switch(modId)
    {
        case SIR_HAL_MODULE_ID:
        case SIR_HAL_EXT_MODULE_ID:
        case SIR_PHY_MODULE_ID:
            return VOS_MODULE_ID_WDA;
        case SIR_PMM_MODULE_ID:
            return VOS_MODULE_ID_PMC;

        case SIR_LIM_MODULE_ID:
        case SIR_SCH_MODULE_ID:
        case SIR_CFG_MODULE_ID:
        case SIR_MNT_MODULE_ID:
        case SIR_DPH_MODULE_ID:
        case SIR_DBG_MODULE_ID:
            return VOS_MODULE_ID_PE;

        case SIR_SYS_MODULE_ID:
            return VOS_MODULE_ID_SYS;

        case SIR_SMS_MODULE_ID:
            return VOS_MODULE_ID_SME;

        default:
            return VOS_MODULE_ID_SYS;
    }
}

#define LOG_SIZE 256
void logDebug(tpAniSirGlobal pMac, tANI_U8 modId, tANI_U32 debugLevel, const char *pStr, va_list marker)
{
    VOS_TRACE_LEVEL  vosDebugLevel;
    VOS_MODULE_ID    vosModuleId;
    char             logBuffer[LOG_SIZE];

    vosDebugLevel = getVosDebugLevel(debugLevel);
    vosModuleId = getVosModuleId(modId);

    vsnprintf(logBuffer, LOG_SIZE - 1, pStr, marker);
    VOS_TRACE(vosModuleId, vosDebugLevel, "%s", logBuffer);

    // The caller must check loglevel
    VOS_ASSERT( ( debugLevel <= pMac->utils.gLogDbgLevel[LOG_INDEX_FOR_MODULE( modId )] ) && ( LOGP != debugLevel ) );
} /*** end logDebug() ***/
