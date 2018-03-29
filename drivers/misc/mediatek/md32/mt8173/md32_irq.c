/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>
#include <mt-plat/aee.h>
#include <mt-plat/sync_write.h>

#include "md32_irq.h"

#define MD32_CFGREG_SIZE	0x100
#define MD32_AED_PHY_SIZE	(MD32_PTCM_SIZE + MD32_DTCM_SIZE + MD32_CFGREG_SIZE)
#define MD32_AED_STR_LEN	(512)

static struct workqueue_struct *wq_md32_reboot;

struct reg_md32_to_host_ipc {
	unsigned int ipc_md2host	:1;
	unsigned int			:7;
	unsigned int md32_ipc_int	:1;
	unsigned int wdt_int		:1;
	unsigned int pmem_disp_int	:1;
	unsigned int dmem_disp_int	:1;
	unsigned int			:20;
};

struct md32_aed_cfg {
	int *log;
	int log_size;
	int *phy;
	int phy_size;
	char *detail;
};

struct md32_reboot_work {
	struct work_struct work;
	struct md32_aed_cfg aed;
};

static struct md32_reboot_work work_md32_reboot;

struct md32_status_reg {
	unsigned int status;
	unsigned int pc;
	unsigned int r14;
	unsigned int r15;
	unsigned int m2h_irq;
};

static struct md32_status_reg md32_aee_status;

void md32_dump_regs(void)
{
	unsigned long *reg;

	pr_err("md32_dump_regs\n");
	pr_err("md32 status = 0x%08x\n", readl(MD32_BASE));
	pr_err("md32 pc=0x%08x, r14=0x%08x, r15=0x%08x\n",
		 readl(MD32_DEBUG_PC_REG), readl(MD32_DEBUG_R14_REG),
		 readl(MD32_DEBUG_R15_REG));
	pr_err("md32 to host inerrupt = 0x%08x\n", readl(MD32_TO_HOST_REG));
	pr_err("host to md32 inerrupt = 0x%08x\n", readl(HOST_TO_MD32_REG));

	pr_err("wdt en=%d, count=0x%08x\n",
		 (readl(MD32_WDT_REG) & 0x10000000) ? 1 : 0,
		 (readl(MD32_WDT_REG) & 0xFFFFF));

	/*dump all md32 regs*/
	for (reg = (unsigned long *)MD32_BASE;
	     (unsigned long)reg < (unsigned long)(MD32_BASE + 0x90); reg++) {
		if (!((uintptr_t)(void *)reg & 0xF)) {
			pr_err("\n");
			pr_err("[0x%p]   ", reg);
		}

		pr_err("0x%016lx  ", *reg);
	}
	pr_err("\n");
	pr_err("md32_dump_regs end\n");
}

void md32_aee_stop(void)
{
	pr_debug("md32_aee_stop\n");

	md32_aee_status.status = readl(MD32_BASE);
	md32_aee_status.pc = readl(MD32_DEBUG_PC_REG);
	md32_aee_status.r14 = readl(MD32_DEBUG_R14_REG);
	md32_aee_status.r15 = readl(MD32_DEBUG_R15_REG);

	mt_reg_sync_writel(0x0, MD32_BASE);

	pr_debug("md32_aee_stop end\n");
}

int md32_aee_dump(char *buf)
{
	unsigned long *reg;
	char *ptr = buf;

	ssize_t i, len;
	unsigned int log_start_idx;
	unsigned int log_end_idx;
	unsigned int log_buf_max_len;
	unsigned char *__log_buf = (unsigned char *)(MD32_DTCM +
				    md32_log_buf_addr);

	if (!buf)
		return 0;

	pr_debug("md32_aee_dump\n");
	ptr += sprintf(ptr, "md32 status = 0x%08x\n", md32_aee_status.status);
	ptr += sprintf(ptr, "md32 pc=0x%08x, r14=0x%08x, r15=0x%08x\n",
		       md32_aee_status.pc, md32_aee_status.r14,
		       md32_aee_status.r15);
	ptr += sprintf(ptr, "md32 to host irq = 0x%08x\n",
		       md32_aee_status.m2h_irq);
	ptr += sprintf(ptr, "host to md32 irq = 0x%08x\n",
		       readl(HOST_TO_MD32_REG));

	ptr += sprintf(ptr, "wdt en=%d, count=0x%08x\n",
		       (readl(MD32_WDT_REG) & 0x10000000) ? 1 : 0,
		       (readl(MD32_WDT_REG) & 0xFFFFF));

	/*dump all md32 regs */
	for (reg = (unsigned long *)MD32_BASE;
	     (unsigned long)reg < (unsigned long)(MD32_BASE + 0x90); reg++) {
		if (!((unsigned long)reg & 0xF)) {
			ptr += sprintf(ptr, "\n");
			ptr += sprintf(ptr, "[0x%016lx]   ",
				       (unsigned long)reg);
		}

		ptr += sprintf(ptr, "0x%016lx  ", *reg);
	}
	ptr += sprintf(ptr, "\n");

#define LOG_BUF_MASK (log_buf_max_len-1)
#define LOG_BUF(idx) (__log_buf[(idx) & LOG_BUF_MASK])

	log_start_idx = readl((void __iomem *)(MD32_DTCM +
			      md32_log_start_idx_addr));
	log_end_idx = readl((void __iomem *)(MD32_DTCM +
			    md32_log_end_idx_addr));
	log_buf_max_len = readl((void __iomem *)(MD32_DTCM +
				md32_log_buf_len_addr));

	ptr += sprintf(ptr, "log_buf_addr = 0x%08x\n",
			(unsigned int)md32_log_buf_addr);
	ptr += sprintf(ptr, "log_start_idx = %u\n", log_start_idx);
	ptr += sprintf(ptr, "log_end_idx = %u\n", log_end_idx);
	ptr += sprintf(ptr, "log_buf_max_len = %u\n", log_buf_max_len);

	ptr += sprintf(ptr, "<<md32 log buf start>>\n");
	len = (log_buf_max_len > 0x1000) ? 0x1000 : log_buf_max_len;
	i = 0;
	while ((log_start_idx != log_end_idx) && i < len) {
		ptr += sprintf(ptr, "%c", LOG_BUF(log_start_idx));
		log_start_idx++;
		i++;
	}
	ptr += sprintf(ptr, "<<md32 log buf end>>\n");

	pr_debug("md32_aee_dump end\n");

	return ptr - buf;
}

void md32_prepare_aed(char *aed_str, struct md32_aed_cfg *aed)
{
	char *detail;
	u8 *log, *phy, *ptr;
	u32 log_size, phy_size;

	pr_debug("md32_prepare_aed\n");

	detail = kmalloc(MD32_AED_STR_LEN, GFP_KERNEL);

	ptr = detail;
	detail[MD32_AED_STR_LEN - 1] = '\0';
	ptr += snprintf(detail, MD32_AED_STR_LEN, "%s", aed_str);
	ptr += sprintf(ptr, " md32 pc=0x%08x, r14=0x%08x, r15=0x%08x\n",
		       md32_aee_status.pc, md32_aee_status.r14,
		       md32_aee_status.r15);

	log_size = 0x4000; /* 16KB */
	log = kmalloc(log_size, GFP_KERNEL);
	if (!log) {
		log_size = 0;
	} else {
		int size;

		memset(log, 0, log_size);

		ptr = log;

		ptr += md32_aee_dump(ptr);

		/* print log in kernel */
		pr_debug("%s", log);

		ptr += sprintf(ptr, "dump memory info\n");
		ptr += sprintf(ptr, "md32 cfgreg: 0x%08x\n", 0);
		ptr += sprintf(ptr, "md32 ptcm  : 0x%08x\n", MD32_CFGREG_SIZE);
		ptr += sprintf(ptr, "md32 dtcm  : 0x%08x\n", MD32_CFGREG_SIZE +
				MD32_PTCM_SIZE);
		ptr += sprintf(ptr, "<<md32 log buf>>\n");
		size = log_size - (ptr - log);
		ptr += md32_get_log_buf(ptr, size);
		*ptr = '\0';
		log_size = ptr - log;
	}

	phy_size = MD32_AED_PHY_SIZE;
	phy = kmalloc(phy_size, GFP_KERNEL);
	if (!phy) {
		pr_err("ap allocate phy buffer fail, size=0x%x\n", phy_size);
		phy_size = 0;
	} else {
		ptr = phy;

		memcpy_from_md32((void *)ptr,
				 (void *)MD32_BASE, MD32_CFGREG_SIZE);
		ptr += MD32_CFGREG_SIZE;

		memcpy_from_md32((void *)ptr,
				 (void *)MD32_PTCM, MD32_PTCM_SIZE);
		ptr += MD32_PTCM_SIZE;

		memcpy_from_md32((void *)ptr,
				 (void *)MD32_DTCM, MD32_DTCM_SIZE);
		ptr += MD32_DTCM_SIZE;
	}

	aed->log = (int *)log;
	aed->log_size = log_size;
	aed->phy = (int *)phy;
	aed->phy_size = phy_size;
	aed->detail = detail;

	pr_debug("md32_prepare_aed end\n");
}

void md32_dmem_abort_handler(void)
{
	pr_err("[MD32 EXCEP] DMEM Abort\n");
	md32_dump_regs();
}

void md32_pmem_abort_handler(void)
{
	pr_err("[MD32 EXCEP] PMEM Abort\n");
	md32_dump_regs();
}

void md32_wdt_handler(void)
{
	pr_err("[MD32 EXCEP] WDT\n");
	md32_dump_regs();
}

irqreturn_t md32_irq_handler(int irq, void *dev_id)
{
	struct reg_md32_to_host_ipc *md32_irq;
	int reboot = 0;

	md32_irq = (struct reg_md32_to_host_ipc *)MD32_TO_HOST_ADDR;

	if (md32_irq->wdt_int) {
		md32_wdt_handler();
		md32_aee_stop();
		md32_aee_status.m2h_irq = readl(MD32_TO_HOST_REG);
		md32_irq->wdt_int = 0;
		reboot = 1;
	}

	if (md32_irq->pmem_disp_int) {
		md32_pmem_abort_handler();
		md32_aee_stop();
		md32_aee_status.m2h_irq = readl(MD32_TO_HOST_REG);
		md32_irq->pmem_disp_int = 0;
		reboot = 1;
	}

	if (md32_irq->dmem_disp_int) {

		md32_dmem_abort_handler();
		md32_aee_stop();
		md32_aee_status.m2h_irq = readl(MD32_TO_HOST_REG);
		md32_irq->dmem_disp_int = 0;
		reboot = 1;
	}

	if (md32_irq->md32_ipc_int) {
		md32_ipi_handler();
		md32_irq->ipc_md2host = 0;
		md32_irq->md32_ipc_int = 0;
	}

	writel(0x0, MD32_TO_HOST_REG);

	if (reboot) {
		queue_work(wq_md32_reboot,
			   (struct work_struct *)&work_md32_reboot);
	}

	return IRQ_HANDLED;
}

void md32_reboot_from_irq(struct work_struct *ws)
{
	struct md32_reboot_work *rb_ws = (struct md32_reboot_work *)ws;
	struct md32_aed_cfg *aed = &rb_ws->aed;
	struct reg_md32_to_host_ipc *md32_irq = (struct reg_md32_to_host_ipc *)
						&md32_aee_status.m2h_irq;

	if (md32_irq->wdt_int)
		md32_prepare_aed("md32 wdt", aed);
	else if (md32_irq->pmem_disp_int)
		md32_prepare_aed("md32 pmem_abort", aed);
	else if (md32_irq->dmem_disp_int)
		md32_prepare_aed("md32 dmem_abort", aed);
	else
		md32_prepare_aed("md32 exception", aed);

	pr_err("%s", aed->detail);

	if (!md32_irq->dmem_disp_int && !md32_irq->pmem_disp_int) {
		aed_md32_exception_api(aed->log, aed->log_size, aed->phy, aed->phy_size,
					   aed->detail, DB_OPT_DEFAULT);
	}

	kfree(aed->detail);
	kfree(aed->phy);
	kfree(aed->log);

	pr_err("[MD32] md32 exception dump is done\n");

	/*For reboot MD32 when receiver MD32 bin exception interrupt*/
	pr_err("[MD32] Reload MD32 bin from IRQ\n");
	reboot_load_md32();

	pr_err("[MD32] Reload MD32 bin from IRQ done\n");
}

void md32_irq_init(void)
{
	writel(0x0, MD32_TO_HOST_REG); /* clear md32 irq */

	wq_md32_reboot = create_workqueue("MD32_REBOOT_WQ");

	INIT_WORK((struct work_struct *)&work_md32_reboot,
		  md32_reboot_from_irq);
}
