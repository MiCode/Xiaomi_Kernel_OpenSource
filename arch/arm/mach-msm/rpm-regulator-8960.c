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

#include "rpm_resources.h"

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

#define REGULATOR_TYPE_LDO			0
#define REGULATOR_TYPE_SMPS			1
#define REGULATOR_TYPE_VS			2
#define REGULATOR_TYPE_NCP			3

#define MICRO_TO_MILLI(uV)			((uV) / 1000)
#define MILLI_TO_MICRO(mV)			((mV) * 1000)

#define SET_PART(_vreg, _part, _val) \
	_vreg->req[_vreg->part->_part.word].value \
		= (_vreg->req[_vreg->part->_part.word].value \
			& ~vreg->part->_part.mask) \
		| (((_val) << vreg->part->_part.shift) & vreg->part->_part.mask)

#define GET_PART(_vreg, _part) \
	((_vreg->req[_vreg->part->_part.word].value & vreg->part->_part.mask) \
		>> vreg->part->_part.shift)

struct request_member {
	int			word;
	unsigned int		mask;
	int			shift;
};

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
	struct request_member	hpm;		/* switch: control OCP ans SS */
	int			request_len;
};

#define REQUEST_MEMBER(_word, _mask, _shift) \
	{ \
		.word	= _word, \
		.mask	= _mask, \
		.shift	= _shift, \
	}

struct rpm_vreg_parts ldo_parts = {
	.request_len	= 2,
	.uV		= REQUEST_MEMBER(0, 0x007FFFFF,  0),
	.pd		= REQUEST_MEMBER(0, 0x00800000, 23),
	.fm		= REQUEST_MEMBER(0, 0x0F000000, 24),
	.pc		= REQUEST_MEMBER(0, 0xF0000000, 28),
	.ip		= REQUEST_MEMBER(1, 0x000003FF,  0),
	.pf		= REQUEST_MEMBER(1, 0x00003C00, 10),
	.ia		= REQUEST_MEMBER(1, 0x00FFC000, 14),
};

struct rpm_vreg_parts smps_parts = {
	.request_len	= 2,
	.uV		= REQUEST_MEMBER(0, 0x007FFFFF,  0),
	.pd		= REQUEST_MEMBER(0, 0x00800000, 23),
	.fm		= REQUEST_MEMBER(0, 0x0F000000, 24),
	.pc		= REQUEST_MEMBER(0, 0xF0000000, 28),
	.ip		= REQUEST_MEMBER(1, 0x000003FF,  0),
	.pf		= REQUEST_MEMBER(1, 0x00003C00, 10),
	.ia		= REQUEST_MEMBER(1, 0x00FFC000, 14),
	.freq		= REQUEST_MEMBER(1, 0x1F000000, 24),
	.freq_clk_src	= REQUEST_MEMBER(1, 0x60000000, 29),
	.pm		= REQUEST_MEMBER(1, 0x80000000, 31),
};

struct rpm_vreg_parts switch_parts = {
	.request_len	= 1,
	.enable_state	= REQUEST_MEMBER(0, 0x00000001,  0),
	.pd		= REQUEST_MEMBER(0, 0x00000002,  1),
	.pc		= REQUEST_MEMBER(0, 0x0000003C,  2),
	.pf		= REQUEST_MEMBER(0, 0x000003C0,  6),
	.hpm		= REQUEST_MEMBER(0, 0x00000C00, 10),
};

struct rpm_vreg_parts ncp_parts = {
	.request_len	= 1,
	.uV		= REQUEST_MEMBER(0, 0x007FFFFF,  0),
	.enable_state	= REQUEST_MEMBER(0, 0x00800000, 23),
	.comp_mode	= REQUEST_MEMBER(0, 0x01000000, 24),
	.freq		= REQUEST_MEMBER(0, 0x3E000000, 25),
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

#define VOLTAGE_RANGE(_min_uV, _max_uV, _step_uV) \
	{ \
		.min_uV  = _min_uV, \
		.max_uV  = _max_uV, \
		.step_uV = _step_uV, \
	}

static struct vreg_range pldo_ranges[] = {
	VOLTAGE_RANGE( 750000, 1487500, 12500),
	VOLTAGE_RANGE(1500000, 3075000, 25000),
	VOLTAGE_RANGE(3100000, 4900000, 50000),
};

static struct vreg_range nldo_ranges[] = {
	VOLTAGE_RANGE( 750000, 1537500, 12500),
};

static struct vreg_range nldo1200_ranges[] = {
	VOLTAGE_RANGE( 375000,  743750,  6250),
	VOLTAGE_RANGE( 750000, 1537500, 12500),
};

static struct vreg_range smps_ranges[] = {
	VOLTAGE_RANGE( 375000,  737500, 12500),
	VOLTAGE_RANGE( 750000, 1487500, 12500),
	VOLTAGE_RANGE(1500000, 3075000, 25000),
};

static struct vreg_range ftsmps_ranges[] = {
	VOLTAGE_RANGE( 350000,  650000, 50000),
	VOLTAGE_RANGE( 700000, 1400000, 12500),
	VOLTAGE_RANGE(1500000, 3300000, 50000),
};

static struct vreg_range ncp_ranges[] = {
	VOLTAGE_RANGE(1500000, 3050000, 50000),
};

#define SET_POINTS(_ranges) \
{ \
	.range	= _ranges, \
	.count	= ARRAY_SIZE(_ranges), \
};

static struct vreg_set_points pldo_set_points = SET_POINTS(pldo_ranges);
static struct vreg_set_points nldo_set_points = SET_POINTS(nldo_ranges);
static struct vreg_set_points nldo1200_set_points = SET_POINTS(nldo1200_ranges);
static struct vreg_set_points smps_set_points = SET_POINTS(smps_ranges);
static struct vreg_set_points ftsmps_set_points = SET_POINTS(ftsmps_ranges);
static struct vreg_set_points ncp_set_points = SET_POINTS(ncp_ranges);

/*
 * This is used when voting for LPM or HPM by subtracting or adding to the
 * hpm_min_load of a regulator.  It has units of uA.
 */
#define LOAD_THRESHOLD_STEP			1000

/* This is the maximum uA load that can be passed to the RPM. */
#define MAX_POSSIBLE_LOAD			(MILLI_TO_MICRO(0xFFF))

struct vreg {
	struct msm_rpm_iv_pair		req[2];
	struct msm_rpm_iv_pair		prev_active_req[2];
	struct msm_rpm_iv_pair		prev_sleep_req[2];
	struct rpm_regulator_init_data	pdata;
	struct regulator_dev		*rdev;
	struct regulator_dev		*rdev_pc;
	const char			*name;
	struct vreg_set_points		*set_points;
	struct rpm_vreg_parts		*part;
	int				type;
	enum rpm_vreg_id		id;
	struct mutex			pc_lock;
	int				save_uV;
	int				mode;
	bool				is_enabled;
	bool				is_enabled_pc;
	const int			hpm_min_load;
	int			       active_min_uV_vote[RPM_VREG_VOTER_COUNT];
	int				sleep_min_uV_vote[RPM_VREG_VOTER_COUNT];

};

#define LDO(_id, _ranges, _hpm_min_load) \
	[RPM_VREG_ID_PM8921_##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_PM8921_##_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_PM8921_##_id##_1, }, \
		}, \
		.hpm_min_load	 = RPM_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
		.type		 = REGULATOR_TYPE_LDO, \
		.set_points	 = &_ranges##_set_points, \
		.part		 = &ldo_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_id, \
	}

#define SMPS(_id, _ranges, _hpm_min_load) \
	[RPM_VREG_ID_PM8921_##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_PM8921_##_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_PM8921_##_id##_1, }, \
		}, \
		.hpm_min_load	 = RPM_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
		.type		 = REGULATOR_TYPE_SMPS, \
		.set_points	 = &_ranges##_set_points, \
		.part		 = &smps_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_id, \
	}

#define LVS(_id) \
	[RPM_VREG_ID_PM8921_##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_PM8921_##_id, }, \
			[1] = { .id = -1, }, \
		}, \
		.type		 = REGULATOR_TYPE_VS, \
		.part		 = &switch_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_id, \
	}

#define MVS(_vreg_id, _rpm_id) \
	[RPM_VREG_ID_PM8921_##_vreg_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id, }, \
			[1] = { .id = -1, }, \
		}, \
		.type		 = REGULATOR_TYPE_VS, \
		.part		 = &switch_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_vreg_id, \
	}

#define NCP(_id) \
	[RPM_VREG_ID_PM8921_##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_##_id##_1, }, \
		}, \
		.type		 = REGULATOR_TYPE_NCP, \
		.set_points	 = &ncp_set_points, \
		.part		 = &ncp_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_id, \
	}

static struct vreg vregs[] = {
	LDO(L1,  nldo,     LDO_150),
	LDO(L2,  nldo,     LDO_150),
	LDO(L3 , pldo,     LDO_150),
	LDO(L4,  pldo,     LDO_50),
	LDO(L5,  pldo,     LDO_300),
	LDO(L6,  pldo,     LDO_600),
	LDO(L7,  pldo,     LDO_150),
	LDO(L8,  pldo,     LDO_300),
	LDO(L9,  pldo,     LDO_300),
	LDO(L10, pldo,     LDO_600),
	LDO(L11, pldo,     LDO_150),
	LDO(L12, nldo,     LDO_150),
	LDO(L14, pldo,     LDO_50),
	LDO(L15, pldo,     LDO_150),
	LDO(L16, pldo,     LDO_300),
	LDO(L17, pldo,     LDO_150),
	LDO(L18, nldo,     LDO_150),
	LDO(L21, pldo,     LDO_150),
	LDO(L22, pldo,     LDO_150),
	LDO(L23, pldo,     LDO_150),
	LDO(L24, nldo1200, LDO_1200),
	LDO(L25, nldo1200, LDO_1200),
	LDO(L26, nldo1200, LDO_1200),
	LDO(L27, nldo1200, LDO_1200),
	LDO(L28, nldo1200, LDO_1200),
	LDO(L29, pldo,     LDO_150),

	SMPS(S1, smps,     SMPS_1500),
	SMPS(S2, smps,     SMPS_1500),
	SMPS(S3, smps,     SMPS_1500),
	SMPS(S4, smps,     SMPS_1500),
	SMPS(S5, ftsmps,   SMPS_2000),
	SMPS(S6, ftsmps,   SMPS_2000),
	SMPS(S7, smps,     SMPS_1500),
	SMPS(S8, smps,     SMPS_1500),

	LVS(LVS1),
	LVS(LVS2),
	LVS(LVS3),
	LVS(LVS4),
	LVS(LVS5),
	LVS(LVS6),
	LVS(LVS7),
	MVS(USB_OTG, USB_OTG_SWITCH),
	MVS(HDMI_MVS, HDMI_SWITCH),

	NCP(NCP),
};

#define vreg_err(vreg, fmt, ...) \
	pr_err("%s: " fmt, vreg->name, ##__VA_ARGS__)

#define VREG_ID_IS_VDD_MEM_OR_DIG(id) \
	((id == RPM_VREG_ID_PM8921_L24) || (id == RPM_VREG_ID_PM8921_S3))

const char *pin_func_label[] = {
	[RPM_VREG_PIN_FN_DONT_CARE]		= "don't care",
	[RPM_VREG_PIN_FN_ENABLE]		= "on/off",
	[RPM_VREG_PIN_FN_MODE]			= "HPM/LPM",
	[RPM_VREG_PIN_FN_SLEEP_B]		= "sleep_b",
	[RPM_VREG_PIN_FN_NONE]			= "none",
};

const char *force_mode_label[] = {
	[RPM_VREG_FORCE_MODE_NONE]		= "none",
	[RPM_VREG_FORCE_MODE_LPM]		= "LPM",
	[RPM_VREG_FORCE_MODE_AUTO]		= "auto",
	[RPM_VREG_FORCE_MODE_HPM]		= "HPM",
	[RPM_VREG_FORCE_MODE_BYPASS]		= "BYP",
};

const char *power_mode_label[] = {
	[RPM_VREG_POWER_MODE_HYSTERETIC]	= "HYS",
	[RPM_VREG_POWER_MODE_PWM]		= "PWM",
};

static void rpm_regulator_req(struct vreg *vreg, int set)
{
	int uV, ip, fm, pm, pc, pf, pd, ia, freq, clk, state, hpm, comp_mode;
	const char *pf_label = "", *fm_label = "", *pc_total = "";
	const char *pc_en0 = "", *pc_en1 = "", *pc_en2 = "", *pc_en3 = "";
	const char *pm_label = "";

	/* Suppress VDD_MEM and VDD_DIG printing. */
	if ((msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_IGNORE_VDD_MEM_DIG)
	    && VREG_ID_IS_VDD_MEM_OR_DIG(vreg->id))
		return;

	if (vreg->part->uV.mask)
		uV = GET_PART(vreg, uV);
	else
		uV = MILLI_TO_MICRO(GET_PART(vreg, mV));

	ip = GET_PART(vreg, ip);
	fm = GET_PART(vreg, fm);
	pm = GET_PART(vreg, pm);
	pc = GET_PART(vreg, pc);
	pf = GET_PART(vreg, pf);
	pd = GET_PART(vreg, pd);
	ia = GET_PART(vreg, ia);
	freq = GET_PART(vreg, freq);
	clk = GET_PART(vreg, freq_clk_src);
	state = GET_PART(vreg, enable_state);
	hpm = GET_PART(vreg, hpm);
	comp_mode = GET_PART(vreg, comp_mode);

	if (pf >= 0 && pf < ARRAY_SIZE(pin_func_label))
		pf_label = pin_func_label[pf];

	if (fm >= 0 && fm < ARRAY_SIZE(force_mode_label))
		fm_label = force_mode_label[fm];

	if (pm >= 0 && pm < ARRAY_SIZE(power_mode_label))
		pm_label = power_mode_label[pm];

	if (pc & RPM_VREG_PIN_CTRL_EN0)
		pc_en0 = " D1";
	if (pc & RPM_VREG_PIN_CTRL_EN1)
		pc_en1 = " A0";
	if (pc & RPM_VREG_PIN_CTRL_EN2)
		pc_en2 = " A1";
	if (pc & RPM_VREG_PIN_CTRL_EN3)
		pc_en3 = " A2";
	if (pc == RPM_VREG_PIN_CTRL_NONE)
		pc_total = " none";

	switch (vreg->type) {
	case REGULATOR_TYPE_LDO:
		pr_info("%s %-9s: s=%c, v=%7d uV, ip=%4d mA, fm=%s (%d), "
			"pc=%s%s%s%s%s (%d), pf=%s (%d), pd=%s (%d), "
			"ia=%4d mA; req[0]={%d, 0x%08X}, req[1]={%d, 0x%08X}\n",
			(set == MSM_RPM_CTX_SET_0 ? "sending " : "buffered"),
			vreg->name,
			(set == MSM_RPM_CTX_SET_0 ? 'A' : 'S'), uV, ip,
			fm_label, fm, pc_en0, pc_en1, pc_en2, pc_en3, pc_total,
			pc, pf_label, pf, (pd == 1 ? "Y" : "N"), pd, ia,
			vreg->req[0].id, vreg->req[0].value,
			vreg->req[1].id, vreg->req[1].value);
		break;
	case REGULATOR_TYPE_SMPS:
		pr_info("%s %-9s: s=%c, v=%7d uV, ip=%4d mA, fm=%s (%d), "
			"pc=%s%s%s%s%s (%d), pf=%s (%d), pd=%s (%d), "
			"ia=%4d mA, freq=%2d, pm=%s (%d), clk_src=%d; "
			"req[0]={%d, 0x%08X}, req[1]={%d, 0x%08X}\n",
			(set == MSM_RPM_CTX_SET_0 ? "sending " : "buffered"),
			vreg->name,
			(set == MSM_RPM_CTX_SET_0 ? 'A' : 'S'), uV, ip,
			fm_label, fm, pc_en0, pc_en1, pc_en2, pc_en3, pc_total,
			pc, pf_label, pf, (pd == 1 ? "Y" : "N"), pd, ia, freq,
			pm_label, pm, clk, vreg->req[0].id, vreg->req[0].value,
			vreg->req[1].id, vreg->req[1].value);
		break;
	case REGULATOR_TYPE_VS:
		pr_info("%s %-9s: s=%c, state=%s (%d), pd=%s (%d), "
			"pc =%s%s%s%s%s (%d), pf=%s (%d), hpm=%d; "
			"req[0]={%d, 0x%08X}\n",
			(set == MSM_RPM_CTX_SET_0 ? "sending " : "buffered"),
			vreg->name, (set == MSM_RPM_CTX_SET_0 ? 'A' : 'S'),
			(state == 1 ? "on" : "off"), state,
			(pd == 1 ? "Y" : "N"), pd, pc_en0, pc_en1, pc_en2,
			pc_en3, pc_total, pc, pf_label, pf, hpm,
			vreg->req[0].id, vreg->req[0].value);
		break;
	case REGULATOR_TYPE_NCP:
		pr_info("%s %-9s: s=%c, v=-%7d uV, state=%s (%d), freq=%2d, "
			"comp=%d; req[0]={%d, 0x%08X}\n",
			(set == MSM_RPM_CTX_SET_0 ? "sending " : "buffered"),
			vreg->name, (set == MSM_RPM_CTX_SET_0 ? 'A' : 'S'),
			uV, (state == 1 ? "on" : "off"), state, freq, comp_mode,
			vreg->req[0].id, vreg->req[0].value);
		break;
	}
}

static void rpm_regulator_vote(struct vreg *vreg, enum rpm_vreg_voter voter,
			int set, int voter_uV, int aggregate_uV)
{
	/* Suppress VDD_MEM and VDD_DIG printing. */
	if ((msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_IGNORE_VDD_MEM_DIG)
	    && VREG_ID_IS_VDD_MEM_OR_DIG(vreg->id))
		return;

	pr_info("vote received %-9s: voter=%d, set=%c, v_voter=%7d uV, "
		"v_aggregate=%7d uV\n", vreg->name, voter,
		(set == 0 ? 'A' : 'S'), voter_uV, aggregate_uV);
}

static void rpm_regulator_duplicate(struct vreg *vreg, int set, int cnt)
{
	/* Suppress VDD_MEM and VDD_DIG printing. */
	if ((msm_rpm_vreg_debug_mask & MSM_RPM_VREG_DEBUG_IGNORE_VDD_MEM_DIG)
	    && VREG_ID_IS_VDD_MEM_OR_DIG(vreg->id))
		return;

	if (cnt == 2)
		pr_info("ignored request %-9s: set=%c; req[0]={%d, 0x%08X}, "
			"req[1]={%d, 0x%08X}\n", vreg->name,
			(set == 0 ? 'A' : 'S'),
			vreg->req[0].id, vreg->req[0].value,
			vreg->req[1].id, vreg->req[1].value);
	else if (cnt == 1)
		pr_info("ignored request %-9s: set=%c; req[0]={%d, 0x%08X}\n",
			vreg->name, (set == 0 ? 'A' : 'S'),
			vreg->req[0].id, vreg->req[0].value);
}

/* Spin lock needed for sleep-selectable regulators. */
static DEFINE_SPINLOCK(pm8921_noirq_lock);

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

	spin_lock_irqsave(&pm8921_noirq_lock, flags);

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

	spin_unlock_irqrestore(&pm8921_noirq_lock, flags);

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
	unsigned int mask[2] = {0}, val[2] = {0};
	struct vreg_range *range;
	struct vreg *vreg;
	int uV = min_uV;
	int lim_min_uV, lim_max_uV, i, rc;

	/*
	 * TODO: make this function a no-op so that it can be called by
	 * consumers before RPM capabilities are present. (needed for
	 * acpuclock driver)
	 */
	return 0;

	if (vreg_id < 0 || vreg_id > RPM_VREG_ID_PM8921_MAX_REAL) {
		pr_err("invalid regulator id=%d\n", vreg_id);
		return -EINVAL;
	}
	vreg = &vregs[vreg_id];
	range = &vreg->set_points->range[0];

	if (!vreg->pdata.sleep_selectable) {
		vreg_err(vreg, "regulator is not marked sleep selectable\n");
		return -EINVAL;
	}

	/*
	 * Check if request voltage is outside of allowed range. The regulator
	 * core has already checked that constraint range is inside of the
	 * physically allowed range.
	 */
	lim_min_uV = vreg->pdata.init_data.constraints.min_uV;
	lim_max_uV = vreg->pdata.init_data.constraints.max_uV;

	if (uV < lim_min_uV && max_uV >= lim_min_uV)
		uV = lim_min_uV;

	if (uV < lim_min_uV || uV > lim_max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside allowed v=[%d, %d]\n",
			 min_uV, max_uV, lim_min_uV, lim_max_uV);
		return -EINVAL;
	}

	/* Find the range which uV is inside of. */
	for (i = vreg->set_points->count - 1; i > 0; i++) {
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
int rpm_vreg_set_frequency(enum rpm_vreg_id vreg_id, enum rpm_vreg_freq freq)
{
	unsigned int mask[2] = {0}, val[2] = {0};
	struct vreg *vreg;
	int rc;

	/*
	 * TODO: make this function a no-op so that it can be called by
	 * consumers before RPM capabilities are present. (needed for
	 * acpuclock driver)
	 */
	return 0;

	if (vreg_id < 0 || vreg_id > RPM_VREG_ID_PM8921_MAX_REAL) {
		pr_err("invalid regulator id=%d\n", vreg_id);
		return -EINVAL;
	}
	vreg = &vregs[vreg_id];

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
		spin_lock_irqsave(&pm8921_noirq_lock, flags);

	vreg->req[0].value &= ~mask0;
	vreg->req[0].value |= val0 & mask0;

	vreg->req[1].value &= ~mask1;
	vreg->req[1].value |= val1 & mask1;

	if (vreg->pdata.sleep_selectable)
		spin_unlock_irqrestore(&pm8921_noirq_lock, flags);

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
	case REGULATOR_TYPE_LDO:
	case REGULATOR_TYPE_SMPS:
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
	case REGULATOR_TYPE_VS:
	case REGULATOR_TYPE_NCP:
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
	case REGULATOR_TYPE_LDO:
	case REGULATOR_TYPE_SMPS:
		/* Disable by setting a voltage of 0 uV. */
		if (vreg->part->uV.mask) {
			val[vreg->part->uV.word] |= 0 << vreg->part->uV.shift;
			mask[vreg->part->uV.word] |= vreg->part->uV.mask;
		} else {
			val[vreg->part->mV.word] |= 0 << vreg->part->mV.shift;
			mask[vreg->part->mV.word] |= vreg->part->mV.mask;
		}
		break;
	case REGULATOR_TYPE_VS:
	case REGULATOR_TYPE_NCP:
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
	for (i = vreg->set_points->count - 1; i > 0; i++) {
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
	} else if (vreg->type == REGULATOR_TYPE_NCP) {
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

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		/* Make sure that request currents are in HPM range. */
		if (peak_uA < vreg_hpm_min_uA(vreg)) {
			val[vreg->part->ip.word]
				= MICRO_TO_MILLI(vreg_hpm_min_uA(vreg))
					<< vreg->part->ip.shift;
			mask[vreg->part->ip.word] = vreg->part->ip.mask;
		}
		break;
	case REGULATOR_MODE_IDLE:
		/* Make sure that request currents are in LPM range. */
		if (peak_uA > vreg_lpm_max_uA(vreg)) {
			val[vreg->part->ip.word]
				= MICRO_TO_MILLI(vreg_lpm_max_uA(vreg))
					<< vreg->part->ip.shift;
			mask[vreg->part->ip.word] = vreg->part->ip.mask;
		}
		break;
	default:
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
	mutex_unlock(&vreg->pc_lock);

	if (load_uA >= vreg->hpm_min_load)
		mode = REGULATOR_MODE_NORMAL;
	else
		mode = REGULATOR_MODE_IDLE;

	return mode;
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
	enum rpm_vreg_pin_fn pin_fn;
	int rc;

	mutex_lock(&vreg->pc_lock);

	val[vreg->part->pc.word]
		|= RPM_VREG_PIN_CTRL_NONE << vreg->part->pc.shift;
	mask[vreg->part->pc.word] |= vreg->part->pc.mask;

	pin_fn = RPM_VREG_PIN_FN_NONE;
	if (vreg->pdata.pin_fn == RPM_VREG_PIN_FN_SLEEP_B)
		pin_fn = RPM_VREG_PIN_FN_SLEEP_B;
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
};

static struct regulator_ops switch_ops = {
	.enable			= vreg_enable,
	.disable		= vreg_disable,
	.is_enabled		= vreg_is_enabled,
};

static struct regulator_ops ncp_ops = {
	.enable			= vreg_enable,
	.disable		= vreg_disable,
	.is_enabled		= vreg_is_enabled,
	.set_voltage		= vreg_set_voltage,
	.get_voltage		= vreg_get_voltage,
	.list_voltage		= vreg_list_voltage,
};

/* Pin control regulator operations. */
static struct regulator_ops pin_control_ops = {
	.enable			= vreg_pin_control_enable,
	.disable		= vreg_pin_control_disable,
	.is_enabled		= vreg_pin_control_is_enabled,
};

#define VREG_DESC(_id, _name, _ops) \
	[RPM_VREG_ID_PM8921_##_id] = { \
		.id	= RPM_VREG_ID_PM8921_##_id, \
		.name	= _name, \
		.ops	= _ops, \
		.type	= REGULATOR_VOLTAGE, \
		.owner	= THIS_MODULE, \
	}

static struct regulator_desc vreg_description[] = {
	VREG_DESC(L1,  "8921_l1",  &ldo_ops),
	VREG_DESC(L2,  "8921_l2",  &ldo_ops),
	VREG_DESC(L3,  "8921_l3",  &ldo_ops),
	VREG_DESC(L4,  "8921_l4",  &ldo_ops),
	VREG_DESC(L5,  "8921_l5",  &ldo_ops),
	VREG_DESC(L6,  "8921_l6",  &ldo_ops),
	VREG_DESC(L7,  "8921_l7",  &ldo_ops),
	VREG_DESC(L8,  "8921_l8",  &ldo_ops),
	VREG_DESC(L9,  "8921_l9",  &ldo_ops),
	VREG_DESC(L10, "8921_l10", &ldo_ops),
	VREG_DESC(L11, "8921_l11", &ldo_ops),
	VREG_DESC(L12, "8921_l12", &ldo_ops),
	VREG_DESC(L14, "8921_l14", &ldo_ops),
	VREG_DESC(L15, "8921_l15", &ldo_ops),
	VREG_DESC(L16, "8921_l16", &ldo_ops),
	VREG_DESC(L17, "8921_l17", &ldo_ops),
	VREG_DESC(L18, "8921_l18", &ldo_ops),
	VREG_DESC(L21, "8921_l21", &ldo_ops),
	VREG_DESC(L22, "8921_l22", &ldo_ops),
	VREG_DESC(L23, "8921_l23", &ldo_ops),
	VREG_DESC(L24, "8921_l24", &ldo_ops),
	VREG_DESC(L25, "8921_l25", &ldo_ops),
	VREG_DESC(L26, "8921_l26", &ldo_ops),
	VREG_DESC(L27, "8921_l27", &ldo_ops),
	VREG_DESC(L28, "8921_l28", &ldo_ops),
	VREG_DESC(L29, "8921_l29", &ldo_ops),

	VREG_DESC(S1, "8921_s1", &smps_ops),
	VREG_DESC(S2, "8921_s2", &smps_ops),
	VREG_DESC(S3, "8921_s3", &smps_ops),
	VREG_DESC(S4, "8921_s4", &smps_ops),
	VREG_DESC(S5, "8921_s5", &smps_ops),
	VREG_DESC(S6, "8921_s6", &smps_ops),
	VREG_DESC(S7, "8921_s7", &smps_ops),
	VREG_DESC(S8, "8921_s8", &smps_ops),

	VREG_DESC(LVS1, "8921_lvs1", &switch_ops),
	VREG_DESC(LVS2, "8921_lvs2", &switch_ops),
	VREG_DESC(LVS3, "8921_lvs3", &switch_ops),
	VREG_DESC(LVS4, "8921_lvs4", &switch_ops),
	VREG_DESC(LVS5, "8921_lvs5", &switch_ops),
	VREG_DESC(LVS6, "8921_lvs6", &switch_ops),
	VREG_DESC(LVS7, "8921_lvs7", &switch_ops),

	VREG_DESC(USB_OTG, "8921_usb_otg", &switch_ops),
	VREG_DESC(HDMI_MVS, "8921_hdmi_mvs", &switch_ops),
	VREG_DESC(NCP, "8921_ncp", &ncp_ops),

	VREG_DESC(L1_PC,  "8921_l1_pc",  &pin_control_ops),
	VREG_DESC(L2_PC,  "8921_l2_pc",  &pin_control_ops),
	VREG_DESC(L3_PC,  "8921_l3_pc",  &pin_control_ops),
	VREG_DESC(L4_PC,  "8921_l4_pc",  &pin_control_ops),
	VREG_DESC(L5_PC,  "8921_l5_pc",  &pin_control_ops),
	VREG_DESC(L6_PC,  "8921_l6_pc",  &pin_control_ops),
	VREG_DESC(L7_PC,  "8921_l7_pc",  &pin_control_ops),
	VREG_DESC(L8_PC,  "8921_l8_pc",  &pin_control_ops),
	VREG_DESC(L9_PC,  "8921_l9_pc",  &pin_control_ops),
	VREG_DESC(L10_PC, "8921_l10_pc", &pin_control_ops),
	VREG_DESC(L11_PC, "8921_l11_pc", &pin_control_ops),
	VREG_DESC(L12_PC, "8921_l12_pc", &pin_control_ops),
	VREG_DESC(L14_PC, "8921_l14_pc", &pin_control_ops),
	VREG_DESC(L15_PC, "8921_l15_pc", &pin_control_ops),
	VREG_DESC(L16_PC, "8921_l16_pc", &pin_control_ops),
	VREG_DESC(L17_PC, "8921_l17_pc", &pin_control_ops),
	VREG_DESC(L18_PC, "8921_l18_pc", &pin_control_ops),
	VREG_DESC(L21_PC, "8921_l21_pc", &pin_control_ops),
	VREG_DESC(L22_PC, "8921_l22_pc", &pin_control_ops),
	VREG_DESC(L23_PC, "8921_l23_pc", &pin_control_ops),
	VREG_DESC(L29_PC, "8921_l29_pc", &pin_control_ops),

	VREG_DESC(S1_PC, "8921_s1_pc", &pin_control_ops),
	VREG_DESC(S2_PC, "8921_s2_pc", &pin_control_ops),
	VREG_DESC(S3_PC, "8921_s3_pc", &pin_control_ops),
	VREG_DESC(S4_PC, "8921_s4_pc", &pin_control_ops),
	VREG_DESC(S7_PC, "8921_s7_pc", &pin_control_ops),
	VREG_DESC(S8_PC, "8921_s8_pc", &pin_control_ops),

	VREG_DESC(LVS1_PC, "8921_lvs1_pc", &pin_control_ops),
	VREG_DESC(LVS3_PC, "8921_lvs3_pc", &pin_control_ops),
	VREG_DESC(LVS4_PC, "8921_lvs4_pc", &pin_control_ops),
	VREG_DESC(LVS5_PC, "8921_lvs5_pc", &pin_control_ops),
	VREG_DESC(LVS6_PC, "8921_lvs6_pc", &pin_control_ops),
	VREG_DESC(LVS7_PC, "8921_lvs7_pc", &pin_control_ops),
};

static inline int is_real_regulator(int id)
{
	return (id >= 0) && (id <= RPM_VREG_ID_PM8921_MAX_REAL);
}

static int pc_id_to_real_id(int id)
{
	int real_id;

	if (id >= RPM_VREG_ID_PM8921_L1_PC && id <= RPM_VREG_ID_PM8921_L23_PC)
		real_id = id - RPM_VREG_ID_PM8921_L1_PC;
	else if (id >= RPM_VREG_ID_PM8921_L29_PC
			&& id <= RPM_VREG_ID_PM8921_S4_PC)
		real_id = id - RPM_VREG_ID_PM8921_L29_PC
				+ RPM_VREG_ID_PM8921_L29;
	else if (id >= RPM_VREG_ID_PM8921_S7_PC
			&& id <= RPM_VREG_ID_PM8921_LVS1_PC)
		real_id = id - RPM_VREG_ID_PM8921_S7_PC + RPM_VREG_ID_PM8921_S7;
	else
		real_id = id - RPM_VREG_ID_PM8921_LVS3_PC
				+ RPM_VREG_ID_PM8921_LVS3;

	return real_id;
}

static int __devinit
rpm_vreg_init_regulator(const struct rpm_regulator_init_data *pdata,
			struct device *dev)
{
	enum rpm_vreg_pin_fn pin_fn;
	struct regulator_desc *rdesc;
	struct regulator_dev *rdev;
	struct vreg *vreg;
	const char *reg_name = "";
	unsigned pin_ctrl;
	int rc = 0, id = pdata->id;

	if (id < 0 || id > RPM_VREG_ID_PM8921_MAX) {
		pr_err("invalid regulator id: %d\n", id);
		return -ENODEV;
	}

	rdesc = &vreg_description[pdata->id];
	if (!is_real_regulator(pdata->id))
		id = pc_id_to_real_id(pdata->id);
	vreg = &vregs[id];
	reg_name = vreg_description[pdata->id].name;
	if (!pdata) {
		pr_err("%s: requires platform data\n", reg_name);
		return -EINVAL;
	}
	if (vreg->set_points)
		rdesc->n_voltages = vreg->set_points->n_voltages;
	else
		rdesc->n_voltages = 0;

	mutex_lock(&vreg->pc_lock);

	if (is_real_regulator(pdata->id)) {
		/* Do not modify pin control and pin function values. */
		pin_ctrl = vreg->pdata.pin_ctrl;
		pin_fn = vreg->pdata.pin_fn;
		memcpy(&(vreg->pdata), pdata,
			sizeof(struct rpm_regulator_init_data));
		vreg->pdata.pin_ctrl = pin_ctrl;
		vreg->pdata.pin_fn = pin_fn;
		vreg->name = reg_name;

		vreg->save_uV = vreg->pdata.default_uV;
		if (vreg->pdata.peak_uA >= vreg->hpm_min_load)
			vreg->mode = REGULATOR_MODE_NORMAL;
		else
			vreg->mode = REGULATOR_MODE_IDLE;

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
			SET_PART(vreg, pf, RPM_VREG_PIN_FN_NONE);
			SET_PART(vreg, pc, RPM_VREG_PIN_CTRL_NONE);
		}
	} else {
		/* Pin control regulator */
		if ((pdata->pin_ctrl & RPM_VREG_PIN_CTRL_ALL)
		      == RPM_VREG_PIN_CTRL_NONE
		    && pdata->pin_fn != RPM_VREG_PIN_FN_SLEEP_B) {
			pr_err("%s: no pin control input specified\n",
				reg_name);
			mutex_unlock(&vreg->pc_lock);
			return -EINVAL;
		}
		vreg->pdata.pin_ctrl = pdata->pin_ctrl;
		vreg->pdata.pin_fn = pdata->pin_fn;
		if (!vreg->name)
			vreg->name = reg_name;

		/* Initialize the RPM request. */
		pin_fn = RPM_VREG_PIN_FN_NONE;
		/* Allow pf=sleep_b to be specified by platform data. */
		if (vreg->pdata.pin_fn == RPM_VREG_PIN_FN_SLEEP_B)
			pin_fn = RPM_VREG_PIN_FN_SLEEP_B;
		SET_PART(vreg, pf, pin_fn);
		SET_PART(vreg, pc, RPM_VREG_PIN_CTRL_NONE);
	}

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		goto bail;

	rdev = regulator_register(rdesc, dev, &(pdata->init_data), vreg);
	if (IS_ERR(rdev)) {
		rc = PTR_ERR(rdev);
		pr_err("regulator_register failed: %s, rc=%d\n", reg_name, rc);
		return rc;
	} else {
		if (is_real_regulator(pdata->id))
			vreg->rdev = rdev;
		else
			vreg->rdev_pc = rdev;
	}

bail:
	if (rc)
		pr_err("error for %s, rc=%d\n", reg_name, rc);

	return rc;
}

static int __devinit rpm_vreg_probe(struct platform_device *pdev)
{
	struct rpm_regulator_platform_data *platform_data;
	int rc = 0;
	int i;

	platform_data = pdev->dev.platform_data;
	if (!platform_data) {
		pr_err("rpm-regulator requires platform data\n");
		return -EINVAL;
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
		if (is_real_regulator(platform_data->init_data[i].id))
			regulator_unregister(vregs[i].rdev);
		else
			regulator_unregister(
				vregs[pc_id_to_real_id(i)].rdev_pc);
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
			if (is_real_regulator(id)) {
				regulator_unregister(vregs[id].rdev);
				vregs[id].rdev = NULL;
			} else {
				regulator_unregister(
					vregs[pc_id_to_real_id(id)].rdev_pc);
				vregs[id].rdev_pc = NULL;
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
	struct vreg_set_points *set_points[] = {
		&pldo_set_points,
		&nldo_set_points,
		&nldo1200_set_points,
		&smps_set_points,
		&ftsmps_set_points,
		&ncp_set_points,
	};
	int i, j;

	/* Calculate the number of set points available for each regualtor. */
	for (i = 0; i < ARRAY_SIZE(set_points); i++) {
		for (j = 0; j < set_points[i]->count; j++) {
			set_points[i]->range[j].n_voltages
				= (set_points[i]->range[j].max_uV
					- set_points[i]->range[j].min_uV)
				   / set_points[i]->range[j].step_uV + 1;
			set_points[i]->n_voltages
				+= set_points[i]->range[j].n_voltages;
		}
	}

	/* Initialize pin control mutexes */
	for (i = 0; i < ARRAY_SIZE(vregs); i++)
		mutex_init(&vregs[i].pc_lock);

	return platform_driver_register(&rpm_vreg_driver);
}

static void __exit rpm_vreg_exit(void)
{
	int i;

	platform_driver_unregister(&rpm_vreg_driver);

	for (i = 0; i < ARRAY_SIZE(vregs); i++)
		mutex_destroy(&vregs[i].pc_lock);
}

postcore_initcall(rpm_vreg_init);
module_exit(rpm_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM8960 rpm regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" RPM_REGULATOR_DEV_NAME);
