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

#if !defined( BAP_HDD_MISC_H )
#define BAP_HDD_MISC_H

/**===========================================================================
  
  \file  BAP_HDD_MISC.h
  
  \brief Linux HDD Adapter Type
         Copyright 2008 (c) Qualcomm, Incorporated.
         All Rights Reserved.
         Qualcomm Confidential and Proprietary.
  
  ==========================================================================*/
  
/*--------------------------------------------------------------------------- 
  Include files
  -------------------------------------------------------------------------*/ 
  
#include <bapApi.h>
#include <vos_types.h>
/*--------------------------------------------------------------------------- 
  Function declarations and documenation
  -------------------------------------------------------------------------*/ 

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_SetConfig() - To updates some configuration for BAP module in
  SME
  
  This should be called after WLANBAP_Start().
  
  \param  - NA
  
  \return -
      The result code associated with performing the operation  

    VOS_STATUS_E_FAILURE:  failed to set the config in SME BAP 
    VOS_STATUS_SUCCESS:  Success

              
  --------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_SetConfig
(
    WLANBAP_ConfigType *pConfig
);

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_RegisterWithHCI() - To register WLAN PAL with HCI
  
  
  \param
   pAdapter : HDD adapter
  
  \return -
      The result code associated with performing the operation  

    VOS_STATUS_E_FAILURE:  failed to register with HCI 
    VOS_STATUS_SUCCESS:  Success

              
  --------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_RegisterWithHCI(hdd_adapter_t *pAdapter);

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_DeregisterFromHCI() - To deregister WLAN PAL with HCI
  
  
  \param - NA
  
  \return -
      The result code associated with performing the operation  

    VOS_STATUS_E_FAILURE:  failed to deregister with HCI 
    VOS_STATUS_SUCCESS:  Success

              
  --------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_DeregisterFromHCI(void);

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_StopAmp() - To stop the current AMP traffic/connection
  
  
  \param - NA
  
  \return -
      The result code associated with performing the operation  

    VOS_STATUS_E_FAILURE:  failed to stop AMP connection 
    VOS_STATUS_SUCCESS:  Success

              
  --------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_StopAmp(void);

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_AmpSessionOn() - To check if AMP connection is on currently
  
  
  \param - NA
  
  \return -
      The result code associated with performing the operation  

    VOS_TRUE:  AMP connection is on 
    VOS_FALSE: AMP connection is not on

              
  --------------------------------------------------------------------------*/
v_BOOL_t WLANBAP_AmpSessionOn(void);
#endif    // end #if !defined( BAP_HDD_MISC_H )
