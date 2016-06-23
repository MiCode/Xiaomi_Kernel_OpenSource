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
  
  @file  wlan_qct_dxe.c
  
  @brief 
               
   This file contains the external API exposed by the wlan data transfer abstraction layer module.
========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when           who        what, where, why
--------    ---         ----------------------------------------------------------
08/03/10    schang      Created module.

===========================================================================*/

/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_dxe.h"
#include "wlan_qct_dxe_i.h"
#include "wlan_qct_pal_device.h"
#ifdef FEATURE_R33D
#include "wlan_qct_pal_bus.h"
#endif /* FEATURE_R33D */

/*----------------------------------------------------------------------------
 * Local Definitions
 * -------------------------------------------------------------------------*/
//#define WLANDXE_DEBUG_CH_INFO_DUMP

/* Temporary configuration defines
 * Have to find out permanent solution */
#define T_WLANDXE_MAX_DESCRIPTOR_COUNT     40
#define T_WLANDXE_MAX_FRAME_SIZE           2000
#define T_WLANDXE_TX_INT_ENABLE_FCOUNT     1
#define T_WLANDXE_MEMDUMP_BYTE_PER_LINE    16
#define T_WLANDXE_MAX_RX_PACKET_WAIT       6000
#define T_WLANDXE_SSR_TIMEOUT              5000
#define T_WLANDXE_PERIODIC_HEALTH_M_TIME   2500
#define T_WLANDXE_MAX_HW_ACCESS_WAIT       2000
#define WLANDXE_MAX_REAPED_RX_FRAMES       512

#define WLANPAL_RX_INTERRUPT_PRO_MASK      0x20
#define WLANDXE_RX_INTERRUPT_PRO_UNMASK    0x5F

/* 1msec busy wait in case CSR is not valid */
#define WLANDXE_CSR_NEXT_READ_WAIT         1000
/* CSR max retry count */
#define WLANDXE_CSR_MAX_READ_COUNT         30


/* This is temporary fot the compile
 * WDI will release official version
 * This must be removed */
#define WDI_GET_PAL_CTX()                  NULL


/*-------------------------------------------------------------------------
  *  Local Varables
  *-------------------------------------------------------------------------*/
/* This is temp, someone have to allocate for me, and must be part of global context */
static WLANDXE_CtrlBlkType    *tempDxeCtrlBlk;
static char                   *channelType[WDTS_CHANNEL_MAX] =
   {
      "TX_LOW_PRI",
      "TX_HIGH_PRI",
      "RX_LOW_PRI",
#ifndef WLANDXE_TEST_CHANNEL_ENABLE
      "RX_HIGH_PRI",
#else
      "H2H_TEST_TX",
      "H2H_TEST_RX"
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */
   };
static  wpt_packet               *rx_reaped_buf[WLANDXE_MAX_REAPED_RX_FRAMES];

/*-------------------------------------------------------------------------
  *  External Function Proto Type
  *-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------
  *  Local Function Proto Type
  *-------------------------------------------------------------------------*/
static wpt_status dxeRXFrameSingleBufferAlloc
(
   WLANDXE_CtrlBlkType      *dxeCtxt,
   WLANDXE_ChannelCBType    *channelEntry,
   WLANDXE_DescCtrlBlkType  *currentCtrlBlock
);

static wpt_status dxeNotifySmsm
(
  wpt_boolean kickDxe,
  wpt_boolean ringEmpty
);

static void dxeStartSSRTimer
(
  WLANDXE_CtrlBlkType     *dxeCtxt
);

/*-------------------------------------------------------------------------
  *  Local Function
  *-------------------------------------------------------------------------*/
/*==========================================================================
  @  Function Name 
      dxeChannelMonitor

  @  Description 

  @  Parameters
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeChannelMonitor
(
   char                    *monitorDescription,
   WLANDXE_ChannelCBType   *channelEntry,
   wpt_log_data_stall_channel_type *channelLog
)
{
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;

   if((NULL == monitorDescription) || (NULL == channelEntry))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "INVALID Input ARG");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if(channelEntry->channelType > WDTS_CHANNEL_RX_HIGH_PRI)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "INVALID Channel type");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
             "%11s : HCBO %d, HCBDP 0x%x, HCBDC 0x%x,",
             channelType[channelEntry->channelType],
             channelEntry->headCtrlBlk->ctrlBlkOrder,
             channelEntry->headCtrlBlk->linkedDescPhyAddr,
             channelEntry->headCtrlBlk->linkedDesc->descCtrl.ctrl);
   wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
             "%11s : TCBO %d, TCBDP 0x%x, TCBDC 0x%x",
             channelType[channelEntry->channelType],
             channelEntry->tailCtrlBlk->ctrlBlkOrder,
             channelEntry->tailCtrlBlk->linkedDescPhyAddr,
             channelEntry->tailCtrlBlk->linkedDesc->descCtrl.ctrl);
   wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
             "%11s : FDC %d, RDC %d, TFC %d",
             channelType[channelEntry->channelType],
             channelEntry->numFreeDesc,
             channelEntry->numRsvdDesc,
             channelEntry->numTotalFrame);

   if(channelLog)
   {
      channelLog->numDesc       = channelEntry->numDesc;
      channelLog->numFreeDesc   = channelEntry->numFreeDesc;
      channelLog->numRsvdDesc   = channelEntry->numRsvdDesc;
      channelLog->headDescOrder = channelEntry->headCtrlBlk->ctrlBlkOrder;
      channelLog->tailDescOrder = channelEntry->tailCtrlBlk->ctrlBlkOrder;
   }

   return status;
}

#ifdef WLANDXE_DEBUG_MEMORY_DUMP
/*==========================================================================
  @  Function Name 
      dxeMemoryDump

  @  Description 

  @  Parameters
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeMemoryDump
(
   wpt_uint8    *dumpPointer,
   wpt_uint32    dumpSize,
   char         *dumpTarget
)
{
   wpt_status                status      = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                numBytes    = 0;
   wpt_uint32                idx;

   if((NULL == dumpPointer) ||
      (NULL == dumpTarget))
   {
      return status;
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Location 0x%x, Size %d", dumpTarget, dumpPointer, dumpSize);

   numBytes = dumpSize % T_WLANDXE_MEMDUMP_BYTE_PER_LINE;
   for(idx = 0; idx < dumpSize; idx++)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
               "0x%2x ", dumpPointer[idx]);
      if(0 == ((idx + 1) % T_WLANDXE_MEMDUMP_BYTE_PER_LINE))
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW, "\n");
      }
   }
   if(0 != numBytes)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW, "\n");
   }
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

   return status;
}
#endif /* WLANDXE_DEBUG_MEMORY_DUMP */

/*==========================================================================
  @  Function Name 
      dxeDescriptorDump

  @  Description 

  @  Parameters
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
wpt_status dxeDescriptorDump
(
   WLANDXE_ChannelCBType   *channelEntry,
   WLANDXE_DescType        *targetDesc,
   wpt_uint32               fragmentOrder
)
{
   wpt_status                status      = eWLAN_PAL_STATUS_SUCCESS;


   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "Descriptor Dump for channel %s, %d / %d fragment",
                   channelType[channelEntry->channelType],
                   fragmentOrder + 1,
                   channelEntry->numFragmentCurrentChain);

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "CTRL WORD 0x%x, TransferSize %d",
                   WLANDXE_U32_SWAP_ENDIAN(targetDesc->descCtrl.ctrl),
            WLANDXE_U32_SWAP_ENDIAN(targetDesc->xfrSize));
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "SRC ADD 0x%x, DST ADD 0x%x, NEXT DESC 0x%x",
                   WLANDXE_U32_SWAP_ENDIAN(targetDesc->dxedesc.dxe_short_desc.srcMemAddrL),
                   WLANDXE_U32_SWAP_ENDIAN(targetDesc->dxedesc.dxe_short_desc.dstMemAddrL),
                   WLANDXE_U32_SWAP_ENDIAN(targetDesc->dxedesc.dxe_short_desc.phyNextL));
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

   return status;
}

/*==========================================================================
  @  Function Name 
      dxeChannelRegisterDump

  @  Description 

  @  Parameters
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
wpt_status dxeChannelRegisterDump
(
   WLANDXE_ChannelCBType   *channelEntry,
   char                    *dumpTarget,
   wpt_log_data_stall_channel_type *channelLog
)
{
   wpt_status   status      = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32   chStatusReg, chControlReg, chDescReg, chLDescReg;

   /* Whatever RIVA power condition try to wakeup RIVA through SMSM
    * This will not simply wakeup RIVA
    * Just incase TX not wanted stuck, Trigger TX again */
   dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
   dxeNotifySmsm(eWLAN_PAL_TRUE, eWLAN_PAL_FALSE);
   wpalSleep(10);

   if(channelEntry->channelType > WDTS_CHANNEL_RX_HIGH_PRI)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "INVALID Channel type");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   wpalReadRegister(channelEntry->channelRegister.chDXEDesclRegAddr, &chDescReg);
   wpalReadRegister(channelEntry->channelRegister.chDXELstDesclRegAddr, &chLDescReg);
   wpalReadRegister(channelEntry->channelRegister.chDXECtrlRegAddr, &chControlReg);
   wpalReadRegister(channelEntry->channelRegister.chDXEStatusRegAddr, &chStatusReg);

   wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
             "%11s : CCR 0x%x, CSR 0x%x, CDR 0x%x, CLDR 0x%x",
             channelType[channelEntry->channelType],
             chControlReg, chStatusReg, chDescReg, chLDescReg);

   if(channelLog)
   {
      channelLog->ctrlRegVal = chControlReg;
      channelLog->statRegVal = chStatusReg;
   }

   return status;
}

/*==========================================================================
  @  Function Name 
      dxeChannelAllDescDump

  @  Description 
      Dump all DXE descriptors within assigned channe;

  @  Parameters
      WLANDXE_ChannelCBType   *channelEntry

  @  Return
      NONE

===========================================================================*/
void dxeChannelAllDescDump
(
   WLANDXE_ChannelCBType   *channelEntry,
   WDTS_ChannelType         channel,
   wpt_log_data_stall_channel_type *channelLog
)
{
   wpt_uint32               channelLoop;
   WLANDXE_DescCtrlBlkType *targetCtrlBlk;
   wpt_uint32               previousCtrlValue = 0;
   wpt_uint32               previousCtrlValid = 0;
   wpt_uint32               currentCtrlValid = 0;
   wpt_uint32               valDescCount = 0;
   wpt_uint32               invalDescCount = 0;

   targetCtrlBlk = channelEntry->headCtrlBlk;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "%11s : %d descriptor chains, head desc ctrl 0x%x",
            channelType[channelEntry->channelType],
            channelEntry->numDesc,
            targetCtrlBlk->linkedDesc->descCtrl.ctrl);
   previousCtrlValue = targetCtrlBlk->linkedDesc->descCtrl.ctrl;

   if((WDTS_CHANNEL_RX_LOW_PRI == channel) ||
      (WDTS_CHANNEL_RX_HIGH_PRI == channel))
   {
      for(channelLoop = 0; channelLoop < channelEntry->numDesc; channelLoop++)
      {
         if(previousCtrlValue != targetCtrlBlk->linkedDesc->descCtrl.ctrl)
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                     "%5d : 0x%x", targetCtrlBlk->ctrlBlkOrder,
                     targetCtrlBlk->linkedDesc->descCtrl.ctrl);
         }
         if(targetCtrlBlk->linkedDesc->descCtrl.ctrl & WLANDXE_DESC_CTRL_VALID)
         {
            valDescCount++;
         }
         else
         {
            invalDescCount++;
         }
         previousCtrlValue = targetCtrlBlk->linkedDesc->descCtrl.ctrl;
         targetCtrlBlk = (WLANDXE_DescCtrlBlkType *)targetCtrlBlk->nextCtrlBlk;
      }
   }
   else
   {
      /* Head Descriptor is valid or not */
      previousCtrlValid = targetCtrlBlk->linkedDesc->descCtrl.ctrl & WLANDXE_DESC_CTRL_VALID;
      targetCtrlBlk = (WLANDXE_DescCtrlBlkType *)targetCtrlBlk->nextCtrlBlk;
      for(channelLoop = 0; channelLoop < channelEntry->numDesc; channelLoop++)
      {
         currentCtrlValid = targetCtrlBlk->linkedDesc->descCtrl.ctrl & WLANDXE_DESC_CTRL_VALID;
         if(currentCtrlValid)
         {
            valDescCount++;
         }
         else
         {
            invalDescCount++;
         }
         if(currentCtrlValid != previousCtrlValid)
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                     "%5d : 0x%x", targetCtrlBlk->ctrlBlkOrder,
                     targetCtrlBlk->linkedDesc->descCtrl.ctrl);
         }
         previousCtrlValid = currentCtrlValid;
         targetCtrlBlk = (WLANDXE_DescCtrlBlkType *)targetCtrlBlk->nextCtrlBlk;
      }
   }

   if(channelLog)
   {
      channelLog->numValDesc   = valDescCount;
      channelLog->numInvalDesc = invalDescCount;
   }

   return;
}

/*==========================================================================
  @  Function Name
      dxeErrChannelDebug

  @  Description
      Dump channel information for which Error interrupt has occured

  @  Parameters
      WLANDXE_ChannelCBType  *channelCb

  @  Return
      NONE

===========================================================================*/
void dxeErrChannelDebug
(
    WLANDXE_ChannelCBType    *channelCb,
    wpt_uint32                chStatusReg
)
{
   wpt_log_data_stall_channel_type channelLog;
   wpt_uint32 chLDescReg, channelLoop;
   WLANDXE_DescCtrlBlkType *targetCtrlBlk;

   dxeChannelMonitor("INT_ERR", channelCb, &channelLog);
   dxeDescriptorDump(channelCb, channelCb->headCtrlBlk->linkedDesc, 0);
   dxeChannelRegisterDump(channelCb, "INT_ERR", &channelLog);
   dxeChannelAllDescDump(channelCb, channelCb->channelType, &channelLog);
   wpalMemoryCopy(channelLog.channelName,
                  "INT_ERR",
                  WPT_TRPT_CHANNEL_NAME);
   wpalPacketStallUpdateInfo(NULL, NULL, &channelLog, channelCb->channelType);
#ifdef FEATURE_WLAN_DIAG_SUPPORT
   wpalPacketStallDumpLog();
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
   switch ((chStatusReg & WLANDXE_CH_STAT_ERR_CODE_MASK) >>
            WLANDXE_CH_STAT_ERR_CODE_OFFSET)
   {

      case WLANDXE_ERROR_PRG_INV_B2H_SRC_QID:
      case WLANDXE_ERROR_PRG_INV_B2H_DST_QID:
      case WLANDXE_ERROR_PRG_INV_B2H_SRC_IDX:
      case WLANDXE_ERROR_PRG_INV_H2B_SRC_QID:
      case WLANDXE_ERROR_PRG_INV_H2B_DST_QID:
      case WLANDXE_ERROR_PRG_INV_H2B_DST_IDX:
      {
         dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
         dxeNotifySmsm(eWLAN_PAL_TRUE, eWLAN_PAL_FALSE);
         wpalSleep(10);

         if(channelCb->channelType > WDTS_CHANNEL_RX_HIGH_PRI)
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                     "%s: Invalid Channel", __func__);
            break;
         }

         wpalReadRegister(channelCb->channelRegister.chDXELstDesclRegAddr, &chLDescReg);

         targetCtrlBlk = channelCb->headCtrlBlk;

         for(channelLoop = 0; channelLoop < channelCb->numDesc; channelLoop++)
         {
            if (targetCtrlBlk->linkedDescPhyAddr == chLDescReg)
            {
                HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                         "%11s :CHx_DESCL: desc ctrl 0x%x, src 0x%x, dst 0x%x, next 0x%x",
                         channelType[channelCb->channelType],
                         targetCtrlBlk->linkedDesc->descCtrl.ctrl,
                         targetCtrlBlk->linkedDesc->dxedesc.dxe_short_desc.srcMemAddrL,
                         targetCtrlBlk->linkedDesc->dxedesc.dxe_short_desc.dstMemAddrL,
                         targetCtrlBlk->linkedDesc->dxedesc.dxe_short_desc.phyNextL);

                targetCtrlBlk = (WLANDXE_DescCtrlBlkType *)targetCtrlBlk->nextCtrlBlk;

                HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                         "%11s :Next Desc: desc ctrl 0x%x, src 0x%x, dst 0x%x, next 0x%x",
                         channelType[channelCb->channelType],
                         targetCtrlBlk->linkedDesc->descCtrl.ctrl,
                         targetCtrlBlk->linkedDesc->dxedesc.dxe_short_desc.srcMemAddrL,
                         targetCtrlBlk->linkedDesc->dxedesc.dxe_short_desc.dstMemAddrL,
                         targetCtrlBlk->linkedDesc->dxedesc.dxe_short_desc.phyNextL);
                break;
            }
            targetCtrlBlk = (WLANDXE_DescCtrlBlkType *)targetCtrlBlk->nextCtrlBlk;
         }
         break;
      }
      default:
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                  "%s: No Debug Inormation", __func__);
         break;
      }

   }
   wpalFwDumpReq(17, 0, 0, 0, 0);
}
/*==========================================================================
  @  Function Name
      dxeTxThreadChannelDebugHandler

  @  Description
      Dump TX channel information

  @  Parameters
      Wwpt_msg               *msgPtr

  @  Return
      NONE

===========================================================================*/
void dxeTxThreadChannelDebugHandler
(
    wpt_msg               *msgPtr
)
{
   wpt_uint8                channelLoop;
   wpt_log_data_stall_channel_type channelLog;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Whatever RIVA power condition try to wakeup RIVA through SMSM
    * This will not simply wakeup RIVA
    * Just incase TX not wanted stuck, Trigger TX again */
   for(channelLoop = 0; channelLoop < WDTS_CHANNEL_RX_LOW_PRI; channelLoop++)
   {
      dxeChannelMonitor("******** Get Descriptor Snapshot ",
                        &tempDxeCtrlBlk->dxeChannel[channelLoop],
                        &channelLog);
      dxeChannelRegisterDump(&tempDxeCtrlBlk->dxeChannel[channelLoop],
                             "Abnormal successive empty interrupt",
                             &channelLog);
      dxeChannelAllDescDump(&tempDxeCtrlBlk->dxeChannel[channelLoop],
                            channelLoop,
                            &channelLog);

      wpalMemoryCopy(channelLog.channelName,
                     channelType[channelLoop],
                     WPT_TRPT_CHANNEL_NAME);
      wpalPacketStallUpdateInfo(NULL, NULL, &channelLog, channelLoop);
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "================== DXE Dump End ======================");
   wpalMemoryFree(msgPtr);

#ifdef FEATURE_WLAN_DIAG_SUPPORT
   wpalPacketStallDumpLog();
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return;
}

/*==========================================================================
  @  Function Name
      dxeRxThreadChannelDebugHandler

  @  Description
      Dump RX channel information

  @  Parameters
      Wwpt_msg               *msgPtr

  @  Return
      NONE

===========================================================================*/
void dxeRxThreadChannelDebugHandler
(
    wpt_msg               *msgPtr
)
{
   wpt_status               status = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint8                channelLoop;
   wpt_log_data_stall_channel_type channelLog;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Whatever RIVA power condition try to wakeup RIVA through SMSM
    * This will not simply wakeup RIVA
    * Just incase TX not wanted stuck, Trigger TX again */
   for(channelLoop = WDTS_CHANNEL_RX_LOW_PRI; channelLoop < WDTS_CHANNEL_MAX; channelLoop++)
   {
      dxeChannelMonitor("******** Get Descriptor Snapshot ",
                        &tempDxeCtrlBlk->dxeChannel[channelLoop],
                        &channelLog);
      dxeChannelRegisterDump(&tempDxeCtrlBlk->dxeChannel[channelLoop],
                             "Abnormal successive empty interrupt",
                             &channelLog);
      dxeChannelAllDescDump(&tempDxeCtrlBlk->dxeChannel[channelLoop],
                            channelLoop, &channelLog);

      wpalMemoryCopy(channelLog.channelName,
                     channelType[channelLoop],
                     WPT_TRPT_CHANNEL_NAME);
      wpalPacketStallUpdateInfo(NULL, NULL, &channelLog, channelLoop);

   }

   /* Now serialise the message through Tx thread also to make sure
    * no register access when RIVA is in powersave */
   /*Use the same message pointer just change the call back function */
   msgPtr->callback = dxeTxThreadChannelDebugHandler;
   status = wpalPostTxMsg(WDI_GET_PAL_CTX(),
                          msgPtr);
   if ( eWLAN_PAL_STATUS_SUCCESS != status )
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Tx thread state dump req serialize fail status=%d",
               status);
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return;
}

/*==========================================================================
  @  Function Name 
      dxeRXHealthMonitor

  @  Description 
      Monitoring RX channel healthy stataus
      If detect any problem, try to recover

  @  Parameters
      healthMonitorMsg    MSG pointer.
                          will have low resource TX channel context

  @  Return
      NONE

===========================================================================*/
void dxeRXHealthMonitor
(
   wpt_msg         *healthMonitorMsg
)
{
   WLANDXE_ChannelCBType    *channelCtrlBlk;
   WLANDXE_ChannelCBType    *testCHCtrlBlk;
   wpt_uint32                regValue;
   wpt_uint32                chStatusReg, chControlReg, chDescReg, chLDescReg;
   wpt_uint32                hwWakeLoop, chLoop;

   if(NULL == healthMonitorMsg)
   {
      return;
   }

   /* Make wake up HW */
   dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
   dxeNotifySmsm(eWLAN_PAL_TRUE, eWLAN_PAL_FALSE);
   dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);

   for(hwWakeLoop = 0; hwWakeLoop < T_WLANDXE_MAX_HW_ACCESS_WAIT; hwWakeLoop++)
   {
      wpalReadRegister(WLANDXE_BMU_AVAILABLE_BD_PDU, &regValue);
      if(0 != regValue)
      {
         break;
      }
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
            "Scheduled RX, num free BD/PDU %d, loop Count %d",
            regValue, hwWakeLoop);

   for(chLoop = WDTS_CHANNEL_RX_LOW_PRI; chLoop < WDTS_CHANNEL_MAX; chLoop++)
   {
      testCHCtrlBlk = &tempDxeCtrlBlk->dxeChannel[chLoop];
      wpalReadRegister(testCHCtrlBlk->channelRegister.chDXECtrlRegAddr, &chControlReg);
      wpalReadRegister(testCHCtrlBlk->channelRegister.chDXEStatusRegAddr, &chStatusReg);
      wpalReadRegister(testCHCtrlBlk->channelRegister.chDXEDesclRegAddr, &chDescReg);
      wpalReadRegister(testCHCtrlBlk->channelRegister.chDXELstDesclRegAddr, &chLDescReg);

      wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,
                "%11s : CCR 0x%x, CSR 0x%x, CDR 0x%x, CLDR 0x%x, HCBO %d, HCBDP 0x%x, HCBDC 0x%x, TCBO %d,TCBDP 0x%x, TCBDC 0x%x",
                channelType[chLoop],
                chControlReg, chStatusReg, chDescReg, chLDescReg,
                testCHCtrlBlk->headCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr,
                testCHCtrlBlk->headCtrlBlk->linkedDesc->descCtrl.ctrl,
                testCHCtrlBlk->tailCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr,
                testCHCtrlBlk->tailCtrlBlk->linkedDesc->descCtrl.ctrl);

      if((chControlReg & WLANDXE_DESC_CTRL_VALID) && 
         (chLDescReg != testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr))
      {
         wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                   "%11s : CCR 0x%x, CSR 0x%x, CDR 0x%x, CLDR 0x%x, "
                   "HCBO %d, HCBDP 0x%x, HCBDC 0x%x, TCBO %d,TCBDP 0x%x, TCBDC 0x%x",
                   channelType[chLoop],
                   chControlReg, chStatusReg, chDescReg, chLDescReg,
                   testCHCtrlBlk->headCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr,
                   testCHCtrlBlk->headCtrlBlk->linkedDesc->descCtrl.ctrl,
                   testCHCtrlBlk->tailCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr,
                   testCHCtrlBlk->tailCtrlBlk->linkedDesc->descCtrl.ctrl);
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%11s : RX CH EN Descriptor Async, resync it", channelType[chLoop]);
         wpalWriteRegister(testCHCtrlBlk->channelRegister.chDXELstDesclRegAddr,
                           testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr);
      }
      else if(!(chControlReg & WLANDXE_DESC_CTRL_VALID) && 
               (chDescReg != testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr))
      {
         wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                   "%11s : CCR 0x%x, CSR 0x%x, CDR 0x%x, CLDR 0x%x, "
                   "HCBO %d, HCBDP 0x%x, HCBDC 0x%x, TCBO %d,TCBDP 0x%x, TCBDC 0x%x",
                   channelType[chLoop],
                   chControlReg, chStatusReg, chDescReg, chLDescReg,
                   testCHCtrlBlk->headCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr,
                   testCHCtrlBlk->headCtrlBlk->linkedDesc->descCtrl.ctrl,
                   testCHCtrlBlk->tailCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr,
                   testCHCtrlBlk->tailCtrlBlk->linkedDesc->descCtrl.ctrl);
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%11s : RX CH DIS Descriptor Async, resync it", channelType[chLoop]);
         wpalWriteRegister(testCHCtrlBlk->channelRegister.chDXEDesclRegAddr,
                           testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr);
      }
   }

   channelCtrlBlk = (WLANDXE_ChannelCBType *)healthMonitorMsg->pContext;
   if(channelCtrlBlk->hitLowResource)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "%11s : Still Low Resource, kick DXE TX and restart timer",
               channelType[channelCtrlBlk->channelType]);
      /* Still Low Resource, Kick DXE again and start timer again */
      wpalTimerStart(&channelCtrlBlk->healthMonitorTimer,
                     T_WLANDXE_PERIODIC_HEALTH_M_TIME);
   }
   else
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "%11s : Out from Low resource condition, do nothing",
               channelType[channelCtrlBlk->channelType]);
      /* Recovered from low resource condition
       * Not need to do anything */
   }

   return;
}

/*==========================================================================
  @  Function Name 
      dxeTXHealthMonitor

  @  Description 
      Monitoring TX channel healthy stataus
      If detect any problem, try to recover

  @  Parameters
      healthMonitorMsg    MSG pointer.
                          will have low resource TX channel context

  @  Return
      NONE

===========================================================================*/
void dxeTXHealthMonitor
(
   wpt_msg         *healthMonitorMsg
)
{
   WLANDXE_ChannelCBType    *channelCtrlBlk;
   WLANDXE_ChannelCBType    *testCHCtrlBlk;
   wpt_uint32                regValue;
   wpt_uint32                chStatusReg, chControlReg, chDescReg, chLDescReg;
   wpt_uint32                hwWakeLoop, chLoop;
   wpt_status                status  = eWLAN_PAL_STATUS_SUCCESS;

   if(NULL == healthMonitorMsg)
   {
      return;
   }

   /* First of all kick TX channel
    * This will fix if there is any problem with SMSM state */
   dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
   dxeNotifySmsm(eWLAN_PAL_TRUE, eWLAN_PAL_FALSE);
   dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);

   /* Wait till RIVA up */
   for(hwWakeLoop = 0; hwWakeLoop < T_WLANDXE_MAX_HW_ACCESS_WAIT; hwWakeLoop++)
   {
      wpalReadRegister(WLANDXE_BMU_AVAILABLE_BD_PDU, &regValue);
      if(0 != regValue)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "num free BD/PDU %d, loop Count %d",
                  regValue, hwWakeLoop);
         break;
      }
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
            "Scheduled TX, num free BD/PDU %d, loop Count %d",
            regValue, hwWakeLoop);

   for(chLoop = 0; chLoop < WDTS_CHANNEL_RX_LOW_PRI; chLoop++)
   {
      testCHCtrlBlk = &tempDxeCtrlBlk->dxeChannel[chLoop];
      wpalReadRegister(testCHCtrlBlk->channelRegister.chDXECtrlRegAddr, &chControlReg);
      wpalReadRegister(testCHCtrlBlk->channelRegister.chDXEStatusRegAddr, &chStatusReg);
      wpalReadRegister(testCHCtrlBlk->channelRegister.chDXEDesclRegAddr, &chDescReg);
      wpalReadRegister(testCHCtrlBlk->channelRegister.chDXELstDesclRegAddr, &chLDescReg);

      wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,
                "%11s : CCR 0x%x, CSR 0x%x, CDR 0x%x, CLDR 0x%x, HCBO %d, HCBDP 0x%x, HCBDC 0x%x, TCBO %d,TCBDP 0x%x, TCBDC 0x%x",
                channelType[chLoop],
                chControlReg, chStatusReg, chDescReg, chLDescReg,
                testCHCtrlBlk->headCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr,
                testCHCtrlBlk->headCtrlBlk->linkedDesc->descCtrl.ctrl,
                testCHCtrlBlk->tailCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr,
                testCHCtrlBlk->tailCtrlBlk->linkedDesc->descCtrl.ctrl);

      if((chControlReg & WLANDXE_DESC_CTRL_VALID) && 
         (chLDescReg != testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr))
      {
         wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                   "%11s : CCR 0x%x, CSR 0x%x, CDR 0x%x, CLDR 0x%x, "
                   "HCBO %d, HCBDP 0x%x, HCBDC 0x%x, TCBO %d,TCBDP 0x%x, TCBDC 0x%x",
                   channelType[chLoop],
                   chControlReg, chStatusReg, chDescReg, chLDescReg,
                   testCHCtrlBlk->headCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr,
                   testCHCtrlBlk->headCtrlBlk->linkedDesc->descCtrl.ctrl,
                   testCHCtrlBlk->tailCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr,
                   testCHCtrlBlk->tailCtrlBlk->linkedDesc->descCtrl.ctrl);
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%11s : TX CH EN Descriptor Async, resync it", channelType[chLoop]);
         wpalWriteRegister(testCHCtrlBlk->channelRegister.chDXELstDesclRegAddr,
                           testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr);
      }
      else if(!(chControlReg & WLANDXE_DESC_CTRL_VALID) && 
               (chDescReg != testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr))
      {
         wpalTrace(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                   "%11s : CCR 0x%x, CSR 0x%x, CDR 0x%x, CLDR 0x%x, "
                   "HCBO %d, HCBDP 0x%x, HCBDC 0x%x, TCBO %d,TCBDP 0x%x, TCBDC 0x%x",
                   channelType[chLoop],
                   chControlReg, chStatusReg, chDescReg, chLDescReg,
                   testCHCtrlBlk->headCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->headCtrlBlk->linkedDescPhyAddr,
                   testCHCtrlBlk->headCtrlBlk->linkedDesc->descCtrl.ctrl,
                   testCHCtrlBlk->tailCtrlBlk->ctrlBlkOrder, testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr,
                   testCHCtrlBlk->tailCtrlBlk->linkedDesc->descCtrl.ctrl);
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%11s : TX CH DIS Descriptor Async, resync it", channelType[chLoop]);
         wpalWriteRegister(testCHCtrlBlk->channelRegister.chDXEDesclRegAddr,
                           testCHCtrlBlk->tailCtrlBlk->linkedDescPhyAddr);
      }
   }

   /* TX channel test done, test RX channels */
   channelCtrlBlk = (WLANDXE_ChannelCBType *)healthMonitorMsg->pContext;
   channelCtrlBlk->healthMonitorMsg->callback = dxeRXHealthMonitor;
   status = wpalPostRxMsg(WDI_GET_PAL_CTX(),
                          channelCtrlBlk->healthMonitorMsg);
   if (eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "TX Low resource Kick DXE MSG Serialize fail status=%d",
               status);
   }

   return;
}

/*==========================================================================
  @  Function Name 
      dxeHealthMonitorTimeout

  @  Description 
      Health Monitor timer started when TX channel low resource condition
      And if reciovered from low resource condition, timer would not fired
      Timer fired means during certain time, TX CH could not be recovered

  @  Parameters
      channelCtxt   Low resource condition happen Channel context

  @  Return
      NONE

===========================================================================*/
void dxeHealthMonitorTimeout
(
   void         *channelCtxt
)
{
   WLANDXE_ChannelCBType    *channelCtrlBlk;
   wpt_status                status  = eWLAN_PAL_STATUS_SUCCESS;

   if(NULL == channelCtxt)
   {
      return;
   }

   /* Timeout Fired, DXE TX should kick on TX thread
    * Serailize to TX Thread */
   channelCtrlBlk = (WLANDXE_ChannelCBType *)channelCtxt;
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,
            "%11s : Health Monitor timer expired",
            channelType[channelCtrlBlk->channelType]);

   channelCtrlBlk->healthMonitorMsg->callback = dxeTXHealthMonitor;
   status = wpalPostTxMsg(WDI_GET_PAL_CTX(),
                          channelCtrlBlk->healthMonitorMsg);
   if (eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "TX Low resource Kick DXE MSG Serialize fail status=%d",
               status);
   }

   return;
}

/*==========================================================================
  @  Function Name 
      dxeCtrlBlkAlloc

  @  Description 
      Allocate DXE Control block
      DXE control block will used by Host DXE driver only, internal structure
      Will make ring linked list

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeCtrlBlkAlloc
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk,
   WLANDXE_ChannelCBType   *channelEntry
)
{
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;
   unsigned int              idx, fIdx;
   WLANDXE_DescCtrlBlkType  *currentCtrlBlk = NULL;
   WLANDXE_DescCtrlBlkType  *freeCtrlBlk = NULL;
   WLANDXE_DescCtrlBlkType  *prevCtrlBlk = NULL;
   WLANDXE_DescCtrlBlkType  *nextCtrlBlk = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity check */
   if((NULL == dxeCtrlBlk) || (NULL == channelEntry))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeCtrlBlkAlloc Channel Entry is not valid");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   /* Allocate pre asigned number of control blocks */
   for(idx = 0; idx < channelEntry->numDesc; idx++)
   {
      currentCtrlBlk = (WLANDXE_DescCtrlBlkType *)wpalMemoryAllocate(sizeof(WLANDXE_DescCtrlBlkType));
      if(NULL == currentCtrlBlk)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeCtrlBlkOpen MemAlloc Fail for channel %d",
                  channelEntry->channelType);
         freeCtrlBlk = channelEntry->headCtrlBlk;
         for(fIdx = 0; fIdx < idx; fIdx++)
         {
            if(NULL == freeCtrlBlk)
            {
               break;
            }

            nextCtrlBlk = freeCtrlBlk->nextCtrlBlk;
            wpalMemoryFree((void *)freeCtrlBlk);
            freeCtrlBlk = nextCtrlBlk;
         }
         return eWLAN_PAL_STATUS_E_FAULT;
      }

      memset((wpt_uint8 *)currentCtrlBlk, 0, sizeof(WLANDXE_DescCtrlBlkType));
      /* Initialize common elements first */
      currentCtrlBlk->xfrFrame          = NULL;
      currentCtrlBlk->linkedDesc        = NULL;
      currentCtrlBlk->linkedDescPhyAddr = 0;
      currentCtrlBlk->ctrlBlkOrder      = idx;

      /* This is the first control block allocated
       * Next Control block is not allocated yet
       * head and tail must be first control block */
      if(0 == idx)
      {
         currentCtrlBlk->nextCtrlBlk = NULL;
         channelEntry->headCtrlBlk   = currentCtrlBlk;
         channelEntry->tailCtrlBlk   = currentCtrlBlk;
      }
      /* This is not first, not last control block
       * previous control block may has next linked block */
      else if((0 < idx) && (idx < (channelEntry->numDesc - 1)))
      {
         prevCtrlBlk->nextCtrlBlk = currentCtrlBlk;
      }
      /* This is last control blocl
       * next control block for the last control block is head, first control block
       * then whole linked list made RING */
      else if((channelEntry->numDesc - 1) == idx)
      {
         prevCtrlBlk->nextCtrlBlk    = currentCtrlBlk;
         currentCtrlBlk->nextCtrlBlk = channelEntry->headCtrlBlk;
      }
      else
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeCtrlBlkOpen Invalid Ctrl Blk location %d",
                  channelEntry->channelType);
         wpalMemoryFree(currentCtrlBlk);
         return eWLAN_PAL_STATUS_E_FAULT;
      }

      prevCtrlBlk = currentCtrlBlk;
      channelEntry->numFreeDesc++;
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,"%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      dxeDescLinkAlloc

  @  Description 
      Allocate DXE descriptor
      DXE descriptor will be shared by DXE host driver and RIVA DXE engine
      Will make RING linked list
      Will be linked with Descriptor control block one by one

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block
  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeDescAllocAndLink
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk,
   WLANDXE_ChannelCBType   *channelEntry
)
{
   wpt_status                status      = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_DescType         *currentDesc = NULL;
   WLANDXE_DescType         *prevDesc    = NULL;
   WLANDXE_DescCtrlBlkType  *currentCtrlBlk = NULL;
   unsigned int              idx;
   void                     *physAddressAlloc = NULL;
   wpt_uint32                physAddress;
#ifdef WLANDXE_TEST_CHANNEL_ENABLE
   WLANDXE_ChannelCBType    *testTXChannelCB = &dxeCtrlBlk->dxeChannel[WDTS_CHANNEL_H2H_TEST_TX];
   WLANDXE_DescCtrlBlkType  *currDescCtrlBlk = testTXChannelCB->headCtrlBlk;
#endif /* WLANDXE_TEST_CHANNEL_ENABLE*/

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity Check */
   if((NULL == dxeCtrlBlk) || (NULL == channelEntry))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeDescLinkAlloc Channel Entry is not valid");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   currentCtrlBlk = channelEntry->headCtrlBlk;

#if !(defined(FEATURE_R33D) || defined(WLANDXE_TEST_CHANNEL_ENABLE))
   /* allocate all DXE descriptors for this channel in one chunk */
   channelEntry->descriptorAllocation = (WLANDXE_DescType *)
      wpalDmaMemoryAllocate(sizeof(WLANDXE_DescType)*channelEntry->numDesc,
                            &physAddressAlloc);
   physAddress = (wpt_uint32) (uintptr_t)(physAddressAlloc);
   if(NULL == channelEntry->descriptorAllocation)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeDescLinkAlloc Descriptor Alloc Fail");
      return eWLAN_PAL_STATUS_E_RESOURCES;
   }
   currentDesc = channelEntry->descriptorAllocation;
#endif

   /* Allocate pre asigned number of descriptor */
   for(idx = 0; idx < channelEntry->numDesc; idx++)
   {
#ifndef FEATURE_R33D
#ifndef WLANDXE_TEST_CHANNEL_ENABLE
      // descriptors were allocated in a chunk -- use the current one
      memset((wpt_uint8 *)currentDesc, 0, sizeof(WLANDXE_DescType));
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
               "Allocated Descriptor VA %p, PA %p", currentDesc, physAddressAlloc);
#else
      if(WDTS_CHANNEL_H2H_TEST_RX != channelEntry->channelType)
      {
         // allocate a descriptor
         currentDesc = (WLANDXE_DescType *)wpalDmaMemoryAllocate(sizeof(WLANDXE_DescType),
                                                                 &physAddressAlloc);
         memset((wpt_uint8 *)currentDesc, 0, sizeof(WLANDXE_DescType));
         physAddress = (wpt_uint32) (uintptr_t)(physAddressAlloc);
      }
      else
      {
         currentDesc     = currDescCtrlBlk->linkedDesc;
         physAddress     = currDescCtrlBlk->linkedDescPhyAddr;
         currDescCtrlBlk = (WLANDXE_DescCtrlBlkType *)currDescCtrlBlk->nextCtrlBlk;
      }
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */
#else
#ifndef WLANDXE_TEST_CHANNEL_ENABLE
      currentDesc = (WLANDXE_DescType *)wpalAcpuDdrDxeDescMemoryAllocate(&physAddressAlloc);
      memset((wpt_uint8 *)currentDesc, 0, sizeof(WLANDXE_DescType));
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
               "Allocated Descriptor VA %p, PA %p", currentDesc, physAddressAlloc);
      physAddress = (wpt_uint32) (uintptr_t)(physAddressAlloc);
#else
      if(WDTS_CHANNEL_H2H_TEST_RX != channelEntry->channelType)
      {
         currentDesc = (WLANDXE_DescType *)wpalAcpuDdrDxeDescMemoryAllocate(&physAddressAlloc);
         memset((wpt_uint8 *)currentDesc, 0, sizeof(WLANDXE_DescType));
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
                  "Allocated Descriptor VA %p, PA %p", currentDesc, physAddressAlloc);
         physAddress = (wpt_uint32) (uintptr_t)(physAddressAlloc);
      }
      else
      {
         currentDesc     = currDescCtrlBlk->linkedDesc;
         physAddress     = currDescCtrlBlk->linkedDescPhyAddr;
         currDescCtrlBlk = (WLANDXE_DescCtrlBlkType *)currDescCtrlBlk->nextCtrlBlk;
      }
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */
#endif /* FEATURE_R33D */
      if(NULL == currentDesc)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeDescLinkAlloc MemAlloc Fail for channel %d",
                  channelEntry->channelType);
         return eWLAN_PAL_STATUS_E_FAULT;
      }

      currentCtrlBlk->linkedDesc        = currentDesc;
      currentCtrlBlk->linkedDescPhyAddr = physAddress;
      /* First descriptor, next none
       * descriptor bottom location is first descriptor address */
      if(0 == idx)
      {
         currentDesc->dxedesc.dxe_short_desc.phyNextL = 0;
         channelEntry->DescBottomLoc                  = currentDesc;
         channelEntry->descBottomLocPhyAddr           = physAddress;
      }
      /* Not first, not last descriptor
       * may make link for previous descriptor with current descriptor
       * ENDIAN SWAP needed ????? */
      else if((0 < idx) && (idx < (channelEntry->numDesc - 1)))
      {
         prevDesc->dxedesc.dxe_short_desc.phyNextL = 
                                WLANDXE_U32_SWAP_ENDIAN(physAddress);
      }
      /* Last descriptor
       * make a ring by asign next pointer as first descriptor
       * ENDIAN SWAP NEEDED ??? */
      else if((channelEntry->numDesc - 1) == idx)
      {
         prevDesc->dxedesc.dxe_short_desc.phyNextL    = 
                                WLANDXE_U32_SWAP_ENDIAN(physAddress);
         currentDesc->dxedesc.dxe_short_desc.phyNextL =
                                WLANDXE_U32_SWAP_ENDIAN(channelEntry->headCtrlBlk->linkedDescPhyAddr);
      }

      /* If Current Channel is RX channel PAL Packet and OS packet buffer should be
       * Pre allocated and physical address must be assigned into
       * Corresponding DXE Descriptor */
#ifdef WLANDXE_TEST_CHANNEL_ENABLE
      if((WDTS_CHANNEL_RX_LOW_PRI  == channelEntry->channelType) ||
         (WDTS_CHANNEL_RX_HIGH_PRI == channelEntry->channelType) ||
         (WDTS_CHANNEL_H2H_TEST_RX == channelEntry->channelType))
#else
      if((WDTS_CHANNEL_RX_LOW_PRI  == channelEntry->channelType) ||
         (WDTS_CHANNEL_RX_HIGH_PRI == channelEntry->channelType))
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */
      {
         status = dxeRXFrameSingleBufferAlloc(dxeCtrlBlk,
                                              channelEntry,
                                              currentCtrlBlk);
         if( !WLAN_PAL_IS_STATUS_SUCCESS(status) )
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "dxeDescLinkAlloc RX Buffer Alloc Fail for channel %d",
                     channelEntry->channelType);
            return status;
         }
         --channelEntry->numFreeDesc;
      }

      if((WDTS_CHANNEL_TX_LOW_PRI == channelEntry->channelType) ||
         (WDTS_CHANNEL_TX_HIGH_PRI == channelEntry->channelType))
      {
         currentDesc->descCtrl.ctrl = channelEntry->extraConfig.cw_ctrl_write;
         currentDesc->dxedesc.dxe_short_desc.dstMemAddrL = channelEntry->extraConfig.refWQ_swapped;
      }
      else if((WDTS_CHANNEL_RX_LOW_PRI == channelEntry->channelType) ||
              (WDTS_CHANNEL_RX_HIGH_PRI == channelEntry->channelType))
      {
         currentDesc->descCtrl.ctrl = channelEntry->extraConfig.cw_ctrl_read;
         currentDesc->dxedesc.dxe_short_desc.srcMemAddrL = channelEntry->extraConfig.refWQ_swapped;
      }
      else
      {
         /* Just in case. H2H Test RX channel, do nothing
          * By Definition this must not happen */
      }

      currentCtrlBlk = currentCtrlBlk->nextCtrlBlk;
      prevDesc       = currentDesc;

#ifndef FEATURE_R33D
#ifndef WLANDXE_TEST_CHANNEL_ENABLE
      // advance to the next pre-allocated descriptor in the chunk
      currentDesc++;
      physAddress = (physAddress + sizeof(WLANDXE_DescType));
#endif
#endif
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name

  @  Description 

  @  Parameters

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeSetInterruptPath
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk
)
{
   wpt_status                 status        = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                 interruptPath = 0;
   wpt_uint32                 idx;
   WLANDXE_ChannelCBType     *channelEntry = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   for(idx = 0; idx < WDTS_CHANNEL_MAX; idx++)
   {
      channelEntry = &dxeCtrlBlk->dxeChannel[idx];
#ifdef WLANDXE_TEST_CHANNEL_ENABLE
      if((WDTS_CHANNEL_TX_LOW_PRI == channelEntry->channelType) ||
         (WDTS_CHANNEL_TX_HIGH_PRI == channelEntry->channelType) ||
         (WDTS_CHANNEL_H2H_TEST_TX == channelEntry->channelType))
#else
      if((WDTS_CHANNEL_TX_LOW_PRI == channelEntry->channelType) ||
         (WDTS_CHANNEL_TX_HIGH_PRI == channelEntry->channelType))
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */
      {
         interruptPath |= (1 << channelEntry->assignedDMAChannel);
      }
      else if((WDTS_CHANNEL_RX_LOW_PRI == channelEntry->channelType) ||
              (WDTS_CHANNEL_RX_HIGH_PRI == channelEntry->channelType))
      {
         interruptPath |= (1 << (channelEntry->assignedDMAChannel + 16));
      }
      else
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "H2H TEST RX???? %d", channelEntry->channelType);
      }
   }
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
            "Interrupt Path Must be 0x%x", interruptPath);
   dxeCtrlBlk->interruptPath = interruptPath;
   wpalWriteRegister(WLANDXE_CCU_DXE_INT_SELECT, interruptPath);

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name
      dxeEngineCoreStart 

  @  Description 
      Trigger to start RIVA DXE Hardware

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeEngineCoreStart
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk
)
{
   wpt_status                 status = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                 registerData = 0;
   wpt_uint8                  readRetry;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

#ifdef WCN_PRONTO
   /* Read default */
   wpalReadRegister(WLANDXE_CCU_SOFT_RESET, &registerData);
   registerData |= WLANDXE_DMA_CCU_DXE_RESET_MASK;

   /* Make reset */
   wpalWriteRegister(WLANDXE_CCU_SOFT_RESET, registerData);

   /* Clear reset */
   registerData &= ~WLANDXE_DMA_CCU_DXE_RESET_MASK;
   wpalWriteRegister(WLANDXE_CCU_SOFT_RESET, registerData);
#else
   /* START This core init is not needed for the integrated system */
   /* Reset First */
   registerData = WLANDXE_DMA_CSR_RESET_MASK;
   wpalWriteRegister(WALNDEX_DMA_CSR_ADDRESS,
                          registerData);
#endif /* WCN_PRONTO */

   for(readRetry = 0; readRetry < WLANDXE_CSR_MAX_READ_COUNT; readRetry++)
   {
      wpalWriteRegister(WALNDEX_DMA_CSR_ADDRESS,
                        WLANDXE_CSR_DEFAULT_ENABLE);
      wpalReadRegister(WALNDEX_DMA_CSR_ADDRESS, &registerData);
      if(!(registerData & WLANDXE_DMA_CSR_EN_MASK))
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "%s CSR 0x%x, count %d",
                  __func__, registerData, readRetry);
         /* CSR is not valid value, re-try to write */
         wpalBusyWait(WLANDXE_CSR_NEXT_READ_WAIT);
      }
      else
      {
         break;
      }
   }
   if(WLANDXE_CSR_MAX_READ_COUNT == readRetry)
   {
      /* MAX wait, still cannot write correct value
       * Panic device */
      wpalDevicePanic();
   }

   /* Is This needed?
    * Not sure, revisit with integrated system */
   /* END This core init is not needed for the integrated system */

   dxeSetInterruptPath(dxeCtrlBlk);
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      dxeChannelInitProgram

  @  Description 
      Program RIVA DXE engine register with initial value
      What must be programmed
         - Source Address             (SADRL, chDXESadrlRegAddr)
         - Destination address        (DADRL, chDXEDadrlRegAddr)
         - Next Descriptor address    (DESCL, chDXEDesclRegAddr)
         - current descriptor address (LST_DESCL, chDXELstDesclRegAddr)

      Not need to program now
         - Channel Control register   (CH_CTRL, chDXECtrlRegAddr)
           TX : Have to program to trigger send out frame
           RX : programmed by DXE engine

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block
  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeChannelInitProgram
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk,
   WLANDXE_ChannelCBType   *channelEntry
)
{
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                idx;
   WLANDXE_DescType         *currentDesc = NULL;
   WLANDXE_DescCtrlBlkType  *currentCtrlBlk = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity Check */
   if((NULL == dxeCtrlBlk) || (NULL == channelEntry))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelInitProgram Channel Entry is not valid");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   /* Program Source address and destination adderss */
   if(!channelEntry->channelConfig.useShortDescFmt)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelInitProgram Long Descriptor not support yet");
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   /* Common register area */
   /* Next linked list Descriptor pointer */
   status = wpalWriteRegister(channelEntry->channelRegister.chDXEDesclRegAddr,
                                   channelEntry->headCtrlBlk->linkedDescPhyAddr);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelInitProgram Write DESC Address register fail");
      return status;
   }

   if((WDTS_CHANNEL_TX_LOW_PRI  == channelEntry->channelType) ||
      (WDTS_CHANNEL_TX_HIGH_PRI == channelEntry->channelType))
   {
      /* Program default registers */
      /* TX DMA channel, DMA destination address is work Q */
      status = wpalWriteRegister(channelEntry->channelRegister.chDXEDadrlRegAddr,
                                      channelEntry->channelConfig.refWQ);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeChannelInitProgram Write TX DAddress register fail");
         return status;
      }
   }
   else if((WDTS_CHANNEL_RX_LOW_PRI  == channelEntry->channelType) ||
           (WDTS_CHANNEL_RX_HIGH_PRI == channelEntry->channelType))
   {
      /* Initialize Descriptor control Word First */
      currentCtrlBlk = channelEntry->headCtrlBlk;
      for(idx = 0; idx < channelEntry->channelConfig.nDescs; idx++)
      {
         currentDesc = currentCtrlBlk->linkedDesc;
         currentCtrlBlk = currentCtrlBlk->nextCtrlBlk;         
      }

      /* RX DMA channel, DMA source address is work Q */
      status = wpalWriteRegister(channelEntry->channelRegister.chDXESadrlRegAddr,
                                      channelEntry->channelConfig.refWQ);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeChannelInitProgram Write RX SAddress WQ register fail");
         return status;
      }

      /* RX DMA channel, Program pre allocated destination Address */
      status = wpalWriteRegister(channelEntry->channelRegister.chDXEDadrlRegAddr,
                                      WLANDXE_U32_SWAP_ENDIAN(channelEntry->DescBottomLoc->dxedesc.dxe_short_desc.phyNextL));
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeChannelInitProgram Write RX DAddress register fail");
         return status;
      }

      /* RX Channels, default Control registers MUST BE ENABLED */
      wpalWriteRegister(channelEntry->channelRegister.chDXECtrlRegAddr,
                             channelEntry->extraConfig.chan_mask);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeChannelInitProgram Write RX Control register fail");
         return status;
      }
   }
   else
   {
      /* H2H test channel, not use work Q */
      /* Program pre allocated destination Address */
      status = wpalWriteRegister(channelEntry->channelRegister.chDXEDadrlRegAddr,
                                      WLANDXE_U32_SWAP_ENDIAN(channelEntry->DescBottomLoc->dxedesc.dxe_short_desc.phyNextL));
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeChannelInitProgram Write RX DAddress register fail");
         return status;
      }
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}


/*==========================================================================
  @  Function Name 
      dxeChannelStart

  @  Description 
      Start Specific Channel

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeChannelStart
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk,
   WLANDXE_ChannelCBType   *channelEntry
)
{
   wpt_status                status     = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                regValue   = 0;
   wpt_uint32                intMaskVal = 0;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   channelEntry->extraConfig.chEnabled    = eWLAN_PAL_TRUE;
   channelEntry->extraConfig.chConfigured = eWLAN_PAL_TRUE;

   /* Enable individual channel
    * not to break current channel setup, first read register */
   status = wpalReadRegister(WALNDEX_DMA_CH_EN_ADDRESS,
                                  &regValue);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelStart Read Channel Enable register fail");
      return status;
   }

   /* Enable Channel specific Interrupt */
   status = wpalReadRegister(WLANDXE_INT_MASK_REG_ADDRESS,
                                  &intMaskVal);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelStart Read INT_MASK register fail");
      return status;         
   }
   intMaskVal |= channelEntry->extraConfig.intMask;
   status = wpalWriteRegister(WLANDXE_INT_MASK_REG_ADDRESS,
                                   intMaskVal);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelStart Write INT_MASK register fail");
      return status;         
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name
      dxeChannelStop

  @  Description 
      Stop Specific Channel

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeChannelStop
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk,
   WLANDXE_ChannelCBType   *channelEntry
)
{
   wpt_status                status     = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                intMaskVal = 0;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity */
   if((NULL == dxeCtrlBlk) || (NULL == channelEntry))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelStop Invalid arg input");
      return eWLAN_PAL_STATUS_E_INVAL; 
   }

   if ( (channelEntry->extraConfig.chEnabled != eWLAN_PAL_TRUE) ||
        (channelEntry->extraConfig.chConfigured != eWLAN_PAL_TRUE))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelStop channels are not enabled ");
      return status; 
   }
   /* Maskout interrupt */
   status = wpalReadRegister(WLANDXE_INT_MASK_REG_ADDRESS,
                                  &intMaskVal);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelStop Read INT_MASK register fail");
      return status;         
   }
   intMaskVal ^= channelEntry->extraConfig.intMask;
   status = wpalWriteRegister(WLANDXE_INT_MASK_REG_ADDRESS,
                                   intMaskVal);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelStop Write INT_MASK register fail");
      return status;         
   }

   channelEntry->extraConfig.chEnabled    = eWLAN_PAL_FALSE;

   /* Stop Channel ??? */
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      dxeChannelClose

  @  Description 
      Close Specific Channel
      Free pre allocated RX frame buffer if RX channel
      Free DXE descriptor for each channel
      Free Descriptor control block for each channel

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeChannelClose
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk,
   WLANDXE_ChannelCBType   *channelEntry
)
{
   wpt_status                status            = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                idx;
   WLANDXE_DescCtrlBlkType  *currentCtrlBlk    = NULL;
   WLANDXE_DescCtrlBlkType  *nextCtrlBlk       = NULL;
   WLANDXE_DescType         *currentDescriptor = NULL;
   WLANDXE_DescType         *nextDescriptor    = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity */
   if((NULL == dxeCtrlBlk) || (NULL == channelEntry))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelStop Invalid arg input");
      return eWLAN_PAL_STATUS_E_INVAL; 
   }

   currentCtrlBlk    = channelEntry->headCtrlBlk;
   if(NULL != currentCtrlBlk)
   {
      currentDescriptor = currentCtrlBlk->linkedDesc;
      for(idx = 0; idx < channelEntry->numDesc; idx++)
      {
          if (idx + 1 != channelEntry->numDesc)
          {
              nextCtrlBlk    = currentCtrlBlk->nextCtrlBlk;
              nextDescriptor = nextCtrlBlk->linkedDesc;
          }
          else
          {
              nextCtrlBlk = NULL;
              nextDescriptor = NULL;
          }
          if((WDTS_CHANNEL_RX_LOW_PRI  == channelEntry->channelType) ||
             (WDTS_CHANNEL_RX_HIGH_PRI == channelEntry->channelType))
          {
            if (NULL != currentCtrlBlk->xfrFrame)
            {
               wpalUnlockPacket(currentCtrlBlk->xfrFrame);
               wpalPacketFree(currentCtrlBlk->xfrFrame);
            }
         }
         /*  
          *  It is the responsibility of DXE to walk through the 
          *  descriptor chain and unlock any pending packets (if 
          *  locked). 
          */
         if((WDTS_CHANNEL_TX_LOW_PRI  == channelEntry->channelType) ||
            (WDTS_CHANNEL_TX_HIGH_PRI == channelEntry->channelType))
         {
             if((NULL != currentCtrlBlk->xfrFrame) && 
                     (eWLAN_PAL_STATUS_SUCCESS == wpalIsPacketLocked(currentCtrlBlk->xfrFrame)))
               {
                  wpalUnlockPacket(currentCtrlBlk->xfrFrame);
                  wpalPacketFree(currentCtrlBlk->xfrFrame);
               }
         }
#if (defined(FEATURE_R33D) || defined(WLANDXE_TEST_CHANNEL_ENABLE))
         // descriptors allocated individually so free them individually
         wpalDmaMemoryFree(currentDescriptor);
#endif
         wpalMemoryFree(currentCtrlBlk);

         currentCtrlBlk    = nextCtrlBlk;
         currentDescriptor = nextDescriptor;
         if(NULL == currentCtrlBlk)
         {
            /* Already reach last of the control block
             * Not need to process anymore, break */
            break;
         }
      }
   }

#if !(defined(FEATURE_R33D) || defined(WLANDXE_TEST_CHANNEL_ENABLE))
   // descriptors were allocated as a single chunk so free the chunk
   if(NULL != channelEntry->descriptorAllocation)
   {
      wpalDmaMemoryFree(channelEntry->descriptorAllocation);
   }
#endif

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name
      dxeChannelCleanInt

  @  Description 
      Clean up interrupt from RIVA HW
      After Host finish to handle interrupt, interrupt signal must be cleaned up
      Otherwise next interrupt will not be generated

  @  Parameters
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block
      wpt_uint32              *chStat
                               Channel Status register value

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeChannelCleanInt
(
   WLANDXE_ChannelCBType   *channelEntry,
   wpt_uint32              *chStat
)
{
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Read Channel Status Register to know why INT Happen */
   status = wpalReadRegister(channelEntry->channelRegister.chDXEStatusRegAddr,
                                  chStat);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelCleanInt Read CH STAT register fail");
      return eWLAN_PAL_STATUS_E_FAULT;         
   }
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Channel INT Clean, Status 0x%x",
            channelType[channelEntry->channelType], *chStat);

   /* Clean up all the INT within this channel */
   status = wpalWriteRegister(WLANDXE_INT_CLR_ADDRESS,
                                   (1 << channelEntry->assignedDMAChannel));
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeChannelCleanInt Write CH Clean register fail");
      return eWLAN_PAL_STATUS_E_FAULT;         
   }

   /* Clean up Error INT Bit */
   if(WLANDXE_CH_STAT_INT_ERR_MASK & *chStat)
   {
      status = wpalWriteRegister(WLANDXE_INT_ERR_CLR_ADDRESS,
                                      (1 << channelEntry->assignedDMAChannel));
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeChannelCleanInt Read CH STAT register fail");
         return eWLAN_PAL_STATUS_E_FAULT;         
      }
   }

   /* Clean up DONE INT Bit */
   if(WLANDXE_CH_STAT_INT_DONE_MASK & *chStat)
   {
      status = wpalWriteRegister(WLANDXE_INT_DONE_CLR_ADDRESS,
                                      (1 << channelEntry->assignedDMAChannel));
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeChannelCleanInt Read CH STAT register fail");
         return eWLAN_PAL_STATUS_E_FAULT;         
      }
   }

   /* Clean up ED INT Bit */
   if(WLANDXE_CH_STAT_INT_ED_MASK & *chStat)
   {
      status = wpalWriteRegister(WLANDXE_INT_ED_CLR_ADDRESS,
                                      (1 << channelEntry->assignedDMAChannel));
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeChannelCleanInt Read CH STAT register fail");
         return eWLAN_PAL_STATUS_E_FAULT;         
      }
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

#ifdef WLAN_DXE_LOW_RESOURCE_TIMER
/*==========================================================================
  @  Function Name
			      dxeRXResourceAvailableTimerExpHandler

  @  Description
      During pre-set timeperiod, if free available RX buffer is not allocated
      Trigger Driver re-loading to recover RX dead end

  @  Parameters
   v_VOID_t     *usrData
                DXE context

  @  Return
      NONE

===========================================================================*/
void dxeRXResourceAvailableTimerExpHandler
(
   void    *usrData
)
{
   WLANDXE_CtrlBlkType      *dxeCtxt    = NULL;
   wpt_uint32               numRxFreePackets;
   wpt_uint32               numAllocFailures;

   dxeCtxt = (WLANDXE_CtrlBlkType *)usrData;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "RX Low resource, Durign wait time period %d, RX resource not allocated",
            T_WLANDXE_MAX_RX_PACKET_WAIT);

   //This API wil also try to replenish packets
   wpalGetNumRxFreePacket(&numRxFreePackets);
   wpalGetNumRxPacketAllocFailures(&numAllocFailures);

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "Free Packets: %u, Alloc Failures: %u",
            numRxFreePackets, numAllocFailures);
   if (numRxFreePackets > 0)
   {
      /* If no. of free packets is greater than 0, it means
       * that some packets were replenished and can be used
       * by DXE to receive frames. So try to restart the
       * resourceAvailable timer here, it will be stopped
       * by the DXE's low resource callback if atleast one
       * free packet reaches DXE.
       */
      if (NULL != dxeCtxt)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                  "%s: Replenish successful. Restart the Rx Low resource timer",
                  __func__);
         wpalTimerStart(&dxeCtxt->rxResourceAvailableTimer,
                        T_WLANDXE_MAX_RX_PACKET_WAIT);
         return;
      }
   }

   if (NULL != dxeCtxt)
      dxeCtxt->driverReloadInProcessing = eWLAN_PAL_TRUE;

   wpalWlanReload();

   if (NULL != usrData)
      dxeStartSSRTimer((WLANDXE_CtrlBlkType *)usrData);

   return;
}
#endif

/*==========================================================================
  @  Function Name
     dxeStartSSRTimer

  @  Description
      Start the dxeSSRTimer after issuing the FIQ to restart the WCN chip,
      this makes sure that if the chip does not respond to the FIQ within
      the timeout period the dxeSSRTimer expiration handler will take the
      appropriate action.

  @  Parameters
      NONE

  @  Return
      NONE

===========================================================================*/
static void dxeStartSSRTimer
(
  WLANDXE_CtrlBlkType     *dxeCtxt
)
{
   if(VOS_TIMER_STATE_RUNNING !=
      wpalTimerGetCurStatus(&dxeCtxt->dxeSSRTimer))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "%s: Starting SSR Timer",__func__);
      wpalTimerStart(&dxeCtxt->dxeSSRTimer,
                     T_WLANDXE_SSR_TIMEOUT);
   }
}

/*==========================================================================
  @  Function Name
     dxeSSRTimerExpHandler

  @  Description
      Issue an explicit subsystem restart of the wcnss subsystem if the
      WCN chip does not respond to the FIQ within the timeout period

  @  Parameters
   v_VOID_t     *usrData

  @  Return
      NONE

===========================================================================*/
void dxeSSRTimerExpHandler
(
   void    *usrData
)
{
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "DXE not shutdown %d ms after FIQ!! Issue SSR",
            T_WLANDXE_SSR_TIMEOUT);
   wpalRivaSubystemRestart();

   return;
}

/*==========================================================================
  @  Function Name 
      dxeRXPacketAvailableCB

  @  Description 
      If RX frame handler encounts RX buffer pool empty condition,
      DXE RX handle loop will be blocked till get available RX buffer pool.
      When new RX buffer pool available, Packet available CB function will
      be called.

  @  Parameters
   wpt_packet   *freePacket
                Newly allocated RX buffer
   v_VOID_t     *usrData
                DXE context

  @  Return
      NONE

===========================================================================*/
void dxeRXPacketAvailableCB
(
   wpt_packet   *freePacket,
   v_VOID_t     *usrData
)
{
   WLANDXE_CtrlBlkType       *dxeCtxt = NULL;
   wpt_status                status;

   /* Simple Sanity */
   if((NULL == freePacket) || (NULL == usrData))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "Get Free RX Buffer fail, Critical Error");
      HDXE_ASSERT(0);
      return;
   }

   dxeCtxt = (WLANDXE_CtrlBlkType *)usrData;

   if(WLANDXE_CTXT_COOKIE != dxeCtxt->dxeCookie)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "DXE Context data corrupted, Critical Error");
      HDXE_ASSERT(0);
      return;
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,
            "DXE RX packet available, post MSG to RX Thread");

   dxeCtxt->freeRXPacket = freePacket;

   /* Serialize RX Packet Available message upon RX thread */
   if (NULL == dxeCtxt->rxPktAvailMsg)
   {
       HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "DXE NULL pkt");
       HDXE_ASSERT(0);
       return;
   }

   status = wpalPostRxMsg(WDI_GET_PAL_CTX(),
                          dxeCtxt->rxPktAvailMsg);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "dxeRXPacketAvailableCB serialize fail");
   }

   return;
}

/*==========================================================================
  @  Function Name 
      dxeRXFrameSingleBufferAlloc

  @  Description 
      Allocate Platform packet buffer to prepare RX frame
      RX frame memory space must be pre allocted and must be asigned to
      descriptor
      then whenever DMA engine want to tranfer frame from BMU,
      buffer must be ready

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block
      WLANDXE_DescCtrlBlkType  currentCtrlBlock
                               current control block which have to be asigned 
                               frame buffer

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeRXFrameSingleBufferAlloc
(
   WLANDXE_CtrlBlkType      *dxeCtxt,
   WLANDXE_ChannelCBType    *channelEntry,
   WLANDXE_DescCtrlBlkType  *currentCtrlBlock
)
{
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;
   wpt_packet               *currentPalPacketBuffer = NULL;
   WLANDXE_DescType         *currentDesc            = NULL;
#ifdef FEATURE_R33D
   wpt_uint32                virtualAddressPCIe;
   wpt_uint32                physicalAddressPCIe;   
#else
   wpt_iterator              iterator;
   wpt_uint32                allocatedSize          = 0;
   void                     *physAddress            = NULL;
#endif /* FEATURE_R33D */


   currentDesc            = currentCtrlBlock->linkedDesc;

   if(currentDesc->descCtrl.valid)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "This Descriptor is valid, Do not refill");
      return eWLAN_PAL_STATUS_E_EXISTS;
   }

   /* First check if a packet pointer has already been provided by a previously
      invoked Rx packet available callback. If so use that packet. */
   if(dxeCtxt->rxPalPacketUnavailable && (NULL != dxeCtxt->freeRXPacket))
   {
      currentPalPacketBuffer = dxeCtxt->freeRXPacket;
      dxeCtxt->rxPalPacketUnavailable = eWLAN_PAL_FALSE;
      dxeCtxt->freeRXPacket = NULL;

      if (channelEntry->doneIntDisabled)
      {
         wpalWriteRegister(channelEntry->channelRegister.chDXECtrlRegAddr,
                           channelEntry->extraConfig.chan_mask);
         channelEntry->doneIntDisabled = 0;
      }
   }
   else if(!dxeCtxt->rxPalPacketUnavailable)
   {
      /* Allocate platform Packet buffer and OS Frame Buffer at here */
      currentPalPacketBuffer = wpalPacketAlloc(eWLAN_PAL_PKT_TYPE_RX_RAW,
                                            WLANDXE_DEFAULT_RX_OS_BUFFER_SIZE,
                                            dxeRXPacketAvailableCB,
                                            (void *)dxeCtxt);

      if(NULL == currentPalPacketBuffer)
      {
         dxeCtxt->rxPalPacketUnavailable = eWLAN_PAL_TRUE;
#ifdef WLAN_DXE_LOW_RESOURCE_TIMER
         /* Out of RX free buffer,
          * Start timer to recover from RX dead end */
         if(VOS_TIMER_STATE_RUNNING !=
            wpalTimerGetCurStatus(&dxeCtxt->rxResourceAvailableTimer))
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                     "RX Low resource, wait available resource");
            wpalTimerStart(&dxeCtxt->rxResourceAvailableTimer,
                           T_WLANDXE_MAX_RX_PACKET_WAIT);
         }
#endif
      }
   }
   
   if(NULL == currentPalPacketBuffer)
   {
      return eWLAN_PAL_STATUS_E_RESOURCES;
   }

   currentCtrlBlock->xfrFrame       = currentPalPacketBuffer;
   currentPalPacketBuffer->pktType  = eWLAN_PAL_PKT_TYPE_RX_RAW;
   currentPalPacketBuffer->pBD      = NULL;
   currentPalPacketBuffer->pBDPhys  = NULL;
   currentPalPacketBuffer->BDLength = 0;
#ifdef FEATURE_R33D
   status = wpalAllocateShadowRxFrame(currentPalPacketBuffer,
                                           &physicalAddressPCIe,
                                           &virtualAddressPCIe);
   if((0 == physicalAddressPCIe) || (0 = virtualAddressPCIe))
   {
       HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED,
               "RX NULL Shadow Memory");
       HDXE_ASSERT(0);
       return eWLAN_PAL_STATUS_E_FAULT;
   }
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED,
            "RX Shadow Memory Va 0x%x, Pa 0x%x",
            virtualAddressPCIe, physicalAddressPCIe);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXFrameBufferAlloc Shadow Mem Alloc fail");
      return status;
   }
   currentCtrlBlock->shadowBufferVa = virtualAddressPCIe;
   currentPalPacketBuffer->pBDPhys  = (void *)physicalAddressPCIe;
   memset((wpt_uint8 *)currentCtrlBlock->shadowBufferVa, 0, WLANDXE_DEFAULT_RX_OS_BUFFER_SIZE);
#else
   status = wpalLockPacketForTransfer(currentPalPacketBuffer);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXFrameBufferAlloc unable to lock packet");
      return status;
   }

   /* Init iterator to get physical os buffer address */
   status = wpalIteratorInit(&iterator, currentPalPacketBuffer);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXFrameBufferAlloc iterator init fail");
      return status;
   }
   status = wpalIteratorNext(&iterator,
                             currentPalPacketBuffer,
                             &physAddress,
                             &allocatedSize);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXFrameBufferAlloc iterator Get Next pointer fail");
      return status;
   }
   currentPalPacketBuffer->pBDPhys = physAddress;
#endif /* FEATURE_R33D */

   /* DXE descriptor must have SWAPPED addres in it's structure
    * !!! SWAPPED !!! */
   currentDesc->dxedesc.dxe_short_desc.dstMemAddrL =
                                       WLANDXE_U32_SWAP_ENDIAN((wpt_uint32)(uintptr_t)currentPalPacketBuffer->pBDPhys);

   return status;
}

/*==========================================================================
  @  Function Name 
      dxeRXFrameRefillRing

  @  Description 
      Allocate Platform packet buffers to try to fill up the DXE Rx ring

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block
      
  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeRXFrameRefillRing
(
   WLANDXE_CtrlBlkType      *dxeCtxt,
   WLANDXE_ChannelCBType    *channelEntry
)
{
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_DescCtrlBlkType  *currentCtrlBlk = channelEntry->tailCtrlBlk;
   WLANDXE_DescType         *currentDesc    = NULL;

   while(channelEntry->numFreeDesc > 0)
   {
      /* Current Control block is free
       * and associated frame buffer is not linked with control block anymore
       * allocate new frame buffer for current control block */
      status = dxeRXFrameSingleBufferAlloc(dxeCtxt,
                                           channelEntry,
                                           currentCtrlBlk);

      if((eWLAN_PAL_STATUS_SUCCESS != status) &&
         (eWLAN_PAL_STATUS_E_EXISTS != status))
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "dxeRXFrameRefillRing, out of RX buffer pool, break here");
         break;
      }

      if(eWLAN_PAL_STATUS_E_EXISTS == status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXFrameRefillRing, Descriptor Non-Empry");
      }

      currentDesc = currentCtrlBlk->linkedDesc;
      currentDesc->descCtrl.ctrl = channelEntry->extraConfig.cw_ctrl_read;

      /* Issue a dummy read from the DXE descriptor DDR location to ensure
         that any posted writes are reflected in memory before DXE looks at
         the descriptor. */
      if(channelEntry->extraConfig.cw_ctrl_read != currentDesc->descCtrl.ctrl)
      {
         //HDXE_ASSERT(0);
      }

      /* Kick off the DXE ring, if not in any power save mode */
      if(WLANDXE_POWER_STATE_FULL == dxeCtxt->hostPowerState)
      {
         wpalWriteRegister(WALNDEX_DMA_ENCH_ADDRESS,
                           1 << channelEntry->assignedDMAChannel);
      }
      currentCtrlBlk = currentCtrlBlk->nextCtrlBlk;
      if(eWLAN_PAL_STATUS_E_EXISTS != status)
      {
         --channelEntry->numFreeDesc;
      }
   }
   
   channelEntry->tailCtrlBlk = currentCtrlBlk;

   return status;
}

/*==========================================================================
  @  Function Name
      dxeRXFrameRouteUpperLayer

  @  Description 
      Test DXE descriptors and if any RX frame pending within RING,
      Route to upper layer

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block
  @  Return
      < 0 Any error happen
      0  No frame pulled from RX RING
      int number of RX frames pulled from RX ring

===========================================================================*/
static wpt_int32 dxeRXFrameRouteUpperLayer
(
   WLANDXE_CtrlBlkType     *dxeCtxt,
   WLANDXE_ChannelCBType   *channelEntry
)
{
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_DescCtrlBlkType  *currentCtrlBlk = NULL;
   WLANDXE_DescType         *currentDesc    = NULL;
   wpt_uint32                descCtrl, frameCount = 0, i;
   wpt_int32                 ret_val = -1;

   currentCtrlBlk = channelEntry->headCtrlBlk;
   currentDesc    = currentCtrlBlk->linkedDesc;

   /* Descriptoe should be SWAPPED ???? */
   descCtrl = currentDesc->descCtrl.ctrl;

   /* Get frames while VALID bit is not set (DMA complete) and a data 
    * associated with it */
   while(!(WLANDXE_U32_SWAP_ENDIAN(descCtrl) & WLANDXE_DESC_CTRL_VALID) &&
         (eWLAN_PAL_STATUS_SUCCESS == wpalIsPacketLocked(currentCtrlBlk->xfrFrame)) &&
         (currentCtrlBlk->xfrFrame->pInternalData != NULL) &&
         (frameCount < WLANDXE_MAX_REAPED_RX_FRAMES) )
   {
      channelEntry->numTotalFrame++;
      channelEntry->numFreeDesc++;
#ifdef FEATURE_R33D
      /* Transfer Size should be */
      currentDesc->xfrSize = WLANDXE_U32_SWAP_ENDIAN(WLANDXE_DEFAULT_RX_OS_BUFFER_SIZE);
      status = wpalPrepareRxFrame(&currentCtrlBlk->xfrFrame,
                                       (wpt_uint32)currentCtrlBlk->xfrFrame->pBDPhys,
                                       currentCtrlBlk->shadowBufferVa,
                                       WLANDXE_DEFAULT_RX_OS_BUFFER_SIZE);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXFrameReady Prepare RX Frame fail");
         return ret_val;
      }
      status = wpalFreeRxFrame(currentCtrlBlk->shadowBufferVa);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXFrameReady Free Shadow RX Frame fail");
         return ret_val;
      }

#else /* FEATURE_R33D */
      status = wpalUnlockPacket(currentCtrlBlk->xfrFrame);
      if (eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXFrameReady unable to unlock packet");
         return ret_val;
      }
#endif /* FEATURE_R33D */
      /* This Descriptor is valid, so linked Control block is also valid
       * Linked Control block has pre allocated packet buffer
       * So, just let upper layer knows preallocated frame pointer will be OK */
      /* Reap Rx frames */ 
      rx_reaped_buf[frameCount] = currentCtrlBlk->xfrFrame;
      frameCount++;
      currentCtrlBlk->xfrFrame = NULL;

      /* Now try to refill the ring with empty Rx buffers to keep DXE busy */
      dxeRXFrameRefillRing(dxeCtxt, channelEntry);

      /* Test next contorl block
       * if valid, this control block also has new RX frame must be handled */
      currentCtrlBlk = (WLANDXE_DescCtrlBlkType *)currentCtrlBlk->nextCtrlBlk;
      currentDesc    = currentCtrlBlk->linkedDesc;
      descCtrl       = currentDesc->descCtrl.ctrl;
   }

   /* Update head control block
    * current control block's valid bit was 0
    * next trial first control block must be current control block */
   channelEntry->headCtrlBlk = currentCtrlBlk;

   /* Deliver all the reaped RX frames to upper layers */
   i = 0;
   while(i < frameCount)
   {
      dxeCtxt->rxReadyCB(dxeCtxt->clientCtxt, rx_reaped_buf[i], channelEntry->channelType);
      i++;
   }

   return frameCount;
}

/*==========================================================================
  @  Function Name 
      dxeRXFrameReady

  @  Description 
      Pop frame from descriptor and route frame to upper transport layer
      Assign new platform packet buffer into used descriptor
      Actual frame pop and resource realloc

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeRXFrameReady
(
   WLANDXE_CtrlBlkType     *dxeCtxt,
   WLANDXE_ChannelCBType   *channelEntry,
   wpt_uint32               chStat
)
{
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_DescCtrlBlkType  *currentCtrlBlk = NULL;
   WLANDXE_DescType         *currentDesc    = NULL;
   wpt_uint32                descCtrl;
   wpt_int32                 frameCount = 0;

   wpt_uint32                descLoop;
   wpt_uint32                invalidatedFound = 0;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity Check */
   if((NULL == dxeCtxt) || (NULL == channelEntry))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXFrameReady Channel Entry is not valid");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   frameCount = dxeRXFrameRouteUpperLayer(dxeCtxt, channelEntry);

   if(0 > frameCount)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXFrameReady RX frame route fail");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if((0 == frameCount) &&
      ((WLANDXE_POWER_STATE_BMPS == dxeCtxt->hostPowerState) ||
       (WLANDXE_POWER_STATE_FULL == dxeCtxt->hostPowerState)))
   {
      /* None of the frame handled and CH is not enabled
       * RX CH wrap around happen and No RX free frame
       * RX side should wait till new free frame available in the pool
       * Do not try reload driver at here*/
      if(!(chStat & WLANDXE_CH_CTRL_EN_MASK))
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXFrameReady %s RING Wrapped, RX Free Low 0x%x",
                  channelType[channelEntry->channelType], chStat);
         /* This is not empty interrupt case
          * If handle this as empty interrupt, false SSR might be issued
          * Frame count '1' is dummy frame count to avoid SSR */
         channelEntry->numFragmentCurrentChain = 1;
         return eWLAN_PAL_STATUS_SUCCESS;
      }

      currentCtrlBlk = channelEntry->headCtrlBlk;
      currentDesc    = currentCtrlBlk->linkedDesc;
      descCtrl       = currentDesc->descCtrl.ctrl;

      if(WLANDXE_POWER_STATE_BMPS != dxeCtxt->hostPowerState)
      {
          HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
                   "RX ISR called but no frame handled PWS %d, channel %s",
                   (int)dxeCtxt->hostPowerState,
                   channelType[channelEntry->channelType]);
      }

      /* Current interupt empty and previous interrupt also empty
       * detected successive empty interrupt
       * or first interrupt empty, this should not happen */
      if(0 == channelEntry->numFragmentCurrentChain)
      {
         dxeChannelMonitor("RX Ready", channelEntry, NULL);
         dxeDescriptorDump(channelEntry, channelEntry->headCtrlBlk->linkedDesc, 0);
         dxeChannelRegisterDump(channelEntry, "RX successive empty interrupt", NULL);
         dxeChannelAllDescDump(channelEntry, channelEntry->channelType, NULL);
         /* Abnormal interrupt detected, try to find not validated descriptor */
         for(descLoop = 0; descLoop < channelEntry->numDesc; descLoop++)
         {
            if(!(WLANDXE_U32_SWAP_ENDIAN(descCtrl) & WLANDXE_DESC_CTRL_VALID))
            {
               HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                        "Found Invalidated Descriptor %d", (int)descLoop);
               if(eWLAN_PAL_STATUS_SUCCESS == wpalIsPacketLocked(currentCtrlBlk->xfrFrame))
               {
                  HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                           "Packet locked, Resync Host and HW");
                  channelEntry->headCtrlBlk = currentCtrlBlk;
                  invalidatedFound = 1;
                  break;
               }
               else
               {
                  HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                           "Packet Not Locked, cannot transfer frame");
               }
            }
            currentCtrlBlk = (WLANDXE_DescCtrlBlkType *)currentCtrlBlk->nextCtrlBlk;
            currentDesc    = currentCtrlBlk->linkedDesc;
            descCtrl       = currentDesc->descCtrl.ctrl;
         }

         /* Invalidated descriptor found, and that is not head descriptor
          * This means HW/SW descriptor miss match happen, and we may recover with just resync
          * Try re-sync here */
         if((invalidatedFound) && (0 != descLoop))
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "Found New Sync location with HW, handle frames from there");
            frameCount = dxeRXFrameRouteUpperLayer(dxeCtxt, channelEntry);
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "re-sync routed %d frames to upper layer", (int)frameCount);
            channelEntry->numFragmentCurrentChain = frameCount;
         }
         /* Successive Empty interrupt
          * But this case, first descriptor also invalidated, then it means head descriptor 
          * is linked with already handled RX frame, then could not unlock RX frame
          * This is just Out of RX buffer pool, not need to anything here */
         else if((invalidatedFound) && (0 == descLoop))
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "Out of RX Low resource, and INT came in, do nothing till get RX resource");
         }
         /* Critical error, reload driver */
         else
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "Could not found invalidated descriptor");
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "RX successive empty interrupt, Could not find invalidated DESC reload driver");
            dxeCtxt->driverReloadInProcessing = eWLAN_PAL_TRUE;
            wpalWlanReload();
            dxeStartSSRTimer(dxeCtxt);
         }
      }
   }
   channelEntry->numFragmentCurrentChain = frameCount;
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      dxeNotifySmsm

  @  Description: Notify SMSM to start DXE engine and/or condition of Tx ring
  buffer 

  @  Parameters

  @  Return
      wpt_status

===========================================================================*/
static wpt_status dxeNotifySmsm
(
  wpt_boolean kickDxe,
  wpt_boolean ringEmpty
)
{
   wpt_uint32 clrSt = 0;
   wpt_uint32 setSt = 0;

   if(kickDxe)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED, "Kick off DXE");

     if(tempDxeCtrlBlk->lastKickOffDxe == 0)
     {
       setSt |= WPAL_SMSM_WLAN_TX_ENABLE; 
       tempDxeCtrlBlk->lastKickOffDxe = 1;
     }
     else if(tempDxeCtrlBlk->lastKickOffDxe == 1)
     {
       clrSt |= WPAL_SMSM_WLAN_TX_ENABLE;
       tempDxeCtrlBlk->lastKickOffDxe = 0;
     }
     else
     {
       HDXE_ASSERT(0);
     }
   }
   else
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED, "no need to kick off DXE");
   }

   tempDxeCtrlBlk->txRingsEmpty = ringEmpty;
   if(ringEmpty)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED, "SMSM Tx Ring Empty");
     clrSt |= WPAL_SMSM_WLAN_TX_RINGS_EMPTY; 
   }
   else
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED, "SMSM Tx Ring Not Empty");
     setSt |= WPAL_SMSM_WLAN_TX_RINGS_EMPTY; 
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_HIGH, "C%x S%x", clrSt, setSt);

   wpalNotifySmsm(clrSt, setSt);

   return eWLAN_PAL_STATUS_SUCCESS;
}

/*==========================================================================
  @  Function Name 
      dxePsComplete

  @  Description: Utility function to check the resv desc to deside if we can
  get into Power Save mode now 

  @  Parameters

  @  Return
      None

===========================================================================*/
static void dxePsComplete(WLANDXE_CtrlBlkType *dxeCtxt, wpt_boolean intr_based)
{
   if( dxeCtxt->hostPowerState == WLANDXE_POWER_STATE_FULL )
   {
     return;
   }

   //if both HIGH & LOW Tx channels don't have anything on resv desc,all Tx pkts
   //must have been consumed by RIVA, OK to get into BMPS
   if((0 == dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI].numRsvdDesc) &&
      (0 == dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_HIGH_PRI].numRsvdDesc))
   {
      tempDxeCtrlBlk->ringNotEmpty = eWLAN_PAL_FALSE;
      //if host is in BMPS & no pkt to Tx, RIVA can go to power save
      if(WLANDXE_POWER_STATE_BMPS == dxeCtxt->hostPowerState)
      {
         dxeCtxt->rivaPowerState = WLANDXE_RIVA_POWER_STATE_BMPS_UNKNOWN;
         dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
      }
   }
   else //still more pkts to be served by RIVA
   {
      tempDxeCtrlBlk->ringNotEmpty = eWLAN_PAL_TRUE;

      switch(dxeCtxt->rivaPowerState)
      {
         case WLANDXE_RIVA_POWER_STATE_ACTIVE:
            //NOP
            break;
         case WLANDXE_RIVA_POWER_STATE_BMPS_UNKNOWN:
            if(intr_based)
            {
               dxeCtxt->rivaPowerState = WLANDXE_RIVA_POWER_STATE_ACTIVE;
               dxeNotifySmsm(eWLAN_PAL_TRUE, eWLAN_PAL_FALSE);
            }
            break;
         default:
            //assert
            break;
      }
   }
}

/*==========================================================================
  @  Function Name 
      dxeRXEventHandler

  @  Description 
      Handle serailized RX frame ready event
      First disable interrupt then pick up frame from pre allocated buffer
      Since frame handle is doen, clear interrupt bit to ready next interrupt
      Finally re enable interrupt

  @  Parameters
      wpt_msg   *rxReadyMsg
                 RX frame ready MSG pointer
                 include DXE control context

  @  Return
      NONE

===========================================================================*/
void dxeRXEventHandler
(
   wpt_msg                 *rxReadyMsg
)
{
   wpt_msg                  *msgContent = (wpt_msg *)rxReadyMsg;
   WLANDXE_CtrlBlkType      *dxeCtxt    = NULL;
   wpt_status                status     = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                intSrc     = 0;
   WLANDXE_ChannelCBType    *channelCb  = NULL;
   wpt_uint32                chHighStat = 0;
   wpt_uint32                chLowStat  = 0;
   wpt_uint32                regValue, chanMask;

   dxeCtxt = (WLANDXE_CtrlBlkType *)(msgContent->pContext);

   if(eWLAN_PAL_TRUE == dxeCtxt->driverReloadInProcessing)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "RX Ready WLAN Driver re-loading in progress");
      return;
   }

   /* Now try to refill the ring with empty Rx buffers to keep DXE busy */
   dxeRXFrameRefillRing(dxeCtxt, &dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI]);
   dxeRXFrameRefillRing(dxeCtxt, &dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI]);

   dxeCtxt = (WLANDXE_CtrlBlkType *)(msgContent->pContext);
      
   if((!dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI].extraConfig.chEnabled) ||
      (!dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI].extraConfig.chEnabled))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
         "DXE already stopped in RX event handler. Just return");
      return;
   }

   if((WLANDXE_POWER_STATE_IMPS == dxeCtxt->hostPowerState) ||
      (WLANDXE_POWER_STATE_DOWN == dxeCtxt->hostPowerState))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
         "%s Riva is in %d, Just Pull frames without any register touch ",
           __func__, dxeCtxt->hostPowerState);

      /* Not to touch any register, just pull frame directly from chain ring
       * First high priority */
      channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI];
      status = dxeRXFrameReady(dxeCtxt,
                               channelCb,
                               chHighStat);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXEventHandler Pull from RX high channel fail");        
      }
      /* In case FW could not power collapse in IMPS mode
       * Next power restore might have empty interrupt
       * If IMPS mode has empty interrupt since RX thread race,
       * Invalid re-load driver might happen
       * To prevent invalid re-load driver,
       * IMPS event handler set dummpy frame count */
      channelCb->numFragmentCurrentChain = 1;

       /* Second low priority */
      channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI];
      status = dxeRXFrameReady(dxeCtxt,
                               channelCb,
                               chLowStat);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXEventHandler Pull from RX low channel fail");        
      }
      /* LOW Priority CH same above */
      channelCb->numFragmentCurrentChain = 1;

      /* Interrupt will not enabled at here, it will be enabled at PS mode change */
      tempDxeCtrlBlk->rxIntDisabledByIMPS = eWLAN_PAL_TRUE;

      return;
   }

   /* Disable device interrupt */
   /* Read whole interrupt mask register and exclusive only this channel int */
   status = wpalReadRegister(WLANDXE_INT_SRC_RAW_ADDRESS,
                             &intSrc);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXEventHandler Read INT_SRC register fail");
      return;         
   }
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED,
            "RX Event Handler INT Source 0x%x", intSrc);

#ifndef WLANDXE_TEST_CHANNEL_ENABLE
   /* Test High Priority Channel interrupt is enabled or not */
   channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI];
   if(intSrc & (1 << channelCb->assignedDMAChannel))
   {
      status = dxeChannelCleanInt(channelCb, &chHighStat);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXEventHandler INT Clean up fail");
         return;         
      }
      if(WLANDXE_CH_STAT_INT_ERR_MASK & chHighStat)
      {
         /* Error Happen during transaction, Handle it */
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                  "%11s : 0x%x Error Reported, Reload Driver",
                  channelType[channelCb->channelType], chHighStat);

         dxeErrChannelDebug(channelCb, chHighStat);

         dxeCtxt->driverReloadInProcessing = eWLAN_PAL_TRUE;
         wpalWlanReload();
         dxeStartSSRTimer(dxeCtxt);
      }
      else if((WLANDXE_CH_STAT_INT_DONE_MASK & chHighStat) ||
              (WLANDXE_CH_STAT_INT_ED_MASK & chHighStat))
      {
         /* Handle RX Ready for high priority channel */
         status = dxeRXFrameReady(dxeCtxt,
                                  channelCb,
                                  chHighStat);
      }
      else if(WLANDXE_CH_STAT_MASKED_MASK & chHighStat)
      {
         status = dxeRXFrameReady(dxeCtxt,
                                  channelCb,
                                  chHighStat);
      }
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,
               "RX HIGH CH EVNT STAT 0x%x, %d frames handled", chHighStat, channelCb->numFragmentCurrentChain);
      /* Update the Rx DONE histogram */
      channelCb->rxDoneHistogram = (channelCb->rxDoneHistogram << 1);
      if(WLANDXE_CH_STAT_INT_DONE_MASK & chHighStat)
      {
         channelCb->rxDoneHistogram |= 1;
      }
      else
      {
         channelCb->rxDoneHistogram &= ~1;
      }
   }
#else
   /* Test H2H Test interrupt is enabled or not */
   channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_H2H_TEST_RX];
   if(intSrc & (1 << channelCb->assignedDMAChannel))
   {
      status = dxeChannelCleanInt(channelCb, &chStat);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXEventHandler INT Clean up fail");
         return;         
      }

      if(WLANDXE_CH_STAT_INT_ERR_MASK & chStat)
      {
         /* Error Happen during transaction, Handle it */
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                  "%11s : 0x%x Error Reported, Reload Driver",
                  channelType[channelCb->channelType], chStat);

         dxeErrChannelDebug(channelCb, chStat);

         dxeCtxt->driverReloadInProcessing = eWLAN_PAL_TRUE;
         wpalWlanReload();
         dxeStartSSRTimer(dxeCtxt);
      }
      else if(WLANDXE_CH_STAT_INT_ED_MASK & chStat)
      {
         /* Handle RX Ready for high priority channel */
         status = dxeRXFrameReady(dxeCtxt,
                                  channelCb,
                                  chStat);
      }
      /* Update the Rx DONE histogram */
      channelCb->rxDoneHistogram = (channelCb->rxDoneHistogram << 1);
      if(WLANDXE_CH_STAT_INT_DONE_MASK & chStat)
      {
         channelCb->rxDoneHistogram |= 1;
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
            "DXE Channel Number %d, Rx DONE Histogram 0x%016llx",
            channelCb->assignedDMAChannel, channelCb->rxDoneHistogram);
      }
      else
      {
         channelCb->rxDoneHistogram &= ~1;
      }
   }
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */

   /* Test Low Priority Channel interrupt is enabled or not */
       channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI];
   if(intSrc & (1 << channelCb->assignedDMAChannel))
   {
      status = dxeChannelCleanInt(channelCb, &chLowStat);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXEventHandler INT Clean up fail");
         return;         
      }

      if(WLANDXE_CH_STAT_INT_ERR_MASK & chLowStat)
      {
         /* Error Happen during transaction, Handle it */
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                  "%11s : 0x%x Error Reported, Reload Driver",
                  channelType[channelCb->channelType], chLowStat);

         dxeErrChannelDebug(channelCb, chLowStat);

         dxeCtxt->driverReloadInProcessing = eWLAN_PAL_TRUE;
         wpalWlanReload();
         dxeStartSSRTimer(dxeCtxt);
      }
      else if((WLANDXE_CH_STAT_INT_ED_MASK & chLowStat) ||
               (WLANDXE_CH_STAT_INT_DONE_MASK & chLowStat))
      {
         /* Handle RX Ready for low priority channel */
         status = dxeRXFrameReady(dxeCtxt,
                                  channelCb,
                                  chLowStat);
       }

      /* Update the Rx DONE histogram */
      channelCb->rxDoneHistogram = (channelCb->rxDoneHistogram << 1);
      if(WLANDXE_CH_STAT_INT_DONE_MASK & chLowStat)
      {
         channelCb->rxDoneHistogram |= 1;
      }
      else
      {
         channelCb->rxDoneHistogram &= ~1;
      }
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,
               "RX LOW CH EVNT STAT 0x%x, %d frames handled", chLowStat, channelCb->numFragmentCurrentChain);
   }
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXEventHandler Handle Frame Ready Fail");
      return;         
   }

   /* Prepare Control Register EN Channel */
   if(!(dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI].extraConfig.chan_mask & WLANDXE_CH_CTRL_EN_MASK))
   {
      HDXE_ASSERT(0);
   }

   if (dxeCtxt->rxPalPacketUnavailable &&
       (WLANDXE_CH_STAT_INT_DONE_MASK & chHighStat))
   {
     chanMask = dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI].extraConfig.chan_mask &
                (~WLANDXE_CH_CTRL_INE_DONE_MASK);
     dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI].doneIntDisabled = 1;
   }
   else
   {
     chanMask = dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI].extraConfig.chan_mask;
     dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI].doneIntDisabled = 0;
   }
   wpalWriteRegister(dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI].channelRegister.chDXECtrlRegAddr,
                     chanMask);

   /* Prepare Control Register EN Channel */
   if(!(dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI].extraConfig.chan_mask & WLANDXE_CH_CTRL_EN_MASK))
   {
      HDXE_ASSERT(0);
   }

   if (dxeCtxt->rxPalPacketUnavailable &&
       (WLANDXE_CH_STAT_INT_DONE_MASK & chLowStat))
   {
     chanMask = dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI].extraConfig.chan_mask &
                (~WLANDXE_CH_CTRL_INE_DONE_MASK);
     dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI].doneIntDisabled = 1;
   }
   else
   {
     chanMask = dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI].extraConfig.chan_mask;
     dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI].doneIntDisabled = 0;
   }
   wpalWriteRegister(dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI].channelRegister.chDXECtrlRegAddr,
                     chanMask);


   /* Clear Interrupt handle processing bit
    * RIVA may power down */
   wpalReadRegister(WLANDXE_INT_MASK_REG_ADDRESS, &regValue);
   regValue &= WLANDXE_RX_INTERRUPT_PRO_UNMASK;
   wpalWriteRegister(WLANDXE_INT_MASK_REG_ADDRESS, regValue);

   /* Enable system level ISR */
   /* Enable RX ready Interrupt at here */
   status = wpalEnableInterrupt(DXE_INTERRUPT_RX_READY);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXEventHandler Enable RX Ready interrupt fail");
      return;
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return;
}

/*==========================================================================
  @  Function Name 
      dxeRXPacketAvailableEventHandler

  @  Description 
      Handle serialized RX Packet Available event when the corresponding callback
      is invoked by WPAL.
      Try to fill up any completed DXE descriptors with available Rx packet buffer
      pointers.

  @  Parameters
      wpt_msg   *rxPktAvailMsg
                 RX frame ready MSG pointer
                 include DXE control context

  @  Return
      NONE

===========================================================================*/
void dxeRXPacketAvailableEventHandler
(
   wpt_msg                 *rxPktAvailMsg
)
{
   WLANDXE_CtrlBlkType      *dxeCtxt    = NULL;
   wpt_status                status     = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_ChannelCBType    *channelCb  = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity Check */
   if(NULL == rxPktAvailMsg)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXPacketAvailableEventHandler Context is not valid");
      return;
   }

   dxeCtxt    = (WLANDXE_CtrlBlkType *)(rxPktAvailMsg->pContext);

#ifdef WLAN_DXE_LOW_RESOURCE_TIMER
   /* Available resource allocated
    * Stop timer not needed */
   if(VOS_TIMER_STATE_RUNNING ==
      wpalTimerGetCurStatus(&dxeCtxt->rxResourceAvailableTimer))
   {
      wpalTimerStop(&dxeCtxt->rxResourceAvailableTimer);
   }
#endif

   do
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "dxeRXPacketAvailableEventHandler, start refilling ring");

      channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_HIGH_PRI];
      status = dxeRXFrameRefillRing(dxeCtxt,channelCb);
   
      // Wait for another callback to indicate when Rx resources are available
      // again.
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         break;
      }
   
      channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_RX_LOW_PRI];
      status = dxeRXFrameRefillRing(dxeCtxt,channelCb);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         break;
      }
   } while(0);

   if((WLANDXE_POWER_STATE_IMPS == dxeCtxt->hostPowerState) ||
      (WLANDXE_POWER_STATE_DOWN == dxeCtxt->hostPowerState))
   {
      /* Interrupt will not enabled at here, it will be enabled at PS mode change */
      tempDxeCtrlBlk->rxIntDisabledByIMPS = eWLAN_PAL_TRUE;
   }
}

/*==========================================================================
  @  Function Name 
      dxeRXISR

  @  Description 
      RX frame ready interrupt service routine
      interrupt entry function, this function called based on ISR context
      Must be serialized

  @  Parameters
      void    *hostCtxt
               DXE host driver control context,
               pre registerd during interrupt registration

  @  Return
      NONE

===========================================================================*/
static void dxeRXISR
(
   void                    *hostCtxt
)
{
   WLANDXE_CtrlBlkType      *dxeCtxt    = (WLANDXE_CtrlBlkType *)hostCtxt;
   wpt_status                status     = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                regValue;

#ifdef FEATURE_R33D
   status = wpalReadRegister(WLANDXE_INT_SRC_RAW_ADDRESS,
                                  &regValue);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXCompISR Read INT_SRC_RAW fail");
      return;
   }
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
            "INT_SRC_RAW 0x%x", regValue);
   if(0 == regValue)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "This is not DXE Interrupt, Reject it 0x%x", regValue);
      return;
   }
#endif /* FEATURE_R33D */

   /* Set Interrupt processing bit
    * During this bit set, WLAN HW may not power collapse */
   wpalReadRegister(WLANDXE_INT_MASK_REG_ADDRESS, &regValue);
   regValue |= WLANPAL_RX_INTERRUPT_PRO_MASK;
   wpalWriteRegister(WLANDXE_INT_MASK_REG_ADDRESS, regValue);

   /* Disable interrupt at here
    * Disable RX Ready system level Interrupt at here
    * Otherwise infinite loop might happen */
   status = wpalDisableInterrupt(DXE_INTERRUPT_RX_READY);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXFrameReadyISR Disable RX ready interrupt fail");
      return;         
   }

   /* Serialize RX Ready interrupt upon RX thread */
   if(NULL == dxeCtxt->rxIsrMsg)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeRXFrameReadyISR NULL message");
      HDXE_ASSERT(0);
      return;
   }

   status = wpalPostRxMsg(WDI_GET_PAL_CTX(),
                          dxeCtxt->rxIsrMsg);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "dxeRXFrameReadyISR interrupt serialize fail");
   }

   return;
}

/*==========================================================================
  @  Function Name 
      dxeTXPushFrame

  @  Description
      Push TX frame into DXE descriptor and DXE register
      Send notification to DXE register that TX frame is ready to transfer

  @  Parameters
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block
      wpt_packet              *palPacket
                               Packet pointer ready to transfer

  @  Return
      PAL_STATUS_T
===========================================================================*/
static wpt_status dxeTXPushFrame
(
   WLANDXE_ChannelCBType   *channelEntry,
   wpt_packet              *palPacket
)
{
   wpt_status                  status = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_DescCtrlBlkType    *currentCtrlBlk = NULL;
   WLANDXE_DescType           *currentDesc    = NULL;
   WLANDXE_DescType           *firstDesc      = NULL;
   WLANDXE_DescType           *LastDesc       = NULL;
   void                       *sourcePhysicalAddress = NULL;
   wpt_uint32                  xferSize = 0;
#ifdef FEATURE_R33D
   tx_frm_pcie_vector_t        frameVector;
   wpt_uint32                  Va;
   wpt_uint32                  fragCount = 0;
#else
   wpt_iterator                iterator;
#endif /* FEATURE_R33D */
   wpt_uint32                  isEmpty = 0;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   tempDxeCtrlBlk->smsmToggled = eWLAN_PAL_FALSE;
   if((0 == tempDxeCtrlBlk->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI].numRsvdDesc) &&
      (0 == tempDxeCtrlBlk->dxeChannel[WDTS_CHANNEL_TX_HIGH_PRI].numRsvdDesc))
   {
      isEmpty = 1;
   }

   channelEntry->numFragmentCurrentChain = 0;
   currentCtrlBlk = channelEntry->headCtrlBlk;

   /* Initialize interator, TX is fragmented */
#ifdef FEATURE_R33D
   memset(&frameVector, 0, sizeof(tx_frm_pcie_vector_t));
   status = wpalPrepareTxFrame(palPacket,
                                    &frameVector,
                                    &Va);
#else
   status = wpalLockPacketForTransfer(palPacket);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXPushFrame unable to lock packet");
      return status;
   }

   status = wpalIteratorInit(&iterator, palPacket);
#endif /* FEATURE_R33D */
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXPushFrame iterator init fail");
      return status;
   }

   /* !!!! Revisit break condition !!!!!!! */
   while(1)
   {
      /* Get current descriptor pointer from current control block */
      currentDesc = currentCtrlBlk->linkedDesc;
      if(NULL == firstDesc)
      {
         firstDesc = currentCtrlBlk->linkedDesc;
      }
      /* All control block will have same palPacket Pointer
       * to make logic simpler */
      currentCtrlBlk->xfrFrame = palPacket;

      /* Get next fragment physical address and fragment size
       * if this is the first trial, will get first physical address
       * if no more fragment, Descriptor src address will be set as NULL, OK??? */
#ifdef FEATURE_R33D
      if(fragCount == frameVector.num_frg)
      {
         break;
      }
      currentCtrlBlk->shadowBufferVa = frameVector.frg[0].va;
      sourcePhysicalAddress          = (void *)frameVector.frg[fragCount].pa;
      xferSize                       = frameVector.frg[fragCount].size;
      fragCount++;
      if(0 == xferSize)
      {
          HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXPushFrame invalid transfer size");

          HDXE_ASSERT(0);
          return eWLAN_PAL_STATUS_E_FAILURE;
      }
      if(NULL == sourcePhysicalAddress)
      {
          HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "dxeTXPushFrame invalid sourcePhysicalAddress");
          HDXE_ASSERT(0);
          return eWLAN_PAL_STATUS_E_FAILURE;
      }
#else
      status = wpalIteratorNext(&iterator,
                                palPacket,
                                &sourcePhysicalAddress,
                                &xferSize);
      if((NULL == sourcePhysicalAddress) ||
         (0    == xferSize))
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
                  "dxeTXPushFrame end of current frame");
         break;
      }
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXPushFrame Get next frame fail");
         return status;
      }
#endif /* FEATURE_R33D */

      /* This is the LAST descriptor valid for this transaction */
      LastDesc    = currentCtrlBlk->linkedDesc;

      /* Program DXE descriptor */
      currentDesc->dxedesc.dxe_short_desc.srcMemAddrL =
                               WLANDXE_U32_SWAP_ENDIAN((wpt_uint32)(uintptr_t)sourcePhysicalAddress);

      /* Just normal data transfer from aCPU Flat Memory to BMU Q */
      if((WDTS_CHANNEL_TX_LOW_PRI  == channelEntry->channelType) ||
         (WDTS_CHANNEL_TX_HIGH_PRI == channelEntry->channelType))
      {
         currentDesc->dxedesc.dxe_short_desc.dstMemAddrL =
                                WLANDXE_U32_SWAP_ENDIAN(channelEntry->channelConfig.refWQ);
      }
      else
      {
         /* Test specific H2H transfer, destination address already set
          * Do Nothing */
      }
      currentDesc->xfrSize = WLANDXE_U32_SWAP_ENDIAN(xferSize);

      /* Program channel control register */
      /* First frame not set VAL bit, why ??? */
      if(0 == channelEntry->numFragmentCurrentChain)
      {
         currentDesc->descCtrl.ctrl = channelEntry->extraConfig.cw_ctrl_write;
      }
      else
      {
         currentDesc->descCtrl.ctrl = channelEntry->extraConfig.cw_ctrl_write_valid;
      }

      /* Update statistics */
      channelEntry->numFragmentCurrentChain++;
      channelEntry->numFreeDesc--;
      channelEntry->numRsvdDesc++;

      /* Get next control block */
      currentCtrlBlk = currentCtrlBlk->nextCtrlBlk;
   }
   channelEntry->numTotalFrame++;
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "NUM TX FRAG %d, Total Frame %d",
            channelEntry->numFragmentCurrentChain, channelEntry->numTotalFrame);

   /* Program Channel control register
    * Set as end of packet
    * Enable interrupt also for first code lock down
    * performace optimization, this will be revisited */
   if(NULL == LastDesc)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXPushFrame NULL Last Descriptor, broken chain");
      return eWLAN_PAL_STATUS_E_FAULT;
   }
   LastDesc->descCtrl.ctrl  = channelEntry->extraConfig.cw_ctrl_write_eop_int;
   /* Now First one also Valid ????
    * this procedure will prevent over handle descriptor from previous
    * TX trigger */
   firstDesc->descCtrl.ctrl = channelEntry->extraConfig.cw_ctrl_write_valid;

   /* If in BMPS mode no need to notify the DXE Engine, notify SMSM instead */
   if(WLANDXE_RIVA_POWER_STATE_BMPS_UNKNOWN == tempDxeCtrlBlk->rivaPowerState)
   {
      /* Update channel head as next avaliable linked slot */
      channelEntry->headCtrlBlk = currentCtrlBlk;
      if(isEmpty)
      {
         tempDxeCtrlBlk->ringNotEmpty = eWLAN_PAL_TRUE;
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
                  "SMSM_ret LO=%d HI=%d",
                  tempDxeCtrlBlk->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI].numRsvdDesc,
                  tempDxeCtrlBlk->dxeChannel[WDTS_CHANNEL_TX_HIGH_PRI].numRsvdDesc );
         dxeNotifySmsm(eWLAN_PAL_TRUE, eWLAN_PAL_FALSE);
         tempDxeCtrlBlk->smsmToggled = eWLAN_PAL_TRUE;
      }
      return status;
   }

   /* If DXE use external descriptor, registers are not needed to be programmed
    * Just after finish to program descriptor, tirigger to send */
   if(channelEntry->extraConfig.chan_mask & WLANDXE_CH_CTRL_EDEN_MASK)
   {
      /* Issue a dummy read from the DXE descriptor DDR location to
         ensure that any previously posted write to the descriptor
         completes. */
      if(channelEntry->extraConfig.cw_ctrl_write_valid != firstDesc->descCtrl.ctrl)
      {
         //HDXE_ASSERT(0);
      }

      /* Everything is ready
       * Trigger to start DMA */
      status = wpalWriteRegister(channelEntry->channelRegister.chDXECtrlRegAddr,
                                      channelEntry->extraConfig.chan_mask);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXPushFrame Write Channel Ctrl Register fail");
         return status;
      }

      /* Update channel head as next avaliable linked slot */
      channelEntry->headCtrlBlk = currentCtrlBlk;

      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
               "%s Exit", __func__);
      return status;
   }

   /* If DXE not use external descriptor, program each registers */
   /* Circular buffer handle not need to program DESC register???
    * GEN5 code not programed RING buffer case
    * REVISIT THIS !!!!!! */
   if((WDTS_CHANNEL_TX_LOW_PRI  == channelEntry->channelType) ||
      (WDTS_CHANNEL_TX_HIGH_PRI == channelEntry->channelType))
   {
      /* Destination address, assigned Work Q */
      status = wpalWriteRegister(channelEntry->channelRegister.chDXEDadrlRegAddr,
                                      channelEntry->channelConfig.refWQ);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXPushFrame Program dest address register fail");
         return status;
      }
      /* If descriptor format is SHORT */
      if(channelEntry->channelConfig.useShortDescFmt)
      {
         status = wpalWriteRegister(channelEntry->channelRegister.chDXEDadrhRegAddr,
                                         0);
         if(eWLAN_PAL_STATUS_SUCCESS != status)
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "dxeTXPushFrame Program dest address register fail");
            return status;
         }
      }
      else
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXPushFrame LONG Descriptor Format!!!");
      }
   }
#ifdef WLANDXE_TEST_CHANNEL_ENABLE
   else if(WDTS_CHANNEL_H2H_TEST_TX  == channelEntry->channelType)
   {
      /* Destination address, Physical memory address */
      status = wpalWriteRegister(channelEntry->channelRegister.chDXEDadrlRegAddr,
                                      WLANDXE_U32_SWAP_ENDIAN(firstDesc->dxedesc.dxe_short_desc.dstMemAddrL));
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXPushFrame Program dest address register fail");
         return status;
      }
      /* If descriptor format is SHORT */
      if(channelEntry->channelConfig.useShortDescFmt)
      {
         status = wpalWriteRegister(channelEntry->channelRegister.chDXEDadrhRegAddr,
                                         0);
         if(eWLAN_PAL_STATUS_SUCCESS != status)
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "dxeTXPushFrame Program dest address register fail");
            return status;
         }
      }
      else
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXPushFrame LONG Descriptor Format!!!");
      }
   }
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */

   /* Program Source address register
    * This address is already programmed into DXE Descriptor
    * But register also upadte */
   status = wpalWriteRegister(channelEntry->channelRegister.chDXESadrlRegAddr,
                                   WLANDXE_U32_SWAP_ENDIAN(firstDesc->dxedesc.dxe_short_desc.srcMemAddrL));
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXPushFrame Program src address register fail");
      return status;
   }
   /* If descriptor format is SHORT */
   if(channelEntry->channelConfig.useShortDescFmt)
   {
      status = wpalWriteRegister(channelEntry->channelRegister.chDXESadrhRegAddr,
                                      0);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXPushFrame Program dest address register fail");
         return status;
      }
   }
   else
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXPushFrame LONG Descriptor Format!!!");
   }

   /* Linked list Descriptor pointer */
   status = wpalWriteRegister(channelEntry->channelRegister.chDXEDesclRegAddr,
                                   channelEntry->headCtrlBlk->linkedDescPhyAddr);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXPushFrame Write DESC Address register fail");
      return status;
   }
   /* If descriptor format is SHORT */
   if(channelEntry->channelConfig.useShortDescFmt)
   {
      status = wpalWriteRegister(channelEntry->channelRegister.chDXEDeschRegAddr,
                                      0);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXPushFrame Program dest address register fail");
         return status;
      }
   }
   else
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXPushFrame LONG Descriptor Format!!!");
   }

   /* Transfer Size */
   xferSize = WLANDXE_U32_SWAP_ENDIAN(firstDesc->xfrSize);
   status = wpalWriteRegister(channelEntry->channelRegister.chDXESzRegAddr,
                                   xferSize);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXPushFrame Write DESC Address register fail");
      return status;
   }

   /* Everything is ready
    * Trigger to start DMA */
   status = wpalWriteRegister(channelEntry->channelRegister.chDXECtrlRegAddr,
                                   channelEntry->extraConfig.chan_mask);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXPushFrame Write Channel Ctrl Register fail");
      return status;
   }

   /* Update channel head as next avaliable linked slot */
   channelEntry->headCtrlBlk = currentCtrlBlk;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      dxeTXCompFrame

  @  Description 
      TX Frame transfer complete event handler

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      PAL_STATUS_T
===========================================================================*/
static wpt_status dxeTXCompFrame
(
   WLANDXE_CtrlBlkType     *hostCtxt,
   WLANDXE_ChannelCBType   *channelEntry
)
{
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_DescCtrlBlkType  *currentCtrlBlk = NULL;
   WLANDXE_DescType         *currentDesc    = NULL;
   wpt_uint32                descCtrlValue  = 0;
   unsigned int             *lowThreshold   = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity */
   if((NULL == hostCtxt) || (NULL == channelEntry))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXCompFrame Invalid ARG");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if(NULL == hostCtxt->txCompCB)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXCompFrame TXCompCB is not registered");
      return eWLAN_PAL_STATUS_SUCCESS;
   }

   status = wpalMutexAcquire(&channelEntry->dxeChannelLock);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXCompFrame Mutex Acquire fail");
      return status;
   }
   
   currentCtrlBlk = channelEntry->tailCtrlBlk;
   currentDesc    = currentCtrlBlk->linkedDesc;

   if( currentCtrlBlk == channelEntry->headCtrlBlk )
   {
      status = wpalMutexRelease(&channelEntry->dxeChannelLock);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXCompFrame Mutex Release fail");
         return status;
      }
      return eWLAN_PAL_STATUS_SUCCESS;
   }


   while(1)
   {
//      HDXE_ASSERT(WLAN_PAL_IS_STATUS_SUCCESS(WLAN_RivaValidateDesc(currentDesc)));
      descCtrlValue = currentDesc->descCtrl.ctrl;
      if((descCtrlValue & WLANDXE_DESC_CTRL_VALID))
      {
         /* caught up with head, bail out */
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED,
                  "dxeTXCompFrame caught up with head - next DESC has VALID set");
         break;
      }

      if(currentCtrlBlk->xfrFrame == NULL)
      {
          HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "Invalid transfer frame");
          HDXE_ASSERT(0);
          break;
      }
      channelEntry->numFreeDesc++;
      channelEntry->numRsvdDesc--;

      /* Send Frame TX Complete notification with frame start fragment location */
      if(WLANDXE_U32_SWAP_ENDIAN(descCtrlValue) & WLANDXE_DESC_CTRL_EOP)
      {
         hostCtxt->txCompletedFrames--;
#ifdef FEATURE_R33D
         wpalFreeTxFrame(currentCtrlBlk->shadowBufferVa);
#else
         status = wpalUnlockPacket(currentCtrlBlk->xfrFrame);
         if (eWLAN_PAL_STATUS_SUCCESS != status)
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "dxeRXFrameReady unable to unlock packet");
            status = wpalMutexRelease(&channelEntry->dxeChannelLock);
            if(eWLAN_PAL_STATUS_SUCCESS != status)
            {
               HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                        "dxeTXCompFrame Mutex Release fail");
            }
            return status;
         }
#endif /* FEATURE_R33D */
         hostCtxt->txCompCB(hostCtxt->clientCtxt,
                            currentCtrlBlk->xfrFrame,
                            eWLAN_PAL_STATUS_SUCCESS);
         channelEntry->numFragmentCurrentChain = 0;
      }
      currentCtrlBlk = currentCtrlBlk->nextCtrlBlk;
      currentDesc    = currentCtrlBlk->linkedDesc;

      /* Break condition
       * Head control block is the control block must be programed for the next TX
       * so, head control block is not programmed control block yet
       * if loop encounte head control block, stop to complete
       * in theory, COMP CB must be called already ??? */
      if(currentCtrlBlk == channelEntry->headCtrlBlk)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED,
                  "dxeTXCompFrame caught up with head ptr");
         break;
      }
      /* VALID Bit check ???? */
   }

   /* Tail and Head Control block must be same */
   channelEntry->tailCtrlBlk = currentCtrlBlk;

   lowThreshold = channelEntry->channelType == WDTS_CHANNEL_TX_LOW_PRI?
      &(hostCtxt->txCompInt.txLowResourceThreshold_LoPriCh):
      &(hostCtxt->txCompInt.txLowResourceThreshold_HiPriCh);

   /* If specific channel hit low resource condition send notification to upper layer */
   if((eWLAN_PAL_TRUE == channelEntry->hitLowResource) &&
      (channelEntry->numFreeDesc > *lowThreshold))
   {
      /* Change it back if we raised it for fetching a remaining packet from TL */
      if(WLANDXE_TX_LOW_RES_THRESHOLD > *lowThreshold)
      {
         *lowThreshold = WLANDXE_TX_LOW_RES_THRESHOLD;
      }

      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "DXE TX %d channel recovered from low resource", channelEntry->channelType);
      hostCtxt->lowResourceCB(hostCtxt->clientCtxt,
                              channelEntry->channelType,
                              eWLAN_PAL_TRUE);
      channelEntry->hitLowResource = eWLAN_PAL_FALSE;
      wpalTimerStop(&channelEntry->healthMonitorTimer);
   }

   status = wpalMutexRelease(&channelEntry->dxeChannelLock);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXCompFrame Mutex Release fail");
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      dxeTXEventHandler

  @  Description 
      If DXE HW sends TX related interrupt, this event handler will be called
      Handle higher priority channel first
      Figureout why interrupt happen and call appropriate final even handler
      TX complete or error happen

  @  Parameters
         void               *msgPtr
                             Even MSG

  @  Return
      PAL_STATUS_T
===========================================================================*/
void dxeTXEventHandler
(
   wpt_msg               *msgPtr
)
{
   wpt_msg                  *msgContent = (wpt_msg *)msgPtr;
   WLANDXE_CtrlBlkType      *dxeCtxt    = NULL;
   wpt_status                status     = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                intSrc     = 0;
   wpt_uint32                chStat     = 0;
   WLANDXE_ChannelCBType    *channelCb  = NULL;

   wpt_uint8                 bEnableISR = 0;
   static wpt_uint8          successiveIntWithIMPS;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   dxeCtxt = (WLANDXE_CtrlBlkType *)(msgContent->pContext);
   dxeCtxt->ucTxMsgCnt = 0;
   
   if(eWLAN_PAL_TRUE == dxeCtxt->driverReloadInProcessing)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "wlan: TX COMP WLAN Driver re-loading in progress");
      return;
   }

   /* Return from here if the RIVA is in IMPS, to avoid register access */
   if(WLANDXE_POWER_STATE_IMPS == dxeCtxt->hostPowerState)
   {
      successiveIntWithIMPS++;
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXEventHandler IMPS TX COMP INT successiveIntWithIMPS %d", successiveIntWithIMPS);
      status = dxeTXCompFrame(dxeCtxt, &dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_HIGH_PRI]);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXEventHandler IMPS HC COMP interrupt fail");
      }

      status = dxeTXCompFrame(dxeCtxt, &dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI]);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXEventHandler IMPS LC COMP interrupt fail");
      }

      if(((dxeCtxt->txCompletedFrames) &&
         (eWLAN_PAL_FALSE == dxeCtxt->txIntEnable)) &&
         (successiveIntWithIMPS == 1))
      {
         dxeCtxt->txIntEnable =  eWLAN_PAL_TRUE; 
         wpalEnableInterrupt(DXE_INTERRUPT_TX_COMPLE);
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                     "TX COMP INT Enabled, remain TX frame count on ring %d",
                     dxeCtxt->txCompletedFrames);
         /*Kicking the DXE after the TX Complete interrupt was enabled - to avoid 
           the posibility of a race*/
         dxePsComplete(dxeCtxt, eWLAN_PAL_TRUE);
      }
      else
      {
         dxeCtxt->txIntEnable =  eWLAN_PAL_FALSE;
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "TX COMP INT NOT Enabled, RIVA still wake up? remain TX frame count on ring %d, successiveIntWithIMPS %d",
                  dxeCtxt->txCompletedFrames, successiveIntWithIMPS);
      }
      return;
   }

   successiveIntWithIMPS = 0;
   if((!dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_HIGH_PRI].extraConfig.chEnabled) ||
      (!dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI].extraConfig.chEnabled))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
         "DXE already stopped in TX event handler. Just return");
      return;
   }

   /* Disable device interrupt */
   /* Read whole interrupt mask register and exclusive only this channel int */
   status = wpalReadRegister(WLANDXE_INT_SRC_RAW_ADDRESS,
                                  &intSrc);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXCompleteEventHandler Read INT_DONE_SRC register fail");
      return;         
   }
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_MED,
            "TX Event Handler INT Source 0x%x", intSrc);

   /* Test High Priority Channel is the INT source or not */
   channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_HIGH_PRI];
   if(intSrc & (1 << channelCb->assignedDMAChannel))
   {
      status = dxeChannelCleanInt(channelCb, &chStat);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXEventHandler INT Clean up fail");
         return;         
      }

      if(WLANDXE_CH_STAT_INT_ERR_MASK & chStat)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                  "%11s : 0x%x Error Reported, Reload Driver",
                  channelType[channelCb->channelType], chStat);

         dxeErrChannelDebug(channelCb, chStat);

         dxeCtxt->driverReloadInProcessing = eWLAN_PAL_TRUE;
         wpalWlanReload();
         dxeStartSSRTimer(dxeCtxt);
      }
      else if(WLANDXE_CH_STAT_INT_DONE_MASK & chStat)
      {
         /* Handle TX complete for high priority channel */
         status = dxeTXCompFrame(dxeCtxt,
                                 channelCb);
         bEnableISR = 1;
      }
      else if(WLANDXE_CH_STAT_INT_ED_MASK & chStat)
      {
         /* Handle TX complete for high priority channel */
         status = dxeTXCompFrame(dxeCtxt,
                                 channelCb);
         bEnableISR = 1;
      }
      else
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "dxeTXEventHandler TX HI status=%x", chStat);
      }

      if(WLANDXE_CH_STAT_MASKED_MASK & chStat)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_HIGH,
                  "dxeTXEventHandler TX HIGH Channel Masked Unmask it!!!!");
      }

      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_HIGH,
               "TX HIGH STAT 0x%x RESRVD %d", chStat, channelCb->numRsvdDesc);
   }

   /* Test Low Priority Channel interrupt is enabled or not */
   channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI];
   if(intSrc & (1 << channelCb->assignedDMAChannel))
   {
      status = dxeChannelCleanInt(channelCb, &chStat);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeTXEventHandler INT Clean up fail");
         return;         
      }

      if(WLANDXE_CH_STAT_INT_ERR_MASK & chStat)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                  "%11s : 0x%x Error Reported, Reload Driver",
                  channelType[channelCb->channelType], chStat);

         dxeErrChannelDebug(channelCb, chStat);

         dxeCtxt->driverReloadInProcessing = eWLAN_PAL_TRUE;
         wpalWlanReload();
         dxeStartSSRTimer(dxeCtxt);
      }
      else if(WLANDXE_CH_STAT_INT_DONE_MASK & chStat)
      {
         /* Handle TX complete for low priority channel */
         status = dxeTXCompFrame(dxeCtxt,
                                 channelCb);
         bEnableISR = 1;
      }
      else if(WLANDXE_CH_STAT_INT_ED_MASK & chStat)
      {
         /* Handle TX complete for low priority channel */
         status = dxeTXCompFrame(dxeCtxt,
                                 channelCb);
         bEnableISR = 1;
      }
      else
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "dxeTXEventHandler TX LO status=%x", chStat);
      }

      if(WLANDXE_CH_STAT_MASKED_MASK & chStat)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_HIGH,
                  "dxeTXEventHandler TX Low Channel Masked Unmask it!!!!");
      }
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,
               "TX LOW STAT 0x%x RESRVD %d", chStat, channelCb->numRsvdDesc);
   }


#ifdef WLANDXE_TEST_CHANNEL_ENABLE
   /* Test H2H TX Channel interrupt is enabled or not */
   channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_H2H_TEST_TX];
   if(intSrc & (1 << channelCb->assignedDMAChannel))
   {
      status = wpalReadRegister(channelCb->channelRegister.chDXEStatusRegAddr,
                                     &chStat);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeChannelCleanInt Read CH STAT register fail");
         return;         
      }

      if(WLANDXE_CH_STAT_INT_ERR_MASK & chStat)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                  "%11s : 0x%x Error Reported, Reload Driver",
                  channelType[channelCb->channelType], chStat);

         dxeErrChannelDebug(channelCb, chStat);

         dxeCtxt->driverReloadInProcessing = eWLAN_PAL_TRUE;
         wpalWlanReload();
         dxeStartSSRTimer(dxeCtxt);
      }
      else if(WLANDXE_CH_STAT_INT_DONE_MASK & chStat)
      {
         /* Handle TX complete for high priority channel */
         status = dxeTXCompFrame(dxeCtxt,
                                 channelCb);
         if(eWLAN_PAL_STATUS_SUCCESS != status)
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "dxeTXEventHandler INT Clean up fail");
            return;         
         }
      }
      else
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "unexpected channel state %d", chStat);
      }
   }
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */

   if((bEnableISR || (dxeCtxt->txCompletedFrames)) &&
      (eWLAN_PAL_FALSE == dxeCtxt->txIntEnable))
   {
      dxeCtxt->txIntEnable =  eWLAN_PAL_TRUE; 
      wpalEnableInterrupt(DXE_INTERRUPT_TX_COMPLE);
      if(0 != dxeCtxt->txCompletedFrames)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "TX COMP INT Enabled, remain TX frame count on ring %d",
                  dxeCtxt->txCompletedFrames);
      }
   }

   /*Kicking the DXE after the TX Complete interrupt was enabled - to avoid 
     the posibility of a race*/
   dxePsComplete(dxeCtxt, eWLAN_PAL_TRUE);

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return;
}


/*==========================================================================
  @  Function Name 
      dxeTXCompleteProcessing

  @  Description 
      If DXE HW sends TX related interrupt, this event handler will be called
      Handle higher priority channel first
      Figureout why interrupt happen and call appropriate final even handler
      TX complete or error happen

  @  Parameters
      dxeCtxt      DXE context 

  @  Return
      PAL_STATUS_T
===========================================================================*/
void dxeTXCompleteProcessing
(
   WLANDXE_CtrlBlkType *dxeCtxt
)
{
   wpt_status                status     = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_ChannelCBType    *channelCb  = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);
  
   /* Test High Priority Channel is the INT source or not */
   channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_HIGH_PRI];

   /* Handle TX complete for high priority channel */
   status = dxeTXCompFrame(dxeCtxt, channelCb);

   /* Test Low Priority Channel interrupt is enabled or not */
   channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI];

   /* Handle TX complete for low priority channel */
   status = dxeTXCompFrame(dxeCtxt, channelCb);
  
   if((eWLAN_PAL_FALSE == dxeCtxt->txIntEnable) &&
      ((dxeCtxt->txCompletedFrames > 0) ||
       (WLANDXE_POWER_STATE_FULL == dxeCtxt->hostPowerState)))
   {
      dxeCtxt->txIntEnable =  eWLAN_PAL_TRUE; 
      wpalEnableInterrupt(DXE_INTERRUPT_TX_COMPLE);
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "%s %s : %d, %s : %d", __func__,
               channelType[dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_HIGH_PRI].channelType],
               dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_HIGH_PRI].numRsvdDesc,
               channelType[dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI].channelType],
               dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI].numRsvdDesc);

      if((WLANDXE_POWER_STATE_FULL != dxeCtxt->hostPowerState) &&
         (eWLAN_PAL_FALSE == tempDxeCtrlBlk->smsmToggled))
      {
         /* After TX Comp processing, still remaining frame on the DXE TX ring
          * And when push frame, RING was not empty marked
          * Then when push frame, no SMSM toggle happen
          * To avoid permanent TX stall, SMSM toggle is needed at here
          * With this toggle, host should gaurantee SMSM state should be changed */
         dxeNotifySmsm(eWLAN_PAL_TRUE, dxeCtxt->txRingsEmpty);
      }
   }
   
   /*Kicking the DXE after the TX Complete interrupt was enabled - to avoid 
     the posibility of a race*/
   dxePsComplete(dxeCtxt, eWLAN_PAL_FALSE);
   
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return;
}

/*==========================================================================
  @  Function Name
      dxeTXReSyncDesc

  @  Description
      When STA comeout from IMPS, check DXE TX next transfer candidate descriptor
      And HW programmed descriptor.
      If any async happen between HW/SW TX stall will happen

  @  Parameters
      void    *msgPtr
               Message pointer to sync with TX thread

  @  Return
      NONE
===========================================================================*/
void dxeTXReSyncDesc
(
   wpt_msg                  *msgPtr
)
{
   wpt_msg                  *msgContent = (wpt_msg *)msgPtr;
   WLANDXE_CtrlBlkType      *pDxeCtrlBlk;
   wpt_uint32                nextDescReg;
   WLANDXE_ChannelCBType    *channelEntry;
   WLANDXE_DescCtrlBlkType  *validCtrlBlk;
   wpt_uint32                descLoop;
   wpt_uint32                channelLoop;

   if(NULL == msgContent)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXReSyncDesc Invalid Control Block");
      return;
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "dxeTXReSyncDesc Try to re-sync TX channel if any problem");
   pDxeCtrlBlk = (WLANDXE_CtrlBlkType *)(msgContent->pContext);

   for(channelLoop = WDTS_CHANNEL_TX_LOW_PRI; channelLoop < WDTS_CHANNEL_RX_LOW_PRI; channelLoop++)
   {
      channelEntry = &pDxeCtrlBlk->dxeChannel[channelLoop];
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
               "%11s : Try to detect TX descriptor async", channelType[channelEntry->channelType]);
      wpalReadRegister(channelEntry->channelRegister.chDXEDesclRegAddr,
                       &nextDescReg);
      /* Async detect without TX pending frame */
      if(channelEntry->tailCtrlBlk == channelEntry->headCtrlBlk)
      {
         if(nextDescReg != channelEntry->tailCtrlBlk->linkedDescPhyAddr)
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                     "TX Async no Pending frame");

            dxeChannelMonitor("!!! TX Async no Pending frame !!!", channelEntry, NULL);
            dxeChannelRegisterDump(channelEntry, "!!! TX Async no Pending frame !!!", NULL);

            wpalWriteRegister(channelEntry->channelRegister.chDXEDesclRegAddr,
                              channelEntry->tailCtrlBlk->linkedDescPhyAddr);
         }
      }
      /* Async detect with some TX pending frames
       * next descriptor register should sync with first valid descriptor */
      else
      {
         validCtrlBlk = channelEntry->tailCtrlBlk;
         for(descLoop = 0; descLoop < channelEntry->numDesc; descLoop++)
         {
            if(validCtrlBlk->linkedDesc->descCtrl.ctrl & WLANDXE_DESC_CTRL_VALID)
            {
               if(nextDescReg != validCtrlBlk->linkedDescPhyAddr)
               {
                  HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                           "TX Async");

                  dxeChannelMonitor("!!! TX Async !!!", channelEntry, NULL);
                  dxeChannelRegisterDump(channelEntry, "!!! TX Async !!!", NULL);

                  wpalWriteRegister(channelEntry->channelRegister.chDXEDesclRegAddr,
                                    validCtrlBlk->linkedDescPhyAddr);
               }
               break;
            }
            validCtrlBlk = (WLANDXE_DescCtrlBlkType *)validCtrlBlk->nextCtrlBlk;
            if(validCtrlBlk == channelEntry->headCtrlBlk->nextCtrlBlk)
            {
               /* Finished to test till head control blcok, but could not find valid descriptor
                * from head to tail all descriptors are invalidated
                * host point of view head descriptor is next TX candidate
                * So, next descriptor control have to be programmed with head descriptor
                * check */
               if(nextDescReg != channelEntry->headCtrlBlk->linkedDescPhyAddr)
               {
                  HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                           "TX Async with not completed transferred frames, next descriptor must be head");

                  dxeChannelMonitor("!!! TX Async !!!", channelEntry, NULL);
                  dxeChannelRegisterDump(channelEntry, "!!! TX Async !!!", NULL);

                  wpalWriteRegister(channelEntry->channelRegister.chDXEDesclRegAddr,
                                    validCtrlBlk->linkedDescPhyAddr);
               }
               break;
            }
         }
      }
   }

   /* HW/SW descriptor resync is done.
    * Next if there are any valid descriptor in chain, Push to HW again */
   for(channelLoop = WDTS_CHANNEL_TX_LOW_PRI; channelLoop < WDTS_CHANNEL_RX_LOW_PRI; channelLoop++)
   {
      channelEntry = &pDxeCtrlBlk->dxeChannel[channelLoop];
      if(channelEntry->tailCtrlBlk == channelEntry->headCtrlBlk)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
                  "%11s : No TX Pending frame",
                  channelType[channelEntry->channelType]);
         /* No Pending frame, Do nothing */
      }
      else
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                  "%11s : TX Pending frame, process it",
                  channelType[channelEntry->channelType]);
         validCtrlBlk = channelEntry->tailCtrlBlk;
         for(descLoop = 0; descLoop < channelEntry->numDesc; descLoop++)
         {
            if(validCtrlBlk->linkedDesc->descCtrl.ctrl & WLANDXE_DESC_CTRL_VALID)
            {
               HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                        "%11s : when exit IMPS found valid descriptor",
                        channelType[channelEntry->channelType]);

               /* Found valid descriptor, kick DXE */
               wpalWriteRegister(channelEntry->channelRegister.chDXECtrlRegAddr,
                                 channelEntry->extraConfig.chan_mask);
               break;
            }
            validCtrlBlk = (WLANDXE_DescCtrlBlkType *)validCtrlBlk->nextCtrlBlk;
            if(validCtrlBlk == channelEntry->headCtrlBlk->nextCtrlBlk)
            {
               /* Finished to test till head control blcok, but could not find valid descriptor
                * from head to tail all descriptors are invalidated */
               break;
            }
         }
      }
   }

   wpalMemoryFree(msgPtr);
   return;
}

/*==========================================================================
  @  Function Name
      dxeDebugTxDescReSync

  @  Description
       Check DXE Tx channel state and correct it in
       case Tx Data stall is detected by calling
       %dxeTXReSyncDesc. Also ensure that WCN SS
       is not power collapsed before calling
       %dxeTXReSyncDesc

  @  Parameters
      void    *msgPtr
               Message pointer to sync with TX thread

  @  Return
      NONE
===========================================================================*/
void dxeDebugTxDescReSync
(
   wpt_msg                  *msgPtr
)
{
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "%s: Check for DXE TX Async",__func__);
   /* Make wake up HW */
   dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
   dxeNotifySmsm(eWLAN_PAL_TRUE, eWLAN_PAL_FALSE);

   wpalSleep(10);

   dxeTXReSyncDesc(msgPtr);
}
/*==========================================================================
  @  Function Name 
      dxeTXISR

  @  Description 
      TX interrupt ISR
      Platform will call this function if INT is happen
      This function must be registered into platform interrupt module

  @  Parameters
      void    *hostCtxt
               DXE host driver control context,
               pre registerd during interrupt registration

  @  Return
      PAL_STATUS_T
===========================================================================*/
static void dxeTXISR
(
   void                    *hostCtxt
)
{
   WLANDXE_CtrlBlkType      *dxeCtxt    = (WLANDXE_CtrlBlkType *)hostCtxt;
   wpt_status                status  = eWLAN_PAL_STATUS_SUCCESS;
#ifdef FEATURE_R33D
   wpt_uint32                regValue;
#endif /* FEATURE_R33D */

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Return from here if the RIVA is in IMPS, to avoid register access */
   if(WLANDXE_POWER_STATE_DOWN == dxeCtxt->hostPowerState)
   {
      dxeCtxt->txIntEnable = eWLAN_PAL_FALSE;
      /* Disable interrupt at here,
         IMPS or IMPS Pending state should not access RIVA register */
      status = wpalDisableInterrupt(DXE_INTERRUPT_TX_COMPLE);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "dxeRXFrameReadyISR Disable RX ready interrupt fail");
         return;         
      }
      dxeCtxt->txIntDisabledByIMPS = eWLAN_PAL_TRUE;
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
         "%s Riva is in %d, return from here ", __func__, dxeCtxt->hostPowerState);
      return;
   }

#ifdef FEATURE_R33D
   status = wpalReadRegister(WLANDXE_INT_SRC_RAW_ADDRESS,
                                  &regValue);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXCompISR Read INT_SRC_RAW fail");
      return;
   }
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "INT_SRC_RAW 0x%x", regValue);
   if(0 == regValue)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "This is not DXE Interrupt, Reject it");
      return;
   }
#endif /* FEATURE_R33D */

   /* Disable TX Complete Interrupt at here */
   status = wpalDisableInterrupt(DXE_INTERRUPT_TX_COMPLE);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXCompISR Disable TX complete interrupt fail");
      return;         
   }
   dxeCtxt->txIntEnable = eWLAN_PAL_FALSE;


   if( dxeCtxt->ucTxMsgCnt )
   {
    HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,
                 "Avoiding serializing TX Complete event");
    return;
   }
   
   dxeCtxt->ucTxMsgCnt = 1;

   /* Serialize TX complete interrupt upon TX thread */
   if(NULL == dxeCtxt->txIsrMsg)
   {
       HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "Invalid message");
       HDXE_ASSERT(0);
       return;
   }
   status = wpalPostTxMsg(WDI_GET_PAL_CTX(),
                          dxeCtxt->txIsrMsg);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "dxeTXCompISR interrupt serialize fail status=%d", status);
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return;
}

/*-------------------------------------------------------------------------
 *  Global Function
 *-------------------------------------------------------------------------*/
/*==========================================================================
  @  Function Name 
      WLANDXE_Open

  @  Description 
      Open host DXE driver, allocate DXE resources
      Allocate, DXE local control block, DXE descriptor pool, DXE descriptor control block pool

  @  Parameters
      pVoid      pAdapter : Driver global control block pointer

  @  Return
      pVoid DXE local module control block pointer
===========================================================================*/
void *WLANDXE_Open
(
   void
)
{
   wpt_status              status = eWLAN_PAL_STATUS_SUCCESS;
   unsigned int            idx;
   WLANDXE_ChannelCBType  *currentChannel = NULL;
   int                     smsmInitState;
#ifdef WLANDXE_TEST_CHANNEL_ENABLE
   wpt_uint32                 sIdx;
   WLANDXE_ChannelCBType     *channel = NULL;
   WLANDXE_DescCtrlBlkType   *crntDescCB = NULL;
   WLANDXE_DescCtrlBlkType   *nextDescCB = NULL;
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* This is temporary allocation */
   tempDxeCtrlBlk = (WLANDXE_CtrlBlkType *)wpalMemoryAllocate(sizeof(WLANDXE_CtrlBlkType));
   if(NULL == tempDxeCtrlBlk)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Open Control Block Alloc Fail");
      return NULL;  
   }
   wpalMemoryZero(tempDxeCtrlBlk, sizeof(WLANDXE_CtrlBlkType));

   status = dxeCommonDefaultConfig(tempDxeCtrlBlk);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Open Common Configuration Fail");
      WLANDXE_Close(tempDxeCtrlBlk);
      return NULL;         
   }

   for(idx = 0; idx < WDTS_CHANNEL_MAX; idx++)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "WLANDXE_Open Channel %s Open Start", channelType[idx]);
      currentChannel = &tempDxeCtrlBlk->dxeChannel[idx];
      if(idx == WDTS_CHANNEL_TX_LOW_PRI)
      {
         currentChannel->channelType = WDTS_CHANNEL_TX_LOW_PRI;
      }
      else if(idx == WDTS_CHANNEL_TX_HIGH_PRI)
      {
         currentChannel->channelType = WDTS_CHANNEL_TX_HIGH_PRI;
      }
      else if(idx == WDTS_CHANNEL_RX_LOW_PRI)
      {
         currentChannel->channelType = WDTS_CHANNEL_RX_LOW_PRI;
      }
      else if(idx == WDTS_CHANNEL_RX_HIGH_PRI)
      {
         currentChannel->channelType = WDTS_CHANNEL_RX_HIGH_PRI;
      }
#ifdef WLANDXE_TEST_CHANNEL_ENABLE
      else if(idx == WDTS_CHANNEL_H2H_TEST_TX)
      {
         currentChannel->channelType = WDTS_CHANNEL_H2H_TEST_TX;
      }
      else if(idx == WDTS_CHANNEL_H2H_TEST_RX)
      {
         currentChannel->channelType = WDTS_CHANNEL_H2H_TEST_RX;
      }
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */

      /* Config individual channels from channel default setup table */
      status = dxeChannelDefaultConfig(tempDxeCtrlBlk,
                                       currentChannel);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "WLANDXE_Open Channel Basic Configuration Fail for channel %d", idx);
         WLANDXE_Close(tempDxeCtrlBlk);
         return NULL;         
      }

      /* Allocate DXE Control Block will be used by host DXE driver */
      status = dxeCtrlBlkAlloc(tempDxeCtrlBlk, currentChannel);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "WLANDXE_Open Alloc DXE Control Block Fail for channel %d", idx);

         WLANDXE_Close(tempDxeCtrlBlk);
         return NULL;         
      }
      status = wpalMutexInit(&currentChannel->dxeChannelLock); 
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "WLANDXE_Open Lock Init Fail for channel %d", idx);
         WLANDXE_Close(tempDxeCtrlBlk);
         return NULL;
      }

      status = wpalTimerInit(&currentChannel->healthMonitorTimer,
                    dxeHealthMonitorTimeout,
                    (void *)currentChannel);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "WLANDXE_Open Health Monitor timer init fail %d", idx);
         WLANDXE_Close(tempDxeCtrlBlk);
         return NULL;
      }

      currentChannel->healthMonitorMsg = (wpt_msg *)wpalMemoryAllocate(sizeof(wpt_msg));
      if(NULL == currentChannel->healthMonitorMsg)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "WLANDXE_Open Health Monitor MSG Alloc fail %d", idx);
         WLANDXE_Close(tempDxeCtrlBlk);
         return NULL;
      }
      wpalMemoryZero(currentChannel->healthMonitorMsg, sizeof(wpt_msg));
      currentChannel->healthMonitorMsg->callback = dxeTXHealthMonitor;
      currentChannel->healthMonitorMsg->pContext = (void *)currentChannel;

      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "WLANDXE_Open Channel %s Open Success", channelType[idx]);
   }

   /* Allocate and Init RX READY ISR Serialize Buffer */
   tempDxeCtrlBlk->rxIsrMsg = (wpt_msg *)wpalMemoryAllocate(sizeof(wpt_msg));
   if(NULL == tempDxeCtrlBlk->rxIsrMsg)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Open Alloc RX ISR Fail");
      WLANDXE_Close(tempDxeCtrlBlk);
      return NULL;
   }
   wpalMemoryZero(tempDxeCtrlBlk->rxIsrMsg, sizeof(wpt_msg));
   tempDxeCtrlBlk->rxIsrMsg->callback = dxeRXEventHandler;
   tempDxeCtrlBlk->rxIsrMsg->pContext = (void *)tempDxeCtrlBlk;

   /* Allocate and Init TX COMP ISR Serialize Buffer */
   tempDxeCtrlBlk->txIsrMsg = (wpt_msg *)wpalMemoryAllocate(sizeof(wpt_msg));
   if(NULL == tempDxeCtrlBlk->txIsrMsg)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Open Alloc TX ISR Fail");
      WLANDXE_Close(tempDxeCtrlBlk);
      return NULL;
   }
   wpalMemoryZero(tempDxeCtrlBlk->txIsrMsg, sizeof(wpt_msg));
   tempDxeCtrlBlk->txIsrMsg->callback = dxeTXEventHandler;
   tempDxeCtrlBlk->txIsrMsg->pContext = (void *)tempDxeCtrlBlk;

   /* Allocate and Init RX Packet Available Serialize Message Buffer */
   tempDxeCtrlBlk->rxPktAvailMsg = (wpt_msg *)wpalMemoryAllocate(sizeof(wpt_msg));
   if(NULL == tempDxeCtrlBlk->rxPktAvailMsg)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Open Alloc RX Packet Available Message Fail");
      WLANDXE_Close(tempDxeCtrlBlk);
      return NULL;
   }
   wpalMemoryZero(tempDxeCtrlBlk->rxPktAvailMsg, sizeof(wpt_msg));
   tempDxeCtrlBlk->rxPktAvailMsg->callback = dxeRXPacketAvailableEventHandler;
   tempDxeCtrlBlk->rxPktAvailMsg->pContext = (void *)tempDxeCtrlBlk;
   
   tempDxeCtrlBlk->freeRXPacket = NULL;
   tempDxeCtrlBlk->dxeCookie    = WLANDXE_CTXT_COOKIE;
   tempDxeCtrlBlk->rxIntDisabledByIMPS = eWLAN_PAL_FALSE;
   tempDxeCtrlBlk->txIntDisabledByIMPS = eWLAN_PAL_FALSE;
   tempDxeCtrlBlk->driverReloadInProcessing = eWLAN_PAL_FALSE;
   tempDxeCtrlBlk->smsmToggled              = eWLAN_PAL_FALSE;

   /* Initialize SMSM state
    * Init State is
    *    Clear TX Enable
    *    RING EMPTY STATE */
   smsmInitState = wpalNotifySmsm(WPAL_SMSM_WLAN_TX_ENABLE,
                                  WPAL_SMSM_WLAN_TX_RINGS_EMPTY);
   if(0 != smsmInitState)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "SMSM Channel init fail %d", smsmInitState);
      for(idx = 0; idx < WDTS_CHANNEL_MAX; idx++)
      {
         dxeChannelClose(tempDxeCtrlBlk, &tempDxeCtrlBlk->dxeChannel[idx]);
      }
      wpalMemoryFree(tempDxeCtrlBlk->rxIsrMsg);
      wpalMemoryFree(tempDxeCtrlBlk->txIsrMsg);
      wpalMemoryFree(tempDxeCtrlBlk);
      return NULL;
   }

#ifdef WLAN_DXE_LOW_RESOURCE_TIMER
   wpalTimerInit(&tempDxeCtrlBlk->rxResourceAvailableTimer,
                 dxeRXResourceAvailableTimerExpHandler,
                 tempDxeCtrlBlk);
#endif

   wpalTimerInit(&tempDxeCtrlBlk->dxeSSRTimer,
                 dxeSSRTimerExpHandler, tempDxeCtrlBlk);

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
            "WLANDXE_Open Success");
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return (void *)tempDxeCtrlBlk;
}

/*==========================================================================
  @  Function Name 
      WLANDXE_ClientRegistration

  @  Description 
      Make callback functions registration into DXE driver from DXE driver client

  @  Parameters
      pVoid                       pDXEContext : DXE module control block
      WDTS_RxFrameReadyCbType     rxFrameReadyCB : RX Frame ready CB function pointer
      WDTS_TxCompleteCbType       txCompleteCB : TX complete CB function pointer
      WDTS_LowResourceCbType      lowResourceCB : Low DXE resource notification CB function pointer
      void                       *userContext : DXE Cliennt control block

  @  Return
      wpt_status
===========================================================================*/
wpt_status WLANDXE_ClientRegistration
(
   void                       *pDXEContext,
   WLANDXE_RxFrameReadyCbType  rxFrameReadyCB,
   WLANDXE_TxCompleteCbType    txCompleteCB,
   WLANDXE_LowResourceCbType   lowResourceCB,
   void                       *userContext
)
{
   wpt_status                 status  = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_CtrlBlkType       *dxeCtxt;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity */
   if(NULL == pDXEContext)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_ClientRegistration Invalid DXE CB");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   if(NULL == rxFrameReadyCB)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_ClientRegistration Invalid RX READY CB");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   if(NULL == txCompleteCB)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_ClientRegistration Invalid txCompleteCB");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   if(NULL == lowResourceCB)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_ClientRegistration Invalid lowResourceCB");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   if(NULL == userContext)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_ClientRegistration Invalid userContext");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   dxeCtxt = (WLANDXE_CtrlBlkType *)pDXEContext;

   /* Assign */
   dxeCtxt->rxReadyCB     = rxFrameReadyCB;
   dxeCtxt->txCompCB      = txCompleteCB;
   dxeCtxt->lowResourceCB = lowResourceCB;
   dxeCtxt->clientCtxt    = userContext;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      WLANDXE_Start

  @  Description 
      Start Host DXE driver
      Initialize DXE channels and start channel

  @  Parameters
      pVoid                       pDXEContext : DXE module control block

  @  Return
      wpt_status
===========================================================================*/
wpt_status WLANDXE_Start
(
   void  *pDXEContext
)
{
   wpt_status                 status  = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                 idx;
   WLANDXE_CtrlBlkType       *dxeCtxt = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity */
   if(NULL == pDXEContext)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Start Invalid DXE CB");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }
   dxeCtxt = (WLANDXE_CtrlBlkType *)pDXEContext;

   /* WLANDXE_Start called means DXE engine already initiates
    * And DXE HW is reset and init finished
    * But here to make sure HW is initialized, reset again */
   status = dxeEngineCoreStart(dxeCtxt);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Start DXE HW init Fail");
      return status;         
   }

   /* Individual Channel Start */
   for(idx = 0; idx < WDTS_CHANNEL_MAX; idx++)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "WLANDXE_Start Channel %s Start", channelType[idx]);

      /* Allocate DXE descriptor will be shared by Host driver and DXE engine */
      /* Make connection between DXE descriptor and DXE control block */
      status = dxeDescAllocAndLink(tempDxeCtrlBlk, &dxeCtxt->dxeChannel[idx]);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "WLANDXE_Start Alloc DXE Descriptor Fail for channel %d", idx);
         return status;         
      }

      /* Program each channel register with configuration arguments */
      status = dxeChannelInitProgram(dxeCtxt,
                                     &dxeCtxt->dxeChannel[idx]);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "WLANDXE_Start %d Program DMA channel Fail", idx);
         return status;         
      }

      /* ??? Trigger to start DMA channel
       * This must be seperated from ??? */
      status = dxeChannelStart(dxeCtxt,
                               &dxeCtxt->dxeChannel[idx]);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "WLANDXE_Start %d Channel Start Fail", idx);
         return status;         
      }
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "WLANDXE_Start Channel %s Start Success", channelType[idx]);
   }

   /* Register ISR to OS */
   /* Register TX complete interrupt into platform */
   status = wpalRegisterInterrupt(DXE_INTERRUPT_TX_COMPLE,
                                       dxeTXISR,
                                       dxeCtxt);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Start TX comp interrupt registration Fail");
      return status;         
   }

   /* Register RX ready interrupt into platform */
   status = wpalRegisterInterrupt(DXE_INTERRUPT_RX_READY,
                                       dxeRXISR,
                                       dxeCtxt);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Start RX Ready interrupt registration Fail");
      return status;         
   }

   /* Enable system level ISR */
   /* Enable RX ready Interrupt at here */
   status = wpalEnableInterrupt(DXE_INTERRUPT_RX_READY);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeTXCompleteEventHandler Enable TX complete interrupt fail");
      return status;         
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      WLANDXE_TXFrame

  @  Description 
      Trigger frame transmit from host to RIVA

  @  Parameters
      pVoid            pDXEContext : DXE Control Block
      wpt_packet       pPacket : transmit packet structure
      WDTS_ChannelType channel : TX channel

  @  Return
      wpt_status
===========================================================================*/
wpt_status WLANDXE_TxFrame
(
   void                *pDXEContext,
   wpt_packet          *pPacket,
   WDTS_ChannelType     channel
)
{
   wpt_status                 status         = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_ChannelCBType     *currentChannel = NULL;
   WLANDXE_CtrlBlkType       *dxeCtxt        = NULL;
   unsigned int              *lowThreshold   = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity */
   if(NULL == pDXEContext)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Start Invalid DXE CB");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   if(NULL == pPacket)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Start Invalid pPacket");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   if((WDTS_CHANNEL_MAX < channel) || (WDTS_CHANNEL_MAX == channel))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Start Invalid channel");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   dxeCtxt = (WLANDXE_CtrlBlkType *)pDXEContext;

   currentChannel = &dxeCtxt->dxeChannel[channel];
   

   status = wpalMutexAcquire(&currentChannel->dxeChannelLock);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_TxFrame Mutex Acquire fail");
      return status;
   }

   lowThreshold = currentChannel->channelType == WDTS_CHANNEL_TX_LOW_PRI?
      &(dxeCtxt->txCompInt.txLowResourceThreshold_LoPriCh):
      &(dxeCtxt->txCompInt.txLowResourceThreshold_HiPriCh);

   /* Decide have to activate TX complete event or not */
   switch(dxeCtxt->txCompInt.txIntEnable)
   {
      /* TX complete interrupt will be activated when low DXE resource */
      case WLANDXE_TX_COMP_INT_LR_THRESHOLD:
         if((currentChannel->numFreeDesc <= *lowThreshold) &&
            (eWLAN_PAL_FALSE == dxeCtxt->txIntEnable))
         {
            dxeCtxt->txIntEnable = eWLAN_PAL_TRUE;
            dxeCtxt->lowResourceCB(dxeCtxt->clientCtxt,
                                   channel,
                                   eWLAN_PAL_FALSE);
         }
         break;

      /* TX complete interrupt will be activated n number of frames transferred */
      case WLANDXE_TX_COMP_INT_PER_K_FRAMES:
         if(channel == WDTS_CHANNEL_TX_LOW_PRI)
         {
            currentChannel->numFrameBeforeInt++;
         }
         break;

      /* TX complete interrupt will be activated periodically */
      case WLANDXE_TX_COMP_INT_TIMER:
         break;
   }

   dxeCtxt->txCompletedFrames++;

   /* Update DXE descriptor, this is frame based
    * if a frame consist of N fragments, N Descriptor will be programed */
   status = dxeTXPushFrame(currentChannel, pPacket);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_TxFrame TX Push Frame fail");
      status = wpalMutexRelease(&currentChannel->dxeChannelLock);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "WLANDXE_TxFrame Mutex Release fail");
      }
      return status;
   }

   /* If specific channel hit low resource condition, send notification to upper layer */
   if(currentChannel->numFreeDesc <= *lowThreshold)
   {
      dxeCtxt->lowResourceCB(dxeCtxt->clientCtxt,
                             channel,
                             eWLAN_PAL_FALSE);
      currentChannel->hitLowResource = eWLAN_PAL_TRUE;

      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_WARN,
               "%11s : Low Resource currentChannel->numRsvdDesc %d",
               channelType[currentChannel->channelType],
               currentChannel->numRsvdDesc);
      if (WLANDXE_RIVA_POWER_STATE_BMPS_UNKNOWN == dxeCtxt->rivaPowerState)
      {
         dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
         dxeNotifySmsm(eWLAN_PAL_TRUE, eWLAN_PAL_FALSE);
      }
      wpalTimerStart(&currentChannel->healthMonitorTimer,
                     T_WLANDXE_PERIODIC_HEALTH_M_TIME);
   }
   status = wpalMutexRelease(&currentChannel->dxeChannelLock);
   if(eWLAN_PAL_STATUS_SUCCESS != status)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_TxFrame Mutex Release fail");
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      WLANDXE_CompleteTX

  @  Description 
      Informs DXE that the current series of Tx packets is complete

  @  Parameters
      pContext            pDXEContext : DXE Control Block
      ucTxResReq          TX resource number required by TL/WDI

  @  Return
      wpt_status
===========================================================================*/
wpt_status
WLANDXE_CompleteTX
(
  void* pContext,
  wpt_uint32 ucTxResReq
)
{
  wpt_status                status  = eWLAN_PAL_STATUS_SUCCESS;
  WLANDXE_CtrlBlkType      *dxeCtxt = (WLANDXE_CtrlBlkType *)(pContext);
  WLANDXE_ChannelCBType    *channelCb  = NULL;
  wpt_boolean               inLowRes;

  /* Sanity Check */
  if( NULL == pContext )
  {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_CompleteTX invalid param");
      return eWLAN_PAL_STATUS_E_INVAL;
  }

  channelCb = &dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI];
  inLowRes  = channelCb->hitLowResource;

  if(WLANDXE_TX_LOW_RES_THRESHOLD < ucTxResReq)
  {
    /* Raise threshold temporarily if necessary */
    dxeCtxt->txCompInt.txLowResourceThreshold_LoPriCh = ucTxResReq;

    if(eWLAN_PAL_FALSE == inLowRes)
    {
      /* Put the channel to low resource condition */
      dxeCtxt->lowResourceCB(dxeCtxt->clientCtxt,
                             WDTS_CHANNEL_TX_LOW_PRI,
                             eWLAN_PAL_FALSE);
      inLowRes = channelCb->hitLowResource = eWLAN_PAL_TRUE;
      wpalTimerStart(&channelCb->healthMonitorTimer,
                     T_WLANDXE_PERIODIC_HEALTH_M_TIME);
    }
  }

  /*Try to reclaim resources*/
  dxeTXCompleteProcessing(dxeCtxt);

  /* In previous WLANTL_GetFrames call, TL didn't fetch a packet 
     because its fragment size is larger than DXE free resource. */
  if(0 < ucTxResReq)
  {
    /* DXE successfully claimed enough free DXE resouces for next fetch. */
    if(WLANDXE_GetFreeTxDataResNumber(dxeCtxt) >= ucTxResReq)
    {
      /* DXE has not been in low resource condition. DXE forces to kick off
         TX tranmit */
      if((eWLAN_PAL_FALSE == inLowRes) && 
         (eWLAN_PAL_FALSE == channelCb->hitLowResource))
      {
        dxeCtxt->lowResourceCB(dxeCtxt->clientCtxt,
                               WDTS_CHANNEL_TX_LOW_PRI,
                               eWLAN_PAL_FALSE);
        dxeCtxt->lowResourceCB(dxeCtxt->clientCtxt,
                               WDTS_CHANNEL_TX_LOW_PRI,
                               eWLAN_PAL_TRUE);
        channelCb->hitLowResource = eWLAN_PAL_FALSE;
      }
    }
    else
    {
      /* DXE doesn't have enough free DXE resources. Put the channel
         to low resource condition. */
      if(eWLAN_PAL_FALSE == channelCb->hitLowResource)
      {
        /* Put the channel to low resource condition */
        dxeCtxt->lowResourceCB(dxeCtxt->clientCtxt,
                             WDTS_CHANNEL_TX_LOW_PRI,
                             eWLAN_PAL_FALSE);
        channelCb->hitLowResource = eWLAN_PAL_TRUE;
        wpalTimerStart(&channelCb->healthMonitorTimer,
                       T_WLANDXE_PERIODIC_HEALTH_M_TIME);
      }
    }
  }
 
  return status; 
}

/*==========================================================================
  @  Function Name 
      WLANDXE_Stop

  @  Description 
      Stop DXE channels and DXE engine operations
      Disable all channel interrupt
      Stop all channel operation

  @  Parameters
      pVoid            pDXEContext : DXE Control Block

  @  Return
      wpt_status
===========================================================================*/
wpt_status WLANDXE_Stop
(
   void *pDXEContext
)
{
   wpt_status                 status = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                 idx;
   WLANDXE_CtrlBlkType       *dxeCtxt = NULL;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity */
   if(NULL == pDXEContext)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Stop Invalid DXE CB");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   dxeCtxt = (WLANDXE_CtrlBlkType *)pDXEContext;
   for(idx = 0; idx < WDTS_CHANNEL_MAX; idx++)
   {
      if(VOS_TIMER_STATE_RUNNING == wpalTimerGetCurStatus(&dxeCtxt->dxeChannel[idx].healthMonitorTimer))
      {
         wpalTimerStop(&dxeCtxt->dxeChannel[idx].healthMonitorTimer);
      }

      status = dxeChannelStop(dxeCtxt, &dxeCtxt->dxeChannel[idx]);
      if(eWLAN_PAL_STATUS_SUCCESS != status)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "WLANDXE_Stop Channel %d Stop Fail", idx);
      }
   }

   /* During Stop unregister interrupt */
   wpalUnRegisterInterrupt(DXE_INTERRUPT_TX_COMPLE);
   wpalUnRegisterInterrupt(DXE_INTERRUPT_RX_READY);

#ifdef WLAN_DXE_LOW_RESOURCE_TIMER
   if(VOS_TIMER_STATE_STOPPED !=
      wpalTimerGetCurStatus(&dxeCtxt->rxResourceAvailableTimer))
   {
      wpalTimerStop(&dxeCtxt->rxResourceAvailableTimer);
   }
#endif

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      WLANDXE_Close

  @  Description 
      Close DXE channels
      Free DXE related resources
      DXE descriptor free
      Descriptor control block free
      Pre allocated RX buffer free

  @  Parameters
      pVoid            pDXEContext : DXE Control Block

  @  Return
      wpt_status
===========================================================================*/
wpt_status WLANDXE_Close
(
   void *pDXEContext
)
{
   wpt_status               status = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                 idx;
   WLANDXE_CtrlBlkType       *dxeCtxt = NULL;
#ifdef WLANDXE_TEST_CHANNEL_ENABLE
   wpt_uint32                 sIdx;
   WLANDXE_ChannelCBType     *channel = NULL;
   WLANDXE_DescCtrlBlkType   *crntDescCB = NULL;
   WLANDXE_DescCtrlBlkType   *nextDescCB = NULL;
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Sanity */
   if(NULL == pDXEContext)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WLANDXE_Stop Invalid DXE CB");
      return eWLAN_PAL_STATUS_E_INVAL;   
   }

   dxeCtxt = (WLANDXE_CtrlBlkType *)pDXEContext;
#ifdef WLAN_DXE_LOW_RESOURCE_TIMER
   wpalTimerDelete(&dxeCtxt->rxResourceAvailableTimer);
#endif
   wpalTimerDelete(&dxeCtxt->dxeSSRTimer);
   for(idx = 0; idx < WDTS_CHANNEL_MAX; idx++)
   {
      wpalMutexDelete(&dxeCtxt->dxeChannel[idx].dxeChannelLock);
      wpalTimerDelete(&dxeCtxt->dxeChannel[idx].healthMonitorTimer);
      if(NULL != dxeCtxt->dxeChannel[idx].healthMonitorMsg)
      {
         wpalMemoryFree(dxeCtxt->dxeChannel[idx].healthMonitorMsg);
      }
      dxeChannelClose(dxeCtxt, &dxeCtxt->dxeChannel[idx]);
#ifdef WLANDXE_TEST_CHANNEL_ENABLE
      channel    = &dxeCtxt->dxeChannel[idx];
      crntDescCB = channel->headCtrlBlk;
      for(sIdx = 0; sIdx < channel->numDesc; sIdx++)
      {
         nextDescCB = (WLANDXE_DescCtrlBlkType *)crntDescCB->nextCtrlBlk;
         wpalMemoryFree((void *)crntDescCB);
         crntDescCB = nextDescCB;
         if(NULL == crntDescCB)
         {
            break;
         }
      }
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */
   }

   if(NULL != dxeCtxt->rxIsrMsg)
   {
      wpalMemoryFree(dxeCtxt->rxIsrMsg);
   }
   if(NULL != dxeCtxt->txIsrMsg)
   {
      wpalMemoryFree(dxeCtxt->txIsrMsg);
   }
   if(NULL != dxeCtxt->rxPktAvailMsg)
   {
      wpalMemoryFree(dxeCtxt->rxPktAvailMsg);
   }

   wpalMemoryFree(pDXEContext);

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      WLANDXE_TriggerTX

  @  Description 
      TBD

  @  Parameters
      pVoid            pDXEContext : DXE Control Block

  @  Return
      wpt_status
===========================================================================*/
wpt_status WLANDXE_TriggerTX
(
   void *pDXEContext
)
{
   wpt_status               status = eWLAN_PAL_STATUS_SUCCESS;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* TBD */

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return status;
}

/*==========================================================================
  @  Function Name 
      dxeTxThreadSetPowerStateEventHandler

  @  Description 
      If WDI sends set power state req, this event handler will be called in Tx
      thread context

  @  Parameters
         void               *msgPtr
                             Event MSG

  @  Return
      None
===========================================================================*/
void dxeTxThreadSetPowerStateEventHandler
(
    wpt_msg               *msgPtr
)
{
   wpt_msg                  *msgContent = (wpt_msg *)msgPtr;
   WLANDXE_CtrlBlkType      *dxeCtxt;
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_PowerStateType    reqPowerState;
   wpt_int8                  i;
   WLANDXE_ChannelCBType     *channelEntry;
   wpt_log_data_stall_channel_type channelLog;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   dxeCtxt = (WLANDXE_CtrlBlkType *)(msgContent->pContext);
   reqPowerState = (WLANDXE_PowerStateType)msgContent->val;
   dxeCtxt->setPowerStateCb = (WLANDXE_SetPowerStateCbType)msgContent->ptr;

   switch(reqPowerState)
   {
      case WLANDXE_POWER_STATE_BMPS:
         if(WLANDXE_RIVA_POWER_STATE_ACTIVE == dxeCtxt->rivaPowerState)
         {
            //don't block MC waiting for num_rsvd to become 0 since it may take a while
            //based on amount of TX and RX activity - during this time any received 
            // management frames will remain un-processed consuming RX buffers
            dxeCtxt->rivaPowerState = WLANDXE_RIVA_POWER_STATE_BMPS_UNKNOWN;
            dxeCtxt->hostPowerState = reqPowerState;
         }
         else
         {
            status = eWLAN_PAL_STATUS_E_INVAL;
         }
         break;
      case WLANDXE_POWER_STATE_IMPS:
         if(WLANDXE_RIVA_POWER_STATE_ACTIVE == dxeCtxt->rivaPowerState)
         {

            for(i = WDTS_CHANNEL_TX_LOW_PRI; i < WDTS_CHANNEL_RX_LOW_PRI; i++)
            {
               channelEntry = &dxeCtxt->dxeChannel[i];
               if(channelEntry->tailCtrlBlk != channelEntry->headCtrlBlk)
               {
                  status = eWLAN_PAL_STATUS_E_FAILURE;
                  HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
                           "%11s : %s :TX Pending frame",
                           channelType[channelEntry->channelType], __func__);

                  dxeChannelMonitor("DXE_IMP_ERR", channelEntry, &channelLog);
                  dxeDescriptorDump(channelEntry,
                                    channelEntry->headCtrlBlk->linkedDesc, 0);
                  dxeChannelRegisterDump(channelEntry, "DXE_IMPS_ERR",
                                         &channelLog);
                  dxeChannelAllDescDump(channelEntry,
                                        channelEntry->channelType,
                                        &channelLog);
               }
            }

            if (eWLAN_PAL_STATUS_SUCCESS == status)
            {
               dxeCtxt->rivaPowerState = WLANDXE_RIVA_POWER_STATE_IMPS_UNKNOWN;
               dxeCtxt->hostPowerState = WLANDXE_POWER_STATE_IMPS;
            }
         }
         else
         {
            status = eWLAN_PAL_STATUS_E_INVAL;
         }
         break;
      case WLANDXE_POWER_STATE_FULL:
         if(WLANDXE_RIVA_POWER_STATE_BMPS_UNKNOWN == dxeCtxt->rivaPowerState)
         {
            dxeCtxt->rivaPowerState = WLANDXE_RIVA_POWER_STATE_ACTIVE;
         }
         dxeCtxt->hostPowerState = reqPowerState;
         dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
         break;
      case WLANDXE_POWER_STATE_DOWN:
         WLANDXE_Stop((void *)dxeCtxt);         
         break;
      default:
         //assert
         break;
   }

   if(WLANDXE_POWER_STATE_BMPS_PENDING != dxeCtxt->hostPowerState)
   {
      dxeCtxt->setPowerStateCb(status, 
                               dxeCtxt->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI].descBottomLocPhyAddr);
   }
   else
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "%s State of DXE is WLANDXE_POWER_STATE_BMPS_PENDING, so cannot proceed", __func__);
   }
   /* Free MSG buffer */
   wpalMemoryFree(msgPtr);
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
   return;
}


/*==========================================================================
  @  Function Name 
      dxeRxThreadSetPowerStateEventHandler

  @  Description 
      If WDI sends set power state req, this event handler will be called in Rx
      thread context

  @  Parameters
         void               *msgPtr
                             Event MSG

  @  Return
      None
===========================================================================*/
void dxeRxThreadSetPowerStateEventHandler
(
    wpt_msg               *msgPtr
)
{
   wpt_status               status = eWLAN_PAL_STATUS_SUCCESS;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   /* Now serialise the message through Tx thread also to make sure
    * no register access when RIVA is in powersave */
   /*Use the same message pointer just change the call back function */
   msgPtr->callback = dxeTxThreadSetPowerStateEventHandler;
   status = wpalPostTxMsg(WDI_GET_PAL_CTX(),
                       msgPtr);
   if ( eWLAN_PAL_STATUS_SUCCESS != status )
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Tx thread Set power state req serialize fail status=%d",
               status);
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);
}

/*==========================================================================
  @  Function Name 
      WLANDXE_SetPowerState

  @  Description 
      From Client let DXE knows what is the WLAN HW(RIVA) power state

  @  Parameters
      pVoid                    pDXEContext : DXE Control Block
      WLANDXE_PowerStateType   powerState

  @  Return
      wpt_status
===========================================================================*/
wpt_status WLANDXE_SetPowerState
(
   void                    *pDXEContext,
   WDTS_PowerStateType      powerState,
   WDTS_SetPSCbType         cBack
)
{
   wpt_status               status = eWLAN_PAL_STATUS_SUCCESS;
   WLANDXE_CtrlBlkType     *pDxeCtrlBlk;
   WLANDXE_PowerStateType   hostPowerState;
   wpt_msg                 *rxCompMsg;
   wpt_msg                 *txDescReSyncMsg;

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);
   if(NULL == pDXEContext)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "NULL pDXEContext passed by caller");
      return eWLAN_PAL_STATUS_E_FAILURE;
   }
   pDxeCtrlBlk = (WLANDXE_CtrlBlkType *)pDXEContext;

   switch(powerState)
   {
      case WDTS_POWER_STATE_FULL:
         if(WLANDXE_POWER_STATE_IMPS == pDxeCtrlBlk->hostPowerState)
         {
            txDescReSyncMsg = (wpt_msg *)wpalMemoryAllocate(sizeof(wpt_msg));
            if(NULL == txDescReSyncMsg)
            {
               HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                        "WLANDXE_SetPowerState, TX Resync MSG MEM alloc Fail");
            }
            else
            {
               txDescReSyncMsg->callback = dxeTXReSyncDesc;
               txDescReSyncMsg->pContext = pDxeCtrlBlk;
               status = wpalPostTxMsg(WDI_GET_PAL_CTX(),
                                      txDescReSyncMsg);
               if(eWLAN_PAL_STATUS_SUCCESS != status)
               {
                  HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                           "WLANDXE_SetPowerState, Post TX re-sync MSG fail");
               }
            }
         }
         hostPowerState = WLANDXE_POWER_STATE_FULL;
         break;
      case WDTS_POWER_STATE_BMPS:
         pDxeCtrlBlk->hostPowerState = WLANDXE_POWER_STATE_BMPS;
         hostPowerState = WLANDXE_POWER_STATE_BMPS;
         break;
      case WDTS_POWER_STATE_IMPS:
         hostPowerState = WLANDXE_POWER_STATE_IMPS;
         break;
      case WDTS_POWER_STATE_DOWN:
         pDxeCtrlBlk->hostPowerState = WLANDXE_POWER_STATE_DOWN;
         hostPowerState = WLANDXE_POWER_STATE_DOWN;
         break;
      default:
         hostPowerState = WLANDXE_POWER_STATE_MAX;
   }

   // A callback i.e. ACK back is needed only when we want to enable BMPS
   // and the data/management path is active because we want to ensure
   // DXE registers are not accessed when RIVA may be power-collapsed. So
   // we need a callback in enter_bmps_req (the request to RIVA is sent
   // only after ACK back from TX thread). A callback is not needed in
   // finish_scan_req during BMPS since data-path is resumed only in 
   // finish_scan_rsp and no management frames are sent in between. No 
   // callback is needed when going from BMPS enabled to BMPS suspended/
   // disabled when it is known that RIVA is awake and cannot enter power
   // collapse autonomously so no callback is needed in exit_bmps_rsp or
   // init_scan_rsp
   if ( cBack )
   {
      //serialize through Rx thread
      rxCompMsg          = (wpt_msg *)wpalMemoryAllocate(sizeof(wpt_msg));
      if(NULL == rxCompMsg)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "WLANDXE_SetPowerState, MSG MEM alloc Fail");
         return eWLAN_PAL_STATUS_E_RESOURCES;
      }

      /* Event type, where it must be defined???? */
      /* THIS MUST BE CLEARED ASAP
      txCompMsg->type     = TX_COMPLETE; */
      rxCompMsg->callback = dxeRxThreadSetPowerStateEventHandler;
      rxCompMsg->pContext = pDxeCtrlBlk;
      rxCompMsg->val      = hostPowerState;
      rxCompMsg->ptr      = cBack;
      status = wpalPostRxMsg(WDI_GET_PAL_CTX(),
                          rxCompMsg);
      if ( eWLAN_PAL_STATUS_SUCCESS != status )
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "Rx thread Set power state req serialize fail status=%d",
                  status);
      }
   }
   else
   {
      if ( WLANDXE_POWER_STATE_FULL == hostPowerState )
      {
         if( WLANDXE_POWER_STATE_BMPS == pDxeCtrlBlk->hostPowerState )
         {
            dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
         }
         else if( WLANDXE_POWER_STATE_IMPS == pDxeCtrlBlk->hostPowerState )
         {
            /* Requested Full power from exit IMPS, reenable the interrupts*/
            if(eWLAN_PAL_TRUE == pDxeCtrlBlk->rxIntDisabledByIMPS)
            {
               pDxeCtrlBlk->rxIntDisabledByIMPS = eWLAN_PAL_FALSE;
               /* Enable RX interrupt at here, if new PS is not IMPS */
               status = wpalEnableInterrupt(DXE_INTERRUPT_RX_READY);
               if(eWLAN_PAL_STATUS_SUCCESS != status)
               {
                  HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                           "%s Enable RX ready interrupt fail", __func__);
                  return status;
               }
            }
            if(eWLAN_PAL_TRUE == pDxeCtrlBlk->txIntDisabledByIMPS)
            {
               pDxeCtrlBlk->txIntDisabledByIMPS = eWLAN_PAL_FALSE;
               pDxeCtrlBlk->txIntEnable =  eWLAN_PAL_TRUE;
               /* Enable RX interrupt at here, if new PS is not IMPS */
               status = wpalEnableInterrupt(DXE_INTERRUPT_TX_COMPLE);
               if(eWLAN_PAL_STATUS_SUCCESS != status)
               {
                  HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                           "%s Enable TX comp interrupt fail", __func__);
                  return status;
               }
            }
         }
         pDxeCtrlBlk->hostPowerState = hostPowerState;
         pDxeCtrlBlk->rivaPowerState = WLANDXE_RIVA_POWER_STATE_ACTIVE;
      }
      else if ( hostPowerState == WLANDXE_POWER_STATE_BMPS )
      {
         pDxeCtrlBlk->hostPowerState = hostPowerState;
         pDxeCtrlBlk->rivaPowerState = WLANDXE_RIVA_POWER_STATE_BMPS_UNKNOWN;
      }
      else if ( hostPowerState == WLANDXE_POWER_STATE_IMPS )
      {
         pDxeCtrlBlk->hostPowerState = WLANDXE_POWER_STATE_IMPS;
      }
      else
      {
         HDXE_ASSERT(0);
      }
   }

   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Exit", __func__);

   return status;
}

/*==========================================================================
  @  Function Name 
      WLANDXE_GetFreeTxDataResNumber

  @  Description 
      Returns free descriptor numbers for TX data channel (TX high priority)

  @  Parameters
      pVoid            pDXEContext : DXE Control Block

  @  Return
      wpt_uint32      Free descriptor number of TX high pri ch
===========================================================================*/
wpt_uint32 WLANDXE_GetFreeTxDataResNumber
(
   void *pDXEContext
)
{
   HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
            "%s Enter", __func__);

   if(NULL == pDXEContext)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "NULL parameter passed by caller");
      return (0);
   }

   return 
      ((WLANDXE_CtrlBlkType *)pDXEContext)->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI].numFreeDesc;
}

/*==========================================================================
  @  Function Name
    WLANDXE_ChannelDebug

  @  Description
    Display DXE Channel debugging information
    User may request to display DXE channel snapshot
    Or if host driver detects any abnormal stcuk may display

  @  Parameters
    displaySnapshot : Display DXE snapshot option
    debugFlags      : Enable stall detect features
                      defined by WPAL_DeviceDebugFlags
                      These features may effect
                      data performance.

  @  Return
    NONE

===========================================================================*/
void WLANDXE_ChannelDebug
(
   wpt_boolean displaySnapshot,
   wpt_uint8   debugFlags
)
{
   wpt_msg                  *channelDebugMsg;
   wpt_msg                  *txDescReSyncMsg ;
   wpt_uint32                regValue, regValueLocal = 0;
   wpt_status                status = eWLAN_PAL_STATUS_SUCCESS;

   /* Debug Type 1, Display current snapshot */
   if(displaySnapshot)
   {
      /* Whatever RIVA power condition try to wakeup RIVA through SMSM
       * This will not simply wakeup RIVA
       * Just incase TX not wanted stuck, Trigger TX again */
      dxeNotifySmsm(eWLAN_PAL_FALSE, eWLAN_PAL_TRUE);
      dxeNotifySmsm(eWLAN_PAL_TRUE, eWLAN_PAL_FALSE);
      /* Get free BD count */
      wpalSleep(10);
      wpalReadRegister(WLANDXE_BMU_AVAILABLE_BD_PDU, &regValue);
#ifdef WCN_PRONTO
      wpalReadRegister(WLANDXE_BMU_AVAILABLE_BD_PDU_LOCAL, &regValueLocal);
#endif
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "===== DXE Dump Start HPS %d, FWS %d, TX PFC %d, ABD %d, ABD LOCAL %d =====",
               tempDxeCtrlBlk->hostPowerState, tempDxeCtrlBlk->rivaPowerState,
               tempDxeCtrlBlk->txCompletedFrames, regValue, regValueLocal);

      wpalPacketStallUpdateInfo((wpt_uint32 *)&tempDxeCtrlBlk->rivaPowerState,
                                &regValue,
                                NULL,
                                0);

      channelDebugMsg = (wpt_msg *)wpalMemoryAllocate(sizeof(wpt_msg));
      if(NULL == channelDebugMsg)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "WLANDXE_ChannelDebug, MSG MEM alloc Fail");
         return ;
      }

      channelDebugMsg->callback = dxeRxThreadChannelDebugHandler;
      status = wpalPostRxMsg(WDI_GET_PAL_CTX(), channelDebugMsg);
      if ( eWLAN_PAL_STATUS_SUCCESS != status )
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "Tx thread Set power state req serialize fail status=%d",
                  status);
      }
   }

   if(debugFlags & WPAL_DEBUG_TX_DESC_RESYNC)
   {
      txDescReSyncMsg = (wpt_msg *)wpalMemoryAllocate(sizeof(wpt_msg));
      if(NULL == txDescReSyncMsg)
      {
         HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "%s: Resync MSG MEM alloc Fail",__func__);
      }
      else
      {
         txDescReSyncMsg->callback = dxeDebugTxDescReSync;
         txDescReSyncMsg->pContext = tempDxeCtrlBlk;
         status = wpalPostTxMsg(WDI_GET_PAL_CTX(),
                                txDescReSyncMsg);
         if(eWLAN_PAL_STATUS_SUCCESS != status)
         {
            HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
                     "%s: Post TX re-sync MSG fail",__func__);
         }
      }
   }

   if(debugFlags & WPAL_DEBUG_START_HEALTH_TIMER)
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "DXE TX Stall detect");
      /* Start Stall detect timer and detect stall */
      wpalTimerStart(&tempDxeCtrlBlk->dxeChannel[WDTS_CHANNEL_TX_LOW_PRI].healthMonitorTimer,
                     T_WLANDXE_PERIODIC_HEALTH_M_TIME);
   }
   return;
}
