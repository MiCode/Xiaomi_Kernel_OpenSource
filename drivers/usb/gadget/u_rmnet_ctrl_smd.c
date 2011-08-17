/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/termios.h>
#include <mach/msm_smd.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/termios.h>

#include "u_rmnet.h"

#define NR_PORTS	1
static int n_ports;
static char *rmnet_ctrl_names[] = { "DATA40_CNTL" };
static struct workqueue_struct *grmnet_ctrl_wq;

#define SMD_CH_MAX_LEN	20
#define CH_OPENED	0
#define CH_READY	1
struct smd_ch_info {
	struct smd_channel	*ch;
	char			*name;
	unsigned long		flags;
	wait_queue_head_t	wait;
	unsigned		dtr;

	struct list_head	tx_q;
	unsigned long		tx_len;

	struct work_struct	read_w;
	struct work_struct	write_w;

	struct rmnet_ctrl_port	*port;

	int			cbits_tomodem;
	/* stats */
	unsigned long		to_modem;
	unsigned long		to_host;
};

struct rmnet_ctrl_port {
	struct smd_ch_info	ctrl_ch;
	unsigned int		port_num;
	struct grmnet		*port_usb;

	spinlock_t		port_lock;
	struct work_struct	connect_w;
};

static struct rmnet_ctrl_ports {
	struct rmnet_ctrl_port *port;
	struct platform_driver pdrv;
} ports[NR_PORTS];


/*---------------misc functions---------------- */

static struct rmnet_ctrl_pkt *rmnet_alloc_ctrl_pkt(unsigned len, gfp_t flags)
{
	struct rmnet_ctrl_pkt *pkt;

	pkt = kzalloc(sizeof(struct rmnet_ctrl_pkt), flags);
	if (!pkt)
		return ERR_PTR(-ENOMEM);

	pkt->buf = kmalloc(len, flags);
	if (!pkt->buf) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}
	pkt->len = len;

	return pkt;
}

static void rmnet_ctrl_pkt_free(struct rmnet_ctrl_pkt *pkt)
{
	kfree(pkt->buf);
	kfree(pkt);
}

/*--------------------------------------------- */

/*---------------control/smd channel functions---------------- */

static void grmnet_ctrl_smd_read_w(struct work_struct *w)
{
	struct smd_ch_info *c = container_of(w, struct smd_ch_info, read_w);
	struct rmnet_ctrl_port *port = c->port;
	int sz;
	struct rmnet_ctrl_pkt *cpkt;
	unsigned long flags;

	while (1) {
		sz = smd_cur_packet_size(c->ch);
		if (sz == 0)
			break;

		if (smd_read_avail(c->ch) < sz)
			break;

		cpkt = rmnet_alloc_ctrl_pkt(sz, GFP_KERNEL);
		if (IS_ERR(cpkt)) {
			pr_err("%s: unable to allocate rmnet control pkt\n",
					__func__);
			return;
		}
		cpkt->len = smd_read(c->ch, cpkt->buf, sz);

		/* send it to USB here */
		spin_lock_irqsave(&port->port_lock, flags);
		if (port->port_usb && port->port_usb->send_cpkt_response) {
			port->port_usb->send_cpkt_response(
							port->port_usb,
							cpkt);
			c->to_host++;
		}
		spin_unlock_irqrestore(&port->port_lock, flags);
	}
}

static void grmnet_ctrl_smd_write_w(struct work_struct *w)
{
	struct smd_ch_info *c = container_of(w, struct smd_ch_info, write_w);
	struct rmnet_ctrl_port *port = c->port;
	unsigned long flags;
	struct rmnet_ctrl_pkt *cpkt;
	int ret;

	spin_lock_irqsave(&port->port_lock, flags);
	while (1) {
		if (list_empty(&c->tx_q))
			break;

		cpkt = list_first_entry(&c->tx_q, struct rmnet_ctrl_pkt, list);

		if (smd_write_avail(c->ch) < cpkt->len)
			break;

		list_del(&cpkt->list);
		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = smd_write(c->ch, cpkt->buf, cpkt->len);
		spin_lock_irqsave(&port->port_lock, flags);
		if (ret != cpkt->len) {
			pr_err("%s: smd_write failed err:%d\n",
					__func__, ret);
			rmnet_ctrl_pkt_free(cpkt);
			break;
		}
		rmnet_ctrl_pkt_free(cpkt);
		c->to_modem++;
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static int
grmnet_ctrl_smd_send_cpkt_tomodem(struct grmnet *gr, u8 portno,
			struct rmnet_ctrl_pkt *cpkt)
{
	unsigned long		flags;
	struct rmnet_ctrl_port	*port;
	struct smd_ch_info	*c;

	if (portno >= n_ports) {
		pr_err("%s: Invalid portno#%d\n", __func__, portno);
		return -ENODEV;
	}

	if (!gr) {
		pr_err("%s: grmnet is null\n", __func__);
		return -ENODEV;
	}

	port = ports[portno].port;

	spin_lock_irqsave(&port->port_lock, flags);
	c = &port->ctrl_ch;

	/* drop cpkt if ch is not open */
	if (!test_bit(CH_OPENED, &c->flags)) {
		rmnet_ctrl_pkt_free(cpkt);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return 0;
	}

	list_add_tail(&cpkt->list, &c->tx_q);
	queue_work(grmnet_ctrl_wq, &c->write_w);
	spin_unlock_irqrestore(&port->port_lock, flags);

	return 0;
}

#define ACM_CTRL_DTR		0x01
static void
gsmd_ctrl_send_cbits_tomodem(struct grmnet *gr, u8 portno, int cbits)
{
	struct rmnet_ctrl_port	*port;
	struct smd_ch_info	*c;
	int			set_bits = 0;
	int			clear_bits = 0;
	int			temp = 0;

	if (portno >= n_ports) {
		pr_err("%s: Invalid portno#%d\n", __func__, portno);
		return;
	}

	if (!gr) {
		pr_err("%s: grmnet is null\n", __func__);
		return;
	}

	port = ports[portno].port;
	cbits = cbits & ACM_CTRL_DTR;
	c = &port->ctrl_ch;

	/* host driver will only send DTR, but to have generic
	 * set and clear bit implementation using two separate
	 * checks
	 */
	if (cbits & ACM_CTRL_DTR)
		set_bits |= TIOCM_DTR;
	else
		clear_bits |= TIOCM_DTR;

	temp |= set_bits;
	temp &= ~clear_bits;

	if (temp == c->cbits_tomodem)
		return;

	c->cbits_tomodem = temp;

	if (!test_bit(CH_OPENED, &c->flags))
		return;

	pr_debug("%s: ctrl_tomodem:%d ctrl_bits:%d setbits:%d clearbits:%d\n",
			__func__, temp, cbits, set_bits, clear_bits);

	smd_tiocmset(c->ch, set_bits, clear_bits);
}

static char *get_smd_event(unsigned event)
{
	switch (event) {
	case SMD_EVENT_DATA:
		return "DATA";
	case SMD_EVENT_OPEN:
		return "OPEN";
	case SMD_EVENT_CLOSE:
		return "CLOSE";
	}

	return "UNDEFINED";
}

static void grmnet_ctrl_smd_notify(void *p, unsigned event)
{
	struct rmnet_ctrl_port	*port = p;
	struct smd_ch_info	*c = &port->ctrl_ch;

	pr_debug("%s: EVENT_(%s)\n", __func__, get_smd_event(event));

	switch (event) {
	case SMD_EVENT_DATA:
		if (smd_read_avail(c->ch))
			queue_work(grmnet_ctrl_wq, &c->read_w);
		if (smd_write_avail(c->ch))
			queue_work(grmnet_ctrl_wq, &c->write_w);
		break;
	case SMD_EVENT_OPEN:
		set_bit(CH_OPENED, &c->flags);
		wake_up(&c->wait);
		break;
	case SMD_EVENT_CLOSE:
		clear_bit(CH_OPENED, &c->flags);
		break;
	}
}
/*------------------------------------------------------------ */

static void grmnet_ctrl_smd_connect_w(struct work_struct *w)
{
	struct rmnet_ctrl_port *port =
			container_of(w, struct rmnet_ctrl_port, connect_w);
	struct smd_ch_info *c = &port->ctrl_ch;
	unsigned long flags;
	int ret;

	pr_debug("%s:\n", __func__);

	if (!test_bit(CH_READY, &c->flags))
		return;

	ret = smd_open(c->name, &c->ch, port, grmnet_ctrl_smd_notify);
	if (ret) {
		pr_err("%s: Unable to open smd ch:%s err:%d\n",
				__func__, c->name, ret);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port_usb)
		smd_tiocmset(c->ch, c->cbits_tomodem, ~c->cbits_tomodem);
	spin_unlock_irqrestore(&port->port_lock, flags);
}

int gsmd_ctrl_connect(struct grmnet *gr, int port_num)
{
	struct rmnet_ctrl_port	*port;
	struct smd_ch_info	*c;
	unsigned long		flags;

	pr_debug("%s: grmnet:%p port#%d\n", __func__, gr, port_num);

	if (port_num >= n_ports) {
		pr_err("%s: invalid portno#%d\n", __func__, port_num);
		return -ENODEV;
	}

	if (!gr) {
		pr_err("%s: grmnet port is null\n", __func__);
		return -ENODEV;
	}

	port = ports[port_num].port;
	c = &port->ctrl_ch;

	spin_lock_irqsave(&port->port_lock, flags);
	port->port_usb = gr;
	gr->send_cpkt_request = grmnet_ctrl_smd_send_cpkt_tomodem;
	gr->send_cbits_tomodem = gsmd_ctrl_send_cbits_tomodem;
	spin_unlock_irqrestore(&port->port_lock, flags);

	queue_work(grmnet_ctrl_wq, &port->connect_w);

	return 0;
}

void gsmd_ctrl_disconnect(struct grmnet *gr, u8 port_num)
{
	struct rmnet_ctrl_port	*port;
	unsigned long		flags;
	struct smd_ch_info	*c;

	pr_debug("%s: grmnet:%p port#%d\n", __func__, gr, port_num);

	if (port_num >= n_ports) {
		pr_err("%s: invalid portno#%d\n", __func__, port_num);
		return;
	}

	if (!gr) {
		pr_err("%s: grmnet port is null\n", __func__);
		return;
	}

	port = ports[port_num].port;
	c = &port->ctrl_ch;

	spin_lock_irqsave(&port->port_lock, flags);
	port->port_usb = 0;
	gr->send_cpkt_request = 0;
	gr->send_cbits_tomodem = 0;
	c->cbits_tomodem = 0;
	spin_unlock_irqrestore(&port->port_lock, flags);

	if (test_bit(CH_OPENED, &c->flags)) {
		/* this should send the dtr zero */
		smd_close(c->ch);
		clear_bit(CH_OPENED, &c->flags);
	}
}

#define SMD_CH_MAX_LEN	20
static int grmnet_ctrl_smd_ch_probe(struct platform_device *pdev)
{
	struct rmnet_ctrl_port	*port;
	struct smd_ch_info	*c;
	int			i;
	unsigned long		flags;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	for (i = 0; i < n_ports; i++) {
		port = ports[i].port;
		c = &port->ctrl_ch;

		if (!strncmp(c->name, pdev->name, SMD_CH_MAX_LEN)) {
			set_bit(CH_READY, &c->flags);

			/* if usb is online, try opening smd_ch */
			spin_lock_irqsave(&port->port_lock, flags);
			if (port->port_usb)
				queue_work(grmnet_ctrl_wq, &port->connect_w);
			spin_unlock_irqrestore(&port->port_lock, flags);

			break;
		}
	}

	return 0;
}

static int grmnet_ctrl_smd_ch_remove(struct platform_device *pdev)
{
	struct rmnet_ctrl_port	*port;
	struct smd_ch_info	*c;
	int			i;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	for (i = 0; i < n_ports; i++) {
		port = ports[i].port;
		c = &port->ctrl_ch;

		if (!strncmp(c->name, pdev->name, SMD_CH_MAX_LEN)) {
			clear_bit(CH_READY, &c->flags);
			clear_bit(CH_OPENED, &c->flags);
			smd_close(c->ch);
			break;
		}
	}

	return 0;
}


static void grmnet_ctrl_smd_port_free(int portno)
{
	struct rmnet_ctrl_port	*port = ports[portno].port;

	if (!port)
		kfree(port);
}

static int grmnet_ctrl_smd_port_alloc(int portno)
{
	struct rmnet_ctrl_port	*port;
	struct smd_ch_info	*c;
	struct platform_driver	*pdrv;

	port = kzalloc(sizeof(struct rmnet_ctrl_port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	port->port_num = portno;

	spin_lock_init(&port->port_lock);
	INIT_WORK(&port->connect_w, grmnet_ctrl_smd_connect_w);

	c = &port->ctrl_ch;
	c->name = rmnet_ctrl_names[portno];
	c->port = port;
	init_waitqueue_head(&c->wait);
	INIT_LIST_HEAD(&c->tx_q);
	INIT_WORK(&c->read_w, grmnet_ctrl_smd_read_w);
	INIT_WORK(&c->write_w, grmnet_ctrl_smd_write_w);

	ports[portno].port = port;

	pdrv = &ports[portno].pdrv;
	pdrv->probe = grmnet_ctrl_smd_ch_probe;
	pdrv->remove = grmnet_ctrl_smd_ch_remove;
	pdrv->driver.name = c->name;
	pdrv->driver.owner = THIS_MODULE;

	platform_driver_register(pdrv);

	pr_debug("%s: port:%p portno:%d\n", __func__, port, portno);

	return 0;
}

int gsmd_ctrl_setup(unsigned int count)
{
	int	i;
	int	ret;

	pr_debug("%s: requested ports:%d\n", __func__, count);

	if (!count || count > NR_PORTS) {
		pr_err("%s: Invalid num of ports count:%d\n",
				__func__, count);
		return -EINVAL;
	}

	grmnet_ctrl_wq = alloc_workqueue("gsmd_ctrl",
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!grmnet_ctrl_wq) {
		pr_err("%s: Unable to create workqueue grmnet_ctrl\n",
				__func__);
		return -ENOMEM;
	}

	for (i = 0; i < count; i++) {
		ret = grmnet_ctrl_smd_port_alloc(i);
		if (ret) {
			pr_err("%s: Unable to alloc port:%d\n", __func__, i);
			goto free_ports;
		}
		n_ports++;
	}

	return 0;

free_ports:
	for (i = 0; i < n_ports; i++)
		grmnet_ctrl_smd_port_free(i);

	destroy_workqueue(grmnet_ctrl_wq);

	return ret;
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	1024
static ssize_t gsmd_ctrl_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct rmnet_ctrl_port	*port;
	struct smd_ch_info	*c;
	char			*buf;
	unsigned long		flags;
	int			ret;
	int			i;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < n_ports; i++) {
		port = ports[i].port;
		if (!port)
			continue;
		spin_lock_irqsave(&port->port_lock, flags);

		c = &port->ctrl_ch;

		temp += scnprintf(buf + temp, DEBUG_BUF_SIZE - temp,
				"#PORT:%d port:%p ctrl_ch:%p#\n"
				"to_usbhost: %lu\n"
				"to_modem:   %lu\n"
				"DTR:        %s\n"
				"ch_open:    %d\n"
				"ch_ready:   %d\n"
				"read_avail: %d\n"
				"write_avail:%d\n",
				i, port, &port->ctrl_ch,
				c->to_host, c->to_modem,
				c->cbits_tomodem ? "HIGH" : "LOW",
				test_bit(CH_OPENED, &c->flags),
				test_bit(CH_READY, &c->flags),
				smd_read_avail(c->ch),
				smd_write_avail(c->ch));

		spin_unlock_irqrestore(&port->port_lock, flags);
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);

	kfree(buf);

	return ret;
}

static ssize_t gsmd_ctrl_reset_stats(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct rmnet_ctrl_port	*port;
	struct smd_ch_info	*c;
	int			i;
	unsigned long		flags;

	for (i = 0; i < n_ports; i++) {
		port = ports[i].port;
		if (!port)
			continue;

		spin_lock_irqsave(&port->port_lock, flags);

		c = &port->ctrl_ch;

		c->to_host = 0;
		c->to_modem = 0;

		spin_unlock_irqrestore(&port->port_lock, flags);
	}
	return count;
}

const struct file_operations gsmd_ctrl_stats_ops = {
	.read = gsmd_ctrl_read_stats,
	.write = gsmd_ctrl_reset_stats,
};

struct dentry *smd_ctrl_dent;
struct dentry *smd_ctrl_dfile;
static void gsmd_ctrl_debugfs_init(void)
{
	smd_ctrl_dent = debugfs_create_dir("usb_rmnet_ctrl_smd", 0);
	if (IS_ERR(smd_ctrl_dent))
		return;

	smd_ctrl_dfile = debugfs_create_file("status", 0444, smd_ctrl_dent, 0,
			&gsmd_ctrl_stats_ops);
	if (!smd_ctrl_dfile || IS_ERR(smd_ctrl_dfile))
		debugfs_remove(smd_ctrl_dent);
}

static void gsmd_ctrl_debugfs_exit(void)
{
	debugfs_remove(smd_ctrl_dfile);
	debugfs_remove(smd_ctrl_dent);
}

#else
static void gsmd_ctrl_debugfs_init(void) { }
static void gsmd_ctrl_debugfs_exit(void) { }
#endif

static int __init gsmd_ctrl_init(void)
{
	gsmd_ctrl_debugfs_init();

	return 0;
}
module_init(gsmd_ctrl_init);

static void __exit gsmd_ctrl_exit(void)
{
	gsmd_ctrl_debugfs_exit();
}
module_exit(gsmd_ctrl_exit);
MODULE_DESCRIPTION("smd control driver");
MODULE_LICENSE("GPL v2");
