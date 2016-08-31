/*
 * drivers/video/tegra/dc/mipi_cal.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/ioport.h>
#include <linux/gfp.h>
#include <linux/export.h>
#include "dc_priv.h"
#include "mipi_cal.h"
#include "mipi_cal_regs.h"
#include "dsi.h"
#include <linux/of_address.h>

#include "../../../../arch/arm/mach-tegra/iomap.h"

#ifdef CONFIG_DEBUG_FS
static int dbg_dsi_mipi_show(struct seq_file *s, void *unused)
{
	struct tegra_mipi_cal *mipi_cal = s->private;
	unsigned long i = 0;
	u32 col = 0;

	/* If gated quitely return */
	if (!tegra_dc_is_powered(mipi_cal->dc))
		return 0;

	BUG_ON(IS_ERR_OR_NULL(mipi_cal));
	mutex_lock(&mipi_cal->lock);
	tegra_mipi_cal_clk_enable(mipi_cal);

	/* mem dd dump */
	for (col = 0, i = 0; i <= MIPI_VALID_REG_LIMIT ; i += 4) {
		if (col == 0)
			seq_printf(s, "%08lX:", TEGRA_MIPI_CAL_BASE + i);
		seq_printf(s, "%c%08lX", col == 2 ? '-' : ' ',
			tegra_mipi_cal_read(mipi_cal, i));
		if (col == 3) {
			seq_printf(s, "\n");
			col = 0;
		} else
			col++;
	}
	seq_printf(s, "\n");

	tegra_mipi_cal_clk_disable(mipi_cal);
	mutex_unlock(&mipi_cal->lock);
	return 0;
}

static int dbg_dsi_mipi_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dsi_mipi_show, inode->i_private);
}

static const struct file_operations dbg_fops = {
	.open		= dbg_dsi_mipi_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *mipidir;

static void dbg_dsi_mipi_dir_create(struct tegra_mipi_cal *mipi_cal)
{
	struct dentry *retval;

	mipidir = debugfs_create_dir("tegra_mipi_cal", NULL);
	if (!mipidir)
		return;
	retval = debugfs_create_file("regs", S_IRUGO, mipidir, mipi_cal,
		&dbg_fops);
	if (!retval)
		goto free_out;
	return;
free_out:
	debugfs_remove_recursive(mipidir);
	mipidir = NULL;
	return;
}
#else
static inline void dbg_dsi_mipi_dir_create(struct tegra_mipi_cal *mipi_cal)
{ }
#endif

int tegra_mipi_cal_init_hw(struct tegra_mipi_cal *mipi_cal)
{
	unsigned cnt = MIPI_CAL_MIPI_CAL_CTRL_0;

	BUG_ON(IS_ERR_OR_NULL(mipi_cal));

	mutex_lock(&mipi_cal->lock);

	tegra_mipi_cal_clk_enable(mipi_cal);

	for (; cnt <= MIPI_VALID_REG_LIMIT; cnt += 4)
		tegra_mipi_cal_write(mipi_cal, 0, cnt);

	/* Clear MIPI cal status register */
	tegra_mipi_cal_write(mipi_cal,
			MIPI_AUTO_CAL_DONE_DSID(0x1) |
			MIPI_AUTO_CAL_DONE_DSIC(0x1) |
			MIPI_AUTO_CAL_DONE_DSIB(0x1) |
			MIPI_AUTO_CAL_DONE_DSIA(0x1) |
			MIPI_AUTO_CAL_DONE_CSIE(0x1) |
			MIPI_AUTO_CAL_DONE_CSID(0x1) |
			MIPI_AUTO_CAL_DONE_CSIC(0x1) |
			MIPI_AUTO_CAL_DONE_CSIB(0x1) |
			MIPI_AUTO_CAL_DONE_CSIA(0x1) |
			MIPI_AUTO_CAL_DONE(0x1) |
			MIPI_CAL_DRIV_DN_ADJ(0x0) |
			MIPI_CAL_DRIV_UP_ADJ(0x0) |
			MIPI_CAL_TERMADJ(0x0) |
			MIPI_CAL_ACTIVE(0x0),
		MIPI_CAL_CIL_MIPI_CAL_STATUS_0);

	tegra_mipi_cal_clk_disable(mipi_cal);
	mutex_unlock(&mipi_cal->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_mipi_cal_init_hw);

struct tegra_mipi_cal *tegra_mipi_cal_init_sw(struct tegra_dc *dc)
{
	struct tegra_mipi_cal *mipi_cal;
	struct resource *res;
	struct resource mipi_res;
	struct resource *base_res;
	struct clk *clk;
	struct clk *fixed_clk;
	void __iomem *base;
	int err = 0;
#ifdef CONFIG_USE_OF
	struct device_node *np_mipi_cal =
		of_find_node_by_path("/mipical");
#else
	struct device_node *np_mipi_cal = NULL;
#endif
	mipi_cal = devm_kzalloc(&dc->ndev->dev, sizeof(*mipi_cal), GFP_KERNEL);
	if (!mipi_cal) {
		dev_err(&dc->ndev->dev, "mipi_cal: memory allocation fail\n");
		err = -ENOMEM;
		goto fail;
	}

	if (np_mipi_cal && of_device_is_available(np_mipi_cal)) {
			of_address_to_resource(
				np_mipi_cal, 0, &mipi_res);
			res = &mipi_res;
	} else {
		res = platform_get_resource_byname(dc->ndev,
				IORESOURCE_MEM, "mipi_cal");
	}
	if (!res) {
		dev_err(&dc->ndev->dev, "mipi_cal: no entry in resource\n");
		err = -ENOENT;
		goto fail_free_mipi_cal;
	}

	base_res = devm_request_mem_region(&dc->ndev->dev, res->start,
		resource_size(res), dc->ndev->name);
	base = devm_ioremap(&dc->ndev->dev, res->start,
			resource_size(res));
	if (!base) {
		dev_err(&dc->ndev->dev, "mipi_cal: bus to virtual mapping failed\n");
		err = -EBUSY;
		goto fail_free_res;
	}

	clk = clk_get_sys("mipi-cal", NULL);
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(&dc->ndev->dev, "mipi_cal: clk get failed\n");
		err = PTR_ERR(clk);
		goto fail_free_map;
	}
	fixed_clk = clk_get_sys("mipi-cal-fixed", NULL);
	if (IS_ERR_OR_NULL(fixed_clk)) {
		dev_err(&dc->ndev->dev, "mipi_cal: fixed clk get failed\n");
		err = PTR_ERR(fixed_clk);
		goto fail_free_map;
	}

	mutex_init(&mipi_cal->lock);
	mipi_cal->dc = dc;
	mipi_cal->res = res;
	mipi_cal->base = base;
	mipi_cal->base_res = base_res;
	mipi_cal->clk = clk;
	mipi_cal->fixed_clk = fixed_clk;
	dbg_dsi_mipi_dir_create(mipi_cal);
	return mipi_cal;

fail_free_map:
	devm_iounmap(&dc->ndev->dev, base);
	devm_release_mem_region(&dc->ndev->dev,
		res->start, resource_size(res));
fail_free_res:
	if (!np_mipi_cal || !of_device_is_available(np_mipi_cal))
		release_resource(res);
fail_free_mipi_cal:
	devm_kfree(&dc->ndev->dev, mipi_cal);
fail:
	return ERR_PTR(err);
}
EXPORT_SYMBOL(tegra_mipi_cal_init_sw);

void tegra_mipi_cal_destroy(struct tegra_dc *dc)
{
#ifdef CONFIG_USE_OF
	struct device_node *np_mipi_cal =
		of_find_node_by_path("/mipical");
#else
	struct device_node *np_mipi_cal = NULL;
#endif
	struct tegra_mipi_cal *mipi_cal =
		((struct tegra_dc_dsi_data *)
		(tegra_dc_get_outdata(dc)))->mipi_cal;

	BUG_ON(IS_ERR_OR_NULL(mipi_cal));

	mutex_lock(&mipi_cal->lock);

	clk_put(mipi_cal->clk);
	devm_iounmap(&dc->ndev->dev, mipi_cal->base);
	devm_release_mem_region(&dc->ndev->dev,
		mipi_cal->res->start,
		resource_size(mipi_cal->res));

	if (!np_mipi_cal || !of_device_is_available(np_mipi_cal))
		release_resource(mipi_cal->res);

	mutex_unlock(&mipi_cal->lock);

	mutex_destroy(&mipi_cal->lock);
	devm_kfree(&dc->ndev->dev, mipi_cal);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(mipidir);
#endif
}
EXPORT_SYMBOL(tegra_mipi_cal_destroy);

