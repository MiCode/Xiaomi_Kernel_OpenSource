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

#if !defined( __VOS_API_H )
#define __VOS_API_H

/**=========================================================================
  
  \file  vos_Api.h
  
  \brief virtual Operating System Services (vOSS) API
               
   Header file that inludes all the vOSS API definitions.
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/
 /*=========================================================================== 

                       EDIT HISTORY FOR FILE 
   
   
  This section contains comments describing changes made to the module. 
  Notice that changes are listed in reverse chronological order. 
   
   
  $Header:$ $DateTime: $ $Author: $ 
   
   
  when        who    what, where, why 
  --------    ---    --------------------------------------------------------
  06/23/08    hba     Added vos_preOpen()
  05/18/08    lac     Created module. 
===========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
// one stop shopping.  This brings in the entire vOSS API.
#include <vos_types.h>
#include <vos_status.h>
#include <vos_memory.h>
#include <vos_list.h>
#include <vos_getBin.h>
#include <vos_trace.h>
#include <vos_event.h>
#include <vos_lock.h>
#include <vos_nvitem.h>
#include <vos_mq.h>
#include <vos_packet.h>
#include <vos_threads.h>
#include <vos_timer.h>
#include <vos_pack_align.h>

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/

/**--------------------------------------------------------------------------
  
  \brief vos_preOpen() - PreOpen the vOSS Module  
    
  The \a vos_preOpen() function allocates the Vos Context, but do not      
  initialize all the members. This overal initialization will happen
  at vos_Open().
  The reason why we need vos_preOpen() is to get a minimum context 
  where to store BAL and SAL relative data, which happens before
  vos_Open() is called.
  
  \param  pVosContext: A pointer to where to store the VOS Context 
 
  
  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and 
          is ready to be used.
              
          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/   
          
  \sa vos_open()
  
---------------------------------------------------------------------------*/
VOS_STATUS vos_preOpen ( v_CONTEXT_t *pVosContext );

VOS_STATUS vos_preClose( v_CONTEXT_t *pVosContext );


VOS_STATUS vos_preStart( v_CONTEXT_t vosContext );


VOS_STATUS vos_open( v_CONTEXT_t *pVosContext, void *devHandle );



VOS_STATUS vos_start( v_CONTEXT_t vosContext ); 

VOS_STATUS vos_stop( v_CONTEXT_t vosContext );

VOS_STATUS vos_close( v_CONTEXT_t vosContext );

/* vos shutdown will not close control transport and will not handshake with Riva */
VOS_STATUS vos_shutdown( v_CONTEXT_t vosContext );

/* the wda interface to shutdown */
VOS_STATUS vos_wda_shutdown( v_CONTEXT_t vosContext );

/**---------------------------------------------------------------------------
  
  \brief vos_get_context() - get context data area
  
  Each module in the system has a context / data area that is allocated
  and maanged by voss.  This API allows any user to get a pointer to its 
  allocated context data area from the VOSS global context.  

  \param vosContext - the VOSS Global Context.  
  
  \param moduleId - the module ID, who's context data are is being retrived.
                      
  \return - pointer to the context data area.
  
          - NULL if the context data is not allocated for the module ID
            specified 
              
  --------------------------------------------------------------------------*/
v_VOID_t *vos_get_context( VOS_MODULE_ID moduleId, 
                           v_CONTEXT_t vosContext );


/**---------------------------------------------------------------------------
  
  \brief vos_get_global_context() - get VOSS global Context
  
  This API allows any user to get the VOS Global Context pointer from a
  module context data area.  
  
  \param moduleContext - the input module context pointer
  
  \param moduleId - the module ID who's context pointer is input in 
         moduleContext.
                      
  \return - pointer to the VOSS global context
  
          - NULL if the function is unable to retreive the VOSS context. 
              
  --------------------------------------------------------------------------*/
v_CONTEXT_t vos_get_global_context( VOS_MODULE_ID moduleId, 
                                    v_VOID_t *moduleContext );

v_U8_t vos_is_logp_in_progress(VOS_MODULE_ID moduleId, v_VOID_t *moduleContext);
void vos_set_logp_in_progress(VOS_MODULE_ID moduleId, v_U8_t value);

v_U8_t vos_is_load_unload_in_progress(VOS_MODULE_ID moduleId, v_VOID_t *moduleContext);
void vos_set_load_unload_in_progress(VOS_MODULE_ID moduleId, v_U8_t value);

v_U8_t vos_is_reinit_in_progress(VOS_MODULE_ID moduleId, v_VOID_t *moduleContext);
void vos_set_reinit_in_progress(VOS_MODULE_ID moduleId, v_U8_t value);

/**---------------------------------------------------------------------------
  
  \brief vos_alloc_context() - allocate a context within the VOSS global Context
  
  This API allows any user to allocate a user context area within the 
  VOS Global Context.  
  
  \param pVosContext - pointer to the global Vos context
  
  \param moduleId - the module ID who's context area is being allocated.
  
  \param ppModuleContext - pointer to location where the pointer to the 
                           allocated context is returned.  Note this 
                           output pointer is valid only if the API
                           returns VOS_STATUS_SUCCESS
  
  \param size - the size of the context area to be allocated.
                      
  \return - VOS_STATUS_SUCCESS - the context for the module ID has been 
            allocated successfully.  The pointer to the context area
            can be found in *ppModuleContext.  
            \note This function returns VOS_STATUS_SUCCESS if the 
            module context was already allocated and the size 
            allocated matches the size on this call.

            VOS_STATUS_E_INVAL - the moduleId is not a valid or does 
            not identify a module that can have a context allocated.

            VOS_STATUS_E_EXISTS - vos could allocate the requested context 
            because a context for this module ID already exists and it is
            a *different* size that specified on this call.
            
            VOS_STATUS_E_NOMEM - vos could not allocate memory for the 
            requested context area.  
              
  \sa vos_get_context(), vos_free_context()
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_alloc_context( v_VOID_t *pVosContext, VOS_MODULE_ID moduleID, 
                              v_VOID_t **ppModuleContext, v_SIZE_t size );


/**---------------------------------------------------------------------------
  
  \brief vos_free_context() - free an allocated a context within the 
                               VOSS global Context
  
  This API allows a user to free the user context area within the 
  VOS Global Context.  
  
  \param pVosContext - pointer to the global Vos context
  
  \param moduleId - the module ID who's context area is being free
  
  \param pModuleContext - pointer to module context area to be free'd.
                      
  \return - VOS_STATUS_SUCCESS - the context for the module ID has been 
            free'd.  The pointer to the context area is not longer 
            available.
            
            VOS_STATUS_E_FAULT - pVosContext or pModuleContext are not 
            valid pointers.
                                 
            VOS_STATUS_E_INVAL - the moduleId is not a valid or does 
            not identify a module that can have a context free'd.
            
            VOS_STATUS_E_EXISTS - vos could not free the requested 
            context area because a context for this module ID does not
            exist in the global vos context.
              
  \sa vos_get_context()              
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_free_context( v_VOID_t *pVosContext, VOS_MODULE_ID moduleID,
                             v_VOID_t *pModuleContext );
                             
v_BOOL_t vos_is_apps_power_collapse_allowed(void* pHddCtx);
void vos_abort_mac_scan(tANI_U8 sessionId);

/**
  @brief vos_wlanShutdown() - This API will shutdown WLAN driver

  This function is called when Riva subsystem crashes.  There are two
  methods (or operations) in WLAN driver to handle Riva crash,
    1. shutdown: Called when Riva goes down, this will shutdown WLAN
                 driver without handshaking with Riva.
    2. re-init:  Next API

  @param
       NONE
  @return
       VOS_STATUS_SUCCESS   - Operation completed successfully.
       VOS_STATUS_E_FAILURE - Operation failed.

*/
VOS_STATUS vos_wlanShutdown(void);

/**
  @brief vos_wlanReInit() - This API will re-init WLAN driver

  This function is called when Riva subsystem reboots.  There are two
  methods (or operations) in WLAN driver to handle Riva crash,
    1. shutdown: Previous API
    2. re-init:  Called when Riva comes back after the crash. This will
                 re-initialize WLAN driver. In some cases re-open may be
                 referred instead of re-init.
  @param
       NONE
  @return
       VOS_STATUS_SUCCESS   - Operation completed successfully.
       VOS_STATUS_E_FAILURE - Operation failed.

*/
VOS_STATUS vos_wlanReInit(void);

/**
  @brief vos_wlanRestart() - This API will reload WLAN driver.

  This function is called if driver detects any fatal state which 
  can be recovered by a WLAN module reload ( Android framwork initiated ).
  Note that this API will not initiate any RIVA subsystem restart.

  @param
       NONE
  @return
       VOS_STATUS_SUCCESS   - Operation completed successfully.
       VOS_STATUS_E_FAILURE - Operation failed.

*/
VOS_STATUS vos_wlanRestart(void);

/**
  @brief vos_fwDumpReq()

  This function is called to issue dump commands to Firmware

  @param
       cmd - Command No. to execute
       arg1 - argument 1 to cmd
       arg2 - argument 2 to cmd
       arg3 - argument 3 to cmd
       arg4 - argument 4 to cmd
  @return
       NONE
*/
v_VOID_t vos_fwDumpReq(tANI_U32 cmd, tANI_U32 arg1, tANI_U32 arg2,
                        tANI_U32 arg3, tANI_U32 arg4);

v_U64_t vos_get_monotonic_boottime(void);

VOS_STATUS vos_randomize_n_bytes(void *mac_addr, tANI_U32 n);

v_BOOL_t vos_is_wlan_in_badState(VOS_MODULE_ID moduleId,
                                 v_VOID_t *moduleContext);

#endif // if !defined __VOS_NVITEM_H
