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

#ifndef __ARCH_ARM_MACH_MSM_RPM_REGULATOR_8660_H
#define __ARCH_ARM_MACH_MSM_RPM_REGULATOR_8660_H

#define RPM_VREG_PIN_CTRL_NONE	0x00
#define RPM_VREG_PIN_CTRL_A0	0x01
#define RPM_VREG_PIN_CTRL_A1	0x02
#define RPM_VREG_PIN_CTRL_D0	0x04
#define RPM_VREG_PIN_CTRL_D1	0x08

/*
 * Pin Function
 * ENABLE  - pin control switches between disable and enable
 * MODE    - pin control switches between LPM and HPM
 * SLEEP_B - regulator is forced into LPM by asserting sleep_b signal
 * NONE    - do not use pin control
 *
 * The pin function specified in platform data corresponds to the active state
 * pin function value.  Pin function will be NONE until a consumer requests
 * pin control with regulator_set_mode(vreg, REGULATOR_MODE_IDLE).
 */
enum rpm_vreg_pin_fn {
	RPM_VREG_PIN_FN_ENABLE = 0,
	RPM_VREG_PIN_FN_MODE,
	RPM_VREG_PIN_FN_SLEEP_B,
	RPM_VREG_PIN_FN_NONE,
};

enum rpm_vreg_mode {
	RPM_VREG_MODE_PIN_CTRL = 0,
	RPM_VREG_MODE_NONE = 0,
	RPM_VREG_MODE_LPM,
	RPM_VREG_MODE_HPM,
};

enum rpm_vreg_state {
	RPM_VREG_STATE_OFF = 0,
	RPM_VREG_STATE_ON,
};

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

enum rpm_vreg_id {
	RPM_VREG_ID_PM8058_L0 = 0,
	RPM_VREG_ID_PM8058_L1,
	RPM_VREG_ID_PM8058_L2,
	RPM_VREG_ID_PM8058_L3,
	RPM_VREG_ID_PM8058_L4,
	RPM_VREG_ID_PM8058_L5,
	RPM_VREG_ID_PM8058_L6,
	RPM_VREG_ID_PM8058_L7,
	RPM_VREG_ID_PM8058_L8,
	RPM_VREG_ID_PM8058_L9,
	RPM_VREG_ID_PM8058_L10,
	RPM_VREG_ID_PM8058_L11,
	RPM_VREG_ID_PM8058_L12,
	RPM_VREG_ID_PM8058_L13,
	RPM_VREG_ID_PM8058_L14,
	RPM_VREG_ID_PM8058_L15,
	RPM_VREG_ID_PM8058_L16,
	RPM_VREG_ID_PM8058_L17,
	RPM_VREG_ID_PM8058_L18,
	RPM_VREG_ID_PM8058_L19,
	RPM_VREG_ID_PM8058_L20,
	RPM_VREG_ID_PM8058_L21,
	RPM_VREG_ID_PM8058_L22,
	RPM_VREG_ID_PM8058_L23,
	RPM_VREG_ID_PM8058_L24,
	RPM_VREG_ID_PM8058_L25,
	RPM_VREG_ID_PM8058_S0,
	RPM_VREG_ID_PM8058_S1,
	RPM_VREG_ID_PM8058_S2,
	RPM_VREG_ID_PM8058_S3,
	RPM_VREG_ID_PM8058_S4,
	RPM_VREG_ID_PM8058_LVS0,
	RPM_VREG_ID_PM8058_LVS1,
	RPM_VREG_ID_PM8058_NCP,
	RPM_VREG_ID_PM8901_L0,
	RPM_VREG_ID_PM8901_L1,
	RPM_VREG_ID_PM8901_L2,
	RPM_VREG_ID_PM8901_L3,
	RPM_VREG_ID_PM8901_L4,
	RPM_VREG_ID_PM8901_L5,
	RPM_VREG_ID_PM8901_L6,
	RPM_VREG_ID_PM8901_S0,
	RPM_VREG_ID_PM8901_S1,
	RPM_VREG_ID_PM8901_S2,
	RPM_VREG_ID_PM8901_S3,
	RPM_VREG_ID_PM8901_S4,
	RPM_VREG_ID_PM8901_LVS0,
	RPM_VREG_ID_PM8901_LVS1,
	RPM_VREG_ID_PM8901_LVS2,
	RPM_VREG_ID_PM8901_LVS3,
	RPM_VREG_ID_PM8901_MVS0,
	RPM_VREG_ID_MAX,
};

/* Minimum high power mode loads in uA. */
#define RPM_VREG_LDO_50_HPM_MIN_LOAD	5000
#define RPM_VREG_LDO_150_HPM_MIN_LOAD	10000
#define RPM_VREG_LDO_300_HPM_MIN_LOAD	10000
#define RPM_VREG_SMPS_HPM_MIN_LOAD	50000
#define RPM_VREG_FTSMPS_HPM_MIN_LOAD	100000

/*
 * default_uV = initial voltage to set the regulator to if enable is called
 *		before set_voltage (e.g. when boot_on or always_on is set).
 * peak_uA    = initial load requirement sent in RPM request; used to determine
 *		initial mode.
 * avg_uA     = initial avg load requirement sent in RPM request; overwritten
 *		along with peak_uA when regulator_set_mode or
 *		regulator_set_optimum_mode is called.
 * pin_fn     = RPM_VREG_PIN_FN_ENABLE  - pin control ON/OFF
 *	      = RPM_VREG_PIN_FN_MODE    - pin control LPM/HPM
 *	      = RPM_VREG_PIN_FN_SLEEP_B - regulator is forced into LPM by
 *					  asserting sleep_b signal
 *	      = RPM_VREG_PIN_FN_NONE    - do not use pin control
 * mode	      = used to specify a force mode which overrides the votes of other
 *		RPM masters.
 * state      = initial state sent in RPM request.
 * sleep_selectable = flag which indicates that regulator should be accessable
 *		by external private API and that spinlocks should be used.
 */
struct rpm_vreg_pdata {
	struct regulator_init_data	init_data;
	int				default_uV;
	unsigned			peak_uA;
	unsigned			avg_uA;
	unsigned			pull_down_enable;
	unsigned			pin_ctrl;
	enum rpm_vreg_freq		freq;
	enum rpm_vreg_pin_fn		pin_fn;
	enum rpm_vreg_mode		mode;
	enum rpm_vreg_state		state;
	int				sleep_selectable;
};

enum rpm_vreg_voter {
	RPM_VREG_VOTER_REG_FRAMEWORK = 0, /* for internal use only */
	RPM_VREG_VOTER1,		  /* for use by the acpu-clock driver */
	RPM_VREG_VOTER2,		  /* for use by the acpu-clock driver */
	RPM_VREG_VOTER3,		  /* for use by other drivers */
	RPM_VREG_VOTER_COUNT,
};

#endif
