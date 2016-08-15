/*
 * ip_over_tty_ldisc.c
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "ip_over_tty.h"

#ifdef VERBOSE_DEBUG
static
void ldisc_hexdump(struct tty_struct *tty, const unsigned char *data, int len)
{
#define DATA_PER_LINE	16
#define CHAR_PER_DATA	3 /* in "%02X " format */
	char linebuf[DATA_PER_LINE*CHAR_PER_DATA + 1];
	int i;
	char *ptr = &linebuf[0];

	if (len > dumpsize)
		len = dumpsize;

	BUG_ON(tty == NULL);
	for (i = 0; i < len; i++) {
		sprintf(ptr, " %02x ", data[i]);
		ptr += CHAR_PER_DATA;
		if (((i + 1) % DATA_PER_LINE) == 0) {
			*ptr = '\0';
			t_vdbg(tty, "%s\n", linebuf);
			ptr = &linebuf[0];
		}
	}

	/* print the last line */
	if (ptr != &linebuf[0]) {
		*ptr = '\0';
		t_vdbg(tty, "%s\n", linebuf);
	}

}
#else
#define ldisc_hexdump(args...)
#endif
/*
 * Called when a tty is put into ip_over_tty line discipline. Called in process
 * context.
 */
static int ldisc_open(struct tty_struct *tty)
{
	struct ipotty_priv *priv;
	char intf_name[16];

	DBG("[%s]\n", __func__);

	/* tty has to be writable */
	if (!tty->ops->write)
		return -EOPNOTSUPP;

	t_info(tty, "process=%s, pid=%d, set driver_name: %s to ldisc\n",
		current->comm, current->pid, tty->driver->driver_name);

	sprintf(intf_name, "net%s%d", tty->driver->name, tty->index);
	priv = ipotty_net_create_interface(intf_name);
	if (!priv) {
		t_err(tty, "[%s] ipotty_net_create_intf() failed\n", __func__);
		return -ENODEV;
	}

	tty->disc_data = priv;
	priv->tty = tty;

	spin_lock_init(&priv->rxlock);
	return 0;
}

/*
 * Called when the tty is put into another line discipline or it hangs up.
 */
static void ldisc_close(struct tty_struct *tty)
{
	struct ipotty_priv *priv;

	t_info(tty, "[%s]\n", __func__);
	priv = tty->disc_data;
	ipotty_net_destroy_interface(priv);
}

static void ldisc_receive(struct tty_struct *tty, const unsigned char *data,
			char *cflags, int count)
{
	struct ipotty_priv *priv;
	int consumed = 0;
	int left = count - consumed;
	unsigned long flags;

	/* TODO
	 * NOTE: cflags may contain information about break or overrun.
	 * This is not yet handled.
	 */

	BUG_ON(tty->disc_data == NULL);
	priv = tty->disc_data;

	spin_lock_irqsave(&priv->rxlock, flags);

	t_info(tty, "[%s] %d bytes\n", __func__, count);
	ldisc_hexdump(tty, data, count);

	while (left > 0) {
		consumed += ipotty_net_receive(priv, &data[consumed], left);
		left = count - consumed;
		t_info(tty, "consumed %d, %d left\n", consumed, left);
	}

	spin_unlock_irqrestore(&priv->rxlock, flags);
	return;
}

/*
 * Called on tty hangup in process context.
 */
static int ldisc_hangup(struct tty_struct *tty)
{
	t_info(tty, "[%s]\n", __func__);

	ldisc_close(tty);
	return 0;
}

static void ldisc_write_wakeup(struct tty_struct *tty)
{
	struct ipotty_priv *priv;

	BUG_ON(tty->disc_data == NULL);
	priv = tty->disc_data;

	t_dbg(tty, "[%s]\n", __func__);
	ipotty_net_wake_transmit(priv);
}

static struct tty_ldisc_ops ip_over_tty_ldisc = {
	.owner = THIS_MODULE,
	.magic = TTY_LDISC_MAGIC,
	.name = "n_ip_over_tty",
	.open = ldisc_open,
	.close = ldisc_close,
	.hangup = ldisc_hangup,
	.receive_buf = ldisc_receive,
	.write_wakeup = ldisc_write_wakeup,
};

int ip_over_tty_ldisc_init(void)
{
	int err;

	DBG("[%s]\n", __func__);

	err = tty_register_ldisc(N_IP_OVER_TTY, &ip_over_tty_ldisc);
	if (err < 0)
		ERROR("tty_register_ldisc() failed, err=%d\n", err);
	return err;
}

void ip_over_tty_ldisc_remove(void)
{
	DBG("[%s]\n", __func__);
	tty_unregister_ldisc(N_IP_OVER_TTY);
}
