/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include <linux/ethtool.h>
#include <linux/rtnetlink.h>

#include "ipa_eth_i.h"

#define IPA_ETH_MIN_BW_MBPS 1
#define IPA_ETH_MAX_BW_MBPS 100000

static void ipa_eth_pm_callback(void *arg, enum ipa_pm_cb_event event)
{
	struct ipa_eth_device *eth_dev = (struct ipa_eth_device *)arg;

	ipa_eth_dev_log(eth_dev, "Received PM event 0x%x", event);
}

/**
 * ipa_eth_pm_register() - Register offload device with IPA PM
 * @eth_dev: Offload device to register with PM
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_pm_register(struct ipa_eth_device *eth_dev)
{
	int rc = 0;
	char name[IPA_PM_MAX_EX_CL];
	struct ipa_pm_register_params pm_params;

	if (!ipa_pm_is_used())
		return 0;

	if (eth_dev->pm_handle != IPA_PM_MAX_CLIENTS)
		return -EEXIST;

	snprintf(name, sizeof(name), IPA_ETH_SUBSYS ":%s",
		 eth_dev->net_dev->name);

	memset(&pm_params, 0, sizeof(pm_params));
	pm_params.name = name;
	pm_params.callback = ipa_eth_pm_callback;
	pm_params.user_data = eth_dev;
	pm_params.group = IPA_PM_GROUP_DEFAULT;
	pm_params.skip_clk_vote = false;

	rc = ipa_pm_register(&pm_params, &eth_dev->pm_handle);
	if (rc)
		eth_dev->pm_handle = IPA_PM_MAX_CLIENTS;

	return rc;
}

/**
 * ipa_eth_pm_unregister() - Unregister offload device from IPA PM
 * @eth_dev: Offload device to unregister
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_pm_unregister(struct ipa_eth_device *eth_dev)
{
	int rc = 0;

	if (!ipa_pm_is_used())
		return 0;

	rc = ipa_pm_deregister(eth_dev->pm_handle);
	if (!rc)
		eth_dev->pm_handle = IPA_PM_MAX_CLIENTS;

	return rc;
}

int ipa_eth_pm_activate(struct ipa_eth_device *eth_dev)
{
	if (!ipa_pm_is_used())
		return 0;

	return ipa_pm_activate_sync(eth_dev->pm_handle);
}

int ipa_eth_pm_deactivate(struct ipa_eth_device *eth_dev)
{
	if (!ipa_pm_is_used())
		return 0;

	return ipa_pm_deactivate_sync(eth_dev->pm_handle);
}

static u32 __fetch_ethtool_link_speed(struct ipa_eth_device *eth_dev)
{
	int rc;
	struct ethtool_link_ksettings link_ksettings;

	rtnl_lock();
	rc = __ethtool_get_link_ksettings(eth_dev->net_dev, &link_ksettings);
	rtnl_unlock();

	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to obtain link settings via ethtool");
		return 0;
	}

	return link_ksettings.base.speed;
}

int ipa_eth_pm_vote_bw(struct ipa_eth_device *eth_dev)
{
	u32 link_speed;
	int throughput;

	if (!ipa_pm_is_used())
		return 0;

	link_speed = __fetch_ethtool_link_speed(eth_dev);
	throughput = clamp_val(link_speed,
				IPA_ETH_MIN_BW_MBPS, IPA_ETH_MAX_BW_MBPS);

	ipa_eth_dev_log(eth_dev, "Link speed is %u, setting throughput as %d",
			link_speed, throughput);

	return ipa_pm_set_throughput(eth_dev->pm_handle, throughput);
}
