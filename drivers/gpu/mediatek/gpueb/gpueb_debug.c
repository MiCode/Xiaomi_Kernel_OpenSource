// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpueb_debug.c
 * @brief   Debug mechanism for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "gpueb_helper.h"
#include "gpueb_debug.h"

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
enum gpueb_smc_op {
	GPUEB_SMC_OP_TRIGGER_WDT            = 0,
	GPUEB_SMC_OP_SET_RGX_BUS_SECURE     = 1,
	GPUEB_SMC_OP_NUMBER                 = 2,
};

static void __iomem *g_gpueb_base;
static void __iomem *g_gpueb_mbox_ipi;
static void __iomem *g_gpueb_mbox_sw_int;

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
void gpueb_dump_status(void)
{
	gpueb_pr_info("@%s: == [GPUEB STATUS] ==\n", __func__);

	if (g_gpueb_base) {
		/* 0x13C60300 */
		gpueb_pr_info("@%s: GPUEB_INTC_IRQ_EN (0x%x): 0x%08x\n", __func__,
			(0x13C40000 + 0x20300), readl(g_gpueb_base + 0x20300));
		/* 0x13C60338 */
		gpueb_pr_info("@%s: GPUEB_INTC_IRQ_STA (0x%x): 0x%08x\n", __func__,
			(0x13C40000 + 0x20338), readl(g_gpueb_base + 0x20338));
		/* 0x13C6033C */
		gpueb_pr_info("@%s: GPUEB_INTC_IRQ_RAW_STA (0x%x): 0x%08x\n", __func__,
			(0x13C40000 + 0x2033C), readl(g_gpueb_base + 0x2033C));
		/* 0x13C60610 */
		gpueb_pr_info("@%s: GPUEB_CFGREG_AXI_STA (0x%x): 0x%08x\n", __func__,
			(0x13C40000 + 0x20610), readl(g_gpueb_base + 0x20610));
		/* 0x13C60618 */
		gpueb_pr_info("@%s: GPUEB_CFGREG_WDT_CON (0x%x): 0x%08x\n", __func__,
			(0x13C40000 + 0x20618), readl(g_gpueb_base + 0x20618));
		/* 0x13C6061C */
		gpueb_pr_info("@%s: GPUEB_CFGREG_WDT_KICK (0x%x): 0x%08x\n", __func__,
			(0x13C40000 + 0x2061C), readl(g_gpueb_base + 0x2061C));
		/* 0x13C60634 */
		gpueb_pr_info("@%s: GPUEB_CFGREG_MDSP_CFG (0x%x): 0x%08x\n", __func__,
			(0x13C40000 + 0x20634), readl(g_gpueb_base + 0x20634));
		/* 0x13C607E8 */
		gpueb_pr_info("@%s: GPUEB_CFGREG_SRAMRC_MASTER_CFG (0x%x): 0x%08x\n", __func__,
			(0x13C40000 + 0x207E8), readl(g_gpueb_base + 0x207E8));
		/* 0x13C5FD30, 0x13C5FD60 */
		gpueb_pr_info("@%s: GPUEB LPM footprint: 0x%08x, Gpufreq footprint: 0x%08x\n",
			__func__,
			readl(g_gpueb_base + 0x1FD30), readl(g_gpueb_base + 0x1FD60));
	} else
		gpueb_pr_info("@%s: skip null g_gpueb_base\n", __func__);

	if (g_gpueb_mbox_ipi)
		/* 0x13C62000 */
		gpueb_pr_info("@%s: GPUEB_MBOX_IPI_GPUEB (0x%x): 0x%08x\n", __func__,
			(0x13C62000), readl(g_gpueb_mbox_ipi));
	else
		gpueb_pr_info("@%s: skip null g_gpueb_mbox_ipi\n", __func__);

	if (g_gpueb_mbox_sw_int)
		/* 0x13C62078 */
		gpueb_pr_info("@%s: GPUEB_MBOX_SW_INT_STA (0x%x): 0x%08x\n", __func__,
			(0x13C62078), readl(g_gpueb_mbox_sw_int));
	else
		gpueb_pr_info("@%s: skip null g_gpueb_mbox_sw_int\n", __func__);
}
EXPORT_SYMBOL(gpueb_dump_status);

void gpueb_trigger_wdt(const char *name)
{
	struct arm_smccc_res res;

	gpueb_pr_info("@%s: GPUEB WDT is triggered by %s\n", __func__, name);

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
		GPUEB_SMC_OP_TRIGGER_WDT,      /* a1 */
		0, 0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(gpueb_trigger_wdt);

void gpu_set_rgx_bus_secure(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,    /* a0 */
		GPUEB_SMC_OP_SET_RGX_BUS_SECURE, /* a1 */
		0, 0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(gpu_set_rgx_bus_secure);

#if defined(CONFIG_PROC_FS)
/* PROCFS: show current gpueb status */
static int gpueb_status_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "[GPUEB-DEBUG] Current Status of GPUEB\n");
	if (g_gpueb_base) {
		/* 0x13C60300 */
		seq_printf(m, "GPUEB_INTC_IRQ_EN (0x%x): 0x%08x\n",
			(0x13C40000 + 0x20300), readl(g_gpueb_base + 0x20300));
		/* 0x13C60338 */
		seq_printf(m, "GPUEB_INTC_IRQ_STA (0x%x): 0x%08x\n",
			(0x13C40000 + 0x20338), readl(g_gpueb_base + 0x20338));
		/* 0x13C6033C */
		seq_printf(m, "GPUEB_INTC_IRQ_RAW_STA (0x%x): 0x%08x\n",
			(0x13C40000 + 0x2033C), readl(g_gpueb_base + 0x2033C));
		/* 0x13C60610 */
		seq_printf(m, "GPUEB_CFGREG_AXI_STA (0x%x): 0x%08x\n",
			(0x13C40000 + 0x20610), readl(g_gpueb_base + 0x20610));
		/* 0x13C60618 */
		seq_printf(m, "GPUEB_CFGREG_WDT_CON (0x%x): 0x%08x\n",
			(0x13C40000 + 0x20618), readl(g_gpueb_base + 0x20618));
		/* 0x13C6061C */
		seq_printf(m, "GPUEB_CFGREG_WDT_KICK (0x%x): 0x%08x\n",
			(0x13C40000 + 0x2061C), readl(g_gpueb_base + 0x2061C));
		/* 0x13C60634 */
		seq_printf(m, "GPUEB_CFGREG_MDSP_CFG (0x%x): 0x%08x\n",
			(0x13C40000 + 0x20634), readl(g_gpueb_base + 0x20634));
	}
	if (g_gpueb_mbox_ipi)
		/* 0x13C62000 */
		seq_printf(m, "GPUEB_MBOX_IPI_GPUEB (0x%x): 0x%08x\n",
			(0x13C62000), readl(g_gpueb_mbox_ipi));
	if (g_gpueb_mbox_sw_int)
		/* 0x13C62078 */
		seq_printf(m, "GPUEB_MBOX_SW_INT_STA (0x%x): 0x%08x\n",
			(0x13C62078), readl(g_gpueb_mbox_sw_int));

	return 0;
}

/* PROCFS: trigger GPUEB WDT */
static int force_trigger_wdt_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "[GPUEB-DEBUG] Force trigger GPUEB WDT\n");
	return 0;
}

static ssize_t force_trigger_wdt_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = 0;
	char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = -1;
		goto done;
	}
	buf[len] = '\0';

	if (sysfs_streq(buf, WDT_EXCEPTION_EN))
		gpueb_trigger_wdt("GPUEB_DEBUG");

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS : initialization */
PROC_FOPS_RO(gpueb_status);
PROC_FOPS_RW(force_trigger_wdt);

static int gpueb_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry default_entries[] = {
		PROC_ENTRY(gpueb_status),
		PROC_ENTRY(force_trigger_wdt),
	};

	dir = proc_mkdir("gpueb", NULL);
	if (!dir) {
		gpueb_pr_info("@%s: fail to create /proc/gpueb (ENOMEM)\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(default_entries); i++) {
		if (!proc_create(default_entries[i].name, 0660,
			dir, default_entries[i].fops))
			gpueb_pr_info("@%s: fail to create /proc/gpueb/%s\n",
				__func__, default_entries[i].name);
	}

	return 0;
}
#endif /* CONFIG_PROC_FS */

void gpueb_debug_init(struct platform_device *pdev)
{
	int ret = 0;
	struct device *gpueb_dev = &pdev->dev;
	struct resource *res = NULL;

#if defined(CONFIG_PROC_FS)
	ret = gpueb_create_procfs();
	if (ret)
		gpueb_pr_info("@%s: fail to create procfs (%d)\n", __func__, ret);
#endif /* CONFIG_PROC_FS */

	if (unlikely(!gpueb_dev)) {
		gpueb_pr_info("@%s: fail to find gpueb device\n", __func__);
		return;
	}

	/* 0x13C40000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_base");
	if (unlikely(!res)) {
		gpueb_pr_info("@%s: fail to get resource GPUEB_BASE\n", __func__);
		return;
	}
	g_gpueb_base = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_base)) {
		gpueb_pr_info("@%s: fail to ioremap GPUEB_BASE: 0x%llx\n", __func__, res->start);
		return;
	}
	/* 0x13C62000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mbox0_send");
	if (unlikely(!res)) {
		gpueb_pr_info("@%s: fail to get resource MBOX0_SEND\n", __func__);
		return;
	}
	g_gpueb_mbox_ipi = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_mbox_ipi)) {
		gpueb_pr_info("@%s: fail to ioremap MBOX0_SEND: 0x%llx\n", __func__, res->start);
		return;
	}
	/* 0x13C62078 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mbox0_recv");
	if (unlikely(!res)) {
		gpueb_pr_info("@%s: fail to get resource MBOX0_RECV\n", __func__);
		return;
	}
	g_gpueb_mbox_sw_int = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_mbox_sw_int)) {
		gpueb_pr_info("@%s: fail to ioremap MBOX0_RECV: 0x%llx\n", __func__, res->start);
		return;
	}
}
