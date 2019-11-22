/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010-2019, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_PWRCTRL_H
#define __KGSL_PWRCTRL_H

#include <linux/clk.h>

/*****************************************************************************
 * power flags
 ****************************************************************************/
#define KGSL_PWRFLAGS_ON   1
#define KGSL_PWRFLAGS_OFF  0

#define KGSL_PWRLEVEL_TURBO 0

#define KGSL_PWR_ON	0xFFFF

#define KGSL_MAX_CLKS 17
#define KGSL_MAX_REGULATORS 2

#define KGSL_MAX_PWRLEVELS 10

/* Only two supported levels, min & max */
#define KGSL_CONSTRAINT_PWR_MAXLEVELS 2

#define KGSL_XO_CLK_FREQ	19200000
#define KGSL_RBBMTIMER_CLK_FREQ	KGSL_XO_CLK_FREQ
#define KGSL_ISENSE_CLK_FREQ	200000000

enum kgsl_pwrctrl_timer_type {
	KGSL_PWR_IDLE_TIMER,
};

struct platform_device;

struct kgsl_clk_stats {
	unsigned int busy;
	unsigned int total;
	unsigned int busy_old;
	unsigned int total_old;
};

struct kgsl_pwr_constraint {
	unsigned int type;
	unsigned int sub_type;
	union {
		struct {
			unsigned int level;
		} pwrlevel;
	} hint;
	unsigned long expires;
	uint32_t owner_id;
};

/**
 * struct kgsl_pwrlevel - Struct holding different pwrlevel info obtained from
 * from dtsi file
 * @gpu_freq:          GPU frequency vote in Hz
 * @bus_freq:          Bus bandwidth vote index
 * @bus_min:           Min bus index @gpu_freq
 * @bus_max:           Max bus index @gpu_freq
 */
struct kgsl_pwrlevel {
	unsigned int gpu_freq;
	unsigned int bus_freq;
	unsigned int bus_min;
	unsigned int bus_max;
	unsigned int acd_level;
};

struct kgsl_regulator {
	struct regulator *reg;
	char name[8];
};

/**
 * struct kgsl_pwrctrl - Power control settings for a KGSL device
 * @interrupt_num - The interrupt number for the device
 * @grp_clks - Array of clocks structures that we control
 * @power_flags - Control flags for power
 * @pwrlevels - List of supported power levels
 * @nb - Notifier block to receive GPU OPP change event
 * @active_pwrlevel - The currently active power level
 * @previous_pwrlevel - The power level before transition
 * @thermal_pwrlevel - maximum powerlevel constraint from thermal
 * @thermal_pwrlevel_floor - minimum powerlevel constraint from thermal
 * @default_pwrlevel - device wake up power level
 * @max_pwrlevel - maximum allowable powerlevel per the user
 * @min_pwrlevel - minimum allowable powerlevel per the user
 * @num_pwrlevels - number of available power levels
 * @throttle_mask - LM throttle mask
 * @interval_timeout - timeout in jiffies to be idle before a power event
 * @clock_times - Each GPU frequency's accumulated active time in us
 * @regulators - array of pointers to kgsl_regulator structs
 * @pcl - bus scale identifier
 * @irq_name - resource name for the IRQ
 * @clk_stats - structure of clock statistics
 * @input_disable - To disable GPU wakeup on touch input event
 * @bus_control - true if the bus calculation is independent
 * @bus_mod - modifier from the current power level for the bus vote
 * @bus_percent_ab - current percent of total possible bus usage
 * @bus_width - target specific bus width in number of bytes
 * @bus_ab_mbytes - AB vote in Mbytes for current bus usage
 * @constraint - currently active power constraint
 * @superfast - Boolean flag to indicate that the GPU start should be run in the
 * higher priority thread
 * isense_clk_indx - index of isense clock, 0 if no isense
 * isense_clk_on_level - isense clock rate is XO rate below this level.
 * tzone_name - pointer to thermal zone name of GPU temperature sensor
 */

struct kgsl_pwrctrl {
	int interrupt_num;
	struct clk *grp_clks[KGSL_MAX_CLKS];
	struct clk *gpu_bimc_int_clk;
	int isense_clk_indx;
	int isense_clk_on_level;
	unsigned long power_flags;
	unsigned long ctrl_flags;
	struct kgsl_pwrlevel pwrlevels[KGSL_MAX_PWRLEVELS];
	struct notifier_block nb;
	unsigned int active_pwrlevel;
	unsigned int previous_pwrlevel;
	unsigned int thermal_pwrlevel;
	unsigned int thermal_pwrlevel_floor;
	unsigned int default_pwrlevel;
	unsigned int wakeup_maxpwrlevel;
	unsigned int max_pwrlevel;
	unsigned int min_pwrlevel;
	unsigned int num_pwrlevels;
	unsigned int throttle_mask;
	unsigned long interval_timeout;
	u64 clock_times[KGSL_MAX_PWRLEVELS];
	struct kgsl_regulator regulators[KGSL_MAX_REGULATORS];
	uint32_t pcl;
	const char *irq_name;
	struct kgsl_clk_stats clk_stats;
	bool input_disable;
	bool bus_control;
	int bus_mod;
	unsigned int bus_percent_ab;
	unsigned int bus_width;
	unsigned long bus_ab_mbytes;
	struct device *devbw;
	/** @bus_ibs: List of the bus bandwidths in use by our target */
	u32 *bus_ibs;
	/** @bus_ibs_count: Number of objects in @bus_ibs */
	int bus_ibs_count;
	/** cur_buslevel: The last buslevel voted by the driver */
	int cur_buslevel;
	/** @bus_max: The maximum bandwidth available to the device */
	unsigned long bus_max;
	struct kgsl_pwr_constraint constraint;
	bool superfast;
	unsigned int gpu_bimc_int_clk_freq;
	bool gpu_bimc_interface_enabled;
	const char *tzone_name;
	/** @icc_path: Interconnect path for the GPU (if applicable) */
	struct icc_path *icc_path;
};

int kgsl_pwrctrl_init(struct kgsl_device *device);
void kgsl_pwrctrl_close(struct kgsl_device *device);
void kgsl_timer(struct timer_list *t);
void kgsl_idle_check(struct work_struct *work);
void kgsl_pre_hwaccess(struct kgsl_device *device);
void kgsl_pwrctrl_pwrlevel_change(struct kgsl_device *device,
	unsigned int level);
void kgsl_pwrctrl_buslevel_update(struct kgsl_device *device,
	bool on);
int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device);
void kgsl_pwrctrl_uninit_sysfs(struct kgsl_device *device);
int kgsl_pwrctrl_change_state(struct kgsl_device *device, int state);
int kgsl_clk_set_rate(struct kgsl_device *device,
	unsigned int pwrlevel);
unsigned int kgsl_pwrctrl_adjust_pwrlevel(struct kgsl_device *device,
	unsigned int new_level);

static inline unsigned long kgsl_get_clkrate(struct clk *clk)
{
	return (clk != NULL) ? clk_get_rate(clk) : 0;
}

/*
 * kgsl_pwrctrl_active_freq - get currently configured frequency
 * @pwr: kgsl_pwrctrl structure for the device
 *
 * Returns the currently configured frequency for the device.
 */
static inline unsigned long
kgsl_pwrctrl_active_freq(struct kgsl_pwrctrl *pwr)
{
	return pwr->pwrlevels[pwr->active_pwrlevel].gpu_freq;
}

int __must_check kgsl_active_count_get(struct kgsl_device *device);
void kgsl_active_count_put(struct kgsl_device *device);
int kgsl_active_count_wait(struct kgsl_device *device, int count);
void kgsl_pwrctrl_busy_time(struct kgsl_device *device, u64 time, u64 busy);
void kgsl_pwrctrl_set_constraint(struct kgsl_device *device,
			struct kgsl_pwr_constraint *pwrc, uint32_t id);
int kgsl_pwrctrl_set_default_gpu_pwrlevel(struct kgsl_device *device);
void kgsl_pwrctrl_disable_unused_opp(struct kgsl_device *device,
		struct device *dev);

#endif /* __KGSL_PWRCTRL_H */
