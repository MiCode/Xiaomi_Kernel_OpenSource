/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _VCP_IPI_H_
#define _VCP_IPI_H_
#include "vcp.h"
#include "vcp_ipi_table.h"

struct vcp_ipi_wrapper {
	uint32_t out_id_0;
	uint32_t out_id_1;
	uint32_t in_id_0;
	uint32_t in_id_1;
	uint32_t out_size;
	uint32_t in_size;
	void *msg_0;
	void *msg_1;
};

extern struct mtk_mbox_device vcp_mboxdev;
extern struct mtk_ipi_device vcp_ipidev;
extern struct mtk_mbox_info *vcp_mbox_info;

#endif
