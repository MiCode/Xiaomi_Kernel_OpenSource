/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kthread.h>
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

#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
#include <mt-plat/env.h>
#endif
#include <mt-plat/mtk_boot_common.h>
#include <mt-plat/mtk_ccci_common.h>

#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_fsm.h"
#include "ccci_modem.h"
#include "ccci_hif.h"
#include "ccci_port.h"
#include "port_proxy.h"
#include "port_udc.h"
#define TAG PORT
#define CCCI_DEV_NAME "ccci"

/****************************************************************************/
/* Port_proxy: instance definition, which is alloced for every Modem */
/****************************************************************************/
static struct port_proxy *proxy_table[MAX_MD_NUM];
#define GET_PORT_PROXY(md_id) (proxy_table[md_id])
#define SET_PORT_PROXY(md_id, proxy_p) (proxy_table[md_id] = proxy_p)
#define CHECK_MD_ID(md_id)
#define CHECK_HIF_ID(hif_id)
#define CHECK_QUEUE_ID(queue_id)

static inline void proxy_set_critical_user(struct port_proxy *proxy_p,
	int user_id, int enabled)
{
	if (enabled)
		proxy_p->critical_user_active |= (1 << user_id);
	else
		proxy_p->critical_user_active &= ~(1 << user_id);
}

static inline int proxy_get_critical_user(struct port_proxy *proxy_p,
	int user_id)
{
	return ((proxy_p->critical_user_active &
		(1 << user_id)) >> user_id);
}

/****************************************************************************/
/*REGION: default char device operation definition for node,
 * which export node for userspace
 ***************************************************************************/
int port_dev_open(struct inode *inode, struct file *file)
{
	int md_id;
	int major = imajor(inode);
	int minor = iminor(inode);
	struct port_t *port;

	port = port_get_by_node(major, minor);
	if (port->rx_ch != CCCI_CCB_CTRL && atomic_read(&port->usage_cnt))
		return -EBUSY;
	md_id = port->md_id;
	CCCI_NORMAL_LOG(md_id, CHAR,
		"port %s open with flag %X by %s\n", port->name, file->f_flags,
		current->comm);
	atomic_inc(&port->usage_cnt);
	file->private_data = port;
	nonseekable_open(inode, file);
	port_user_register(port);
	return 0;
}

int port_dev_close(struct inode *inode, struct file *file)
{
	struct port_t *port = file->private_data;
	struct sk_buff *skb = NULL;
	unsigned long flags;
	int clear_cnt = 0;
	int md_id = port->md_id;

	/* 0. decrease usage count, so when we ask more,
	 * the packet can be dropped in recv_request
	 */
	atomic_dec(&port->usage_cnt);
	/* 1. purge Rx request list */
	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL) {
		ccci_free_skb(skb);
		clear_cnt++;
	}
	port->rx_drop_cnt += clear_cnt;
	/*  flush Rx */
	port_ask_more_req_to_md(port);
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	CCCI_NORMAL_LOG(md_id, CHAR,
		"port %s close rx_len=%d empty=%d, clear_cnt=%d, drop=%d\n",
		port->name, port->rx_skb_list.qlen,
		skb_queue_empty(&port->rx_skb_list),
		clear_cnt, port->rx_drop_cnt);
	ccci_event_log(
		"md%d: port %s close rx_len=%d empty=%d, clear_cnt=%d, drop=%d\n",
		md_id, port->name,
		port->rx_skb_list.qlen,
		skb_queue_empty(&port->rx_skb_list),
		clear_cnt, port->rx_drop_cnt);
	port_user_unregister(port);

	return 0;
}

ssize_t port_dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct port_t *port = file->private_data;
	struct sk_buff *skb = NULL;
	int ret = 0, read_len = 0, full_req_done = 0;
	unsigned long flags = 0;
	int md_id = port->md_id;

READ_START:
	/* 1. get incoming request */
	if (skb_queue_empty(&port->rx_skb_list)) {
		if (!(file->f_flags & O_NONBLOCK)) {
			spin_lock_irq(&port->rx_wq.lock);
			ret = wait_event_interruptible_locked_irq(port->rx_wq,
				!skb_queue_empty(&port->rx_skb_list));
			spin_unlock_irq(&port->rx_wq.lock);
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto exit;
			}
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	}
	CCCI_DEBUG_LOG(md_id, CHAR,
		"read on %s for %zu\n", port->name, count);
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
	if (skb == NULL) {
		ret = -EFAULT;
		goto exit;
	}

	read_len = skb->len;

	if (count >= read_len) {
		full_req_done = 1;
		__skb_unlink(skb, &port->rx_skb_list);
		/*
		 * here we only ask for more request when rx list is empty.
		 * no need to be too gready, because
		 * for most of the case, queue will not stop
		 * sending request to port.
		 * actually we just need to ask by ourselves when
		 * we rejected requests before. these
		 * rejected requests will staty in queue's buffer and may
		 * never get a chance to be handled again.
		 */
		if (port->rx_skb_list.qlen == 0)
			port_ask_more_req_to_md(port);
		if (port->rx_skb_list.qlen < 0) {
			spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
			CCCI_ERROR_LOG(md_id, CHAR,
				"%s:port->rx_skb_list.qlen < 0 %s\n",
				__func__, port->name);
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
		CCCI_ERROR_LOG(md_id, CHAR,
			"read on %s, copy to user failed, %d/%zu\n",
			port->name, read_len, count);
		ret = -EFAULT;
	}
	skb_pull(skb, read_len);
	/* 4. free request */
	if (full_req_done)
		ccci_free_skb(skb);

 exit:
	return ret ? ret : read_len;
}

ssize_t port_dev_write(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	struct port_t *port = file->private_data;
	unsigned char blocking = !(file->f_flags & O_NONBLOCK);
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h = NULL;
	size_t actual_count = 0, alloc_size = 0;
	int ret = 0, header_len = 0;
	int md_id = port->md_id;
	int md_state;

	if (count == 0)
		return -EINVAL;

	CHECK_MD_ID(md_id);
	md_state = ccci_fsm_get_md_state(md_id);
	if ((md_state == BOOT_WAITING_FOR_HS1
		|| md_state == BOOT_WAITING_FOR_HS2)
		&& port->tx_ch != CCCI_FS_TX && port->tx_ch != CCCI_RPC_TX) {
		CCCI_DEBUG_LOG(port->md_id, TAG,
			"port %s ch%d write fail when md_state=%d !!!\n",
			port->name, port->tx_ch, md_state);
		return -ENODEV;
	}

	header_len = sizeof(struct ccci_header) +
		(port->rx_ch == CCCI_FS_RX ? sizeof(unsigned int) : 0);
	if (port->flags & PORT_F_USER_HEADER) {
		if (count > (CCCI_MTU + header_len)) {
			CCCI_ERROR_LOG(md_id, CHAR,
				"reject packet(size=%zu ), larger than MTU on %s\n",
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
		/* 1. for Tx packet, who issued it should know
		 * whether recycle it  or not
		 */
		/* 2. prepare CCCI header, every member of header
		 * should be re-write as request may be re-used
		 */
		if (!(port->flags & PORT_F_USER_HEADER)) {
			ccci_h = (struct ccci_header *)skb_put(skb,
				sizeof(struct ccci_header));
			ccci_h->data[0] = 0;
			ccci_h->data[1] =
				actual_count + sizeof(struct ccci_header);
			ccci_h->channel = port->tx_ch;
			ccci_h->reserved = 0;
		} else {
			ccci_h = (struct ccci_header *)skb->data;
		}
		/* 3. get user data */
		ret = copy_from_user(skb_put(skb, actual_count),
				buf, actual_count);
		if (ret)
			goto err_out;
		if (port->flags & PORT_F_USER_HEADER) {
			/* ccci_header provided by user,
			 * For only send ccci_header
			 * without additional data case,
			 * data[0]=CCCI_MAGIC_NUM, data[1]=user_data,
			 * ch=tx_channel,reserved=no_use,
			 * For send ccci_header with additional data case,
			 * data[0]=0, data[1]=data_size, ch=tx_channel,
			 * reserved=user_data
			 */
			if (actual_count == sizeof(struct ccci_header))
				ccci_h->data[0] = CCCI_MAGIC_NUM;
			else
				ccci_h->data[1] = actual_count;
			/* as EEMCS VA will not fill this filed */
			ccci_h->channel = port->tx_ch;
		}

		if (port->flags & PORT_F_CH_TRAFFIC) {
			if (port->flags & PORT_F_USER_HEADER)
				port_ch_dump(port, 1, skb->data, actual_count);
			else
				port_ch_dump(port, 1, skb->data + header_len,
					actual_count);
		}
		if (port->rx_ch == CCCI_IPC_RX) {
			ret = port_ipc_write_check_id(port, skb);
			if (ret < 0)
				goto err_out;
			else
				ccci_h->reserved = ret;	/* Unity ID */
		}
		/* 4. send out */
		/*
		 * for md3, ccci_h->channel will probably
		 * change after call send_skb
		 * because md3's channel mapping
		 */
		ret = port_send_skb_to_md(port, skb, blocking);
		/* do NOT reference request after called this,
		 * modem may have freed it, unless you get -EBUSY
		 */
		if (ret) {
			if (ret == -EBUSY && !blocking)
				ret = -EAGAIN;
			goto err_out;
		}
		return actual_count;

 err_out:
		CCCI_NORMAL_LOG(md_id, CHAR,
			"write error done on %s, l=%zu r=%d\n",
			port->name, actual_count, ret);
		ccci_free_skb(skb);
		return ret;
	}
	/* consider this case as non-blocking */
	return -EBUSY;
}

long port_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct port_t *port = file->private_data;
	struct ccci_smem_region *sub_smem;

	switch (cmd) {
	case CCCI_IOC_SET_HEADER:
		port->flags |= PORT_F_USER_HEADER;
		break;
	case CCCI_IOC_CLR_HEADER:
		port->flags &= ~PORT_F_USER_HEADER;
		break;
	case CCCI_IOC_SMEM_BASE:
		if (port->rx_ch != CCCI_WIFI_RX)
			return -EFAULT;
		sub_smem = ccci_md_get_smem_by_user_id(port->md_id,
						SMEM_USER_MD_WIFI_PROXY);
		if (!sub_smem)
			return -EFAULT;
		CCCI_NORMAL_LOG(port->md_id, TAG, "wifi smem phy =%lx\n",
			(unsigned long)sub_smem->base_ap_view_phy);
		ret = put_user((unsigned int)sub_smem->base_ap_view_phy,
				(unsigned int __user *)arg);
		break;
	case CCCI_IOC_SMEM_LEN:
		if (port->rx_ch != CCCI_WIFI_RX)
			return -EFAULT;
		sub_smem = ccci_md_get_smem_by_user_id(port->md_id,
						SMEM_USER_MD_WIFI_PROXY);
		if (!sub_smem)
			return -EFAULT;
		sub_smem->size &= ~(PAGE_SIZE - 1);
		CCCI_NORMAL_LOG(port->md_id, TAG, "wifi smem size =%lx(%d)\n",
			(unsigned long)sub_smem->size, (int)PAGE_SIZE);
		ret = put_user((unsigned int)sub_smem->size,
				(unsigned int __user *)arg);
		break;
	default:
		ret = -1;
		break;
	}
	if (ret == -1)
		ret = ccci_fsm_ioctl(port->md_id, cmd, arg);

	return ret;
}

#ifdef CONFIG_COMPAT
long port_dev_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	struct port_t *port = filp->private_data;
	int md_id = port->md_id;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		CCCI_ERROR_LOG(md_id, CHAR,
			"dev_char_compat_ioctl(!filp->f_op || !filp->f_op->unlocked_ioctl)\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case CCCI_IOC_PCM_BASE_ADDR:
	case CCCI_IOC_PCM_LEN:
	case CCCI_IOC_ALLOC_MD_LOG_MEM:
	case CCCI_IOC_FORCE_FD:
	case CCCI_IOC_AP_ENG_BUILD:
	case CCCI_IOC_GET_MD_MEM_SIZE:
		CCCI_ERROR_LOG(md_id, CHAR,
			"dev_char_compat_ioctl deprecated cmd(%d)\n", cmd);
		return 0;
	default:
		return filp->f_op->unlocked_ioctl(filp, cmd,
				(unsigned long)compat_ptr(arg));
	}
}
#endif


int port_dev_mmap(struct file *fp, struct vm_area_struct *vma)
{
	struct port_t *port = fp->private_data;
	int md_id = port->md_id;
	int len, ret;
	unsigned long pfn;
	struct ccci_smem_region *wifi_smem;

	if (port->rx_ch != CCCI_WIFI_RX)
		return -EFAULT;

	wifi_smem = ccci_md_get_smem_by_user_id(md_id,
						SMEM_USER_MD_WIFI_PROXY);
	if (!wifi_smem)
		return -EFAULT;
	wifi_smem->size &= ~(PAGE_SIZE - 1);
	CCCI_NORMAL_LOG(md_id, CHAR,
			"remap wifi smem addr:0x%llx len:%d  map-len:%lu\n",
			(unsigned long long)wifi_smem->base_ap_view_phy,
			wifi_smem->size, vma->vm_end - vma->vm_start);
	if ((vma->vm_end - vma->vm_start) > wifi_smem->size) {
		CCCI_ERROR_LOG(md_id, CHAR,
			"invalid mm size request from %s\n",
			port->name);
		return -EINVAL;
	}

	len = (vma->vm_end - vma->vm_start) < wifi_smem->size ?
		vma->vm_end - vma->vm_start : wifi_smem->size;
	pfn = wifi_smem->base_ap_view_phy;
	pfn >>= PAGE_SHIFT;
	/* ensure that memory does not get swapped to disk */
	vma->vm_flags |= VM_IO;
	/* ensure non-cacheable */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = remap_pfn_range(vma, vma->vm_start, pfn,
				len, vma->vm_page_prot);
	if (ret) {
		CCCI_ERROR_LOG(md_id, CHAR,
			"wifi smem remap failed %d/%lx, 0x%llx -> 0x%llx\n",
			ret, pfn,
			(unsigned long long)wifi_smem->base_ap_view_phy,
			(unsigned long long)vma->vm_start);
		return -EAGAIN;
	}

	CCCI_NORMAL_LOG(md_id, CHAR,
		"wifi smem remap succeed %lx, 0x%llx -> 0x%llx\n", pfn,
		(unsigned long long)wifi_smem->base_ap_view_phy,
		(unsigned long long)vma->vm_start);

	return 0;
}

/**************************************************************************/
/* REGION: port common API implementation,
 * these APIs are valiable for every port
 */
/**************************************************************************/
static inline void port_struct_init(struct port_t *port,
	struct port_proxy *port_p)
{
	INIT_LIST_HEAD(&port->entry);
	INIT_LIST_HEAD(&port->exp_entry);
	INIT_LIST_HEAD(&port->queue_entry);
	skb_queue_head_init(&port->rx_skb_list);
	if (port->tx_ch == CCCI_UDC_TX)
		skb_queue_head_init(&port->rx_skb_list_hp);
	init_waitqueue_head(&port->rx_wq);
	port->tx_busy_count = 0;
	port->rx_busy_count = 0;
	atomic_set(&port->usage_cnt, 0);
	port->port_proxy = port_p;
	port->md_id = port_p->md_id;

	wakeup_source_init(&port->rx_wakelock, port->name);
}

static void port_dump_string(struct port_t *port, int dir,
	void *msg_buf, int len)
{
#define DUMP_BUF_SIZE 32
	unsigned char *char_ptr = (unsigned char *)msg_buf;
	char buf[DUMP_BUF_SIZE];
	int i, j;
	u64 ts_nsec;
	unsigned long rem_nsec;
	char *replace_str;

	for (i = 0, j = 0; i < len && i < DUMP_BUF_SIZE &&
		j + 4 < DUMP_BUF_SIZE; i++) {
		if (((char_ptr[i] >= 32) && (char_ptr[i] <= 126))) {
			buf[j] = char_ptr[i];
			j += 1;
		} else if (char_ptr[i] == '\r' ||
			char_ptr[i] == '\n' ||
			char_ptr[i] == '\t') {
			switch (char_ptr[i]) {
			case '\r':
				replace_str = "\\r";
				break;
			case '\n':
				replace_str = "\\n";
				break;
			case '\t':
				replace_str = "\\t";
				break;
			default:
				replace_str = "";
				break;
			}
			snprintf(buf+j, DUMP_BUF_SIZE - j,
				"%s", replace_str);
			j += 2;
		} else {
			snprintf(buf+j, DUMP_BUF_SIZE - j,
				"[%02X]", char_ptr[i]);
			j += 4;
		}
	}
	buf[j] = '\0';
	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);
	if (dir == 0)
		CCCI_HISTORY_LOG(port->md_id, TAG,
			"[%5lu.%06lu]C:%d,%d(%d,%d,%d) %s: %d<%s\n",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			port->flags, port->rx_ch,
			port->rx_skb_list.qlen, port->rx_pkg_cnt,
			port->rx_drop_cnt, "R", len, buf);
	else
		CCCI_HISTORY_LOG(port->md_id, TAG,
			"[%5lu.%06lu]C:%d,%d(%d) %s: %d>%s\n",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			port->flags, port->tx_ch,
			port->tx_pkg_cnt, "W", len, buf);
}
static void port_dump_raw_data(struct port_t *port, int dir,
	void *msg_buf, int len)
{
#define DUMP_RAW_DATA_SIZE 16
	unsigned int *curr_p = (unsigned int *)msg_buf;
	unsigned char *curr_ch_p;
	int _16_fix_num = len / 16;
	int tail_num = len % 16;
	char buf[16];
	int i, j;
	int dump_size;
	u64 ts_nsec;
	unsigned long rem_nsec;

	if (curr_p == NULL) {
		CCCI_HISTORY_LOG(port->md_id, TAG, "start_addr <NULL>\n");
		return;
	}
	if (len == 0) {
		CCCI_HISTORY_LOG(port->md_id, TAG, "len [0]\n");
		return;
	}
	if (port->rx_ch == CCCI_FS_RX)
		curr_p++;

	dump_size = len > DUMP_RAW_DATA_SIZE ? DUMP_RAW_DATA_SIZE : len;
	_16_fix_num = dump_size / 16;
	tail_num = dump_size % 16;

	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);

	if (dir == 0)
		CCCI_HISTORY_LOG(port->md_id, TAG,
			"[%5lu.%06lu]C:%d,%d(%d,%d,%d) %s: %d<",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			port->flags, port->rx_ch,
			port->rx_skb_list.qlen, port->rx_pkg_cnt,
			port->rx_drop_cnt, "R", len);
	else
		CCCI_HISTORY_LOG(port->md_id, TAG,
			"[%5lu.%06lu]C:%d,%d(%d) %s: %d>",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			port->flags, port->tx_ch,
			port->tx_pkg_cnt, "W", len);
	/* Fix section */
	for (i = 0; i < _16_fix_num; i++) {
		CCCI_HISTORY_LOG(port->md_id, TAG,
			"%03X: %08X %08X %08X %08X\n",
			i * 16, *curr_p, *(curr_p + 1),
			*(curr_p + 2), *(curr_p + 3));
		curr_p += 4;
	}

	/* Tail section */
	if (tail_num > 0) {
		curr_ch_p = (unsigned char *)curr_p;
		for (j = 0; j < tail_num; j++) {
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for (; j < 16; j++)
			buf[j] = 0;
		curr_p = (unsigned int *)buf;
		CCCI_HISTORY_LOG(port->md_id, TAG,
			"%03X: %08X %08X %08X %08X\n",
			i * 16, *curr_p, *(curr_p + 1),
			*(curr_p + 2), *(curr_p + 3));
	}
}
static inline int port_get_queue_no(struct port_t *port, DIRECTION dir,
			 int is_ack)
{
	int md_state;
	int md_id = port->md_id;

	CHECK_MD_ID(md_id);
	md_state = ccci_fsm_get_md_state(md_id);
	if (dir == OUT) {
		if (is_ack == 1)
			return (md_state == EXCEPTION ? port->txq_exp_index :
				(port->txq_exp_index&0x0F));
		return (md_state == EXCEPTION ? port->txq_exp_index :
				port->txq_index);
	} else if (dir == IN)
		return (md_state == EXCEPTION ? port->rxq_exp_index :
				port->rxq_index);
	else
		return -1;
}

static inline int port_adjust_skb(struct port_t *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = NULL;

	ccci_h = (struct ccci_header *)skb->data;
	/* header provide by user */
	if (port->flags & PORT_F_USER_HEADER) {
		/* CCCI_MON_CH should fall in here,
		 * as header must be send to md_init
		 */
		if (ccci_h->data[0] == CCCI_MAGIC_NUM) {
			if (unlikely(skb->len > sizeof(struct ccci_header))) {
				CCCI_ERROR_LOG(port->md_id, TAG,
					"recv unexpected data for %s, skb->len=%d\n",
					port->name, skb->len);
				skb_trim(skb, sizeof(struct ccci_header));
			}
		}
	} else {
		/* remove CCCI header */
		skb_pull(skb, sizeof(struct ccci_header));
	}

	return 0;
}

/*
 *This API is common API for port to resceive skb form modem or HIF,
 *
 */
int port_recv_skb(struct port_t *port, struct sk_buff *skb)
{
	unsigned long flags;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;

	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	CCCI_DEBUG_LOG(port->md_id, TAG,
		"recv on %s, len=%d\n", port->name,
		port->rx_skb_list.qlen);
	if (port->rx_skb_list.qlen < port->rx_length_th) {
		port->flags &= ~PORT_F_RX_FULLED;
		if (port->flags & PORT_F_ADJUST_HEADER)
			port_adjust_skb(port, skb);
		if (ccci_h->channel == CCCI_STATUS_RX)
			port->skb_handler(port, skb);
		else
			__skb_queue_tail(&port->rx_skb_list, skb);
		port->rx_pkg_cnt++;
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		__pm_wakeup_event(&port->rx_wakelock, jiffies_to_msecs(HZ/2));
		spin_lock_irqsave(&port->rx_wq.lock, flags);
		wake_up_all_locked(&port->rx_wq);
		spin_unlock_irqrestore(&port->rx_wq.lock, flags);

		return 0;
	}
	port->flags |= PORT_F_RX_FULLED;
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	if (port->flags & PORT_F_ALLOW_DROP) {
		CCCI_NORMAL_LOG(port->md_id, TAG,
			"port %s Rx full, drop packet\n",
			port->name);
		goto drop;
	} else
		return -CCCI_ERR_PORT_RX_FULL;

 drop:
	/* only return drop and caller do drop */
	CCCI_NORMAL_LOG(port->md_id, TAG,
		"drop on %s, len=%d\n", port->name,
		port->rx_skb_list.qlen);
	port->rx_drop_cnt++;
	return -CCCI_ERR_DROP_PACKET;
}

int port_kthread_handler(void *arg)
{
	struct port_t *port = arg;
	/* struct sched_param param = { .sched_priority = 1 }; */
	struct sk_buff *skb;
	unsigned long flags;
	int ret = 0;
	int md_id = port->md_id;

	CCCI_DEBUG_LOG(md_id, TAG,
		"port %s's thread running\n", port->name);

	while (1) {
		if (skb_queue_empty(&port->rx_skb_list)) {
			ret = wait_event_interruptible(port->rx_wq,
					!skb_queue_empty(&port->rx_skb_list));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;
		CCCI_DEBUG_LOG(md_id, TAG, "read on %s\n", port->name);
		/* 1. dequeue */
		spin_lock_irqsave(&port->rx_skb_list.lock, flags);
		skb = __skb_dequeue(&port->rx_skb_list);
		if (port->rx_skb_list.qlen == 0)
			port_ask_more_req_to_md(port);
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		/* 2. process port skb */
		if (port->skb_handler)
			port->skb_handler(port, skb);
	}
	return 0;
}

/*
 * This API is called by port,
 * which wants to dump message as
 * ascii string or raw binary format,
 */
void port_ch_dump(struct port_t *port, int dir, void *msg_buf, int len)
{
	if (port->flags & PORT_F_DUMP_RAW_DATA)
		port_dump_raw_data(port, dir, msg_buf, len);
	else
		port_dump_string(port, dir, msg_buf, len);
}


int port_ask_more_req_to_md(struct port_t *port)
{
	int ret = -1;
	int rx_qno = port_get_queue_no(port, IN, -1);

	if (port->flags & PORT_F_RX_FULLED)
		ret = ccci_hif_ask_more_request(port->hif_id, rx_qno);
	return ret;
}

int port_write_room_to_md(struct port_t *port)
{
	int ret = -1;
	int tx_qno;

	tx_qno = port_get_queue_no(port, OUT, -1);
	if (port->flags & PORT_F_RX_FULLED)
		ret = ccci_hif_write_room(port->hif_id, tx_qno);
	return ret;
}

int port_user_register(struct port_t *port)
{
	int md_id = port->md_id;
	int rx_ch = port->rx_ch;
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);
	if (rx_ch == CCCI_FS_RX)
		proxy_set_critical_user(proxy_p, CRIT_USR_FS, 1);
	if (rx_ch == CCCI_UART2_RX)
		proxy_set_critical_user(proxy_p, CRIT_USR_MUXD, 1);
	if (rx_ch == CCCI_MD_LOG_RX || (rx_ch == CCCI_SMEM_CH &&
		strcmp(port->name, "ccci_ccb_dhl") == 0))
		proxy_set_critical_user(proxy_p, CRIT_USR_MDLOG, 1);
	if (rx_ch == CCCI_UART1_RX)
		proxy_set_critical_user(proxy_p, CRIT_USR_META, 1);
	return 0;
}

int port_user_unregister(struct port_t *port)
{
	int rx_ch = port->rx_ch;
	int md_id = port->md_id;
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);

	if (rx_ch == CCCI_FS_RX)
		proxy_set_critical_user(proxy_p, CRIT_USR_FS, 0);
	if (rx_ch == CCCI_UART2_RX)
		proxy_set_critical_user(proxy_p, CRIT_USR_MUXD, 0);
	if (rx_ch == CCCI_MD_LOG_RX || (rx_ch == CCCI_SMEM_CH &&
		strcmp(port->name, "ccci_ccb_dhl") == 0))
		proxy_set_critical_user(proxy_p, CRIT_USR_MDLOG, 0);
	if (rx_ch == CCCI_UART1_RX)
		proxy_set_critical_user(proxy_p, CRIT_USR_META, 0);

	CCCI_NORMAL_LOG(md_id, TAG, "critical user check: 0x%x\n",
		proxy_p->critical_user_active);
	ccci_event_log("md%d: critical user check: 0x%x\n",
		md_id, proxy_p->critical_user_active);
	return 0;
}


/*
 * This API is called by port_net,
 * which is used to send skb message to md
 */
int port_net_send_skb_to_md(struct port_t *port, int is_ack,
	struct sk_buff *skb)
{
	int tx_qno = 0;
	int md_id = port->md_id;

	CHECK_MD_ID(md_id);
	if (ccci_fsm_get_md_state(md_id) != READY)
		return -ENODEV;
	tx_qno = port_get_queue_no(port, OUT, is_ack);
	return ccci_hif_send_skb(port->hif_id, tx_qno, skb,
			port->skb_from_pool, 0);
}

int port_send_skb_to_md(struct port_t *port, struct sk_buff *skb, int blocking)
{
	int tx_qno = 0;
	int ret = 0;
	int md_id = port->md_id;
	int md_state;

	CHECK_MD_ID(md_id);
	md_state = ccci_fsm_get_md_state(md_id);

	if ((md_state == BOOT_WAITING_FOR_HS1 ||
		md_state == BOOT_WAITING_FOR_HS2)
		&& port->tx_ch != CCCI_FS_TX && port->tx_ch != CCCI_RPC_TX) {
		CCCI_NORMAL_LOG(port->md_id, TAG,
			"port %s ch%d write fail when md_state=%d\n",
			port->name, port->tx_ch, md_state);
		return -ENODEV;
	}
	if (md_state == EXCEPTION
		&& port->tx_ch != CCCI_MD_LOG_TX
		&& port->tx_ch != CCCI_UART1_TX
	    && port->tx_ch != CCCI_FS_TX)
		return -ETXTBSY;
	if (md_state == GATED
			|| md_state == WAITING_TO_STOP
			|| md_state == INVALID)
		return -ENODEV;

	tx_qno = port_get_queue_no(port, OUT, -1);
	ret = ccci_hif_send_skb(port->hif_id, tx_qno, skb,
			port->skb_from_pool, blocking);
	if (ret == 0)
		port->tx_pkg_cnt++;
	return ret;
}
/****************************************************************************/
/*REGION: port_proxy class method implementation*/
/****************************************************************************/
static inline int proxy_check_critical_user(struct port_proxy *proxy_p)
{
	int ret = 1;
	int md_id = proxy_p->md_id;

	if (proxy_get_critical_user(proxy_p, CRIT_USR_MUXD) == 0) {
		if (is_meta_mode() || is_advanced_meta_mode()) {
			if (proxy_get_critical_user(proxy_p,
				CRIT_USR_META) == 0) {
				CCCI_NORMAL_LOG(md_id, TAG,
				"ready to reset MD in META mode\n");
				ret = 0;
				goto __EXIT_FUN__;
			}
			/* this should never happen */
			CCCI_ERROR_LOG(md_id, TAG,
				"DHL ctrl is still open in META mode\n");
		} else {
			if (proxy_get_critical_user(proxy_p,
				CRIT_USR_MDLOG) == 0 &&
				proxy_get_critical_user(proxy_p,
				CRIT_USR_MDLOG_CTRL) == 0) {
				CCCI_NORMAL_LOG(md_id, TAG,
					"ready to reset MD in normal mode\n");
				ret = 0;
				goto __EXIT_FUN__;
			}
		}
	}
__EXIT_FUN__:
	return ret;
}


static inline void proxy_setup_channel_mapping(struct port_proxy *proxy_p)
{
	int i, hif;
	struct port_t *port = NULL;

	/* Init RX_CH=>port list mapping */
	for (i = 0; i < ARRAY_SIZE(proxy_p->rx_ch_ports); i++)
		INIT_LIST_HEAD(&proxy_p->rx_ch_ports[i]);
	/* Init queue_id=>port list mapping per HIF*/
	for (hif = 0; hif < ARRAY_SIZE(proxy_p->queue_ports); hif++) {
		for (i = 0; i < ARRAY_SIZE(proxy_p->queue_ports[hif]); i++)
			INIT_LIST_HEAD(&proxy_p->queue_ports[hif][i]);
	}
	/*Init EE_ports list*/
	INIT_LIST_HEAD(&proxy_p->exp_ports);
	/*setup port mapping*/
	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		/*setup RX_CH=>port list mapping*/
		list_add_tail(&port->entry, &proxy_p->rx_ch_ports[port->rx_ch]);

		/* skip no data transmission port,
		 * such as CCCI_DUMMY_CH type port
		 */
		if (port->txq_index == 0xFF || port->rxq_index == 0xFF)
			continue;
		/*setup QUEUE_ID=>port list mapping*/
		list_add_tail(&port->queue_entry,
			&proxy_p->queue_ports[port->hif_id][port->rxq_index]);
		/*if port configure exception queue index, setup ee_ports*/
		if ((port->txq_exp_index & 0xF0) == 0 ||
			(port->rxq_exp_index & 0xF0) == 0)
			list_add_tail(&port->exp_entry, &proxy_p->exp_ports);
	}
	/*Dump RX_CH=> port list mapping for debugging*/
	for (i = 0; i < ARRAY_SIZE(proxy_p->rx_ch_ports); i++) {
		list_for_each_entry(port, &proxy_p->rx_ch_ports[i], entry) {
			CCCI_DEBUG_LOG(proxy_p->md_id, TAG,
				"CH%d ports:%s(%d/%d)\n",
				i, port->name, port->rx_ch, port->tx_ch);
		}
	}
	/*Dump Queue_ID=> port list mapping for debugging*/
	for (hif = 0; hif < ARRAY_SIZE(proxy_p->queue_ports); hif++) {
		for (i = 0; i < ARRAY_SIZE(proxy_p->queue_ports[hif]); i++) {
			list_for_each_entry(port,
				&proxy_p->queue_ports[hif][i], queue_entry) {
				CCCI_DEBUG_LOG(proxy_p->md_id, TAG,
					"HIF%d, Q%d, ports:%s(%d/%d)\n",
					hif, i, port->name, port->rx_ch,
					port->tx_ch);
			}
		}
	}
	/*Dump exp Queue_ID=> port list mapping for debugging*/
	list_for_each_entry(port, &proxy_p->exp_ports, exp_entry) {
		CCCI_DEBUG_LOG(proxy_p->md_id, TAG,
			"EXP: HIF%d, ports:%s(%d/%d) q(%d/%d),ee_q(%d/%d)\n",
			port->hif_id, port->name, port->rx_ch, port->tx_ch,
			port->rxq_index, port->txq_index,
			port->rxq_exp_index, port->txq_exp_index);
	}
}

static struct port_t *proxy_get_port(struct port_proxy *proxy_p, int minor,
	CCCI_CH ch)
{
	int i;
	struct port_t *port;

	if (proxy_p == NULL)
		return NULL;
	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if (minor >= 0 && port->minor == minor)
			return port;
		if (ch != CCCI_INVALID_CH_ID &&
			(port->rx_ch == ch || port->tx_ch == ch))
			return port;
	}
	return NULL;
}

/*
 * kernel inject CCCI message to modem.
 */
static inline int proxy_send_msg_to_md(struct port_proxy *proxy_p,
	int ch, unsigned int msg, unsigned int resv, int blocking)
{
	struct port_t *port = NULL;
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h;
	int ret = 0;
	int md_state;
	int qno = -1;
	int md_id;

	if (!proxy_p) {
		CCCI_ERROR_LOG(0, TAG,
			"proxy_send_msg_to_md: proxy_p is NULL\n");
		return -CCCI_ERR_MD_NOT_READY;
	}
	md_id = proxy_p->md_id;
	md_state = ccci_fsm_get_md_state(md_id);
	if (md_state != BOOT_WAITING_FOR_HS1 &&
		md_state != BOOT_WAITING_FOR_HS2
		&& md_state != READY && md_state != EXCEPTION)
		return -CCCI_ERR_MD_NOT_READY;
	if (ch == CCCI_SYSTEM_TX && md_state != READY)
		return -CCCI_ERR_MD_NOT_READY;
	if ((msg == CCISM_SHM_INIT || msg == CCISM_SHM_INIT_DONE ||
		msg == C2K_CCISM_SHM_INIT ||
		msg == C2K_CCISM_SHM_INIT_DONE) &&
		md_state != READY) {
		return -CCCI_ERR_MD_NOT_READY;
	}
	if (ch == CCCI_SYSTEM_TX)
		port = proxy_p->sys_port;
	else if (ch == CCCI_CONTROL_TX)
		port = proxy_p->ctl_port;
	else
		port = port_get_by_channel(md_id, ch);
	if (port) {
		skb = ccci_alloc_skb(sizeof(struct ccci_header),
				port->skb_from_pool, blocking);
		if (skb) {
			ccci_h = (struct ccci_header *)skb_put(skb,
				sizeof(struct ccci_header));
			ccci_h->data[0] = CCCI_MAGIC_NUM;
			ccci_h->data[1] = msg;
			ccci_h->channel = ch;
			ccci_h->reserved = resv;
			qno = port_get_queue_no(port, OUT, -1);
			ret = ccci_hif_send_skb(port->hif_id, qno, skb,
					port->skb_from_pool, blocking);
			if (ret)
				ccci_free_skb(skb);
			return ret;
		} else {
			return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
		}
	}
	return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
}

/*
 * if recv_request returns 0 or -CCCI_ERR_DROP_PACKET,
 * then it's port's duty to free the request, and caller should
 * NOT reference the request any more. but if it returns other error,
 * caller should be responsible to free the request.
 */
static inline int proxy_dispatch_recv_skb(struct port_proxy *proxy_p,
	int hif_id, struct sk_buff *skb, unsigned int flag)
{
	struct ccci_header *ccci_h = NULL;
	struct lhif_header *lhif_h;
	struct ccmni_ch ccmni;
	struct port_t *port = NULL;
	struct list_head *port_list = NULL;
	int ret = -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
	char matched = 0;
	int md_id = proxy_p->md_id;
	int md_state = ccci_fsm_get_md_state(md_id);
	int channel = CCCI_INVALID_CH_ID;

	if (unlikely(!skb)) {
		ret = -CCCI_ERR_INVALID_PARAM;
		goto err_exit;
	}

	if (flag == NORMAL_DATA) {
		ccci_h = (struct ccci_header *)skb->data;
		channel = ccci_h->channel;
	} else if (flag == CLDMA_NET_DATA) {
		lhif_h = (struct lhif_header *)skb->data;
		if (!ccci_get_ccmni_channel(proxy_p->md_id,
			lhif_h->netif, &ccmni))
			channel = ccmni.rx;
	} else {
		WARN_ON(1);
	}

	if (unlikely(channel >= CCCI_MAX_CH_NUM ||
		channel == CCCI_INVALID_CH_ID)) {
		ret = -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
		goto err_exit;
	}
	if (unlikely(md_state == GATED || md_state == INVALID)) {
		ret = -CCCI_ERR_HIF_NOT_POWER_ON;
		goto err_exit;
	}

	port_list = &proxy_p->rx_ch_ports[channel];
	list_for_each_entry(port, port_list, entry) {
		/*
		 * multi-cast is not supported, because one port may
		 * freed or modified this request
		 * before another port can process it. but we still can
		 * use req->state to achive some
		 * kind of multi-cast if needed.
		 */
		matched = (hif_id == port->hif_id) &&
			((port->ops->recv_match == NULL) ?
			(channel == port->rx_ch) :
			port->ops->recv_match(port, skb));
		if (matched) {
			if (likely(skb && port->ops->recv_skb)) {
				ret = port->ops->recv_skb(port, skb);
			} else {
				CCCI_ERROR_LOG(md_id, TAG,
					"port->ops->recv_skb is null\n");
				ret = -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
				goto err_exit;
			}
			if (ret == -CCCI_ERR_PORT_RX_FULL)
				port->rx_busy_count++;
			break;
		}
	}

 err_exit:
	if (ret < 0 && ret != -CCCI_ERR_PORT_RX_FULL) {
		if (channel == CCCI_CONTROL_RX)
			CCCI_ERROR_LOG(md_id, CORE,
				"drop on channel %d, ret %d\n", channel, ret);
		if (skb)
			ccci_free_skb(skb);
		ret = -CCCI_ERR_DROP_PACKET;
	}

	return ret;
}
static inline void proxy_dispatch_queue_status(struct port_proxy *proxy_p,
	int hif, int qno, int dir, unsigned int state)
{
	struct port_t *port;
	int match = 0;
	int i, matched = 0;

	/*EE then notify EE port*/
	if (unlikely(ccci_fsm_get_md_state(proxy_p->md_id)
		== EXCEPTION)) {
		list_for_each_entry(port,
		&proxy_p->exp_ports, exp_entry) {
			match = (port->hif_id == hif);
			if (dir == OUT)
				match =	(qno == port->txq_exp_index);
			else
				match = (qno == port->rxq_exp_index);
			if (match && port->ops->queue_state_notify)
				port->ops->queue_state_notify(port, dir,
				qno, state);
		}
		return;
	}

	list_for_each_entry(port,
		&proxy_p->queue_ports[hif][qno], queue_entry) {
		match = (port->hif_id == hif);
		/* consider network data/ack queue design */
		if (dir == OUT)
			match = qno == port->txq_index
				|| qno == (port->txq_exp_index & 0x0F);
		else
			match = qno == port->rxq_index;
		if (match && port->ops->queue_state_notify) {
			port->ops->queue_state_notify(port, dir, qno, state);
			matched = 1;
		}
	}
	/*handle ccmni tx queue or tx ack queue state change*/
	if (!matched && hif == MD1_NET_HIF) {
		for (i = 0; i < proxy_p->port_number; i++) {
			port = proxy_p->ports + i;
			if (port->hif_id == MD1_NET_HIF) {
				/* consider network data/ack queue design */
				if (dir == OUT)
					match = qno == port->txq_index
					|| qno == (port->txq_exp_index & 0x0F);
				else
					match = qno == port->rxq_index;
				if (match && port->ops->queue_state_notify)
					port->ops->queue_state_notify(port, dir,
						qno, state);
			} else
				break;
		}
	}
}

static inline void proxy_dispatch_md_status(struct port_proxy *proxy_p,
	unsigned int state)
{
	int i;
	struct port_t *port;

	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if ((state == GATED) && (port->flags &
			PORT_F_CH_TRAFFIC)) {
			port->rx_pkg_cnt = 0;
			port->rx_drop_cnt = 0;
			port->tx_pkg_cnt = 0;
		}
		if (port->ops->md_state_notify)
			port->ops->md_state_notify(port, state);
	}
}

static inline void proxy_dump_status(struct port_proxy *proxy_p)
{
	struct port_t *port;
	/* hardcode, port number should not be larger than 64 */
	unsigned long long port_full = 0;
	unsigned int i;

	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if (port->flags & PORT_F_RX_FULLED)
			port_full |= (1LL << i);
		if (port->tx_busy_count != 0 || port->rx_busy_count != 0) {
			CCCI_REPEAT_LOG(proxy_p->md_id, TAG,
				"port %s busy count %d/%d\n", port->name,
				port->tx_busy_count, port->rx_busy_count);
			port->tx_busy_count = 0;
			port->rx_busy_count = 0;
		}
		if (port->ops->dump_info)
			port->ops->dump_info(port, 0);
	}
	if (port_full)
		CCCI_ERROR_LOG(proxy_p->md_id, TAG,
			"port_full status=%llx\n", port_full);
}

static inline int proxy_register_char_dev(struct port_proxy *proxy_p)
{
	int ret = 0;
	dev_t dev = 0;

	if (proxy_p->major) {
		dev = MKDEV(proxy_p->major, proxy_p->minor_base);
		ret = register_chrdev_region(dev, 120, CCCI_DEV_NAME);
	} else {
		ret = alloc_chrdev_region(&dev, proxy_p->minor_base,
				120, CCCI_DEV_NAME);
		if (ret)
			CCCI_ERROR_LOG(proxy_p->md_id, CHAR,
				"alloc_chrdev_region fail,ret=%d\n", ret);
		proxy_p->major = MAJOR(dev);
	}
	return ret;
}

static inline void proxy_init_all_ports(struct port_proxy *proxy_p)
{
	int i;
	int md_id;
	struct port_t *port;

	md_id = proxy_p->md_id;
	for (i = 0; i < ARRAY_SIZE(proxy_p->rx_ch_ports); i++)
		INIT_LIST_HEAD(&proxy_p->rx_ch_ports[i]);

	/* init port */
	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		port_struct_init(port, proxy_p);
		if (port->tx_ch == CCCI_SYSTEM_TX)
			proxy_p->sys_port = port;
		if (port->tx_ch == CCCI_CONTROL_TX)
			proxy_p->ctl_port = port;
		port->major = proxy_p->major;
		port->minor_base = proxy_p->minor_base;
		if (port->ops->init)
			port->ops->init(port);
	}
	proxy_setup_channel_mapping(proxy_p);
}

static inline void proxy_set_traffic_flag(struct port_proxy *proxy_p,
	unsigned int dump_flag)
{
	int idx;
	struct port_t *port;

	proxy_p->traffic_dump_flag = dump_flag;
	CCCI_NORMAL_LOG(proxy_p->md_id, TAG,
			 "%s: 0x%x\n", __func__, proxy_p->traffic_dump_flag);
	for (idx = 0; idx < proxy_p->port_number; idx++) {
		port = proxy_p->ports + idx;
		/*clear traffic & dump flag*/
		port->flags &= ~(PORT_F_CH_TRAFFIC | PORT_F_DUMP_RAW_DATA);

		/*set RILD related port*/
		if (proxy_p->traffic_dump_flag & (1 << PORT_DBG_DUMP_RILD)) {
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
		}
		/*set AUDIO related port*/
		if (proxy_p->traffic_dump_flag & (1 << PORT_DBG_DUMP_AUDIO)) {
			if (port->rx_ch == CCCI_PCM_RX)
				port->flags |= (PORT_F_CH_TRAFFIC
					| PORT_F_DUMP_RAW_DATA);
		}
		/*set IMS related port*/
		if (proxy_p->traffic_dump_flag & (1 << PORT_DBG_DUMP_IMS)) {
			if (port->rx_ch == CCCI_IMSV_DL ||
				port->rx_ch == CCCI_IMSC_DL ||
				port->rx_ch == CCCI_IMSA_DL ||
				port->rx_ch == CCCI_IMSA_DL ||
				port->rx_ch == CCCI_IMSEM_DL)
				port->flags |= (PORT_F_CH_TRAFFIC
					| PORT_F_DUMP_RAW_DATA);
		}
	}
}

static inline struct port_proxy *proxy_alloc(int md_id)
{
	int ret = 0;
	struct port_proxy *proxy_p;

	/* Allocate port_proxy obj and set all member zero */
	proxy_p = kzalloc(sizeof(struct port_proxy), GFP_KERNEL);
	if (proxy_p == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc port_proxy fail\n", __func__);
		return NULL;
	}
	proxy_p->md_id = md_id;

	ret = proxy_register_char_dev(proxy_p);
	if (ret)
		goto EXIT_FUN;
	proxy_p->port_number =
		port_get_cfg(proxy_p->md_id, &proxy_p->ports);
	if (proxy_p->port_number > 0 && proxy_p->ports)
		proxy_init_all_ports(proxy_p);
	else
		ret = -1;

EXIT_FUN:
	if (ret) {
		kfree(proxy_p);
		proxy_p = NULL;
		CCCI_ERROR_LOG(-1, TAG,
			"%s:get md port config fail,ret=%d\n", __func__, ret);
	}
	return proxy_p;
};

struct port_t *port_get_by_minor(int md_id, int minor)
{
	CHECK_MD_ID(md_id);
	return proxy_get_port(GET_PORT_PROXY(md_id), minor,
			CCCI_INVALID_CH_ID);
}
struct port_t *port_get_by_channel(int md_id, CCCI_CH ch)
{
	CHECK_MD_ID(md_id);
	return proxy_get_port(GET_PORT_PROXY(md_id), -1, ch);
}
struct port_t *port_get_by_node(int major, int minor)
{
	int i;
	struct port_proxy *proxy_p = NULL;

	for (i = 0; i < MAX_MD_NUM; i++) {
		proxy_p = GET_PORT_PROXY(i);
		if (proxy_p && proxy_p->major == major)
			return proxy_get_port(proxy_p, minor,
					CCCI_INVALID_CH_ID);
	}
	return NULL;
}
int port_send_msg_to_md(struct port_t *port, unsigned int msg,
	unsigned int resv, int blocking)
{
	int md_id = port->md_id;
	int ch = port->tx_ch;
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);
	return proxy_send_msg_to_md(port->port_proxy, ch,
			msg, resv, blocking);
}

/****************************************************************************/
/* Extern API implement for none port related module */
/****************************************************************************/
/*
 * This API is called by ccci_modem,
 * and used to create all ccci port instance for per modem
 */
int ccci_port_init(int md_id)
{
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = proxy_alloc(md_id);
	if (proxy_p == NULL) {
		CCCI_ERROR_LOG(md_id, TAG, "alloc port_proxy fail\n");
		return -1;
	}
	SET_PORT_PROXY(md_id, proxy_p);
	return 0;
}

/*
 * This API is called by ccci_fsm,
 * and used to dump all ccci port status for debugging
 */
void ccci_port_dump_status(int md_id)
{
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);
	proxy_dump_status(proxy_p);
}

static inline void user_broadcast_wrapper(int md_id, unsigned int state)
{
	int mapped_event = -1;

	switch (state) {
	case GATED:
		mapped_event = MD_STA_EV_STOP;
		break;
	case BOOT_WAITING_FOR_HS1:
		break;
	case BOOT_WAITING_FOR_HS2:
		mapped_event = MD_STA_EV_HS1;
		break;
	case READY:
		mapped_event = MD_STA_EV_READY;
		break;
	case EXCEPTION:
		mapped_event = MD_STA_EV_EXCEPTION;
		break;
	case RESET:
		break;
	case WAITING_TO_STOP:
		break;
	default:
		break;
	}

	if (mapped_event >= 0)
		inject_md_status_event(md_id, mapped_event, NULL);
}

/*
 * This API is called by ccci_fsm,
 * and used to dispatch modem status for all ports,
 * which want to know md state transition.
 */
void ccci_port_md_status_notify(int md_id, unsigned int state)
{
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);
	proxy_dispatch_md_status(proxy_p, (unsigned int)state);
	user_broadcast_wrapper(md_id, state);
}


/*
 * This API is called by HIF,
 * and used to dispatch Queue status for all ports,
 * which is mounted on the hif_id & qno
 */
void ccci_port_queue_status_notify(int md_id, int hif_id, int qno,
	int dir, unsigned int state)
{
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	CHECK_HIF_ID(hif_id);
	CHECK_QUEUE_ID(qno);
	proxy_p = GET_PORT_PROXY(md_id);
	proxy_dispatch_queue_status(proxy_p, hif_id, qno,
		dir, (unsigned int)state);
}

/*
 * This API is called by HIF,
 * and used to dispatch RX data for related port
 */
int ccci_port_recv_skb(int md_id, int hif_id, struct sk_buff *skb,
	unsigned int flag)
{
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);
	return proxy_dispatch_recv_skb(proxy_p, hif_id, skb, flag);
}

/*
 * This API is called by ccci fsm,
 * and used to check whether all critical user exited.
 */
int ccci_port_check_critical_user(int md_id)
{
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);
	return proxy_check_critical_user(proxy_p);
}

/*
 * This API is called by ccci fsm,
 * and used to get critical user status.
 */
int ccci_port_get_critical_user(int md_id, unsigned int user_id)
{
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);
	return proxy_get_critical_user(proxy_p, user_id);
}

/*
 * This API is called by ccci fsm,
 * and used to send a ccci msg for modem.
 */
int ccci_port_send_msg_to_md(int md_id, int ch, unsigned int msg,
	unsigned int resv, int blocking)
{
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);
	return proxy_send_msg_to_md(proxy_p, ch, msg, resv, blocking);
}

/*
 * This API is called by ccci fsm,
 * and used to set port traffic flag to catch traffic history on
 * some important channel.
 * port traffic use md_boot_data[MD_CFG_DUMP_FLAG] =
 * 0x6000_000x as port dump flag
 */
void ccci_port_set_traffic_flag(int md_id, unsigned int dump_flag)
{
	struct port_proxy *proxy_p;

	CHECK_MD_ID(md_id);
	proxy_p = GET_PORT_PROXY(md_id);
	proxy_set_traffic_flag(proxy_p, dump_flag);
}

#ifdef CONFIG_MTK_ECCCI_C2K /* only md3 can usb bypass */
int modem_dtr_set(int on, int low_latency)
{
	struct c2k_ctrl_port_msg c2k_ctl_msg;
	int ret = 0;
	int md_id;

	c2k_ctl_msg.chan_num = DATA_PPP_CH_C2K;
	c2k_ctl_msg.id_hi = (C2K_STATUS_IND_MSG & 0xFF00) >> 8;
	c2k_ctl_msg.id_low = C2K_STATUS_IND_MSG & 0xFF;
	c2k_ctl_msg.option = 0;
	if (on)
		c2k_ctl_msg.option |= 0x04;
	else
		c2k_ctl_msg.option &= 0xFB;

#if (MD_GENERATION <= 6292)
	md_id = MD_SYS3;
	CCCI_NORMAL_LOG(md_id, TAG, "usb bypass dtr set(%d)(0x%x)\n",
		on, (u32) (*((u32 *)&c2k_ctl_msg)));
	ccci_port_send_msg_to_md(md_id, CCCI_CONTROL_TX, C2K_STATUS_IND_MSG,
		(u32) (*((u32 *)&c2k_ctl_msg)), 1);
#else
	md_id = MD_SYS1;
	CCCI_NORMAL_LOG(md_id, TAG, "usb bypass dtr set(%d)(0x%x)\n",
		on, (u32) (*((u32 *)&c2k_ctl_msg)));
	ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX, C2K_PPP_LINE_STATUS,
		(u32) (*((u32 *)&c2k_ctl_msg)), 1);
#endif
	return ret;
}

int modem_dcd_state(void)
{
	struct c2k_ctrl_port_msg c2k_ctl_msg;
	int dcd_state = 0;
	int md_id, ret = 0;
	struct ccci_per_md *per_md_data = NULL;

	c2k_ctl_msg.chan_num = DATA_PPP_CH_C2K;
	c2k_ctl_msg.id_hi = (C2K_STATUS_QUERY_MSG & 0xFF00) >> 8;
	c2k_ctl_msg.id_low = C2K_STATUS_QUERY_MSG & 0xFF;
	c2k_ctl_msg.option = 0;

#if (MD_GENERATION <= 6292)
	md_id = MD_SYS3;
	ret = ccci_port_send_msg_to_md(md_id, CCCI_CONTROL_TX,
			C2K_STATUS_QUERY_MSG,
			(u32) (*((u32 *)&c2k_ctl_msg)), 1);
#else
	md_id = MD_SYS1;
	ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
			C2K_PPP_LINE_STATUS,
			(u32) (*((u32 *)&c2k_ctl_msg)), 1);

#endif
	CCCI_NORMAL_LOG(md_id, TAG,
		"usb bypass query state(0x%x)\n",
		(u32) (*((u32 *)&c2k_ctl_msg)));

	per_md_data = ccci_get_per_md_data(md_id);

	if (ret == -CCCI_ERR_MD_NOT_READY)
		dcd_state = 0;
	else {
		msleep(20);
		dcd_state = per_md_data->dtr_state;
	}
	return dcd_state;
}

int ccci_c2k_rawbulk_intercept(int ch_id, unsigned int interception)
{
	int ret = 0;
	struct port_proxy *proxy_p;
	struct port_t *port = NULL;
	struct list_head *port_list = NULL;
	char matched = 0;
	int ch_id_tx, ch_id_rx = 0;
#if (MD_GENERATION <= 6292)
	int md_id = MD_SYS3;
#else
	int md_id = MD_SYS1;
#endif
	struct ccci_per_md *per_md_data = ccci_get_per_md_data(md_id);

	/* USB bypass's channel id offset,
	 * please refer to viatel_rawbulk.h
	 */
	if (ch_id >= FS_CH_C2K)
		ch_id += 2;
	else
		ch_id += 1;

	/*only data and log channel are legal*/
	if (ch_id == DATA_PPP_CH_C2K) {
#if (MD_GENERATION <= 6292)
		ch_id_tx = CCCI_C2K_PPP_DATA;
		ch_id_rx = CCCI_C2K_PPP_DATA;
#else
		ch_id_tx = CCCI_C2K_PPP_TX;
		ch_id_rx = CCCI_C2K_PPP_RX;
#endif
	} else if (ch_id == MDLOG_CH_C2K) {
		ch_id_tx = CCCI_MD_LOG_TX;
		ch_id_rx = CCCI_MD_LOG_RX;
	} else {
		ret = -ENODEV;
		CCCI_ERROR_LOG(md_id, TAG,
			"Err: wrong ch_id(%d) from usb bypass\n", ch_id);
		return ret;
	}

	proxy_p = GET_PORT_PROXY(md_id);

	/*use rx channel to find port*/
	port_list = &proxy_p->rx_ch_ports[ch_id_rx];
	list_for_each_entry(port, port_list, entry) {
		matched = (ch_id_tx == port->tx_ch);
		if (matched) {
			port->interception = !!interception;
			if (port->interception)
				atomic_inc(&port->usage_cnt);
			else
				atomic_dec(&port->usage_cnt);
			if (ch_id == DATA_PPP_CH_C2K)
				per_md_data->data_usb_bypass = !!interception;
			ret = 0;
			CCCI_NORMAL_LOG(proxy_p->md_id, TAG,
				"port(%s) ch(%d) interception(%d) set\n",
				port->name, ch_id_tx, interception);
		}
	}
	if (!matched) {
		ret = -ENODEV;
		CCCI_ERROR_LOG(proxy_p->md_id, TAG,
			"Err: no port found when setting interception(%d,%d)\n",
			ch_id_tx, interception);
	}

	return ret;
}


int ccci_c2k_buffer_push(int ch_id, void *buf, int count)
{
	int ret = 0;
	struct port_proxy *proxy_p;
	struct port_t *port = NULL;
	struct list_head *port_list = NULL;
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h = NULL;
	char matched = 0;
	size_t actual_count = 0;
	int ch_id_tx, ch_id_rx = 0;
	/* usb will call this routine in ISR, so we cannot schedule */
	unsigned char blk1 = 0;
	/* non-blocking for all request from USB */
	unsigned char blk2 = 0;
#if (MD_GENERATION <= 6292)
	int md_id = MD_SYS3;
#else
	int md_id = MD_SYS1;
#endif

	/* USB bypass's channel id offset, please refer to viatel_rawbulk.h */
	if (ch_id >= FS_CH_C2K)
		ch_id += 2;
	else
		ch_id += 1;

	/* only data and log channel are legal */
	if (ch_id == DATA_PPP_CH_C2K) {
#if (MD_GENERATION <= 6292)
		ch_id_tx = CCCI_C2K_PPP_DATA;
		ch_id_rx = CCCI_C2K_PPP_DATA;
#else
		ch_id_tx = CCCI_C2K_PPP_TX;
		ch_id_rx = CCCI_C2K_PPP_RX;
#endif
	} else if (ch_id == MDLOG_CH_C2K) {
		ch_id_tx = CCCI_MD_LOG_TX;
		ch_id_rx = CCCI_MD_LOG_RX;
	} else {
		ret = -ENODEV;
		CCCI_ERROR_LOG(md_id, TAG,
			"Err: wrong ch_id(%d) from usb bypass\n", ch_id);
		return ret;
	}

	/* only md3 can usb bypass */
	proxy_p = GET_PORT_PROXY(md_id);

	CCCI_NORMAL_LOG(md_id, TAG,
		"data from usb bypass (ch%d)(%d)\n", ch_id_tx, count);

	actual_count = count > CCCI_MTU ? CCCI_MTU : count;

	port_list = &proxy_p->rx_ch_ports[ch_id_rx];
	list_for_each_entry(port, port_list, entry) {
		matched = (ch_id_tx == port->tx_ch);
		if (matched) {
			skb = ccci_alloc_skb(actual_count,
					port->skb_from_pool, blk1);
			if (skb) {
				ccci_h = (struct ccci_header *)skb_put(skb,
				sizeof(struct ccci_header));
				ccci_h->data[0] = 0;
				ccci_h->data[1] = actual_count +
				sizeof(struct ccci_header);
				ccci_h->channel = port->tx_ch;
				ccci_h->reserved = 0;

				memcpy(skb_put(skb, actual_count), buf,
				actual_count);

				/*
				 * for md3, ccci_h->channel will probably change
				 * after call send_skb,
				 * because md3's channel mapping.
				 * do NOT reference request after called this,
				 * modem may have freed it, unless you get
				 * -EBUSY
				 */
				ret = port_send_skb_to_md(port, skb, blk2);

				if (ret) {
					if (ret == -EBUSY && !blk2)
						ret = -EAGAIN;
					goto push_err_out;
				}
				return actual_count;
push_err_out:
				ccci_free_skb(skb);
				return ret;
			}
			/* consider this case as non-blocking */
			return -ENOMEM;
		}
	}
	return -ENODEV;
}

#endif

