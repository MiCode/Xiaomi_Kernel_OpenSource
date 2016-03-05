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

/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * macInitApi.c - This file has all the mac level init functions
 *                   for all the defined threads at system level.
 * Author:    Dinesh Upadhyay
 * Date:      04/23/2007
 * History:-
 * Date: 04/08/2008       Modified by: Santosh Mandiganal
 * Modification Information: Code to allocate and free the  memory for DumpTable entry.
 * --------------------------------------------------------------------------
 *
 */
/* Standard include files */
#include "cfgApi.h"         // cfgCleanup
#include "limApi.h"         // limCleanup
#include "sirTypes.h"
#include "sysDebug.h"
#include "sysEntryFunc.h"
#include "macInitApi.h"
#if defined(ANI_LOGDUMP)
#include "logDump.h"
#endif //#if defined(ANI_LOGDUMP)

#ifdef TRACE_RECORD
#include "macTrace.h"
#endif

extern tSirRetStatus halDoCfgInit(tpAniSirGlobal pMac);
extern tSirRetStatus halProcessStartEvent(tpAniSirGlobal pMac);




tSirRetStatus macReset(tpAniSirGlobal pMac, tANI_U32 rc);

tSirRetStatus macPreStart(tHalHandle hHal)
{
   tSirRetStatus status = eSIR_SUCCESS;
   tANI_BOOLEAN memAllocFailed = eANI_BOOLEAN_FALSE;
   tpAniSirGlobal pMac = (tpAniSirGlobal) hHal;
   tANI_U8 i;

   for(i=0; i<MAX_DUMP_TABLE_ENTRY; i++)
   {
      pMac->dumpTableEntry[i] = vos_mem_malloc(sizeof(tDumpModuleEntry));
      if ( NULL == pMac->dumpTableEntry[i] )
      {
         memAllocFailed = eANI_BOOLEAN_TRUE;
         break;
      }
      else
      {
         vos_mem_set(pMac->dumpTableEntry[i], sizeof(tSirMbMsg), 0);
      }
   }
   if( memAllocFailed )
   {
      while(i>0)
      {
         i--;
         vos_mem_free(pMac->dumpTableEntry[i]);
      }
      sysLog(pMac, LOGE, FL("pMac->dumpTableEntry is NULL\n"));
      status = eSIR_FAILURE;
   }

#if defined(ANI_LOGDUMP)
   //logDumpInit must be called before any module starts
   logDumpInit(pMac);
#endif //#if defined(ANI_LOGDUMP)

   return status;
}

tSirRetStatus macStart(tHalHandle hHal, void* pHalMacStartParams)
{
   tSirRetStatus status = eSIR_SUCCESS;
   tpAniSirGlobal pMac = (tpAniSirGlobal) hHal;

   if (NULL == pMac)
   {
      VOS_ASSERT(0);
      status = eSIR_FAILURE;
      return status;
   }

   pMac->gDriverType = ((tHalMacStartParameters*)pHalMacStartParams)->driverType;

   sysLog(pMac, LOG2, FL("called\n"));

   do
   {
      pMac->pResetMsg = vos_mem_malloc(sizeof(tSirMbMsg));
      if ( NULL == pMac->pResetMsg )
      {
         sysLog(pMac, LOGE, FL("pMac->pResetMsg is NULL\n"));
         status = eSIR_FAILURE;
         break;
      }
      else
      {
         vos_mem_set(pMac->pResetMsg, sizeof(tSirMbMsg), 0);
      }

      if (pMac->gDriverType != eDRIVER_TYPE_MFG)
      {
         status = peStart(pMac);
      }

   } while(0);
   pMac->sys.abort = false;

   return status;
}


/** -------------------------------------------------------------
\fn macStop
\brief this function will be called from HDD to stop MAC. This function will stop all the mac modules.
\       memory with global context will only be initialized not freed here.
\param   tHalHandle hHal
\param tHalStopType
\return tSirRetStatus
  -------------------------------------------------------------*/

tSirRetStatus macStop(tHalHandle hHal, tHalStopType stopType)
{
    tANI_U8 i;
    tpAniSirGlobal pMac = (tpAniSirGlobal) hHal;
    peStop(pMac);
    cfgCleanup( pMac );
    // need to free memory if not called in reset context.
    // in reset context this memory will be freed by HDD.
    if(false == pMac->sys.abort)
    {
        vos_mem_free(pMac->pResetMsg);
        pMac->pResetMsg = NULL;
    }
    /* Free the DumpTableEntry */
    for(i=0; i<MAX_DUMP_TABLE_ENTRY; i++)
    {
        vos_mem_free(pMac->dumpTableEntry[i]);
    }

    return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn macOpen
\brief this function will be called during init. This function is suppose to allocate all the
\       memory with the global context will be allocated here.
\param   tHalHandle pHalHandle
\param   tHddHandle hHdd
\param   tHalOpenParameters* pHalOpenParams
\return tSirRetStatus
  -------------------------------------------------------------*/

tSirRetStatus macOpen(tHalHandle *pHalHandle, tHddHandle hHdd, tMacOpenParameters *pMacOpenParms)
{
    tpAniSirGlobal pMac = NULL;

    if(pHalHandle == NULL)
        return eSIR_FAILURE;

    /*
     * Make sure this adapter is not already opened. (Compare pAdapter pointer in already
     * allocated pMac structures.)
     * If it is opened just return pointer to previously allocated pMac pointer.
     * Or should this result in error?
     */

    /* Allocate pMac */
    pMac = vos_mem_malloc(sizeof(tAniSirGlobal));
    if ( NULL == pMac )
        return eSIR_FAILURE;

    /* Initialize the pMac structure */
    vos_mem_set(pMac, sizeof(tAniSirGlobal), 0);

    /** Store the Driver type in pMac Global.*/
    //pMac->gDriverType = pMacOpenParms->driverType;

    /*
     * Set various global fields of pMac here
     * (Could be platform dependant as some variables in pMac are platform
     * dependant)
     */
    pMac->hHdd      = hHdd;
    pMac->pAdapter  = hHdd; //This line wil be removed
    *pHalHandle     = (tHalHandle)pMac;

    {
        /* Call various PE (and other layer init here) */
        if( eSIR_SUCCESS != logInit(pMac))
           return eSIR_FAILURE;

        /* Call routine to initialize CFG data structures */
        if( eSIR_SUCCESS != cfgInit(pMac) )
            return eSIR_FAILURE;

        sysInitGlobals(pMac);
    }


    return peOpen(pMac, pMacOpenParms);
}

/** -------------------------------------------------------------
\fn macClose
\brief this function will be called in shutdown sequence from HDD. All the
\       allocated memory with global context will be freed here.
\param   tpAniSirGlobal pMac
\return none
  -------------------------------------------------------------*/

tSirRetStatus macClose(tHalHandle hHal)
{

    tpAniSirGlobal pMac = (tpAniSirGlobal) hHal;

    peClose(pMac);

    /* Call routine to free-up all CFG data structures */
    cfgDeInit(pMac);

    logDeinit(pMac);

    // Finally, de-allocate the global MAC datastructure:
    vos_mem_free( pMac );

    return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn macReset
\brief this function is called to send Reset message to HDD. Then HDD will start the reset process.
\param   tpAniSirGlobal pMac
\param   tANI_U32 rc
\return    tSirRetStatus.
  -------------------------------------------------------------*/

tSirRetStatus macReset(tpAniSirGlobal pMac, tANI_U32 rc)
{
    tSirRetStatus status = eSIR_SUCCESS;
    sysLog(pMac, LOGE, FL("*************No-op. Need to call WDA reset function \n"));
    return status;
}

// ----------------------------------------------------------------------
/**
 * macSysResetReq
 *
 * FUNCTION:
 *   All MAC modules use this interface in case of an exception.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 *
 * NOTE:
 *
 * @param tpAniSirGlobal MAC parameters structure
 * @param tANI_U32 reset reason code
 * @return tANI_U16 - returs the status.
 */

void
macSysResetReq(tpAniSirGlobal pMac, tANI_U32 rc)
{
    sysLog(pMac, LOGE, FL("Reason Code = 0x%X\n"),rc);

    switch (rc)
    {
        case eSIR_STOP_BSS:
        case eSIR_SME_BSS_RESTART:
        case eSIR_RADIO_HW_SWITCH_STATUS_IS_OFF:
        case eSIR_CFB_FLAG_STUCK_EXCEPTION:
                // FIXME
                //macReset(pMac, rc);
                break;

        case eSIR_EOF_SOF_EXCEPTION:
        case eSIR_BMU_EXCEPTION:
        case eSIR_CP_EXCEPTION:
        case eSIR_LOW_PDU_EXCEPTION:
        case eSIR_USER_TRIG_RESET:
        case eSIR_AHB_HANG_EXCEPTION:
        default:
             macReset(pMac, rc);
            break;

    }
}

// -------------------------------------------------------------
/**
 * macSysResetReqFromHDD
 *
 * FUNCTION:
 *   This reset function gets invoked from the HDD to request a reset.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 *
 * NOTE:
 *
 * @param tpAniSirGlobal MAC parameters structure
 * @return tANI_U16 - returs the status.
 */

void
macSysResetReqFromHDD(void *pMac, tANI_U32 rc)
{
    macSysResetReq( (tpAniSirGlobal)pMac, rc );
}

