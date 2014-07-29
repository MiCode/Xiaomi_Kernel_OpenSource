/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/usb/usb_ctrl_qti.h>


#include "u_rmnet.h"
#include "usb_gadget_xport.h"

#define RMNET_CTRL_QTI_NAME "rmnet_ctrl"

struct qti_ctrl_port {
	void		*port_usb;
	char		name[sizeof(RMNET_CTRL_QTI_NAME) + 2];
	struct miscdevice ctrl_device;

	bool		is_open;
	int index;
	unsigned	intf;
	int		ipa_prod_idx;
	int		ipa_cons_idx;
	enum peripheral_ep_type	ep_type;

	atomic_t	connected;
	atomic_t	line_state;

	atomic_t	open_excl;
	atomic_t	read_excl;
	atomic_t	write_excl;
	atomic_t	ioctl_excl;

	wait_queue_head_t	read_wq;

	struct list_head	cpkt_req_q;

	spinlock_t	lock;
	enum gadget_type	gtype;
};
static struct qti_ctrl_port *ctrl_port[NR_QTI_PORTS];

static inline int qti_ctrl_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -EBUSY;
	}
}

static inline void qti_ctrl_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static void qti_ctrl_queue_notify(struct qti_ctrl_port *port)
{
	unsigned long		flags;
	struct rmnet_ctrl_pkt	*cpkt = NULL;

	pr_debug("%s: Queue empty packet for QTI for port%d",
		 __func__, port->index);

	spin_lock_irqsave(&port->lock, flags);
	if (!port->is_open) {
		pr_err("%s: rmnet ctrl file handler %p is not open",
			   __func__, port);
		spin_unlock_irqrestore(&port->lock, flags);
		return;
	}

	cpkt = alloc_rmnet_ctrl_pkt(0, GFP_ATOMIC);
	if (!cpkt) {
		pr_err("%s: Unable to allocate reset function pkt\n", __func__);
		spin_unlock_irqrestore(&port->lock, flags);
		return;
	}

	list_add_tail(&cpkt->list, &port->cpkt_req_q);
	spin_unlock_irqrestore(&port->lock, flags);

	pr_debug("%s: Wake up read queue", __func__);
	wake_up(&port->read_wq);
}

static int gqti_ctrl_send_cpkt_tomodem(u8 portno, void *buf, size_t len)
{
	unsigned long		flags;
	struct qti_ctrl_port	*port;
	struct rmnet_ctrl_pkt *cpkt;

	if (len > MAX_QTI_PKT_SIZE) {
		pr_err("given pkt size too big:%zu > max_pkt_size:%d\n",
				len, MAX_QTI_PKT_SIZE);
		return -EINVAL;
	}

	if (portno >= NR_QTI_PORTS) {
		pr_err("%s: Invalid QTI port %d\n", __func__, portno);
		return -ENODEV;
	}
	port = ctrl_port[portno];

	cpkt = alloc_rmnet_ctrl_pkt(len, GFP_ATOMIC);
	if (IS_ERR(cpkt)) {
		pr_err("%s: Unable to allocate ctrl pkt\n", __func__);
		return -ENOMEM;
	}

	memcpy(cpkt->buf, buf, len);
	cpkt->len = len;

	pr_debug("%s: Add to cpkt_req_q packet with len = %zu\n",
			__func__, len);
	spin_lock_irqsave(&port->lock, flags);

	/* drop cpkt if port is not open */
	if (!port->is_open) {
		pr_debug("rmnet file handler %p(index=%d) is not open",
		       port, port->index);
		spin_unlock_irqrestore(&port->lock, flags);
		free_rmnet_ctrl_pkt(cpkt);
		return 0;
	}

	list_add_tail(&cpkt->list, &port->cpkt_req_q);
	spin_unlock_irqrestore(&port->lock, flags);

	/* wakeup read thread */
	pr_debug("%s: Wake up read queue", __func__);
	wake_up(&port->read_wq);

	return 0;
}

static void
gqti_ctrl_notify_modem(void *gptr, u8 portno, int val)
{
	struct qti_ctrl_port *port;

	if (portno >= NR_QTI_PORTS) {
		pr_err("%s: Invalid QTI port %d\n", __func__, portno);
		return;
	}
	port = ctrl_port[portno];

	atomic_set(&port->line_state, val);

	/* send 0 len pkt to qti to notify state change */
	qti_ctrl_queue_notify(port);
}

#define BAM_DMUX_CHANNEL_ID 8
int gqti_ctrl_connect(void *gr, u8 port_num, unsigned intf,
			enum transport_type dxport, enum gadget_type gtype)
{
	struct qti_ctrl_port	*port;
	struct grmnet *g_rmnet = NULL;
	unsigned long flags;

	pr_debug("%s: grmnet:%p\n", __func__, gr);
	if (port_num >= NR_QTI_PORTS) {
		pr_err("%s: Invalid QTI port %d\n", __func__, port_num);
		return -ENODEV;
	}

	if (gtype != USB_GADGET_RMNET) {
		pr_err("%s(): unrecognized gadget type(%d).\n",
						__func__, gtype);
		return -EINVAL;
	}

	port = ctrl_port[port_num];

	if (!gr || !port) {
		pr_err("%s: grmnet port is null\n", __func__);
		return -ENODEV;
	}


	spin_lock_irqsave(&port->lock, flags);
	port->gtype = gtype;
	port->port_usb = gr;
	if (dxport == USB_GADGET_XPORT_BAM) {
		/*
		 * BAM-DMUX data transport is used for RMNET
		 * on some targets where IPA is not available.
		 * Set endpoint type as BAM-DMUX and interface
		 * id as channel number. This information is
		 * sent to user space via EP_LOOKUP ioctl.
		 *
		 * The BAM data transport driver supports only
		 * 1 BAM channel and the number is fixed so far
		 * on all targets. This number needs to be same
		 * as the bam_ch_ids defined in u_bam.c.
		 *
		 */
		port->ep_type = DATA_EP_TYPE_BAM_DMUX;
		port->intf = BAM_DMUX_CHANNEL_ID;
		port->ipa_prod_idx = 0;
		port->ipa_cons_idx = 0;
	} else {
		port->ep_type = DATA_EP_TYPE_HSUSB;
		port->intf = intf;
	}

	g_rmnet = (struct grmnet *)gr;
	g_rmnet->send_encap_cmd = gqti_ctrl_send_cpkt_tomodem;
	g_rmnet->notify_modem = gqti_ctrl_notify_modem;

	spin_unlock_irqrestore(&port->lock, flags);

	atomic_set(&port->connected, 1);
	wake_up(&port->read_wq);

	if (g_rmnet && g_rmnet->connect)
		g_rmnet->connect(port->port_usb);

	return 0;
}

void gqti_ctrl_disconnect(void *gr, u8 port_num)
{
	struct qti_ctrl_port	*port;
	unsigned long		flags;
	struct rmnet_ctrl_pkt	*cpkt;
	struct grmnet *g_rmnet = NULL;

	pr_debug("%s: grmnet:%p\n", __func__, gr);

	if (port_num >= NR_QTI_PORTS) {
		pr_err("%s: Invalid QTI port %d\n", __func__, port_num);
		return;
	}

	port = ctrl_port[port_num];

	if (!gr || !port) {
		pr_err("%s: grmnet port is null\n", __func__);
		return;
	}

	if (port->gtype != USB_GADGET_RMNET) {
		pr_err("%s(): unrecognized gadget type(%d).\n",
					__func__, port->gtype);
		return;
	}

	g_rmnet = (struct grmnet *)gr;
	g_rmnet->disconnect(port->port_usb);

	atomic_set(&port->connected, 0);
	atomic_set(&port->line_state, 0);
	spin_lock_irqsave(&port->lock, flags);
	port->port_usb = NULL;

	if (g_rmnet) {
		g_rmnet->send_encap_cmd = NULL;
		g_rmnet->notify_modem = NULL;
	}

	while (!list_empty(&port->cpkt_req_q)) {
		cpkt = list_first_entry(&port->cpkt_req_q,
					struct rmnet_ctrl_pkt, list);

		list_del(&cpkt->list);
		free_rmnet_ctrl_pkt(cpkt);
	}

	spin_unlock_irqrestore(&port->lock, flags);

	/* send 0 len pkt to qti to notify state change */
	qti_ctrl_queue_notify(port);
}

void gqti_ctrl_update_ipa_pipes(void *gr, u8 port_num, u32 ipa_prod,
							u32 ipa_cons)
{
	struct qti_ctrl_port	*port;

	if (port_num >= NR_QTI_PORTS) {
		pr_err("%s: Invalid QTI port %d\n", __func__, port_num);
		return;
	}

	port = ctrl_port[port_num];

	port->ipa_prod_idx = ipa_prod;
	port->ipa_cons_idx = ipa_cons;

}


static int qti_ctrl_open(struct inode *ip, struct file *fp)
{
	unsigned long		flags;
	struct qti_ctrl_port *port = container_of(fp->private_data,
						struct qti_ctrl_port,
						ctrl_device);

	pr_debug("Open rmnet_ctrl_qti device file name=%s(index=%d)\n",
		port->name, port->index);

	if (qti_ctrl_lock(&port->open_excl)) {
		pr_err("Already opened\n");
		return -EBUSY;
	}

	spin_lock_irqsave(&port->lock, flags);
	port->is_open = true;
	spin_unlock_irqrestore(&port->lock, flags);

	return 0;
}

static int qti_ctrl_release(struct inode *ip, struct file *fp)
{
	unsigned long		flags;
	struct qti_ctrl_port *port = container_of(fp->private_data,
						struct qti_ctrl_port,
						ctrl_device);

	pr_debug("Close rmnet control file");

	spin_lock_irqsave(&port->lock, flags);
	port->is_open = false;
	spin_unlock_irqrestore(&port->lock, flags);

	qti_ctrl_unlock(&port->open_excl);

	return 0;
}

static ssize_t
qti_ctrl_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	struct qti_ctrl_port *port = container_of(fp->private_data,
						struct qti_ctrl_port,
						ctrl_device);
	struct rmnet_ctrl_pkt *cpkt = NULL;
	unsigned long flags;
	int ret = 0;

	pr_debug("%s: Enter(%zu)\n", __func__, count);

	if (count > MAX_QTI_PKT_SIZE) {
		pr_err("Buffer size is too big %zu, should be at most %d\n",
			count, MAX_QTI_PKT_SIZE);
		return -EINVAL;
	}

	if (qti_ctrl_lock(&port->read_excl)) {
		pr_err("Previous reading is not finished yet\n");
		return -EBUSY;
	}

	/* block until a new packet is available */
	do {
		spin_lock_irqsave(&port->lock, flags);
		if (!list_empty(&port->cpkt_req_q))
			break;
		spin_unlock_irqrestore(&port->lock, flags);

		pr_debug("%s: Requests list is empty. Wait.\n", __func__);
		ret = wait_event_interruptible(port->read_wq,
					!list_empty(&port->cpkt_req_q));
		if (ret < 0) {
			pr_debug("Waiting failed\n");
			qti_ctrl_unlock(&port->read_excl);
			return -ERESTARTSYS;
		}
	} while (1);

	cpkt = list_first_entry(&port->cpkt_req_q, struct rmnet_ctrl_pkt,
							list);
	list_del(&cpkt->list);
	spin_unlock_irqrestore(&port->lock, flags);

	if (cpkt->len > count) {
		pr_err("cpkt size too big:%d > buf size:%zu\n",
				cpkt->len, count);
		qti_ctrl_unlock(&port->read_excl);
		free_rmnet_ctrl_pkt(cpkt);
		return -ENOMEM;
	}

	pr_debug("%s: cpkt size:%d\n", __func__, cpkt->len);


	qti_ctrl_unlock(&port->read_excl);

	ret = copy_to_user(buf, cpkt->buf, cpkt->len);
	if (ret) {
		pr_err("copy_to_user failed: err %d\n", ret);
		ret = -EFAULT;
	} else {
		pr_debug("%s: copied %d bytes to user\n", __func__, cpkt->len);
		ret = cpkt->len;
	}

	free_rmnet_ctrl_pkt(cpkt);

	return ret;
}

static ssize_t
qti_ctrl_write(struct file *fp, const char __user *buf, size_t count,
		   loff_t *pos)
{
	struct qti_ctrl_port *port = container_of(fp->private_data,
						struct qti_ctrl_port,
						ctrl_device);
	void *kbuf;
	unsigned long flags;
	int ret = 0;
	struct grmnet *g_rmnet = NULL;

	pr_debug("%s: Enter(%zu) port_index=%d", __func__, count, port->index);

	if (!count) {
		pr_debug("zero length ctrl pkt\n");
		return -EINVAL;
	}

	if (count > MAX_QTI_PKT_SIZE) {
		pr_debug("given pkt size too big:%zu > max_pkt_size:%d\n",
				count, MAX_QTI_PKT_SIZE);
		return -EINVAL;
	}

	if (qti_ctrl_lock(&port->write_excl)) {
		pr_err("Previous writing not finished yet\n");
		return -EBUSY;
	}

	if (!atomic_read(&port->connected)) {
		pr_debug("USB cable not connected\n");
		qti_ctrl_unlock(&port->write_excl);
		return -EPIPE;
	}

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf) {
		pr_err("failed to allocate ctrl pkt\n");
		qti_ctrl_unlock(&port->write_excl);
		return -ENOMEM;
	}
	ret = copy_from_user(kbuf, buf, count);
	if (ret) {
		pr_err("copy_from_user failed err:%d\n", ret);
		kfree(kbuf);
		qti_ctrl_unlock(&port->write_excl);
		return -EFAULT;
	}

	spin_lock_irqsave(&port->lock, flags);
	if (port && port->port_usb) {
		if (port->gtype == USB_GADGET_RMNET) {
			g_rmnet = (struct grmnet *)port->port_usb;
		} else {
			spin_unlock_irqrestore(&port->lock, flags);
			pr_err("%s(): unrecognized gadget type(%d).\n",
						__func__, port->gtype);
			return -EINVAL;
		}

		if (g_rmnet && g_rmnet->send_cpkt_response) {
			ret = g_rmnet->send_cpkt_response(port->port_usb,
							kbuf, count);
			if (ret)
				pr_err("%d failed to send ctrl packet.\n", ret);
		} else {
			pr_err("send_cpkt_response callback is NULL\n");
			ret = -EINVAL;
		}
	}

	spin_unlock_irqrestore(&port->lock, flags);
	kfree(kbuf);
	qti_ctrl_unlock(&port->write_excl);

	pr_debug("%s: Exit(%zu)", __func__, count);
	return (ret) ? ret : count;
}

static long qti_ctrl_ioctl(struct file *fp, unsigned cmd, unsigned long arg)
{
	struct qti_ctrl_port *port = container_of(fp->private_data,
						struct qti_ctrl_port,
						ctrl_device);
	struct grmnet *gr = NULL;
	struct ep_info info;
	int val, ret = 0;

	pr_debug("%s: Received command %d for gtype:%d\n",
				__func__, cmd, port->gtype);

	if (qti_ctrl_lock(&port->ioctl_excl))
		return -EBUSY;

	switch (cmd) {
	case QTI_CTRL_MODEM_OFFLINE:
		if (port && port->port_usb)
			gr = port->port_usb;

		if (gr && gr->disconnect)
			gr->disconnect(gr);
		break;
	case QTI_CTRL_MODEM_ONLINE:
		if (port && port->port_usb)
			gr = port->port_usb;

		if (gr && gr->connect)
			gr->connect(gr);
		break;
	case QTI_CTRL_GET_LINE_STATE:
		val = atomic_read(&port->line_state);
		ret = copy_to_user((void __user *)arg, &val, sizeof(val));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_debug("%s: Sent line_state: %d for gtype:%d\n", __func__,
				atomic_read(&port->line_state), port->gtype);
		break;
	case QTI_CTRL_EP_LOOKUP:
		val = atomic_read(&port->connected);
		if (!val) {
			pr_err("EP_LOOKUP failed - not connected");
			ret = -EAGAIN;
			break;
		}

		info.ph_ep_info.ep_type = port->ep_type;
		info.ph_ep_info.peripheral_iface_id = port->intf;
		info.ipa_ep_pair.cons_pipe_num = port->ipa_cons_idx;
		info.ipa_ep_pair.prod_pipe_num = port->ipa_prod_idx;

		pr_debug("%s(): gtype:%d ep_type:%d intf:%d\n",
				__func__, port->gtype, info.ph_ep_info.ep_type,
				info.ph_ep_info.peripheral_iface_id);

		pr_debug("%s(): ipa_cons_idx:%d ipa_prod_idx:%d\n",
				__func__, info.ipa_ep_pair.cons_pipe_num,
				info.ipa_ep_pair.prod_pipe_num);

		ret = copy_to_user((void __user *)arg, &info,
			sizeof(info));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		break;
	default:
		pr_err("wrong parameter");
		ret = -EINVAL;
	}

	qti_ctrl_unlock(&port->ioctl_excl);

	return ret;
}

static unsigned int qti_ctrl_poll(struct file *file, poll_table *wait)
{
	struct qti_ctrl_port *port = container_of(file->private_data,
						struct qti_ctrl_port,
						ctrl_device);
	unsigned long flags;
	unsigned int mask = 0;

	if (!port) {
		pr_err("%s on a NULL device\n", __func__);
		return POLLERR;
	}

	poll_wait(file, &port->read_wq, wait);

	spin_lock_irqsave(&port->lock, flags);
	if (!list_empty(&port->cpkt_req_q)) {
		mask |= POLLIN | POLLRDNORM;
		pr_debug("%s sets POLLIN for rmnet_ctrl_qti_port\n", __func__);
	}
	spin_unlock_irqrestore(&port->lock, flags);

	return mask;
}

/* file operations for rmnet device /dev/rmnet_ctrl */
static const struct file_operations qti_ctrl_fops = {
	.owner = THIS_MODULE,
	.open = qti_ctrl_open,
	.release = qti_ctrl_release,
	.read = qti_ctrl_read,
	.write = qti_ctrl_write,
	.unlocked_ioctl = qti_ctrl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = qti_ctrl_ioctl,
#endif
	.poll = qti_ctrl_poll,
};

static int __init gqti_ctrl_init(void)
{
	int ret, i, sz = sizeof(RMNET_CTRL_QTI_NAME)+2;
	struct qti_ctrl_port *port = NULL;

	for (i = 0; i < NR_QTI_PORTS; i++) {
		port = kzalloc(sizeof(struct qti_ctrl_port), GFP_KERNEL);
		if (!port) {
			pr_err("Failed to allocate rmnet control device\n");
			ret = -ENOMEM;
			goto fail_init;
		}

		INIT_LIST_HEAD(&port->cpkt_req_q);
		spin_lock_init(&port->lock);

		atomic_set(&port->open_excl, 0);
		atomic_set(&port->read_excl, 0);
		atomic_set(&port->write_excl, 0);
		atomic_set(&port->ioctl_excl, 0);
		atomic_set(&port->connected, 0);
		atomic_set(&port->line_state, 0);

		init_waitqueue_head(&port->read_wq);

		ctrl_port[i] = port;
		port->index = i;
		port->ipa_prod_idx = -1;
		port->ipa_cons_idx = -1;

		if (i == 0)
			strlcat(port->name, RMNET_CTRL_QTI_NAME, sz);
		else
			snprintf(port->name, sz, "%s%d",
					RMNET_CTRL_QTI_NAME, i);

		port->ctrl_device.name = port->name;
		port->ctrl_device.fops = &qti_ctrl_fops;
		port->ctrl_device.minor = MISC_DYNAMIC_MINOR;

		ret = misc_register(&port->ctrl_device);
		if (ret) {
			pr_err("rmnet control driver failed to register");
			goto fail_init;
		}
	}

	return ret;

fail_init:
	for (i--; i >= 0; i--) {
		misc_deregister(&ctrl_port[i]->ctrl_device);
		kfree(ctrl_port[i]);
		ctrl_port[i] = NULL;
	}
	return ret;
}
module_init(gqti_ctrl_init);

static void __exit gqti_ctrl_cleanup(void)
{
	int i;

	for (i = 0; i < NR_QTI_PORTS; i++) {
		misc_deregister(&ctrl_port[i]->ctrl_device);
		kfree(ctrl_port[i]);
		ctrl_port[i] = NULL;
	}
}
module_exit(gqti_ctrl_cleanup);
