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
 * This file limIbssPeerMgmt.h contains prototypes for
 * the utility functions LIM uses to maintain peers in IBSS.
 * Author:        Chandra Modumudi
 * Date:          03/12/04
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "sirCommon.h"
#include "limUtils.h"

#define IBSS_STATIONS_USED_DURING_INIT 4  //(broadcast + self + p2p + softap)

void limIbssInit(tpAniSirGlobal);
void limIbssDelete(tpAniSirGlobal,tpPESession psessionEntry);
tSirRetStatus limIbssCoalesce(tpAniSirGlobal, tpSirMacMgmtHdr, tpSchBeaconStruct, tANI_U8*,tANI_U32, tANI_U16,tpPESession);
tSirRetStatus limIbssStaAdd(tpAniSirGlobal, void *,tpPESession);
tSirRetStatus limIbssAddStaRsp( tpAniSirGlobal, void *,tpPESession);
void limIbssDelBssRsp( tpAniSirGlobal, void *,tpPESession);
void limIbssDelBssRspWhenCoalescing(tpAniSirGlobal,  void *,tpPESession);
void limIbssAddBssRspWhenCoalescing(tpAniSirGlobal  pMac, void * msg, tpPESession pSessionEntry);
void limIbssDecideProtectionOnDelete(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpUpdateBeaconParams pBeaconParams,tpPESession pSessionEntry);
void limIbssHeartBeatHandle(tpAniSirGlobal pMac,tpPESession psessionEntry);
void limProcessIbssPeerInactivity(tpAniSirGlobal pMac, void *buf);
