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
#include <linux/usb/rmnet_ctrl_qti.h>


#include "u_rmnet.h"

#define RMNET_CTRL_QTI_NAME "rmnet_ctrl"

struct rmnet_ctrl_qti_port {
	struct grmnet	*port_usb;
	char		name[sizeof(RMNET_CTRL_QTI_NAME) + 2];
	struct miscdevice rmnet_device;

	bool		is_open;
	int index;
	unsigned	intf;
	int		ipa_prod_idx;
	int		ipa_cons_idx;

	atomic_t	connected;
	atomic_t	line_state;

	atomic_t	open_excl;
	atomic_t	read_excl;
	atomic_t	write_excl;
	atomic_t	ioctl_excl;

	wait_queue_head_t	read_wq;

	struct list_head	cpkt_req_q;

	spinlock_t	lock;
};
static struct rmnet_ctrl_qti_port *ctrl_port[NR_QTI_PORTS];

static inline int rmnet_ctrl_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -EBUSY;
	}
}

static inline void rmnet_ctrl_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static void rmnet_ctrl_queue_notify(struct rmnet_ctrl_qti_port *port)
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

static int grmnet_ctrl_qti_send_cpkt_tomodem(u8 portno,
	void *buf, size_t len)
{
	unsigned long		flags;
	struct rmnet_ctrl_qti_port	*port;
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
	struct rmnet_ctrl_qti_port *port;

	if (portno >= NR_QTI_PORTS) {
		pr_err("%s: Invalid QTI port %d\n", __func__, portno);
		return;
	}
	port = ctrl_port[portno];

	atomic_set(&port->line_state, val);

	/* send 0 len pkt to qti to notify state change */
	rmnet_ctrl_queue_notify(port);
}

int gqti_ctrl_connect(struct grmnet *gr, u8 port_num, unsigned intf)
{
	struct rmnet_ctrl_qti_port	*port;
	unsigned long		flags;

	pr_debug("%s: grmnet:%p\n", __func__, gr);
	if (port_num >= NR_QTI_PORTS) {
		pr_err("%s: Invalid QTI port %d\n", __func__, port_num);
		return -ENODEV;
	}
	port = ctrl_port[port_num];

	if (!gr || !port) {
		pr_err("%s: grmnet port is null\n", __func__);
		return -ENODEV;
	}

	spin_lock_irqsave(&port->lock, flags);
	port->port_usb = gr;
	port->intf = intf;
	gr->send_encap_cmd = grmnet_ctrl_qti_send_cpkt_tomodem;
	gr->notify_modem = gqti_ctrl_notify_modem;
	spin_unlock_irqrestore(&port->lock, flags);

	atomic_set(&port->connected, 1);
	wake_up(&port->read_wq);

	if (port->port_usb && port->port_usb->connect)
		port->port_usb->connect(port->port_usb);

	return 0;
}

void gqti_ctrl_disconnect(struct grmnet *gr, u8 port_num)
{
	struct rmnet_ctrl_qti_port	*port;
	unsigned long		flags;
	struct rmnet_ctrl_pkt	*cpkt;

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

	if (port->port_usb && port->port_usb->disconnect)
		port->port_usb->disconnect(port->port_usb);

	atomic_set(&port->connected, 0);
	atomic_set(&port->line_state, 0);
	spin_lock_irqsave(&port->lock, flags);
	port->port_usb = 0;
	gr->send_encap_cmd = 0;
	gr->notify_modem = 0;

	while (!list_empty(&port->cpkt_req_q)) {
		cpkt = list_first_entry(&port->cpkt_req_q,
					struct rmnet_ctrl_pkt, list);

		list_del(&cpkt->list);
		free_rmnet_ctrl_pkt(cpkt);
	}

	spin_unlock_irqrestore(&port->lock, flags);

	/* send 0 len pkt to qti to notify state change */
	rmnet_ctrl_queue_notify(port);
}

void gqti_ctrl_update_ipa_pipes(struct grmnet *gr, u8 port_num, u32 ipa_prod,
				u32 ipa_cons)
{
	struct rmnet_ctrl_qti_port	*port;

	if (port_num >= NR_QTI_PORTS) {
		pr_err("%s: Invalid QTI port %d\n", __func__, port_num);
		return;
	}

	port = ctrl_port[port_num];

	port->ipa_prod_idx = ipa_prod;
	port->ipa_cons_idx = ipa_cons;

}


static int rmnet_ctrl_open(struct inode *ip, struct file *fp)
{
	unsigned long		flags;
	struct rmnet_ctrl_qti_port *port = container_of(fp->private_data,
						struct rmnet_ctrl_qti_port,
						rmnet_device);

	pr_debug("Open rmnet_ctrl_qti device file name=%s(index=%d)\n",
		port->name, port->index);

	if (rmnet_ctrl_lock(&port->open_excl)) {
		pr_err("Already opened\n");
		return -EBUSY;
	}

	spin_lock_irqsave(&port->lock, flags);
	port->is_open = true;
	spin_unlock_irqrestore(&port->lock, flags);

	return 0;
}

static int rmnet_ctrl_release(struct inode *ip, struct file *fp)
{
	unsigned long		flags;
	struct rmnet_ctrl_qti_port *port = container_of(fp->private_data,
						struct rmnet_ctrl_qti_port,
						rmnet_device);

	pr_debug("Close rmnet control file");

	spin_lock_irqsave(&port->lock, flags);
	port->is_open = false;
	spin_unlock_irqrestore(&port->lock, flags);

	rmnet_ctrl_unlock(&port->open_excl);

	return 0;
}

static ssize_t
rmnet_ctrl_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	struct rmnet_ctrl_qti_port *port = container_of(fp->private_data,
						struct rmnet_ctrl_qti_port,
						rmnet_device);
	struct rmnet_ctrl_pkt *cpkt = NULL;
	unsigned long flags;
	int ret = 0;

	pr_debug("%s: Enter(%zu)\n", __func__, count);

	if (count > MAX_QTI_PKT_SIZE) {
		pr_err("Buffer size is too big %zu, should be at most %d\n",
			count, MAX_QTI_PKT_SIZE);
		return -EINVAL;
	}

	if (rmnet_ctrl_lock(&port->read_excl)) {
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
			rmnet_ctrl_unlock(&port->read_excl);
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
		rmnet_ctrl_unlock(&port->read_excl);
		free_rmnet_ctrl_pkt(cpkt);
		return -ENOMEM;
	}

	pr_debug("%s: cpkt size:%d\n", __func__, cpkt->len);


	rmnet_ctrl_unlock(&port->read_excl);

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
rmnet_ctrl_write(struct file *fp, const char __user *buf, size_t count,
		   loff_t *pos)
{
	struct rmnet_ctrl_qti_port *port = container_of(fp->private_data,
						struct rmnet_ctrl_qti_port,
						rmnet_device);
	void *kbuf;
	unsigned long flags;
	int ret = 0;

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

	if (rmnet_ctrl_lock(&port->write_excl)) {
		pr_err("Previous writing not finished yet\n");
		return -EBUSY;
	}

	if (!atomic_read(&port->connected)) {
		pr_debug("USB cable not connected\n");
		rmnet_ctrl_unlock(&port->write_excl);
		return -EPIPE;
	}

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf) {
		pr_err("failed to allocate ctrl pkt\n");
		rmnet_ctrl_unlock(&port->write_excl);
		return -ENOMEM;
	}
	ret = copy_from_user(kbuf, buf, count);
	if (ret) {
		pr_err("copy_from_user failed err:%d\n", ret);
		kfree(kbuf);
		rmnet_ctrl_unlock(&port->write_excl);
		return -EFAULT;
	}

	spin_lock_irqsave(&port->lock, flags);
	if (port->port_usb && port->port_usb->send_cpkt_response) {
		ret = port->port_usb->send_cpkt_response(port->port_usb,
							kbuf, count);
		if (ret) {
			pr_err("failed to send ctrl packet. error=%d\n", ret);
			spin_unlock_irqrestore(&port->lock, flags);
			kfree(kbuf);
			rmnet_ctrl_unlock(&port->write_excl);
			return ret;
		}
	} else {
		pr_err("send_cpkt_response callback is NULL\n");
		spin_unlock_irqrestore(&port->lock, flags);
		kfree(kbuf);
		rmnet_ctrl_unlock(&port->write_excl);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&port->lock, flags);

	kfree(kbuf);
	rmnet_ctrl_unlock(&port->write_excl);

	pr_debug("%s: Exit(%zu)", __func__, count);

	return count;

}

static long rmnet_ctrl_ioctl(struct file *fp, unsigned cmd, unsigned long arg)
{
	struct rmnet_ctrl_qti_port *port = container_of(fp->private_data,
						struct rmnet_ctrl_qti_port,
						rmnet_device);
	struct grmnet *gr = NULL;
	struct ep_info info;
	int val, ret = 0;

	pr_debug("%s: Received command %d", __func__, cmd);

	if (rmnet_ctrl_lock(&port->ioctl_excl))
		return -EBUSY;

	switch (cmd) {
	case FRMNET_CTRL_MODEM_OFFLINE:
		if (port && port->port_usb)
			gr = port->port_usb;

		if (gr && gr->disconnect)
			gr->disconnect(gr);
		break;
	case FRMNET_CTRL_MODEM_ONLINE:
		if (port && port->port_usb)
			gr = port->port_usb;

		if (gr && gr->connect)
			gr->connect(gr);
		break;
	case FRMNET_CTRL_GET_LINE_STATE:
		val = atomic_read(&port->line_state);
		ret = copy_to_user((void __user *)arg, &val, sizeof(val));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_debug("%s: Sent line_state: %d", __func__,
				 atomic_read(&port->line_state));
		break;
	case FRMNET_CTRL_EP_LOOKUP:
		val = atomic_read(&port->connected);
		if (!val) {
			pr_err("EP_LOOKUP failed - not connected");
			ret = -EAGAIN;
			break;
		}

		if (port->ipa_prod_idx == -1 ||  port->ipa_cons_idx == -1) {
			pr_err("EP_LOOKUP failed - ipa pipes were not updated");
			ret = -EAGAIN;
			break;

		}

		info.ph_ep_info.ep_type = DATA_EP_TYPE_HSUSB;
		info.ph_ep_info.peripheral_iface_id = port->intf;
		info.ipa_ep_pair.cons_pipe_num = port->ipa_cons_idx;
		info.ipa_ep_pair.prod_pipe_num = port->ipa_prod_idx;

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

	rmnet_ctrl_unlock(&port->ioctl_excl);

	return ret;
}

static unsigned int rmnet_ctrl_poll(struct file *file, poll_table *wait)
{
	struct rmnet_ctrl_qti_port *port = container_of(file->private_data,
						struct rmnet_ctrl_qti_port,
						rmnet_device);
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
static const struct file_operations rmnet_ctrl_fops = {
	.owner = THIS_MODULE,
	.open = rmnet_ctrl_open,
	.release = rmnet_ctrl_release,
	.read = rmnet_ctrl_read,
	.write = rmnet_ctrl_write,
	.unlocked_ioctl = rmnet_ctrl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = rmnet_ctrl_ioctl,
#endif
	.poll = rmnet_ctrl_poll,
};

static int __init gqti_ctrl_init(void)
{
	int ret, i, sz = sizeof(RMNET_CTRL_QTI_NAME)+2;
	struct rmnet_ctrl_qti_port *port = NULL;

	for (i = 0; i < NR_QTI_PORTS; i++) {
		port = kzalloc(sizeof(struct rmnet_ctrl_qti_port), GFP_KERNEL);
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

		port->rmnet_device.name = port->name;
		port->rmnet_device.fops = &rmnet_ctrl_fops;
		port->rmnet_device.minor = MISC_DYNAMIC_MINOR;

		ret = misc_register(&port->rmnet_device);
		if (ret) {
			pr_err("rmnet control driver failed to register");
			goto fail_init;
		}
	}

	return ret;

fail_init:
	for (i--; i >= 0; i--) {
		misc_deregister(&ctrl_port[i]->rmnet_device);
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
		misc_deregister(&ctrl_port[i]->rmnet_device);
		kfree(ctrl_port[i]);
		ctrl_port[i] = NULL;
	}
}
module_exit(gqti_ctrl_cleanup);
