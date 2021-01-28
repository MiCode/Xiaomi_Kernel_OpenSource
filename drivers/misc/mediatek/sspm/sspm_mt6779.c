// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include "sspm_define.h"
#include "sspm_ipi.h"
#include "sspm_sysfs.h"
#include "sspm_reservedmem.h"

#include "sspm_common.h"
#include "sspm_ipi_mbox.h"

#include "sspm_ipi_define_mt6779.h"
#include "sspm_reservedmem_define_mt6779.h"
#include "sspm_timesync.h"

static struct workqueue_struct *mt6779_sspm_workqueue;
static atomic_t sspm_inited = ATOMIC_INIT(0);


static int __init mt6779_sspm_module_init(void)
{
	if (atomic_inc_return(&sspm_inited) != 1)
		return 0;

	pr_info("[SSPM] mt6779-sspm_module_init.\n");

	/* static initialise */
	sspm_ready = 0;

	mt6779_sspm_workqueue = create_workqueue("mt6779-SSPM_WQ");

	if (!mt6779_sspm_workqueue) {
		pr_err("[SSPM] Workqueue Create Failed\n");
		goto error;
	}

#ifdef CONFIG_OF_RESERVED_MEM
	if (sspm_reserve_memory_init()) {
		pr_err("[SSPM] Reserved Memory Failed\n");
		goto error;
	}
#endif

	sspm_ready = 1;

	if (sspm_sysfs_init()) {
		pr_err("[SSPM] Sysfs Init Failed\n");
		return -1;
	}

	if (sspm_ipi_init()) {
		pr_err("[SSPM] IPI Init Failed\n");
		return -1;
	}

	pr_info("SSPM is ready to service IPI\n");


#if SSPM_PLT_SERV_SUPPORT
	if (sspm_plt_init()) {
		pr_err("[SSPM] Platform Init Failed\n");
		return -1;
	}
	pr_info("SSPM platform service is ready\n");
#endif

#if SSPM_TIMESYNC_SUPPORT
	if (sspm_timesync_init()) {
		pr_err("SSPM timesync init fail\n");
		return -1;
	}
#endif

	sspm_lock_emi_mpu(4);

	return 0;

error:
	atomic_set(&sspm_inited, 1);
	return -1;
}

static int __init mt6779_sspm_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct clk *sspm_26m, *sspm_32k, *sspm_bus_hclk;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfgreg");
	sspmreg.cfg = devm_ioremap_resource(dev, res);

	if (IS_ERR((void const *) sspmreg.cfg)) {
		pr_err("[SSPM] Unable to ioremap registers\n");
		return -1;
	}

	sspmreg.cfgregsize = (unsigned int) resource_size(res);

	sspmreg.irq = platform_get_irq_byname(pdev, "ipc");
	if (sspmreg.irq < 0) {
		pr_err("[SSPM] Unable to get IRQ\n");
		return -1;
	}

	pr_info("[SSPM] mt6779-sspm irq=%d, cfgreg=0x%p\n",
			sspmreg.irq, sspmreg.cfg);

	sspm_26m = devm_clk_get(dev, "sspm_26m");
	if (IS_ERR(sspm_26m)) {
		pr_err("[SSPM] Get sspm clock fail: 26M.\n");
		return -1;
	}

	sspm_32k = devm_clk_get(dev, "sspm_32k");
	if (IS_ERR(sspm_32k)) {
		pr_err("[SSPM] Get sspm clock fail: 32K.\n");
		return -1;
	}

	sspm_bus_hclk = devm_clk_get(dev, "sspm_bus_hclk");
	if (IS_ERR(sspm_bus_hclk)) {
		pr_err("[SSPM] Get sspm clock fail: Bus_hclk.\n");
		return -1;
	}

	clk_prepare_enable(sspm_26m);
	clk_prepare_enable(sspm_32k);
	clk_prepare_enable(sspm_bus_hclk);

	sspm_pdev = pdev;

#ifdef SSPM_SHARE_BUFFER_SUPPORT
	if (sspm_sbuf_init()) {
		pr_err("[SSPM] Shared Buffer Init Failed\n");
		return -1;
	}
#endif

	mbox_table = mt6779_mbox_table;
	send_pintable = mt6779_send_pintable;
	recv_pintable = mt6779_recv_pintable;
	pin_name = mt6779_pin_name;

	sspm_reserve_mblock = mt6779_sspm_reserve_mblock;

	pr_info("[SSPM] mt6779-sspm_probe Done.\n");

	mt6779_sspm_module_init();

	return 0;
}

#ifdef CONFIG_PM
static int mt6779_sspm_suspend(struct device *dev)
{
	sspm_timesync_suspend();
	return 0;
}

static int mt6779_sspm_resume(struct device *dev)
{
	sspm_timesync_resume();
	return 0;
}

static const struct dev_pm_ops mt6779_sspm_dev_pm_ops = {
	.suspend = mt6779_sspm_suspend,
	.resume  = mt6779_sspm_resume,
};
#endif

static const struct of_device_id mt6779_sspm_of_match[] = {
	{ .compatible = "mediatek,mt6779-sspm", },
	{},
};

static const struct platform_device_id mt6779_sspm_id_table[] = {
	{ "mt6779-sspm", 0},
	{ },
};

static struct platform_driver mtk_mt6779_sspm_driver __refdata = {
	.probe  = mt6779_sspm_probe,
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "mt6779-sspm",
		.owner = THIS_MODULE,
		.of_match_table = mt6779_sspm_of_match,
#ifdef CONFIG_PM
		.pm = &mt6779_sspm_dev_pm_ops,
#endif
	},
	.id_table = mt6779_sspm_id_table,
};

/*
 * driver initialization entry point
 */
static int __init mt6779_sspm_init(void)
{
	return platform_driver_register(&mtk_mt6779_sspm_driver);
}

static void __exit mt6779_sspm_exit(void)
{
	pr_info("[SSPM] mt6779-sspm Exit.\n");
}

MODULE_DESCRIPTION("MEDIATEK Module SSPM driver");
MODULE_LICENSE("GPL v2");

subsys_initcall(mt6779_sspm_init);
module_exit(mt6779_sspm_exit);

