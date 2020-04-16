/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __CCU_A_REGS_H__
#define __CCU_A_REGS_H__

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------- CCU_A Bit Field Definitions -------------------*/

#define PACKING
#define FIELD unsigned int

PACKING union CCU_A_REG_RESET
{
	PACKING struct
	{
		FIELD RDMA_SOFT_RST_ST          : 1;
		FIELD WDMA_SOFT_RST_ST          : 1;
		FIELD rsv_2                     : 6;
		FIELD RDMA_SOFT_RST             : 1;
		FIELD WDMA_SOFT_RST             : 1;
		FIELD rsv_10                    : 6;
		FIELD CCU_HW_RST                : 1;
		FIELD ARBITER_HW_RST            : 1;
		FIELD H2T_A_HW_RST              : 1;
		FIELD T2H_A_HW_RST              : 1;
		FIELD rsv_20                    : 1;
		FIELD RDMA_HW_RST               : 1;
		FIELD WDMA_HW_RST               : 1;
		FIELD AHB2GMC_HW_RST            : 1;
		FIELD rsv_24                    : 8;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_START_TRIG
{
	PACKING struct
	{
		FIELD rsv_0                     : 1;
		FIELD H2T_A_START               : 1;
		FIELD T2H_A_START               : 1;
		FIELD RDMA_START                : 1;
		FIELD WDMA_START                : 1;
		FIELD rsv_5                     : 27;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_BANK_INCR
{
	PACKING struct
	{
		FIELD H2T_A_FINISH              : 1;
		FIELD T2H_A_FINISH              : 1;
		FIELD rsv_2                     : 30;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CTRL
{
	PACKING struct
	{
		FIELD RDMA_ENABLE               : 1;
		FIELD WDMA_ENABLE               : 1;
		FIELD CCU_PRINNER               : 1;
		FIELD DB_LOAD_DISABLE           : 1;
		FIELD INT_CLR_MODE              : 1;
		FIELD H2X_SECURE_EN             : 1;
		FIELD H2X_MSB                   : 1;
		FIELD H2X_DOMAIN_ID             : 5;
		FIELD DMA_PMEM_EN               : 1;
		FIELD CCUI_DCM_DIS              : 1;
		FIELD CCUO_DCM_DIS              : 1;
		FIELD SRAM_DCM_DIS              : 1;
		FIELD CCU_PROT_RANGE            : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_DONE_ST
{
	PACKING struct
	{
		FIELD T2H_A_BANK_DONE           : 1;
		FIELD T2H_A_FRAME_DONE          : 1;
		FIELD rsv_2                     : 30;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_DONE_ST
{
	PACKING struct
	{
		FIELD H2T_A_BANK_DONE           : 1;
		FIELD H2T_A_FRAME_DONE          : 1;
		FIELD rsv_2                     : 30;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_WDMA_DONE_ST
{
	PACKING struct
	{
		FIELD WDMA_DONE_ST              : 1;
		FIELD rsv_1                     : 31;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REGXI_REMAP
{
	PACKING struct
	{
		FIELD CCU_AXI_OFFSET            : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_CTL
{
	PACKING struct
	{
		FIELD CCU_HALT_REQ              : 1;
		FIELD rsv_1                     : 3;
		FIELD CCU_RUN_REQ               : 1;
		FIELD rsv_5                     : 3;
		FIELD CCU_WAKE_EVT              : 1;
		FIELD rsv_9                     : 3;
		FIELD CCU_ICCM_AHB_PRIO         : 1;
		FIELD CCU_DCCM_AHB_PRIO         : 1;
		FIELD rsv_14                    : 18;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_ST
{
	PACKING struct
	{
		FIELD CCU_HALT_ACK              : 1;
		FIELD rsv_1                     : 3;
		FIELD CCU_RUN_ACK               : 1;
		FIELD rsv_5                     : 3;
		FIELD CCU_SYS_HALT              : 1;
		FIELD rsv_9                     : 3;
		FIELD CCU_SYS_SLEEP             : 1;
		FIELD CCU_SYS_SLEEP_MODE        : 3;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_H2X_CFG
{
	PACKING struct
	{
		FIELD H2X_PREULTRA              : 1;
		FIELD H2X_ULTRA                 : 1;
		FIELD rsv_2                     : 2;
		FIELD H2X_MERGE_EN              : 1;
		FIELD H2X_BUFFER_EN             : 1;
		FIELD rsv_6                     : 26;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INT_EN
{
	PACKING struct
	{
		FIELD CCU_ERR_INT_EN            : 1;
		FIELD ISP_INT_EN                : 1;
		FIELD APMCU_INT_EN              : 1;
		FIELD PSO_INT_EN                : 1;
		FIELD T2H_INT_EN                : 1;
		FIELD H2T_INT_EN                : 1;
		FIELD AFO_INT_EN                : 1;
		FIELD AAO_INT_EN                : 1;
		FIELD CAMSV_0_INT_EN            : 1;
		FIELD CAMSV_1_INT_EN            : 1;
		FIELD CAMSV_2_INT_EN            : 1;
		FIELD CAMSV_3_INT_EN            : 1;
		FIELD rsv_12                    : 20;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INT
{
	PACKING struct
	{
		FIELD INT_CCU                   : 1;
		FIELD rsv_1                     : 31;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CTL_CCU_INT
{
	PACKING struct
	{
		FIELD INT_CTL_CCU               : 1;
		FIELD rsv_1                     : 31;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DCM_ST
{
	PACKING struct
	{
		FIELD CCUI_DCM_ST               : 1;
		FIELD rsv_1                     : 3;
		FIELD CCUO_DCM_ST               : 1;
		FIELD rsv_5                     : 3;
		FIELD SRAM_DCM_ST               : 1;
		FIELD rsv_9                     : 23;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_ERR_ST
{
	PACKING struct
	{
		FIELD DMA_ERR_INT               : 1;
		FIELD rsv_1                     : 7;
		FIELD DMA_REQ_ST                : 4;
		FIELD rsv_12                    : 4;
		FIELD DMA_RDY_ST                : 1;
		FIELD rsv_17                    : 15;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_DEBUG
{
	PACKING struct
	{
		FIELD DMA_DEBUG                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EINTC_MASK
{
	PACKING struct
	{
		FIELD CCU_EINTC_MASK            : 8;
		FIELD rsv_8                     : 8;
		FIELD CCU_EINTC_MODE            : 1;
		FIELD rsv_17                    : 15;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EINTC_CLR
{
	PACKING struct
	{
		FIELD CCU_EINTC_CLR             : 8;
		FIELD rsv_8                     : 24;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EINTC_ST
{
	PACKING struct
	{
		FIELD CCU_EINTC_ST              : 8;
		FIELD rsv_8                     : 24;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EINTC_MISC
{
	PACKING struct
	{
		FIELD CCU_EINTC_RAW_ST          : 8;
		FIELD rsv_8                     : 8;
		FIELD CCU_EINTC_TRIG_ST         : 1;
		FIELD rsv_17                    : 15;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REGHB_REMAP_A
{
	PACKING struct
	{
		FIELD CCU_AHB_REMAP_0           : 10;
		FIELD rsv_10                    : 6;
		FIELD CCU_AHB_REMAP_1           : 10;
		FIELD rsv_26                    : 6;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REGHB_REMAP_B
{
	PACKING struct
	{
		FIELD CCU_AHB_REMAP_2           : 10;
		FIELD rsv_10                    : 6;
		FIELD CCU_AHB_REMAP_3           : 10;
		FIELD rsv_26                    : 6;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REGHB_REMAP_C
{
	PACKING struct
	{
		FIELD CCU_AHB_REMAP_4           : 10;
		FIELD rsv_10                    : 6;
		FIELD CCU_AHB_REMAP_5           : 10;
		FIELD rsv_26                    : 6;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REGHB_REMAP_D
{
	PACKING struct
	{
		FIELD CCU_AHB_REMAP_6           : 10;
		FIELD rsv_10                    : 6;
		FIELD CCU_AHB_REMAP_7           : 10;
		FIELD rsv_26                    : 6;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_PC
{
	PACKING struct
	{
		FIELD CCU_CCU_PC                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TOP_SPARE
{
	PACKING struct
	{
		FIELD CCU_SPARE                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO00
{
	PACKING struct
	{
		FIELD CCU_INFO0                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO01
{
	PACKING struct
	{
		FIELD CCU_INFO1                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO02
{
	PACKING struct
	{
		FIELD CCU_INFO2                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO03
{
	PACKING struct
	{
		FIELD CCU_INFO3                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO04
{
	PACKING struct
	{
		FIELD CCU_INFO4                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO05
{
	PACKING struct
	{
		FIELD CCU_INFO5                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO06
{
	PACKING struct
	{
		FIELD CCU_INFO6                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO07
{
	PACKING struct
	{
		FIELD CCU_INFO7                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO08
{
	PACKING struct
	{
		FIELD CCU_INFO8                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO09
{
	PACKING struct
	{
		FIELD CCU_INFO9                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO10
{
	PACKING struct
	{
		FIELD CCU_INFO10                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO11
{
	PACKING struct
	{
		FIELD CCU_INFO11                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO12
{
	PACKING struct
	{
		FIELD CCU_INFO12                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO13
{
	PACKING struct
	{
		FIELD CCU_INFO13                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO14
{
	PACKING struct
	{
		FIELD CCU_INFO14                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO15
{
	PACKING struct
	{
		FIELD CCU_INFO15                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO16
{
	PACKING struct
	{
		FIELD CCU_INFO16                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO17
{
	PACKING struct
	{
		FIELD CCU_INFO17                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO18
{
	PACKING struct
	{
		FIELD CCU_INFO18                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO19
{
	PACKING struct
	{
		FIELD CCU_INFO19                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO20
{
	PACKING struct
	{
		FIELD CCU_INFO20                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO21
{
	PACKING struct
	{
		FIELD CCU_INFO21                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO22
{
	PACKING struct
	{
		FIELD CCU_INFO22                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO23
{
	PACKING struct
	{
		FIELD CCU_INFO23                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO24
{
	PACKING struct
	{
		FIELD CCU_INFO24                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO25
{
	PACKING struct
	{
		FIELD CCU_INFO25                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO26
{
	PACKING struct
	{
		FIELD CCU_INFO26                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO27
{
	PACKING struct
	{
		FIELD CCU_INFO27                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO28
{
	PACKING struct
	{
		FIELD CCU_INFO28                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO29
{
	PACKING struct
	{
		FIELD CCU_INFO29                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO30
{
	PACKING struct
	{
		FIELD CCU_INFO30                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_INFO31
{
	PACKING struct
	{
		FIELD CCU_INFO31                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_AAO_INT_PERIOD
{
	PACKING struct
	{
		FIELD AAO_A_INT_PRD             : 16;
		FIELD AAO_B_INT_PRD             : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_AAO_INT_CNT
{
	PACKING struct
	{
		FIELD AAO_A_INT_CNT             : 16;
		FIELD AAO_B_INT_CNT             : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_AFO_INT_PERIOD
{
	PACKING struct
	{
		FIELD AFO_A_INT_PRD             : 16;
		FIELD AFO_B_INT_PRD             : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_AFO_INT_CNT
{
	PACKING struct
	{
		FIELD AFO_A_INT_CNT             : 16;
		FIELD AFO_B_INT_CNT             : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_PSO_INT_PERIOD
{
	PACKING struct
	{
		FIELD PSO_A_INT_PRD             : 16;
		FIELD PSO_B_INT_PRD             : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_PSO_INT_CNT
{
	PACKING struct
	{
		FIELD PSO_A_INT_CNT             : 16;
		FIELD PSO_B_INT_CNT             : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_AAO_FRAME_BCNT
{
	PACKING struct
	{
		FIELD AAO_A_FRAME_BCNT          : 16;
		FIELD AAO_B_FRAME_BCNT          : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_AAO_INT_ST
{
	PACKING struct
	{
		FIELD AAO_A_FRAME_INT_ST        : 1;
		FIELD AAO_A_BANK_INT_ST         : 1;
		FIELD AAO_B_FRAME_INT_ST        : 1;
		FIELD AAO_B_BANK_INT_ST         : 1;
		FIELD rsv_4                     : 28;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_AFO_FRAME_BCNT
{
	PACKING struct
	{
		FIELD AFO_A_FRAME_BCNT          : 16;
		FIELD AFO_B_FRAME_BCNT          : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_AFO_INT_ST
{
	PACKING struct
	{
		FIELD AFO_A_FRAME_INT_ST        : 1;
		FIELD AFO_A_BANK_INT_ST         : 1;
		FIELD AFO_B_FRAME_INT_ST        : 1;
		FIELD AFO_B_BANK_INT_ST         : 1;
		FIELD rsv_4                     : 28;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_PSO_FRAME_BCNT
{
	PACKING struct
	{
		FIELD PSO_A_FRAME_BCNT          : 16;
		FIELD PSO_B_FRAME_BCNT          : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_PSO_INT_ST
{
	PACKING struct
	{
		FIELD PSO_A_FRAME_INT_ST        : 1;
		FIELD PSO_A_BANK_INT_ST         : 1;
		FIELD PSO_B_FRAME_INT_ST        : 1;
		FIELD PSO_B_BANK_INT_ST         : 1;
		FIELD rsv_4                     : 28;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_INT_EN
{
	PACKING struct
	{
		FIELD CAMSV_0_LINE_INT_EN       : 1;
		FIELD CAMSV_0_FRAME_INT_EN      : 1;
		FIELD CAMSV_0_SOF_INT_EN        : 1;
		FIELD CAMSV_0_IRQ_INT_EN        : 1;
		FIELD CAMSV_0_TOG_EN            : 1;
		FIELD rsv_5                     : 3;
		FIELD CAMSV_1_LINE_INT_EN       : 1;
		FIELD CAMSV_1_FRAME_INT_EN      : 1;
		FIELD CAMSV_1_SOF_INT_EN        : 1;
		FIELD CAMSV_1_IRQ_INT_EN        : 1;
		FIELD CAMSV_1_TOG_EN            : 1;
		FIELD rsv_13                    : 3;
		FIELD CAMSV_2_LINE_INT_EN       : 1;
		FIELD CAMSV_2_FRAME_INT_EN      : 1;
		FIELD CAMSV_2_SOF_INT_EN        : 1;
		FIELD CAMSV_2_IRQ_INT_EN        : 1;
		FIELD CAMSV_2_TOG_EN            : 1;
		FIELD rsv_21                    : 3;
		FIELD CAMSV_3_LINE_INT_EN       : 1;
		FIELD CAMSV_3_FRAME_INT_EN      : 1;
		FIELD CAMSV_3_SOF_INT_EN        : 1;
		FIELD CAMSV_3_IRQ_INT_EN        : 1;
		FIELD CAMSV_3_TOG_EN            : 1;
		FIELD rsv_29                    : 3;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_0_ICNT
{
	PACKING struct
	{
		FIELD CAMSV_0_INT_CNT           : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_0_INT_ST
{
	PACKING struct
	{
		FIELD CAMSV_0_LINE_INT_ST       : 1;
		FIELD CAMSV_0_FRAME_INT_ST      : 1;
		FIELD CAMSV_0_SOF_INT_ST        : 1;
		FIELD CAMSV_0_IRQ_INT_ST        : 1;
		FIELD rsv_4                     : 28;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_1_ICNT
{
	PACKING struct
	{
		FIELD CAMSV_1_INT_CNT           : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_1_INT_ST
{
	PACKING struct
	{
		FIELD CAMSV_1_LINE_INT_ST       : 1;
		FIELD CAMSV_1_FRAME_INT_ST      : 1;
		FIELD CAMSV_1_SOF_INT_ST        : 1;
		FIELD CAMSV_1_IRQ_INT_ST        : 1;
		FIELD rsv_4                     : 28;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_2_ICNT
{
	PACKING struct
	{
		FIELD CAMSV_2_INT_CNT           : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_2_INT_ST
{
	PACKING struct
	{
		FIELD CAMSV_2_LINE_INT_ST       : 1;
		FIELD CAMSV_2_FRAME_INT_ST      : 1;
		FIELD CAMSV_2_SOF_INT_ST        : 1;
		FIELD CAMSV_2_IRQ_INT_ST        : 1;
		FIELD rsv_4                     : 28;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_3_ICNT
{
	PACKING struct
	{
		FIELD CAMSV_3_INT_CNT           : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_3_INT_ST
{
	PACKING struct
	{
		FIELD CAMSV_3_LINE_INT_ST       : 1;
		FIELD CAMSV_3_FRAME_INT_ST      : 1;
		FIELD CAMSV_3_SOF_INT_ST        : 1;
		FIELD CAMSV_3_IRQ_INT_ST        : 1;
		FIELD rsv_4                     : 28;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_SOF_CNT
{
	PACKING struct
	{
		FIELD CCU_SOF_CNT               : 32;
	} Bits;
	UINT32 Raw;
};

/* ----------------- CCU_A  Grouping Definitions -------------------*/
/* ----------------- CCU_A Register Definition -------------------*/
PACKING struct CCU_A_REGS
{
	PACKING union CCU_A_REG_RESET             RESET;            /* 1000*/
	PACKING union CCU_A_REG_START_TRIG        START_TRIG;       /* 1004*/
	PACKING union CCU_A_REG_BANK_INCR         BANK_INCR;        /* 1008*/
	PACKING union CCU_A_REG_CTRL              CTRL;             /* 100C*/
	PACKING union CCU_A_REG_T2H_A_DONE_ST     T2H_A_DONE_ST;    /* 1010*/
	UINT32                          rsv_1014;         /* 1014*/

	PACKING union CCU_A_REG_H2T_A_DONE_ST     H2T_A_DONE_ST;    /* 1018*/
	PACKING union CCU_A_REG_WDMA_DONE_ST      WDMA_DONE_ST;     /* 101C*/
	PACKING union CCU_A_REGXI_REMAP           CCU_AXI_REMAP;    /* 1020*/
	PACKING union CCU_A_REG_CCU_CTL           CCU_CTL;          /* 1024*/
	PACKING union CCU_A_REG_CCU_ST            CCU_ST;           /* 1028*/
	PACKING union CCU_A_REG_CCU_H2X_CFG       CCU_H2X_CFG;      /* 102C*/
	UINT32                          rsv_1030;         /* 1030*/

	PACKING union CCU_A_REG_CCU_INT_EN        CCU_INT_EN;       /* 1034*/
	PACKING union CCU_A_REG_CCU_INT           CCU_INT;          /* 1038*/
	PACKING union CCU_A_REG_CTL_CCU_INT       CTL_CCU_INT;      /* 103C*/
	PACKING union CCU_A_REG_DCM_ST            DCM_ST;           /* 1040*/
	PACKING union CCU_A_REG_DMA_ERR_ST        DMA_ERR_ST;       /* 1044*/
	PACKING union CCU_A_REG_DMA_DEBUG         DMA_DEBUG;        /* 1048*/
	UINT32                          rsv_104C;         /* 104C*/

	PACKING union CCU_A_REG_EINTC_MASK        EINTC_MASK;       /* 1050*/
	PACKING union CCU_A_REG_EINTC_CLR         EINTC_CLR;        /* 1054*/
	PACKING union CCU_A_REG_EINTC_ST          EINTC_ST;         /* 1058*/
	PACKING union CCU_A_REG_EINTC_MISC        EINTC_MISC;       /* 105C*/
	PACKING union CCU_A_REGHB_REMAP_A         CCU_AHB_REMAP_A;  /* 1060*/
	PACKING union CCU_A_REGHB_REMAP_B         CCU_AHB_REMAP_B;  /* 1064*/
	PACKING union CCU_A_REGHB_REMAP_C         CCU_AHB_REMAP_C;  /* 1068*/
	PACKING union CCU_A_REGHB_REMAP_D         CCU_AHB_REMAP_D;  /* 106C*/
	PACKING union CCU_A_REG_CCU_PC            CCU_PC;           /* 1070*/
	PACKING union CCU_A_REG_TOP_SPARE         TOP_SPARE;        /* 1074*/
	UINT32                          rsv_1078[2];      /* 1078..107C*/

	PACKING union CCU_A_REG_CCU_INFO00        CCU_INFO00;       /* 1080*/
	PACKING union CCU_A_REG_CCU_INFO01        CCU_INFO01;       /* 1084*/
	PACKING union CCU_A_REG_CCU_INFO02        CCU_INFO02;       /* 1088*/
	PACKING union CCU_A_REG_CCU_INFO03        CCU_INFO03;       /* 108C*/
	PACKING union CCU_A_REG_CCU_INFO04        CCU_INFO04;       /* 1090*/
	PACKING union CCU_A_REG_CCU_INFO05        CCU_INFO05;       /* 1094*/
	PACKING union CCU_A_REG_CCU_INFO06        CCU_INFO06;       /* 1098*/
	PACKING union CCU_A_REG_CCU_INFO07        CCU_INFO07;       /* 109C*/
	PACKING union CCU_A_REG_CCU_INFO08        CCU_INFO08;       /* 10A0*/
	PACKING union CCU_A_REG_CCU_INFO09        CCU_INFO09;       /* 10A4*/
	PACKING union CCU_A_REG_CCU_INFO10        CCU_INFO10;       /* 10A8*/
	PACKING union CCU_A_REG_CCU_INFO11        CCU_INFO11;       /* 10AC*/
	PACKING union CCU_A_REG_CCU_INFO12        CCU_INFO12;       /* 10B0*/
	PACKING union CCU_A_REG_CCU_INFO13        CCU_INFO13;       /* 10B4*/
	PACKING union CCU_A_REG_CCU_INFO14        CCU_INFO14;       /* 10B8*/
	PACKING union CCU_A_REG_CCU_INFO15        CCU_INFO15;       /* 10BC*/
	PACKING union CCU_A_REG_CCU_INFO16        CCU_INFO16;       /* 10C0*/
	PACKING union CCU_A_REG_CCU_INFO17        CCU_INFO17;       /* 10C4*/
	PACKING union CCU_A_REG_CCU_INFO18        CCU_INFO18;       /* 10C8*/
	PACKING union CCU_A_REG_CCU_INFO19        CCU_INFO19;       /* 10CC*/
	PACKING union CCU_A_REG_CCU_INFO20        CCU_INFO20;       /* 10D0*/
	PACKING union CCU_A_REG_CCU_INFO21        CCU_INFO21;       /* 10D4*/
	PACKING union CCU_A_REG_CCU_INFO22        CCU_INFO22;       /* 10D8*/
	PACKING union CCU_A_REG_CCU_INFO23        CCU_INFO23;       /* 10DC*/
	PACKING union CCU_A_REG_CCU_INFO24        CCU_INFO24;       /* 10E0*/
	PACKING union CCU_A_REG_CCU_INFO25        CCU_INFO25;       /* 10E4*/
	PACKING union CCU_A_REG_CCU_INFO26        CCU_INFO26;       /* 10E8*/
	PACKING union CCU_A_REG_CCU_INFO27        CCU_INFO27;       /* 10EC*/
	PACKING union CCU_A_REG_CCU_INFO28        CCU_INFO28;       /* 10F0*/
	PACKING union CCU_A_REG_CCU_INFO29        CCU_INFO29;       /* 10F4*/
	PACKING union CCU_A_REG_CCU_INFO30        CCU_INFO30;       /* 10F8*/
	PACKING union CCU_A_REG_CCU_INFO31        CCU_INFO31;       /* 10FC*/
	PACKING union CCU_A_REG_AAO_INT_PERIOD    AAO_INT_PERIOD;   /* 1100*/
	PACKING union CCU_A_REG_AAO_INT_CNT       AAO_INT_CNT;      /* 1104*/
	PACKING union CCU_A_REG_AFO_INT_PERIOD    AFO_INT_PERIOD;   /* 1108*/
	PACKING union CCU_A_REG_AFO_INT_CNT       AFO_INT_CNT;      /* 110C*/
	PACKING union CCU_A_REG_PSO_INT_PERIOD    PSO_INT_PERIOD;   /* 1110*/
	PACKING union CCU_A_REG_PSO_INT_CNT       PSO_INT_CNT;      /* 1114*/
	UINT32                          rsv_1118[2];      /* 1118..111C*/

	PACKING union CCU_A_REG_AAO_FRAME_BCNT    AAO_FRAME_BCNT;   /* 1120*/
	PACKING union CCU_A_REG_AAO_INT_ST        AAO_INT_ST;       /* 1124*/
	PACKING union CCU_A_REG_AFO_FRAME_BCNT    AFO_FRAME_BCNT;   /* 1128*/
	PACKING union CCU_A_REG_AFO_INT_ST        AFO_INT_ST;       /* 112C*/
	PACKING union CCU_A_REG_PSO_FRAME_BCNT    PSO_FRAME_BCNT;   /* 1130*/
	PACKING union CCU_A_REG_PSO_INT_ST        PSO_INT_ST;       /* 1134*/
	UINT32                          rsv_1138;         /* 1138*/

	PACKING union CCU_A_REG_CAMSV_INT_EN      CAMSV_INT_EN;     /* 113C*/
	PACKING union CCU_A_REG_CAMSV_0_ICNT      CAMSV_0_ICNT;     /* 1140*/
	PACKING union CCU_A_REG_CAMSV_0_INT_ST    CAMSV_0_INT_ST;   /* 1144*/
	PACKING union CCU_A_REG_CAMSV_1_ICNT      CAMSV_1_ICNT;     /* 1148*/
	PACKING union CCU_A_REG_CAMSV_1_INT_ST    CAMSV_1_INT_ST;   /* 114C*/
	PACKING union CCU_A_REG_CAMSV_2_ICNT      CAMSV_2_ICNT;     /* 1150*/
	PACKING union CCU_A_REG_CAMSV_2_INT_ST    CAMSV_2_INT_ST;   /* 1154*/
	PACKING union CCU_A_REG_CAMSV_3_ICNT      CAMSV_3_ICNT;     /* 1158*/
	PACKING union CCU_A_REG_CAMSV_3_INT_ST    CAMSV_3_INT_ST;   /* 115C*/
	PACKING union CCU_A_REG_CCU_SOF_CNT       CCU_SOF_CNT;      /* 1160*/
	UINT32                          rsv_1164[14246];  /* 1164..EFF8*/
	UINT8                           rsv_EFFC;         /* EFFC*/
	UINT16                          rsv_EFFD;         /* EFFD*/
	UINT8                           rsv_F0FF;         /* F0FF*/
	UINT32                          rsv_F100[959];    /* F100..FFF8*/
	UINT8                           rsv_FFFC;         /* FFFC*/
};

#ifdef __cplusplus
}
#endif

#endif /* __CCU_A_REGS_H__*/
