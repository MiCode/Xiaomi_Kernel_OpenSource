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
#ifndef _MTK_DRCC_H_
#define _MTK_DRCC_H_

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */

#define DRCC_NUM 8
#define DRCC_L_NUM 4
#define DRCC_REG_CNT 6
#define DRCC_L_AOREG_OFFSET 0x000
#define DRFC_V101_OFFSET 0x400

/*  V102			V103		V104		V105		V106*/
/*  0x404			0x408		0x40C		0x410		0x414*/
/*0x0C532404	0x0C532408	0x0C53240C	0x0C532410	0x0C532414*/
/*0x0C532C04	0x0C532C08	0x0C532C0C	0x0C532C10	0x0C532C14*/
/*0x0C533404	0x0C533408	0x0C53340C	0x0C533410	0x0C533414*/
/*0x0C533C04	0x0C533C08	0x0C533C0C	0x0C533C10	0x0C533C14*/

/************************************************
 * config enum
 ************************************************/
enum DRCC_GROUP {
	DRCC_GROUP_DrccEnable,
	DRCC_GROUP_Hwgatepct,
	DRCC_GROUP_Hwgatepct_R,
	DRCC_GROUP_DrccCode,
	DRCC_GROUP_DrccCode_R,
	DRCC_GROUP_DrccVrefFilt,
	DRCC_GROUP_DrccVrefFilt_R,
	DRCC_GROUP_DrccClockEdgeSel,
	DRCC_GROUP_DrccClockEdgeSel_R,
	DRCC_GROUP_DrccForceTrim,
	DRCC_GROUP_DrccForceTrim_R,
	DRCC_GROUP_ForceTrimEn,
	DRCC_GROUP_ForceTrimEn_R,
	DRCC_GROUP_AutoCalibDelay,
	DRCC_GROUP_AutoCalibDelay_R,
	DRCC_GROUP_EventSource0,
	DRCC_GROUP_EventSource0_R,
	DRCC_GROUP_EventType0,
	DRCC_GROUP_EventType0_R,
	DRCC_GROUP_EventSource1,
	DRCC_GROUP_EventSource1_R,
	DRCC_GROUP_EventType1,
	DRCC_GROUP_EventType1_R,
	DRCC_GROUP_EventFreeze,
	DRCC_GROUP_EventFreeze_R,
	DRCC_GROUP_EventCount0_R,
	DRCC_GROUP_EventCount1_R,
	DRCC_GROUP_CFG,
	DRCC_GROUP_READ, //dump_reg, info,

	NR_DRCC_GROUP,
};

enum DRCC_TRIGGER_STAGE {
	DRCC_TRIGGER_STAGE_PROBE,
	DRCC_TRIGGER_STAGE_SUSPEND,
	DRCC_TRIGGER_STAGE_RESUME,

	NR_DRCC_TRIGGER_STAGE,
};

struct drcc_l_class {
	/* 0x0C53B000, 0x0C53B200, 0x0C53B400, 0x0C53B600 */
	unsigned int drcc_enable:4;				/* [0] */
	unsigned int drcc_code:8;				/* [9:4] */
	unsigned int drcc_hwgatepct:4;			/* [14:12] */
	unsigned int drcc_verffilt:4;			/* [18:16] */
	unsigned int drcc_autocalibdelay:12;	/* [23:20] */

	/* DRCC_CFG_REG0 0x0C530280, 0A80, 1280, 1A80, 2280, 2A80, 3280, 3A80 */
	unsigned int drcc_compstate:4;			/* [1] */
	unsigned int drcc_trigen:1;				/* [4] */
	unsigned int drcc_trigsel:1;			/* [5] */
	unsigned int drcc_counten:1;			/* [6] */
	unsigned int drcc_countsel:1;			/* [7] */
	unsigned int drcc_cpustandbywfi_mask:1;	/* [8] */
	unsigned int drcc_cpustandbywfe_mask:3;	/* [9] */
	unsigned int drcc_mode:12;				/* [14:12] */

	/* DRCC_CFG_REG2 0x0C530288, 0A88, 1288, 1A88, 2288, 2A88, 3288, 3A88 */
	unsigned int drcc_forcetrim:8;			/* B[7:1] L[6:0] */
	unsigned int drcc_forcetrimen:1;		/* [8] */
	unsigned int drcc_disableautoprtdurcalib:3; /* [9] */
	unsigned int drcc_autocalibdone:1;		/* [12] */
	unsigned int drcc_autocaliberror:3;		/* [13] */
	unsigned int drcc_autocalibtrim:16;		/* [22:19]*/

	/* DRCC_CFG_REG3 0x0C53028C, 0A8C, 128C, 1A8C, 228C, 2A8C, 328C, 3A8C */
	unsigned int drcc_eventcount;
};

struct drcc_b_class {
	/*DRFC_V101 0x0C532400, 0x0C532C00, 0x0C533400, 0x0C533C00 */
	unsigned int drcc_SafeFreqReqOverride:1;	/* [0] */
	unsigned int drcc_GlobalEventEn:1;			/* [1] */
	unsigned int drcc_TestMode:4;				/* [5:2] */
	unsigned int drcc_CttEn:1;					/* [6] */
	unsigned int drcc_FllSlowReqGateEn:1;		/* [7] */
	unsigned int drcc_FllSlowReqEn:1;			/* [8] */
	unsigned int drcc_FllClkOutSelect:1;		/* [9] */
	unsigned int drcc_FllEn:1;					/* [10] */
	unsigned int drcc_ConfigComplete:1;			/* [11] */
	unsigned int drcc_DrccGateEn:1;				/* [12] */
	unsigned int drcc_SamplerEn:1;				/* [13] */
	unsigned int drcc_V101:18;					/* [14:31] */

	/*DRFC_V102 0x0C532404, 0x0C532C04, 0x0C533404, 0x0C533C04 */
	unsigned int drcc_AutoCalibDelay:4;			/*[3:0]*/
	unsigned int drcc_ForceTrimEn:1;			/*[4:4]*/
	unsigned int drcc_DrccForceTrim:7;			/*[11:5]*/
	unsigned int drcc_DrccClockEdgeSel:1;		/*[12:12]*/
	unsigned int drcc_DrccVrefFilt:3;			/*[15:13]*/
	unsigned int drcc_DrccCode:6;				/*[21:16]*/
	unsigned int drcc_V102:11;					/*[31:21]*/

	/*DRFC_V103 0x0C532408, 0x0C532C08, 0x0C533408, 0x0C533C08 */
	unsigned int drcc_DrccAutoCalibTrim:7;		/*[6:0]*/
	unsigned int drcc_DrccAutoCalibError:1;		/*[7:7]*/
	unsigned int drcc_DrccAutoCalibDone:1;		/*[8:8]*/
	unsigned int drcc_DrccCMP:1;				/*[9:9]*/
	unsigned int drcc_V103:22;					/*[31:10]*/

	/*DRFC_V104 0x0C53240C, 0x0C532C0C, 0x0C53340C, 0x0C533C0C */
	unsigned int drcc_EventFreeze:1;			/*[0:0]*/
	unsigned int drcc_EventType1:2;				/*[2:1]*/
	unsigned int drcc_EventSource1:3;			/*[5:3]*/
	unsigned int drcc_EventType0:2;				/*[7:6]*/
	unsigned int drcc_EventSource0:3;			/*[10:8]*/
	unsigned int drcc_V104:21;					/*[31:11]*/

	/*DRFC_V105 0x0C532410, 0x0C532C10, 0x0C533410, 0x0C533C10*/
	unsigned int drcc_EventCount0:32;			/*[31:0]*/

	/*DRFC_V106 0x0C532414, 0x0C532C14, 0x0C533414, 0x0C533C14*/
	unsigned int drcc_EventCount1:32;			/*[31:0]*/

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
