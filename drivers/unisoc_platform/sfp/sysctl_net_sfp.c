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
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/netdevice.h>
#include <linux/ratelimit.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kmemleak.h>

#include <net/ip.h>
#include <net/sock.h>
#include <net/net_ratelimit.h>
#include <net/busy_poll.h>
#include <net/pkt_sched.h>

#include "sfp.h"

int sysctl_net_sfp_enable  __read_mostly;

int sysctl_net_sfp_tether_scheme  __read_mostly;

int sysctl_tcp_aging_time  __read_mostly = DEFAULT_SFP_TCP_AGING_TIME;
int sysctl_udp_aging_time  __read_mostly = DEFAULT_SFP_UDP_AGING_TIME;

int get_sfp_tether_scheme(void)
{
	return sysctl_net_sfp_tether_scheme;
}
EXPORT_SYMBOL(get_sfp_tether_scheme);

void set_sfp_tether_scheme(int tether_scheme)
{
	if (tether_scheme < SFP_HARD_PATH || tether_scheme > SFP_HALF_PATH) {
		FP_PRT_DBG(FP_PRT_ERR, "illegal tether_scheme %d\n", tether_scheme);
		return;
	}

	FP_PRT_DBG(FP_PRT_INFO, "set tether_scheme to %d\n", tether_scheme);
	sysctl_net_sfp_tether_scheme = tether_scheme;
}
EXPORT_SYMBOL(set_sfp_tether_scheme);

void set_sfp_enable(int enable)
{
	if (enable != 1 && enable != 0) {
		FP_PRT_DBG(FP_PRT_ERR, "illegal enable %d\n", enable);
		return;
	}

	FP_PRT_DBG(FP_PRT_INFO, "set enable to %d\n", enable);
	sysctl_net_sfp_enable = enable;
}
EXPORT_SYMBOL(set_sfp_enable);

static struct ctl_table net_sfp_table[] = {
#if IS_ENABLED(CONFIG_NET)
	{
		.procname	= "sfp_tcp_aging_time",
		.data		= &sysctl_tcp_aging_time,
		.maxlen		= sizeof(u32),
		.mode		= 0664,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "sfp_udp_aging_time",
		.data		= &sysctl_udp_aging_time,
		.maxlen		= sizeof(u32),
		.mode		= 0664,
		.proc_handler	= proc_dointvec
	},
#endif /* CONFIG_NET */
	{ }
};

static __net_init int sysctl_sfp_net_init(struct net *net)
{
	return 0;
}

static __net_exit void sysctl_sfp_net_exit(struct net *net)
{
}

static struct pernet_operations sysctl_sfp_ops __net_initdata = {
	.init = sysctl_sfp_net_init,
	.exit = sysctl_sfp_net_exit,
};

int __init sysctl_sfp_init(void)
{
	register_net_sysctl(&init_net, "net/sfp", net_sfp_table);
	return register_pernet_subsys(&sysctl_sfp_ops);
}

void sysctl_sfp_exit(void)
{
	pr_debug("Unregistering sysctl_sfp");

	unregister_pernet_subsys(&sysctl_sfp_ops);
}

