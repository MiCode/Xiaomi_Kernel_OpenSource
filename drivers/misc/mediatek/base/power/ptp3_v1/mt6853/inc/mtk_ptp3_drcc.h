// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_DRCC_H_
#define _MTK_DRCC_H_

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define DRCC_MOUTON //???

#ifdef DRCC_MOUTON
#define DRCC_NUM 8
#define DRCC_L_NUM 6
#endif

/************************************************
 * Register addr, offset, bits range
 ************************************************/
#define DRCC_BASE 0x0C530000
#define CPU0_DRCC_A0_CONFIG	(DRCC_BASE + 0xB000)
#define CPU0_DRCC_CFG_REG0	(DRCC_BASE + 0x0280)
#define CPU0_DRCC_CFG_REG2	(DRCC_BASE + 0x0288)
#define CPU0_DRCC_CFG_REG3	(DRCC_BASE + 0x028C)

#define CPU1_DRCC_A0_CONFIG	(DRCC_BASE + 0xB200)
#define CPU1_DRCC_CFG_REG0	(DRCC_BASE + 0x0A80)
#define CPU1_DRCC_CFG_REG2	(DRCC_BASE + 0x0A88)
#define CPU1_DRCC_CFG_REG3	(DRCC_BASE + 0x0A8C)

#define CPU2_DRCC_A0_CONFIG	(DRCC_BASE + 0xB400)
#define CPU2_DRCC_CFG_REG0	(DRCC_BASE + 0x1280)
#define CPU2_DRCC_CFG_REG2	(DRCC_BASE + 0x1288)
#define CPU2_DRCC_CFG_REG3	(DRCC_BASE + 0x128C)

#define CPU3_DRCC_A0_CONFIG	(DRCC_BASE + 0xB600)
#define CPU3_DRCC_CFG_REG0	(DRCC_BASE + 0x1A80)
#define CPU3_DRCC_CFG_REG2	(DRCC_BASE + 0x1A88)
#define CPU3_DRCC_CFG_REG3	(DRCC_BASE + 0x1A8C)

#define CPU4_DRCC_A0_CONFIG	(DRCC_BASE + 0xB800)
#define CPU4_DRCC_CFG_REG0	(DRCC_BASE + 0x2280)
#define CPU4_DRCC_CFG_REG2	(DRCC_BASE + 0x2288)
#define CPU4_DRCC_CFG_REG3	(DRCC_BASE + 0x228C)

#define CPU5_DRCC_A0_CONFIG	(DRCC_BASE + 0xBA00)
#define CPU5_DRCC_CFG_REG0	(DRCC_BASE + 0x2A80)
#define CPU5_DRCC_CFG_REG2	(DRCC_BASE + 0x2A88)
#define CPU5_DRCC_CFG_REG3	(DRCC_BASE + 0x2A8C)

#define CPU6_DRCC_A0_CONFIG	(DRCC_BASE + 0xBC00)
#define CPU6_DRCC_CFG_REG0	(DRCC_BASE + 0x3280)
#define CPU6_DRCC_CFG_REG2	(DRCC_BASE + 0x3288)
#define CPU6_DRCC_CFG_REG3	(DRCC_BASE + 0x328C)

#define CPU7_DRCC_A0_CONFIG	(DRCC_BASE + 0xBE00)
#define CPU7_DRCC_CFG_REG0	(DRCC_BASE + 0x3A80)
#define CPU7_DRCC_CFG_REG2	(DRCC_BASE + 0x3A88)
#define CPU7_DRCC_CFG_REG3	(DRCC_BASE + 0x3A8C)

#define DRCC_ENABLE			0
#define DRCC_TRIGEN			0x0
#define DRCC_TRIGSEL		0x0
#define DRCC_COUNTEN		0x0
#define DRCC_COUNTSEL		0x0
#define DRCC_STANDBYWFIL2_MASK	0x0
#define DRCC_MODE			0
#define DRCC_CODE			0x3A	/* 58 */
#define DRCC_HWGATEPCT		0x3		/* 50 % */
#define DRCC_VERFFILT		0x1		/* 2 Mhz */
#define DRCC_AUTOCALIBDELAY	0x0
#define DRCC_FORCETRIM		0x0
#define DRCC_FORCETRIMEN	0x0
#define DRCC_DISABLEAUTOPRTDURCALIB	0x0

/************************************************
 * config enum
 ************************************************/
enum DRCC_GROUP {
	DRCC_GROUP_ENABLE,
	DRCC_GROUP_TRIG,
	DRCC_GROUP_CG_CNT_EN,
	DRCC_GROUP_CMP_CNT_EN,
	DRCC_GROUP_MODE,
	DRCC_GROUP_CODE,
	DRCC_GROUP_HWGATEOCT,
	DRCC_GROUP_VREFFILT,
	DRCC_GROUP_AUTOCALIBDELAY,
	DRCC_GROUP_READ,

	NR_DRCC_GROUP,
};

enum DRCC_TRIGGER_STAGE {
	DRCC_TRIGGER_STAGE_PROBE,

	NR_DRCC_TRIGGER_STAGE,
};

/************************************************
 * Func prototype
 ************************************************/
int drcc_resume(struct platform_device *pdev);
int drcc_suspend(struct platform_device *pdev, pm_message_t state);
int drcc_probe(struct platform_device *pdev);
int drcc_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int drcc_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum DRCC_TRIGGER_STAGE drcc_tri_stage);
void drcc_save_memory_info(char *buf, unsigned long long ptp3_mem_size);

/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_DRCC_CONTROL				0xC200051B
#else
#define MTK_SIP_KERNEL_DRCC_CONTROL				0x8200051B
#endif
#endif //_MTK_DRCC_H_
