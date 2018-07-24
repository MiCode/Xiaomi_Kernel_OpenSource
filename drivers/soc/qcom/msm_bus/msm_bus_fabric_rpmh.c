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
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/cmd-db.h>
#include <soc/qcom/rpmh.h>
#include <soc/qcom/tcs.h>
#include <trace/events/trace_msm_bus.h>
#include <dt-bindings/msm/msm-bus-ids.h>
#include "msm_bus_core.h"
#include "msm_bus_rpmh.h"
#include "msm_bus_noc.h"
#include "msm_bus_bimc.h"

#define BCM_TCS_CMD_COMMIT_SHFT		30
#define BCM_TCS_CMD_COMMIT_MASK		0x40000000
#define BCM_TCS_CMD_VALID_SHFT		29
#define BCM_TCS_CMD_VALID_MASK		0x20000000
#define BCM_TCS_CMD_VOTE_X_SHFT		14
#define BCM_TCS_CMD_VOTE_MASK		0x3FFF
#define BCM_TCS_CMD_VOTE_Y_SHFT		0
#define BCM_TCS_CMD_VOTE_Y_MASK		0xFFFC000

#define BCM_TCS_CMD(commit, valid, vote_x, vote_y) \
	(((commit & 0x1) << BCM_TCS_CMD_COMMIT_SHFT) |\
	((valid & 0x1) << BCM_TCS_CMD_VALID_SHFT) |\
	((vote_x & BCM_TCS_CMD_VOTE_MASK) << BCM_TCS_CMD_VOTE_X_SHFT) |\
	((vote_y & BCM_TCS_CMD_VOTE_MASK) << BCM_TCS_CMD_VOTE_Y_SHFT))

static int msm_bus_dev_init_qos(struct device *dev, void *data);
static int msm_bus_dev_sbm_config(struct device *dev, bool enable);

static struct list_head bcm_query_list_inorder[VCD_MAX_CNT];
static struct msm_bus_node_device_type *cur_rsc;
static bool init_time = true;

struct bcm_db {
	uint32_t unit_size;
	uint16_t width;
	uint8_t clk_domain;
	uint8_t reserved;
};

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
			dev_dbg(dev, "Successfully got regulator for %d\n",
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

static int tcs_cmd_gen(struct msm_bus_node_device_type *cur_bcm,
				struct tcs_cmd *cmd, uint64_t vec_a,
					uint64_t vec_b, bool commit)
{
	int ret = 0;
	bool valid = true;

	if (!cmd)
		return ret;

	if (vec_a == 0 && vec_b == 0)
		valid = false;

	if (vec_a > BCM_TCS_CMD_VOTE_MASK)
		vec_a = BCM_TCS_CMD_VOTE_MASK;

	if (vec_b > BCM_TCS_CMD_VOTE_MASK)
		vec_b = BCM_TCS_CMD_VOTE_MASK;

	cmd->addr = cur_bcm->bcmdev->addr;
	cmd->data = BCM_TCS_CMD(commit, valid, vec_a, vec_b);
	cmd->complete = commit;

	return ret;
}

static int tcs_cmd_list_gen(int *n_active,
				int *n_wake,
				int *n_sleep,
				struct tcs_cmd *cmdlist_active,
				struct tcs_cmd *cmdlist_wake,
				struct tcs_cmd *cmdlist_sleep,
				struct list_head *cur_bcm_clist)
{
	struct msm_bus_node_device_type *cur_bcm = NULL;
	int i = 0;
	int k = 0;
	int idx = 0;
	int last_tcs = -1;
	bool commit = false;

	if (!cmdlist_active)
		goto exit_tcs_cmd_list_gen;

	for (i = 0; i < VCD_MAX_CNT; i++) {
		last_tcs = -1;
		if (list_empty(&cur_bcm_clist[i]))
			continue;
		list_for_each_entry(cur_bcm, &cur_bcm_clist[i], link) {
			if (cur_bcm->updated ||
				(cur_bcm->node_vec[DUAL_CTX].vec_a == 0 &&
				cur_bcm->node_vec[ACTIVE_CTX].vec_a == 0 &&
				cur_bcm->node_vec[DUAL_CTX].vec_b == 0 &&
				cur_bcm->node_vec[ACTIVE_CTX].vec_b == 0 &&
				init_time == true)) {
				if (last_tcs != -1 &&
					list_is_last(&cur_bcm->link,
						&cur_bcm_clist[i])) {
					cmdlist_active[last_tcs].data |=
						BCM_TCS_CMD_COMMIT_MASK;
					cmdlist_active[last_tcs].complete
								= true;
				}
				continue;
			}
			n_active[idx]++;
			commit = false;
			if (list_is_last(&cur_bcm->link,
						&cur_bcm_clist[i])) {
				commit = true;
				idx++;
			}
			tcs_cmd_gen(cur_bcm, &cmdlist_active[k],
				cur_bcm->node_vec[ACTIVE_CTX].vec_a,
				cur_bcm->node_vec[ACTIVE_CTX].vec_b, commit);
			last_tcs = k;
			k++;
			cur_bcm->updated = true;
		}
	}

	if (!cmdlist_wake || !cmdlist_sleep)
		goto exit_tcs_cmd_list_gen;

	k = 0;
	idx = 0;
	for (i = 0; i < VCD_MAX_CNT; i++) {
		last_tcs = -1;
		if (list_empty(&cur_bcm_clist[i]))
			continue;
		list_for_each_entry(cur_bcm, &cur_bcm_clist[i], link) {
			commit = false;
			if ((cur_bcm->node_vec[DUAL_CTX].vec_a ==
				cur_bcm->node_vec[ACTIVE_CTX].vec_a) &&
				(cur_bcm->node_vec[DUAL_CTX].vec_b ==
				cur_bcm->node_vec[ACTIVE_CTX].vec_b)) {
				if (last_tcs != -1 &&
					list_is_last(&cur_bcm->link,
					&cur_bcm_clist[i])) {
					cmdlist_wake[last_tcs].data |=
						BCM_TCS_CMD_COMMIT_MASK;
					cmdlist_sleep[last_tcs].data |=
						BCM_TCS_CMD_COMMIT_MASK;
					cmdlist_wake[last_tcs].complete = true;
					cmdlist_sleep[last_tcs].complete = true;
					idx++;
				}
				continue;
			}
			last_tcs = k;
			n_sleep[idx]++;
			n_wake[idx]++;
			if (list_is_last(&cur_bcm->link,
						&cur_bcm_clist[i])) {
				commit = true;
				idx++;
			}

			tcs_cmd_gen(cur_bcm, &cmdlist_wake[k],
				cur_bcm->node_vec[ACTIVE_CTX].vec_a,
				cur_bcm->node_vec[ACTIVE_CTX].vec_b, commit);

			if (cur_rsc->rscdev->req_state == RPMH_AWAKE_STATE)
				commit = false;

			tcs_cmd_gen(cur_bcm, &cmdlist_sleep[k],
				cur_bcm->node_vec[DUAL_CTX].vec_a,
				cur_bcm->node_vec[DUAL_CTX].vec_b, commit);
			k++;
		}
	}

exit_tcs_cmd_list_gen:
	return k;
}

static int tcs_cmd_query_list_gen(struct tcs_cmd *cmdlist_active)
{
	struct msm_bus_node_device_type *cur_bcm = NULL;
	struct list_head *bcm_list_inorder = NULL;
	int i = 0;
	int k = 0;
	bool commit = false;
	int ret = 0;

	if (!cmdlist_active)
		goto exit_tcs_cmd_list_gen;

	bcm_list_inorder = bcm_query_list_inorder;

	for (i = 0; i < VCD_MAX_CNT; i++) {
		if (list_empty(&bcm_list_inorder[i]))
			continue;
		list_for_each_entry(cur_bcm, &bcm_list_inorder[i], query_link) {
			commit = false;
			if (list_is_last(&cur_bcm->query_link,
						&bcm_list_inorder[i])) {
				commit = true;
			}
			tcs_cmd_gen(cur_bcm, &cmdlist_active[k],
				cur_bcm->node_vec[ACTIVE_CTX].query_vec_a,
				cur_bcm->node_vec[ACTIVE_CTX].query_vec_b,
								commit);
			k++;
		}
	}

exit_tcs_cmd_list_gen:
	return ret;
}

static int bcm_clist_add(struct msm_bus_node_device_type *cur_dev)
{
	int ret = 0;
	int cur_vcd = 0;
	int i = 0;
	struct msm_bus_node_device_type *cur_bcm = NULL;

	if (!cur_dev->node_info->num_bcm_devs)
		goto exit_bcm_clist_add;

	for (i = 0; i < cur_dev->node_info->num_bcm_devs; i++) {
		cur_bcm = to_msm_bus_node(cur_dev->node_info->bcm_devs[i]);
		cur_vcd = cur_bcm->bcmdev->clk_domain;

		if (!cur_bcm->node_info->num_rsc_devs)
			goto exit_bcm_clist_add;

		if (!cur_rsc)
			cur_rsc = to_msm_bus_node(
					cur_bcm->node_info->rsc_devs[0]);

		if (!cur_bcm->dirty) {
			list_add_tail(&cur_bcm->link,
					&cur_rsc->rscdev->bcm_clist[cur_vcd]);
			cur_bcm->dirty = true;
		}
		cur_bcm->updated = false;
	}

exit_bcm_clist_add:
	return ret;
}

static void tcs_cmd_n_shrink(int *n)
{
	int i = 0, j = 0, sum = 0;

	do {
		if (sum + n[i] > MAX_RPMH_PAYLOAD) {
			n[j] = sum;
			sum = 0;
			j++;
		}
		sum += n[i];
	} while (n[i++]);

	n[j] = sum;
	n[j+1] = 0;
}

static int bcm_query_list_add(struct msm_bus_node_device_type *cur_dev)
{
	int ret = 0;
	int cur_vcd = 0;
	int i = 0;
	struct msm_bus_node_device_type *cur_bcm = NULL;

	if (!cur_dev->node_info->num_bcm_devs)
		goto exit_bcm_query_list_add;

	for (i = 0; i < cur_dev->node_info->num_bcm_devs; i++) {
		cur_bcm = to_msm_bus_node(cur_dev->node_info->bcm_devs[i]);
		cur_vcd = cur_bcm->bcmdev->clk_domain;

		if (!cur_bcm->query_dirty) {
			list_add_tail(&cur_bcm->query_link,
					&bcm_query_list_inorder[cur_vcd]);
			cur_bcm->query_dirty = true;
		}
	}

exit_bcm_query_list_add:
	return ret;
}

static int bcm_clist_clean(struct msm_bus_node_device_type *cur_dev)
{
	int ret = 0;
	int i = 0;
	struct msm_bus_node_device_type *cur_bcm = NULL;

	if (!cur_dev->node_info->num_bcm_devs)
		goto exit_bcm_clist_clean;

	for (i = 0; i < cur_dev->node_info->num_bcm_devs; i++) {
		cur_bcm = to_msm_bus_node(cur_dev->node_info->bcm_devs[i]);

		if (cur_bcm->node_vec[DUAL_CTX].vec_a == 0 &&
			cur_bcm->node_vec[ACTIVE_CTX].vec_a == 0 &&
			cur_bcm->node_vec[DUAL_CTX].vec_b == 0 &&
			cur_bcm->node_vec[ACTIVE_CTX].vec_b == 0 &&
			init_time == false) {
			cur_bcm->dirty = false;
			list_del_init(&cur_bcm->link);
		}
	}

exit_bcm_clist_clean:
	return ret;
}

static int bcm_query_list_clean(struct msm_bus_node_device_type *cur_dev)
{
	int ret = 0;
	int i = 0;
	struct msm_bus_node_device_type *cur_bcm = NULL;

	if (!cur_dev->node_info->num_bcm_devs)
		goto exit_bcm_clist_add;

	for (i = 0; i < cur_dev->node_info->num_bcm_devs; i++) {
		cur_bcm = to_msm_bus_node(cur_dev->node_info->bcm_devs[i]);

		cur_bcm->query_dirty = false;
		list_del_init(&cur_bcm->query_link);
	}

exit_bcm_clist_add:
	return ret;
}

int msm_bus_commit_data(struct list_head *clist)
{
	int ret = 0;
	int bcm_cnt;
	struct msm_bus_node_device_type *node = NULL;
	struct msm_bus_node_device_type *node_tmp = NULL;
	struct msm_bus_node_device_type *cur_bcm = NULL;
	struct tcs_cmd *cmdlist_active = NULL;
	struct tcs_cmd *cmdlist_wake = NULL;
	struct tcs_cmd *cmdlist_sleep = NULL;
	struct rpmh_client *cur_mbox = NULL;
	struct list_head *cur_bcm_clist = NULL;
	int n_active[VCD_MAX_CNT];
	int n_wake[VCD_MAX_CNT];
	int n_sleep[VCD_MAX_CNT];
	int cnt_vcd = 0;
	int cnt_active = 0;
	int cnt_wake = 0;
	int cnt_sleep = 0;
	int i = 0;

	if (!clist)
		return ret;

	list_for_each_entry_safe(node, node_tmp, clist, link) {
		bcm_clist_add(node);
		msm_bus_dev_sbm_config(&node->dev, false);
	}

	if (!cur_rsc) {
		MSM_BUS_ERR("%s: Error for cur_rsc is NULL.\n", __func__);
		return ret;
	}

	cur_mbox = cur_rsc->rscdev->mbox;
	cur_bcm_clist = cur_rsc->rscdev->bcm_clist;
	cmdlist_active = cur_rsc->rscdev->cmdlist_active;
	cmdlist_wake = cur_rsc->rscdev->cmdlist_wake;
	cmdlist_sleep = cur_rsc->rscdev->cmdlist_sleep;

	for (i = 0; i < VCD_MAX_CNT; i++) {
		n_active[i] = 0;
		n_wake[i] = 0;
		n_sleep[i] = 0;

		if (list_empty(&cur_bcm_clist[i]))
			continue;
		list_for_each_entry(cur_bcm, &cur_bcm_clist[i], link) {
			if ((cur_bcm->node_vec[DUAL_CTX].vec_a !=
				cur_bcm->node_vec[ACTIVE_CTX].vec_a) ||
				(cur_bcm->node_vec[DUAL_CTX].vec_b !=
				cur_bcm->node_vec[ACTIVE_CTX].vec_b)) {
				cnt_sleep++;
				cnt_wake++;
			}
			if (cur_bcm->updated ||
				(cur_bcm->node_vec[DUAL_CTX].vec_a == 0 &&
				cur_bcm->node_vec[ACTIVE_CTX].vec_a == 0 &&
				cur_bcm->node_vec[DUAL_CTX].vec_b == 0 &&
				cur_bcm->node_vec[ACTIVE_CTX].vec_b == 0 &&
				init_time == true))
				continue;
			cnt_active++;
		}
		cnt_vcd++;
	}

	if (!cnt_active)
		goto exit_msm_bus_commit_data;

	bcm_cnt = tcs_cmd_list_gen(n_active, n_wake, n_sleep, cmdlist_active,
				cmdlist_wake, cmdlist_sleep, cur_bcm_clist);

	tcs_cmd_n_shrink(n_active);
	tcs_cmd_n_shrink(n_wake);
	tcs_cmd_n_shrink(n_sleep);

	ret = rpmh_invalidate(cur_mbox);
	if (ret)
		MSM_BUS_ERR("%s: Error invalidating mbox: %d\n",
						__func__, ret);

	if (cur_rsc->node_info->id == MSM_BUS_RSC_DISP) {
		ret = rpmh_write_batch(cur_mbox, cur_rsc->rscdev->req_state,
						cmdlist_active, n_active);
		/*
		 * Ignore -EBUSY from rpmh_write if it's an AMC
		 * request to Display RSC which are invalid when
		 * the display RSC is in solver mode and the bus driver
		 * does not know the current state of the display RSC.
		 */
		if (ret && ret != -EBUSY)
			MSM_BUS_ERR("%s: error sending active/awake sets: %d\n",
						__func__, ret);
	} else {
		ret = rpmh_write_batch(cur_mbox, cur_rsc->rscdev->req_state,
						cmdlist_active, n_active);
		if (ret)
			MSM_BUS_ERR("%s: error sending active/awake sets: %d\n",
						__func__, ret);
	}
	if (cnt_wake) {
		ret = rpmh_write_batch(cur_mbox, RPMH_WAKE_ONLY_STATE,
							cmdlist_wake, n_wake);
		if (ret)
			MSM_BUS_ERR("%s: error sending wake sets: %d\n",
							__func__, ret);
	}
	if (cnt_sleep) {
		ret = rpmh_write_batch(cur_mbox, RPMH_SLEEP_STATE,
							cmdlist_sleep, n_sleep);
		if (ret)
			MSM_BUS_ERR("%s: error sending sleep sets: %d\n",
							__func__, ret);
	}

	list_for_each_entry_safe(node, node_tmp, clist, link) {
		if (unlikely(node->node_info->defer_qos))
			msm_bus_dev_init_qos(&node->dev, NULL);
		msm_bus_dev_sbm_config(&node->dev, true);
	}

exit_msm_bus_commit_data:
	list_for_each_entry_safe(node, node_tmp, clist, link) {
		bcm_clist_clean(node);
		node->dirty = false;
		list_del_init(&node->link);
	}
	cur_rsc = NULL;
	return ret;
}

int msm_bus_query_gen(struct list_head *query_list,
				struct msm_bus_tcs_usecase *tcs_usecase)
{
	int ret = 0;
	struct msm_bus_node_device_type *node = NULL;
	struct msm_bus_node_device_type *node_tmp = NULL;
	struct msm_bus_node_device_type *cur_bcm = NULL;
	int *n_active = NULL;
	int cnt_vcd = 0;
	int cnt_active = 0;
	int i = 0;

	list_for_each_entry_safe(node, node_tmp, query_list, query_link)
		bcm_query_list_add(node);

	for (i = 0; i < VCD_MAX_CNT; i++) {
		if (list_empty(&bcm_query_list_inorder[i]))
			continue;
		list_for_each_entry(cur_bcm, &bcm_query_list_inorder[i],
							query_link) {
			cnt_active++;
		}
		cnt_vcd++;
	}

	tcs_usecase->num_cmds = cnt_active;
	ret = tcs_cmd_query_list_gen(tcs_usecase->cmds);

	list_for_each_entry_safe(node, node_tmp, query_list, query_link) {
		bcm_query_list_clean(node);
		node->query_dirty = false;
		list_del_init(&node->query_link);
	}

	kfree(n_active);
	return ret;
}

static void bcm_commit_single_req(struct msm_bus_node_device_type *cur_bcm,
					uint64_t vec_a, uint64_t vec_b)
{
	struct msm_bus_node_device_type *cur_rsc = NULL;
	struct rpmh_client *cur_mbox = NULL;
	struct tcs_cmd *cmd_active = NULL;

	if (!cur_bcm->node_info->num_rsc_devs)
		return;

	cmd_active = kzalloc(sizeof(struct tcs_cmd), GFP_KERNEL);

	if (!cmd_active)
		return;

	cur_rsc = to_msm_bus_node(cur_bcm->node_info->rsc_devs[0]);
	cur_mbox = cur_rsc->rscdev->mbox;

	tcs_cmd_gen(cur_bcm, cmd_active, vec_a, vec_b, true);
	rpmh_write_single(cur_mbox, RPMH_ACTIVE_ONLY_STATE,
					cmd_active->addr, cmd_active->data);

	kfree(cmd_active);
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
	int i;
	int ret = 0;

	if (!node) {
		ret = -ENXIO;
		goto exit_disable_node_qos_clk;
	}

	for (i = 0; i < node->num_node_qos_clks; i++)
		ret = disable_nodeclk(&node->node_qos_clks[i]);

exit_disable_node_qos_clk:
	return ret;
}

static int msm_bus_enable_node_qos_clk(struct msm_bus_node_device_type *node)
{
	int i;
	int ret = 0;
	long rounded_rate;

	for (i = 0; i < node->num_node_qos_clks; i++) {
		if (!node->node_qos_clks[i].enable_only_clk) {
			rounded_rate =
				clk_round_rate(
					node->node_qos_clks[i].clk, 1);
			ret = setrate_nodeclk(&node->node_qos_clks[i],
								rounded_rate);
			if (ret)
				MSM_BUS_DBG("%s: Failed set rate clk,node %d\n",
					__func__, node->node_info->id);
		}
		ret = enable_nodeclk(&node->node_qos_clks[i],
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

static int msm_bus_vote_qos_bcms(struct msm_bus_node_device_type *node)
{
	struct msm_bus_node_device_type *cur_dev = NULL;
	struct msm_bus_node_device_type *cur_bcm = NULL;
	int i;
	struct device *dev = NULL;

	if (!node || (!to_msm_bus_node(node->node_info->bus_device)))
		return -ENXIO;

	cur_dev = node;

	for (i = 0; i < cur_dev->num_qos_bcms; i++) {
		dev = bus_find_device(&msm_bus_type, NULL,
				(void *) &cur_dev->qos_bcms[i].qos_bcm_id,
					msm_bus_device_match_adhoc);

		if (!dev) {
			MSM_BUS_ERR("Can't find dev node for %d",
					cur_dev->qos_bcms[i].qos_bcm_id);
			return -ENODEV;
		}

		cur_bcm = to_msm_bus_node(dev);
		if (cur_bcm->node_vec[ACTIVE_CTX].vec_a != 0 ||
			cur_bcm->node_vec[ACTIVE_CTX].vec_b != 0 ||
			cur_bcm->node_vec[DUAL_CTX].vec_a != 0 ||
			cur_bcm->node_vec[DUAL_CTX].vec_b != 0)
			return 0;

		bcm_commit_single_req(cur_bcm,
					cur_dev->qos_bcms[i].vec.vec_a,
					cur_dev->qos_bcms[i].vec.vec_b);
	}

	return 0;
}

static int msm_bus_rm_vote_qos_bcms(struct msm_bus_node_device_type *node)
{
	struct msm_bus_node_device_type *cur_dev = NULL;
	struct msm_bus_node_device_type *cur_bcm = NULL;
	int i;
	struct device *dev = NULL;

	if (!node || (!to_msm_bus_node(node->node_info->bus_device)))
		return -ENXIO;

	cur_dev = node;

	for (i = 0; i < cur_dev->num_qos_bcms; i++) {
		dev = bus_find_device(&msm_bus_type, NULL,
				(void *) &cur_dev->qos_bcms[i].qos_bcm_id,
					msm_bus_device_match_adhoc);

		if (!dev) {
			MSM_BUS_ERR("Can't find dev node for %d",
					cur_dev->qos_bcms[i].qos_bcm_id);
			return -ENODEV;
		}

		cur_bcm = to_msm_bus_node(dev);
		if (cur_bcm->node_vec[ACTIVE_CTX].vec_a != 0 ||
			cur_bcm->node_vec[ACTIVE_CTX].vec_b != 0 ||
			cur_bcm->node_vec[DUAL_CTX].vec_a != 0 ||
			cur_bcm->node_vec[DUAL_CTX].vec_b != 0)
			return 0;

		bcm_commit_single_req(cur_bcm, 0, 0);
	}

	return 0;
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
		MSM_BUS_ERR("%s: Unable to get node device info", __func__);
		ret = -ENXIO;
		goto exit_init_qos;
	}

	MSM_BUS_DBG("Device = %d", node_dev->node_info->id);

	if (node_dev->node_info->qos_params.defer_init_qos) {
		node_dev->node_info->qos_params.defer_init_qos = false;
		node_dev->node_info->defer_qos = true;
		goto exit_init_qos;
	}

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

			if (node_dev->ap_owned) {
				if (bus_node_info->fabdev->bypass_qos_prg)
					goto exit_init_qos;

				ret = msm_bus_vote_qos_bcms(node_dev);
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
				ret = msm_bus_rm_vote_qos_bcms(node_dev);
				node_dev->node_info->defer_qos = false;
			}
		} else
			MSM_BUS_ERR("%s: Skipping QOS init for %d",
				__func__, node_dev->node_info->id);
	}
exit_init_qos:
	return ret;
}

static int msm_bus_dev_sbm_config(struct device *dev, bool enable)
{
	int ret = 0, idx = 0;
	struct msm_bus_node_device_type *node_dev = NULL;
	struct msm_bus_node_device_type *fab_dev = NULL;

	node_dev = to_msm_bus_node(dev);
	if (!node_dev) {
		MSM_BUS_ERR("%s: Unable to get node device info", __func__);
		return -ENXIO;
	}

	if (!node_dev->node_info->num_disable_ports)
		return 0;

	if ((node_dev->node_bw[DUAL_CTX].sum_ab ||
		node_dev->node_bw[DUAL_CTX].max_ib ||
		!node_dev->is_connected) && !enable)
		return 0;
	else if (((!node_dev->node_bw[DUAL_CTX].sum_ab &&
		!node_dev->node_bw[DUAL_CTX].max_ib) ||
		node_dev->is_connected) && enable)
		return 0;

	if (enable) {
		for (idx = 0; idx < node_dev->num_regs; idx++) {
			if (!node_dev->node_regs[idx].reg)
				node_dev->node_regs[idx].reg =
				devm_regulator_get(dev,
				node_dev->node_regs[idx].name);

			if ((IS_ERR_OR_NULL(node_dev->node_regs[idx].reg)))
				return -ENXIO;
			ret = regulator_enable(node_dev->node_regs[idx].reg);
			if (ret) {
				MSM_BUS_ERR("%s: Failed to enable reg:%s\n",
				__func__, node_dev->node_regs[idx].name);
				return ret;
			}
		}
		node_dev->is_connected = true;
	}

	fab_dev = to_msm_bus_node(node_dev->node_info->bus_device);
	if (!fab_dev) {
		MSM_BUS_ERR("%s: Unable to get bus device info for %d",
			__func__,
			node_dev->node_info->id);
		return -ENXIO;
	}

	if (fab_dev->fabdev &&
			fab_dev->fabdev->noc_ops.sbm_config) {
		ret = fab_dev->fabdev->noc_ops.sbm_config(
			node_dev,
			fab_dev->fabdev->qos_base,
			fab_dev->fabdev->sbm_offset,
			enable);
	}

	if (!enable) {
		for (idx = 0; idx < node_dev->num_regs; idx++) {
			if (!node_dev->node_regs[idx].reg)
				node_dev->node_regs[idx].reg =
				devm_regulator_get(dev,
					node_dev->node_regs[idx].name);

			if ((IS_ERR_OR_NULL(node_dev->node_regs[idx].reg)))
				return -ENXIO;
			ret = regulator_disable(node_dev->node_regs[idx].reg);
			if (ret) {
				MSM_BUS_ERR("%s: Failed to disable reg:%s\n",
				__func__, node_dev->node_regs[idx].name);
				return ret;
			}
		}
		node_dev->is_connected = false;
	}
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
		MSM_BUS_ERR("%s: Unable to get bus device info", __func__);
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
	fabdev->sbm_offset = pdata->fabdev->sbm_offset;
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

static int msm_bus_bcm_init(struct device *dev,
			struct msm_bus_node_device_type *pdata)
{
	struct msm_bus_bcm_device_type *bcmdev;
	struct msm_bus_node_device_type *node_dev = NULL;
	struct bcm_db aux_data = {0};
	int ret = 0;
	int i = 0;

	node_dev = to_msm_bus_node(dev);
	if (!node_dev) {
		ret = -ENXIO;
		goto exit_bcm_init;
	}

	bcmdev = devm_kzalloc(dev, sizeof(struct msm_bus_bcm_device_type),
								GFP_KERNEL);
	if (!bcmdev) {
		ret = -ENOMEM;
		goto exit_bcm_init;
	}

	node_dev->bcmdev = bcmdev;
	bcmdev->name = pdata->bcmdev->name;

	if (!cmd_db_get_aux_data_len(bcmdev->name)) {
		MSM_BUS_ERR("%s: Error getting bcm info, bcm:%s",
			__func__, bcmdev->name);
		ret = -ENXIO;
		goto exit_bcm_init;
	}

	cmd_db_get_aux_data(bcmdev->name, (u8 *)&aux_data,
						sizeof(struct bcm_db));

	bcmdev->addr = cmd_db_get_addr(bcmdev->name);
	bcmdev->width = (uint32_t)aux_data.width;
	bcmdev->clk_domain = aux_data.clk_domain;
	bcmdev->unit_size = aux_data.unit_size;
	bcmdev->type = 0;
	bcmdev->num_bus_devs = 0;

	// Add way to count # of VCDs, initialize LL
	for (i = 0; i < VCD_MAX_CNT; i++)
		INIT_LIST_HEAD(&bcm_query_list_inorder[i]);

exit_bcm_init:
	return ret;
}

static int msm_bus_rsc_init(struct platform_device *pdev,
			struct device *dev,
			struct msm_bus_node_device_type *pdata)
{
	struct msm_bus_rsc_device_type *rscdev;
	struct msm_bus_node_device_type *node_dev = NULL;
	int ret = 0;
	int i = 0;

	node_dev = to_msm_bus_node(dev);
	if (!node_dev) {
		ret = -ENXIO;
		goto exit_rsc_init;
	}

	rscdev = devm_kzalloc(dev, sizeof(struct msm_bus_rsc_device_type),
								GFP_KERNEL);
	if (!rscdev) {
		ret = -ENOMEM;
		goto exit_rsc_init;
	}

	node_dev->rscdev = rscdev;
	rscdev->req_state = pdata->rscdev->req_state;
	rscdev->mbox = rpmh_get_byname(pdev, node_dev->node_info->name);

	if (IS_ERR_OR_NULL(rscdev->mbox)) {
		MSM_BUS_ERR("%s: Failed to get mbox:%s", __func__,
						node_dev->node_info->name);
	}

	// Add way to count # of VCDs, initialize LL
	for (i = 0; i < VCD_MAX_CNT; i++)
		INIT_LIST_HEAD(&rscdev->bcm_clist[i]);

exit_rsc_init:
	return ret;
}

static int msm_bus_postcon_setup(struct device *bus_dev, void *data)
{
	struct msm_bus_node_device_type *bus_node = NULL;
	struct msm_bus_rsc_device_type *rscdev;

	bus_node = to_msm_bus_node(bus_dev);
	if (!bus_node) {
		MSM_BUS_ERR("%s: Can't get device info", __func__);
		return -ENODEV;
	}

	if (bus_node->node_info->is_rsc_dev) {
		rscdev = bus_node->rscdev;
		rscdev->cmdlist_active = devm_kcalloc(bus_dev,
					rscdev->num_bcm_devs,
					sizeof(struct tcs_cmd), GFP_KERNEL);
		if (!rscdev->cmdlist_active)
			return -ENOMEM;

		rscdev->cmdlist_wake = devm_kcalloc(bus_dev,
					rscdev->num_bcm_devs,
					sizeof(struct tcs_cmd), GFP_KERNEL);
		if (!rscdev->cmdlist_wake)
			return -ENOMEM;

		rscdev->cmdlist_sleep = devm_kcalloc(bus_dev,
					rscdev->num_bcm_devs,
					sizeof(struct tcs_cmd),	GFP_KERNEL);
		if (!rscdev->cmdlist_sleep)
			return -ENOMEM;
	}

	return 0;
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
	int ret = 0, i = 0;
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
	node_info->bcm_req_idx = devm_kzalloc(bus_dev,
			sizeof(int) * pdata_node_info->num_bcm_devs,
			GFP_KERNEL);
	if (!node_info->bcm_req_idx) {
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	for (i = 0; i < pdata_node_info->num_bcm_devs; i++)
		node_info->bcm_req_idx[i] = -1;

	node_info->bus_device_id = pdata_node_info->bus_device_id;
	node_info->mas_rpm_id = pdata_node_info->mas_rpm_id;
	node_info->slv_rpm_id = pdata_node_info->slv_rpm_id;
	node_info->num_connections = pdata_node_info->num_connections;
	node_info->num_blist = pdata_node_info->num_blist;
	node_info->num_bcm_devs = pdata_node_info->num_bcm_devs;
	node_info->num_rsc_devs = pdata_node_info->num_rsc_devs;
	node_info->num_qports = pdata_node_info->num_qports;
	node_info->num_disable_ports = pdata_node_info->num_disable_ports;
	node_info->disable_ports = pdata_node_info->disable_ports;
	node_info->virt_dev = pdata_node_info->virt_dev;
	node_info->is_fab_dev = pdata_node_info->is_fab_dev;
	node_info->is_bcm_dev = pdata_node_info->is_bcm_dev;
	node_info->is_rsc_dev = pdata_node_info->is_rsc_dev;
	node_info->qos_params.prio_dflt = pdata_node_info->qos_params.prio_dflt;
	node_info->qos_params.limiter.bw =
				pdata_node_info->qos_params.limiter.bw;
	node_info->qos_params.limiter.sat =
				pdata_node_info->qos_params.limiter.sat;
	node_info->qos_params.limiter_en =
				pdata_node_info->qos_params.limiter_en;
	node_info->qos_params.reg.low_prio =
				pdata_node_info->qos_params.reg.low_prio;
	node_info->qos_params.reg.hi_prio =
				pdata_node_info->qos_params.reg.hi_prio;
	node_info->qos_params.reg.bw =
				pdata_node_info->qos_params.reg.bw;
	node_info->qos_params.reg.sat =
				pdata_node_info->qos_params.reg.sat;
	node_info->qos_params.reg_mode.read =
				pdata_node_info->qos_params.reg_mode.read;
	node_info->qos_params.reg_mode.write =
				pdata_node_info->qos_params.reg_mode.write;
	node_info->qos_params.urg_fwd_en =
				pdata_node_info->qos_params.urg_fwd_en;
	node_info->qos_params.defer_init_qos =
				pdata_node_info->qos_params.defer_init_qos;
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

	node_info->bl_cons = devm_kzalloc(bus_dev,
			pdata_node_info->num_blist * sizeof(int),
			GFP_KERNEL);
	if (!node_info->bl_cons) {
		MSM_BUS_ERR("%s:Bus black list connections alloc failed\n",
					__func__);
		devm_kfree(bus_dev, node_info->black_connections);
		devm_kfree(bus_dev, node_info->dev_connections);
		devm_kfree(bus_dev, node_info->connections);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	memcpy(node_info->bl_cons,
		pdata_node_info->bl_cons,
		sizeof(int) * pdata_node_info->num_blist);

	node_info->bcm_devs = devm_kzalloc(bus_dev,
			sizeof(struct device *) *
				pdata_node_info->num_bcm_devs,
			GFP_KERNEL);
	if (!node_info->bcm_devs) {
		MSM_BUS_ERR("%s:Bcm dev connections alloc failed\n", __func__);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	node_info->bcm_dev_ids = devm_kzalloc(bus_dev,
			sizeof(int) * pdata_node_info->num_bcm_devs,
			GFP_KERNEL);
	if (!node_info->bcm_dev_ids) {
		MSM_BUS_ERR("%s:Bus connections alloc failed\n", __func__);
		devm_kfree(bus_dev, node_info->bcm_devs);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	memcpy(node_info->bcm_dev_ids,
		pdata_node_info->bcm_dev_ids,
		sizeof(int) * pdata_node_info->num_bcm_devs);

	node_info->rsc_devs = devm_kzalloc(bus_dev,
			sizeof(struct device *) *
				pdata_node_info->num_rsc_devs,
			GFP_KERNEL);
	if (!node_info->rsc_devs) {
		MSM_BUS_ERR("%s:rsc dev connections alloc failed\n", __func__);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	node_info->rsc_dev_ids = devm_kzalloc(bus_dev,
			sizeof(int) * pdata_node_info->num_rsc_devs,
			GFP_KERNEL);
	if (!node_info->rsc_dev_ids) {
		MSM_BUS_ERR("%s:Bus connections alloc failed\n", __func__);
		devm_kfree(bus_dev, node_info->rsc_devs);
		ret = -ENOMEM;
		goto exit_copy_node_info;
	}

	memcpy(node_info->rsc_dev_ids,
		pdata_node_info->rsc_dev_ids,
		sizeof(int) * pdata_node_info->num_rsc_devs);

	node_info->qport = devm_kzalloc(bus_dev,
			sizeof(int) * pdata_node_info->num_qports,
			GFP_KERNEL);
	if (!node_info->qport) {
		MSM_BUS_ERR("%s:Bus qport allocation failed\n", __func__);
		devm_kfree(bus_dev, node_info->dev_connections);
		devm_kfree(bus_dev, node_info->connections);
		devm_kfree(bus_dev, node_info->bl_cons);
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
	int ret = -ENODEV, i = 0;

	/*
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
	bus_node->dirty = false;
	bus_node->num_qos_bcms = pdata->num_qos_bcms;
	if (bus_node->num_qos_bcms) {
		bus_node->qos_bcms = devm_kzalloc(bus_dev,
					(sizeof(struct qos_bcm_type) *
					bus_node->num_qos_bcms), GFP_KERNEL);
		if (!bus_node->qos_bcms) {
			ret = -ENOMEM;
			goto err_put_device;
		}
		for (i = 0; i < bus_node->num_qos_bcms; i++) {
			bus_node->qos_bcms[i].qos_bcm_id =
					pdata->qos_bcms[i].qos_bcm_id;
			bus_node->qos_bcms[i].vec.vec_a =
					pdata->qos_bcms[i].vec.vec_a;
			bus_node->qos_bcms[i].vec.vec_b =
					pdata->qos_bcms[i].vec.vec_b;
		}
	}
	bus_node->num_regs = pdata->num_regs;
	if (bus_node->num_regs)
		bus_node->node_regs = pdata->node_regs;

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
	struct msm_bus_node_device_type *bcm_node = NULL;
	struct msm_bus_node_device_type *rsc_node = NULL;
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
	if (!bus_node->node_info->is_fab_dev &&
		!bus_node->node_info->is_bcm_dev &&
		!bus_node->node_info->is_rsc_dev) {
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
				(void *)&bus_node->node_info->bl_cons[j],
				msm_bus_device_match_adhoc);

		if (!bus_node->node_info->black_connections[j]) {
			MSM_BUS_ERR("%s: Error finding conn %d for device %d\n",
				__func__, bus_node->node_info->bl_cons[j],
				bus_node->node_info->id);
			ret = -ENODEV;
			goto exit_setup_dev_conn;
		}
	}

	for (j = 0; j < bus_node->node_info->num_bcm_devs; j++) {
		bus_node->node_info->bcm_devs[j] =
			bus_find_device(&msm_bus_type, NULL,
				(void *)&bus_node->node_info->bcm_dev_ids[j],
				msm_bus_device_match_adhoc);

		if (!bus_node->node_info->bcm_devs[j]) {
			MSM_BUS_ERR("%s: Error finding conn %d for device %d",
				__func__, bus_node->node_info->bcm_dev_ids[j],
				 bus_node->node_info->id);
			ret = -ENODEV;
			goto exit_setup_dev_conn;
		}
		bcm_node = to_msm_bus_node(bus_node->node_info->bcm_devs[j]);
		bcm_node->bcmdev->num_bus_devs++;
	}

	for (j = 0; j < bus_node->node_info->num_rsc_devs; j++) {
		bus_node->node_info->rsc_devs[j] =
			bus_find_device(&msm_bus_type, NULL,
				(void *)&bus_node->node_info->rsc_dev_ids[j],
				msm_bus_device_match_adhoc);

		if (!bus_node->node_info->rsc_devs[j]) {
			MSM_BUS_ERR("%s: Error finding conn %d for device %d",
				__func__, bus_node->node_info->rsc_dev_ids[j],
				 bus_node->node_info->id);
			ret = -ENODEV;
			goto exit_setup_dev_conn;
		}
		rsc_node = to_msm_bus_node(bus_node->node_info->rsc_devs[j]);
		rsc_node->rscdev->num_bcm_devs++;
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

static int msm_bus_device_probe(struct platform_device *pdev)
{
	unsigned int i = 1, ret;
	struct msm_bus_device_node_registration *pdata;

	MSM_BUS_ERR("msm_bus: Probe started");
	/* If possible, get pdata from device-tree */
	if (pdev->dev.of_node)
		pdata = msm_bus_of_to_pdata(pdev);
	else {
		pdata = (struct msm_bus_device_node_registration *)
			pdev->dev.platform_data;
	}

	MSM_BUS_ERR("msm_bus: DT Parsing complete");

	if (IS_ERR_OR_NULL(pdata)) {
		MSM_BUS_ERR("No platform data found");
		ret = -ENODATA;
		goto exit_device_probe;
	}

	for (i = 0; i < pdata->num_devices; i++) {
		struct device *node_dev = NULL;

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
		}
		if (pdata->info[i].node_info->is_bcm_dev) {
			ret = msm_bus_bcm_init(node_dev, &pdata->info[i]);
			if (ret) {
				MSM_BUS_ERR("%s: Error intializing bcm %d",
					__func__, pdata->info[i].node_info->id);
				goto exit_device_probe;
			}
		}
		if (pdata->info[i].node_info->is_rsc_dev) {
			ret = msm_bus_rsc_init(pdev, node_dev, &pdata->info[i]);
			if (ret) {
				MSM_BUS_ERR("%s: Error intializing rsc %d",
					__func__, pdata->info[i].node_info->id);
				goto exit_device_probe;
			}
		}
	}

	ret = bus_for_each_dev(&msm_bus_type, NULL, NULL,
						msm_bus_setup_dev_conn);
	if (ret) {
		MSM_BUS_ERR("%s: Error setting up dev connections", __func__);
		goto exit_device_probe;
	}

	ret = bus_for_each_dev(&msm_bus_type, NULL, NULL,
						msm_bus_postcon_setup);
	if (ret) {
		MSM_BUS_ERR("%s: Error post connection setup", __func__);
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


static const struct of_device_id rules_match[] = {
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

static const struct of_device_id fabric_match[] = {
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

	MSM_BUS_ERR("msm_bus_fabric_rpmh_init_driver\n");
	rc =  platform_driver_register(&msm_bus_device_driver);

	if (rc) {
		MSM_BUS_ERR("Failed to register bus device driver");
		return rc;
	}
	return platform_driver_register(&msm_bus_rules_driver);
}

int __init msm_bus_device_late_init(void)
{
	commit_late_init_data(true);
	MSM_BUS_ERR("msm_bus_late_init: Remove handoff bw requests\n");
	init_time = false;
	return commit_late_init_data(false);
}
subsys_initcall(msm_bus_device_init_driver);
late_initcall_sync(msm_bus_device_late_init);
