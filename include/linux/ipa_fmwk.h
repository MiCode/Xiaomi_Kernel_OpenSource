/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 - 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_FMWK_H_
#define _IPA_FMWK_H_

#include <linux/types.h>
#include <linux/ipa_usb.h>

struct ipa_core_data {
	int (*ipa_tx_dp)(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);
};

struct ipa_usb_data {
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

#if IS_ENABLED(CONFIG_IPA3)

int ipa_fmwk_register_ipa(const struct ipa_core_data *in);

int ipa_fmwk_register_ipa_usb(const struct ipa_usb_data *in);

#else /* IS_ENABLED(CONFIG_IPA3) */

int ipa_fmwk_register_ipa(const struct ipa_core_data *in)
{
	return -EPERM;
}
int ipa_fmwk_register_ipa_usb(const struct ipa_usb_data *in)
{
	return -EPERM;
}

#endif /* IS_ENABLED(CONFIG_IPA3) */

#endif /* _IPA_FMWK_H_ */
