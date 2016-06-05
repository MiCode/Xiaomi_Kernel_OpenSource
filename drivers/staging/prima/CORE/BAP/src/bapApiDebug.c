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

                      b a p A p i D e b u g . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN BAP modules
  Debug functions.
  
  The functions externalized by this module are to be called ONLY by other 
  WLAN modules (HDD) that properly register with the BAP Layer initially.

  DEPENDENCIES: 

  Are listed for each API below. 
  
  
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /cygdrive/e/Builds/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT/CORE/BAP/src/bapApiDebug.c,v 1.2 2008/11/10 22:37:58 jzmuda Exp jzmuda $$DateTime$$Author: jzmuda $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-09-15    jez     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
//#include "wlan_qct_tl.h"
#include "vos_trace.h"

/* BT-AMP PAL API header file */ 
#include "bapApi.h" 
#include "bapInternal.h" 

//
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

/*
Debug Commands
*/

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLoopbackMode()

  DESCRIPTION 
    Implements the actual HCI Read Loopback Mode command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadLoopbackMode:  pointer to the "HCI Read Loopback Mode".
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLoopbackMode or 
                         pBapHCILoopbackMode is NULL.
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLoopbackMode
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Loopback_Mode_Cmd  *pBapHCIReadLoopbackMode,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadLoopbackMode */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteLoopbackMode()

  DESCRIPTION 
    Implements the actual HCI Write Loopback Mode command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteLoopbackMode:  pointer to the "HCI Write Loopback Mode" Structure.
    
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteLoopbackMode is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteLoopbackMode
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Loopback_Mode_Cmd   *pBapHCIWriteLoopbackMode,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPWriteLoopbackMode */





