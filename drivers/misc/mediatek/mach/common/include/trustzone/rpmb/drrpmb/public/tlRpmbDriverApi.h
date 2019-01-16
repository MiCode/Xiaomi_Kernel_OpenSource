/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All rights reserved
 *
 * The present software is the confidential and proprietary information of
 * TRUSTONIC LIMITED. You shall not disclose the present software and shall
 * use it only in accordance with the terms of the license agreement you
 * entered into with TRUSTONIC LIMITED. This software may be subject to
 * export or import laws in certain countries.
 */

/*
 * @file   tlRpmbDriverApi.h
 * @brief  Contains trustlet API definitions
 *
 */

#ifndef __TLDRIVERAPI_H__
#define __TLDRIVERAPI_H__

#include "tlStd.h"
#include "TlApi/TlApiError.h"

#include "tlApirpmb.h"

typedef enum
{
  WIDEVINE_ID = 0,
  MARLIN_ID,
  HDCP_1X_TX_ID,
  HDCP_2X_V1_TX_ID,
  HDCP_2X_V1_RX_ID,
  HDCP_2X_V2_TX_ID,
  HDCP_2X_V2_RX_ID,
  PLAYREADY_BGROUPCERT_ID,
  PLAYREADY_ZGPRIV_ID,
  PLAYREADY_KEYFILE_ID,
  DEVICE_RSA_KEYPAIR,
  LEK_ID,
  GOOGLE_VOUCHER_ID,
  DAP_ID,
  DRM_KEY_MAX,
  DRM_SP_EKKB = 0xFFFF
} RPMB_USER_ID;

/*
 * Open session to the driver with given data
 *
 * @return  session id
 */
_TLAPI_EXTERN_C uint32_t tlApiRpmbOpenSession( uint32_t uid );


/*
 * Close session
 *
 * @param sid  session id
 *
 * @return  TLAPI_OK upon success or specific error
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiRpmbCloseSession( uint32_t sid );


/*
 * Executes command
 *
 * @param sid        session id
 * @param commandId  command id
 *
 * @return  TLAPI_OK upon success or specific error
 */
//_TLAPI_EXTERN_C tlApiResult_t tlApiExecute(
//        uint32_t sid,
//        tlApiRpmb_ptr RpmbData);

_TLAPI_EXTERN_C tlApiResult_t tlApiRpmbReadData(
        uint32_t sid,
        uint8_t *buf, 
        uint32_t bufSize,        
        int *result);        

_TLAPI_EXTERN_C tlApiResult_t tlApiRpmbWriteData(
        uint32_t sid,
        uint8_t *buf,
        uint32_t bufSize,        
        int *result);



/* tlApi function to call driver via IPC.
 * Function should be called only from customer specific TlApi library. 
 * Sends a MSG_RQ message via IPC to a MobiCore driver.
 *
 * @param driverID The driver to send the IPC to.
 * @param pMarParam MPointer to marshaling parameters to send to the driver.
 *
 * @return TLAPI_OK
 * @return E_TLAPI_COM_ERROR in case of an IPC error.
 */
_TLAPI_EXTERN_C tlApiResult_t tlApi_callDriver(
        uint32_t driver_ID,
        void* pMarParam);

_TLAPI_EXTERN_C tlApiResult_t tlApiRandomGenerateData (
        tlApiRngAlg_t alg,
        uint8_t * randomBuffer,
        size_t * randomLen);

#endif // __TLDRIVERAPI_H__
