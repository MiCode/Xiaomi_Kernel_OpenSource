// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
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
#include "sspm_ipi.h"
#include "sspm_excep.h"
#include "sspm_reservedmem.h"
#include "sspm_reservedmem_define.h"
#if SSPM_LOGGER_SUPPORT
#include "sspm_logger.h"
#endif
#if SSPM_TIMESYNC_SUPPORT
#include "sspm_timesync.h"
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
#if SSPM_COREDUMP_SUPPORT
	unsigned int coredump_ofs;
#endif
#if SSPM_TIMESYNC_SUPPORT
	unsigned int ts_ofs;
#endif

};

#if (SSPM_COREDUMP_SUPPORT || SSPM_LASTK_SUPPORT)
/* dont send any IPI from this thread, use workqueue instead */
static int sspm_recv_thread(void *userdata)
{
	struct plt_ipi_data_s data;
	struct ipi_action dev;
	unsigned int rdata, ret;

	dev.data = &data;

	ret = sspm_ipi_recv_registration(IPI_ID_PLATFORM, &dev);

	do {
		rdata = 0;
		sspm_ipi_recv_wait(IPI_ID_PLATFORM);

		switch (data.cmd) {
#if SSPM_LASTK_SUPPORT
		case PLT_LASTK_READY:
			sspm_log_lastk_recv(data.u.logger.enable);
			break;
#endif
#if SSPM_COREDUMP_SUPPORT
		case PLT_COREDUMP_READY:
			sspm_log_coredump_recv(data.u.coredump.exists);
			break;
#endif
		}
		sspm_ipi_send_ack(IPI_ID_PLATFORM, &rdata);
	} while (!kthread_should_stop());

	return 0;
}
#endif

static ssize_t sspm_alive_show(struct device *kobj,
	struct device_attribute *attr, char *buf)
{

	struct plt_ipi_data_s ipi_data;
	int ackdata = 0;

	ipi_data.cmd = 0xDEAD;

	sspm_ipi_send_sync(IPI_ID_PLATFORM, IPI_OPT_WAIT,
		&ipi_data, sizeof(ipi_data) / SSPM_MBOX_SLOT_SIZE, &ackdata, 1);

	return snprintf(buf, PAGE_SIZE, "%s\n", ackdata ? "Alive" : "Dead");
}
DEVICE_ATTR(sspm_alive, 0444, sspm_alive_show, NULL);


int __init sspm_plt_init(void)
{
	phys_addr_t phys_addr, virt_addr, mem_sz;
	struct plt_ipi_data_s ipi_data;
	struct plt_ctrl_s *plt_ctl;
#if (SSPM_COREDUMP_SUPPORT || SSPM_LASTK_SUPPORT)
	struct task_struct *sspm_task;
#endif
	int ret, ackdata;
	unsigned int last_ofs;
#if (SSPM_COREDUMP_SUPPORT || SSPM_LOGGER_SUPPORT || SSPM_TIMESYNC_SUPPORT)
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

#if (SSPM_COREDUMP_SUPPORT || SSPM_LASTK_SUPPORT)
	sspm_task = kthread_run(sspm_recv_thread, NULL, "sspm_recv");
#endif
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

#if SSPM_COREDUMP_SUPPORT
	plt_ctl->coredump_ofs = last_ofs;
	last_sz = sspm_coredump_init(virt_addr + last_ofs, mem_sz - last_ofs);

	if (last_sz == 0) {
		pr_err("SSPM: sspm_coredump_init return fail\n");
		goto error;
	}

	last_ofs += last_sz;
	pr_debug("SSPM: %s(): after coredump, ofs=%u\n", __func__, last_ofs);
#endif

#if SSPM_TIMESYNC_SUPPORT
	plt_ctl->ts_ofs = last_ofs;
	last_sz = sspm_timesync_init(virt_addr + last_ofs, mem_sz - last_ofs);

	if (last_sz == 0) {
		pr_err("SSPM: sspm_timesync_init return fail\n");
		goto error;
	}

	last_ofs += last_sz;
	pr_debug("SSPM: %s(): after timesync, ofs=%u\n", __func__, last_ofs);
#endif

	ipi_data.cmd = PLT_INIT;
	ipi_data.u.ctrl.phys = phys_addr;
	ipi_data.u.ctrl.size = mem_sz;

	ret = sspm_ipi_send_sync(IPI_ID_PLATFORM, IPI_OPT_POLLING, &ipi_data,
			sizeof(ipi_data) / SSPM_MBOX_SLOT_SIZE, &ackdata, 1);
	if (ret != 0) {
		pr_err("SSPM: logger IPI fail ret=%d\n", ret);
		goto error;
	}

	if (!ackdata) {
		pr_err("SSPM: logger IPI init fail, ret=%d\n", ackdata);
		goto error;
	}

#if SSPM_TIMESYNC_SUPPORT
	sspm_timesync_init_done();
#endif
#if SSPM_COREDUMP_SUPPORT
	sspm_coredump_init_done();
#endif
#if SSPM_LOGGER_SUPPORT
	sspm_logger_init_done();
#endif

	return 0;

error:
	return -1;
}
#endif
