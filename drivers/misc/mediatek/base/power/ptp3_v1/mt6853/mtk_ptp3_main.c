// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


/**
 * @file	mkt_ptp3_main.c
 * @brief   Driver for ptp3
 *
 */

#define __MTK_PTP3_C__


/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#ifdef __KERNEL__
	#include <linux/topology.h>
	#include <mt-plat/mtk_chip.h>
	#include <mt-plat/mtk_devinfo.h>
	#include <mt-plat/mtk_secure_api.h>
	#include "mtk_devinfo.h"
	#include "mtk_ptp3_common.h"
	#include "mtk_ptp3_fll.h"
	#include "mtk_ptp3_cinst.h"
	#include "mtk_ptp3_drcc.h"
#endif

#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
#include <mcupm_ipi_id.h>
#include <mcupm_driver.h>
#endif

#ifdef CONFIG_OF
	#include <linux/cpu.h>
	#include <linux/cpu_pm.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
#endif

#ifdef CONFIG_OF_RESERVED_MEM
	#include <of_reserved_mem.h>
#endif

/************************************************
 * Debug print
 ************************************************/

#define PTP3_DEBUG
#define PTP3_TAG	 "[PTP3]"

#define ptp3_err(fmt, args...)	\
	pr_info(PTP3_TAG"[ERROR][%s():%d]" fmt, __func__, __LINE__, ##args)
#define ptp3_msg(fmt, args...)	\
	pr_info(PTP3_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#ifdef PTP3_DEBUG
#define ptp3_debug(fmt, args...)	\
	pr_debug(PTP3_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define ptp3_debug(fmt, args...)
#endif

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

#define ptp3_read(addr)		__raw_readl((void __iomem *)(addr))
#define ptp3_write(addr, val)	mt_reg_sync_writel(val, addr)
#define EEM_TEMPSPARE0		0x11278F20
#define PTP3_MEM_SIZE 0x80000
#define PTP3_FLL_MEM_OFFSET 0x0
#define PTP3_CINST_MEM_OFFSET 0x10000
#define PTP3_DRCC_MEM_OFFSET 0x20000

static unsigned long long ptp3_reserve_memory_init(void)
{
	/* GAT log use */
	phys_addr_t ptp3_mem_base_phys = ptp3_read(ioremap(EEM_TEMPSPARE0, 0));
	phys_addr_t ptp3_mem_size = PTP3_MEM_SIZE;
	phys_addr_t ptp3_mem_base_virt = 0;

	if ((char *)ptp3_mem_base_phys != NULL) {
		ptp3_mem_base_virt =
			(phys_addr_t)(uintptr_t)ioremap_wc(
			ptp3_mem_base_phys,
			ptp3_mem_size);
	}

	ptp3_msg("[PTP3] phys:0x%llx, size:0x%llx, virt:0x%llx\n",
		(unsigned long long)ptp3_mem_base_phys,
		(unsigned long long)ptp3_mem_size,
		(unsigned long long)ptp3_mem_base_virt);

	return (unsigned long long)ptp3_mem_base_virt;

}

#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */

/************************************************
 * IPI between kernel and mcupm/cputoeb
 ************************************************/
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
static DEFINE_MUTEX(ptp3_ipi_mutex);
unsigned int ptp3_ipi_handle(struct ptp3_ipi_data *ptp3_data)
{
	int ret;

	mutex_lock(&ptp3_ipi_mutex);

	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_PLATFORM,
		IPI_SEND_WAIT,
		ptp3_data,
		sizeof(struct ptp3_ipi_data) / PTP3_SLOT_NUM,
		2000);

	if (ret != 0)
		ptp3_err("send init cmd(%d) error ret:%d\n",
			ptp3_data->cmd, ret);

	mutex_unlock(&ptp3_ipi_mutex);
	return ret;
}
#else
unsigned int ptp3_ipi_handle(struct ptp3_ipi_data *ptp3_data)
{
	ptp3_msg("IPI from kernel to MCUPM not exist\n");

	return 0;
}
#endif

/************************************************
 * Kernel driver nodes
 ************************************************/
static int create_procfs(void)
{
	const char *proc_name = "ptp3";
	struct proc_dir_entry *dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	/* create proc dir */
	dir = proc_mkdir(proc_name, NULL);
	if (!dir) {
		ptp3_err(
			"[%s]: mkdir /proc/%s failed\n", __func__, proc_name);
		return -1;
	}

	fll_create_procfs(proc_name, dir);
	cinst_create_procfs(proc_name, dir);
	drcc_create_procfs(proc_name, dir);
	return 0;
}

static int ptp3_probe(struct platform_device *pdev)
{
	/* GAT log use */
	unsigned long long ptp3_mem_size = PTP3_MEM_SIZE;
	unsigned long long ptp3_mem_base_virt;

	/* init for DRAM memory request */
	ptp3_mem_base_virt = ptp3_reserve_memory_init();
	if ((char *)ptp3_mem_base_virt != NULL) {
		/* FLL: save register status for reserved memory */
		fll_save_memory_info(
			(char *)(uintptr_t)
			(ptp3_mem_base_virt+PTP3_FLL_MEM_OFFSET),
			ptp3_mem_size);
		/* CINST: save register status for reserved memory */
		cinst_save_memory_info(
			(char *)(uintptr_t)
			(ptp3_mem_base_virt+PTP3_CINST_MEM_OFFSET),
			ptp3_mem_size);
		/* DRCC: save register status for reserved memory */
		drcc_save_memory_info(
			(char *)(uintptr_t)
			(ptp3_mem_base_virt+PTP3_DRCC_MEM_OFFSET),
			ptp3_mem_size);
	} else
		ptp3_err("ptp3_mem_base_virt is null !\n");

	/* probe trigger for ptp3 features */
	fll_probe(pdev);
	cinst_probe(pdev);
	drcc_probe(pdev);

	return 0;
}

static int ptp3_suspend(struct platform_device *pdev, pm_message_t state)
{
	fll_suspend(pdev, state);
	cinst_suspend(pdev, state);
	return 0;
}

static int ptp3_resume(struct platform_device *pdev)
{
	fll_resume(pdev);
	cinst_resume(pdev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_ptp3_of_match[] = {
	{ .compatible = "mediatek,ptp3", },
	{},
};
#endif

static struct platform_driver ptp3_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= ptp3_probe,
	.suspend	= ptp3_suspend,
	.resume		= ptp3_resume,
	.driver		= {
		.name   = "mt-ptp3",
#ifdef CONFIG_OF
		.of_match_table = mt_ptp3_of_match,
#endif
	},
};

static int __init __ptp3_init(void)
{
	int err = 0;

	create_procfs();

	err = platform_driver_register(&ptp3_driver);
	if (err) {
		ptp3_err("PTP3 driver callback register failed..\n");
		return err;
	}

	return 0;
}

static void __exit __ptp3_exit(void)
{
	ptp3_msg("ptp3 de-initialization\n");
}


module_init(__ptp3_init);
module_exit(__ptp3_exit);

MODULE_DESCRIPTION("MediaTek PTP3 Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_PTP3_C__
