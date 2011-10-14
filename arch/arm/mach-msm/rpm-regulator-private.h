/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_RPM_REGULATOR_INT_H
#define __ARCH_ARM_MACH_MSM_RPM_REGULATOR_INT_H

#include <linux/regulator/driver.h>
#include <mach/rpm.h>
#include <mach/rpm-regulator.h>

/* Possible RPM regulator request types */
enum rpm_regulator_type {
	RPM_REGULATOR_TYPE_LDO,
	RPM_REGULATOR_TYPE_SMPS,
	RPM_REGULATOR_TYPE_VS,
	RPM_REGULATOR_TYPE_NCP,
	RPM_REGULATOR_TYPE_MAX = RPM_REGULATOR_TYPE_NCP,
};

struct request_member {
	int			word;
	unsigned int		mask;
	int			shift;
};

/* Possible RPM regulator request members */
struct rpm_vreg_parts {
	struct request_member	mV;	/* voltage: used if voltage is in mV */
	struct request_member	uV;	/* voltage: used if voltage is in uV */
	struct request_member	ip;		/* peak current in mA */
	struct request_member	pd;		/* pull down enable */
	struct request_member	ia;		/* average current in mA */
	struct request_member	fm;		/* force mode */
	struct request_member	pm;		/* power mode */
	struct request_member	pc;		/* pin control */
	struct request_member	pf;		/* pin function */
	struct request_member	enable_state;	/* NCP and switch */
	struct request_member	comp_mode;	/* NCP */
	struct request_member	freq;		/* frequency: NCP and SMPS */
	struct request_member	freq_clk_src;	/* clock source: SMPS */
	struct request_member	hpm;		/* switch: control OCP and SS */
	int			request_len;
};

struct vreg_range {
	int			min_uV;
	int			max_uV;
	int			step_uV;
	unsigned		n_voltages;
};

struct vreg_set_points {
	struct vreg_range	*range;
	int			count;
	unsigned		n_voltages;
};

struct vreg {
	struct msm_rpm_iv_pair		req[2];
	struct msm_rpm_iv_pair		prev_active_req[2];
	struct msm_rpm_iv_pair		prev_sleep_req[2];
	struct rpm_regulator_init_data	pdata;
	struct regulator_desc		rdesc;
	struct regulator_desc		rdesc_pc;
	struct regulator_dev		*rdev;
	struct regulator_dev		*rdev_pc;
	struct vreg_set_points		*set_points;
	struct rpm_vreg_parts		*part;
	int				type;
	int				id;
	struct mutex			pc_lock;
	int				save_uV;
	int				mode;
	bool				is_enabled;
	bool				is_enabled_pc;
	const int			hpm_min_load;
	int			       active_min_uV_vote[RPM_VREG_VOTER_COUNT];
	int				sleep_min_uV_vote[RPM_VREG_VOTER_COUNT];
};

struct vreg_config {
	struct vreg			*vregs;
	int				vregs_len;

	int				vreg_id_min;
	int				vreg_id_max;

	int				pin_func_none;
	int				pin_func_sleep_b;

	unsigned int			mode_lpm;
	unsigned int			mode_hpm;

	struct vreg_set_points		**set_points;
	int				set_points_len;

	const char			**label_pin_ctrl;
	int				label_pin_ctrl_len;
	const char			**label_pin_func;
	int				label_pin_func_len;
	const char			**label_force_mode;
	int				label_force_mode_len;
	const char			**label_power_mode;
	int				label_power_mode_len;

	int				(*is_real_id) (int vreg_id);
	int				(*pc_id_to_real_id) (int vreg_id);

	/* Legacy options to be used with MSM8660 */
	int				use_legacy_optimum_mode;
	int				ia_follows_ip;
};

#define REQUEST_MEMBER(_word, _mask, _shift) \
	{ \
		.word	= _word, \
		.mask	= _mask, \
		.shift	= _shift, \
	}

#define VOLTAGE_RANGE(_min_uV, _max_uV, _step_uV) \
	{ \
		.min_uV  = _min_uV, \
		.max_uV  = _max_uV, \
		.step_uV = _step_uV, \
	}

#define SET_POINTS(_ranges) \
{ \
	.range	= _ranges, \
	.count	= ARRAY_SIZE(_ranges), \
};

#define MICRO_TO_MILLI(uV)			((uV) / 1000)
#define MILLI_TO_MICRO(mV)			((mV) * 1000)

#if defined(CONFIG_ARCH_MSM8X60)
struct vreg_config *get_config_8660(void);
#else
static inline struct vreg_config *get_config_8660(void)
{
	return NULL;
}
#endif

#if defined(CONFIG_ARCH_MSM8960) || defined(CONFIG_ARCH_APQ8064)
struct vreg_config *get_config_8960(void);
#else
static inline struct vreg_config *get_config_8960(void)
{
	return NULL;
}
#endif

#if defined(CONFIG_ARCH_MSM9615)
struct vreg_config *get_config_9615(void);
#else
static inline struct vreg_config *get_config_9615(void)
{
	return NULL;
}
#endif

#endif
