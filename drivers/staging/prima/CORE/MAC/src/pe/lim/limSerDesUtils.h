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
 *
 * This file limSerDesUtils.h contains the utility definitions
 * LIM uses while processing messages from upper layer software
 * modules
 * Author:        Chandra Modumudi
 * Date:          10/20/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_SERDES_UTILS_H
#define __LIM_SERDES_UTILS_H

#include "sirApi.h"
#include "aniSystemDefs.h"
#include "sirMacProtDef.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limPropExtsUtils.h"

tSirRetStatus   limStartBssReqSerDes(tpAniSirGlobal, tpSirSmeStartBssReq, tANI_U8 *);
tSirRetStatus   limStopBssReqSerDes(tpAniSirGlobal, tpSirSmeStopBssReq, tANI_U8 *);
tSirRetStatus   limJoinReqSerDes(tpAniSirGlobal, tpSirSmeJoinReq, tANI_U8 *);
void            limAssocIndSerDes(tpAniSirGlobal, tpLimMlmAssocInd, tANI_U8 *, tpPESession);
void            limReassocIndSerDes(tpAniSirGlobal, tpLimMlmReassocInd, tANI_U8 *, tpPESession psessionEntry);
tSirRetStatus   limAssocCnfSerDes(tpAniSirGlobal, tpSirSmeAssocCnf, tANI_U8 *);
tSirRetStatus   limDisassocCnfSerDes(tpAniSirGlobal, tpSirSmeDisassocCnf, tANI_U8 *);
tSirRetStatus   limSetContextReqSerDes(tpAniSirGlobal, tpSirSmeSetContextReq, tANI_U8 *);
tSirRetStatus   limDisassocReqSerDes(tpAniSirGlobal, tSirSmeDisassocReq *, tANI_U8 *);
tSirRetStatus   limDeauthReqSerDes(tpAniSirGlobal, tSirSmeDeauthReq *, tANI_U8 *);
void            limAuthIndSerDes(tpAniSirGlobal, tpLimMlmAuthInd, tANI_U8 *);
void            limStatSerDes(tpAniSirGlobal, tpAniStaStatStruct, tANI_U8 *);
void            limGetSessionInfo(tpAniSirGlobal pMac, tANI_U8 *, tANI_U8 *, tANI_U16 *);


void            limPackBkgndScanFailNotify(tpAniSirGlobal, tSirSmeStatusChangeCode, 
                                           tpSirBackgroundScanInfo, tSirSmeWmStatusChangeNtf *, tANI_U8);


tSirRetStatus limRemoveKeyReqSerDes(tpAniSirGlobal pMac, tpSirSmeRemoveKeyReq pRemoveKeyReq, tANI_U8 * pBuf);

tANI_BOOLEAN    limIsSmeGetAssocSTAsReqValid(tpAniSirGlobal pMac, tpSirSmeGetAssocSTAsReq pGetAssocSTAsReq, tANI_U8 *pBuf);
tSirRetStatus   limTkipCntrMeasReqSerDes(tpAniSirGlobal pMac, tpSirSmeTkipCntrMeasReq  ptkipCntrMeasReq, tANI_U8 *pBuf);

tSirRetStatus limUpdateAPWPSIEsReqSerDes(tpAniSirGlobal pMac, tpSirUpdateAPWPSIEsReq pUpdateAPWPSIEsReq, tANI_U8 *pBuf);
tSirRetStatus limUpdateAPWPARSNIEsReqSerDes(tpAniSirGlobal pMac, tpSirUpdateAPWPARSNIEsReq pUpdateAPWPARSNIEsReq, tANI_U8 *pBuf);


// Byte String <--> tANI_U16/tANI_U32 copy functions
static inline void limCopyU16(tANI_U8 *ptr, tANI_U16 u16Val)
{
#if ((defined(ANI_OS_TYPE_QNX) && defined(ANI_LITTLE_BYTE_ENDIAN)) ||   \
     (defined(ANI_OS_TYPE_ANDROID) && defined(ANI_LITTLE_BYTE_ENDIAN)))
    *ptr++ = (tANI_U8) (u16Val & 0xff);
    *ptr   = (tANI_U8) ((u16Val >> 8) & 0xff);
#else
#error "Unknown combination of OS Type and endianess"
#endif
}
        
static inline tANI_U16 limGetU16(tANI_U8 *ptr)
{
#if ((defined(ANI_OS_TYPE_QNX) && defined(ANI_LITTLE_BYTE_ENDIAN)) ||   \
     (defined(ANI_OS_TYPE_ANDROID) && defined(ANI_LITTLE_BYTE_ENDIAN)))
    return (((tANI_U16) (*(ptr+1) << 8)) |
            ((tANI_U16) (*ptr)));
#else
#error "Unknown combination of OS Type and endianess"
#endif
}

static inline void limCopyU32(tANI_U8 *ptr, tANI_U32 u32Val)
{
#if ((defined(ANI_OS_TYPE_QNX) && defined(ANI_LITTLE_BYTE_ENDIAN)) ||   \
     (defined(ANI_OS_TYPE_ANDROID) && defined(ANI_LITTLE_BYTE_ENDIAN)))
    *ptr++ = (tANI_U8) (u32Val & 0xff);
    *ptr++ = (tANI_U8) ((u32Val >> 8) & 0xff);
    *ptr++ = (tANI_U8) ((u32Val >> 16) & 0xff);
    *ptr   = (tANI_U8) ((u32Val >> 24) & 0xff);
#else
#error "Unknown combination of OS Type and endianess"
#endif
}

static inline tANI_U32 limGetU32(tANI_U8 *ptr)
{
#if ((defined(ANI_OS_TYPE_QNX) && defined(ANI_LITTLE_BYTE_ENDIAN)) ||   \
     (defined(ANI_OS_TYPE_ANDROID) && defined(ANI_LITTLE_BYTE_ENDIAN)))
    return ((*(ptr+3) << 24) |
            (*(ptr+2) << 16) |
            (*(ptr+1) << 8) |
            (*(ptr)));
#else
#error "Unknown combination of OS Type and endianess"
#endif
}

#endif /* __LIM_SERDES_UTILS_H */

