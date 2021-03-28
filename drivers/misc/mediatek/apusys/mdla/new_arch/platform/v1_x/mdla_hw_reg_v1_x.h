/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MDLA_V1_X_HW_REG_H__
#define _MDLA_V1_X_HW_REG_H__

#define MDLA_IRQ_SWCMD_TILECNT_INT BIT(1)
#define MDLA_IRQ_TILECNT_DONE      BIT(1)
#define MDLA_IRQ_SWCMD_DONE        BIT(2)
#define MDLA_IRQ_CDMA_FIFO_EMPTY   BIT(5)
#define MDLA_IRQ_PMU_INTE          BIT(9)
#define MDLA_IRQ_MASK              (0x1FFFFF)
#define MDLA_AXI_CTRL_MASK         (BIT(1) | BIT(10))
#define MDLA_SW_RST_SETETING       (0x3F)

/* MDLA config */
#define MDLA_CG_CON		(0x000)
#define MDLA_CG_SET		(0x004)
#define MDLA_CG_CLR		(0x008)
#define MDLA_SW_RST		(0x00C)
#define MDLA_MBIST_MODE0	(0x010)
#define MDLA_MBIST_MODE1	(0x014)
#define MDLA_MBIST_CTL		(0x018)
#define MDLA_AXI_BIST_CTL	(0x01C)
#define MDLA_AXI_BIST_STA	(0x020)
#define MDLA_MBIST_DEFAULT_DELSEL	(0x024)
#define MDLA_RP_RST		(0x060)
#define MDLA_RP_CON		(0x064)
#define MDLA_RP_PRE_FUSE_0	(0x068)
#define MDLA_RP_PRE_FUSE_1	(0x06C)
#define MDLA_RP_PRE_FUSE_2	(0x070)
#define MDLA_RP_PRE_FUSE_3	(0x074)
#define MDLA_RP_PRE_FUSE_4	(0x078)
#define MDLA_SPARE_0		(0x07C)
#define MDLA_SPARE_1		(0x080)
#define MDLA_SPARE_2		(0x084)
#define MDLA_SPARE_3		(0x088)
#define MDLA_SPARE_4		(0x08C)
#define MDLA_SPARE_5		(0x090)
#define MDLA_SPARE_6		(0x094)
#define MDLA_SPARE_7		(0x098)
#define MDLA_CTRL		(0x110)
#define MDLA_CSYSREQ		(0x114)
#define MDLA_CSYSACK		(0x118)
#define MDLA_AXI_CTRL		(0x120)
#define MDLA_AXI1_CTRL		(0x124)
#define MDLA_APB_CTRL		(0x128)
#define MDLA_APB_STA		(0x12C)
#define MDLA_GALS_M0_CTRL	(0x130)
#define MDLA_GALS_M1_CTRL	(0x134)
#define MDLA_GALS_STA		(0x138)
#define MDLA_AXI_GALS_DBG	(0x13C)
#define MDLA_SES_CTL0		(0x140)
#define MDLA_SES_CTL1		(0x144)
#define MDLA_SES_CTL2		(0x148)

/* MDLA command */
#define MREG_TOP_SWCMD_DONE_CNT	(0x008C)

#define MREG_TOP_G_REV     (0x0500)
#define MREG_TOP_G_INTP0   (0x0504)
#define MREG_TOP_G_INTP1   (0x0508)
#define MREG_TOP_G_INTP2   (0x050C)
#define MREG_TOP_G_CDMA0   (0x0510)
#define MREG_TOP_G_CDMA1   (0x0514)
#define MREG_TOP_G_CDMA2   (0x0518)
#define MREG_TOP_G_CDMA3   (0x051C)
#define MREG_TOP_G_CDMA4   (0x0520)
#define MREG_TOP_G_CDMA5   (0x0524)
#define MREG_TOP_G_CDMA6   (0x0528)
#define MREG_TOP_G_CUR0    (0x052C)
#define MREG_TOP_G_CUR1    (0x0530)
#define MREG_TOP_G_FIN0    (0x0534)
#define MREG_TOP_G_FIN1    (0x0538)
#define MREG_TOP_G_STREAM0 (0x053C)
#define MREG_TOP_G_STREAM1 (0x0540)
#define MREG_TOP_G_IDLE    (0x0544)

#define MREG_TOP_ENG0      (0x0550)
#define MREG_TOP_ENG1      (0x0554)
#define MREG_TOP_ENG2      (0x0558)
#define MREG_TOP_ENG11     (0x057C)
#define MREG_TOP_G_FIN3     (0x0584)
#define MREG_TOP_G_COREINFO     (0x0588)

#define MREG_CMD_GENERAL_CTRL_0 (0x70)
#define MREG_CMD_SIZE      (0x1C0)
#define MREG_CMD_SWCMD_ID  (0x150)
#define MREG_CMD_TILE_CNT_INT  (0x154)
#define MREG_CMD_GENERAL_CTRL_1  (0x15C)

#define MSK_MREG_TOP_G_STREAM0_PROD_CMD_ID GENMASK(31, 0)
#define MSK_MREG_TOP_G_STREAM1_HALT_EN BIT(31)
#define MSK_MREG_TOP_G_STREAM1_PROD_TILE_ID GENMASK(23, 0)

#define MSK_MREG_CMD_SWCMD_FINISH_INT_EN GENMASK(24, 24)
#define MSK_MREG_CMD_LAYER_END GENMASK(12, 12)
#define MSK_MREG_CMD_SWCMD_WAIT_SWCMDDONE GENMASK(16, 16)
#define MSK_MREG_CMD_SWCMD_INT_SWCMDDONE GENMASK(15, 15)

/* MDLA PMU */
#define CFG_PMCR_DEFAULT   (0x1F021)

#define CFG_PMCR          (0x0E00)
#define PMU_CLK_CNT       (0x0E04)
#define PMU_START_TSTAMP  (0x0E08)
#define PMU_END_TSTAMP    (0x0E0C)
#define PMU1              (0x0E10)
#define PMU1_CNT          (0x0E14)
#define PMU1_CNT_LATCH    (0x0E18)
#define PMU_CMDID_LATCH   (0x0F00)

/* id: 0 ~ 14 */
#define PMU(id)           (0x10 * (id) + PMU1)
#define PMU_CNT(id)       (0x10 * (id) + PMU1_CNT)
#define PMU_CNT_LATCH(id) (0x10 * (id) + PMU1_CNT_LATCH)



/* CFG_PMCR : 0xE00 */
#define PMU_CNT_EN(i)   BIT(17 + (i))
#define PMU_CCNT_EN     BIT(16)
#define PMU_PERCMD_MODE BIT(5)
#define PMU_EXPORT      BIT(4)
#define PMU_CCNT_RESET  BIT(2)
#define PMU_CNT_RESET   BIT(1)
#define PMU_ENABLE      BIT(0)


#endif /* _MDLA_V1_X_HW_REG_H__ */
