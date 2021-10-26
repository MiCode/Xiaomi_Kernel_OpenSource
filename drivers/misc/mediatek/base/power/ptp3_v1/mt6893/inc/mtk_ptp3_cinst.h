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
#define NR_CINST_CPU 4
#define CINST_CPU_START_ID 4
#define CINST_CPU_END_ID 7

/************************************************
 * Register addr, offset, max
 ************************************************/
#define CINST_REG_B_CNT 3
#define CINST_BASE 0x0C530000
#define CPU4_CINST_A0_CONFIG	(CINST_BASE + 0xB830)
#define CPU4_CINST_CFG_CONFIG	(CINST_BASE + 0x2000)
#define LS_DIDT_CONTROL_OFFSET	0x314
#define VX_DIDT_CONTROL_OFFSET	0x318
#define CINST_CONTROL_OFFSET	0xB830

#define CINST_CREDIT_MAX 31
#define CINST_PERIOD_MAX 7


/************************************************
 * config enum
 ************************************************/
enum CINST_GROUP {
	CINST_GROUP_LS_ENABLE,
	CINST_GROUP_LS_ENABLE_R,
	CINST_GROUP_LS_CREDIT,
	CINST_GROUP_LS_CREDIT_R,
	CINST_GROUP_LS_PERIOD,
	CINST_GROUP_LS_PERIOD_R,
	CINST_GROUP_VX_ENABLE,
	CINST_GROUP_VX_ENABLE_R,
	CINST_GROUP_VX_CREDIT,
	CINST_GROUP_VX_CREDIT_R,
	CINST_GROUP_VX_PERIOD,
	CINST_GROUP_VX_PERIOD_R,
	CINST_GROUP_CFG,
	CINST_GROUP_READ,
	CINST_GROUP_INIT,
	CINST_GROUP_LS_LOW_EN,
	CINST_GROUP_LS_LOW_EN_R,
	CINST_GROUP_LS_LOW_PERIOD,
	CINST_GROUP_LS_LOW_PERIOD_R,
	CINST_GROUP_VX_LOW_EN,
	CINST_GROUP_VX_LOW_EN_R,
	CINST_GROUP_VX_LOW_PERIOD,
	CINST_GROUP_VX_LOW_PERIOD_R,
	CINST_GROUP_LS_CONST_EN,
	CINST_GROUP_LS_CONST_EN_R,
	CINST_GROUP_VX_CONST_EN,
	CINST_GROUP_VX_CONST_EN_R,

	NR_CINST_GROUP,
};

enum CINST_TRIGGER_STAGE {
	CINST_TRIGGER_STAGE_PROBE,
	CINST_TRIGGER_STAGE_SUSPEND,
	CINST_TRIGGER_STAGE_RESUME,

	NR_CINST_TRIGGER_STAGE,
};

struct cinst_class {
	/* DIDT_REG */
	unsigned int cinst_rg_vx_cfg_pipe_issue_sel:1;		/* [0:0] */
	unsigned int cinst_rg_ls_cfg_pipe_issue_sel:1;		/* [1:1] */
	unsigned int cinst_rg_vx_ctrl_en:1;		/* [2:2] */
	unsigned int cinst_rg_ls_ctrl_en:29;		/* [3:3] */

	/* LS_DIDT_CONTROL */
	unsigned int cinst_ls_cfg_credit:8;			/* [4:0] */
	unsigned int cinst_ls_cfg_period:8;			/* [10:8] */
	unsigned int cinst_ls_cfg_low_period:8;		/* [18:16] */
	unsigned int cinst_ls_cfg_low_freq_en:1;	/* [24:24] */
	unsigned int cinst_ls_low_freq:1;			/* [25:25] */
	unsigned int cinst_ls_ctrl_en_local:1;		/* [26:26] */
	unsigned int cinst_ls_cfg_const_en:5;		/* [27:27] */

	/* VX_DIDT_CONTROL */
	unsigned int cinst_vx_cfg_credit:8;			/* [4:0] */
	unsigned int cinst_vx_cfg_period:8;			/* [10:8] */
	unsigned int cinst_vx_cfg_low_period:8;		/* [18:16] */
	unsigned int cinst_vx_cfg_low_freq_en:1;	/* [24:24] */
	unsigned int cinst_vx_low_freq:1;			/* [25:25] */
	unsigned int cinst_vx_ctrl_en_local:1;		/* [26:26] */
	unsigned int cinst_vx_cfg_const_en:5;		/* [27:27] */
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
#define MTK_SIP_KERNEL_CINST_CONTROL			0xC200051B
#else
#define MTK_SIP_KERNEL_CINST_CONTROL			0x8200051B
#endif
#endif //_MTK_CINST_H_
