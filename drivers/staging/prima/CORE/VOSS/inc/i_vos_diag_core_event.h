/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
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

#if !defined( __I_VOS_DIAG_CORE_EVENT_H )
#define __I_VOS_DIAG_CORE_EVENT_H

/**=========================================================================
  
  \file  i_vos_diag_core_event.h
  
  \brief Android specific definitions for vOSS DIAG events
  
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#ifdef FEATURE_WLAN_DIAG_SUPPORT
#include <event_defs.h>
#endif

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef FEATURE_WLAN_DIAG_SUPPORT

void vos_event_report_payload(v_U16_t event_Id, v_U16_t length, v_VOID_t *pPayload);
/*---------------------------------------------------------------------------
  Allocate an event payload holder
---------------------------------------------------------------------------*/
#define WLAN_VOS_DIAG_EVENT_DEF( payload_name, payload_type ) \
           payload_type(payload_name)                         

/*---------------------------------------------------------------------------
  Report the event
---------------------------------------------------------------------------*/
#define WLAN_VOS_DIAG_EVENT_REPORT( payload_ptr, ev_id ) \
   do                                                    \
   {                                                     \
       vos_event_report_payload( ev_id,                  \
                              sizeof( *(payload_ptr) ),  \
                              (void *)(payload_ptr) );   \
                                                       \
   } while (0)

#else /* FEATURE_WLAN_DIAG_SUPPORT */

#define WLAN_VOS_DIAG_EVENT_DEF( payload_name, payload_type ) 
#define WLAN_VOS_DIAG_EVENT_REPORT( payload_ptr, ev_id ) 

#endif /* FEATURE_WLAN_DIAG_SUPPORT */


/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
#ifdef FEATURE_WLAN_DIAG_SUPPORT
void vos_log_wlock_diag(uint32_t reason, const char *wake_lock_name,
                              uint32_t timeout, uint32_t status);
#else
static inline void vos_log_wlock_diag(uint32_t reason,
                                 const char *wake_lock_name,
                           uint32_t timeout, uint32_t status)
{

}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __I_VOS_DIAG_CORE_EVENT_H
