/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __HELIO_DVFSRC_IPI_H
#define __HELIO_DVFSRC_IPI_H

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT

#include <sspm_ipi.h>
#include <sspm_ipi_pin.h>

enum {
	QOS_IPI_QOS_ENABLE = 0,
	QOS_IPI_OPP_TABLE,
	QOS_IPI_VCORE_OPP,
	QOS_IPI_DDR_OPP,
	QOS_IPI_ERROR_HANDLER,
	QOS_IPI_SWPM_INIT,
	QOS_IPI_UPOWER_DATA_TRANSFER,
	QOS_IPI_UPOWER_DUMP_TABLE,
	QOS_IPI_GET_GPU_BW,
	QOS_IPI_SWPM_ENABLE,
	QOS_IPI_SWPM_SET_UPDATE_CNT,

	NR_QOS_IPI,
};


struct qos_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int dvfsrc_en;
		} qos_init;
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
			unsigned int master_type;
			unsigned int emi_data;
			unsigned int predict_data;
		} error_handler;
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
	} u;
};

static int qos_ipi_to_sspm_command(void *buffer, int slot)
{
	int ack_data;

	return sspm_ipi_send_sync(IPI_ID_QOS, IPI_OPT_POLLING,
			buffer, slot, &ack_data, 1);
}
#endif

#endif

