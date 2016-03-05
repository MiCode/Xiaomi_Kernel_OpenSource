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

/******************************************************************************
*
* Name:  pmcLogDump.c
*
* Description: Implements the dump commands specific to PMC module
*
* Copyright 2008 (c) Qualcomm, Incorporated.
* All Rights Reserved.
* Qualcomm Confidential and Proprietary.
*
******************************************************************************/

#include "palTypes.h"
#include "aniGlobal.h"
#include "pmcApi.h"
#include "pmc.h"
#include "logDump.h"
#include "smsDebug.h"
#include "sme_Api.h"
#include "cfgApi.h"

#if defined(ANI_LOGDUMP)

void dump_pmc_callbackRoutine (void *callbackContext, eHalStatus status)
{
    tpAniSirGlobal pMac = (tpAniSirGlobal)callbackContext;
    pmcLog(pMac, LOGW, "*********Received callback from PMC with status = %d\n*********",status);
}

#ifdef WLAN_WAKEUP_EVENTS
void dump_pmc_callbackRoutine2 (void *callbackContext, tpSirWakeReasonInd pWakeReasonInd)
{
    tpAniSirGlobal pMac = (tpAniSirGlobal)callbackContext;
    pmcLog(pMac, LOGW, "*********Received callback from PMC with reason = %d\n*********",pWakeReasonInd->ulReason);
}
#endif // WLAN_WAKEUP_EVENTS

void dump_pmc_deviceUpdateRoutine (void *callbackContext, tPmcState pmcState)
{
    tpAniSirGlobal pMac = (tpAniSirGlobal)callbackContext;
    pmcLog(pMac, LOGW, "*********Received msg from PMC: Device is in %s state\n*********", pmcGetPmcStateStr(pmcState));
}

static char *
dump_pmc_state( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    char *ptr = p;

    (void) arg1; (void) arg2; (void) arg3; (void) arg4;

    p += log_sprintf( pMac,p, "********  PMC State & Configuration ******** \n");
    p += log_sprintf( pMac,p, " PMC: IMPS Enabled? %d\n", pMac->pmc.impsEnabled);
    p += log_sprintf( pMac,p, " PMC: Auto BMPS Timer Enabled? %d\n", pMac->pmc.autoBmpsEntryEnabled);
    p += log_sprintf( pMac,p, " PMC: BMPS Enabled? %d\n", pMac->pmc.bmpsEnabled);
    p += log_sprintf( pMac,p, " PMC: UAPSD Enabled? %d\n", pMac->pmc.uapsdEnabled);
    p += log_sprintf( pMac,p, " PMC: WoWL Enabled? %d\n", pMac->pmc.wowlEnabled);
    p += log_sprintf( pMac,p, " PMC: Standby Enabled? %d\n", pMac->pmc.standbyEnabled);
    p += log_sprintf( pMac,p, " PMC: Auto BMPS timer period (ms): %d\n", pMac->pmc.bmpsConfig.trafficMeasurePeriod);
    p += log_sprintf( pMac,p, " PMC: BMPS Listen Interval (Beacon intervals): %d\n", pMac->pmc.bmpsConfig.bmpsPeriod);
    p += log_sprintf( pMac,p, " PMC: Device State = %s\n", pmcGetPmcStateStr(pMac->pmc.pmcState));
    p += log_sprintf( pMac,p, " PMC: RequestFullPowerPending = %d\n", pMac->pmc.requestFullPowerPending);
    p += log_sprintf( pMac,p, " PMC: UapsdSessionRequired = %d\n", pMac->pmc.uapsdSessionRequired);
    p += log_sprintf( pMac,p, " PMC: wowlModeRequired = %d\n\n", pMac->pmc.wowlModeRequired);

    pmcLog(pMac, LOGW, "\n%s", ptr);

    return p;
}

static char *
dump_pmc_enable_imps( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)pmcEnablePowerSave(pMac, ePMC_IDLE_MODE_POWER_SAVE);
    return p;
}

static char *
dump_pmc_disable_imps( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)pmcDisablePowerSave(pMac, ePMC_IDLE_MODE_POWER_SAVE);
    return p;
}

static char *
dump_pmc_request_imps( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
    pMac->pmc.impsEnabled = TRUE;
    (void)pmcRequestImps(pMac, arg1, dump_pmc_callbackRoutine, pMac);
    return p;
}

static char *
dump_pmc_start_auto_bmps_timer( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    pMac->pmc.bmpsEnabled = TRUE;
    (void)pmcStartAutoBmpsTimer(pMac);
    return p;
}

static char *
dump_pmc_stop_auto_bmps_timer( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)pmcStopAutoBmpsTimer(pMac);
    return p;
}

static char *
dump_pmc_enable_bmps( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)pmcEnablePowerSave(pMac, ePMC_BEACON_MODE_POWER_SAVE);
    return p;
}

static char *
dump_pmc_disable_bmps( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)pmcDisablePowerSave(pMac, ePMC_BEACON_MODE_POWER_SAVE);
    return p;
}

static char *
dump_pmc_request_bmps( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    pMac->pmc.bmpsEnabled = TRUE;
    (void)sme_RequestBmps(pMac, dump_pmc_callbackRoutine, pMac);
    return p;
}

static char *
dump_pmc_enable_uapsd( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)pmcEnablePowerSave(pMac, ePMC_UAPSD_MODE_POWER_SAVE);
    return p;
}

static char *
dump_pmc_disable_uapsd( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)pmcDisablePowerSave(pMac, ePMC_UAPSD_MODE_POWER_SAVE);
    return p;
}

static char *
dump_pmc_start_uapsd( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    pMac->pmc.bmpsEnabled = TRUE;
    pMac->pmc.uapsdEnabled = TRUE;
    (void)pmcStartUapsd(pMac, dump_pmc_callbackRoutine, pMac);
    return p;
}

static char *
dump_pmc_stop_uapsd( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)pmcStopUapsd(pMac);
    return p;
}

static char *
dump_pmc_request_standby( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    pMac->pmc.standbyEnabled = TRUE;
    (void)pmcRequestStandby(pMac, dump_pmc_callbackRoutine, pMac);
    return p;
}

static char *
dump_pmc_request_full_power( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)sme_RequestFullPower(pMac, dump_pmc_callbackRoutine, pMac, eSME_FULL_PWR_NEEDED_BY_HDD);
    return p;
}

static char *
dump_pmc_enter_wowl( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tSirSmeWowlEnterParams wowlEnterParams;
    tSirRetStatus status;
    tANI_U32 length;
    tANI_U8  sessionId = 0;

    (void) arg4;

    palZeroMemory(pMac->hHdd, &wowlEnterParams, sizeof(tSirSmeWowlEnterParams));

    if (arg1 == 0 && arg2 == 0)
    {
        pmcLog(pMac, LOGE,
               "Requesting WoWL but neither magic pkt and ptrn byte matching is being enabled\n");
        return p;
    }
    if(arg1 == 1)
    {
        wowlEnterParams.ucMagicPktEnable = 1;
        /* magic packet */
        length = SIR_MAC_ADDR_LENGTH;
        status = wlan_cfgGetStr(pMac, WNI_CFG_STA_ID, (tANI_U8 *)wowlEnterParams.magicPtrn, &length); 
        if (eSIR_SUCCESS != status)
        {
            pmcLog(pMac, LOGE,
                   "Reading of WNI_CFG_STA_ID from CFG failed. Using hardcoded STA MAC Addr\n");
            wowlEnterParams.magicPtrn[0] = 0x00;
            wowlEnterParams.magicPtrn[1] = 0x0a;
            wowlEnterParams.magicPtrn[2] = 0xf5;
            wowlEnterParams.magicPtrn[3] = 0x04;
            wowlEnterParams.magicPtrn[4] = 0x05;
            wowlEnterParams.magicPtrn[5] = 0x06;
        }
    }
    if(arg2 == 1)
    {
      wowlEnterParams.ucPatternFilteringEnable = 1;
    }

    if(arg3 == CSR_ROAM_SESSION_MAX )
    {
        pmcLog(pMac, LOGE, "Enter valid sessionId\n");
        return p;
    }
    pMac->pmc.bmpsEnabled = TRUE;
    pMac->pmc.wowlEnabled = TRUE;

    sessionId = (tANI_U8 ) arg3;
#ifdef WLAN_WAKEUP_EVENTS
    (void)sme_EnterWowl(pMac, dump_pmc_callbackRoutine, pMac, dump_pmc_callbackRoutine2, pMac, 
                        &wowlEnterParams, sessionId);
#else // WLAN_WAKEUP_EVENTS
    (void)sme_EnterWowl(pMac, dump_pmc_callbackRoutine, pMac, &wowlEnterParams, sessionId);
#endif // WLAN_WAKEUP_EVENTS
    return p;
}

static char *
dump_pmc_exit_wowl( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    (void)sme_ExitWowl(pMac);
    return p;
}

static char *
dump_pmc_remove_ptrn( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tSirWowlDelBcastPtrn delPattern;
    tANI_U8  sessionId = 0;
    (void) arg3; (void) arg4;
 
    palZeroMemory(pMac->hHdd, &delPattern, sizeof(tSirWowlDelBcastPtrn));

    if((arg1 <= 7) || (arg2 == CSR_ROAM_SESSION_MAX))
    {
        delPattern.ucPatternId = (tANI_U8)arg1;
    }
    else
    {
        pmcLog(pMac, LOGE, "dump_pmc_remove_ptrn: Invalid pattern Id %d\n",arg1);
        return p;
    }

    sessionId = (tANI_U8 ) arg2;
    (void)pmcWowlDelBcastPattern(pMac, &delPattern, sessionId);
    return p;
}

static char *
dump_pmc_test_uapsd( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tSirSmeRsp smeRsp;
    smeRsp.statusCode = eSIR_SME_SUCCESS;

    (void) arg1; (void) arg2; (void) arg3; (void) arg4;

    pMac->pmc.uapsdEnabled = TRUE;
    pMac->pmc.pmcState = BMPS;

    pmcRegisterDeviceStateUpdateInd(pMac, dump_pmc_deviceUpdateRoutine, pMac);

    pmcStartUapsd(pMac, dump_pmc_callbackRoutine, pMac);
    smeRsp.messageType = eWNI_PMC_ENTER_UAPSD_RSP;
    pmcMessageProcessor(pMac, &smeRsp);
    pmcStopUapsd(pMac);
    smeRsp.messageType = eWNI_PMC_EXIT_UAPSD_RSP;
    pmcMessageProcessor(pMac, &smeRsp);
    pmcDeregisterDeviceStateUpdateInd(pMac, dump_pmc_deviceUpdateRoutine);
    return p;
}

static char *
dump_pmc_test_Wowl( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tSirSmeRsp smeRsp;
    tSirWowlAddBcastPtrn addPattern;
    tSirWowlDelBcastPtrn delPattern;
    tSirSmeWowlEnterParams wowlEnterParams;
    tANI_U8            sessionId = 0;

    smeRsp.statusCode = eSIR_SME_SUCCESS;
    palZeroMemory(pMac->hHdd, &addPattern, sizeof(tSirWowlAddBcastPtrn));
    palZeroMemory(pMac->hHdd, &delPattern, sizeof(tSirWowlDelBcastPtrn));
    palZeroMemory(pMac->hHdd, &wowlEnterParams, sizeof(tSirSmeWowlEnterParams));

    (void) arg2; (void) arg3; (void) arg4;

    if(arg1 == CSR_ROAM_SESSION_MAX)
    {
        pmcLog(pMac, LOGE, "dump_pmc_test_Wowl: Invalid sessionId\n");
        return p;
    }

    sessionId = (tANI_U8 ) arg1;
    //Add pattern
    sme_WowlAddBcastPattern(pMac, &addPattern, sessionId);

    //Delete pattern
    sme_WowlDelBcastPattern(pMac, &delPattern, sessionId);

    //Force the device into BMPS
    pMac->pmc.pmcState = BMPS;

    //Enter Wowl
#ifdef WLAN_WAKEUP_EVENTS
    sme_EnterWowl(pMac, dump_pmc_callbackRoutine, pMac, dump_pmc_callbackRoutine2, pMac, 
                   &wowlEnterParams, sessionId);
#else // WLAN_WAKEUP_EVENTS
    sme_EnterWowl(pMac, dump_pmc_callbackRoutine, pMac, &wowlEnterParams, sessionId);
#endif // WLAN_WAKEUP_EVENTS
    smeRsp.messageType = eWNI_PMC_ENTER_WOWL_RSP;
    pmcMessageProcessor(pMac, &smeRsp);

    //Exit Wowl
    sme_ExitWowl(pMac);
    smeRsp.messageType = eWNI_PMC_EXIT_WOWL_RSP;
    pmcMessageProcessor(pMac, &smeRsp);
    return p;
}

static tDumpFuncEntry pmcMenuDumpTable[] = {
    {0,     "PMC (900-925)",           NULL},
    // General
    {900,   "PMC: Dump State + config", dump_pmc_state},
    // IMPS Related
    {901,   "PMC: Enable IMPS",         dump_pmc_enable_imps},
    {902,   "PMC: Disable IMPS",        dump_pmc_disable_imps},
    {903,   "PMC: Request IMPS: Syntax: dump 903 <imps_period_ms>", dump_pmc_request_imps},
    // BMPS Related
    {904,   "PMC: Start Auto BMPS Timer",  dump_pmc_start_auto_bmps_timer},
    {905,   "PMC: Stop Auto BMPS Timer", dump_pmc_stop_auto_bmps_timer},
    {906,   "PMC: Request BMPS",        dump_pmc_request_bmps},
    // UAPSD Related
    {907,   "PMC: Enable UAPSD",        dump_pmc_enable_uapsd},
    {908,   "PMC: Disable UAPSD",       dump_pmc_disable_uapsd},
    {909,   "PMC: Start UAPSD",         dump_pmc_start_uapsd},
    {910,   "PMC: Stop UAPSD",          dump_pmc_stop_uapsd},
    // Standby Related
    {911,   "PMC: Request Standby",     dump_pmc_request_standby},
    // Full Power Related
    {912,   "PMC: Request Full Power",  dump_pmc_request_full_power},
    //Unit Test Related
    {913,   "PMC: Test UAPSD",          dump_pmc_test_uapsd},
    {914,   "PMC: Test WOWL : Syntax :dump 914 <sessionId>",           dump_pmc_test_Wowl},
    // WoWL Related
    {915,   "PMC: Enter WoWL: Syntax: dump 915 <enable_magic_pkt> <enable_ptrn_match> <sessionId>",  dump_pmc_enter_wowl},
    {916,   "PMC: Exit WoWL",  dump_pmc_exit_wowl},
    {917,   "PMC: Remove a pattern: Syntax: dump 917 <pattern_id(0-7) <sessionId>>",  dump_pmc_remove_ptrn},
    {918,   "PMC: Enable BMPS",         dump_pmc_enable_bmps},
    {919,   "PMC: Disable BMPS",        dump_pmc_disable_bmps}
};

void pmcDumpInit(tHalHandle hHal)
{
    logDumpRegisterTable( (tpAniSirGlobal)hHal, &pmcMenuDumpTable[0],
                          sizeof(pmcMenuDumpTable)/sizeof(pmcMenuDumpTable[0]) );
}

#endif //#if defined(ANI_LOGDUMP)
