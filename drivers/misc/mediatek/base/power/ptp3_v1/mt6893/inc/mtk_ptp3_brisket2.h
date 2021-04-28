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
#ifndef _MTK_BRISKET2_H_
#define _MTK_BRISKET2_H_

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define NR_BRISKET2_CPU 4
#define BRISKET2_CPU_START_ID 4
#define BRISKET2_CPU_END_ID 7

/************************************************
 * Register addr, offset, bits range
 ************************************************/
/* BRISKET2 List Macro*/
#define SamplerEn "SamplerEn"
#define DrccGateEn "DrccGateEn"
#define ConfigComplete "ConfigComplete"
#define FllEn "FllEn"
#define FllClkOutSelect "FllClkOutSelect"
#define FllSlowReqEn "FllSlowReqEn"
#define FllSlowReqGateEn "FllSlowReqGateEn"
#define CttEn "CttEn"
#define TestMode "TestMode"
#define GlobalEventEn "GlobalEventEn"
#define SafeFreqReqOverride "SafeFreqReqOverride"
#define SafeFreqEn "SafeFreqEn"
#define	SafeFreqBypass "SafeFreqBypass"

#define V101 "V101"

/* BRISKET2 Cfg metadata offset */
#define BRISKET2_CFG_OFFSET_VALUE 0
#define BRISKET2_CFG_OFFSET_OPTION 20
#define BRISKET2_CFG_OFFSET_CPU 28

/* BRISKET2 Cfg metadata bitmask */
#define BRISKET2_CFG_BITMASK_VALUE 0xFFFF
#define BRISKET2_CFG_BITMASK_OPTION 0xFF00000
#define BRISKET2_CFG_BITMASK_CPU 0xF0000000

/************************************************
 * config enum
 ************************************************/
enum BRISKET2_NODE {
	BRISKET2_NODE_LIST_READ,
	BRISKET2_NODE_LIST_WRITE,
	BRISKET2_NODE_RW_REG_READ,
	BRISKET2_NODE_RW_REG_WRITE,

	NR_BRISKET2_NODE
};

enum BRISKET2_RW_GROUP {
	BRISKET2_RW_GROUP_V101,

	NR_BRISKET2_RW_GROUP,
};

enum BRISKET2_LIST {
	BRISKET2_LIST_SamplerEn,
	BRISKET2_LIST_DrccGateEn,
	BRISKET2_LIST_ConfigComplete,
	BRISKET2_LIST_FllEn,
	BRISKET2_LIST_FllClkOutSelect,
	BRISKET2_LIST_FllSlowReqEn,
	BRISKET2_LIST_FllSlowReqGateEn,
	BRISKET2_LIST_CttEn,
	BRISKET2_LIST_TestMode,
	BRISKET2_LIST_GlobalEventEn,
	BRISKET2_LIST_SafeFreqReqOverride,
	BRISKET2_LIST_Cfg,
	BRISKET2_LIST_PollingEn,
	BRISKET2_LIST_SafeFreqEn,
	BRISKET2_LIST_SafeFreqBypass,

	NR_BRISKET2_LIST,
};

enum BRISKET2_TRIGGER_STAGE {
	BRISKET2_TRIGGER_STAGE_PROBE,
	BRISKET2_TRIGGER_STAGE_SUSPEND,
	BRISKET2_TRIGGER_STAGE_RESUME,

	NR_BRISKET2_TRIGGER_STAGE,
};

enum BRISKET2_IPI_CFG {
	BRISKET2_IPI_CFG_POLLING,

	NR_BRISKET2_IPI_CFG,
};

/************************************************
 * Func prototype
 ************************************************/
int brisket2_resume(struct platform_device *pdev);
int brisket2_suspend(struct platform_device *pdev, pm_message_t state);
int brisket2_probe(struct platform_device *pdev);
int brisket2_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int brisket2_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum BRISKET2_TRIGGER_STAGE brisket2_tri_stage);
void brisket2_save_memory_info(char *buf, unsigned long long ptp3_mem_size);

/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_BRISKET2_CONTROL				0xC200051B
#else
#define MTK_SIP_KERNEL_BRISKET2_CONTROL				0x8200051B
#endif
#endif //_MTK_BRISKET2_H_
