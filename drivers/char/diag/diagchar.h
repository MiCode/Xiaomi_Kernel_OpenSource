/* Copyright (c) 2008-2016, The Linux Foundation. All rights reserved.
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
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <soc/qcom/smd.h>
#include <asm/atomic.h>
#include "diagfwd_bridge.h"

/* Size of the USB buffers used for read and write*/
#define USB_MAX_OUT_BUF 4096
#define APPS_BUF_SIZE	4096
#define IN_BUF_SIZE		16384
#define MAX_SYNC_OBJ_NAME_SIZE	32

#define DIAG_MAX_REQ_SIZE	(16 * 1024)
#define DIAG_MAX_RSP_SIZE	(16 * 1024)
#define APF_DIAG_PADDING	256
/*
 * In the worst case, the HDLC buffer can be atmost twice the size of the
 * original packet. Add 3 bytes for 16 bit CRC (2 bytes) and a delimiter
 * (1 byte)
 */
#define DIAG_MAX_HDLC_BUF_SIZE	((DIAG_MAX_REQ_SIZE * 2) + 3)

/* The header of callback data type has remote processor token (of type int) */
#define CALLBACK_HDR_SIZE	(sizeof(int))
#define CALLBACK_BUF_SIZE	(DIAG_MAX_REQ_SIZE + CALLBACK_HDR_SIZE)

#define MAX_SSID_PER_RANGE	200

#define ALL_PROC		-1

#define REMOTE_DATA		4

#define USER_SPACE_DATA		16384

#define DIAG_CTRL_MSG_LOG_MASK	9
#define DIAG_CTRL_MSG_EVENT_MASK	10
#define DIAG_CTRL_MSG_F3_MASK	11
#define CONTROL_CHAR	0x7E

#define DIAG_CON_APSS		(0x0001)	/* Bit mask for APSS */
#define DIAG_CON_MPSS		(0x0002)	/* Bit mask for MPSS */
#define DIAG_CON_LPASS		(0x0004)	/* Bit mask for LPASS */
#define DIAG_CON_WCNSS		(0x0008)	/* Bit mask for WCNSS */
#define DIAG_CON_SENSORS	(0x0010)	/* Bit mask for Sensors */
#define DIAG_CON_NONE		(0x0000)	/* Bit mask for No SS*/
#define DIAG_CON_ALL		(DIAG_CON_APSS | DIAG_CON_MPSS \
				| DIAG_CON_LPASS | DIAG_CON_WCNSS \
				| DIAG_CON_SENSORS)

#define DIAG_STM_MODEM	0x01
#define DIAG_STM_LPASS	0x02
#define DIAG_STM_WCNSS	0x04
#define DIAG_STM_APPS	0x08
#define DIAG_STM_SENSORS 0x10

#define INVALID_PID		-1
#define DIAG_CMD_FOUND		1
#define DIAG_CMD_NOT_FOUND	0
#define DIAG_CMD_POLLING	1
#define DIAG_CMD_NOT_POLLING	0
#define DIAG_CMD_ADD		1
#define DIAG_CMD_REMOVE		0

#define DIAG_CMD_VERSION	0
#define DIAG_CMD_ERROR		0x13
#define DIAG_CMD_DOWNLOAD	0x3A
#define DIAG_CMD_DIAG_SUBSYS	0x4B
#define DIAG_CMD_LOG_CONFIG	0x73
#define DIAG_CMD_LOG_ON_DMND	0x78
#define DIAG_CMD_EXT_BUILD	0x7c
#define DIAG_CMD_MSG_CONFIG	0x7D
#define DIAG_CMD_GET_EVENT_MASK	0x81
#define DIAG_CMD_SET_EVENT_MASK	0x82
#define DIAG_CMD_EVENT_TOGGLE	0x60
#define DIAG_CMD_NO_SUBSYS	0xFF
#define DIAG_CMD_STATUS	0x0C
#define DIAG_SS_WCDMA	0x04
#define DIAG_CMD_QUERY_CALL	0x0E
#define DIAG_SS_GSM	0x08
#define DIAG_CMD_QUERY_TMC	0x02
#define DIAG_SS_TDSCDMA	0x57
#define DIAG_CMD_TDSCDMA_STATUS	0x0E
#define DIAG_CMD_DIAG_SUBSYS_DELAY 0x80

#define DIAG_SS_DIAG		0x12
#define DIAG_SS_PARAMS		0x32
#define DIAG_SS_FILE_READ_MODEM 0x0816
#define DIAG_SS_FILE_READ_ADSP  0x0E10
#define DIAG_SS_FILE_READ_WCNSS 0x141F
#define DIAG_SS_FILE_READ_SLPI 0x01A18
#define DIAG_SS_FILE_READ_APPS 0x020F

#define DIAG_DIAG_MAX_PKT_SZ	0x55
#define DIAG_DIAG_STM		0x214
#define DIAG_DIAG_POLL		0x03
#define DIAG_DEL_RSP_WRAP	0x04
#define DIAG_DEL_RSP_WRAP_CNT	0x05
#define DIAG_EXT_MOBILE_ID	0x06
#define DIAG_GET_TIME_API	0x21B
#define DIAG_SET_TIME_API	0x21C
#define DIAG_SWITCH_COMMAND	0x081B
#define DIAG_BUFFERING_MODE	0x080C

#define DIAG_CMD_OP_LOG_DISABLE		0
#define DIAG_CMD_OP_GET_LOG_RANGE	1
#define DIAG_CMD_OP_SET_LOG_MASK	3
#define DIAG_CMD_OP_GET_LOG_MASK	4

#define DIAG_CMD_OP_GET_SSID_RANGE	1
#define DIAG_CMD_OP_GET_BUILD_MASK	2
#define DIAG_CMD_OP_GET_MSG_MASK	3
#define DIAG_CMD_OP_SET_MSG_MASK	4
#define DIAG_CMD_OP_SET_ALL_MSG_MASK	5

#define DIAG_CMD_OP_GET_MSG_ALLOC       0x33
#define DIAG_CMD_OP_GET_MSG_DROP	0x30
#define DIAG_CMD_OP_RESET_MSG_STATS	0x2F
#define DIAG_CMD_OP_GET_LOG_ALLOC	0x31
#define DIAG_CMD_OP_GET_LOG_DROP	0x2C
#define DIAG_CMD_OP_RESET_LOG_STATS	0x2B
#define DIAG_CMD_OP_GET_EVENT_ALLOC	0x32
#define DIAG_CMD_OP_GET_EVENT_DROP	0x2E
#define DIAG_CMD_OP_RESET_EVENT_STATS	0x2D

#define DIAG_CMD_OP_HDLC_DISABLE	0x218

#define BAD_PARAM_RESPONSE_MESSAGE 20

#define PERSIST_TIME_SUCCESS 0
#define PERSIST_TIME_FAILURE 1
#define PERSIST_TIME_NOT_SUPPORTED 2

#define MODE_CMD	41
#define RESET_ID	2

#define PKT_DROP	0
#define PKT_ALLOC	1
#define PKT_RESET	2

#define FEATURE_MASK_LEN	2

#define DIAG_MD_NONE			0
#define DIAG_MD_PERIPHERAL		1

/*
 * The status bit masks when received in a signal handler are to be
 * used in conjunction with the peripheral list bit mask to determine the
 * status for a peripheral. For instance, 0x00010002 would denote an open
 * status on the MPSS
 */
#define DIAG_STATUS_OPEN (0x00010000)	/* DCI channel open status mask   */
#define DIAG_STATUS_CLOSED (0x00020000)	/* DCI channel closed status mask */

#define MODE_NONREALTIME	0
#define MODE_REALTIME		1
#define MODE_UNKNOWN		2

#define DIAG_BUFFERING_MODE_STREAMING	0
#define DIAG_BUFFERING_MODE_THRESHOLD	1
#define DIAG_BUFFERING_MODE_CIRCULAR	2

#define DIAG_MIN_WM_VAL		0
#define DIAG_MAX_WM_VAL		100

#define DEFAULT_LOW_WM_VAL	15
#define DEFAULT_HIGH_WM_VAL	85

#define TYPE_DATA		0
#define TYPE_CNTL		1
#define TYPE_DCI		2
#define TYPE_CMD		3
#define TYPE_DCI_CMD		4
#define NUM_TYPES		5

#define PERIPHERAL_MODEM	0
#define PERIPHERAL_LPASS	1
#define PERIPHERAL_WCNSS	2
#define PERIPHERAL_SENSORS	3
#define NUM_PERIPHERALS		4
#define APPS_DATA		(NUM_PERIPHERALS)

/* Number of sessions possible in Memory Device Mode. +1 for Apps data */
#define NUM_MD_SESSIONS		(NUM_PERIPHERALS + 1)

#define MD_PERIPHERAL_MASK(x)	(1 << x)

/*
 * Number of stm processors includes all the peripherals and
 * apps.Added 1 below to indicate apps
 */
#define NUM_STM_PROCESSORS	(NUM_PERIPHERALS + 1)
/*
 * Indicates number of peripherals that can support DCI and Apps
 * processor. This doesn't mean that a peripheral has the
 * feature.
 */
#define NUM_DCI_PERIPHERALS	(NUM_PERIPHERALS + 1)

#define DIAG_PROC_DCI			1
#define DIAG_PROC_MEMORY_DEVICE		2

/* Flags to vote the DCI or Memory device process up or down
   when it becomes active or inactive */
#define VOTE_DOWN			0
#define VOTE_UP				1

#define DIAG_TS_SIZE	50

#define DIAG_MDM_BUF_SIZE	2048
/* The Maximum request size is 2k + DCI header + footer (6 bytes) */
#define DIAG_MDM_DCI_BUF_SIZE	(2048 + 6)

#define DIAG_LOCAL_PROC	0

#ifndef CONFIG_DIAGFWD_BRIDGE_CODE
/* Local Processor only */
#define DIAG_NUM_PROC	1
#else
/* Local Processor + Remote Devices */
#define DIAG_NUM_PROC	(1 + NUM_REMOTE_DEV)
#endif

#define DIAG_WS_DCI		0
#define DIAG_WS_MUX		1

#define DIAG_DATA_TYPE		1
#define DIAG_CNTL_TYPE		2
#define DIAG_DCI_TYPE		3

/* List of remote processor supported */
enum remote_procs {
	MDM = 1,
	MDM2 = 2,
	QSC = 5,
};

struct diag_pkt_header_t {
	uint8_t cmd_code;
	uint8_t subsys_id;
	uint16_t subsys_cmd_code;
} __packed;

struct diag_cmd_ext_mobile_rsp_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t padding[3];
	uint32_t family;
	uint32_t chip_id;
} __packed;

struct diag_cmd_time_sync_query_req_t {
	struct diag_pkt_header_t header;
	uint8_t version;
};

struct diag_cmd_time_sync_query_rsp_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t time_api;
};

struct diag_cmd_time_sync_switch_req_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t time_api;
	uint8_t persist_time;
};

struct diag_cmd_time_sync_switch_rsp_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t time_api;
	uint8_t time_api_status;
	uint8_t persist_time_status;
};

struct diag_cmd_reg_entry_t {
	uint16_t cmd_code;
	uint16_t subsys_id;
	uint16_t cmd_code_lo;
	uint16_t cmd_code_hi;
} __packed;

struct diag_cmd_reg_t {
	struct list_head link;
	struct diag_cmd_reg_entry_t entry;
	uint8_t proc;
	int pid;
};

/*
 * @sync_obj_name: name of the synchronization object associated with this proc
 * @count: number of entries in the bind
 * @entries: the actual packet registrations
 */
struct diag_cmd_reg_tbl_t {
	char sync_obj_name[MAX_SYNC_OBJ_NAME_SIZE];
	uint32_t count;
	struct diag_cmd_reg_entry_t *entries;
};

struct diag_client_map {
	char name[20];
	int pid;
};

struct real_time_vote_t {
	int client_id;
	uint16_t proc;
	uint8_t real_time_vote;
} __packed;

struct real_time_query_t {
	int real_time;
	int proc;
} __packed;

struct diag_buffering_mode_t {
	uint8_t peripheral;
	uint8_t mode;
	uint8_t high_wm_val;
	uint8_t low_wm_val;
} __packed;

struct diag_callback_reg_t {
	int proc;
} __packed;

struct diag_ws_ref_t {
	int ref_count;
	int copy_count;
	spinlock_t lock;
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

struct diag_pkt_stats_t {
	uint32_t alloc_count;
	uint32_t drop_count;
};

struct diag_cmd_stats_rsp_t {
	struct diag_pkt_header_t header;
	uint32_t payload;
};

struct diag_cmd_hdlc_disable_rsp_t {
	struct diag_pkt_header_t header;
	uint8_t framing_version;
	uint8_t result;
};

struct diag_pkt_frame_t {
	uint8_t start;
	uint8_t version;
	uint16_t length;
};

struct diag_partial_pkt_t {
	uint32_t total_len;
	uint32_t read_len;
	uint32_t remaining;
	uint32_t capacity;
	uint8_t processing;
	unsigned char *data;
} __packed;

struct diag_logging_mode_param_t {
	uint32_t req_mode;
	uint32_t peripheral_mask;
	uint8_t mode_param;
} __packed;

struct diag_md_session_t {
	int pid;
	int peripheral_mask;
	uint8_t hdlc_disabled;
	struct timer_list hdlc_reset_timer;
	struct diag_mask_info *msg_mask;
	struct diag_mask_info *log_mask;
	struct diag_mask_info *event_mask;
	struct task_struct *task;
};

/*
 * High level structure for storing Diag masks.
 *
 * @ptr: Pointer to the buffer that stores the masks
 * @mask_len: Length of the buffer pointed by ptr
 * @update_buf: Buffer for performing mask updates to peripherals
 * @update_buf_len: Length of the buffer pointed by buf
 * @status: status of the mask - all enable, disabled, valid
 * @lock: To protect access to the mask variables
 */
struct diag_mask_info {
	uint8_t *ptr;
	int mask_len;
	uint8_t *update_buf;
	int update_buf_len;
	uint8_t status;
	struct mutex lock;
};

struct diag_md_proc_info {
	int pid;
	struct task_struct *socket_process;
	struct task_struct *callback_process;
	struct task_struct *mdlog_process;
};

struct diag_feature_t {
	uint8_t feature_mask[FEATURE_MASK_LEN];
	uint8_t rcvd_feature_mask;
	uint8_t log_on_demand;
	uint8_t separate_cmd_rsp;
	uint8_t encode_hdlc;
	uint8_t peripheral_buffering;
	uint8_t mask_centralization;
	uint8_t stm_support;
	uint8_t sockets_enabled;
	uint8_t sent_feature_mask;
};

struct diagchar_dev {

	/* State for the char driver */
	unsigned int major;
	unsigned int minor_start;
	int num;
	struct cdev *cdev;
	char *name;
	struct class *diagchar_class;
	struct device *diag_dev;
	int ref_count;
	struct mutex diagchar_mutex;
	struct mutex diag_file_mutex;
	wait_queue_head_t wait_q;
	struct diag_client_map *client_map;
	int *data_ready;
	int num_clients;
	int polling_reg_flag;
	int use_device_tree;
	int supports_separate_cmdrsp;
	int supports_apps_hdlc_encoding;
	int supports_sockets;
	/* The state requested in the STM command */
	int stm_state_requested[NUM_STM_PROCESSORS];
	/* The current STM state */
	int stm_state[NUM_STM_PROCESSORS];
	uint16_t stm_peripheral;
	struct work_struct stm_update_work;
	uint16_t mask_update;
	struct work_struct mask_update_work;
	uint16_t close_transport;
	struct work_struct close_transport_work;
	struct workqueue_struct *cntl_wq;
	struct mutex cntl_lock;
	/* Whether or not the peripheral supports STM */
	/* Delayed response Variables */
	uint16_t delayed_rsp_id;
	struct mutex delayed_rsp_mutex;
	/* DCI related variables */
	struct list_head dci_req_list;
	struct list_head dci_client_list;
	int dci_tag;
	int dci_client_id;
	struct mutex dci_mutex;
	int num_dci_client;
	unsigned char *apps_dci_buf;
	int dci_state;
	struct workqueue_struct *diag_dci_wq;
	struct list_head cmd_reg_list;
	struct mutex cmd_reg_mutex;
	uint32_t cmd_reg_count;
	struct mutex diagfwd_channel_mutex;
	/* Sizes that reflect memory pool sizes */
	unsigned int poolsize;
	unsigned int poolsize_hdlc;
	unsigned int poolsize_dci;
	unsigned int poolsize_user;
	/* Buffers for masks */
	struct mutex diag_cntl_mutex;
	/* Members for Sending response */
	unsigned char *encoded_rsp_buf;
	int encoded_rsp_len;
	uint8_t rsp_buf_busy;
	spinlock_t rsp_buf_busy_lock;
	int rsp_buf_ctxt;
	struct diagfwd_info *diagfwd_data[NUM_PERIPHERALS];
	struct diagfwd_info *diagfwd_cntl[NUM_PERIPHERALS];
	struct diagfwd_info *diagfwd_dci[NUM_PERIPHERALS];
	struct diagfwd_info *diagfwd_cmd[NUM_PERIPHERALS];
	struct diagfwd_info *diagfwd_dci_cmd[NUM_PERIPHERALS];
	struct diag_feature_t feature[NUM_PERIPHERALS];
	struct diag_buffering_mode_t buffering_mode[NUM_PERIPHERALS];
	uint8_t buffering_flag[NUM_PERIPHERALS];
	struct mutex mode_lock;
	unsigned char *user_space_data_buf;
	uint8_t user_space_data_busy;
	struct diag_pkt_stats_t msg_stats;
	struct diag_pkt_stats_t log_stats;
	struct diag_pkt_stats_t event_stats;
	/* buffer for updating mask to peripherals */
	unsigned char *buf_feature_mask_update;
	uint8_t hdlc_disabled;
	struct mutex hdlc_disable_mutex;
	struct timer_list hdlc_reset_timer;
	struct mutex diag_hdlc_mutex;
	unsigned char *hdlc_buf;
	uint32_t hdlc_buf_len;
	unsigned char *apps_rsp_buf;
	struct diag_partial_pkt_t incoming_pkt;
	int in_busy_pktdata;
	/* Variables for non real time mode */
	int real_time_mode[DIAG_NUM_PROC];
	int real_time_update_busy;
	uint16_t proc_active_mask;
	uint16_t proc_rt_vote_mask[DIAG_NUM_PROC];
	struct mutex real_time_mutex;
	struct work_struct diag_real_time_work;
	struct workqueue_struct *diag_real_time_wq;
#ifdef CONFIG_DIAG_OVER_USB
	int usb_connected;
#endif
	struct workqueue_struct *diag_wq;
	struct work_struct diag_drain_work;
	struct work_struct update_user_clients;
	struct work_struct update_md_clients;
	struct workqueue_struct *diag_cntl_wq;
	uint8_t log_on_demand_support;
	uint8_t *apps_req_buf;
	uint32_t apps_req_buf_len;
	uint8_t *dci_pkt_buf; /* For Apps DCI packets */
	uint32_t dci_pkt_length;
	int in_busy_dcipktdata;
	int logging_mode;
	int logging_mask;
	int mask_check;
	uint32_t md_session_mask;
	uint8_t md_session_mode;
	struct diag_md_session_t *md_session_map[NUM_MD_SESSIONS];
	struct mutex md_session_lock;
	/* Power related variables */
	struct diag_ws_ref_t dci_ws;
	struct diag_ws_ref_t md_ws;
	/* Pointers to Diag Masks */
	struct diag_mask_info *msg_mask;
	struct diag_mask_info *log_mask;
	struct diag_mask_info *event_mask;
	struct diag_mask_info *build_time_mask;
	uint8_t msg_mask_tbl_count;
	uint16_t event_mask_size;
	uint16_t last_event_id;
	/* Variables for Mask Centralization */
	uint16_t num_event_id[NUM_PERIPHERALS];
	uint32_t num_equip_id[NUM_PERIPHERALS];
	uint32_t max_ssid_count[NUM_PERIPHERALS];
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	/* For sending command requests in callback mode */
	unsigned char *hdlc_encode_buf;
	int hdlc_encode_buf_len;
#endif
	int time_sync_enabled;
	uint8_t uses_time_api;
};

extern struct diagchar_dev *driver;

extern int wrap_enabled;
extern uint16_t wrap_count;

void diag_get_timestamp(char *time_str);
void check_drain_timer(void);
int diag_get_remote(int remote_info);

void diag_ws_init(void);
void diag_ws_on_notify(void);
void diag_ws_on_read(int type, int pkt_len);
void diag_ws_on_copy(int type);
void diag_ws_on_copy_fail(int type);
void diag_ws_on_copy_complete(int type);
void diag_ws_reset(int type);
void diag_ws_release(void);
void chk_logging_wakeup(void);
int diag_cmd_add_reg(struct diag_cmd_reg_entry_t *new_entry, uint8_t proc,
		     int pid);
struct diag_cmd_reg_entry_t *diag_cmd_search(
			struct diag_cmd_reg_entry_t *entry,
			int proc);
void diag_cmd_remove_reg(struct diag_cmd_reg_entry_t *entry, uint8_t proc);
void diag_cmd_remove_reg_by_pid(int pid);
void diag_cmd_remove_reg_by_proc(int proc);
int diag_cmd_chk_polling(struct diag_cmd_reg_entry_t *entry);

void diag_record_stats(int type, int flag);

struct diag_md_session_t *diag_md_session_get_pid(int pid);
struct diag_md_session_t *diag_md_session_get_peripheral(uint8_t peripheral);

#endif
