/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_INCLUDE_MACH_RPM_REGULATOR_H
#define __ARCH_ARM_MACH_MSM_INCLUDE_MACH_RPM_REGULATOR_H

#include <linux/regulator/machine.h>

#define RPM_REGULATOR_DEV_NAME "rpm-regulator"

#include <mach/rpm-regulator-8660.h>
#include <mach/rpm-regulator-8960.h>
#include <mach/rpm-regulator-9615.h>
#include <mach/rpm-regulator-8974.h>
#include <mach/rpm-regulator-8930.h>

/**
 * enum rpm_vreg_version - supported RPM regulator versions
 */
enum rpm_vreg_version {
	RPM_VREG_VERSION_8660,
	RPM_VREG_VERSION_8960,
	RPM_VREG_VERSION_9615,
	RPM_VREG_VERSION_8930,
	RPM_VREG_VERSION_MAX = RPM_VREG_VERSION_8930,
};

#define RPM_VREG_PIN_CTRL_NONE		0x00

/**
 * enum rpm_vreg_state - enable state for switch or NCP
 */
enum rpm_vreg_state {
	RPM_VREG_STATE_OFF,
	RPM_VREG_STATE_ON,
};

/**
 * enum rpm_vreg_freq - switching frequency for SMPS or NCP
 */
enum rpm_vreg_freq {
	RPM_VREG_FREQ_NONE,
	RPM_VREG_FREQ_19p20,
	RPM_VREG_FREQ_9p60,
	RPM_VREG_FREQ_6p40,
	RPM_VREG_FREQ_4p80,
	RPM_VREG_FREQ_3p84,
	RPM_VREG_FREQ_3p20,
	RPM_VREG_FREQ_2p74,
	RPM_VREG_FREQ_2p40,
	RPM_VREG_FREQ_2p13,
	RPM_VREG_FREQ_1p92,
	RPM_VREG_FREQ_1p75,
	RPM_VREG_FREQ_1p60,
	RPM_VREG_FREQ_1p48,
	RPM_VREG_FREQ_1p37,
	RPM_VREG_FREQ_1p28,
	RPM_VREG_FREQ_1p20,
};

/**
 * enum rpm_vreg_voltage_corner - possible voltage corner values
 *
 * These should be used in regulator_set_voltage and rpm_vreg_set_voltage calls
 * for corner type regulators as if they had units of uV.
 */
enum rpm_vreg_voltage_corner {
	RPM_VREG_CORNER_NONE = 1,
	RPM_VREG_CORNER_LOW,
	RPM_VREG_CORNER_NOMINAL,
	RPM_VREG_CORNER_HIGH,
};

/**
 * enum rpm_vreg_voter - RPM regulator voter IDs for private APIs
 */
enum rpm_vreg_voter {
	RPM_VREG_VOTER_REG_FRAMEWORK,	/* for internal use only */
	RPM_VREG_VOTER1,		/* for use by the acpu-clock driver */
	RPM_VREG_VOTER2,		/* for use by the acpu-clock driver */
	RPM_VREG_VOTER3,		/* for use by other drivers */
	RPM_VREG_VOTER4,		/* for use by the acpu-clock driver */
	RPM_VREG_VOTER5,		/* for use by the acpu-clock driver */
	RPM_VREG_VOTER6,		/* for use by the acpu-clock driver */
	RPM_VREG_VOTER_COUNT,
};

/**
 * struct rpm_regulator_init_data - RPM regulator initialization data
 * @init_data:		regulator constraints
 * @id:			regulator id; from enum rpm_vreg_id
 * @sleep_selectable:	flag which indicates that regulator should be accessable
 *			by external private API and that spinlocks should be
 *			used instead of mutex locks
 * @system_uA:		current drawn from regulator not accounted for by any
 *			regulator framework consumer
 * @enable_time:	time in us taken to enable a regulator to the maximum
 *			allowed voltage for the system.  This is dependent upon
 *			the load and capacitance for a regulator on the board.
 * @pull_down_enable:	0 = no pulldown, 1 = pulldown when regulator disabled
 * @freq:		enum value representing the switching frequency of an
 *			SMPS or NCP
 * @pin_ctrl:		pin control inputs to use for the regulator; should be
 *			a combination of RPM_VREG_PIN_CTRL_* values
 * @pin_fn:		action to perform when pin control pin(s) is/are active
 * @force_mode:		used to specify a force mode which overrides the votes
 *			of other RPM masters.
 * @sleep_set_force_mode: force mode to use in sleep-set requests
 * @power_mode:		mode to use as HPM (typically PWM or hysteretic) when
 *			utilizing Auto mode selection
 * @default_uV:		initial voltage to set the regulator to if enable is
 *			called before set_voltage (e.g. when boot_on or
 *			always_on is set).
 * @peak_uA:		initial peak load requirement sent in RPM request; used
 *			to determine initial mode.
 * @avg_uA:		average load requirement sent in RPM request
 * @state:		initial enable state sent in RPM request for switch or
 *			NCP
 */
struct rpm_regulator_init_data {
	struct regulator_init_data	init_data;
	int				id;
	int				sleep_selectable;
	int				system_uA;
	int				enable_time;
	unsigned			pull_down_enable;
	enum rpm_vreg_freq		freq;
	unsigned			pin_ctrl;
	int				pin_fn;
	int				force_mode;
	int				sleep_set_force_mode;
	int				power_mode;
	int				default_uV;
	unsigned			peak_uA;
	unsigned			avg_uA;
	enum rpm_vreg_state		state;
};

/**
 * struct rpm_regulator_consumer_mapping - mapping used by private consumers
 */
struct rpm_regulator_consumer_mapping {
	const char		*dev_name;
	const char		*supply;
	int			vreg_id;
	enum rpm_vreg_voter	voter;
	int			sleep_also;
};

/**
 * struct rpm_regulator_platform_data - RPM regulator platform data
 */
struct rpm_regulator_platform_data {
	struct rpm_regulator_init_data		*init_data;
	int					num_regulators;
	enum rpm_vreg_version			version;
	int					vreg_id_vdd_mem;
	int					vreg_id_vdd_dig;
	bool					requires_tcxo_workaround;
	struct rpm_regulator_consumer_mapping	*consumer_map;
	int					consumer_map_len;
};

#ifdef CONFIG_MSM_RPM_REGULATOR
/**
 * rpm_vreg_set_voltage - vote for a min_uV value of specified regualtor
 * @vreg: ID for regulator
 * @voter: ID for the voter
 * @min_uV: minimum acceptable voltage (in uV) that is voted for
 * @max_uV: maximum acceptable voltage (in uV) that is voted for
 * @sleep_also: 0 for active set only, non-0 for active set and sleep set
 *
 * Returns 0 on success or errno.
 *
 * This function is used to vote for the voltage of a regulator without
 * using the regulator framework.  It is needed by consumers which hold spin
 * locks or have interrupts disabled because the regulator framework can sleep.
 * It is also needed by consumers which wish to only vote for active set
 * regulator voltage.
 *
 * If sleep_also == 0, then a sleep-set value of 0V will be voted for.
 *
 * This function may only be called for regulators which have the sleep flag
 * specified in their private data.
 *
 * Consumers can vote to disable a regulator with this function by passing
 * min_uV = 0 and max_uV = 0.
 *
 * Voltage switch type regulators may be controlled via rpm_vreg_set_voltage
 * as well.  For this type of regulator, max_uV > 0 is treated as an enable
 * request and max_uV == 0 is treated as a disable request.
 */
int rpm_vreg_set_voltage(int vreg_id, enum rpm_vreg_voter voter, int min_uV,
			 int max_uV, int sleep_also);

/**
 * rpm_vreg_set_frequency - sets the frequency of a switching regulator
 * @vreg: ID for regulator
 * @freq: enum corresponding to desired frequency
 *
 * Returns 0 on success or errno.
 */
int rpm_vreg_set_frequency(int vreg_id, enum rpm_vreg_freq freq);

#else

/*
 * These stubs exist to allow consumers of these APIs to compile and run
 * in absence of a real RPM regulator driver. It is assumed that they are
 * aware of the state of their regulators and have either set them
 * correctly by some other means or don't care about their state at all.
 */
static inline int rpm_vreg_set_voltage(int vreg_id, enum rpm_vreg_voter voter,
				       int min_uV, int max_uV, int sleep_also)
{
	return 0;
}

static inline int rpm_vreg_set_frequency(int vreg_id, enum rpm_vreg_freq freq)
{
	return 0;
}

#endif /* CONFIG_MSM_RPM_REGULATOR */

#endif
