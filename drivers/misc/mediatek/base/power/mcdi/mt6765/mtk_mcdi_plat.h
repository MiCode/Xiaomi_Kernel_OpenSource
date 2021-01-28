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

#ifndef __MTK_MCDI_PLAT_H__
#define __MTK_MCDI_PLAT_H__

#define NF_CPU                  8
#define NF_CLUSTER              2
#define CPU_PWR_STAT_MASK       0x000000FF
#define CLUSTER_PWR_STAT_MASK   0x00030000

#define cpu_is_invalid(id)      (!(id >= 0 && id < NF_CPU))
#define cluster_is_invalid(id)  (!(id >= 0 && id < NF_CLUSTER))

/* MCDI mbox interface */
#define MCDI_SSPM_INTF
/* #define MCDI_MCUPM_INTF */

enum {
	ALL_CPU_IN_CLUSTER = 0,
	CPU_CLUSTER,
	CPU_IN_OTHER_CLUSTER,
	NF_PWR_STAT_MAP_TYPE
};

enum {
	CPU_TYPE_L,
	CPU_TYPE_LL,

	NF_CPU_TYPE
};

#define get_cpu_type_str(i) \
	(i == 0 ? "cpu L" : (i == 1 ? "cpu LL" : "unknown"))

unsigned int get_pwr_stat_check_map(int type, int idx);
int cluster_idx_get(int cpu);
int cpu_type_idx_get(int cpu);

void mcdi_status_init(void);
void mcdi_of_init(void **base);

#endif /* __MTK_MCDI_PLAT_H__ */
