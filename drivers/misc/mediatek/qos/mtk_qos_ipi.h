/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_QOS_IPI_H__
#define __MTK_QOS_IPI_H__

struct qos_ipi_cmd {
	int id;
	bool valid;
};

enum {
	QOS_IPI_QOS_ENABLE,
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
};

struct qos_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int dvfsrc_en;
			unsigned int dram_type;
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
			unsigned int ena;
			unsigned int enc[4];
		} smi_met_mon;
		struct {
			unsigned int addr;
			unsigned int addr_hi;
			unsigned int size;
		} gpu_info;
	} u;
};

extern int qos_ipi_to_sspm_command(void *buffer, int slot);

#endif

