/* Copyright (c) 2008-2018, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/diagchar.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/msm_mhi.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include <asm/current.h>
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_cntl.h"
#include "diag_dci.h"
#include "diag_debugfs.h"
#include "diag_masks.h"
#include "diagfwd_bridge.h"
#include "diag_usb.h"
#include "diag_memorydevice.h"
#include "diag_mux.h"
#include "diag_ipc_logging.h"
#include "diagfwd_peripheral.h"

#include <linux/coresight-stm.h>
#include <linux/kernel.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

MODULE_DESCRIPTION("Diag Char Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

#define MIN_SIZ_ALLOW 4
#define INIT	1
#define EXIT	-1
struct diagchar_dev *driver;
struct diagchar_priv {
	int pid;
};

#define USER_SPACE_RAW_DATA	0
#define USER_SPACE_HDLC_DATA	1

/* Memory pool variables */
/* Used for copying any incoming packet from user space clients. */
static unsigned int poolsize = 12;
module_param(poolsize, uint, 0);

/*
 * Used for HDLC encoding packets coming from the user
 * space.
 */
static unsigned int poolsize_hdlc = 10;
module_param(poolsize_hdlc, uint, 0);

/*
 * This is used for incoming DCI requests from the user space clients.
 * Don't expose itemsize as it is internal.
 */
static unsigned int poolsize_user = 8;
module_param(poolsize_user, uint, 0);

/*
 * USB structures allocated for writing Diag data generated on the Apps to USB.
 * Don't expose itemsize as it is constant.
 */
static unsigned int itemsize_usb_apps = sizeof(struct diag_request);
static unsigned int poolsize_usb_apps = 10;
module_param(poolsize_usb_apps, uint, 0);

/* Used for DCI client buffers. Don't expose itemsize as it is constant. */
static unsigned int poolsize_dci = 10;
module_param(poolsize_dci, uint, 0);

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
/* Used for reading data from the remote device. */
static unsigned int itemsize_mdm = DIAG_MDM_BUF_SIZE;
static unsigned int poolsize_mdm = 18;
module_param(itemsize_mdm, uint, 0);
module_param(poolsize_mdm, uint, 0);

/*
 * Used for reading DCI data from the remote device.
 * Don't expose poolsize for DCI data. There is only one read buffer
 */
static unsigned int itemsize_mdm_dci = DIAG_MDM_BUF_SIZE;
static unsigned int poolsize_mdm_dci = 1;
module_param(itemsize_mdm_dci, uint, 0);

/*
 * Used for USB structues associated with a remote device.
 * Don't expose the itemsize since it is constant.
 */
static unsigned int itemsize_mdm_usb = sizeof(struct diag_request);
static unsigned int poolsize_mdm_usb = 18;
module_param(poolsize_mdm_usb, uint, 0);

/*
 * Used for writing read DCI data to remote peripherals. Don't
 * expose poolsize for DCI data. There is only one read
 * buffer. Add 6 bytes for DCI header information: Start (1),
 * Version (1), Length (2), Tag (2)
 */
static unsigned int itemsize_mdm_dci_write = DIAG_MDM_DCI_BUF_SIZE;
static unsigned int poolsize_mdm_dci_write = 1;
module_param(itemsize_mdm_dci_write, uint, 0);

/*
 * Used for USB structures associated with a remote SMUX
 * device Don't expose the itemsize since it is constant
 */
static unsigned int itemsize_qsc_usb = sizeof(struct diag_request);
static unsigned int poolsize_qsc_usb = 8;
module_param(poolsize_qsc_usb, uint, 0);
#endif

/* This is the max number of user-space clients supported at initialization*/
static unsigned int max_clients = 15;
static unsigned int threshold_client_limit = 50;
module_param(max_clients, uint, 0);

/* Timer variables */
static struct timer_list drain_timer;
static int timer_in_progress;

/*
 * Diag Mask clear variable
 * Used for clearing masks upon
 * USB disconnection and stopping ODL
 */
static int diag_mask_clear_param = 1;
module_param(diag_mask_clear_param, int, 0644);

struct diag_apps_data_t {
	void *buf;
	uint32_t len;
	int ctxt;
};

static struct diag_apps_data_t hdlc_data;
static struct diag_apps_data_t non_hdlc_data;
static struct mutex apps_data_mutex;

#define DIAGPKT_MAX_DELAYED_RSP 0xFFFF

#ifdef DIAG_DEBUG
uint16_t diag_debug_mask;
void *diag_ipc_log;
#endif

static void diag_md_session_close(int pid);

/*
 * Returns the next delayed rsp id. If wrapping is enabled,
 * wraps the delayed rsp id to DIAGPKT_MAX_DELAYED_RSP.
 */
static uint16_t diag_get_next_delayed_rsp_id(void)
{
	uint16_t rsp_id = 0;

	mutex_lock(&driver->delayed_rsp_mutex);
	rsp_id = driver->delayed_rsp_id;
	if (rsp_id < DIAGPKT_MAX_DELAYED_RSP)
		rsp_id++;
	else {
		if (wrap_enabled) {
			rsp_id = 1;
			wrap_count++;
		} else
			rsp_id = DIAGPKT_MAX_DELAYED_RSP;
	}
	driver->delayed_rsp_id = rsp_id;
	mutex_unlock(&driver->delayed_rsp_mutex);

	return rsp_id;
}

static int diag_switch_logging(struct diag_logging_mode_param_t *param);

#define COPY_USER_SPACE_OR_EXIT(buf, data, length)		\
do {								\
	if ((count < ret+length) || (copy_to_user(buf,		\
			(void *)&data, length))) {		\
		ret = -EFAULT;					\
		goto exit;					\
	}							\
	ret += length;						\
} while (0)

#define COPY_USER_SPACE_OR_ERR(buf, data, length)		\
do {								\
	if ((count < ret+length) || (copy_to_user(buf,		\
			(void *)&data, length))) {		\
		ret = -EFAULT;					\
		break;						\
	}							\
	ret += length;						\
} while (0)

static void drain_timer_func(unsigned long data)
{
	queue_work(driver->diag_wq , &(driver->diag_drain_work));
}

static void diag_drain_apps_data(struct diag_apps_data_t *data)
{
	int err = 0;

	if (!data || !data->buf)
		return;

	err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
			     data->ctxt);
	if (err)
		diagmem_free(driver, data->buf, POOL_TYPE_HDLC);

	data->buf = NULL;
	data->len = 0;
}

void diag_update_user_client_work_fn(struct work_struct *work)
{
	diag_update_userspace_clients(HDLC_SUPPORT_TYPE);
}

static void diag_update_md_client_work_fn(struct work_struct *work)
{
	diag_update_md_clients(HDLC_SUPPORT_TYPE);
}

void diag_drain_work_fn(struct work_struct *work)
{
	struct diag_md_session_t *session_info = NULL;
	uint8_t hdlc_disabled = 0;

	timer_in_progress = 0;
	mutex_lock(&apps_data_mutex);
	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_peripheral(APPS_DATA);
	if (session_info)
		hdlc_disabled = session_info->hdlc_disabled;
	else
		hdlc_disabled = driver->hdlc_disabled;
	mutex_unlock(&driver->md_session_lock);
	if (!hdlc_disabled)
		diag_drain_apps_data(&hdlc_data);
	else
		diag_drain_apps_data(&non_hdlc_data);
	mutex_unlock(&apps_data_mutex);
}

void check_drain_timer(void)
{
	int ret = 0;

	if (!timer_in_progress) {
		timer_in_progress = 1;
		ret = mod_timer(&drain_timer, jiffies + msecs_to_jiffies(200));
	}
}

void diag_add_client(int i, struct file *file)
{
	struct diagchar_priv *diagpriv_data;

	driver->client_map[i].pid = current->tgid;
	diagpriv_data = kmalloc(sizeof(struct diagchar_priv),
							GFP_KERNEL);
	if (diagpriv_data)
		diagpriv_data->pid = current->tgid;
	file->private_data = diagpriv_data;
	strlcpy(driver->client_map[i].name, current->comm, 20);
	driver->client_map[i].name[19] = '\0';
}

static void diag_mempool_init(void)
{
	uint32_t itemsize = DIAG_MAX_REQ_SIZE;
	uint32_t itemsize_hdlc = DIAG_MAX_HDLC_BUF_SIZE + APF_DIAG_PADDING;
	uint32_t itemsize_dci = IN_BUF_SIZE;
	uint32_t itemsize_user = DCI_REQ_BUF_SIZE;

	itemsize += ((DCI_HDR_SIZE > CALLBACK_HDR_SIZE) ? DCI_HDR_SIZE :
		     CALLBACK_HDR_SIZE);
	diagmem_setsize(POOL_TYPE_COPY, itemsize, poolsize);
	diagmem_setsize(POOL_TYPE_HDLC, itemsize_hdlc, poolsize_hdlc);
	diagmem_setsize(POOL_TYPE_DCI, itemsize_dci, poolsize_dci);
	diagmem_setsize(POOL_TYPE_USER, itemsize_user, poolsize_user);

	diagmem_init(driver, POOL_TYPE_COPY);
	diagmem_init(driver, POOL_TYPE_HDLC);
	diagmem_init(driver, POOL_TYPE_USER);
	diagmem_init(driver, POOL_TYPE_DCI);
}

static void diag_mempool_exit(void)
{
	diagmem_exit(driver, POOL_TYPE_COPY);
	diagmem_exit(driver, POOL_TYPE_HDLC);
	diagmem_exit(driver, POOL_TYPE_USER);
	diagmem_exit(driver, POOL_TYPE_DCI);
}

static int diagchar_open(struct inode *inode, struct file *file)
{
	int i = 0;
	void *temp;

	if (driver) {
		mutex_lock(&driver->diagchar_mutex);

		for (i = 0; i < driver->num_clients; i++)
			if (driver->client_map[i].pid == 0)
				break;

		if (i < driver->num_clients) {
			diag_add_client(i, file);
		} else {
			if (i < threshold_client_limit) {
				driver->num_clients++;
				temp = krealloc(driver->client_map
					, (driver->num_clients) * sizeof(struct
						 diag_client_map), GFP_KERNEL);
				if (!temp)
					goto fail;
				else
					driver->client_map = temp;
				temp = krealloc(driver->data_ready
					, (driver->num_clients) * sizeof(int),
							GFP_KERNEL);
				if (!temp)
					goto fail;
				else
					driver->data_ready = temp;
				diag_add_client(i, file);
			} else {
				mutex_unlock(&driver->diagchar_mutex);
				pr_err_ratelimited("diag: Max client limit for DIAG reached\n");
				pr_err_ratelimited("diag: Cannot open handle %s"
					   " %d", current->comm, current->tgid);
				for (i = 0; i < driver->num_clients; i++)
					pr_debug("%d) %s PID=%d", i, driver->
						client_map[i].name,
						driver->client_map[i].pid);
				return -ENOMEM;
			}
		}
		driver->data_ready[i] = 0x0;
		driver->data_ready[i] |= MSG_MASKS_TYPE;
		driver->data_ready[i] |= EVENT_MASKS_TYPE;
		driver->data_ready[i] |= LOG_MASKS_TYPE;
		driver->data_ready[i] |= DCI_LOG_MASKS_TYPE;
		driver->data_ready[i] |= DCI_EVENT_MASKS_TYPE;

		if (driver->ref_count == 0)
			diag_mempool_init();
		driver->ref_count++;
		mutex_unlock(&driver->diagchar_mutex);
		return 0;
	}
	return -ENOMEM;

fail:
	driver->num_clients--;
	mutex_unlock(&driver->diagchar_mutex);
	pr_err_ratelimited("diag: Insufficient memory for new client");
	return -ENOMEM;
}

static uint32_t diag_translate_kernel_to_user_mask(uint32_t peripheral_mask)
{
	uint32_t ret = 0;

	if (peripheral_mask & MD_PERIPHERAL_MASK(APPS_DATA))
		ret |= DIAG_CON_APSS;
	if (peripheral_mask & MD_PERIPHERAL_MASK(PERIPHERAL_MODEM))
		ret |= DIAG_CON_MPSS;
	if (peripheral_mask & MD_PERIPHERAL_MASK(PERIPHERAL_LPASS))
		ret |= DIAG_CON_LPASS;
	if (peripheral_mask & MD_PERIPHERAL_MASK(PERIPHERAL_WCNSS))
		ret |= DIAG_CON_WCNSS;
	if (peripheral_mask & MD_PERIPHERAL_MASK(PERIPHERAL_SENSORS))
		ret |= DIAG_CON_SENSORS;

	return ret;
}
int diag_mask_param(void)
{
	return diag_mask_clear_param;
}

void diag_clear_masks(int pid)
{
	int ret;
	char cmd_disable_log_mask[] = { 0x73, 0, 0, 0, 0, 0, 0, 0};
	char cmd_disable_msg_mask[] = { 0x7D, 0x05, 0, 0, 0, 0, 0, 0};
	char cmd_disable_event_mask[] = { 0x60, 0};

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
	"diag: %s: masks clear request upon %s\n", __func__,
	((pid) ? "ODL exit" : "USB Disconnection"));

	ret = diag_process_apps_masks(cmd_disable_log_mask,
			sizeof(cmd_disable_log_mask), pid);
	ret = diag_process_apps_masks(cmd_disable_msg_mask,
			sizeof(cmd_disable_msg_mask), pid);
	ret = diag_process_apps_masks(cmd_disable_event_mask,
			sizeof(cmd_disable_event_mask), pid);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
	"diag:%s: masks cleared successfully\n", __func__);
}

static void diag_close_logging_process(const int pid)
{
	int i;
	int session_peripheral_mask;
	struct diag_md_session_t *session_info = NULL;
	struct diag_logging_mode_param_t params;

	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_pid(pid);
	if (!session_info) {
		mutex_unlock(&driver->md_session_lock);
		return;
	}
	session_peripheral_mask = session_info->peripheral_mask;
	mutex_unlock(&driver->md_session_lock);

	if (diag_mask_clear_param)
		diag_clear_masks(pid);

	mutex_lock(&driver->diag_maskclear_mutex);
	driver->mask_clear = 1;
	mutex_unlock(&driver->diag_maskclear_mutex);

	mutex_lock(&driver->diagchar_mutex);
	session_peripheral_mask = session_info->peripheral_mask;
	for (i = 0; i < NUM_MD_SESSIONS; i++)
		if (MD_PERIPHERAL_MASK(i) & session_peripheral_mask)
			diag_mux_close_peripheral(DIAG_LOCAL_PROC, i);

	params.req_mode = USB_MODE;
	params.mode_param = 0;
	params.peripheral_mask =
		diag_translate_kernel_to_user_mask(session_peripheral_mask);

	mutex_lock(&driver->md_session_lock);
	diag_md_session_close(pid);
	mutex_unlock(&driver->md_session_lock);
	diag_switch_logging(&params);
	mutex_unlock(&driver->diagchar_mutex);
}

static int diag_remove_client_entry(struct file *file)
{
	int i = -1;
	struct diagchar_priv *diagpriv_data = NULL;
	struct diag_dci_client_tbl *dci_entry = NULL;

	if (!driver)
		return -ENOMEM;

	mutex_lock(&driver->diag_file_mutex);
	if (!file) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE, "Invalid file pointer\n");
		mutex_unlock(&driver->diag_file_mutex);
		return -ENOENT;
	}
	if (!(file->private_data)) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE, "Invalid private data\n");
		mutex_unlock(&driver->diag_file_mutex);
		return -EINVAL;
	}

	diagpriv_data = file->private_data;

	/*
	 * clean up any DCI registrations, if this is a DCI client
	 * This will specially help in case of ungraceful exit of any DCI client
	 * This call will remove any pending registrations of such client
	 */
	mutex_lock(&driver->dci_mutex);
	dci_entry = dci_lookup_client_entry_pid(current->tgid);
	if (dci_entry)
		diag_dci_deinit_client(dci_entry);
	mutex_unlock(&driver->dci_mutex);

	diag_close_logging_process(current->tgid);

	/* Delete the pkt response table entry for the exiting process */
	diag_cmd_remove_reg_by_pid(current->tgid);

	mutex_lock(&driver->diagchar_mutex);
	driver->ref_count--;
	if (driver->ref_count == 0)
		diag_mempool_exit();

	for (i = 0; i < driver->num_clients; i++) {
		if (NULL != diagpriv_data && diagpriv_data->pid ==
						driver->client_map[i].pid) {
			driver->client_map[i].pid = 0;
			kfree(diagpriv_data);
			diagpriv_data = NULL;
			file->private_data = 0;
			break;
		}
	}
	mutex_unlock(&driver->diagchar_mutex);
	mutex_unlock(&driver->diag_file_mutex);
	return 0;
}
static int diagchar_close(struct inode *inode, struct file *file)
{
	int ret;
	DIAG_LOG(DIAG_DEBUG_USERSPACE, "diag: process exit %s\n",
		current->comm);
	ret = diag_remove_client_entry(file);
	mutex_lock(&driver->diag_maskclear_mutex);
	driver->mask_clear = 0;
	mutex_unlock(&driver->diag_maskclear_mutex);
	return ret;
}

void diag_record_stats(int type, int flag)
{
	struct diag_pkt_stats_t *pkt_stats = NULL;

	switch (type) {
	case DATA_TYPE_EVENT:
		pkt_stats = &driver->event_stats;
		break;
	case DATA_TYPE_F3:
		pkt_stats = &driver->msg_stats;
		break;
	case DATA_TYPE_LOG:
		pkt_stats = &driver->log_stats;
		break;
	case DATA_TYPE_RESPONSE:
		if (flag != PKT_DROP)
			return;
		pr_err_ratelimited("diag: In %s, dropping response. This shouldn't happen\n",
				   __func__);
		return;
	case DATA_TYPE_DELAYED_RESPONSE:
		/* No counters to increase for Delayed responses */
		return;
	default:
		pr_err_ratelimited("diag: In %s, invalid pkt_type: %d\n",
				   __func__, type);
		return;
	}

	switch (flag) {
	case PKT_ALLOC:
		atomic_add(1, (atomic_t *)&pkt_stats->alloc_count);
		break;
	case PKT_DROP:
		atomic_add(1, (atomic_t *)&pkt_stats->drop_count);
		break;
	case PKT_RESET:
		atomic_set((atomic_t *)&pkt_stats->alloc_count, 0);
		atomic_set((atomic_t *)&pkt_stats->drop_count, 0);
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid flag: %d\n",
				   __func__, flag);
		return;
	}
}

void diag_get_timestamp(char *time_str)
{
	struct timeval t;
	struct tm broken_tm;
	do_gettimeofday(&t);
	if (!time_str)
		return;
	time_to_tm(t.tv_sec, 0, &broken_tm);
	scnprintf(time_str, DIAG_TS_SIZE, "%d:%d:%d:%ld", broken_tm.tm_hour,
				broken_tm.tm_min, broken_tm.tm_sec, t.tv_usec);
}

int diag_get_remote(int remote_info)
{
	int val = (remote_info < 0) ? -remote_info : remote_info;
	int remote_val;

	switch (val) {
	case MDM:
	case MDM2:
	case QSC:
		remote_val = -remote_info;
		break;
	default:
		remote_val = 0;
		break;
	}

	return remote_val;
}

int diag_cmd_chk_polling(struct diag_cmd_reg_entry_t *entry)
{
	int polling = DIAG_CMD_NOT_POLLING;

	if (!entry)
		return -EIO;

	if (entry->cmd_code == DIAG_CMD_NO_SUBSYS) {
		if (entry->subsys_id == DIAG_CMD_NO_SUBSYS &&
		    entry->cmd_code_hi >= DIAG_CMD_STATUS &&
		    entry->cmd_code_lo <= DIAG_CMD_STATUS)
			polling = DIAG_CMD_POLLING;
		else if (entry->subsys_id == DIAG_SS_WCDMA &&
			 entry->cmd_code_hi >= DIAG_CMD_QUERY_CALL &&
			 entry->cmd_code_lo <= DIAG_CMD_QUERY_CALL)
			polling = DIAG_CMD_POLLING;
		else if (entry->subsys_id == DIAG_SS_GSM &&
			 entry->cmd_code_hi >= DIAG_CMD_QUERY_TMC &&
			 entry->cmd_code_lo <= DIAG_CMD_QUERY_TMC)
			polling = DIAG_CMD_POLLING;
		else if (entry->subsys_id == DIAG_SS_PARAMS &&
			 entry->cmd_code_hi >= DIAG_DIAG_POLL  &&
			 entry->cmd_code_lo <= DIAG_DIAG_POLL)
			polling = DIAG_CMD_POLLING;
		else if (entry->subsys_id == DIAG_SS_TDSCDMA &&
			 entry->cmd_code_hi >= DIAG_CMD_TDSCDMA_STATUS &&
			 entry->cmd_code_lo <= DIAG_CMD_TDSCDMA_STATUS)
			polling = DIAG_CMD_POLLING;
	}

	return polling;
}

static void diag_cmd_invalidate_polling(int change_flag)
{
	int polling = DIAG_CMD_NOT_POLLING;
	struct list_head *start;
	struct list_head *temp;
	struct diag_cmd_reg_t *item = NULL;

	if (change_flag == DIAG_CMD_ADD) {
		if (driver->polling_reg_flag)
			return;
	}

	driver->polling_reg_flag = 0;
	list_for_each_safe(start, temp, &driver->cmd_reg_list) {
		item = list_entry(start, struct diag_cmd_reg_t, link);
		if (&item->entry == NULL) {
			pr_err("diag: In %s, unable to search command\n",
			       __func__);
			return;
		}
		polling = diag_cmd_chk_polling(&item->entry);
		if (polling == DIAG_CMD_POLLING) {
			driver->polling_reg_flag = 1;
			break;
		}
	}
}

int diag_cmd_add_reg(struct diag_cmd_reg_entry_t *new_entry, uint8_t proc,
		     int pid)
{
	struct diag_cmd_reg_t *new_item = NULL;

	if (!new_entry) {
		pr_err("diag: In %s, invalid new entry\n", __func__);
		return -EINVAL;
	}

	if (proc > APPS_DATA) {
		pr_err("diag: In %s, invalid peripheral %d\n", __func__, proc);
		return -EINVAL;
	}

	if (proc != APPS_DATA)
		pid = INVALID_PID;

	new_item = kzalloc(sizeof(struct diag_cmd_reg_t), GFP_KERNEL);
	if (!new_item) {
		pr_err("diag: In %s, unable to create memory for new command registration\n",
		       __func__);
		return -ENOMEM;
	}
	kmemleak_not_leak(new_item);

	new_item->pid = pid;
	new_item->proc = proc;
	memcpy(&new_item->entry, new_entry,
	       sizeof(struct diag_cmd_reg_entry_t));
	INIT_LIST_HEAD(&new_item->link);

	mutex_lock(&driver->cmd_reg_mutex);
	list_add_tail(&new_item->link, &driver->cmd_reg_list);
	driver->cmd_reg_count++;
	diag_cmd_invalidate_polling(DIAG_CMD_ADD);
	mutex_unlock(&driver->cmd_reg_mutex);

	return 0;
}

struct diag_cmd_reg_entry_t *diag_cmd_search(
			struct diag_cmd_reg_entry_t *entry, int proc)
{
	struct list_head *start;
	struct list_head *temp;
	struct diag_cmd_reg_t *item = NULL;
	struct diag_cmd_reg_entry_t *temp_entry = NULL;

	if (!entry) {
		pr_err("diag: In %s, invalid entry\n", __func__);
		return NULL;
	}

	list_for_each_safe(start, temp, &driver->cmd_reg_list) {
		item = list_entry(start, struct diag_cmd_reg_t, link);
		if (&item->entry == NULL) {
			pr_err("diag: In %s, unable to search command\n",
			       __func__);
			return NULL;
		}
		temp_entry = &item->entry;
		if (temp_entry->cmd_code == entry->cmd_code &&
		    temp_entry->subsys_id == entry->subsys_id &&
		    temp_entry->cmd_code_hi >= entry->cmd_code_hi &&
		    temp_entry->cmd_code_lo <= entry->cmd_code_lo &&
		    (proc == item->proc || proc == ALL_PROC)) {
			return &item->entry;
		} else if (temp_entry->cmd_code == DIAG_CMD_NO_SUBSYS &&
			   entry->cmd_code == DIAG_CMD_DIAG_SUBSYS) {
			if (temp_entry->subsys_id == entry->subsys_id &&
			    temp_entry->cmd_code_hi >= entry->cmd_code_hi &&
			    temp_entry->cmd_code_lo <= entry->cmd_code_lo &&
			    (proc == item->proc || proc == ALL_PROC)) {
				return &item->entry;
			}
		} else if (temp_entry->cmd_code == DIAG_CMD_NO_SUBSYS &&
			   temp_entry->subsys_id == DIAG_CMD_NO_SUBSYS) {
			if ((temp_entry->cmd_code_hi >= entry->cmd_code) &&
			    (temp_entry->cmd_code_lo <= entry->cmd_code) &&
			    (proc == item->proc || proc == ALL_PROC)) {
				if (entry->cmd_code == MODE_CMD) {
					if (entry->subsys_id == RESET_ID &&
						item->proc != APPS_DATA) {
						continue;
					}
					if (entry->subsys_id != RESET_ID &&
						item->proc == APPS_DATA) {
						continue;
					}
				}
				return &item->entry;
			}
		}
	}

	return NULL;
}

void diag_cmd_remove_reg(struct diag_cmd_reg_entry_t *entry, uint8_t proc)
{
	struct diag_cmd_reg_t *item = NULL;
	struct diag_cmd_reg_entry_t *temp_entry;
	if (!entry) {
		pr_err("diag: In %s, invalid entry\n", __func__);
		return;
	}

	mutex_lock(&driver->cmd_reg_mutex);
	temp_entry = diag_cmd_search(entry, proc);
	if (temp_entry) {
		item = container_of(temp_entry, struct diag_cmd_reg_t, entry);
		if (!item) {
			mutex_unlock(&driver->cmd_reg_mutex);
			return;
		}
		list_del(&item->link);
		kfree(item);
		driver->cmd_reg_count--;
	}
	diag_cmd_invalidate_polling(DIAG_CMD_REMOVE);
	mutex_unlock(&driver->cmd_reg_mutex);
}

void diag_cmd_remove_reg_by_pid(int pid)
{
	struct list_head *start;
	struct list_head *temp;
	struct diag_cmd_reg_t *item = NULL;

	mutex_lock(&driver->cmd_reg_mutex);
	list_for_each_safe(start, temp, &driver->cmd_reg_list) {
		item = list_entry(start, struct diag_cmd_reg_t, link);
		if (&item->entry == NULL) {
			pr_err("diag: In %s, unable to search command\n",
			       __func__);
			mutex_unlock(&driver->cmd_reg_mutex);
			return;
		}
		if (item->pid == pid) {
			list_del(&item->link);
			kfree(item);
			driver->cmd_reg_count--;
		}
	}
	mutex_unlock(&driver->cmd_reg_mutex);
}

void diag_cmd_remove_reg_by_proc(int proc)
{
	struct list_head *start;
	struct list_head *temp;
	struct diag_cmd_reg_t *item = NULL;

	mutex_lock(&driver->cmd_reg_mutex);
	list_for_each_safe(start, temp, &driver->cmd_reg_list) {
		item = list_entry(start, struct diag_cmd_reg_t, link);
		if (&item->entry == NULL) {
			pr_err("diag: In %s, unable to search command\n",
			       __func__);
			mutex_unlock(&driver->cmd_reg_mutex);
			return;
		}
		if (item->proc == proc) {
			list_del(&item->link);
			kfree(item);
			driver->cmd_reg_count--;
		}
	}
	diag_cmd_invalidate_polling(DIAG_CMD_REMOVE);
	mutex_unlock(&driver->cmd_reg_mutex);
}

static int diag_copy_dci(char __user *buf, size_t count,
			struct diag_dci_client_tbl *entry, int *pret)
{
	int total_data_len = 0;
	int ret = 0;
	int exit_stat = 1;
	uint8_t drain_again = 0;
	struct diag_dci_buffer_t *buf_entry, *temp;

	if (!buf || !entry || !pret)
		return exit_stat;

	ret = *pret;

	ret += sizeof(int);
	if (ret >= count) {
		pr_err("diag: In %s, invalid value for ret: %d, count: %zu\n",
		       __func__, ret, count);
		return -EINVAL;
	}

	mutex_lock(&entry->write_buf_mutex);
	list_for_each_entry_safe(buf_entry, temp, &entry->list_write_buf,
								buf_track) {

		if ((ret + buf_entry->data_len) > count) {
			drain_again = 1;
			break;
		}

		list_del(&buf_entry->buf_track);
		mutex_lock(&buf_entry->data_mutex);
		if ((buf_entry->data_len > 0) &&
		    (buf_entry->in_busy) &&
		    (buf_entry->data)) {
			if (copy_to_user(buf+ret, (void *)buf_entry->data,
					 buf_entry->data_len))
				goto drop;
			ret += buf_entry->data_len;
			total_data_len += buf_entry->data_len;
			diag_ws_on_copy(DIAG_WS_DCI);
drop:
			buf_entry->in_busy = 0;
			buf_entry->data_len = 0;
			buf_entry->in_list = 0;
			if (buf_entry->buf_type == DCI_BUF_CMD) {
				mutex_unlock(&buf_entry->data_mutex);
				continue;
			} else if (buf_entry->buf_type == DCI_BUF_SECONDARY) {
				diagmem_free(driver, buf_entry->data,
					     POOL_TYPE_DCI);
				buf_entry->data = NULL;
				mutex_unlock(&buf_entry->data_mutex);
				kfree(buf_entry);
				continue;
			}

		}
		mutex_unlock(&buf_entry->data_mutex);
	}

	if (total_data_len > 0) {
		/* Copy the total data length */
		COPY_USER_SPACE_OR_EXIT(buf+8, total_data_len, 4);
		ret -= 4;
	} else {
		pr_debug("diag: In %s, Trying to copy ZERO bytes, total_data_len: %d\n",
			__func__, total_data_len);
	}

	exit_stat = 0;
exit:
	entry->in_service = 0;
	mutex_unlock(&entry->write_buf_mutex);
	*pret = ret;
	if (drain_again)
		dci_drain_data(0);

	return exit_stat;
}

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
static int diag_remote_init(void)
{
	diagmem_setsize(POOL_TYPE_MDM, itemsize_mdm, poolsize_mdm);
	diagmem_setsize(POOL_TYPE_MDM2, itemsize_mdm, poolsize_mdm);
	diagmem_setsize(POOL_TYPE_MDM_DCI, itemsize_mdm_dci, poolsize_mdm_dci);
	diagmem_setsize(POOL_TYPE_MDM2_DCI, itemsize_mdm_dci,
			poolsize_mdm_dci);
	diagmem_setsize(POOL_TYPE_MDM_MUX, itemsize_mdm_usb, poolsize_mdm_usb);
	diagmem_setsize(POOL_TYPE_MDM2_MUX, itemsize_mdm_usb, poolsize_mdm_usb);
	diagmem_setsize(POOL_TYPE_MDM_DCI_WRITE, itemsize_mdm_dci_write,
			poolsize_mdm_dci_write);
	diagmem_setsize(POOL_TYPE_MDM2_DCI_WRITE, itemsize_mdm_dci_write,
			poolsize_mdm_dci_write);
	diagmem_setsize(POOL_TYPE_QSC_MUX, itemsize_qsc_usb,
			poolsize_qsc_usb);
	driver->hdlc_encode_buf = kzalloc(DIAG_MAX_HDLC_BUF_SIZE, GFP_KERNEL);
	if (!driver->hdlc_encode_buf)
		return -ENOMEM;
	driver->hdlc_encode_buf_len = 0;
	return 0;
}

static void diag_remote_exit(void)
{
	kfree(driver->hdlc_encode_buf);
}

static int diag_send_raw_data_remote(int proc, void *buf, int len,
				    uint8_t hdlc_flag)
{
	int err = 0;
	int max_len = 0;
	uint8_t retry_count = 0;
	uint8_t max_retries = 3;
	uint16_t payload = 0;
	struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
	struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
	int bridge_index = proc - 1;
	struct diag_md_session_t *session_info = NULL;
	uint8_t hdlc_disabled = 0;

	if (!buf)
		return -EINVAL;

	if (len <= 0) {
		pr_err("diag: In %s, invalid len: %d", __func__, len);
		return -EBADMSG;
	}

	if (bridge_index < 0 || bridge_index > NUM_REMOTE_DEV) {
		pr_err("diag: In %s, invalid bridge index: %d\n", __func__,
			bridge_index);
		return -EINVAL;
	 }

	do {
		if (driver->hdlc_encode_buf_len == 0)
			break;
		usleep_range(10000, 10100);
		retry_count++;
	} while (retry_count < max_retries);

	if (driver->hdlc_encode_buf_len != 0)
		return -EAGAIN;
	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_peripheral(APPS_DATA);
	if (session_info)
		hdlc_disabled = session_info->hdlc_disabled;
	else
		hdlc_disabled = driver->hdlc_disabled;
	mutex_unlock(&driver->md_session_lock);
	if (hdlc_disabled) {
		if (len < 4) {
			pr_err("diag: In %s, invalid len: %d of non_hdlc pkt",
			__func__, len);
			return -EBADMSG;
		}
		payload = *(uint16_t *)(buf + 2);
		if (payload > DIAG_MAX_HDLC_BUF_SIZE) {
			pr_err("diag: Dropping packet, payload size is %d\n",
				payload);
			return -EBADMSG;
		}
		driver->hdlc_encode_buf_len = payload;
		/*
		 * Adding 5 bytes for start (1 byte), version (1 byte),
		 * payload (2 bytes) and end (1 byte)
		 */
		if (len == (payload + 5)) {
			/*
			 * Adding 4 bytes for start (1 byte), version (1 byte)
			 * and payload (2 bytes)
			 */
			memcpy(driver->hdlc_encode_buf, buf + 4, payload);
			goto send_data;
		} else {
			pr_err("diag: In %s, invalid len: %d of non_hdlc pkt",
			__func__, len);
			return -EBADMSG;
		}
	}

	if (hdlc_flag) {
		if (DIAG_MAX_HDLC_BUF_SIZE < len) {
			pr_err("diag: Dropping packet, HDLC encoded packet payload size crosses buffer limit. Current payload size %d\n",
			       len);
			return -EBADMSG;
		}
		driver->hdlc_encode_buf_len = len;
		memcpy(driver->hdlc_encode_buf, buf, len);
		goto send_data;
	}

	/*
	 * The worst case length will be twice as the incoming packet length.
	 * Add 3 bytes for CRC bytes (2 bytes) and delimiter (1 byte)
	 */
	max_len = (2 * len) + 3;
	if (DIAG_MAX_HDLC_BUF_SIZE < max_len) {
		pr_err("diag: Dropping packet, HDLC encoded packet payload size crosses buffer limit. Current payload size %d\n",
		       max_len);
		return -EBADMSG;
	}

	/* Perform HDLC encoding on incoming data */
	send.state = DIAG_STATE_START;
	send.pkt = (void *)(buf);
	send.last = (void *)(buf + len - 1);
	send.terminate = 1;

	enc.dest = driver->hdlc_encode_buf;
	enc.dest_last = (void *)(driver->hdlc_encode_buf + max_len - 1);
	diag_hdlc_encode(&send, &enc);
	driver->hdlc_encode_buf_len = (int)(enc.dest -
					(void *)driver->hdlc_encode_buf);

send_data:
	err = diagfwd_bridge_write(bridge_index, driver->hdlc_encode_buf,
				   driver->hdlc_encode_buf_len);
	if (err) {
		pr_err_ratelimited("diag: Error writing Callback packet to proc: %d, err: %d\n",
				   proc, err);
		driver->hdlc_encode_buf_len = 0;
	}

	return err;
}

static int diag_process_userspace_remote(int proc, void *buf, int len)
{
	int bridge_index = proc - 1;

	if (!buf || len < 0) {
		pr_err("diag: Invalid input in %s, buf: %pK, len: %d\n",
		       __func__, buf, len);
		return -EINVAL;
	}

	if (bridge_index < 0 || bridge_index > NUM_REMOTE_DEV) {
		pr_err("diag: In %s, invalid bridge index: %d\n", __func__,
		       bridge_index);
		return -EINVAL;
	}

	driver->user_space_data_busy = 1;
	return diagfwd_bridge_write(bridge_index, buf, len);
}
#else
static int diag_remote_init(void)
{
	return 0;
}

static void diag_remote_exit(void)
{
	return;
}

int diagfwd_bridge_init(void)
{
	return 0;
}

void diagfwd_bridge_exit(void)
{
	return;
}

uint16_t diag_get_remote_device_mask(void)
{
	return 0;
}

static int diag_send_raw_data_remote(int proc, void *buf, int len,
				    uint8_t hdlc_flag)
{
	return -EINVAL;
}

static int diag_process_userspace_remote(int proc, void *buf, int len)
{
	return 0;
}
#endif

static int mask_request_validate(unsigned char mask_buf[])
{
	uint8_t packet_id;
	uint8_t subsys_id;
	uint16_t ss_cmd;

	packet_id = mask_buf[0];

	if (packet_id == DIAG_CMD_DIAG_SUBSYS_DELAY) {
		subsys_id = mask_buf[1];
		ss_cmd = *(uint16_t *)(mask_buf + 2);
		switch (subsys_id) {
		case DIAG_SS_DIAG:
			if ((ss_cmd == DIAG_SS_FILE_READ_MODEM) ||
				(ss_cmd == DIAG_SS_FILE_READ_ADSP) ||
				(ss_cmd == DIAG_SS_FILE_READ_WCNSS) ||
				(ss_cmd == DIAG_SS_FILE_READ_SLPI) ||
				(ss_cmd == DIAG_SS_FILE_READ_APPS))
				return 1;
			break;
		default:
			return 0;
		}
	} else if (packet_id == 0x4B) {
		subsys_id = mask_buf[1];
		ss_cmd = *(uint16_t *)(mask_buf + 2);
		/* Packets with SSID which are allowed */
		switch (subsys_id) {
		case 0x04: /* DIAG_SUBSYS_WCDMA */
			if ((ss_cmd == 0) || (ss_cmd == 0xF))
				return 1;
			break;
		case 0x08: /* DIAG_SUBSYS_GSM */
			if ((ss_cmd == 0) || (ss_cmd == 0x1))
				return 1;
			break;
		case 0x09: /* DIAG_SUBSYS_UMTS */
		case 0x0F: /* DIAG_SUBSYS_CM */
			if (ss_cmd == 0)
				return 1;
			break;
		case 0x0C: /* DIAG_SUBSYS_OS */
			if ((ss_cmd == 2) || (ss_cmd == 0x100))
				return 1; /* MPU and APU */
			break;
		case 0x12: /* DIAG_SUBSYS_DIAG_SERV */
			if ((ss_cmd == 0) || (ss_cmd == 0x6) || (ss_cmd == 0x7))
				return 1;
			else if (ss_cmd == 0x218) /* HDLC Disabled Command*/
				return 0;
			else if (ss_cmd == DIAG_GET_TIME_API)
				return 1;
			else if (ss_cmd == DIAG_SET_TIME_API)
				return 1;
			else if (ss_cmd == DIAG_SWITCH_COMMAND)
				return 1;
			else if (ss_cmd == DIAG_BUFFERING_MODE)
				return 1;
			break;
		case 0x13: /* DIAG_SUBSYS_FS */
			if ((ss_cmd == 0) || (ss_cmd == 0x1))
				return 1;
			break;
		default:
			return 0;
			break;
		}
	} else {
		switch (packet_id) {
		case 0x00:    /* Version Number */
		case 0x0C:    /* CDMA status packet */
		case 0x1C:    /* Diag Version */
		case 0x1D:    /* Time Stamp */
		case 0x60:    /* Event Report Control */
		case 0x63:    /* Status snapshot */
		case 0x73:    /* Logging Configuration */
		case 0x7C:    /* Extended build ID */
		case 0x7D:    /* Extended Message configuration */
		case 0x81:    /* Event get mask */
		case 0x82:    /* Set the event mask */
			return 1;
			break;
		default:
			return 0;
			break;
		}
	}
	return 0;
}

static void diag_md_session_init(void)
{
	int i;

	mutex_init(&driver->md_session_lock);
	driver->md_session_mask = 0;
	driver->md_session_mode = DIAG_MD_NONE;
	for (i = 0; i < NUM_MD_SESSIONS; i++)
		driver->md_session_map[i] = NULL;
}

static void diag_md_session_exit(void)
{
	int i;
	struct diag_md_session_t *session_info = NULL;

	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (driver->md_session_map[i]) {
			session_info = driver->md_session_map[i];
			diag_log_mask_free(session_info->log_mask);
			kfree(session_info->log_mask);
			session_info->log_mask = NULL;
			diag_msg_mask_free(session_info->msg_mask);
			kfree(session_info->msg_mask);
			session_info->msg_mask = NULL;
			diag_event_mask_free(session_info->event_mask);
			kfree(session_info->event_mask);
			session_info->event_mask = NULL;
			kfree(session_info);
			session_info = NULL;
			driver->md_session_map[i] = NULL;
		}
	}
	mutex_destroy(&driver->md_session_lock);
	driver->md_session_mask = 0;
	driver->md_session_mode = DIAG_MD_NONE;
}

int diag_md_session_create(int mode, int peripheral_mask, int proc)
{
	int i;
	int err = 0;
	struct diag_md_session_t *new_session = NULL;

	/*
	 * If a session is running with a peripheral mask and a new session
	 * request comes in with same peripheral mask value then return
	 * invalid param
	 */
	if (driver->md_session_mode == DIAG_MD_PERIPHERAL &&
	    (driver->md_session_mask & peripheral_mask) != 0)
		return -EINVAL;

	mutex_lock(&driver->md_session_lock);
	new_session = kzalloc(sizeof(struct diag_md_session_t), GFP_KERNEL);
	if (!new_session) {
		mutex_unlock(&driver->md_session_lock);
		return -ENOMEM;
	}

	new_session->peripheral_mask = 0;
	new_session->pid = current->tgid;
	new_session->task = current;

	new_session->log_mask = kzalloc(sizeof(struct diag_mask_info),
					GFP_KERNEL);
	if (!new_session->log_mask) {
		err = -ENOMEM;
		goto fail_peripheral;
	}
	new_session->event_mask = kzalloc(sizeof(struct diag_mask_info),
					  GFP_KERNEL);
	if (!new_session->event_mask) {
		err = -ENOMEM;
		goto fail_peripheral;
	}
	new_session->msg_mask = kzalloc(sizeof(struct diag_mask_info),
					GFP_KERNEL);
	if (!new_session->msg_mask) {
		err = -ENOMEM;
		goto fail_peripheral;
	}

	err = diag_log_mask_copy(new_session->log_mask, &log_mask);
	if (err) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			 "return value of log copy. err %d\n", err);
		goto fail_peripheral;
	}
	err = diag_event_mask_copy(new_session->event_mask, &event_mask);
	if (err) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			 "return value of event copy. err %d\n", err);
		goto fail_peripheral;
	}
	err = diag_msg_mask_copy(new_session->msg_mask, &msg_mask);
	if (err) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			 "return value of msg copy. err %d\n", err);
		goto fail_peripheral;
	}
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if ((MD_PERIPHERAL_MASK(i) & peripheral_mask) == 0)
			continue;
		if (driver->md_session_map[i] != NULL) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
				 "another instance present for %d\n", i);
			err = -EEXIST;
			goto fail_peripheral;
		}
		new_session->peripheral_mask |= MD_PERIPHERAL_MASK(i);
		driver->md_session_map[i] = new_session;
		driver->md_session_mask |= MD_PERIPHERAL_MASK(i);
	}
	setup_timer(&new_session->hdlc_reset_timer,
		diag_md_hdlc_reset_timer_func,
		new_session->pid);

	driver->md_session_mode = DIAG_MD_PERIPHERAL;
	mutex_unlock(&driver->md_session_lock);
	DIAG_LOG(DIAG_DEBUG_USERSPACE,
		 "created session in peripheral mode\n");
	return 0;

fail_peripheral:
	diag_log_mask_free(new_session->log_mask);
	kfree(new_session->log_mask);
	new_session->log_mask = NULL;
	diag_event_mask_free(new_session->event_mask);
	kfree(new_session->event_mask);
	new_session->event_mask = NULL;
	diag_msg_mask_free(new_session->msg_mask);
	kfree(new_session->msg_mask);
	new_session->msg_mask = NULL;
	kfree(new_session);
	new_session = NULL;
	mutex_unlock(&driver->md_session_lock);
	return err;
}

static void diag_md_session_close(int pid)
{
	int i;
	uint8_t found = 0;
	struct diag_md_session_t *session_info = NULL;

	session_info = diag_md_session_get_pid(pid);
	if (!session_info)
		return;

	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (driver->md_session_map[i] != session_info)
			continue;
		driver->md_session_map[i] = NULL;
		driver->md_session_mask &= ~session_info->peripheral_mask;
	}
	diag_log_mask_free(session_info->log_mask);
	kfree(session_info->log_mask);
	session_info->log_mask = NULL;
	diag_msg_mask_free(session_info->msg_mask);
	kfree(session_info->msg_mask);
	session_info->msg_mask = NULL;
	diag_event_mask_free(session_info->event_mask);
	kfree(session_info->event_mask);
	session_info->event_mask = NULL;
	del_timer(&session_info->hdlc_reset_timer);

	for (i = 0; i < NUM_MD_SESSIONS && !found; i++) {
		if (driver->md_session_map[i] != NULL)
			found = 1;
	}

	driver->md_session_mode = (found) ? DIAG_MD_PERIPHERAL : DIAG_MD_NONE;
	kfree(session_info);
	session_info = NULL;
	DIAG_LOG(DIAG_DEBUG_USERSPACE, "cleared up session\n");
}

struct diag_md_session_t *diag_md_session_get_pid(int pid)
{
	int i;

	if (pid <= 0)
		return NULL;
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (driver->md_session_map[i] &&
		    driver->md_session_map[i]->pid == pid)
			return driver->md_session_map[i];
	}
	return NULL;
}

struct diag_md_session_t *diag_md_session_get_peripheral(uint8_t peripheral)
{
	if (peripheral >= NUM_MD_SESSIONS)
		return NULL;
	return driver->md_session_map[peripheral];
}

static int diag_md_peripheral_switch(int pid,
				int peripheral_mask, int req_mode) {
	int i, bit = 0;
	struct diag_md_session_t *session_info = NULL;

	session_info = diag_md_session_get_pid(pid);
	if (!session_info)
		return -EINVAL;
	if (req_mode != DIAG_USB_MODE || req_mode != DIAG_MEMORY_DEVICE_MODE)
		return -EINVAL;

	/*
	 * check that md_session_map for i == session_info,
	 * if not then race condition occurred and bail
	 */
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		bit = MD_PERIPHERAL_MASK(i) & peripheral_mask;
		if (!bit)
			continue;
		if (req_mode == DIAG_USB_MODE) {
			if (driver->md_session_map[i] != session_info)
				return -EINVAL;
			driver->md_session_map[i] = NULL;
			driver->md_session_mask &= ~bit;
			session_info->peripheral_mask &= ~bit;

		} else {
			if (driver->md_session_map[i] != NULL)
				return -EINVAL;
			driver->md_session_map[i] = session_info;
			driver->md_session_mask |= bit;
			session_info->peripheral_mask |= bit;

		}
	}

	driver->md_session_mode = DIAG_MD_PERIPHERAL;
	DIAG_LOG(DIAG_DEBUG_USERSPACE, "Changed Peripherals:0x%x to mode:%d\n",
		peripheral_mask, req_mode);
}

static int diag_md_session_check(int curr_mode, int req_mode,
				 const struct diag_logging_mode_param_t *param,
				 uint8_t *change_mode)
{
	int i, bit = 0, err = 0, peripheral_mask = 0;
	int change_mask = 0;
	struct diag_md_session_t *session_info = NULL;

	if (!param || !change_mode)
		return -EIO;

	*change_mode = 0;

	switch (curr_mode) {
	case DIAG_USB_MODE:
	case DIAG_MEMORY_DEVICE_MODE:
	case DIAG_MULTI_MODE:
		break;
	default:
		return -EINVAL;
	}

	if (req_mode != DIAG_USB_MODE && req_mode != DIAG_MEMORY_DEVICE_MODE)
		return -EINVAL;

	if (req_mode == DIAG_USB_MODE) {
		if (curr_mode == DIAG_USB_MODE)
			return 0;
		mutex_lock(&driver->md_session_lock);
		if (driver->md_session_mode == DIAG_MD_NONE
		    && driver->md_session_mask == 0 && driver->logging_mask) {
			*change_mode = 1;
			mutex_unlock(&driver->md_session_lock);
			return 0;
		}
		/*
		 * curr_mode is either DIAG_MULTI_MODE or DIAG_MD_MODE
		 * Check if requested peripherals are already in usb mode
		 */
		for (i = 0; i < NUM_MD_SESSIONS; i++) {
			bit = MD_PERIPHERAL_MASK(i) & param->peripheral_mask;
			if (!bit)
				continue;
			if (bit & driver->logging_mask)
				change_mask |= bit;
		}
		if (!change_mask) {
			mutex_unlock(&driver->md_session_lock);
			return 0;
		}

		/*
		 * Change is needed. Check if this md_session has set all the
		 * requested peripherals. If another md session set a requested
		 * peripheral then we cannot switch that peripheral to USB.
		 * If this session owns all the requested peripherals, then
		 * call function to switch the modes/masks for the md_session
		 */
		session_info = diag_md_session_get_pid(current->tgid);
		if (!session_info) {
			*change_mode = 1;
			mutex_unlock(&driver->md_session_lock);
			return 0;
		}
		peripheral_mask = session_info->peripheral_mask;
		if ((change_mask & peripheral_mask)
							!= change_mask) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
			    "Another MD Session owns a requested peripheral\n");
			mutex_unlock(&driver->md_session_lock);
			return -EINVAL;
		}
		*change_mode = 1;

		/* If all peripherals are being set to USB Mode, call close */
		if (~change_mask & peripheral_mask) {
			err = diag_md_peripheral_switch(current->tgid,
					change_mask, DIAG_USB_MODE);
		} else
			diag_md_session_close(current->tgid);
		mutex_unlock(&driver->md_session_lock);
		return err;

	} else if (req_mode == DIAG_MEMORY_DEVICE_MODE) {
		/*
		 * Get bit mask that represents what peripherals already have
		 * been set. Check that requested peripherals already set are
		 * owned by this md session
		 */
		mutex_lock(&driver->md_session_lock);
		change_mask = driver->md_session_mask & param->peripheral_mask;
		session_info = diag_md_session_get_pid(current->tgid);

		if (session_info) {
			if ((session_info->peripheral_mask & change_mask)
							!= change_mask) {
				DIAG_LOG(DIAG_DEBUG_USERSPACE,
				    "Another MD Session owns a requested peripheral\n");
				mutex_unlock(&driver->md_session_lock);
				return -EINVAL;
			}
			err = diag_md_peripheral_switch(current->tgid,
					change_mask, DIAG_USB_MODE);
			mutex_unlock(&driver->md_session_lock);
		} else {
			mutex_unlock(&driver->md_session_lock);
			if (change_mask) {
				DIAG_LOG(DIAG_DEBUG_USERSPACE,
				    "Another MD Session owns a requested peripheral\n");
				return -EINVAL;
			}
			err = diag_md_session_create(DIAG_MD_PERIPHERAL,
				param->peripheral_mask, DIAG_LOCAL_PROC);
		}
		*change_mode = 1;
		return err;
	}
	return -EINVAL;
}

static uint32_t diag_translate_mask(uint32_t peripheral_mask)
{
	uint32_t ret = 0;

	if (peripheral_mask & DIAG_CON_APSS)
		ret |= (1 << APPS_DATA);
	if (peripheral_mask & DIAG_CON_MPSS)
		ret |= (1 << PERIPHERAL_MODEM);
	if (peripheral_mask & DIAG_CON_LPASS)
		ret |= (1 << PERIPHERAL_LPASS);
	if (peripheral_mask & DIAG_CON_WCNSS)
		ret |= (1 << PERIPHERAL_WCNSS);
	if (peripheral_mask & DIAG_CON_SENSORS)
		ret |= (1 << PERIPHERAL_SENSORS);

	return ret;
}

static int diag_switch_logging(struct diag_logging_mode_param_t *param)
{
	int new_mode;
	int curr_mode;
	int err = 0;
	uint8_t do_switch = 1;
	uint32_t peripheral_mask = 0;

	if (!param)
		return -EINVAL;

	if (!param->peripheral_mask) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"asking for mode switch with no peripheral mask set\n");
		return -EINVAL;
	}

	peripheral_mask = diag_translate_mask(param->peripheral_mask);
	param->peripheral_mask = peripheral_mask;

	switch (param->req_mode) {
	case CALLBACK_MODE:
	case UART_MODE:
	case SOCKET_MODE:
	case MEMORY_DEVICE_MODE:
		new_mode = DIAG_MEMORY_DEVICE_MODE;
		break;
	case USB_MODE:
		new_mode = DIAG_USB_MODE;
		break;
	default:
		pr_err("diag: In %s, request to switch to invalid mode: %d\n",
		       __func__, param->req_mode);
		return -EINVAL;
	}

	curr_mode = driver->logging_mode;
	DIAG_LOG(DIAG_DEBUG_USERSPACE,
		"request to switch logging from %d mask:%0x to %d mask:%0x\n",
		curr_mode, driver->md_session_mask, new_mode, peripheral_mask);

	err = diag_md_session_check(curr_mode, new_mode, param, &do_switch);
	if (err) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			 "err from diag_md_session_check, err: %d\n", err);
		return err;
	}

	if (do_switch == 0) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			 "not switching modes c: %d n: %d\n",
			 curr_mode, new_mode);
		return 0;
	}

	diag_ws_reset(DIAG_WS_MUX);
	err = diag_mux_switch_logging(&new_mode, &peripheral_mask);
	if (err) {
		pr_err("diag: In %s, unable to switch mode from %d to %d, err: %d\n",
		       __func__, curr_mode, new_mode, err);
		driver->logging_mode = curr_mode;
		goto fail;
	}
	driver->logging_mode = new_mode;
	driver->logging_mask = peripheral_mask;
	DIAG_LOG(DIAG_DEBUG_USERSPACE,
		"Switch logging to %d mask:%0x\n", new_mode, peripheral_mask);

	/* Update to take peripheral_mask */
	if (new_mode != DIAG_MEMORY_DEVICE_MODE) {
		diag_update_real_time_vote(DIAG_PROC_MEMORY_DEVICE,
					   MODE_REALTIME, ALL_PROC);
	} else {
		diag_update_proc_vote(DIAG_PROC_MEMORY_DEVICE, VOTE_UP,
				      ALL_PROC);
	}

	if (!(new_mode == DIAG_MEMORY_DEVICE_MODE &&
	      curr_mode == DIAG_USB_MODE)) {
		queue_work(driver->diag_real_time_wq,
			   &driver->diag_real_time_work);
	}

	return 0;
fail:
	return err;
}

static int diag_ioctl_dci_reg(unsigned long ioarg)
{
	int result = -EINVAL;
	struct diag_dci_reg_tbl_t dci_reg_params;

	if (copy_from_user(&dci_reg_params, (void __user *)ioarg,
				sizeof(struct diag_dci_reg_tbl_t)))
		return -EFAULT;

	result = diag_dci_register_client(&dci_reg_params);

	return result;
}

static int diag_ioctl_dci_health_stats(unsigned long ioarg)
{
	int result = -EINVAL;
	struct diag_dci_health_stats_proc stats;

	if (copy_from_user(&stats, (void __user *)ioarg,
				sizeof(struct diag_dci_health_stats_proc)))
		return -EFAULT;

	result = diag_dci_copy_health_stats(&stats);
	if (result == DIAG_DCI_NO_ERROR) {
		if (copy_to_user((void __user *)ioarg, &stats,
			sizeof(struct diag_dci_health_stats_proc)))
			return -EFAULT;
	}

	return result;
}

static int diag_ioctl_dci_log_status(unsigned long ioarg)
{
	struct diag_log_event_stats le_stats;
	struct diag_dci_client_tbl *dci_client = NULL;

	if (copy_from_user(&le_stats, (void __user *)ioarg,
				sizeof(struct diag_log_event_stats)))
		return -EFAULT;

	dci_client = diag_dci_get_client_entry(le_stats.client_id);
	if (!dci_client)
		return DIAG_DCI_NOT_SUPPORTED;
	le_stats.is_set = diag_dci_query_log_mask(dci_client, le_stats.code);
	if (copy_to_user((void __user *)ioarg, &le_stats,
				sizeof(struct diag_log_event_stats)))
		return -EFAULT;

	return DIAG_DCI_NO_ERROR;
}

static int diag_ioctl_dci_event_status(unsigned long ioarg)
{
	struct diag_log_event_stats le_stats;
	struct diag_dci_client_tbl *dci_client = NULL;

	if (copy_from_user(&le_stats, (void __user *)ioarg,
				sizeof(struct diag_log_event_stats)))
		return -EFAULT;

	dci_client = diag_dci_get_client_entry(le_stats.client_id);
	if (!dci_client)
		return DIAG_DCI_NOT_SUPPORTED;

	le_stats.is_set = diag_dci_query_event_mask(dci_client, le_stats.code);
	if (copy_to_user((void __user *)ioarg, &le_stats,
				sizeof(struct diag_log_event_stats)))
		return -EFAULT;

	return DIAG_DCI_NO_ERROR;
}

static int diag_ioctl_lsm_deinit(void)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid == current->tgid)
			break;

	if (i == driver->num_clients) {
		mutex_unlock(&driver->diagchar_mutex);
		return -EINVAL;
	}

	driver->data_ready[i] |= DEINIT_TYPE;
	mutex_unlock(&driver->diagchar_mutex);
	wake_up_interruptible(&driver->wait_q);

	return 1;
}

static int diag_ioctl_vote_real_time(unsigned long ioarg)
{
	int real_time = 0;
	int temp_proc = ALL_PROC;
	struct real_time_vote_t vote;
	struct diag_dci_client_tbl *dci_client = NULL;

	if (copy_from_user(&vote, (void __user *)ioarg,
			sizeof(struct real_time_vote_t)))
		return -EFAULT;

	if (vote.proc > DIAG_PROC_MEMORY_DEVICE ||
		vote.real_time_vote > MODE_UNKNOWN ||
		vote.client_id < 0) {
		pr_err("diag: %s, invalid params, proc: %d, vote: %d, client_id: %d\n",
			__func__, vote.proc, vote.real_time_vote,
			vote.client_id);
		return -EINVAL;
	}

	driver->real_time_update_busy++;
	if (vote.proc == DIAG_PROC_DCI) {
		dci_client = diag_dci_get_client_entry(vote.client_id);
		if (!dci_client) {
			driver->real_time_update_busy--;
			return DIAG_DCI_NOT_SUPPORTED;
		}
		diag_dci_set_real_time(dci_client, vote.real_time_vote);
		real_time = diag_dci_get_cumulative_real_time(
					dci_client->client_info.token);
		diag_update_real_time_vote(vote.proc, real_time,
					dci_client->client_info.token);
	} else {
		real_time = vote.real_time_vote;
		temp_proc = vote.client_id;
		diag_update_real_time_vote(vote.proc, real_time,
					   temp_proc);
	}
	queue_work(driver->diag_real_time_wq, &driver->diag_real_time_work);
	return 0;
}

static int diag_ioctl_get_real_time(unsigned long ioarg)
{
	int i;
	int retry_count = 0;
	int timer = 0;
	struct real_time_query_t rt_query;

	if (copy_from_user(&rt_query, (void __user *)ioarg,
					sizeof(struct real_time_query_t)))
		return -EFAULT;
	while (retry_count < 3) {
		if (driver->real_time_update_busy > 0) {
			retry_count++;
			/*
			 * The value 10000 was chosen empirically as an
			 * optimum value in order to give the work in
			 * diag_real_time_wq to complete processing.
			 */
			for (timer = 0; timer < 5; timer++)
				usleep_range(10000, 10100);
		} else {
			break;
		}
	}

	if (driver->real_time_update_busy > 0)
		return -EAGAIN;

	if (rt_query.proc < 0 || rt_query.proc >= DIAG_NUM_PROC) {
		pr_err("diag: Invalid proc %d in %s\n", rt_query.proc,
		       __func__);
		return -EINVAL;
	}
	rt_query.real_time = driver->real_time_mode[rt_query.proc];
	/*
	 * For the local processor, if any of the peripherals is in buffering
	 * mode, overwrite the value of real time with UNKNOWN_MODE
	 */
	if (rt_query.proc == DIAG_LOCAL_PROC) {
		for (i = 0; i < NUM_PERIPHERALS; i++) {
			if (!driver->feature[i].peripheral_buffering)
				continue;
			switch (driver->buffering_mode[i].mode) {
			case DIAG_BUFFERING_MODE_CIRCULAR:
			case DIAG_BUFFERING_MODE_THRESHOLD:
				rt_query.real_time = MODE_UNKNOWN;
				break;
			}
		}
	}

	if (copy_to_user((void __user *)ioarg, &rt_query,
			 sizeof(struct real_time_query_t)))
		return -EFAULT;

	return 0;
}

static int diag_ioctl_set_buffering_mode(unsigned long ioarg)
{
	struct diag_buffering_mode_t params;

	if (copy_from_user(&params, (void __user *)ioarg, sizeof(params)))
		return -EFAULT;

	if (params.peripheral >= NUM_PERIPHERALS)
		return -EINVAL;

	mutex_lock(&driver->mode_lock);
	driver->buffering_flag[params.peripheral] = 1;
	mutex_unlock(&driver->mode_lock);

	return diag_send_peripheral_buffering_mode(&params);
}

static int diag_ioctl_peripheral_drain_immediate(unsigned long ioarg)
{
	uint8_t peripheral;

	if (copy_from_user(&peripheral, (void __user *)ioarg, sizeof(uint8_t)))
		return -EFAULT;

	if (peripheral >= NUM_PERIPHERALS) {
		pr_err("diag: In %s, invalid peripheral %d\n", __func__,
		       peripheral);
		return -EINVAL;
	}

	if (!driver->feature[peripheral].peripheral_buffering) {
		pr_err("diag: In %s, peripheral %d doesn't support buffering\n",
		       __func__, peripheral);
		return -EIO;
	}

	return diag_send_peripheral_drain_immediate(peripheral);
}

static int diag_ioctl_dci_support(unsigned long ioarg)
{
	struct diag_dci_peripherals_t dci_support;
	int result = -EINVAL;

	if (copy_from_user(&dci_support, (void __user *)ioarg,
				sizeof(struct diag_dci_peripherals_t)))
		return -EFAULT;

	result = diag_dci_get_support_list(&dci_support);
	if (result == DIAG_DCI_NO_ERROR)
		if (copy_to_user((void __user *)ioarg, &dci_support,
				sizeof(struct diag_dci_peripherals_t)))
			return -EFAULT;

	return result;
}

static int diag_ioctl_hdlc_toggle(unsigned long ioarg)
{
	uint8_t hdlc_support;
	struct diag_md_session_t *session_info = NULL;

	if (copy_from_user(&hdlc_support, (void __user *)ioarg,
				sizeof(uint8_t)))
		return -EFAULT;
	mutex_lock(&driver->hdlc_disable_mutex);
	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_pid(current->tgid);
	if (session_info)
		session_info->hdlc_disabled = hdlc_support;
	else
		driver->hdlc_disabled = hdlc_support;
	mutex_unlock(&driver->md_session_lock);
	mutex_unlock(&driver->hdlc_disable_mutex);
	diag_update_md_clients(HDLC_SUPPORT_TYPE);

	return 0;
}

static int diag_ioctl_register_callback(unsigned long ioarg)
{
	int err = 0;
	struct diag_callback_reg_t reg;

	if (copy_from_user(&reg, (void __user *)ioarg,
			   sizeof(struct diag_callback_reg_t))) {
		return -EFAULT;
	}

	if (reg.proc < 0 || reg.proc >= DIAG_NUM_PROC) {
		pr_err("diag: In %s, invalid proc %d for callback registration\n",
		       __func__, reg.proc);
		return -EINVAL;
	}

	if (driver->md_session_mode == DIAG_MD_PERIPHERAL)
		return -EIO;

	return err;
}

static int diag_cmd_register_tbl(struct diag_cmd_reg_tbl_t *reg_tbl)
{
	int i;
	int err = 0;
	uint32_t count = 0;
	struct diag_cmd_reg_entry_t *entries = NULL;
	const uint16_t entry_len = sizeof(struct diag_cmd_reg_entry_t);


	if (!reg_tbl) {
		pr_err("diag: In %s, invalid registration table\n", __func__);
		return -EINVAL;
	}

	count = reg_tbl->count;
	if ((UINT_MAX / entry_len) < count) {
		pr_warn("diag: In %s, possbile integer overflow.\n", __func__);
		return -EFAULT;
	}

	entries = kzalloc(count * entry_len, GFP_KERNEL);
	if (!entries) {
		pr_err("diag: In %s, unable to create memory for registration table entries\n",
		       __func__);
		return -ENOMEM;
	}

	err = copy_from_user(entries, reg_tbl->entries, count * entry_len);
	if (err) {
		pr_err("diag: In %s, error copying data from userspace, err: %d\n",
		       __func__, err);
		kfree(entries);
		return -EFAULT;
	}

	for (i = 0; i < count; i++) {
		err = diag_cmd_add_reg(&entries[i], APPS_DATA, current->tgid);
		if (err) {
			pr_err("diag: In %s, unable to register command, err: %d\n",
			       __func__, err);
			break;
		}
	}

	kfree(entries);
	return err;
}

static int diag_ioctl_cmd_reg(unsigned long ioarg)
{
	struct diag_cmd_reg_tbl_t reg_tbl;

	if (copy_from_user(&reg_tbl, (void __user *)ioarg,
			   sizeof(struct diag_cmd_reg_tbl_t))) {
		return -EFAULT;
	}

	return diag_cmd_register_tbl(&reg_tbl);
}

static int diag_ioctl_cmd_dereg(void)
{
	diag_cmd_remove_reg_by_pid(current->tgid);
	return 0;
}

#ifdef CONFIG_COMPAT
/*
 * @sync_obj_name: name of the synchronization object associated with this proc
 * @count: number of entries in the bind
 * @params: the actual packet registrations
 */
struct diag_cmd_reg_tbl_compat_t {
	char sync_obj_name[MAX_SYNC_OBJ_NAME_SIZE];
	uint32_t count;
	compat_uptr_t entries;
};

static int diag_ioctl_cmd_reg_compat(unsigned long ioarg)
{
	struct diag_cmd_reg_tbl_compat_t reg_tbl_compat;
	struct diag_cmd_reg_tbl_t reg_tbl;

	if (copy_from_user(&reg_tbl_compat, (void __user *)ioarg,
			   sizeof(struct diag_cmd_reg_tbl_compat_t))) {
		return -EFAULT;
	}

	strlcpy(reg_tbl.sync_obj_name, reg_tbl_compat.sync_obj_name,
		MAX_SYNC_OBJ_NAME_SIZE);
	reg_tbl.count = reg_tbl_compat.count;
	reg_tbl.entries = (struct diag_cmd_reg_entry_t *)
			  (uintptr_t)reg_tbl_compat.entries;

	return diag_cmd_register_tbl(&reg_tbl);
}

long diagchar_compat_ioctl(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL;
	int client_id = 0;
	uint16_t delayed_rsp_id = 0;
	uint16_t remote_dev;
	struct diag_dci_client_tbl *dci_client = NULL;
	struct diag_logging_mode_param_t mode_param;

	switch (iocmd) {
	case DIAG_IOCTL_COMMAND_REG:
		result = diag_ioctl_cmd_reg_compat(ioarg);
		break;
	case DIAG_IOCTL_COMMAND_DEREG:
		result = diag_ioctl_cmd_dereg();
		break;
	case DIAG_IOCTL_GET_DELAYED_RSP_ID:
		delayed_rsp_id = diag_get_next_delayed_rsp_id();
		if (copy_to_user((void __user *)ioarg, &delayed_rsp_id,
				 sizeof(uint16_t)))
			result = -EFAULT;
		else
			result = 0;
		break;
	case DIAG_IOCTL_DCI_REG:
		result = diag_ioctl_dci_reg(ioarg);
		break;
	case DIAG_IOCTL_DCI_DEINIT:
		mutex_lock(&driver->dci_mutex);
		if (copy_from_user((void *)&client_id, (void __user *)ioarg,
			sizeof(int))) {
			mutex_unlock(&driver->dci_mutex);
			return -EFAULT;
		}
		dci_client = diag_dci_get_client_entry(client_id);
		if (!dci_client) {
			mutex_unlock(&driver->dci_mutex);
			return DIAG_DCI_NOT_SUPPORTED;
		}
		result = diag_dci_deinit_client(dci_client);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_SUPPORT:
		result = diag_ioctl_dci_support(ioarg);
		break;
	case DIAG_IOCTL_DCI_HEALTH_STATS:
		mutex_lock(&driver->dci_mutex);
		result = diag_ioctl_dci_health_stats(ioarg);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_LOG_STATUS:
		mutex_lock(&driver->dci_mutex);
		result = diag_ioctl_dci_log_status(ioarg);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_EVENT_STATUS:
		mutex_lock(&driver->dci_mutex);
		result = diag_ioctl_dci_event_status(ioarg);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_CLEAR_LOGS:
		mutex_lock(&driver->dci_mutex);
		if (copy_from_user((void *)&client_id, (void __user *)ioarg,
			sizeof(int))) {
			mutex_unlock(&driver->dci_mutex);
			return -EFAULT;
		}
		result = diag_dci_clear_log_mask(client_id);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_CLEAR_EVENTS:
		mutex_lock(&driver->dci_mutex);
		if (copy_from_user(&client_id, (void __user *)ioarg,
			sizeof(int))) {
			mutex_unlock(&driver->dci_mutex);
			return -EFAULT;
		}
		result = diag_dci_clear_event_mask(client_id);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_LSM_DEINIT:
		result = diag_ioctl_lsm_deinit();
		break;
	case DIAG_IOCTL_SWITCH_LOGGING:
		if (copy_from_user((void *)&mode_param, (void __user *)ioarg,
				   sizeof(mode_param)))
			return -EFAULT;
		mutex_lock(&driver->diagchar_mutex);
		result = diag_switch_logging(&mode_param);
		mutex_unlock(&driver->diagchar_mutex);
		break;
	case DIAG_IOCTL_REMOTE_DEV:
		remote_dev = diag_get_remote_device_mask();
		if (copy_to_user((void __user *)ioarg, &remote_dev,
			sizeof(uint16_t)))
			result = -EFAULT;
		else
			result = 1;
		break;
	case DIAG_IOCTL_VOTE_REAL_TIME:
		mutex_lock(&driver->dci_mutex);
		result = diag_ioctl_vote_real_time(ioarg);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_GET_REAL_TIME:
		result = diag_ioctl_get_real_time(ioarg);
		break;
	case DIAG_IOCTL_PERIPHERAL_BUF_CONFIG:
		result = diag_ioctl_set_buffering_mode(ioarg);
		break;
	case DIAG_IOCTL_PERIPHERAL_BUF_DRAIN:
		result = diag_ioctl_peripheral_drain_immediate(ioarg);
		break;
	case DIAG_IOCTL_REGISTER_CALLBACK:
		result = diag_ioctl_register_callback(ioarg);
		break;
	case DIAG_IOCTL_HDLC_TOGGLE:
		result = diag_ioctl_hdlc_toggle(ioarg);
		break;
	}
	return result;
}
#endif

long diagchar_ioctl(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL;
	int client_id = 0;
	uint16_t delayed_rsp_id;
	uint16_t remote_dev;
	struct diag_dci_client_tbl *dci_client = NULL;
	struct diag_logging_mode_param_t mode_param;

	switch (iocmd) {
	case DIAG_IOCTL_COMMAND_REG:
		result = diag_ioctl_cmd_reg(ioarg);
		break;
	case DIAG_IOCTL_COMMAND_DEREG:
		result = diag_ioctl_cmd_dereg();
		break;
	case DIAG_IOCTL_GET_DELAYED_RSP_ID:
		delayed_rsp_id = diag_get_next_delayed_rsp_id();
		if (copy_to_user((void __user *)ioarg, &delayed_rsp_id,
				 sizeof(uint16_t)))
			result = -EFAULT;
		else
			result = 0;
		break;
	case DIAG_IOCTL_DCI_REG:
		result = diag_ioctl_dci_reg(ioarg);
		break;
	case DIAG_IOCTL_DCI_DEINIT:
		mutex_lock(&driver->dci_mutex);
		if (copy_from_user((void *)&client_id, (void __user *)ioarg,
			sizeof(int))) {
			mutex_unlock(&driver->dci_mutex);
			return -EFAULT;
		}
		dci_client = diag_dci_get_client_entry(client_id);
		if (!dci_client) {
			mutex_unlock(&driver->dci_mutex);
			return DIAG_DCI_NOT_SUPPORTED;
		}
		result = diag_dci_deinit_client(dci_client);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_SUPPORT:
		result = diag_ioctl_dci_support(ioarg);
		break;
	case DIAG_IOCTL_DCI_HEALTH_STATS:
		mutex_lock(&driver->dci_mutex);
		result = diag_ioctl_dci_health_stats(ioarg);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_LOG_STATUS:
		mutex_lock(&driver->dci_mutex);
		result = diag_ioctl_dci_log_status(ioarg);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_EVENT_STATUS:
		mutex_lock(&driver->dci_mutex);
		result = diag_ioctl_dci_event_status(ioarg);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_CLEAR_LOGS:
		mutex_lock(&driver->dci_mutex);
		if (copy_from_user((void *)&client_id, (void __user *)ioarg,
			sizeof(int))) {
			mutex_unlock(&driver->dci_mutex);
			return -EFAULT;
		}
		result = diag_dci_clear_log_mask(client_id);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_DCI_CLEAR_EVENTS:
		mutex_lock(&driver->dci_mutex);
		if (copy_from_user(&client_id, (void __user *)ioarg,
			sizeof(int))) {
			mutex_unlock(&driver->dci_mutex);
			return -EFAULT;
		}
		result = diag_dci_clear_event_mask(client_id);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_LSM_DEINIT:
		result = diag_ioctl_lsm_deinit();
		break;
	case DIAG_IOCTL_SWITCH_LOGGING:
		if (copy_from_user((void *)&mode_param, (void __user *)ioarg,
				   sizeof(mode_param)))
			return -EFAULT;
		mutex_lock(&driver->diagchar_mutex);
		result = diag_switch_logging(&mode_param);
		mutex_unlock(&driver->diagchar_mutex);
		break;
	case DIAG_IOCTL_REMOTE_DEV:
		remote_dev = diag_get_remote_device_mask();
		if (copy_to_user((void __user *)ioarg, &remote_dev,
			sizeof(uint16_t)))
			result = -EFAULT;
		else
			result = 1;
		break;
	case DIAG_IOCTL_VOTE_REAL_TIME:
		mutex_lock(&driver->dci_mutex);
		result = diag_ioctl_vote_real_time(ioarg);
		mutex_unlock(&driver->dci_mutex);
		break;
	case DIAG_IOCTL_GET_REAL_TIME:
		result = diag_ioctl_get_real_time(ioarg);
		break;
	case DIAG_IOCTL_PERIPHERAL_BUF_CONFIG:
		result = diag_ioctl_set_buffering_mode(ioarg);
		break;
	case DIAG_IOCTL_PERIPHERAL_BUF_DRAIN:
		result = diag_ioctl_peripheral_drain_immediate(ioarg);
		break;
	case DIAG_IOCTL_REGISTER_CALLBACK:
		result = diag_ioctl_register_callback(ioarg);
		break;
	case DIAG_IOCTL_HDLC_TOGGLE:
		result = diag_ioctl_hdlc_toggle(ioarg);
		break;
	}
	return result;
}

static int diag_process_apps_data_hdlc(unsigned char *buf, int len,
				       int pkt_type)
{
	int err = 0;
	int ret = PKT_DROP;
	struct diag_apps_data_t *data = &hdlc_data;
	struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
	struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
	/*
	 * The maximum encoded size of the buffer can be atmost twice the length
	 * of the packet. Add three bytes foe footer - 16 bit CRC (2 bytes) +
	 * delimiter (1 byte).
	 */
	const uint32_t max_encoded_size = ((2 * len) + 3);

	if (!buf || len <= 0) {
		pr_err("diag: In %s, invalid buf: %pK len: %d\n",
		       __func__, buf, len);
		return -EIO;
	}

	if (DIAG_MAX_HDLC_BUF_SIZE < max_encoded_size) {
		pr_err_ratelimited("diag: In %s, encoded data is larger %d than the buffer size %d\n",
		       __func__, max_encoded_size, DIAG_MAX_HDLC_BUF_SIZE);
		return -EBADMSG;
	}

	send.state = DIAG_STATE_START;
	send.pkt = buf;
	send.last = (void *)(buf + len - 1);
	send.terminate = 1;

	if (!data->buf)
		data->buf = diagmem_alloc(driver, DIAG_MAX_HDLC_BUF_SIZE +
					APF_DIAG_PADDING,
					  POOL_TYPE_HDLC);
	if (!data->buf) {
		ret = PKT_DROP;
		goto fail_ret;
	}

	if ((DIAG_MAX_HDLC_BUF_SIZE - data->len) <= max_encoded_size) {
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
		data->buf = NULL;
		data->len = 0;
		data->buf = diagmem_alloc(driver, DIAG_MAX_HDLC_BUF_SIZE +
					APF_DIAG_PADDING,
					  POOL_TYPE_HDLC);
		if (!data->buf) {
			ret = PKT_DROP;
			goto fail_ret;
		}
	}

	enc.dest = data->buf + data->len;
	enc.dest_last = (void *)(data->buf + data->len + max_encoded_size);
	diag_hdlc_encode(&send, &enc);

	/*
	 * This is to check if after HDLC encoding, we are still within
	 * the limits of aggregation buffer. If not, we write out the
	 * current buffer and start aggregation in a newly allocated
	 * buffer.
	 */
	if ((uintptr_t)enc.dest >= (uintptr_t)(data->buf +
					       DIAG_MAX_HDLC_BUF_SIZE)) {
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
		data->buf = NULL;
		data->len = 0;
		data->buf = diagmem_alloc(driver, DIAG_MAX_HDLC_BUF_SIZE +
					APF_DIAG_PADDING,
					 POOL_TYPE_HDLC);
		if (!data->buf) {
			ret = PKT_DROP;
			goto fail_ret;
		}

		enc.dest = data->buf + data->len;
		enc.dest_last = (void *)(data->buf + data->len +
					 max_encoded_size);
		diag_hdlc_encode(&send, &enc);
	}

	data->len = (((uintptr_t)enc.dest - (uintptr_t)data->buf) <
			DIAG_MAX_HDLC_BUF_SIZE) ?
			((uintptr_t)enc.dest - (uintptr_t)data->buf) :
			DIAG_MAX_HDLC_BUF_SIZE;

	if (pkt_type == DATA_TYPE_RESPONSE) {
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
		data->buf = NULL;
		data->len = 0;
	}

	return PKT_ALLOC;

fail_free_buf:
	diagmem_free(driver, data->buf, POOL_TYPE_HDLC);
	data->buf = NULL;
	data->len = 0;

fail_ret:
	return ret;
}

static int diag_process_apps_data_non_hdlc(unsigned char *buf, int len,
					   int pkt_type)
{
	int err = 0;
	int ret = PKT_DROP;
	struct diag_pkt_frame_t header;
	struct diag_apps_data_t *data = &non_hdlc_data;
	/*
	 * The maximum packet size, when the data is non hdlc encoded is equal
	 * to the size of the packet frame header and the length. Add 1 for the
	 * delimiter 0x7E at the end.
	 */
	const uint32_t max_pkt_size = sizeof(header) + len + 1;

	if (!buf || len <= 0) {
		pr_err("diag: In %s, invalid buf: %pK len: %d\n",
		       __func__, buf, len);
		return -EIO;
	}

	if (!data->buf) {
		data->buf = diagmem_alloc(driver, DIAG_MAX_HDLC_BUF_SIZE +
					APF_DIAG_PADDING,
					  POOL_TYPE_HDLC);
		if (!data->buf) {
			ret = PKT_DROP;
			goto fail_ret;
		}
	}

	if ((DIAG_MAX_HDLC_BUF_SIZE - data->len) <= max_pkt_size) {
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
		data->buf = NULL;
		data->len = 0;
		data->buf = diagmem_alloc(driver, DIAG_MAX_HDLC_BUF_SIZE +
					APF_DIAG_PADDING,
					  POOL_TYPE_HDLC);
		if (!data->buf) {
			ret = PKT_DROP;
			goto fail_ret;
		}
	}

	header.start = CONTROL_CHAR;
	header.version = 1;
	header.length = len;
	memcpy(data->buf + data->len, &header, sizeof(header));
	data->len += sizeof(header);
	memcpy(data->buf + data->len, buf, len);
	data->len += len;
	*(uint8_t *)(data->buf + data->len) = CONTROL_CHAR;
	data->len += sizeof(uint8_t);
	if (pkt_type == DATA_TYPE_RESPONSE) {
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
		data->buf = NULL;
		data->len = 0;
	}

	return PKT_ALLOC;

fail_free_buf:
	diagmem_free(driver, data->buf, POOL_TYPE_HDLC);
	data->buf = NULL;
	data->len = 0;

fail_ret:
	return ret;
}

static int diag_user_process_dci_data(const char __user *buf, int len)
{
	int err = 0;
	const int mempool = POOL_TYPE_USER;
	unsigned char *user_space_data = NULL;

	if (!buf || len <= 0 || len > diag_mempools[mempool].itemsize) {
		pr_err_ratelimited("diag: In %s, invalid buf %pK len: %d\n",
				   __func__, buf, len);
		return -EBADMSG;
	}

	user_space_data = diagmem_alloc(driver, len, mempool);
	if (!user_space_data)
		return -ENOMEM;

	err = copy_from_user(user_space_data, buf, len);
	if (err) {
		pr_err_ratelimited("diag: In %s, unable to copy data from userspace, err: %d\n",
				   __func__, err);
		err = DIAG_DCI_SEND_DATA_FAIL;
		goto fail;
	}

	err = diag_process_dci_transaction(user_space_data, len);
fail:
	diagmem_free(driver, user_space_data, mempool);
	user_space_data = NULL;
	return err;
}

static int diag_user_process_dci_apps_data(const char __user *buf, int len,
					   int pkt_type)
{
	int err = 0;
	const int mempool = POOL_TYPE_COPY;
	unsigned char *user_space_data = NULL;

	if (!buf || len <= 0 || len > diag_mempools[mempool].itemsize) {
		pr_err_ratelimited("diag: In %s, invalid buf %pK len: %d\n",
				   __func__, buf, len);
		return -EBADMSG;
	}

	pkt_type &= (DCI_PKT_TYPE | DATA_TYPE_DCI_LOG | DATA_TYPE_DCI_EVENT);
	if (!pkt_type) {
		pr_err_ratelimited("diag: In %s, invalid pkt_type: %d\n",
				   __func__, pkt_type);
		return -EBADMSG;
	}

	user_space_data = diagmem_alloc(driver, len, mempool);
	if (!user_space_data)
		return -ENOMEM;

	err = copy_from_user(user_space_data, buf, len);
	if (err) {
		pr_alert("diag: In %s, unable to copy data from userspace, err: %d\n",
			 __func__, err);
		goto fail;
	}

	diag_process_apps_dci_read_data(pkt_type, user_space_data, len);
fail:
	diagmem_free(driver, user_space_data, mempool);
	user_space_data = NULL;
	return err;
}

static int diag_user_process_raw_data(const char __user *buf, int len)
{
	int err = 0;
	int ret = 0;
	int token_offset = 0;
	int remote_proc = 0;
	const int mempool = POOL_TYPE_COPY;
	unsigned char *user_space_data = NULL;

	if (!buf || len <= 0 || len > CALLBACK_BUF_SIZE) {
		pr_err_ratelimited("diag: In %s, invalid buf %pK len: %d\n",
				   __func__, buf, len);
		return -EBADMSG;
	}

	user_space_data = diagmem_alloc(driver, len, mempool);
	if (!user_space_data)
		return -ENOMEM;

	err = copy_from_user(user_space_data, buf, len);
	if (err) {
		pr_err("diag: copy failed for user space data\n");
		goto fail;
	}

	/* Check for proc_type */
	remote_proc = diag_get_remote(*(int *)user_space_data);
	if (remote_proc) {
		token_offset = sizeof(int);
		if (len <= MIN_SIZ_ALLOW) {
			pr_err("diag: In %s, possible integer underflow, payload size: %d\n",
		       __func__, len);
			diagmem_free(driver, user_space_data, mempool);
			user_space_data = NULL;
			return -EBADMSG;
		}
		len -= sizeof(int);
	}
	if (driver->mask_check) {
		if (!mask_request_validate(user_space_data +
						token_offset)) {
			pr_alert("diag: mask request Invalid\n");
			diagmem_free(driver, user_space_data, mempool);
			user_space_data = NULL;
			return -EFAULT;
		}
	}
	if (remote_proc) {
		ret = diag_send_raw_data_remote(remote_proc,
				(void *)(user_space_data + token_offset),
				len, USER_SPACE_RAW_DATA);
		if (ret) {
			pr_err("diag: Error sending data to remote proc %d, err: %d\n",
				remote_proc, ret);
		}
	} else {
		wait_event_interruptible(driver->wait_q,
					 (driver->in_busy_pktdata == 0));
		ret = diag_process_apps_pkt(user_space_data, len,
			current->tgid);
		if (ret == 1)
			diag_send_error_rsp((void *)(user_space_data), len);
	}
fail:
	diagmem_free(driver, user_space_data, mempool);
	user_space_data = NULL;
	return ret;
}

static int diag_user_process_userspace_data(const char __user *buf, int len)
{
	int err = 0;
	int max_retries = 3;
	int retry_count = 0;
	int remote_proc = 0;
	int token_offset = 0;
	struct diag_md_session_t *session_info = NULL;
	uint8_t hdlc_disabled;

	if (!buf || len <= 0 || len > USER_SPACE_DATA) {
		pr_err_ratelimited("diag: In %s, invalid buf %pK len: %d\n",
				   __func__, buf, len);
		return -EBADMSG;
	}

	do {
		if (!driver->user_space_data_busy)
			break;
		retry_count++;
		usleep_range(10000, 10100);
	} while (retry_count < max_retries);

	if (driver->user_space_data_busy)
		return -EAGAIN;

	err = copy_from_user(driver->user_space_data_buf, buf, len);
	if (err) {
		pr_err("diag: In %s, failed to copy data from userspace, err: %d\n",
		       __func__, err);
		return -EIO;
	}

	/* Check for proc_type */
	remote_proc = diag_get_remote(*(int *)driver->user_space_data_buf);
	if (remote_proc) {
		if (len <= MIN_SIZ_ALLOW) {
			pr_err("diag: Integer underflow in %s, payload size: %d",
			       __func__, len);
			return -EBADMSG;
		}
		token_offset = sizeof(int);
		len -= sizeof(int);
	}

	/* Check masks for On-Device logging */
	if (driver->mask_check) {
		if (!mask_request_validate(driver->user_space_data_buf +
					   token_offset)) {
			pr_alert("diag: mask request Invalid\n");
			return -EFAULT;
		}
	}

	/* send masks to local processor now */
	if (!remote_proc) {
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_pid(current->tgid);
		if (!session_info) {
			pr_err("diag:In %s request came from invalid md session pid:%d",
				__func__, current->tgid);
			mutex_unlock(&driver->md_session_lock);
			return -EINVAL;
		}
		if (session_info)
			hdlc_disabled = session_info->hdlc_disabled;
		else
			hdlc_disabled = driver->hdlc_disabled;
		mutex_unlock(&driver->md_session_lock);
		if (!hdlc_disabled)
			diag_process_hdlc_pkt((void *)
				(driver->user_space_data_buf),
				len, current->tgid);
		else
			diag_process_non_hdlc_pkt((char *)
						(driver->user_space_data_buf),
						len, current->tgid);
		return 0;
	}

	err = diag_process_userspace_remote(remote_proc,
					    driver->user_space_data_buf +
					    token_offset, len);
	if (err) {
		driver->user_space_data_busy = 0;
		pr_err("diag: Error sending mask to remote proc %d, err: %d\n",
		       remote_proc, err);
	}

	return err;
}

static int diag_user_process_apps_data(const char __user *buf, int len,
				       int pkt_type)
{
	int ret = 0;
	int stm_size = 0;
	const int mempool = POOL_TYPE_COPY;
	unsigned char *user_space_data = NULL;
	struct diag_md_session_t *session_info = NULL;
	uint8_t hdlc_disabled;

	if (!buf || len <= 0 || len > DIAG_MAX_RSP_SIZE) {
		pr_err_ratelimited("diag: In %s, invalid buf %pK len: %d\n",
				   __func__, buf, len);
		return -EBADMSG;
	}

	switch (pkt_type) {
	case DATA_TYPE_EVENT:
	case DATA_TYPE_F3:
	case DATA_TYPE_LOG:
	case DATA_TYPE_RESPONSE:
	case DATA_TYPE_DELAYED_RESPONSE:
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid pkt_type: %d\n",
				   __func__, pkt_type);
		return -EBADMSG;
	}

	user_space_data = diagmem_alloc(driver, len, mempool);
	if (!user_space_data) {
		diag_record_stats(pkt_type, PKT_DROP);
		return -ENOMEM;
	}

	ret = copy_from_user(user_space_data, buf, len);
	if (ret) {
		pr_alert("diag: In %s, unable to copy data from userspace, err: %d\n",
			 __func__, ret);
		diagmem_free(driver, user_space_data, mempool);
		user_space_data = NULL;
		diag_record_stats(pkt_type, PKT_DROP);
		return -EBADMSG;
	}

	if (driver->stm_state[APPS_DATA] &&
	    (pkt_type >= DATA_TYPE_EVENT) && (pkt_type <= DATA_TYPE_LOG)) {
		stm_size = stm_log_inv_ts(OST_ENTITY_DIAG, 0, user_space_data,
					  len);
		if (stm_size == 0) {
			pr_debug("diag: In %s, stm_log_inv_ts returned size of 0\n",
				 __func__);
		}
		diagmem_free(driver, user_space_data, mempool);
		user_space_data = NULL;

		return 0;
	}

	mutex_lock(&apps_data_mutex);
	mutex_lock(&driver->hdlc_disable_mutex);
	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_peripheral(APPS_DATA);
	if (session_info)
		hdlc_disabled = session_info->hdlc_disabled;
	else
		hdlc_disabled = driver->hdlc_disabled;
	mutex_unlock(&driver->md_session_lock);
	if (hdlc_disabled)
		ret = diag_process_apps_data_non_hdlc(user_space_data, len,
						      pkt_type);
	else
		ret = diag_process_apps_data_hdlc(user_space_data, len,
						  pkt_type);
	mutex_unlock(&driver->hdlc_disable_mutex);
	mutex_unlock(&apps_data_mutex);

	diagmem_free(driver, user_space_data, mempool);
	user_space_data = NULL;

	check_drain_timer();

	if (ret == PKT_DROP)
		diag_record_stats(pkt_type, PKT_DROP);
	else if (ret == PKT_ALLOC)
		diag_record_stats(pkt_type, PKT_ALLOC);
	else
		return ret;

	return 0;
}

static int check_data_ready(int index)
{
	int data_type = 0;

	mutex_lock(&driver->diagchar_mutex);
	data_type = driver->data_ready[index];
	mutex_unlock(&driver->diagchar_mutex);
	return data_type;
}

static ssize_t diagchar_read(struct file *file, char __user *buf, size_t count,
			  loff_t *ppos)
{
	struct diag_dci_client_tbl *entry;
	struct list_head *start, *temp;
	int index = -1, i = 0, ret = 0;
	int data_type;
	int copy_dci_data = 0;
	int exit_stat = 0;
	int write_len = 0;
	struct diag_md_session_t *session_info = NULL;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid == current->tgid)
			index = i;
	mutex_unlock(&driver->diagchar_mutex);

	if (index == -1) {
		pr_err("diag: Client PID not found in table");
		return -EINVAL;
	}
	if (!buf) {
		pr_err("diag: bad address from user side\n");
		return -EFAULT;
	}
	wait_event_interruptible(driver->wait_q, (check_data_ready(index)) > 0);

	mutex_lock(&driver->diagchar_mutex);

	if ((driver->data_ready[index] & USER_SPACE_DATA_TYPE) &&
	    (driver->logging_mode == DIAG_MEMORY_DEVICE_MODE ||
	     driver->logging_mode == DIAG_MULTI_MODE)) {
		pr_debug("diag: process woken up\n");
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & USER_SPACE_DATA_TYPE;
		driver->data_ready[index] ^= USER_SPACE_DATA_TYPE;
		COPY_USER_SPACE_OR_EXIT(buf, data_type, sizeof(int));
		/* place holder for number of data field */
		ret += sizeof(int);
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_pid(current->tgid);
		exit_stat = diag_md_copy_to_user(buf, &ret, count,
						 session_info);
		mutex_unlock(&driver->md_session_lock);
		goto exit;
	} else if (driver->data_ready[index] & USER_SPACE_DATA_TYPE) {
		/* In case, the thread wakes up and the logging mode is
		not memory device any more, the condition needs to be cleared */
		driver->data_ready[index] ^= USER_SPACE_DATA_TYPE;
	}

	if (driver->data_ready[index] & HDLC_SUPPORT_TYPE) {
		data_type = driver->data_ready[index] & HDLC_SUPPORT_TYPE;
		driver->data_ready[index] ^= HDLC_SUPPORT_TYPE;
		COPY_USER_SPACE_OR_EXIT(buf, data_type, sizeof(int));
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_pid(current->tgid);
		if (session_info) {
			COPY_USER_SPACE_OR_ERR(buf+4,
					session_info->hdlc_disabled,
					sizeof(uint8_t));
			if (ret == -EFAULT) {
				mutex_unlock(&driver->md_session_lock);
				goto exit;
			}
		}
		mutex_unlock(&driver->md_session_lock);
		goto exit;
	}

	if (driver->data_ready[index] & DEINIT_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & DEINIT_TYPE;
		COPY_USER_SPACE_OR_EXIT(buf, data_type, 4);
		driver->data_ready[index] ^= DEINIT_TYPE;
		mutex_unlock(&driver->diagchar_mutex);
		diag_remove_client_entry(file);
		return ret;
	}

	if (driver->data_ready[index] & MSG_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & MSG_MASKS_TYPE;
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_peripheral(APPS_DATA);
		COPY_USER_SPACE_OR_ERR(buf, data_type, sizeof(int));
		if (ret == -EFAULT) {
			mutex_unlock(&driver->md_session_lock);
			goto exit;
		}
		write_len = diag_copy_to_user_msg_mask(buf + ret, count,
						       session_info);
		mutex_unlock(&driver->md_session_lock);
		if (write_len > 0)
			ret += write_len;
		driver->data_ready[index] ^= MSG_MASKS_TYPE;
		goto exit;
	}

	if (driver->data_ready[index] & EVENT_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & EVENT_MASKS_TYPE;
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_peripheral(APPS_DATA);
		COPY_USER_SPACE_OR_ERR(buf, data_type, 4);
		if (ret == -EFAULT) {
			mutex_unlock(&driver->md_session_lock);
			goto exit;
		}
		if (session_info && session_info->event_mask &&
		    session_info->event_mask->ptr) {
			COPY_USER_SPACE_OR_ERR(buf + sizeof(int),
					*(session_info->event_mask->ptr),
					session_info->event_mask->mask_len);
			if (ret == -EFAULT) {
				mutex_unlock(&driver->md_session_lock);
				goto exit;
			}
		} else {
			COPY_USER_SPACE_OR_ERR(buf + sizeof(int),
						*(event_mask.ptr),
						event_mask.mask_len);
			if (ret == -EFAULT) {
				mutex_unlock(&driver->md_session_lock);
				goto exit;
			}
		}
		mutex_unlock(&driver->md_session_lock);
		driver->data_ready[index] ^= EVENT_MASKS_TYPE;
		goto exit;
	}

	if (driver->data_ready[index] & LOG_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & LOG_MASKS_TYPE;
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_peripheral(APPS_DATA);
		COPY_USER_SPACE_OR_ERR(buf, data_type, sizeof(int));
		if (ret == -EFAULT) {
			mutex_unlock(&driver->md_session_lock);
			goto exit;
		}
		write_len = diag_copy_to_user_log_mask(buf + ret, count,
						       session_info);
		mutex_unlock(&driver->md_session_lock);
		if (write_len > 0)
			ret += write_len;
		driver->data_ready[index] ^= LOG_MASKS_TYPE;
		goto exit;
	}

	if (driver->data_ready[index] & PKT_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & PKT_TYPE;
		COPY_USER_SPACE_OR_EXIT(buf, data_type, sizeof(data_type));
		COPY_USER_SPACE_OR_EXIT(buf + sizeof(data_type),
					*(driver->apps_req_buf),
					driver->apps_req_buf_len);
		driver->data_ready[index] ^= PKT_TYPE;
		driver->in_busy_pktdata = 0;
		goto exit;
	}

	if (driver->data_ready[index] & DCI_PKT_TYPE) {
		/* Copy the type of data being passed */
		data_type = driver->data_ready[index] & DCI_PKT_TYPE;
		COPY_USER_SPACE_OR_EXIT(buf, data_type, 4);
		COPY_USER_SPACE_OR_EXIT(buf+4, *(driver->dci_pkt_buf),
					driver->dci_pkt_length);
		driver->data_ready[index] ^= DCI_PKT_TYPE;
		driver->in_busy_dcipktdata = 0;
		goto exit;
	}

	if (driver->data_ready[index] & DCI_EVENT_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & DCI_EVENT_MASKS_TYPE;
		COPY_USER_SPACE_OR_EXIT(buf, data_type, 4);
		COPY_USER_SPACE_OR_EXIT(buf+4, driver->num_dci_client, 4);
		COPY_USER_SPACE_OR_EXIT(buf + 8, (dci_ops_tbl[DCI_LOCAL_PROC].
				event_mask_composite), DCI_EVENT_MASK_SIZE);
		driver->data_ready[index] ^= DCI_EVENT_MASKS_TYPE;
		goto exit;
	}

	if (driver->data_ready[index] & DCI_LOG_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & DCI_LOG_MASKS_TYPE;
		COPY_USER_SPACE_OR_EXIT(buf, data_type, 4);
		COPY_USER_SPACE_OR_EXIT(buf+4, driver->num_dci_client, 4);
		COPY_USER_SPACE_OR_EXIT(buf+8, (dci_ops_tbl[DCI_LOCAL_PROC].
				log_mask_composite), DCI_LOG_MASK_SIZE);
		driver->data_ready[index] ^= DCI_LOG_MASKS_TYPE;
		goto exit;
	}

exit:
	if (driver->data_ready[index] & DCI_DATA_TYPE) {
		data_type = driver->data_ready[index] & DCI_DATA_TYPE;
		mutex_unlock(&driver->diagchar_mutex);
		/* Copy the type of data being passed */
		mutex_lock(&driver->dci_mutex);
		list_for_each_safe(start, temp, &driver->dci_client_list) {
			entry = list_entry(start, struct diag_dci_client_tbl,
									track);
			if (entry->client->tgid != current->tgid)
				continue;
			if (!entry->in_service)
				continue;
			if (copy_to_user(buf + ret, &data_type, sizeof(int))) {
				mutex_unlock(&driver->dci_mutex);
				goto end;
			}
			ret += sizeof(int);
			if (copy_to_user(buf + ret, &entry->client_info.token,
				sizeof(int))) {
				mutex_unlock(&driver->dci_mutex);
				goto end;
			}
			ret += sizeof(int);
			copy_dci_data = 1;
			exit_stat = diag_copy_dci(buf, count, entry, &ret);
			mutex_lock(&driver->diagchar_mutex);
			driver->data_ready[index] ^= DCI_DATA_TYPE;
			mutex_unlock(&driver->diagchar_mutex);
			if (exit_stat == 1) {
				mutex_unlock(&driver->dci_mutex);
				goto end;
			}
		}
		mutex_unlock(&driver->dci_mutex);
		goto end;
	}
	mutex_unlock(&driver->diagchar_mutex);
end:
	/*
	 * Flush any read that is currently pending on DCI data and
	 * command channnels. This will ensure that the next read is not
	 * missed.
	 */
	if (copy_dci_data) {
		diag_ws_on_copy_complete(DIAG_WS_DCI);
		flush_workqueue(driver->diag_dci_wq);
	}
	return ret;
}

static ssize_t diagchar_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	int err = 0;
	int pkt_type = 0;
	int payload_len = 0;
	const char __user *payload_buf = NULL;

	/*
	 * The data coming from the user sapce should at least have the
	 * packet type heeader.
	 */
	if (count < sizeof(int)) {
		pr_err("diag: In %s, client is sending short data, len: %d\n",
		       __func__, (int)count);
		return -EBADMSG;
	}

	err = copy_from_user((&pkt_type), buf, sizeof(int));
	if (err) {
		pr_err_ratelimited("diag: In %s, unable to copy pkt_type from userspace, err: %d\n",
				   __func__, err);
		return -EIO;
	}

	if (driver->logging_mode == DIAG_USB_MODE && !driver->usb_connected) {
		if (!((pkt_type == DCI_DATA_TYPE) ||
		    (pkt_type == DCI_PKT_TYPE) ||
		    (pkt_type & DATA_TYPE_DCI_LOG) ||
		    (pkt_type & DATA_TYPE_DCI_EVENT))) {
			pr_debug("diag: In %s, Dropping non DCI packet type\n",
				 __func__);
			return -EIO;
		}
	}

	payload_buf = buf + sizeof(int);
	payload_len = count - sizeof(int);

	if (pkt_type == DCI_PKT_TYPE)
		return diag_user_process_dci_apps_data(payload_buf,
						       payload_len,
						       pkt_type);
	else if (pkt_type == DCI_DATA_TYPE)
		return diag_user_process_dci_data(payload_buf, payload_len);
	else if (pkt_type == USER_SPACE_RAW_DATA_TYPE)
		return diag_user_process_raw_data(payload_buf,
							    payload_len);
	else if (pkt_type == USER_SPACE_DATA_TYPE)
		return diag_user_process_userspace_data(payload_buf,
							payload_len);
	if (pkt_type & (DATA_TYPE_DCI_LOG | DATA_TYPE_DCI_EVENT)) {
		err = diag_user_process_dci_apps_data(payload_buf, payload_len,
						      pkt_type);
		if (pkt_type & DATA_TYPE_DCI_LOG)
			pkt_type ^= DATA_TYPE_DCI_LOG;
		if (pkt_type & DATA_TYPE_DCI_EVENT)
			pkt_type ^= DATA_TYPE_DCI_EVENT;
		/*
		 * Check if the log or event is selected even on the regular
		 * stream. If USB is not connected and we are not in memory
		 * device mode, we should not process these logs/events.
		 */
		if (pkt_type && driver->logging_mode == DIAG_USB_MODE &&
		    !driver->usb_connected)
			return err;
	}

	switch (pkt_type) {
	case DATA_TYPE_EVENT:
	case DATA_TYPE_F3:
	case DATA_TYPE_LOG:
	case DATA_TYPE_DELAYED_RESPONSE:
	case DATA_TYPE_RESPONSE:
		return diag_user_process_apps_data(payload_buf, payload_len,
						   pkt_type);
	default:
		pr_err_ratelimited("diag: In %s, invalid pkt_type: %d\n",
				   __func__, pkt_type);
		return -EINVAL;
	}

	return err;
}

void diag_ws_init()
{
	driver->dci_ws.ref_count = 0;
	driver->dci_ws.copy_count = 0;
	spin_lock_init(&driver->dci_ws.lock);

	driver->md_ws.ref_count = 0;
	driver->md_ws.copy_count = 0;
	spin_lock_init(&driver->md_ws.lock);
}

static void diag_stats_init(void)
{
	if (!driver)
		return;

	driver->msg_stats.alloc_count = 0;
	driver->msg_stats.drop_count = 0;

	driver->log_stats.alloc_count = 0;
	driver->log_stats.drop_count = 0;

	driver->event_stats.alloc_count = 0;
	driver->event_stats.drop_count = 0;
}

void diag_ws_on_notify()
{
	/*
	 * Do not deal with reference count here as there can be spurious
	 * interrupts.
	 */
	pm_stay_awake(driver->diag_dev);
}

void diag_ws_on_read(int type, int pkt_len)
{
	unsigned long flags;
	struct diag_ws_ref_t *ws_ref = NULL;

	switch (type) {
	case DIAG_WS_DCI:
		ws_ref = &driver->dci_ws;
		break;
	case DIAG_WS_MUX:
		ws_ref = &driver->md_ws;
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type: %d\n",
				   __func__, type);
		return;
	}

	spin_lock_irqsave(&ws_ref->lock, flags);
	if (pkt_len > 0) {
		ws_ref->ref_count++;
	} else {
		if (ws_ref->ref_count < 1) {
			ws_ref->ref_count = 0;
			ws_ref->copy_count = 0;
		}
		diag_ws_release();
	}
	spin_unlock_irqrestore(&ws_ref->lock, flags);
}


void diag_ws_on_copy(int type)
{
	unsigned long flags;
	struct diag_ws_ref_t *ws_ref = NULL;

	switch (type) {
	case DIAG_WS_DCI:
		ws_ref = &driver->dci_ws;
		break;
	case DIAG_WS_MUX:
		ws_ref = &driver->md_ws;
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type: %d\n",
				   __func__, type);
		return;
	}

	spin_lock_irqsave(&ws_ref->lock, flags);
	ws_ref->copy_count++;
	spin_unlock_irqrestore(&ws_ref->lock, flags);
}

void diag_ws_on_copy_fail(int type)
{
	unsigned long flags;
	struct diag_ws_ref_t *ws_ref = NULL;

	switch (type) {
	case DIAG_WS_DCI:
		ws_ref = &driver->dci_ws;
		break;
	case DIAG_WS_MUX:
		ws_ref = &driver->md_ws;
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type: %d\n",
				   __func__, type);
		return;
	}

	spin_lock_irqsave(&ws_ref->lock, flags);
	ws_ref->ref_count--;
	spin_unlock_irqrestore(&ws_ref->lock, flags);

	diag_ws_release();
}

void diag_ws_on_copy_complete(int type)
{
	unsigned long flags;
	struct diag_ws_ref_t *ws_ref = NULL;

	switch (type) {
	case DIAG_WS_DCI:
		ws_ref = &driver->dci_ws;
		break;
	case DIAG_WS_MUX:
		ws_ref = &driver->md_ws;
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type: %d\n",
				   __func__, type);
		return;
	}

	spin_lock_irqsave(&ws_ref->lock, flags);
	ws_ref->ref_count -= ws_ref->copy_count;
		if (ws_ref->ref_count < 1)
			ws_ref->ref_count = 0;
		ws_ref->copy_count = 0;
	spin_unlock_irqrestore(&ws_ref->lock, flags);

	diag_ws_release();
}

void diag_ws_reset(int type)
{
	unsigned long flags;
	struct diag_ws_ref_t *ws_ref = NULL;

	switch (type) {
	case DIAG_WS_DCI:
		ws_ref = &driver->dci_ws;
		break;
	case DIAG_WS_MUX:
		ws_ref = &driver->md_ws;
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type: %d\n",
				   __func__, type);
		return;
	}

	spin_lock_irqsave(&ws_ref->lock, flags);
	ws_ref->ref_count = 0;
	ws_ref->copy_count = 0;
	spin_unlock_irqrestore(&ws_ref->lock, flags);

	diag_ws_release();
}

void diag_ws_release()
{
	if (driver->dci_ws.ref_count == 0 && driver->md_ws.ref_count == 0)
		pm_relax(driver->diag_dev);
}

#ifdef DIAG_DEBUG
static void diag_debug_init(void)
{
	diag_ipc_log = ipc_log_context_create(DIAG_IPC_LOG_PAGES, "diag", 0);
	if (!diag_ipc_log)
		pr_err("diag: Failed to create IPC logging context\n");
	/*
	 * Set the bit mask here as per diag_ipc_logging.h to enable debug logs
	 * to be logged to IPC
	 */
	diag_debug_mask = DIAG_DEBUG_PERIPHERALS | DIAG_DEBUG_DCI |
				DIAG_DEBUG_BRIDGE;
}
#else
static void diag_debug_init(void)
{

}
#endif

static int diag_real_time_info_init(void)
{
	int i;
	if (!driver)
		return -EIO;
	for (i = 0; i < DIAG_NUM_PROC; i++) {
		driver->real_time_mode[i] = 1;
		driver->proc_rt_vote_mask[i] |= DIAG_PROC_DCI;
		driver->proc_rt_vote_mask[i] |= DIAG_PROC_MEMORY_DEVICE;
	}
	driver->real_time_update_busy = 0;
	driver->proc_active_mask = 0;
	driver->diag_real_time_wq = create_singlethread_workqueue(
							"diag_real_time_wq");
	if (!driver->diag_real_time_wq)
		return -ENOMEM;
	INIT_WORK(&(driver->diag_real_time_work), diag_real_time_work_fn);
	mutex_init(&driver->real_time_mutex);
	return 0;
}

static const struct file_operations diagcharfops = {
	.owner = THIS_MODULE,
	.read = diagchar_read,
	.write = diagchar_write,
#ifdef CONFIG_COMPAT
	.compat_ioctl = diagchar_compat_ioctl,
#endif
	.unlocked_ioctl = diagchar_ioctl,
	.open = diagchar_open,
	.release = diagchar_close
};

static int diagchar_setup_cdev(dev_t devno)
{

	int err;

	cdev_init(driver->cdev, &diagcharfops);

	driver->cdev->owner = THIS_MODULE;
	driver->cdev->ops = &diagcharfops;

	err = cdev_add(driver->cdev, devno, 1);

	if (err) {
		printk(KERN_INFO "diagchar cdev registration failed !\n\n");
		return -1;
	}

	driver->diagchar_class = class_create(THIS_MODULE, "diag");

	if (IS_ERR(driver->diagchar_class)) {
		printk(KERN_ERR "Error creating diagchar class.\n");
		return -1;
	}

	driver->diag_dev = device_create(driver->diagchar_class, NULL, devno,
					 (void *)driver, "diag");

	if (!driver->diag_dev)
		return -EIO;

	driver->diag_dev->power.wakeup = wakeup_source_register("DIAG_WS");
	return 0;

}

static int diagchar_cleanup(void)
{
	if (driver) {
		if (driver->cdev) {
			/* TODO - Check if device exists before deleting */
			device_destroy(driver->diagchar_class,
				       MKDEV(driver->major,
					     driver->minor_start));
			cdev_del(driver->cdev);
		}
		if (!IS_ERR(driver->diagchar_class))
			class_destroy(driver->diagchar_class);
		kfree(driver);
	}
	return 0;
}

static int diag_mhi_probe(struct platform_device *pdev)
{
	int ret;

	if (!mhi_is_device_ready(&pdev->dev, "qcom,mhi"))
		return -EPROBE_DEFER;
	driver->pdev = pdev;
	ret = diag_remote_init();
	if (ret) {
		diag_remote_exit();
		return ret;
	}
	ret = diagfwd_bridge_init();
	if (ret) {
		diagfwd_bridge_exit();
		return ret;
	}
	pr_debug("diag: mhi device is ready\n");
	return 0;
}

static const struct of_device_id diag_mhi_table[] = {
	{.compatible = "qcom,diag-mhi"},
	{},
};

static struct platform_driver diag_mhi_driver = {
	.probe = diag_mhi_probe,
	.driver = {
		.name = "DIAG MHI Platform",
		.owner = THIS_MODULE,
		.of_match_table = diag_mhi_table,
	},
};

static int __init diagchar_init(void)
{
	dev_t dev;
	int error, ret, i;

	pr_debug("diagfwd initializing ..\n");
	ret = 0;
	driver = kzalloc(sizeof(struct diagchar_dev) + 5, GFP_KERNEL);
	if (!driver)
		return -ENOMEM;
	kmemleak_not_leak(driver);

	timer_in_progress = 0;
	driver->delayed_rsp_id = 0;
	driver->hdlc_disabled = 0;
	driver->dci_state = DIAG_DCI_NO_ERROR;
	setup_timer(&drain_timer, drain_timer_func, 1234);
	driver->supports_sockets = 1;
	driver->time_sync_enabled = 0;
	driver->uses_time_api = 0;
	driver->poolsize = poolsize;
	driver->poolsize_hdlc = poolsize_hdlc;
	driver->poolsize_dci = poolsize_dci;
	driver->poolsize_user = poolsize_user;
	/*
	 * POOL_TYPE_MUX_APPS is for the buffers in the Diag MUX layer.
	 * The number of buffers encompasses Diag data generated on
	 * the Apss processor + 1 for the responses generated exclusively on
	 * the Apps processor + data from data channels (4 channels per
	 * peripheral) + data from command channels (2)
	 */
	diagmem_setsize(POOL_TYPE_MUX_APPS, itemsize_usb_apps,
			poolsize_usb_apps + 1 + (NUM_PERIPHERALS * 6));
	driver->num_clients = max_clients;
	driver->logging_mode = DIAG_USB_MODE;
	driver->mask_check = 0;
	driver->in_busy_pktdata = 0;
	driver->in_busy_dcipktdata = 0;
	driver->rsp_buf_ctxt = SET_BUF_CTXT(APPS_DATA, TYPE_CMD, 1);
	hdlc_data.ctxt = SET_BUF_CTXT(APPS_DATA, TYPE_DATA, 1);
	hdlc_data.len = 0;
	non_hdlc_data.ctxt = SET_BUF_CTXT(APPS_DATA, TYPE_DATA, 1);
	non_hdlc_data.len = 0;
	mutex_init(&driver->hdlc_disable_mutex);
	mutex_init(&driver->diagchar_mutex);
	mutex_init(&driver->diag_maskclear_mutex);
	mutex_init(&driver->diag_notifier_mutex);
	mutex_init(&driver->diag_file_mutex);
	mutex_init(&driver->delayed_rsp_mutex);
	mutex_init(&apps_data_mutex);
	mutex_init(&driver->msg_mask_lock);
	mutex_init(&driver->hdlc_recovery_mutex);
	for (i = 0; i < NUM_PERIPHERALS; i++)
		mutex_init(&driver->diagfwd_channel_mutex[i]);
	init_waitqueue_head(&driver->wait_q);
	INIT_WORK(&(driver->diag_drain_work), diag_drain_work_fn);
	INIT_WORK(&(driver->update_user_clients),
			diag_update_user_client_work_fn);
	INIT_WORK(&(driver->update_md_clients),
			diag_update_md_client_work_fn);
	diag_ws_init();
	diag_stats_init();
	diag_debug_init();
	diag_md_session_init();

	driver->incoming_pkt.capacity = DIAG_MAX_REQ_SIZE;
	driver->incoming_pkt.data = kzalloc(DIAG_MAX_REQ_SIZE, GFP_KERNEL);
	if (!driver->incoming_pkt.data)
		goto fail;
	kmemleak_not_leak(driver->incoming_pkt.data);
	driver->incoming_pkt.processing = 0;
	driver->incoming_pkt.read_len = 0;
	driver->incoming_pkt.remaining = 0;
	driver->incoming_pkt.total_len = 0;

	ret = diag_real_time_info_init();
	if (ret)
		goto fail;
	ret = diag_debugfs_init();
	if (ret)
		goto fail;
	ret = diag_masks_init();
	if (ret)
		goto fail;
	ret = diag_mux_init();
	if (ret)
		goto fail;
	ret = diagfwd_init();
	if (ret)
		goto fail;
	ret = diagfwd_cntl_init();
	if (ret)
		goto fail;
	driver->dci_state = diag_dci_init();
	ret = diagfwd_peripheral_init();
	if (ret)
		goto fail;
	diagfwd_cntl_channel_init();
	if (driver->dci_state == DIAG_DCI_NO_ERROR)
		diag_dci_channel_init();
	pr_debug("diagchar initializing ..\n");
	driver->num = 1;
	driver->name = ((void *)driver) + sizeof(struct diagchar_dev);
	strlcpy(driver->name, "diag", 4);
	/* Get major number from kernel and initialize */
	error = alloc_chrdev_region(&dev, driver->minor_start,
				    driver->num, driver->name);
	if (!error) {
		driver->major = MAJOR(dev);
		driver->minor_start = MINOR(dev);
	} else {
		pr_err("diag: Major number not allocated\n");
		goto fail;
	}
	driver->cdev = cdev_alloc();
	error = diagchar_setup_cdev(dev);
	if (error)
		goto fail;

	pr_debug("diagchar initialized now");
	platform_driver_register(&diag_mhi_driver);
	return 0;

fail:
	pr_err("diagchar is not initialized, ret: %d\n", ret);
	diag_debugfs_cleanup();
	diagchar_cleanup();
	diag_mux_exit();
	diagfwd_peripheral_exit();
	diagfwd_bridge_exit();
	diagfwd_exit();
	diagfwd_cntl_exit();
	diag_dci_exit();
	diag_masks_exit();
	return -1;
}

static void diagchar_exit(void)
{
	printk(KERN_INFO "diagchar exiting ..\n");
	diag_mempool_exit();
	diag_mux_exit();
	diagfwd_peripheral_exit();
	diagfwd_exit();
	diagfwd_cntl_exit();
	diag_dci_exit();
	diag_masks_exit();
	diag_md_session_exit();
	diag_remote_exit();
	diag_debugfs_cleanup();
	diagchar_cleanup();
	printk(KERN_INFO "done diagchar exit\n");
}

module_init(diagchar_init);
module_exit(diagchar_exit);
