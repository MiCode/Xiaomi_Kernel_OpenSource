/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MTK_NANOHUB_IPI_H_
#define _MTK_NANOHUB_IPI_H_

#include <linux/list.h>

struct ipi_transfer {
	int id;
	const unsigned char *tx_buf;
	unsigned char *rx_buf;
	unsigned int tx_len;
	unsigned int rx_len;
	struct list_head transfer_list;
};

struct ipi_message {
	struct list_head transfers;
	struct list_head list;
	void *context;
	int status;
	void (*complete)(void *context);
};

static inline void ipi_message_init(struct ipi_message *m)
{
	memset(m, 0, sizeof(*m));
	INIT_LIST_HEAD(&m->transfers);
}

static inline void ipi_message_add_tail(struct ipi_transfer *t,
		struct ipi_message *m)
{
	list_add_tail(&t->transfer_list, &m->transfers);
}

int get_ctrl_id(void);
int get_notify_id(void);
int ipi_comm_sync(int id, unsigned char *tx, unsigned int n_tx,
		unsigned char *rx, unsigned int n_rx);
int ipi_comm_async(struct ipi_message *m);
int ipi_comm_noack(int id, unsigned char *tx, unsigned int n_tx);
void ipi_comm_notify_handler_register(
		void (*f)(int id, void *data, unsigned int len));
void ipi_comm_notify_handler_unregister(void);
int ipi_comm_init(void);
void ipi_comm_exit(void);

#endif
