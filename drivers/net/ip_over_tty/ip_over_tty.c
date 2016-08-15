/*
 * ip_over_tty.c
 *
 * Network driver for sending IP packets over tty devices.
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/etherdevice.h>
#include <linux/usb.h>
#include "ip_over_tty.h"

int debug = LEVEL_WARNING;
module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "level 0~5");

int dumpsize = 32;
module_param(dumpsize, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dumpsize, "max data length to dump");

int tx_backoff = IPOTTY_TX_BACKOFF;
module_param(tx_backoff, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tx_backoff, "tx backoff time (ms) when tty has no room");

void ipotty_print(int level, char *who, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	switch (level) {
	case LEVEL_ERROR:
		ERROR("%s %pV", who, &vaf);
		break;

	case LEVEL_WARNING:
		WARNING("%s %pV", who, &vaf);
		break;

	case LEVEL_INFO:
		INFO("%s %pV", who, &vaf);
		break;

	case LEVEL_VERBOSE:
		VDBG("%s %pV", who, &vaf);
		break;

	case LEVEL_DEBUG:
	default:
		DBG("%s %pV", who, &vaf);
		break;
	}

	va_end(args);
}

static int ip_over_tty_init(void)
{
	DBG("[%s]\n", __func__);

	ip_over_tty_ldisc_init();
	ipotty_net_init();

	return 0;
}

static void ip_over_tty_exit(void)
{
	DBG("[%s]\n", __func__);

	ip_over_tty_ldisc_remove();
	ipotty_net_remove();
}

module_init(ip_over_tty_init)
module_exit(ip_over_tty_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JC Kuo <jckuo@nvidia.com>");
MODULE_DESCRIPTION("IP over TTY Network Driver");
