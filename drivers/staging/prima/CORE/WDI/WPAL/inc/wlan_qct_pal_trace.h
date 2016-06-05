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

#if !defined( __WLAN_QCT_PAL_TRACE_H )
#define __WLAN_QCT_PAL_TRACE_H

/**=========================================================================
  
  \file  wlan_qct_pal_api.h
  
  \brief define general APIs PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform independent
  
  
  ========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"

typedef enum
{
   // NONE means NO traces will be logged.  This value is in place for the 
   // vos_trace_setlevel() to allow the user to turn off all traces.
   eWLAN_PAL_TRACE_LEVEL_NONE = 0,
   
   // the following trace levels are the ones that 'callers' of VOS_TRACE()
   // can specify in for the VOS_TRACE_LEVEL parameter.  Traces are classified
   // by severity (FATAL being more serious than INFO for example).   
   eWLAN_PAL_TRACE_LEVEL_FATAL,
   eWLAN_PAL_TRACE_LEVEL_ERROR, 
   eWLAN_PAL_TRACE_LEVEL_WARN,  
   eWLAN_PAL_TRACE_LEVEL_INFO,
   eWLAN_PAL_TRACE_LEVEL_INFO_HIGH,
   eWLAN_PAL_TRACE_LEVEL_INFO_MED,
   eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
   
   // ALL means all trace levels will be active.  This value is in place for the 
   // vos_trace_setlevel() to allow the user to turn ON all traces.
   eWLAN_PAL_TRACE_LEVEL_ALL, 
   
   // not a real level.  Used to identify the maximum number of 
   // VOS_TRACE_LEVELs defined.
   eWLAN_PAL_TRACE_LEVEL_COUNT
} wpt_tracelevel;

#include "wlan_qct_os_trace.h"

/*----------------------------------------------------------------------------
  
  \brief wpalTraceSetLevel() - Set the trace level for a particular module
  
  This is an external API that allows trace levels to be set for each module.
  
  \param module - id of the module whos trace level is being modified
  \param level - trace level.   A member of the wpt_tracelevel 
         enumeration indicating the severity of the condition causing the
         trace message to be issued.   More severe conditions are more 
         likely to be logged.
  \param on - boolean to indicate if tracing at the given level should be
         enabled or disabled.
         
  \return  nothing
    
  \sa
  --------------------------------------------------------------------------*/
void wpalTraceSetLevel( wpt_moduleid module, wpt_tracelevel level,
                        wpt_boolean on );

/**----------------------------------------------------------------------------
  
  \brief wpalTraceCheckLevel() 
  
  This is an external API that returns a boolean value to signify if a 
  particular trace level is set for the specified module.
  
  \param level - trace level.   A member of the wpt_tracelevel enumeration 
                 indicating the severity of the condition causing the trace 
                 message to be issued.
         
                 Note that individual trace levels are the only valid values
                 for this API.  eWLAN_PAL_TRACE_LEVEL_NONE and eWLAN_PAL_TRACE_LEVEL_ALL
                 are not valid input and will return FALSE

  \return  eWLAN_PAL_FALSE - the specified trace level for the specified module is OFF 
    
           eWLAN_PAL_TRUE - the specified trace level for the specified module is ON
    
  \sa 
  --------------------------------------------------------------------------*/
wpt_boolean wpalTraceCheckLevel( wpt_moduleid module, wpt_tracelevel level );


/*----------------------------------------------------------------------------
  
  \brief wpalTraceDisplay() - Display current state of trace level for
                              all modules
  
  This is an external API that allows trace levels to be displayed to
  an end user
  
  \param none

  \return  nothing
    
  \sa
  --------------------------------------------------------------------------*/
void wpalTraceDisplay(void);

#define WPAL_BUG VOS_BUG
#endif // __WLAN_QCT_PAL_TRACE_H
