/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include "ipa_ut_framework.h"
#include "../ipa_v3/ipa_i.h"
#include <linux/netdevice.h>

struct ipa_test_hw_stats_ctx {
	u32 odu_prod_hdl;
	u32 odu_cons_hdl;
	u32 rt4_usb;
	u32 rt4_usb_cnt_id;
	u32 rt6_usb;
	u32 rt6_usb_cnt_id;
	u32 rt4_odu_cons;
	u32 rt4_odu_cnt_id;
	u32 rt6_odu_cons;
	u32 rt6_odu_cnt_id;
	u32 flt4_usb_cnt_id;
	u32 flt6_usb_cnt_id;
	u32 flt4_odu_cnt_id;
	u32 flt6_odu_cnt_id;
	atomic_t odu_pending;
};

static struct ipa_test_hw_stats_ctx *ctx;

static int ipa_test_hw_stats_suite_setup(void **ppriv)
{
	IPA_UT_DBG("Start Setup\n");

	if (!ctx)
		ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

	return 0;
}

static int ipa_test_hw_stats_suite_teardown(void *priv)
{
	IPA_UT_DBG("Start Teardown\n");

	return 0;
}

static void odu_prod_notify(void *priv, enum ipa_dp_evt_type evt,
	unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;

	switch (evt) {
	case IPA_RECEIVE:
		dev_kfree_skb_any(skb);
		break;
	case IPA_WRITE_DONE:
		atomic_dec(&ctx->odu_pending);
		dev_kfree_skb_any(skb);
		break;
	default:
		IPA_UT_ERR("unexpected evt %d\n", evt);
	}
}
static void odu_cons_notify(void *priv, enum ipa_dp_evt_type evt,
	unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	int ret;

	switch (evt) {
	case IPA_RECEIVE:
		if (atomic_read(&ctx->odu_pending) >= 64)
			msleep(20);
		atomic_inc(&ctx->odu_pending);
		skb_put(skb, 100);
		ret = ipa_tx_dp(IPA_CLIENT_ODU_PROD, skb, NULL);
		while (ret) {
			msleep(100);
			ret = ipa_tx_dp(IPA_CLIENT_ODU_PROD, skb, NULL);
		}
		break;
	case IPA_WRITE_DONE:
		dev_kfree_skb_any(skb);
		break;
	default:
		IPA_UT_ERR("unexpected evt %d\n", evt);
	}
}

static int ipa_test_hw_stats_configure(void *priv)
{
	struct ipa_sys_connect_params odu_prod_params;
	struct ipa_sys_connect_params odu_emb_cons_params;
	int res;

	/* first connect all additional pipe */
	memset(&odu_prod_params, 0, sizeof(odu_prod_params));
	memset(&odu_emb_cons_params, 0, sizeof(odu_emb_cons_params));

	odu_prod_params.client = IPA_CLIENT_ODU_PROD;
	odu_prod_params.desc_fifo_sz = 0x1000;
	odu_prod_params.priv = NULL;
	odu_prod_params.notify = odu_prod_notify;
	res = ipa_setup_sys_pipe(&odu_prod_params,
		&ctx->odu_prod_hdl);
	if (res) {
		IPA_UT_ERR("fail to setup sys pipe ODU_PROD %d\n", res);
		return res;
	}

	odu_emb_cons_params.client = IPA_CLIENT_ODU_EMB_CONS;
	odu_emb_cons_params.desc_fifo_sz = 0x1000;
	odu_emb_cons_params.priv = NULL;
	odu_emb_cons_params.notify = odu_cons_notify;
	res = ipa_setup_sys_pipe(&odu_emb_cons_params,
		&ctx->odu_cons_hdl);
	if (res) {
		IPA_UT_ERR("fail to setup sys pipe ODU_EMB_CONS %d\n", res);
		ipa_teardown_sys_pipe(ctx->odu_prod_hdl);
		return res;
	}

	IPA_UT_INFO("Configured. Please connect USB RNDIS now\n");

	return 0;
}

static int ipa_test_hw_stats_add_FnR(void *priv)
{
	struct ipa_ioc_add_rt_rule_v2 *rt_rule;
	struct ipa_ioc_add_flt_rule_v2 *flt_rule;
	struct ipa_ioc_get_rt_tbl rt_lookup;
	struct ipa_ioc_flt_rt_counter_alloc *counter = NULL;
	struct ipa_ioc_flt_rt_query *query;
	int pyld_size = 0;
	int ret;
	rt_rule = kzalloc(sizeof(*rt_rule), GFP_KERNEL);
	if (!rt_rule) {
		IPA_UT_DBG("no mem\n");
		return -ENOMEM;
	}
	rt_rule->rules = (uint64_t)kzalloc(1 *
		sizeof(struct ipa_rt_rule_add_v2), GFP_KERNEL);
	if (!rt_rule->rules) {
		IPA_UT_DBG("no mem\n");
		ret = -ENOMEM;
		goto free_rt;
	}

	flt_rule = kzalloc(sizeof(*flt_rule), GFP_KERNEL);
	if (!flt_rule) {
		IPA_UT_DBG("no mem\n");
		ret = -ENOMEM;
		goto free_rt;
	}
	flt_rule->rules = (uint64_t)kzalloc(1 *
		sizeof(struct ipa_flt_rule_add_v2), GFP_KERNEL);
	if (!flt_rule->rules) {
		IPA_UT_DBG("no mem\n");
		ret = -ENOMEM;
		goto free_flt;
	}

	counter = kzalloc(sizeof(struct ipa_ioc_flt_rt_counter_alloc),
					  GFP_KERNEL);
	if (!counter) {
		IPA_UT_DBG("no mem\n");
		ret = -ENOMEM;
		goto free_flt;
	}
	counter->hw_counter.num_counters = 8;
	counter->sw_counter.num_counters = 1;

	/* allocate counters */
	ret = ipa3_alloc_counter_id(counter);
	if (ret < 0) {
		IPA_UT_DBG("ipa3_alloc_counter_id fails\n");
		ret = -ENOMEM;
		goto free_counter;
	}

	/* initially clean all allocated counters */
	query = kzalloc(sizeof(struct ipa_ioc_flt_rt_query),
		GFP_KERNEL);
	if (!query) {
		IPA_UT_DBG("no mem\n");
		ret = -ENOMEM;
		goto free_counter;
	}
	query->start_id = counter->hw_counter.start_id;
	query->end_id = counter->hw_counter.start_id +
		counter->hw_counter.num_counters - 1;
	query->reset = true;
	query->stats_size = sizeof(struct ipa_flt_rt_stats);
	pyld_size = IPA_MAX_FLT_RT_CNT_INDEX *
		sizeof(struct ipa_flt_rt_stats);
	query->stats = (uint64_t)kzalloc(pyld_size, GFP_KERNEL);
	if (!query->stats) {
		IPA_UT_DBG("no mem\n");
		ret = -ENOMEM;
		goto free_query;
	}
	ipa_get_flt_rt_stats(query);

	query->start_id = counter->sw_counter.start_id;
	query->end_id = counter->sw_counter.start_id +
		counter->sw_counter.num_counters - 1;
	query->reset = true;
	query->stats_size = sizeof(struct ipa_flt_rt_stats);
	ipa_get_flt_rt_stats(query);

	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v4;
	rt_lookup.ip = rt_rule->ip;
	strlcpy(rt_rule->rt_tbl_name, "V4_RT_TO_USB_CONS",
		IPA_RESOURCE_NAME_MAX);
	strlcpy(rt_lookup.name, rt_rule->rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	rt_rule->num_rules = 1;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.dst = IPA_CLIENT_USB_CONS;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.attrib.dst_port = 5002;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.hashable = true;
	ctx->rt4_usb_cnt_id = counter->hw_counter.start_id;
	IPA_UT_INFO("rt4_usb_cnt_id %u\n", ctx->rt4_usb_cnt_id);
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.cnt_idx = ctx->rt4_usb_cnt_id;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.enable_stats = true;
	if (ipa_add_rt_rule_v2(rt_rule) || ((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}
	if (ipa_get_rt_tbl(&rt_lookup)) {
		IPA_UT_ERR("failed to query V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}
	ctx->rt4_usb = rt_lookup.hdl;

	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v6;
	rt_lookup.ip = rt_rule->ip;
	strlcpy(rt_rule->rt_tbl_name, "V6_RT_TO_USB_CONS",
		IPA_RESOURCE_NAME_MAX);
	strlcpy(rt_lookup.name, rt_rule->rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	rt_rule->num_rules = 1;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.dst = IPA_CLIENT_USB_CONS;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.attrib.dst_port = 5002;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.hashable = true;
	ctx->rt6_usb_cnt_id = counter->hw_counter.start_id + 1;
	IPA_UT_INFO("rt6_usb_cnt_id %u\n", ctx->rt6_usb_cnt_id);
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.cnt_idx = ctx->rt6_usb_cnt_id;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.enable_stats = true;
	if (ipa_add_rt_rule_v2(rt_rule) || ((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}
	if (ipa_get_rt_tbl(&rt_lookup)) {
		IPA_UT_ERR("failed to query V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}
	ctx->rt6_usb = rt_lookup.hdl;

	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v4;
	rt_lookup.ip = rt_rule->ip;
	strlcpy(rt_rule->rt_tbl_name, "V4_RT_TO_ODU_CONS",
		IPA_RESOURCE_NAME_MAX);
	strlcpy(rt_lookup.name, rt_rule->rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	rt_rule->num_rules = 1;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.dst = IPA_CLIENT_ODU_EMB_CONS;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.attrib.dst_port = 5002;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.hashable = true;
	ctx->rt4_odu_cnt_id = counter->hw_counter.start_id + 2;
	IPA_UT_INFO("rt4_odu_cnt_id %u\n", ctx->rt4_odu_cnt_id);
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.cnt_idx = ctx->rt4_odu_cnt_id;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.enable_stats = true;
	if (ipa_add_rt_rule_v2(rt_rule) || ((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}
	if (ipa_get_rt_tbl(&rt_lookup)) {
		IPA_UT_ERR("failed to query V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}
	ctx->rt4_odu_cons = rt_lookup.hdl;

	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v6;
	rt_lookup.ip = rt_rule->ip;
	strlcpy(rt_rule->rt_tbl_name, "V6_RT_TO_ODU_CONS",
		IPA_RESOURCE_NAME_MAX);
	strlcpy(rt_lookup.name, rt_rule->rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	rt_rule->num_rules = 1;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.dst = IPA_CLIENT_ODU_EMB_CONS;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.attrib.dst_port = 5002;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.hashable = true;
	ctx->rt6_odu_cnt_id = counter->hw_counter.start_id + 3;
	IPA_UT_INFO("rt6_odu_cnt_id %u\n", ctx->rt6_odu_cnt_id);
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.cnt_idx = ctx->rt6_odu_cnt_id;
	((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].rule.enable_stats = true;
	if (ipa_add_rt_rule_v2(rt_rule) || ((struct ipa_rt_rule_add_v2 *)
	rt_rule->rules)[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}
	if (ipa_get_rt_tbl(&rt_lookup)) {
		IPA_UT_ERR("failed to query V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}
	ctx->rt6_odu_cons = rt_lookup.hdl;

	flt_rule->commit = 1;
	flt_rule->ip = IPA_IP_v4;
	flt_rule->ep = IPA_CLIENT_USB_PROD;
	flt_rule->num_rules = 1;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].at_rear = 0;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.action = IPA_PASS_TO_ROUTING;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.attrib.dst_port = 5002;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.rt_tbl_hdl = ctx->rt4_odu_cons;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.hashable = 1;
	ctx->flt4_usb_cnt_id = counter->hw_counter.start_id + 4;
	IPA_UT_INFO("flt4_usb_cnt_id %u\n", ctx->flt4_usb_cnt_id);
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.cnt_idx = ctx->flt4_usb_cnt_id;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.enable_stats = true;
	if (ipa_add_flt_rule_v2(flt_rule) || ((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}

	flt_rule->commit = 1;
	flt_rule->ip = IPA_IP_v6;
	flt_rule->ep = IPA_CLIENT_USB_PROD;
	flt_rule->num_rules = 1;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].at_rear = 0;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.action = IPA_PASS_TO_ROUTING;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.attrib.dst_port = 5002;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.rt_tbl_hdl = ctx->rt6_odu_cons;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.hashable = 1;
	ctx->flt6_usb_cnt_id = counter->hw_counter.start_id + 5;
	IPA_UT_INFO("flt6_usb_cnt_id %u\n", ctx->flt6_usb_cnt_id);
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.cnt_idx = ctx->flt6_usb_cnt_id;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.enable_stats = true;
	if (ipa_add_flt_rule_v2(flt_rule) || ((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].status) {
		IPA_UT_ERR("failed to install V6 rules\n");
		ret = -EFAULT;
		goto free_query;
	}

	flt_rule->commit = 1;
	flt_rule->ip = IPA_IP_v4;
	flt_rule->ep = IPA_CLIENT_ODU_PROD;
	flt_rule->num_rules = 1;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].at_rear = 0;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.action = IPA_PASS_TO_ROUTING;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.attrib.dst_port = 5002;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.rt_tbl_hdl = ctx->rt4_usb;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.hashable = 1;
	ctx->flt4_odu_cnt_id = counter->hw_counter.start_id + 6;
	IPA_UT_INFO("flt4_odu_cnt_id %u\n", ctx->flt4_odu_cnt_id);
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.cnt_idx = ctx->flt4_odu_cnt_id;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.enable_stats = true;
	if (ipa_add_flt_rule_v2(flt_rule) || ((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_query;
	}

	flt_rule->commit = 1;
	flt_rule->ip = IPA_IP_v6;
	flt_rule->ep = IPA_CLIENT_ODU_PROD;
	flt_rule->num_rules = 1;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].at_rear = 0;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.action = IPA_PASS_TO_ROUTING;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.attrib.dst_port = 5002;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.rt_tbl_hdl = ctx->rt6_usb;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.hashable = 1;
	ctx->flt6_odu_cnt_id = counter->hw_counter.start_id + 7;
	IPA_UT_INFO("flt4_odu_cnt_id %u\n", ctx->flt6_odu_cnt_id);
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.cnt_idx = ctx->flt6_odu_cnt_id;
	((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].rule.enable_stats = true;
	if (ipa_add_flt_rule_v2(flt_rule) || ((struct ipa_flt_rule_add_v2 *)
	flt_rule->rules)[0].status) {
		IPA_UT_ERR("failed to install V6 rules\n");
		ret = -EFAULT;
		goto free_query;
	}
	IPA_UT_INFO(
		"Rules added. Please start data transfer on ports 5001/5002\n");
	ret = 0;

free_query:
	if (query->stats)
		kfree((void *)(query->stats));
	kfree(query);
free_counter:
	kfree(counter);
free_flt:
	if (flt_rule->rules)
		kfree((void *)(flt_rule->rules));
	kfree(flt_rule);
free_rt:
	if (rt_rule->rules)
		kfree((void *)(rt_rule->rules));
	kfree(rt_rule);
	return ret;
}

static int ipa_test_hw_stats_query_FnR_one_by_one(void *priv)
{
	int ret;
	struct ipa_ioc_flt_rt_query *query;
	int pyld_size = 0;

	query = kzalloc(sizeof(struct ipa_ioc_flt_rt_query), GFP_KERNEL);
	if (!query) {
		IPA_UT_DBG("no mem\n");
		return -ENOMEM;
	}
	pyld_size = IPA_MAX_FLT_RT_CNT_INDEX *
		sizeof(struct ipa_flt_rt_stats);
	query->stats = (uint64_t)kzalloc(pyld_size, GFP_KERNEL);
	if (!query->stats) {
		kfree(query);
		return -ENOMEM;
	}
	/* query 1 by 1 */
	IPA_UT_INFO("========query 1 by 1========\n");
	query->start_id = ctx->rt4_usb_cnt_id;
	query->end_id = ctx->rt4_usb_cnt_id;
	ipa_get_flt_rt_stats(query);
	IPA_UT_INFO(
		"usb v4 route counter %u pkt_cnt %u bytes cnt %llu\n",
		ctx->rt4_usb_cnt_id, ((struct ipa_flt_rt_stats *)
		query->stats)[0].num_pkts,
		((struct ipa_flt_rt_stats *)
		query->stats)[0].num_bytes);

	query->start_id = ctx->rt6_usb_cnt_id;
	query->end_id = ctx->rt6_usb_cnt_id;
	ipa_get_flt_rt_stats(query);
	IPA_UT_INFO(
		"usb v6 route counter %u pkt_cnt %u bytes cnt %llu\n",
		ctx->rt6_usb_cnt_id, ((struct ipa_flt_rt_stats *)
		query->stats)[0].num_pkts,
		((struct ipa_flt_rt_stats *)
		query->stats)[0].num_bytes);

	query->start_id = ctx->rt4_odu_cnt_id;
	query->end_id = ctx->rt4_odu_cnt_id;
	ipa_get_flt_rt_stats(query);
	IPA_UT_INFO(
		"odu v4 route counter %u pkt_cnt %u bytes cnt %llu\n",
		ctx->rt4_odu_cnt_id, ((struct ipa_flt_rt_stats *)
		query->stats)[0].num_pkts,
		((struct ipa_flt_rt_stats *)
		query->stats)[0].num_bytes);

	query->start_id = ctx->rt6_odu_cnt_id;
	query->end_id = ctx->rt6_odu_cnt_id;
	ipa_get_flt_rt_stats(query);
	IPA_UT_INFO(
		"odu v6 route counter %u pkt_cnt %u bytes cnt %llu\n",
		ctx->rt6_odu_cnt_id, ((struct ipa_flt_rt_stats *)
		query->stats)[0].num_pkts,
		((struct ipa_flt_rt_stats *)
		query->stats)[0].num_bytes);

	query->start_id = ctx->flt4_usb_cnt_id;
	query->end_id = ctx->flt4_usb_cnt_id;
	ipa_get_flt_rt_stats(query);
	IPA_UT_INFO(
		"usb v4 filter counter %u pkt_cnt %u bytes cnt %llu\n",
		ctx->flt4_usb_cnt_id, ((struct ipa_flt_rt_stats *)
		query->stats)[0].num_pkts,
		((struct ipa_flt_rt_stats *)
		query->stats)[0].num_bytes);

	query->start_id = ctx->flt6_usb_cnt_id;
	query->end_id = ctx->flt6_usb_cnt_id;
	ipa_get_flt_rt_stats(query);
	IPA_UT_INFO(
		"usb v6 filter counter %u pkt_cnt %u bytes cnt %llu\n",
		ctx->flt6_usb_cnt_id, ((struct ipa_flt_rt_stats *)
		query->stats)[0].num_pkts,
		((struct ipa_flt_rt_stats *)
		query->stats)[0].num_bytes);

	query->start_id = ctx->flt4_odu_cnt_id;
	query->end_id = ctx->flt4_odu_cnt_id;
	ipa_get_flt_rt_stats(query);
	IPA_UT_INFO(
		"odu v4 filter counter %u pkt_cnt %u bytes cnt %llu\n",
		ctx->flt4_odu_cnt_id, ((struct ipa_flt_rt_stats *)
		query->stats)[0].num_pkts,
		((struct ipa_flt_rt_stats *)
		query->stats)[0].num_bytes);

	query->start_id = ctx->flt6_odu_cnt_id;
	query->end_id = ctx->flt6_odu_cnt_id;
	ipa_get_flt_rt_stats(query);
	IPA_UT_INFO(
		"odu v6 filter counter %u pkt_cnt %u bytes cnt %llu\n",
		ctx->flt6_odu_cnt_id, ((struct ipa_flt_rt_stats *)
		query->stats)[0].num_pkts,
		((struct ipa_flt_rt_stats *)
		query->stats)[0].num_bytes);
	IPA_UT_INFO("================ done ============\n");

	ret = 0;
	kfree(query);
	return ret;
}

static int ipa_test_hw_stats_query_FnR_one_shot(void *priv)
{
	int ret, i, start = 0;
	struct ipa_ioc_flt_rt_query *query;
	int pyld_size = 0;

	query = kzalloc(sizeof(struct ipa_ioc_flt_rt_query), GFP_KERNEL);
	if (!query) {
		IPA_UT_DBG("no mem\n");
		return -ENOMEM;
	}
	pyld_size = IPA_MAX_FLT_RT_CNT_INDEX *
		sizeof(struct ipa_flt_rt_stats);
	query->stats = (uint64_t)kzalloc(pyld_size, GFP_KERNEL);
	if (!query->stats) {
		kfree(query);
		return -ENOMEM;
	}

	/* query all together */
	IPA_UT_INFO("========query all together========\n");
	query->start_id = ctx->rt4_usb_cnt_id;
	query->end_id = ctx->flt6_odu_cnt_id;
	ipa_get_flt_rt_stats(query);
	start = 0;
	for (i = ctx->rt4_usb_cnt_id;
		i <= ctx->flt6_odu_cnt_id; i++) {
		IPA_UT_INFO(
			"counter %u pkt_cnt %u bytes cnt %llu\n",
			i, ((struct ipa_flt_rt_stats *)
			query->stats)[start].num_pkts,
			((struct ipa_flt_rt_stats *)
			query->stats)[start].num_bytes);
		start++;
	}
	IPA_UT_INFO("================ done ============\n");

	ret = 0;
	kfree((void *)(query->stats));
	kfree(query);
	return ret;
}

static int ipa_test_hw_stats_query_FnR_clean(void *priv)
{
	int ret, i, start = 0;
	struct ipa_ioc_flt_rt_query *query;
	int pyld_size = 0;

	query = kzalloc(sizeof(struct ipa_ioc_flt_rt_query), GFP_KERNEL);
	if (!query) {
		IPA_UT_DBG("no mem\n");
		return -ENOMEM;
	}
	pyld_size = IPA_MAX_FLT_RT_CNT_INDEX *
		sizeof(struct ipa_flt_rt_stats);
	query->stats = (uint64_t)kzalloc(pyld_size, GFP_KERNEL);
	if (!query->stats) {
		kfree(query);
		return -ENOMEM;
	}

	/* query and reset */
	IPA_UT_INFO("========query and reset========\n");
	query->start_id = ctx->rt4_usb_cnt_id;
	query->reset = true;
	query->end_id = ctx->flt6_odu_cnt_id;
	start = 0;
	ipa_get_flt_rt_stats(query);
	for (i = ctx->rt4_usb_cnt_id;
		i <= ctx->flt6_odu_cnt_id; i++) {
		IPA_UT_INFO(
			"counter %u pkt_cnt %u bytes cnt %llu\n",
			i, ((struct ipa_flt_rt_stats *)
			query->stats)[start].num_pkts,
			((struct ipa_flt_rt_stats *)
			query->stats)[start].num_bytes);
		start++;
	}
	IPA_UT_INFO("================ done ============\n");

	ret = 0;
	kfree((void *)(query->stats));
	kfree(query);
	return ret;
}

/* Suite definition block */
IPA_UT_DEFINE_SUITE_START(hw_stats, "HW stats test",
	ipa_test_hw_stats_suite_setup, ipa_test_hw_stats_suite_teardown)
{
	IPA_UT_ADD_TEST(configure, "Configure the setup",
		ipa_test_hw_stats_configure, false, IPA_HW_v4_0, IPA_HW_MAX),

	IPA_UT_ADD_TEST(add_rules, "Add FLT and RT rules",
		ipa_test_hw_stats_add_FnR, false, IPA_HW_v4_5, IPA_HW_MAX),

	IPA_UT_ADD_TEST(query_stats_one_by_one, "Query one by one",
		ipa_test_hw_stats_query_FnR_one_by_one, false,
		IPA_HW_v4_5, IPA_HW_MAX),

	IPA_UT_ADD_TEST(query_stats_one_shot, "Query one shot",
		ipa_test_hw_stats_query_FnR_one_shot, false,
		IPA_HW_v4_5, IPA_HW_MAX),

	IPA_UT_ADD_TEST(query_stats_one_shot_clean, "Query and clean",
		ipa_test_hw_stats_query_FnR_clean, false,
		IPA_HW_v4_5, IPA_HW_MAX),

} IPA_UT_DEFINE_SUITE_END(hw_stats);
