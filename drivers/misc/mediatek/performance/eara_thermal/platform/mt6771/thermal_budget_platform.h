/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EARA_THRM_PB_PLAT_H
#define EARA_THRM_PB_PLAT_H

#define GPU_OPP_RANGE	5
#define CPU_OPP_NUM	16
#define KEEP_L_CORE	1

#define CLUSTER_B	1
#define CLUSTER_L	0
#define CPU_CORE_NUM	8

extern struct mt_gpufreq_power_table_info *pass_gpu_table_to_eara(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);

#endif
