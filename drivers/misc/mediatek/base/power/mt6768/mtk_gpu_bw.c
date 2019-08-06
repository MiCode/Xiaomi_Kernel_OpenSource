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

/**
 * @file	mtk_gpu_bw
 * @brief   Driver for GPU BW Prediction
 */

#include <linux/pm_qos.h>
#include <sspm_ipi.h>
#include <sspm_ipi_pin.h>
#include <helio-dvfsrc-ipi.h>
#include "mtk_gpufreq_core.h"
#include "mtk_gpu_bw.h"

static unsigned int g_cur_request_bw;
static struct pm_qos_request gpu_qos_request;

#ifdef CONFIG_MTK_QOS_SUPPORT
void mt_gpu_bw_toggle(int i32Restore)
{
	if (i32Restore) /* powered on */
		pm_qos_update_request(&gpu_qos_request, g_cur_request_bw);
	else
		pm_qos_update_request(&gpu_qos_request, 0);
}

uint32_t gpu_bw_pull;
void mt_gpu_bw_qos_vcore(unsigned int ui32BW)
{
	g_cur_request_bw = ui32BW + gpu_bw_pull;

	pm_qos_update_request(&gpu_qos_request, g_cur_request_bw);

}
EXPORT_SYMBOL(mt_gpu_bw_qos_vcore);

static int mt_gpu_bw_ap2sspm(unsigned int eCMD, int type)
{
	struct qos_ipi_data qos_d;
	int md32Ret = -1;
	int apDebug = -1;

	qos_d.cmd = eCMD;


	/*
	 *  sspm_ipi_send_sync( $IPI_ID, $IPI_OPT, $sending_message, $IPI_ID_size, $pointer_to_save_md32return);
	 *  $IPI_ID_size == 0 -> use registed default size according to $IPI_ID
	 *
	 * apDebug = sspm_ipi_send_sync_new(IPI_ID_GPU_DVFS, IPI_OPT_POLLING, &gdvfs_d, 1, &md32Ret, 1);
	 */
	apDebug = sspm_ipi_send_sync(IPI_ID_QOS, IPI_OPT_POLLING, &qos_d, 1, &md32Ret, 1);

	if (apDebug < 0 || md32Ret < 0) {
		dump_stack();

		if (apDebug < 0)
			gpufreq_pr_debug("%s: AP side err (%d)\n", __func__, apDebug);
		if (md32Ret < 0)
			gpufreq_pr_debug("%s: SSPM err (%d)\n", __func__, md32Ret);

		md32Ret = -1;
	}
	return md32Ret;
}

unsigned int mt_gpu_bw_get_BW(int type)
{
	unsigned int ret;

	ret = mt_gpu_bw_ap2sspm(QOS_IPI_GET_GPU_BW, type);
	return ret;
}
EXPORT_SYMBOL(mt_gpu_bw_get_BW);


void mt_gpu_bw_init(void)
{
	pm_qos_add_request(&gpu_qos_request, PM_QOS_GPU_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
}

module_param(gpu_bw_pull, uint, 0644);

#else /* do nothing function if QOS is disabled */
void mt_gpu_bw_toggle(int i32Restore)
{
	;
}

void mt_gpu_bw_qos_vcore(unsigned int ui32BW)
{
	;
}
EXPORT_SYMBOL(mt_gpu_bw_qos_vcore);

unsigned int mt_gpu_bw_get_BW(int type)
{
	return -1;
}
EXPORT_SYMBOL(mt_gpu_bw_get_BW);


void mt_gpu_bw_init(void)
{
	;
}
#endif

