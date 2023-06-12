// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/if_ether.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msm_ipa.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/ipv6.h>
#include <net/addrconf.h>
#include <linux/ipa.h>
#include <linux/cdev.h>
#include <linux/ipa_odu_bridge.h>
#include "ipa_common_i.h"
#include "ipa_pm.h"
#include "ipa_i.h"
#include <linux/ipa_fmwk.h>

#define IPA_GSB_DRV_NAME "ipa_gsb"

#define MAX_SUPPORTED_IFACE 5

#define IPA_GSB_DBG(fmt, args...) \
	do { \
		pr_debug(IPA_GSB_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf(), \
			IPA_GSB_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf_low(), \
			IPA_GSB_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_GSB_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPA_GSB_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf_low(), \
			IPA_GSB_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_GSB_ERR(fmt, args...) \
	do { \
		pr_err(IPA_GSB_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf(), \
			IPA_GSB_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf_low(), \
			IPA_GSB_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_GSB_ERR_RL(fmt, args...) \
	do { \
		pr_err_ratelimited_ipa(IPA_GSB_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf(), \
			IPA_GSB_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf_low(), \
			IPA_GSB_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_GSB_MAX_MSG_LEN 512

#ifdef CONFIG_DEBUG_FS
static struct dentry *dent;
static struct dentry *dfile_stats;
static char dbg_buff[IPA_GSB_MAX_MSG_LEN];
#endif

#define IPA_GSB_SKB_HEADROOM 256
#define IPA_GSB_SKB_DUMMY_HEADER 42
#define IPA_GSB_AGGR_BYTE_LIMIT 14
#define IPA_GSB_AGGR_TIME_LIMIT 1000 /* 1000 us */


/**
 * struct stats - driver statistics,
 * @num_ul_packets: number of uplink packets
 * @num_dl_packets: number of downlink packets
 * @num_insufficient_headroom_packets: number of
	packets with insufficient headroom
 */
struct stats {
	u64 num_ul_packets;
	u64 num_dl_packets;
	u64 num_insufficient_headroom_packets;
};

/**
 * struct ipa_gsb_mux_hdr - ipa gsb mux header,
 * @iface_hdl: interface handle
 * @qmap_id: qmap id
 * @pkt_size: packet size
 */
struct ipa_gsb_mux_hdr {
	u8 iface_hdl;
	u8 qmap_id;
	u16 pkt_size;
};

/**
 * struct ipa_gsb_iface_info - GSB interface information
 * @netdev_name: network interface name
 * @device_ethaddr: network interface ethernet address
 * @priv: client's private data. to be used in client's callbacks
 * @tx_dp_notify: client callback for handling IPA ODU_PROD callback
 * @send_dl_skb: client callback for sending skb in downlink direction
 * @iface_stats: statistics, how many packets were transmitted
 * using the SW bridge.
 * @partial_hdr_hdl: handle for partial header
 * @wakeup_request: client callback to wakeup
 * @is_conencted: is interface connected ?
 * @is_resumed: is interface resumed ?
 * @iface_hdl: interface handle
 */
struct ipa_gsb_iface_info {
	char netdev_name[IPA_RESOURCE_NAME_MAX];
	u8 device_ethaddr[ETH_ALEN];
	void *priv;
	ipa_notify_cb tx_dp_notify;
	int (*send_dl_skb)(void *priv, struct sk_buff *skb);
	struct stats iface_stats;
	uint32_t partial_hdr_hdl[IPA_IP_MAX];
	void (*wakeup_request)(void *cl_priv);
	bool is_connected;
	bool is_resumed;
	u8 iface_hdl;
};

/**
 * struct ipa_gsb_context - GSB driver context information
 * @logbuf: buffer of ipc logging
 * @logbuf_low: buffer of ipc logging (low priority)
 * @lock: global mutex lock for global variables
 * @prod_hdl: handle for prod pipe
 * @cons_hdl: handle for cons pipe
 * @ipa_sys_desc_size: sys pipe desc size
 * @num_iface: number of interface
 * @iface_hdl: interface handles
 * @num_connected_iface: number of connected interface
 * @num_resumed_iface: number of resumed interface
 * @iface: interface information
 * @iface_lock: interface mutex lock for control path
 * @iface_spinlock: interface spinlock for data path
 * @pm_hdl: IPA PM handle
 */
struct ipa_gsb_context {
	void *logbuf;
	void *logbuf_low;
	struct mutex lock;
	u32 prod_hdl;
	u32 cons_hdl;
	u32 ipa_sys_desc_size;
	int num_iface;
	bool iface_hdl[MAX_SUPPORTED_IFACE];
	int num_connected_iface;
	int num_resumed_iface;
	struct ipa_gsb_iface_info *iface[MAX_SUPPORTED_IFACE];
	struct mutex iface_lock[MAX_SUPPORTED_IFACE];
	spinlock_t iface_spinlock[MAX_SUPPORTED_IFACE];
	u32 pm_hdl;
	atomic_t disconnect_in_progress;
	atomic_t suspend_in_progress;
};

static struct ipa_gsb_context *ipa_gsb_ctx;

#ifdef CONFIG_DEBUG_FS
static ssize_t ipa_gsb_debugfs_stats(struct file *file,
				  char __user *ubuf,
				  size_t count,
				  loff_t *ppos)
{
	int i, nbytes = 0;
	struct ipa_gsb_iface_info *iface = NULL;
	struct stats iface_stats;

	for (i = 0; i < MAX_SUPPORTED_IFACE; i++) {
		iface = ipa_gsb_ctx->iface[i];
		if (iface != NULL) {
			iface_stats = iface->iface_stats;
			nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_GSB_MAX_MSG_LEN - nbytes,
				"netdev: %s\n",
				iface->netdev_name);

			nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_GSB_MAX_MSG_LEN - nbytes,
				"UL packets: %lld\n",
				iface_stats.num_ul_packets);

			nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_GSB_MAX_MSG_LEN - nbytes,
				"DL packets: %lld\n",
				iface_stats.num_dl_packets);

			nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_GSB_MAX_MSG_LEN - nbytes,
				"packets with insufficient headroom: %lld\n",
				iface_stats.num_insufficient_headroom_packets);
		}
	}
	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static const struct file_operations ipa_gsb_stats_ops = {
	.read = ipa_gsb_debugfs_stats,
};

static void ipa_gsb_debugfs_init(void)
{
	const mode_t read_only_mode = 00444;

	dent = debugfs_create_dir("ipa_gsb", NULL);
	if (IS_ERR(dent)) {
		IPA_GSB_ERR("fail to create folder ipa_gsb\n");
		return;
	}

	dfile_stats =
		debugfs_create_file("stats", read_only_mode, dent,
					NULL, &ipa_gsb_stats_ops);
	if (!dfile_stats || IS_ERR(dfile_stats)) {
		IPA_GSB_ERR("fail to create file stats\n");
		goto fail;
	}

	return;

fail:
	debugfs_remove_recursive(dent);
}

static void ipa_gsb_debugfs_destroy(void)
{
	debugfs_remove_recursive(dent);
}
#else
static void ipa_gsb_debugfs_init(void)
{
}

static void ipa_gsb_debugfs_destroy(void)
{
}
#endif

static int ipa_gsb_driver_init(struct odu_bridge_params *params)
{
	int i;

	if (!ipa_is_ready()) {
		IPA_GSB_ERR("IPA is not ready\n");
		return -EFAULT;
	}

	ipa_gsb_ctx = kzalloc(sizeof(*ipa_gsb_ctx),
		GFP_KERNEL);

	if (!ipa_gsb_ctx)
		return -ENOMEM;

	mutex_init(&ipa_gsb_ctx->lock);
	for (i = 0; i < MAX_SUPPORTED_IFACE; i++) {
		mutex_init(&ipa_gsb_ctx->iface_lock[i]);
		spin_lock_init(&ipa_gsb_ctx->iface_spinlock[i]);
	}
	ipa_gsb_debugfs_init();

	return 0;
}

static int ipa_gsb_commit_partial_hdr(struct ipa_gsb_iface_info *iface_info)
{
	int i;
	struct ipa_ioc_add_hdr *hdr;

	if (!iface_info) {
		IPA_GSB_ERR("invalid input\n");
		return -EINVAL;
	}

	hdr = kzalloc(sizeof(struct ipa_ioc_add_hdr) +
		2 * sizeof(struct ipa_hdr_add), GFP_KERNEL);
	if (!hdr)
		return -ENOMEM;

	hdr->commit = 1;
	hdr->num_hdrs = 2;

	snprintf(hdr->hdr[0].name, sizeof(hdr->hdr[0].name),
			 "%s_ipv4", iface_info->netdev_name);
	snprintf(hdr->hdr[1].name, sizeof(hdr->hdr[1].name),
			 "%s_ipv6", iface_info->netdev_name);
	/*
	 * partial header:
	 * [hdl][QMAP ID][pkt size][Dummy Header][ETH header]
	 */
	for (i = IPA_IP_v4; i < IPA_IP_MAX; i++) {
		/*
		 * Optimization: add dummy header to reserve space
		 * for rndis header, so we can do the skb_clone
		 * instead of deep copy.
		 */
		hdr->hdr[i].hdr_len = ETH_HLEN +
			sizeof(struct ipa_gsb_mux_hdr) +
			IPA_GSB_SKB_DUMMY_HEADER;
		hdr->hdr[i].type = IPA_HDR_L2_ETHERNET_II;
		hdr->hdr[i].is_partial = 1;
		hdr->hdr[i].is_eth2_ofst_valid = 1;
		hdr->hdr[i].eth2_ofst = sizeof(struct ipa_gsb_mux_hdr) +
			IPA_GSB_SKB_DUMMY_HEADER;
		/* populate iface handle */
		hdr->hdr[i].hdr[0] = iface_info->iface_hdl;
		/* populate src ETH address */
		memcpy(&hdr->hdr[i].hdr[10 + IPA_GSB_SKB_DUMMY_HEADER],
			iface_info->device_ethaddr, 6);
		/* populate Ethertype */
		if (i == IPA_IP_v4)
			*(u16 *)(hdr->hdr[i].hdr + 16 +
				IPA_GSB_SKB_DUMMY_HEADER) = htons(ETH_P_IP);
		else
			*(u16 *)(hdr->hdr[i].hdr + 16 +
				IPA_GSB_SKB_DUMMY_HEADER) = htons(ETH_P_IPV6);
	}

	if (ipa3_add_hdr(hdr)) {
		IPA_GSB_ERR("fail to add partial headers\n");
		kfree(hdr);
		return -EFAULT;
	}

	for (i = IPA_IP_v4; i < IPA_IP_MAX; i++)
		iface_info->partial_hdr_hdl[i] =
			hdr->hdr[i].hdr_hdl;

	IPA_GSB_DBG("added partial hdr hdl for ipv4: %d\n",
		iface_info->partial_hdr_hdl[IPA_IP_v4]);
	IPA_GSB_DBG("added partial hdr hdl for ipv6: %d\n",
		iface_info->partial_hdr_hdl[IPA_IP_v6]);

	kfree(hdr);
	return 0;
}

static void ipa_gsb_delete_partial_hdr(struct ipa_gsb_iface_info *iface_info)
{
	struct ipa_ioc_del_hdr *del_hdr;

	del_hdr = kzalloc(sizeof(struct ipa_ioc_del_hdr) +
		2 * sizeof(struct ipa_hdr_del), GFP_KERNEL);
	if (!del_hdr)
		return;

	del_hdr->commit = 1;
	del_hdr->num_hdls = 2;
	del_hdr->hdl[IPA_IP_v4].hdl = iface_info->partial_hdr_hdl[IPA_IP_v4];
	del_hdr->hdl[IPA_IP_v6].hdl = iface_info->partial_hdr_hdl[IPA_IP_v6];

	if (ipa3_del_hdr(del_hdr) != 0)
		IPA_GSB_ERR("failed to delete partial hdr\n");

	IPA_GSB_DBG("deleted partial hdr hdl for ipv4: %d\n",
		iface_info->partial_hdr_hdl[IPA_IP_v4]);
	IPA_GSB_DBG("deleted partial hdr hdl for ipv6: %d\n",
		iface_info->partial_hdr_hdl[IPA_IP_v6]);

	kfree(del_hdr);
}

static int ipa_gsb_reg_intf_props(struct ipa_gsb_iface_info *iface_info)
{
	struct ipa_tx_intf tx;
	struct ipa_rx_intf rx;
	struct ipa_ioc_tx_intf_prop tx_prop[2];
	struct ipa_ioc_rx_intf_prop rx_prop[2];

	/* populate tx prop */
	tx.num_props = 2;
	tx.prop = tx_prop;

	memset(tx_prop, 0, sizeof(tx_prop));
	tx_prop[0].ip = IPA_IP_v4;
	tx_prop[0].dst_pipe = IPA_CLIENT_ODU_EMB_CONS;
	tx_prop[0].hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	snprintf(tx_prop[0].hdr_name, sizeof(tx_prop[0].hdr_name),
			 "%s_ipv4", iface_info->netdev_name);

	tx_prop[1].ip = IPA_IP_v6;
	tx_prop[1].dst_pipe = IPA_CLIENT_ODU_EMB_CONS;
	tx_prop[1].hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	snprintf(tx_prop[1].hdr_name, sizeof(tx_prop[1].hdr_name),
			 "%s_ipv6", iface_info->netdev_name);

	/* populate rx prop */
	rx.num_props = 2;
	rx.prop = rx_prop;

	memset(rx_prop, 0, sizeof(rx_prop));
	rx_prop[0].ip = IPA_IP_v4;
	rx_prop[0].src_pipe = IPA_CLIENT_ODU_PROD;
	rx_prop[0].hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	rx_prop[0].attrib.attrib_mask |= IPA_FLT_META_DATA;
	rx_prop[0].attrib.meta_data = iface_info->iface_hdl;
	rx_prop[0].attrib.meta_data_mask = 0xFF;

	rx_prop[1].ip = IPA_IP_v6;
	rx_prop[1].src_pipe = IPA_CLIENT_ODU_PROD;
	rx_prop[1].hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	rx_prop[1].attrib.attrib_mask |= IPA_FLT_META_DATA;
	rx_prop[1].attrib.meta_data = iface_info->iface_hdl;
	rx_prop[1].attrib.meta_data_mask = 0xFF;

	if (ipa3_register_intf(iface_info->netdev_name, &tx, &rx)) {
		IPA_GSB_ERR("fail to add interface prop\n");
		return -EFAULT;
	}

	return 0;
}

static void ipa_gsb_dereg_intf_props(struct ipa_gsb_iface_info *iface_info)
{
	if (ipa3_deregister_intf(iface_info->netdev_name) != 0)
		IPA_GSB_ERR("fail to dereg intf props\n");

	IPA_GSB_DBG("deregistered iface props for %s\n",
		iface_info->netdev_name);
}

static void ipa_gsb_pm_cb(void *user_data, enum ipa_pm_cb_event event)
{
	int i;

	if (event != IPA_PM_REQUEST_WAKEUP) {
		IPA_GSB_ERR("Unexpected event %d\n", event);
		WARN_ON(1);
		return;
	}

	IPA_GSB_DBG_LOW("wake up clients\n");
	for (i = 0; i < MAX_SUPPORTED_IFACE; i++)
		if (ipa_gsb_ctx->iface[i] != NULL)
			ipa_gsb_ctx->iface[i]->wakeup_request(
				ipa_gsb_ctx->iface[i]->priv);
}

static int ipa_gsb_register_pm(void)
{
	struct ipa_pm_register_params reg_params;
	int ret;

	memset(&reg_params, 0, sizeof(reg_params));
	reg_params.name = "ipa_gsb";
	reg_params.callback = ipa_gsb_pm_cb;
	reg_params.user_data = NULL;
	reg_params.group = IPA_PM_GROUP_DEFAULT;

	ret = ipa_pm_register(&reg_params,
		&ipa_gsb_ctx->pm_hdl);
	if (ret) {
		IPA_GSB_ERR("fail to register with PM %d\n", ret);
		goto fail_pm_reg;
	}
	IPA_GSB_DBG("ipa pm hdl: %d\n", ipa_gsb_ctx->pm_hdl);

	ret = ipa_pm_associate_ipa_cons_to_client(ipa_gsb_ctx->pm_hdl,
		IPA_CLIENT_ODU_EMB_CONS);
	if (ret) {
		IPA_GSB_ERR("fail to associate cons with PM %d\n", ret);
		goto fail_pm_cons;
	}

	return 0;

fail_pm_cons:
	ipa_pm_deregister(ipa_gsb_ctx->pm_hdl);
	ipa_gsb_ctx->pm_hdl = ~0;
fail_pm_reg:
	return ret;
}

static int ipa_bridge_init_internal(struct ipa_bridge_init_params *params, u32 *hdl)
{
	int i, ret;
	struct ipa_gsb_iface_info *new_intf;

	if (!params || !params->wakeup_request || !hdl ||
		!params->info.netdev_name || !params->info.tx_dp_notify ||
		!params->info.send_dl_skb) {
		IPA_GSB_ERR("Invalid parameters\n");
		return -EINVAL;
	}

	IPA_GSB_DBG("netdev_name: %s\n", params->info.netdev_name);

	if (ipa_gsb_ctx == NULL) {
		ret = ipa_gsb_driver_init(&params->info);
		if (ret) {
			IPA_GSB_ERR("fail to init ipa gsb driver\n");
			return -EFAULT;
		}
		ipa_gsb_ctx->ipa_sys_desc_size =
			params->info.ipa_desc_size;
		IPA_GSB_DBG("desc size: %d\n", ipa_gsb_ctx->ipa_sys_desc_size);
	}

	mutex_lock(&ipa_gsb_ctx->lock);

	if (params->info.ipa_desc_size != ipa_gsb_ctx->ipa_sys_desc_size) {
		IPA_GSB_ERR("unmatch: orig desc size %d, new desc size %d\n",
			ipa_gsb_ctx->ipa_sys_desc_size,
			params->info.ipa_desc_size);
		mutex_unlock(&ipa_gsb_ctx->lock);
		return -EFAULT;
	}

	for (i = 0; i < MAX_SUPPORTED_IFACE; i++)
		if (ipa_gsb_ctx->iface[i] != NULL &&
			strnlen(ipa_gsb_ctx->iface[i]->netdev_name,
					IPA_RESOURCE_NAME_MAX) ==
			strnlen(params->info.netdev_name,
					IPA_RESOURCE_NAME_MAX) &&
			strcmp(ipa_gsb_ctx->iface[i]->netdev_name,
				params->info.netdev_name) == 0) {
			IPA_GSB_ERR("intf was added before.\n");
			mutex_unlock(&ipa_gsb_ctx->lock);
			return -EFAULT;
		}

	if (ipa_gsb_ctx->num_iface == MAX_SUPPORTED_IFACE) {
		IPA_GSB_ERR("reached maximum supported interfaces");
		mutex_unlock(&ipa_gsb_ctx->lock);
		return -EFAULT;
	}

	for (i = 0; i < MAX_SUPPORTED_IFACE; i++)
		if (!ipa_gsb_ctx->iface_hdl[i]) {
			ipa_gsb_ctx->iface_hdl[i] = true;
			*hdl = i;
			IPA_GSB_DBG("iface hdl: %d\n", *hdl);
			break;
		}

	IPA_GSB_DBG("intf was not added before, proceed.\n");
	new_intf = kzalloc(sizeof(*new_intf), GFP_KERNEL);
	if (new_intf == NULL) {
		ret = -ENOMEM;
		goto fail_alloc_mem;
	}

	strlcpy(new_intf->netdev_name, params->info.netdev_name,
		sizeof(new_intf->netdev_name));
	new_intf->wakeup_request = params->wakeup_request;
	new_intf->priv = params->info.priv;
	new_intf->tx_dp_notify = params->info.tx_dp_notify;
	new_intf->send_dl_skb = params->info.send_dl_skb;
	new_intf->iface_hdl = *hdl;
	memcpy(new_intf->device_ethaddr, params->info.device_ethaddr,
		sizeof(new_intf->device_ethaddr));

	if (ipa_gsb_commit_partial_hdr(new_intf) != 0) {
		IPA_GSB_ERR("fail to commit partial hdrs\n");
		ret = -EFAULT;
		goto fail_partial_hdr;
	}

	if (ipa_gsb_reg_intf_props(new_intf) != 0) {
		IPA_GSB_ERR("fail to register interface props\n");
		ret = -EFAULT;
		goto fail_reg_intf_props;
	}

	if (ipa_gsb_ctx->num_iface == 0) {
		ret = ipa_gsb_register_pm();
		if (ret) {
			IPA_GSB_ERR("fail to register with IPA PM %d\n", ret);
			ret = -EFAULT;
			goto fail_register_pm;
		}
	}

	ipa_gsb_ctx->iface[*hdl] = new_intf;
	ipa_gsb_ctx->num_iface++;
	IPA_GSB_DBG("num_iface %d\n", ipa_gsb_ctx->num_iface);
	mutex_unlock(&ipa_gsb_ctx->lock);
	return 0;

fail_register_pm:
	ipa_gsb_dereg_intf_props(new_intf);
fail_reg_intf_props:
	ipa_gsb_delete_partial_hdr(new_intf);
fail_partial_hdr:
	kfree(new_intf);
fail_alloc_mem:
	ipa_gsb_ctx->iface_hdl[*hdl] = false;
	mutex_unlock(&ipa_gsb_ctx->lock);
	return ret;
}

static void ipa_gsb_deregister_pm(void)
{
	IPA_GSB_DBG("deregister ipa pm hdl: %d\n", ipa_gsb_ctx->pm_hdl);
	ipa_pm_deactivate_sync(ipa_gsb_ctx->pm_hdl);
	ipa_pm_deregister(ipa_gsb_ctx->pm_hdl);
	ipa_gsb_ctx->pm_hdl = ~0;
}

static int ipa_bridge_cleanup_internal(u32 hdl)
{
	int i;

	if (!ipa_gsb_ctx) {
		IPA_GSB_ERR("ipa_gsb_ctx was not initialized\n");
		return -EFAULT;
	}

	if (hdl >= MAX_SUPPORTED_IFACE) {
		IPA_GSB_ERR("invalid hdl: %d\n", hdl);
		return -EINVAL;
	}

	mutex_lock(&ipa_gsb_ctx->iface_lock[hdl]);
	if (!ipa_gsb_ctx->iface[hdl]) {
		IPA_GSB_ERR("fail to find interface, hdl: %d\n", hdl);
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return -EFAULT;
	}

	IPA_GSB_DBG("client hdl: %d\n", hdl);

	if (ipa_gsb_ctx->iface[hdl]->is_connected) {
		IPA_GSB_ERR("cannot cleanup when iface is connected\n");
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return -EFAULT;
	}
	ipa_gsb_dereg_intf_props(ipa_gsb_ctx->iface[hdl]);
	ipa_gsb_delete_partial_hdr(ipa_gsb_ctx->iface[hdl]);
	spin_lock_bh(&ipa_gsb_ctx->iface_spinlock[hdl]);
	kfree(ipa_gsb_ctx->iface[hdl]);
	ipa_gsb_ctx->iface[hdl] = NULL;
	ipa_gsb_ctx->iface_hdl[hdl] = false;
	spin_unlock_bh(&ipa_gsb_ctx->iface_spinlock[hdl]);
	mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
	mutex_lock(&ipa_gsb_ctx->lock);
	ipa_gsb_ctx->num_iface--;
	IPA_GSB_DBG("num_iface %d\n", ipa_gsb_ctx->num_iface);
	if (ipa_gsb_ctx->num_iface == 0) {
		ipa_gsb_deregister_pm();
		ipa_gsb_debugfs_destroy();
		ipc_log_context_destroy(ipa_gsb_ctx->logbuf);
		ipc_log_context_destroy(ipa_gsb_ctx->logbuf_low);
		mutex_unlock(&ipa_gsb_ctx->lock);
		mutex_destroy(&ipa_gsb_ctx->lock);
		for (i = 0; i < MAX_SUPPORTED_IFACE; i++)
			mutex_destroy(&ipa_gsb_ctx->iface_lock[i]);
		kfree(ipa_gsb_ctx);
		ipa_gsb_ctx = NULL;
		return 0;
	}
	mutex_unlock(&ipa_gsb_ctx->lock);
	return 0;
}

static void ipa_gsb_cons_cb(void *priv, enum ipa_dp_evt_type evt,
	unsigned long data)
{
	struct sk_buff *skb;
	struct sk_buff *skb2;
	struct ipa_gsb_mux_hdr *mux_hdr;
	u16 pkt_size, pad_byte;
	u8 hdl;

	if (evt != IPA_RECEIVE) {
		IPA_GSB_ERR("unexpected event\n");
		WARN_ON(1);
		return;
	}

	skb = (struct sk_buff *)data;

	if (skb == NULL) {
		IPA_GSB_ERR("unexpected NULL data\n");
		WARN_ON(1);
		return;
	}

	while (skb->len) {
		mux_hdr = (struct ipa_gsb_mux_hdr *)skb->data;
		pkt_size = mux_hdr->pkt_size;
		/* 4-byte padding */
		pad_byte = ((pkt_size + sizeof(*mux_hdr) + ETH_HLEN +
			3 + IPA_GSB_SKB_DUMMY_HEADER) & ~3) -
			(pkt_size + sizeof(*mux_hdr) +
			ETH_HLEN + IPA_GSB_SKB_DUMMY_HEADER);
		hdl = mux_hdr->iface_hdl;
		if (hdl >= MAX_SUPPORTED_IFACE) {
			IPA_GSB_ERR("invalid hdl: %d\n", hdl);
			break;
		}
		IPA_GSB_DBG_LOW("pkt_size: %d, pad_byte: %d, hdl: %d\n",
			pkt_size, pad_byte, hdl);

		/* remove 4 byte mux header AND dummy header*/
		skb_pull(skb, sizeof(*mux_hdr) + IPA_GSB_SKB_DUMMY_HEADER);

		skb2 = skb_clone(skb, GFP_KERNEL);
		if (!skb2) {
			IPA_GSB_ERR("skb_clone failed\n");
			WARN_ON(1);
			break;
		}
		skb_trim(skb2, pkt_size + ETH_HLEN);
		spin_lock_bh(&ipa_gsb_ctx->iface_spinlock[hdl]);
		if (ipa_gsb_ctx->iface[hdl] != NULL) {
			ipa_gsb_ctx->iface[hdl]->send_dl_skb(
				ipa_gsb_ctx->iface[hdl]->priv, skb2);
			ipa_gsb_ctx->iface[hdl]->iface_stats.num_dl_packets++;
			spin_unlock_bh(&ipa_gsb_ctx->iface_spinlock[hdl]);
			skb_pull(skb, pkt_size + ETH_HLEN + pad_byte);
		} else {
			IPA_GSB_ERR("Invalid hdl: %d, drop the skb\n", hdl);
			spin_unlock_bh(&ipa_gsb_ctx->iface_spinlock[hdl]);
			dev_kfree_skb_any(skb2);
			break;
		}
	}

	if (skb) {
		dev_kfree_skb_any(skb);
		skb = NULL;
	}
}

static void ipa_gsb_tx_dp_notify(void *priv, enum ipa_dp_evt_type evt,
	unsigned long data)
{
	struct sk_buff *skb;
	struct ipa_gsb_mux_hdr *mux_hdr;
	u8 hdl;

	skb = (struct sk_buff *)data;

	if (skb == NULL) {
		IPA_GSB_ERR("unexpected NULL data\n");
		WARN_ON(1);
		return;
	}

	if (evt != IPA_WRITE_DONE && evt != IPA_RECEIVE) {
		IPA_GSB_ERR("unexpected event: %d\n", evt);
		dev_kfree_skb_any(skb);
		return;
	}

	/* fetch iface handle from header */
	mux_hdr = (struct ipa_gsb_mux_hdr *)skb->data;
	/* change to host order */
	*(u32 *)mux_hdr = ntohl(*(u32 *)mux_hdr);
	hdl = mux_hdr->iface_hdl;
	if ((hdl < 0) || (hdl >= MAX_SUPPORTED_IFACE) ||
		!ipa_gsb_ctx->iface[hdl]) {
		IPA_GSB_ERR("invalid hdl: %d and cb, drop the skb\n", hdl);
		dev_kfree_skb_any(skb);
		return;
	}
	IPA_GSB_DBG_LOW("evt: %d, hdl in tx_dp_notify: %d\n", evt, hdl);

	/* remove 4 byte mux header */
	skb_pull(skb, sizeof(struct ipa_gsb_mux_hdr));
	ipa_gsb_ctx->iface[hdl]->tx_dp_notify(
	   ipa_gsb_ctx->iface[hdl]->priv, evt,
	   (unsigned long)skb);
}

static int ipa_gsb_connect_sys_pipe(void)
{
	struct ipa_sys_connect_params prod_params;
	struct ipa_sys_connect_params cons_params;
	int res;

	memset(&prod_params, 0, sizeof(prod_params));
	memset(&cons_params, 0, sizeof(cons_params));

	/* configure RX EP */
	prod_params.client = IPA_CLIENT_ODU_PROD;
	prod_params.ipa_ep_cfg.hdr.hdr_len =
		ETH_HLEN + sizeof(struct ipa_gsb_mux_hdr);
	prod_params.ipa_ep_cfg.nat.nat_en = IPA_SRC_NAT;
	prod_params.ipa_ep_cfg.hdr.hdr_ofst_metadata_valid = 1;
	prod_params.ipa_ep_cfg.hdr.hdr_ofst_metadata = 0;
	prod_params.desc_fifo_sz = ipa_gsb_ctx->ipa_sys_desc_size;
	prod_params.priv = NULL;
	prod_params.notify = ipa_gsb_tx_dp_notify;
	res = ipa_setup_sys_pipe(&prod_params,
		&ipa_gsb_ctx->prod_hdl);
	if (res) {
		IPA_GSB_ERR("fail to setup prod sys pipe %d\n", res);
		goto fail_prod;
	}

	/* configure TX EP */
	cons_params.client = IPA_CLIENT_ODU_EMB_CONS;
	cons_params.ipa_ep_cfg.hdr.hdr_len =
		ETH_HLEN + sizeof(struct ipa_gsb_mux_hdr) +
		IPA_GSB_SKB_DUMMY_HEADER;
	cons_params.ipa_ep_cfg.hdr.hdr_ofst_pkt_size_valid = 1;
	cons_params.ipa_ep_cfg.hdr.hdr_ofst_pkt_size = 2;
	cons_params.ipa_ep_cfg.hdr_ext.hdr_pad_to_alignment = 2;
	cons_params.ipa_ep_cfg.hdr_ext.hdr_little_endian = true;
	cons_params.ipa_ep_cfg.nat.nat_en = IPA_BYPASS_NAT;
	/* setup aggregation */
	cons_params.ipa_ep_cfg.aggr.aggr_en = IPA_ENABLE_AGGR;
	cons_params.ipa_ep_cfg.aggr.aggr = IPA_GENERIC;
	cons_params.ipa_ep_cfg.aggr.aggr_time_limit =
		IPA_GSB_AGGR_TIME_LIMIT;
	cons_params.ipa_ep_cfg.aggr.aggr_byte_limit =
		IPA_GSB_AGGR_BYTE_LIMIT;
	cons_params.desc_fifo_sz = ipa_gsb_ctx->ipa_sys_desc_size;
	cons_params.priv = NULL;
	cons_params.notify = ipa_gsb_cons_cb;
	res = ipa_setup_sys_pipe(&cons_params,
		&ipa_gsb_ctx->cons_hdl);
	if (res) {
		IPA_GSB_ERR("fail to setup cons sys pipe %d\n", res);
		goto fail_cons;
	}

	IPA_GSB_DBG("prod_hdl = %d, cons_hdl = %d\n",
		ipa_gsb_ctx->prod_hdl, ipa_gsb_ctx->cons_hdl);

	return 0;

fail_cons:
	ipa_teardown_sys_pipe(ipa_gsb_ctx->prod_hdl);
	ipa_gsb_ctx->prod_hdl = 0;
fail_prod:
	return res;
}

static int ipa_bridge_connect_internal(u32 hdl)
{
	int ret;

	if (!ipa_gsb_ctx) {
		IPA_GSB_ERR("ipa_gsb_ctx was not initialized\n");
		return -EFAULT;
	}

	if (hdl >= MAX_SUPPORTED_IFACE) {
		IPA_GSB_ERR("invalid hdl: %d\n", hdl);
		return -EINVAL;
	}

	IPA_GSB_DBG("client hdl: %d\n", hdl);

	mutex_lock(&ipa_gsb_ctx->iface_lock[hdl]);
	if (!ipa_gsb_ctx->iface[hdl]) {
		IPA_GSB_ERR("fail to find interface, hdl: %d\n", hdl);
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return -EFAULT;
	}

	if (ipa_gsb_ctx->iface[hdl]->is_connected) {
		IPA_GSB_DBG("iface was already connected\n");
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return 0;
	}

	mutex_lock(&ipa_gsb_ctx->lock);
	if (ipa_gsb_ctx->num_connected_iface == 0) {
		ret = ipa_pm_activate_sync(ipa_gsb_ctx->pm_hdl);
		if (ret) {
			IPA_GSB_ERR("failed to activate ipa pm\n");
			mutex_unlock(&ipa_gsb_ctx->lock);
			mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
			return ret;
		}
		ret = ipa_gsb_connect_sys_pipe();
		if (ret) {
			IPA_GSB_ERR("fail to connect pipe\n");
			mutex_unlock(&ipa_gsb_ctx->lock);
			mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
			return ret;
		}
	}

	/* connect = connect + resume */
	ipa_gsb_ctx->iface[hdl]->is_connected = true;
	ipa_gsb_ctx->iface[hdl]->is_resumed = true;

	ipa_gsb_ctx->num_connected_iface++;
	IPA_GSB_DBG("connected iface: %d\n",
		ipa_gsb_ctx->num_connected_iface);
	ipa_gsb_ctx->num_resumed_iface++;
	IPA_GSB_DBG("num resumed iface: %d\n",
		ipa_gsb_ctx->num_resumed_iface);
	mutex_unlock(&ipa_gsb_ctx->lock);
	mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
	return 0;
}

static int ipa_gsb_disconnect_sys_pipe(void)
{
	int ret;

	IPA_GSB_DBG("prod_hdl = %d, cons_hdl = %d\n",
		ipa_gsb_ctx->prod_hdl, ipa_gsb_ctx->cons_hdl);

	ret = ipa_teardown_sys_pipe(ipa_gsb_ctx->prod_hdl);
	if (ret) {
		IPA_GSB_ERR("failed to tear down prod pipe\n");
		return -EFAULT;
	}
	ipa_gsb_ctx->prod_hdl = 0;

	ret = ipa_teardown_sys_pipe(ipa_gsb_ctx->cons_hdl);
	if (ret) {
		IPA_GSB_ERR("failed to tear down cons pipe\n");
		return -EFAULT;
	}
	ipa_gsb_ctx->cons_hdl = 0;

	return 0;
}

static int ipa_bridge_disconnect_internal(u32 hdl)
{
	int ret = 0;

	if (!ipa_gsb_ctx) {
		IPA_GSB_ERR("ipa_gsb_ctx was not initialized\n");
		return -EFAULT;
	}

	if (hdl >= MAX_SUPPORTED_IFACE) {
		IPA_GSB_ERR("invalid hdl: %d\n", hdl);
		return -EINVAL;
	}

	IPA_GSB_DBG("client hdl: %d\n", hdl);

	mutex_lock(&ipa_gsb_ctx->iface_lock[hdl]);
	atomic_set(&ipa_gsb_ctx->disconnect_in_progress, 1);

	if (!ipa_gsb_ctx->iface[hdl]) {
		IPA_GSB_ERR("fail to find interface, hdl: %d\n", hdl);
		ret = -EFAULT;
		goto fail;
	}

	if (!ipa_gsb_ctx->iface[hdl]->is_connected) {
		IPA_GSB_DBG("iface was not connected\n");
		ret = 0;
		goto fail;
	}

	mutex_lock(&ipa_gsb_ctx->lock);
	if (ipa_gsb_ctx->num_connected_iface == 1) {
		ret = ipa_gsb_disconnect_sys_pipe();
		if (ret) {
			IPA_GSB_ERR("fail to discon pipes\n");
			ret = -EFAULT;
			goto fail;
		}

		ret = ipa_pm_deactivate_sync(ipa_gsb_ctx->pm_hdl);
		if (ret) {
			IPA_GSB_ERR("failed to deactivate ipa pm\n");
			ret = -EFAULT;
			goto fail;
		}
	}

	/* disconnect = suspend + disconnect */
	ipa_gsb_ctx->iface[hdl]->is_connected = false;
	ipa_gsb_ctx->num_connected_iface--;
	IPA_GSB_DBG("connected iface: %d\n",
		ipa_gsb_ctx->num_connected_iface);

	if (ipa_gsb_ctx->iface[hdl]->is_resumed) {
		ipa_gsb_ctx->iface[hdl]->is_resumed = false;
		ipa_gsb_ctx->num_resumed_iface--;
		IPA_GSB_DBG("num resumed iface: %d\n",
			ipa_gsb_ctx->num_resumed_iface);
	}

fail:
	mutex_unlock(&ipa_gsb_ctx->lock);
	atomic_set(&ipa_gsb_ctx->disconnect_in_progress, 0);
	mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
	return ret;
}

static int ipa_bridge_resume_internal(u32 hdl)
{
	int ret;

	if (!ipa_gsb_ctx) {
		IPA_GSB_ERR("ipa_gsb_ctx was not initialized\n");
		return -EFAULT;
	}

	if (hdl >= MAX_SUPPORTED_IFACE) {
		IPA_GSB_ERR("invalid hdl: %d\n", hdl);
		return -EINVAL;
	}

	IPA_GSB_DBG_LOW("client hdl: %d\n", hdl);

	mutex_lock(&ipa_gsb_ctx->iface_lock[hdl]);
	if (!ipa_gsb_ctx->iface[hdl]) {
		IPA_GSB_ERR("fail to find interface, hdl: %d\n", hdl);
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return -EFAULT;
	}

	if (!ipa_gsb_ctx->iface[hdl]->is_connected) {
		IPA_GSB_ERR("iface is not connected\n");
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return -EFAULT;
	}

	if (ipa_gsb_ctx->iface[hdl]->is_resumed) {
		IPA_GSB_DBG_LOW("iface was already resumed\n");
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return 0;
	}

	mutex_lock(&ipa_gsb_ctx->lock);
	if (ipa_gsb_ctx->num_resumed_iface == 0) {
		ret = ipa_pm_activate_sync(ipa_gsb_ctx->pm_hdl);
		if (ret) {
			IPA_GSB_ERR("fail to activate ipa pm\n");
			mutex_unlock(&ipa_gsb_ctx->lock);
			mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
			return ret;
		}

		ret = ipa3_start_gsi_channel(
			ipa_gsb_ctx->cons_hdl);
		if (ret) {
			IPA_GSB_ERR(
				"fail to start con ep %d\n",
				ret);
			mutex_unlock(&ipa_gsb_ctx->lock);
			mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
			return ret;
		}
	}

	ipa_gsb_ctx->iface[hdl]->is_resumed = true;
	ipa_gsb_ctx->num_resumed_iface++;
	IPA_GSB_DBG_LOW("num resumed iface: %d\n",
		ipa_gsb_ctx->num_resumed_iface);

	mutex_unlock(&ipa_gsb_ctx->lock);
	mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
	return 0;
}

static int ipa_bridge_suspend_internal(u32 hdl)
{
	int ret;

	if (!ipa_gsb_ctx) {
		IPA_GSB_ERR("ipa_gsb_ctx was not initialized\n");
		return -EFAULT;
	}

	if (hdl >= MAX_SUPPORTED_IFACE) {
		IPA_GSB_ERR("invalid hdl: %d\n", hdl);
		return -EINVAL;
	}

	IPA_GSB_DBG_LOW("client hdl: %d\n", hdl);

	mutex_lock(&ipa_gsb_ctx->iface_lock[hdl]);
	atomic_set(&ipa_gsb_ctx->suspend_in_progress, 1);
	if (!ipa_gsb_ctx->iface[hdl]) {
		IPA_GSB_ERR("fail to find interface, hdl: %d\n", hdl);
		atomic_set(&ipa_gsb_ctx->suspend_in_progress, 0);
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return -EFAULT;
	}

	if (!ipa_gsb_ctx->iface[hdl]->is_connected) {
		IPA_GSB_ERR("iface is not connected\n");
		atomic_set(&ipa_gsb_ctx->suspend_in_progress, 0);
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return -EFAULT;
	}

	if (!ipa_gsb_ctx->iface[hdl]->is_resumed) {
		IPA_GSB_DBG_LOW("iface was already suspended\n");
		atomic_set(&ipa_gsb_ctx->suspend_in_progress, 0);
		mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
		return 0;
	}

	mutex_lock(&ipa_gsb_ctx->lock);
	if (ipa_gsb_ctx->num_resumed_iface == 1) {
		ret = ipa3_stop_gsi_channel(
			ipa_gsb_ctx->cons_hdl);
		if (ret) {
			IPA_GSB_ERR(
				"fail to stop cons ep %d\n",
				ret);
			atomic_set(&ipa_gsb_ctx->suspend_in_progress, 0);
			mutex_unlock(&ipa_gsb_ctx->lock);
			mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
			return ret;
		}

		ret = ipa_pm_deactivate_sync(ipa_gsb_ctx->pm_hdl);
		if (ret) {
			IPA_GSB_ERR("fail to deactivate ipa pm\n");
			ipa3_start_gsi_channel(ipa_gsb_ctx->cons_hdl);
			atomic_set(&ipa_gsb_ctx->suspend_in_progress, 0);
			mutex_unlock(&ipa_gsb_ctx->lock);
			mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
			return ret;
		}
	}

	ipa_gsb_ctx->iface[hdl]->is_resumed = false;
	ipa_gsb_ctx->num_resumed_iface--;
	IPA_GSB_DBG_LOW("num resumed iface: %d\n",
		ipa_gsb_ctx->num_resumed_iface);
	atomic_set(&ipa_gsb_ctx->suspend_in_progress, 0);
	mutex_unlock(&ipa_gsb_ctx->lock);
	mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
	return 0;
}

static int ipa_bridge_set_perf_profile_internal(u32 hdl, u32 bandwidth)
{
	int ret;

	if (!ipa_gsb_ctx) {
		IPA_GSB_ERR("ipa_gsb_ctx was not initialized\n");
		return -EFAULT;
	}

	if (hdl >= MAX_SUPPORTED_IFACE) {
		IPA_GSB_ERR("invalid hdl: %d\n", hdl);
		return -EINVAL;
	}

	IPA_GSB_DBG("client hdl: %d, BW: %d\n", hdl, bandwidth);

	mutex_lock(&ipa_gsb_ctx->iface_lock[hdl]);

	ret = ipa_pm_set_throughput(ipa_gsb_ctx->pm_hdl,
		bandwidth);
	if (ret)
		IPA_GSB_ERR("fail to set perf profile\n");

	mutex_unlock(&ipa_gsb_ctx->iface_lock[hdl]);
	return ret;
}

static int ipa_bridge_tx_dp_internal(u32 hdl, struct sk_buff *skb,
	struct ipa_tx_meta *metadata)
{
	struct ipa_gsb_mux_hdr *mux_hdr;
	struct sk_buff *skb2;
	struct stats iface_stats;
	int ret;

	IPA_GSB_DBG_LOW("client hdl: %d\n", hdl);

	iface_stats = ipa_gsb_ctx->iface[hdl]->iface_stats;
	if (!ipa_gsb_ctx->iface[hdl]) {
		IPA_GSB_ERR("fail to find interface, hdl: %d\n", hdl);
		return -EFAULT;
	}

	if (unlikely(atomic_read(&ipa_gsb_ctx->disconnect_in_progress))) {
		IPA_GSB_ERR("ipa bridge disconnect_in_progress\n");
		return -EFAULT;
	}

	if (unlikely(atomic_read(&ipa_gsb_ctx->suspend_in_progress))) {
		IPA_GSB_ERR("ipa bridge suspend_in_progress\n");
		return -EFAULT;
	}

	if (unlikely(!ipa_gsb_ctx->iface[hdl]->is_resumed)) {
		IPA_GSB_ERR_RL("iface %d was suspended\n", hdl);
		return -EFAULT;
	}

	/* make sure skb has enough headroom */
	if (unlikely(skb_headroom(skb) < sizeof(struct ipa_gsb_mux_hdr))) {
		IPA_GSB_DBG_LOW("skb doesn't have enough headroom\n");
		skb2 = skb_copy_expand(skb, sizeof(struct ipa_gsb_mux_hdr),
			0, GFP_ATOMIC);
		if (!skb2) {
			dev_kfree_skb_any(skb);
			return -ENOMEM;
		}
		dev_kfree_skb_any(skb);
		skb = skb2;
		iface_stats.num_insufficient_headroom_packets++;
	}

	/* add 4 byte header for mux */
	mux_hdr = (struct ipa_gsb_mux_hdr *)skb_push(skb,
		sizeof(struct ipa_gsb_mux_hdr));
	mux_hdr->iface_hdl = (u8)hdl;
	/* change to network order */
	*(u32 *)mux_hdr = htonl(*(u32 *)mux_hdr);

	ret = ipa_tx_dp(IPA_CLIENT_ODU_PROD, skb, metadata);
	if (ret) {
		IPA_GSB_ERR("tx dp failed %d\n", ret);
		return -EFAULT;
	}
	ipa_gsb_ctx->iface[hdl]->iface_stats.num_ul_packets++;

	return 0;
}

void ipa_gsb_register(void)
{
	struct ipa_gsb_data funcs;

	funcs.ipa_bridge_init = ipa_bridge_init_internal;
	funcs.ipa_bridge_connect = ipa_bridge_connect_internal;
	funcs.ipa_bridge_set_perf_profile = ipa_bridge_set_perf_profile_internal;
	funcs.ipa_bridge_disconnect = ipa_bridge_disconnect_internal;
	funcs.ipa_bridge_suspend = ipa_bridge_suspend_internal;
	funcs.ipa_bridge_resume = ipa_bridge_resume_internal;
	funcs.ipa_bridge_tx_dp = ipa_bridge_tx_dp_internal;
	funcs.ipa_bridge_cleanup = ipa_bridge_cleanup_internal;

	if (ipa_fmwk_register_gsb(&funcs))
		pr_err("failed to register ipa_gsb APIs\n");
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ipa gsb driver");
