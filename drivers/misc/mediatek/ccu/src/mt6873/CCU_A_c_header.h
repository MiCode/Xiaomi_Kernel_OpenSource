// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CCU_A_REGS_H__
#define __CCU_A_REGS_H__

#include "common.h"

// ----------------- CCU_A Bit Field Definitions -------------------

#define PACKING
#define FIELD unsigned int

PACKING union CCU_A_REG_RESET
{
	PACKING struct
	{
		FIELD RDMA_SOFT_RST_ST          : 1;
		FIELD WDMA_SOFT_RST_ST          : 1;
		FIELD H2T_SOFT_RST_ST           : 1;
		FIELD rsv_3                     : 5;
		FIELD RDMA_SOFT_RST             : 1;
		FIELD WDMA_SOFT_RST             : 1;
		FIELD H2T_A_SOFT_RST            : 1;
		FIELD rsv_11                    : 5;
		FIELD CCU_HW_RST                : 1;
		FIELD ARBITER_HW_RST            : 1;
		FIELD H2T_A_HW_RST              : 1;
		FIELD T2H_A_HW_RST              : 1;
		FIELD FBR_HW_RST                : 1;
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
		FIELD rsv_5                     : 1;
		FIELD H2X_MSB                   : 1;
		FIELD rsv_7                     : 5;
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

PACKING union CCU_A_REG_T2H_B_DONE_ST
{
	PACKING struct
	{
		FIELD T2H_B_BANK_DONE           : 1;
		FIELD T2H_B_FRAME_DONE          : 1;
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

PACKING union CCU_A_REG_AXI_REMAP
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

PACKING union CCU_A_REG_H2X_CFG
{
	PACKING struct
	{
		FIELD H2X_ULTRA                 : 1;
		FIELD H2X_PREULTRA              : 1;
		FIELD rsv_2                     : 2;
		FIELD H2X_HUSER                 : 2;
		FIELD rsv_6                     : 2;
		FIELD H2X_MERGE_EN              : 1;
		FIELD H2X_BUFFER_EN             : 1;
		FIELD rsv_10                    : 22;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EXT2CCU_INT_M0
{
	PACKING struct
	{
		FIELD EXT2CCU_INT_M0            : 1;
		FIELD rsv_1                     : 31;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EXT2CCU_INT_M1
{
	PACKING struct
	{
		FIELD EXT2CCU_INT_M1            : 1;
		FIELD rsv_1                     : 31;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EXT2CCU_INT_M2
{
	PACKING struct
	{
		FIELD EXT2CCU_INT_M2            : 1;
		FIELD rsv_1                     : 31;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EXT2CCU_INT_CCU
{
	PACKING struct
	{
		FIELD EXT2CCU_INT_CCU           : 1;
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
		FIELD DMA_REQ_ST                : 5;
		FIELD rsv_13                    : 3;
		FIELD DMA_RDY_ST                : 5;
		FIELD rsv_21                    : 11;
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
		FIELD CCU_EINTC_MASK            : 16;
		FIELD CCU_EINTC_MODE            : 1;
		FIELD rsv_17                    : 15;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EINTC_CLR
{
	PACKING struct
	{
		FIELD CCU_EINTC_CLR             : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EINTC_ST
{
	PACKING struct
	{
		FIELD CCU_EINTC_ST              : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EINTC_MISC
{
	PACKING struct
	{
		FIELD CCU_EINTC_RAW_ST          : 16;
		FIELD CCU_EINTC_TRIG_ST         : 1;
		FIELD rsv_17                    : 15;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_AHB_REMAP_A
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

PACKING union CCU_A_REG_AHB_REMAP_B
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

PACKING union CCU_A_REG_AHB_REMAP_C
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

PACKING union CCU_A_REG_AHB_REMAP_D
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

PACKING union CCU_A_REG_CAMSV_INT_EN
{
	PACKING struct
	{
		FIELD rsv_0                     : 1;
		FIELD CAMSV_0_SOF_INT_EN        : 1;
		FIELD CAMSV_0_LIN_INT_EN        : 1;
		FIELD CAMSV_0_FRM_INT_EN        : 1;
		FIELD rsv_4                     : 1;
		FIELD CAMSV_1_SOF_INT_EN        : 1;
		FIELD CAMSV_1_LIN_INT_EN        : 1;
		FIELD CAMSV_1_FRM_INT_EN        : 1;
		FIELD rsv_8                     : 1;
		FIELD CAMSV_2_SOF_INT_EN        : 1;
		FIELD CAMSV_2_LIN_INT_EN        : 1;
		FIELD CAMSV_2_FRM_INT_EN        : 1;
		FIELD rsv_12                    : 1;
		FIELD CAMSV_3_SOF_INT_EN        : 1;
		FIELD CAMSV_3_LIN_INT_EN        : 1;
		FIELD CAMSV_3_FRM_INT_EN        : 1;
		FIELD rsv_16                    : 1;
		FIELD CAMSV_4_SOF_INT_EN        : 1;
		FIELD CAMSV_4_LIN_INT_EN        : 1;
		FIELD CAMSV_4_FRM_INT_EN        : 1;
		FIELD rsv_20                    : 1;
		FIELD CAMSV_5_SOF_INT_EN        : 1;
		FIELD CAMSV_5_LIN_INT_EN        : 1;
		FIELD CAMSV_5_FRM_INT_EN        : 1;
		FIELD rsv_24                    : 1;
		FIELD CAMSV_6_SOF_INT_EN        : 1;
		FIELD CAMSV_6_LIN_INT_EN        : 1;
		FIELD CAMSV_6_FRM_INT_EN        : 1;
		FIELD rsv_28                    : 1;
		FIELD CAMSV_7_SOF_INT_EN        : 1;
		FIELD CAMSV_7_LIN_INT_EN        : 1;
		FIELD CAMSV_7_FRM_INT_EN        : 1;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_INT_CTL
{
	PACKING struct
	{
		FIELD CAMSV_LIN_DONE_SUB_EN     : 1;
		FIELD CAMSV_INT_TOG_IN          : 1;
		FIELD rsv_2                     : 30;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_LINT_SET0
{
	PACKING struct
	{
		FIELD CAMSV_0_LIN_INT_SET       : 16;
		FIELD CAMSV_1_LIN_INT_SET       : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_LINT_SET1
{
	PACKING struct
	{
		FIELD CAMSV_2_LIN_INT_SET       : 16;
		FIELD CAMSV_3_LIN_INT_SET       : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_LINT_SET2
{
	PACKING struct
	{
		FIELD CAMSV_4_LIN_INT_SET       : 16;
		FIELD CAMSV_5_LIN_INT_SET       : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_LINT_SET3
{
	PACKING struct
	{
		FIELD CAMSV_6_LIN_INT_SET       : 16;
		FIELD CAMSV_7_LIN_INT_SET       : 16;
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
		FIELD rsv_3                     : 29;
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
		FIELD rsv_3                     : 29;
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
		FIELD rsv_3                     : 29;
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
		FIELD rsv_3                     : 29;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_4_ICNT
{
	PACKING struct
	{
		FIELD CAMSV_4_INT_CNT           : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_4_INT_ST
{
	PACKING struct
	{
		FIELD CAMSV_4_LINE_INT_ST       : 1;
		FIELD CAMSV_4_FRAME_INT_ST      : 1;
		FIELD CAMSV_4_SOF_INT_ST        : 1;
		FIELD rsv_3                     : 29;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_5_ICNT
{
	PACKING struct
	{
		FIELD CAMSV_5_INT_CNT           : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_5_INT_ST
{
	PACKING struct
	{
		FIELD CAMSV_5_LINE_INT_ST       : 1;
		FIELD CAMSV_5_FRAME_INT_ST      : 1;
		FIELD CAMSV_5_SOF_INT_ST        : 1;
		FIELD rsv_3                     : 29;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_6_ICNT
{
	PACKING struct
	{
		FIELD CAMSV_6_INT_CNT           : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_6_INT_ST
{
	PACKING struct
	{
		FIELD CAMSV_6_LINE_INT_ST       : 1;
		FIELD CAMSV_6_FRAME_INT_ST      : 1;
		FIELD CAMSV_6_SOF_INT_ST        : 1;
		FIELD rsv_3                     : 29;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_7_ICNT
{
	PACKING struct
	{
		FIELD CAMSV_7_INT_CNT           : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CAMSV_7_INT_ST
{
	PACKING struct
	{
		FIELD CAMSV_7_LINE_INT_ST       : 1;
		FIELD CAMSV_7_FRAME_INT_ST      : 1;
		FIELD CAMSV_7_SOF_INT_ST        : 1;
		FIELD rsv_3                     : 29;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DVFSRC_REQ
{
	PACKING struct
	{
		FIELD DVFSRC_request            : 1;
		FIELD rsv_1                     : 31;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_DONE
{
	PACKING struct
	{
		FIELD CCU_done                  : 1;
		FIELD rsv_1                     : 31;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_INT_ST_REG1_EN
{
	PACKING struct
	{
		FIELD MAE_A_INT_EN              : 1;
		FIELD MAE_B_INT_EN              : 1;
		FIELD MAE_C_INT_EN              : 1;
		FIELD rsv_3                     : 13;
		FIELD AAO_A_INT_EN              : 1;
		FIELD AAO_B_INT_EN              : 1;
		FIELD AAO_C_INT_EN              : 1;
		FIELD rsv_19                    : 13;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_INT_ST_REG1_MSK
{
	PACKING struct
	{
		FIELD MAE_A_INT_MSK             : 1;
		FIELD MAE_B_INT_MSK             : 1;
		FIELD MAE_C_INT_MSK             : 1;
		FIELD rsv_3                     : 13;
		FIELD AAO_A_INT_MSK             : 1;
		FIELD AAO_B_INT_MSK             : 1;
		FIELD AAO_C_INT_MSK             : 1;
		FIELD rsv_19                    : 13;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_INT_ST_REG1
{
	PACKING struct
	{
		FIELD MAE_A_INT                 : 1;
		FIELD MAE_B_INT                 : 1;
		FIELD MAE_C_INT                 : 1;
		FIELD rsv_3                     : 13;
		FIELD AAO_A_INT                 : 1;
		FIELD AAO_B_INT                 : 1;
		FIELD AAO_C_INT                 : 1;
		FIELD rsv_19                    : 13;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_INT_ST_REG2_EN
{
	PACKING struct
	{
		FIELD rsv_0                     : 8;
		FIELD AFO_A_INT_EN              : 1;
		FIELD AFO_B_INT_EN              : 1;
		FIELD AFO_C_INT_EN              : 1;
		FIELD rsv_11                    : 21;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_INT_ST_REG2_MSK
{
	PACKING struct
	{
		FIELD rsv_0                     : 8;
		FIELD AFO_A_INT_MSK             : 1;
		FIELD AFO_B_INT_MSK             : 1;
		FIELD AFO_C_INT_MSK             : 1;
		FIELD rsv_11                    : 21;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_INT_ST_REG2
{
	PACKING struct
	{
		FIELD rsv_0                     : 8;
		FIELD AFO_A_INT                 : 1;
		FIELD AFO_B_INT                 : 1;
		FIELD AFO_C_INT                 : 1;
		FIELD rsv_11                    : 21;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_INT_ST_REG3_EN
{
	PACKING struct
	{
		FIELD EXT2CCU_INT_M0_EN         : 1;
		FIELD EXT2CCU_INT_M1_EN         : 1;
		FIELD EXT2CCU_INT_M2_EN         : 1;
		FIELD EXT2CCU_INT_CCU_EN        : 1;
		FIELD EXCEPT_INT_EN             : 1;
		FIELD RDMA_ADDR_EXCEPT_EN       : 1;
		FIELD WDMA_ADDR_EXCEPT_EN       : 1;
		FIELD rsv_7                     : 9;
		FIELD CAMSV_INT_0_EN            : 1;
		FIELD CAMSV_INT_1_EN            : 1;
		FIELD CAMSV_INT_2_EN            : 1;
		FIELD CAMSV_INT_3_EN            : 1;
		FIELD CAMSV_INT_4_EN            : 1;
		FIELD CAMSV_INT_5_EN            : 1;
		FIELD CAMSV_INT_6_EN            : 1;
		FIELD CAMSV_INT_7_EN            : 1;
		FIELD rsv_24                    : 1;
		FIELD H2T_A_INT_EN              : 1;
		FIELD T2H_A_INT_EN              : 1;
		FIELD WDT_TIMEOUT_INT_EN        : 1;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_INT_ST_REG3_MSK
{
	PACKING struct
	{
		FIELD EXT2CCU_INT_M0_MSK        : 1;
		FIELD EXT2CCU_INT_M1_MSK        : 1;
		FIELD EXT2CCU_INT_M2_MSK        : 1;
		FIELD EXT2CCU_INT_CCU_MSK       : 1;
		FIELD EXCEPT_INT_MSK            : 1;
		FIELD RDMA_ADDR_EXCEPT_MSK      : 1;
		FIELD WDMA_ADDR_EXCEPT_MSK      : 1;
		FIELD rsv_7                     : 9;
		FIELD CAMSV_INT_0_MSK           : 1;
		FIELD CAMSV_INT_1_MSK           : 1;
		FIELD CAMSV_INT_2_MSK           : 1;
		FIELD CAMSV_INT_3_MSK           : 1;
		FIELD CAMSV_INT_4_MSK           : 1;
		FIELD CAMSV_INT_5_MSK           : 1;
		FIELD CAMSV_INT_6_MSK           : 1;
		FIELD CAMSV_INT_7_MSK           : 1;
		FIELD rsv_24                    : 1;
		FIELD H2T_A_INT_MSK             : 1;
		FIELD T2H_A_INT_MSK             : 1;
		FIELD WDT_TIMEOUT_INT_MSK       : 1;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_INT_ST_REG3
{
	PACKING struct
	{
		FIELD EXT2CCU_INT_M0            : 1;
		FIELD EXT2CCU_INT_M1            : 1;
		FIELD EXT2CCU_INT_M2            : 1;
		FIELD EXT2CCU_INT_CCU           : 1;
		FIELD EXCEPT_INT                : 1;
		FIELD RDMA_ADDR_EXCEPT          : 1;
		FIELD WDMA_ADDR_EXCEPT          : 1;
		FIELD rsv_7                     : 9;
		FIELD CAMSV_INT_0               : 1;
		FIELD CAMSV_INT_1               : 1;
		FIELD CAMSV_INT_2               : 1;
		FIELD CAMSV_INT_3               : 1;
		FIELD CAMSV_INT_4               : 1;
		FIELD CAMSV_INT_5               : 1;
		FIELD CAMSV_INT_6               : 1;
		FIELD CAMSV_INT_7               : 1;
		FIELD rsv_24                    : 1;
		FIELD H2T_A_INT                 : 1;
		FIELD T2H_A_INT                 : 1;
		FIELD WDT_TIMEOUT_INT           : 1;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU2EXT_INT
{
	PACKING struct
	{
		FIELD CCU2EXT_INT_M0            : 1;
		FIELD CCU2EXT_INT_M1            : 1;
		FIELD CCU2EXT_INT_M2            : 1;
		FIELD rsv_3                     : 29;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_FBR_FIFO_THRE
{
	PACKING struct
	{
		FIELD CCU_FBR_RG_FIFO_THRE      : 2;
		FIELD CACHE_FBR_RG_FIFO_THRE    : 2;
		FIELD CCM_FBR_RG_FIFO_THRE      : 2;
		FIELD rsv_6                     : 26;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_ADDR_START_BOUND
{
	PACKING struct
	{
		FIELD DMA_ADDR_START_BOUND      : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_ADDR_END_BOUND
{
	PACKING struct
	{
		FIELD DMA_ADDR_END_BOUND        : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_RMDA_FAIL_ADDR
{
	PACKING struct
	{
		FIELD RDMA_FAIL_ADDR            : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_RDMA_FAIL_LEN
{
	PACKING struct
	{
		FIELD RDMA_FAIL_LEN             : 3;
		FIELD rsv_3                     : 29;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_WDMA_FAIL_ADDR
{
	PACKING struct
	{
		FIELD WDMA_FAIL_ADDR            : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_WDMA_FAIL_LEN
{
	PACKING struct
	{
		FIELD WDMA_FAIL_LEN             : 3;
		FIELD rsv_3                     : 29;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_WDT_CTL
{
	PACKING struct
	{
		FIELD WDT_LENGTH                : 24;
		FIELD WDT_RESET                 : 1;
		FIELD WDT_ENABLE                : 1;
		FIELD rsv_26                    : 6;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_WDT_CNT_ST
{
	PACKING struct
	{
		FIELD WDT_COUNTER               : 24;
		FIELD rsv_24                    : 8;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_SECURITY_CTL
{
	PACKING struct
	{
		FIELD HSECUR_EN                 : 1;
		FIELD HSECUR_B                  : 1;
		FIELD HDOMAIN_APC               : 5;
		FIELD ARPROT1                   : 1;
		FIELD AWPROT1                   : 1;
		FIELD rsv_9                     : 23;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EXT_IRQ_EN
{
	PACKING struct
	{
		FIELD RAW_A_IRQ_EN              : 1;
		FIELD RAW_B_IRQ_EN              : 1;
		FIELD RAW_C_IRQ_EN              : 1;
		FIELD CAMSV_IRQ_0_EN            : 1;
		FIELD CAMSV_IRQ_1_EN            : 1;
		FIELD CAMSV_IRQ_2_EN            : 1;
		FIELD CAMSV_IRQ_3_EN            : 1;
		FIELD CAMSV_IRQ_4_EN            : 1;
		FIELD CAMSV_IRQ_5_EN            : 1;
		FIELD CAMSV_IRQ_6_EN            : 1;
		FIELD CAMSV_IRQ_7_EN            : 1;
		FIELD ASG_IRQ_EN                : 1;
		FIELD I2C_IRQ_EN                : 1;
		FIELD DMA_IRQ_EN                : 1;
		FIELD VPU_IRQ_0_EN              : 1;
		FIELD VPU_IRQ_1_EN              : 1;
		FIELD VPU_IRQ_2_EN              : 1;
		FIELD rsv_17                    : 15;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCU_STACK_PTR
{
	PACKING struct
	{
		FIELD CCU_STACK_PTR             : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_LBIST_ST
{
	PACKING struct
	{
		FIELD LBIST_ST                  : 1;
		FIELD rsv_1                     : 31;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DCLS_DBG_B0
{
	PACKING struct
	{
		FIELD DCLS_DBG_B0               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DCLS_DBG_B1
{
	PACKING struct
	{
		FIELD DCLS_DBG_B1               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DCLS_DBG_B2
{
	PACKING struct
	{
		FIELD DCLS_DBG_B2               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DCLS_DBG_B3
{
	PACKING struct
	{
		FIELD DCLS_DBG_B3               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DCLS_DBG_B4
{
	PACKING struct
	{
		FIELD DCLS_DBG_B4               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_FBR_MON0
{
	PACKING struct
	{
		FIELD ERR_RW                    : 1;
		FIELD ERR_SIZE                  : 1;
		FIELD ERR_BURST                 : 3;
		FIELD ERR_FLAG_DROP_BOUNDARY    : 2;
		FIELD ERR_FLAG_1K_BOUNDARY      : 1;
		FIELD rsv_8                     : 24;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_FBR_MON1
{
	PACKING struct
	{
		FIELD ERR_ADDRESS               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CACHE_FBR_MON0
{
	PACKING struct
	{
		FIELD ERR_RW                    : 1;
		FIELD ERR_SIZE                  : 1;
		FIELD ERR_BURST                 : 3;
		FIELD ERR_FLAG_DROP_BOUNDARY    : 2;
		FIELD ERR_FLAG_1K_BOUNDARY      : 1;
		FIELD rsv_8                     : 24;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CACHE_FBR_MON1
{
	PACKING struct
	{
		FIELD ERR_ADDRESS               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCM_FBR_MON0
{
	PACKING struct
	{
		FIELD ERR_RW                    : 1;
		FIELD ERR_SIZE                  : 1;
		FIELD ERR_BURST                 : 3;
		FIELD ERR_FLAG_DROP_BOUNDARY    : 2;
		FIELD ERR_FLAG_1K_BOUNDARY      : 1;
		FIELD rsv_8                     : 24;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCM_FBR_MON1
{
	PACKING struct
	{
		FIELD ERR_ADDRESS               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EMCORE_ERR_INT_ST
{
	PACKING struct
	{
		FIELD ECC_ICCM_SB_ERR_ST        : 1;
		FIELD ECC_DCCM_SB_ERR_ST        : 1;
		FIELD ECC_DC_SB_ERR_ST          : 1;
		FIELD ECC_IC_SB_ERR_ST          : 1;
		FIELD DCLS_ERR_ST               : 1;
		FIELD ECC_ICCM_DB_ERR_ST        : 1;
		FIELD ECC_ICCM_ADR_ERR_ST       : 1;
		FIELD ECC_DCCM_DB_ERR_ST        : 1;
		FIELD ECC_DCCM_ADR_ERR_ST       : 1;
		FIELD ECC_EXCEOPTION_ST         : 1;
		FIELD ECC_DC_DC_DB_ERR_ST       : 1;
		FIELD ECC_DC_ADR_ERR_ST         : 1;
		FIELD ECC_IC_DB_ERR_ST          : 1;
		FIELD ECC_IC_ADR_ERR_ST         : 1;
		FIELD rsv_14                    : 18;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EMCORE_ERR_INT_EN
{
	PACKING struct
	{
		FIELD ECC_ICCM_SB_ERR_EN        : 1;
		FIELD ECC_DCCM_SB_ERR_EN        : 1;
		FIELD ECC_DC_SB_ERR_EN          : 1;
		FIELD ECC_IC_SB_ERR_EN          : 1;
		FIELD DCLS_ERR_EN               : 1;
		FIELD ECC_ICCM_DB_ERR_EN        : 1;
		FIELD ECC_ICCM_ADR_ERR_EN       : 1;
		FIELD ECC_DCCM_DB_ERR_EN        : 1;
		FIELD ECC_DCCM_ADR_ERR_EN       : 1;
		FIELD ECC_EXCEOPTION_EN         : 1;
		FIELD ECC_DC_DC_DB_ERR_EN       : 1;
		FIELD ECC_DC_ADR_ERR_EN         : 1;
		FIELD ECC_IC_DB_ERR_EN          : 1;
		FIELD ECC_IC_ADR_ERR_EN         : 1;
		FIELD rsv_14                    : 18;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_EMCORE_ERR_INT_MSK
{
	PACKING struct
	{
		FIELD ECC_ICCM_SB_ERR_MSK       : 1;
		FIELD ECC_DCCM_SB_ERR_MSK       : 1;
		FIELD ECC_DC_SB_ERR_MSK         : 1;
		FIELD ECC_IC_SB_ERR_MSK         : 1;
		FIELD DCLS_ERR_MSK              : 1;
		FIELD ECC_ICCM_DB_ERR_MSK       : 1;
		FIELD ECC_ICCM_ADR_ERR_MSK      : 1;
		FIELD ECC_DCCM_DB_ERR_MSK       : 1;
		FIELD ECC_DCCM_ADR_ERR_MSK      : 1;
		FIELD ECC_EXCEOPTION_MSK        : 1;
		FIELD ECC_DC_DC_DB_ERR_MSK      : 1;
		FIELD ECC_DC_ADR_ERR_MSK        : 1;
		FIELD ECC_IC_DB_ERR_MSK         : 1;
		FIELD ECC_IC_ADR_ERR_MSK        : 1;
		FIELD rsv_14                    : 18;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_XSIZE
{
	PACKING struct
	{
		FIELD H2T_A_X_SIZE              : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_YSIZE
{
	PACKING struct
	{
		FIELD H2T_A_Y_SIZE              : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_STRIDE
{
	PACKING struct
	{
		FIELD H2T_A_STRIDE              : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BANK_LINE_NUMBER
{
	PACKING struct
	{
		FIELD H2T_A_BANK_LINE_NUMBER    : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_CTL
{
	PACKING struct
	{
		FIELD H2T_A_BANK_NUMBER_MAX     : 3;
		FIELD H2T_A_UNPACK_BYTE         : 2;
		FIELD H2T_A_ERR_MODE            : 1;
		FIELD rsv_6                     : 26;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BASE_ADDR_0
{
	PACKING struct
	{
		FIELD H2T_A_BASE_ADDR_0         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BASE_ADDR_1
{
	PACKING struct
	{
		FIELD H2T_A_BASE_ADDR_1         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BASE_ADDR_2
{
	PACKING struct
	{
		FIELD H2T_A_BASE_ADDR_2         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BASE_ADDR_3
{
	PACKING struct
	{
		FIELD H2T_A_BASE_ADDR_3         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BASE_ADDR_4
{
	PACKING struct
	{
		FIELD H2T_A_BASE_ADDR_4         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BASE_ADDR_5
{
	PACKING struct
	{
		FIELD H2T_A_BASE_ADDR_5         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BASE_ADDR_6
{
	PACKING struct
	{
		FIELD H2T_A_BASE_ADDR_6         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BASE_ADDR_7
{
	PACKING struct
	{
		FIELD H2T_A_BASE_ADDR_7         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_BANK_STAT
{
	PACKING struct
	{
		FIELD H2T_A_FULL                : 1;
		FIELD H2T_A_EMPTY               : 1;
		FIELD H2T_A_BANK_AVA_CNT        : 3;
		FIELD H2T_A_BANK_RCNT           : 3;
		FIELD H2T_A_BANK_WCNT           : 3;
		FIELD rsv_11                    : 21;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_LINE_STAT
{
	PACKING struct
	{
		FIELD H2T_A_LINE_RCNT           : 16;
		FIELD H2T_A_LINE_WCNT           : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_LINE_AVA_CNT
{
	PACKING struct
	{
		FIELD H2T_A_LINE_AVA_CNT        : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_H2T_A_SPARE
{
	PACKING struct
	{
		FIELD H2T_A_SPARE               : 8;
		FIELD rsv_8                     : 24;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_XSIZE
{
	PACKING struct
	{
		FIELD T2H_A_X_SIZE              : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_YSIZE
{
	PACKING struct
	{
		FIELD T2H_A_Y_SIZE              : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_STRIDE
{
	PACKING struct
	{
		FIELD T2H_A_STRIDE              : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BANK_LINE_NUMBER
{
	PACKING struct
	{
		FIELD T2H_A_BANK_LINE_NUMBER    : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_CTL
{
	PACKING struct
	{
		FIELD T2H_A_BANK_NUMBER_MAX     : 3;
		FIELD T2H_A_PACK_BYTE           : 2;
		FIELD T2H_A_ERR_MODE            : 1;
		FIELD rsv_6                     : 26;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BASE_ADDR_0
{
	PACKING struct
	{
		FIELD T2H_A_BASE_ADDR_0         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BASE_ADDR_1
{
	PACKING struct
	{
		FIELD T2H_A_BASE_ADDR_1         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BASE_ADDR_2
{
	PACKING struct
	{
		FIELD T2H_A_BASE_ADDR_2         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BASE_ADDR_3
{
	PACKING struct
	{
		FIELD T2H_A_BASE_ADDR_3         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BASE_ADDR_4
{
	PACKING struct
	{
		FIELD T2H_A_BASE_ADDR_4         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BASE_ADDR_5
{
	PACKING struct
	{
		FIELD T2H_A_BASE_ADDR_5         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BASE_ADDR_6
{
	PACKING struct
	{
		FIELD T2H_A_BASE_ADDR_6         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BASE_ADDR_7
{
	PACKING struct
	{
		FIELD T2H_A_BASE_ADDR_7         : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_BANK_STAT
{
	PACKING struct
	{
		FIELD T2H_A_FULL                : 1;
		FIELD T2H_A_EMPTY               : 1;
		FIELD T2H_A_BANK_AVA_CNT        : 3;
		FIELD T2H_A_BANK_RCNT           : 3;
		FIELD T2H_A_BANK_WCNT           : 3;
		FIELD rsv_11                    : 21;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_LINE_STAT
{
	PACKING struct
	{
		FIELD T2H_A_LINE_RCNT           : 16;
		FIELD T2H_A_LINE_WCNT           : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_LINE_AVA_CNT
{
	PACKING struct
	{
		FIELD T2H_A_LINE_AVA_CNT        : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_T2H_A_SPARE
{
	PACKING struct
	{
		FIELD T2H_A_SPARE               : 8;
		FIELD rsv_8                     : 24;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_SOFT_RSTSTAT
{
	PACKING struct
	{
		FIELD CCUO_SOFT_RST_STAT        : 1;
		FIELD TSFO_SOFT_RST_STAT        : 1;
		FIELD rsv_2                     : 14;
		FIELD CCUI_SOFT_RST_STAT        : 1;
		FIELD TSFI_SOFT_RST_STAT        : 1;
		FIELD VEC3I_SOFT_RST_STAT       : 1;
		FIELD rsv_19                    : 13;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_VERTICAL_FLIP_EN
{
	PACKING struct
	{
		FIELD CCUO_V_FLIP_EN            : 1;
		FIELD TSFO_V_FLIP_EN            : 1;
		FIELD rsv_2                     : 14;
		FIELD CCUI_V_FLIP_EN            : 1;
		FIELD TSFI_V_FLIP_EN            : 1;
		FIELD VEC3I_V_FLIP_EN           : 1;
		FIELD rsv_19                    : 13;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_SOFT_RESET
{
	PACKING struct
	{
		FIELD CCUO_SOFT_RST             : 1;
		FIELD TSFO_SOFT_RST             : 1;
		FIELD rsv_2                     : 14;
		FIELD CCUI_SOFT_RST             : 1;
		FIELD TSFI_SOFT_RST             : 1;
		FIELD VEC3I_SOFT_RST            : 1;
		FIELD rsv_19                    : 12;
		FIELD SEPARATE_SOFT_RST_EN      : 1;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_LAST_ULTRA_EN
{
	PACKING struct
	{
		FIELD CCUO_LAST_ULTRA_EN        : 1;
		FIELD TSFO_LAST_ULTRA_EN        : 1;
		FIELD rsv_2                     : 14;
		FIELD CCUI_LAST_ULTRA_EN        : 1;
		FIELD TSFI_LAST_ULTRA_EN        : 1;
		FIELD rsv_18                    : 14;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_SPECIAL_FUN_EN
{
	PACKING struct
	{
		FIELD SLOW_CNT                  : 16;
		FIELD rsv_16                    : 4;
		FIELD CONTINUOUS_COM_CON        : 2;
		FIELD rsv_22                    : 2;
		FIELD MULTI_PLANE_ID_EN         : 1;
		FIELD CONTINUOUS_COM_EN         : 1;
		FIELD FIFO_CHANGE_EN            : 1;
		FIELD GCLAST_EN                 : 1;
		FIELD rsv_28                    : 3;
		FIELD SLOW_EN                   : 1;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_BASE_ADDR
{
	PACKING struct
	{
		FIELD BASE_ADDR                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_OFST_ADDR
{
	PACKING struct
	{
		FIELD OFFSET_ADDR               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_DRS
{
	PACKING struct
	{
		FIELD FIFO_DRS_THRL             : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_DRS_THRH             : 12;
		FIELD rsv_28                    : 3;
		FIELD FIFO_DRS_EN               : 1;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_XSIZE
{
	PACKING struct
	{
		FIELD XSIZE                     : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_YSIZE
{
	PACKING struct
	{
		FIELD YSIZE                     : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_STRIDE
{
	PACKING struct
	{
		FIELD STRIDE                    : 16;
		FIELD BUS_SIZE                  : 4;
		FIELD FORMAT                    : 2;
		FIELD rsv_22                    : 1;
		FIELD FORMAT_EN                 : 1;
		FIELD BUS_SIZE_EN               : 1;
		FIELD rsv_25                    : 7;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_CON
{
	PACKING struct
	{
		FIELD FIFO_SIZE                 : 12;
		FIELD rsv_12                    : 16;
		FIELD MAX_BURST_LEN             : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_CON2
{
	PACKING struct
	{
		FIELD FIFO_PRI_THRL             : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_PRI_THRH             : 12;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_CON3
{
	PACKING struct
	{
		FIELD FIFO_PRE_PRI_THRL         : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_PRE_PRI_THRH         : 12;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_CROP
{
	PACKING struct
	{
		FIELD XOFFSET                   : 16;
		FIELD YOFFSET                   : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_BASE_ADDR
{
	PACKING struct
	{
		FIELD BASE_ADDR                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_OFST_ADDR
{
	PACKING struct
	{
		FIELD OFFSET_ADDR               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_DRS
{
	PACKING struct
	{
		FIELD FIFO_DRS_THRL             : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_DRS_THRH             : 12;
		FIELD rsv_28                    : 3;
		FIELD FIFO_DRS_EN               : 1;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_XSIZE
{
	PACKING struct
	{
		FIELD XSIZE                     : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_YSIZE
{
	PACKING struct
	{
		FIELD YSIZE                     : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_STRIDE
{
	PACKING struct
	{
		FIELD STRIDE                    : 16;
		FIELD BUS_SIZE                  : 4;
		FIELD FORMAT                    : 2;
		FIELD rsv_22                    : 1;
		FIELD FORMAT_EN                 : 1;
		FIELD BUS_SIZE_EN               : 1;
		FIELD rsv_25                    : 7;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_CON
{
	PACKING struct
	{
		FIELD FIFO_SIZE                 : 12;
		FIELD rsv_12                    : 16;
		FIELD MAX_BURST_LEN             : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_CON2
{
	PACKING struct
	{
		FIELD FIFO_PRI_THRL             : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_PRI_THRH             : 12;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_CON3
{
	PACKING struct
	{
		FIELD FIFO_PRE_PRI_THRL         : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_PRE_PRI_THRH         : 12;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_CROP
{
	PACKING struct
	{
		FIELD XOFFSET                   : 16;
		FIELD YOFFSET                   : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_BASE_ADDR
{
	PACKING struct
	{
		FIELD BASE_ADDR                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_OFST_ADDR
{
	PACKING struct
	{
		FIELD OFFSET_ADDR               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_DRS
{
	PACKING struct
	{
		FIELD FIFO_DRS_THRL             : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_DRS_THRH             : 12;
		FIELD rsv_28                    : 3;
		FIELD FIFO_DRS_EN               : 1;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_XSIZE
{
	PACKING struct
	{
		FIELD XSIZE                     : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_YSIZE
{
	PACKING struct
	{
		FIELD YSIZE                     : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_STRIDE
{
	PACKING struct
	{
		FIELD STRIDE                    : 16;
		FIELD BUS_SIZE                  : 4;
		FIELD FORMAT                    : 2;
		FIELD rsv_22                    : 1;
		FIELD FORMAT_EN                 : 1;
		FIELD BUS_SIZE_EN               : 1;
		FIELD rsv_25                    : 5;
		FIELD SWAP                      : 2;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_CON
{
	PACKING struct
	{
		FIELD FIFO_SIZE                 : 12;
		FIELD rsv_12                    : 16;
		FIELD MAX_BURST_LEN             : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_CON2
{
	PACKING struct
	{
		FIELD FIFO_PRI_THRL             : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_PRI_THRH             : 12;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_CON3
{
	PACKING struct
	{
		FIELD FIFO_PRE_PRI_THRL         : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_PRE_PRI_THRH         : 12;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_BASE_ADDR
{
	PACKING struct
	{
		FIELD BASE_ADDR                 : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_OFST_ADDR
{
	PACKING struct
	{
		FIELD OFFSET_ADDR               : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_DRS
{
	PACKING struct
	{
		FIELD FIFO_DRS_THRL             : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_DRS_THRH             : 12;
		FIELD rsv_28                    : 3;
		FIELD FIFO_DRS_EN               : 1;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_XSIZE
{
	PACKING struct
	{
		FIELD XSIZE                     : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_YSIZE
{
	PACKING struct
	{
		FIELD YSIZE                     : 16;
		FIELD rsv_16                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_STRIDE
{
	PACKING struct
	{
		FIELD STRIDE                    : 16;
		FIELD BUS_SIZE                  : 4;
		FIELD FORMAT                    : 2;
		FIELD rsv_22                    : 1;
		FIELD FORMAT_EN                 : 1;
		FIELD BUS_SIZE_EN               : 1;
		FIELD rsv_25                    : 5;
		FIELD SWAP                      : 2;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_CON
{
	PACKING struct
	{
		FIELD FIFO_SIZE                 : 12;
		FIELD rsv_12                    : 16;
		FIELD MAX_BURST_LEN             : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_CON2
{
	PACKING struct
	{
		FIELD FIFO_PRI_THRL             : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_PRI_THRH             : 12;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_CON3
{
	PACKING struct
	{
		FIELD FIFO_PRE_PRI_THRL         : 12;
		FIELD rsv_12                    : 4;
		FIELD FIFO_PRE_PRI_THRH         : 12;
		FIELD rsv_28                    : 4;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_ERR_CTRL
{
	PACKING struct
	{
		FIELD CCUO_ERR                  : 1;
		FIELD TSFO_ERR                  : 1;
		FIELD rsv_2                     : 14;
		FIELD CCUI_ERR                  : 1;
		FIELD TSFI_ERR                  : 1;
		FIELD rsv_18                    : 13;
		FIELD ERR_CLR_MD                : 1;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUO_ERR_STAT
{
	PACKING struct
	{
		FIELD ERR_STAT                  : 16;
		FIELD ERR_EN                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFO_ERR_STAT
{
	PACKING struct
	{
		FIELD ERR_STAT                  : 16;
		FIELD ERR_EN                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_CCUI_ERR_STAT
{
	PACKING struct
	{
		FIELD ERR_STAT                  : 16;
		FIELD ERR_EN                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_TSFI_ERR_STAT
{
	PACKING struct
	{
		FIELD ERR_STAT                  : 16;
		FIELD ERR_EN                    : 16;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_DEBUG_ADDR
{
	PACKING struct
	{
		FIELD DEBUG_ADDR                : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_RSV1
{
	PACKING struct
	{
		FIELD RSV                       : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_RSV2
{
	PACKING struct
	{
		FIELD RSV                       : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_RSV3
{
	PACKING struct
	{
		FIELD RSV                       : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_RSV4
{
	PACKING struct
	{
		FIELD RSV                       : 32;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_DEBUG_SEL
{
	PACKING struct
	{
		FIELD DMA_TOP_SEL               : 8;
		FIELD R_W_DMA_TOP_SEL           : 8;
		FIELD SUB_MODULE_SEL            : 8;
		FIELD rsv_24                    : 6;
		FIELD ARBITER_BVALID_FULL       : 1;
		FIELD ARBITER_COM_FULL          : 1;
	} Bits;
	UINT32 Raw;
};

PACKING union CCU_A_REG_DMA_BW_SELF_TEST
{
	PACKING struct
	{
		FIELD BW_SELF_TEST_EN_CCUO      : 1;
		FIELD BW_SELF_TEST_EN_TSFO      : 1;
		FIELD rsv_2                     : 30;
	} Bits;
	UINT32 Raw;
};

// ----------------- CCU_A  Grouping Definitions -------------------
// ----------------- CCU_A Register Definition -------------------
PACKING struct CCU_A_REGS
{
	union CCU_A_REG_RESET                       RESET;            // 1000
	union CCU_A_REG_START_TRIG                  START_TRIG;       // 1004
	union CCU_A_REG_BANK_INCR                   BANK_INCR;        // 1008
	union CCU_A_REG_CTRL                        CTRL;             // 100C
	union CCU_A_REG_T2H_A_DONE_ST               T2H_A_DONE_ST;    // 1010
	union CCU_A_REG_T2H_B_DONE_ST               T2H_B_DONE_ST;    // 1014
	union CCU_A_REG_H2T_A_DONE_ST               H2T_A_DONE_ST;    // 1018
	union CCU_A_REG_WDMA_DONE_ST                WDMA_DONE_ST;     // 101C
	union CCU_A_REG_AXI_REMAP                   AXI_REMAP;        // 1020
	union CCU_A_REG_CCU_CTL                     CCU_CTL;          // 1024
	union CCU_A_REG_CCU_ST                      CCU_ST;           // 1028
	union CCU_A_REG_H2X_CFG                     H2X_CFG;          // 102C
	union CCU_A_REG_EXT2CCU_INT_M0              EXT2CCU_INT_M0;   // 1030
	union CCU_A_REG_EXT2CCU_INT_M1              EXT2CCU_INT_M1;   // 1034
	union CCU_A_REG_EXT2CCU_INT_M2              EXT2CCU_INT_M2;   // 1038
	union CCU_A_REG_EXT2CCU_INT_CCU             EXT2CCU_INT_CCU;  // 103C
	union CCU_A_REG_DCM_ST                      DCM_ST;           // 1040
	union CCU_A_REG_DMA_ERR_ST                  DMA_ERR_ST;       // 1044
	union CCU_A_REG_DMA_DEBUG                   DMA_DEBUG;        // 1048
	UINT32                          rsv_104C;         // 104C
	union CCU_A_REG_EINTC_MASK                  EINTC_MASK;       // 1050
	union CCU_A_REG_EINTC_CLR                   EINTC_CLR;        // 1054
	union CCU_A_REG_EINTC_ST                    EINTC_ST;         // 1058
	union CCU_A_REG_EINTC_MISC                  EINTC_MISC;       // 105C
	union CCU_A_REG_AHB_REMAP_A                 AHB_REMAP_A;      // 1060
	union CCU_A_REG_AHB_REMAP_B                 AHB_REMAP_B;      // 1064
	union CCU_A_REG_AHB_REMAP_C                 AHB_REMAP_C;      // 1068
	union CCU_A_REG_AHB_REMAP_D                 AHB_REMAP_D;      // 106C
	union CCU_A_REG_CCU_PC                      CCU_PC;           // 1070
	union CCU_A_REG_TOP_SPARE                   TOP_SPARE;        // 1074
	UINT32                          rsv_1078[2];      // 1078..107C
	union CCU_A_REG_CCU_INFO00                  CCU_INFO00;       // 1080
	union CCU_A_REG_CCU_INFO01                  CCU_INFO01;       // 1084
	union CCU_A_REG_CCU_INFO02                  CCU_INFO02;       // 1088
	union CCU_A_REG_CCU_INFO03                  CCU_INFO03;       // 108C
	union CCU_A_REG_CCU_INFO04                  CCU_INFO04;       // 1090
	union CCU_A_REG_CCU_INFO05                  CCU_INFO05;       // 1094
	union CCU_A_REG_CCU_INFO06                  CCU_INFO06;       // 1098
	union CCU_A_REG_CCU_INFO07                  CCU_INFO07;       // 109C
	union CCU_A_REG_CCU_INFO08                  CCU_INFO08;       // 10A0
	union CCU_A_REG_CCU_INFO09                  CCU_INFO09;       // 10A4
	union CCU_A_REG_CCU_INFO10                  CCU_INFO10;       // 10A8
	union CCU_A_REG_CCU_INFO11                  CCU_INFO11;       // 10AC
	union CCU_A_REG_CCU_INFO12                  CCU_INFO12;       // 10B0
	union CCU_A_REG_CCU_INFO13                  CCU_INFO13;       // 10B4
	union CCU_A_REG_CCU_INFO14                  CCU_INFO14;       // 10B8
	union CCU_A_REG_CCU_INFO15                  CCU_INFO15;       // 10BC
	union CCU_A_REG_CCU_INFO16                  CCU_INFO16;       // 10C0
	union CCU_A_REG_CCU_INFO17                  CCU_INFO17;       // 10C4
	union CCU_A_REG_CCU_INFO18                  CCU_INFO18;       // 10C8
	union CCU_A_REG_CCU_INFO19                  CCU_INFO19;       // 10CC
	union CCU_A_REG_CCU_INFO20                  CCU_INFO20;       // 10D0
	union CCU_A_REG_CCU_INFO21                  CCU_INFO21;       // 10D4
	union CCU_A_REG_CCU_INFO22                  CCU_INFO22;       // 10D8
	union CCU_A_REG_CCU_INFO23                  CCU_INFO23;       // 10DC
	union CCU_A_REG_CCU_INFO24                  CCU_INFO24;       // 10E0
	union CCU_A_REG_CCU_INFO25                  CCU_INFO25;       // 10E4
	union CCU_A_REG_CCU_INFO26                  CCU_INFO26;       // 10E8
	union CCU_A_REG_CCU_INFO27                  CCU_INFO27;       // 10EC
	union CCU_A_REG_CCU_INFO28                  CCU_INFO28;       // 10F0
	union CCU_A_REG_CCU_INFO29                  CCU_INFO29;       // 10F4
	union CCU_A_REG_CCU_INFO30                  CCU_INFO30;       // 10F8
	union CCU_A_REG_CCU_INFO31                  CCU_INFO31;       // 10FC
	UINT32                          rsv_1100[20];     // 1100..114C
	union CCU_A_REG_CAMSV_INT_EN                CAMSV_INT_EN;     // 1150
	union CCU_A_REG_CAMSV_INT_CTL               CAMSV_INT_CTL;    // 1154
	union CCU_A_REG_CAMSV_LINT_SET0             CAMSV_LINT_SET0;  // 1158
	union CCU_A_REG_CAMSV_LINT_SET1             CAMSV_LINT_SET1;  // 115C
	union CCU_A_REG_CAMSV_LINT_SET2             CAMSV_LINT_SET2;  // 1160
	union CCU_A_REG_CAMSV_LINT_SET3             CAMSV_LINT_SET3;  // 1164
	union CCU_A_REG_CAMSV_0_ICNT                CAMSV_0_ICNT;     // 1168
	union CCU_A_REG_CAMSV_0_INT_ST              CAMSV_0_INT_ST;   // 116C
	union CCU_A_REG_CAMSV_1_ICNT                CAMSV_1_ICNT;     // 1170
	union CCU_A_REG_CAMSV_1_INT_ST              CAMSV_1_INT_ST;   // 1174
	union CCU_A_REG_CAMSV_2_ICNT                CAMSV_2_ICNT;     // 1178
	union CCU_A_REG_CAMSV_2_INT_ST              CAMSV_2_INT_ST;   // 117C
	union CCU_A_REG_CAMSV_3_ICNT                CAMSV_3_ICNT;     // 1180
	union CCU_A_REG_CAMSV_3_INT_ST              CAMSV_3_INT_ST;   // 1184
	union CCU_A_REG_CAMSV_4_ICNT                CAMSV_4_ICNT;     // 1188
	union CCU_A_REG_CAMSV_4_INT_ST              CAMSV_4_INT_ST;   // 118C
	union CCU_A_REG_CAMSV_5_ICNT                CAMSV_5_ICNT;     // 1190
	union CCU_A_REG_CAMSV_5_INT_ST              CAMSV_5_INT_ST;   // 1194
	union CCU_A_REG_CAMSV_6_ICNT                CAMSV_6_ICNT;     // 1198
	union CCU_A_REG_CAMSV_6_INT_ST              CAMSV_6_INT_ST;   // 119C
	union CCU_A_REG_CAMSV_7_ICNT                CAMSV_7_ICNT;     // 11A0
	union CCU_A_REG_CAMSV_7_INT_ST              CAMSV_7_INT_ST;   // 11A4
	union CCU_A_REG_DVFSRC_REQ                  DVFSRC_REQ;       // 11A8
	union CCU_A_REG_CCU_DONE                    CCU_DONE;         // 11AC
	UINT32                          rsv_11B0[3];      // 11B0..11B8
	union CCU_A_REG_INT_ST_REG1_EN              INT_ST_REG1_EN;   // 11BC
	union CCU_A_REG_INT_ST_REG1_MSK             INT_ST_REG1_MSK;  // 11C0
	union CCU_A_REG_INT_ST_REG1                 INT_ST_REG1;      // 11C4
	union CCU_A_REG_INT_ST_REG2_EN              INT_ST_REG2_EN;   // 11C8
	union CCU_A_REG_INT_ST_REG2_MSK             INT_ST_REG2_MSK;  // 11CC
	union CCU_A_REG_INT_ST_REG2                 INT_ST_REG2;      // 11D0
	union CCU_A_REG_INT_ST_REG3_EN              INT_ST_REG3_EN;   // 11D4
	union CCU_A_REG_INT_ST_REG3_MSK             INT_ST_REG3_MSK;  // 11D8
	union CCU_A_REG_INT_ST_REG3                 INT_ST_REG3;      // 11DC
	union CCU_A_REG_CCU2EXT_INT                 CCU2EXT_INT;      // 11E0
	union CCU_A_REG_FBR_FIFO_THRE               FBR_FIFO_THRE;    // 11E4
	union CCU_A_REG_DMA_ADDR_START_BOUND    DMA_ADDR_START_BOUND; // 11E8
	union CCU_A_REG_DMA_ADDR_END_BOUND          DMA_ADDR_END_BOUND; // 11EC
	union CCU_A_REG_RMDA_FAIL_ADDR              RMDA_FAIL_ADDR;   // 11F0
	union CCU_A_REG_RDMA_FAIL_LEN               RDMA_FAIL_LEN;    // 11F4
	union CCU_A_REG_WDMA_FAIL_ADDR              WDMA_FAIL_ADDR;   // 11F8
	union CCU_A_REG_WDMA_FAIL_LEN               WDMA_FAIL_LEN;    // 11FC
	union CCU_A_REG_WDT_CTL                     WDT_CTL;          // 1200
	union CCU_A_REG_WDT_CNT_ST                  WDT_CNT_ST;       // 1204
	union CCU_A_REG_SECURITY_CTL                SECURITY_CTL;     // 1208
	union CCU_A_REG_EXT_IRQ_EN                  EXT_IRQ_EN;       // 120C
	union CCU_A_REG_CCU_STACK_PTR               CCU_STACK_PTR;    // 1210
	union CCU_A_REG_LBIST_ST                    LBIST_ST;         // 1214
	union CCU_A_REG_DCLS_DBG_B0                 DCLS_DBG_B0;      // 1218
	union CCU_A_REG_DCLS_DBG_B1                 DCLS_DBG_B1;      // 121C
	union CCU_A_REG_DCLS_DBG_B2                 DCLS_DBG_B2;      // 1220
	union CCU_A_REG_DCLS_DBG_B3                 DCLS_DBG_B3;      // 1224
	union CCU_A_REG_DCLS_DBG_B4                 DCLS_DBG_B4;      // 1228
	union CCU_A_REG_FBR_MON0                    FBR_MON0;         // 122C
	union CCU_A_REG_FBR_MON1                    FBR_MON1;         // 1230
	union CCU_A_REG_CACHE_FBR_MON0              CACHE_FBR_MON0;   // 1234
	union CCU_A_REG_CACHE_FBR_MON1              CACHE_FBR_MON1;   // 1238
	union CCU_A_REG_CCM_FBR_MON0                CCM_FBR_MON0;     // 123C
	union CCU_A_REG_CCM_FBR_MON1                CCM_FBR_MON1;     // 1240
	union CCU_A_REG_EMCORE_ERR_INT_ST           EMCORE_ERR_INT_ST; // 1244
	union CCU_A_REG_EMCORE_ERR_INT_EN           EMCORE_ERR_INT_EN; // 1248
	union CCU_A_REG_EMCORE_ERR_INT_MSK          EMCORE_ERR_INT_MSK; // 124C
	UINT32                          rsv_1250[44];     // 1250..12FC
	union CCU_A_REG_H2T_A_XSIZE                 H2T_A_XSIZE;      // 1300
	union CCU_A_REG_H2T_A_YSIZE                 H2T_A_YSIZE;      // 1304
	union CCU_A_REG_H2T_A_STRIDE                H2T_A_STRIDE;     // 1308
	union CCU_A_REG_H2T_A_BANK_LINE_NUMBER  H2T_A_BANK_LINE_NUMBER; // 130C
	union CCU_A_REG_H2T_A_CTL                   H2T_A_CTL;        // 1310
	union CCU_A_REG_H2T_A_BASE_ADDR_0           H2T_A_BASE_ADDR_0; // 1314
	union CCU_A_REG_H2T_A_BASE_ADDR_1           H2T_A_BASE_ADDR_1; // 1318
	union CCU_A_REG_H2T_A_BASE_ADDR_2           H2T_A_BASE_ADDR_2; // 131C
	union CCU_A_REG_H2T_A_BASE_ADDR_3           H2T_A_BASE_ADDR_3; // 1320
	union CCU_A_REG_H2T_A_BASE_ADDR_4           H2T_A_BASE_ADDR_4; // 1324
	union CCU_A_REG_H2T_A_BASE_ADDR_5           H2T_A_BASE_ADDR_5; // 1328
	union CCU_A_REG_H2T_A_BASE_ADDR_6           H2T_A_BASE_ADDR_6; // 132C
	union CCU_A_REG_H2T_A_BASE_ADDR_7           H2T_A_BASE_ADDR_7; // 1330
	union CCU_A_REG_H2T_A_BANK_STAT             H2T_A_BANK_STAT;  // 1334
	union CCU_A_REG_H2T_A_LINE_STAT             H2T_A_LINE_STAT;  // 1338
	union CCU_A_REG_H2T_A_LINE_AVA_CNT          H2T_A_LINE_AVA_CNT; // 133C
	union CCU_A_REG_H2T_A_SPARE                 H2T_A_SPARE;      // 1340
	UINT32                          rsv_1344[15];     // 1344..137C
	union CCU_A_REG_T2H_A_XSIZE                 T2H_A_XSIZE;      // 1380
	union CCU_A_REG_T2H_A_YSIZE                 T2H_A_YSIZE;      // 1384
	union CCU_A_REG_T2H_A_STRIDE                T2H_A_STRIDE;     // 1388
	union CCU_A_REG_T2H_A_BANK_LINE_NUMBER  T2H_A_BANK_LINE_NUMBER; // 138C
	union CCU_A_REG_T2H_A_CTL                   T2H_A_CTL;        // 1390
	union CCU_A_REG_T2H_A_BASE_ADDR_0           T2H_A_BASE_ADDR_0; // 1394
	union CCU_A_REG_T2H_A_BASE_ADDR_1           T2H_A_BASE_ADDR_1; // 1398
	union CCU_A_REG_T2H_A_BASE_ADDR_2           T2H_A_BASE_ADDR_2; // 139C
	union CCU_A_REG_T2H_A_BASE_ADDR_3           T2H_A_BASE_ADDR_3; // 13A0
	union CCU_A_REG_T2H_A_BASE_ADDR_4           T2H_A_BASE_ADDR_4; // 13A4
	union CCU_A_REG_T2H_A_BASE_ADDR_5           T2H_A_BASE_ADDR_5; // 13A8
	union CCU_A_REG_T2H_A_BASE_ADDR_6           T2H_A_BASE_ADDR_6; // 13AC
	union CCU_A_REG_T2H_A_BASE_ADDR_7           T2H_A_BASE_ADDR_7; // 13B0
	union CCU_A_REG_T2H_A_BANK_STAT             T2H_A_BANK_STAT;  // 13B4
	union CCU_A_REG_T2H_A_LINE_STAT             T2H_A_LINE_STAT;  // 13B8
	union CCU_A_REG_T2H_A_LINE_AVA_CNT          T2H_A_LINE_AVA_CNT; // 13BC
	union CCU_A_REG_T2H_A_SPARE                 T2H_A_SPARE;      // 13C0
	UINT32                          rsv_13C4[15];     // 13C4..13FC
	union CCU_A_REG_DMA_SOFT_RSTSTAT            DMA_SOFT_RSTSTAT; // 1400
	UINT32                          rsv_1404[3];      // 1404..140C
	union CCU_A_REG_VERTICAL_FLIP_EN            VERTICAL_FLIP_EN; // 1410
	union CCU_A_REG_DMA_SOFT_RESET              DMA_SOFT_RESET;   // 1414
	union CCU_A_REG_LAST_ULTRA_EN               LAST_ULTRA_EN;    // 1418
	union CCU_A_REG_SPECIAL_FUN_EN              SPECIAL_FUN_EN;   // 141C
	UINT32                          rsv_1420[4];      // 1420..142C
	union CCU_A_REG_CCUO_BASE_ADDR              CCUO_BASE_ADDR;   // 1430
	UINT32                          rsv_1434;         // 1434
	union CCU_A_REG_CCUO_OFST_ADDR              CCUO_OFST_ADDR;   // 1438
	union CCU_A_REG_CCUO_DRS                    CCUO_DRS;         // 143C
	union CCU_A_REG_CCUO_XSIZE                  CCUO_XSIZE;       // 1440
	union CCU_A_REG_CCUO_YSIZE                  CCUO_YSIZE;       // 1444
	union CCU_A_REG_CCUO_STRIDE                 CCUO_STRIDE;      // 1448
	union CCU_A_REG_CCUO_CON                    CCUO_CON;         // 144C
	union CCU_A_REG_CCUO_CON2                   CCUO_CON2;        // 1450
	union CCU_A_REG_CCUO_CON3                   CCUO_CON3;        // 1454
	union CCU_A_REG_CCUO_CROP                   CCUO_CROP;        // 1458
	UINT32                          rsv_145C;         // 145C
	union CCU_A_REG_TSFO_BASE_ADDR              TSFO_BASE_ADDR;   // 1460
	UINT32                          rsv_1464;         // 1464
	union CCU_A_REG_TSFO_OFST_ADDR              TSFO_OFST_ADDR;   // 1468
	union CCU_A_REG_TSFO_DRS                    TSFO_DRS;         // 146C
	union CCU_A_REG_TSFO_XSIZE                  TSFO_XSIZE;       // 1470
	union CCU_A_REG_TSFO_YSIZE                  TSFO_YSIZE;       // 1474
	union CCU_A_REG_TSFO_STRIDE                 TSFO_STRIDE;      // 1478
	union CCU_A_REG_TSFO_CON                    TSFO_CON;         // 147C
	union CCU_A_REG_TSFO_CON2                   TSFO_CON2;        // 1480
	union CCU_A_REG_TSFO_CON3                   TSFO_CON3;        // 1484
	union CCU_A_REG_TSFO_CROP                   TSFO_CROP;        // 1488
	UINT32                          rsv_148C;         // 148C
	union CCU_A_REG_CCUI_BASE_ADDR              CCUI_BASE_ADDR;   // 1490
	UINT32                          rsv_1494;         // 1494
	union CCU_A_REG_CCUI_OFST_ADDR              CCUI_OFST_ADDR;   // 1498
	union CCU_A_REG_CCUI_DRS                    CCUI_DRS;         // 149C
	union CCU_A_REG_CCUI_XSIZE                  CCUI_XSIZE;       // 14A0
	union CCU_A_REG_CCUI_YSIZE                  CCUI_YSIZE;       // 14A4
	union CCU_A_REG_CCUI_STRIDE                 CCUI_STRIDE;      // 14A8
	union CCU_A_REG_CCUI_CON                    CCUI_CON;         // 14AC
	union CCU_A_REG_CCUI_CON2                   CCUI_CON2;        // 14B0
	union CCU_A_REG_CCUI_CON3                   CCUI_CON3;        // 14B4
	UINT32                          rsv_14B8[2];      // 14B8..14BC
	union CCU_A_REG_TSFI_BASE_ADDR              TSFI_BASE_ADDR;   // 14C0
	UINT32                          rsv_14C4;         // 14C4
	union CCU_A_REG_TSFI_OFST_ADDR              TSFI_OFST_ADDR;   // 14C8
	union CCU_A_REG_TSFI_DRS                    TSFI_DRS;         // 14CC
	union CCU_A_REG_TSFI_XSIZE                  TSFI_XSIZE;       // 14D0
	union CCU_A_REG_TSFI_YSIZE                  TSFI_YSIZE;       // 14D4
	union CCU_A_REG_TSFI_STRIDE                 TSFI_STRIDE;      // 14D8
	union CCU_A_REG_TSFI_CON                    TSFI_CON;         // 14DC
	union CCU_A_REG_TSFI_CON2                   TSFI_CON2;        // 14E0
	union CCU_A_REG_TSFI_CON3                   TSFI_CON3;        // 14E4
	UINT32                          rsv_14E8[14];     // 14E8..151C
	union CCU_A_REG_DMA_ERR_CTRL                DMA_ERR_CTRL;     // 1520
	union CCU_A_REG_CCUO_ERR_STAT               CCUO_ERR_STAT;    // 1524
	union CCU_A_REG_TSFO_ERR_STAT               TSFO_ERR_STAT;    // 1528
	union CCU_A_REG_CCUI_ERR_STAT               CCUI_ERR_STAT;    // 152C
	union CCU_A_REG_TSFI_ERR_STAT               TSFI_ERR_STAT;    // 1530
	UINT32                          rsv_1534;         // 1534
	union CCU_A_REG_DMA_DEBUG_ADDR              DMA_DEBUG_ADDR;   // 1538
	union CCU_A_REG_DMA_RSV1                    DMA_RSV1;         // 153C
	union CCU_A_REG_DMA_RSV2                    DMA_RSV2;         // 1540
	union CCU_A_REG_DMA_RSV3                    DMA_RSV3;         // 1544
	union CCU_A_REG_DMA_RSV4                    DMA_RSV4;         // 1548
	union CCU_A_REG_DMA_DEBUG_SEL               DMA_DEBUG_SEL;    // 154C
	union CCU_A_REG_DMA_BW_SELF_TEST            DMA_BW_SELF_TEST; // 1550
};

#endif // __CCU_A_REGS_H__
