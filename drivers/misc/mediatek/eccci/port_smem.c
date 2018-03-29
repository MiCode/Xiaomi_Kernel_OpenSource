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

#include "port_smem.h"
#include "port_proxy.h"
#define TAG "smem"
/* FIXME, global structures are indexed by SMEM_USER_ID */
static struct ccci_smem_port md1_ccci_smem_ports[] = {
	{SMEM_USER_RAW_DBM, TYPE_RAW, }, /* mt_pbm.c */
	{SMEM_USER_CCB_DHL, TYPE_CCB, }, /* CCB DHL */
	{SMEM_USER_RAW_DHL, TYPE_RAW, }, /* raw region for DHL settings */
	{SMEM_USER_RAW_NETD, TYPE_RAW, }, /* for direct tethering */
	{SMEM_USER_RAW_USB, TYPE_RAW, }, /* for derect tethering */
};
static struct ccci_smem_port md3_ccci_smem_ports[] = {
	{SMEM_USER_RAW_DBM, TYPE_RAW, }, /* mt_pbm.c */
};
struct tx_notify_task md1_tx_notify_tasks[SMEM_USER_MAX];

static enum hrtimer_restart smem_tx_timer_func(struct hrtimer *timer)
{
	struct tx_notify_task *notify_task = container_of(timer, struct tx_notify_task, notify_timer);

	port_proxy_send_ccb_tx_notify_to_md(notify_task->port->port_proxy, notify_task->core_id);
	return HRTIMER_NORESTART;
}

int port_smem_init(struct ccci_port *port)
{
	/* FIXME, port->minor is indexed by SMEM_USER_ID */
	switch (port->md_id) {
	case MD_SYS1:
		port->private_data = &(md1_ccci_smem_ports[port->minor]);
		md1_ccci_smem_ports[port->minor].port = port;
		break;
	case MD_SYS3:
		port->private_data = &(md3_ccci_smem_ports[port->minor]);
		md3_ccci_smem_ports[port->minor].port = port;
		break;
	default:
		return -1;
	}
	port->minor += CCCI_SMEM_MINOR_BASE;

	return 0;
}

int port_smem_tx_nofity(struct ccci_port *port, unsigned int user_data)
{
	struct ccci_smem_port *smem_port = (struct ccci_smem_port *)port->private_data;

	if (smem_port->type != TYPE_CCB)
		return -EFAULT;
	if (!hrtimer_active(&(md1_tx_notify_tasks[smem_port->user_id].notify_timer))) {
		port_proxy_send_ccb_tx_notify_to_md(port->port_proxy, user_data);
		md1_tx_notify_tasks[smem_port->user_id].core_id = user_data;
		md1_tx_notify_tasks[smem_port->user_id].port = port;
		hrtimer_start(&(md1_tx_notify_tasks[smem_port->user_id].notify_timer),
				ktime_set(0, 1000000), HRTIMER_MODE_REL);
	}
	return 0;
}

int port_smem_rx_poll(struct ccci_port *port)
{
	struct ccci_smem_port *smem_port = (struct ccci_smem_port *)port->private_data;
	int ret;

	if (smem_port->type != TYPE_CCB)
		return -EFAULT;
	ret = wait_event_interruptible_exclusive(smem_port->rx_wq, smem_port->wakeup != 0);
	smem_port->wakeup = 0;
	if (ret == -ERESTARTSYS)
		ret = -EINTR;
	return ret;
}

int port_smem_rx_wakeup(struct ccci_port *port)
{
	struct ccci_smem_port *smem_port = (struct ccci_smem_port *)port->private_data;

	if (smem_port->type != TYPE_CCB)
		return -EFAULT;
	smem_port->wakeup = 1;
	wake_up_all(&smem_port->rx_wq);
	return 0;
}

int port_smem_cfg(int md_id, struct ccci_smem_layout *smem_layout)
{
	int i;

	switch (md_id) {
	case MD_SYS1:
#ifdef FEATURE_DBM_SUPPORT
		md1_ccci_smem_ports[SMEM_USER_RAW_DBM].addr_vir = smem_layout->ccci_exp_smem_dbm_debug_vir +
								CCCI_SMEM_DBM_GUARD_SIZE;
		md1_ccci_smem_ports[SMEM_USER_RAW_DBM].length = CCCI_SMEM_DBM_SIZE;
#endif

		md1_ccci_smem_ports[SMEM_USER_CCB_DHL].addr_vir = smem_layout->ccci_ccb_dhl_base_vir;
		md1_ccci_smem_ports[SMEM_USER_CCB_DHL].addr_phy = smem_layout->ccci_ccb_dhl_base_phy;
		md1_ccci_smem_ports[SMEM_USER_CCB_DHL].length = smem_layout->ccci_ccb_dhl_size;

		md1_ccci_smem_ports[SMEM_USER_RAW_DHL].addr_vir = smem_layout->ccci_raw_dhl_base_vir;
		md1_ccci_smem_ports[SMEM_USER_RAW_DHL].addr_phy = smem_layout->ccci_raw_dhl_base_phy;
		md1_ccci_smem_ports[SMEM_USER_RAW_DHL].length = smem_layout->ccci_raw_dhl_size;

		md1_ccci_smem_ports[SMEM_USER_RAW_NETD].addr_vir = smem_layout->ccci_dt_netd_smem_base_vir;
		md1_ccci_smem_ports[SMEM_USER_RAW_NETD].addr_phy = smem_layout->ccci_dt_netd_smem_base_phy;
		md1_ccci_smem_ports[SMEM_USER_RAW_NETD].length = smem_layout->ccci_dt_netd_smem_size;

		md1_ccci_smem_ports[SMEM_USER_RAW_USB].addr_vir = smem_layout->ccci_dt_usb_smem_base_vir;
		md1_ccci_smem_ports[SMEM_USER_RAW_USB].addr_phy = smem_layout->ccci_dt_usb_smem_base_phy;
		md1_ccci_smem_ports[SMEM_USER_RAW_USB].length = smem_layout->ccci_dt_usb_smem_size;

		for (i = 0; i < ARRAY_SIZE(md1_ccci_smem_ports); i++) {
			md1_ccci_smem_ports[i].state = CCB_USER_INVALID;
			md1_ccci_smem_ports[i].wakeup = 0;
			init_waitqueue_head(&(md1_ccci_smem_ports[i].rx_wq));
		}

		for (i = 0; i < ARRAY_SIZE(md1_tx_notify_tasks); i++) {
			hrtimer_init(&(md1_tx_notify_tasks[i].notify_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			md1_tx_notify_tasks[i].notify_timer.function = smem_tx_timer_func;
		}
		break;
	case MD_SYS3:
#ifdef FEATURE_DBM_SUPPORT
		md3_ccci_smem_ports[SMEM_USER_RAW_DBM].addr_vir = smem_layout->ccci_exp_smem_dbm_debug_vir +
								CCCI_SMEM_DBM_GUARD_SIZE;
		md3_ccci_smem_ports[SMEM_USER_RAW_DBM].length = CCCI_SMEM_DBM_SIZE;
#endif
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

void __iomem *get_smem_start_addr(int md_id, SMEM_USER_ID user_id, int *size_o)
{
	void __iomem *addr = NULL;

	switch (md_id) {
	case MD_SYS1:
		if (user_id < 0 || user_id >= ARRAY_SIZE(md1_ccci_smem_ports))
			return NULL;
		addr = (void __iomem *)md1_ccci_smem_ports[user_id].addr_vir;
		if (size_o)
			*size_o = md1_ccci_smem_ports[user_id].length;
		break;
	case MD_SYS3:
		if (user_id < 0 || user_id >= ARRAY_SIZE(md3_ccci_smem_ports))
			return NULL;
		addr = (void __iomem *)md3_ccci_smem_ports[user_id].addr_vir;
		if (size_o)
			*size_o = md3_ccci_smem_ports[user_id].length;
		break;
	default:
		return NULL;
	}
	return addr;
}

long port_smem_ioctl(struct ccci_port *port, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	int md_id = port->md_id;
	unsigned int data;
	struct ccci_smem_port *smem_port;

	if (port->rx_ch == CCCI_SMEM_CH)
		return ret = -EPERM;
	switch (cmd) {
	case CCCI_IOC_SMEM_BASE:
		smem_port = (struct ccci_smem_port *)port->private_data;
		ret = put_user((unsigned int)smem_port->addr_phy,
			(unsigned int __user *)arg);
		break;
	case CCCI_IOC_SMEM_LEN:
		smem_port = (struct ccci_smem_port *)port->private_data;
		ret = put_user((unsigned int)smem_port->length,
			(unsigned int __user *)arg);
		break;
	case CCCI_IOC_SMEM_TX_NOTIFY:
		if (copy_from_user(&data, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, TAG, "smem tx notify fail: copy_from_user fail!\n");
			ret = -EFAULT;
		} else
			ret = port_smem_tx_nofity(port, data);
		break;
	case CCCI_IOC_SMEM_RX_POLL:
		ret = port_smem_rx_poll(port);
		break;
	case CCCI_IOC_SMEM_SET_STATE:
		smem_port = (struct ccci_smem_port *)port->private_data;
		if (copy_from_user(&data, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, TAG, "smem set state fail: copy_from_user fail!\n");
			ret = -EFAULT;
		} else
			smem_port->state = data;
		break;
	case CCCI_IOC_SMEM_GET_STATE:
		smem_port = (struct ccci_smem_port *)port->private_data;
		ret = put_user((unsigned int)smem_port->state,
				(unsigned int __user *)arg);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}
int port_smem_mmap(struct ccci_port *port, struct vm_area_struct *vma)
{
	struct ccci_smem_port *smem_port = (struct ccci_smem_port *)port->private_data;
	int len, ret;
	unsigned long pfn;
	int md_id = port->md_id;

	CCCI_NORMAL_LOG(md_id, CHAR, "remap addr:0x%llx len:%d  map-len:%lu\n",
			(unsigned long long)smem_port->addr_phy, smem_port->length, vma->vm_end - vma->vm_start);
	if ((vma->vm_end - vma->vm_start) > smem_port->length) {
		CCCI_ERROR_LOG(md_id, CHAR,
			     "invalid mm size request from %s\n", port->name);
		return -EINVAL;
	}

	len =
	    (vma->vm_end - vma->vm_start) <
	    smem_port->length ? vma->vm_end - vma->vm_start : smem_port->length;
	pfn = smem_port->addr_phy;
	pfn >>= PAGE_SHIFT;
	/* ensure that memory does not get swapped to disk */
	vma->vm_flags |= VM_IO;
	/* ensure non-cacheable */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);
	if (ret) {
		CCCI_ERROR_LOG(md_id, CHAR, "remap failed %d/%lx, 0x%llx -> 0x%llx\n", ret, pfn,
			(unsigned long long)smem_port->addr_phy, (unsigned long long)vma->vm_start);
		return -EAGAIN;
	}
	return 0;
}
