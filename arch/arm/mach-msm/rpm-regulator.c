/*
 * Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <mach/rpm.h>
#include <mach/rpm-regulator.h>
#include <mach/socinfo.h>

#include "rpm_resources.h"
#include "rpm-regulator-private.h"

/* Debug Definitions */

enum {
	MSM_RPM_VREG_DEBUG_REQUEST = BIT(0),
	MSM_RPM_VREG_DEBUG_VOTE = BIT(1),
	MSM_RPM_VREG_DEBUG_DUPLICATE = BIT(2),
	MSM_RPM_VREG_DEBUG_IGNORE_VDD_MEM_DIG = BIT(3),
};

static int msm_rpm_vreg_debug_mask;
module_param_named(
	debug_mask, msm_rpm_vreg_debug_mask, int, S_IRUSR | S_IWUSR
);

struct vreg_config *(*get_config[])(void) = {
	[RPM_VREG_VERSION_8660] = get_config_8660,
	[RPM_VREG_VERSION_8960] = get_config_8960,
	[RPM_VREG_VERSION_9615] = get_config_9615,
};

#define SET_PART(_vreg, _part, _val) \
	_vreg->req[_vreg->part->_part.word].value \
		= (_vreg->req[_vreg->part->_part.word].value \
			& ~_vreg->part->_part.mask) \
		  | (((_val) << _vreg->part->_part.shift) \
			& _vreg->part->_part.mask)

#define GET_PART(_vreg, _part) \
	((_vreg->req[_vreg->part->_part.word].value & _vreg->part->_part.mask) \
		>> _vreg->part->_part.shift)

#define USES_PART(_vreg, _part) (_vreg->part->_part.mask)

#define vreg_err(vreg, fmt, ...) \
	pr_err("%s: " fmt, vreg->rdesc.name, ##__VA_ARGS__)

#define RPM_VREG_PIN_CTRL_EN0		0x01
#define RPM_VREG_PIN_CTRL_EN1		0x02
#define RPM_VREG_PIN_CTRL_EN2		0x04
#define RPM_VREG_PIN_CTRL_EN3		0x08
#define RPM_VREG_PIN_CTRL_ALL		0x0F

static const char *label_freq[] = {
	[RPM_VREG_FREQ_NONE]		= " N/A",
	[RPM_VREG_FREQ_19p20]		= "19.2",
	[RPM_VREG_FREQ_9p60]		= "9.60",
	[RPM_VREG_FREQ_6p40]		= "6.40",
	[RPM_VREG_FREQ_4p80]		= "4.80",
	[RPM_VREG_FREQ_3p84]		= "3.84",
	[RPM_VREG_FREQ_3p20]		= "3.20",
	[RPM_VREG_FREQ_2p74]		= "2.74",
	[RPM_VREG_FREQ_2p40]		= "2.40",
	[RPM_VREG_FREQ_2p13]		= "2.13",
	[RPM_VREG_FREQ_1p92]		= "1.92",
	[RPM_VREG_FREQ_1p75]		= "1.75",
	[RPM_VREG_FREQ_1p60]		= "1.60",
	[RPM_VREG_FREQ_1p48]		= "1.48",
	[RPM_VREG_FREQ_1p37]		= "1.37",
	[RPM_VREG_FREQ_1p28]		= "1.28",
	[RPM_VREG_FREQ_1p20]		= "1.20",
};
/*
 * This is used when voting for LPM or HPM by subtracting or adding to the
 * hpm_min_load of a regulator.  It has units of uA.
 */
#define LOAD_THRESHOLD_STEP		1000

/* rpm_version keeps track of the version for the currently running driver. */
enum rpm_vreg_version rpm_version = -1;

/* config holds all configuration data of the currently running driver. */
static struct vreg_config *config;

/* These regulator ID values are specified in the board file. */
static int vreg_id_vdd_mem, vreg_id_vdd_dig;

static inline int vreg_id_is_vdd_mem_or_dig(int id)
{
	return id == vreg_id_vdd_mem || id == vreg_id_vdd_dig;
}

#define DEBUG_PRINT_BUFFER_SIZE 512

static void rpm_regulator_req(struct vreg *vreg, int set)
{
	int uV, mV, fm, pm, pc, pf, pd, freq, state, i;
	const char *pf_label = "", *fm_label = "", *pc_total = "";
	const char *pc_en[4] = {"", "", "", ""};
	const char *pm_label = "", *freq_label = "";
	char buf[DEBUG_PRINT_BUFFER_SIZE];
	size_t buflen = DEBUG_PRINT_BUFFER_SIZE;
	int pos = 0;

	/* Suppress VDD_MEM and VDD_DIG printing. */
	if ((msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_IGNORE_VDD_MEM_DIG)
	    && vreg_id_is_vdd_mem_or_dig(vreg->id))
		return;

	uV = GET_PART(vreg, uV);
	mV = GET_PART(vreg, mV);
	if (vreg->type == RPM_REGULATOR_TYPE_NCP) {
		uV = -uV;
		mV = -mV;
	}

	fm = GET_PART(vreg, fm);
	pm = GET_PART(vreg, pm);
	pc = GET_PART(vreg, pc);
	pf = GET_PART(vreg, pf);
	pd = GET_PART(vreg, pd);
	freq = GET_PART(vreg, freq);
	state = GET_PART(vreg, enable_state);

	if (pf >= 0 && pf < config->label_pin_func_len)
		pf_label = config->label_pin_func[pf];

	if (fm >= 0 && fm < config->label_force_mode_len)
		fm_label = config->label_force_mode[fm];

	if (pm >= 0 && pm < config->label_power_mode_len)
		pm_label = config->label_power_mode[pm];

	if (freq >= 0 && freq < ARRAY_SIZE(label_freq))
		freq_label = label_freq[freq];

	for (i = 0; i < config->label_pin_ctrl_len; i++)
		if (pc & (1 << i))
			pc_en[i] = config->label_pin_ctrl[i];

	if (pc == RPM_VREG_PIN_CTRL_NONE)
		pc_total = " none";

	pos += scnprintf(buf + pos, buflen - pos, "%s%s: ",
			 KERN_INFO, __func__);

	pos += scnprintf(buf + pos, buflen - pos, "%s %-9s: s=%c",
			(set == MSM_RPM_CTX_SET_0 ? "sending " : "buffered"),
			vreg->rdesc.name,
			(set == MSM_RPM_CTX_SET_0 ? 'A' : 'S'));

	if (USES_PART(vreg, uV))
		pos += scnprintf(buf + pos, buflen - pos, ", v=%7d uV", uV);
	if (USES_PART(vreg, mV))
		pos += scnprintf(buf + pos, buflen - pos, ", v=%4d mV", mV);
	if (USES_PART(vreg, enable_state))
		pos += scnprintf(buf + pos, buflen - pos, ", state=%s (%d)",
				 (state == 1 ? "on" : "off"), state);
	if (USES_PART(vreg, ip))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", ip=%4d mA", GET_PART(vreg, ip));
	if (USES_PART(vreg, fm))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", fm=%s (%d)", fm_label, fm);
	if (USES_PART(vreg, pc))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", pc=%s%s%s%s%s (%X)", pc_en[0], pc_en[1],
				 pc_en[2], pc_en[3], pc_total, pc);
	if (USES_PART(vreg, pf))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", pf=%s (%d)", pf_label, pf);
	if (USES_PART(vreg, pd))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", pd=%s (%d)", (pd == 1 ? "Y" : "N"), pd);
	if (USES_PART(vreg, ia))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", ia=%4d mA", GET_PART(vreg, ia));
	if (USES_PART(vreg, freq)) {
		if (vreg->type == RPM_REGULATOR_TYPE_NCP)
			pos += scnprintf(buf + pos, buflen - pos,
				       ", freq=%2d", freq);
		else
			pos += scnprintf(buf + pos, buflen - pos,
				       ", freq=%s MHz (%2d)", freq_label, freq);
	}
	if (USES_PART(vreg, pm))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", pm=%s (%d)", pm_label, pm);
	if (USES_PART(vreg, freq_clk_src))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", clk_src=%d", GET_PART(vreg, freq_clk_src));
	if (USES_PART(vreg, comp_mode))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", comp=%d", GET_PART(vreg, comp_mode));
	if (USES_PART(vreg, hpm))
		pos += scnprintf(buf + pos, buflen - pos,
				 ", hpm=%d", GET_PART(vreg, hpm));

	pos += scnprintf(buf + pos, buflen - pos, "; req[0]={%d, 0x%08X}",
			 vreg->req[0].id, vreg->req[0].value);
	if (vreg->part->request_len > 1)
		pos += scnprintf(buf + pos, buflen - pos,
				 ", req[1]={%d, 0x%08X}", vreg->req[1].id,
				 vreg->req[1].value);

	pos += scnprintf(buf + pos, buflen - pos, "\n");
	printk(buf);
}

static void rpm_regulator_vote(struct vreg *vreg, enum rpm_vreg_voter voter,
			int set, int voter_uV, int aggregate_uV)
{
	/* Suppress VDD_MEM and VDD_DIG printing. */
	if ((msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_IGNORE_VDD_MEM_DIG)
	    && vreg_id_is_vdd_mem_or_dig(vreg->id))
		return;

	pr_info("vote received %-9s: voter=%d, set=%c, v_voter=%7d uV, "
		"v_aggregate=%7d uV\n", vreg->rdesc.name, voter,
		(set == 0 ? 'A' : 'S'), voter_uV, aggregate_uV);
}

static void rpm_regulator_duplicate(struct vreg *vreg, int set, int cnt)
{
	/* Suppress VDD_MEM and VDD_DIG printing. */
	if ((msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_IGNORE_VDD_MEM_DIG)
	    && vreg_id_is_vdd_mem_or_dig(vreg->id))
		return;

	if (cnt == 2)
		pr_info("ignored request %-9s: set=%c; req[0]={%d, 0x%08X}, "
			"req[1]={%d, 0x%08X}\n", vreg->rdesc.name,
			(set == 0 ? 'A' : 'S'),
			vreg->req[0].id, vreg->req[0].value,
			vreg->req[1].id, vreg->req[1].value);
	else if (cnt == 1)
		pr_info("ignored request %-9s: set=%c; req[0]={%d, 0x%08X}\n",
			vreg->rdesc.name, (set == 0 ? 'A' : 'S'),
			vreg->req[0].id, vreg->req[0].value);
}

/* Spin lock needed for sleep-selectable regulators. */
static DEFINE_SPINLOCK(rpm_noirq_lock);

static int voltage_from_req(struct vreg *vreg)
{
	int uV = 0;

	if (vreg->part->uV.mask)
		uV = GET_PART(vreg, uV);
	else
		uV = MILLI_TO_MICRO(GET_PART(vreg, mV));

	return uV;
}

static void voltage_to_req(int uV, struct vreg *vreg)
{
	if (vreg->part->uV.mask)
		SET_PART(vreg, uV, uV);
	else
		SET_PART(vreg, mV, MICRO_TO_MILLI(uV));
}

static int vreg_send_request(struct vreg *vreg, enum rpm_vreg_voter voter,
			  int set, unsigned mask0, unsigned val0,
			  unsigned mask1, unsigned val1, unsigned cnt,
			  int update_voltage)
{
	struct msm_rpm_iv_pair *prev_req;
	int rc = 0, max_uV_vote = 0;
	unsigned prev0, prev1;
	int *min_uV_vote;
	int i;

	if (set == MSM_RPM_CTX_SET_0) {
		min_uV_vote = vreg->active_min_uV_vote;
		prev_req = vreg->prev_active_req;
	} else {
		min_uV_vote = vreg->sleep_min_uV_vote;
		prev_req = vreg->prev_sleep_req;
	}

	prev0 = vreg->req[0].value;
	vreg->req[0].value &= ~mask0;
	vreg->req[0].value |= val0 & mask0;

	prev1 = vreg->req[1].value;
	vreg->req[1].value &= ~mask1;
	vreg->req[1].value |= val1 & mask1;

	if (update_voltage)
		min_uV_vote[voter] = voltage_from_req(vreg);

	/* Find the highest voltage voted for and use it. */
	for (i = 0; i < RPM_VREG_VOTER_COUNT; i++)
		max_uV_vote = max(max_uV_vote, min_uV_vote[i]);
	voltage_to_req(max_uV_vote, vreg);

	if (msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_VOTE)
		rpm_regulator_vote(vreg, voter, set, min_uV_vote[voter],
				max_uV_vote);

	/* Ignore duplicate requests */
	if (vreg->req[0].value != prev_req[0].value ||
	    vreg->req[1].value != prev_req[1].value) {
		rc = msm_rpmrs_set_noirq(set, vreg->req, cnt);
		if (rc) {
			vreg->req[0].value = prev0;
			vreg->req[1].value = prev1;

			vreg_err(vreg, "msm_rpmrs_set_noirq failed - "
				"set=%s, id=%d, rc=%d\n",
				(set == MSM_RPM_CTX_SET_0 ? "active" : "sleep"),
				vreg->req[0].id, rc);
		} else {
			/* Only save if nonzero and active set. */
			if (max_uV_vote && (set == MSM_RPM_CTX_SET_0))
				vreg->save_uV = max_uV_vote;
			if (msm_rpm_vreg_debug_mask
			    & MSM_RPM_VREG_DEBUG_REQUEST)
				rpm_regulator_req(vreg, set);
			prev_req[0].value = vreg->req[0].value;
			prev_req[1].value = vreg->req[1].value;
		}
	} else if (msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_DUPLICATE) {
		rpm_regulator_duplicate(vreg, set, cnt);
	}

	return rc;
}

static int vreg_set_noirq(struct vreg *vreg, enum rpm_vreg_voter voter,
			  int sleep, unsigned mask0, unsigned val0,
			  unsigned mask1, unsigned val1, unsigned cnt,
			  int update_voltage)
{
	unsigned int s_mask[2] = {mask0, mask1}, s_val[2] = {val0, val1};
	unsigned long flags;
	int rc;

	if (voter < 0 || voter >= RPM_VREG_VOTER_COUNT)
		return -EINVAL;

	spin_lock_irqsave(&rpm_noirq_lock, flags);

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
		if (vreg->part->uV.mask) {
			s_val[vreg->part->uV.word] = 0 << vreg->part->uV.shift;
			s_mask[vreg->part->uV.word] = vreg->part->uV.mask;
		} else {
			s_val[vreg->part->mV.word] = 0 << vreg->part->mV.shift;
			s_mask[vreg->part->mV.word] = vreg->part->mV.mask;
		}

		rc = vreg_send_request(vreg, voter, MSM_RPM_CTX_SET_SLEEP,
				       s_mask[0], s_val[0], s_mask[1], s_val[1],
				       cnt, update_voltage);
	}

	rc = vreg_send_request(vreg, voter, MSM_RPM_CTX_SET_0, mask0, val0,
					mask1, val1, cnt, update_voltage);

	spin_unlock_irqrestore(&rpm_noirq_lock, flags);

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
 *
 * Consumers can vote to disable a regulator with this function by passing
 * min_uV = 0 and max_uV = 0.
 */
int rpm_vreg_set_voltage(int vreg_id, enum rpm_vreg_voter voter, int min_uV,
			 int max_uV, int sleep_also)
{
	unsigned int mask[2] = {0}, val[2] = {0};
	struct vreg_range *range;
	struct vreg *vreg;
	int uV = min_uV;
	int lim_min_uV, lim_max_uV, i, rc;

	/*
	 * HACK: make this function a no-op for 8064 so that it can be called by
	 * consumers on 8064 before RPM capabilities are present. (needed for
	 * acpuclock driver)
	 */
	if (cpu_is_apq8064())
		return 0;

	if (!config) {
		pr_err("rpm-regulator driver has not probed yet.\n");
		return -ENODEV;
	}

	if (vreg_id < config->vreg_id_min || vreg_id > config->vreg_id_max) {
		pr_err("invalid regulator id=%d\n", vreg_id);
		return -EINVAL;
	}

	vreg = &config->vregs[vreg_id];
	range = &vreg->set_points->range[0];

	if (!vreg->pdata.sleep_selectable) {
		vreg_err(vreg, "regulator is not marked sleep selectable\n");
		return -EINVAL;
	}

	/* Allow min_uV == max_uV == 0 to represent a disable request. */
	if (min_uV != 0 || max_uV != 0) {
		/*
		 * Check if request voltage is outside of allowed range. The
		 * regulator core has already checked that constraint range
		 * is inside of the physically allowed range.
		 */
		lim_min_uV = vreg->pdata.init_data.constraints.min_uV;
		lim_max_uV = vreg->pdata.init_data.constraints.max_uV;

		if (uV < lim_min_uV && max_uV >= lim_min_uV)
			uV = lim_min_uV;

		if (uV < lim_min_uV || uV > lim_max_uV) {
			vreg_err(vreg, "request v=[%d, %d] is outside allowed "
				"v=[%d, %d]\n", min_uV, max_uV, lim_min_uV,
				lim_max_uV);
			return -EINVAL;
		}

		/* Find the range which uV is inside of. */
		for (i = vreg->set_points->count - 1; i > 0; i--) {
			if (uV > vreg->set_points->range[i - 1].max_uV) {
				range = &vreg->set_points->range[i];
				break;
			}
		}

		/*
		 * Force uV to be an allowed set point and apply a ceiling
		 * function to non-set point values.
		 */
		uV = (uV - range->min_uV + range->step_uV - 1) / range->step_uV;
		uV = uV * range->step_uV + range->min_uV;

		if (uV > max_uV) {
			vreg_err(vreg,
			  "request v=[%d, %d] cannot be met by any set point; "
			  "next set point: %d\n",
			  min_uV, max_uV, uV);
			return -EINVAL;
		}
	}

	if (vreg->part->uV.mask) {
		val[vreg->part->uV.word] = uV << vreg->part->uV.shift;
		mask[vreg->part->uV.word] = vreg->part->uV.mask;
	} else {
		val[vreg->part->mV.word]
			= MICRO_TO_MILLI(uV) << vreg->part->mV.shift;
		mask[vreg->part->mV.word] = vreg->part->mV.mask;
	}

	rc = vreg_set_noirq(vreg, voter, sleep_also, mask[0], val[0], mask[1],
			    val[1], vreg->part->request_len, 1);
	if (rc)
		vreg_err(vreg, "vreg_set_noirq failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL_GPL(rpm_vreg_set_voltage);

/**
 * rpm_vreg_set_frequency - sets the frequency of a switching regulator
 * @vreg: ID for regulator
 * @freq: enum corresponding to desired frequency
 *
 * Returns 0 on success or errno.
 */
int rpm_vreg_set_frequency(int vreg_id, enum rpm_vreg_freq freq)
{
	unsigned int mask[2] = {0}, val[2] = {0};
	struct vreg *vreg;
	int rc;

	/*
	 * HACK: make this function a no-op for 8064 so that it can be called by
	 * consumers on 8064 before RPM capabilities are present.
	 */
	if (cpu_is_apq8064())
		return 0;

	if (!config) {
		pr_err("rpm-regulator driver has not probed yet.\n");
		return -ENODEV;
	}

	if (vreg_id < config->vreg_id_min || vreg_id > config->vreg_id_max) {
		pr_err("invalid regulator id=%d\n", vreg_id);
		return -EINVAL;
	}

	vreg = &config->vregs[vreg_id];

	if (freq < 0 || freq > RPM_VREG_FREQ_1p20) {
		vreg_err(vreg, "invalid frequency=%d\n", freq);
		return -EINVAL;
	}
	if (!vreg->pdata.sleep_selectable) {
		vreg_err(vreg, "regulator is not marked sleep selectable\n");
		return -EINVAL;
	}
	if (!vreg->part->freq.mask) {
		vreg_err(vreg, "frequency not supported\n");
		return -EINVAL;
	}

	val[vreg->part->freq.word] = freq << vreg->part->freq.shift;
	mask[vreg->part->freq.word] = vreg->part->freq.mask;

	rc = vreg_set_noirq(vreg, RPM_VREG_VOTER_REG_FRAMEWORK, 1, mask[0],
			   val[0], mask[1], val[1], vreg->part->request_len, 0);
	if (rc)
		vreg_err(vreg, "vreg_set failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL_GPL(rpm_vreg_set_frequency);

static inline int vreg_hpm_min_uA(struct vreg *vreg)
{
	return vreg->hpm_min_load;
}

static inline int vreg_lpm_max_uA(struct vreg *vreg)
{
	return vreg->hpm_min_load - LOAD_THRESHOLD_STEP;
}

static inline unsigned saturate_peak_load(struct vreg *vreg, unsigned load_uA)
{
	unsigned load_max
		= MILLI_TO_MICRO(vreg->part->ip.mask >> vreg->part->ip.shift);

	return (load_uA > load_max ? load_max : load_uA);
}

static inline unsigned saturate_avg_load(struct vreg *vreg, unsigned load_uA)
{
	unsigned load_max
		= MILLI_TO_MICRO(vreg->part->ia.mask >> vreg->part->ia.shift);
	return (load_uA > load_max ? load_max : load_uA);
}

/* Change vreg->req, but do not send it to the RPM. */
static int vreg_store(struct vreg *vreg, unsigned mask0, unsigned val0,
		unsigned mask1, unsigned val1)
{
	unsigned long flags = 0;

	if (vreg->pdata.sleep_selectable)
		spin_lock_irqsave(&rpm_noirq_lock, flags);

	vreg->req[0].value &= ~mask0;
	vreg->req[0].value |= val0 & mask0;

	vreg->req[1].value &= ~mask1;
	vreg->req[1].value |= val1 & mask1;

	if (vreg->pdata.sleep_selectable)
		spin_unlock_irqrestore(&rpm_noirq_lock, flags);

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
	if (vreg->pdata.sleep_selectable)
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
			rpm_regulator_duplicate(vreg, MSM_RPM_CTX_SET_0, cnt);
		return 0;
	}

	rc = msm_rpm_set(MSM_RPM_CTX_SET_0, vreg->req, cnt);
	if (rc) {
		vreg->req[0].value = prev0;
		vreg->req[1].value = prev1;

		vreg_err(vreg, "msm_rpm_set failed, set=active, id=%d, rc=%d\n",
			vreg->req[0].id, rc);
	} else {
		if (msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_REQUEST)
			rpm_regulator_req(vreg, MSM_RPM_CTX_SET_0);
		vreg->prev_active_req[0].value = vreg->req[0].value;
		vreg->prev_active_req[1].value = vreg->req[1].value;
	}

	return rc;
}

static int vreg_is_enabled(struct regulator_dev *rdev)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);
	int enabled;

	mutex_lock(&vreg->pc_lock);
	enabled = vreg->is_enabled;
	mutex_unlock(&vreg->pc_lock);

	return enabled;
}

static void set_enable(struct vreg *vreg, unsigned int *mask, unsigned int *val)
{
	switch (vreg->type) {
	case RPM_REGULATOR_TYPE_LDO:
	case RPM_REGULATOR_TYPE_SMPS:
		/* Enable by setting a voltage. */
		if (vreg->part->uV.mask) {
			val[vreg->part->uV.word]
				|= vreg->save_uV << vreg->part->uV.shift;
			mask[vreg->part->uV.word] |= vreg->part->uV.mask;
		} else {
			val[vreg->part->mV.word]
				|= MICRO_TO_MILLI(vreg->save_uV)
					<< vreg->part->mV.shift;
			mask[vreg->part->mV.word] |= vreg->part->mV.mask;
		}
		break;
	case RPM_REGULATOR_TYPE_VS:
	case RPM_REGULATOR_TYPE_NCP:
		/* Enable by setting enable_state. */
		val[vreg->part->enable_state.word]
			|= RPM_VREG_STATE_ON << vreg->part->enable_state.shift;
		mask[vreg->part->enable_state.word]
			|= vreg->part->enable_state.mask;
	}
}

static int vreg_enable(struct regulator_dev *rdev)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mask[2] = {0}, val[2] = {0};
	int rc = 0;

	set_enable(vreg, mask, val);

	mutex_lock(&vreg->pc_lock);

	rc = vreg_set(vreg, mask[0], val[0], mask[1], val[1],
			vreg->part->request_len);
	if (!rc)
		vreg->is_enabled = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "vreg_set failed, rc=%d\n", rc);

	return rc;
}

static void set_disable(struct vreg *vreg, unsigned int *mask,
			unsigned int *val)
{
	switch (vreg->type) {
	case RPM_REGULATOR_TYPE_LDO:
	case RPM_REGULATOR_TYPE_SMPS:
		/* Disable by setting a voltage of 0 uV. */
		if (vreg->part->uV.mask) {
			val[vreg->part->uV.word] |= 0 << vreg->part->uV.shift;
			mask[vreg->part->uV.word] |= vreg->part->uV.mask;
		} else {
			val[vreg->part->mV.word] |= 0 << vreg->part->mV.shift;
			mask[vreg->part->mV.word] |= vreg->part->mV.mask;
		}
		break;
	case RPM_REGULATOR_TYPE_VS:
	case RPM_REGULATOR_TYPE_NCP:
		/* Disable by setting enable_state. */
		val[vreg->part->enable_state.word]
			|= RPM_VREG_STATE_OFF << vreg->part->enable_state.shift;
		mask[vreg->part->enable_state.word]
			|= vreg->part->enable_state.mask;
	}
}

static int vreg_disable(struct regulator_dev *rdev)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mask[2] = {0}, val[2] = {0};
	int rc = 0;

	set_disable(vreg, mask, val);

	mutex_lock(&vreg->pc_lock);

	/* Only disable if pin control is not in use. */
	if (!vreg->is_enabled_pc)
		rc = vreg_set(vreg, mask[0], val[0], mask[1], val[1],
				vreg->part->request_len);

	if (!rc)
		vreg->is_enabled = false;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "vreg_set failed, rc=%d\n", rc);

	return rc;
}

static int vreg_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV,
			    unsigned *selector)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);
	struct vreg_range *range = &vreg->set_points->range[0];
	unsigned int mask[2] = {0}, val[2] = {0};
	int rc = 0, uV = min_uV;
	int lim_min_uV, lim_max_uV, i;

	/* Check if request voltage is outside of physically settable range. */
	lim_min_uV = vreg->set_points->range[0].min_uV;
	lim_max_uV =
		vreg->set_points->range[vreg->set_points->count - 1].max_uV;

	if (uV < lim_min_uV && max_uV >= lim_min_uV)
		uV = lim_min_uV;

	if (uV < lim_min_uV || uV > lim_max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, lim_min_uV, lim_max_uV);
		return -EINVAL;
	}

	/* Find the range which uV is inside of. */
	for (i = vreg->set_points->count - 1; i > 0; i--) {
		if (uV > vreg->set_points->range[i - 1].max_uV) {
			range = &vreg->set_points->range[i];
			break;
		}
	}

	/*
	 * Force uV to be an allowed set point and apply a ceiling function
	 * to non-set point values.
	 */
	uV = (uV - range->min_uV + range->step_uV - 1) / range->step_uV;
	uV = uV * range->step_uV + range->min_uV;

	if (uV > max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] cannot be met by any set point; "
			"next set point: %d\n",
			min_uV, max_uV, uV);
		return -EINVAL;
	}

	if (vreg->part->uV.mask) {
		val[vreg->part->uV.word] = uV << vreg->part->uV.shift;
		mask[vreg->part->uV.word] = vreg->part->uV.mask;
	} else {
		val[vreg->part->mV.word]
			= MICRO_TO_MILLI(uV) << vreg->part->mV.shift;
		mask[vreg->part->mV.word] = vreg->part->mV.mask;
	}

	mutex_lock(&vreg->pc_lock);

	/*
	 * Only send a request for a new voltage if the regulator is currently
	 * enabled.  This will ensure that LDO and SMPS regulators are not
	 * inadvertently turned on because voltage > 0 is equivalent to
	 * enabling.  For NCP, this just removes unnecessary RPM requests.
	 */
	if (vreg->is_enabled) {
		rc = vreg_set(vreg, mask[0], val[0], mask[1], val[1],
				vreg->part->request_len);
		if (rc)
			vreg_err(vreg, "vreg_set failed, rc=%d\n", rc);
	} else if (vreg->type == RPM_REGULATOR_TYPE_NCP) {
		/* Regulator is disabled; store but don't send new request. */
		rc = vreg_store(vreg, mask[0], val[0], mask[1], val[1]);
	}

	if (!rc && (!vreg->pdata.sleep_selectable || !vreg->is_enabled))
		vreg->save_uV = uV;

	mutex_unlock(&vreg->pc_lock);

	return rc;
}

static int vreg_get_voltage(struct regulator_dev *rdev)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->save_uV;
}

static int vreg_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);
	int uV = 0;
	int i;

	if (!vreg->set_points) {
		vreg_err(vreg, "no voltages available\n");
		return -EINVAL;
	}

	if (selector >= vreg->set_points->n_voltages)
		return 0;

	for (i = 0; i < vreg->set_points->count; i++) {
		if (selector < vreg->set_points->range[i].n_voltages) {
			uV = selector * vreg->set_points->range[i].step_uV
				+ vreg->set_points->range[i].min_uV;
			break;
		} else {
			selector -= vreg->set_points->range[i].n_voltages;
		}
	}

	return uV;
}

static int vreg_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mask[2] = {0}, val[2] = {0};
	int rc = 0;
	int peak_uA;

	mutex_lock(&vreg->pc_lock);

	peak_uA = MILLI_TO_MICRO((vreg->req[vreg->part->ip.word].value
				& vreg->part->ip.mask) >> vreg->part->ip.shift);

	if (mode == config->mode_hpm) {
		/* Make sure that request currents are in HPM range. */
		if (peak_uA < vreg_hpm_min_uA(vreg)) {
			val[vreg->part->ip.word]
				= MICRO_TO_MILLI(vreg_hpm_min_uA(vreg))
					<< vreg->part->ip.shift;
			mask[vreg->part->ip.word] = vreg->part->ip.mask;

			if (config->ia_follows_ip) {
				val[vreg->part->ia.word]
					|= MICRO_TO_MILLI(vreg_hpm_min_uA(vreg))
						<< vreg->part->ia.shift;
				mask[vreg->part->ia.word]
					|= vreg->part->ia.mask;
			}
		}
	} else if (mode == config->mode_lpm) {
		/* Make sure that request currents are in LPM range. */
		if (peak_uA > vreg_lpm_max_uA(vreg)) {
			val[vreg->part->ip.word]
				= MICRO_TO_MILLI(vreg_lpm_max_uA(vreg))
					<< vreg->part->ip.shift;
			mask[vreg->part->ip.word] = vreg->part->ip.mask;

			if (config->ia_follows_ip) {
				val[vreg->part->ia.word]
					|= MICRO_TO_MILLI(vreg_lpm_max_uA(vreg))
						<< vreg->part->ia.shift;
				mask[vreg->part->ia.word]
					|= vreg->part->ia.mask;
			}
		}
	} else {
		vreg_err(vreg, "invalid mode: %u\n", mode);
		mutex_unlock(&vreg->pc_lock);
		return -EINVAL;
	}

	if (vreg->is_enabled) {
		rc = vreg_set(vreg, mask[0], val[0], mask[1], val[1],
					vreg->part->request_len);
	} else {
		/* Regulator is disabled; store but don't send new request. */
		rc = vreg_store(vreg, mask[0], val[0], mask[1], val[1]);
	}

	if (rc)
		vreg_err(vreg, "vreg_set failed, rc=%d\n", rc);
	else
		vreg->mode = mode;

	mutex_unlock(&vreg->pc_lock);

	return rc;
}

static unsigned int vreg_get_mode(struct regulator_dev *rdev)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->mode;
}

static unsigned int vreg_get_optimum_mode(struct regulator_dev *rdev,
			int input_uV, int output_uV, int load_uA)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mode;

	load_uA += vreg->pdata.system_uA;

	mutex_lock(&vreg->pc_lock);
	SET_PART(vreg, ip, MICRO_TO_MILLI(saturate_peak_load(vreg, load_uA)));
	if (config->ia_follows_ip)
		SET_PART(vreg, ia,
			 MICRO_TO_MILLI(saturate_avg_load(vreg, load_uA)));
	mutex_unlock(&vreg->pc_lock);

	if (load_uA >= vreg->hpm_min_load)
		mode = config->mode_hpm;
	else
		mode = config->mode_lpm;

	return mode;
}

static unsigned int vreg_legacy_get_optimum_mode(struct regulator_dev *rdev,
			int input_uV, int output_uV, int load_uA)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);

	if (MICRO_TO_MILLI(load_uA) <= 0) {
		/*
		 * vreg_legacy_get_optimum_mode is being called before consumers
		 * have specified their load currents via
		 * regulator_set_optimum_mode. Return whatever the existing mode
		 * is.
		 */
		return vreg->mode;
	}

	return vreg_get_optimum_mode(rdev, input_uV, output_uV, load_uA);
}

/*
 * Returns the logical pin control enable state because the pin control options
 * present in the hardware out of restart could be different from those desired
 * by the consumer.
 */
static int vreg_pin_control_is_enabled(struct regulator_dev *rdev)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->is_enabled_pc;
}

static int vreg_pin_control_enable(struct regulator_dev *rdev)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mask[2] = {0}, val[2] = {0};
	int rc;

	mutex_lock(&vreg->pc_lock);

	val[vreg->part->pc.word]
		|= vreg->pdata.pin_ctrl << vreg->part->pc.shift;
	mask[vreg->part->pc.word] |= vreg->part->pc.mask;

	val[vreg->part->pf.word]  |= vreg->pdata.pin_fn << vreg->part->pf.shift;
	mask[vreg->part->pf.word] |= vreg->part->pf.mask;

	if (!vreg->is_enabled)
		set_enable(vreg, mask, val);

	rc = vreg_set(vreg, mask[0], val[0], mask[1], val[1],
			vreg->part->request_len);

	if (!rc)
		vreg->is_enabled_pc = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "vreg_set failed, rc=%d\n", rc);

	return rc;
}

static int vreg_pin_control_disable(struct regulator_dev *rdev)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mask[2] = {0}, val[2] = {0};
	int pin_fn, rc;

	mutex_lock(&vreg->pc_lock);

	val[vreg->part->pc.word]
		|= RPM_VREG_PIN_CTRL_NONE << vreg->part->pc.shift;
	mask[vreg->part->pc.word] |= vreg->part->pc.mask;

	pin_fn = config->pin_func_none;
	if (vreg->pdata.pin_fn == config->pin_func_sleep_b)
		pin_fn = config->pin_func_sleep_b;
	val[vreg->part->pf.word]  |= pin_fn << vreg->part->pf.shift;
	mask[vreg->part->pf.word] |= vreg->part->pf.mask;

	if (!vreg->is_enabled)
		set_disable(vreg, mask, val);

	rc = vreg_set(vreg, mask[0], val[0], mask[1], val[1],
			vreg->part->request_len);

	if (!rc)
		vreg->is_enabled_pc = false;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "vreg_set failed, rc=%d\n", rc);

	return rc;
}

static int vreg_enable_time(struct regulator_dev *rdev)
{
	struct vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->pdata.enable_time;
}

/* Real regulator operations. */
static struct regulator_ops ldo_ops = {
	.enable			= vreg_enable,
	.disable		= vreg_disable,
	.is_enabled		= vreg_is_enabled,
	.set_voltage		= vreg_set_voltage,
	.get_voltage		= vreg_get_voltage,
	.list_voltage		= vreg_list_voltage,
	.set_mode		= vreg_set_mode,
	.get_mode		= vreg_get_mode,
	.get_optimum_mode	= vreg_get_optimum_mode,
	.enable_time		= vreg_enable_time,
};

static struct regulator_ops smps_ops = {
	.enable			= vreg_enable,
	.disable		= vreg_disable,
	.is_enabled		= vreg_is_enabled,
	.set_voltage		= vreg_set_voltage,
	.get_voltage		= vreg_get_voltage,
	.list_voltage		= vreg_list_voltage,
	.set_mode		= vreg_set_mode,
	.get_mode		= vreg_get_mode,
	.get_optimum_mode	= vreg_get_optimum_mode,
	.enable_time		= vreg_enable_time,
};

static struct regulator_ops switch_ops = {
	.enable			= vreg_enable,
	.disable		= vreg_disable,
	.is_enabled		= vreg_is_enabled,
	.enable_time		= vreg_enable_time,
};

static struct regulator_ops ncp_ops = {
	.enable			= vreg_enable,
	.disable		= vreg_disable,
	.is_enabled		= vreg_is_enabled,
	.set_voltage		= vreg_set_voltage,
	.get_voltage		= vreg_get_voltage,
	.list_voltage		= vreg_list_voltage,
	.enable_time		= vreg_enable_time,
};

/* Pin control regulator operations. */
static struct regulator_ops pin_control_ops = {
	.enable			= vreg_pin_control_enable,
	.disable		= vreg_pin_control_disable,
	.is_enabled		= vreg_pin_control_is_enabled,
};

struct regulator_ops *vreg_ops[] = {
	[RPM_REGULATOR_TYPE_LDO]	= &ldo_ops,
	[RPM_REGULATOR_TYPE_SMPS]	= &smps_ops,
	[RPM_REGULATOR_TYPE_VS]		= &switch_ops,
	[RPM_REGULATOR_TYPE_NCP]	= &ncp_ops,
};

static int __devinit
rpm_vreg_init_regulator(const struct rpm_regulator_init_data *pdata,
			struct device *dev)
{
	struct regulator_desc *rdesc = NULL;
	struct regulator_dev *rdev;
	struct vreg *vreg;
	unsigned pin_ctrl;
	int id, pin_fn;
	int rc = 0;

	if (!pdata) {
		pr_err("platform data missing\n");
		return -EINVAL;
	}

	id = pdata->id;

	if (id < config->vreg_id_min || id > config->vreg_id_max) {
		pr_err("invalid regulator id: %d\n", id);
		return -ENODEV;
	}

	if (!config->is_real_id(pdata->id))
		id = config->pc_id_to_real_id(pdata->id);
	vreg = &config->vregs[id];

	if (config->is_real_id(pdata->id))
		rdesc = &vreg->rdesc;
	else
		rdesc = &vreg->rdesc_pc;

	if (vreg->type < 0 || vreg->type > RPM_REGULATOR_TYPE_MAX) {
		pr_err("%s: invalid regulator type: %d\n",
			vreg->rdesc.name, vreg->type);
		return -EINVAL;
	}

	mutex_lock(&vreg->pc_lock);

	if (vreg->set_points)
		rdesc->n_voltages = vreg->set_points->n_voltages;
	else
		rdesc->n_voltages = 0;

	rdesc->id    = pdata->id;
	rdesc->owner = THIS_MODULE;
	rdesc->type  = REGULATOR_VOLTAGE;

	if (config->is_real_id(pdata->id)) {
		/*
		 * Real regulator; do not modify pin control and pin function
		 * values.
		 */
		rdesc->ops = vreg_ops[vreg->type];
		pin_ctrl = vreg->pdata.pin_ctrl;
		pin_fn = vreg->pdata.pin_fn;
		memcpy(&(vreg->pdata), pdata,
			sizeof(struct rpm_regulator_init_data));
		vreg->pdata.pin_ctrl = pin_ctrl;
		vreg->pdata.pin_fn = pin_fn;

		vreg->save_uV = vreg->pdata.default_uV;
		if (vreg->pdata.peak_uA >= vreg->hpm_min_load)
			vreg->mode = config->mode_hpm;
		else
			vreg->mode = config->mode_lpm;

		/* Initialize the RPM request. */
		SET_PART(vreg, ip,
		 MICRO_TO_MILLI(saturate_peak_load(vreg, vreg->pdata.peak_uA)));
		SET_PART(vreg, fm, vreg->pdata.force_mode);
		SET_PART(vreg, pm, vreg->pdata.power_mode);
		SET_PART(vreg, pd, vreg->pdata.pull_down_enable);
		SET_PART(vreg, ia,
		   MICRO_TO_MILLI(saturate_avg_load(vreg, vreg->pdata.avg_uA)));
		SET_PART(vreg, freq, vreg->pdata.freq);
		SET_PART(vreg, freq_clk_src, 0);
		SET_PART(vreg, comp_mode, 0);
		SET_PART(vreg, hpm, 0);
		if (!vreg->is_enabled_pc) {
			SET_PART(vreg, pf, config->pin_func_none);
			SET_PART(vreg, pc, RPM_VREG_PIN_CTRL_NONE);
		}
	} else {
		if ((pdata->pin_ctrl & RPM_VREG_PIN_CTRL_ALL)
		      == RPM_VREG_PIN_CTRL_NONE
		    && pdata->pin_fn != config->pin_func_sleep_b) {
			pr_err("%s: no pin control input specified\n",
				vreg->rdesc.name);
			mutex_unlock(&vreg->pc_lock);
			return -EINVAL;
		}
		rdesc->ops = &pin_control_ops;
		vreg->pdata.pin_ctrl = pdata->pin_ctrl;
		vreg->pdata.pin_fn = pdata->pin_fn;

		/* Initialize the RPM request. */
		pin_fn = config->pin_func_none;
		/* Allow pf=sleep_b to be specified by platform data. */
		if (vreg->pdata.pin_fn == config->pin_func_sleep_b)
			pin_fn = config->pin_func_sleep_b;
		SET_PART(vreg, pf, pin_fn);
		SET_PART(vreg, pc, RPM_VREG_PIN_CTRL_NONE);
	}

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		goto bail;

	rdev = regulator_register(rdesc, dev, &(pdata->init_data), vreg);
	if (IS_ERR(rdev)) {
		rc = PTR_ERR(rdev);
		pr_err("regulator_register failed: %s, rc=%d\n",
			vreg->rdesc.name, rc);
		return rc;
	} else {
		if (config->is_real_id(pdata->id))
			vreg->rdev = rdev;
		else
			vreg->rdev_pc = rdev;
	}

bail:
	if (rc)
		pr_err("error for %s, rc=%d\n", vreg->rdesc.name, rc);

	return rc;
}

static void rpm_vreg_set_point_init(void)
{
	struct vreg_set_points **set_points;
	int i, j, temp;

	set_points = config->set_points;

	/* Calculate the number of set points available for each regulator. */
	for (i = 0; i < config->set_points_len; i++) {
		temp = 0;
		for (j = 0; j < set_points[i]->count; j++) {
			set_points[i]->range[j].n_voltages
				= (set_points[i]->range[j].max_uV
					- set_points[i]->range[j].min_uV)
				   / set_points[i]->range[j].step_uV + 1;
			temp += set_points[i]->range[j].n_voltages;
		}
		set_points[i]->n_voltages = temp;
	}
}

static int __devinit rpm_vreg_probe(struct platform_device *pdev)
{
	struct rpm_regulator_platform_data *platform_data;
	int rc = 0;
	int i, id;

	platform_data = pdev->dev.platform_data;
	if (!platform_data) {
		pr_err("rpm-regulator requires platform data\n");
		return -EINVAL;
	}

	if (rpm_version >= 0 && rpm_version <= RPM_VREG_VERSION_MAX
	    && platform_data->version != rpm_version) {
		pr_err("rpm version %d does not match previous version %d\n",
			platform_data->version, rpm_version);
		return -EINVAL;
	}

	if (platform_data->version < 0
		|| platform_data->version > RPM_VREG_VERSION_MAX) {
		pr_err("rpm version %d is invalid\n", platform_data->version);
		return -EINVAL;
	}

	if (rpm_version < 0 || rpm_version > RPM_VREG_VERSION_MAX) {
		rpm_version = platform_data->version;
		config = get_config[platform_data->version]();
		vreg_id_vdd_mem = platform_data->vreg_id_vdd_mem;
		vreg_id_vdd_dig = platform_data->vreg_id_vdd_dig;
		if (!config) {
			pr_err("rpm version %d is not available\n",
				platform_data->version);
			return -ENODEV;
		}
		if (config->use_legacy_optimum_mode)
			for (i = 0; i < ARRAY_SIZE(vreg_ops); i++)
				vreg_ops[i]->get_optimum_mode
					= vreg_legacy_get_optimum_mode;
		rpm_vreg_set_point_init();
		/* First time probed; initialize pin control mutexes. */
		for (i = 0; i < config->vregs_len; i++)
			mutex_init(&config->vregs[i].pc_lock);
	}

	/* Initialize all of the regulators listed in the platform data. */
	for (i = 0; i < platform_data->num_regulators; i++) {
		rc = rpm_vreg_init_regulator(&platform_data->init_data[i],
			&pdev->dev);
		if (rc) {
			pr_err("rpm_vreg_init_regulator failed, rc=%d\n", rc);
			goto remove_regulators;
		}
	}

	platform_set_drvdata(pdev, platform_data);

	return rc;

remove_regulators:
	/* Unregister all regulators added before the erroring one. */
	for (; i >= 0; i--) {
		id = platform_data->init_data[i].id;
		if (config->is_real_id(id)) {
			regulator_unregister(config->vregs[id].rdev);
			config->vregs[id].rdev = NULL;
		} else {
			regulator_unregister(config->vregs[
				config->pc_id_to_real_id(id)].rdev_pc);
			config->vregs[id].rdev_pc = NULL;
		}
	}

	return rc;
}

static int __devexit rpm_vreg_remove(struct platform_device *pdev)
{
	struct rpm_regulator_platform_data *platform_data;
	int i, id;

	platform_data = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);

	if (platform_data) {
		for (i = 0; i < platform_data->num_regulators; i++) {
			id = platform_data->init_data[i].id;
			if (config->is_real_id(id)) {
				regulator_unregister(config->vregs[id].rdev);
				config->vregs[id].rdev = NULL;
			} else {
				regulator_unregister(config->vregs[
					config->pc_id_to_real_id(id)].rdev_pc);
				config->vregs[id].rdev_pc = NULL;
			}
		}
	}

	return 0;
}

static struct platform_driver rpm_vreg_driver = {
	.probe = rpm_vreg_probe,
	.remove = __devexit_p(rpm_vreg_remove),
	.driver = {
		.name = RPM_REGULATOR_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init rpm_vreg_init(void)
{
	return platform_driver_register(&rpm_vreg_driver);
}

static void __exit rpm_vreg_exit(void)
{
	int i;

	platform_driver_unregister(&rpm_vreg_driver);

	for (i = 0; i < config->vregs_len; i++)
		mutex_destroy(&config->vregs[i].pc_lock);
}

postcore_initcall(rpm_vreg_init);
module_exit(rpm_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM RPM regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" RPM_REGULATOR_DEV_NAME);
