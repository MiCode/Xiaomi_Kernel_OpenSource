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

#include "mtk_ppm_internal.h"

#ifdef PPM_SSPM_SUPPORT
#include <linux/ktime.h>
#include "sspm_ipi.h"


static int ppm_ipi_to_sspm_command(unsigned char cmd,
	struct ppm_ipi_data *data)
{
	int ack_data = 0, ret = 0, i, opt;
	ktime_t now;
	unsigned long long delta;

	ppm_dbg(IPI, "@%s: cmd=0x%x\n", __func__, cmd);

	now = ktime_get();

	opt = IPI_OPT_POLLING;

	switch (cmd) {
	case PPM_IPI_INIT:
		data->cmd = cmd;

		ppm_dbg(IPI,
			"efuse_val=%d,cobra_tbl_addr=0x%x,dvfs_tbl_type=%d\n",
			data->u.init.efuse_val,
			data->u.init.cobra_tbl_addr,
			data->u.init.dvfs_tbl_type);

		ret = sspm_ipi_send_sync(IPI_ID_PPM,
			opt, data, PPM_D_LEN, &ack_data, 1);
		if (ret != 0)
			ppm_err("sspm_ipi_send_sync failed, ret=%d\n", ret);
		else if (ack_data < 0) {
			ret = ack_data;
			ppm_err("cmd(0x%x) return %d\n", cmd, ret);
		}
		break;

	case PPM_IPI_UPDATE_LIMIT:
		data->cmd = cmd;

		for_each_ppm_clusters(i) {
			ppm_dbg(IPI, "cluster %d limit: (%d)(%d) (%d) (%d)\n",
			i,
			data->u.update_limit.cluster_limit[i].min_cpufreq_idx,
			data->u.update_limit.cluster_limit[i].max_cpufreq_idx,
			data->u.update_limit.cluster_limit[i].max_cpu_core,
			data->u.update_limit.cluster_limit[i].advise_freq_idx
			);
		}

		ret = sspm_ipi_send_sync(IPI_ID_PPM,
			opt, data, PPM_D_LEN, &ack_data, 1);
		if (ret != 0)
			ppm_err("sspm_ipi_send_sync failed, ret=%d\n", ret);
		else if (ack_data < 0) {
			ret = ack_data;
			ppm_err("cmd(0x%x) return %d\n", cmd, ret);
		}
		break;

	case PPM_IPI_THERMAL_LIMIT_TEST:
		data->cmd = cmd;

		ppm_dbg(IPI, "thermal test budget = %d\n",
			data->u.thermal_limit_test.budget);

		ret = sspm_ipi_send_sync(IPI_ID_PPM,
			opt, data, PPM_D_LEN, &ack_data, 1);
		if (ret != 0)
			ppm_err("sspm_ipi_send_sync failed, ret=%d\n", ret);
		else if (ack_data < 0) {
			ret = ack_data;
			ppm_err("cmd(0x%x) return %d\n", cmd, ret);
		}
		break;

	case PPM_IPI_PTPOD_TEST:
		data->cmd = cmd;

		ppm_dbg(IPI, "ptpod test activate=%d\n",
			data->u.ptpod_test.activate);

		ret = sspm_ipi_send_sync(IPI_ID_PPM,
			opt, data, PPM_D_LEN, &ack_data, 1);
		if (ret != 0)
			ppm_err("sspm_ipi_send_sync failed, ret=%d\n", ret);
		else if (ack_data < 0) {
			ret = ack_data;
			ppm_err("cmd(0x%x) return %d\n", cmd, ret);
		}
		break;

	default:
		ppm_err("@%s cmd(0x%x) wrong!!!\n", __func__, cmd);
		return -1;
	}

	delta = ktime_to_us(ktime_sub(ktime_get(), now));
	ppm_profile_update_ipi_exec_time(cmd, delta);

	return ret;
}

void ppm_ipi_init(unsigned int efuse_val, unsigned int cobra_tbl_addr)
{
	struct ppm_ipi_data data;

	data.u.init.efuse_val = efuse_val;
	data.u.init.cobra_tbl_addr = cobra_tbl_addr;
	data.u.init.dvfs_tbl_type = (unsigned int)ppm_main_info.dvfs_tbl_type;

	ppm_ipi_to_sspm_command(PPM_IPI_INIT, &data);
}

void ppm_ipi_update_limit(struct ppm_client_req req)
{
	struct ppm_ipi_data data;
	int i;

	for (i = 0; i < req.cluster_num; i++) {
		data.u.update_limit.cluster_limit[i].min_cpufreq_idx =
			(char)req.cpu_limit[i].min_cpufreq_idx;
		data.u.update_limit.cluster_limit[i].max_cpufreq_idx =
			(char)req.cpu_limit[i].max_cpufreq_idx;
		data.u.update_limit.cluster_limit[i].max_cpu_core =
			(unsigned char)req.cpu_limit[i].max_cpu_core;
		data.u.update_limit.cluster_limit[i].advise_freq_idx =
			(req.cpu_limit[i].advise_freq_idx < 0)
			? 0xFF : (char)req.cpu_limit[i].advise_freq_idx;
	}

	ppm_ipi_to_sspm_command(PPM_IPI_UPDATE_LIMIT, &data);
}

void ppm_ipi_thermal_limit_test(unsigned int budget)
{
	struct ppm_ipi_data data;

	data.u.thermal_limit_test.budget = budget;

	ppm_ipi_to_sspm_command(PPM_IPI_THERMAL_LIMIT_TEST, &data);
}

void ppm_ipi_ptpod_test(unsigned int activate)
{
	struct ppm_ipi_data data;

	data.u.ptpod_test.activate = activate;

	ppm_ipi_to_sspm_command(PPM_IPI_PTPOD_TEST, &data);
}
#endif

