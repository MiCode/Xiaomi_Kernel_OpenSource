/*
 *  Copyright (c) 2016 MediaTek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "btmtk_config.h"
#include <linux/version.h>
#include <linux/firmware.h>
#include <linux/slab.h>

#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio_func.h>
#include <linux/module.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/device.h>

/* Define for proce node */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "btmtk_define.h"
#include "btmtk_drv.h"
#include "btmtk_sdio.h"

/* Used for WoBLE on EINT */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/input.h>

#include <linux/of.h>
#include <linux/of_irq.h>

typedef int (*sdio_card_probe)(struct sdio_func *func,
					const struct sdio_device_id *id);

static struct bt_stereo_clk stereo_clk;
static u64 sys_clk_tmp;
static unsigned int stereo_irq;
struct _OSAL_UNSLEEPABLE_LOCK_ stereo_spin_lock;

static dev_t g_devIDfwlog;
static struct class *pBTClass;
static struct device *pBTDev;
struct device *pBTDevfwlog;
static wait_queue_head_t inq;
static wait_queue_head_t fw_log_inq;
static struct fasync_struct *fasync;
/*static int btmtk_woble_state = BTMTK_WOBLE_STATE_UNKNOWN;*/

static int need_reset_stack;
static int get_hci_reset;
static int need_reopen;
static int wlan_remove_done;

static u8 user_rmmod;
static int need_retry_load_woble;

struct completion g_done;
unsigned char probe_counter;
struct btmtk_private *g_priv;
#define STR_COREDUMP_END "coredump end\n\n"
const u8 READ_ADDRESS_EVENT[] = { 0x0e, 0x0a, 0x01, 0x09, 0x10, 0x00 };

static struct ring_buffer metabuffer;
static struct ring_buffer fwlog_metabuffer;

u8 probe_ready;
/* record firmware version */
static char fw_version_str[FW_VERSION_BUF_SIZE];
static struct proc_dir_entry *g_proc_dir;

static int btmtk_fops_state = BTMTK_FOPS_STATE_UNKNOWN;
static DEFINE_MUTEX(btmtk_fops_state_mutex);
#define FOPS_MUTEX_LOCK()	mutex_lock(&btmtk_fops_state_mutex)
#define FOPS_MUTEX_UNLOCK()	mutex_unlock(&btmtk_fops_state_mutex)

/** read_write for proc */
static int btmtk_proc_show(struct seq_file *m, void *v);
static int btmtk_proc_open(struct inode *inode, struct  file *file);
static void btmtk_proc_create_new_entry(void);
static int btmtk_sdio_trigger_fw_assert(void);

static int btmtk_sdio_RegisterBTIrq(struct btmtk_sdio_card *data);
static int btmtk_sdio_woble_input_init(struct btmtk_sdio_card *data);
static void btmtk_sdio_woble_input_deinit(struct btmtk_sdio_card *data);
/* bluetooth KPI feautre, bperf */
u8 btmtk_bluetooth_kpi;
u8 btmtk_log_lvl = BTMTK_LOG_LEVEL_DEFAULT;

static char event_need_compare[EVENT_COMPARE_SIZE] = {0};
static char event_need_compare_len;
static char event_compare_status;
/*add special header in the beginning of even, stack won't recognize these event*/

/* timer for coredump end */
struct task_struct *wait_dump_complete_tsk;
struct task_struct *wait_wlan_remove_tsk;
int wlan_status = WLAN_STATUS_DEFAULT;

static int dump_data_counter;
static int dump_data_length;
static struct file *fw_dump_file;

const struct file_operations BT_proc_fops = {
	.open = btmtk_proc_open,
	.read = seq_read,
	.release = single_release,
};

static const struct btmtk_sdio_card_reg btmtk_reg_6630 = {
	.cfg = 0x03,
	.host_int_mask = 0x04,
	.host_intstatus = 0x05,
	.card_status = 0x20,
	.sq_read_base_addr_a0 = 0x10,
	.sq_read_base_addr_a1 = 0x11,
	.card_fw_status0 = 0x40,
	.card_fw_status1 = 0x41,
	.card_rx_len = 0x42,
	.card_rx_unit = 0x43,
	.io_port_0 = 0x00,
	.io_port_1 = 0x01,
	.io_port_2 = 0x02,
	.int_read_to_clear = false,
	.func_num = 2,
	.chip_id = 0x6630,
};

static const struct btmtk_sdio_card_reg btmtk_reg_6632 = {
	.cfg = 0x03,
	.host_int_mask = 0x04,
	.host_intstatus = 0x05,
	.card_status = 0x20,
	.sq_read_base_addr_a0 = 0x10,
	.sq_read_base_addr_a1 = 0x11,
	.card_fw_status0 = 0x40,
	.card_fw_status1 = 0x41,
	.card_rx_len = 0x42,
	.card_rx_unit = 0x43,
	.io_port_0 = 0x00,
	.io_port_1 = 0x01,
	.io_port_2 = 0x02,
	.int_read_to_clear = false,
	.func_num = 2,
	.chip_id = 0x6632,
};

static const struct btmtk_sdio_card_reg btmtk_reg_7668 = {
	.cfg = 0x03,
	.host_int_mask = 0x04,
	.host_intstatus = 0x05,
	.card_status = 0x20,
	.sq_read_base_addr_a0 = 0x10,
	.sq_read_base_addr_a1 = 0x11,
	.card_fw_status0 = 0x40,
	.card_fw_status1 = 0x41,
	.card_rx_len = 0x42,
	.card_rx_unit = 0x43,
	.io_port_0 = 0x00,
	.io_port_1 = 0x01,
	.io_port_2 = 0x02,
	.int_read_to_clear = false,
	.func_num = 2,
	.chip_id = 0x7668,
};

static const struct btmtk_sdio_card_reg btmtk_reg_7663 = {
	.cfg = 0x03,
	.host_int_mask = 0x04,
	.host_intstatus = 0x05,
	.card_status = 0x20,
	.sq_read_base_addr_a0 = 0x10,
	.sq_read_base_addr_a1 = 0x11,
	.card_fw_status0 = 0x40,
	.card_fw_status1 = 0x41,
	.card_rx_len = 0x42,
	.card_rx_unit = 0x43,
	.io_port_0 = 0x00,
	.io_port_1 = 0x01,
	.io_port_2 = 0x02,
	.int_read_to_clear = false,
	.func_num = 2,
	.chip_id = 0x7663,
};

static const struct btmtk_sdio_card_reg btmtk_reg_7666 = {
	.cfg = 0x03,
	.host_int_mask = 0x04,
	.host_intstatus = 0x05,
	.card_status = 0x20,
	.sq_read_base_addr_a0 = 0x10,
	.sq_read_base_addr_a1 = 0x11,
	.card_fw_status0 = 0x40,
	.card_fw_status1 = 0x41,
	.card_rx_len = 0x42,
	.card_rx_unit = 0x43,
	.io_port_0 = 0x00,
	.io_port_1 = 0x01,
	.io_port_2 = 0x02,
	.int_read_to_clear = false,
	.func_num = 2,
	.chip_id = 0x7666,
};

static const struct btmtk_sdio_device btmtk_sdio_6630 = {
	.helper = "mtmk/sd8688_helper.bin",
	.reg = &btmtk_reg_6630,
	.support_pscan_win_report = false,
	.sd_blksz_fw_dl = 64,
	.supports_fw_dump = false,
};

static const struct btmtk_sdio_device btmtk_sdio_6632 = {
	.helper = "mtmk/sd8688_helper.bin",
	.reg = &btmtk_reg_6632,
	.support_pscan_win_report = false,
	.sd_blksz_fw_dl = 64,
	.supports_fw_dump = false,
};

static const struct btmtk_sdio_device btmtk_sdio_7668 = {
	.helper = "mtmk/sd8688_helper.bin",
	.reg = &btmtk_reg_7668,
	.support_pscan_win_report = false,
	.sd_blksz_fw_dl = 64,
	.supports_fw_dump = false,
};

static const struct btmtk_sdio_device btmtk_sdio_7663 = {
	.helper = "mtmk/sd8688_helper.bin",
	.reg = &btmtk_reg_7663,
	.support_pscan_win_report = false,
	.sd_blksz_fw_dl = 64,
	.supports_fw_dump = false,
};

static const struct btmtk_sdio_device btmtk_sdio_7666 = {
	.helper = "mtmk/sd8688_helper.bin",
	.reg = &btmtk_reg_7666,
	.support_pscan_win_report = false,
	.sd_blksz_fw_dl = 64,
	.supports_fw_dump = false,
};

typedef int (*sdio_reset_func) (struct mmc_card *card);
static sdio_reset_func pf_sdio_reset;

static u8 hci_cmd_snoop_buf[HCI_SNOOP_ENTRY_NUM][HCI_SNOOP_BUF_SIZE];
static u8 hci_cmd_snoop_len[HCI_SNOOP_ENTRY_NUM];
static unsigned int hci_cmd_snoop_timestamp[HCI_SNOOP_ENTRY_NUM];

static u8 hci_event_snoop_buf[HCI_SNOOP_ENTRY_NUM][HCI_SNOOP_BUF_SIZE];
static u8 hci_event_snoop_len[HCI_SNOOP_ENTRY_NUM];
static unsigned int hci_event_snoop_timestamp[HCI_SNOOP_ENTRY_NUM];

static u8 hci_acl_snoop_buf[HCI_SNOOP_ENTRY_NUM][HCI_SNOOP_BUF_SIZE];
static u8 hci_acl_snoop_len[HCI_SNOOP_ENTRY_NUM];
static unsigned int hci_acl_snoop_timestamp[HCI_SNOOP_ENTRY_NUM];

static u8 fw_log_snoop_buf[HCI_SNOOP_ENTRY_NUM][HCI_SNOOP_BUF_SIZE];
static u8 fw_log_snoop_len[HCI_SNOOP_ENTRY_NUM];
static unsigned int fw_log_snoop_timestamp[HCI_SNOOP_ENTRY_NUM];

static int hci_cmd_snoop_index;
static int hci_event_snoop_index;
static int hci_acl_snoop_index;
static int fw_log_snoop_index;

unsigned char *txbuf;
static unsigned char *rxbuf;
static unsigned char *userbuf;
static unsigned char *userbuf_fwlog;
static u32 rx_length;
static struct btmtk_sdio_card *g_card;

static u32 reg_CHISR; /* Add for debug, need remove later */

#define SDIO_VENDOR_ID_MEDIATEK 0x037A

static const struct sdio_device_id btmtk_sdio_ids[] = {
	/* Mediatek SD8688 Bluetooth device */
	{ SDIO_DEVICE(SDIO_VENDOR_ID_MEDIATEK, 0x6630),
			.driver_data = (unsigned long) &btmtk_sdio_6630 },

	{ SDIO_DEVICE(SDIO_VENDOR_ID_MEDIATEK, 0x6632),
			.driver_data = (unsigned long) &btmtk_sdio_6632 },

	{ SDIO_DEVICE(SDIO_VENDOR_ID_MEDIATEK, 0x7668),
			.driver_data = (unsigned long) &btmtk_sdio_7668 },

	{ SDIO_DEVICE(SDIO_VENDOR_ID_MEDIATEK, 0x7663),
			.driver_data = (unsigned long) &btmtk_sdio_7663 },

	{ SDIO_DEVICE(SDIO_VENDOR_ID_MEDIATEK, 0x7666),
			.driver_data = (unsigned long) &btmtk_sdio_7666 },

	{ }	/* Terminating entry */
};
MODULE_DEVICE_TABLE(sdio, btmtk_sdio_ids);

static int btmtk_clean_queue(void);
static void btmtk_sdio_do_reset_or_wait_wlan_remove_done(void);
static int btmtk_sdio_download_partial_rom_patch(u8 *fwbuf, int firmwarelen);
static int btmtk_sdio_probe(struct sdio_func *func,
					const struct sdio_device_id *id);
static void btmtk_sdio_L0_hook_new_probe(sdio_card_probe pFn_Probe);
static int btmtk_sdio_set_sleep(void);
static int btmtk_sdio_set_audio(void);
static int btmtk_sdio_send_hci_cmd(u8 cmd_type, u8 *cmd, int cmd_len,
		const u8 *event, const int event_len,
		int total_timeout);

static int timestamp_threshold[BTMTK_SDIO_RX_CHECKPOINT_NUM];
static unsigned int timestamp[BTMTK_SDIO_RX_CHECKPOINT_NUM][BTMTK_SDIO_TIMESTAMP_NUM];

static inline unsigned long btmtk_kallsyms_lookup_name(const char *name)
{
	unsigned long ret = 0;

	ret = kallsyms_lookup_name(name);
	if (ret) {
#ifdef CONFIG_ARM
#ifdef CONFIG_THUMB2_KERNEL
		/* set bit 0 in address for thumb mode */
		ret |= 1;
#endif
#endif
	}
	return ret;
}

static unsigned int btmtk_sdio_get_microseconds(void)
{
	struct timeval now;

	do_gettimeofday(&now);
	return now.tv_sec * 1000000 + now.tv_usec;
}

void btmtk_sdio_timestamp(enum BTMTK_SDIO_RX_CHECKPOINT type)
{
	static int now;
	static unsigned int ms_intr_now, ms_intr_last;
	unsigned int ms_now;
	unsigned int sum, max, min, avg;
	int total, checkpoint_index, timestamp_index;
	int threshold_zero[BTMTK_SDIO_RX_CHECKPOINT_NUM];

	memset(threshold_zero, 0, sizeof(threshold_zero));
	if (memcmp(timestamp_threshold, threshold_zero, sizeof(timestamp_threshold)) == 0)
		return;

	if (type == BTMTK_SDIO_RX_CHECKPOINT_INTR)
		now++;
	if (now == BTMTK_SDIO_TIMESTAMP_NUM)
		now = 0;

	ms_now = btmtk_sdio_get_microseconds();

	if (type == BTMTK_SDIO_RX_CHECKPOINT_INTR) {
		if (ms_intr_last == 0)
			timestamp[type][now] = 0;
		else
			timestamp[type][now] = ms_now - ms_intr_last;
		ms_intr_last = ms_now;
		ms_intr_now = ms_now;
	} else {
		timestamp[type][now] = ms_now - ms_intr_now;
	}

	/* show statistics */
	if (timestamp_threshold[type] != 0 && timestamp[type][now] > timestamp_threshold[type]) {
		BTMTK_INFO("---------------------START---------------------");
		BTMTK_INFO("Type(%d), Cur = %u", type, timestamp[type][now]);
		for (checkpoint_index = 0; checkpoint_index < BTMTK_SDIO_RX_CHECKPOINT_NUM; checkpoint_index++) {
			sum = 0;
			total = 0;
			max = 0;
			min = 0xFFFFFFFF;
			avg = 0;
			for (timestamp_index = 0; timestamp_index < BTMTK_SDIO_TIMESTAMP_NUM; timestamp_index++) {
				if (timestamp[checkpoint_index][timestamp_index] != 0) {
					sum += timestamp[checkpoint_index][timestamp_index];
					total++;
					if (timestamp[checkpoint_index][timestamp_index] > max)
						max = timestamp[checkpoint_index][timestamp_index];
					if (timestamp[checkpoint_index][timestamp_index] < min)
						min = timestamp[checkpoint_index][timestamp_index];
				}
			}
			avg = sum/total;
			BTMTK_INFO("Type(%d), Max = %u, Min = %u, Avg = %u",
				checkpoint_index, max, min, avg);
		}
		BTMTK_INFO("----------------------END----------------------");
	}
}

static int btmtk_fops_get_state(void)
{
	return btmtk_fops_state;
}

static void btmtk_fops_set_state(int new_state)
{
	static const char * const fstate_msg[BTMTK_FOPS_STATE_MAX] = {"UNKNOWN", "INIT", "OPENING", "OPENED", "CLOSING", "CLOSED"};

	BTMTK_INFO("%s: FOPS_%s(%d) -> FOPS_%s(%d)", __func__, fstate_msg[btmtk_fops_state],
			btmtk_fops_state, fstate_msg[new_state], new_state);
	btmtk_fops_state = new_state;
}

void btmtk_sdio_stop_wait_wlan_remove_tsk(void)
{
	if (wait_wlan_remove_tsk == NULL)
		BTMTK_INFO("wait_wlan_remove_tsk is NULL");
	else if (IS_ERR(wait_wlan_remove_tsk))
		BTMTK_INFO("wait_wlan_remove_tsk is error");
	else {
		BTMTK_INFO("call kthread_stop wait_wlan_remove_tsk");
		kthread_stop(wait_wlan_remove_tsk);
		wait_wlan_remove_tsk = NULL;
	}
}

int btmtk_sdio_notify_wlan_remove_start(void)
{
	/* notify_wlan_remove_start */
	int ret = 0;
	typedef void (*pnotify_wlan_remove_start) (int reserved);
	char *notify_wlan_remove_start_func_name;
	pnotify_wlan_remove_start pnotify_wlan_remove_start_func;

	BTMTK_INFO("wlan_status %d", wlan_status);
	if (wlan_status == WLAN_STATUS_CALL_REMOVE_START) {
		/* do notify before, just return */
		return ret;
	}

	if (is_mt7663(g_card))
		notify_wlan_remove_start_func_name =
			"BT_rst_L0_notify_WF_step1";
	else
		notify_wlan_remove_start_func_name =
			"notify_wlan_remove_start";

	pnotify_wlan_remove_start_func =
		(pnotify_wlan_remove_start)btmtk_kallsyms_lookup_name
			(notify_wlan_remove_start_func_name);

	BTMTK_INFO(L0_RESET_TAG);
	/* void notify_wlan_remove_start(void) */
	if (pnotify_wlan_remove_start_func) {
		BTMTK_INFO("do notify %s", notify_wlan_remove_start_func_name);
		wlan_remove_done = 0;
		if (is_mt7663(g_card))
			pnotify_wlan_remove_start_func(0);
		else
			pnotify_wlan_remove_start_func(1);
		wlan_status = WLAN_STATUS_CALL_REMOVE_START;
	} else {
		ret = -1;
		BTMTK_ERR("do not get %s", notify_wlan_remove_start_func_name);
		wlan_status = WLAN_STATUS_IS_NOT_LOAD;
		wlan_remove_done = 1;
	}
	return ret;
}

/*============================================================================*/
/* Interface Functions : timer for coredump inform wifi */
/*============================================================================*/
static void btmtk_sdio_wakeup_mainthread_do_reset(void)
{
	if (g_priv) {
		g_priv->btmtk_dev.reset_dongle = 1;
		BTMTK_INFO("set reset_dongle %d", g_priv->btmtk_dev.reset_dongle);
		wake_up_interruptible(&g_priv->main_thread.wait_q);
	} else
		BTMTK_ERR("g_priv is NULL");
}

static int btmtk_sdio_wait_wlan_remove_thread(void *ptr)
{
	int  i = 0;

	BTMTK_INFO("begin");
	if (g_priv == NULL) {
		BTMTK_ERR("g_priv is NULL, return");
		return 0;
	}

	g_priv->btmtk_dev.reset_progress = 1;
	for (i = 0; i < 30; i++) {
		if ((wait_wlan_remove_tsk && kthread_should_stop()) || wlan_remove_done) {
			BTMTK_WARN("break wlan_remove_done %d", wlan_remove_done);
			break;
		}
		msleep(500);
	}

	btmtk_sdio_wakeup_mainthread_do_reset();

	while (!kthread_should_stop()) {
		BTMTK_INFO("no one call stop");
		msleep(500);
	}

	BTMTK_INFO("end");
	return 0;
}

static void btmtk_sdio_start_reset_dongle_progress(void)
{
	if (!g_card->bt_cfg.support_dongle_reset) {
		BTMTK_WARN("debug mode do not do reset");
	} else {
		BTMTK_WARN("user mode do reset");
		btmtk_sdio_notify_wlan_remove_start();
		btmtk_sdio_do_reset_or_wait_wlan_remove_done();
	}
}

/*============================================================================*/
/* Interface Functions : timer for uncomplete coredump */
/*============================================================================*/
static void btmtk_sdio_do_reset_or_wait_wlan_remove_done(void)
{
	BTMTK_INFO("wlan_remove_done %d", wlan_remove_done);
	if (wlan_remove_done || (wlan_status == WLAN_STATUS_IS_NOT_LOAD))
		/* wifi inform bt already, reset chip */
		btmtk_sdio_wakeup_mainthread_do_reset();
	else {
		/* makesure wait thread is stopped */
		btmtk_sdio_stop_wait_wlan_remove_tsk();
		/* create thread wait wifi inform bt */
		BTMTK_INFO("create btmtk_sdio_wait_wlan_remove_thread");
		wait_wlan_remove_tsk = kthread_run(
			btmtk_sdio_wait_wlan_remove_thread, NULL,
			"btmtk_sdio_wait_wlan_remove_thread");
		if (wait_wlan_remove_tsk == NULL)
			BTMTK_ERR("btmtk_sdio_wait_wlan_remove_thread create fail");
	}
}

static int btmtk_sdio_wait_dump_complete_thread(void *ptr)
{
	int  i = 0;

	BTMTK_INFO("begin");
	for (i = 0; i < 60; i++) {
		if (wait_dump_complete_tsk && kthread_should_stop()) {
			BTMTK_WARN("thread is stopped, break");
			break;
		}
		msleep(500);
	}

	if (!g_card->bt_cfg.support_dongle_reset) {
		BTMTK_INFO("debug mode don't do reset");
	} else {
		BTMTK_INFO("user mode call do reset");
		btmtk_sdio_do_reset_or_wait_wlan_remove_done();
	}

	if (i >= 60)
		BTMTK_WARN("wait dump complete timeout");
	wait_dump_complete_tsk = NULL;

	BTMTK_INFO("end");
	return 0;
}

static void btmtk_sdio_free_fw_cfg_struct(struct fw_cfg_struct *fw_cfg, int count)
{
	int i = 0;

	for (i = 0; i < count; i++) {
		if (fw_cfg[i].content) {
			BTMTK_INFO("kfree %d", i);
			kfree(fw_cfg[i].content);
			fw_cfg[i].content = NULL;
			fw_cfg[i].length = 0;
		} else
			fw_cfg[i].length = 0;
	}
}

static void btmtk_sdio_free_bt_cfg(void)
{
	BTMTK_INFO("begin");
	if (g_card == NULL) {
		BTMTK_ERR("g_card == NULL");
		return;
	}

	btmtk_sdio_free_fw_cfg_struct(&g_card->bt_cfg.picus_filter, 1);
	btmtk_sdio_free_fw_cfg_struct(g_card->bt_cfg.wmt_cmd, WMT_CMD_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->bt_cfg.vendor_cmd, VENDOR_CMD_COUNT);

	kfree(g_card->bt_cfg.sys_log_file_name);
	g_card->bt_cfg.sys_log_file_name = NULL;

	kfree(g_card->bt_cfg.fw_dump_file_name);
	g_card->bt_cfg.fw_dump_file_name = NULL;

	kfree(g_card->bt_cfg_file_name);
	g_card->bt_cfg_file_name = NULL;

	memset(&g_card->bt_cfg, 0, sizeof(g_card->bt_cfg));

	BTMTK_INFO("end");
}

static void btmtk_sdio_woble_free_setting(void)
{
	BTMTK_INFO("begin");
	if (g_card == NULL) {
		BTMTK_ERR("g_card == NULL");
		return;
	}

	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_apcf, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_apcf_fill_mac, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_apcf_fill_mac_location, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_radio_off, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_radio_off_status_event, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_radio_off_comp_event, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_radio_on, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_radio_on_status_event, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_radio_on_comp_event, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_apcf_resume, WOBLE_SETTING_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->woble_setting_apcf_resume_event, WOBLE_SETTING_COUNT);

	kfree(g_card->woble_setting_file_name);
	g_card->woble_setting_file_name = NULL;

	if (g_card->bt_cfg.support_woble_by_eint) {
		if (g_card->wobt_irq != 0 && atomic_read(&(g_card->irq_enable_count)) == 1) {
			BTMTK_INFO("disable BT IRQ:%d", g_card->wobt_irq);
			atomic_dec(&(g_card->irq_enable_count));
			disable_irq_nosync(g_card->wobt_irq);
		} else
			BTMTK_INFO("irq_enable count:%d", atomic_read(&(g_card->irq_enable_count)));
		free_irq(g_card->wobt_irq, g_card);
	}

	BTMTK_INFO("end");
}

static void btmtk_sdio_initialize_cfg_items(void)
{
	BTMTK_INFO("begin");
	if (g_card == NULL) {
		BTMTK_ERR("g_card == NULL");
		return;
	}

	g_card->bt_cfg.dongle_reset_gpio_pin = 0;
	g_card->bt_cfg.save_fw_dump_in_kernel = 0;
	g_card->bt_cfg.support_dongle_reset = 0;
	g_card->bt_cfg.support_full_fw_dump = 1;
	g_card->bt_cfg.support_legacy_woble = 0;
	g_card->bt_cfg.support_unify_woble = 0;
	g_card->bt_cfg.support_woble_by_eint = 0;
	g_card->bt_cfg.support_woble_for_bt_disable = 0;
	g_card->bt_cfg.support_woble_wakelock = 0;
	g_card->bt_cfg.reset_stack_after_woble = 0;
	g_card->bt_cfg.sys_log_file_name = NULL;
	g_card->bt_cfg.fw_dump_file_name = NULL;
	g_card->bt_cfg.support_auto_picus = 0;
	btmtk_sdio_free_fw_cfg_struct(&g_card->bt_cfg.picus_filter, 1);
	btmtk_sdio_free_fw_cfg_struct(g_card->bt_cfg.wmt_cmd, WMT_CMD_COUNT);
	btmtk_sdio_free_fw_cfg_struct(g_card->bt_cfg.vendor_cmd, VENDOR_CMD_COUNT);

	BTMTK_INFO("end");
}

static bool btmtk_sdio_parse_bt_cfg_file(char *item_name, char *text,
					char *searchcontent)
{
	bool ret = true;
	int temp_len = 0;
	char search[32];
	char *ptr = NULL, *p = NULL;
	char *temp = text;

	if (text == NULL) {
		BTMTK_ERR("text param is invalid!");
		ret = false;
		goto out;
	}

	memset(search, 0, sizeof(search));
	snprintf(search, sizeof(search), "%s", item_name); /* EX: SUPPORT_UNIFY_WOBLE */
	p = ptr = strstr(searchcontent, search);

	if (!ptr) {
		BTMTK_ERR("Can't find %s", item_name);
		ret = false;
		goto out;
	}

	if (p > searchcontent) {
		p--;
		while ((*p == ' ') && (p != searchcontent))
			p--;
		if (*p == '#') {
			BTMTK_ERR("It's invalid bt cfg item");
			ret = false;
			goto out;
		}
	}

	p = ptr + strlen(item_name) + 1;
	ptr = p;

	for (;;) {
		switch (*p) {
		case '\n':
			goto textdone;
		default:
			*temp++ = *p++;
			break;
		}
	}

textdone:
	temp_len = p - ptr;
	*temp = '\0';

out:
	return ret;
}

static void btmtk_sdio_bt_cfg_item_value_to_bool(char *item_value, bool *value)
{
	unsigned long text_value = 0;

	if (item_value == NULL) {
		BTMTK_ERR("item_value is NULL!");
		return;
	}

	if (kstrtoul(item_value, 10, &text_value) == 0) {
		if (text_value == 1)
			*value = true;
		else
			*value = false;
	} else {
		BTMTK_WARN("kstrtoul failed!");
	}
}

static int btmtk_sdio_load_fw_cfg_setting(char *block_name, struct fw_cfg_struct *save_content,
			int save_content_count, u8 *searchconetnt, enum fw_cfg_index_len index_length)
{
	int ret = 0;
	int i = 0;
	long parsing_result = 0;
	u8 *search_result = NULL;
	u8 *search_end = NULL;
	u8 search[32];
	u8 temp[260]; /* save for total hex number */
	u8 *next_number = NULL;
	u8 *next_block = NULL;
	u8 number[8];
	int temp_len;

	memset(search, 0, sizeof(search));
	memset(temp, 0, sizeof(temp));
	memset(number, 0, sizeof(number));

	/* search block name */
	for (i = 0; i < save_content_count; i++) {
		temp_len = 0;
		if (index_length == FW_CFG_INX_LEN_2) /* EX: APCF01 */
			snprintf(search, sizeof(search), "%s%02d:", block_name, i);
		else if (index_length == FW_CFG_INX_LEN_3) /* EX: APCF001 */
			snprintf(search, sizeof(search), "%s%03d:", block_name, i);
		else
			snprintf(search, sizeof(search), "%s:", block_name);
		search_result = strstr(searchconetnt, search);
		if (search_result) {
			memset(temp, 0, sizeof(temp));
			temp_len = 0;
			search_result += strlen(search); /* move to first number */

			do {
				next_number = NULL;
				search_end = strstr(search_result, ",");
				if ((search_end - search_result) <= 0) {
					BTMTK_INFO("can not find search end, break");
					break;
				}

				if ((search_end - search_result) > sizeof(number))
					break;

				memset(number, 0, sizeof(number));
				memcpy(number, search_result, search_end - search_result);

				if (number[0] == 0x20) /* space */
					ret = kstrtol(number + 1, 0, &parsing_result);
				else
					ret = kstrtol(number, 0, &parsing_result);

				if (ret == 0) {
					if (temp_len >= sizeof(temp)) {
						BTMTK_ERR("%s data over %zu", search, sizeof(temp));
						break;
					}
					temp[temp_len] = parsing_result;
					temp_len++;
					/* find next number */
					next_number = strstr(search_end, "0x");

					/* find next block */
					next_block = strstr(search_end, ":");
				} else {
					BTMTK_DBG("kstrtol ret = %d, search %s", ret, search);
					break;
				}

				if (next_number == NULL) {
					BTMTK_DBG("not find next apcf number temp_len %d, break, search %s",
						temp_len, search);
					break;
				}

				if ((next_number > next_block) && (next_block != 0)) {
					BTMTK_DBG("find next apcf number is over to next block ");
					BTMTK_DBG("temp_len %d, break, search %s",
						temp_len, search);
					break;
				}

				search_result = search_end + 1;
			} while (1);
		} else
			BTMTK_DBG("%s is not found", search);

		if (temp_len) {
			BTMTK_INFO("%s found", search);
			BTMTK_DBG("kzalloc i=%d temp_len=%d", i, temp_len);
			save_content[i].content = kzalloc(temp_len, GFP_KERNEL);
			memcpy(save_content[i].content, temp, temp_len);
			save_content[i].length = temp_len;
			BTMTK_DBG("x  save_content[%d].length %d temp_len=%d",
				i, save_content[i].length, temp_len);
		}

	}
	return ret;
}

static bool btmtk_sdio_load_bt_cfg_item(struct bt_cfg_struct *bt_cfg_content,
					char *searchcontent)
{
	bool ret = true;
	char text[128]; /* save for search text */
	unsigned long text_value = 0;

	memset(text, 0, sizeof(text));
	ret = btmtk_sdio_parse_bt_cfg_file(BT_UNIFY_WOBLE, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->support_unify_woble);
		BTMTK_INFO("bt_cfg_content->support_unify_woble = %d",
				bt_cfg_content->support_unify_woble);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_UNIFY_WOBLE);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_LEGACY_WOBLE, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->support_legacy_woble);
		BTMTK_INFO("bt_cfg_content->support_legacy_woble = %d",
			bt_cfg_content->support_legacy_woble);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_LEGACY_WOBLE);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_WOBLE_BY_EINT, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->support_woble_by_eint);
		BTMTK_INFO("bt_cfg_content->support_woble_by_eint = %d",
					bt_cfg_content->support_woble_by_eint);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_WOBLE_BY_EINT);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_DONGLE_RESET_PIN, text, searchcontent);
	if (ret) {
		if (kstrtoul(text, 10, &text_value) == 0)
			bt_cfg_content->dongle_reset_gpio_pin = text_value;
		else
			BTMTK_WARN("kstrtoul failed %s!", BT_DONGLE_RESET_PIN);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_DONGLE_RESET_PIN);
	}

	BTMTK_INFO("bt_cfg_content->dongle_reset_gpio_pin = %d",
			bt_cfg_content->dongle_reset_gpio_pin);

	ret = btmtk_sdio_parse_bt_cfg_file(BT_SAVE_FW_DUMP_IN_KERNEL, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->save_fw_dump_in_kernel);
		BTMTK_INFO("bt_cfg_content->save_fw_dump_in_kernel = %d",
				bt_cfg_content->save_fw_dump_in_kernel);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_SAVE_FW_DUMP_IN_KERNEL);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_RESET_DONGLE, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->support_dongle_reset);
		BTMTK_INFO("bt_cfg_content->support_dongle_reset = %d",
				bt_cfg_content->support_dongle_reset);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_RESET_DONGLE);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_FULL_FW_DUMP, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->support_full_fw_dump);
		BTMTK_INFO("bt_cfg_content->support_full_fw_dump = %d",
				bt_cfg_content->support_full_fw_dump);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_FULL_FW_DUMP);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_WOBLE_WAKELOCK, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->support_woble_wakelock);
		BTMTK_INFO("bt_cfg_content->support_woble_wakelock = %d",
				bt_cfg_content->support_woble_wakelock);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_WOBLE_WAKELOCK);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_WOBLE_FOR_BT_DISABLE, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->support_woble_for_bt_disable);
		BTMTK_INFO("bt_cfg_content->support_woble_for_bt_disable = %d",
				bt_cfg_content->support_woble_for_bt_disable);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_WOBLE_FOR_BT_DISABLE);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_RESET_STACK_AFTER_WOBLE, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->reset_stack_after_woble);
		BTMTK_INFO("%s: bt_cfg_content->reset_stack_after_woble = %d", __func__,
				bt_cfg_content->reset_stack_after_woble);
	} else {
		BTMTK_WARN("%s: search item %s is invalid!", __func__, BT_RESET_STACK_AFTER_WOBLE);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_SYS_LOG_FILE, text, searchcontent);
	if (ret) {
		if (bt_cfg_content->sys_log_file_name != NULL) {
			kfree(bt_cfg_content->sys_log_file_name);
			bt_cfg_content->sys_log_file_name = NULL;
		}
		bt_cfg_content->sys_log_file_name = kzalloc(strlen(text) + 1, GFP_KERNEL);
		if (bt_cfg_content->sys_log_file_name == NULL) {
			ret = false;
			return ret;
		}
		memcpy(bt_cfg_content->sys_log_file_name, text, strlen(text));
		bt_cfg_content->sys_log_file_name[strlen(text)] = '\0';
		BTMTK_INFO("bt_cfg_content->sys_log_file_name = %s",
				bt_cfg_content->sys_log_file_name);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_SYS_LOG_FILE);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_FW_DUMP_FILE, text, searchcontent);
	if (ret) {
		if (bt_cfg_content->fw_dump_file_name != NULL) {
			kfree(bt_cfg_content->fw_dump_file_name);
			bt_cfg_content->fw_dump_file_name = NULL;
		}
		bt_cfg_content->fw_dump_file_name = kzalloc(strlen(text) + 1, GFP_KERNEL);
		if (bt_cfg_content->fw_dump_file_name == NULL) {
			ret = false;
			return ret;
		}
		memcpy(bt_cfg_content->fw_dump_file_name, text, strlen(text));
		bt_cfg_content->fw_dump_file_name[strlen(text)] = '\0';
		BTMTK_INFO("bt_cfg_content->fw_dump_file_name = %s",
				bt_cfg_content->fw_dump_file_name);
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_FW_DUMP_FILE);
	}

	ret = btmtk_sdio_parse_bt_cfg_file(BT_AUTO_PICUS, text, searchcontent);
	if (ret) {
		btmtk_sdio_bt_cfg_item_value_to_bool(text, &bt_cfg_content->support_auto_picus);
		BTMTK_INFO("bt_cfg_content->support_auto_picus = %d",
				bt_cfg_content->support_auto_picus);
		if (bt_cfg_content->support_auto_picus == true) {
			ret = btmtk_sdio_load_fw_cfg_setting(BT_AUTO_PICUS_FILTER,
					&bt_cfg_content->picus_filter, 1, searchcontent, FW_CFG_INX_LEN_NONE);
			if (ret)
				BTMTK_WARN("search item %s is invalid!", BT_AUTO_PICUS_FILTER);
		}
	} else {
		BTMTK_WARN("search item %s is invalid!", BT_AUTO_PICUS);
	}

	ret = btmtk_sdio_load_fw_cfg_setting(BT_WMT_CMD, bt_cfg_content->wmt_cmd,
				WMT_CMD_COUNT, searchcontent, FW_CFG_INX_LEN_3);
	if (ret)
		BTMTK_WARN("search item %s is invalid!", BT_WMT_CMD);

	ret = btmtk_sdio_load_fw_cfg_setting(BT_VENDOR_CMD, bt_cfg_content->vendor_cmd,
				VENDOR_CMD_COUNT, searchcontent, FW_CFG_INX_LEN_3);
	if (ret)
		BTMTK_WARN("search item %s is invalid!", BT_VENDOR_CMD);

	/* release setting file memory */
	if (g_card) {
		kfree(g_card->setting_file);
		g_card->setting_file = NULL;
	}
	return ret;
}

static void btmtk_sdio_load_woble_setting_callback(const struct firmware *fw_data, void *context)
{
	struct btmtk_sdio_card *card = (struct btmtk_sdio_card *)context;
	int err = 0;
	unsigned char *image = NULL;

	if (!fw_data) {
		BTMTK_ERR("fw_data is NULL callback request_firmware fail or can't find file!!");

		/* Request original woble_setting.bin */
		memcpy(g_card->woble_setting_file_name,
				WOBLE_SETTING_FILE_NAME,
				sizeof(WOBLE_SETTING_FILE_NAME));
		BTMTK_INFO("begin load orignial woble_setting_file_name = %s",
				g_card->woble_setting_file_name);
		if (need_retry_load_woble < BTMTK_LOAD_WOBLE_RETRY_COUNT) {
			need_retry_load_woble++;
			err = request_firmware_nowait(THIS_MODULE, true, g_card->woble_setting_file_name,
				&g_card->func->dev, GFP_KERNEL, g_card, btmtk_sdio_load_woble_setting_callback);
			if (err < 0) {
				BTMTK_ERR("request %s file fail(%d)",
					g_card->woble_setting_file_name, err);
			}
		} else {
			BTMTK_ERR("request %s file fail(%d), need_load_origin_woble = %d",
				g_card->woble_setting_file_name, err, need_retry_load_woble);
		}
		return;
	}

	BTMTK_INFO("woble_setting callback request_firmware size %zu success", fw_data->size);
	image = kzalloc(fw_data->size + 1, GFP_KERNEL);
	if (image == NULL) {
		BTMTK_ERR("kzalloc size %zu failed!!", fw_data->size);
		goto LOAD_END;
	}

	memcpy(image, fw_data->data, fw_data->size);
	image[fw_data->size] = '\0';

	err = btmtk_sdio_load_fw_cfg_setting("APCF",
			card->woble_setting_apcf, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("APCF_ADD_MAC",
			card->woble_setting_apcf_fill_mac, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("APCF_ADD_MAC_LOCATION",
			card->woble_setting_apcf_fill_mac_location, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("RADIOOFF",
			card->woble_setting_radio_off, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("RADIOOFF_STATUS_EVENT",
			card->woble_setting_radio_off_status_event, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("RADIOOFF_COMPLETE_EVENT",
			card->woble_setting_radio_off_comp_event, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("RADIOON",
			card->woble_setting_radio_on, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("RADIOON_STATUS_EVENT",
			card->woble_setting_radio_on_status_event, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("RADIOON_COMPLETE_EVENT",
			card->woble_setting_radio_on_comp_event, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("APCF_RESMUE",
		card->woble_setting_apcf_resume, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);
	if (err)
		goto LOAD_END;

	err = btmtk_sdio_load_fw_cfg_setting("APCF_COMPLETE_EVENT",
			card->woble_setting_apcf_resume_event, WOBLE_SETTING_COUNT, image, FW_CFG_INX_LEN_2);

LOAD_END:
	if (image) {
		kfree(image);
		image = NULL;
	}
	release_firmware(fw_data);
	if (err)
		BTMTK_WARN("result fail");
	else
		BTMTK_INFO("result success");
}

static int btmtk_sdio_load_code_from_setting_files(char *setting_file_name,
				struct btmtk_sdio_card *data, u32 *code_len)
{
	int err = 0;
	const struct firmware *fw_entry = NULL;
	*code_len = 0;

	if (data == NULL || setting_file_name == NULL) {
		BTMTK_ERR("invalid parameter!!");
		err = -1;
		return err;
	}

	err = request_firmware(&fw_entry, setting_file_name, &data->func->dev);
	if (err != 0) {
		BTMTK_ERR("request %s file fail(%d)", setting_file_name, err);
		return err;
	}

	if (data->setting_file != NULL) {
		kfree(data->setting_file);
		data->setting_file = NULL;
	}

	if (fw_entry) {
		/* alloc setting file memory */
		data->setting_file = kzalloc(fw_entry->size + 1, GFP_KERNEL);
		BTMTK_INFO("setting file request_firmware size %zu success", fw_entry->size);
	} else {
		BTMTK_ERR("fw_entry is NULL request_firmware may fail!! error code = %d", err);
		return err;
	}

	if (data->setting_file == NULL) {
		BTMTK_ERR("kzalloc size %zu failed!!", fw_entry->size);
		release_firmware(fw_entry);
		err = -1;
		return err;
	}

	memcpy(data->setting_file, fw_entry->data, fw_entry->size);
	data->setting_file[fw_entry->size] = '\0';

	*code_len = fw_entry->size;
	release_firmware(fw_entry);

	BTMTK_INFO("cfg_file len (%d) assign done", *code_len);
	return err;
}

static int btmtk_sdio_load_setting_files(char *bin_name, struct device *dev,
					struct btmtk_sdio_card *data)
{
	int err = 0;
	char *ptr_name = NULL;
	u32 code_len = 0;

	BTMTK_INFO("begin setting_file_name = %s", bin_name);
	ptr_name = strstr(bin_name, "woble_setting");
	if (ptr_name) {
		err = request_firmware_nowait(THIS_MODULE, true, bin_name,
			&data->func->dev, GFP_KERNEL, data, btmtk_sdio_load_woble_setting_callback);

		if (err < 0)
			BTMTK_ERR("request %s file fail(%d)", bin_name, err);
		else
			BTMTK_INFO("request %s file success(%d)", bin_name, err);
	} else if (strcmp(bin_name, BT_CFG_NAME) == 0) {
		err = btmtk_sdio_load_code_from_setting_files(bin_name, data, &code_len);
		if (err != 0) {
			BTMTK_ERR("btmtk_sdio_load_code_from_cfg_files failed!!");
			return err;
		}

		if (btmtk_sdio_load_bt_cfg_item(&data->bt_cfg, data->setting_file)) {
			BTMTK_ERR("btmtk_sdio_load_bt_cfg_item error!!");
			err = -1;
			return err;
		}
	} else
		BTMTK_WARN("bin_name is not defined");

	return err;
}

static inline void btmtk_sdio_woble_wake_lock(struct btmtk_sdio_card *data)
{
	if (!data->woble_ws)
		return;

	if (data->bt_cfg.support_unify_woble && data->bt_cfg.support_woble_wakelock) {
		BTMTK_INFO("wake lock");
		__pm_stay_awake(data->woble_ws);
	}
}

static inline void btmtk_sdio_woble_wake_unlock(struct btmtk_sdio_card *data)
{
	if (!data->woble_ws)
		return;

	if (data->bt_cfg.support_unify_woble && data->bt_cfg.support_woble_wakelock) {
		BTMTK_INFO("wake unlock");
		__pm_relax(data->woble_ws);
	}
}

u32 LOCK_UNSLEEPABLE_LOCK(struct _OSAL_UNSLEEPABLE_LOCK_ *pUSL)
{
	spin_lock_irqsave(&(pUSL->lock), pUSL->flag);
	return 0;
}

u32 UNLOCK_UNSLEEPABLE_LOCK(struct _OSAL_UNSLEEPABLE_LOCK_ *pUSL)
{
	spin_unlock_irqrestore(&(pUSL->lock), pUSL->flag);
	return 0;
}

static int btmtk_sdio_writesb(u32 offset, u8 *val, int len)
{
	u32 ret = 0;
	u32 retry_count = 0;

	if (g_card->func == NULL) {
		BTMTK_ERR("g_card->func is NULL");
		return -EIO;
	}

	do {
		sdio_claim_host(g_card->func);
		ret = sdio_writesb(g_card->func, offset, val, len);
		sdio_release_host(g_card->func);
		retry_count++;
		if (retry_count > BTMTK_SDIO_RETRY_COUNT) {
			BTMTK_ERR("ret:%d", ret);
			break;
		}
	} while (ret);

	return ret;
}


static int btmtk_sdio_readsb(u32 offset, u8 *val, int len)
{
	u32 ret = 0;
	u32 retry_count = 0;

	if (g_card->func == NULL) {
		BTMTK_ERR("g_card->func is NULL");
		return -EIO;
	}

	do {
		sdio_claim_host(g_card->func);
		ret = sdio_readsb(g_card->func, val, offset, len);
		sdio_release_host(g_card->func);
		retry_count++;
		if (retry_count > BTMTK_SDIO_RETRY_COUNT) {
			BTMTK_ERR("ret:%d", ret);
			break;
		}
	} while (ret);

	return ret;
}

static int btmtk_sdio_writeb(u32 offset, u8 val)
{
	u32 ret = 0;
	u32 retry_count = 0;

	if (g_card->func == NULL) {
		BTMTK_ERR("g_card->func is NULL");
		return -EIO;
	}

	do {
		sdio_claim_host(g_card->func);
		sdio_writeb(g_card->func, val, offset, &ret);
		sdio_release_host(g_card->func);
		retry_count++;
		if (retry_count > BTMTK_SDIO_RETRY_COUNT) {
			BTMTK_ERR("ret:%d", ret);
			break;
		}
	} while (ret);

	return ret;
}

static int btmtk_sdio_readb(u32 offset, u8 *val)
{
	u32 ret = 0;
	u32 retry_count = 0;

	if (g_card->func == NULL) {
		BTMTK_ERR("g_card->func is NULL");
		return -EIO;
	}

	do {
		sdio_claim_host(g_card->func);
		*val = sdio_readb(g_card->func, offset, &ret);
		sdio_release_host(g_card->func);
		retry_count++;
		if (retry_count > BTMTK_SDIO_RETRY_COUNT) {
			BTMTK_ERR("ret:%d", ret);
			break;
		}
	} while (ret);

	return ret;
}

static int btmtk_sdio_writel(u32 offset, u32 val)
{
	u32 ret = 0;
	u32 retry_count = 0;

	if (g_card->func == NULL) {
		BTMTK_ERR("g_card->func is NULL");
		return -EIO;
	}

	do {
		sdio_claim_host(g_card->func);
		sdio_writel(g_card->func, val, offset, &ret);
		sdio_release_host(g_card->func);
		retry_count++;
		if (retry_count > BTMTK_SDIO_RETRY_COUNT) {
			BTMTK_ERR("ret:%d", ret);
			break;
		}
	} while (ret);

	return ret;
}

static int btmtk_sdio_readl(u32 offset,  u32 *val)
{
	u32 ret = 0;
	u32 retry_count = 0;

	if (g_card->func == NULL) {
		BTMTK_ERR("g_card->func is NULL");
		return -EIO;
	}

	do {
		sdio_claim_host(g_card->func);
		*val = sdio_readl(g_card->func, offset, &ret);
		sdio_release_host(g_card->func);
		retry_count++;
		if (retry_count > BTMTK_SDIO_RETRY_COUNT) {
			BTMTK_ERR("ret:%d", ret);
			break;
		}
	} while (ret);

	return ret;
}

static void btmtk_sdio_print_debug_sr(void)
{
	u32 ret = 0;
	u32 CCIR_Value = 0;
	u32 CHLPCR_Value = 0;
	u32 CSDIOCSR_Value = 0;
	u32 CHISR_Value = 0;
	u32 CHIER_Value = 0;
	u32 CTFSR_Value = 0;
	u32 CRPLR_Value = 0;
	u32 SWPCDBGR_Value = 0;
	unsigned char X0_Value = 0;
	unsigned char F8_Value = 0;
	unsigned char F9_Value = 0;
	unsigned char FA_Value = 0;
	unsigned char FB_Value = 0;
	unsigned char FC_Value = 0;
	unsigned char FD_Value = 0;
	unsigned char FE_Value = 0;
	unsigned char FF_Value = 0;

	ret = btmtk_sdio_readl(CCIR, &CCIR_Value);
	ret = btmtk_sdio_readl(CHLPCR, &CHLPCR_Value);
	ret = btmtk_sdio_readl(CSDIOCSR, &CSDIOCSR_Value);
	ret = btmtk_sdio_readl(CHISR, &CHISR_Value);
	ret = btmtk_sdio_readl(CHIER, &CHIER_Value);
	ret = btmtk_sdio_readl(CTFSR, &CTFSR_Value);
	ret = btmtk_sdio_readl(CRPLR, &CRPLR_Value);
	ret = btmtk_sdio_readl(SWPCDBGR, &SWPCDBGR_Value);
	sdio_claim_host(g_card->func);
	X0_Value = sdio_f0_readb(g_card->func, 0x00, &ret);
	F8_Value = sdio_f0_readb(g_card->func, 0xF8, &ret);
	F9_Value = sdio_f0_readb(g_card->func, 0xF9, &ret);
	FA_Value = sdio_f0_readb(g_card->func, 0xFA, &ret);
	FB_Value = sdio_f0_readb(g_card->func, 0xFB, &ret);
	FC_Value = sdio_f0_readb(g_card->func, 0xFC, &ret);
	FD_Value = sdio_f0_readb(g_card->func, 0xFD, &ret);
	FE_Value = sdio_f0_readb(g_card->func, 0xFE, &ret);
	FF_Value = sdio_f0_readb(g_card->func, 0xFF, &ret);
	sdio_release_host(g_card->func);
	BTMTK_INFO("CCIR: 0x%x, CHLPCR: 0x%x, CSDIOCSR: 0x%x, CHISR: 0x%x",
		CCIR_Value, CHLPCR_Value, CSDIOCSR_Value, CHISR_Value);
	BTMTK_INFO("CHIER: 0x%x, CTFSR: 0x%x, CRPLR: 0x%x, SWPCDBGR: 0x%x",
		CHIER_Value, CTFSR_Value, CRPLR_Value, SWPCDBGR_Value);
	BTMTK_INFO("CCCR 00: 0x%x, F8: 0x%x, F9: 0x%x, FA: 0x%x, FB: 0x%x",
		X0_Value, F8_Value, F9_Value, FA_Value, FB_Value);
	BTMTK_INFO("FC: 0x%x, FD: 0x%x, FE: 0x%x, FE: 0x%x",
		FC_Value, FD_Value, FE_Value, FF_Value);
}


static void btmtk_sdio_hci_snoop_save(u8 type, u8 *buf, u32 len)
{
	u32 copy_len = HCI_SNOOP_BUF_SIZE;

	if (buf == NULL || len == 0)
		return;

	if (len < HCI_SNOOP_BUF_SIZE)
		copy_len = len;

	switch (type) {
	case HCI_COMMAND_PKT:
		hci_cmd_snoop_len[hci_cmd_snoop_index] = copy_len & 0xff;
		memset(hci_cmd_snoop_buf[hci_cmd_snoop_index], 0, HCI_SNOOP_BUF_SIZE);
		memcpy(hci_cmd_snoop_buf[hci_cmd_snoop_index], buf, copy_len & 0xff);
		hci_cmd_snoop_timestamp[hci_cmd_snoop_index] = btmtk_sdio_get_microseconds();

		hci_cmd_snoop_index--;
		if (hci_cmd_snoop_index < 0)
			hci_cmd_snoop_index = HCI_SNOOP_ENTRY_NUM - 1;
		break;
	case HCI_ACLDATA_PKT:
		hci_acl_snoop_len[hci_acl_snoop_index] = copy_len & 0xff;
		memset(hci_acl_snoop_buf[hci_acl_snoop_index], 0, HCI_SNOOP_BUF_SIZE);
		memcpy(hci_acl_snoop_buf[hci_acl_snoop_index], buf, copy_len & 0xff);
		hci_acl_snoop_timestamp[hci_acl_snoop_index] = btmtk_sdio_get_microseconds();

		hci_acl_snoop_index--;
		if (hci_acl_snoop_index < 0)
			hci_acl_snoop_index = HCI_SNOOP_ENTRY_NUM - 1;
		break;
	case HCI_EVENT_PKT:
		if (buf[0] == 0x3E) /* Not save BLE Event */
			break;
		hci_event_snoop_len[hci_event_snoop_index] = copy_len;
		memset(hci_event_snoop_buf[hci_event_snoop_index], 0,
			HCI_SNOOP_BUF_SIZE);
		memcpy(hci_event_snoop_buf[hci_event_snoop_index], buf, copy_len);
		hci_event_snoop_timestamp[hci_event_snoop_index] = btmtk_sdio_get_microseconds();

		hci_event_snoop_index--;
		if (hci_event_snoop_index < 0)
			hci_event_snoop_index = HCI_SNOOP_ENTRY_NUM - 1;
		break;
	case FW_LOG_PKT:
		fw_log_snoop_len[fw_log_snoop_index] = copy_len;
		memset(fw_log_snoop_buf[fw_log_snoop_index], 0,
			HCI_SNOOP_BUF_SIZE);
		memcpy(fw_log_snoop_buf[fw_log_snoop_index], buf, copy_len);
		fw_log_snoop_timestamp[fw_log_snoop_index] = btmtk_sdio_get_microseconds();

		fw_log_snoop_index--;
		if (fw_log_snoop_index < 0)
			fw_log_snoop_index = HCI_SNOOP_ENTRY_NUM - 1;
		break;
	default:
		BTSDIO_INFO_RAW(buf, len,
			"type(%d):", type);
	}
}

static void btmtk_sdio_hci_snoop_print(void)
{
	int counter, index;

	BTMTK_INFO("HCI Command Dump");
	index = hci_cmd_snoop_index + 1;
	if (index >= HCI_SNOOP_ENTRY_NUM)
		index = 0;
	for (counter = 0; counter < HCI_SNOOP_ENTRY_NUM; counter++) {
		if (hci_cmd_snoop_len[index] != 0)
			BTSDIO_INFO_RAW(hci_cmd_snoop_buf[index], hci_cmd_snoop_len[index],
				"	time(%u):", hci_cmd_snoop_timestamp[index]);
		index++;
		if (index >= HCI_SNOOP_ENTRY_NUM)
			index = 0;
	}

	BTMTK_INFO("HCI Event Dump");
	index = hci_event_snoop_index + 1;
	if (index >= HCI_SNOOP_ENTRY_NUM)
		index = 0;
	for (counter = 0; counter < HCI_SNOOP_ENTRY_NUM; counter++) {
		if (hci_event_snoop_len[index] != 0)
			BTSDIO_INFO_RAW(hci_event_snoop_buf[index], hci_event_snoop_len[index],
				"	time(%u):", hci_event_snoop_timestamp[index]);
		index++;
		if (index >= HCI_SNOOP_ENTRY_NUM)
			index = 0;
	}

	BTMTK_INFO("HCI ACL Dump");
	index = hci_acl_snoop_index + 1;
	if (index >= HCI_SNOOP_ENTRY_NUM)
		index = 0;
	for (counter = 0; counter < HCI_SNOOP_ENTRY_NUM; counter++) {
		if (hci_acl_snoop_len[index] != 0)
			BTSDIO_INFO_RAW(hci_acl_snoop_buf[index], hci_acl_snoop_len[index],
				"	time(%u):", hci_acl_snoop_timestamp[index]);
		index++;
		if (index >= HCI_SNOOP_ENTRY_NUM)
			index = 0;
	}

	BTMTK_INFO("FW LOG Dump");
	index = fw_log_snoop_index + 1;
	if (index >= HCI_SNOOP_ENTRY_NUM)
		index = 0;
	for (counter = 0; counter < HCI_SNOOP_ENTRY_NUM; counter++) {
		if (fw_log_snoop_len[index] != 0)
			BTSDIO_INFO_RAW(fw_log_snoop_buf[index], fw_log_snoop_len[index],
				"	time(%u):", fw_log_snoop_timestamp[index]);
		index++;
		if (index >= HCI_SNOOP_ENTRY_NUM)
			index = 0;
	}
}

static void btmtk_sdio_hci_snoop_init(void)
{
	hci_cmd_snoop_index = HCI_SNOOP_ENTRY_NUM - 1;
	hci_event_snoop_index = HCI_SNOOP_ENTRY_NUM - 1;
	hci_acl_snoop_index = HCI_SNOOP_ENTRY_NUM - 1;
	fw_log_snoop_index = HCI_SNOOP_ENTRY_NUM - 1;

	memset(hci_cmd_snoop_buf, 0, HCI_SNOOP_ENTRY_NUM * HCI_SNOOP_BUF_SIZE * sizeof(u8));
	memset(hci_cmd_snoop_len, 0, HCI_SNOOP_ENTRY_NUM * sizeof(u8));
	memset(hci_cmd_snoop_timestamp, 0, HCI_SNOOP_ENTRY_NUM * sizeof(unsigned int));

	memset(hci_event_snoop_buf, 0, HCI_SNOOP_ENTRY_NUM * HCI_SNOOP_BUF_SIZE * sizeof(u8));
	memset(hci_event_snoop_len, 0, HCI_SNOOP_ENTRY_NUM * sizeof(u8));
	memset(hci_event_snoop_timestamp, 0, HCI_SNOOP_ENTRY_NUM * sizeof(unsigned int));

	memset(hci_acl_snoop_buf, 0, HCI_SNOOP_ENTRY_NUM * HCI_SNOOP_BUF_SIZE * sizeof(u8));
	memset(hci_acl_snoop_len, 0, HCI_SNOOP_ENTRY_NUM * sizeof(u8));
	memset(hci_acl_snoop_timestamp, 0, HCI_SNOOP_ENTRY_NUM * sizeof(unsigned int));

	memset(fw_log_snoop_buf, 0, HCI_SNOOP_ENTRY_NUM * HCI_SNOOP_BUF_SIZE * sizeof(u8));
	memset(fw_log_snoop_len, 0, HCI_SNOOP_ENTRY_NUM * sizeof(u8));
	memset(fw_log_snoop_timestamp, 0, HCI_SNOOP_ENTRY_NUM * sizeof(unsigned int));
}

struct sk_buff *btmtk_create_send_data(struct sk_buff *skb)
{
	struct sk_buff *queue_skb = NULL;
	u32 sdio_header_len = skb->len + BTM_HEADER_LEN;

	if (skb_headroom(skb) < (BTM_HEADER_LEN)) {
		queue_skb = bt_skb_alloc(sdio_header_len, GFP_ATOMIC);
		if (queue_skb == NULL) {
			BTMTK_ERR("bt_skb_alloc fail return");
			return 0;
		}

		queue_skb->data[0] = (sdio_header_len & 0x0000ff);
		queue_skb->data[1] = (sdio_header_len & 0x00ff00) >> 8;
		queue_skb->data[2] = 0;
		queue_skb->data[3] = 0;
		queue_skb->data[4] = bt_cb(skb)->pkt_type;
		queue_skb->len = sdio_header_len;
		memcpy(&queue_skb->data[5], &skb->data[0], skb->len);
		kfree_skb(skb);
	} else {
		queue_skb = skb;
		skb_push(queue_skb, BTM_HEADER_LEN);
		queue_skb->data[0] = (sdio_header_len & 0x0000ff);
		queue_skb->data[1] = (sdio_header_len & 0x00ff00) >> 8;
		queue_skb->data[2] = 0;
		queue_skb->data[3] = 0;
		queue_skb->data[4] = bt_cb(skb)->pkt_type;
	}

	BTMTK_INFO("end");
	return queue_skb;
}

static void btmtk_sdio_set_no_fw_own(struct btmtk_private *priv, bool no_fw_own)
{
	if (priv) {
		priv->no_fw_own = no_fw_own;
		BTMTK_DBG("set no_fw_own %d", priv->no_fw_own);
	} else
		BTMTK_DBG("priv is NULL");
}

static int btmtk_sdio_set_own_back(int owntype)
{
	/*Set driver own*/
	int ret = 0, retry_ret = 0;
	u32 u32LoopCount = 0;
	u32 u32ReadCRValue = 0;
	u32 ownValue = 0;
	u32 set_checkretry = 30;
	int i = 0;

	BTMTK_DBG("owntype %d", owntype);

	if (user_rmmod)
		set_checkretry = 1;


	if (owntype == FW_OWN && (g_priv)) {
		if (g_priv->no_fw_own) {
			ret = btmtk_sdio_readl(SWPCDBGR, &u32ReadCRValue);
			BTMTK_DBG("no_fw_own is on, just return, u32ReadCRValue = 0x%08X, ret = %d",
				u32ReadCRValue, ret);
			return ret;
		}
	}

	ret = btmtk_sdio_readl(CHLPCR, &u32ReadCRValue);

	BTMTK_DBG("btmtk_sdio_readl  CHLPCR done");
	if (owntype == DRIVER_OWN) {
		if ((u32ReadCRValue&0x100) == 0x100) {
			BTMTK_DBG("already driver own 0x%0x, return", u32ReadCRValue);
			goto set_own_end;
		}
	} else if (owntype == FW_OWN) {
		if ((u32ReadCRValue&0x100) == 0) {
			BTMTK_DBG("already FW own 0x%0x, return", u32ReadCRValue);
			goto set_own_end;
		}
	}

setretry:

	if (owntype == DRIVER_OWN)
		ownValue = 0x00000200;
	else
		ownValue = 0x00000100;

	BTMTK_DBG("write CHLPCR 0x%x", ownValue);
	ret = btmtk_sdio_writel(CHLPCR, ownValue);
	if (ret) {
		ret = -EINVAL;
		goto done;
	}
	BTMTK_DBG("write CHLPCR 0x%x done", ownValue);

	u32LoopCount = 1000;

	if (owntype == DRIVER_OWN) {
		do {
			usleep_range(100, 200);
			ret = btmtk_sdio_readl(CHLPCR, &u32ReadCRValue);
			u32LoopCount--;
			BTMTK_DBG("DRIVER_OWN btmtk_sdio_readl CHLPCR 0x%x", u32ReadCRValue);
		} while ((u32LoopCount > 0) &&
			((u32ReadCRValue&0x100) != 0x100));

		if ((u32LoopCount == 0) && (0x100 != (u32ReadCRValue&0x100))
				&& (set_checkretry > 0)) {
			BTMTK_WARN("retry set_check driver own, CHLPCR 0x%x", u32ReadCRValue);
			for (i = 0; i < 3; i++) {
				ret = btmtk_sdio_readl(SWPCDBGR, &u32ReadCRValue);
				BTMTK_WARN("ret %d,SWPCDBGR 0x%x, and not sleep!", ret, u32ReadCRValue);
			}
			btmtk_sdio_print_debug_sr();

			set_checkretry--;
			mdelay(20);
			goto setretry;
		}
	} else {
		do {
			usleep_range(100, 200);
			ret = btmtk_sdio_readl(CHLPCR, &u32ReadCRValue);
			u32LoopCount--;
			BTMTK_DBG("FW_OWN btmtk_sdio_readl CHLPCR 0x%x", u32ReadCRValue);
		} while ((u32LoopCount > 0) && ((u32ReadCRValue&0x100) != 0));

		if ((u32LoopCount == 0) &&
				((u32ReadCRValue&0x100) != 0) &&
				(set_checkretry > 0)) {
			BTMTK_WARN("retry set_check FW own, CHLPCR 0x%x", u32ReadCRValue);
			set_checkretry--;
			goto setretry;
		}
	}

	BTMTK_DBG("CHLPCR(0x%x), is 0x%x", CHLPCR, u32ReadCRValue);

	if (owntype == DRIVER_OWN) {
		if ((u32ReadCRValue&0x100) == 0x100)
			BTMTK_DBG("check %04x, is 0x100 driver own success", CHLPCR);
		else {
			BTMTK_DBG("check %04x, is %x shuld be 0x100", CHLPCR, u32ReadCRValue);
			ret = -EINVAL;
			goto done;
		}
	} else {
		if (0x0 == (u32ReadCRValue&0x100))
			BTMTK_DBG("check %04x, bit 8 is 0 FW own success", CHLPCR);
		else{
			BTMTK_DBG("bit 8 should be 0, %04x bit 8 is %04x", u32ReadCRValue,
				(u32ReadCRValue&0x100));
			ret = -EINVAL;
			goto done;
		}
	}

done:
	if (owntype == DRIVER_OWN) {
		if (ret) {
			BTMTK_ERR("set driver own fail");
			for (i = 0; i < 8; i++) {
				retry_ret = btmtk_sdio_readl(SWPCDBGR, &u32ReadCRValue);
				BTMTK_ERR("ret %d,SWPCDBGR 0x%x, then sleep 200ms", retry_ret, u32ReadCRValue);
				msleep(200);
			}
		} else
			BTMTK_DBG("set driver own success");
	} else if (owntype == FW_OWN) {
		if (ret)
			BTMTK_ERR("set FW own fail");
		else
			BTMTK_DBG("set FW own success");
	} else
		BTMTK_ERR("unknown type %d", owntype);

set_own_end:
	if (ret)
		btmtk_sdio_print_debug_sr();

	return ret;
}

static int btmtk_sdio_enable_interrupt(int enable)
{
	u32 ret = 0;
	u32 cr_value = 0;

	if (enable)
		cr_value |= C_FW_INT_EN_SET;
	else
		cr_value |= C_FW_INT_EN_CLEAR;

	ret = btmtk_sdio_writel(CHLPCR, cr_value);
	BTMTK_DBG("enable %d write CHLPCR 0x%08x", enable, cr_value);

	return ret;
}

static int btmtk_sdio_get_rx_unit(struct btmtk_sdio_card *card)
{
	u8 reg;
	int ret;

	ret = btmtk_sdio_readb(card->reg->card_rx_unit, &reg);
	if (!ret)
		card->rx_unit = reg;

	return ret;
}

static int btmtk_sdio_enable_host_int_mask(
				struct btmtk_sdio_card *card,
				u8 mask)
{
	int ret;

	ret = btmtk_sdio_writeb(card->reg->host_int_mask, mask);
	if (ret) {
		BTMTK_ERR("Unable to enable the host interrupt!");
		ret = -EIO;
	}

	return ret;
}

static int btmtk_sdio_disable_host_int_mask(
				struct btmtk_sdio_card *card,
				u8 mask)
{
	u8 host_int_mask;
	int ret;

	ret = btmtk_sdio_readb(card->reg->host_int_mask, &host_int_mask);
	if (ret)
		return -EIO;

	host_int_mask &= ~mask;

	ret = btmtk_sdio_writeb(card->reg->host_int_mask, host_int_mask);
	if (ret < 0) {
		BTMTK_ERR("Unable to disable the host interrupt!");
		return -EIO;
	}

	return 0;
}

/*for debug*/
int btmtk_print_buffer_conent(u8 *buf, u32 Datalen)
{
	int i = 0;
	int print_finish = 0;
	/*Print out txbuf data for debug*/
	for (i = 0; i <= (Datalen-1); i += 16) {
		if ((i+16) <= Datalen) {
			BTMTK_DBG("%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X %02X%02X%02X%02X%02X %02X",
				buf[i], buf[i+1], buf[i+2], buf[i+3],
				buf[i+4], buf[i+5], buf[i+6], buf[i+7],
				buf[i+8], buf[i+9], buf[i+10], buf[i+11],
				buf[i+12], buf[i+13], buf[i+14], buf[i+15]);
		} else {
			for (; i < (Datalen); i++)
				BTMTK_DBG("%02X", buf[i]);

			print_finish = 1;
		}

		if (print_finish)
			break;
	}
	return 0;
}

static int btmtk_sdio_send_tx_data(u8 *buffer, int tx_data_len)
{
	int ret = 0;
	u8 MultiBluckCount = 0;
	u8 redundant = 0;

	MultiBluckCount = tx_data_len/SDIO_BLOCK_SIZE;
	redundant = tx_data_len % SDIO_BLOCK_SIZE;

	if (redundant)
		tx_data_len = (MultiBluckCount+1)*SDIO_BLOCK_SIZE;

	ret = btmtk_sdio_writesb(CTDR, buffer, tx_data_len);
	return ret;
}

static int btmtk_sdio_recv_rx_data(void)
{
	int ret = 0;
	u32 u32ReadCRValue = 0;
	int retry_count = 500;
	u32 sdio_header_length = 0;

	memset(rxbuf, 0, MTK_RXDATA_SIZE);

	do {
		ret = btmtk_sdio_readl(CHISR, &u32ReadCRValue);
		BTMTK_DBG("loop Get CHISR 0x%08X", u32ReadCRValue);
		reg_CHISR = u32ReadCRValue;
		rx_length = (u32ReadCRValue & RX_PKT_LEN) >> 16;
		if (rx_length == 0xFFFF) {
			BTMTK_WARN("0xFFFF==rx_length, error return -EIO");
			ret = -EIO;
			break;
		}

		if ((RX_DONE&u32ReadCRValue) && rx_length) {
			if (rx_length > MTK_RXDATA_SIZE) {
				BTMTK_ERR("rx_length %d is bigger than MTK_RXDATA_SIZE %d",
					rx_length, MTK_RXDATA_SIZE);
				ret = -EIO;
				break;
			}

			BTMTK_DBG("u32ReadCRValue = %08X", u32ReadCRValue);
			u32ReadCRValue &= 0xFFFB;
			ret = btmtk_sdio_writel(CHISR, u32ReadCRValue);
			BTMTK_DBG("write = %08X", u32ReadCRValue);


			ret = btmtk_sdio_readsb(CRDR, rxbuf, rx_length);
			sdio_header_length = (rxbuf[1] << 8);
			sdio_header_length |= rxbuf[0];

			if (sdio_header_length != rx_length) {
				BTMTK_ERR("sdio header length %d, rx_length %d mismatch",
					sdio_header_length, rx_length);
				break;
			}

			if (sdio_header_length == 0) {
				BTMTK_WARN("get sdio_header_length = %d", sdio_header_length);
				continue;
			}

			break;
		}

		retry_count--;
		if (retry_count <= 0) {
			BTMTK_WARN("retry_count = %d,timeout", retry_count);
			btmtk_sdio_print_debug_sr();
			ret = -EIO;
			break;
		}

		/* msleep(1); */
		mdelay(3);
		BTMTK_DBG("retry_count = %d,wait", retry_count);

		if (ret)
			break;
	} while (1);

	if (ret)
		return -EIO;

	return ret;
}

static int btmtk_sdio_send_wmt_cmd(u8 *cmd, int cmd_len,
		const u8 *event, const int event_len)
{
	int ret = 0;
	u8 mtksdio_packet_header[MTK_SDIO_PACKET_HEADER_SIZE] = {0};

	BTMTK_INFO("begin");
	mtksdio_packet_header[0] = sizeof(mtksdio_packet_header) + cmd_len + 1;

	memcpy(txbuf, mtksdio_packet_header, MTK_SDIO_PACKET_HEADER_SIZE);
	memcpy(txbuf + MTK_SDIO_PACKET_HEADER_SIZE + 1, cmd, cmd_len);
	txbuf[MTK_SDIO_PACKET_HEADER_SIZE] = 0x1;

	btmtk_sdio_send_tx_data(txbuf, MTK_SDIO_PACKET_HEADER_SIZE + cmd_len + 1);
	btmtk_sdio_recv_rx_data();

	if (event && event_len) {
		/*compare rx data is wmt reset correct response or not*/
		if (memcmp(event, rxbuf + MTK_SDIO_PACKET_HEADER_SIZE + 1, event_len) != 0) {
			ret = -EIO;
			BTMTK_WARN("fail");
		}
	} else {
		BTSDIO_INFO_RAW(cmd, cmd_len, "%s: CMD:", __func__);
		BTSDIO_INFO_RAW(rxbuf + MTK_SDIO_PACKET_HEADER_SIZE + 1,
			rx_length - MTK_SDIO_PACKET_HEADER_SIZE - 1,
			"EVT:");
	}

	return ret;
}

static int btmtk_sdio_send_wmt_cfg(void)
{
	int ret = 0;
	int index = 0;

	BTMTK_INFO("begin");

	for (index = 0; index < WMT_CMD_COUNT; index++) {
		if (g_card->bt_cfg.wmt_cmd[index].content && g_card->bt_cfg.wmt_cmd[index].length) {
			ret = btmtk_sdio_send_wmt_cmd(g_card->bt_cfg.wmt_cmd[index].content,
					g_card->bt_cfg.wmt_cmd[index].length,
					NULL, 0);
			if (ret) {
				BTMTK_ERR("Send wmt cmd failed(%d)! Index: %d", ret, index);
				return ret;
			}
		}
	}

	return ret;
}

static int btmtk_sdio_send_wmt_reset(void)
{
	int ret = 0;
	u8 wmt_event[8] = {4, 0xE4, 5, 2, 7, 1, 0, 0};
	u8 mtksdio_packet_header[MTK_SDIO_PACKET_HEADER_SIZE] = {0};
	u8 mtksdio_wmt_reset[9] = {1, 0x6F, 0xFC, 5, 1, 7, 1, 0, 4};

	BTMTK_INFO("begin");
	mtksdio_packet_header[0] = sizeof(mtksdio_packet_header) +
		sizeof(mtksdio_wmt_reset);

	memcpy(txbuf, mtksdio_packet_header, MTK_SDIO_PACKET_HEADER_SIZE);
	memcpy(txbuf+MTK_SDIO_PACKET_HEADER_SIZE, mtksdio_wmt_reset,
		sizeof(mtksdio_wmt_reset));

	btmtk_sdio_send_tx_data(txbuf,
		MTK_SDIO_PACKET_HEADER_SIZE+sizeof(mtksdio_wmt_reset));
	btmtk_sdio_recv_rx_data();

	/*compare rx data is wmt reset correct response or not*/
	if (memcmp(wmt_event, rxbuf+MTK_SDIO_PACKET_HEADER_SIZE,
			sizeof(wmt_event)) != 0) {
		ret = -EIO;
		BTMTK_WARN("fail");
	}

	return ret;
}

static u32 btmtk_sdio_bt_memRegister_read(u32 cr)
{
	int retrytime = 3;
	u32 result = 0;
	u8 wmt_event[15] = {0x04, 0xE4, 0x10, 0x02,
				0x08, 0x0C/*0x1C*/, 0x00, 0x00,
				0x00, 0x00, 0x01, 0x00,
				0x00, 0x00, 0x80};
	/* msleep(1000); */
	u8 mtksdio_packet_header[MTK_SDIO_PACKET_HEADER_SIZE] = {0};
	u8 mtksdio_wmt_cmd[16] = {0x1, 0x6F, 0xFC, 0x0C,
				0x01, 0x08, 0x08, 0x00,
				0x02, 0x01, 0x00, 0x01,
				0x00, 0x00, 0x00, 0x00};
	mtksdio_packet_header[0] = sizeof(mtksdio_packet_header)
				+ sizeof(mtksdio_wmt_cmd);
	BTMTK_INFO("read cr %x", cr);

	memcpy(&mtksdio_wmt_cmd[12], &cr, sizeof(cr));

	memcpy(txbuf, mtksdio_packet_header, MTK_SDIO_PACKET_HEADER_SIZE);
	memcpy(txbuf + MTK_SDIO_PACKET_HEADER_SIZE, mtksdio_wmt_cmd,
		sizeof(mtksdio_wmt_cmd));

	btmtk_sdio_send_tx_data(txbuf,
		MTK_SDIO_PACKET_HEADER_SIZE + sizeof(mtksdio_wmt_cmd));
	btmtk_print_buffer_conent(txbuf,
		MTK_SDIO_PACKET_HEADER_SIZE + sizeof(mtksdio_wmt_cmd));

	do {
		usleep_range(10*1000, 15*1000);
		btmtk_sdio_recv_rx_data();
		retrytime--;
		if (retrytime <= 0)
			break;

		BTMTK_INFO("retrytime %d", retrytime);
	} while (!rxbuf[0]);

	btmtk_print_buffer_conent(rxbuf, rx_length);
	/* compare rx data is wmt reset correct response or not */
#if 0
	if (memcmp(wmt_event,
			rxbuf+MTK_SDIO_PACKET_HEADER_SIZE,
			sizeof(wmt_event)) != 0) {
		ret = -EIO;
		BTMTK_INFO("fail");
	}
#endif
	memcpy(&result, rxbuf+MTK_SDIO_PACKET_HEADER_SIZE + sizeof(wmt_event),
		sizeof(result));
	BTMTK_INFO("ger cr 0x%x value 0x%x", cr, result);
	return result;
}

static int btmtk_sdio_send_hci_reset(bool is_closed)
{
	int ret = 0;
	u8 event[] = {0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00};
	u8 cmd[] = {0x03, 0x0c, 0x00};

	BTMTK_INFO("begin\n");
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
		cmd, sizeof(cmd),
		event, sizeof(event), COMP_EVENT_TIMO);
	if (ret != 0)
		BTMTK_ERR("ret = %d\n", ret);

	if (is_closed == true)
		get_hci_reset = 0;
	return ret;
}

/* 1:on ,  0:off */
static int btmtk_sdio_bt_set_power(u8 onoff)
{
	int ret = 0;
	int count = 0;
	u8 event[] = {0xE4, 0x05, 0x02, 0x06, 0x01, 0x00, 0x00};
	u8 cmd[] = {0x6F, 0xFC, 0x06, 0x01, 0x06, 0x02, 0x00, 0x00, 0x01};

	BTMTK_INFO("onoff %d", onoff);
	cmd[8] = onoff;

	do {
		ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd),
			event, sizeof(event), COMP_EVENT_TIMO);
		if (ret)
			g_card->dongle_state = BT_SDIO_DONGLE_STATE_ERROR;
		else
			g_card->dongle_state =
				onoff ? BT_SDIO_DONGLE_STATE_POWER_ON : BT_SDIO_DONGLE_STATE_POWER_OFF;
	} while (++count < SET_POWER_NUM && ret);

	return ret;
}

static int btmtk_sdio_send_vendor_cfg(void)
{
	int ret = 0;
	int index = 0;

	BTMTK_INFO("begin");

	for (index = 0; index < VENDOR_CMD_COUNT; index++) {
		if (g_card->bt_cfg.vendor_cmd[index].content && g_card->bt_cfg.vendor_cmd[index].length) {
			ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
					g_card->bt_cfg.vendor_cmd[index].content,
					g_card->bt_cfg.vendor_cmd[index].length,
					NULL, 0, COMP_EVENT_TIMO);
			if (ret) {
				BTMTK_ERR("Send vendor cmd failed(%d)! Index: %d", ret, index);
				return ret;
			}
		}
	}

	return ret;
}

static int btmtk_sdio_setup_picus_param(struct fw_cfg_struct *picus_setting)
{
	u8 dft_cmd[] = {0x5F, 0xFC, 0x2E, 0x50, 0x01, 0x0A, 0x00,
			0x00, 0x00, 0x01, 0x00, 0x00, 0xE0, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
			0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
			0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00,
			0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00};
	u8 *cmd = NULL;
	int len = 0;
	u8 event[] = { 0x0E, 0x04, 0x01, 0x5F, 0xFC, 0x00 };
	int ret = -1;	/* if successful, 0 */

	if (picus_setting->content && picus_setting->length) {
		cmd = picus_setting->content;
		len = picus_setting->length;
	} else {
		cmd = dft_cmd;
		len = sizeof(dft_cmd);
	}

	BTSDIO_INFO_RAW(cmd, len, "%s: Filter CMD:", __func__);
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
			cmd, len,
			event, sizeof(event), COMP_EVENT_TIMO);
	if (ret) {
		BTMTK_ERR("Send picus filter cmd failed(%d)!", ret);
		return ret;
	}

	return ret;
}

static int btmtk_sdio_picus_operation(bool enable)
{

	u8 cmd[] = {0xBE, 0xFC, 0x01, 0x15};
	u8 event[] = {0x0E, 0x04, 0x01, 0xBE, 0xFC, 0x00};
	int ret = -1;	/* if successful, 0 */

	if (enable == false)
		cmd[3] = 0x00;

	BTSDIO_INFO_RAW(cmd, (int)sizeof(cmd), "%s: Enable CMD:", __func__);
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
			cmd, sizeof(cmd),
			event, sizeof(event), COMP_EVENT_TIMO);
	if (ret) {
		BTMTK_ERR("Send picus enable cmd failed(%d)!", ret);
		return ret;
	}

	return ret;
}

/*get 1 byte only*/
static int btmtk_efuse_read(u16 addr, u8 *value)
{
	uint8_t efuse_r[] = {0x6F, 0xFC, 0x0E,
				0x01, 0x0D, 0x0A, 0x00, 0x02, 0x04,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00};/*4 sub block number(sub block 0~3)*/

	uint8_t efuse_r_event[] = {0xE4, 0x1E, 0x02, 0x0D, 0x1A, 0x00, 02, 04};
	/*check event
	 *04 E4 LEN(1B) 02 0D LEN(2Byte) 02 04 ADDR(2Byte) VALUE(4B) ADDR(2Byte) VALUE(4Byte)
	 *ADDR(2Byte) VALUE(4B)  ADDR(2Byte) VALUE(4Byte)
	 */
	int ret = 0;
	uint8_t sub_block_addr_in_event = 0;
	uint16_t sub_block = (addr / 16) * 4;
	uint8_t temp = 0;

	efuse_r[9] = sub_block & 0xFF;
	efuse_r[10] = (sub_block & 0xFF00) >> 8;
	efuse_r[11] = (sub_block + 1) & 0xFF;
	efuse_r[12] = ((sub_block + 1) & 0xFF00) >> 8;
	efuse_r[13] = (sub_block + 2) & 0xFF;
	efuse_r[14] = ((sub_block + 2) & 0xFF00) >> 8;
	efuse_r[15] = (sub_block + 3) & 0xFF;
	efuse_r[16] = ((sub_block + 3) & 0xFF00) >> 8;

	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
		efuse_r, sizeof(efuse_r),
		efuse_r_event, sizeof(efuse_r_event), COMP_EVENT_TIMO);
	if (ret) {
		BTMTK_WARN("btmtk_sdio_send_hci_cmd error");
		BTMTK_WARN("rx_length %d", rx_length);
		return ret;
	}

	if (memcmp(rxbuf + MTK_SDIO_PACKET_HEADER_SIZE + 1, efuse_r_event, sizeof(efuse_r_event)) == 0) {
		/*compare rxbuf format ok, compare addr*/
		BTMTK_DBG("compare rxbuf format ok");
		if (efuse_r[9] == rxbuf[9 + MTK_SDIO_PACKET_HEADER_SIZE] &&
			efuse_r[10] == rxbuf[10 + MTK_SDIO_PACKET_HEADER_SIZE] &&
			efuse_r[11] == rxbuf[15 + MTK_SDIO_PACKET_HEADER_SIZE] &&
			efuse_r[12] == rxbuf[16 + MTK_SDIO_PACKET_HEADER_SIZE] &&
			efuse_r[13] == rxbuf[21 + MTK_SDIO_PACKET_HEADER_SIZE] &&
			efuse_r[14] == rxbuf[22 + MTK_SDIO_PACKET_HEADER_SIZE] &&
			efuse_r[15] == rxbuf[27 + MTK_SDIO_PACKET_HEADER_SIZE] &&
			efuse_r[16] == rxbuf[28 + MTK_SDIO_PACKET_HEADER_SIZE]) {

			BTMTK_DBG("address compare ok");
			/*Get value*/
			sub_block_addr_in_event = ((addr / 16) / 4);/*cal block num*/
			temp = addr % 16;
			BTMTK_DBG("address in block %d", temp);
			switch (temp) {
			case 0:
			case 1:
			case 2:
			case 3:
				*value = rxbuf[11 + temp + MTK_SDIO_PACKET_HEADER_SIZE];
				break;
			case 4:
			case 5:
			case 6:
			case 7:
				*value = rxbuf[17 + temp + MTK_SDIO_PACKET_HEADER_SIZE];
				break;
			case 8:
			case 9:
			case 10:
			case 11:
				*value = rxbuf[22 + temp + MTK_SDIO_PACKET_HEADER_SIZE];
				break;

			case 12:
			case 13:
			case 14:
			case 15:
				*value = rxbuf[34 + temp + MTK_SDIO_PACKET_HEADER_SIZE];
				break;
			}


		} else {
			BTMTK_WARN("address compare fail");
			ret = -1;
		}


	} else {
		BTMTK_WARN("compare rxbuf format fail");
		ret = -1;
	}

	return ret;
}

static int btmtk_check_auto_mode(struct btmtk_sdio_card *card)
{
	u16 addr = 1;
	u8 value = 0;

	if (card->efuse_mode != EFUSE_AUTO_MODE)
		return 0;

	if (btmtk_efuse_read(addr, &value)) {
		BTMTK_WARN("read fail");
		BTMTK_WARN("Use EEPROM Bin file mode");
		card->efuse_mode = EFUSE_BIN_FILE_MODE;
		return -EIO;
	}

	if (value == 0x76) {
		BTMTK_WARN("get efuse[1]: 0x%02x", value);
		BTMTK_WARN("use efuse mode");
		card->efuse_mode = EFUSE_MODE;
	} else {
		BTMTK_WARN("get efuse[1]: 0x%02x", value);
		BTMTK_WARN("Use EEPROM Bin file mode");
		card->efuse_mode = EFUSE_BIN_FILE_MODE;
	}

	return 0;
}

static int btmtk_sdio_send_init_cmds(struct btmtk_sdio_card *card)
{
	if (btmtk_sdio_bt_set_power(1)) {
		BTMTK_ERR("power on failed");
		return -EIO;
	}

	if (btmtk_check_auto_mode(card)) {
		BTMTK_ERR("check auto mode failed");
		return -EIO;
	}

	if (btmtk_sdio_set_sleep()) {
		BTMTK_ERR("set sleep failed");
		return -EIO;
	}

	if (btmtk_sdio_set_audio()) {
		BTMTK_ERR("set audio failed");
		return -EIO;
	}

	if (btmtk_sdio_send_vendor_cfg()) {
		BTMTK_ERR("send vendor cmd failed");
		return -EIO;
	}

	if (g_card->bt_cfg.support_auto_picus == true) {
		if (btmtk_sdio_setup_picus_param(&g_card->bt_cfg.picus_filter)) {
			BTMTK_ERR("send setup_picus_param cmd failed");
			return -EIO;
		}
	}

	return 0;
}

static int btmtk_sdio_send_deinit_cmds(void)
{
	if (btmtk_sdio_bt_set_power(0)) {
		BTMTK_ERR("power off failed");
		return -EIO;
	}
	return 0;
}

static void btmtk_parse_efuse_mode(uint8_t *buf, size_t buf_size, struct btmtk_sdio_card *card)
{
	char *p_buf = NULL;
	char *ptr = NULL, *p = NULL;

	card->efuse_mode = EFUSE_MODE;
	if (!buf) {
		BTMTK_WARN("buf is null");
		return;
	} else if (buf_size < (strlen(E2P_MODE) + 2)) {
		BTMTK_WARN("incorrect buf size(%d)", (int)buf_size);
		return;
	}

	p_buf = kmalloc(buf_size + 1, GFP_KERNEL);
	if (!p_buf)
		return;
	memcpy(p_buf, buf, buf_size);
	p_buf[buf_size] = '\0';

	/* find string */
	p = ptr = strstr(p_buf, E2P_MODE);
	if (!ptr) {
		BTMTK_ERR("Can't find %s", E2P_MODE);
		goto out;
	}

	if (p > p_buf) {
		p--;
		while ((*p == ' ') && (p != p_buf))
			p--;
		if (*p == '#') {
			BTMTK_ERR("It's not EEPROM - Bin file mode");
			goto out;
		}
	}

	/* check access mode */
	ptr += (strlen(E2P_MODE) + 1);
	BTMTK_WARN("It's EEPROM bin mode: %c", *ptr);
	card->efuse_mode = *ptr - '0';
	if (card->efuse_mode > EFUSE_AUTO_MODE)
		card->efuse_mode = EFUSE_MODE;
out:
	kfree(p_buf);
	return;
}

static void btmtk_set_eeprom2ctrler(uint8_t *buf,
						size_t buf_size,
						int device)
{
	int ret = -1;
	uint8_t set_bdaddr[] = {0x1A, 0xFC, 0x06,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t set_bdaddr_e[] = {0x0E, 0x04, 0x01,
			0x1A, 0xFC, 0x00};
	uint8_t set_radio[] = {0x79, 0xFC, 0x08,
			0x07, 0x80, 0x00, 0x06, 0x07, 0x07, 0x00, 0x00};
	uint8_t set_radio_e[] = {0x0E, 0x04, 0x01,
			0x79, 0xFC, 0x00};
	uint8_t set_pwr_offset[] = {0x93, 0xFC, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t set_pwr_offset_e[] = {0x0E, 0x04, 0x01,
			0x93, 0xFC, 0x00};
	uint8_t set_xtal[] = {0x0E, 0xFC, 0x02, 0x00, 0x00};
	uint8_t set_xtal_e[] = {0x0E, 0x04, 0x01,
			0x0E, 0xFC, 0x00};
	uint16_t offset = 0;

	BTMTK_INFO("start, device: 0x%x", device);

	if (!buf) {
		BTMTK_WARN("buf is null");
		return;
	} else if (device == 0x7668 && buf_size < 0x389) {
		BTMTK_WARN("incorrect buf size(%d)", (int)buf_size);
		return;
	} else if (device == 0x7663 && buf_size < 0x389) {
		BTMTK_WARN("incorrect buf size(%d)", (int)buf_size);
		return;
	}

	/* set BD address */
	if (device == 0x7668)
		offset = 0x384;
	else if (device == 0x7663)
		offset = 0x131;
	else
		offset = 0x1A;

	set_bdaddr[3] = *(buf + offset);
	set_bdaddr[4] = *(buf + offset + 1);
	set_bdaddr[5] = *(buf + offset + 2);
	set_bdaddr[6] = *(buf + offset + 3);
	set_bdaddr[7] = *(buf + offset + 4);
	set_bdaddr[8] = *(buf + offset + 5);
	if (0x0 == set_bdaddr[3] && 0x0 == set_bdaddr[4]
			&& 0x0 == set_bdaddr[5] && 0x0 == set_bdaddr[6]
			&& 0x0 == set_bdaddr[7] && 0x0 == set_bdaddr[8]) {
		BTMTK_WARN("BDAddr is Zero, not set");
	} else {
		ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, set_bdaddr,
			set_bdaddr[2] + 3,
			set_bdaddr_e, sizeof(set_bdaddr_e), COMP_EVENT_TIMO);
		BTMTK_WARN("set BDAddress(%02X-%02X-%02X-%02X-%02X-%02X) %s",
				set_bdaddr[8], set_bdaddr[7], set_bdaddr[6],
				set_bdaddr[5], set_bdaddr[4], set_bdaddr[3],
				ret < 0 ? "fail" : "OK");
	}
	/* radio setting - BT power */
	if (device == 0x7668) {
		offset = 0x382;
		/* BT default power */
		set_radio[3] = (*(buf + offset) & 0x07);
		/* BLE default power */
		set_radio[7] = (*(buf + offset + 1) & 0x07);
		/* TX MAX power */
		set_radio[8] = (*(buf + offset) & 0x70) >> 4;
		/* TX power sub level */
		set_radio[9] = (*(buf + offset + 1) & 0x30) >> 4;
		/* BR/EDR power diff mode */
		set_radio[10] = (*(buf + offset + 1) & 0xc0) >> 6;
	} else if (device == 0x7663) {
		offset = 0x137;
		/* BT default power */
		set_radio[3] = (*(buf + offset) & 0x07);
		/* BLE default power */
		set_radio[7] = (*(buf + offset + 1) & 0x07);
		/* TX MAX power */
		set_radio[8] = (*(buf + offset) & 0x70) >> 4;
		/* TX power sub level */
		set_radio[9] = (*(buf + offset + 1) & 0x30) >> 4;
		/* BR/EDR power diff mode */
		set_radio[10] = (*(buf + offset + 1) & 0xc0) >> 6;
	} else {
		offset = 0x132;
		/* BT default power */
		set_radio[3] = *(buf + offset);
		/* BLE default power(no this for 7662 in table) */
		set_radio[7] = *(buf + offset);
		/* TX MAX power */
		set_radio[8] = *(buf + offset + 1);
	}
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, set_radio,
		set_radio[2] + 3,
		set_radio_e, sizeof(set_radio_e), COMP_EVENT_TIMO);
	BTMTK_WARN("set radio(BT/BLE default power: %d/%d MAX power: %d) %s",
			set_radio[3], set_radio[7], set_radio[8],
			ret < 0 ? "fail" : "OK");

	/*
	 * BT TX power compensation for low, middle and high
	 * channel
	 */
	if (device == 0x7668) {
		offset = 0x36D;
		/* length */
		set_pwr_offset[2] = 6;
		/* Group 0 CH 0 ~ CH14 */
		set_pwr_offset[3] = *(buf + offset);
		/* Group 1 CH15 ~ CH27 */
		set_pwr_offset[4] = *(buf + offset + 1);
		/* Group 2 CH28 ~ CH40 */
		set_pwr_offset[5] = *(buf + offset + 2);
		/* Group 3 CH41 ~ CH53 */
		set_pwr_offset[6] = *(buf + offset + 3);
		/* Group 4 CH54 ~ CH66 */
		set_pwr_offset[7] = *(buf + offset + 4);
		/* Group 5 CH67 ~ CH84 */
		set_pwr_offset[8] = *(buf + offset + 5);
	} else if (device == 0x7663) {
		offset = 0x180;
		/* length */
		set_pwr_offset[2] = 16;
		/* Group 0 CH 0 ~ CH6 */
		set_pwr_offset[3] = *(buf + offset);
		/* Group 1 CH7 ~ CH11 */
		set_pwr_offset[4] = *(buf + offset + 1);
		/* Group 2 CH12 ~ CH16 */
		set_pwr_offset[5] = *(buf + offset + 2);
		/* Group 3 CH17 ~ CH21 */
		set_pwr_offset[6] = *(buf + offset + 3);
		/* Group 4 CH22 ~ CH26 */
		set_pwr_offset[7] = *(buf + offset + 4);
		/* Group 5 CH27 ~ CH31 */
		set_pwr_offset[8] = *(buf + offset + 5);
		/* Group 6 CH32 ~ CH36 */
		set_pwr_offset[9] = *(buf + offset + 6);
		/* Group 7 CH37 ~ CH41 */
		set_pwr_offset[10] = *(buf + offset + 7);
		/* Group 8 CH42 ~ CH46 */
		set_pwr_offset[11] = *(buf + offset + 8);
		/* Group 9 CH47 ~ CH51 */
		set_pwr_offset[12] = *(buf + offset + 9);
		/* Group 10 CH52 ~ CH56 */
		set_pwr_offset[13] = *(buf + offset + 10);
		/* Group 11 CH57 ~ CH61 */
		set_pwr_offset[14] = *(buf + offset + 11);
		/* Group 12 CH62 ~ CH66 */
		set_pwr_offset[15] = *(buf + offset + 12);
		/* Group 13 CH67 ~ CH71 */
		set_pwr_offset[16] = *(buf + offset + 13);
		/* Group 14 CH72 ~ CH76 */
		set_pwr_offset[17] = *(buf + offset + 14);
		/* Group 15 CH77 ~ CH78 */
		set_pwr_offset[18] = *(buf + offset + 15);
	} else {
		offset = 0x139;
		/* length */
		set_pwr_offset[2] = 3;
		/* low channel */
		set_pwr_offset[3] = *(buf + offset);
		/* middle channel */
		set_pwr_offset[4] = *(buf + offset + 1);
		/* high channel */
		set_pwr_offset[5] = *(buf + offset + 2);
	}
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, set_pwr_offset,
		set_pwr_offset[2] + 3,
		set_pwr_offset_e, sizeof(set_pwr_offset_e), COMP_EVENT_TIMO);
	BTMTK_WARN("set power offset(%02X %02X %02X %02X %02X %02X) %s",
			set_pwr_offset[3], set_pwr_offset[4],
			set_pwr_offset[5], set_pwr_offset[6],
			set_pwr_offset[7], set_pwr_offset[8],
			ret < 0 ? "fail" : "OK");

	/* XTAL setting */
	if (device == 0x7668) {
		offset = 0xF4;
		/* BT default power */
		set_xtal[3] = *(buf + offset);
		set_xtal[4] = *(buf + offset + 1);
		ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, set_xtal,
			set_xtal[2] + 3,
			set_xtal_e, sizeof(set_xtal_e), COMP_EVENT_TIMO);
		BTMTK_WARN("set XTAL(0x%02X %02X) %s",
				set_xtal[3], set_xtal[4],
				ret < 0 ? "fail" : "OK");
	}
	BTMTK_INFO("end");
}

static void btmtk_set_pa(uint8_t pa)
{
	int ret = -1;
	uint8_t epa[] = {0x70, 0xFD, 0x09,
		0x00,
		0x07, 0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x00};
	uint8_t epa_e[] = {0x0E, 0x04, 0x01,
		0x70, 0xFD, 0x00};

	epa[3] = pa;
	if (pa > 1 || pa < 0) {
		BTMTK_WARN("Incorrect format");
		return;
	}
	if (pa == 1) {
		BTMTK_WARN("ePA mode, change power level to level 9.");
		epa[4] = 0x09;
		epa[8] = 0x09;
		epa[9] = 0x09;
	}

	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, epa, sizeof(epa),
		epa_e, sizeof(epa_e), COMP_EVENT_TIMO);
	BTMTK_WARN("set PA(%d) %s",
		pa, ret < 0 ? "fail" : "OK");
}

static void btmtk_set_duplex(uint8_t duplex)
{
	int ret = -1;
	uint8_t ant[] = {0x71, 0xFD, 0x01, 0x00};
	uint8_t ant_e[] = {0x0E, 0x04, 0x01,
		0x71, 0xFD, 0x00};

	ant[3] = duplex;
	if (duplex > 1 || duplex < 0) {
		BTMTK_WARN("Incorrect format");
		return;
	}

	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, ant, sizeof(ant),
		ant_e, sizeof(ant_e), COMP_EVENT_TIMO);
	BTMTK_WARN("set Duplex(%d) %s",
		duplex, ret < 0 ? "fail" : "OK");
}

static void btmtk_set_pa_and_duplex(uint8_t *buf, size_t buf_size)
{
	char *p_buf = NULL;
	char *ptr = NULL, *p = NULL;

	if (!buf) {
		BTMTK_WARN("buf is null");
		return;
	} else if (buf_size < (strlen(E2P_MODE) + 2)) {
		BTMTK_WARN("incorrect buf size(%d)", (int)buf_size);
		return;
	}
	p_buf = kmalloc(buf_size + 1, GFP_KERNEL);
	if (!p_buf)
		return;

	memcpy(p_buf, buf, buf_size);
	p_buf[buf_size] = '\0';
	/* find string */
	p = ptr = strstr(p_buf, E2P_ACCESS_EPA);
	if (!ptr) {
		BTMTK_WARN("Can't find %s", E2P_ACCESS_EPA);
		g_card->pa_setting = -1;
		g_card->duplex_setting = -1;
		goto out;
	}
	if (p > p_buf) {
		p--;
		while ((*p == ' ') && (p != p_buf))
			p--;
		if (*p == '#') {
			BTMTK_WARN("It's no pa setting");
			g_card->pa_setting = -1;
			g_card->duplex_setting = -1;
			goto out;
		}
	}
	/* check access mode */
	ptr += (strlen(E2P_ACCESS_EPA) + 1);
	if (*ptr != '0') {
		BTMTK_WARN("ePA mode: %c", *ptr);
		g_card->pa_setting = 1;
	} else {
		BTMTK_WARN("iPA mode: %c", *ptr);
		g_card->pa_setting = 0;
	}

	p = ptr = strstr(p_buf, E2P_ACCESS_DUPLEX);
	if (!ptr) {
		BTMTK_WARN("Can't find %s", E2P_ACCESS_DUPLEX);
		g_card->duplex_setting = -1;
		goto out;
	}
	if (p > p_buf) {
		p--;
		while ((*p == ' ') && (p != p_buf))
			p--;
		if (*p == '#') {
			BTMTK_WARN("It's no duplex setting");
			g_card->duplex_setting = -1;
			goto out;
		}
	}
	/* check access mode */
	ptr += (strlen(E2P_ACCESS_DUPLEX) + 1);
	if (*ptr != '0') {
		BTMTK_WARN("TDD mode: %c", *ptr);
		g_card->duplex_setting = 1;
	} else {
		BTMTK_WARN("FDD mode: %c", *ptr);
		g_card->duplex_setting = 0;
	}

out:
	kfree(p_buf);
}

void btmtk_set_keep_full_pwr(uint8_t *buf, size_t buf_size)
{
	char *p_buf = NULL;
	char *ptr = NULL;

	g_card->is_KeepFullPwr = false;

	if (!buf) {
		BTMTK_ERR("buf is null");
		return;
	} else if (buf_size < (strlen(KEEP_FULL_PWR) + 2)) {
		BTMTK_ERR("incorrect buf size(%d)", (int)buf_size);
		return;
	}

	p_buf = kmalloc(buf_size + 1, GFP_KERNEL);
	if (!p_buf)
		return;

	memcpy(p_buf, buf, buf_size);
	p_buf[buf_size] = '\0';

	/* find string */
	ptr = strstr(p_buf, KEEP_FULL_PWR);
	if (!ptr) {
		BTMTK_ERR("Can't find %s", KEEP_FULL_PWR);
		goto out;
	}

	/* check always driver own */
	ptr += (strlen(KEEP_FULL_PWR) + 1);

	if (*ptr == PWR_KEEP_NO_FW_OWN) {
		/* always driver own is set*/
		BTMTK_INFO("Read KeepFullPwr on: %c", *ptr);
		g_card->is_KeepFullPwr = true;
	} else if (*ptr == PWR_SWITCH_DRIVER_FW_OWN) {
		/* always driver own is not set */
		BTMTK_INFO("Read KeepFullPwr off: %c", *ptr);
	} else {
		BTMTK_WARN("It's not the correct own setting: %c", *ptr);
	}

out:
	kfree(p_buf);
}

static void btmtk_set_power_limit(uint8_t *buf,
						size_t buf_size,
						bool is7668)
{
	int ret = -1;
	uint8_t set_radio[] = {0x79, 0xFC, 0x08,
		0x07, 0x80, 0x00, 0x06, 0x07, 0x07, 0x00, 0x00};
	uint8_t set_radio_e[] = {0x0E, 0x04, 0x01,
		0x79, 0xFC, 0x00};
	uint16_t offset = 0;

	if (!buf) {
		BTMTK_WARN("buf is null");
		return;
	} else if (is7668 == false) {
		BTMTK_WARN("only support mt7668 right now");
		return;
	}

	/* radio setting - BT power */
	if (is7668) {
		/* BT default power */
		set_radio[3] = (*(buf + offset));
		/* BLE default power */
		set_radio[7] = (*(buf + offset + 1));
		/* TX MAX power */
		set_radio[8] = (*(buf + offset + 2));
		/* TX power sub level */
		set_radio[9] = 0;
		/* BR/EDR power diff mode */
		set_radio[10] = (*(buf + offset + 3));
	}

	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, set_radio, sizeof(set_radio),
		set_radio_e, sizeof(set_radio_e), COMP_EVENT_TIMO);
	BTMTK_WARN("set radio(BT/BLE default power: %d/%d MAX power: %d) %s",
		set_radio[3], set_radio[7], set_radio[8],
		ret < 0 ? "fail" : "OK");

}

static void btmtk_requset_power_limit_callback(const struct firmware *pwr_fw, void *context)
{
	struct btmtk_sdio_card *card = (struct btmtk_sdio_card *)context;

	BTMTK_INFO("begin");
	if (pwr_fw != NULL) {
		/* set parameters to controller */
		btmtk_set_power_limit((uint8_t *)pwr_fw->data,
			pwr_fw->size,
			(card->func->device == 0x7668 ? true : false));
		release_firmware(pwr_fw);
	} else {
		BTMTK_INFO("pwr_fw is NULL");
	}
}

static void btmtk_eeprom_bin_file(struct btmtk_sdio_card *card)
{
	char *cfg_file = NULL;
	char bin_file[32];

	const struct firmware *cfg_fw = NULL;
	const struct firmware *bin_fw = NULL;

	int ret = -1;
	int chipid = card->func->device;

	BTMTK_INFO("%X series", chipid);
	cfg_file = E2P_ACCESS_MODE_SWITCHER;
	sprintf(bin_file, E2P_BIN_FILE, chipid);

	usleep_range(10*1000, 15*1000);

	/* request configure file */
	ret = request_firmware(&cfg_fw, cfg_file, &card->func->dev);
	if (ret < 0) {
		if (ret == -EAGAIN) {
			cfg_fw = NULL;
			BTMTK_WARN("try to load configure file again");
			ret = request_firmware(&cfg_fw, cfg_file, &card->func->dev);
			if (ret < 0) {
				BTMTK_WARN("load configure file again but still fail(%d)", ret);
				goto exit;
			}
			BTMTK_WARN("load configure file again and success(%d)", ret);
		} else {
			if (ret == -ENOENT)
				BTMTK_WARN("Configure file not found, ignore EEPROM bin file");
			else
				BTMTK_WARN("request configure file fail(%d)", ret);
			goto exit;
		}
	}

	if (cfg_fw) {
		btmtk_set_pa_and_duplex((uint8_t *)cfg_fw->data, cfg_fw->size);
		btmtk_set_keep_full_pwr((uint8_t *)cfg_fw->data, cfg_fw->size);
	} else {
		BTMTK_ERR("cfg_fw is null");
		card->efuse_mode = EFUSE_MODE;
		goto exit;
	}

	btmtk_parse_efuse_mode((uint8_t *)cfg_fw->data, cfg_fw->size, card);
	if (card->efuse_mode == EFUSE_MODE) {
		if (card->bin_file_buffer != NULL) {
			kfree(card->bin_file_buffer);
			card->bin_file_buffer = NULL;
			card->bin_file_size = 0;
		}
		goto exit;
	}

	usleep_range(10*1000, 15*1000);

	/* open bin file for EEPROM */
	ret = request_firmware(&bin_fw, bin_file, &card->func->dev);
	if (ret < 0) {
		BTMTK_WARN("request bin file fail(%d)", ret);
		goto exit;
	}

	card->bin_file_buffer = kmalloc(bin_fw->size, GFP_KERNEL);
	if (card->bin_file_buffer == NULL) {
		goto exit;
	}
	memcpy(card->bin_file_buffer, bin_fw->data, bin_fw->size);
	card->bin_file_size = bin_fw->size;

exit:
	/* open power limit */
	ret = request_firmware_nowait(THIS_MODULE, true, TX_PWR_LIMIT,
		&card->func->dev, GFP_KERNEL, g_card, btmtk_requset_power_limit_callback);
	if (ret < 0)
		BTMTK_WARN("request power limit file fail(%d)", ret);

	if (cfg_fw)
		release_firmware(cfg_fw);
	if (bin_fw)
		release_firmware(bin_fw);
}

/* 1:on ,  0:off */
static int btmtk_sdio_set_sleep(void)
{
	int ret = 0;
	u8 event[] = {0x0E, 0x04, 0x01, 0x7A, 0xFC, 0x00};
	u8 cmd[] = {0x7A, 0xFC, 0x07,
		/*3:sdio, 5:usb*/0x03,
		/*host non sleep duration*/0x80, 0x02,
		/*host non sleep duration*/0x80, 0x02, 0x00, 0x00};
	BTMTK_INFO("begin");

	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd),
		event, sizeof(event), COMP_EVENT_TIMO);

	return ret;
}

static int btmtk_sdio_skb_enq_fwlog(void *src, u32 len, u8 type, struct sk_buff_head *queue)
{
	struct sk_buff *skb_tmp = NULL;
	int retry = 10;

	do {
		/* If we need hci type, len + 1 */
		skb_tmp = alloc_skb(type ? len + 1 : len, GFP_ATOMIC);
		if (skb_tmp != NULL)
			break;
		else if (retry <= 0) {
			BTMTK_ERR("alloc_skb return 0, error");
			return -ENOMEM;
		}
		BTMTK_ERR("alloc_skb return 0, error, retry = %d", retry);
	} while (retry-- > 0);

	if (type) {
		memcpy(&skb_tmp->data[0], &type, 1);
		memcpy(&skb_tmp->data[1], src, len);
		skb_tmp->len = len + 1;
	} else {
		memcpy(skb_tmp->data, src, len);
		skb_tmp->len = len;
	}

	LOCK_UNSLEEPABLE_LOCK(&(fwlog_metabuffer.spin_lock));
	skb_queue_tail(queue, skb_tmp);
	UNLOCK_UNSLEEPABLE_LOCK(&(fwlog_metabuffer.spin_lock));
	return 0;
}

static int btmtk_sdio_dispatch_fwlog(u8 *buf, int len)
{
	static u8 fwlog_picus_blocking_warn;
	static u8 fwlog_fwdump_blocking_warn;
	int ret = 0;

	if ((buf[0] == 0xFF && buf[2] == 0x50) ||
		(buf[0] == 0xFF && buf[1] == 0x05)) {
		if (skb_queue_len(&g_card->fwlog_fops_queue) < FWLOG_QUEUE_COUNT) {
			BTMTK_DBG("This is picus data");
			if (btmtk_sdio_skb_enq_fwlog(buf, len, 0, &g_card->fwlog_fops_queue) == 0)
				wake_up_interruptible(&fw_log_inq);

			fwlog_picus_blocking_warn = 0;
		} else {
			if (fwlog_picus_blocking_warn == 0) {
				fwlog_picus_blocking_warn = 1;
				BTMTK_WARN("fwlog queue size is full(picus)");
			}
		}
	} else if (buf[0] == 0x6f && buf[1] == 0xfc) {
		/* Coredump */
		if (skb_queue_len(&g_card->fwlog_fops_queue) < FWLOG_ASSERT_QUEUE_COUNT) {
			BTMTK_DBG("Receive coredump, move data to fwlogqueue for picus");
			if (btmtk_sdio_skb_enq_fwlog(buf, len, 0, &g_card->fwlog_fops_queue) == 0)
				wake_up_interruptible(&fw_log_inq);
			fwlog_fwdump_blocking_warn = 0;
		} else {
			if (fwlog_fwdump_blocking_warn == 0) {
				fwlog_fwdump_blocking_warn = 1;
				BTMTK_WARN("fwlog queue size is full(coredump)");
			}
		}
	}
	return ret;
}

static int btmtk_sdio_dispatch_data_bluetooth_kpi(u8 *buf, int len, u8 type)
{
	static u8 fwlog_blocking_warn;
	int ret = 0;

	if (!btmtk_bluetooth_kpi)
		return ret;

	if (skb_queue_len(&g_card->fwlog_fops_queue) < FWLOG_BLUETOOTH_KPI_QUEUE_COUNT) {
		/* sent event to queue, picus tool will log it for bluetooth KPI feature */
		if (btmtk_sdio_skb_enq_fwlog(buf, len, type, &g_card->fwlog_fops_queue) == 0) {
			wake_up_interruptible(&fw_log_inq);
			fwlog_blocking_warn = 0;
		}
	} else {
		if (fwlog_blocking_warn == 0) {
			fwlog_blocking_warn = 1;
			BTMTK_WARN("fwlog queue size is full(bluetooth_kpi)");
		}
	}
	return ret;
}

static int btmtk_sdio_host_to_card(struct btmtk_private *priv,
				u8 *payload, u16 nb)
{
	struct btmtk_sdio_card *card = priv->btmtk_dev.card;
	int ret = 0;
	int i = 0;
	u8 MultiBluckCount = 0;
	u8 redundant = 0;
	int len = 0;

	if (payload != txbuf) {
		memset(txbuf, 0, MTK_TXDATA_SIZE);
		memcpy(txbuf, payload, nb);
	}

	if (!card || !card->func) {
		BTMTK_ERR("card or function is NULL!");
		return -EINVAL;
	}

	len = nb - MTK_SDIO_PACKET_HEADER_SIZE;

	btmtk_sdio_dispatch_data_bluetooth_kpi(&txbuf[MTK_SDIO_PACKET_HEADER_SIZE], len, 0);

	MultiBluckCount = nb/SDIO_BLOCK_SIZE;
	redundant = nb % SDIO_BLOCK_SIZE;

	if (redundant)
		nb = (MultiBluckCount+1)*SDIO_BLOCK_SIZE;

	if (nb < 16)
		btmtk_print_buffer_conent(txbuf, nb);
	else
		btmtk_print_buffer_conent(txbuf, 16);

	do {
		/* Transfer data to card */
		ret = btmtk_sdio_writesb(CTDR, txbuf, nb);
		if (ret < 0) {
			i++;
			BTMTK_ERR("i=%d writesb failed: %d", i, ret);
			ret = -EIO;
			if (i > MAX_WRITE_IOMEM_RETRY)
				goto exit;
		}
	} while (ret);

	priv->btmtk_dev.tx_dnld_rdy = false;

exit:

	return ret;
}

static int btmtk_sdio_set_audio_slave(void)
{
	int ret = 0;
	u8 *cmd = NULL;
	u8 event[] = { 0x0E, 0x04, 0x01, 0x72, 0xFC, 0x00 };
#ifdef MTK_CHIP_PCM /* For PCM setting */
	u8 cmd_pcm[] = { 0x72, 0xFC, 0x04, 0x03, 0x10, 0x00, 0x4A };
#if SUPPORT_MT7668
	u8 cmd_7668[] = { 0x72, 0xFC, 0x04, 0x03, 0x10, 0x00, 0x4A };
#endif
#if SUPPORT_MT7663
	u8 cmd_7663[] = { 0x72, 0xFC, 0x04, 0x49, 0x00, 0x80, 0x00 };
#endif
#else /* For I2S setting */
#if SUPPORT_MT7668
	u8 cmd_7668[] = { 0x72, 0xFC, 0x04, 0x03, 0x10, 0x00, 0x02 };
#endif
#if SUPPORT_MT7663
	u8 cmd_7663[] = { 0x72, 0xFC, 0x04, 0x49, 0x00, 0x80, 0x00 };
#endif
#endif
	u8 mode = 0;

	BTMTK_INFO();
#if SUPPORT_MT7668
	if (is_mt7668(g_card)) {
		cmd = cmd_7668;
		mode = 0x08;
	}
#endif
#if SUPPORT_MT7663
	if (is_mt7663(g_card)) {
		cmd = cmd_7663;
		mode = 0x03;
	}
#endif
#ifdef MTK_CHIP_PCM
	if (!cmd) {
		BTMTK_ERR("cmd == null, set default PCM setting");
		cmd = cmd_pcm;
		mode |= 0x20;
	} else
		mode |= 0x10;
#endif

	BTMTK_INFO("set mode = 0x%x", mode);

	if (cmd) {
		ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, cmd[2] + 3,
			event, sizeof(event), COMP_EVENT_TIMO);
	} else
		BTMTK_ERR("No any audio cmd applied!!");

	return ret;
}

static int btmtk_sdio_read_pin_mux_setting(u32 *value)
{
	int ret = 0;
	u8 cmd[] = { 0xD1, 0xFC, 0x04, 0x54, 0x30, 0x02, 0x81 };
	u8 event[] = { 0x0E, 0x08, 0x01, 0xD1, 0xFC };

	BTMTK_INFO();

	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd),
		event, sizeof(event), COMP_EVENT_TIMO);

	if (ret)
		return ret;

	*value = (rxbuf[14] << 24) + (rxbuf[13] << 16) + (rxbuf[12] << 8) + rxbuf[11];
	BTMTK_DBG("value=%08x", *value);
	return ret;
}

static int btmtk_sdio_write_pin_mux_setting(u32 value)
{
	int ret = 0;
	u8 *cmd = NULL;
	u8 event[] = {0x0E, 0x04, 0x01, 0xD0, 0xFC, 0x00};
	u8 cmd_7668[] = {0xD0, 0xFC, 0x08,
		0x54, 0x30, 0x02, 0x81, 0x00, 0x00, 0x00, 0x00};
	u8 cmd_7663[] = {0xD0, 0xFC, 0x08,
		0x54, 0x50, 0x00, 0x78, 0x00, 0x10, 0x11, 0x01};

#if SUPPORT_MT7668
	if (is_mt7668(g_card))
		cmd = cmd_7668;
#endif
#if SUPPORT_MT7663
	if (is_mt7663(g_card))
		cmd = cmd_7663;
#endif
	if (!cmd) {
		BTMTK_INFO("not supported");
		return 0;
	}

	BTMTK_INFO("begin, value = 0x%08x", value);

	cmd[7] = (value & 0x000000FF);
	cmd[8] = ((value & 0x0000FF00) >> 8);
	cmd[9] = ((value & 0x00FF0000) >> 16);
	cmd[10] = ((value & 0xFF000000) >> 24);

	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, cmd[2] + 3,
		event, sizeof(event), COMP_EVENT_TIMO);

	return ret;
}

static int btmtk_sdio_set_audio_pin_mux(void)
{
	int ret = 0;
	u32 pinmux = 0;

	ret = btmtk_sdio_read_pin_mux_setting(&pinmux);
	if (ret) {
		BTMTK_ERR("btmtk_sdio_read_pin_mux_setting error(%d)", ret);
		return ret;
	}

#if SUPPORT_MT7668
	if (is_mt7668(g_card)) {
		pinmux &= 0x0000FFFF;
		pinmux |= 0x22220000;
	}
#endif
#if SUPPORT_MT7663
	if (is_mt7663(g_card)) {
		pinmux &= 0xF0000FFF;
		pinmux |= 0x01111000;
	}
#endif
	ret = btmtk_sdio_write_pin_mux_setting(pinmux);

	if (ret) {
		BTMTK_ERR("btmtk_sdio_write_pin_mux_setting error(%d)", ret);
		return ret;
	}

	pinmux = 0;
	ret = btmtk_sdio_read_pin_mux_setting(&pinmux);
	if (ret) {
		BTMTK_ERR("btmtk_sdio_read_pin_mux_setting error(%d)", ret);
		return ret;
	}
	BTMTK_INFO("confirm pinmux %04x", pinmux);

	return ret;
}

static int btmtk_send_rom_patch(u8 *fwbuf, u32 fwlen, int mode)
{
	int ret = 0;
	u8 mtksdio_packet_header[MTK_SDIO_PACKET_HEADER_SIZE] = {0};
	int stp_len = 0;
	u8 mtkdata_header[MTKDATA_HEADER_SIZE] = {0};

	int copy_len = 0;
	int Datalen = fwlen;
	u32 u32ReadCRValue = 0;


	BTMTK_DBG("fwlen %d, mode = %d", fwlen, mode);
	if (fwlen < Datalen) {
		BTMTK_ERR("file size = %d,is not corect", fwlen);
		return -ENOENT;
	}

	stp_len = Datalen + MTKDATA_HEADER_SIZE;


	mtkdata_header[0] = 0x2;/*ACL data*/
	mtkdata_header[1] = 0x6F;
	mtkdata_header[2] = 0xFC;

	mtkdata_header[3] = ((Datalen+4+1)&0x00FF);
	mtkdata_header[4] = ((Datalen+4+1)&0xFF00)>>8;

	mtkdata_header[5] = 0x1;
	mtkdata_header[6] = 0x1;

	mtkdata_header[7] = ((Datalen+1)&0x00FF);
	mtkdata_header[8] = ((Datalen+1)&0xFF00)>>8;

	mtkdata_header[9] = mode;

/* 0 and 1 is packet length, include MTKSTP_HEADER_SIZE */
	mtksdio_packet_header[0] =
		(Datalen+4+MTKSTP_HEADER_SIZE+6)&0xFF;
	mtksdio_packet_header[1] =
		((Datalen+4+MTKSTP_HEADER_SIZE+6)&0xFF00)>>8;
	mtksdio_packet_header[2] = 0;
	mtksdio_packet_header[3] = 0;

/*
 * mtksdio_packet_header[2] and mtksdio_packet_header[3]
 * are reserved
 */
	BTMTK_DBG("result %02x  %02x",
		((Datalen+4+MTKSTP_HEADER_SIZE+6)&0xFF00)>>8,
		(Datalen+4+MTKSTP_HEADER_SIZE+6));

	memcpy(txbuf+copy_len, mtksdio_packet_header,
		MTK_SDIO_PACKET_HEADER_SIZE);
	copy_len += MTK_SDIO_PACKET_HEADER_SIZE;

	memcpy(txbuf+copy_len, mtkdata_header, MTKDATA_HEADER_SIZE);
	copy_len += MTKDATA_HEADER_SIZE;

	memcpy(txbuf+copy_len, fwbuf, Datalen);
	copy_len += Datalen;

	BTMTK_DBG("txbuf %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		txbuf[0], txbuf[1], txbuf[2], txbuf[3], txbuf[4],
		txbuf[5], txbuf[6], txbuf[7], txbuf[8], txbuf[9]);


	ret = btmtk_sdio_readl(CHIER, &u32ReadCRValue);
	BTMTK_DBG("CHIER u32ReadCRValue %x, ret %d", u32ReadCRValue, ret);

	ret = btmtk_sdio_readl(CHLPCR, &u32ReadCRValue);
	BTMTK_DBG("CHLPCR u32ReadCRValue %x, ret %d", u32ReadCRValue, ret);

	ret = btmtk_sdio_readl(CHISR, &u32ReadCRValue);
	BTMTK_DBG("0CHISR u32ReadCRValue %x, ret %d", u32ReadCRValue, ret);
	ret = btmtk_sdio_readl(CHISR, &u32ReadCRValue);
	BTMTK_DBG("00CHISR u32ReadCRValue %x, ret %d", u32ReadCRValue, ret);

	btmtk_sdio_send_tx_data(txbuf, copy_len);

	ret = btmtk_sdio_recv_rx_data();

	return ret;
}

/*
 * type: cmd:1, ACL:2
 * -------------------------------------------------
 * mtksdio hedaer 4 byte| wmt header  |
 *
 *
 * data len should less than 512-4-4
 */
static int btmtk_sdio_send_wohci(u8 type, u32 len, u8 *data)
{
	u32 ret = 0;
	u32 push_in_data_len = 0;
	u32 MultiBluckCount = 0;
	u32 redundant = 0;
	u8 mtk_wmt_header[MTKWMT_HEADER_SIZE] = {0};
	u8 mtksdio_packet_header[MTK_SDIO_PACKET_HEADER_SIZE] = {0};
	u8 mtk_tx_data[512] = {0};

	mtk_wmt_header[0] = type;
	mtk_wmt_header[1] = 0x6F;
	mtk_wmt_header[2] = 0xFC;
	mtk_wmt_header[3] = len;

	mtksdio_packet_header[0] =
		(len+MTKWMT_HEADER_SIZE+MTK_SDIO_PACKET_HEADER_SIZE)&0xFF;
	mtksdio_packet_header[1] =
		((len+MTKWMT_HEADER_SIZE+MTK_SDIO_PACKET_HEADER_SIZE)&0xFF00)
		>>8;
	mtksdio_packet_header[2] = 0;
	mtksdio_packet_header[3] = 0;
/*
 * mtksdio_packet_header[2] and mtksdio_packet_header[3]
 * are reserved
 */

	memcpy(mtk_tx_data, mtksdio_packet_header,
		sizeof(mtksdio_packet_header));
	push_in_data_len += sizeof(mtksdio_packet_header);

	memcpy(mtk_tx_data+push_in_data_len, mtk_wmt_header,
		sizeof(mtk_wmt_header));
	push_in_data_len += sizeof(mtk_wmt_header);

	memcpy(mtk_tx_data+push_in_data_len, data, len);
	push_in_data_len += len;
	memcpy(txbuf, mtk_tx_data, push_in_data_len);

	MultiBluckCount = push_in_data_len/4;
	redundant = push_in_data_len % 4;
	if (redundant)
		push_in_data_len = (MultiBluckCount+1)*4;

	ret = btmtk_sdio_writesb(CTDR, txbuf, push_in_data_len);
	BTMTK_INFO("return  0x%0x", ret);
	return ret;
}

/*
 * data event:
 * return
 * 0:
 * patch download is not complete/get patch semaphore fail
 * 1:
 * patch download is complete by others
 * 2
 * patch download is not coplete
 * 3:(for debug)
 * release patch semaphore success
 */
static int btmtk_sdio_need_load_rom_patch(void)
{
	u32 ret = -1;
	u8 cmd[] = {0x1, 0x17, 0x1, 0x0, 0x1};
	u8 event[] = {0x2, 0x17, 0x1, 0x0};

	do {
		ret = btmtk_sdio_send_wohci(HCI_COMMAND_PKT, sizeof(cmd), cmd);

		if (ret) {
			BTMTK_ERR("btmtk_sdio_send_wohci return fail ret %d", ret);
			break;
		}

		ret = btmtk_sdio_recv_rx_data();
		if (ret)
			break;

		if (rx_length == 12) {
			if (memcmp(rxbuf+7, event, sizeof(event)) == 0)
				return rxbuf[11];

			BTMTK_ERR("receive event content is not correct, print receive data");
			btmtk_print_buffer_conent(rxbuf, rx_length);
		}
	} while (0);
	BTMTK_ERR("return ret %d", ret);
	return ret;
}

static int btmtk_sdio_set_write_clear(void)
{
	u32 u32ReadCRValue = 0;
	u32 ret = 0;

	ret = btmtk_sdio_readl(CHCR, &u32ReadCRValue);
	if (ret) {
		BTMTK_ERR("read CHCR error");
		ret = EINVAL;
		return ret;
	}

	u32ReadCRValue |= 0x00000002;
	btmtk_sdio_writel(CHCR, u32ReadCRValue);
	BTMTK_INFO("write CHCR 0x%08X", u32ReadCRValue);
	ret = btmtk_sdio_readl(CHCR, &u32ReadCRValue);
	BTMTK_INFO("read CHCR 0x%08X", u32ReadCRValue);
	if (u32ReadCRValue&0x00000002)
		BTMTK_INFO("write clear");
	else
		BTMTK_INFO("read clear");

	return ret;
}

static int btmtk_sdio_set_audio(void)
{
	int ret = 0;

	ret = btmtk_sdio_set_audio_slave();
	if (ret) {
		BTMTK_ERR("btmtk_sdio_set_audio_slave error(%d)", ret);
		return ret;
	}

	ret = btmtk_sdio_set_audio_pin_mux();
	if (ret) {
		BTMTK_ERR("btmtk_sdio_set_audio_pin_mux error(%d)", ret);
		return ret;
	}

	return ret;
}

static int btmtk_sdio_send_audio_slave(void)
{
	int ret = 0;

	ret = btmtk_sdio_set_audio_slave();
	if (ret) {
		BTMTK_ERR("btmtk_sdio_set_audio_slave error(%d)", ret);
		return ret;
	}

	if (is_mt7663(g_card)) {
		ret = btmtk_sdio_set_audio_pin_mux();
		if (ret) {
			BTMTK_ERR("btmtk_sdio_set_audio_pin_mux error(%d)", ret);
			return ret;
		}
	}

	return ret;
}

static int btmtk_sdio_download_rom_patch(struct btmtk_sdio_card *card)
{
	const struct firmware *fw_firmware = NULL;
	int firmwarelen, ret = 0;
	u8 *fwbuf;
	struct _PATCH_HEADER *patchHdr;
	u16 u2HwVer = 0;
	u16 u2SwVer = 0;
	u32 u4PatchVer = 0;
	u32 u4FwVersion = 0;
	u32 u4ChipId = 0;
	u32 u32ReadCRValue = 0;
	u8  patch_status = 0;
	char strDateTime[17];
	bool load_sysram3 = false;
	int retry = 20;

	ret = btmtk_sdio_set_own_back(DRIVER_OWN);
	if (ret)
		goto done;

	u4FwVersion = btmtk_sdio_bt_memRegister_read(FW_VERSION);
	BTMTK_INFO("Fw Version 0x%x", u4FwVersion);
	u4ChipId = btmtk_sdio_bt_memRegister_read(CHIP_ID);
	BTMTK_INFO("Chip Id 0x%x", u4ChipId);

	if ((u4FwVersion & 0xff) == 0xff) {
		BTMTK_ERR("failed ! wrong fw version : 0x%x", u4FwVersion);
		ret = -ENODEV;
		goto done;

	} else {
		u8 uFirmwareName[MAX_BIN_FILE_NAME_LEN] = {0};

		/* Bin filename format : "mt$$$$_patch_e%.bin" */
		/*     $$$$ : chip id */
		/*     % : fw version + 1 (in HEX) */
		snprintf(uFirmwareName, MAX_BIN_FILE_NAME_LEN, "mt%04x_patch_e%x_hdr.bin",
				u4ChipId & 0xffff, (u4FwVersion & 0x0ff) + 1);
		BTMTK_INFO("request_firmware(firmware name %s)", uFirmwareName);
		ret = request_firmware(&fw_firmware, uFirmwareName,
				&card->func->dev);

		if ((ret < 0) || !fw_firmware) {
			BTMTK_ERR("request_firmware(firmware name %s) failed, error code = %d",
					uFirmwareName,
					ret);
			ret = -ENOENT;
			goto done;
		}
	}
	memset(fw_version_str, 0, FW_VERSION_BUF_SIZE);
	if ((fw_firmware->data[8] >= '0') && (fw_firmware->data[8] <= '9'))
		memcpy(fw_version_str, fw_firmware->data, FW_VERSION_SIZE - 1);
	else
		sprintf(fw_version_str, "%.4s-%.2s-%.2s.%.1s.%.2s.%.1s.%.1s.%.2s",
			fw_firmware->data, fw_firmware->data + 4, fw_firmware->data + 6,
			fw_firmware->data + 8, fw_firmware->data + 9,
			fw_firmware->data + 11, fw_firmware->data + 12,
			fw_firmware->data + 13);

#if SUPPORT_MT7668
	if (is_mt7668(g_card))
		load_sysram3 =
			(fw_firmware->size > (PATCH_INFO_SIZE + PATCH_LEN_ILM))
				? true : false;
#endif

	do {
		patch_status = btmtk_sdio_need_load_rom_patch();
		BTMTK_DBG("patch_status %d", patch_status);

		if (patch_status > PATCH_NEED_DOWNLOAD || patch_status < 0) {
			BTMTK_ERR("patch_status error");
			ret = -ENODEV;
			goto done;
		} else if (patch_status == PATCH_READY) {
			BTMTK_INFO("patch is ready no need load patch again");
			if (!load_sysram3)
				goto patch_end;
			else
				goto sysram3;
		} else if (patch_status == PATCH_IS_DOWNLOAD_BY_OTHER) {
			msleep(100);
			retry--;
		} else if (patch_status == PATCH_NEED_DOWNLOAD) {
			if (is_mt7663(g_card)) {
				if (btmtk_sdio_send_wmt_cfg())
					BTMTK_ERR("send wmt cfg failed!");
			}
			break;  /* Download ROM patch directly */
		}
	} while (retry > 0);

	if (patch_status == PATCH_IS_DOWNLOAD_BY_OTHER) {
		BTMTK_WARN("Hold by another fun more than 2 seconds");
		ret = -ENODEV;
		goto done;
	}

	fwbuf = (u8 *)fw_firmware->data;

	/*Display rom patch info*/
	patchHdr =  (struct _PATCH_HEADER *)fwbuf;
	memcpy(strDateTime, patchHdr->ucDateTime, sizeof(patchHdr->ucDateTime));
	strDateTime[16] = '\0';
	u2HwVer = patchHdr->u2HwVer;
	u2SwVer = patchHdr->u2SwVer;
	u4PatchVer = patchHdr->u4PatchVer;

	BTMTK_INFO("[btmtk] =============== Patch Info ==============");
	BTMTK_INFO("[btmtk] Built Time = %s", strDateTime);
	BTMTK_INFO("[btmtk] Hw Ver = 0x%x",
			((u2HwVer & 0x00ff) << 8) | ((u2HwVer & 0xff00) >> 8));
	BTMTK_INFO("[btmtk] Sw Ver = 0x%x",
			((u2SwVer & 0x00ff) << 8) | ((u2SwVer & 0xff00) >> 8));
	BTMTK_INFO("[btmtk] Patch Ver = 0x%04x",
			((u4PatchVer & 0xff000000) >> 24) |
			((u4PatchVer & 0x00ff0000) >> 16));

	BTMTK_INFO("[btmtk] Platform = %c%c%c%c",
			patchHdr->ucPlatform[0],
			patchHdr->ucPlatform[1],
			patchHdr->ucPlatform[2],
			patchHdr->ucPlatform[3]);
	BTMTK_INFO("[btmtk] Patch start addr = %02x", patchHdr->u2PatchStartAddr);
	BTMTK_INFO("[btmtk] =========================================");

	firmwarelen = load_sysram3 ?
			PATCH_LEN_ILM :	(fw_firmware->size - PATCH_INFO_SIZE);

	BTMTK_INFO("loading ILM rom patch...");
	ret = btmtk_sdio_download_partial_rom_patch(fwbuf, firmwarelen);
	BTMTK_INFO("loading ILM rom patch... Done");

	if (btmtk_sdio_need_load_rom_patch() == PATCH_READY) {
		BTMTK_INFO("patchdownload is done by BT");
	} else {
		/* TODO: Need error handling here*/
		BTMTK_WARN("patchdownload download by BT, not ready");
	}

	/* CHIP_RESET, ROM patch would be reactivated.
	 * Currently, wmt reset is only for ILM rom patch, and there are also
	 * some preparations need to be done in FW for loading sysram3 patch...
	 */
	ret = btmtk_sdio_send_wmt_reset();
	if (ret)
		goto done;

sysram3:
	if (load_sysram3) {
		firmwarelen = fw_firmware->size - PATCH_INFO_SIZE
			- PATCH_LEN_ILM - PATCH_INFO_SIZE;
		fwbuf = (u8 *)fw_firmware->data + PATCH_INFO_SIZE
			+ PATCH_LEN_ILM;
		BTMTK_INFO("loading sysram3 rom patch...");
		ret = btmtk_sdio_download_partial_rom_patch(fwbuf, firmwarelen);
		BTMTK_INFO("loading sysram3 rom patch... Done");
	}

patch_end:
	ret = btmtk_sdio_readl(0, &u32ReadCRValue);
	BTMTK_INFO("read chipid =  %x", u32ReadCRValue);

	/*Set interrupt output*/
	ret = btmtk_sdio_writel(CHIER, FIRMWARE_INT|TX_FIFO_OVERFLOW |
			FW_INT_IND_INDICATOR | TX_COMPLETE_COUNT |
			TX_UNDER_THOLD | TX_EMPTY | RX_DONE);
	if (ret) {
		BTMTK_ERR("Set interrupt output fail(%d)", ret);
		ret = -EIO;
		goto done;
	}

	/*enable interrupt output*/
	ret = btmtk_sdio_writel(CHLPCR, C_FW_INT_EN_SET);
	if (ret) {
		BTMTK_ERR("enable interrupt output fail(%d)", ret);
		ret = -EIO;
		goto done;
	}

	btmtk_sdio_set_write_clear();

done:
	btmtk_sdio_set_own_back(FW_OWN);

	if (fw_firmware)
		release_firmware(fw_firmware);

	if (!ret)
		BTMTK_INFO("success");
	else
		BTMTK_INFO("fail");

	return ret;
}

static int btmtk_sdio_download_partial_rom_patch(u8 *fwbuf, int firmwarelen)
{
	int ret = 0;
	int RedundantSize = 0;
	u32 bufferOffset = 0;

	BTMTK_INFO("Downloading FW image (%d bytes)", firmwarelen);

	fwbuf += PATCH_INFO_SIZE;
	BTMTK_DBG("PATCH_HEADER size %d", PATCH_INFO_SIZE);

	RedundantSize = firmwarelen;
	BTMTK_DBG("firmwarelen %d", firmwarelen);

	do {
		bufferOffset = firmwarelen - RedundantSize;

		if (RedundantSize == firmwarelen &&
				RedundantSize >= PATCH_DOWNLOAD_SIZE)
			ret = btmtk_send_rom_patch(fwbuf + bufferOffset,
					PATCH_DOWNLOAD_SIZE,
					SDIO_PATCH_DOWNLOAD_FIRST);
		else if (RedundantSize == firmwarelen)
			ret = btmtk_send_rom_patch(fwbuf + bufferOffset,
					RedundantSize,
					SDIO_PATCH_DOWNLOAD_FIRST);
		else if (RedundantSize < PATCH_DOWNLOAD_SIZE) {
			ret = btmtk_send_rom_patch(fwbuf + bufferOffset,
					RedundantSize,
					SDIO_PATCH_DOWNLOAD_END);
			BTMTK_DBG("patch downoad last patch part");
		} else
			ret = btmtk_send_rom_patch(fwbuf + bufferOffset,
					PATCH_DOWNLOAD_SIZE,
					SDIO_PATCH_DOWNLOAD_CON);

		RedundantSize -= PATCH_DOWNLOAD_SIZE;

		if (ret) {
			BTMTK_ERR("btmtk_send_rom_patch fail");
			return ret;
		}
		BTMTK_DBG("RedundantSize %d", RedundantSize);
		if (RedundantSize <= 0) {
			BTMTK_DBG("patch downoad finish");
			break;
		}
	} while (1);

	return ret;
}

static void btmtk_sdio_close_coredump_file(void)
{
	BTMTK_DBG("vfs_fsync");

	if (g_card->bt_cfg.save_fw_dump_in_kernel && fw_dump_file)
		vfs_fsync(fw_dump_file, 0);

	if (fw_dump_file) {
		BTMTK_INFO("close file  %s", g_card->bt_cfg.fw_dump_file_name);
		if (g_card->bt_cfg.save_fw_dump_in_kernel)
			filp_close(fw_dump_file, NULL);
		fw_dump_file = NULL;
	} else {
		BTMTK_WARN("fw_dump_file is NULL can't close file %s", g_card->bt_cfg.fw_dump_file_name);
	}
}

static void btmtk_sdio_stop_wait_dump_complete_thread(void)
{
	if (IS_ERR(wait_dump_complete_tsk) || wait_dump_complete_tsk == NULL)
		BTMTK_ERR("wait_dump_complete_tsk is error");
	else {
		kthread_stop(wait_dump_complete_tsk);
		wait_dump_complete_tsk = NULL;
	}
}

static int btmtk_sdio_card_to_host(struct btmtk_private *priv, const u8 *event, const int event_len,
	int add_spec_header)
/*event: check event which want to compare*/
/*return value: -x fail, 0 success*/
{
	u16 buf_len = 0;
	int ret = 0;
	struct sk_buff *skb = NULL;
	struct sk_buff *fops_skb = NULL;
	u32 type;
	u32 fourbalignment_len = 0;
	u32 dump_len = 0;
	char *core_dump_end = NULL;
	int i = 0;
	u16 retry = 0;
	u32 u32ReadCRValue = 0;
	u8 is_fwdump = 0, tail_len = 0;
	int fops_state = 0;
	static u8 picus_blocking_warn;
	static u8 fwdump_blocking_warn;

	if (rx_length > (MTK_SDIO_PACKET_HEADER_SIZE + 1)) {
		buf_len = rx_length - (MTK_SDIO_PACKET_HEADER_SIZE + 1);
	} else {
		BTMTK_ERR("rx_length error(%d)", rx_length);
		return -EINVAL;
	}

	/* Core dump packet format:
	 * A0 00 00 00 80 AA BB CC 02 6F FC YY ZZ XX ... XX 00 00 00 00
	 * A0 00 00 00: SDIO Header
	 * 80 AA BB CC: STP Header
	 * 02 6F FC: Core dump header
	 * YY ZZ: Coredump length
	 * 00 00 00 00: STP CRC and aligment to multiple of 4 by SDIO
	 */
	if (rx_length > (COREDUMP_PACKET_HEADER_LEN) &&
		rxbuf[SDIO_HEADER_LEN] == 0x80 &&
		rxbuf[SDIO_HEADER_LEN + STP_HEADER_LEN + 1] == 0x6F &&
		rxbuf[SDIO_HEADER_LEN + STP_HEADER_LEN + 2] == 0xFC) {

		dump_len = rxbuf[SDIO_HEADER_LEN + STP_HEADER_LEN + 3]
			+ (rxbuf[SDIO_HEADER_LEN + STP_HEADER_LEN + 4] << 8);
		BTMTK_DBG("get dump len %d", dump_len);

		dump_data_counter++;
		/* Total dump length to fw_dump_files */
		dump_data_length += dump_len;
		is_fwdump = 1;

		if (dump_data_counter % 1000 == 0)
			BTMTK_WARN("coredump on-going, total_packet = %d, total_length = %d",
					dump_data_counter, dump_data_length);

		if (dump_data_counter < PRINT_DUMP_COUNT) {
			BTMTK_WARN("dump %d %s",
				dump_data_counter,
				&rxbuf[COREDUMP_PACKET_HEADER_LEN]);
		/* release mode do reset dongle if print dump finish */
		} else if (!g_card->bt_cfg.support_full_fw_dump &&
			dump_data_counter == PRINT_DUMP_COUNT) {
			/* create dump file fail and is user mode */
			BTMTK_INFO("user mode, do reset after print dump done %d", dump_data_counter);
			picus_blocking_warn = 0;
			fwdump_blocking_warn = 0;
			btmtk_sdio_close_coredump_file();
			btmtk_sdio_stop_wait_dump_complete_thread();
			goto exit;
		}

		if (dump_data_counter == 1) {
			g_card->dongle_state = BT_SDIO_DONGLE_STATE_FW_DUMP;
			btmtk_sdio_hci_snoop_print();
			BTMTK_INFO("create btmtk_sdio_wait_dump_complete_thread");
			wait_dump_complete_tsk = kthread_run(btmtk_sdio_wait_dump_complete_thread,
				NULL, "btmtk_sdio_wait_dump_complete_thread");

			msleep(100);
			if (!wait_dump_complete_tsk)
				BTMTK_ERR("wait_dump_complete_tsk is NULL");

			btmtk_sdio_notify_wlan_remove_start();
			btmtk_sdio_set_no_fw_own(g_priv, TRUE);

			if (g_card->bt_cfg.save_fw_dump_in_kernel) {
				BTMTK_WARN("open file %s",
					g_card->bt_cfg.fw_dump_file_name);
				fw_dump_file = filp_open(g_card->bt_cfg.fw_dump_file_name, O_RDWR | O_CREAT, 0644);

				if (!(IS_ERR(fw_dump_file))) {
					BTMTK_WARN("open file %s success",
						g_card->bt_cfg.fw_dump_file_name);
				} else {
					BTMTK_WARN("open file %s fail",
						g_card->bt_cfg.fw_dump_file_name);
					fw_dump_file = NULL;
				}

				if (fw_dump_file && fw_dump_file->f_op == NULL) {
					BTMTK_WARN("%s fw_dump_file->f_op is NULL, close",
						g_card->bt_cfg.fw_dump_file_name);
					filp_close(fw_dump_file, NULL);
					fw_dump_file = NULL;
				}

				if (fw_dump_file && fw_dump_file->f_op->write == NULL) {
					BTMTK_WARN("%s fw_dump_file->f_op->write is NULL, close",
						g_card->bt_cfg.fw_dump_file_name);
					filp_close(fw_dump_file, NULL);
					fw_dump_file = NULL;
				}
			}
		}

		if (g_card->bt_cfg.save_fw_dump_in_kernel && (dump_len > 0)
			&& fw_dump_file && fw_dump_file->f_op && fw_dump_file->f_op->write)
			fw_dump_file->f_op->write(fw_dump_file, &rxbuf[COREDUMP_PACKET_HEADER_LEN],
				dump_len, &fw_dump_file->f_pos);

		if (skb_queue_len(&g_card->fwlog_fops_queue) < FWLOG_ASSERT_QUEUE_COUNT) {
			/* This is coredump data, save coredump data to picus_queue */
			BTMTK_DBG("Receive coredump data, move data to fwlog queue for picus");
			/* Save coredump data to picus_queue from 6F FC, minus ACL header */
			btmtk_sdio_dispatch_fwlog(&rxbuf[SDIO_HEADER_LEN + STP_HEADER_LEN + 1],
				dump_len + COREDUMP_HEADER_LEN - HCI_TYPE_LEN);
			fwdump_blocking_warn = 0;
		} else if (fwdump_blocking_warn == 0) {
			fwdump_blocking_warn = 1;
			BTMTK_WARN("btmtk_sdio FW dump queue size is full");
		}

		/* Modify header to ACL format, handle is 0xFFF0
		 * Core dump header:
		 * 80 AA BB CC DD 6F FC XX XX XX ......
		 * 80 AA BB CC	-> STP header, droped, 4 bytes extra at tailed for CRC
		 * DD		-> 02 (ACL TYPE)
		 * 6F FC	-> FF F0
		 */
		rxbuf[SDIO_HEADER_LEN + 4] = HCI_ACLDATA_PKT;
		rxbuf[SDIO_HEADER_LEN + 5] = 0xFF;
		rxbuf[SDIO_HEADER_LEN + 6] = 0xF0;

		if (dump_len >= strlen(FW_DUMP_END_EVENT)) {
			core_dump_end = strstr(&rxbuf[SDIO_HEADER_LEN + 10],
					FW_DUMP_END_EVENT);

			if (core_dump_end) {
				BTMTK_WARN("core_dump_end %s, total_packet = %d, total_length = %d",
				    core_dump_end, dump_data_counter, dump_data_length);
				BTMTK_WARN("rxbuf = %02x %02x %02x",
					rxbuf[4], rxbuf[5], rxbuf[6]);
				sdio_claim_host(g_card->func);
				sdio_release_irq(g_card->func);
				sdio_release_host(g_card->func);
				dump_data_counter = 0;
				dump_data_length = 0;
				picus_blocking_warn = 0;
				fwdump_blocking_warn = 0;
				btmtk_sdio_close_coredump_file();
				btmtk_sdio_stop_wait_dump_complete_thread();
			}
		}
	} else if (rx_length > (SDIO_HEADER_LEN + 4) &&
			((rxbuf[SDIO_HEADER_LEN] == 0x04 &&
			  rxbuf[SDIO_HEADER_LEN + 1] == 0xFF &&
			  rxbuf[SDIO_HEADER_LEN + 3] == 0x50) ||
			(rxbuf[SDIO_HEADER_LEN] == 0x02 &&
			 rxbuf[SDIO_HEADER_LEN + 1] == 0xFF &&
			 rxbuf[SDIO_HEADER_LEN + 2] == 0x05))) {
		 /*receive picus data to fwlog_queue*/
		if (rxbuf[SDIO_HEADER_LEN] == 0x04) {
			dump_len = rxbuf[SDIO_HEADER_LEN + 2] - 1;
			buf_len = dump_len + 3;
		} else {
			dump_len = ((rxbuf[SDIO_HEADER_LEN + 4] & 0x0F) << 8) + rxbuf[SDIO_HEADER_LEN + 3];
			buf_len = dump_len + 4;
		}
		BTMTK_DBG("This is debug log data, length = %d", dump_len);
		if (rx_length < (buf_len + MTK_SDIO_PACKET_HEADER_SIZE + 1))
			goto data_err;
		btmtk_sdio_dispatch_fwlog(&rxbuf[MTK_SDIO_PACKET_HEADER_SIZE + 1], buf_len);
		btmtk_sdio_hci_snoop_save(FW_LOG_PKT, &rxbuf[MTK_SDIO_PACKET_HEADER_SIZE + 1], buf_len);
		goto exit;
	} else if (rxbuf[SDIO_HEADER_LEN] == 0x04
			&& rxbuf[SDIO_HEADER_LEN + 1] == 0x0E
			&& rxbuf[SDIO_HEADER_LEN + 2] == 0x04
			&& rxbuf[SDIO_HEADER_LEN + 3] == 0x01
			&& rxbuf[SDIO_HEADER_LEN + 4] == 0x02
			&& rxbuf[SDIO_HEADER_LEN + 5] == 0xFD) {
		BTMTK_ERR("This is btclk event, status:%02x", rxbuf[SDIO_HEADER_LEN + 6]);
		buf_len = rx_length - (MTK_SDIO_PACKET_HEADER_SIZE + 1);
		goto exit;
	} else if (rx_length >= (SDIO_HEADER_LEN + 13)
			&& rxbuf[SDIO_HEADER_LEN] == 0x04
			&& rxbuf[SDIO_HEADER_LEN + 1] == 0xFF
			&& rxbuf[SDIO_HEADER_LEN + 3] == 0x41) {
		/* receive BT clock data */
		BTMTK_DBG("This is btclk data - %d", rx_length);
		BTMTK_DBG("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			rxbuf[SDIO_HEADER_LEN + 0], rxbuf[SDIO_HEADER_LEN + 1], rxbuf[SDIO_HEADER_LEN + 2],
			rxbuf[SDIO_HEADER_LEN + 3], rxbuf[SDIO_HEADER_LEN + 4], rxbuf[SDIO_HEADER_LEN + 5],
			rxbuf[SDIO_HEADER_LEN + 6], rxbuf[SDIO_HEADER_LEN + 7], rxbuf[SDIO_HEADER_LEN + 8],
			rxbuf[SDIO_HEADER_LEN + 9], rxbuf[SDIO_HEADER_LEN + 10], rxbuf[SDIO_HEADER_LEN + 11],
			rxbuf[SDIO_HEADER_LEN + 12], rxbuf[SDIO_HEADER_LEN + 13], rxbuf[SDIO_HEADER_LEN + 14],
			rxbuf[SDIO_HEADER_LEN + 15], rxbuf[SDIO_HEADER_LEN + 16], rxbuf[SDIO_HEADER_LEN + 17]);

		if (rxbuf[SDIO_HEADER_LEN + 12] == 0x0) {
			u32 intra_clk = 0, clk = 0;

			memcpy(&intra_clk, &rxbuf[SDIO_HEADER_LEN + 6], 2);
			memcpy(&clk, &rxbuf[SDIO_HEADER_LEN + 8], 4);

			LOCK_UNSLEEPABLE_LOCK(&stereo_spin_lock);
			stereo_clk.fw_clk = (u64)(intra_clk + (clk & 0x0FFFFFFC) * 3125 / 10);
			stereo_clk.sys_clk = sys_clk_tmp;
			UNLOCK_UNSLEEPABLE_LOCK(&stereo_spin_lock);
			BTMTK_DBG("btclk intra:%x, clk:%x, fw_clk:%llu, sysclk: %llu",
				intra_clk, clk, stereo_clk.fw_clk, stereo_clk.sys_clk);
		} else {
			BTMTK_WARN("No ACL CONNECTION(%d), disable event and interrupt",
				rxbuf[SDIO_HEADER_LEN + 12]);
		}

		buf_len = rx_length - (MTK_SDIO_PACKET_HEADER_SIZE + 1);
		goto exit;
	} else if (rxbuf[SDIO_HEADER_LEN] == HCI_EVENT_PKT &&
			rxbuf[SDIO_HEADER_LEN + 1] == 0x0E &&
			rxbuf[SDIO_HEADER_LEN + 4] == 0x03 &&
			rxbuf[SDIO_HEADER_LEN + 5] == 0x0C &&
			rxbuf[SDIO_HEADER_LEN + 6] == 0x00) {
		BTMTK_INFO("get hci reset");
		get_hci_reset = 1;
	}

	btmtk_print_buffer_conent(rxbuf, rx_length);

	/* Read the length of data to be transferred , not include pkt type*/
	buf_len = rx_length - (MTK_SDIO_PACKET_HEADER_SIZE + 1);

	BTMTK_DBG("buf_len : %d", buf_len);
	if (rx_length <= SDIO_HEADER_LEN) {
		BTMTK_WARN("invalid packet length: %d", buf_len);
		ret = -EINVAL;
		goto exit;
	}

	/* Allocate buffer */
	/* rx_length = num_blocks * blksz + BTSDIO_DMA_ALIGN*/
	skb = bt_skb_alloc(rx_length, GFP_ATOMIC);
	if (skb == NULL) {
		BTMTK_WARN("No free skb");
		ret = -ENOMEM;
		goto exit;
	}

	BTMTK_DBG("rx_length %d,buf_len %d", rx_length, buf_len);

	if (is_fwdump == 0) {
		memcpy(skb->data, &rxbuf[MTK_SDIO_PACKET_HEADER_SIZE + 1], buf_len);
		type = rxbuf[MTK_SDIO_PACKET_HEADER_SIZE];
	} else {
		memcpy(skb->data, &rxbuf[MTK_SDIO_PACKET_HEADER_SIZE + 5], buf_len - 4);
		type = rxbuf[MTK_SDIO_PACKET_HEADER_SIZE + 4];
	}

	switch (type) {
	case HCI_ACLDATA_PKT:
		BTMTK_DBG("data[2] 0x%02x, data[3] 0x%02x"
			, skb->data[2], skb->data[3]);
		buf_len = skb->data[2] + skb->data[3] * 256 + 4;
		BTMTK_DBG("acl buf_len %d", buf_len);
		break;
	case HCI_SCODATA_PKT:
		buf_len = skb->data[2] + 3;
		break;
	case HCI_EVENT_PKT:
		buf_len = skb->data[1] + 2;
		break;
	default:
		BTSDIO_INFO_RAW(skb->data, buf_len, "CHISR(0x%08X) skb->data(type %d):", reg_CHISR, type);

		for (retry = 0; retry < 5; retry++) {
			ret = btmtk_sdio_readl(SWPCDBGR, &u32ReadCRValue);
			BTMTK_INFO("ret %d, SWPCDBGR 0x%x, and not sleep!", ret, u32ReadCRValue);
		}
		btmtk_sdio_print_debug_sr();

		/* trigger fw core dump */
		FOPS_MUTEX_LOCK();
		fops_state = btmtk_fops_get_state();
		FOPS_MUTEX_UNLOCK();
		if (fops_state == BTMTK_FOPS_STATE_OPENED)
			btmtk_sdio_trigger_fw_assert();
		ret = -EINVAL;
		goto exit;
	}

	if (buf_len > MTK_RXDATA_SIZE) {
		BTMTK_ERR("buf_len %d is invalid, more than %d", buf_len, MTK_RXDATA_SIZE);
		ret = -EINVAL;
		goto exit;
	}

	if ((buf_len >= sizeof(READ_ADDRESS_EVENT))
		&& (event_compare_status == BTMTK_SDIO_EVENT_COMPARE_STATE_NEED_COMPARE)) {
		if ((memcmp(skb->data, READ_ADDRESS_EVENT, sizeof(READ_ADDRESS_EVENT)) == 0) && (buf_len == 12)) {
			for (i = 0; i < BD_ADDRESS_SIZE; i++)
				g_card->bdaddr[i] = skb->data[6 + i];

			BTMTK_DBG("GET TV BDADDR = %02X:%02X:%02X:%02X:%02X:%02X",
			g_card->bdaddr[0], g_card->bdaddr[1], g_card->bdaddr[2],
			g_card->bdaddr[3], g_card->bdaddr[4], g_card->bdaddr[5]);

			/*
			 * event_compare_status =
			 * BTMTK_SDIO_EVENT_COMPARE_STATE_COMPARE_SUCCESS;
			 */
		} else
			BTMTK_DBG("READ_ADDRESS_EVENT compare fail buf_len %d", buf_len);
	}

	if (event_compare_status == BTMTK_SDIO_EVENT_COMPARE_STATE_NEED_COMPARE) {
		if (buf_len >= event_need_compare_len) {
			if (memcmp(skb->data, event_need_compare, event_need_compare_len) == 0) {
				event_compare_status = BTMTK_SDIO_EVENT_COMPARE_STATE_COMPARE_SUCCESS;
				BTMTK_DBG("compare success");
				/* Drop by driver, don't send to stack */
				goto exit;

			} else {
				BTMTK_DBG("%s compare fail", __func__);
		BTSDIO_INFO_RAW(event_need_compare, event_need_compare_len,
			"%s: event_need_compare:", __func__);
		BTSDIO_INFO_RAW(skb->data, buf_len,
			"%s: skb->data:", __func__);
			}
		}
	}

	if (is_fwdump == 0) {
		btmtk_sdio_hci_snoop_save(type, skb->data, buf_len);
		btmtk_sdio_dispatch_data_bluetooth_kpi(&rxbuf[MTK_SDIO_PACKET_HEADER_SIZE], buf_len + 1, 0);
	}

#if 0
	/* to drop picus related event after save event, don't send picus event to host,
	 * because host will trace this event as other host cmd's event,
	 * it will cause command timeout
	 */
	if ((skb->data[3] == 0x5F || skb->data[3] == 0xBE) && skb->data[4] == 0xFC) {
		BTSDIO_INFO_RAW(skb->data, buf_len, "%s: discard picus related event:", __func__);
		goto exit;
	}
#endif

	fops_skb = bt_skb_alloc(buf_len, GFP_ATOMIC);
	if (fops_skb == NULL) {
		BTMTK_WARN("No free fops_skb");
		ret = -ENOMEM;
		goto exit;
	}

	bt_cb(fops_skb)->pkt_type = type;
	memcpy(fops_skb->data, skb->data, buf_len);

	fops_skb->len = buf_len;
	LOCK_UNSLEEPABLE_LOCK(&(metabuffer.spin_lock));
	skb_queue_tail(&g_card->fops_queue, fops_skb);
	if (skb_queue_empty(&g_card->fops_queue))
		BTMTK_INFO("fops_queue is empty");
	UNLOCK_UNSLEEPABLE_LOCK(&(metabuffer.spin_lock));

	wake_up_interruptible(&inq);

exit:
	if (skb) {
		BTMTK_DBG("fail free skb");
		kfree_skb(skb);
	}

	if (is_fwdump == 1) {
		/*
		 * This is a fw issue
		 * Fw will send some bytes extra 0x00 at tail of coredump packet
		 * It will cause alignment failed
		 * Workaround is that use buf_len with bytes extra 0x00 added
		 * bytes extra 0x00 = Total length - buf_len - header length
		 * 9 = 4(SDIO header) + 4(STP header) + 1(type)
		 */
		tail_len = rx_length - buf_len - 9;
		buf_len += (4 + tail_len);
	}
	buf_len += 1;
	if (buf_len % 4)
		fourbalignment_len = buf_len + 4 - (buf_len % 4);
	else
		fourbalignment_len = buf_len;

	if (rx_length < fourbalignment_len)
		goto data_err;

	rx_length -= fourbalignment_len;

	if (rx_length > (MTK_SDIO_PACKET_HEADER_SIZE)) {
		memcpy(&rxbuf[MTK_SDIO_PACKET_HEADER_SIZE],
		&rxbuf[MTK_SDIO_PACKET_HEADER_SIZE + fourbalignment_len],
		rx_length - MTK_SDIO_PACKET_HEADER_SIZE);
	}

	BTMTK_DBG("ret %d, rx_length, %d,fourbalignment_len %d <--",
		ret, rx_length, fourbalignment_len);

	return ret;

data_err:
	BTMTK_ERR("data error!!! discard rxbuf:");
	BTSDIO_INFO_RAW(rxbuf, rx_length, "rxbuf");
	rx_length = MTK_SDIO_PACKET_HEADER_SIZE;
	return -EINVAL;
}

static int btmtk_sdio_process_int_status(
		struct btmtk_private *priv)
{
	int ret = 0;
	u32 u32rxdatacount = 0;
	u32 u32ReadCRValue = 0;
	btmtk_sdio_timestamp(BTMTK_SDIO_RX_CHECKPOINT_RX_START);

	ret = btmtk_sdio_readl(CHISR, &u32ReadCRValue);
	BTMTK_DBG("CHISR 0x%08x", u32ReadCRValue);
	if (u32ReadCRValue & FIRMWARE_INT_BIT15) {
		btmtk_sdio_set_no_fw_own(g_priv, TRUE);
		btmtk_sdio_writel(CHISR, FIRMWARE_INT_BIT15);
		BTMTK_DBG("CHISR 0x%08x", u32ReadCRValue);
	}

	BTMTK_DBG("check TX_EMPTY CHISR 0x%08x", u32ReadCRValue);
	if (TX_EMPTY&u32ReadCRValue) {
		ret = btmtk_sdio_writel(CHISR, (TX_EMPTY | TX_COMPLETE_COUNT));
		priv->btmtk_dev.tx_dnld_rdy = true;
		BTMTK_DBG("set tx_dnld_rdy 1");
	}

	if (RX_DONE&u32ReadCRValue)
		ret = btmtk_sdio_recv_rx_data();

	if (ret == 0) {
		btmtk_sdio_timestamp(BTMTK_SDIO_RX_CHECKPOINT_RX_DONE);
		while (rx_length > (MTK_SDIO_PACKET_HEADER_SIZE)) {
			btmtk_sdio_card_to_host(priv, NULL, -1, 0);
			u32rxdatacount++;
			BTMTK_DBG("u32rxdatacount %d, rx_length %d",
				u32rxdatacount, rx_length);
		}
	}

	btmtk_sdio_timestamp(BTMTK_SDIO_RX_CHECKPOINT_ENABLE_INTR);
	ret = btmtk_sdio_enable_interrupt(1);

	return ret;
}

static void btmtk_sdio_interrupt(struct sdio_func *func)
{
	struct btmtk_private *priv;
	struct btmtk_sdio_card *card;

	card = sdio_get_drvdata(func);

	if (!card)
		return;


	if (!card->priv)
		return;

	btmtk_sdio_timestamp(BTMTK_SDIO_RX_CHECKPOINT_INTR);
	priv = card->priv;
	btmtk_sdio_enable_interrupt(0);

	btmtk_interrupt(priv);
}

static int btmtk_sdio_register_dev(struct btmtk_sdio_card *card)
{
	struct sdio_func *func;
	u8	u8ReadCRValue = 0;
	u8 reg;
	int ret = 0;

	if (!card || !card->func) {
		BTMTK_ERR("Error: card or function is NULL!");
		ret = -EINVAL;
		goto failed;
	}

	func = card->func;

	sdio_claim_host(func);

	ret = sdio_enable_func(func);
	sdio_release_host(g_card->func);
	if (ret) {
		BTMTK_ERR("sdio_enable_func() failed: ret=%d", ret);
		ret = -EIO;
		goto failed;
	}

	btmtk_sdio_readb(SDIO_CCCR_IENx, &u8ReadCRValue);
	BTMTK_INFO("before claim irq read SDIO_CCCR_IENx %x, func num %d",
		u8ReadCRValue, func->num);

	sdio_claim_host(g_card->func);
	ret = sdio_claim_irq(func, btmtk_sdio_interrupt);
	sdio_release_host(g_card->func);
	if (ret) {
		BTMTK_ERR("sdio_claim_irq failed: ret=%d", ret);
		ret = -EIO;
		goto disable_func;
	}
	BTMTK_INFO("sdio_claim_irq success: ret=%d", ret);

	btmtk_sdio_readb(SDIO_CCCR_IENx, &u8ReadCRValue);
	BTMTK_INFO("after claim irq read SDIO_CCCR_IENx %x", u8ReadCRValue);

	sdio_claim_host(g_card->func);
	ret = sdio_set_block_size(card->func, SDIO_BLOCK_SIZE);
	sdio_release_host(g_card->func);
	if (ret) {
		BTMTK_ERR("cannot set SDIO block size");
		ret = -EIO;
		goto release_irq;
	}

	ret = btmtk_sdio_readb(card->reg->io_port_0, &reg);
	if (ret < 0) {
		ret = -EIO;
		goto release_irq;
	}
	card->ioport = reg;

	ret = btmtk_sdio_readb(card->reg->io_port_1, &reg);
	if (ret < 0) {
		ret = -EIO;
		goto release_irq;
	}
	card->ioport |= (reg << 8);

	ret = btmtk_sdio_readb(card->reg->io_port_2, &reg);
	if (ret < 0) {
		ret = -EIO;
		goto release_irq;
	}

	card->ioport |= (reg << 16);

	BTMTK_INFO("SDIO FUNC%d IO port: 0x%x", func->num, card->ioport);

	if (card->reg->int_read_to_clear) {
		ret = btmtk_sdio_readb(card->reg->host_int_rsr, &reg);
		if (ret < 0) {
			ret = -EIO;
			goto release_irq;
		}
		ret = btmtk_sdio_writeb(card->reg->host_int_rsr, reg | 0x3f);
		if (ret < 0) {
			ret = -EIO;
			goto release_irq;
		}

		ret = btmtk_sdio_readb(card->reg->card_misc_cfg, &reg);
		if (ret < 0) {
			ret = -EIO;
			goto release_irq;
		}
		ret = btmtk_sdio_writeb(card->reg->card_misc_cfg, reg | 0x10);
		if (ret < 0) {
			ret = -EIO;
			goto release_irq;
		}
	}

	sdio_set_drvdata(func, card);

	return 0;

release_irq:
	sdio_release_irq(func);

disable_func:
	sdio_disable_func(func);

failed:
	BTMTK_INFO("fail");
	return ret;
}

static int btmtk_sdio_unregister_dev(struct btmtk_sdio_card *card)
{
	if (card && card->func) {
		sdio_claim_host(card->func);
		sdio_release_irq(card->func);
		sdio_disable_func(card->func);
		sdio_release_host(card->func);
		sdio_set_drvdata(card->func, NULL);
	}

	return 0;
}

static int btmtk_sdio_enable_host_int(struct btmtk_sdio_card *card)
{
	int ret;
	u32 read_data = 0;

	if (!card || !card->func)
		return -EINVAL;

	ret = btmtk_sdio_enable_host_int_mask(card, HIM_ENABLE);

	btmtk_sdio_get_rx_unit(card);

	if (0) {
		typedef int (*fp_sdio_hook)(struct mmc_host *host,
						unsigned int width);
		fp_sdio_hook func_sdio_hook =
			(fp_sdio_hook)btmtk_kallsyms_lookup_name("mmc_set_bus_width");
		unsigned char data = 0;

		sdio_claim_host(g_card->func);
		data = sdio_f0_readb(card->func, SDIO_CCCR_IF, &ret);
		if (ret)
			BTMTK_INFO("sdio_f0_readb ret %d", ret);

		BTMTK_INFO("sdio_f0_readb data 0x%X!", data);

		data  &= ~SDIO_BUS_WIDTH_MASK;
		data  |= SDIO_BUS_ASYNC_INT;
		card->func->card->quirks |= MMC_QUIRK_LENIENT_FN0;

		sdio_f0_writeb(card->func, data, SDIO_CCCR_IF, &ret);
		if (ret)
			BTMTK_INFO("sdio_f0_writeb ret %d", ret);

		BTMTK_INFO("func_sdio_hook at 0x%p!", func_sdio_hook);
		if (func_sdio_hook)
			func_sdio_hook(card->func->card->host, MMC_BUS_WIDTH_1);

		data = sdio_f0_readb(card->func, SDIO_CCCR_IF, &ret);
		if (ret)
			BTMTK_INFO("sdio_f0_readb 2 ret %d", ret);
		sdio_release_host(g_card->func);

		BTMTK_INFO("sdio_f0_readb2 data 0x%X", data);
	}

/* workaround for some platform no host clock sometimes */

	btmtk_sdio_readl(CSDIOCSR, &read_data);
	BTMTK_INFO("read CSDIOCSR is 0x%X", read_data);
	read_data |= 0x4;
	btmtk_sdio_writel(CSDIOCSR, read_data);
	BTMTK_INFO("write CSDIOCSR is 0x%X", read_data);

	return ret;
}

static int btmtk_sdio_disable_host_int(struct btmtk_sdio_card *card)
{
	int ret;

	if (!card || !card->func)
		return -EINVAL;

	ret = btmtk_sdio_disable_host_int_mask(card, HIM_DISABLE);

	return ret;
}

static int btmtk_sdio_download_fw(struct btmtk_sdio_card *card)
{
	int ret = 0;

	BTMTK_INFO("begin");
	if (!card || !card->func) {
		BTMTK_ERR("card or function is NULL!");
		return -EINVAL;
	}

	sdio_claim_host(card->func);

	if (btmtk_sdio_download_rom_patch(card)) {
		BTMTK_ERR("Failed to download firmware!");
		ret = -EIO;
	}
	sdio_release_host(card->func);

	return ret;
}

static int btmtk_sdio_push_data_to_metabuffer(
						struct ring_buffer *metabuffer,
						char *data,
						int len,
						u8 type,
						bool use_type)
{
	int remainLen = 0;

	if (metabuffer->write_p >= metabuffer->read_p)
		remainLen = metabuffer->write_p - metabuffer->read_p;
	else
		remainLen = META_BUFFER_SIZE -
			(metabuffer->read_p - metabuffer->write_p);

	if ((remainLen + 1 + len) >= META_BUFFER_SIZE) {
		BTMTK_WARN("copy copyLen %d > META_BUFFER_SIZE(%d), push back to queue",
			(remainLen + 1 + len),
			META_BUFFER_SIZE);
		return -1;
	}

	if (use_type) {
		metabuffer->buffer[metabuffer->write_p] = type;
		metabuffer->write_p++;
	}
	if (metabuffer->write_p >= META_BUFFER_SIZE)
		metabuffer->write_p = 0;

	if (metabuffer->write_p + len <= META_BUFFER_SIZE)
		memcpy(&metabuffer->buffer[metabuffer->write_p],
			data,
			len);
	else {
		memcpy(&metabuffer->buffer[metabuffer->write_p],
			data,
			META_BUFFER_SIZE - metabuffer->write_p);
		memcpy(metabuffer->buffer,
			&data[META_BUFFER_SIZE - metabuffer->write_p],
			len - (META_BUFFER_SIZE - metabuffer->write_p));
	}

	metabuffer->write_p += len;
	if (metabuffer->write_p >= META_BUFFER_SIZE)
		metabuffer->write_p -= META_BUFFER_SIZE;

	remainLen += (1 + len);
	return 0;
}

static int btmtk_sdio_pull_data_from_metabuffer(
						struct ring_buffer *metabuffer,
						char __user *buf,
						size_t count)
{
	int copyLen = 0;
	unsigned long ret = 0;

	if (metabuffer->write_p >= metabuffer->read_p)
		copyLen = metabuffer->write_p - metabuffer->read_p;
	else
		copyLen = META_BUFFER_SIZE -
			(metabuffer->read_p - metabuffer->write_p);

	if (copyLen > count)
		copyLen = count;

	if (metabuffer->read_p + copyLen <= META_BUFFER_SIZE)
		ret = copy_to_user(buf,
				&metabuffer->buffer[metabuffer->read_p],
				copyLen);
	else {
		ret = copy_to_user(buf,
				&metabuffer->buffer[metabuffer->read_p],
				META_BUFFER_SIZE - metabuffer->read_p);
		if (!ret)
			ret = copy_to_user(
				&buf[META_BUFFER_SIZE - metabuffer->read_p],
				metabuffer->buffer,
				copyLen - (META_BUFFER_SIZE-metabuffer->read_p));
	}

	if (ret)
		BTMTK_WARN("copy to user fail, ret %d", (int)ret);

	metabuffer->read_p += (copyLen - ret);
	if (metabuffer->read_p >= META_BUFFER_SIZE)
		metabuffer->read_p -= META_BUFFER_SIZE;

	return (copyLen - ret);
}

static int btmtk_sdio_reset_dev(struct btmtk_sdio_card *card)
{
	struct sdio_func *func = NULL;
	u8 reg  = 0;
	int ret = 0;

	if (!card || !card->func) {
		BTMTK_ERR("Error: card or function is NULL!");
		return -1;
	}

	func = card->func;

	sdio_claim_host(func);

	ret = sdio_enable_func(func);
	if (ret) {
		BTMTK_ERR("sdio_enable_func() failed: ret=%d", ret);
		goto reset_dev_end;
	}

	reg = sdio_f0_readb(func, SDIO_CCCR_IENx, &ret);
	if (ret) {
		BTMTK_ERR("read SDIO_CCCR_IENx %x, func num %d, ret %d", reg, func->num, ret);
		goto reset_dev_end;
	}

	/*return negative value due to inturrept function is register before*/
	ret = sdio_claim_irq(func, btmtk_sdio_interrupt);
	BTMTK_INFO("sdio_claim_irq return %d", ret);

	reg |= 1 << func->num;
	reg |= 1;

	/* for bt driver can write SDIO_CCCR_IENx */
	func->card->quirks |= MMC_QUIRK_LENIENT_FN0;
	ret = 0;
	sdio_f0_writeb(func, reg, SDIO_CCCR_IENx, &ret);
	if (ret) {
		BTMTK_ERR("f0_writeb SDIO_CCCR_IENx %x, func num %d, ret %d error", reg, func->num, ret);
		goto reset_dev_end;
	}

	reg = sdio_f0_readb(func, SDIO_CCCR_IENx, &ret);
	if (ret) {
		BTMTK_ERR("f0_readb SDIO_CCCR_IENx %x, func num %d, ret %d error", reg, func->num, ret);
		goto reset_dev_end;
	}

	ret = sdio_set_block_size(card->func, SDIO_BLOCK_SIZE);
	if (ret) {
		BTMTK_ERR("cannot set SDIO block size");
		goto reset_dev_end;
	}

	reg = sdio_readb(func, card->reg->io_port_0, &ret);
	if (ret < 0) {
		BTMTK_ERR("read io port0 fail");
		goto reset_dev_end;
	}

	card->ioport = reg;

	reg = sdio_readb(func, card->reg->io_port_1, &ret);
	if (ret < 0) {
		BTMTK_ERR("read io port1 fail");
		goto reset_dev_end;
	}

	card->ioport |= (reg << 8);

	reg = sdio_readb(func, card->reg->io_port_2, &ret);
	if (ret < 0) {
		BTMTK_ERR("read io port2 fail");
		goto reset_dev_end;
	}

	card->ioport |= (reg << 16);

	BTMTK_INFO("SDIO FUNC%d IO port: 0x%x",
		func->num, card->ioport);

	if (card->reg->int_read_to_clear) {
		reg = sdio_readb(func, card->reg->host_int_rsr, &ret);
		if (ret < 0) {
			BTMTK_ERR("read init rsr fail");
			goto reset_dev_end;
		}
		sdio_writeb(func, reg | 0x3f, card->reg->host_int_rsr, &ret);
		if (ret < 0) {
			BTMTK_ERR("write init rsr fail");
			goto reset_dev_end;
		}

		reg = sdio_readb(func, card->reg->card_misc_cfg, &ret);
		if (ret < 0) {
			BTMTK_ERR("read misc cfg fail");
			goto reset_dev_end;
		}
		sdio_writeb(func, reg | 0x10, card->reg->card_misc_cfg, &ret);
		if (ret < 0)
			BTMTK_ERR("write misc cfg fail");
	}

	sdio_set_drvdata(func, card);
reset_dev_end:
	sdio_release_host(func);

	return ret;
}

static int btmtk_sdio_reset_fw(struct btmtk_sdio_card *card)
{
	int ret = 0;

	BTMTK_INFO("Mediatek Bluetooth driver Version=%s", VERSION);

	if (card->bt_cfg.support_woble_by_eint) {
		btmtk_sdio_RegisterBTIrq(card);
		btmtk_sdio_woble_input_init(card);
	}

	BTMTK_DBG("func device %X, call btmtk_sdio_register_dev", card->func->device);
	ret = btmtk_sdio_reset_dev(card);
	if (ret) {
		BTMTK_ERR("btmtk_sdio_reset_dev failed!");
		return ret;
	}

	BTMTK_DBG("btmtk_sdio_register_dev success");
	btmtk_sdio_enable_host_int(card);
	if (btmtk_sdio_download_fw(card)) {
		BTMTK_ERR("Downloading firmware failed!");
		ret = -ENODEV;
	}

	return ret;
}

static int btmtk_sdio_set_card_clkpd(int on)
{
	int ret = -1;
	/* call sdio_set_card_clkpd in sdio host driver */
	typedef void (*psdio_set_card_clkpd) (int on, struct sdio_func *func);
	char *sdio_set_card_clkpd_func_name = "sdio_set_card_clkpd";
	psdio_set_card_clkpd psdio_set_card_clkpd_func =
		(psdio_set_card_clkpd)btmtk_kallsyms_lookup_name
				(sdio_set_card_clkpd_func_name);

	if (psdio_set_card_clkpd_func) {
		BTMTK_INFO("get  %s",
				sdio_set_card_clkpd_func_name);
		psdio_set_card_clkpd_func(on, g_card->func);
		ret = 0;
	} else
		BTMTK_ERR("do not get %s",
			sdio_set_card_clkpd_func_name);
	return ret;
}

/*toggle PMU enable*/
static int btmtk_sdio_toggle_rst_pin(void)
{
	uint32_t pmu_en_delay = MT76x8_PMU_EN_DEFAULT_DELAY;
	int pmu_en;
	struct device *prDev;

	if (g_card == NULL) {
		BTMTK_ERR("g_card is NULL return");
		return -1;
	}
	sdio_claim_host(g_card->func);
	btmtk_sdio_set_card_clkpd(0);
	sdio_release_host(g_card->func);
	prDev = mmc_dev(g_card->func->card->host);
	if (!prDev) {
		BTMTK_ERR("unable to get struct dev for BT");
		return -1;
	}
	pmu_en = of_get_named_gpio(prDev->of_node, MT76x8_PMU_EN_PIN_NAME, 0);
	BTMTK_INFO("pmu_en %d", pmu_en);
	if (gpio_is_valid(pmu_en)) {
		gpio_direction_output(pmu_en, 0);
		mdelay(pmu_en_delay);
		gpio_direction_output(pmu_en, 1);
		BTMTK_INFO("%s pull low/high done",
				MT76x8_PMU_EN_PIN_NAME);
	} else {
		BTMTK_ERR("*** Invalid GPIO %s ***",
				MT76x8_PMU_EN_PIN_NAME);
		return -1;
	}
	return 0;
}

int btmtk_sdio_notify_wlan_remove_end(void)
{
	BTMTK_INFO("begin");
	wlan_remove_done = 1;
	btmtk_sdio_stop_wait_wlan_remove_tsk();

	BTMTK_INFO("done");
	return 0;
}
EXPORT_SYMBOL(btmtk_sdio_notify_wlan_remove_end);

int btmtk_sdio_bt_trigger_core_dump(int trigger_dump)
{
	struct sk_buff *skb = NULL;
	u8 coredump_cmd[] = {0x6F, 0xFC, 0x05,
			0x00, 0x01, 0x02, 0x01, 0x00, 0x08};

	if (g_priv == NULL) {
		BTMTK_ERR("g_priv is NULL return");
		return 0;
	}

	if (wait_dump_complete_tsk) {
		BTMTK_WARN("wait_dump_complete_tsk is working, return");
		return 0;
	}

	if (wait_wlan_remove_tsk) {
		BTMTK_WARN("wait_wlan_remove_tsk is working, return");
		return 0;
	}

	if (g_priv->btmtk_dev.reset_dongle) {
		BTMTK_WARN("reset_dongle is true, return");
		return 0;
	}

	if (!probe_ready) {
		BTMTK_INFO("probe_ready %d, return -1",
			probe_ready);
		return -1;/*BT driver is not ready, ask wifi do coredump*/
	}

	BTMTK_INFO("trigger_dump %d", trigger_dump);
	if (trigger_dump) {
		if (is_mt7663(g_card))
			wlan_status = WLAN_STATUS_CALL_REMOVE_START;
		skb = bt_skb_alloc(sizeof(coredump_cmd), GFP_ATOMIC);
		bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
		memcpy(&skb->data[0], &coredump_cmd[0], sizeof(coredump_cmd));
		skb->len = sizeof(coredump_cmd);
		skb_queue_tail(&g_card->tx_queue, skb);
		wake_up_interruptible(&g_priv->main_thread.wait_q);
	} else {
		if (g_card->bt_cfg.support_dongle_reset == 1) {
			/* makesure wait thread is stopped */
			btmtk_sdio_stop_wait_wlan_remove_tsk();
			wait_wlan_remove_tsk =
				kthread_run(btmtk_sdio_wait_wlan_remove_thread,
					NULL,
					"btmtk_sdio_wait_wlan_remove_thread");

			msleep(100);
			btmtk_sdio_notify_wlan_remove_start();
		} else {
			BTMTK_ERR("not support chip reset!");
		}
	}

	return 0;
}
EXPORT_SYMBOL(btmtk_sdio_bt_trigger_core_dump);

void btmtk_sdio_notify_wlan_toggle_rst_end(void)
{
	typedef void (*pnotify_wlan_toggle_rst_end) (int reserved);
	char *notify_wlan_toggle_rst_end_func_name =
			"notify_wlan_toggle_rst_end";
	/*void notify_wlan_toggle_rst_end(void)*/
	pnotify_wlan_toggle_rst_end pnotify_wlan_toggle_rst_end_func =
		(pnotify_wlan_toggle_rst_end) btmtk_kallsyms_lookup_name
				(notify_wlan_toggle_rst_end_func_name);

	BTMTK_INFO(L0_RESET_TAG);
	if (pnotify_wlan_toggle_rst_end_func) {
		BTMTK_INFO("do notify %s",
			notify_wlan_toggle_rst_end_func_name);
		pnotify_wlan_toggle_rst_end_func(1);
	} else
		BTMTK_ERR("do not get %s",
			notify_wlan_toggle_rst_end_func_name);
}

int btmtk_sdio_driver_reset_dongle(void)
{
	int ret = 0;
	int retry = 3;

	BTMTK_INFO("begin");
	if (g_priv == NULL) {
		BTMTK_INFO("g_priv = NULL, return");
		return -1;
	}

	need_reset_stack = 1;
	wlan_remove_done = 0;

retry_reset:
	retry--;
	if (retry < 0) {
		BTMTK_ERR("retry overtime fail");
		goto rst_dongle_err;
	}
	BTMTK_INFO("run %d", retry);
	ret = 0;
	if (btmtk_sdio_toggle_rst_pin()) {
		ret = -1;
		goto rst_dongle_err;
	}

	btmtk_sdio_set_no_fw_own(g_priv, FALSE);
	msleep(100);
	sdio_claim_host(g_card->func);
	if (pf_sdio_reset)
		ret = pf_sdio_reset(g_card->func->card);
	sdio_release_host(g_card->func);
	if (ret) {
		BTMTK_WARN("sdio_reset_comm error %d", ret);
		goto retry_reset;
	}
	BTMTK_WARN("sdio_reset_comm done");
	msleep(100);
	ret = btmtk_sdio_reset_fw(g_card);
	if (ret) {
		BTMTK_INFO("reset fw fail");
		goto retry_reset;
	} else
		BTMTK_INFO("reset fw done");

rst_dongle_err:
	btmtk_sdio_notify_wlan_toggle_rst_end();

	g_priv->btmtk_dev.tx_dnld_rdy = 1;
	g_priv->btmtk_dev.reset_dongle = 0;

	wlan_status = WLAN_STATUS_DEFAULT;
	btmtk_clean_queue();
	g_priv->btmtk_dev.reset_progress = 0;
	dump_data_counter = 0;
	BTMTK_INFO("return ret = %d", ret);
	return ret;
}

int WF_rst_L0_notify_BT_step2(void)
{
	int ret = -1;

	if (is_mt7663(g_card)) {
		BTMTK_INFO(L0_RESET_TAG "begin");
		btmtk_sdio_notify_wlan_remove_end();
		BTMTK_INFO(L0_RESET_TAG "done");
		ret = 0;
	} else {
		BTMTK_ERR(L0_RESET_TAG "is not MT7663");
	}
	return ret;
}
EXPORT_SYMBOL(WF_rst_L0_notify_BT_step2);

int WF_rst_L0_notify_BT_step1(int reserved)
{
	int ret = -1;

	if (is_mt7663(g_card)) {
		BTMTK_INFO(L0_RESET_TAG "begin");
		btmtk_sdio_bt_trigger_core_dump(true);
		BTMTK_INFO(L0_RESET_TAG "done");
		ret = 0;
	} else {
		BTMTK_ERR(L0_RESET_TAG "is not MT7663");
	}
	return ret;
}
EXPORT_SYMBOL(WF_rst_L0_notify_BT_step1);

#ifdef MTK_KERNEL_DEBUG
static int btmtk_sdio_L0_hang_thread(void *data)
{
	do {
		BTMTK_INFO(L0_RESET_TAG "Whole Chip Reset was triggered");
		msleep(3000);
	} while (1);
	return 0;
}

static int btmtk_sdio_L0_debug_probe(struct sdio_func *func,
					const struct sdio_device_id *id)
{
	int ret = 0;
	struct task_struct *task = NULL;
	struct btmtk_sdio_device *data = (void *) id->driver_data;
	u32 u32ReadCRValue = 0;
	u8 fw_download_fail = 0;

	BTMTK_INFO(L0_RESET_TAG "flow end");
	probe_counter++;
	BTMTK_INFO(L0_RESET_TAG "Mediatek Bluetooth driver Version=%s", VERSION);
	BTMTK_INFO(L0_RESET_TAG "vendor=0x%x, device=0x%x, class=%d, fn=%d, support func_num %d",
			id->vendor, id->device, id->class,
			func->num, data->reg->func_num);

	if (func->num != data->reg->func_num) {
		BTMTK_ERR(L0_RESET_TAG "func num is not match");
		return -ENODEV;
		}

	g_card->func = func;

	if (id->driver_data) {
		g_card->helper = data->helper;
		g_card->reg = data->reg;
		g_card->sd_blksz_fw_dl = data->sd_blksz_fw_dl;
		g_card->support_pscan_win_report = data->support_pscan_win_report;
		g_card->supports_fw_dump = data->supports_fw_dump;
		g_card->chip_id = data->reg->chip_id;
		g_card->suspend_count = 0;
		BTMTK_INFO(L0_RESET_TAG "chip_id %x", data->reg->chip_id);
	}

	if (btmtk_sdio_register_dev(g_card) < 0) {
		BTMTK_ERR(L0_RESET_TAG "Failed to register BT device!");
		return -ENODEV;
	}
	BTMTK_INFO("btmtk_sdio_register_dev success");

	/* Disable the interrupts on the card */
	btmtk_sdio_enable_host_int(g_card);
	BTMTK_DBG(L0_RESET_TAG "call btmtk_sdio_enable_host_int done");
	if (btmtk_sdio_download_fw(g_card)) {
		BTMTK_ERR(L0_RESET_TAG "Downloading firmware failed!");
		fw_download_fail = 1;
	}

	task = kthread_run(btmtk_sdio_L0_hang_thread,
					NULL, "btmtk_sdio_L0_hang_thread");
	if (IS_ERR(task)) {
		BTMTK_INFO(L0_RESET_TAG "create thread fail");
		goto unreg_dev;
	}

	ret = btmtk_sdio_readl(CHLPCR, &u32ReadCRValue);
	BTMTK_DBG(L0_RESET_TAG "chipid (0x%X)", g_card->chip_id);

	BTMTK_INFO(L0_RESET_TAG "PASS!!");

	if (fw_download_fail)
		btmtk_sdio_start_reset_dongle_progress();

	return 0;

unreg_dev:
	btmtk_sdio_unregister_dev(g_card);

	BTMTK_ERR(L0_RESET_TAG "fail end");
	return ret;
}
#else
static int btmtk_sdio_L0_probe(struct sdio_func *func,
					const struct sdio_device_id *id)
{
	int ret = 0;
	/* Set flags/functions here to leave HW reset mark before probe. */

	/* Now, ready to branch onto true sdio card probe. */
	ret = btmtk_sdio_probe(func, id);

	need_reset_stack = 1;
	BTMTK_INFO("need_reset_stack %d probe_ret %d", need_reset_stack, ret);
	wake_up_interruptible(&inq);
	return ret;
}
#endif

static int btmtk_sdio_L0_reset_host_config(struct mmc_host *host)
{

	if (host == NULL) {
		BTMTK_ERR(L0_RESET_TAG "mmc host is NULL");
		return -1;
	}

	if (host->rescan_entered != 0) {
		host->rescan_entered = 0;
		BTMTK_INFO(L0_RESET_TAG "set mmc_host rescan to 0");
	}

	BTMTK_DBG(L0_RESET_TAG "done");
		return 0;
}

static int btmtk_sdio_L0_reset(struct mmc_card *card)
{
	int ret = -1;
	struct mmc_host *host = NULL;

	if ((card == NULL) || (card->host  == NULL)) {
		BTMTK_ERR(L0_RESET_TAG "mmc structs are NULL");
		return ret;
	}

	host = card->host;
	ret = btmtk_sdio_L0_reset_host_config(host);
	if (ret != 0) {
		BTMTK_ERR(L0_RESET_TAG "set SDIO host failed");
		return ret;
	}

	BTMTK_INFO(L0_RESET_TAG "mmc_remove_host");
	mmc_remove_host(host);

	/* Replace hooked SDIO driver probe to new API;
	 * 1. It will be new kthread(state) after mmc_add_host;
	 * 2. Extend flexibility to notify us that HW reset was triggered,
	 * more flexiable on reviving in exchanging old/new kthread(state).
	 */
#ifdef MTK_KERNEL_DEBUG
	/* For DBG purpose only, replace to customized probe.
	 * Will only re-probe SDIO card function then hang for warning.
	 */
	btmtk_sdio_L0_hook_new_probe(btmtk_sdio_L0_debug_probe);
#else
	btmtk_sdio_L0_hook_new_probe(btmtk_sdio_L0_probe);
#endif

	BTMTK_INFO(L0_RESET_TAG "mmc_add_host");
	ret = mmc_add_host(host);

	BTMTK_INFO(L0_RESET_TAG "mmc_add_host return %d", ret);
	return ret;
}

int btmtk_sdio_host_reset_dongle(void)
{
	int ret = -1;

	BTMTK_INFO("begin");
	if (g_priv == NULL) {
		BTMTK_ERR("g_priv = NULL, return");
		return ret;
	}
	if ((!g_card) || (!g_card->func) || (!g_card->func->card)) {
		BTMTK_ERR(L0_RESET_TAG "data corrupted");
		goto rst_dongle_done;
	}

	wlan_remove_done = 0;

	ret = btmtk_sdio_L0_reset(g_card->func->card);
	BTMTK_INFO(L0_RESET_TAG "HW Reset status <%d>.", ret);

rst_dongle_done:
	btmtk_sdio_notify_wlan_toggle_rst_end();

	g_priv->btmtk_dev.tx_dnld_rdy = 1;
	g_priv->btmtk_dev.reset_dongle = 0;

	wlan_status = WLAN_STATUS_DEFAULT;
	btmtk_clean_queue();
	g_priv->btmtk_dev.reset_progress = 0;
	dump_data_counter = 0;
	dump_data_length = 0;
	return ret;
}

int btmtk_sdio_reset_dongle(void)
{
	if (is_mt7663(g_card))
		return btmtk_sdio_host_reset_dongle();
	else
		return btmtk_sdio_driver_reset_dongle();
}

static irqreturn_t btmtk_sdio_woble_isr(int irq, void *dev)
{
	struct btmtk_sdio_card *data = (struct btmtk_sdio_card *)dev;

	BTMTK_INFO("begin");
	disable_irq_nosync(data->wobt_irq);
	atomic_dec(&(data->irq_enable_count));
	BTMTK_INFO("disable BT IRQ, call wake lock");
	__pm_wakeup_event(data->eint_ws, WAIT_POWERKEY_TIMEOUT);

	input_report_key(data->WoBLEInputDev, KEY_WAKEUP, 1);
	input_sync(data->WoBLEInputDev);
	input_report_key(data->WoBLEInputDev, KEY_WAKEUP, 0);
	input_sync(data->WoBLEInputDev);
	BTMTK_INFO("end");
	return IRQ_HANDLED;
}

static int btmtk_sdio_RegisterBTIrq(struct btmtk_sdio_card *data)
{
	struct device_node *eint_node = NULL;
	int interrupts[2];

	eint_node = of_find_compatible_node(NULL, NULL, "mediatek,mt7668_bt_ctrl");
	BTMTK_INFO("begin");
	if (eint_node) {
		BTMTK_INFO("Get mt76xx_bt_ctrl compatible node");
		data->wobt_irq = irq_of_parse_and_map(eint_node, 0);
		BTMTK_INFO("wobt_irq number:%d", data->wobt_irq);
		if (data->wobt_irq) {
			of_property_read_u32_array(eint_node, "interrupts",
						   interrupts, ARRAY_SIZE(interrupts));
			data->wobt_irqlevel = interrupts[1];
			if (request_irq(data->wobt_irq, btmtk_sdio_woble_isr,
					data->wobt_irqlevel, "mt7668_bt_ctrl-eint", data))
				BTMTK_INFO("WOBTIRQ LINE NOT AVAILABLE!!");
			else {
				BTMTK_INFO("disable BT IRQ");
				disable_irq_nosync(data->wobt_irq);
			}

		} else
			BTMTK_INFO("can't find mt76xx_bt_ctrl irq");

	} else {
		data->wobt_irq = 0;
		BTMTK_INFO("can't find mt76xx_bt_ctrl compatible node");
	}


	BTMTK_INFO("end");
	return 0;
}

static int btmtk_sdio_woble_input_init(struct btmtk_sdio_card *data)
{
	int ret = 0;

	data->WoBLEInputDev = input_allocate_device();
	if (IS_ERR(data->WoBLEInputDev)) {
		BTMTK_ERR("input_allocate_device error");
		return -ENOMEM;
	}

	data->WoBLEInputDev->name = "WOBLE_INPUT_DEVICE";
	data->WoBLEInputDev->id.bustype = BUS_HOST;
	data->WoBLEInputDev->id.vendor = 0x0002;
	data->WoBLEInputDev->id.product = 0x0002;
	data->WoBLEInputDev->id.version = 0x0002;

	__set_bit(EV_KEY, data->WoBLEInputDev->evbit);
	__set_bit(KEY_WAKEUP, data->WoBLEInputDev->keybit);

	ret = input_register_device(data->WoBLEInputDev);
	if (ret < 0) {
		input_free_device(data->WoBLEInputDev);
		data->WoBLEInputDev = NULL;
		BTMTK_ERR("input_register_device %d", ret);
		return ret;
	}

	return ret;
}

static void btmtk_sdio_woble_input_deinit(struct btmtk_sdio_card *data)
{
	if (data->WoBLEInputDev) {
		input_unregister_device(data->WoBLEInputDev);
		input_free_device(data->WoBLEInputDev);
		data->WoBLEInputDev = NULL;
	}
}

static int btmtk_stereo_irq_handler(int irq, void *dev)
{
	/* Get sys clk */
	struct timeval tv;

	do_gettimeofday(&tv);
	sys_clk_tmp = (u64)tv.tv_sec * 1000000L + tv.tv_usec;
	BTMTK_DBG("tv_sec %d, tv_usec %d, sys_clk %llu",
		(int)tv.tv_sec, (int)tv.tv_usec, sys_clk_tmp);
	return 0;
}

static int btmtk_stereo_reg_irq(void)
{
	int ret = 0;
	struct device_node *node;
	int stereo_gpio;

	BTMTK_INFO("start");
	node = of_find_compatible_node(NULL, NULL, "mediatek,connectivity-combo");
	if (node) {
		stereo_gpio = of_get_named_gpio(node, "gpio_bt_stereo_pin", 0);
		BTMTK_INFO("pmu_en %d", stereo_gpio);
		if (gpio_is_valid(stereo_gpio))
			gpio_direction_input(stereo_gpio);
		else
			BTMTK_ERR("invalid stereo gpio");

		stereo_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(stereo_irq, (irq_handler_t)btmtk_stereo_irq_handler,
						IRQF_TRIGGER_RISING, "BTSTEREO_ISR_Handler", NULL);
		if (ret) {
			BTMTK_ERR("fail(%d)!!! irq_number=%d", ret, stereo_irq);
			stereo_irq = -1;
		}
	} else {
		BTMTK_INFO("of_find_compatible_node fail!!!");
		ret = -1;
		stereo_irq = -1;
	}
	return ret;
}

static void btmtk_stereo_unreg_irq(void)
{
	BTMTK_INFO("enter");
	if (stereo_irq != -1)
		free_irq(stereo_irq, NULL);
	stereo_irq = -1;
	BTMTK_INFO("exit");
}

static int btmtk_sdio_probe(struct sdio_func *func,
					const struct sdio_device_id *id)
{
	int ret = 0;
	struct btmtk_private *priv = NULL;
	struct btmtk_sdio_device *data = (void *) id->driver_data;
	u32 u32ReadCRValue = 0;
	u8 fw_download_fail = 0;

	probe_counter++;
	BTMTK_INFO("Mediatek Bluetooth driver Version=%s", VERSION);
	BTMTK_INFO("vendor=0x%x, device=0x%x, class=%d, fn=%d, support func_num %d",
			id->vendor, id->device, id->class,
			func->num, data->reg->func_num);

	if (func->num != data->reg->func_num) {
		BTMTK_INFO("func num is not match");
		return -ENODEV;
	}

	g_card->func = func;
	g_card->bin_file_buffer = NULL;

	if (id->driver_data) {
		g_card->helper = data->helper;
		g_card->reg = data->reg;
		g_card->sd_blksz_fw_dl = data->sd_blksz_fw_dl;
		g_card->support_pscan_win_report = data->support_pscan_win_report;
		g_card->supports_fw_dump = data->supports_fw_dump;
		g_card->chip_id = data->reg->chip_id;
		g_card->suspend_count = 0;
		BTMTK_INFO("chip_id is %x", data->reg->chip_id);
		/*allocate memory for woble_setting_file*/
		g_card->woble_setting_file_name = kzalloc(MAX_BIN_FILE_NAME_LEN, GFP_KERNEL);
		if (!g_card->woble_setting_file_name)
			return -1;
		need_retry_load_woble = 0;
#if SUPPORT_MT7663
		if (is_mt7668(g_card)) {
			memcpy(g_card->woble_setting_file_name,
					WOBLE_SETTING_FILE_NAME_7668,
					sizeof(WOBLE_SETTING_FILE_NAME_7668));
		}
#endif

#if SUPPORT_MT7668
		if (is_mt7663(g_card)) {
			memcpy(g_card->woble_setting_file_name,
					WOBLE_SETTING_FILE_NAME_7663,
					sizeof(WOBLE_SETTING_FILE_NAME_7663));
		}
#endif

		/*allocate memory for bt_cfg_file_name*/
		g_card->bt_cfg_file_name = kzalloc(MAX_BIN_FILE_NAME_LEN, GFP_KERNEL);
		if (!g_card->bt_cfg_file_name)
			return -1;

		memcpy(g_card->bt_cfg_file_name, BT_CFG_NAME, sizeof(BT_CFG_NAME));
	}

	btmtk_sdio_initialize_cfg_items();
	btmtk_sdio_load_setting_files(g_card->bt_cfg_file_name, &g_card->func->dev, g_card);

	BTMTK_DBG("func device %X, call btmtk_sdio_register_dev", g_card->func->device);
	if (btmtk_sdio_register_dev(g_card) < 0) {
		BTMTK_ERR("Failed to register BT device!");
		return -ENODEV;
	}

	BTMTK_DBG("btmtk_sdio_register_dev success");

	/* Disable the interrupts on the card */
	btmtk_sdio_enable_host_int(g_card);
	BTMTK_DBG("call btmtk_sdio_enable_host_int done");

	if (btmtk_sdio_download_fw(g_card)) {
		BTMTK_ERR("Downloading firmware failed!");
		fw_download_fail = 1;
	}

	/* check buffer mode */
	btmtk_eeprom_bin_file(g_card);

	/* Move from btmtk_fops_open() */
	spin_lock_init(&(metabuffer.spin_lock.lock));
	spin_lock_init(&(fwlog_metabuffer.spin_lock.lock));

	spin_lock_init(&(stereo_spin_lock.lock));

	BTMTK_DBG("spin_lock_init end");

	priv = btmtk_add_card(g_card);
	if (!priv) {
		BTMTK_ERR("Initializing card failed!");
		ret = -ENODEV;
		goto unreg_dev;
	}
	BTMTK_DBG("btmtk_add_card success");
	BTMTK_DBG("assign priv done");
	/* Initialize the interface specific function pointers */
	pf_sdio_reset = (sdio_reset_func) btmtk_kallsyms_lookup_name("sdio_reset_comm");
	if (!pf_sdio_reset && is_mt7668(g_card)) {
		BTMTK_WARN("no sdio_reset_comm() api, can't support chip reset!");
		g_card->bt_cfg.support_dongle_reset = 0;
	}
	g_priv->hw_host_to_card = btmtk_sdio_host_to_card;
	g_priv->hw_process_int_status = btmtk_sdio_process_int_status;
	g_priv->hw_set_own_back =  btmtk_sdio_set_own_back;
	g_priv->hw_sdio_reset_dongle = btmtk_sdio_reset_dongle;
	g_priv->start_reset_dongle_progress = btmtk_sdio_start_reset_dongle_progress;
	g_priv->hci_snoop_save = btmtk_sdio_hci_snoop_save;
	btmtk_sdio_set_no_fw_own(g_priv, g_card->is_KeepFullPwr);

	memset(&metabuffer.buffer, 0, META_BUFFER_SIZE);

	fw_dump_file = NULL;

	ret = btmtk_sdio_readl(CHLPCR, &u32ReadCRValue);
	BTMTK_DBG("read CHLPCR (0x%08X), chipid is  (0x%X)",
		u32ReadCRValue, g_card->chip_id);
	if (is_support_unify_woble(g_card)) {
		memset(g_card->bdaddr, 0, BD_ADDRESS_SIZE);
		btmtk_sdio_load_setting_files(g_card->woble_setting_file_name,
			&g_card->func->dev,
			g_card);
	}

	if (g_card->bt_cfg.support_unify_woble && g_card->bt_cfg.support_woble_wakelock) {
#ifdef CONFIG_MP_WAKEUP_SOURCE_SYSFS_STAT
		g_card->woble_ws = wakeup_source_register(NULL, "btevent_woble");
#else
		g_card->woble_ws = wakeup_source_register("btevent_woble");
#endif
		if (!g_card->woble_ws) {
			BTMTK_WARN("woble_ws register fail!");
			goto unreg_dev;
		}
	}

	if (g_card->bt_cfg.support_woble_by_eint) {
#ifdef CONFIG_MP_WAKEUP_SOURCE_SYSFS_STAT
		g_card->eint_ws = wakeup_source_register(NULL, "btevent_eint");
#else
		g_card->eint_ws = wakeup_source_register("btevent_eint");
#endif
		if (!g_card->eint_ws) {
			wakeup_source_unregister(g_card->woble_ws);
			BTMTK_WARN("eint_ws register fail!");
			goto unreg_dev;
		}

		btmtk_sdio_RegisterBTIrq(g_card);
		btmtk_sdio_woble_input_init(g_card);
	}

	sema_init(&g_priv->wr_mtx, 1);
	sema_init(&g_priv->rd_mtx, 1);

	BTMTK_INFO("normal end");
	probe_ready = true;
	if (fw_download_fail)
		btmtk_sdio_start_reset_dongle_progress();

	return 0;

unreg_dev:
	btmtk_sdio_unregister_dev(g_card);

	BTMTK_ERR("fail end");
	return ret;
}

static void btmtk_sdio_remove(struct sdio_func *func)
{
	struct btmtk_sdio_card *card;

	BTMTK_INFO("begin user_rmmod %d", user_rmmod);
	probe_ready = false;

	btmtk_sdio_set_no_fw_own(g_priv, FALSE);
	if (func) {
		card = sdio_get_drvdata(func);
		if (card) {
			BTMTK_INFO(L0_RESET_TAG "begin reset_dongle <%d>",
				card->priv->btmtk_dev.reset_dongle);
			/* Send SHUTDOWN command & disable interrupt
			 * if user removes the module.
			 */
			if (user_rmmod) {
				BTMTK_INFO("begin user_rmmod %d in user mode", user_rmmod);
				btmtk_sdio_enable_interrupt(0);
				btmtk_sdio_disable_host_int(card);
			}

			if (card->bt_cfg.support_unify_woble && card->bt_cfg.support_woble_wakelock)
				wakeup_source_unregister(card->woble_ws);

			if (card->bt_cfg.support_woble_by_eint) {
				wakeup_source_unregister(card->eint_ws);
				btmtk_sdio_woble_input_deinit(g_card);
			}

			btmtk_sdio_woble_free_setting();
			btmtk_sdio_free_bt_cfg();
			BTMTK_DBG("unregister dev");
			card->priv->surprise_removed = true;
			if (!card->priv->btmtk_dev.reset_dongle)
				btmtk_remove_card(card->priv);
			btmtk_sdio_unregister_dev(card);
			if (card->bin_file_buffer != NULL) {
				kfree(card->bin_file_buffer);
				card->bin_file_buffer = NULL;
			}
			need_reset_stack = 1;
		}
	}
	BTMTK_INFO("end");
}

/*
 * cmd_type:
 * #define HCI_COMMAND_PKT 0x01
 * #define HCI_ACLDATA_PKT 0x02
 * #define HCI_SCODATA_PKT 0x03
 * #define HCI_EVENT_PKT 0x04
 * #define HCI_VENDOR_PKT 0xff
 */
static int btmtk_sdio_send_hci_cmd(u8 cmd_type,
				u8 *cmd, int cmd_len,
				const u8 *event, const int event_len,
				int total_timeout)
		/* cmd: if cmd is null, don't compare event, just return 0 if send cmd success */
		/* total_timeout: -1 */
		/* add_spec_header:0 hci event, 1 use vend specic event header*/
		/* return 0 if compare successfully and no need to compare */
		/* return < 0 if error*/
		/* return value: 0 or positive success, -x fail */
{
	int ret = -1;
	unsigned long comp_event_timo = 0, start_time = 0;
	struct sk_buff *skb = NULL;

	if (cmd_len == 0) {
		BTMTK_ERR("cmd_len (%d) error return", cmd_len);
		return -EINVAL;
	}


	skb = bt_skb_alloc(cmd_len, GFP_ATOMIC);
	if (skb == NULL) {
		BTMTK_WARN("skb is null");
		return -ENOMEM;
	}
	bt_cb(skb)->pkt_type = cmd_type;
	memcpy(&skb->data[0], cmd, cmd_len);
	skb->len = cmd_len;
	if (event) {
		event_compare_status = BTMTK_SDIO_EVENT_COMPARE_STATE_NEED_COMPARE;
		memcpy(event_need_compare, event, event_len);
		event_need_compare_len = event_len;
	}
	skb_queue_tail(&g_card->tx_queue, skb);
	wake_up_interruptible(&g_priv->main_thread.wait_q);


	if (event == NULL)
		return 0;

	if (event_len > EVENT_COMPARE_SIZE) {
		BTMTK_ERR("event_len (%d) > EVENT_COMPARE_SIZE(%d), error", event_len, EVENT_COMPARE_SIZE);
		return -1;
	}

	start_time = jiffies;
	/* check HCI event */
	comp_event_timo = jiffies + msecs_to_jiffies(total_timeout);
	ret = -1;
	BTMTK_DBG("event_need_compare_len %d, event_compare_status %d",
		event_need_compare_len, event_compare_status);
	do {
		/* check if event_compare_success */
		if (event_compare_status == BTMTK_SDIO_EVENT_COMPARE_STATE_COMPARE_SUCCESS) {
			BTMTK_DBG("compare success");
			ret = 0;
			break;
		}

		msleep(100);
	} while (time_before(jiffies, comp_event_timo));
	event_compare_status = BTMTK_SDIO_EVENT_COMPARE_STATE_NOTHING_NEED_COMPARE;
	BTMTK_DBG("ret %d", ret);
	return ret;
}

static int btmtk_sdio_trigger_fw_assert(void)
{
	int ret = 0;
	/*
	 * fw dump has 2 types
	 * 1. Assert: trigger by hci cmd "5b fd 00" defined by bluedroid,
	 * 2. Exception: trigger by wmt cmd "6F FC 05 01 02 01 00 08"
	 */
	u8 cmd[] = { 0x5b, 0xfd, 0x00 };

	BTMTK_INFO("begin");
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd,
		sizeof(cmd),
		NULL, 0, WOBLE_COMP_EVENT_TIMO);
	if (ret != 0)
		BTMTK_INFO("ret = %d", ret);
	return ret;
}

static int btmtk_sdio_send_get_vendor_cap(void)
{
	int ret = -1;
	u8 get_vendor_cap_cmd[] = { 0x53, 0xFD, 0x00 };
	u8 get_vendor_cap_event[] = { 0x0e, 0x12, 0x01, 0x53, 0xFD, 0x00};

	BTMTK_DBG("begin");
	BTSDIO_DEBUG_RAW(get_vendor_cap_cmd, (unsigned int)sizeof(get_vendor_cap_cmd),
		"%s: send vendor_cap_cmd is:", __func__);
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
			get_vendor_cap_cmd, sizeof(get_vendor_cap_cmd),
			get_vendor_cap_event, sizeof(get_vendor_cap_event),
			WOBLE_COMP_EVENT_TIMO);

	BTMTK_DBG("ret %d", ret);
	return ret;
}

static int btmtk_sdio_send_read_BDADDR_cmd(void)
{
	u8 cmd[] = { 0x09, 0x10, 0x00 };
	int ret = -1;
	unsigned char zero[BD_ADDRESS_SIZE];

	BTMTK_DBG("begin");
	if (g_card == NULL) {
		BTMTK_ERR("g_card == NULL!");
		return -1;
	}

	memset(zero, 0, sizeof(zero));
	if (memcmp(g_card->bdaddr, zero, BD_ADDRESS_SIZE) != 0) {
		BTMTK_DBG("already got bdaddr %02x%02x%02x%02x%02x%02x, return 0",
		g_card->bdaddr[0], g_card->bdaddr[1], g_card->bdaddr[2],
		g_card->bdaddr[3], g_card->bdaddr[4], g_card->bdaddr[5]);
		return 0;
	}
	BTSDIO_DEBUG_RAW(cmd, (unsigned int)sizeof(cmd), "%s: send read bd address cmd is:", __func__);
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd),
		READ_ADDRESS_EVENT, sizeof(READ_ADDRESS_EVENT), WOBLE_COMP_EVENT_TIMO);
	/*BD address will get in btmtk_sdio_host_to_card*/
	BTMTK_DBG("ret = %d", ret);

	return ret;
}

static int btmtk_sdio_set_Woble_APCF_filter_parameter(void)
{
	int ret = -1;
	u8 cmd[] = { 0x57, 0xfd, 0x0a, 0x01, 0x00, 0x5a, 0x20, 0x00, 0x20, 0x00, 0x01, 0x80, 0x00 };
	u8 event[] = { 0x0e, 0x07, 0x01, 0x57, 0xfd, 0x00, 0x01/*, 00, 63*/ };

	BTMTK_DBG("begin");
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd),
		event, sizeof(event),
		WOBLE_COMP_EVENT_TIMO);
	if (ret < 0)
		BTMTK_ERR("end ret %d", ret);
	else
		ret = 0;

	BTMTK_INFO("end ret=%d", ret);
	return ret;
}


/**
 * Set APCF manufacturer data and filter parameter
 */
static int btmtk_sdio_set_Woble_APCF(void)
{
	int ret = -1;
	int i = 0;
	u8 manufactur_data[] = { 0x57, 0xfd, 0x27, 0x06, 0x00, 0x5a,
		0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x43, 0x52, 0x4B, 0x54, 0x4D,
		0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 event_complete[] = { 0x0e, 0x07, 0x01, 0x57, 0xfd};

	BTMTK_DBG("begin");
	if (!g_card) {
		BTMTK_INFO("g_card is NULL, return -1");
		return -1;
	}

	BTMTK_DBG("g_card->woble_setting_apcf[0].length %d",
		g_card->woble_setting_apcf[0].length);

	/* start to send apcf cmd from woble setting  file */
	if (g_card->woble_setting_apcf[0].length) {
		for (i = 0; i < WOBLE_SETTING_COUNT; i++) {
			if (!g_card->woble_setting_apcf[i].length)
				continue;

			BTMTK_INFO("g_card->woble_setting_apcf_fill_mac[%d].content[0] = 0x%02x",
				i, g_card->woble_setting_apcf_fill_mac[i].content[0]);
			BTMTK_INFO("g_card->woble_setting_apcf_fill_mac_location[%d].length = %d",
				i, g_card->woble_setting_apcf_fill_mac_location[i].length);

			if ((g_card->woble_setting_apcf_fill_mac[i].content[0] == 1) &&
				g_card->woble_setting_apcf_fill_mac_location[i].length) {
				/* need add BD addr to apcf cmd */
				memcpy(g_card->woble_setting_apcf[i].content +
					(*g_card->woble_setting_apcf_fill_mac_location[i].content),
					g_card->bdaddr, BD_ADDRESS_SIZE);
				BTMTK_INFO("apcf %d ,add mac to location %d",
					i, (*g_card->woble_setting_apcf_fill_mac_location[i].content));
			}

			BTMTK_INFO("send APCF %d", i);
			BTSDIO_INFO_RAW(g_card->woble_setting_apcf[i].content, g_card->woble_setting_apcf[i].length,
				"woble_setting_apcf");

			ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, g_card->woble_setting_apcf[i].content,
				g_card->woble_setting_apcf[i].length,
				event_complete, sizeof(event_complete), WOBLE_COMP_EVENT_TIMO);

			if (ret < 0) {
				BTMTK_ERR("apcf %d error ret %d", i, ret);
				return ret;
			}

		}
	} else { /* use default */
		BTMTK_INFO("use default manufactur data");
		memcpy(manufactur_data + 9, g_card->bdaddr, BD_ADDRESS_SIZE);
		BTSDIO_DEBUG_RAW(manufactur_data, (unsigned int)sizeof(manufactur_data),
						"send manufactur_data ");

		ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, manufactur_data,
				sizeof(manufactur_data),
				event_complete, sizeof(event_complete), WOBLE_COMP_EVENT_TIMO);
		if (ret < 0) {
			BTMTK_ERR("manufactur_data error ret %d", ret);
			return ret;
		}

		ret = btmtk_sdio_set_Woble_APCF_filter_parameter();
	}

	BTMTK_INFO("end ret=%d", ret);
	return ret;
}



static int btmtk_sdio_send_woble_settings(struct fw_cfg_struct *settings_cmd,
	struct fw_cfg_struct *settings_event, char *message)
{
	int ret = -1;
	int i = 0;

	BTMTK_INFO("%s length %d", message, settings_cmd->length);
	if (g_card->woble_setting_radio_on[0].length) {
		for (i = 0; i < WOBLE_SETTING_COUNT; i++) {
			if (settings_cmd[i].length) {
				BTMTK_INFO("send %s %d", message, i);
				BTSDIO_INFO_RAW(settings_cmd[i].content,
					settings_cmd[i].length, "Raw");

				ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
						settings_cmd[i].content,
						settings_cmd[i].length,
						settings_event[i].content,
						settings_event[i].length,
						WOBLE_COMP_EVENT_TIMO);

				if (ret) {
					BTMTK_ERR("%s %d return error", message, i);
					return ret;
				}
			}
		}
	}
	return ret;
}

static int btmtk_sdio_send_unify_woble_suspend_default_cmd(void)
{
	int ret = 0;	/* if successful, 0 */
	/* Turn off WOBLE, FW go into low power mode only */
	u8 cmd[] = { 0xC9, 0xFC, 0x14, 0x01, 0x20, 0x02, 0x00, 0x00,
		0x02, 0x01, 0x00, 0x05, 0x10, 0x01, 0x00, 0x40, 0x06,
		0x02, 0x40, 0x5A, 0x02, 0x41, 0x0F };
	/*u8 status[] = { 0x0F, 0x04, 0x00, 0x01, 0xC9, 0xFC };*/
	u8 event[] = { 0xE6, 0x02, 0x08, 0x00 };

	BTMTK_DBG("begin");
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd),
				event, sizeof(event), WOBLE_COMP_EVENT_TIMO);
	if (ret)
		BTMTK_ERR("comp_event return error(%d)", ret);

	return ret;
}

/**
 * Set APCF manufacturer data and filter parameter
 *
 * WoBLE test command(TCI_TRIGGER_GPIO, 0xFD77) define:
 * b3 GPIO pin (1 or 9)
 * b4 active mode (0: low active, 1: high active)
 * b5 duration (slots)
 *
 */
static int btmtk_sdio_set_Woble_radio_off(u8 is_suspend)
{
	int ret = -1;
	u8 cmd[] = { 0x77, 0xFD, 0x03, 0x01, 0x00, 0xA0 };

	if (is_suspend) {
		BTMTK_DBG("g_card->woble_setting_radio_off[0].length %d",
				g_card->woble_setting_radio_off[0].length);
		BTMTK_DBG("g_card->woble_setting_radio_off_comp_event[0].length %d",
				g_card->woble_setting_radio_off_comp_event[0].length);

		if (g_card->woble_setting_radio_off[0].length) {
			if (g_card->woble_setting_radio_off_comp_event[0].length &&
					is_support_unify_woble(g_card)) {
				ret = btmtk_sdio_send_woble_settings(g_card->woble_setting_radio_off,
						g_card->woble_setting_radio_off_comp_event,
						"radio off");
				if (ret) {
					BTMTK_ERR("radio off error");
					return ret;
				}
			} else
				BTMTK_INFO("woble_setting_radio_off length is %d",
					g_card->woble_setting_radio_off[0].length);
		} else {/* use default */
			BTMTK_INFO("use default radio off cmd");
			ret = btmtk_sdio_send_unify_woble_suspend_default_cmd();
		}
	} else {
		BTMTK_INFO("begin");

		ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd),
					NULL, 0, WOBLE_COMP_EVENT_TIMO);
		if (ret)
			BTMTK_ERR("comp_event return error(%d)", ret);
	}
	BTMTK_INFO("end ret=%d", ret);
	return ret;
}

static int btmtk_sdio_handle_entering_WoBLE_state(u8 is_suspend)
{
	int ret = 0;
	u8 radio_off_cmd[] = { 0xC9, 0xFC, 0x05, 0x01, 0x20, 0x02, 0x00, 0x00 };
	u8 radio_off_evt[] = { 0xE6, 0x02, 0x08, 0x00 };
	int fops_state = 0;

	BTMTK_DBG("begin");

	FOPS_MUTEX_LOCK();
	fops_state = btmtk_fops_get_state();
	FOPS_MUTEX_UNLOCK();

	if (!is_support_unify_woble(g_card)) {
		if (fops_state == BTMTK_FOPS_STATE_OPENED) {
			BTMTK_ERR("not support, send radio off");

			BTSDIO_DEBUG_RAW(radio_off_cmd, (unsigned int)sizeof(radio_off_cmd),
				"%s: send radio_off_cmd is:", __func__);
			ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
					radio_off_cmd, sizeof(radio_off_cmd),
					radio_off_evt, sizeof(radio_off_evt),
					WOBLE_COMP_EVENT_TIMO);

			BTMTK_DBG("ret %d", ret);
		} else
			BTMTK_WARN("when not support woble, in bt off state, do nothing!");
	} else {
		if (g_card->dongle_state != BT_SDIO_DONGLE_STATE_POWER_ON) {
			if (!g_card->bt_cfg.support_woble_for_bt_disable) {
				BTMTK_INFO("BT is off, not support WoBLE");
				goto Finish;
			}

			if (btmtk_sdio_bt_set_power(1)) {
				BTMTK_ERR("power on failed");
				ret = -EIO;
				goto Finish;
			}
			g_card->dongle_state = BT_SDIO_DONGLE_STATE_POWER_ON_FOR_WOBLE;
		} else {
			g_card->dongle_state = BT_SDIO_DONGLE_STATE_WOBLE;
		}

		if (is_suspend) {
			ret = btmtk_sdio_send_get_vendor_cap();
			if (ret < 0) {
				BTMTK_ERR("btmtk_sdio_send_get_vendor_cap fail ret = %d", ret);
				goto Finish;
			}

			ret = btmtk_sdio_send_read_BDADDR_cmd();
			if (ret < 0) {
				BTMTK_ERR("btmtk_sdio_send_read_BDADDR_cmd fail ret = %d", ret);
				goto Finish;
			}

			ret = btmtk_sdio_set_Woble_APCF();
			if (ret < 0) {
				BTMTK_ERR("btmtk_sdio_set_Woble_APCF fail %d", ret);
				goto Finish;
			}
		}
		ret = btmtk_sdio_set_Woble_radio_off(is_suspend);
		if (ret < 0) {
			BTMTK_ERR("btmtk_sdio_set_Woble_radio_off return fail %d", ret);
			goto Finish;
		}
	}
Finish:
	if (ret)
		btmtk_sdio_woble_wake_lock(g_card);

	BTMTK_INFO("end");
	return ret;
}

static int btmtk_sdio_send_leave_woble_suspend_cmd(void)
{
	int ret = 0;	/* if successful, 0 */
	u8 cmd[] = { 0xC9, 0xFC, 0x05, 0x01, 0x21, 0x02, 0x00, 0x00 };
	u8 event[] = { 0xe6, 0x02, 0x08, 0x01 };

	BTSDIO_DEBUG_RAW(cmd, (unsigned int)sizeof(cmd), "cmd ");
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd),
				event, sizeof(event), WOBLE_COMP_EVENT_TIMO);

	if (ret < 0) {
		BTMTK_ERR("failed(%d)", ret);
	} else {
		BTMTK_INFO("OK");
		ret = 0;
	}
	return ret;
}

static int btmtk_sdio_del_Woble_APCF_inde(void)
{
	int ret = -1;
	u8 cmd[] = { 0x57, 0xfd, 0x03, 0x01, 0x01, 0x5a };
	u8 event[] = { 0x0e, 0x07, 0x01, 0x57, 0xfd, 0x00, 0x01, /* 00, 63 */ };

	BTMTK_DBG("begin");
	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd),
		event, sizeof(event), WOBLE_COMP_EVENT_TIMO);

	if (ret < 0)
		BTMTK_ERR("Got error %d", ret);

	BTMTK_INFO("end ret = %d", ret);
	return ret;
}

static void btmtk_sdio_check_wobx_debug_log(void)
{
	/* 0xFF, 0xFF, 0xFF, 0xFF is log level */
	u8 cmd[] = { 0xCE, 0xFC, 0x04, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 event[] = { 0xE8 };
	u8 *p = NULL, *pend = NULL;
	int ret = -1;
	u8 recv_len = 0;

	BTMTK_INFO("%s: begin", __func__);
	if (g_card == NULL) {
		BTMTK_ERR("%s: Incorrect g_card", __func__);
		return;
	}

	ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT, cmd, sizeof(cmd), event, sizeof(event), WOBLE_COMP_EVENT_TIMO);
	if (ret != 0) {
		BTMTK_ERR("%s: failed(%d)", __func__, ret);
		return;
	}

	recv_len = SDIO_HEADER_LEN + HCI_TYPE_LEN + HCI_EVENT_CODE_LEN;
	recv_len += rxbuf[recv_len];
	BTSDIO_INFO_RAW(rxbuf, recv_len, "%s: ", __func__);

	/* parse WoBX debug log */
	p = &rxbuf[SDIO_HEADER_LEN + HCI_TYPE_LEN + HCI_EVENT_CODE_LEN];
	pend = p + rxbuf[SDIO_HEADER_LEN + HCI_TYPE_LEN + HCI_EVENT_CODE_LEN];
	while (p < pend) {
		u8 attr_len = *(p + 1);
		u8 attr_type = *(p + 2);

		BTMTK_INFO("attr_len = 0x%x, attr_type = 0x%x", attr_len, attr_type);
		switch (attr_type) {
		case WOBX_TRIGGER_INFO_ADDR_TYPE:
			break;
		case WOBX_TRIGGER_INFO_ADV_DATA_TYPE:
			break;
		case WOBX_TRIGGER_INFO_TRACE_LOG_TYPE:
			break;
		case WOBX_TRIGGER_INFO_SCAN_LOG_TYPE:
			break;
		case WOBX_TRIGGER_INFO_TRIGGER_CNT_TYPE:
			BTMTK_INFO("wakeup times(via BT) = %02X%02X%02X%02X",
				*(p + 6), *(p + 5), *(p + 4), *(p + 3));
			break;
		default:
			BTMTK_ERR("%s: unexpected attribute type(0x%x)", __func__, attr_type);
			return;
		}
		p += 1 + attr_len;	// 1: len
	}
}

static int btmtk_sdio_handle_leaving_WoBLE_state(void)
{
	int ret = 0;
	u8 radio_on_cmd[] = { 0xC9, 0xFC, 0x05, 0x01, 0x21, 0x02, 0x00, 0x00 };
	u8 radio_on_evt[] = { 0xE6, 0x02, 0x08, 0x01 };
	int fops_state = 0;

	BTMTK_DBG("begin");

	if (g_card == NULL) {
		BTMTK_ERR("g_card is NULL return");
		goto exit;
	}

	FOPS_MUTEX_LOCK();
	fops_state = btmtk_fops_get_state();
	FOPS_MUTEX_UNLOCK();

	if (!is_support_unify_woble(g_card)) {
		if (fops_state == BTMTK_FOPS_STATE_OPENED) {
			BTMTK_ERR("not support, send radio on");
			BTSDIO_DEBUG_RAW(radio_on_cmd, (unsigned int)sizeof(radio_on_cmd),
				"%s: send radio_on_cmd is:", __func__);
			ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
					radio_on_cmd, sizeof(radio_on_cmd),
					radio_on_evt, sizeof(radio_on_evt),
					WOBLE_COMP_EVENT_TIMO);
			BTMTK_DBG("ret %d", ret);
			goto exit;
		} else {
			BTMTK_WARN("when not support woble, in bt off state, do nothing!");
			goto exit;
		}
	}

	if ((g_card->dongle_state != BT_SDIO_DONGLE_STATE_POWER_ON_FOR_WOBLE)
		&& (g_card->dongle_state != BT_SDIO_DONGLE_STATE_WOBLE)) {
		BTMTK_ERR("Not in woble mode");
		goto exit;
	}

	if (g_card->woble_setting_radio_on[0].length &&
		g_card->woble_setting_radio_on_comp_event[0].length &&
		g_card->woble_setting_apcf_resume[0].length) {
			/* start to send radio on cmd from woble setting file */
		ret = btmtk_sdio_send_woble_settings(g_card->woble_setting_radio_on,
			g_card->woble_setting_radio_on_comp_event, "radio on");
		if (ret) {
			BTMTK_ERR("woble radio on error");
			goto finish;
		}

		ret = btmtk_sdio_send_woble_settings(g_card->woble_setting_apcf_resume,
			g_card->woble_setting_apcf_resume_event, "apcf resume");
		if (ret) {
			BTMTK_ERR("apcf resume error");
			goto finish;
		}

	} else { /* use default */
		ret = btmtk_sdio_send_leave_woble_suspend_cmd();
		if (ret) {
			BTMTK_ERR("radio on error");
			goto finish;
		}

		ret = btmtk_sdio_del_Woble_APCF_inde();
		if (ret) {
			BTMTK_ERR("del apcf index error");
			goto finish;
		}
	}

finish:
	btmtk_sdio_check_wobx_debug_log();

	if (g_card->dongle_state == BT_SDIO_DONGLE_STATE_POWER_ON_FOR_WOBLE) {
		if (btmtk_sdio_bt_set_power(0)) {
			BTMTK_ERR("power off failed");
			return -EIO;
		}
	} else {
		g_card->dongle_state = BT_SDIO_DONGLE_STATE_POWER_ON;
	}

exit:
	BTMTK_INFO("end");
	return ret;
}

static int btmtk_sdio_send_apcf_reserved(void)
{
	int ret = -1;
	/* 76x8 APCF Cmd formate: [Header(0xFC5C), Len, Groups of APCF (Max. reserved 10)]
	 * 76x3 APCF Cmd Formate: [Header(0xFC85), Len, Groups of APCF (Max. reserved 2)]
	 */
	u8 reserve_apcf_cmd_7668[] = { 0x5C, 0xFC, 0x01, 0x0A };
	u8 reserve_apcf_event_7668[] = { 0x0e, 0x06, 0x01, 0x5C, 0xFC, 0x00 };

	/* change apcf cmd and event according to mt7663 fw requirement in mp1.4*/
	u8 reserve_apcf_cmd_7663[] = { 0x85, 0xFC, 0x01, 0x02 };
	u8 reserve_apcf_event_7663[] = { 0x0e, 0x06, 0x01, 0x85, 0xFC, 0x00 };

	if (g_card->func->device == 0x7668)
		ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
			reserve_apcf_cmd_7668, sizeof(reserve_apcf_cmd_7668),
			reserve_apcf_event_7668, sizeof(reserve_apcf_event_7668),
			WOBLE_COMP_EVENT_TIMO);
	else if (g_card->func->device == 0x7663)
		ret = btmtk_sdio_send_hci_cmd(HCI_COMMAND_PKT,
			reserve_apcf_cmd_7663, sizeof(reserve_apcf_cmd_7663),
			reserve_apcf_event_7663, sizeof(reserve_apcf_event_7663),
			WOBLE_COMP_EVENT_TIMO);
	else
		BTMTK_WARN("not support for 0x%x", g_card->func->device);

	BTMTK_INFO("ret %d", ret);
	return ret;
}

static int btmtk_sdio_suspend(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	int fops_state = 0;
	int ret = 0;
	mmc_pm_flag_t pm_flags;

	BTMTK_INFO("begin");

	if (g_card == NULL) {
		BTMTK_ERR("g_card is NULL return");
		return 0;
	}

	if (g_card->suspend_count++) {
		BTMTK_WARN("Has suspended. suspend_count: %d, end", g_card->suspend_count);
		return 0;
	}

	FOPS_MUTEX_LOCK();
	fops_state = btmtk_fops_get_state();
	FOPS_MUTEX_UNLOCK();
	if ((fops_state == BTMTK_FOPS_STATE_OPENING) || (fops_state == BTMTK_FOPS_STATE_CLOSING)) {
		BTMTK_WARN("fops state is %d, suspend abort", fops_state);
		return -ENOSYS;
	}

	btmtk_sdio_handle_entering_WoBLE_state(1);

	if (g_card->bt_cfg.support_unify_woble && g_card->bt_cfg.support_woble_by_eint) {
		if (g_card->wobt_irq != 0 && atomic_read(&(g_card->irq_enable_count)) == 0) {
			BTMTK_INFO("enable BT IRQ:%d", g_card->wobt_irq);
			irq_set_irq_wake(g_card->wobt_irq, 1);
			enable_irq(g_card->wobt_irq);
			atomic_inc(&(g_card->irq_enable_count));
		} else
			BTMTK_INFO("irq_enable count:%d", atomic_read(&(g_card->irq_enable_count)));
	}

	if (func) {
		pm_flags = sdio_get_host_pm_caps(func);
		if (!(pm_flags & MMC_PM_KEEP_POWER)) {
			BTMTK_ERR("%s cannot remain alive while suspended(0x%x)",
				sdio_func_id(func), pm_flags);
		}

		pm_flags = MMC_PM_KEEP_POWER;
		ret = sdio_set_host_pm_flags(func, pm_flags);
		if (ret) {
			BTMTK_ERR("set flag 0x%x err %d", pm_flags, (int)ret);
			return -ENOSYS;
		}
	} else {
		BTMTK_ERR("sdio_func is not specified");
		return 0;
	}

	return 0;
}

static int btmtk_sdio_resume(struct device *dev)
{
	u8 ret = 0;
	int fops_state = 0;

	if (g_card == NULL) {
		BTMTK_ERR("g_card is NULL return");
		return 0;
	}

	g_card->suspend_count--;
	if (g_card->suspend_count) {
		BTMTK_INFO("data->suspend_count %d, return 0", g_card->suspend_count);
		return 0;
	}

	if (g_card->bt_cfg.support_unify_woble && g_card->bt_cfg.support_woble_by_eint) {
		if (g_card->wobt_irq != 0 && atomic_read(&(g_card->irq_enable_count)) == 1) {
			BTMTK_INFO("disable BT IRQ:%d", g_card->wobt_irq);
			atomic_dec(&(g_card->irq_enable_count));
			disable_irq_nosync(g_card->wobt_irq);
		} else
			BTMTK_INFO("irq_enable count:%d", atomic_read(&(g_card->irq_enable_count)));
	}

	ret = btmtk_sdio_handle_leaving_WoBLE_state();
	if (ret)
		BTMTK_ERR("btmtk_sdio_handle_leaving_WoBLE_state return fail  %d", ret);

	FOPS_MUTEX_LOCK();
	fops_state = btmtk_fops_get_state();
	FOPS_MUTEX_UNLOCK();
	if (g_card->bt_cfg.reset_stack_after_woble
		&& need_reset_stack == 0
		&& fops_state == BTMTK_FOPS_STATE_OPENED)
		need_reset_stack = 1;

	BTMTK_INFO("end");
	return 0;
}

static const struct dev_pm_ops btmtk_sdio_pm_ops = {
	.suspend = btmtk_sdio_suspend,
	.resume = btmtk_sdio_resume,
};

static struct sdio_driver bt_mtk_sdio = {
	.name = "btmtk_sdio",
	.id_table = btmtk_sdio_ids,
	.probe = btmtk_sdio_probe,
	.remove = btmtk_sdio_remove,
	.drv = {
		.owner = THIS_MODULE,
		.pm = &btmtk_sdio_pm_ops,
	}
};

static void btmtk_sdio_L0_hook_new_probe(sdio_card_probe pFn_Probe)
{
	if (pFn_Probe == NULL) {
		BTMTK_ERR(L0_RESET_TAG "new probe is NULL");
		return;
	}
	bt_mtk_sdio.probe = pFn_Probe;
}

static int btmtk_clean_queue(void)
{
	BTMTK_INFO("enter");
	LOCK_UNSLEEPABLE_LOCK(&(metabuffer.spin_lock));
	skb_queue_purge(&g_card->tx_queue);
	skb_queue_purge(&g_card->fops_queue);
	UNLOCK_UNSLEEPABLE_LOCK(&(metabuffer.spin_lock));
	BTMTK_INFO("end");
	return 0;
}

static int btmtk_fops_open(struct inode *inode, struct file *file)
{
	int fops_state = 0;

	BTMTK_INFO("begin");

	BTMTK_INFO("Mediatek Bluetooth SDIO driver ver %s", VERSION);
	BTMTK_INFO("major %d minor %d (pid %d), probe counter: %d",
		imajor(inode), iminor(inode), current->pid, probe_counter);

	if (!probe_ready) {
		BTMTK_ERR("probe_ready is %d return", probe_ready);
		return -EFAULT;
	}

	if (g_priv == NULL) {
		BTMTK_ERR("g_priv is NULL");
		return -ENOENT;
	}

	if (g_priv->adapter == NULL) {
		BTMTK_ERR("g_priv->adapter is NULL");
		return -ENOENT;
	}

	FOPS_MUTEX_LOCK();
	fops_state = btmtk_fops_get_state();
	FOPS_MUTEX_UNLOCK();
	if (fops_state == BTMTK_FOPS_STATE_CLOSING) {
		BTMTK_ERR("mode is %d", fops_state);
		return -EAGAIN;
	}

	if (fops_state == BTMTK_FOPS_STATE_OPENING || fops_state == BTMTK_FOPS_STATE_OPENED) {
		BTMTK_ERR("mode is %d", fops_state);
		return -ENOENT;
	}

	FOPS_MUTEX_LOCK();
	btmtk_fops_set_state(BTMTK_FOPS_STATE_OPENING);
	FOPS_MUTEX_UNLOCK();

	metabuffer.read_p = 0;
	metabuffer.write_p = 0;

	btmtk_sdio_hci_snoop_init();

	if (btmtk_sdio_send_init_cmds(g_card)) {
		BTMTK_ERR("send init failed");
		return -EIO;
	}

	if (is_support_unify_woble(g_card)) {
		if (btmtk_sdio_send_apcf_reserved()) {
			BTMTK_ERR("send apcf failed");
			return -EIO;
		}
	}

	btmtk_clean_queue();

	FOPS_MUTEX_LOCK();
	btmtk_fops_set_state(BTMTK_FOPS_STATE_OPENED);
	FOPS_MUTEX_UNLOCK();

	need_reset_stack = 0;
	need_reopen = 0;
	stereo_irq = -1;
	BTMTK_INFO("end");
	return 0;
}

static int btmtk_fops_close(struct inode *inode, struct file *file)
{
	int fops_state = 0;

	BTMTK_INFO("begin");

	BTMTK_INFO("Mediatek Bluetooth SDIO driver ver %s", VERSION);
	BTMTK_INFO("major %d minor %d (pid %d), probe counter: %d",
		imajor(inode), iminor(inode), current->pid, probe_counter);

	if (!probe_ready) {
		BTMTK_ERR("probe_ready is %d return", probe_ready);
		return -EFAULT;
	}

	if (g_priv == NULL) {
		pr_info("%s g_priv is NULL\n", __func__);
		return -ENOENT;
	}

	if (g_priv->adapter == NULL) {
		pr_info("%s g_priv->adapter is NULL\n", __func__);
		return -ENOENT;
	}

	if (g_card->dongle_state != BT_SDIO_DONGLE_STATE_POWER_ON) {
		BTMTK_ERR("dongle_state is %d", g_card->dongle_state);
		goto exit;
	}

	FOPS_MUTEX_LOCK();
	fops_state = btmtk_fops_get_state();
	FOPS_MUTEX_UNLOCK();
	if (fops_state != BTMTK_FOPS_STATE_OPENED) {
		BTMTK_ERR("mode is %d", fops_state);
		goto exit;
	}

	FOPS_MUTEX_LOCK();
	btmtk_fops_set_state(BTMTK_FOPS_STATE_CLOSING);
	FOPS_MUTEX_UNLOCK();

	if (g_card->bt_cfg.support_auto_picus == true)
		btmtk_sdio_picus_operation(false);

	btmtk_sdio_send_hci_reset(true);
	btmtk_sdio_send_deinit_cmds();

exit:
	btmtk_stereo_unreg_irq();
	btmtk_clean_queue();
	need_reopen = 0;

	FOPS_MUTEX_LOCK();
	btmtk_fops_set_state(BTMTK_FOPS_STATE_CLOSED);
	FOPS_MUTEX_UNLOCK();

	if (g_priv && g_priv->adapter)
		BTMTK_INFO("end");
	else
		BTMTK_INFO("end g_priv or adapter is null");
	return 0;
}

ssize_t btmtk_fops_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *f_pos)
{
	int retval = 0;
	struct sk_buff *skb = NULL;
	static u8 waiting_for_hci_without_packet_type; /* INITIALISED_STATIC: do not initialise statics to 0 */
	static u8 hci_packet_type = 0xff;
	u32 copy_size = 0;
	unsigned long c_result = 0;
	u8 pkt_type = 0xff;
	u32 pkt_len = 0;
	unsigned char *pkt_data = NULL;
	int fops_state = 0;
#if SUPPORT_CR_WR
	u32 crAddr = 0, crValue = 0, crMask = 0;
#endif

	if (!probe_ready) {
		BTMTK_ERR("probe_ready is %d return", probe_ready);
		return -EFAULT;
	}

	if (g_priv == NULL) {
		BTMTK_ERR("g_priv is NULL");
		return -EFAULT;
	}

	FOPS_MUTEX_LOCK();
	fops_state = btmtk_fops_get_state();
	FOPS_MUTEX_UNLOCK();
	if (fops_state != BTMTK_FOPS_STATE_OPENED) {
		BTMTK_ERR("fops_state is %d", fops_state);
		return -EFAULT;
	}

	if (g_card->dongle_state != BT_SDIO_DONGLE_STATE_POWER_ON) {
		BTMTK_ERR("dongle_state is %d return", g_card->dongle_state);
		return 0;
	}

	if (need_reopen) {
		BTMTK_ERR("need_reopen (%d)!", need_reopen);
		return -EFAULT;
	}

	if (g_card->suspend_count) {
		BTMTK_ERR("suspend_count is %d", g_card->suspend_count);
		return -EAGAIN;
	}

	down(&g_priv->wr_mtx);

	if (count > 0 && count < MTK_TXDATA_SIZE) {
		memset(userbuf, 0, MTK_TXDATA_SIZE);
		c_result = copy_from_user(userbuf, buf, count);
	} else {
		BTMTK_ERR("target packet length:%zu is not allowed", count);
		retval = -EFAULT;
		goto OUT;
	}

	if (c_result != 0) {
		BTMTK_ERR("copy_from_user failed!");
		retval = -EFAULT;
		goto OUT;
	}

#if SUPPORT_CR_WR
	if (userbuf[0] == 0x7 && waiting_for_hci_without_packet_type == 0) {
		/* write CR */
		if (count < 15) {
			BTMTK_ERR("count=%zd less than 15, error", count);
			retval = -EFAULT;
			goto OUT;
		}

		crAddr = (userbuf[3] & 0xff) + ((userbuf[4] & 0xff) << 8)
			+ ((userbuf[5] & 0xff) << 16) + ((userbuf[6] & 0xff) << 24);
		crValue = (userbuf[7] & 0xff) + ((userbuf[8] & 0xff) << 8)
			+ ((userbuf[9] & 0xff) << 16) + ((userbuf[10] & 0xff) << 24);
		crMask = (userbuf[11] & 0xff) + ((userbuf[12] & 0xff) << 8)
			+ ((userbuf[13] & 0xff) << 16) + ((userbuf[14] & 0xff) << 24);

		BTMTK_INFO("crAddr=0x%08x crValue=0x%08x crMask=0x%08x", crAddr, crValue, crMask);
		crValue &= crMask;

		BTMTK_INFO("write crAddr=0x%08x crValue=0x%08x",
			crAddr, crValue);
		btmtk_sdio_writel(crAddr, crValue);
		retval = count;
		goto OUT;
	} else if (userbuf[0] == 0x8 && waiting_for_hci_without_packet_type == 0) {
		/* read CR */
		if (count < 16) {
			BTMTK_ERR("count=%zd less than 15, error", count);
			retval = -EFAULT;
			goto OUT;
		}

		crAddr = (userbuf[3] & 0xff) + ((userbuf[4] & 0xff) << 8) +
			((userbuf[5] & 0xff) << 16) + ((userbuf[6] & 0xff) << 24);
		crMask = (userbuf[11] & 0xff) + ((userbuf[12] & 0xff)<<8) +
			((userbuf[13] & 0xff) << 16) + ((userbuf[14] & 0xff) << 24);

		btmtk_sdio_readl(crAddr, &crValue);
		BTMTK_INFO("read crAddr=0x%08x crValue=0x%08x crMask=0x%08x",
			crAddr, crValue, crMask);
		retval = count;
		goto OUT;
	}
#endif
	if (count == 1) {
		if (waiting_for_hci_without_packet_type == 1) {
			BTMTK_WARN("Waiting for hci_without_packet_type, but receive data count is 1!");
			BTMTK_WARN("Treat this packet as packet_type(0x%02X)", userbuf[0]);
		}
		memcpy(&hci_packet_type, &userbuf[0], 1);
		waiting_for_hci_without_packet_type = 1;
		retval = 1;
		goto OUT;
	}

	if (waiting_for_hci_without_packet_type) {
		copy_size = count + 1;
		pkt_type = hci_packet_type;
		pkt_data = &userbuf[0];
	} else {
		copy_size = count;
		pkt_type = userbuf[0];
		pkt_data = &userbuf[1];
	}

	/* Check input length which must follow the BT spec.*/
	switch (pkt_type) {
	case HCI_COMMAND_PKT:
		/* HCI command : Type(8b) OpCode(16b) length(8b)
		 * Header length = 1 + 2 + 1
		 */
		pkt_len = pkt_data[2] + MTK_HCI_CMD_HEADER_LEN;
		break;
	case HCI_ACLDATA_PKT:
		/* ACL data : Type(8b) handle+flag(16b) length(16b)
		 * Header length = 1 + 2 + 2
		 */
		pkt_len = (pkt_data[2] | (pkt_data[3] << 8)) + MTK_HCI_ACL_HEADER_LEN;
		break;
	case HCI_SCODATA_PKT:
		/* SCO data : Type(8b) handle+flag(16b) length(8b)
		 * Header length = 1 + 2 + 1
		 */
		pkt_len = pkt_data[2] + MTK_HCI_SCO_HEADER_LEN;
		break;
	default:
		BTMTK_ERR("type is 0x%x", pkt_type);
		retval = -EFAULT;
		goto OUT;
	}

	/* check frame length is valid */
	if (pkt_len != copy_size) {
		BTMTK_ERR("input len(%d) error (expect %d)", copy_size, pkt_type);
		retval = -EFAULT;
		goto OUT;
	}


	skb = bt_skb_alloc(copy_size - 1, GFP_ATOMIC);
	if (skb == NULL) {
		BTMTK_ERR(" No meory for skb");
		retval = -EFAULT;
		goto OUT;
	}
	bt_cb(skb)->pkt_type = pkt_type;
	memcpy(&skb->data[0], pkt_data, copy_size - 1);

	skb->len = copy_size - 1;
	skb_queue_tail(&g_card->tx_queue, skb);

	if (bt_cb(skb)->pkt_type == HCI_COMMAND_PKT) {
		u8 fw_assert_cmd[] = { 0x6F, 0xFC, 0x05, 0x01, 0x02, 0x01, 0x00, 0x08 };
		u8 reset_cmd[] = { 0x03, 0x0C, 0x00 };
		u8 read_ver_cmd[] = { 0x01, 0x10, 0x00 };

		if (skb->len == sizeof(fw_assert_cmd) &&
			!memcmp(&skb->data[0], fw_assert_cmd, sizeof(fw_assert_cmd)))
			BTMTK_INFO("Donge FW Assert Triggered by upper layer");
		else if (skb->len == sizeof(reset_cmd) &&
			!memcmp(&skb->data[0], reset_cmd, sizeof(reset_cmd)))
			BTMTK_INFO("got command: 0x03 0C 00 (HCI_RESET)");
		else if (skb->len == sizeof(read_ver_cmd) &&
			!memcmp(&skb->data[0], read_ver_cmd, sizeof(read_ver_cmd)))
			BTMTK_INFO("got command: 0x01 10 00 (READ_LOCAL_VERSION)");
	}

	wake_up_interruptible(&g_priv->main_thread.wait_q);

	retval = copy_size;

	if (waiting_for_hci_without_packet_type) {
		hci_packet_type = 0xff;
		waiting_for_hci_without_packet_type = 0;
		if (retval > 0)
			retval -= 1;
	}

OUT:
	BTMTK_DBG("end");
	up(&g_priv->wr_mtx);
	return retval;
}

ssize_t btmtk_fops_read(struct file *filp, char __user *buf,
			size_t count, loff_t *f_pos)
{
	struct sk_buff *skb = NULL;
	int copyLen = 0;
	u8 hwerr_event[] = { 0x04, 0x10, 0x01, 0xff };
	static int send_hw_err_event_count;
	int fops_state = 0;

	if (!probe_ready) {
		BTMTK_ERR("probe_ready is %d return", probe_ready);
		return -EFAULT;
	}

	if (g_priv == NULL) {
		BTMTK_ERR("g_priv is NULL");
		return -EFAULT;
	}

	FOPS_MUTEX_LOCK();
	fops_state = btmtk_fops_get_state();
	FOPS_MUTEX_UNLOCK();
	if ((fops_state != BTMTK_FOPS_STATE_OPENED) && (need_reset_stack == 0)) {
		BTMTK_ERR("fops_state is %d", fops_state);
		return -EFAULT;
	}

	if (g_card->suspend_count) {
		BTMTK_ERR("suspend_count is %d", g_card->suspend_count);
		return -EAGAIN;
	}

	down(&g_priv->rd_mtx);

	if (need_reset_stack == 1) {
		BTMTK_WARN("Reset BT stack, go if send_hw_err_event_count %d", send_hw_err_event_count);
		if (send_hw_err_event_count < sizeof(hwerr_event)) {
			if (count < (sizeof(hwerr_event) - send_hw_err_event_count)) {
				copyLen = count;
				BTMTK_INFO("call wake_up_interruptible");
				wake_up_interruptible(&inq);
			} else
				copyLen = (sizeof(hwerr_event) - send_hw_err_event_count);
			BTMTK_WARN("in if copyLen = %d", copyLen);
			if (copy_to_user(buf, hwerr_event + send_hw_err_event_count, copyLen)) {
				BTMTK_ERR("send_hw_err_event_count %d copy to user fail, count = %d, go out",
					send_hw_err_event_count, copyLen);
				copyLen = -EFAULT;
				goto OUT;
			}
			send_hw_err_event_count += copyLen;
			BTMTK_WARN("in if send_hw_err_event_count = %d", send_hw_err_event_count);
			if (send_hw_err_event_count >= sizeof(hwerr_event)) {
				send_hw_err_event_count  = 0;
				BTMTK_WARN("set need_reset_stack=0");
				need_reset_stack = 0;
				need_reopen = 1;
				kill_fasync(&fasync, SIGIO, POLL_IN);
			}
			BTMTK_WARN("set call up");
			goto OUT;
		} else {
			BTMTK_ERR("xx set copyLen = -EFAULT");
			copyLen = -EFAULT;
			goto OUT;
		}
	}

	if (get_hci_reset == 1) {
		BTMTK_INFO("get reset complete and set audio!");
		btmtk_sdio_send_audio_slave();
		if (g_card->pa_setting != -1)
			btmtk_set_pa(g_card->pa_setting);
		if (g_card->duplex_setting != -1)
			btmtk_set_duplex(g_card->duplex_setting);
		if (g_card->efuse_mode == EFUSE_BIN_FILE_MODE)
			btmtk_set_eeprom2ctrler((uint8_t *)g_card->bin_file_buffer,
					g_card->bin_file_size,
					g_card->func->device);
		if (g_card->bt_cfg.support_auto_picus == true)
			btmtk_sdio_picus_operation(true);

		get_hci_reset = 0;
	}

	LOCK_UNSLEEPABLE_LOCK(&(metabuffer.spin_lock));
	if (skb_queue_empty(&g_card->fops_queue)) {
		/* if (filp->f_flags & O_NONBLOCK) { */
		if (metabuffer.write_p == metabuffer.read_p) {
			UNLOCK_UNSLEEPABLE_LOCK(&(metabuffer.spin_lock));
			up(&g_priv->rd_mtx);
			return 0;
		}
	}

	if (need_reset_stack == 1) {
		kill_fasync(&fasync, SIGIO, POLL_IN);
		need_reset_stack = 0;
		BTMTK_ERR("Call kill_fasync and set reset_stack 0");
		UNLOCK_UNSLEEPABLE_LOCK(&(metabuffer.spin_lock));
		up(&g_priv->rd_mtx);
		return -ENODEV;
	}

	do {
		skb = skb_dequeue(&g_card->fops_queue);
		if (skb == NULL) {
			BTMTK_DBG("skb=NULL break");
			break;
		}

		btmtk_print_buffer_conent(skb->data, skb->len);

		if (btmtk_sdio_push_data_to_metabuffer(&metabuffer, skb->data,
				skb->len, bt_cb(skb)->pkt_type, true) < 0) {
			skb_queue_head(&g_card->fops_queue, skb);
			break;
		}
		kfree_skb(skb);
	} while (!skb_queue_empty(&g_card->fops_queue));
	UNLOCK_UNSLEEPABLE_LOCK(&(metabuffer.spin_lock));

	up(&g_priv->rd_mtx);
	return btmtk_sdio_pull_data_from_metabuffer(&metabuffer, buf, count);

OUT:
	up(&g_priv->rd_mtx);
	return copyLen;
}

static int btmtk_fops_fasync(int fd, struct file *file, int on)
{
	BTMTK_INFO("fd = 0x%X, flag = 0x%X", fd, on);
	return fasync_helper(fd, file, on, &fasync);
}

unsigned int btmtk_fops_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	if (!probe_ready) {
		BTMTK_ERR("%s probe_ready is %d return\n", __func__, probe_ready);
		return 0;
	}

	if (g_priv == NULL) {
		BTMTK_ERR("g_priv is NULL");
		return 0;
	}

	poll_wait(filp, &inq, wait);

	if (metabuffer.write_p != metabuffer.read_p || need_reset_stack)
		mask |= (POLLIN | POLLRDNORM);

	if (!skb_queue_empty(&g_card->fops_queue)) {
		if (skb_queue_len(&g_card->fops_queue))
			mask |= (POLLIN | POLLRDNORM);
		/* BTMTK_INFO("poll done"); */
	}

	mask |= (POLLOUT | POLLWRNORM);

	/* BTMTK_INFO("poll mask 0x%x",mask); */
	return mask;
}

static long btmtk_fops_unlocked_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	u32 ret = 0;
	struct bt_stereo_para stereo_para;
	struct sk_buff *skb = NULL;
	u8 set_btclk[] = {0x01, 0x02, 0xFD, 0x0B,
		0x00, 0x00,			/* Handle */
		0x00,				/* Method */
						/* bit0~3 = 0x0:CVSD remove, 0x1:GPIO, 0x2:In-band with transport*/
						/* bit4~7 = 0x0:Shared memory, 0x1:auto event */
		0x00, 0x00, 0x00, 0x00,		/* Period = value * 0.625ms */
		0x09,				/* GPIO num - 0x01:BGF_INT_B, 0x09:GPIO0 */
		0x01,				/* trigger mode - 0x00:Low, 0x01:High */
		0x00, 0x00};			/* active slots = value * 0.625ms */


	switch (cmd) {
	case BTMTK_IOCTL_STEREO_GET_CLK:
		BTMTK_DBG("BTMTK_IOCTL_STEREO_GET_CLK cmd");

		LOCK_UNSLEEPABLE_LOCK(&stereo_spin_lock);
		if (copy_to_user((struct bt_stereo_clk __user *)arg, &stereo_clk, sizeof(struct bt_stereo_clk)))
			ret = -ENOMEM;
		UNLOCK_UNSLEEPABLE_LOCK(&stereo_spin_lock);
		break;
	case BTMTK_IOCTL_STEREO_SET_PARA:
		BTMTK_DBG("BTMTK_IOCTL_STEREO_SET_PARA cmd");
		if (copy_from_user(&stereo_para, (struct bt_stereo_para __user *)arg,
						sizeof(struct bt_stereo_para)))
			return -ENOMEM;

		if (stereo_para.period != 0)
			btmtk_stereo_reg_irq();

		/* Send and check HCI cmd */
		memcpy(&set_btclk[4], &stereo_para.handle, sizeof(stereo_para.handle));
		memcpy(&set_btclk[6], &stereo_para.method, sizeof(stereo_para.method));
		memcpy(&set_btclk[7], &stereo_para.period, sizeof(stereo_para.period));
		memcpy(&set_btclk[13], &stereo_para.active_slots, sizeof(stereo_para.active_slots));
#if SUPPORT_MT7668
		if (is_mt7668(g_card))
			set_btclk[11] = 0x09;
#endif
#if SUPPORT_MT7663
		if (is_mt7663(g_card))
			set_btclk[11] = 0x0B;
#endif

		skb = bt_skb_alloc(sizeof(set_btclk) - 1, GFP_ATOMIC);
		bt_cb(skb)->pkt_type = set_btclk[0];
		memcpy(&skb->data[0], &set_btclk[1], sizeof(set_btclk) - 1);

		skb->len = sizeof(set_btclk) - 1;
		skb_queue_tail(&g_card->tx_queue, skb);
		wake_up_interruptible(&g_priv->main_thread.wait_q);

		if (stereo_para.period == 0)
			btmtk_stereo_unreg_irq();
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int btmtk_fops_openfwlog(struct inode *inode,
					struct file *file)
{
	if (g_priv == NULL) {
		BTMTK_ERR("ERROR, g_priv is NULL!");
		return -ENODEV;
	}

	sema_init(&g_priv->wr_fwlog_mtx, 1);
	BTMTK_INFO("OK");
	return 0;
}

static int btmtk_fops_closefwlog(struct inode *inode,
					struct file *file)
{
	if (g_priv == NULL) {
		BTMTK_ERR("ERROR, g_priv is NULL!");
		return -ENODEV;
	}

	BTMTK_INFO("OK");
	return 0;
}

static ssize_t btmtk_fops_readfwlog(struct file *filp,
			char __user *buf,
			size_t count,
			loff_t *f_pos)
{
	struct sk_buff *skb = NULL;
	int copyLen = 0;

	if (g_priv == NULL || g_priv->adapter == NULL) {
		BTMTK_ERR("g_priv is NULL");
		return -EFAULT;
	}

	if (g_card->suspend_count) {
		BTMTK_ERR("suspend_count is %d", g_card->suspend_count);
		return -EAGAIN;
	}

	/* picus read a queue, it may occur performace issue */
	LOCK_UNSLEEPABLE_LOCK(&(fwlog_metabuffer.spin_lock));
	if (skb_queue_len(&g_card->fwlog_fops_queue))
		skb = skb_dequeue(&g_card->fwlog_fops_queue);
	UNLOCK_UNSLEEPABLE_LOCK(&(fwlog_metabuffer.spin_lock));

	if (skb == NULL)
		return 0;

	if (skb->len <= count) {
		if (copy_to_user(buf, skb->data, skb->len)) {
			BTMTK_ERR("copy_to_user failed!");
			/* copy_to_user failed, add skb to fwlog_fops_queue */
			skb_queue_head(&g_card->fwlog_fops_queue, skb);
			copyLen = -EFAULT;
			goto OUT;
		}
		copyLen = skb->len;
	} else {
		BTMTK_ERR("socket buffer length error(count: %d, skb.len: %d)", (int)count, skb->len);
	}
	kfree_skb(skb);
OUT:
	return copyLen;
}

static ssize_t btmtk_fops_writefwlog(
			struct file *filp, const char __user *buf,
			size_t count, loff_t *f_pos)
{
	struct sk_buff *skb = NULL;
	int i = 0, len = 0, val = 0, ret = -1;
	/*+1 is for i_fwlog_buf[count] = 0, end string byte*/
	int i_fwlog_buf_size = HCI_MAX_COMMAND_BUF_SIZE + 1;
	u8 *i_fwlog_buf = NULL;
	u8 *o_fwlog_buf = NULL;
	int fops_state = 0;
	u32 crAddr = 0, crValue = 0;

	if (g_priv == NULL || g_priv->adapter == NULL) {
		BTMTK_ERR("g_priv is NULL");
		goto exit;
	}

	down(&g_priv->wr_fwlog_mtx);

	FOPS_MUTEX_LOCK();
	fops_state = btmtk_fops_get_state();
	FOPS_MUTEX_UNLOCK();
	if (fops_state != BTMTK_FOPS_STATE_OPENED) {
		BTMTK_ERR("fops_state is %d", fops_state);
		count = -EFAULT;
		goto exit;
	}

	if (g_card->suspend_count) {
		BTMTK_ERR("suspend_count is %d", g_card->suspend_count);
		count = -EAGAIN;
		goto exit;
	}

	i_fwlog_buf = kmalloc(i_fwlog_buf_size, GFP_KERNEL);
	o_fwlog_buf = kmalloc(HCI_MAX_COMMAND_SIZE, GFP_KERNEL);

	if (i_fwlog_buf == NULL || o_fwlog_buf == NULL) {
		BTMTK_ERR("buf alloc fail");
		count = -ENOMEM;
		goto exit;
	}

	memset(i_fwlog_buf, 0, i_fwlog_buf_size);
	memset(o_fwlog_buf, 0, HCI_MAX_COMMAND_SIZE);

	if (count > HCI_MAX_COMMAND_BUF_SIZE) {
		BTMTK_ERR("your command is larger than maximum length, count = %zd", count);
		count = -EFAULT;
		goto exit;
	}

	if (copy_from_user(i_fwlog_buf, buf, count) != 0) {
		BTMTK_ERR("copy_from_user failed!");
		count = -EFAULT;
		goto exit;
	}

	i_fwlog_buf[count] = 0;

	if (strstr(i_fwlog_buf, FW_OWN_OFF)) {
		BTMTK_WARN("set FW_OWN_OFF");
		btmtk_sdio_set_no_fw_own(g_priv, true);
		len = count;
		wake_up_interruptible(&g_priv->main_thread.wait_q);
		goto exit;
	}

	if (strstr(i_fwlog_buf, FW_OWN_ON)) {
		BTMTK_WARN("set FW_OWN_ON");
		btmtk_sdio_set_no_fw_own(g_priv, false);
		len = count;
		wake_up_interruptible(&g_priv->main_thread.wait_q);
		goto exit;
	}

	if (strstr(i_fwlog_buf, WOBLE_ON)) {
		BTMTK_INFO("set WOBLE_ON");
		btmtk_sdio_handle_entering_WoBLE_state(0);
		len = count;
		if (g_card->bt_cfg.support_woble_by_eint) {
			if (g_card->wobt_irq != 0 &&
				atomic_read(&(g_card->irq_enable_count)) == 0) {
				BTMTK_INFO("enable BT IRQ:%d", g_card->wobt_irq);
				irq_set_irq_wake(g_card->wobt_irq, 1);
				enable_irq(g_card->wobt_irq);
				atomic_inc(&(g_card->irq_enable_count));
			} else
				BTMTK_INFO("irq_enable count:%d",
					atomic_read(&(g_card->irq_enable_count)));
		}
		goto exit;
	}

	if (strstr(i_fwlog_buf, WOBLE_OFF)) {
		BTMTK_INFO("set WOBLE_OFF");
		if (g_card->bt_cfg.support_woble_by_eint) {
			if (g_card->wobt_irq != 0 &&
				atomic_read(&(g_card->irq_enable_count)) == 1) {
				BTMTK_INFO("disable BT IRQ:%d", g_card->wobt_irq);
				atomic_dec(&(g_card->irq_enable_count));
				disable_irq_nosync(g_card->wobt_irq);
			} else
				BTMTK_INFO(":irq_enable count:%d",
					atomic_read(&(g_card->irq_enable_count)));
		}
		btmtk_sdio_send_leave_woble_suspend_cmd();
		len = count;
		goto exit;
	}

	if (strstr(i_fwlog_buf, RX_CHECK_ON)) {
		/* echo rx check on 0=100000 */
		int type, i;

		if (strstr(i_fwlog_buf, "=")) {
			type = i_fwlog_buf[strlen(RX_CHECK_ON) + 1] - '0';
			if (type >= BTMTK_SDIO_RX_CHECKPOINT_NUM) {
				BTMTK_WARN("set RX_CHECK_ON failed. type=%d", type);
				goto exit;
			}
			timestamp_threshold[type] = 0;
			for (i = strlen("rx check on 0="); (i_fwlog_buf[i] >= '0' && i_fwlog_buf[i] <= '9'); i++) {
				timestamp_threshold[type] = timestamp_threshold[type] * 10;
				timestamp_threshold[type] += i_fwlog_buf[i] - '0';
			}
		} else {
			type = 0;
			timestamp_threshold[BTMTK_SDIO_RX_CHECKPOINT_INTR] = 50000;
		}
		BTMTK_WARN("set RX_CHECK_ON %d=%d", type, timestamp_threshold[type]);
		goto exit;
	}

	if (strstr(i_fwlog_buf, RX_CHECK_OFF)) {
		BTMTK_WARN("set RX_CHECK_OFF");
		memset(timestamp_threshold, 0, sizeof(timestamp_threshold));
		goto exit;
	}

	if (strstr(i_fwlog_buf, RELOAD_SETTING)) {
		BTMTK_INFO("set RELOAD_SETTING");
		btmtk_eeprom_bin_file(g_card);
		len = count;
		goto exit;
	}

	/* For log_lvl, EX: echo log_lvl=4 > /dev/stpbtfwlog */
	if (strcmp(i_fwlog_buf, "log_lvl=") > 0) {
		val = *(i_fwlog_buf + strlen("log_lvl=")) - 48;

		if (val > BTMTK_LOG_LEVEL_MAX || val <= 0) {
			BTMTK_ERR("Got incorrect value for log level(%d)", val);
			count = -EINVAL;
			goto exit;
		}
		btmtk_log_lvl = val;
		BTMTK_INFO("btmtk_log_lvl = %d", btmtk_log_lvl);
		goto exit;
	}

	/* For btmtk_bluetooth_kpi, EX: echo bperf=1 > /dev/stpbtfwlog */
	if (strcmp(i_fwlog_buf, "bperf=") >= 0) {
		u8 val = *(i_fwlog_buf + strlen("bperf=")) - 48;

		btmtk_bluetooth_kpi = val;
		BTMTK_INFO("set bluetooth KPI feature(bperf) to %d", btmtk_bluetooth_kpi);
		goto exit;
	}

	/* hci input command format : echo 01 be fc 01 05 > /dev/stpbtfwlog */
	/* We take the data from index three to end. */
	for (i = 0; i < count; i++) {
		char *pos = i_fwlog_buf + i;
		char temp_str[3] = {'\0'};
		long res = 0;

		if (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') {
			continue;
		} else if (*pos == '0' && (*(pos + 1) == 'x' || *(pos + 1) == 'X')) {
			i++;
			continue;
		} else if (!(*pos >= '0' && *pos <= '9') && !(*pos >= 'A' && *pos <= 'F')
			&& !(*pos >= 'a' && *pos <= 'f')) {
			BTMTK_ERR("There is an invalid input(%c)", *pos);
			count = -EINVAL;
			goto exit;
		}
		temp_str[0] = *pos;
		temp_str[1] = *(pos + 1);
		i++;
		ret = kstrtol(temp_str, 16, &res);
		if (ret == 0) {
			o_fwlog_buf[len++] = (u8)res;
			if (len >= (HCI_MAX_COMMAND_SIZE - 1) && ((i + 1) < len)) {
				BTMTK_WARN("buf is full, input format may error");
				goto exit;
			}
		}
		else
			BTMTK_ERR("Convert %s failed(%d)", temp_str, ret);
	}

	/*
	 * Receive command from stpbtfwlog, then Sent hci command
	 * to controller
	 */
	BTMTK_DBG("hci buff is %02x%02x%02x%02x%02x, length %d",
		o_fwlog_buf[0], o_fwlog_buf[1],
		o_fwlog_buf[2], o_fwlog_buf[3], o_fwlog_buf[4], len);

	/* check HCI command length */
	if (len > HCI_MAX_COMMAND_SIZE) {
		BTMTK_ERR("your command is larger than maximum length, length = %d", len);
		goto exit;
	}

	if (len <= 1) {
		BTMTK_ERR("length = %d, command format may error", len);
		goto exit;
	}

	BTMTK_DBG("hci buff is %02x%02x%02x%02x%02x",
		o_fwlog_buf[0], o_fwlog_buf[1],
		o_fwlog_buf[2], o_fwlog_buf[3], o_fwlog_buf[4]);

	switch (o_fwlog_buf[0]) {
	case MTK_HCI_READ_CR_PKT:
		if (len == MTK_HCI_READ_CR_PKT_LENGTH) {
			crAddr = (o_fwlog_buf[1] << 24) + (o_fwlog_buf[2] << 16) +
			(o_fwlog_buf[3] << 8) + (o_fwlog_buf[4]);
			btmtk_sdio_readl(crAddr, &crValue);
			BTMTK_INFO("read crAddr=0x%08x crValue=0x%08x", crAddr, crValue);
		} else
			BTMTK_INFO("read length=%d is incorrect, should be %d",
				len, MTK_HCI_READ_CR_PKT_LENGTH);
		break;

	case MTK_HCI_WRITE_CR_PKT:
		if (len == MTK_HCI_WRITE_CR_PKT_LENGTH) {
			crAddr = (o_fwlog_buf[1] << 24) + (o_fwlog_buf[2] << 16) +
			(o_fwlog_buf[3] << 8) + (o_fwlog_buf[4]);
			crValue = (o_fwlog_buf[5] << 24) + (o_fwlog_buf[6] << 16) +
			(o_fwlog_buf[7] << 8) + (o_fwlog_buf[8]);
			BTMTK_INFO("write crAddr=0x%08x crValue=0x%08x",
				crAddr, crValue);
			btmtk_sdio_writel(crAddr, crValue);
		} else
			BTMTK_INFO("write length=%d is incorrect, should be %d",
				len, MTK_HCI_WRITE_CR_PKT_LENGTH);
		break;

	default:
		/*
		 * Receive command from stpbtfwlog, then Sent hci command
		 * to Stack
		 */
		skb = bt_skb_alloc(len - 1, GFP_ATOMIC);
		if (skb == NULL) {
			BTMTK_WARN("skb is null");
			count = -ENOMEM;
			goto exit;
		}
		bt_cb(skb)->pkt_type = o_fwlog_buf[0];
		memcpy(&skb->data[0], &o_fwlog_buf[1], len - 1);
		skb->len = len - 1;
		skb_queue_tail(&g_card->tx_queue, skb);
		wake_up_interruptible(&g_priv->main_thread.wait_q);
		break;
	}



	BTMTK_INFO("write end");
exit:
	BTMTK_INFO("exit, length = %d", len);
	if (i_fwlog_buf)
		kfree(i_fwlog_buf);
	if (o_fwlog_buf)
		kfree(o_fwlog_buf);
	if (g_priv)
		up(&g_priv->wr_fwlog_mtx);
	return count;
}

static unsigned int btmtk_fops_pollfwlog(
			struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	if (g_priv == NULL) {
		BTMTK_ERR("g_priv is NULL");
		return 0;
	}

	poll_wait(file, &fw_log_inq, wait);

	if (!skb_queue_empty(&g_card->fwlog_fops_queue)) {
		if (skb_queue_len(&g_card->fwlog_fops_queue))
			mask |= (POLLIN | POLLRDNORM);
		/* BTMTK_INFO("poll done"); */
	}

	mask |= (POLLOUT | POLLWRNORM);

	/* BTMTK_INFO("poll mask 0x%x",mask); */
	return mask;
}

static long btmtk_fops_unlocked_ioctlfwlog(
			struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	int retval = 0;

	BTMTK_INFO("->");
	if (g_priv == NULL) {
		BTMTK_ERR("ERROR, g_priv is NULL!");
		return -ENODEV;
	}

	return retval;
}

/*============================================================================*/
/* Interface Functions : Proc */
/*============================================================================*/
#define __________________________________________Interface_Function_for_Proc
static int btmtk_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "patch version:%s  driver version:%s", fw_version_str, VERSION);
	return 0;
}

static int btmtk_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, btmtk_proc_show, NULL);
}

void btmtk_proc_create_new_entry(void)
{
	struct proc_dir_entry *proc_show_entry;

	BTMTK_INFO("proc initialized");

	g_proc_dir = proc_mkdir("stpbt", 0);
	if (g_proc_dir == 0) {
		BTMTK_ERR("Unable to creat dir");
		return;
	}
	proc_show_entry =  proc_create("bt_fw_version", 0644, g_proc_dir, &BT_proc_fops);
}

static int BTMTK_major;
static int BT_majorfwlog;
static struct cdev BTMTK_cdev;
static struct cdev BT_cdevfwlog;
static int BTMTK_devs = 1;

static wait_queue_head_t inq;
const struct file_operations BTMTK_fops = {
	.open = btmtk_fops_open,
	.release = btmtk_fops_close,
	.read = btmtk_fops_read,
	.write = btmtk_fops_write,
	.poll = btmtk_fops_poll,
	.unlocked_ioctl = btmtk_fops_unlocked_ioctl,
	.fasync = btmtk_fops_fasync
};

const struct file_operations BT_fopsfwlog = {
	.open = btmtk_fops_openfwlog,
	.release = btmtk_fops_closefwlog,
	.read = btmtk_fops_readfwlog,
	.write = btmtk_fops_writefwlog,
	.poll = btmtk_fops_pollfwlog,
	.unlocked_ioctl = btmtk_fops_unlocked_ioctlfwlog

};

static int BTMTK_init(void)
{
	dev_t devID = MKDEV(BTMTK_major, 0);
	dev_t devIDfwlog = MKDEV(BT_majorfwlog, 1);
	int ret = 0;
	int cdevErr = 0;
	int major = 0;
	int majorfwlog = 0;

	BTMTK_INFO("begin");

	g_card = NULL;
	txbuf = NULL;
	rxbuf = NULL;
	userbuf = NULL;
	rx_length = 0;
	fw_dump_file = NULL;
	g_priv = NULL;
	probe_counter = 0;

	btmtk_proc_create_new_entry();

	ret = alloc_chrdev_region(&devID, 0, 1, "BT_chrdev");
	if (ret) {
		BTMTK_ERR("fail to allocate chrdev");
		return ret;
	}

	ret = alloc_chrdev_region(&devIDfwlog, 0, 1, "BT_chrdevfwlog");
	if (ret) {
		BTMTK_ERR("fail to allocate chrdev");
		return ret;
	}

	BTMTK_major = major = MAJOR(devID);
	BTMTK_INFO("major number:%d", BTMTK_major);
	BT_majorfwlog = majorfwlog = MAJOR(devIDfwlog);
	BTMTK_INFO("BT_majorfwlog number: %d", BT_majorfwlog);

	cdev_init(&BTMTK_cdev, &BTMTK_fops);
	BTMTK_cdev.owner = THIS_MODULE;

	cdev_init(&BT_cdevfwlog, &BT_fopsfwlog);
	BT_cdevfwlog.owner = THIS_MODULE;

	cdevErr = cdev_add(&BTMTK_cdev, devID, BTMTK_devs);
	if (cdevErr)
		goto error;

	cdevErr = cdev_add(&BT_cdevfwlog, devIDfwlog, 1);
	if (cdevErr)
		goto error;

	BTMTK_INFO("%s driver(major %d) installed.",
			"BT_chrdev", BTMTK_major);
	BTMTK_INFO("%s driver(major %d) installed.",
			"BT_chrdevfwlog", BT_majorfwlog);

	pBTClass = class_create(THIS_MODULE, "BT_chrdev");
	if (IS_ERR(pBTClass)) {
		BTMTK_ERR("class create fail, error code(%ld)",
			PTR_ERR(pBTClass));
		goto error;
	}

	pBTDev = device_create(pBTClass, NULL, devID, NULL, BT_NODE);
	if (IS_ERR(pBTDev)) {
		BTMTK_ERR("device create fail, error code(%ld)",
			PTR_ERR(pBTDev));
		goto err2;
	}

	pBTDevfwlog = device_create(pBTClass, NULL,
				devIDfwlog, NULL, "stpbtfwlog");
	if (IS_ERR(pBTDevfwlog)) {
		BTMTK_ERR("device(stpbtfwlog) create fail, error code(%ld)",
			PTR_ERR(pBTDevfwlog));
		goto err2;
	}

	BTMTK_INFO("BT_major %d, BT_majorfwlog %d", BTMTK_major, BT_majorfwlog);
	BTMTK_INFO("devID %d, devIDfwlog %d", devID, devIDfwlog);

	FOPS_MUTEX_LOCK();
	btmtk_fops_set_state(BTMTK_FOPS_STATE_INIT);
	FOPS_MUTEX_UNLOCK();

	/* init wait queue */
	g_devIDfwlog = devIDfwlog;
	init_waitqueue_head(&(fw_log_inq));
	init_waitqueue_head(&(inq));

	return 0;

 err2:
	if (pBTClass) {
		class_destroy(pBTClass);
		pBTClass = NULL;
	}

 error:
	if (cdevErr == 0)
		cdev_del(&BTMTK_cdev);

	if (ret == 0)
		unregister_chrdev_region(devID, BTMTK_devs);

	return -1;
}

static void BTMTK_exit(void)
{
	dev_t dev = MKDEV(BTMTK_major, 0);
	dev_t devIDfwlog = g_devIDfwlog;

	BTMTK_INFO("begin");

	if (g_proc_dir != NULL) {
		remove_proc_entry("bt_fw_version", g_proc_dir);
		remove_proc_entry("stpbt", NULL);
		g_proc_dir = NULL;
		BTMTK_INFO("proc device node and folder removed!!");
	}

	if (pBTDevfwlog) {
		BTMTK_INFO("6");
		device_destroy(pBTClass, devIDfwlog);
		pBTDevfwlog = NULL;
	}

	if (pBTDev) {
		device_destroy(pBTClass, dev);
		pBTDev = NULL;
	}

	if (pBTClass) {
		class_destroy(pBTClass);
		pBTClass = NULL;
	}
	cdev_del(&BTMTK_cdev);
	unregister_chrdev_region(dev, 1);

	cdev_del(&BT_cdevfwlog);
	unregister_chrdev_region(devIDfwlog, 1);
	BTMTK_INFO("%s driver removed.", BT_DRIVER_NAME);
}

static int btmtk_sdio_allocate_memory(void)
{
	txbuf = kmalloc(MTK_TXDATA_SIZE, GFP_ATOMIC);
	memset(txbuf, 0, MTK_TXDATA_SIZE);

	rxbuf = kmalloc(MTK_RXDATA_SIZE, GFP_ATOMIC);
	memset(rxbuf, 0, MTK_RXDATA_SIZE);

	userbuf = kmalloc(MTK_TXDATA_SIZE, GFP_ATOMIC);
	memset(userbuf, 0, MTK_TXDATA_SIZE);

	userbuf_fwlog = kmalloc(MTK_TXDATA_SIZE, GFP_ATOMIC);
	memset(userbuf_fwlog, 0, MTK_TXDATA_SIZE);

	g_card = kzalloc(sizeof(*g_card), GFP_KERNEL);
	memset(g_card, 0, sizeof(*g_card));

	g_priv = kzalloc(sizeof(*g_priv), GFP_KERNEL);
	memset(g_priv, 0, sizeof(*g_priv));

	g_priv->adapter = kzalloc(sizeof(*g_priv->adapter), GFP_KERNEL);
	memset(g_priv->adapter, 0, sizeof(*g_priv->adapter));

	g_card->priv = g_priv;

	skb_queue_head_init(&g_card->tx_queue);
	skb_queue_head_init(&g_card->fops_queue);
	skb_queue_head_init(&g_card->fwlog_fops_queue);

	return 0;
}

static int btmtk_sdio_free_memory(void)
{
	skb_queue_purge(&g_card->tx_queue);
	skb_queue_purge(&g_card->fops_queue);
	skb_queue_purge(&g_card->fwlog_fops_queue);

	kfree(txbuf);
	txbuf = NULL;

	kfree(rxbuf);
	rxbuf = NULL;

	kfree(userbuf);
	userbuf = NULL;

	kfree(userbuf_fwlog);
	userbuf_fwlog = NULL;

	kfree(g_card->priv->adapter);
	g_card->priv->adapter = NULL;

	kfree(g_card->priv);
	g_card->priv = NULL;
	g_priv = NULL;

	kfree(g_card);
	g_card = NULL;

	return 0;
}

static int __init btmtk_sdio_init_module(void)
{
	int ret = 0;

	ret = BTMTK_init();
	if (ret) {
		BTMTK_ERR("BTMTK_init failed!");
		return ret;
	}

	if (btmtk_sdio_allocate_memory() < 0) {
		BTMTK_ERR("allocate memory failed!");
		return -ENOMEM;
	}

	if (sdio_register_driver(&bt_mtk_sdio) != 0) {
		BTMTK_ERR("SDIO Driver Registration Failed");
		return -ENODEV;
	}

	BTMTK_INFO("SDIO Driver Registration Success");

	/* Clear the flag in case user removes the card. */
	user_rmmod = 0;

	return 0;
}

static void __exit btmtk_sdio_exit_module(void)
{
	/* Set the flag as user is removing this module. */
	user_rmmod = 1;
	BTMTK_exit();
	sdio_unregister_driver(&bt_mtk_sdio);
	btmtk_sdio_free_memory();
}

module_init(btmtk_sdio_init_module);
module_exit(btmtk_sdio_exit_module);
