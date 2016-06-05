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
 * This file utilsParser.h contains the utility function protos
 * used internally by the parser
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __UTILS_PARSE_H__
#define __UTILS_PARSE_H__

#include <stdarg.h>
#include "sirApi.h"
#include "dot11f.h"
#include "utilsApi.h"

void          ConvertSSID           (tpAniSirGlobal, tSirMacSSid*,               tDot11fIESSID*);
void          ConvertSuppRates      (tpAniSirGlobal, tSirMacRateSet*,            tDot11fIESuppRates*);
void          ConvertFHParams       (tpAniSirGlobal, tSirMacFHParamSet*,         tDot11fIEFHParamSet*);
void          ConvertExtSuppRates   (tpAniSirGlobal, tSirMacRateSet*,            tDot11fIEExtSuppRates*);
void          ConvertQOSCaps        (tpAniSirGlobal, tSirMacQosCapabilityIE*,    tDot11fIEQOSCapsAp*);
void          ConvertQOSCapsStation (tpAniSirGlobal, tSirMacQosCapabilityStaIE*, tDot11fIEQOSCapsStation*);
tSirRetStatus ConvertWPA            (tpAniSirGlobal, tSirMacWpaInfo*,            tDot11fIEWPA*);
tSirRetStatus ConvertWPAOpaque      (tpAniSirGlobal, tSirMacWpaInfo*,            tDot11fIEWPAOpaque*);
tSirRetStatus ConvertRSN            (tpAniSirGlobal, tSirMacRsnInfo*,            tDot11fIERSN*);
tSirRetStatus ConvertRSNOpaque      (tpAniSirGlobal, tSirMacRsnInfo*,            tDot11fIERSNOpaque*);
void          ConvertPowerCaps      (tpAniSirGlobal, tSirMacPowerCapabilityIE*,  tDot11fIEPowerCaps*);
void          ConvertSuppChannels   (tpAniSirGlobal, tSirMacSupportedChannelIE*, tDot11fIESuppChannels*);
void          ConvertCFParams       (tpAniSirGlobal, tSirMacCfParamSet*,         tDot11fIECFParams*);
void          ConvertTIM            (tpAniSirGlobal, tSirMacTim*,                tDot11fIETIM*);
void          ConvertCountry        (tpAniSirGlobal, tSirCountryInformation*,    tDot11fIECountry*);
void          ConvertWMMParams      (tpAniSirGlobal, tSirMacEdcaParamSetIE*,     tDot11fIEWMMParams*);
void          ConvertERPInfo        (tpAniSirGlobal, tSirMacErpInfo*,            tDot11fIEERPInfo*);
void          ConvertEDCAParam      (tpAniSirGlobal, tSirMacEdcaParamSetIE*,     tDot11fIEEDCAParamSet*);
void          ConvertTSPEC          (tpAniSirGlobal, tSirMacTspecIE*,            tDot11fIETSPEC*);
tSirRetStatus ConvertTCLAS          (tpAniSirGlobal, tSirTclasInfo*,             tDot11fIETCLAS*);
void          ConvertWMMTSPEC       (tpAniSirGlobal, tSirMacTspecIE*,            tDot11fIEWMMTSPEC*);
tSirRetStatus ConvertWMMTCLAS       (tpAniSirGlobal, tSirTclasInfo*,             tDot11fIEWMMTCLAS*);
void          ConvertTSDelay        (tpAniSirGlobal, tSirMacTsDelayIE*,          tDot11fIETSDelay*);
void          ConvertSchedule       (tpAniSirGlobal, tSirMacScheduleIE*,         tDot11fIESchedule*);
void          ConvertWMMSchedule    (tpAniSirGlobal, tSirMacScheduleIE*,         tDot11fIEWMMSchedule*);
tSirRetStatus ConvertWscOpaque      (tpAniSirGlobal, tSirAddie*,                 tDot11fIEWscIEOpaque*);
tSirRetStatus ConvertP2POpaque      (tpAniSirGlobal, tSirAddie*,                 tDot11fIEP2PIEOpaque*);
#ifdef WLAN_FEATURE_WFD
tSirRetStatus ConvertWFDOpaque      (tpAniSirGlobal, tSirAddie*,                 tDot11fIEWFDIEOpaque*);
#endif
void          ConvertQosMapsetFrame(tpAniSirGlobal, tSirQosMapSet*,     tDot11fIEQosMapSet*);


#endif
