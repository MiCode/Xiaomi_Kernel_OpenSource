// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    mtk_common_init.c
 * @brief   GPUEB driver init and probe
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

#include "gpueb_common_helper.h"
#include "gpueb_common_ipi.h"
#include "gpueb_common_reserved_mem.h"

/*
 * ===============================================
 * SECTION : Local functions declaration
 * ===============================================
 */

static int __mt_gpueb_pdrv_probe(struct platform_device *pdev);

/*
 * ===============================================
 * SECTION : Local variables definition
 * ===============================================
 */

static bool g_probe_done;
static struct platform_device *g_pdev;

static const struct of_device_id g_gpueb_of_match[] = {
    { .compatible = "mediatek,gpueb" },
    { /* sentinel */ }
};

static struct platform_driver g_gpueb_pdrv = {
    .probe = __mt_gpueb_pdrv_probe,
    .remove = NULL,
    .driver = {
        .name = "gpueb",
        .owner = THIS_MODULE,
        .of_match_table = g_gpueb_of_match,
    },
};

unsigned int mt_gpueb_bringup(void)
{
    return MT_GPUEB_BRINGUP;
}
EXPORT_SYMBOL(mt_gpueb_bringup);

/*
 * GPUEB driver probe
 */
static int __mt_gpueb_pdrv_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct device_node *node;

    gpueb_pr_info("@%s: GPUEB driver probe start\n", __func__);


    node = of_find_matching_node(NULL, g_gpueb_of_match);
    if (!node)
        gpueb_pr_info("@%s: find GPU node failed\n", __func__);

    ret = gpueb_common_mbox_init(pdev);
    if (ret != 0)
        gpueb_pr_info("@%s: ipi init fail\n", __func__);

    ret = gpueb_common_ipi_init(pdev);
    if (ret != 0)
        gpueb_pr_info("@%s: ipi init fail\n", __func__);

    ret = gpueb_common_reserved_mem_init(pdev);
    if (ret != 0)
        gpueb_pr_info("@%s: ipi init fail\n", __func__);

#ifdef MT_GPUEB_LOGGER_ENABLE
	gpueb_pr_info("@%s: logger init\n", __func__);
	gpueb_logger_workqueue = create_singlethread_workqueue("GPUEB_LOG_WQ");
	if (gpueb_common_logger_init(pdev,
                gpueb_common_get_reserve_mem_virt(GPUEB_LOGGER_MEM_ID),
                gpueb_common_get_reserve_mem_size(GPUEB_LOGGER_MEM_ID)) == -1) {
		gpueb_pr_info("@%s: gpueb_common_logger_init\n", __func__);
		goto err;
	}
#endif

    g_pdev = pdev;
    g_probe_done = true;
    gpueb_pr_info("@%s: GPUEB driver probe done\n", __func__);

    return 0;
}

/*
 * Register the GPUEB driver
 */
static int __init __mt_gpueb_init(void)
{
    int ret = 0;

    if (mt_gpueb_bringup()) {
        gpueb_pr_info("skip driver init when bringup\n");
        return 0;
    }

    gpueb_pr_debug("start to initialize gpueb driver\n");

#ifdef CONFIG_PROC_FS
    // Create PROC FS
#endif

    // Register platform driver
    ret = platform_driver_register(&g_gpueb_pdrv);
    if (ret)
        gpueb_pr_info("fail to register gpueb driver\n");

    return ret;
}

/*
 * Unregister the GPUEB driver
 */
static void __exit __mt_gpueb_exit(void)
{
    platform_driver_unregister(&g_gpueb_pdrv);
}

module_init(__mt_gpueb_init);
module_exit(__mt_gpueb_exit);

MODULE_DEVICE_TABLE(of, g_gpueb_of_match);
MODULE_DESCRIPTION("MediaTek GPUEB-PLAT driver");
MODULE_LICENSE("GPL");