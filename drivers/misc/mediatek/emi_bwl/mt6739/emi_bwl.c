/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#define MET_USER_EVENT_SUPPORT
/* #include <linux/met_drv.h> */

#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>

#include <ext_wd_drv.h>
#include "emi_bwl.h"
#include "emi_mbw.h"
#include "emi_elm.h"

DEFINE_SEMAPHORE(emi_bwl_sem);

static void __iomem *CEN_EMI_BASE;
static void __iomem *CHA_EMI_BASE;
static void __iomem *EMI_MPU_BASE;
static unsigned int mpu_irq;
static unsigned int cgm_irq;
static unsigned int elm_irq;

static struct emi_info_t emi_info;

static int emi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	pr_debug("[EMI] module probe.\n");

	if (node) {
		mpu_irq = irq_of_parse_and_map(node, 0);
		cgm_irq = irq_of_parse_and_map(node, 1);
		elm_irq = irq_of_parse_and_map(node, 2);
		pr_info("[EMI] get irq of MPU(%d), GCM(%d), ELM(%d)\n",
			mpu_irq, cgm_irq, elm_irq);
	} else {
		mpu_irq = 0;
		cgm_irq = 0;
		elm_irq = 0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	CEN_EMI_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(CEN_EMI_BASE)) {
		pr_info("[EMI] unable to map CEN_EMI_BASE\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	CHA_EMI_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(CHA_EMI_BASE)) {
		pr_info("[EMI] unable to map CHA_EMI_BASE\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	EMI_MPU_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(EMI_MPU_BASE)) {
		pr_info("[EMI] unable to map EMI_MPU_BASE\n");
		return -EINVAL;
	}

	pr_info("[EMI] get CEN_EMI_BASE @ %p\n", mt_cen_emi_base_get());
	pr_info("[EMI] get CHA_EMI_BASE @ %p\n", mt_chn_emi_base_get());
	pr_info("[EMI] get EMI_MPU_BASE @ %p\n", mt_emi_mpu_base_get());

	ret = mtk_mem_bw_ctrl(CON_SCE_UI, ENABLE_CON_SCE);
	if (ret)
		pr_info("[EMI/BWL] fail to set EMI bandwidth limiter\n");

	writel(0x00000040, CHA_EMI_BASE+0x0008);
	mt_reg_sync_writel(0x00000913, CEN_EMI_BASE+0x5B0);
#if ENABLE_MBW
	mbw_init();
#endif

#if ENABLE_ELM
	elm_init(cgm_irq);
#endif

	return 0;
}

static int emi_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id emi_of_ids[] = {
	{.compatible = "mediatek,emi",},
	{}
};
#endif

#ifdef CONFIG_PM
static int emi_suspend_noirq(struct device *dev)
{
	/* pr_info("[EMI] suspend\n"); */
	suspend_elm();

	return 0;
}

static int emi_resume_noirq(struct device *dev)
{
	/* pr_info("[EMI] resume\n"); */
	resume_elm();

	return 0;
}

static const struct dev_pm_ops emi_pm_ops = {
	.suspend_noirq = emi_suspend_noirq,
	.resume_noirq = emi_resume_noirq,
};
#define EMI_PM_OPS	(&emi_pm_ops)
#else
#define EMI_PM_OPS	NULL
#endif

static struct platform_driver emi_ctrl = {
	.probe = emi_probe,
	.remove = emi_remove,
	.driver = {
		.name = "emi_ctrl",
		.owner = THIS_MODULE,
		.pm = EMI_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = emi_of_ids,
#endif
	},
};

/* define EMI bandwiwth limiter control table */
static struct emi_bwl_ctrl ctrl_tbl[NR_CON_SCE];

/* current concurrency scenario */
static int cur_con_sce = 0x0FFFFFFF;

/* define concurrency scenario strings */
static const char * const con_sce_str[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) (#con_sce),
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};

/****************** For LPDDR4-3200******************/

static const unsigned int emi_arba_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) arba,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};
static const unsigned int emi_arbb_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) arbb,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};
static const unsigned int emi_arbc_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) arbc,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};
static const unsigned int emi_arbd_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) arbd,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};
static const unsigned int emi_arbe_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) arbe,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};
static const unsigned int emi_arbf_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) arbf,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};
static const unsigned int emi_arbg_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) arbg,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};
static const unsigned int emi_arbh_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) arbh,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};
static const unsigned int emi_conm_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) conm,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};
static const unsigned int emi_mdct_lpddr4_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) mdct,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
};

/*
 * mtk_mem_bw_ctrl: set EMI bandwidth limiter for memory bandwidth control
 * @sce: concurrency scenario ID
 * @op: either ENABLE_CON_SCE or DISABLE_CON_SCE
 * Return 0 for success; return negative values for failure.
 */
int mtk_mem_bw_ctrl(int sce, int op)
{
	int i, highest;

	if (sce >= NR_CON_SCE)
		return -1;

	if (op != ENABLE_CON_SCE && op != DISABLE_CON_SCE)
		return -1;

	if (in_interrupt())
		return -1;

	down(&emi_bwl_sem);

	if (op == ENABLE_CON_SCE)
		ctrl_tbl[sce].ref_cnt++;

	else if (op == DISABLE_CON_SCE) {
		if (ctrl_tbl[sce].ref_cnt != 0)
			ctrl_tbl[sce].ref_cnt--;
	}

	/* find the scenario with the highest priority */
	highest = -1;
	for (i = 0; i < NR_CON_SCE; i++) {
		if (ctrl_tbl[i].ref_cnt != 0) {
			highest = i;
			break;
		}
	}
	if (highest == -1)
		highest = CON_SCE_UI;

	/* set new EMI bandwidth limiter value */
	if (highest != cur_con_sce) {
		writel(emi_arba_lpddr4_val[highest], EMI_ARBA);
		writel(emi_arbb_lpddr4_val[highest], EMI_ARBB);
		writel(emi_arbc_lpddr4_val[highest], EMI_ARBC);
		writel(emi_arbd_lpddr4_val[highest], EMI_ARBD);
		writel(emi_arbe_lpddr4_val[highest], EMI_ARBE);
		writel(emi_arbf_lpddr4_val[highest], EMI_ARBF);
		writel(emi_arbg_lpddr4_val[highest], EMI_ARBG);
		writel(emi_arbh_lpddr4_val[highest], EMI_ARBH);
		writel(emi_conm_lpddr4_val[highest], EMI_CONM);
		mt_reg_sync_writel(emi_mdct_lpddr4_val[highest], EMI_MDCT);
		cur_con_sce = highest;
	}

	up(&emi_bwl_sem);

	return 0;
}

/*
 * con_sce_show: sysfs con_sce file show function.
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t con_sce_show(struct device_driver *driver, char *buf)
{
	char *ptr = buf;
	int i = 0;

	if (cur_con_sce >= NR_CON_SCE) {
		ptr += sprintf(ptr, "none\n");
		return strlen(buf);
	}
	ptr += snprintf(ptr, 64, "current scenario: %s\n", con_sce_str[cur_con_sce]);

#if 1
	ptr += snprintf(ptr, 32, "%s\n", con_sce_str[cur_con_sce]);
	ptr += sprintf(ptr, "EMI_ARBA = 0x%x\n",  readl(IOMEM(EMI_ARBA)));
	ptr += sprintf(ptr, "EMI_ARBB = 0x%x\n",  readl(IOMEM(EMI_ARBB)));
	ptr += sprintf(ptr, "EMI_ARBC = 0x%x\n",  readl(IOMEM(EMI_ARBC)));
	ptr += sprintf(ptr, "EMI_ARBD = 0x%x\n",  readl(IOMEM(EMI_ARBD)));
	ptr += sprintf(ptr, "EMI_ARBE = 0x%x\n",  readl(IOMEM(EMI_ARBE)));
	ptr += sprintf(ptr, "EMI_ARBF = 0x%x\n",  readl(IOMEM(EMI_ARBF)));
	ptr += sprintf(ptr, "EMI_ARBG = 0x%x\n",  readl(IOMEM(EMI_ARBG)));
	ptr += sprintf(ptr, "EMI_ARBH = 0x%x\n",  readl(IOMEM(EMI_ARBH)));
	ptr += sprintf(ptr, "EMI_CONM = 0x%x\n",  readl(IOMEM(EMI_CONM)));
	ptr += sprintf(ptr, "EMI_MDCT = 0x%x\n",  readl(IOMEM(EMI_MDCT)));
	for (i = 0; i < NR_CON_SCE; i++)
		ptr += snprintf(ptr, 64, "%s = 0x%x\n",
			con_sce_str[i], ctrl_tbl[i].ref_cnt);

	pr_debug("[EMI BWL] EMI_ARBA = 0x%x\n", readl(IOMEM(EMI_ARBA)));
	pr_debug("[EMI BWL] EMI_ARBB = 0x%x\n", readl(IOMEM(EMI_ARBB)));
	pr_debug("[EMI BWL] EMI_ARBC = 0x%x\n", readl(IOMEM(EMI_ARBC)));
	pr_debug("[EMI BWL] EMI_ARBD = 0x%x\n", readl(IOMEM(EMI_ARBD)));
	pr_debug("[EMI BWL] EMI_ARBE = 0x%x\n", readl(IOMEM(EMI_ARBE)));
	pr_debug("[EMI BWL] EMI_ARBF = 0x%x\n", readl(IOMEM(EMI_ARBF)));
	pr_debug("[EMI BWL] EMI_ARBG = 0x%x\n", readl(IOMEM(EMI_ARBG)));
	pr_debug("[EMI BWL] EMI_ARBH = 0x%x\n", readl(IOMEM(EMI_ARBH)));
	pr_debug("[EMI BWL] EMI_CONM = 0x%x\n", readl(IOMEM(EMI_CONM)));
	pr_debug("[EMI BWL] EMI_MDCT = 0x%x\n", readl(IOMEM(EMI_MDCT)));
#endif

	return strlen(buf);

}

/*
 * con_sce_store: sysfs con_sce file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t con_sce_store(struct device_driver *driver,
const char *buf, size_t count)
{
	int i;

	for (i = 0; i < NR_CON_SCE; i++) {
		if (!strncmp(buf, con_sce_str[i], strlen(con_sce_str[i]))) {
			if (!strncmp(buf + strlen(con_sce_str[i]) + 1,
				EN_CON_SCE_STR, strlen(EN_CON_SCE_STR))) {

				mtk_mem_bw_ctrl(i, ENABLE_CON_SCE);
				/* pr_debug("concurrency scenario %s ON\n", con_sce_str[i]); */
				break;
			} else if (!strncmp(buf + strlen(con_sce_str[i]) + 1,
				DIS_CON_SCE_STR, strlen(DIS_CON_SCE_STR))) {

				mtk_mem_bw_ctrl(i, DISABLE_CON_SCE);
				/* pr_debug("concurrency scenario %s OFF\n", con_sce_str[i]); */
				break;
			}
		}
	}

	return count;
}

DRIVER_ATTR(concurrency_scenario, 0644, con_sce_show, con_sce_store);

static ssize_t elm_ctrl_show(struct device_driver *driver, char *buf)
{
	char *ptr;

	ptr = (char *)buf;
	ptr += sprintf(ptr, "ELM enabled: %d\n", is_elm_enabled());

	return strlen(buf);
}

static ssize_t elm_ctrl_store(struct device_driver *driver,
	const char *buf, size_t count)
{
	if (!strncmp(buf, "ON", strlen("ON")))
		enable_elm();
	else if (!strncmp(buf, "OFF", strlen("OFF")))
		disable_elm();

	return count;
}

DRIVER_ATTR(elm_ctrl, 0644, elm_ctrl_show, elm_ctrl_store);

/*
 * emi_ctrl_init: module init function.
 */
static int __init emi_ctrl_init(void)
{
	int ret;
	int i;

	/* register EMI ctrl interface */
	ret = platform_driver_register(&emi_ctrl);
	if (ret)
		pr_info("[EMI/BWL] fail to register emi_ctrl driver\n");

	ret = driver_create_file(&emi_ctrl.driver, &driver_attr_concurrency_scenario);
	if (ret)
		pr_info("[EMI/BWL] fail to create emi_bwl sysfs file\n");

	ret = driver_create_file(&emi_ctrl.driver, &driver_attr_elm_ctrl);
	if (ret)
		pr_info("[EMI/ELM] fail to create elm_ctrl file\n");

	/* get EMI info from boot tags */
	if (of_chosen) {
		ret = of_property_read_u32(of_chosen, "emi_info,dram_type", &(emi_info.dram_type));
		if (ret)
			pr_info("[EMI] fail to get dram_type\n");
		ret = of_property_read_u32(of_chosen, "emi_info,ch_num", &(emi_info.ch_num));
		if (ret)
			pr_info("[EMI] fail to get ch_num\n");
		ret = of_property_read_u32(of_chosen, "emi_info,rk_num", &(emi_info.rk_num));
		if (ret)
			pr_info("[EMI] fail to get rk_num\n");
		ret = of_property_read_u32_array(of_chosen, "emi_info,rank_size",
			emi_info.rank_size, MAX_RK);
		if (ret)
			pr_info("[EMI] fail to get rank_size\n");
	}

	pr_info("[EMI] dram_type(%d)\n", get_dram_type());
	pr_info("[EMI] ch_num(%d)\n", get_ch_num());
	pr_info("[EMI] rk_num(%d)\n", get_rk_num());
	for (i = 0; i < get_rk_num(); i++)
		pr_info("[EMI] rank%d_size(0x%x)", i, get_rank_size(i));

	return 0;
}

/*
 * emi_ctrl_exit: module exit function.
 */
static void __exit emi_ctrl_exit(void)
{
}


postcore_initcall(emi_ctrl_init);
module_exit(emi_ctrl_exit);

unsigned int get_ch_num(void)
{
	return emi_info.ch_num;
}

unsigned int get_rk_num(void)
{
	if (emi_info.rk_num > MAX_RK)
		pr_info("[EMI] rank overflow\n");

	return emi_info.rk_num;
}

unsigned int get_rank_size(unsigned int rank_index)
{
	if (rank_index < emi_info.rk_num)
		return emi_info.rank_size[rank_index];

	return 0;
}

unsigned int get_dram_type(void)
{
	return emi_info.dram_type;
}
EXPORT_SYMBOL(get_dram_type);

void __iomem *mt_cen_emi_base_get(void)
{
	return CEN_EMI_BASE;
}
EXPORT_SYMBOL(mt_cen_emi_base_get);

void __iomem *mt_emi_base_get(void)
{
	return mt_cen_emi_base_get();
}
EXPORT_SYMBOL(mt_emi_base_get);

void __iomem *mt_chn_emi_base_get()
{
	return CHA_EMI_BASE;
}
EXPORT_SYMBOL(mt_chn_emi_base_get);

void __iomem *mt_emi_mpu_base_get(void)
{
	return EMI_MPU_BASE;
}
EXPORT_SYMBOL(mt_emi_mpu_base_get);

unsigned int mt_emi_mpu_irq_get(void)
{
	return mpu_irq;
}

unsigned int mt_emi_elm_irq_get(void)
{
	return elm_irq;
}

unsigned int mt_emi_cgm_irq_get(void)
{
	return cgm_irq;
}
