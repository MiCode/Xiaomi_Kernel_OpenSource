/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_MCDI_GOVERNOR_LIB_H__
#define __MTK_MCDI_GOVERNOR_LIB_H__

int acquire_last_core_prot(int cpu);
int acquire_cluster_last_core_prot(int cpu);
void release_last_core_prot(void);
void release_cluster_last_core_prot(void);

void mcdi_ap_ready(void);
#endif /* __MTK_MCDI_GOVERNOR_LIB_H__ */
