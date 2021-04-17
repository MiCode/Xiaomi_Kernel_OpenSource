// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb_common_ipi.c
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

#include "gpueb_common_ipi.h"
#include "gpueb_common_helper.h"
#include "gpueb_plat_config.h"

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

int plat_service_ack_data;

int gpueb_common_plat_service_init(struct platform_device *pdev)
{
    int ret = 0;
    int channel_id = 0;

    channel_id = gpueb_plat_get_channelID_by_name("CH_PLATFORM");
    if (channel_id == -1) {
        gpueb_pr_debug("get channel ID fail!");
        return -1;
    }

    // IPI channel - CH_PLATFORM register
    ret = mtk_ipi_register(&gpueb_plat_ipidev,
                            channel_id,
                            NULL,
                            NULL,
                            (void *)&plat_service_ack_data);

    if (ret != IPI_ACTION_DONE) {
        gpueb_pr_debug("ipi register fail!");
        return ret;
    }

    if (gpueb_plat_is_ipi_test_support()) {
        ret = gpueb_plat_ipi_send_testing();
        if (ret != 0)
            gpueb_pr_info("@%s: ipi send testing fail\n", __func__);
    }

    return 0;
}