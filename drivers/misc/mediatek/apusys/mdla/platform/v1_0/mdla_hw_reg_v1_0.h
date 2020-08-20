/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MDLA_V1_0_HW_REG_H__
#define __MDLA_V1_0_HW_REG_H__


#define APU2_IRQ_ID        (321+32)
#define MDLA_IRQ_SWCMD_TILECNT_INT (1 << 1)
#define MDLA_IRQ_TILECNT_DONE      (1 << 1)
#define MDLA_IRQ_SWCMD_DONE        (1 << 2)
#define MDLA_IRQ_PMU_INTE  (1 << 9)
#define MDLA_IRQ_MASK      (0x1FFFFF)
#define MDLA_AXI_CTRL_MASK ((1 << 7) | (1 << 16))

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

/* MDLA config */
#define MDLA_CG_CON        (0x000)
#define MDLA_CG_SET        (0x004)
#define MDLA_CG_CLR        (0x008)
#define MDLA_SW_RST        (0x00C)
#define MDLA_MBIST_MODE0   (0x010)
#define MDLA_MBIST_MODE1   (0x014)
#define MDLA_MBIST_CTL     (0x018)
#define MDLA_RP_OK0        (0x01C)
#define MDLA_RP_OK1        (0x020)
#define MDLA_RP_OK2        (0x024)
#define MDLA_RP_OK3        (0x028)
#define MDLA_RP_FAIL0      (0x02C)
#define MDLA_RP_FAIL1      (0x030)
#define MDLA_RP_FAIL2      (0x034)
#define MDLA_RP_FAIL3      (0x038)
#define MDLA_MBIST_FAIL0   (0x03C)
#define MDLA_MBIST_FAIL1   (0x040)
#define MDLA_MBIST_FAIL2   (0x044)
#define MDLA_MBIST_FAIL3   (0x048)
#define MDLA_MBIST_FAIL4   (0x04C)
#define MDLA_MBIST_FAIL5   (0x050)
#define MDLA_MBIST_DONE0   (0x054)
#define MDLA_MBIST_DONE1   (0x058)
#define MDLA_MBIST_DEFAULT_DELSEL (0x05C)
#define MDLA_SRAM_DELSEL0  (0x060)
#define MDLA_SRAM_DELSEL1  (0x064)
#define MDLA_RP_RST        (0x068)
#define MDLA_RP_CON        (0x06C)
#define MDLA_RP_PRE_FUSE   (0x070)
#define MDLA_SPARE_0       (0x074)
#define MDLA_SPARE_1       (0x078)
#define MDLA_SPARE_2       (0x07C)
#define MDLA_SPARE_3       (0x080)
#define MDLA_SPARE_4       (0x084)
#define MDLA_SPARE_5       (0x088)
#define MDLA_SPARE_6       (0x08C)
#define MDLA_SPARE_7       (0x090)

#define MDLA_AXI_CTRL      (0x120)
#define MDLA_AXI1_CTRL     (0x124)

/* MDLA command */
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

#define MREG_CMD_SIZE      (0x180)
//#define MREG_CMD_SWCMD_ID  (0x150)
//#define MREG_CMD_EXE_FLOW  (0x158)
//#define MREG_CMD_CBL_FUNC  (0x0AC)
//#define MREG_CMD_SBL_FUNC  (0x11C)
//#define MREG_CMD_CONV_FUNC (0x0BC)
//#define MREG_CMD_ELW_FUNC  (0x0CC)
//#define MREG_CMD_ACTI_FUNC (0x0E0)
//#define MREG_CMD_POOL_FUNC_0 (0x0F8)
//#define MREG_CMD_STE_FUNC  (0x118)

/* MDLA PMU */

#define CFG_PMCR_DEFAULT   (0x1F021)
#define PMU_CNT_SHIFT      (0x0010)
#define PMU_CLR_CMDE_SHIFT (0x5)

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

#endif /* __MDLA_V1_0_HW_REG_H__ */
