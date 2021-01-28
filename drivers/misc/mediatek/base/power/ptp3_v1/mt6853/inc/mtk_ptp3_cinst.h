/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MTK_CINST_H_
#define _MTK_CINST_H_

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define NR_CINST_CPU 2
#define CINST_CPU_START_ID 6
#define CINST_CPU_END_ID 7

/************************************************
 * Register addr, offset, bits range
 ************************************************/

/* CINST AO control register */
#define CINST_MCUSYS_REG_BASE_ADDR	(0x0C530000)
#define CINST_CPU6_DIDT_REG	(CINST_MCUSYS_REG_BASE_ADDR + 0xBC30)
#define CINST_CPU7_DIDT_REG	(CINST_MCUSYS_REG_BASE_ADDR + 0xBE30)

static const unsigned int CINST_CPU_AO_BASE[NR_CINST_CPU] = {
	CINST_CPU6_DIDT_REG,
	CINST_CPU7_DIDT_REG
};

#define CINST_BITS_LS_CTRL_EN		1
#define CINST_BITS_LS_INDEX_SEL	1
#define CINST_BITS_VX_CTRL_EN		1

#define CINST_SHIFT_LS_CTRL_EN	15
#define CINST_SHIFT_LS_INDEX_SEL	17
#define CINST_SHIFT_VX_CTRL_EN	18

/* CINST control register */
#define CINST_CPU6_CONFIG_REG	(0x0C533000)
#define CINST_CPU7_CONFIG_REG	(0x0C533800)

static const unsigned int CINST_CPU_BASE[NR_CINST_CPU] = {
	CINST_CPU6_CONFIG_REG,
	CINST_CPU7_CONFIG_REG
};


#define CINST_DIDT_CONTROL		(0x480)

#define CINST_BITS_LS_CFG_CREDIT			5
#define CINST_BITS_LS_CFG_PERIOD			3
#define CINST_BITS_LS_CFG_LOW_PERIOD		3
#define CINST_BITS_LS_CFG_LOW_FREQ_EN		1
#define CINST_BITS_VX_CFG_CREDIT			5
#define CINST_BITS_VX_CFG_PERIOD			3
#define CINST_BITS_VX_CFG_LOW_PERIOD		3
#define CINST_BITS_VX_CFG_LOW_FREQ_EN		1
#define CINST_BITS_CONST_MODE				1

#define CINST_SHIFT_LS_CFG_CREDIT			0
#define CINST_SHIFT_LS_CFG_PERIOD			5
#define CINST_SHIFT_LS_CFG_LOW_PERIOD		8
#define CINST_SHIFT_LS_CFG_LOW_FREQ_EN	11
#define CINST_SHIFT_VX_CFG_CREDIT			16
#define CINST_SHIFT_VX_CFG_PERIOD			21
#define CINST_SHIFT_VX_CFG_LOW_PERIOD		24
#define CINST_SHIFT_VX_CFG_LOW_FREQ_EN	27
#define CINST_SHIFT_CONST_MODE			31


/************************************************
 * config enum
 ************************************************/
enum CINST_RW {
	CINST_RW_READ,
	CINST_RW_WRITE,
	CINST_RW_REG_READ,
	CINST_RW_REG_WRITE,

	NR_CINST_RW,
};

enum CINST_CHANNEL {
	CINST_CHANNEL_LS,
	CINST_CHANNEL_VX,

	NR_CINST_CHANNEL,
};

enum CINST_CFG {
	CINST_CFG_PERIOD,
	CINST_CFG_CREDIT,
	CINST_CFG_LOW_PWR_PERIOD,
	CINST_CFG_LOW_PWR_ENABLE,
	CINST_CFG_ENABLE,

	NR_CINST_CFG,
};

enum CINST_PARAM {

	CINST_PARAM_LS_PERIOD =
		CINST_CHANNEL_LS * NR_CINST_CFG,
	CINST_PARAM_LS_CREDIT,
	CINST_PARAM_LS_LOW_PWR_PERIOD,
	CINST_PARAM_LS_LOW_PWR_ENABLE,
	CINST_PARAM_LS_ENABLE,

	CINST_PARAM_VX_PERIOD =
		CINST_CHANNEL_VX * NR_CINST_CFG,
	CINST_PARAM_VX_CREDIT,
	CINST_PARAM_VX_LOW_PWR_PERIOD,
	CINST_PARAM_VX_LOW_PWR_ENABLE,
	CINST_PARAM_VX_ENABLE,

	CINST_PARAM_CONST_MODE,
	CINST_PARAM_LS_IDX_SEL,

	NR_CINST_PARAM,
};

enum CINST_TRIGGER_STAGE {
	CINST_TRIGGER_STAGE_PROBE,
	CINST_TRIGGER_STAGE_SUSPEND,
	CINST_TRIGGER_STAGE_RESUME,

	NR_CINST_TRIGGER_STAGE,
};

/************************************************
 * Func prototype
 ************************************************/
int cinst_resume(struct platform_device *pdev);
int cinst_suspend(struct platform_device *pdev, pm_message_t state);
int cinst_probe(struct platform_device *pdev);
int cinst_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int cinst_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum CINST_TRIGGER_STAGE cinst_tri_stage);
void cinst_save_memory_info(char *buf, unsigned long long ptp3_mem_size);

/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_CINST_CONTROL			0xC200051A
#else
#define MTK_SIP_KERNEL_CINST_CONTROL			0x8200051A
#endif
#endif //_MTK_CINST_H_
