/******************************************************************************
 *
 * This file is provided under the GPLv2 license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * Author: Johannes Berg <johannes.berg@intel.com>
 *
 *****************************************************************************/
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <net/cfg80211.h>
#include "core.h"
#include "rdev-ops.h"

static int cfg80211_android_p2pdev_open(struct net_device *dev)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wdev->wiphy);
	int err;

	if (!rdev->ops->start_p2p_device)
		return -EOPNOTSUPP;

	if (wdev->iftype != NL80211_IFTYPE_P2P_DEVICE)
		return -EOPNOTSUPP;

	if (wdev->p2p_started)
		return 0;

	err = rdev_start_p2p_device(rdev, wdev);
	if (err)
		return err;

	wdev->p2p_started = true;
	rdev->opencount++;

	return 0;
}

static int cfg80211_android_p2pdev_stop(struct net_device *dev)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wdev->wiphy);

	if (!wdev->p2p_started)
		return 0;

	cfg80211_stop_p2p_device(rdev, wdev);

	return 0;
}

static netdev_tx_t cfg80211_android_p2pdev_start_xmit(struct sk_buff *skb,
						      struct net_device *dev)
{
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

static const struct net_device_ops cfg80211_android_p2pdev_ops = {
	.ndo_open = cfg80211_android_p2pdev_open,
	.ndo_stop = cfg80211_android_p2pdev_stop,
	.ndo_start_xmit = cfg80211_android_p2pdev_start_xmit,
};

static void cfg80211_android_p2pdev_setup(struct net_device *dev)
{
	ether_setup(dev);
	dev->features |= NETIF_F_NETNS_LOCAL;
	dev->netdev_ops = &cfg80211_android_p2pdev_ops;
	dev->destructor = free_netdev;
}

int cfg80211_android_create_p2p_device(struct wireless_dev *wdev,
				       const char *name)
{
	int ret;

	if (WARN_ON(wdev->p2pdev))
		return -EEXIST;

	wdev->p2pdev = alloc_netdev(0, name, NET_NAME_UNKNOWN,
				    cfg80211_android_p2pdev_setup);
	if (!wdev->p2pdev)
		return -ENOMEM;

	memcpy(wdev->p2pdev->dev_addr, wdev->address, ETH_ALEN);
	wdev->p2pdev->ieee80211_ptr = wdev;

	ret = register_netdevice(wdev->p2pdev);
	if (ret)
		goto out_free;

	ret = sysfs_create_link(&wdev->p2pdev->dev.kobj,
				&wdev->wiphy->dev.kobj, "phy80211");
	if (ret) {
		pr_err("failed to add phy80211 symlink to netdev!\n");
		goto out_unregister;
	}

	return ret;

out_unregister:
	unregister_netdevice(wdev->p2pdev);
out_free:
	free_netdev(wdev->p2pdev);
	wdev->p2pdev = NULL;
	return ret;
}

void cfg80211_android_destroy_p2p_device(struct wireless_dev *wdev)
{
	ASSERT_RTNL();

	if (!wdev->p2pdev)
		return;

	dev_close(wdev->p2pdev);
	wdev->p2pdev->ieee80211_ptr = NULL;
	unregister_netdevice(wdev->p2pdev);
	wdev->p2pdev = NULL;
}
