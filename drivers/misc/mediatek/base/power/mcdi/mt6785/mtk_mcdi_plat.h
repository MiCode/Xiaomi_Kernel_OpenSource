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
#define NF_CLUSTER              1
#define CPU_PWR_STAT_MASK       0x000000FF
#define CLUSTER_PWR_STAT_MASK   (1U << 14)

/* mcdi governor policy parameters */
#define DEFAULT_CHK_RES_EACH_CORE (true)
#define DEFAULT_TMR_EN            (true)
#define DEFAULT_PRE_LOCK          (false)
#define DEFAULT_MTK_IDLE_SEL_ONLY (1)

#define cpu_is_invalid(id)      (!(id >= 0 && id < NF_CPU))
#define cluster_is_invalid(id)  (!!(id))

/* Enable CPC mode */
#define MCDI_CPC_MODE

/* MCDI mbox interface */
#define MCDI_SSPM_INTF
/* #define MCDI_MCUPM_INTF */

/* CPC last core request */
#define NON_SECURE_REQ

enum {
	ALL_CPU_IN_CLUSTER = 0,
	CPU_CLUSTER,
	CPU_IN_OTHER_CLUSTER,
	NF_PWR_STAT_MAP_TYPE
};

enum {
	CPU_TYPE_L,
	CPU_TYPE_B,

	NF_CPU_TYPE
};

#define get_cpu_type_str(i) \
	(i == 0 ? "cpu L" : (i == 1 ? "cpu B" : "unknown"))
#define cpc_cpusys_off_hw_prot() 1

unsigned int get_pwr_stat_check_map(int type, int idx);
int cluster_idx_get(int cpu);
int cpu_type_idx_get(int cpu);

void mcdi_status_init(void);
void mcdi_of_init(void **base);
#endif /* __MTK_MCDI_PLAT_H__ */
