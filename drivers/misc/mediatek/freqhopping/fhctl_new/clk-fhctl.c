// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include "clk-fhctl.h"
#include "clk-fhctl-util.h"

static int (*subsys_init[])(struct platform_device *pdev,
		struct pll_dts *array) = {
	&fhctl_ap_init,
#ifdef USE_FHCTL_MCUPM
	&fhctl_mcupm_init,
#endif
#ifdef CONFIG_DEBUG_FS
	&fhctl_debugfs_init,
#endif
	NULL,
};

static bool _inited;
static struct pll_dts *_array;
static void set_dts_array(struct pll_dts *array) {_array = array; }
static struct pll_dts *get_dts_array(void) {return _array; }

int mt_dfs_general_pll(int fh_id, int dds)
{
	int i;
	struct fh_hdlr *hdlr = NULL;
	struct pll_dts *array = get_dts_array();
	int num_pll = array->num_pll;

	if (!_inited) {
		FHDBG("!_inited\n");
		return -1;
	}

	for (i = 0; i < num_pll; i++, array++) {
		if (fh_id == array->fh_id) {
			hdlr = array->hdlr;
			break;
		}
	}

	if (hdlr && (array->perms & PERM_DRV_HOP))
		return hdlr->ops->hopping(hdlr->data,
				array->domain,
				array->fh_id,
				dds, 9999);

	FHDBG("hdlr<%x>, perms<%x>",
			hdlr, array->perms);
	return -1;
}
EXPORT_SYMBOL(mt_dfs_general_pll);
#ifdef CONFIG_MACH_MT6739
#define FH_ARM_PLLID 0
int mt_dfs_armpll(int fh_id, int dds)
{
	return mt_dfs_general_pll(FH_ARM_PLLID, dds);
}
#define FH_ID_MEM_6739 4
int freqhopping_config(unsigned int fh_id
	, unsigned long vco_freq, unsigned int enable)
{
	int i;
	struct fh_hdlr *hdlr = NULL;
	struct pll_dts *array = get_dts_array();
	int num_pll = array->num_pll;
	static bool on;
	static DEFINE_MUTEX(lock);

	if (!_inited) {
		FHDBG("!_inited\n");
		return -1;
	}

	for (i = 0; i < num_pll; i++, array++) {
		if (fh_id == array->fh_id) {
			hdlr = array->hdlr;
			break;
		}
	}

	if (!hdlr || fh_id != FH_ID_MEM_6739) {
		FHDBG("err!, hdlr<%x>, fh_id<%d>\n",
				hdlr, fh_id);
		return -1;
	}

	mutex_lock(&lock);
	if (!on && enable) {
		FHDBG("enable\n");
		hdlr->ops->ssc_enable(hdlr->data,
				array->domain,
				array->fh_id,
				8);
		on = true;
	} else if (on && !enable) {
		FHDBG("disable\n");
		hdlr->ops->ssc_disable(hdlr->data,
				array->domain,
				array->fh_id);
		on = false;
	} else
		FHDBG("already %s\n",
				on ? "enabled" : "disabled");
	mutex_unlock(&lock);

	return 0;
}
EXPORT_SYMBOL(freqhopping_config);
#else
int mt_dfs_armpll(int fh_id, int dds)
{
	return mt_dfs_general_pll(fh_id, dds);
}
#endif
EXPORT_SYMBOL(mt_dfs_armpll);
static struct pll_dts *parse_dt(struct platform_device *pdev)
{
	struct device_node *child;
	struct device_node *root;
	unsigned int num_pll = 0;
	int iomap_idx = 0;
	struct pll_dts *array;
	int pll_idx = 0;
	const struct of_device_id *match;
	int size;

	root = pdev->dev.of_node;
	match = of_match_node(pdev->dev.driver->of_match_table, root);
	of_property_read_u32(root, "num-pll", &num_pll);

	size = sizeof(*array)*num_pll;
	array = kzalloc(size, GFP_KERNEL);
	FHDBG("array<%x>, num_pll<%d>, comp<%s>\n",
			array, num_pll,
			match->compatible);
	for_each_child_of_node(root, child) {
		struct device_node *m, *n;
		void __iomem *fhctl_base, *apmixed_base;
		char *domain, *method;
		int num;

		fhctl_base = of_iomap(root, iomap_idx);
		apmixed_base = of_iomap(root, iomap_idx + 1);
		of_property_read_string(child, "domain", (const char **)&domain);
		of_property_read_string(child, "method", (const char **)&method);

		m = child;
		num = 0;
		FHDBG("---------------------\n");
		for_each_child_of_node(m, n) {
			int fh_id, pll_id;
			int perms, ssc_rate;

			if (pll_idx >= num_pll) {
				FHDBG("pll<%s> skipped\n",
						n->name);
				pll_idx++;
				continue;
			}

			/* default for optional field */
			perms = 0xffffffff;
			ssc_rate = 0;

			of_property_read_u32(n, "fh-id", &fh_id);
			of_property_read_u32(n, "pll-id", &pll_id);
			of_property_read_u32(n, "perms", &perms);
			of_property_read_u32(n, "ssc-rate", &ssc_rate);
			array[pll_idx].num_pll = num_pll;
			array[pll_idx].comp = (char *)match->compatible;
			array[pll_idx].pll_name = (char *)n->name;
			array[pll_idx].fh_id = fh_id;
			array[pll_idx].pll_id = pll_id;
			array[pll_idx].perms = perms;
			array[pll_idx].ssc_rate = ssc_rate;
			array[pll_idx].domain = domain;
			array[pll_idx].method = method;
			array[pll_idx].fhctl_base = fhctl_base;
			array[pll_idx].apmixed_base = apmixed_base;
			num++;
			pll_idx++;
		}
		iomap_idx++;

		FHDBG("domain<%s>, method<%s>\n", domain, method);
		FHDBG("base<%x,%x>\n", fhctl_base, apmixed_base);
		FHDBG("num<%d>\n", num);
		FHDBG("---------------------\n");
	}

	set_dts_array(array);

	return array;
}
static int fh_plt_drv_probe(struct platform_device *pdev)
{
	int i;
	int num_pll;
	struct pll_dts *array;

	int (**init_call)(struct platform_device *,
			struct pll_dts *) = subsys_init;

	/* convert dt to data */
	array = parse_dt(pdev);
	dev_set_drvdata(&pdev->dev, (void *)array);

	/* init every subsys */
	while (*init_call != NULL) {
		(*init_call)(pdev, array);
		init_call++;
	}

	/* make sure array is complete */
	num_pll = array->num_pll;
	for (i = 0; i < num_pll; i++, array++) {
		struct fh_hdlr *hdlr = array->hdlr;

		if (!hdlr) {
			FHDBG("hdlr is NULL!!! <%s,%s,%s>\n",
					array->pll_name,
					array->domain,
					array->method);
			return -1;
		}
	}

	/* set _inited is the last step */
	mb();
	_inited = true;

	return 0;
}
static int fh_plt_drv_remove(struct platform_device *pdev) {return 0; }
static void fh_plt_drv_shutdown(struct platform_device *pdev)
{
	struct pll_dts *array = get_dts_array();
	int num_pll = array->num_pll;
	int i;

	for (i = 0; i < num_pll; i++, array++) {
		struct fh_hdlr *hdlr = array->hdlr;

		if (array->ssc_rate)
			hdlr->ops->ssc_disable(hdlr->data,
					array->domain,
					array->fh_id);
	}
}

static const struct of_device_id fh_of_match[] = {
	{ .compatible = "mediatek,mt6853-fhctl"},
	{ .compatible = "mediatek,mt6739-fhctl"},
	{}
};
static struct platform_driver fhctl_driver = {
	.probe = fh_plt_drv_probe,
	.remove = fh_plt_drv_remove,
	.shutdown = fh_plt_drv_shutdown,
	.driver = {
		.name = "fhctl",
		.owner = THIS_MODULE,
		.of_match_table = fh_of_match,
	},
};
static int __init fhctl_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&fhctl_driver);
	FHDBG("ret<%d>\n", ret);
	return ret;
}
subsys_initcall(fhctl_driver_init);
