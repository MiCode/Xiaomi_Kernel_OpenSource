/* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
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

#ifndef DIAGCHAR_H
#define DIAGCHAR_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/mempool.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <mach/msm_smd.h>
#include <asm/atomic.h>
#include <asm/mach-types.h>

/* Size of the USB buffers used for read and write*/
#define USB_MAX_OUT_BUF 4096
#define APPS_BUF_SIZE	2000
#define IN_BUF_SIZE		16384
#define MAX_IN_BUF_SIZE	32768
#define MAX_SYNC_OBJ_NAME_SIZE	32
/* Size of the buffer used for deframing a packet
  reveived from the PC tool*/
#define HDLC_MAX 4096
#define HDLC_OUT_BUF_SIZE	8192
#define POOL_TYPE_COPY		1
#define POOL_TYPE_HDLC		2
#define POOL_TYPE_WRITE_STRUCT	4
#define POOL_TYPE_HSIC		5
#define POOL_TYPE_HSIC_2	6
#define POOL_TYPE_HSIC_WRITE	11
#define POOL_TYPE_HSIC_2_WRITE	12
#define POOL_TYPE_ALL		10
#define MODEM_DATA		0
#define LPASS_DATA		1
#define WCNSS_DATA		2
#define APPS_DATA		3
#define SDIO_DATA		4
#define HSIC_DATA		5
#define HSIC_2_DATA		6
#define SMUX_DATA		10
#define APPS_PROC		1
#define MSG_MASK_SIZE 10000
#define LOG_MASK_SIZE 8000
#define EVENT_MASK_SIZE 1000
#define USER_SPACE_DATA 8000
#define PKT_SIZE 4096
#define MAX_EQUIP_ID 15
#define DIAG_CTRL_MSG_LOG_MASK	9
#define DIAG_CTRL_MSG_EVENT_MASK	10
#define DIAG_CTRL_MSG_F3_MASK	11
#define CONTROL_CHAR	0x7E

#define DIAG_CON_APSS (0x0001)	/* Bit mask for APSS */
#define DIAG_CON_MPSS (0x0002)	/* Bit mask for MPSS */
#define DIAG_CON_LPASS (0x0004)	/* Bit mask for LPASS */
#define DIAG_CON_WCNSS (0x0008)	/* Bit mask for WCNSS */

/*
 * The status bit masks when received in a signal handler are to be
 * used in conjunction with the peripheral list bit mask to determine the
 * status for a peripheral. For instance, 0x00010002 would denote an open
 * status on the MPSS
 */
#define DIAG_STATUS_OPEN (0x00010000)	/* DCI channel open status mask   */
#define DIAG_STATUS_CLOSED (0x00020000)	/* DCI channel closed status mask */

#define MODE_REALTIME 1
#define MODE_NONREALTIME 0

#define NUM_SMD_DATA_CHANNELS 3
#define NUM_SMD_CONTROL_CHANNELS 3
#define NUM_SMD_DCI_CHANNELS 1

#define SMD_DATA_TYPE 0
#define SMD_CNTL_TYPE 1
#define SMD_DCI_TYPE 2

/* Maximum number of pkt reg supported at initialization*/
extern int diag_max_reg;
extern int diag_threshold_reg;

#define APPEND_DEBUG(ch) \
do {							\
	diag_debug_buf[diag_debug_buf_idx] = ch; \
	(diag_debug_buf_idx < 1023) ? \
	(diag_debug_buf_idx++) : (diag_debug_buf_idx = 0); \
} while (0)

/* List of remote processor supported */
enum remote_procs {
	MDM = 1,
	MDM2 = 2,
	MDM3 = 3,
	MDM4 = 4,
	QSC = 5,
};

struct diag_master_table {
	uint16_t cmd_code;
	uint16_t subsys_id;
	uint32_t client_id;
	uint16_t cmd_code_lo;
	uint16_t cmd_code_hi;
	int process_id;
};

struct bindpkt_params_per_process {
	/* Name of the synchronization object associated with this proc */
	char sync_obj_name[MAX_SYNC_OBJ_NAME_SIZE];
	uint32_t count;	/* Number of entries in this bind */
	struct bindpkt_params *params; /* first bind params */
};

struct bindpkt_params {
	uint16_t cmd_code;
	uint16_t subsys_id;
	uint16_t cmd_code_lo;
	uint16_t cmd_code_hi;
	/* For Central Routing, used to store Processor number */
	uint16_t proc_id;
	uint32_t event_id;
	uint32_t log_code;
	/* For Central Routing, used to store SMD channel pointer */
	uint32_t client_id;
};

struct diag_write_device {
	void *buf;
	int length;
};

struct diag_client_map {
	char name[20];
	int pid;
};

struct diag_nrt_wake_lock {
	int enabled;
	int ref_count;
	int copy_count;
	struct wake_lock read_lock;
	spinlock_t read_spinlock;
};

/* This structure is defined in USB header file */
#ifndef CONFIG_DIAG_OVER_USB
struct diag_request {
	char *buf;
	int length;
	int actual;
	int status;
	void *context;
};
#endif

struct diag_smd_info {
	int peripheral;	/* The peripheral this smd channel communicates with */
	int type;	/* The type of smd channel (data, control, dci) */
	uint16_t peripheral_mask;

	smd_channel_t *ch;
	smd_channel_t *ch_save;

	struct mutex smd_ch_mutex;

	int in_busy_1;
	int in_busy_2;

	unsigned char *buf_in_1;
	unsigned char *buf_in_2;

	struct diag_request *write_ptr_1;
	struct diag_request *write_ptr_2;

	struct diag_nrt_wake_lock nrt_lock;

	struct work_struct diag_read_smd_work;
	struct work_struct diag_notify_update_smd_work;
	int notify_context;

	/*
	 * Function ptr for function to call to process the data that
	 * was just read from the smd channel
	 */
	int (*process_smd_read_data)(struct diag_smd_info *smd_info,
						void *buf, int num_bytes);
};

struct diagchar_dev {

	/* State for the char driver */
	unsigned int major;
	unsigned int minor_start;
	int num;
	struct cdev *cdev;
	char *name;
	int dropped_count;
	struct class *diagchar_class;
	int ref_count;
	struct mutex diagchar_mutex;
	wait_queue_head_t wait_q;
	wait_queue_head_t smd_wait_q;
	struct diag_client_map *client_map;
	int *data_ready;
	int num_clients;
	int polling_reg_flag;
	struct diag_write_device *buf_tbl;
	int use_device_tree;
	/* DCI related variables */
	struct dci_pkt_req_tracking_tbl *req_tracking_tbl;
	struct diag_dci_client_tbl *dci_client_tbl;
	int dci_tag;
	int dci_client_id;
	struct mutex dci_mutex;
	int num_dci_client;
	unsigned char *apps_dci_buf;
	int dci_state;
	struct workqueue_struct *diag_dci_wq;
	/* Memory pool parameters */
	unsigned int itemsize;
	unsigned int poolsize;
	unsigned int itemsize_hdlc;
	unsigned int poolsize_hdlc;
	unsigned int itemsize_write_struct;
	unsigned int poolsize_write_struct;
	unsigned int debug_flag;
	/* State for the mempool for the char driver */
	mempool_t *diagpool;
	mempool_t *diag_hdlc_pool;
	mempool_t *diag_write_struct_pool;
	struct mutex diagmem_mutex;
	int count;
	int count_hdlc_pool;
	int count_write_struct_pool;
	int used;
	/* Buffers for masks */
	struct mutex diag_cntl_mutex;
	struct diag_ctrl_event_mask *event_mask;
	struct diag_ctrl_log_mask *log_mask;
	struct diag_ctrl_msg_mask *msg_mask;
	struct diag_ctrl_feature_mask *feature_mask;
	/* State for diag forwarding */
	int real_time_mode;
	struct diag_smd_info smd_data[NUM_SMD_DATA_CHANNELS];
	struct diag_smd_info smd_cntl[NUM_SMD_CONTROL_CHANNELS];
	struct diag_smd_info smd_dci[NUM_SMD_DCI_CHANNELS];
	unsigned char *usb_buf_out;
	unsigned char *apps_rsp_buf;
	unsigned char *user_space_data;
	/* buffer for updating mask to peripherals */
	unsigned char *buf_msg_mask_update;
	unsigned char *buf_log_mask_update;
	unsigned char *buf_event_mask_update;
	unsigned char *buf_feature_mask_update;
	int read_len_legacy;
	struct mutex diag_hdlc_mutex;
	unsigned char *hdlc_buf;
	unsigned hdlc_count;
	unsigned hdlc_escape;
	int in_busy_pktdata;
#ifdef CONFIG_DIAG_OVER_USB
	int usb_connected;
	struct usb_diag_ch *legacy_ch;
	struct work_struct diag_proc_hdlc_work;
	struct work_struct diag_read_work;
#endif
	struct workqueue_struct *diag_wq;
	struct work_struct diag_drain_work;
	struct workqueue_struct *diag_cntl_wq;
	uint8_t *msg_masks;
	uint8_t *log_masks;
	int log_masks_length;
	uint8_t *event_masks;
	uint8_t log_on_demand_support;
	struct diag_master_table *table;
	uint8_t *pkt_buf;
	int pkt_length;
	struct diag_request *usb_read_ptr;
	struct diag_request *write_ptr_svc;
	int logging_mode;
	int mask_check;
	int logging_process_id;
	struct task_struct *socket_process;
	struct task_struct *callback_process;
#ifdef CONFIG_DIAG_SDIO_PIPE
	unsigned char *buf_in_sdio;
	unsigned char *usb_buf_mdm_out;
	struct sdio_channel *sdio_ch;
	int read_len_mdm;
	int in_busy_sdio;
	struct usb_diag_ch *mdm_ch;
	struct work_struct diag_read_mdm_work;
	struct workqueue_struct *diag_sdio_wq;
	struct work_struct diag_read_sdio_work;
	struct work_struct diag_close_sdio_work;
	struct diag_request *usb_read_mdm_ptr;
	struct diag_request *write_ptr_mdm;
#endif
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	spinlock_t hsic_ready_spinlock;
	/* common for all bridges */
	struct work_struct diag_disconnect_work;
	/* SGLTE variables */
	int lcid;
	unsigned char *buf_in_smux;
	int in_busy_smux;
	int diag_smux_enabled;
	int smux_connected;
	struct diag_request *write_ptr_mdm;
#endif
};

extern struct diag_bridge_dev *diag_bridge;
extern struct diag_hsic_dev *diag_hsic;
extern struct diagchar_dev *driver;

extern int wrap_enabled;
extern uint16_t wrap_count;

#endif
