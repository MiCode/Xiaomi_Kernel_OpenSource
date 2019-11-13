// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/ipa_wigig.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include "../ipa_common_i.h"
#include "../ipa_v3/ipa_pm.h"

#define OFFLOAD_DRV_NAME "ipa_wigig"
#define IPA_WIGIG_DBG(fmt, args...) \
	do { \
		pr_debug(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_WIGIG_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_WIGIG_ERR(fmt, args...) \
	do { \
		pr_err(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_WIGIG_ERR_RL(fmt, args...) \
	do { \
		pr_err_ratelimited_ipa( \
		OFFLOAD_DRV_NAME " %s:%d " fmt, __func__,\
		__LINE__, ## args);\
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)


#define IPA_WIGIG_TX_PIPE_NUM	4

enum ipa_wigig_pipes_idx {
	IPA_CLIENT_WIGIG_PROD_IDX = 0,
	IPA_CLIENT_WIGIG1_CONS_IDX = 1,
	IPA_CLIENT_WIGIG2_CONS_IDX = 2,
	IPA_CLIENT_WIGIG3_CONS_IDX = 3,
	IPA_CLIENT_WIGIG4_CONS_IDX = 4,
	IPA_WIGIG_MAX_PIPES
};

struct ipa_wigig_intf_info {
	char netdev_name[IPA_RESOURCE_NAME_MAX];
	u8 netdev_mac[IPA_MAC_ADDR_SIZE];
	u8 hdr_len;
	u32 partial_hdr_hdl[IPA_IP_MAX];
	struct list_head link;
};

struct ipa_wigig_pipe_values {
	uint8_t dir;
	uint8_t tx_ring_id;
	uint32_t desc_ring_HWHEAD;
	uint16_t desc_ring_HWHEAD_masked;
	uint32_t desc_ring_HWTAIL;
	uint32_t status_ring_HWHEAD;
	uint16_t status_ring_HWHEAD_masked;
	uint32_t status_ring_HWTAIL;
};

struct ipa_wigig_regs_save {
	struct ipa_wigig_pipe_values pipes_val[IPA_WIGIG_MAX_PIPES];
	u32 int_gen_tx_val;
	u32 int_gen_rx_val;
};

struct ipa_wigig_context {
	struct list_head head_intf_list;
	struct mutex lock;
	u32 ipa_pm_hdl;
	phys_addr_t periph_baddr_pa;
	phys_addr_t pseudo_cause_pa;
	phys_addr_t int_gen_tx_pa;
	phys_addr_t int_gen_rx_pa;
	phys_addr_t dma_ep_misc_pa;
	ipa_notify_cb tx_notify;
	void *priv;
	union pipes {
		struct ipa_wigig_pipe_setup_info flat[IPA_WIGIG_MAX_PIPES];
		struct ipa_wigig_pipe_setup_info_smmu
			smmu[IPA_WIGIG_MAX_PIPES];
	} pipes;
	struct ipa_wigig_rx_pipe_data_buffer_info_smmu rx_buff_smmu;
	struct ipa_wigig_tx_pipe_data_buffer_info_smmu
		tx_buff_smmu[IPA_WIGIG_TX_PIPE_NUM];
	char clients_mac[IPA_WIGIG_TX_PIPE_NUM][IPA_MAC_ADDR_SIZE];
	struct ipa_wigig_regs_save regs_save;
	bool smmu_en;
	bool shared_cb;
	u8 conn_pipes;
	struct dentry *parent;
	struct dentry *dent_conn_clients;
	struct dentry *dent_smmu;
};

static struct ipa_wigig_context *ipa_wigig_ctx;

#ifdef CONFIG_DEBUG_FS
static int ipa_wigig_init_debugfs(struct dentry *parent);
static inline void ipa_wigig_deinit_debugfs(void);
#else
static int ipa_wigig_init_debugfs(struct dentry *parent) { return 0; }
static inline void ipa_wigig_deinit_debugfs(void) { }
#endif

int ipa_wigig_init(struct ipa_wigig_init_in_params *in,
	struct ipa_wigig_init_out_params *out)
{
	struct ipa_wdi_uc_ready_params inout;

	if (!in || !out) {
		IPA_WIGIG_ERR("invalid params in=%pK, out %pK\n", in, out);
		return -EINVAL;
	}

	IPA_WIGIG_DBG("\n");
	if (ipa_wigig_ctx) {
		IPA_WIGIG_ERR("ipa_wigig_ctx was initialized before\n");
		return -EINVAL;
	}

	ipa_wigig_ctx = kzalloc(sizeof(*ipa_wigig_ctx), GFP_KERNEL);
	if (ipa_wigig_ctx == NULL)
		return -ENOMEM;

	mutex_init(&ipa_wigig_ctx->lock);
	INIT_LIST_HEAD(&ipa_wigig_ctx->head_intf_list);

	ipa_wigig_ctx->pseudo_cause_pa = in->pseudo_cause_pa;
	ipa_wigig_ctx->int_gen_tx_pa = in->int_gen_tx_pa;
	ipa_wigig_ctx->int_gen_rx_pa = in->int_gen_rx_pa;
	ipa_wigig_ctx->dma_ep_misc_pa = in->dma_ep_misc_pa;
	ipa_wigig_ctx->periph_baddr_pa = in->periph_baddr_pa;

	IPA_WIGIG_DBG(
		"periph_baddr_pa 0x%pa pseudo_cause_pa 0x%pa, int_gen_tx_pa 0x%pa, int_gen_rx_pa 0x%pa, dma_ep_misc_pa 0x%pa"
		, &ipa_wigig_ctx->periph_baddr_pa,
		&ipa_wigig_ctx->pseudo_cause_pa,
		&ipa_wigig_ctx->int_gen_tx_pa,
		&ipa_wigig_ctx->int_gen_rx_pa,
		&ipa_wigig_ctx->dma_ep_misc_pa);

	inout.notify = in->notify;
	inout.priv = in->priv;
	if (ipa_wigig_internal_init(&inout, in->int_notify,
		&out->uc_db_pa)) {
		kfree(ipa_wigig_ctx);
		ipa_wigig_ctx = NULL;
		return -EFAULT;
	}

	IPA_WIGIG_DBG("uc_db_pa 0x%pa\n", &out->uc_db_pa);

	out->is_uc_ready = inout.is_uC_ready;

	out->lan_rx_napi_enable = ipa_get_lan_rx_napi();
	IPA_WIGIG_DBG("LAN RX NAPI enabled = %s\n",
				out->lan_rx_napi_enable ? "True" : "False");

	IPA_WIGIG_DBG("exit\n");

	return 0;
}
EXPORT_SYMBOL(ipa_wigig_init);

int ipa_wigig_cleanup(void)
{
	struct ipa_wigig_intf_info *entry;
	struct ipa_wigig_intf_info *next;

	IPA_WIGIG_DBG("\n");

	if (!ipa_wigig_ctx)
		return -ENODEV;

	/* clear interface list */
	list_for_each_entry_safe(entry, next,
		&ipa_wigig_ctx->head_intf_list, link) {
		list_del(&entry->link);
		kfree(entry);
	}

	mutex_destroy(&ipa_wigig_ctx->lock);

	ipa_wigig_deinit_debugfs();

	kfree(ipa_wigig_ctx);
	ipa_wigig_ctx = NULL;

	IPA_WIGIG_DBG("exit\n");
	return 0;
}
EXPORT_SYMBOL(ipa_wigig_cleanup);

bool ipa_wigig_is_smmu_enabled(void)
{
	struct ipa_smmu_in_params in;
	struct ipa_smmu_out_params out;

	IPA_WIGIG_DBG("\n");

	in.smmu_client = IPA_SMMU_WIGIG_CLIENT;
	ipa_get_smmu_params(&in, &out);

	IPA_WIGIG_DBG("exit (%d)\n", out.smmu_enable);

	return out.smmu_enable;
}
EXPORT_SYMBOL(ipa_wigig_is_smmu_enabled);

static int ipa_wigig_init_smmu_params(void)
{
	struct ipa_smmu_in_params in;
	struct ipa_smmu_out_params out;
	int ret;

	IPA_WIGIG_DBG("\n");

	in.smmu_client = IPA_SMMU_WIGIG_CLIENT;
	ret = ipa_get_smmu_params(&in, &out);
	if (ret) {
		IPA_WIGIG_ERR("couldn't get SMMU params %d\n", ret);
		return ret;
	}
	ipa_wigig_ctx->smmu_en = out.smmu_enable;
	ipa_wigig_ctx->shared_cb = out.shared_cb;
	IPA_WIGIG_DBG("SMMU (%s), 11ad CB (%s)\n",
		out.smmu_enable ? "enabled" : "disabled",
		out.shared_cb ? "shared" : "not shared");

	return 0;
}

static int ipa_wigig_commit_partial_hdr(
	struct ipa_ioc_add_hdr *hdr,
	const char *netdev_name,
	struct ipa_wigig_hdr_info *hdr_info)
{
	int i;

	IPA_WIGIG_DBG("\n");

	if (!netdev_name) {
		IPA_WIGIG_ERR("Invalid input\n");
		return -EINVAL;
	}

	IPA_WIGIG_DBG("dst_mac_addr_offset %d hdr_len %d hdr_type %d\n",
		hdr_info->dst_mac_addr_offset,
		hdr_info->hdr_len,
		hdr_info->hdr_type);

	hdr->commit = 1;
	hdr->num_hdrs = 2;

	snprintf(hdr->hdr[0].name, sizeof(hdr->hdr[0].name),
		"%s_ipv4", netdev_name);
	snprintf(hdr->hdr[1].name, sizeof(hdr->hdr[1].name),
		"%s_ipv6", netdev_name);
	for (i = IPA_IP_v4; i < IPA_IP_MAX; i++) {
		hdr->hdr[i].hdr_len = hdr_info[i].hdr_len;
		memcpy(hdr->hdr[i].hdr, hdr_info[i].hdr, hdr->hdr[i].hdr_len);
		hdr->hdr[i].type = hdr_info[i].hdr_type;
		hdr->hdr[i].is_partial = 1;
		hdr->hdr[i].is_eth2_ofst_valid = 1;
		hdr->hdr[i].eth2_ofst = hdr_info[i].dst_mac_addr_offset;
	}

	if (ipa_add_hdr(hdr)) {
		IPA_WIGIG_ERR("fail to add partial headers\n");
		return -EFAULT;
	}

	IPA_WIGIG_DBG("exit\n");

	return 0;
}

static void ipa_wigig_free_msg(void *msg, uint32_t len, uint32_t type)
{
	IPA_WIGIG_DBG("free msg type:%d, len:%d, buff %pK", type, len, msg);
	kfree(msg);
	IPA_WIGIG_DBG("exit\n");
}

static int ipa_wigig_send_wlan_msg(enum ipa_wlan_event msg_type,
	const char *netdev_name, u8 *mac)
{
	struct ipa_msg_meta msg_meta;
	struct ipa_wlan_msg *wlan_msg;
	int ret;

	IPA_WIGIG_DBG("%d\n", msg_type);

	wlan_msg = kzalloc(sizeof(*wlan_msg), GFP_KERNEL);
	if (wlan_msg == NULL)
		return -ENOMEM;
	strlcpy(wlan_msg->name, netdev_name, IPA_RESOURCE_NAME_MAX);
	memcpy(wlan_msg->mac_addr, mac, IPA_MAC_ADDR_SIZE);
	msg_meta.msg_len = sizeof(struct ipa_wlan_msg);
	msg_meta.msg_type = msg_type;

	IPA_WIGIG_DBG("send msg type:%d, len:%d, buff %pK", msg_meta.msg_type,
		msg_meta.msg_len, wlan_msg);
	ret = ipa_send_msg(&msg_meta, wlan_msg, ipa_wigig_free_msg);

	IPA_WIGIG_DBG("exit\n");

	return ret;
}

int ipa_wigig_send_msg(int msg_type,
	const char *netdev_name, u8 *mac,
	enum ipa_client_type client, bool to_wigig)
{
	struct ipa_msg_meta msg_meta;
	struct ipa_wigig_msg *wigig_msg;
	int ret;

	IPA_WIGIG_DBG("\n");

	wigig_msg = kzalloc(sizeof(struct ipa_wigig_msg), GFP_KERNEL);
	if (wigig_msg == NULL)
		return -ENOMEM;
	strlcpy(wigig_msg->name, netdev_name, IPA_RESOURCE_NAME_MAX);
	memcpy(wigig_msg->client_mac_addr, mac, IPA_MAC_ADDR_SIZE);
	if (msg_type == WIGIG_CLIENT_CONNECT)
		wigig_msg->u.ipa_client = client;
	else
		wigig_msg->u.to_wigig = to_wigig;

	msg_meta.msg_type = msg_type;
	msg_meta.msg_len = sizeof(struct ipa_wigig_msg);

	IPA_WIGIG_DBG("send msg type:%d, len:%d, buff %pK", msg_meta.msg_type,
		msg_meta.msg_len, wigig_msg);
	ret = ipa_send_msg(&msg_meta, wigig_msg, ipa_wigig_free_msg);

	IPA_WIGIG_DBG("exit\n");

	return ret;
}

static int ipa_wigig_get_devname(char *netdev_name)
{
	struct ipa_wigig_intf_info *entry;

	mutex_lock(&ipa_wigig_ctx->lock);

	if (!list_is_singular(&ipa_wigig_ctx->head_intf_list)) {
		IPA_WIGIG_DBG("list is not singular, was an IF registered?\n");
		mutex_unlock(&ipa_wigig_ctx->lock);
		return -EFAULT;
	}
	entry = list_first_entry(&ipa_wigig_ctx->head_intf_list,
		struct ipa_wigig_intf_info,
		link);
	strlcpy(netdev_name, entry->netdev_name, IPA_RESOURCE_NAME_MAX);

	mutex_unlock(&ipa_wigig_ctx->lock);

	return 0;
}

int ipa_wigig_reg_intf(
	struct ipa_wigig_reg_intf_in_params *in)
{
	struct ipa_wigig_intf_info *new_intf;
	struct ipa_wigig_intf_info *entry;
	struct ipa_tx_intf tx;
	struct ipa_rx_intf rx;
	struct ipa_ioc_tx_intf_prop tx_prop[2];
	struct ipa_ioc_rx_intf_prop rx_prop[2];
	struct ipa_ioc_add_hdr *hdr;
	struct ipa_ioc_del_hdr *del_hdr = NULL;
	u32 len;
	int ret = 0;

	IPA_WIGIG_DBG("\n");

	if (in == NULL) {
		IPA_WIGIG_ERR("invalid params in=%pK\n", in);
		return -EINVAL;
	}

	if (!ipa_wigig_ctx) {
		IPA_WIGIG_ERR("wigig ctx is not initialized\n");
		return -EPERM;
	}

	IPA_WIGIG_DBG(
		"register interface for netdev %s, MAC 0x[%X][%X][%X][%X][%X][%X]\n"
		, in->netdev_name,
		in->netdev_mac[0], in->netdev_mac[1], in->netdev_mac[2],
		in->netdev_mac[3], in->netdev_mac[4], in->netdev_mac[5]);

	mutex_lock(&ipa_wigig_ctx->lock);
	list_for_each_entry(entry, &ipa_wigig_ctx->head_intf_list, link)
		if (strcmp(entry->netdev_name, in->netdev_name) == 0) {
			IPA_WIGIG_DBG("intf was added before.\n");
			mutex_unlock(&ipa_wigig_ctx->lock);
			return 0;
		}

	IPA_WIGIG_DBG("intf was not added before, proceed.\n");
	new_intf = kzalloc(sizeof(*new_intf), GFP_KERNEL);
	if (new_intf == NULL) {
		ret = -ENOMEM;
		goto fail;
	}

	INIT_LIST_HEAD(&new_intf->link);
	strlcpy(new_intf->netdev_name, in->netdev_name,
		sizeof(new_intf->netdev_name));
	new_intf->hdr_len = in->hdr_info[0].hdr_len;
	memcpy(new_intf->netdev_mac, in->netdev_mac, IPA_MAC_ADDR_SIZE);

	/* add partial header */
	len = sizeof(struct ipa_ioc_add_hdr) + 2 * sizeof(struct ipa_hdr_add);
	hdr = kzalloc(len, GFP_KERNEL);
	if (hdr == NULL) {
		ret = -EFAULT;
		goto fail_alloc_hdr;
	}

	if (ipa_wigig_commit_partial_hdr(hdr,
		in->netdev_name,
		in->hdr_info)) {
		IPA_WIGIG_ERR("fail to commit partial headers\n");
		ret = -EFAULT;
		goto fail_commit_hdr;
	}

	new_intf->partial_hdr_hdl[IPA_IP_v4] = hdr->hdr[IPA_IP_v4].hdr_hdl;
	new_intf->partial_hdr_hdl[IPA_IP_v6] = hdr->hdr[IPA_IP_v6].hdr_hdl;
	IPA_WIGIG_DBG("IPv4 hdr hdl: %d IPv6 hdr hdl: %d\n",
		hdr->hdr[IPA_IP_v4].hdr_hdl, hdr->hdr[IPA_IP_v6].hdr_hdl);

	/* populate tx prop */
	tx.num_props = 2;
	tx.prop = tx_prop;

	memset(tx_prop, 0, sizeof(tx_prop));
	tx_prop[0].ip = IPA_IP_v4;
	/*
	 * for consumers, we register a default pipe, but IPACM will determine
	 * the actual pipe according to the relevant client MAC
	 */
	tx_prop[0].dst_pipe = IPA_CLIENT_WIGIG1_CONS;
	tx_prop[0].hdr_l2_type = in->hdr_info[0].hdr_type;
	strlcpy(tx_prop[0].hdr_name, hdr->hdr[IPA_IP_v4].name,
		sizeof(tx_prop[0].hdr_name));

	tx_prop[1].ip = IPA_IP_v6;
	tx_prop[1].dst_pipe = IPA_CLIENT_WIGIG1_CONS;
	tx_prop[1].hdr_l2_type = in->hdr_info[1].hdr_type;
	strlcpy(tx_prop[1].hdr_name, hdr->hdr[IPA_IP_v6].name,
		sizeof(tx_prop[1].hdr_name));

	/* populate rx prop */
	rx.num_props = 2;
	rx.prop = rx_prop;

	memset(rx_prop, 0, sizeof(rx_prop));
	rx_prop[0].ip = IPA_IP_v4;
	rx_prop[0].src_pipe = IPA_CLIENT_WIGIG_PROD;
	rx_prop[0].hdr_l2_type = in->hdr_info[0].hdr_type;

	rx_prop[1].ip = IPA_IP_v6;
	rx_prop[1].src_pipe = IPA_CLIENT_WIGIG_PROD;
	rx_prop[1].hdr_l2_type = in->hdr_info[1].hdr_type;

	if (ipa_register_intf(in->netdev_name, &tx, &rx)) {
		IPA_WIGIG_ERR("fail to add interface prop\n");
		ret = -EFAULT;
		goto fail_register;
	}

	if (ipa_wigig_send_wlan_msg(WLAN_AP_CONNECT,
		in->netdev_name,
		in->netdev_mac)) {
		IPA_WIGIG_ERR("couldn't send msg to IPACM\n");
		ret = -EFAULT;
		goto fail_sendmsg;
	}

	list_add(&new_intf->link, &ipa_wigig_ctx->head_intf_list);

	kfree(hdr);
	mutex_unlock(&ipa_wigig_ctx->lock);

	IPA_WIGIG_DBG("exit\n");
	return 0;
fail_sendmsg:
	ipa_deregister_intf(in->netdev_name);
fail_register:
	del_hdr = kzalloc(sizeof(struct ipa_ioc_del_hdr) +
		2 * sizeof(struct ipa_hdr_del), GFP_KERNEL);
	if (del_hdr) {
		del_hdr->commit = 1;
		del_hdr->num_hdls = 2;
		del_hdr->hdl[0].hdl = new_intf->partial_hdr_hdl[IPA_IP_v4];
		del_hdr->hdl[1].hdl = new_intf->partial_hdr_hdl[IPA_IP_v6];
		ipa_del_hdr(del_hdr);
		kfree(del_hdr);
	}
	new_intf->partial_hdr_hdl[IPA_IP_v4] = 0;
	new_intf->partial_hdr_hdl[IPA_IP_v6] = 0;
fail_commit_hdr:
	kfree(hdr);
fail_alloc_hdr:
	kfree(new_intf);
fail:
	mutex_unlock(&ipa_wigig_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(ipa_wigig_reg_intf);

int ipa_wigig_dereg_intf(const char *netdev_name)
{
	int len, ret;
	struct ipa_ioc_del_hdr *hdr = NULL;
	struct ipa_wigig_intf_info *entry;
	struct ipa_wigig_intf_info *next;

	if (!netdev_name) {
		IPA_WIGIG_ERR("no netdev name\n");
		return -EINVAL;
	}

	IPA_WIGIG_DBG("netdev %s\n", netdev_name);

	if (!ipa_wigig_ctx) {
		IPA_WIGIG_ERR("wigig ctx is not initialized\n");
		return -EPERM;
	}

	mutex_lock(&ipa_wigig_ctx->lock);

	ret = -EFAULT;

	list_for_each_entry_safe(entry, next, &ipa_wigig_ctx->head_intf_list,
		link)
		if (strcmp(entry->netdev_name, netdev_name) == 0) {
			len = sizeof(struct ipa_ioc_del_hdr) +
				2 * sizeof(struct ipa_hdr_del);
			hdr = kzalloc(len, GFP_KERNEL);
			if (hdr == NULL) {
				mutex_unlock(&ipa_wigig_ctx->lock);
				return -ENOMEM;
			}

			hdr->commit = 1;
			hdr->num_hdls = 2;
			hdr->hdl[0].hdl = entry->partial_hdr_hdl[0];
			hdr->hdl[1].hdl = entry->partial_hdr_hdl[1];
			IPA_WIGIG_DBG("IPv4 hdr hdl: %d IPv6 hdr hdl: %d\n",
				hdr->hdl[0].hdl, hdr->hdl[1].hdl);

			if (ipa_del_hdr(hdr)) {
				IPA_WIGIG_ERR(
					"fail to delete partial header\n");
				ret = -EFAULT;
				goto fail;
			}

			if (ipa_deregister_intf(entry->netdev_name)) {
				IPA_WIGIG_ERR("fail to del interface props\n");
				ret = -EFAULT;
				goto fail;
			}

			if (ipa_wigig_send_wlan_msg(WLAN_AP_DISCONNECT,
				entry->netdev_name,
				entry->netdev_mac)) {
				IPA_WIGIG_ERR("couldn't send msg to IPACM\n");
				ret = -EFAULT;
				goto fail;
			}

			list_del(&entry->link);
			kfree(entry);

			ret = 0;
			break;
		}

	IPA_WIGIG_DBG("exit\n");
fail:
	kfree(hdr);
	mutex_unlock(&ipa_wigig_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(ipa_wigig_dereg_intf);

static void ipa_wigig_pm_cb(void *p, enum ipa_pm_cb_event event)
{
	IPA_WIGIG_DBG("received pm event %d\n", event);
}

static int ipa_wigig_store_pipe_info(struct ipa_wigig_pipe_setup_info *pipe,
	unsigned int idx)
{
	IPA_WIGIG_DBG(
		"idx %d: desc_ring HWHEAD_pa %pa, HWTAIL_pa %pa, status_ring HWHEAD_pa %pa, HWTAIL_pa %pa\n",
		idx,
		&pipe->desc_ring_HWHEAD_pa,
		&pipe->desc_ring_HWTAIL_pa,
		&pipe->status_ring_HWHEAD_pa,
		&pipe->status_ring_HWTAIL_pa);

	/* store regs */
	ipa_wigig_ctx->pipes.flat[idx].desc_ring_HWHEAD_pa =
		pipe->desc_ring_HWHEAD_pa;
	ipa_wigig_ctx->pipes.flat[idx].desc_ring_HWTAIL_pa =
		pipe->desc_ring_HWTAIL_pa;

	ipa_wigig_ctx->pipes.flat[idx].status_ring_HWHEAD_pa =
		pipe->status_ring_HWHEAD_pa;

	ipa_wigig_ctx->pipes.flat[idx].status_ring_HWTAIL_pa =
		pipe->status_ring_HWTAIL_pa;

	IPA_WIGIG_DBG("exit\n");

	return 0;
}

static u8 ipa_wigig_pipe_to_bit_val(int client)
{
	u8 shift_val;

	switch (client) {
	case IPA_CLIENT_WIGIG_PROD:
		shift_val = 0x1 << IPA_CLIENT_WIGIG_PROD_IDX;
		break;
	case IPA_CLIENT_WIGIG1_CONS:
		shift_val = 0x1 << IPA_CLIENT_WIGIG1_CONS_IDX;
		break;
	case IPA_CLIENT_WIGIG2_CONS:
		shift_val = 0x1 << IPA_CLIENT_WIGIG2_CONS_IDX;
		break;
	case IPA_CLIENT_WIGIG3_CONS:
		shift_val = 0x1 << IPA_CLIENT_WIGIG3_CONS_IDX;
		break;
	case IPA_CLIENT_WIGIG4_CONS:
		shift_val = 0x1 << IPA_CLIENT_WIGIG4_CONS_IDX;
		break;
	default:
		IPA_WIGIG_ERR("invalid pipe %d\n", client);
		return 1;
	}

	return shift_val;
}

int ipa_wigig_conn_rx_pipe(struct ipa_wigig_conn_rx_in_params *in,
	struct ipa_wigig_conn_out_params *out)
{
	int ret;
	struct ipa_pm_register_params pm_params;

	IPA_WIGIG_DBG("\n");

	if (!in || !out) {
		IPA_WIGIG_ERR("empty parameters. in=%pK out=%pK\n", in, out);
		return -EINVAL;
	}

	if (!ipa_wigig_ctx) {
		IPA_WIGIG_ERR("wigig ctx is not initialized\n");
		return -EPERM;
	}

	ret = ipa_uc_state_check();
	if (ret) {
		IPA_WIGIG_ERR("uC not ready\n");
		return ret;
	}

	if (ipa_wigig_init_smmu_params())
		return -EINVAL;

	if (ipa_wigig_ctx->smmu_en) {
		IPA_WIGIG_ERR("IPA SMMU is enabled, wrong API used\n");
		return -EFAULT;
	}

	memset(&pm_params, 0, sizeof(pm_params));
	pm_params.name = "wigig";
	pm_params.callback = ipa_wigig_pm_cb;
	pm_params.user_data = NULL;
	pm_params.group = IPA_PM_GROUP_DEFAULT;
	if (ipa_pm_register(&pm_params, &ipa_wigig_ctx->ipa_pm_hdl)) {
		IPA_WIGIG_ERR("fail to register ipa pm\n");
		ret = -EFAULT;
		goto fail_pm;
	}
	IPA_WIGIG_DBG("pm hdl %d\n", ipa_wigig_ctx->ipa_pm_hdl);

	ret = ipa_wigig_uc_msi_init(true,
		ipa_wigig_ctx->periph_baddr_pa,
		ipa_wigig_ctx->pseudo_cause_pa,
		ipa_wigig_ctx->int_gen_tx_pa,
		ipa_wigig_ctx->int_gen_rx_pa,
		ipa_wigig_ctx->dma_ep_misc_pa);
	if (ret) {
		IPA_WIGIG_ERR("failed configuring msi regs at uC\n");
		ret = -EFAULT;
		goto fail_msi;
	}

	if (ipa_conn_wigig_rx_pipe_i(in, out, &ipa_wigig_ctx->parent)) {
		IPA_WIGIG_ERR("fail to connect rx pipe\n");
		ret = -EFAULT;
		goto fail_connect_pipe;
	}

	ipa_wigig_ctx->tx_notify = in->notify;
	ipa_wigig_ctx->priv = in->priv;

	if (ipa_wigig_ctx->parent)
		ipa_wigig_init_debugfs(ipa_wigig_ctx->parent);

	ipa_wigig_store_pipe_info(ipa_wigig_ctx->pipes.flat,
		IPA_CLIENT_WIGIG_PROD_IDX);

	ipa_wigig_ctx->conn_pipes |=
		ipa_wigig_pipe_to_bit_val(IPA_CLIENT_WIGIG_PROD);

	IPA_WIGIG_DBG("exit\n");

	return 0;

fail_connect_pipe:
	ipa_wigig_uc_msi_init(false,
		ipa_wigig_ctx->periph_baddr_pa,
		ipa_wigig_ctx->pseudo_cause_pa,
		ipa_wigig_ctx->int_gen_tx_pa,
		ipa_wigig_ctx->int_gen_rx_pa,
		ipa_wigig_ctx->dma_ep_misc_pa);
fail_msi:
	ipa_pm_deregister(ipa_wigig_ctx->ipa_pm_hdl);
fail_pm:
	return ret;
}
EXPORT_SYMBOL(ipa_wigig_conn_rx_pipe);

static int ipa_wigig_client_to_idx(enum ipa_client_type client,
	unsigned int *idx)
{
	switch (client) {
	case IPA_CLIENT_WIGIG1_CONS:
		*idx = 1;
		break;
	case IPA_CLIENT_WIGIG2_CONS:
		*idx = 2;
		break;
	case IPA_CLIENT_WIGIG3_CONS:
		*idx = 3;
		break;
	case IPA_CLIENT_WIGIG4_CONS:
		*idx = 4;
		break;
	default:
		IPA_WIGIG_ERR("invalid client %d\n", client);
		return -EINVAL;
	}

	return 0;
}

static int ipa_wigig_clean_pipe_info(unsigned int idx)
{
	IPA_WIGIG_DBG("cleaning pipe %d info\n", idx);

	if (idx >= IPA_WIGIG_MAX_PIPES) {
		IPA_WIGIG_ERR("invalid index %d\n", idx);
		return -EINVAL;
	}

	if (ipa_wigig_ctx->smmu_en) {
		sg_free_table(
			&ipa_wigig_ctx->pipes.smmu[idx].desc_ring_base);
		sg_free_table(
			&ipa_wigig_ctx->pipes.smmu[idx].status_ring_base);

		memset(ipa_wigig_ctx->pipes.smmu + idx,
			0,
			sizeof(ipa_wigig_ctx->pipes.smmu[idx]));
	} else {
		memset(ipa_wigig_ctx->pipes.flat + idx, 0,
			sizeof(ipa_wigig_ctx->pipes.flat[idx]));
	}

	IPA_WIGIG_DBG("exit\n");

	return 0;
}

static int ipa_wigig_clone_sg_table(struct sg_table *source,
	struct sg_table *dst)
{
	struct scatterlist *next, *s, *sglist;
	int i, nents = source->nents;

	if (sg_alloc_table(dst, nents, GFP_KERNEL))
		return -EINVAL;
	next = dst->sgl;
	sglist = source->sgl;
	for_each_sg(sglist, s, nents, i) {
		*next = *s;
		next = sg_next(next);
	}

	dst->nents = nents;
	dst->orig_nents = source->orig_nents;

	return 0;
}

static int ipa_wigig_store_pipe_smmu_info
	(struct ipa_wigig_pipe_setup_info_smmu *pipe_smmu, unsigned int idx)
{
	int ret;

	IPA_WIGIG_DBG(
		"idx %d: desc_ring HWHEAD_pa %pa, HWTAIL_pa %pa, status_ring HWHEAD_pa %pa, HWTAIL_pa %pa, desc_ring_base 0x%llx, status_ring_base 0x%llx\n",
		idx,
		&pipe_smmu->desc_ring_HWHEAD_pa,
		&pipe_smmu->desc_ring_HWTAIL_pa,
		&pipe_smmu->status_ring_HWHEAD_pa,
		&pipe_smmu->status_ring_HWTAIL_pa,
		(unsigned long long)pipe_smmu->desc_ring_base_iova,
		(unsigned long long)pipe_smmu->status_ring_base_iova);

	/* store regs */
	ipa_wigig_ctx->pipes.smmu[idx].desc_ring_HWHEAD_pa =
		pipe_smmu->desc_ring_HWHEAD_pa;
	ipa_wigig_ctx->pipes.smmu[idx].desc_ring_HWTAIL_pa =
		pipe_smmu->desc_ring_HWTAIL_pa;

	ipa_wigig_ctx->pipes.smmu[idx].status_ring_HWHEAD_pa =
		pipe_smmu->status_ring_HWHEAD_pa;
	ipa_wigig_ctx->pipes.smmu[idx].status_ring_HWTAIL_pa =
		pipe_smmu->status_ring_HWTAIL_pa;

	/* store rings IOVAs */
	ipa_wigig_ctx->pipes.smmu[idx].desc_ring_base_iova =
		pipe_smmu->desc_ring_base_iova;
	ipa_wigig_ctx->pipes.smmu[idx].status_ring_base_iova =
		pipe_smmu->status_ring_base_iova;

	/* copy sgt */
	ret = ipa_wigig_clone_sg_table(
		&pipe_smmu->desc_ring_base,
		&ipa_wigig_ctx->pipes.smmu[idx].desc_ring_base);
	if (ret)
		goto fail_desc;

	ret = ipa_wigig_clone_sg_table(
		&pipe_smmu->status_ring_base,
		&ipa_wigig_ctx->pipes.smmu[idx].status_ring_base);
	if (ret)
		goto fail_stat;

	IPA_WIGIG_DBG("exit\n");

	return 0;
fail_stat:
	sg_free_table(&ipa_wigig_ctx->pipes.smmu[idx].desc_ring_base);
	memset(&ipa_wigig_ctx->pipes.smmu[idx].desc_ring_base, 0,
	       sizeof(ipa_wigig_ctx->pipes.smmu[idx].desc_ring_base));
fail_desc:
	return ret;
}

static int ipa_wigig_get_pipe_smmu_info(
	struct ipa_wigig_pipe_setup_info_smmu **pipe_smmu, unsigned int idx)
{
	if (idx >= IPA_WIGIG_MAX_PIPES) {
		IPA_WIGIG_ERR("exceeded pipe num %d > %d\n",
			idx, IPA_WIGIG_MAX_PIPES);
		return -EINVAL;
	}

	*pipe_smmu = &ipa_wigig_ctx->pipes.smmu[idx];

	return 0;
}

static int ipa_wigig_get_pipe_info(
	struct ipa_wigig_pipe_setup_info **pipe, unsigned int idx)
{
	if (idx >= IPA_WIGIG_MAX_PIPES) {
		IPA_WIGIG_ERR("exceeded pipe num %d >= %d\n", idx,
			IPA_WIGIG_MAX_PIPES);
		return -EINVAL;
	}

	*pipe = &ipa_wigig_ctx->pipes.flat[idx];

	return 0;
}

static int ipa_wigig_get_regs_addr(
	void __iomem **desc_ring_h, void __iomem **desc_ring_t,
	void __iomem **status_ring_h, void __iomem **status_ring_t,
	unsigned int idx)
{
	struct ipa_wigig_pipe_setup_info *pipe;
	struct ipa_wigig_pipe_setup_info_smmu *pipe_smmu;
	int ret = 0;

	IPA_WIGIG_DBG("\n");

	if (idx >= IPA_WIGIG_MAX_PIPES) {
		IPA_WIGIG_DBG("exceeded pipe num %d >= %d\n", idx,
			IPA_WIGIG_MAX_PIPES);
		return -EINVAL;
	}

	if (!ipa_wigig_ctx) {
		IPA_WIGIG_DBG("wigig ctx is not initialized\n");
		return -EPERM;
	}

	if (!(ipa_wigig_ctx->conn_pipes &
		ipa_wigig_pipe_to_bit_val(IPA_CLIENT_WIGIG_PROD))) {
		IPA_WIGIG_DBG(
			"must connect rx pipe before connecting any client\n");
		return -EINVAL;
	}

	if (ipa_wigig_ctx->smmu_en) {
		ret = ipa_wigig_get_pipe_smmu_info(&pipe_smmu, idx);
		if (ret)
			return -EINVAL;

		*desc_ring_h =
			ioremap(pipe_smmu->desc_ring_HWHEAD_pa, sizeof(u32));
		if (!*desc_ring_h) {
			IPA_WIGIG_DBG(
				"couldn't ioremap desc ring head address\n");
			ret = -EINVAL;
			goto fail_map_desc_h;
		}
		*desc_ring_t =
			ioremap(pipe_smmu->desc_ring_HWTAIL_pa, sizeof(u32));
		if (!*desc_ring_t) {
			IPA_WIGIG_DBG(
				"couldn't ioremap desc ring tail address\n");
			ret = -EINVAL;
			goto fail_map_desc_t;
		}
		*status_ring_h =
			ioremap(pipe_smmu->status_ring_HWHEAD_pa, sizeof(u32));
		if (!*status_ring_h) {
			IPA_WIGIG_DBG(
				"couldn't ioremap status ring head address\n");
			ret = -EINVAL;
			goto fail_map_status_h;
		}
		*status_ring_t =
			ioremap(pipe_smmu->status_ring_HWTAIL_pa, sizeof(u32));
		if (!*status_ring_t) {
			IPA_WIGIG_DBG(
				"couldn't ioremap status ring tail address\n");
			ret = -EINVAL;
			goto fail_map_status_t;
		}
	} else {
		ret = ipa_wigig_get_pipe_info(&pipe, idx);
		if (ret)
			return -EINVAL;

		*desc_ring_h = ioremap(pipe->desc_ring_HWHEAD_pa, sizeof(u32));
		if (!*desc_ring_h) {
			IPA_WIGIG_DBG(
				"couldn't ioremap desc ring head address\n");
			ret = -EINVAL;
			goto fail_map_desc_h;
		}
		*desc_ring_t = ioremap(pipe->desc_ring_HWTAIL_pa, sizeof(u32));
		if (!*desc_ring_t) {
			IPA_WIGIG_DBG(
				"couldn't ioremap desc ring tail address\n");
			ret = -EINVAL;
			goto fail_map_desc_t;
		}
		*status_ring_h =
			ioremap(pipe->status_ring_HWHEAD_pa, sizeof(u32));
		if (!*status_ring_h) {
			IPA_WIGIG_DBG(
				"couldn't ioremap status ring head address\n");
			ret = -EINVAL;
			goto fail_map_status_h;
		}
		*status_ring_t =
			ioremap(pipe->status_ring_HWTAIL_pa, sizeof(u32));
		if (!*status_ring_t) {
			IPA_WIGIG_DBG(
				"couldn't ioremap status ring tail address\n");
			ret = -EINVAL;
			goto fail_map_status_t;
		}
	}

	IPA_WIGIG_DBG("exit\n");
	return 0;

fail_map_status_t:
	iounmap(*status_ring_h);
fail_map_status_h:
	iounmap(*desc_ring_t);
fail_map_desc_t:
	iounmap(*desc_ring_h);
fail_map_desc_h:
	IPA_WIGIG_DBG("couldn't get regs information idx %d\n", idx);
	return ret;
}

int ipa_wigig_save_regs(void)
{
	void __iomem *desc_ring_h = NULL, *desc_ring_t = NULL,
		*status_ring_h = NULL, *status_ring_t = NULL,
		*int_gen_rx_pa = NULL, *int_gen_tx_pa = NULL;
	uint32_t readval;
	u8 pipe_connected;
	int i, ret = 0;

	IPA_WIGIG_DBG("Start collecting pipes information\n");

	if (!ipa_wigig_ctx) {
		IPA_WIGIG_ERR("wigig ctx is not initialized\n");
		return -EPERM;
	}
	if (!(ipa_wigig_ctx->conn_pipes &
		ipa_wigig_pipe_to_bit_val(IPA_CLIENT_WIGIG_PROD))) {
		IPA_WIGIG_ERR(
			"must connect rx pipe before connecting any client\n");
		return -EINVAL;
	}

	for (i = 0; i < IPA_WIGIG_MAX_PIPES; i++) {
		pipe_connected = (ipa_wigig_ctx->conn_pipes & (0x1 << i));
		if (pipe_connected) {
			uint32_t mask;
			uint8_t shift;

			ret = ipa_wigig_get_regs_addr(
				&desc_ring_h, &desc_ring_t,
				&status_ring_h, &status_ring_t, i);

			if (ret) {
				IPA_WIGIG_ERR(
					"couldn't get registers information on client %d\n",
					i);
				return -EINVAL;
			}

			IPA_WIGIG_DBG("collecting pipe info of index %d\n", i);
			if (i == IPA_CLIENT_WIGIG_PROD_IDX) {
				ipa_wigig_ctx->regs_save.pipes_val[i].dir = 0;
			} else {
				ipa_wigig_ctx->regs_save.pipes_val[i].dir = 1;
				/* TX ids start from 2 */
				ipa_wigig_ctx->regs_save.pipes_val[i]
					.tx_ring_id = i + 1;
			}

			readval = readl_relaxed(desc_ring_h);
			ipa_wigig_ctx->regs_save.pipes_val[i].desc_ring_HWHEAD =
				readval;
			/* HWHEAD LSbs are for even IDs, MSbs for odd IDs */
			if (i != IPA_CLIENT_WIGIG_PROD_IDX) {
				mask = 0xFFFF0000;
				shift = 16;

				if ((ipa_wigig_ctx->regs_save.pipes_val[i]
					.tx_ring_id % 2) == 0) {
					mask = 0x0000FFFF;
					shift = 0;
				}
				ipa_wigig_ctx->regs_save.pipes_val[i]
					.desc_ring_HWHEAD_masked =
					(readval & mask) >> shift;
			}
			readval = readl_relaxed(desc_ring_t);
			ipa_wigig_ctx->regs_save.pipes_val[i].desc_ring_HWTAIL =
				readval;
			readval = readl_relaxed(status_ring_h);
			ipa_wigig_ctx->regs_save.pipes_val[i]
				.status_ring_HWHEAD = readval;
			/* two status rings, MSbs for RX LSbs for TX */
			if (i == IPA_CLIENT_WIGIG_PROD_IDX) {
				mask = 0xFFFF0000;
				shift = 16;
			} else {
				mask = 0x0000FFFF;
				shift = 0;
			}
			ipa_wigig_ctx->regs_save.pipes_val[i]
				.status_ring_HWHEAD_masked =
				(readval & mask) >> shift;

			readval = readl_relaxed(status_ring_t);
			ipa_wigig_ctx->regs_save.pipes_val[i]
				.status_ring_HWTAIL = readval;
			/* unmap all regs */
			iounmap(desc_ring_h);
			iounmap(desc_ring_t);
			iounmap(status_ring_h);
			iounmap(status_ring_t);
		}
	}
	int_gen_rx_pa = ioremap(ipa_wigig_ctx->int_gen_rx_pa, sizeof(u32));
	if (!int_gen_rx_pa) {
		IPA_WIGIG_ERR("couldn't ioremap gen rx address\n");
		ret = -EINVAL;
		goto fail_map_gen_rx;
	}
	int_gen_tx_pa = ioremap(ipa_wigig_ctx->int_gen_tx_pa, sizeof(u32));
	if (!int_gen_tx_pa) {
		IPA_WIGIG_ERR("couldn't ioremap gen tx address\n");
		ret = -EINVAL;
		goto fail_map_gen_tx;
	}

	IPA_WIGIG_DBG("collecting int_gen_rx_pa info\n");
	readval = readl_relaxed(int_gen_rx_pa);
	ipa_wigig_ctx->regs_save.int_gen_rx_val = readval;

	IPA_WIGIG_DBG("collecting int_gen_tx_pa info\n");
	readval = readl_relaxed(int_gen_tx_pa);
	ipa_wigig_ctx->regs_save.int_gen_tx_val = readval;

	IPA_WIGIG_DBG("Finish collecting pipes info\n");
	IPA_WIGIG_DBG("exit\n");

	iounmap(int_gen_tx_pa);
fail_map_gen_tx:
	iounmap(int_gen_rx_pa);
fail_map_gen_rx:
	return ret;
}

static void ipa_wigig_clean_rx_buff_smmu_info(void)
{
	IPA_WIGIG_DBG("clearing rx buff smmu info\n");

	sg_free_table(&ipa_wigig_ctx->rx_buff_smmu.data_buffer_base);
	memset(&ipa_wigig_ctx->rx_buff_smmu,
		0,
		sizeof(ipa_wigig_ctx->rx_buff_smmu));

	IPA_WIGIG_DBG("\n");
}

static int ipa_wigig_store_rx_buff_smmu_info(
	struct ipa_wigig_rx_pipe_data_buffer_info_smmu *dbuff_smmu)
{
	IPA_WIGIG_DBG("\n");
	if (ipa_wigig_clone_sg_table(&dbuff_smmu->data_buffer_base,
		&ipa_wigig_ctx->rx_buff_smmu.data_buffer_base))
		return -EINVAL;

	ipa_wigig_ctx->rx_buff_smmu.data_buffer_base_iova =
		dbuff_smmu->data_buffer_base_iova;
	ipa_wigig_ctx->rx_buff_smmu.data_buffer_size =
		dbuff_smmu->data_buffer_size;

	IPA_WIGIG_DBG("exit\n");

	return 0;
}

static int ipa_wigig_get_rx_buff_smmu_info(
	struct ipa_wigig_rx_pipe_data_buffer_info_smmu **dbuff_smmu)
{
	IPA_WIGIG_DBG("\n");

	*dbuff_smmu = &ipa_wigig_ctx->rx_buff_smmu;

	IPA_WIGIG_DBG("exit\n");

	return 0;
}

static int ipa_wigig_store_tx_buff_smmu_info(
	struct ipa_wigig_tx_pipe_data_buffer_info_smmu *dbuff_smmu,
	unsigned int idx)
{
	int result, i;
	struct ipa_wigig_tx_pipe_data_buffer_info_smmu *tx_buff_smmu;

	IPA_WIGIG_DBG("\n");

	if (idx > (IPA_WIGIG_TX_PIPE_NUM - 1)) {
		IPA_WIGIG_ERR("invalid tx index %d\n", idx);
		return -EINVAL;
	}

	tx_buff_smmu = ipa_wigig_ctx->tx_buff_smmu + idx;

	tx_buff_smmu->data_buffer_base =
		kcalloc(dbuff_smmu->num_buffers,
			sizeof(struct sg_table),
			GFP_KERNEL);
	if (!tx_buff_smmu->data_buffer_base)
		return -ENOMEM;

	tx_buff_smmu->data_buffer_base_iova =
		kcalloc(dbuff_smmu->num_buffers, sizeof(u64), GFP_KERNEL);
	if (!tx_buff_smmu->data_buffer_base_iova) {
		result = -ENOMEM;
		goto fail_iova;
	}

	for (i = 0; i < dbuff_smmu->num_buffers; i++) {
		result = ipa_wigig_clone_sg_table(
			dbuff_smmu->data_buffer_base + i,
			tx_buff_smmu->data_buffer_base + i);
		if (result)
			goto fail_sg_clone;

		tx_buff_smmu->data_buffer_base_iova[i] =
			dbuff_smmu->data_buffer_base_iova[i];
	}
	tx_buff_smmu->num_buffers = dbuff_smmu->num_buffers;
	tx_buff_smmu->data_buffer_size =
		dbuff_smmu->data_buffer_size;

	IPA_WIGIG_DBG("exit\n");

	return 0;
fail_sg_clone:
	i--;
	for (; i >= 0; i--)
		sg_free_table(tx_buff_smmu->data_buffer_base + i);
	kfree(tx_buff_smmu->data_buffer_base_iova);
	tx_buff_smmu->data_buffer_base_iova = NULL;
fail_iova:
	kfree(tx_buff_smmu->data_buffer_base);
	tx_buff_smmu->data_buffer_base = NULL;
	return result;
}

static int ipa_wigig_clean_tx_buff_smmu_info(unsigned int idx)
{
	unsigned int i;
	struct ipa_wigig_tx_pipe_data_buffer_info_smmu *dbuff_smmu;

	IPA_WIGIG_DBG("\n");

	if (idx > (IPA_WIGIG_TX_PIPE_NUM - 1)) {
		IPA_WIGIG_ERR("invalid tx index %d\n", idx);
		return -EINVAL;
	}

	dbuff_smmu = &ipa_wigig_ctx->tx_buff_smmu[idx];

	if (!dbuff_smmu->data_buffer_base) {
		IPA_WIGIG_ERR("no pa has been allocated\n");
		return -EFAULT;
	}

	for (i = 0; i < dbuff_smmu->num_buffers; i++)
		sg_free_table(dbuff_smmu->data_buffer_base + i);

	kfree(dbuff_smmu->data_buffer_base);
	dbuff_smmu->data_buffer_base = NULL;

	kfree(dbuff_smmu->data_buffer_base_iova);
	dbuff_smmu->data_buffer_base_iova = NULL;

	dbuff_smmu->data_buffer_size = 0;
	dbuff_smmu->num_buffers = 0;

	IPA_WIGIG_DBG("exit\n");

	return 0;
}

static int ipa_wigig_get_tx_buff_smmu_info(
struct ipa_wigig_tx_pipe_data_buffer_info_smmu **dbuff_smmu,
	unsigned int idx)
{
	if (idx > (IPA_WIGIG_TX_PIPE_NUM - 1)) {
		IPA_WIGIG_ERR("invalid tx index %d\n", idx);
		return -EINVAL;
	}

	*dbuff_smmu = &ipa_wigig_ctx->tx_buff_smmu[idx];

	return 0;
}

static int ipa_wigig_store_rx_smmu_info
	(struct ipa_wigig_conn_rx_in_params_smmu *in)
{
	int ret;

	IPA_WIGIG_DBG("\n");

	ret = ipa_wigig_store_pipe_smmu_info(&in->pipe_smmu,
		IPA_CLIENT_WIGIG_PROD_IDX);
	if (ret)
		return ret;

	if (!ipa_wigig_ctx->shared_cb) {
		ret = ipa_wigig_store_rx_buff_smmu_info(&in->dbuff_smmu);
		if (ret)
			goto fail_buff;
	}

	IPA_WIGIG_DBG("exit\n");

	return 0;

fail_buff:
	ipa_wigig_clean_pipe_info(IPA_CLIENT_WIGIG_PROD_IDX);
	return ret;
}

static int ipa_wigig_store_client_smmu_info
(struct ipa_wigig_conn_tx_in_params_smmu *in, enum ipa_client_type client)
{
	int ret;
	unsigned int idx;

	IPA_WIGIG_DBG("\n");

	ret = ipa_wigig_client_to_idx(client, &idx);
	if (ret)
		return ret;

	ret = ipa_wigig_store_pipe_smmu_info(&in->pipe_smmu, idx);
	if (ret)
		return ret;

	if (!ipa_wigig_ctx->shared_cb) {
		ret = ipa_wigig_store_tx_buff_smmu_info(
			&in->dbuff_smmu, idx - 1);
		if (ret)
			goto fail_buff;
	}

	IPA_WIGIG_DBG("exit\n");

	return 0;

fail_buff:
	ipa_wigig_clean_pipe_info(IPA_CLIENT_WIGIG_PROD_IDX);
	return ret;
}

static int ipa_wigig_get_rx_smmu_info(
	struct ipa_wigig_pipe_setup_info_smmu **pipe_smmu,
	struct ipa_wigig_rx_pipe_data_buffer_info_smmu **dbuff_smmu)
{
	int ret;

	ret = ipa_wigig_get_pipe_smmu_info(pipe_smmu,
		IPA_CLIENT_WIGIG_PROD_IDX);
	if (ret)
		return ret;

	ret = ipa_wigig_get_rx_buff_smmu_info(dbuff_smmu);
	if (ret)
		return ret;

	return 0;
}

static int ipa_wigig_get_tx_smmu_info(
	struct ipa_wigig_pipe_setup_info_smmu **pipe_smmu,
	struct ipa_wigig_tx_pipe_data_buffer_info_smmu **dbuff_smmu,
	enum ipa_client_type client)
{
	unsigned int idx;
	int ret;

	ret = ipa_wigig_client_to_idx(client, &idx);
	if (ret)
		return ret;

	ret = ipa_wigig_get_pipe_smmu_info(pipe_smmu, idx);
	if (ret)
		return ret;

	ret = ipa_wigig_get_tx_buff_smmu_info(dbuff_smmu, idx - 1);
	if (ret)
		return ret;

	return 0;
}

static int ipa_wigig_clean_smmu_info(enum ipa_client_type client)
{
	int ret;

	if (client == IPA_CLIENT_WIGIG_PROD) {
		ret = ipa_wigig_clean_pipe_info(IPA_CLIENT_WIGIG_PROD_IDX);
		if (ret)
			return ret;
		if (!ipa_wigig_ctx->shared_cb)
			ipa_wigig_clean_rx_buff_smmu_info();
	} else {
		unsigned int idx;

		ret = ipa_wigig_client_to_idx(client, &idx);
		if (ret)
			return ret;

		ret = ipa_wigig_clean_pipe_info(idx);
		if (ret)
			return ret;

		if (!ipa_wigig_ctx->shared_cb) {
			ret = ipa_wigig_clean_tx_buff_smmu_info(idx - 1);
			if (ret) {
				IPA_WIGIG_ERR(
					"cleaned tx pipe info but wasn't able to clean buff info, client %d\n"
					, client);
				WARN_ON(1);
				return ret;
			}
		}
	}

	return 0;
}
int ipa_wigig_conn_rx_pipe_smmu(
	struct ipa_wigig_conn_rx_in_params_smmu *in,
	struct ipa_wigig_conn_out_params *out)
{
	int ret;
	struct ipa_pm_register_params pm_params;

	IPA_WIGIG_DBG("\n");

	if (!in || !out) {
		IPA_WIGIG_ERR("empty parameters. in=%pK out=%pK\n", in, out);
		return -EINVAL;
	}

	if (!ipa_wigig_ctx) {
		IPA_WIGIG_ERR("wigig ctx is not initialized\n");
		return -EPERM;
	}

	ret = ipa_uc_state_check();
	if (ret) {
		IPA_WIGIG_ERR("uC not ready\n");
		return ret;
	}

	if (ipa_wigig_init_smmu_params())
		return -EINVAL;

	if (!ipa_wigig_ctx->smmu_en) {
		IPA_WIGIG_ERR("IPA SMMU is disabled, wrong API used\n");
		return -EFAULT;
	}

	memset(&pm_params, 0, sizeof(pm_params));
	pm_params.name = "wigig";
	pm_params.callback = ipa_wigig_pm_cb;
	pm_params.user_data = NULL;
	pm_params.group = IPA_PM_GROUP_DEFAULT;
	if (ipa_pm_register(&pm_params, &ipa_wigig_ctx->ipa_pm_hdl)) {
		IPA_WIGIG_ERR("fail to register ipa pm\n");
		ret = -EFAULT;
		goto fail_pm;
	}

	ret = ipa_wigig_uc_msi_init(true,
		ipa_wigig_ctx->periph_baddr_pa,
		ipa_wigig_ctx->pseudo_cause_pa,
		ipa_wigig_ctx->int_gen_tx_pa,
		ipa_wigig_ctx->int_gen_rx_pa,
		ipa_wigig_ctx->dma_ep_misc_pa);
	if (ret) {
		IPA_WIGIG_ERR("failed configuring msi regs at uC\n");
		ret = -EFAULT;
		goto fail_msi;
	}

	if (ipa_conn_wigig_rx_pipe_i(in, out, &ipa_wigig_ctx->parent)) {
		IPA_WIGIG_ERR("fail to connect rx pipe\n");
		ret = -EFAULT;
		goto fail_connect_pipe;
	}

	if (ipa_wigig_ctx->parent)
		ipa_wigig_init_debugfs(ipa_wigig_ctx->parent);

	if (ipa_wigig_store_rx_smmu_info(in)) {
		IPA_WIGIG_ERR("fail to store smmu data for rx pipe\n");
		ret = -EFAULT;
		goto fail_smmu_store;
	}

	ipa_wigig_ctx->tx_notify = in->notify;
	ipa_wigig_ctx->priv = in->priv;

	ipa_wigig_ctx->conn_pipes |=
		ipa_wigig_pipe_to_bit_val(IPA_CLIENT_WIGIG_PROD);

	IPA_WIGIG_DBG("exit\n");

	return 0;

fail_smmu_store:
	ipa_disconn_wigig_pipe_i(IPA_CLIENT_WIGIG_PROD,
		&in->pipe_smmu,
		&in->dbuff_smmu);
fail_connect_pipe:
	ipa_wigig_uc_msi_init(false,
		ipa_wigig_ctx->periph_baddr_pa,
		ipa_wigig_ctx->pseudo_cause_pa,
		ipa_wigig_ctx->int_gen_tx_pa,
		ipa_wigig_ctx->int_gen_rx_pa,
		ipa_wigig_ctx->dma_ep_misc_pa);
fail_msi:
	ipa_pm_deregister(ipa_wigig_ctx->ipa_pm_hdl);
fail_pm:
	return ret;
}
EXPORT_SYMBOL(ipa_wigig_conn_rx_pipe_smmu);

int ipa_wigig_set_perf_profile(u32 max_supported_bw_mbps)
{
	IPA_WIGIG_DBG("setting throughput to %d\n", max_supported_bw_mbps);

	if (!ipa_wigig_ctx) {
		IPA_WIGIG_ERR("wigig ctx is not initialized\n");
		return -EPERM;
	}

	IPA_WIGIG_DBG("ipa_pm handle %d\n", ipa_wigig_ctx->ipa_pm_hdl);
	if (ipa_pm_set_throughput(ipa_wigig_ctx->ipa_pm_hdl,
		max_supported_bw_mbps)) {
		IPA_WIGIG_ERR("fail to setup pm perf profile\n");
		return -EFAULT;
	}
	IPA_WIGIG_DBG("exit\n");

	return 0;
}
EXPORT_SYMBOL(ipa_wigig_set_perf_profile);

static int ipa_wigig_store_client_mac(enum ipa_client_type client,
	const char *mac)
{
	unsigned int idx;

	if (ipa_wigig_client_to_idx(client, &idx)) {
		IPA_WIGIG_ERR("couldn't acquire idx\n");
		return -EFAULT;
	}
	memcpy(ipa_wigig_ctx->clients_mac[idx - 1], mac, IPA_MAC_ADDR_SIZE);
	return 0;
}

static int ipa_wigig_get_client_mac(enum ipa_client_type client, char *mac)
{
	unsigned int idx;

	if (ipa_wigig_client_to_idx(client, &idx)) {
		IPA_WIGIG_ERR("couldn't acquire idx\n");
		return -EFAULT;
	}
	memcpy(mac, ipa_wigig_ctx->clients_mac[idx - 1], IPA_MAC_ADDR_SIZE);
	return 0;
}

static int ipa_wigig_clean_client_mac(enum ipa_client_type client)
{
	char zero_mac[IPA_MAC_ADDR_SIZE] = { 0 };

	return ipa_wigig_store_client_mac(client, zero_mac);
}

int ipa_wigig_conn_client(struct ipa_wigig_conn_tx_in_params *in,
	struct ipa_wigig_conn_out_params *out)
{
	char dev_name[IPA_RESOURCE_NAME_MAX];
	unsigned int idx;

	IPA_WIGIG_DBG("\n");

	if (!in || !out) {
		IPA_WIGIG_ERR("empty parameters. in=%pK out=%pK\n", in, out);
		return -EINVAL;
	}

	if (!ipa_wigig_ctx) {
		IPA_WIGIG_ERR("wigig ctx is not initialized\n");
		return -EPERM;
	}

	if (!(ipa_wigig_ctx->conn_pipes &
		ipa_wigig_pipe_to_bit_val(IPA_CLIENT_WIGIG_PROD))) {
		IPA_WIGIG_ERR(
			"must connect rx pipe before connecting any client\n"
		);
		return -EINVAL;
	}

	if (ipa_wigig_ctx->smmu_en) {
		IPA_WIGIG_ERR("IPA SMMU is enabled, wrong API used\n");
		return -EFAULT;
	}

	if (ipa_uc_state_check()) {
		IPA_WIGIG_ERR("uC not ready\n");
		return -EFAULT;
	}

	if (ipa_wigig_get_devname(dev_name)) {
		IPA_WIGIG_ERR("couldn't get dev name\n");
		return -EFAULT;
	}

	if (ipa_conn_wigig_client_i(in, out, ipa_wigig_ctx->tx_notify,
		ipa_wigig_ctx->priv)) {
		IPA_WIGIG_ERR(
			"fail to connect client. MAC [%X][%X][%X][%X][%X][%X]\n"
		, in->client_mac[0], in->client_mac[1], in->client_mac[2]
		, in->client_mac[3], in->client_mac[4], in->client_mac[5]);
		return -EFAULT;
	}

	if (ipa_wigig_client_to_idx(out->client, &idx)) {
		IPA_WIGIG_ERR("couldn't acquire idx\n");
		goto fail_convert_client_to_idx;
	}

	ipa_wigig_store_pipe_info(&in->pipe, idx);

	if (ipa_wigig_send_msg(WIGIG_CLIENT_CONNECT,
		dev_name,
		in->client_mac, out->client, false)) {
		IPA_WIGIG_ERR("couldn't send msg to IPACM\n");
		goto fail_sendmsg;
	}

	/* update connected clients */
	ipa_wigig_ctx->conn_pipes |=
		ipa_wigig_pipe_to_bit_val(out->client);

	ipa_wigig_store_client_mac(out->client, in->client_mac);

	IPA_WIGIG_DBG("exit\n");
	return 0;

fail_sendmsg:
	ipa_wigig_clean_pipe_info(idx);
fail_convert_client_to_idx:
	ipa_disconn_wigig_pipe_i(out->client, NULL, NULL);
	return -EINVAL;
}
EXPORT_SYMBOL(ipa_wigig_conn_client);

int ipa_wigig_conn_client_smmu(
	struct ipa_wigig_conn_tx_in_params_smmu *in,
	struct ipa_wigig_conn_out_params *out)
{
	char netdev_name[IPA_RESOURCE_NAME_MAX];
	int ret;

	IPA_WIGIG_DBG("\n");

	if (!in || !out) {
		IPA_WIGIG_ERR("empty parameters. in=%pK out=%pK\n", in, out);
		return -EINVAL;
	}

	if (!ipa_wigig_ctx) {
		IPA_WIGIG_ERR("wigig ctx is not initialized\n");
		return -EPERM;
	}

	if (!(ipa_wigig_ctx->conn_pipes &
		ipa_wigig_pipe_to_bit_val(IPA_CLIENT_WIGIG_PROD))) {
		IPA_WIGIG_ERR(
			"must connect rx pipe before connecting any client\n"
		);
		return -EINVAL;
	}

	if (!ipa_wigig_ctx->smmu_en) {
		IPA_WIGIG_ERR("IPA SMMU is disabled, wrong API used\n");
		return -EFAULT;
	}

	ret = ipa_uc_state_check();
	if (ret) {
		IPA_WIGIG_ERR("uC not ready\n");
		return ret;
	}

	if (ipa_wigig_get_devname(netdev_name)) {
		IPA_WIGIG_ERR("couldn't get dev name\n");
		return -EFAULT;
	}

	if (ipa_conn_wigig_client_i(in, out, ipa_wigig_ctx->tx_notify,
		ipa_wigig_ctx->priv)) {
		IPA_WIGIG_ERR(
			"fail to connect client. MAC [%X][%X][%X][%X][%X][%X]\n"
			, in->client_mac[0], in->client_mac[1]
			, in->client_mac[2], in->client_mac[3]
			, in->client_mac[4], in->client_mac[5]);
		return -EFAULT;
	}

	if (ipa_wigig_send_msg(WIGIG_CLIENT_CONNECT,
		netdev_name,
		in->client_mac, out->client, false)) {
		IPA_WIGIG_ERR("couldn't send msg to IPACM\n");
		ret = -EFAULT;
		goto fail_sendmsg;
	}

	ret = ipa_wigig_store_client_smmu_info(in, out->client);
	if (ret)
		goto fail_smmu;

	/* update connected clients */
	ipa_wigig_ctx->conn_pipes |=
		ipa_wigig_pipe_to_bit_val(out->client);

	ipa_wigig_store_client_mac(out->client, in->client_mac);

	IPA_WIGIG_DBG("exit\n");
	return 0;

fail_smmu:
	/*
	 * wigig clients are disconnected with legacy message since there is
	 * no need to send ep, client MAC is sufficient for disconnect
	 */
	ipa_wigig_send_wlan_msg(WLAN_CLIENT_DISCONNECT, netdev_name,
		in->client_mac);
fail_sendmsg:
	ipa_disconn_wigig_pipe_i(out->client, &in->pipe_smmu, &in->dbuff_smmu);
	return ret;
}
EXPORT_SYMBOL(ipa_wigig_conn_client_smmu);

static inline int ipa_wigig_validate_client_type(enum ipa_client_type client)
{
	switch (client) {
	case IPA_CLIENT_WIGIG_PROD:
	case IPA_CLIENT_WIGIG1_CONS:
	case IPA_CLIENT_WIGIG2_CONS:
	case IPA_CLIENT_WIGIG3_CONS:
	case IPA_CLIENT_WIGIG4_CONS:
		break;
	default:
		IPA_WIGIG_ERR_RL("invalid client type %d\n", client);
		return -EINVAL;
	}

	return 0;
}

int ipa_wigig_disconn_pipe(enum ipa_client_type client)
{
	int ret;
	char dev_name[IPA_RESOURCE_NAME_MAX];
	char client_mac[IPA_MAC_ADDR_SIZE];

	IPA_WIGIG_DBG("\n");

	ret = ipa_wigig_validate_client_type(client);
	if (ret)
		return ret;

	if (client != IPA_CLIENT_WIGIG_PROD) {
		if (ipa_wigig_get_devname(dev_name)) {
			IPA_WIGIG_ERR("couldn't get dev name\n");
			return -EFAULT;
		}

		if (ipa_wigig_get_client_mac(client, client_mac)) {
			IPA_WIGIG_ERR("couldn't get client mac\n");
			return -EFAULT;
		}
	}

	IPA_WIGIG_DBG("disconnecting ipa_client_type %d\n", client);

	if (ipa_wigig_is_smmu_enabled()) {
		struct ipa_wigig_pipe_setup_info_smmu *pipe_smmu;
		struct ipa_wigig_rx_pipe_data_buffer_info_smmu *rx_dbuff_smmu;
		struct ipa_wigig_tx_pipe_data_buffer_info_smmu *tx_dbuff_smmu;

		if (client == IPA_CLIENT_WIGIG_PROD) {
			ret = ipa_wigig_get_rx_smmu_info(&pipe_smmu,
				&rx_dbuff_smmu);
			if (ret)
				return ret;

			ret = ipa_disconn_wigig_pipe_i(client,
				pipe_smmu,
				rx_dbuff_smmu);
		} else {
			ret = ipa_wigig_get_tx_smmu_info(&pipe_smmu,
				&tx_dbuff_smmu, client);
			if (ret)
				return ret;

			ret = ipa_disconn_wigig_pipe_i(client,
				pipe_smmu,
				tx_dbuff_smmu);
		}

	} else {
		ret = ipa_disconn_wigig_pipe_i(client, NULL, NULL);
	}

	if (ret) {
		IPA_WIGIG_ERR("couldn't disconnect client %d\n", client);
		return ret;
	}

	/* RX will be disconnected last, deinit uC msi config */
	if (client == IPA_CLIENT_WIGIG_PROD) {
		IPA_WIGIG_DBG("Rx pipe disconnected, deIniting uc\n");
		ret = ipa_wigig_uc_msi_init(false,
			ipa_wigig_ctx->periph_baddr_pa,
			ipa_wigig_ctx->pseudo_cause_pa,
			ipa_wigig_ctx->int_gen_tx_pa,
			ipa_wigig_ctx->int_gen_rx_pa,
			ipa_wigig_ctx->dma_ep_misc_pa);
		if (ret) {
			IPA_WIGIG_ERR("failed unmapping msi regs\n");
			WARN_ON(1);
		}

		ret = ipa_pm_deregister(ipa_wigig_ctx->ipa_pm_hdl);
		if (ret) {
			IPA_WIGIG_ERR("failed dereg pm\n");
			WARN_ON(1);
		}

		ipa_wigig_ctx->conn_pipes &=
			~ipa_wigig_pipe_to_bit_val(IPA_CLIENT_WIGIG_PROD);
		WARN_ON(ipa_wigig_ctx->conn_pipes);
	} else {
		/*
		 * wigig clients are disconnected with legacy message since
		 * there is no need to send ep, client MAC is sufficient for
		 * disconnect.
		 */
		ipa_wigig_send_wlan_msg(WLAN_CLIENT_DISCONNECT, dev_name,
			client_mac);
		ipa_wigig_clean_client_mac(client);

		ipa_wigig_ctx->conn_pipes &=
			~ipa_wigig_pipe_to_bit_val(client);
	}
	if (ipa_wigig_is_smmu_enabled())
		ipa_wigig_clean_smmu_info(client);

	IPA_WIGIG_DBG("exit\n");
	return 0;
}
EXPORT_SYMBOL(ipa_wigig_disconn_pipe);

int ipa_wigig_enable_pipe(enum ipa_client_type client)
{
	int ret;

	IPA_WIGIG_DBG("\n");

	ret = ipa_wigig_validate_client_type(client);
	if (ret)
		return ret;

	IPA_WIGIG_DBG("enabling pipe %d\n", client);

	ret = ipa_enable_wigig_pipe_i(client);
	if (ret)
		return ret;

	/* do only when Rx pipe is enabled */
	if (client == IPA_CLIENT_WIGIG_PROD) {
		ret = ipa_pm_activate_sync(ipa_wigig_ctx->ipa_pm_hdl);
		if (ret) {
			IPA_WIGIG_ERR("fail to activate ipa pm\n");
			ret = -EFAULT;
			goto fail_pm_active;
		}
	}

	IPA_WIGIG_DBG("exit\n");
	return 0;

fail_pm_active:
	ipa_disable_wigig_pipe_i(client);
	return ret;
}
EXPORT_SYMBOL(ipa_wigig_enable_pipe);

int ipa_wigig_disable_pipe(enum ipa_client_type client)
{
	int ret;

	IPA_WIGIG_DBG("\n");

	ret = ipa_wigig_validate_client_type(client);
	if (ret)
		return ret;

	ret = ipa_disable_wigig_pipe_i(client);
	if (ret)
		return ret;

	/* do only when Rx pipe is disabled */
	if (client == IPA_CLIENT_WIGIG_PROD) {
		ret = ipa_pm_deactivate_sync(ipa_wigig_ctx->ipa_pm_hdl);
		if (ret) {
			IPA_WIGIG_ERR("fail to deactivate ipa pm\n");
			return -EFAULT;
		}
	}

	IPA_WIGIG_DBG("exit\n");
	return 0;
}
EXPORT_SYMBOL(ipa_wigig_disable_pipe);

int ipa_wigig_tx_dp(enum ipa_client_type dst, struct sk_buff *skb)
{
	int ret;

	IPA_WIGIG_DBG_LOW("\n");

	ret = ipa_wigig_validate_client_type(dst);
	if (unlikely(ret))
		return ret;

	ret = ipa_tx_dp(dst, skb, NULL);
	if (unlikely(ret))
		return ret;

	IPA_WIGIG_DBG_LOW("exit\n");
	return 0;
}
EXPORT_SYMBOL(ipa_wigig_tx_dp);


#ifdef CONFIG_DEBUG_FS
#define IPA_MAX_MSG_LEN 4096

static ssize_t ipa_wigig_read_conn_clients(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int i;
	int nbytes = 0;
	u8 pipe_connected;
	char *dbg_buff;
	ssize_t ret;

	dbg_buff = kzalloc(IPA_MAX_MSG_LEN, GFP_KERNEL);
	if (!dbg_buff)
		return -ENOMEM;

	if (!ipa_wigig_ctx) {
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"IPA WIGIG not initialized\n");
		goto finish;
	}

	if (!ipa_wigig_ctx->conn_pipes) {
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"no WIGIG pipes connected\n");
		goto finish;
	}

	for (i = 0; i < IPA_WIGIG_MAX_PIPES; i++) {
		pipe_connected = (ipa_wigig_ctx->conn_pipes & (0x1 << i));
		switch (i) {
		case 0:
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"IPA_CLIENT_WIGIG_PROD");
			break;
		case 1:
		case 2:
		case 3:
		case 4:
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"IPA_CLIENT_WIGIG%d_CONS",
				i);
			break;
		default:
			IPA_WIGIG_ERR("invalid pipe %d\n", i);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"invalid pipe %d",
				i);
			break;
		}
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			" %s connected\n", pipe_connected ? "is" : "not");
	}

finish:
	ret = simple_read_from_buffer(
		ubuf, count, ppos, dbg_buff, nbytes);
	kfree(dbg_buff);
	return ret;
}

static ssize_t ipa_wigig_read_smmu_status(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes = 0;
	char *dbg_buff;
	ssize_t ret;

	dbg_buff = kzalloc(IPA_MAX_MSG_LEN, GFP_KERNEL);
	if (!dbg_buff)
		return -ENOMEM;

	if (!ipa_wigig_ctx) {
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"IPA WIGIG not initialized\n");
		goto finish;
	}

	if (ipa_wigig_ctx->smmu_en) {
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"SMMU enabled\n");

		if (ipa_wigig_ctx->shared_cb) {
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"CB shared\n");
		} else {
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"CB not shared\n");
		}
	} else {
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"SMMU in S1 bypass\n");
	}
finish:
	ret = simple_read_from_buffer(
		ubuf, count, ppos, dbg_buff, nbytes);
	kfree(dbg_buff);
	return ret;
}
static const struct file_operations ipa_wigig_conn_clients_ops = {
	.read = ipa_wigig_read_conn_clients,
};

static const struct file_operations ipa_wigig_smmu_ops = {
	.read = ipa_wigig_read_smmu_status,
};

static inline void ipa_wigig_deinit_debugfs(void)
{
	debugfs_remove(ipa_wigig_ctx->dent_conn_clients);
	debugfs_remove(ipa_wigig_ctx->dent_smmu);
}

static int ipa_wigig_init_debugfs(struct dentry *parent)
{
	const mode_t read_only_mode = 0444;

	ipa_wigig_ctx->dent_conn_clients =
		debugfs_create_file("conn_clients", read_only_mode, parent,
				NULL, &ipa_wigig_conn_clients_ops);
	if (IS_ERR_OR_NULL(ipa_wigig_ctx->dent_conn_clients)) {
		IPA_WIGIG_ERR("fail to create file %s\n", "conn_clients");
		goto fail_conn_clients;
	}

	ipa_wigig_ctx->dent_smmu =
		debugfs_create_file("smmu", read_only_mode, parent, NULL,
				&ipa_wigig_smmu_ops);
	if (IS_ERR_OR_NULL(ipa_wigig_ctx->dent_smmu)) {
		IPA_WIGIG_ERR("fail to create file %s\n", "smmu");
		goto fail_smmu;
	}

	return 0;
fail_smmu:
	debugfs_remove(ipa_wigig_ctx->dent_conn_clients);
fail_conn_clients:
	return -EFAULT;
}
#endif
