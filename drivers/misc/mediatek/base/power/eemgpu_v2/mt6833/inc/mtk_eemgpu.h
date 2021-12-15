/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_EEMG_
#define _MTK_EEMG_

#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#include "mtk_eemgpu_config.h"

#define EN_EEMGPU (1) /* enable/disable EEM (SW) */

/* have 5 banks */
enum eemg_ctrl_id {
	EEMG_CTRL_GPU,
#if ENABLE_LOO_G
	EEMG_CTRL_GPU_HI,
#endif

	NR_EEMG_CTRL,
};

enum eemg_det_id {
	EEMG_DET_GPU     =       EEMG_CTRL_GPU,
#if ENABLE_LOO_G
	EEMG_DET_GPU_HI  =       EEMG_CTRL_GPU_HI,
#endif

	NR_EEMG_DET,
};


enum mt_eemg_add_extra_mode {
	NO_EXTRA,
	ADD_EXTRA,
	UNDEF_EXTRA,
};

/* internal use */
/* EEM detector is disabled by who */
enum {
	BY_PROCFS	= BIT(0),
	BY_INIT_ERROR	= BIT(1),
	BY_MON_ERROR	= BIT(2),
};

enum eemg_phase {
	EEMG_PHASE_INIT01,
	EEMG_PHASE_INIT02,
	EEMG_PHASE_MON,
	EEMG_PHASE_CORN,

	NR_EEMG_PHASE,
};

enum eemg_features {
	FEA_INIT01	= BIT(EEMG_PHASE_INIT01),
	FEA_INIT02	= BIT(EEMG_PHASE_INIT02),
	FEA_MON		= BIT(EEMG_PHASE_MON),
	FEA_CORN	= BIT(EEMG_PHASE_CORN),
};

enum {
	EEMG_VOLT_NONE	= 0,
	EEMG_VOLT_UPDATE  = BIT(0),
	EEMG_VOLT_RESTORE = BIT(1),
};

enum {
	EEM_NORMAL_T	= 0,
	EEM_LOW_T,
	EEM_EXTRALOW_T,
	EEM_HIGH_T
};

#if ENABLE_LOO
enum eemg_loo_role {
	NO_LOO_BANK	=	0,
	LOW_BANK	=	1,
	HIGH_BANK	=	2,

	NR_EEMG_LOO_BANK,
};
#endif

extern u32 get_devinfo_with_index(u32 index);
extern unsigned int eemg_corn_flag;
extern const unsigned int reg_gpu_addr_off[DUMP_LEN];

#ifdef CONFIG_MTK_RAM_CONSOLE
#define CONFIG_EEMG_AEE_RR_REC 1
#endif

#ifdef CONFIG_EEMG_AEE_RR_REC
enum eemg_state {
	EEMG_GPU_IS_SET_VOLT = 3,		/* G */
#if ENABLE_LOO
	EEMG_GPU_HI_IS_SET_VOLT = 4,
#endif
#if ENABLE_LOO
	EEMG_CPU_BIG_HI_IS_SET_VOLT = 4,
#endif
};

extern void aee_rr_rec_ptp_devinfo_0(u32 val);
extern void aee_rr_rec_ptp_devinfo_1(u32 val);
extern void aee_rr_rec_ptp_devinfo_2(u32 val);
extern void aee_rr_rec_ptp_devinfo_3(u32 val);
extern void aee_rr_rec_ptp_devinfo_4(u32 val);
extern void aee_rr_rec_ptp_devinfo_5(u32 val);
extern void aee_rr_rec_ptp_devinfo_6(u32 val);
extern void aee_rr_rec_ptp_devinfo_7(u32 val);
extern void aee_rr_rec_ptp_e0(u32 val);
extern void aee_rr_rec_ptp_e1(u32 val);
extern void aee_rr_rec_ptp_e2(u32 val);
extern void aee_rr_rec_ptp_e3(u32 val);
extern void aee_rr_rec_ptp_e4(u32 val);
extern void aee_rr_rec_ptp_e5(u32 val);
extern void aee_rr_rec_ptp_e6(u32 val);
extern void aee_rr_rec_ptp_e7(u32 val);
extern void aee_rr_rec_ptp_e8(u32 val);
extern void aee_rr_rec_ptp_e9(u32 val);
extern void aee_rr_rec_ptp_e10(u32 val);
extern void aee_rr_rec_ptp_e11(u32 val);
extern void aee_rr_rec_ptp_vboot(u64 val);
extern void aee_rr_rec_ptp_cpu_big_volt(u64 val);
extern void aee_rr_rec_ptp_cpu_big_volt_1(u64 val);
extern void aee_rr_rec_ptp_cpu_big_volt_2(u64 val);
extern void aee_rr_rec_ptp_cpu_big_volt_3(u64 val);
extern void aee_rr_rec_ptp_gpu_volt(u64 val);
extern void aee_rr_rec_ptp_gpu_volt_1(u64 val);
extern void aee_rr_rec_ptp_gpu_volt_2(u64 val);
extern void aee_rr_rec_ptp_gpu_volt_3(u64 val);
extern void aee_rr_rec_ptp_cpu_little_volt(u64 val);
extern void aee_rr_rec_ptp_cpu_little_volt_1(u64 val);
extern void aee_rr_rec_ptp_cpu_little_volt_2(u64 val);
extern void aee_rr_rec_ptp_cpu_little_volt_3(u64 val);
extern void aee_rr_rec_ptp_cpu_2_little_volt(u64 val);
extern void aee_rr_rec_ptp_cpu_2_little_volt_1(u64 val);
extern void aee_rr_rec_ptp_cpu_2_little_volt_2(u64 val);
extern void aee_rr_rec_ptp_cpu_2_little_volt_3(u64 val);
extern void aee_rr_rec_ptp_cpu_cci_volt(u64 val);
extern void aee_rr_rec_ptp_cpu_cci_volt_1(u64 val);
extern void aee_rr_rec_ptp_cpu_cci_volt_2(u64 val);
extern void aee_rr_rec_ptp_cpu_cci_volt_3(u64 val);
extern void aee_rr_rec_ptp_temp(u64 val);
extern void aee_rr_rec_ptp_status(u8 val);
extern void aee_rr_rec_eemg_pi_offset(u8 val);

extern u32 aee_rr_curr_ptp_devinfo_0(void);
extern u32 aee_rr_curr_ptp_devinfo_1(void);
extern u32 aee_rr_curr_ptp_devinfo_2(void);
extern u32 aee_rr_curr_ptp_devinfo_3(void);
extern u32 aee_rr_curr_ptp_devinfo_4(void);
extern u32 aee_rr_curr_ptp_devinfo_5(void);
extern u32 aee_rr_curr_ptp_devinfo_6(void);
extern u32 aee_rr_curr_ptp_devinfo_7(void);
extern u32 aee_rr_curr_ptp_e0(void);
extern u32 aee_rr_curr_ptp_e1(void);
extern u32 aee_rr_curr_ptp_e2(void);
extern u32 aee_rr_curr_ptp_e3(void);
extern u32 aee_rr_curr_ptp_e4(void);
extern u32 aee_rr_curr_ptp_e5(void);
extern u32 aee_rr_curr_ptp_e6(void);
extern u32 aee_rr_curr_ptp_e7(void);
extern u32 aee_rr_curr_ptp_e8(void);
extern u32 aee_rr_curr_ptp_e9(void);
extern u32 aee_rr_curr_ptp_e10(void);
extern u32 aee_rr_curr_ptp_e11(void);
extern u64 aee_rr_curr_ptp_vboot(void);
extern u64 aee_rr_curr_ptp_cpu_big_volt(void);
extern u64 aee_rr_curr_ptp_cpu_big_volt_1(void);
extern u64 aee_rr_curr_ptp_cpu_big_volt_2(void);
extern u64 aee_rr_curr_ptp_cpu_big_volt_3(void);
extern u64 aee_rr_curr_ptp_gpu_volt(void);
extern u64 aee_rr_curr_ptp_gpu_volt_1(void);
extern u64 aee_rr_curr_ptp_gpu_volt_2(void);
extern u64 aee_rr_curr_ptp_gpu_volt_3(void);
extern u64 aee_rr_curr_ptp_cpu_little_volt(void);
extern u64 aee_rr_curr_ptp_cpu_little_volt_1(void);
extern u64 aee_rr_curr_ptp_cpu_little_volt_2(void);
extern u64 aee_rr_curr_ptp_cpu_little_volt_3(void);
extern u64 aee_rr_curr_ptp_cpu_2_little_volt(void);
extern u64 aee_rr_curr_ptp_cpu_2_little_volt_1(void);
extern u64 aee_rr_curr_ptp_cpu_2_little_volt_2(void);
extern u64 aee_rr_curr_ptp_cpu_2_little_volt_3(void);
extern u64 aee_rr_curr_ptp_cpu_cci_volt(void);
extern u64 aee_rr_curr_ptp_cpu_cci_volt_1(void);
extern u64 aee_rr_curr_ptp_cpu_cci_volt_2(void);
extern u64 aee_rr_curr_ptp_cpu_cci_volt_3(void);
extern u64 aee_rr_curr_ptp_temp(void);
extern u8 aee_rr_curr_ptp_status(void);
#endif

/* EEM Extern Function */
extern int mt_eemg_status(enum eemg_det_id id);
extern unsigned int get_efuse_status(void);
extern unsigned int mt_eemg_is_enabled(void);

extern void eemg_set_pi_efuse(enum eemg_det_id id,
		unsigned int pi_efuse,
		unsigned int loo_enabled);
extern void eemg_set_pi_dvtfixed(enum eemg_det_id id,
		unsigned int pi_dvtfixed);

/* DRCC */
extern unsigned int drcc_offset_done;
#endif
