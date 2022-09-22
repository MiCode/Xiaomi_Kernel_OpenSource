/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __THERMAL_INTERFACE_H__
#define __THERMAL_INTERFACE_H__

#define CPU_TEMP_OFFSET             (0)
#define CPU_HEADROOM_OFFSET         (0x20)
#define CPU_HEADROOM_RATIO_OFFSET   (0x40)
#define CPU_PREDICT_TEMP_OFFSET     (0x60)
#define AP_NTC_HEADROOM_OFFSET      (0x80)
#define TPCB_OFFSET                 (0x84)
#define TARGET_TPCB_OFFSET          (0x88)
#define SPORTS_MODE_ENABLE          (0x90)
#define TTJ_OFFSET                 (0x100)
#define POWER_BUDGET_OFFSET        (0x110)
#define CPU_MIN_OPP_HINT_OFFSET    (0x120)
#define CPU_ACTIVE_BITMASK_OFFSET  (0x130)
#define CPU_JATM_SUSPEND_OFFSET    (0x140)
#define GPU_JATM_SUSPEND_OFFSET    (0x144)
#define MIN_THROTTLE_FREQ_OFFSET    (0x14C)
#define GPU_TEMP_OFFSET            (0x180)
#define APU_TEMP_OFFSET            (0x190)
#define APU_LIMIT_OPP_OFFSET       (0x194)
#define APU_CUR_OPP_OFFSET         (0x198)
#define EMUL_TEMP_OFFSET           (0x1B0)
#define CPU_LIMIT_FREQ_OFFSET      (0x200)
#define CPU_CUR_FREQ_OFFSET        (0x210)
#define CPU_MAX_TEMP_OFFSET        (0x220)
#define CPU_LIMIT_OPP_OFFSET       (0x260)
#define ATC_OFFSET                 (0x280)
#define ATC_NUM                    (17)
#define UTC_COUNT_OFFSET           (0x27C)
#define INFOB_OFFSET               (0x2C4)

#define APU_MBOX_TTJ_OFFSET        (0x700)
#define APU_MBOX_PB_OFFSET         (0x704)
#define APU_MBOX_TEMP_OFFSET       (0x708)
#define APU_MBOX_LIMIT_OPP_OFFSET  (0x70C)
#define APU_MBOX_CUR_OPP_OFFSET    (0x710)
#define APU_MBOX_EMUL_TEMP_OFFSET  (0x714)

struct headroom_info {
    int temp;
    int predict_temp;
    int headroom;
    int ratio;
};
enum headroom_id {
	/* SoC Tj */
	SOC_CPU0,
	SOC_CPU1,
	SOC_CPU2,
	SOC_CPU3,
	SOC_CPU4,
	SOC_CPU5,
	SOC_CPU6,
	SOC_CPU7,
	/* PCB */
	PCB_AP,

	NR_HEADROOM_ID
};

enum ttj_user {
	JATM_OFF = -1,
	CATM,
	JATM_ON,
	NR_TTJ_USER
};

struct ttj_info {
	int jatm_on;
	unsigned int catm_cpu_ttj;
	unsigned int catm_gpu_ttj;
	unsigned int catm_apu_ttj;
	unsigned int cpu_max_ttj;
	unsigned int gpu_max_ttj;
	unsigned int apu_max_ttj;
	unsigned int min_ttj;
};

struct frs_info {
	int enable;
	int activated;
	int pid;
	int target_fps;
	int diff;
	int tpcb;
	int tpcb_slope;
	int ap_headroom;
	int n_sec_to_ttpcb;
};

#define MAX_MD_NAME_LENGTH  (20)

struct md_thermal_sensor_t {
	int id;
	char sensor_name[MAX_MD_NAME_LENGTH];
	int cur_temp;
};

struct md_thermal_actuator_t {
	int id;
	char actuator_name[MAX_MD_NAME_LENGTH];
	int cur_status;
	int max_status;
};

struct md_info {
	int sensor_num;
	struct md_thermal_sensor_t *sensor_info;
	int md_autonomous_ctrl;
	int actuator_num;
	struct md_thermal_actuator_t *actuator_info;
};

extern void update_ap_ntc_headroom(int temp, int polling_interval);
extern int get_thermal_headroom(enum headroom_id id);
extern int set_cpu_min_opp(int gear, int opp);
extern int set_cpu_active_bitmask(int mask);
extern int get_cpu_temp(int cpu_id);
extern void set_ttj(int user);
extern void write_jatm_suspend(int jatm_suspend);
extern int get_jatm_suspend(void);
extern int get_catm_ttj(void);
extern int get_catm_min_ttj(void);

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
extern void __iomem *thermal_csram_base;
extern void __iomem *thermal_apu_mbox_base;
extern struct frs_info frs_data;
#else
static void __iomem *thermal_csram_base;
static void __iomem *thermal_apu_mbox_base;
static struct frs_info frs_data;
#endif
#endif
