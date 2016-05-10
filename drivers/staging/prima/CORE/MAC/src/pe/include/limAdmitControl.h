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

/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * Author:      Dinesh Upadhyay
 * Date:        10/24/06
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __LIM_ADMIT_CONTROL_H__
#define __LIM_ADMIT_CONTROL_H__

#include "sirCommon.h"
#include "sirMacProtDef.h"

#include "aniGlobal.h"

tSirRetStatus
limTspecFindByAssocId(tpAniSirGlobal, tANI_U16, tSirMacTspecIE*, tpLimTspecInfo, tpLimTspecInfo *);

// Add TSPEC in lim local table
tSirRetStatus limTspecAdd(
    tpAniSirGlobal    pMac,
    tANI_U8               *pAddr,
    tANI_U16               assocId,
    tSirMacTspecIE   *pTspec,
    tANI_U32               interval,
    tpLimTspecInfo   *ppInfo);
    

// admit control interface
extern tSirRetStatus
limAdmitControlAddTS(
    tpAniSirGlobal          pMac,
    tANI_U8                     *pAddr,
    tSirAddtsReqInfo       *addts,
    tSirMacQosCapabilityStaIE *qos,
    tANI_U16                     assocId,
    tANI_U8                    alloc,
    tSirMacScheduleIE      *pSch,
    tANI_U8                   *pTspecIdx ,//index to the lim tspec table.
    tpPESession psessionEntry
    );

static inline tSirRetStatus
limAdmitControlAddSta(
    tpAniSirGlobal  pMac,
    tANI_U8             *staAddr,
    tANI_U8            alloc)
{ return eSIR_SUCCESS;}

extern tSirRetStatus
limAdmitControlDeleteSta(
    tpAniSirGlobal  pMac,
    tANI_U16             assocId);

extern tSirRetStatus
limAdmitControlDeleteTS(
    tpAniSirGlobal    pMac,
    tANI_U16               assocId,
    tSirMacTSInfo    *tsinfo,
    tANI_U8               *tsStatus,
    tANI_U8  *tspecIdx);

extern tSirRetStatus
limUpdateAdmitPolicy(
    tpAniSirGlobal    pMac);

tSirRetStatus limAdmitControlInit(tpAniSirGlobal pMac);

tSirRetStatus limSendHalMsgAddTs(tpAniSirGlobal pMac, tANI_U16 staIdx, tANI_U8 tspecIdx, tSirMacTspecIE tspecIE, tANI_U8 sessionId);
tSirRetStatus limSendHalMsgDelTs(tpAniSirGlobal pMac,
                                 tANI_U16 staIdx,
                                 tANI_U8 tspecIdx,
                                 tSirDeltsReqInfo delts,
                                 tANI_U8 sessionId,
                                 tANI_U8 *bssId);
void limProcessHalAddTsRsp(tpAniSirGlobal pMac, tpSirMsgQ limMsg);

#endif
