/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/pmic8901.h>
#include <mach/rpm.h>
#include <mach/rpm-regulator.h>

#include "rpm_resources.h"

/* Debug Definitions */

enum {
	MSM_RPM_VREG_DEBUG_REQUEST = BIT(0),
	MSM_RPM_VREG_DEBUG_VOTE = BIT(1),
	MSM_RPM_VREG_DEBUG_DUPLICATE = BIT(2),
	MSM_RPM_VREG_DEBUG_IGNORE_8058_S0_S1 = BIT(3),
};

static int msm_rpm_vreg_debug_mask;
module_param_named(
	debug_mask, msm_rpm_vreg_debug_mask, int, S_IRUSR | S_IWUSR
);

#define MICRO_TO_MILLI(uV)			((uV) / 1000)
#define MILLI_TO_MICRO(mV)			((mV) * 1000)

/* LDO register word 1 */
#define LDO_VOLTAGE				0x00000FFF
#define LDO_VOLTAGE_SHIFT			0
#define LDO_PEAK_CURRENT			0x00FFF000
#define LDO_PEAK_CURRENT_SHIFT			12
#define LDO_MODE				0x03000000
#define LDO_MODE_SHIFT				24
#define LDO_PIN_CTRL				0x3C000000
#define LDO_PIN_CTRL_SHIFT			26
#define LDO_PIN_FN				0xC0000000
#define LDO_PIN_FN_SHIFT			30

/* LDO register word 2 */
#define LDO_PULL_DOWN_ENABLE			0x00000001
#define LDO_PULL_DOWN_ENABLE_SHIFT		0
#define LDO_AVG_CURRENT				0x00001FFE
#define LDO_AVG_CURRENT_SHIFT			1

/* SMPS register word 1 */
#define SMPS_VOLTAGE				0x00000FFF
#define SMPS_VOLTAGE_SHIFT			0
#define SMPS_PEAK_CURRENT			0x00FFF000
#define SMPS_PEAK_CURRENT_SHIFT			12
#define SMPS_MODE				0x03000000
#define SMPS_MODE_SHIFT				24
#define SMPS_PIN_CTRL				0x3C000000
#define SMPS_PIN_CTRL_SHIFT			26
#define SMPS_PIN_FN				0xC0000000
#define SMPS_PIN_FN_SHIFT			30

/* SMPS register word 2 */
#define SMPS_PULL_DOWN_ENABLE			0x00000001
#define SMPS_PULL_DOWN_ENABLE_SHIFT		0
#define SMPS_AVG_CURRENT			0x00001FFE
#define SMPS_AVG_CURRENT_SHIFT			1
#define SMPS_FREQ				0x001FE000
#define SMPS_FREQ_SHIFT				13
#define SMPS_CLK_SRC				0x00600000
#define SMPS_CLK_SRC_SHIFT			21

/* SWITCH register word 1 */
#define SWITCH_STATE				0x0001
#define SWITCH_STATE_SHIFT			0
#define SWITCH_PULL_DOWN_ENABLE			0x0002
#define SWITCH_PULL_DOWN_ENABLE_SHIFT		1
#define SWITCH_PIN_CTRL				0x003C
#define SWITCH_PIN_CTRL_SHIFT			2
#define SWITCH_PIN_FN				0x00C0
#define SWITCH_PIN_FN_SHIFT			6

/* NCP register word 1 */
#define NCP_VOLTAGE				0x0FFF
#define NCP_VOLTAGE_SHIFT			0
#define NCP_STATE				0x1000
#define NCP_STATE_SHIFT				12

/*
 * This is used when voting for LPM or HPM by subtracting or adding to the
 * hpm_min_load of a regulator.  It has units of uA.
 */
#define LOAD_THRESHOLD_STEP			1000

/* This is the maximum uA load that can be passed to the RPM. */
#define MAX_POSSIBLE_LOAD			(MILLI_TO_MICRO(0xFFF))

/* Voltage regulator types */
#define IS_LDO(id)	((id >= RPM_VREG_ID_PM8058_L0 && \
			  id <= RPM_VREG_ID_PM8058_L25) || \
			 (id >= RPM_VREG_ID_PM8901_L0 && \
			  id <= RPM_VREG_ID_PM8901_L6))
#define IS_SMPS(id)	((id >= RPM_VREG_ID_PM8058_S0 && \
			  id <= RPM_VREG_ID_PM8058_S4) || \
			 (id >= RPM_VREG_ID_PM8901_S0 && \
			  id <= RPM_VREG_ID_PM8901_S4))
#define IS_SWITCH(id)	((id >= RPM_VREG_ID_PM8058_LVS0 && \
			  id <= RPM_VREG_ID_PM8058_LVS1) || \
			 (id >= RPM_VREG_ID_PM8901_LVS0 && \
			  id <= RPM_VREG_ID_PM8901_LVS3) || \
			 (id == RPM_VREG_ID_PM8901_MVS0))
#define IS_NCP(id)	(id == RPM_VREG_ID_PM8058_NCP)

#define IS_8901_SMPS(id) ((id >= RPM_VREG_ID_PM8901_S0 && \
			  id <= RPM_VREG_ID_PM8901_S4))

struct vreg {
	struct msm_rpm_iv_pair	req[2];
	struct msm_rpm_iv_pair	prev_active_req[2];
	struct msm_rpm_iv_pair	prev_sleep_req[2];
	struct rpm_vreg_pdata	*pdata;
	int			save_uV;
	const int		hpm_min_load;
	unsigned		pc_vote;
	unsigned		optimum;
	unsigned		mode_initialized;
	int			active_min_mV_vote[RPM_VREG_VOTER_COUNT];
	int			sleep_min_mV_vote[RPM_VREG_VOTER_COUNT];
	enum rpm_vreg_id	id;
};

#define RPM_VREG_NCP_HPM_MIN_LOAD	0

#define VREG_2(_vreg_id, _rpm_id, _hpm_min_load) \
	[RPM_VREG_ID_##_vreg_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_##_rpm_id##_1, }, \
		}, \
		.hpm_min_load = RPM_VREG_##_hpm_min_load, \
	}

#define VREG_1(_vreg_id, _rpm_id) \
	[RPM_VREG_ID_##_vreg_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id, }, \
			[1] = { .id = -1, }, \
		}, \
	}

static struct vreg vregs[RPM_VREG_ID_MAX] = {
	VREG_2(PM8058_L0, LDO0, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L1, LDO1, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L2, LDO2, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L3, LDO3, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L4, LDO4, LDO_50_HPM_MIN_LOAD),
	VREG_2(PM8058_L5, LDO5, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L6, LDO6, LDO_50_HPM_MIN_LOAD),
	VREG_2(PM8058_L7, LDO7, LDO_50_HPM_MIN_LOAD),
	VREG_2(PM8058_L8, LDO8, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L9, LDO9, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L10, LDO10, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L11, LDO11, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L12, LDO12, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L13, LDO13, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L14, LDO14, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L15, LDO15, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L16, LDO16, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L17, LDO17, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L18, LDO18, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L19, LDO19, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L20, LDO20, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L21, LDO21, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L22, LDO22, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L23, LDO23, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8058_L24, LDO24, LDO_150_HPM_MIN_LOAD),
	VREG_2(PM8058_L25, LDO25, LDO_150_HPM_MIN_LOAD),

	VREG_2(PM8058_S0, SMPS0, SMPS_HPM_MIN_LOAD),
	VREG_2(PM8058_S1, SMPS1, SMPS_HPM_MIN_LOAD),
	VREG_2(PM8058_S2, SMPS2, SMPS_HPM_MIN_LOAD),
	VREG_2(PM8058_S3, SMPS3, SMPS_HPM_MIN_LOAD),
	VREG_2(PM8058_S4, SMPS4, SMPS_HPM_MIN_LOAD),

	VREG_1(PM8058_LVS0, LVS0),
	VREG_1(PM8058_LVS1, LVS1),

	VREG_2(PM8058_NCP, NCP, NCP_HPM_MIN_LOAD),

	VREG_2(PM8901_L0, LDO0B, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8901_L1, LDO1B, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8901_L2, LDO2B, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8901_L3, LDO3B, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8901_L4, LDO4B, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8901_L5, LDO5B, LDO_300_HPM_MIN_LOAD),
	VREG_2(PM8901_L6, LDO6B, LDO_300_HPM_MIN_LOAD),

	VREG_2(PM8901_S0, SMPS0B, FTSMPS_HPM_MIN_LOAD),
	VREG_2(PM8901_S1, SMPS1B, FTSMPS_HPM_MIN_LOAD),
	VREG_2(PM8901_S2, SMPS2B, FTSMPS_HPM_MIN_LOAD),
	VREG_2(PM8901_S3, SMPS3B, FTSMPS_HPM_MIN_LOAD),
	VREG_2(PM8901_S4, SMPS4B, FTSMPS_HPM_MIN_LOAD),

	VREG_1(PM8901_LVS0, LVS0B),
	VREG_1(PM8901_LVS1, LVS1B),
	VREG_1(PM8901_LVS2, LVS2B),
	VREG_1(PM8901_LVS3, LVS3B),

	VREG_1(PM8901_MVS0, MVS),
};

static void print_rpm_request(struct vreg *vreg, int set);
static void print_rpm_vote(struct vreg *vreg, enum rpm_vreg_voter voter,
			int set, int voter_mV, int aggregate_mV);
static void print_rpm_duplicate(struct vreg *vreg, int set, int cnt);

static unsigned int smps_get_mode(struct regulator_dev *dev);
static unsigned int ldo_get_mode(struct regulator_dev *dev);
static unsigned int switch_get_mode(struct regulator_dev *dev);

/* Spin lock needed for sleep-selectable regulators. */
static DEFINE_SPINLOCK(pm8058_noirq_lock);

static int voltage_from_req(struct vreg *vreg)
{
	int shift = 0;
	uint32_t value = 0, mask = 0;

	value = vreg->req[0].value;

	if (IS_SMPS(vreg->id)) {
		mask = SMPS_VOLTAGE;
		shift = SMPS_VOLTAGE_SHIFT;
	} else if (IS_LDO(vreg->id)) {
		mask = LDO_VOLTAGE;
		shift = LDO_VOLTAGE_SHIFT;
	} else if (IS_NCP(vreg->id)) {
		mask = NCP_VOLTAGE;
		shift = NCP_VOLTAGE_SHIFT;
	}

	return (value & mask) >> shift;
}

static void voltage_to_req(int voltage, struct vreg *vreg)
{
	int shift = 0;
	uint32_t *value = NULL, mask = 0;

	value = &(vreg->req[0].value);

	if (IS_SMPS(vreg->id)) {
		mask = SMPS_VOLTAGE;
		shift = SMPS_VOLTAGE_SHIFT;
	} else if (IS_LDO(vreg->id)) {
		mask = LDO_VOLTAGE;
		shift = LDO_VOLTAGE_SHIFT;
	} else if (IS_NCP(vreg->id)) {
		mask = NCP_VOLTAGE;
		shift = NCP_VOLTAGE_SHIFT;
	}

	*value &= ~mask;
	*value |= (voltage << shift) & mask;
}

static int vreg_send_request(struct vreg *vreg, enum rpm_vreg_voter voter,
			  int set, unsigned mask0, unsigned val0,
			  unsigned mask1, unsigned val1, unsigned cnt,
			  int update_voltage)
{
	struct msm_rpm_iv_pair *prev_req;
	int rc = 0, max_mV_vote = 0, i;
	unsigned prev0, prev1;
	int *min_mV_vote;

	if (set == MSM_RPM_CTX_SET_0) {
		min_mV_vote = vreg->active_min_mV_vote;
		prev_req = vreg->prev_active_req;
	} else {
		min_mV_vote = vreg->sleep_min_mV_vote;
		prev_req = vreg->prev_sleep_req;
	}

	prev0 = vreg->req[0].value;
	vreg->req[0].value &= ~mask0;
	vreg->req[0].value |= val0 & mask0;

	prev1 = vreg->req[1].value;
	vreg->req[1].value &= ~mask1;
	vreg->req[1].value |= val1 & mask1;

	if (update_voltage)
		min_mV_vote[voter] = voltage_from_req(vreg);

	/* Find the highest voltage voted for and use it. */
	for (i = 0; i < RPM_VREG_VOTER_COUNT; i++)
		max_mV_vote = max(max_mV_vote, min_mV_vote[i]);
	voltage_to_req(max_mV_vote, vreg);

	if (msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_VOTE)
		print_rpm_vote(vreg, voter, set, min_mV_vote[voter],
				max_mV_vote);

	/* Ignore duplicate requests */
	if (vreg->req[0].value != prev_req[0].value ||
	    vreg->req[1].value != prev_req[1].value) {

		rc = msm_rpmrs_set_noirq(set, vreg->req, cnt);
		if (rc) {
			vreg->req[0].value = prev0;
			vreg->req[1].value = prev1;

			pr_err("%s: msm_rpmrs_set_noirq failed - "
				"set=%s, id=%d, rc=%d\n", __func__,
				(set == MSM_RPM_CTX_SET_0 ? "active" : "sleep"),
				vreg->req[0].id, rc);
		} else {
			/* Only save if nonzero and active set. */
			if (max_mV_vote && (set == MSM_RPM_CTX_SET_0))
				vreg->save_uV = MILLI_TO_MICRO(max_mV_vote);
			if (msm_rpm_vreg_debug_mask
			    & MSM_RPM_VREG_DEBUG_REQUEST)
				print_rpm_request(vreg, set);
			prev_req[0].value = vreg->req[0].value;
			prev_req[1].value = vreg->req[1].value;
		}
	} else if (msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_DUPLICATE) {
		print_rpm_duplicate(vreg, set, cnt);
	}

	return rc;
}

static int vreg_set_noirq(struct vreg *vreg, enum rpm_vreg_voter voter,
			  int sleep, unsigned mask0, unsigned val0,
			  unsigned mask1, unsigned val1, unsigned cnt,
			  int update_voltage)
{
	unsigned long flags;
	int rc;
	unsigned val0_sleep, mask0_sleep;

	if (voter < 0 || voter >= RPM_VREG_VOTER_COUNT)
		return -EINVAL;

	spin_lock_irqsave(&pm8058_noirq_lock, flags);

	/*
	 * Send sleep set request first so that subsequent set_mode, etc calls
	 * use the voltage from the active set.
	 */
	if (sleep)
		rc = vreg_send_request(vreg, voter, MSM_RPM_CTX_SET_SLEEP,
				mask0, val0, mask1, val1, cnt, update_voltage);
	else {
		/*
		 * Vote for 0 V in the sleep set when active set-only is
		 * specified.  This ensures that a disable vote will be issued
		 * at some point for the sleep set of the regulator.
		 */
		val0_sleep = val0;
		mask0_sleep = mask0;
		if (IS_SMPS(vreg->id)) {
			val0_sleep &= ~SMPS_VOLTAGE;
			mask0_sleep |= SMPS_VOLTAGE;
		} else if (IS_LDO(vreg->id)) {
			val0_sleep &= ~LDO_VOLTAGE;
			mask0_sleep |= LDO_VOLTAGE;
		} else if (IS_NCP(vreg->id)) {
			val0_sleep &= ~NCP_VOLTAGE;
			mask0_sleep |= NCP_VOLTAGE;
		}

		rc = vreg_send_request(vreg, voter, MSM_RPM_CTX_SET_SLEEP,
					mask0_sleep, val0_sleep,
					mask1, val1, cnt, update_voltage);
	}

	rc = vreg_send_request(vreg, voter, MSM_RPM_CTX_SET_0, mask0, val0,
					mask1, val1, cnt, update_voltage);

	spin_unlock_irqrestore(&pm8058_noirq_lock, flags);

	return rc;
}

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
 */
int rpm_vreg_set_voltage(enum rpm_vreg_id vreg_id, enum rpm_vreg_voter voter,
			 int min_uV, int max_uV, int sleep_also)
{
	int rc;
	unsigned val0 = 0, val1 = 0, mask0 = 0, mask1 = 0, cnt = 2;

	if (vreg_id < 0 || vreg_id >= RPM_VREG_ID_MAX)
		return -EINVAL;

	if (!vregs[vreg_id].pdata->sleep_selectable)
		return -EINVAL;

	if (min_uV < vregs[vreg_id].pdata->init_data.constraints.min_uV ||
	    min_uV > vregs[vreg_id].pdata->init_data.constraints.max_uV)
		return -EINVAL;

	if (IS_SMPS(vreg_id)) {
		mask0 = SMPS_VOLTAGE;
		val0 = MICRO_TO_MILLI(min_uV) << SMPS_VOLTAGE_SHIFT;
	} else if (IS_LDO(vreg_id)) {
		mask0 = LDO_VOLTAGE;
		val0 = MICRO_TO_MILLI(min_uV) << LDO_VOLTAGE_SHIFT;
	} else if (IS_NCP(vreg_id)) {
		mask0 = NCP_VOLTAGE;
		val0 = MICRO_TO_MILLI(min_uV) << NCP_VOLTAGE_SHIFT;
		cnt = 1;
	} else {
		cnt = 1;
	}

	rc = vreg_set_noirq(&vregs[vreg_id], voter, sleep_also, mask0, val0,
			    mask1, val1, cnt, 1);

	return rc;
}
EXPORT_SYMBOL_GPL(rpm_vreg_set_voltage);

/**
 * rpm_vreg_set_frequency - sets the frequency of a switching regulator
 * @vreg: ID for regulator
 * @min_uV: minimum acceptable frequency of operation
 *
 * Returns 0 on success or errno.
 */
int rpm_vreg_set_frequency(enum rpm_vreg_id vreg_id, enum rpm_vreg_freq freq)
{
	unsigned val0 = 0, val1 = 0, mask0 = 0, mask1 = 0, cnt = 2;
	int rc;

	if (vreg_id < 0 || vreg_id >= RPM_VREG_ID_MAX) {
		pr_err("%s: invalid regulator id=%d\n", __func__, vreg_id);
		return -EINVAL;
	}

	if (freq < 0 || freq > RPM_VREG_FREQ_1p20) {
		pr_err("%s: invalid frequency=%d\n", __func__, freq);
		return -EINVAL;
	}

	if (!IS_SMPS(vreg_id)) {
		pr_err("%s: regulator id=%d does not support frequency\n",
			__func__, vreg_id);
		return -EINVAL;
	}

	if (!vregs[vreg_id].pdata->sleep_selectable) {
		pr_err("%s: regulator id=%d is not marked sleep selectable\n",
			__func__, vreg_id);
		return -EINVAL;
	}

	mask1 = SMPS_FREQ;
	val1 = freq << SMPS_FREQ_SHIFT;

	rc = vreg_set_noirq(&vregs[vreg_id], RPM_VREG_VOTER_REG_FRAMEWORK,
			    1, mask0, val0, mask1, val1, cnt, 0);

	return rc;
}
EXPORT_SYMBOL_GPL(rpm_vreg_set_frequency);

#define IS_PMIC_8901_V1(rev)		((rev) == PM_8901_REV_1p0 || \
					 (rev) == PM_8901_REV_1p1)

#define PMIC_8901_V1_SCALE(uV)		((((uV) - 62100) * 23) / 25)

static inline int vreg_hpm_min_uA(struct vreg *vreg)
{
	return vreg->hpm_min_load;
}

static inline int vreg_lpm_max_uA(struct vreg *vreg)
{
	return vreg->hpm_min_load - LOAD_THRESHOLD_STEP;
}

static inline unsigned saturate_load(unsigned load_uA)
{
	return (load_uA > MAX_POSSIBLE_LOAD ? MAX_POSSIBLE_LOAD : load_uA);
}

/* Change vreg->req, but do not send it to the RPM. */
static int vreg_store(struct vreg *vreg, unsigned mask0, unsigned val0,
		unsigned mask1, unsigned val1)
{
	unsigned long flags = 0;

	if (vreg->pdata->sleep_selectable)
		spin_lock_irqsave(&pm8058_noirq_lock, flags);

	vreg->req[0].value &= ~mask0;
	vreg->req[0].value |= val0 & mask0;

	vreg->req[1].value &= ~mask1;
	vreg->req[1].value |= val1 & mask1;

	if (vreg->pdata->sleep_selectable)
		spin_unlock_irqrestore(&pm8058_noirq_lock, flags);

	return 0;
}

static int vreg_set(struct vreg *vreg, unsigned mask0, unsigned val0,
		unsigned mask1, unsigned val1, unsigned cnt)
{
	unsigned prev0 = 0, prev1 = 0;
	int rc;

	/*
	 * Bypass the normal route for regulators that can be called to change
	 * just the active set values.
	 */
	if (vreg->pdata->sleep_selectable)
		return vreg_set_noirq(vreg, RPM_VREG_VOTER_REG_FRAMEWORK, 1,
					mask0, val0, mask1, val1, cnt, 1);

	prev0 = vreg->req[0].value;
	vreg->req[0].value &= ~mask0;
	vreg->req[0].value |= val0 & mask0;

	prev1 = vreg->req[1].value;
	vreg->req[1].value &= ~mask1;
	vreg->req[1].value |= val1 & mask1;

	/* Ignore duplicate requests */
	if (vreg->req[0].value == vreg->prev_active_req[0].value &&
	    vreg->req[1].value == vreg->prev_active_req[1].value) {
		if (msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_DUPLICATE)
			print_rpm_duplicate(vreg, MSM_RPM_CTX_SET_0, cnt);
		return 0;
	}

	rc = msm_rpm_set(MSM_RPM_CTX_SET_0, vreg->req, cnt);
	if (rc) {
		vreg->req[0].value = prev0;
		vreg->req[1].value = prev1;

		pr_err("%s: msm_rpm_set fail id=%d, rc=%d\n",
				__func__, vreg->req[0].id, rc);
	} else {
		if (msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_REQUEST)
			print_rpm_request(vreg, MSM_RPM_CTX_SET_0);
		vreg->prev_active_req[0].value = vreg->req[0].value;
		vreg->prev_active_req[1].value = vreg->req[1].value;
	}

	return rc;
}

static int smps_is_enabled(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	return ((vreg->req[0].value & SMPS_VOLTAGE) >> SMPS_VOLTAGE_SHIFT) != 0;
}

static int _smps_set_voltage(struct regulator_dev *dev, int min_uV)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	int scaled_min_uV = min_uV;
	static int pmic8901_rev;

	/* Scale input request voltage down if using v1 PMIC 8901. */
	if (IS_8901_SMPS(vreg->id) && min_uV) {
		if (pmic8901_rev <= 0)
			pmic8901_rev = pm8901_rev(NULL);

		if (pmic8901_rev < 0)
			pr_err("%s: setting %s to %d uV; PMIC 8901 revision "
				"unavailable, no scaling can be performed.\n",
				__func__, dev->desc->name, min_uV);
		else if (IS_PMIC_8901_V1(pmic8901_rev))
			scaled_min_uV = PMIC_8901_V1_SCALE(min_uV);
	}

	return vreg_set(vreg, SMPS_VOLTAGE,
			MICRO_TO_MILLI(scaled_min_uV) << SMPS_VOLTAGE_SHIFT,
			0, 0, 2);
}

static int smps_set_voltage(struct regulator_dev *dev, int min_uV, int max_uV,
			    unsigned *selector)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	int rc = 0;

	if (smps_is_enabled(dev))
		rc = _smps_set_voltage(dev, min_uV);
	if (rc)
		return rc;

	/* only save if nonzero (or not disabling) */
	if (min_uV && (!vreg->pdata->sleep_selectable || !smps_is_enabled(dev)))
		vreg->save_uV = min_uV;

	return rc;
}

static int smps_get_voltage(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	return vreg->save_uV;
}

static int smps_enable(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	int rc = 0;
	unsigned mask, val;

	/* enable by setting voltage */
	if (MICRO_TO_MILLI(vreg->save_uV) > 0) {
		/* reenable pin control if it is in use */
		if (smps_get_mode(dev) == REGULATOR_MODE_IDLE) {
			mask = SMPS_PIN_CTRL | SMPS_PIN_FN;
			val = vreg->pdata->pin_ctrl << SMPS_PIN_CTRL_SHIFT
				| vreg->pdata->pin_fn << SMPS_PIN_FN_SHIFT;
			vreg_store(vreg, mask, val, 0, 0);
		}

		rc = _smps_set_voltage(dev, vreg->save_uV);
	}
	return rc;
}

static int smps_disable(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	unsigned mask, val;

	/* turn off pin control */
	mask = SMPS_PIN_CTRL | SMPS_PIN_FN;
	val = RPM_VREG_PIN_CTRL_NONE << SMPS_PIN_CTRL_SHIFT
		| RPM_VREG_PIN_FN_NONE << SMPS_PIN_FN_SHIFT;
	vreg_store(vreg, mask, val, 0, 0);

	/* disable by setting voltage to zero */
	return _smps_set_voltage(dev, 0);
}

/*
 * Optimum mode programming:
 * REGULATOR_MODE_FAST: Go to HPM (highest priority)
 * REGULATOR_MODE_STANDBY: Go to pin ctrl mode if there are any pin ctrl
 * votes, else go to LPM
 *
 * Pin ctrl mode voting via regulator set_mode:
 * REGULATOR_MODE_IDLE: Go to pin ctrl mode if the optimum mode is LPM, else
 * go to HPM
 * REGULATOR_MODE_NORMAL: Go to LPM if it is the optimum mode, else go to HPM
 *
 * Pin ctrl mode takes priority on the RPM when force mode is not set;
 * therefore, pin ctrl bits must be cleared if LPM or HPM is being voted for.
 */
static int smps_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	unsigned optimum = vreg->optimum;
	unsigned pc_vote = vreg->pc_vote;
	unsigned mode_initialized = vreg->mode_initialized;
	unsigned mask0 = 0, val0 = 0, mask1 = 0, val1 = 0;
	int set_hpm = -1, set_pin_control = -1;
	int peak_uA;
	int rc = 0;

	peak_uA = MILLI_TO_MICRO((vreg->req[0].value & SMPS_PEAK_CURRENT) >>
		  SMPS_PEAK_CURRENT_SHIFT);

	switch (mode) {
	case REGULATOR_MODE_FAST:
		set_hpm = 1;
		set_pin_control = 0;
		optimum = REGULATOR_MODE_FAST;
		mode_initialized = 1;
		break;

	case REGULATOR_MODE_STANDBY:
		set_hpm = 0;
		if (pc_vote)
			set_pin_control = 1;
		else
			set_pin_control = 0;
		optimum = REGULATOR_MODE_STANDBY;
		mode_initialized = 1;
		break;

	case REGULATOR_MODE_IDLE:
		if (pc_vote++)
			goto done; /* already taken care of */

		if (mode_initialized && optimum == REGULATOR_MODE_FAST) {
			set_hpm = 1;
			set_pin_control = 0;
		} else {
			set_pin_control = 1;
		}
		break;

	case REGULATOR_MODE_NORMAL:
		if (pc_vote && --pc_vote)
			goto done; /* already taken care of */

		if (optimum == REGULATOR_MODE_STANDBY)
			set_hpm = 0;
		else
			set_hpm = 1;
		set_pin_control = 0;
		break;

	default:
		return -EINVAL;
	}

	if (set_hpm == 1) {
		/* Make sure that request currents are at HPM level. */
		if (peak_uA < vreg_hpm_min_uA(vreg)) {
			mask0 = SMPS_PEAK_CURRENT;
			mask1 = SMPS_AVG_CURRENT;
			val0 = (MICRO_TO_MILLI(vreg_hpm_min_uA(vreg)) <<
			   SMPS_PEAK_CURRENT_SHIFT) & SMPS_PEAK_CURRENT;
			val1 = (MICRO_TO_MILLI(vreg_hpm_min_uA(vreg)) <<
			     SMPS_AVG_CURRENT_SHIFT) & SMPS_AVG_CURRENT;
		}
	} else if (set_hpm == 0) {
		/* Make sure that request currents are at LPM level. */
		if (peak_uA > vreg_lpm_max_uA(vreg)) {
			mask0 = SMPS_PEAK_CURRENT;
			mask1 = SMPS_AVG_CURRENT;
			val0 = (MICRO_TO_MILLI(vreg_lpm_max_uA(vreg)) <<
			   SMPS_PEAK_CURRENT_SHIFT) & SMPS_PEAK_CURRENT;
			val1 = (MICRO_TO_MILLI(vreg_lpm_max_uA(vreg)) <<
			     SMPS_AVG_CURRENT_SHIFT) & SMPS_AVG_CURRENT;
		}
	}

	if (set_pin_control == 1) {
		/* Enable pin control and pin function. */
		mask0 |= SMPS_PIN_CTRL | SMPS_PIN_FN;
		val0 |= vreg->pdata->pin_ctrl << SMPS_PIN_CTRL_SHIFT
			| vreg->pdata->pin_fn << SMPS_PIN_FN_SHIFT;
	} else if (set_pin_control == 0) {
		/* Clear pin control and pin function*/
		mask0 |= SMPS_PIN_CTRL | SMPS_PIN_FN;
		val0 |= RPM_VREG_PIN_CTRL_NONE << SMPS_PIN_CTRL_SHIFT
			| RPM_VREG_PIN_FN_NONE << SMPS_PIN_FN_SHIFT;
	}

	if (smps_is_enabled(dev)) {
		rc = vreg_set(vreg, mask0, val0, mask1, val1, 2);
	} else {
		/* Regulator is disabled; store but don't send new request. */
		rc = vreg_store(vreg, mask0, val0, mask1, val1);
	}
	if (rc)
		return rc;

done:
	vreg->mode_initialized = mode_initialized;
	vreg->optimum = optimum;
	vreg->pc_vote = pc_vote;

	return 0;
}

static unsigned int smps_get_mode(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);

	if ((vreg->optimum == REGULATOR_MODE_FAST) && vreg->mode_initialized)
		return REGULATOR_MODE_FAST;
	else if (vreg->pc_vote)
		return REGULATOR_MODE_IDLE;
	else if (vreg->optimum == REGULATOR_MODE_STANDBY)
		return REGULATOR_MODE_STANDBY;
	return REGULATOR_MODE_FAST;
}

unsigned int smps_get_optimum_mode(struct regulator_dev *dev, int input_uV,
		int output_uV, int load_uA)
{
	struct vreg *vreg = rdev_get_drvdata(dev);

	if (MICRO_TO_MILLI(load_uA) > 0) {
		vreg->req[0].value &= ~SMPS_PEAK_CURRENT;
		vreg->req[0].value |= (MICRO_TO_MILLI(saturate_load(load_uA)) <<
				   SMPS_PEAK_CURRENT_SHIFT) & SMPS_PEAK_CURRENT;
		vreg->req[1].value &= ~SMPS_AVG_CURRENT;
		vreg->req[1].value |= (MICRO_TO_MILLI(saturate_load(load_uA)) <<
				     SMPS_AVG_CURRENT_SHIFT) & SMPS_AVG_CURRENT;
	} else {
		/*
		 * smps_get_optimum_mode is being called before consumers have
		 * specified their load currents via regulator_set_optimum_mode.
		 * Return whatever the existing mode is.
		 */
		return smps_get_mode(dev);
	}

	if (load_uA >= vreg->hpm_min_load)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_STANDBY;
}

static int ldo_is_enabled(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	return ((vreg->req[0].value & LDO_VOLTAGE) >> LDO_VOLTAGE_SHIFT) != 0;
}

static int _ldo_set_voltage(struct regulator_dev *dev, int min_uV)
{
	struct vreg *vreg = rdev_get_drvdata(dev);

	return vreg_set(vreg, LDO_VOLTAGE,
			MICRO_TO_MILLI(min_uV) << LDO_VOLTAGE_SHIFT,
			0, 0, 2);
}

static int ldo_set_voltage(struct regulator_dev *dev, int min_uV, int max_uV,
			   unsigned *selector)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	int rc = 0;

	if (ldo_is_enabled(dev))
		rc = _ldo_set_voltage(dev, min_uV);
	if (rc)
		return rc;

	/* only save if nonzero (or not disabling) */
	if (min_uV && (!vreg->pdata->sleep_selectable || !ldo_is_enabled(dev)))
		vreg->save_uV = min_uV;

	return rc;
}

static int ldo_get_voltage(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	return vreg->save_uV;
}

static int ldo_enable(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	int rc = 0;
	unsigned mask, val;

	/* enable by setting voltage */
	if (MICRO_TO_MILLI(vreg->save_uV) > 0) {
		/* reenable pin control if it is in use */
		if (ldo_get_mode(dev) == REGULATOR_MODE_IDLE) {
			mask = LDO_PIN_CTRL | LDO_PIN_FN;
			val = vreg->pdata->pin_ctrl << LDO_PIN_CTRL_SHIFT
				| vreg->pdata->pin_fn << LDO_PIN_FN_SHIFT;
			vreg_store(vreg, mask, val, 0, 0);
		}

		rc = _ldo_set_voltage(dev, vreg->save_uV);
	}
	return rc;
}

static int ldo_disable(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	unsigned mask, val;

	/* turn off pin control */
	mask = LDO_PIN_CTRL | LDO_PIN_FN;
	val = RPM_VREG_PIN_CTRL_NONE << LDO_PIN_CTRL_SHIFT
		| RPM_VREG_PIN_FN_NONE << LDO_PIN_FN_SHIFT;
	vreg_store(vreg, mask, val, 0, 0);

	/* disable by setting voltage to zero */
	return _ldo_set_voltage(dev, 0);
}

/*
 * Optimum mode programming:
 * REGULATOR_MODE_FAST: Go to HPM (highest priority)
 * REGULATOR_MODE_STANDBY: Go to pin ctrl mode if there are any pin ctrl
 * votes, else go to LPM
 *
 * Pin ctrl mode voting via regulator set_mode:
 * REGULATOR_MODE_IDLE: Go to pin ctrl mode if the optimum mode is LPM, else
 * go to HPM
 * REGULATOR_MODE_NORMAL: Go to LPM if it is the optimum mode, else go to HPM
 *
 * Pin ctrl mode takes priority on the RPM when force mode is not set;
 * therefore, pin ctrl bits must be cleared if LPM or HPM is being voted for.
 */
static int ldo_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	unsigned optimum = vreg->optimum;
	unsigned pc_vote = vreg->pc_vote;
	unsigned mode_initialized = vreg->mode_initialized;
	unsigned mask0 = 0, val0 = 0, mask1 = 0, val1 = 0;
	int set_hpm = -1, set_pin_control = -1;
	int peak_uA;
	int rc = 0;

	peak_uA = MILLI_TO_MICRO((vreg->req[0].value & LDO_PEAK_CURRENT) >>
		  LDO_PEAK_CURRENT_SHIFT);

	switch (mode) {
	case REGULATOR_MODE_FAST:
		set_hpm = 1;
		set_pin_control = 0;
		optimum = REGULATOR_MODE_FAST;
		mode_initialized = 1;
		break;

	case REGULATOR_MODE_STANDBY:
		set_hpm = 0;
		if (pc_vote)
			set_pin_control = 1;
		else
			set_pin_control = 0;
		optimum = REGULATOR_MODE_STANDBY;
		mode_initialized = 1;
		break;

	case REGULATOR_MODE_IDLE:
		if (pc_vote++)
			goto done; /* already taken care of */

		if (mode_initialized && optimum == REGULATOR_MODE_FAST) {
			set_hpm = 1;
			set_pin_control = 0;
		} else {
			set_pin_control = 1;
		}
		break;

	case REGULATOR_MODE_NORMAL:
		if (pc_vote && --pc_vote)
			goto done; /* already taken care of */

		if (optimum == REGULATOR_MODE_STANDBY)
			set_hpm = 0;
		else
			set_hpm = 1;
		set_pin_control = 0;
		break;

	default:
		return -EINVAL;
	}

	if (set_hpm == 1) {
		/* Make sure that request currents are at HPM level. */
		if (peak_uA < vreg_hpm_min_uA(vreg)) {
			mask0 = LDO_PEAK_CURRENT;
			mask1 = LDO_AVG_CURRENT;
			val0 = (MICRO_TO_MILLI(vreg_hpm_min_uA(vreg)) <<
				LDO_PEAK_CURRENT_SHIFT) & LDO_PEAK_CURRENT;
			val1 = (MICRO_TO_MILLI(vreg_hpm_min_uA(vreg)) <<
				LDO_AVG_CURRENT_SHIFT) & LDO_AVG_CURRENT;
		}
	} else if (set_hpm == 0) {
		/* Make sure that request currents are at LPM level. */
		if (peak_uA > vreg_lpm_max_uA(vreg)) {
			mask0 = LDO_PEAK_CURRENT;
			mask1 = LDO_AVG_CURRENT;
			val0 = (MICRO_TO_MILLI(vreg_lpm_max_uA(vreg)) <<
				LDO_PEAK_CURRENT_SHIFT) & LDO_PEAK_CURRENT;
			val1 = (MICRO_TO_MILLI(vreg_lpm_max_uA(vreg)) <<
				LDO_AVG_CURRENT_SHIFT) & LDO_AVG_CURRENT;
		}
	}

	if (set_pin_control == 1) {
		/* Enable pin control and pin function. */
		mask0 |= LDO_PIN_CTRL | LDO_PIN_FN;
		val0 |= vreg->pdata->pin_ctrl << LDO_PIN_CTRL_SHIFT
			| vreg->pdata->pin_fn << LDO_PIN_FN_SHIFT;
	} else if (set_pin_control == 0) {
		/* Clear pin control and pin function*/
		mask0 |= LDO_PIN_CTRL | LDO_PIN_FN;
		val0 |= RPM_VREG_PIN_CTRL_NONE << LDO_PIN_CTRL_SHIFT
			| RPM_VREG_PIN_FN_NONE << LDO_PIN_FN_SHIFT;
	}

	if (ldo_is_enabled(dev)) {
		rc = vreg_set(vreg, mask0, val0, mask1, val1, 2);
	} else {
		/* Regulator is disabled; store but don't send new request. */
		rc = vreg_store(vreg, mask0, val0, mask1, val1);
	}
	if (rc)
		return rc;

done:
	vreg->mode_initialized = mode_initialized;
	vreg->optimum = optimum;
	vreg->pc_vote = pc_vote;

	return 0;
}

static unsigned int ldo_get_mode(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);

	if ((vreg->optimum == REGULATOR_MODE_FAST) && vreg->mode_initialized)
		return REGULATOR_MODE_FAST;
	else if (vreg->pc_vote)
		return REGULATOR_MODE_IDLE;
	else if (vreg->optimum == REGULATOR_MODE_STANDBY)
		return REGULATOR_MODE_STANDBY;
	return REGULATOR_MODE_FAST;
}

unsigned int ldo_get_optimum_mode(struct regulator_dev *dev, int input_uV,
		int output_uV, int load_uA)
{
	struct vreg *vreg = rdev_get_drvdata(dev);

	if (MICRO_TO_MILLI(load_uA) > 0) {
		vreg->req[0].value &= ~LDO_PEAK_CURRENT;
		vreg->req[0].value |= (MICRO_TO_MILLI(saturate_load(load_uA)) <<
				     LDO_PEAK_CURRENT_SHIFT) & LDO_PEAK_CURRENT;
		vreg->req[1].value &= ~LDO_AVG_CURRENT;
		vreg->req[1].value |= (MICRO_TO_MILLI(saturate_load(load_uA)) <<
				       LDO_AVG_CURRENT_SHIFT) & LDO_AVG_CURRENT;
	} else {
		/*
		 * ldo_get_optimum_mode is being called before consumers have
		 * specified their load currents via regulator_set_optimum_mode.
		 * Return whatever the existing mode is.
		 */
		return ldo_get_mode(dev);
	}

	if (load_uA >= vreg->hpm_min_load)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_STANDBY;
}

static int switch_enable(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	unsigned mask = 0, val = 0;

	/* reenable pin control if it is in use */
	if (switch_get_mode(dev) == REGULATOR_MODE_IDLE) {
		mask = SWITCH_PIN_CTRL | SWITCH_PIN_FN;
		val = vreg->pdata->pin_ctrl << SWITCH_PIN_CTRL_SHIFT
			| vreg->pdata->pin_fn << SWITCH_PIN_FN_SHIFT;
	}

	return vreg_set(rdev_get_drvdata(dev), SWITCH_STATE | mask,
		(RPM_VREG_STATE_ON << SWITCH_STATE_SHIFT) | val, 0, 0, 1);
}

static int switch_disable(struct regulator_dev *dev)
{
	unsigned mask, val;

	/* turn off pin control */
	mask = SWITCH_PIN_CTRL | SWITCH_PIN_FN;
	val = RPM_VREG_PIN_CTRL_NONE << SWITCH_PIN_CTRL_SHIFT
		| RPM_VREG_PIN_FN_NONE << SWITCH_PIN_FN_SHIFT;

	return vreg_set(rdev_get_drvdata(dev), SWITCH_STATE | mask,
		(RPM_VREG_STATE_OFF << SWITCH_STATE_SHIFT) | val, 0, 0, 1);
}

static int switch_is_enabled(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	enum rpm_vreg_state state;

	state = (vreg->req[0].value & SWITCH_STATE) >> SWITCH_STATE_SHIFT;

	return state == RPM_VREG_STATE_ON;
}

/*
 * Pin ctrl mode voting via regulator set_mode:
 * REGULATOR_MODE_IDLE: Go to pin ctrl mode if the optimum mode is LPM, else
 * go to HPM
 * REGULATOR_MODE_NORMAL: Go to LPM if it is the optimum mode, else go to HPM
 */
static int switch_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	unsigned pc_vote = vreg->pc_vote;
	unsigned mask, val;
	int rc;

	switch (mode) {
	case REGULATOR_MODE_IDLE:
		if (pc_vote++)
			goto done; /* already taken care of */

		mask = SWITCH_PIN_CTRL | SWITCH_PIN_FN;
		val = vreg->pdata->pin_ctrl << SWITCH_PIN_CTRL_SHIFT
			| vreg->pdata->pin_fn << SWITCH_PIN_FN_SHIFT;
		break;

	case REGULATOR_MODE_NORMAL:
		if (--pc_vote)
			goto done; /* already taken care of */

		mask = SWITCH_PIN_CTRL | SWITCH_PIN_FN;
		val = RPM_VREG_PIN_CTRL_NONE << SWITCH_PIN_CTRL_SHIFT
			| RPM_VREG_PIN_FN_NONE << SWITCH_PIN_FN_SHIFT;
		break;

	default:
		return -EINVAL;
	}

	if (switch_is_enabled(dev)) {
		rc = vreg_set(vreg, mask, val, 0, 0, 2);
	} else {
		/* Regulator is disabled; store but don't send new request. */
		rc = vreg_store(vreg, mask, val, 0, 0);
	}
	if (rc)
		return rc;

done:
	vreg->pc_vote = pc_vote;
	return 0;
}

static unsigned int switch_get_mode(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);

	if (vreg->pc_vote)
		return REGULATOR_MODE_IDLE;
	return REGULATOR_MODE_NORMAL;
}

static int ncp_enable(struct regulator_dev *dev)
{
	return vreg_set(rdev_get_drvdata(dev), NCP_STATE,
			RPM_VREG_STATE_ON << NCP_STATE_SHIFT, 0, 0, 2);
}

static int ncp_disable(struct regulator_dev *dev)
{
	return vreg_set(rdev_get_drvdata(dev), NCP_STATE,
			RPM_VREG_STATE_OFF << NCP_STATE_SHIFT, 0, 0, 2);
}

static int ncp_is_enabled(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);
	enum rpm_vreg_state state;

	state = (vreg->req[0].value & NCP_STATE) >> NCP_STATE_SHIFT;

	return state == RPM_VREG_STATE_ON;
}

static int ncp_set_voltage(struct regulator_dev *dev, int min_uV, int max_uV,
			   unsigned *selector)
{
	return vreg_set(rdev_get_drvdata(dev), NCP_VOLTAGE,
			MICRO_TO_MILLI(min_uV) << NCP_VOLTAGE_SHIFT, 0, 0, 2);
}

static int ncp_get_voltage(struct regulator_dev *dev)
{
	struct vreg *vreg = rdev_get_drvdata(dev);

	return MILLI_TO_MICRO((vreg->req[0].value & NCP_VOLTAGE) >>
			NCP_VOLTAGE_SHIFT);
}

static struct regulator_ops ldo_ops = {
	.enable = ldo_enable,
	.disable = ldo_disable,
	.is_enabled = ldo_is_enabled,
	.set_voltage = ldo_set_voltage,
	.get_voltage = ldo_get_voltage,
	.set_mode = ldo_set_mode,
	.get_optimum_mode = ldo_get_optimum_mode,
	.get_mode = ldo_get_mode,
};

static struct regulator_ops smps_ops = {
	.enable = smps_enable,
	.disable = smps_disable,
	.is_enabled = smps_is_enabled,
	.set_voltage = smps_set_voltage,
	.get_voltage = smps_get_voltage,
	.set_mode = smps_set_mode,
	.get_optimum_mode = smps_get_optimum_mode,
	.get_mode = smps_get_mode,
};

static struct regulator_ops switch_ops = {
	.enable = switch_enable,
	.disable = switch_disable,
	.is_enabled = switch_is_enabled,
	.set_mode = switch_set_mode,
	.get_mode = switch_get_mode,
};

static struct regulator_ops ncp_ops = {
	.enable = ncp_enable,
	.disable = ncp_disable,
	.is_enabled = ncp_is_enabled,
	.set_voltage = ncp_set_voltage,
	.get_voltage = ncp_get_voltage,
};

#define DESC(_id, _name, _ops) \
	[_id] = { \
		.id = _id, \
		.name = _name, \
		.ops = _ops, \
		.type = REGULATOR_VOLTAGE, \
		.owner = THIS_MODULE, \
	}

static struct regulator_desc vreg_descrip[RPM_VREG_ID_MAX] = {
	DESC(RPM_VREG_ID_PM8058_L0, "8058_l0", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L1, "8058_l1", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L2, "8058_l2", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L3, "8058_l3", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L4, "8058_l4", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L5, "8058_l5", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L6, "8058_l6", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L7, "8058_l7", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L8, "8058_l8", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L9, "8058_l9", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L10, "8058_l10", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L11, "8058_l11", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L12, "8058_l12", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L13, "8058_l13", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L14, "8058_l14", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L15, "8058_l15", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L16, "8058_l16", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L17, "8058_l17", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L18, "8058_l18", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L19, "8058_l19", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L20, "8058_l20", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L21, "8058_l21", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L22, "8058_l22", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L23, "8058_l23", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L24, "8058_l24", &ldo_ops),
	DESC(RPM_VREG_ID_PM8058_L25, "8058_l25", &ldo_ops),

	DESC(RPM_VREG_ID_PM8058_S0, "8058_s0", &smps_ops),
	DESC(RPM_VREG_ID_PM8058_S1, "8058_s1", &smps_ops),
	DESC(RPM_VREG_ID_PM8058_S2, "8058_s2", &smps_ops),
	DESC(RPM_VREG_ID_PM8058_S3, "8058_s3", &smps_ops),
	DESC(RPM_VREG_ID_PM8058_S4, "8058_s4", &smps_ops),

	DESC(RPM_VREG_ID_PM8058_LVS0, "8058_lvs0", &switch_ops),
	DESC(RPM_VREG_ID_PM8058_LVS1, "8058_lvs1", &switch_ops),

	DESC(RPM_VREG_ID_PM8058_NCP, "8058_ncp", &ncp_ops),

	DESC(RPM_VREG_ID_PM8901_L0, "8901_l0", &ldo_ops),
	DESC(RPM_VREG_ID_PM8901_L1, "8901_l1", &ldo_ops),
	DESC(RPM_VREG_ID_PM8901_L2, "8901_l2", &ldo_ops),
	DESC(RPM_VREG_ID_PM8901_L3, "8901_l3", &ldo_ops),
	DESC(RPM_VREG_ID_PM8901_L4, "8901_l4", &ldo_ops),
	DESC(RPM_VREG_ID_PM8901_L5, "8901_l5", &ldo_ops),
	DESC(RPM_VREG_ID_PM8901_L6, "8901_l6", &ldo_ops),

	DESC(RPM_VREG_ID_PM8901_S0, "8901_s0", &smps_ops),
	DESC(RPM_VREG_ID_PM8901_S1, "8901_s1", &smps_ops),
	DESC(RPM_VREG_ID_PM8901_S2, "8901_s2", &smps_ops),
	DESC(RPM_VREG_ID_PM8901_S3, "8901_s3", &smps_ops),
	DESC(RPM_VREG_ID_PM8901_S4, "8901_s4", &smps_ops),

	DESC(RPM_VREG_ID_PM8901_LVS0, "8901_lvs0", &switch_ops),
	DESC(RPM_VREG_ID_PM8901_LVS1, "8901_lvs1", &switch_ops),
	DESC(RPM_VREG_ID_PM8901_LVS2, "8901_lvs2", &switch_ops),
	DESC(RPM_VREG_ID_PM8901_LVS3, "8901_lvs3", &switch_ops),

	DESC(RPM_VREG_ID_PM8901_MVS0, "8901_mvs0", &switch_ops),
};

static void ldo_init(struct vreg *vreg)
{
	enum rpm_vreg_pin_fn pf = RPM_VREG_PIN_FN_NONE;

	/* Allow pf=sleep_b to be specified by platform data. */
	if (vreg->pdata->pin_fn == RPM_VREG_PIN_FN_SLEEP_B)
		pf = RPM_VREG_PIN_FN_SLEEP_B;

	vreg->req[0].value =
		MICRO_TO_MILLI(saturate_load(vreg->pdata->peak_uA)) <<
			LDO_PEAK_CURRENT_SHIFT |
		vreg->pdata->mode << LDO_MODE_SHIFT | pf << LDO_PIN_FN_SHIFT |
		RPM_VREG_PIN_CTRL_NONE << LDO_PIN_CTRL_SHIFT;

	vreg->req[1].value =
		vreg->pdata->pull_down_enable << LDO_PULL_DOWN_ENABLE_SHIFT |
		MICRO_TO_MILLI(saturate_load(vreg->pdata->avg_uA)) <<
			LDO_AVG_CURRENT_SHIFT;
}

static void smps_init(struct vreg *vreg)
{
	enum rpm_vreg_pin_fn pf = RPM_VREG_PIN_FN_NONE;

	/* Allow pf=sleep_b to be specified by platform data. */
	if (vreg->pdata->pin_fn == RPM_VREG_PIN_FN_SLEEP_B)
		pf = RPM_VREG_PIN_FN_SLEEP_B;

	vreg->req[0].value =
		MICRO_TO_MILLI(saturate_load(vreg->pdata->peak_uA)) <<
			SMPS_PEAK_CURRENT_SHIFT |
		vreg->pdata->mode << SMPS_MODE_SHIFT | pf << SMPS_PIN_FN_SHIFT |
		RPM_VREG_PIN_CTRL_NONE << SMPS_PIN_CTRL_SHIFT;


	vreg->req[1].value =
		vreg->pdata->pull_down_enable << SMPS_PULL_DOWN_ENABLE_SHIFT |
		MICRO_TO_MILLI(saturate_load(vreg->pdata->avg_uA)) <<
			SMPS_AVG_CURRENT_SHIFT |
		vreg->pdata->freq << SMPS_FREQ_SHIFT |
		0 << SMPS_CLK_SRC_SHIFT;
}

static void ncp_init(struct vreg *vreg)
{
	vreg->req[0].value = vreg->pdata->state << NCP_STATE_SHIFT;
}

static void switch_init(struct vreg *vreg)
{
	enum rpm_vreg_pin_fn pf = RPM_VREG_PIN_FN_NONE;

	/* Allow pf=sleep_b to be specified by platform data. */
	if (vreg->pdata->pin_fn == RPM_VREG_PIN_FN_SLEEP_B)
		pf = RPM_VREG_PIN_FN_SLEEP_B;

	vreg->req[0].value =
		vreg->pdata->state << SWITCH_STATE_SHIFT |
		vreg->pdata->pull_down_enable <<
			SWITCH_PULL_DOWN_ENABLE_SHIFT |
		pf << SWITCH_PIN_FN_SHIFT |
		RPM_VREG_PIN_CTRL_NONE << SWITCH_PIN_CTRL_SHIFT;
}

static int vreg_init(enum rpm_vreg_id id, struct vreg *vreg)
{
	vreg->save_uV = vreg->pdata->default_uV;

	if (vreg->pdata->peak_uA >= vreg->hpm_min_load)
		vreg->optimum = REGULATOR_MODE_FAST;
	else
		vreg->optimum = REGULATOR_MODE_STANDBY;

	vreg->mode_initialized = 0;

	if (IS_LDO(id))
		ldo_init(vreg);
	else if (IS_SMPS(id))
		smps_init(vreg);
	else if (IS_NCP(id))
		ncp_init(vreg);
	else if (IS_SWITCH(id))
		switch_init(vreg);
	else
		return -EINVAL;

	return 0;
}

static int __devinit rpm_vreg_probe(struct platform_device *pdev)
{
	struct regulator_desc *rdesc;
	struct regulator_dev *rdev;
	struct vreg *vreg;
	int rc;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id < 0 || pdev->id >= RPM_VREG_ID_MAX)
		return -ENODEV;

	vreg = &vregs[pdev->id];
	vreg->pdata = pdev->dev.platform_data;
	vreg->id = pdev->id;
	rdesc = &vreg_descrip[pdev->id];

	rc = vreg_init(pdev->id, vreg);
	if (rc) {
		pr_err("%s: vreg_init failed, rc=%d\n", __func__, rc);
		return rc;
	}

	/* Disallow idle and normal modes if pin control isn't set. */
	if ((vreg->pdata->pin_ctrl == RPM_VREG_PIN_CTRL_NONE)
	    && ((vreg->pdata->pin_fn == RPM_VREG_PIN_FN_ENABLE)
		    || (vreg->pdata->pin_fn == RPM_VREG_PIN_FN_MODE)))
		vreg->pdata->init_data.constraints.valid_modes_mask
			&= ~(REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE);

	rdev = regulator_register(rdesc, &pdev->dev,
			&vreg->pdata->init_data, vreg);
	if (IS_ERR(rdev)) {
		rc = PTR_ERR(rdev);
		pr_err("%s: id=%d, rc=%d\n", __func__,
				pdev->id, rc);
		return rc;
	}

	platform_set_drvdata(pdev, rdev);

	return rc;
}

static int __devexit rpm_vreg_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	regulator_unregister(rdev);

	return 0;
}

static struct platform_driver rpm_vreg_driver = {
	.probe = rpm_vreg_probe,
	.remove = __devexit_p(rpm_vreg_remove),
	.driver = {
		.name = "rpm-regulator",
		.owner = THIS_MODULE,
	},
};

static int __init rpm_vreg_init(void)
{
	return platform_driver_register(&rpm_vreg_driver);
}

static void __exit rpm_vreg_exit(void)
{
	platform_driver_unregister(&rpm_vreg_driver);
}

postcore_initcall(rpm_vreg_init);
module_exit(rpm_vreg_exit);

#define VREG_ID_IS_8058_S0_OR_S1(id) \
	((id == RPM_VREG_ID_PM8058_S0) || (id == RPM_VREG_ID_PM8058_S1))

static void print_rpm_request(struct vreg *vreg, int set)
{
	int v, ip, fm, pc, pf, pd, ia, freq, clk, state;

	/* Suppress 8058_s0 and 8058_s1 printing. */
	if ((msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_IGNORE_8058_S0_S1)
	    && VREG_ID_IS_8058_S0_OR_S1(vreg->id))
		return;

	if (IS_LDO(vreg->id)) {
		v = (vreg->req[0].value & LDO_VOLTAGE) >> LDO_VOLTAGE_SHIFT;
		ip = (vreg->req[0].value & LDO_PEAK_CURRENT)
			>> LDO_PEAK_CURRENT_SHIFT;
		fm = (vreg->req[0].value & LDO_MODE) >> LDO_MODE_SHIFT;
		pc = (vreg->req[0].value & LDO_PIN_CTRL) >> LDO_PIN_CTRL_SHIFT;
		pf = (vreg->req[0].value & LDO_PIN_FN) >> LDO_PIN_FN_SHIFT;
		pd = (vreg->req[1].value & LDO_PULL_DOWN_ENABLE)
			>> LDO_PULL_DOWN_ENABLE_SHIFT;
		ia = (vreg->req[1].value & LDO_AVG_CURRENT)
				>> LDO_AVG_CURRENT_SHIFT;

		pr_info("rpm-regulator: %s %-9s: s=%c, v=%4d mV, ip=%4d "
			"mA, fm=%s (%d), pc=%s%s%s%s%s (%d), pf=%s (%d), pd=%s "
			"(%d), ia=%4d mA; req[0]={%d, 0x%08X}, "
			"req[1]={%d, 0x%08X}\n",
			(set == MSM_RPM_CTX_SET_0 ? "sending " : "buffered"),
			vreg_descrip[vreg->id].name,
			(set == MSM_RPM_CTX_SET_0 ? 'A' : 'S'), v, ip,
			(fm == RPM_VREG_MODE_NONE ? "none" :
				(fm == RPM_VREG_MODE_LPM ? "LPM" :
				       (fm == RPM_VREG_MODE_HPM ? "HPM" : ""))),
			fm,
			(pc & RPM_VREG_PIN_CTRL_A0 ? " A0" : ""),
			(pc & RPM_VREG_PIN_CTRL_A1 ? " A1" : ""),
			(pc & RPM_VREG_PIN_CTRL_D0 ? " D0" : ""),
			(pc & RPM_VREG_PIN_CTRL_D1 ? " D1" : ""),
			(pc == RPM_VREG_PIN_CTRL_NONE ? " none" : ""), pc,
			(pf == RPM_VREG_PIN_FN_NONE ?
				"none" :
				(pf == RPM_VREG_PIN_FN_ENABLE ?
					"on/off" :
					(pf == RPM_VREG_PIN_FN_MODE ?
						"HPM/LPM" :
						(pf == RPM_VREG_PIN_FN_SLEEP_B ?
							"sleep_b" : "")))),
			pf, (pd == 1 ? "Y" : "N"), pd, ia,
			vreg->req[0].id, vreg->req[0].value,
			vreg->req[1].id, vreg->req[1].value);

	} else if (IS_SMPS(vreg->id)) {
		v = (vreg->req[0].value & SMPS_VOLTAGE) >> SMPS_VOLTAGE_SHIFT;
		ip = (vreg->req[0].value & SMPS_PEAK_CURRENT)
			>> SMPS_PEAK_CURRENT_SHIFT;
		fm = (vreg->req[0].value & SMPS_MODE) >> SMPS_MODE_SHIFT;
		pc = (vreg->req[0].value & SMPS_PIN_CTRL)
			>> SMPS_PIN_CTRL_SHIFT;
		pf = (vreg->req[0].value & SMPS_PIN_FN) >> SMPS_PIN_FN_SHIFT;
		pd = (vreg->req[1].value & SMPS_PULL_DOWN_ENABLE)
			>> SMPS_PULL_DOWN_ENABLE_SHIFT;
		ia = (vreg->req[1].value & SMPS_AVG_CURRENT)
			>> SMPS_AVG_CURRENT_SHIFT;
		freq = (vreg->req[1].value & SMPS_FREQ) >> SMPS_FREQ_SHIFT;
		clk = (vreg->req[1].value & SMPS_CLK_SRC) >> SMPS_CLK_SRC_SHIFT;

		pr_info("rpm-regulator: %s %-9s: s=%c, v=%4d mV, ip=%4d "
			"mA, fm=%s (%d), pc=%s%s%s%s%s (%d), pf=%s (%d), pd=%s "
			"(%d), ia=%4d mA, freq=%2d, clk=%d; "
			"req[0]={%d, 0x%08X}, req[1]={%d, 0x%08X}\n",
			(set == MSM_RPM_CTX_SET_0 ? "sending " : "buffered"),
			vreg_descrip[vreg->id].name,
			(set == MSM_RPM_CTX_SET_0 ? 'A' : 'S'), v, ip,
			(fm == RPM_VREG_MODE_NONE ? "none" :
				(fm == RPM_VREG_MODE_LPM ? "LPM" :
				       (fm == RPM_VREG_MODE_HPM ? "HPM" : ""))),
			fm,
			(pc & RPM_VREG_PIN_CTRL_A0 ? " A0" : ""),
			(pc & RPM_VREG_PIN_CTRL_A1 ? " A1" : ""),
			(pc & RPM_VREG_PIN_CTRL_D0 ? " D0" : ""),
			(pc & RPM_VREG_PIN_CTRL_D1 ? " D1" : ""),
			(pc == RPM_VREG_PIN_CTRL_NONE ? " none" : ""), pc,
			(pf == RPM_VREG_PIN_FN_NONE ?
				"none" :
				(pf == RPM_VREG_PIN_FN_ENABLE ?
					"on/off" :
					(pf == RPM_VREG_PIN_FN_MODE ?
						"HPM/LPM" :
						(pf == RPM_VREG_PIN_FN_SLEEP_B ?
							"sleep_b" : "")))),
			pf, (pd == 1 ? "Y" : "N"), pd, ia, freq, clk,
			vreg->req[0].id, vreg->req[0].value,
			vreg->req[1].id, vreg->req[1].value);

	} else if (IS_SWITCH(vreg->id)) {
		state = (vreg->req[0].value & SWITCH_STATE)
			>> SWITCH_STATE_SHIFT;
		pd = (vreg->req[0].value & SWITCH_PULL_DOWN_ENABLE)
			>> SWITCH_PULL_DOWN_ENABLE_SHIFT;
		pc = (vreg->req[0].value & SWITCH_PIN_CTRL)
			>> SWITCH_PIN_CTRL_SHIFT;
		pf = (vreg->req[0].value & SWITCH_PIN_FN)
			>> SWITCH_PIN_FN_SHIFT;

		pr_info("rpm-regulator: %s %-9s: s=%c, state=%s (%d), "
			"pd=%s (%d), pc =%s%s%s%s%s (%d), pf=%s (%d); "
			"req[0]={%d, 0x%08X}\n",
			(set == MSM_RPM_CTX_SET_0 ? "sending " : "buffered"),
			vreg_descrip[vreg->id].name,
			(set == MSM_RPM_CTX_SET_0 ? 'A' : 'S'),
			(state == 1 ? "on" : "off"), state,
			(pd == 1 ? "Y" : "N"), pd,
			(pc & RPM_VREG_PIN_CTRL_A0 ? " A0" : ""),
			(pc & RPM_VREG_PIN_CTRL_A1 ? " A1" : ""),
			(pc & RPM_VREG_PIN_CTRL_D0 ? " D0" : ""),
			(pc & RPM_VREG_PIN_CTRL_D1 ? " D1" : ""),
			(pc == RPM_VREG_PIN_CTRL_NONE ? " none" : ""), pc,
			(pf == RPM_VREG_PIN_FN_NONE ?
				"none" :
				(pf == RPM_VREG_PIN_FN_ENABLE ?
					"on/off" :
					(pf == RPM_VREG_PIN_FN_MODE ?
						"HPM/LPM" :
						(pf == RPM_VREG_PIN_FN_SLEEP_B ?
							"sleep_b" : "")))),
			pf, vreg->req[0].id, vreg->req[0].value);

	} else if (IS_NCP(vreg->id)) {
		v = (vreg->req[0].value & NCP_VOLTAGE) >> NCP_VOLTAGE_SHIFT;
		state = (vreg->req[0].value & NCP_STATE) >> NCP_STATE_SHIFT;

		pr_info("rpm-regulator: %s %-9s: s=%c, v=-%4d mV, "
			"state=%s (%d); req[0]={%d, 0x%08X}\n",
			(set == MSM_RPM_CTX_SET_0 ? "sending " : "buffered"),
			vreg_descrip[vreg->id].name,
			(set == MSM_RPM_CTX_SET_0 ? 'A' : 'S'),
			v, (state == 1 ? "on" : "off"), state,
			vreg->req[0].id, vreg->req[0].value);
	}
}

static void print_rpm_vote(struct vreg *vreg, enum rpm_vreg_voter voter,
			int set, int voter_mV, int aggregate_mV)
{
	/* Suppress 8058_s0 and 8058_s1 printing. */
	if ((msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_IGNORE_8058_S0_S1)
	    && VREG_ID_IS_8058_S0_OR_S1(vreg->id))
		return;

	pr_info("rpm-regulator: vote received %-9s: voter=%d, set=%c, "
		"v_voter=%4d mV, v_aggregate=%4d mV\n",
		vreg_descrip[vreg->id].name, voter, (set == 0 ? 'A' : 'S'),
		voter_mV, aggregate_mV);
}

static void print_rpm_duplicate(struct vreg *vreg, int set, int cnt)
{
	/* Suppress 8058_s0 and 8058_s1 printing. */
	if ((msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_IGNORE_8058_S0_S1)
	    && VREG_ID_IS_8058_S0_OR_S1(vreg->id))
		return;

	if (cnt == 2)
		pr_info("rpm-regulator: ignored duplicate request %-9s: set=%c;"
			" req[0]={%d, 0x%08X}, req[1]={%d, 0x%08X}\n",
			vreg_descrip[vreg->id].name, (set == 0 ? 'A' : 'S'),
			vreg->req[0].id, vreg->req[0].value,
			vreg->req[1].id, vreg->req[1].value);
	else if (cnt == 1)
		pr_info("rpm-regulator: ignored duplicate request %-9s: set=%c;"
			" req[0]={%d, 0x%08X}\n",
			vreg_descrip[vreg->id].name, (set == 0 ? 'A' : 'S'),
			vreg->req[0].id, vreg->req[0].value);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("rpm regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:rpm-regulator");
