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
#define MAX_DCI_CLIENT 10
#define DCI_CMD_CODE 0x93

extern unsigned int dci_max_reg;
extern unsigned int dci_max_clients;
struct diag_dci_tbl {
	int pid;
	int uid;
	int tag;
};

struct dci_notification_tbl {
	struct task_struct *client;
	uint16_t list; /* bit mask */
	int signal_type;
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
int diag_process_dci_client(unsigned char *buf, int len);
int diag_send_dci_pkt(struct diag_master_table entry, unsigned char *buf,
							 int len, int index);
#endif
