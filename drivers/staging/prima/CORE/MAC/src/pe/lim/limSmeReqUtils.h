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
 * This file limSmeReqUtils.h contains the utility definitions
 * LIM uses while processing SME request messsages.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_SME_REQ_UTILS_H
#define __LIM_SME_REQ_UTILS_H

#include "sirApi.h"
#include "limTypes.h"


// LIM SME request messages related utility functions
tANI_U8 limIsSmeStartReqValid(tpAniSirGlobal, tANI_U32 *);
tANI_U8 limIsSmeStartBssReqValid(tpAniSirGlobal, tpSirSmeStartBssReq);
tANI_U8 limSetRSNieWPAiefromSmeStartBSSReqMessage(tpAniSirGlobal, 
                                          tpSirRSNie,
                                          tpPESession);
tANI_U8 limIsSmeScanReqValid(tpAniSirGlobal, tpSirSmeScanReq);
tANI_U8 limIsSmeJoinReqValid(tpAniSirGlobal, tpSirSmeJoinReq);
tANI_U8 limIsSmeAuthReqValid(tpSirSmeAuthReq);
tANI_U8 limIsSmeDisassocReqValid(tpAniSirGlobal, tpSirSmeDisassocReq, tpPESession);
tANI_U8 limIsSmeDeauthReqValid(tpAniSirGlobal, tpSirSmeDeauthReq, tpPESession);
tANI_U8 limIsSmeSetContextReqValid(tpAniSirGlobal, tpSirSmeSetContextReq);
tANI_U8 limIsSmeStopBssReqValid(tANI_U32 *);
tANI_U8*  limGetBssIdFromSmeJoinReqMsg(tANI_U8 *);
tANI_U8 limIsSmeDisassocCnfValid(tpAniSirGlobal, tpSirSmeDisassocCnf, tpPESession);

#endif /* __LIM_SME_REQ_UTILS_H */
