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

#if !defined( __WLAN_QCT_OS_TRACE_H )
#define __WLAN_QCT_OS_TRACE_H

#include <vos_trace.h>

#if !defined(__printf)
#define __printf(a,b)
#endif

#ifdef WLAN_DEBUG

/**----------------------------------------------------------------------------
  
 \brief WPAL_TRACE() / wpalTrace() - Trace / logging API
   
 Users wishing to add tracing information to their code should use 
 WPAL_TRACE.  WPAL_TRACE() will compile into a call to wpalTrace() when
 tracing is enabled.
  
 \param module - module identifier.   A member of the wpt_moduleid
                 enumeration that identifies the module issuing the trace message.
         
 \param level - trace level.   A member of the wpt_tracelevel 
                enumeration indicating the severity of the condition causing the
                trace message to be issued.   More severe conditions are more 
                likely to be logged.
         
   \param strFormat - format string.  The message to be logged.  This format
                      string contains printf-like replacement parameters, which follow
                      this parameter in the variable argument list.                    
  
   \return  nothing
    
  --------------------------------------------------------------------------*/
void __printf(3,4) wpalTrace( wpt_moduleid module, wpt_tracelevel level,
                              char *strFormat, ... );

/**----------------------------------------------------------------------------
  
 \brief WPAL_DUMP() / wpalDump() - Trace / logging API
   
 Users wishing to add tracing memory dumps to their code should use 
 WPAL_DUMP.  WPAL_DUMP() will compile into a call to wpalDump() when
 tracing is enabled.
  
 \param module - module identifier.   A member of the wpt_moduleid
                 enumeration that identifies the module performing the dump
         
 \param level - trace level.   A member of the wpt_tracelevel 
                enumeration indicating the severity of the condition causing the
                memory to be dumped.   More severe conditions are more 
                likely to be logged.
         
 \param pMemory - memory.  A pointer to the memory to be dumped

 \param length - length.  How many bytes of memory to be dumped
  
   \return  nothing
    
  --------------------------------------------------------------------------*/
void wpalDump( wpt_moduleid module, wpt_tracelevel level,
               wpt_uint8 *memory, wpt_uint32 length);

#define WPAL_ASSERT( _condition )   do {                                \
        if ( ! ( _condition ) )                                         \
        {                                                               \
            printk(KERN_CRIT "VOS ASSERT in %s Line %d\n", __func__, __LINE__); \
            WARN_ON(1);                                                 \
        }                                                               \
    } while (0)
#else //WLAN_DEBUG

static inline void wpalTrace( wpt_moduleid module, wpt_tracelevel level,
                              char *strFormat, ... ){};
static inline void wpalDump( wpt_moduleid module, wpt_tracelevel level,
                             wpt_uint8 *memory, wpt_uint32 length) {};
static inline void wpalTraceSetLevel( wpt_moduleid module,
                         wpt_tracelevel level, wpt_boolean on ) {};
static inline void wpalTraceDisplay(void) {};
#define WPAL_ASSERT(x) do {} while (0);

#endif //WLAN_DEBUG

#define WPAL_TRACE wpalTrace
#define WPAL_DUMP wpalDump

#endif // __WLAN_QCT_OS_TRACE_H
