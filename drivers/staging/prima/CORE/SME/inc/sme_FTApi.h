/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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

#if !defined( __SME_FTAPI_H )
#define __SME_FTAPI_H

#include <limFTDefs.h>
#include <palTimer.h>

/**=========================================================================
  
  \brief macros and prototype for SME APIs
  
  ========================================================================*/
typedef enum eFTIEState
{
    eFT_START_READY,                // Start before and after 11r assoc
    eFT_AUTH_REQ_READY,             // When we have recvd the 1st or nth auth req
    eFT_WAIT_AUTH2,                 // Sent auth1 and waiting auth2
    eFT_AUTH_COMPLETE,              // We are now ready for FT phase, send auth1, recd auth2
    eFT_REASSOC_REQ_WAIT,           // Now we have sent Auth Rsp to the supplicant and waiting
                                    // Reassoc Req from the supplicant.
    eFT_SET_KEY_WAIT,               // We have received the Reassoc request from 
                                    // supplicant. Waiting for the keys.
} tFTIEStates;


typedef struct sFTSMEContext
{
    tANI_U8           *auth_ft_ies;
    tANI_U32          auth_ft_ies_length;

    tANI_U8           *reassoc_ft_ies;
    tANI_U16          reassoc_ft_ies_length;

    // Pre-Auth info
    tFTIEStates       FTState;               // The state of FT in the current 11rAssoc
    tSirMacAddr       preAuthbssId;          // BSSID to preauth to
    tANI_U32          smeSessionId;

    // Saved pFTPreAuthRsp
    tpSirFTPreAuthRsp psavedFTPreAuthRsp;
    v_BOOL_t          setFTPreAuthState;
    v_BOOL_t          setFTPTKState;

    // Time to trigger reassoc once pre-auth is successful
    vos_timer_t       preAuthReassocIntvlTimer;
    tCsrRoamSetKey    *pCsrFTKeyInfo;

    v_BOOL_t          addMDIE;
} tftSMEContext, *tpftSMEContext;

/*--------------------------------------------------------------------------
  Prototype functions
  ------------------------------------------------------------------------*/
void sme_FTOpen(tHalHandle hHal);
void sme_FTClose(tHalHandle hHal);
void sme_SetFTIEs( tHalHandle hHal, tANI_U8 sessionId, const tANI_U8 *ft_ies, tANI_U16 ft_ies_length );
eHalStatus sme_FTUpdateKey( tHalHandle hHal, tCsrRoamSetKey * pFTKeyInfo );
void csrFTPreAuthRspProcessor( tHalHandle hHal, tpSirFTPreAuthRsp pFTPreAuthRsp );
void sme_GetFTPreAuthResponse( tHalHandle hHal, tANI_U8 *ft_ies, tANI_U32 ft_ies_ip_len, tANI_U16 *ft_ies_length );
void sme_GetRICIEs( tHalHandle hHal, tANI_U8 *ric_ies, tANI_U32 ric_ies_ip_len, tANI_U32 *ric_ies_length );
void sme_PreauthReassocIntvlTimerCallback(void *context);
void sme_SetFTPreAuthState(tHalHandle hHal, v_BOOL_t state);
v_BOOL_t sme_GetFTPreAuthState(tHalHandle hHal);
v_BOOL_t sme_GetFTPTKState(tHalHandle hHal);
void sme_SetFTPTKState(tHalHandle hHal, v_BOOL_t state);
#if defined(WLAN_FEATURE_VOWIFI_11R)
void sme_FTReset(tHalHandle hHal);
#endif



#endif //#if !defined( __SME_FTAPI_H )
