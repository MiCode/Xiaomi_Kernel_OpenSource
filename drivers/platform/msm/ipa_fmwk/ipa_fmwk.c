// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/ipa.h>
#include <linux/ipa_uc_offload.h>
#include <linux/ipa_mhi.h>
#include <linux/ipa_wigig.h>
#include <linux/ipa_wdi3.h>
#include <linux/ipa_usb.h>
#include <linux/ipa_odu_bridge.h>
#include <linux/ipa_fmwk.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/list.h>

#define IPA_FMWK_DISPATCH_RETURN(api, p...) \
	do { \
		if (!ipa_fmwk_ctx) { \
			pr_err("%s:%d IPA framework was not inited\n", \
				__func__, __LINE__); \
			ret = -EPERM; \
		} \
		else { \
			if (ipa_fmwk_ctx->api) { \
				ret = ipa_fmwk_ctx->api(p); \
			} else { \
				WARN(1, \
					"%s was not registered on ipa_fmwk\n", \
						__func__); \
				ret = -EPERM; \
			} \
		} \
	} while (0)

/**
 * struct ipa_ready_cb_info - A list of all the registrations
 *  for an indication of IPA driver readiness
 *
 * @link: linked list link
 * @ready_cb: callback
 * @user_data: User data
 *
 */
struct ipa_ready_cb_info {
	struct list_head link;
	ipa_ready_cb ready_cb;
	void *user_data;
};

struct ipa_fmwk_contex {
	bool ipa_ready;
	struct list_head ipa_ready_cb_list;
	struct mutex lock;

	/* ipa core driver APIs */
	int (*ipa_tx_dp)(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

	/* ipa_usb APIs */
	int (*ipa_usb_init_teth_prot)(enum ipa_usb_teth_prot teth_prot,
		struct ipa_usb_teth_params *teth_params,
		int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event,
			void *),
		void *user_data);

	int (*ipa_usb_xdci_connect)(
		struct ipa_usb_xdci_chan_params *ul_chan_params,
		struct ipa_usb_xdci_chan_params *dl_chan_params,
		struct ipa_req_chan_out_params *ul_out_params,
		struct ipa_req_chan_out_params *dl_out_params,
		struct ipa_usb_xdci_connect_params *connect_params);

	int (*ipa_usb_xdci_disconnect)(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
		enum ipa_usb_teth_prot teth_prot);

	int (*ipa_usb_deinit_teth_prot)(enum ipa_usb_teth_prot teth_prot);

	int (*ipa_usb_xdci_suspend)(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
		enum ipa_usb_teth_prot teth_prot,
		bool with_remote_wakeup);

	int (*ipa_usb_xdci_resume)(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
		enum ipa_usb_teth_prot teth_prot);
};

static struct ipa_fmwk_contex *ipa_fmwk_ctx;

static inline void ipa_trigger_ipa_ready_cbs(void)
{
	struct ipa_ready_cb_info *info;
	struct ipa_ready_cb_info *next;

	/* Call all the CBs */
	list_for_each_entry_safe(info, next,
		&ipa_fmwk_ctx->ipa_ready_cb_list, link) {
		if (info->ready_cb)
			info->ready_cb(info->user_data);

		list_del(&info->link);
		kfree(info);
	}
}

/* registration API for IPA core module */
int ipa_fmwk_register_ipa(const struct ipa_core_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	mutex_lock(&ipa_fmwk_ctx->lock);
	if (ipa_fmwk_ctx->ipa_ready) {
		pr_err("ipa core driver API were already registered\n");
		mutex_unlock(&ipa_fmwk_ctx->lock);
		return -EPERM;
	}

	ipa_fmwk_ctx->ipa_tx_dp = in->ipa_tx_dp;

	ipa_fmwk_ctx->ipa_ready = true;
	ipa_trigger_ipa_ready_cbs();
	mutex_unlock(&ipa_fmwk_ctx->lock);

	pr_info("IPA driver is now in ready state\n");
	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_ipa);

int ipa_register_ipa_ready_cb(void(*ipa_ready_cb)(void *user_data),
	void *user_data)
{
	struct ipa_ready_cb_info *cb_info = NULL;

	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	mutex_lock(&ipa_fmwk_ctx->lock);
	if (ipa_fmwk_ctx->ipa_ready) {
		pr_debug("IPA driver finished initialization already\n");
		mutex_unlock(&ipa_fmwk_ctx->lock);
		return -EEXIST;
	}

	cb_info = kmalloc(sizeof(struct ipa_ready_cb_info), GFP_KERNEL);
	if (!cb_info) {
		mutex_unlock(&ipa_fmwk_ctx->lock);
		return -ENOMEM;
	}

	cb_info->ready_cb = ipa_ready_cb;
	cb_info->user_data = user_data;

	list_add_tail(&cb_info->link, &ipa_fmwk_ctx->ipa_ready_cb_list);
	mutex_unlock(&ipa_fmwk_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(ipa_register_ipa_ready_cb);

/* ipa core driver API wrappers*/

int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
	struct ipa_tx_meta *metadata)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_tx_dp,
		dst, skb, metadata);

	return ret;
}
EXPORT_SYMBOL(ipa_tx_dp);

/* registration API for IPA usb module */
int ipa_fmwk_register_ipa_usb(const struct ipa_usb_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	if (ipa_fmwk_ctx->ipa_usb_init_teth_prot ||
		ipa_fmwk_ctx->ipa_usb_xdci_connect ||
		ipa_fmwk_ctx->ipa_usb_xdci_disconnect ||
		ipa_fmwk_ctx->ipa_usb_deinit_teth_prot ||
		ipa_fmwk_ctx->ipa_usb_xdci_suspend ||
		ipa_fmwk_ctx->ipa_usb_xdci_resume) {
		pr_err("ipa_usb APIs were already initialized\n");
		return -EPERM;
	}
	ipa_fmwk_ctx->ipa_usb_init_teth_prot = in->ipa_usb_init_teth_prot;
	ipa_fmwk_ctx->ipa_usb_xdci_connect = in->ipa_usb_xdci_connect;
	ipa_fmwk_ctx->ipa_usb_xdci_disconnect = in->ipa_usb_xdci_disconnect;
	ipa_fmwk_ctx->ipa_usb_deinit_teth_prot = in->ipa_usb_deinit_teth_prot;
	ipa_fmwk_ctx->ipa_usb_xdci_suspend = in->ipa_usb_xdci_suspend;
	ipa_fmwk_ctx->ipa_usb_xdci_resume = in->ipa_usb_xdci_resume;

	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_ipa_usb);

/* ipa_usb API wrappers*/
int ipa_usb_init_teth_prot(enum ipa_usb_teth_prot teth_prot,
	struct ipa_usb_teth_params *teth_params,
	int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event,
		void *),
	void *user_data)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_init_teth_prot,
		teth_prot, teth_params, ipa_usb_notify_cb, user_data);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_init_teth_prot);

int ipa_usb_xdci_connect(struct ipa_usb_xdci_chan_params *ul_chan_params,
	struct ipa_usb_xdci_chan_params *dl_chan_params,
	struct ipa_req_chan_out_params *ul_out_params,
	struct ipa_req_chan_out_params *dl_out_params,
	struct ipa_usb_xdci_connect_params *connect_params)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_xdci_connect,
		ul_chan_params, dl_chan_params, ul_out_params,
		dl_out_params, connect_params);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_connect);

int ipa_usb_xdci_disconnect(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_xdci_disconnect,
		ul_clnt_hdl, dl_clnt_hdl, teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_disconnect);

int ipa_usb_deinit_teth_prot(enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_deinit_teth_prot,
		teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_deinit_teth_prot);

int ipa_usb_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot,
	bool with_remote_wakeup)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_xdci_suspend,
		ul_clnt_hdl, dl_clnt_hdl, teth_prot, with_remote_wakeup);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_suspend);

int ipa_usb_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_xdci_resume,
		ul_clnt_hdl, dl_clnt_hdl, teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_resume);

static int __init ipa_fmwk_init(void)
{
	pr_info("IPA framework init\n");

	ipa_fmwk_ctx = kzalloc(sizeof(struct ipa_fmwk_contex), GFP_KERNEL);
	if (ipa_fmwk_ctx == NULL)
		return -ENOMEM;

	INIT_LIST_HEAD(&ipa_fmwk_ctx->ipa_ready_cb_list);
	mutex_init(&ipa_fmwk_ctx->lock);

	return 0;
}
subsys_initcall(ipa_fmwk_init);

static void __exit ipa_fmwk_exit(void)
{
	struct ipa_ready_cb_info *info;
	struct ipa_ready_cb_info *next;

	pr_debug("IPA framework exit\n");
	list_for_each_entry_safe(info, next,
		&ipa_fmwk_ctx->ipa_ready_cb_list, link) {
		list_del(&info->link);
		kfree(info);
	}
	kfree(ipa_fmwk_ctx);
	ipa_fmwk_ctx = NULL;
}
module_exit(ipa_fmwk_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW framework");
