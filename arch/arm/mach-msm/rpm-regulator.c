/*
 * Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/regulator/driver.h>

#include <mach/rpm.h>
#include <mach/rpm-regulator.h>
#include <mach/rpm-regulator-smd.h>
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

/* Used for access via the rpm_regulator_* API. */
struct rpm_regulator {
	int			vreg_id;
	enum rpm_vreg_voter	voter;
	int			sleep_also;
	int			min_uV;
	int			max_uV;
};

struct vreg_config *(*get_config[])(void) = {
	[RPM_VREG_VERSION_8660] = get_config_8660,
	[RPM_VREG_VERSION_8960] = get_config_8960,
	[RPM_VREG_VERSION_9615] = get_config_9615,
	[RPM_VREG_VERSION_8930] = get_config_8930,
	[RPM_VREG_VERSION_8930_PM8917] = get_config_8930_pm8917,
	[RPM_VREG_VERSION_8960_PM8917] = get_config_8960_pm8917,
};

static struct rpm_regulator_consumer_mapping *consumer_map;
static int consumer_map_len;

#define SET_PART(_vreg, _part, _val) \
	_vreg->req[_vreg->part->_part.word].value \
		= (_vreg->req[_vreg->part->_part.word].value \
			& ~_vreg->part->_part.mask) \
		  | (((_val) << _vreg->part->_part.shift) \
			& _vreg->part->_part.mask)

#define GET_PART(_vreg, _part) \
	((_vreg->req[_vreg->part->_part.word].value & _vreg->part->_part.mask) \
		>> _vreg->part->_part.shift)

#define GET_PART_PREV_ACT(_vreg, _part) \
	((_vreg->prev_active_req[_vreg->part->_part.word].value \
	  & _vreg->part->_part.mask) \
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

static const char *label_corner[] = {
	[RPM_VREG_CORNER_NONE]		= "NONE",
	[RPM_VREG_CORNER_LOW]		= "LOW",
	[RPM_VREG_CORNER_NOMINAL]	= "NOM",
	[RPM_VREG_CORNER_HIGH]		= "HIGH",
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
	const char *pm_label = "", *freq_label = "", *corner_label = "";
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

	if (USES_PART(vreg, uV) && vreg->type != RPM_REGULATOR_TYPE_CORNER)
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
	if (USES_PART(vreg, uV) && vreg->type == RPM_REGULATOR_TYPE_CORNER) {
		if (uV >= 0 && uV < (ARRAY_SIZE(label_corner) - 1))
			corner_label = label_corner[uV+1];
		pos += scnprintf(buf + pos, buflen - pos, ", corner=%s (%d)",
			corner_label, uV);
	}

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

static bool requires_tcxo_workaround;
static struct clk *tcxo_handle;
static struct wake_lock tcxo_wake_lock;
static DEFINE_MUTEX(tcxo_mutex);
static bool tcxo_is_enabled;
/*
 * TCXO must be kept on for at least the duration of its warmup (4 ms);
 * otherwise, it will stay on when hardware disabling is attempted.
 */
#define TCXO_WARMUP_TIME_MS 4

static void tcxo_get_handle(void)
{
	if (!tcxo_handle) {
		tcxo_handle = clk_get_sys("rpm-regulator", "vref_buff");
		if (IS_ERR(tcxo_handle))
			tcxo_handle = NULL;
	}
}

/*
 * Perform best effort enable of CXO.  Since the MSM clock drivers depend upon
 * the rpm-regulator driver, any rpm-regulator devices that are configured with
 * always_on == 1 will not be able to enable CXO during probe.  This does not
 * cause a problem though since CXO will be enabled by the boot loaders before
 * Apps boots up.
 */
static bool tcxo_enable(void)
{
	int rc;

	if (tcxo_handle && !tcxo_is_enabled) {
		rc = clk_prepare_enable(tcxo_handle);
		if (!rc) {
			tcxo_is_enabled = true;
			wake_lock(&tcxo_wake_lock);
			return true;
		}
	}

	return false;
}

static void tcxo_delayed_disable_work(struct work_struct *work)
{
	mutex_lock(&tcxo_mutex);

	clk_disable_unprepare(tcxo_handle);
	tcxo_is_enabled = false;
	wake_unlock(&tcxo_wake_lock);

	mutex_unlock(&tcxo_mutex);
}

static DECLARE_DELAYED_WORK(tcxo_disable_work, tcxo_delayed_disable_work);

static void tcxo_delayed_disable(void)
{
	/*
	 * The delay in jiffies has 1 added to it to ensure that at least
	 * one jiffy takes place before the work is enqueued.  Without this,
	 * the work would be scheduled to run in the very next jiffy which could
	 * result in too little delay and TCXO being stuck on.
	 */
	if (tcxo_handle)
		schedule_delayed_work(&tcxo_disable_work,
				msecs_to_jiffies(TCXO_WARMUP_TIME_MS) + 1);
}

/* Mutex lock needed for sleep-selectable regulators. */
static DEFINE_MUTEX(rpm_sleep_sel_lock);

static int voltage_from_req(struct vreg *vreg)
{
	int uV = 0;

	if (vreg->part->uV.mask)
		uV = GET_PART(vreg, uV);
	else if (vreg->part->mV.mask)
		uV = MILLI_TO_MICRO(GET_PART(vreg, mV));
	else if (vreg->part->enable_state.mask)
		uV = GET_PART(vreg, enable_state);

	return uV;
}

static void voltage_to_req(int uV, struct vreg *vreg)
{
	if (vreg->part->uV.mask)
		SET_PART(vreg, uV, uV);
	else if (vreg->part->mV.mask)
		SET_PART(vreg, mV, MICRO_TO_MILLI(uV));
	else if (vreg->part->enable_state.mask)
		SET_PART(vreg, enable_state, uV);
}

static int vreg_send_request(struct vreg *vreg, enum rpm_vreg_voter voter,
			  int set, unsigned mask0, unsigned val0,
			  unsigned mask1, unsigned val1, unsigned cnt,
			  int update_voltage)
{
	struct msm_rpm_iv_pair *prev_req;
	int rc = 0, max_uV_vote = 0;
	bool tcxo_enabled = false;
	bool voltage_increased = false;
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

	/* Set the force mode field based on which set is being requested. */
	if (set == MSM_RPM_CTX_SET_0)
		SET_PART(vreg, fm, vreg->pdata.force_mode);
	else
		SET_PART(vreg, fm, vreg->pdata.sleep_set_force_mode);

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

		/* Enable CXO clock if necessary for TCXO workaround. */
		if (requires_tcxo_workaround && vreg->requires_cxo
		    && (set == MSM_RPM_CTX_SET_0)
		    && (GET_PART(vreg, uV) > GET_PART_PREV_ACT(vreg, uV))) {
			mutex_lock(&tcxo_mutex);
			if (!tcxo_handle)
				tcxo_get_handle();
			voltage_increased = true;
			tcxo_enabled = tcxo_enable();
		}

		rc = msm_rpmrs_set(set, vreg->req, cnt);
		if (rc) {
			vreg->req[0].value = prev0;
			vreg->req[1].value = prev1;

			vreg_err(vreg, "msm_rpmrs_set failed - "
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

		/*
		 * Schedule CXO clock to be disabled after TCXO warmup time if
		 * TCXO workaround is applicable for this regulator.
		 */
		if (voltage_increased) {
			if (tcxo_enabled)
				tcxo_delayed_disable();
			mutex_unlock(&tcxo_mutex);
		}
	} else if (msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_DUPLICATE) {
		rpm_regulator_duplicate(vreg, set, cnt);
	}

	return rc;
}

static int vreg_set_sleep_sel(struct vreg *vreg, enum rpm_vreg_voter voter,
			  int sleep, unsigned mask0, unsigned val0,
			  unsigned mask1, unsigned val1, unsigned cnt,
			  int update_voltage)
{
	unsigned int s_mask[2] = {mask0, mask1}, s_val[2] = {val0, val1};
	int rc;

	if (voter < 0 || voter >= RPM_VREG_VOTER_COUNT)
		return -EINVAL;

	mutex_lock(&rpm_sleep_sel_lock);

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
		} else if (vreg->part->mV.mask) {
			s_val[vreg->part->mV.word] = 0 << vreg->part->mV.shift;
			s_mask[vreg->part->mV.word] = vreg->part->mV.mask;
		} else if (vreg->part->enable_state.mask) {
			s_val[vreg->part->enable_state.word]
				= 0 << vreg->part->enable_state.shift;
			s_mask[vreg->part->enable_state.word]
				= vreg->part->enable_state.mask;
		}

		rc = vreg_send_request(vreg, voter, MSM_RPM_CTX_SET_SLEEP,
				       s_mask[0], s_val[0], s_mask[1], s_val[1],
				       cnt, update_voltage);
	}

	rc = vreg_send_request(vreg, voter, MSM_RPM_CTX_SET_0, mask0, val0,
					mask1, val1, cnt, update_voltage);

	mutex_unlock(&rpm_sleep_sel_lock);

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
 * using the regulator framework.  It is needed for consumers which wish to only
 * vote for active set regulator voltage.
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
			 int max_uV, int sleep_also)
{
	unsigned int mask[2] = {0}, val[2] = {0};
	struct vreg_range *range;
	struct vreg *vreg;
	int uV = min_uV;
	int lim_min_uV, lim_max_uV, i, rc;

	if (!config) {
		pr_err("rpm-regulator driver has not probed yet.\n");
		return -ENODEV;
	}

	if (vreg_id < config->vreg_id_min || vreg_id > config->vreg_id_max) {
		pr_err("invalid regulator id=%d\n", vreg_id);
		return -EINVAL;
	}

	vreg = &config->vregs[vreg_id];

	if (!vreg->pdata.sleep_selectable) {
		vreg_err(vreg, "regulator is not marked sleep selectable\n");
		return -EINVAL;
	}

	/* Allow min_uV == max_uV == 0 to represent a disable request. */
	if ((min_uV != 0 || max_uV != 0)
	    && (vreg->part->uV.mask || vreg->part->mV.mask)) {
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

		range = &vreg->set_points->range[0];
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

	if (vreg->type == RPM_REGULATOR_TYPE_CORNER) {
		/*
		 * Translate from enum values which work as inputs in the
		 * rpm_vreg_set_voltage function to the actual corner values
		 * sent to the RPM.
		 */
		if (uV > 0)
			uV -= RPM_VREG_CORNER_NONE;
	}

	if (vreg->part->uV.mask) {
		val[vreg->part->uV.word] = uV << vreg->part->uV.shift;
		mask[vreg->part->uV.word] = vreg->part->uV.mask;
	} else if (vreg->part->mV.mask) {
		val[vreg->part->mV.word]
			= MICRO_TO_MILLI(uV) << vreg->part->mV.shift;
		mask[vreg->part->mV.word] = vreg->part->mV.mask;
	} else if (vreg->part->enable_state.mask) {
		/*
		 * Translate max_uV > 0 into an enable request for regulator
		 * types which to not support voltage setting, e.g. voltage
		 * switches.
		 */
		val[vreg->part->enable_state.word]
		    = (max_uV > 0 ? 1 : 0) << vreg->part->enable_state.shift;
		mask[vreg->part->enable_state.word]
		    = vreg->part->enable_state.mask;
	}

	rc = vreg_set_sleep_sel(vreg, voter, sleep_also, mask[0], val[0],
				mask[1], val[1], vreg->part->request_len, 1);
	if (rc)
		vreg_err(vreg, "vreg_set_sleep_sel failed, rc=%d\n", rc);

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

	rc = vreg_set_sleep_sel(vreg, RPM_VREG_VOTER_REG_FRAMEWORK, 1, mask[0],
			   val[0], mask[1], val[1], vreg->part->request_len, 0);
	if (rc)
		vreg_err(vreg, "vreg_set_sleep_sel failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL_GPL(rpm_vreg_set_frequency);

#define MAX_NAME_LEN 64
/**
 * rpm_regulator_get() - lookup and obtain a handle to an RPM regulator
 * @dev: device for regulator consumer
 * @supply: supply name
 *
 * Returns a struct rpm_regulator corresponding to the regulator producer,
 * or ERR_PTR() containing errno.
 *
 * This function may only be called from nonatomic context.  The mapping between
 * <dev, supply> tuples and rpm_regulators struct pointers is specified via
 * rpm-regulator platform data.
 */
struct rpm_regulator *rpm_regulator_get(struct device *dev, const char *supply)
{
	struct rpm_regulator_consumer_mapping *mapping = NULL;
	const char *devname = NULL;
	struct rpm_regulator *regulator;
	int i;

	if (!config) {
		pr_err("rpm-regulator driver has not probed yet.\n");
		return ERR_PTR(-ENODEV);
	}

	if (consumer_map == NULL || consumer_map_len == 0) {
		pr_err("No private consumer mapping has been specified.\n");
		return ERR_PTR(-ENODEV);
	}

	if (supply == NULL) {
		pr_err("supply name must be specified\n");
		return ERR_PTR(-EINVAL);
	}

	if (dev)
		devname = dev_name(dev);

	for (i = 0; i < consumer_map_len; i++) {
		/* If the mapping has a device set up it must match */
		if (consumer_map[i].dev_name &&
			(!devname || strncmp(consumer_map[i].dev_name, devname,
					     MAX_NAME_LEN)))
			continue;

		if (strncmp(consumer_map[i].supply, supply, MAX_NAME_LEN)
		    == 0) {
			mapping = &consumer_map[i];
			break;
		}
	}

	if (mapping == NULL) {
		pr_err("could not find mapping for dev=%s, supply=%s\n",
			(devname ? devname : "(null)"), supply);
		return ERR_PTR(-ENODEV);
	}

	regulator = kzalloc(sizeof(struct rpm_regulator), GFP_KERNEL);
	if (regulator == NULL) {
		pr_err("could not allocate memory for regulator\n");
		return ERR_PTR(-ENOMEM);
	}

	regulator->vreg_id	= mapping->vreg_id;
	regulator->voter	= mapping->voter;
	regulator->sleep_also	= mapping->sleep_also;

	return regulator;
}
EXPORT_SYMBOL_GPL(rpm_regulator_get);

static int rpm_regulator_check_input(struct rpm_regulator *regulator)
{
	int rc = 0;

	if (regulator == NULL) {
		rc = -EINVAL;
		pr_err("invalid (null) rpm_regulator pointer\n");
	} else if (IS_ERR(regulator)) {
		rc = PTR_ERR(regulator);
		pr_err("invalid rpm_regulator pointer, rc=%d\n", rc);
	}

	return rc;
}

/**
 * rpm_regulator_put() - free the RPM regulator handle
 * @regulator: RPM regulator handle
 *
 * Parameter reaggregation does not take place when rpm_regulator_put is called.
 * Therefore, regulator enable state and voltage must be configured
 * appropriately before calling rpm_regulator_put.
 *
 * This function may be called from either atomic or nonatomic context.
 */
void rpm_regulator_put(struct rpm_regulator *regulator)
{
	kfree(regulator);
}
EXPORT_SYMBOL_GPL(rpm_regulator_put);

/**
 * rpm_regulator_enable() - enable regulator output
 * @regulator: RPM regulator handle
 *
 * Returns 0 on success or errno on failure.
 *
 * This function may be called from either atomic or nonatomic context.  This
 * function may only be called for regulators which have the sleep_selectable
 * flag set in their configuration data.
 *
 * rpm_regulator_set_voltage must be called before rpm_regulator_enable because
 * enabling is defined by the RPM interface to be requesting the desired
 * non-zero regulator output voltage.
 */
int rpm_regulator_enable(struct rpm_regulator *regulator)
{
	int rc = rpm_regulator_check_input(regulator);
	struct vreg *vreg;

	if (rc)
		return rc;

	if (regulator->vreg_id < config->vreg_id_min
			|| regulator->vreg_id > config->vreg_id_max) {
		pr_err("invalid regulator id=%d\n", regulator->vreg_id);
		return -EINVAL;
	}

	vreg = &config->vregs[regulator->vreg_id];

	/*
	 * Handle voltage switches which can be enabled without
	 * rpm_regulator_set_voltage ever being called.
	 */
	if (regulator->min_uV == 0 && regulator->max_uV == 0
	    && vreg->part->uV.mask == 0 && vreg->part->mV.mask == 0) {
		regulator->min_uV = 1;
		regulator->max_uV = 1;
	}

	if (regulator->min_uV == 0 && regulator->max_uV == 0) {
		pr_err("Voltage must be set with rpm_regulator_set_voltage "
			"before calling rpm_regulator_enable; vreg_id=%d, "
			"voter=%d\n", regulator->vreg_id, regulator->voter);
		return -EINVAL;
	}

	rc = rpm_vreg_set_voltage(regulator->vreg_id, regulator->voter,
		regulator->min_uV, regulator->max_uV, regulator->sleep_also);

	if (rc)
		pr_err("rpm_vreg_set_voltage failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL_GPL(rpm_regulator_enable);

/**
 * rpm_regulator_disable() - disable regulator output
 * @regulator: RPM regulator handle
 *
 * Returns 0 on success or errno on failure.
 *
 * The enable state of the regulator is determined by aggregating the requests
 * of all consumers.  Therefore, it is possible that the regulator will remain
 * enabled even after rpm_regulator_disable is called.
 *
 * This function may be called from either atomic or nonatomic context.  This
 * function may only be called for regulators which have the sleep_selectable
 * flag set in their configuration data.
 */
int rpm_regulator_disable(struct rpm_regulator *regulator)
{
	int rc = rpm_regulator_check_input(regulator);

	if (rc)
		return rc;

	rc = rpm_vreg_set_voltage(regulator->vreg_id, regulator->voter, 0, 0,
				  regulator->sleep_also);

	if (rc)
		pr_err("rpm_vreg_set_voltage failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL_GPL(rpm_regulator_disable);

/**
 * rpm_regulator_set_voltage() - set regulator output voltage
 * @regulator: RPM regulator handle
 * @min_uV: minimum required voltage in uV
 * @max_uV: maximum acceptable voltage in uV
 *
 * Sets a voltage regulator to the desired output voltage. This can be set
 * while the regulator is disabled or enabled.  If the regulator is disabled,
 * then rpm_regulator_set_voltage will both enable the regulator and set it to
 * output at the requested voltage.
 *
 * The min_uV to max_uV voltage range requested must intersect with the
 * voltage constraint range configured for the regulator.
 *
 * Returns 0 on success or errno on failure.
 *
 * The final voltage value that is sent to the RPM is aggregated based upon the
 * values requested by all consumers of the regulator.  This corresponds to the
 * maximum min_uV value.
 *
 * This function may be called from either atomic or nonatomic context.  This
 * function may only be called for regulators which have the sleep_selectable
 * flag set in their configuration data.
 */
int rpm_regulator_set_voltage(struct rpm_regulator *regulator, int min_uV,
			      int max_uV)
{
	int rc = rpm_regulator_check_input(regulator);

	if (rc)
		return rc;

	rc = rpm_vreg_set_voltage(regulator->vreg_id, regulator->voter, min_uV,
				 max_uV, regulator->sleep_also);

	if (rc) {
		pr_err("rpm_vreg_set_voltage failed, rc=%d\n", rc);
	} else {
		regulator->min_uV = min_uV;
		regulator->max_uV = max_uV;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(rpm_regulator_set_voltage);

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
	if (vreg->pdata.sleep_selectable)
		mutex_lock(&rpm_sleep_sel_lock);

	vreg->req[0].value &= ~mask0;
	vreg->req[0].value |= val0 & mask0;

	vreg->req[1].value &= ~mask1;
	vreg->req[1].value |= val1 & mask1;

	if (vreg->pdata.sleep_selectable)
		mutex_unlock(&rpm_sleep_sel_lock);

	return 0;
}

static int vreg_set(struct vreg *vreg, unsigned mask0, unsigned val0,
		unsigned mask1, unsigned val1, unsigned cnt)
{
	unsigned prev0 = 0, prev1 = 0;
	bool tcxo_enabled = false;
	bool voltage_increased = false;
	int rc;

	/*
	 * Bypass the normal route for regulators that can be called to change
	 * just the active set values.
	 */
	if (vreg->pdata.sleep_selectable)
		return vreg_set_sleep_sel(vreg, RPM_VREG_VOTER_REG_FRAMEWORK, 1,
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

	/* Enable CXO clock if necessary for TCXO workaround. */
	if (requires_tcxo_workaround && vreg->requires_cxo
	    && (GET_PART(vreg, uV) > GET_PART_PREV_ACT(vreg, uV))) {
		mutex_lock(&tcxo_mutex);
		if (!tcxo_handle)
			tcxo_get_handle();
		voltage_increased = true;
		tcxo_enabled = tcxo_enable();
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

	/*
	 * Schedule CXO clock to be disabled after TCXO warmup time if TCXO
	 * workaround is applicable for this regulator.
	 */
	if (voltage_increased) {
		if (tcxo_enabled)
			tcxo_delayed_disable();
		mutex_unlock(&tcxo_mutex);
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
	case RPM_REGULATOR_TYPE_CORNER:
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

static int rpm_vreg_enable(struct regulator_dev *rdev)
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
	case RPM_REGULATOR_TYPE_CORNER:
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

static int rpm_vreg_disable(struct regulator_dev *rdev)
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

	if (vreg->type == RPM_REGULATOR_TYPE_CORNER) {
		/*
		 * Translate from enum values which work as inputs in the
		 * regulator_set_voltage function to the actual corner values
		 * sent to the RPM.
		 */
		uV -= RPM_VREG_CORNER_NONE;
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
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
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
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
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
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= vreg_is_enabled,
	.enable_time		= vreg_enable_time,
};

static struct regulator_ops ncp_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= vreg_is_enabled,
	.set_voltage		= vreg_set_voltage,
	.get_voltage		= vreg_get_voltage,
	.list_voltage		= vreg_list_voltage,
	.enable_time		= vreg_enable_time,
};

static struct regulator_ops corner_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
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
	[RPM_REGULATOR_TYPE_CORNER]	= &corner_ops,
};

static struct vreg *rpm_vreg_get_vreg(int id)
{
	struct vreg *vreg;

	if (id < config->vreg_id_min || id > config->vreg_id_max)
		return NULL;

	if (!config->is_real_id(id))
		id = config->pc_id_to_real_id(id);
	vreg = &config->vregs[id];

	return vreg;
}

static int __devinit
rpm_vreg_init_regulator(const struct rpm_regulator_init_data *pdata,
			struct device *dev)
{
	struct regulator_desc *rdesc = NULL;
	struct regulator_dev *rdev;
	struct vreg *vreg;
	unsigned pin_ctrl;
	int pin_fn;
	int rc = 0;

	if (!pdata) {
		pr_err("platform data missing\n");
		return -EINVAL;
	}

	vreg = rpm_vreg_get_vreg(pdata->id);
	if (!vreg) {
		pr_err("invalid regulator id: %d\n", pdata->id);
		return -ENODEV;
	}

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

	rdev = regulator_register(rdesc, dev, &(pdata->init_data), vreg, NULL);
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
	static struct rpm_regulator_consumer_mapping *prev_consumer_map;
	static int prev_consumer_map_len;
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

	/* Copy the list of private API consumers. */
	if (platform_data->consumer_map_len > 0) {
		if (consumer_map_len == 0) {
			consumer_map_len = platform_data->consumer_map_len;
			consumer_map = kmemdup(platform_data->consumer_map,
				sizeof(struct rpm_regulator_consumer_mapping)
				* consumer_map_len, GFP_KERNEL);
			if (consumer_map == NULL) {
				pr_err("memory allocation failed\n");
				consumer_map_len = 0;
				return -ENOMEM;
			}
		} else {
			/* Concatenate new map with the existing one. */
			prev_consumer_map = consumer_map;
			prev_consumer_map_len = consumer_map_len;
			consumer_map_len += platform_data->consumer_map_len;
			consumer_map = kmalloc(
				sizeof(struct rpm_regulator_consumer_mapping)
				* consumer_map_len, GFP_KERNEL);
			if (consumer_map == NULL) {
				pr_err("memory allocation failed\n");
				consumer_map_len = 0;
				return -ENOMEM;
			}
			memcpy(consumer_map, prev_consumer_map,
				sizeof(struct rpm_regulator_consumer_mapping)
				* prev_consumer_map_len);
			memcpy(&consumer_map[prev_consumer_map_len],
				platform_data->consumer_map,
				sizeof(struct rpm_regulator_consumer_mapping)
				* platform_data->consumer_map_len);
		}

	}

	if (platform_data->requires_tcxo_workaround
	    && !requires_tcxo_workaround) {
		requires_tcxo_workaround = true;
		wake_lock_init(&tcxo_wake_lock, WAKE_LOCK_SUSPEND,
				"rpm_regulator_tcxo");
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

	kfree(consumer_map);

	for (i = 0; i < config->vregs_len; i++)
		mutex_destroy(&config->vregs[i].pc_lock);

	if (tcxo_handle)
		clk_put(tcxo_handle);
}

postcore_initcall(rpm_vreg_init);
module_exit(rpm_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM RPM regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" RPM_REGULATOR_DEV_NAME);
