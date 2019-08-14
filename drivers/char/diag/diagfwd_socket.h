/* Copyright (c) 2015-2017, 2019, The Linux Foundation. All rights reserved.
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

#ifndef DIAGFWD_SOCKET_H
#define DIAGFWD_SOCKET_H

#include <linux/socket.h>
#include <linux/msm_ipc.h>

#define DIAG_SOCKET_NAME_SZ		24

#define DIAG_SOCK_MODEM_SVC_ID		64
#define DIAG_SOCK_MODEM_INS_ID		3

#define PORT_TYPE_SERVER		0
#define PORT_TYPE_CLIENT		1

#define PERIPHERAL_AFTER_BOOT		0
#define PERIPHERAL_SSR_DOWN		1
#define PERIPHERAL_SSR_UP		2

#define CNTL_CMD_NEW_SERVER		4
#define CNTL_CMD_REMOVE_SERVER		5
#define CNTL_CMD_REMOVE_CLIENT		6

enum {
	SOCKET_MODEM,
	SOCKET_ADSP,
	SOCKET_WCNSS,
	SOCKET_SLPI,
	SOCKET_CDSP,
	SOCKET_APPS,
	NUM_SOCKET_SUBSYSTEMS,
};

struct diag_socket_info {
	uint8_t peripheral;
	uint8_t type;
	uint8_t port_type;
	uint8_t inited;
	atomic_t opened;
	atomic_t diag_state;
	uint32_t pkt_len;
	uint32_t pkt_read;
	uint32_t svc_id;
	uint32_t ins_id;
	uint32_t data_ready;
	atomic_t flow_cnt;
	char name[DIAG_SOCKET_NAME_SZ];
	spinlock_t lock;
	wait_queue_head_t wait_q;
	struct sockaddr_msm_ipc remote_addr;
	struct socket *hdl;
	struct workqueue_struct *wq;
	struct work_struct init_work;
	struct work_struct read_work;
	struct diagfwd_info *fwd_ctxt;
	wait_queue_head_t read_wait_q;
	struct mutex socket_info_mutex;
};

union cntl_port_msg {
	struct {
		uint32_t cmd;
		uint32_t service;
		uint32_t instance;
		uint32_t node_id;
		uint32_t port_id;
	} srv;
	struct {
		uint32_t cmd;
		uint32_t node_id;
		uint32_t port_id;
	} cli;
};

struct diag_cntl_socket_info {
	uint32_t svc_id;
	uint32_t ins_id;
	atomic_t data_ready;
	struct workqueue_struct *wq;
	struct work_struct read_work;
	struct work_struct init_work;
	wait_queue_head_t read_wait_q;
	struct socket *hdl;
};

extern struct diag_socket_info socket_data[NUM_PERIPHERALS];
extern struct diag_socket_info socket_cntl[NUM_PERIPHERALS];
extern struct diag_socket_info socket_dci[NUM_PERIPHERALS];
extern struct diag_socket_info socket_cmd[NUM_PERIPHERALS];
extern struct diag_socket_info socket_dci_cmd[NUM_PERIPHERALS];

extern struct diag_cntl_socket_info *cntl_socket;

int diag_socket_init(void);
int diag_socket_init_peripheral(uint8_t peripheral);
void diag_socket_exit(void);
void diag_socket_early_exit(void);
void diag_socket_invalidate(void *ctxt, struct diagfwd_info *fwd_ctxt);
int diag_socket_check_state(void *ctxt);
#endif
