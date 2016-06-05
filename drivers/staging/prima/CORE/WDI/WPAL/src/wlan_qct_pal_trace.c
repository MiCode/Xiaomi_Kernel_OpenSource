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

/**=========================================================================
  
  \file  wlan_qct_pal_trace.c
  
  \brief Implementation trace/logging APIs PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for Linux/Android platform
  
  
  ========================================================================*/

#include "wlan_qct_pal_trace.h"
#include "i_vos_types.h"

#ifdef WLAN_DEBUG


/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#define WPAL_TRACE_BUFFER_SIZE ( 512 )

// macro to map wpal trace levels into the bitmask
#define WPAL_TRACE_LEVEL_TO_MODULE_BITMASK( _level ) ( ( 1 << (_level) ) )

typedef struct
{
   // Trace level for a module, as a bitmask.  The bits in this mask
   // are ordered by wpt_tracelevel.  For example, each bit represents
   // one of the bits in wpt_tracelevel that may be turned on to have
   // traces at that level logged, i.e. if eWLAN_PAL_TRACE_LEVEL_ERROR is
   // == 2, then if bit 2 (low order) is turned ON, then ERROR traces
   // will be printed to the trace log.
   //
   // Note that all bits turned OFF means no traces.
   wpt_uint16 moduleTraceLevel;

   // 3 character string name for the module
   wpt_uint8 moduleNameStr[ 4 ];   // 3 chars plus the NULL

} moduleTraceInfo;


// Array of static data that contains all of the per module trace
// information.  This includes the trace level for the module and
// the 3 character 'name' of the module for marking the trace logs.
moduleTraceInfo gTraceInfo[ eWLAN_MODULE_COUNT ] =
{
   { (1<<eWLAN_PAL_TRACE_LEVEL_FATAL)|(1<<eWLAN_PAL_TRACE_LEVEL_ERROR), "DAL" }, 
   { (1<<eWLAN_PAL_TRACE_LEVEL_FATAL)|(1<<eWLAN_PAL_TRACE_LEVEL_ERROR), "CTL" },
   { (1<<eWLAN_PAL_TRACE_LEVEL_FATAL)|(1<<eWLAN_PAL_TRACE_LEVEL_ERROR), "DAT" }, 
   { (1<<eWLAN_PAL_TRACE_LEVEL_FATAL)|(1<<eWLAN_PAL_TRACE_LEVEL_ERROR), "PAL" }, 
};


// the trace level strings in an array.  these are ordered in the same order
// as the trace levels are defined in the enum (see wpt_tracelevel) so we
// can index into this array with the level and get the right string.  The
// trace levels are...
// none, Fatal, Error, Warning, Info, InfoHigh, InfoMed, InfoLow
static const char * TRACE_LEVEL_STR[] = {
   "  ", "F ", "E ", "W ", "I ", "IH", "IM", "IL" };


/*-------------------------------------------------------------------------
  Functions
  ------------------------------------------------------------------------*/
static void wpalOutput(wpt_tracelevel level, char *strBuffer)
{
   switch(level)
   {
   default:
      printk(KERN_CRIT "%s: Unknown trace level passed in!\n", __func__); 
      // fall thru and use FATAL

   case eWLAN_PAL_TRACE_LEVEL_FATAL:
      printk(KERN_CRIT "%s\n", strBuffer);
      break;

   case eWLAN_PAL_TRACE_LEVEL_ERROR:
      printk(KERN_ERR "%s\n", strBuffer);
      break;

   case eWLAN_PAL_TRACE_LEVEL_WARN:
      printk(KERN_WARNING "%s\n", strBuffer);
      break;

   case eWLAN_PAL_TRACE_LEVEL_INFO:
      printk(KERN_INFO "%s\n", strBuffer);
      break;

   case eWLAN_PAL_TRACE_LEVEL_INFO_HIGH:
      printk(KERN_NOTICE "%s\n", strBuffer);
      break;

   case eWLAN_PAL_TRACE_LEVEL_INFO_MED:
      printk(KERN_NOTICE "%s\n", strBuffer);
      break;

   case eWLAN_PAL_TRACE_LEVEL_INFO_LOW:
      printk(KERN_INFO "%s\n", strBuffer);
      break;
   }
}

void wpalTraceSetLevel( wpt_moduleid module, wpt_tracelevel level,
                        wpt_boolean on )
{
   // Make sure the caller is passing in a valid LEVEL and MODULE.
   if ( (eWLAN_PAL_TRACE_LEVEL_COUNT <= level) ||
        (eWLAN_MODULE_COUNT <= module) )
   {
      return;
   }

   if ( eWLAN_PAL_TRACE_LEVEL_NONE == level )
   {
      // Treat 'none' differently.  NONE means we have to turn off all
      // the bits in the bit mask so none of the traces appear.
      gTraceInfo[ module ].moduleTraceLevel = 0;
   }
   else if ( eWLAN_PAL_TRACE_LEVEL_ALL == level )
   {
      // Treat 'all' differently.  ALL means we have to turn on all
      // the bits in the bit mask so all of the traces appear.
      gTraceInfo[ module ].moduleTraceLevel = 0xFFFF;
   }
   else
   {
      // We are turning a particular trace level on or off
      if (on)
      {
         // Set the desired bit in the bit mask for the module trace level.
         gTraceInfo[ module ].moduleTraceLevel |=
            WPAL_TRACE_LEVEL_TO_MODULE_BITMASK( level );
      }
      else
      {
         // Clear the desired bit in the bit mask for the module trace level.
         gTraceInfo[ module ].moduleTraceLevel &=
            ~(WPAL_TRACE_LEVEL_TO_MODULE_BITMASK( level ));
      }
   }
}

wpt_boolean wpalTraceCheckLevel( wpt_moduleid module, wpt_tracelevel level )
{
   wpt_boolean traceOn = eWLAN_PAL_FALSE;

   if ( ( eWLAN_PAL_TRACE_LEVEL_NONE == level ) ||
        ( level >= eWLAN_PAL_TRACE_LEVEL_COUNT ))
   {
      traceOn = eWLAN_PAL_FALSE;
   }
   else
   {
      traceOn = ( level & gTraceInfo[ module ].moduleTraceLevel ) ? eWLAN_PAL_TRUE : eWLAN_PAL_FALSE;
   }

   return( traceOn );
}

void wpalTraceDisplay(void)
{
   wpt_moduleid moduleId;

   printk(KERN_CRIT
          "     1)FATAL  2)ERROR  3)WARN  4)INFO  "
          "5)INFO_H  6)INFO_M  7)INFO_L\n"); 
   for (moduleId = 0; moduleId < eWLAN_MODULE_COUNT; ++moduleId)
   {
      printk(KERN_CRIT
             "%2d)%s    %s        %s       %s       "
             "%s        %s         %s         %s\n",
             (int)moduleId,
             gTraceInfo[moduleId].moduleNameStr,
             (gTraceInfo[moduleId].moduleTraceLevel &
              (1 << eWLAN_PAL_TRACE_LEVEL_FATAL)) ? "X":" ",
             (gTraceInfo[moduleId].moduleTraceLevel &
              (1 << eWLAN_PAL_TRACE_LEVEL_ERROR)) ? "X":" ",
             (gTraceInfo[moduleId].moduleTraceLevel &
              (1 << eWLAN_PAL_TRACE_LEVEL_WARN)) ? "X":" ",
             (gTraceInfo[moduleId].moduleTraceLevel &
              (1 << eWLAN_PAL_TRACE_LEVEL_INFO)) ? "X":" ",
             (gTraceInfo[moduleId].moduleTraceLevel &
              (1 << eWLAN_PAL_TRACE_LEVEL_INFO_HIGH)) ? "X":" ",
             (gTraceInfo[moduleId].moduleTraceLevel &
              (1 << eWLAN_PAL_TRACE_LEVEL_INFO_MED)) ? "X":" ",
             (gTraceInfo[moduleId].moduleTraceLevel &
              (1 << eWLAN_PAL_TRACE_LEVEL_INFO_LOW)) ? "X":" "
         );
   }

}

/*----------------------------------------------------------------------------

  \brief wpalTrace() - Externally called trace function

  Checks the level of severity and accordingly prints the trace messages

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

  \sa

  --------------------------------------------------------------------------*/
void wpalTrace( wpt_moduleid module, wpt_tracelevel level, char *strFormat, ... )
{
   wpt_uint8 strBuffer[ WPAL_TRACE_BUFFER_SIZE ];
   int n;

   // Print the trace message when the desired level bit is set in the module
   // tracel level mask.
   if ( gTraceInfo[ module ].moduleTraceLevel & WPAL_TRACE_LEVEL_TO_MODULE_BITMASK( level ) )
   {
      va_list val;
      va_start(val, strFormat);

      // print the prefix string into the string buffer...
      n = snprintf(strBuffer, WPAL_TRACE_BUFFER_SIZE, "wlan: [%d:%2s:%3s] ",
                   in_interrupt() ? 0 : current->pid,
                   (char *) TRACE_LEVEL_STR[ level ],
                   (char *) gTraceInfo[ module ].moduleNameStr);


      // print the formatted log message after the prefix string.
      // note we reserve space for the terminating NUL
      if ((n >= 0) && (n < WPAL_TRACE_BUFFER_SIZE))
      {
         vsnprintf(strBuffer + n, WPAL_TRACE_BUFFER_SIZE - n - 1, strFormat, val);
         wpalOutput(level, strBuffer);
      }
      va_end(val);
   }
}

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
// how many bytes do we output per line
#define BYTES_PER_LINE 16

// each byte takes 2 characters plus a space, plus need room for NUL
#define CHARS_PER_LINE ((BYTES_PER_LINE * 3) + 1)

void wpalDump( wpt_moduleid module, wpt_tracelevel level,
               wpt_uint8 *pMemory, wpt_uint32 length)
{
   char strBuffer[CHARS_PER_LINE];
   int n, num, offset;

   // Dump the memory when the desired level bit is set in the module
   // tracel level mask.
   if ( gTraceInfo[ module ].moduleTraceLevel & WPAL_TRACE_LEVEL_TO_MODULE_BITMASK( level ) )
   {
      num = 0;
      offset = 0;
      while (length > 0)
      {
         n = snprintf(strBuffer + offset, CHARS_PER_LINE - offset - 1,
                      "%02X ", *pMemory);
         offset += n;
         num++;
         length--;
         pMemory++;
         if (BYTES_PER_LINE == num)
         {
            wpalOutput(level, strBuffer);
            num = 0;
            offset = 0;
         }
      }

      if (offset > 0)
      {
         // partial line remains
         wpalOutput(level, strBuffer);
      }
   }
}
#endif //WLAN_DEBUG
