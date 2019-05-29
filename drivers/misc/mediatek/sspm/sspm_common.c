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
#include "sspm_define.h"
#include "sspm_ipi.h"
#include "sspm_excep.h"
#include "sspm_sysfs.h"
#include "sspm_reservedmem.h"

#include "sspm_common.h"

#define SEM_TIMEOUT		5000
#define SSPM_INIT_FLAG	0x1


struct sspm_regs sspmreg;
struct platform_device *sspm_pdev;

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

	if (!read_back) {
		writel((1 << flag), sema);

		while (count != SEM_TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(sema) >> flag) & 0x1;
			if (read_back) {
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

	if (read_back) {
		/* Write 1 clear */
		writel((1 << flag), sema);
		read_back = (readl(sema) >> flag) & 0x1;
		if (!read_back)
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

