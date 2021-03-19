/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_HW_CMDE_V2_0_H__
#define __MDLA_HW_CMDE_V2_0_H__

#define MREG_TOP_G_DBG_CMDE01           (0x0004)
#define MREG_TOP_G_DBG_CMDE28           (0x0088)
#define MREG_TOP_G_DBG_CMDE29           (0x008c)

#define MREG_TOP_G_REV                  (0x0500)
#define MREG_TOP_G_INTP0                (0x0504)
#define MREG_TOP_G_INTP1                (0x0508)
#define MREG_TOP_G_INTP2                (0x050C)
#define MREG_TOP_G_CDMA0                (0x0510)
#define MREG_TOP_G_CDMA1                (0x0514)
#define MREG_TOP_G_CDMA2                (0x0518)
#define MREG_TOP_G_CDMA3                (0x051C)
#define MREG_TOP_G_CDMA4                (0x0520)
#define MREG_TOP_G_CDMA5                (0x0524)
#define MREG_TOP_G_CDMA6                (0x0528)
#define MREG_TOP_G_CUR0                 (0x052C)
#define MREG_TOP_G_CUR1                 (0x0530)
#define MREG_TOP_G_FIN0                 (0x0534)
#define MREG_TOP_G_FIN1                 (0x0538)
#define MREG_TOP_G_STREAM0              (0x053C)
#define MREG_TOP_G_STREAM1              (0x0540)
#define MREG_TOP_G_IDLE                 (0x0544)
#define MREG_TOP_G_DBG_STREAM0          (0x0548)
#define MREG_TOP_G_DBG_STREAM1          (0x054C)
#define MREG_TOP_ENG0                   (0x0550)
#define MREG_TOP_ENG1                   (0x0554)
#define MREG_TOP_ENG2                   (0x0558)
#define MREG_TOP_ENG3                   (0x055C)
#define MREG_TOP_ENG4                   (0x0560)
#define MREG_TOP_ENG5                   (0x0564)
#define MREG_TOP_ENG6                   (0x0568)
#define MREG_TOP_ENG7                   (0x056C)
#define MREG_TOP_ENG8                   (0x0570)
#define MREG_TOP_ENG9                   (0x0574)
#define MREG_TOP_ENG10                  (0x0578)
#define MREG_TOP_ENG11                  (0x057C)
#define MREG_TOP_ENG12                  (0x0580)
#define MREG_TOP_G_FIN3                 (0x0584)
#define MREG_TOP_G_COREINFO             (0x0588)
#define MREG_TOP_G_FIN4                 (0x058C)
#define MREG_TOP_G_MDLA_HWSYNC0         (0x0590)
#define MREG_TOP_G_MDLA_HWSYNC1         (0x0594)
#define MREG_TOP_G_MDLA_HWSYNC2         (0x0598)
#define MREG_TOP_G_MDLA_HWSYNC3         (0x059C)
#define MREG_TOP_ENG13                  (0x05A0)


/* Register fields */

/* MREG_TOP_G_DBG_CMDE01 : 0x0004 */
#define LCE_HWCMD_RDCNT                 BIT(0)
#define LCE_INT_SWCMD_DONE              BIT(1)
#define LCE_WAIT_INT_SWCMD_DONE         BIT(2)
#define LCE_MDLA_EXT_SWCMD_DONE(i)      BIT((i + 3))
#define LCE_MDLA_WAIT_EXT_SWCMD_DONE(i) BIT((i + 11))
#define LCE_WAIT_LAST_ACTI              BIT(19)
#define LCE_WAIT_LAST_CFG               BIT(20)
#define LCE_LAYER_END                   BIT(21)

/* MREG_TOP_G_DBG_CMDE28/29 : 0x0088, 0x008c */
#define INT_SWCMD_DONE_CNT              GENMASK(3, 0)
#define MDLA0_SWCMD_DONE_CNT            GENMASK(7, 4)
#define MDLA1_SWCMD_DONE_CNT            GENMASK(11, 8)

/* MREG_TOP_G_DBG_CMDE29 : 0x008c */
#define MREG_TOP_G_DBG_CMDE29           (0x008c)

/* MREG_TOP_G_INTPx : 0x0500, 0x0504, 0x0508 */
#define INTR_SWCMD_TILECNT_INT               BIT(1)
#define INTR_SWCMD_DONE                      BIT(2)
#define INTR_CDMA_EVT_FAIL                   BIT(4)
#define INTR_CDMA_FIFO_EMPTY                 BIT(5)
#define INTR_CDMA_FIFO_FULL                  BIT(6)
#define INTR_PMU_INT                         BIT(9)
#define INTR_CONV_GCU_SAT_EXCEPTION_INT      BIT(10)
#define INRQ_CONV_AQU_ACC_SAT_EXCEPTION_INT  BIT(11)
#define INRQ_CONV_AQU_ADD_SAT_EXCEPTION_INT  BIT(12)
#define INTR_FIN4_CMD_STOP_INT               BIT(21)
#define MDLA_IRQ_MASK                        (0x3FFFFFFF)
#define INTR_SUPPORT_MASK \
	(INTR_SWCMD_DONE | INTR_FIN4_CMD_STOP_INT)


#endif /* __MDLA_HW_CMDE_V2_0_H__ */
