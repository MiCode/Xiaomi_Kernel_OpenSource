/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MTK_QOS_IPI_H__
#define __MTK_QOS_IPI_H__

enum {
	QOS_IPI_QOS_ENABLE,
	QOS_IPI_DVFSRC_ENABLE,
	QOS_IPI_OPP_TABLE,
	QOS_IPI_VCORE_OPP,
	QOS_IPI_DDR_OPP,
	QOS_IPI_SWPM_INIT,
	QOS_IPI_UPOWER_DATA_TRANSFER,
	QOS_IPI_UPOWER_DUMP_TABLE,
	QOS_IPI_GET_GPU_BW,
	QOS_IPI_SWPM_ENABLE,
	QOS_IPI_QOS_BOUND,
	QOS_IPI_QOS_BOUND_ENABLE,
	QOS_IPI_QOS_BOUND_STRESS_ENABLE,
	QOS_IPI_SMI_MET_MON,
	QOS_IPI_SETUP_GPU_INFO,
	QOS_IPI_SWPM_SET_UPDATE_CNT,
	QOS_IPI_QOS_SHARE_INIT,

	NR_QOS_IPI,
};

struct qos_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int dvfsrc_en;
		} dvfsrc_enable;
		struct {
			unsigned int vcore_dvfs_opp;
			unsigned int vcore_uv;
			unsigned int ddr_khz;
		} opp_table;
		struct {
			unsigned int vcore_dvfs_opp;
			unsigned int vcore_opp;
		} vcore_opp;
		struct {
			unsigned int vcore_dvfs_opp;
			unsigned int ddr_opp;
		} ddr_opp;
		struct {
			unsigned int dram_addr;
			unsigned int dram_size;
			unsigned int dram_ch_num;
		} swpm_init;
		struct {
			unsigned int type;
			unsigned int enable;
		} swpm_enable;
		struct {
			unsigned int type;
			unsigned int cnt;
		} swpm_set_update_cnt;
		struct {
			unsigned int arg[3];
		} upower_data;
		struct {
			unsigned int state;
		} qos_bound;
		struct {
			unsigned int enable;
		} qos_bound_enable;
		struct {
			unsigned int enable;
		} qos_bound_stress_enable;
		struct {
			uint16_t ena;
			uint16_t enc[4];
		} smi_met_mon;
		struct {
			unsigned int addr;
			unsigned int addr_hi;
			unsigned int size;
		} gpu_info;
		struct {
			unsigned int dram_addr;
			unsigned int dram_size;
		} qos_share_init;
		struct {
			unsigned int arg[5];
		} max;
	} u;
};

#ifdef CONFIG_MTK_QOS_FRAMEWORK
extern void qos_ipi_init(void);
extern void qos_ipi_recv_init(void);
extern int qos_ipi_to_sspm_command(void *buffer, int slot);
#else
__weak void qos_ipi_init(void) { }
__weak void qos_ipi_recv_init(void) { }
__weak int qos_ipi_to_sspm_comman(void *buffer, int slot) { return 0; }
#endif
#endif

