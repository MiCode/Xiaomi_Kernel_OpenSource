/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/smux.h>
#include <linux/completion.h>

#include "usb_gadget_xport.h"

#define CH_OPENED 0
#define CH_READY 1
#define CH_CONNECTED 2

static unsigned int num_ctrl_ports;

static const char *ghsuart_ctrl_names[] = {
	"SMUX_RMNET_CTL_HSUART"
};

struct ghsuart_ctrl_port {
	/* port */
	unsigned port_num;
	/* gadget */
	enum gadget_type gtype;
	spinlock_t port_lock;
	void *port_usb;
	struct completion close_complete;
	/* work queue*/
	struct workqueue_struct	*wq;
	struct work_struct connect_w;
	struct work_struct disconnect_w;
	/*ctrl pkt response cb*/
	int (*send_cpkt_response)(void *g, void *buf, size_t len);
	void *ctxt;
	unsigned int ch_id;
	/* flow control bits */
	unsigned long flags;
	int (*send_pkt)(void *, void *, size_t actual);
	/* Channel status */
	unsigned long channel_sts;
	/* control bits */
	unsigned cbits_tomodem;
	/* counters */
	unsigned long to_modem;
	unsigned long to_host;
	unsigned long drp_cpkt_cnt;
};

static struct {
	struct ghsuart_ctrl_port	*port;
	struct platform_driver	pdrv;
} ghsuart_ctrl_ports[NUM_HSUART_PORTS];

static int ghsuart_ctrl_receive(void *dev, void *buf, size_t actual);

static void smux_control_event(void *priv, int event_type, const void *metadata)
{
	struct grmnet		*gr = NULL;
	struct ghsuart_ctrl_port	*port = priv;
	void			*buf;
	unsigned long		flags;
	size_t			len;

	switch (event_type) {
	case SMUX_LOCAL_CLOSED:
		clear_bit(CH_OPENED, &port->channel_sts);
		complete(&port->close_complete);
		break;
	case SMUX_CONNECTED:
		spin_lock_irqsave(&port->port_lock, flags);
		if (!port->port_usb) {
			spin_unlock_irqrestore(&port->port_lock, flags);
			return;
		}
		spin_unlock_irqrestore(&port->port_lock, flags);
		set_bit(CH_CONNECTED, &port->channel_sts);
		if (port->gtype == USB_GADGET_RMNET) {
			gr = port->port_usb;
			if (gr && gr->connect)
				gr->connect(gr);
		}
		break;
	case SMUX_DISCONNECTED:
		clear_bit(CH_CONNECTED, &port->channel_sts);
		break;
	case SMUX_READ_DONE:
		len = ((struct smux_meta_read *)metadata)->len;
		buf = ((struct smux_meta_read *)metadata)->buffer;
		ghsuart_ctrl_receive(port, buf, len);
		break;
	case SMUX_READ_FAIL:
		buf = ((struct smux_meta_read *)metadata)->buffer;
		kfree(buf);
		break;
	case SMUX_WRITE_DONE:
	case SMUX_WRITE_FAIL:
		buf = ((struct smux_meta_write *)metadata)->buffer;
		kfree(buf);
		break;
	case SMUX_LOW_WM_HIT:
	case SMUX_HIGH_WM_HIT:
	case SMUX_TIOCM_UPDATE:
		break;
	default:
		pr_err("%s Event %d not supported\n", __func__, event_type);
	};
}

static int rx_control_buffer(void *priv, void **pkt_priv, void **buffer,
			int size)
{
	void *rx_buf;

	rx_buf = kmalloc(size, GFP_KERNEL);
	if (!rx_buf)
		return -EAGAIN;
	*buffer = rx_buf;
	*pkt_priv = NULL;

	return 0;
}

static int ghsuart_ctrl_receive(void *dev, void *buf, size_t actual)
{
	struct ghsuart_ctrl_port	*port = dev;
	int retval = 0;

	pr_debug_ratelimited("%s: read complete bytes read: %zu\n",
			__func__, actual);

	/* send it to USB here */
	if (port && port->send_cpkt_response) {
		retval = port->send_cpkt_response(port->port_usb, buf, actual);
		port->to_host++;
	}
	kfree(buf);
	return retval;
}

static int
ghsuart_send_cpkt_tomodem(u8 portno, void *buf, size_t len)
{
	void			*cbuf;
	struct ghsuart_ctrl_port	*port;
	int			ret;

	if (portno >= num_ctrl_ports) {
		pr_err("%s: Invalid portno#%d\n", __func__, portno);
		return -ENODEV;
	}

	port = ghsuart_ctrl_ports[portno].port;
	if (!port) {
		pr_err("%s: port is null\n", __func__);
		return -ENODEV;
	}
	/* drop cpkt if ch is not open */
	if (!test_bit(CH_CONNECTED, &port->channel_sts)) {
		port->drp_cpkt_cnt++;
		return 0;
	}
	cbuf = kmalloc(len, GFP_ATOMIC);
	if (!cbuf)
		return -ENOMEM;

	memcpy(cbuf, buf, len);

	pr_debug("%s: ctrl_pkt:%zu bytes\n", __func__, len);

	ret = msm_smux_write(port->ch_id, port, (void *)cbuf, len);
	if (ret < 0) {
		pr_err_ratelimited("%s: write error:%d\n",
				__func__, ret);
		port->drp_cpkt_cnt++;
		kfree(cbuf);
		return ret;
	}
	port->to_modem++;

	return 0;
}

static void
ghsuart_send_cbits_tomodem(void *gptr, u8 portno, int cbits)
{
	struct ghsuart_ctrl_port	*port;

	if (portno >= num_ctrl_ports || !gptr) {
		pr_err("%s: Invalid portno#%d\n", __func__, portno);
		return;
	}

	port = ghsuart_ctrl_ports[portno].port;
	if (!port) {
		pr_err("%s: port is null\n", __func__);
		return;
	}

	if (cbits == port->cbits_tomodem)
		return;

	port->cbits_tomodem = cbits;

	if (!test_bit(CH_CONNECTED, &port->channel_sts))
		return;

	pr_debug("%s: ctrl_tomodem:%d\n", __func__, cbits);
	/* Send the control bits to the Modem */
	msm_smux_tiocm_set(port->ch_id, cbits, ~cbits);
}

static void ghsuart_ctrl_connect_w(struct work_struct *w)
{
	struct ghsuart_ctrl_port	*port =
			container_of(w, struct ghsuart_ctrl_port, connect_w);
	int			retval;

	if (!port || !test_bit(CH_READY, &port->channel_sts))
		return;

	pr_debug("%s: port:%p\n", __func__, port);

	if (test_bit(CH_OPENED, &port->channel_sts)) {
		retval = wait_for_completion_timeout(
				&port->close_complete, 3 * HZ);
		if (retval == 0) {
			pr_err("%s: smux close timedout\n", __func__);
			return;
		}
	}
	retval = msm_smux_open(port->ch_id, port->ctxt, smux_control_event,
				rx_control_buffer);
	if (retval < 0) {
		pr_err(" %s smux_open failed\n", __func__);
		return;
	}
	set_bit(CH_OPENED, &port->channel_sts);

}

int ghsuart_ctrl_connect(void *gptr, int port_num)
{
	struct ghsuart_ctrl_port	*port;
	struct grmnet		*gr;
	unsigned long		flags;

	pr_debug("%s: port#%d\n", __func__, port_num);

	if (port_num > num_ctrl_ports || !gptr) {
		pr_err("%s: invalid portno#%d\n", __func__, port_num);
		return -ENODEV;
	}

	port = ghsuart_ctrl_ports[port_num].port;
	if (!port) {
		pr_err("%s: port is null\n", __func__);
		return -ENODEV;
	}

	spin_lock_irqsave(&port->port_lock, flags);

	gr = gptr;
	port->send_cpkt_response = gr->send_cpkt_response;
	gr->send_encap_cmd = ghsuart_send_cpkt_tomodem;
	gr->notify_modem = ghsuart_send_cbits_tomodem;

	port->port_usb = gptr;
	port->to_host = 0;
	port->to_modem = 0;
	port->drp_cpkt_cnt = 0;
	spin_unlock_irqrestore(&port->port_lock, flags);

	if (test_bit(CH_READY, &port->channel_sts))
		queue_work(port->wq, &port->connect_w);

	return 0;
}

static void ghsuart_ctrl_disconnect_w(struct work_struct *w)
{
	struct ghsuart_ctrl_port	*port =
			container_of(w, struct ghsuart_ctrl_port, disconnect_w);

	if (!test_bit(CH_OPENED, &port->channel_sts))
		return;

	INIT_COMPLETION(port->close_complete);
	msm_smux_close(port->ch_id);
	clear_bit(CH_CONNECTED, &port->channel_sts);
}

void ghsuart_ctrl_disconnect(void *gptr, int port_num)
{
	struct ghsuart_ctrl_port	*port;
	struct grmnet		*gr = NULL;
	unsigned long		flags;

	pr_debug("%s: port#%d\n", __func__, port_num);

	if (port_num > num_ctrl_ports) {
		pr_err("%s: invalid portno#%d\n", __func__, port_num);
		return;
	}

	port = ghsuart_ctrl_ports[port_num].port;

	if (!gptr || !port) {
		pr_err("%s: grmnet port is null\n", __func__);
		return;
	}

	gr = gptr;

	spin_lock_irqsave(&port->port_lock, flags);
	gr->send_encap_cmd = 0;
	gr->notify_modem = 0;
	port->cbits_tomodem = 0;
	port->port_usb = 0;
	port->send_cpkt_response = 0;
	spin_unlock_irqrestore(&port->port_lock, flags);

	queue_work(port->wq, &port->disconnect_w);
}

static int ghsuart_ctrl_probe(struct platform_device *pdev)
{
	struct ghsuart_ctrl_port	*port;
	unsigned long		flags;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	port = ghsuart_ctrl_ports[pdev->id].port;
	set_bit(CH_READY, &port->channel_sts);

	/* if usb is online, start read */
	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port_usb)
		queue_work(port->wq, &port->connect_w);
	spin_unlock_irqrestore(&port->port_lock, flags);

	return 0;
}

static int ghsuart_ctrl_remove(struct platform_device *pdev)
{
	struct ghsuart_ctrl_port	*port;
	struct grmnet		*gr = NULL;
	unsigned long		flags;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	port = ghsuart_ctrl_ports[pdev->id].port;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		goto not_ready;
	}

	gr = port->port_usb;

	spin_unlock_irqrestore(&port->port_lock, flags);

	if (gr && gr->disconnect)
		gr->disconnect(gr);

	clear_bit(CH_OPENED, &port->channel_sts);
	clear_bit(CH_CONNECTED, &port->channel_sts);
not_ready:
	clear_bit(CH_READY, &port->channel_sts);

	return 0;
}

static void ghsuart_ctrl_port_free(int portno)
{
	struct ghsuart_ctrl_port	*port = ghsuart_ctrl_ports[portno].port;
	struct platform_driver	*pdrv = &ghsuart_ctrl_ports[portno].pdrv;

	destroy_workqueue(port->wq);
	if (pdrv)
		platform_driver_unregister(pdrv);
	kfree(port);
}

static int ghsuart_ctrl_port_alloc(int portno, enum gadget_type gtype)
{
	struct ghsuart_ctrl_port	*port;
	struct platform_driver	*pdrv;
	int err;

	port = kzalloc(sizeof(struct ghsuart_ctrl_port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	port->wq = create_singlethread_workqueue(ghsuart_ctrl_names[portno]);
	if (!port->wq) {
		pr_err("%s: Unable to create workqueue:%s\n",
			__func__, ghsuart_ctrl_names[portno]);
		kfree(port);
		return -ENOMEM;
	}

	port->port_num = portno;
	port->gtype = gtype;

	spin_lock_init(&port->port_lock);

	init_completion(&port->close_complete);
	INIT_WORK(&port->connect_w, ghsuart_ctrl_connect_w);
	INIT_WORK(&port->disconnect_w, ghsuart_ctrl_disconnect_w);

	port->ch_id = SMUX_USB_RMNET_CTL_0;
	port->ctxt = port;
	port->send_pkt = ghsuart_ctrl_receive;
	ghsuart_ctrl_ports[portno].port = port;

	pdrv = &ghsuart_ctrl_ports[portno].pdrv;
	pdrv->probe = ghsuart_ctrl_probe;
	pdrv->remove = ghsuart_ctrl_remove;
	pdrv->driver.name = ghsuart_ctrl_names[portno];
	pdrv->driver.owner = THIS_MODULE;

	err = platform_driver_register(pdrv);
	if (unlikely(err < 0))
		return err;
	pr_debug("%s: port:%p portno:%d\n", __func__, port, portno);

	return 0;
}

int ghsuart_ctrl_setup(unsigned int num_ports, enum gadget_type gtype)
{
	int	first_port_id = num_ctrl_ports;
	int	total_num_ports = num_ports + num_ctrl_ports;
	int	i;
	int	ret = 0;

	if (!num_ports || total_num_ports > NUM_HSUART_PORTS) {
		pr_err("%s: Invalid num of ports count:%d\n",
				__func__, num_ports);
		return -EINVAL;
	}

	pr_debug("%s: requested ports:%d\n", __func__, num_ports);

	for (i = first_port_id; i < (first_port_id + num_ports); i++) {

		num_ctrl_ports++;
		ret = ghsuart_ctrl_port_alloc(i, gtype);
		if (ret) {
			num_ctrl_ports--;
			pr_err("%s: Unable to alloc port:%d\n", __func__, i);
			goto free_ports;
		}
	}

	return first_port_id;

free_ports:
	for (i = first_port_id; i < num_ctrl_ports; i++)
		ghsuart_ctrl_port_free(i);
		num_ctrl_ports = first_port_id;
	return ret;
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	1024
static ssize_t ghsuart_ctrl_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct ghsuart_ctrl_port	*port;
	char			*buf;
	unsigned long		flags;
	int			ret;
	int			i;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < num_ctrl_ports; i++) {
		port = ghsuart_ctrl_ports[i].port;
		if (!port)
			continue;
		spin_lock_irqsave(&port->port_lock, flags);

		temp += scnprintf(buf + temp, DEBUG_BUF_SIZE - temp,
				"#PORT:%d port: %p\n"
				"to_usbhost:    %lu\n"
				"to_modem:      %lu\n"
				"cpkt_drp_cnt:  %lu\n"
				"DTR:           %s\n",
				i, port,
				port->to_host, port->to_modem,
				port->drp_cpkt_cnt,
				port->cbits_tomodem ? "HIGH" : "LOW");

		spin_unlock_irqrestore(&port->port_lock, flags);
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);

	kfree(buf);

	return ret;
}

static ssize_t ghsuart_ctrl_reset_stats(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct ghsuart_ctrl_port	*port;
	int			i;
	unsigned long		flags;

	for (i = 0; i < num_ctrl_ports; i++) {
		port = ghsuart_ctrl_ports[i].port;
		if (!port)
			continue;

		spin_lock_irqsave(&port->port_lock, flags);
		port->to_host = 0;
		port->to_modem = 0;
		port->drp_cpkt_cnt = 0;
		spin_unlock_irqrestore(&port->port_lock, flags);
	}
	return count;
}

static const struct file_operations ghsuart_ctrl_stats_ops = {
	.read = ghsuart_ctrl_read_stats,
	.write = ghsuart_ctrl_reset_stats,
};

static struct dentry	*ghsuart_ctrl_dent;
static int ghsuart_ctrl_debugfs_init(void)
{
	struct dentry	*ghsuart_ctrl_dfile;

	ghsuart_ctrl_dent = debugfs_create_dir("ghsuart_ctrl_xport", 0);
	if (!ghsuart_ctrl_dent || IS_ERR(ghsuart_ctrl_dent))
		return -ENODEV;

	ghsuart_ctrl_dfile =
		debugfs_create_file("status", S_IRUGO | S_IWUSR,
				ghsuart_ctrl_dent, 0, &ghsuart_ctrl_stats_ops);
	if (!ghsuart_ctrl_dfile || IS_ERR(ghsuart_ctrl_dfile)) {
		debugfs_remove(ghsuart_ctrl_dent);
		ghsuart_ctrl_dent = NULL;
		return -ENODEV;
	}
	return 0;
}

static void ghsuart_ctrl_debugfs_exit(void)
{
	debugfs_remove_recursive(ghsuart_ctrl_dent);
}
#else
static int ghsuart_ctrl_debugfs_init(void) { return 0; }
static void ghsuart_ctrl_debugfs_exit(void) {}
#endif

static int __init ghsuart_ctrl_init(void)
{
	int ret;

	ret = ghsuart_ctrl_debugfs_init();
	if (ret) {
		pr_debug("mode debugfs file is not available\n");
		return ret;
	}
	return 0;
}
module_init(ghsuart_ctrl_init);

static void __exit ghsuart_ctrl_exit(void)
{
	ghsuart_ctrl_debugfs_exit();
}
module_exit(ghsuart_ctrl_exit);

MODULE_DESCRIPTION("HSUART control xport for RmNet");
MODULE_LICENSE("GPL v2");
