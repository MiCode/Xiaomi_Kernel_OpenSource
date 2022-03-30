/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SENINF_TG1_H__
#define __SENINF_TG1_H__

#define TM_CTL 0x0008
#define TM_EN_SHIFT 0
#define TM_EN_MASK (0x1 << 0)
#define TM_RST_SHIFT 1
#define TM_RST_MASK (0x1 << 1)
#define TM_FMT_SHIFT 2
#define TM_FMT_MASK (0x1 << 2)
#define TM_BIN_IMG_SWITCH_EN_SHIFT 3
#define TM_BIN_IMG_SWITCH_EN_MASK (0x1 << 3)
#define TM_PAT_SHIFT 4
#define TM_PAT_MASK (0x1f << 4)

#define TM_SIZE 0x000c
#define TM_PXL_SHIFT 0
#define TM_PXL_MASK (0xffff << 0)
#define TM_LINE_SHIFT 16
#define TM_LINE_MASK (0xffff << 16)

#define TM_CLK 0x0010
#define TM_CLK_CNT_SHIFT 0
#define TM_CLK_CNT_MASK (0xff << 0)
#define TM_CLRBAR_OFT_SHIFT 8
#define TM_CLRBAR_OFT_MASK (0x1fff << 8)
#define TM_CLRBAR_IDX_SHIFT 28
#define TM_CLRBAR_IDX_MASK (0x7 << 28)

#define TM_DUM 0x0018
#define TM_DUMMYPXL_SHIFT 0
#define TM_DUMMYPXL_MASK (0xffff << 0)
#define TM_VSYNC_SHIFT 16
#define TM_VSYNC_MASK (0xffff << 16)

#define TM_RAND_SEED 0x001c
#define TM_SEED_SHIFT 0
#define TM_SEED_MASK (0xffffffff << 0)

#define TM_RAND_CTL 0x0020
#define TM_DIFF_FRM_SHIFT 0
#define TM_DIFF_FRM_MASK (0x1 << 0)

#define TM_STAGGER_CTL 0x0024
#define STAGGER_MODE_EN_SHIFT 0
#define STAGGER_MODE_EN_MASK (0x1 << 0)
#define EXP_NUM_SHIFT 4
#define EXP_NUM_MASK (0x7 << 4)
#define EXP_ONE_VSYNC_SHIFT 8
#define EXP_ONE_VSYNC_MASK (0x1 << 8)

#define TM_STAGGER_CON0 0x0028
#define TM_EXP_DT0_SHIFT 0
#define TM_EXP_DT0_MASK (0x3f << 0)
#define TM_EXP_VSYNC_VC0_SHIFT 8
#define TM_EXP_VSYNC_VC0_MASK (0x1f << 8)
#define TM_EXP_HSYNC_VC0_SHIFT 16
#define TM_EXP_HSYNC_VC0_MASK (0x1f << 16)

#define TM_STAGGER_CON1 0x002c
#define TM_EXP_DT1_SHIFT 0
#define TM_EXP_DT1_MASK (0x3f << 0)
#define TM_EXP_VSYNC_VC1_SHIFT 8
#define TM_EXP_VSYNC_VC1_MASK (0x1f << 8)
#define TM_EXP_HSYNC_VC1_SHIFT 16
#define TM_EXP_HSYNC_VC1_MASK (0x1f << 16)

#define TM_STAGGER_CON2 0x0030
#define TM_EXP_DT2_SHIFT 0
#define TM_EXP_DT2_MASK (0x3f << 0)
#define TM_EXP_VSYNC_VC2_SHIFT 8
#define TM_EXP_VSYNC_VC2_MASK (0x1f << 8)
#define TM_EXP_HSYNC_VC2_SHIFT 16
#define TM_EXP_HSYNC_VC2_MASK (0x1f << 16)

#define TM_STAGGER_CON3 0x0034
#define TM_EXP_DT3_SHIFT 0
#define TM_EXP_DT3_MASK (0x3f << 0)
#define TM_EXP_VSYNC_VC3_SHIFT 8
#define TM_EXP_VSYNC_VC3_MASK (0x1f << 8)
#define TM_EXP_HSYNC_VC3_SHIFT 16
#define TM_EXP_HSYNC_VC3_MASK (0x1f << 16)

#endif
