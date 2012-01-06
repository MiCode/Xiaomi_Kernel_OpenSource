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

#define pr_fmt(fmt) "%s: " fmt, __func__

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
#include <mach/socinfo.h>
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
#define VCAP_GFS_CTL_REG	REG(0x0254)

#define CLAMP_BIT		BIT(5)
#define ENABLE_BIT		BIT(8)
#define RETENTION_BIT		BIT(9)

#define RESET_DELAY_US		1
/* Clock rate to use if one has not previously been set. */
#define DEFAULT_RATE		27000000
#define MAX_CLKS		10

/*
 * Lock is only needed to protect against the first footswitch_enable()
 * call occuring concurrently with late_footswitch_init().
 */
static DEFINE_MUTEX(claim_lock);

struct clk_data {
	const char *name;
	struct clk *clk;
	unsigned long rate;
	unsigned long reset_rate;
	bool enabled;
};

struct footswitch {
	struct regulator_dev	*rdev;
	struct regulator_desc	desc;
	void			*gfs_ctl_reg;
	int			bus_port0, bus_port1;
	bool			is_enabled;
	bool			is_claimed;
	struct clk_data		*clk_data;
	struct clk		*core_clk;
	unsigned int		gfs_delay_cnt:5;
};

static int setup_clocks(struct footswitch *fs)
{
	int rc = 0;
	struct clk_data *clock;
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
				pr_err("Failed to set %s rate to %lu Hz.\n",
					clock->name, clock->rate);
				for (clock--; clock >= fs->clk_data; clock--) {
					if (clock->enabled)
						clk_disable(clock->clk);
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
		clock->enabled = !clk_enable(clock->clk);
	}

	return 0;
}

static void restore_clocks(struct footswitch *fs)
{
	struct clk_data *clock;

	/* Restore clocks to their orignal states before setup_clocks(). */
	for (clock = fs->clk_data; clock->clk; clock++) {
		if (clock->enabled)
			clk_disable(clock->clk);
		if (clock->rate && clk_set_rate(clock->clk, clock->rate))
			pr_err("Failed to restore %s rate to %lu Hz.\n",
				clock->name, clock->rate);
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
	struct clk_data *clock;
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
			pr_err("Port 0 unhalt failed.\n");
			goto err;
		}
	}
	if (fs->bus_port1) {
		rc = msm_bus_axi_portunhalt(fs->bus_port1);
		if (rc) {
			pr_err("Port 1 unhalt failed.\n");
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
	udelay(RESET_DELAY_US);

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
		udelay(RESET_DELAY_US);
		clk_reset(fs->core_clk, CLK_RESET_DEASSERT);
		udelay(RESET_DELAY_US);
	}

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
	struct clk_data *clock;
	uint32_t regval, rc = 0;

	/* Return early if already disabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & ENABLE_BIT) == 0)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/* Halt all bus ports in the power domain. */
	if (fs->bus_port0) {
		rc = msm_bus_axi_porthalt(fs->bus_port0);
		if (rc) {
			pr_err("Port 0 halt failed.\n");
			goto err;
		}
	}
	if (fs->bus_port1) {
		rc = msm_bus_axi_porthalt(fs->bus_port1);
		if (rc) {
			pr_err("Port 1 halt failed.\n");
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
	udelay(RESET_DELAY_US);

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
	restore_clocks(fs);
	return rc;
}

static int gfx2d_footswitch_enable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	struct clk_data *clock;
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
			pr_err("Port 0 unhalt failed.\n");
			goto err;
		}
	}

	/* Disable core clock. */
	clk_disable(fs->core_clk);

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
	udelay(RESET_DELAY_US);

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
	udelay(RESET_DELAY_US);

	/* Re-enable core clock. */
	clk_enable(fs->core_clk);

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
	struct clk_data *clock;
	uint32_t regval, rc = 0;

	/* Return early if already disabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & ENABLE_BIT) == 0)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/* Halt all bus ports in the power domain. */
	if (fs->bus_port0) {
		rc = msm_bus_axi_porthalt(fs->bus_port0);
		if (rc) {
			pr_err("Port 0 halt failed.\n");
			goto err;
		}
	}

	/* Disable core clock. */
	clk_disable(fs->core_clk);

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
	clk_enable(fs->core_clk);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = false;
	return 0;

err:
	restore_clocks(fs);
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

/*
 * Lists of required clocks for the collapse and restore sequences.
 *
 * Order matters here. Clocks are listed in the same order as their
 * resets will be de-asserted when the core is restored. Also, rate-
 * settable clocks must be listed before any of the branches that
 * are derived from them. Otherwise, the branches may fail to enable
 * if their parent's rate is not yet set.
 */

static struct clk_data gfx2d0_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ 0 }
};

static struct clk_data gfx2d1_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ 0 }
};

static struct clk_data gfx3d_clks[] = {
	{ .name = "core_clk", .reset_rate = 27000000 },
	{ .name = "iface_clk" },
	{ 0 }
};


static struct clk_data ijpeg_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ .name = "bus_clk" },
	{ 0 }
};

static struct clk_data mdp_8960_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ .name = "bus_clk" },
	{ .name = "vsync_clk" },
	{ .name = "lut_clk" },
	{ .name = "tv_src_clk" },
	{ .name = "tv_clk" },
	{ 0 }
};

static struct clk_data mdp_8660_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ .name = "bus_clk" },
	{ .name = "vsync_clk" },
	{ .name = "tv_src_clk" },
	{ .name = "tv_clk" },
	{ .name = "pixel_mdp_clk" },
	{ .name = "pixel_lcdc_clk" },
	{ 0 }
};

static struct clk_data rot_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ .name = "bus_clk" },
	{ 0 }
};

static struct clk_data ved_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ .name = "bus_clk" },
	{ 0 }
};

static struct clk_data vfe_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ .name = "bus_clk" },
	{ 0 }
};

static struct clk_data vpe_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ .name = "bus_clk" },
	{ 0 }
};

static struct clk_data vcap_clks[] = {
	{ .name = "core_clk" },
	{ .name = "iface_clk" },
	{ .name = "bus_clk" },
	{ 0 }
};

#define FOOTSWITCH(_id, _name, _ops, _gfs_ctl_reg, _dc, _clk_data, \
		   _bp1, _bp2) \
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
		.clk_data = (_clk_data), \
		.bus_port0 = (_bp1), \
		.bus_port1 = (_bp2), \
	}
static struct footswitch footswitches[] = {
	FOOTSWITCH(FS_GFX2D0, "fs_gfx2d0", &gfx2d_fs_ops,
		GFX2D0_GFS_CTL_REG, 31, gfx2d0_clks,
		MSM_BUS_MASTER_GRAPHICS_2D_CORE0, 0),
	FOOTSWITCH(FS_GFX2D1, "fs_gfx2d1", &gfx2d_fs_ops,
		GFX2D1_GFS_CTL_REG, 31, gfx2d1_clks,
		MSM_BUS_MASTER_GRAPHICS_2D_CORE1, 0),
	FOOTSWITCH(FS_GFX3D, "fs_gfx3d", &standard_fs_ops,
		GFX3D_GFS_CTL_REG, 31, gfx3d_clks,
		MSM_BUS_MASTER_GRAPHICS_3D, 0),
	FOOTSWITCH(FS_IJPEG, "fs_ijpeg", &standard_fs_ops,
		GEMINI_GFS_CTL_REG, 31, ijpeg_clks,
		MSM_BUS_MASTER_JPEG_ENC, 0),
	FOOTSWITCH(FS_MDP, "fs_mdp", &standard_fs_ops,
		MDP_GFS_CTL_REG, 31, NULL,
		MSM_BUS_MASTER_MDP_PORT0,
		MSM_BUS_MASTER_MDP_PORT1),
	FOOTSWITCH(FS_ROT, "fs_rot", &standard_fs_ops,
		ROT_GFS_CTL_REG, 31, rot_clks,
		MSM_BUS_MASTER_ROTATOR, 0),
	FOOTSWITCH(FS_VED, "fs_ved", &standard_fs_ops,
		VED_GFS_CTL_REG, 31, ved_clks,
		MSM_BUS_MASTER_HD_CODEC_PORT0,
		MSM_BUS_MASTER_HD_CODEC_PORT1),
	FOOTSWITCH(FS_VFE, "fs_vfe", &standard_fs_ops,
		VFE_GFS_CTL_REG, 31, vfe_clks,
		MSM_BUS_MASTER_VFE, 0),
	FOOTSWITCH(FS_VPE, "fs_vpe", &standard_fs_ops,
		VPE_GFS_CTL_REG, 31, vpe_clks,
		MSM_BUS_MASTER_VPE, 0),
	FOOTSWITCH(FS_VCAP, "fs_vcap", &standard_fs_ops,
		VCAP_GFS_CTL_REG, 31, vcap_clks,
		MSM_BUS_MASTER_VIDEO_CAP, 0),
};

static int footswitch_probe(struct platform_device *pdev)
{
	struct footswitch *fs;
	struct regulator_init_data *init_data;
	struct clk_data *clock;
	uint32_t regval, rc = 0;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id >= MAX_FS)
		return -ENODEV;

	fs = &footswitches[pdev->id];
	init_data = pdev->dev.platform_data;

	if (pdev->id == FS_MDP) {
		if (cpu_is_msm8960() || cpu_is_msm8930())
			fs->clk_data = mdp_8960_clks;
		else if (cpu_is_msm8x60())
			fs->clk_data = mdp_8660_clks;
		else
			BUG();
	}

	for (clock = fs->clk_data; clock->name; clock++) {
		clock->clk = clk_get(&pdev->dev, clock->name);
		if (IS_ERR(clock->clk)) {
			rc = PTR_ERR(clock->clk);
			pr_err("clk_get(%s) failed\n", clock->name);
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
	regval |= fs->gfs_delay_cnt;
	regval &= ~RETENTION_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	fs->rdev = regulator_register(&fs->desc, &pdev->dev, init_data, fs);
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
	struct clk_data *clock;

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
