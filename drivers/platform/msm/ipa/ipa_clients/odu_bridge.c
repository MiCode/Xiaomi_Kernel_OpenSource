/* Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
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
#include "../ipa_common_i.h"
#include "../ipa_v3/ipa_pm.h"

#define ODU_BRIDGE_DRV_NAME "odu_ipa_bridge"

#define ODU_BRIDGE_DBG(fmt, args...) \
	do { \
		pr_debug(ODU_BRIDGE_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			ODU_BRIDGE_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			ODU_BRIDGE_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)
#define ODU_BRIDGE_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(ODU_BRIDGE_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			ODU_BRIDGE_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)
#define ODU_BRIDGE_ERR(fmt, args...) \
	do { \
		pr_err(ODU_BRIDGE_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			ODU_BRIDGE_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			ODU_BRIDGE_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define ODU_BRIDGE_FUNC_ENTRY() \
	ODU_BRIDGE_DBG_LOW("ENTRY\n")
#define ODU_BRIDGE_FUNC_EXIT() \
	ODU_BRIDGE_DBG_LOW("EXIT\n")


#define ODU_BRIDGE_IS_QMI_ADDR(daddr) \
	(memcmp(&(daddr), &odu_bridge_ctx->llv6_addr, sizeof((daddr))) \
		== 0)

#define ODU_BRIDGE_IPV4_HDR_NAME "odu_br_ipv4"
#define ODU_BRIDGE_IPV6_HDR_NAME "odu_br_ipv6"

#define IPA_ODU_SYS_DESC_FIFO_SZ 0x800

#ifdef CONFIG_COMPAT
#define ODU_BRIDGE_IOC_SET_LLV6_ADDR32 _IOW(ODU_BRIDGE_IOC_MAGIC, \
				ODU_BRIDGE_IOCTL_SET_LLV6_ADDR, \
				compat_uptr_t)
#endif

#define IPA_ODU_VER_CHECK() \
	do { \
		ret = 0;\
		if (ipa_get_hw_type() == IPA_HW_None) { \
			pr_err("IPA HW is unknown\n"); \
			ret = -EFAULT; \
		} \
		else if (ipa_get_hw_type() < IPA_HW_v3_0) \
			ret = 1; \
	} while (0)

/**
 * struct stats - driver statistics, viewable using debugfs
 * @num_ul_packets: number of packets bridged in uplink direction
 * @num_dl_packets: number of packets bridged in downink direction
 * bridge
 * @num_lan_packets: number of packets bridged to APPS on bridge mode
 */
struct stats {
	u64 num_ul_packets;
	u64 num_dl_packets;
	u64 num_lan_packets;
};

/**
 * struct odu_bridge_ctx - ODU bridge driver context information
 * @class: kernel class pointer
 * @dev_num: kernel device number
 * @dev: kernel device struct pointer
 * @cdev: kernel character device struct
 * @netdev_name: network interface name
 * @device_ethaddr: network interface ethernet address
 * @priv: client's private data. to be used in client's callbacks
 * @tx_dp_notify: client callback for handling IPA ODU_PROD callback
 * @send_dl_skb: client callback for sending skb in downlink direction
 * @stats: statistics, how many packets were transmitted using the SW bridge
 * @is_conencted: is bridge connected ?
 * @is_suspended: is bridge suspended ?
 * @mode: ODU mode (router/bridge)
 * @lock: for the initialization, connect and disconnect synchronization
 * @llv6_addr: link local IPv6 address of ODU network interface
 * @odu_br_ipv4_hdr_hdl: handle for partial ipv4 ethernet header
 * @odu_br_ipv6_hdr_hdl: handle for partial ipv6 ethernet header
 * @odu_prod_hdl: handle for IPA_CLIENT_ODU_PROD pipe
 * @odu_emb_cons_hdl: handle for IPA_CLIENT_ODU_EMB_CONS pipe
 * @odu_teth_cons_hdl: handle for IPA_CLIENT_ODU_TETH_CONS pipe
 * @rm_comp: completion object for IP RM
 * @wakeup_request: client callback to wakeup
 */
struct odu_bridge_ctx {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	char netdev_name[IPA_RESOURCE_NAME_MAX];
	u8 device_ethaddr[ETH_ALEN];
	void *priv;
	ipa_notify_cb tx_dp_notify;
	int (*send_dl_skb)(void *priv, struct sk_buff *skb);
	struct stats stats;
	bool is_connected;
	bool is_suspended;
	enum odu_bridge_mode mode;
	struct mutex lock;
	struct in6_addr llv6_addr;
	uint32_t odu_br_ipv4_hdr_hdl;
	uint32_t odu_br_ipv6_hdr_hdl;
	u32 odu_prod_hdl;
	u32 odu_emb_cons_hdl;
	u32 odu_teth_cons_hdl;
	u32 ipa_sys_desc_size;
	void *logbuf;
	void *logbuf_low;
	struct completion rm_comp;
	void (*wakeup_request)(void *);
	u32 pm_hdl;
};
static struct odu_bridge_ctx *odu_bridge_ctx;

#ifdef CONFIG_DEBUG_FS
#define ODU_MAX_MSG_LEN 512
static char dbg_buff[ODU_MAX_MSG_LEN];
#endif

static void odu_bridge_emb_cons_cb(void *priv, enum ipa_dp_evt_type evt,
	unsigned long data)
{
	ODU_BRIDGE_FUNC_ENTRY();
	if (evt != IPA_RECEIVE) {
		ODU_BRIDGE_ERR("unexpected event\n");
		WARN_ON(1);
		return;
	}
	odu_bridge_ctx->send_dl_skb(priv, (struct sk_buff *)data);
	odu_bridge_ctx->stats.num_dl_packets++;
	ODU_BRIDGE_FUNC_EXIT();
}

static void odu_bridge_teth_cons_cb(void *priv, enum ipa_dp_evt_type evt,
	unsigned long data)
{
	struct ipv6hdr *ipv6hdr;
	struct sk_buff *skb = (struct sk_buff *)data;
	struct sk_buff *skb_copied;

	ODU_BRIDGE_FUNC_ENTRY();
	if (evt != IPA_RECEIVE) {
		ODU_BRIDGE_ERR("unexpected event\n");
		WARN_ON(1);
		return;
	}

	ipv6hdr = (struct ipv6hdr *)(skb->data + ETH_HLEN);
	if (ipv6hdr->version == 6 &&
	    ipv6_addr_is_multicast(&ipv6hdr->daddr)) {
		ODU_BRIDGE_DBG_LOW("Multicast pkt, send to APPS and adapter\n");
		skb_copied = skb_clone(skb, GFP_KERNEL);
		if (skb_copied) {
			odu_bridge_ctx->tx_dp_notify(odu_bridge_ctx->priv,
						IPA_RECEIVE,
						(unsigned long) skb_copied);
			odu_bridge_ctx->stats.num_lan_packets++;
		} else {
			ODU_BRIDGE_ERR("No memory\n");
		}
	}

	odu_bridge_ctx->send_dl_skb(priv, skb);
	odu_bridge_ctx->stats.num_dl_packets++;
	ODU_BRIDGE_FUNC_EXIT();
}

static int odu_bridge_connect_router(void)
{
	struct ipa_sys_connect_params odu_prod_params;
	struct ipa_sys_connect_params odu_emb_cons_params;
	int res;

	ODU_BRIDGE_FUNC_ENTRY();

	memset(&odu_prod_params, 0, sizeof(odu_prod_params));
	memset(&odu_emb_cons_params, 0, sizeof(odu_emb_cons_params));

	/* configure RX (ODU->IPA) EP */
	odu_prod_params.client = IPA_CLIENT_ODU_PROD;
	odu_prod_params.ipa_ep_cfg.hdr.hdr_len = ETH_HLEN;
	odu_prod_params.ipa_ep_cfg.nat.nat_en = IPA_SRC_NAT;
	odu_prod_params.desc_fifo_sz = odu_bridge_ctx->ipa_sys_desc_size;
	odu_prod_params.priv = odu_bridge_ctx->priv;
	odu_prod_params.notify = odu_bridge_ctx->tx_dp_notify;
	res = ipa_setup_sys_pipe(&odu_prod_params,
		&odu_bridge_ctx->odu_prod_hdl);
	if (res) {
		ODU_BRIDGE_ERR("fail to setup sys pipe ODU_PROD %d\n", res);
		goto fail_odu_prod;
	}

	/* configure TX (IPA->ODU) EP */
	odu_emb_cons_params.client = IPA_CLIENT_ODU_EMB_CONS;
	odu_emb_cons_params.ipa_ep_cfg.hdr.hdr_len = ETH_HLEN;
	odu_emb_cons_params.ipa_ep_cfg.nat.nat_en = IPA_BYPASS_NAT;
	odu_emb_cons_params.desc_fifo_sz = odu_bridge_ctx->ipa_sys_desc_size;
	odu_emb_cons_params.priv = odu_bridge_ctx->priv;
	odu_emb_cons_params.notify = odu_bridge_emb_cons_cb;
	res = ipa_setup_sys_pipe(&odu_emb_cons_params,
		&odu_bridge_ctx->odu_emb_cons_hdl);
	if (res) {
		ODU_BRIDGE_ERR("fail to setup sys pipe ODU_EMB_CONS %d\n", res);
		goto fail_odu_emb_cons;
	}

	ODU_BRIDGE_DBG("odu_prod_hdl = %d, odu_emb_cons_hdl = %d\n",
		odu_bridge_ctx->odu_prod_hdl, odu_bridge_ctx->odu_emb_cons_hdl);

	ODU_BRIDGE_FUNC_EXIT();

	return 0;

fail_odu_emb_cons:
	ipa_teardown_sys_pipe(odu_bridge_ctx->odu_prod_hdl);
	odu_bridge_ctx->odu_prod_hdl = 0;
fail_odu_prod:
	return res;
}

static int odu_bridge_connect_bridge(void)
{
	struct ipa_sys_connect_params odu_prod_params;
	struct ipa_sys_connect_params odu_emb_cons_params;
	struct ipa_sys_connect_params odu_teth_cons_params;
	int res;

	ODU_BRIDGE_FUNC_ENTRY();

	memset(&odu_prod_params, 0, sizeof(odu_prod_params));
	memset(&odu_emb_cons_params, 0, sizeof(odu_emb_cons_params));

	if (!ipa_pm_is_used()) {
		/* Build IPA Resource manager dependency graph */
		ODU_BRIDGE_DBG_LOW("build dependency graph\n");
		res = ipa_rm_add_dependency(IPA_RM_RESOURCE_ODU_ADAPT_PROD,
					IPA_RM_RESOURCE_Q6_CONS);
		if (res && res != -EINPROGRESS) {
			ODU_BRIDGE_ERR("ipa_rm_add_dependency() failed\n");
			goto fail_add_dependency_1;
		}

		res = ipa_rm_add_dependency(IPA_RM_RESOURCE_Q6_PROD,
					IPA_RM_RESOURCE_ODU_ADAPT_CONS);
		if (res && res != -EINPROGRESS) {
			ODU_BRIDGE_ERR("ipa_rm_add_dependency() failed\n");
			goto fail_add_dependency_2;
		}
	}

	/* configure RX (ODU->IPA) EP */
	odu_prod_params.client = IPA_CLIENT_ODU_PROD;
	odu_prod_params.desc_fifo_sz = IPA_ODU_SYS_DESC_FIFO_SZ;
	odu_prod_params.priv = odu_bridge_ctx->priv;
	odu_prod_params.notify = odu_bridge_ctx->tx_dp_notify;
	odu_prod_params.skip_ep_cfg = true;
	res = ipa_setup_sys_pipe(&odu_prod_params,
		&odu_bridge_ctx->odu_prod_hdl);
	if (res) {
		ODU_BRIDGE_ERR("fail to setup sys pipe ODU_PROD %d\n", res);
		goto fail_odu_prod;
	}

	/* configure TX tethered (IPA->ODU) EP */
	odu_teth_cons_params.client = IPA_CLIENT_ODU_TETH_CONS;
	odu_teth_cons_params.desc_fifo_sz = IPA_ODU_SYS_DESC_FIFO_SZ;
	odu_teth_cons_params.priv = odu_bridge_ctx->priv;
	odu_teth_cons_params.notify = odu_bridge_teth_cons_cb;
	odu_teth_cons_params.skip_ep_cfg = true;
	res = ipa_setup_sys_pipe(&odu_teth_cons_params,
		&odu_bridge_ctx->odu_teth_cons_hdl);
	if (res) {
		ODU_BRIDGE_ERR("fail to setup sys pipe ODU_TETH_CONS %d\n",
				res);
		goto fail_odu_teth_cons;
	}

	/* configure TX embedded(IPA->ODU) EP */
	odu_emb_cons_params.client = IPA_CLIENT_ODU_EMB_CONS;
	odu_emb_cons_params.ipa_ep_cfg.hdr.hdr_len = ETH_HLEN;
	odu_emb_cons_params.ipa_ep_cfg.nat.nat_en = IPA_BYPASS_NAT;
	odu_emb_cons_params.desc_fifo_sz = IPA_ODU_SYS_DESC_FIFO_SZ;
	odu_emb_cons_params.priv = odu_bridge_ctx->priv;
	odu_emb_cons_params.notify = odu_bridge_emb_cons_cb;
	res = ipa_setup_sys_pipe(&odu_emb_cons_params,
		&odu_bridge_ctx->odu_emb_cons_hdl);
	if (res) {
		ODU_BRIDGE_ERR("fail to setup sys pipe ODU_EMB_CONS %d\n", res);
		goto fail_odu_emb_cons;
	}

	ODU_BRIDGE_DBG_LOW("odu_prod_hdl = %d, odu_emb_cons_hdl = %d\n",
		odu_bridge_ctx->odu_prod_hdl, odu_bridge_ctx->odu_emb_cons_hdl);
	ODU_BRIDGE_DBG_LOW("odu_teth_cons_hdl = %d\n",
		odu_bridge_ctx->odu_teth_cons_hdl);

	ODU_BRIDGE_FUNC_EXIT();

	return 0;

fail_odu_emb_cons:
	ipa_teardown_sys_pipe(odu_bridge_ctx->odu_teth_cons_hdl);
	odu_bridge_ctx->odu_teth_cons_hdl = 0;
fail_odu_teth_cons:
	ipa_teardown_sys_pipe(odu_bridge_ctx->odu_prod_hdl);
	odu_bridge_ctx->odu_prod_hdl = 0;
fail_odu_prod:
	if (!ipa_pm_is_used())
		ipa_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
				IPA_RM_RESOURCE_ODU_ADAPT_CONS);
fail_add_dependency_2:
	if (!ipa_pm_is_used())
		ipa_rm_delete_dependency(IPA_RM_RESOURCE_ODU_ADAPT_PROD,
				IPA_RM_RESOURCE_Q6_CONS);
fail_add_dependency_1:
	return res;
}

static int odu_bridge_disconnect_router(void)
{
	int res;

	ODU_BRIDGE_FUNC_ENTRY();

	res = ipa_teardown_sys_pipe(odu_bridge_ctx->odu_prod_hdl);
	if (res)
		ODU_BRIDGE_ERR("teardown ODU PROD failed\n");
	odu_bridge_ctx->odu_prod_hdl = 0;

	res = ipa_teardown_sys_pipe(odu_bridge_ctx->odu_emb_cons_hdl);
	if (res)
		ODU_BRIDGE_ERR("teardown ODU EMB CONS failed\n");
	odu_bridge_ctx->odu_emb_cons_hdl = 0;

	ODU_BRIDGE_FUNC_EXIT();

	return 0;
}

static int odu_bridge_disconnect_bridge(void)
{
	int res;

	ODU_BRIDGE_FUNC_ENTRY();

	res = ipa_teardown_sys_pipe(odu_bridge_ctx->odu_prod_hdl);
	if (res)
		ODU_BRIDGE_ERR("teardown ODU PROD failed\n");
	odu_bridge_ctx->odu_prod_hdl = 0;

	res = ipa_teardown_sys_pipe(odu_bridge_ctx->odu_teth_cons_hdl);
	if (res)
		ODU_BRIDGE_ERR("teardown ODU TETH CONS failed\n");
	odu_bridge_ctx->odu_teth_cons_hdl = 0;

	res = ipa_teardown_sys_pipe(odu_bridge_ctx->odu_emb_cons_hdl);
	if (res)
		ODU_BRIDGE_ERR("teardown ODU EMB CONS failed\n");
	odu_bridge_ctx->odu_emb_cons_hdl = 0;

	if (!ipa_pm_is_used()) {
		/* Delete IPA Resource manager dependency graph */
		ODU_BRIDGE_DBG("deleting dependency graph\n");
		res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_ODU_ADAPT_PROD,
			IPA_RM_RESOURCE_Q6_CONS);
		if (res && res != -EINPROGRESS)
			ODU_BRIDGE_ERR("ipa_rm_delete_dependency() failed\n");

		res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
			IPA_RM_RESOURCE_ODU_ADAPT_CONS);
		if (res && res != -EINPROGRESS)
			ODU_BRIDGE_ERR("ipa_rm_delete_dependency() failed\n");
	}

	return 0;
}

/**
 * odu_bridge_disconnect() - Disconnect odu bridge
 *
 * Disconnect all pipes and deletes IPA RM dependencies on bridge mode
 *
 * Return codes: 0- success, error otherwise
 */
int odu_bridge_disconnect(void)
{
	int res;

	ODU_BRIDGE_FUNC_ENTRY();

	if (!odu_bridge_ctx) {
		ODU_BRIDGE_ERR("Not initialized\n");
		return -EFAULT;
	}

	if (!odu_bridge_ctx->is_connected) {
		ODU_BRIDGE_ERR("Not connected\n");
		return -EFAULT;
	}

	mutex_lock(&odu_bridge_ctx->lock);
	if (odu_bridge_ctx->mode == ODU_BRIDGE_MODE_ROUTER) {
		res = odu_bridge_disconnect_router();
		if (res) {
			ODU_BRIDGE_ERR("disconnect_router failed %d\n", res);
			goto out;
		}
	} else {
		res = odu_bridge_disconnect_bridge();
		if (res) {
			ODU_BRIDGE_ERR("disconnect_bridge failed %d\n", res);
			goto out;
		}
	}

	odu_bridge_ctx->is_connected = false;
	res = 0;
out:
	mutex_unlock(&odu_bridge_ctx->lock);
	ODU_BRIDGE_FUNC_EXIT();
	return res;
}
EXPORT_SYMBOL(odu_bridge_disconnect);

/**
 * odu_bridge_connect() - Connect odu bridge.
 *
 * Call to the mode-specific connect function for connection IPA pipes
 * and adding IPA RM dependencies

 * Return codes: 0: success
 *		-EINVAL: invalid parameters
 *		-EPERM: Operation not permitted as the bridge is already
 *		connected
 */
int odu_bridge_connect(void)
{
	int res;

	ODU_BRIDGE_FUNC_ENTRY();

	if (!odu_bridge_ctx) {
		ODU_BRIDGE_ERR("Not initialized\n");
		return -EFAULT;
	}

	if (odu_bridge_ctx->is_connected) {
		ODU_BRIDGE_ERR("already connected\n");
		return -EFAULT;
	}

	mutex_lock(&odu_bridge_ctx->lock);
	if (odu_bridge_ctx->mode == ODU_BRIDGE_MODE_ROUTER) {
		res = odu_bridge_connect_router();
		if (res) {
			ODU_BRIDGE_ERR("connect_router failed\n");
			goto bail;
		}
	} else {
		res = odu_bridge_connect_bridge();
		if (res) {
			ODU_BRIDGE_ERR("connect_bridge failed\n");
			goto bail;
		}
	}

	odu_bridge_ctx->is_connected = true;
	res = 0;
bail:
	mutex_unlock(&odu_bridge_ctx->lock);
	ODU_BRIDGE_FUNC_EXIT();
	return res;
}
EXPORT_SYMBOL(odu_bridge_connect);

/**
 * odu_bridge_set_mode() - Set bridge mode to Router/Bridge
 * @mode: mode to be set
 */
static int odu_bridge_set_mode(enum odu_bridge_mode mode)
{
	int res;

	ODU_BRIDGE_FUNC_ENTRY();

	if (mode < 0 || mode >= ODU_BRIDGE_MODE_MAX) {
		ODU_BRIDGE_ERR("Unsupported mode: %d\n", mode);
		return -EFAULT;
	}

	ODU_BRIDGE_DBG_LOW("setting mode: %d\n", mode);
	mutex_lock(&odu_bridge_ctx->lock);

	if (odu_bridge_ctx->mode == mode) {
		ODU_BRIDGE_DBG_LOW("same mode\n");
		res = 0;
		goto bail;
	}

	if (odu_bridge_ctx->is_connected) {
		/* first disconnect the old configuration */
		if (odu_bridge_ctx->mode == ODU_BRIDGE_MODE_ROUTER) {
			res = odu_bridge_disconnect_router();
			if (res) {
				ODU_BRIDGE_ERR("disconnect_router failed\n");
				goto bail;
			}
		} else {
			res = odu_bridge_disconnect_bridge();
			if (res) {
				ODU_BRIDGE_ERR("disconnect_bridge failed\n");
				goto bail;
			}
		}

		/* connect the new configuration */
		if (mode == ODU_BRIDGE_MODE_ROUTER) {
			res = odu_bridge_connect_router();
			if (res) {
				ODU_BRIDGE_ERR("connect_router failed\n");
				goto bail;
			}
		} else {
			res = odu_bridge_connect_bridge();
			if (res) {
				ODU_BRIDGE_ERR("connect_bridge failed\n");
				goto bail;
			}
		}
	}
	odu_bridge_ctx->mode = mode;
	res = 0;
bail:
	mutex_unlock(&odu_bridge_ctx->lock);
	ODU_BRIDGE_FUNC_EXIT();
	return res;
};

/**
 * odu_bridge_set_llv6_addr() - Set link local ipv6 address
 * @llv6_addr: odu network interface link local address
 *
 * This function sets the link local ipv6 address provided by IOCTL
 */
static int odu_bridge_set_llv6_addr(struct in6_addr *llv6_addr)
{
	struct in6_addr llv6_addr_host;

	ODU_BRIDGE_FUNC_ENTRY();

	llv6_addr_host.s6_addr32[0] = ntohl(llv6_addr->s6_addr32[0]);
	llv6_addr_host.s6_addr32[1] = ntohl(llv6_addr->s6_addr32[1]);
	llv6_addr_host.s6_addr32[2] = ntohl(llv6_addr->s6_addr32[2]);
	llv6_addr_host.s6_addr32[3] = ntohl(llv6_addr->s6_addr32[3]);

	memcpy(&odu_bridge_ctx->llv6_addr, &llv6_addr_host,
				sizeof(odu_bridge_ctx->llv6_addr));
	ODU_BRIDGE_DBG_LOW("LLV6 addr: %pI6c\n", &odu_bridge_ctx->llv6_addr);

	ODU_BRIDGE_FUNC_EXIT();

	return 0;
};

static long odu_bridge_ioctl(struct file *filp,
			      unsigned int cmd,
			      unsigned long arg)
{
	int res = 0;
	struct in6_addr llv6_addr;

	ODU_BRIDGE_DBG("cmd=%x nr=%d\n", cmd, _IOC_NR(cmd));

	if ((_IOC_TYPE(cmd) != ODU_BRIDGE_IOC_MAGIC) ||
	    (_IOC_NR(cmd) >= ODU_BRIDGE_IOCTL_MAX)) {
		ODU_BRIDGE_ERR("Invalid ioctl\n");
		return -ENOIOCTLCMD;
	}

	switch (cmd) {
	case ODU_BRIDGE_IOC_SET_MODE:
		ODU_BRIDGE_DBG("ODU_BRIDGE_IOC_SET_MODE ioctl called\n");
		res = odu_bridge_set_mode(arg);
		if (res) {
			ODU_BRIDGE_ERR("Error, res = %d\n", res);
			break;
		}
		break;

	case ODU_BRIDGE_IOC_SET_LLV6_ADDR:
		ODU_BRIDGE_DBG("ODU_BRIDGE_IOC_SET_LLV6_ADDR ioctl called\n");
		res = copy_from_user(&llv6_addr,
			(struct in6_addr *)arg,
			sizeof(llv6_addr));
		if (res) {
			ODU_BRIDGE_ERR("Error, res = %d\n", res);
			res = -EFAULT;
			break;
		}

		res = odu_bridge_set_llv6_addr(&llv6_addr);
		if (res) {
			ODU_BRIDGE_ERR("Error, res = %d\n", res);
			break;
		}
		break;

	default:
		ODU_BRIDGE_ERR("Unknown ioctl: %d\n", cmd);
		WARN_ON(1);
	}

	return res;
}

#ifdef CONFIG_COMPAT
static long compat_odu_bridge_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case ODU_BRIDGE_IOC_SET_LLV6_ADDR32:
		cmd = ODU_BRIDGE_IOC_SET_LLV6_ADDR;
		break;
	case ODU_BRIDGE_IOC_SET_MODE:
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return odu_bridge_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

#ifdef CONFIG_DEBUG_FS
static struct dentry *dent;
static struct dentry *dfile_stats;
static struct dentry *dfile_mode;

static ssize_t odu_debugfs_stats(struct file *file,
				  char __user *ubuf,
				  size_t count,
				  loff_t *ppos)
{
	int nbytes = 0;

	nbytes += scnprintf(&dbg_buff[nbytes],
			    ODU_MAX_MSG_LEN - nbytes,
			   "UL packets: %lld\n",
			    odu_bridge_ctx->stats.num_ul_packets);
	nbytes += scnprintf(&dbg_buff[nbytes],
			    ODU_MAX_MSG_LEN - nbytes,
			   "DL packets: %lld\n",
			    odu_bridge_ctx->stats.num_dl_packets);
	nbytes += scnprintf(&dbg_buff[nbytes],
			    ODU_MAX_MSG_LEN - nbytes,
			    "LAN packets: %lld\n",
			    odu_bridge_ctx->stats.num_lan_packets);
	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t odu_debugfs_hw_bridge_mode_write(struct file *file,
					const char __user *ubuf,
					size_t count,
					loff_t *ppos)
{
	unsigned long missing;
	enum odu_bridge_mode mode;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, ubuf, min(sizeof(dbg_buff), count));
	if (missing)
		return -EFAULT;

	if (count > 0)
		dbg_buff[count-1] = '\0';

	if (strcmp(dbg_buff, "router") == 0) {
		mode = ODU_BRIDGE_MODE_ROUTER;
	} else if (strcmp(dbg_buff, "bridge") == 0) {
		mode = ODU_BRIDGE_MODE_BRIDGE;
	} else {
		ODU_BRIDGE_ERR("Bad mode, got %s,\n"
			 "Use <router> or <bridge>.\n", dbg_buff);
		return count;
	}

	odu_bridge_set_mode(mode);
	return count;
}

static ssize_t odu_debugfs_hw_bridge_mode_read(struct file *file,
					     char __user *ubuf,
					     size_t count,
					     loff_t *ppos)
{
	int nbytes = 0;

	switch (odu_bridge_ctx->mode) {
	case ODU_BRIDGE_MODE_ROUTER:
		nbytes += scnprintf(&dbg_buff[nbytes],
			ODU_MAX_MSG_LEN - nbytes,
			"router\n");
		break;
	case ODU_BRIDGE_MODE_BRIDGE:
		nbytes += scnprintf(&dbg_buff[nbytes],
			ODU_MAX_MSG_LEN - nbytes,
			"bridge\n");
		break;
	default:
		nbytes += scnprintf(&dbg_buff[nbytes],
			ODU_MAX_MSG_LEN - nbytes,
			"mode error\n");
		break;

	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

const struct file_operations odu_stats_ops = {
	.read = odu_debugfs_stats,
};

const struct file_operations odu_hw_bridge_mode_ops = {
	.read = odu_debugfs_hw_bridge_mode_read,
	.write = odu_debugfs_hw_bridge_mode_write,
};

static void odu_debugfs_init(void)
{
	const mode_t read_only_mode = 0444;
	const mode_t read_write_mode = 0666;

	dent = debugfs_create_dir("odu_ipa_bridge", 0);
	if (IS_ERR(dent)) {
		ODU_BRIDGE_ERR("fail to create folder odu_ipa_bridge\n");
		return;
	}

	dfile_stats =
		debugfs_create_file("stats", read_only_mode, dent,
				    0, &odu_stats_ops);
	if (!dfile_stats || IS_ERR(dfile_stats)) {
		ODU_BRIDGE_ERR("fail to create file stats\n");
		goto fail;
	}

	dfile_mode =
		debugfs_create_file("mode", read_write_mode,
				    dent, 0, &odu_hw_bridge_mode_ops);
	if (!dfile_mode ||
	    IS_ERR(dfile_mode)) {
		ODU_BRIDGE_ERR("fail to create file dfile_mode\n");
		goto fail;
	}

	return;
fail:
	debugfs_remove_recursive(dent);
}

static void odu_debugfs_destroy(void)
{
	debugfs_remove_recursive(dent);
}

#else
static void odu_debugfs_init(void) {}
static void odu_debugfs_destroy(void) {}
#endif /* CONFIG_DEBUG_FS */


static const struct file_operations odu_bridge_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = odu_bridge_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_odu_bridge_ioctl,
#endif
};

/**
 * odu_bridge_tx_dp() - Send skb to ODU bridge
 * @skb: skb to send
 * @metadata: metadata on packet
 *
 * This function handles uplink packet.
 * In Router Mode:
 *	packet is sent directly to IPA.
 * In Router Mode:
 *	packet is classified if it should arrive to network stack.
 *	QMI IP packet should arrive to APPS network stack
 *	IPv6 Multicast packet should arrive to APPS network stack and Q6
 *
 * Return codes: 0- success, error otherwise
 */
int odu_bridge_tx_dp(struct sk_buff *skb, struct ipa_tx_meta *metadata)
{
	struct sk_buff *skb_copied = NULL;
	struct ipv6hdr *ipv6hdr;
	int res;

	ODU_BRIDGE_FUNC_ENTRY();

	switch (odu_bridge_ctx->mode) {
	case ODU_BRIDGE_MODE_ROUTER:
		/* Router mode - pass skb to IPA */
		res = ipa_tx_dp(IPA_CLIENT_ODU_PROD, skb, metadata);
		if (res) {
			ODU_BRIDGE_DBG("tx dp failed %d\n", res);
			goto out;
		}
		odu_bridge_ctx->stats.num_ul_packets++;
		goto out;

	case ODU_BRIDGE_MODE_BRIDGE:
		ipv6hdr = (struct ipv6hdr *)(skb->data + ETH_HLEN);
		if (ipv6hdr->version == 6 &&
		    ODU_BRIDGE_IS_QMI_ADDR(ipv6hdr->daddr)) {
			ODU_BRIDGE_DBG_LOW("QMI packet\n");
			skb_copied = skb_clone(skb, GFP_KERNEL);
			if (!skb_copied) {
				ODU_BRIDGE_ERR("No memory\n");
				return -ENOMEM;
			}
			odu_bridge_ctx->tx_dp_notify(odu_bridge_ctx->priv,
						     IPA_RECEIVE,
						     (unsigned long)skb_copied);
			odu_bridge_ctx->tx_dp_notify(odu_bridge_ctx->priv,
						     IPA_WRITE_DONE,
						     (unsigned long)skb);
			odu_bridge_ctx->stats.num_ul_packets++;
			odu_bridge_ctx->stats.num_lan_packets++;
			res = 0;
			goto out;
		}

		if (ipv6hdr->version == 6 &&
		    ipv6_addr_is_multicast(&ipv6hdr->daddr)) {
			ODU_BRIDGE_DBG_LOW(
				"Multicast pkt, send to APPS and IPA\n");
			skb_copied = skb_clone(skb, GFP_KERNEL);
			if (!skb_copied) {
				ODU_BRIDGE_ERR("No memory\n");
				return -ENOMEM;
			}

			res = ipa_tx_dp(IPA_CLIENT_ODU_PROD, skb, metadata);
			if (res) {
				ODU_BRIDGE_DBG("tx dp failed %d\n", res);
				dev_kfree_skb(skb_copied);
				goto out;
			}

			odu_bridge_ctx->tx_dp_notify(odu_bridge_ctx->priv,
						     IPA_RECEIVE,
						     (unsigned long)skb_copied);
			odu_bridge_ctx->stats.num_ul_packets++;
			odu_bridge_ctx->stats.num_lan_packets++;
			goto out;
		}

		res = ipa_tx_dp(IPA_CLIENT_ODU_PROD, skb, metadata);
		if (res) {
			ODU_BRIDGE_DBG("tx dp failed %d\n", res);
			goto out;
		}
		odu_bridge_ctx->stats.num_ul_packets++;
		goto out;

	default:
		ODU_BRIDGE_ERR("Unsupported mode: %d\n", odu_bridge_ctx->mode);
		WARN_ON(1);
		res = -EFAULT;

	}
out:
	ODU_BRIDGE_FUNC_EXIT();
	return res;
}
EXPORT_SYMBOL(odu_bridge_tx_dp);

static int odu_bridge_add_hdrs(void)
{
	struct ipa_ioc_add_hdr *hdrs;
	struct ipa_hdr_add *ipv4_hdr;
	struct ipa_hdr_add *ipv6_hdr;
	struct ethhdr *eth_ipv4;
	struct ethhdr *eth_ipv6;
	int res;

	ODU_BRIDGE_FUNC_ENTRY();
	hdrs = kzalloc(sizeof(*hdrs) + sizeof(*ipv4_hdr) + sizeof(*ipv6_hdr),
			GFP_KERNEL);
	if (!hdrs) {
		ODU_BRIDGE_ERR("no mem\n");
		res = -ENOMEM;
		goto out;
	}
	ipv4_hdr = &hdrs->hdr[0];
	eth_ipv4 = (struct ethhdr *)(ipv4_hdr->hdr);
	ipv6_hdr = &hdrs->hdr[1];
	eth_ipv6 = (struct ethhdr *)(ipv6_hdr->hdr);
	strlcpy(ipv4_hdr->name, ODU_BRIDGE_IPV4_HDR_NAME,
		IPA_RESOURCE_NAME_MAX);
	memcpy(eth_ipv4->h_source, odu_bridge_ctx->device_ethaddr, ETH_ALEN);
	eth_ipv4->h_proto = htons(ETH_P_IP);
	ipv4_hdr->hdr_len = ETH_HLEN;
	ipv4_hdr->is_partial = 1;
	ipv4_hdr->is_eth2_ofst_valid = 1;
	ipv4_hdr->eth2_ofst = 0;
	strlcpy(ipv6_hdr->name, ODU_BRIDGE_IPV6_HDR_NAME,
		IPA_RESOURCE_NAME_MAX);
	memcpy(eth_ipv6->h_source, odu_bridge_ctx->device_ethaddr, ETH_ALEN);
	eth_ipv6->h_proto = htons(ETH_P_IPV6);
	ipv6_hdr->hdr_len = ETH_HLEN;
	ipv6_hdr->is_partial = 1;
	ipv6_hdr->is_eth2_ofst_valid = 1;
	ipv6_hdr->eth2_ofst = 0;
	hdrs->commit = 1;
	hdrs->num_hdrs = 2;
	res = ipa_add_hdr(hdrs);
	if (res) {
		ODU_BRIDGE_ERR("Fail on Header-Insertion(%d)\n", res);
		goto out_free_mem;
	}
	if (ipv4_hdr->status) {
		ODU_BRIDGE_ERR("Fail on Header-Insertion ipv4(%d)\n",
				ipv4_hdr->status);
		res = ipv4_hdr->status;
		goto out_free_mem;
	}
	if (ipv6_hdr->status) {
		ODU_BRIDGE_ERR("Fail on Header-Insertion ipv6(%d)\n",
				ipv6_hdr->status);
		res = ipv6_hdr->status;
		goto out_free_mem;
	}
	odu_bridge_ctx->odu_br_ipv4_hdr_hdl = ipv4_hdr->hdr_hdl;
	odu_bridge_ctx->odu_br_ipv6_hdr_hdl = ipv6_hdr->hdr_hdl;

	res = 0;
out_free_mem:
	kfree(hdrs);
out:
	ODU_BRIDGE_FUNC_EXIT();
	return res;
}

static void odu_bridge_del_hdrs(void)
{
	struct ipa_ioc_del_hdr *del_hdr;
	struct ipa_hdr_del *ipv4;
	struct ipa_hdr_del *ipv6;
	int result;

	del_hdr = kzalloc(sizeof(*del_hdr) + sizeof(*ipv4) +
			sizeof(*ipv6), GFP_KERNEL);
	if (!del_hdr)
		return;
	del_hdr->commit = 1;
	del_hdr->num_hdls = 2;
	ipv4 = &del_hdr->hdl[0];
	ipv4->hdl = odu_bridge_ctx->odu_br_ipv4_hdr_hdl;
	ipv6 = &del_hdr->hdl[1];
	ipv6->hdl = odu_bridge_ctx->odu_br_ipv6_hdr_hdl;
	result = ipa_del_hdr(del_hdr);
	if (result || ipv4->status || ipv6->status)
		ODU_BRIDGE_ERR("ipa_del_hdr failed");
	kfree(del_hdr);
}

/**
 * odu_bridge_register_properties() - set Tx/Rx properties for ipacm
 *
 * Register the network interface interface with Tx and Rx properties
 * Tx properties are for data flowing from IPA to adapter, they
 * have Header-Insertion properties both for Ipv4 and Ipv6 Ethernet framing.
 * Rx properties are for data flowing from adapter to IPA, they have
 * simple rule which always "hit".
 *
 */
static int odu_bridge_register_properties(void)
{
	struct ipa_tx_intf tx_properties = {0};
	struct ipa_ioc_tx_intf_prop properties[2] = { {0}, {0} };
	struct ipa_ioc_tx_intf_prop *ipv4_property;
	struct ipa_ioc_tx_intf_prop *ipv6_property;
	struct ipa_ioc_rx_intf_prop rx_ioc_properties[2] = { {0}, {0} };
	struct ipa_rx_intf rx_properties = {0};
	struct ipa_ioc_rx_intf_prop *rx_ipv4_property;
	struct ipa_ioc_rx_intf_prop *rx_ipv6_property;
	int res = 0;

	ODU_BRIDGE_FUNC_ENTRY();

	tx_properties.prop = properties;
	ipv4_property = &tx_properties.prop[0];
	ipv4_property->ip = IPA_IP_v4;
	ipv4_property->dst_pipe = IPA_CLIENT_ODU_EMB_CONS;
	ipv4_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	strlcpy(ipv4_property->hdr_name, ODU_BRIDGE_IPV4_HDR_NAME,
			IPA_RESOURCE_NAME_MAX);
	ipv6_property = &tx_properties.prop[1];
	ipv6_property->ip = IPA_IP_v6;
	ipv6_property->dst_pipe = IPA_CLIENT_ODU_EMB_CONS;
	ipv6_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	strlcpy(ipv6_property->hdr_name, ODU_BRIDGE_IPV6_HDR_NAME,
			IPA_RESOURCE_NAME_MAX);
	tx_properties.num_props = 2;

	rx_properties.prop = rx_ioc_properties;
	rx_ipv4_property = &rx_properties.prop[0];
	rx_ipv4_property->ip = IPA_IP_v4;
	rx_ipv4_property->attrib.attrib_mask = 0;
	rx_ipv4_property->src_pipe = IPA_CLIENT_ODU_PROD;
	rx_ipv4_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	rx_ipv6_property = &rx_properties.prop[1];
	rx_ipv6_property->ip = IPA_IP_v6;
	rx_ipv6_property->attrib.attrib_mask = 0;
	rx_ipv6_property->src_pipe = IPA_CLIENT_ODU_PROD;
	rx_ipv6_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	rx_properties.num_props = 2;

	res = ipa_register_intf(odu_bridge_ctx->netdev_name, &tx_properties,
		&rx_properties);
	if (res) {
		ODU_BRIDGE_ERR("fail on Tx/Rx properties registration %d\n",
									res);
	}

	ODU_BRIDGE_FUNC_EXIT();

	return res;
}

static void odu_bridge_deregister_properties(void)
{
	int res;

	ODU_BRIDGE_FUNC_ENTRY();
	res = ipa_deregister_intf(odu_bridge_ctx->netdev_name);
	if (res)
		ODU_BRIDGE_ERR("Fail on Tx prop deregister %d\n", res);
	ODU_BRIDGE_FUNC_EXIT();
}

/**
 * odu_bridge_init() - Initialize the ODU bridge driver
 * @params: initialization parameters
 *
 * This function initialize all bridge internal data and register odu bridge to
 * kernel for IOCTL and debugfs.
 * Header addition and properties are registered to IPA driver.
 *
 * Return codes: 0: success,
 *		-EINVAL - Bad parameter
 *		Other negative value - Failure
 */
int odu_bridge_init(struct odu_bridge_params *params)
{
	int res;

	ODU_BRIDGE_FUNC_ENTRY();

	if (!params) {
		ODU_BRIDGE_ERR("null pointer params\n");
		return -EINVAL;
	}
	if (!params->netdev_name) {
		ODU_BRIDGE_ERR("null pointer params->netdev_name\n");
		return -EINVAL;
	}
	if (!params->tx_dp_notify) {
		ODU_BRIDGE_ERR("null pointer params->tx_dp_notify\n");
		return -EINVAL;
	}
	if (!params->send_dl_skb) {
		ODU_BRIDGE_ERR("null pointer params->send_dl_skb\n");
		return -EINVAL;
	}
	if (odu_bridge_ctx) {
		ODU_BRIDGE_ERR("Already initialized\n");
		return -EFAULT;
	}
	if (!ipa_is_ready()) {
		ODU_BRIDGE_ERR("IPA is not ready\n");
		return -EFAULT;
	}

	ODU_BRIDGE_DBG("device_ethaddr=%pM\n", params->device_ethaddr);

	odu_bridge_ctx = kzalloc(sizeof(*odu_bridge_ctx), GFP_KERNEL);
	if (!odu_bridge_ctx) {
		ODU_BRIDGE_ERR("kzalloc err.\n");
		return -ENOMEM;
	}

	odu_bridge_ctx->class = class_create(THIS_MODULE, ODU_BRIDGE_DRV_NAME);
	if (!odu_bridge_ctx->class) {
		ODU_BRIDGE_ERR("Class_create err.\n");
		res = -ENODEV;
		goto fail_class_create;
	}

	res = alloc_chrdev_region(&odu_bridge_ctx->dev_num, 0, 1,
				  ODU_BRIDGE_DRV_NAME);
	if (res) {
		ODU_BRIDGE_ERR("alloc_chrdev_region err.\n");
		res = -ENODEV;
		goto fail_alloc_chrdev_region;
	}

	odu_bridge_ctx->dev = device_create(odu_bridge_ctx->class, NULL,
		odu_bridge_ctx->dev_num, odu_bridge_ctx, ODU_BRIDGE_DRV_NAME);
	if (IS_ERR(odu_bridge_ctx->dev)) {
		ODU_BRIDGE_ERR(":device_create err.\n");
		res = -ENODEV;
		goto fail_device_create;
	}

	cdev_init(&odu_bridge_ctx->cdev, &odu_bridge_drv_fops);
	odu_bridge_ctx->cdev.owner = THIS_MODULE;
	odu_bridge_ctx->cdev.ops = &odu_bridge_drv_fops;

	res = cdev_add(&odu_bridge_ctx->cdev, odu_bridge_ctx->dev_num, 1);
	if (res) {
		ODU_BRIDGE_ERR(":cdev_add err=%d\n", -res);
		res = -ENODEV;
		goto fail_cdev_add;
	}

	odu_debugfs_init();

	strlcpy(odu_bridge_ctx->netdev_name, params->netdev_name,
		IPA_RESOURCE_NAME_MAX);
	odu_bridge_ctx->priv = params->priv;
	odu_bridge_ctx->tx_dp_notify = params->tx_dp_notify;
	odu_bridge_ctx->send_dl_skb = params->send_dl_skb;
	memcpy(odu_bridge_ctx->device_ethaddr, params->device_ethaddr,
		ETH_ALEN);
	odu_bridge_ctx->ipa_sys_desc_size = params->ipa_desc_size;
	odu_bridge_ctx->mode = ODU_BRIDGE_MODE_ROUTER;

	mutex_init(&odu_bridge_ctx->lock);

	res = odu_bridge_add_hdrs();
	if (res) {
		ODU_BRIDGE_ERR("fail on odu_bridge_add_hdr %d\n", res);
		goto fail_add_hdrs;
	}

	res = odu_bridge_register_properties();
	if (res) {
		ODU_BRIDGE_ERR("fail on register properties %d\n", res);
		goto fail_register_properties;
	}

	ODU_BRIDGE_FUNC_EXIT();
	return 0;

fail_register_properties:
	odu_bridge_del_hdrs();
fail_add_hdrs:
	odu_debugfs_destroy();
fail_cdev_add:
	device_destroy(odu_bridge_ctx->class, odu_bridge_ctx->dev_num);
fail_device_create:
	unregister_chrdev_region(odu_bridge_ctx->dev_num, 1);
fail_alloc_chrdev_region:
	class_destroy(odu_bridge_ctx->class);
fail_class_create:
	kfree(odu_bridge_ctx);
	odu_bridge_ctx = NULL;
	return res;
}
EXPORT_SYMBOL(odu_bridge_init);

/**
 * odu_bridge_cleanup() - De-Initialize the ODU bridge driver
 *
 * Return codes: 0: success,
 *		-EINVAL - Bad parameter
 *		Other negative value - Failure
 */
int odu_bridge_cleanup(void)
{
	ODU_BRIDGE_FUNC_ENTRY();

	if (!odu_bridge_ctx) {
		ODU_BRIDGE_ERR("Not initialized\n");
		return -EFAULT;
	}

	if (odu_bridge_ctx->is_connected) {
		ODU_BRIDGE_ERR("cannot deinit while bridge is conncetd\n");
		return -EFAULT;
	}

	odu_bridge_deregister_properties();
	odu_bridge_del_hdrs();
	odu_debugfs_destroy();
	cdev_del(&odu_bridge_ctx->cdev);
	device_destroy(odu_bridge_ctx->class, odu_bridge_ctx->dev_num);
	unregister_chrdev_region(odu_bridge_ctx->dev_num, 1);
	class_destroy(odu_bridge_ctx->class);
	ipc_log_context_destroy(odu_bridge_ctx->logbuf);
	ipc_log_context_destroy(odu_bridge_ctx->logbuf_low);
	kfree(odu_bridge_ctx);
	odu_bridge_ctx = NULL;

	ODU_BRIDGE_FUNC_EXIT();
	return 0;
}
EXPORT_SYMBOL(odu_bridge_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ODU bridge driver");
