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

/**=========================================================================
  
  @file  wlan_qct_dxe_cfg_i.c
  
  @brief 
               
   This file contains the external API exposed by the wlan data transfer abstraction layer module.
   Copyright (c) 2011 QUALCOMM Incorporated.
   All Rights Reserved.
   Qualcomm Confidential and Proprietary
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
#include "wlan_qct_dxe_i.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
typedef struct
{
   WDTS_ChannelType           wlanChannel;
   WLANDXE_DMAChannelType     DMAChannel;
   WLANDXE_ChannelConfigType *channelConfig;
} WLANDXE_ChannelMappingType;

wpt_uint32 channelBaseAddressList[WLANDXE_DMA_CHANNEL_MAX] =
{
   WLANDXE_DMA_CHAN0_BASE_ADDRESS,
   WLANDXE_DMA_CHAN1_BASE_ADDRESS,
   WLANDXE_DMA_CHAN2_BASE_ADDRESS,
   WLANDXE_DMA_CHAN3_BASE_ADDRESS,
   WLANDXE_DMA_CHAN4_BASE_ADDRESS,
   WLANDXE_DMA_CHAN5_BASE_ADDRESS,
   WLANDXE_DMA_CHAN6_BASE_ADDRESS
};

wpt_uint32 channelInterruptMask[WLANDXE_DMA_CHANNEL_MAX] =
{
   WLANDXE_INT_MASK_CHAN_0,
   WLANDXE_INT_MASK_CHAN_1,
   WLANDXE_INT_MASK_CHAN_2,
   WLANDXE_INT_MASK_CHAN_3,
   WLANDXE_INT_MASK_CHAN_4,
   WLANDXE_INT_MASK_CHAN_5,
   WLANDXE_INT_MASK_CHAN_6
};

WLANDXE_ChannelConfigType chanTXLowPriConfig =
{
   /* Q handle type, Circular */
   WLANDXE_CHANNEL_HANDLE_CIRCULA,

   /* Number of Descriptor, NOT CLEAR YET !!! */
   WLANDXE_LO_PRI_RES_NUM ,

   /* MAX num RX Buffer */
   0,

   /* Reference WQ, TX23 */
   23,

   /* USB Only, End point info */
   0,

   /* Transfer Type */
   WLANDXE_DESC_CTRL_XTYPE_H2B,

   /* Channel Priority 7(Highest) - 0(Lowest) NOT CLEAR YET !!! */
   4,

   /* BD attached to frames for this pipe */
   eWLAN_PAL_TRUE,

   /* chk_size, NOT CLEAR YET !!!*/
   0,

   /* bmuThdSel, NOT CLEAR YET !!! */
   5,

   /* Added in Gen5 for Prefetch, NOT CLEAR YET !!! */
   eWLAN_PAL_TRUE,

   /* Use short Descriptor */
   eWLAN_PAL_TRUE
};

WLANDXE_ChannelConfigType chanTXHighPriConfig =
{
   /* Q handle type, Circular */
   WLANDXE_CHANNEL_HANDLE_CIRCULA,

   /* Number of Descriptor, NOT CLEAR YET !!! */
   WLANDXE_HI_PRI_RES_NUM ,

   /* MAX num RX Buffer */
   0,

   /* Reference WQ, TX23 */
   23,

   /* USB Only, End point info */
   0,

   /* Transfer Type */
   WLANDXE_DESC_CTRL_XTYPE_H2B,

   /* Channel Priority 7(Highest) - 0(Lowest), NOT CLEAR YET !!! */
   6,

   /* BD attached to frames for this pipe */
   eWLAN_PAL_TRUE,

   /* chk_size, NOT CLEAR YET !!!*/
   0,

   /* bmuThdSel, NOT CLEAR YET !!! */
   7,

   /* Added in Gen5 for Prefetch, NOT CLEAR YET !!!*/
   eWLAN_PAL_TRUE,

   /* Use short Descriptor */
   eWLAN_PAL_TRUE
};

WLANDXE_ChannelConfigType chanRXLowPriConfig =
{
   /* Q handle type, Circular */
   WLANDXE_CHANNEL_HANDLE_CIRCULA,

   /* Number of Descriptor, NOT CLEAR YET !!! */
   256,

   /* MAX num RX Buffer, NOT CLEAR YET !!! */
   1,

   /* Reference WQ, NOT CLEAR YET !!! */
   /* Temporary BMU Work Q 4 */
   11,

   /* USB Only, End point info */
   0,

   /* Transfer Type */
   WLANDXE_DESC_CTRL_XTYPE_B2H,

   /* Channel Priority 7(Highest) - 0(Lowest), NOT CLEAR YET !!! */
   5,

   /* BD attached to frames for this pipe */
   eWLAN_PAL_TRUE,

   /* chk_size, NOT CLEAR YET !!!*/
   0,

   /* bmuThdSel, NOT CLEAR YET !!! */
   6,

   /* Added in Gen5 for Prefetch, NOT CLEAR YET !!!*/
   eWLAN_PAL_TRUE,

   /* Use short Descriptor */
   eWLAN_PAL_TRUE
};

WLANDXE_ChannelConfigType chanRXHighPriConfig =
{
   /* Q handle type, Circular */
   WLANDXE_CHANNEL_HANDLE_CIRCULA,

   /* Number of Descriptor, NOT CLEAR YET !!! */
   256,

   /* MAX num RX Buffer, NOT CLEAR YET !!! */
   1,

   /* Reference WQ, RX11 */
   4,

   /* USB Only, End point info */
   0,

   /* Transfer Type */
   WLANDXE_DESC_CTRL_XTYPE_B2H,

   /* Channel Priority 7(Highest) - 0(Lowest), NOT CLEAR YET !!! */
   6,

   /* BD attached to frames for this pipe */
   eWLAN_PAL_TRUE,

   /* chk_size, NOT CLEAR YET !!!*/
   0,

   /* bmuThdSel, NOT CLEAR YET !!! */
   8,

   /* Added in Gen5 for Prefetch, NOT CLEAR YET !!!*/
   eWLAN_PAL_TRUE,

   /* Use short Descriptor */
   eWLAN_PAL_TRUE
};

#ifdef WLANDXE_TEST_CHANNEL_ENABLE
WLANDXE_ChannelConfigType chanH2HTestConfig =
{
   /* Q handle type, Circular */
   WLANDXE_CHANNEL_HANDLE_CIRCULA,

   /* Number of Descriptor, NOT CLEAR YET !!! */
   5,

   /* MAX num RX Buffer, NOT CLEAR YET !!! */
   0,

   /* Reference WQ, NOT CLEAR YET !!! */
   /* Temporary BMU Work Q 5 */
   5,

   /* USB Only, End point info */
   0,

   /* Transfer Type */
   WLANDXE_DESC_CTRL_XTYPE_H2H,

   /* Channel Priority 7(Highest) - 0(Lowest), NOT CLEAR YET !!! */
   5,

   /* BD attached to frames for this pipe */
   eWLAN_PAL_FALSE,

   /* chk_size, NOT CLEAR YET !!!*/
   0,

   /* bmuThdSel, NOT CLEAR YET !!! */
   0,

   /* Added in Gen5 for Prefetch, NOT CLEAR YET !!!*/
   eWLAN_PAL_TRUE,

   /* Use short Descriptor */
   eWLAN_PAL_TRUE
};
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */

WLANDXE_ChannelMappingType channelList[WDTS_CHANNEL_MAX] =
{
   {WDTS_CHANNEL_TX_LOW_PRI,  WLANDXE_DMA_CHANNEL_0, &chanTXLowPriConfig},
   {WDTS_CHANNEL_TX_HIGH_PRI, WLANDXE_DMA_CHANNEL_4, &chanTXHighPriConfig},
   {WDTS_CHANNEL_RX_LOW_PRI,  WLANDXE_DMA_CHANNEL_1, &chanRXLowPriConfig},
#ifndef WLANDXE_TEST_CHANNEL_ENABLE
   {WDTS_CHANNEL_RX_HIGH_PRI, WLANDXE_DMA_CHANNEL_3, &chanRXHighPriConfig},
#else
   {WDTS_CHANNEL_H2H_TEST_TX,    WLANDXE_DMA_CHANNEL_2, &chanH2HTestConfig},
   {WDTS_CHANNEL_H2H_TEST_RX,    WLANDXE_DMA_CHANNEL_2, &chanH2HTestConfig}
#endif /* WLANDXE_TEST_CHANNEL_ENABLE */
};

WLANDXE_TxCompIntConfigType txCompInt = 
{
   /* TX Complete Interrupt enable method */
   WLANDXE_TX_COMP_INT_PER_K_FRAMES,

   /* TX Low Resource remaining resource threshold for Low Pri Ch */
   WLANDXE_TX_LOW_RES_THRESHOLD,

   /* TX Low Resource remaining resource threshold for High Pri Ch */
   WLANDXE_TX_LOW_RES_THRESHOLD,

   /* RX Low Resource remaining resource threshold */
   5,

   /* Per K frame enable Interrupt */
   /*WLANDXE_HI_PRI_RES_NUM*/ 5,

   /* Periodic timer msec */
   10
};

/*==========================================================================
  @  Function Name 
      dxeCommonDefaultConfig

  @  Description 

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block

  @  Return
      wpt_status

===========================================================================*/
wpt_status dxeCommonDefaultConfig
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk
)
{
   wpt_status                  status = eWLAN_PAL_STATUS_SUCCESS;

   dxeCtrlBlk->rxReadyCB     = NULL;
   dxeCtrlBlk->txCompCB      = NULL;
   dxeCtrlBlk->lowResourceCB = NULL;

   wpalMemoryCopy(&dxeCtrlBlk->txCompInt,
                  &txCompInt,
                  sizeof(WLANDXE_TxCompIntConfigType));

   return status;
}

/*==========================================================================
  @  Function Name 
      dxeChannelDefaultConfig

  @  Description 
      Get defualt configuration values from pre defined structure
      All the channels must have it's own configurations

  @  Parameters
      WLANDXE_CtrlBlkType:    *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
wpt_status dxeChannelDefaultConfig
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk,
   WLANDXE_ChannelCBType   *channelEntry
)
{
   wpt_status                  status = eWLAN_PAL_STATUS_SUCCESS;
   wpt_uint32                  baseAddress;
   wpt_uint32                  dxeControlRead  = 0;
   wpt_uint32                  dxeControlWrite = 0;
   wpt_uint32                  dxeControlWriteValid = 0;
   wpt_uint32                  dxeControlWriteEop = 0;
   wpt_uint32                  dxeControlWriteEopInt = 0;
   wpt_uint32                  idx;
   wpt_uint32                  rxResourceCount = 0;
   WLANDXE_ChannelMappingType *mappedChannel = NULL;

   /* Sanity Check */
   if((NULL == dxeCtrlBlk) || (NULL == channelEntry))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "dxeLinkDescAndCtrlBlk Channel Entry is not valid");
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   for(idx = 0; idx < WDTS_CHANNEL_MAX; idx++)
   {
      if(channelEntry->channelType == channelList[idx].wlanChannel)
      {
         mappedChannel = &channelList[idx];
         break;
      }
   }

   if((NULL == mappedChannel) || (WDTS_CHANNEL_MAX == idx))
   {
      HDXE_MSG(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "%s Failed to map channel", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   wpalMemoryCopy(&channelEntry->channelConfig,
                  mappedChannel->channelConfig,
                  sizeof(WLANDXE_ChannelConfigType));

   baseAddress = channelBaseAddressList[mappedChannel->DMAChannel];
   channelEntry->channelRegister.chDXEBaseAddr        = baseAddress;
   channelEntry->channelRegister.chDXEStatusRegAddr   = baseAddress + WLANDXE_DMA_CH_STATUS_REG;
   channelEntry->channelRegister.chDXEDesclRegAddr    = baseAddress + WLANDXE_DMA_CH_DESCL_REG;
   channelEntry->channelRegister.chDXEDeschRegAddr    = baseAddress + WLANDXE_DMA_CH_DESCH_REG;
   channelEntry->channelRegister.chDXELstDesclRegAddr = baseAddress + WLANDXE_DMA_CH_LST_DESCL_REG;
   channelEntry->channelRegister.chDXECtrlRegAddr     = baseAddress + WLANDXE_DMA_CH_CTRL_REG;
   channelEntry->channelRegister.chDXESzRegAddr       = baseAddress + WLANDXE_DMA_CH_SZ_REG;
   channelEntry->channelRegister.chDXEDadrlRegAddr    = baseAddress + WLANDXE_DMA_CH_DADRL_REG;
   channelEntry->channelRegister.chDXEDadrhRegAddr    = baseAddress + WLANDXE_DMA_CH_DADRH_REG;
   channelEntry->channelRegister.chDXESadrlRegAddr    = baseAddress + WLANDXE_DMA_CH_SADRL_REG;
   channelEntry->channelRegister.chDXESadrhRegAddr    = baseAddress + WLANDXE_DMA_CH_SADRH_REG;

   /* Channel Mask?
    * This value will control channel control register.
    * This register will be set to trigger actual DMA transfer activate
    * CH_N_CTRL */
   channelEntry->extraConfig.chan_mask = 0;
   /* Check VAL bit before processing descriptor */
   channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_EDVEN_MASK;
   /* Use External Descriptor Linked List */
   channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_EDEN_MASK;
   /* Enable Channel Interrupt on error */
   channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_INE_ERR_MASK;
   /* Enable INT after XFER done */
   channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_INE_DONE_MASK;
   /* Enable INT External Descriptor */
   channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_INE_ED_MASK;
   /* Set Channel This is not channel, event counter, somthing wrong */
   channelEntry->extraConfig.chan_mask |= 
                mappedChannel->DMAChannel << WLANDXE_CH_CTRL_CTR_SEL_OFFSET;
   /* Transfer Type */
   channelEntry->extraConfig.chan_mask |= mappedChannel->channelConfig->xfrType;
   /* Use Short Descriptor, THIS LOOKS SOME WIERD, REVISIT */
   if(!channelEntry->channelConfig.useShortDescFmt)
   {
      channelEntry->extraConfig.chan_mask |= WLANDXE_DESC_CTRL_DFMT;
   }
   /* TX Channel, Set DIQ bit, Clear SIQ bit since source is not WQ */
   if((WDTS_CHANNEL_TX_LOW_PRI  == channelEntry->channelType) ||
      (WDTS_CHANNEL_TX_HIGH_PRI == channelEntry->channelType))
   {
      channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_DIQ_MASK;
   }
   /* RX Channel, Set SIQ bit, Clear DIQ bit since source is not WQ */
   else if((WDTS_CHANNEL_RX_LOW_PRI  == channelEntry->channelType) ||
           (WDTS_CHANNEL_RX_HIGH_PRI == channelEntry->channelType))
   {
      channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_SIQ_MASK;
   }
   else
   {
      /* This is test H2H channel, TX, RX not use work Q
       * Do Nothing */
   }
   /* Frame Contents Swap */
   channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_SWAP_MASK;
   /* Host System Using Little Endian */
   channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_ENDIAN_MASK;
   /* BMU Threshold select */
   channelEntry->extraConfig.chan_mask |= 
                 channelEntry->channelConfig.bmuThdSel << WLANDXE_CH_CTRL_BTHLD_SEL_OFFSET;
   /* EOP for control register ??? */
   channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_EOP_MASK;
   /* Channel Priority */
   channelEntry->extraConfig.chan_mask |= channelEntry->channelConfig.chPriority << WLANDXE_CH_CTRL_PRIO_OFFSET;
   /* PDU REL */
   channelEntry->extraConfig.chan_mask |= WLANDXE_DESC_CTRL_PDU_REL;
   /* Disable DMA transfer on this channel */
   channelEntry->extraConfig.chan_mask_read_disable = channelEntry->extraConfig.chan_mask;
   /* Enable DMA transfer on this channel */
   channelEntry->extraConfig.chan_mask |= WLANDXE_CH_CTRL_EN_MASK;
   /* Channel Mask done */

   /* Control Read
    * Default Descriptor control Word value for RX ready DXE descriptor
    * DXE engine will reference this value before DMA transfer */
   dxeControlRead = 0;
   /* Source is a Queue ID, not flat memory address */
   dxeControlRead |= WLANDXE_DESC_CTRL_SIQ;
   /* Transfer direction is BMU 2 Host */
   dxeControlRead |= WLANDXE_DESC_CTRL_XTYPE_B2H;
   /* End of Packet, RX is single fragment */
   dxeControlRead |= WLANDXE_DESC_CTRL_EOP;
   /* BD Present, default YES, B2H case it must be 0 to insert BD */
   if(!channelEntry->channelConfig.bdPresent)
   {
      dxeControlRead |= WLANDXE_DESC_CTRL_BDH;
   }
   /* Channel Priority */
   dxeControlRead |= channelEntry->channelConfig.chPriority << WLANDXE_CH_CTRL_PRIO_OFFSET;
   /* BMU Threshold select, only used H2B, not this case??? */
   dxeControlRead |= channelEntry->channelConfig.bmuThdSel << WLANDXE_CH_CTRL_BTHLD_SEL_OFFSET;
   /* PDU Release, Release BD/PDU when DMA done */
   dxeControlRead |= WLANDXE_DESC_CTRL_PDU_REL;
   /* Use Short Descriptor, THIS LOOKS SOME WIERD, REVISIT */
   if(!channelEntry->channelConfig.useShortDescFmt)
   {
      dxeControlRead |= WLANDXE_DESC_CTRL_DFMT;
   }
   /* Interrupt on Descriptor done */
   dxeControlRead |= WLANDXE_DESC_CTRL_INT;
   /* For ready status, this Control WORD must be VALID */
   dxeControlRead |= WLANDXE_DESC_CTRL_VALID;
   /* Frame Contents Swap */
   dxeControlRead |= WLANDXE_DESC_CTRL_BDT_SWAP;
   /* Host Little Endian */
   if((WDTS_CHANNEL_TX_LOW_PRI  == channelEntry->channelType) ||
      (WDTS_CHANNEL_TX_HIGH_PRI == channelEntry->channelType))
   {
      dxeControlRead |= WLANDXE_DESC_CTRL_ENDIANNESS;
   }

   /* SWAP if needed */
   channelEntry->extraConfig.cw_ctrl_read = WLANDXE_U32_SWAP_ENDIAN(dxeControlRead);
   /* Control Read Done */

   /* Control Write
    * Write into DXE descriptor control word to TX frame
    * DXE engine will reference this word to contorl TX DMA channel */
   channelEntry->extraConfig.cw_ctrl_write = 0;
   /* Transfer type, from Host 2 BMU */
   dxeControlWrite |= mappedChannel->channelConfig->xfrType;
   /* BD Present, this looks some weird ??? */
   if(!channelEntry->channelConfig.bdPresent)
   {
      dxeControlWrite |= WLANDXE_DESC_CTRL_BDH;
   }
   /* Channel Priority */
   dxeControlWrite |= channelEntry->channelConfig.chPriority << WLANDXE_CH_CTRL_PRIO_OFFSET;
   /* Use Short Descriptor, THIS LOOKS SOME WIERD, REVISIT */
   if(!channelEntry->channelConfig.useShortDescFmt)
   {
      dxeControlWrite |= WLANDXE_DESC_CTRL_DFMT;
   }
   /* BMU Threshold select, only used H2B, not this case??? */
   dxeControlWrite |= channelEntry->channelConfig.bmuThdSel << WLANDXE_CH_CTRL_BTHLD_SEL_OFFSET;
   /* Destination is WQ */
   dxeControlWrite |= WLANDXE_DESC_CTRL_DIQ;
   /* Frame Contents Swap */
   dxeControlWrite |= WLANDXE_DESC_CTRL_BDT_SWAP;
   /* Host Little Endian */
   dxeControlWrite |= WLANDXE_DESC_CTRL_ENDIANNESS;
   /* Interrupt Enable */
   dxeControlWrite |= WLANDXE_DESC_CTRL_INT;

   dxeControlWriteValid  = dxeControlWrite | WLANDXE_DESC_CTRL_VALID;
   dxeControlWriteEop    = dxeControlWriteValid | WLANDXE_DESC_CTRL_EOP;
   dxeControlWriteEopInt = dxeControlWriteEop | WLANDXE_DESC_CTRL_INT;

   /* DXE Descriptor must has Endian swapped value */
   channelEntry->extraConfig.cw_ctrl_write = WLANDXE_U32_SWAP_ENDIAN(dxeControlWrite);
   /* Control Write DONE */

   /* Control Write include VAL bit
    * This Control word used to set valid bit and
    * trigger DMA transfer for specific descriptor */
   channelEntry->extraConfig.cw_ctrl_write_valid =
                  WLANDXE_U32_SWAP_ENDIAN(dxeControlWriteValid);

   /* Control Write include EOP
    * End of Packet */
   channelEntry->extraConfig.cw_ctrl_write_eop =
                  WLANDXE_U32_SWAP_ENDIAN(dxeControlWriteEop);

   /* Control Write include EOP and INT
    * indicate End Of Packet and generate interrupt on descriptor Done */
   channelEntry->extraConfig.cw_ctrl_write_eop_int =
                  WLANDXE_U32_SWAP_ENDIAN(dxeControlWriteEopInt);


   /* size mask???? */
   channelEntry->extraConfig.chk_size_mask = 
            mappedChannel->channelConfig->chk_size << 10;

   channelEntry->extraConfig.refWQ_swapped = 
                WLANDXE_U32_SWAP_ENDIAN(channelEntry->channelConfig.refWQ);

   /* Set Channel specific Interrupt mask */
   channelEntry->extraConfig.intMask = channelInterruptMask[mappedChannel->DMAChannel];


   wpalGetNumRxRawPacket(&rxResourceCount);
   if((WDTS_CHANNEL_TX_LOW_PRI == channelEntry->channelType) ||
      (0 == rxResourceCount))
   {
      channelEntry->numDesc         = mappedChannel->channelConfig->nDescs;
   }
   else
   {
      channelEntry->numDesc         = rxResourceCount / 4;
   }
   channelEntry->assignedDMAChannel = mappedChannel->DMAChannel;
   channelEntry->numFreeDesc             = 0;
   channelEntry->numRsvdDesc             = 0;
   channelEntry->numFragmentCurrentChain = 0;
   channelEntry->numTotalFrame           = 0;
   channelEntry->hitLowResource          = eWLAN_PAL_FALSE;

   return status;
}
