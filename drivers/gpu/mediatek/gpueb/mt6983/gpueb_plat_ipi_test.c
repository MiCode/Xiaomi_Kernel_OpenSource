// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb_plat_ipi_test.c
 * @brief   GPUEB platform related ipi testing
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
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

#include "gpueb_plat_ipi_test.h"
#include "gpueb_plat_ipi_setting.h"
#include "gpueb_common_helper.h"

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

int gpueb_plat_ipi_send_testing(void)
{
    int ret = 0;
    struct plat_ipi_send_data plat_send_data;

    plat_send_data.cmd = PLT_INIT;
    plat_send_data.u.ctrl.phys = 1;
    plat_send_data.u.ctrl.size = 2;
    plat_send_data.u.logger.enable = 1;

    // CH_PLATFORM message size is 16 byte, 4 slots
    ret = mtk_ipi_send_compl(
        &gpueb_plat_ipidev, // GPUEB's IPI device
        CH_PLATFORM, // Send channel
        0,   // 0: wait, 1: polling
        (void *)&plat_send_data, // Send data
        4,   // 4 slots message = 4 * 4 = 16tyte
        2000 // Timeout value in milisecond);
    );

    return ret;
}
