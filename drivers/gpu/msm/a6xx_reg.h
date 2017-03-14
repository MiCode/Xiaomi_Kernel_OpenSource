/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _A6XX_REG_H
#define _A6XX_REG_H

/* A6XX interrupt bits */
#define A6XX_INT_RBBM_GPU_IDLE          0
#define A6XX_INT_CP_AHB_ERROR           1
#define A6XX_INT_ATB_ASYNCFIFO_OVERFLOW 6
#define A6XX_INT_RBBM_GPC_ERROR         7
#define A6XX_INT_CP_SW                  8
#define A6XX_INT_CP_HW_ERROR            9
#define A6XX_INT_CP_CCU_FLUSH_DEPTH_TS  10
#define A6XX_INT_CP_CCU_FLUSH_COLOR_TS  11
#define A6XX_INT_CP_CCU_RESOLVE_TS      12
#define A6XX_INT_CP_IB2                 13
#define A6XX_INT_CP_IB1                 14
#define A6XX_INT_CP_RB                  15
#define A6XX_INT_CP_RB_DONE_TS          17
#define A6XX_INT_CP_WT_DONE_TS          18
#define A6XX_INT_CP_CACHE_FLUSH_TS      20
#define A6XX_INT_RBBM_ATB_BUS_OVERFLOW  22
#define A6XX_INT_RBBM_HANG_DETECT       23
#define A6XX_INT_UCHE_OOB_ACCESS        24
#define A6XX_INT_UCHE_TRAP_INTR         25
#define A6XX_INT_DEBBUS_INTR_0          26
#define A6XX_INT_DEBBUS_INTR_1          27
#define A6XX_INT_ISDB_CPU_IRQ           30
#define A6XX_INT_ISDB_UNDER_DEBUG       31

/* CP Interrupt bits */
#define A6XX_CP_OPCODE_ERROR                    0
#define A6XX_CP_UCODE_ERROR                     1
#define A6XX_CP_HW_FAULT_ERROR                  2
#define A6XX_CP_REGISTER_PROTECTION_ERROR       4
#define A6XX_CP_AHB_ERROR                       5
#define A6XX_CP_VSD_PARITY_ERROR                6
#define A6XX_CP_ILLEGAL_INSTR_ERROR             7

/* CP registers */
#define A6XX_CP_RB_BASE                  0x800
#define A6XX_CP_RB_BASE_HI               0x801
#define A6XX_CP_RB_CNTL                  0x802
#define A6XX_CP_RB_RPTR_ADDR_LO          0x804
#define A6XX_CP_RB_RPTR_ADDR_HI          0x805
#define A6XX_CP_RB_RPTR                  0x806
#define A6XX_CP_RB_WPTR                  0x807
#define A6XX_CP_SQE_CNTL                 0x808
#define A6XX_CP_HW_FAULT                 0x821
#define A6XX_CP_INTERRUPT_STATUS         0x823
#define A6XX_CP_PROTECT_STATUS           0X824
#define A6XX_CP_SQE_INSTR_BASE_LO        0x830
#define A6XX_CP_SQE_INSTR_BASE_HI        0x831
#define A6XX_CP_MISC_CNTL                0x840
#define A6XX_CP_ROQ_THRESHOLDS_1         0x8C1
#define A6XX_CP_ROQ_THRESHOLDS_2         0x8C2
#define A6XX_CP_MEM_POOL_SIZE            0x8C3
#define A6XX_CP_CHICKEN_DBG              0x841
#define A6XX_CP_ADDR_MODE_CNTL           0x842
#define A6XX_CP_PROTECT_CNTL             0x84F
#define A6XX_CP_PROTECT_REG              0x850
#define A6XX_CP_SQE_STAT_ADDR            0x908
#define A6XX_CP_SQE_STAT_DATA            0x909
#define A6XX_CP_ALWAYS_ON_COUNTER_LO     0x980
#define A6XX_CP_ALWAYS_ON_COUNTER_HI     0x981
#define A6XX_CP_AHB_CNTL                 0x98D
#define A6XX_VSC_ADDR_MODE_CNTL          0xC01

/* RBBM registers */
#define A6XX_RBBM_VBIF_CLIENT_QOS_CNTL           0x10
#define A6XX_RBBM_INTERFACE_HANG_INT_CNTL        0x1f
#define A6XX_RBBM_INT_CLEAR_CMD                  0x37
#define A6XX_RBBM_INT_0_MASK                     0x38
#define A6XX_RBBM_SW_RESET_CMD                   0x43
#define A6XX_RBBM_BLOCK_SW_RESET_CMD             0x45
#define A6XX_RBBM_BLOCK_SW_RESET_CMD2            0x46
#define A6XX_RBBM_CLOCK_CNTL                     0xAE
#define A6XX_RBBM_INT_0_STATUS                   0x201
#define A6XX_RBBM_STATUS                         0x210
#define A6XX_RBBM_STATUS3                        0x213
#define A6XX_RBBM_SECVID_TRUST_CNTL              0xF400
#define A6XX_RBBM_SECVID_TSB_ADDR_MODE_CNTL      0xF810

/* VSC registers */
#define A6XX_GRAS_ADDR_MODE_CNTL            0x8601

/* RB registers */
#define A6XX_RB_ADDR_MODE_CNTL              0x8E05
#define A6XX_RB_NC_MODE_CNTL                0x8E08

/* PC registers */
#define A6XX_PC_DBG_ECO_CNTL                0x9E00
#define A6XX_PC_ADDR_MODE_CNTL              0x9E01

/* HLSQ registers */
#define A6XX_HLSQ_ADDR_MODE_CNTL            0xBE05

/* VFD registers */
#define A6XX_VFD_ADDR_MODE_CNTL             0xA601

/* VPC registers */
#define A6XX_VPC_ADDR_MODE_CNTL             0x9601

/* UCHE registers */
#define A6XX_UCHE_ADDR_MODE_CNTL            0xE00
#define A6XX_UCHE_MODE_CNTL                 0xE01
#define A6XX_UCHE_WRITE_RANGE_MAX_LO        0xE05
#define A6XX_UCHE_WRITE_RANGE_MAX_HI        0xE06
#define A6XX_UCHE_WRITE_THRU_BASE_LO        0xE07
#define A6XX_UCHE_WRITE_THRU_BASE_HI        0xE08
#define A6XX_UCHE_TRAP_BASE_LO              0xE09
#define A6XX_UCHE_TRAP_BASE_HI              0xE0A
#define A6XX_UCHE_GMEM_RANGE_MIN_LO         0xE0B
#define A6XX_UCHE_GMEM_RANGE_MIN_HI         0xE0C
#define A6XX_UCHE_GMEM_RANGE_MAX_LO         0xE0D
#define A6XX_UCHE_GMEM_RANGE_MAX_HI         0xE0E
#define A6XX_UCHE_CACHE_WAYS                0xE17
#define A6XX_UCHE_FILTER_CNTL               0xE18

/* SP registers */
#define A6XX_SP_ADDR_MODE_CNTL              0xAE01
#define A6XX_SP_NC_MODE_CNTL                0xAE02

/* TP registers */
#define A6XX_TPL1_ADDR_MODE_CNTL            0xB601
#define A6XX_TPL1_NC_MODE_CNTL              0xB604

/* VBIF registers */
#define A6XX_VBIF_VERSION                       0x3000
#define A6XX_VBIF_GATE_OFF_WRREQ_EN             0x302A
#define A6XX_VBIF_XIN_HALT_CTRL0                0x3080
#define A6XX_VBIF_XIN_HALT_CTRL1                0x3081

/* GMU registers */
#define A6XX_GMU_CX_ALWAYS_ON_COUNTER_L         0x1f888
#define A6XX_GMU_CX_ALWAYS_ON_COUNTER_H         0x1f889

#endif /* _A6XX_REG_H */

