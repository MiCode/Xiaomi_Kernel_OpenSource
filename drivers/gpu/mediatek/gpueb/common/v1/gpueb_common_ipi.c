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

#include "gpueb_plat_ipi_setting.h"
#include "gpueb_common_helper.h"
#ifdef MT_GPUEB_IPI_TEST
#include "gpueb_plat_ipi_test.h"
#endif

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

static int ack_data;

int gpueb_common_mbox_init(struct platform_device *pdev)
{
    int i = 0;
    int ret;
    struct device *dev = &pdev->dev;
    void __iomem *base;
    struct resource *res;
    char name[32];

    /*
     * Using mbox 0 to probe common mbox driver.
     * It will update gpueb_plat_mbox_table's IRQ set, clear, status register.
     */
    gpueb_plat_mbox_table[0].mbdev = &gpueb_plat_mboxdev;
    ret = mtk_mbox_probe(pdev, gpueb_plat_mbox_table[0].mbdev, 0);
    if (ret != MBOX_DONE) {
        gpueb_pr_debug("MBOX probe fail at mbox0, ret = %d\n", ret);
        return ret;
    }
    gpueb_pr_debug("mbox-%d base = 0x%X\n",
                    0, gpueb_plat_mbox_table[0].mbdev->info_table[0].base);

    for (i = 1; i < GPUEB_MBOX_TOTAL; i++) {
        gpueb_plat_mbox_table[i].mbdev = &gpueb_plat_mboxdev;

        // Get base address of MBOX i
        snprintf(name, sizeof(name), "mbox%d_base", i);
        res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
        base = devm_ioremap_resource(dev, res);
        if (IS_ERR((void const *) base)) {
            ret = PTR_ERR(base);
            gpueb_pr_debug("mbox-%d can't remap base\n", i);
            return ret;
        }
        gpueb_pr_debug("mbox-%d base = 0x%X\n", i, base);

        // Bind MBOX i's SRAM with MOBX IRQ set, clear, status register
        ret = mtk_smem_init(pdev, gpueb_plat_mbox_table[i].mbdev, i, base,
                gpueb_plat_mbox_table[0].mbdev->info_table[0].set_irq_reg,
                gpueb_plat_mbox_table[0].mbdev->info_table[0].clr_irq_reg,
                gpueb_plat_mbox_table[0].mbdev->info_table[0].send_status_reg,
                gpueb_plat_mbox_table[0].mbdev->info_table[0].recv_status_reg);

        if (ret != MBOX_DONE) {
            gpueb_pr_debug("mbox probe fail at mbox-%d, ret %d\n", i, ret);
            return ret;
        }
    }

    return 0;
}

int gpueb_common_ipi_init(struct platform_device *pdev)
{
    int ret = 0;

    // IPI device register
    ret = mtk_ipi_device_register(&gpueb_plat_ipidev,
                                    pdev,
                                    &gpueb_plat_mboxdev,
                                    GPUEB_IPI_COUNT);
    if (ret != IPI_ACTION_DONE) {
        gpueb_pr_debug("ipi devcie register fail!");
        return ret;
    }

    // IPI channel - CH_PLATFORM register
    ret = mtk_ipi_register(&gpueb_plat_ipidev,
                            CH_PLATFORM,
                            NULL, NULL,
                            (void *)&ack_data);
    if (ret != IPI_ACTION_DONE) {
        gpueb_pr_debug("ipi register fail!");
        return ret;
    }

#ifdef MT_GPUEB_IPI_TEST
    ret = gpueb_plat_ipi_send_testing();
    if (ret != 0)
        gpueb_pr_info("@%s: ipi send testing fail\n", __func__);
#endif

    return 0;
}