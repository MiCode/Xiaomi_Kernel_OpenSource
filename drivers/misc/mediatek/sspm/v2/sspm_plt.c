/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>
#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
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
#include <linux/types.h>
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_helper.h"
#include "sspm_ipi_id.h"
#include "sspm_reservedmem.h"
#include "sspm_reservedmem_define.h"
#if SSPM_LOGGER_SUPPORT
#include "sspm_logger.h"
#endif
#include "sspm_sysfs.h"

#if SSPM_PLT_SERV_SUPPORT

struct plt_ctrl_s {
	unsigned int magic;
	unsigned int size;
	unsigned int mem_sz;
#if SSPM_LOGGER_SUPPORT
	unsigned int logger_ofs;
#endif
};

static ssize_t sspm_alive_show(struct device *kobj,
	struct device_attribute *attr, char *buf)
{

	struct plt_ipi_data_s ipi_data;
	int ret;

	ipi_data.cmd = 0xDEAD;
	sspm_plt_ackdata = 0;

	ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_PLATFORM, IPI_SEND_WAIT,
		&ipi_data, sizeof(ipi_data) / SSPM_MBOX_SLOT_SIZE, 10);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		sspm_plt_ackdata ? "Alive" : "Dead");
}
DEVICE_ATTR(sspm_alive, 0444, sspm_alive_show, NULL);


int __init sspm_plt_init(void)
{
	phys_addr_t phys_addr, virt_addr, mem_sz;
	struct plt_ipi_data_s ipi_data;
	struct plt_ctrl_s *plt_ctl;
	int ret;
	unsigned int last_ofs;
#if SSPM_LOGGER_SUPPORT
	unsigned int last_sz;
#endif
	unsigned int *mark;
	unsigned char *b;


	ret = sspm_sysfs_create_file(&dev_attr_sspm_alive);

	if (unlikely(ret != 0))
		goto error;

	phys_addr = sspm_reserve_mem_get_phys(SSPM_MEM_ID);
	if (phys_addr == 0) {
		pr_err("SSPM: Can't get logger phys mem\n");
		goto error;
	}

	virt_addr = sspm_reserve_mem_get_virt(SSPM_MEM_ID);
	if (virt_addr == 0) {
		pr_err("SSPM: Can't get logger virt mem\n");
		goto error;
	}

	mem_sz = sspm_reserve_mem_get_size(SSPM_MEM_ID);
	if (mem_sz == 0) {
		pr_err("SSPM: Can't get logger mem size\n");
		goto error;
	}

	b = (unsigned char *) (uintptr_t)virt_addr;
	for (last_ofs = 0; last_ofs < sizeof(*plt_ctl); last_ofs++)
		b[last_ofs] = 0x0;

	mark = (unsigned int *) (uintptr_t)virt_addr;
	*mark = PLT_INIT;
	mark = (unsigned int *) ((unsigned char *) (uintptr_t)
		virt_addr + mem_sz - 4);
	*mark = PLT_INIT;

	plt_ctl = (struct plt_ctrl_s *) (uintptr_t)virt_addr;
	plt_ctl->magic = PLT_INIT;
	plt_ctl->size = sizeof(*plt_ctl);
	plt_ctl->mem_sz = mem_sz;

	last_ofs = plt_ctl->size;


	pr_debug("SSPM: %s(): after plt, ofs=%u\n", __func__, last_ofs);

#if SSPM_LOGGER_SUPPORT
	plt_ctl->logger_ofs = last_ofs;
	last_sz = sspm_logger_init(virt_addr + last_ofs, mem_sz - last_ofs);

	if (last_sz == 0) {
		pr_err("SSPM: sspm_logger_init return fail\n");
		goto error;
	}

	last_ofs += last_sz;
	pr_debug("SSPM: %s(): after logger, ofs=%u\n", __func__, last_ofs);
#endif

	ipi_data.cmd = PLT_INIT;
	ipi_data.u.ctrl.phys = phys_addr;
	ipi_data.u.ctrl.size = mem_sz;
	sspm_plt_ackdata = 0;

	ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_PLATFORM,
		IPI_SEND_POLLING, &ipi_data,
		sizeof(ipi_data) / SSPM_MBOX_SLOT_SIZE, 10);
	if (ret) {
		pr_err("SSPM: plt IPI fail ret=%d\n", ret);
		goto error;
	}

	if (!sspm_plt_ackdata) {
		pr_err("SSPM: plt IPI init fail, ackdata=%d\n",
		sspm_plt_ackdata);
		goto error;
	}

#if SSPM_LOGGER_SUPPORT
	sspm_logger_init_done();
#endif

	return 0;

error:
	return -1;
}
#endif
