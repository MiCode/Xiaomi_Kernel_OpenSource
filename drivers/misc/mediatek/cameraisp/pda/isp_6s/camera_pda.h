/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_PDA_HW_H__
#define __MTK_PDA_HW_H__

#include <linux/completion.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>

#include <linux/firmware.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/io.h>

#define PDA_MAGIC               'P'

void __iomem *m_pda_base;
void __iomem *CAMSYS_CONFIG_BASE;

#define PDA_BASE_HW             m_pda_base
#define PDA_CFG_0_REG (PDA_BASE_HW + 0x000)
#define PDA_CFG_1_REG (PDA_BASE_HW + 0x004)
#define PDA_CFG_2_REG (PDA_BASE_HW + 0x008)
#define PDA_CFG_3_REG (PDA_BASE_HW + 0x00c)
#define PDA_CFG_4_REG (PDA_BASE_HW + 0x010)
#define PDA_CFG_5_REG (PDA_BASE_HW + 0x014)
#define PDA_CFG_6_REG (PDA_BASE_HW + 0x018)
#define PDA_CFG_7_REG (PDA_BASE_HW + 0x01c)
#define PDA_CFG_8_REG (PDA_BASE_HW + 0x020)
#define PDA_CFG_9_REG (PDA_BASE_HW + 0x024)
#define PDA_CFG_10_REG (PDA_BASE_HW + 0x028)
#define PDA_CFG_11_REG (PDA_BASE_HW + 0x02c)
#define PDA_CFG_12_REG (PDA_BASE_HW + 0x030)
#define PDA_CFG_13_REG (PDA_BASE_HW + 0x034)
#define PDA_CFG_14_REG (PDA_BASE_HW + 0x038)
#define PDA_CFG_15_REG (PDA_BASE_HW + 0x03c)
#define PDA_CFG_16_REG (PDA_BASE_HW + 0x040)
#define PDA_CFG_17_REG (PDA_BASE_HW + 0x044)
#define PDA_CFG_18_REG (PDA_BASE_HW + 0x048)
#define PDA_CFG_19_REG (PDA_BASE_HW + 0x04c)
#define PDA_CFG_20_REG (PDA_BASE_HW + 0x050)
#define PDA_CFG_21_REG (PDA_BASE_HW + 0x054)
#define PDA_CFG_22_REG (PDA_BASE_HW + 0x058)
#define PDA_CFG_23_REG (PDA_BASE_HW + 0x05c)
#define PDA_CFG_24_REG (PDA_BASE_HW + 0x060)
#define PDA_CFG_25_REG (PDA_BASE_HW + 0x064)
#define PDA_CFG_26_REG (PDA_BASE_HW + 0x068)
#define PDA_CFG_27_REG (PDA_BASE_HW + 0x06c)
#define PDA_CFG_28_REG (PDA_BASE_HW + 0x070)
#define PDA_CFG_29_REG (PDA_BASE_HW + 0x074)
#define PDA_CFG_30_REG (PDA_BASE_HW + 0x078)
#define PDA_CFG_31_REG (PDA_BASE_HW + 0x07c)
#define PDA_CFG_32_REG (PDA_BASE_HW + 0x080)
#define PDA_CFG_33_REG (PDA_BASE_HW + 0x084)
#define PDA_CFG_34_REG (PDA_BASE_HW + 0x088)
#define PDA_CFG_35_REG (PDA_BASE_HW + 0x08c)
#define PDA_CFG_36_REG (PDA_BASE_HW + 0x090)
#define PDA_CFG_37_REG (PDA_BASE_HW + 0x094)
#define PDA_CFG_38_REG (PDA_BASE_HW + 0x098)
#define PDA_CFG_39_REG (PDA_BASE_HW + 0x09c)
#define PDA_CFG_40_REG (PDA_BASE_HW + 0x0a0)
#define PDA_CFG_41_REG (PDA_BASE_HW + 0x0a4)
#define PDA_CFG_42_REG (PDA_BASE_HW + 0x0a8)
#define PDA_CFG_43_REG (PDA_BASE_HW + 0x0ac)
#define PDA_CFG_44_REG (PDA_BASE_HW + 0x0b0)
#define PDA_CFG_45_REG (PDA_BASE_HW + 0x0b4)
#define PDA_CFG_46_REG (PDA_BASE_HW + 0x0b8)
#define PDA_CFG_47_REG (PDA_BASE_HW + 0x0bc)
#define PDA_CFG_48_REG (PDA_BASE_HW + 0x0c0)
#define PDA_CFG_49_REG (PDA_BASE_HW + 0x0c4)
#define PDA_CFG_50_REG (PDA_BASE_HW + 0x0c8)
#define PDA_CFG_51_REG (PDA_BASE_HW + 0x0cc)
#define PDA_CFG_52_REG (PDA_BASE_HW + 0x0d0)
#define PDA_CFG_53_REG (PDA_BASE_HW + 0x0d4)
#define PDA_CFG_54_REG (PDA_BASE_HW + 0x0d8)
#define PDA_CFG_55_REG (PDA_BASE_HW + 0x0dc)
#define PDA_CFG_56_REG (PDA_BASE_HW + 0x0e0)
#define PDA_CFG_57_REG (PDA_BASE_HW + 0x0e4)
#define PDA_CFG_58_REG (PDA_BASE_HW + 0x0e8)
#define PDA_CFG_59_REG (PDA_BASE_HW + 0x0ec)
#define PDA_CFG_60_REG (PDA_BASE_HW + 0x0f0)
#define PDA_CFG_61_REG (PDA_BASE_HW + 0x0f4)
#define PDA_CFG_62_REG (PDA_BASE_HW + 0x0f8)
#define PDA_CFG_63_REG (PDA_BASE_HW + 0x0fc)
#define PDA_CFG_64_REG (PDA_BASE_HW + 0x100)
#define PDA_CFG_65_REG (PDA_BASE_HW + 0x104)
#define PDA_CFG_66_REG (PDA_BASE_HW + 0x108)
#define PDA_CFG_67_REG (PDA_BASE_HW + 0x10c)
#define PDA_CFG_68_REG (PDA_BASE_HW + 0x110)
#define PDA_CFG_69_REG (PDA_BASE_HW + 0x114)
#define PDA_CFG_70_REG (PDA_BASE_HW + 0x118)
#define PDA_CFG_71_REG (PDA_BASE_HW + 0x11c)
#define PDA_CFG_72_REG (PDA_BASE_HW + 0x120)
#define PDA_CFG_73_REG (PDA_BASE_HW + 0x124)
#define PDA_CFG_74_REG (PDA_BASE_HW + 0x128)
#define PDA_CFG_75_REG (PDA_BASE_HW + 0x12c)
#define PDA_CFG_76_REG (PDA_BASE_HW + 0x130)
#define PDA_CFG_77_REG (PDA_BASE_HW + 0x134)
#define PDA_CFG_78_REG (PDA_BASE_HW + 0x138)
#define PDA_CFG_79_REG (PDA_BASE_HW + 0x13c)
#define PDA_CFG_80_REG (PDA_BASE_HW + 0x140)
#define PDA_CFG_81_REG (PDA_BASE_HW + 0x144)
#define PDA_CFG_82_REG (PDA_BASE_HW + 0x148)
#define PDA_CFG_83_REG (PDA_BASE_HW + 0x14c)
#define PDA_CFG_84_REG (PDA_BASE_HW + 0x150)
#define PDA_CFG_85_REG (PDA_BASE_HW + 0x154)
#define PDA_CFG_86_REG (PDA_BASE_HW + 0x158)
#define PDA_CFG_87_REG (PDA_BASE_HW + 0x15c)
#define PDA_CFG_88_REG (PDA_BASE_HW + 0x160)
#define PDA_CFG_89_REG (PDA_BASE_HW + 0x164)
#define PDA_CFG_90_REG (PDA_BASE_HW + 0x168)
#define PDA_CFG_91_REG (PDA_BASE_HW + 0x16c)
#define PDA_CFG_92_REG (PDA_BASE_HW + 0x170)
#define PDA_CFG_93_REG (PDA_BASE_HW + 0x174)
#define PDA_CFG_94_REG (PDA_BASE_HW + 0x178)
#define PDA_CFG_95_REG (PDA_BASE_HW + 0x17c)
#define PDA_CFG_96_REG (PDA_BASE_HW + 0x180)
#define PDA_CFG_97_REG (PDA_BASE_HW + 0x184)
#define PDA_CFG_98_REG (PDA_BASE_HW + 0x188)
#define PDA_CFG_99_REG (PDA_BASE_HW + 0x18c)
#define PDA_CFG_100_REG (PDA_BASE_HW + 0x190)
#define PDA_CFG_101_REG (PDA_BASE_HW + 0x194)
#define PDA_CFG_102_REG (PDA_BASE_HW + 0x198)
#define PDA_CFG_103_REG (PDA_BASE_HW + 0x19c)
#define PDA_CFG_104_REG (PDA_BASE_HW + 0x1a0)
#define PDA_CFG_105_REG (PDA_BASE_HW + 0x1a4)
#define PDA_CFG_106_REG (PDA_BASE_HW + 0x1a8)
#define PDA_CFG_107_REG (PDA_BASE_HW + 0x1ac)
#define PDA_CFG_108_REG (PDA_BASE_HW + 0x1b0)
#define PDA_CFG_109_REG (PDA_BASE_HW + 0x1b4)
#define PDA_CFG_110_REG (PDA_BASE_HW + 0x1b8)
#define PDA_CFG_111_REG (PDA_BASE_HW + 0x1bc)
#define PDA_CFG_112_REG (PDA_BASE_HW + 0x1c0)
#define PDA_CFG_113_REG (PDA_BASE_HW + 0x1c4)
#define PDA_CFG_114_REG (PDA_BASE_HW + 0x1c8)
#define PDA_CFG_115_REG (PDA_BASE_HW + 0x1cc)
#define PDA_CFG_116_REG (PDA_BASE_HW + 0x1d0)
#define PDA_CFG_117_REG (PDA_BASE_HW + 0x1d4)
#define PDA_CFG_118_REG (PDA_BASE_HW + 0x1d8)
#define PDA_CFG_119_REG (PDA_BASE_HW + 0x1dc)
#define PDA_CFG_120_REG (PDA_BASE_HW + 0x1e0)
#define PDA_CFG_121_REG (PDA_BASE_HW + 0x1e4)
#define PDA_CFG_122_REG (PDA_BASE_HW + 0x1e8)
#define PDA_CFG_123_REG (PDA_BASE_HW + 0x1ec)
#define PDA_CFG_124_REG (PDA_BASE_HW + 0x1f0)
#define PDA_CFG_125_REG (PDA_BASE_HW + 0x1f4)
#define PDA_CFG_126_REG (PDA_BASE_HW + 0x1f8)
#define PDA_PDAI_P1_BASE_ADDR_REG (PDA_BASE_HW + 0x300)
#define PDA_PDATI_P1_BASE_ADDR_REG (PDA_BASE_HW + 0x304)
#define PDA_PDAI_P2_BASE_ADDR_REG (PDA_BASE_HW + 0x308)
#define PDA_PDATI_P2_BASE_ADDR_REG (PDA_BASE_HW + 0x30c)
#define PDA_PDAI_STRIDE_REG (PDA_BASE_HW + 0x310)
#define PDA_PDAI_P1_CON0_REG (PDA_BASE_HW + 0x314)
#define PDA_PDAI_P1_CON1_REG (PDA_BASE_HW + 0x318)
#define PDA_PDAI_P1_CON2_REG (PDA_BASE_HW + 0x31c)
#define PDA_PDAI_P1_CON3_REG (PDA_BASE_HW + 0x320)
#define PDA_PDAI_P1_CON4_REG (PDA_BASE_HW + 0x324)
#define PDA_PDATI_P1_CON0_REG (PDA_BASE_HW + 0x328)
#define PDA_PDATI_P1_CON1_REG (PDA_BASE_HW + 0x32c)
#define PDA_PDATI_P1_CON2_REG (PDA_BASE_HW + 0x330)
#define PDA_PDATI_P1_CON3_REG (PDA_BASE_HW + 0x334)
#define PDA_PDATI_P1_CON4_REG (PDA_BASE_HW + 0x338)
#define PDA_PDAI_P2_CON0_REG (PDA_BASE_HW + 0x33c)
#define PDA_PDAI_P2_CON1_REG (PDA_BASE_HW + 0x340)
#define PDA_PDAI_P2_CON2_REG (PDA_BASE_HW + 0x344)
#define PDA_PDAI_P2_CON3_REG (PDA_BASE_HW + 0x348)
#define PDA_PDAI_P2_CON4_REG (PDA_BASE_HW + 0x34c)
#define PDA_PDATI_P2_CON0_REG (PDA_BASE_HW + 0x350)
#define PDA_PDATI_P2_CON1_REG (PDA_BASE_HW + 0x354)
#define PDA_PDATI_P2_CON2_REG (PDA_BASE_HW + 0x358)
#define PDA_PDATI_P2_CON3_REG (PDA_BASE_HW + 0x35c)
#define PDA_PDATI_P2_CON4_REG (PDA_BASE_HW + 0x360)
#define PDA_PDAO_P1_BASE_ADDR_REG (PDA_BASE_HW + 0x364)
#define PDA_PDAO_P1_XSIZE_REG (PDA_BASE_HW + 0x368)
#define PDA_PDAO_P1_CON0_REG (PDA_BASE_HW + 0x36c)
#define PDA_PDAO_P1_CON1_REG (PDA_BASE_HW + 0x370)
#define PDA_PDAO_P1_CON2_REG (PDA_BASE_HW + 0x374)
#define PDA_PDAO_P1_CON3_REG (PDA_BASE_HW + 0x378)
#define PDA_PDAO_P1_CON4_REG (PDA_BASE_HW + 0x37c)
#define PDA_PDA_DMA_EN_REG (PDA_BASE_HW + 0x380)
#define PDA_PDA_DMA_RST_REG (PDA_BASE_HW + 0x384)
#define PDA_PDA_DMA_TOP_REG (PDA_BASE_HW + 0x388)
#define PDA_PDA_SECURE_REG (PDA_BASE_HW + 0x38c)
#define PDA_PDA_TILE_STATUS_REG (PDA_BASE_HW + 0x390)
#define PDA_PDA_DCM_DIS_REG (PDA_BASE_HW + 0x394)
#define PDA_PDA_DCM_ST_REG (PDA_BASE_HW + 0x398)
#define PDA_PDAI_P1_ERR_STAT_REG (PDA_BASE_HW + 0x39c)
#define PDA_PDATI_P1_ERR_STAT_REG (PDA_BASE_HW + 0x3a0)
#define PDA_PDAI_P2_ERR_STAT_REG (PDA_BASE_HW + 0x3a4)
#define PDA_PDATI_P2_ERR_STAT_REG (PDA_BASE_HW + 0x3a8)
#define PDA_PDAO_P1_ERR_STAT_REG (PDA_BASE_HW + 0x3ac)
#define PDA_PDA_ERR_STAT_EN_REG (PDA_BASE_HW + 0x3b0)
#define PDA_PDA_ERR_STAT_REG (PDA_BASE_HW + 0x3b4)
#define PDA_PDA_TOP_CTL_REG (PDA_BASE_HW + 0x3b8)
#define PDA_PDA_DEBUG_SEL_REG (PDA_BASE_HW + 0x3bc)
#define PDA_PDA_IRQ_TRIG_REG (PDA_BASE_HW + 0x3c0)
#define PDA_PDA_SPARE1_REG (PDA_BASE_HW + 0x3c4)
#define PDA_PDA_SPARE2_REG (PDA_BASE_HW + 0x3c8)
#define PDA_PDA_SPARE3_REG (PDA_BASE_HW + 0x3cc)
#define PDA_PDA_SPARE4_REG (PDA_BASE_HW + 0x3d0)
#define PDA_PDA_SPARE5_REG (PDA_BASE_HW + 0x3d4)
#define PDA_PDA_SPARE6_REG (PDA_BASE_HW + 0x3d8)
#define PDA_PDA_SPARE7_REG (PDA_BASE_HW + 0x3dc)

struct PDA_Config {
	unsigned int PDA_CFG[127];
	unsigned int PDA_PDAI_P1_BASE_ADDR;
	unsigned int PDA_PDATI_P1_BASE_ADDR;
	unsigned int PDA_PDAI_P2_BASE_ADDR;
	unsigned int PDA_PDATI_P2_BASE_ADDR;
	unsigned int PDA_PDAI_STRIDE;
	unsigned int PDA_PDAI_P1_CON0;
	unsigned int PDA_PDAI_P1_CON1;
	unsigned int PDA_PDAI_P1_CON2;
	unsigned int PDA_PDAI_P1_CON3;
	unsigned int PDA_PDAI_P1_CON4;
	unsigned int PDA_PDATI_P1_CON0;
	unsigned int PDA_PDATI_P1_CON1;
	unsigned int PDA_PDATI_P1_CON2;
	unsigned int PDA_PDATI_P1_CON3;
	unsigned int PDA_PDATI_P1_CON4;
	unsigned int PDA_PDAI_P2_CON0;
	unsigned int PDA_PDAI_P2_CON1;
	unsigned int PDA_PDAI_P2_CON2;
	unsigned int PDA_PDAI_P2_CON3;
	unsigned int PDA_PDAI_P2_CON4;
	unsigned int PDA_PDATI_P2_CON0;
	unsigned int PDA_PDATI_P2_CON1;
	unsigned int PDA_PDATI_P2_CON2;
	unsigned int PDA_PDATI_P2_CON3;
	unsigned int PDA_PDATI_P2_CON4;
	unsigned int PDA_PDAO_P1_BASE_ADDR;
	unsigned int PDA_PDAO_P1_XSIZE;
	unsigned int PDA_PDAO_P1_CON0;
	unsigned int PDA_PDAO_P1_CON1;
	unsigned int PDA_PDAO_P1_CON2;
	unsigned int PDA_PDAO_P1_CON3;
	unsigned int PDA_PDAO_P1_CON4;
	unsigned int PDA_PDA_DMA_EN;
	unsigned int PDA_PDA_DMA_RST;
	unsigned int PDA_PDA_DMA_TOP;
	unsigned int PDA_PDA_SECURE;
	unsigned int PDA_PDA_TILE_STATUS;
	unsigned int PDA_PDA_DCM_DIS;
	unsigned int PDA_PDA_DCM_ST;
	unsigned int PDA_PDAI_P1_ERR_STAT;
	unsigned int PDA_PDATI_P1_ERR_STAT;
	unsigned int PDA_PDAI_P2_ERR_STAT;
	unsigned int PDA_PDATI_P2_ERR_STAT;
	unsigned int PDA_PDAO_P1_ERR_STAT;
	unsigned int PDA_PDA_ERR_STAT_EN;
	unsigned int PDA_PDA_ERR_STAT;
	unsigned int PDA_PDA_TOP_CTL;
	unsigned int PDA_PDA_DEBUG_SEL;
	unsigned int PDA_PDA_IRQ_TRIG;
	unsigned int PDA_PDA_SPARE1;
	unsigned int PDA_PDA_SPARE2;
	unsigned int PDA_PDA_SPARE3;
	unsigned int PDA_PDA_SPARE4;
	unsigned int PDA_PDA_SPARE5;
	unsigned int PDA_PDA_SPARE6;
	unsigned int PDA_PDA_SPARE7;
};

enum PDA_CMD_ENUM {
	PDA_CMD_RESET,		/* Reset */
	PDA_CMD_ENQUE,		/* PDA Enque */
	PDA_CMD_DEQUE,		/* PDA Deque */
	PDA_CMD_WAIT_IRQ,
	PDA_CMD_TOTAL,
};

struct PDA_WAIT_IRQ_STRUCT {
	unsigned int Status;    /*IRQ Status */
	unsigned int Timeout;
	//int ProcessID;      /* user ProcessID (will filled in kernel) */
};

#define PDA_RESET	_IO(PDA_MAGIC, PDA_CMD_RESET)
#define PDA_ENQUE	_IOWR(PDA_MAGIC, PDA_CMD_ENQUE, struct PDA_Config)
#define PDA_DEQUE	_IOWR(PDA_MAGIC, PDA_CMD_DEQUE, struct PDA_Config)
#define PDA_WAIT_IRQ	_IOWR(PDA_MAGIC, PDA_CMD_WAIT_IRQ, struct PDA_WAIT_IRQ_STRUCT)

#endif/*__MTK_PDA_HW_H__*/
