/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef _MT_CPUFREQ_HYBRID_
#define _MT_CPUFREQ_HYBRID_

#include <linux/kernel.h>


/**************************************
 * [Hybrid DVFS] Config
 **************************************/
#if defined(CONFIG_ARCH_MT6755)
#define CONFIG_HYBRID_CPU_DVFS
/*#define __TRIAL_RUN__*/

#elif defined(CONFIG_ARCH_MT6757) && !defined(CONFIG_FPGA_EARLY_PORTING)
#define CONFIG_HYBRID_CPU_DVFS
#ifdef CONFIG_HYBRID_CPU_DVFS
/*#define CPUHVFS_HW_GOVERNOR*/
#endif
/*#define __TRIAL_RUN__*/

#elif defined(CONFIG_ARCH_MT6797) /*&& defined(CONFIG_MTK_HYBRID_CPU_DVFS)*/
#include "../mt6797/mt_cpufreq.h"
#ifdef ENABLE_IDVFS
#define CONFIG_HYBRID_CPU_DVFS
#endif
#ifdef CONFIG_HYBRID_CPU_DVFS
/*#define CPUHVFS_HW_GOVERNOR*/
#endif
/*#define __TRIAL_RUN__*/

#endif


/**************************************
 * [Hybrid DVFS] Parameter
 **************************************/
#if defined(CONFIG_ARCH_MT6797)
enum cpu_cluster {
	CPU_CLUSTER_LL,
	CPU_CLUSTER_L,
	CPU_CLUSTER_B,
	CPU_CLUSTER_CCI,	/* virtual */
	NUM_CPU_CLUSTER
};

#define NUM_PHY_CLUSTER		(NUM_CPU_CLUSTER - 1)
#define NUM_CPU_OPP		16

#elif defined(CONFIG_ARCH_MT6757)
enum cpu_cluster {
	CPU_CLUSTER_LL,
	CPU_CLUSTER_L,
	CPU_CLUSTER_CCI,	/* virtual */
	NUM_CPU_CLUSTER
};

#define NUM_PHY_CLUSTER		(NUM_CPU_CLUSTER - 1)
#define NUM_CPU_OPP		16

#else	/* CONFIG_ARCH_MT6755 */
enum cpu_cluster {
	CPU_CLUSTER_LL,
	CPU_CLUSTER_L,
	NUM_CPU_CLUSTER
};

#define NUM_PHY_CLUSTER		NUM_CPU_CLUSTER
#define NUM_CPU_OPP		8
#endif


/**************************************
 * [Hybrid DVFS] Definition
 **************************************/
#define OPP_AT_SUSPEND		UINT_MAX
#define VOLT_AT_SUSPEND		UINT_MAX
#define VSRAM_AT_SUSPEND	UINT_MAX
#define CEILING_AT_SUSPEND	UINT_MAX
#define FLOOR_AT_SUSPEND	UINT_MAX

enum sema_user {
	SEMA_FHCTL_DRV,
	SEMA_I2C_DRV,
	NUM_SEMA_USER
};

enum pause_src {
	PAUSE_INIT,
	PAUSE_I2CDRV,
	PAUSE_IDLE,
	PAUSE_SUSPEND,
	PAUSE_HWGOV,
	NUM_PAUSE_SRC
};

enum power_mode {
	POWER_NORMAL,
	POWER_SPORTS,
	NUM_POWER_MODE
};

struct init_sta {
	unsigned int opp[NUM_CPU_CLUSTER];	/* SW index */
	unsigned int freq[NUM_CPU_CLUSTER];	/* KHz */
	unsigned int volt[NUM_CPU_CLUSTER];	/* Vproc PMIC value */
	unsigned int vsram[NUM_CPU_CLUSTER];	/* Vsram PMIC value */
	unsigned int ceiling[NUM_CPU_CLUSTER];	/* SW index */
	unsigned int floor[NUM_CPU_CLUSTER];	/* SW index */
	bool is_on[NUM_CPU_CLUSTER];		/* on/off */
};

struct dvfs_log {
	unsigned int time;			/* (1/32768)s */
	unsigned int opp[NUM_CPU_CLUSTER];	/* SW index */
};

typedef void (*dvfs_notify_t)(struct dvfs_log *log_box, int num_log);


/**************************************
 * [Hybrid DVFS] Macro / Inline
 **************************************/
static inline unsigned int opp_limit_to_ceiling(int limit)
{
	unsigned int ceiling;

	if (limit >= 0)
		ceiling = (limit < NUM_CPU_OPP ? limit : NUM_CPU_OPP - 1);
	else	/* no limit */
		ceiling = 0;

	return ceiling;
}

static inline unsigned int opp_limit_to_floor(int limit)
{
	unsigned int floor;

	if (limit >= 0)
		floor = (limit < NUM_CPU_OPP ? limit : NUM_CPU_OPP - 1);
	else	/* no limit */
		floor = NUM_CPU_OPP - 1;

	return floor;
}


/**************************************
 * [Hybrid DVFS] API
 **************************************/
#ifdef CONFIG_HYBRID_CPU_DVFS
extern int cpuhvfs_module_init(void);
extern int cpuhvfs_kick_dvfsp_to_run(struct init_sta *sta);

extern void cpuhvfs_notify_cluster_on(unsigned int cluster);
extern void cpuhvfs_notify_cluster_off(unsigned int cluster);

extern int cpuhvfs_set_target_opp(unsigned int cluster, unsigned int index, unsigned int *ret_volt);
extern unsigned int cpuhvfs_get_curr_volt(unsigned int cluster);

extern void cpuhvfs_set_opp_limit(unsigned int cluster, unsigned int ceiling, unsigned int floor);

extern int cpuhvfs_get_dvfsp_semaphore(enum sema_user user);
extern void cpuhvfs_release_dvfsp_semaphore(enum sema_user user);

extern int cpuhvfs_pause_dvfsp_running(enum pause_src src);
extern void cpuhvfs_unpause_dvfsp_to_run(enum pause_src src);

extern int cpuhvfs_stop_dvfsp_running(void);
extern int cpuhvfs_restart_dvfsp_running(struct init_sta *sta);

extern int cpuhvfs_dvfsp_suspend(void);
extern void cpuhvfs_dvfsp_resume(unsigned int on_cluster, struct init_sta *sta);

extern void cpuhvfs_dump_dvfsp_info(void);
#else
static inline int cpuhvfs_module_init(void)		{ return -ENODEV; }
static inline int cpuhvfs_kick_dvfsp_to_run(struct init_sta *sta)	{ return -ENODEV; }

static inline void cpuhvfs_notify_cluster_on(unsigned int cluster)	{}
static inline void cpuhvfs_notify_cluster_off(unsigned int cluster)	{ WARN_ON(1); }

static inline int cpuhvfs_set_target_opp(unsigned int cluster, unsigned int index,
					 unsigned int *ret_volt)	{ return -ENODEV; }
static inline unsigned int cpuhvfs_get_curr_volt(unsigned int cluster)	{ return UINT_MAX; }

static inline void cpuhvfs_set_opp_limit(unsigned int cluster, unsigned int ceiling,
					 unsigned int floor)		{}

static inline int cpuhvfs_get_dvfsp_semaphore(enum sema_user user)	{ return 0; }
static inline void cpuhvfs_release_dvfsp_semaphore(enum sema_user user)	{}

static inline int cpuhvfs_pause_dvfsp_running(enum pause_src src)	{ return 0; }
static inline void cpuhvfs_unpause_dvfsp_to_run(enum pause_src src)	{}

static inline int cpuhvfs_stop_dvfsp_running(void)			{ return 0; }
static inline int cpuhvfs_restart_dvfsp_running(struct init_sta *sta)	{ return -ENODEV; }

static inline int cpuhvfs_dvfsp_suspend(void)		{ return 0; }
static inline void cpuhvfs_dvfsp_resume(unsigned int on_cluster, struct init_sta *sta)	{}

static inline void cpuhvfs_dump_dvfsp_info(void)	{}
#endif


#if defined(CONFIG_HYBRID_CPU_DVFS) && defined(CPUHVFS_HW_GOVERNOR)
extern void cpuhvfs_register_dvfs_notify(dvfs_notify_t callback);

extern void cpuhvfs_set_power_mode(enum power_mode mode);

extern int cpuhvfs_enable_hw_governor(struct init_sta *sta);
extern int cpuhvfs_disable_hw_governor(struct init_sta *ret_sta);
#else
static inline void cpuhvfs_register_dvfs_notify(dvfs_notify_t callback)	{}

static inline void cpuhvfs_set_power_mode(enum power_mode mode)		{}

static inline int cpuhvfs_enable_hw_governor(struct init_sta *sta)	{ return -EPERM; }
static inline int cpuhvfs_disable_hw_governor(struct init_sta *ret_sta)	{ return 0; }
#endif


#ifdef CONFIG_ARCH_MT6797
#ifdef CONFIG_HYBRID_CPU_DVFS
extern void cpuhvfs_get_pause_status_i2c(void);		/* deprecated */
#else
static inline void cpuhvfs_get_pause_status_i2c(void)	{}
#endif
#endif

#endif	/* _MT_CPUFREQ_HYBRID_ */
