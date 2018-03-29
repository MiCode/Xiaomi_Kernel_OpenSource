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

#include "mt_ppm_internal.h"

#ifdef PPM_PMCU_SUPPORT
#include "sspm_ipi.h"


static int ppm_ipi_to_sspm_command(unsigned char cmd, struct ppm_ipi_data *data)
{
	int ack_data = 0, ret = 0, i;

	ppm_dbg(IPI, "@%s: cmd=0x%x\n", __func__, cmd);

	switch (cmd) {
	case PPM_IPI_INIT:
		data->cmd = cmd;

		ppm_dbg(IPI, "efuse_val = %d, ratio = %d, dvfs_tbl_type = %d\n",
			data->u.init.efuse_val, data->u.init.ratio, data->u.init.dvfs_tbl_type);

		/* ret = sspm_ipi_send_sync(IPI_ID_PPM, OPT, data, PPM_D_LEN, &ack_data); */
		if (ret != 0)
			ppm_err("@%s: sspm_ipi_send_sync failed, ret=%d\n", __func__, ret);
		else if (ack_data < 0) {
			ret = ack_data;
			ppm_err("@%s cmd(0x%x) return %d\n", __func__, cmd, ret);
		}
		break;

	case PPM_IPI_UPDATE_ACT_CORE:
		data->cmd = cmd;

		for (i = 0; i < data->cluster_num; i++)
			ppm_dbg(IPI, "cluster %d active core = %d\n", i, data->u.act_core[i]);

		/* ret = sspm_ipi_send_sync(IPI_ID_PPM, OPT, data, PPM_D_LEN, &ack_data); */
		if (ret != 0)
			ppm_err("@%s: sspm_ipi_send_sync failed, ret=%d\n", __func__, ret);
		else if (ack_data < 0) {
			ret = ack_data;
			ppm_err("@%s cmd(0x%x) return %d\n", __func__, cmd, ret);
		}
		break;

	case PPM_IPI_UPDATE_LIMIT:
		data->cmd = cmd;

		ppm_dbg(IPI, "cluster num=%d, min_pwr_bgt=%d\n",
			data->cluster_num, data->u.update_limit.min_pwr_bgt);
		for (i = 0; i < data->cluster_num; i++)
			ppm_dbg(IPI, "cluster %d limit: (%d)(%d)(%d)(%d) (%d)(%d)\n",
				i, data->u.update_limit.cluster_limit[i].min_cpufreq_idx,
				data->u.update_limit.cluster_limit[i].max_cpufreq_idx,
				data->u.update_limit.cluster_limit[i].min_cpu_core,
				data->u.update_limit.cluster_limit[i].max_cpu_core,
				data->u.update_limit.cluster_limit[i].has_advise_freq,
				data->u.update_limit.cluster_limit[i].advise_cpufreq_idx);

		/* ret = sspm_ipi_send_sync(IPI_ID_PPM, OPT, data, PPM_D_LEN, &ack_data); */
		if (ret != 0)
			ppm_err("@%s: sspm_ipi_send_sync failed, ret=%d\n", __func__, ret);
		else if (ack_data < 0) {
			ret = ack_data;
			ppm_err("@%s cmd(0x%x) return %d\n", __func__, cmd, ret);
		}
		break;

	default:
		ppm_err("@%s cmd(0x%x) wrong!!!\n", __func__, cmd);
		break;
	}

	return ret;
}

void ppm_ipi_init(unsigned int efuse_val, unsigned int ratio)
{
	struct ppm_ipi_data data;

	data.u.init.efuse_val = efuse_val;
	data.u.init.ratio = ratio;
	data.u.init.dvfs_tbl_type = (unsigned int)ppm_main_info.dvfs_tbl_type;

	ppm_ipi_to_sspm_command(PPM_IPI_INIT, &data);
}

void ppm_ipi_update_act_core(struct ppm_cluster_status *cluster_status,
				unsigned int cluster_num)
{
	struct ppm_ipi_data data;
	int i;

	data.cluster_num = (unsigned char)cluster_num;
	for (i = 0; i < cluster_num; i++)
		data.u.act_core[i] = (unsigned char)cluster_status[i].core_num;

	ppm_ipi_to_sspm_command(PPM_IPI_UPDATE_ACT_CORE, &data);
}

void ppm_ipi_update_limit(struct ppm_client_req req)
{
	struct ppm_ipi_data data;
	int i;

	data.cluster_num = (unsigned char)req.cluster_num;
	data.u.update_limit.min_pwr_bgt = (unsigned short)ppm_main_info.min_power_budget;
	for (i = 0; i < data.cluster_num; i++) {
		data.u.update_limit.cluster_limit[i].min_cpufreq_idx = (char)req.cpu_limit[i].min_cpufreq_idx;
		data.u.update_limit.cluster_limit[i].max_cpufreq_idx = (char)req.cpu_limit[i].max_cpufreq_idx;
		data.u.update_limit.cluster_limit[i].min_cpu_core = (unsigned char)req.cpu_limit[i].min_cpu_core;
		data.u.update_limit.cluster_limit[i].max_cpu_core = (unsigned char)req.cpu_limit[i].max_cpu_core;
		data.u.update_limit.cluster_limit[i].has_advise_freq = req.cpu_limit[i].has_advise_freq;
		data.u.update_limit.cluster_limit[i].advise_cpufreq_idx = (req.cpu_limit[i].advise_cpufreq_idx < 0)
			? 0xFF : (char)req.cpu_limit[i].advise_cpufreq_idx;
	}

	ppm_ipi_to_sspm_command(PPM_IPI_UPDATE_LIMIT, &data);
}
#endif

