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
#ifndef _MTK_DT_H_
#define _MTK_DT_H_

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define DT_PETRUSP

#ifdef DT_PETRUSP
#define DT_NUM 8
#define DT_B_START_ID 4
#define DT_END_ID 7
#define DT_PINCTL_BIT 3
#define DT_THR_START_BIT 0
#define DT_THR_END_BIT 2
#define DT_DEBUG_BIT 16

#endif

/************************************************
 * Register addr, offset, bits range
 ************************************************/
#define DT_BASE 0x0C530000
#define DT_OFFSET 0xCD14
#define DT_CONFIG_REG	(DT_BASE + 0xCD14)
#define DT_MCUSYS_RESERVED_REG1 (DT_BASE + 0xFFE4)

/************************************************
 * config enum
 ************************************************/
enum DT_RW {
	DT_RW_READ,
	DT_RW_WRITE,
	DT_RW_PINCTL_READ,
	DT_RW_PINCTL_WRITE,
	DT_RW_TOG_READ,
};

enum DT_TRIGGER_STAGE {
	DT_TRIGGER_STAGE_PROBE,
	DT_TRIGGER_STAGE_SUSPEND,
	DT_TRIGGER_STAGE_RESUME,

	NR_DT_TRIGGER_STAGE,
};

enum DT_THR {
	p0_disp_red,
	p12_disp_red,
	p25_disp_red,
	p30_disp_red,
	p35_disp_red,
	p40_disp_red,
	p75_disp_red,
	p75_disp_red_pinctl_is_p100,

};

enum DT_BITS_DEFINED {
	DT_PINCTL = 3,
	DT_BCORE4 = 4,
	DT_BCORE5 = 5,
	DT_BCORE6 = 6,
	DT_BCORE7 = 7,
};

/************************************************
 * Func prototype
 ************************************************/
int dt_resume(struct platform_device *pdev);
int dt_suspend(struct platform_device *pdev, pm_message_t state);
int dt_probe(struct platform_device *pdev);
int dt_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int dt_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum DT_TRIGGER_STAGE dt_tri_stage);
void dt_save_memory_info(char *buf, unsigned long long ptp3_mem_size);
/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_DT_CONTROL				0xC200051B
#else
#define MTK_SIP_KERNEL_DT_CONTROL				0x8200051B
#endif
#endif //_MTK_DT_H_
