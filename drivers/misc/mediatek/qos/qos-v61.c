// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/kthread.h>

#include <sspm_ipi.h>
#include <sspm_ipi_pin.h>

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "mtk_qos_ipi.h"
#include "mtk_qos_sram.h"
#include "mtk_qos_common.h"

static const struct qos_ipi_cmd mt6761_qos_ipi_pin[] = {
	[QOS_IPI_QOS_ENABLE] = {
			.id = 0,
			.valid = true,
		},
	[QOS_IPI_QOS_BOUND] = {
			.id = 0x100,
			.valid = false,
		},
	[QOS_IPI_QOS_BOUND_ENABLE] = {
			.id = 0x100,
			.valid = false,
		},
	[QOS_IPI_QOS_BOUND_STRESS_ENABLE] = {
			.id = 0x100,
			.valid = false,
		},
	[QOS_IPI_SWPM_INIT] = {
			.id = 5,
			.valid = true,
		},
	[QOS_IPI_UPOWER_DATA_TRANSFER] = {
			.id = 6,
			.valid = true,
		},
	[QOS_IPI_UPOWER_DUMP_TABLE] = {
			.id = 7,
			.valid = true,
		},
	[QOS_IPI_GET_GPU_BW] = {
			.id = 8,
			.valid = true,
		},
	[QOS_IPI_SWPM_ENABLE] = {
			.id = 9,
			.valid = true,
		},
	[QOS_IPI_SMI_MET_MON] = {
			.id = 0x100,
			.valid = false,
		},
	[QOS_IPI_SETUP_GPU_INFO] = {
			.id = 0x100,
			.valid = false,
		},
	[QOS_IPI_SWPM_SET_UPDATE_CNT] = {
			.id = 0x100,
			.valid = false,
		},
};

static const struct qos_sram_addr mt6761_qos_sram_pin[] = {
	[QOS_DEBUG_0] = {
			.offset = 0x38,
			.valid = true,
		},
	[QOS_DEBUG_1] = {
			.offset = 0x3C,
			.valid = true,
		},
	[QOS_DEBUG_2] = {
			.offset = 0x40,
			.valid = true,
		},
	[QOS_DEBUG_3] = {
			.offset = 0x44,
			.valid = true,
		},
	[QOS_DEBUG_4] = {
			.offset = 0x48,
			.valid = true,
		},
	[MM_SMI_VENC] = {
			.offset = 0x100,
			.valid = false,
		},
	[MM_SMI_CAM] = {
			.offset = 0x100,
			.valid = false,
		},
	[MM_SMI_IMG] = {
			.offset = 0x100,
			.valid = false,
		},
	[MM_SMI_MDP] = {
			.offset = 0x100,
			.valid = false,
		},
	[MM_SMI_CLK] = {
			.offset = 0x100,
			.valid = false,
		},
	[MM_SMI_CLR] = {
			.offset = 0x100,
			.valid = false,
		},
	[MM_SMI_EXE] = {
			.offset = 0x100,
			.valid = false,
		},
	[MM_SMI_DUMP] = {
			.offset = 0x100,
			.valid = false,
		},
	[APU_CLK] = {
			.offset = 0x100,
			.valid = false,
		},
	[APU_BW_NORD] = {
			.offset = 0x100,
			.valid = false,
		},
	[DVFSRC_TIMESTAMP_OFFSET] = {
			.offset = 0x100,
			.valid = false,
		},
	[CM_STALL_RATIO_ID_0] = {
			.offset = 0x100,
			.valid = false,
		},
	[CM_STALL_RATIO_ID_1] = {
			.offset = 0x100,
			.valid = false,
		},
	[CM_STALL_RATIO_ID_2] = {
			.offset = 0x100,
			.valid = false,
		},
	[CM_STALL_RATIO_ID_3] = {
			.offset = 0x100,
			.valid = false,
		},
	[CM_STALL_RATIO_ID_4] = {
			.offset = 0x100,
			.valid = false,
		},
	[CM_STALL_RATIO_ID_5] = {
			.offset = 0x100,
			.valid = false,
		},
	[CM_STALL_RATIO_ID_6] = {
			.offset = 0x100,
			.valid = false,
		},
	[CM_STALL_RATIO_ID_7] = {
			.offset = 0x100,
			.valid = false,
		},
	[QOS_TOTAL_BW] = {
			.offset = 0x20,
			.valid = true,
		},
};


static const struct mtk_qos_soc mt6761_qos_data = {
	.ipi_pin = mt6761_qos_ipi_pin,
	.sram_pin = mt6761_qos_sram_pin,
	.qos_sspm_init = NULL,
	.qos_ipi_recv_handler = NULL,
};


static int mt6761_qos_probe(struct platform_device *pdev)
{
	return mtk_qos_probe(pdev, &mt6761_qos_data);
}


static const struct of_device_id mtk_qos_of_match[] = {
	{
		.compatible = "mediatek,mt6761-qos",
		.data = &mt6761_qos_data,
	}, {
		.compatible = "mediatek,mt6765-qos",
		.data = &mt6761_qos_data,
	}, {
		/* sentinel */
	},
};

static int mt6761_qos_remove(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver mt6761_qos_platdrv = {
	.probe	= mt6761_qos_probe,
	.remove	= mt6761_qos_remove,
	.driver	= {
		.name	= "mt6761-qos",
		.of_match_table = mtk_qos_of_match,
	},
};

static int __init mt6761_qos_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mt6761_qos_platdrv);

	return ret;
}

late_initcall(mt6761_qos_init)

static void __exit mt6761_qos_exit(void)
{
	platform_driver_unregister(&mt6761_qos_platdrv);
}
module_exit(mt6761_qos_exit)
