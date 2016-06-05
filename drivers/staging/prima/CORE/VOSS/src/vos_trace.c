/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

  \file  vos_trace.c

  \brief virtual Operating System Servies (vOS)

   Trace, logging, and debugging definitions and APIs


  ========================================================================*/

/*===========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  09/16/08    hvm    Adding ability to set multiple trace levels per component
  09/11/08    lac    Added trace levels per component.  Cleanup from review.
  08/14/08    vpai   Particular modules and desired level can be selected
  06/20/08    vpai   Created Module
===========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_trace.h>
#include <aniGlobal.h>
#include <wlan_logging_sock_svc.h>
/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#define VOS_TRACE_BUFFER_SIZE ( 512 )

// macro to map vos trace levels into the bitmask
#define VOS_TRACE_LEVEL_TO_MODULE_BITMASK( _level ) ( ( 1 << (_level) ) )

typedef struct
{
   // Trace level for a module, as a bitmask.  The bits in this mask
   // are ordered by VOS_TRACE_LEVEL.  For example, each bit represents
   // one of the bits in VOS_TRACE_LEVEL that may be turned on to have
   // traces at that level logged, i.e. if VOS_TRACE_LEVEL_ERROR is
   // == 2, then if bit 2 (low order) is turned ON, then ERROR traces
   // will be printed to the trace log.
   //
   // Note that all bits turned OFF means no traces.
   v_U16_t moduleTraceLevel;

   // 3 character string name for the module
   unsigned char moduleNameStr[ 4 ];   // 3 chars plus the NULL

} moduleTraceInfo;

#define VOS_DEFAULT_TRACE_LEVEL \
   ((1<<VOS_TRACE_LEVEL_FATAL)|(1<<VOS_TRACE_LEVEL_ERROR))

// Array of static data that contains all of the per module trace
// information.  This includes the trace level for the module and
// the 3 character 'name' of the module for marking the trace logs.
moduleTraceInfo gVosTraceInfo[ VOS_MODULE_ID_MAX ] =
{
   [VOS_MODULE_ID_BAP]        = { VOS_DEFAULT_TRACE_LEVEL, "BAP" },
   [VOS_MODULE_ID_TL]         = { VOS_DEFAULT_TRACE_LEVEL, "TL " },
   [VOS_MODULE_ID_WDI]        = { VOS_DEFAULT_TRACE_LEVEL, "WDI" },
   [VOS_MODULE_ID_SVC]        = { VOS_DEFAULT_TRACE_LEVEL, "SVC" },
   [VOS_MODULE_ID_RSV4]       = { VOS_DEFAULT_TRACE_LEVEL, "RS4" },
   [VOS_MODULE_ID_HDD]        = { VOS_DEFAULT_TRACE_LEVEL, "HDD" },
   [VOS_MODULE_ID_SME]        = { VOS_DEFAULT_TRACE_LEVEL, "SME" },
   [VOS_MODULE_ID_PE]         = { VOS_DEFAULT_TRACE_LEVEL, "PE " },
   [VOS_MODULE_ID_WDA]        = { VOS_DEFAULT_TRACE_LEVEL, "WDA" },
   [VOS_MODULE_ID_SYS]        = { VOS_DEFAULT_TRACE_LEVEL, "SYS" },
   [VOS_MODULE_ID_VOSS]       = { VOS_DEFAULT_TRACE_LEVEL, "VOS" },
   [VOS_MODULE_ID_SAP]        = { VOS_DEFAULT_TRACE_LEVEL, "SAP" },
   [VOS_MODULE_ID_HDD_SOFTAP] = { VOS_DEFAULT_TRACE_LEVEL, "HSP" },
   [VOS_MODULE_ID_PMC]        = { VOS_DEFAULT_TRACE_LEVEL, "PMC" },
   [VOS_MODULE_ID_HDD_DATA]   = { VOS_DEFAULT_TRACE_LEVEL, "HDP" },
   [VOS_MODULE_ID_HDD_SAP_DATA] = { VOS_DEFAULT_TRACE_LEVEL, "SDP" },
};
/*-------------------------------------------------------------------------
  Static and Global variables
  ------------------------------------------------------------------------*/
static spinlock_t ltraceLock;

static tvosTraceRecord gvosTraceTbl[MAX_VOS_TRACE_RECORDS];
// Global vosTraceData
static tvosTraceData gvosTraceData;
/*
 * all the call back functions for dumping MTRACE messages from ring buffer
 * are stored in vostraceCBTable,these callbacks are initialized during init only
 * so, we will make a copy of these call back functions and maintain in to
 * vostraceRestoreCBTable. Incase if we make modifications to vostraceCBTable,
 * we can certainly retrieve all the call back functions back from Restore Table
 */
static tpvosTraceCb vostraceCBTable[VOS_MODULE_ID_MAX];
static tpvosTraceCb vostraceRestoreCBTable[VOS_MODULE_ID_MAX];
/*-------------------------------------------------------------------------
  Functions
  ------------------------------------------------------------------------*/
void vos_trace_setLevel( VOS_MODULE_ID module, VOS_TRACE_LEVEL level )
{
   // Make sure the caller is passing in a valid LEVEL.
   if ( level >= VOS_TRACE_LEVEL_MAX )
   {
      pr_err("%s: Invalid trace level %d passed in!\n", __func__, level);
      return;
   }

   // Treat 'none' differently.  NONE means we have to run off all
   // the bits in the bit mask so none of the traces appear.  Anything other
   // than 'none' means we need to turn ON a bit in the bitmask.
   if ( VOS_TRACE_LEVEL_NONE == level )
   {
      gVosTraceInfo[ module ].moduleTraceLevel = VOS_TRACE_LEVEL_NONE;
   }
   else
   {
      // Set the desired bit in the bit mask for the module trace level.
      gVosTraceInfo[ module ].moduleTraceLevel |= VOS_TRACE_LEVEL_TO_MODULE_BITMASK( level );
   }
}

void vos_trace_setValue( VOS_MODULE_ID module, VOS_TRACE_LEVEL level, v_U8_t on)
{
   // Make sure the caller is passing in a valid LEVEL.
   if ( level < 0  || level >= VOS_TRACE_LEVEL_MAX )
   {
      pr_err("%s: Invalid trace level %d passed in!\n", __func__, level);
      return;
   }

   // Make sure the caller is passing in a valid module.
   if ( module < 0 || module >= VOS_MODULE_ID_MAX )
   {
      pr_err("%s: Invalid module id %d passed in!\n", __func__, module);
      return;
   }

   // Treat 'none' differently.  NONE means we have to turn off all
   // the bits in the bit mask so none of the traces appear.
   if ( VOS_TRACE_LEVEL_NONE == level )
   {
      gVosTraceInfo[ module ].moduleTraceLevel = VOS_TRACE_LEVEL_NONE;
   }
   // Treat 'All' differently.  All means we have to turn on all
   // the bits in the bit mask so all of the traces appear.
   else if ( VOS_TRACE_LEVEL_ALL == level )
   {
      gVosTraceInfo[ module ].moduleTraceLevel = 0xFFFF;
   }

   else
   {
      if (on)
         // Set the desired bit in the bit mask for the module trace level.
         gVosTraceInfo[ module ].moduleTraceLevel |= VOS_TRACE_LEVEL_TO_MODULE_BITMASK( level );
      else
         // Clear the desired bit in the bit mask for the module trace level.
         gVosTraceInfo[ module ].moduleTraceLevel &= ~(VOS_TRACE_LEVEL_TO_MODULE_BITMASK( level ));
   }
}


v_BOOL_t vos_trace_getLevel( VOS_MODULE_ID module, VOS_TRACE_LEVEL level )
{
   v_BOOL_t traceOn = VOS_FALSE;

   if ( ( VOS_TRACE_LEVEL_NONE == level ) ||
        ( VOS_TRACE_LEVEL_ALL  == level ) ||
        ( level >= VOS_TRACE_LEVEL_MAX  )    )
   {
      traceOn = VOS_FALSE;
   }
   else
   {
      traceOn = ( level & gVosTraceInfo[ module ].moduleTraceLevel ) ? VOS_TRUE : VOS_FALSE;
   }

   return( traceOn );
}

void vos_snprintf(char *strBuffer, unsigned  int size, char *strFormat, ...)
{
    va_list val;

    va_start( val, strFormat );
    snprintf (strBuffer, size, strFormat, val);
    va_end( val );
}

#ifdef VOS_ENABLE_TRACING

/*----------------------------------------------------------------------------

  \brief vos_trace_msg() - Externally called trace function

  Checks the level of severity and accordingly prints the trace messages

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

  \sa

  --------------------------------------------------------------------------*/
void vos_trace_msg( VOS_MODULE_ID module, VOS_TRACE_LEVEL level, char *strFormat, ... )
{
   char strBuffer[VOS_TRACE_BUFFER_SIZE];
   int n;

   // Print the trace message when the desired level bit is set in the module
   // tracel level mask.
   if ( gVosTraceInfo[ module ].moduleTraceLevel & VOS_TRACE_LEVEL_TO_MODULE_BITMASK( level ) )
   {
      // the trace level strings in an array.  these are ordered in the same order
      // as the trace levels are defined in the enum (see VOS_TRACE_LEVEL) so we
      // can index into this array with the level and get the right string.  The
      // vos trace levels are...
      // none, Fatal, Error, Warning, Info, InfoHigh, InfoMed, InfoLow, Debug
      static const char * TRACE_LEVEL_STR[] = { "  ", "F ", "E ", "W ", "I ", "IH", "IM", "IL", "D" };
      va_list val;
      va_start(val, strFormat);

      // print the prefix string into the string buffer...
      n = snprintf(strBuffer, VOS_TRACE_BUFFER_SIZE, "wlan: [%2s:%3s] ",
                   (char *) TRACE_LEVEL_STR[ level ],
                   (char *) gVosTraceInfo[ module ].moduleNameStr );

      // print the formatted log message after the prefix string.
      if ((n >= 0) && (n < VOS_TRACE_BUFFER_SIZE))
      {
         vsnprintf(strBuffer + n, VOS_TRACE_BUFFER_SIZE - n, strFormat, val );

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
         wlan_log_to_user(level, (char *)strBuffer, strlen(strBuffer));
#else
         pr_err("%s\n", strBuffer);
#endif
      }
      va_end(val);
   }
}

void vos_trace_display(void)
{
   VOS_MODULE_ID moduleId;

   pr_err("     1)FATAL  2)ERROR  3)WARN  4)INFO  5)INFO_H  6)INFO_M  7)INFO_L 8)DEBUG\n");
   for (moduleId = 0; moduleId < VOS_MODULE_ID_MAX; ++moduleId)
   {
      pr_err("%2d)%s    %s        %s       %s       %s        %s         %s         %s        %s\n",
             (int)moduleId,
             gVosTraceInfo[moduleId].moduleNameStr,
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_FATAL)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_ERROR)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_WARN)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_INFO)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_INFO_HIGH)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_INFO_MED)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_INFO_LOW)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_DEBUG)) ? "X":" "
         );
   }
}

/*----------------------------------------------------------------------------

  \brief vos_trace_hex_dump() - Externally called hex dump function

  Checks the level of severity and accordingly prints the trace messages

  \param module - module identifier.   A member of the VOS_MODULE_ID
         enumeration that identifies the module issuing the trace message.

  \param level - trace level.   A member of the VOS_TRACE_LEVEL
         enumeration indicating the severity of the condition causing the
         trace message to be issued.   More severe conditions are more
         likely to be logged.

  \param data - .  The base address of the buffer to be logged.

  \param buf_len - .  The size of the buffer to be logged.

  \return  nothing

  \sa
  --------------------------------------------------------------------------*/
void vos_trace_hex_dump( VOS_MODULE_ID module, VOS_TRACE_LEVEL level,
                                void *data, int buf_len )
{
    char *buf = (char *)data;
    int i;
    for (i=0; (i+7)<buf_len; i+=8)
    {
        vos_trace_msg( module, level,
                 "%02x %02x %02x %02x %02x %02x %02x %02x \n",
                 buf[i],
                 buf[i+1],
                 buf[i+2],
                 buf[i+3],
                 buf[i+4],
                 buf[i+5],
                 buf[i+6],
                 buf[i+7]);
    }

    // Dump the bytes in the last line
    for (; i < buf_len; i++)
    {
        vos_trace_msg( module, level, "%02x ", buf[i]);
        if ((i+1) == buf_len)
            vos_trace_msg( module, level, "\n");
    }

}

#endif

/*-----------------------------------------------------------------------------
  \brief vosTraceEnable() - Enable MTRACE for specific modules whose bits are
  set in bitmask and enable is true. if enable is false it disables MTRACE for
  that module. set the bitmask according to enum value of the modules.

  this functions will be called when you issue ioctl as mentioned following
  [iwpriv wlan0 setdumplog <value> <enable>].
  <value> - Decimal number, i.e. 64 decimal value shows only SME module,
  128 decimal value shows only PE module, 192 decimal value shows PE and SME.

  \param - bitmask_of_moduleId - as explained above set bitmask according to
  enum of the modules.
  32 [dec]  = 0010 0000 [bin] <enum of HDD is 5>
  64 [dec]  = 0100 0000 [bin] <enum of SME is 6>
  128 [dec] = 1000 0000 [bin] <enum of PE is 7>
  \param - enable - can be true or false.
           True implies enabling MTRACE, false implies disabling MTRACE.
  ---------------------------------------------------------------------------*/
void vosTraceEnable(v_U32_t bitmask_of_moduleId, v_U8_t enable)
{
    int i;
    if (bitmask_of_moduleId)
    {
       for (i=0; i<VOS_MODULE_ID_MAX; i++)
       {
           if (((bitmask_of_moduleId >> i) & 1 ))
           {
             if(enable)
             {
                if (NULL != vostraceRestoreCBTable[i])
                {
                   vostraceCBTable[i] = vostraceRestoreCBTable[i];
                }
             }
             else
             {
                vostraceRestoreCBTable[i] = vostraceCBTable[i];
                vostraceCBTable[i] = NULL;
             }
           }
       }
    }

    else
    {
      if(enable)
      {
         for (i=0; i<VOS_MODULE_ID_MAX; i++)
         {
             if (NULL != vostraceRestoreCBTable[i])
             {
                vostraceCBTable[i] = vostraceRestoreCBTable[i];
             }
         }
      }
      else
      {
         for (i=0; i<VOS_MODULE_ID_MAX; i++)
         {
            vostraceRestoreCBTable[i] = vostraceCBTable[i];
            vostraceCBTable[i] = NULL;
         }
      }
    }
}

/*-----------------------------------------------------------------------------
  \brief vosTraceInit() - Initializes vos trace structures and variables.

  Called immediately after vos_preopen, so that we can start recording HDD
  events ASAP.
  ----------------------------------------------------------------------------*/
void vosTraceInit()
{
    v_U8_t i;
    gvosTraceData.head = INVALID_VOS_TRACE_ADDR;
    gvosTraceData.tail = INVALID_VOS_TRACE_ADDR;
    gvosTraceData.num = 0;
    gvosTraceData.enable = TRUE;
    gvosTraceData.dumpCount = DEFAULT_VOS_TRACE_DUMP_COUNT;
    gvosTraceData.numSinceLastDump = 0;

    for (i=0; i<VOS_MODULE_ID_MAX; i++)
    {
        vostraceCBTable[i] = NULL;
        vostraceRestoreCBTable[i] = NULL;
    }
}

/*-----------------------------------------------------------------------------
  \brief vos_trace() - puts the messages in to ring-buffer

  This function will be called from each module who wants record the messages
  in circular queue. Before calling this functions make sure you have
  registered your module with voss through vosTraceRegister function.

  \param module - enum of module, basically module id.
  \param code -
  \param session -
  \param data - actual message contents.
  ----------------------------------------------------------------------------*/
void vos_trace(v_U8_t module, v_U8_t code, v_U8_t session, v_U32_t data)
{
    tpvosTraceRecord rec = NULL;
    unsigned long flags;


    if (!gvosTraceData.enable)
    {
        return;
    }
    //If module is not registered, don't record for that module.
    if (NULL == vostraceCBTable[module])
    {
        return;
    }

    /* Aquire the lock so that only one thread at a time can fill the ring buffer */
    spin_lock_irqsave(&ltraceLock, flags);

    gvosTraceData.num++;

    if (gvosTraceData.num > MAX_VOS_TRACE_RECORDS)
    {
        gvosTraceData.num = MAX_VOS_TRACE_RECORDS;
    }

    if (INVALID_VOS_TRACE_ADDR == gvosTraceData.head)
    {
        /* first record */
        gvosTraceData.head = 0;
        gvosTraceData.tail = 0;
    }
    else
    {
        /* queue is not empty */
        v_U32_t tail = gvosTraceData.tail + 1;

        if (MAX_VOS_TRACE_RECORDS == tail)
        {
            tail = 0;
        }

        if (gvosTraceData.head == tail)
        {
            /* full */
            if (MAX_VOS_TRACE_RECORDS == ++gvosTraceData.head)
            {
                gvosTraceData.head = 0;
            }
        }

        gvosTraceData.tail = tail;
    }

    rec = &gvosTraceTbl[gvosTraceData.tail];
    rec->code = code;
    rec->session = session;
    rec->data = data;
    rec->time = vos_timer_get_system_time();
    rec->module =  module;
    gvosTraceData.numSinceLastDump ++;
    spin_unlock_irqrestore(&ltraceLock, flags);
}


/*-----------------------------------------------------------------------------
  \brief vos_trace_spin_lock_init() - Initializes the lock variable before use

  This function will be called from vos_preOpen, we will have lock available
  to use ASAP.
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_trace_spin_lock_init()
{
    spin_lock_init(&ltraceLock);

    return VOS_STATUS_SUCCESS;
}

/*-----------------------------------------------------------------------------
  \brief vosTraceRegister() - Registers the call back functions to display the
  messages in particular format mentioned in these call back functions.

  this functions should be called by interested module in their init part as
  we will be ready to register as soon as modules are up.

  \param moduleID - enum value of module
  \param vostraceCb - call back functions to display the messages in particular
  format.
  ----------------------------------------------------------------------------*/
void vosTraceRegister(VOS_MODULE_ID moduleID, tpvosTraceCb vostraceCb)
{
    vostraceCBTable[moduleID] = vostraceCb;
}

/*------------------------------------------------------------------------------
  \brief vosTraceDumpAll() - Dump data from ring buffer via call back functions
  registered with VOSS

  This function will be called up on issueing ioctl call as mentioned following
  [iwpriv wlan0 dumplog 0 0 <n> <bitmask_of_module>]

  <n> - number lines to dump starting from tail to head.

  <bitmask_of_module> - if anybody wants to know how many messages were recorded
  for particular module/s mentioned by setbit in bitmask from last <n> messages.
  it is optional, if you don't provide then it will dump everything from buffer.

  \param pMac - context of particular module
  \param code -
  \param session -
  \param count - number of lines to dump starting from tail to head
  ----------------------------------------------------------------------------*/
void vosTraceDumpAll(void *pMac, v_U8_t code, v_U8_t session,
                     v_U32_t count, v_U32_t bitmask_of_module)
{
    tvosTraceRecord pRecord;
    tANI_S32 i, tail;


    if (!gvosTraceData.enable)
    {
        VOS_TRACE( VOS_MODULE_ID_SYS,
                   VOS_TRACE_LEVEL_ERROR, "Tracing Disabled");
        return;
    }

    VOS_TRACE( VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO,
               "Total Records: %d, Head: %d, Tail: %d",
               gvosTraceData.num, gvosTraceData.head, gvosTraceData.tail);

    /* Aquire the lock so that only one thread at a time can read the ring buffer */
    spin_lock(&ltraceLock);

    if (gvosTraceData.head != INVALID_VOS_TRACE_ADDR)
    {
        i = gvosTraceData.head;
        tail = gvosTraceData.tail;

        if (count)
        {
            if (count > gvosTraceData.num)
            {
                count = gvosTraceData.num;
            }
            if (tail >= (count - 1))
            {
                i = tail - count + 1;
            }
            else if (count != MAX_VOS_TRACE_RECORDS)
            {
                i = MAX_VOS_TRACE_RECORDS - ((count - 1) - tail);
            }
        }

        pRecord = gvosTraceTbl[i];
        /* right now we are not using numSinceLastDump member but in future
           we might re-visit and use this member to track how many latest
           messages got added while we were dumping from ring buffer */
        gvosTraceData.numSinceLastDump = 0;
        spin_unlock(&ltraceLock);
        for (;;)
        {
            if ((code == 0 || (code == pRecord.code)) &&
                    (vostraceCBTable[pRecord.module] != NULL))
            {
                if (0 == bitmask_of_module)
                {
                   vostraceCBTable[pRecord.module](pMac, &pRecord, (v_U16_t)i);
                }
                else
                {
                   if (bitmask_of_module & (1 << pRecord.module))
                   {
                      vostraceCBTable[pRecord.module](pMac, &pRecord, (v_U16_t)i);
                   }
                }
            }

            if (i == tail)
            {
                break;
            }
            i += 1;

            spin_lock(&ltraceLock);
            if (MAX_VOS_TRACE_RECORDS == i)
            {
                i = 0;
                pRecord= gvosTraceTbl[0];
            }
            else
            {
                pRecord = gvosTraceTbl[i];
            }
            spin_unlock(&ltraceLock);
        }
    }
    else
    {
        spin_unlock(&ltraceLock);
    }
}
