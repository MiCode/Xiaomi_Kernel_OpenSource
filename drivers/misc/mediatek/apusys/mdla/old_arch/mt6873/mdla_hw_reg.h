// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MDLA_HW_REG_H_
#define _MDLA_HW_REG_H_

#define MDLA_IRQ_SWCMD_TILECNT_INT (1 << 1)
#define MDLA_IRQ_TILECNT_DONE      (1 << 1)
#define MDLA_IRQ_SWCMD_DONE        (1 << 2)
#define MDLA_IRQ_CDMA_FIFO_EMPTY   (1 << 5)
#define MDLA_IRQ_PMU_INTE  (1 << 9)
#define MDLA_IRQ_MASK      (0x1FFFFF)
#define MDLA_AXI_CTRL_MASK ((1 << 1) | (1 << 10))
#define MDLA_SW_RST_SETETING (0x3F)

#if 0
/* Infra TOPAXI */
#define INFRA_TOPAXI_PROTECTEN_MCU_SET  (0x2C4)
#define INFRA_TOPAXI_PROTECTEN_MCU_CLR  (0x2C8)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA0	(0x2E0)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA1	(0x2E4)
#define VPU_CORE2_PROT_STEP1_0_MASK \
	((0x1 << 8) | (0x1 << 9) | (0x1 << 10))
#define VPU_CORE2_PROT_STEP1_0_ACK_MASK \
	((0x1 << 8) | (0x1 << 9) | (0x1 << 10))


/* APU CONN */
#define APU_CONN_SW_RST    (0x00C)
#define APU_CORE2_RSTB     (1 << 15)
#endif

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
#define MREG_CMD_EXE_FLOW  (0x158)//need refine for 6885
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
#define PMU_CNT_SHIFT      (0x0010)
#define PMU_CLR_CMDE_SHIFT (0x5)

#define PMU_PMCR_CCNT_EN   (0x10000)
#define PMU_PMCR_CCNT_RST  (0x4)
#define PMU_PMCR_CNT_RST   (0x2)

#define PMU_CFG_PMCR       (0x0E00)
#define PMU_CYCLE          (0x0E04)
#define PMU_START_TSTAMP   (0x0E08)
#define PMU_END_TSTAMP     (0x0E0C)
#define PMU_EVENT_OFFSET   (0x0E10)
#define PMU_CNT_OFFSET     (0x0E14)
#define PMU_CMDID_LATCH    (0x0F00)

/* MDLA Debug Reg*/
#define MREG_IT_FRONT_C_INVALID  (0x0D74)
#define MREG_DEBUG_IF_0  (0x0DB0)
#define MREG_DEBUG_IF_2  (0x0DB8)

#endif
