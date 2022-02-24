// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_FLL_H_
#define _MTK_FLL_H_

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define FLL_MARGAUX

#ifdef FLL_MARGAUX
#define NR_FLL_CPU 8
#define FLL_CPU_START_ID 0
#define FLL_CPU_END_ID 7
#endif

/************************************************
 * Register addr, offset, bits range
 ************************************************/
/* FLL_CONTROL config */
#define FLL_CONTROL_BITS_Pllclken		1
/* FLL01 config */
#define FLL01_BITS_ErrOnline			11
#define FLL01_BITS_ErrOffline			11
#define FLL01_BITS_fsm_state			2
/* FLL02 config */
#define FLL02_BITS_cctrl_ping			6
#define FLL02_BITS_fctrl_ping			6
#define FLL02_BITS_cctrl_pong			6
#define FLL02_BITS_fctrl_pong			6
/* FLL03 config */
#define FLL03_BITS_InFreq				8
#define FLL03_BITS_OutFreq				8
#define FLL03_BITS_CalFreq				8
/* FLL04 config */
#define FLL04_BITS_Status				8
#define FLL04_BITS_PhaseErr				16
#define FLL04_BITS_LockDet				1
#define FLL04_BITS_Clk26mDet			1
/* FLL05 config */
#define FLL05_BITS_Bren					1
#define FLL05_BITS_KpOnline				4
#define FLL05_BITS_KiOnline				6
#define FLL05_BITS_KpOffline			4
#define FLL05_BITS_KiOffline			6
/* FLL06 config */
#define FLL06_BITS_FreqErrWtOnline		6
#define FLL06_BITS_FreqErrWtOffline		6
#define FLL06_BITS_PhaseErrWt			4
/* FLL07 config */
#define FLL07_BITS_FreqErrCapOnline		4
#define FLL07_BITS_FreqErrCapOffline	4
#define FLL07_BITS_PhaseErrCap			4
/* FLL08 config */
#define FLL08_BITS_PingMaxThreshold		6
#define FLL08_BITS_PongMinThreshold		6
#define FLL08_BITS_StartInPong			1
/* FLL09 config */
#define FLL09_BITS_PhlockThresh			3
#define FLL09_BITS_PhlockCycles			3
#define FLL09_BITS_Control				8

/* FLL_CONTROL config */
#define FLL_CONTROL_SHIFT_Pllclken		0
/* FLL01 config */
#define FLL01_SHIFT_ErrOnline			13
#define FLL01_SHIFT_ErrOffline			2
#define FLL01_SHIFT_fsm_state			0
/* FLL02 config */
#define FLL02_SHIFT_cctrl_ping			18
#define FLL02_SHIFT_fctrl_ping			12
#define FLL02_SHIFT_cctrl_pong			6
#define FLL02_SHIFT_fctrl_pong			0
/* FLL03 config */
#define FLL03_SHIFT_InFreq				16
#define FLL03_SHIFT_OutFreq				8
#define FLL03_SHIFT_CalFreq				0
/* FLL04 config */
#define FLL04_SHIFT_Status				18
#define FLL04_SHIFT_PhaseErr			2
#define FLL04_SHIFT_LockDet				1
#define FLL04_SHIFT_Clk26mDet			0
/* FLL05 config */
#define FLL05_SHIFT_Bren				20
#define FLL05_SHIFT_KpOnline			16
#define FLL05_SHIFT_KiOnline			10
#define FLL05_SHIFT_KpOffline			6
#define FLL05_SHIFT_KiOffline			0
/* FLL06 config */
#define FLL06_SHIFT_FreqErrWtOnline		10
#define FLL06_SHIFT_FreqErrWtOffline	4
#define FLL06_SHIFT_PhaseErrWt			0
/* FLL07 config */
#define FLL07_SHIFT_FreqErrCapOnline	8
#define FLL07_SHIFT_FreqErrCapOffline	4
#define FLL07_SHIFT_PhaseErrCap			0
/* FLL08 config */
#define FLL08_SHIFT_PingMaxThreshold	7
#define FLL08_SHIFT_PongMinThreshold	1
#define FLL08_SHIFT_StartInPong			0
/* FLL09 config */
#define FLL09_SHIFT_PhlockThresh		11
#define FLL09_SHIFT_PhlockCycles		8
#define FLL09_SHIFT_Control				0


/************************************************
 * config enum
 ************************************************/
enum FLL_RW {
	FLL_RW_READ,
	FLL_RW_WRITE,

	NR_FLL_RW,
};

enum FLL_GROUP {
	FLL_GROUP_CONTROL,
	FLL_GROUP_01,
	FLL_GROUP_02,
	FLL_GROUP_03,
	FLL_GROUP_04,
	FLL_GROUP_05,
	FLL_GROUP_06,
	FLL_GROUP_07,
	FLL_GROUP_08,
	FLL_GROUP_09,

	NR_FLL_GROUP,
};

enum FLL_TRIGGER_STAGE {
	FLL_TRIGGER_STAGE_PROBE,
	FLL_TRIGGER_STAGE_SUSPEND,
	FLL_TRIGGER_STAGE_RESUME,

	NR_FLL_TRIGGER_STAGE,
};

/************************************************
 * Func prototype
 ************************************************/
int fll_resume(struct platform_device *pdev);
int fll_suspend(struct platform_device *pdev, pm_message_t state);
int fll_probe(struct platform_device *pdev);
int fll_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int fll_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum FLL_TRIGGER_STAGE fll_tri_stage);
void fll_save_memory_info(char *buf, unsigned long long ptp3_mem_size);

/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_FLL_CONTROL				0xC200051B
#else
#define MTK_SIP_KERNEL_FLL_CONTROL				0x8200051B
#endif
#endif //_MTK_FLL_H_
