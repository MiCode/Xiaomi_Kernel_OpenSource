/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _NANOHUB_MTK_IPI_H
#define _NANOHUB_MTK_IPI_H

#include <include/scp.h>
#include <linux/notifier.h>

int __init nanohub_ipi_init(void);
extern struct nanohub_data *g_nanohub_data_p;
void scp_wdt_reset(enum scp_core_id cpu_id);

#endif

