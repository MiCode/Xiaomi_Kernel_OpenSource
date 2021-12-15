// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/kmemleak.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_fsm.h"
#include "port_smem.h"

#define TAG SMEM

#define DUMMY_PAGE_SIZE (128)
#define DUMMY_PADDING_CNT (5)

#define CTRL_PAGE_SIZE (1024)
#define CTRL_PAGE_NUM (32)

#define MD_EX_PAGE_SIZE (20*1024)
#define MD_EX_PAGE_NUM  (6)


/*
 *  Note : Moidy this size will affect dhl frame size in this page
 *  Minimum : 352B to reserve 256B for header frame
 */
#define MD_HW_PAGE_SIZE (512)

/* replace with HW page */
#define MD_BUF1_PAGE_SIZE (MD_HW_PAGE_SIZE)
#define MD_BUF1_PAGE_NUM  (72)
#define AP_BUF1_PAGE_SIZE (1024)
#define AP_BUF1_PAGE_NUM  (32)

#define MD_BUF2_0_PAGE_SIZE (MD_HW_PAGE_SIZE)
#define MD_BUF2_1_PAGE_SIZE (MD_HW_PAGE_SIZE)
#define MD_BUF2_2_PAGE_SIZE (MD_HW_PAGE_SIZE)

#define MD_BUF2_0_PAGE_NUM (64)
#define MD_BUF2_1_PAGE_NUM (64)
#define MD_BUF2_2_PAGE_NUM (256)

#define MD_MDM_PAGE_SIZE (MD_HW_PAGE_SIZE)
#define MD_MDM_PAGE_NUM  (32)

#define AP_MDM_PAGE_SIZE (1024)
#define AP_MDM_PAGE_NUM  (16)

#define MD_META_PAGE_SIZE (65*1024)
#define MD_META_PAGE_NUM (8)

#define AP_META_PAGE_SIZE (63*1024)

/*kernel 4.14 diff kernel 4.19
kernel4.14中
MT6765-----GEN93
MT6833-----GEN97
MT6853 (MT6877)------GEN97
#define AP_META_PAGE_SIZE (65*1024)

其他部分
#define AP_META_PAGE_SIZE (63*1024)
kernel4.19中
#define AP_META_PAGE_SIZE (63*1024)
*/

#define AP_META_PAGE_NUM (8)

struct ccci_ccb_config ccb_configs[] = {
	{SMEM_USER_CCB_DHL, P_CORE, CTRL_PAGE_SIZE,
			CTRL_PAGE_SIZE, CTRL_PAGE_SIZE*CTRL_PAGE_NUM,
			CTRL_PAGE_SIZE*CTRL_PAGE_NUM}, /* Ctrl */
	{SMEM_USER_CCB_DHL, P_CORE, MD_EX_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, MD_EX_PAGE_SIZE*MD_EX_PAGE_NUM,
			DUMMY_PAGE_SIZE},			/* exception */
	{SMEM_USER_CCB_DHL, P_CORE, MD_BUF1_PAGE_SIZE,
	 AP_BUF1_PAGE_SIZE, (MD_BUF1_PAGE_SIZE*MD_BUF1_PAGE_NUM),
			AP_BUF1_PAGE_SIZE*AP_BUF1_PAGE_NUM},/* PS */
	{SMEM_USER_CCB_DHL, P_CORE, MD_BUF2_0_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, MD_BUF2_0_PAGE_SIZE*MD_BUF2_0_PAGE_NUM,
			DUMMY_PAGE_SIZE},     /* HWLOGGER1 */
	{SMEM_USER_CCB_DHL, P_CORE, MD_BUF2_1_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, MD_BUF2_1_PAGE_SIZE*MD_BUF2_1_PAGE_NUM,
			DUMMY_PAGE_SIZE},     /* HWLOGGER2  */
	{SMEM_USER_CCB_DHL, P_CORE, MD_BUF2_2_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, MD_BUF2_2_PAGE_SIZE*MD_BUF2_2_PAGE_NUM,
			DUMMY_PAGE_SIZE},     /* HWLOGGER3 */
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE*DUMMY_PADDING_CNT,
		DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_MD_MONITOR, P_CORE, MD_MDM_PAGE_SIZE,
		 AP_MDM_PAGE_SIZE, MD_MDM_PAGE_SIZE*MD_MDM_PAGE_NUM,
		AP_MDM_PAGE_SIZE*AP_MDM_PAGE_NUM},     /* MDM */
	{SMEM_USER_CCB_META, P_CORE, MD_META_PAGE_SIZE,
		AP_META_PAGE_SIZE, MD_META_PAGE_SIZE*MD_META_PAGE_NUM,
		AP_META_PAGE_SIZE*AP_META_PAGE_NUM},   /* META */
};
unsigned int ccb_configs_len =
			sizeof(ccb_configs)/sizeof(struct ccci_ccb_config);
			
			
#ifdef DEBUG_FOR_CCB
static struct buffer_header *s_ccb_ctl_head_tbl;
static unsigned int *s_dl_last_w;
static unsigned int s_dl_active_bitmap;

static unsigned int dl_active_scan(void)
{
	unsigned int i;
	struct buffer_header *ptr = NULL;
	unsigned int bit_mask;

	if (!s_ccb_ctl_head_tbl)
		return 0;
	if (!s_dl_last_w)
		return 0;
	ptr = s_ccb_ctl_head_tbl;
	bit_mask = 0;

	for (i = 0; i < ccb_configs_len; i++) {
		if ((s_dl_last_w[i] != ptr[i].dl_write_index) ||
			(ptr[i].dl_read_index != ptr[i].dl_write_index)) {
			bit_mask |= 1 << i;
			s_dl_last_w[i] = ptr[i].dl_write_index;
		}
	}
	return bit_mask;
}

static inline int append_ccb_str(char buf[], int offset, int size,
				unsigned int id, unsigned int w, unsigned int r)
{
	int ret;

	if (!buf)
		return 0;

	ret = snprintf(&buf[offset], size - offset, "[%u]w:%u-r:%u,", id, w, r);
	if (ret > 0)
		return ret + offset;
	return 0;
}

static void ccb_fifo_peek(struct buffer_header *ptr)
{
	unsigned int i, r, w, wakeup_map = 0;
	int offset = 0;
	char *out_buf;

	out_buf = kmalloc(4096, GFP_ATOMIC);

	for (i = 0; i < ccb_configs_len; i++) {
		if (ptr[i].dl_read_index != ptr[i].dl_write_index) {
			r = ptr[i].dl_read_index;
			w = ptr[i].dl_write_index;
			wakeup_map |= 1 << i;
			offset = append_ccb_str(out_buf, offset, 4096, i, w, r);
		}
	}

	if (out_buf) {
		CCCI_NORMAL_LOG(0, "CCB", "Wakeup peek:0x%x %s\r\n", wakeup_map,
				out_buf);
		kfree(out_buf);
	} else
		CCCI_NORMAL_LOG(0, "CCB", "Wakeup peek bitmap: 0x%x\r\n",
					wakeup_map);
}

void mtk_ccci_ccb_info_peek(void)
{
	if (s_ccb_ctl_head_tbl)
		ccb_fifo_peek(s_ccb_ctl_head_tbl);
}
#endif

static enum hrtimer_restart smem_tx_timer_func(struct hrtimer *timer)
{
	struct ccci_smem_port *smem_port =
		container_of(timer, struct ccci_smem_port, notify_timer);

	ccci_md_send_ccb_tx_notify(smem_port->port->md_id,
		smem_port->core_id);
	return HRTIMER_NORESTART;
}

static void collect_ccb_info(int md_id, struct ccci_smem_port *smem_port)
{
	unsigned int i, j, len, curr_size;
	struct ccci_smem_region *prev = NULL, *curr = NULL;

	if (md_id != MD_SYS1)
		return;
	if (smem_port->user_id < SMEM_USER_CCB_START
		|| smem_port->user_id > SMEM_USER_CCB_END)
		return;
	/* check current port is CCB or not */
	for (i = SMEM_USER_CCB_START; i <= SMEM_USER_CCB_END; i++) {
		if (smem_port->user_id == i) {
			curr_size = 0;
			/* calculate length */
			for (j = 0; j < ccb_configs_len; j++) {
				/* search for first present */
				if (smem_port->user_id ==
					ccb_configs[j].user_id)
					break;
			}
			smem_port->ccb_ctrl_offset = j;
			for ( ; j < ccb_configs_len; j++) {
				/* traverse to last present */
				if (smem_port->user_id !=
					ccb_configs[j].user_id)
					break;
				len = ccb_configs[j].dl_buff_size +
				ccb_configs[j].ul_buff_size;
				curr_size += len;
			}
			/* align to 4k */
			curr_size = (curr_size + 0xFFF) & (~0xFFF);
			curr = ccci_md_get_smem_by_user_id(md_id, i);
			if (curr)
				curr->size = curr_size;
			CCCI_BOOTUP_LOG(md_id, TAG,
				"CCB user %d: ccb_ctrl_offset=%d, length=%d\n",
				i, smem_port->ccb_ctrl_offset, curr_size);
			/* init other member */
			smem_port->state = CCB_USER_INVALID;
			smem_port->wakeup = 0;
			smem_port->type = TYPE_CCB;
			init_waitqueue_head(&smem_port->rx_wq);
			hrtimer_init(&smem_port->notify_timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			smem_port->notify_timer.function = smem_tx_timer_func;
			break;
		}
	}

	/* refresh all CCB users' address, except the first one,
	 * because user's size has been re-calculated above
	 */
	if (SMEM_USER_CCB_END - SMEM_USER_CCB_START >= 1) {
		for (i = SMEM_USER_CCB_START + 1;
			 i <= SMEM_USER_CCB_END; i++) {
			curr = ccci_md_get_smem_by_user_id(md_id, i);
			prev = ccci_md_get_smem_by_user_id(md_id, i - 1);
			if (curr && prev) {
				curr->base_ap_view_phy =
					prev->base_ap_view_phy + prev->size;
				curr->base_ap_view_vir =
					prev->base_ap_view_vir + prev->size;
				curr->base_md_view_phy =
					prev->base_md_view_phy + prev->size;
				curr->offset = prev->offset + prev->size;
				CCCI_BOOTUP_LOG(md_id, TAG,
				"CCB user %d: offset=%d, size=%d, base_ap = 0x%x, base_md = 0x%x\n",
				i, curr->offset, curr->size,
				(unsigned int)curr->base_ap_view_phy,
				(unsigned int)curr->base_md_view_phy);
			}
		}
#ifdef DEBUG_FOR_CCB
		curr = ccci_md_get_smem_by_user_id(md_id,
						SMEM_USER_RAW_CCB_CTRL);
		if (curr)
			s_ccb_ctl_head_tbl =
				(struct buffer_header *)curr->base_ap_view_vir;
#endif
	}
}

int port_smem_tx_nofity(struct port_t *port, unsigned int user_data)
{
	struct ccci_smem_port *smem_port =
		(struct ccci_smem_port *)port->private_data;

	if (smem_port->type != TYPE_CCB)
		return -EFAULT;
	if ((smem_port->addr_phy == 0) || (smem_port->length == 0))
		return -EFAULT;
	if (!hrtimer_active(&(smem_port->notify_timer))) {
		smem_port->core_id = user_data;
		ccci_md_send_ccb_tx_notify(smem_port->port->md_id,
			smem_port->core_id);
		hrtimer_start(&(smem_port->notify_timer),
				ktime_set(0, 1000000), HRTIMER_MODE_REL);
	}
	return 0;
}

int port_smem_rx_poll(struct port_t *port, unsigned int user_data)
{
	struct ccci_smem_port *smem_port =
		(struct ccci_smem_port *)port->private_data;
#ifdef DEBUG_FOR_CCB
	struct buffer_header *buf = smem_port->ccb_vir_addr;
	unsigned char idx;
#endif
	int md_state, ret;
	unsigned long flags;
	int md_id = port->md_id;

	if (smem_port->type != TYPE_CCB)
		return -EFAULT;
	CCCI_DEBUG_LOG(md_id, TAG,
		"before wait event, bitmask=%x\n", user_data);
#ifdef DEBUG_FOR_CCB
	idx = smem_port->poll_save_idx;
	if (idx >= CCB_POLL_PTR_MAX - 2) {
		CCCI_ERROR_LOG(md_id, TAG,
			"invalid idx = %d\n", idx);
		return -EFAULT;
	}
	smem_port->last_poll_time[idx] = local_clock();
	smem_port->last_in[idx].al_id = buf[0].dl_alloc_index;
	smem_port->last_in[idx].fr_id = buf[0].dl_free_index;
	smem_port->last_in[idx].r_id = buf[0].dl_read_index;
	smem_port->last_in[idx].w_id = buf[0].dl_write_index;
	smem_port->last_in[idx + 1].al_id = buf[1].dl_alloc_index;
	smem_port->last_in[idx + 1].fr_id = buf[1].dl_free_index;
	smem_port->last_in[idx + 1].r_id = buf[1].dl_read_index;
	smem_port->last_in[idx + 1].w_id = buf[1].dl_write_index;
	smem_port->last_in[idx + 2].al_id = buf[2].dl_alloc_index;
	smem_port->last_in[idx + 2].fr_id = buf[2].dl_free_index;
	smem_port->last_in[idx + 2].r_id = buf[2].dl_read_index;
	smem_port->last_in[idx + 2].w_id = buf[2].dl_write_index;
	if (user_data == 0x01) {
		atomic_set(&smem_port->poll_processing[0], 1);
		smem_port->last_mask[0] = user_data;
		smem_port->last_poll_time[idx + 1] = local_clock();
	} else {
		atomic_set(&smem_port->poll_processing[1], 1);
		smem_port->last_mask[1] = user_data;
		smem_port->last_poll_time[idx + 2] = local_clock();
	}
#endif
	ret = wait_event_interruptible(smem_port->rx_wq,
		smem_port->wakeup & user_data);
	spin_lock_irqsave(&smem_port->write_lock, flags);
	smem_port->wakeup &= ~user_data;
	CCCI_DEBUG_LOG(md_id, TAG,
		"after wait event, wakeup=%x\n", smem_port->wakeup);
	spin_unlock_irqrestore(&smem_port->write_lock, flags);

	if (ret == -ERESTARTSYS)
		ret = -EINTR;
	else {
		md_state = ccci_fsm_get_md_state(md_id);
		if (md_state == WAITING_TO_STOP) {
			CCCI_REPEAT_LOG(md_id, TAG,
				"smem poll return, md_state = %d\n", md_state);
			ret = -ENODEV;
		}
	}
#ifdef DEBUG_FOR_CCB
	smem_port->last_poll_t_exit[idx] = local_clock();

	smem_port->last_out[idx].al_id = buf[0].dl_alloc_index;
	smem_port->last_out[idx].fr_id = buf[0].dl_free_index;
	smem_port->last_out[idx].r_id = buf[0].dl_read_index;
	smem_port->last_out[idx].w_id = buf[0].dl_write_index;

	smem_port->last_out[idx + 1].al_id = buf[1].dl_alloc_index;
	smem_port->last_out[idx + 1].fr_id = buf[1].dl_free_index;
	smem_port->last_out[idx + 1].r_id = buf[1].dl_read_index;
	smem_port->last_out[idx + 1].w_id = buf[1].dl_write_index;

	smem_port->last_out[idx + 2].al_id = buf[2].dl_alloc_index;
	smem_port->last_out[idx + 2].fr_id = buf[2].dl_free_index;
	smem_port->last_out[idx + 2].r_id = buf[2].dl_read_index;
	smem_port->last_out[idx + 2].w_id = buf[2].dl_write_index;
	if (user_data == 0x01) {
		atomic_set(&smem_port->poll_processing[0], 0);
		smem_port->last_poll_t_exit[idx + 1] = local_clock();
	} else {
		atomic_set(&smem_port->poll_processing[1], 0);
		smem_port->last_poll_t_exit[idx + 2] = local_clock();
	}
	idx += 3;

	if (idx >= CCB_POLL_PTR_MAX)
		idx = 0;
	smem_port->poll_save_idx = idx;
#endif
	return ret;
}

int port_smem_rx_wakeup(struct port_t *port)
{
	struct ccci_smem_port *smem_port =
		(struct ccci_smem_port *)port->private_data;
	unsigned long flags;
	int md_id = port->md_id;

	if (smem_port == NULL)
		return -EFAULT;

	if (smem_port->type != TYPE_CCB)
		return -EFAULT;
	if ((smem_port->addr_phy == 0) || (smem_port->length == 0))
		return -EFAULT;
	spin_lock_irqsave(&smem_port->write_lock, flags);
	smem_port->wakeup = 0xFFFFFFFF;
	spin_unlock_irqrestore(&smem_port->write_lock, flags);

	__pm_wakeup_event(port->rx_wakelock, jiffies_to_msecs(HZ));
	CCCI_DEBUG_LOG(md_id, TAG, "wakeup port.\n");
#ifdef DEBUG_FOR_CCB
	s_dl_active_bitmap |= dl_active_scan();
	smem_port->last_rx_wk_time = local_clock();
#endif
	wake_up_all(&smem_port->rx_wq);
	return 0;
}

void __iomem *get_smem_start_addr(int md_id,
	enum SMEM_USER_ID user_id, int *size_o)
{
	void __iomem *addr = NULL;
	struct ccci_smem_region *smem_region =
		ccci_md_get_smem_by_user_id(md_id, user_id);

	if (smem_region) {
		addr = smem_region->base_ap_view_vir;

		#if (MD_GENERATION < 6297)
		/* dbm addr returned to user should
		 * step over Guard pattern header
		 */
		if (user_id == SMEM_USER_RAW_DBM)
			addr += CCCI_SMEM_SIZE_DBM_GUARD;
		#endif

		if (size_o)
			*size_o = smem_region->size;
	}
	return addr;
}

phys_addr_t get_smem_phy_start_addr(int md_id,
	enum SMEM_USER_ID user_id, int *size_o)
{
	phys_addr_t addr = 0;
	struct ccci_smem_region *smem_region =
		ccci_md_get_smem_by_user_id(md_id, user_id);

	if (smem_region) {
		addr = smem_region->base_ap_view_phy;
		CCCI_NORMAL_LOG(md_id, TAG, "phy address: 0x%lx, ",
			(unsigned long)addr);
		if (size_o) {
			*size_o = smem_region->size;
			CCCI_NORMAL_LOG(md_id, TAG, "0x%x",
				*size_o);
		} else {
			CCCI_NORMAL_LOG(md_id, TAG, "size_0 is NULL(invalid)");
		}
	}
	return addr;
}
EXPORT_SYMBOL(get_smem_phy_start_addr);

static struct port_t *find_smem_port_by_user_id(int md_id, int user_id)
{
	return port_get_by_minor(md_id, user_id + CCCI_SMEM_MINOR_BASE);
}

long port_ccb_ioctl(struct port_t *port, unsigned int cmd, unsigned long arg)
{
	int md_id = port->md_id;
	long ret = 0;
	struct ccci_ccb_config in_ccb, out_ccb;
	struct ccci_smem_region *ccb_ctl =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_CCB_CTRL);
	struct ccb_ctrl_info ctrl_info;
	struct port_t *s_port = NULL;
	struct ccci_smem_port *smem_port =
		(struct ccci_smem_port *)port->private_data;

	if (ccb_ctl == NULL) {
		CCCI_ERROR_LOG(md_id, TAG, "ccb ctrl is NULL!\n");
		return -1;
	}

	/*
	 * all users share this ccb ctrl region, and
	 * port CCCI_SMEM_CH's initailization is special,
	 * so, these ioctl cannot use CCCI_SMEM_CH channel.
	 */
	switch (cmd) {
	case CCCI_IOC_CCB_CTRL_BASE:
		ret = put_user((unsigned int)ccb_ctl->base_ap_view_phy,
		(unsigned int __user *)arg);
		break;
	case CCCI_IOC_CCB_CTRL_LEN:
		ret = put_user((unsigned int)ccb_ctl->size,
				(unsigned int __user *)arg);
		break;
	case CCCI_IOC_CCB_CTRL_INFO:
		if (copy_from_user(&ctrl_info, (void __user *)arg,
			sizeof(struct ccb_ctrl_info))) {
			CCCI_ERROR_LOG(md_id, TAG,
			"get ccb ctrl fail: copy_from_user fail!\n");
			ret = -EINVAL;
			break;
		}
		/*user id counts from ccb start*/
		if (ctrl_info.user_id + SMEM_USER_CCB_START >
				SMEM_USER_CCB_END) {
			CCCI_ERROR_LOG(md_id, TAG,
				"get ccb ctrl fail: user_id = %d!\n",
				ctrl_info.user_id);
			ret = -EINVAL;
			break;
		}
		/*get ctrl info by user id*/
		s_port = find_smem_port_by_user_id(md_id,
			ctrl_info.user_id + SMEM_USER_CCB_START);
		if (!s_port) {
			CCCI_ERROR_LOG(md_id, TAG,
				"get ccb port fail: user_id = %d!\n",
				ctrl_info.user_id);
			ret = -EINVAL;
			break;
		}
		CCCI_NORMAL_LOG(md_id, TAG, "find ccb port %s for user%d!\n",
			s_port->name, ctrl_info.user_id + SMEM_USER_CCB_START);
		smem_port = (struct ccci_smem_port *)s_port->private_data;
		ctrl_info.ctrl_offset = smem_port->ccb_ctrl_offset;
		ctrl_info.ctrl_addr = (unsigned int)ccb_ctl->base_ap_view_phy;
		ctrl_info.ctrl_length = (unsigned int)ccb_ctl->size;
		if (copy_to_user((void __user *)arg, &ctrl_info,
			sizeof(struct ccb_ctrl_info))) {
			CCCI_ERROR_LOG(md_id, TAG,
				"copy_to_user ccb ctrl failed !!\n");
			ret = -EINVAL;
			break;
		}
		/*set smem state as OK if get ccb ctrl success*/
		smem_port->state = CCB_USER_OK;
		break;
	case CCCI_IOC_GET_CCB_CONFIG_LENGTH:
		CCCI_NORMAL_LOG(md_id, TAG, "ccb_configs_len: %d\n",
		ccb_configs_len);

		ret = put_user(ccb_configs_len, (unsigned int __user *)arg);
		break;
	case CCCI_IOC_GET_CCB_CONFIG:
		if (copy_from_user(&in_ccb, (void __user *)arg,
			sizeof(struct ccci_ccb_config))) {
			CCCI_ERROR_LOG(md_id, TAG,
				"set user_id fail: copy_from_user fail!\n");
			ret = -EINVAL;
			break;
		}
		/* use user_id as input param, which is the array index,
		 * and it will override user space's ID value
		 */
		if (in_ccb.user_id >= ccb_configs_len) {
			ret = -EINVAL;
			break;
		}
		memcpy(&out_ccb, &ccb_configs[in_ccb.user_id],
		sizeof(struct ccci_ccb_config));
		/* user space's CCB array index is count from zero,
		 * as it only deal with CCB user, no raw user
		 */
		out_ccb.user_id -= SMEM_USER_CCB_START;
		if (copy_to_user((void __user *)arg, &out_ccb,
			sizeof(struct ccci_ccb_config)))
			CCCI_ERROR_LOG(md_id, TAG,
				"copy_to_user ccb failed !!\n");
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

long port_smem_ioctl(struct port_t *port, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	int md_id = port->md_id;
	unsigned int data;
	struct ccci_smem_port *smem_port =
		(struct ccci_smem_port *)port->private_data;

	switch (cmd) {
	case CCCI_IOC_SMEM_BASE:
		smem_port = (struct ccci_smem_port *)port->private_data;
		CCCI_NORMAL_LOG(md_id, TAG, "smem_port->addr_phy=%lx\n",
			(unsigned long)smem_port->addr_phy);
		ret = put_user((unsigned int)smem_port->addr_phy,
				(unsigned int __user *)arg);
		break;
	case CCCI_IOC_SMEM_LEN:
		smem_port = (struct ccci_smem_port *)port->private_data;
		ret = put_user((unsigned int)smem_port->length,
				(unsigned int __user *)arg);
		break;
	case CCCI_IOC_CCB_CTRL_OFFSET:
		CCCI_REPEAT_LOG(md_id, TAG,
			"rx_ch who invoke CCCI_IOC_CCB_CTRL_OFFSET:%d\n",
			port->rx_ch);
		if ((smem_port->addr_phy == 0) || (smem_port->length == 0)) {
			ret = -EFAULT;
			break;
		}

		smem_port = (struct ccci_smem_port *)port->private_data;
		ret = put_user((unsigned int)smem_port->ccb_ctrl_offset,
				(unsigned int __user *)arg);
		CCCI_REPEAT_LOG(md_id, TAG,
			"get ctrl_offset=%d\n", smem_port->ccb_ctrl_offset);

		break;

	case CCCI_IOC_SMEM_TX_NOTIFY:
		if (copy_from_user(&data, (void __user *)arg,
			sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, TAG,
				"smem tx notify fail: copy_from_user fail!\n");
			ret = -EFAULT;
		} else
			ret = port_smem_tx_nofity(port, data);
		break;
	case CCCI_IOC_SMEM_RX_POLL:

		if (copy_from_user(&data, (void __user *)arg,
			sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, TAG,
			"smem rx poll fail: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			ret = port_smem_rx_poll(port, data);
		}
		break;
	case CCCI_IOC_SMEM_SET_STATE:
		smem_port = (struct ccci_smem_port *)port->private_data;
		if (copy_from_user(&data, (void __user *)arg,
			sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, TAG,
				"smem set state fail: copy_from_user fail!\n");
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
		ret = -1;
		break;
	}

	return ret;
}
static long smem_dev_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	struct port_t *port = file->private_data;
	int ch = port->rx_ch;

	if (ch == CCCI_SMEM_CH)
		ret = port_smem_ioctl(port, cmd, arg);
	else if (ch == CCCI_CCB_CTRL)
		ret = port_ccb_ioctl(port, cmd, arg);

	if (ret == -1)
		ret = port_dev_ioctl(file, cmd, arg);
	return ret;
}
static int smem_dev_mmap(struct file *fp, struct vm_area_struct *vma)
{
	struct port_t *port = fp->private_data;
	struct ccci_smem_port *smem_port =
		(struct ccci_smem_port *)port->private_data;
	int md_id = port->md_id;
	int ret;
	unsigned long pfn, len;
	struct ccci_smem_region *ccb_ctl =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_CCB_CTRL);

	if ((smem_port->addr_phy == 0) || (smem_port->length == 0))
		return -EFAULT;

	switch (port->rx_ch) {
	case CCCI_CCB_CTRL:
		CCCI_NORMAL_LOG(md_id, CHAR,
			"remap control addr:0x%llx len:%d  map-len:%lu\n",
			(unsigned long long)ccb_ctl->base_ap_view_phy,
			ccb_ctl->size, vma->vm_end - vma->vm_start);
		if (vma->vm_end < vma->vm_start) {
			CCCI_ERROR_LOG(md_id, CHAR,
				"vm_end:%lu < vm_start:%lu request from %s\n",
				vma->vm_end, vma->vm_start, port->name);
			return -EINVAL;
		}
		len = vma->vm_end - vma->vm_start;
		if (len > ccb_ctl->size) {
			CCCI_ERROR_LOG(md_id, CHAR,
				"invalid mm size request from %s\n",
				port->name);
			return -EINVAL;
		}

		pfn = ccb_ctl->base_ap_view_phy;
		pfn >>= PAGE_SHIFT;
		/* ensure that memory does not get swapped to disk */
		vma->vm_flags |= VM_IO;
		/* ensure non-cacheable */
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		ret = remap_pfn_range(vma, vma->vm_start, pfn,
				len, vma->vm_page_prot);
		if (ret) {
			CCCI_ERROR_LOG(md_id, CHAR,
				"remap failed %d/%lx, 0x%llx -> 0x%llx\n",
				ret, pfn,
				(unsigned long long)ccb_ctl->base_ap_view_phy,
				(unsigned long long)vma->vm_start);
			return -EAGAIN;
		}

		CCCI_NORMAL_LOG(md_id, CHAR,
			"remap succeed %lx, 0x%llx -> 0x%llx\n", pfn,
			(unsigned long long)ccb_ctl->base_ap_view_phy,
			(unsigned long long)vma->vm_start);
		break;

	case CCCI_SMEM_CH:
		CCCI_NORMAL_LOG(md_id, CHAR,
			"remap addr:0x%llx len:%d  map-len:%lu\n",
			(unsigned long long)smem_port->addr_phy,
			smem_port->length,
			vma->vm_end - vma->vm_start);
		if (vma->vm_end < vma->vm_start) {
			CCCI_ERROR_LOG(md_id, CHAR,
				"vm_end:%lu < vm_start:%lu request from %s\n",
				vma->vm_end, vma->vm_start, port->name);
			return -EINVAL;
		}
		len = vma->vm_end - vma->vm_start;
		if (len > smem_port->length) {
			CCCI_ERROR_LOG(md_id, CHAR,
				"invalid mm size request from %s\n",
				port->name);
			return -EINVAL;
		}

		pfn = smem_port->addr_phy;
		pfn >>= PAGE_SHIFT;
		/* ensure that memory does not get swapped to disk */
		vma->vm_flags |= VM_IO;
		/* ensure non-cacheable */
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		ret = remap_pfn_range(vma, vma->vm_start, pfn, len,
				vma->vm_page_prot);
		if (ret) {
			CCCI_ERROR_LOG(md_id, CHAR,
				"remap failed %d/%lx, 0x%llx -> 0x%llx\n",
				ret, pfn,
				(unsigned long long)smem_port->addr_phy,
				(unsigned long long)vma->vm_start);
			return -EAGAIN;
		}

		break;

	default:
		return -EPERM;
	}
	return 0;
}
static const struct file_operations smem_dev_fops = {
	.owner = THIS_MODULE,
	.open = &port_dev_open, /*use default API*/
	.read = &port_dev_read, /*use default API*/
	.write = &port_dev_write, /*use default API*/
	.release = &port_dev_close,/*use default API*/
	.unlocked_ioctl = &smem_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = &port_dev_compat_ioctl,
#endif
	.mmap = &smem_dev_mmap,

};

int port_smem_init(struct port_t *port)
{
	struct cdev *dev = NULL;
	int ret = 0;
	int md_id = port->md_id;
	struct ccci_smem_port *smem_port = NULL;
	struct ccci_smem_region *smem_region =
		ccci_md_get_smem_by_user_id(md_id, port->minor);

#if (MD_GENERATION < 6293)
	if (!smem_region) {
		CCCI_ERROR_LOG(md_id, CHAR,
			"smem port %d not available\n", port->minor);
		return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
	}
#endif
	/*Set SMEM MINOR base*/
	port->minor += CCCI_SMEM_MINOR_BASE;
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
		kmemleak_ignore(dev);
		if (unlikely(!dev)) {
			CCCI_ERROR_LOG(port->md_id, CHAR,
				"alloc smem char dev fail!!\n");
			return -1;
		}
		cdev_init(dev, &smem_dev_fops);
		dev->owner = THIS_MODULE;
		ret = cdev_add(dev,
			MKDEV(port->major, port->minor_base + port->minor), 1);
		ret = ccci_register_dev_node(port->name, port->major,
				port->minor_base + port->minor);
		port->interception = 0;
		port->flags |= PORT_F_ADJUST_HEADER;
	}

	port->private_data = smem_port =
		kzalloc(sizeof(struct ccci_smem_port), GFP_KERNEL);
	if (smem_port == NULL) {
		CCCI_ERROR_LOG(port->md_id, CHAR,
			"alloc ccci_smem_port fail\n");
		return -1;
	}
	kmemleak_ignore(smem_port);
	/*user ID is from 0*/
	smem_port->user_id = port->minor - CCCI_SMEM_MINOR_BASE;
	spin_lock_init(&smem_port->write_lock);
	smem_port->port = port;
	if (!smem_region || smem_region->size == 0) {
		smem_port->addr_phy = 0;
		smem_port->addr_vir = 0;
		smem_port->length = 0;
	} else {
		smem_port->addr_phy = smem_region->base_ap_view_phy;
		smem_port->addr_vir = smem_region->base_ap_view_vir;
		smem_port->length = smem_region->size;
		/* this may override addr_phy/vir and length */
		collect_ccb_info(md_id, smem_port);
	}
#ifdef DEBUG_FOR_CCB
{
	struct ccci_smem_region *ccb_ctl =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_CCB_CTRL);
	smem_port->ccb_vir_addr =
		(struct buffer_header *)ccb_ctl->base_ap_view_vir;
	smem_port->poll_save_idx = 0;
}
	s_dl_last_w = kmalloc(sizeof(int) * ccb_configs_len, GFP_KERNEL);
	if (!s_dl_last_w) {
		CCCI_ERROR_LOG(port->md_id, CHAR,
			"%s:kmalloc s_dl_last_w fail\n",
			__func__);
		return -1;
	}
	kmemleak_ignore(s_dl_last_w);
#endif

	return 0;
}

static void port_smem_md_state_notify(struct port_t *port, unsigned int state)
{
	switch (state) {
	case EXCEPTION:
	case RESET:
	case WAITING_TO_STOP:
		port_smem_rx_wakeup(port);
		break;
	default:
		break;
	};
}

static void port_smem_queue_state_notify(struct port_t *port, int dir, int qno,
	unsigned int qstate)
{
	if (port->rx_ch == CCCI_SMEM_CH && qstate == RX_IRQ)
		port_smem_rx_wakeup(port);
}
#ifdef DEBUG_FOR_CCB
static void port_smem_dump_info(struct port_t *port, unsigned int flag)
{
	struct ccci_smem_port *smem_port =
		(struct ccci_smem_port *)port->private_data;
	unsigned long long ts = 0, ts_e = 0, ts_s = 0;
	unsigned long nsec_rem = 0, nsec_rem_s = 0, nsec_rem_e = 0;
	unsigned char idx;

	if (smem_port->last_poll_time[0] == 0 &&
		smem_port->last_poll_t_exit[0] == 0
		/*&& smem_port->last_rx_wk_time ==0 */)
		return;

	ts = smem_port->last_rx_wk_time;
	nsec_rem = do_div(ts, NSEC_PER_SEC);
	CCCI_MEM_LOG_TAG(port->md_id, TAG,
		"ccb port_smem(%d) poll history: last_wake<%llu.%06lu>, poll(%d/%d, 0x%x/0x%x)\n",
		smem_port->user_id, ts, nsec_rem / 1000,
		atomic_read(&smem_port->poll_processing[0]),
		atomic_read(&smem_port->poll_processing[1]),
		smem_port->last_mask[0], smem_port->last_mask[1]);
	for (idx = 0; idx < CCB_POLL_PTR_MAX; idx++) {
		ts_s = smem_port->last_poll_time[idx];
		nsec_rem_s = do_div(ts_s, NSEC_PER_SEC);
		ts_e = smem_port->last_poll_t_exit[idx];
		nsec_rem_e = do_div(ts_e, NSEC_PER_SEC);
		CCCI_MEM_LOG(port->md_id, TAG,
			"<%llu.%06lu ~ %llu.%06lu> ",
			ts_s, nsec_rem_s/1000, ts_e, nsec_rem_e/1000);
	}
	CCCI_MEM_LOG(port->md_id, TAG, "\n");
	for (idx = 0; idx < CCB_POLL_PTR_MAX;) {
		CCCI_MEM_LOG(port->md_id, TAG,
			"0x%x, 0x%x, 0x%x, 0x%x; 0x%x, 0x%x, 0x%x, 0x%x; 0x%x, 0x%x, 0x%x, 0x%x",
			smem_port->last_in[idx + 0].al_id,
			smem_port->last_in[idx + 0].fr_id,
			smem_port->last_in[idx + 0].r_id,
			smem_port->last_in[idx + 0].w_id,
			smem_port->last_in[idx + 1].al_id,
			smem_port->last_in[idx + 1].fr_id,
			smem_port->last_in[idx + 1].r_id,
			smem_port->last_in[idx + 1].w_id,
			smem_port->last_in[idx + 2].al_id,
			smem_port->last_in[idx + 2].fr_id,
			smem_port->last_in[idx + 2].r_id,
			smem_port->last_in[idx + 2].w_id);
		CCCI_MEM_LOG(port->md_id, TAG,
			"  ~ 0x%x, 0x%x, 0x%x, 0x%x; 0x%x, 0x%x, 0x%x, 0x%x; 0x%x, 0x%x, 0x%x, 0x%x\n",
			smem_port->last_out[idx + 0].al_id,
			smem_port->last_out[idx + 0].fr_id,
			smem_port->last_out[idx + 0].r_id,
			smem_port->last_out[idx + 0].w_id,
			smem_port->last_out[idx + 1].al_id,
			smem_port->last_out[idx + 1].fr_id,
			smem_port->last_out[idx + 1].r_id,
			smem_port->last_out[idx + 1].w_id,
			smem_port->last_out[idx + 2].al_id,
			smem_port->last_out[idx + 2].fr_id,
			smem_port->last_out[idx + 2].r_id,
			smem_port->last_out[idx + 2].w_id);
		idx += 3;
	}
	CCCI_NORMAL_LOG(0, "CCB", "CCB active bitmap:0x%x\r\n",
			s_dl_active_bitmap);
	s_dl_active_bitmap = 0;
}
#endif

struct port_ops smem_port_ops = {
	.init = &port_smem_init,
	.md_state_notify = &port_smem_md_state_notify,
	.queue_state_notify = &port_smem_queue_state_notify,
#ifdef DEBUG_FOR_CCB
	.dump_info = port_smem_dump_info,
#endif

};

