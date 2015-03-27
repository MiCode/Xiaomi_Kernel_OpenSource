/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __REGULATOR_CPR3_REGULATOR_H__
#define __REGULATOR_CPR3_REGULATOR_H__

#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/power/qcom/apm.h>
#include <linux/regulator/driver.h>

struct cpr3_controller;

/**
 * struct cpr3_fuse_param - defines one contiguous segment of a fuse parameter
 *			    that is contained within a given row.
 * @row:	Fuse row number
 * @bit_start:	The first bit within the row of the fuse parameter segment
 * @bit_end:	The last bit within the row of the fuse parameter segment
 *
 * Each fuse row is 64 bits in length.  bit_start and bit_end may take values
 * from 0 to 63.  bit_start must be less than or equal to bit_end.
 */
struct cpr3_fuse_param {
	unsigned		row;
	unsigned		bit_start;
	unsigned		bit_end;
};

/**
 * struct cpr3_corner - CPR3 virtual voltage corner data structure
 * @floor_volt:		CPR closed-loop floor voltage in microvolts
 * @ceiling_volt:	CPR closed-loop ceiling voltage in microvolts
 * @open_loop_volt:	CPR open-loop voltage (i.e. initial voltage) in
 *			microvolts
 * @last_volt:		Last known settled CPR closed-loop voltage which is used
 *			when switching to a new corner
 * @proc_freq:		Processor frequency in Hertz (only used by platform
 *			specific CPR3 driver for interpolation)
 * @cpr_fuse_corner:	Fused corner index associated with this virtual corner
 *			(only used by platform specific CPR3 driver for
 *			mapping purposes)
 *
 * The value of last_volt is initialized inside of the cpr3_regulator_register()
 * call with the open_loop_volt value.  It can later be updated to the settled
 * VDD supply voltage.
 */
struct cpr3_corner {
	int			floor_volt;
	int			ceiling_volt;
	int			open_loop_volt;
	int			last_volt;
	u32			proc_freq;
	int			cpr_fuse_corner;
};

/**
 * struct cpr3_thread - CPR3 hardware thread data structure
 * @thread_id:		Hardware thread ID
 * @of_node:		Device node associated with the device tree child node
 *			of this CPR3 thread
 * @ctrl:		Pointer to the CPR3 controller which manages this thread
 * @rdesc:		Regulator description for this thread
 * @rdev:		Regulator device pointer for the regulator registered
 *			for this thread
 * @name:		Unique name for this thread which is filled using the
 *			device tree regulator-name property
 * @corner:		Array of all corners supported by this thread
 * @corner_count:	The number of elements in the corner array
 * @platform_fuses:	Pointer to platform specific CPR fuse data (only used by
 *			platform specific CPR3 driver)
 * @speed_bin_fuse:	Value read from the speed bin fuse parameter
 * @cpr_rev_fuse:	Value read from the CPR fusing revision fuse parameter
 * @fuse_combo:		Platform specific enum value identifying the specific
 *			combination of fuse values found on a given chip
 * @fuse_corner_count:	Number of corners defined by fuse parameters
 * @step_volt:		Step size in microvolts between available set points
 *			of the VDD supply
 * @current_corner:	Index identifying the currently selected voltage corner
 *			for the thread or less than 0 if no corner has been
 *			requested
 * @vreg_enabled:	Boolean defining the state of the thread's regulator
 *			within the regulator framework.
 *
 * This structure contains both configuration and runtime state data.  The
 * elements current_corner and vreg_enabled are state variables.
 */
struct cpr3_thread {
	u32			thread_id;
	struct device_node	*of_node;
	struct cpr3_controller	*ctrl;
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	const char		*name;
	struct cpr3_corner	*corner;
	int			corner_count;
	void			*platform_fuses;
	int			speed_bin_fuse;
	int			cpr_rev_fuse;
	int			fuse_combo;
	int			fuse_corner_count;
	int			step_volt;
	int			current_corner;
	bool			vreg_enabled;
};

/* Per CPR controller data */
/**
 * struct cpr3_controller - CPR3 controller data structure
 * @dev:		Device pointer for the CPR3 controller device
 * @name:		Unique name for the CPR3 controller
 * @cpr_ctrl_base:	Virtual address of the CPR3 controller base register
 * @fuse_base:		Virtual address of fuse row 0
 * @list:		list head used in a global cpr3-regulator list so that
 *			cpr3-regulator structs can be found easily in RAM dumps
 * @thread:		Array of CPR3 threads managed by the CPR3 controller
 * @thread_count:	Number of elements in the thread array
 * @lock:		Mutex lock used to ensure mutual exclusion between
 *			all of the threads associated with the controller
 * @vdd_regulator:	Pointer to the VDD supply regulator which this CPR3
 *			controller manages
 * @apm:		Handle to the array power mux (APM)
 * @apm_threshold_volt:	APM threshold voltage in microvolts
 * @apm_adj_volt:	Minimum difference between APM threshold voltage and
 *			open-loop voltage which allows the APM threshold voltage
 *			to be used as a ceiling
 * @apm_high_supply:	APM supply to configure if VDD voltage is greater than
 *			or equal to the APM threshold voltage
 * @apm_low_supply:	APM supply to configure if the VDD voltage is less than
 *			the APM threshold voltage
 * @cpr_allowed:	Boolean which indicates if closed-loop CPR operation is
 *			permitted for a given chip
 * @aggr_corner:	CPR corner containing the most recently aggregated
 *			voltage configurations which are being used currently
 *
 * This structure contains both configuration and runtime state data.  The
 * element aggr_corner is a state variable.
 *
 * The apm* elements do not need to be initialized if the VDD supply managed by
 * the CPR3 controller does not utilize an APM.
 */
struct cpr3_controller {
	struct device		*dev;
	const char		*name;
	void __iomem		*cpr_ctrl_base;
	void __iomem		*fuse_base;
	struct list_head	list;
	struct cpr3_thread	*thread;
	int			thread_count;
	struct mutex		lock;
	struct regulator	*vdd_regulator;
	struct msm_apm_ctrl_dev *apm;
	int			apm_threshold_volt;
	int			apm_adj_volt;
	enum msm_apm_supply	apm_high_supply;
	enum msm_apm_supply	apm_low_supply;
	bool			cpr_allowed;
	struct cpr3_corner	aggr_corner;
};

/* Used for rounding voltages to the closest physically available set point. */
#define CPR3_ROUND(n, d) (DIV_ROUND_UP(n, d) * (d))

#define cpr3_err(cpr3_thread, message, ...) \
	pr_err("%s: " message, (cpr3_thread)->name, ##__VA_ARGS__)
#define cpr3_info(cpr3_thread, message, ...) \
	pr_info("%s: " message, (cpr3_thread)->name, ##__VA_ARGS__)
#define cpr3_debug(cpr3_thread, message, ...) \
	pr_debug("%s: " message, (cpr3_thread)->name, ##__VA_ARGS__)

/*
 * Offset subtracted from voltage corner values passed in from the regulator
 * framework in order to get internal voltage corner values.  This is needed
 * since the regulator framework treats 0 as an error value at regulator
 * registration time.
 */
#define CPR3_CORNER_OFFSET	1

#ifdef CONFIG_REGULATOR_CPR3

int cpr3_regulator_register(struct platform_device *pdev,
			struct cpr3_controller *ctrl);
int cpr3_regulator_unregister(struct cpr3_controller *ctrl);
int cpr3_regulator_suspend(struct cpr3_controller *ctrl);
int cpr3_regulator_resume(struct cpr3_controller *ctrl);

int cpr3_get_thread_name(struct cpr3_thread *thread,
			struct device_node *thread_node);
int cpr3_allocate_threads(struct cpr3_controller *ctrl, u32 min_thread_id,
			u32 max_thread_id);
int cpr3_map_fuse_base(struct cpr3_controller *ctrl,
			struct platform_device *pdev);
int cpr3_read_fuse_param(void __iomem *fuse_base_addr,
			const struct cpr3_fuse_param *param, u64 *param_value);
int cpr3_convert_open_loop_voltage_fuse(int ref_volt, int step_volt, u32 fuse,
			int fuse_len);
u64 cpr3_interpolate(u64 x1, u64 y1, u64 x2, u64 y2, u64 x);
int cpr3_parse_array_property(struct cpr3_thread *thread,
			const char *prop_name, int corner_count, int corner_sum,
			int combo_offset, u32 *out);
int cpr3_parse_common_corner_data(struct cpr3_thread *thread, int *corner_sum,
			int *combo_offset);
int cpr3_limit_open_loop_voltages(struct cpr3_thread *thread);

#else

static inline int cpr3_regulator_register(struct platform_device *pdev,
			struct cpr3_controller *ctrl)
{
	return -ENXIO;
}

static inline int cpr3_regulator_unregister(struct cpr3_controller *ctrl)
{
	return -ENXIO;
}

static inline int cpr3_regulator_suspend(struct cpr3_controller *ctrl)
{
	return -ENXIO;
}

static inline int cpr3_regulator_resume(struct cpr3_controller *ctrl)
{
	return -ENXIO;
}

static inline int cpr3_get_thread_name(struct cpr3_thread *thread,
			struct device_node *thread_node)
{
	return -EPERM;
}

static inline int cpr3_allocate_threads(struct cpr3_controller *ctrl,
			u32 min_thread_id, u32 max_thread_id)
{
	return -EPERM;
}

static inline int cpr3_map_fuse_base(struct cpr3_controller *ctrl,
			struct platform_device *pdev)
{
	return -ENXIO;
}

static inline int cpr3_read_fuse_param(void __iomem *fuse_base_addr,
			const struct cpr3_fuse_param *param, u64 *param_value)
{
	return -EPERM;
}

static inline int cpr3_convert_open_loop_voltage_fuse(int ref_volt,
			int step_volt, u32 fuse, int fuse_len)
{
	return -EPERM;
}

static inline u64 cpr3_interpolate(u64 x1, u64 y1, u64 x2, u64 y2, u64 x)
{
	return 0;
}

static inline int cpr3_parse_array_property(struct cpr3_thread *thread,
			const char *prop_name, int corner_count, int corner_sum,
			int combo_offset, u32 *out)
{
	return -EPERM;
}

static inline int cpr3_parse_common_corner_data(struct cpr3_thread *thread,
			int *corner_sum, int *combo_offset)
{
	return -EPERM;
}

static inline int cpr3_limit_open_loop_voltages(struct cpr3_thread *thread)
{
	return -EPERM;
}

#endif /* CONFIG_REGULATOR_CPR3 */

#endif /* __REGULATOR_CPR_REGULATOR_H__ */
