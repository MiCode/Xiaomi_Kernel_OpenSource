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
#include "gpueb_helper.h"
#include "gpueb_reserved_mem.h"

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

int plat_service_ack_data;

int gpueb_plat_service_init(struct platform_device *pdev)
{
	int ret = 0;
	int channel_id = 0;
	struct plat_ipi_send_data plat_send_data;

	channel_id = gpueb_get_send_PIN_ID_by_name("IPI_ID_PLATFORM");
	if (channel_id == -1) {
		gpueb_pr_debug("get channel ID fail!");
		return -1;
	}

	// IPI channel - CH_PLATFORM register
	ret = mtk_ipi_register(&gpueb_ipidev,
			channel_id,
			NULL,
			NULL,
			(void *)&plat_service_ack_data);

	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_debug("ipi register fail!");
		return ret;
	}

	plat_send_data.cmd = PLT_INIT;
	plat_send_data.u.init.tab_phys = (u64)gpueb_get_reserve_mem_phys(1);

	// CH_PLATFORM message size is 16 bytes, 4 slots
	ret = mtk_ipi_send_compl(
		&gpueb_ipidev, // GPUEB's IPI device
		channel_id, // Send channel
		0, // 0: wait, 1: polling
		(void *)&plat_send_data, // Send data
		4, // 4 slots message = 4 * 4 = 16 bytes
		2000); // Timeout value in milisecond

	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_info("%s: IPI fail ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}