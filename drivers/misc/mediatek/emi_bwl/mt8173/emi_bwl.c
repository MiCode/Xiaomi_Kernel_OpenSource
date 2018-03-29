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
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "mach/emi_bwl.h"
#include <mt-plat/sync_write.h>
#include <mt-plat/mt_io.h>

void __iomem *DRAMC_BASE_ADDR = NULL;
void __iomem *EMI_BASE_ADDR = NULL;


DEFINE_SEMAPHORE(emi_bwl_sem);

static struct platform_driver mem_bw_ctrl = {
	.driver = {
		   .name = "mem_bw_ctrl",
		   .owner = THIS_MODULE,
		   },
};

static struct platform_driver ddr_type = {
	.driver = {
		   .name = "ddr_type",
		   .owner = THIS_MODULE,
		   },
};

/* define EMI bandwiwth limiter control table */
static struct emi_bwl_ctrl ctrl_tbl[NR_CON_SCE];

/* current concurrency scenario */
static int cur_con_sce = 0x0FFFFFFF;

/* define concurrency scenario strings */
static const char * const con_sce_str[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) (#con_sce),
#include "mach/con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

/* define EMI bandwidth allocation tables */
/****************** For 2xLPDDR2-1066 ******************/

static const unsigned int emi_arba_2xlpddr2_1066_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arba,
#include "mach/con_sce_2xlpddr2_1066.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbb_2xlpddr2_1066_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbb,
#include "mach/con_sce_2xlpddr2_1066.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbc_2xlpddr2_1066_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbc,
#include "mach/con_sce_2xlpddr2_1066.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbd_2xlpddr2_1066_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbd,
#include "mach/con_sce_2xlpddr2_1066.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbe_2xlpddr2_1066_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbe,
#include "mach/con_sce_2xlpddr2_1066.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbf_2xlpddr2_1066_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbf,
#include "mach/con_sce_2xlpddr2_1066.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbg_2xlpddr2_1066_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbg,
#include "mach/con_sce_2xlpddr2_1066.h"
#undef X_CON_SCE
};

/****************** For LPDDR3-1866 ******************/

static const unsigned int emi_arba_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arba,
#include "mach/con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbb_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbb,
#include "mach/con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbc_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbc,
#include "mach/con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbd_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbd,
#include "mach/con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbe_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbe,
#include "mach/con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbf_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbf,
#include "mach/con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbg_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbg,
#include "mach/con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};


/****************** For 2 x LPDDR3-1600******************/

static const unsigned int emi_arba_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arba,
#include "mach/con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbb_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbb,
#include "mach/con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbc_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbc,
#include "mach/con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbd_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbd,
#include "mach/con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbe_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbe,
#include "mach/con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbf_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbf,
#include "mach/con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbg_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbg,
#include "mach/con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

int get_dram_type(void)
{
	unsigned int value = readl(IOMEM(DRAMC_BASE_ADDR + DRAMC_LPDDR2));

	if ((value >> 28) & 0x1) {	/* check LPDDR2_EN */
		return LPDDR2;
	}

	value = readl(IOMEM(DRAMC_BASE_ADDR + DRAMC_ACTIM1));
	if ((value >> 28) & 0x1) {	/* check LPDDR3_EN */
		value = readl(IOMEM(EMI_CONA));
		if (value & 0x01)/* support 2 channel */
			return DUAL_LPDDR3_1600;
		else
			return LPDDR3_1866;
	}

	return mDDR;
}
EXPORT_SYMBOL(get_dram_type);

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

	if (op == ENABLE_CON_SCE) {
		ctrl_tbl[sce].ref_cnt++;
	} else if (op == DISABLE_CON_SCE) {
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
		highest = CON_SCE_NORMAL;

	/* set new EMI bandwidth limiter value */
	if (highest != cur_con_sce) {
		if (get_dram_type() == LPDDR2) {
			mt_reg_sync_writel(emi_arba_2xlpddr2_1066_val[highest], EMI_ARBA);
			mt_reg_sync_writel(emi_arbb_2xlpddr2_1066_val[highest], EMI_ARBB);
			mt_reg_sync_writel(emi_arbc_2xlpddr2_1066_val[highest], EMI_ARBC);
			mt_reg_sync_writel(emi_arbd_2xlpddr2_1066_val[highest], EMI_ARBD);
			mt_reg_sync_writel(emi_arbe_2xlpddr2_1066_val[highest], EMI_ARBE);
			mt_reg_sync_writel(emi_arbf_2xlpddr2_1066_val[highest], EMI_ARBF);
			mt_reg_sync_writel(emi_arbg_2xlpddr2_1066_val[highest], EMI_ARBG);
		} else if (get_dram_type() == DUAL_LPDDR3_1600) {
			mt_reg_sync_writel(emi_arba_2xlpddr3_1600_val[highest], EMI_ARBA);
			mt_reg_sync_writel(emi_arbb_2xlpddr3_1600_val[highest], EMI_ARBB);
			mt_reg_sync_writel(emi_arbc_2xlpddr3_1600_val[highest], EMI_ARBC);
			mt_reg_sync_writel(emi_arbd_2xlpddr3_1600_val[highest], EMI_ARBD);
			mt_reg_sync_writel(emi_arbe_2xlpddr3_1600_val[highest], EMI_ARBE);
			mt_reg_sync_writel(emi_arbf_2xlpddr3_1600_val[highest], EMI_ARBF);
			mt_reg_sync_writel(emi_arbg_2xlpddr3_1600_val[highest], EMI_ARBG);
		} else if (get_dram_type() == LPDDR3_1866) {
			mt_reg_sync_writel(emi_arba_lpddr3_1866_val[highest], EMI_ARBA);
			mt_reg_sync_writel(emi_arbb_lpddr3_1866_val[highest], EMI_ARBB);
			mt_reg_sync_writel(emi_arbc_lpddr3_1866_val[highest], EMI_ARBC);
			mt_reg_sync_writel(emi_arbd_lpddr3_1866_val[highest], EMI_ARBD);
			mt_reg_sync_writel(emi_arbe_lpddr3_1866_val[highest], EMI_ARBE);
			mt_reg_sync_writel(emi_arbf_lpddr3_1866_val[highest], EMI_ARBF);
			mt_reg_sync_writel(emi_arbg_lpddr3_1866_val[highest], EMI_ARBG);
		}
		cur_con_sce = highest;
	}

	up(&emi_bwl_sem);

	return 0;
}

/*
 * ddr_type_show: sysfs ddr_type file show function.
 * @driver:
 * @buf: the string of ddr type
 * Return the number of read bytes.
 */
static ssize_t ddr_type_show(struct device_driver *driver, char *buf)
{
	if (get_dram_type() == LPDDR2)
		sprintf(buf, "LPDDR2\n");
	else if (get_dram_type() == DDR3_16)
		sprintf(buf, "DDR3_16\n");
	else if (get_dram_type() == DDR3_32)
		sprintf(buf, "DDR3_32\n");
	else if (get_dram_type() == DUAL_LPDDR3_1600)
		sprintf(buf, "LPDDR3\n");
	else if (get_dram_type() == LPDDR3_1866)
		sprintf(buf, "LPDDR3\n");
	else
		sprintf(buf, "mDDR\n");

	return strlen(buf);
}

/*
 * ddr_type_store: sysfs ddr_type file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t ddr_type_store(struct device_driver *driver, const char *buf, size_t count)
{
	/*do nothing */
	return count;
}

DRIVER_ATTR(ddr_type, 0644, ddr_type_show, ddr_type_store);

/*
 * con_sce_show: sysfs con_sce file show function.
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t con_sce_show(struct device_driver *driver, char *buf)
{
	if (cur_con_sce >= NR_CON_SCE)
		sprintf(buf, "none\n");
	else
		sprintf(buf, "%s\n", con_sce_str[cur_con_sce]);

	return strlen(buf);
}

/*
 * con_sce_store: sysfs con_sce file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t con_sce_store(struct device_driver *driver, const char *buf, size_t count)
{
	int i;

	for (i = 0; i < NR_CON_SCE; i++) {
		if (!strncmp(buf, con_sce_str[i], strlen(con_sce_str[i]))) {
			if (!strncmp
			    (buf + strlen(con_sce_str[i]) + 1, EN_CON_SCE_STR,
			     strlen(EN_CON_SCE_STR))) {
				mtk_mem_bw_ctrl(i, ENABLE_CON_SCE);
				pr_err("concurrency scenario %s ON\n", con_sce_str[i]);
				break;
			} else if (!strncmp(buf + strlen(con_sce_str[i]) + 1, DIS_CON_SCE_STR,
						strlen(DIS_CON_SCE_STR))) {
				mtk_mem_bw_ctrl(i, DISABLE_CON_SCE);
				pr_err("concurrency scenario %s OFF\n", con_sce_str[i]);
				break;
			}
		}
	}

	return count;
}

DRIVER_ATTR(concurrency_scenario, 0644, con_sce_show, con_sce_store);


/*
 * finetune_md_show: sysfs con_sce file show function.
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t finetune_md_show(struct device_driver *driver, char *buf)
{
	unsigned int dram_type;

	dram_type = get_dram_type();

	if (dram_type == LPDDR2) {	/*LPDDR2. FIXME */
		switch (cur_con_sce) {
		case CON_SCE_VR:
			sprintf(buf, "true");
			break;
		default:
			sprintf(buf, "false");
			break;
		}
	} else if (dram_type == DDR3_16) {	/*DDR3-16bit. FIXME */
	 /*TBD*/} else if (dram_type == DDR3_32) {	/*DDR3-32bit. FIXME */
	 /*TBD*/} else if (dram_type == DUAL_LPDDR3_1600) {	/*2XLPDDR3-1600. FIXME */
	 /*TBD*/} else if (dram_type == LPDDR3_1866) {	/*LPDDR3-1866. FIXME */
	 /*TBD*/} else if (dram_type == mDDR) {	/*mDDR. FIXME */
	 /*TBD*/} else {
		/*unknown dram type */
		sprintf(buf, "ERROR: unknown dram type!");
	}

	return strlen(buf);
}

/*
 * finetune_md_store: sysfs con_sce file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t finetune_md_store(struct device_driver *driver, const char *buf, size_t count)
{
	/*Do nothing */
	return count;
}

DRIVER_ATTR(finetune_md, 0644, finetune_md_show, finetune_md_store);


/*
 * finetune_mm_show: sysfs con_sce file show function.
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t finetune_mm_show(struct device_driver *driver, char *buf)
{
	unsigned int dram_type;

	dram_type = get_dram_type();

	if (dram_type == LPDDR2) {	/*LPDDR2. FIXME */
		switch (cur_con_sce) {
		default:
			sprintf(buf, "false");
			break;
		}
	} else if (dram_type == DDR3_16) {	/*DDR3-16bit. FIXME */
	 /*TBD*/} else if (dram_type == DDR3_32) {	/*DDR3-32bit. FIXME */
	 /*TBD*/} else if (dram_type == DUAL_LPDDR3_1600) {	/*2XLPDDR3-1600. FIXME */
	 /*TBD*/} else if (dram_type == LPDDR3_1866) {	/*LPDDR3-1866. FIXME */
	 /*TBD*/} else if (dram_type == mDDR) {	/*mDDR. FIXME */
	 /*TBD*/} else {
		/*unknown dram type */
		sprintf(buf, "ERROR: unknown dram type!");
	}

	return strlen(buf);
}

/*
 * finetune_md_store: sysfs con_sce file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t finetune_mm_store(struct device_driver *driver, const char *buf, size_t count)
{
	/*Do nothing */
	return count;
}

DRIVER_ATTR(finetune_mm, 0644, finetune_mm_show, finetune_mm_store);

#if 0
/*
 * dramc_high_show: show the status of DRAMC_HI_EN
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t dramc_high_show(struct device_driver *driver, char *buf)
{
	unsigned int dramc_hi;

	dramc_hi = (readl(EMI_TESTB) >> 12) & 0x1;
	if (dramc_hi == 1)
		return sprintf(buf, "DRAMC_HI is ON\n");
	else
		return sprintf(buf, "DRAMC_HI is OFF\n");
}

/*
 dramc_hign_store: enable/disable DRAMC untra high. WARNING: ONLY CAN BE ENABLED AT MD_STANDALONE!!!
 * @driver:
 * @buf: need to be "0" or "1"
 * @count:
 * Return the number of write bytes.
*/
static ssize_t dramc_high_store(struct device_driver *driver, const char *buf, size_t count)
{
	unsigned int value;
	unsigned int emi_testb;

	if (kstrtoint(buf, 0, &value) != 1)
		return -EINVAL;

	emi_testb = readl(EMI_TESTB);

	if (value == 1) {
		emi_testb |= 0x1000;	/* Enable DRAM_HI */
		mt65xx_reg_sync_writel(emi_testb, EMI_TESTB);
	} else if (value == 0) {
		emi_testb &= ~0x1000;	/* Disable DRAM_HI */
		mt65xx_reg_sync_writel(emi_testb, EMI_TESTB);
	} else
		return -EINVAL;

	return count;
}

DRIVER_ATTR(dramc_high, 0644, dramc_high_show, dramc_high_store);
#endif
/*
 * emi_bwl_mod_init: module init function.
 */
static int __init emi_bwl_mod_init(void)
{
	int ret;

	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-dramco");
	if (node) {
		DRAMC_BASE_ADDR = of_iomap(node, 0);
		/*pr_err("get DRAMC_BASE_ADDR @ %p\n", DRAMC_BASE_ADDR);*/
	} else {
		pr_err("can't find compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-emi");
	if (node) {
		EMI_BASE_ADDR = of_iomap(node, 0);
		/*pr_err("get EMI_BASE_ADDR @ %p\n", EMI_BASE_ADDR);*/
	} else {
		pr_err("can't find compatible node\n");
		return -1;
	}

	/* write Filter Priority Encode */
	/*
	ret = mtk_mem_bw_ctrl(CON_SCE_NORMAL, ENABLE_CON_SCE);
	if (ret)
		pr_err("EMI/BWL, fail to set EMI bandwidth limiter\n");
	*/

	/* Register BW ctrl interface */
	ret = platform_driver_register(&mem_bw_ctrl);
	if (ret)
		pr_err("EMI/BWL, fail to register EMI_BW_LIMITER driver\n");

	ret = driver_create_file(&mem_bw_ctrl.driver, &driver_attr_concurrency_scenario);
	if (ret)
		pr_err("EMI/BWL, fail to create EMI_BW_LIMITER sysfs file\n");

	/* Register DRAM type information interface */
	ret = platform_driver_register(&ddr_type);
	if (ret)
		pr_err("EMI/BWL, fail to register DRAM_TYPE driver\n");

	ret = driver_create_file(&ddr_type.driver, &driver_attr_ddr_type);
	if (ret)
		pr_err("EMI/BWL, fail to create DRAM_TYPE sysfs file\n");

	return 0;
}

/*
 * emi_bwl_mod_exit: module exit function.
 */
static void __exit emi_bwl_mod_exit(void)
{
}

late_initcall(emi_bwl_mod_init);
module_exit(emi_bwl_mod_exit);
