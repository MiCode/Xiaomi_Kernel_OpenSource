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

#ifndef WLAN_QCT_WLANBAP_API_EXT_H
#define WLAN_QCT_WLANBAP_API_EXT_H

/*===========================================================================

               W L A N   B T - A M P  P A L   L A Y E R 
                       E X T E R N A L  A P I
                
                   
DESCRIPTION
  This file contains the external APIs used by the wlan BT-AMP PAL layer 
  module.
  
      
  Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header: /cygdrive/e/Builds/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT/CORE/BAP/src/bapApiExt.h,v 1.1 2008/11/21 20:29:13 jzmuda Exp jzmuda $ $DateTime: $ $Author: jzmuda $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
10/22/08    jez     Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
// Pick up all the BT-AMP internal definitions 
// And underlying supporting types. (Including VOSS, CSR, and...)
#include "bapInternal.h" 

/* Pick up the SIRIUS and HAL types */ 
// Already taken care of, above 
//#include "sirApi.h"
//#include "halTypes.h"

/* Pick up the CCM API def'n */ 
#include "ccmApi.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
 #ifdef __cplusplus
 extern "C" {
 #endif 
 

/*----------------------------------------------------------------------------
 *  Defines
 * -------------------------------------------------------------------------*/
// Temporary 
//#define BAP_DEBUG

// How do I get BAP context from voss context? 
//#define VOS_GET_BAP_CB(ctx) vos_get_context( VOS_MODULE_ID_BAP, ctx) 
// How do I get halHandle from voss context? 
//#define VOS_GET_HAL_CB(ctx) vos_get_context( VOS_MODULE_ID_HAL, ctx) 

/*----------------------------------------------------------------------------
 *  Typedefs
 * -------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 *  External declarations for global context 
 * -------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 *  Function prototypes 
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  Utility Function prototypes 
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
);


#ifdef __cplusplus
 }
#endif 


#endif /* #ifndef WLAN_QCT_WLANBAP_API_EXT_H */

