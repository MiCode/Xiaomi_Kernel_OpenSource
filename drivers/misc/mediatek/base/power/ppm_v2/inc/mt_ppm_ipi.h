/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MT_PPM_IPI_
#define _MT_PPM_IPI_

#include "mach/mt_ppm_api.h"
#include "mt_ppm_platform.h"


#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#define PPM_PMCU_SUPPORT	(1)
#endif

#define OPT		(1) /* reserve for extensibility */
#define PPM_D_LEN	(7) /* # of cmd + arg0 + arg1 + ... */

/* IPI Msg type */
enum {
	PPM_IPI_INIT,
	PPM_IPI_UPDATE_ACT_CORE,
	PPM_IPI_UPDATE_LIMIT,

	NR_PPM_IPI,
};

/* IPI Msg data structure */
struct ppm_ipi_data {
	unsigned char cmd;
	unsigned char cluster_num;
	union {
		struct {
			unsigned int efuse_val;
			unsigned int ratio;
			unsigned int dvfs_tbl_type;
		} init;
		unsigned char act_core[NR_PPM_CLUSTERS];
		struct {
			unsigned short min_pwr_bgt;
			struct {
				char min_cpufreq_idx;
				char max_cpufreq_idx;
				unsigned char min_cpu_core;
				unsigned char max_cpu_core;

				bool has_advise_freq;
				char advise_cpufreq_idx;
			} cluster_limit[NR_PPM_CLUSTERS];
		} update_limit;
	} u;
};

#ifdef PPM_PMCU_SUPPORT
extern void ppm_ipi_init(unsigned int efuse_val, unsigned int ratio);
extern void ppm_ipi_update_act_core(struct ppm_cluster_status *cluster_status,
					unsigned int cluster_num);
extern void ppm_ipi_update_limit(struct ppm_client_req req);
#endif

#endif

