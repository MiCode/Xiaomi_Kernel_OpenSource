/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 */

#ifndef DIAGFWD_SOCKET_H
#define DIAGFWD_SOCKET_H

#include <linux/socket.h>
#include <linux/soc/qcom/qmi.h>

#define DIAG_SOCKET_NAME_SZ		24

#define PORT_TYPE_SERVER		0
#define PORT_TYPE_CLIENT		1

#define PERIPHERAL_AFTER_BOOT		0
#define PERIPHERAL_SSR_DOWN		1
#define PERIPHERAL_SSR_UP		2

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
	struct sockaddr_qrtr remote_addr;
	struct socket *hdl;
	struct workqueue_struct *wq;
	struct work_struct init_work;
	struct work_struct read_work;
	struct diagfwd_info *fwd_ctxt;
	wait_queue_head_t read_wait_q;
};


extern struct diag_socket_info socket_data[NUM_PERIPHERALS];
extern struct diag_socket_info socket_cntl[NUM_PERIPHERALS];
extern struct diag_socket_info socket_dci[NUM_PERIPHERALS];
extern struct diag_socket_info socket_cmd[NUM_PERIPHERALS];
extern struct diag_socket_info socket_dci_cmd[NUM_PERIPHERALS];

extern struct qmi_handle *cntl_qmi;

void diag_socket_invalidate(void *ctxt, struct diagfwd_info *fwd_ctxt);
int diag_socket_check_state(void *ctxt);

int diag_socket_init(void);
void diag_socket_exit(void);
int diag_socket_init_peripheral(uint8_t peripheral);
#endif
