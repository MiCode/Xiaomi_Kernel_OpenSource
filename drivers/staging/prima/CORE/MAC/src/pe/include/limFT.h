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

#if defined WLAN_FEATURE_VOWIFI_11R
/**=========================================================================
  
   Macros and Function prototypes FT and 802.11R purposes
  
  ========================================================================*/

#ifndef __LIMFT_H__
#define __LIMFT_H__


#include <palTypes.h>
#include <limGlobal.h>
#include <aniGlobal.h>
#include <limDebug.h>
#include <limSerDesUtils.h>


/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
extern void limFTOpen(tpAniSirGlobal pMac);
extern void limFTCleanup(tpAniSirGlobal pMac);
extern void limFTInit(tpAniSirGlobal pMac);
extern int  limProcessFTPreAuthReq(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
extern void limPerformFTPreAuth(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data, 
                tpPESession psessionEntry);
void        limPerformPostFTPreAuth(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data, 
                tpPESession psessionEntry);
void        limFTResumeLinkCb(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data);
void        limPostFTPreAuthRsp(tpAniSirGlobal pMac, tSirRetStatus status,
                tANI_U8 *auth_rsp, tANI_U16  auth_rsp_length,
                tpPESession psessionEntry);
void        limHandleFTPreAuthRsp(tpAniSirGlobal pMac, tSirRetStatus status,
                tANI_U8 *auth_rsp, tANI_U16  auth_rsp_len,
                tpPESession psessionEntry);
void        limProcessMlmFTReassocReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf,
                tpPESession psessionEntry);
void        limProcessFTPreauthRspTimeout(tpAniSirGlobal pMac);

tANI_BOOLEAN   limProcessFTUpdateKey(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf );
tSirRetStatus  limProcessFTAggrQosReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf );
void        limProcessFTAggrQoSRsp(tpAniSirGlobal pMac, tpSirMsgQ limMsg);

#endif /* __LIMFT_H__ */ 

#endif /* WLAN_FEATURE_VOWIFI_11R */
