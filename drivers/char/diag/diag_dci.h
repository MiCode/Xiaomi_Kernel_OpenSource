/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#define LOG_CMD_CODE		0x10
#define EVENT_CMD_CODE		0x60
#define DCI_PKT_RSP_TYPE	0
#define DCI_LOG_TYPE		-1
#define DCI_EVENT_TYPE		-2
#define SET_LOG_MASK		1
#define DISABLE_LOG_MASK	0
#define MAX_EVENT_SIZE		100

/* 16 log code categories, each has:
 * 1 bytes equip id + 1 dirty byte + 512 byte max log mask
 */
#define DCI_LOG_MASK_SIZE		(16*514)
#define DCI_EVENT_MASK_SIZE		512
#define DCI_MASK_STREAM			2
#define DCI_MAX_LOG_CODES		16
#define DCI_MAX_ITEMS_PER_LOG_CODE	512

extern unsigned int dci_max_reg;
extern unsigned int dci_max_clients;

struct dci_pkt_req_tracking_tbl {
	int pid;
	int uid;
	int tag;
};

struct diag_dci_client_tbl {
	struct task_struct *client;
	uint16_t list; /* bit mask */
	int signal_type;
	unsigned char dci_log_mask[DCI_LOG_MASK_SIZE];
	unsigned char dci_event_mask[DCI_EVENT_MASK_SIZE];
	unsigned char *dci_data;
	int data_len;
	int total_capacity;
	int dropped_logs;
	int dropped_events;
	int received_logs;
	int received_events;
};

/* This is used for DCI health stats */
struct diag_dci_health_stats {
	int dropped_logs;
	int dropped_events;
	int received_logs;
	int received_events;
	int reset_status;
};

enum {
	DIAG_DCI_NO_ERROR = 1001,	/* No error */
	DIAG_DCI_NO_REG,		/* Could not register */
	DIAG_DCI_NO_MEM,		/* Failed memory allocation */
	DIAG_DCI_NOT_SUPPORTED,	/* This particular client is not supported */
	DIAG_DCI_HUGE_PACKET,	/* Request/Response Packet too huge */
	DIAG_DCI_SEND_DATA_FAIL,/* writing to kernel or peripheral fails */
	DIAG_DCI_TABLE_ERR	/* Error dealing with registration tables */
};

int diag_dci_init(void);
void diag_dci_exit(void);
void diag_read_smd_dci_work_fn(struct work_struct *);
void diag_update_smd_dci_work_fn(struct work_struct *);
int diag_process_dci_transaction(unsigned char *buf, int len);
int diag_send_dci_pkt(struct diag_master_table entry, unsigned char *buf,
							 int len, int index);
void extract_dci_pkt_rsp(unsigned char *buf);
/* DCI Log streaming functions */
void create_dci_log_mask_tbl(unsigned char *tbl_buf);
void update_dci_cumulative_log_mask(int offset, int byte_index,
						uint8_t byte_mask);
int diag_send_dci_log_mask(smd_channel_t *ch);
void extract_dci_log(unsigned char *buf);
/* DCI event streaming functions */
void update_dci_cumulative_event_mask(int offset, uint8_t byte_mask);
int diag_send_dci_event_mask(smd_channel_t *ch);
void extract_dci_events(unsigned char *buf);
void create_dci_event_mask_tbl(unsigned char *tbl_buf);
#endif
