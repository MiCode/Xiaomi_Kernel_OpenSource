/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/netdevice.h>

struct ipa_test_hw_stats_ctx {
	u32 odu_prod_hdl;
	u32 odu_cons_hdl;
	u32 rt4_usb;
	u32 rt6_usb;
	u32 rt4_odu_cons;
	u32 rt6_odu_cons;
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
	struct ipa_ioc_add_rt_rule *rt_rule;
	struct ipa_ioc_add_flt_rule *flt_rule;
	struct ipa_ioc_get_rt_tbl rt_lookup;
	int ret;

	rt_rule = kzalloc(sizeof(*rt_rule) + 1 * sizeof(struct ipa_rt_rule_add),
		GFP_KERNEL);
	if (!rt_rule) {
		IPA_UT_DBG("no mem\n");
		return -ENOMEM;
	}

	flt_rule = kzalloc(sizeof(*flt_rule) +
		1 * sizeof(struct ipa_flt_rule_add), GFP_KERNEL);
	if (!flt_rule) {
		IPA_UT_DBG("no mem\n");
		ret = -ENOMEM;
		goto free_rt;
	}

	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v4;
	rt_lookup.ip = rt_rule->ip;
	strlcpy(rt_rule->rt_tbl_name, "V4_RT_TO_USB_CONS",
		IPA_RESOURCE_NAME_MAX);
	strlcpy(rt_lookup.name, rt_rule->rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	rt_rule->num_rules = 1;
	rt_rule->rules[0].rule.dst = IPA_CLIENT_USB_CONS;
	rt_rule->rules[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	rt_rule->rules[0].rule.attrib.dst_port = 5002;
	rt_rule->rules[0].rule.hashable = true;
	if (ipa_add_rt_rule(rt_rule) || rt_rule->rules[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}
	if (ipa_get_rt_tbl(&rt_lookup)) {
		IPA_UT_ERR("failed to query V4 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}
	ctx->rt4_usb = rt_lookup.hdl;

	memset(rt_rule, 0, sizeof(*rt_rule));
	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v6;
	rt_lookup.ip = rt_rule->ip;
	strlcpy(rt_rule->rt_tbl_name, "V6_RT_TO_USB_CONS",
		IPA_RESOURCE_NAME_MAX);
	strlcpy(rt_lookup.name, rt_rule->rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	rt_rule->num_rules = 1;
	rt_rule->rules[0].rule.dst = IPA_CLIENT_USB_CONS;
	rt_rule->rules[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	rt_rule->rules[0].rule.attrib.dst_port = 5002;
	rt_rule->rules[0].rule.hashable = true;
	if (ipa_add_rt_rule(rt_rule) || rt_rule->rules[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}
	if (ipa_get_rt_tbl(&rt_lookup)) {
		IPA_UT_ERR("failed to query V4 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}
	ctx->rt6_usb = rt_lookup.hdl;

	memset(rt_rule, 0, sizeof(*rt_rule));
	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v4;
	rt_lookup.ip = rt_rule->ip;
	strlcpy(rt_rule->rt_tbl_name, "V4_RT_TO_ODU_CONS",
		IPA_RESOURCE_NAME_MAX);
	strlcpy(rt_lookup.name, rt_rule->rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	rt_rule->num_rules = 1;
	rt_rule->rules[0].rule.dst = IPA_CLIENT_ODU_EMB_CONS;
	rt_rule->rules[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	rt_rule->rules[0].rule.attrib.dst_port = 5002;
	rt_rule->rules[0].rule.hashable = true;
	if (ipa_add_rt_rule(rt_rule) || rt_rule->rules[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}
	if (ipa_get_rt_tbl(&rt_lookup)) {
		IPA_UT_ERR("failed to query V4 rules\n");
		return -EFAULT;
	}
	ctx->rt4_odu_cons = rt_lookup.hdl;

	memset(rt_rule, 0, sizeof(*rt_rule));
	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v6;
	rt_lookup.ip = rt_rule->ip;
	strlcpy(rt_rule->rt_tbl_name, "V6_RT_TO_ODU_CONS",
		IPA_RESOURCE_NAME_MAX);
	strlcpy(rt_lookup.name, rt_rule->rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	rt_rule->num_rules = 1;
	rt_rule->rules[0].rule.dst = IPA_CLIENT_ODU_EMB_CONS;
	rt_rule->rules[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	rt_rule->rules[0].rule.attrib.dst_port = 5002;
	rt_rule->rules[0].rule.hashable = true;
	if (ipa_add_rt_rule(rt_rule) || rt_rule->rules[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}
	if (ipa_get_rt_tbl(&rt_lookup)) {
		IPA_UT_ERR("failed to query V4 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}
	ctx->rt6_odu_cons = rt_lookup.hdl;

	flt_rule->commit = 1;
	flt_rule->ip = IPA_IP_v4;
	flt_rule->ep = IPA_CLIENT_USB_PROD;
	flt_rule->num_rules = 1;
	flt_rule->rules[0].at_rear = 1;
	flt_rule->rules[0].rule.action = IPA_PASS_TO_ROUTING;
	flt_rule->rules[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	flt_rule->rules[0].rule.attrib.dst_port = 5002;
	flt_rule->rules[0].rule.rt_tbl_hdl = ctx->rt4_odu_cons;
	flt_rule->rules[0].rule.hashable = 1;
	if (ipa_add_flt_rule(flt_rule) || flt_rule->rules[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}

	memset(flt_rule, 0, sizeof(*flt_rule));
	flt_rule->commit = 1;
	flt_rule->ip = IPA_IP_v6;
	flt_rule->ep = IPA_CLIENT_USB_PROD;
	flt_rule->num_rules = 1;
	flt_rule->rules[0].at_rear = 1;
	flt_rule->rules[0].rule.action = IPA_PASS_TO_ROUTING;
	flt_rule->rules[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	flt_rule->rules[0].rule.attrib.dst_port = 5002;
	flt_rule->rules[0].rule.rt_tbl_hdl = ctx->rt6_odu_cons;
	flt_rule->rules[0].rule.hashable = 1;
	if (ipa_add_flt_rule(flt_rule) || flt_rule->rules[0].status) {
		IPA_UT_ERR("failed to install V6 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}

	memset(flt_rule, 0, sizeof(*flt_rule));
	flt_rule->commit = 1;
	flt_rule->ip = IPA_IP_v4;
	flt_rule->ep = IPA_CLIENT_ODU_PROD;
	flt_rule->num_rules = 1;
	flt_rule->rules[0].at_rear = 1;
	flt_rule->rules[0].rule.action = IPA_PASS_TO_ROUTING;
	flt_rule->rules[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	flt_rule->rules[0].rule.attrib.dst_port = 5002;
	flt_rule->rules[0].rule.rt_tbl_hdl = ctx->rt4_usb;
	flt_rule->rules[0].rule.hashable = 1;
	if (ipa_add_flt_rule(flt_rule) || flt_rule->rules[0].status) {
		IPA_UT_ERR("failed to install V4 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}

	memset(flt_rule, 0, sizeof(*flt_rule));
	flt_rule->commit = 1;
	flt_rule->ip = IPA_IP_v6;
	flt_rule->ep = IPA_CLIENT_ODU_PROD;
	flt_rule->num_rules = 1;
	flt_rule->rules[0].at_rear = 1;
	flt_rule->rules[0].rule.action = IPA_PASS_TO_ROUTING;
	flt_rule->rules[0].rule.attrib.attrib_mask = IPA_FLT_DST_PORT;
	flt_rule->rules[0].rule.attrib.dst_port = 5002;
	flt_rule->rules[0].rule.rt_tbl_hdl = ctx->rt6_usb;
	flt_rule->rules[0].rule.hashable = 1;
	if (ipa_add_flt_rule(flt_rule) || flt_rule->rules[0].status) {
		IPA_UT_ERR("failed to install V6 rules\n");
		ret = -EFAULT;
		goto free_flt;
	}

	IPA_UT_INFO(
		"Rules added. Please start data transfer on ports 5001/5002\n");
	ret = 0;
free_flt:
	kfree(flt_rule);
free_rt:
	kfree(rt_rule);
	return ret;

}

/* Suite definition block */
IPA_UT_DEFINE_SUITE_START(hw_stats, "HW stats test",
	ipa_test_hw_stats_suite_setup, ipa_test_hw_stats_suite_teardown)
{
	IPA_UT_ADD_TEST(configure, "Configure the setup",
		ipa_test_hw_stats_configure, false, IPA_HW_v4_0, IPA_HW_MAX),

	IPA_UT_ADD_TEST(add_rules, "Add FLT and RT rules",
		ipa_test_hw_stats_add_FnR, false, IPA_HW_v4_0, IPA_HW_MAX),

} IPA_UT_DEFINE_SUITE_END(hw_stats);
