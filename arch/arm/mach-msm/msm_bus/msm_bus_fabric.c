/* Copyright (c) 2010-2014, Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "AXI: %s(): " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/radix-tree.h>
#include <mach/board.h>
#include "msm_bus_core.h"

enum {
	SLAVE_NODE,
	MASTER_NODE,
	CLK_NODE,
};

enum {
	DISABLE,
	ENABLE,
};

struct msm_bus_fabric {
	struct msm_bus_fabric_device fabdev;
	int ahb;
	void *cdata[NUM_CTX];
	bool arb_dirty;
	bool clk_dirty;
	struct radix_tree_root fab_tree;
	int num_nodes;
	struct list_head gateways;
	struct msm_bus_inode_info info;
	struct msm_bus_fabric_registration *pdata;
	void *hw_data;
};
#define to_msm_bus_fabric(d) container_of(d, \
	struct msm_bus_fabric, d)

/**
 * msm_bus_fabric_add_node() - Add a node to the fabric structure
 * @fabric: Fabric device to which the node should be added
 * @info: The node to be added
 */
static int msm_bus_fabric_add_node(struct msm_bus_fabric *fabric,
	struct msm_bus_inode_info *info)
{
	int status = -ENOMEM, ctx;
	MSM_BUS_DBG("msm_bus_fabric_add_node: ID %d Gw: %d\n",
		info->node_info->priv_id, info->node_info->gateway);
	status = radix_tree_preload(GFP_ATOMIC);
	if (status)
		goto out;

	status = radix_tree_insert(&fabric->fab_tree, info->node_info->priv_id,
			info);
	radix_tree_preload_end();
	if (IS_SLAVE(info->node_info->priv_id))
		radix_tree_tag_set(&fabric->fab_tree, info->node_info->priv_id,
			SLAVE_NODE);

	for (ctx = 0; ctx < NUM_CTX; ctx++) {
		if (info->node_info->slaveclk[ctx]) {
			radix_tree_tag_set(&fabric->fab_tree,
				info->node_info->priv_id, CLK_NODE);
			break;
		}

		info->nodeclk[ctx].enable = false;
		info->nodeclk[ctx].dirty = false;
	}

out:
	return status;
}

/**
 * msm_bus_add_fab() - Add a fabric (gateway) to the current fabric
 * @fabric: Fabric device to which the gateway info should be added
 * @info: Gateway node to be added to the fabric
 */
static int msm_bus_fabric_add_fab(struct msm_bus_fabric *fabric,
	struct msm_bus_inode_info *info)
{
	struct msm_bus_fabnodeinfo *fabnodeinfo;
	MSM_BUS_DBG("msm_bus_fabric_add_fab: ID %d Gw: %d\n",
		info->node_info->priv_id, info->node_info->gateway);
	fabnodeinfo = kzalloc(sizeof(struct msm_bus_fabnodeinfo), GFP_KERNEL);
	if (fabnodeinfo == NULL) {
		MSM_FAB_ERR("msm_bus_fabric_add_fab: "
			"No Node Info\n");
		MSM_FAB_ERR("axi: Cannot register fabric!\n");
		return -ENOMEM;
	}

	fabnodeinfo->info = info;
	fabnodeinfo->info->num_pnodes = -1;
	list_add_tail(&fabnodeinfo->list, &fabric->gateways);
	return 0;
}

/**
 * register_fabric_info() - Create the internal fabric structure and
 * build the topology tree from platform specific data
 * @pdev: Platform device for getting base addresses
 * @fabric: Fabric to which the gateways, nodes should be added
 *
 * This function is called from probe. Iterates over the platform data,
 * and builds the topology
 */
static int register_fabric_info(struct platform_device *pdev,
	struct msm_bus_fabric *fabric)
{
	int i = 0, ret = 0, err = 0;

	MSM_BUS_DBG("id:%d pdata-id: %d len: %d\n", fabric->fabdev.id,
		fabric->pdata->id, fabric->pdata->len);
	fabric->hw_data = fabric->fabdev.hw_algo.allocate_hw_data(pdev,
		fabric->pdata);
	if (ZERO_OR_NULL_PTR(fabric->hw_data) && fabric->pdata->ahb == 0) {
		MSM_BUS_ERR("Couldn't allocate hw_data for fab: %d\n",
			fabric->fabdev.id);
		goto error;
	}

	for (i = 0; i < fabric->pdata->len; i++) {
		struct msm_bus_inode_info *info;
		int ctx, j;

		info = kzalloc(sizeof(struct msm_bus_inode_info), GFP_KERNEL);
		if (info == NULL) {
			MSM_BUS_ERR("Error allocating info\n");
			return -ENOMEM;
		}

		info->node_info = fabric->pdata->info + i;
		info->commit_index = -1;
		info->num_pnodes = -1;

		for (ctx = 0; ctx < NUM_CTX; ctx++) {
			if (info->node_info->slaveclk[ctx]) {
				info->nodeclk[ctx].clk = clk_get_sys("msm_bus",
					info->node_info->slaveclk[ctx]);
				if (IS_ERR(info->nodeclk[ctx].clk)) {
					MSM_BUS_ERR("Couldn't get clk %s\n",
						info->node_info->slaveclk[ctx]);
					err = -EINVAL;
				}
				info->nodeclk[ctx].enable = false;
				info->nodeclk[ctx].dirty = false;
			}

			if (info->node_info->memclk[ctx]) {
				info->memclk[ctx].clk = clk_get_sys("msm_bus",
						info->node_info->memclk[ctx]);
				if (IS_ERR(info->memclk[ctx].clk)) {
					MSM_BUS_ERR("Couldn't get clk %s\n",
						info->node_info->memclk[ctx]);
					err = -EINVAL;
				}
				info->memclk[ctx].enable = false;
				info->memclk[ctx].dirty = false;
			}
		}

		if (info->node_info->iface_clk_node) {
			info->iface_clk.clk = clk_get_sys(info->node_info->
				iface_clk_node, "iface_clk");
			if (IS_ERR(info->iface_clk.clk)) {
				MSM_BUS_ERR("ERR: Couldn't get clk %s\n",
					info->node_info->iface_clk_node);
			}
		}

		ret = info->node_info->gateway ?
			msm_bus_fabric_add_fab(fabric, info) :
			msm_bus_fabric_add_node(fabric, info);
		if (ret) {
			MSM_BUS_ERR("Unable to add node info, ret: %d\n", ret);
			kfree(info);
			goto error;
		}

		if (fabric->fabdev.hw_algo.node_init == NULL)
			continue;

		if (info->iface_clk.clk) {
			MSM_BUS_DBG("Enabled iface clock for node init: %d\n",
				info->node_info->priv_id);
			clk_prepare_enable(info->iface_clk.clk);
		}

		for (j = 0; j < NUM_CTX; j++)
			clk_prepare_enable(fabric->info.nodeclk[j].clk);

		fabric->fabdev.hw_algo.node_init(fabric->hw_data, info);
		if (ret) {
			MSM_BUS_ERR("Unable to init node info, ret: %d\n", ret);
			kfree(info);
		}

		for (j = 0; j < NUM_CTX; j++)
			clk_disable_unprepare(fabric->info.nodeclk[j].clk);

		if (info->iface_clk.clk) {
			MSM_BUS_DBG("Disable iface_clk after node init: %d\n",
				info->node_info->priv_id);
			clk_disable_unprepare(info->iface_clk.clk);
		}


	}

	MSM_BUS_DBG("Fabric: %d nmasters: %d nslaves: %d\n"
		" ntieredslaves: %d, rpm_enabled: %d\n",
		fabric->fabdev.id, fabric->pdata->nmasters,
		fabric->pdata->nslaves, fabric->pdata->ntieredslaves,
		fabric->pdata->rpm_enabled);
	MSM_BUS_DBG("msm_bus_register_fabric_info i: %d\n", i);
	fabric->num_nodes = fabric->pdata->len;
error:
	fabric->num_nodes = i;
	msm_bus_dbg_commit_data(fabric->fabdev.name, NULL, 0, 0, 0,
		MSM_BUS_DBG_REGISTER);
	return ret | err;
}

/**
 * msm_bus_fabric_update_clks() - Set the clocks for fabrics and slaves
 * @fabric: Fabric for which the clocks need to be updated
 * @slave: The node for which the clocks need to be updated
 * @index: The index for which the current clocks are set
 * @curr_clk_hz:Current clock value
 * @req_clk_hz: Requested clock value
 * @bwsum: Bandwidth Sum
 * @clk_flag: Flag determining whether fabric clock or the slave clock has to
 * be set. If clk_flag is set, fabric clock is set, else slave clock is set.
 */
static int msm_bus_fabric_update_clks(struct msm_bus_fabric_device *fabdev,
		struct msm_bus_inode_info *slave, int index,
		uint64_t curr_clk_hz, uint64_t req_clk_hz,
		uint64_t bwsum_hz, int clk_flag, int ctx,
		unsigned int cl_active_flag)
{
	int i, status = 0;
	uint64_t max_pclk = 0, rate;
	uint64_t *pclk = NULL;
	struct msm_bus_fabric *fabric = to_msm_bus_fabric(fabdev);
	struct nodeclk *nodeclk;

	/**
	 * Integration for clock rates is not required if context is not
	 * same as client's active-only flag
	 */
	if (ctx != cl_active_flag)
		goto skip_set_clks;

	/* Maximum for this gateway */
	for (i = 0; i <= slave->num_pnodes; i++) {
		if (i == index && (req_clk_hz < curr_clk_hz))
			continue;
		slave->pnode[i].sel_clk = &slave->pnode[i].clk[ctx];
		max_pclk = max(max_pclk, *slave->pnode[i].sel_clk);
	}

	*slave->link_info.sel_clk =
		max(max_pclk, max(bwsum_hz, req_clk_hz));
	/* Is this gateway or slave? */
	if (clk_flag && (!fabric->ahb)) {
		struct msm_bus_fabnodeinfo *fabgw = NULL;
		struct msm_bus_inode_info *info = NULL;
		/* Maximum of all gateways set at fabric */
		list_for_each_entry(fabgw, &fabric->gateways, list) {
			info = fabgw->info;
			if (!info)
				continue;
			info->link_info.sel_clk = &info->link_info.clk[ctx];
			max_pclk = max(max_pclk, *info->link_info.sel_clk);
		}
		MSM_BUS_DBG("max_pclk from gateways: %llu\n", max_pclk);

		/* Maximum of all slave clocks. */

		for (i = 0; i < fabric->pdata->len; i++) {
			if (fabric->pdata->info[i].gateway ||
				(fabric->pdata->info[i].id < SLAVE_ID_KEY))
				continue;
			info = radix_tree_lookup(&fabric->fab_tree,
				fabric->pdata->info[i].priv_id);
			if (!info)
				continue;
			info->link_info.sel_clk = &info->link_info.clk[ctx];
			max_pclk = max(max_pclk, *info->link_info.sel_clk);
		}


		MSM_BUS_DBG("max_pclk from slaves & gws: %llu\n", max_pclk);
		fabric->info.link_info.sel_clk =
			&fabric->info.link_info.clk[ctx];
		pclk = fabric->info.link_info.sel_clk;
	} else {
		slave->link_info.sel_clk = &slave->link_info.clk[ctx];
		pclk = slave->link_info.sel_clk;
	}


	*pclk = max(max_pclk, max(bwsum_hz, req_clk_hz));

	if (!fabric->pdata->rpm_enabled)
		goto skip_set_clks;

	if (clk_flag) {
		nodeclk = &fabric->info.nodeclk[ctx];
		if (nodeclk->clk) {
			MSM_BUS_DBG("clks: id: %d set-clk: %llu bws_hz:%llu\n",
			fabric->fabdev.id, *pclk, bwsum_hz);
			if (nodeclk->rate != *pclk) {
				nodeclk->dirty = true;
				nodeclk->rate = *pclk;
			}
			fabric->clk_dirty = true;
		}
	} else {
		nodeclk = &slave->nodeclk[ctx];
		if (nodeclk->clk) {
			rate = *pclk;
			MSM_BUS_DBG("clks: id: %d set-clk: %llu bws_hz: %llu\n",
				slave->node_info->priv_id, rate,
			bwsum_hz);
			if (nodeclk->rate != rate) {
				nodeclk->dirty = true;
				nodeclk->rate = rate;
			}
		}
		if (!status && slave->memclk[ctx].clk) {
			rate = *slave->link_info.sel_clk;
			if (slave->memclk[ctx].rate != rate) {
				slave->memclk[ctx].rate = rate;
				slave->memclk[ctx].dirty = true;
			}
			slave->memclk[ctx].rate = rate;
			fabric->clk_dirty = true;
		}
	}
skip_set_clks:
	return status;
}

void msm_bus_fabric_update_bw(struct msm_bus_fabric_device *fabdev,
	struct msm_bus_inode_info *hop, struct msm_bus_inode_info *info,
	int64_t add_bw, int *master_tiers, int ctx)
{
	struct msm_bus_fabric *fabric = to_msm_bus_fabric(fabdev);
	void *sel_cdata;
	long rounded_rate, cur_rate;

	sel_cdata = fabric->cdata[ctx];

	/* If it's an ahb fabric, don't calculate arb values */
	if (fabric->ahb) {
		MSM_BUS_DBG("AHB fabric, skipping bw calculation\n");
		return;
	}
	if (!add_bw) {
		MSM_BUS_DBG("No bandwidth delta. Skipping commit\n");
		return;
	}

	/* Enable clocks before accessing QoS registers */
	if (fabric->info.nodeclk[DUAL_CTX].clk) {
		if (fabric->info.nodeclk[DUAL_CTX].rate == 0) {
			cur_rate = clk_get_rate(
					fabric->info.nodeclk[DUAL_CTX].clk);
			rounded_rate = clk_round_rate(
					fabric->info.nodeclk[DUAL_CTX].clk,
					cur_rate ? cur_rate : 1);
		if (clk_set_rate(fabric->info.nodeclk[DUAL_CTX].clk,
				rounded_rate))
			MSM_BUS_ERR("Error: clk: en: Node: %d rate: %ld",
				fabric->fabdev.id, rounded_rate);

		clk_prepare_enable(fabric->info.nodeclk[DUAL_CTX].clk);
		}
	}

	if (info->iface_clk.clk)
		clk_prepare_enable(info->iface_clk.clk);

	if (hop->iface_clk.clk)
		clk_prepare_enable(hop->iface_clk.clk);

	fabdev->hw_algo.update_bw(hop, info, fabric->pdata, sel_cdata,
		master_tiers, add_bw);

	/* Disable clocks after accessing QoS registers */
	if (fabric->info.nodeclk[DUAL_CTX].clk &&
			fabric->info.nodeclk[DUAL_CTX].rate == 0)
		clk_disable_unprepare(fabric->info.nodeclk[DUAL_CTX].clk);

	if (info->iface_clk.clk) {
		MSM_BUS_DBG("Commented: Will disable clock for info: %d\n",
			info->node_info->priv_id);
		clk_disable_unprepare(info->iface_clk.clk);
	}

	if (hop->iface_clk.clk) {
		MSM_BUS_DBG("Commented Will disable clock for hop: %d\n",
			hop->node_info->priv_id);
		clk_disable_unprepare(hop->iface_clk.clk);
	}

	fabric->arb_dirty = true;
}

static int msm_bus_fabric_clk_set(int enable, struct msm_bus_inode_info *info)
{
	int i, status = 0;
	long rounded_rate;

	for (i = 0; i < NUM_CTX; i++) {
		if (info->nodeclk[i].dirty) {
			if (info->nodeclk[i].rate != 0) {
				rounded_rate = clk_round_rate(info->
					nodeclk[i].clk, info->nodeclk[i].rate);
				status = clk_set_rate(info->nodeclk[i].clk,
					rounded_rate);
				MSM_BUS_DBG("AXI: node: %d set_rate: %ld\n",
					info->node_info->id, rounded_rate);
			}

			if (enable && !(info->nodeclk[i].enable)) {
				clk_prepare_enable(info->nodeclk[i].clk);
				info->nodeclk[i].dirty = false;
				info->nodeclk[i].enable = true;
			} else if ((info->nodeclk[i].rate == 0) && (!enable)
				&& (info->nodeclk[i].enable)) {
				clk_disable_unprepare(info->nodeclk[i].clk);
				info->nodeclk[i].dirty = false;
				info->nodeclk[i].enable = false;
			}
		}

		if (info->memclk[i].dirty) {
			if (info->nodeclk[i].rate != 0) {
				rounded_rate = clk_round_rate(info->
					memclk[i].clk, info->memclk[i].rate);
				status = clk_set_rate(info->memclk[i].clk,
					rounded_rate);
				MSM_BUS_DBG("AXI: node: %d set_rate: %ld\n",
					info->node_info->id, rounded_rate);
			}

			if (enable && !(info->memclk[i].enable)) {
				clk_prepare_enable(info->memclk[i].clk);
				info->memclk[i].dirty = false;
				info->memclk[i].enable = true;
			} else if (info->memclk[i].rate == 0 && (!enable) &&
				(info->memclk[i].enable)) {
				clk_disable_unprepare(info->memclk[i].clk);
				info->memclk[i].dirty = false;
				info->memclk[i].enable = false;
			}
		}
	}

	return status;
}

/**
 * msm_bus_fabric_clk_commit() - Call clock enable and update clock
 * values.
*/
static int msm_bus_fabric_clk_commit(int enable, struct msm_bus_fabric *fabric)
{
	unsigned int i, nfound = 0, status = 0;
	struct msm_bus_inode_info *info[fabric->pdata->nslaves];

	if (fabric->clk_dirty == true)
		status = msm_bus_fabric_clk_set(enable, &fabric->info);

	if (status)
		MSM_BUS_WARN("Error setting clocks on fabric: %d\n",
			fabric->fabdev.id);

	nfound = radix_tree_gang_lookup_tag(&fabric->fab_tree, (void **)&info,
		fabric->fabdev.id, fabric->pdata->nslaves, CLK_NODE);
	if (nfound == 0) {
		MSM_BUS_DBG("No clock nodes found for fabric: %d\n",
			fabric->fabdev.id);
		goto out;
	}

	for (i = 0; i < nfound; i++) {
		status = msm_bus_fabric_clk_set(enable, info[i]);
		if (status)
			MSM_BUS_WARN("Error setting clocks for node: %d\n",
				info[i]->node_info->id);
	}

out:
	return status;
}

static void msm_bus_fabric_config_master(
	struct msm_bus_fabric_device *fabdev,
	struct msm_bus_inode_info *info, uint64_t req_clk, uint64_t req_bw)
{
	struct msm_bus_fabric *fabric = to_msm_bus_fabric(fabdev);
	long rounded_rate, cur_rate;

	if (fabdev->hw_algo.config_master == NULL)
		return;

	/* Enable clocks before accessing QoS registers */
	if (fabric->info.nodeclk[DUAL_CTX].clk) {
		if (fabric->info.nodeclk[DUAL_CTX].rate == 0) {
			cur_rate = clk_get_rate(
					fabric->info.nodeclk[DUAL_CTX].clk);
			rounded_rate = clk_round_rate(
					fabric->info.nodeclk[DUAL_CTX].clk,
					cur_rate ? cur_rate : 1);
		if (clk_set_rate(fabric->info.nodeclk[DUAL_CTX].clk,
				rounded_rate))
			MSM_BUS_ERR("Error: clk: en: Node: %d rate: %ld",
				fabric->fabdev.id, rounded_rate);

		clk_prepare_enable(fabric->info.nodeclk[DUAL_CTX].clk);
		}
	}

	if (info->iface_clk.clk)
		clk_prepare_enable(info->iface_clk.clk);

	fabdev->hw_algo.config_master(fabric->pdata, info, req_clk, req_bw);

	/* Disable clocks after accessing QoS registers */
	if (fabric->info.nodeclk[DUAL_CTX].clk &&
			fabric->info.nodeclk[DUAL_CTX].rate == 0)
		clk_disable_unprepare(fabric->info.nodeclk[DUAL_CTX].clk);

	if (info->iface_clk.clk) {
		MSM_BUS_DBG("Commented: Will disable clock for info: %d\n",
			info->node_info->priv_id);
		clk_disable_unprepare(info->iface_clk.clk);
	}
}

/**
 * msm_bus_fabric_hw_commit() - Commit the arbitration data to Hardware.
 * @fabric: Fabric for which the data should be committed
 * */
static int msm_bus_fabric_hw_commit(struct msm_bus_fabric_device *fabdev)
{
	int status = 0;
	struct msm_bus_fabric *fabric = to_msm_bus_fabric(fabdev);

	/*
	 * For a non-zero bandwidth request, clocks should be enabled before
	 * sending the arbitration data to RPM, but should be disabled only
	 * after commiting the data.
	 */
	status = msm_bus_fabric_clk_commit(ENABLE, fabric);
	if (status)
		MSM_BUS_DBG("Error setting clocks on fabric: %d\n",
			fabric->fabdev.id);

	if (!fabric->arb_dirty) {
		MSM_BUS_DBG("Not committing as fabric not arb_dirty\n");
		goto skip_arb;
	}

	status = fabdev->hw_algo.commit(fabric->pdata, fabric->hw_data,
		(void **)fabric->cdata);
	if (status)
		MSM_BUS_DBG("Error committing arb data for fabric: %d\n",
			fabric->fabdev.id);

	fabric->arb_dirty = false;
skip_arb:
	/*
	 * If the bandwidth request is 0 for a fabric, the clocks
	 * should be disabled after arbitration data is committed.
	 */
	status = msm_bus_fabric_clk_commit(DISABLE, fabric);
	if (status)
		MSM_BUS_WARN("Error disabling clocks on fabric: %d\n",
			fabric->fabdev.id);
	fabric->clk_dirty = false;
	return status;
}

/**
 * msm_bus_fabric_port_halt() - Used to halt a master port
 * @fabric: Fabric on which the current master node is present
 * @portid: Port id of the master
 */
int msm_bus_fabric_port_halt(struct msm_bus_fabric_device *fabdev, int iid)
{
	struct msm_bus_inode_info *info = NULL;
	uint8_t mport;
	uint32_t haltid = 0;
	struct msm_bus_fabric *fabric = to_msm_bus_fabric(fabdev);

	info = fabdev->algo->find_node(fabdev, iid);
	if (!info) {
		MSM_BUS_ERR("Error: Info not found for id: %u", iid);
		return -EINVAL;
	}

	haltid = fabric->pdata->haltid;
	mport = info->node_info->masterp[0];

	return fabdev->hw_algo.port_halt(haltid, mport);
}

/**
 * msm_bus_fabric_port_unhalt() - Used to unhalt a master port
 * @fabric: Fabric on which the current master node is present
 * @portid: Port id of the master
 */
int msm_bus_fabric_port_unhalt(struct msm_bus_fabric_device *fabdev, int iid)
{
	struct msm_bus_inode_info *info = NULL;
	uint8_t mport;
	uint32_t haltid = 0;
	struct msm_bus_fabric *fabric = to_msm_bus_fabric(fabdev);

	info = fabdev->algo->find_node(fabdev, iid);
	if (!info) {
		MSM_BUS_ERR("Error: Info not found for id: %u", iid);
		return -EINVAL;
	}

	haltid = fabric->pdata->haltid;
	mport = info->node_info->masterp[0];
	return fabdev->hw_algo.port_unhalt(haltid, mport);
}

/**
 * msm_bus_fabric_find_gw_node() - This function finds the gateway node
 * attached on a given fabric
 * @id:       ID of the gateway node
 * @fabric:   Fabric to find the gateway node on
 * Function returns: Pointer to the gateway node
 */
static struct msm_bus_inode_info *msm_bus_fabric_find_gw_node(struct
	msm_bus_fabric_device * fabdev, int id)
{
	struct msm_bus_inode_info *info = NULL;
	struct msm_bus_fabnodeinfo *fab;
	struct msm_bus_fabric *fabric;
	if (!fabdev) {
		MSM_BUS_ERR("No fabric device found!\n");
		return NULL;
	}

	fabric = to_msm_bus_fabric(fabdev);
	if (!fabric || IS_ERR(fabric)) {
		MSM_BUS_ERR("No fabric type found!\n");
		return NULL;
	}
	list_for_each_entry(fab, &fabric->gateways, list) {
		if (fab->info->node_info->priv_id == id) {
			info = fab->info;
			break;
		}
	}

	return info;
}

static struct msm_bus_inode_info *msm_bus_fabric_find_node(struct
	msm_bus_fabric_device * fabdev, int id)
{
	struct msm_bus_inode_info *info = NULL;
	struct msm_bus_fabric *fabric = to_msm_bus_fabric(fabdev);
	info = radix_tree_lookup(&fabric->fab_tree, id);
	if (!info)
		MSM_BUS_ERR("Null info found for id %d\n", id);
	return info;
}

static struct list_head *msm_bus_fabric_get_gw_list(struct msm_bus_fabric_device
	*fabdev)
{
	struct msm_bus_fabric *fabric = to_msm_bus_fabric(fabdev);
	if (!fabric || IS_ERR(fabric)) {
		MSM_BUS_ERR("No fabric found from fabdev\n");
		return NULL;
	}
	return &fabric->gateways;

}
static struct msm_bus_fab_algorithm msm_bus_algo = {
	.update_clks = msm_bus_fabric_update_clks,
	.update_bw = msm_bus_fabric_update_bw,
	.port_halt = msm_bus_fabric_port_halt,
	.port_unhalt = msm_bus_fabric_port_unhalt,
	.commit = msm_bus_fabric_hw_commit,
	.find_node = msm_bus_fabric_find_node,
	.find_gw_node = msm_bus_fabric_find_gw_node,
	.get_gw_list = msm_bus_fabric_get_gw_list,
	.config_master = msm_bus_fabric_config_master,
};

static int msm_bus_fabric_hw_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo)
{
	int ret = 0;

	switch (pdata->hw_sel) {
	case MSM_BUS_NOC:
		msm_bus_noc_hw_init(pdata, hw_algo);
		break;
	case MSM_BUS_BIMC:
		msm_bus_bimc_hw_init(pdata, hw_algo);
		break;
	default:
		ret = msm_bus_rpm_hw_init(pdata, hw_algo);
		if (ret) {
			MSM_BUS_ERR("RPM initialization failed\n");
			ret = -EINVAL;
		}
		break;
	}
	return ret;
}

static int __devinit msm_bus_fabric_probe(struct platform_device *pdev)
{
	int ctx, ret = 0;
	struct msm_bus_fabric *fabric;
	struct msm_bus_fabric_registration *pdata;

	fabric = kzalloc(sizeof(struct msm_bus_fabric), GFP_KERNEL);
	if (!fabric) {
		MSM_BUS_ERR("Fabric alloc failed\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&fabric->gateways);
	INIT_RADIX_TREE(&fabric->fab_tree, GFP_ATOMIC);
	fabric->num_nodes = 0;
	fabric->fabdev.visited = false;

	fabric->info.node_info = kzalloc(sizeof(struct msm_bus_node_info),
				GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(fabric->info.node_info)) {
		MSM_BUS_ERR("Fabric node info alloc failed\n");
		kfree(fabric);
		return -ENOMEM;
	}

	fabric->info.num_pnodes = -1;
	fabric->info.link_info.clk[DUAL_CTX] = 0;
	fabric->info.link_info.bw[DUAL_CTX] = 0;
	fabric->info.link_info.clk[ACTIVE_CTX] = 0;
	fabric->info.link_info.bw[ACTIVE_CTX] = 0;

	/* If possible, get pdata from device-tree */
	if (pdev->dev.of_node) {
		pdata = msm_bus_of_get_fab_data(pdev);
		if (IS_ERR(pdata) || ZERO_OR_NULL_PTR(pdata)) {
			pr_err("Null platform data\n");
			kfree(fabric->info.node_info);
			kfree(fabric);
			return PTR_ERR(pdata);
		}
		msm_bus_board_init(pdata);
		fabric->fabdev.id = pdata->id;
	} else {
		pdata = (struct msm_bus_fabric_registration *)pdev->
			dev.platform_data;
		fabric->fabdev.id = pdev->id;
	}

	fabric->fabdev.name = pdata->name;
	fabric->fabdev.algo = &msm_bus_algo;
	fabric->info.node_info->priv_id = fabric->fabdev.id;
	fabric->info.node_info->id = fabric->fabdev.id;
	ret = msm_bus_fabric_hw_init(pdata, &fabric->fabdev.hw_algo);
	if (ret) {
		MSM_BUS_ERR("Error initializing hardware for fabric: %d\n",
			fabric->fabdev.id);
		goto err;
	}

	fabric->ahb = pdata->ahb;
	fabric->pdata = pdata;
	fabric->pdata->board_algo->assign_iids(fabric->pdata,
		fabric->fabdev.id);
	fabric->fabdev.board_algo = fabric->pdata->board_algo;

	/*
	 * clk and bw for fabric->info will contain the max bw and clk
	 * it will allow. This info will come from the boards file.
	 */
	ret = msm_bus_fabric_device_register(&fabric->fabdev);
	if (ret) {
		MSM_BUS_ERR("Error registering fabric %d ret %d\n",
			fabric->fabdev.id, ret);
		goto err;
	}

	for (ctx = 0; ctx < NUM_CTX; ctx++) {
		if (pdata->fabclk[ctx]) {
			fabric->info.nodeclk[ctx].clk = clk_get(
				&fabric->fabdev.dev, pdata->fabclk[ctx]);
			if (IS_ERR(fabric->info.nodeclk[ctx].clk)) {
				MSM_BUS_ERR("Couldn't get clock %s\n",
					pdata->fabclk[ctx]);
				ret = -EINVAL;
				goto err;
			}
			fabric->info.nodeclk[ctx].enable = false;
			fabric->info.nodeclk[ctx].dirty = false;
		}
	}

	/* Find num. of slaves, masters, populate gateways, radix tree */
	ret = register_fabric_info(pdev, fabric);
	if (ret) {
		MSM_BUS_ERR("Could not register fabric %d info, ret: %d\n",
			fabric->fabdev.id, ret);
		goto err;
	}
	if (!fabric->ahb) {
		/* Allocate memory for commit data */
		for (ctx = 0; ctx < NUM_CTX; ctx++) {
			ret = fabric->fabdev.hw_algo.allocate_commit_data(
				fabric->pdata, &fabric->cdata[ctx], ctx);
			if (ret) {
				MSM_BUS_ERR("Failed to alloc commit data for "
					"fab: %d, ret = %d\n",
					fabric->fabdev.id, ret);
				goto err;
			}
		}
	}

	if (msmbus_coresight_init(pdev))
		pr_warn("Coresight support absent for bus: %d\n", pdata->id);

	return ret;
err:
	kfree(fabric->info.node_info);
	kfree(fabric);
	return ret;
}

static int msm_bus_fabric_remove(struct platform_device *pdev)
{
	struct msm_bus_fabric_device *fabdev = NULL;
	struct msm_bus_fabric *fabric;
	int i;
	int ret = 0;

	fabdev = platform_get_drvdata(pdev);
	msmbus_coresight_remove(pdev);
	msm_bus_fabric_device_unregister(fabdev);
	fabric = to_msm_bus_fabric(fabdev);
	msm_bus_dbg_commit_data(fabric->fabdev.name, NULL, 0, 0, 0,
		MSM_BUS_DBG_UNREGISTER);
	for (i = 0; i < fabric->pdata->nmasters; i++)
		radix_tree_delete(&fabric->fab_tree, fabric->fabdev.id + i);
	for (i = (fabric->fabdev.id + SLAVE_ID_KEY); i <
		fabric->pdata->nslaves; i++)
		radix_tree_delete(&fabric->fab_tree, i);
	if (!fabric->ahb) {
		fabdev->hw_algo.free_commit_data(fabric->cdata[DUAL_CTX]);
		fabdev->hw_algo.free_commit_data(fabric->cdata[ACTIVE_CTX]);
	}

	kfree(fabric->info.node_info);
	kfree(fabric->hw_data);
	kfree(fabric);
	return ret;
}

static struct of_device_id fabric_match[] = {
	{.compatible = "msm-bus-fabric"},
	{}
};

static struct platform_driver msm_bus_fabric_driver = {
	.probe = msm_bus_fabric_probe,
	.remove = msm_bus_fabric_remove,
	.driver = {
		.name = "msm_bus_fabric",
		.owner = THIS_MODULE,
		.of_match_table = fabric_match,
	},
};

int __init msm_bus_fabric_init_driver(void)
{
	static bool initialized;

	if (initialized)
		return 0;
	else
		initialized = true;

	MSM_BUS_ERR("msm_bus_fabric_init_driver\n");
	return platform_driver_register(&msm_bus_fabric_driver);
}
EXPORT_SYMBOL(msm_bus_fabric_init_driver);
subsys_initcall(msm_bus_fabric_init_driver);
