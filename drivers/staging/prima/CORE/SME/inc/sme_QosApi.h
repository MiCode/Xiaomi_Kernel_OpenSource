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

#if !defined( __SME_QOSAPI_H )
#define __SME_QOSAPI_H


/**=========================================================================
  
  \file  sme_QosApi.h
  
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

/*--------------------------------------------------------------------------
  Pre-processor Definitions
  ------------------------------------------------------------------------*/
#define SME_QOS_UAPSD_VO      0x01
#define SME_QOS_UAPSD_VI      0x02
#define SME_QOS_UAPSD_BE      0x08
#define SME_QOS_UAPSD_BK      0x04

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
   Enumeration of the various QoS status types that would be reported to HDD
---------------------------------------------------------------------------*/
typedef enum
{
   //async: once PE notifies successful TSPEC negotiation, or CSR notifies for
   //successful reassoc, notifies HDD with current QoS Params
   SME_QOS_STATUS_SETUP_SUCCESS_IND = 0, 
   //sync: only when App asked for APSD & it's already set with ACM = 0
   SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY,
   //both: sync or async: in case of async notifies HDD with current QoS Params
   SME_QOS_STATUS_SETUP_FAILURE_RSP, 
   //sync
   SME_QOS_STATUS_SETUP_INVALID_PARAMS_RSP, 
   //sync: AP doesn't support QoS (WMM)
   SME_QOS_STATUS_SETUP_NOT_QOS_AP_RSP, 
   //sync: either req has been sent down to PE or just buffered in SME
   SME_QOS_STATUS_SETUP_REQ_PENDING_RSP, 
   //async: in case of flow aggregation, if the new TSPEC negotiation is 
   //successful, OR, 
   //notify existing flows that TSPEC is modified with current QoS Params
   SME_QOS_STATUS_SETUP_MODIFIED_IND,
   //sync: no APSD asked for & ACM = 0
   SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP, 
   //async: In case of UAPSD, once PE notifies successful TSPEC negotiation, or 
   //CSR notifies for successful reassoc to SME-QoS, notify HDD if PMC can't
   //put the module in UAPSD mode right away (eHAL_STATUS_PMC_PENDING)
   SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_PENDING, 
   //async: In case of UAPSD, once PE notifies successful TSPEC negotiation, or 
   //CSR notifies for successful reassoc to SME-QoS, notify HDD if PMC can't
   //put the module in UAPSD mode at all (eHAL_STATUS_FAILURE)
   SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_SET_FAILED, 

   //sync: req has been sent down to PE in case of delts or addts for remain 
   // flows, OR if the AC doesn't have APSD or ACM
   //async: once the downgrade req for QoS params is successful
   SME_QOS_STATUS_RELEASE_SUCCESS_RSP = 100,
   //both: sync or async: in case of async notifies HDD with current QoS Params
   SME_QOS_STATUS_RELEASE_FAILURE_RSP,
   //async: AP sent DELTS indication
   SME_QOS_STATUS_RELEASE_QOS_LOST_IND,
   //sync: an addts req has been sent down to PE to downgrade the QoS params or 
   // just buffered in SME
   SME_QOS_STATUS_RELEASE_REQ_PENDING_RSP, 
   //sync
   SME_QOS_STATUS_RELEASE_INVALID_PARAMS_RSP, 

   //async: for QoS modify request if modification is successful, notifies HDD 
   // with current QoS Params
   SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND = 200,
   //sync: only when App asked for APSD & it's already set with ACM = 0
   SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_APSD_SET_ALREADY,
   //both: sync or async: in case of async notifies HDD with current QoS Params
   SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP,
   //sync: either req has been sent down to PE or just buffered in SME
   SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP,
   //sync: no APSD asked for & ACM = 0
   SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP, 
   //sync
   SME_QOS_STATUS_MODIFY_SETUP_INVALID_PARAMS_RSP, 
   //async: In case of UAPSD, once PE notifies successful TSPEC negotiation, or 
   //CSR notifies for successful reassoc to SME-QoS, notify HDD if PMC can't
   //put the module in UAPSD mode right away (eHAL_STATUS_PMC_PENDING)
   SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND_APSD_PENDING, 
   //async: In case of UAPSD, once PE notifies successful TSPEC negotiation, or 
   //CSR notifies for successful reassoc to SME-QoS, notify HDD if PMC can't
   //put the module in UAPSD mode at all (eHAL_STATUS_FAILURE)
   SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND_APSD_SET_FAILED,
   //sync: STA is handing off to a new AP
   SME_QOS_STATUS_HANDING_OFF = 300,
   //async:powersave mode changed by PMC from UAPSD to Full power
   SME_QOS_STATUS_OUT_OF_APSD_POWER_MODE_IND = 400,
   //async:powersave mode changed by PMC from Full power to UAPSD
   SME_QOS_STATUS_INTO_APSD_POWER_MODE_IND,
   
}sme_QosStatusType;

/*---------------------------------------------------------------------------
   Enumeration of the various User priority (UP) types
   
   From 802.1D/802.11e/WMM specifications (all refer to same table)
---------------------------------------------------------------------------*/
typedef enum
{
   SME_QOS_WMM_UP_BE      = 0,
   SME_QOS_WMM_UP_BK      = 1,
   SME_QOS_WMM_UP_RESV    = 2,    /* Reserved                              */
   SME_QOS_WMM_UP_EE      = 3,
   SME_QOS_WMM_UP_CL      = 4,
   SME_QOS_WMM_UP_VI      = 5,
   SME_QOS_WMM_UP_VO      = 6,
   SME_QOS_WMM_UP_NC      = 7,

   SME_QOS_WMM_UP_MAX

}sme_QosWmmUpType;

/*---------------------------------------------------------------------------
   Enumeration of the various TSPEC directions
   
   From 802.11e/WMM specifications
---------------------------------------------------------------------------*/

typedef enum
{
   SME_QOS_WMM_TS_DIR_UPLINK   = 0,
   SME_QOS_WMM_TS_DIR_DOWNLINK = 1,
   SME_QOS_WMM_TS_DIR_RESV     = 2,   /* Reserved                          */
   SME_QOS_WMM_TS_DIR_BOTH     = 3,

}sme_QosWmmDirType;

/*---------------------------------------------------------------------------
   Enumeration of the various TSPEC ack policies.
   
   From 802.11 WMM specification
---------------------------------------------------------------------------*/

typedef enum
{
   SME_QOS_WMM_TS_ACK_POLICY_NORMAL_ACK   = 0,
   SME_QOS_WMM_TS_ACK_POLICY_RESV1 = 1,
   SME_QOS_WMM_TS_ACK_POLICY_RESV2     = 2,   /* Reserved                          */
   SME_QOS_WMM_TS_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK     = 3,

}sme_QosWmmAckPolicyType;

/*---------------------------------------------------------------------------
   TS Info field in the WMM TSPEC
   
   See suggestive values above
---------------------------------------------------------------------------*/
typedef struct
{
   v_U8_t              burst_size_defn;
   sme_QosWmmAckPolicyType    ack_policy;
   sme_QosWmmUpType    up;        /* User priority                    */
   v_U8_t              psb;       /* power-save bit                   */
   sme_QosWmmDirType   direction; /* Direction                        */
   v_U8_t              tid;       /* TID : To be filled up by SME-QoS */
} sme_QosWmmTsInfoType;

/*---------------------------------------------------------------------------
    The WMM TSPEC Element (from the WMM spec)
---------------------------------------------------------------------------*/
typedef struct
{ 
  sme_QosWmmTsInfoType            ts_info;
  v_U16_t                         nominal_msdu_size;
  v_U16_t                         maximum_msdu_size;
  v_U32_t                         min_service_interval;
  v_U32_t                         max_service_interval;
  v_U32_t                         inactivity_interval;
  v_U32_t                         suspension_interval;
  v_U32_t                         svc_start_time;
  v_U32_t                         min_data_rate;
  v_U32_t                         mean_data_rate;
  v_U32_t                         peak_data_rate;
  v_U32_t                         max_burst_size;
  v_U32_t                         delay_bound;
  v_U32_t                         min_phy_rate;
  v_U16_t                         surplus_bw_allowance;
  v_U16_t                         medium_time;
  v_U8_t                          expec_psb_byapp;
} sme_QosWmmTspecInfo;


/*-------------------------------------------------------------------------- 
                         External APIs
  ------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
  \brief sme_QosCallback() - This is a callback function which is registered 
   per flow while HDD is requesting for QoS. Used for any notification for the 
   flow (i.e. setup success/failure/release) which needs to be sent to HDD. HDD
   will notify the application in turn, if needed.
  
  \param hHal - The handle returned by macOpen.
  \param HDDcontext - A cookie passed by HDD during QoS setup, to be used by SME 
                      during any QoS notification (through the callabck) to HDD 
  \param pCurrentQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM 
                           TSPEC related info as defined above, fed back to HDD
  \param status - The status of the flow running on an AC. It could be of 
                  sme_QosStatusType
  
  \return eHAL_STATUS_SUCCESS - Callback invoke successful.
  

  \sa
  
  --------------------------------------------------------------------------*/
typedef eHalStatus (*sme_QosCallback)(tHalHandle hHal, void * HDDcontext, 
                                      sme_QosWmmTspecInfo * pCurrentQoSInfo, 
                                      sme_QosStatusType status,
                                      v_U32_t QosFlowID);   

/*--------------------------------------------------------------------------
  \brief sme_QosSetupReq() - The SME QoS API exposed to HDD to request for QoS 
  on a particular AC. This function should be called after a link has been 
  established, i.e. STA is associated with an AP etc. If the request involves 
  admission control on the requested AC, HDD needs to provide the necessary 
  Traffic Specification (TSPEC) parameters otherwise SME is going to use the
  default params.
  
  \param hHal - The handle returned by macOpen.
  \param sessionId - sessionId returned by sme_OpenSession. Current QOS code doesn't 
                     support multiple session. This function returns failure when different
                     sessionId is passed in before calling sme_QosReleaseReq.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info as defined above, provided by HDD
  \param QoSCallback - The callback which is registered per flow while 
                       requesting for QoS. Used for any notification for the 
                       flow (i.e. setup success/failure/release) which needs to 
                       be sent to HDD
  \param HDDcontext - A cookie passed by HDD to be used by SME during any QoS 
                      notification (through the callabck) to HDD 
  \param UPType - Useful only if HDD or any other upper layer module (BAP etc.)
                  looking for implicit QoS setup, in that 
                  case, the pQoSInfo will be NULL & SME will know about the AC
                  (from the UP provided in this param) QoS is requested on
  \param pQosFlowID - Identification per flow running on each AC generated by 
                      SME. 
                     It is only meaningful if the QoS setup for the flow is 
                     successful
                  
  \return SME_QOS_STATUS_SETUP_SUCCESS - Setup request processed successfully.
  
          Other status means Setup request failed     
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosSetupReq(tHalHandle hHal, tANI_U32 sessionId,
                                  sme_QosWmmTspecInfo * pQoSInfo,
                                  sme_QosCallback QoSCallback, void * HDDcontext,
                                  sme_QosWmmUpType UPType, v_U32_t * pQosFlowID);

/*--------------------------------------------------------------------------
  \brief sme_QosModifyReq() - The SME QoS API exposed to HDD to request for 
  modification of certain QoS params on a flow running on a particular AC. 
  This function should be called after a link has been established, i.e. STA is 
  associated with an AP etc. & a QoS setup has been succesful for that flow. 
  If the request involves admission control on the requested AC, HDD needs to 
  provide the necessary Traffic Specification (TSPEC) parameters & SME might
  start the renegotiation process through ADDTS.
  
  \param hHal - The handle returned by macOpen.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info as defined above, provided by HDD
  \param QosFlowID - Identification per flow running on each AC generated by 
                      SME. 
                     It is only meaningful if the QoS setup for the flow has 
                     been successful already
                  
  \return SME_QOS_STATUS_SETUP_SUCCESS - Modification request processed 
                                         successfully.
  
          Other status means request failed     
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosModifyReq(tHalHandle hHal, 
                                   sme_QosWmmTspecInfo * pQoSInfo,
                                   v_U32_t QosFlowID);

/*--------------------------------------------------------------------------
  \brief sme_QosReleaseReq() - The SME QoS API exposed to HDD to request for 
  releasing a QoS flow running on a particular AC. This function should be 
  called only if a QoS is set up with a valid FlowID. HDD sould invoke this 
  API only if an explicit request for QoS release has come from Application 
  
  \param hHal - The handle returned by macOpen.
  \param QosFlowID - Identification per flow running on each AC generated by SME 
                     It is only meaningful if the QoS setup for the flow is 
                     successful
  
  \return SME_QOS_STATUS_RELEASE_SUCCESS - Release request processed 
                                           successfully.
  
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosReleaseReq(tHalHandle hHal, v_U32_t QosFlowID);

/*--------------------------------------------------------------------------
  \brief sme_QosIsTSInfoAckPolicyValid() - The SME QoS API exposed to HDD to 
  check if TS info ack policy field can be set to "HT-immediate block acknowledgement" 
  
  \param pMac - The handle returned by macOpen.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info, provided by HDD
  \param sessionId - sessionId returned by sme_OpenSession.
  
  \return VOS_TRUE - Current Association is HT association and so TS info ack policy
                     can be set to "HT-immediate block acknowledgement"
  
  \sa
  
  --------------------------------------------------------------------------*/
v_BOOL_t sme_QosIsTSInfoAckPolicyValid(tpAniSirGlobal pMac,
    sme_QosWmmTspecInfo * pQoSInfo,
    v_U8_t sessionId);

/*--------------------------------------------------------------------------
  \brief sme_QosTspecActive() - The SME QoS API exposed to HDD to
  check no of active Tspecs

  \param pMac - The handle returned by macOpen.
  \param ac - Determines type of Access Category
  \param sessionId - sessionId returned by sme_OpenSession.

  \return VOS_TRUE -When there is no error with pSession

  \sa
  --------------------------------------------------------------------------*/
v_BOOL_t sme_QosTspecActive(tpAniSirGlobal pMac,
    WLANTL_ACEnumType ac, v_U8_t sessionId, v_U8_t *pActiveTspec);


/*--------------------------------------------------------------------------
  \brief sme_QosUpdateHandOff() - Function which can be called to update
   Hand-off state of SME QoS Session
  \param sessionId - session id
  \param updateHandOff - value True/False to update the handoff flag

  \sa

-------------------------------------------------------------------------*/
void sme_QosUpdateHandOff(v_U8_t sessionId,
     v_BOOL_t updateHandOff);


/*--------------------------------------------------------------------------
  \brief sme_UpdateDSCPtoUPMapping() - Function which can be called to update
   qos mapping table maintained in HDD
  \param hHal - The handle returned by macOpen.
  \param dscpmapping - pointer to the qos mapping structure in HDD
  \param sessionId - session id

  \sa
-------------------------------------------------------------------------*/
VOS_STATUS sme_UpdateDSCPtoUPMapping(tHalHandle hHal,
    sme_QosWmmUpType* dscpmapping, v_U8_t sessionId);

#endif //#if !defined( __SME_QOSAPI_H )
