/*
 *  Copyright (c) 2016,2017 MediaTek Inc.
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

#define SAVE_FW_DUMP_IN_KERNEL	1

#define SUPPORT_FW_DUMP		1
#define BTM_HEADER_LEN                  5
#define BTM_UPLD_SIZE                   2312

#define MTK_TXDATA_SIZE 2000
#define MTK_RXDATA_SIZE 2000

/* Time to wait until Host Sleep state change in millisecond */
#define WAIT_UNTIL_HS_STATE_CHANGED     msecs_to_jiffies(5000)
/* Time to wait for command response in millisecond */
#define WAIT_UNTIL_CMD_RESP             msecs_to_jiffies(5000)

#define BTMTK_BIN_FILE_MODE 1
#if BTMTK_BIN_FILE_MODE
/** For 7668 please storage cfg/bin file in ${firmware} */
#define E2P_ACCESS_MODE_SWITCHER	"wifi.cfg"
#define E2P_BIN_FILE			"EEPROM_MT%X.bin"

#define E2P_MODE	"EfuseBufferModeCal"
#define BIN_FILE_MODE	'1'
#endif

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
};

struct btmtk_device {
	void *card;
	/* struct hci_dev *hcidev; */

	u8 dev_type;

	u8 tx_dnld_rdy;

	u8 psmode;
	u8 pscmd;
	u8 hsmode;
	u8 hscmd;

	/* Low byte is gap, high byte is GPIO */
	u16 gpio_gap;

	u8 hscfgcmd;
	u8 sendcmdflag;
};

struct btmtk_adapter {
	void *hw_regs_buf;
	u8 *hw_regs;
	u32 int_count;
	struct sk_buff_head tx_queue;
	struct sk_buff_head fops_queue;
	struct sk_buff_head fwlog_fops_queue;
	struct sk_buff_head fwlog_tx_queue;
	u8 fops_mode;
	u8 psmode;
	u8 ps_state;
	u8 hs_state;
	u8 wakeup_tries;
	wait_queue_head_t cmd_wait_q;
	wait_queue_head_t event_hs_wait_q;
	u8 cmd_complete;
	bool is_suspended;
};

struct btmtk_private {
	struct btmtk_device btmtk_dev;
	struct btmtk_adapter *adapter;
	struct btmtk_thread main_thread;
	int (*hw_host_to_card)(struct btmtk_private *priv,
				u8 *payload, u16 nb);

	int (*hw_set_own_back)(int owntype);
	int (*hw_process_int_status)(struct btmtk_private *priv);
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


/* #define SAVE_FW_DUMP_IN_KERNEL     1 */

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
int btmtk_enable_hs(struct btmtk_private *priv);
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

#ifdef CONFIG_DEBUG_FS

#define FIXED_STPBT_MAJOR_DEV_ID 111



#define FW_DUMP_END_EVENT "coredump end"

#endif

#endif

