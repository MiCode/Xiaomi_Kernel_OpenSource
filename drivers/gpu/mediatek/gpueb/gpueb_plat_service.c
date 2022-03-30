// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb_ipi.c
 * @brief   IPI init flow for gpueb
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>
#include <mboot_params.h>

#include "gpueb_ipi.h"
#include "gpueb_plat_service.h"
#include "gpueb_helper.h"

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

int plat_service_init_ret;

int gpueb_plat_service_init(struct platform_device *pdev)
{
	int ret = 0;
	int channel_id = 0;
#if PLAT_IPI_TEST
	struct plat_ipi_send_data plat_send_data;
#endif

	channel_id = gpueb_get_send_PIN_ID_by_name("IPI_ID_PLATFORM");
	if (channel_id == -1) {
		gpueb_pr_debug("get channel ID fail!");
		return -1;
	}

#if !IPI_TEST
	// IPI channel - CH_PLATFORM register
	ret = mtk_ipi_register(&gpueb_ipidev,
			channel_id,
			NULL,
			NULL,
			(void *)&plat_service_init_ret);

	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_debug("%s: ipi:#%d register fail! ret = %d\n",
				__func__, channel_id, ret);
		if (ret != IPI_DUPLEX)
			return ret;
	}
#endif

#if PLAT_IPI_TEST
	/* Check gpueb alive and IPI is OK */
	plat_send_data.cmd = 0xDEAD;
	ret = mtk_ipi_send(
		&gpueb_ipidev, // GPUEB's IPI device
		channel_id, // Send channel
		0, // 0: wait, 1: polling
		(void *)&plat_send_data, // Send data
		1, // 1 slots message = 1 * 4 = 4 bytes
		IPI_TIMEOUT_MS); // Timeout value in milisecond
	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_info("%s: IPI fail ret=%d\n", __func__, ret);
		return ret;
	}

	ret = mtk_ipi_recv(&gpueb_ipidev, channel_id);
	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_info("%s: IPI fail ret=%d\n", __func__, ret);
		return ret;
	}

	if (plat_service_init_ret == 1)
		gpueb_pr_info("%s: plt IPI success, recv data=%d\n",
			__func__, plat_service_init_ret);
#endif // PLAT_IPI_TEST

	return ret;
}