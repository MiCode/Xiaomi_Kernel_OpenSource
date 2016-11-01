/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

/*
 * WWAN Transport Network Driver.
 */

#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <net/pkt_sched.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include "ipa_qmi_service.h"
#include <linux/rmnet_ipa_fd_ioctl.h>
#include <linux/ipa.h>
#include <uapi/linux/net_map.h>

#include "ipa_trace.h"

#define WWAN_METADATA_SHFT 24
#define WWAN_METADATA_MASK 0xFF000000
#define WWAN_DATA_LEN 2000
#define IPA_RM_INACTIVITY_TIMER 100 /* IPA_RM */
#define HEADROOM_FOR_QMAP   8 /* for mux header */
#define TAILROOM            0 /* for padding by mux layer */
#define MAX_NUM_OF_MUX_CHANNEL  10 /* max mux channels */
#define UL_FILTER_RULE_HANDLE_START 69
#define DEFAULT_OUTSTANDING_HIGH_CTL 96
#define DEFAULT_OUTSTANDING_HIGH 64
#define DEFAULT_OUTSTANDING_LOW 32

#define IPA_WWAN_DEV_NAME "rmnet_ipa%d"

#define IPA_WWAN_RX_SOFTIRQ_THRESH 16

#define INVALID_MUX_ID 0xFF
#define IPA_QUOTA_REACH_ALERT_MAX_SIZE 64
#define IPA_QUOTA_REACH_IF_NAME_MAX_SIZE 64
#define IPA_UEVENT_NUM_EVNP 4 /* number of event pointers */
#define NAPI_WEIGHT 60

#define IPA_NETDEV() \
	((rmnet_ipa3_ctx && rmnet_ipa3_ctx->wwan_priv) ? \
	  rmnet_ipa3_ctx->wwan_priv->net : NULL)


static int ipa3_wwan_add_ul_flt_rule_to_ipa(void);
static int ipa3_wwan_del_ul_flt_rule_to_ipa(void);
static void ipa3_wwan_msg_free_cb(void*, u32, u32);
static void ipa3_rmnet_rx_cb(void *priv);
static int ipa3_rmnet_poll(struct napi_struct *napi, int budget);

static void ipa3_wake_tx_queue(struct work_struct *work);
static DECLARE_WORK(ipa3_tx_wakequeue_work, ipa3_wake_tx_queue);

static void tethering_stats_poll_queue(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa_tether_stats_poll_wakequeue_work,
			    tethering_stats_poll_queue);

enum ipa3_wwan_device_status {
	WWAN_DEVICE_INACTIVE = 0,
	WWAN_DEVICE_ACTIVE   = 1
};

struct ipa3_rmnet_plat_drv_res {
	bool ipa_rmnet_ssr;
	bool ipa_loaduC;
	bool ipa_advertise_sg_support;
	bool ipa_napi_enable;
};

/**
 * struct ipa3_wwan_private - WWAN private data
 * @net: network interface struct implemented by this driver
 * @stats: iface statistics
 * @outstanding_pkts: number of packets sent to IPA without TX complete ACKed
 * @outstanding_high: number of outstanding packets allowed
 * @outstanding_low: number of outstanding packets which shall cause
 * @ch_id: channel id
 * @lock: spinlock for mutual exclusion
 * @device_status: holds device status
 *
 * WWAN private - holds all relevant info about WWAN driver
 */
struct ipa3_wwan_private {
	struct net_device *net;
	struct net_device_stats stats;
	atomic_t outstanding_pkts;
	int outstanding_high_ctl;
	int outstanding_high;
	int outstanding_low;
	uint32_t ch_id;
	spinlock_t lock;
	struct completion resource_granted_completion;
	enum ipa3_wwan_device_status device_status;
	struct napi_struct napi;
};

struct rmnet_ipa3_context {
	struct ipa3_wwan_private *wwan_priv;
	struct ipa_sys_connect_params apps_to_ipa_ep_cfg;
	struct ipa_sys_connect_params ipa_to_apps_ep_cfg;
	u32 qmap_hdr_hdl;
	u32 dflt_v4_wan_rt_hdl;
	u32 dflt_v6_wan_rt_hdl;
	struct ipa3_rmnet_mux_val mux_channel[MAX_NUM_OF_MUX_CHANNEL];
	int num_q6_rules;
	int old_num_q6_rules;
	int rmnet_index;
	bool egress_set;
	bool a7_ul_flt_set;
	struct workqueue_struct *rm_q6_wq;
	atomic_t is_initialized;
	atomic_t is_ssr;
	void *subsys_notify_handle;
	u32 apps_to_ipa3_hdl;
	u32 ipa3_to_apps_hdl;
	struct mutex ipa_to_apps_pipe_handle_guard;
};

static struct rmnet_ipa3_context *rmnet_ipa3_ctx;
static struct ipa3_rmnet_plat_drv_res ipa3_rmnet_res;

/**
* ipa3_setup_a7_qmap_hdr() - Setup default a7 qmap hdr
*
* Return codes:
* 0: success
* -ENOMEM: failed to allocate memory
* -EPERM: failed to add the tables
*/
static int ipa3_setup_a7_qmap_hdr(void)
{
	struct ipa_ioc_add_hdr *hdr;
	struct ipa_hdr_add *hdr_entry;
	u32 pyld_sz;
	int ret;

	/* install the basic exception header */
	pyld_sz = sizeof(struct ipa_ioc_add_hdr) + 1 *
		      sizeof(struct ipa_hdr_add);
	hdr = kzalloc(pyld_sz, GFP_KERNEL);
	if (!hdr) {
		IPAWANERR("fail to alloc exception hdr\n");
		return -ENOMEM;
	}
	hdr->num_hdrs = 1;
	hdr->commit = 1;
	hdr_entry = &hdr->hdr[0];

	strlcpy(hdr_entry->name, IPA_A7_QMAP_HDR_NAME,
				IPA_RESOURCE_NAME_MAX);
	hdr_entry->hdr_len = IPA_QMAP_HEADER_LENGTH; /* 4 bytes */

	if (ipa3_add_hdr(hdr)) {
		IPAWANERR("fail to add IPA_A7_QMAP hdr\n");
		ret = -EPERM;
		goto bail;
	}

	if (hdr_entry->status) {
		IPAWANERR("fail to add IPA_A7_QMAP hdr\n");
		ret = -EPERM;
		goto bail;
	}
	rmnet_ipa3_ctx->qmap_hdr_hdl = hdr_entry->hdr_hdl;

	ret = 0;
bail:
	kfree(hdr);
	return ret;
}

static void ipa3_del_a7_qmap_hdr(void)
{
	struct ipa_ioc_del_hdr *del_hdr;
	struct ipa_hdr_del *hdl_entry;
	u32 pyld_sz;
	int ret;

	pyld_sz = sizeof(struct ipa_ioc_del_hdr) + 1 *
		      sizeof(struct ipa_hdr_del);
	del_hdr = kzalloc(pyld_sz, GFP_KERNEL);
	if (!del_hdr) {
		IPAWANERR("fail to alloc exception hdr_del\n");
		return;
	}

	del_hdr->commit = 1;
	del_hdr->num_hdls = 1;
	hdl_entry = &del_hdr->hdl[0];
	hdl_entry->hdl = rmnet_ipa3_ctx->qmap_hdr_hdl;

	ret = ipa3_del_hdr(del_hdr);
	if (ret || hdl_entry->status)
		IPAWANERR("ipa3_del_hdr failed\n");
	else
		IPAWANDBG("hdrs deletion done\n");

	rmnet_ipa3_ctx->qmap_hdr_hdl = 0;
	kfree(del_hdr);
}

static void ipa3_del_qmap_hdr(uint32_t hdr_hdl)
{
	struct ipa_ioc_del_hdr *del_hdr;
	struct ipa_hdr_del *hdl_entry;
	u32 pyld_sz;
	int ret;

	if (hdr_hdl == 0) {
		IPAWANERR("Invalid hdr_hdl provided\n");
		return;
	}

	pyld_sz = sizeof(struct ipa_ioc_del_hdr) + 1 *
		sizeof(struct ipa_hdr_del);
	del_hdr = kzalloc(pyld_sz, GFP_KERNEL);
	if (!del_hdr) {
		IPAWANERR("fail to alloc exception hdr_del\n");
		return;
	}

	del_hdr->commit = 1;
	del_hdr->num_hdls = 1;
	hdl_entry = &del_hdr->hdl[0];
	hdl_entry->hdl = hdr_hdl;

	ret = ipa3_del_hdr(del_hdr);
	if (ret || hdl_entry->status)
		IPAWANERR("ipa3_del_hdr failed\n");
	else
		IPAWANDBG("header deletion done\n");

	rmnet_ipa3_ctx->qmap_hdr_hdl = 0;
	kfree(del_hdr);
}

static void ipa3_del_mux_qmap_hdrs(void)
{
	int index;

	for (index = 0; index < rmnet_ipa3_ctx->rmnet_index; index++) {
		ipa3_del_qmap_hdr(rmnet_ipa3_ctx->mux_channel[index].hdr_hdl);
		rmnet_ipa3_ctx->mux_channel[index].hdr_hdl = 0;
	}
}

static int ipa3_add_qmap_hdr(uint32_t mux_id, uint32_t *hdr_hdl)
{
	struct ipa_ioc_add_hdr *hdr;
	struct ipa_hdr_add *hdr_entry;
	char hdr_name[IPA_RESOURCE_NAME_MAX];
	u32 pyld_sz;
	int ret;

	pyld_sz = sizeof(struct ipa_ioc_add_hdr) + 1 *
		      sizeof(struct ipa_hdr_add);
	hdr = kzalloc(pyld_sz, GFP_KERNEL);
	if (!hdr) {
		IPAWANERR("fail to alloc exception hdr\n");
		return -ENOMEM;
	}
	hdr->num_hdrs = 1;
	hdr->commit = 1;
	hdr_entry = &hdr->hdr[0];

	snprintf(hdr_name, IPA_RESOURCE_NAME_MAX, "%s%d",
		 A2_MUX_HDR_NAME_V4_PREF,
		 mux_id);
	 strlcpy(hdr_entry->name, hdr_name,
				IPA_RESOURCE_NAME_MAX);

	hdr_entry->hdr_len = IPA_QMAP_HEADER_LENGTH; /* 4 bytes */
	hdr_entry->hdr[1] = (uint8_t) mux_id;
	IPAWANDBG("header (%s) with mux-id: (%d)\n",
		hdr_name,
		hdr_entry->hdr[1]);
	if (ipa3_add_hdr(hdr)) {
		IPAWANERR("fail to add IPA_QMAP hdr\n");
		ret = -EPERM;
		goto bail;
	}

	if (hdr_entry->status) {
		IPAWANERR("fail to add IPA_QMAP hdr\n");
		ret = -EPERM;
		goto bail;
	}

	ret = 0;
	*hdr_hdl = hdr_entry->hdr_hdl;
bail:
	kfree(hdr);
	return ret;
}

/**
* ipa3_setup_dflt_wan_rt_tables() - Setup default wan routing tables
*
* Return codes:
* 0: success
* -ENOMEM: failed to allocate memory
* -EPERM: failed to add the tables
*/
static int ipa3_setup_dflt_wan_rt_tables(void)
{
	struct ipa_ioc_add_rt_rule *rt_rule;
	struct ipa_rt_rule_add *rt_rule_entry;

	rt_rule =
	   kzalloc(sizeof(struct ipa_ioc_add_rt_rule) + 1 *
			   sizeof(struct ipa_rt_rule_add), GFP_KERNEL);
	if (!rt_rule) {
		IPAWANERR("fail to alloc mem\n");
		return -ENOMEM;
	}
	/* setup a default v4 route to point to Apps */
	rt_rule->num_rules = 1;
	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v4;
	strlcpy(rt_rule->rt_tbl_name, IPA_DFLT_WAN_RT_TBL_NAME,
			IPA_RESOURCE_NAME_MAX);

	rt_rule_entry = &rt_rule->rules[0];
	rt_rule_entry->at_rear = 1;
	rt_rule_entry->rule.dst = IPA_CLIENT_APPS_WAN_CONS;
	rt_rule_entry->rule.hdr_hdl = rmnet_ipa3_ctx->qmap_hdr_hdl;

	if (ipa3_add_rt_rule(rt_rule)) {
		IPAWANERR("fail to add dflt_wan v4 rule\n");
		kfree(rt_rule);
		return -EPERM;
	}

	IPAWANDBG("dflt v4 rt rule hdl=%x\n", rt_rule_entry->rt_rule_hdl);
	rmnet_ipa3_ctx->dflt_v4_wan_rt_hdl = rt_rule_entry->rt_rule_hdl;

	/* setup a default v6 route to point to A5 */
	rt_rule->ip = IPA_IP_v6;
	if (ipa3_add_rt_rule(rt_rule)) {
		IPAWANERR("fail to add dflt_wan v6 rule\n");
		kfree(rt_rule);
		return -EPERM;
	}
	IPAWANDBG("dflt v6 rt rule hdl=%x\n", rt_rule_entry->rt_rule_hdl);
	rmnet_ipa3_ctx->dflt_v6_wan_rt_hdl = rt_rule_entry->rt_rule_hdl;

	kfree(rt_rule);
	return 0;
}

static void ipa3_del_dflt_wan_rt_tables(void)
{
	struct ipa_ioc_del_rt_rule *rt_rule;
	struct ipa_rt_rule_del *rt_rule_entry;
	int len;

	len = sizeof(struct ipa_ioc_del_rt_rule) + 1 *
			   sizeof(struct ipa_rt_rule_del);
	rt_rule = kzalloc(len, GFP_KERNEL);
	if (!rt_rule) {
		IPAWANERR("unable to allocate memory for del route rule\n");
		return;
	}

	memset(rt_rule, 0, len);
	rt_rule->commit = 1;
	rt_rule->num_hdls = 1;
	rt_rule->ip = IPA_IP_v4;

	rt_rule_entry = &rt_rule->hdl[0];
	rt_rule_entry->status = -1;
	rt_rule_entry->hdl = rmnet_ipa3_ctx->dflt_v4_wan_rt_hdl;

	IPAWANERR("Deleting Route hdl:(0x%x) with ip type: %d\n",
		rt_rule_entry->hdl, IPA_IP_v4);
	if (ipa3_del_rt_rule(rt_rule) ||
			(rt_rule_entry->status)) {
		IPAWANERR("Routing rule deletion failed!\n");
	}

	rt_rule->ip = IPA_IP_v6;
	rt_rule_entry->hdl = rmnet_ipa3_ctx->dflt_v6_wan_rt_hdl;
	IPAWANERR("Deleting Route hdl:(0x%x) with ip type: %d\n",
		rt_rule_entry->hdl, IPA_IP_v6);
	if (ipa3_del_rt_rule(rt_rule) ||
			(rt_rule_entry->status)) {
		IPAWANERR("Routing rule deletion failed!\n");
	}

	kfree(rt_rule);
}

int ipa3_copy_ul_filter_rule_to_ipa(struct ipa_install_fltr_rule_req_msg_v01
		*rule_req)
{
	int i, j;

	if (rule_req->filter_spec_ex_list_valid == true) {
		rmnet_ipa3_ctx->num_q6_rules =
			rule_req->filter_spec_ex_list_len;
		IPAWANDBG("Received (%d) install_flt_req\n",
			rmnet_ipa3_ctx->num_q6_rules);
	} else {
		rmnet_ipa3_ctx->num_q6_rules = 0;
		IPAWANERR("got no UL rules from modem\n");
		return -EINVAL;
	}

	/* copy UL filter rules from Modem*/
	for (i = 0; i < rmnet_ipa3_ctx->num_q6_rules; i++) {
		/* check if rules overside the cache*/
		if (i == MAX_NUM_Q6_RULE) {
			IPAWANERR("Reaching (%d) max cache ",
				MAX_NUM_Q6_RULE);
			IPAWANERR(" however total (%d)\n",
				rmnet_ipa3_ctx->num_q6_rules);
			goto failure;
		}
		ipa3_qmi_ctx->q6_ul_filter_rule[i].ip =
			rule_req->filter_spec_ex_list[i].ip_type;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].action =
			rule_req->filter_spec_ex_list[i].filter_action;
		if (rule_req->filter_spec_ex_list[i].
			is_routing_table_index_valid == true)
			ipa3_qmi_ctx->q6_ul_filter_rule[i].rt_tbl_idx =
			rule_req->filter_spec_ex_list[i].route_table_index;
		if (rule_req->filter_spec_ex_list[i].is_mux_id_valid == true)
			ipa3_qmi_ctx->q6_ul_filter_rule[i].mux_id =
			rule_req->filter_spec_ex_list[i].mux_id;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].rule_id =
			rule_req->filter_spec_ex_list[i].rule_id;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].is_rule_hashable =
			rule_req->filter_spec_ex_list[i].is_rule_hashable;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.rule_eq_bitmap =
			rule_req->filter_spec_ex_list[i].filter_rule.
			rule_eq_bitmap;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.tos_eq_present =
			rule_req->filter_spec_ex_list[i].filter_rule.
			tos_eq_present;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.tos_eq =
			rule_req->filter_spec_ex_list[i].filter_rule.tos_eq;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			protocol_eq_present = rule_req->filter_spec_ex_list[i].
			filter_rule.protocol_eq_present;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.protocol_eq =
			rule_req->filter_spec_ex_list[i].filter_rule.
			protocol_eq;

		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			num_ihl_offset_range_16 =
			rule_req->filter_spec_ex_list[i].
			filter_rule.num_ihl_offset_range_16;
		for (j = 0; j < ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			num_ihl_offset_range_16; j++) {
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			ihl_offset_range_16[j].offset = rule_req->
			filter_spec_ex_list[i].filter_rule.
			ihl_offset_range_16[j].offset;
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			ihl_offset_range_16[j].range_low = rule_req->
			filter_spec_ex_list[i].filter_rule.
			ihl_offset_range_16[j].range_low;
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			ihl_offset_range_16[j].range_high = rule_req->
			filter_spec_ex_list[i].filter_rule.
			ihl_offset_range_16[j].range_high;
		}
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.num_offset_meq_32 =
			rule_req->filter_spec_ex_list[i].filter_rule.
			num_offset_meq_32;
		for (j = 0; j < ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
				num_offset_meq_32; j++) {
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			offset_meq_32[j].offset =
			rule_req->filter_spec_ex_list[i].
			filter_rule.offset_meq_32[j].offset;
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			offset_meq_32[j].mask =
			rule_req->filter_spec_ex_list[i].
			filter_rule.offset_meq_32[j].mask;
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			offset_meq_32[j].value =
			rule_req->filter_spec_ex_list[i].
			filter_rule.offset_meq_32[j].value;
		}

		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.tc_eq_present =
			rule_req->filter_spec_ex_list[i].
			filter_rule.tc_eq_present;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.tc_eq =
			rule_req->filter_spec_ex_list[i].filter_rule.tc_eq;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.fl_eq_present =
			rule_req->filter_spec_ex_list[i].filter_rule.
			flow_eq_present;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.fl_eq =
			rule_req->filter_spec_ex_list[i].filter_rule.flow_eq;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
		ihl_offset_eq_16_present = rule_req->filter_spec_ex_list[i].
		filter_rule.ihl_offset_eq_16_present;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
		ihl_offset_eq_16.offset = rule_req->filter_spec_ex_list[i].
		filter_rule.ihl_offset_eq_16.offset;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
		ihl_offset_eq_16.value = rule_req->filter_spec_ex_list[i].
		filter_rule.ihl_offset_eq_16.value;

		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
		ihl_offset_eq_32_present = rule_req->filter_spec_ex_list[i].
		filter_rule.ihl_offset_eq_32_present;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
		ihl_offset_eq_32.offset = rule_req->filter_spec_ex_list[i].
		filter_rule.ihl_offset_eq_32.offset;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
		ihl_offset_eq_32.value = rule_req->filter_spec_ex_list[i].
		filter_rule.ihl_offset_eq_32.value;

		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
		num_ihl_offset_meq_32 = rule_req->filter_spec_ex_list[i].
		filter_rule.num_ihl_offset_meq_32;
		for (j = 0; j < ipa3_qmi_ctx->q6_ul_filter_rule[i].
			eq_attrib.num_ihl_offset_meq_32; j++) {
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
				ihl_offset_meq_32[j].offset = rule_req->
				filter_spec_ex_list[i].filter_rule.
				ihl_offset_meq_32[j].offset;
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
				ihl_offset_meq_32[j].mask = rule_req->
				filter_spec_ex_list[i].filter_rule.
				ihl_offset_meq_32[j].mask;
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
				ihl_offset_meq_32[j].value = rule_req->
				filter_spec_ex_list[i].filter_rule.
				ihl_offset_meq_32[j].value;
		}
		ipa3_qmi_ctx->
			q6_ul_filter_rule[i].eq_attrib.num_offset_meq_128 =
			rule_req->filter_spec_ex_list[i].filter_rule.
			num_offset_meq_128;
		for (j = 0; j < ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			num_offset_meq_128; j++) {
			ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
				offset_meq_128[j].offset = rule_req->
				filter_spec_ex_list[i].filter_rule.
				offset_meq_128[j].offset;
			memcpy(ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
					offset_meq_128[j].mask,
					rule_req->filter_spec_ex_list[i].
					filter_rule.offset_meq_128[j].mask, 16);
			memcpy(ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
					offset_meq_128[j].value, rule_req->
					filter_spec_ex_list[i].filter_rule.
					offset_meq_128[j].value, 16);
		}

		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			metadata_meq32_present =
				rule_req->filter_spec_ex_list[i].
				filter_rule.metadata_meq32_present;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			metadata_meq32.offset =
			rule_req->filter_spec_ex_list[i].
			filter_rule.metadata_meq32.offset;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			metadata_meq32.mask = rule_req->filter_spec_ex_list[i].
			filter_rule.metadata_meq32.mask;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.metadata_meq32.
			value = rule_req->filter_spec_ex_list[i].filter_rule.
			metadata_meq32.value;
		ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib.
			ipv4_frag_eq_present = rule_req->filter_spec_ex_list[i].
			filter_rule.ipv4_frag_eq_present;
	}

	if (rule_req->xlat_filter_indices_list_valid) {
		if (rule_req->xlat_filter_indices_list_len >
		    rmnet_ipa3_ctx->num_q6_rules) {
			IPAWANERR("Number of xlat indices is not valid: %d\n",
					rule_req->xlat_filter_indices_list_len);
			goto failure;
		}
		IPAWANDBG("Receive %d XLAT indices: ",
				rule_req->xlat_filter_indices_list_len);
		for (i = 0; i < rule_req->xlat_filter_indices_list_len; i++)
			IPAWANDBG("%d ", rule_req->xlat_filter_indices_list[i]);
		IPAWANDBG("\n");

		for (i = 0; i < rule_req->xlat_filter_indices_list_len; i++) {
			if (rule_req->xlat_filter_indices_list[i]
				>= rmnet_ipa3_ctx->num_q6_rules) {
				IPAWANERR("Xlat rule idx is wrong: %d\n",
					rule_req->xlat_filter_indices_list[i]);
				goto failure;
			} else {
				ipa3_qmi_ctx->q6_ul_filter_rule
				[rule_req->xlat_filter_indices_list[i]]
				.is_xlat_rule = 1;
				IPAWANDBG("Rule %d is xlat rule\n",
					rule_req->xlat_filter_indices_list[i]);
			}
		}
	}
	goto success;

failure:
	rmnet_ipa3_ctx->num_q6_rules = 0;
	memset(ipa3_qmi_ctx->q6_ul_filter_rule, 0,
		sizeof(ipa3_qmi_ctx->q6_ul_filter_rule));
	return -EINVAL;

success:
	return 0;
}

static int ipa3_wwan_add_ul_flt_rule_to_ipa(void)
{
	u32 pyld_sz;
	int i, retval = 0;
	struct ipa_ioc_add_flt_rule *param;
	struct ipa_flt_rule_add flt_rule_entry;
	struct ipa_fltr_installed_notif_req_msg_v01 *req;

	pyld_sz = sizeof(struct ipa_ioc_add_flt_rule) +
	   sizeof(struct ipa_flt_rule_add);
	param = kzalloc(pyld_sz, GFP_KERNEL);
	if (!param)
		return -ENOMEM;

	req = (struct ipa_fltr_installed_notif_req_msg_v01 *)
		kzalloc(sizeof(struct ipa_fltr_installed_notif_req_msg_v01),
			GFP_KERNEL);
	if (!req) {
		kfree(param);
		return -ENOMEM;
	}

	param->commit = 1;
	param->ep = IPA_CLIENT_APPS_LAN_WAN_PROD;
	param->global = false;
	param->num_rules = (uint8_t)1;

	for (i = 0; i < rmnet_ipa3_ctx->num_q6_rules; i++) {
		param->ip = ipa3_qmi_ctx->q6_ul_filter_rule[i].ip;
		memset(&flt_rule_entry, 0, sizeof(struct ipa_flt_rule_add));
		flt_rule_entry.at_rear = true;
		flt_rule_entry.rule.action =
			ipa3_qmi_ctx->q6_ul_filter_rule[i].action;
		flt_rule_entry.rule.rt_tbl_idx
		= ipa3_qmi_ctx->q6_ul_filter_rule[i].rt_tbl_idx;
		flt_rule_entry.rule.retain_hdr = true;
		flt_rule_entry.rule.hashable =
			ipa3_qmi_ctx->q6_ul_filter_rule[i].is_rule_hashable;
		flt_rule_entry.rule.rule_id =
			ipa3_qmi_ctx->q6_ul_filter_rule[i].rule_id;

		/* debug rt-hdl*/
		IPAWANDBG("install-IPA index(%d),rt-tbl:(%d)\n",
			i, flt_rule_entry.rule.rt_tbl_idx);
		flt_rule_entry.rule.eq_attrib_type = true;
		memcpy(&(flt_rule_entry.rule.eq_attrib),
			&ipa3_qmi_ctx->q6_ul_filter_rule[i].eq_attrib,
			sizeof(struct ipa_ipfltri_rule_eq));
		memcpy(&(param->rules[0]), &flt_rule_entry,
			sizeof(struct ipa_flt_rule_add));
		if (ipa3_add_flt_rule((struct ipa_ioc_add_flt_rule *)param)) {
			retval = -EFAULT;
			IPAWANERR("add A7 UL filter rule(%d) failed\n", i);
		} else {
			/* store the rule handler */
			ipa3_qmi_ctx->q6_ul_filter_rule_hdl[i] =
				param->rules[0].flt_rule_hdl;
		}
	}

	/* send ipa_fltr_installed_notif_req_msg_v01 to Q6*/
	req->source_pipe_index =
		ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_WAN_PROD);
	req->install_status = QMI_RESULT_SUCCESS_V01;
	req->rule_id_valid = 1;
	req->rule_id_len = rmnet_ipa3_ctx->num_q6_rules;
	for (i = 0; i < rmnet_ipa3_ctx->num_q6_rules; i++) {
		req->rule_id[i] =
			ipa3_qmi_ctx->q6_ul_filter_rule[i].rule_id;
	}
	if (ipa3_qmi_filter_notify_send(req)) {
		IPAWANDBG("add filter rule index on A7-RX failed\n");
		retval = -EFAULT;
	}
	rmnet_ipa3_ctx->old_num_q6_rules = rmnet_ipa3_ctx->num_q6_rules;
	IPAWANDBG("add (%d) filter rule index on A7-RX\n",
			rmnet_ipa3_ctx->old_num_q6_rules);
	kfree(param);
	kfree(req);
	return retval;
}

static int ipa3_wwan_del_ul_flt_rule_to_ipa(void)
{
	u32 pyld_sz;
	int i, retval = 0;
	struct ipa_ioc_del_flt_rule *param;
	struct ipa_flt_rule_del flt_rule_entry;

	pyld_sz = sizeof(struct ipa_ioc_del_flt_rule) +
	   sizeof(struct ipa_flt_rule_del);
	param = kzalloc(pyld_sz, GFP_KERNEL);
	if (!param) {
		IPAWANERR("kzalloc failed\n");
		return -ENOMEM;
	}

	param->commit = 1;
	param->num_hdls = (uint8_t) 1;

	for (i = 0; i < rmnet_ipa3_ctx->old_num_q6_rules; i++) {
		param->ip = ipa3_qmi_ctx->q6_ul_filter_rule[i].ip;
		memset(&flt_rule_entry, 0, sizeof(struct ipa_flt_rule_del));
		flt_rule_entry.hdl = ipa3_qmi_ctx->q6_ul_filter_rule_hdl[i];
		/* debug rt-hdl*/
		IPAWANDBG("delete-IPA rule index(%d)\n", i);
		memcpy(&(param->hdl[0]), &flt_rule_entry,
			sizeof(struct ipa_flt_rule_del));
		if (ipa3_del_flt_rule((struct ipa_ioc_del_flt_rule *)param)) {
			IPAWANERR("del A7 UL filter rule(%d) failed\n", i);
			kfree(param);
			return -EFAULT;
		}
	}

	/* set UL filter-rule add-indication */
	rmnet_ipa3_ctx->a7_ul_flt_set = false;
	rmnet_ipa3_ctx->old_num_q6_rules = 0;

	kfree(param);
	return retval;
}

static int ipa3_find_mux_channel_index(uint32_t mux_id)
{
	int i;

	for (i = 0; i < MAX_NUM_OF_MUX_CHANNEL; i++) {
		if (mux_id == rmnet_ipa3_ctx->mux_channel[i].mux_id)
			return i;
	}
	return MAX_NUM_OF_MUX_CHANNEL;
}

static int find_vchannel_name_index(const char *vchannel_name)
{
	int i;

	for (i = 0; i < MAX_NUM_OF_MUX_CHANNEL; i++) {
		if (strcmp(rmnet_ipa3_ctx->mux_channel[i].vchannel_name,
					vchannel_name) == 0)
			return i;
	}
	return MAX_NUM_OF_MUX_CHANNEL;
}

static int ipa3_wwan_register_to_ipa(int index)
{
	struct ipa_tx_intf tx_properties = {0};
	struct ipa_ioc_tx_intf_prop tx_ioc_properties[2] = { {0}, {0} };
	struct ipa_ioc_tx_intf_prop *tx_ipv4_property;
	struct ipa_ioc_tx_intf_prop *tx_ipv6_property;
	struct ipa_rx_intf rx_properties = {0};
	struct ipa_ioc_rx_intf_prop rx_ioc_properties[2] = { {0}, {0} };
	struct ipa_ioc_rx_intf_prop *rx_ipv4_property;
	struct ipa_ioc_rx_intf_prop *rx_ipv6_property;
	struct ipa_ext_intf ext_properties = {0};
	struct ipa_ioc_ext_intf_prop *ext_ioc_properties;
	u32 pyld_sz;
	int ret = 0, i;

	IPAWANDBG("index(%d) device[%s]:\n", index,
		rmnet_ipa3_ctx->mux_channel[index].vchannel_name);
	if (!rmnet_ipa3_ctx->mux_channel[index].mux_hdr_set) {
		ret = ipa3_add_qmap_hdr(
			rmnet_ipa3_ctx->mux_channel[index].mux_id,
			&rmnet_ipa3_ctx->mux_channel[index].hdr_hdl);
		if (ret) {
			IPAWANERR("ipa_add_mux_hdr failed (%d)\n", index);
			return ret;
		}
		rmnet_ipa3_ctx->mux_channel[index].mux_hdr_set = true;
	}
	tx_properties.prop = tx_ioc_properties;
	tx_ipv4_property = &tx_properties.prop[0];
	tx_ipv4_property->ip = IPA_IP_v4;
	tx_ipv4_property->dst_pipe = IPA_CLIENT_APPS_WAN_CONS;
	snprintf(tx_ipv4_property->hdr_name, IPA_RESOURCE_NAME_MAX, "%s%d",
		 A2_MUX_HDR_NAME_V4_PREF,
		 rmnet_ipa3_ctx->mux_channel[index].mux_id);
	tx_ipv6_property = &tx_properties.prop[1];
	tx_ipv6_property->ip = IPA_IP_v6;
	tx_ipv6_property->dst_pipe = IPA_CLIENT_APPS_WAN_CONS;
	/* no need use A2_MUX_HDR_NAME_V6_PREF, same header */
	snprintf(tx_ipv6_property->hdr_name, IPA_RESOURCE_NAME_MAX, "%s%d",
		 A2_MUX_HDR_NAME_V4_PREF,
		 rmnet_ipa3_ctx->mux_channel[index].mux_id);
	tx_properties.num_props = 2;

	rx_properties.prop = rx_ioc_properties;
	rx_ipv4_property = &rx_properties.prop[0];
	rx_ipv4_property->ip = IPA_IP_v4;
	rx_ipv4_property->attrib.attrib_mask |= IPA_FLT_META_DATA;
	rx_ipv4_property->attrib.meta_data =
		rmnet_ipa3_ctx->mux_channel[index].mux_id << WWAN_METADATA_SHFT;
	rx_ipv4_property->attrib.meta_data_mask = WWAN_METADATA_MASK;
	rx_ipv4_property->src_pipe = IPA_CLIENT_APPS_LAN_WAN_PROD;
	rx_ipv6_property = &rx_properties.prop[1];
	rx_ipv6_property->ip = IPA_IP_v6;
	rx_ipv6_property->attrib.attrib_mask |= IPA_FLT_META_DATA;
	rx_ipv6_property->attrib.meta_data =
		rmnet_ipa3_ctx->mux_channel[index].mux_id << WWAN_METADATA_SHFT;
	rx_ipv6_property->attrib.meta_data_mask = WWAN_METADATA_MASK;
	rx_ipv6_property->src_pipe = IPA_CLIENT_APPS_LAN_WAN_PROD;
	rx_properties.num_props = 2;

	pyld_sz = rmnet_ipa3_ctx->num_q6_rules *
	   sizeof(struct ipa_ioc_ext_intf_prop);
	ext_ioc_properties = kmalloc(pyld_sz, GFP_KERNEL);
	if (!ext_ioc_properties) {
		IPAWANERR("Error allocate memory\n");
		return -ENOMEM;
	}

	ext_properties.prop = ext_ioc_properties;
	ext_properties.excp_pipe_valid = true;
	ext_properties.excp_pipe = IPA_CLIENT_APPS_WAN_CONS;
	ext_properties.num_props = rmnet_ipa3_ctx->num_q6_rules;
	for (i = 0; i < rmnet_ipa3_ctx->num_q6_rules; i++) {
		memcpy(&(ext_properties.prop[i]),
				 &(ipa3_qmi_ctx->q6_ul_filter_rule[i]),
				sizeof(struct ipa_ioc_ext_intf_prop));
	ext_properties.prop[i].mux_id =
		rmnet_ipa3_ctx->mux_channel[index].mux_id;
	IPAWANDBG("index %d ip: %d rt-tbl:%d\n", i,
		ext_properties.prop[i].ip,
		ext_properties.prop[i].rt_tbl_idx);
	IPAWANDBG("action: %d mux:%d\n",
		ext_properties.prop[i].action,
		ext_properties.prop[i].mux_id);
	}
	ret = ipa3_register_intf_ext(rmnet_ipa3_ctx->mux_channel[index].
		vchannel_name, &tx_properties,
		&rx_properties, &ext_properties);
	if (ret) {
		IPAWANERR("[%s]:ipa3_register_intf failed %d\n",
			rmnet_ipa3_ctx->mux_channel[index].vchannel_name, ret);
		goto fail;
	}
	rmnet_ipa3_ctx->mux_channel[index].ul_flt_reg = true;
fail:
	kfree(ext_ioc_properties);
	return ret;
}

static void ipa3_cleanup_deregister_intf(void)
{
	int i;
	int ret;

	for (i = 0; i < rmnet_ipa3_ctx->rmnet_index; i++) {
		if (rmnet_ipa3_ctx->mux_channel[i].ul_flt_reg) {
			ret = ipa3_deregister_intf(
				rmnet_ipa3_ctx->mux_channel[i].vchannel_name);
			if (ret < 0) {
				IPAWANERR("de-register device %s(%d) failed\n",
					rmnet_ipa3_ctx->mux_channel[i].
					vchannel_name,
					i);
				return;
			}
			IPAWANDBG("de-register device %s(%d) success\n",
				rmnet_ipa3_ctx->mux_channel[i].vchannel_name,
				i);
		}
		rmnet_ipa3_ctx->mux_channel[i].ul_flt_reg = false;
	}
}

int ipa3_wwan_update_mux_channel_prop(void)
{
	int ret = 0, i;
	/* install UL filter rules */
	if (rmnet_ipa3_ctx->egress_set) {
		if (ipa3_qmi_ctx->modem_cfg_emb_pipe_flt == false) {
			IPAWANDBG("setup UL filter rules\n");
			if (rmnet_ipa3_ctx->a7_ul_flt_set) {
				IPAWANDBG("del previous UL filter rules\n");
				/* delete rule hdlers */
				ret = ipa3_wwan_del_ul_flt_rule_to_ipa();
				if (ret) {
					IPAWANERR("failed to del old rules\n");
					return -EINVAL;
				}
				IPAWANDBG("deleted old UL rules\n");
			}
			ret = ipa3_wwan_add_ul_flt_rule_to_ipa();
		}
		if (ret)
			IPAWANERR("failed to install UL rules\n");
		else
			rmnet_ipa3_ctx->a7_ul_flt_set = true;
	}
	/* update Tx/Rx/Ext property */
	IPAWANDBG("update Tx/Rx/Ext property in IPA\n");
	if (rmnet_ipa3_ctx->rmnet_index == 0) {
		IPAWANDBG("no Tx/Rx/Ext property registered in IPA\n");
		return ret;
	}

	ipa3_cleanup_deregister_intf();

	for (i = 0; i < rmnet_ipa3_ctx->rmnet_index; i++) {
		ret = ipa3_wwan_register_to_ipa(i);
		if (ret < 0) {
			IPAWANERR("failed to re-regist %s, mux %d, index %d\n",
				rmnet_ipa3_ctx->mux_channel[i].vchannel_name,
				rmnet_ipa3_ctx->mux_channel[i].mux_id,
				i);
			return -ENODEV;
		}
		IPAWANERR("dev(%s) has registered to IPA\n",
		rmnet_ipa3_ctx->mux_channel[i].vchannel_name);
		rmnet_ipa3_ctx->mux_channel[i].ul_flt_reg = true;
	}
	return ret;
}

#ifdef INIT_COMPLETION
#define reinit_completion(x) INIT_COMPLETION(*(x))
#endif /* INIT_COMPLETION */

static int __ipa_wwan_open(struct net_device *dev)
{
	struct ipa3_wwan_private *wwan_ptr = netdev_priv(dev);

	IPAWANDBG("[%s] __wwan_open()\n", dev->name);
	if (wwan_ptr->device_status != WWAN_DEVICE_ACTIVE)
		reinit_completion(&wwan_ptr->resource_granted_completion);
	wwan_ptr->device_status = WWAN_DEVICE_ACTIVE;

	if (ipa3_rmnet_res.ipa_napi_enable)
		napi_enable(&(wwan_ptr->napi));
	return 0;
}

/**
 * wwan_open() - Opens the wwan network interface. Opens logical
 * channel on A2 MUX driver and starts the network stack queue
 *
 * @dev: network device
 *
 * Return codes:
 * 0: success
 * -ENODEV: Error while opening logical channel on A2 MUX driver
 */
static int ipa3_wwan_open(struct net_device *dev)
{
	int rc = 0;

	IPAWANDBG("[%s] wwan_open()\n", dev->name);
	rc = __ipa_wwan_open(dev);
	if (rc == 0)
		netif_start_queue(dev);
	return rc;
}

static int __ipa_wwan_close(struct net_device *dev)
{
	struct ipa3_wwan_private *wwan_ptr = netdev_priv(dev);
	int rc = 0;

	if (wwan_ptr->device_status == WWAN_DEVICE_ACTIVE) {
		wwan_ptr->device_status = WWAN_DEVICE_INACTIVE;
		/* do not close wwan port once up,  this causes
		 * remote side to hang if tried to open again
		 */
		reinit_completion(&wwan_ptr->resource_granted_completion);
		rc = ipa3_deregister_intf(dev->name);
		if (rc) {
			IPAWANERR("[%s]: ipa3_deregister_intf failed %d\n",
			       dev->name, rc);
			return rc;
		}
		return rc;
	} else {
		return -EBADF;
	}
}

/**
 * ipa3_wwan_stop() - Stops the wwan network interface. Closes
 * logical channel on A2 MUX driver and stops the network stack
 * queue
 *
 * @dev: network device
 *
 * Return codes:
 * 0: success
 * -ENODEV: Error while opening logical channel on A2 MUX driver
 */
static int ipa3_wwan_stop(struct net_device *dev)
{
	IPAWANDBG("[%s] ipa3_wwan_stop()\n", dev->name);
	__ipa_wwan_close(dev);
	netif_stop_queue(dev);
	return 0;
}

static int ipa3_wwan_change_mtu(struct net_device *dev, int new_mtu)
{
	if (0 > new_mtu || WWAN_DATA_LEN < new_mtu)
		return -EINVAL;
	IPAWANDBG("[%s] MTU change: old=%d new=%d\n",
		dev->name, dev->mtu, new_mtu);
	dev->mtu = new_mtu;
	return 0;
}

/**
 * ipa3_wwan_xmit() - Transmits an skb.
 *
 * @skb: skb to be transmitted
 * @dev: network device
 *
 * Return codes:
 * 0: success
 * NETDEV_TX_BUSY: Error while transmitting the skb. Try again
 * later
 * -EFAULT: Error while transmitting the skb
 */
static int ipa3_wwan_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret = 0;
	bool qmap_check;
	struct ipa3_wwan_private *wwan_ptr = netdev_priv(dev);
	struct ipa_tx_meta meta;

	if (skb->protocol != htons(ETH_P_MAP)) {
		IPAWANDBG_LOW
		("SW filtering out none QMAP packet received from %s",
		current->comm);
		return NETDEV_TX_OK;
	}

	qmap_check = RMNET_MAP_GET_CD_BIT(skb);
	if (netif_queue_stopped(dev)) {
		if (qmap_check &&
			atomic_read(&wwan_ptr->outstanding_pkts) <
					wwan_ptr->outstanding_high_ctl) {
			pr_err("[%s]Queue stop, send ctrl pkts\n", dev->name);
			goto send;
		} else {
			pr_err("[%s]fatal: ipa_wwan_xmit stopped\n", dev->name);
			return NETDEV_TX_BUSY;
		}
	}

	/* checking High WM hit */
	if (atomic_read(&wwan_ptr->outstanding_pkts) >=
					wwan_ptr->outstanding_high) {
		if (!qmap_check) {
			IPAWANDBG_LOW("pending(%d)/(%d)- stop(%d)\n",
				atomic_read(&wwan_ptr->outstanding_pkts),
				wwan_ptr->outstanding_high,
				netif_queue_stopped(dev));
			IPAWANDBG_LOW("qmap_chk(%d)\n", qmap_check);
			netif_stop_queue(dev);
			return NETDEV_TX_BUSY;
		}
	}

send:
	/* IPA_RM checking start */
	ret = ipa_rm_inactivity_timer_request_resource(
		IPA_RM_RESOURCE_WWAN_0_PROD);
	if (ret == -EINPROGRESS) {
		netif_stop_queue(dev);
		return NETDEV_TX_BUSY;
	}
	if (ret) {
		pr_err("[%s] fatal: ipa rm timer request resource failed %d\n",
		       dev->name, ret);
		return -EFAULT;
	}
	/* IPA_RM checking end */

	if (RMNET_MAP_GET_CD_BIT(skb)) {
		memset(&meta, 0, sizeof(meta));
		meta.pkt_init_dst_ep_valid = true;
		meta.pkt_init_dst_ep_remote = true;
		ret = ipa3_tx_dp(IPA_CLIENT_Q6_LAN_CONS, skb, &meta);
	} else {
		ret = ipa3_tx_dp(IPA_CLIENT_APPS_LAN_WAN_PROD, skb, NULL);
	}

	if (ret) {
		ret = NETDEV_TX_BUSY;
		dev->stats.tx_dropped++;
		goto out;
	}

	atomic_inc(&wwan_ptr->outstanding_pkts);
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	ret = NETDEV_TX_OK;
out:
	ipa_rm_inactivity_timer_release_resource(
		IPA_RM_RESOURCE_WWAN_0_PROD);
	return ret;
}

static void ipa3_wwan_tx_timeout(struct net_device *dev)
{
	IPAWANERR("[%s] ipa3_wwan_tx_timeout(), data stall in UL\n", dev->name);
}

/**
 * apps_ipa_tx_complete_notify() - Rx notify
 *
 * @priv: driver context
 * @evt: event type
 * @data: data provided with event
 *
 * Check that the packet is the one we sent and release it
 * This function will be called in defered context in IPA wq.
 */
static void apps_ipa_tx_complete_notify(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct net_device *dev = (struct net_device *)priv;
	struct ipa3_wwan_private *wwan_ptr;

	if (dev != IPA_NETDEV()) {
		IPAWANDBG("Received pre-SSR packet completion\n");
		dev_kfree_skb_any(skb);
		return;
	}

	if (evt != IPA_WRITE_DONE) {
		IPAWANERR("unsupported evt on Tx callback, Drop the packet\n");
		dev_kfree_skb_any(skb);
		dev->stats.tx_dropped++;
		return;
	}

	wwan_ptr = netdev_priv(dev);
	atomic_dec(&wwan_ptr->outstanding_pkts);
	__netif_tx_lock_bh(netdev_get_tx_queue(dev, 0));
	if (!atomic_read(&rmnet_ipa3_ctx->is_ssr) &&
		netif_queue_stopped(wwan_ptr->net) &&
		atomic_read(&wwan_ptr->outstanding_pkts) <
					(wwan_ptr->outstanding_low)) {
		IPAWANDBG_LOW("Outstanding low (%d) - waking up queue\n",
				wwan_ptr->outstanding_low);
		netif_wake_queue(wwan_ptr->net);
	}
	__netif_tx_unlock_bh(netdev_get_tx_queue(dev, 0));
	dev_kfree_skb_any(skb);
	ipa_rm_inactivity_timer_release_resource(
		IPA_RM_RESOURCE_WWAN_0_PROD);
}

/**
 * apps_ipa_packet_receive_notify() - Rx notify
 *
 * @priv: driver context
 * @evt: event type
 * @data: data provided with event
 *
 * IPA will pass a packet to the Linux network stack with skb->data
 */
static void apps_ipa_packet_receive_notify(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	struct net_device *dev = (struct net_device *)priv;

	if (evt == IPA_RECEIVE) {
		struct sk_buff *skb = (struct sk_buff *)data;
		int result;
		unsigned int packet_len = skb->len;

		IPAWANDBG_LOW("Rx packet was received");
		skb->dev = IPA_NETDEV();
		skb->protocol = htons(ETH_P_MAP);

		if (ipa3_rmnet_res.ipa_napi_enable) {
			trace_rmnet_ipa_netif_rcv_skb3(dev->stats.rx_packets);
			result = netif_receive_skb(skb);
		} else {
			if (dev->stats.rx_packets % IPA_WWAN_RX_SOFTIRQ_THRESH
					== 0) {
				trace_rmnet_ipa_netifni3(dev->stats.rx_packets);
				result = netif_rx_ni(skb);
			} else {
				trace_rmnet_ipa_netifrx3(dev->stats.rx_packets);
				result = netif_rx(skb);
			}
		}

		if (result)	{
			pr_err_ratelimited(DEV_NAME " %s:%d fail on netif_receive_skb\n",
							   __func__, __LINE__);
			dev->stats.rx_dropped++;
		}
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += packet_len;
	} else if (evt == IPA_CLIENT_START_POLL)
		ipa3_rmnet_rx_cb(priv);
	else if (evt == IPA_CLIENT_COMP_NAPI) {
		if (ipa3_rmnet_res.ipa_napi_enable)
			napi_complete(&(rmnet_ipa3_ctx->wwan_priv->napi));
	} else
		IPAWANERR("Invalid evt %d received in wan_ipa_receive\n", evt);
}

/**
 * ipa3_wwan_ioctl() - I/O control for wwan network driver.
 *
 * @dev: network device
 * @ifr: ignored
 * @cmd: cmd to be excecuded. can be one of the following:
 * IPA_WWAN_IOCTL_OPEN - Open the network interface
 * IPA_WWAN_IOCTL_CLOSE - Close the network interface
 *
 * Return codes:
 * 0: success
 * NETDEV_TX_BUSY: Error while transmitting the skb. Try again
 * later
 * -EFAULT: Error while transmitting the skb
 */
static int ipa3_wwan_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int rc = 0;
	int mru = 1000, epid = 1, mux_index, len;
	struct ipa_msg_meta msg_meta;
	struct ipa_wan_msg *wan_msg = NULL;
	struct rmnet_ioctl_extended_s extend_ioctl_data;
	struct rmnet_ioctl_data_s ioctl_data;
	struct ipa3_rmnet_mux_val *mux_channel;
	int rmnet_index;

	IPAWANDBG("rmnet_ipa got ioctl number 0x%08x", cmd);
	switch (cmd) {
	/*  Set Ethernet protocol  */
	case RMNET_IOCTL_SET_LLP_ETHERNET:
		break;
	/*  Set RAWIP protocol  */
	case RMNET_IOCTL_SET_LLP_IP:
		break;
	/*  Get link protocol  */
	case RMNET_IOCTL_GET_LLP:
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	/*  Set QoS header enabled  */
	case RMNET_IOCTL_SET_QOS_ENABLE:
		return -EINVAL;
	/*  Set QoS header disabled  */
	case RMNET_IOCTL_SET_QOS_DISABLE:
		break;
	/*  Get QoS header state  */
	case RMNET_IOCTL_GET_QOS:
		ioctl_data.u.operation_mode = RMNET_MODE_NONE;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	/*  Get operation mode */
	case RMNET_IOCTL_GET_OPMODE:
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	/*  Open transport port  */
	case RMNET_IOCTL_OPEN:
		break;
	/*  Close transport port  */
	case RMNET_IOCTL_CLOSE:
		break;
	/*  Flow enable  */
	case RMNET_IOCTL_FLOW_ENABLE:
		IPAWANDBG("Received flow enable\n");
		if (copy_from_user(&ioctl_data, ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_data_s))) {
			rc = -EFAULT;
			break;
		}
		ipa3_flow_control(IPA_CLIENT_USB_PROD, true,
			ioctl_data.u.tcm_handle);
		break;
	/*  Flow disable  */
	case RMNET_IOCTL_FLOW_DISABLE:
		IPAWANDBG("Received flow disable\n");
		if (copy_from_user(&ioctl_data, ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_data_s))) {
			rc = -EFAULT;
			break;
		}
		ipa3_flow_control(IPA_CLIENT_USB_PROD, false,
			ioctl_data.u.tcm_handle);
		break;
	/*  Set flow handle  */
	case RMNET_IOCTL_FLOW_SET_HNDL:
		break;

	/*  Extended IOCTLs  */
	case RMNET_IOCTL_EXTENDED:
		IPAWANDBG("get ioctl: RMNET_IOCTL_EXTENDED\n");
		if (copy_from_user(&extend_ioctl_data,
			(u8 *)ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_extended_s))) {
			IPAWANERR("failed to copy extended ioctl data\n");
			rc = -EFAULT;
			break;
		}
		switch (extend_ioctl_data.extended_ioctl) {
		/*  Get features  */
		case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
			IPAWANDBG("get RMNET_IOCTL_GET_SUPPORTED_FEATURES\n");
			extend_ioctl_data.u.data =
				(RMNET_IOCTL_FEAT_NOTIFY_MUX_CHANNEL |
				RMNET_IOCTL_FEAT_SET_EGRESS_DATA_FORMAT |
				RMNET_IOCTL_FEAT_SET_INGRESS_DATA_FORMAT);
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&extend_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			break;
		/*  Set MRU  */
		case RMNET_IOCTL_SET_MRU:
			mru = extend_ioctl_data.u.data;
			IPAWANDBG("get MRU size %d\n",
				extend_ioctl_data.u.data);
			break;
		/*  Get MRU  */
		case RMNET_IOCTL_GET_MRU:
			extend_ioctl_data.u.data = mru;
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&extend_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			break;
		/* GET SG support */
		case RMNET_IOCTL_GET_SG_SUPPORT:
			extend_ioctl_data.u.data =
				ipa3_rmnet_res.ipa_advertise_sg_support;
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&extend_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			break;
		/*  Get endpoint ID  */
		case RMNET_IOCTL_GET_EPID:
			IPAWANDBG("get ioctl: RMNET_IOCTL_GET_EPID\n");
			extend_ioctl_data.u.data = epid;
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&extend_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			if (copy_from_user(&extend_ioctl_data,
				(u8 *)ifr->ifr_ifru.ifru_data,
				sizeof(struct rmnet_ioctl_extended_s))) {
				IPAWANERR("copy extended ioctl data failed\n");
				rc = -EFAULT;
			break;
			}
			IPAWANDBG("RMNET_IOCTL_GET_EPID return %d\n",
					extend_ioctl_data.u.data);
			break;
		/*  Endpoint pair  */
		case RMNET_IOCTL_GET_EP_PAIR:
			IPAWANDBG("get ioctl: RMNET_IOCTL_GET_EP_PAIR\n");
			extend_ioctl_data.u.ipa_ep_pair.consumer_pipe_num =
			ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_WAN_PROD);
			extend_ioctl_data.u.ipa_ep_pair.producer_pipe_num =
			ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&extend_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			if (copy_from_user(&extend_ioctl_data,
				(u8 *)ifr->ifr_ifru.ifru_data,
				sizeof(struct rmnet_ioctl_extended_s))) {
				IPAWANERR("copy extended ioctl data failed\n");
				rc = -EFAULT;
			break;
		}
			IPAWANDBG("RMNET_IOCTL_GET_EP_PAIR c: %d p: %d\n",
			extend_ioctl_data.u.ipa_ep_pair.consumer_pipe_num,
			extend_ioctl_data.u.ipa_ep_pair.producer_pipe_num);
			break;
		/*  Get driver name  */
		case RMNET_IOCTL_GET_DRIVER_NAME:
			memcpy(&extend_ioctl_data.u.if_name,
				IPA_NETDEV()->name,
							sizeof(IFNAMSIZ));
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
					&extend_ioctl_data,
					sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			break;
		/*  Add MUX ID  */
		case RMNET_IOCTL_ADD_MUX_CHANNEL:
			mux_index = ipa3_find_mux_channel_index(
				extend_ioctl_data.u.rmnet_mux_val.mux_id);
			if (mux_index < MAX_NUM_OF_MUX_CHANNEL) {
				IPAWANDBG("already setup mux(%d)\n",
					extend_ioctl_data.u.
					rmnet_mux_val.mux_id);
				return rc;
			}
			if (rmnet_ipa3_ctx->rmnet_index
				>= MAX_NUM_OF_MUX_CHANNEL) {
				IPAWANERR("Exceed mux_channel limit(%d)\n",
				rmnet_ipa3_ctx->rmnet_index);
				return -EFAULT;
			}
			IPAWANDBG("ADD_MUX_CHANNEL(%d, name: %s)\n",
			extend_ioctl_data.u.rmnet_mux_val.mux_id,
			extend_ioctl_data.u.rmnet_mux_val.vchannel_name);
			/* cache the mux name and id */
			mux_channel = rmnet_ipa3_ctx->mux_channel;
			rmnet_index = rmnet_ipa3_ctx->rmnet_index;

			mux_channel[rmnet_index].mux_id =
				extend_ioctl_data.u.rmnet_mux_val.mux_id;
			memcpy(mux_channel[rmnet_index].vchannel_name,
				extend_ioctl_data.u.rmnet_mux_val.vchannel_name,
				sizeof(mux_channel[rmnet_index]
					.vchannel_name));
			IPAWANDBG("cashe device[%s:%d] in IPA_wan[%d]\n",
				mux_channel[rmnet_index].vchannel_name,
				mux_channel[rmnet_index].mux_id,
				rmnet_index);
			/* check if UL filter rules coming*/
			if (rmnet_ipa3_ctx->num_q6_rules != 0) {
				IPAWANERR("dev(%s) register to IPA\n",
					extend_ioctl_data.u.rmnet_mux_val.
					vchannel_name);
				rc = ipa3_wwan_register_to_ipa(
						rmnet_ipa3_ctx->rmnet_index);
				if (rc < 0) {
					IPAWANERR("device %s reg IPA failed\n",
						extend_ioctl_data.u.
						rmnet_mux_val.vchannel_name);
					return -ENODEV;
				}
				mux_channel[rmnet_index].mux_channel_set = true;
				mux_channel[rmnet_index].ul_flt_reg = true;
			} else {
				IPAWANDBG("dev(%s) haven't registered to IPA\n",
					extend_ioctl_data.u.
					rmnet_mux_val.vchannel_name);
				mux_channel[rmnet_index].mux_channel_set = true;
				mux_channel[rmnet_index].ul_flt_reg = false;
			}
			rmnet_ipa3_ctx->rmnet_index++;
			break;
		case RMNET_IOCTL_SET_EGRESS_DATA_FORMAT:
			IPAWANDBG("get RMNET_IOCTL_SET_EGRESS_DATA_FORMAT\n");
			if ((extend_ioctl_data.u.data) &
					RMNET_IOCTL_EGRESS_FORMAT_CHECKSUM) {
				rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.
					ipa_ep_cfg.hdr.hdr_len = 8;
				rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.
					ipa_ep_cfg.cfg.cs_offload_en =
					IPA_ENABLE_CS_OFFLOAD_UL;
				rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.
					ipa_ep_cfg.cfg.cs_metadata_hdr_offset
						= 1;
			} else {
				rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.
					ipa_ep_cfg.hdr.hdr_len = 4;
			}
			if ((extend_ioctl_data.u.data) &
					RMNET_IOCTL_EGRESS_FORMAT_AGGREGATION)
				rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.
					ipa_ep_cfg.aggr.aggr_en =
						IPA_ENABLE_AGGR;
			else
				rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.
					ipa_ep_cfg.aggr.aggr_en =
						IPA_BYPASS_AGGR;
			rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.ipa_ep_cfg.hdr.
				hdr_ofst_metadata_valid = 1;
			/* modem want offset at 0! */
			rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.ipa_ep_cfg.hdr.
				hdr_ofst_metadata = 0;
			rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.ipa_ep_cfg.mode.
				dst = IPA_CLIENT_APPS_LAN_WAN_PROD;
			rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.ipa_ep_cfg.mode.
				mode = IPA_BASIC;

			rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.client =
				IPA_CLIENT_APPS_LAN_WAN_PROD;
			rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.notify =
				apps_ipa_tx_complete_notify;
			rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.desc_fifo_sz =
			IPA_SYS_TX_DATA_DESC_FIFO_SZ;
			rmnet_ipa3_ctx->apps_to_ipa_ep_cfg.priv = dev;

			rc = ipa3_setup_sys_pipe(
				&rmnet_ipa3_ctx->apps_to_ipa_ep_cfg,
				&rmnet_ipa3_ctx->apps_to_ipa3_hdl);
			if (rc)
				IPAWANERR("failed to config egress endpoint\n");

			if (rmnet_ipa3_ctx->num_q6_rules != 0) {
				/* already got Q6 UL filter rules*/
				if (ipa3_qmi_ctx->modem_cfg_emb_pipe_flt
					== false)
					rc = ipa3_wwan_add_ul_flt_rule_to_ipa();
				else
					rc = 0;
				rmnet_ipa3_ctx->egress_set = true;
				if (rc)
					IPAWANERR("install UL rules failed\n");
				else
					rmnet_ipa3_ctx->a7_ul_flt_set = true;
			} else {
				/* wait Q6 UL filter rules*/
				rmnet_ipa3_ctx->egress_set = true;
				IPAWANDBG("no UL-rules, egress_set(%d)\n",
					rmnet_ipa3_ctx->egress_set);
			}
			break;
		case RMNET_IOCTL_SET_INGRESS_DATA_FORMAT:/*  Set IDF  */
			IPAWANDBG("get RMNET_IOCTL_SET_INGRESS_DATA_FORMAT\n");
			if ((extend_ioctl_data.u.data) &
					RMNET_IOCTL_INGRESS_FORMAT_CHECKSUM)
				rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.
					ipa_ep_cfg.cfg.cs_offload_en =
					IPA_ENABLE_CS_OFFLOAD_DL;

			if ((extend_ioctl_data.u.data) &
					RMNET_IOCTL_INGRESS_FORMAT_AGG_DATA) {
				IPAWANERR("get AGG size %d count %d\n",
					extend_ioctl_data.u.
					ingress_format.agg_size,
					extend_ioctl_data.u.
					ingress_format.agg_count);
				if (!ipa_disable_apps_wan_cons_deaggr(
					extend_ioctl_data.u.
					ingress_format.agg_size,
					extend_ioctl_data.
					u.ingress_format.agg_count)) {
					rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.
					ipa_ep_cfg.aggr.aggr_byte_limit =
					extend_ioctl_data.u.ingress_format.
					agg_size;
					rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.
					ipa_ep_cfg.aggr.aggr_pkt_limit =
					extend_ioctl_data.u.ingress_format.
					agg_count;
				}
			}

			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.hdr.
				hdr_len = 4;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.hdr.
				hdr_ofst_metadata_valid = 1;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.
				hdr.hdr_ofst_metadata = 1;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.hdr.
				hdr_ofst_pkt_size_valid = 1;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.hdr.
				hdr_ofst_pkt_size = 2;

			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.hdr_ext.
				hdr_total_len_or_pad_valid = true;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.hdr_ext.
				hdr_total_len_or_pad = 0;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.hdr_ext.
				hdr_payload_len_inc_padding = true;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.hdr_ext.
				hdr_total_len_or_pad_offset = 0;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.hdr_ext.
				hdr_little_endian = 0;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.ipa_ep_cfg.
				metadata_mask.metadata_mask = 0xFF000000;

			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.client =
				IPA_CLIENT_APPS_WAN_CONS;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.notify =
				apps_ipa_packet_receive_notify;
			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.priv = dev;

			rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.napi_enabled =
				ipa3_rmnet_res.ipa_napi_enable;
			if (rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.napi_enabled)
				rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.
				desc_fifo_sz = IPA_WAN_CONS_DESC_FIFO_SZ;
			else
				rmnet_ipa3_ctx->ipa_to_apps_ep_cfg.
				desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;

			mutex_lock(
				&rmnet_ipa3_ctx->ipa_to_apps_pipe_handle_guard);
			if (atomic_read(&rmnet_ipa3_ctx->is_ssr)) {
				IPAWANDBG("In SSR sequence/recovery\n");
				mutex_unlock(&rmnet_ipa3_ctx->
					ipa_to_apps_pipe_handle_guard);
				rc = -EFAULT;
				break;
			}
			rc = ipa3_setup_sys_pipe(
				&rmnet_ipa3_ctx->ipa_to_apps_ep_cfg,
				&rmnet_ipa3_ctx->ipa3_to_apps_hdl);
			mutex_unlock(&rmnet_ipa3_ctx->
				ipa_to_apps_pipe_handle_guard);
			if (rc)
				IPAWANERR("failed to configure ingress\n");
			break;
		case RMNET_IOCTL_SET_XLAT_DEV_INFO:
			wan_msg = kzalloc(sizeof(struct ipa_wan_msg),
						GFP_KERNEL);
			if (!wan_msg) {
				IPAWANERR("Failed to allocate memory.\n");
				return -ENOMEM;
			}
			len = sizeof(wan_msg->upstream_ifname) >
			sizeof(extend_ioctl_data.u.if_name) ?
				sizeof(extend_ioctl_data.u.if_name) :
				sizeof(wan_msg->upstream_ifname);
			strlcpy(wan_msg->upstream_ifname,
				extend_ioctl_data.u.if_name, len);
			memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
			msg_meta.msg_type = WAN_XLAT_CONNECT;
			msg_meta.msg_len = sizeof(struct ipa_wan_msg);
			rc = ipa3_send_msg(&msg_meta, wan_msg,
						ipa3_wwan_msg_free_cb);
			if (rc) {
				IPAWANERR("Failed to send XLAT_CONNECT msg\n");
				kfree(wan_msg);
			}
			break;
		/*  Get agg count  */
		case RMNET_IOCTL_GET_AGGREGATION_COUNT:
			break;
		/*  Set agg count  */
		case RMNET_IOCTL_SET_AGGREGATION_COUNT:
			break;
		/*  Get agg size  */
		case RMNET_IOCTL_GET_AGGREGATION_SIZE:
			break;
		/*  Set agg size  */
		case RMNET_IOCTL_SET_AGGREGATION_SIZE:
			break;
		/*  Do flow control  */
		case RMNET_IOCTL_FLOW_CONTROL:
			break;
		/*  For legacy use  */
		case RMNET_IOCTL_GET_DFLT_CONTROL_CHANNEL:
			break;
		/*  Get HW/SW map  */
		case RMNET_IOCTL_GET_HWSW_MAP:
			break;
		/*  Set RX Headroom  */
		case RMNET_IOCTL_SET_RX_HEADROOM:
			break;
		default:
			IPAWANERR("[%s] unsupported extended cmd[%d]",
				dev->name,
				extend_ioctl_data.extended_ioctl);
			rc = -EINVAL;
		}
		break;
	default:
			IPAWANERR("[%s] unsupported cmd[%d]",
				dev->name, cmd);
			rc = -EINVAL;
	}
	return rc;
}

static const struct net_device_ops ipa3_wwan_ops_ip = {
	.ndo_open = ipa3_wwan_open,
	.ndo_stop = ipa3_wwan_stop,
	.ndo_start_xmit = ipa3_wwan_xmit,
	.ndo_tx_timeout = ipa3_wwan_tx_timeout,
	.ndo_do_ioctl = ipa3_wwan_ioctl,
	.ndo_change_mtu = ipa3_wwan_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

/**
 * wwan_setup() - Setups the wwan network driver.
 *
 * @dev: network device
 *
 * Return codes:
 * None
 */

static void ipa3_wwan_setup(struct net_device *dev)
{
	dev->netdev_ops = &ipa3_wwan_ops_ip;
	ether_setup(dev);
	/* set this after calling ether_setup */
	dev->header_ops = 0;  /* No header */
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->mtu = WWAN_DATA_LEN;
	dev->addr_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
	dev->needed_headroom = HEADROOM_FOR_QMAP;
	dev->needed_tailroom = TAILROOM;
	dev->watchdog_timeo = 1000;
}

/* IPA_RM related functions start*/
static void ipa3_q6_prod_rm_request_resource(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_q6_con_rm_request,
		ipa3_q6_prod_rm_request_resource);
static void ipa3_q6_prod_rm_release_resource(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_q6_con_rm_release,
		ipa3_q6_prod_rm_release_resource);

static void ipa3_q6_prod_rm_request_resource(struct work_struct *work)
{
	int ret = 0;

	ret = ipa_rm_request_resource(IPA_RM_RESOURCE_Q6_PROD);
	if (ret < 0 && ret != -EINPROGRESS) {
		IPAWANERR("%s: ipa_rm_request_resource failed %d\n", __func__,
		       ret);
		return;
	}
}

static int ipa3_q6_rm_request_resource(void)
{
	queue_delayed_work(rmnet_ipa3_ctx->rm_q6_wq,
	   &ipa3_q6_con_rm_request, 0);
	return 0;
}

static void ipa3_q6_prod_rm_release_resource(struct work_struct *work)
{
	int ret = 0;

	ret = ipa_rm_release_resource(IPA_RM_RESOURCE_Q6_PROD);
	if (ret < 0 && ret != -EINPROGRESS) {
		IPAWANERR("%s: ipa_rm_release_resource failed %d\n", __func__,
		      ret);
		return;
	}
}


static int ipa3_q6_rm_release_resource(void)
{
	queue_delayed_work(rmnet_ipa3_ctx->rm_q6_wq,
	   &ipa3_q6_con_rm_release, 0);
	return 0;
}


static void ipa3_q6_rm_notify_cb(void *user_data,
		enum ipa_rm_event event,
		unsigned long data)
{
	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		IPAWANDBG_LOW("%s: Q6_PROD GRANTED CB\n", __func__);
		break;
	case IPA_RM_RESOURCE_RELEASED:
		IPAWANDBG_LOW("%s: Q6_PROD RELEASED CB\n", __func__);
		break;
	default:
		return;
	}
}
static int ipa3_q6_initialize_rm(void)
{
	struct ipa_rm_create_params create_params;
	struct ipa_rm_perf_profile profile;
	int result;

	/* Initialize IPA_RM workqueue */
	rmnet_ipa3_ctx->rm_q6_wq = create_singlethread_workqueue("clnt_req");
	if (!rmnet_ipa3_ctx->rm_q6_wq)
		return -ENOMEM;

	memset(&create_params, 0, sizeof(create_params));
	create_params.name = IPA_RM_RESOURCE_Q6_PROD;
	create_params.reg_params.notify_cb = &ipa3_q6_rm_notify_cb;
	result = ipa_rm_create_resource(&create_params);
	if (result)
		goto create_rsrc_err1;
	memset(&create_params, 0, sizeof(create_params));
	create_params.name = IPA_RM_RESOURCE_Q6_CONS;
	create_params.release_resource = &ipa3_q6_rm_release_resource;
	create_params.request_resource = &ipa3_q6_rm_request_resource;
	result = ipa_rm_create_resource(&create_params);
	if (result)
		goto create_rsrc_err2;
	/* add dependency*/
	result = ipa_rm_add_dependency(IPA_RM_RESOURCE_Q6_PROD,
			IPA_RM_RESOURCE_APPS_CONS);
	if (result)
		goto add_dpnd_err;
	/* setup Performance profile */
	memset(&profile, 0, sizeof(profile));
	profile.max_supported_bandwidth_mbps = 100;
	result = ipa_rm_set_perf_profile(IPA_RM_RESOURCE_Q6_PROD,
			&profile);
	if (result)
		goto set_perf_err;
	result = ipa_rm_set_perf_profile(IPA_RM_RESOURCE_Q6_CONS,
			&profile);
	if (result)
		goto set_perf_err;
	return result;

set_perf_err:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
			IPA_RM_RESOURCE_APPS_CONS);
add_dpnd_err:
	result = ipa_rm_delete_resource(IPA_RM_RESOURCE_Q6_CONS);
	if (result < 0)
		IPAWANERR("Error deleting resource %d, ret=%d\n",
			IPA_RM_RESOURCE_Q6_CONS, result);
create_rsrc_err2:
	result = ipa_rm_delete_resource(IPA_RM_RESOURCE_Q6_PROD);
	if (result < 0)
		IPAWANERR("Error deleting resource %d, ret=%d\n",
			IPA_RM_RESOURCE_Q6_PROD, result);
create_rsrc_err1:
	destroy_workqueue(rmnet_ipa3_ctx->rm_q6_wq);
	return result;
}

void ipa3_q6_deinitialize_rm(void)
{
	int ret;

	ret = ipa_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
			IPA_RM_RESOURCE_APPS_CONS);
	if (ret < 0)
		IPAWANERR("Error deleting dependency %d->%d, ret=%d\n",
			IPA_RM_RESOURCE_Q6_PROD, IPA_RM_RESOURCE_APPS_CONS,
			ret);
	ret = ipa_rm_delete_resource(IPA_RM_RESOURCE_Q6_CONS);
	if (ret < 0)
		IPAWANERR("Error deleting resource %d, ret=%d\n",
			IPA_RM_RESOURCE_Q6_CONS, ret);
	ret = ipa_rm_delete_resource(IPA_RM_RESOURCE_Q6_PROD);
	if (ret < 0)
		IPAWANERR("Error deleting resource %d, ret=%d\n",
			IPA_RM_RESOURCE_Q6_PROD, ret);
	destroy_workqueue(rmnet_ipa3_ctx->rm_q6_wq);
}

static void ipa3_wake_tx_queue(struct work_struct *work)
{
	if (IPA_NETDEV()) {
		__netif_tx_lock_bh(netdev_get_tx_queue(IPA_NETDEV(), 0));
		netif_wake_queue(IPA_NETDEV());
		__netif_tx_unlock_bh(netdev_get_tx_queue(IPA_NETDEV(), 0));
	}
}

/**
 * ipa3_rm_resource_granted() - Called upon
 * IPA_RM_RESOURCE_GRANTED event. Wakes up queue is was stopped.
 *
 * @work: work object supplied ny workqueue
 *
 * Return codes:
 * None
 */
static void ipa3_rm_resource_granted(void *dev)
{
	IPAWANDBG_LOW("Resource Granted - starting queue\n");
	schedule_work(&ipa3_tx_wakequeue_work);
}

/**
 * ipa3_rm_notify() - Callback function for RM events. Handles
 * IPA_RM_RESOURCE_GRANTED and IPA_RM_RESOURCE_RELEASED events.
 * IPA_RM_RESOURCE_GRANTED is handled in the context of shared
 * workqueue.
 *
 * @dev: network device
 * @event: IPA RM event
 * @data: Additional data provided by IPA RM
 *
 * Return codes:
 * None
 */
static void ipa3_rm_notify(void *dev, enum ipa_rm_event event,
			  unsigned long data)
{
	struct ipa3_wwan_private *wwan_ptr = netdev_priv(dev);

	pr_debug("%s: event %d\n", __func__, event);
	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		if (wwan_ptr->device_status == WWAN_DEVICE_INACTIVE) {
			complete_all(&wwan_ptr->resource_granted_completion);
			break;
		}
		ipa3_rm_resource_granted(dev);
		break;
	case IPA_RM_RESOURCE_RELEASED:
		break;
	default:
		pr_err("%s: unknown event %d\n", __func__, event);
		break;
	}
}

/* IPA_RM related functions end*/

static int ipa3_ssr_notifier_cb(struct notifier_block *this,
			   unsigned long code,
			   void *data);

static struct notifier_block ipa3_ssr_notifier = {
	.notifier_call = ipa3_ssr_notifier_cb,
};

static int get_ipa_rmnet_dts_configuration(struct platform_device *pdev,
		struct ipa3_rmnet_plat_drv_res *ipa_rmnet_drv_res)
{
	ipa_rmnet_drv_res->ipa_rmnet_ssr =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,rmnet-ipa-ssr");
	pr_info("IPA SSR support = %s\n",
		ipa_rmnet_drv_res->ipa_rmnet_ssr ? "True" : "False");
	ipa_rmnet_drv_res->ipa_loaduC =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-loaduC");
	pr_info("IPA ipa-loaduC = %s\n",
		ipa_rmnet_drv_res->ipa_loaduC ? "True" : "False");

	ipa_rmnet_drv_res->ipa_advertise_sg_support =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,ipa-advertise-sg-support");
	pr_info("IPA SG support = %s\n",
		ipa_rmnet_drv_res->ipa_advertise_sg_support ? "True" : "False");
	return 0;
}

struct ipa3_rmnet_context ipa3_rmnet_ctx;
static int ipa3_wwan_probe(struct platform_device *pdev);
struct platform_device *m_pdev;

static void ipa3_delayed_probe(struct work_struct *work)
{
	(void)ipa3_wwan_probe(m_pdev);
}

static DECLARE_WORK(ipa3_scheduled_probe, ipa3_delayed_probe);

static void ipa3_ready_cb(void *user_data)
{
	struct platform_device *pdev = (struct platform_device *)(user_data);

	m_pdev = pdev;

	IPAWANDBG("IPA ready callback has been triggered!\n");

	schedule_work(&ipa3_scheduled_probe);
}

/**
 * ipa3_wwan_probe() - Initialized the module and registers as a
 * network interface to the network stack
 *
 * Note: In case IPA driver hasn't initialized already, the probe function
 * will return immediately after registering a callback to be invoked when
 * IPA driver initialization is complete.
 *
 * Return codes:
 * 0: success
 * -ENOMEM: No memory available
 * -EFAULT: Internal error
 */
static int ipa3_wwan_probe(struct platform_device *pdev)
{
	int ret, i;
	struct net_device *dev;
	struct ipa_rm_create_params ipa_rm_params;	/* IPA_RM */
	struct ipa_rm_perf_profile profile;			/* IPA_RM */

	pr_info("rmnet_ipa3 started initialization\n");

	if (!ipa3_is_ready()) {
		IPAWANDBG("IPA driver not ready, registering callback\n");
		ret = ipa_register_ipa_ready_cb(ipa3_ready_cb, (void *)pdev);

		/*
		 * If we received -EEXIST, IPA has initialized. So we need
		 * to continue the probing process.
		 */
		if (ret != -EEXIST) {
			if (ret)
				IPAWANERR("IPA CB reg failed - %d\n", ret);
			return ret;
		}
	}

	ret = get_ipa_rmnet_dts_configuration(pdev, &ipa3_rmnet_res);
	ipa3_rmnet_ctx.ipa_rmnet_ssr = ipa3_rmnet_res.ipa_rmnet_ssr;

	ret = ipa3_init_q6_smem();
	if (ret) {
		IPAWANERR("ipa3_init_q6_smem failed!\n");
		return ret;
	}

	/* initialize tx/rx endpoint setup */
	memset(&rmnet_ipa3_ctx->apps_to_ipa_ep_cfg, 0,
		sizeof(struct ipa_sys_connect_params));
	memset(&rmnet_ipa3_ctx->ipa_to_apps_ep_cfg, 0,
		sizeof(struct ipa_sys_connect_params));

	/* initialize ex property setup */
	rmnet_ipa3_ctx->num_q6_rules = 0;
	rmnet_ipa3_ctx->old_num_q6_rules = 0;
	rmnet_ipa3_ctx->rmnet_index = 0;
	rmnet_ipa3_ctx->egress_set = false;
	rmnet_ipa3_ctx->a7_ul_flt_set = false;
	for (i = 0; i < MAX_NUM_OF_MUX_CHANNEL; i++)
		memset(&rmnet_ipa3_ctx->mux_channel[i], 0,
				sizeof(struct ipa3_rmnet_mux_val));

	/* start A7 QMI service/client */
	if (ipa3_rmnet_res.ipa_loaduC)
		/* Android platform loads uC */
		ipa3_qmi_service_init(QMI_IPA_PLATFORM_TYPE_MSM_ANDROID_V01);
	else
		/* LE platform not loads uC */
		ipa3_qmi_service_init(QMI_IPA_PLATFORM_TYPE_LE_V01);

	/* construct default WAN RT tbl for IPACM */
	ret = ipa3_setup_a7_qmap_hdr();
	if (ret)
		goto setup_a7_qmap_hdr_err;
	ret = ipa3_setup_dflt_wan_rt_tables();
	if (ret)
		goto setup_dflt_wan_rt_tables_err;

	if (!atomic_read(&rmnet_ipa3_ctx->is_ssr)) {
		/* Start transport-driver fd ioctl for ipacm for first init */
		ret = ipa3_wan_ioctl_init();
		if (ret)
			goto wan_ioctl_init_err;
	} else {
		/* Enable sending QMI messages after SSR */
		ipa3_wan_ioctl_enable_qmi_messages();
	}

	/* initialize wan-driver netdev */
	dev = alloc_netdev(sizeof(struct ipa3_wwan_private),
			   IPA_WWAN_DEV_NAME,
			   NET_NAME_UNKNOWN,
			   ipa3_wwan_setup);
	if (!dev) {
		IPAWANERR("no memory for netdev\n");
		ret = -ENOMEM;
		goto alloc_netdev_err;
	}
	rmnet_ipa3_ctx->wwan_priv = netdev_priv(dev);
	memset(rmnet_ipa3_ctx->wwan_priv, 0,
		sizeof(*(rmnet_ipa3_ctx->wwan_priv)));
	IPAWANDBG("wwan_ptr (private) = %p", rmnet_ipa3_ctx->wwan_priv);
	rmnet_ipa3_ctx->wwan_priv->net = dev;
	rmnet_ipa3_ctx->wwan_priv->outstanding_high = DEFAULT_OUTSTANDING_HIGH;
	rmnet_ipa3_ctx->wwan_priv->outstanding_low = DEFAULT_OUTSTANDING_LOW;
	atomic_set(&rmnet_ipa3_ctx->wwan_priv->outstanding_pkts, 0);
	spin_lock_init(&rmnet_ipa3_ctx->wwan_priv->lock);
	init_completion(
		&rmnet_ipa3_ctx->wwan_priv->resource_granted_completion);

	if (!atomic_read(&rmnet_ipa3_ctx->is_ssr)) {
		/* IPA_RM configuration starts */
		ret = ipa3_q6_initialize_rm();
		if (ret) {
			IPAWANERR("%s: ipa3_q6_initialize_rm failed, ret: %d\n",
				__func__, ret);
			goto q6_init_err;
		}
	}

	memset(&ipa_rm_params, 0, sizeof(struct ipa_rm_create_params));
	ipa_rm_params.name = IPA_RM_RESOURCE_WWAN_0_PROD;
	ipa_rm_params.reg_params.user_data = dev;
	ipa_rm_params.reg_params.notify_cb = ipa3_rm_notify;
	ret = ipa_rm_create_resource(&ipa_rm_params);
	if (ret) {
		pr_err("%s: unable to create resourse %d in IPA RM\n",
		       __func__, IPA_RM_RESOURCE_WWAN_0_PROD);
		goto create_rsrc_err;
	}
	ret = ipa_rm_inactivity_timer_init(IPA_RM_RESOURCE_WWAN_0_PROD,
					   IPA_RM_INACTIVITY_TIMER);
	if (ret) {
		pr_err("%s: ipa rm timer init failed %d on resourse %d\n",
		       __func__, ret, IPA_RM_RESOURCE_WWAN_0_PROD);
		goto timer_init_err;
	}
	/* add dependency */
	ret = ipa_rm_add_dependency(IPA_RM_RESOURCE_WWAN_0_PROD,
			IPA_RM_RESOURCE_Q6_CONS);
	if (ret)
		goto add_dpnd_err;
	/* setup Performance profile */
	memset(&profile, 0, sizeof(profile));
	profile.max_supported_bandwidth_mbps = IPA_APPS_MAX_BW_IN_MBPS;
	ret = ipa_rm_set_perf_profile(IPA_RM_RESOURCE_WWAN_0_PROD,
			&profile);
	if (ret)
		goto set_perf_err;
	/* IPA_RM configuration ends */

	/* Enable SG support in netdevice. */
	if (ipa3_rmnet_res.ipa_advertise_sg_support)
		dev->hw_features |= NETIF_F_SG;

	if (ipa3_rmnet_res.ipa_napi_enable)
		netif_napi_add(dev, &(rmnet_ipa3_ctx->wwan_priv->napi),
		       ipa3_rmnet_poll, NAPI_WEIGHT);
	ret = register_netdev(dev);
	if (ret) {
		IPAWANERR("unable to register ipa_netdev %d rc=%d\n",
			0, ret);
		goto set_perf_err;
	}

	IPAWANDBG("IPA-WWAN devices (%s) initialization ok :>>>>\n", dev->name);
	if (ret) {
		IPAWANERR("default configuration failed rc=%d\n",
				ret);
		goto config_err;
	}
	atomic_set(&rmnet_ipa3_ctx->is_initialized, 1);
	if (!atomic_read(&rmnet_ipa3_ctx->is_ssr)) {
		/* offline charging mode */
		ipa3_proxy_clk_unvote();
	}
	atomic_set(&rmnet_ipa3_ctx->is_ssr, 0);

	pr_info("rmnet_ipa completed initialization\n");
	return 0;
config_err:
	if (ipa3_rmnet_res.ipa_napi_enable)
		netif_napi_del(&(rmnet_ipa3_ctx->wwan_priv->napi));
	unregister_netdev(dev);
set_perf_err:
	ret = ipa_rm_delete_dependency(IPA_RM_RESOURCE_WWAN_0_PROD,
		IPA_RM_RESOURCE_Q6_CONS);
	if (ret)
		IPAWANERR("Error deleting dependency %d->%d, ret=%d\n",
			IPA_RM_RESOURCE_WWAN_0_PROD, IPA_RM_RESOURCE_Q6_CONS,
			ret);
add_dpnd_err:
	ret = ipa_rm_inactivity_timer_destroy(
		IPA_RM_RESOURCE_WWAN_0_PROD); /* IPA_RM */
	if (ret)
		IPAWANERR("Error ipa_rm_inactivity_timer_destroy %d, ret=%d\n",
		IPA_RM_RESOURCE_WWAN_0_PROD, ret);
timer_init_err:
	ret = ipa_rm_delete_resource(IPA_RM_RESOURCE_WWAN_0_PROD);
	if (ret)
		IPAWANERR("Error deleting resource %d, ret=%d\n",
		IPA_RM_RESOURCE_WWAN_0_PROD, ret);
create_rsrc_err:
	ipa3_q6_deinitialize_rm();
q6_init_err:
	free_netdev(dev);
	rmnet_ipa3_ctx->wwan_priv = NULL;
alloc_netdev_err:
	ipa3_wan_ioctl_deinit();
wan_ioctl_init_err:
	ipa3_del_dflt_wan_rt_tables();
setup_dflt_wan_rt_tables_err:
	ipa3_del_a7_qmap_hdr();
setup_a7_qmap_hdr_err:
	ipa3_qmi_service_exit();
	atomic_set(&rmnet_ipa3_ctx->is_ssr, 0);
	return ret;
}

static int ipa3_wwan_remove(struct platform_device *pdev)
{
	int ret;

	pr_info("rmnet_ipa started deinitialization\n");
	mutex_lock(&rmnet_ipa3_ctx->ipa_to_apps_pipe_handle_guard);
	ret = ipa3_teardown_sys_pipe(rmnet_ipa3_ctx->ipa3_to_apps_hdl);
	if (ret < 0)
		IPAWANERR("Failed to teardown IPA->APPS pipe\n");
	else
		rmnet_ipa3_ctx->ipa3_to_apps_hdl = -1;
	if (ipa3_rmnet_res.ipa_napi_enable)
		netif_napi_del(&(rmnet_ipa3_ctx->wwan_priv->napi));
	mutex_unlock(&rmnet_ipa3_ctx->ipa_to_apps_pipe_handle_guard);
	unregister_netdev(IPA_NETDEV());
	ret = ipa_rm_delete_dependency(IPA_RM_RESOURCE_WWAN_0_PROD,
		IPA_RM_RESOURCE_Q6_CONS);
	if (ret < 0)
		IPAWANERR("Error deleting dependency %d->%d, ret=%d\n",
			IPA_RM_RESOURCE_WWAN_0_PROD, IPA_RM_RESOURCE_Q6_CONS,
			ret);
	ret = ipa_rm_inactivity_timer_destroy(IPA_RM_RESOURCE_WWAN_0_PROD);
	if (ret < 0)
		IPAWANERR(
		"Error ipa_rm_inactivity_timer_destroy resource %d, ret=%d\n",
		IPA_RM_RESOURCE_WWAN_0_PROD, ret);
	ret = ipa_rm_delete_resource(IPA_RM_RESOURCE_WWAN_0_PROD);
	if (ret < 0)
		IPAWANERR("Error deleting resource %d, ret=%d\n",
		IPA_RM_RESOURCE_WWAN_0_PROD, ret);
	cancel_work_sync(&ipa3_tx_wakequeue_work);
	cancel_delayed_work(&ipa_tether_stats_poll_wakequeue_work);
	if (IPA_NETDEV())
		free_netdev(IPA_NETDEV());
	rmnet_ipa3_ctx->wwan_priv = NULL;
	/* No need to remove wwan_ioctl during SSR */
	if (!atomic_read(&rmnet_ipa3_ctx->is_ssr))
		ipa3_wan_ioctl_deinit();
	ipa3_del_dflt_wan_rt_tables();
	ipa3_del_a7_qmap_hdr();
	ipa3_del_mux_qmap_hdrs();
	if (ipa3_qmi_ctx->modem_cfg_emb_pipe_flt == false)
		ipa3_wwan_del_ul_flt_rule_to_ipa();
	ipa3_cleanup_deregister_intf();
	atomic_set(&rmnet_ipa3_ctx->is_initialized, 0);
	pr_info("rmnet_ipa completed deinitialization\n");
	return 0;
}

/**
* rmnet_ipa_ap_suspend() - suspend callback for runtime_pm
* @dev: pointer to device
*
* This callback will be invoked by the runtime_pm framework when an AP suspend
* operation is invoked, usually by pressing a suspend button.
*
* Returns -EAGAIN to runtime_pm framework in case there are pending packets
* in the Tx queue. This will postpone the suspend operation until all the
* pending packets will be transmitted.
*
* In case there are no packets to send, releases the WWAN0_PROD entity.
* As an outcome, the number of IPA active clients should be decremented
* until IPA clocks can be gated.
*/
static int rmnet_ipa_ap_suspend(struct device *dev)
{
	struct net_device *netdev = IPA_NETDEV();
	struct ipa3_wwan_private *wwan_ptr;

	IPAWANDBG_LOW("Enter...\n");
	if (netdev == NULL) {
		IPAWANERR("netdev is NULL.\n");
		return 0;
	}

	wwan_ptr = netdev_priv(netdev);
	if (wwan_ptr == NULL) {
		IPAWANERR("wwan_ptr is NULL.\n");
		return 0;
	}

	/* Do not allow A7 to suspend in case there are oustanding packets */
	if (atomic_read(&wwan_ptr->outstanding_pkts) != 0) {
		IPAWANDBG("Outstanding packets, postponing AP suspend.\n");
		return -EAGAIN;
	}

	/* Make sure that there is no Tx operation ongoing */
	netif_tx_lock_bh(netdev);
	ipa_rm_release_resource(IPA_RM_RESOURCE_WWAN_0_PROD);
	netif_tx_unlock_bh(netdev);
	IPAWANDBG_LOW("Exit\n");

	return 0;
}

/**
* rmnet_ipa_ap_resume() - resume callback for runtime_pm
* @dev: pointer to device
*
* This callback will be invoked by the runtime_pm framework when an AP resume
* operation is invoked.
*
* Enables the network interface queue and returns success to the
* runtime_pm framework.
*/
static int rmnet_ipa_ap_resume(struct device *dev)
{
	struct net_device *netdev = IPA_NETDEV();

	IPAWANDBG_LOW("Enter...\n");
	if (netdev)
		netif_wake_queue(netdev);
	IPAWANDBG_LOW("Exit\n");

	return 0;
}

static void ipa_stop_polling_stats(void)
{
	cancel_delayed_work(&ipa_tether_stats_poll_wakequeue_work);
	ipa3_rmnet_ctx.polling_interval = 0;
}

static const struct of_device_id rmnet_ipa_dt_match[] = {
	{.compatible = "qcom,rmnet-ipa3"},
	{},
};
MODULE_DEVICE_TABLE(of, rmnet_ipa_dt_match);

static const struct dev_pm_ops rmnet_ipa_pm_ops = {
	.suspend_noirq = rmnet_ipa_ap_suspend,
	.resume_noirq = rmnet_ipa_ap_resume,
};

static struct platform_driver rmnet_ipa_driver = {
	.driver = {
		.name = "rmnet_ipa3",
		.owner = THIS_MODULE,
		.pm = &rmnet_ipa_pm_ops,
		.of_match_table = rmnet_ipa_dt_match,
	},
	.probe = ipa3_wwan_probe,
	.remove = ipa3_wwan_remove,
};

static int ipa3_ssr_notifier_cb(struct notifier_block *this,
			   unsigned long code,
			   void *data)
{
	if (!ipa3_rmnet_ctx.ipa_rmnet_ssr)
		return NOTIFY_DONE;

	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		IPAWANINFO("IPA received MPSS BEFORE_SHUTDOWN\n");
		atomic_set(&rmnet_ipa3_ctx->is_ssr, 1);
		ipa3_q6_pre_shutdown_cleanup();
		if (IPA_NETDEV())
			netif_stop_queue(IPA_NETDEV());
		ipa3_qmi_stop_workqueues();
		ipa3_wan_ioctl_stop_qmi_messages();
		ipa_stop_polling_stats();
		if (atomic_read(&rmnet_ipa3_ctx->is_initialized))
			platform_driver_unregister(&rmnet_ipa_driver);
		IPAWANINFO("IPA BEFORE_SHUTDOWN handling is complete\n");
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		IPAWANINFO("IPA Received MPSS AFTER_SHUTDOWN\n");
		if (atomic_read(&rmnet_ipa3_ctx->is_ssr))
			ipa3_q6_post_shutdown_cleanup();
		IPAWANINFO("IPA AFTER_SHUTDOWN handling is complete\n");
		break;
	case SUBSYS_BEFORE_POWERUP:
		IPAWANINFO("IPA received MPSS BEFORE_POWERUP\n");
		if (atomic_read(&rmnet_ipa3_ctx->is_ssr))
			/* clean up cached QMI msg/handlers */
			ipa3_qmi_service_exit();
		/*hold a proxy vote for the modem*/
		ipa3_proxy_clk_vote();
		IPAWANINFO("IPA BEFORE_POWERUP handling is complete\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		IPAWANINFO("%s:%d IPA received MPSS AFTER_POWERUP\n",
			__func__, __LINE__);
		if (!atomic_read(&rmnet_ipa3_ctx->is_initialized) &&
		       atomic_read(&rmnet_ipa3_ctx->is_ssr))
			platform_driver_register(&rmnet_ipa_driver);

		IPAWANINFO("IPA AFTER_POWERUP handling is complete\n");
		break;
	default:
		IPAWANDBG("Unsupported subsys notification, IPA received: %lu",
			code);
		break;
	}

	IPAWANDBG_LOW("Exit\n");
	return NOTIFY_DONE;
}

/**
 * rmnet_ipa_free_msg() - Free the msg sent to user space via ipa_send_msg
 * @buff: pointer to buffer containing the message
 * @len: message len
 * @type: message type
 *
 * This function is invoked when ipa_send_msg is complete (Provided as a
 * free function pointer along with the message).
 */
static void rmnet_ipa_free_msg(void *buff, u32 len, u32 type)
{
	if (!buff) {
		IPAWANERR("Null buffer\n");
		return;
	}

	if (type != IPA_TETHERING_STATS_UPDATE_STATS &&
		type != IPA_TETHERING_STATS_UPDATE_NETWORK_STATS) {
		IPAWANERR("Wrong type given. buff %p type %d\n",
			  buff, type);
	}
	kfree(buff);
}

/**
 * rmnet_ipa_get_stats_and_update() - Gets pipe stats from Modem
 *
 * This function queries the IPA Modem driver for the pipe stats
 * via QMI, and updates the user space IPA entity.
 */
static void rmnet_ipa_get_stats_and_update(void)
{
	struct ipa_get_data_stats_req_msg_v01 req;
	struct ipa_get_data_stats_resp_msg_v01 *resp;
	struct ipa_msg_meta msg_meta;
	int rc;

	resp = kzalloc(sizeof(struct ipa_get_data_stats_resp_msg_v01),
		       GFP_KERNEL);
	if (!resp) {
		IPAWANERR("Can't allocate memory for stats message\n");
		return;
	}

	memset(&req, 0, sizeof(struct ipa_get_data_stats_req_msg_v01));
	memset(resp, 0, sizeof(struct ipa_get_data_stats_resp_msg_v01));

	req.ipa_stats_type = QMI_IPA_STATS_TYPE_PIPE_V01;

	rc = ipa3_qmi_get_data_stats(&req, resp);

	if (!rc) {
		memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
		msg_meta.msg_type = IPA_TETHERING_STATS_UPDATE_STATS;
		msg_meta.msg_len =
			sizeof(struct ipa_get_data_stats_resp_msg_v01);
		rc = ipa_send_msg(&msg_meta, resp, rmnet_ipa_free_msg);
		if (rc) {
			IPAWANERR("ipa_send_msg failed: %d\n", rc);
			kfree(resp);
			return;
		}
	}
}

/**
 * tethering_stats_poll_queue() - Stats polling function
 * @work - Work entry
 *
 * This function is scheduled periodically (per the interval) in
 * order to poll the IPA Modem driver for the pipe stats.
 */
static void tethering_stats_poll_queue(struct work_struct *work)
{
	rmnet_ipa_get_stats_and_update();

	/* Schedule again only if there's an active polling interval */
	if (ipa3_rmnet_ctx.polling_interval != 0)
		schedule_delayed_work(&ipa_tether_stats_poll_wakequeue_work,
			msecs_to_jiffies(ipa3_rmnet_ctx.polling_interval*1000));
}

/**
 * rmnet_ipa_get_network_stats_and_update() - Get network stats from IPA Modem
 *
 * This function retrieves the data usage (used quota) from the IPA Modem driver
 * via QMI, and updates IPA user space entity.
 */
static void rmnet_ipa_get_network_stats_and_update(void)
{
	struct ipa_get_apn_data_stats_req_msg_v01 req;
	struct ipa_get_apn_data_stats_resp_msg_v01 *resp;
	struct ipa_msg_meta msg_meta;
	int rc;

	resp = kzalloc(sizeof(struct ipa_get_apn_data_stats_resp_msg_v01),
		       GFP_KERNEL);
	if (!resp) {
		IPAWANERR("Can't allocate memory for network stats message\n");
		return;
	}

	memset(&req, 0, sizeof(struct ipa_get_apn_data_stats_req_msg_v01));
	memset(resp, 0, sizeof(struct ipa_get_apn_data_stats_resp_msg_v01));

	req.mux_id_list_valid = true;
	req.mux_id_list_len = 1;
	req.mux_id_list[0] = ipa3_rmnet_ctx.metered_mux_id;

	rc = ipa3_qmi_get_network_stats(&req, resp);

	if (!rc) {
		memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
		msg_meta.msg_type = IPA_TETHERING_STATS_UPDATE_NETWORK_STATS;
		msg_meta.msg_len =
			sizeof(struct ipa_get_apn_data_stats_resp_msg_v01);
		rc = ipa_send_msg(&msg_meta, resp, rmnet_ipa_free_msg);
		if (rc) {
			IPAWANERR("ipa_send_msg failed: %d\n", rc);
			kfree(resp);
			return;
		}
	}
}

/**
 * rmnet_ipa3_poll_tethering_stats() - Tethering stats polling IOCTL handler
 * @data - IOCTL data
 *
 * This function handles WAN_IOC_POLL_TETHERING_STATS.
 * In case polling interval received is 0, polling will stop
 * (If there's a polling in progress, it will allow it to finish), and then will
 * fetch network stats, and update the IPA user space.
 *
 * Return codes:
 * 0: Success
 */
int rmnet_ipa3_poll_tethering_stats(struct wan_ioctl_poll_tethering_stats *data)
{
	ipa3_rmnet_ctx.polling_interval = data->polling_interval_secs;

	cancel_delayed_work_sync(&ipa_tether_stats_poll_wakequeue_work);

	if (ipa3_rmnet_ctx.polling_interval == 0) {
		ipa3_qmi_stop_data_qouta();
		rmnet_ipa_get_network_stats_and_update();
		rmnet_ipa_get_stats_and_update();
		return 0;
	}

	schedule_delayed_work(&ipa_tether_stats_poll_wakequeue_work, 0);
	return 0;
}

/**
 * rmnet_ipa_set_data_quota() - Data quota setting IOCTL handler
 * @data - IOCTL data
 *
 * This function handles WAN_IOC_SET_DATA_QUOTA.
 * It translates the given interface name to the Modem MUX ID and
 * sends the request of the quota to the IPA Modem driver via QMI.
 *
 * Return codes:
 * 0: Success
 * -EFAULT: Invalid interface name provided
 * other: See ipa_qmi_set_data_quota
 */
int rmnet_ipa3_set_data_quota(struct wan_ioctl_set_data_quota *data)
{
	u32 mux_id;
	int index;
	struct ipa_set_data_usage_quota_req_msg_v01 req;

	index = find_vchannel_name_index(data->interface_name);
	IPAWANERR("iface name %s, quota %lu\n",
		  data->interface_name,
		  (unsigned long int) data->quota_mbytes);

	if (index == MAX_NUM_OF_MUX_CHANNEL) {
		IPAWANERR("%s is an invalid iface name\n",
			  data->interface_name);
		return -EFAULT;
	}

	mux_id = rmnet_ipa3_ctx->mux_channel[index].mux_id;
	ipa3_rmnet_ctx.metered_mux_id = mux_id;

	memset(&req, 0, sizeof(struct ipa_set_data_usage_quota_req_msg_v01));
	req.apn_quota_list_valid = true;
	req.apn_quota_list_len = 1;
	req.apn_quota_list[0].mux_id = mux_id;
	req.apn_quota_list[0].num_Mbytes = data->quota_mbytes;

	return ipa3_qmi_set_data_quota(&req);
}

 /* rmnet_ipa_set_tether_client_pipe() -
 * @data - IOCTL data
 *
 * This function handles WAN_IOC_SET_DATA_QUOTA.
 * It translates the given interface name to the Modem MUX ID and
 * sends the request of the quota to the IPA Modem driver via QMI.
 *
 * Return codes:
 * 0: Success
 * -EFAULT: Invalid interface name provided
 * other: See ipa_qmi_set_data_quota
 */
int rmnet_ipa3_set_tether_client_pipe(
	struct wan_ioctl_set_tether_client_pipe *data)
{
	int number, i;

	IPAWANDBG("client %d, UL %d, DL %d, reset %d\n",
	data->ipa_client,
	data->ul_src_pipe_len,
	data->dl_dst_pipe_len,
	data->reset_client);
	number = data->ul_src_pipe_len;
	for (i = 0; i < number; i++) {
		IPAWANDBG("UL index-%d pipe %d\n", i,
			data->ul_src_pipe_list[i]);
		if (data->reset_client)
			ipa3_set_client(data->ul_src_pipe_list[i],
				0, false);
		else
			ipa3_set_client(data->ul_src_pipe_list[i],
				data->ipa_client, true);
	}
	number = data->dl_dst_pipe_len;
	for (i = 0; i < number; i++) {
		IPAWANDBG("DL index-%d pipe %d\n", i,
			data->dl_dst_pipe_list[i]);
		if (data->reset_client)
			ipa3_set_client(data->dl_dst_pipe_list[i],
				0, false);
		else
			ipa3_set_client(data->dl_dst_pipe_list[i],
				data->ipa_client, false);
	}
	return 0;
}

int rmnet_ipa3_query_tethering_stats(struct wan_ioctl_query_tether_stats *data,
	bool reset)
{
	struct ipa_get_data_stats_req_msg_v01 *req;
	struct ipa_get_data_stats_resp_msg_v01 *resp;
	int pipe_len, rc;

	req = kzalloc(sizeof(struct ipa_get_data_stats_req_msg_v01),
			GFP_KERNEL);
	if (!req) {
		IPAWANERR("Can't allocate memory for stats message\n");
		return -ENOMEM;
	}
	resp = kzalloc(sizeof(struct ipa_get_data_stats_resp_msg_v01),
			GFP_KERNEL);
	if (!resp) {
		IPAWANERR("Can't allocate memory for stats message\n");
		kfree(req);
		return -ENOMEM;
	}
	memset(req, 0, sizeof(struct ipa_get_data_stats_req_msg_v01));
	memset(resp, 0, sizeof(struct ipa_get_data_stats_resp_msg_v01));

	req->ipa_stats_type = QMI_IPA_STATS_TYPE_PIPE_V01;
	if (reset) {
		req->reset_stats_valid = true;
		req->reset_stats = true;
		IPAWANERR("reset the pipe stats\n");
	} else {
		/* print tethered-client enum */
		IPAWANDBG_LOW("Tethered-client enum(%d)\n", data->ipa_client);
	}

	rc = ipa3_qmi_get_data_stats(req, resp);
	if (rc) {
		IPAWANERR("can't get ipa_qmi_get_data_stats\n");
		kfree(req);
		kfree(resp);
		return rc;
	} else if (reset) {
		kfree(req);
		kfree(resp);
		return 0;
	}

	if (resp->dl_dst_pipe_stats_list_valid) {
		for (pipe_len = 0; pipe_len < resp->dl_dst_pipe_stats_list_len;
			pipe_len++) {
			IPAWANDBG_LOW("Check entry(%d) dl_dst_pipe(%d)\n",
				pipe_len, resp->dl_dst_pipe_stats_list
					[pipe_len].pipe_index);
			IPAWANDBG_LOW("dl_p_v4(%lu)v6(%lu)\n",
				(unsigned long int) resp->
				dl_dst_pipe_stats_list[pipe_len].
				num_ipv4_packets,
				(unsigned long int) resp->
				dl_dst_pipe_stats_list[pipe_len].
				num_ipv6_packets);
			IPAWANDBG_LOW("dl_b_v4(%lu)v6(%lu)\n",
				(unsigned long int) resp->
				dl_dst_pipe_stats_list[pipe_len].
				num_ipv4_bytes,
				(unsigned long int) resp->
				dl_dst_pipe_stats_list[pipe_len].
				num_ipv6_bytes);
			if (ipa_get_client_uplink(resp->
				dl_dst_pipe_stats_list[pipe_len].
				pipe_index) == false) {
				if (data->ipa_client == ipa_get_client(resp->
					dl_dst_pipe_stats_list[pipe_len].
					pipe_index)) {
					/* update the DL stats */
					data->ipv4_rx_packets += resp->
					dl_dst_pipe_stats_list[pipe_len].
					num_ipv4_packets;
					data->ipv6_rx_packets += resp->
					dl_dst_pipe_stats_list[pipe_len].
					num_ipv6_packets;
					data->ipv4_rx_bytes += resp->
					dl_dst_pipe_stats_list[pipe_len].
					num_ipv4_bytes;
					data->ipv6_rx_bytes += resp->
					dl_dst_pipe_stats_list[pipe_len].
					num_ipv6_bytes;
				}
			}
		}
	}
	IPAWANDBG_LOW("v4_rx_p(%lu) v6_rx_p(%lu) v4_rx_b(%lu) v6_rx_b(%lu)\n",
		(unsigned long int) data->ipv4_rx_packets,
		(unsigned long int) data->ipv6_rx_packets,
		(unsigned long int) data->ipv4_rx_bytes,
		(unsigned long int) data->ipv6_rx_bytes);

	if (resp->ul_src_pipe_stats_list_valid) {
		for (pipe_len = 0; pipe_len < resp->ul_src_pipe_stats_list_len;
			pipe_len++) {
			IPAWANDBG_LOW("Check entry(%d) ul_dst_pipe(%d)\n",
				pipe_len,
				resp->ul_src_pipe_stats_list[pipe_len].
				pipe_index);
			IPAWANDBG_LOW("ul_p_v4(%lu)v6(%lu)\n",
				(unsigned long int) resp->
				ul_src_pipe_stats_list[pipe_len].
				num_ipv4_packets,
				(unsigned long int) resp->
				ul_src_pipe_stats_list[pipe_len].
				num_ipv6_packets);
			IPAWANDBG_LOW("ul_b_v4(%lu)v6(%lu)\n",
				(unsigned long int) resp->
				ul_src_pipe_stats_list[pipe_len].
				num_ipv4_bytes,
				(unsigned long int) resp->
				ul_src_pipe_stats_list[pipe_len].
				num_ipv6_bytes);
			if (ipa_get_client_uplink(resp->
				ul_src_pipe_stats_list[pipe_len].
				pipe_index) == true) {
				if (data->ipa_client == ipa_get_client(resp->
				ul_src_pipe_stats_list[pipe_len].
				pipe_index)) {
					/* update the DL stats */
					data->ipv4_tx_packets += resp->
					ul_src_pipe_stats_list[pipe_len].
					num_ipv4_packets;
					data->ipv6_tx_packets += resp->
					ul_src_pipe_stats_list[pipe_len].
					num_ipv6_packets;
					data->ipv4_tx_bytes += resp->
					ul_src_pipe_stats_list[pipe_len].
					num_ipv4_bytes;
					data->ipv6_tx_bytes += resp->
					ul_src_pipe_stats_list[pipe_len].
					num_ipv6_bytes;
				}
			}
		}
	}
	IPAWANDBG_LOW("tx_p_v4(%lu)v6(%lu)tx_b_v4(%lu) v6(%lu)\n",
		(unsigned long int) data->ipv4_tx_packets,
		(unsigned long  int) data->ipv6_tx_packets,
		(unsigned long int) data->ipv4_tx_bytes,
		(unsigned long int) data->ipv6_tx_bytes);
	kfree(req);
	kfree(resp);
	return 0;
}

/**
 * ipa3_broadcast_quota_reach_ind() - Send Netlink broadcast on Quota
 * @mux_id - The MUX ID on which the quota has been reached
 *
 * This function broadcasts a Netlink event using the kobject of the
 * rmnet_ipa interface in order to alert the user space that the quota
 * on the specific interface which matches the mux_id has been reached.
 *
 */
void ipa3_broadcast_quota_reach_ind(u32 mux_id)
{
	char alert_msg[IPA_QUOTA_REACH_ALERT_MAX_SIZE];
	char iface_name_m[IPA_QUOTA_REACH_IF_NAME_MAX_SIZE];
	char iface_name_l[IPA_QUOTA_REACH_IF_NAME_MAX_SIZE];
	char *envp[IPA_UEVENT_NUM_EVNP] = {
		alert_msg, iface_name_l, iface_name_m, NULL };
	int res;
	int index;

	index = ipa3_find_mux_channel_index(mux_id);

	if (index == MAX_NUM_OF_MUX_CHANNEL) {
		IPAWANERR("%u is an mux ID\n", mux_id);
		return;
	}

	res = snprintf(alert_msg, IPA_QUOTA_REACH_ALERT_MAX_SIZE,
			"ALERT_NAME=%s", "quotaReachedAlert");
	if (res >= IPA_QUOTA_REACH_ALERT_MAX_SIZE) {
		IPAWANERR("message too long (%d)", res);
		return;
	}
	/* posting msg for L-release for CNE */
	res = snprintf(iface_name_l, IPA_QUOTA_REACH_IF_NAME_MAX_SIZE,
	    "UPSTREAM=%s", rmnet_ipa3_ctx->mux_channel[index].vchannel_name);
	if (res >= IPA_QUOTA_REACH_IF_NAME_MAX_SIZE) {
		IPAWANERR("message too long (%d)", res);
		return;
	}
	/* posting msg for M-release for CNE */
	res = snprintf(iface_name_m, IPA_QUOTA_REACH_IF_NAME_MAX_SIZE,
	    "INTERFACE=%s", rmnet_ipa3_ctx->mux_channel[index].vchannel_name);
	if (res >= IPA_QUOTA_REACH_IF_NAME_MAX_SIZE) {
		IPAWANERR("message too long (%d)", res);
		return;
	}

	IPAWANERR("putting nlmsg: <%s> <%s> <%s>\n",
		alert_msg, iface_name_l, iface_name_m);
	kobject_uevent_env(&(IPA_NETDEV()->dev.kobj),
		KOBJ_CHANGE, envp);
}

/**
 * ipa3_q6_handshake_complete() - Perform operations once Q6 is up
 * @ssr_bootup - Indicates whether this is a cold boot-up or post-SSR.
 *
 * This function is invoked once the handshake between the IPA AP driver
 * and IPA Q6 driver is complete. At this point, it is possible to perform
 * operations which can't be performed until IPA Q6 driver is up.
 *
 */
void ipa3_q6_handshake_complete(bool ssr_bootup)
{
	/* It is required to recover the network stats after SSR recovery */
	if (ssr_bootup) {
		/*
		 * In case the uC is required to be loaded by the Modem,
		 * the proxy vote will be removed only when uC loading is
		 * complete and indication is received by the AP. After SSR,
		 * uC is already loaded. Therefore, proxy vote can be removed
		 * once Modem init is complete.
		 */
		ipa3_proxy_clk_unvote();

		/*
		 * It is required to recover the network stats after
		 * SSR recovery
		 */
		rmnet_ipa_get_network_stats_and_update();
	}
}

static int __init ipa3_wwan_init(void)
{
	rmnet_ipa3_ctx = kzalloc(sizeof(*rmnet_ipa3_ctx), GFP_KERNEL);
	if (!rmnet_ipa3_ctx) {
		IPAWANERR("no memory\n");
		return -ENOMEM;
	}

	atomic_set(&rmnet_ipa3_ctx->is_initialized, 0);
	atomic_set(&rmnet_ipa3_ctx->is_ssr, 0);

	mutex_init(&rmnet_ipa3_ctx->ipa_to_apps_pipe_handle_guard);
	rmnet_ipa3_ctx->ipa3_to_apps_hdl = -1;
	/* Register for Modem SSR */
	rmnet_ipa3_ctx->subsys_notify_handle = subsys_notif_register_notifier(
			SUBSYS_MODEM,
			&ipa3_ssr_notifier);
	if (!IS_ERR(rmnet_ipa3_ctx->subsys_notify_handle))
		return platform_driver_register(&rmnet_ipa_driver);
	else
		return (int)PTR_ERR(rmnet_ipa3_ctx->subsys_notify_handle);
}

static void __exit ipa3_wwan_cleanup(void)
{
	int ret;

	mutex_destroy(&rmnet_ipa3_ctx->ipa_to_apps_pipe_handle_guard);
	ret = subsys_notif_unregister_notifier(
		rmnet_ipa3_ctx->subsys_notify_handle, &ipa3_ssr_notifier);
	if (ret)
		IPAWANERR(
		"Error subsys_notif_unregister_notifier system %s, ret=%d\n",
		SUBSYS_MODEM, ret);
	platform_driver_unregister(&rmnet_ipa_driver);
	kfree(rmnet_ipa3_ctx);
	rmnet_ipa3_ctx = NULL;
}

static void ipa3_wwan_msg_free_cb(void *buff, u32 len, u32 type)
{
	if (!buff)
		IPAWANERR("Null buffer.\n");
	kfree(buff);
}

static void ipa3_rmnet_rx_cb(void *priv)
{
	IPAWANDBG_LOW("\n");
	napi_schedule(&(rmnet_ipa3_ctx->wwan_priv->napi));
}

static int ipa3_rmnet_poll(struct napi_struct *napi, int budget)
{
	int rcvd_pkts = 0;

	rcvd_pkts = ipa_rx_poll(rmnet_ipa3_ctx->ipa3_to_apps_hdl,
					NAPI_WEIGHT);
	IPAWANDBG_LOW("rcvd packets: %d\n", rcvd_pkts);
	return rcvd_pkts;
}

late_initcall(ipa3_wwan_init);
module_exit(ipa3_wwan_cleanup);
MODULE_DESCRIPTION("WWAN Network Interface");
MODULE_LICENSE("GPL v2");
