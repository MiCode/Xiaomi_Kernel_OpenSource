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

#ifndef __MTK_MCDI_GOVERNOR_LIB_H__
#define __MTK_MCDI_GOVERNOR_LIB_H__

int acquire_last_core_prot(int cpu);
int acquire_cluster_last_core_prot(int cpu);
void release_last_core_prot(void);
void release_cluster_last_core_prot(void);

void mcdi_ap_ready(void);
#endif /* __MTK_MCDI_GOVERNOR_LIB_H__ */
