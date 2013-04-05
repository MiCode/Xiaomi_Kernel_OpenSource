/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>
#include <mach/msm_iomap.h>
#include <mach/msm_bus.h>
#include <mach/scm-io.h>
#include <mach/clk.h>
#include <mach/rpm.h>

#include "footswitch.h"
#include "rpm_resources.h"

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
#define VCAP_GFS_CTL_REG	REG(0x0254)

#define CLAMP_BIT		BIT(5)
#define ENABLE_BIT		BIT(8)
#define RETENTION_BIT		BIT(9)

#define GFS_DELAY_CNT		31

#define DEFAULT_RESET_DELAY_US	1
/* Clock rate to use if one has not previously been set. */
#define DEFAULT_RATE		27000000
#define MAX_CLKS		10

/*
 * Lock is only needed to protect against the first footswitch_enable()
 * call occuring concurrently with late_footswitch_init().
 */
static DEFINE_MUTEX(claim_lock);

struct footswitch {
	struct regulator_dev	*rdev;
	struct regulator_desc	desc;
	void			*gfs_ctl_reg;
	int			bus_port0, bus_port1;
	bool			is_enabled;
	bool			is_claimed;
	struct fs_clk_data	*clk_data;
	struct clk		*core_clk;
	unsigned long		reset_delay_us;
};

static int setup_clocks(struct footswitch *fs)
{
	int rc = 0;
	struct fs_clk_data *clock;
	long rate;

	/*
	 * Enable all clocks in the power domain. If a specific clock rate is
	 * required for reset timing, set that rate before enabling the clocks.
	 */
	for (clock = fs->clk_data; clock->clk; clock++) {
		clock->rate = clk_get_rate(clock->clk);
		if (!clock->rate || clock->reset_rate) {
			rate = clock->reset_rate ?
					clock->reset_rate : DEFAULT_RATE;
			rc = clk_set_rate(clock->clk, rate);
			if (rc && rc != -ENOSYS) {
				pr_err("Failed to set %s %s rate to %lu Hz.\n",
				       fs->desc.name, clock->name, clock->rate);
				for (clock--; clock >= fs->clk_data; clock--) {
					if (clock->enabled)
						clk_disable_unprepare(
								clock->clk);
					clk_set_rate(clock->clk, clock->rate);
				}
				return rc;
			}
		}
		/*
		 * Some clocks are for reset purposes only. These clocks will
		 * fail to enable. Ignore the failures but keep track of them so
		 * we don't try to disable them later and crash due to
		 * unbalanced calls.
		 */
		clock->enabled = !clk_prepare_enable(clock->clk);
	}

	return 0;
}

static void restore_clocks(struct footswitch *fs)
{
	struct fs_clk_data *clock;

	/* Restore clocks to their orignal states before setup_clocks(). */
	for (clock = fs->clk_data; clock->clk; clock++) {
		if (clock->enabled)
			clk_disable_unprepare(clock->clk);
		if (clock->rate && clk_set_rate(clock->clk, clock->rate))
			pr_err("Failed to restore %s %s rate to %lu Hz.\n",
			       fs->desc.name, clock->name, clock->rate);
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
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	mutex_lock(&claim_lock);
	fs->is_claimed = true;
	mutex_unlock(&claim_lock);

	/* Return early if already enabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & (ENABLE_BIT | CLAMP_BIT)) == ENABLE_BIT)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/* Un-halt all bus ports in the power domain. */
	if (fs->bus_port0) {
		rc = msm_bus_axi_portunhalt(fs->bus_port0);
		if (rc) {
			pr_err("%s port 0 unhalt failed.\n", fs->desc.name);
			goto err;
		}
	}
	if (fs->bus_port1) {
		rc = msm_bus_axi_portunhalt(fs->bus_port1);
		if (rc) {
			pr_err("%s port 1 unhalt failed.\n", fs->desc.name);
			goto err_port2_halt;
		}
	}

	/*
	 * (Re-)Assert resets for all clocks in the clock domain, since
	 * footswitch_enable() is first called before footswitch_disable()
	 * and resets should be asserted before power is restored.
	 */
	for (clock = fs->clk_data; clock->clk; clock++)
		; /* Do nothing */
	for (clock--; clock >= fs->clk_data; clock--)
		clk_reset(clock->clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(fs->reset_delay_us);

	/* Enable the power rail at the footswitch. */
	regval |= ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);
	/* Wait for the rail to fully charge. */
	mb();
	udelay(1);

	/* Un-clamp the I/O ports. */
	regval &= ~CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Deassert resets for all clocks in the power domain. */
	for (clock = fs->clk_data; clock->clk; clock++)
		clk_reset(clock->clk, CLK_RESET_DEASSERT);
	/* Toggle core reset again after first power-on (required for GFX3D). */
	if (fs->desc.id == FS_GFX3D) {
		clk_reset(fs->core_clk, CLK_RESET_ASSERT);
		udelay(fs->reset_delay_us);
		clk_reset(fs->core_clk, CLK_RESET_DEASSERT);
		udelay(fs->reset_delay_us);
	}

	/* Prevent core memory from collapsing when its clock is gated. */
	clk_set_flags(fs->core_clk, CLKFLAG_RETAIN_MEM);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = true;
	return 0;

err_port2_halt:
	msm_bus_axi_porthalt(fs->bus_port0);
err:
	restore_clocks(fs);
	return rc;
}

static int footswitch_disable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	/* Return early if already disabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & ENABLE_BIT) == 0)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/* Allow core memory to collapse when its clock is gated. */
	if (fs->desc.id != FS_GFX3D_8064)
		clk_set_flags(fs->core_clk, CLKFLAG_NORETAIN_MEM);

	/* Halt all bus ports in the power domain. */
	if (fs->bus_port0) {
		rc = msm_bus_axi_porthalt(fs->bus_port0);
		if (rc) {
			pr_err("%s port 0 halt failed.\n", fs->desc.name);
			goto err;
		}
	}
	if (fs->bus_port1) {
		rc = msm_bus_axi_porthalt(fs->bus_port1);
		if (rc) {
			pr_err("%s port 1 halt failed.\n", fs->desc.name);
			goto err_port2_halt;
		}
	}

	/*
	 * Assert resets for all clocks in the clock domain so that
	 * outputs settle prior to clamping.
	 */
	for (clock = fs->clk_data; clock->clk; clock++)
		; /* Do nothing */
	for (clock--; clock >= fs->clk_data; clock--)
		clk_reset(clock->clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(fs->reset_delay_us);

	/*
	 * Return clocks to their state before this function. For robustness
	 * if memory-retention across collapses is required, clocks should
	 * be disabled before asserting the clamps. Assuming clocks were off
	 * before entering footswitch_disable(), this will be true.
	 */
	restore_clocks(fs);

	/*
	 * Clamp the I/O ports of the core to ensure the values
	 * remain fixed while the core is collapsed.
	 */
	regval |= CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Collapse the power rail at the footswitch. */
	regval &= ~ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	fs->is_enabled = false;
	return 0;

err_port2_halt:
	msm_bus_axi_portunhalt(fs->bus_port0);
err:
	clk_set_flags(fs->core_clk, CLKFLAG_RETAIN_MEM);
	restore_clocks(fs);
	return rc;
}

static int gfx2d_footswitch_enable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	mutex_lock(&claim_lock);
	fs->is_claimed = true;
	mutex_unlock(&claim_lock);

	/* Return early if already enabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & (ENABLE_BIT | CLAMP_BIT)) == ENABLE_BIT)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/* Un-halt all bus ports in the power domain. */
	if (fs->bus_port0) {
		rc = msm_bus_axi_portunhalt(fs->bus_port0);
		if (rc) {
			pr_err("%s port 0 unhalt failed.\n", fs->desc.name);
			goto err;
		}
	}

	/* Disable core clock. */
	clk_disable_unprepare(fs->core_clk);

	/*
	 * (Re-)Assert resets for all clocks in the clock domain, since
	 * footswitch_enable() is first called before footswitch_disable()
	 * and resets should be asserted before power is restored.
	 */
	for (clock = fs->clk_data; clock->clk; clock++)
		; /* Do nothing */
	for (clock--; clock >= fs->clk_data; clock--)
		clk_reset(clock->clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(fs->reset_delay_us);

	/* Enable the power rail at the footswitch. */
	regval |= ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);
	mb();
	udelay(1);

	/* Un-clamp the I/O ports. */
	regval &= ~CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Deassert resets for all clocks in the power domain. */
	for (clock = fs->clk_data; clock->clk; clock++)
		clk_reset(clock->clk, CLK_RESET_DEASSERT);
	udelay(fs->reset_delay_us);

	/* Re-enable core clock. */
	clk_prepare_enable(fs->core_clk);

	/* Prevent core memory from collapsing when its clock is gated. */
	clk_set_flags(fs->core_clk, CLKFLAG_RETAIN_MEM);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = true;
	return 0;

err:
	restore_clocks(fs);
	return rc;
}

static int gfx2d_footswitch_disable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	/* Return early if already disabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & ENABLE_BIT) == 0)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/* Allow core memory to collapse when its clock is gated. */
	clk_set_flags(fs->core_clk, CLKFLAG_NORETAIN_MEM);

	/* Halt all bus ports in the power domain. */
	if (fs->bus_port0) {
		rc = msm_bus_axi_porthalt(fs->bus_port0);
		if (rc) {
			pr_err("%s port 0 halt failed.\n", fs->desc.name);
			goto err;
		}
	}

	/* Disable core clock. */
	clk_disable_unprepare(fs->core_clk);

	/*
	 * Assert resets for all clocks in the clock domain so that
	 * outputs settle prior to clamping.
	 */
	for (clock = fs->clk_data; clock->clk; clock++)
		; /* Do nothing */
	for (clock--; clock >= fs->clk_data; clock--)
		clk_reset(clock->clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(5);

	/*
	 * Clamp the I/O ports of the core to ensure the values
	 * remain fixed while the core is collapsed.
	 */
	regval |= CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Collapse the power rail at the footswitch. */
	regval &= ~ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Re-enable core clock. */
	clk_prepare_enable(fs->core_clk);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = false;
	return 0;

err:
	clk_set_flags(fs->core_clk, CLKFLAG_RETAIN_MEM);
	restore_clocks(fs);
	return rc;
}

static void force_bus_clocks(bool enforce)
{
	static struct msm_rpm_iv_pair iv;
	int ret;

	if (enforce) {
		iv.id = MSM_RPM_STATUS_ID_RPM_CTL;
		ret = msm_rpm_get_status(&iv, 1);
		if (ret)
			pr_err("Failed to read RPM_CTL resource status\n");

		iv.id = MSM_RPM_ID_RPM_CTL;
		iv.value |= BIT(6);
	} else {
		iv.id = MSM_RPM_ID_RPM_CTL;
		iv.value &= ~BIT(6);
	}

	ret = msm_rpmrs_set(MSM_RPM_CTX_SET_0, &iv, 1);
	if (ret)
		pr_err("Force bus clocks request=%d failed\n", enforce);
}

static int gfx3d_8064_footswitch_enable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	mutex_lock(&claim_lock);
	fs->is_claimed = true;
	mutex_unlock(&claim_lock);

	/* Return early if already enabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & (ENABLE_BIT | CLAMP_BIT)) == ENABLE_BIT)
		return 0;

	/* Un-halt all bus ports in the power domain. */
	if (fs->bus_port0) {
		rc = msm_bus_axi_portunhalt(fs->bus_port0);
		if (rc) {
			pr_err("%s port 0 unhalt failed.\n", fs->desc.name);
			goto err;
		}
	}
	if (fs->bus_port1) {
		rc = msm_bus_axi_portunhalt(fs->bus_port1);
		if (rc) {
			pr_err("%s port 1 unhalt failed.\n", fs->desc.name);
			goto err_port2_halt;
		}
	}

	/* Apply AFAB/EBI clock limits. */
	force_bus_clocks(true);

	/* Enable the power rail at the footswitch. */
	regval |= ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);
	/* Wait for the rail to fully charge. */
	mb();
	udelay(1);

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		goto err_setup_clocks;

	/*
	 * (Re-)Assert resets for all clocks in the clock domain, since
	 * footswitch_enable() is first called before footswitch_disable()
	 * and resets should be asserted before power is restored.
	 */
	for (clock = fs->clk_data; clock->clk; clock++)
		; /* Do nothing */
	for (clock--; clock >= fs->clk_data; clock--)
		clk_reset(clock->clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(fs->reset_delay_us);

	/* Un-clamp the I/O ports. */
	regval &= ~CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Deassert resets for all clocks in the power domain. */
	for (clock = fs->clk_data; clock->clk; clock++)
		clk_reset(clock->clk, CLK_RESET_DEASSERT);

	/* Prevent core memory from collapsing when its clock is gated. */
	clk_set_flags(fs->core_clk, CLKFLAG_RETAIN_MEM);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	/* Remove AFAB/EBI clock limits after any transients have settled. */
	udelay(30);
	force_bus_clocks(false);

	fs->is_enabled = true;
	return 0;

err_setup_clocks:
	regval &= ~ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);
	force_bus_clocks(false);
	msm_bus_axi_porthalt(fs->bus_port1);
err_port2_halt:
	msm_bus_axi_porthalt(fs->bus_port0);
err:
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

static struct regulator_ops gfx3d_8064_fs_ops = {
	.is_enabled = footswitch_is_enabled,
	.enable = gfx3d_8064_footswitch_enable,
	.disable = footswitch_disable,
};

#define FOOTSWITCH(_id, _name, _ops, _gfs_ctl_reg) \
	[(_id)] = { \
		.desc = { \
			.id = (_id), \
			.name = (_name), \
			.ops = (_ops), \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
		}, \
		.gfs_ctl_reg = (_gfs_ctl_reg), \
	}
static struct footswitch footswitches[] = {
	FOOTSWITCH(FS_GFX2D0, "fs_gfx2d0", &gfx2d_fs_ops, GFX2D0_GFS_CTL_REG),
	FOOTSWITCH(FS_GFX2D1, "fs_gfx2d1", &gfx2d_fs_ops, GFX2D1_GFS_CTL_REG),
	FOOTSWITCH(FS_GFX3D_8064, "fs_gfx3d", &gfx3d_8064_fs_ops,
		   GFX3D_GFS_CTL_REG),
	FOOTSWITCH(FS_GFX3D,  "fs_gfx3d", &standard_fs_ops, GFX3D_GFS_CTL_REG),
	FOOTSWITCH(FS_IJPEG,  "fs_ijpeg", &standard_fs_ops, GEMINI_GFS_CTL_REG),
	FOOTSWITCH(FS_MDP,    "fs_mdp",   &standard_fs_ops, MDP_GFS_CTL_REG),
	FOOTSWITCH(FS_ROT,    "fs_rot",   &standard_fs_ops, ROT_GFS_CTL_REG),
	FOOTSWITCH(FS_VED,    "fs_ved",   &standard_fs_ops, VED_GFS_CTL_REG),
	FOOTSWITCH(FS_VFE,    "fs_vfe",   &standard_fs_ops, VFE_GFS_CTL_REG),
	FOOTSWITCH(FS_VPE,    "fs_vpe",   &standard_fs_ops, VPE_GFS_CTL_REG),
	FOOTSWITCH(FS_VCAP,   "fs_vcap",  &standard_fs_ops, VCAP_GFS_CTL_REG),
};

static int footswitch_probe(struct platform_device *pdev)
{
	struct footswitch *fs;
	struct regulator_init_data *init_data;
	struct fs_driver_data *driver_data;
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id >= MAX_FS)
		return -ENODEV;

	init_data = pdev->dev.platform_data;
	driver_data = init_data->driver_data;
	fs = &footswitches[pdev->id];
	fs->clk_data = driver_data->clks;
	fs->bus_port0 = driver_data->bus_port0;
	fs->bus_port1 = driver_data->bus_port1;
	fs->reset_delay_us =
		driver_data->reset_delay_us ? : DEFAULT_RESET_DELAY_US;

	for (clock = fs->clk_data; clock->name; clock++) {
		clock->clk = clk_get(&pdev->dev, clock->name);
		if (IS_ERR(clock->clk)) {
			rc = PTR_ERR(clock->clk);
			pr_err("%s clk_get(%s) failed\n", fs->desc.name,
			       clock->name);
			goto err;
		}
		if (!strncmp(clock->name, "core_clk", 8))
			fs->core_clk = clock->clk;
	}

	/*
	 * Set number of AHB_CLK cycles to delay the assertion of gfs_en_all
	 * after enabling the footswitch.  Also ensure the retention bit is
	 * clear so disabling the footswitch will power-collapse the core.
	 */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	regval |= GFS_DELAY_CNT;
	regval &= ~RETENTION_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	fs->rdev = regulator_register(&fs->desc, &pdev->dev,
							init_data, fs, NULL);
	if (IS_ERR(footswitches[pdev->id].rdev)) {
		pr_err("regulator_register(\"%s\") failed\n",
			fs->desc.name);
		rc = PTR_ERR(footswitches[pdev->id].rdev);
		goto err;
	}

	return 0;

err:
	for (clock = fs->clk_data; clock->clk; clock++)
		clk_put(clock->clk);

	return rc;
}

static int __devexit footswitch_remove(struct platform_device *pdev)
{
	struct footswitch *fs = &footswitches[pdev->id];
	struct fs_clk_data *clock;

	for (clock = fs->clk_data; clock->clk; clock++)
		clk_put(clock->clk);
	regulator_unregister(fs->rdev);

	return 0;
}

static struct platform_driver footswitch_driver = {
	.probe		= footswitch_probe,
	.remove		= __devexit_p(footswitch_remove),
	.driver		= {
		.name		= "footswitch-8x60",
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
