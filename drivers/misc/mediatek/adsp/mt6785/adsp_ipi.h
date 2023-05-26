/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __ADSP_IPI_H
#define __ADSP_IPI_H

#include "adsp_helper.h"

#define SHARE_BUF_SIZE 288
/* adsp awake timeout count definition*/
#define ADSP_AWAKE_TIMEOUT 5000

struct adsp_ipi_desc {
	void (*handler)(int id, void *data, unsigned int len);
	const char *name;
};

struct adsp_share_obj {
	enum adsp_ipi_id id;
	unsigned int len;
	unsigned char reserve[8];
	unsigned char share_buf[SHARE_BUF_SIZE - 16];
};

struct ipi_ctrl_s {
	enum adsp_ipi_id ipi_mutex_owner;
	enum adsp_ipi_id ipi_owner;

	struct adsp_share_obj *send_obj;
	struct adsp_share_obj *recv_obj;
	struct mutex lock;      /* ap used */
};

void adsp_ipi_init(void);
void adsp_ipi_handler(int irq, void *data, int cid);
enum adsp_ipi_status adsp_ipi_send_ipc(enum adsp_ipi_id id, void *buf,
				   unsigned int len, unsigned int wait,
				   unsigned int adsp_id);

#endif
