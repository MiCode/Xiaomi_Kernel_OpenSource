/*
 * File: l3mhi.c
 *
 * L2 channels to AF_MHI binding.
 *
 * Copyright (C) 2011 Renesas Mobile Corporation. All rights reserved.
 *
 * Author: Petri To Mattila <petri.to.mattila@renesasmobile.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/mhi.h>
#include <linux/l2mux.h>

#include <net/af_mhi.h>
#include <net/mhi/sock.h>
#include <net/mhi/dgram.h>

#define MAX_CHANNELS  256

#ifdef CONFIG_MHI_DEBUG
# define DPRINTK(...)    printk(KERN_DEBUG "L3MHI: " __VA_ARGS__)
#else
# define DPRINTK(...)
#endif


/* Module parameters - with defaults */
static int l2chs[MAX_CHANNELS] = {
	MHI_L3_FILE,
	MHI_L3_XFILE,
	MHI_L3_SECURITY,
	MHI_L3_TEST,
	MHI_L3_TEST_PRIO,
	MHI_L3_THERMAL,
	MHI_L3_HIGH_PRIO_TEST,
	MHI_L3_MED_PRIO_TEST,
	MHI_L3_LOW_PRIO_TEST
};
static int l2cnt = 9;



/* Functions */

static int
mhi_netif_rx(struct sk_buff *skb, struct net_device *dev)
{
	skb->protocol = htons(ETH_P_MHI);

	return netif_rx(skb);
}


/* Module registration */

int __init l3mhi_init(void)
{
	int ch, i;
	int err;

	for (i = 0; i < l2cnt; i++) {
		ch = l2chs[i];
		if (ch >= 0 && ch < MHI_L3_NPROTO) {
			err = l2mux_netif_rx_register(ch, mhi_netif_rx);
			if (err)
				goto error;

			err = mhi_register_protocol(ch);
			if (err)
				goto error;
		}
	}

	return 0;

error:
	for (i = 0; i < l2cnt; i++) {
		ch = l2chs[i];
		if (ch >= 0 && ch < MHI_L3_NPROTO) {
			if (mhi_protocol_registered(ch)) {
				l2mux_netif_rx_unregister(ch);
				mhi_unregister_protocol(ch);
			}
		}
	}

	return err;
}

void __exit l3mhi_exit(void)
{
	int ch, i;

	for (i = 0; i < l2cnt; i++) {
		ch = l2chs[i];
		if (ch >= 0 && ch < MHI_L3_NPROTO) {
			if (mhi_protocol_registered(ch)) {
				l2mux_netif_rx_unregister(ch);
				mhi_unregister_protocol(ch);
			}
		}
	}
}


module_init(l3mhi_init);
module_exit(l3mhi_exit);

module_param_array_named(l2_channels, l2chs, int, &l2cnt, 0444);

MODULE_AUTHOR("Petri Mattila <petri.to.mattila@renesasmobile.com>");
MODULE_DESCRIPTION("L3 MHI Binding");
MODULE_LICENSE("GPL");

