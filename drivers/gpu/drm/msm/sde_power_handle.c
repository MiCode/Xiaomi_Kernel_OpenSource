/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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

#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/sde_io_util.h>
#include <linux/sde_rsc.h>

#include "sde_power_handle.h"
#include "sde_trace.h"
#include "sde_dbg.h"

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

	list_for_each_entry(event, &phandle->event_list, list) {
		if (event->event_type & event_type)
			event->cb_fnc(event_type, event->usr);
	}
}

static int sde_power_rsc_update(struct sde_power_handle *phandle, bool enable)
{
	u32 rsc_state;
	int ret = 0;

	/* creates the rsc client on the first enable */
	if (!phandle->rsc_client_init) {
		phandle->rsc_client = sde_rsc_client_create(SDE_RSC_INDEX,
				"sde_power_handle", false);
		if (IS_ERR_OR_NULL(phandle->rsc_client)) {
			pr_debug("sde rsc client create failed :%ld\n",
						PTR_ERR(phandle->rsc_client));
			phandle->rsc_client = NULL;
		}
		phandle->rsc_client_init = true;
	}

	rsc_state = enable ? SDE_RSC_CLK_STATE : SDE_RSC_IDLE_STATE;

	if (phandle->rsc_client)
		ret = sde_rsc_client_state_update(phandle->rsc_client,
			rsc_state, NULL, SDE_RSC_INVALID_CRTC_ID, NULL);

	return ret;
}

struct sde_power_client *sde_power_client_create(
	struct sde_power_handle *phandle, char *client_name)
{
	struct sde_power_client *client;
	static u32 id;

	if (!client_name || !phandle) {
		pr_err("client name is null or invalid power data\n");
		return ERR_PTR(-EINVAL);
	}

	client = kzalloc(sizeof(struct sde_power_client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&phandle->phandle_lock);
	strlcpy(client->name, client_name, MAX_CLIENT_NAME_LEN);
	client->usecase_ndx = VOTE_INDEX_DISABLE;
	client->id = id;
	client->active = true;
	pr_debug("client %s created:%pK id :%d\n", client_name,
		client, id);
	id++;
	list_add(&client->list, &phandle->power_client_clist);
	mutex_unlock(&phandle->phandle_lock);

	return client;
}

void sde_power_client_destroy(struct sde_power_handle *phandle,
	struct sde_power_client *client)
{
	if (!client  || !phandle) {
		pr_err("reg bus vote: invalid client handle\n");
	} else if (!client->active) {
		pr_err("sde power deinit already done\n");
		kfree(client);
	} else {
		pr_debug("bus vote client %s destroyed:%pK id:%u\n",
			client->name, client, client->id);
		mutex_lock(&phandle->phandle_lock);
		list_del_init(&client->list);
		mutex_unlock(&phandle->phandle_lock);
		kfree(client);
	}
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

#ifdef CONFIG_QCOM_BUS_SCALING

#define MAX_AXI_PORT_COUNT 3

static int _sde_power_data_bus_set_quota(
		struct sde_power_data_bus_handle *pdbus,
		u64 ab_quota_rt, u64 ab_quota_nrt,
		u64 ib_quota_rt, u64 ib_quota_nrt)
{
	int new_uc_idx;
	u64 ab_quota[MAX_AXI_PORT_COUNT] = {0, 0};
	u64 ib_quota[MAX_AXI_PORT_COUNT] = {0, 0};
	int rc;

	if (pdbus->data_bus_hdl < 1) {
		pr_err("invalid bus handle %d\n", pdbus->data_bus_hdl);
		return -EINVAL;
	}

	pdbus->ab_rt = ab_quota_rt;
	pdbus->ib_rt = ib_quota_rt;
	pdbus->ab_nrt = ab_quota_nrt;
	pdbus->ib_nrt = ib_quota_nrt;

	if (pdbus->enable) {
		ab_quota_rt = max_t(u64, ab_quota_rt,
				SDE_POWER_HANDLE_ENABLE_BUS_AB_QUOTA);
		ib_quota_rt = max_t(u64, ib_quota_rt,
				SDE_POWER_HANDLE_ENABLE_BUS_IB_QUOTA);
		ab_quota_nrt = max_t(u64, ab_quota_nrt,
				SDE_POWER_HANDLE_ENABLE_BUS_AB_QUOTA);
		ib_quota_nrt = max_t(u64, ib_quota_nrt,
				SDE_POWER_HANDLE_ENABLE_NRT_BUS_IB_QUOTA);
	} else {
		ab_quota_rt = min_t(u64, ab_quota_rt,
				SDE_POWER_HANDLE_DISABLE_BUS_AB_QUOTA);
		ib_quota_rt = min_t(u64, ib_quota_rt,
				SDE_POWER_HANDLE_DISABLE_BUS_IB_QUOTA);
		ab_quota_nrt = min_t(u64, ab_quota_nrt,
				SDE_POWER_HANDLE_DISABLE_BUS_AB_QUOTA);
		ib_quota_nrt = min_t(u64, ib_quota_nrt,
				SDE_POWER_HANDLE_DISABLE_BUS_IB_QUOTA);
	}

	if (!ab_quota_rt && !ab_quota_nrt && !ib_quota_rt && !ib_quota_nrt)  {
		new_uc_idx = 0;
	} else {
		int i;
		struct msm_bus_vectors *vect = NULL;
		struct msm_bus_scale_pdata *bw_table =
			pdbus->data_bus_scale_table;
		u32 nrt_data_paths_cnt = pdbus->nrt_data_paths_cnt;
		u32 total_data_paths_cnt = pdbus->data_paths_cnt;
		u32 rt_data_paths_cnt = total_data_paths_cnt -
			nrt_data_paths_cnt;

		if (!bw_table || !total_data_paths_cnt ||
		    total_data_paths_cnt > MAX_AXI_PORT_COUNT) {
			pr_err("invalid input\n");
			return -EINVAL;
		}

		if (nrt_data_paths_cnt) {

			ab_quota_rt = div_u64(ab_quota_rt, rt_data_paths_cnt);
			ab_quota_nrt = div_u64(ab_quota_nrt,
						nrt_data_paths_cnt);

			ib_quota_rt = div_u64(ib_quota_rt,
						rt_data_paths_cnt);
			ib_quota_nrt = div_u64(ib_quota_nrt,
						nrt_data_paths_cnt);

			for (i = 0; i < total_data_paths_cnt; i++) {
				if (i < rt_data_paths_cnt) {
					ab_quota[i] = ab_quota_rt;
					ib_quota[i] = ib_quota_rt;
				} else {
					ab_quota[i] = ab_quota_nrt;
					ib_quota[i] = ib_quota_nrt;
				}
			}
		} else {
			ab_quota[0] = div_u64(ab_quota_rt + ab_quota_nrt,
					total_data_paths_cnt);
			ib_quota[0] = div_u64(ib_quota_rt + ib_quota_nrt,
					total_data_paths_cnt);

			for (i = 1; i < total_data_paths_cnt; i++) {
				ab_quota[i] = ab_quota[0];
				ib_quota[i] = ib_quota[0];
			}
		}

		new_uc_idx = (pdbus->curr_bw_uc_idx %
			(bw_table->num_usecases - 1)) + 1;

		for (i = 0; i < total_data_paths_cnt; i++) {
			vect = &bw_table->usecase[new_uc_idx].vectors[i];
			vect->ab = ab_quota[i];
			vect->ib = ib_quota[i];

			pr_debug(
				"%s uc_idx=%d %s path idx=%d ab=%llu ib=%llu\n",
				bw_table->name,
				new_uc_idx, (i < rt_data_paths_cnt) ?
				"rt" : "nrt", i, vect->ab, vect->ib);
		}
	}
	pdbus->curr_bw_uc_idx = new_uc_idx;
	pdbus->ao_bw_uc_idx = new_uc_idx;

	SDE_ATRACE_BEGIN("msm_bus_scale_req");
	rc = msm_bus_scale_client_update_request(pdbus->data_bus_hdl,
			new_uc_idx);
	SDE_ATRACE_END("msm_bus_scale_req");

	return rc;
}

int sde_power_data_bus_set_quota(struct sde_power_handle *phandle,
		struct sde_power_client *pclient,
		int bus_client, u32 bus_id,
		u64 ab_quota, u64 ib_quota)
{
	int rc = 0;
	int i;
	u64 total_ab_rt = 0, total_ib_rt = 0;
	u64 total_ab_nrt = 0, total_ib_nrt = 0;
	struct sde_power_client *client;

	if (!phandle || !pclient ||
			bus_client >= SDE_POWER_HANDLE_DATA_BUS_CLIENT_MAX ||
			bus_id >= SDE_POWER_HANDLE_DBUS_ID_MAX) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	mutex_lock(&phandle->phandle_lock);

	pclient->ab[bus_client] = ab_quota;
	pclient->ib[bus_client] = ib_quota;
	trace_sde_perf_update_bus(bus_client, bus_id, ab_quota, ib_quota);

	list_for_each_entry(client, &phandle->power_client_clist, list) {
		for (i = 0; i < SDE_POWER_HANDLE_DATA_BUS_CLIENT_MAX; i++) {
			if (i == SDE_POWER_HANDLE_DATA_BUS_CLIENT_NRT) {
				total_ab_nrt += client->ab[i];
				total_ib_nrt += client->ib[i];
			} else {
				total_ab_rt += client->ab[i];
				total_ib_rt = max(total_ib_rt, client->ib[i]);
			}
		}
	}

	if (phandle->data_bus_handle[bus_id].data_bus_hdl)
		rc = _sde_power_data_bus_set_quota(
			&phandle->data_bus_handle[bus_id],
			total_ab_rt, total_ab_nrt,
			total_ib_rt, total_ib_nrt);

	mutex_unlock(&phandle->phandle_lock);

	return rc;
}

static void sde_power_data_bus_unregister(
		struct sde_power_data_bus_handle *pdbus)
{
	if (pdbus->data_bus_hdl) {
		msm_bus_scale_unregister_client(pdbus->data_bus_hdl);
		pdbus->data_bus_hdl = 0;
	}
}

static int sde_power_data_bus_parse(struct platform_device *pdev,
	struct sde_power_data_bus_handle *pdbus, const char *name)
{
	struct device_node *node;
	int rc = 0;
	int paths;

	pdbus->bus_channels = 1;
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,sde-dram-channels", &pdbus->bus_channels);
	if (rc) {
		pr_debug("number of channels property not specified\n");
		rc = 0;
	}

	pdbus->nrt_data_paths_cnt = 0;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,sde-num-nrt-paths",
			&pdbus->nrt_data_paths_cnt);
	if (rc) {
		pr_debug("number of axi port property not specified\n");
		rc = 0;
	}

	node = of_get_child_by_name(pdev->dev.of_node, name);
	if (node) {
		rc = of_property_read_u32(node,
				"qcom,msm-bus,num-paths", &paths);
		if (rc) {
			pr_err("Error. qcom,msm-bus,num-paths not found\n");
			return rc;
		}
		pdbus->data_paths_cnt = paths;

		pdbus->data_bus_scale_table =
				msm_bus_pdata_from_node(pdev, node);
		if (IS_ERR_OR_NULL(pdbus->data_bus_scale_table)) {
			pr_err("reg bus handle parsing failed\n");
			rc = PTR_ERR(pdbus->data_bus_scale_table);
			if (!pdbus->data_bus_scale_table)
				rc = -EINVAL;
			goto end;
		}
		pdbus->data_bus_hdl = msm_bus_scale_register_client(
				pdbus->data_bus_scale_table);
		if (!pdbus->data_bus_hdl) {
			pr_err("data_bus_client register failed\n");
			rc = -EINVAL;
			goto end;
		}
		pr_debug("register %s data_bus_hdl=%x\n", name,
				pdbus->data_bus_hdl);
	}

end:
	return rc;
}

static int sde_power_reg_bus_parse(struct platform_device *pdev,
	struct sde_power_handle *phandle)
{
	struct device_node *node;
	struct msm_bus_scale_pdata *bus_scale_table;
	int rc = 0;

	node = of_get_child_by_name(pdev->dev.of_node, "qcom,sde-reg-bus");
	if (node) {
		bus_scale_table = msm_bus_pdata_from_node(pdev, node);
		if (IS_ERR_OR_NULL(bus_scale_table)) {
			pr_err("reg bus handle parsing failed\n");
			rc = PTR_ERR(bus_scale_table);
			if (!bus_scale_table)
				rc = -EINVAL;
			goto end;
		}
		phandle->reg_bus_hdl = msm_bus_scale_register_client(
			      bus_scale_table);
		if (!phandle->reg_bus_hdl) {
			pr_err("reg_bus_client register failed\n");
			rc = -EINVAL;
			goto end;
		}
		pr_debug("register reg_bus_hdl=%x\n", phandle->reg_bus_hdl);
	}

end:
	return rc;
}

static void sde_power_reg_bus_unregister(u32 reg_bus_hdl)
{
	if (reg_bus_hdl)
		msm_bus_scale_unregister_client(reg_bus_hdl);
}

int sde_power_data_bus_state_update(struct sde_power_handle *phandle,
							bool enable)
{
	int i;

	if (!phandle) {
		pr_err("invalid param\n");
		return -EINVAL;
	}

	for (i = SDE_POWER_HANDLE_DBUS_ID_MNOC;
			i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
		phandle->data_bus_handle[i].enable = enable;

	return 0;
}

static int sde_power_data_bus_update(struct sde_power_data_bus_handle *pdbus,
							bool enable)
{
	int rc = 0;

	pdbus->enable = enable;

	if (pdbus->data_bus_hdl)
		rc = _sde_power_data_bus_set_quota(pdbus, pdbus->ab_rt,
				pdbus->ab_nrt, pdbus->ib_rt, pdbus->ib_nrt);

	if (rc)
		pr_err("failed to set data bus vote rc=%d enable:%d\n",
							rc, enable);

	return rc;
}

static int sde_power_reg_bus_update(u32 reg_bus_hdl, u32 usecase_ndx)
{
	int rc = 0;

	if (reg_bus_hdl)
		rc = msm_bus_scale_client_update_request(reg_bus_hdl,
								usecase_ndx);
	if (rc)
		pr_err("failed to set reg bus vote rc=%d\n", rc);

	return rc;
}
#else
static int sde_power_data_bus_parse(struct platform_device *pdev,
		struct sde_power_data_bus_handle *pdbus, const char *name)
{
	return 0;
}

static void sde_power_data_bus_unregister(
		struct sde_power_data_bus_handle *pdbus)
{
}

int sde_power_data_bus_set_quota(struct sde_power_handle *phandle,
		struct sde_power_client *pclient,
		int bus_client, u32 bus_id,
		u64 ab_quota, u64 ib_quota)
{
	return 0;
}

static int sde_power_reg_bus_parse(struct platform_device *pdev,
	struct sde_power_handle *phandle)
{
	return 0;
}

static void sde_power_reg_bus_unregister(u32 reg_bus_hdl)
{
}

static int sde_power_reg_bus_update(u32 reg_bus_hdl, u32 usecase_ndx)
{
	return 0;
}

static int sde_power_data_bus_update(struct sde_power_data_bus_handle *pdbus,
							bool enable)
{
	return 0;
}

int sde_power_data_bus_state_update(struct sde_power_handle *phandle,
							bool enable)
{
	return 0;
}
#endif

int sde_power_resource_init(struct platform_device *pdev,
	struct sde_power_handle *phandle)
{
	int rc = 0, i;
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

	rc = msm_dss_config_vreg(&pdev->dev,
				mp->vreg_config, mp->num_vreg, 1);
	if (rc) {
		pr_err("vreg config failed rc=%d\n", rc);
		goto vreg_err;
	}

	rc = msm_dss_get_clk(&pdev->dev, mp->clk_config, mp->num_clk);
	if (rc) {
		pr_err("clock get failed rc=%d\n", rc);
		goto clk_err;
	}

	rc = msm_dss_clk_set_rate(mp->clk_config, mp->num_clk);
	if (rc) {
		pr_err("clock set rate failed rc=%d\n", rc);
		goto bus_err;
	}

	rc = sde_power_reg_bus_parse(pdev, phandle);
	if (rc) {
		pr_err("register bus parse failed rc=%d\n", rc);
		goto bus_err;
	}

	for (i = SDE_POWER_HANDLE_DBUS_ID_MNOC;
			i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
		rc = sde_power_data_bus_parse(pdev,
				&phandle->data_bus_handle[i],
				data_bus_name[i]);
		if (rc) {
			pr_err("register data bus parse failed id=%d rc=%d\n",
					i, rc);
			goto data_bus_err;
		}
	}

	INIT_LIST_HEAD(&phandle->power_client_clist);
	INIT_LIST_HEAD(&phandle->event_list);

	phandle->rsc_client = NULL;
	phandle->rsc_client_init = false;

	mutex_init(&phandle->phandle_lock);

	return rc;

data_bus_err:
	for (i--; i >= 0; i--)
		sde_power_data_bus_unregister(&phandle->data_bus_handle[i]);
	sde_power_reg_bus_unregister(phandle->reg_bus_hdl);
bus_err:
	msm_dss_put_clk(mp->clk_config, mp->num_clk);
clk_err:
	msm_dss_config_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);
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
	struct sde_power_client *curr_client, *next_client;
	struct sde_power_event *curr_event, *next_event;
	int i;

	if (!phandle || !pdev) {
		pr_err("invalid input param\n");
		return;
	}
	mp = &phandle->mp;

	mutex_lock(&phandle->phandle_lock);
	list_for_each_entry_safe(curr_client, next_client,
			&phandle->power_client_clist, list) {
		pr_err("cliend:%s-%d still registered with refcount:%d\n",
				curr_client->name, curr_client->id,
				curr_client->refcount);
		curr_client->active = false;
		list_del(&curr_client->list);
	}

	list_for_each_entry_safe(curr_event, next_event,
			&phandle->event_list, list) {
		pr_err("event:%d, client:%s still registered\n",
				curr_event->event_type,
				curr_event->client_name);
		curr_event->active = false;
		list_del(&curr_event->list);
	}
	mutex_unlock(&phandle->phandle_lock);

	for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
		sde_power_data_bus_unregister(&phandle->data_bus_handle[i]);

	sde_power_reg_bus_unregister(phandle->reg_bus_hdl);

	msm_dss_put_clk(mp->clk_config, mp->num_clk);

	msm_dss_config_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);

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
	struct sde_power_client *pclient, u32 usecase_ndx, bool skip_lock)
{
	struct sde_power_client *client;
	int rc = 0;
	u32 max_usecase_ndx = VOTE_INDEX_DISABLE;

	if (!skip_lock) {
		mutex_lock(&phandle->phandle_lock);

		if (WARN_ON(pclient->refcount == 0)) {
			/*
			 * This is not expected, clients calling without skip
			 * lock are outside the power resource enable, which
			 * means that they should have enabled the power
			 * resource before trying to scale.
			 */
			rc = -EINVAL;
			goto exit;
		}
	}

	pr_debug("%pS: current idx:%d requested:%d client:%d\n",
		__builtin_return_address(0), pclient->usecase_ndx,
		usecase_ndx, pclient->id);

	pclient->usecase_ndx = usecase_ndx;

	list_for_each_entry(client, &phandle->power_client_clist, list) {
		if (client->usecase_ndx < VOTE_INDEX_MAX &&
		    client->usecase_ndx > max_usecase_ndx)
			max_usecase_ndx = client->usecase_ndx;
	}

	rc = sde_power_reg_bus_update(phandle->reg_bus_hdl,
						max_usecase_ndx);
	if (rc)
		pr_err("failed to set reg bus vote rc=%d\n", rc);

exit:
	if (!skip_lock)
		mutex_unlock(&phandle->phandle_lock);

	return rc;
}

static inline bool _resource_changed(u32 current_usecase_ndx,
		u32 max_usecase_ndx)
{
	WARN_ON((current_usecase_ndx >= VOTE_INDEX_MAX)
		|| (max_usecase_ndx >= VOTE_INDEX_MAX));

	if (((current_usecase_ndx >= VOTE_INDEX_LOW) && /*current enabled */
		(max_usecase_ndx == VOTE_INDEX_DISABLE)) || /* max disabled */
		((current_usecase_ndx == VOTE_INDEX_DISABLE) && /* disabled */
		(max_usecase_ndx >= VOTE_INDEX_LOW))) /* max enabled */
		return true;

	return false;
}

int sde_power_resource_enable(struct sde_power_handle *phandle,
	struct sde_power_client *pclient, bool enable)
{
	int rc = 0, i;
	bool changed = false;
	u32 max_usecase_ndx = VOTE_INDEX_DISABLE, prev_usecase_ndx;
	struct sde_power_client *client;
	struct dss_module_power *mp;

	if (!phandle || !pclient) {
		pr_err("invalid input argument\n");
		return -EINVAL;
	}

	mp = &phandle->mp;

	mutex_lock(&phandle->phandle_lock);
	if (enable)
		pclient->refcount++;
	else if (pclient->refcount)
		pclient->refcount--;

	if (pclient->refcount)
		pclient->usecase_ndx = VOTE_INDEX_LOW;
	else
		pclient->usecase_ndx = VOTE_INDEX_DISABLE;

	list_for_each_entry(client, &phandle->power_client_clist, list) {
		if (client->usecase_ndx < VOTE_INDEX_MAX &&
		    client->usecase_ndx > max_usecase_ndx)
			max_usecase_ndx = client->usecase_ndx;
	}

	/*
	 * Check if we need to enable/disable the power resource, we won't
	 * only-scale up/down the AHB vote in this API; if a client wants to
	 * bump up the AHB clock above the LOW (default) level, it needs to
	 * call 'sde_power_scale_reg_bus' with the desired vote after the power
	 * resource was enabled.
	 */
	if (_resource_changed(phandle->current_usecase_ndx,
			max_usecase_ndx)) {
		changed = true;
		prev_usecase_ndx = phandle->current_usecase_ndx;
		phandle->current_usecase_ndx = max_usecase_ndx;
	}

	pr_debug("%pS: changed=%d current idx=%d request client %s id:%u enable:%d refcount:%d\n",
		__builtin_return_address(0), changed, max_usecase_ndx,
		pclient->name, pclient->id, enable, pclient->refcount);

	if (!changed)
		goto end;

	if (enable) {
		sde_power_event_trigger_locked(phandle,
				SDE_POWER_EVENT_PRE_ENABLE);

		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
			rc = sde_power_data_bus_update(
					&phandle->data_bus_handle[i], enable);
			if (rc) {
				pr_err("failed to set data bus vote id=%d rc=%d\n",
						i, rc);
				goto data_bus_hdl_err;
			}
		}
		/*
		 * - When the target is RSCC enabled, regulator should
		 *   be enabled by the s/w only for the first time during
		 *   bootup. After that, RSCC hardware takes care of enabling/
		 *   disabling it.
		 * - When the target is not RSCC enabled, regulator should
		 *   be totally handled by the software.
		 */
		if (!phandle->rsc_client) {
			rc = msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg,
									enable);
			if (rc) {
				pr_err("failed to enable vregs rc=%d\n", rc);
				goto vreg_err;
			}
		}

		rc = sde_power_scale_reg_bus(phandle, pclient,
				max_usecase_ndx, true);
		if (rc) {
			pr_err("failed to set reg bus vote rc=%d\n", rc);
			goto reg_bus_hdl_err;
		}

		rc = sde_power_rsc_update(phandle, true);
		if (rc) {
			pr_err("failed to update rsc\n");
			goto rsc_err;
		}

		SDE_EVT32_VERBOSE(enable, SDE_EVTLOG_FUNC_CASE1);
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
		msm_dss_enable_clk(mp->clk_config, mp->num_clk, enable);

		sde_power_rsc_update(phandle, false);

		sde_power_scale_reg_bus(phandle, pclient,
				max_usecase_ndx, true);

		if (!phandle->rsc_client)
			msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg,
									enable);
		for (i = 0 ; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
			sde_power_data_bus_update(&phandle->data_bus_handle[i],
					enable);

		sde_power_event_trigger_locked(phandle,
				SDE_POWER_EVENT_POST_DISABLE);
	}

end:
	SDE_EVT32_VERBOSE(enable, SDE_EVTLOG_FUNC_EXIT);
	mutex_unlock(&phandle->phandle_lock);

	return rc;

clk_err:
	sde_power_rsc_update(phandle, false);
rsc_err:
	sde_power_scale_reg_bus(phandle, pclient, max_usecase_ndx, true);
reg_bus_hdl_err:
	if (!phandle->rsc_client)
		msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg, 0);
vreg_err:
	for (i = 0 ; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
		sde_power_data_bus_update(&phandle->data_bus_handle[i], 0);
data_bus_hdl_err:
	phandle->current_usecase_ndx = prev_usecase_ndx;
	mutex_unlock(&phandle->phandle_lock);
	return rc;
}

int sde_power_resource_is_enabled(struct sde_power_handle *phandle)
{
	if (!phandle) {
		pr_err("invalid input argument\n");
		return false;
	}

	return phandle->current_usecase_ndx != VOTE_INDEX_DISABLE;
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
	mp = &phandle->mp;

	for (i = 0; i < mp->num_clk; i++) {
		if (!strcmp(mp->clk_config[i].clk_name, clock_name)) {
			if (mp->clk_config[i].max_rate &&
					(rate > mp->clk_config[i].max_rate))
				rate = mp->clk_config[i].max_rate;

			mp->clk_config[i].rate = rate;
			rc = msm_dss_clk_set_rate(mp->clk_config, mp->num_clk);
			break;
		}
	}

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
