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

#define IGLRE_NUM 4
#define DBG_CONTROL 0x308

/************************************************
 * config enum
 ************************************************/
enum IGLRE_KEY {
	IG_CFG,
	IG_R_CFG,
	LRE_CFG,
	LRE_R_CFG,
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
	/* DBG_CONTROL 0x0C532308, 2B08, 3308, 3B08 */
	unsigned int IGLRE_DBG_CONTROL:16;   /* [15:0] */
	unsigned int mem_ig_en:10;         /* [25:16] */
	unsigned int mem_lre_en:6;              /* [31:26] */
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

