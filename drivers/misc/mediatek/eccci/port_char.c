/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "ccci_config.h"
#include "ccci_bm.h"
#include "port_proxy.h"
#include "port_char.h"

#define MAX_QUEUE_LENGTH 32

static int dev_char_open(struct inode *inode, struct file *file)
{
	int major = imajor(inode);
	int minor = iminor(inode);
	struct ccci_port *port;
	struct ccci_port *status_poller;

	port = port_proxy_get_port_by_node(major, minor);
	if (atomic_read(&port->usage_cnt))
		return -EBUSY;
	CCCI_NORMAL_LOG(port->md_id, CHAR, "port %s open with flag %X by %s\n", port->name, file->f_flags,
		     current->comm);
	atomic_inc(&port->usage_cnt);
	file->private_data = port;
	nonseekable_open(inode, file);
#ifdef FEATURE_POLL_MD_EN
	if (port->rx_ch == CCCI_MD_LOG_RX && port_proxy_get_md_state(port->port_proxy) == READY) {
		status_poller = port_proxy_get_port_by_channel(port->port_proxy, CCCI_STATUS_RX);
		port_poller_start(status_poller);
	}
#endif
	port_proxy_user_register(port->port_proxy, port);
	return 0;
}

static int dev_char_close(struct inode *inode, struct file *file)
{
	struct ccci_port *port = file->private_data;
	struct sk_buff *skb = NULL;
	unsigned long flags;
	struct ccci_port *status_poller;
	int clear_cnt = 0;

	/* 0. decrease usage count, so when we ask more, the packet can be dropped in recv_request */
	atomic_dec(&port->usage_cnt);
	/* 1. purge Rx request list */
	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL) {
		ccci_free_skb(skb);
		clear_cnt++;
	}
	port->rx_drop_cnt += clear_cnt;
	/*  flush Rx */
	port_proxy_ask_more_req_to_md(port->port_proxy, port);
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	CCCI_NORMAL_LOG(port->md_id, CHAR, "port %s close rx_len=%d empty=%d, clear_cnt=%d, drop=%d\n", port->name,
		     port->rx_skb_list.qlen, skb_queue_empty(&port->rx_skb_list), clear_cnt, port->rx_drop_cnt);
	ccci_event_log("md%d: port %s close rx_len=%d empty=%d, clear_cnt=%d, drop=%d\n", port->md_id, port->name,
		     port->rx_skb_list.qlen, skb_queue_empty(&port->rx_skb_list), clear_cnt, port->rx_drop_cnt);
#ifdef FEATURE_POLL_MD_EN
	if (port->rx_ch == CCCI_MD_LOG_RX && port_proxy_get_md_state(port->port_proxy) == READY) {
		status_poller = port_proxy_get_port_by_channel(port->port_proxy, CCCI_STATUS_RX);
		port_poller_stop(status_poller);
	}
#endif
	port_proxy_user_unregister(port->port_proxy, port);

	return 0;
}

static ssize_t dev_char_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct ccci_port *port = file->private_data;
	struct sk_buff *skb = NULL;
	int ret = 0, read_len = 0, full_req_done = 0;
	unsigned long flags = 0;

READ_START:
	/* 1. get incoming request */
	if (skb_queue_empty(&port->rx_skb_list)) {
		if (!(file->f_flags & O_NONBLOCK)) {
			ret = wait_event_interruptible(port->rx_wq, !skb_queue_empty(&port->rx_skb_list));
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto exit;
			}
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	}
	CCCI_DEBUG_LOG(port->md_id, CHAR, "read on %s for %zu\n", port->name, count);
	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	if (skb_queue_empty(&port->rx_skb_list)) {
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		if (!(file->f_flags & O_NONBLOCK)) {
			goto READ_START;
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	}

	skb = skb_peek(&port->rx_skb_list);
	read_len = skb->len;

	if (count >= read_len) {
		full_req_done = 1;
		__skb_unlink(skb, &port->rx_skb_list);
		/*
		 * here we only ask for more request when rx list is empty. no need to be too gready, because
		 * for most of the case, queue will not stop sending request to port.
		 * actually we just need to ask by ourselves when we rejected requests before. these
		 * rejected requests will staty in queue's buffer and may never get a chance to be handled again.
		 */
		if (port->rx_skb_list.qlen == 0)
			port_proxy_ask_more_req_to_md(port->port_proxy, port);
		if (port->rx_skb_list.qlen < 0) {
			spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
			CCCI_ERROR_LOG(port->md_id, CHAR, "%s:port->rx_skb_list.qlen < 0 %s\n", __func__, port->name);
			return -EFAULT;
		}
	} else {
		read_len = count;
	}
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	if (port->flags & PORT_F_CH_TRAFFIC)
		port_ch_dump(port, 0, skb->data, read_len);
	/* 3. copy to user */
	if (copy_to_user(buf, skb->data, read_len)) {
		CCCI_ERROR_LOG(port->md_id, CHAR, "read on %s, copy to user failed, %d/%zu\n", port->name,
			     read_len, count);
		ret = -EFAULT;
	}
	skb_pull(skb, read_len);
	/* 4. free request */
	if (full_req_done)
		ccci_free_skb(skb);

 exit:
	return ret ? ret : read_len;
}

static ssize_t dev_char_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct ccci_port *port = file->private_data;
	unsigned char blocking = !(file->f_flags & O_NONBLOCK);
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h = NULL;
	size_t actual_count = 0, alloc_size = 0;
	int ret = 0, header_len = 0;

	if (count == 0)
		return -EINVAL;

	if (port->tx_ch == CCCI_MONITOR_CH)
		return -EPERM;

	header_len = sizeof(struct ccci_header) + (port->rx_ch == CCCI_FS_RX ? sizeof(unsigned int) : 0);
	if (port->flags & PORT_F_USER_HEADER) {
		if (count > (CCCI_MTU + header_len)) {
			CCCI_ERROR_LOG(port->md_id, CHAR, "reject packet(size=%zu ), larger than MTU on %s\n",
				     count, port->name);
			return -ENOMEM;
		}
		actual_count = count;
		alloc_size = actual_count;
	} else {
		actual_count = count > CCCI_MTU ? CCCI_MTU : count;
		alloc_size = actual_count + header_len;
	}
	skb = ccci_alloc_skb(alloc_size, 1, blocking);
	if (skb) {
		/* 1. for Tx packet, who issued it should know whether recycle it  or not */
		/* 2. prepare CCCI header, every member of header should be re-write as request may be re-used */
		if (!(port->flags & PORT_F_USER_HEADER)) {
			ccci_h = (struct ccci_header *)skb_put(skb, sizeof(struct ccci_header));
			ccci_h->data[0] = 0;
			ccci_h->data[1] = actual_count + sizeof(struct ccci_header);
			ccci_h->channel = port->tx_ch;
			ccci_h->reserved = 0;
		} else {
			ccci_h = (struct ccci_header *)skb->data;
		}
		/* 3. get user data */
		ret = copy_from_user(skb_put(skb, actual_count), buf, actual_count);
		if (ret)
			goto err_out;
		if (port->flags & PORT_F_USER_HEADER) {
			/* ccci_header provided by user,
			 * For only send ccci_header without additional data case,
			 *	data[0]=CCCI_MAGIC_NUM, data[1]=user_data, ch=tx_channel, reserved=no_use
			 * For send ccci_header with additional data case,
			 *	data[0]=0, data[1]=data_size, ch=tx_channel, reserved=user_data
			 */
			if (actual_count == sizeof(struct ccci_header))
				ccci_h->data[0] = CCCI_MAGIC_NUM;
			else
				ccci_h->data[1] = actual_count;
			ccci_h->channel = port->tx_ch;	/* as EEMCS VA will not fill this filed */
		}
		if (port->rx_ch == CCCI_IPC_RX) {
			ret = port_ipc_write_check_id(port, skb);
			if (ret < 0)
				goto err_out;
			else
				ccci_h->reserved = ret;	/* Unity ID */
		}
		if (port->flags & PORT_F_CH_TRAFFIC) {
			if (port->flags & PORT_F_USER_HEADER)
				port_ch_dump(port, 1, skb->data, actual_count);
			else
				port_ch_dump(port, 1, skb->data + header_len,
					actual_count);
		}

		/* 4. send out */
		/*
		 * for md3, ccci_h->channel will probably change after call send_skb
		 * because md3's channel mapping
		 */
		ret = port_proxy_send_skb_to_md(port->port_proxy, port, skb, blocking);
			/* do NOT reference request after called this, modem may have freed it, unless you get -EBUSY */
#if 0
		if (ccci_h && ccci_h->channel == CCCI_UART2_TX) {
			CCCI_NORMAL_LOG(port->md_id, CHAR,
				"write done on %s, l=%zu r=%d\n", port->name, actual_count, ret);
		}
#endif
		if (ret) {
			if (ret == -EBUSY && !blocking)
				ret = -EAGAIN;
			goto err_out;
		}
		return actual_count;

 err_out:
		CCCI_NORMAL_LOG(port->md_id, CHAR, "write error done on %s, l=%zu r=%d\n",
			 port->name, actual_count, ret);
		ccci_free_skb(skb);
		return ret;
	}
	/* consider this case as non-blocking */
	return -EBUSY;
}

static long dev_char_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct ccci_port *port = file->private_data;
	int ch = port->rx_ch;

	if (ch == CCCI_SMEM_CH) {
		ret = port_smem_ioctl(port, cmd, arg);
	} else if (ch == CCCI_IPC_RX) {
		ret = port_ipc_ioctl(port, cmd, arg);
	} else {
		switch (cmd) {
		case CCCI_IOC_SET_HEADER:
			port->flags |= PORT_F_USER_HEADER;
			break;
		case CCCI_IOC_CLR_HEADER:
			port->flags &= ~PORT_F_USER_HEADER;
			break;
		default:
			ret = port_proxy_user_ioctl(port->port_proxy, ch, cmd, arg);
			break;
		}
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long dev_char_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ccci_port *port = filp->private_data;
	int md_id = port->md_id;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		CCCI_ERROR_LOG(md_id, CHAR, "dev_char_compat_ioctl(!filp->f_op || !filp->f_op->unlocked_ioctl)\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case CCCI_IOC_PCM_BASE_ADDR:
	case CCCI_IOC_PCM_LEN:
	case CCCI_IOC_ALLOC_MD_LOG_MEM:
	case CCCI_IOC_FORCE_FD:
	case CCCI_IOC_AP_ENG_BUILD:
	case CCCI_IOC_GET_MD_MEM_SIZE:
		CCCI_ERROR_LOG(md_id, CHAR, "dev_char_compat_ioctl deprecated cmd(%d)\n", cmd);
		return 0;
	default:
		return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
	}
}
#endif
unsigned int dev_char_poll(struct file *fp, struct poll_table_struct *poll)
{
	struct ccci_port *port = fp->private_data;
	unsigned int mask = 0;
	int md_id = port->md_id;
	int md_state = port_proxy_get_md_state(port->port_proxy);

	CCCI_DEBUG_LOG(md_id, CHAR, "poll on %s\n", port->name);
	if (port->rx_ch == CCCI_IPC_RX) {
		mask = port_ipc_poll(fp, poll);
	} else {
		poll_wait(fp, &port->rx_wq, poll);
		/* TODO: lack of poll wait for Tx */
		if (!skb_queue_empty(&port->rx_skb_list))
			mask |= POLLIN | POLLRDNORM;
		if (port_proxy_write_room_to_md(port->port_proxy, port) > 0)
			mask |= POLLOUT | POLLWRNORM;
		if (port->rx_ch == CCCI_UART1_RX &&
		    md_state != READY &&
			md_state != EXCEPTION) {
			mask |= POLLERR;	/* notify MD logger to save its log before md_init kills it */
			CCCI_NORMAL_LOG(md_id, CHAR, "poll error for MD logger at state %d,mask=%d\n",
				     md_state, mask);
		}
	}

	return mask;
}

static int dev_char_mmap(struct file *fp, struct vm_area_struct *vma)
{
	struct ccci_port *port = fp->private_data;

	CCCI_DEBUG_LOG(port->md_id, CHAR, "mmap on %s\n", port->name);
	if (port->rx_ch == CCCI_SMEM_CH)
		return port_smem_mmap(port, vma);

	return -EPERM;
}

static const struct file_operations char_dev_fops = {
	.owner = THIS_MODULE,
	.open = &dev_char_open,
	.read = &dev_char_read,
	.write = &dev_char_write,
	.release = &dev_char_close,
	.unlocked_ioctl = &dev_char_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = &dev_char_compat_ioctl,
#endif
	.poll = &dev_char_poll,
	.mmap = &dev_char_mmap,
};

static int port_char_init(struct ccci_port *port)
{
	struct cdev *dev;
	int ret = 0;
	int md_id = port->md_id;

	CCCI_DEBUG_LOG(md_id, CHAR, "char port %s is initializing\n", port->name);
	dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	cdev_init(dev, &char_dev_fops);
	dev->owner = THIS_MODULE;
	port->rx_length_th = MAX_QUEUE_LENGTH;
	if (port->rx_ch == CCCI_IPC_RX)
		port_ipc_init(port);	/* this will change port->minor, call it before register device */
	else if (port->rx_ch == CCCI_SMEM_CH)
		port_smem_init(port);   /* this will change port->minor, call it before register device */
	else
		port->private_data = dev;	/* not using */
	ret = cdev_add(dev, MKDEV(port->major, port->minor_base + port->minor), 1);
	ret = ccci_register_dev_node(port->name, port->major, port->minor_base + port->minor);
	port->interception = 0;
	port->skb_from_pool = 1;
	port->flags |= PORT_F_ADJUST_HEADER;

	if (port->rx_ch == CCCI_UART2_RX ||
		port->rx_ch == CCCI_C2K_AT ||
		port->rx_ch == CCCI_C2K_AT2 ||
		port->rx_ch == CCCI_C2K_AT3 ||
		port->rx_ch == CCCI_C2K_AT4 ||
		port->rx_ch == CCCI_C2K_AT5 ||
		port->rx_ch == CCCI_C2K_AT6 ||
		port->rx_ch == CCCI_C2K_AT7 ||
		port->rx_ch == CCCI_C2K_AT8)
		port->flags |= PORT_F_CH_TRAFFIC;

	return ret;
}

#ifdef CONFIG_MTK_ECCCI_C2K
static int c2k_req_push_to_usb(struct ccci_port *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	struct ccci_header *ccci_h = NULL;
	int read_len, read_count, ret = 0;
	int c2k_ch_id;

	if (port->rx_ch == CCCI_C2K_PPP_DATA)
		c2k_ch_id = DATA_PPP_CH_C2K-1;
	else if (port->rx_ch == CCCI_MD_LOG_RX)
		c2k_ch_id = MDLOG_CH_C2K-2;
	else {
		ret = -ENODEV;
		CCCI_ERROR_LOG(md_id, CHAR, "Err: wrong ch_id(%d) from usb bypass\n", port->rx_ch);
		return ret;
	}

	/* caculate available data */
	ccci_h = (struct ccci_header *)skb->data;
	read_len = skb->len - sizeof(struct ccci_header);
	/* remove CCCI header */
	skb_pull(skb, sizeof(struct ccci_header));

retry_push:
	/* push to usb */
	read_count = rawbulk_push_upstream_buffer(c2k_ch_id, skb->data, read_len);
	CCCI_DEBUG_LOG(md_id, CHAR, "data push to usb bypass (ch%d)(%d)\n", port->rx_ch, read_count);

	if (read_count > 0) {
		skb_pull(skb, read_count);
		read_len -= read_count;
		if (read_len > 0)
			goto retry_push;
		else if (read_len == 0)
			ccci_free_skb(skb);
		else if (read_len < 0)
			CCCI_ERROR_LOG(md_id, CHAR, "read_len error, check why come here\n");
	} else {
		CCCI_NORMAL_LOG(md_id, CHAR, "usb buf full\n");
		msleep(20);
		goto retry_push;
	}

	return read_len;

}
#endif

static int port_char_recv_skb(struct ccci_port *port, struct sk_buff *skb)
{
	int md_id = port->md_id;

	if (!atomic_read(&port->usage_cnt) &&
		(port->rx_ch != CCCI_UART2_RX && port->rx_ch != CCCI_C2K_AT && port->rx_ch != CCCI_PCM_RX &&
			port->rx_ch != CCCI_FS_RX && port->rx_ch != CCCI_RPC_RX && port->rx_ch != CCCI_MONITOR_CH))
		return -CCCI_ERR_DROP_PACKET;

#ifdef CONFIG_MTK_ECCCI_C2K
	if ((md_id == MD_SYS3) && port->interception) {
		c2k_req_push_to_usb(port, skb);
		return 0;
	}
#endif
	CCCI_DEBUG_LOG(md_id, CHAR, "recv on %s, len=%d\n", port->name, port->rx_skb_list.qlen);
	return port_recv_skb(port, skb);
}

static int port_char_recv_match(struct ccci_port *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;

	if (ccci_h->channel == port->rx_ch) {
		if (unlikely(port->rx_ch == CCCI_IPC_RX))
			return port_ipc_recv_match(port, skb);
		if (unlikely(port->rx_ch == CCCI_RPC_RX))
			return (port_rpc_recv_match(port, skb) == 0);
		return 1;
	}
	return 0;
}

static void port_char_md_state_notice(struct ccci_port *port, MD_STATE state)
{
	if (unlikely(port->rx_ch == CCCI_IPC_RX))
		port_ipc_md_state_notice(port, state);
	if (port->rx_ch == CCCI_UART1_RX && state == GATED)
		wake_up_all(&port->rx_wq);	/* check poll function */
	if (port->rx_ch == CCCI_SMEM_CH && state == RX_IRQ)
		port_smem_rx_wakeup(port);
}
void port_char_dump_info(struct ccci_port *port, unsigned int flag)
{
	if (port == NULL) {
		CCCI_ERROR_LOG(0, CHAR, "%s: port==NULL\n", __func__);
		return;
	}
	if (atomic_read(&port->usage_cnt) == 0)
		return;
	if (port->flags & PORT_F_CH_TRAFFIC)
		CCCI_REPEAT_LOG(port->md_id, CHAR, "CHR:(%d):%dR(%d,%d,%d):%dT(%d)\n",
			port->flags,
			port->rx_ch, port->rx_skb_list.qlen, port->rx_pkg_cnt, port->rx_drop_cnt,
			port->tx_ch, port->tx_pkg_cnt);
}
struct ccci_port_ops char_port_ops = {
	.init = &port_char_init,
	.recv_skb = &port_char_recv_skb,
	.recv_match = &port_char_recv_match,
	.md_state_notice = &port_char_md_state_notice,
	.dump_info = &port_char_dump_info,
};

