/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#include "sde_power_handle.h"
#include "sde_trace.h"

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

	if (!ab_quota_rt && !ab_quota_nrt && !ib_quota_rt && !ib_quota_nrt)  {
		new_uc_idx = 0;
	} else {
		int i;
		struct msm_bus_vectors *vect = NULL;
		struct msm_bus_scale_pdata *bw_table =
			pdbus->data_bus_scale_table;
		u32 nrt_axi_port_cnt = pdbus->nrt_axi_port_cnt;
		u32 total_axi_port_cnt = pdbus->axi_port_cnt;
		u32 rt_axi_port_cnt = total_axi_port_cnt - nrt_axi_port_cnt;
		int match_cnt = 0;

		if (!bw_table || !total_axi_port_cnt ||
		    total_axi_port_cnt > MAX_AXI_PORT_COUNT) {
			pr_err("invalid input\n");
			return -EINVAL;
		}

		if (pdbus->bus_channels) {
			ib_quota_rt = div_u64(ib_quota_rt,
						pdbus->bus_channels);
			ib_quota_nrt = div_u64(ib_quota_nrt,
						pdbus->bus_channels);
		}

		if (nrt_axi_port_cnt) {

			ab_quota_rt = div_u64(ab_quota_rt, rt_axi_port_cnt);
			ab_quota_nrt = div_u64(ab_quota_nrt, nrt_axi_port_cnt);

			for (i = 0; i < total_axi_port_cnt; i++) {
				if (i < rt_axi_port_cnt) {
					ab_quota[i] = ab_quota_rt;
					ib_quota[i] = ib_quota_rt;
				} else {
					ab_quota[i] = ab_quota_nrt;
					ib_quota[i] = ib_quota_nrt;
				}
			}
		} else {
			ab_quota[0] = div_u64(ab_quota_rt + ab_quota_nrt,
					total_axi_port_cnt);
			ib_quota[0] = ib_quota_rt + ib_quota_nrt;

			for (i = 1; i < total_axi_port_cnt; i++) {
				ab_quota[i] = ab_quota[0];
				ib_quota[i] = ib_quota[0];
			}
		}

		for (i = 0; i < total_axi_port_cnt; i++) {
			vect = &bw_table->usecase
				[pdbus->curr_bw_uc_idx].vectors[i];
			/* avoid performing updates for small changes */
			if ((ab_quota[i] == vect->ab) &&
				(ib_quota[i] == vect->ib))
				match_cnt++;
		}

		if (match_cnt == total_axi_port_cnt) {
			pr_debug("skip BW vote\n");
			return 0;
		}

		new_uc_idx = (pdbus->curr_bw_uc_idx %
			(bw_table->num_usecases - 1)) + 1;

		for (i = 0; i < total_axi_port_cnt; i++) {
			vect = &bw_table->usecase[new_uc_idx].vectors[i];
			vect->ab = ab_quota[i];
			vect->ib = ib_quota[i];

			pr_debug("uc_idx=%d %s path idx=%d ab=%llu ib=%llu\n",
				new_uc_idx, (i < rt_axi_port_cnt) ? "rt" : "nrt"
				, i, vect->ab, vect->ib);
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
		int bus_client, u64 ab_quota, u64 ib_quota)
{
	int rc = 0;
	int i;
	u64 total_ab_rt = 0, total_ib_rt = 0;
	u64 total_ab_nrt = 0, total_ib_nrt = 0;
	struct sde_power_client *client;

	if (!phandle || !pclient ||
			bus_client >= SDE_POWER_HANDLE_DATA_BUS_CLIENT_MAX) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	mutex_lock(&phandle->phandle_lock);

	pclient->ab[bus_client] = ab_quota;
	pclient->ib[bus_client] = ib_quota;
	trace_sde_perf_update_bus(bus_client, ab_quota, ib_quota);

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

	rc = _sde_power_data_bus_set_quota(&phandle->data_bus_handle,
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
	struct sde_power_data_bus_handle *pdbus)
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

	pdbus->nrt_axi_port_cnt = 0;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,sde-num-nrt-paths",
			&pdbus->nrt_axi_port_cnt);
	if (rc) {
		pr_debug("number of axi port property not specified\n");
		rc = 0;
	}

	node = of_get_child_by_name(pdev->dev.of_node, "qcom,sde-data-bus");
	if (node) {
		rc = of_property_read_u32(node,
				"qcom,msm-bus,num-paths", &paths);
		if (rc) {
			pr_err("Error. qcom,msm-bus,num-paths not found\n");
			return rc;
		}
		pdbus->axi_port_cnt = paths;

		pdbus->data_bus_scale_table =
				msm_bus_pdata_from_node(pdev, node);
		if (IS_ERR_OR_NULL(pdbus->data_bus_scale_table)) {
			pr_err("reg bus handle parsing failed\n");
			rc = PTR_ERR(pdbus->data_bus_scale_table);
			goto end;
		}
		pdbus->data_bus_hdl = msm_bus_scale_register_client(
				pdbus->data_bus_scale_table);
		if (!pdbus->data_bus_hdl) {
			pr_err("data_bus_client register failed\n");
			rc = -EINVAL;
			goto end;
		}
		pr_debug("register data_bus_hdl=%x\n", pdbus->data_bus_hdl);

		/*
		 * Following call will not result in actual vote rather update
		 * the current index and ab/ib value. When continuous splash
		 * is enabled, actual vote will happen when splash handoff is
		 * done.
		 */
		return _sde_power_data_bus_set_quota(pdbus,
				SDE_POWER_HANDLE_DATA_BUS_AB_QUOTA,
				SDE_POWER_HANDLE_DATA_BUS_AB_QUOTA,
				SDE_POWER_HANDLE_DATA_BUS_IB_QUOTA,
				SDE_POWER_HANDLE_DATA_BUS_IB_QUOTA);
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
		struct sde_power_data_bus_handle *pdbus)
{
	return 0;
}

static void sde_power_data_bus_unregister(
		struct sde_power_data_bus_handle *pdbus)
{
}

int sde_power_data_bus_set_quota(struct sde_power_handle *phandle,
		struct sde_power_client *pclient,
		int bus_client, u64 ab_quota, u64 ib_quota)
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
#endif

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

	rc = sde_power_data_bus_parse(pdev, &phandle->data_bus_handle);
	if (rc) {
		pr_err("register data bus parse failed rc=%d\n", rc);
		goto data_bus_err;
	}

	INIT_LIST_HEAD(&phandle->power_client_clist);
	mutex_init(&phandle->phandle_lock);

	return rc;

data_bus_err:
	sde_power_reg_bus_unregister(phandle->reg_bus_hdl);
bus_err:
	msm_dss_put_clk(mp->clk_config, mp->num_clk);
clk_err:
	msm_dss_config_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);
vreg_err:
	devm_kfree(&pdev->dev, mp->vreg_config);
	mp->num_vreg = 0;
parse_vreg_err:
	devm_kfree(&pdev->dev, mp->clk_config);
	mp->num_clk = 0;
end:
	return rc;
}

void sde_power_resource_deinit(struct platform_device *pdev,
	struct sde_power_handle *phandle)
{
	struct dss_module_power *mp;

	if (!phandle || !pdev) {
		pr_err("invalid input param\n");
		return;
	}
	mp = &phandle->mp;

	sde_power_data_bus_unregister(&phandle->data_bus_handle);

	sde_power_reg_bus_unregister(phandle->reg_bus_hdl);

	msm_dss_put_clk(mp->clk_config, mp->num_clk);

	msm_dss_config_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);

	if (mp->clk_config)
		devm_kfree(&pdev->dev, mp->clk_config);

	if (mp->vreg_config)
		devm_kfree(&pdev->dev, mp->vreg_config);

	mp->num_vreg = 0;
	mp->num_clk = 0;
}

int sde_power_resource_enable(struct sde_power_handle *phandle,
	struct sde_power_client *pclient, bool enable)
{
	int rc = 0;
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

	if (phandle->current_usecase_ndx != max_usecase_ndx) {
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
		rc = msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg, enable);
		if (rc) {
			pr_err("failed to enable vregs rc=%d\n", rc);
			goto vreg_err;
		}

		rc = sde_power_reg_bus_update(phandle->reg_bus_hdl,
							max_usecase_ndx);
		if (rc) {
			pr_err("failed to set reg bus vote rc=%d\n", rc);
			goto reg_bus_hdl_err;
		}

		rc = msm_dss_enable_clk(mp->clk_config, mp->num_clk, enable);
		if (rc) {
			pr_err("clock enable failed rc:%d\n", rc);
			goto clk_err;
		}
	} else {
		msm_dss_enable_clk(mp->clk_config, mp->num_clk, enable);

		sde_power_reg_bus_update(phandle->reg_bus_hdl,
							max_usecase_ndx);

		msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg, enable);
	}

end:
	mutex_unlock(&phandle->phandle_lock);
	return rc;

clk_err:
	sde_power_reg_bus_update(phandle->reg_bus_hdl, prev_usecase_ndx);
reg_bus_hdl_err:
	msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg, 0);
vreg_err:
	phandle->current_usecase_ndx = prev_usecase_ndx;
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
