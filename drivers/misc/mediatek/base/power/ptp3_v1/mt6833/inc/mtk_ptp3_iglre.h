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
#ifndef _MTK_IGLRE_H_
#define _MTK_IGLRE_H_

#define IGLRE_NUM 2
#define IGLRE_CONTROL 0x34
#define SPWR_CONTROL 0x23C

/************************************************
 * config enum
 ************************************************/
enum IGLRE_KEY {
	IG_CFG,
	LRE_CFG,
	BYTE_CFG,
	RCAN_CFG,

	IG_R_CFG,
	LRE_R_CFG,
	BYTE_R_CFG,
	RCAN_R_CFG,

	IGLRE_R,
	NR_IGLRE,
};

enum IGLRE_TRIGGER_STAGE {
	IGLRE_TRIGGER_STAGE_PROBE,
	IGLRE_TRIGGER_STAGE_SUSPEND,
	IGLRE_TRIGGER_STAGE_RESUME,

	NR_IGLRE_TRIGGER_STAGE,
};

struct iglre_class {
	/* IGLRE_CONTROL 0x0C53BC34, BE34 */
	unsigned int mem_ig_en:1;	/* [0] */
	unsigned int mem_lre_en_if_itag:1;	/* [1] */
	unsigned int mem_lre_en_ls_pf_pht:1;	/* [2] */
	unsigned int mem_lre_en_ls_data:1;	/* [3] */
	unsigned int mem_lre_en_ls_tag:1;	/* [4] */
	unsigned int MEM_LRE_REV:27;		/* [5:31] */

	/* BYTE_CONTROL 0x0C53323C, 3A3C */
	unsigned int sram_ig_low_pwr_sel:1;	/* [0] */
	unsigned int l1data_sram_ig_en:2;	/* [1:2] */
	unsigned int l2tag_sram_ig_en:2;	/* [3:4] */
	unsigned int sram_ig_en_ctrl:1;		/* [5] */
	unsigned int sram_byte_en:1;		/* [6] */
	unsigned int sram_rcan_en:1;		/* [7] */
	unsigned int SRAM_PWR_REV:24;		/* [8:31] */
};

/************************************************
 * Func prototype
 ************************************************/
int iglre_resume(struct platform_device *pdev);
int iglre_suspend(struct platform_device *pdev, pm_message_t state);
int iglre_probe(struct platform_device *pdev);
int iglre_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int iglre_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum IGLRE_TRIGGER_STAGE iglre_tri_stage);
void iglre_save_memory_info(char *buf, unsigned long long ptp3_mem_size);
#endif //_MTK_IGLRE_H_

