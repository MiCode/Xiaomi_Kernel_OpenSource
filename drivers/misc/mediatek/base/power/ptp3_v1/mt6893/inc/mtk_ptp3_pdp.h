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
#ifndef _MTK_PDP_H_
#define _MTK_PDP_H_

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define PDP_PETRUSP

#ifdef PDP_PETRUSP
#define PDP_NUM 8
#define PDP_B_START_ID 4
#define PDP_END_ID 7
#define PDP_PINCTL_BIT 3
#define PDP_DEBUG_BIT 16

#endif

/************************************************
 * Register addr, offset, bits range
 ************************************************/
#define PDP_BASE 0x0C530000
#define PDP_OFFSET 0xCD14
#define PDP_CONFIG_REG	(PDP_BASE + 0xCD14)
#define PDP_MCUSYS_RESERVED_REG0 (PDP_BASE + 0xFFE0)

/************************************************
 * config enum
 ************************************************/
enum PDP_RW {
	PDP_RW_READ,
	PDP_RW_WRITE,
	PDP_RW_PINCTL_READ,
	PDP_RW_PINCTL_WRITE,
	PDP_RW_TOG_READ,
};

enum PDP_TRIGGER_STAGE {
	PDP_TRIGGER_STAGE_PROBE,
	PDP_TRIGGER_STAGE_SUSPEND,
	PDP_TRIGGER_STAGE_RESUME,

	NR_PDP_TRIGGER_STAGE,
};

enum PDP_BITS_DEFINE {
	PDP_DIS_IQ,
	PDP_DIS_LSINH,
	PDP_DIS_SPEC,
	PDPPINCTL,
	PDP_BCORE4,
	PDP_BCORE5,
	PDP_BCORE6,
	PDP_BCORE7,

};

enum PDP_SUBFEATURE_VALUE {
	PDP_ALLFEATURE_ON,
	PDP_DISABLE_DIS_IQ,
	PDP_DISABLE_DIS_LSINH,
	PDP_DISABLE_DIS_IQ_DIS_LSINH,
	PDP_DISABLE_DIS_SPEC,
	PDP_DISABLE_DIS_IQ_DIS_SPEC,
	PDP_DISABLE_DIS_LSINH_DIS_SPEC,
	PDP_ALLFEATURE_OFF,

};

enum PDP_CONFIG_DEFINE {
	PDP_ALL_OFF,
	PDP_ALL_ON,
	PDP_DISABLE_SUB2,
};


/************************************************
 * Func prototype
 ************************************************/
int pdp_resume(struct platform_device *pdev);
int pdp_suspend(struct platform_device *pdev, pm_message_t state);
int pdp_probe(struct platform_device *pdev);
int pdp_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int pdp_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum PDP_TRIGGER_STAGE pdp_tri_stage);
void pdp_save_memory_info(char *buf, unsigned long long ptp3_mem_size);
/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_PDP_CONTROL				0xC200051B
#else
#define MTK_SIP_KERNEL_PDP_CONTROL				0x8200051B
#endif
#endif //_MTK_PDP_H_
