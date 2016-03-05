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

#ifdef FEATURE_OEM_DATA_SUPPORT

/** ------------------------------------------------------------------------- * 
    ------------------------------------------------------------------------- *  

  
    \file oemDataApi.h
  
    Exports and types for the Common OEM DATA REQ/RSP Module interfaces.
  
    Copyright (C) 2010 Qualcomm Inc.
  
 
   ========================================================================== */

#ifndef __OEM_DATA_API_H__
#define __OEM_DATA_API_H__
#include "sirApi.h"
#include "sirMacProtDef.h"
#include "csrLinkList.h"

#ifndef OEM_DATA_REQ_SIZE
#define OEM_DATA_REQ_SIZE 134
#endif

#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 1968
#endif

/*************************************************************************************************************
  OEM DATA REQ/RSP - DATA STRUCTURES
*************************************************************************************************************/

/* Structure for defining req sent to the PE */
typedef struct tagOemDataReq
{
    tANI_U8   sessionId;
    tANI_U8   oemDataReq[OEM_DATA_REQ_SIZE];
} tOemDataReq, tOemDataReqConfig;

/*************************************************************************************************************
  OEM DATA RESPONSE - DATA STRUCTURES
*************************************************************************************************************/
typedef struct tagOemDataRsp
{
    tANI_U8   oemDataRsp[OEM_DATA_RSP_SIZE];
} tOemDataRsp;

/*************************************************************************************************************/

typedef enum
{
    eOEM_DATA_REQ_SUCCESS=1,
    eOEM_DATA_REQ_FAILURE,
    eOEM_DATA_REQ_INVALID_MODE,
} eOemDataReqStatus;

/* ---------------------------------------------------------------------------
    \fn oemData_OemDataReqOpen
    \brief This function must be called before any API call to MEAS (OEM DATA REQ/RSP module)
    \return eHalStatus     
  -------------------------------------------------------------------------------*/

eHalStatus oemData_OemDataReqOpen(tHalHandle hHal);

/* ---------------------------------------------------------------------------
    \fn oemData_OemDataReqClose
    \brief This function must be called before closing the csr module
    \return eHalStatus     
  -------------------------------------------------------------------------------*/

eHalStatus oemData_OemDataReqClose(tHalHandle hHal);

/* HDD Callback function for the sme to callback when the oem data rsp is available */
typedef eHalStatus (*oemData_OemDataReqCompleteCallback)(
                                           tHalHandle, 
                                           void* p2, 
                                           tANI_U32 oemDataReqID, 
                                           eOemDataReqStatus status);

/* ---------------------------------------------------------------------------
    \fn oemData_OemDataReq
    \brief Request an OEM DATA RSP
    \param sessionId - Id of session to be used
    \param pOemDataReqID - pointer to an object to get back the request ID
    \param callback - a callback function that is called upon finish
    \param pContext - a pointer passed in for the callback
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus oemData_OemDataReq(tHalHandle, tANI_U8, tOemDataReqConfig *, tANI_U32 *pOemDataReqID, 
                            oemData_OemDataReqCompleteCallback callback, void *pContext);

/* ---------------------------------------------------------------------------
    \fn sme_HandleOemDataRsp
    \brief This function processes the oem data response obtained from the PE
    \param pMsg - Pointer to the pSirSmeOemDataRsp
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus sme_HandleOemDataRsp(tHalHandle hHal, tANI_U8*);

/* ---------------------------------------------------------------------------
    \fn oemData_IsOemDataReqAllowed
    \brief This function checks if oem data req/rsp can be performed in the 
           current driver state
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus oemData_IsOemDataReqAllowed(tHalHandle hHal);

#endif //_OEM_DATA_API_H__

#endif //FEATURE_OEM_DATA_SUPPORT
