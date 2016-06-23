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

/*===========================================================================

                      b a p A p i E x t . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the external interfaces
  required by the WLAN BAP module.  It is currently a temporary 
  respository for API routines which should be furnished by CSR
  or TL, but aren't yet implemented.
  
  The functions provide by this module are called by the rest of 
  the BT-AMP PAL module.

  DEPENDENCIES: 

  Are listed for each API below. 
  
  
  Copyright (c) 2008 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /cygdrive/e/Builds/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT/CORE/BAP/src/bapApiExt.c,v 1.1 2008/11/21 20:28:18 jzmuda Exp jzmuda $$DateTime$$Author: jzmuda $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-10-22    jez     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
// I think this pulls in everything
#include "bapApiExt.h"

//#define BAP_DEBUG
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  External declarations for global context 
 * -------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
* -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Utility Function implementations 
 * -------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANBAP_GetCurrentChannel

  DESCRIPTION 
    Clear out all fields in the BAP context.
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pBtampCtx:   pointer to the BAP control block
    channel:     current configured channel number.
    activeFlag:  flag indicating whether there is an active link.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to return channel is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_GetCurrentChannel
( 
  ptBtampContext  pBtampCtx,
  v_U32_t *channel, // return current channel here
  v_U32_t *activeFlag   // return active flag here
)
{
  //v_U32_t cb_enabled;
  tHalHandle halHandle;

  /*------------------------------------------------------------------------
    Sanity check BAP control block 
   ------------------------------------------------------------------------*/

  if (( NULL == pBtampCtx ) || (NULL == channel) || (NULL == activeFlag))
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "Invalid BAP pointer in %s", __func__);
    return VOS_STATUS_E_FAULT;
  }

  halHandle =  VOS_GET_HAL_CB(pBtampCtx->pvosGCtx);

  if(NULL == halHandle)
  {
     VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                  "halHandle is NULL in %s", __func__);
     return VOS_STATUS_E_FAULT;
  }

  if (ccmCfgGetInt(halHandle, WNI_CFG_CURRENT_CHANNEL, channel) 
          != eHAL_STATUS_SUCCESS ) 
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "Get CFG failed in %s", __func__);
    return VOS_STATUS_E_FAULT;
  }

  *activeFlag  = FALSE;  // return active flag here

  return VOS_STATUS_SUCCESS;
}/* WLANBAP_GetCurrentChannel */


