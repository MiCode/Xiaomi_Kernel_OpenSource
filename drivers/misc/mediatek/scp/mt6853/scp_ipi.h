/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef _SCP_IPI_H_
#define _SCP_IPI_H_

#include "scp_ipi_wrapper.h"

enum scp_ipi_status {
	SCP_IPI_NOT_READY = -2,
	SCP_IPI_ERROR = -1,
	SCP_IPI_DONE,
	SCP_IPI_BUSY,
};

struct scp_ipi_wrapper {
	uint32_t out_id_0;
	uint32_t out_id_1;
	uint32_t in_id_0;
	uint32_t in_id_1;
	uint32_t out_size;
	uint32_t in_size;
	void *msg_0;
	void *msg_1;
};

extern struct mtk_mbox_device scp_mboxdev;
extern struct mtk_ipi_device scp_ipidev;
extern struct mtk_mbox_info scp_mbox_info[SCP_MBOX_TOTAL];

extern enum scp_ipi_status scp_ipi_registration(enum ipi_id id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name);
extern enum scp_ipi_status scp_ipi_send(enum ipi_id id, void *buf,
	unsigned int len, unsigned int wait, enum scp_core_id scp_id);
extern enum scp_ipi_status scp_ipi_unregistration(enum ipi_id id);
extern enum scp_ipi_status scp_legacy_ipi_init(void);

#endif
