/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef DIAGFWD_CNTL_H
#define DIAGFWD_CNTL_H

/* Message registration commands */
#define DIAG_CTRL_MSG_REG		1
/* Message passing for DTR events */
#define DIAG_CTRL_MSG_DTR		2
/* Control Diag sleep vote, buffering etc */
#define DIAG_CTRL_MSG_DIAGMODE		3
/* Diag data based on "light" diag mask */
#define DIAG_CTRL_MSG_DIAGDATA		4
/* Deprecated */
#define DIAG_CTRL_MSG_LOG_MASK		5
#define DIAG_CTRL_MSG_EVENT_MASK	6
#define DIAG_CTRL_MSG_F3_MASK		7
/* Send diag internal feature mask 'diag_int_feature_mask' */
#define DIAG_CTRL_MSG_FEATURE		8
/* Send Diag log mask for a particular equip id */
#define DIAG_CTRL_MSG_EQUIP_LOG_MASK	9
/* Send Diag event mask */
#define DIAG_CTRL_MSG_EVENT_MASK_V2	10
/* Send Diag F3 mask */
#define DIAG_CTRL_MSG_F3_MASK_V2	11

struct cmd_code_range {
	uint16_t cmd_code_lo;
	uint16_t cmd_code_hi;
	uint32_t data;
};

struct diag_ctrl_msg {
	uint32_t version;
	uint16_t cmd_code;
	uint16_t subsysid;
	uint16_t count_entries;
	uint16_t port;
};

void diagfwd_cntl_init(void);
void diagfwd_cntl_exit(void);
void diag_read_smd_cntl_work_fn(struct work_struct *);
void diag_read_smd_qdsp_cntl_work_fn(struct work_struct *);
void diag_read_smd_wcnss_cntl_work_fn(struct work_struct *);
void diag_smd_cntl_notify(void *ctxt, unsigned event);
void diag_smd_qdsp_cntl_notify(void *ctxt, unsigned event);
void diag_smd_wcnss_cntl_notify(void *ctxt, unsigned event);

#endif
