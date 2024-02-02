/* Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
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
#include <uapi/linux/msm_rmnet.h>
#include <net/rmnet_config.h>
#include "ipa_mhi_proxy.h"

#include "ipa_trace.h"
#include "ipa_odl.h"

#define OUTSTANDING_HIGH_DEFAULT 256
#define OUTSTANDING_HIGH_CTL_DEFAULT (OUTSTANDING_HIGH_DEFAULT + 32)
#define OUTSTANDING_LOW_DEFAULT 128

static unsigned int outstanding_high = OUTSTANDING_HIGH_DEFAULT;
module_param(outstanding_high, uint, 0644);
MODULE_PARM_DESC(outstanding_high, "Outstanding high");

static unsigned int outstanding_high_ctl = OUTSTANDING_HIGH_CTL_DEFAULT;
module_param(outstanding_high_ctl, uint, 0644);
MODULE_PARM_DESC(outstanding_high_ctl, "Outstanding high control");

static unsigned int outstanding_low = OUTSTANDING_LOW_DEFAULT;
module_param(outstanding_low, uint, 0644);
MODULE_PARM_DESC(outstanding_low, "Outstanding low");

#define WWAN_METADATA_SHFT 24
#define WWAN_METADATA_MASK 0xFF000000
#define WWAN_DATA_LEN 9216
#define IPA_RM_INACTIVITY_TIMER 100 /* IPA_RM */
#define HEADROOM_FOR_QMAP   8 /* for mux header */
#define TAILROOM            0 /* for padding by mux layer */
#define MAX_NUM_OF_MUX_CHANNEL  15 /* max mux channels */
#define UL_FILTER_RULE_HANDLE_START 69

#define IPA_WWAN_DEV_NAME "rmnet_ipa%d"
#define IPA_UPSTEAM_WLAN_IFACE_NAME "wlan0"
#define IPA_UPSTEAM_WLAN1_IFACE_NAME "wlan1"

#define IPA_WWAN_RX_SOFTIRQ_THRESH 16

#define INVALID_MUX_ID 0xFF
#define IPA_QUOTA_REACH_ALERT_MAX_SIZE 64
#define IPA_QUOTA_REACH_IF_NAME_MAX_SIZE 64
#define IPA_UEVENT_NUM_EVNP 4 /* number of event pointers */

#define IPA_NETDEV() \
	((rmnet_ipa3_ctx && rmnet_ipa3_ctx->wwan_priv) ? \
	  rmnet_ipa3_ctx->wwan_priv->net : NULL)

#define IPA_WWAN_CONS_DESC_FIFO_SZ 256

#define LAN_STATS_FOR_ALL_CLIENTS 0xFFFFFFFF

static void rmnet_ipa_free_msg(void *buff, u32 len, u32 type);
static void rmnet_ipa_get_stats_and_update(void);

static int ipa3_wwan_add_ul_flt_rule_to_ipa(void);
static int ipa3_wwan_del_ul_flt_rule_to_ipa(void);
static void ipa3_wwan_msg_free_cb(void*, u32, u32);
static int ipa3_rmnet_poll(struct napi_struct *napi, int budget);

static void ipa3_wake_tx_queue(struct work_struct *work);
static DECLARE_WORK(ipa3_tx_wakequeue_work, ipa3_wake_tx_queue);

static void tethering_stats_poll_queue(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa_tether_stats_poll_wakequeue_work,
			    tethering_stats_poll_queue);

static int rmnet_ipa_send_coalesce_notification(uint8_t qmap_id, bool enable,
					bool tcp, bool udp);

enum ipa3_wwan_device_status {
	WWAN_DEVICE_INACTIVE = 0,
	WWAN_DEVICE_ACTIVE   = 1
};

struct ipa3_rmnet_plat_drv_res {
	bool ipa_rmnet_ssr;
	bool is_platform_type_msm;
	bool ipa_advertise_sg_support;
	bool ipa_napi_enable;
	u32 wan_rx_desc_size;
};

/**
 * struct ipa3_wwan_private - WWAN private data
 * @net: network interface struct implemented by this driver
 * @stats: iface statistics
 * @outstanding_pkts: number of packets sent to IPA without TX complete ACKed
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
	void *lcl_mdm_subsys_notify_handle;
	void *rmt_mdm_subsys_notify_handle;
	u32 apps_to_ipa3_hdl;
	u32 ipa3_to_apps_hdl;
	struct mutex pipe_handle_guard;
	struct mutex add_mux_channel_lock;
	u32 pm_hdl;
	u32 q6_pm_hdl;
	u32 q6_teth_pm_hdl;
	struct mutex per_client_stats_guard;
	struct ipa_tether_device_info
		tether_device
		[IPACM_MAX_CLIENT_DEVICE_TYPES];
	bool dl_csum_offload_enabled;
	atomic_t ap_suspend;
	bool ipa_config_is_apq;
	bool ipa_mhi_aggr_formet_set;
	bool no_qmap_config;
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
	if (!hdr)
		return -ENOMEM;

	hdr->num_hdrs = 1;
	hdr->commit = 1;
	hdr_entry = &hdr->hdr[0];

	strlcpy(hdr_entry->name, IPA_A7_QMAP_HDR_NAME,
				IPA_RESOURCE_NAME_MAX);
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5 &&
		rmnet_ipa3_ctx->dl_csum_offload_enabled) {
		hdr_entry->hdr_len = IPA_DL_CHECKSUM_LENGTH; /* 8 bytes */
		/* new DL QMAP header format */
		hdr_entry->hdr[0] = 0x40;
		hdr_entry->hdr[1] = 0;
		hdr_entry->hdr[2] = 0;
		hdr_entry->hdr[3] = 0;
		hdr_entry->hdr[4] = 0x4;
		/*
		 * Need to set csum required/valid bit on which will be replaced
		 * by HW if checksum is incorrect after validation
		 */
		hdr_entry->hdr[5] = 0x80;
		hdr_entry->hdr[6] = 0;
		hdr_entry->hdr[7] = 0;
	} else
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
	if (!hdr)
		return -ENOMEM;

	hdr->num_hdrs = 1;
	hdr->commit = 1;
	hdr_entry = &hdr->hdr[0];

	snprintf(hdr_name, IPA_RESOURCE_NAME_MAX, "%s%d",
		 A2_MUX_HDR_NAME_V4_PREF,
		 mux_id);
	 strlcpy(hdr_entry->name, hdr_name,
				IPA_RESOURCE_NAME_MAX);

	if (rmnet_ipa3_ctx->dl_csum_offload_enabled) {
		if (rmnet_ipa3_ctx->ipa_config_is_apq ||
			ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5) {
			hdr_entry->hdr_len =
				IPA_DL_CHECKSUM_LENGTH; /* 8 bytes */
			/* new DL QMAP header format */
			hdr_entry->hdr[0] = 0x40;
			hdr_entry->hdr[1] = (uint8_t) mux_id;
			hdr_entry->hdr[2] = 0;
			hdr_entry->hdr[3] = 0;
			hdr_entry->hdr[4] = 0x4;
			/*
			 * Need to set csum required/valid bit on
			 * which will be replaced by HW if checksum
			 * is incorrect after validation
			 */
			hdr_entry->hdr[5] = 0x80;
			hdr_entry->hdr[6] = 0;
			hdr_entry->hdr[7] = 0;
		} else {
			hdr_entry->hdr_len =
				IPA_QMAP_HEADER_LENGTH; /* 4 bytes */
			hdr_entry->hdr[1] = (uint8_t) mux_id;
		}
	} else {
		hdr_entry->hdr_len = IPA_QMAP_HEADER_LENGTH; /* 4 bytes */
		hdr_entry->hdr[1] = (uint8_t) mux_id;
	}

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
	if (!rt_rule)
		return -ENOMEM;

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
	if (!rt_rule)
		return;

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
		IPAWANERR("Routing rule deletion failed\n");
	}

	rt_rule->ip = IPA_IP_v6;
	rt_rule_entry->hdl = rmnet_ipa3_ctx->dflt_v6_wan_rt_hdl;
	IPAWANERR("Deleting Route hdl:(0x%x) with ip type: %d\n",
		rt_rule_entry->hdl, IPA_IP_v6);
	if (ipa3_del_rt_rule(rt_rule) ||
			(rt_rule_entry->status)) {
		IPAWANERR("Routing rule deletion failed\n");
	}

	kfree(rt_rule);
}

static void ipa3_copy_qmi_flt_rule_ex(
	struct ipa_ioc_ext_intf_prop *q6_ul_flt_rule_ptr,
	void *flt_spec_ptr_void)
{
	int j;
	struct ipa_filter_spec_ex_type_v01 *flt_spec_ptr;
	struct ipa_ipfltr_range_eq_16 *q6_ul_filter_nat_ptr;
	struct ipa_ipfltr_range_eq_16_type_v01 *filter_spec_nat_ptr;

	/*
	 * pure_ack and tos has the same size and type and we will treat tos
	 * field as pure_ack in ipa4.5 version
	 */
	flt_spec_ptr = (struct ipa_filter_spec_ex_type_v01 *) flt_spec_ptr_void;

	q6_ul_flt_rule_ptr->ip = flt_spec_ptr->ip_type;
	q6_ul_flt_rule_ptr->action = flt_spec_ptr->filter_action;
	if (flt_spec_ptr->is_routing_table_index_valid == true)
		q6_ul_flt_rule_ptr->rt_tbl_idx =
		flt_spec_ptr->route_table_index;
	if (flt_spec_ptr->is_mux_id_valid == true)
		q6_ul_flt_rule_ptr->mux_id =
		flt_spec_ptr->mux_id;
	q6_ul_flt_rule_ptr->rule_id =
		flt_spec_ptr->rule_id;
	q6_ul_flt_rule_ptr->is_rule_hashable =
		flt_spec_ptr->is_rule_hashable;
	q6_ul_flt_rule_ptr->eq_attrib.rule_eq_bitmap =
		flt_spec_ptr->filter_rule.rule_eq_bitmap;
	q6_ul_flt_rule_ptr->eq_attrib.tos_eq_present =
		flt_spec_ptr->filter_rule.tos_eq_present;
	q6_ul_flt_rule_ptr->eq_attrib.tos_eq =
		flt_spec_ptr->filter_rule.tos_eq;
	q6_ul_flt_rule_ptr->eq_attrib.protocol_eq_present =
		flt_spec_ptr->filter_rule.protocol_eq_present;
	q6_ul_flt_rule_ptr->eq_attrib.protocol_eq =
		flt_spec_ptr->filter_rule.protocol_eq;
	q6_ul_flt_rule_ptr->eq_attrib.num_ihl_offset_range_16 =
		flt_spec_ptr->filter_rule.num_ihl_offset_range_16;

	for (j = 0;
		j < q6_ul_flt_rule_ptr->eq_attrib.num_ihl_offset_range_16;
		j++) {
		q6_ul_filter_nat_ptr =
			&q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_range_16[j];
		filter_spec_nat_ptr =
			&flt_spec_ptr->filter_rule.ihl_offset_range_16[j];
		q6_ul_filter_nat_ptr->offset =
			filter_spec_nat_ptr->offset;
		q6_ul_filter_nat_ptr->range_low =
			filter_spec_nat_ptr->range_low;
		q6_ul_filter_nat_ptr->range_high =
			filter_spec_nat_ptr->range_high;
	}
	q6_ul_flt_rule_ptr->eq_attrib.num_offset_meq_32 =
		flt_spec_ptr->filter_rule.num_offset_meq_32;
	for (j = 0;
		j < q6_ul_flt_rule_ptr->eq_attrib.num_offset_meq_32;
		j++) {
		q6_ul_flt_rule_ptr->eq_attrib.offset_meq_32[j].offset =
			flt_spec_ptr->filter_rule.offset_meq_32[j].offset;
		q6_ul_flt_rule_ptr->eq_attrib.offset_meq_32[j].mask =
			flt_spec_ptr->filter_rule.offset_meq_32[j].mask;
		q6_ul_flt_rule_ptr->eq_attrib.offset_meq_32[j].value =
			flt_spec_ptr->filter_rule.offset_meq_32[j].value;
	}

	q6_ul_flt_rule_ptr->eq_attrib.tc_eq_present =
		flt_spec_ptr->filter_rule.tc_eq_present;
	q6_ul_flt_rule_ptr->eq_attrib.tc_eq =
		flt_spec_ptr->filter_rule.tc_eq;
	q6_ul_flt_rule_ptr->eq_attrib.fl_eq_present =
		flt_spec_ptr->filter_rule.flow_eq_present;
	q6_ul_flt_rule_ptr->eq_attrib.fl_eq =
		flt_spec_ptr->filter_rule.flow_eq;
	q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_eq_16_present =
		flt_spec_ptr->filter_rule.ihl_offset_eq_16_present;
	q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_eq_16.offset =
		flt_spec_ptr->filter_rule.ihl_offset_eq_16.offset;
	q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_eq_16.value =
		flt_spec_ptr->filter_rule.ihl_offset_eq_16.value;

	q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_eq_32_present =
		flt_spec_ptr->filter_rule.ihl_offset_eq_32_present;
	q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_eq_32.offset =
		flt_spec_ptr->filter_rule.ihl_offset_eq_32.offset;
	q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_eq_32.value =
		flt_spec_ptr->filter_rule.ihl_offset_eq_32.value;

	q6_ul_flt_rule_ptr->eq_attrib.num_ihl_offset_meq_32 =
		flt_spec_ptr->filter_rule.num_ihl_offset_meq_32;
	for (j = 0;
		j < q6_ul_flt_rule_ptr->eq_attrib.num_ihl_offset_meq_32;
		j++) {
		q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_meq_32[j].offset =
			flt_spec_ptr->filter_rule.ihl_offset_meq_32[j].offset;
		q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_meq_32[j].mask =
			flt_spec_ptr->filter_rule.ihl_offset_meq_32[j].mask;
		q6_ul_flt_rule_ptr->eq_attrib.ihl_offset_meq_32[j].value =
			flt_spec_ptr->filter_rule.ihl_offset_meq_32[j].value;
	}
	q6_ul_flt_rule_ptr->eq_attrib.num_offset_meq_128 =
		flt_spec_ptr->filter_rule.num_offset_meq_128;
	for (j = 0;
		j < q6_ul_flt_rule_ptr->eq_attrib.num_offset_meq_128;
		j++) {
		q6_ul_flt_rule_ptr->eq_attrib.offset_meq_128[j].offset =
			flt_spec_ptr->filter_rule.offset_meq_128[j].offset;
		memcpy(q6_ul_flt_rule_ptr->eq_attrib.offset_meq_128[j].mask,
			flt_spec_ptr->filter_rule.offset_meq_128[j].mask, 16);
		memcpy(q6_ul_flt_rule_ptr->eq_attrib.offset_meq_128[j].value,
			flt_spec_ptr->filter_rule.offset_meq_128[j].value, 16);
	}

	q6_ul_flt_rule_ptr->eq_attrib.metadata_meq32_present =
		flt_spec_ptr->filter_rule.metadata_meq32_present;
	q6_ul_flt_rule_ptr->eq_attrib.metadata_meq32.offset =
		flt_spec_ptr->filter_rule.metadata_meq32.offset;
	q6_ul_flt_rule_ptr->eq_attrib.metadata_meq32.mask =
		flt_spec_ptr->filter_rule.metadata_meq32.mask;
	q6_ul_flt_rule_ptr->eq_attrib.metadata_meq32.value =
		flt_spec_ptr->filter_rule.metadata_meq32.value;
	q6_ul_flt_rule_ptr->eq_attrib.ipv4_frag_eq_present =
		flt_spec_ptr->filter_rule.ipv4_frag_eq_present;
}

int ipa3_copy_ul_filter_rule_to_ipa(struct ipa_install_fltr_rule_req_msg_v01
		*rule_req)
{
	int i;

	/* prevent multi-threads accessing rmnet_ipa3_ctx->num_q6_rules */
	mutex_lock(&rmnet_ipa3_ctx->add_mux_channel_lock);
	if (rule_req->filter_spec_ex_list_valid == true &&
		rule_req->filter_spec_ex2_list_valid == false) {
		rmnet_ipa3_ctx->num_q6_rules =
			rule_req->filter_spec_ex_list_len;
		IPAWANDBG("Received (%d) install_flt_req_ex_list\n",
			rmnet_ipa3_ctx->num_q6_rules);
	} else if (rule_req->filter_spec_ex2_list_valid == true &&
		rule_req->filter_spec_ex_list_valid == false) {
		rmnet_ipa3_ctx->num_q6_rules =
			rule_req->filter_spec_ex2_list_len;
		IPAWANDBG("Received (%d) install_flt_req_ex2_list\n",
			rmnet_ipa3_ctx->num_q6_rules);
	} else {
		rmnet_ipa3_ctx->num_q6_rules = 0;
		if (rule_req->filter_spec_ex2_list_valid == true)
			IPAWANERR(
			"both ex and ex2 flt rules are set to valid\n");
		else
			IPAWANERR("got no UL rules from modem\n");
		mutex_unlock(
			&rmnet_ipa3_ctx->add_mux_channel_lock);
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
		if (rule_req->filter_spec_ex_list_valid == true)
			ipa3_copy_qmi_flt_rule_ex(
				&ipa3_qmi_ctx->q6_ul_filter_rule[i],
				&rule_req->filter_spec_ex_list[i]);
		else if (rule_req->filter_spec_ex2_list_valid == true)
			ipa3_copy_qmi_flt_rule_ex(
				&ipa3_qmi_ctx->q6_ul_filter_rule[i],
				&rule_req->filter_spec_ex2_list[i]);
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

	if (rule_req->ul_firewall_indices_list_valid) {
		IPAWANDBG("Receive ul_firewall_indices_list_len = (%d)",
			rule_req->ul_firewall_indices_list_len);

		if (rule_req->ul_firewall_indices_list_len >
			rmnet_ipa3_ctx->num_q6_rules) {
			IPAWANERR("UL rule indices are not valid: (%d/%d)\n",
					rule_req->xlat_filter_indices_list_len,
					rmnet_ipa3_ctx->num_q6_rules);
			goto failure;
		}

		ipa3_qmi_ctx->ul_firewall_indices_list_valid = 1;
		ipa3_qmi_ctx->ul_firewall_indices_list_len =
			rule_req->ul_firewall_indices_list_len;

		for (i = 0; i < rule_req->ul_firewall_indices_list_len; i++) {
			ipa3_qmi_ctx->ul_firewall_indices_list[i] =
				rule_req->ul_firewall_indices_list[i];
		}

		for (i = 0; i < rule_req->ul_firewall_indices_list_len; i++) {
			if (rule_req->ul_firewall_indices_list[i]
				>= rmnet_ipa3_ctx->num_q6_rules) {
				IPAWANERR("UL rule idx is wrong: %d\n",
					rule_req->ul_firewall_indices_list[i]);
				goto failure;
			} else {
				ipa3_qmi_ctx->q6_ul_filter_rule
				[rule_req->ul_firewall_indices_list[i]]
				.replicate_needed = 1;
			}
		}
	}
	goto success;

failure:
	rmnet_ipa3_ctx->num_q6_rules = 0;
	memset(ipa3_qmi_ctx->q6_ul_filter_rule, 0,
		sizeof(ipa3_qmi_ctx->q6_ul_filter_rule));
	mutex_unlock(
		&rmnet_ipa3_ctx->add_mux_channel_lock);
	return -EINVAL;

success:
	mutex_unlock(
		&rmnet_ipa3_ctx->add_mux_channel_lock);
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
	param->ep = IPA_CLIENT_APPS_WAN_PROD;
	param->global = false;
	param->num_rules = (uint8_t)1;

	memset(req, 0, sizeof(struct ipa_fltr_installed_notif_req_msg_v01));

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
		ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_PROD);
	if (req->source_pipe_index == IPA_EP_NOT_ALLOCATED) {
		IPAWANERR("ep mapping failed\n");
		retval = -EFAULT;
	}

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
	if (!param)
		return -ENOMEM;


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

	for (i = 0; i < rmnet_ipa3_ctx->rmnet_index; i++) {
		if (strcmp(rmnet_ipa3_ctx->mux_channel[i].vchannel_name,
					vchannel_name) == 0)
			return i;
	}
	return MAX_NUM_OF_MUX_CHANNEL;
}

static enum ipa_upstream_type find_upstream_type(const char *upstreamIface)
{
	int i;

	for (i = 0; i < MAX_NUM_OF_MUX_CHANNEL; i++) {
		if (strcmp(rmnet_ipa3_ctx->mux_channel[i].vchannel_name,
					upstreamIface) == 0)
			return IPA_UPSTEAM_MODEM;
	}

	if ((strcmp(IPA_UPSTEAM_WLAN_IFACE_NAME, upstreamIface) == 0) ||
		(strcmp(IPA_UPSTEAM_WLAN1_IFACE_NAME, upstreamIface) == 0))
		return IPA_UPSTEAM_WLAN;
	else
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
	if (rmnet_ipa3_ctx->ipa_config_is_apq)
		tx_ipv4_property->dst_pipe = IPA_CLIENT_MHI_PRIME_TETH_CONS;
	else
		tx_ipv4_property->dst_pipe = IPA_CLIENT_APPS_WAN_CONS;
	snprintf(tx_ipv4_property->hdr_name, IPA_RESOURCE_NAME_MAX, "%s%d",
		 A2_MUX_HDR_NAME_V4_PREF,
		 rmnet_ipa3_ctx->mux_channel[index].mux_id);
	tx_ipv6_property = &tx_properties.prop[1];
	tx_ipv6_property->ip = IPA_IP_v6;
	if (rmnet_ipa3_ctx->ipa_config_is_apq)
		tx_ipv6_property->dst_pipe = IPA_CLIENT_MHI_PRIME_TETH_CONS;
	else
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
	if (rmnet_ipa3_ctx->ipa_config_is_apq)
		rx_ipv4_property->src_pipe = IPA_CLIENT_MHI_PRIME_TETH_PROD;
	else
		rx_ipv4_property->src_pipe = IPA_CLIENT_APPS_WAN_PROD;
	rx_ipv6_property = &rx_properties.prop[1];
	rx_ipv6_property->ip = IPA_IP_v6;
	rx_ipv6_property->attrib.attrib_mask |= IPA_FLT_META_DATA;
	rx_ipv6_property->attrib.meta_data =
		rmnet_ipa3_ctx->mux_channel[index].mux_id << WWAN_METADATA_SHFT;
	rx_ipv6_property->attrib.meta_data_mask = WWAN_METADATA_MASK;
	if (rmnet_ipa3_ctx->ipa_config_is_apq)
		rx_ipv6_property->src_pipe = IPA_CLIENT_MHI_PRIME_TETH_PROD;
	else
		rx_ipv6_property->src_pipe = IPA_CLIENT_APPS_WAN_PROD;
	rx_properties.num_props = 2;

	if (rmnet_ipa3_ctx->ipa_config_is_apq) {
		/* provide mux-id to ipacm in apq platform*/
		pyld_sz = sizeof(struct ipa_ioc_ext_intf_prop);
		ext_ioc_properties = kmalloc(pyld_sz, GFP_KERNEL);
		if (!ext_ioc_properties)
			return -ENOMEM;

		ext_properties.prop = ext_ioc_properties;
		ext_properties.num_props = 1;
		ext_properties.prop[0].mux_id =
			rmnet_ipa3_ctx->mux_channel[index].mux_id;
		ext_properties.prop[0].ip = IPA_IP_MAX;
		IPAWANDBG("ip: %d mux:%d\n",
			ext_properties.prop[0].ip,
			ext_properties.prop[0].mux_id);
		ret = ipa3_register_intf_ext(
			rmnet_ipa3_ctx->mux_channel[index].vchannel_name,
			&tx_properties,
			&rx_properties,
			&ext_properties);
		if (ret) {
			IPAWANERR("[%d]ipa3_register_intf failed %d\n",
				index,
				ret);
			goto fail;
		}
		goto end;
	}
	/* non apq case */
	pyld_sz = rmnet_ipa3_ctx->num_q6_rules *
	sizeof(struct ipa_ioc_ext_intf_prop);
	ext_ioc_properties = kmalloc(pyld_sz, GFP_KERNEL);
	if (!ext_ioc_properties)
		return -ENOMEM;

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
	ret = ipa3_register_intf_ext(
		rmnet_ipa3_ctx->mux_channel[index].vchannel_name,
		&tx_properties,
		&rx_properties,
		&ext_properties);
	if (ret) {
		IPAWANERR("[%s]:ipa3_register_intf failed %d\n",
			rmnet_ipa3_ctx->mux_channel[index].vchannel_name,
				ret);
		goto fail;
	}
end:
	rmnet_ipa3_ctx->mux_channel[index].ul_flt_reg = true;
fail:
	kfree(ext_ioc_properties);
	return ret;
}

static void ipa3_cleanup_deregister_intf(void)
{
	int i;
	int ret;
	int8_t *v_name;

	for (i = 0; i < rmnet_ipa3_ctx->rmnet_index; i++) {
		v_name = rmnet_ipa3_ctx->mux_channel[i].vchannel_name;

		if (rmnet_ipa3_ctx->mux_channel[i].ul_flt_reg) {
			ret = ipa3_deregister_intf(v_name);
			if (ret < 0) {
				IPAWANERR("de-register device %s(%d) failed\n",
					v_name,
					i);
				return;
			}
			IPAWANDBG("de-register device %s(%d) success\n",
				v_name,
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
	struct ipa3_wwan_private *wwan_ptr = netdev_priv(dev);

	IPAWANDBG("[%s]\n", dev->name);
	__ipa_wwan_close(dev);
	if (ipa3_rmnet_res.ipa_napi_enable)
		napi_disable(&(wwan_ptr->napi));
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
	unsigned long flags;

	if (rmnet_ipa3_ctx->ipa_config_is_apq) {
		IPAWANERR_RL("IPA embedded data on APQ platform\n");
		dev_kfree_skb_any(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (skb->protocol != htons(ETH_P_MAP) &&
		(!rmnet_ipa3_ctx->no_qmap_config)) {
		IPAWANDBG_LOW
		("SW filtering out none QMAP packet received from %s",
		current->comm);
		dev_kfree_skb_any(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	qmap_check = RMNET_MAP_GET_CD_BIT(skb);
	spin_lock_irqsave(&wwan_ptr->lock, flags);
	/* There can be a race between enabling the wake queue and
	 * suspend in progress. Check if suspend is pending and
	 * return from here itself.
	 */
	if (atomic_read(&rmnet_ipa3_ctx->ap_suspend)) {
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&wwan_ptr->lock, flags);
		return NETDEV_TX_BUSY;
	}
	if (netif_queue_stopped(dev)) {
		if (rmnet_ipa3_ctx->no_qmap_config) {
			spin_unlock_irqrestore(&wwan_ptr->lock, flags);
			return NETDEV_TX_BUSY;
		} else {
			if (qmap_check &&
				atomic_read(&wwan_ptr->outstanding_pkts) <
					outstanding_high_ctl) {
				IPAWANERR("[%s]Queue stop, send ctrl pkts\n",
						dev->name);
				goto send;
			} else {
				IPAWANERR("[%s]fatal: %s stopped\n", dev->name,
							__func__);
				spin_unlock_irqrestore(&wwan_ptr->lock, flags);
				return NETDEV_TX_BUSY;
			}
		}
	}

	/* checking High WM hit */
	if (atomic_read(&wwan_ptr->outstanding_pkts) >=
					outstanding_high) {
		if (!qmap_check) {
			IPAWANDBG_LOW("pending(%d)/(%d)- stop(%d)\n",
				atomic_read(&wwan_ptr->outstanding_pkts),
				outstanding_high,
				netif_queue_stopped(dev));
			IPAWANDBG_LOW("qmap_chk(%d)\n", qmap_check);
			netif_stop_queue(dev);
			spin_unlock_irqrestore(&wwan_ptr->lock, flags);
			return NETDEV_TX_BUSY;
		}
	}

send:
	/* IPA_RM checking start */
	if (ipa3_ctx->use_ipa_pm) {
		/* activate the modem pm for clock scaling */
		ipa_pm_activate(rmnet_ipa3_ctx->q6_pm_hdl);
		ret = ipa_pm_activate(rmnet_ipa3_ctx->pm_hdl);
	} else {
		ret = ipa_rm_inactivity_timer_request_resource(
			IPA_RM_RESOURCE_WWAN_0_PROD);
	}
	if (ret == -EINPROGRESS) {
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&wwan_ptr->lock, flags);
		return NETDEV_TX_BUSY;
	}
	if (ret) {
		IPAWANERR("[%s] fatal: ipa rm timer req resource failed %d\n",
		       dev->name, ret);
		dev_kfree_skb_any(skb);
		dev->stats.tx_dropped++;
		spin_unlock_irqrestore(&wwan_ptr->lock, flags);
		return NETDEV_TX_OK;
	}
	/*
	 * increase the outstanding_pkts count first
	 * to avoid suspend happens in parallel
	 * after unlock
	 */
	atomic_inc(&wwan_ptr->outstanding_pkts);
	/* IPA_RM checking end */
	spin_unlock_irqrestore(&wwan_ptr->lock, flags);

	/*
	 * both data packets and command will be routed to
	 * IPA_CLIENT_Q6_WAN_CONS based on status configuration
	 */
	ret = ipa3_tx_dp(IPA_CLIENT_APPS_WAN_PROD, skb, NULL);
	if (ret) {
		atomic_dec(&wwan_ptr->outstanding_pkts);
		if (ret == -EPIPE) {
			IPAWANERR_RL("[%s] fatal: pipe is not valid\n",
				dev->name);
			dev_kfree_skb_any(skb);
			dev->stats.tx_dropped++;
			return NETDEV_TX_OK;
		}
		ret = NETDEV_TX_BUSY;
		goto out;
	}

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	ret = NETDEV_TX_OK;
out:
	if (atomic_read(&wwan_ptr->outstanding_pkts) == 0) {
		if (ipa3_ctx->use_ipa_pm) {
			ipa_pm_deferred_deactivate(rmnet_ipa3_ctx->pm_hdl);
			ipa_pm_deferred_deactivate(rmnet_ipa3_ctx->q6_pm_hdl);
		} else {
			ipa_rm_inactivity_timer_release_resource(
				IPA_RM_RESOURCE_WWAN_0_PROD);
		}
	}
	return ret;
}

static void ipa3_wwan_tx_timeout(struct net_device *dev)
{
	struct ipa3_wwan_private *wwan_ptr = netdev_priv(dev);

	if (atomic_read(&wwan_ptr->outstanding_pkts) != 0)
		IPAWANERR("[%s] data stall in UL, %d outstanding\n",
			dev->name, atomic_read(&wwan_ptr->outstanding_pkts));
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
		atomic_read(&wwan_ptr->outstanding_pkts) < outstanding_low) {
		IPAWANDBG_LOW("Outstanding low (%d) - waking up queue\n",
				outstanding_low);
		netif_wake_queue(wwan_ptr->net);
	}

	if (atomic_read(&wwan_ptr->outstanding_pkts) == 0) {
		if (ipa3_ctx->use_ipa_pm) {
			ipa_pm_deferred_deactivate(rmnet_ipa3_ctx->pm_hdl);
			ipa_pm_deferred_deactivate(rmnet_ipa3_ctx->q6_pm_hdl);
		} else {
			ipa_rm_inactivity_timer_release_resource(
			IPA_RM_RESOURCE_WWAN_0_PROD);
		}
	}
	__netif_tx_unlock_bh(netdev_get_tx_queue(dev, 0));
	dev_kfree_skb_any(skb);
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
		if (!rmnet_ipa3_ctx->no_qmap_config)
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
	} else {
		IPAWANERR("Invalid evt %d received in wan_ipa_receive\n", evt);
	}
}

/* Send MHI endpoint info to modem using QMI indication message */
static int ipa_send_mhi_endp_ind_to_modem(void)
{
	struct ipa_endp_desc_indication_msg_v01 req;
	struct ipa_ep_id_type_v01 *ep_info;
	int ipa_mhi_prod_ep_idx =
		ipa3_get_ep_mapping(IPA_CLIENT_MHI_LOW_LAT_PROD);
	int ipa_mhi_cons_ep_idx =
		ipa3_get_ep_mapping(IPA_CLIENT_MHI_LOW_LAT_CONS);

	if (ipa_mhi_prod_ep_idx == IPA_EP_NOT_ALLOCATED ||
		ipa_mhi_cons_ep_idx == IPA_EP_NOT_ALLOCATED)
		return -EINVAL;

	memset(&req, 0, sizeof(struct ipa_endp_desc_indication_msg_v01));
	req.ep_info_len = 2;
	req.ep_info_valid = true;
	req.num_eps_valid = true;
	req.num_eps = 2;
	ep_info = &req.ep_info[0];
	ep_info->ep_id = ipa_mhi_cons_ep_idx;
	ep_info->ic_type = DATA_IC_TYPE_MHI_V01;
	ep_info->ep_type = DATA_EP_DESC_TYPE_EMB_FLOW_CTL_PROD_V01;
	ep_info->ep_status = DATA_EP_STATUS_CONNECTED_V01;
	ep_info = &req.ep_info[1];
	ep_info->ep_id = ipa_mhi_prod_ep_idx;
	ep_info->ic_type = DATA_IC_TYPE_MHI_V01;
	ep_info->ep_type = DATA_EP_DESC_TYPE_EMB_FLOW_CTL_CONS_V01;
	ep_info->ep_status = DATA_EP_STATUS_CONNECTED_V01;
	return ipa3_qmi_send_endp_desc_indication(&req);
}

/* Send RSC endpoint info to modem using QMI indication message */
static int ipa_send_rsc_pipe_ind_to_modem(void)
{
	struct ipa_endp_desc_indication_msg_v01 req;
	struct ipa_ep_id_type_v01 *ep_info;

	memset(&req, 0, sizeof(struct ipa_endp_desc_indication_msg_v01));
	req.ep_info_len = 1;
	req.ep_info_valid = true;
	req.num_eps_valid = true;
	req.num_eps = 1;
	ep_info = &req.ep_info[req.ep_info_len - 1];
	ep_info->ep_id = rmnet_ipa3_ctx->ipa3_to_apps_hdl;
	ep_info->ic_type = DATA_IC_TYPE_AP_V01;
	ep_info->ep_type = DATA_EP_DESC_TYPE_RSC_PROD_V01;
	ep_info->ep_status = DATA_EP_STATUS_CONNECTED_V01;
	return ipa3_qmi_send_endp_desc_indication(&req);
}

static int handle3_ingress_format(struct net_device *dev,
			struct rmnet_ioctl_extended_s *in)
{
	int ret = 0;
	struct ipa_sys_connect_params *ipa_wan_ep_cfg;
	int ep_idx;

	IPAWANDBG("Get RMNET_IOCTL_SET_INGRESS_DATA_FORMAT\n");

	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
	if (ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPAWANDBG("Embedded datapath not supported\n");
		return -EFAULT;
	}

	ipa_wan_ep_cfg = &rmnet_ipa3_ctx->ipa_to_apps_ep_cfg;
	if ((in->u.data) & RMNET_IOCTL_INGRESS_FORMAT_CHECKSUM) {
		if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5)
			ipa_wan_ep_cfg->ipa_ep_cfg.cfg.cs_offload_en =
				IPA_ENABLE_CS_DL_QMAP;
		else
			ipa_wan_ep_cfg->ipa_ep_cfg.cfg.cs_offload_en =
				IPA_ENABLE_CS_OFFLOAD_DL;
		IPAWANDBG("DL chksum set\n");
	}


	if ((in->u.data) & RMNET_IOCTL_INGRESS_FORMAT_IP_ROUTE) {
		rmnet_ipa3_ctx->no_qmap_config = true;
		ipa_wan_ep_cfg->bypass_agg = true;
	}

	if ((in->u.data) & RMNET_IOCTL_INGRESS_FORMAT_AGG_DATA) {
		IPAWANDBG("get AGG size %d count %d\n",
				  in->u.ingress_format.agg_size,
				  in->u.ingress_format.agg_count);

		ret = ipa_disable_apps_wan_cons_deaggr(
			  in->u.ingress_format.agg_size,
			  in->u.ingress_format.agg_count);

		if (!ret) {
			ipa_wan_ep_cfg->ipa_ep_cfg.aggr.aggr_byte_limit =
			   in->u.ingress_format.agg_size;
			ipa_wan_ep_cfg->ipa_ep_cfg.aggr.aggr_pkt_limit =
			   in->u.ingress_format.agg_count;
		}
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5 &&
		(in->u.data) & RMNET_IOCTL_INGRESS_FORMAT_CHECKSUM) {
		ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 8;
		rmnet_ipa3_ctx->dl_csum_offload_enabled = true;
	} else {
		rmnet_ipa3_ctx->dl_csum_offload_enabled = false;
		if (rmnet_ipa3_ctx->no_qmap_config)
			ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 0;
		else
			ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 4;
	}

	ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata_valid = 1;
	ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata = 1;
	ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_pkt_size_valid = 1;
	ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_pkt_size = 2;

	ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_valid = true;
	ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad = 0;
	ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_payload_len_inc_padding = true;
	ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_offset = 0;
	ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_little_endian = 0;
	ipa_wan_ep_cfg->ipa_ep_cfg.metadata_mask.metadata_mask = 0xFF000000;

	ipa_wan_ep_cfg->client = IPA_CLIENT_APPS_WAN_CONS;

	if (dev->features & NETIF_F_GRO_HW)
	 /* Setup coalescing pipes */
		ipa_wan_ep_cfg->client = IPA_CLIENT_APPS_WAN_COAL_CONS;

	ipa_wan_ep_cfg->notify = apps_ipa_packet_receive_notify;
	ipa_wan_ep_cfg->priv = dev;

	if (ipa3_rmnet_res.ipa_napi_enable)
		ipa_wan_ep_cfg->napi_obj = &(rmnet_ipa3_ctx->wwan_priv->napi);
	ipa_wan_ep_cfg->desc_fifo_sz =
		ipa3_rmnet_res.wan_rx_desc_size * IPA_FIFO_ELEMENT_SIZE;

	mutex_lock(&rmnet_ipa3_ctx->pipe_handle_guard);

	if (atomic_read(&rmnet_ipa3_ctx->is_ssr)) {
		IPAWANDBG("In SSR sequence/recovery\n");
		mutex_unlock(&rmnet_ipa3_ctx->pipe_handle_guard);
		return -EFAULT;
	}
	ret = ipa3_setup_sys_pipe(&rmnet_ipa3_ctx->ipa_to_apps_ep_cfg,
	   &rmnet_ipa3_ctx->ipa3_to_apps_hdl);

	mutex_unlock(&rmnet_ipa3_ctx->pipe_handle_guard);
	if (ret)
		goto end;

	/* construct default WAN RT tbl for IPACM */
	ret = ipa3_setup_a7_qmap_hdr();
	if (ret)
		goto end;

	ret = ipa3_setup_dflt_wan_rt_tables();
	if (ret)
		ipa3_del_a7_qmap_hdr();

	/* Sending QMI indication message share RSC pipe details*/
	if (dev->features & NETIF_F_GRO_HW)
		ipa_send_rsc_pipe_ind_to_modem();
end:
	if (ret)
		IPAWANERR("failed to configure ingress\n");

	return ret;
}

/**
 * handle3_egress_format() - Egress data format configuration
 *
 * Setup IPA egress system pipe and Configure:
 *	header handling, checksum, de-aggregation and fifo size
 *
 * @dev: network device
 * @e: egress configuration
 */
static int handle3_egress_format(struct net_device *dev,
			struct rmnet_ioctl_extended_s *e)
{
	int rc;
	struct ipa_sys_connect_params *ipa_wan_ep_cfg;
	int ep_idx;

	IPAWANDBG("get RMNET_IOCTL_SET_EGRESS_DATA_FORMAT %x\n", e->u.data);

	/* in APQ platform, only get QMAP format */
	if (rmnet_ipa3_ctx->ipa_config_is_apq) {
		if ((e->u.data) & RMNET_IOCTL_EGRESS_FORMAT_CHECKSUM) {
			/* QMAPv5 */
			rmnet_ipa3_ctx->dl_csum_offload_enabled = false;
			/* send aggr_info_qmi */
			rc = ipa3_qmi_set_aggr_info(DATA_AGGR_TYPE_QMAP_V01);
		} else {
			/* QMAP */
			rmnet_ipa3_ctx->dl_csum_offload_enabled = false;
			/* send aggr_info_qmi */
			rc = ipa3_qmi_set_aggr_info(DATA_AGGR_TYPE_QMAP_V01);
		}
		rmnet_ipa3_ctx->ipa_mhi_aggr_formet_set = true;
		/* register Q6 indication */
		rc = ipa3_qmi_req_ind();
		return rc;
	}

	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_PROD);
	if (ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPAWANDBG("Embedded datapath not supported\n");
		return -EFAULT;
	}

	if ((e->u.data) & RMNET_IOCTL_EGRESS_FORMAT_IP_ROUTE)
		rmnet_ipa3_ctx->no_qmap_config = true;

	ipa_wan_ep_cfg = &rmnet_ipa3_ctx->apps_to_ipa_ep_cfg;
	if ((e->u.data) & RMNET_IOCTL_EGRESS_FORMAT_CHECKSUM) {
		IPAWANDBG("UL chksum set\n");
		ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 8;
		ipa_wan_ep_cfg->ipa_ep_cfg.cfg.cs_offload_en =
			IPA_ENABLE_CS_OFFLOAD_UL;
		ipa_wan_ep_cfg->ipa_ep_cfg.cfg.cs_metadata_hdr_offset = 1;
	} else {
		if (rmnet_ipa3_ctx->no_qmap_config)
			ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 0;
		else
			ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 4;
	}

	if ((e->u.data) & RMNET_IOCTL_EGRESS_FORMAT_AGGREGATION) {
		IPAWANDBG("WAN UL Aggregation enabled\n");
		ipa_wan_ep_cfg->ipa_ep_cfg.aggr.aggr_en = IPA_ENABLE_DEAGGR;
		ipa_wan_ep_cfg->ipa_ep_cfg.aggr.aggr = IPA_QCMAP;

		ipa_wan_ep_cfg->ipa_ep_cfg.deaggr.packet_offset_valid = false;

		ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_pkt_size = 2;

		ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_valid =
			true;
		ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad =
			IPA_HDR_PAD;
		ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_pad_to_alignment =
			2;
		ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_payload_len_inc_padding =
			true;
		ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_offset =
			0;
		ipa_wan_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_little_endian =
			false;
	} else {
		IPAWANDBG("WAN UL Aggregation disabled\n");
		ipa_wan_ep_cfg->ipa_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
	}

	ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata_valid = 1;
	/* modem want offset at 0! */
	ipa_wan_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata = 0;

	ipa_wan_ep_cfg->ipa_ep_cfg.mode.dst = IPA_CLIENT_APPS_WAN_PROD;
	ipa_wan_ep_cfg->ipa_ep_cfg.mode.mode = IPA_BASIC;

	ipa_wan_ep_cfg->client = IPA_CLIENT_APPS_WAN_PROD;
	ipa_wan_ep_cfg->notify = apps_ipa_tx_complete_notify;
	ipa_wan_ep_cfg->desc_fifo_sz = IPA_SYS_TX_DATA_DESC_FIFO_SZ;
	ipa_wan_ep_cfg->priv = dev;

	mutex_lock(&rmnet_ipa3_ctx->pipe_handle_guard);
	if (atomic_read(&rmnet_ipa3_ctx->is_ssr)) {
		IPAWANDBG("In SSR sequence/recovery\n");
		mutex_unlock(&rmnet_ipa3_ctx->pipe_handle_guard);
		return -EFAULT;
	}
	rc = ipa3_setup_sys_pipe(
		ipa_wan_ep_cfg, &rmnet_ipa3_ctx->apps_to_ipa3_hdl);
	if (rc) {
		IPAWANERR("failed to config egress endpoint\n");
		mutex_unlock(&rmnet_ipa3_ctx->pipe_handle_guard);
		return rc;
	}
	mutex_unlock(&rmnet_ipa3_ctx->pipe_handle_guard);

	if (rmnet_ipa3_ctx->num_q6_rules != 0) {
		/* already got Q6 UL filter rules*/
		if (ipa3_qmi_ctx->modem_cfg_emb_pipe_flt == false) {
			/* prevent multi-threads accessing num_q6_rules */
			mutex_lock(&rmnet_ipa3_ctx->add_mux_channel_lock);
			rc = ipa3_wwan_add_ul_flt_rule_to_ipa();
			mutex_unlock(
				&rmnet_ipa3_ctx->add_mux_channel_lock);
		}
		if (rc)
			IPAWANERR("install UL rules failed\n");
		else
			rmnet_ipa3_ctx->a7_ul_flt_set = true;
	} else {
		/* wait Q6 UL filter rules*/
		IPAWANDBG("no UL-rules\n");
	}
	rmnet_ipa3_ctx->egress_set = true;

	return rc;
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
	struct rmnet_ioctl_extended_s ext_ioctl_data;
	struct rmnet_ioctl_data_s ioctl_data;
	struct ipa3_rmnet_mux_val *mux_channel;
	int rmnet_index;
	uint32_t  mux_id;
	int8_t *v_name;
	struct mutex *mux_mutex_ptr;
	int wan_ep;
	bool tcp_en = false, udp_en = false;

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
		IPAWANERR("RMNET_IOCTL_FLOW_ENABLE not supported\n");
		rc = -EFAULT;
		break;
	/*  Flow disable  */
	case RMNET_IOCTL_FLOW_DISABLE:
		IPAWANERR("RMNET_IOCTL_FLOW_DISABLE not supported\n");
		rc = -EFAULT;
		break;
	/*  Set flow handle  */
	case RMNET_IOCTL_FLOW_SET_HNDL:
		break;

	/*  Extended IOCTLs  */
	case RMNET_IOCTL_EXTENDED:
		if (!ns_capable(dev_net(dev)->user_ns, CAP_NET_ADMIN))
			return -EPERM;
		IPAWANDBG("get ioctl: RMNET_IOCTL_EXTENDED\n");
		if (copy_from_user(&ext_ioctl_data,
			(u8 *)ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_extended_s))) {
			IPAWANERR("failed to copy extended ioctl data\n");
			rc = -EFAULT;
			break;
		}
		switch (ext_ioctl_data.extended_ioctl) {
		/*  Get features  */
		case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
			IPAWANDBG("get RMNET_IOCTL_GET_SUPPORTED_FEATURES\n");
			ext_ioctl_data.u.data =
				(RMNET_IOCTL_FEAT_NOTIFY_MUX_CHANNEL |
				RMNET_IOCTL_FEAT_SET_EGRESS_DATA_FORMAT |
				RMNET_IOCTL_FEAT_SET_INGRESS_DATA_FORMAT);
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&ext_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			break;
		/*  Set MRU  */
		case RMNET_IOCTL_SET_MRU:
			mru = ext_ioctl_data.u.data;
			IPAWANDBG("get MRU size %d\n",
				ext_ioctl_data.u.data);
			break;
		/*  Get MRU  */
		case RMNET_IOCTL_GET_MRU:
			ext_ioctl_data.u.data = mru;
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&ext_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			break;
		/* GET SG support */
		case RMNET_IOCTL_GET_SG_SUPPORT:
			ext_ioctl_data.u.data =
				ipa3_rmnet_res.ipa_advertise_sg_support;
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&ext_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			break;
		/*  Get endpoint ID  */
		case RMNET_IOCTL_GET_EPID:
			IPAWANDBG("get ioctl: RMNET_IOCTL_GET_EPID\n");
			ext_ioctl_data.u.data = epid;
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&ext_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			if (copy_from_user(&ext_ioctl_data,
				(u8 *)ifr->ifr_ifru.ifru_data,
				sizeof(struct rmnet_ioctl_extended_s))) {
				IPAWANERR("copy extended ioctl data failed\n");
				rc = -EFAULT;
			break;
			}
			IPAWANDBG("RMNET_IOCTL_GET_EPID return %d\n",
					ext_ioctl_data.u.data);
			break;
		/*  Endpoint pair  */
		case RMNET_IOCTL_GET_EP_PAIR:
			IPAWANDBG("get ioctl: RMNET_IOCTL_GET_EP_PAIR\n");
			wan_ep = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
			if (wan_ep == IPA_EP_NOT_ALLOCATED) {
				IPAWANERR("Embedded datapath not supported\n");
				rc = -EFAULT;
				break;
			}
			ext_ioctl_data.u.ipa_ep_pair.producer_pipe_num =
				wan_ep;

			wan_ep = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_PROD);
			if (wan_ep == IPA_EP_NOT_ALLOCATED) {
				IPAWANERR("Embedded datapath not supported\n");
				rc = -EFAULT;
				break;
			}
			ext_ioctl_data.u.ipa_ep_pair.consumer_pipe_num =
				wan_ep;
			if (copy_to_user((u8 *)ifr->ifr_ifru.ifru_data,
				&ext_ioctl_data,
				sizeof(struct rmnet_ioctl_extended_s)))
				rc = -EFAULT;
			if (copy_from_user(&ext_ioctl_data,
				(u8 *)ifr->ifr_ifru.ifru_data,
				sizeof(struct rmnet_ioctl_extended_s))) {
				IPAWANERR("copy extended ioctl data failed\n");
				rc = -EFAULT;
				break;
			}
			IPAWANDBG("RMNET_IOCTL_GET_EP_PAIR c: %d p: %d\n",
			ext_ioctl_data.u.ipa_ep_pair.consumer_pipe_num,
			ext_ioctl_data.u.ipa_ep_pair.producer_pipe_num);
			break;
		/*  Get driver name  */
		case RMNET_IOCTL_GET_DRIVER_NAME:
			if (IPA_NETDEV() != NULL) {
				memcpy(&ext_ioctl_data.u.if_name,
					IPA_NETDEV()->name, IFNAMSIZ);
				ext_ioctl_data.u.if_name[IFNAMSIZ - 1] = '\0';
				if (copy_to_user(ifr->ifr_ifru.ifru_data,
					&ext_ioctl_data,
					sizeof(struct rmnet_ioctl_extended_s)))
					rc = -EFAULT;
			} else {
				IPAWANDBG("IPA_NETDEV is NULL\n");
				rc = -EFAULT;
			}
			break;
		/*  Add MUX ID  */
		case RMNET_IOCTL_ADD_MUX_CHANNEL:
			mux_id = ext_ioctl_data.u.rmnet_mux_val.mux_id;
			mux_index = ipa3_find_mux_channel_index(
				ext_ioctl_data.u.rmnet_mux_val.mux_id);
			if (mux_index < MAX_NUM_OF_MUX_CHANNEL) {
				IPAWANDBG("already setup mux(%d)\n", mux_id);
				return rc;
			}
			mutex_lock(&rmnet_ipa3_ctx->add_mux_channel_lock);
			if (rmnet_ipa3_ctx->rmnet_index
				>= MAX_NUM_OF_MUX_CHANNEL) {
				IPAWANERR("Exceed mux_channel limit(%d)\n",
				rmnet_ipa3_ctx->rmnet_index);
				mutex_unlock(
					&rmnet_ipa3_ctx->add_mux_channel_lock);
				return -EFAULT;
			}
			ext_ioctl_data.u.rmnet_mux_val.vchannel_name
				[IFNAMSIZ-1] = '\0';
			IPAWANDBG("ADD_MUX_CHANNEL(%d, name: %s)\n",
			ext_ioctl_data.u.rmnet_mux_val.mux_id,
			ext_ioctl_data.u.rmnet_mux_val.vchannel_name);
			/* cache the mux name and id */
			mux_channel = rmnet_ipa3_ctx->mux_channel;
			rmnet_index = rmnet_ipa3_ctx->rmnet_index;

			mux_channel[rmnet_index].mux_id =
				ext_ioctl_data.u.rmnet_mux_val.mux_id;
			memcpy(mux_channel[rmnet_index].vchannel_name,
				ext_ioctl_data.u.rmnet_mux_val.vchannel_name,
				sizeof(mux_channel[rmnet_index]
					.vchannel_name));
			mux_channel[rmnet_index].vchannel_name[
				IFNAMSIZ - 1] = '\0';

			IPAWANDBG("cashe device[%s:%d] in IPA_wan[%d]\n",
				mux_channel[rmnet_index].vchannel_name,
				mux_channel[rmnet_index].mux_id,
				rmnet_index);
			/* check if UL filter rules coming*/
			v_name =
				ext_ioctl_data.u.rmnet_mux_val.vchannel_name;
			if (rmnet_ipa3_ctx->num_q6_rules != 0 ||
					(rmnet_ipa3_ctx->ipa_config_is_apq)) {
				mux_mutex_ptr =
					&rmnet_ipa3_ctx->add_mux_channel_lock;
				IPAWANERR_RL("dev(%s) register to IPA\n",
					v_name);
				rc = ipa3_wwan_register_to_ipa(
						rmnet_ipa3_ctx->rmnet_index);
				if (rc < 0) {
					IPAWANERR("device %s reg IPA failed\n",
						v_name);
					mutex_unlock(mux_mutex_ptr);
					return -ENODEV;
				}
				mux_channel[rmnet_index].mux_channel_set =
					true;
				mux_channel[rmnet_index].ul_flt_reg =
					true;
			} else {
				IPAWANDBG("dev(%s) haven't registered to IPA\n",
					v_name);
				mux_channel[rmnet_index].mux_channel_set =
					true;
				mux_channel[rmnet_index].ul_flt_reg =
					false;
			}
			rmnet_ipa3_ctx->rmnet_index++;
			mutex_unlock(&rmnet_ipa3_ctx->add_mux_channel_lock);
			break;
		case RMNET_IOCTL_SET_EGRESS_DATA_FORMAT:
			rc = handle3_egress_format(dev, &ext_ioctl_data);
			break;
		case RMNET_IOCTL_SET_INGRESS_DATA_FORMAT:/*  Set IDF  */
			rc = handle3_ingress_format(dev, &ext_ioctl_data);
			break;
		case RMNET_IOCTL_SET_XLAT_DEV_INFO:
			wan_msg = kzalloc(sizeof(struct ipa_wan_msg),
						GFP_KERNEL);
			if (!wan_msg)
				return -ENOMEM;
			ext_ioctl_data.u.if_name[IFNAMSIZ-1] = '\0';
			len = sizeof(wan_msg->upstream_ifname) >
			sizeof(ext_ioctl_data.u.if_name) ?
				sizeof(ext_ioctl_data.u.if_name) :
				sizeof(wan_msg->upstream_ifname);
			strlcpy(wan_msg->upstream_ifname,
				ext_ioctl_data.u.if_name, len);
			wan_msg->upstream_ifname[len-1] = '\0';
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
		/*  Set RSC/RSB  */
		case RMNET_IOCTL_SET_OFFLOAD:
			if (ext_ioctl_data.u.offload_params.flags
				& RMNET_IOCTL_COALESCING_FORMAT_TCP)
				tcp_en = true;
			if (ext_ioctl_data.u.offload_params.flags
				& RMNET_IOCTL_COALESCING_FORMAT_UDP)
				udp_en = true;
			rc = rmnet_ipa_send_coalesce_notification(
				ext_ioctl_data.u.offload_params.mux_id,
				tcp_en || udp_en, tcp_en, udp_en);
			break;
		default:
			IPAWANERR("[%s] unsupported extended cmd[%d]",
				dev->name,
				ext_ioctl_data.extended_ioctl);
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
		IPAWANERR("ipa_rm_request_resource failed %d\n", ret);
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
		IPAWANERR("ipa_rm_release_resource failed %d\n", ret);
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
		IPAWANDBG_LOW("Q6_PROD GRANTED CB\n");
		break;
	case IPA_RM_RESOURCE_RELEASED:
		IPAWANDBG_LOW("Q6_PROD RELEASED CB\n");
		break;
	default:
		return;
	}
}

/**
 * rmnet_ipa_send_coalesce_notification
 * (uint8_t qmap_id, bool enable, bool tcp, bool udp)
 * send RSC notification
 *
 * This function sends the rsc enable/disable notification
 * fot tcp, udp to user-space module
 */
static int rmnet_ipa_send_coalesce_notification(uint8_t qmap_id,
		bool enable,
		bool tcp,
		bool udp)
{
	struct ipa_msg_meta msg_meta;
	struct ipa_coalesce_info *coalesce_info;
	int rc;

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	coalesce_info = kzalloc(sizeof(*coalesce_info), GFP_KERNEL);
	if (!coalesce_info)
		return -ENOMEM;

	if (enable) {
		coalesce_info->qmap_id = qmap_id;
		coalesce_info->tcp_enable = tcp;
		coalesce_info->udp_enable = udp;
		msg_meta.msg_type = IPA_COALESCE_ENABLE;
		msg_meta.msg_len = sizeof(struct ipa_coalesce_info);
	} else {
		msg_meta.msg_type = IPA_COALESCE_DISABLE;
		msg_meta.msg_len = sizeof(struct ipa_coalesce_info);
	}
	rc = ipa_send_msg(&msg_meta, coalesce_info, ipa3_wwan_msg_free_cb);
	if (rc) {
		IPAWANERR("ipa_send_msg failed: %d\n", rc);
		return -EFAULT;
	}
	IPAWANDBG("qmap-id(%d),enable(%d),tcp(%d),udp(%d)\n",
		qmap_id, enable, tcp, udp);
	return 0;
}

int ipa3_wwan_set_modem_state(struct wan_ioctl_notify_wan_state *state)
{
	int ret = 0;

	if (!state)
		return -EINVAL;

	if (!ipa_pm_is_used())
		return 0;

	if (state->up)
		ret = ipa_pm_activate_sync(rmnet_ipa3_ctx->q6_teth_pm_hdl);
	else
		ret = ipa_pm_deactivate_sync(rmnet_ipa3_ctx->q6_teth_pm_hdl);

	return ret;
}

/**
 * ipa3_q6_register_pm - Register modem clients for PM
 *
 * This function will register 2 client with IPA PM to represent modem
 * in clock scaling calculation:
 *	- "EMB MODEM" - this client will be activated with embedded traffic
	- "TETH MODEM" - this client we be activated by IPACM on offload to
	  modem.
*/
static int ipa3_q6_register_pm(void)
{
	int result;
	struct ipa_pm_register_params pm_reg;

	memset(&pm_reg, 0, sizeof(pm_reg));
	pm_reg.name = "EMB MODEM";
	pm_reg.group = IPA_PM_GROUP_MODEM;
	pm_reg.skip_clk_vote = true;
	result = ipa_pm_register(&pm_reg, &rmnet_ipa3_ctx->q6_pm_hdl);
	if (result) {
		IPAERR("failed to create IPA PM client %d\n", result);
		return result;
	}

	pm_reg.name = "TETH MODEM";
	pm_reg.group = IPA_PM_GROUP_MODEM;
	pm_reg.skip_clk_vote = true;
	result = ipa_pm_register(&pm_reg, &rmnet_ipa3_ctx->q6_teth_pm_hdl);
	if (result) {
		IPAERR("failed to create IPA PM client %d\n", result);
		return result;
	}

	return 0;
}

static void ipa3_q6_deregister_pm(void)
{
	ipa_pm_deactivate_sync(rmnet_ipa3_ctx->q6_pm_hdl);
	ipa_pm_deregister(rmnet_ipa3_ctx->q6_pm_hdl);
}

int ipa3_wwan_set_modem_perf_profile(int throughput)
{
	struct ipa_rm_perf_profile profile;
	int ret;

	IPAWANDBG("throughput: %d\n", throughput);

	if (ipa3_ctx->use_ipa_pm) {
		/* for TETH MODEM on softap/rndis */
		ret = ipa_pm_set_throughput(rmnet_ipa3_ctx->q6_teth_pm_hdl,
			throughput);
	} else {
		memset(&profile, 0, sizeof(profile));
		profile.max_supported_bandwidth_mbps = throughput;
		ret = ipa_rm_set_perf_profile(IPA_RM_RESOURCE_Q6_PROD,
			&profile);
	}

	return ret;
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

	if (rmnet_ipa3_ctx->rm_q6_wq)
		destroy_workqueue(rmnet_ipa3_ctx->rm_q6_wq);
}

static void ipa3_wake_tx_queue(struct work_struct *work)
{
	if (IPA_NETDEV()) {
		__netif_tx_lock_bh(netdev_get_tx_queue(IPA_NETDEV(), 0));
		IPAWANDBG("Waking up the workqueue.\n");
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

static int ipa3_lcl_mdm_ssr_notifier_cb(struct notifier_block *this,
			   unsigned long code,
			   void *data);

static int ipa3_rmt_mdm_ssr_notifier_cb(struct notifier_block *this,
			   unsigned long code,
			   void *data);

static struct notifier_block ipa3_lcl_mdm_ssr_notifier = {
	.notifier_call = ipa3_lcl_mdm_ssr_notifier_cb,
};

static struct notifier_block ipa3_rmt_mdm_ssr_notifier = {
	.notifier_call = ipa3_rmt_mdm_ssr_notifier_cb,
};


static int get_ipa_rmnet_dts_configuration(struct platform_device *pdev,
		struct ipa3_rmnet_plat_drv_res *ipa_rmnet_drv_res)
{
	int result;

	ipa_rmnet_drv_res->wan_rx_desc_size = IPA_WWAN_CONS_DESC_FIFO_SZ;
	ipa_rmnet_drv_res->ipa_rmnet_ssr =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,rmnet-ipa-ssr");
	pr_info("IPA SSR support = %s\n",
		ipa_rmnet_drv_res->ipa_rmnet_ssr ? "True" : "False");
	ipa_rmnet_drv_res->is_platform_type_msm =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-platform-type-msm");
	pr_info("IPA is_platform_type_msm = %s\n",
		ipa_rmnet_drv_res->is_platform_type_msm ? "True" : "False");

	ipa_rmnet_drv_res->ipa_advertise_sg_support =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,ipa-advertise-sg-support");
	pr_info("IPA SG support = %s\n",
		ipa_rmnet_drv_res->ipa_advertise_sg_support ? "True" : "False");

	ipa_rmnet_drv_res->ipa_napi_enable =
		of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-napi-enable");
	pr_info("IPA Napi Enable = %s\n",
		ipa_rmnet_drv_res->ipa_napi_enable ? "True" : "False");

	/* Get IPA WAN RX desc fifo size */
	result = of_property_read_u32(pdev->dev.of_node,
			"qcom,wan-rx-desc-size",
			&ipa_rmnet_drv_res->wan_rx_desc_size);
	if (result)
		pr_info("using default for wan-rx-desc-size = %u\n",
				ipa_rmnet_drv_res->wan_rx_desc_size);
	else
		IPAWANDBG(": found ipa_drv_res->wan-rx-desc-size = %u\n",
				ipa_rmnet_drv_res->wan_rx_desc_size);

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

	IPAWANDBG("IPA ready callback has been triggered\n");

	schedule_work(&ipa3_scheduled_probe);
}

static void ipa_pm_wwan_pm_cb(void *p, enum ipa_pm_cb_event event)
{
	struct net_device *dev = (struct net_device *)p;
	struct ipa3_wwan_private *wwan_ptr = netdev_priv(dev);

	IPAWANDBG_LOW("event %d\n", event);
	switch (event) {
	case IPA_PM_CLIENT_ACTIVATED:
		if (wwan_ptr->device_status == WWAN_DEVICE_INACTIVE) {
			complete_all(&wwan_ptr->resource_granted_completion);
			break;
		}
		ipa3_rm_resource_granted(dev);
		break;
	default:
		pr_err("%s: unknown event %d\n", __func__, event);
		break;
	}
}

static int ipa3_wwan_register_netdev_pm_client(struct net_device *dev)
{
	int result;
	struct ipa_pm_register_params pm_reg;

	memset(&pm_reg, 0, sizeof(pm_reg));
	pm_reg.name = IPA_NETDEV()->name;
	pm_reg.user_data = dev;
	pm_reg.callback = ipa_pm_wwan_pm_cb;
	pm_reg.group = IPA_PM_GROUP_APPS;
	result = ipa_pm_register(&pm_reg, &rmnet_ipa3_ctx->pm_hdl);
	if (result) {
		IPAERR("failed to create IPA PM client %d\n", result);
			return result;
	}
	return 0;
}

static void ipa3_wwan_deregister_netdev_pm_client(void)
{
	ipa_pm_deactivate_sync(rmnet_ipa3_ctx->pm_hdl);
	ipa_pm_deregister(rmnet_ipa3_ctx->pm_hdl);
}

static int ipa3_wwan_create_wwan_rm_resource(struct net_device *dev)
{
	struct ipa_rm_create_params ipa_rm_params;
	struct ipa_rm_perf_profile profile;
	int ret;

	memset(&ipa_rm_params, 0, sizeof(struct ipa_rm_create_params));
	ipa_rm_params.name = IPA_RM_RESOURCE_WWAN_0_PROD;
	ipa_rm_params.reg_params.user_data = dev;
	ipa_rm_params.reg_params.notify_cb = ipa3_rm_notify;
	ret = ipa_rm_create_resource(&ipa_rm_params);
	if (ret) {
		pr_err("%s: unable to create resourse %d in IPA RM\n",
			__func__, IPA_RM_RESOURCE_WWAN_0_PROD);
		return ret;
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

	return 0;

set_perf_err:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_WWAN_0_PROD,
		IPA_RM_RESOURCE_Q6_CONS);
add_dpnd_err:
	ipa_rm_inactivity_timer_destroy(
		IPA_RM_RESOURCE_WWAN_0_PROD); /* IPA_RM */
timer_init_err:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_WWAN_0_PROD);
	return ret;
}

static void ipa3_wwan_delete_wwan_rm_resource(void)
{
	int ret;

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
	int wan_cons_ep;

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

	wan_cons_ep = ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
	ret = get_ipa_rmnet_dts_configuration(pdev, &ipa3_rmnet_res);
	ipa3_rmnet_ctx.ipa_rmnet_ssr = ipa3_rmnet_res.ipa_rmnet_ssr;

	/* check if booting as mhi-prime */
	rmnet_ipa3_ctx->ipa_config_is_apq
		= ipa3_is_apq();

	ret = ipa3_init_q6_smem();
	if (ret) {
		IPAWANERR("ipa3_init_q6_smem failed\n");
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
	rmnet_ipa3_ctx->ipa_mhi_aggr_formet_set = false;
	for (i = 0; i < MAX_NUM_OF_MUX_CHANNEL; i++)
		memset(&rmnet_ipa3_ctx->mux_channel[i], 0,
				sizeof(struct ipa3_rmnet_mux_val));

	/* start A7 QMI service/client */
	if (ipa3_rmnet_res.is_platform_type_msm)
		/* Android platform loads uC */
		ipa3_qmi_service_init(QMI_IPA_PLATFORM_TYPE_MSM_ANDROID_V01);
	else if (ipa3_ctx->ipa_config_is_mhi)
		/* LE MHI platform */
		ipa3_qmi_service_init(QMI_IPA_PLATFORM_TYPE_LE_MHI_V01);
	else
		/* LE platform not loads uC */
		ipa3_qmi_service_init(QMI_IPA_PLATFORM_TYPE_LE_V01);

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
	IPAWANDBG("wwan_ptr (private) = %pK", rmnet_ipa3_ctx->wwan_priv);
	rmnet_ipa3_ctx->wwan_priv->net = dev;
	atomic_set(&rmnet_ipa3_ctx->wwan_priv->outstanding_pkts, 0);
	spin_lock_init(&rmnet_ipa3_ctx->wwan_priv->lock);
	init_completion(
		&rmnet_ipa3_ctx->wwan_priv->resource_granted_completion);

	if (!atomic_read(&rmnet_ipa3_ctx->is_ssr)) {
		/* IPA_RM configuration starts */
		if (ipa3_ctx->use_ipa_pm)
			ret = ipa3_q6_register_pm();
		else
			ret = ipa3_q6_initialize_rm();
		if (ret) {
			IPAWANERR("ipa3_q6_initialize_rm failed, ret: %d\n",
					ret);
			goto q6_init_err;
		}
	}

	if (ipa3_ctx->use_ipa_pm)
		ret = ipa3_wwan_register_netdev_pm_client(dev);
	else
		ret = ipa3_wwan_create_wwan_rm_resource(dev);
	if (ret) {
		IPAWANERR("fail to create/register pm resources\n");
		goto fail_pm;
	}

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

	/* for > IPA 4.5, we set the colaescing feature flag on */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5)
		dev->hw_features |= NETIF_F_GRO_HW | NETIF_F_RXCSUM;

	/*
	 * for IPA 4.0 offline charge is not needed and we need to prevent
	 * power collapse until IPA uC is loaded.
	 */
	atomic_set(&rmnet_ipa3_ctx->is_initialized, 1);
	if (!atomic_read(&rmnet_ipa3_ctx->is_ssr) && ipa3_ctx->ipa_hw_type !=
		IPA_HW_v4_0) {
		/* offline charging mode */
		ipa3_proxy_clk_unvote();
	}
	atomic_set(&rmnet_ipa3_ctx->is_ssr, 0);
	atomic_set(&rmnet_ipa3_ctx->ap_suspend, 0);
	ipa3_update_ssr_state(false);

	IPAWANERR("rmnet_ipa completed initialization\n");
	return 0;
config_err:
	if (ipa3_rmnet_res.ipa_napi_enable)
		netif_napi_del(&(rmnet_ipa3_ctx->wwan_priv->napi));
	unregister_netdev(dev);
set_perf_err:
	if (ipa3_ctx->use_ipa_pm)
		ipa3_wwan_deregister_netdev_pm_client();
	else
		ipa3_wwan_delete_wwan_rm_resource();
fail_pm:
	if (ipa3_ctx->use_ipa_pm) {
		if (!atomic_read(&rmnet_ipa3_ctx->is_ssr))
			ipa3_q6_deregister_pm();
	} else {
		if (!atomic_read(&rmnet_ipa3_ctx->is_ssr))
			ipa3_q6_deinitialize_rm();
	}
q6_init_err:
	free_netdev(dev);
	rmnet_ipa3_ctx->wwan_priv = NULL;
alloc_netdev_err:
	ipa3_wan_ioctl_deinit();
wan_ioctl_init_err:
	ipa3_qmi_service_exit();
	atomic_set(&rmnet_ipa3_ctx->is_ssr, 0);
	return ret;
}

static int ipa3_wwan_remove(struct platform_device *pdev)
{
	int ret;

	IPAWANINFO("rmnet_ipa started deinitialization\n");
	mutex_lock(&rmnet_ipa3_ctx->pipe_handle_guard);
	ret = ipa3_teardown_sys_pipe(rmnet_ipa3_ctx->ipa3_to_apps_hdl);
	if (ret < 0)
		IPAWANERR("Failed to teardown IPA->APPS pipe\n");
	else
		rmnet_ipa3_ctx->ipa3_to_apps_hdl = -1;
	ret = ipa3_teardown_sys_pipe(rmnet_ipa3_ctx->apps_to_ipa3_hdl);
	if (ret < 0)
		IPAWANERR("Failed to teardown APPS->IPA pipe\n");
	else
		rmnet_ipa3_ctx->apps_to_ipa3_hdl = -1;
	if (ipa3_rmnet_res.ipa_napi_enable)
		netif_napi_del(&(rmnet_ipa3_ctx->wwan_priv->napi));
	mutex_unlock(&rmnet_ipa3_ctx->pipe_handle_guard);
	IPAWANINFO("rmnet_ipa unregister_netdev\n");
	unregister_netdev(IPA_NETDEV());
	if (ipa3_ctx->use_ipa_pm)
		ipa3_wwan_deregister_netdev_pm_client();
	else
		ipa3_wwan_delete_wwan_rm_resource();
	cancel_work_sync(&ipa3_tx_wakequeue_work);
	cancel_delayed_work(&ipa_tether_stats_poll_wakequeue_work);
	if (IPA_NETDEV())
		free_netdev(IPA_NETDEV());
	rmnet_ipa3_ctx->wwan_priv = NULL;
	/* No need to remove wwan_ioctl during SSR */
	if (!atomic_read(&rmnet_ipa3_ctx->is_ssr))
		ipa3_wan_ioctl_deinit();
	if (ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS) !=
		IPA_EP_NOT_ALLOCATED) {
		ipa3_del_dflt_wan_rt_tables();
		ipa3_del_a7_qmap_hdr();
	}
	ipa3_del_mux_qmap_hdrs();
	if (ipa3_qmi_ctx->modem_cfg_emb_pipe_flt == false)
		ipa3_wwan_del_ul_flt_rule_to_ipa();
	ipa3_cleanup_deregister_intf();
	/* reset dl_csum_offload_enabled */
	rmnet_ipa3_ctx->dl_csum_offload_enabled = false;
	atomic_set(&rmnet_ipa3_ctx->is_initialized, 0);
	IPAWANINFO("rmnet_ipa completed deinitialization\n");
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
	int ret;
	unsigned long flags;

	IPAWANDBG("Enter...\n");

	if (netdev == NULL) {
		IPAWANERR("netdev is NULL.\n");
		ret = 0;
		goto bail;
	}

	wwan_ptr = netdev_priv(netdev);
	if (wwan_ptr == NULL) {
		IPAWANERR("wwan_ptr is NULL.\n");
		ret = 0;
		goto bail;
	}

	/*
	 * Rmnert supend and xmit are executing at the same time, In those
	 * scenarios observing the data was processed when IPA clock are off.
	 * Added changes to synchronize rmnet supend and xmit.
	 */
	atomic_set(&rmnet_ipa3_ctx->ap_suspend, 1);
	spin_lock_irqsave(&wwan_ptr->lock, flags);
	/* Do not allow A7 to suspend in case there are outstanding packets */
	if (atomic_read(&wwan_ptr->outstanding_pkts) != 0) {
		IPAWANDBG("Outstanding packets, postponing AP suspend.\n");
		ret = -EAGAIN;
		atomic_set(&rmnet_ipa3_ctx->ap_suspend, 0);
		spin_unlock_irqrestore(&wwan_ptr->lock, flags);
		goto bail;
	}

	/* Make sure that there is no Tx operation ongoing */
	netif_stop_queue(netdev);
	/* Stoppig Watch dog timer when pipe was in suspend state */
	if (del_timer(&netdev->watchdog_timer))
		dev_put(netdev);
	spin_unlock_irqrestore(&wwan_ptr->lock, flags);

	IPAWANDBG("De-activating the PM/RM resource.\n");
	if (ipa3_ctx->use_ipa_pm)
		ipa_pm_deactivate_sync(rmnet_ipa3_ctx->pm_hdl);
	else
		ipa_rm_release_resource(IPA_RM_RESOURCE_WWAN_0_PROD);
	ret = 0;
bail:
	IPAWANDBG("Exit with %d\n", ret);
	return ret;
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

	IPAWANDBG("Enter...\n");
	/* Clear the suspend in progress flag. */
	atomic_set(&rmnet_ipa3_ctx->ap_suspend, 0);
	if (netdev) {
		netif_wake_queue(netdev);
		/* Starting Watch dog timer, pipe was changes to resume state */
		if (netif_running(netdev) && netdev->watchdog_timeo <= 0)
			__netdev_watchdog_up(netdev);
	}
	IPAWANDBG("Exit\n");

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

/**
 * rmnet_ipa_send_ssr_notification(bool ssr_done) - send SSR notification
 *
 * This function sends the SSR notification before modem shutdown and
 * after_powerup from SSR framework, to user-space module
 */
static void rmnet_ipa_send_ssr_notification(bool ssr_done)
{
	struct ipa_msg_meta msg_meta;
	int rc;

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	if (ssr_done)
		msg_meta.msg_type = IPA_SSR_AFTER_POWERUP;
	else
		msg_meta.msg_type = IPA_SSR_BEFORE_SHUTDOWN;
	rc = ipa_send_msg(&msg_meta, NULL, NULL);
	if (rc) {
		IPAWANERR("ipa_send_msg failed: %d\n", rc);
		return;
	}
}

static int ipa3_lcl_mdm_ssr_notifier_cb(struct notifier_block *this,
			   unsigned long code,
			   void *data)
{
	if (!ipa3_rmnet_ctx.ipa_rmnet_ssr)
		return NOTIFY_DONE;

	if (!ipa3_ctx) {
		IPAWANERR_RL("ipa3_ctx was not initialized\n");
		return NOTIFY_DONE;
	}

	if (rmnet_ipa3_ctx->ipa_config_is_apq) {
		IPAWANERR("Local modem SSR event=%lu on APQ platform\n",
			code);
		return NOTIFY_DONE;
	}

	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		IPAWANINFO("IPA received MPSS BEFORE_SHUTDOWN\n");
		/* send SSR before-shutdown notification to IPACM */
		rmnet_ipa_send_ssr_notification(false);
		atomic_set(&rmnet_ipa3_ctx->is_ssr, 1);
		ipa3_q6_pre_shutdown_cleanup();
		if (IPA_NETDEV())
			netif_stop_queue(IPA_NETDEV());
		ipa3_qmi_stop_workqueues();
		ipa3_wan_ioctl_stop_qmi_messages();
		ipa_stop_polling_stats();
		if (atomic_read(&rmnet_ipa3_ctx->is_initialized))
			platform_driver_unregister(&rmnet_ipa_driver);
		if (ipa3_ctx->ipa_mhi_proxy)
			imp_handle_modem_shutdown();
		if (atomic_read(&rmnet_ipa3_ctx->is_ssr) &&
			ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
			ipa3_q6_post_shutdown_cleanup();
		ipa3_odl_pipe_cleanup(true);
		IPAWANINFO("IPA BEFORE_SHUTDOWN handling is complete\n");
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		IPAWANINFO("IPA Received MPSS AFTER_SHUTDOWN\n");
		if (atomic_read(&rmnet_ipa3_ctx->is_ssr) &&
			ipa3_ctx->ipa_hw_type < IPA_HW_v4_0)
			ipa3_q6_post_shutdown_cleanup();

		if (ipa3_ctx->ipa_endp_delay_wa)
			ipa3_client_prod_post_shutdown_cleanup();

		IPAWANINFO("IPA AFTER_SHUTDOWN handling is complete\n");
		break;
	case SUBSYS_BEFORE_POWERUP:
		IPAWANINFO("IPA received MPSS BEFORE_POWERUP\n");
		if (atomic_read(&rmnet_ipa3_ctx->is_ssr)) {
			/* clean up cached QMI msg/handlers */
			ipa3_qmi_service_exit();
			ipa3_q6_pre_powerup_cleanup();
		}
		/* hold a proxy vote for the modem. */
		ipa3_proxy_clk_vote();
		ipa3_reset_freeze_vote();
		IPAWANINFO("IPA BEFORE_POWERUP handling is complete\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		IPAWANINFO("IPA received MPSS AFTER_POWERUP\n");
		if (!atomic_read(&rmnet_ipa3_ctx->is_initialized) &&
		       atomic_read(&rmnet_ipa3_ctx->is_ssr))
			platform_driver_register(&rmnet_ipa_driver);
		ipa3_odl_pipe_open();
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

static int ipa3_rmt_mdm_ssr_notifier_cb(struct notifier_block *this,
			   unsigned long code,
			   void *data)
{
	if (!ipa3_rmnet_ctx.ipa_rmnet_ssr) {
		IPAWANERR("SSR event=%lu while not enabled\n", code);
		return NOTIFY_DONE;
	}

	if (!rmnet_ipa3_ctx->ipa_config_is_apq) {
		IPAWANERR("Remote mdm SSR event=%lu on non-APQ platform=%d\n",
			code, ipa3_ctx->platform_type);
		return NOTIFY_DONE;
	}

	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		IPAWANINFO("IPA received RMT MPSS BEFORE_SHUTDOWN\n");
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		IPAWANINFO("IPA Received RMT MPSS AFTER_SHUTDOWN\n");
		break;
	case SUBSYS_BEFORE_POWERUP:
		IPAWANINFO("IPA received RMT MPSS BEFORE_POWERUP\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		IPAWANINFO("IPA received RMT MPSS AFTER_POWERUP\n");
		break;
	default:
		IPAWANDBG("IPA received RMT MPSS event %lu", code);
		break;
	}
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
			type != IPA_TETHERING_STATS_UPDATE_NETWORK_STATS &&
			type != IPA_PER_CLIENT_STATS_CONNECT_EVENT &&
			type != IPA_PER_CLIENT_STATS_DISCONNECT_EVENT) {
		IPAWANERR("Wrong type given. buff %pK type %d\n",
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
	if (!resp)
		return;

	memset(&req, 0, sizeof(struct ipa_get_data_stats_req_msg_v01));
	memset(resp, 0, sizeof(struct ipa_get_data_stats_resp_msg_v01));

	req.ipa_stats_type = QMI_IPA_STATS_TYPE_PIPE_V01;

	rc = ipa3_qmi_get_data_stats(&req, resp);
	if (rc) {
		IPAWANERR("ipa3_qmi_get_data_stats failed: %d\n", rc);
		kfree(resp);
		return;
	}

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	msg_meta.msg_type = IPA_TETHERING_STATS_UPDATE_STATS;
	msg_meta.msg_len = sizeof(struct ipa_get_data_stats_resp_msg_v01);
	rc = ipa_send_msg(&msg_meta, resp, rmnet_ipa_free_msg);
	if (rc) {
		IPAWANERR("ipa_send_msg failed: %d\n", rc);
		kfree(resp);
		return;
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
	if (!resp)
		return;

	memset(&req, 0, sizeof(struct ipa_get_apn_data_stats_req_msg_v01));
	memset(resp, 0, sizeof(struct ipa_get_apn_data_stats_resp_msg_v01));

	req.mux_id_list_valid = true;
	req.mux_id_list_len = 1;
	req.mux_id_list[0] = ipa3_rmnet_ctx.metered_mux_id;

	rc = ipa3_qmi_get_network_stats(&req, resp);
	if (rc) {
		IPAWANERR("ipa3_qmi_get_network_stats failed: %d\n", rc);
		kfree(resp);
		return;
	}

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	msg_meta.msg_type = IPA_TETHERING_STATS_UPDATE_NETWORK_STATS;
	msg_meta.msg_len = sizeof(struct ipa_get_apn_data_stats_resp_msg_v01);
	rc = ipa_send_msg(&msg_meta, resp, rmnet_ipa_free_msg);
	if (rc) {
		IPAWANERR("ipa_send_msg failed: %d\n", rc);
		kfree(resp);
		return;
	}
}

/**
 * rmnet_ipa_send_quota_reach_ind() - send quota_reach notification from
 * IPA Modem
 * This function sends the quota_reach indication from the IPA Modem driver
 * via QMI, to user-space module
 */
static void rmnet_ipa_send_quota_reach_ind(void)
{
	struct ipa_msg_meta msg_meta;
	int rc;

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	msg_meta.msg_type = IPA_QUOTA_REACH;
	rc = ipa_send_msg(&msg_meta, NULL, NULL);
	if (rc) {
		IPAWANERR("ipa_send_msg failed: %d\n", rc);
		return;
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
 * rmnet_ipa_set_data_quota_modem() - Data quota setting IOCTL handler
 * @data - IOCTL data
 *
 * This function handles WAN_IOC_SET_DATA_QUOTA on modem interface.
 * It translates the given interface name to the Modem MUX ID and
 * sends the request of the quota to the IPA Modem driver via QMI.
 *
 * Return codes:
 * 0: Success
 * -EFAULT: Invalid interface name provided
 * other: See ipa_qmi_set_data_quota
 */
static int rmnet_ipa3_set_data_quota_modem(
	struct wan_ioctl_set_data_quota *data)
{
	u32 mux_id;
	int index;
	struct ipa_set_data_usage_quota_req_msg_v01 req;

	/* stop quota */
	if (!data->set_quota)
		ipa3_qmi_stop_data_qouta();

	/* prevent string buffer overflows */
	data->interface_name[IFNAMSIZ-1] = '\0';

	index = find_vchannel_name_index(data->interface_name);
	IPAWANERR("iface name %s, quota %lu\n",
		  data->interface_name,
		  (unsigned long int) data->quota_mbytes);

	if (index == MAX_NUM_OF_MUX_CHANNEL) {
		IPAWANERR("%s is an invalid iface name\n",
			  data->interface_name);
		return -ENODEV;
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

static int rmnet_ipa3_set_data_quota_wifi(struct wan_ioctl_set_data_quota *data)
{
	struct ipa_set_wifi_quota wifi_quota;
	int rc = 0;

	memset(&wifi_quota, 0, sizeof(struct ipa_set_wifi_quota));
	wifi_quota.set_quota = data->set_quota;
	wifi_quota.quota_bytes = data->quota_mbytes;
	IPAWANERR("iface name %s, quota %lu\n",
		  data->interface_name,
		  (unsigned long int) data->quota_mbytes);

	rc = ipa3_set_wlan_quota(&wifi_quota);
	/* check if wlan-fw takes this quota-set */
	if (!wifi_quota.set_valid)
		rc = -EFAULT;
	return rc;
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
	enum ipa_upstream_type upstream_type;
	int rc = 0;

	/* prevent string buffer overflows */
	data->interface_name[IFNAMSIZ-1] = '\0';

	/* get IPA backhaul type */
	upstream_type = find_upstream_type(data->interface_name);

	if (upstream_type == IPA_UPSTEAM_MAX) {
		IPAWANERR("Wrong interface_name name %s\n",
			data->interface_name);
	} else if (upstream_type == IPA_UPSTEAM_WLAN) {
		rc = rmnet_ipa3_set_data_quota_wifi(data);
		if (rc) {
			IPAWANERR("set quota on wifi failed\n");
			return rc;
		}
	} else {
		rc = rmnet_ipa3_set_data_quota_modem(data);
		if (rc) {
			IPAWANERR("set quota on modem failed\n");
			return rc;
		}
	}
	return rc;
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

	/* error checking if ul_src_pipe_len valid or not*/
	if (data->ul_src_pipe_len > QMI_IPA_MAX_PIPES_V01 ||
		data->ul_src_pipe_len < 0) {
		IPAWANERR("UL src pipes %d exceeding max %d\n",
			data->ul_src_pipe_len,
			QMI_IPA_MAX_PIPES_V01);
		return -EFAULT;
	}
	/* error checking if dl_dst_pipe_len valid or not*/
	if (data->dl_dst_pipe_len > QMI_IPA_MAX_PIPES_V01 ||
		data->dl_dst_pipe_len < 0) {
		IPAWANERR("DL dst pipes %d exceeding max %d\n",
			data->dl_dst_pipe_len,
			QMI_IPA_MAX_PIPES_V01);
		return -EFAULT;
	}

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

static int rmnet_ipa3_query_tethering_stats_wifi(
	struct wan_ioctl_query_tether_stats *data, bool reset)
{
	struct ipa_get_wdi_sap_stats *sap_stats;
	int rc;

	sap_stats = kzalloc(sizeof(struct ipa_get_wdi_sap_stats),
			GFP_KERNEL);
	if (!sap_stats)
		return -ENOMEM;

	memset(sap_stats, 0, sizeof(struct ipa_get_wdi_sap_stats));

	sap_stats->reset_stats = reset;
	IPAWANDBG("reset the pipe stats %d\n", sap_stats->reset_stats);

	rc = ipa3_get_wlan_stats(sap_stats);
	if (rc) {
		IPAWANERR_RL("can't get ipa3_get_wlan_stats\n");
		kfree(sap_stats);
		return rc;
	} else if (data == NULL) {
		IPAWANDBG("only reset wlan stats\n");
		kfree(sap_stats);
		return 0;
	}

	if (sap_stats->stats_valid) {
		data->ipv4_tx_packets = sap_stats->ipv4_tx_packets;
		data->ipv4_tx_bytes = sap_stats->ipv4_tx_bytes;
		data->ipv4_rx_packets = sap_stats->ipv4_rx_packets;
		data->ipv4_rx_bytes = sap_stats->ipv4_rx_bytes;
		data->ipv6_tx_packets = sap_stats->ipv6_tx_packets;
		data->ipv6_tx_bytes = sap_stats->ipv6_tx_bytes;
		data->ipv6_rx_packets = sap_stats->ipv6_rx_packets;
		data->ipv6_rx_bytes = sap_stats->ipv6_rx_bytes;
	}

	IPAWANDBG("v4_rx_p(%lu) v6_rx_p(%lu) v4_rx_b(%lu) v6_rx_b(%lu)\n",
		(unsigned long int) data->ipv4_rx_packets,
		(unsigned long int) data->ipv6_rx_packets,
		(unsigned long int) data->ipv4_rx_bytes,
		(unsigned long int) data->ipv6_rx_bytes);
	IPAWANDBG("tx_p_v4(%lu)v6(%lu)tx_b_v4(%lu) v6(%lu)\n",
		(unsigned long int) data->ipv4_tx_packets,
		(unsigned long  int) data->ipv6_tx_packets,
		(unsigned long int) data->ipv4_tx_bytes,
		(unsigned long int) data->ipv6_tx_bytes);

	kfree(sap_stats);
	return rc;
}

static int rmnet_ipa3_query_tethering_stats_modem(
	struct wan_ioctl_query_tether_stats *data, bool reset)
{
	struct ipa_get_data_stats_req_msg_v01 *req;
	struct ipa_get_data_stats_resp_msg_v01 *resp;
	int pipe_len, rc;
	struct ipa_pipe_stats_info_type_v01 *stat_ptr;

	req = kzalloc(sizeof(struct ipa_get_data_stats_req_msg_v01),
			GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(struct ipa_get_data_stats_resp_msg_v01),
			GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}
	memset(req, 0, sizeof(struct ipa_get_data_stats_req_msg_v01));
	memset(resp, 0, sizeof(struct ipa_get_data_stats_resp_msg_v01));

	req->ipa_stats_type = QMI_IPA_STATS_TYPE_PIPE_V01;
	if (reset) {
		req->reset_stats_valid = true;
		req->reset_stats = true;
		IPAWANDBG("reset the pipe stats\n");
	}

	rc = ipa3_qmi_get_data_stats(req, resp);
	if (rc) {
		IPAWANERR("can't get ipa_qmi_get_data_stats\n");
		kfree(req);
		kfree(resp);
		return rc;
	} else if (data == NULL) {
		IPAWANDBG("only reset modem stats\n");
		kfree(req);
		kfree(resp);
		return 0;
	}

	if (resp->dl_dst_pipe_stats_list_valid) {
		for (pipe_len = 0; pipe_len < resp->dl_dst_pipe_stats_list_len;
			pipe_len++) {
			stat_ptr =
				&resp->dl_dst_pipe_stats_list[pipe_len];

			IPAWANDBG_LOW("Check entry(%d) dl_dst_pipe(%d)\n",
				pipe_len,
				stat_ptr->pipe_index);
			IPAWANDBG_LOW("dl_p_v4(%lu)v6(%lu)\n",
				(unsigned long int) stat_ptr->num_ipv4_packets,
				(unsigned long int) stat_ptr->num_ipv6_packets
			);
			IPAWANDBG_LOW("dl_b_v4(%lu)v6(%lu)\n",
				(unsigned long int) stat_ptr->num_ipv4_bytes,
				(unsigned long int) stat_ptr->num_ipv6_bytes);
			if (ipa_get_client_uplink(
				stat_ptr->pipe_index) == false) {
				if (data->ipa_client == ipa_get_client(
					stat_ptr->pipe_index)) {
					/* update the DL stats */
					data->ipv4_rx_packets +=
						stat_ptr->num_ipv4_packets;
					data->ipv6_rx_packets +=
						stat_ptr->num_ipv6_packets;
					data->ipv4_rx_bytes +=
						stat_ptr->num_ipv4_bytes;
					data->ipv6_rx_bytes +=
						stat_ptr->num_ipv6_bytes;
				}
			}
		}
	}
	IPAWANDBG("v4_rx_p(%lu) v6_rx_p(%lu) v4_rx_b(%lu) v6_rx_b(%lu)\n",
		(unsigned long int) data->ipv4_rx_packets,
		(unsigned long int) data->ipv6_rx_packets,
		(unsigned long int) data->ipv4_rx_bytes,
		(unsigned long int) data->ipv6_rx_bytes);

	if (resp->ul_src_pipe_stats_list_valid) {
		for (pipe_len = 0; pipe_len < resp->ul_src_pipe_stats_list_len;
			pipe_len++) {
			stat_ptr =
				&resp->ul_src_pipe_stats_list[pipe_len];
			IPAWANDBG_LOW("Check entry(%d) ul_dst_pipe(%d)\n",
				pipe_len,
				stat_ptr->pipe_index);
			IPAWANDBG_LOW("ul_p_v4(%lu)v6(%lu)\n",
				(unsigned long int) stat_ptr->num_ipv4_packets,
				(unsigned long int) stat_ptr->num_ipv6_packets
			);
			IPAWANDBG_LOW("ul_b_v4(%lu)v6(%lu)\n",
				(unsigned long int)stat_ptr->num_ipv4_bytes,
				(unsigned long int) stat_ptr->num_ipv6_bytes);
			if (ipa_get_client_uplink(
				stat_ptr->pipe_index) == true) {
				if (data->ipa_client == ipa_get_client(
					stat_ptr->pipe_index)) {
					/* update the DL stats */
					data->ipv4_tx_packets +=
						stat_ptr->num_ipv4_packets;
					data->ipv6_tx_packets +=
						stat_ptr->num_ipv6_packets;
					data->ipv4_tx_bytes +=
						stat_ptr->num_ipv4_bytes;
					data->ipv6_tx_bytes +=
						stat_ptr->num_ipv6_bytes;
				}
			}
		}
	}
	IPAWANDBG("tx_p_v4(%lu)v6(%lu)tx_b_v4(%lu) v6(%lu)\n",
		(unsigned long int) data->ipv4_tx_packets,
		(unsigned long  int) data->ipv6_tx_packets,
		(unsigned long int) data->ipv4_tx_bytes,
		(unsigned long int) data->ipv6_tx_bytes);
	kfree(req);
	kfree(resp);
	return 0;
}

static int rmnet_ipa3_query_tethering_stats_hw(
	struct wan_ioctl_query_tether_stats *data, bool reset)
{
	int rc = 0, index = 0;
	struct ipa_quota_stats_all *con_stats;
	struct ipa_quota_stats  *client;

	/* qet HW-stats */
	rc = ipa_get_teth_stats();
	if (rc) {
		IPAWANDBG("ipa_get_teth_stats failed %d,\n", rc);
		return rc;
	}

	/* query DL stats */
	IPAWANDBG("reset the pipe stats? (%d)\n", reset);
	con_stats = kzalloc(sizeof(*con_stats), GFP_KERNEL);
	if (!con_stats) {
		IPAWANERR("no memory\n");
		return -ENOMEM;
	}

	if (rmnet_ipa3_ctx->ipa_config_is_apq) {
		rc = ipa_query_teth_stats(IPA_CLIENT_MHI_PRIME_TETH_PROD,
			con_stats, reset);
		if (rc) {
			IPAERR("MHI_PRIME_TETH_PROD query failed %d,\n", rc);
			kfree(con_stats);
			return rc;
		}
	} else {
		rc = ipa_query_teth_stats(IPA_CLIENT_Q6_WAN_PROD,
			con_stats, reset);
		if (rc) {
			IPAERR("IPA_CLIENT_Q6_WAN_PROD query failed %d,\n", rc);
			kfree(con_stats);
			return rc;
		}
	}
	IPAWANDBG("wlan: v4_rx_p(%d) b(%lld) v6_rx_p(%d) b(%lld)\n",
	con_stats->client[IPA_CLIENT_WLAN1_CONS].num_ipv4_pkts,
	con_stats->client[IPA_CLIENT_WLAN1_CONS].num_ipv4_bytes,
	con_stats->client[IPA_CLIENT_WLAN1_CONS].num_ipv6_pkts,
	con_stats->client[IPA_CLIENT_WLAN1_CONS].num_ipv6_bytes);

	IPAWANDBG("usb: v4_rx_p(%d) b(%lld) v6_rx_p(%d) b(%lld)\n",
	con_stats->client[IPA_CLIENT_USB_CONS].num_ipv4_pkts,
	con_stats->client[IPA_CLIENT_USB_CONS].num_ipv4_bytes,
	con_stats->client[IPA_CLIENT_USB_CONS].num_ipv6_pkts,
	con_stats->client[IPA_CLIENT_USB_CONS].num_ipv6_bytes);

	/* update the DL stats */
	data->ipv4_rx_packets =
		con_stats->client[IPA_CLIENT_WLAN1_CONS].num_ipv4_pkts +
			con_stats->client[IPA_CLIENT_USB_CONS].num_ipv4_pkts;
	data->ipv6_rx_packets =
		con_stats->client[IPA_CLIENT_WLAN1_CONS].num_ipv6_pkts +
			con_stats->client[IPA_CLIENT_USB_CONS].num_ipv6_pkts;
	data->ipv4_rx_bytes =
		con_stats->client[IPA_CLIENT_WLAN1_CONS].num_ipv4_bytes +
			con_stats->client[IPA_CLIENT_USB_CONS].num_ipv4_bytes;
	data->ipv6_rx_bytes =
		con_stats->client[IPA_CLIENT_WLAN1_CONS].num_ipv6_bytes +
			con_stats->client[IPA_CLIENT_USB_CONS].num_ipv6_bytes;

	IPAWANDBG("v4_rx_p(%lu) v6_rx_p(%lu) v4_rx_b(%lu) v6_rx_b(%lu)\n",
		(unsigned long int) data->ipv4_rx_packets,
		(unsigned long int) data->ipv6_rx_packets,
		(unsigned long int) data->ipv4_rx_bytes,
		(unsigned long int) data->ipv6_rx_bytes);

	/* query USB UL stats */
	memset(con_stats, 0, sizeof(struct ipa_quota_stats_all));
	rc = ipa_query_teth_stats(IPA_CLIENT_USB_PROD, con_stats, reset);
	if (rc) {
		IPAERR("IPA_CLIENT_USB_PROD query failed %d\n", rc);
		kfree(con_stats);
		return rc;
	}

	if (rmnet_ipa3_ctx->ipa_config_is_apq)
		index = IPA_CLIENT_MHI_PRIME_TETH_CONS;
	else
		index = IPA_CLIENT_Q6_WAN_CONS;

	IPAWANDBG("usb: v4_tx_p(%d) b(%lld) v6_tx_p(%d) b(%lld)\n",
	con_stats->client[index].num_ipv4_pkts,
	con_stats->client[index].num_ipv4_bytes,
	con_stats->client[index].num_ipv6_pkts,
	con_stats->client[index].num_ipv6_bytes);

	/* update the USB UL stats */
	data->ipv4_tx_packets =
		con_stats->client[index].num_ipv4_pkts;
	data->ipv6_tx_packets =
		con_stats->client[index].num_ipv6_pkts;
	data->ipv4_tx_bytes =
		con_stats->client[index].num_ipv4_bytes;
	data->ipv6_tx_bytes =
		con_stats->client[index].num_ipv6_bytes;

	/* usb UL stats on cv2 */
	client = &con_stats->client[IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS];
	IPAWANDBG("usb (cv2): v4_tx_p(%d) b(%lld) v6_tx_p(%d) b(%lld)\n",
		client->num_ipv4_pkts,
		client->num_ipv4_bytes,
		client->num_ipv6_pkts,
		client->num_ipv6_bytes);

	/* update cv2 USB UL stats */
	data->ipv4_tx_packets +=
		client->num_ipv4_pkts;
	data->ipv6_tx_packets +=
		client->num_ipv6_pkts;
	data->ipv4_tx_bytes +=
		client->num_ipv4_bytes;
	data->ipv6_tx_bytes +=
		client->num_ipv6_bytes;

	/* query WLAN UL stats */
	memset(con_stats, 0, sizeof(struct ipa_quota_stats_all));
	rc = ipa_query_teth_stats(IPA_CLIENT_WLAN1_PROD, con_stats, reset);
	if (rc) {
		IPAERR("IPA_CLIENT_WLAN1_PROD query failed %d\n", rc);
		kfree(con_stats);
		return rc;
	}

	if (rmnet_ipa3_ctx->ipa_config_is_apq)
		index = IPA_CLIENT_MHI_PRIME_TETH_CONS;
	else
		index = IPA_CLIENT_Q6_WAN_CONS;

	IPAWANDBG("wlan: v4_tx_p(%d) b(%lld) v6_tx_p(%d) b(%lld)\n",
	con_stats->client[index].num_ipv4_pkts,
	con_stats->client[index].num_ipv4_bytes,
	con_stats->client[index].num_ipv6_pkts,
	con_stats->client[index].num_ipv6_bytes);

	/* update the wlan UL stats */
	data->ipv4_tx_packets +=
		con_stats->client[index].num_ipv4_pkts;
	data->ipv6_tx_packets +=
		con_stats->client[index].num_ipv6_pkts;
	data->ipv4_tx_bytes +=
		con_stats->client[index].num_ipv4_bytes;
	data->ipv6_tx_bytes +=
		con_stats->client[index].num_ipv6_bytes;

	/* wlan UL stats on cv2 */
	IPAWANDBG("wlan (cv2): v4_tx_p(%d) b(%lld) v6_tx_p(%d) b(%lld)\n",
	con_stats->client[IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS].num_ipv4_pkts,
	con_stats->client[IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS].num_ipv4_bytes,
	con_stats->client[IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS].num_ipv6_pkts,
	con_stats->client[IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS].num_ipv6_bytes);

	/* update cv2 wlan UL stats */
	client = &con_stats->client[IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS];
	data->ipv4_tx_packets +=
		client->num_ipv4_pkts;
	data->ipv6_tx_packets +=
		client->num_ipv6_pkts;
	data->ipv4_tx_bytes +=
		client->num_ipv4_bytes;
	data->ipv6_tx_bytes +=
		client->num_ipv6_bytes;

	IPAWANDBG("v4_tx_p(%lu) v6_tx_p(%lu) v4_tx_b(%lu) v6_tx_b(%lu)\n",
		(unsigned long int) data->ipv4_tx_packets,
		(unsigned long  int) data->ipv6_tx_packets,
		(unsigned long int) data->ipv4_tx_bytes,
		(unsigned long int) data->ipv6_tx_bytes);
	kfree(con_stats);
	return rc;
}


int rmnet_ipa3_query_tethering_stats(struct wan_ioctl_query_tether_stats *data,
	bool reset)
{
	enum ipa_upstream_type upstream_type;
	int rc = 0;

	/* prevent string buffer overflows */
	data->upstreamIface[IFNAMSIZ-1] = '\0';
	data->tetherIface[IFNAMSIZ-1] = '\0';

	/* get IPA backhaul type */
	upstream_type = find_upstream_type(data->upstreamIface);

	if (upstream_type == IPA_UPSTEAM_MAX) {
		IPAWANERR(" Wrong upstreamIface name %s\n",
			data->upstreamIface);
	} else if (upstream_type == IPA_UPSTEAM_WLAN) {
		IPAWANDBG_LOW(" query wifi-backhaul stats\n");
		rc = rmnet_ipa3_query_tethering_stats_wifi(
			data, false);
		if (rc) {
			IPAWANERR("wlan WAN_IOC_QUERY_TETHER_STATS failed\n");
			return rc;
		}
	} else {
		IPAWANDBG_LOW(" query modem-backhaul stats\n");
		rc = rmnet_ipa3_query_tethering_stats_modem(
			data, false);
		if (rc) {
			IPAWANERR("modem WAN_IOC_QUERY_TETHER_STATS failed\n");
			return rc;
		}
	}
	return rc;
}

int rmnet_ipa3_query_tethering_stats_all(
	struct wan_ioctl_query_tether_stats_all *data)
{
	struct wan_ioctl_query_tether_stats tether_stats;
	enum ipa_upstream_type upstream_type;
	int rc = 0;

	memset(&tether_stats, 0, sizeof(struct wan_ioctl_query_tether_stats));

	/* prevent string buffer overflows */
	data->upstreamIface[IFNAMSIZ-1] = '\0';

	/* get IPA backhaul type */
	upstream_type = find_upstream_type(data->upstreamIface);

	if (upstream_type == IPA_UPSTEAM_MAX) {
		IPAWANERR(" Wrong upstreamIface name %s\n",
			data->upstreamIface);
	} else if (upstream_type == IPA_UPSTEAM_WLAN) {
		IPAWANDBG_LOW(" query wifi-backhaul stats\n");
		rc = rmnet_ipa3_query_tethering_stats_wifi(
			&tether_stats, data->reset_stats);
		if (rc) {
			IPAWANERR_RL(
				"wlan WAN_IOC_QUERY_TETHER_STATS failed\n");
			return rc;
		}
		data->tx_bytes = tether_stats.ipv4_tx_bytes
			+ tether_stats.ipv6_tx_bytes;
		data->rx_bytes = tether_stats.ipv4_rx_bytes
			+ tether_stats.ipv6_rx_bytes;
	} else {
		IPAWANDBG_LOW(" query modem-backhaul stats\n");
		tether_stats.ipa_client = data->ipa_client;
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0 ||
			!ipa3_ctx->hw_stats.enabled) {
			IPAWANDBG("hw version %d,hw_stats.enabled %d\n",
				ipa3_ctx->ipa_hw_type,
				ipa3_ctx->hw_stats.enabled);
			/* get modem stats from QMI */
			rc = rmnet_ipa3_query_tethering_stats_modem(
				&tether_stats, data->reset_stats);
			if (rc) {
				IPAWANERR("modem QUERY_TETHER_STATS failed\n");
				return rc;
			}
		} else {
			/* get modem stats from IPA-HW counters */
			rc = rmnet_ipa3_query_tethering_stats_hw(
				&tether_stats, data->reset_stats);
			if (rc) {
				IPAWANERR("modem QUERY_TETHER_STATS failed\n");
				return rc;
			}
		}
		data->tx_bytes = tether_stats.ipv4_tx_bytes
			+ tether_stats.ipv6_tx_bytes;
		data->rx_bytes = tether_stats.ipv4_rx_bytes
			+ tether_stats.ipv6_rx_bytes;
	}
	return rc;
}

int rmnet_ipa3_reset_tethering_stats(struct wan_ioctl_reset_tether_stats *data)
{
	enum ipa_upstream_type upstream_type;
	int rc = 0;

	/* prevent string buffer overflows */
	data->upstreamIface[IFNAMSIZ-1] = '\0';

	/* get IPA backhaul type */
	upstream_type = find_upstream_type(data->upstreamIface);

	if (upstream_type == IPA_UPSTEAM_MAX) {
		IPAWANERR(" Wrong upstreamIface name %s\n",
			data->upstreamIface);
	} else if (upstream_type == IPA_UPSTEAM_WLAN) {
		IPAWANERR(" reset wifi-backhaul stats\n");
		rc = rmnet_ipa3_query_tethering_stats_wifi(
			NULL, true);
		if (rc) {
			IPAWANERR("reset WLAN stats failed\n");
			return rc;
		}
	} else {
		IPAWANERR(" reset modem-backhaul stats\n");
		rc = rmnet_ipa3_query_tethering_stats_modem(
			NULL, true);
		if (rc) {
			IPAWANERR("reset MODEM stats failed\n");
			return rc;
		}
	}
	return rc;
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
void ipa3_broadcast_quota_reach_ind(u32 mux_id,
	enum ipa_upstream_type upstream_type)
{
	char alert_msg[IPA_QUOTA_REACH_ALERT_MAX_SIZE];
	char iface_name_m[IPA_QUOTA_REACH_IF_NAME_MAX_SIZE];
	char iface_name_l[IPA_QUOTA_REACH_IF_NAME_MAX_SIZE];
	char *envp[IPA_UEVENT_NUM_EVNP] = {
		alert_msg, iface_name_l, iface_name_m, NULL};
	int res;
	int index;

	/* check upstream_type*/
	if (upstream_type == IPA_UPSTEAM_MAX) {
		IPAWANERR(" Wrong upstreamIface type %d\n", upstream_type);
		return;
	} else if (upstream_type == IPA_UPSTEAM_MODEM) {
		index = ipa3_find_mux_channel_index(mux_id);
		if (index == MAX_NUM_OF_MUX_CHANNEL) {
			IPAWANERR("%u is an mux ID\n", mux_id);
			return;
		}
	}
	res = snprintf(alert_msg, IPA_QUOTA_REACH_ALERT_MAX_SIZE,
			"ALERT_NAME=%s", "quotaReachedAlert");
	if (res >= IPA_QUOTA_REACH_ALERT_MAX_SIZE) {
		IPAWANERR("message too long (%d)", res);
		return;
	}
	/* posting msg for L-release for CNE */
	if (upstream_type == IPA_UPSTEAM_MODEM) {
		res = snprintf(iface_name_l,
			IPA_QUOTA_REACH_IF_NAME_MAX_SIZE,
			"UPSTREAM=%s",
			rmnet_ipa3_ctx->mux_channel[index].vchannel_name);
	} else {
		res = snprintf(iface_name_l, IPA_QUOTA_REACH_IF_NAME_MAX_SIZE,
			"UPSTREAM=%s", IPA_UPSTEAM_WLAN_IFACE_NAME);
	}
	if (res >= IPA_QUOTA_REACH_IF_NAME_MAX_SIZE) {
		IPAWANERR("message too long (%d)", res);
		return;
	}
	/* posting msg for M-release for CNE */
	if (upstream_type == IPA_UPSTEAM_MODEM) {
		res = snprintf(iface_name_m,
			IPA_QUOTA_REACH_IF_NAME_MAX_SIZE,
			"INTERFACE=%s",
			rmnet_ipa3_ctx->mux_channel[index].vchannel_name);
	} else {
		res = snprintf(iface_name_m,
			IPA_QUOTA_REACH_IF_NAME_MAX_SIZE,
			"INTERFACE=%s",
			IPA_UPSTEAM_WLAN_IFACE_NAME);
	}
	if (res >= IPA_QUOTA_REACH_IF_NAME_MAX_SIZE) {
		IPAWANERR("message too long (%d)", res);
		return;
	}

	IPAWANERR("putting nlmsg: <%s> <%s> <%s>\n",
		alert_msg, iface_name_l, iface_name_m);
	kobject_uevent_env(&(IPA_NETDEV()->dev.kobj),
		KOBJ_CHANGE, envp);

	rmnet_ipa_send_quota_reach_ind();
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

		/* send SSR power-up notification to IPACM */
		rmnet_ipa_send_ssr_notification(true);

		/*
		 * It is required to recover the network stats after
		 * SSR recovery
		 */
		rmnet_ipa_get_network_stats_and_update();
	}
	if (ipa3_ctx->ipa_mhi_proxy)
		imp_handle_modem_ready();

	/*
	 * currently the endp_desc indication only send
	 * on non-auto mode for low latency pipes
	 */
	if (ipa3_ctx->ipa_config_is_mhi &&
		!ipa3_ctx->ipa_config_is_auto)
		ipa_send_mhi_endp_ind_to_modem();
}

static inline bool rmnet_ipa3_check_any_client_inited
(
	enum ipacm_per_client_device_type device_type
)
{
	int i = 0;
	struct ipa_tether_device_info *teth_ptr = NULL;

	for (; i < IPA_MAX_NUM_HW_PATH_CLIENTS; i++) {
		teth_ptr = &rmnet_ipa3_ctx->tether_device[device_type];

		if (teth_ptr->lan_client[i].client_idx != -1 &&
			teth_ptr->lan_client[i].inited) {
			IPAWANERR("Found client index: %d which is inited\n",
				 i);
			return true;
		}
	}

	return false;
}

static inline int rmnet_ipa3_get_lan_client_info
(
	enum ipacm_per_client_device_type device_type,
	uint8_t mac[]
)
{
	int i = 0;
	struct ipa_tether_device_info *teth_ptr = NULL;

	IPAWANDBG("Client MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac[0], mac[1], mac[2],
		mac[3], mac[4], mac[5]);

	for (; i < IPA_MAX_NUM_HW_PATH_CLIENTS; i++) {
		teth_ptr = &rmnet_ipa3_ctx->tether_device[device_type];

		if (memcmp(
			teth_ptr->lan_client[i].mac,
			mac,
			IPA_MAC_ADDR_SIZE) == 0) {
			IPAWANDBG("Matched client index: %d\n", i);
			return i;
		}
	}

	return -EINVAL;
}

static inline int rmnet_ipa3_delete_lan_client_info
(
	enum ipacm_per_client_device_type device_type,
	int lan_clnt_idx
)
{
	struct ipa_lan_client *lan_client = NULL;
	int i;
	struct ipa_tether_device_info *teth_ptr = NULL;

	IPAWANDBG("Delete lan client info: %d, %d, %d\n",
		rmnet_ipa3_ctx->tether_device[device_type].num_clients,
		lan_clnt_idx, device_type);
	/* Check if Device type is valid. */

	if (device_type >= IPACM_MAX_CLIENT_DEVICE_TYPES ||
		device_type < 0) {
		IPAWANERR("Invalid Device type: %d\n", device_type);
		return -EINVAL;
	}

	/* Check if the request is to clean up all clients. */
	teth_ptr = &rmnet_ipa3_ctx->tether_device[device_type];

	if (lan_clnt_idx == 0xffffffff) {
		/* Reset the complete device info. */
		memset(teth_ptr, 0,
				sizeof(struct ipa_tether_device_info));
		teth_ptr->ul_src_pipe = -1;
		for (i = 0; i < IPA_MAX_NUM_HW_PATH_CLIENTS; i++)
			teth_ptr->lan_client[i].client_idx = -1;
	} else {
		lan_client = &teth_ptr->lan_client[lan_clnt_idx];

		/* Reset the client info before sending the message. */
		memset(lan_client, 0, sizeof(struct ipa_lan_client));
		lan_client->client_idx = -1;
		/* Decrement the number of clients. */
		rmnet_ipa3_ctx->tether_device[device_type].num_clients--;

	}
	return 0;
}

/* Query must be free-d by the caller */
static int rmnet_ipa_get_hw_fnr_stats_v2(
	struct ipa_lan_client_cntr_index *client,
	struct wan_ioctl_query_per_client_stats *data,
	struct ipa_ioc_flt_rt_query *query)
{
	int num_counters;

	query->start_id = client->ul_cnt_idx;
	query->end_id = client->dl_cnt_idx;

	query->reset = data->reset_stats;
	num_counters = query->end_id - query->start_id + 1;

	if (num_counters != 2) {
		IPAWANERR("Dont support more than 2 counter\n");
		return -EINVAL;
	}

	IPAWANDBG(" Start/End %u/%u, num counters = %d\n",
		query->start_id, query->end_id, num_counters);

	query->stats = (uint64_t)kcalloc(
			num_counters,
			sizeof(struct ipa_flt_rt_stats),
			GFP_KERNEL);
	if (!query->stats) {
		IPAERR("Failed to allocate memory for query stats\n");
		return -ENOMEM;
	}

	if (ipa_get_flt_rt_stats(query)) {
		IPAERR("Failed to request stats from h/w\n");
		return -EINVAL;
	}

	IPAWANDBG("ul: bytes = %llu, pkts = %u, pkts_hash = %u\n",
	  ((struct ipa_flt_rt_stats *)query->stats)[0].num_bytes,
	  ((struct ipa_flt_rt_stats *)query->stats)[0].num_pkts,
	  ((struct ipa_flt_rt_stats *)query->stats)[0].num_pkts_hash);
	IPAWANDBG("dl: bytes = %llu, pkts = %u, pkts_hash = %u\n",
	  ((struct ipa_flt_rt_stats *)query->stats)[1].num_bytes,
	  ((struct ipa_flt_rt_stats *)query->stats)[1].num_pkts,
	  ((struct ipa_flt_rt_stats *)query->stats)[1].num_pkts_hash);

	return 0;
}

/* rmnet_ipa3_set_lan_client_info() -
 * @data - IOCTL data
 *
 * This function handles WAN_IOC_SET_LAN_CLIENT_INFO.
 * It is used to store LAN client information which
 * is used to fetch the packet stats for a client.
 *
 * Return codes:
 * 0: Success
 * -EINVAL: Invalid args provided
 */
int rmnet_ipa3_set_lan_client_info(
	struct wan_ioctl_lan_client_info *data)
{
	struct ipa_lan_client *lan_client = NULL;
	struct ipa_lan_client_cntr_index
		*client_index = NULL;
	struct ipa_tether_device_info *teth_ptr = NULL;


	IPAWANDBG("Client MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
		data->mac[0], data->mac[1], data->mac[2],
		data->mac[3], data->mac[4], data->mac[5]);

	/* Check if Device type is valid. */
	if (data->device_type >= IPACM_MAX_CLIENT_DEVICE_TYPES ||
		data->device_type < 0) {
		IPAWANERR("Invalid Device type: %d\n", data->device_type);
		return -EINVAL;
	}

	/* Check if Client index is valid. */
	if (data->client_idx >= IPA_MAX_NUM_HW_PATH_CLIENTS ||
		data->client_idx < 0) {
		IPAWANERR("Invalid Client Index: %d\n", data->client_idx);
		return -EINVAL;
	}

	/* This should be done when allocation of hw fnr counters happens */
	if (!(data->ul_cnt_idx > 0 &&
		data->dl_cnt_idx == (data->ul_cnt_idx + 1))) {
		IPAWANERR("Invalid counter indices %u, %u\n",
				data->ul_cnt_idx, data->dl_cnt_idx);
		return -EINVAL;
	}

	mutex_lock(&rmnet_ipa3_ctx->per_client_stats_guard);
	if (data->client_init) {
		/* check if the client is already inited. */
		if (rmnet_ipa3_ctx->tether_device[data->device_type]
			.lan_client[data->client_idx].inited) {
			IPAWANERR("Client already inited: %d:%d\n",
				data->device_type, data->client_idx);
			mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
			return -EINVAL;
		}
	}

	teth_ptr = &rmnet_ipa3_ctx->tether_device[data->device_type];
	lan_client = &teth_ptr->lan_client[data->client_idx];
	client_index = &teth_ptr->lan_client_indices[data->client_idx];

	memcpy(lan_client->mac, data->mac, IPA_MAC_ADDR_SIZE);

	lan_client->client_idx = data->client_idx;

	/* Update the Source pipe. */
	rmnet_ipa3_ctx->tether_device[data->device_type].ul_src_pipe =
			ipa3_get_ep_mapping(data->ul_src_pipe);

	/* Update the header length if not set. */
	if (!rmnet_ipa3_ctx->tether_device[data->device_type].hdr_len)
		rmnet_ipa3_ctx->tether_device[data->device_type].hdr_len =
			data->hdr_len;
	client_index->ul_cnt_idx = data->ul_cnt_idx;
	client_index->dl_cnt_idx = data->dl_cnt_idx;

	IPAWANDBG("Device type %d, ul/dl = %d/%d\n",
			data->device_type,
			data->ul_cnt_idx,
			data->dl_cnt_idx);

	lan_client->inited = true;

	rmnet_ipa3_ctx->tether_device[data->device_type].num_clients++;

	IPAWANDBG("Set the lan client info: %d, %d, %d\n",
		lan_client->client_idx,
		rmnet_ipa3_ctx->tether_device[data->device_type].ul_src_pipe,
		rmnet_ipa3_ctx->tether_device[data->device_type].num_clients);

	mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);

	return 0;
}

/* rmnet_ipa3_delete_lan_client_info() -
 * @data - IOCTL data
 *
 * This function handles WAN_IOC_DELETE_LAN_CLIENT_INFO.
 * It is used to delete LAN client information which
 * is used to fetch the packet stats for a client.
 *
 * Return codes:
 * 0: Success
 * -EINVAL: Invalid args provided
 */
int rmnet_ipa3_clear_lan_client_info(
	struct wan_ioctl_lan_client_info *data)
{
	struct ipa_lan_client *lan_client = NULL;
	struct ipa_tether_device_info *teth_ptr = NULL;

	IPAWANDBG("Client MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
		data->mac[0], data->mac[1], data->mac[2],
		data->mac[3], data->mac[4], data->mac[5]);

	/* Check if Device type is valid. */
	if (data->device_type >= IPACM_MAX_CLIENT_DEVICE_TYPES ||
		data->device_type < 0) {
		IPAWANERR("Invalid Device type: %d\n", data->device_type);
		return -EINVAL;
	}

	/* Check if Client index is valid. */
	if (data->client_idx >= IPA_MAX_NUM_HW_PATH_CLIENTS ||
		data->client_idx < 0) {
		IPAWANERR("Invalid Client Index: %d\n", data->client_idx);
		return -EINVAL;
	}

	IPAWANDBG("Client : %d:%d:%d\n",
		data->device_type, data->client_idx,
		rmnet_ipa3_ctx->tether_device[data->device_type].num_clients);

	teth_ptr = &rmnet_ipa3_ctx->tether_device[data->device_type];
	mutex_lock(&rmnet_ipa3_ctx->per_client_stats_guard);
	lan_client = &teth_ptr->lan_client[data->client_idx];

	if (!data->client_init) {
		/* check if the client is already de-inited. */
		if (!lan_client->inited) {
			IPAWANERR("Client already de-inited: %d:%d\n",
				data->device_type, data->client_idx);
			mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
			return -EINVAL;
		}
	}

	lan_client->inited = false;
	mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);

	return 0;
}


/* rmnet_ipa3_send_lan_client_msg() -
 * @data - IOCTL data
 *
 * This function handles WAN_IOC_SEND_LAN_CLIENT_MSG.
 * It is used to send LAN client information to IPACM.
 *
 * Return codes:
 * 0: Success
 * -EINVAL: Invalid args provided
 */
int rmnet_ipa3_send_lan_client_msg(
	struct wan_ioctl_send_lan_client_msg *data)
{
	struct ipa_msg_meta msg_meta;
	int rc;
	struct ipa_lan_client_msg *lan_client;

	/* Notify IPACM to reset the client index. */
	lan_client = kzalloc(sizeof(struct ipa_lan_client_msg),
		       GFP_KERNEL);
	if (!lan_client) {
		IPAWANERR("Can't allocate memory for tether_info\n");
		return -ENOMEM;
	}

	if (data->client_event != IPA_PER_CLIENT_STATS_CONNECT_EVENT &&
		data->client_event != IPA_PER_CLIENT_STATS_DISCONNECT_EVENT) {
		IPAWANERR("Wrong event given. Event:- %d\n",
			data->client_event);
		kfree(lan_client);
		return -EINVAL;
	}
	data->lan_client.lanIface[IPA_RESOURCE_NAME_MAX-1] = '\0';
	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	memcpy(lan_client, &data->lan_client,
		sizeof(struct ipa_lan_client_msg));
	msg_meta.msg_type = data->client_event;
	msg_meta.msg_len = sizeof(struct ipa_lan_client_msg);

	rc = ipa_send_msg(&msg_meta, lan_client, rmnet_ipa_free_msg);
	if (rc) {
		IPAWANERR("ipa_send_msg failed: %d\n", rc);
		kfree(lan_client);
		return rc;
	}
	return 0;
}

/* rmnet_ipa3_enable_per_client_stats() -
 * @data - IOCTL data
 *
 * This function handles WAN_IOC_ENABLE_PER_CLIENT_STATS.
 * It is used to indicate Q6 to start capturing per client stats.
 *
 * Return codes:
 * 0: Success
 * -EINVAL: Invalid args provided
 */
int rmnet_ipa3_enable_per_client_stats(
	bool *data)
{
	struct ipa_enable_per_client_stats_req_msg_v01 *req;
	struct ipa_enable_per_client_stats_resp_msg_v01 *resp;
	int rc;

	req =
	kzalloc(sizeof(struct ipa_enable_per_client_stats_req_msg_v01),
			GFP_KERNEL);
	if (!req) {
		IPAWANERR("Can't allocate memory for stats message\n");
		return -ENOMEM;
	}
	resp =
	kzalloc(sizeof(struct ipa_enable_per_client_stats_resp_msg_v01),
			GFP_KERNEL);
	if (!resp) {
		IPAWANERR("Can't allocate memory for stats message\n");
		kfree(req);
		return -ENOMEM;
	}
	memset(req, 0,
		sizeof(struct ipa_enable_per_client_stats_req_msg_v01));
	memset(resp, 0,
		sizeof(struct ipa_enable_per_client_stats_resp_msg_v01));

	if (*data)
		req->enable_per_client_stats = 1;
	else
		req->enable_per_client_stats = 0;

	rc = ipa3_qmi_enable_per_client_stats(req, resp);
	if (rc) {
		IPAWANERR("can't enable per client stats\n");
		kfree(req);
		kfree(resp);
		return rc;
	}

	kfree(req);
	kfree(resp);
	return 0;
}

int rmnet_ipa3_query_per_client_stats(
	struct wan_ioctl_query_per_client_stats *data)
{
	struct ipa_get_stats_per_client_req_msg_v01 *req;
	struct ipa_get_stats_per_client_resp_msg_v01 *resp;
	int rc, lan_clnt_idx, lan_clnt_idx1, i;
	struct ipa_lan_client *lan_client = NULL;
	struct ipa_tether_device_info *teth_ptr = NULL;

	IPAWANDBG("Client MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
		data->client_info[0].mac[0],
		data->client_info[0].mac[1],
		data->client_info[0].mac[2],
		data->client_info[0].mac[3],
		data->client_info[0].mac[4],
		data->client_info[0].mac[5]);

	/* Check if Device type is valid. */
	if (data->device_type >= IPACM_MAX_CLIENT_DEVICE_TYPES ||
		data->device_type < 0) {
		IPAWANERR("Invalid Device type: %d\n", data->device_type);
		return -EINVAL;
	}

	/* Check if num_clients is valid. */
	if (data->num_clients != IPA_MAX_NUM_HW_PATH_CLIENTS &&
		data->num_clients != 1) {
		IPAWANERR("Invalid number of clients: %d\n", data->num_clients);
		return -EINVAL;
	}

	mutex_lock(&rmnet_ipa3_ctx->per_client_stats_guard);

	/* Check if Source pipe is valid. */
	if (rmnet_ipa3_ctx->tether_device
		[data->device_type].ul_src_pipe == -1) {
		IPAWANERR("Device not initialized: %d\n", data->device_type);
		mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
		return -EINVAL;
	}

	/* Check if we have clients connected. */
	if (rmnet_ipa3_ctx->tether_device[data->device_type].num_clients == 0) {
		IPAWANERR("No clients connected: %d\n", data->device_type);
		mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
		return -EINVAL;
	}

	if (data->num_clients == 1) {
		/* Check if the client info is valid.*/
		lan_clnt_idx1 = rmnet_ipa3_get_lan_client_info(
			data->device_type,
			data->client_info[0].mac);
		if (lan_clnt_idx1 < 0) {
			IPAWANERR("Client info not available return.\n");
			mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
			return -EINVAL;
		}

		teth_ptr = &rmnet_ipa3_ctx->tether_device[data->device_type];
		lan_client = &teth_ptr->lan_client[lan_clnt_idx1];

		/*
		 * Check if disconnect flag is set and
		 * see if all the clients info are cleared.
		 */
		if (data->disconnect_clnt &&
			lan_client->inited) {
			IPAWANERR("Client not inited. Try again.\n");
			mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
			return -EAGAIN;
		}

	} else {
		/* Max number of clients. */
		/* Check if disconnect flag is set and
		 * see if all the clients info are cleared.
		 */
		if (data->disconnect_clnt &&
			rmnet_ipa3_check_any_client_inited(data->device_type)) {
			IPAWANERR("CLient not inited. Try again.\n");
			mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
			return -EAGAIN;
		}
		lan_clnt_idx1 = 0xffffffff;
	}

	req = kzalloc(sizeof(struct ipa_get_stats_per_client_req_msg_v01),
			GFP_KERNEL);
	if (!req) {
		IPAWANERR("Can't allocate memory for stats message\n");
		mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
		return -ENOMEM;
	}
	resp = kzalloc(sizeof(struct ipa_get_stats_per_client_resp_msg_v01),
			GFP_KERNEL);
	if (!resp) {
		IPAWANERR("Can't allocate memory for stats message\n");
		mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
		kfree(req);
		return -ENOMEM;
	}
	memset(req, 0, sizeof(struct ipa_get_stats_per_client_req_msg_v01));
	memset(resp, 0, sizeof(struct ipa_get_stats_per_client_resp_msg_v01));

	IPAWANDBG("Reset stats: %s",
		data->reset_stats?"Yes":"No");

	if (data->reset_stats) {
		req->reset_stats_valid = true;
		req->reset_stats = true;
		IPAWANDBG("fetch and reset the client stats\n");
	}

	req->client_id = lan_clnt_idx1;
	req->src_pipe_id =
		rmnet_ipa3_ctx->tether_device[data->device_type].ul_src_pipe;

	IPAWANDBG("fetch the client stats for %d, %d\n", req->client_id,
		req->src_pipe_id);

	rc = ipa3_qmi_get_per_client_packet_stats(req, resp);
	if (rc) {
		IPAWANERR("can't get per client stats\n");
		mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
		kfree(req);
		kfree(resp);
		return rc;
	}

	if (resp->per_client_stats_list_valid) {
		for (i = 0; i < resp->per_client_stats_list_len
				&& i < IPA_MAX_NUM_HW_PATH_CLIENTS; i++) {
			/* Subtract the header bytes from the DL bytes. */
			data->client_info[i].ipv4_rx_bytes =
			(resp->per_client_stats_list[i].num_dl_ipv4_bytes) -
			(teth_ptr->hdr_len *
			resp->per_client_stats_list[i].num_dl_ipv4_pkts);
			/* UL header bytes are subtracted by Q6. */
			data->client_info[i].ipv4_tx_bytes =
			resp->per_client_stats_list[i].num_ul_ipv4_bytes;
			/* Subtract the header bytes from the DL bytes. */
			data->client_info[i].ipv6_rx_bytes =
			(resp->per_client_stats_list[i].num_dl_ipv6_bytes) -
			(teth_ptr->hdr_len *
			resp->per_client_stats_list[i].num_dl_ipv6_pkts);
			/* UL header bytes are subtracted by Q6. */
			data->client_info[i].ipv6_tx_bytes =
			resp->per_client_stats_list[i].num_ul_ipv6_bytes;

			IPAWANDBG("tx_b_v4(%lu)v6(%lu)rx_b_v4(%lu) v6(%lu)\n",
			(unsigned long int) data->client_info[i].ipv4_tx_bytes,
			(unsigned long	int) data->client_info[i].ipv6_tx_bytes,
			(unsigned long int) data->client_info[i].ipv4_rx_bytes,
			(unsigned long int) data->client_info[i].ipv6_rx_bytes);

			/* Get the lan client index. */
			lan_clnt_idx = resp->per_client_stats_list[i].client_id;
			/* Check if lan_clnt_idx is valid. */
			if (lan_clnt_idx < 0 ||
				lan_clnt_idx >= IPA_MAX_NUM_HW_PATH_CLIENTS) {
				IPAWANERR("Lan client index not valid.\n");
				mutex_unlock(
				&rmnet_ipa3_ctx->per_client_stats_guard);
				kfree(req);
				kfree(resp);
				ipa_assert();
				return -EINVAL;
			}
			memcpy(data->client_info[i].mac,
				teth_ptr->lan_client[lan_clnt_idx].mac,
				IPA_MAC_ADDR_SIZE);
		}
	}

	IPAWANDBG("Disconnect clnt: %s",
		data->disconnect_clnt?"Yes":"No");

	if (data->disconnect_clnt) {
		rmnet_ipa3_delete_lan_client_info(data->device_type,
		lan_clnt_idx1);
	}

	mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
	kfree(req);
	kfree(resp);
	return 0;
}

int rmnet_ipa3_query_per_client_stats_v2(
		struct wan_ioctl_query_per_client_stats *data)
{
	int lan_clnt_idx, i, j;
	struct ipa_lan_client *lan_client = NULL;
	struct ipa_lan_client_cntr_index
		*lan_client_index = NULL;
	struct ipa_tether_device_info *teth_ptr = NULL;
	struct ipa_ioc_flt_rt_query query_f;
	struct ipa_ioc_flt_rt_query *query = &query_f;
	struct ipa_flt_rt_stats *fnr_stats = NULL;
	int ret = 1;

	/* Check if Device type is valid. */
	if (data->device_type >= IPACM_MAX_CLIENT_DEVICE_TYPES ||
			data->device_type < 0) {
		IPAWANERR("Invalid Device type: %d\n", data->device_type);
		return -EINVAL;
	}

	/* Check if num_clients is valid. */
	if (data->num_clients != IPA_MAX_NUM_HW_PATH_CLIENTS &&
			data->num_clients != 1) {
		IPAWANERR("Invalid number of clients: %d\n", data->num_clients);
		return -EINVAL;
	}

	mutex_lock(&rmnet_ipa3_ctx->per_client_stats_guard);

	/* Check if Source pipe is valid. */
	if (rmnet_ipa3_ctx->tether_device
			[data->device_type].ul_src_pipe == -1) {
		IPAWANERR("Device not initialized: %d\n", data->device_type);
		mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
		return -EINVAL;
	}

	/* Check if we have clients connected. */
	if (rmnet_ipa3_ctx->tether_device[data->device_type].num_clients == 0) {
		IPAWANERR("No clients connected: %d\n", data->device_type);
		mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
		return -EINVAL;
	}

	if (data->num_clients == 1) {
		/* Check if the client info is valid.*/
		lan_clnt_idx = rmnet_ipa3_get_lan_client_info(
				data->device_type,
				data->client_info[0].mac);
		if (lan_clnt_idx < 0) {
			IPAWANERR("Client info not available return.\n");
			mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
			return -EINVAL;
		}
	} else {
		/* Max number of clients. */
		/* Check if disconnect flag is set and
		 * see if all the clients info are cleared.
		 */
		if (data->disconnect_clnt &&
			rmnet_ipa3_check_any_client_inited(data->device_type)) {
			IPAWANERR("CLient not inited. Try again.\n");
			mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
			return -EAGAIN;
		}
		lan_clnt_idx = LAN_STATS_FOR_ALL_CLIENTS;
	}

	IPAWANDBG("Query stats for client index (0x%x)\n",
		lan_clnt_idx);

	teth_ptr = &rmnet_ipa3_ctx->tether_device[data->device_type];
	lan_client = teth_ptr->lan_client;
	lan_client_index = teth_ptr->lan_client_indices;

	if (lan_clnt_idx == LAN_STATS_FOR_ALL_CLIENTS) {
		i = 0;
		j = IPA_MAX_NUM_HW_PATH_CLIENTS;
	} else {
		i = lan_clnt_idx;
		j = i + 1;
	}

	for (; i < j; i++) {
		if (!lan_client[i].inited && !data->disconnect_clnt)
			continue;

		IPAWANDBG("Client MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
				lan_client[i].mac[0],
				lan_client[i].mac[1],
				lan_client[i].mac[2],
				lan_client[i].mac[3],
				lan_client[i].mac[4],
				lan_client[i].mac[5]);
		IPAWANDBG("Lan client %d inited\n", i);
		IPAWANDBG("Query stats ul/dl indices = %u/%u\n",
				lan_client_index[i].ul_cnt_idx,
				lan_client_index[i].dl_cnt_idx);
		memset(query, 0, sizeof(query_f));
		ret = rmnet_ipa_get_hw_fnr_stats_v2(&lan_client_index[i],
				data, query);
		if (ret) {
			IPAWANERR("Failed: Client type %d, idx %d\n",
					data->device_type, i);
			kfree((void *)query->stats);
			continue;
		}
		fnr_stats = &((struct ipa_flt_rt_stats *)
				query->stats)[0];
		data->client_info[i].ipv4_tx_bytes =
			fnr_stats->num_bytes;
		fnr_stats = &((struct ipa_flt_rt_stats *)
				query->stats)[1];
		data->client_info[i].ipv4_rx_bytes =
			fnr_stats->num_bytes;
		memcpy(data->client_info[i].mac,
				lan_client[i].mac,
				IPA_MAC_ADDR_SIZE);

		IPAWANDBG("Client ipv4_tx_bytes = %llu, ipv4_rx_bytes = %llu\n",
				data->client_info[i].ipv4_tx_bytes,
				data->client_info[i].ipv4_rx_bytes);

		kfree((void *)query->stats);
	}

	/* Legacy per-client stats */
	IPAWANDBG("Disconnect clnt: %s",
			data->disconnect_clnt?"Yes":"No");

	if (data->disconnect_clnt) {
		rmnet_ipa3_delete_lan_client_info(data->device_type,
				lan_clnt_idx);
	}

	mutex_unlock(&rmnet_ipa3_ctx->per_client_stats_guard);
	return ret;
}

static int __init ipa3_wwan_init(void)
{
	int i, j;
	struct ipa_tether_device_info *teth_ptr = NULL;
	void *ssr_hdl;

	if (!ipa3_ctx) {
		IPAWANERR_RL("ipa3_ctx was not initialized\n");
		return -EINVAL;
	}
	rmnet_ipa3_ctx = kzalloc(sizeof(*rmnet_ipa3_ctx), GFP_KERNEL);

	if (!rmnet_ipa3_ctx)
		return -ENOMEM;


	atomic_set(&rmnet_ipa3_ctx->is_initialized, 0);
	atomic_set(&rmnet_ipa3_ctx->is_ssr, 0);

	mutex_init(&rmnet_ipa3_ctx->pipe_handle_guard);
	mutex_init(&rmnet_ipa3_ctx->add_mux_channel_lock);
	mutex_init(&rmnet_ipa3_ctx->per_client_stats_guard);
	/* Reset the Lan Stats. */
	for (i = 0; i < IPACM_MAX_CLIENT_DEVICE_TYPES; i++) {
		teth_ptr = &rmnet_ipa3_ctx->tether_device[i];
		teth_ptr->ul_src_pipe = -1;

		for (j = 0; j < IPA_MAX_NUM_HW_PATH_CLIENTS; j++)
			teth_ptr->lan_client[j].client_idx = -1;
	}
	rmnet_ipa3_ctx->ipa3_to_apps_hdl = -1;
	rmnet_ipa3_ctx->apps_to_ipa3_hdl = -1;

	ipa3_qmi_init();

	/* Register for Local Modem SSR */
	ssr_hdl = subsys_notif_register_notifier(SUBSYS_LOCAL_MODEM,
		&ipa3_lcl_mdm_ssr_notifier);
	if (!IS_ERR(ssr_hdl))
		rmnet_ipa3_ctx->lcl_mdm_subsys_notify_handle = ssr_hdl;
	else if (!rmnet_ipa3_ctx->ipa_config_is_apq)
		return (int)PTR_ERR(ssr_hdl);

	if (rmnet_ipa3_ctx->ipa_config_is_apq) {
		/* Register for Remote Modem SSR */
		ssr_hdl = subsys_notif_register_notifier(SUBSYS_REMOTE_MODEM,
			&ipa3_rmt_mdm_ssr_notifier);
		if (IS_ERR(ssr_hdl)) {
			if (rmnet_ipa3_ctx->lcl_mdm_subsys_notify_handle) {
				subsys_notif_unregister_notifier(
				rmnet_ipa3_ctx->lcl_mdm_subsys_notify_handle,
				&ipa3_lcl_mdm_ssr_notifier);
				rmnet_ipa3_ctx->lcl_mdm_subsys_notify_handle =
					NULL;
			}
			return (int)PTR_ERR(ssr_hdl);
		}
		rmnet_ipa3_ctx->rmt_mdm_subsys_notify_handle = ssr_hdl;
	}

	return platform_driver_register(&rmnet_ipa_driver);
}

static void __exit ipa3_wwan_cleanup(void)
{
	int ret;

	platform_driver_unregister(&rmnet_ipa_driver);
	if (rmnet_ipa3_ctx->lcl_mdm_subsys_notify_handle) {
		ret = subsys_notif_unregister_notifier(
			rmnet_ipa3_ctx->lcl_mdm_subsys_notify_handle,
			&ipa3_lcl_mdm_ssr_notifier);
		if (ret)
			IPAWANERR(
			"Failed to unregister subsys %s notifier ret=%d\n",
			SUBSYS_LOCAL_MODEM, ret);
	}
	if (rmnet_ipa3_ctx->rmt_mdm_subsys_notify_handle) {
		ret = subsys_notif_unregister_notifier(
			rmnet_ipa3_ctx->rmt_mdm_subsys_notify_handle,
			&ipa3_rmt_mdm_ssr_notifier);
		if (ret)
			IPAWANERR(
			"Failed to unregister subsys %s notifier ret=%d\n",
			SUBSYS_REMOTE_MODEM, ret);
	}
	ipa3_qmi_cleanup();
	mutex_destroy(&rmnet_ipa3_ctx->per_client_stats_guard);
	mutex_destroy(&rmnet_ipa3_ctx->add_mux_channel_lock);
	mutex_destroy(&rmnet_ipa3_ctx->pipe_handle_guard);
	kfree(rmnet_ipa3_ctx);
	rmnet_ipa3_ctx = NULL;
}

static void ipa3_wwan_msg_free_cb(void *buff, u32 len, u32 type)
{
	kfree(buff);
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
