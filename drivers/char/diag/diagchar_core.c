// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2008-2020, The Linux Foundation. All rights reserved.
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
#include <linux/jiffies.h>
#include <linux/sched/task.h>
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
#include "diagfwd_mhi.h"

#include <linux/coresight-stm.h>
#include <linux/kernel.h>
#include <linux/kmemleak.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

MODULE_DESCRIPTION("Diag Char Driver");
MODULE_LICENSE("GPL v2");

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

/*
 * Used for HDLC encoding packets coming from the user
 * space.
 */
static unsigned int poolsize_hdlc = 10;

/*
 * This is used for incoming DCI requests from the user space clients.
 * Don't expose itemsize as it is internal.
 */
static unsigned int poolsize_user = 8;

/*
 * USB structures allocated for writing Diag data generated on the Apps to USB.
 * Don't expose itemsize as it is constant.
 */
static unsigned int itemsize_usb_apps = sizeof(struct diag_request);
static unsigned int poolsize_usb_apps = 10;

/* Used for DCI client buffers. Don't expose itemsize as it is constant. */
static unsigned int poolsize_dci = 10;

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
/* Used for reading data from the remote device. */
static unsigned int itemsize_mdm = DIAG_MDM_BUF_SIZE;
static unsigned int poolsize_mdm = 18;

/*
 * Used for reading DCI data from the remote device.
 * Don't expose poolsize for DCI data. There is only one read buffer
 */
static unsigned int itemsize_mdm_dci = DIAG_MDM_BUF_SIZE;
static unsigned int poolsize_mdm_dci = 1;

/*
 * Used for USB structues associated with a remote device.
 * Don't expose the itemsize since it is constant.
 */
static unsigned int itemsize_mdm_usb = sizeof(struct diag_request);
static unsigned int poolsize_mdm_usb = 18;

/*
 * Used for writing read DCI data to remote peripherals. Don't
 * expose poolsize for DCI data. There is only one read
 * buffer. Add 6 bytes for DCI header information: Start (1),
 * Version (1), Length (2), Tag (2)
 */
static unsigned int itemsize_mdm_dci_write = DIAG_MDM_DCI_BUF_SIZE;
static unsigned int poolsize_mdm_dci_write = 1;

/*
 * Used for USB structures associated with a remote SMUX
 * device Don't expose the itemsize since it is constant
 */
static unsigned int itemsize_qsc_usb = sizeof(struct diag_request);
static unsigned int poolsize_qsc_usb = 8;
#endif

/* This is the max number of user-space clients supported at initialization*/
static unsigned int max_clients = 15;

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

struct diag_apps_data_t hdlc_data;
struct diag_apps_data_t non_hdlc_data;
static struct mutex apps_data_mutex;

#define DIAGPKT_MAX_DELAYED_RSP 0xFFFF

#ifdef CONFIG_IPC_LOGGING
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

#define COPY_USER_SPACE_OR_ERR(buf, data, length)		\
do {								\
	if ((count < ret+length) || (copy_to_user(buf,		\
			(void *)&data, length))) {		\
		ret = -EFAULT;					\
		break;							\
	}							\
	ret += length;						\
} while (0)

static void drain_timer_func(struct timer_list *tlist)
{
	queue_work(driver->diag_wq, &(driver->diag_drain_work));
}

static void diag_drain_apps_data(struct diag_apps_data_t *data)
{
	int err = 0;
	unsigned long flags;

	if (!data || !data->buf)
		return;

	spin_lock_irqsave(&driver->diagmem_lock, flags);
	if (data->flushed) {
		spin_unlock_irqrestore(&driver->diagmem_lock, flags);
		return;
	}
	data->flushed = 1;
	spin_unlock_irqrestore(&driver->diagmem_lock, flags);
	err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
			     data->ctxt);

	if (err) {
		spin_lock_irqsave(&driver->diagmem_lock, flags);
		diagmem_free(driver, data->buf, POOL_TYPE_HDLC);
		data->buf = NULL;
		data->len = 0;
		data->allocated = 0;
		data->flushed = 0;
		spin_unlock_irqrestore(&driver->diagmem_lock, flags);
	}
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
	uint8_t hdlc_disabled = 0;

	timer_in_progress = 0;
	mutex_lock(&apps_data_mutex);
	mutex_lock(&driver->hdlc_disable_mutex);
	hdlc_disabled = driver->p_hdlc_disabled[APPS_DATA];
	mutex_unlock(&driver->hdlc_disable_mutex);
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

static int diag_add_client(int i, struct file *file)
{
	struct diagchar_priv *diagpriv_data = NULL;

	diagpriv_data = kmalloc(sizeof(struct diagchar_priv),
							GFP_KERNEL);
	if (!diagpriv_data)
		return -ENOMEM;
	driver->client_map[i].pid = current->tgid;
	diagpriv_data->pid = current->tgid;
	file->private_data = diagpriv_data;
	strlcpy(driver->client_map[i].name, current->comm, 20);
	driver->client_map[i].name[19] = '\0';
	return 0;
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

	spin_lock_init(&driver->diagmem_lock);
	init_waitqueue_head(&driver->hdlc_wait_q);
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
	int i = 0, ret = 0;
	void *temp;

	if (driver) {
		mutex_lock(&driver->diagchar_mutex);

		for (i = 0; i < driver->num_clients; i++) {
			if (driver->client_map[i].pid == current->tgid) {
				pr_err_ratelimited("diag: Client already present current->tgid: %d\n",
					current->tgid);
				mutex_unlock(&driver->diagchar_mutex);
				return -EEXIST;
			}
		}
		for (i = 0; i < driver->num_clients; i++)
			if (driver->client_map[i].pid == 0)
				break;

		if (i < driver->num_clients) {
			ret = diag_add_client(i, file);
			if (ret < 0) {
				mutex_unlock(&driver->diagchar_mutex);
				pr_err_ratelimited("diag: Insufficient memory for adding new client\n");
				return ret;
			}
		} else {
			if (i < THRESHOLD_CLIENT_LIMIT) {
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
				ret = diag_add_client(i, file);
				if (ret < 0)
					goto fail;
			} else {
				mutex_unlock(&driver->diagchar_mutex);
				pr_err_ratelimited("diag: Max client limit for DIAG reached\n");
				pr_err_ratelimited("diag: Cannot open handle %s %d\n",
						current->comm, current->tgid);
				for (i = 0; i < driver->num_clients; i++)
					pr_debug("%d) %s PID=%d\n", i,
						driver->client_map[i].name,
						driver->client_map[i].pid);
				return -ENOMEM;
			}
		}
		driver->data_ready[i] = 0x0;
		atomic_set(&driver->data_ready_notif[i], 0);
		driver->data_ready[i] |= MSG_MASKS_TYPE;
		atomic_inc(&driver->data_ready_notif[i]);
		driver->data_ready[i] |= EVENT_MASKS_TYPE;
		atomic_inc(&driver->data_ready_notif[i]);
		driver->data_ready[i] |= LOG_MASKS_TYPE;
		atomic_inc(&driver->data_ready_notif[i]);
		driver->data_ready[i] |= DCI_LOG_MASKS_TYPE;
		atomic_inc(&driver->data_ready_notif[i]);
		driver->data_ready[i] |= DCI_EVENT_MASKS_TYPE;
		atomic_inc(&driver->data_ready_notif[i]);

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
	pr_err_ratelimited("diag: Insufficient memory for new client\n");
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
	if (peripheral_mask & MD_PERIPHERAL_MASK(PERIPHERAL_WDSP))
		ret |= DIAG_CON_WDSP;
	if (peripheral_mask & MD_PERIPHERAL_MASK(PERIPHERAL_CDSP))
		ret |= DIAG_CON_CDSP;
	if (peripheral_mask & MD_PERIPHERAL_MASK(PERIPHERAL_NPU))
		ret |= DIAG_CON_NPU;
	if (peripheral_mask & MD_PERIPHERAL_MASK(UPD_WLAN))
		ret |= DIAG_CON_UPD_WLAN;
	if (peripheral_mask & MD_PERIPHERAL_MASK(UPD_AUDIO))
		ret |= DIAG_CON_UPD_AUDIO;
	if (peripheral_mask & MD_PERIPHERAL_MASK(UPD_SENSORS))
		ret |= DIAG_CON_UPD_SENSORS;
	if (peripheral_mask & MD_PERIPHERAL_MASK(UPD_CHARGER))
		ret |= DIAG_CON_UPD_CHARGER;
	return ret;
}

uint8_t diag_mask_to_pd_value(uint32_t peripheral_mask)
{
	uint8_t upd = 0;

	for (upd = UPD_WLAN; upd < NUM_MD_SESSIONS; upd++) {
		if (peripheral_mask & (1 << upd))
			return upd;
	}
	return 0;
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
	int i, j;
	int session_mask = 0;
	int device_mask = 0;
	uint32_t p_mask = 0;
	struct diag_md_session_t *session_info = NULL;
	struct diag_logging_mode_param_t params;

	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_pid(pid);
	if (!session_info) {
		mutex_unlock(&driver->md_session_lock);
		return;
	}
	for (i = 0; i < NUM_DIAG_MD_DEV; i++) {
		if (session_info->peripheral_mask[i]) {
			session_mask = session_info->peripheral_mask[i];
			device_mask = device_mask | (1 << i);
		}
	}
	mutex_unlock(&driver->md_session_lock);

	if (diag_mask_clear_param)
		diag_clear_masks(pid);

	mutex_lock(&driver->diagchar_mutex);
	if (session_mask)
		p_mask =
		diag_translate_kernel_to_user_mask(session_mask);

	for (i = 0; i < NUM_MD_SESSIONS; i++)
		if (MD_PERIPHERAL_MASK(i) & session_mask)
			diag_mux_close_peripheral(DIAG_LOCAL_PROC, i);

	params.req_mode = USB_MODE;
	params.mode_param = 0;
	params.pd_mask = 0;
	params.peripheral_mask = p_mask;
	params.device_mask = device_mask;

	if (driver->num_pd_session > 0) {
		for (i = UPD_WLAN; (i < NUM_MD_SESSIONS); i++) {
			if (session_mask & MD_PERIPHERAL_MASK(i)) {
				j = i - UPD_WLAN;
				driver->pd_session_clear[j] = 1;
				driver->pd_logging_mode[j] = 0;
				driver->num_pd_session -= 1;
				params.pd_mask = p_mask;
			}
		}
	}
	mutex_lock(&driver->hdlc_disable_mutex);
	mutex_lock(&driver->md_session_lock);
	diag_md_session_close(pid);
	mutex_unlock(&driver->md_session_lock);
	mutex_unlock(&driver->hdlc_disable_mutex);
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
	for (i = 0; i < driver->num_clients; i++)
		if (diagpriv_data && diagpriv_data->pid ==
			driver->client_map[i].pid)
			break;
	if (i == driver->num_clients) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"pid %d, not present in client map\n",
			diagpriv_data->pid);
		mutex_unlock(&driver->diag_file_mutex);
		return -EINVAL;
	}
	DIAG_LOG(DIAG_DEBUG_USERSPACE, "diag: %s process exit with pid = %d\n",
		driver->client_map[i].name, diagpriv_data->pid);
	/*
	 * clean up any DCI registrations, if this is a DCI client
	 * This will specially help in case of ungraceful exit of any DCI client
	 * This call will remove any pending registrations of such client
	 */
	mutex_lock(&driver->dci_mutex);
	do {
		dci_entry = dci_lookup_client_entry_pid(diagpriv_data->pid);
		if (dci_entry)
			diag_dci_deinit_client(dci_entry);
	} while (dci_entry);
	mutex_unlock(&driver->dci_mutex);

	diag_close_logging_process(diagpriv_data->pid);

	/* Delete the pkt response table entry for the exiting process */
	diag_cmd_remove_reg_by_pid(diagpriv_data->pid);

	mutex_lock(&driver->diagchar_mutex);
	driver->ref_count--;
	if (driver->ref_count == 0)
		diag_mempool_exit();

	driver->client_map[i].pid = 0;
	kfree(diagpriv_data);
	diagpriv_data = NULL;
	file->private_data = 0;

	mutex_unlock(&driver->diagchar_mutex);
	mutex_unlock(&driver->diag_file_mutex);
	return 0;
}
static int diagchar_close(struct inode *inode, struct file *file)
{
	int ret;

	ret = diag_remove_client_entry(file);

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
		if (!item) {
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
	if (!new_item)
		return -ENOMEM;
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
		if (!item) {
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
		if (!item) {
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
		if (!item) {
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
			} else {
				mutex_unlock(&buf_entry->data_mutex);
				continue;
			}

		}
		mutex_unlock(&buf_entry->data_mutex);
	}

	if (total_data_len > 0) {
		/* Copy the total data length */
		COPY_USER_SPACE_OR_ERR(buf+(*pret), total_data_len, 4);
		if (ret == -EFAULT)
			goto exit;
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
int diag_remote_init(void)
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
	diag_md_mdm_init();
	if (diag_dci_init_remote())
		return -ENOMEM;
	driver->hdlc_encode_buf = kzalloc(DIAG_MAX_HDLC_BUF_SIZE, GFP_KERNEL);
	if (!driver->hdlc_encode_buf)
		return -ENOMEM;
	driver->hdlc_encode_buf_len = 0;
	return 0;
}

void diag_remote_exit(void)
{
	kfree(driver->hdlc_encode_buf);
}

static int diag_send_raw_data_remote(int proc, void *buf, int len,
				    uint8_t hdlc_flag)
{
	int err = 0;
	int max_len = 0;
	uint8_t retry_count = 0;
	uint8_t max_retries = 50;
	uint16_t payload = 0;
	struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
	struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
	int bridge_index = proc - 1;
	uint8_t hdlc_disabled = 0;

	if (!buf)
		return -EINVAL;

	if (len <= 0) {
		pr_err("diag: In %s, invalid len: %d\n", __func__, len);
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
	mutex_lock(&driver->hdlc_disable_mutex);
	hdlc_disabled = driver->p_hdlc_disabled[APPS_DATA];
	mutex_unlock(&driver->hdlc_disable_mutex);
	if (hdlc_disabled) {
		if (len < 4) {
			pr_err("diag: In %s, invalid len: %d of non_hdlc pkt\n",
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
			pr_err("diag: In %s, invalid len: %d of non_hdlc pkt\n",
			__func__, len);
			return -EBADMSG;
		}
	}

	if (hdlc_flag) {
		if (len > DIAG_MAX_HDLC_BUF_SIZE) {
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
	if (max_len > DIAG_MAX_HDLC_BUF_SIZE) {
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
int diag_remote_init(void)
{
	return 0;
}

void diag_remote_exit(void)
{
}

int diagfwd_bridge_init(void)
{
	return 0;
}

void diagfwd_bridge_exit(void)
{
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

static int mask_request_validate(unsigned char mask_buf[], int len)
{
	uint8_t packet_id;
	uint8_t subsys_id;
	uint16_t ss_cmd;

	if (len <= 0)
		return 0;
	packet_id = mask_buf[0];

	if (packet_id == DIAG_CMD_DIAG_SUBSYS_DELAY) {
		if (len < 2*sizeof(uint8_t) + sizeof(uint16_t))
			return 0;
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
		if (len < 2*sizeof(uint8_t) + sizeof(uint16_t))
			return 0;
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
		default:
			return 0;
		}
	}
	return 0;
}

static void diag_md_session_init(void)
{
	int i, proc;

	mutex_init(&driver->md_session_lock);
	for (proc = 0; proc < NUM_DIAG_MD_DEV; proc++) {
		driver->md_session_mask[proc] = 0;
		driver->md_session_mode[proc] = DIAG_MD_NONE;
		for (i = 0; i < NUM_MD_SESSIONS; i++)
			driver->md_session_map[proc][i] = NULL;
	}
}

static void diag_md_session_exit(void)
{
	int i, proc;
	struct diag_md_session_t *session_info = NULL;

	for (proc = 0; proc < NUM_DIAG_MD_DEV; proc++) {
		for (i = 0; i < NUM_MD_SESSIONS; i++) {
			if (driver->md_session_map[proc][i]) {
				session_info = driver->md_session_map[proc][i];
				diag_log_mask_free(session_info->log_mask);
				kfree(session_info->log_mask);
				session_info->log_mask = NULL;
				diag_msg_mask_free(session_info->msg_mask,
					session_info);
				kfree(session_info->msg_mask);
				session_info->msg_mask = NULL;
				diag_event_mask_free(session_info->event_mask);
				kfree(session_info->event_mask);
				session_info->event_mask = NULL;
				kfree(session_info);
				session_info = NULL;
				driver->md_session_map[proc][i] = NULL;
			}
		}
		mutex_destroy(&driver->md_session_lock);
		driver->md_session_mask[proc] = 0;
		driver->md_session_mode[proc] = DIAG_MD_NONE;
	}
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
	if (driver->md_session_mode[proc] == DIAG_MD_PERIPHERAL &&
	    (driver->md_session_mask[proc] & peripheral_mask) != 0)
		return -EINVAL;

	mutex_lock(&driver->md_session_lock);
	new_session = diag_md_session_get_pid(current->tgid);
	if (!new_session) {
		new_session = kzalloc(sizeof(struct diag_md_session_t),
					GFP_KERNEL);
		if (!new_session) {
			mutex_unlock(&driver->md_session_lock);
			return -ENOMEM;
		}
		new_session->peripheral_mask[proc] = 0;
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
		err = diag_event_mask_copy(new_session->event_mask,
								   &event_mask);
		if (err) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
				 "return value of event copy. err %d\n", err);
			goto fail_peripheral;
		}
		new_session->msg_mask_tbl_count = 0;
		err = diag_msg_mask_copy(new_session, new_session->msg_mask,
					&msg_mask);
		if (err) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
				 "return value of msg copy. err %d\n", err);
			goto fail_peripheral;
		}
	}
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if ((MD_PERIPHERAL_MASK(i) & peripheral_mask) == 0)
			continue;
		if (driver->md_session_map[proc][i] != NULL) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
				 "another instance present for %d\n", i);
			err = -EEXIST;
			goto fail_peripheral;
		}
		new_session->peripheral_mask[proc] |= MD_PERIPHERAL_MASK(i);
		driver->md_session_map[proc][i] = new_session;
		driver->md_session_mask[proc] |= MD_PERIPHERAL_MASK(i);
	}
	timer_setup(&new_session->hdlc_reset_timer,
		diag_md_hdlc_reset_timer_func,
		0);

	driver->md_session_mode[proc] = DIAG_MD_PERIPHERAL;
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
	diag_msg_mask_free(new_session->msg_mask,
		new_session);
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
	int proc;

	session_info = diag_md_session_get_pid(pid);
	if (!session_info)
		return;
	for (proc = 0; proc < NUM_DIAG_MD_DEV; proc++) {
		for (i = 0; i < NUM_MD_SESSIONS; i++) {
			if (driver->md_session_map[proc][i] != session_info)
				continue;
			driver->md_session_map[proc][i] = NULL;
			driver->md_session_mask[proc] &=
				~session_info->peripheral_mask[proc];
			driver->p_hdlc_disabled[i] =
				driver->hdlc_disabled;
		}
	}
	diag_log_mask_free(session_info->log_mask);
	kfree(session_info->log_mask);
	session_info->log_mask = NULL;
	diag_msg_mask_free(session_info->msg_mask,
		session_info);
	kfree(session_info->msg_mask);
	session_info->msg_mask = NULL;
	diag_event_mask_free(session_info->event_mask);
	kfree(session_info->event_mask);
	session_info->event_mask = NULL;
	del_timer(&session_info->hdlc_reset_timer);
	for (proc = 0; proc < NUM_DIAG_MD_DEV; proc++) {
		for (i = 0; i < NUM_MD_SESSIONS && !found; i++) {
			if (driver->md_session_map[proc][i] != NULL)
				found = 1;
		}
		driver->md_session_mode[proc] = (found) ? DIAG_MD_PERIPHERAL :
								DIAG_MD_NONE;
		found = 0;
	}
	kfree(session_info);
	session_info = NULL;
	DIAG_LOG(DIAG_DEBUG_USERSPACE, "cleared up session\n");
}

struct diag_md_session_t *diag_md_session_get_pid(int pid)
{
	int i;
	int proc;

	if (pid <= 0)
		return NULL;

	for (proc = 0; proc < NUM_DIAG_MD_DEV; proc++) {
		for (i = 0; i < NUM_MD_SESSIONS; i++) {
			if (driver->md_session_map[proc][i] &&
				driver->md_session_map[proc][i]->pid == pid)
				return driver->md_session_map[proc][i];
		}
	}
	return NULL;
}

struct diag_md_session_t *diag_md_session_get_peripheral(int proc,
				uint8_t peripheral)
{
	if (peripheral >= NUM_MD_SESSIONS)
		return NULL;
	return driver->md_session_map[proc][peripheral];
}

/*
 * diag_md_session_match_pid_peripheral
 *
 *	1. Pass valid PID and get all the peripherals in logging session
 *		for that PID
 *	2. Pass valid Peipheral and get the pid logging for that peripheral
 *
 */

int diag_md_session_match_pid_peripheral(int proc, int pid,
	uint8_t peripheral)
{
	int i, flag = 0;

	if (pid <= 0 || peripheral >= NUM_MD_SESSIONS)
		return -EINVAL;

	if (!peripheral) {
		for (proc = 0; proc < NUM_DIAG_MD_DEV; proc++) {
			for (i = 0; i < NUM_MD_SESSIONS; i++) {
				if (driver->md_session_map[proc][i] &&
					driver->md_session_map[proc][i]->pid ==
								pid) {
					peripheral |= 1 << i;
					flag = 1;
				}
			}
			if (flag)
				return peripheral;
		}
	}

	if (!pid) {
		if (driver->md_session_map[proc][peripheral])
			return driver->md_session_map[proc][peripheral]->pid;
	}

	return -EINVAL;
}

static int diag_md_peripheral_switch(int proc, int pid,
				int peripheral_mask, int req_mode)
{
	int i, bit = 0;
	struct diag_md_session_t *session_info = NULL;

	session_info = diag_md_session_get_pid(pid);
	if (!session_info)
		return -EINVAL;
	if (req_mode != DIAG_USB_MODE && req_mode != DIAG_MEMORY_DEVICE_MODE)
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
			if (driver->md_session_map[proc][i] != session_info)
				return -EINVAL;
			driver->md_session_map[proc][i] = NULL;
			driver->md_session_mask[proc] &= ~bit;
			session_info->peripheral_mask[proc] &= ~bit;

		} else {
			if (driver->md_session_map[proc][i] != NULL)
				return -EINVAL;
			driver->md_session_map[proc][i] = session_info;
			driver->md_session_mask[proc] |= bit;
			session_info->peripheral_mask[proc] |= bit;

		}
	}

	driver->md_session_mode[proc] = DIAG_MD_PERIPHERAL;
	DIAG_LOG(DIAG_DEBUG_USERSPACE, "Changed Peripherals:0x%x to mode:%d\n",
		peripheral_mask, req_mode);
	return 0;
}

static int diag_md_session_check(int proc, int curr_mode, int req_mode,
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
		if (driver->md_session_mode[proc] == DIAG_MD_NONE
		    && driver->md_session_mask[proc] == 0 &&
			driver->logging_mask[proc]) {
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
			if (bit & driver->logging_mask[proc])
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
		peripheral_mask = session_info->peripheral_mask[proc];
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
			err = diag_md_peripheral_switch(proc, current->tgid,
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
		change_mask = driver->md_session_mask[proc] &
				param->peripheral_mask;
		session_info = diag_md_session_get_pid(current->tgid);

		if (session_info && driver->md_session_mode[proc] !=
							DIAG_MD_NONE) {
			if ((session_info->peripheral_mask[proc] & change_mask)
							!= change_mask) {
				DIAG_LOG(DIAG_DEBUG_USERSPACE,
				    "Another MD Session owns a requested peripheral\n");
				mutex_unlock(&driver->md_session_lock);
				return -EINVAL;
			}
			err = diag_md_peripheral_switch(proc, current->tgid,
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
				param->peripheral_mask, proc);
			mutex_lock(&driver->hdlc_disable_mutex);
			for (i = 0; i < NUM_MD_SESSIONS; i++) {
				if ((param->peripheral_mask > 0) &&
					(param->peripheral_mask & (1 << i)))
					driver->p_hdlc_disabled[i] = 0;
			}
			mutex_unlock(&driver->hdlc_disable_mutex);
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
	if (peripheral_mask & DIAG_CON_WDSP)
		ret |= (1 << PERIPHERAL_WDSP);
	if (peripheral_mask & DIAG_CON_CDSP)
		ret |= (1 << PERIPHERAL_CDSP);
	if (peripheral_mask & DIAG_CON_NPU)
		ret |= (1 << PERIPHERAL_NPU);
	if (peripheral_mask & DIAG_CON_UPD_WLAN)
		ret |= (1 << UPD_WLAN);
	if (peripheral_mask & DIAG_CON_UPD_AUDIO)
		ret |= (1 << UPD_AUDIO);
	if (peripheral_mask & DIAG_CON_UPD_SENSORS)
		ret |= (1 << UPD_SENSORS);
	if (peripheral_mask & DIAG_CON_UPD_CHARGER)
		ret |= (1 << UPD_CHARGER);
	return ret;
}

static void diag_switch_logging_clear_mask(
		struct diag_logging_mode_param_t *param, int pid)
{
	int new_mode;
	struct diag_md_session_t *session_info = NULL;

	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_pid(pid);
	if (!session_info) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE, "Invalid pid: %d\n", pid);
		mutex_unlock(&driver->md_session_lock);
		return;
	}
	mutex_unlock(&driver->md_session_lock);

	if (!param)
		return;

	if (!param->peripheral_mask) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"asking for mode switch with no peripheral mask set\n");
		return;
	}

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
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"Request to switch to invalid mode: %d\n",
			param->req_mode);
		return;
	}
	if ((new_mode == DIAG_USB_MODE) && diag_mask_clear_param)
		diag_clear_masks(pid);

}
static int diag_switch_logging_proc(struct diag_logging_mode_param_t *param,
					int new_mode, uint32_t peripheral_mask)
{
	int proc = 0, local_proc = 0, err = 0, curr_mode;
	uint8_t do_switch = 1;

	for (proc = 0; proc < NUM_DIAG_MD_DEV; proc++) {
		local_proc = 1 << proc;
		if (param->device_mask & (local_proc)) {
			curr_mode = driver->logging_mode[proc];
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
				"request to switch logging from %d mask:%0x to new_mode %d mask:%0x\n",
				curr_mode, driver->md_session_mask[proc],
				new_mode, peripheral_mask);

			err = diag_md_session_check(proc, curr_mode, new_mode,
							param, &do_switch);
			if (err) {
				DIAG_LOG(DIAG_DEBUG_USERSPACE,
					"err from diag_md_session_check, err: %d\n",
					err);
				return err;
			}

			if (do_switch == 0) {
				DIAG_LOG(DIAG_DEBUG_USERSPACE,
					"not switching modes c: %d n: %d\n",
					curr_mode, new_mode);
				return 0;
			}

			diag_ws_reset(DIAG_WS_MUX);
			err = diag_mux_switch_logging(proc, &new_mode,
					&peripheral_mask);
			if (err) {
				pr_err("diag: In %s, unable to switch mode from %d to %d, err: %d\n",
					__func__, curr_mode, new_mode, err);
				driver->logging_mode[proc] = curr_mode;
				return err;
			}
			driver->logging_mode[proc] = new_mode;
			driver->logging_mask[proc] = peripheral_mask;
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
				"Switch logging to %d mask:%0x\n", new_mode,
					peripheral_mask);

			/* Update to take peripheral_mask */
			if (new_mode != DIAG_MEMORY_DEVICE_MODE &&
				new_mode != DIAG_MULTI_MODE) {
				diag_update_real_time_vote(
						DIAG_PROC_MEMORY_DEVICE,
						MODE_REALTIME,
						ALL_PROC);
			} else {
				diag_update_proc_vote(DIAG_PROC_MEMORY_DEVICE,
							VOTE_UP,
							ALL_PROC);
			}

			if (!((new_mode == DIAG_MEMORY_DEVICE_MODE ||
				new_mode == DIAG_MULTI_MODE) &&
				curr_mode == DIAG_USB_MODE)) {
				queue_work(driver->diag_real_time_wq,
					   &driver->diag_real_time_work);
			}
		}
		peripheral_mask =
			diag_translate_mask(param->peripheral_mask);
	}
	return err;
}
static int diag_switch_logging(struct diag_logging_mode_param_t *param)
{
	int new_mode, i = 0;
	int err = 0, peripheral = 0;
	uint32_t peripheral_mask = 0, pd_mask = 0;

	if (!param)
		return -EINVAL;

	if (!param->peripheral_mask) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"asking for mode switch with no peripheral mask set\n");
		return -EINVAL;
	}
	if (!param->device_mask) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"asking for mode switch with no device mask set\n");
		return -EINVAL;
	}

	if (param->pd_mask) {
		pd_mask = diag_translate_mask(param->pd_mask);
		param->diag_id = 0;
		param->pd_val = 0;
		param->peripheral = -EINVAL;

		for (i = UPD_WLAN; i < NUM_MD_SESSIONS; i++) {
			if (pd_mask & (1 << i)) {
				if (diag_search_diagid_by_pd(i, &param->diag_id,
					&param->peripheral)) {
					param->pd_val = i;
					break;
				}
			}
		}

		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"diag: pd_mask = %d, diag_id = %d, peripheral = %d, pd_val = %d\n",
			param->pd_mask, param->diag_id,
			param->peripheral, param->pd_val);

		if (!param->diag_id ||
			(param->pd_val < UPD_WLAN) ||
			(param->pd_val >= NUM_MD_SESSIONS)) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"diag_id support is not present for the pd mask = %d\n",
			param->pd_mask);
			return -EINVAL;
		}

		peripheral = param->peripheral;
		if ((peripheral < PERIPHERAL_MODEM) ||
			(peripheral >= NUM_PERIPHERALS)) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"Invalid peripheral: %d\n", peripheral);
			return -EINVAL;
		}
		i = param->pd_val - UPD_WLAN;
		mutex_lock(&driver->md_session_lock);
		if (driver->md_session_map[DIAG_LOCAL_PROC][peripheral] &&
			(MD_PERIPHERAL_MASK(peripheral) &
			diag_mux->mux_mask[DIAG_LOCAL_PROC]) &&
			!driver->pd_session_clear[i]) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"diag_fr: User PD is already logging onto active peripheral logging\n");
			mutex_unlock(&driver->md_session_lock);
			driver->pd_session_clear[i] = 0;
			return -EINVAL;
		}
		mutex_unlock(&driver->md_session_lock);
		peripheral_mask =
			diag_translate_mask(param->pd_mask);
		param->peripheral_mask = peripheral_mask;
		if (!driver->pd_session_clear[i]) {
			driver->pd_logging_mode[i] = 1;
			driver->num_pd_session += 1;
		}
		driver->pd_session_clear[i] = 0;
	} else {
		peripheral_mask =
			diag_translate_mask(param->peripheral_mask);
		param->peripheral_mask = peripheral_mask;
	}

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
	err = diag_switch_logging_proc(param, new_mode, peripheral_mask);

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
	if (!(driver->data_ready[i] & DEINIT_TYPE)) {
		driver->data_ready[i] |= DEINIT_TYPE;
		atomic_inc(&driver->data_ready_notif[i]);
	}
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
	int peripheral = 0;
	uint8_t diag_id = 0;

	if (copy_from_user(&params, (void __user *)ioarg, sizeof(params)))
		return -EFAULT;

	diag_map_pd_to_diagid(params.peripheral, &diag_id, &peripheral);

	if ((peripheral < 0) ||
		peripheral >= NUM_PERIPHERALS) {
		pr_err("diag: In %s, invalid peripheral = %d\n", __func__,
		       peripheral);
		return -EIO;
	}

	if (params.peripheral > NUM_PERIPHERALS &&
		!driver->feature[peripheral].pd_buffering) {
		pr_err("diag: In %s, pd buffering not supported for peripheral:%d\n",
			__func__, peripheral);
		return -EIO;
	}

	if (!driver->feature[peripheral].peripheral_buffering) {
		pr_err("diag: In %s, peripheral %d doesn't support buffering\n",
		       __func__, peripheral);
		return -EIO;
	}

	mutex_lock(&driver->mode_lock);
	driver->buffering_flag[params.peripheral] = 1;
	mutex_unlock(&driver->mode_lock);

	return diag_send_peripheral_buffering_mode(&params);
}

static int diag_ioctl_peripheral_drain_immediate(unsigned long ioarg)
{
	uint8_t pd, diag_id = 0;
	int peripheral = 0;

	if (copy_from_user(&pd, (void __user *)ioarg, sizeof(uint8_t)))
		return -EFAULT;

	diag_map_pd_to_diagid(pd, &diag_id, &peripheral);

	if ((peripheral < 0) ||
		peripheral >= NUM_PERIPHERALS) {
		pr_err("diag: In %s, invalid peripheral %d\n", __func__,
		       peripheral);
		return -EINVAL;
	}

	if (pd > NUM_PERIPHERALS &&
		!driver->feature[peripheral].pd_buffering) {
		pr_err("diag: In %s, pd buffering not supported for peripheral:%d\n",
			__func__, peripheral);
		return -EIO;
	}

	return diag_send_peripheral_drain_immediate(pd, diag_id, peripheral);
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
	uint8_t hdlc_support, i;
	int peripheral = -EINVAL, proc = DIAG_LOCAL_PROC;
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

	peripheral =
		diag_md_session_match_pid_peripheral(DIAG_LOCAL_PROC,
							current->tgid,
		0);
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (peripheral > 0 && session_info) {
			if (peripheral & (1 << i))
				driver->p_hdlc_disabled[i] =
				session_info->hdlc_disabled;
			else if (!diag_md_session_get_peripheral(proc, i))
				driver->p_hdlc_disabled[i] =
				driver->hdlc_disabled;
		} else {
			if (!diag_md_session_get_peripheral(proc, i))
				driver->p_hdlc_disabled[i] =
				driver->hdlc_disabled;
		}
	}

	mutex_unlock(&driver->md_session_lock);
	mutex_unlock(&driver->hdlc_disable_mutex);
	diag_update_md_clients(HDLC_SUPPORT_TYPE);

	return 0;
}

/*
 * diag_search_peripheral_by_pd(uint8_t pd_val)
 *
 * Function will return peripheral by searching pd in the
 * diag_id table.
 *
 */

int diag_search_peripheral_by_pd(uint8_t pd_val)
{
	struct list_head *start;
	struct list_head *temp;
	struct diag_id_tbl_t *item = NULL;

	mutex_lock(&driver->diag_id_mutex);
	list_for_each_safe(start, temp, &driver->diag_id_list) {
		item = list_entry(start, struct diag_id_tbl_t, link);
		if (pd_val == item->pd_val) {
			mutex_unlock(&driver->diag_id_mutex);
			return item->peripheral;
		}
	}
	mutex_unlock(&driver->diag_id_mutex);
	return -EINVAL;
}

/*
 * diag_search_diagid_by_pd(uint8_t pd_val,
 *	uint8_t *diag_id, int *peripheral)
 *
 * Function will update the peripheral and diag_id
 * from the pd passed as an argument.
 *
 */

uint8_t diag_search_diagid_by_pd(uint8_t pd_val,
	uint8_t *diag_id, int *peripheral)
{
	struct list_head *start;
	struct list_head *temp;
	struct diag_id_tbl_t *item = NULL;

	mutex_lock(&driver->diag_id_mutex);
	list_for_each_safe(start, temp, &driver->diag_id_list) {
		item = list_entry(start, struct diag_id_tbl_t, link);
		if (pd_val == item->pd_val) {
			*peripheral = item->peripheral;
			*diag_id = item->diag_id;
			mutex_unlock(&driver->diag_id_mutex);
			return 1;
		}
	}
	mutex_unlock(&driver->diag_id_mutex);
	return 0;
}

/*
 * diag_query_pd_name(char *process_name, char *search_str)
 *
 * The function searches the pd string in the control packet string
 * from the peripheral
 *
 */

static int diag_query_pd_name(char *process_name, char *search_str)
{
	if (!process_name)
		return -EINVAL;

	if (strnstr(process_name, search_str, strlen(process_name)))
		return 1;

	return 0;
}

/*
 * diag_query_pd_name(char *process_name, char *search_str)
 *
 * The function returns the PD information based on the presence of
 * the pd specific string in the control packet's string from peripheral.
 *
 */

int diag_query_pd(char *process_name)
{
	if (!process_name)
		return -EINVAL;

	if (diag_query_pd_name(process_name, "modem/root_pd"))
		return PERIPHERAL_MODEM;
	if (diag_query_pd_name(process_name, "adsp/root_pd"))
		return PERIPHERAL_LPASS;
	if (diag_query_pd_name(process_name, "slpi/root_pd"))
		return PERIPHERAL_SENSORS;
	if (diag_query_pd_name(process_name, "cdsp/root_pd"))
		return PERIPHERAL_CDSP;
	if (diag_query_pd_name(process_name, "npu/root_pd"))
		return PERIPHERAL_NPU;
	if (diag_query_pd_name(process_name, "wlan_pd"))
		return UPD_WLAN;
	if (diag_query_pd_name(process_name, "audio_pd"))
		return UPD_AUDIO;
	if (diag_query_pd_name(process_name, "sensor_pd"))
		return UPD_SENSORS;
	if (diag_query_pd_name(process_name, "charger_pd"))
		return UPD_CHARGER;

	return -EINVAL;
}

/*
 * diag_ioctl_query_pd_logging(struct diag_logging_mode_param_t *param)
 *
 * IOCTL handler based on the parameter received will check on which peripheral
 * the PD is present and validate if the peripheral supports the diag_id and
 * tagging feature.
 *
 */

static int diag_ioctl_query_pd_logging(struct diag_logging_mode_param_t *param)
{
	int ret = -EINVAL, i = 0;
	int peripheral = -EINVAL;
	uint32_t pd_mask = 0;

	if (!param)
		return -EINVAL;

	if (!param->pd_mask) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"query with no pd mask set, returning error\n");
		return -EINVAL;
	}

	if (param->pd_mask) {
		pd_mask = diag_translate_mask(param->pd_mask);
		for (i = UPD_WLAN; i < NUM_MD_SESSIONS; i++) {
			if (pd_mask & (1 << i)) {
				peripheral = diag_search_peripheral_by_pd(i);
				break;
			}
		}
		if (peripheral < 0) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"diag_id support is not present for the pd mask = %d\n",
			param->pd_mask);
			return -EINVAL;
		}
	}
	mutex_lock(&driver->diag_cntl_mutex);
	DIAG_LOG(DIAG_DEBUG_USERSPACE,
	"diag: %s: Untagging support on APPS is %s\n", __func__,
	((driver->supports_apps_header_untagging) ?
	"present" : "absent"));

	DIAG_LOG(DIAG_DEBUG_USERSPACE,
	"diag: %s: Tagging support on peripheral = %d is %s\n",
	__func__, peripheral,
	(driver->feature[peripheral].untag_header ?
	"present" : "absent"));

	if (driver->supports_apps_header_untagging &&
		driver->feature[peripheral].untag_header)
		ret = 0;

	mutex_unlock(&driver->diag_cntl_mutex);
	return ret;
}

void diag_map_index_to_hw_accel(uint8_t index,
	uint8_t *hw_accel_type, uint8_t *hw_accel_ver)
{
	*hw_accel_type = 0;
	*hw_accel_ver = 0;

	switch (index) {
	case DIAG_HW_ACCEL_TYPE_STM:
		*hw_accel_type = DIAG_HW_ACCEL_TYPE_STM;
		*hw_accel_ver = DIAG_HW_ACCEL_VER_MIN;
		break;
	case DIAG_HW_ACCEL_TYPE_ATB:
		*hw_accel_type = DIAG_HW_ACCEL_TYPE_ATB;
		*hw_accel_ver = DIAG_HW_ACCEL_VER_MIN;
		break;
	default:
		break;
	}
}

int diag_map_hw_accel_type_ver(uint8_t hw_accel_type, uint8_t hw_accel_ver)
{
	int index = -EINVAL;

	if (hw_accel_ver == DIAG_HW_ACCEL_VER_MIN) {
		switch (hw_accel_type) {
		case DIAG_HW_ACCEL_TYPE_STM:
			index = DIAG_HW_ACCEL_TYPE_STM;
			break;
		case DIAG_HW_ACCEL_TYPE_ATB:
			index = DIAG_HW_ACCEL_TYPE_ATB;
			break;
		default:
			index = -EINVAL;
			break;
		}
	}
	return index;
}

static int diag_ioctl_query_pd_featuremask(
	struct diag_hw_accel_query_sub_payload_rsp_t *query_params)
{
	int f_index = -1;

	if (!query_params)
		return -EINVAL;

	if (query_params->hw_accel_type > DIAG_HW_ACCEL_TYPE_MAX ||
		query_params->hw_accel_ver > DIAG_HW_ACCEL_VER_MAX) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE, "Invalid parameters\n");
		return -EINVAL;
	}

	mutex_lock(&driver->diagid_v2_mutex);

	f_index = diag_map_hw_accel_type_ver(query_params->hw_accel_type,
				query_params->hw_accel_ver);
	if (f_index < 0) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE, "Invalid feature index\n");
		query_params->diagid_mask_supported = 0;
		query_params->diagid_mask_enabled = 0;
	} else {
		query_params->diagid_mask_supported = DIAGIDV2_FEATURE(f_index);
		query_params->diagid_mask_enabled = DIAGIDV2_STATUS(f_index);
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
		"HW acceleration: type: %d, ver:%d, supported diagid mask: %d, enabled diagid mask: %d\n",
		query_params->hw_accel_type,
		query_params->hw_accel_ver,
		query_params->diagid_mask_supported,
		query_params->diagid_mask_enabled);
	}
	mutex_unlock(&driver->diagid_v2_mutex);
	return 0;
}

static int diag_ioctl_passthru_control_func(
	struct diag_hw_accel_cmd_req_t *req_params)
{
	return diag_send_passthru_ctrl_pkt(req_params);
}

static void diag_query_session_pid(struct diag_query_pid_t *param)
{
	int prev_pid = 0, test_pid = 0, i = 0, count = 0;
	int local_proc = 0;
	int proc = 0;

	for (proc = 0; proc < NUM_DIAG_MD_DEV; proc++) {
		local_proc = 1<<proc;
		if (param->device_mask & (local_proc)) {
			for (i = 0; i <= NUM_PERIPHERALS; i++) {
				if (driver->md_session_map[proc][i]) {
					test_pid =
					driver->md_session_map[proc][i]->pid;
					count++;
					if (!prev_pid)
						prev_pid = test_pid;
					if (test_pid != prev_pid) {
						DIAG_LOG(DIAG_DEBUG_USERSPACE,
						"diag: One of the peripherals is being logged already\n");
						param->pid = -EINVAL;
					}
				}
			}
			if (i == count && prev_pid)
				param->pid = prev_pid;
		}
	}
}
static void diag_ioctl_query_session_pid(struct diag_query_pid_t *param)
{
	int prev_pid = 0, test_pid = 0, i = 0;
	unsigned int proc = DIAG_LOCAL_PROC;
	uint32_t pd_mask = 0, peripheral_mask = 0;
	struct diag_md_session_t *info = NULL;

	param->pid = 0;

	if (param->pd_mask && param->peripheral_mask) {
		param->pid = -EINVAL;
		return;
	} else if (param->peripheral_mask) {
		if (param->peripheral_mask == DIAG_CON_ALL) {
			diag_query_session_pid(param);
		} else {
			peripheral_mask =
				diag_translate_mask(param->peripheral_mask);
			for (i = 0; i <= NUM_PERIPHERALS; i++) {
				if (driver->md_session_map[proc][i] &&
					(peripheral_mask &
					MD_PERIPHERAL_MASK(i))) {
					info =
					driver->md_session_map[proc][i];
					if (peripheral_mask !=
					info->peripheral_mask[proc]) {
						DIAG_LOG(DIAG_DEBUG_USERSPACE,
						"diag: Invalid Peripheral mask given as input\n");
						param->pid = -EINVAL;
						return;
					}
					test_pid = info->pid;
					if (!prev_pid)
						prev_pid = test_pid;
					if (test_pid != prev_pid) {
						DIAG_LOG(DIAG_DEBUG_USERSPACE,
						"diag: One of the peripherals is logged in different session\n");
						param->pid = -EINVAL;
						return;
					}
				}
			}
			param->pid = prev_pid;
		}
	} else if (param->pd_mask) {
		pd_mask =
			diag_translate_mask(param->pd_mask);
		for (i = UPD_WLAN; i < NUM_MD_SESSIONS; i++) {
			if (driver->md_session_map[proc][i] &&
				(pd_mask & MD_PERIPHERAL_MASK(i))) {
				info =
				driver->md_session_map[proc][i];
				if (pd_mask !=
				info->peripheral_mask[proc]) {
					DIAG_LOG(DIAG_DEBUG_USERSPACE,
					"diag: Invalid PD mask given as input\n");
					param->pid = -EINVAL;
					return;
				}
				test_pid = info->pid;
				if (!prev_pid)
					prev_pid = test_pid;
				if (test_pid != prev_pid) {
					DIAG_LOG(DIAG_DEBUG_USERSPACE,
					"diag: One of the PDs is being logged already\n");
					param->pid = -EINVAL;
					return;
				}
			}
		}
		param->pid = prev_pid;
	}
	DIAG_LOG(DIAG_DEBUG_USERSPACE,
	"diag: Pid for the active ODL session: %d\n", param->pid);
}

static int diag_ioctl_register_callback(unsigned long ioarg)
{
	int err = 0;
	struct diag_callback_reg_t reg;

	if (copy_from_user(&reg, (void __user *)ioarg,
			   sizeof(struct diag_callback_reg_t))) {
		return -EFAULT;
	}

	if (reg.proc < 0 || reg.proc >= NUM_DIAG_MD_DEV) {
		pr_err("diag: In %s, invalid proc %d for callback registration\n",
		       __func__, reg.proc);
		return -EINVAL;
	}

	if (driver->md_session_mode[reg.proc] == DIAG_MD_PERIPHERAL)
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
	if (!entries)
		return -ENOMEM;


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

static long diagchar_ioctl_init(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL;
	uint16_t delayed_rsp_id = 0;
	struct diag_logging_mode_param_t mode_param;

	switch (iocmd) {
	case DIAG_IOCTL_COMMAND_DEREG:
		result = diag_ioctl_cmd_dereg();
		break;
	case DIAG_IOCTL_SWITCH_LOGGING:
		if (copy_from_user((void *)&mode_param, (void __user *)ioarg,
				   sizeof(mode_param)))
			return -EFAULT;
		diag_switch_logging_clear_mask(&mode_param, current->tgid);
		mutex_lock(&driver->diagchar_mutex);
		result = diag_switch_logging(&mode_param);
		mutex_unlock(&driver->diagchar_mutex);
		break;
	case DIAG_IOCTL_GET_DELAYED_RSP_ID:
		delayed_rsp_id = diag_get_next_delayed_rsp_id();
		if (copy_to_user((void __user *)ioarg, &delayed_rsp_id,
				 sizeof(uint16_t)))
			result = -EFAULT;
		else
			result = 0;
		break;
	case DIAG_IOCTL_LSM_DEINIT:
		result = diag_ioctl_lsm_deinit();
		break;
	}
	return result;
}

static long diagchar_ioctl_dci(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL, client_id = 0;
	struct diag_dci_client_tbl *dci_client = NULL;

	switch (iocmd) {
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
	}
	return result;
}

static long diagchar_ioctl_buffering(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL;

	switch (iocmd) {
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
	}
	return result;
}

static long diagchar_ioctl_remote(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL;
	uint16_t remote_dev;

	remote_dev = diag_get_remote_device_mask();
	if (copy_to_user((void __user *)ioarg, &remote_dev,
		sizeof(uint16_t)))
		result = -EFAULT;
	else
		result = 1;

	return result;
}

static long diagchar_ioctl_misc(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL;

	switch (iocmd) {
	case DIAG_IOCTL_REGISTER_CALLBACK:
		result = diag_ioctl_register_callback(ioarg);
		break;
	case DIAG_IOCTL_HDLC_TOGGLE:
		result = diag_ioctl_hdlc_toggle(ioarg);
		break;
	}
	return result;
}

static long diagchar_ioctl_mdlog(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL;
	struct diag_logging_mode_param_t mode_param;
	struct diag_con_all_param_t con_param;
	struct diag_query_pid_t pid_query;

	switch (iocmd) {
	case DIAG_IOCTL_QUERY_PD_LOGGING:
		if (copy_from_user((void *)&mode_param, (void __user *)ioarg,
				   sizeof(mode_param)))
			return -EFAULT;
		result = diag_ioctl_query_pd_logging(&mode_param);
		break;
	case DIAG_IOCTL_QUERY_CON_ALL:
		con_param.diag_con_all = DIAG_CON_ALL;
		con_param.num_peripherals = NUM_PERIPHERALS;
		con_param.upd_map_supported = 1;
		if (copy_to_user((void __user *)ioarg, &con_param,
				sizeof(struct diag_con_all_param_t)))
			result = -EFAULT;
		else
			result = 0;
		break;
	case DIAG_IOCTL_QUERY_MD_PID:
		if (copy_from_user((void *)&pid_query, (void __user *)ioarg,
				   sizeof(pid_query))) {
			result = -EFAULT;
			break;
		}
		mutex_lock(&driver->md_session_lock);
		diag_ioctl_query_session_pid(&pid_query);
		mutex_unlock(&driver->md_session_lock);

		if (copy_to_user((void __user *)ioarg, &pid_query,
				sizeof(pid_query)))
			result = -EFAULT;
		else
			result = 0;
		break;
	}
	return result;
}

static long diagchar_ioctl_hw_accel(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL;
	struct diag_hw_accel_query_sub_payload_rsp_t query_params;
	struct diag_hw_accel_cmd_req_t req_params;

	switch (iocmd) {
	case DIAG_IOCTL_QUERY_PD_FEATUREMASK:
		if (copy_from_user((void *)&query_params, (void __user *)ioarg,
			sizeof(struct diag_hw_accel_query_sub_payload_rsp_t))) {
			result = -EFAULT;
			break;
		}
		result = diag_ioctl_query_pd_featuremask(&query_params);
		if (result) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE, "%02x %02x %02x %02x\n",
			query_params.hw_accel_type,
			query_params.hw_accel_ver,
			query_params.diagid_mask_supported,
			query_params.diagid_mask_enabled);
			break;
		}
		if (copy_to_user((void __user *)ioarg, &query_params,
				sizeof(query_params)))
			result = -EFAULT;
		else
			result = 0;
		break;
	case DIAG_IOCTL_PASSTHRU_CONTROL:
		if (copy_from_user((void *)&req_params, (void __user *)ioarg,
				   sizeof(struct diag_hw_accel_cmd_req_t))) {
			result = -EFAULT;
			break;
		}
		result = diag_ioctl_passthru_control_func(&req_params);
		if (result)
			break;
		if (copy_to_user((void __user *)ioarg, &req_params,
				sizeof(req_params)))
			result = -EFAULT;
		else
			result = 0;
		break;
	}
	return result;
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

	if (iocmd == DIAG_IOCTL_COMMAND_REG) {
		result = diag_ioctl_cmd_reg_compat(ioarg);
	} else if (iocmd >= DIAG_IOCTL_COMMAND_DEREG &&
			iocmd <= DIAG_IOCTL_LSM_DEINIT) {
		result = diagchar_ioctl_init(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_DCI_INIT &&
			iocmd <= DIAG_IOCTL_DCI_CLEAR_EVENTS) {
		result = diagchar_ioctl_dci(filp, iocmd, ioarg);
	} else if (iocmd == DIAG_IOCTL_REMOTE_DEV) {
		result = diagchar_ioctl_remote(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_VOTE_REAL_TIME &&
			iocmd <= DIAG_IOCTL_PERIPHERAL_BUF_DRAIN) {
		result = diagchar_ioctl_buffering(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_REGISTER_CALLBACK &&
			iocmd <= DIAG_IOCTL_HDLC_TOGGLE) {
		result = diagchar_ioctl_misc(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_QUERY_PD_LOGGING &&
			iocmd <= DIAG_IOCTL_QUERY_MD_PID) {
		result = diagchar_ioctl_mdlog(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_QUERY_PD_FEATUREMASK &&
			iocmd <= DIAG_IOCTL_PASSTHRU_CONTROL) {
		result = diagchar_ioctl_hw_accel(filp, iocmd, ioarg);
	} else {
		result = -EINVAL;
	}
	return result;
}
#endif

long diagchar_ioctl(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int result = -EINVAL;

	if (iocmd == DIAG_IOCTL_COMMAND_REG) {
		result = diag_ioctl_cmd_reg(ioarg);
	} else if (iocmd >= DIAG_IOCTL_COMMAND_DEREG &&
			iocmd <= DIAG_IOCTL_LSM_DEINIT) {
		result = diagchar_ioctl_init(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_DCI_INIT &&
			iocmd <= DIAG_IOCTL_DCI_CLEAR_EVENTS) {
		result = diagchar_ioctl_dci(filp, iocmd, ioarg);
	} else if (iocmd == DIAG_IOCTL_REMOTE_DEV) {
		result = diagchar_ioctl_remote(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_VOTE_REAL_TIME &&
			iocmd <= DIAG_IOCTL_PERIPHERAL_BUF_DRAIN) {
		result = diagchar_ioctl_buffering(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_REGISTER_CALLBACK &&
			iocmd <= DIAG_IOCTL_HDLC_TOGGLE) {
		result = diagchar_ioctl_misc(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_QUERY_PD_LOGGING &&
			iocmd <= DIAG_IOCTL_QUERY_MD_PID) {
		result = diagchar_ioctl_mdlog(filp, iocmd, ioarg);
	} else if (iocmd >= DIAG_IOCTL_QUERY_PD_FEATUREMASK &&
			iocmd <= DIAG_IOCTL_PASSTHRU_CONTROL) {
		result = diagchar_ioctl_hw_accel(filp, iocmd, ioarg);
	} else {
		result = -EINVAL;
	}
	return result;
}

static int diag_process_apps_data_hdlc(unsigned char *buf, int len,
				       int pkt_type)
{
	int err = 0, wait_err = 0;
	int ret = PKT_DROP;
	struct diag_apps_data_t *data = &hdlc_data;
	struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
	struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
	unsigned long flags;
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

	if (max_encoded_size > DIAG_MAX_HDLC_BUF_SIZE) {
		pr_err_ratelimited("diag: In %s, encoded data is larger %d than the buffer size %d\n",
		       __func__, max_encoded_size, DIAG_MAX_HDLC_BUF_SIZE);
		return -EBADMSG;
	}

	send.state = DIAG_STATE_START;
	send.pkt = buf;
	send.last = (void *)(buf + len - 1);
	send.terminate = 1;

wait_for_buffer:
	wait_err = wait_event_interruptible_timeout(driver->hdlc_wait_q,
			(data->flushed == 0),
			msecs_to_jiffies(PKT_PROCESS_TIMEOUT));
	if (wait_err <= 0) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
		"diag: Timeout while waiting for hdlc buffer to be flushed, err: %d\n",
		wait_err);
		return PKT_DROP;
	}
	spin_lock_irqsave(&driver->diagmem_lock, flags);
	if (data->flushed) {
		spin_unlock_irqrestore(&driver->diagmem_lock, flags);
		goto wait_for_buffer;
	}
	if (!data->buf) {
		data->buf = diagmem_alloc(driver, DIAG_MAX_HDLC_BUF_SIZE +
					APF_DIAG_PADDING,
					  POOL_TYPE_HDLC);
		if (!data->buf) {
			ret = PKT_DROP;
			spin_unlock_irqrestore(&driver->diagmem_lock, flags);
			goto fail_ret;
		}
		data->allocated = 1;
		data->flushed = 0;
	}

	if ((DIAG_MAX_HDLC_BUF_SIZE - data->len) <= max_encoded_size) {
		data->flushed = 1;
		spin_unlock_irqrestore(&driver->diagmem_lock, flags);
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
		goto wait_for_buffer;
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
		data->flushed = 1;
		spin_unlock_irqrestore(&driver->diagmem_lock, flags);
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
wait_for_agg_buff:
		wait_err = wait_event_interruptible_timeout(driver->hdlc_wait_q,
				(data->flushed == 0),
				msecs_to_jiffies(PKT_PROCESS_TIMEOUT));
		if (wait_err <= 0) {
			DIAG_LOG(DIAG_DEBUG_USERSPACE,
			"diag: Timeout while waiting for hdlc aggregation buffer to be flushed, err: %d\n",
			wait_err);
			return PKT_DROP;
		}

		spin_lock_irqsave(&driver->diagmem_lock, flags);
		if (data->flushed) {
			spin_unlock_irqrestore(&driver->diagmem_lock, flags);
			goto wait_for_agg_buff;
		}
		data->buf = diagmem_alloc(driver, DIAG_MAX_HDLC_BUF_SIZE +
					APF_DIAG_PADDING,
					 POOL_TYPE_HDLC);
		if (!data->buf) {
			ret = PKT_DROP;
			spin_unlock_irqrestore(&driver->diagmem_lock, flags);
			goto fail_ret;
		}
		data->allocated = 1;
		data->flushed = 0;

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
		data->flushed = 1;
		spin_unlock_irqrestore(&driver->diagmem_lock, flags);
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
		return PKT_ALLOC;
	}
	spin_unlock_irqrestore(&driver->diagmem_lock, flags);
	return PKT_ALLOC;

fail_free_buf:
	spin_lock_irqsave(&driver->diagmem_lock, flags);
	if (data->allocated)
		diagmem_free(driver, data->buf, POOL_TYPE_HDLC);
	data->buf = NULL;
	data->len = 0;
	data->allocated = 0;
	data->flushed = 0;
	spin_unlock_irqrestore(&driver->diagmem_lock, flags);
fail_ret:
	return ret;
}

static int diag_process_apps_data_non_hdlc(unsigned char *buf, int len,
					   int pkt_type)
{
	int err = 0, wait_err = 0;
	int ret = PKT_DROP;
	struct diag_pkt_frame_t header;
	struct diag_apps_data_t *data = &non_hdlc_data;
	unsigned long flags;
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
wait_for_buffer:
	wait_err = wait_event_interruptible_timeout(driver->hdlc_wait_q,
					(data->flushed == 0),
					msecs_to_jiffies(PKT_PROCESS_TIMEOUT));
	if (wait_err <= 0) {
		DIAG_LOG(DIAG_DEBUG_USERSPACE,
		"diag: Timeout while waiting for non-hdlc buffer to be flushed, err: %d\n",
		wait_err);
		return PKT_DROP;
	}

	spin_lock_irqsave(&driver->diagmem_lock, flags);
	if (data->flushed) {
		spin_unlock_irqrestore(&driver->diagmem_lock, flags);
		goto wait_for_buffer;
	}
	if (!data->buf) {
		data->buf = diagmem_alloc(driver, DIAG_MAX_HDLC_BUF_SIZE +
					APF_DIAG_PADDING,
					  POOL_TYPE_HDLC);
		if (!data->buf) {
			ret = PKT_DROP;
			spin_unlock_irqrestore(&driver->diagmem_lock, flags);
			goto fail_ret;
		}
		data->allocated = 1;
		data->flushed = 0;
	}
	if ((DIAG_MAX_HDLC_BUF_SIZE - data->len) <= max_pkt_size) {
		data->flushed = 1;
		spin_unlock_irqrestore(&driver->diagmem_lock, flags);
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
		goto wait_for_buffer;
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
		data->flushed = 1;
		spin_unlock_irqrestore(&driver->diagmem_lock, flags);
		err = diag_mux_write(DIAG_LOCAL_PROC, data->buf, data->len,
				     data->ctxt);
		if (err) {
			ret = -EIO;
			goto fail_free_buf;
		}
		return PKT_ALLOC;
	}
	spin_unlock_irqrestore(&driver->diagmem_lock, flags);
	return PKT_ALLOC;

fail_free_buf:
	spin_lock_irqsave(&driver->diagmem_lock, flags);
	if (data->allocated)
		diagmem_free(driver, data->buf, POOL_TYPE_HDLC);
	data->buf = NULL;
	data->len = 0;
	data->allocated = 0;
	data->flushed = 0;
	spin_unlock_irqrestore(&driver->diagmem_lock, flags);
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
	if (len >= sizeof(int))
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
						token_offset, len)) {
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
			diag_send_error_rsp((void *)(user_space_data), len,
						current->tgid);
	}
fail:
	diagmem_free(driver, user_space_data, mempool);
	user_space_data = NULL;
	return ret;
}

static int diag_user_process_userspace_data(const char __user *buf, int len)
{
	int err = 0;
	int max_retries = 50;
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
			pr_err("diag: Integer underflow in %s, payload size: %d\n",
			       __func__, len);
			return -EBADMSG;
		}
		token_offset = sizeof(int);
		len -= sizeof(int);
	}

	/* Check masks for On-Device logging */
	if (driver->mask_check) {
		if (!mask_request_validate(driver->user_space_data_buf +
					   token_offset, len)) {
			pr_alert("diag: mask request Invalid\n");
			return -EFAULT;
		}
	}

	/* send masks to local processor now */
	if (!remote_proc) {
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_pid(current->tgid);
		if (!session_info) {
			pr_err("diag:In %s request came from invalid md session pid:%d\n",
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
	hdlc_disabled = driver->p_hdlc_disabled[APPS_DATA];
	mutex_unlock(&driver->hdlc_disable_mutex);
	if (hdlc_disabled)
		ret = diag_process_apps_data_non_hdlc(user_space_data, len,
						      pkt_type);
	else
		ret = diag_process_apps_data_hdlc(user_space_data, len,
						  pkt_type);
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
	struct pid *pid_struct = NULL;
	struct task_struct *task_s = NULL;
	int proc = 0;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++) {
		if (driver->client_map[i].pid == current->tgid) {
			index = i;
			break;
		}
	}
	mutex_unlock(&driver->diagchar_mutex);

	if (index == -1) {
		pr_err("diag: Client PID not found in table\n");
		return -EINVAL;
	}
	if (!buf) {
		pr_err("diag: bad address from user side\n");
		return -EFAULT;
	}
	wait_event_interruptible(driver->wait_q,
			atomic_read(&driver->data_ready_notif[index]) > 0);

	mutex_lock(&driver->diagchar_mutex);

	if (driver->data_ready[index] & USER_SPACE_DATA_TYPE) {
		for (proc = 0 ; proc < NUM_DIAG_MD_DEV; proc++) {
			if (driver->logging_mode[proc] ==
				DIAG_MEMORY_DEVICE_MODE ||
			 driver->logging_mode[proc] == DIAG_MULTI_MODE) {
				pr_debug("diag: process woken up\n");
				/*Copy the type of data being passed*/
				data_type = driver->data_ready[index] &
						USER_SPACE_DATA_TYPE;
				driver->data_ready[index] ^=
						USER_SPACE_DATA_TYPE;
				atomic_dec(&driver->data_ready_notif[index]);
				COPY_USER_SPACE_OR_ERR(buf, data_type,
							sizeof(int));
				if (ret == -EFAULT)
					goto exit;
				/* place holder for number of data field */
				ret += sizeof(int);
				mutex_lock(&driver->md_session_lock);
				session_info =
					diag_md_session_get_pid(current->tgid);
				exit_stat = diag_md_copy_to_user(buf, &ret,
							count,
						 session_info);
				mutex_unlock(&driver->md_session_lock);
				goto exit;
			}
		}
		/* In case, the thread wakes up and the logging mode is not
		 * memory device any more, the condition needs to be cleared.
		 */
		driver->data_ready[index] ^= USER_SPACE_DATA_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
	}

	if (driver->data_ready[index] & HDLC_SUPPORT_TYPE) {
		data_type = driver->data_ready[index] & HDLC_SUPPORT_TYPE;
		driver->data_ready[index] ^= HDLC_SUPPORT_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
		COPY_USER_SPACE_OR_ERR(buf, data_type, sizeof(int));
		if (ret == -EFAULT)
			goto exit;

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
		COPY_USER_SPACE_OR_ERR(buf, data_type, 4);
		if (ret == -EFAULT)
			goto exit;
		driver->data_ready[index] ^= DEINIT_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
		mutex_unlock(&driver->diagchar_mutex);
		diag_remove_client_entry(file);
		return ret;
	}

	if (driver->data_ready[index] & MSG_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & MSG_MASKS_TYPE;
		mutex_unlock(&driver->diagchar_mutex);
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_peripheral(DIAG_LOCAL_PROC,
								APPS_DATA);
		COPY_USER_SPACE_OR_ERR(buf, data_type, sizeof(int));
		if (ret == -EFAULT) {
			mutex_unlock(&driver->md_session_lock);
			goto end;
		}
		if (!session_info)
			mutex_unlock(&driver->md_session_lock);
		write_len = diag_copy_to_user_msg_mask(buf + ret, count,
						       session_info);
		if (session_info)
			mutex_unlock(&driver->md_session_lock);
		if (write_len > 0)
			ret += write_len;
		mutex_lock(&driver->diagchar_mutex);
		driver->data_ready[index] ^= MSG_MASKS_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
		goto exit;
	}

	if (driver->data_ready[index] & EVENT_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & EVENT_MASKS_TYPE;
		mutex_unlock(&driver->diagchar_mutex);
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_peripheral(DIAG_LOCAL_PROC,
								APPS_DATA);
		COPY_USER_SPACE_OR_ERR(buf, data_type, 4);
		if (ret == -EFAULT) {
			mutex_unlock(&driver->md_session_lock);
			goto end;
		}
		if (session_info && session_info->event_mask &&
		    session_info->event_mask->ptr) {
			COPY_USER_SPACE_OR_ERR(buf + sizeof(int),
					*(session_info->event_mask->ptr),
					session_info->event_mask->mask_len);
			if (ret == -EFAULT) {
				mutex_unlock(&driver->md_session_lock);
				goto end;
			}
		} else {
			COPY_USER_SPACE_OR_ERR(buf + sizeof(int),
						*(event_mask.ptr),
						event_mask.mask_len);
			if (ret == -EFAULT) {
				mutex_unlock(&driver->md_session_lock);
				goto end;
			}
		}
		mutex_unlock(&driver->md_session_lock);
		mutex_lock(&driver->diagchar_mutex);
		driver->data_ready[index] ^= EVENT_MASKS_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
		goto exit;
	}

	if (driver->data_ready[index] & LOG_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & LOG_MASKS_TYPE;
		mutex_unlock(&driver->diagchar_mutex);
		mutex_lock(&driver->md_session_lock);
		session_info = diag_md_session_get_peripheral(DIAG_LOCAL_PROC,
								APPS_DATA);
		COPY_USER_SPACE_OR_ERR(buf, data_type, sizeof(int));
		if (ret == -EFAULT) {
			mutex_unlock(&driver->md_session_lock);
			goto end;
		}
		if (!session_info)
			mutex_unlock(&driver->md_session_lock);
		write_len = diag_copy_to_user_log_mask(buf + ret, count,
						       session_info);
		if (session_info)
			mutex_unlock(&driver->md_session_lock);
		if (write_len > 0)
			ret += write_len;
		mutex_lock(&driver->diagchar_mutex);
		driver->data_ready[index] ^= LOG_MASKS_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
		goto exit;
	}

	if (driver->data_ready[index] & PKT_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & PKT_TYPE;
		COPY_USER_SPACE_OR_ERR(buf, data_type, sizeof(data_type));
		if (ret == -EFAULT)
			goto exit;

		COPY_USER_SPACE_OR_ERR(buf + sizeof(data_type),
					*(driver->apps_req_buf),
					driver->apps_req_buf_len);
		if (ret == -EFAULT)
			goto exit;
		driver->data_ready[index] ^= PKT_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
		driver->in_busy_pktdata = 0;
		goto exit;
	}

	if (driver->data_ready[index] & DCI_PKT_TYPE) {
		/* Copy the type of data being passed */
		data_type = driver->data_ready[index] & DCI_PKT_TYPE;
		COPY_USER_SPACE_OR_ERR(buf, data_type, 4);
		if (ret == -EFAULT)
			goto exit;

		COPY_USER_SPACE_OR_ERR(buf+4, *(driver->dci_pkt_buf),
					driver->dci_pkt_length);
		if (ret == -EFAULT)
			goto exit;

		driver->data_ready[index] ^= DCI_PKT_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
		driver->in_busy_dcipktdata = 0;
		goto exit;
	}

	if (driver->data_ready[index] & DCI_EVENT_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & DCI_EVENT_MASKS_TYPE;
		COPY_USER_SPACE_OR_ERR(buf, data_type, 4);
		if (ret == -EFAULT)
			goto exit;

		COPY_USER_SPACE_OR_ERR(buf+4, driver->num_dci_client, 4);
		if (ret == -EFAULT)
			goto exit;

		COPY_USER_SPACE_OR_ERR(buf + 8,
			(dci_ops_tbl[DCI_LOCAL_PROC].event_mask_composite),
			DCI_EVENT_MASK_SIZE);
		if (ret == -EFAULT)
			goto exit;

		driver->data_ready[index] ^= DCI_EVENT_MASKS_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
		goto exit;
	}

	if (driver->data_ready[index] & DCI_LOG_MASKS_TYPE) {
		/*Copy the type of data being passed*/
		data_type = driver->data_ready[index] & DCI_LOG_MASKS_TYPE;
		COPY_USER_SPACE_OR_ERR(buf, data_type, 4);
		if (ret == -EFAULT)
			goto exit;

		COPY_USER_SPACE_OR_ERR(buf+4, driver->num_dci_client, 4);
		if (ret == -EFAULT)
			goto exit;

		COPY_USER_SPACE_OR_ERR(buf+8,
			(dci_ops_tbl[DCI_LOCAL_PROC].log_mask_composite),
			DCI_LOG_MASK_SIZE);
		if (ret == -EFAULT)
			goto exit;
		driver->data_ready[index] ^= DCI_LOG_MASKS_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
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
			pid_struct = find_get_pid(entry->tgid);
			if (!pid_struct)
				continue;
			task_s = get_pid_task(pid_struct, PIDTYPE_PID);
			if (!task_s) {
				DIAG_LOG(DIAG_DEBUG_DCI,
				"diag: valid task doesn't exist for pid = %d\n",
				entry->tgid);
				put_pid(pid_struct);
				continue;
			}
			if (task_s == entry->client) {
				if (entry->client->tgid != current->tgid) {
					put_task_struct(task_s);
					put_pid(pid_struct);
					continue;
				}
			}
			if (!entry->in_service) {
				put_task_struct(task_s);
				put_pid(pid_struct);
				continue;
			}
			if (copy_to_user(buf + ret, &data_type, sizeof(int))) {
				put_task_struct(task_s);
				put_pid(pid_struct);
				mutex_unlock(&driver->dci_mutex);
				goto end;
			}
			ret += sizeof(int);
			if (copy_to_user(buf + ret, &entry->client_info.token,
				sizeof(int))) {
				put_task_struct(task_s);
				put_pid(pid_struct);
				mutex_unlock(&driver->dci_mutex);
				goto end;
			}
			ret += sizeof(int);
			copy_dci_data = 1;
			exit_stat = diag_copy_dci(buf, count, entry, &ret);
			if (exit_stat == 1) {
				put_task_struct(task_s);
				put_pid(pid_struct);
				mutex_lock(&driver->diagchar_mutex);
				driver->data_ready[index] ^= DCI_DATA_TYPE;
				atomic_dec(&driver->data_ready_notif[index]);
				mutex_unlock(&driver->diagchar_mutex);
				mutex_unlock(&driver->dci_mutex);
				goto end;
			}
			put_task_struct(task_s);
			put_pid(pid_struct);
			continue;
		}
		mutex_lock(&driver->diagchar_mutex);
		driver->data_ready[index] ^= DCI_DATA_TYPE;
		atomic_dec(&driver->data_ready_notif[index]);
		mutex_unlock(&driver->diagchar_mutex);
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
	int token = 0;
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

	err = copy_from_user(&token, buf+4, sizeof(int));
	if (err) {
		pr_err("diag: copy failed for user space data\n");
		return -EIO;
	}
	if (token < 0)
		token = diag_get_remote(token);
	else
		token = 0;

	if (driver->logging_mode[token] == DIAG_USB_MODE &&
		!driver->usb_connected) {
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
		if (pkt_type && driver->logging_mode[DIAG_LOCAL_PROC] ==
			DIAG_USB_MODE &&
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

void diag_ws_init(void)
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

void diag_ws_on_notify(void)
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

void diag_ws_release(void)
{
	if (driver->dci_ws.ref_count == 0 && driver->md_ws.ref_count == 0)
		pm_relax(driver->diag_dev);
}

#ifdef CONFIG_IPC_LOGGING
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
				DIAG_DEBUG_USERSPACE | DIAG_DEBUG_BRIDGE;
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
		pr_info("diagchar cdev registration failed !\n");
		return err;
	}

	driver->diagchar_class = class_create(THIS_MODULE, "diag");

	if (IS_ERR(driver->diagchar_class)) {
		pr_err("Error creating diagchar class.\n");
		return PTR_ERR(driver->diagchar_class);
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

static int __init diagchar_init(void)
{
	dev_t dev;
	int ret, i;
	int proc;

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
	timer_setup(&drain_timer, drain_timer_func, 0);
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
	for (proc = 0; proc < NUM_DIAG_MD_DEV; proc++)
		driver->logging_mode[proc] = DIAG_USB_MODE;
	for (i = 0; i < NUM_UPD; i++) {
		driver->pd_logging_mode[i] = 0;
		driver->pd_session_clear[i] = 0;
	}
	driver->num_pd_session = 0;
	driver->mask_check = 0;
	driver->in_busy_pktdata = 0;
	driver->in_busy_dcipktdata = 0;
	driver->rsp_buf_ctxt = SET_BUF_CTXT(APPS_DATA, TYPE_CMD, TYPE_CMD);
	hdlc_data.ctxt = SET_BUF_CTXT(APPS_DATA, TYPE_DATA, 1);
	hdlc_data.ctxt |= SET_HDLC_CTXT(HDLC_CTXT);
	hdlc_data.len = 0;
	hdlc_data.allocated = 0;
	hdlc_data.flushed = 0;
	non_hdlc_data.ctxt = SET_BUF_CTXT(APPS_DATA, TYPE_DATA, 1);
	non_hdlc_data.ctxt |= SET_HDLC_CTXT(NON_HDLC_CTXT);
	non_hdlc_data.len = 0;
	non_hdlc_data.allocated = 0;
	non_hdlc_data.flushed = 0;
	mutex_init(&driver->hdlc_disable_mutex);
	mutex_init(&driver->diagchar_mutex);
	mutex_init(&driver->diag_notifier_mutex);
	mutex_init(&driver->diagid_v2_mutex);
	mutex_init(&driver->diag_file_mutex);
	mutex_init(&driver->delayed_rsp_mutex);
	mutex_init(&apps_data_mutex);
	mutex_init(&driver->msg_mask_lock);
	mutex_init(&driver->hdlc_recovery_mutex);
	for (i = 0; i < NUM_PERIPHERALS; i++) {
		mutex_init(&driver->diagfwd_channel_mutex[i]);
		mutex_init(&driver->rpmsginfo_mutex[i]);
		driver->diag_id_sent[i] = 0;
	}
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
	if (!driver->incoming_pkt.data) {
		ret = -ENOMEM;
		goto fail;
	}
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
	strlcpy(driver->name, "diag", 5);
	/* Get major number from kernel and initialize */
	ret = alloc_chrdev_region(&dev, driver->minor_start,
				    driver->num, driver->name);
	if (!ret) {
		driver->major = MAJOR(dev);
		driver->minor_start = MINOR(dev);
	} else {
		pr_err("diag: Major number not allocated\n");
		goto fail;
	}
	driver->cdev = cdev_alloc();
	ret = diagchar_setup_cdev(dev);
	if (ret)
		goto fail;
	mutex_init(&driver->diag_id_mutex);
	INIT_LIST_HEAD(&driver->diag_id_list);
	diag_add_diag_id_to_list(DIAG_ID_APPS, "APPS", APPS_DATA, APPS_DATA);
	pr_debug("diagchar initialized now\n");
	#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	diag_register_with_mhi();
	#endif
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
	diag_remote_exit();
	return ret;

}

static void diagchar_exit(void)
{
	pr_info("diagchar exiting...\n");
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
	pr_info("done diagchar exit\n");
}

module_init(diagchar_init);
module_exit(diagchar_exit);
