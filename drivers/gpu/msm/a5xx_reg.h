/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _A5XX_REG_H
#define _A5XX_REG_H

/* A5XX interrupt bits */
#define A5XX_INT_RBBM_GPU_IDLE           0
#define A5XX_INT_RBBM_AHB_ERROR          1
#define A5XX_INT_RBBM_TRANSFER_TIMEOUT   2
#define A5XX_INT_RBBM_ME_MS_TIMEOUT      3
#define A5XX_INT_RBBM_PFP_MS_TIMEOUT     4
#define A5XX_INT_RBBM_ETS_MS_TIMEOUT     5
#define A5XX_INT_RBBM_ATB_ASYNC_OVERFLOW 6
#define A5XX_INT_RBBM_GPC_ERROR          7
#define A5XX_INT_CP_SW                   8
#define A5XX_INT_CP_HW_ERROR             9
#define A5XX_INT_CP_CCU_FLUSH_DEPTH_TS   10
#define A5XX_INT_CP_CCU_FLUSH_COLOR_TS   11
#define A5XX_INT_CP_CCU_RESOLVE_TS       12
#define A5XX_INT_CP_IB2                  13
#define A5XX_INT_CP_IB1                  14
#define A5XX_INT_CP_RB                   15
#define A5XX_INT_CP_UNUSED_1             16
#define A5XX_INT_CP_RB_DONE_TS           17
#define A5XX_INT_CP_WT_DONE_TS           18
#define A5XX_INT_UNKNOWN_1               19
#define A5XX_INT_CP_CACHE_FLUSH_TS       20
#define A5XX_INT_UNUSED_2                21
#define A5XX_INT_RBBM_ATB_BUS_OVERFLOW   22
#define A5XX_INT_MISC_HANG_DETECT        23
#define A5XX_INT_UCHE_OOB_ACCESS         24
#define A5XX_INT_UCHE_TRAP_INTR          25
#define A5XX_INT_DEBBUS_INTR_0           26
#define A5XX_INT_DEBBUS_INTR_1           27
#define A5XX_INT_GPMU_VOLTAGE_DROOP      28
#define A5XX_INT_GPMU_FIRMWARE           29
#define A5XX_INT_ISDB_CPU_IRQ            30
#define A5XX_INT_ISDB_UNDER_DEBUG        31

/* CP Interrupt bits */
#define A5XX_CP_OPCODE_ERROR               0
#define A5XX_CP_RESERVED_BIT_ERROR         1
#define A5XX_CP_HW_FAULT_ERROR             2
#define A5XX_CP_DMA_ERROR                  3
#define A5XX_CP_REGISTER_PROTECTION_ERROR  4
#define A5XX_CP_AHB_ERROR                  5

/* CP registers */
#define A5XX_CP_RB_BASE                  0x800
#define A5XX_CP_RB_BASE_HI               0x801
#define A5XX_CP_RB_CNTL                  0x802
#define A5XX_CP_RB_RPTR_ADDR_LO          0x804
#define A5XX_CP_RB_RPTR_ADDR_HI          0x805
#define A5XX_CP_RB_RPTR                  0x806
#define A5XX_CP_RB_WPTR                  0x807
#define A5XX_CP_PFP_STAT_ADDR            0x808
#define A5XX_CP_PFP_STAT_DATA            0x809
#define A5XX_CP_DRAW_STATE_ADDR          0x80B
#define A5XX_CP_DRAW_STATE_DATA          0x80C
#define A5XX_CP_CRASH_SCRIPT_BASE_LO     0x817
#define A5XX_CP_CRASH_SCRIPT_BASE_HI     0x818
#define A5XX_CP_CRASH_DUMP_CNTL          0x819
#define A5XX_CP_ME_STAT_ADDR             0x81A
#define A5XX_CP_ROQ_THRESHOLDS_1         0x81F
#define A5XX_CP_ROQ_THRESHOLDS_2         0x820
#define A5XX_CP_ROQ_DBG_ADDR             0x821
#define A5XX_CP_ROQ_DBG_DATA             0x822
#define A5XX_CP_MEQ_DBG_ADDR             0x823
#define A5XX_CP_MEQ_DBG_DATA             0x824
#define A5XX_CP_MEQ_THRESHOLDS           0x825
#define A5XX_CP_MERCIU_SIZE              0x826
#define A5XX_CP_MERCIU_DBG_ADDR          0x827
#define A5XX_CP_MERCIU_DBG_DATA_1        0x828
#define A5XX_CP_MERCIU_DBG_DATA_2        0x829
#define A5XX_CP_PFP_UCODE_DBG_ADDR       0x82A
#define A5XX_CP_PFP_UCODE_DBG_DATA       0x82B
#define A5XX_CP_ME_UCODE_DBG_ADDR        0x82F
#define A5XX_CP_ME_UCODE_DBG_DATA        0x830
#define A5XX_CP_CNTL                     0x831
#define A5XX_CP_ME_CNTL                  0x832
#define A5XX_CP_CHICKEN_DBG              0x833
#define A5XX_CP_PFP_INSTR_BASE_LO        0x835
#define A5XX_CP_PFP_INSTR_BASE_HI        0x836
#define A5XX_CP_PM4_INSTR_BASE_LO        0x838
#define A5XX_CP_PM4_INSTR_BASE_HI        0x839
#define A5XX_CP_CONTEXT_SWITCH_CNTL      0x83B
#define A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_LO   0x83C
#define A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_HI   0x83D
#define A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_LO   0x83E
#define A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_HI   0x83F
#define A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_LO   0x840
#define A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_HI   0x841
#define A5XX_CP_ADDR_MODE_CNTL           0x860
#define A5XX_CP_ME_STAT_DATA             0xB14
#define A5XX_CP_WFI_PEND_CTR             0xB15
#define A5XX_CP_INTERRUPT_STATUS         0xB18
#define A5XX_CP_HW_FAULT                 0xB1A
#define A5XX_CP_PROTECT_STATUS           0xB1C
#define A5XX_CP_IB1_BASE                 0xB1F
#define A5XX_CP_IB1_BASE_HI              0xB20
#define A5XX_CP_IB1_BUFSZ                0xB21
#define A5XX_CP_IB2_BASE                 0xB22
#define A5XX_CP_IB2_BASE_HI              0xB23
#define A5XX_CP_IB2_BUFSZ                0xB24
#define A5XX_CP_PROTECT_REG_0            0x880
#define A5XX_CP_PROTECT_CNTL             0x8A0
#define A5XX_CP_AHB_FAULT                0xB1B
#define A5XX_CP_PERFCTR_CP_SEL_0         0xBB0
#define A5XX_CP_PERFCTR_CP_SEL_1         0xBB1
#define A5XX_CP_PERFCTR_CP_SEL_2         0xBB2
#define A5XX_CP_PERFCTR_CP_SEL_3         0xBB3
#define A5XX_CP_PERFCTR_CP_SEL_4         0xBB4
#define A5XX_CP_PERFCTR_CP_SEL_5         0xBB5
#define A5XX_CP_PERFCTR_CP_SEL_6         0xBB6
#define A5XX_CP_PERFCTR_CP_SEL_7         0xBB7

#define A5XX_VSC_ADDR_MODE_CNTL          0xBC1

/* CP Power Counter Registers Select */
#define A5XX_CP_POWERCTR_CP_SEL_0        0xBBA
#define A5XX_CP_POWERCTR_CP_SEL_1        0xBBB
#define A5XX_CP_POWERCTR_CP_SEL_2        0xBBC
#define A5XX_CP_POWERCTR_CP_SEL_3        0xBBD

/* RBBM registers */
#define A5XX_RBBM_CFG_DBGBUS_SEL_A               0x4
#define A5XX_RBBM_CFG_DBGBUS_SEL_B               0x5
#define A5XX_RBBM_CFG_DBGBUS_SEL_C               0x6
#define A5XX_RBBM_CFG_DBGBUS_SEL_D               0x7
#define A5XX_RBBM_CFG_DBGBUS_SEL_PING_INDEX_SHIFT    0x0
#define A5XX_RBBM_CFG_DBGBUS_SEL_PING_BLK_SEL_SHIFT  0x8

#define A5XX_RBBM_CFG_DBGBUS_CNTLT               0x8
#define A5XX_RBBM_CFG_DBGBUS_CNTLM               0x9
#define A5XX_RBBM_CFG_DEBBUS_CTLTM_ENABLE_SHIFT  0x18
#define A5XX_RBBM_CFG_DBGBUS_OPL                 0xA
#define A5XX_RBBM_CFG_DBGBUS_OPE                 0xB
#define A5XX_RBBM_CFG_DBGBUS_IVTL_0              0xC
#define A5XX_RBBM_CFG_DBGBUS_IVTL_1              0xD
#define A5XX_RBBM_CFG_DBGBUS_IVTL_2              0xE
#define A5XX_RBBM_CFG_DBGBUS_IVTL_3              0xF
#define A5XX_RBBM_CFG_DBGBUS_MASKL_0             0x10
#define A5XX_RBBM_CFG_DBGBUS_MASKL_1             0x11
#define A5XX_RBBM_CFG_DBGBUS_MASKL_2             0x12
#define A5XX_RBBM_CFG_DBGBUS_MASKL_3             0x13
#define A5XX_RBBM_CFG_DBGBUS_BYTEL_0             0x14
#define A5XX_RBBM_CFG_DBGBUS_BYTEL_1             0x15
#define A5XX_RBBM_CFG_DBGBUS_IVTE_0              0x16
#define A5XX_RBBM_CFG_DBGBUS_IVTE_1              0x17
#define A5XX_RBBM_CFG_DBGBUS_IVTE_2              0x18
#define A5XX_RBBM_CFG_DBGBUS_IVTE_3              0x19
#define A5XX_RBBM_CFG_DBGBUS_MASKE_0             0x1A
#define A5XX_RBBM_CFG_DBGBUS_MASKE_1             0x1B
#define A5XX_RBBM_CFG_DBGBUS_MASKE_2             0x1C
#define A5XX_RBBM_CFG_DBGBUS_MASKE_3             0x1D
#define A5XX_RBBM_CFG_DBGBUS_NIBBLEE             0x1E
#define A5XX_RBBM_CFG_DBGBUS_PTRC0               0x1F
#define A5XX_RBBM_CFG_DBGBUS_PTRC1               0x20
#define A5XX_RBBM_CFG_DBGBUS_LOADREG             0x21
#define A5XX_RBBM_CFG_DBGBUS_IDX                 0x22
#define A5XX_RBBM_CFG_DBGBUS_CLRC                0x23
#define A5XX_RBBM_CFG_DBGBUS_LOADIVT             0x24
#define A5XX_RBBM_INTERFACE_HANG_INT_CNTL        0x2F
#define A5XX_RBBM_INT_CLEAR_CMD                  0x37
#define A5XX_RBBM_INT_0_MASK                     0x38
#define A5XX_RBBM_AHB_DBG_CNTL                   0x3F
#define A5XX_RBBM_EXT_VBIF_DBG_CNTL              0x41
#define A5XX_RBBM_SW_RESET_CMD                   0x43
#define A5XX_RBBM_BLOCK_SW_RESET_CMD             0x45
#define A5XX_RBBM_BLOCK_SW_RESET_CMD2            0x46
#define A5XX_RBBM_DBG_LO_HI_GPIO                 0x48
#define A5XX_RBBM_EXT_TRACE_BUS_CNTL             0x49
#define A5XX_RBBM_CLOCK_CNTL_TP0                 0x4A
#define A5XX_RBBM_CLOCK_CNTL_TP1                 0x4B
#define A5XX_RBBM_CLOCK_CNTL_TP2                 0x4C
#define A5XX_RBBM_CLOCK_CNTL_TP3                 0x4D
#define A5XX_RBBM_CLOCK_CNTL2_TP0                0x4E
#define A5XX_RBBM_CLOCK_CNTL2_TP1                0x4F
#define A5XX_RBBM_CLOCK_CNTL2_TP2                0x50
#define A5XX_RBBM_CLOCK_CNTL2_TP3                0x51
#define A5XX_RBBM_CLOCK_CNTL3_TP0                0x52
#define A5XX_RBBM_CLOCK_CNTL3_TP1                0x53
#define A5XX_RBBM_CLOCK_CNTL3_TP2                0x54
#define A5XX_RBBM_CLOCK_CNTL3_TP3                0x55
#define A5XX_RBBM_READ_AHB_THROUGH_DBG           0x59
#define A5XX_RBBM_CLOCK_CNTL_UCHE                0x5A
#define A5XX_RBBM_CLOCK_CNTL2_UCHE               0x5B
#define A5XX_RBBM_CLOCK_CNTL3_UCHE               0x5C
#define A5XX_RBBM_CLOCK_CNTL4_UCHE               0x5D
#define A5XX_RBBM_CLOCK_HYST_UCHE                0x5E
#define A5XX_RBBM_CLOCK_DELAY_UCHE               0x5F
#define A5XX_RBBM_CLOCK_MODE_GPC                 0x60
#define A5XX_RBBM_CLOCK_DELAY_GPC                0x61
#define A5XX_RBBM_CLOCK_HYST_GPC                 0x62
#define A5XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM        0x63
#define A5XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM        0x64
#define A5XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM       0x65
#define A5XX_RBBM_CLOCK_DELAY_HLSQ               0x66
#define A5XX_RBBM_CLOCK_CNTL                     0x67
#define A5XX_RBBM_CLOCK_CNTL_SP0                 0x68
#define A5XX_RBBM_CLOCK_CNTL_SP1                 0x69
#define A5XX_RBBM_CLOCK_CNTL_SP2                 0x6A
#define A5XX_RBBM_CLOCK_CNTL_SP3                 0x6B
#define A5XX_RBBM_CLOCK_CNTL2_SP0                0x6C
#define A5XX_RBBM_CLOCK_CNTL2_SP1                0x6D
#define A5XX_RBBM_CLOCK_CNTL2_SP2                0x6E
#define A5XX_RBBM_CLOCK_CNTL2_SP3                0x6F
#define A5XX_RBBM_CLOCK_HYST_SP0                 0x70
#define A5XX_RBBM_CLOCK_HYST_SP1                 0x71
#define A5XX_RBBM_CLOCK_HYST_SP2                 0x72
#define A5XX_RBBM_CLOCK_HYST_SP3                 0x73
#define A5XX_RBBM_CLOCK_DELAY_SP0                0x74
#define A5XX_RBBM_CLOCK_DELAY_SP1                0x75
#define A5XX_RBBM_CLOCK_DELAY_SP2                0x76
#define A5XX_RBBM_CLOCK_DELAY_SP3                0x77
#define A5XX_RBBM_CLOCK_CNTL_RB0                 0x78
#define A5XX_RBBM_CLOCK_CNTL_RB1                 0x79
#define A5XX_RBBM_CLOCK_CNTL_RB2                 0x7a
#define A5XX_RBBM_CLOCK_CNTL_RB3                 0x7B
#define A5XX_RBBM_CLOCK_CNTL2_RB0                0x7C
#define A5XX_RBBM_CLOCK_CNTL2_RB1                0x7D
#define A5XX_RBBM_CLOCK_CNTL2_RB2                0x7E
#define A5XX_RBBM_CLOCK_CNTL2_RB3                0x7F
#define A5XX_RBBM_CLOCK_HYST_RAC                 0x80
#define A5XX_RBBM_CLOCK_DELAY_RAC                0x81
#define A5XX_RBBM_CLOCK_CNTL_CCU0                0x82
#define A5XX_RBBM_CLOCK_CNTL_CCU1                0x83
#define A5XX_RBBM_CLOCK_CNTL_CCU2                0x84
#define A5XX_RBBM_CLOCK_CNTL_CCU3                0x85
#define A5XX_RBBM_CLOCK_HYST_RB_CCU0             0x86
#define A5XX_RBBM_CLOCK_HYST_RB_CCU1             0x87
#define A5XX_RBBM_CLOCK_HYST_RB_CCU2             0x88
#define A5XX_RBBM_CLOCK_HYST_RB_CCU3             0x89
#define A5XX_RBBM_CLOCK_CNTL_RAC                 0x8A
#define A5XX_RBBM_CLOCK_CNTL2_RAC                0x8B
#define A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_0        0x8C
#define A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_1        0x8D
#define A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_2        0x8E
#define A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_3        0x8F
#define A5XX_RBBM_CLOCK_HYST_VFD                 0x90
#define A5XX_RBBM_CLOCK_MODE_VFD                 0x91
#define A5XX_RBBM_CLOCK_DELAY_VFD                0x92
#define A5XX_RBBM_AHB_CNTL0                      0x93
#define A5XX_RBBM_AHB_CNTL1                      0x94
#define A5XX_RBBM_AHB_CNTL2                      0x95
#define A5XX_RBBM_AHB_CMD                        0x96
#define A5XX_RBBM_INTERFACE_HANG_MASK_CNTL11     0x9C
#define A5XX_RBBM_INTERFACE_HANG_MASK_CNTL12     0x9D
#define A5XX_RBBM_INTERFACE_HANG_MASK_CNTL13     0x9E
#define A5XX_RBBM_INTERFACE_HANG_MASK_CNTL14     0x9F
#define A5XX_RBBM_INTERFACE_HANG_MASK_CNTL15     0xA0
#define A5XX_RBBM_INTERFACE_HANG_MASK_CNTL16     0xA1
#define A5XX_RBBM_INTERFACE_HANG_MASK_CNTL17     0xA2
#define A5XX_RBBM_INTERFACE_HANG_MASK_CNTL18     0xA3
#define A5XX_RBBM_CLOCK_DELAY_TP0                0xA4
#define A5XX_RBBM_CLOCK_DELAY_TP1                0xA5
#define A5XX_RBBM_CLOCK_DELAY_TP2                0xA6
#define A5XX_RBBM_CLOCK_DELAY_TP3                0xA7
#define A5XX_RBBM_CLOCK_DELAY2_TP0               0xA8
#define A5XX_RBBM_CLOCK_DELAY2_TP1               0xA9
#define A5XX_RBBM_CLOCK_DELAY2_TP2               0xAA
#define A5XX_RBBM_CLOCK_DELAY2_TP3               0xAB
#define A5XX_RBBM_CLOCK_DELAY3_TP0               0xAC
#define A5XX_RBBM_CLOCK_DELAY3_TP1               0xAD
#define A5XX_RBBM_CLOCK_DELAY3_TP2               0xAE
#define A5XX_RBBM_CLOCK_DELAY3_TP3               0xAF
#define A5XX_RBBM_CLOCK_HYST_TP0                 0xB0
#define A5XX_RBBM_CLOCK_HYST_TP1                 0xB1
#define A5XX_RBBM_CLOCK_HYST_TP2                 0xB2
#define A5XX_RBBM_CLOCK_HYST_TP3                 0xB3
#define A5XX_RBBM_CLOCK_HYST2_TP0                0xB4
#define A5XX_RBBM_CLOCK_HYST2_TP1                0xB5
#define A5XX_RBBM_CLOCK_HYST2_TP2                0xB6
#define A5XX_RBBM_CLOCK_HYST2_TP3                0xB7
#define A5XX_RBBM_CLOCK_HYST3_TP0                0xB8
#define A5XX_RBBM_CLOCK_HYST3_TP1                0xB9
#define A5XX_RBBM_CLOCK_HYST3_TP2                0xBA
#define A5XX_RBBM_CLOCK_HYST3_TP3                0xBB
#define A5XX_RBBM_CLOCK_CNTL_GPMU                0xC8
#define A5XX_RBBM_CLOCK_DELAY_GPMU               0xC9
#define A5XX_RBBM_CLOCK_HYST_GPMU                0xCA
#define A5XX_RBBM_PERFCTR_CP_0_LO                0x3A0
#define A5XX_RBBM_PERFCTR_CP_0_HI                0x3A1
#define A5XX_RBBM_PERFCTR_CP_1_LO                0x3A2
#define A5XX_RBBM_PERFCTR_CP_1_HI                0x3A3
#define A5XX_RBBM_PERFCTR_CP_2_LO                0x3A4
#define A5XX_RBBM_PERFCTR_CP_2_HI                0x3A5
#define A5XX_RBBM_PERFCTR_CP_3_LO                0x3A6
#define A5XX_RBBM_PERFCTR_CP_3_HI                0x3A7
#define A5XX_RBBM_PERFCTR_CP_4_LO                0x3A8
#define A5XX_RBBM_PERFCTR_CP_4_HI                0x3A9
#define A5XX_RBBM_PERFCTR_CP_5_LO                0x3AA
#define A5XX_RBBM_PERFCTR_CP_5_HI                0x3AB
#define A5XX_RBBM_PERFCTR_CP_6_LO                0x3AC
#define A5XX_RBBM_PERFCTR_CP_6_HI                0x3AD
#define A5XX_RBBM_PERFCTR_CP_7_LO                0x3AE
#define A5XX_RBBM_PERFCTR_CP_7_HI                0x3AF
#define A5XX_RBBM_PERFCTR_RBBM_0_LO              0x3B0
#define A5XX_RBBM_PERFCTR_RBBM_0_HI              0x3B1
#define A5XX_RBBM_PERFCTR_RBBM_1_LO              0x3B2
#define A5XX_RBBM_PERFCTR_RBBM_1_HI              0x3B3
#define A5XX_RBBM_PERFCTR_RBBM_2_LO              0x3B4
#define A5XX_RBBM_PERFCTR_RBBM_2_HI              0x3B5
#define A5XX_RBBM_PERFCTR_RBBM_3_LO              0x3B6
#define A5XX_RBBM_PERFCTR_RBBM_3_HI              0x3B7
#define A5XX_RBBM_PERFCTR_PC_0_LO                0x3B8
#define A5XX_RBBM_PERFCTR_PC_0_HI                0x3B9
#define A5XX_RBBM_PERFCTR_PC_1_LO                0x3BA
#define A5XX_RBBM_PERFCTR_PC_1_HI                0x3BB
#define A5XX_RBBM_PERFCTR_PC_2_LO                0x3BC
#define A5XX_RBBM_PERFCTR_PC_2_HI                0x3BD
#define A5XX_RBBM_PERFCTR_PC_3_LO                0x3BE
#define A5XX_RBBM_PERFCTR_PC_3_HI                0x3BF
#define A5XX_RBBM_PERFCTR_PC_4_LO                0x3C0
#define A5XX_RBBM_PERFCTR_PC_4_HI                0x3C1
#define A5XX_RBBM_PERFCTR_PC_5_LO                0x3C2
#define A5XX_RBBM_PERFCTR_PC_5_HI                0x3C3
#define A5XX_RBBM_PERFCTR_PC_6_LO                0x3C4
#define A5XX_RBBM_PERFCTR_PC_6_HI                0x3C5
#define A5XX_RBBM_PERFCTR_PC_7_LO                0x3C6
#define A5XX_RBBM_PERFCTR_PC_7_HI                0x3C7
#define A5XX_RBBM_PERFCTR_VFD_0_LO               0x3C8
#define A5XX_RBBM_PERFCTR_VFD_0_HI               0x3C9
#define A5XX_RBBM_PERFCTR_VFD_1_LO               0x3CA
#define A5XX_RBBM_PERFCTR_VFD_1_HI               0x3CB
#define A5XX_RBBM_PERFCTR_VFD_2_LO               0x3CC
#define A5XX_RBBM_PERFCTR_VFD_2_HI               0x3CD
#define A5XX_RBBM_PERFCTR_VFD_3_LO               0x3CE
#define A5XX_RBBM_PERFCTR_VFD_3_HI               0x3CF
#define A5XX_RBBM_PERFCTR_VFD_4_LO               0x3D0
#define A5XX_RBBM_PERFCTR_VFD_4_HI               0x3D1
#define A5XX_RBBM_PERFCTR_VFD_5_LO               0x3D2
#define A5XX_RBBM_PERFCTR_VFD_5_HI               0x3D3
#define A5XX_RBBM_PERFCTR_VFD_6_LO               0x3D4
#define A5XX_RBBM_PERFCTR_VFD_6_HI               0x3D5
#define A5XX_RBBM_PERFCTR_VFD_7_LO               0x3D6
#define A5XX_RBBM_PERFCTR_VFD_7_HI               0x3D7
#define A5XX_RBBM_PERFCTR_HLSQ_0_LO              0x3D8
#define A5XX_RBBM_PERFCTR_HLSQ_0_HI              0x3D9
#define A5XX_RBBM_PERFCTR_HLSQ_1_LO              0x3DA
#define A5XX_RBBM_PERFCTR_HLSQ_1_HI              0x3DB
#define A5XX_RBBM_PERFCTR_HLSQ_2_LO              0x3DC
#define A5XX_RBBM_PERFCTR_HLSQ_2_HI              0x3DD
#define A5XX_RBBM_PERFCTR_HLSQ_3_LO              0x3DE
#define A5XX_RBBM_PERFCTR_HLSQ_3_HI              0x3DF
#define A5XX_RBBM_PERFCTR_HLSQ_4_LO              0x3E0
#define A5XX_RBBM_PERFCTR_HLSQ_4_HI              0x3E1
#define A5XX_RBBM_PERFCTR_HLSQ_5_LO              0x3E2
#define A5XX_RBBM_PERFCTR_HLSQ_5_HI              0x3E3
#define A5XX_RBBM_PERFCTR_HLSQ_6_LO              0x3E4
#define A5XX_RBBM_PERFCTR_HLSQ_6_HI              0x3E5
#define A5XX_RBBM_PERFCTR_HLSQ_7_LO              0x3E6
#define A5XX_RBBM_PERFCTR_HLSQ_7_HI              0x3E7
#define A5XX_RBBM_PERFCTR_VPC_0_LO               0x3E8
#define A5XX_RBBM_PERFCTR_VPC_0_HI               0x3E9
#define A5XX_RBBM_PERFCTR_VPC_1_LO               0x3EA
#define A5XX_RBBM_PERFCTR_VPC_1_HI               0x3EB
#define A5XX_RBBM_PERFCTR_VPC_2_LO               0x3EC
#define A5XX_RBBM_PERFCTR_VPC_2_HI               0x3ED
#define A5XX_RBBM_PERFCTR_VPC_3_LO               0x3EE
#define A5XX_RBBM_PERFCTR_VPC_3_HI               0x3EF
#define A5XX_RBBM_PERFCTR_CCU_0_LO               0x3F0
#define A5XX_RBBM_PERFCTR_CCU_0_HI               0x3F1
#define A5XX_RBBM_PERFCTR_CCU_1_LO               0x3F2
#define A5XX_RBBM_PERFCTR_CCU_1_HI               0x3F3
#define A5XX_RBBM_PERFCTR_CCU_2_LO               0x3F4
#define A5XX_RBBM_PERFCTR_CCU_2_HI               0x3F5
#define A5XX_RBBM_PERFCTR_CCU_3_LO               0x3F6
#define A5XX_RBBM_PERFCTR_CCU_3_HI               0x3F7
#define A5XX_RBBM_PERFCTR_TSE_0_LO               0x3F8
#define A5XX_RBBM_PERFCTR_TSE_0_HI               0x3F9
#define A5XX_RBBM_PERFCTR_TSE_1_LO               0x3FA
#define A5XX_RBBM_PERFCTR_TSE_1_HI               0x3FB
#define A5XX_RBBM_PERFCTR_TSE_2_LO               0x3FC
#define A5XX_RBBM_PERFCTR_TSE_2_HI               0x3FD
#define A5XX_RBBM_PERFCTR_TSE_3_LO               0x3FE
#define A5XX_RBBM_PERFCTR_TSE_3_HI               0x3FF
#define A5XX_RBBM_PERFCTR_RAS_0_LO               0x400
#define A5XX_RBBM_PERFCTR_RAS_0_HI               0x401
#define A5XX_RBBM_PERFCTR_RAS_1_LO               0x402
#define A5XX_RBBM_PERFCTR_RAS_1_HI               0x403
#define A5XX_RBBM_PERFCTR_RAS_2_LO               0x404
#define A5XX_RBBM_PERFCTR_RAS_2_HI               0x405
#define A5XX_RBBM_PERFCTR_RAS_3_LO               0x406
#define A5XX_RBBM_PERFCTR_RAS_3_HI               0x407
#define A5XX_RBBM_PERFCTR_UCHE_0_LO              0x408
#define A5XX_RBBM_PERFCTR_UCHE_0_HI              0x409
#define A5XX_RBBM_PERFCTR_UCHE_1_LO              0x40A
#define A5XX_RBBM_PERFCTR_UCHE_1_HI              0x40B
#define A5XX_RBBM_PERFCTR_UCHE_2_LO              0x40C
#define A5XX_RBBM_PERFCTR_UCHE_2_HI              0x40D
#define A5XX_RBBM_PERFCTR_UCHE_3_LO              0x40E
#define A5XX_RBBM_PERFCTR_UCHE_3_HI              0x40F
#define A5XX_RBBM_PERFCTR_UCHE_4_LO              0x410
#define A5XX_RBBM_PERFCTR_UCHE_4_HI              0x411
#define A5XX_RBBM_PERFCTR_UCHE_5_LO              0x412
#define A5XX_RBBM_PERFCTR_UCHE_5_HI              0x413
#define A5XX_RBBM_PERFCTR_UCHE_6_LO              0x414
#define A5XX_RBBM_PERFCTR_UCHE_6_HI              0x415
#define A5XX_RBBM_PERFCTR_UCHE_7_LO              0x416
#define A5XX_RBBM_PERFCTR_UCHE_7_HI              0x417
#define A5XX_RBBM_PERFCTR_TP_0_LO                0x418
#define A5XX_RBBM_PERFCTR_TP_0_HI                0x419
#define A5XX_RBBM_PERFCTR_TP_1_LO                0x41A
#define A5XX_RBBM_PERFCTR_TP_1_HI                0x41B
#define A5XX_RBBM_PERFCTR_TP_2_LO                0x41C
#define A5XX_RBBM_PERFCTR_TP_2_HI                0x41D
#define A5XX_RBBM_PERFCTR_TP_3_LO                0x41E
#define A5XX_RBBM_PERFCTR_TP_3_HI                0x41F
#define A5XX_RBBM_PERFCTR_TP_4_LO                0x420
#define A5XX_RBBM_PERFCTR_TP_4_HI                0x421
#define A5XX_RBBM_PERFCTR_TP_5_LO                0x422
#define A5XX_RBBM_PERFCTR_TP_5_HI                0x423
#define A5XX_RBBM_PERFCTR_TP_6_LO                0x424
#define A5XX_RBBM_PERFCTR_TP_6_HI                0x425
#define A5XX_RBBM_PERFCTR_TP_7_LO                0x426
#define A5XX_RBBM_PERFCTR_TP_7_HI                0x427
#define A5XX_RBBM_PERFCTR_SP_0_LO                0x428
#define A5XX_RBBM_PERFCTR_SP_0_HI                0x429
#define A5XX_RBBM_PERFCTR_SP_1_LO                0x42A
#define A5XX_RBBM_PERFCTR_SP_1_HI                0x42B
#define A5XX_RBBM_PERFCTR_SP_2_LO                0x42C
#define A5XX_RBBM_PERFCTR_SP_2_HI                0x42D
#define A5XX_RBBM_PERFCTR_SP_3_LO                0x42E
#define A5XX_RBBM_PERFCTR_SP_3_HI                0x42F
#define A5XX_RBBM_PERFCTR_SP_4_LO                0x430
#define A5XX_RBBM_PERFCTR_SP_4_HI                0x431
#define A5XX_RBBM_PERFCTR_SP_5_LO                0x432
#define A5XX_RBBM_PERFCTR_SP_5_HI                0x433
#define A5XX_RBBM_PERFCTR_SP_6_LO                0x434
#define A5XX_RBBM_PERFCTR_SP_6_HI                0x435
#define A5XX_RBBM_PERFCTR_SP_7_LO                0x436
#define A5XX_RBBM_PERFCTR_SP_7_HI                0x437
#define A5XX_RBBM_PERFCTR_SP_8_LO                0x438
#define A5XX_RBBM_PERFCTR_SP_8_HI                0x439
#define A5XX_RBBM_PERFCTR_SP_9_LO                0x43A
#define A5XX_RBBM_PERFCTR_SP_9_HI                0x43B
#define A5XX_RBBM_PERFCTR_SP_10_LO               0x43C
#define A5XX_RBBM_PERFCTR_SP_10_HI               0x43D
#define A5XX_RBBM_PERFCTR_SP_11_LO               0x43E
#define A5XX_RBBM_PERFCTR_SP_11_HI               0x43F
#define A5XX_RBBM_PERFCTR_RB_0_LO                0x440
#define A5XX_RBBM_PERFCTR_RB_0_HI                0x441
#define A5XX_RBBM_PERFCTR_RB_1_LO                0x442
#define A5XX_RBBM_PERFCTR_RB_1_HI                0x443
#define A5XX_RBBM_PERFCTR_RB_2_LO                0x444
#define A5XX_RBBM_PERFCTR_RB_2_HI                0x445
#define A5XX_RBBM_PERFCTR_RB_3_LO                0x446
#define A5XX_RBBM_PERFCTR_RB_3_HI                0x447
#define A5XX_RBBM_PERFCTR_RB_4_LO                0x448
#define A5XX_RBBM_PERFCTR_RB_4_HI                0x449
#define A5XX_RBBM_PERFCTR_RB_5_LO                0x44A
#define A5XX_RBBM_PERFCTR_RB_5_HI                0x44B
#define A5XX_RBBM_PERFCTR_RB_6_LO                0x44C
#define A5XX_RBBM_PERFCTR_RB_6_HI                0x44D
#define A5XX_RBBM_PERFCTR_RB_7_LO                0x44E
#define A5XX_RBBM_PERFCTR_RB_7_HI                0x44F
#define A5XX_RBBM_PERFCTR_VSC_0_LO               0x450
#define A5XX_RBBM_PERFCTR_VSC_0_HI               0x451
#define A5XX_RBBM_PERFCTR_VSC_1_LO               0x452
#define A5XX_RBBM_PERFCTR_VSC_1_HI               0x453
#define A5XX_RBBM_PERFCTR_LRZ_0_LO               0x454
#define A5XX_RBBM_PERFCTR_LRZ_0_HI               0x455
#define A5XX_RBBM_PERFCTR_LRZ_1_LO               0x456
#define A5XX_RBBM_PERFCTR_LRZ_1_HI               0x457
#define A5XX_RBBM_PERFCTR_LRZ_2_LO               0x458
#define A5XX_RBBM_PERFCTR_LRZ_2_HI               0x459
#define A5XX_RBBM_PERFCTR_LRZ_3_LO               0x45A
#define A5XX_RBBM_PERFCTR_LRZ_3_HI               0x45B
#define A5XX_RBBM_PERFCTR_CMP_0_LO               0x45C
#define A5XX_RBBM_PERFCTR_CMP_0_HI               0x45D
#define A5XX_RBBM_PERFCTR_CMP_1_LO               0x45E
#define A5XX_RBBM_PERFCTR_CMP_1_HI               0x45F
#define A5XX_RBBM_PERFCTR_CMP_2_LO               0x460
#define A5XX_RBBM_PERFCTR_CMP_2_HI               0x461
#define A5XX_RBBM_PERFCTR_CMP_3_LO               0x462
#define A5XX_RBBM_PERFCTR_CMP_3_HI               0x463
#define A5XX_RBBM_PERFCTR_RBBM_SEL_0             0x46B
#define A5XX_RBBM_PERFCTR_RBBM_SEL_1             0x46C
#define A5XX_RBBM_PERFCTR_RBBM_SEL_2             0x46D
#define A5XX_RBBM_PERFCTR_RBBM_SEL_3             0x46E
#define A5XX_RBBM_ALWAYSON_COUNTER_LO            0x4D2
#define A5XX_RBBM_ALWAYSON_COUNTER_HI            0x4D3
#define A5XX_RBBM_STATUS                         0x4F5
#define A5XX_RBBM_STATUS3                        0x530
#define A5XX_RBBM_INT_0_STATUS                   0x4E1
#define A5XX_RBBM_AHB_ME_SPLIT_STATUS            0x4F0
#define A5XX_RBBM_AHB_PFP_SPLIT_STATUS           0x4F1
#define A5XX_RBBM_AHB_ERROR_STATUS               0x4F4
#define A5XX_RBBM_PERFCTR_CNTL                   0x464
#define A5XX_RBBM_PERFCTR_LOAD_CMD0              0x465
#define A5XX_RBBM_PERFCTR_LOAD_CMD1              0x466
#define A5XX_RBBM_PERFCTR_LOAD_CMD2              0x467
#define A5XX_RBBM_PERFCTR_LOAD_CMD3              0x468
#define A5XX_RBBM_PERFCTR_LOAD_VALUE_LO          0x469
#define A5XX_RBBM_PERFCTR_LOAD_VALUE_HI          0x46A
#define A5XX_RBBM_PERFCTR_RBBM_SEL_0             0x46B
#define A5XX_RBBM_PERFCTR_RBBM_SEL_1             0x46C
#define A5XX_RBBM_PERFCTR_RBBM_SEL_2             0x46D
#define A5XX_RBBM_PERFCTR_RBBM_SEL_3             0x46E
#define A5XX_RBBM_PERFCTR_GPU_BUSY_MASKED        0x46F
#define A5XX_RBBM_CFG_DBGBUS_EVENT_LOGIC         0x504
#define A5XX_RBBM_CFG_DBGBUS_OVER                0x505
#define A5XX_RBBM_CFG_DBGBUS_COUNT0              0x506
#define A5XX_RBBM_CFG_DBGBUS_COUNT1              0x507
#define A5XX_RBBM_CFG_DBGBUS_COUNT2              0x508
#define A5XX_RBBM_CFG_DBGBUS_COUNT3              0x509
#define A5XX_RBBM_CFG_DBGBUS_COUNT4              0x50A
#define A5XX_RBBM_CFG_DBGBUS_COUNT5              0x50B
#define A5XX_RBBM_CFG_DBGBUS_TRACE_ADDR          0x50C
#define A5XX_RBBM_CFG_DBGBUS_TRACE_BUF0          0x50D
#define A5XX_RBBM_CFG_DBGBUS_TRACE_BUF1          0x50E
#define A5XX_RBBM_CFG_DBGBUS_TRACE_BUF2          0x50F
#define A5XX_RBBM_CFG_DBGBUS_TRACE_BUF3          0x510
#define A5XX_RBBM_CFG_DBGBUS_TRACE_BUF4          0x511
#define A5XX_RBBM_CFG_DBGBUS_MISR0               0x512
#define A5XX_RBBM_CFG_DBGBUS_MISR1               0x513
#define A5XX_RBBM_ISDB_CNT                       0x533
#define A5XX_RBBM_SECVID_TRUST_CONFIG            0xF000
#define A5XX_RBBM_SECVID_TRUST_CNTL              0xF400
#define A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO     0xF800
#define A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_HI     0xF801
#define A5XX_RBBM_SECVID_TSB_TRUSTED_SIZE        0xF802
#define A5XX_RBBM_SECVID_TSB_CNTL                0xF803
#define A5XX_RBBM_SECVID_TSB_ADDR_MODE_CNTL      0xF810

/* VSC registers */
#define A5XX_VSC_PERFCTR_VSC_SEL_0          0xC60
#define A5XX_VSC_PERFCTR_VSC_SEL_1          0xC61

#define A5XX_GRAS_ADDR_MODE_CNTL            0xC81

/* TSE registers */
#define A5XX_GRAS_PERFCTR_TSE_SEL_0         0xC90
#define A5XX_GRAS_PERFCTR_TSE_SEL_1         0xC91
#define A5XX_GRAS_PERFCTR_TSE_SEL_2         0xC92
#define A5XX_GRAS_PERFCTR_TSE_SEL_3         0xC93

/* RAS registers */
#define A5XX_GRAS_PERFCTR_RAS_SEL_0         0xC94
#define A5XX_GRAS_PERFCTR_RAS_SEL_1         0xC95
#define A5XX_GRAS_PERFCTR_RAS_SEL_2         0xC96
#define A5XX_GRAS_PERFCTR_RAS_SEL_3         0xC97

/* LRZ registers */
#define A5XX_GRAS_PERFCTR_LRZ_SEL_0         0xC98
#define A5XX_GRAS_PERFCTR_LRZ_SEL_1         0xC99
#define A5XX_GRAS_PERFCTR_LRZ_SEL_2         0xC9A
#define A5XX_GRAS_PERFCTR_LRZ_SEL_3         0xC9B


/* RB registers */
#define A5XX_RB_DBG_ECO_CNT                 0xCC4
#define A5XX_RB_ADDR_MODE_CNTL              0xCC5
#define A5XX_RB_MODE_CNTL                   0xCC6
#define A5XX_RB_PERFCTR_RB_SEL_0            0xCD0
#define A5XX_RB_PERFCTR_RB_SEL_1            0xCD1
#define A5XX_RB_PERFCTR_RB_SEL_2            0xCD2
#define A5XX_RB_PERFCTR_RB_SEL_3            0xCD3
#define A5XX_RB_PERFCTR_RB_SEL_4            0xCD4
#define A5XX_RB_PERFCTR_RB_SEL_5            0xCD5
#define A5XX_RB_PERFCTR_RB_SEL_6            0xCD6
#define A5XX_RB_PERFCTR_RB_SEL_7            0xCD7

/* CCU registers */
#define A5XX_RB_PERFCTR_CCU_SEL_0           0xCD8
#define A5XX_RB_PERFCTR_CCU_SEL_1           0xCD9
#define A5XX_RB_PERFCTR_CCU_SEL_2           0xCDA
#define A5XX_RB_PERFCTR_CCU_SEL_3           0xCDB

/* RB Power Counter RB Registers Select */
#define A5XX_RB_POWERCTR_RB_SEL_0           0xCE0
#define A5XX_RB_POWERCTR_RB_SEL_1           0xCE1
#define A5XX_RB_POWERCTR_RB_SEL_2           0xCE2
#define A5XX_RB_POWERCTR_RB_SEL_3           0xCE3

/* RB Power Counter CCU Registers Select */
#define A5XX_RB_POWERCTR_CCU_SEL_0          0xCE4
#define A5XX_RB_POWERCTR_CCU_SEL_1          0xCE5

/* CMP registers */
#define A5XX_RB_PERFCTR_CMP_SEL_0           0xCEC
#define A5XX_RB_PERFCTR_CMP_SEL_1           0xCED
#define A5XX_RB_PERFCTR_CMP_SEL_2           0xCEE
#define A5XX_RB_PERFCTR_CMP_SEL_3           0xCEF

/* PC registers */
#define A5XX_PC_DBG_ECO_CNTL                0xD00
#define A5XX_PC_ADDR_MODE_CNTL              0xD01
#define A5XX_PC_PERFCTR_PC_SEL_0            0xD10
#define A5XX_PC_PERFCTR_PC_SEL_1            0xD11
#define A5XX_PC_PERFCTR_PC_SEL_2            0xD12
#define A5XX_PC_PERFCTR_PC_SEL_3            0xD13
#define A5XX_PC_PERFCTR_PC_SEL_4            0xD14
#define A5XX_PC_PERFCTR_PC_SEL_5            0xD15
#define A5XX_PC_PERFCTR_PC_SEL_6            0xD16
#define A5XX_PC_PERFCTR_PC_SEL_7            0xD17

/* HLSQ registers */
#define A5XX_HLSQ_ADDR_MODE_CNTL            0xE05
#define A5XX_HLSQ_PERFCTR_HLSQ_SEL_0        0xE10
#define A5XX_HLSQ_PERFCTR_HLSQ_SEL_1        0xE11
#define A5XX_HLSQ_PERFCTR_HLSQ_SEL_2        0xE12
#define A5XX_HLSQ_PERFCTR_HLSQ_SEL_3        0xE13
#define A5XX_HLSQ_PERFCTR_HLSQ_SEL_4        0xE14
#define A5XX_HLSQ_PERFCTR_HLSQ_SEL_5        0xE15
#define A5XX_HLSQ_PERFCTR_HLSQ_SEL_6        0xE16
#define A5XX_HLSQ_PERFCTR_HLSQ_SEL_7        0xE17
#define A5XX_HLSQ_DBG_READ_SEL              0xBC00
#define A5XX_HLSQ_DBG_AHB_READ_APERTURE     0xA000

/* VFD registers */
#define A5XX_VFD_ADDR_MODE_CNTL             0xE41
#define A5XX_VFD_PERFCTR_VFD_SEL_0          0xE50
#define A5XX_VFD_PERFCTR_VFD_SEL_1          0xE51
#define A5XX_VFD_PERFCTR_VFD_SEL_2          0xE52
#define A5XX_VFD_PERFCTR_VFD_SEL_3          0xE53
#define A5XX_VFD_PERFCTR_VFD_SEL_4          0xE54
#define A5XX_VFD_PERFCTR_VFD_SEL_5          0xE55
#define A5XX_VFD_PERFCTR_VFD_SEL_6          0xE56
#define A5XX_VFD_PERFCTR_VFD_SEL_7          0xE57

/* VPC registers */
#define A5XX_VPC_ADDR_MODE_CNTL             0xE61
#define A5XX_VPC_PERFCTR_VPC_SEL_0          0xE64
#define A5XX_VPC_PERFCTR_VPC_SEL_1          0xE65
#define A5XX_VPC_PERFCTR_VPC_SEL_2          0xE66
#define A5XX_VPC_PERFCTR_VPC_SEL_3          0xE67

/* UCHE registers */
#define A5XX_UCHE_ADDR_MODE_CNTL            0xE80
#define A5XX_UCHE_WRITE_THRU_BASE_LO        0xE87
#define A5XX_UCHE_WRITE_THRU_BASE_HI        0xE88
#define A5XX_UCHE_TRAP_BASE_LO              0xE89
#define A5XX_UCHE_TRAP_BASE_HI              0xE8A
#define A5XX_UCHE_GMEM_RANGE_MIN_LO         0xE8B
#define A5XX_UCHE_GMEM_RANGE_MIN_HI         0xE8C
#define A5XX_UCHE_GMEM_RANGE_MAX_LO         0xE8D
#define A5XX_UCHE_GMEM_RANGE_MAX_HI         0xE8E
#define A5XX_UCHE_DBG_ECO_CNTL_2            0xE8F
#define A5XX_UCHE_INVALIDATE0               0xE95
#define A5XX_UCHE_CACHE_WAYS                0xE96
#define A5XX_UCHE_PERFCTR_UCHE_SEL_0        0xEA0
#define A5XX_UCHE_PERFCTR_UCHE_SEL_1        0xEA1
#define A5XX_UCHE_PERFCTR_UCHE_SEL_2        0xEA2
#define A5XX_UCHE_PERFCTR_UCHE_SEL_3        0xEA3
#define A5XX_UCHE_PERFCTR_UCHE_SEL_4        0xEA4
#define A5XX_UCHE_PERFCTR_UCHE_SEL_5        0xEA5
#define A5XX_UCHE_PERFCTR_UCHE_SEL_6        0xEA6
#define A5XX_UCHE_PERFCTR_UCHE_SEL_7        0xEA7

/* UCHE Power Counter UCHE Registers Select */
#define A5XX_UCHE_POWERCTR_UCHE_SEL_0       0xEA8
#define A5XX_UCHE_POWERCTR_UCHE_SEL_1       0xEA9
#define A5XX_UCHE_POWERCTR_UCHE_SEL_2       0xEAA
#define A5XX_UCHE_POWERCTR_UCHE_SEL_3       0xEAB

/* SP registers */
#define A5XX_SP_DBG_ECO_CNTL                0xEC0
#define A5XX_SP_ADDR_MODE_CNTL              0xEC1
#define A5XX_SP_PERFCTR_SP_SEL_0            0xED0
#define A5XX_SP_PERFCTR_SP_SEL_1            0xED1
#define A5XX_SP_PERFCTR_SP_SEL_2            0xED2
#define A5XX_SP_PERFCTR_SP_SEL_3            0xED3
#define A5XX_SP_PERFCTR_SP_SEL_4            0xED4
#define A5XX_SP_PERFCTR_SP_SEL_5            0xED5
#define A5XX_SP_PERFCTR_SP_SEL_6            0xED6
#define A5XX_SP_PERFCTR_SP_SEL_7            0xED7
#define A5XX_SP_PERFCTR_SP_SEL_8            0xED8
#define A5XX_SP_PERFCTR_SP_SEL_9            0xED9
#define A5XX_SP_PERFCTR_SP_SEL_10           0xEDA
#define A5XX_SP_PERFCTR_SP_SEL_11           0xEDB

/* SP Power Counter SP Registers Select */
#define A5XX_SP_POWERCTR_SP_SEL_0           0xEDC
#define A5XX_SP_POWERCTR_SP_SEL_1           0xEDD
#define A5XX_SP_POWERCTR_SP_SEL_2           0xEDE
#define A5XX_SP_POWERCTR_SP_SEL_3           0xEDF

/* TP registers */
#define A5XX_TPL1_ADDR_MODE_CNTL            0xF01
#define A5XX_TPL1_MODE_CNTL                 0xF02
#define A5XX_TPL1_PERFCTR_TP_SEL_0          0xF10
#define A5XX_TPL1_PERFCTR_TP_SEL_1          0xF11
#define A5XX_TPL1_PERFCTR_TP_SEL_2          0xF12
#define A5XX_TPL1_PERFCTR_TP_SEL_3          0xF13
#define A5XX_TPL1_PERFCTR_TP_SEL_4          0xF14
#define A5XX_TPL1_PERFCTR_TP_SEL_5          0xF15
#define A5XX_TPL1_PERFCTR_TP_SEL_6          0xF16
#define A5XX_TPL1_PERFCTR_TP_SEL_7          0xF17

/* TP Power Counter TP Registers Select */
#define A5XX_TPL1_POWERCTR_TP_SEL_0         0xF18
#define A5XX_TPL1_POWERCTR_TP_SEL_1         0xF19
#define A5XX_TPL1_POWERCTR_TP_SEL_2         0xF1A
#define A5XX_TPL1_POWERCTR_TP_SEL_3         0xF1B

/* VBIF registers */
#define A5XX_VBIF_VERSION                       0x3000
#define A5XX_VBIF_CLKON                         0x3001
#define A5XX_VBIF_CLKON_FORCE_ON_TESTBUS_MASK   0x1
#define A5XX_VBIF_CLKON_FORCE_ON_TESTBUS_SHIFT  0x1

#define A5XX_VBIF_ROUND_ROBIN_QOS_ARB      0x3049
#define A5XX_VBIF_GATE_OFF_WRREQ_EN        0x302A

#define A5XX_VBIF_XIN_HALT_CTRL0	   0x3080
#define A5XX_VBIF_XIN_HALT_CTRL0_MASK	   0xF
#define A510_VBIF_XIN_HALT_CTRL0_MASK	   0x7
#define A5XX_VBIF_XIN_HALT_CTRL1	   0x3081

#define A5XX_VBIF_TEST_BUS_OUT_CTRL            0x3084
#define A5XX_VBIF_TEST_BUS_OUT_CTRL_EN_MASK    0x1
#define A5XX_VBIF_TEST_BUS_OUT_CTRL_EN_SHIFT   0x0

#define A5XX_VBIF_TEST_BUS1_CTRL0                0x3085
#define A5XX_VBIF_TEST_BUS1_CTRL1                0x3086
#define A5XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_MASK  0xF
#define A5XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_SHIFT 0x0

#define A5XX_VBIF_TEST_BUS2_CTRL0                   0x3087
#define A5XX_VBIF_TEST_BUS2_CTRL1                   0x3088
#define A5XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_MASK     0x1FF
#define A5XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_SHIFT    0x0

#define A5XX_VBIF_TEST_BUS_OUT             0x308c

#define A5XX_VBIF_PERF_CNT_SEL0            0x30D0
#define A5XX_VBIF_PERF_CNT_SEL1            0x30D1
#define A5XX_VBIF_PERF_CNT_SEL2            0x30D2
#define A5XX_VBIF_PERF_CNT_SEL3            0x30D3
#define A5XX_VBIF_PERF_CNT_LOW0            0x30D8
#define A5XX_VBIF_PERF_CNT_LOW1            0x30D9
#define A5XX_VBIF_PERF_CNT_LOW2            0x30DA
#define A5XX_VBIF_PERF_CNT_LOW3            0x30DB
#define A5XX_VBIF_PERF_CNT_HIGH0           0x30E0
#define A5XX_VBIF_PERF_CNT_HIGH1           0x30E1
#define A5XX_VBIF_PERF_CNT_HIGH2           0x30E2
#define A5XX_VBIF_PERF_CNT_HIGH3           0x30E3

#define A5XX_VBIF_PERF_PWR_CNT_EN0         0x3100
#define A5XX_VBIF_PERF_PWR_CNT_EN1         0x3101
#define A5XX_VBIF_PERF_PWR_CNT_EN2         0x3102

#define A5XX_VBIF_PERF_PWR_CNT_LOW0        0x3110
#define A5XX_VBIF_PERF_PWR_CNT_LOW1        0x3111
#define A5XX_VBIF_PERF_PWR_CNT_LOW2        0x3112

#define A5XX_VBIF_PERF_PWR_CNT_HIGH0       0x3118
#define A5XX_VBIF_PERF_PWR_CNT_HIGH1       0x3119
#define A5XX_VBIF_PERF_PWR_CNT_HIGH2       0x311A

/* GPMU registers */
#define A5XX_GPMU_INST_RAM_BASE            0x8800
#define A5XX_GPMU_DATA_RAM_BASE            0x9800
#define A5XX_GPMU_SP_POWER_CNTL            0xA881
#define A5XX_GPMU_RBCCU_CLOCK_CNTL         0xA886
#define A5XX_GPMU_RBCCU_POWER_CNTL         0xA887
#define A5XX_GPMU_SP_PWR_CLK_STATUS        0xA88B
#define A5XX_GPMU_RBCCU_PWR_CLK_STATUS     0xA88D
#define A5XX_GPMU_PWR_COL_STAGGER_DELAY    0xA891
#define A5XX_GPMU_PWR_COL_INTER_FRAME_CTRL 0xA892
#define A5XX_GPMU_PWR_COL_INTER_FRAME_HYST 0xA893
#define A5XX_GPMU_PWR_COL_BINNING_CTRL     0xA894
#define A5XX_GPMU_CLOCK_THROTTLE_CTRL      0xA8A3
#define A5XX_GPMU_WFI_CONFIG               0xA8C1
#define A5XX_GPMU_RBBM_INTR_INFO           0xA8D6
#define A5XX_GPMU_CM3_SYSRESET             0xA8D8
#define A5XX_GPMU_GENERAL_0                0xA8E0
#define A5XX_GPMU_GENERAL_1                0xA8E1

/* COUNTABLE FOR SP PERFCOUNTER */
#define A5XX_SP_ALU_ACTIVE_CYCLES          0x1
#define A5XX_SP0_ICL1_MISSES               0x35
#define A5XX_SP_FS_CFLOW_INSTRUCTIONS      0x27

/* COUNTABLE FOR TSE PERFCOUNTER */
#define A5XX_TSE_INPUT_PRIM_NUM            0x6

/* GPMU POWER COUNTERS */
#define A5XX_SP_POWER_COUNTER_0_LO		0xA840
#define A5XX_SP_POWER_COUNTER_0_HI		0xA841
#define A5XX_SP_POWER_COUNTER_1_LO		0xA842
#define A5XX_SP_POWER_COUNTER_1_HI		0xA843
#define A5XX_SP_POWER_COUNTER_2_LO		0xA844
#define A5XX_SP_POWER_COUNTER_2_HI		0xA845
#define A5XX_SP_POWER_COUNTER_3_LO		0xA846
#define A5XX_SP_POWER_COUNTER_3_HI		0xA847

#define A5XX_TP_POWER_COUNTER_0_LO		0xA848
#define A5XX_TP_POWER_COUNTER_0_HI		0xA849
#define A5XX_TP_POWER_COUNTER_1_LO		0xA84A
#define A5XX_TP_POWER_COUNTER_1_HI		0xA84B
#define A5XX_TP_POWER_COUNTER_2_LO		0xA84C
#define A5XX_TP_POWER_COUNTER_2_HI		0xA84D
#define A5XX_TP_POWER_COUNTER_3_LO		0xA84E
#define A5XX_TP_POWER_COUNTER_3_HI		0xA84F

#define A5XX_RB_POWER_COUNTER_0_LO		0xA850
#define A5XX_RB_POWER_COUNTER_0_HI		0xA851
#define A5XX_RB_POWER_COUNTER_1_LO		0xA852
#define A5XX_RB_POWER_COUNTER_1_HI		0xA853
#define A5XX_RB_POWER_COUNTER_2_LO		0xA854
#define A5XX_RB_POWER_COUNTER_2_HI		0xA855
#define A5XX_RB_POWER_COUNTER_3_LO		0xA856
#define A5XX_RB_POWER_COUNTER_3_HI		0xA857

#define A5XX_CCU_POWER_COUNTER_0_LO		0xA858
#define A5XX_CCU_POWER_COUNTER_0_HI		0xA859
#define A5XX_CCU_POWER_COUNTER_1_LO		0xA85A
#define A5XX_CCU_POWER_COUNTER_1_HI		0xA85B

#define A5XX_UCHE_POWER_COUNTER_0_LO		0xA85C
#define A5XX_UCHE_POWER_COUNTER_0_HI		0xA85D
#define A5XX_UCHE_POWER_COUNTER_1_LO		0xA85E
#define A5XX_UCHE_POWER_COUNTER_1_HI		0xA85F
#define A5XX_UCHE_POWER_COUNTER_2_LO		0xA860
#define A5XX_UCHE_POWER_COUNTER_2_HI		0xA861
#define A5XX_UCHE_POWER_COUNTER_3_LO		0xA862
#define A5XX_UCHE_POWER_COUNTER_3_HI		0xA863

#define A5XX_CP_POWER_COUNTER_0_LO		0xA864
#define A5XX_CP_POWER_COUNTER_0_HI		0xA865
#define A5XX_CP_POWER_COUNTER_1_LO		0xA866
#define A5XX_CP_POWER_COUNTER_1_HI		0xA867
#define A5XX_CP_POWER_COUNTER_2_LO		0xA868
#define A5XX_CP_POWER_COUNTER_2_HI		0xA869
#define A5XX_CP_POWER_COUNTER_3_LO		0xA86A
#define A5XX_CP_POWER_COUNTER_3_HI		0xA86B

#define A5XX_GPMU_POWER_COUNTER_0_LO		0xA86C
#define A5XX_GPMU_POWER_COUNTER_0_HI		0xA86D
#define A5XX_GPMU_POWER_COUNTER_1_LO		0xA86E
#define A5XX_GPMU_POWER_COUNTER_1_HI		0xA86F
#define A5XX_GPMU_POWER_COUNTER_2_LO		0xA870
#define A5XX_GPMU_POWER_COUNTER_2_HI		0xA871
#define A5XX_GPMU_POWER_COUNTER_3_LO		0xA872
#define A5XX_GPMU_POWER_COUNTER_3_HI		0xA873
#define A5XX_GPMU_POWER_COUNTER_4_LO		0xA874
#define A5XX_GPMU_POWER_COUNTER_4_HI		0xA875
#define A5XX_GPMU_POWER_COUNTER_5_LO		0xA876
#define A5XX_GPMU_POWER_COUNTER_5_HI		0xA877

#define A5XX_GPMU_POWER_COUNTER_ENABLE		0xA878
#define A5XX_GPMU_ALWAYS_ON_COUNTER_LO		0xA879
#define A5XX_GPMU_ALWAYS_ON_COUNTER_HI		0xA87A
#define A5XX_GPMU_ALWAYS_ON_COUNTER_RESET	0xA87B
#define A5XX_GPMU_POWER_COUNTER_SELECT_0	0xA87C
#define A5XX_GPMU_POWER_COUNTER_SELECT_1	0xA87D

#define A5XX_GPMU_CLOCK_THROTTLE_CTRL		0xA8A3
#define A5XX_GPMU_THROTTLE_UNMASK_FORCE_CTRL	0xA8A8

#define A5XX_GPMU_TEMP_SENSOR_ID		0xAC00
#define A5XX_GPMU_TEMP_SENSOR_CONFIG		0xAC01
#define A5XX_GPMU_DELTA_TEMP_THRESHOLD		0xAC03
#define A5XX_GPMU_TEMP_THRESHOLD_INTR_EN_MASK	0xAC06

#define A5XX_GPMU_LEAKAGE_TEMP_COEFF_0_1	0xAC40
#define A5XX_GPMU_LEAKAGE_TEMP_COEFF_2_3	0xAC41
#define A5XX_GPMU_LEAKAGE_VTG_COEFF_0_1		0xAC42
#define A5XX_GPMU_LEAKAGE_VTG_COEFF_2_3		0xAC43
#define A5XX_GPMU_BASE_LEAKAGE			0xAC46

#define A5XX_GPMU_GPMU_VOLTAGE			0xAC60
#define A5XX_GPMU_GPMU_VOLTAGE_INTR_STATUS	0xAC61
#define A5XX_GPMU_GPMU_VOLTAGE_INTR_EN_MASK	0xAC62
#define A5XX_GPMU_GPMU_PWR_THRESHOLD		0xAC80
#define A5XX_GPMU_GPMU_LLM_GLM_SLEEP_CTRL	0xACC4
#define A5XX_GPMU_GPMU_LLM_GLM_SLEEP_STATUS	0xACC5

#define A5XX_GDPM_CONFIG1			0xB80C
#define A5XX_GDPM_INT_EN			0xB80F
#define A5XX_GDPM_INT_MASK			0xB811
#define A5XX_GPMU_BEC_ENABLE			0xB9A0

#define A5XX_GPU_CS_SENSOR_GENERAL_STATUS	0xC41A
#define A5XX_GPU_CS_AMP_CALIBRATION_STATUS1_0	0xC41D
#define A5XX_GPU_CS_AMP_CALIBRATION_STATUS1_2	0xC41F
#define A5XX_GPU_CS_AMP_CALIBRATION_STATUS1_4	0xC421
#define A5XX_GPU_CS_ENABLE_REG			0xC520
#define A5XX_GPU_CS_AMP_CALIBRATION_CONTROL1	0xC557
#define A5XX_GPU_CS_AMP_CALIBRATION_DONE	0xC565
#endif /* _A5XX_REG_H */

