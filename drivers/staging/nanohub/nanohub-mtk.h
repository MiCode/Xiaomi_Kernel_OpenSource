/*
 * Copyright (C) 2016 MediaTek Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _NANOHUB_MTK_IPI_H
#define _NANOHUB_MTK_IPI_H
#include <scp_ipi.h>
#include <scp_helper.h>
#include <linux/notifier.h>

int __init nanohub_ipi_init(void);
extern struct nanohub_data *g_nanohub_data_p;
void scp_wdt_reset(enum scp_core_id cpu_id);

#endif

