/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
