/* Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT2712_ETHTOOL_H__
#define __MT2712_ETHTOOL_H__

static void get_pauseparam(struct net_device *dev, struct ethtool_pauseparam *pause);
static int set_pauseparam(struct net_device *dev, struct ethtool_pauseparam *pause);
static int set_coalesce(struct net_device *dev, struct ethtool_coalesce *ec);
static int get_coalesce(struct net_device *dev, struct ethtool_coalesce *ec);
static int get_sset_count(struct net_device *dev, int sset);
static void get_strings(struct net_device *dev, u32 stringset, u8 *data);
static void get_ethtool_stats(struct net_device *dev, struct ethtool_stats *dummy, u64 *data);
static int get_ts_info(struct net_device *ndev, struct ethtool_ts_info *info);

#endif
