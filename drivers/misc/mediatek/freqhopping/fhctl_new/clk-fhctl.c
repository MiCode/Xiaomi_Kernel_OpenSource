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

static int (*subsys_init[])(struct pll_dts *array) = {
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

int mt_dfs_general_pll(int pll_id, int dds)
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
		if (pll_id == array->pll_id) {
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
int mt_dfs_armpll(int fh_id, int dds)
{
	return mt_dfs_general_pll(fh_id, dds);
}
EXPORT_SYMBOL(mt_dfs_armpll);
static struct pll_dts *parse_dt(struct platform_device *pdev)
{
	struct device_node *root, *map, *of_pll;
	unsigned int num_pll = 0;
	int iomap_idx = 0;
	struct pll_dts *array;
	int pll_idx = 0;
	const struct of_device_id *match;
	int size;

	root = pdev->dev.of_node;
	match = of_match_node(pdev->dev.driver->of_match_table, root);

	/* iterate dts to get pll count */
	for_each_child_of_node(root, map) {
		for_each_child_of_node(map, of_pll) {
			num_pll++;
		}
	}
	FHDBG("num_pll<%d>\n", num_pll);

	size = sizeof(*array)*num_pll;
	array = kzalloc(size, GFP_KERNEL);
	FHDBG("array<%x>, num_pll<%d>, comp<%s>, sizeof(*array)=%d, size<%d>\n",
			array, num_pll,
			match->compatible, sizeof(*array), size);
	for_each_child_of_node(root, map) {
		void __iomem *fhctl_base, *apmixed_base;
		char *domain, *method;
		int num;

		fhctl_base = of_iomap(root, iomap_idx++);
		apmixed_base = of_iomap(root, iomap_idx++);
		of_property_read_string(map, "domain", (const char **)&domain);
		of_property_read_string(map, "method", (const char **)&method);

		num = 0;
		FHDBG("---------------------\n");
		for_each_child_of_node(map, of_pll) {
			int fh_id, pll_id;
			int perms, ssc_rate;

			if (pll_idx >= num_pll) {
				FHDBG("pll<%s> skipped\n",
						of_pll->name);
				pll_idx++;
				continue;
			}

			/* default for optional field */
			perms = 0xffffffff;
			ssc_rate = 0;

			of_property_read_u32(of_pll, "fh-id", &fh_id);
			of_property_read_u32(of_pll, "pll-id", &pll_id);
			of_property_read_u32(of_pll, "perms", &perms);
			of_property_read_u32(of_pll, "ssc-rate", &ssc_rate);
			array[pll_idx].num_pll = num_pll;
			array[pll_idx].comp = (char *)match->compatible;
			array[pll_idx].pll_name = (char *)of_pll->name;
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

		FHDBG("domain<%s>, method<%s>\n", domain, method);
		FHDBG("base<%lx,%lx>\n", fhctl_base, apmixed_base);
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

	int (**init_call)(struct pll_dts *) = subsys_init;

	FHDBG("in\n");

	/* convert dt to data */
	array = parse_dt(pdev);

	if (array == NULL) {
		FHDBG("array is null!");
		WARN_ON(1);
		return 0;
	}

	/* init every subsys */
	while (*init_call != NULL) {
		(*init_call)(array);
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
	{ .compatible = "mediatek,mt6877-fhctl"},
	{ .compatible = "mediatek,mt6873-fhctl"},
	{ .compatible = "mediatek,mt6885-fhctl"},
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
