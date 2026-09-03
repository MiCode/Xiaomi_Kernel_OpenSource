// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include "sla.h"
#include "sla_net_stats_netlink.h"

unsigned int sla_dbg_lvl = SLA_PRT_ALL;
spinlock_t mgr_lock;/* Spinlock for sla */

static int __init init_sla_module(void)
{
	spin_lock_init(&mgr_lock);
	sla_net_stats_init();
	sla_nl_init();

	return 0;
}

static void __exit exit_sla_module(void)
{
	sla_net_stats_exit();
	sla_nl_exit();
}

late_initcall(init_sla_module);
module_exit(exit_sla_module);
MODULE_ALIAS("platform:SPRD SLA.");
MODULE_DESCRIPTION("Smartlink Aggregation (SLA) Technology:Application can access network by multiple links simultaneously.");
MODULE_AUTHOR("Xiang Qiu <xiang.qiu@unisoc.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
