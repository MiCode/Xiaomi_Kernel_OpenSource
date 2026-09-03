/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SIPA_ETH_H_
#define _SIPA_ETH_H_

#include <linux/sipa.h>
#include <linux/if.h>

#define SIPA_DUMMY_IFACE_NUM 4

/* Struct of data transfer statistics */
struct sipa_eth_dtrans_stats {
	u32 rx_sum;
	u32 rx_cnt;
	u32 rx_fail;

	u32 tx_sum;
	u32 tx_cnt;
	u32 tx_fail;
};

struct sipa_eth_init_data {
	char name[IFNAMSIZ];
	u32 src_id;
	int netid;
};

/* struct sipa_eth: Device instance data.
 * @struct net_device *netdev: Linux net device
 * @struct napi_struct_napi: Napi instance
 * @struct sipa_eth_dtrans_stats dt_stats: Record statistics
 * @struct net_device_stats *stats: Net statistics
 * @struct sipa_eth_init_data *pdata: Platform data
 */
struct sipa_eth {
	int state;
	int gro_enable;
	struct net_device *netdev;
	enum sipa_nic_id nic_id;
	struct napi_struct napi;
	struct sipa_eth_dtrans_stats dt_stats;
	struct net_device_stats *stats;
	struct sipa_eth_init_data *pdata;
};

/* Struct of data transfer statistics */
struct sipa_usb_dtrans_stats {
	u32 rx_sum;
	u32 rx_cnt;
	u32 rx_fail;

	u32 tx_sum;
	u32 tx_cnt;
	u32 tx_fail;
};

struct sipa_usb_init_data {
	char name[IFNAMSIZ];
	s32 netid;
	u32 src_id;
};

struct sipa_usb {
	int state;
	int gro_enable;
	struct net_device *ndev;
	enum sipa_nic_id nic_id;
	struct napi_struct napi;
	struct net_device_stats *stats;
	struct sipa_usb_init_data *pdata;
};

#endif
