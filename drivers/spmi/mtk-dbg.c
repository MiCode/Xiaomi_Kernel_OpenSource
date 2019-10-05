// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */
/* #define DEBUG */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6315/registers.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <spmi_sw.h>

enum pmif_dbg_regs {
	PMIF_INIT_DONE,
	PMIF_INF_BUSY_STA,
	PMIF_OTHER_BUSY_STA_0,
	PMIF_OTHER_BUSY_STA_1,
	PMIF_IRQ_EVENT_EN_0,
	PMIF_IRQ_FLAG_0,
	PMIF_IRQ_CLR_0,
	PMIF_IRQ_EVENT_EN_1,
	PMIF_IRQ_FLAG_1,
	PMIF_IRQ_CLR_1,
	PMIF_IRQ_EVENT_EN_2,
	PMIF_IRQ_FLAG_2,
	PMIF_IRQ_CLR_2,
	PMIF_IRQ_EVENT_EN_3,
	PMIF_IRQ_FLAG_3,
	PMIF_IRQ_CLR_3,
	PMIF_IRQ_EVENT_EN_4,
	PMIF_IRQ_FLAG_4,
	PMIF_IRQ_CLR_4,
	PMIF_WDT_EVENT_EN_0,
	PMIF_WDT_FLAG_0,
	PMIF_WDT_EVENT_EN_1,
	PMIF_WDT_FLAG_1,
	PMIF_MONITOR_CTRL,
	PMIF_MONITOR_TARGET_CHAN_0,
	PMIF_MONITOR_TARGET_CHAN_1,
	PMIF_MONITOR_TARGET_CHAN_2,
	PMIF_MONITOR_TARGET_CHAN_3,
	PMIF_MONITOR_TARGET_CHAN_4,
	PMIF_MONITOR_TARGET_CHAN_5,
	PMIF_MONITOR_TARGET_CHAN_6,
	PMIF_MONITOR_TARGET_CHAN_7,
	PMIF_MONITOR_TARGET_WRITE,
	PMIF_MONITOR_TARGET_ADDR_0,
	PMIF_MONITOR_TARGET_ADDR_1,
	PMIF_MONITOR_TARGET_ADDR_2,
	PMIF_MONITOR_TARGET_ADDR_3,
	PMIF_MONITOR_TARGET_ADDR_4,
	PMIF_MONITOR_TARGET_ADDR_5,
	PMIF_MONITOR_TARGET_ADDR_6,
	PMIF_MONITOR_TARGET_ADDR_7,
	PMIF_MONITOR_TARGET_WDATA_0,
	PMIF_MONITOR_TARGET_WDATA_1,
	PMIF_MONITOR_TARGET_WDATA_2,
	PMIF_MONITOR_TARGET_WDATA_3,
	PMIF_MONITOR_TARGET_WDATA_4,
	PMIF_MONITOR_TARGET_WDATA_5,
	PMIF_MONITOR_TARGET_WDATA_6,
	PMIF_MONITOR_TARGET_WDATA_7,
	PMIF_MONITOR_STA,
	PMIF_MONITOR_RECORD_0_0,
	PMIF_MONITOR_RECORD_0_1,
	PMIF_MONITOR_RECORD_0_2,
	PMIF_MONITOR_RECORD_0_3,
	PMIF_MONITOR_RECORD_0_4,
	PMIF_MONITOR_RECORD_1_0,
	PMIF_MONITOR_RECORD_1_1,
	PMIF_MONITOR_RECORD_1_2,
	PMIF_MONITOR_RECORD_1_3,
	PMIF_MONITOR_RECORD_1_4,
	PMIF_MONITOR_RECORD_2_0,
	PMIF_MONITOR_RECORD_2_1,
	PMIF_MONITOR_RECORD_2_2,
	PMIF_MONITOR_RECORD_2_3,
	PMIF_MONITOR_RECORD_2_4,
	PMIF_MONITOR_RECORD_3_0,
	PMIF_MONITOR_RECORD_3_1,
	PMIF_MONITOR_RECORD_3_2,
	PMIF_MONITOR_RECORD_3_3,
	PMIF_MONITOR_RECORD_3_4,
	PMIF_MONITOR_RECORD_4_0,
	PMIF_MONITOR_RECORD_4_1,
	PMIF_MONITOR_RECORD_4_2,
	PMIF_MONITOR_RECORD_4_3,
	PMIF_MONITOR_RECORD_4_4,
	PMIF_MONITOR_RECORD_5_0,
	PMIF_MONITOR_RECORD_5_1,
	PMIF_MONITOR_RECORD_5_2,
	PMIF_MONITOR_RECORD_5_3,
	PMIF_MONITOR_RECORD_5_4,
	PMIF_MONITOR_RECORD_6_0,
	PMIF_MONITOR_RECORD_6_1,
	PMIF_MONITOR_RECORD_6_2,
	PMIF_MONITOR_RECORD_6_3,
	PMIF_MONITOR_RECORD_6_4,
	PMIF_MONITOR_RECORD_7_0,
	PMIF_MONITOR_RECORD_7_1,
	PMIF_MONITOR_RECORD_7_2,
	PMIF_MONITOR_RECORD_7_3,
	PMIF_MONITOR_RECORD_7_4,
	PMIF_MONITOR_RECORD_8_0,
	PMIF_MONITOR_RECORD_8_1,
	PMIF_MONITOR_RECORD_8_2,
	PMIF_MONITOR_RECORD_8_3,
	PMIF_MONITOR_RECORD_8_4,
	PMIF_MONITOR_RECORD_9_0,
	PMIF_MONITOR_RECORD_9_1,
	PMIF_MONITOR_RECORD_9_2,
	PMIF_MONITOR_RECORD_9_3,
	PMIF_MONITOR_RECORD_9_4,
	PMIF_MONITOR_RECORD_10_0,
	PMIF_MONITOR_RECORD_10_1,
	PMIF_MONITOR_RECORD_10_2,
	PMIF_MONITOR_RECORD_10_3,
	PMIF_MONITOR_RECORD_10_4,
	PMIF_MONITOR_RECORD_11_0,
	PMIF_MONITOR_RECORD_11_1,
	PMIF_MONITOR_RECORD_11_2,
	PMIF_MONITOR_RECORD_11_3,
	PMIF_MONITOR_RECORD_11_4,
	PMIF_MONITOR_RECORD_12_0,
	PMIF_MONITOR_RECORD_12_1,
	PMIF_MONITOR_RECORD_12_2,
	PMIF_MONITOR_RECORD_12_3,
	PMIF_MONITOR_RECORD_12_4,
	PMIF_MONITOR_RECORD_13_0,
	PMIF_MONITOR_RECORD_13_1,
	PMIF_MONITOR_RECORD_13_2,
	PMIF_MONITOR_RECORD_13_3,
	PMIF_MONITOR_RECORD_13_4,
	PMIF_MONITOR_RECORD_14_0,
	PMIF_MONITOR_RECORD_14_1,
	PMIF_MONITOR_RECORD_14_2,
	PMIF_MONITOR_RECORD_14_3,
	PMIF_MONITOR_RECORD_14_4,
	PMIF_MONITOR_RECORD_15_0,
	PMIF_MONITOR_RECORD_15_1,
	PMIF_MONITOR_RECORD_15_2,
	PMIF_MONITOR_RECORD_15_3,
	PMIF_MONITOR_RECORD_15_4,
	PMIF_MONITOR_RECORD_16_0,
	PMIF_MONITOR_RECORD_16_1,
	PMIF_MONITOR_RECORD_16_2,
	PMIF_MONITOR_RECORD_16_3,
	PMIF_MONITOR_RECORD_16_4,
	PMIF_MONITOR_RECORD_17_0,
	PMIF_MONITOR_RECORD_17_1,
	PMIF_MONITOR_RECORD_17_2,
	PMIF_MONITOR_RECORD_17_3,
	PMIF_MONITOR_RECORD_17_4,
	PMIF_MONITOR_RECORD_18_0,
	PMIF_MONITOR_RECORD_18_1,
	PMIF_MONITOR_RECORD_18_2,
	PMIF_MONITOR_RECORD_18_3,
	PMIF_MONITOR_RECORD_18_4,
	PMIF_MONITOR_RECORD_19_0,
	PMIF_MONITOR_RECORD_19_1,
	PMIF_MONITOR_RECORD_19_2,
	PMIF_MONITOR_RECORD_19_3,
	PMIF_MONITOR_RECORD_19_4,
	PMIF_MONITOR_RECORD_20_0,
	PMIF_MONITOR_RECORD_20_1,
	PMIF_MONITOR_RECORD_20_2,
	PMIF_MONITOR_RECORD_20_3,
	PMIF_MONITOR_RECORD_20_4,
	PMIF_MONITOR_RECORD_21_0,
	PMIF_MONITOR_RECORD_21_1,
	PMIF_MONITOR_RECORD_21_2,
	PMIF_MONITOR_RECORD_21_3,
	PMIF_MONITOR_RECORD_21_4,
	PMIF_MONITOR_RECORD_22_0,
	PMIF_MONITOR_RECORD_22_1,
	PMIF_MONITOR_RECORD_22_2,
	PMIF_MONITOR_RECORD_22_3,
	PMIF_MONITOR_RECORD_22_4,
	PMIF_MONITOR_RECORD_23_0,
	PMIF_MONITOR_RECORD_23_1,
	PMIF_MONITOR_RECORD_23_2,
	PMIF_MONITOR_RECORD_23_3,
	PMIF_MONITOR_RECORD_23_4,
	PMIF_MONITOR_RECORD_24_0,
	PMIF_MONITOR_RECORD_24_1,
	PMIF_MONITOR_RECORD_24_2,
	PMIF_MONITOR_RECORD_24_3,
	PMIF_MONITOR_RECORD_24_4,
	PMIF_MONITOR_RECORD_25_0,
	PMIF_MONITOR_RECORD_25_1,
	PMIF_MONITOR_RECORD_25_2,
	PMIF_MONITOR_RECORD_25_3,
	PMIF_MONITOR_RECORD_25_4,
	PMIF_MONITOR_RECORD_26_0,
	PMIF_MONITOR_RECORD_26_1,
	PMIF_MONITOR_RECORD_26_2,
	PMIF_MONITOR_RECORD_26_3,
	PMIF_MONITOR_RECORD_26_4,
	PMIF_MONITOR_RECORD_27_0,
	PMIF_MONITOR_RECORD_27_1,
	PMIF_MONITOR_RECORD_27_2,
	PMIF_MONITOR_RECORD_27_3,
	PMIF_MONITOR_RECORD_27_4,
	PMIF_MONITOR_RECORD_28_0,
	PMIF_MONITOR_RECORD_28_1,
	PMIF_MONITOR_RECORD_28_2,
	PMIF_MONITOR_RECORD_28_3,
	PMIF_MONITOR_RECORD_28_4,
	PMIF_MONITOR_RECORD_29_0,
	PMIF_MONITOR_RECORD_29_1,
	PMIF_MONITOR_RECORD_29_2,
	PMIF_MONITOR_RECORD_29_3,
	PMIF_MONITOR_RECORD_29_4,
	PMIF_MONITOR_RECORD_30_0,
	PMIF_MONITOR_RECORD_30_1,
	PMIF_MONITOR_RECORD_30_2,
	PMIF_MONITOR_RECORD_30_3,
	PMIF_MONITOR_RECORD_30_4,
	PMIF_MONITOR_RECORD_31_0,
	PMIF_MONITOR_RECORD_31_1,
	PMIF_MONITOR_RECORD_31_2,
	PMIF_MONITOR_RECORD_31_3,
	PMIF_MONITOR_RECORD_31_4,
	PMIF_DEBUG_CTRL,
	PMIF_RESERVED_0,
	PMIF_SWINF_0_ACC,
	PMIF_SWINF_0_WDATA_31_0,
	PMIF_SWINF_0_WDATA_63_32,
	PMIF_SWINF_0_RDATA_31_0,
	PMIF_SWINF_0_RDATA_63_32,
	PMIF_SWINF_0_VLD_CLR,
	PMIF_SWINF_0_STA,
	PMIF_SWINF_1_ACC,
	PMIF_SWINF_1_WDATA_31_0,
	PMIF_SWINF_1_WDATA_63_32,
	PMIF_SWINF_1_RDATA_31_0,
	PMIF_SWINF_1_RDATA_63_32,
	PMIF_SWINF_1_VLD_CLR,
	PMIF_SWINF_1_STA,
	PMIF_SWINF_2_ACC,
	PMIF_SWINF_2_WDATA_31_0,
	PMIF_SWINF_2_WDATA_63_32,
	PMIF_SWINF_2_RDATA_31_0,
	PMIF_SWINF_2_RDATA_63_32,
	PMIF_SWINF_2_VLD_CLR,
	PMIF_SWINF_2_STA,
	PMIF_SWINF_3_ACC,
	PMIF_SWINF_3_WDATA_31_0,
	PMIF_SWINF_3_WDATA_63_32,
	PMIF_SWINF_3_RDATA_31_0,
	PMIF_SWINF_3_RDATA_63_32,
	PMIF_SWINF_3_VLD_CLR,
	PMIF_SWINF_3_STA,
};

static int mt6xxx_pmif_dbg_regs[] = {
	[PMIF_INIT_DONE] =			0x0000,
	[PMIF_INF_BUSY_STA] =			0x0018,
	[PMIF_OTHER_BUSY_STA_0] =		0x001C,
	[PMIF_OTHER_BUSY_STA_1] =		0x0020,
	[PMIF_IRQ_EVENT_EN_0] =                 0x0418,
	[PMIF_IRQ_FLAG_0] =                     0x0420,
	[PMIF_IRQ_CLR_0] =                      0x0424,
	[PMIF_IRQ_EVENT_EN_1] =                 0x0428,
	[PMIF_IRQ_FLAG_1] =                     0x0430,
	[PMIF_IRQ_CLR_1] =                      0x0434,
	[PMIF_IRQ_EVENT_EN_2] =                 0x0438,
	[PMIF_IRQ_FLAG_2] =                     0x0440,
	[PMIF_IRQ_CLR_2] =                      0x0444,
	[PMIF_IRQ_EVENT_EN_3] =                 0x0448,
	[PMIF_IRQ_FLAG_3] =                     0x0450,
	[PMIF_IRQ_CLR_3] =                      0x0454,
	[PMIF_IRQ_EVENT_EN_4] =                 0x0458,
	[PMIF_IRQ_FLAG_4] =                     0x0460,
	[PMIF_IRQ_CLR_4] =                      0x0464,
	[PMIF_WDT_EVENT_EN_0] =			0x046C,
	[PMIF_WDT_FLAG_0] =			0x0470,
	[PMIF_WDT_EVENT_EN_1] =			0x0474,
	[PMIF_WDT_FLAG_1] =			0x0478,
	[PMIF_MONITOR_CTRL] =			0x047C,
	[PMIF_MONITOR_TARGET_CHAN_0] =		0x0480,
	[PMIF_MONITOR_TARGET_CHAN_1] =		0x0484,
	[PMIF_MONITOR_TARGET_CHAN_2] =		0x0488,
	[PMIF_MONITOR_TARGET_CHAN_3] =		0x048C,
	[PMIF_MONITOR_TARGET_CHAN_4] =		0x0490,
	[PMIF_MONITOR_TARGET_CHAN_5] =		0x0494,
	[PMIF_MONITOR_TARGET_CHAN_6] =		0x0498,
	[PMIF_MONITOR_TARGET_CHAN_7] =		0x049C,
	[PMIF_MONITOR_TARGET_WRITE] =		0x04A0,
	[PMIF_MONITOR_TARGET_ADDR_0] =		0x04A4,
	[PMIF_MONITOR_TARGET_ADDR_1] =		0x04A8,
	[PMIF_MONITOR_TARGET_ADDR_2] =		0x04AC,
	[PMIF_MONITOR_TARGET_ADDR_3] =		0x04B0,
	[PMIF_MONITOR_TARGET_ADDR_4] =		0x04B4,
	[PMIF_MONITOR_TARGET_ADDR_5] =		0x04B8,
	[PMIF_MONITOR_TARGET_ADDR_6] =		0x04BC,
	[PMIF_MONITOR_TARGET_ADDR_7] =		0x04C0,
	[PMIF_MONITOR_TARGET_WDATA_0] =		0x04C4,
	[PMIF_MONITOR_TARGET_WDATA_1] =		0x04C8,
	[PMIF_MONITOR_TARGET_WDATA_2] =		0x04CC,
	[PMIF_MONITOR_TARGET_WDATA_3] =		0x04D0,
	[PMIF_MONITOR_TARGET_WDATA_4] =		0x04D4,
	[PMIF_MONITOR_TARGET_WDATA_5] =		0x04D8,
	[PMIF_MONITOR_TARGET_WDATA_6] =		0x04DC,
	[PMIF_MONITOR_TARGET_WDATA_7] =		0x04E0,
	[PMIF_MONITOR_STA] =			0x04E4,
	[PMIF_MONITOR_RECORD_0_0] =		0x04E8,
	[PMIF_MONITOR_RECORD_0_1] =		0x04EC,
	[PMIF_MONITOR_RECORD_0_2] =		0x04F0,
	[PMIF_MONITOR_RECORD_0_3] =		0x04F4,
	[PMIF_MONITOR_RECORD_0_4] =		0x04F8,
	[PMIF_MONITOR_RECORD_1_0] =		0x04FC,
	[PMIF_MONITOR_RECORD_1_1] =		0x0500,
	[PMIF_MONITOR_RECORD_1_2] =		0x0504,
	[PMIF_MONITOR_RECORD_1_3] =		0x0508,
	[PMIF_MONITOR_RECORD_1_4] =		0x050C,
	[PMIF_MONITOR_RECORD_2_0] =		0x0510,
	[PMIF_MONITOR_RECORD_2_1] =		0x0514,
	[PMIF_MONITOR_RECORD_2_2] =		0x0518,
	[PMIF_MONITOR_RECORD_2_3] =		0x051C,
	[PMIF_MONITOR_RECORD_2_4] =		0x0520,
	[PMIF_MONITOR_RECORD_3_0] =		0x0524,
	[PMIF_MONITOR_RECORD_3_1] =		0x0528,
	[PMIF_MONITOR_RECORD_3_2] =		0x052C,
	[PMIF_MONITOR_RECORD_3_3] =		0x0530,
	[PMIF_MONITOR_RECORD_3_4] =		0x0534,
	[PMIF_MONITOR_RECORD_4_0] =		0x0538,
	[PMIF_MONITOR_RECORD_4_1] =		0x053C,
	[PMIF_MONITOR_RECORD_4_2] =		0x0540,
	[PMIF_MONITOR_RECORD_4_3] =		0x0544,
	[PMIF_MONITOR_RECORD_4_4] =		0x0548,
	[PMIF_MONITOR_RECORD_5_0] =		0x054C,
	[PMIF_MONITOR_RECORD_5_1] =		0x0550,
	[PMIF_MONITOR_RECORD_5_2] =		0x0554,
	[PMIF_MONITOR_RECORD_5_3] =		0x0558,
	[PMIF_MONITOR_RECORD_5_4] =		0x055C,
	[PMIF_MONITOR_RECORD_6_0] =		0x0560,
	[PMIF_MONITOR_RECORD_6_1] =		0x0564,
	[PMIF_MONITOR_RECORD_6_2] =		0x0568,
	[PMIF_MONITOR_RECORD_6_3] =		0x056C,
	[PMIF_MONITOR_RECORD_6_4] =		0x0570,
	[PMIF_MONITOR_RECORD_7_0] =		0x0574,
	[PMIF_MONITOR_RECORD_7_1] =		0x0578,
	[PMIF_MONITOR_RECORD_7_2] =		0x057C,
	[PMIF_MONITOR_RECORD_7_3] =		0x0580,
	[PMIF_MONITOR_RECORD_7_4] =		0x0584,
	[PMIF_MONITOR_RECORD_8_0] =		0x0588,
	[PMIF_MONITOR_RECORD_8_1] =		0x058C,
	[PMIF_MONITOR_RECORD_8_2] =		0x0590,
	[PMIF_MONITOR_RECORD_8_3] =		0x0594,
	[PMIF_MONITOR_RECORD_8_4] =		0x0598,
	[PMIF_MONITOR_RECORD_9_0] =		0x059C,
	[PMIF_MONITOR_RECORD_9_1] =		0x05A0,
	[PMIF_MONITOR_RECORD_9_2] =		0x05A4,
	[PMIF_MONITOR_RECORD_9_3] =		0x05A8,
	[PMIF_MONITOR_RECORD_9_4] =		0x05AC,
	[PMIF_MONITOR_RECORD_10_0] =		0x05B0,
	[PMIF_MONITOR_RECORD_10_1] =		0x05B4,
	[PMIF_MONITOR_RECORD_10_2] =		0x05B8,
	[PMIF_MONITOR_RECORD_10_3] =		0x05BC,
	[PMIF_MONITOR_RECORD_10_4] =		0x05C0,
	[PMIF_MONITOR_RECORD_11_0] =		0x05C4,
	[PMIF_MONITOR_RECORD_11_1] =		0x05C8,
	[PMIF_MONITOR_RECORD_11_2] =		0x05CC,
	[PMIF_MONITOR_RECORD_11_3] =		0x05D0,
	[PMIF_MONITOR_RECORD_11_4] =		0x05D4,
	[PMIF_MONITOR_RECORD_12_0] =		0x05D8,
	[PMIF_MONITOR_RECORD_12_1] =		0x05DC,
	[PMIF_MONITOR_RECORD_12_2] =		0x05E0,
	[PMIF_MONITOR_RECORD_12_3] =		0x05E4,
	[PMIF_MONITOR_RECORD_12_4] =		0x05E8,
	[PMIF_MONITOR_RECORD_13_0] =		0x05EC,
	[PMIF_MONITOR_RECORD_13_1] =		0x05F0,
	[PMIF_MONITOR_RECORD_13_2] =		0x05F4,
	[PMIF_MONITOR_RECORD_13_3] =		0x05F8,
	[PMIF_MONITOR_RECORD_13_4] =		0x05FC,
	[PMIF_MONITOR_RECORD_14_0] =		0x0600,
	[PMIF_MONITOR_RECORD_14_1] =		0x0604,
	[PMIF_MONITOR_RECORD_14_2] =		0x0608,
	[PMIF_MONITOR_RECORD_14_3] =		0x060C,
	[PMIF_MONITOR_RECORD_14_4] =		0x0610,
	[PMIF_MONITOR_RECORD_15_0] =		0x0614,
	[PMIF_MONITOR_RECORD_15_1] =		0x0618,
	[PMIF_MONITOR_RECORD_15_2] =		0x061C,
	[PMIF_MONITOR_RECORD_15_3] =		0x0620,
	[PMIF_MONITOR_RECORD_15_4] =		0x0624,
	[PMIF_MONITOR_RECORD_16_0] =		0x0628,
	[PMIF_MONITOR_RECORD_16_1] =		0x062C,
	[PMIF_MONITOR_RECORD_16_2] =		0x0630,
	[PMIF_MONITOR_RECORD_16_3] =		0x0634,
	[PMIF_MONITOR_RECORD_16_4] =		0x0638,
	[PMIF_MONITOR_RECORD_17_0] =		0x063C,
	[PMIF_MONITOR_RECORD_17_1] =		0x0640,
	[PMIF_MONITOR_RECORD_17_2] =		0x0644,
	[PMIF_MONITOR_RECORD_17_3] =		0x0648,
	[PMIF_MONITOR_RECORD_17_4] =		0x064C,
	[PMIF_MONITOR_RECORD_18_0] =		0x0650,
	[PMIF_MONITOR_RECORD_18_1] =		0x0654,
	[PMIF_MONITOR_RECORD_18_2] =		0x0658,
	[PMIF_MONITOR_RECORD_18_3] =		0x065C,
	[PMIF_MONITOR_RECORD_18_4] =		0x0660,
	[PMIF_MONITOR_RECORD_19_0] =		0x0664,
	[PMIF_MONITOR_RECORD_19_1] =		0x0668,
	[PMIF_MONITOR_RECORD_19_2] =		0x066C,
	[PMIF_MONITOR_RECORD_19_3] =		0x0670,
	[PMIF_MONITOR_RECORD_19_4] =		0x0674,
	[PMIF_MONITOR_RECORD_20_0] =		0x0678,
	[PMIF_MONITOR_RECORD_20_1] =		0x067C,
	[PMIF_MONITOR_RECORD_20_2] =		0x0680,
	[PMIF_MONITOR_RECORD_20_3] =		0x0684,
	[PMIF_MONITOR_RECORD_20_4] =		0x0688,
	[PMIF_MONITOR_RECORD_21_0] =		0x068C,
	[PMIF_MONITOR_RECORD_21_1] =		0x0690,
	[PMIF_MONITOR_RECORD_21_2] =		0x0694,
	[PMIF_MONITOR_RECORD_21_3] =		0x0698,
	[PMIF_MONITOR_RECORD_21_4] =		0x069C,
	[PMIF_MONITOR_RECORD_22_0] =		0x06A0,
	[PMIF_MONITOR_RECORD_22_1] =		0x06A4,
	[PMIF_MONITOR_RECORD_22_2] =		0x06A8,
	[PMIF_MONITOR_RECORD_22_3] =		0x06AC,
	[PMIF_MONITOR_RECORD_22_4] =		0x06B0,
	[PMIF_MONITOR_RECORD_23_0] =		0x06B4,
	[PMIF_MONITOR_RECORD_23_1] =		0x06B8,
	[PMIF_MONITOR_RECORD_23_2] =		0x06BC,
	[PMIF_MONITOR_RECORD_23_3] =		0x06C0,
	[PMIF_MONITOR_RECORD_23_4] =		0x06C4,
	[PMIF_MONITOR_RECORD_24_0] =		0x06C8,
	[PMIF_MONITOR_RECORD_24_1] =		0x06CC,
	[PMIF_MONITOR_RECORD_24_2] =		0x06D0,
	[PMIF_MONITOR_RECORD_24_3] =		0x06D4,
	[PMIF_MONITOR_RECORD_24_4] =		0x06D8,
	[PMIF_MONITOR_RECORD_25_0] =		0x06DC,
	[PMIF_MONITOR_RECORD_25_1] =		0x06E0,
	[PMIF_MONITOR_RECORD_25_2] =		0x06E4,
	[PMIF_MONITOR_RECORD_25_3] =		0x06E8,
	[PMIF_MONITOR_RECORD_25_4] =		0x06EC,
	[PMIF_MONITOR_RECORD_26_0] =		0x06F0,
	[PMIF_MONITOR_RECORD_26_1] =		0x06F4,
	[PMIF_MONITOR_RECORD_26_2] =		0x06F8,
	[PMIF_MONITOR_RECORD_26_3] =		0x06FC,
	[PMIF_MONITOR_RECORD_26_4] =		0x0700,
	[PMIF_MONITOR_RECORD_27_0] =		0x0704,
	[PMIF_MONITOR_RECORD_27_1] =		0x0708,
	[PMIF_MONITOR_RECORD_27_2] =		0x070C,
	[PMIF_MONITOR_RECORD_27_3] =		0x0710,
	[PMIF_MONITOR_RECORD_27_4] =		0x0714,
	[PMIF_MONITOR_RECORD_28_0] =		0x0718,
	[PMIF_MONITOR_RECORD_28_1] =		0x071C,
	[PMIF_MONITOR_RECORD_28_2] =		0x0720,
	[PMIF_MONITOR_RECORD_28_3] =		0x0724,
	[PMIF_MONITOR_RECORD_28_4] =		0x0728,
	[PMIF_MONITOR_RECORD_29_0] =		0x072C,
	[PMIF_MONITOR_RECORD_29_1] =		0x0730,
	[PMIF_MONITOR_RECORD_29_2] =		0x0734,
	[PMIF_MONITOR_RECORD_29_3] =		0x0738,
	[PMIF_MONITOR_RECORD_29_4] =		0x073C,
	[PMIF_MONITOR_RECORD_30_0] =		0x0740,
	[PMIF_MONITOR_RECORD_30_1] =		0x0744,
	[PMIF_MONITOR_RECORD_30_2] =		0x0748,
	[PMIF_MONITOR_RECORD_30_3] =		0x074C,
	[PMIF_MONITOR_RECORD_30_4] =		0x0750,
	[PMIF_MONITOR_RECORD_31_0] =		0x0754,
	[PMIF_MONITOR_RECORD_31_1] =		0x0758,
	[PMIF_MONITOR_RECORD_31_2] =		0x075C,
	[PMIF_MONITOR_RECORD_31_3] =		0x0760,
	[PMIF_MONITOR_RECORD_31_4] =		0x0764,
	[PMIF_DEBUG_CTRL] =			0x0768,
	[PMIF_RESERVED_0] =			0x0770,
	[PMIF_SWINF_0_ACC] =			0x0C00,
	[PMIF_SWINF_0_WDATA_31_0] =		0x0C04,
	[PMIF_SWINF_0_WDATA_63_32] =		0x0C08,
	[PMIF_SWINF_0_RDATA_31_0] =		0x0C14,
	[PMIF_SWINF_0_RDATA_63_32] =		0x0C18,
	[PMIF_SWINF_0_VLD_CLR] =		0x0C24,
	[PMIF_SWINF_0_STA] =			0x0C28,
	[PMIF_SWINF_1_ACC] =			0x0C40,
	[PMIF_SWINF_1_WDATA_31_0] =		0x0C44,
	[PMIF_SWINF_1_WDATA_63_32] =		0x0C48,
	[PMIF_SWINF_1_RDATA_31_0] =		0x0C54,
	[PMIF_SWINF_1_RDATA_63_32] =		0x0C58,
	[PMIF_SWINF_1_VLD_CLR] =		0x0C64,
	[PMIF_SWINF_1_STA] =			0x0C68,
	[PMIF_SWINF_2_ACC] =			0x0C80,
	[PMIF_SWINF_2_WDATA_31_0] =		0x0C84,
	[PMIF_SWINF_2_WDATA_63_32] =		0x0C88,
	[PMIF_SWINF_2_RDATA_31_0] =		0x0C94,
	[PMIF_SWINF_2_RDATA_63_32] =		0x0C98,
	[PMIF_SWINF_2_VLD_CLR] =		0x0CA4,
	[PMIF_SWINF_2_STA] =			0x0CA8,
	[PMIF_SWINF_3_ACC] =			0x0CC0,
	[PMIF_SWINF_3_WDATA_31_0] =		0x0CC4,
	[PMIF_SWINF_3_WDATA_63_32] =		0x0CC8,
	[PMIF_SWINF_3_RDATA_31_0] =		0x0CD4,
	[PMIF_SWINF_3_RDATA_63_32] =		0x0CD8,
	[PMIF_SWINF_3_VLD_CLR] =		0x0CE4,
	[PMIF_SWINF_3_STA] =			0x0CE8,
};
static char d_log_buf[1280];
static struct spmi_controller *dbg_ctrl;
static int is_drv_attr;
static char *wp;

/*
 * Function : mtk_spmi_readl_d()
 * Description : mtk spmi controller read api
 * Parameter :
 * Return :
 */
static u32 mtk_spmi_readl_d(struct pmif *arb, enum spmi_regs reg)
{
	return readl(arb->spmimst_base + arb->spmimst_regs[reg]);
}

/*
 * Function : mtk_spmi_writel_d()
 * Description : mtk spmi controller write api
 * Parameter :
 * Return :
 */
static void mtk_spmi_writel_d(struct pmif *arb, u32 val,
		enum spmi_regs reg)
{
	writel(val, arb->spmimst_base + arb->spmimst_regs[reg]);
}

/* spmi & pmif debug mechanism */
inline void spmi_dump_pmif_busy_reg(void)
{
	struct pmif *arb = spmi_controller_get_drvdata(dbg_ctrl);
	unsigned int i = 0, offset = 0, tmp_dat = 0;
	unsigned int start = 0, end = 0, log_size = 0;

	start = arb->dbgregs[PMIF_INF_BUSY_STA]/4;
	end = arb->dbgregs[PMIF_OTHER_BUSY_STA_1]/4;

	log_size += sprintf(wp, "");
	for (i = start; i <= end; i++) {
		offset = arb->dbgregs[PMIF_INF_BUSY_STA] + (i * 4);
		tmp_dat = readl(arb->base + offset);
		log_size += sprintf(wp + log_size, "(0x%x)=0x%x ",
				offset, tmp_dat);

		if (i == 0)
			continue;
		if (i == 4)
			log_size += sprintf(wp + log_size, "\n[PMIF]");
	}
	log_size += sprintf(wp + log_size, "\n");
	pr_info("\n[PMIF] %s", wp);
	spmi_dump_pmif_swinf_reg();
}
inline void spmi_dump_pmif_swinf_reg(void)
{
	struct pmif *arb = spmi_controller_get_drvdata(dbg_ctrl);
	unsigned int i = 0, offset = 0, j = 0, tmp_dat = 0, log_size = 0;
	unsigned int swinf[4] = {0}, cmd[4] = {0}, rw[4] = {0};
	unsigned int slvid[4] = {0}, bytecnt[4] = {0}, adr[4] = {0};
	unsigned int wd_31_0[4] = {0};
	unsigned int rd_31_0[4] = {0};
	unsigned int err[4] = {0}, sbusy[4] = {0}, done[4] = {0};
	unsigned int qfillcnt[4] = {0}, qfreecnt[4] = {0}, qempty[4] = {0};
	unsigned int qfull[4] = {0}, req[4] = {0}, fsm[4] = {0}, en[4] = {0};

	for (i = 0; i < 4; i++) {
		offset = arb->dbgregs[PMIF_SWINF_0_ACC] + (i * 0x40);
		tmp_dat = readl(arb->base + offset);
		swinf[j] = i;
		cmd[j] = (tmp_dat & (0x3 << 30)) >> 30;
		rw[j] = (tmp_dat & (0x1 << 29)) >> 29;
		slvid[j] = (tmp_dat & (0xf << 24)) >> 24;
		bytecnt[j] = (tmp_dat & (0xf << 16)) >> 16;
		adr[j] = (tmp_dat & (0xffff << 0)) >> 0;
		j += 1;
	}
	j = 0;
	for (i = 0; i < 4; i++) {
		offset = arb->dbgregs[PMIF_SWINF_0_WDATA_31_0] + (i * 0x40);
		tmp_dat = readl(arb->base + offset);
		wd_31_0[j] = tmp_dat;
		j += 1;
	}
	j = 0;
	for (i = 0; i < 4; i++) {
		offset = arb->dbgregs[PMIF_SWINF_0_RDATA_31_0] + (i * 0x40);
		tmp_dat = readl(arb->base + offset);
		rd_31_0[j] = tmp_dat;
		j += 1;
	}
	j = 0;
	for (i = 0; i < 4; i++) {
		offset = arb->dbgregs[PMIF_SWINF_0_STA] + (i * 0x40);
		tmp_dat = readl(arb->base + offset);
		err[j] = (tmp_dat & (0x1 << 18)) >> 18;
		sbusy[j] = (tmp_dat & (0x1 << 17)) >> 17;
		done[j] = (tmp_dat & (0x1 << 15)) >> 15;
		qfillcnt[j] = (tmp_dat & (0xf << 11)) >> 11;
		qfreecnt[j] = (tmp_dat & (0xf << 7)) >> 7;
		qempty[j] = (tmp_dat & (0x1 << 6)) >> 6;
		qfull[j] = (tmp_dat & (0x1 << 5)) >> 5;
		req[j] = (tmp_dat & (0x1 << 4)) >> 4;
		fsm[j] = (tmp_dat & (0x7 << 1)) >> 1;
		en[j] = (tmp_dat & (0x1 << 0)) >> 0;
		j += 1;
	}
	log_size += sprintf(wp, "");
	for (i = 0; i < 4; i++) {
		if (rw[i] == 0) {
			log_size += sprintf(wp + log_size,
				"[swinf:%d, cmd:0x%x, rw:0x%x, slvid:%d ",
				swinf[i], cmd[i], rw[i], slvid[i]);
			log_size += sprintf(wp + log_size,
				"bytecnt:%d (read adr 0x%04x=0x%x)]\n[PMIF] ",
				bytecnt[i], adr[i], rd_31_0[i]);
			log_size += sprintf(wp + log_size,
				"[err:%d, sbusy:%d, done:%d, qfillcnt:%d ",
				err[i], sbusy[i], done[i], qfillcnt[i]);
			log_size += sprintf(wp + log_size,
				"qfreecnt:%d, qempty:%d, qfull:%d, req:%d ",
				qfreecnt[i], qempty[i], qfull[i], req[i]);
			log_size += sprintf(wp + log_size,
				"fsm:%d, en:%d]\n[PMIF] ", fsm[i], en[i]);
		} else {
			log_size += sprintf(wp + log_size,
				"[swinf:%d, cmd:0x%x, rw:0x%x, slvid:%d ",
				swinf[i], cmd[i], rw[i], slvid[i]);
			log_size += sprintf(wp + log_size,
				"bytecnt:%d (write adr 0x%04x=0x%x)]\n[PMIF] ",
				bytecnt[i], adr[i], wd_31_0[i]);
			log_size += sprintf(wp + log_size,
				"[err:%d, sbusy:%d, done:%d, qfillcnt:%d ",
				err[i], sbusy[i], done[i], qfillcnt[i]);
			log_size += sprintf(wp + log_size,
				"qfreecnt:%d, qempty:%d, qfull:%d, req:%d ",
				qfreecnt[i], qempty[i], qfull[i], req[i]);
			log_size += sprintf(wp + log_size,
				"fsm:%d, en:%d]\n[PMIF] ", fsm[i], en[i]);
		}
	}
	pr_info("\n[PMIF] %s", wp);
}

inline void spmi_dump_pmif_reg(void)
{
	struct pmif *arb = spmi_controller_get_drvdata(dbg_ctrl);
	unsigned int i = 0, offset = 0, tmp_dat = 0;
	unsigned int start = 0, end = 0, log_size = 0;

	start = arb->dbgregs[PMIF_INIT_DONE]/4;
	end = arb->dbgregs[PMIF_RESERVED_0]/4;

	log_size += sprintf(wp, "");
	for (i = start; i <= end; i++) {
		offset = arb->dbgregs[PMIF_INIT_DONE] + (i * 4);
		tmp_dat = readl(arb->base + offset);
		log_size += sprintf(wp + log_size,
			"(0x%x)=0x%x ", offset, tmp_dat);

		if (i == 0)
			continue;
		if (i % 8 == 0) {
			log_size += sprintf(wp + log_size,
				"\n[PMIF] ");
		}
		if (i % 0x28 == 0) {
			pr_info("\n[PMIF] %s", wp);
			if (!is_drv_attr)
				log_size = 0;
		}
	}
	pr_info("\n[PMIF] %s", wp);
	spmi_dump_pmif_swinf_reg();
}

inline void spmi_dump_pmif_record_reg(void)
{
	struct pmif *arb = spmi_controller_get_drvdata(dbg_ctrl);
	unsigned int i = 0, offset = 0, j = 0, tmp_dat = 0;
	unsigned int chan[32] = {0}, cmd[32] = {0}, rw[32] = {0};
	unsigned int slvid[32] = {0}, bytecnt[32] = {0}, adr[32] = {0};
	unsigned int wd_31_0[32] = {0}, log_size = 0;

	for (i = 0; i < 32; i++) {
		offset = arb->dbgregs[PMIF_MONITOR_RECORD_0_0] + (i * 0x14);
		tmp_dat = readl(arb->base + offset);
		chan[j] = (tmp_dat & (0xf8000000)) >> 27;
		cmd[j] = (tmp_dat & (0x3 << 25)) >> 25;
		rw[j] = (tmp_dat & (0x1 << 24)) >> 24;
		slvid[j] = (tmp_dat & (0xf << 20)) >> 20;
		bytecnt[j] = (tmp_dat & (0xf << 16)) >> 16;
		adr[j] = (tmp_dat & (0xffff << 0)) >> 0;
		j += 1;
	}
	j = 0;
	for (i = 0; i < 32; i++) {
		offset = arb->dbgregs[PMIF_MONITOR_RECORD_0_1] + (i * 0x14);
		tmp_dat = readl(arb->base + offset);
		wd_31_0[j] = tmp_dat;
		j += 1;
	}

	log_size += sprintf(wp, "");
	for (i = 0; i < 32; i++) {
		log_size += sprintf(wp + log_size,
			"[swinf:%d, cmd:0x%x, rw:0x%x, slvid:%d ",
			chan[i], cmd[i], rw[i], slvid[i]);
		log_size += sprintf(wp + log_size,
			"bytecnt:%d (adr 0x%04x=0x%x)]\n[PMIF] ",
			bytecnt[i], adr[i], wd_31_0[i]);
		if (i % 0x4 == 0) {
			pr_info("\n[PMIF] %s", wp);
			/* if kernel dumped, reset log_size;adb do nothing */
			if (!is_drv_attr)
				log_size = 0;
		}
	}
	pr_info("\n[PMIF] %s", wp);
	spmi_dump_pmif_swinf_reg();

	/* clear record data and re-enable */
	writel(0x800, arb->base + arb->dbgregs[PMIF_MONITOR_CTRL]);
	writel(0x5, arb->base + arb->dbgregs[PMIF_MONITOR_CTRL]);
}
inline void spmi_dump_spmimst_reg(void)
{
	struct pmif *arb = spmi_controller_get_drvdata(dbg_ctrl);
	unsigned int i = 0, offset = 0, tmp_dat = 0;
	unsigned int start = 0, end = 0, log_size = 0;

	start = arb->spmimst_regs[SPMI_OP_ST_CTRL]/4;
	end = arb->spmimst_regs[SPMI_REC4]/4;

	log_size += sprintf(wp, "");
	for (i = start; i <= end; i++) {
		offset = arb->spmimst_regs[SPMI_OP_ST_CTRL] + (i * 4);
		tmp_dat = readl(arb->spmimst_base + offset);
		log_size += sprintf(wp + log_size,
			"(0x%x)=0x%x ", offset, tmp_dat);

		if (i == 0)
			continue;
		if (i % 8 == 0) {
			log_size += sprintf(wp + log_size,
					"\n[SPMIMST] ");
		}
	}
#if SPMI_RCS_SUPPORT
	offset = arb->spmimst_regs[SPMI_DEC_DBG];
	tmp_dat = readl(arb->spmimst_base + offset);
	log_size += sprintf(wp + log_size,
		"(0x%x)=0x%x ", offset, tmp_dat);
#endif
	offset = arb->spmimst_regs[SPMI_MST_DBG];
	tmp_dat = readl(arb->spmimst_base + offset);
	log_size += sprintf(wp + log_size,
		"(0x%x)=0x%x\n", offset, tmp_dat);
	pr_info("\n[SPMIMST] %s", wp);
}

inline void spmi_dump_mst_record_reg(struct pmif *arb)
{
	unsigned int log_size = 0;

	log_size += sprintf(wp,
			"[0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x\n",
			arb->spmimst_regs[SPMI_OP_ST_STA],
			mtk_spmi_readl_d(arb, SPMI_OP_ST_STA),
			arb->spmimst_regs[SPMI_REC0],
			mtk_spmi_readl_d(arb, SPMI_REC0),
			arb->spmimst_regs[SPMI_REC1],
			mtk_spmi_readl_d(arb, SPMI_REC1),
			arb->spmimst_regs[SPMI_REC2],
			mtk_spmi_readl_d(arb, SPMI_REC2));
	log_size += sprintf(wp + log_size,
			"[SPMIMST] [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x\n",
			arb->spmimst_regs[SPMI_REC3],
			mtk_spmi_readl_d(arb, SPMI_REC3),
			arb->spmimst_regs[SPMI_REC4],
			mtk_spmi_readl_d(arb, SPMI_REC4),
			arb->spmimst_regs[SPMI_MST_DBG],
			mtk_spmi_readl_d(arb, SPMI_MST_DBG));
	pr_info("\n[SPMIMST] %s", wp);
}

inline void spmi_dump_slv_record_reg(u8 sid)
{
	struct pmif *arb = spmi_controller_get_drvdata(dbg_ctrl);
	u8 rdata1 = 0, rdata2 = 0, rdata3 = 0, rdata4 = 0;
	unsigned int offset, i, j = 0, log_size = 0, ret = 0;

	log_size += sprintf(wp,  "");
	/* log sequence, idx 0->1->2->3->0 */
	for (offset = 0x34; offset < 0x50; offset += 4) {
		ret = arb->read_cmd(dbg_ctrl, SPMI_CMD_EXT_READL,
			sid, (MT6315_PLT0_ID_ANA_ID + offset), &rdata1, 1);
		ret = arb->read_cmd(dbg_ctrl, SPMI_CMD_EXT_READL,
			sid, (MT6315_PLT0_ID_ANA_ID + offset + 1), &rdata2, 1);
		ret = arb->read_cmd(dbg_ctrl, SPMI_CMD_EXT_READL,
			sid, (MT6315_PLT0_ID_ANA_ID + offset + 2), &rdata3, 1);
		ret = arb->read_cmd(dbg_ctrl, SPMI_CMD_EXT_READL,
			sid, (MT6315_PLT0_ID_ANA_ID + offset + 3), &rdata4, 1);
		if ((offset + 3) == 0x37) {
			i = (rdata4 & 0xc) >> 2;
			if (i == 0)
				log_size += sprintf(wp + log_size,
					"slvid:%d DBG. Last cmd idx:0x3\n",
					sid);
			else {
				log_size += sprintf(wp + log_size,
					"slvid:%d DBG. Last cmd idx:0x%x\n",
					sid, ((rdata4 & 0xc) >> 2) - 1);
			}

		}
		/*
		 *log_size += sprintf(wp + log_size,
		 *"[0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x ",
		 *offset, rdata1, (offset + 1), rdata2,
		 *(offset + 2), rdata3, (offset + 3), rdata4);
		 */

		log_size += sprintf(wp + log_size,
			"Idx:%d slvid:%d Type:0x%x, [0x%x]=0x%x\n", j,
			sid, (rdata4 & 0x3),
			(rdata2 << 0x8) | rdata1, rdata3);
		if (j <= 3)
			j++;

	}
	pr_info("\n[SPMISLV] %s", wp);
}

static ssize_t dump_rec_pmif_show(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		pr_notice("%s() *buf is NULL\n", __func__);
		return -EINVAL;
	}

	is_drv_attr = 1;
	wp = buf;
	spmi_dump_pmif_record_reg();
	pr_info("%s() buf_size:%d\n", __func__, (int)strlen(buf));

	wp = d_log_buf;
	is_drv_attr = 0;
	return strlen(buf);
}
static ssize_t dump_rec_spmimst_show(struct device_driver *ddri, char *buf)
{
	struct pmif *arb = spmi_controller_get_drvdata(dbg_ctrl);

	if (buf == NULL) {
		pr_notice("%s() *buf is NULL\n", __func__);
		return -EINVAL;
	}

	is_drv_attr = 1;
	wp = buf;
	spmi_dump_mst_record_reg(arb);
	pr_info("%s() buf_size:%d\n", __func__, (int)strlen(buf));

	wp = d_log_buf;
	is_drv_attr = 0;
	return strlen(buf);
}
static u8 gsid;
static ssize_t dump_rec_pmic_show(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		pr_notice("[%s] *buf is NULL!\n",  __func__);
		return -EINVAL;
	}
	is_drv_attr = 1;
	wp = buf;
	spmi_dump_slv_record_reg(gsid);
	wp = d_log_buf;
	is_drv_attr = 0;

	return strlen(buf);
}
static ssize_t dump_rec_pmic_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int ret = 0, sid;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!\n", __func__);
		return -EINVAL;
	}

	ret = kstrtoint(buf, 10, &sid);
	gsid = sid;
	if (ret < 0) {
		pr_notice("%s() kstrtoint failed! ret:%d\n", __func__, ret);
		return ret;
	}
	pr_info("%s() get slvid:%d pmic dump\n", __func__, gsid);

	return count;
}
static u32 goffset;
static u32 gvalue;
static ssize_t set_reg_show(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		pr_notice("[%s] *buf is NULL!\n",  __func__);
		return -EINVAL;
	}
	sprintf(buf, "[%s] [0x%x]=0x%x\n", __func__, goffset, gvalue);

	return strlen(buf);
}
static ssize_t set_reg_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	struct pmif *arb = spmi_controller_get_drvdata(dbg_ctrl);
	int ret = 0;
	u32 offset = 0;
	u32 value = 0;

	if (strlen(buf) < 3) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

	ret = sscanf(buf, "0x%x,0x%x", &offset, &value);
	if (ret < 0)
		return ret;

	pr_info("%s() set offset[0x%x]=0x%x\n", __func__, offset, value);

	if (offset > arb->regs[PMIF_SWINF_3_STA])
		pr_notice("%s() Illegal offset[0x%x]!!\n", __func__, offset);
	else
		writel(value, arb->base + offset);

	goffset = offset;
	gvalue = readl(arb->base + offset);

	return count;
}
static DRIVER_ATTR_RO(dump_rec_pmif);
static DRIVER_ATTR_RO(dump_rec_spmimst);
static DRIVER_ATTR_RW(dump_rec_pmic);
static DRIVER_ATTR_RW(set_reg);

static struct driver_attribute *spmi_pmif_attr_list[] = {
	&driver_attr_dump_rec_pmif,
	&driver_attr_dump_rec_spmimst,
	&driver_attr_dump_rec_pmic,
	&driver_attr_set_reg,
};

int spmi_pmif_create_attr(struct device_driver *driver)
{
	int idx, err;
	int num = ARRAY_SIZE(spmi_pmif_attr_list);

	if (driver == NULL)
		return -EINVAL;
	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, spmi_pmif_attr_list[idx]);
		if (err) {
			pr_notice("%s() driver_create_file %s err:%d\n",
			__func__, spmi_pmif_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
int spmi_pmif_dbg_init(struct spmi_controller *ctrl)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);

	dbg_ctrl = ctrl;
	wp = d_log_buf;
	arb->dbgregs = mt6xxx_pmif_dbg_regs;

	return 0;
}

