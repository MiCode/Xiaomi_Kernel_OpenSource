/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_PWRCTRL_H
#define __KGSL_PWRCTRL_H

#include <linux/clk.h>
#include <linux/pm_qos.h>

/*****************************************************************************
 * power flags
 ****************************************************************************/
#define KGSL_MAX_CLKS 18

#define KGSL_MAX_PWRLEVELS 16

#define KGSL_PWRFLAGS_POWER_ON 0
#define KGSL_PWRFLAGS_CLK_ON   1
#define KGSL_PWRFLAGS_AXI_ON   2
#define KGSL_PWRFLAGS_IRQ_ON   3
#define KGSL_PWRFLAGS_NAP_OFF  5

/* Use to enable all the force power on states at once */
#define KGSL_PWR_ON GENMASK(5, 0)

/* Only two supported levels, min & max */
#define KGSL_CONSTRAINT_PWR_MAXLEVELS 2

#define KGSL_XO_CLK_FREQ	19200000
#define KGSL_ISENSE_CLK_FREQ	200000000

struct platform_device;
struct icc_path;

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
	u32 owner_timestamp;
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
	/** @voltage_level: Voltage level used by the GMU to vote RPMh */
	u32 voltage_level;
};

/**
 * struct kgsl_pwrctrl - Power control settings for a KGSL device
 * @interrupt_num - The interrupt number for the device
 * @grp_clks - Array of clocks structures that we control
 * @power_flags - Control flags for power
 * @pwrlevels - List of supported power levels
 * @active_pwrlevel - The currently active power level
 * @previous_pwrlevel - The power level before transition
 * @thermal_pwrlevel - maximum powerlevel constraint from thermal
 * @thermal_pwrlevel_floor - minimum powerlevel constraint from thermal
 * @default_pwrlevel - device wake up power level
 * @max_pwrlevel - maximum allowable powerlevel per the user
 * @min_pwrlevel - minimum allowable powerlevel per the user
 * @num_pwrlevels - number of available power levels
 * @throttle_mask - LM throttle mask
 * @interval_timeout - timeout to be idle before a power event
 * @clock_times - Each GPU frequency's accumulated active time in us
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
 */

struct kgsl_pwrctrl {
	int interrupt_num;
	struct clk *grp_clks[KGSL_MAX_CLKS];
	struct clk *gpu_bimc_int_clk;
	/** @cx_gdsc: Pointer to the CX domain regulator if applicable */
	struct regulator *cx_gdsc;
	/** @gx_gdsc: Pointer to the GX domain regulator if applicable */
	struct regulator *gx_gdsc;
	/** @gx_gdsc: Pointer to the GX domain parent supply */
	struct regulator *gx_gdsc_parent;
	/** @gx_gdsc_parent_min_corner: Minimum supply voltage for GX parent */
	u32 gx_gdsc_parent_min_corner;
	int isense_clk_indx;
	int isense_clk_on_level;
	unsigned long power_flags;
	unsigned long ctrl_flags;
	struct kgsl_pwrlevel pwrlevels[KGSL_MAX_PWRLEVELS];
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
	u32 interval_timeout;
	u64 clock_times[KGSL_MAX_PWRLEVELS];
	struct kgsl_clk_stats clk_stats;
	bool bus_control;
	int bus_mod;
	unsigned int bus_percent_ab;
	unsigned int bus_width;
	unsigned long bus_ab_mbytes;
	/** @ddr_table: List of the DDR bandwidths in KBps for the target */
	u32 *ddr_table;
	/** @ddr_table_count: Number of objects in @ddr_table */
	int ddr_table_count;
	/** cur_buslevel: The last buslevel voted by the driver */
	int cur_buslevel;
	/** @bus_max: The maximum bandwidth available to the device */
	unsigned long bus_max;
	struct kgsl_pwr_constraint constraint;
	bool superfast;
	unsigned int gpu_bimc_int_clk_freq;
	bool gpu_bimc_interface_enabled;
	/** @icc_path: Interconnect path for the GPU (if applicable) */
	struct icc_path *icc_path;
	/** cur_ab: The last ab voted by the driver */
	u32 cur_ab;
	/** @minbw_timer - Timer struct for entering minimum bandwidth state */
	struct timer_list minbw_timer;
	/** @minbw_timeout - Timeout for entering minimum bandwidth state */
	u32 minbw_timeout;
	/** @sysfs_thermal_req - PM QoS maximum frequency request from user (via sysfs) */
	struct dev_pm_qos_request sysfs_thermal_req;
	/** @time_in_pwrlevel: Each pwrlevel active duration in usec */
	u64 time_in_pwrlevel[KGSL_MAX_PWRLEVELS];
	/** @last_stat_updated: The last time stats were updated */
	ktime_t last_stat_updated;
	/** @nb_max: Notifier block for DEV_PM_QOS_MAX_FREQUENCY */
	struct notifier_block nb_max;
};

int kgsl_pwrctrl_init(struct kgsl_device *device);
void kgsl_pwrctrl_close(struct kgsl_device *device);
void kgsl_timer(struct timer_list *t);
void kgsl_pre_hwaccess(struct kgsl_device *device);
void kgsl_pwrctrl_pwrlevel_change(struct kgsl_device *device,
	unsigned int level);
int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device);
int kgsl_pwrctrl_change_state(struct kgsl_device *device, int state);

unsigned int kgsl_pwrctrl_adjust_pwrlevel(struct kgsl_device *device,
	unsigned int new_level);

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

/**
 * kgsl_active_count_wait() - Wait for activity to finish.
 * @device: Pointer to a KGSL device
 * @count: Active count value to wait for
 * @wait_jiffies: Jiffies to wait
 *
 * Block until the active_cnt value hits the desired value
 */
int kgsl_active_count_wait(struct kgsl_device *device, int count,
	unsigned long wait_jiffies);
void kgsl_pwrctrl_busy_time(struct kgsl_device *device, u64 time, u64 busy);

/**
 * kgsl_pwrctrl_set_constraint() - Validate and change enforced constraint
 * @device: Pointer to the kgsl_device struct
 * @pwrc: Pointer to requested constraint
 * @id: Context id which owns the constraint
 * @ts: The timestamp for which this constraint is enforced
 *
 * Accept the new constraint if no previous constraint existed or if the
 * new constraint is faster than the previous one.  If the new and previous
 * constraints are equal, update the timestamp and ownership to make sure
 * the constraint expires at the correct time.
 */
void kgsl_pwrctrl_set_constraint(struct kgsl_device *device,
			struct kgsl_pwr_constraint *pwrc, u32 id, u32 ts);
int kgsl_pwrctrl_set_default_gpu_pwrlevel(struct kgsl_device *device);

/**
 * kgsl_pwrctrl_request_state - Request a specific power state
 * @device: Pointer to the kgsl device
 * @state: Power state requested
 */
void kgsl_pwrctrl_request_state(struct kgsl_device *device, u32 state);

/**
 * kgsl_pwrctrl_axi - Propagate bus votes during slumber entry and exit
 * @device: Pointer to the kgsl device
 * @state: Whether we are going to slumber or coming out of slumber
 *
 * This function will propagate the default bus vote when coming out of
 * slumber and set bus bandwidth to 0 when going into slumber
 *
 * Return: 0 on success or negative error on failure
 */
int kgsl_pwrctrl_axi(struct kgsl_device *device, bool state);

/**
 * kgsl_idle_check - kgsl idle function
 * @work: work item being run by the function
 *
 * This function is called for work that is queued by the interrupt
 * handler or the idle timer. It attempts to transition to a clocks
 * off state if the active_cnt is 0 and the hardware is idle.
 */
void kgsl_idle_check(struct work_struct *work);

/**
 * kgsl_pwrctrl_irq - Enable or disable gpu interrupts
 * @device: Handle to the kgsl device
 * @state: Variable to decide whether interrupts need to be enabled or disabled
 *
 */
void kgsl_pwrctrl_irq(struct kgsl_device *device, bool state);

/**
 * kgsl_pwrctrl_clear_l3_vote - Relinquish l3 vote
 * @device: Handle to the kgsl device
 *
 * Clear the l3 vote when going into slumber
 */
void kgsl_pwrctrl_clear_l3_vote(struct kgsl_device *device);
#endif /* __KGSL_PWRCTRL_H */
