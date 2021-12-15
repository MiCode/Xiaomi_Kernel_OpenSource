// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APUSYS_POWER_CUST_H_
#define _APUSYS_POWER_CUST_H_

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/sched/clock.h>
#include <linux/platform_device.h>

#include "apusys_power.h"
#ifdef BUILD_POLICY_TEST
#include "test.h"
#endif

//[Fix me]
//#define APUSYS_POWER_BRINGUP
#define APUPWR_TASK_DEBOUNCE
#define APUPWR_TAG_TP

#define BYPASS_POWER_OFF	(0)	// 1: bypass power off (return directly)
#define BYPASS_POWER_CTL	(0)	// 1: bypass power on/off feature
#define BYPASS_DVFS_CTL		(0)	// 1: bypass set DVFS opp feature
#define DEFAULT_POWER_ON	(0)	// 1: default power on in power probe
#define AUTO_BUCK_OFF_SUSPEND	(0)
#define AUTO_BUCK_OFF_DEEPIDLE	(0)
#define ASSERTIOM_CHECK (1)
#define DVFS_ASSERTION_CHECK (1)
#define VCORE_DVFS_SUPPORT	(0)
#define ASSERTION_PERCENTAGE	(1)	// 1%
#define BINNING_VOLTAGE_SUPPORT (1)
#define SUPPORT_HW_CONTROL_PMIC	(0)
#define TIME_PROFILING		(0)
#define APUSYS_SETTLE_TIME_TEST (0)
#define SUPPORT_VCORE_TO_IPUIF	(1)

#define APUSYS_MAX_NUM_OPPS		(5)
#define APUSYS_PATH_USER_NUM		(3)   // num of DVFS_XXX_PATH
#define APUSYS_DVFS_CONSTRAINT_NUM	(1)
#define APUSYS_VPU_NUM			(2)
#define APUSYS_MDLA_NUM			(0)
#define APUSYS_DEFAULT_OPP		(APUSYS_MAX_NUM_OPPS - 1)

#define VOLTAGE_CHECKER		(0)
/*
 * CCF_SET_RATE == 1
 * --> use CCF frame work to set (A/N)PUPLL but no hopping
 *
 * CCF_SET_RATE == 0
 * --> program register directly to set (A/N)PUPLL with hopping
 */
#define CCF_SET_RATE		(0)

/* Raise up lowest voltage */
#define VOLTAGE_RAISE_UP	(1)

// FIXME: check default value with APUSYS_DEFAULT_OPP
#define VCORE_DEFAULT_VOLT	DVFS_VOLT_00_550000_V
#define VVPU_DEFAULT_VOLT	DVFS_VOLT_00_575000_V
#define VMDLA_DEFAULT_VOLT	DVFS_VOLT_00_575000_V
#define VSRAM_DEFAULT_VOLT	DVFS_VOLT_00_750000_V

#define VCORE_SHUTDOWN_VOLT	DVFS_VOLT_00_550000_V
/* per de's request, take vvpu shutdown voltage as 550mv */
#define VVPU_SHUTDOWN_VOLT	DVFS_VOLT_00_550000_V
#define VMDLA_SHUTDOWN_VOLT	DVFS_VOLT_NOT_SUPPORT
#define VSRAM_SHUTDOWN_VOLT	DVFS_VOLT_00_750000_V

// FIXME: check default value with APUSYS_DEFAULT_OPP
#define BUCK_VVPU_DOMAIN_DEFAULT_FREQ DVFS_FREQ_00_273000_F
#define BUCK_VMDLA_DOMAIN_DEFAULT_FREQ DVFS_VOLT_NOT_SUPPORT
#define BUCK_VCONN_DOMAIN_DEFAULT_FREQ DVFS_FREQ_00_242700_F
#define VCORE_ON_FREQ		DVFS_FREQ_00_208000_F
#define VCORE_OFF_FREQ		DVFS_FREQ_00_026000_F


#define VSRAM_TRANS_VOLT	DVFS_VOLT_00_750000_V
#define VSRAM_LOW_VOLT		DVFS_VOLT_00_750000_V
#define VSRAM_HIGH_VOLT		DVFS_VOLT_00_800000_V

enum SEGMENT_INFO {
	SEGMENT_0 = 0,	// 5G_A++
	SEGMENT_1 = 1,	// 5G_A
	SEGMENT_2 = 2,	// 5G_A+(defalut)
};


enum DVFS_VPU0_PWR_PATH {
	VPU0_VPU = 0,
	VPU0_APU_CONN,
	VPU0_VCORE,
};

enum DVFS_VPU1_PWR_PATH {
	VPU1_VPU = 0,
	VPU1_APU_CONN,
	VPU1_VCORE,
};

enum DVFS_MDLA0_PWR_PATH {
	MDLA0_MDLA = 0,
	MDLA0_APU_CONN,
	MDLA0_VCORE,
};


struct apusys_dvfs_steps {
	enum DVFS_FREQ freq;
	enum DVFS_VOLTAGE voltage;
	enum DVFS_FREQ_POSTDIV post_divider;
	unsigned int dds;
};

struct apusys_dvfs_constraint {
	enum DVFS_BUCK buck0;
	enum DVFS_VOLTAGE voltage0;
	enum DVFS_BUCK buck1;
	enum DVFS_VOLTAGE voltage1;
};

struct apusys_aging_steps {
	enum DVFS_FREQ freq;
	int volt;
};

#if SUPPORT_VCORE_TO_IPUIF
struct ipuif_opp_table {
	unsigned int ipuif_khz;
	unsigned int ipuif_vcore;
};
#endif

struct apusys_dvfs_opps {
	// map to dvfs_table
	struct apusys_dvfs_steps (*opps)[APUSYS_BUCK_DOMAIN_NUM];
	enum DVFS_VOLTAGE user_path_volt[APUSYS_DVFS_USER_NUM]
					[APUSYS_PATH_USER_NUM];
	enum DVFS_VOLTAGE next_buck_volt[APUSYS_BUCK_NUM];
	enum DVFS_VOLTAGE cur_buck_volt[APUSYS_BUCK_NUM];
	uint8_t next_opp_index[APUSYS_BUCK_DOMAIN_NUM];
	uint8_t cur_opp_index[APUSYS_BUCK_DOMAIN_NUM];
	uint8_t power_lock_max_opp[APUSYS_DVFS_USER_NUM];
	uint8_t power_lock_min_opp[APUSYS_DVFS_USER_NUM];
	uint8_t thermal_opp[APUSYS_DVFS_USER_NUM];
	uint8_t user_opp_index[APUSYS_DVFS_USER_NUM];
	uint8_t driver_opp_index[APUSYS_DVFS_USER_NUM];
	bool is_power_on[APUSYS_POWER_USER_NUM];
	uint32_t power_bit_mask;
	uint64_t id;
	enum DVFS_VOLTAGE vsram_volatge;
#if APUSYS_SETTLE_TIME_TEST
	/* Here +1 is due to profile Vsram settle time */
	struct profiling_timestamp st[APUSYS_BUCK_NUM + 1];
#endif
#if SUPPORT_VCORE_TO_IPUIF
	int qos_apu_vcore;
	int driver_apu_vcore;
#endif
};

extern char *user_str[APUSYS_DVFS_USER_NUM];
extern char *buck_domain_str[APUSYS_BUCK_DOMAIN_NUM];
extern char *buck_str[APUSYS_BUCK_NUM];
extern bool apusys_dvfs_user_support[APUSYS_DVFS_USER_NUM];
extern bool apusys_dvfs_buck_domain_support[APUSYS_BUCK_DOMAIN_NUM];
extern enum DVFS_VOLTAGE_DOMAIN apusys_user_to_buck_domain
					[APUSYS_DVFS_USER_NUM];
extern enum DVFS_BUCK apusys_user_to_buck[APUSYS_DVFS_USER_NUM];
extern enum DVFS_USER apusys_buck_domain_to_user[APUSYS_BUCK_DOMAIN_NUM];
extern enum DVFS_BUCK apusys_buck_domain_to_buck[APUSYS_BUCK_DOMAIN_NUM];
extern enum DVFS_VOLTAGE_DOMAIN apusys_buck_to_buck_domain[APUSYS_BUCK_NUM];
extern uint8_t dvfs_clk_path[APUSYS_DVFS_USER_NUM][APUSYS_PATH_USER_NUM];
extern uint8_t dvfs_buck_for_clk_path[APUSYS_DVFS_USER_NUM][APUSYS_BUCK_NUM];
extern enum DVFS_VOLTAGE
	dvfs_clk_path_max_vol[APUSYS_DVFS_USER_NUM][APUSYS_PATH_USER_NUM];
extern bool buck_shared[APUSYS_BUCK_NUM]
				[APUSYS_DVFS_USER_NUM]
				[APUSYS_PATH_USER_NUM];
extern struct apusys_dvfs_constraint dvfs_constraint_table
					[APUSYS_DVFS_CONSTRAINT_NUM];
extern enum DVFS_VOLTAGE vcore_opp_mapping[];
extern struct apusys_dvfs_steps dvfs_table_0[APUSYS_MAX_NUM_OPPS]
						[APUSYS_BUCK_DOMAIN_NUM];
extern struct apusys_dvfs_steps dvfs_table_1[APUSYS_MAX_NUM_OPPS]
						[APUSYS_BUCK_DOMAIN_NUM];
extern struct apusys_dvfs_steps dvfs_table_2[APUSYS_MAX_NUM_OPPS]
						[APUSYS_BUCK_DOMAIN_NUM];

extern struct apusys_aging_steps aging_tbl[APUSYS_MAX_NUM_OPPS]
						[V_VCORE];

#if SUPPORT_VCORE_TO_IPUIF
extern struct ipuif_opp_table g_ipuif_opp_table[];
#endif
#ifdef APUPWR_TASK_DEBOUNCE
static inline void task_debounce(void)
{
	msleep_interruptible(20);
}
#endif /* APUPWR_TASK_DEBOUNCE */
#endif /* _APUSYS_POWER_CUST_H_ */
