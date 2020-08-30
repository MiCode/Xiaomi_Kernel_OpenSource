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

/**
 * @file	mkt_ptp3_main.c
 * @brief   Driver for ptp3
 *
 */

#define __MTK_PTP3_C__

/*
 *=============================================================
 * Include files
 *=============================================================
 */

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
	#include "mtk_ptp3_brisket2.h"
	#include "mtk_ptp3_ctt.h"
	#include "mtk_ptp3_pdp.h"
	#include "mtk_ptp3_dt.h"
	#include "mtk_ptp3_adcc.h"
	#include "mtk_ptp3_iglre.h"
#ifdef PICACHU_READY
	#include "mtk_picachu.h"
#endif
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

// #define PTP3_DEBUG
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
#define PTP3_MEM_SIZE 0x40000
#define PTP3_BRISKET2_MEM_OFFSET (0x4000 * 0)
#define PTP3_FLL_MEM_OFFSET (0x4000 * 1)
#define PTP3_CINST_MEM_OFFSET (0x4000 * 2)
#define PTP3_DRCC_MEM_OFFSET (0x4000 * 3)
#define PTP3_CTT_MEM_OFFSET (0x4000 * 4)
#define PTP3_PDP_MEM_OFFSET (0x4000 * 5)
#define PTP3_DT_MEM_OFFSET  (0x4000 * 6)
#define PTP3_ADCC_MEM_OFFSET (0x4000 * 7)
#define PTP3_IGLRE_MEM_OFFSET (0x4000 * 8)

static unsigned long long ptp3_reserve_memory_init(void)
{
#ifdef PICACHU_READY
	return picachu_reserve_mem_get_virt(0); /* picachu_log_buffer_id_PTP3 */
#else
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

	ptp3_msg("phys:0x%llx, size:0x%llx, virt:0x%llx\n",
		(unsigned long long)ptp3_mem_base_phys,
		(unsigned long long)ptp3_mem_size,
		(unsigned long long)ptp3_mem_base_virt);

	return ptp3_mem_base_virt;
#endif
}

#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */

/************************************************
 * SMC between kernel and atf
 ************************************************/
unsigned int ptp3_smc_handle(
	unsigned int feature, unsigned int x2, unsigned int x3, unsigned int x4)
{
	unsigned int ret;

	/* update atf via smc */
	ret = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
		feature,
		x2,
		x3,
		x4);

	return ret;
}

/************************************************
 * IPI between kernel and mcupm/cpu_eb
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

	brisket2_create_procfs(proc_name, dir);
	fll_create_procfs(proc_name, dir);
	cinst_create_procfs(proc_name, dir);
	drcc_create_procfs(proc_name, dir);
	ctt_create_procfs(proc_name, dir);
	pdp_create_procfs(proc_name, dir);
	dt_create_procfs(proc_name, dir);
	adcc_create_procfs(proc_name, dir);
	iglre_create_procfs(proc_name, dir);
	return 0;
}

static int ptp3_probe(struct platform_device *pdev)
{

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	/* GAT log use */
	unsigned long long ptp3_mem_size = PTP3_MEM_SIZE;
	unsigned long long ptp3_mem_base_virt;

	/* init for DRAM memory request */
	ptp3_mem_base_virt = ptp3_reserve_memory_init();

	if ((char *)ptp3_mem_base_virt != NULL) {
		/* BRISKET2: save register status for reserved memory */
		brisket2_save_memory_info(
			(char *)(uintptr_t)
			(ptp3_mem_base_virt+PTP3_BRISKET2_MEM_OFFSET),
			ptp3_mem_size);
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
		/* CTT: save register status for reserved memory */
		ctt_save_memory_info(
			(char *)(uintptr_t)
			(ptp3_mem_base_virt+PTP3_CTT_MEM_OFFSET),
			ptp3_mem_size);
		/* PDP: save register status for reserved memory */
		pdp_save_memory_info(
			(char *)(uintptr_t)
			(ptp3_mem_base_virt+PTP3_PDP_MEM_OFFSET),
			ptp3_mem_size);
		/* DT: save register status for reserved memory */
		dt_save_memory_info(
			(char *)(uintptr_t)
			(ptp3_mem_base_virt+PTP3_DT_MEM_OFFSET),
			ptp3_mem_size);
		/* ADCC: save register status for reserved memory */
		adcc_save_memory_info(
			(char *)(uintptr_t)
			(ptp3_mem_base_virt+PTP3_ADCC_MEM_OFFSET),
			ptp3_mem_size);
		/* IGLRE: save register status for reserved memory */
		iglre_save_memory_info(
			(char *)(uintptr_t)
			(ptp3_mem_base_virt+PTP3_IGLRE_MEM_OFFSET),
			ptp3_mem_size);
	} else
		ptp3_err("ptp3_mem_base_virt is NULL\n");
#endif /* CONFIG_OF_RESERVED_MEM */
#endif /* CONFIG_FPGA_EARLY_PORTING */

	/* probe trigger for ptp3 features */
	adcc_probe(pdev);
	fll_probe(pdev);
	ctt_probe(pdev);
	drcc_probe(pdev);
	brisket2_probe(pdev);
	cinst_probe(pdev);
	pdp_probe(pdev);
	dt_probe(pdev);
	iglre_probe(pdev);

	return 0;
}

static int ptp3_suspend(struct platform_device *pdev, pm_message_t state)
{
	adcc_suspend(pdev, state);
	fll_suspend(pdev, state);
	ctt_suspend(pdev, state);
	drcc_suspend(pdev, state);
	brisket2_suspend(pdev, state);
	cinst_suspend(pdev, state);
	pdp_suspend(pdev, state);
	dt_suspend(pdev, state);
	iglre_suspend(pdev, state);
	return 0;
}

static int ptp3_resume(struct platform_device *pdev)
{
	adcc_resume(pdev);
	fll_resume(pdev);
	ctt_resume(pdev);
	drcc_resume(pdev);
	brisket2_resume(pdev);
	cinst_resume(pdev);
	pdp_resume(pdev);
	dt_resume(pdev);
	iglre_resume(pdev);
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
