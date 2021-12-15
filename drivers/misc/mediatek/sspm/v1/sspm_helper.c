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
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_helper.h"
#include "sspm_ipi.h"
#include "sspm_excep.h"
#include "sspm_sysfs.h"
#include "sspm_timesync.h"
#include "sspm_reservedmem.h"

#define SEM_TIMEOUT		5000
#define SSPM_INIT_FLAG	0x1

static int __init sspm_init(void);

struct sspm_regs sspmreg;
struct platform_device *sspm_pdev;
static struct workqueue_struct *sspm_workqueue;
static atomic_t sspm_inited = ATOMIC_INIT(0);
static atomic_t sspm_dev_inited = ATOMIC_INIT(0);
static unsigned int sspm_ready;

void memcpy_to_sspm(void __iomem *trg, const void *src, int size)
{
	int i, len;
	u32 __iomem *t = trg;
	const u32 *s = src;

	len = (size + 3) >> 2;

	for (i = 0; i < len; i++)
		*t++ = *s++;
}
EXPORT_SYMBOL_GPL(memcpy_to_sspm);

void memcpy_from_sspm(void *trg, const void __iomem *src, int size)
{
	int i, len;
	u32 *t = trg;
	const u32 __iomem *s = src;

	len = (size + 3) >> 2;

	for (i = 0; i < len; i++)
		*t++ = *s++;
}
EXPORT_SYMBOL_GPL(memcpy_from_sspm);

/*
 * acquire a hardware semaphore
 * @param flag: semaphore id
 */
int get_sspm_semaphore(int flag)
{
	void __iomem *sema = sspmreg.cfg + SSPM_CFG_OFS_SEMA;
	int read_back;
	int count = 0;
	int ret = -1;

	flag = (flag * 2) + 1;

	read_back = (readl(sema) >> flag) & 0x1;

	if (read_back == 0) {
		writel((1 << flag), sema);

		while (count != SEM_TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(sema) >> flag) & 0x1;
			if (read_back == 1) {
				ret = 1;
				break;
			}
			writel((1 << flag), sema);
			count++;
		}

		if (ret < 0)
			pr_debug("[SSPM] get semaphore %d TIMEOUT...!\n", flag);
	} else {
		pr_debug("[SSPM] already hold semaphore %d\n", flag);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(get_sspm_semaphore);

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 */
int release_sspm_semaphore(int flag)
{
	void __iomem *sema = sspmreg.cfg + SSPM_CFG_OFS_SEMA;
	int read_back;
	int ret = -1;

	flag = (flag * 2) + 1;

	read_back = (readl(sema) >> flag) & 0x1;

	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), sema);
		read_back = (readl(sema) >> flag) & 0x1;
		if (read_back == 0)
			ret = 1;
		else
			pr_debug("[SSPM] release semaphore %d failed!\n", flag);
	} else {
		pr_debug("[SSPM] try to release semaphore %d not own by me\n",
			flag);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(release_sspm_semaphore);

/*
 * schedule a work on sspm's work queue
 * @param sspm_ws: work_struct to schedule
 */
void sspm_schedule_work(struct sspm_work_struct *sspm_ws)
{
	queue_work(sspm_workqueue, &sspm_ws->work);
}
EXPORT_SYMBOL_GPL(sspm_schedule_work);

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
EXPORT_SYMBOL_GPL(is_sspm_ready);

static int sspm_device_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;

	if (atomic_inc_return(&sspm_dev_inited) != 1)
		return -1;

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

	if (sspm_ipi_init()) {
		pr_err("[SSPM] IPI Init Failed\n");
		return -1;
	}

	pr_info("SSPM is ready to service IPI\n");
#ifdef CONFIG_OF_RESERVED_MEM
	if (sspm_reserve_memory_init(sspm_pdev)) {
		pr_err("[SSPM] Reserved Memory Failed\n");
		atomic_set(&sspm_inited, 1);
		return -1;
	}
#endif
	if (of_property_read_bool(pdev->dev.of_node, "sspm_share_buffer_supported")) {
		if (sspm_sbuf_init()) {
			pr_err("[SSPM] Shared Buffer Init Failed\n");
			return -1;
		}
	}

	return 0;
}

static const struct of_device_id sspm_of_match[] = {
	{ .compatible = "mediatek,sspm", },
	{},
};

static const struct platform_device_id sspm_id_table[] = {
	{ "sspm", 0},
	{ },
};

static struct platform_driver mtk_sspm_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.probe = sspm_device_probe,
	.driver = {
		.name = "sspm",
		.owner = THIS_MODULE,
		.of_match_table = sspm_of_match,
	},
	.id_table = sspm_id_table,
};

/*
 * driver initialization entry point
 */
static int __init sspm_init(void)
{
	if (atomic_inc_return(&sspm_inited) != 1)
		return 0;

	/* static initialise */
	sspm_ready = 0;

	sspm_workqueue = create_workqueue("SSPM_WQ");

	if (!sspm_workqueue) {
		pr_err("[SSPM] Workqueue Create Failed\n");
		goto error;
	}

	if (platform_driver_register(&mtk_sspm_driver)) {
		pr_err("[SSPM] Device Init Failed\n");
		goto error;
	}
	pr_debug("[SSPM] Helper Init\n");

	sspm_ready = 1;

	atomic_set(&sspm_inited, 1);
	return 0;

error:
	atomic_set(&sspm_inited, 1);
	return -1;
}

static int __init sspm_module_init(void)
{
	if (sspm_sysfs_init()) {
		pr_err("[SSPM] Sysfs Init Failed\n");
		return -1;
	}

#if SSPM_PLT_SERV_SUPPORT
	if (sspm_plt_init()) {
		pr_err("[SSPM] Platform Init Failed\n");
		return -1;
	}
	pr_info("SSPM platform service is ready\n");
#endif

	sspm_lock_emi_mpu();

	return 0;
}

arch_initcall(sspm_init);
module_init(sspm_module_init);
