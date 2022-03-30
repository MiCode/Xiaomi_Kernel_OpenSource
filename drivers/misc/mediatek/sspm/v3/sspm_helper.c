// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
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
#include "sspm_define.h"
#include "sspm_helper.h"
#include "sspm_sysfs.h"
#include "sspm_reservedmem.h"
#include "sspm_timesync.h"

struct sspm_regs sspmreg;
struct platform_device *sspm_pdev;
static struct workqueue_struct *sspm_workqueue;
static atomic_t sspm_inited = ATOMIC_INIT(0);
static atomic_t sspm_dev_inited = ATOMIC_INIT(0);
static unsigned int sspm_ready;


/*
 * schedule a work on sspm's work queue
 * @param sspm_ws: work_struct to schedule
 */
void sspm_schedule_work(struct sspm_work_struct *sspm_ws)
{
	queue_work(sspm_workqueue, &sspm_ws->work);
}

/*
 * @return: 1 if sspm is ready for running tasks
 */
unsigned int is_sspm_ready(void)
{
	if (sspm_ready)
		return 1;
	else
		return 0;
}

static int __init sspm_module_init(void)
{
	if (atomic_inc_return(&sspm_inited) != 1)
		return 0;

	pr_info("[SSPM] sspm_module Init.\n");

	/* static initialise */
	sspm_ready = 0;
	sspm_workqueue = create_workqueue("SSPM_WQ");

	if (!sspm_workqueue) {
		pr_err("[SSPM] Workqueue Create Failed\n");
		goto error;
	}

#if IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (sspm_reserve_memory_init()) {
		pr_err("[SSPM] Reserved Memory Failed\n");
		goto error;
	}
#endif

	if (sspm_sysfs_init()) {
		pr_err("[SSPM] Sysfs Init Failed\n");
		goto error;
	}

#if SSPM_PLT_SERV_SUPPORT
	if (sspm_plt_init()) {
		pr_err("[SSPM] Platform Init Failed\n");
		goto error;
	}
	pr_info("SSPM platform service is ready\n");
#endif

	if (sspm_timesync_init()) {
		pr_err("[SSPM] Timesync Init Failed\n");
		goto error;
	}

	sspm_lock_emi_mpu();

	pr_debug("[SSPM] sspm_module Done\n");

	sspm_ready = 1;

	atomic_set(&sspm_inited, 1);
	return 0;

error:
	atomic_set(&sspm_inited, 1);
	return -1;
}

static int __init sspm_device_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	unsigned int fake_sspm;
	int ret;

	if (atomic_inc_return(&sspm_dev_inited) != 1)
		return -1;

	ret = of_property_read_u32(pdev->dev.of_node,
		"mediatek,fake_sspm", &fake_sspm);

	if (!ret) {
		pr_info("[SSPM] It's fake probe\n");
		return 0;
	}

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

	pr_debug("[SSPM] sspm irq=%d, cfgreg=0x%p\n", sspmreg.irq, sspmreg.cfg);

	sspm_pdev = pdev;
#ifdef SSPM_MBOX_SHARE_SUPPORT
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mbox_share");
	sspmreg.mboxshare = devm_ioremap_resource(dev, res);

	if (IS_ERR((void const *) sspmreg.mboxshare)) {
		pr_err("[SSPM] Unable to ioremap mboxshare\n");
		return -1;
	}
#endif

#ifdef SSPM_SHARE_BUFFER_SUPPORT
	if (sspm_sbuf_init()) {
		pr_err("[SSPM] Shared Buffer Init Failed\n");
		return -1;
	}
#endif

	pr_info("[SSPM] sspm_pdrv probe Done.\n");

	sspm_module_init();

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int sspm_suspend(struct device *dev)
{
	sspm_timesync_suspend();

	return 0;
}

static int sspm_resume(struct device *dev)
{
	sspm_timesync_resume();

	return 0;
}

static const struct dev_pm_ops sspm_dev_pm_ops = {
	.suspend = sspm_suspend,
	.resume  = sspm_resume,
};
#endif

static const struct of_device_id sspm_of_match[] = {
	{ .compatible = "mediatek,sspm", },
	{},
};

static const struct platform_device_id sspm_id_table[] = {
	{ "sspm", 0},
	{ },
};

static struct platform_driver mtk_sspm_driver __refdata = {
	.probe = sspm_device_probe,
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "sspm",
		.owner = THIS_MODULE,
		.of_match_table = sspm_of_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &sspm_dev_pm_ops,
#endif
	},
	.id_table = sspm_id_table,
};

/*
 * driver initialization entry point
 */
static int __init sspm_pdrv_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_sspm_driver);
	if (ret)
		pr_err("[SSPM] sspm platform driver Init Failed\n");

	return ret;
}

static void __exit sspm_pdrv_exit(void)
{
	pr_info("[SSPM] sspm platform driver Exit.\n");
}

MODULE_SOFTDEP("pre: tinysys-scmi.ko");
MODULE_DESCRIPTION("MEDIATEK Module SSPM platform driver");
MODULE_LICENSE("GPL v2");

subsys_initcall(sspm_pdrv_init);
module_exit(sspm_pdrv_exit);
