/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>
#include "footswitch.h"
#include "proc_comm.h"

/* PCOM power rail IDs */
#define PCOM_FS_GRP		8
#define PCOM_FS_GRP_2D		58
#define PCOM_FS_MDP		14
#define PCOM_FS_MFC		68
#define PCOM_FS_ROTATOR		90
#define PCOM_FS_VFE		41
#define PCOM_FS_VPE		76

#define PCOM_RAIL_MODE_AUTO	0
#define PCOM_RAIL_MODE_MANUAL	1

/**
 * struct footswitch - Per-footswitch data and state
 * @rdev: Regulator framework device
 * @desc: Regulator descriptor
 * @init_data: Regulator platform data
 * @pcom_id: Proc-comm ID of the footswitch
 * @is_enabled: Flag set when footswitch is enabled
 * @is_manual: Flag set when footswitch is in manual proc-comm mode
 * @has_ahb_clk: Flag set if footswitched core has an ahb_clk
 * @has_src_clk: Flag set if footswitched core has a src_clk
 * @src_clk: Controls the core clock's rate
 * @core_clk: Clocks the core
 * @ahb_clk: Clocks the core's register interface
 * @src_clk_init_rate: Rate to use for src_clk if it has not been set yet
 * @is_rate_set: Flag set if core_clk's rate has been set
 */
struct footswitch {
	struct regulator_dev			*rdev;
	struct regulator_desc			desc;
	struct regulator_init_data		init_data;
	unsigned				pcom_id;
	bool					is_enabled;
	bool					is_manual;
	struct clk				*src_clk;
	struct clk				*core_clk;
	struct clk				*ahb_clk;
	const bool				has_ahb_clk;
	const bool				has_src_clk;
	const int				src_clk_init_rate;
	bool					is_rate_set;
};

static inline int set_rail_mode(int pcom_id, int mode)
{
	int  rc;

	rc = msm_proc_comm(PCOM_CLKCTL_RPC_RAIL_CONTROL, &pcom_id, &mode);
	if (!rc && pcom_id)
		rc = -EINVAL;

	return rc;
}

static inline int set_rail_state(int pcom_id, int state)
{
	int  rc;

	rc = msm_proc_comm(state, &pcom_id, NULL);
	if (!rc && pcom_id)
		rc = -EINVAL;

	return rc;
}

static int enable_clocks(struct footswitch *fs)
{
	fs->is_rate_set = !!(clk_get_rate(fs->src_clk));
	if (!fs->is_rate_set)
		clk_set_rate(fs->src_clk, fs->src_clk_init_rate);
	clk_enable(fs->core_clk);

	if (fs->ahb_clk)
		clk_enable(fs->ahb_clk);

	return 0;
}

static void disable_clocks(struct footswitch *fs)
{
	if (fs->ahb_clk)
		clk_disable(fs->ahb_clk);
	clk_disable(fs->core_clk);
}

static int footswitch_is_enabled(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);

	return fs->is_enabled;
}

static int footswitch_enable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	int rc;

	rc = enable_clocks(fs);
	if (rc)
		return rc;

	rc = set_rail_state(fs->pcom_id, PCOM_CLKCTL_RPC_RAIL_ENABLE);
	if (!rc)
		fs->is_enabled = true;

	disable_clocks(fs);

	return rc;
}

static int footswitch_disable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	int rc;

	rc = enable_clocks(fs);
	if (rc)
		return rc;

	rc = set_rail_state(fs->pcom_id, PCOM_CLKCTL_RPC_RAIL_DISABLE);
	if (!rc)
		fs->is_enabled = false;

	disable_clocks(fs);

	return rc;
}

static struct regulator_ops footswitch_ops = {
	.is_enabled = footswitch_is_enabled,
	.enable = footswitch_enable,
	.disable = footswitch_disable,
};

#define FOOTSWITCH(_id, _pcom_id, _name, _src_clk, _rate, _ahb_clk) \
	[_id] = { \
		.desc = { \
			.id = _id, \
			.name = _name, \
			.ops = &footswitch_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
		}, \
		.pcom_id = _pcom_id, \
		.has_src_clk = _src_clk, \
		.src_clk_init_rate = _rate, \
		.has_ahb_clk = _ahb_clk, \
	}
static struct footswitch footswitches[] = {
	FOOTSWITCH(FS_GFX3D,  PCOM_FS_GRP,
		"fs_gfx3d",   true, 24576000, true),
	FOOTSWITCH(FS_GFX2D0, PCOM_FS_GRP_2D,
		"fs_gfx2d0", false, 24576000, true),
	FOOTSWITCH(FS_MDP,    PCOM_FS_MDP,
		"fs_mdp",    false, 24576000, true),
	FOOTSWITCH(FS_MFC,    PCOM_FS_MFC,
		"fs_mfc",    false, 24576000, true),
	FOOTSWITCH(FS_ROT,    PCOM_FS_ROTATOR,
		"fs_rot",    false,        0, true),
	FOOTSWITCH(FS_VFE,    PCOM_FS_VFE,
		"fs_vfe",    false, 24576000, true),
	FOOTSWITCH(FS_VPE,    PCOM_FS_VPE,
		"fs_vpe",    false, 24576000, false),
};

static int get_clocks(struct device *dev, struct footswitch *fs)
{
	int rc;

	/*
	 * Some SoCs may not have a separate rate-settable clock.
	 * If one can't be found, try to use the core clock for
	 * rate-setting instead.
	 */
	if (fs->has_src_clk) {
		fs->src_clk = clk_get(dev, "src_clk");
		if (IS_ERR(fs->src_clk))
			fs->src_clk = clk_get(dev, "core_clk");
	} else {
		fs->src_clk = clk_get(dev, "core_clk");
	}
	if (IS_ERR(fs->src_clk)) {
		pr_err("clk_get(src_clk) failed\n");
		rc = PTR_ERR(fs->src_clk);
		goto err_src_clk;
	}

	fs->core_clk = clk_get(dev, "core_clk");
	if (IS_ERR(fs->core_clk)) {
		pr_err("clk_get(core_clk) failed\n");
		rc = PTR_ERR(fs->core_clk);
		goto err_core_clk;
	}

	if (fs->has_ahb_clk) {
		fs->ahb_clk = clk_get(dev, "iface_clk");
		if (IS_ERR(fs->ahb_clk)) {
			pr_err("clk_get(iface_clk) failed\n");
			rc = PTR_ERR(fs->ahb_clk);
			goto err_ahb_clk;
		}
	}

	return 0;

err_ahb_clk:
	clk_put(fs->core_clk);
err_core_clk:
	clk_put(fs->src_clk);
err_src_clk:
	return rc;
}

static void put_clocks(struct footswitch *fs)
{
	clk_put(fs->src_clk);
	clk_put(fs->core_clk);
	clk_put(fs->ahb_clk);
}

static int footswitch_probe(struct platform_device *pdev)
{
	struct footswitch *fs;
	struct regulator_init_data *init_data;
	int rc;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id >= MAX_FS)
		return -ENODEV;

	fs = &footswitches[pdev->id];
	if (!fs->is_manual) {
		pr_err("%s is not in manual mode\n", fs->desc.name);
		return -EINVAL;
	}
	init_data = pdev->dev.platform_data;

	rc = get_clocks(&pdev->dev, fs);
	if (rc)
		return rc;

	fs->rdev = regulator_register(&fs->desc, &pdev->dev, init_data, fs);
	if (IS_ERR(fs->rdev)) {
		pr_err("regulator_register(%s) failed\n", fs->desc.name);
		rc = PTR_ERR(fs->rdev);
		goto err_register;
	}

	return 0;

err_register:
	put_clocks(fs);

	return rc;
}

static int __devexit footswitch_remove(struct platform_device *pdev)
{
	struct footswitch *fs = &footswitches[pdev->id];

	regulator_unregister(fs->rdev);
	set_rail_mode(fs->pcom_id, PCOM_RAIL_MODE_AUTO);
	put_clocks(fs);

	return 0;
}

static struct platform_driver footswitch_driver = {
	.probe		= footswitch_probe,
	.remove		= __devexit_p(footswitch_remove),
	.driver		= {
		.name		= "footswitch-pcom",
		.owner		= THIS_MODULE,
	},
};

static int __init footswitch_init(void)
{
	struct footswitch *fs;
	int ret;

	/*
	 * Enable all footswitches in manual mode (ie. not controlled along
	 * with pcom clocks).
	 */
	for (fs = footswitches; fs < footswitches + ARRAY_SIZE(footswitches);
	     fs++) {
		set_rail_state(fs->pcom_id, PCOM_CLKCTL_RPC_RAIL_ENABLE);
		ret = set_rail_mode(fs->pcom_id, PCOM_RAIL_MODE_MANUAL);
		if (!ret)
			fs->is_manual = 1;
	}

	return platform_driver_register(&footswitch_driver);
}
subsys_initcall(footswitch_init);

static void __exit footswitch_exit(void)
{
	platform_driver_unregister(&footswitch_driver);
}
module_exit(footswitch_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("proc_comm rail footswitch");
MODULE_ALIAS("platform:footswitch-pcom");
