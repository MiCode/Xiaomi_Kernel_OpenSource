/* Copyright (c) 2014-2017, Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/rpm-smd.h>
#include <trace/events/trace_msm_bus.h>
#include "msm_bus_core.h"
#include "msm_bus_adhoc.h"
#include "msm_bus_noc.h"
#include "msm_bus_bimc.h"

static LIST_HEAD(fabdev_list);

static int msm_bus_dev_init_qos(struct device *dev, void *data);

ssize_t bw_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct msm_bus_node_info_type *node_info = NULL;
	struct msm_bus_node_device_type *bus_node = NULL;
	int i;
	int off = 0;

	bus_node = to_msm_bus_node(dev);
	if (!bus_node)
		return -EINVAL;

	node_info = bus_node->node_info;

	for (i = 0; i < bus_node->num_lnodes; i++) {
		if (!bus_node->lnode_list[i].in_use)
			continue;
		off += scnprintf((buf + off), PAGE_SIZE,
		"[%d]:%s:Act_IB %llu Act_AB %llu Slp_IB %llu Slp_AB %llu\n",
			i, bus_node->lnode_list[i].cl_name,
			bus_node->lnode_list[i].lnode_ib[ACTIVE_CTX],
			bus_node->lnode_list[i].lnode_ab[ACTIVE_CTX],
			bus_node->lnode_list[i].lnode_ib[DUAL_CTX],
			bus_node->lnode_list[i].lnode_ab[DUAL_CTX]);
		trace_printk(
		"[%d]:%s:Act_IB %llu Act_AB %llu Slp_IB %llu Slp_AB %llu\n",
			i, bus_node->lnode_list[i].cl_name,
			bus_node->lnode_list[i].lnode_ib[ACTIVE_CTX],
			bus_node->lnode_list[i].lnode_ab[ACTIVE_CTX],
			bus_node->lnode_list[i].lnode_ib[DUAL_CTX],
			bus_node->lnode_list[i].lnode_ab[DUAL_CTX]);
	}
	off += scnprintf((buf + off), PAGE_SIZE,
	"Max_Act_IB %llu Sum_Act_AB %llu Act_Util_fact %d Act_Vrail_comp %d\n",
		bus_node->node_bw[ACTIVE_CTX].max_ib,
		bus_node->node_bw[ACTIVE_CTX].sum_ab,
		bus_node->node_bw[ACTIVE_CTX].util_used,
		bus_node->node_bw[ACTIVE_CTX].vrail_used);
	off += scnprintf((buf + off), PAGE_SIZE,
	"Max_Slp_IB %llu Sum_Slp_AB %llu Slp_Util_fact %d Slp_Vrail_comp %d\n",
		bus_node->node_bw[DUAL_CTX].max_ib,
		bus_node->node_bw[DUAL_CTX].sum_ab,
		bus_node->node_bw[DUAL_CTX].util_used,
		bus_node->node_bw[DUAL_CTX].vrail_used);
	trace_printk(
	"Max_Act_IB %llu Sum_Act_AB %llu Act_Util_fact %d Act_Vrail_comp %d\n",
		bus_node->node_bw[ACTIVE_CTX].max_ib,
		bus_node->node_bw[ACTIVE_CTX].sum_ab,
		bus_node->node_bw[ACTIVE_CTX].util_used,
		bus_node->node_bw[ACTIVE_CTX].vrail_used);
	trace_printk(
	"Max_Slp_IB %llu Sum_Slp_AB %lluSlp_Util_fact %d Slp_Vrail_comp %d\n",
		bus_node->node_bw[DUAL_CTX].max_ib,
		bus_node->node_bw[DUAL_CTX].sum_ab,
		bus_node->node_bw[DUAL_CTX].util_used,
		bus_node->node_bw[DUAL_CTX].vrail_used);
	return off;
}

ssize_t bw_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	return count;
}

DEVICE_ATTR(bw, 0600, bw_show, bw_store);

struct static_rules_type {
	int num_rules;
	struct bus_rule_type *rules;
};

static struct static_rules_type static_rules;

static int bus_get_reg(struct nodeclk *nclk, struct device *dev)
{
	int ret = 0;
	struct msm_bus_node_device_type *node_dev;

	if (!(dev && nclk))
		return -ENXIO;

	node_dev = to_msm_bus_node(dev);
	if (!strlen(nclk->reg_name)) {
		dev_dbg(dev, "No regulator exist for node %d\n",
						node_dev->node_info->id);
		goto exit_of_get_reg;
	} else {
		if (!(IS_ERR_OR_NULL(nclk->reg)))
			goto exit_of_get_reg;

		nclk->reg = devm_regulator_get(dev, nclk->reg_name);
		if (IS_ERR_OR_NULL(nclk->reg)) {
			ret =
			(IS_ERR(nclk->reg) ? PTR_ERR(nclk->reg) : -ENXIO);
			dev_err(dev, "Error: Failed to get regulator %s:%d\n",
							nclk->reg_name, ret);
		} else {
			dev_dbg(dev, "Succesfully got regulator for %d\n",
				node_dev->node_info->id);
		}
	}

exit_of_get_reg:
	return ret;
}

static int bus_enable_reg(struct nodeclk *nclk)
{
	int ret = 0;

	if (!nclk) {
		ret = -ENXIO;
		goto exit_bus_enable_reg;
	}

	if ((IS_ERR_OR_NULL(nclk->reg))) {
		ret = -ENXIO;
		goto exit_bus_enable_reg;
	}

	ret = regulator_enable(nclk->reg);
	if (ret) {
		MSM_BUS_ERR("Failed to enable regulator for %s\n",
							nclk->reg_name);
		goto exit_bus_enable_reg;
	}
	pr_debug("%s: Enabled Reg\n", __func__);
exit_bus_enable_reg:
	return ret;
}

static int bus_disable_reg(struct nodeclk *nclk)
{
	int ret = 0;

	if (!nclk) {
		ret = -ENXIO;
		goto exit_bus_disable_reg;
	}

	if ((IS_ERR_OR_NULL(nclk->reg))) {
		ret = -ENXIO;
		goto exit_bus_disable_reg;
	}

	regulator_disable(nclk->reg);
	pr_debug("%s: Disabled Reg\n", __func__);
exit_bus_disable_reg:
	return ret;
}

static int enable_nodeclk(struct nodeclk *nclk, struct device *dev)
{
	int ret = 0;

	if (!nclk->enable && !nclk->setrate_only_clk) {
		if (dev && strlen(nclk->reg_name)) {
			if (IS_ERR_OR_NULL(nclk->reg)) {
				ret = bus_get_reg(nclk, dev);
				if (ret) {
					dev_dbg(dev,
						"Failed to get reg.Err %d\n",
									ret);
					goto exit_enable_nodeclk;
				}
			}

			ret = bus_enable_reg(nclk);
			if (ret) {
				dev_dbg(dev, "Failed to enable reg. Err %d\n",
									ret);
				goto exit_enable_nodeclk;
			}
		}
		ret = clk_prepare_enable(nclk->clk);

		if (ret) {
			MSM_BUS_ERR("%s: failed to enable clk ", __func__);
			nclk->enable = false;
		} else
			nclk->enable = true;
	}
exit_enable_nodeclk:
	return ret;
}

static int disable_nodeclk(struct nodeclk *nclk)
{
	int ret = 0;

	if (nclk->enable && !nclk->setrate_only_clk) {
		clk_disable_unprepare(nclk->clk);
		nclk->enable = false;
		bus_disable_reg(nclk);
	}
	return ret;
}

static int setrate_nodeclk(struct nodeclk *nclk, long rate)
{
	int ret = 0;

	if (!nclk->enable_only_clk)
		ret = clk_set_rate(nclk->clk, rate);

	if (ret)
		MSM_BUS_ERR("%s: failed to setrate clk", __func__);
	return ret;
}

static int send_rpm_msg(struct msm_bus_node_device_type *ndev, int ctx)
{
	int ret = 0;
	int rsc_type;
	struct msm_rpm_kvp rpm_kvp;
	int rpm_ctx;

	if (!ndev) {
		MSM_BUS_ERR("%s: Error getting node info.", __func__);
		ret = -ENODEV;
		goto exit_send_rpm_msg;
	}

	rpm_kvp.length = sizeof(uint64_t);
	rpm_kvp.key = RPM_MASTER_FIELD_BW;

	if (ctx == DUAL_CTX)
		rpm_ctx = MSM_RPM_CTX_SLEEP_SET;
	else
		rpm_ctx = MSM_RPM_CTX_ACTIVE_SET;

	rpm_kvp.data = (uint8_t *)&ndev->node_bw[ctx].sum_ab;

	if (ndev->node_info->mas_rpm_id != -1) {
		rsc_type = RPM_BUS_MASTER_REQ;
		ret = msm_rpm_send_message(rpm_ctx, rsc_type,
			ndev->node_info->mas_rpm_id, &rpm_kvp, 1);
		if (ret) {
			MSM_BUS_ERR("%s: Failed to send RPM message:",
					__func__);
			MSM_BUS_ERR("%s:Node Id %d RPM id %d",
			__func__, ndev->node_info->id,
				 ndev->node_info->mas_rpm_id);
			goto exit_send_rpm_msg;
		}
		trace_bus_agg_bw(ndev->node_info->id,
			ndev->node_info->mas_rpm_id, rpm_ctx,
			ndev->node_bw[ctx].sum_ab);
	}

	if (ndev->node_info->slv_rpm_id != -1) {
		rsc_type = RPM_BUS_SLAVE_REQ;
		ret = msm_rpm_send_message(rpm_ctx, rsc_type,
			ndev->node_info->slv_rpm_id, &rpm_kvp, 1);
		if (ret) {
			MSM_BUS_ERR("%s: Failed to send RPM message:",
						__func__);
			MSM_BUS_ERR("%s: Node Id %d RPM id %d",
			__func__, ndev->node_info->id,
				ndev->node_info->slv_rpm_id);
			goto exit_send_rpm_msg;
		}
		trace_bus_agg_bw(ndev->node_info->id,
			ndev->node_info->slv_rpm_id, rpm_ctx,
			ndev->node_bw[ctx].sum_ab);
	}
exit_send_rpm_msg:
	return ret;
}

static int flush_bw_data(struct msm_bus_node_device_type *node_info, int ctx)
{
	int ret = 0;

	if (!node_info) {
		MSM_BUS_ERR("%s: Unable to find bus device for device",
			__func__);
		ret = -ENODEV;
		goto exit_flush_bw_data;
	}

	if (node_info->node_bw[ctx].last_sum_ab !=
				node_info->node_bw[ctx].sum_ab) {
		if (node_info->ap_owned) {
			struct msm_bus_node_device_type *bus_device =
			to_msm_bus_node(node_info->node_info->bus_device);
			struct msm_bus_fab_device_type *fabdev =
							bus_device->fabdev;

			/*
			 * For AP owned ports, only care about the Active
			 * context bandwidth.
			 */
			if (fabdev && (ctx == ACTIVE_CTX) &&
				fabdev->noc_ops.update_bw_reg &&
				fabdev->noc_ops.update_bw_reg
					(node_info->node_info->qos_params.mode))
				ret = fabdev->noc_ops.set_bw(node_info,
							fabdev->qos_base,
							fabdev->base_offset,
							fabdev->qos_off,
							fabdev->qos_freq);
		} else {
			ret = send_rpm_msg(node_info, ctx);

			if (ret)
				MSM_BUS_ERR("%s: Failed to send RPM msg for%d",
				__func__, node_info->node_info->id);
		}
		node_info->node_bw[ctx].last_sum_ab =
					node_info->node_bw[ctx].sum_ab;
	}

exit_flush_bw_data:
	return ret;

}

static int flush_clk_data(struct msm_bus_node_device_type *node, int ctx)
{
	struct nodeclk *nodeclk = NULL;
	int ret = 0;

	if (!node) {
		MSM_BUS_ERR("Unable to find bus device");
		ret = -ENODEV;
		goto exit_flush_clk_data;
	}

	nodeclk = &node->clk[ctx];

	if (IS_ERR_OR_NULL(nodeclk) || IS_ERR_OR_NULL(nodeclk->clk))
		goto exit_flush_clk_data;

	if (nodeclk->rate != node->node_bw[ctx].cur_clk_hz) {
		long rounded_rate;

		nodeclk->rate = node->node_bw[ctx].cur_clk_hz;
		nodeclk->dirty = true;

		if (nodeclk->rate) {
			rounded_rate = clk_round_rate(nodeclk->clk,
							nodeclk->rate);
			ret = setrate_nodeclk(nodeclk, rounded_rate);

			if (ret) {
				MSM_BUS_ERR("%s: Failed to set_rate %lu for %d",
					__func__, rounded_rate,
						node->node_info->id);
				ret = -ENODEV;
				goto exit_flush_clk_data;
			}

			ret = enable_nodeclk(nodeclk, &node->dev);

			if ((node->node_info->is_fab_dev) &&
				!IS_ERR_OR_NULL(node->bus_qos_clk.clk))
					ret = enable_nodeclk(&node->bus_qos_clk,
								&node->dev);
		} else {
			if ((node->node_info->is_fab_dev) &&
				!IS_ERR_OR_NULL(node->bus_qos_clk.clk))
					ret =
					disable_nodeclk(&node->bus_qos_clk);

			ret = disable_nodeclk(nodeclk);
		}

		if (ret) {
			MSM_BUS_ERR("%s: Failed to enable for %d", __func__,
						node->node_info->id);
			ret = -ENODEV;
			goto exit_flush_clk_data;
		}
		MSM_BUS_DBG("%s: Updated %d clk to %llu", __func__,
				node->node_info->id, nodeclk->rate);
	}
exit_flush_clk_data:
	/* Reset the aggregated clock rate for fab devices*/
	if (node && node->node_info->is_fab_dev)
		node->node_bw[ctx].cur_clk_hz = 0;

	if (nodeclk)
		nodeclk->dirty = 0;
	return ret;
}

static int msm_bus_agg_fab_clks(struct msm_bus_node_device_type *bus_dev)
{
	int ret = 0;
	struct msm_bus_node_device_type *node;
	int ctx;

	list_for_each_entry(node, &bus_dev->devlist, dev_link) {
		for (ctx = 0; ctx < NUM_CTX; ctx++) {
			if (node->node_bw[ctx].cur_clk_hz >=
					bus_dev->node_bw[ctx].cur_clk_hz)
				bus_dev->node_bw[ctx].cur_clk_hz =
						node->node_bw[ctx].cur_clk_hz;
		}
	}
	return ret;
}

static void msm_bus_log_fab_max_votes(struct msm_bus_node_device_type *bus_dev)
{
	int ctx;
	struct timespec ts;
	uint32_t vrail_comp = 0;
	struct msm_bus_node_device_type *node;
	uint64_t max_ib, max_ib_temp[NUM_CTX];

	for (ctx = 0; ctx < NUM_CTX; ctx++) {
		max_ib_temp[ctx] = 0;
		bus_dev->node_bw[ctx].max_ib = 0;
		bus_dev->node_bw[ctx].max_ab = 0;
		bus_dev->node_bw[ctx].max_ib_cl_name = NULL;
		bus_dev->node_bw[ctx].max_ab_cl_name = NULL;
	}

	list_for_each_entry(node, &bus_dev->devlist, dev_link) {
		for (ctx = 0; ctx < NUM_CTX; ctx++) {
			max_ib = node->node_bw[ctx].max_ib;
			vrail_comp = node->node_bw[ctx].vrail_used;

			if (vrail_comp && (vrail_comp != 100)) {
				max_ib *= 100;
				max_ib = msm_bus_div64(vrail_comp, max_ib);
			}

			if (max_ib > max_ib_temp[ctx]) {
				max_ib_temp[ctx] = max_ib;
				bus_dev->node_bw[ctx].max_ib =
					node->node_bw[ctx].max_ib;
				bus_dev->node_bw[ctx].max_ib_cl_name =
					node->node_bw[ctx].max_ib_cl_name;
			}

			if (node->node_bw[ctx].max_ab >
					bus_dev->node_bw[ctx].max_ab) {
				bus_dev->node_bw[ctx].max_ab =
					node->node_bw[ctx].max_ab;
				bus_dev->node_bw[ctx].max_ab_cl_name =
					node->node_bw[ctx].max_ab_cl_name;
			}
		}
	}

	ts = ktime_to_timespec(ktime_get());
	for (ctx = 0; ctx < NUM_CTX; ctx++) {
		trace_bus_max_votes((int)ts.tv_sec, (int)ts.tv_nsec,
				bus_dev->node_info->name,
				((ctx == ACTIVE_CTX) ? "active" : "sleep"),
				"ib", bus_dev->node_bw[ctx].max_ib,
				bus_dev->node_bw[ctx].max_ib_cl_name);
	}

	for (ctx = 0; ctx < NUM_CTX; ctx++) {
		trace_bus_max_votes((int)ts.tv_sec, (int)ts.tv_nsec,
				bus_dev->node_info->name,
				((ctx == ACTIVE_CTX) ? "active" : "sleep"),
				"ab", bus_dev->node_bw[ctx].max_ab,
				bus_dev->node_bw[ctx].max_ab_cl_name);
	}
}

int msm_bus_commit_data(struct list_head *clist)
{
	int ret = 0;
	int ctx;
	struct msm_bus_node_device_type *node;
	struct msm_bus_node_device_type *node_tmp;

	list_for_each_entry(node, clist, link) {
		/* Aggregate the bus clocks */
		if (node->node_info->is_fab_dev) {
			msm_bus_agg_fab_clks(node);
			msm_bus_log_fab_max_votes(node);
		}
	}

	list_for_each_entry_safe(node, node_tmp, clist, link) {
		if (unlikely(node->node_info->defer_qos))
				msm_bus_dev_init_qos(&node->dev, NULL);

		for (ctx = 0; ctx < NUM_CTX; ctx++) {
			ret = flush_clk_data(node, ctx);
			if (ret)
				MSM_BUS_ERR("%s: Err flushing clk data for:%d",
						__func__, node->node_info->id);
			ret = flush_bw_data(node, ctx);
			if (ret)
				MSM_BUS_ERR("%s: Error flushing bw data for %d",
					__func__, node->node_info->id);
		}
		node->dirty = false;
		list_del_init(&node->link);
	}
	return ret;
}

void *msm_bus_realloc_devmem(struct device *dev, void *p, size_t old_size,
					size_t new_size, gfp_t flags)
{
	void *ret;
	size_t copy_size = old_size;

	if (!new_size) {
		devm_kfree(dev, p);
		return ZERO_SIZE_PTR;
	}

	if (new_size < old_size)
		copy_size = new_size;

	ret = devm_kzalloc(dev, new_size, flags);
	if (!ret) {
		MSM_BUS_ERR("%s: Error Reallocating memory", __func__);
		goto exit_realloc_devmem;
	}

	memcpy(ret, p, copy_size);
	devm_kfree(dev, p);
exit_realloc_devmem:
	return ret;
}

static void msm_bus_fab_init_noc_ops(struct msm_bus_node_device_type *bus_dev)
{
	switch (bus_dev->fabdev->bus_type) {
	case MSM_BUS_NOC:
		msm_bus_noc_set_ops(bus_dev);
		break;
	case MSM_BUS_BIMC:
		msm_bus_bimc_set_ops(bus_dev);
		break;
	default:
		MSM_BUS_ERR("%s: Invalid Bus type", __func__);
	}
}

static int msm_bus_disable_node_qos_clk(struct msm_bus_node_device_type *node)
{
	struct msm_bus_node_device_type *bus_node = NULL;
	int i;
	int ret = 0;

	if (!node || (!to_msm_bus_node(node->node_info->bus_device))) {
		ret = -ENXIO;
		goto exit_disable_node_qos_clk;
	}
	bus_node = to_msm_bus_node(node->node_info->bus_device);

	for (i = 0; i < bus_node->num_node_qos_clks; i++)
		ret = disable_nodeclk(&bus_node->node_qos_clks[i]);

exit_disable_node_qos_clk:
	return ret;
}

static int msm_bus_enable_node_qos_clk(struct msm_bus_node_device_type *node)
{
	struct msm_bus_node_device_type *bus_node = NULL;
	int i;
	int ret = 0;
	long rounded_rate;

	if (!node || (!to_msm_bus_node(node->node_info->bus_device))) {
		ret = -ENXIO;
		goto exit_enable_node_qos_clk;
	}
	bus_node = to_msm_bus_node(node->node_info->bus_device);

	for (i = 0; i < bus_node->num_node_qos_clks; i++) {
		if (!bus_node->node_qos_clks[i].enable_only_clk) {
			rounded_rate =
				clk_round_rate(
					bus_node->node_qos_clks[i].clk, 1);
			ret = setrate_nodeclk(&bus_node->node_qos_clks[i],
								rounded_rate);
			if (ret)
				MSM_BUS_DBG("%s: Failed set rate clk,node %d\n",
					__func__, node->node_info->id);
		}
		ret = enable_nodeclk(&bus_node->node_qos_clks[i],
					node->node_info->bus_device);
		if (ret) {
			MSM_BUS_DBG("%s: Failed to set Qos Clks ret %d\n",
				__func__, ret);
			msm_bus_disable_node_qos_clk(node);
			goto exit_enable_node_qos_clk;
		}

	}
exit_enable_node_qos_clk:
	return ret;
}

int msm_bus_enable_limiter(struct msm_bus_node_device_type *node_dev,
				int enable, uint64_t lim_bw)
{
	int ret = 0;
	struct msm_bus_node_device_type *bus_node_dev;

	if (!node_dev) {
		MSM_BUS_ERR("No device specified");
		ret = -ENXIO;
		goto exit_enable_limiter;
	}

	if (!node_dev->ap_owned) {
		MSM_BUS_ERR("Device is not AP owned %d",
						node_dev->node_info->id);
		ret = -ENXIO;
		goto exit_enable_limiter;
	}

	bus_node_dev = to_msm_bus_node(node_dev->node_info->bus_device);
	if (!bus_node_dev) {
		MSM_BUS_ERR("Unable to get bus device infofor %d",
			node_dev->node_info->id);
		ret = -ENXIO;
		goto exit_enable_limiter;
	}
	if (bus_node_dev->fabdev &&
		bus_node_dev->fabdev->noc_ops.limit_mport) {
		if (ret < 0) {
			MSM_BUS_ERR("Can't Enable QoS clk %d",
				node_dev->node_info->id);
			goto exit_enable_limiter;
		}
		bus_node_dev->fabdev->noc_ops.limit_mport(
				node_dev,
				bus_node_dev->fabdev->qos_base,
				bus_node_dev->fabdev->base_offset,
				bus_node_dev->fabdev->qos_off,
				bus_node_dev->fabdev->qos_freq,
				enable, lim_bw);
	}

exit_enable_limiter:
	return ret;
}

static int msm_bus_dev_init_qos(struct device *dev, void *data)
{
	int ret = 0;
	struct msm_bus_node_device_type *node_dev = NULL;

	node_dev = to_msm_bus_node(dev);
	if (!node_dev) {
		MSM_BUS_ERR("%s: Unable to get node device info" , __func__);
		ret = -ENXIO;
		goto exit_init_qos;
	}

	MSM_BUS_DBG("Device = %d", node_dev->node_info->id);

	if (node_dev->ap_owned) {
		struct msm_bus_node_device_type *bus_node_info;

		bus_node_info =
			to_msm_bus_node(node_dev->node_info->bus_device);

		if (!bus_node_info) {
			MSM_BUS_ERR("%s: Unable to get bus device info for %d",
				__func__,
				node_dev->node_info->id);
			ret = -ENXIO;
			goto exit_init_qos;
		}

		if (bus_node_info->fabdev &&
			bus_node_info->fabdev->noc_ops.qos_init) {
			int ret = 0;

			if (node_dev->ap_owned &&
				(node_dev->node_info->qos_params.mode) != -1) {

				if (bus_node_info->fabdev->bypass_qos_prg)
					goto exit_init_qos;

				ret = msm_bus_enable_node_qos_clk(node_dev);
				if (ret < 0) {
					MSM_BUS_DBG("Can't Enable QoS clk %d\n",
					node_dev->node_info->id);
					node_dev->node_info->defer_qos = true;
					goto exit_init_qos;
				}

				bus_node_info->fabdev->noc_ops.qos_init(
					node_dev,
					bus_node_info->fabdev->qos_base,
					bus_node_info->fabdev->base_offset,
					bus_node_info->fabdev->qos_off,
					bus_node_info->fabdev->qos_freq);
				ret = msm_bus_disable_node_qos_clk(node_dev);
				node_dev->node_info->defer_qos = false;
			}
		} else
			MSM_BUS_ERR("%s: Skipping QOS init for %d",
				__func__, node_dev->node_info->id);
	}
exit_init_qos:
	return ret;
}

static int msm_bus_fabric_init(struct device *dev,
			struct msm_bus_node_device_type *pdata)
{
	struct msm_bus_fab_device_type *fabdev;
	struct msm_bus_node_device_type *node_dev = NULL;
	int ret = 0;

	node_dev = to_msm_bus_node(dev);
	if (!node_dev) {
		MSM_BUS_ERR("%s: Unable to get bus device info" , __func__);
		ret = -ENXIO;
		goto exit_fabric_init;
	}

	if (node_dev->node_info->virt_dev) {
		MSM_BUS_ERR("%s: Skip Fab init for virtual device %d", __func__,
						node_dev->node_info->id);
		goto exit_fabric_init;
	}

	fabdev = devm_kzalloc(dev, sizeof(struct msm_bus_fab_device_type),
								GFP_KERNEL);
	if (!fabdev) {
		MSM_BUS_ERR("Fabric alloc failed\n");
		ret = -ENOMEM;
		goto exit_fabric_init;
	}

	node_dev->fabdev = fabdev;
	fabdev->pqos_base = pdata->fabdev->pqos_base;
	fabdev->qos_range = pdata->fabdev->qos_range;
	fabdev->base_offset = pdata->fabdev->base_offset;
	fabdev->qos_off = pdata->fabdev->qos_off;
	fabdev->qos_freq = pdata->fabdev->qos_freq;
	fabdev->bus_type = pdata->fabdev->bus_type;
	fabdev->bypass_qos_prg = pdata->fabdev->bypass_qos_prg;
	msm_bus_fab_init_noc_ops(node_dev);

	fabdev->qos_base = devm_ioremap(dev,
				fabdev->pqos_base, fabdev->qos_range);
	if (!fabdev->qos_base) {
		MSM_BUS_ERR("%s: Error remapping address 0x%zx :bus device %d",
			__func__,
			 (size_t)fabdev->pqos_base, node_dev->node_info->id);
		ret = -ENOMEM;
		goto exit_fabric_init;
	}

exit_fabric_init:
	return ret;
}

static int msm_bus_init_clk(struct device *bus_dev,
				struct msm_bus_node_device_type *pdata)
{
	unsigned int ctx;
	struct msm_bus_node_device_type *node_dev = to_msm_bus_node(bus_dev);
	int i;

	for (ctx = 0; ctx < NUM_CTX; ctx++) {
		if (!IS_ERR_OR_NULL(pdata->clk[ctx].clk)) {
			node_dev->clk[ctx].clk = pdata->clk[ctx].clk;
			node_dev->clk[ctx].enable_only_clk =
					pdata->clk[ctx].enable_only_clk;
			node_dev->clk[ctx].setrate_only_clk =
					pdata->clk[ctx].setrate_only_clk;
			node_dev->clk[ctx].enable = false;
			node_dev->clk[ctx].dirty = false;
			strlcpy(node_dev->clk[ctx].reg_name,
				pdata->clk[ctx].reg_name, MAX_REG_NAME);
			node_dev->clk[ctx].reg = NULL;
			bus_get_reg(&node_dev->clk[ctx], bus_dev);
			MSM_BUS_DBG("%s: Valid node clk node %d ctx %d\n",
				__func__, node_dev->node_info->id, ctx);
		}
	}

	if (!IS_ERR_OR_NULL(pdata->bus_qos_clk.clk)) {
		node_dev->bus_qos_clk.clk = pdata->bus_qos_clk.clk;
		node_dev->bus_qos_clk.enable_only_clk =
					pdata->bus_qos_clk.enable_only_clk;
		node_dev->bus_qos_clk.setrate_only_clk =
					pdata->bus_qos_clk.setrate_only_clk;
		node_dev->bus_qos_clk.enable = false;
		strlcpy(node_dev->bus_qos_clk.reg_name,
			pdata->bus_qos_clk.reg_name, MAX_REG_NAME);
		node_dev->bus_qos_clk.reg = NULL;
		MSM_BUS_DBG("%s: Valid bus qos clk node %d\n", __func__,
						node_dev->node_info->id);
	}

	if (pdata->num_node_qos_clks) {
		node_dev->num_node_qos_clks = pdata->num_node_qos_clks;
		node_dev->node_qos_clks = devm_kzalloc(bus_dev,
			(node_dev->num_node_qos_clks * sizeof(struct nodeclk)),
			GFP_KERNEL);
		if (!node_dev->node_qos_clks) {
			dev_err(bus_dev, "Failed to alloc memory for qos clk");
			return -ENOMEM;
		}

		for (i = 0; i < pdata->num_node_qos_clks; i++) {
			node_dev->node_qos_clks[i].clk =
					pdata->node_qos_clks[i].clk;
			node_dev->node_qos_clks[i].enable_only_clk =
					pdata->node_qos_clks[i].enable_only_clk;
			node_dev->node_qos_clks[i].setrate_only_clk =
				pdata->node_qos_clks[i].setrate_only_clk;
			node_dev->node_qos_clks[i].enable = false;
			strlcpy(node_dev->node_qos_clks[i].reg_name,
				pdata->node_qos_clks[i].reg_name, MAX_REG_NAME);
			node_dev->node_qos_clks[i].reg = NULL;
			MSM_BUS_DBG("%s: Valid qos clk[%d] node %d %d Reg%s\n",
					__func__, i,
					node_dev->node_info->id,
					node_dev->num_node_qos_clks,
					node_dev->node_qos_clks[i].reg_name);
		}
	}

	return 0;
}

static int msm_bus_copy_node_info(struct msm_bus_node_device_type *pdata,
				struct device *bus_dev)
{
	int ret = 0;
	struct msm_bus_node_info_type *node_info = NULL;
	struct msm_bus_node_info_type *pdata_node_info = NULL;
	struct msm_bus_node_device_type *bus_node = NULL;

	bus_node = to_msm_bus_node(bus_dev);

	if (!bus_node || !pdata) {
		ret = -ENXIO;
		MSM_BUS_ERR("%s: Invalid pointers pdata %p, bus_node %p",
			__func__, pdata, bus_node);
		goto exit_copy_node_info;
	}

	node_info = bus_node->node_info;
	pdata_node_info = pdata->node_info;

	node_info->name = pdata_node_info->name;
	node_info->id =  pdata_node_info->id;
	node_info->bus_device_id = pdata_node_info->bus_device_id;
	node_info->mas_rpm_id = pdata_node_info->mas_rpm_id;
	node_info->slv_rpm_id = pdata_node_info->slv_rpm_id;
	node_info->num_connections = pdata_node_info->num_connections;
	node_info->num_blist = pdata_node_info->num_blist;
	node_info->num_qports = pdata_node_info->num_qports;
	node_info->virt_dev = pdata_node_info->virt_dev;
	node_info->is_fab_dev = pdata_node_info->is_fab_dev;
	node_info->qos_params.mode = pdata_node_info->qos_params.mode;
	node_info->qos_params.prio1 = pdata_node_info->qos_params.prio1;
	node_info->qos_params.prio0 = pdata_node_info->qos_params.prio0;
	node_info->qos_params.reg_prio1 = pdata_node_info->qos_params.reg_prio1;
	node_info->qos_params.reg_prio0 = pdata_node_info->qos_params.reg_prio0;
	node_info->qos_params.prio_lvl = pdata_node_info->qos_params.prio_lvl;
	node_info->qos_params.prio_rd = pdata_node_info->qos_params.prio_rd;
	node_info->qos_params.prio_wr = pdata_node_info->qos_params.prio_wr;
	node_info->qos_params.gp = pdata_node_info->qos_params.gp;
	node_info->qos_params.thmp = pdata_node_info->qos_params.thmp;
	node_info->qos_params.ws = pdata_node_info->qos_params.ws;
	node_info->qos_params.bw_buffer = pdata_node_info->qos_params.bw_buffer;
	node_info->agg_params.buswidth = pdata_node_info->agg_params.buswidth;
	node_info->agg_params.agg_scheme =
					pdata_node_info->agg_params.agg_scheme;
	node_info->agg_params.vrail_comp =
					pdata_node_info->agg_params.vrail_comp;
	node_info->agg_params.num_aggports =
				pdata_node_info->agg_params.num_aggports;
	node_info->agg_params.num_util_levels =
				pdata_node_info->agg_params.num_util_levels;
	node_info->agg_params.util_levels = devm_kzalloc(bus_dev,
			sizeof(struct node_util_levels_type) *
			node_info->agg_params.num_util_levels,
			GFP_KERNEL);
	if (!node_info->agg_params.util_levels) {
		MSM_BUS_ERR("%s: Agg util level alloc failed\n", __func__);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}
	memcpy(node_info->agg_params.util_levels,
		pdata_node_info->agg_params.util_levels,
		sizeof(struct node_util_levels_type) *
			pdata_node_info->agg_params.num_util_levels);

	node_info->dev_connections = devm_kzalloc(bus_dev,
			sizeof(struct device *) *
				pdata_node_info->num_connections,
			GFP_KERNEL);
	if (!node_info->dev_connections) {
		MSM_BUS_ERR("%s:Bus dev connections alloc failed\n", __func__);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	node_info->connections = devm_kzalloc(bus_dev,
			sizeof(int) * pdata_node_info->num_connections,
			GFP_KERNEL);
	if (!node_info->connections) {
		MSM_BUS_ERR("%s:Bus connections alloc failed\n", __func__);
		devm_kfree(bus_dev, node_info->dev_connections);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	memcpy(node_info->connections,
		pdata_node_info->connections,
		sizeof(int) * pdata_node_info->num_connections);

	node_info->black_connections = devm_kzalloc(bus_dev,
			sizeof(struct device *) *
				pdata_node_info->num_blist,
			GFP_KERNEL);
	if (!node_info->black_connections) {
		MSM_BUS_ERR("%s: Bus black connections alloc failed\n",
			__func__);
		devm_kfree(bus_dev, node_info->dev_connections);
		devm_kfree(bus_dev, node_info->connections);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	node_info->black_listed_connections = devm_kzalloc(bus_dev,
			pdata_node_info->num_blist * sizeof(int),
			GFP_KERNEL);
	if (!node_info->black_listed_connections) {
		MSM_BUS_ERR("%s:Bus black list connections alloc failed\n",
					__func__);
		devm_kfree(bus_dev, node_info->black_connections);
		devm_kfree(bus_dev, node_info->dev_connections);
		devm_kfree(bus_dev, node_info->connections);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	memcpy(node_info->black_listed_connections,
		pdata_node_info->black_listed_connections,
		sizeof(int) * pdata_node_info->num_blist);

	node_info->qport = devm_kzalloc(bus_dev,
			sizeof(int) * pdata_node_info->num_qports,
			GFP_KERNEL);
	if (!node_info->qport) {
		MSM_BUS_ERR("%s:Bus qport allocation failed\n", __func__);
		devm_kfree(bus_dev, node_info->dev_connections);
		devm_kfree(bus_dev, node_info->connections);
		devm_kfree(bus_dev, node_info->black_listed_connections);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	memcpy(node_info->qport,
		pdata_node_info->qport,
		sizeof(int) * pdata_node_info->num_qports);

exit_copy_node_info:
	return ret;
}

static struct device *msm_bus_device_init(
			struct msm_bus_node_device_type *pdata)
{
	struct device *bus_dev = NULL;
	struct msm_bus_node_device_type *bus_node = NULL;
	struct msm_bus_node_info_type *node_info = NULL;
	int ret = 0;

	/**
	* Init here so we can use devm calls
	*/

	bus_node = kzalloc(sizeof(struct msm_bus_node_device_type), GFP_KERNEL);
	if (!bus_node) {
		ret = -ENOMEM;
		goto err_device_init;
	}
	bus_dev = &bus_node->dev;
	device_initialize(bus_dev);

	node_info = devm_kzalloc(bus_dev,
			sizeof(struct msm_bus_node_info_type), GFP_KERNEL);
	if (!node_info) {
		ret = -ENOMEM;
		goto err_put_device;
	}

	bus_node->node_info = node_info;
	bus_node->ap_owned = pdata->ap_owned;
	bus_dev->of_node = pdata->of_node;

	ret = msm_bus_copy_node_info(pdata, bus_dev);
	if (ret)
		goto err_put_device;

	bus_dev->bus = &msm_bus_type;
	dev_set_name(bus_dev, bus_node->node_info->name);

	ret = device_add(bus_dev);
	if (ret) {
		MSM_BUS_ERR("%s: Error registering device %d",
				__func__, pdata->node_info->id);
		goto err_put_device;
	}
	device_create_file(bus_dev, &dev_attr_bw);
	INIT_LIST_HEAD(&bus_node->devlist);
	return bus_dev;
err_put_device:
	put_device(bus_dev);
	bus_dev = NULL;
	kfree(bus_node);
err_device_init:
	return ERR_PTR(ret);
}

static int msm_bus_setup_dev_conn(struct device *bus_dev, void *data)
{
	struct msm_bus_node_device_type *bus_node = NULL;
	int ret = 0;
	int j;
	struct msm_bus_node_device_type *fab;

	bus_node = to_msm_bus_node(bus_dev);
	if (!bus_node) {
		MSM_BUS_ERR("%s: Can't get device info", __func__);
		ret = -ENODEV;
		goto exit_setup_dev_conn;
	}

	/* Setup parent bus device for this node */
	if (!bus_node->node_info->is_fab_dev) {
		struct device *bus_parent_device =
			bus_find_device(&msm_bus_type, NULL,
				(void *)&bus_node->node_info->bus_device_id,
				msm_bus_device_match_adhoc);

		if (!bus_parent_device) {
			MSM_BUS_ERR("%s: Error finding parentdev %d parent %d",
				__func__,
				bus_node->node_info->id,
				bus_node->node_info->bus_device_id);
			ret = -ENXIO;
			goto exit_setup_dev_conn;
		}
		bus_node->node_info->bus_device = bus_parent_device;
		fab = to_msm_bus_node(bus_parent_device);
		list_add_tail(&bus_node->dev_link, &fab->devlist);
	}

	bus_node->node_info->is_traversed = false;

	for (j = 0; j < bus_node->node_info->num_connections; j++) {
		bus_node->node_info->dev_connections[j] =
			bus_find_device(&msm_bus_type, NULL,
				(void *)&bus_node->node_info->connections[j],
				msm_bus_device_match_adhoc);

		if (!bus_node->node_info->dev_connections[j]) {
			MSM_BUS_ERR("%s: Error finding conn %d for device %d",
				__func__, bus_node->node_info->connections[j],
				 bus_node->node_info->id);
			ret = -ENODEV;
			goto exit_setup_dev_conn;
		}
	}

	for (j = 0; j < bus_node->node_info->num_blist; j++) {
		bus_node->node_info->black_connections[j] =
			bus_find_device(&msm_bus_type, NULL,
				(void *)&bus_node->node_info->
				black_listed_connections[j],
				msm_bus_device_match_adhoc);

		if (!bus_node->node_info->black_connections[j]) {
			MSM_BUS_ERR("%s: Error finding conn %d for device %d\n",
				__func__, bus_node->node_info->
				black_listed_connections[j],
				bus_node->node_info->id);
			ret = -ENODEV;
			goto exit_setup_dev_conn;
		}
	}

exit_setup_dev_conn:
	return ret;
}

static int msm_bus_node_debug(struct device *bus_dev, void *data)
{
	int j;
	int ret = 0;
	struct msm_bus_node_device_type *bus_node = NULL;

	bus_node = to_msm_bus_node(bus_dev);
	if (!bus_node) {
		MSM_BUS_ERR("%s: Can't get device info", __func__);
		ret = -ENODEV;
		goto exit_node_debug;
	}

	MSM_BUS_DBG("Device = %d buswidth %u", bus_node->node_info->id,
				bus_node->node_info->agg_params.buswidth);
	for (j = 0; j < bus_node->node_info->num_connections; j++) {
		struct msm_bus_node_device_type *bdev =
		to_msm_bus_node(bus_node->node_info->dev_connections[j]);
		MSM_BUS_DBG("\n\t Connection[%d] %d", j, bdev->node_info->id);
	}

	if (bus_node->node_info->is_fab_dev)
		msm_bus_floor_init(bus_dev);

exit_node_debug:
	return ret;
}

static int msm_bus_free_dev(struct device *dev, void *data)
{
	struct msm_bus_node_device_type *bus_node = NULL;

	bus_node = to_msm_bus_node(dev);

	if (bus_node)
		MSM_BUS_ERR("\n%s: Removing device %d", __func__,
						bus_node->node_info->id);
	device_unregister(dev);
	kfree(bus_node);
	return 0;
}

int msm_bus_device_remove(struct platform_device *pdev)
{
	bus_for_each_dev(&msm_bus_type, NULL, NULL, msm_bus_free_dev);
	return 0;
}

/**
 * msm_bus_panic_callback() - panic notification callback function.
 *              This function is invoked when a kernel panic occurs.
 * @nfb:        Notifier block pointer
 * @event:      Value passed unmodified to notifier function
 * @data:       Pointer passed unmodified to notifier function
 *
 * Return: NOTIFY_OK
 */
static int msm_bus_panic_callback(struct notifier_block *nfb,
					unsigned long event, void *data)
{
	struct msm_bus_node_device_type *bus_node = NULL;
	unsigned int ctx;

	list_for_each_entry(bus_node, &fabdev_list, dev_link) {
		for (ctx = 0; ctx < NUM_CTX; ctx++) {
			if (bus_node->node_bw[ctx].max_ib_cl_name &&
				bus_node->node_bw[ctx].max_ib) {
				pr_err("%s: %s: %s max_ib: %llu: client-name: %s\n",
				__func__, bus_node->node_info->name,
				((ctx == ACTIVE_CTX) ? "active" : "sleep"),
				bus_node->node_bw[ctx].max_ib,
				bus_node->node_bw[ctx].max_ib_cl_name);
			}
		}

		for (ctx = 0; ctx < NUM_CTX; ctx++) {
			if (bus_node->node_bw[ctx].max_ab_cl_name &&
				bus_node->node_bw[ctx].max_ab) {
				pr_err("%s: %s: %s max_ab: %llu: client-name: %s\n",
				__func__, bus_node->node_info->name,
				((ctx == ACTIVE_CTX) ? "active" : "sleep"),
				bus_node->node_bw[ctx].max_ab,
				bus_node->node_bw[ctx].max_ab_cl_name);
			}
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block msm_bus_panic_notifier = {
	.notifier_call = msm_bus_panic_callback,
	.priority = 1,
};

static int msm_bus_device_probe(struct platform_device *pdev)
{
	unsigned int i, ret;
	struct msm_bus_device_node_registration *pdata;

	/* If possible, get pdata from device-tree */
	if (pdev->dev.of_node)
		pdata = msm_bus_of_to_pdata(pdev);
	else {
		pdata = (struct msm_bus_device_node_registration *)pdev->
			dev.platform_data;
	}

	if (IS_ERR_OR_NULL(pdata)) {
		MSM_BUS_ERR("No platform data found");
		ret = -ENODATA;
		goto exit_device_probe;
	}

	for (i = 0; i < pdata->num_devices; i++) {
		struct device *node_dev = NULL;
		struct msm_bus_node_device_type *bus_node = NULL;

		node_dev = msm_bus_device_init(&pdata->info[i]);

		if (IS_ERR(node_dev)) {
			MSM_BUS_ERR("%s: Error during dev init for %d",
				__func__, pdata->info[i].node_info->id);
			ret = PTR_ERR(node_dev);
			goto exit_device_probe;
		}

		ret = msm_bus_init_clk(node_dev, &pdata->info[i]);
		if (ret) {
			MSM_BUS_ERR("\n Failed to init bus clk. ret %d", ret);
			msm_bus_device_remove(pdev);
			goto exit_device_probe;
		}
		/*Is this a fabric device ?*/
		if (pdata->info[i].node_info->is_fab_dev) {
			MSM_BUS_DBG("%s: %d is a fab", __func__,
						pdata->info[i].node_info->id);
			ret = msm_bus_fabric_init(node_dev, &pdata->info[i]);
			if (ret) {
				MSM_BUS_ERR("%s: Error intializing fab %d",
					__func__, pdata->info[i].node_info->id);
				goto exit_device_probe;
			}

			bus_node = to_msm_bus_node(node_dev);
			list_add_tail(&bus_node->dev_link, &fabdev_list);
		}
	}

	ret = bus_for_each_dev(&msm_bus_type, NULL, NULL,
						msm_bus_setup_dev_conn);
	if (ret) {
		MSM_BUS_ERR("%s: Error setting up dev connections", __func__);
		goto exit_device_probe;
	}

	/*
	 * Setup the QoS for the nodes, don't check the error codes as we
	 * defer QoS programming to the first transaction in cases of failure
	 * and we want to continue the probe.
	 */
	ret = bus_for_each_dev(&msm_bus_type, NULL, NULL, msm_bus_dev_init_qos);

	/* Register the arb layer ops */
	msm_bus_arb_setops_adhoc(&arb_ops);
	bus_for_each_dev(&msm_bus_type, NULL, NULL, msm_bus_node_debug);

	atomic_notifier_chain_register(&panic_notifier_list,
						&msm_bus_panic_notifier);

	devm_kfree(&pdev->dev, pdata->info);
	devm_kfree(&pdev->dev, pdata);
exit_device_probe:
	return ret;
}

static int msm_bus_device_rules_probe(struct platform_device *pdev)
{
	struct bus_rule_type *rule_data = NULL;
	int num_rules = 0;

	num_rules = msm_bus_of_get_static_rules(pdev, &rule_data);

	if (!rule_data)
		goto exit_rules_probe;

	msm_rule_register(num_rules, rule_data, NULL);
	static_rules.num_rules = num_rules;
	static_rules.rules = rule_data;
	pdev->dev.platform_data = &static_rules;

exit_rules_probe:
	return 0;
}

int msm_bus_device_rules_remove(struct platform_device *pdev)
{
	struct static_rules_type *static_rules = NULL;

	static_rules = pdev->dev.platform_data;
	if (static_rules)
		msm_rule_unregister(static_rules->num_rules,
					static_rules->rules, NULL);
	return 0;
}


static struct of_device_id rules_match[] = {
	{.compatible = "qcom,msm-bus-static-bw-rules"},
	{}
};

static struct platform_driver msm_bus_rules_driver = {
	.probe = msm_bus_device_rules_probe,
	.remove = msm_bus_device_rules_remove,
	.driver = {
		.name = "msm_bus_rules_device",
		.owner = THIS_MODULE,
		.of_match_table = rules_match,
	},
};

static struct of_device_id fabric_match[] = {
	{.compatible = "qcom,msm-bus-device"},
	{}
};

static struct platform_driver msm_bus_device_driver = {
	.probe = msm_bus_device_probe,
	.remove = msm_bus_device_remove,
	.driver = {
		.name = "msm_bus_device",
		.owner = THIS_MODULE,
		.of_match_table = fabric_match,
	},
};

int __init msm_bus_device_init_driver(void)
{
	int rc;

	MSM_BUS_ERR("msm_bus_fabric_init_driver\n");
	rc =  platform_driver_register(&msm_bus_device_driver);

	if (rc) {
		MSM_BUS_ERR("Failed to register bus device driver");
		return rc;
	}
	return platform_driver_register(&msm_bus_rules_driver);
}
subsys_initcall(msm_bus_device_init_driver);
