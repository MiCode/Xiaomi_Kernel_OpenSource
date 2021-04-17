// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb_common_ipi_init.c
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

int gpueb_common_ipi_init(struct platform_device *pdev)
{
    int i = 0;
    int ret;
    struct device *dev = &pdev->dev;
    void __iomem *base;
    struct resource *res;
    char name[32];

    /* MBOX device register
     *
     * Using mbox 0 to probe common mbox driver.
     * It will update gpueb_plat_mbox_table's IRQ set, clear, status register.
     *
     * Then init SRAM base address for mbox1 ~ mbox_total
     */
    gpueb_plat_mbox_table[0].mbdev = &gpueb_plat_mboxdev;
    ret = mtk_mbox_probe(pdev, gpueb_plat_mbox_table[0].mbdev, 0);
    if (ret != MBOX_DONE) {
        gpueb_pr_debug("MBOX probe fail at mbox0, ret = %d\n", ret);
        return ret;
    }
    gpueb_pr_debug("mbox-%d base = 0x%X\n",
                    0, gpueb_plat_mbox_table[0].mbdev->info_table[0].base);

    for (i = 1; i < gpueb_plat_mbox_table[0].mbdev->count; i++) {
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

    /*
     * IPI device register
     *
     * It must be noted that the number of GPUEB's
     * send pin and receive pin are the same.
     * If you need specific design on it, you must adjust
     * the registered ipi num. 
     */
    ret = mtk_ipi_device_register(
                &gpueb_plat_ipidev,
                pdev,
                &gpueb_plat_mboxdev,
                gpueb_plat_mbox_table[0].mbdev->send_count
            );
    if (ret != IPI_ACTION_DONE) {
        gpueb_pr_debug("ipi devcie register fail!");
        return ret;
    }

    return 0;
}
