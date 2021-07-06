/*
 * Copyright (C) 2016 MediaTek Inc.
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
#ifndef __MTK_IR_REGS_H__
#define __MTK_IR_REGS_H__
#include <linux/types.h>
#include <asm/memory.h>

#define IRRX_CLK_FREQUENCE    (32 * 1000)

/**************************************************
 ****************IRRX register define******************
 **************************************************/
#define IRRX_COUNT_HIGH_REG         0x0000
#define IRRX_CH_BITCNT_MASK         0x0000003f
#define IRRX_CH_BITCNT_BITSFT       0
#define IRRX_CH_1ST_PULSE_MASK      0x0000ff00
#define IRRX_CH_1ST_PULSE_BITSFT    8
#define IRRX_CH_2ND_PULSE_MASK      0x00ff0000
#define IRRX_CH_2ND_PULSE_BITSFT    16
#define IRRX_CH_3RD_PULSE_MASK      0xff000000
#define IRRX_CH_3RD_PULSE_BITSFT    24

#define IRRX_COUNT_MID_REG          0x0004
#define IRRX_COUNT_LOW_REG          0x0008

#define IRRX_CONFIG_HIGH_REG        0x000c
#define IRRX_WAKE_STATUS            ((u32)(0x1 << 27))
#define IRRX_INT_STATUS             ((u32)(0x1 << 26))
#define IRRX_SM_STATUS_MASK         ((u32)(0x7 << 23))
#define IRRX_SM_STATUS_OFFSET       ((u32)(23))
#define IRRX_CH_IGB0                ((u32)(0x1 << 14))
#define IRRX_CH_CHKEN               ((u32)(0x1 << 13))
#define IRRX_CH_DISCH               ((u32)(0x1 << 7))
#define IRRX_CH_DISCL               ((u32)(0x1 << 6))
#define IRRX_CH_IGSYN               ((u32)(0x1 << 5))
#define IRRX_CH_ORDINV              ((u32)(0x1 << 4))
#define IRRX_CH_RC5_1ST             ((u32)(0x1 << 3))
#define IRRX_CH_RC5                 ((u32)(0x1 << 2))
#define IRRX_CH_IRI                 ((u32)(0x1 << 1))
#define IRRX_CH_HWIR                ((u32)(0x1 << 0))

#define IRRX_CH_END_7               ((u32)(0x07 << 16))
#define IRRX_CH_END_15              ((u32)(0x0f << 16))
#define IRRX_CH_END_23              ((u32)(0x17 << 16))
#define IRRX_CH_END_31              ((u32)(0x1f << 16))
#define IRRX_CH_END_39              ((u32)(0x27 << 16))
#define IRRX_CH_END_47              ((u32)(0x2f << 16))
#define IRRX_CH_END_55              ((u32)(0x07 << 16))
#define IRRX_CH_END_63              ((u32)(0x0f << 16))

#define IRRX_CONFIG_LOW_REG         0x0010
#define IRRX_CHK_PERIOD_MASK        ((u32)0x1fff << 8)
#define IRRX_CHK_PERIOD_OFFSET      ((u32)8)
#define IRRX_SAPERIOD_MASK          ((u32)0xff << 0)
#define IRRX_SAPERIOD_OFFSET        ((u32)0)

#define IRRX_THRESHOLD_REG          0x0014
#define IRRX_PRDY_SEL               ((u32)0x1 << 16)
#define IRRX_DGDEL_MASK             ((u32)0x1f << 8)
#define IRRX_DGDEL_OFFSET           ((u32)8)
#define IRRX_ICLR_MASK              ((u32)0x1 << 7)
#define IRRX_ICLR_OFFSET            ((u32)7)
#define IRRX_THRESHOLD_MASK         ((u32)0x7f << 0)
#define IRRX_THRESHOLD_OFFSET       ((u32)0)

#define IRRX_RCMM_THD_REG           0x0018
#define IRRX_RCMM_ENABLE_MASK       ((u32)0x1 << 31)
#define IRRX_RCMM_ENABLE_OFFSET     ((u32)31)
#define IRRX_RCMM_THD_11_MASK       ((u32)0x7f << 21)
#define IRRX_RCMM_THD_11_OFFSET     ((u32)21)
#define IRRX_RCMM_THD_10_MASK       ((u32)0x7f << 14)
#define IRRX_RCMM_THD_10_OFFSET     ((u32)14)
#define IRRX_RCMM_THD_01_MASK       ((u32)0x7f << 7)
#define IRRX_RCMM_THD_01_OFFSET     ((u32)7)
#define IRRX_RCMM_THD_00_MASK       ((u32)0x7f << 0)
#define IRRX_RCMM_THD_00_OFFSET     ((u32)0)

#define IRRX_RCMM_THD_REG0          0x001c
#define IRRX_RCMM_THD_21_MASK       ((u32)0x7f << 7)
#define IRRX_RCMM_THD_21_OFFSET     ((u32)7)
#define IRRX_RCMM_THD_20_MASK       ((u32)0x7f << 0)
#define IRRX_RCMM_THD_20_OFFSET     ((u32)0)

#define IRRX_IRCLR                  0x0020
#define IRRX_IRCLR_MASK             ((u32)0x1 << 0)
#define IRRX_IRCLR_OFFSET           ((u32)0)

#define IRRX_IREXPEN                0x0024
#define IRRX_BCEPEN_MASK            ((u32)(0x1 << 8))
#define IRRX_BCEPEN_OFFSET          ((u32)8)
#define IRRX_IREXPEN_MASK           ((u32)(0xff << 0))
#define IRRX_IREXPEN_OFFSET         ((u32)0)

#define IRRX_EXPBCNT                0x0028
#define IRRX_IRCHK_CNT              ((u32)0x7f)
#define IRRX_IRCHK_CNT_OFFSET       ((u32)6)
#define IRRX_EXP_BITCNT_MASK        ((u32)0x3f)
#define IRRX_EXP_BITCNT_OFFSET      ((u32)0)

#define IRRX_ENEXP_IRM              0x002C
#define IRRX_ENEXP_IRL              0x0030

#define IRRX_EXP_IRL0               0x0034
#define IRRX_EXP_IRL1               0x0038
#define IRRX_EXP_IRL2               0x003C
#define IRRX_EXP_IRL3               0x0040
#define IRRX_EXP_IRL4               0x0044
#define IRRX_EXP_IRL5               0x0048
#define IRRX_EXP_IRL6               0x004C
#define IRRX_EXP_IRL7               0x0050
#define IRRX_EXP_IRL8               0x0054
#define IRRX_EXP_IRL9               0x0058

#define IRRX_EXP_IRM0               0x005C
#define IRRX_EXP_IRM1               0x0060
#define IRRX_EXP_IRM2               0x0064
#define IRRX_EXP_IRM3               0x0068
#define IRRX_EXP_IRM4               0x006C
#define IRRX_EXP_IRM5               0x0070
#define IRRX_EXP_IRM6               0x0074
#define IRRX_EXP_IRM7               0x0078
#define IRRX_EXP_IRM8               0x007C
#define IRRX_EXP_IRM9               0x0080

#define IRRX_IRINT_EN               0x0084
#define IRRX_INTEN_MASK             ((u32)0x1 << 0)
#define IRRX_INTEN_OFFSET           ((u32)0)

#define IRRX_IRINT_CLR              0x0088
#define IRRX_INTCLR_MASK            ((u32)0x1 << 0)
#define IRRX_INTCLR_OFFSET          ((u32)0)

#define IRRX_WAKEEN                 0x008C
#define IRRX_WAKEEN_MASK            ((u32)0x1 << 0)
#define IRRX_WAKEEN_OFFSET          ((u32)0)

#define IRRX_WAKECLR                0x0090
#define IRRX_WAKECLR_MASK           ((u32)0x1 << 0)
#define IRRX_WAKECLR_OFFSET         ((u32)0)

#define IRRX_SOFTEN                 0x0098
#define IRRX_SOFTEN_MASK            ((u32)0x1 << 0)
#define IRRX_SOFTEN_OFFSET          ((u32)0)

#define IRRX_SELECT                 0x009C
#define IRRX_INT_SELECT_MASK        ((u32)0xf << 0)
#define IRRX_INT_SELECT_OFFSET      ((u32)0)
#define IRRX_AP_WAKEUP_EN           ((u32)0x1 << 3)
#define IRRX_AP_INT_EN              ((u32)0x1 << 2)
#define IRRX_CM4_WAKEUP_EN          ((u32)0x1 << 1)
#define IRRX_CM4_INT_EN             ((u32)0x1 << 0)

#define IRRX_CHKDATA0               0x00A0
#define IRRX_CHKDATA1               0x00A4
#define IRRX_CHKDATA2               0x00A8
#define IRRX_CHKDATA3               0x00AC
#define IRRX_CHKDATA4               0x00B0
#define IRRX_CHKDATA5               0x00B4
#define IRRX_CHKDATA6               0x00B8
#define IRRX_CHKDATA7               0x00BC
#define IRRX_CHKDATA8               0x00C0
#define IRRX_CHKDATA9               0x00C4
#define IRRX_CHKDATA10              0x00C8
#define IRRX_CHKDATA11              0x00CC
#define IRRX_CHKDATA12              0x00D0
#define IRRX_CHKDATA13              0x00D4
#define IRRX_CHKDATA14              0x00D8
#define IRRX_CHKDATA15              0x00DC
#define IRRX_CHKDATA16              0x00E0
#define IRRX_CHKDATA17              0x00E4
#define IRRX_CHKDATA18              0x00E8
#define IRRX_CHKDATA19              0x00EC
#define IRRX_CHKDATA20              0x00F0
#define IRRX_CHKDATA21              0x00F4
#define IRRX_CHKDATA22              0x00F8
#define IRRX_CHKDATA23              0x00FC
#define IRRX_CHKDATA24              0x00100
#define IRRX_CHKDATA25              0x00104
#define IRRX_CHKDATA26              0x00108
#define IRRX_CHKDATA27              0x0010C
#define IRRX_CHKDATA28              0x00110
#define IRRX_CHKDATA29              0x00114
#define IRRX_CHKDATA30              0x00118
#define IRRX_CHKDATA31              0x0011C

#endif
