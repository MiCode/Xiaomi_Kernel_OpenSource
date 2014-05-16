/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#ifndef DIAG_DCI_H
#define DIAG_DCI_H

#define MAX_DCI_CLIENTS		10
#define DCI_PKT_RSP_CODE	0x93
#define DCI_DELAYED_RSP_CODE	0x94
#define DCI_CONTROL_PKT_CODE	0x9A
#define LOG_CMD_CODE		0x10
#define EVENT_CMD_CODE		0x60
#define DCI_PKT_RSP_TYPE	0
#define DCI_LOG_TYPE		-1
#define DCI_EVENT_TYPE		-2
#define SET_LOG_MASK		1
#define DISABLE_LOG_MASK	0
#define MAX_EVENT_SIZE		512
#define DCI_CLIENT_INDEX_INVALID -1
#define DCI_PKT_REQ_MIN_LEN		9
#define DCI_LOG_CON_MIN_LEN		14
#define DCI_EVENT_CON_MIN_LEN		16

#define DIAG_DATA_TYPE		1
#define DIAG_CNTL_TYPE		2

#define DCI_BUF_PRIMARY		1
#define DCI_BUF_SECONDARY	2
#define DCI_BUF_CMD		3

#ifdef CONFIG_DEBUG_FS
#define DIAG_DCI_DEBUG_CNT	100
#define DIAG_DCI_DEBUG_LEN	100
#endif

/* 16 log code categories, each has:
 * 1 bytes equip id + 1 dirty byte + 512 byte max log mask
 */
#define DCI_LOG_MASK_SIZE		(16*514)
#define DCI_EVENT_MASK_SIZE		512
#define DCI_MASK_STREAM			2
#define DCI_MAX_LOG_CODES		16
#define DCI_MAX_ITEMS_PER_LOG_CODE	512

#define DCI_LOG_MASK_CLEAN		0
#define DCI_LOG_MASK_DIRTY		1

#define MIN_DELAYED_RSP_LEN		12
/*
 * Maximum data size that peripherals send = 8.5K log +
 * DCI header + footer (6 bytes)
 */
#define MAX_DCI_PACKET_SZ		8710

#define DCI_LOCAL_PROC	0
#define DCI_MDM_PROC	1

#define DCI_BRIDGE_MDM_IDX	1
#define DCI_HSIC_CH_IDX		0

#define DCI_REMOTE_DATA	0

extern unsigned int dci_max_reg;
extern unsigned int dci_max_clients;

struct dci_pkt_req_entry_t {
	int client_id;
	int uid;
	int tag;
	struct list_head track;
} __packed;

struct diag_dci_reg_tbl_t {
	int client_id;
	uint16_t notification_list;
	int signal_type;
	int token;
} __packed;

struct diag_dci_health_t {
	int dropped_logs;
	int dropped_events;
	int received_logs;
	int received_events;
};

struct diag_dci_partial_pkt_t {
	unsigned char *data;
	uint32_t total_len;
	uint32_t read_len;
	uint32_t remaining;
	uint8_t processing;
} __packed;

struct diag_dci_buffer_t {
	unsigned char *data;
	unsigned int data_len;
	struct mutex data_mutex;
	uint8_t in_busy;
	uint8_t buf_type;
	int data_source;
	int capacity;
	uint8_t in_list;
	struct list_head buf_track;
};

struct diag_dci_buf_peripheral_t {
	struct diag_dci_buffer_t *buf_curr;
	struct diag_dci_buffer_t *buf_primary;
	struct diag_dci_buffer_t *buf_cmd;
	struct diag_dci_health_t health;
	struct mutex health_mutex;
	struct mutex buf_mutex;
};

struct diag_dci_client_tbl {
	struct diag_dci_reg_tbl_t client_info;
	struct task_struct *client;
	unsigned char *dci_log_mask;
	unsigned char *dci_event_mask;
	uint8_t real_time;
	struct list_head track;
	struct diag_dci_buf_peripheral_t *buffers;
	uint8_t num_buffers;
	uint8_t in_service;
	struct list_head list_write_buf;
	struct mutex write_buf_mutex;
};

struct diag_dci_health_stats {
	struct diag_dci_health_t stats;
	int reset_status;
};

struct diag_dci_health_stats_proc {
	int client_id;
	struct diag_dci_health_stats health;
	int proc;
} __packed;

struct diag_dci_peripherals_t {
	int proc;
	uint16_t list;
} __packed;

/* This is used for querying DCI Log
   or Event Mask */
struct diag_log_event_stats {
	int client_id;
	uint16_t code;
	int is_set;
} __packed;

struct diag_dci_pkt_rsp_header_t {
	int type;
	int length;
	uint8_t delete_flag;
	int uid;
} __packed;

struct diag_dci_pkt_header_t {
	uint8_t start;
	uint8_t version;
	uint16_t len;
	uint8_t pkt_code;
	int tag;
} __packed;

struct diag_dci_header_t {
	uint8_t start;
	uint8_t version;
	uint16_t length;
	uint8_t cmd_code;
} __packed;

struct dci_ops_tbl_t {
	unsigned char log_mask_composite[DCI_LOG_MASK_SIZE];
	unsigned char event_mask_composite[DCI_EVENT_MASK_SIZE];
	int (*send_log_mask)(int token);
	int (*send_event_mask)(int token);
	uint16_t peripheral_status;
} __packed;

extern struct dci_ops_tbl_t *dci_ops_tbl;

enum {
	DIAG_DCI_NO_ERROR = 1001,	/* No error */
	DIAG_DCI_NO_REG,		/* Could not register */
	DIAG_DCI_NO_MEM,		/* Failed memory allocation */
	DIAG_DCI_NOT_SUPPORTED,	/* This particular client is not supported */
	DIAG_DCI_HUGE_PACKET,	/* Request/Response Packet too huge */
	DIAG_DCI_SEND_DATA_FAIL,/* writing to kernel or peripheral fails */
	DIAG_DCI_TABLE_ERR	/* Error dealing with registration tables */
};

#ifdef CONFIG_DEBUG_FS
/* To collect debug information during each smd read */
struct diag_dci_data_info {
	unsigned long iteration;
	int data_size;
	char time_stamp[DIAG_TS_SIZE];
	uint8_t peripheral;
	uint8_t ch_type;
	uint8_t proc;
};

extern struct diag_dci_data_info *dci_traffic;
extern struct mutex dci_stat_mutex;
#endif

int diag_dci_init(void);
void diag_dci_exit(void);
int diag_dci_register_client(struct diag_dci_reg_tbl_t *reg_entry);
int diag_dci_deinit_client(struct diag_dci_client_tbl *entry);
void diag_update_smd_dci_work_fn(struct work_struct *);
void diag_dci_notify_client(int peripheral_mask, int data, int proc);
void diag_dci_wakeup_clients(void);
void diag_process_apps_dci_read_data(int data_type, void *buf, int recd_bytes);
int diag_process_smd_dci_read_data(struct diag_smd_info *smd_info, void *buf,
								int recd_bytes);
int diag_process_dci_transaction(unsigned char *buf, int len);
void extract_dci_pkt_rsp(unsigned char *buf, int len, int data_source,
			 int token);
void extract_dci_ctrl_pkt(unsigned char *buf, int len, int token);
struct diag_dci_client_tbl *diag_dci_get_client_entry(int client_id);
struct diag_dci_client_tbl *dci_lookup_client_entry_pid(int pid);
int diag_process_hsic_dci_read_data(int index, void *buf, int recd_bytes);
int diag_dci_get_support_list(struct diag_dci_peripherals_t *support_list);
/* DCI Log streaming functions */
void update_dci_cumulative_log_mask(int offset, unsigned int byte_index,
						uint8_t byte_mask, int token);
void diag_dci_invalidate_cumulative_log_mask(int token);
int diag_send_dci_log_mask(int token);
void extract_dci_log(unsigned char *buf, int len, int data_source, int token);
int diag_dci_clear_log_mask(int client_id);
int diag_dci_query_log_mask(struct diag_dci_client_tbl *entry,
			    uint16_t log_code);
/* DCI event streaming functions */
void update_dci_cumulative_event_mask(int offset, uint8_t byte_mask, int token);
void diag_dci_invalidate_cumulative_event_mask(int token);
int diag_send_dci_event_mask(int token);
void extract_dci_events(unsigned char *buf, int len, int data_source,
			int token);
int diag_dci_clear_event_mask(int client_id);
int diag_dci_query_event_mask(struct diag_dci_client_tbl *entry,
			      uint16_t event_id);
void diag_dci_record_traffic(int read_bytes, uint8_t ch_type,
			     uint8_t peripheral, uint8_t proc);
uint8_t diag_dci_get_cumulative_real_time(int token);
int diag_dci_set_real_time(struct diag_dci_client_tbl *entry,
			   uint8_t real_time);
int diag_dci_copy_health_stats(struct diag_dci_health_stats_proc *stats_proc);
int diag_dci_write_proc(int peripheral, int pkt_type, char *buf, int len);

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
int diag_send_dci_log_mask_remote(int token);
int diag_send_dci_event_mask_remote(int token);
unsigned char *dci_get_buffer_from_bridge(int index);
int diag_dci_write_bridge(int index, unsigned char *buf, int len);
#endif

#endif
