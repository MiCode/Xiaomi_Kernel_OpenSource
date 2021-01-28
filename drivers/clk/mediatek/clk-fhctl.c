// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Pierre Lee <pierre.lee@mediatek.com>
 */


#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include "clk-fhctl.h"
#include "clk-fhctl-debug.h"
#include "clk-mtk.h"


/************************************************
 **********      register base addr    **********
 ************************************************/
#define REG_ADDR(base, x) (void __iomem *)((unsigned long)base + (x))

/************************************************
 **********         Variable           **********
 ************************************************/

/* spinlock for fhctl */
static DEFINE_SPINLOCK(fhctl_lock);
static LIST_HEAD(clk_mt_fhctl_list);

static struct mtk_fhctl *g_p_fhctl;

/*****************************************************************
 * Global variable operation
 ****************************************************************/
static void __set_fhctl(struct mtk_fhctl *pfhctl)
{
	g_p_fhctl = pfhctl;
}

static struct mtk_fhctl *__get_fhctl(void)
{
	return g_p_fhctl;
}


/*****************************************************************
 * OF Info init
 ****************************************************************/
static int mtk_fhctl_parse_dt(struct mtk_fhctl *fhctl)
{

	unsigned int pll_num;
	struct device *dev;
	struct device_node *child;
	struct device_node *node;

	pll_num = fhctl->pll_num;
	dev = fhctl->dev;
	node = dev->of_node;

	for_each_child_of_node(node, child) {
		struct clk_mt_fhctl_pll_data *pll_data;
		unsigned int id, pll_id, ssc;
		int err, tbl_size;
		bool ret;

		/* search for fhctl id */
		err = of_property_read_u32(child, "mediatek,fh-id", &id);
		if (err) {
			dev_info(dev, "miss fh-id property: %s", child->name);
			return err;
		}

		if (id >= pll_num) {
			dev_info(dev, "invalid %s fh-id:%d", child->name, id);
			return -EINVAL;
		}

		pll_data = fhctl->fh_tbl[id]->pll_data;

		/* Search for pll type */
		pll_data->pll_type = FH_PLL_TYPE_FORCE;

		/* Search for freqhopping table */
		tbl_size = of_property_count_u32_elems(child,
						"mediatek,fh-tbl");
		if (tbl_size > 0) {
			pll_data->hp_tbl_size = tbl_size;
			pll_data->hp_tbl = devm_kzalloc(dev,
					sizeof(unsigned int *)*tbl_size,
					GFP_KERNEL);

			if (!pll_data->hp_tbl)
				return -ENOMEM;

			err = of_property_read_u32_array(child,
							"mediatek,fh-tbl",
							pll_data->hp_tbl,
							tbl_size);
			if (err) {
				dev_info(dev, "invalid fh-tbl property of %s",
								child->name);
				return err;
			}

			/* Parse successfully. Set pll type */
			pll_data->pll_type = FH_PLL_TYPE_GENERAL;
		}

		/* Search for cpu pll type property */
		ret = of_property_read_bool(child, "mediatek,fh-cpu-pll");
		if (ret)
			pll_data->pll_type = FH_PLL_TYPE_CPU;

		/* Search for fh-pll-id */
		err = of_property_read_u32(child, "mediatek,fh-pll-id",
								&pll_id);
		if (!err)
			fhctl->idmap[id] = pll_id;

		/* Search for default ssc rate */
		err = of_property_read_u32(child, "mediatek,fh-ssc-rate",
								&ssc);
		if (!err)
			pll_data->pll_default_ssc_rate = ssc;
	}

	return 0;
}


static int __add_fh_obj_tbl(struct mtk_fhctl *pfhctl, int posi,
			struct clk_mt_fhctl *pfh)
{
	if (pfhctl == NULL) {
		pr_info("Error: null pointer pfhctl");
		return -EFAULT;
	}

	if (posi >= pfhctl->pll_num)
		return -EINVAL;

	pfhctl->fh_tbl[posi] = pfh;
	return 0;
}

struct clk_mt_fhctl *mtk_fh_get_fh_obj_tbl(struct mtk_fhctl *pfhctl, int posi)
{
	int size;
	struct clk_mt_fhctl *pfh;

	if (pfhctl == NULL) {
		pr_info("Error: null pointer pfhctl");
		return ERR_PTR(-EFAULT);
	}

	size = pfhctl->pll_num;

	if (posi >= size) {
		dev_info(pfhctl->dev, "Error: size:%d posi:%d", size, posi);
		return ERR_PTR(-EINVAL);
	}

	pfh = pfhctl->fh_tbl[posi];

	dev_dbg(pfhctl->dev, "get fh:0x%p pll_id:%d", pfh, posi);

	return pfh;
}
EXPORT_SYMBOL(mtk_fh_get_fh_obj_tbl);

/*********************************************************
 * For clock driver control
 ********************************************************/
bool _mtk_fh_set_rate(int pll_id, unsigned long dds, int postdiv)
{
	struct mtk_fhctl *fhctl;
	struct clk_mt_fhctl *fh;
	int fhctl_pll_id;

	int i, tbl_size, ret;

	pr_debug("check pll_id:0x%x dds:0x%lx", pll_id, dds);

	fhctl = __get_fhctl();
	if (fhctl == NULL) {
		pr_info("ERROR: fhctl is not initialized");
		return false;
	}

	/* Lookup table */
	if (unlikely(pll_id < 0))
		return false;

	fhctl_pll_id = -1;
	for (i = 0; i < fhctl->pll_num; i++)
		if (fhctl->idmap[i] == pll_id) {
			fhctl_pll_id = i;
			break;
		}

	if (fhctl_pll_id == -1) {
		pr_debug("pll not supportted by fhctl");
		return false;
	}

	pr_debug("found fhctl_pll_id:%d", fhctl_pll_id);

	fh = mtk_fh_get_fh_obj_tbl(fhctl, fhctl_pll_id);

	if (IS_ERR_OR_NULL(fh))
		return false;

	if (fh->pll_data->pll_type == FH_PLL_TYPE_NOT_SUPPORT) {
		pr_info("ERROR: pll not support");
		return false;
	}

	if (fh->pll_data->pll_type == FH_PLL_TYPE_CPU) {
		pr_info("ERROR: CPU hopping not support in AP side");
		return false;
	}

	if (fh->pll_data->pll_type == FH_PLL_TYPE_FORCE) {
		/* Force hopping by FHCTL. */
		ret = fh->hal_ops->pll_hopping(fh, dds, postdiv);
		return (ret == 0);
	}

	/* Look up hopping support table */
	if (fh->pll_data->hp_tbl == NULL) {
		pr_info("ERROR: fh->pll_data->hp_tbl NULL!");
		return false;
	}

	tbl_size = fh->pll_data->hp_tbl_size;
	for (i = 0; i < tbl_size; i++) {
		if (fh->pll_data->hp_tbl[i] == dds) {
			pr_debug("%s dds:0x%lx by fhctl hopping",
				fh->pll_data->pll_name, dds);
			ret = fh->hal_ops->pll_hopping(fh, dds, postdiv);
			return (ret == 0);
		}
	}

	return false;
}

/****************************************************
 * CLK FHCTL reg init
 ***************************************************/

static struct clk_mt_fhctl_regs *__mt_fhctl_fh_regs_init(
				struct mtk_fhctl *fhctl, unsigned int pll_id)
{
	struct clk_mt_fhctl_regs *fh_regs;
	void *fhctl_base = fhctl->fhctl_base;
	void *apmixed_base = fhctl->apmixed_base;
	unsigned int reg_cfg_offs = fhctl->dev_comp->pll_regs[pll_id];
	unsigned int reg_con0_offs = fhctl->dev_comp->pll_con0_regs[pll_id];

	fh_regs = devm_kmalloc(fhctl->dev, sizeof(struct clk_mt_fhctl_regs),
			GFP_KERNEL);
	if (!fh_regs)
		return ERR_PTR(-ENOMEM);

	/* fhctl common regs */
	fh_regs->reg_unitslope_en = REG_ADDR(fhctl_base,
		fhctl->dev_comp->common_regs[OFFSET_UNITSLOPE_EN]);
	fh_regs->reg_hp_en = REG_ADDR(fhctl_base,
		fhctl->dev_comp->common_regs[OFFSET_HP_EN]);
	fh_regs->reg_clk_con = REG_ADDR(fhctl_base,
		fhctl->dev_comp->common_regs[OFFSET_CLK_CON]);
	fh_regs->reg_rst_con = REG_ADDR(fhctl_base,
		fhctl->dev_comp->common_regs[OFFSET_RST_CON]);
	fh_regs->reg_slope0 = REG_ADDR(fhctl_base,
		fhctl->dev_comp->common_regs[OFFSET_SLOPE0]);
	fh_regs->reg_slope1 = REG_ADDR(fhctl_base,
		fhctl->dev_comp->common_regs[OFFSET_SLOPE1]);

	/* fhctl PLL specific regs */
	fh_regs->reg_cfg = REG_ADDR(fhctl_base, reg_cfg_offs);
	fh_regs->reg_updnlmt = REG_ADDR(fhctl_base, reg_cfg_offs + 0x04);
	fh_regs->reg_dds = REG_ADDR(fhctl_base, reg_cfg_offs + 0x08);
	fh_regs->reg_dvfs = REG_ADDR(fhctl_base, reg_cfg_offs + 0xC);
	fh_regs->reg_mon = REG_ADDR(fhctl_base, reg_cfg_offs + 0x10);

	fh_regs->reg_con0 = REG_ADDR(apmixed_base, reg_con0_offs);
	fh_regs->reg_con1 = REG_ADDR(apmixed_base, reg_con0_offs + 0x04);

	return fh_regs;
}


static struct clk_mt_fhctl *clk_register_fhctl_pll(
			struct device *dev,
			const struct clk_mt_fhctl_hal_ops *hal_ops,
			struct clk_mt_fhctl_pll_data *pll_data,
			struct clk_mt_fhctl_regs *fh_regs)
{
	struct clk_mt_fhctl *fh;

	fh = devm_kmalloc(dev, sizeof(struct clk_mt_fhctl), GFP_KERNEL);
	if (!fh)
		return ERR_PTR(-ENOMEM);

	fh->pll_data = pll_data;
	fh->fh_regs = fh_regs;
	fh->hal_ops = hal_ops;
	fh->lock = &fhctl_lock;

	return fh;
}


static int mt_fh_plt_drv_probe(struct platform_device *pdev)
{
	int i, err, pll_num;
	int dds_mask_size;
	struct mtk_fhctl *fhctl;
	struct resource *res;
	struct device_node *apmixed_node;

	dev_info(&pdev->dev, "FHCTL driver probe start");

	fhctl = devm_kmalloc(&pdev->dev, sizeof(*fhctl), GFP_KERNEL);
	if (!fhctl)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	fhctl->fhctl_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(fhctl->fhctl_base))
		return PTR_ERR(fhctl->fhctl_base);

	/* Init APMIXED base address */
	apmixed_node = of_parse_phandle(pdev->dev.of_node,
					"mediatek,apmixed", 0);
	if (!apmixed_node) {
		dev_info(&pdev->dev, "fhctl: missing mediatek,apmixed node");
		return -ENODEV;
	}
	fhctl->apmixed_base = of_iomap(apmixed_node, 0);


	fhctl->dev = &pdev->dev;
	fhctl->dev_comp = of_device_get_match_data(&pdev->dev);
	fhctl->pll_num = fhctl->dev_comp->pll_num;
	dds_mask_size = fhctl->dev_comp->pll_dds_reg_field_size;

	pll_num = fhctl->pll_num;

	fhctl->idmap = devm_kmalloc(&pdev->dev,
					sizeof(int)*pll_num, GFP_KERNEL);
	if (!fhctl->idmap)
		return -ENOMEM;

	platform_set_drvdata(pdev, fhctl);

	fhctl->fh_tbl = devm_kmalloc(&pdev->dev,
		sizeof(struct clk_mt_fhctl *) * pll_num, GFP_KERNEL);
	if (!fhctl->fh_tbl)
		return -ENOMEM;

	/* register all fhctl pll */
	for (i = 0; i < pll_num; i++) {
		struct clk_mt_fhctl *fh;
		struct clk_mt_fhctl_pll_data *pll_data;
		struct clk_mt_fhctl_regs *fh_regs;

		pll_data = devm_kmalloc(&pdev->dev,
			sizeof(struct clk_mt_fhctl_pll_data), GFP_KERNEL);
		if (!pll_data)
			return -ENOMEM;

		fhctl->idmap[i] = -1;

		/* Set pll data */
		pll_data->pll_id = i;
		pll_data->pll_name = fhctl->dev_comp->pll_names[i];
		pll_data->pll_type = FH_PLL_TYPE_NOT_SUPPORT;
		pll_data->dds_mask = GENMASK(dds_mask_size-1, 0);
		pll_data->pll_default_ssc_rate = 0;
		pll_data->slope0_value =
			fhctl->dev_comp->pll_slope0_reg_setting;
		pll_data->slope1_value =
			fhctl->dev_comp->pll_slope1_reg_setting;
		pll_data->hp_tbl = NULL;
		pll_data->hp_tbl_size = 0;

		/* Init fhctl PLL regs */
		fh_regs = __mt_fhctl_fh_regs_init(fhctl, i);
		if (IS_ERR_OR_NULL(fh_regs)) {
			dev_info(&pdev->dev, "ERROR: init fh_regs fail.");
			return PTR_ERR(fh_regs);
		}

		fh = clk_register_fhctl_pll(&pdev->dev, &mt_fhctl_hal_ops,
				pll_data, fh_regs);
		if (IS_ERR(fh)) {
			dev_info(&pdev->dev,
				"register clk fhctl failed: %s",
				pll_data->pll_name);
			return PTR_ERR(fh);
		}

		list_add(&fh->node, &clk_mt_fhctl_list);

		/* Add fh object to table */
		err = __add_fh_obj_tbl(fhctl, i, fh);
		if (err)
			dev_info(&pdev->dev,
				"add fh object %d to table failed", i);

	}


	/* Read fhctl setting by device tree */
	err = mtk_fhctl_parse_dt(fhctl);
	if (err) {
		dev_info(&pdev->dev, "ERROR mtk_fhctl_parse_dt fail");
		return err;
	}

	for (i = 0; i < pll_num ; i++)
		fhctl->fh_tbl[i]->hal_ops->pll_init(fhctl->fh_tbl[i]);

	__set_fhctl(fhctl);

	mt_fhctl_init_debugfs(fhctl);

	mtk_fh_set_rate = _mtk_fh_set_rate;

	dev_info(&pdev->dev, "FHCTL Init Done");

	for (i = 0; i < fhctl->pll_num; i++)
		dev_info(&pdev->dev, "pllid_map[%d]=%d", i, fhctl->idmap[i]);

	/* show setting value */
	dev_dbg(&pdev->dev, "pll_num:%d", fhctl->pll_num);
	dev_dbg(&pdev->dev, "apmixed_base:0x%lx",
		(unsigned long)fhctl->apmixed_base);
	dev_dbg(&pdev->dev, "fhctl_base:0x%lx",
		(unsigned long)fhctl->fhctl_base);
	dev_dbg(&pdev->dev, "pll_dds_reg_field_size:%d",
		fhctl->dev_comp->pll_dds_reg_field_size);
	dev_dbg(&pdev->dev, "pll_reg_offs:0x%x",
		fhctl->dev_comp->pll_regs[0]);
	dev_dbg(&pdev->dev, "pll-type[0]:%d",
		fhctl->fh_tbl[0]->pll_data->pll_type);
	dev_dbg(&pdev->dev, "pll_default_enable_ssc[0]:%d",
		fhctl->fh_tbl[0]->pll_data->pll_default_ssc_rate);
	dev_dbg(&pdev->dev, "pll_con0_regs[0]:0x%x",
		fhctl->dev_comp->pll_con0_regs[0]);
	dev_dbg(&pdev->dev, "pll_slope0_reg_settings[0]:0x%x",
		fhctl->dev_comp->pll_slope0_reg_setting);
	dev_dbg(&pdev->dev, "pll_slope1_reg_settings[0]:0x%x",
		fhctl->dev_comp->pll_slope1_reg_setting);
	dev_dbg(&pdev->dev, "pll_names[0]:%s",
		fhctl->fh_tbl[0]->pll_data->pll_name);

	return 0;
}

static int mt_fh_plt_drv_remove(struct platform_device *pdev)
{
	struct mtk_fhctl *fhctl = platform_get_drvdata(pdev);

	mtk_fh_set_rate = NULL;
	mt_fhctl_exit_debugfs(fhctl);
	return 0;
}

static void mt_fh_plt_drv_shutdown(struct platform_device *pdev)
{
	struct clk_mt_fhctl *fh;

	dev_dbg(&pdev->dev, "%s!", __func__);

	list_for_each_entry(fh, &clk_mt_fhctl_list, node) {
		if (fh->pll_data->pll_default_ssc_rate > 0) {
			dev_info(&pdev->dev, "Shutdown to Disable SSC => PLL:%s ",
					fh->pll_data->pll_name);
			fh->hal_ops->pll_ssc_disable(fh);
		}
	}
	dev_dbg(&pdev->dev, "%s Done!", __func__);
}


static const u16 mt_fhctl_regs_v1[] = {
	[OFFSET_HP_EN] = 0x0,
	[OFFSET_CLK_CON] = 0x4,
	[OFFSET_RST_CON] = 0x8,
	[OFFSET_SLOPE0] = 0xc,
	[OFFSET_SLOPE1] = 0x10,
	[OFFSET_FHCTL_DSSC_CFG] = 0x14,
};

static const u16 mt_fhctl_regs_v2[] = {
	[OFFSET_UNITSLOPE_EN] = 0x0,
	[OFFSET_HP_EN] = 0x4,
	[OFFSET_CLK_CON] = 0x8,
	[OFFSET_RST_CON] = 0xc,
	[OFFSET_SLOPE0] = 0x10,
	[OFFSET_SLOPE1] = 0x14,
	[OFFSET_FHCTL_DSSC_CFG] = 0x18,
};


static const char * const mt6779_pll_names[] = {
			"armpll_ll", "armpll_bl", "armpll_bb", "ccipll",
			"mfgpll", "mpll", "mempll", "mainpll",
			"msdcpll", "mmpll", "adsppll", "tvdpll"};


static const u16 mt6779_pll_regs[] = {
			0x0038, 0x004C, 0xdead, 0x0074,
			0x088, 0x009C, 0x00B0, 0x00C4,
			0x00D8, 0x00EC, 0x0100, 0x0114};

static const u16 mt6779_pll_con0_regs[] = {
			0x200, 0x210, 0x0220, 0x02A0,
			0x0250, 0x0290, 0xdead, 0x0230,
			0x0260, 0x0280, 0x02b0, 0x0270};

static const char * const mt6761_pll_names[] = {
			"armpll", "mainpll", "msdcpll", "mfgpll",
			"mempll", "mpll", "mmpll"};


static const u16 mt6761_pll_regs[] = {
			0x003C, 0x0050, 0x0064, 0x0078,
			0x008C, 0x00A0, 0x00B4};

static const u16 mt6761_pll_con0_regs[] = {
			0x030C, 0x0228, 0x0350, 0x0218,
			0xdead, 0x0340, 0x0330};

static const struct mtk_fhctl_compatible mt6779_fhctl_compat = {
	.common_regs = mt_fhctl_regs_v1,
	.pll_num = 12,
	.pll_names = mt6779_pll_names,
	.pll_dds_reg_field_size = 22,
	.pll_regs = mt6779_pll_regs,
	.pll_con0_regs = mt6779_pll_con0_regs,
	.pll_slope0_reg_setting = 0x6003c97,
	.pll_slope1_reg_setting = 0x6003c97,
};

static const struct mtk_fhctl_compatible mt6761_fhctl_compat = {
	.common_regs = mt_fhctl_regs_v2,
	.pll_num = 7,
	.pll_names = mt6761_pll_names,
	.pll_dds_reg_field_size = 22,
	.pll_regs = mt6761_pll_regs,
	.pll_con0_regs = mt6761_pll_con0_regs,
	.pll_slope0_reg_setting = 0x6003c97,
	.pll_slope1_reg_setting = 0x6003c97,
};

static const struct of_device_id mtk_fhctl_of_match[] = {
	{ .compatible = "mediatek,mt6779-fhctl", .data = &mt6779_fhctl_compat },
	{ .compatible = "mediatek,mt6761-fhctl", .data = &mt6761_fhctl_compat },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_fhctl_of_match);


static struct platform_driver fhctl_driver = {
	.probe = mt_fh_plt_drv_probe,
	.remove = mt_fh_plt_drv_remove,
	.shutdown = mt_fh_plt_drv_shutdown,
	.driver = {
		.name = "mt-freqhopping",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_fhctl_of_match),
	},
};

module_platform_driver(fhctl_driver);



MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek FHCTL Driver");
MODULE_AUTHOR("Pierre Lee <pierre.lee@mediatek.com>");

