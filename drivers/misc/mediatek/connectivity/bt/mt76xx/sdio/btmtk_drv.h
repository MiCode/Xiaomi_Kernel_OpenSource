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

#ifndef _BTMTK_DRV_H_
#define _BTMTK_DRV_H_

#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <net/bluetooth/bluetooth.h>

#define SUPPORT_FW_DUMP		1
#define BTM_HEADER_LEN                  5
#define BTM_UPLD_SIZE                   2312

#ifndef SUPPORT_CR_WR
#define SUPPORT_CR_WR 0
#endif

#define MTK_TXDATA_SIZE 2000
#define MTK_RXDATA_SIZE 2000

/* Time to wait until Host Sleep state change in millisecond */
#define WAIT_UNTIL_HS_STATE_CHANGED	msecs_to_jiffies(5000)
/* Time to wait for command response in millisecond */
#define WAIT_UNTIL_CMD_RESP		msecs_to_jiffies(5000)

/** For 7668 please storage cfg/bin file in ${firmware} */
#define E2P_ACCESS_MODE_SWITCHER	"wifi.cfg"
#define E2P_BIN_FILE			"EEPROM_MT%X.bin"

#define E2P_MODE			"EfuseBufferModeCal"
#define E2P_ACCESS_EPA			"BtUseExternalPA"
#define E2P_ACCESS_DUPLEX		"BtDuplexMode"
#define TX_PWR_LIMIT			"BtTxPwrLimit.bin"
#define KEEP_FULL_PWR			"KeepFullPwr"
#define PWR_KEEP_NO_FW_OWN		'1'
#define PWR_SWITCH_DRIVER_FW_OWN	'0'
#define EFUSE_MODE			0
#define EFUSE_BIN_FILE_MODE		1
#define EFUSE_AUTO_MODE			2


enum rdwr_status {
	RDWR_STATUS_SUCCESS = 0,
	RDWR_STATUS_FAILURE = 1,
	RDWR_STATUS_DONE = 2
};

#define FW_DUMP_MAX_NAME_LEN    8
#define FW_DUMP_HOST_READY      0xEE
#define FW_DUMP_DONE            0xFF
#define FW_DUMP_READ_DONE       0xFE

struct memory_type_mapping {
	u8 mem_name[FW_DUMP_MAX_NAME_LEN];
	u8 *mem_ptr;
	u32 mem_size;
	u8 done_flag;
};

struct btmtk_thread {
	struct task_struct *task;
	wait_queue_head_t wait_q;
	void *priv;
	u8 thread_status;
};

struct btmtk_device {
	void *card;
	/* struct hci_dev *hcidev; */

	u8 reset_progress;
	u8 reset_dongle;
	u8 tx_dnld_rdy;
};

struct btmtk_adapter {
	void *hw_regs_buf;
	u8 *hw_regs;
	u32 int_count;
};

struct btmtk_private {
	struct btmtk_device btmtk_dev;
	struct btmtk_adapter *adapter;
	struct btmtk_thread main_thread;
	int (*hw_host_to_card)(struct btmtk_private *priv,
				u8 *payload, u16 nb);

	void (*start_reset_dongle_progress)(void);
	int (*hw_sdio_reset_dongle)(void);
	int (*hw_set_own_back)(int owntype);
	int (*hw_process_int_status)(struct btmtk_private *priv);
	void (*hci_snoop_save)(u8 type, u8 *buf, u32 len);
	void (*firmware_dump)(struct btmtk_private *priv);
	spinlock_t driver_lock;         /* spinlock used by driver */
#ifdef CONFIG_DEBUG_FS
	void *debugfs_data;
#endif
	bool surprise_removed;
#if SUPPORT_FW_DUMP
	struct semaphore fw_dump_semaphore;
	struct task_struct *fw_dump_tsk;
	struct task_struct *fw_dump_end_check_tsk;
#endif
	struct semaphore wr_mtx;
	struct semaphore rd_mtx;
	struct semaphore wr_fwlog_mtx;
	bool no_fw_own;
};

#define MTK_VENDOR_PKT                 0xFE

/* Vendor specific Bluetooth commands */
#define BT_CMD_PSCAN_WIN_REPORT_ENABLE  0xFC03
#define BT_CMD_ROUTE_SCO_TO_HOST        0xFC1D
#define BT_CMD_SET_BDADDR               0xFC22
#define BT_CMD_AUTO_SLEEP_MODE          0xFC23
#define BT_CMD_HOST_SLEEP_CONFIG        0xFC59
#define BT_CMD_HOST_SLEEP_ENABLE        0xFC5A
#define BT_CMD_MODULE_CFG_REQ           0xFC5B
#define BT_CMD_LOAD_CONFIG_DATA         0xFC61

/* Sub-commands: Module Bringup/Shutdown Request/Response */
#define MODULE_BRINGUP_REQ              0xF1
#define MODULE_BROUGHT_UP               0x00
#define MODULE_ALREADY_UP               0x0C

#define MODULE_SHUTDOWN_REQ             0xF2

/* Vendor specific Bluetooth events */
#define BT_EVENT_AUTO_SLEEP_MODE        0x23
#define BT_EVENT_HOST_SLEEP_CONFIG      0x59
#define BT_EVENT_HOST_SLEEP_ENABLE      0x5A
#define BT_EVENT_MODULE_CFG_REQ         0x5B
#define BT_EVENT_POWER_STATE            0x20

/* Bluetooth Power States */
#define BT_PS_ENABLE                    0x02
#define BT_PS_DISABLE                   0x03
#define BT_PS_SLEEP                     0x01

/* Host Sleep states */
#define HS_ACTIVATED                    0x01
#define HS_DEACTIVATED                  0x00

/* Power Save modes */
#define PS_SLEEP                        0x01
#define PS_AWAKE                        0x00

#define BT_CAL_HDR_LEN                  4
#define BT_CAL_DATA_SIZE                28

#define FW_DUMP_BUF_SIZE (1024*512)

#define FW_DUMP_FILE_NAME_SIZE     64

#define EVENT_COMPARE_SIZE     64


/* stpbt device node */
#define BT_NODE "stpbt"
#define BT_DRIVER_NAME "BT_chrdev"

struct btmtk_event {
	u8 ec;          /* event counter */
	u8 length;
	u8 data[4];
} __packed;

/* Prototype of global function */

struct btmtk_private *btmtk_add_card(void *card);
int btmtk_remove_card(struct btmtk_private *priv);

void btmtk_interrupt(struct btmtk_private *priv);

bool btmtk_check_evtpkt(struct btmtk_private *priv, struct sk_buff *skb);
int btmtk_process_event(struct btmtk_private *priv, struct sk_buff *skb);

int btmtk_send_module_cfg_cmd(struct btmtk_private *priv, u8 subcmd);
int btmtk_pscan_window_reporting(struct btmtk_private *priv, u8 subcmd);
int btmtk_send_hscfg_cmd(struct btmtk_private *priv);
int btmtk_enable_ps(struct btmtk_private *priv);
int btmtk_prepare_command(struct btmtk_private *priv);
void btmtk_firmware_dump(struct btmtk_private *priv);

#define META_BUFFER_SIZE (1024*50)

struct _OSAL_UNSLEEPABLE_LOCK_ {
	spinlock_t lock;
	unsigned long flag;
};

struct ring_buffer {
	struct _OSAL_UNSLEEPABLE_LOCK_ spin_lock;
	u8 buffer[META_BUFFER_SIZE];	/* MTKSTP_BUFFER_SIZE:1024 */
	u32 read_p;		/* indicate the current read index */
	u32 write_p;		/* indicate the current write index */
};

#define FIXED_STPBT_MAJOR_DEV_ID 111

#define FW_DUMP_END_EVENT "coredump end"

#endif

