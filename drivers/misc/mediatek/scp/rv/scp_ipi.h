/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _SCP_IPI_H_
#define _SCP_IPI_H_

#include "scp_ipi_wrapper.h"
#include "scp.h"

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
extern struct mtk_mbox_info *scp_mbox_info;
extern struct scp_ipi_wrapper scp_ipi_legacy_id[1];

extern enum scp_ipi_status scp_legacy_ipi_init(void);

#endif
