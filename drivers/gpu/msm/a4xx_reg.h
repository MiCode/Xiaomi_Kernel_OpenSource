/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef _A4XX_REG_H
#define _A4XX_REG_H

/* RB registers */
#define A4XX_RB_GMEM_BASE_ADDR		0xcc0

/* RBBM registers */
#define A4XX_RBBM_AHB_CMD		0x25
#define A4XX_RBBM_SP_HYST_CNT		0x21
#define A4XX_RBBM_AHB_CTL0		0x23
#define A4XX_RBBM_AHB_CTL1		0x24
#define A4XX_RBBM_WAIT_IDLE_CLOCKS_CTL	0x2b
#define A4XX_RBBM_INTERFACE_HANG_INT_CTL	0x2f
#define A4XX_RBBM_AHB_ERROR_STATUS	0x18f
#define A4XX_RBBM_STATUS		0x191
#define A4XX_RBBM_INT_CLEAR_CMD		0x36
#define A4XX_RBBM_INT_0_MASK		0x37
#define A4XX_RBBM_INT_0_STATUS		0x17d
#define A4XX_RBBM_PERFCTR_CTL		0x170
#define A4XX_RBBM_PERFCTR_LOAD_CMD0	0x171
#define A4XX_RBBM_PERFCTR_LOAD_CMD1	0x172
#define A4XX_RBBM_PERFCTR_LOAD_CMD2	0x173
#define A4XX_RBBM_GPU_BUSY_MASKED	0x17a
#define A4XX_RBBM_PERFCTR_PWR_1_LO	0x168

/* CP registers */
#define A4XX_CP_SCRATCH_REG0		0x578
#define A4XX_CP_SCRATCH_UMASK		0x228
#define A4XX_CP_SCRATCH_ADDR		0x229
#define A4XX_CP_RB_BASE			0x200
#define A4XX_CP_RB_CNTL			0x201
#define A4XX_CP_RB_WPTR			0x205
#define A4XX_CP_RB_RPTR_ADDR		0x203
#define A4XX_CP_RB_RPTR			0x204
#define A4XX_CP_IB1_BASE		0x206
#define A4XX_CP_IB1_BUFSZ		0x207
#define A4XX_CP_IB2_BASE		0x208
#define A4XX_CP_IB2_BUFSZ		0x209
#define A4XX_CP_WFI_PEND_CTR		0x4d2
#define A4XX_CP_ME_CNTL			0x22d
#define A4XX_CP_ME_RAM_WADDR		0x225
#define A4XX_CP_ME_RAM_RADDR		0x226
#define A4XX_CP_ME_RAM_DATA		0x227
#define A4XX_CP_PFP_UCODE_ADDR		0x223
#define A4XX_CP_PFP_UCODE_DATA		0x224
#define A4XX_CP_PROTECT_CTRL		0x250
#define A4XX_CP_DEBUG			0x22e

/* SP registers */
#define A4XX_SP_VS_OBJ_START		0x22e1
#define A4XX_SP_VS_PVT_MEM_ADDR		0x22e3
#define A4XX_SP_FS_OBJ_START		0x22eb
#define A4XX_SP_FS_PVT_MEM_ADDR		0x22ed

/* VPC registers */
#define A4XX_VPC_DEBUG_RAM_SEL		0xe60
#define A4XX_VPC_DEBUG_RAM_READ		0xe61

/* VSC registers */
#define A4XX_VSC_SIZE_ADDRESS		0xc01
#define A4XX_VSC_PIPE_DATA_ADDRESS_0	0xc10
#define A4XX_VSC_PIPE_DATA_LENGTH_7	0xc1f

/* VFD registers */
#define A4XX_VFD_CONTROL_0		0x2200
#define A4XX_VFD_FETCH_INSTR_0_0	0x220a
#define A4XX_VFD_FETCH_INSTR_1_31	0x2287

#endif /* _A400_REG_H */
