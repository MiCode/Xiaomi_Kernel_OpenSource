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

#if !defined( __I_VOS_TRACE_H )
#define __I_VOS_TRACE_H

#if !defined(__printf)
#define __printf(a,b)
#endif

/**=========================================================================
  
  \file  i_vos_trace.h
  
  \brief Linux-specific definitions for VOSS trace
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------
  
 \brief VOS_TRACE() / vos_trace_msg() - Trace / logging API
   
 Users wishing to add tracing information to their code should use 
 VOS_TRACE.  VOS_TRACE() will compile into a call to vos_trace_msg() when
 tracing is enabled.
  
 \param module - module identifier.   A member of the VOS_MODULE_ID
                 enumeration that identifies the module issuing the trace message.
         
 \param level - trace level.   A member of the VOS_TRACE_LEVEL 
                enumeration indicating the severity of the condition causing the
                trace message to be issued.   More severe conditions are more 
                likely to be logged.
         
   \param strFormat - format string.  The message to be logged.  This format
                      string contains printf-like replacement parameters, which follow
                      this parameter in the variable argument list.                    
  
   \return  nothing
    
  --------------------------------------------------------------------------*/
void __printf(3,4) vos_trace_msg( VOS_MODULE_ID module, VOS_TRACE_LEVEL level,
                                  char *strFormat, ... );

void vos_trace_hex_dump( VOS_MODULE_ID module, VOS_TRACE_LEVEL level,
                                void *data, int buf_len );

void vos_trace_display(void);

void vos_trace_setValue( VOS_MODULE_ID module, VOS_TRACE_LEVEL level, v_U8_t on );


// VOS_TRACE is the macro invoked to add trace messages to code.  See the 
// documenation for vos_trace_msg() for the parameters etc. for this function.
// 
// NOTE:  Code VOS_TRACE() macros into the source code.  Do not code directly
// to the vos_trace_msg() function.
//
// NOTE 2:  vos tracing is totally turned off if WLAN_DEBUG is *not* defined.
// This allows us to build 'performance' builds where we can measure performance
// without being bogged down by all the tracing in the code.
#if defined( WLAN_DEBUG )
#define VOS_TRACE vos_trace_msg
#define VOS_TRACE_HEX_DUMP vos_trace_hex_dump
#else
#define VOS_TRACE(arg...)
#define VOS_TRACE_HEX_DUMP(arg...)
#endif


void __printf(3,4) vos_snprintf(char *strBuffer, unsigned  int size,
                                char *strFormat, ...);
#define VOS_SNPRINTF vos_snprintf

#ifdef VOS_ENABLE_TRACING


#define VOS_ASSERT( _condition ) do {                                   \
        if ( ! ( _condition ) )                                         \
        {                                                               \
            printk(KERN_CRIT "VOS ASSERT in %s Line %d\n", __func__, __LINE__); \
            WARN_ON(1);                                                 \
        }                                                               \
    } while(0)

#else 


  // This code will be used for compilation if tracing is to be compiled out
  // of the code so these functions/macros are 'do nothing'
  VOS_INLINE_FN void vos_trace_msg( VOS_MODULE_ID module, ... ){}
  
  #define VOS_ASSERT( _condition )

#endif

#ifdef PANIC_ON_BUG

#define VOS_BUG( _condition ) do {                                      \
        if ( ! ( _condition ) )                                         \
        {                                                               \
            printk(KERN_CRIT "VOS BUG in %s Line %d\n", __func__, __LINE__); \
            BUG_ON(1);                                                  \
        }                                                               \
    } while(0)

#else

#define VOS_BUG( _condition ) do {                                      \
        if ( ! ( _condition ) )                                         \
        {                                                               \
            printk(KERN_CRIT "VOS BUG in %s Line %d\n", __func__, __LINE__); \
            WARN_ON(1);                                                 \
        }                                                               \
    } while(0)

#endif

#define VOS_RETURN_ADDRESS  __builtin_return_address(0)

#endif
