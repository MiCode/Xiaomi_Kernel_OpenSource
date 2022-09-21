// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm:%s:%d]: " fmt, __func__, __LINE__

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>

#include <linux/sde_io_util.h>
#include <linux/sde_rsc.h>

#include "sde_power_handle.h"
#include "sde_trace.h"
#include "sde_dbg.h"

#define KBPS2BPS(x) ((x) * 1000ULL)

static const char *data_bus_name[SDE_POWER_HANDLE_DBUS_ID_MAX] = {
	[SDE_POWER_HANDLE_DBUS_ID_MNOC] = "qcom,sde-data-bus",
	[SDE_POWER_HANDLE_DBUS_ID_LLCC] = "qcom,sde-llcc-bus",
	[SDE_POWER_HANDLE_DBUS_ID_EBI] = "qcom,sde-ebi-bus",
};

const char *sde_power_handle_get_dbus_name(u32 bus_id)
{
	if (bus_id < SDE_POWER_HANDLE_DBUS_ID_MAX)
		return data_bus_name[bus_id];

	return NULL;
}

static void sde_power_event_trigger_locked(struct sde_power_handle *phandle,
		u32 event_type)
{
	struct sde_power_event *event;

	phandle->last_event_handled = event_type;
	list_for_each_entry(event, &phandle->event_list, list) {
		if (event->event_type & event_type) {
			event->cb_fnc(event_type, event->usr);
		}
	}
}

static inline void sde_power_rsc_client_init(struct sde_power_handle *phandle)
{
	/* creates the rsc client */
	if (!phandle->rsc_client_init) {
		phandle->rsc_client = sde_rsc_client_create(SDE_RSC_INDEX,
				"sde_power_handle", SDE_RSC_CLK_CLIENT, 0);
		if (IS_ERR_OR_NULL(phandle->rsc_client)) {
			pr_debug("sde rsc client create failed :%ld\n",
						PTR_ERR(phandle->rsc_client));
			phandle->rsc_client = NULL;
		}
		phandle->rsc_client_init = true;
	}
}

static int sde_power_rsc_update(struct sde_power_handle *phandle, bool enable)
{
	u32 rsc_state;
	int ret = 0;

	rsc_state = enable ? SDE_RSC_CLK_STATE : SDE_RSC_IDLE_STATE;

	if (phandle->rsc_client)
		ret = sde_rsc_client_state_update(phandle->rsc_client,
			rsc_state, NULL, SDE_RSC_INVALID_CRTC_ID, NULL);

	return ret;
}

static int sde_power_parse_dt_supply(struct platform_device *pdev,
				struct dss_module_power *mp)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *of_node = NULL, *supply_root_node = NULL;
	struct device_node *supply_node = NULL;

	if (!pdev || !mp) {
		pr_err("invalid input param pdev:%pK mp:%pK\n", pdev, mp);
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;

	mp->num_vreg = 0;
	supply_root_node = of_get_child_by_name(of_node,
						"qcom,platform-supply-entries");
	if (!supply_root_node) {
		pr_debug("no supply entry present\n");
		return rc;
	}

	for_each_child_of_node(supply_root_node, supply_node)
		mp->num_vreg++;

	if (mp->num_vreg == 0) {
		pr_debug("no vreg\n");
		return rc;
	}

	pr_debug("vreg found. count=%d\n", mp->num_vreg);
	mp->vreg_config = devm_kzalloc(&pdev->dev, sizeof(struct dss_vreg) *
						mp->num_vreg, GFP_KERNEL);
	if (!mp->vreg_config) {
		rc = -ENOMEM;
		return rc;
	}

	for_each_child_of_node(supply_root_node, supply_node) {

		const char *st = NULL;

		rc = of_property_read_string(supply_node,
						"qcom,supply-name", &st);
		if (rc) {
			pr_err("error reading name. rc=%d\n", rc);
			goto error;
		}

		strlcpy(mp->vreg_config[i].vreg_name, st,
					sizeof(mp->vreg_config[i].vreg_name));

		rc = of_property_read_u32(supply_node,
					"qcom,supply-min-voltage", &tmp);
		if (rc) {
			pr_err("error reading min volt. rc=%d\n", rc);
			goto error;
		}
		mp->vreg_config[i].min_voltage = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-max-voltage", &tmp);
		if (rc) {
			pr_err("error reading max volt. rc=%d\n", rc);
			goto error;
		}
		mp->vreg_config[i].max_voltage = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-enable-load", &tmp);
		if (rc) {
			pr_err("error reading enable load. rc=%d\n", rc);
			goto error;
		}
		mp->vreg_config[i].enable_load = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-disable-load", &tmp);
		if (rc) {
			pr_err("error reading disable load. rc=%d\n", rc);
			goto error;
		}
		mp->vreg_config[i].disable_load = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-pre-on-sleep", &tmp);
		if (rc)
			pr_debug("error reading supply pre sleep value. rc=%d\n",
							rc);

		mp->vreg_config[i].pre_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-pre-off-sleep", &tmp);
		if (rc)
			pr_debug("error reading supply pre sleep value. rc=%d\n",
							rc);

		mp->vreg_config[i].pre_off_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-post-on-sleep", &tmp);
		if (rc)
			pr_debug("error reading supply post sleep value. rc=%d\n",
							rc);

		mp->vreg_config[i].post_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-post-off-sleep", &tmp);
		if (rc)
			pr_debug("error reading supply post sleep value. rc=%d\n",
							rc);

		mp->vreg_config[i].post_off_sleep = (!rc ? tmp : 0);

		pr_debug("%s min=%d, max=%d, enable=%d, disable=%d, preonsleep=%d, postonsleep=%d, preoffsleep=%d, postoffsleep=%d\n",
					mp->vreg_config[i].vreg_name,
					mp->vreg_config[i].min_voltage,
					mp->vreg_config[i].max_voltage,
					mp->vreg_config[i].enable_load,
					mp->vreg_config[i].disable_load,
					mp->vreg_config[i].pre_on_sleep,
					mp->vreg_config[i].post_on_sleep,
					mp->vreg_config[i].pre_off_sleep,
					mp->vreg_config[i].post_off_sleep);
		++i;

		rc = 0;
	}

	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(&pdev->dev, mp->vreg_config);
		mp->vreg_config = NULL;
		mp->num_vreg = 0;
	}

	return rc;
}

static int sde_power_parse_dt_clock(struct platform_device *pdev,
					struct dss_module_power *mp)
{
	u32 i = 0, rc = 0;
	const char *clock_name;
	u32 clock_rate = 0;
	u32 clock_max_rate = 0;
	int num_clk = 0;

	if (!pdev || !mp) {
		pr_err("invalid input param pdev:%pK mp:%pK\n", pdev, mp);
		return -EINVAL;
	}

	mp->num_clk = 0;
	num_clk = of_property_count_strings(pdev->dev.of_node,
							"clock-names");
	if (num_clk <= 0) {
		pr_debug("clocks are not defined\n");
		goto clk_err;
	}

	mp->num_clk = num_clk;
	mp->clk_config = devm_kzalloc(&pdev->dev,
			sizeof(struct dss_clk) * num_clk, GFP_KERNEL);
	if (!mp->clk_config) {
		rc = -ENOMEM;
		mp->num_clk = 0;
		goto clk_err;
	}

	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(pdev->dev.of_node, "clock-names",
							i, &clock_name);
		strlcpy(mp->clk_config[i].clk_name, clock_name,
				sizeof(mp->clk_config[i].clk_name));

		of_property_read_u32_index(pdev->dev.of_node, "clock-rate",
							i, &clock_rate);
		mp->clk_config[i].rate = clock_rate;

		if (!clock_rate)
			mp->clk_config[i].type = DSS_CLK_AHB;
		else
			mp->clk_config[i].type = DSS_CLK_PCLK;

		clock_max_rate = 0;
		of_property_read_u32_index(pdev->dev.of_node, "clock-max-rate",
							i, &clock_max_rate);
		mp->clk_config[i].max_rate = clock_max_rate;
	}

clk_err:
	return rc;
}

#define MAX_AXI_PORT_COUNT 3

static int _sde_power_data_bus_set_quota(
	struct sde_power_data_bus_handle *pdbus,
	u64 in_ab_quota, u64 in_ib_quota)
{
	int rc = 0, i = 0;
	u32 paths = pdbus->data_paths_cnt;

	if (!paths || paths > DATA_BUS_PATH_MAX) {
		pr_err("invalid data bus handle, paths %d\n", paths);
		return -EINVAL;
	}

	in_ab_quota = div_u64(in_ab_quota, paths);

	SDE_ATRACE_BEGIN("msm_bus_scale_req");
	for (i = 0; i < paths; i++) {
		if (pdbus->data_bus_hdl[i]) {
			rc = icc_set_bw(pdbus->data_bus_hdl[i],
				kBps_to_icc(div_u64(in_ab_quota, 1000)),
				kBps_to_icc(div_u64(in_ib_quota, 1000)));
			if (rc)
				goto err;
		}
	}

	pdbus->curr_val.ab = in_ab_quota;
	pdbus->curr_val.ib = in_ib_quota;

	SDE_ATRACE_END("msm_bus_scale_req");

	return rc;
err:
	for (; i >= 0; --i)
		if (pdbus->data_bus_hdl[i])
			icc_set_bw(pdbus->data_bus_hdl[i],
				kBps_to_icc(div_u64(pdbus->curr_val.ab, 1000)),
				kBps_to_icc(div_u64(pdbus->curr_val.ib, 1000)));

	SDE_ATRACE_END("msm_bus_scale_req");
	pr_err("failed to set data bus vote ab=%llu ib=%llu rc=%d\n",
	       rc, in_ab_quota, in_ib_quota);

	return rc;
}

int sde_power_data_bus_set_quota(struct sde_power_handle *phandle,
	u32 bus_id, u64 ab_quota, u64 ib_quota)
{
	int rc = 0;
	u32 paths;

	if (!phandle || bus_id >= SDE_POWER_HANDLE_DBUS_ID_MAX) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	paths = phandle->data_bus_handle[bus_id].data_paths_cnt;
	if (!paths)
		goto skip_vote;

	trace_sde_perf_update_bus(bus_id, ab_quota, ib_quota, paths);

	mutex_lock(&phandle->phandle_lock);
	rc = _sde_power_data_bus_set_quota(&phandle->data_bus_handle[bus_id],
			ab_quota, ib_quota);
	mutex_unlock(&phandle->phandle_lock);

skip_vote:
	pr_debug("bus=%d, ab=%llu, ib=%llu, paths=%d\n", bus_id, ab_quota,
			ib_quota, paths);

	return rc;
}

/**
 * sde_power_icc_get - get the interconnect path for the given bus_name
 * @pdev - platform device
 * @bus_name - bus name for the corresponding interconnect
 * @path - the icc_path object we want to obtain for this @bus_name (output)
 * @count - if given, incremented only if the path was successfully retrieved
 **/
static int sde_power_icc_get(struct platform_device *pdev,
	const char *bus_name, struct icc_path **path, u32 *count)
{
	int rc = of_property_match_string(pdev->dev.of_node,
			"interconnect-names", bus_name);

	/* bus_names are optional for any given device node, skip if missing */
	if (rc < 0)
		goto end;

	*path = of_icc_get(&pdev->dev, bus_name);
	if (IS_ERR_OR_NULL(*path)) {
		rc = PTR_ERR(*path);
		pr_err("bus %s parsing failed, rc:%d\n", bus_name, rc);
		*path = NULL;
		return rc;
	}

	if (count)
		(*count)++;

end:
	pr_debug("bus %s dt node %s(%d), icc_path is %s, count:%d\n",
			bus_name, rc < 0 ? "missing" : "found", rc,
			*path ? "valid" : "NULL", count ? *count : -1);
	return 0;
}

static int sde_power_reg_bus_parse(struct platform_device *pdev,
		struct sde_power_reg_bus_handle *reg_bus)
{
	const char *bus_name = "qcom,sde-reg-bus";
	const u32 *vec_arr = NULL;
	int rc, len, i, vec_idx = 0;
	u32 paths = 0;

	rc = sde_power_icc_get(pdev, bus_name, &reg_bus->reg_bus_hdl, &paths);
	if (rc)
		return rc;

	if (!paths) {
		pr_debug("%s not defined for pdev %s\n", bus_name, pdev->name ?
				pdev->name : "<unknown>");
		return 0;
	}

	vec_arr = of_get_property(pdev->dev.of_node,
			"qcom,sde-reg-bus,vectors-KBps", &len);
	if (!vec_arr) {
		pr_err("%s scale table property not found\n", bus_name);
		return -EINVAL;
	}

	if (len / sizeof(*vec_arr) != VOTE_INDEX_MAX * 2) {
		pr_err("wrong size for %s vector table\n", bus_name);
		return -EINVAL;
	}

	for (i = 0; i < VOTE_INDEX_MAX; ++i) {
		reg_bus->scale_table[i].ab = (u64)KBPS2BPS(be32_to_cpu(
				vec_arr[vec_idx++]));
		reg_bus->scale_table[i].ib = (u64)KBPS2BPS(be32_to_cpu(
				vec_arr[vec_idx++]));
	}

	return rc;
}

static int sde_power_mnoc_bus_parse(struct platform_device *pdev,
	struct sde_power_data_bus_handle *pdbus, const char *name)
{
	int i, rc = 0;
	char bus_name[32];

	for (i = 0; i < DATA_BUS_PATH_MAX; ++i) {
		snprintf(bus_name, sizeof(bus_name), "%s%d", name, i);
		rc = sde_power_icc_get(pdev, bus_name, &pdbus->data_bus_hdl[i],
				&pdbus->data_paths_cnt);
		if (rc)
			break;
	}

	/* at least one databus path is required */
	if (!pdbus->data_paths_cnt) {
		pr_info("mnoc interconnect path(s) not defined, rc: %d\n", rc);
	} else if (rc) {
		pr_info("ignoring error %d for non-primary data path\n", rc);
		rc = 0;
	}

	return rc;
}

static int sde_power_bus_parse(struct platform_device *pdev,
	struct sde_power_handle *phandle)
{
	int i, j, rc = 0;
	bool active_only = false;
	struct sde_power_data_bus_handle *pdbus = phandle->data_bus_handle;

	/* reg bus */
	rc = sde_power_reg_bus_parse(pdev, &phandle->reg_bus_handle);
	if (rc)
		return rc;

	/* data buses */
	if (of_find_property(pdev->dev.of_node,
			"qcom,msm-bus,active-only", NULL))
		active_only = true;

	for (i = SDE_POWER_HANDLE_DBUS_ID_MNOC;
			i < SDE_POWER_HANDLE_DBUS_ID_MAX; ++i) {
		if (i == SDE_POWER_HANDLE_DBUS_ID_MNOC)
			rc = sde_power_mnoc_bus_parse(pdev, &pdbus[i],
					data_bus_name[i]);
		else
			rc = sde_power_icc_get(pdev, data_bus_name[i],
					&pdbus[i].data_bus_hdl[0],
					&pdbus[i].data_paths_cnt);

		if (rc)
			break;

		if (active_only) {
			pdbus[i].bus_active_only = true;
			for (j = 0; j < pdbus[i].data_paths_cnt; ++j)
				icc_set_tag(pdbus[i].data_bus_hdl[j],
						QCOM_ICC_TAG_ACTIVE_ONLY);
		}

		pr_debug("found %d paths for %s\n", pdbus[i].data_paths_cnt,
				data_bus_name[i]);
	}

	return rc;
}

static void sde_power_bus_unregister(struct sde_power_handle *phandle)
{
	int i, j;
	struct sde_power_reg_bus_handle *reg_bus = &phandle->reg_bus_handle;
	struct sde_power_data_bus_handle *pdbus = phandle->data_bus_handle;

	icc_put(reg_bus->reg_bus_hdl);
	reg_bus->reg_bus_hdl = NULL;

	for (i = SDE_POWER_HANDLE_DBUS_ID_MAX - 1;
			i >= SDE_POWER_HANDLE_DBUS_ID_MNOC; i--) {
		for (j = 0; j < pdbus[i].data_paths_cnt; j++) {
			if (pdbus[i].data_bus_hdl[j]) {
				icc_put(pdbus[i].data_bus_hdl[j]);
				pdbus[i].data_bus_hdl[j] = NULL;
			}
		}
	}
}

static int sde_power_reg_bus_update(struct sde_power_reg_bus_handle *reg_bus,
	u32 usecase_ndx)
{
	int rc = 0;
	u64 ab_quota, ib_quota;

	ab_quota = reg_bus->scale_table[usecase_ndx].ab;
	ib_quota = reg_bus->scale_table[usecase_ndx].ib;

	if (reg_bus->reg_bus_hdl) {
		SDE_ATRACE_BEGIN("msm_bus_scale_req");
		rc = icc_set_bw(reg_bus->reg_bus_hdl,
				kBps_to_icc(div_u64(ab_quota, 1000)),
				kBps_to_icc(div_u64(ib_quota, 1000)));
		SDE_ATRACE_END("msm_bus_scale_req");
	}

	if (rc)
		pr_err("failed to set reg bus vote to index %d, rc=%d\n",
				usecase_ndx, rc);
	else {
		reg_bus->curr_idx = usecase_ndx;
		pr_debug("reg-bus vote set to index=%d, ab=%llu, ib=%llu\n",
				usecase_ndx, ab_quota, ib_quota);
	}

	return rc;
}

int sde_power_resource_init(struct platform_device *pdev,
	struct sde_power_handle *phandle)
{
	int rc = 0;
	struct dss_module_power *mp;

	if (!phandle || !pdev) {
		pr_err("invalid input param\n");
		rc = -EINVAL;
		goto end;
	}
	mp = &phandle->mp;
	phandle->dev = &pdev->dev;

	rc = sde_power_parse_dt_clock(pdev, mp);
	if (rc) {
		pr_err("device clock parsing failed\n");
		goto end;
	}

	rc = sde_power_parse_dt_supply(pdev, mp);
	if (rc) {
		pr_err("device vreg supply parsing failed\n");
		goto parse_vreg_err;
	}

	rc = msm_dss_get_vreg(&pdev->dev,
				mp->vreg_config, mp->num_vreg, 1);
	if (rc) {
		pr_err("get config failed rc=%d\n", rc);
		goto vreg_err;
	}

	rc = msm_dss_get_clk(&pdev->dev, mp->clk_config, mp->num_clk);
	if (rc) {
		pr_err("clock get failed rc=%d\n", rc);
		goto clkget_err;
	}

	rc = msm_dss_clk_set_rate(mp->clk_config, mp->num_clk);
	if (rc) {
		pr_err("clock set rate failed rc=%d\n", rc);
		goto clkset_err;
	}

	rc = sde_power_bus_parse(pdev, phandle);
	if (rc) {
		pr_err("bus parse failed rc=%d\n", rc);
		goto bus_err;
	}

	INIT_LIST_HEAD(&phandle->event_list);

	phandle->rsc_client = NULL;
	phandle->rsc_client_init = false;

	mutex_init(&phandle->phandle_lock);

	return rc;

bus_err:
	sde_power_bus_unregister(phandle);
clkset_err:
	msm_dss_put_clk(mp->clk_config, mp->num_clk);
clkget_err:
	msm_dss_get_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);
vreg_err:
	if (mp->vreg_config)
		devm_kfree(&pdev->dev, mp->vreg_config);
	mp->num_vreg = 0;
parse_vreg_err:
	if (mp->clk_config)
		devm_kfree(&pdev->dev, mp->clk_config);
	mp->num_clk = 0;
end:
	return rc;
}

void sde_power_resource_deinit(struct platform_device *pdev,
	struct sde_power_handle *phandle)
{
	struct dss_module_power *mp;
	struct sde_power_event *curr_event, *next_event;

	if (!phandle || !pdev) {
		pr_err("invalid input param\n");
		return;
	}
	mp = &phandle->mp;

	mutex_lock(&phandle->phandle_lock);
	list_for_each_entry_safe(curr_event, next_event,
			&phandle->event_list, list) {
		pr_err("event:%d, client:%s still registered\n",
				curr_event->event_type,
				curr_event->client_name);
		curr_event->active = false;
		list_del(&curr_event->list);
	}
	mutex_unlock(&phandle->phandle_lock);

	sde_power_bus_unregister(phandle);

	msm_dss_put_clk(mp->clk_config, mp->num_clk);

	msm_dss_get_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);

	if (mp->clk_config)
		devm_kfree(&pdev->dev, mp->clk_config);

	if (mp->vreg_config)
		devm_kfree(&pdev->dev, mp->vreg_config);

	mp->num_vreg = 0;
	mp->num_clk = 0;

	if (phandle->rsc_client)
		sde_rsc_client_destroy(phandle->rsc_client);
}

int sde_power_scale_reg_bus(struct sde_power_handle *phandle,
	u32 usecase_ndx, bool skip_lock)
{
	int rc = 0;

	if (!phandle->reg_bus_handle.reg_bus_hdl)
		return 0;

	if (!skip_lock)
		mutex_lock(&phandle->phandle_lock);

	pr_debug("%pS: requested:%d\n",
		__builtin_return_address(0), usecase_ndx);

	rc = sde_power_reg_bus_update(&phandle->reg_bus_handle,
						usecase_ndx);

	if (!skip_lock)
		mutex_unlock(&phandle->phandle_lock);

	return rc;
}

int sde_power_resource_enable(struct sde_power_handle *phandle, bool enable)
{
	int rc = 0, i = 0;
	struct dss_module_power *mp;

	if (!phandle) {
		pr_err("invalid input argument\n");
		return -EINVAL;
	}

	mp = &phandle->mp;

	mutex_lock(&phandle->phandle_lock);

	pr_debug("enable:%d\n", enable);

	SDE_ATRACE_BEGIN("sde_power_resource_enable");

	/* RSC client init */
	sde_power_rsc_client_init(phandle);

	if (enable) {
		sde_power_event_trigger_locked(phandle,
				SDE_POWER_EVENT_PRE_ENABLE);

		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX &&
			phandle->data_bus_handle[i].data_paths_cnt > 0; i++) {
			rc = _sde_power_data_bus_set_quota(
				&phandle->data_bus_handle[i],
				SDE_POWER_HANDLE_ENABLE_BUS_AB_QUOTA,
				SDE_POWER_HANDLE_ENABLE_BUS_IB_QUOTA);
			if (rc) {
				pr_err("failed to set data bus vote id=%d rc=%d\n",
						i, rc);
				goto vreg_err;
			}
		}
		rc = msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg,
				enable);
		if (rc) {
			pr_err("failed to enable vregs rc=%d\n", rc);
			goto vreg_err;
		}

		rc = sde_power_scale_reg_bus(phandle, VOTE_INDEX_LOW, true);
		if (rc) {
			pr_err("failed to set reg bus vote rc=%d\n", rc);
			goto reg_bus_hdl_err;
		}

		SDE_EVT32_VERBOSE(enable, SDE_EVTLOG_FUNC_CASE1);
		rc = sde_power_rsc_update(phandle, true);
		if (rc) {
			pr_err("failed to update rsc\n");
			goto rsc_err;
		}

		rc = msm_dss_enable_clk(mp->clk_config, mp->num_clk, enable);
		if (rc) {
			pr_err("clock enable failed rc:%d\n", rc);
			goto clk_err;
		}

		sde_power_event_trigger_locked(phandle,
				SDE_POWER_EVENT_POST_ENABLE);

	} else {
		sde_power_event_trigger_locked(phandle,
				SDE_POWER_EVENT_PRE_DISABLE);

		SDE_EVT32_VERBOSE(enable, SDE_EVTLOG_FUNC_CASE2);
		sde_power_rsc_update(phandle, false);

		msm_dss_enable_clk(mp->clk_config, mp->num_clk, enable);

		sde_power_scale_reg_bus(phandle, VOTE_INDEX_DISABLE, true);

		msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg, enable);

		for (i = SDE_POWER_HANDLE_DBUS_ID_MAX - 1; i >= 0; i--)
			if (phandle->data_bus_handle[i].data_paths_cnt > 0)
				_sde_power_data_bus_set_quota(
					&phandle->data_bus_handle[i],
					SDE_POWER_HANDLE_DISABLE_BUS_AB_QUOTA,
					SDE_POWER_HANDLE_DISABLE_BUS_IB_QUOTA);

		sde_power_event_trigger_locked(phandle,
				SDE_POWER_EVENT_POST_DISABLE);
	}

	SDE_EVT32_VERBOSE(enable, SDE_EVTLOG_FUNC_EXIT);
	SDE_ATRACE_END("sde_power_resource_enable");
	mutex_unlock(&phandle->phandle_lock);
	return rc;

clk_err:
	sde_power_rsc_update(phandle, false);
rsc_err:
	sde_power_scale_reg_bus(phandle, VOTE_INDEX_DISABLE, true);
reg_bus_hdl_err:
	msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg, 0);
vreg_err:
	for (i-- ; i >= 0 && phandle->data_bus_handle[i].data_paths_cnt > 0; i--)
		_sde_power_data_bus_set_quota(
			&phandle->data_bus_handle[i],
			SDE_POWER_HANDLE_DISABLE_BUS_AB_QUOTA,
			SDE_POWER_HANDLE_DISABLE_BUS_IB_QUOTA);
	SDE_ATRACE_END("sde_power_resource_enable");
	mutex_unlock(&phandle->phandle_lock);
	return rc;
}

int sde_power_clk_set_rate(struct sde_power_handle *phandle, char *clock_name,
	u64 rate)
{
	int i, rc = -EINVAL;
	struct dss_module_power *mp;

	if (!phandle) {
		pr_err("invalid input power handle\n");
		return -EINVAL;
	}

	mutex_lock(&phandle->phandle_lock);
	if (phandle->last_event_handled & SDE_POWER_EVENT_POST_DISABLE) {
		pr_debug("invalid power state %u\n",
				phandle->last_event_handled);
		SDE_EVT32(phandle->last_event_handled, SDE_EVTLOG_ERROR);
		mutex_unlock(&phandle->phandle_lock);
		return -EINVAL;
	}

	mp = &phandle->mp;

	for (i = 0; i < mp->num_clk; i++) {
		if (!strcmp(mp->clk_config[i].clk_name, clock_name)) {
			if (mp->clk_config[i].max_rate &&
					(rate > mp->clk_config[i].max_rate))
				rate = mp->clk_config[i].max_rate;

			mp->clk_config[i].rate = rate;
			rc = msm_dss_single_clk_set_rate(&mp->clk_config[i]);
			break;
		}
	}
	mutex_unlock(&phandle->phandle_lock);

	return rc;
}

u64 sde_power_clk_get_rate(struct sde_power_handle *phandle, char *clock_name)
{
	int i;
	struct dss_module_power *mp;
	u64 rate = -EINVAL;

	if (!phandle) {
		pr_err("invalid input power handle\n");
		return -EINVAL;
	}
	mp = &phandle->mp;

	for (i = 0; i < mp->num_clk; i++) {
		if (!strcmp(mp->clk_config[i].clk_name, clock_name)) {
			rate = clk_get_rate(mp->clk_config[i].clk);
			break;
		}
	}

	return rate;
}

u64 sde_power_clk_get_max_rate(struct sde_power_handle *phandle,
		char *clock_name)
{
	int i;
	struct dss_module_power *mp;
	u64 rate = 0;

	if (!phandle) {
		pr_err("invalid input power handle\n");
		return 0;
	}
	mp = &phandle->mp;

	for (i = 0; i < mp->num_clk; i++) {
		if (!strcmp(mp->clk_config[i].clk_name, clock_name)) {
			rate = mp->clk_config[i].max_rate;
			break;
		}
	}

	return rate;
}

struct clk *sde_power_clk_get_clk(struct sde_power_handle *phandle,
		char *clock_name)
{
	int i;
	struct dss_module_power *mp;
	struct clk *clk = NULL;

	if (!phandle) {
		pr_err("invalid input power handle\n");
		return 0;
	}
	mp = &phandle->mp;

	for (i = 0; i < mp->num_clk; i++) {
		if (!strcmp(mp->clk_config[i].clk_name, clock_name)) {
			clk = mp->clk_config[i].clk;
			break;
		}
	}

	return clk;
}

struct sde_power_event *sde_power_handle_register_event(
		struct sde_power_handle *phandle,
		u32 event_type, void (*cb_fnc)(u32 event_type, void *usr),
		void *usr, char *client_name)
{
	struct sde_power_event *event;

	if (!phandle) {
		pr_err("invalid power handle\n");
		return ERR_PTR(-EINVAL);
	} else if (!cb_fnc || !event_type) {
		pr_err("no callback fnc or event type\n");
		return ERR_PTR(-EINVAL);
	}

	event = kzalloc(sizeof(struct sde_power_event), GFP_KERNEL);
	if (!event)
		return ERR_PTR(-ENOMEM);

	event->event_type = event_type;
	event->cb_fnc = cb_fnc;
	event->usr = usr;
	strlcpy(event->client_name, client_name, MAX_CLIENT_NAME_LEN);
	event->active = true;

	mutex_lock(&phandle->phandle_lock);
	list_add(&event->list, &phandle->event_list);
	mutex_unlock(&phandle->phandle_lock);

	return event;
}

void sde_power_handle_unregister_event(
		struct sde_power_handle *phandle,
		struct sde_power_event *event)
{
	if (!phandle || !event) {
		pr_err("invalid phandle or event\n");
	} else if (!event->active) {
		pr_err("power handle deinit already done\n");
		kfree(event);
	} else {
		mutex_lock(&phandle->phandle_lock);
		list_del_init(&event->list);
		mutex_unlock(&phandle->phandle_lock);
		kfree(event);
	}
}
