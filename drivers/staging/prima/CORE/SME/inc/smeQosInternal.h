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

#if !defined( __SMEQOSINTERNAL_H )
#define __SMEQOSINTERNAL_H


/**=========================================================================
  
  \file  smeQosInternal.h
  
  \brief prototype for SME QoS APIs
  
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_lock.h"
#include "vos_trace.h"
#include "vos_memory.h"
#include "vos_types.h"
#include "aniGlobal.h"
#include "sirApi.h"
#include "sme_QosApi.h"
#include "smeInternal.h"

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
#define SME_QOS_AP_SUPPORTS_APSD         0x80

/*---------------------------------------------------------------------------
   Enumeration of the various EDCA Access Categories:
   Based on AC to ACI mapping in 802.11e spec (identical to WMM)
---------------------------------------------------------------------------*/
typedef enum
{
   SME_QOS_EDCA_AC_BE = 0,  /* Best effort access category             */
   SME_QOS_EDCA_AC_BK = 1,  /* Background access category              */
   SME_QOS_EDCA_AC_VI = 2,  /* Video access category                   */
   SME_QOS_EDCA_AC_VO = 3,  /* Voice access category                   */
  
   SME_QOS_EDCA_AC_MAX
} sme_QosEdcaAcType;


/*---------------------------------------------------------------------------
   Enumeration of the various CSR event indication types that would be reported 
   by CSR
---------------------------------------------------------------------------*/
typedef enum
{
   SME_QOS_CSR_JOIN_REQ = 0,
   SME_QOS_CSR_ASSOC_COMPLETE,
   SME_QOS_CSR_REASSOC_REQ,
   SME_QOS_CSR_REASSOC_COMPLETE,
   SME_QOS_CSR_REASSOC_FAILURE,
   SME_QOS_CSR_DISCONNECT_REQ,
   SME_QOS_CSR_DISCONNECT_IND,
   SME_QOS_CSR_HANDOFF_ASSOC_REQ,
   SME_QOS_CSR_HANDOFF_COMPLETE,
   SME_QOS_CSR_HANDOFF_FAILURE,
#ifdef WLAN_FEATURE_VOWIFI_11R
   SME_QOS_CSR_PREAUTH_SUCCESS_IND,
   SME_QOS_CSR_SET_KEY_SUCCESS_IND,
#endif
}sme_QosCsrEventIndType;

#ifdef FEATURE_WLAN_DIAG_SUPPORT
typedef enum
{
   SME_QOS_DIAG_ADDTS_REQ = 0,
   SME_QOS_DIAG_ADDTS_RSP,
   SME_QOS_DIAG_DELTS

}sme_QosDiagQosEventSubtype;

typedef enum
{
   SME_QOS_DIAG_ADDTS_ADMISSION_ACCEPTED = 0,
   SME_QOS_DIAG_ADDTS_INVALID_PARAMS,
   SME_QOS_DIAG_ADDTS_RESERVED,
   SME_QOS_DIAG_ADDTS_REFUSED,
   SME_QOS_DIAG_USER_REQUESTED,
   SME_QOS_DIAG_DELTS_IND_FROM_AP,

}sme_QosDiagQosEventReasonCode;

#endif //FEATURE_WLAN_DIAG_SUPPORT
/*---------------------------------------------------------------------------
    The association information structure to be passed by CSR after assoc or 
    reassoc is done
---------------------------------------------------------------------------*/
typedef struct
{ 
   tSirBssDescription            *pBssDesc;
   tCsrRoamProfile               *pProfile;
} sme_QosAssocInfo;

/*-------------------------------------------------------------------------- 
                         External APIs for CSR - Internal to SME
  ------------------------------------------------------------------------*/

/* --------------------------------------------------------------------------
    \brief sme_QosOpen() - This function must be called before any API call to 
    SME QoS module.

    \param pMac - Pointer to the global MAC parameter structure.
    
    \return eHalStatus     
----------------------------------------------------------------------------*/
eHalStatus sme_QosOpen(tpAniSirGlobal pMac);

/* --------------------------------------------------------------------------
    \brief sme_QosClose() - To close down SME QoS module. There should not be 
    any API call into this module after calling this function until another
    call of sme_QosOpen.

    \param pMac - Pointer to the global MAC parameter structure.
    
    \return eHalStatus     
----------------------------------------------------------------------------*/
eHalStatus sme_QosClose(tpAniSirGlobal pMac);

/*--------------------------------------------------------------------------
  \brief sme_QosSetParams() - This function is used by HDD to provide the 
   default TSPEC params to SME.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info per AC as defined above, provided by HDD
  
  \return eHAL_STATUS_SUCCESS - Setparam is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosSetParams(tpAniSirGlobal pMac, sme_QosWmmTspecInfo * pQoSInfo);

/*--------------------------------------------------------------------------
  \brief sme_QosMsgProcessor() - sme_ProcessMsg() calls this function for the 
  messages that are handled by SME QoS module.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param msg_type - the type of msg passed by PE as defined in wniApi.h
  \param pMsgBuf - a pointer to a buffer that maps to various structures base 
                   on the message type.
                   The beginning of the buffer can always map to tSirSmeRsp.
  
  \return eHalStatus.
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosMsgProcessor( tpAniSirGlobal pMac,  v_U16_t msg_type, 
                                void *pMsgBuf);

/*-------------------------------------------------------------------------- 
                         Internal APIs for CSR
  ------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
  \brief sme_QosValidateParams() - The SME QoS API exposed to CSR to validate AP 
  capabilities regarding QoS support & any other QoS parameter validation.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param pBssDesc - Pointer to the BSS Descriptor information passed down by 
                    CSR to PE while issuing the Join request
  
  \return eHAL_STATUS_SUCCESS - Validation is successful
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosValidateParams(tpAniSirGlobal pMac, 
                                 tSirBssDescription *pBssDesc);

/*--------------------------------------------------------------------------
  \brief sme_QosCsrEventInd() - The QoS sub-module in SME expects notifications 
  from CSR when certain events occur as mentioned in sme_QosCsrEventIndType.

  \param pMac - Pointer to the global MAC parameter structure.
  \param ind - The event occurred of type sme_QosCsrEventIndType.
  \param pEvent_info - Information related to the event
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosCsrEventInd(tpAniSirGlobal pMac,
                              v_U8_t sessionId,
                              sme_QosCsrEventIndType ind, 
                              void *pEvent_info);

/*--------------------------------------------------------------------------
  \brief sme_QosGetACMMask() - The QoS sub-module API to find out on which ACs
  AP mandates Admission Control (ACM = 1)

  \param pMac - Pointer to the global MAC parameter structure.
  \param pSirBssDesc - The event occurred of type sme_QosCsrEventIndType.
  \param pIes - the parsed IE for pSirBssDesc. This can be NULL.

  
  \return a bit mask indicating for which ACs AP has ACM set to 1
  
  \sa
  
  --------------------------------------------------------------------------*/
v_U8_t sme_QosGetACMMask(tpAniSirGlobal pMac, tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes);

/*
  sme_QosTriggerUapsdChange
        It trigger a change on UAPSD (either disable/enable UAPSD) on current QoS flows
*/
sme_QosStatusType sme_QosTriggerUapsdChange( tpAniSirGlobal pMac );

void sme_QoSUpdateUapsdBTEvent(tpAniSirGlobal pMac);

#ifdef FEATURE_WLAN_ESE
v_U8_t sme_QosESERetrieveTspecInfo(tpAniSirGlobal pMac, v_U8_t sessionId, tTspecInfo *pTspecInfo);

#endif

#endif //#if !defined( __SMEQOSINTERNAL_H )
