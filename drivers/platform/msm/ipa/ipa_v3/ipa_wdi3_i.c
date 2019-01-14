// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include "ipa_i.h"
#include <linux/ipa_wdi3.h>

int ipa3_conn_wdi3_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out,
	ipa_wdi_meter_notifier_cb wdi_notify)
{
	IPAERR("wdi3 over uc offload not supported");
	WARN_ON(1);

	return -EFAULT;
}

int ipa3_disconn_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx)
{
	IPAERR("wdi3 over uc offload not supported");
	WARN_ON(1);

	return -EFAULT;
}

int ipa3_enable_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx)
{
	IPAERR("wdi3 over uc offload not supported");
	WARN_ON(1);

	return -EFAULT;
}

int ipa3_disable_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx)
{
	IPAERR("wdi3 over uc offload not supported");
	WARN_ON(1);

	return -EFAULT;
}
