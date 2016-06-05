/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_QCT_DXE_I_H
#define WLAN_QCT_DXE_I_H

/**=========================================================================
  
  @file  wlan_qct_dxe_i.h
  
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
#include "wlan_qct_pal_trace.h"
#include "wlan_qct_pal_timer.h"
#include "vos_trace.h"
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define WLANDXE_CTXT_COOKIE              0xC00CC111

#define foreach_valid_channel(idx)                \
    for (idx = 0; idx < WDTS_CHANNEL_MAX; idx++)  \
        if (!(dxeGetEnabledChannels() & 1<<idx))  \
            continue;                             \
        else

#define WLANDXE_IS_VALID_CHANNEL(idx) \
    (dxeGetEnabledChannels() & 1<<idx)

/* From here WCNSS DXE register information
 * This is temporary definition location to make compile and unit test
 * If official msmreg.h integrated, this part will be eliminated */
/* Start with base address */

#ifdef WCN_PRONTO
#define WLANDXE_CCU_DXE_INT_SELECT        0x2050dc
#define WLANDXE_CCU_DXE_INT_SELECT_STAT   0x2050e0
#define WLANDXE_CCU_ASIC_INT_ENABLE       0x2050e4
#define WLANDXE_CCU_SOFT_RESET            0x204010
#define WLANDXE_BMU_AVAILABLE_BD_PDU_LOCAL 0x80260
#else
#define WLANDXE_CCU_DXE_INT_SELECT        0x200b10
#define WLANDXE_CCU_DXE_INT_SELECT_STAT   0x200b14
#define WLANDXE_CCU_ASIC_INT_ENABLE       0x200b18
#endif

#define WLANDXE_BMU_AVAILABLE_BD_PDU      0x80084
#define WLANDXE_BMU_ERROR_INTR_STATUS     0x80004

#define WLANDXE_REGISTER_BASE_ADDRESS     0x202000

#define WLAN_PMU_SPARE_OUT_ADDRESS        0x21c088
#define WLAN_PMU_POWER_DOWN_MASK          0x04000000

/* Common over the channels register addresses */
#define WALNDEX_DMA_CSR_ADDRESS          (WLANDXE_REGISTER_BASE_ADDRESS + 0x00)
#define WALNDEX_DMA_ENCH_ADDRESS         (WLANDXE_REGISTER_BASE_ADDRESS + 0x04)
#define WALNDEX_DMA_CH_EN_ADDRESS        (WLANDXE_REGISTER_BASE_ADDRESS + 0x08)
#define WALNDEX_DMA_CH_DONE_ADDRESS      (WLANDXE_REGISTER_BASE_ADDRESS + 0x0C)
#define WALNDEX_DMA_CH_ERR_ADDRESS       (WLANDXE_REGISTER_BASE_ADDRESS + 0x10)
#define WALNDEX_DMA_CH_STOP_ADDRESS      (WLANDXE_REGISTER_BASE_ADDRESS + 0x14)

/* Interrupt Control register address */
#define WLANDXE_INT_MASK_REG_ADDRESS     (WLANDXE_REGISTER_BASE_ADDRESS + 0x18)
#define WLANDXE_INT_SRC_MSKD_ADDRESS     (WLANDXE_REGISTER_BASE_ADDRESS + 0x1C)
#define WLANDXE_INT_SRC_RAW_ADDRESS      (WLANDXE_REGISTER_BASE_ADDRESS + 0x20)
#define WLANDXE_INT_ED_SRC_ADDRESS       (WLANDXE_REGISTER_BASE_ADDRESS + 0x24)
#define WLANDXE_INT_DONE_SRC_ADDRESS     (WLANDXE_REGISTER_BASE_ADDRESS + 0x28)
#define WLANDXE_INT_ERR_SRC_ADDRESS      (WLANDXE_REGISTER_BASE_ADDRESS + 0x2C)
#define WLANDXE_INT_CLR_ADDRESS          (WLANDXE_REGISTER_BASE_ADDRESS + 0x30)
#define WLANDXE_INT_ED_CLR_ADDRESS       (WLANDXE_REGISTER_BASE_ADDRESS + 0x34)
#define WLANDXE_INT_DONE_CLR_ADDRESS     (WLANDXE_REGISTER_BASE_ADDRESS + 0x38)
#define WLANDXE_INT_ERR_CLR_ADDRESS      (WLANDXE_REGISTER_BASE_ADDRESS + 0x3C)

#define WLANDXE_DMA_CH_PRES_ADDRESS      (WLANDXE_REGISTER_BASE_ADDRESS + 0x40)
#define WLANDXE_ARB_CH_MSK_CLR_ADDRRESS  (WLANDXE_REGISTER_BASE_ADDRESS + 0x74)

/* Channel Counter register */
#define WLANDXE_DMA_COUNTER_0            (WLANDXE_REGISTER_BASE_ADDRESS + 0x200)
#define WLANDXE_DMA_COUNTER_1            (WLANDXE_REGISTER_BASE_ADDRESS + 0x204)
#define WLANDXE_DMA_COUNTER_2            (WLANDXE_REGISTER_BASE_ADDRESS + 0x208)
#define WLANDXE_DMA_COUNTER_3            (WLANDXE_REGISTER_BASE_ADDRESS + 0x20C)
#define WLANDXE_DMA_COUNTER_4            (WLANDXE_REGISTER_BASE_ADDRESS + 0x210)
#define WLANDXE_DMA_COUNTER_5            (WLANDXE_REGISTER_BASE_ADDRESS + 0x214)
#define WLANDXE_DMA_COUNTER_6            (WLANDXE_REGISTER_BASE_ADDRESS + 0x218)

#define WLANDXE_ENGINE_STAT_ADDRESS      (WLANDXE_REGISTER_BASE_ADDRESS + 0x64)
#define WLANDXE_BMU_SB_QDAT_AV_ADDRESS   (WLANDXE_REGISTER_BASE_ADDRESS + 0x5c)

/* Channel Base address */
#define WLANDXE_DMA_CHAN0_BASE_ADDRESS   (WLANDXE_REGISTER_BASE_ADDRESS + 0x400)
#define WLANDXE_DMA_CHAN1_BASE_ADDRESS   (WLANDXE_REGISTER_BASE_ADDRESS + 0x440)
#define WLANDXE_DMA_CHAN2_BASE_ADDRESS   (WLANDXE_REGISTER_BASE_ADDRESS + 0x480)
#define WLANDXE_DMA_CHAN3_BASE_ADDRESS   (WLANDXE_REGISTER_BASE_ADDRESS + 0x4C0)
#define WLANDXE_DMA_CHAN4_BASE_ADDRESS   (WLANDXE_REGISTER_BASE_ADDRESS + 0x500)
#define WLANDXE_DMA_CHAN5_BASE_ADDRESS   (WLANDXE_REGISTER_BASE_ADDRESS + 0x540)
#define WLANDXE_DMA_CHAN6_BASE_ADDRESS   (WLANDXE_REGISTER_BASE_ADDRESS + 0x580)
#define WLANDXE_DMA_CHAN7_BASE_ADDRESS   (WLANDXE_REGISTER_BASE_ADDRESS + 0x5c0)

/* Channel specific register offset */
#define WLANDXE_DMA_CH_CTRL_REG          0x0000
#define WLANDXE_DMA_CH_STATUS_REG        0x0004
#define WLANDXE_DMA_CH_SZ_REG            0x0008
#define WLANDXE_DMA_CH_SADRL_REG         0x000C
#define WLANDXE_DMA_CH_SADRH_REG         0x0010
#define WLANDXE_DMA_CH_DADRL_REG         0x0014
#define WLANDXE_DMA_CH_DADRH_REG         0x0018
#define WLANDXE_DMA_CH_DESCL_REG         0x001C
#define WLANDXE_DMA_CH_DESCH_REG         0x0020
#define WLANDXE_DMA_CH_LST_DESCL_REG     0x0024
#define WLANDXE_DMA_CH_LST_DESCH_REG     0x0028
#define WLANDXE_DMA_CH_BD_REG            0x002C
#define WLANDXE_DMA_CH_HEAD_REG          0x0030
#define WLANDXE_DMA_CH_TAIL_REG          0x0034
#define WLANDXE_DMA_CH_PDU_REG           0x0038
#define WLANDXE_DMA_CH_TSTMP_REG         0x003C

/* Common CSR Register Contorol mask and offset */
#ifdef WCN_PRONTO
#define WLANDXE_DMA_CSR_RESERVED_MASK         0xFFFF0000
#define WLANDXE_DMA_CSR_RESERVED_OFFSET       0x10
#define WLANDXE_DMA_CSR_RESERVED_DEFAULT      0x0

#define WLANDXE_DMA_CSR_FW_BMU_RECOVERY       0x400000
#define WLANDXE_DMA_CSR_RECOVERY_DONE         0x200000
#define WLANDXE_DMA_CSR_HOST_RECOVERY_DONE    0x800000

#define WLANDXE_DMA_CSR_H2H_SYNC_EN_MASK      0x8000
#define WLANDXE_DMA_CSR_H2H_SYNC_EN_OFFSET    0x0F
#define WLANDXE_DMA_CSR_H2H_SYNC_EN_DEFAULT   0x0

#define WLANDXE_DMA_CSR_PAUSED_MASK           0x4000
#define WLANDXE_DMA_CSR_PAUSED_OFFSET         0x0E
#define WLANDXE_DMA_CSR_PAUSED_DEFAULT        0x0

#define WLANDXE_DMA_CSR_ECTR_EN_MASK          0x2000
#define WLANDXE_DMA_CSR_ECTR_EN_OFFSET        0x0D
#define WLANDXE_DMA_CSR_ECTR_EN_DEFAULT       0x2000

#define WLANDXE_DMA_CSR_B2H_TSTMP_OFF_MASK    0x1F00
#define WLANDXE_DMA_CSR_B2H_TSTMP_OFF_OFFSET  0x08
#define WLANDXE_DMA_CSR_B2H_TSTMP_OFF_DEFAULT 0x0F00

#define WLANDXE_DMA_CSR_H2B_TSTMP_OFF_MASK    0xF8
#define WLANDXE_DMA_CSR_H2B_TSTMP_OFF_OFFSET  0x03
#define WLANDXE_DMA_CSR_H2B_TSTMP_OFF_DEFAULT 0x28

#define WLANDXE_DMA_CSR_TSTMP_EN_MASK         0x04
#define WLANDXE_DMA_CSR_TSTMP_EN_OFFSET       0x02
#define WLANDXE_DMA_CSR_TSTMP_EN_DEFAULT      0x0

#define WLANDXE_DMA_CCU_DXE_RESET_MASK        0x4
#else
#define WLANDXE_DMA_CSR_RESERVED_MASK         0xFFFE0000
#define WLANDXE_DMA_CSR_RESERVED_OFFSET       0x11
#define WLANDXE_DMA_CSR_RESERVED_DEFAULT      0x0

#define WLANDXE_DMA_CSR_H2H_SYNC_EN_MASK      0x10000
#define WLANDXE_DMA_CSR_H2H_SYNC_EN_OFFSET    0x10
#define WLANDXE_DMA_CSR_H2H_SYNC_EN_DEFAULT   0x0

#define WLANDXE_DMA_CSR_PAUSED_MASK           0x8000
#define WLANDXE_DMA_CSR_PAUSED_OFFSET         0xF
#define WLANDXE_DMA_CSR_PAUSED_DEFAULT        0x0

#define WLANDXE_DMA_CSR_ECTR_EN_MASK          0x4000
#define WLANDXE_DMA_CSR_ECTR_EN_OFFSET        0xE
#define WLANDXE_DMA_CSR_ECTR_EN_DEFAULT       0x4000

#define WLANDXE_DMA_CSR_B2H_TSTMP_OFF_MASK    0x3E00
#define WLANDXE_DMA_CSR_B2H_TSTMP_OFF_OFFSET  0x9
#define WLANDXE_DMA_CSR_B2H_TSTMP_OFF_DEFAULT 0xE00

#define WLANDXE_DMA_CSR_H2B_TSTMP_OFF_MASK    0x1F0
#define WLANDXE_DMA_CSR_H2B_TSTMP_OFF_OFFSET  0x4
#define WLANDXE_DMA_CSR_H2B_TSTMP_OFF_DEFAULT 0x50

#define WLANDXE_DMA_CSR_TSTMP_EN_MASK         0x8
#define WLANDXE_DMA_CSR_TSTMP_EN_OFFSET       0x3
#define WLANDXE_DMA_CSR_TSTMP_EN_DEFAULT      0x0

#define WLANDXE_DMA_CSR_RESET_MASK            0x4
#define WLANDXE_DMA_CSR_RESET_OFFSET          0x2
#define WLANDXE_DMA_CSR_RESET_DEFAULT         0x0
#endif /* WCN_PRONTO */

#define WLANDXE_DMA_CSR_PAUSE_MASK            0x2
#define WLANDXE_DMA_CSR_PAUSE_OFFSET          0x1
#define WLANDXE_DMA_CSR_PAUSE_DEFAULT         0x0

#define WLANDXE_DMA_CSR_EN_MASK               0x1
#define WLANDXE_DMA_CSR_EN_OFFSET             0x0
#define WLANDXE_DMA_CSR_EN_DEFAULT            0x0

/* DXE CSR Master enable register value */
#define WLANDXE_CSR_DEFAULT_ENABLE            (WLANDXE_DMA_CSR_H2H_SYNC_EN_MASK | \
                                               WLANDXE_DMA_CSR_ECTR_EN_MASK | \
                                               WLANDXE_DMA_CSR_EN_MASK)

/* Channel CTRL Register Control mask and offset */
#define WLANDXE_CH_CTRL_RSVD_MASK             0x80000000
#define WLANDXE_CH_CTRL_RSVD_OFFSET           0x1F
#define WLANDXE_CH_CTRL_RSVD_DEFAULT          0x0

#define WLANDXE_CH_CTRL_SWAP_MASK             0x80000000

#define WLANDXE_CH_CTRL_BDT_IDX_MASK          0x60000000
#define WLANDXE_CH_CTRL_BDT_IDX_OFFSET        0x1D
#define WLANDXE_CH_CTRL_BDT_IDX_DEFAULT       0x0

#define WLANDXE_CH_CTRL_DFMT_MASK             0x10000000
#define WLANDXE_CH_CTRL_DFMT_OFFSET           0x1C
#define WLANDXE_CH_CTRL_DFMT_DEFAULT          0x10000000
#define WLANDXE_CH_CTRL_DFMT_ESHORT           0x0
#define WLANDXE_CH_CTRL_DFMT_ELONG            0x1

#define WLANDXE_CH_CTRL_ABORT_MASK            0x8000000
#define WLANDXE_CH_CTRL_ABORT_OFFSET          0x1B
#define WLANDXE_CH_CTRL_ABORT_DEFAULT         0x0

#define WLANDXE_CH_CTRL_ENDIAN_MASK           0x4000000

#define WLANDXE_CH_CTRL_CTR_SEL_MASK          0x3C00000
#define WLANDXE_CH_CTRL_CTR_SEL_OFFSET        0x16
#define WLANDXE_CH_CTRL_CTR_SEL_DEFAULT       0x0

#define WLANDXE_CH_CTRL_EDVEN_MASK            0x200000
#define WLANDXE_CH_CTRL_EDVEN_OFFSET          0x15
#define WLANDXE_CH_CTRL_EDVEN_DEFAULT         0x0

#define WLANDXE_CH_CTRL_EDEN_MASK             0x100000
#define WLANDXE_CH_CTRL_EDEN_OFFSET           0x14
#define WLANDXE_CH_CTRL_EDEN_DEFAULT          0x0

#define WLANDXE_CH_CTRL_INE_DONE_MASK         0x80000
#define WLANDXE_CH_CTRL_INE_DONE_OFFSET       0x13
#define WLANDXE_CH_CTRL_INE_DONE_DEFAULT      0x0

#define WLANDXE_CH_CTRL_INE_ERR_MASK          0x40000
#define WLANDXE_CH_CTRL_INE_ERR_OFFSET        0x12
#define WLANDXE_CH_CTRL_INE_ERR_DEFAULT       0x0

#define WLANDXE_CH_CTRL_INE_ED_MASK           0x20000
#define WLANDXE_CH_CTRL_INE_ED_OFFSET         0x11
#define WLANDXE_CH_CTRL_INE_ED_DEFAULT        0x0

#define WLANDXE_CH_CTRL_STOP_MASK             0x10000
#define WLANDXE_CH_CTRL_STOP_OFFSET           0x10
#define WLANDXE_CH_CTRL_STOP_DEFAULT          0x0

#define WLANDXE_CH_CTRL_PRIO_MASK             0xE000
#define WLANDXE_CH_CTRL_PRIO_OFFSET           0xD
#define WLANDXE_CH_CTRL_PRIO_DEFAULT          0x0

#define WLANDXE_CH_CTRL_BTHLD_SEL_MASK        0x1E00
#define WLANDXE_CH_CTRL_BTHLD_SEL_OFFSET      0x9
#define WLANDXE_CH_CTRL_BTHLD_SEL_DEFAULT     0x600
#define WLANDXE_CH_CTRL_BTHLD_SEL_ERSVD0      0x0
#define WLANDXE_CH_CTRL_BTHLD_SEL_ERSVD1      0x1
#define WLANDXE_CH_CTRL_BTHLD_SEL_ETHLD2      0x2
#define WLANDXE_CH_CTRL_BTHLD_SEL_ETHLD3      0x3
#define WLANDXE_CH_CTRL_BTHLD_SEL_ETHLD4      0x4
#define WLANDXE_CH_CTRL_BTHLD_SEL_ETHLD5      0x5
#define WLANDXE_CH_CTRL_BTHLD_SEL_ETHLD6      0x6
#define WLANDXE_CH_CTRL_BTHLD_SEL_ETHLD7      0x7
#define WLANDXE_CH_CTRL_BTHLD_SEL_ETHLD8      0x8
#define WLANDXE_CH_CTRL_BTHLD_SEL_ETHLD9      0x9
#define WLANDXE_CH_CTRL_BTHLD_SEL_ETHLD10     0xA
#define WLANDXE_CH_CTRL_BTHLD_SEL_ERSVD11     0xB
#define WLANDXE_CH_CTRL_BTHLD_SEL_ERSVD12     0xC
#define WLANDXE_CH_CTRL_BTHLD_SEL_ERSVD13     0xD
#define WLANDXE_CH_CTRL_BTHLD_SEL_ERSVD14     0xE
#define WLANDXE_CH_CTRL_BTHLD_SEL_ERSVD15     0xF

#define WLANDXE_CH_CTRL_PDU_REL_MASK          0x100
#define WLANDXE_CH_CTRL_PDU_REL_OFFSET        0x8
#define WLANDXE_CH_CTRL_PDU_REL_DEFAULT       0x100
#define WLANDXE_CH_CTRL_PDU_REL_EKEEP         0x0
#define WLANDXE_CH_CTRL_PDU_REL_ERELEASE      0x1

#define WLANDXE_CH_CTRL_PIQ_MASK              0x80
#define WLANDXE_CH_CTRL_PIQ_OFFSET            0x7
#define WLANDXE_CH_CTRL_PIQ_DEFAULT           0x0
#define WLANDXE_CH_CTRL_PIQ_EFLAT             0x0
#define WLANDXE_CH_CTRL_PIQ_EQUEUE            0x1

#define WLANDXE_CH_CTRL_DIQ_MASK              0x40
#define WLANDXE_CH_CTRL_DIQ_OFFSET            0x6
#define WLANDXE_CH_CTRL_DIQ_DEFAULT           0x0
#define WLANDXE_CH_CTRL_DIQ_EFLAT             0x0
#define WLANDXE_CH_CTRL_DIQ_EQUEUE            0x1

#define WLANDXE_CH_CTRL_SIQ_MASK              0x20
#define WLANDXE_CH_CTRL_SIQ_OFFSET            0x5
#define WLANDXE_CH_CTRL_SIQ_DEFAULT           0x0
#define WLANDXE_CH_CTRL_SIQ_EFLAT             0x0
#define WLANDXE_CH_CTRL_SIQ_EQUEUE            0x1

#define WLANDXE_CH_CTRL_BDH_MASK              0x10
#define WLANDXE_CH_CTRL_BDH_OFFSET            0x4
#define WLANDXE_CH_CTRL_BDH_DEFAULT           0x0

#define WLANDXE_CH_CTRL_EOP_MASK              0x8
#define WLANDXE_CH_CTRL_EOP_OFFSET            0x3
#define WLANDXE_CH_CTRL_EOP_DEFAULT           0x8

#define WLANDXE_CH_CTRL_XTYPE_MASK            0x6
#define WLANDXE_CH_CTRL_XTYPE_OFFSET          0x1
#define WLANDXE_CH_CTRL_XTYPE_DEFAULT         0x0
#define WLANDXE_CH_CTRL_XTYPE_EH2H            0x0
#define WLANDXE_CH_CTRL_XTYPE_EB2B            0x1
#define WLANDXE_CH_CTRL_XTYPE_EH2B            0x2
#define WLANDXE_CH_CTRL_XTYPE_EB2H            0x3

#define WLANDXE_CH_CTRL_DONE_MASK             0x4

#define WLANDXE_CH_CTRL_ERR_MASK              0x20

#define WLANDXE_CH_CTRL_MASKED_MASK           0x8

#define WLANDXE_CH_CTRL_EN_MASK               0x1
#define WLANDXE_CH_CTRL_EN_OFFSET             0x0
#define WLANDXE_CH_CTRL_EN_DEFAULT            0x0
#define WLANDXE_CH_CTRL_DEFAULT               0x10000708


#define WLANDXE_DESC_CTRL_VALID          0x00000001
#define WLANDXE_DESC_CTRL_XTYPE_MASK     0x00000006
#define WLANDXE_DESC_CTRL_XTYPE_H2H      0x00000000
#define WLANDXE_DESC_CTRL_XTYPE_B2B      0x00000002
#define WLANDXE_DESC_CTRL_XTYPE_H2B      0x00000004
#define WLANDXE_DESC_CTRL_XTYPE_B2H      0x00000006
#define WLANDXE_DESC_CTRL_EOP            0x00000008
#define WLANDXE_DESC_CTRL_BDH            0x00000010
#define WLANDXE_DESC_CTRL_SIQ            0x00000020
#define WLANDXE_DESC_CTRL_DIQ            0x00000040
#define WLANDXE_DESC_CTRL_PIQ            0x00000080
#define WLANDXE_DESC_CTRL_PDU_REL        0x00000100
#define WLANDXE_DESC_CTRL_BTHLD_SEL      0x00001E00
#define WLANDXE_DESC_CTRL_PRIO           0x0000E000
#define WLANDXE_DESC_CTRL_STOP           0x00010000
#define WLANDXE_DESC_CTRL_INT            0x00020000
#define WLANDXE_DESC_CTRL_BDT_SWAP       0x00100000
#define WLANDXE_DESC_CTRL_ENDIANNESS     0x00200000
#define WLANDXE_DESC_CTRL_DFMT           0x10000000
#define WLANDXE_DESC_CTRL_RSVD           0xfffc0000
/* CSR Register Control mask and offset */

#define WLANDXE_CH_STAT_INT_DONE_MASK   0x00008000
#define WLANDXE_CH_STAT_INT_ERR_MASK    0x00004000
#define WLANDXE_CH_STAT_INT_ED_MASK     0x00002000
#define WLANDXE_CH_STAT_ERR_CODE_MASK   0x000007c0
#define WLANDXE_CH_STAT_ERR_CODE_OFFSET (6)

#define WLANDXE_CH_STAT_MASKED_MASK     0x00000008
#define WLANDXE_CH_STAT_ENABLED_MASK    0x00000001
/* Till here WCNSS DXE register information
 * This is temporary definition location to make compile and unit test
 * If official msmreg.h integrated, this part will be eliminated */

/* Interrupt control channel mask */
#define WLANDXE_INT_MASK_CHAN_0          0x00000001
#define WLANDXE_INT_MASK_CHAN_1          0x00000002
#define WLANDXE_INT_MASK_CHAN_2          0x00000004
#define WLANDXE_INT_MASK_CHAN_3          0x00000008
#define WLANDXE_INT_MASK_CHAN_4          0x00000010
#define WLANDXE_INT_MASK_CHAN_5          0x00000020
#define WLANDXE_INT_MASK_CHAN_6          0x00000040
#define WLANDXE_INT_MASK_CHAN_7          0x00000080

#define WLANDXE_TX_LOW_RES_THRESHOLD     (5)

typedef enum {
   WLANDXE_ERROR_NONE                = 0,
   WLANDXE_ERROR_SAHB_ERR            = 1,
   WLANDXE_ERROR_H2H_RD_BUS_ERR      = 2,
   WLANDXE_ERROR_H2H_WR_BUS_ERR      = 3,
   WLANDXE_ERROR_PRG_INV_XTYPE       = 4,
   WLANDXE_ERROR_BERR_POPWQ          = 5,
   WLANDXE_ERROR_BERR_PUSHWQ         = 6,
   WLANDXE_ERROR_BERR_RLSS           = 7,
   WLANDXE_ERROR_BERR_GETPDU         = 8,
   WLANDXE_ERROR_PRG_INV_WQ          = 9,
   WLANDXE_ERROR_PRG_INV_H2H_SRC_QID = 10,
   WLANDXE_ERROR_PRG_INV_H2H_DST_QID = 11,
   WLANDXE_ERROR_PRG_INV_B2H_SRC_QID = 12,
   WLANDXE_ERROR_PRG_INV_B2H_DST_QID = 13,
   WLANDXE_ERROR_PRG_INV_B2H_SRC_IDX = 14,
   WLANDXE_ERROR_PRG_INV_H2B_SRC_QID = 15,
   WLANDXE_ERROR_PRG_INV_H2B_DST_QID = 16,
   WLANDXE_ERROR_PRG_INV_H2B_DST_IDX = 17,
   WLANDXE_ERROR_PRG_INV_H2B_SZ      = 18,
   WLANDXE_ERROR_PRG_INV_SADR        = 19,
   WLANDXE_ERROR_PRG_INV_DADR        = 20,
   WLANDXE_ERROR_PRG_INV_EDADR       = 21,
   WLANDXE_ERROR_PRG_INV_SRC_WQID    = 22,
   WLANDXE_ERROR_PRG_INV_DST_WQID    = 23,
   WLANDXE_ERROR_PRG_XTYPE_MSMTCH    = 24,
   WLANDXE_ERROR_PKT_ERR             = 25,
   WLANDXE_ERROR_ABORT               = 26,
   WLANDXE_ERROR_PDU_CNT_OVFL        = 27,
}WLANDXE_ErrorCode;

/* DXE Descriptor Endian swap macro */
#ifdef WLANDXE_ENDIAN_SWAP_ENABLE
#define WLANDXE_U32_SWAP_ENDIAN(a) (((a & 0x000000FF) << 24) |    \
                                    ((a & 0x0000FF00) << 8)  |    \
                                    ((a & 0x00FF0000) >> 8)  |    \
                                    ((a & 0xFF000000) >> 24))
#else
/* If DXE HW does not need endian swap, DO NOTHING */
#define WLANDXE_U32_SWAP_ENDIAN(a) (a)
#endif /* WLANDXE_ENDIAN_SWAP_ENABLE */

/* Log Definition will be mappped with PAL MSG */
#define HDXE_MSG                     WPAL_TRACE
#define HDXE_ASSERT(a)               VOS_ASSERT(a)

#define WLANDXE_PRONTO_TX_WQ       0x6
/*----------------------------------------------------------------------------
 *  Type Declarations
 * -------------------------------------------------------------------------*/
/* DMA Channel Q handle Method type
  * Linear handle or circular */
typedef enum
{
   WLANDXE_CHANNEL_HANDLE_LINEAR,
   WLANDXE_CHANNEL_HANDLE_CIRCULA
}WLANDXE_ChannelHandleType;

typedef enum
{
   WLANDXE_TX_COMP_INT_LR_THRESHOLD,
   WLANDXE_TX_COMP_INT_PER_K_FRAMES,
   WLANDXE_TX_COMP_INT_TIMER
} WLANDXE_TXCompIntEnableType;

typedef enum
{
   WLANDXE_SHORT_DESCRIPTOR,
   WLANDXE_LONG_DESCRIPTOR
} WLANDXE_DescriptorType;

typedef enum
{
   WLANDXE_DMA_CHANNEL_0,
   WLANDXE_DMA_CHANNEL_1,
   WLANDXE_DMA_CHANNEL_2,
   WLANDXE_DMA_CHANNEL_3,
   WLANDXE_DMA_CHANNEL_4,
   WLANDXE_DMA_CHANNEL_5,
   WLANDXE_DMA_CHANNEL_6,
   WLANDXE_DMA_CHANNEL_7,
   WLANDXE_DMA_CHANNEL_MAX
} WLANDXE_DMAChannelType;

/** DXE HW Long Descriptor format */
typedef struct
{
   wpt_uint32                      srcMemAddrL;
   wpt_uint32                      srcMemAddrH;
   wpt_uint32                      dstMemAddrL;
   wpt_uint32                      dstMemAddrH;
   wpt_uint32                      phyNextL;
   wpt_uint32                      phyNextH;
} WLANDXE_LongDesc;


/** DXE HW Short Descriptor format */
typedef struct tDXEShortDesc
{
   wpt_uint32                      srcMemAddrL;
   wpt_uint32                      dstMemAddrL;
   wpt_uint32                      phyNextL;
} WLANDXE_ShortDesc;


/* DXE Descriptor Data Type
  * Pick up from GEN5 */
typedef struct
{
   union
   {
      wpt_uint32                   ctrl;
      wpt_uint32                   valid          :1;     //0 = DMA stop, 1 = DMA continue with this descriptor
      wpt_uint32                   transferType   :2;     //0 = Host to Host space
      wpt_uint32                   eop            :1;     //End of Packet
      wpt_uint32                   bdHandling     :1;          //if transferType = Host to BMU, then 0 means first 128 bytes contain BD, and 1 means create new empty BD
      wpt_uint32                   siq            :1;     // SIQ
      wpt_uint32                   diq            :1;     // DIQ
      wpt_uint32                   pduRel         :1;     //0 = don't release BD and PDUs when done, 1 = release them
      wpt_uint32                   bthldSel       :4;     //BMU Threshold Select
      wpt_uint32                   prio           :3;     //Specifies the priority level to use for the transfer
      wpt_uint32                   stopChannel    :1;     //1 = DMA stops processing further, channel requires re-enabling after this
      wpt_uint32                   intr           :1;     //Interrupt on Descriptor Done
      wpt_uint32                   rsvd           :1;     //reserved
      wpt_uint32                   transferSize   :14;    //14 bits used - ignored for BMU transfers, only used for host to host transfers?
   } descCtrl;
   wpt_uint32                      xfrSize;
   union
   {
      WLANDXE_LongDesc             dxe_long_desc;
      WLANDXE_ShortDesc            dxe_short_desc;
   }dxedesc; 
} WLANDXE_DescType;

typedef struct
{
   void                            *nextCtrlBlk;
   wpt_packet                      *xfrFrame;  
   WLANDXE_DescType                *linkedDesc;
   wpt_uint32                       linkedDescPhyAddr;
   wpt_uint32                       ctrlBlkOrder;
} WLANDXE_DescCtrlBlkType;

typedef struct
{
   /* Q handle method, linear or ring */
   WLANDXE_ChannelHandleType       queueMethod;

   /* Number of descriptors for DXE that can be queued for transfer at one time */
   wpt_uint32                      nDescs;

   /* Maximum number of receive buffers  of shared memory to use for this pipe */
   wpt_uint32                      nRxBuffers;

   /* Reference WQ - for H2B and B2H only */
   wpt_uint32                      refWQ;

   /* for usb only, endpoint info for CH_SADR or CH_DADR */
   wpt_uint32                      refEP;

   /* H2B(Tx), B2H(Rx), H2H(SRAM<->HostMem R/W) */
   wpt_uint32                      xfrType;

   /* Channel Priority 7(Highest) - 0(Lowest) */
   wpt_uint32                      chPriority;

   /* 1 = BD attached to frames for this pipe */
   wpt_boolean                     bdPresent;

   wpt_uint32                      chk_size;

   wpt_uint32                      bmuThdSel;

   /*  Added in Gen5 for Prefetch */
   wpt_boolean                     useLower4G;

   wpt_boolean                     useShortDescFmt;
   /* Till here inharited from GEN5 code */
   /* From now on, added for PRIMA  */
} WLANDXE_ChannelConfigType;

typedef struct
{
   wpt_uint32                      chDXEBaseAddr;
   wpt_uint32                      chDXEStatusRegAddr;
   wpt_uint32                      chDXEDesclRegAddr;
   wpt_uint32                      chDXEDeschRegAddr;
   wpt_uint32                      chDXELstDesclRegAddr;
   wpt_uint32                      chDXECtrlRegAddr;
   wpt_uint32                      chDXESzRegAddr;
   wpt_uint32                      chDXEDadrlRegAddr;
   wpt_uint32                      chDXEDadrhRegAddr;
   wpt_uint32                      chDXESadrlRegAddr;
   wpt_uint32                      chDXESadrhRegAddr;
} WLANDXE_ChannelRegisterType;

typedef struct
{
   wpt_uint32                      refWQ_swapped;
   wpt_boolean                     chEnabled;
   wpt_boolean                     chConfigured;    
   wpt_uint32                      channel;
   wpt_uint32                      chk_size_mask;
   wpt_uint32                      bmuThdSel_mask;
   wpt_uint32                      cw_ctrl_read;
   wpt_uint32                      cw_ctrl_write;
   wpt_uint32                      cw_ctrl_write_valid;
   wpt_uint32                      cw_ctrl_write_eop;
   wpt_uint32                      cw_ctrl_write_eop_int;
   wpt_uint32                      chan_mask;
   wpt_uint32                      chan_mask_read_disable;
   wpt_uint32                      intMask;
} WLANDXE_ChannelExConfigType;

typedef struct
{
   WDTS_ChannelType                channelType;
   WLANDXE_DescCtrlBlkType        *headCtrlBlk;
   WLANDXE_DescCtrlBlkType        *tailCtrlBlk;
   WLANDXE_DescType               *descriptorAllocation;
   WLANDXE_DescType               *DescBottomLoc;
   wpt_uint32                      descBottomLocPhyAddr;
   wpt_uint32                      numDesc;
   wpt_uint32                      numFreeDesc;
   wpt_uint32                      numRsvdDesc;
   wpt_uint32                      desc_write_fail_count;
   wpt_uint32                      maxFrameSize;
   wpt_uint32                      numFragmentCurrentChain;
   wpt_uint32                      numFrameBeforeInt;
   wpt_uint32                      numTotalFrame;
   wpt_uint32                      doneIntDisabled;
   wpt_mutex                       dxeChannelLock;
   wpt_boolean                     hitLowResource;
   WLANDXE_ChannelConfigType       channelConfig;
   WLANDXE_ChannelRegisterType     channelRegister;
   WLANDXE_ChannelExConfigType     extraConfig;
   WLANDXE_DMAChannelType          assignedDMAChannel;
   wpt_uint64                      rxDoneHistogram;
} WLANDXE_ChannelCBType;

typedef struct
{
   WLANDXE_TXCompIntEnableType     txIntEnable;
   unsigned int                    txLowResourceThreshold_LoPriCh;
   unsigned int                    txLowResourceThreshold_HiPriCh;
   unsigned int                    rxLowResourceThreshold;
   unsigned int                    txInterruptEnableFrameCount;
   unsigned int                    txInterruptEnablePeriod;
} WLANDXE_TxCompIntConfigType;

typedef struct
{
   WLANDXE_ChannelCBType           dxeChannel[WDTS_CHANNEL_MAX];
   WLANDXE_RxFrameReadyCbType      rxReadyCB;
   WLANDXE_TxCompleteCbType        txCompCB;
   WLANDXE_LowResourceCbType       lowResourceCB;
   WLANDXE_MbReceiveMsgCbType      receiveMbMsgCB;
   WLANDXE_RxLogDoneType           receiveLogCompleteCB;
   WLANDXE_TxCompIntConfigType     txCompInt;
   void                           *clientCtxt;
   wpt_uint32                      interruptPath;
   wpt_msg                        *rxIsrMsg;
   wpt_msg                        *txIsrMsg;
   wpt_msg                        *rxPktAvailMsg;
   volatile WLANDXE_PowerStateType hostPowerState;
   wpt_boolean                     rxIntDisabledByIMPS;
   wpt_boolean                     txIntDisabledByIMPS;
   WLANDXE_SetPowerStateCbType     setPowerStateCb;
   volatile WLANDXE_RivaPowerStateType rivaPowerState;
   wpt_boolean                     ringNotEmpty;
   wpt_boolean                     txIntEnable;
   wpt_uint32                      txCompletedFrames;
   wpt_uint8                       ucTxMsgCnt;
   wpt_uint16                      lastKickOffDxe;
   wpt_uint32                      smsmRingsEmptyHistogram;
   wpt_uint32                      smsmDxeHistogram;
   wpt_uint32                      dxeCookie;
   wpt_packet                     *freeRXPacket;
   wpt_boolean                     rxPalPacketUnavailable;
   wpt_boolean                     driverReloadInProcessing;
   wpt_boolean                     smsmToggled;
   wpt_boolean                     txRingsEmpty;
   wpt_boolean                     hostInitiatedH2H;
#ifdef WLAN_DXE_LOW_RESOURCE_TIMER
   wpt_timer                       rxResourceAvailableTimer;
#endif
   wpt_timer                       dxeSSRTimer;
} WLANDXE_CtrlBlkType;

typedef struct
{
   u64                             *rxIntDisableReturn;
   wpt_uint8                       rxIntChanlSrc;
   wpt_uint8                       txCmpIntChanlSrc;
} WLANDXE_EnvInformation;
/*==========================================================================
  @  Function Name 
      dxeCommonDefaultConfig

  @  Description 

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block

  @  Return
      void

===========================================================================*/
extern void dxeCommonDefaultConfig
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk
);

/*==========================================================================
  @  Function Name 
      dxeChannelDefaultConfig

  @  Description 
      Get defualt configuration values from pre defined structure
      All the channels must have it's own configurations

  @  Parameters
      WLANDXE_CtrlBlkType     *dxeCtrlBlk,
                               DXE host driver main control block
      WLANDXE_ChannelCBType   *channelEntry
                               Channel specific control block

  @  Return
      wpt_status

===========================================================================*/
extern wpt_status dxeChannelDefaultConfig
(
   WLANDXE_CtrlBlkType     *dxeCtrlBlk,
   WLANDXE_ChannelCBType   *channelEntry
);

void dxeSetEnabledChannels
(
   wpt_uint8 enabledChannels
);

wpt_uint8 dxeGetEnabledChannels
(
   void
);
#endif /* WLAN_QCT_DXE_I_H */
