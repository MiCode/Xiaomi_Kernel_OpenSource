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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/rtmutex.h>
#include <linux/clk.h>
#include <linux/msm-bus.h>
#include "msm_bus_core.h"
#include "msm_bus_rpmh.h"

#define NUM_CL_HANDLES	50
#define NUM_LNODES	3
#define MAX_STR_CL	50

#define MSM_BUS_MAS_ALC			144
#define MSM_BUS_RSC_APPS		8000
#define MSM_BUS_RSC_DISP		8001
#define BCM_TCS_CMD_ACV_APPS		0x8

struct bus_search_type {
	struct list_head link;
	struct list_head node_list;
};

struct handle_type {
	int num_entries;
	struct msm_bus_client **cl_list;
};

static struct handle_type handle_list;
static LIST_HEAD(commit_list);
static LIST_HEAD(late_init_clist);
static LIST_HEAD(query_list);

DEFINE_RT_MUTEX(msm_bus_adhoc_lock);

static bool chk_bl_list(struct list_head *black_list, unsigned int id)
{
	struct msm_bus_node_device_type *bus_node = NULL;

	list_for_each_entry(bus_node, black_list, link) {
		if (bus_node->node_info->id == id)
			return true;
	}
	return false;
}

static void copy_remaining_nodes(struct list_head *edge_list, struct list_head
	*traverse_list, struct list_head *route_list)
{
	struct bus_search_type *search_node;

	if (list_empty(edge_list) && list_empty(traverse_list))
		return;

	search_node = kzalloc(sizeof(struct bus_search_type), GFP_KERNEL);
	INIT_LIST_HEAD(&search_node->node_list);
	list_splice_init(edge_list, traverse_list);
	list_splice_init(traverse_list, &search_node->node_list);
	list_add_tail(&search_node->link, route_list);
}

/*
 * Duplicate instantiaion from msm_bus_arb.c. Todo there needs to be a
 * "util" file for these common func/macros.
 *
 */
uint64_t msm_bus_div64(uint64_t num, unsigned int base)
{
	uint64_t *n = &num;

	if ((num > 0) && (num < base))
		return 1;

	switch (base) {
	case 0:
		WARN(1, "AXI: Divide by 0 attempted\n");
	case 1: return num;
	case 2: return (num >> 1);
	case 4: return (num >> 2);
	case 8: return (num >> 3);
	case 16: return (num >> 4);
	case 32: return (num >> 5);
	}

	do_div(*n, base);
	return *n;
}

int msm_bus_device_match_adhoc(struct device *dev, void *id)
{
	int ret = 0;
	struct msm_bus_node_device_type *bnode = to_msm_bus_node(dev);

	if (bnode)
		ret = (bnode->node_info->id == *(unsigned int *)id);
	else
		ret = 0;

	return ret;
}

static void bcm_add_bus_req(struct device *dev)
{
	struct msm_bus_node_device_type *cur_dev = NULL;
	struct msm_bus_node_device_type *bcm_dev = NULL;
	struct link_node *lnode;
	int lnode_idx = -1;
	int max_num_lnodes = 0;
	int i;

	cur_dev = to_msm_bus_node(dev);
	if (!cur_dev) {
		MSM_BUS_ERR("%s: Null device ptr", __func__);
		goto exit_bcm_add_bus_req;
	}

	if (!cur_dev->node_info->num_bcm_devs)
		goto exit_bcm_add_bus_req;

	for (i = 0; i < cur_dev->node_info->num_bcm_devs; i++) {
		if (cur_dev->node_info->bcm_req_idx[i] != -1)
			continue;
		bcm_dev = to_msm_bus_node(cur_dev->node_info->bcm_devs[i]);
		max_num_lnodes = bcm_dev->bcmdev->num_bus_devs;
		if (!bcm_dev->num_lnodes) {
			bcm_dev->lnode_list = devm_kzalloc(dev,
				sizeof(struct link_node) * max_num_lnodes,
								GFP_KERNEL);
			if (!bcm_dev->lnode_list)
				goto exit_bcm_add_bus_req;

			lnode = bcm_dev->lnode_list;
			bcm_dev->num_lnodes = max_num_lnodes;
			lnode_idx = 0;
		} else {
			int i;

			for (i = 0; i < bcm_dev->num_lnodes; i++) {
				if (!bcm_dev->lnode_list[i].in_use)
					break;
			}

			if (i < bcm_dev->num_lnodes) {
				lnode = &bcm_dev->lnode_list[i];
				lnode_idx = i;
			} else {
				struct link_node *realloc_list;
				size_t cur_size = sizeof(struct link_node) *
						bcm_dev->num_lnodes;

				bcm_dev->num_lnodes += NUM_LNODES;
				realloc_list = msm_bus_realloc_devmem(
						dev,
						bcm_dev->lnode_list,
						cur_size,
						sizeof(struct link_node) *
						bcm_dev->num_lnodes,
								GFP_KERNEL);

				if (!realloc_list)
					goto exit_bcm_add_bus_req;

				bcm_dev->lnode_list = realloc_list;
				lnode = &bcm_dev->lnode_list[i];
				lnode_idx = i;
			}
		}

		lnode->in_use = 1;
		lnode->bus_dev_id = cur_dev->node_info->id;
		cur_dev->node_info->bcm_req_idx[i] = lnode_idx;
		memset(lnode->lnode_ib, 0, sizeof(uint64_t) * NUM_CTX);
		memset(lnode->lnode_ab, 0, sizeof(uint64_t) * NUM_CTX);
	}

exit_bcm_add_bus_req:
	return;
}

static int gen_lnode(struct device *dev,
			int next_hop, int prev_idx, const char *cl_name)
{
	struct link_node *lnode;
	struct msm_bus_node_device_type *cur_dev = NULL;
	int lnode_idx = -1;

	if (!dev)
		goto exit_gen_lnode;

	cur_dev = to_msm_bus_node(dev);
	if (!cur_dev) {
		MSM_BUS_ERR("%s: Null device ptr", __func__);
		goto exit_gen_lnode;
	}

	if (!cur_dev->num_lnodes) {
		cur_dev->lnode_list = devm_kzalloc(dev,
				sizeof(struct link_node) * NUM_LNODES,
								GFP_KERNEL);
		if (!cur_dev->lnode_list)
			goto exit_gen_lnode;

		lnode = cur_dev->lnode_list;
		cur_dev->num_lnodes = NUM_LNODES;
		lnode_idx = 0;
	} else {
		int i;

		for (i = 0; i < cur_dev->num_lnodes; i++) {
			if (!cur_dev->lnode_list[i].in_use)
				break;
		}

		if (i < cur_dev->num_lnodes) {
			lnode = &cur_dev->lnode_list[i];
			lnode_idx = i;
		} else {
			struct link_node *realloc_list;
			size_t cur_size = sizeof(struct link_node) *
					cur_dev->num_lnodes;

			cur_dev->num_lnodes += NUM_LNODES;
			realloc_list = msm_bus_realloc_devmem(
					dev,
					cur_dev->lnode_list,
					cur_size,
					sizeof(struct link_node) *
					cur_dev->num_lnodes, GFP_KERNEL);

			if (!realloc_list)
				goto exit_gen_lnode;

			cur_dev->lnode_list = realloc_list;
			lnode = &cur_dev->lnode_list[i];
			lnode_idx = i;
		}
	}

	lnode->in_use = 1;
	lnode->cl_name = cl_name;
	if (next_hop == cur_dev->node_info->id) {
		lnode->next = -1;
		lnode->next_dev = NULL;
	} else {
		lnode->next = prev_idx;
		lnode->next_dev = bus_find_device(&msm_bus_type, NULL,
					(void *) &next_hop,
					msm_bus_device_match_adhoc);
	}

	memset(lnode->lnode_ib, 0, sizeof(uint64_t) * NUM_CTX);
	memset(lnode->lnode_ab, 0, sizeof(uint64_t) * NUM_CTX);

exit_gen_lnode:
	return lnode_idx;
}

static int remove_lnode(struct msm_bus_node_device_type *cur_dev,
				int lnode_idx)
{
	int ret = 0;

	if (!cur_dev) {
		MSM_BUS_ERR("%s: Null device ptr", __func__);
		ret = -ENODEV;
		goto exit_remove_lnode;
	}

	if (lnode_idx != -1) {
		if (!cur_dev->num_lnodes ||
				(lnode_idx > (cur_dev->num_lnodes - 1))) {
			MSM_BUS_ERR("%s: Invalid Idx %d, num_lnodes %d",
				__func__, lnode_idx, cur_dev->num_lnodes);
			ret = -ENODEV;
			goto exit_remove_lnode;
		}

		cur_dev->lnode_list[lnode_idx].next = -1;
		cur_dev->lnode_list[lnode_idx].next_dev = NULL;
		cur_dev->lnode_list[lnode_idx].in_use = 0;
		cur_dev->lnode_list[lnode_idx].cl_name = NULL;
	}

exit_remove_lnode:
	return ret;
}

static int prune_path(struct list_head *route_list, int dest, int src,
				struct list_head *black_list, int found,
				const char *cl_name)
{
	struct bus_search_type *search_node, *temp_search_node;
	struct msm_bus_node_device_type *bus_node;
	struct list_head *bl_list;
	struct list_head *temp_bl_list;
	int search_dev_id = dest;
	struct device *dest_dev = bus_find_device(&msm_bus_type, NULL,
					(void *) &dest,
					msm_bus_device_match_adhoc);
	int lnode_hop = -1;

	if (!found)
		goto reset_links;

	if (!dest_dev) {
		MSM_BUS_ERR("%s: Can't find dest dev %d", __func__, dest);
		goto exit_prune_path;
	}

	lnode_hop = gen_lnode(dest_dev, search_dev_id, lnode_hop, cl_name);
	bcm_add_bus_req(dest_dev);

	list_for_each_entry_reverse(search_node, route_list, link) {
		list_for_each_entry(bus_node, &search_node->node_list, link) {
			unsigned int i;

			for (i = 0; i < bus_node->node_info->num_connections;
									i++) {
				if (bus_node->node_info->connections[i] ==
								search_dev_id) {
					dest_dev = bus_find_device(
						&msm_bus_type,
						NULL,
						(void *)
						&bus_node->node_info->
						id,
						msm_bus_device_match_adhoc);

					if (!dest_dev) {
						lnode_hop = -1;
						goto reset_links;
					}

					lnode_hop = gen_lnode(dest_dev,
							search_dev_id,
							lnode_hop, cl_name);
					bcm_add_bus_req(dest_dev);
					search_dev_id =
						bus_node->node_info->id;
					break;
				}
			}
		}
	}
reset_links:
	list_for_each_entry_safe(search_node, temp_search_node, route_list,
									link) {
		list_for_each_entry(bus_node, &search_node->node_list, link)
			bus_node->node_info->is_traversed = false;

		list_del(&search_node->link);
		kfree(search_node);
	}

	list_for_each_safe(bl_list, temp_bl_list, black_list)
		list_del(bl_list);

exit_prune_path:
	return lnode_hop;
}

static void setup_bl_list(struct msm_bus_node_device_type *node,
				struct list_head *black_list)
{
	unsigned int i;

	for (i = 0; i < node->node_info->num_blist; i++) {
		struct msm_bus_node_device_type *bdev;

		bdev = to_msm_bus_node(node->node_info->black_connections[i]);
		list_add_tail(&bdev->link, black_list);
	}
}

static int getpath(struct device *src_dev, int dest, const char *cl_name)
{
	struct list_head traverse_list;
	struct list_head edge_list;
	struct list_head route_list;
	struct list_head black_list;
	struct msm_bus_node_device_type *src_node;
	struct bus_search_type *search_node;
	int found = 0;
	int depth_index = 0;
	int first_hop = -1;
	int src;

	INIT_LIST_HEAD(&traverse_list);
	INIT_LIST_HEAD(&edge_list);
	INIT_LIST_HEAD(&route_list);
	INIT_LIST_HEAD(&black_list);

	if (!src_dev) {
		MSM_BUS_ERR("%s: Cannot locate src dev ", __func__);
		goto exit_getpath;
	}

	src_node = to_msm_bus_node(src_dev);
	if (!src_node) {
		MSM_BUS_ERR("%s:Fatal, Source node not found", __func__);
		goto exit_getpath;
	}
	src = src_node->node_info->id;
	list_add_tail(&src_node->link, &traverse_list);

	while ((!found && !list_empty(&traverse_list))) {
		struct msm_bus_node_device_type *bus_node = NULL;
		/* Locate dest_id in the traverse list */
		list_for_each_entry(bus_node, &traverse_list, link) {
			if (bus_node->node_info->id == dest) {
				found = 1;
				break;
			}
		}

		if (!found) {
			unsigned int i;
			/* Setup the new edge list */
			list_for_each_entry(bus_node, &traverse_list, link) {
				/* Setup list of black-listed nodes */
				setup_bl_list(bus_node, &black_list);

				for (i = 0; i < bus_node->node_info->
						num_connections; i++) {
					bool skip;
					struct msm_bus_node_device_type
							*node_conn;
					node_conn =
					to_msm_bus_node(bus_node->node_info->
						dev_connections[i]);
					if (node_conn->node_info->
							is_traversed) {
						MSM_BUS_ERR("Circ Path %d\n",
						node_conn->node_info->id);
						goto reset_traversed;
					}
					skip = chk_bl_list(&black_list,
							bus_node->node_info->
							connections[i]);
					if (!skip) {
						list_add_tail(&node_conn->link,
							&edge_list);
						node_conn->node_info->
							is_traversed = true;
					}
				}
			}

			/* Keep tabs of the previous search list */
			search_node = kzalloc(sizeof(struct bus_search_type),
					 GFP_KERNEL);
			INIT_LIST_HEAD(&search_node->node_list);
			list_splice_init(&traverse_list,
					 &search_node->node_list);
			/* Add the previous search list to a route list */
			list_add_tail(&search_node->link, &route_list);
			/* Advancing the list depth */
			depth_index++;
			list_splice_init(&edge_list, &traverse_list);
		}
	}
reset_traversed:
	copy_remaining_nodes(&edge_list, &traverse_list, &route_list);
	first_hop = prune_path(&route_list, dest, src, &black_list, found,
								cl_name);

exit_getpath:
	return first_hop;
}

static void bcm_update_acv_req(struct msm_bus_node_device_type *cur_rsc,
				uint64_t max_ab, uint64_t max_ib,
				uint64_t *vec_a, uint64_t *vec_b,
				uint32_t *acv, int ctx)
{
	uint32_t acv_bmsk = 0;
	/*
	 * Base ACV voting on current RSC until mapping is set up in commanddb
	 * that allows us to vote ACV based on master.
	 */

	if (cur_rsc->node_info->id == MSM_BUS_RSC_APPS)
		acv_bmsk = BCM_TCS_CMD_ACV_APPS;

	if (max_ab == 0 && max_ib == 0)
		*acv = *acv & ~acv_bmsk;
	else
		*acv = *acv | acv_bmsk;
	*vec_a = 0;
	*vec_b = *acv;
}

static void bcm_update_bus_req(struct device *dev, int ctx)
{
	struct msm_bus_node_device_type *cur_dev = NULL;
	struct msm_bus_node_device_type *bcm_dev = NULL;
	struct msm_bus_node_device_type *cur_rsc = NULL;

	int i, j;
	uint64_t max_ib = 0;
	uint64_t max_ab = 0;
	int lnode_idx = 0;

	cur_dev = to_msm_bus_node(dev);
	if (!cur_dev) {
		MSM_BUS_ERR("%s: Null device ptr", __func__);
		goto exit_bcm_update_bus_req;
	}

	if (!cur_dev->node_info->num_bcm_devs)
		goto exit_bcm_update_bus_req;

	for (i = 0; i < cur_dev->node_info->num_bcm_devs; i++) {
		bcm_dev = to_msm_bus_node(cur_dev->node_info->bcm_devs[i]);

		if (!bcm_dev)
			goto exit_bcm_update_bus_req;

		lnode_idx = cur_dev->node_info->bcm_req_idx[i];
		bcm_dev->lnode_list[lnode_idx].lnode_ib[ctx] =
			msm_bus_div64(cur_dev->node_bw[ctx].max_ib *
					(uint64_t)bcm_dev->bcmdev->width,
				cur_dev->node_info->agg_params.buswidth);

		bcm_dev->lnode_list[lnode_idx].lnode_ab[ctx] =
			msm_bus_div64(cur_dev->node_bw[ctx].sum_ab *
					(uint64_t)bcm_dev->bcmdev->width,
				cur_dev->node_info->agg_params.buswidth *
				cur_dev->node_info->agg_params.num_aggports);

		for (j = 0; j < bcm_dev->num_lnodes; j++) {
			if (ctx == ACTIVE_CTX) {
				max_ib = max(max_ib,
				max(bcm_dev->lnode_list[j].lnode_ib[ACTIVE_CTX],
				bcm_dev->lnode_list[j].lnode_ib[DUAL_CTX]));
				max_ab = max(max_ab,
				bcm_dev->lnode_list[j].lnode_ab[ACTIVE_CTX] +
				bcm_dev->lnode_list[j].lnode_ab[DUAL_CTX]);
			} else {
				max_ib = max(max_ib,
					bcm_dev->lnode_list[j].lnode_ib[ctx]);
				max_ab = max(max_ab,
					bcm_dev->lnode_list[j].lnode_ab[ctx]);
			}
		}
		bcm_dev->node_bw[ctx].max_ab = max_ab;
		bcm_dev->node_bw[ctx].max_ib = max_ib;

		max_ab = msm_bus_div64(max_ab, bcm_dev->bcmdev->unit_size);
		max_ib = msm_bus_div64(max_ib, bcm_dev->bcmdev->unit_size);

		if (bcm_dev->node_info->id == MSM_BUS_BCM_ACV) {
			cur_rsc = to_msm_bus_node(bcm_dev->node_info->
						rsc_devs[0]);
			bcm_update_acv_req(cur_rsc, max_ab, max_ib,
					&bcm_dev->node_vec[ctx].vec_a,
					&bcm_dev->node_vec[ctx].vec_b,
					&cur_rsc->rscdev->acv[ctx], ctx);

		} else {
			bcm_dev->node_vec[ctx].vec_a = max_ab;
			bcm_dev->node_vec[ctx].vec_b = max_ib;
		}
	}
exit_bcm_update_bus_req:
	return;
}

static void bcm_query_bus_req(struct device *dev, int ctx)
{
	struct msm_bus_node_device_type *cur_dev = NULL;
	struct msm_bus_node_device_type *bcm_dev = NULL;
	struct msm_bus_node_device_type *cur_rsc = NULL;
	int i, j;
	uint64_t max_query_ib = 0;
	uint64_t max_query_ab = 0;
	int lnode_idx = 0;

	cur_dev = to_msm_bus_node(dev);
	if (!cur_dev) {
		MSM_BUS_ERR("%s: Null device ptr", __func__);
		goto exit_bcm_query_bus_req;
	}

	if (!cur_dev->node_info->num_bcm_devs)
		goto exit_bcm_query_bus_req;

	for (i = 0; i < cur_dev->node_info->num_bcm_devs; i++) {
		bcm_dev = to_msm_bus_node(cur_dev->node_info->bcm_devs[i]);

		if (!bcm_dev)
			goto exit_bcm_query_bus_req;

		lnode_idx = cur_dev->node_info->bcm_req_idx[i];
		bcm_dev->lnode_list[lnode_idx].lnode_query_ib[ctx] =
			msm_bus_div64(cur_dev->node_bw[ctx].max_query_ib *
					(uint64_t)bcm_dev->bcmdev->width,
				cur_dev->node_info->agg_params.buswidth);

		bcm_dev->lnode_list[lnode_idx].lnode_query_ab[ctx] =
			msm_bus_div64(cur_dev->node_bw[ctx].sum_query_ab *
					(uint64_t)bcm_dev->bcmdev->width,
				cur_dev->node_info->agg_params.num_aggports *
				cur_dev->node_info->agg_params.buswidth);

		for (j = 0; j < bcm_dev->num_lnodes; j++) {
			if (ctx == ACTIVE_CTX) {
				max_query_ib = max(max_query_ib,
				max(bcm_dev->lnode_list[j].
					lnode_query_ib[ACTIVE_CTX],
				bcm_dev->lnode_list[j].
					lnode_query_ib[DUAL_CTX]));

				max_query_ab = max(max_query_ab,
				bcm_dev->lnode_list[j].
						lnode_query_ab[ACTIVE_CTX] +
				bcm_dev->lnode_list[j].
						lnode_query_ab[DUAL_CTX]);
			} else {
				max_query_ib = max(max_query_ib,
					bcm_dev->lnode_list[j].
						lnode_query_ib[ctx]);
				max_query_ab = max(max_query_ab,
					bcm_dev->lnode_list[j].
						lnode_query_ab[ctx]);
			}
		}

		max_query_ab = msm_bus_div64(max_query_ab,
						bcm_dev->bcmdev->unit_size);
		max_query_ib = msm_bus_div64(max_query_ib,
						bcm_dev->bcmdev->unit_size);

		if (bcm_dev->node_info->id == MSM_BUS_BCM_ACV) {
			cur_rsc = to_msm_bus_node(bcm_dev->node_info->
						rsc_devs[0]);
			bcm_update_acv_req(cur_rsc, max_query_ab, max_query_ib,
					&bcm_dev->node_vec[ctx].query_vec_a,
					&bcm_dev->node_vec[ctx].query_vec_b,
					&cur_rsc->rscdev->query_acv[ctx], ctx);
		} else {
			bcm_dev->node_vec[ctx].query_vec_a = max_query_ab;
			bcm_dev->node_vec[ctx].query_vec_b = max_query_ib;
		}

		bcm_dev->node_bw[ctx].max_query_ab = max_query_ab;
		bcm_dev->node_bw[ctx].max_query_ib = max_query_ib;
	}
exit_bcm_query_bus_req:
	return;
}

static void bcm_update_alc_req(struct msm_bus_node_device_type *dev, int ctx)
{
	struct msm_bus_node_device_type *bcm_dev = NULL;
	int i;
	uint64_t max_alc = 0;

	if (!dev || !to_msm_bus_node(dev->node_info->bus_device)) {
		MSM_BUS_ERR("Bus node pointer is Invalid");
		goto exit_bcm_update_alc_req;
	}

	for (i = 0; i < dev->num_lnodes; i++)
		max_alc = max(max_alc, dev->lnode_list[i].alc_idx[ctx]);

	dev->node_bw[ctx].max_alc = max_alc;

	bcm_dev = to_msm_bus_node(dev->node_info->bcm_devs[0]);

	if (ctx == ACTIVE_CTX) {
		max_alc = max(max_alc,
				max(dev->node_bw[ACTIVE_CTX].max_alc,
				dev->node_bw[DUAL_CTX].max_alc));
	} else {
		max_alc = dev->node_bw[ctx].max_alc;
	}

	bcm_dev->node_bw[ctx].max_alc = max_alc;
	bcm_dev->node_vec[ctx].vec_a = max_alc;
	bcm_dev->node_vec[ctx].vec_b = 0;

exit_bcm_update_alc_req:
	return;
}

int bcm_remove_handoff_req(struct device *dev, void *data)
{
	struct msm_bus_node_device_type *bus_dev = NULL;
	struct msm_bus_node_device_type *cur_bcm = NULL;
	struct msm_bus_node_device_type *cur_rsc = NULL;
	int ret = 0;

	bus_dev = to_msm_bus_node(dev);
	if (bus_dev->node_info->is_bcm_dev ||
		bus_dev->node_info->is_fab_dev ||
		bus_dev->node_info->is_rsc_dev)
		goto exit_bcm_remove_handoff_req;

	if (bus_dev->node_info->num_bcm_devs) {
		cur_bcm = to_msm_bus_node(bus_dev->node_info->bcm_devs[0]);
		if (cur_bcm->node_info->num_rsc_devs) {
			cur_rsc =
			to_msm_bus_node(cur_bcm->node_info->rsc_devs[0]);
			if (cur_rsc->node_info->id != MSM_BUS_RSC_APPS)
				goto exit_bcm_remove_handoff_req;
		}
	}

	if (!bus_dev->dirty) {
		list_add_tail(&bus_dev->link, &late_init_clist);
		bus_dev->dirty = true;
	}

exit_bcm_remove_handoff_req:
	return ret;
}

static void aggregate_bus_req(struct msm_bus_node_device_type *bus_dev,
									int ctx)
{
	int i;
	uint64_t max_ib = 0;
	uint64_t sum_ab = 0;

	if (!bus_dev || !to_msm_bus_node(bus_dev->node_info->bus_device)) {
		MSM_BUS_ERR("Bus node pointer is Invalid");
		goto exit_agg_bus_req;
	}

	for (i = 0; i < bus_dev->num_lnodes; i++) {
		max_ib = max(max_ib, bus_dev->lnode_list[i].lnode_ib[ctx]);
		sum_ab += bus_dev->lnode_list[i].lnode_ab[ctx];
	}

	bus_dev->node_bw[ctx].sum_ab = sum_ab;
	bus_dev->node_bw[ctx].max_ib = max_ib;

exit_agg_bus_req:
	return;
}

static void aggregate_bus_query_req(struct msm_bus_node_device_type *bus_dev,
									int ctx)
{
	int i;
	uint64_t max_ib = 0;
	uint64_t sum_ab = 0;

	if (!bus_dev || !to_msm_bus_node(bus_dev->node_info->bus_device)) {
		MSM_BUS_ERR("Bus node pointer is Invalid");
		goto exit_agg_bus_req;
	}

	for (i = 0; i < bus_dev->num_lnodes; i++) {
		max_ib = max(max_ib,
				bus_dev->lnode_list[i].lnode_query_ib[ctx]);
		sum_ab += bus_dev->lnode_list[i].lnode_query_ab[ctx];
	}

	bus_dev->node_bw[ctx].sum_query_ab = sum_ab;
	bus_dev->node_bw[ctx].max_query_ib = max_ib;

exit_agg_bus_req:
	return;
}

static void commit_data(void)
{
	msm_bus_commit_data(&commit_list);
	INIT_LIST_HEAD(&commit_list);
}

int commit_late_init_data(bool lock)
{
	int rc;

	if (lock) {
		rt_mutex_lock(&msm_bus_adhoc_lock);
		return 0;
	}

	rc = bus_for_each_dev(&msm_bus_type, NULL, NULL,
						bcm_remove_handoff_req);

	msm_bus_commit_data(&late_init_clist);
	INIT_LIST_HEAD(&late_init_clist);

	rt_mutex_unlock(&msm_bus_adhoc_lock);
	return rc;
}



static void add_node_to_clist(struct msm_bus_node_device_type *node)
{
	struct msm_bus_node_device_type *node_parent =
			to_msm_bus_node(node->node_info->bus_device);

	if (!node->dirty) {
		list_add_tail(&node->link, &commit_list);
		node->dirty = true;
	}

	if (!node_parent->dirty) {
		list_add_tail(&node_parent->link, &commit_list);
		node_parent->dirty = true;
	}
}

static void add_node_to_query_list(struct msm_bus_node_device_type *node)
{
	if (!node->query_dirty) {
		list_add_tail(&node->query_link, &query_list);
		node->query_dirty = true;
	}
}

static int update_path(struct device *src_dev, int dest, uint64_t act_req_ib,
			uint64_t act_req_bw, uint64_t slp_req_ib,
			uint64_t slp_req_bw, uint64_t cur_ib, uint64_t cur_bw,
			int src_idx, int ctx)
{
	struct device *next_dev = NULL;
	struct link_node *lnode = NULL;
	struct msm_bus_node_device_type *dev_info = NULL;
	int curr_idx;
	int ret = 0;

	if (IS_ERR_OR_NULL(src_dev)) {
		MSM_BUS_ERR("%s: No source device", __func__);
		ret = -ENODEV;
		goto exit_update_path;
	}

	next_dev = src_dev;

	if (src_idx < 0) {
		MSM_BUS_ERR("%s: Invalid lnode idx %d", __func__, src_idx);
		ret = -ENXIO;
		goto exit_update_path;
	}
	curr_idx = src_idx;

	while (next_dev) {
		int i;

		dev_info = to_msm_bus_node(next_dev);

		if (curr_idx >= dev_info->num_lnodes) {
			MSM_BUS_ERR("%s: Invalid lnode Idx %d num lnodes %d",
			 __func__, curr_idx, dev_info->num_lnodes);
			ret = -ENXIO;
			goto exit_update_path;
		}

		lnode = &dev_info->lnode_list[curr_idx];
		if (!lnode) {
			MSM_BUS_ERR("%s: Invalid lnode ptr lnode %d",
				 __func__, curr_idx);
			ret = -ENXIO;
			goto exit_update_path;
		}
		lnode->lnode_ib[ACTIVE_CTX] = act_req_ib;
		lnode->lnode_ab[ACTIVE_CTX] = act_req_bw;
		lnode->lnode_ib[DUAL_CTX] = slp_req_ib;
		lnode->lnode_ab[DUAL_CTX] = slp_req_bw;

		for (i = 0; i < NUM_CTX; i++) {
			aggregate_bus_req(dev_info, i);
			bcm_update_bus_req(next_dev, i);
		}

		add_node_to_clist(dev_info);

		next_dev = lnode->next_dev;
		curr_idx = lnode->next;
	}

exit_update_path:
	return ret;
}

static int update_alc_vote(struct device *alc_dev, uint64_t act_req_fa_lat,
			uint64_t act_req_idle_time, uint64_t slp_req_fa_lat,
			uint64_t slp_req_idle_time, uint64_t cur_fa_lat,
			uint64_t cur_idle_time, int idx, int ctx)
{
	struct link_node *lnode = NULL;
	struct msm_bus_node_device_type *dev_info = NULL;
	int curr_idx, i;
	int ret = 0;

	if (IS_ERR_OR_NULL(alc_dev)) {
		MSM_BUS_ERR("%s: No source device", __func__);
		ret = -ENODEV;
		goto exit_update_alc_vote;
	}

	if (idx < 0) {
		MSM_BUS_ERR("%s: Invalid lnode idx %d", __func__, idx);
		ret = -ENXIO;
		goto exit_update_alc_vote;
	}

	dev_info = to_msm_bus_node(alc_dev);
	curr_idx = idx;

	if (curr_idx >= dev_info->num_lnodes) {
		MSM_BUS_ERR("%s: Invalid lnode Idx %d num lnodes %d",
				 __func__, curr_idx, dev_info->num_lnodes);
		ret = -ENXIO;
		goto exit_update_alc_vote;
	}

	lnode = &dev_info->lnode_list[curr_idx];
	if (!lnode) {
		MSM_BUS_ERR("%s: Invalid lnode ptr lnode %d",
			 __func__, curr_idx);
		ret = -ENXIO;
		goto exit_update_alc_vote;
	}

	/*
	 * Add aggregation and mapping logic once LUT is avail.
	 * Use default values for time being.
	 */
	lnode->alc_idx[ACTIVE_CTX] = 12;
	lnode->alc_idx[DUAL_CTX] = 0;

	for (i = 0; i < NUM_CTX; i++)
		bcm_update_alc_req(dev_info, i);

	add_node_to_clist(dev_info);

exit_update_alc_vote:
	return ret;
}


static int query_path(struct device *src_dev, int dest, uint64_t act_req_ib,
			uint64_t act_req_bw, uint64_t slp_req_ib,
			uint64_t slp_req_bw, uint64_t cur_ib, uint64_t cur_bw,
			int src_idx)
{
	struct device *next_dev = NULL;
	struct link_node *lnode = NULL;
	struct msm_bus_node_device_type *dev_info = NULL;
	int curr_idx;
	int ret = 0;

	if (IS_ERR_OR_NULL(src_dev)) {
		MSM_BUS_ERR("%s: No source device", __func__);
		ret = -ENODEV;
		goto exit_query_path;
	}

	next_dev = src_dev;

	if (src_idx < 0) {
		MSM_BUS_ERR("%s: Invalid lnode idx %d", __func__, src_idx);
		ret = -ENXIO;
		goto exit_query_path;
	}
	curr_idx = src_idx;

	while (next_dev) {
		int i;

		dev_info = to_msm_bus_node(next_dev);

		if (curr_idx >= dev_info->num_lnodes) {
			MSM_BUS_ERR("%s: Invalid lnode Idx %d num lnodes %d",
			 __func__, curr_idx, dev_info->num_lnodes);
			ret = -ENXIO;
			goto exit_query_path;
		}

		lnode = &dev_info->lnode_list[curr_idx];
		if (!lnode) {
			MSM_BUS_ERR("%s: Invalid lnode ptr lnode %d",
				 __func__, curr_idx);
			ret = -ENXIO;
			goto exit_query_path;
		}
		lnode->lnode_query_ib[ACTIVE_CTX] = act_req_ib;
		lnode->lnode_query_ab[ACTIVE_CTX] = act_req_bw;
		lnode->lnode_query_ib[DUAL_CTX] = slp_req_ib;
		lnode->lnode_query_ab[DUAL_CTX] = slp_req_bw;

		for (i = 0; i < NUM_CTX; i++) {
			aggregate_bus_query_req(dev_info, i);
			bcm_query_bus_req(next_dev, i);
		}

		add_node_to_query_list(dev_info);

		next_dev = lnode->next_dev;
		curr_idx = lnode->next;
	}

exit_query_path:
	return ret;
}

static int remove_path(struct device *src_dev, int dst, uint64_t cur_ib,
			uint64_t cur_ab, int src_idx, int active_only)
{
	struct device *next_dev = NULL;
	struct link_node *lnode = NULL;
	struct msm_bus_node_device_type *dev_info = NULL;
	int ret = 0;
	int cur_idx = src_idx;
	int next_idx;

	/* Update the current path to zero out all request from
	 * this cient on all paths
	 */
	if (!src_dev) {
		MSM_BUS_ERR("%s: Can't find source device", __func__);
		ret = -ENODEV;
		goto exit_remove_path;
	}

	ret = update_path(src_dev, dst, 0, 0, 0, 0, cur_ib, cur_ab, src_idx,
							active_only);
	if (ret) {
		MSM_BUS_ERR("%s: Error zeroing out path ctx %d",
					__func__, ACTIVE_CTX);
		goto exit_remove_path;
	}

	next_dev = src_dev;

	while (next_dev) {
		dev_info = to_msm_bus_node(next_dev);
		lnode = &dev_info->lnode_list[cur_idx];
		next_idx = lnode->next;
		next_dev = lnode->next_dev;
		remove_lnode(dev_info, cur_idx);
		cur_idx = next_idx;
	}

exit_remove_path:
	return ret;
}

static void getpath_debug(int src, int curr, int active_only)
{
	struct device *dev_node;
	struct device *dev_it;
	unsigned int hop = 1;
	int idx;
	struct msm_bus_node_device_type *devinfo;
	int i;

	dev_node = bus_find_device(&msm_bus_type, NULL,
				(void *) &src,
				msm_bus_device_match_adhoc);

	if (!dev_node) {
		MSM_BUS_ERR("SRC NOT FOUND %d", src);
		return;
	}

	idx = curr;
	devinfo = to_msm_bus_node(dev_node);
	dev_it = dev_node;

	MSM_BUS_ERR("Route list Src %d", src);
	while (dev_it) {
		struct msm_bus_node_device_type *busdev =
			to_msm_bus_node(devinfo->node_info->bus_device);

		MSM_BUS_ERR("Hop[%d] at Device %d ctx %d", hop,
					devinfo->node_info->id, active_only);

		for (i = 0; i < NUM_CTX; i++) {
			MSM_BUS_ERR("dev info sel ib %llu",
						devinfo->node_bw[i].cur_clk_hz);
			MSM_BUS_ERR("dev info sel ab %llu",
						devinfo->node_bw[i].sum_ab);
		}

		dev_it = devinfo->lnode_list[idx].next_dev;
		idx = devinfo->lnode_list[idx].next;
		if (dev_it)
			devinfo = to_msm_bus_node(dev_it);

		MSM_BUS_ERR("Bus Device %d", busdev->node_info->id);
		MSM_BUS_ERR("Bus Clock %llu", busdev->clk[active_only].rate);

		if (idx < 0)
			break;
		hop++;
	}
}

static void unregister_client_adhoc(uint32_t cl)
{
	int i;
	struct msm_bus_scale_pdata *pdata;
	int lnode, src, curr, dest;
	uint64_t  cur_clk, cur_bw;
	struct msm_bus_client *client;
	struct device *src_dev;

	rt_mutex_lock(&msm_bus_adhoc_lock);
	if (!cl) {
		MSM_BUS_ERR("%s: Null cl handle passed unregister\n",
				__func__);
		goto exit_unregister_client;
	}
	client = handle_list.cl_list[cl];
	pdata = client->pdata;
	if (!pdata) {
		MSM_BUS_ERR("%s: Null pdata passed to unregister\n",
				__func__);
		goto exit_unregister_client;
	}

	curr = client->curr;
	if (curr >= pdata->num_usecases || curr < 0) {
		MSM_BUS_ERR("Invalid index Defaulting curr to 0");
		curr = 0;
	}

	for (i = 0; i < pdata->usecase->num_paths; i++) {
		src = client->pdata->usecase[curr].vectors[i].src;
		dest = client->pdata->usecase[curr].vectors[i].dst;

		lnode = client->src_pnode[i];
		src_dev = client->src_devs[i];
		cur_clk = client->pdata->usecase[curr].vectors[i].ib;
		cur_bw = client->pdata->usecase[curr].vectors[i].ab;
		remove_path(src_dev, dest, cur_clk, cur_bw, lnode,
						pdata->active_only);
	}
	commit_data();
	msm_bus_dbg_client_data(client->pdata, MSM_BUS_DBG_UNREGISTER, cl);
	kfree(client->src_pnode);
	kfree(client->src_devs);
	kfree(client);
	handle_list.cl_list[cl] = NULL;
exit_unregister_client:
	rt_mutex_unlock(&msm_bus_adhoc_lock);
}

static int alloc_handle_lst(int size)
{
	int ret = 0;
	struct msm_bus_client **t_cl_list;

	if (!handle_list.num_entries) {
		t_cl_list = kzalloc(sizeof(struct msm_bus_client *)
			* NUM_CL_HANDLES, GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(t_cl_list)) {
			ret = -ENOMEM;
			MSM_BUS_ERR("%s: Failed to allocate handles list",
								__func__);
			goto exit_alloc_handle_lst;
		}
		handle_list.cl_list = t_cl_list;
		handle_list.num_entries += NUM_CL_HANDLES;
	} else {
		t_cl_list = krealloc(handle_list.cl_list,
				sizeof(struct msm_bus_client *) *
				(handle_list.num_entries + NUM_CL_HANDLES),
				GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(t_cl_list)) {
			ret = -ENOMEM;
			MSM_BUS_ERR("%s: Failed to allocate handles list",
								__func__);
			goto exit_alloc_handle_lst;
		}

		handle_list.cl_list = t_cl_list;
		memset(&handle_list.cl_list[handle_list.num_entries], 0,
			NUM_CL_HANDLES * sizeof(struct msm_bus_client *));
		handle_list.num_entries += NUM_CL_HANDLES;
	}
exit_alloc_handle_lst:
	return ret;
}

static uint32_t gen_handle(struct msm_bus_client *client)
{
	uint32_t handle = 0;
	int i;
	int ret = 0;

	for (i = 0; i < handle_list.num_entries; i++) {
		if (i && !handle_list.cl_list[i]) {
			handle = i;
			break;
		}
	}

	if (!handle) {
		ret = alloc_handle_lst(NUM_CL_HANDLES);

		if (ret) {
			MSM_BUS_ERR("%s: Failed to allocate handle list",
							__func__);
			goto exit_gen_handle;
		}
		handle = i + 1;
	}
	handle_list.cl_list[handle] = client;
exit_gen_handle:
	return handle;
}

static uint32_t register_client_adhoc(struct msm_bus_scale_pdata *pdata)
{
	int src, dest;
	int i;
	struct msm_bus_client *client = NULL;
	int *lnode;
	struct device *dev;
	uint32_t handle = 0;

	rt_mutex_lock(&msm_bus_adhoc_lock);
	client = kzalloc(sizeof(struct msm_bus_client), GFP_KERNEL);
	if (!client) {
		MSM_BUS_ERR("%s: Error allocating client data", __func__);
		goto exit_register_client;
	}
	client->pdata = pdata;

	if (pdata->alc) {
		client->curr = -1;
		lnode = kzalloc(sizeof(int), GFP_KERNEL);

		if (ZERO_OR_NULL_PTR(lnode)) {
			MSM_BUS_ERR("%s: Error allocating lnode!", __func__);
			goto exit_lnode_malloc_fail;
		}
		client->src_pnode = lnode;

		client->src_devs = kzalloc(sizeof(struct device *),
							GFP_KERNEL);
		if (IS_ERR_OR_NULL(client->src_devs)) {
			MSM_BUS_ERR("%s: Error allocating src_dev!", __func__);
			goto exit_src_dev_malloc_fail;
		}
		src = MSM_BUS_MAS_ALC;
		dev = bus_find_device(&msm_bus_type, NULL,
				(void *) &src,
				msm_bus_device_match_adhoc);
		if (IS_ERR_OR_NULL(dev)) {
			MSM_BUS_ERR("%s:Failed to find alc device",
				__func__);
			goto exit_invalid_data;
		}
		gen_lnode(dev, MSM_BUS_MAS_ALC, 0, pdata->name);
		bcm_add_bus_req(dev);

		client->src_devs[0] = dev;

		handle = gen_handle(client);
		goto exit_register_client;
	}

	lnode = kcalloc(pdata->usecase->num_paths, sizeof(int), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(lnode)) {
		MSM_BUS_ERR("%s: Error allocating pathnode ptr!", __func__);
		goto exit_lnode_malloc_fail;
	}
	client->src_pnode = lnode;

	client->src_devs = kcalloc(pdata->usecase->num_paths,
					sizeof(struct device *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(client->src_devs)) {
		MSM_BUS_ERR("%s: Error allocating pathnode ptr!", __func__);
		goto exit_src_dev_malloc_fail;
	}
	client->curr = -1;

	for (i = 0; i < pdata->usecase->num_paths; i++) {
		src = pdata->usecase->vectors[i].src;
		dest = pdata->usecase->vectors[i].dst;

		if ((src < 0) || (dest < 0) || (src == dest)) {
			MSM_BUS_ERR("%s:Invalid src/dst.src %d dest %d",
				__func__, src, dest);
			goto exit_invalid_data;
		}
		dev = bus_find_device(&msm_bus_type, NULL,
				(void *) &src,
				msm_bus_device_match_adhoc);
		if (IS_ERR_OR_NULL(dev)) {
			MSM_BUS_ERR("%s:Failed to find path.src %d dest %d",
				__func__, src, dest);
			goto exit_invalid_data;
		}
		client->src_devs[i] = dev;

		MSM_BUS_ERR("%s:find path.src %d dest %d",
				__func__, src, dest);

		lnode[i] = getpath(dev, dest, client->pdata->name);
		if (lnode[i] < 0) {
			MSM_BUS_ERR("%s:Failed to find path.src %d dest %d",
				__func__, src, dest);
			goto exit_invalid_data;
		}
	}

	handle = gen_handle(client);
	msm_bus_dbg_client_data(client->pdata, MSM_BUS_DBG_REGISTER,
					handle);
	MSM_BUS_ERR("%s:Client handle %d %s", __func__, handle,
						client->pdata->name);
	rt_mutex_unlock(&msm_bus_adhoc_lock);
	return handle;
exit_invalid_data:
	kfree(client->src_devs);
exit_src_dev_malloc_fail:
	kfree(lnode);
exit_lnode_malloc_fail:
	kfree(client);
exit_register_client:
	rt_mutex_unlock(&msm_bus_adhoc_lock);
	return handle;
}

static int update_client_paths(struct msm_bus_client *client, bool log_trns,
							unsigned int idx)
{
	int lnode, src, dest, cur_idx;
	uint64_t req_clk, req_bw, curr_clk, curr_bw, slp_clk, slp_bw;
	int i, ret = 0;
	struct msm_bus_scale_pdata *pdata;
	struct device *src_dev;

	if (!client) {
		MSM_BUS_ERR("Client handle  Null");
		ret = -ENXIO;
		goto exit_update_client_paths;
	}

	pdata = client->pdata;
	if (!pdata) {
		MSM_BUS_ERR("Client pdata Null");
		ret = -ENXIO;
		goto exit_update_client_paths;
	}

	cur_idx = client->curr;
	client->curr = idx;
	for (i = 0; i < pdata->usecase->num_paths; i++) {
		src = pdata->usecase[idx].vectors[i].src;
		dest = pdata->usecase[idx].vectors[i].dst;

		lnode = client->src_pnode[i];
		src_dev = client->src_devs[i];
		req_clk = client->pdata->usecase[idx].vectors[i].ib;
		req_bw = client->pdata->usecase[idx].vectors[i].ab;
		if (cur_idx < 0) {
			curr_clk = 0;
			curr_bw = 0;
		} else {
			curr_clk =
				client->pdata->usecase[cur_idx].vectors[i].ib;
			curr_bw = client->pdata->usecase[cur_idx].vectors[i].ab;
			MSM_BUS_DBG("%s:ab: %llu ib: %llu\n", __func__,
					curr_bw, curr_clk);
		}

		if (pdata->active_only) {
			slp_clk = 0;
			slp_bw = 0;
		} else {
			slp_clk = req_clk;
			slp_bw = req_bw;
			req_clk = 0;
			req_bw = 0;
		}

		ret = update_path(src_dev, dest, req_clk, req_bw, slp_clk,
			slp_bw, curr_clk, curr_bw, lnode, pdata->active_only);

		if (ret) {
			MSM_BUS_ERR("%s: Update path failed! %d ctx %d\n",
					__func__, ret, pdata->active_only);
			goto exit_update_client_paths;
		}

		if (log_trns)
			getpath_debug(src, lnode, pdata->active_only);
	}
	commit_data();
exit_update_client_paths:
	return ret;
}

static int update_client_alc(struct msm_bus_client *client, bool log_trns,
							unsigned int idx)
{
	int lnode, cur_idx;
	uint64_t req_idle_time, req_fal, dual_idle_time, dual_fal,
	cur_idle_time, cur_fal;
	int ret = 0;
	struct msm_bus_scale_pdata *pdata;
	struct device *src_dev;

	if (!client) {
		MSM_BUS_ERR("Client handle  Null");
		ret = -ENXIO;
		goto exit_update_client_alc;
	}

	pdata = client->pdata;
	if (!pdata) {
		MSM_BUS_ERR("Client pdata Null");
		ret = -ENXIO;
		goto exit_update_client_alc;
	}

	cur_idx = client->curr;
	client->curr = idx;
	req_fal = pdata->usecase_lat[idx].fal_ns;
	req_idle_time = pdata->usecase_lat[idx].idle_t_ns;
	lnode = client->src_pnode[0];
	src_dev = client->src_devs[0];

	if (pdata->active_only) {
		dual_fal = 0;
		dual_idle_time = 0;
	} else {
		dual_fal = req_fal;
		dual_idle_time = req_idle_time;
	}

	ret = update_alc_vote(src_dev, req_fal, req_idle_time, dual_fal,
		dual_idle_time, cur_fal, cur_idle_time, lnode,
		pdata->active_only);

	if (ret) {
		MSM_BUS_ERR("%s: Update path failed! %d ctx %d\n",
				__func__, ret, pdata->active_only);
		goto exit_update_client_alc;
	}
	commit_data();
exit_update_client_alc:
	return ret;
}

static int query_usecase(struct msm_bus_client *client, bool log_trns,
					unsigned int idx,
					struct msm_bus_tcs_usecase *tcs_usecase)
{
	int lnode, src, dest, cur_idx;
	uint64_t req_clk, req_bw, curr_clk, curr_bw;
	int i, ret = 0;
	struct msm_bus_scale_pdata *pdata;
	struct device *src_dev;
	struct msm_bus_node_device_type *node = NULL;
	struct msm_bus_node_device_type *node_tmp = NULL;

	if (!client) {
		MSM_BUS_ERR("Client handle  Null");
		ret = -ENXIO;
		goto exit_query_usecase;
	}

	pdata = client->pdata;
	if (!pdata) {
		MSM_BUS_ERR("Client pdata Null");
		ret = -ENXIO;
		goto exit_query_usecase;
	}

	cur_idx = client->curr;
	client->curr = idx;
	for (i = 0; i < pdata->usecase->num_paths; i++) {
		src = pdata->usecase[idx].vectors[i].src;
		dest = pdata->usecase[idx].vectors[i].dst;

		lnode = client->src_pnode[i];
		src_dev = client->src_devs[i];
		req_clk = client->pdata->usecase[idx].vectors[i].ib;
		req_bw = client->pdata->usecase[idx].vectors[i].ab;
		if (cur_idx < 0) {
			curr_clk = 0;
			curr_bw = 0;
		} else {
			curr_clk =
				client->pdata->usecase[cur_idx].vectors[i].ib;
			curr_bw = client->pdata->usecase[cur_idx].vectors[i].ab;
			MSM_BUS_DBG("%s:ab: %llu ib: %llu\n", __func__,
					curr_bw, curr_clk);
		}

		ret = query_path(src_dev, dest, req_clk, req_bw, 0,
			0, curr_clk, curr_bw, lnode);

		if (ret) {
			MSM_BUS_ERR("%s: Query path failed! %d ctx %d\n",
					__func__, ret, pdata->active_only);
			goto exit_query_usecase;
		}
	}
	msm_bus_query_gen(&query_list, tcs_usecase);
	INIT_LIST_HEAD(&query_list);

	for (i = 0; i < pdata->usecase->num_paths; i++) {
		src = pdata->usecase[idx].vectors[i].src;
		dest = pdata->usecase[idx].vectors[i].dst;

		lnode = client->src_pnode[i];
		src_dev = client->src_devs[i];

		ret = query_path(src_dev, dest, 0, 0, 0, 0,
						curr_clk, curr_bw, lnode);

		if (ret) {
			MSM_BUS_ERR("%s: Clear query path failed! %d ctx %d\n",
					__func__, ret, pdata->active_only);
			goto exit_query_usecase;
		}
	}

	list_for_each_entry_safe(node, node_tmp, &query_list, query_link) {
		node->query_dirty = false;
		list_del_init(&node->query_link);
	}

	INIT_LIST_HEAD(&query_list);

exit_query_usecase:
	return ret;
}

static int update_context(uint32_t cl, bool active_only,
					unsigned int ctx_idx)
{
	int ret = 0;
	struct msm_bus_scale_pdata *pdata;
	struct msm_bus_client *client;

	rt_mutex_lock(&msm_bus_adhoc_lock);
	if (!cl) {
		MSM_BUS_ERR("%s: Invalid client handle %d", __func__, cl);
		ret = -ENXIO;
		goto exit_update_context;
	}

	client = handle_list.cl_list[cl];
	if (!client) {
		ret = -ENXIO;
		goto exit_update_context;
	}

	pdata = client->pdata;
	if (!pdata) {
		ret = -ENXIO;
		goto exit_update_context;
	}
	if (pdata->active_only == active_only) {
		MSM_BUS_ERR("No change in context(%d==%d), skip\n",
					pdata->active_only, active_only);
		ret = -ENXIO;
		goto exit_update_context;
	}

	if (ctx_idx >= pdata->num_usecases) {
		MSM_BUS_ERR("Client %u passed invalid index: %d\n",
			cl, ctx_idx);
		ret = -ENXIO;
		goto exit_update_context;
	}

	pdata->active_only = active_only;

	msm_bus_dbg_client_data(client->pdata, ctx_idx, cl);
	ret = update_client_paths(client, false, ctx_idx);
	if (ret) {
		pr_err("%s: Err updating path\n", __func__);
		goto exit_update_context;
	}

//	trace_bus_update_request_end(pdata->name);

exit_update_context:
	rt_mutex_unlock(&msm_bus_adhoc_lock);
	return ret;
}

static int update_request_adhoc(uint32_t cl, unsigned int index)
{
	int ret = 0;
	struct msm_bus_scale_pdata *pdata;
	struct msm_bus_client *client;
	const char *test_cl = "Null";
	bool log_transaction = false;

	rt_mutex_lock(&msm_bus_adhoc_lock);

	if (!cl) {
		MSM_BUS_ERR("%s: Invalid client handle %d", __func__, cl);
		ret = -ENXIO;
		goto exit_update_request;
	}

	client = handle_list.cl_list[cl];
	if (!client) {
		MSM_BUS_ERR("%s: Invalid client pointer ", __func__);
		ret = -ENXIO;
		goto exit_update_request;
	}

	pdata = client->pdata;
	if (!pdata) {
		MSM_BUS_ERR("%s: Client data Null.[client didn't register]",
				__func__);
		ret = -ENXIO;
		goto exit_update_request;
	}

	if (index >= pdata->num_usecases) {
		MSM_BUS_ERR("Client %u passed invalid index: %d\n",
			cl, index);
		ret = -ENXIO;
		goto exit_update_request;
	}

	if (client->curr == index) {
		MSM_BUS_DBG("%s: Not updating client request idx %d unchanged",
				__func__, index);
		goto exit_update_request;
	}

	if (!strcmp(test_cl, pdata->name))
		log_transaction = true;

	MSM_BUS_DBG("%s: cl: %u index: %d curr: %d num_paths: %d\n", __func__,
		cl, index, client->curr, client->pdata->usecase->num_paths);

	if (pdata->alc)
		ret = update_client_alc(client, log_transaction, index);
	else {
		msm_bus_dbg_client_data(client->pdata, index, cl);
		ret = update_client_paths(client, log_transaction, index);
	}
	if (ret) {
		pr_err("%s: Err updating path\n", __func__);
		goto exit_update_request;
	}

//	trace_bus_update_request_end(pdata->name);

exit_update_request:
	rt_mutex_unlock(&msm_bus_adhoc_lock);
	return ret;
}

static int query_client_usecase(struct msm_bus_tcs_usecase *tcs_usecase,
					uint32_t cl, unsigned int index)
{
	int ret = 0;
	struct msm_bus_scale_pdata *pdata;
	struct msm_bus_client *client;
	const char *test_cl = "Null";
	bool log_transaction = false;

	rt_mutex_lock(&msm_bus_adhoc_lock);

	if (!cl) {
		MSM_BUS_ERR("%s: Invalid client handle %d", __func__, cl);
		ret = -ENXIO;
		goto exit_query_client_usecase;
	}

	client = handle_list.cl_list[cl];
	if (!client) {
		MSM_BUS_ERR("%s: Invalid client pointer ", __func__);
		ret = -ENXIO;
		goto exit_query_client_usecase;
	}

	pdata = client->pdata;
	if (!pdata) {
		MSM_BUS_ERR("%s: Client data Null.[client didn't register]",
				__func__);
		ret = -ENXIO;
		goto exit_query_client_usecase;
	}

	if (index >= pdata->num_usecases) {
		MSM_BUS_ERR("Client %u passed invalid index: %d\n",
			cl, index);
		ret = -ENXIO;
		goto exit_query_client_usecase;
	}

	if (!strcmp(test_cl, pdata->name))
		log_transaction = true;

	MSM_BUS_DBG("%s: cl: %u index: %d curr: %d num_paths: %d\n", __func__,
		cl, index, client->curr, client->pdata->usecase->num_paths);
	ret = query_usecase(client, log_transaction, index, tcs_usecase);
	if (ret) {
		pr_err("%s: Err updating path\n", __func__);
		goto exit_query_client_usecase;
	}

//	trace_bus_update_request_end(pdata->name);

exit_query_client_usecase:
	rt_mutex_unlock(&msm_bus_adhoc_lock);
	return ret;
}

static int query_client_usecase_all(struct msm_bus_tcs_handle *tcs_handle,
					uint32_t cl)
{
	int ret = 0;
	struct msm_bus_scale_pdata *pdata;
	struct msm_bus_client *client;
	const char *test_cl = "Null";
	bool log_transaction = false;
	int i = 0;

	rt_mutex_lock(&msm_bus_adhoc_lock);

	if (!cl) {
		MSM_BUS_ERR("%s: Invalid client handle %d", __func__, cl);
		ret = -ENXIO;
		goto exit_query_client_usecase_all;
	}

	client = handle_list.cl_list[cl];
	if (!client) {
		MSM_BUS_ERR("%s: Invalid client pointer ", __func__);
		ret = -ENXIO;
		goto exit_query_client_usecase_all;
	}

	pdata = client->pdata;
	if (!pdata) {
		MSM_BUS_ERR("%s: Client data Null.[client didn't register]",
				__func__);
		ret = -ENXIO;
		goto exit_query_client_usecase_all;
	}

	if (!strcmp(test_cl, pdata->name))
		log_transaction = true;

	MSM_BUS_ERR("%s: query_start", __func__);
	for (i = 0; i < pdata->num_usecases; i++)
		query_usecase(client, log_transaction, i,
						&tcs_handle->usecases[i]);
	tcs_handle->num_usecases = pdata->num_usecases;

	if (ret) {
		pr_err("%s: Err updating path\n", __func__);
		goto exit_query_client_usecase_all;
	}

//	trace_bus_update_request_end(pdata->name);

exit_query_client_usecase_all:
	rt_mutex_unlock(&msm_bus_adhoc_lock);
	return ret;
}

static void free_cl_mem(struct msm_bus_client_handle *cl)
{
	if (cl) {
		kfree(cl->name);
		kfree(cl);
		cl = NULL;
	}
}

static int update_bw_adhoc(struct msm_bus_client_handle *cl, u64 ab, u64 ib)
{
	int ret = 0;
	char *test_cl = "test-client";
	bool log_transaction = false;
	u64 dual_ib, dual_ab, act_ib, act_ab;

	rt_mutex_lock(&msm_bus_adhoc_lock);

	if (!cl) {
		MSM_BUS_ERR("%s: Invalid client handle %p", __func__, cl);
		ret = -ENXIO;
		goto exit_update_request;
	}

	if (!strcmp(test_cl, cl->name))
		log_transaction = true;

	msm_bus_dbg_rec_transaction(cl, ab, ib);

	if (cl->active_only) {
		if ((cl->cur_act_ib == ib) && (cl->cur_act_ab == ab)) {
			MSM_BUS_DBG("%s:no change in request", cl->name);
			goto exit_update_request;
		}
		act_ib = ib;
		act_ab = ab;
		dual_ib = 0;
		dual_ab = 0;
	} else {
		if ((cl->cur_dual_ib == ib) && (cl->cur_dual_ab == ab)) {
			MSM_BUS_DBG("%s:no change in request", cl->name);
			goto exit_update_request;
		}
		dual_ib = ib;
		dual_ab = ab;
		act_ib = 0;
		act_ab = 0;
	}

	ret = update_path(cl->mas_dev, cl->slv, act_ib, act_ab, dual_ib,
		dual_ab, cl->cur_act_ib, cl->cur_act_ab, cl->first_hop,
							cl->active_only);

	if (ret) {
		MSM_BUS_ERR("%s: Update path failed! %d active_only %d\n",
				__func__, ret, cl->active_only);
		goto exit_update_request;
	}

	commit_data();
	cl->cur_act_ib = act_ib;
	cl->cur_act_ab = act_ab;
	cl->cur_dual_ib = dual_ib;
	cl->cur_dual_ab = dual_ab;

	if (log_transaction)
		getpath_debug(cl->mas, cl->first_hop, cl->active_only);
//	trace_bus_update_request_end(cl->name);
exit_update_request:
	rt_mutex_unlock(&msm_bus_adhoc_lock);

	return ret;
}

static int update_bw_context(struct msm_bus_client_handle *cl, u64 act_ab,
				u64 act_ib, u64 dual_ib, u64 dual_ab)
{
	int ret = 0;

	rt_mutex_lock(&msm_bus_adhoc_lock);
	if (!cl) {
		MSM_BUS_ERR("Invalid client handle %p", cl);
		ret = -ENXIO;
		goto exit_change_context;
	}

	if ((cl->cur_act_ib == act_ib) &&
		(cl->cur_act_ab == act_ab) &&
		(cl->cur_dual_ib == dual_ib) &&
		(cl->cur_dual_ab == dual_ab)) {
		MSM_BUS_ERR("No change in vote");
		goto exit_change_context;
	}

	if (!dual_ab && !dual_ib)
		cl->active_only = true;
	msm_bus_dbg_rec_transaction(cl, cl->cur_act_ab, cl->cur_dual_ib);
	ret = update_path(cl->mas_dev, cl->slv, act_ib, act_ab, dual_ib,
				dual_ab, cl->cur_act_ab, cl->cur_act_ab,
				cl->first_hop, cl->active_only);
	if (ret) {
		MSM_BUS_ERR("%s: Update path failed! %d active_only %d\n",
				__func__, ret, cl->active_only);
		goto exit_change_context;
	}
	commit_data();
	cl->cur_act_ib = act_ib;
	cl->cur_act_ab = act_ab;
	cl->cur_dual_ib = dual_ib;
	cl->cur_dual_ab = dual_ab;
//	trace_bus_update_request_end(cl->name);
exit_change_context:
	rt_mutex_unlock(&msm_bus_adhoc_lock);
	return ret;
}

static void unregister_adhoc(struct msm_bus_client_handle *cl)
{
	rt_mutex_lock(&msm_bus_adhoc_lock);
	if (!cl) {
		MSM_BUS_ERR("%s: Null cl handle passed unregister\n",
				__func__);
		goto exit_unregister_client;
	}

	MSM_BUS_DBG("%s: Unregistering client %p", __func__, cl);

	remove_path(cl->mas_dev, cl->slv, cl->cur_act_ib, cl->cur_act_ab,
				cl->first_hop, cl->active_only);
	commit_data();
	msm_bus_dbg_remove_client(cl);
	kfree(cl);
	MSM_BUS_DBG("%s: Unregistered client", __func__);
exit_unregister_client:
	rt_mutex_unlock(&msm_bus_adhoc_lock);
}

static struct msm_bus_client_handle*
register_adhoc(uint32_t mas, uint32_t slv, char *name, bool active_only)
{
	struct msm_bus_client_handle *client = NULL;
	int len = 0;

	rt_mutex_lock(&msm_bus_adhoc_lock);

	if (!(mas && slv && name)) {
		pr_err("%s: Error: src dst name num_paths are required",
								 __func__);
		goto exit_register;
	}

	client = kzalloc(sizeof(struct msm_bus_client_handle), GFP_KERNEL);
	if (!client) {
		MSM_BUS_ERR("%s: Error allocating client data", __func__);
		goto exit_register;
	}

	len = strnlen(name, MAX_STR_CL);
	client->name = kzalloc((len + 1), GFP_KERNEL);
	if (!client->name) {
		MSM_BUS_ERR("%s: Error allocating client name buf", __func__);
		free_cl_mem(client);
		goto exit_register;
	}
	strlcpy(client->name, name, MAX_STR_CL);
	client->active_only = active_only;

	client->mas = mas;
	client->slv = slv;

	client->mas_dev = bus_find_device(&msm_bus_type, NULL,
					(void *) &mas,
					msm_bus_device_match_adhoc);
	if (IS_ERR_OR_NULL(client->mas_dev)) {
		MSM_BUS_ERR("%s:Failed to find path.src %d dest %d",
			__func__, client->mas, client->slv);
		free_cl_mem(client);
		goto exit_register;
	}

	client->first_hop = getpath(client->mas_dev, client->slv, client->name);
	if (client->first_hop < 0) {
		MSM_BUS_ERR("%s:Failed to find path.src %d dest %d",
			__func__, client->mas, client->slv);
		free_cl_mem(client);
		goto exit_register;
	}

	MSM_BUS_DBG("%s:Client handle %p %s", __func__, client,
						client->name);
	msm_bus_dbg_add_client(client);
exit_register:
	rt_mutex_unlock(&msm_bus_adhoc_lock);
	return client;
}
/**
 *  msm_bus_arb_setops_adhoc() : Setup the bus arbitration ops
 *  @ arb_ops: pointer to the arb ops.
 */
void msm_bus_arb_setops_adhoc(struct msm_bus_arb_ops *arb_ops)
{
	arb_ops->register_client = register_client_adhoc;
	arb_ops->update_request = update_request_adhoc;
	arb_ops->unregister_client = unregister_client_adhoc;
	arb_ops->update_context = update_context;

	arb_ops->register_cl = register_adhoc;
	arb_ops->unregister = unregister_adhoc;
	arb_ops->update_bw = update_bw_adhoc;
	arb_ops->update_bw_context = update_bw_context;
	arb_ops->query_usecase = query_client_usecase;
	arb_ops->query_usecase_all = query_client_usecase_all;
}
