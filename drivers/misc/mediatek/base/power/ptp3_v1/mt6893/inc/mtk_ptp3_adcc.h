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
#ifndef _MTK_ADCC_H_
#define _MTK_ADCC_H_

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define NR_ADCC_CPU 4
#define ADCC_CPU_START_ID 4
#define ADCC_CPU_END_ID 7

/************************************************
 * Register addr, offset, bits range
 ************************************************/

/************************************************
 * config & struct
 ************************************************/
enum ADCC_INDEX {
	ADCC_ENABLE,
	ADCC_SET_SHAPER,
	ADCC_SET_CALIN,
	ADCC_SET_DCDSELECT,
	ADCC_SET_DCTARGET,
	ADCC_DUMP_INFO,

	NR_ADCC_INDEX
};

enum ADCC_TRIGGER_STAGE {
	ADCC_TRIGGER_STAGE_PROBE,
	ADCC_TRIGGER_STAGE_SUSPEND,
	ADCC_TRIGGER_STAGE_RESUME,

	NR_ADCC_TRIGGER_STAGE,
};

/************************************************
 * Func prototype
 ************************************************/
int adcc_resume(struct platform_device *pdev);
int adcc_suspend(struct platform_device *pdev, pm_message_t state);
int adcc_probe(struct platform_device *pdev);
int adcc_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int adcc_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum ADCC_TRIGGER_STAGE adcc_tri_stage);
void adcc_save_memory_info(char *buf, unsigned long long ptp3_mem_size);

/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_PTP3_CONTROL				0xC2000522
#else
#define MTK_SIP_KERNEL_PTP3_CONTROL				0x82000522
#endif

#endif //_MTK_ADCC_H_
