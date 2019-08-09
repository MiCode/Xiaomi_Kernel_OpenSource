/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef EARA_JOB_USEDEXT_H
#define EARA_JOB_USEDEXT_H

enum device_component {
	DEVICE_CPU,
	DEVICE_GPU,
	DEVICE_VPU,
	DEVICE_MDLA
};
extern struct ppm_cobra_data *ppm_cobra_pass_tbl(void);
extern unsigned int mt_cpufreq_get_freq_by_idx(int id, int idx);
#endif

