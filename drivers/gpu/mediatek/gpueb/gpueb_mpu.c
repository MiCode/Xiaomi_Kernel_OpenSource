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
#include "gpueb_mpu.h"

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

int mpu_ack_data;

int gpueb_mpu_init(struct platform_device *pdev)
{
#if IPI_SUPPORT
	int ret = 0;
	int channel_id = -1;
#endif
	struct mpu_ipi_send_data mpu_send_data;

	/* register IPI channel */
#if IPI_SUPPORT
	channel_id = gpueb_get_send_PIN_ID_by_name("IPI_ID_GPUMPU");
	if (channel_id == -1) {
		gpueb_pr_info("get channel ID fail!");
		return -1;
	}
	ret = mtk_ipi_register(&gpueb_ipidev,
			channel_id,
			NULL,
			NULL,
			(void *)&mpu_ack_data);
	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_info("ipi register fail!");
		return ret;
	}
#endif

	/* prepare send data */
	mpu_send_data.cmd = CMD_INIT_MPU_TABLE;
	mpu_send_data.u.mpu_table.phys_base = (u64)gpueb_get_reserve_mem_phys_by_name("MEM_ID_MPU");
	mpu_send_data.u.mpu_table.size = (u64)gpueb_get_reserve_mem_size_by_name("MEM_ID_MPU");
	gpueb_pr_debug("%s: cmd=%d, phys_base=0x%llx, size=0x%llx, sizeof(struct mpu_ipi_send_data)=%d\n",
			__func__,
			mpu_send_data.cmd,
			mpu_send_data.u.mpu_table.phys_base,
			mpu_send_data.u.mpu_table.size,
			sizeof(struct mpu_ipi_send_data));

	/* send IPI to GPUEB */
#if IPI_SUPPORT
	ret = mtk_ipi_send_compl(
		&gpueb_ipidev,
		channel_id,
		0, // 0=IPI_SEND_WAIT, 1=IPI_SEND_POLLING
		(void *)&mpu_send_data,
		MPU_IPI_SEND_DATA_LEN,
		IPI_TIMEOUT_MS);
	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_info("%s: IPI fail ret=%d\n", __func__, ret);
		return ret;
	}
#endif

	return 0;
}
