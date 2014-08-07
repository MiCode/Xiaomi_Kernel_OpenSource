/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <soc/qcom/rpm-smd.h>

/* Debug Definitions */

enum {
	RPM_VREG_DEBUG_REQUEST		= BIT(0),
	RPM_VREG_DEBUG_FULL_REQUEST	= BIT(1),
	RPM_VREG_DEBUG_DUPLICATE	= BIT(2),
};

static int rpm_vreg_debug_mask;
module_param_named(
	debug_mask, rpm_vreg_debug_mask, int, S_IRUSR | S_IWUSR
);

#define vreg_err(req, fmt, ...) \
	pr_err("%s: " fmt, req->rdesc.name, ##__VA_ARGS__)

/* RPM regulator request types */
enum rpm_regulator_type {
	RPM_REGULATOR_TYPE_LDO,
	RPM_REGULATOR_TYPE_SMPS,
	RPM_REGULATOR_TYPE_VS,
	RPM_REGULATOR_TYPE_NCP,
	RPM_REGULATOR_TYPE_MAX,
};

/* RPM resource parameters */
enum rpm_regulator_param_index {
	RPM_REGULATOR_PARAM_ENABLE,
	RPM_REGULATOR_PARAM_VOLTAGE,
	RPM_REGULATOR_PARAM_CURRENT,
	RPM_REGULATOR_PARAM_MODE_LDO,
	RPM_REGULATOR_PARAM_MODE_SMPS,
	RPM_REGULATOR_PARAM_PIN_CTRL_ENABLE,
	RPM_REGULATOR_PARAM_PIN_CTRL_MODE,
	RPM_REGULATOR_PARAM_FREQUENCY,
	RPM_REGULATOR_PARAM_HEAD_ROOM,
	RPM_REGULATOR_PARAM_QUIET_MODE,
	RPM_REGULATOR_PARAM_FREQ_REASON,
	RPM_REGULATOR_PARAM_CORNER,
	RPM_REGULATOR_PARAM_BYPASS,
	RPM_REGULATOR_PARAM_FLOOR_CORNER,
	RPM_REGULATOR_PARAM_MAX,
};

enum rpm_regulator_smps_mode {
	RPM_REGULATOR_SMPS_MODE_AUTO	= 0,
	RPM_REGULATOR_SMPS_MODE_IPEAK	= 1,
	RPM_REGULATOR_SMPS_MODE_PWM	= 2,
};

enum rpm_regulator_ldo_mode {
	RPM_REGULATOR_LDO_MODE_IPEAK	= 0,
	RPM_REGULATOR_LDO_MODE_HPM	= 1,
};

#define RPM_SET_CONFIG_ACTIVE			BIT(0)
#define RPM_SET_CONFIG_SLEEP			BIT(1)
#define RPM_SET_CONFIG_BOTH			(RPM_SET_CONFIG_ACTIVE \
						 | RPM_SET_CONFIG_SLEEP)
struct rpm_regulator_param {
	char	*name;
	char	*property_name;
	u32	key;
	u32	min;
	u32	max;
	u32	supported_regulator_types;
};

#define PARAM(_idx, _support_ldo, _support_smps, _support_vs, _support_ncp, \
		_name, _min, _max, _property_name)	\
	[RPM_REGULATOR_PARAM_##_idx] = { \
		.name = _name, \
		.property_name = _property_name, \
		.min = _min, \
		.max = _max, \
		.supported_regulator_types = \
			_support_ldo << RPM_REGULATOR_TYPE_LDO | \
			_support_smps << RPM_REGULATOR_TYPE_SMPS | \
			_support_vs << RPM_REGULATOR_TYPE_VS | \
			_support_ncp << RPM_REGULATOR_TYPE_NCP, \
	}

static struct rpm_regulator_param params[RPM_REGULATOR_PARAM_MAX] = {
	/*    ID             LDO SMPS VS  NCP  name  min max          property-name */
	PARAM(ENABLE,          1,  1,  1,  1, "swen", 0, 1,          "qcom,init-enable"),
	PARAM(VOLTAGE,         1,  1,  0,  1, "uv",   0, 0x7FFFFFF,  "qcom,init-voltage"),
	PARAM(CURRENT,         1,  1,  0,  0, "ma",   0, 0x1FFF,     "qcom,init-current"),
	PARAM(MODE_LDO,        1,  0,  0,  0, "lsmd", 0, 1,          "qcom,init-ldo-mode"),
	PARAM(MODE_SMPS,       0,  1,  0,  0, "ssmd", 0, 2,          "qcom,init-smps-mode"),
	PARAM(PIN_CTRL_ENABLE, 1,  1,  1,  0, "pcen", 0, 0xF,        "qcom,init-pin-ctrl-enable"),
	PARAM(PIN_CTRL_MODE,   1,  1,  1,  0, "pcmd", 0, 0x1F,       "qcom,init-pin-ctrl-mode"),
	PARAM(FREQUENCY,       0,  1,  0,  1, "freq", 0, 31,         "qcom,init-frequency"),
	PARAM(HEAD_ROOM,       1,  0,  0,  1, "hr",   0, 0x7FFFFFFF, "qcom,init-head-room"),
	PARAM(QUIET_MODE,      0,  1,  0,  0, "qm",   0, 2,          "qcom,init-quiet-mode"),
	PARAM(FREQ_REASON,     0,  1,  0,  1, "resn", 0, 8,          "qcom,init-freq-reason"),
	PARAM(CORNER,          1,  1,  0,  0, "corn", 0, 6,          "qcom,init-voltage-corner"),
	PARAM(BYPASS,          1,  0,  0,  0, "bypa", 0, 1,          "qcom,init-disallow-bypass"),
	PARAM(FLOOR_CORNER,    1,  1,  0,  0, "vfc",  0, 6,          "qcom,init-voltage-floor-corner"),
};

struct rpm_regulator_mode_map {
	int			ldo_mode;
	int			smps_mode;
};

static struct rpm_regulator_mode_map mode_mapping[] = {
	[RPM_REGULATOR_MODE_AUTO]
		= {-1,				 RPM_REGULATOR_SMPS_MODE_AUTO},
	[RPM_REGULATOR_MODE_IPEAK]
		= {RPM_REGULATOR_LDO_MODE_IPEAK, RPM_REGULATOR_SMPS_MODE_IPEAK},
	[RPM_REGULATOR_MODE_HPM]
		= {RPM_REGULATOR_LDO_MODE_HPM,   RPM_REGULATOR_SMPS_MODE_PWM},
};

/* Indices for use with pin control enable via enable/disable feature. */
#define RPM_VREG_PIN_CTRL_STATE_DISABLE	0
#define RPM_VREG_PIN_CTRL_STATE_ENABLE	1
#define RPM_VREG_PIN_CTRL_STATE_COUNT	2

struct rpm_vreg_request {
	u32			param[RPM_REGULATOR_PARAM_MAX];
	u32			valid;
	u32			modified;
};

struct rpm_vreg {
	struct rpm_vreg_request	aggr_req_active;
	struct rpm_vreg_request	aggr_req_sleep;
	struct list_head	reg_list;
	const char		*resource_name;
	u32			resource_id;
	bool			allow_atomic;
	int			regulator_type;
	int			hpm_min_load;
	int			enable_time;
	spinlock_t		slock;
	struct mutex		mlock;
	unsigned long		flags;
	bool			sleep_request_sent;
	bool			apps_only;
	struct msm_rpm_request	*handle_active;
	struct msm_rpm_request	*handle_sleep;
};

struct rpm_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	struct rpm_vreg		*rpm_vreg;
	struct list_head	list;
	bool			set_active;
	bool			set_sleep;
	bool			always_send_voltage;
	bool			always_send_current;
	bool			use_pin_ctrl_for_enable;
	struct rpm_vreg_request	req;
	int			system_load;
	int			min_uV;
	int			max_uV;
	u32			pin_ctrl_mask[RPM_VREG_PIN_CTRL_STATE_COUNT];
};

/*
 * This voltage in uV is returned by get_voltage functions when there is no way
 * to determine the current voltage level.  It is needed because the regulator
 * framework treats a 0 uV voltage as an error.
 */
#define VOLTAGE_UNKNOWN 1

/*
 * Regulator requests sent in the active set take effect immediately.  Requests
 * sent in the sleep set take effect when the Apps processor transitions into
 * RPM assisted power collapse.  For any given regulator, if an active set
 * request is present, but not a sleep set request, then the active set request
 * is used at all times, even when the Apps processor is power collapsed.
 *
 * The rpm-regulator-smd takes advantage of this default usage of the active set
 * request by only sending a sleep set request if it differs from the
 * corresponding active set request.
 */
#define RPM_SET_ACTIVE	MSM_RPM_CTX_ACTIVE_SET
#define RPM_SET_SLEEP	MSM_RPM_CTX_SLEEP_SET

static u32 rpm_vreg_string_to_int(const u8 *str)
{
	int i, len;
	u32 output = 0;

	len = strnlen(str, sizeof(u32));
	for (i = 0; i < len; i++)
		output |= str[i] << (i * 8);

	return output;
}

static inline void rpm_vreg_lock(struct rpm_vreg *rpm_vreg)
{
	if (rpm_vreg->allow_atomic)
		spin_lock_irqsave(&rpm_vreg->slock, rpm_vreg->flags);
	else
		mutex_lock(&rpm_vreg->mlock);
}

static inline void rpm_vreg_unlock(struct rpm_vreg *rpm_vreg)
{
	if (rpm_vreg->allow_atomic)
		spin_unlock_irqrestore(&rpm_vreg->slock, rpm_vreg->flags);
	else
		mutex_unlock(&rpm_vreg->mlock);
}

static inline bool rpm_vreg_active_or_sleep_enabled(struct rpm_vreg *rpm_vreg)
{
	return (rpm_vreg->aggr_req_active.param[RPM_REGULATOR_PARAM_ENABLE]
			&& (rpm_vreg->aggr_req_active.valid
				& BIT(RPM_REGULATOR_PARAM_ENABLE)))
	    || ((rpm_vreg->aggr_req_sleep.param[RPM_REGULATOR_PARAM_ENABLE])
				&& (rpm_vreg->aggr_req_sleep.valid
					& BIT(RPM_REGULATOR_PARAM_ENABLE)));
}

static inline bool rpm_vreg_shared_active_or_sleep_enabled_valid
						(struct rpm_vreg *rpm_vreg)
{
	return !rpm_vreg->apps_only &&
		((rpm_vreg->aggr_req_active.valid
					& BIT(RPM_REGULATOR_PARAM_ENABLE))
		 || (rpm_vreg->aggr_req_sleep.valid
					& BIT(RPM_REGULATOR_PARAM_ENABLE)));
}

/*
 * This is used when voting for LPM or HPM by subtracting or adding to the
 * hpm_min_load of a regulator.  It has units of uA.
 */
#define LOAD_THRESHOLD_STEP	1000

static inline int rpm_vreg_hpm_min_uA(struct rpm_vreg *rpm_vreg)
{
	return rpm_vreg->hpm_min_load;
}

static inline int rpm_vreg_lpm_max_uA(struct rpm_vreg *rpm_vreg)
{
	return rpm_vreg->hpm_min_load - LOAD_THRESHOLD_STEP;
}

#define MICRO_TO_MILLI(uV)	((uV) / 1000)
#define MILLI_TO_MICRO(uV)	((uV) * 1000)

#define DEBUG_PRINT_BUFFER_SIZE 512
#define REQ_SENT	0
#define REQ_PREV	1
#define REQ_CACHED	2
#define REQ_TYPES	3

static void rpm_regulator_req(struct rpm_regulator *regulator, int set,
				bool sent)
{
	char buf[DEBUG_PRINT_BUFFER_SIZE];
	size_t buflen = DEBUG_PRINT_BUFFER_SIZE;
	struct rpm_vreg *rpm_vreg = regulator->rpm_vreg;
	struct rpm_vreg_request *aggr;
	bool first;
	u32 mask[REQ_TYPES] = {0, 0, 0};
	const char *req_names[REQ_TYPES] = {"sent", "prev", "cached"};
	int pos = 0;
	int i, j;

	aggr = (set == RPM_SET_ACTIVE)
		? &rpm_vreg->aggr_req_active : &rpm_vreg->aggr_req_sleep;

	if (rpm_vreg_debug_mask & RPM_VREG_DEBUG_DUPLICATE) {
		mask[REQ_SENT] = aggr->modified;
		mask[REQ_PREV] = aggr->valid & ~aggr->modified;
	} else if (sent
		   && (rpm_vreg_debug_mask & RPM_VREG_DEBUG_FULL_REQUEST)) {
		mask[REQ_SENT] = aggr->modified;
		mask[REQ_PREV] = aggr->valid & ~aggr->modified;
	} else if (sent && (rpm_vreg_debug_mask & RPM_VREG_DEBUG_REQUEST)) {
		mask[REQ_SENT] = aggr->modified;
	}

	if (!(mask[REQ_SENT] | mask[REQ_PREV]))
		return;

	if (set == RPM_SET_SLEEP && !rpm_vreg->sleep_request_sent) {
		mask[REQ_CACHED] = mask[REQ_SENT] | mask[REQ_PREV];
		mask[REQ_SENT] = 0;
		mask[REQ_PREV] = 0;
	}

	pos += scnprintf(buf + pos, buflen - pos, "%s%s: ",
			KERN_INFO, __func__);

	pos += scnprintf(buf + pos, buflen - pos, "%s %u (%s): s=%s",
			rpm_vreg->resource_name, rpm_vreg->resource_id,
			regulator->rdesc.name,
			(set == RPM_SET_ACTIVE ? "act" : "slp"));

	for (i = 0; i < REQ_TYPES; i++) {
		if (mask[i])
			pos += scnprintf(buf + pos, buflen - pos, "; %s: ",
					req_names[i]);

		first = true;
		for (j = 0; j < RPM_REGULATOR_PARAM_MAX; j++) {
			if (mask[i] & BIT(j)) {
				pos += scnprintf(buf + pos, buflen - pos,
					"%s%s=%u", (first ? "" : ", "),
					params[j].name, aggr->param[j]);
				first = false;
			}
		}
	}

	pos += scnprintf(buf + pos, buflen - pos, "\n");
	printk(buf);
}

#define RPM_VREG_SET_PARAM(_regulator, _param, _val) \
{ \
	(_regulator)->req.param[RPM_REGULATOR_PARAM_##_param] = _val; \
	(_regulator)->req.modified |= BIT(RPM_REGULATOR_PARAM_##_param); \
} \

static int rpm_vreg_add_kvp_to_request(struct rpm_vreg *rpm_vreg,
				       const u32 *param, int idx, u32 set)
{
	struct msm_rpm_request *handle;

	handle = (set == RPM_SET_ACTIVE	? rpm_vreg->handle_active
					: rpm_vreg->handle_sleep);

	if (rpm_vreg->allow_atomic)
		return msm_rpm_add_kvp_data_noirq(handle, params[idx].key,
						  (u8 *)&param[idx], 4);
	else
		return msm_rpm_add_kvp_data(handle, params[idx].key,
					    (u8 *)&param[idx], 4);
}

static void rpm_vreg_check_modified_requests(const u32 *prev_param,
		const u32 *param, u32 prev_valid, u32 *modified)
{
	u32 value_changed = 0;
	int i;

	for (i = 0; i < RPM_REGULATOR_PARAM_MAX; i++) {
		if (param[i] != prev_param[i])
			value_changed |= BIT(i);
	}

	/*
	 * Only keep bits that are for changed parameters or previously
	 * invalid parameters.
	 */
	*modified &= value_changed | ~prev_valid;
}

static int rpm_vreg_add_modified_requests(struct rpm_regulator *regulator,
		u32 set, const u32 *param, u32 modified)
{
	struct rpm_vreg *rpm_vreg = regulator->rpm_vreg;
	int rc = 0;
	int i;

	for (i = 0; i < RPM_REGULATOR_PARAM_MAX; i++) {
		/* Only send requests for modified parameters. */
		if (modified & BIT(i)) {
			rc = rpm_vreg_add_kvp_to_request(rpm_vreg, param, i,
							set);
			if (rc) {
				vreg_err(regulator,
					"add KVP failed: %s %u; %s, rc=%d\n",
					rpm_vreg->resource_name,
					rpm_vreg->resource_id, params[i].name,
					rc);
				return rc;
			}
		}
	}

	return rc;
}

static int rpm_vreg_send_request(struct rpm_regulator *regulator, u32 set)
{
	struct rpm_vreg *rpm_vreg = regulator->rpm_vreg;
	struct msm_rpm_request *handle
		= (set == RPM_SET_ACTIVE ? rpm_vreg->handle_active
					: rpm_vreg->handle_sleep);
	int rc;

	if (rpm_vreg->allow_atomic)
		rc = msm_rpm_wait_for_ack_noirq(msm_rpm_send_request_noirq(
						  handle));
	else
		rc = msm_rpm_wait_for_ack(msm_rpm_send_request(handle));

	if (rc)
		vreg_err(regulator,
			"msm rpm send failed: %s %u; set=%s, rc=%d\n",
			rpm_vreg->resource_name,
			rpm_vreg->resource_id,
			(set == RPM_SET_ACTIVE ? "act" : "slp"), rc);

	return rc;
}

#define RPM_VREG_AGGR_MIN(_idx, _param_aggr, _param_reg) \
{ \
	_param_aggr[RPM_REGULATOR_PARAM_##_idx] \
	 = min(_param_aggr[RPM_REGULATOR_PARAM_##_idx], \
		_param_reg[RPM_REGULATOR_PARAM_##_idx]); \
}

#define RPM_VREG_AGGR_MAX(_idx, _param_aggr, _param_reg) \
{ \
	_param_aggr[RPM_REGULATOR_PARAM_##_idx] \
	 = max(_param_aggr[RPM_REGULATOR_PARAM_##_idx], \
		_param_reg[RPM_REGULATOR_PARAM_##_idx]); \
}

#define RPM_VREG_AGGR_SUM(_idx, _param_aggr, _param_reg) \
{ \
	_param_aggr[RPM_REGULATOR_PARAM_##_idx] \
		 += _param_reg[RPM_REGULATOR_PARAM_##_idx]; \
}

#define RPM_VREG_AGGR_OR(_idx, _param_aggr, _param_reg) \
{ \
	_param_aggr[RPM_REGULATOR_PARAM_##_idx] \
		|= _param_reg[RPM_REGULATOR_PARAM_##_idx]; \
}

/*
 * Aggregation is performed on each parameter based on the way that the RPM
 * aggregates that type internally between RPM masters.
 */
static void rpm_vreg_aggregate_params(u32 *param_aggr, const u32 *param_reg)
{
	RPM_VREG_AGGR_MAX(ENABLE, param_aggr, param_reg);
	RPM_VREG_AGGR_MAX(VOLTAGE, param_aggr, param_reg);
	RPM_VREG_AGGR_SUM(CURRENT, param_aggr, param_reg);
	RPM_VREG_AGGR_MAX(MODE_LDO, param_aggr, param_reg);
	RPM_VREG_AGGR_MAX(MODE_SMPS, param_aggr, param_reg);
	RPM_VREG_AGGR_OR(PIN_CTRL_ENABLE, param_aggr, param_reg);
	RPM_VREG_AGGR_OR(PIN_CTRL_MODE, param_aggr, param_reg);
	RPM_VREG_AGGR_MIN(FREQUENCY, param_aggr, param_reg);
	RPM_VREG_AGGR_MAX(HEAD_ROOM, param_aggr, param_reg);
	RPM_VREG_AGGR_MAX(QUIET_MODE, param_aggr, param_reg);
	RPM_VREG_AGGR_MAX(FREQ_REASON, param_aggr, param_reg);
	RPM_VREG_AGGR_MAX(CORNER, param_aggr, param_reg);
	RPM_VREG_AGGR_MAX(BYPASS, param_aggr, param_reg);
	RPM_VREG_AGGR_MAX(FLOOR_CORNER, param_aggr, param_reg);
}

static int rpm_vreg_aggregate_requests(struct rpm_regulator *regulator)
{
	struct rpm_vreg *rpm_vreg = regulator->rpm_vreg;
	u32 param_active[RPM_REGULATOR_PARAM_MAX];
	u32 param_sleep[RPM_REGULATOR_PARAM_MAX];
	u32 modified_active, modified_sleep;
	struct rpm_regulator *reg;
	bool sleep_set_differs = false;
	bool send_active = false;
	bool send_sleep = false;
	int rc = 0;
	int i;

	memset(param_active, 0, sizeof(param_active));
	memset(param_sleep, 0, sizeof(param_sleep));
	modified_active = rpm_vreg->aggr_req_active.modified;
	modified_sleep = rpm_vreg->aggr_req_sleep.modified;

	/*
	 * Aggregate all of the requests for this regulator in both active
	 * and sleep sets.
	 */
	list_for_each_entry(reg, &rpm_vreg->reg_list, list) {
		if (reg->set_active) {
			rpm_vreg_aggregate_params(param_active, reg->req.param);
			modified_active |= reg->req.modified;
		}
		if (reg->set_sleep) {
			rpm_vreg_aggregate_params(param_sleep, reg->req.param);
			modified_sleep |= reg->req.modified;
		}
	}

	/*
	 * Check if the aggregated sleep set parameter values differ from the
	 * aggregated active set parameter values.
	 */
	if (!rpm_vreg->sleep_request_sent) {
		for (i = 0; i < RPM_REGULATOR_PARAM_MAX; i++) {
			if ((param_active[i] != param_sleep[i])
			    && (modified_sleep & BIT(i))) {
				sleep_set_differs = true;
				break;
			}
		}
	}

	/* Add KVPs to the active set RPM request if they have new values. */
	rpm_vreg_check_modified_requests(rpm_vreg->aggr_req_active.param,
		param_active, rpm_vreg->aggr_req_active.valid,
		&modified_active);
	rc = rpm_vreg_add_modified_requests(regulator, RPM_SET_ACTIVE,
		param_active, modified_active);
	if (rc)
		return rc;
	send_active = modified_active;

	/*
	 * Sleep set configurations are only sent if they differ from the
	 * active set values.  This is because the active set values will take
	 * effect during rpm assisted power collapse in the absence of sleep set
	 * values.
	 *
	 * However, once a sleep set request is sent for a given regulator,
	 * additional sleep set requests must be sent in the future even if they
	 * match the corresponding active set requests.
	 */
	if (rpm_vreg->sleep_request_sent || sleep_set_differs) {
		/* Add KVPs to the sleep set RPM request if they are new. */
		rpm_vreg_check_modified_requests(rpm_vreg->aggr_req_sleep.param,
			param_sleep, rpm_vreg->aggr_req_sleep.valid,
			&modified_sleep);
		rc = rpm_vreg_add_modified_requests(regulator, RPM_SET_SLEEP,
			param_sleep, modified_sleep);
		if (rc)
			return rc;
		send_sleep = modified_sleep;
	}

	/* Send active set request to the RPM if it contains new KVPs. */
	if (send_active) {
		rc = rpm_vreg_send_request(regulator, RPM_SET_ACTIVE);
		if (rc)
			return rc;
		rpm_vreg->aggr_req_active.valid |= modified_active;
	}
	/* Store the results of the aggregation. */
	rpm_vreg->aggr_req_active.modified = modified_active;
	memcpy(rpm_vreg->aggr_req_active.param, param_active,
		sizeof(param_active));

	/* Handle debug printing of the active set request. */
	rpm_regulator_req(regulator, RPM_SET_ACTIVE, send_active);
	if (send_active)
		rpm_vreg->aggr_req_active.modified = 0;

	/* Send sleep set request to the RPM if it contains new KVPs. */
	if (send_sleep) {
		rc = rpm_vreg_send_request(regulator, RPM_SET_SLEEP);
		if (rc)
			return rc;
		else
			rpm_vreg->sleep_request_sent = true;
		rpm_vreg->aggr_req_sleep.valid |= modified_sleep;
	}
	/* Store the results of the aggregation. */
	rpm_vreg->aggr_req_sleep.modified = modified_sleep;
	memcpy(rpm_vreg->aggr_req_sleep.param, param_sleep,
		sizeof(param_sleep));

	/* Handle debug printing of the sleep set request. */
	rpm_regulator_req(regulator, RPM_SET_SLEEP, send_sleep);
	if (send_sleep)
		rpm_vreg->aggr_req_sleep.modified = 0;

	/*
	 * Loop over all requests for this regulator to update the valid and
	 * modified values for use in future aggregation.
	 */
	list_for_each_entry(reg, &rpm_vreg->reg_list, list) {
		reg->req.valid |= reg->req.modified;
		reg->req.modified = 0;
	}

	return rc;
}

static int rpm_vreg_is_enabled(struct regulator_dev *rdev)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);

	if (likely(!reg->use_pin_ctrl_for_enable))
		return reg->req.param[RPM_REGULATOR_PARAM_ENABLE];
	else
		return reg->req.param[RPM_REGULATOR_PARAM_PIN_CTRL_ENABLE]
			== reg->pin_ctrl_mask[RPM_VREG_PIN_CTRL_STATE_ENABLE];
}

static int rpm_vreg_enable(struct regulator_dev *rdev)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);
	int rc;
	u32 prev_enable;

	rpm_vreg_lock(reg->rpm_vreg);

	if (likely(!reg->use_pin_ctrl_for_enable)) {
		/* Enable using swen KVP. */
		prev_enable = reg->req.param[RPM_REGULATOR_PARAM_ENABLE];
		RPM_VREG_SET_PARAM(reg, ENABLE, 1);
		rc = rpm_vreg_aggregate_requests(reg);
		if (rc) {
			vreg_err(reg, "enable failed, rc=%d", rc);
			RPM_VREG_SET_PARAM(reg, ENABLE, prev_enable);
		}
	} else {
		/* Enable using pcen KVP. */
		prev_enable
			= reg->req.param[RPM_REGULATOR_PARAM_PIN_CTRL_ENABLE];
		RPM_VREG_SET_PARAM(reg, PIN_CTRL_ENABLE,
			reg->pin_ctrl_mask[RPM_VREG_PIN_CTRL_STATE_ENABLE]);
		rc = rpm_vreg_aggregate_requests(reg);
		if (rc) {
			vreg_err(reg, "enable failed, rc=%d", rc);
			RPM_VREG_SET_PARAM(reg, PIN_CTRL_ENABLE, prev_enable);
		}
	}

	rpm_vreg_unlock(reg->rpm_vreg);

	return rc;
}

static int rpm_vreg_disable(struct regulator_dev *rdev)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);
	int rc;
	u32 prev_enable;

	rpm_vreg_lock(reg->rpm_vreg);

	if (likely(!reg->use_pin_ctrl_for_enable)) {
		/* Disable using swen KVP. */
		prev_enable = reg->req.param[RPM_REGULATOR_PARAM_ENABLE];
		RPM_VREG_SET_PARAM(reg, ENABLE, 0);
		rc = rpm_vreg_aggregate_requests(reg);
		if (rc) {
			vreg_err(reg, "disable failed, rc=%d", rc);
			RPM_VREG_SET_PARAM(reg, ENABLE, prev_enable);
		}
	} else {
		/* Disable using pcen KVP. */
		prev_enable
			= reg->req.param[RPM_REGULATOR_PARAM_PIN_CTRL_ENABLE];
		RPM_VREG_SET_PARAM(reg, PIN_CTRL_ENABLE,
			reg->pin_ctrl_mask[RPM_VREG_PIN_CTRL_STATE_DISABLE]);
		rc = rpm_vreg_aggregate_requests(reg);
		if (rc) {
			vreg_err(reg, "disable failed, rc=%d", rc);
			RPM_VREG_SET_PARAM(reg, PIN_CTRL_ENABLE, prev_enable);
		}
	}

	rpm_vreg_unlock(reg->rpm_vreg);

	return rc;
}

static int rpm_vreg_set_voltage(struct regulator_dev *rdev, int min_uV,
				int max_uV, unsigned *selector)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);
	int rc = 0;
	u32 prev_voltage;

	rpm_vreg_lock(reg->rpm_vreg);

	prev_voltage = reg->req.param[RPM_REGULATOR_PARAM_VOLTAGE];
	RPM_VREG_SET_PARAM(reg, VOLTAGE, min_uV);

	/*
	 * Only send a new voltage if the regulator is currently enabled or
	 * if the regulator has been configured to always send voltage updates.
	 */
	if (reg->always_send_voltage
	    || rpm_vreg_active_or_sleep_enabled(reg->rpm_vreg)
	    || rpm_vreg_shared_active_or_sleep_enabled_valid(reg->rpm_vreg))
		rc = rpm_vreg_aggregate_requests(reg);

	if (rc) {
		vreg_err(reg, "set voltage failed, rc=%d", rc);
		RPM_VREG_SET_PARAM(reg, VOLTAGE, prev_voltage);
	}

	rpm_vreg_unlock(reg->rpm_vreg);

	return rc;
}

static int rpm_vreg_get_voltage(struct regulator_dev *rdev)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);
	int uV;

	uV = reg->req.param[RPM_REGULATOR_PARAM_VOLTAGE];
	if (uV == 0)
		uV = VOLTAGE_UNKNOWN;

	return uV;
}

static int rpm_vreg_set_voltage_corner(struct regulator_dev *rdev, int min_uV,
				int max_uV, unsigned *selector)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);
	int rc = 0;
	int corner;
	u32 prev_corner;

	/*
	 * Translate from values which work as inputs in the
	 * regulator_set_voltage function to the actual corner values
	 * sent to the RPM.
	 */
	corner = min_uV - RPM_REGULATOR_CORNER_NONE;

	if (corner < params[RPM_REGULATOR_PARAM_CORNER].min
	    || corner > params[RPM_REGULATOR_PARAM_CORNER].max) {
		vreg_err(reg, "corner=%d is not within allowed range: [%u, %u]\n",
			corner, params[RPM_REGULATOR_PARAM_CORNER].min,
			params[RPM_REGULATOR_PARAM_CORNER].max);
		return -EINVAL;
	}

	rpm_vreg_lock(reg->rpm_vreg);

	prev_corner = reg->req.param[RPM_REGULATOR_PARAM_CORNER];
	RPM_VREG_SET_PARAM(reg, CORNER, corner);

	/*
	 * Only send a new voltage corner if the regulator is currently enabled
	 * or if the regulator has been configured to always send voltage
	 * updates.
	 */
	if (reg->always_send_voltage
	    || rpm_vreg_active_or_sleep_enabled(reg->rpm_vreg)
	    || rpm_vreg_shared_active_or_sleep_enabled_valid(reg->rpm_vreg))
		rc = rpm_vreg_aggregate_requests(reg);

	if (rc) {
		vreg_err(reg, "set voltage corner failed, rc=%d", rc);
		RPM_VREG_SET_PARAM(reg, CORNER, prev_corner);
	}

	rpm_vreg_unlock(reg->rpm_vreg);

	return rc;
}

static int rpm_vreg_get_voltage_corner(struct regulator_dev *rdev)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);

	return reg->req.param[RPM_REGULATOR_PARAM_CORNER]
		+ RPM_REGULATOR_CORNER_NONE;
}

static int rpm_vreg_set_voltage_floor_corner(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);
	int rc = 0;
	int corner;
	u32 prev_corner;

	/*
	 * Translate from values which work as inputs in the
	 * regulator_set_voltage function to the actual corner values
	 * sent to the RPM.
	 */
	corner = min_uV - RPM_REGULATOR_CORNER_NONE;

	if (corner < params[RPM_REGULATOR_PARAM_FLOOR_CORNER].min
	    || corner > params[RPM_REGULATOR_PARAM_FLOOR_CORNER].max) {
		vreg_err(reg, "corner=%d is not within allowed range: [%u, %u]\n",
			corner, params[RPM_REGULATOR_PARAM_FLOOR_CORNER].min,
			params[RPM_REGULATOR_PARAM_FLOOR_CORNER].max);
		return -EINVAL;
	}

	rpm_vreg_lock(reg->rpm_vreg);

	prev_corner = reg->req.param[RPM_REGULATOR_PARAM_FLOOR_CORNER];
	RPM_VREG_SET_PARAM(reg, FLOOR_CORNER, corner);

	/*
	 * Only send a new voltage floor corner if the regulator is currently
	 * enabled or if the regulator has been configured to always send
	 * voltage updates.
	 */
	if (reg->always_send_voltage
	    || rpm_vreg_active_or_sleep_enabled(reg->rpm_vreg)
	    || rpm_vreg_shared_active_or_sleep_enabled_valid(reg->rpm_vreg))
		rc = rpm_vreg_aggregate_requests(reg);

	if (rc) {
		vreg_err(reg, "set voltage corner failed, rc=%d", rc);
		RPM_VREG_SET_PARAM(reg, FLOOR_CORNER, prev_corner);
	}

	rpm_vreg_unlock(reg->rpm_vreg);

	return rc;
}

static int rpm_vreg_get_voltage_floor_corner(struct regulator_dev *rdev)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);

	return reg->req.param[RPM_REGULATOR_PARAM_FLOOR_CORNER]
		+ RPM_REGULATOR_CORNER_NONE;
}

static int rpm_vreg_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);
	int rc = 0;
	u32 prev_current;
	int prev_uA;

	rpm_vreg_lock(reg->rpm_vreg);

	prev_current = reg->req.param[RPM_REGULATOR_PARAM_CURRENT];
	prev_uA = MILLI_TO_MICRO(prev_current);

	if (mode == REGULATOR_MODE_NORMAL) {
		/* Make sure that request current is in HPM range. */
		if (prev_uA < rpm_vreg_hpm_min_uA(reg->rpm_vreg))
			RPM_VREG_SET_PARAM(reg, CURRENT,
			    MICRO_TO_MILLI(rpm_vreg_hpm_min_uA(reg->rpm_vreg)));
	} else if (REGULATOR_MODE_IDLE) {
		/* Make sure that request current is in LPM range. */
		if (prev_uA > rpm_vreg_lpm_max_uA(reg->rpm_vreg))
			RPM_VREG_SET_PARAM(reg, CURRENT,
			    MICRO_TO_MILLI(rpm_vreg_lpm_max_uA(reg->rpm_vreg)));
	} else {
		vreg_err(reg, "invalid mode: %u\n", mode);
		rpm_vreg_unlock(reg->rpm_vreg);
		return -EINVAL;
	}

	/*
	 * Only send a new load current value if the regulator is currently
	 * enabled or if the regulator has been configured to always send
	 * current updates.
	 */
	if (reg->always_send_current
	    || rpm_vreg_active_or_sleep_enabled(reg->rpm_vreg)
	    || rpm_vreg_shared_active_or_sleep_enabled_valid(reg->rpm_vreg))
		rc = rpm_vreg_aggregate_requests(reg);

	if (rc) {
		vreg_err(reg, "set mode failed, rc=%d", rc);
		RPM_VREG_SET_PARAM(reg, CURRENT, prev_current);
	}

	rpm_vreg_unlock(reg->rpm_vreg);

	return rc;
}

static unsigned int rpm_vreg_get_mode(struct regulator_dev *rdev)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);

	return (reg->req.param[RPM_REGULATOR_PARAM_CURRENT]
			>= MICRO_TO_MILLI(reg->rpm_vreg->hpm_min_load))
		? REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE;
}

static unsigned int rpm_vreg_get_optimum_mode(struct regulator_dev *rdev,
			int input_uV, int output_uV, int load_uA)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);
	u32 load_mA;

	load_uA += reg->system_load;

	load_mA = MICRO_TO_MILLI(load_uA);
	if (load_mA > params[RPM_REGULATOR_PARAM_CURRENT].max)
		load_mA = params[RPM_REGULATOR_PARAM_CURRENT].max;

	rpm_vreg_lock(reg->rpm_vreg);
	RPM_VREG_SET_PARAM(reg, CURRENT, load_mA);
	rpm_vreg_unlock(reg->rpm_vreg);

	return (load_uA >= reg->rpm_vreg->hpm_min_load)
		? REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE;
}

static int rpm_vreg_enable_time(struct regulator_dev *rdev)
{
	struct rpm_regulator *reg = rdev_get_drvdata(rdev);

	return reg->rpm_vreg->enable_time;
}

static int rpm_vreg_send_defaults(struct rpm_regulator *reg)
{
	int rc;

	rpm_vreg_lock(reg->rpm_vreg);
	rc = rpm_vreg_aggregate_requests(reg);
	if (rc)
		vreg_err(reg, "RPM request failed, rc=%d", rc);
	rpm_vreg_unlock(reg->rpm_vreg);

	return rc;
}

static int rpm_vreg_configure_pin_control_enable(struct rpm_regulator *reg,
		struct device_node *node)
{
	struct rpm_regulator_param *pcen_param =
			&params[RPM_REGULATOR_PARAM_PIN_CTRL_ENABLE];
	int rc, i;

	if (!of_find_property(node, "qcom,enable-with-pin-ctrl", NULL))
		return 0;

	if (pcen_param->supported_regulator_types
			& BIT(reg->rpm_vreg->regulator_type)) {
		rc = of_property_read_u32_array(node,
			"qcom,enable-with-pin-ctrl", reg->pin_ctrl_mask,
			RPM_VREG_PIN_CTRL_STATE_COUNT);
		if (rc) {
			vreg_err(reg, "could not read qcom,enable-with-pin-ctrl, rc=%d\n",
				rc);
			return rc;
		}

		/* Verify that the mask values are valid. */
		for (i = 0; i < RPM_VREG_PIN_CTRL_STATE_COUNT; i++) {
			if (reg->pin_ctrl_mask[i] < pcen_param->min
			    || reg->pin_ctrl_mask[i] > pcen_param->max) {
				vreg_err(reg, "device tree property: qcom,enable-with-pin-ctrl[%d]=%u is outside allowed range [%u, %u]\n",
					i, reg->pin_ctrl_mask[i],
					pcen_param->min, pcen_param->max);
				return -EINVAL;
			}
		}

		reg->use_pin_ctrl_for_enable = true;
	} else {
		pr_warn("%s: regulator type=%d does not support device tree property: qcom,enable-with-pin-ctrl\n",
			reg->rdesc.name, reg->rpm_vreg->regulator_type);
	}

	return 0;
}

/**
 * rpm_regulator_get() - lookup and obtain a handle to an RPM regulator
 * @dev: device for regulator consumer
 * @supply: supply name
 *
 * Returns a struct rpm_regulator corresponding to the regulator producer,
 * or ERR_PTR() containing errno.
 *
 * This function may only be called from nonatomic context.
 */
struct rpm_regulator *rpm_regulator_get(struct device *dev, const char *supply)
{
	struct rpm_regulator *framework_reg;
	struct rpm_regulator *priv_reg = NULL;
	struct regulator *regulator;
	struct rpm_vreg *rpm_vreg;

	regulator = regulator_get(dev, supply);
	if (IS_ERR(regulator)) {
		pr_err("could not find regulator for: dev=%s, supply=%s, rc=%ld\n",
			(dev ? dev_name(dev) : ""), (supply ? supply : ""),
			PTR_ERR(regulator));
		return ERR_CAST(regulator);
	}

	framework_reg = regulator_get_drvdata(regulator);
	if (framework_reg == NULL) {
		pr_err("regulator structure not found.\n");
		regulator_put(regulator);
		return ERR_PTR(-ENODEV);
	}
	regulator_put(regulator);

	rpm_vreg = framework_reg->rpm_vreg;

	priv_reg = kzalloc(sizeof(struct rpm_regulator), GFP_KERNEL);
	if (priv_reg == NULL) {
		vreg_err(framework_reg,
			"could not allocate memory for regulator\n");
		return ERR_PTR(-ENOMEM);
	}

	/*
	 * Allocate a regulator_dev struct so that framework callback functions
	 * can be called from the private API functions.
	 */
	priv_reg->rdev = kzalloc(sizeof(struct regulator_dev), GFP_KERNEL);
	if (priv_reg->rdev == NULL) {
		vreg_err(framework_reg,
			"could not allocate memory for regulator_dev\n");
		kfree(priv_reg);
		return ERR_PTR(-ENOMEM);
	}
	priv_reg->rdev->reg_data	= priv_reg;
	priv_reg->rpm_vreg		= rpm_vreg;
	priv_reg->rdesc.name		= framework_reg->rdesc.name;
	priv_reg->rdesc.ops		= framework_reg->rdesc.ops;
	priv_reg->set_active		= framework_reg->set_active;
	priv_reg->set_sleep		= framework_reg->set_sleep;
	priv_reg->min_uV		= framework_reg->min_uV;
	priv_reg->max_uV		= framework_reg->max_uV;
	priv_reg->system_load		= framework_reg->system_load;

	might_sleep_if(!rpm_vreg->allow_atomic);
	rpm_vreg_lock(rpm_vreg);
	list_add(&priv_reg->list, &rpm_vreg->reg_list);
	rpm_vreg_unlock(rpm_vreg);

	return priv_reg;
}
EXPORT_SYMBOL(rpm_regulator_get);

static int rpm_regulator_check_input(struct rpm_regulator *regulator)
{
	if (IS_ERR_OR_NULL(regulator) || regulator->rpm_vreg == NULL) {
		pr_err("invalid rpm_regulator pointer\n");
		return -EINVAL;
	}

	might_sleep_if(!regulator->rpm_vreg->allow_atomic);

	return 0;
}

/**
 * rpm_regulator_put() - free the RPM regulator handle
 * @regulator: RPM regulator handle
 *
 * Parameter reaggregation does not take place when rpm_regulator_put is called.
 * Therefore, regulator enable state and voltage must be configured
 * appropriately before calling rpm_regulator_put.
 *
 * This function may be called from either atomic or nonatomic context.  If this
 * function is called from atomic context, then the regulator being operated on
 * must be configured via device tree with qcom,allow-atomic == 1.
 */
void rpm_regulator_put(struct rpm_regulator *regulator)
{
	struct rpm_vreg *rpm_vreg;
	int rc = rpm_regulator_check_input(regulator);

	if (rc)
		return;

	rpm_vreg = regulator->rpm_vreg;

	might_sleep_if(!rpm_vreg->allow_atomic);
	rpm_vreg_lock(rpm_vreg);
	list_del(&regulator->list);
	rpm_vreg_unlock(rpm_vreg);

	kfree(regulator->rdev);
	kfree(regulator);
}
EXPORT_SYMBOL(rpm_regulator_put);

/**
 * rpm_regulator_enable() - enable regulator output
 * @regulator: RPM regulator handle
 *
 * Returns 0 on success or errno on failure.
 *
 * This function may be called from either atomic or nonatomic context.  If this
 * function is called from atomic context, then the regulator being operated on
 * must be configured via device tree with qcom,allow-atomic == 1.
 */
int rpm_regulator_enable(struct rpm_regulator *regulator)
{
	int rc = rpm_regulator_check_input(regulator);

	if (rc)
		return rc;

	return rpm_vreg_enable(regulator->rdev);
}
EXPORT_SYMBOL(rpm_regulator_enable);

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
 * This function may be called from either atomic or nonatomic context.  If this
 * function is called from atomic context, then the regulator being operated on
 * must be configured via device tree with qcom,allow-atomic == 1.
 */
int rpm_regulator_disable(struct rpm_regulator *regulator)
{
	int rc = rpm_regulator_check_input(regulator);

	if (rc)
		return rc;

	return rpm_vreg_disable(regulator->rdev);
}
EXPORT_SYMBOL(rpm_regulator_disable);

/**
 * rpm_regulator_set_voltage() - set regulator output voltage
 * @regulator: RPM regulator handle
 * @min_uV: minimum required voltage in uV
 * @max_uV: maximum acceptable voltage in uV
 *
 * Sets a voltage regulator to the desired output voltage. This can be set
 * while the regulator is disabled or enabled.  If the regulator is enabled then
 * the voltage will change to the new value immediately; otherwise, if the
 * regulator is disabled, then the regulator will output at the new voltage when
 * enabled.
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
 * This function may be called from either atomic or nonatomic context.  If this
 * function is called from atomic context, then the regulator being operated on
 * must be configured via device tree with qcom,allow-atomic == 1.
 */
int rpm_regulator_set_voltage(struct rpm_regulator *regulator, int min_uV,
			      int max_uV)
{
	int rc = rpm_regulator_check_input(regulator);
	int uV = min_uV;

	if (rc)
		return rc;

	if (regulator->rpm_vreg->regulator_type == RPM_REGULATOR_TYPE_VS) {
		vreg_err(regulator, "unsupported regulator type: %d\n",
			regulator->rpm_vreg->regulator_type);
		return -EINVAL;
	}

	if (min_uV > max_uV) {
		vreg_err(regulator, "min_uV=%d must be less than max_uV=%d\n",
			min_uV, max_uV);
		return -EINVAL;
	}

	if (uV < regulator->min_uV && max_uV >= regulator->min_uV)
		uV = regulator->min_uV;

	if (uV < regulator->min_uV || uV > regulator->max_uV) {
		vreg_err(regulator,
			"request v=[%d, %d] is outside allowed v=[%d, %d]\n",
			min_uV, max_uV, regulator->min_uV, regulator->max_uV);
		return -EINVAL;
	}

	return regulator->rdesc.ops->set_voltage(regulator->rdev, uV, uV, NULL);
}
EXPORT_SYMBOL(rpm_regulator_set_voltage);

/**
 * rpm_regulator_set_mode() - set regulator operating mode
 * @regulator: RPM regulator handle
 * @mode: operating mode requested for the regulator
 *
 * Requests that the mode of the regulator be set to the mode specified.  This
 * parameter is aggregated using a max function such that AUTO < IPEAK < HPM.
 *
 * Returns 0 on success or errno on failure.
 */
int rpm_regulator_set_mode(struct rpm_regulator *regulator,
				enum rpm_regulator_mode mode)
{
	int index = 0;
	u32 new_mode, prev_mode;
	int rc;

	rc = rpm_regulator_check_input(regulator);
	if (rc)
		return rc;

	if (mode < 0 || mode >= ARRAY_SIZE(mode_mapping)) {
		vreg_err(regulator, "invalid mode requested: %d\n", mode);
		return -EINVAL;
	}

	switch (regulator->rpm_vreg->regulator_type) {
	case RPM_REGULATOR_TYPE_SMPS:
		index = RPM_REGULATOR_PARAM_MODE_SMPS;
		new_mode = mode_mapping[mode].smps_mode;
		break;
	case RPM_REGULATOR_TYPE_LDO:
		index = RPM_REGULATOR_PARAM_MODE_LDO;
		new_mode = mode_mapping[mode].ldo_mode;
		break;
	default:
		vreg_err(regulator, "unsupported regulator type: %d\n",
			regulator->rpm_vreg->regulator_type);
		return -EINVAL;
	};

	if (new_mode < params[index].min || new_mode > params[index].max) {
		vreg_err(regulator, "invalid mode requested: %d for type: %d\n",
			mode, regulator->rpm_vreg->regulator_type);
		return -EINVAL;
	}

	rpm_vreg_lock(regulator->rpm_vreg);

	prev_mode = regulator->req.param[index];
	regulator->req.param[index] = new_mode;
	regulator->req.modified |= BIT(index);

	rc = rpm_vreg_aggregate_requests(regulator);
	if (rc) {
		vreg_err(regulator, "set mode failed, rc=%d", rc);
		regulator->req.param[index] = prev_mode;
	}

	rpm_vreg_unlock(regulator->rpm_vreg);

	return rc;
}
EXPORT_SYMBOL(rpm_regulator_set_mode);

static struct regulator_ops ldo_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= rpm_vreg_is_enabled,
	.set_voltage		= rpm_vreg_set_voltage,
	.get_voltage		= rpm_vreg_get_voltage,
	.set_mode		= rpm_vreg_set_mode,
	.get_mode		= rpm_vreg_get_mode,
	.get_optimum_mode	= rpm_vreg_get_optimum_mode,
	.enable_time		= rpm_vreg_enable_time,
};

static struct regulator_ops ldo_corner_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= rpm_vreg_is_enabled,
	.set_voltage		= rpm_vreg_set_voltage_corner,
	.get_voltage		= rpm_vreg_get_voltage_corner,
	.set_mode		= rpm_vreg_set_mode,
	.get_mode		= rpm_vreg_get_mode,
	.get_optimum_mode	= rpm_vreg_get_optimum_mode,
	.enable_time		= rpm_vreg_enable_time,
};

static struct regulator_ops ldo_floor_corner_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= rpm_vreg_is_enabled,
	.set_voltage		= rpm_vreg_set_voltage_floor_corner,
	.get_voltage		= rpm_vreg_get_voltage_floor_corner,
	.set_mode		= rpm_vreg_set_mode,
	.get_mode		= rpm_vreg_get_mode,
	.get_optimum_mode	= rpm_vreg_get_optimum_mode,
	.enable_time		= rpm_vreg_enable_time,
};

static struct regulator_ops smps_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= rpm_vreg_is_enabled,
	.set_voltage		= rpm_vreg_set_voltage,
	.get_voltage		= rpm_vreg_get_voltage,
	.set_mode		= rpm_vreg_set_mode,
	.get_mode		= rpm_vreg_get_mode,
	.get_optimum_mode	= rpm_vreg_get_optimum_mode,
	.enable_time		= rpm_vreg_enable_time,
};

static struct regulator_ops smps_corner_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= rpm_vreg_is_enabled,
	.set_voltage		= rpm_vreg_set_voltage_corner,
	.get_voltage		= rpm_vreg_get_voltage_corner,
	.set_mode		= rpm_vreg_set_mode,
	.get_mode		= rpm_vreg_get_mode,
	.get_optimum_mode	= rpm_vreg_get_optimum_mode,
	.enable_time		= rpm_vreg_enable_time,
};

static struct regulator_ops smps_floor_corner_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= rpm_vreg_is_enabled,
	.set_voltage		= rpm_vreg_set_voltage_floor_corner,
	.get_voltage		= rpm_vreg_get_voltage_floor_corner,
	.set_mode		= rpm_vreg_set_mode,
	.get_mode		= rpm_vreg_get_mode,
	.get_optimum_mode	= rpm_vreg_get_optimum_mode,
	.enable_time		= rpm_vreg_enable_time,
};

static struct regulator_ops switch_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= rpm_vreg_is_enabled,
	.enable_time		= rpm_vreg_enable_time,
};

static struct regulator_ops ncp_ops = {
	.enable			= rpm_vreg_enable,
	.disable		= rpm_vreg_disable,
	.is_enabled		= rpm_vreg_is_enabled,
	.set_voltage		= rpm_vreg_set_voltage,
	.get_voltage		= rpm_vreg_get_voltage,
	.enable_time		= rpm_vreg_enable_time,
};

static struct regulator_ops *vreg_ops[] = {
	[RPM_REGULATOR_TYPE_LDO]	= &ldo_ops,
	[RPM_REGULATOR_TYPE_SMPS]	= &smps_ops,
	[RPM_REGULATOR_TYPE_VS]		= &switch_ops,
	[RPM_REGULATOR_TYPE_NCP]	= &ncp_ops,
};

static int rpm_vreg_device_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rpm_regulator *reg;
	struct rpm_vreg *rpm_vreg;

	reg = platform_get_drvdata(pdev);
	if (reg) {
		rpm_vreg = reg->rpm_vreg;
		rpm_vreg_lock(rpm_vreg);
		regulator_unregister(reg->rdev);
		list_del(&reg->list);
		kfree(reg);
		rpm_vreg_unlock(rpm_vreg);
	} else {
		dev_err(dev, "%s: drvdata missing\n", __func__);
		return -EINVAL;
	}

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int rpm_vreg_resource_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rpm_regulator *reg, *reg_temp;
	struct rpm_vreg *rpm_vreg;

	rpm_vreg = platform_get_drvdata(pdev);
	if (rpm_vreg) {
		rpm_vreg_lock(rpm_vreg);
		list_for_each_entry_safe(reg, reg_temp, &rpm_vreg->reg_list,
				list) {
			/* Only touch data for private consumers. */
			if (reg->rdev->desc == NULL) {
				list_del(&reg->list);
				kfree(reg->rdev);
				kfree(reg);
			} else {
				dev_err(dev, "%s: not all child devices have been removed\n",
					__func__);
			}
		}
		rpm_vreg_unlock(rpm_vreg);

		msm_rpm_free_request(rpm_vreg->handle_active);
		msm_rpm_free_request(rpm_vreg->handle_sleep);

		kfree(rpm_vreg);
	} else {
		dev_err(dev, "%s: drvdata missing\n", __func__);
		return -EINVAL;
	}

	platform_set_drvdata(pdev, NULL);

	return 0;
}

/*
 * This probe is called for child rpm-regulator devices which have
 * properties which are required to configure individual regulator
 * framework regulators for a given RPM regulator resource.
 */
static int rpm_vreg_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct regulator_init_data *init_data;
	struct rpm_vreg *rpm_vreg;
	struct rpm_regulator *reg;
	struct regulator_config reg_config = {};
	int rc = 0;
	int i, regulator_type;
	u32 val;

	if (!dev->of_node) {
		dev_err(dev, "%s: device tree information missing\n", __func__);
		return -ENODEV;
	}

	if (pdev->dev.parent == NULL) {
		dev_err(dev, "%s: parent device missing\n", __func__);
		return -ENODEV;
	}

	rpm_vreg = dev_get_drvdata(pdev->dev.parent);
	if (rpm_vreg == NULL) {
		dev_err(dev, "%s: rpm_vreg not found in parent device\n",
			__func__);
		return -ENODEV;
	}

	reg = kzalloc(sizeof(struct rpm_regulator), GFP_KERNEL);
	if (reg == NULL) {
		dev_err(dev, "%s: could not allocate memory for reg\n",
			__func__);
		return -ENOMEM;
	}

	regulator_type		= rpm_vreg->regulator_type;
	reg->rpm_vreg		= rpm_vreg;
	reg->rdesc.ops		= vreg_ops[regulator_type];
	reg->rdesc.owner	= THIS_MODULE;
	reg->rdesc.type		= REGULATOR_VOLTAGE;

	/*
	 * Switch to voltage corner regulator ops if qcom,use-voltage-corner
	 * is specified in the device node (SMPS and LDO only).
	 */
	if (of_property_read_bool(node, "qcom,use-voltage-corner")) {
		if (of_property_read_bool(node,
				"qcom,use-voltage-floor-corner")) {
			dev_err(dev, "%s: invalid properties: both qcom,use-voltage-corner and qcom,use-voltage-floor-corner specified\n",
				__func__);
			goto fail_free_reg;
		}

		if (regulator_type == RPM_REGULATOR_TYPE_SMPS)
			reg->rdesc.ops = &smps_corner_ops;
		else if (regulator_type == RPM_REGULATOR_TYPE_LDO)
			reg->rdesc.ops = &ldo_corner_ops;
	} else if (of_property_read_bool(node,
			"qcom,use-voltage-floor-corner")) {
		if (regulator_type == RPM_REGULATOR_TYPE_SMPS)
			reg->rdesc.ops = &smps_floor_corner_ops;
		else if (regulator_type == RPM_REGULATOR_TYPE_LDO)
			reg->rdesc.ops = &ldo_floor_corner_ops;
	}

	reg->always_send_voltage
		= of_property_read_bool(node, "qcom,always-send-voltage");
	reg->always_send_current
		= of_property_read_bool(node, "qcom,always-send-current");

	if (regulator_type == RPM_REGULATOR_TYPE_VS)
		reg->rdesc.n_voltages = 0;
	else
		reg->rdesc.n_voltages = 2;

	rc = of_property_read_u32(node, "qcom,set", &val);
	if (rc) {
		dev_err(dev, "%s: sleep set and/or active set must be configured via qcom,set property, rc=%d\n",
			__func__, rc);
		goto fail_free_reg;
	} else if (!(val & RPM_SET_CONFIG_BOTH)) {
		dev_err(dev, "%s: qcom,set=%u property is invalid\n", __func__,
			val);
		rc = -EINVAL;
		goto fail_free_reg;
	}

	reg->set_active = !!(val & RPM_SET_CONFIG_ACTIVE);
	reg->set_sleep = !!(val & RPM_SET_CONFIG_SLEEP);

	init_data = of_get_regulator_init_data(dev, node);
	if (init_data == NULL) {
		dev_err(dev, "%s: unable to allocate memory\n", __func__);
		rc = -ENOMEM;
		goto fail_free_reg;
	}
	if (init_data->constraints.name == NULL) {
		dev_err(dev, "%s: regulator name not specified\n", __func__);
		rc = -EINVAL;
		goto fail_free_reg;
	}

	init_data->constraints.input_uV	= init_data->constraints.max_uV;

	if (of_get_property(node, "parent-supply", NULL))
		init_data->supply_regulator = "parent";

	/*
	 * Fill in ops and mode masks based on callbacks specified for
	 * this type of regulator.
	 */
	if (reg->rdesc.ops->enable)
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;
	if (reg->rdesc.ops->get_voltage)
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE;
	if (reg->rdesc.ops->get_mode) {
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_DRMS;
		init_data->constraints.valid_modes_mask
			|= REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE;
	}

	reg->rdesc.name		= init_data->constraints.name;
	reg->min_uV		= init_data->constraints.min_uV;
	reg->max_uV		= init_data->constraints.max_uV;

	/* Initialize the param array based on optional properties. */
	for (i = 0; i < RPM_REGULATOR_PARAM_MAX; i++) {
		rc = of_property_read_u32(node, params[i].property_name, &val);
		if (rc == 0) {
			if (params[i].supported_regulator_types
					& BIT(regulator_type)) {
				if (val < params[i].min
						|| val > params[i].max) {
					pr_warn("%s: device tree property: %s=%u is outsided allowed range [%u, %u]\n",
						reg->rdesc.name,
						params[i].property_name, val,
						params[i].min, params[i].max);
					continue;
				}
				reg->req.param[i] = val;
				reg->req.modified |= BIT(i);
			} else {
				pr_warn("%s: regulator type=%d does not support device tree property: %s\n",
					reg->rdesc.name, regulator_type,
					params[i].property_name);
			}
		}
	}

	of_property_read_u32(node, "qcom,system-load", &reg->system_load);

	rc = rpm_vreg_configure_pin_control_enable(reg, node);
	if (rc) {
		vreg_err(reg, "could not configure pin control enable, rc=%d\n",
			rc);
		goto fail_free_reg;
	}

	rpm_vreg_lock(rpm_vreg);
	list_add(&reg->list, &rpm_vreg->reg_list);
	rpm_vreg_unlock(rpm_vreg);

	if (of_property_read_bool(node, "qcom,send-defaults")) {
		rc = rpm_vreg_send_defaults(reg);
		if (rc) {
			vreg_err(reg, "could not send defaults, rc=%d\n", rc);
			goto fail_remove_from_list;
		}
	}

	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.of_node = node;
	reg_config.driver_data = reg;
	reg->rdev = regulator_register(&reg->rdesc, &reg_config);
	if (IS_ERR(reg->rdev)) {
		rc = PTR_ERR(reg->rdev);
		reg->rdev = NULL;
		pr_err("regulator_register failed: %s, rc=%d\n",
			reg->rdesc.name, rc);
		goto fail_remove_from_list;
	}

	platform_set_drvdata(pdev, reg);

	pr_debug("successfully probed: %s\n", reg->rdesc.name);

	return 0;

fail_remove_from_list:
	rpm_vreg_lock(rpm_vreg);
	list_del(&reg->list);
	rpm_vreg_unlock(rpm_vreg);

fail_free_reg:
	kfree(reg);
	return rc;
}

/*
 * This probe is called for parent rpm-regulator devices which have
 * properties which are required to identify a given RPM resource.
 */
static int rpm_vreg_resource_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct rpm_vreg *rpm_vreg;
	int val = 0;
	u32 resource_type;
	int rc;

	if (!dev->of_node) {
		dev_err(dev, "%s: device tree information missing\n", __func__);
		return -ENODEV;
	}

	/* Create new rpm_vreg entry. */
	rpm_vreg = kzalloc(sizeof(struct rpm_vreg), GFP_KERNEL);
	if (rpm_vreg == NULL) {
		dev_err(dev, "%s: could not allocate memory for vreg\n",
			__func__);
		return -ENOMEM;
	}

	/* Required device tree properties: */
	rc = of_property_read_string(node, "qcom,resource-name",
			&rpm_vreg->resource_name);
	if (rc) {
		dev_err(dev, "%s: qcom,resource-name missing in DT node\n",
			__func__);
		goto fail_free_vreg;
	}
	resource_type = rpm_vreg_string_to_int(rpm_vreg->resource_name);

	rc = of_property_read_u32(node, "qcom,resource-id",
			&rpm_vreg->resource_id);
	if (rc) {
		dev_err(dev, "%s: qcom,resource-id missing in DT node\n",
			__func__);
		goto fail_free_vreg;
	}

	rc = of_property_read_u32(node, "qcom,regulator-type",
			&rpm_vreg->regulator_type);
	if (rc) {
		dev_err(dev, "%s: qcom,regulator-type missing in DT node\n",
			__func__);
		goto fail_free_vreg;
	}

	if ((rpm_vreg->regulator_type < 0)
	    || (rpm_vreg->regulator_type >= RPM_REGULATOR_TYPE_MAX)) {
		dev_err(dev, "%s: invalid regulator type: %d\n", __func__,
			rpm_vreg->regulator_type);
		rc = -EINVAL;
		goto fail_free_vreg;
	}

	/* Optional device tree properties: */
	of_property_read_u32(node, "qcom,allow-atomic", &val);
	rpm_vreg->allow_atomic = !!val;
	of_property_read_u32(node, "qcom,enable-time", &rpm_vreg->enable_time);
	of_property_read_u32(node, "qcom,hpm-min-load",
		&rpm_vreg->hpm_min_load);
	rpm_vreg->apps_only = of_property_read_bool(node, "qcom,apps-only");

	rpm_vreg->handle_active = msm_rpm_create_request(RPM_SET_ACTIVE,
		resource_type, rpm_vreg->resource_id, RPM_REGULATOR_PARAM_MAX);
	if (rpm_vreg->handle_active == NULL
	    || IS_ERR(rpm_vreg->handle_active)) {
		rc = PTR_ERR(rpm_vreg->handle_active);
		dev_err(dev, "%s: failed to create active RPM handle, rc=%d\n",
			__func__, rc);
		goto fail_free_vreg;
	}

	rpm_vreg->handle_sleep = msm_rpm_create_request(RPM_SET_SLEEP,
		resource_type, rpm_vreg->resource_id, RPM_REGULATOR_PARAM_MAX);
	if (rpm_vreg->handle_sleep == NULL || IS_ERR(rpm_vreg->handle_sleep)) {
		rc = PTR_ERR(rpm_vreg->handle_sleep);
		dev_err(dev, "%s: failed to create sleep RPM handle, rc=%d\n",
			__func__, rc);
		goto fail_free_handle_active;
	}

	INIT_LIST_HEAD(&rpm_vreg->reg_list);

	if (rpm_vreg->allow_atomic)
		spin_lock_init(&rpm_vreg->slock);
	else
		mutex_init(&rpm_vreg->mlock);

	platform_set_drvdata(pdev, rpm_vreg);

	rc = of_platform_populate(node, NULL, NULL, dev);
	if (rc) {
		dev_err(dev, "%s: failed to add child nodes, rc=%d\n", __func__,
			rc);
		goto fail_unset_drvdata;
	}

	pr_debug("successfully probed: %s (%08X) %u\n", rpm_vreg->resource_name,
		resource_type, rpm_vreg->resource_id);

	return rc;

fail_unset_drvdata:
	platform_set_drvdata(pdev, NULL);
	msm_rpm_free_request(rpm_vreg->handle_sleep);

fail_free_handle_active:
	msm_rpm_free_request(rpm_vreg->handle_active);

fail_free_vreg:
	kfree(rpm_vreg);

	return rc;
}

static struct of_device_id rpm_vreg_match_table_device[] = {
	{ .compatible = "qcom,rpm-smd-regulator", },
	{}
};

static struct of_device_id rpm_vreg_match_table_resource[] = {
	{ .compatible = "qcom,rpm-smd-regulator-resource", },
	{}
};

static struct platform_driver rpm_vreg_device_driver = {
	.probe = rpm_vreg_device_probe,
	.remove = rpm_vreg_device_remove,
	.driver = {
		.name = "qcom,rpm-smd-regulator",
		.owner = THIS_MODULE,
		.of_match_table = rpm_vreg_match_table_device,
	},
};

static struct platform_driver rpm_vreg_resource_driver = {
	.probe = rpm_vreg_resource_probe,
	.remove = rpm_vreg_resource_remove,
	.driver = {
		.name = "qcom,rpm-smd-regulator-resource",
		.owner = THIS_MODULE,
		.of_match_table = rpm_vreg_match_table_resource,
	},
};

/**
 * rpm_smd_regulator_driver_init() - initialize the RPM SMD regulator drivers
 *
 * This function registers the RPM SMD regulator platform drivers.
 *
 * Returns 0 on success or errno on failure.
 */
int __init rpm_smd_regulator_driver_init(void)
{
	static bool initialized;
	int i, rc;

	if (initialized)
		return 0;
	else
		initialized = true;

	/* Store parameter string names as integers */
	for (i = 0; i < RPM_REGULATOR_PARAM_MAX; i++)
		params[i].key = rpm_vreg_string_to_int(params[i].name);

	rc = platform_driver_register(&rpm_vreg_device_driver);
	if (rc)
		return rc;

	return platform_driver_register(&rpm_vreg_resource_driver);
}
EXPORT_SYMBOL(rpm_smd_regulator_driver_init);

static void __exit rpm_vreg_exit(void)
{
	platform_driver_unregister(&rpm_vreg_device_driver);
	platform_driver_unregister(&rpm_vreg_resource_driver);
}

arch_initcall(rpm_smd_regulator_driver_init);
module_exit(rpm_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM RPM SMD regulator driver");
