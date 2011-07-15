/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>
#include <mach/msm_iomap.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>
#include <mach/scm-io.h>
#include "clock.h"
#include "footswitch.h"

#ifdef CONFIG_MSM_SECURE_IO
#undef readl_relaxed
#undef writel_relaxed
#define readl_relaxed secure_readl
#define writel_relaxed secure_writel
#endif

#define REG(off) (MSM_MMSS_CLK_CTL_BASE + (off))
#define GEMINI_GFS_CTL_REG	REG(0x01A0)
#define GFX2D0_GFS_CTL_REG	REG(0x0180)
#define GFX2D1_GFS_CTL_REG	REG(0x0184)
#define GFX3D_GFS_CTL_REG	REG(0x0188)
#define MDP_GFS_CTL_REG		REG(0x0190)
#define ROT_GFS_CTL_REG		REG(0x018C)
#define VED_GFS_CTL_REG		REG(0x0194)
#define VFE_GFS_CTL_REG		REG(0x0198)
#define VPE_GFS_CTL_REG		REG(0x019C)

#define CLAMP_BIT		BIT(5)
#define ENABLE_BIT		BIT(8)
#define RETENTION_BIT		BIT(9)

#define RESET_DELAY_US		1
/* Core clock rate to use if one has not previously been set. */
#define DEFAULT_CLK_RATE	27000000

/*
 * Lock is only needed to protect against the first footswitch_enable()
 * call occuring concurrently with late_footswitch_init().
 */
static DEFINE_MUTEX(claim_lock);

struct clock_state {
	int ahb_clk_en;
	int axi_clk_en;
	int core_clk_rate;
};

struct footswitch {
	struct regulator_dev	*rdev;
	struct regulator_desc	desc;
	void			*gfs_ctl_reg;
	int			bus_port1, bus_port2;
	bool			is_enabled;
	bool			is_claimed;
	const char		*core_clk_name;
	const char		*ahb_clk_name;
	const char		*axi_clk_name;
	struct clk		*core_clk;
	struct clk		*ahb_clk;
	struct clk		*axi_clk;
	unsigned int		reset_rate;
	struct clock_state	clk_state;
	unsigned int		gfs_delay_cnt:5;
};

static int setup_clocks(struct footswitch *fs)
{
	int rc = 0;

	/*
	 * Enable all clocks in the power domain. If a core requires a
	 * specific clock rate when being reset, apply it.
	 */
	fs->clk_state.core_clk_rate = clk_get_rate(fs->core_clk);
	if (!fs->clk_state.core_clk_rate || fs->reset_rate) {
		int rate = fs->reset_rate ? fs->reset_rate : DEFAULT_CLK_RATE;
		rc = clk_set_rate(fs->core_clk, rate);
		if (rc) {
			pr_err("%s: Failed to set %s rate to %d Hz.\n",
				__func__, fs->core_clk_name,
				fs->reset_rate);
			return rc;
		}
	}
	clk_enable(fs->core_clk);

	/*
	 * Some AHB and AXI clocks are for reset purposes only. These clocks
	 * will fail to enable. Keep track of them so we don't try to disable
	 * them later and crash.
	 */
	fs->clk_state.ahb_clk_en = !clk_enable(fs->ahb_clk);
	if (fs->axi_clk)
		fs->clk_state.axi_clk_en = !clk_enable(fs->axi_clk);

	return rc;
}

static void restore_clocks(struct footswitch *fs)
{
	/* Restore clocks to their orignal states before setup_clocks(). */
	if (fs->axi_clk && fs->clk_state.axi_clk_en)
		clk_disable(fs->axi_clk);
	if (fs->clk_state.ahb_clk_en)
		clk_disable(fs->ahb_clk);
	clk_disable(fs->core_clk);
	if (fs->clk_state.core_clk_rate) {
		if (clk_set_rate(fs->core_clk, fs->clk_state.core_clk_rate))
			pr_err("%s: Failed to restore %s rate.\n",
					__func__, fs->core_clk_name);
	}
}

static int footswitch_is_enabled(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);

	return fs->is_enabled;
}

static int footswitch_enable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	uint32_t regval, rc = 0;

	mutex_lock(&claim_lock);
	fs->is_claimed = true;
	mutex_unlock(&claim_lock);

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		goto out;

	/* Un-halt all bus ports in the power domain. */
	if (fs->bus_port1) {
		rc = msm_bus_axi_portunhalt(fs->bus_port1);
		if (rc) {
			pr_err("%s: Port 1 unhalt failed.\n", __func__);
			goto out;
		}
	}
	if (fs->bus_port2) {
		rc = msm_bus_axi_portunhalt(fs->bus_port2);
		if (rc) {
			pr_err("%s: Port 2 unhalt failed.\n", __func__);
			goto out;
		}
	}

	/*
	 * (Re-)Assert resets for all clocks in the clock domain, since
	 * footswitch_enable() is first called before footswitch_disable()
	 * and resets should be asserted before power is restored.
	 */
	if (fs->axi_clk)
		clk_reset(fs->axi_clk, CLK_RESET_ASSERT);
	clk_reset(fs->ahb_clk, CLK_RESET_ASSERT);
	clk_reset(fs->core_clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(RESET_DELAY_US);

	/* Enable the power rail at the footswitch. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	regval |= ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);
	/* Wait for the rail to fully charge. */
	mb();
	udelay(1);

	/* Un-clamp the I/O ports. */
	regval &= ~CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Deassert resets for all clocks in the power domain. */
	clk_reset(fs->core_clk, CLK_RESET_DEASSERT);
	clk_reset(fs->ahb_clk, CLK_RESET_DEASSERT);
	if (fs->axi_clk)
		clk_reset(fs->axi_clk, CLK_RESET_DEASSERT);
	/* Toggle core reset again after first power-on (required for GFX3D). */
	if (fs->desc.id == FS_GFX3D) {
		clk_reset(fs->core_clk, CLK_RESET_ASSERT);
		udelay(RESET_DELAY_US);
		clk_reset(fs->core_clk, CLK_RESET_DEASSERT);
		udelay(RESET_DELAY_US);
	}

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = true;
out:
	return rc;
}

static int footswitch_disable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	uint32_t regval, rc = 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		goto out;

	/* Halt all bus ports in the power domain. */
	if (fs->bus_port1) {
		rc = msm_bus_axi_porthalt(fs->bus_port1);
		if (rc) {
			pr_err("%s: Port 1 halt failed.\n", __func__);
			goto out;
		}
	}
	if (fs->bus_port2) {
		rc = msm_bus_axi_porthalt(fs->bus_port2);
		if (rc) {
			pr_err("%s: Port 1 halt failed.\n", __func__);
			goto err_port2_halt;
		}
	}

	/*
	 * Assert resets for all clocks in the clock domain so that
	 * outputs settle prior to clamping.
	 */
	if (fs->axi_clk)
		clk_reset(fs->axi_clk, CLK_RESET_ASSERT);
	clk_reset(fs->ahb_clk, CLK_RESET_ASSERT);
	clk_reset(fs->core_clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(RESET_DELAY_US);

	/*
	 * Clamp the I/O ports of the core to ensure the values
	 * remain fixed while the core is collapsed.
	 */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	regval |= CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Collapse the power rail at the footswitch. */
	regval &= ~ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = false;

	return rc;

err_port2_halt:
	msm_bus_axi_portunhalt(fs->bus_port1);
out:
	return rc;
}

static int gfx2d_footswitch_enable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	uint32_t regval, rc = 0;

	mutex_lock(&claim_lock);
	fs->is_claimed = true;
	mutex_unlock(&claim_lock);

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		goto out;

	/* Un-halt all bus ports in the power domain. */
	if (fs->bus_port1) {
		rc = msm_bus_axi_portunhalt(fs->bus_port1);
		if (rc) {
			pr_err("%s: Port 1 unhalt failed.\n", __func__);
			goto out;
		}
	}

	/* Disable core clock. */
	clk_disable(fs->core_clk);

	/*
	 * (Re-)Assert resets for all clocks in the clock domain, since
	 * footswitch_enable() is first called before footswitch_disable()
	 * and resets should be asserted before power is restored.
	 */
	if (fs->axi_clk)
		clk_reset(fs->axi_clk, CLK_RESET_ASSERT);
	clk_reset(fs->ahb_clk, CLK_RESET_ASSERT);
	clk_reset(fs->core_clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(20);

	/* Enable the power rail at the footswitch. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	regval |= ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);
	mb();
	udelay(1);

	/* Un-clamp the I/O ports. */
	regval &= ~CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Deassert resets for all clocks in the power domain. */
	if (fs->axi_clk)
		clk_reset(fs->axi_clk, CLK_RESET_DEASSERT);
	clk_reset(fs->ahb_clk, CLK_RESET_DEASSERT);
	clk_reset(fs->core_clk, CLK_RESET_DEASSERT);
	udelay(20);

	/* Re-enable core clock. */
	clk_enable(fs->core_clk);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = true;
out:
	return rc;
}

static int gfx2d_footswitch_disable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	uint32_t regval, rc = 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		goto out;

	/* Halt all bus ports in the power domain. */
	if (fs->bus_port1) {
		rc = msm_bus_axi_porthalt(fs->bus_port1);
		if (rc) {
			pr_err("%s: Port 1 halt failed.\n", __func__);
			goto out;
		}
	}

	/* Disable core clock. */
	clk_disable(fs->core_clk);

	/*
	 * Assert resets for all clocks in the clock domain so that
	 * outputs settle prior to clamping.
	 */
	if (fs->axi_clk)
		clk_reset(fs->axi_clk, CLK_RESET_ASSERT);
	clk_reset(fs->ahb_clk, CLK_RESET_ASSERT);
	clk_reset(fs->core_clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(20);

	/*
	 * Clamp the I/O ports of the core to ensure the values
	 * remain fixed while the core is collapsed.
	 */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	regval |= CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Collapse the power rail at the footswitch. */
	regval &= ~ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Re-enable core clock. */
	clk_enable(fs->core_clk);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = false;

out:
	return rc;
}

static struct regulator_ops standard_fs_ops = {
	.is_enabled = footswitch_is_enabled,
	.enable = footswitch_enable,
	.disable = footswitch_disable,
};

static struct regulator_ops gfx2d_fs_ops = {
	.is_enabled = footswitch_is_enabled,
	.enable = gfx2d_footswitch_enable,
	.disable = gfx2d_footswitch_disable,
};

#define FOOTSWITCH(_id, _name, _ops, _gfs_ctl_reg, _dc, _bp1, _bp2, \
		   _core_clk, _ahb_clk, _axi_clk, _reset_rate) \
	[(_id)] = { \
		.desc = { \
			.id = (_id), \
			.name = (_name), \
			.ops = (_ops), \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
		}, \
		.gfs_ctl_reg = (_gfs_ctl_reg), \
		.gfs_delay_cnt = (_dc), \
		.bus_port1 = (_bp1), \
		.bus_port2 = (_bp2), \
		.core_clk_name = (_core_clk), \
		.ahb_clk_name = (_ahb_clk), \
		.axi_clk_name = (_axi_clk), \
		.reset_rate = (_reset_rate), \
	}
static struct footswitch footswitches[] = {
	FOOTSWITCH(FS_GFX2D0, "fs_gfx2d0", &gfx2d_fs_ops,
		GFX2D0_GFS_CTL_REG, 31,
		MSM_BUS_MASTER_GRAPHICS_2D_CORE0, 0,
		"gfx2d0_clk", "gfx2d0_pclk", NULL, 0),
	FOOTSWITCH(FS_GFX2D1, "fs_gfx2d1", &gfx2d_fs_ops,
		GFX2D1_GFS_CTL_REG, 31,
		MSM_BUS_MASTER_GRAPHICS_2D_CORE1, 0,
		"gfx2d1_clk", "gfx2d1_pclk", NULL, 0),
	FOOTSWITCH(FS_GFX3D, "fs_gfx3d", &standard_fs_ops,
		GFX3D_GFS_CTL_REG, 31,
		MSM_BUS_MASTER_GRAPHICS_3D, 0,
		"gfx3d_clk", "gfx3d_pclk", NULL, 27000000),
	FOOTSWITCH(FS_IJPEG, "fs_ijpeg", &standard_fs_ops,
		GEMINI_GFS_CTL_REG, 31,
		MSM_BUS_MASTER_JPEG_ENC, 0,
		"ijpeg_clk", "ijpeg_pclk", "ijpeg_axi_clk", 0),
	FOOTSWITCH(FS_MDP, "fs_mdp", &standard_fs_ops,
		MDP_GFS_CTL_REG, 31,
		MSM_BUS_MASTER_MDP_PORT0,
		MSM_BUS_MASTER_MDP_PORT1,
		"mdp_clk", "mdp_pclk", "mdp_axi_clk", 0),
	FOOTSWITCH(FS_ROT, "fs_rot", &standard_fs_ops,
		ROT_GFS_CTL_REG, 31,
		MSM_BUS_MASTER_ROTATOR, 0,
		"rot_clk", "rotator_pclk", "rot_axi_clk", 0),
	FOOTSWITCH(FS_VED, "fs_ved", &standard_fs_ops,
		VED_GFS_CTL_REG, 31,
		MSM_BUS_MASTER_HD_CODEC_PORT0,
		MSM_BUS_MASTER_HD_CODEC_PORT1,
		"vcodec_clk", "vcodec_pclk", "vcodec_axi_clk", 0),
	FOOTSWITCH(FS_VFE, "fs_vfe", &standard_fs_ops,
		VFE_GFS_CTL_REG, 31,
		MSM_BUS_MASTER_VFE, 0,
		"vfe_clk", "vfe_pclk", "vfe_axi_clk", 0),
	FOOTSWITCH(FS_VPE, "fs_vpe", &standard_fs_ops,
		VPE_GFS_CTL_REG, 31,
		MSM_BUS_MASTER_VPE, 0,
		"vpe_clk", "vpe_pclk", "vpe_axi_clk", 0),
};

static int footswitch_probe(struct platform_device *pdev)
{
	struct footswitch *fs;
	struct regulator_init_data *init_data;
	uint32_t regval, rc = 0;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id >= MAX_FS)
		return -ENODEV;

	fs = &footswitches[pdev->id];
	init_data = pdev->dev.platform_data;

	/* Setup core clock. */
	fs->core_clk = clk_get(NULL, fs->core_clk_name);
	if (IS_ERR(fs->core_clk)) {
		pr_err("%s: clk_get(\"%s\") failed\n", __func__,
						fs->core_clk_name);
		rc = PTR_ERR(fs->core_clk);
		goto err_core_clk;
	}

	/* Setup AHB clock. */
	fs->ahb_clk = clk_get(NULL, fs->ahb_clk_name);
	if (IS_ERR(fs->ahb_clk)) {
		pr_err("%s: clk_get(\"%s\") failed\n", __func__,
						fs->ahb_clk_name);
		rc = PTR_ERR(fs->ahb_clk);
		goto err_ahb_clk;
	}

	/* Setup AXI clock. */
	if (fs->axi_clk_name) {
		fs->axi_clk = clk_get(NULL, fs->axi_clk_name);
		if (IS_ERR(fs->axi_clk)) {
			pr_err("%s: clk_get(\"%s\") failed\n", __func__,
						fs->axi_clk_name);
			rc = PTR_ERR(fs->axi_clk);
			goto err_axi_clk;
		}
	}

	/*
	 * Set number of AHB_CLK cycles to delay the assertion of gfs_en_all
	 * after enabling the footswitch.  Also ensure the retention bit is
	 * clear so disabling the footswitch will power-collapse the core.
	 */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	regval |= fs->gfs_delay_cnt;
	regval &= ~RETENTION_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	fs->rdev = regulator_register(&fs->desc, &pdev->dev, init_data, fs);
	if (IS_ERR(footswitches[pdev->id].rdev)) {
		pr_err("%s: regulator_register(\"%s\") failed\n",
			__func__, fs->desc.name);
		rc = PTR_ERR(footswitches[pdev->id].rdev);
		goto err_register;
	}

	return 0;

err_register:
	if (fs->axi_clk_name)
		clk_put(fs->axi_clk);
err_axi_clk:
	clk_put(fs->ahb_clk);
err_ahb_clk:
	clk_put(fs->core_clk);
err_core_clk:
	return rc;
}

static int __devexit footswitch_remove(struct platform_device *pdev)
{
	struct footswitch *fs = &footswitches[pdev->id];

	clk_put(fs->core_clk);
	clk_put(fs->ahb_clk);
	if (fs->axi_clk)
		clk_put(fs->axi_clk);

	regulator_unregister(fs->rdev);

	return 0;
}

static struct platform_driver footswitch_driver = {
	.probe		= footswitch_probe,
	.remove		= __devexit_p(footswitch_remove),
	.driver		= {
		.name		= "footswitch-msm8x60",
		.owner		= THIS_MODULE,
	},
};

static int __init late_footswitch_init(void)
{
	int i;

	mutex_lock(&claim_lock);
	/* Turn off all registered but unused footswitches. */
	for (i = 0; i < ARRAY_SIZE(footswitches); i++)
		if (footswitches[i].rdev && !footswitches[i].is_claimed)
			footswitches[i].rdev->desc->ops->
				disable(footswitches[i].rdev);
	mutex_unlock(&claim_lock);

	return 0;
}
late_initcall(late_footswitch_init);

static int __init footswitch_init(void)
{
	return platform_driver_register(&footswitch_driver);
}
subsys_initcall(footswitch_init);

static void __exit footswitch_exit(void)
{
	platform_driver_unregister(&footswitch_driver);
}
module_exit(footswitch_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM8x60 rail footswitch");
MODULE_ALIAS("platform:footswitch-msm8x60");
