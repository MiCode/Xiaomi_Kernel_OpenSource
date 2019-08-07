// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/msm_pcie.h>
#include <asm/dma-iommu.h>
#include <linux/msm-bus.h>
#include <linux/iommu.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/ramdump.h>
#include <soc/qcom/memory_dump.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/sched/core_ctl.h>
#include "wil_platform.h"
#include "msm_11ad.h"

#define WIGIG_ENABLE_DELAY	50

#define WIGIG_SUBSYS_NAME	"WIGIG"
#define WIGIG_RAMDUMP_SIZE_SPARROW	0x200000 /* maximum ramdump size */
#define WIGIG_RAMDUMP_SIZE_TALYN	0x400000 /* maximum ramdump size */
#define WIGIG_DUMP_FORMAT_VER   0x1
#define WIGIG_DUMP_MAGIC_VER_V1 0x57474947
#define VDD_MIN_UV	1028000
#define VDD_MAX_UV	1028000
#define VDD_MAX_UA	575000
#define VDDIO_MIN_UV	1824000
#define VDDIO_MAX_UV	2040000
#define VDDIO_MAX_UA	70300
#define VDD_LDO_MIN_UV	1800000
#define VDD_LDO_MAX_UV	1800000
#define VDD_LDO_MAX_UA	100000

#define WIGIG_MIN_CPU_BOOST_KBPS	150000

struct device;

static const char * const gpio_en_name = "qcom,wigig-en";
static const char * const gpio_dc_name = "qcom,wigig-dc";
static const char * const sleep_clk_en_name = "qcom,sleep-clk-en";

struct wigig_pci {
	struct pci_device_id pci_dev;
	u32 ramdump_sz;
};

static const struct wigig_pci wigig_pci_tbl[] = {
	{ .pci_dev = { PCI_DEVICE(0x1ae9, 0x0310) },
	  .ramdump_sz = WIGIG_RAMDUMP_SIZE_SPARROW},
	{ .pci_dev = { PCI_DEVICE(0x17cb, 0x1201) },
	  .ramdump_sz = WIGIG_RAMDUMP_SIZE_TALYN},
};

struct msm11ad_vreg {
	const char *name;
	struct regulator *reg;
	int max_uA;
	int min_uV;
	int max_uV;
	bool enabled;
};

struct msm11ad_clk {
	const char *name;
	struct clk *clk;
	bool enabled;
};

struct msm11ad_ctx {
	struct list_head list;
	struct device *dev; /* for platform device */
	int gpio_en; /* card enable */
	int gpio_dc;
	int sleep_clk_en; /* sleep clock enable for low PM management */

	/* pci device */
	u32 rc_index; /* PCIE root complex index */
	struct pci_dev *pcidev;
	struct pci_saved_state *pristine_state;
	struct pci_saved_state *golden_state;
	struct msm_pcie_register_event pci_event;

	int smmu_s1_bypass;

	/* bus frequency scaling */
	struct msm_bus_scale_pdata *bus_scale;
	u32 msm_bus_handle;

	/* subsystem restart */
	struct wil_platform_rops rops;
	void *wil_handle;
	struct subsys_desc subsysdesc;
	struct subsys_device *subsys;
	void *subsys_handle;
	bool recovery_in_progress;

	/* ramdump */
	void *ramdump_addr;
	struct msm_dump_data dump_data;
	struct ramdump_device *ramdump_dev;
	u32 ramdump_size;

	/* external vregs and clocks */
	struct msm11ad_vreg vdd;
	struct msm11ad_vreg vddio;
	struct msm11ad_vreg vdd_ldo;
	struct msm11ad_clk rf_clk;
	struct msm11ad_clk rf_clk_pin;

	/* cpu boost support */
	bool use_cpu_boost;
	bool is_cpu_boosted;
	struct cpumask boost_cpu_0;
	struct cpumask boost_cpu_1;

	bool keep_radio_on_during_sleep;
	bool use_ap_ps;
	int features;
};

static LIST_HEAD(dev_list);

static struct msm11ad_ctx *pcidev2ctx(struct pci_dev *pcidev)
{
	struct msm11ad_ctx *ctx;

	list_for_each_entry(ctx, &dev_list, list) {
		if (ctx->pcidev == pcidev)
			return ctx;
	}
	return NULL;
}

static int msm_11ad_init_vreg(struct device *dev,
			      struct msm11ad_vreg *vreg, const char *name)
{
	int rc = 0;

	if (!vreg)
		return 0;

	vreg->name = kstrdup(name, GFP_KERNEL);
	if (!vreg->name)
		return -ENOMEM;

	vreg->reg = devm_regulator_get(dev, name);
	if (IS_ERR_OR_NULL(vreg->reg)) {
		rc = PTR_ERR(vreg->reg);
		dev_err(dev, "%s: failed to get %s, rc=%d\n",
			__func__, name, rc);
		kfree(vreg->name);
		vreg->reg = NULL;
		goto out;
	}

	dev_dbg(dev, "%s: %s initialized successfully\n", __func__, name);

out:
	return rc;
}

static int msm_11ad_release_vreg(struct device *dev, struct msm11ad_vreg *vreg)
{
	if (!vreg || !vreg->reg)
		return 0;

	dev_dbg(dev, "%s: %s released\n", __func__, vreg->name);

	devm_regulator_put(vreg->reg);
	vreg->reg = NULL;
	kfree(vreg->name);

	return 0;
}

static int msm_11ad_init_clk(struct device *dev, struct msm11ad_clk *clk,
			     const char *name)
{
	int rc = 0;

	clk->name = kstrdup(name, GFP_KERNEL);
	if (!clk->name)
		return -ENOMEM;

	clk->clk = devm_clk_get(dev, name);
	if (IS_ERR(clk->clk)) {
		rc = PTR_ERR(clk->clk);
		if (rc == -ENOENT)
			rc = -EPROBE_DEFER;
		dev_err(dev, "%s: failed to get %s rc %d\n",
				__func__, name, rc);
		kfree(clk->name);
		clk->clk = NULL;
		goto out;
	}

	dev_dbg(dev, "%s: %s initialized successfully\n", __func__, name);

out:
	return rc;
}

static int msm_11ad_release_clk(struct device *dev, struct msm11ad_clk *clk)
{
	if (!clk || !clk->clk)
		return 0;

	dev_dbg(dev, "%s: %s released\n", __func__, clk->name);

	devm_clk_put(dev, clk->clk);
	clk->clk = NULL;

	kfree(clk->name);

	return 0;
}

static int msm_11ad_init_vregs(struct msm11ad_ctx *ctx)
{
	int rc;
	struct device *dev = ctx->dev;

	if (!of_property_read_bool(dev->of_node, "qcom,use-ext-supply"))
		return 0;

	rc = msm_11ad_init_vreg(dev, &ctx->vdd, "vdd");
	if (rc)
		goto out;

	ctx->vdd.max_uV = VDD_MAX_UV;
	ctx->vdd.min_uV = VDD_MIN_UV;
	ctx->vdd.max_uA = VDD_MAX_UA;

	rc = msm_11ad_init_vreg(dev, &ctx->vddio, "vddio");
	if (rc)
		goto vddio_fail;

	ctx->vddio.max_uV = VDDIO_MAX_UV;
	ctx->vddio.min_uV = VDDIO_MIN_UV;
	ctx->vddio.max_uA = VDDIO_MAX_UA;

	rc = msm_11ad_init_vreg(dev, &ctx->vdd_ldo, "vdd-ldo");
	if (rc)
		goto vdd_ldo_fail;

	ctx->vdd_ldo.max_uV = VDD_LDO_MAX_UV;
	ctx->vdd_ldo.min_uV = VDD_LDO_MIN_UV;
	ctx->vdd_ldo.max_uA = VDD_LDO_MAX_UA;

	return rc;

vdd_ldo_fail:
	msm_11ad_release_vreg(dev, &ctx->vddio);
vddio_fail:
	msm_11ad_release_vreg(dev, &ctx->vdd);
out:
	return rc;
}

static void msm_11ad_release_vregs(struct msm11ad_ctx *ctx)
{
	msm_11ad_release_vreg(ctx->dev, &ctx->vdd_ldo);
	msm_11ad_release_vreg(ctx->dev, &ctx->vdd);
	msm_11ad_release_vreg(ctx->dev, &ctx->vddio);
}

static int msm_11ad_cfg_vreg(struct device *dev,
			     struct msm11ad_vreg *vreg, bool on)
{
	int rc = 0;
	int min_uV;
	int uA_load;

	if (!vreg || !vreg->reg)
		goto out;

	if (regulator_count_voltages(vreg->reg) > 0) {
		min_uV = on ? vreg->min_uV : 0;
		rc = regulator_set_voltage(vreg->reg, min_uV, vreg->max_uV);
		if (rc) {
			dev_err(dev, "%s: %s set voltage failed, err=%d\n",
					__func__, vreg->name, rc);
			goto out;
		}
		uA_load = on ? vreg->max_uA : 0;
		rc = regulator_set_load(vreg->reg, uA_load);
		if (rc >= 0) {
			/*
			 * regulator_set_load() returns new regulator
			 * mode upon success.
			 */
			dev_dbg(dev,
				  "%s: %s regulator_set_load rc(%d)\n",
				  __func__, vreg->name, rc);
			rc = 0;
		} else {
			dev_err(dev,
				"%s: %s set load(uA_load=%d) failed, rc=%d\n",
				__func__, vreg->name, uA_load, rc);
			goto out;
		}
	}

out:
	return rc;
}

static int msm_11ad_enable_vreg(struct msm11ad_ctx *ctx,
				struct msm11ad_vreg *vreg)
{
	struct device *dev = ctx->dev;
	int rc = 0;

	if (!vreg || !vreg->reg || vreg->enabled)
		goto out;

	rc = msm_11ad_cfg_vreg(dev, vreg, true);
	if (rc)
		goto out;

	rc = regulator_enable(vreg->reg);
	if (rc) {
		dev_err(dev, "%s: %s enable failed, rc=%d\n",
				__func__, vreg->name, rc);
		goto enable_fail;
	}

	vreg->enabled = true;

	dev_dbg(dev, "%s: %s enabled\n", __func__, vreg->name);

	return rc;

enable_fail:
	msm_11ad_cfg_vreg(dev, vreg, false);
out:
	return rc;
}

static int msm_11ad_disable_vreg(struct msm11ad_ctx *ctx,
				 struct msm11ad_vreg *vreg)
{
	struct device *dev = ctx->dev;
	int rc = 0;

	if (!vreg || !vreg->reg || !vreg->enabled)
		goto out;

	rc = regulator_disable(vreg->reg);
	if (rc) {
		dev_err(dev, "%s: %s disable failed, rc=%d\n",
				__func__, vreg->name, rc);
		goto out;
	}

	/* ignore errors on applying disable config */
	msm_11ad_cfg_vreg(dev, vreg, false);
	vreg->enabled = false;

	dev_dbg(dev, "%s: %s disabled\n", __func__, vreg->name);

out:
	return rc;
}

static int msm_11ad_enable_vregs(struct msm11ad_ctx *ctx)
{
	int rc = 0;

	rc = msm_11ad_enable_vreg(ctx, &ctx->vdd);
	if (rc)
		goto out;

	rc = msm_11ad_enable_vreg(ctx, &ctx->vddio);
	if (rc)
		goto vddio_fail;

	rc = msm_11ad_enable_vreg(ctx, &ctx->vdd_ldo);
	if (rc)
		goto vdd_ldo_fail;

	return rc;

vdd_ldo_fail:
	msm_11ad_disable_vreg(ctx, &ctx->vddio);
vddio_fail:
	msm_11ad_disable_vreg(ctx, &ctx->vdd);
out:
	return rc;
}

static int msm_11ad_disable_vregs(struct msm11ad_ctx *ctx)
{
	if (!ctx->vdd.reg && !ctx->vddio.reg && !ctx->vdd_ldo.reg)
		goto out;

	/* ignore errors on disable vreg */
	msm_11ad_disable_vreg(ctx, &ctx->vdd_ldo);
	msm_11ad_disable_vreg(ctx, &ctx->vdd);
	msm_11ad_disable_vreg(ctx, &ctx->vddio);

out:
	return 0;
}

static int msm_11ad_enable_clk(struct msm11ad_ctx *ctx,
				struct msm11ad_clk *clk)
{
	struct device *dev = ctx->dev;
	int rc = 0;

	if (!clk || !clk->clk || clk->enabled)
		goto out;

	rc = clk_prepare_enable(clk->clk);
	if (rc) {
		dev_err(dev, "%s: failed to enable %s, rc(%d)\n",
			__func__, clk->name, rc);
		goto out;
	}
	clk->enabled = true;

	dev_dbg(dev, "%s: %s enabled\n", __func__, clk->name);

out:
	return rc;
}

static void msm_11ad_disable_clk(struct msm11ad_ctx *ctx,
				struct msm11ad_clk *clk)
{
	struct device *dev = ctx->dev;

	if (!clk || !clk->clk || !clk->enabled)
		goto out;

	clk_disable_unprepare(clk->clk);
	clk->enabled = false;

	dev_dbg(dev, "%s: %s disabled\n", __func__, clk->name);

out:
	return;
}

static int msm_11ad_enable_clocks(struct msm11ad_ctx *ctx)
{
	int rc;

	rc = msm_11ad_enable_clk(ctx, &ctx->rf_clk);
	if (rc)
		return rc;

	rc = msm_11ad_enable_clk(ctx, &ctx->rf_clk_pin);
	if (rc)
		msm_11ad_disable_clk(ctx, &ctx->rf_clk);

	return rc;
}

static int msm_11ad_init_clocks(struct msm11ad_ctx *ctx)
{
	int rc;
	struct device *dev = ctx->dev;
	int rf_clk_pin_idx;

	if (!of_property_read_bool(dev->of_node, "qcom,use-ext-clocks"))
		return 0;

	rc = msm_11ad_init_clk(dev, &ctx->rf_clk, "rf_clk");
	if (rc)
		return rc;

	rf_clk_pin_idx = of_property_match_string(dev->of_node, "clock-names",
						   "rf_clk_pin_clk");
	if (rf_clk_pin_idx >= 0) {
		rc = msm_11ad_init_clk(dev, &ctx->rf_clk_pin,
				       "rf_clk_pin_clk");
		if (rc)
			msm_11ad_release_clk(ctx->dev, &ctx->rf_clk);
	}

	return rc;
}

static void msm_11ad_release_clocks(struct msm11ad_ctx *ctx)
{
	msm_11ad_release_clk(ctx->dev, &ctx->rf_clk_pin);
	msm_11ad_release_clk(ctx->dev, &ctx->rf_clk);
}

static void msm_11ad_disable_clocks(struct msm11ad_ctx *ctx)
{
	msm_11ad_disable_clk(ctx, &ctx->rf_clk_pin);
	msm_11ad_disable_clk(ctx, &ctx->rf_clk);
}

static int msm_11ad_turn_device_power_off(struct msm11ad_ctx *ctx)
{
	if (ctx->gpio_en >= 0)
		gpio_direction_output(ctx->gpio_en, 0);

	if (ctx->gpio_dc >= 0)
		gpio_direction_output(ctx->gpio_dc, 0);

	if (ctx->sleep_clk_en >= 0)
		gpio_direction_output(ctx->sleep_clk_en, 0);

	msm_11ad_disable_clocks(ctx);

	msm_11ad_disable_vregs(ctx);

	return 0;
}

static int msm_11ad_turn_device_power_on(struct msm11ad_ctx *ctx)
{
	int rc;

	rc = msm_11ad_enable_vregs(ctx);
	if (rc) {
		dev_err(ctx->dev, "msm_11ad_enable_vregs failed :%d\n",
			rc);
		return rc;
	}

	rc = msm_11ad_enable_clocks(ctx);
	if (rc) {
		dev_err(ctx->dev, "msm_11ad_enable_clocks failed :%d\n", rc);
		goto err_disable_vregs;
	}

	if (ctx->sleep_clk_en >= 0)
		gpio_direction_output(ctx->sleep_clk_en, 1);

	if (ctx->gpio_dc >= 0) {
		gpio_direction_output(ctx->gpio_dc, 1);
		msleep(WIGIG_ENABLE_DELAY);
	}

	if (ctx->gpio_en >= 0) {
		gpio_direction_output(ctx->gpio_en, 1);
		msleep(WIGIG_ENABLE_DELAY);
	}

	return 0;

err_disable_vregs:
	msm_11ad_disable_vregs(ctx);
	return rc;
}

static int msm_11ad_suspend_power_off(void *handle)
{
	int rc;
	struct msm11ad_ctx *ctx = handle;
	struct pci_dev *pcidev;

	pr_debug("%s\n", __func__);

	if (!ctx) {
		pr_err("%s: No context\n", __func__);
		return -ENODEV;
	}

	pcidev = ctx->pcidev;

	/* free the old saved state and save the latest state */
	rc = pci_save_state(pcidev);
	if (rc) {
		dev_err(ctx->dev, "pci_save_state failed :%d\n", rc);
		goto out;
	}
	kfree(ctx->pristine_state);
	ctx->pristine_state = pci_store_saved_state(pcidev);

	rc = msm_pcie_pm_control(MSM_PCIE_SUSPEND, pcidev->bus->number,
				 pcidev, NULL, 0);
	if (rc) {
		dev_err(ctx->dev, "msm_pcie_pm_control(SUSPEND) failed :%d\n",
			rc);
		goto out;
	}

	rc = msm_11ad_turn_device_power_off(ctx);

out:
	return rc;
}

static int ops_pci_linkdown_recovery(void *handle)
{
	struct msm11ad_ctx *ctx = handle;
	struct pci_dev *pcidev;
	int rc;

	if (!ctx) {
		pr_err("11ad pci_linkdown_recovery: No context\n");
		return -ENODEV;
	}

	pcidev = ctx->pcidev;

	/* suspend */
	dev_dbg(ctx->dev, "11ad pci_linkdown_recovery, suspend the device\n");
	pci_disable_device(pcidev);
	rc = msm_pcie_pm_control(MSM_PCIE_SUSPEND, pcidev->bus->number,
				 pcidev, NULL, 0);
	if (rc) {
		dev_err(ctx->dev, "msm_pcie_pm_control(SUSPEND) failed: %d\n",
			rc);
		goto out;
	}

	rc = msm_11ad_turn_device_power_off(ctx);
	if (rc) {
		dev_err(ctx->dev, "failed to turn off device: %d\n",
			rc);
		goto out;
	}

	/* resume */
	rc = msm_11ad_turn_device_power_on(ctx);
	if (rc)
		goto out;

	rc = msm_pcie_pm_control(MSM_PCIE_RESUME, pcidev->bus->number,
				 pcidev, NULL, 0);
	if (rc) {
		dev_err(ctx->dev, "msm_pcie_pm_control(RESUME) failed: %d\n",
			rc);
		goto err_disable_power;
	}

	pci_set_power_state(pcidev, PCI_D0);

	if (ctx->golden_state)
		pci_load_saved_state(pcidev, ctx->golden_state);
	pci_restore_state(pcidev);

	rc = pci_enable_device(pcidev);
	if (rc) {
		dev_err(ctx->dev, "pci_enable_device failed (%d)\n", rc);
		goto err_disable_power;
	}

	pci_set_master(pcidev);

out:
	return rc;

err_disable_power:
	msm_11ad_turn_device_power_off(ctx);
	return rc;
}

static int ops_suspend(void *handle, bool keep_device_power)
{
	struct msm11ad_ctx *ctx = handle;
	struct pci_dev *pcidev;
	int rc;

	pr_debug("11ad suspend: %s\n", __func__);
	if (!ctx) {
		pr_err("11ad suspend: No context\n");
		return -ENODEV;
	}

	if (!keep_device_power)
		return msm_11ad_suspend_power_off(handle);

	pcidev = ctx->pcidev;

	dev_dbg(ctx->dev, "disable device and save config\n");
	pci_disable_device(pcidev);
	pci_save_state(pcidev);
	kfree(ctx->pristine_state);
	ctx->pristine_state = pci_store_saved_state(pcidev);
	dev_dbg(ctx->dev, "moving to D3\n");
	pci_set_power_state(pcidev, PCI_D3hot);

	rc = msm_pcie_pm_control(MSM_PCIE_SUSPEND, pcidev->bus->number,
				 pcidev, NULL, 0);
	if (rc)
		dev_err(ctx->dev, "msm_pcie_pm_control(SUSPEND) failed :%d\n",
			rc);

	return rc;
}

static int msm_11ad_resume_power_on(void *handle)
{
	int rc;
	struct msm11ad_ctx *ctx = handle;
	struct pci_dev *pcidev;

	pr_debug("%s\n", __func__);

	if (!ctx) {
		pr_err("%s: No context\n", __func__);
		return -ENODEV;
	}
	pcidev = ctx->pcidev;

	rc = msm_11ad_turn_device_power_on(ctx);
	if (rc)
		return rc;

	rc = msm_pcie_pm_control(MSM_PCIE_RESUME, pcidev->bus->number,
				 pcidev, NULL, 0);
	if (rc) {
		dev_err(ctx->dev, "msm_pcie_pm_control(RESUME) failed :%d\n",
			rc);
		goto err_disable_power;
	}

	pci_set_power_state(pcidev, PCI_D0);

	if (ctx->pristine_state)
		pci_load_saved_state(ctx->pcidev, ctx->pristine_state);
	pci_restore_state(ctx->pcidev);

	return 0;

err_disable_power:
	msm_11ad_turn_device_power_off(ctx);
	return rc;
}

static int ops_resume(void *handle, bool device_powered_on)
{
	struct msm11ad_ctx *ctx = handle;
	struct pci_dev *pcidev;
	int rc;

	pr_debug("11ad resume: %s\n", __func__);
	if (!ctx) {
		pr_err("11ad resume: No context\n");
		return -ENODEV;
	}

	pcidev = ctx->pcidev;

	if (!device_powered_on)
		return msm_11ad_resume_power_on(handle);

	rc = msm_pcie_pm_control(MSM_PCIE_RESUME, pcidev->bus->number,
				 pcidev, NULL, 0);
	if (rc) {
		dev_err(ctx->dev, "msm_pcie_pm_control(RESUME) failed :%d\n",
			rc);
		return rc;
	}
	pci_set_power_state(pcidev, PCI_D0);

	dev_dbg(ctx->dev, "restore state and enable device\n");
	pci_load_saved_state(pcidev, ctx->pristine_state);
	pci_restore_state(pcidev);

	rc = pci_enable_device(pcidev);
	if (rc) {
		dev_err(ctx->dev, "pci_enable_device failed (%d)\n", rc);
		goto out;
	}

	dev_dbg(ctx->dev, "pci set master\n");
	pci_set_master(pcidev);

out:
	return rc;
}

static int msm_11ad_ssr_shutdown(const struct subsys_desc *subsys,
				 bool force_stop)
{
	pr_info("%s(%pK,%d)\n", __func__, subsys, force_stop);
	/* nothing is done in shutdown. We do full recovery in powerup */
	return 0;
}

static int msm_11ad_ssr_powerup(const struct subsys_desc *subsys)
{
	int rc = 0;
	struct platform_device *pdev;
	struct msm11ad_ctx *ctx;

	pr_info("%s(%pK)\n", __func__, subsys);

	pdev = to_platform_device(subsys->dev);
	ctx = platform_get_drvdata(pdev);

	if (!ctx)
		return -ENODEV;

	if (ctx->recovery_in_progress) {
		if (ctx->rops.fw_recovery && ctx->wil_handle) {
			dev_dbg(ctx->dev, "requesting FW recovery\n");
			rc = ctx->rops.fw_recovery(ctx->wil_handle);
		}
		ctx->recovery_in_progress = false;
	}

	return rc;
}

static int msm_11ad_ssr_copy_ramdump(struct msm11ad_ctx *ctx)
{
	if (ctx->rops.ramdump && ctx->wil_handle) {
		int rc = ctx->rops.ramdump(ctx->wil_handle, ctx->ramdump_addr,
					   ctx->ramdump_size);
		if (rc) {
			dev_err(ctx->dev, "ramdump failed : %d\n", rc);
			return -EINVAL;
		}
	}

	ctx->dump_data.version = WIGIG_DUMP_FORMAT_VER;
	strlcpy(ctx->dump_data.name, WIGIG_SUBSYS_NAME,
		sizeof(ctx->dump_data.name));

	ctx->dump_data.magic = WIGIG_DUMP_MAGIC_VER_V1;
	return 0;
}

static int msm_11ad_ssr_ramdump(int enable, const struct subsys_desc *subsys)
{
	int rc;
	struct ramdump_segment segment;
	struct platform_device *pdev;
	struct msm11ad_ctx *ctx;

	pdev = to_platform_device(subsys->dev);
	ctx = platform_get_drvdata(pdev);

	if (!ctx)
		return -ENODEV;

	if (!enable)
		return 0;

	if (!ctx->recovery_in_progress) {
		rc = msm_11ad_ssr_copy_ramdump(ctx);
		if (rc)
			return rc;
	}

	memset(&segment, 0, sizeof(segment));
	segment.v_address = ctx->ramdump_addr;
	segment.size = ctx->ramdump_size;

	return do_ramdump(ctx->ramdump_dev, &segment, 1);
}

static void msm_11ad_ssr_crash_shutdown(const struct subsys_desc *subsys)
{
	struct platform_device *pdev;
	struct msm11ad_ctx *ctx;

	pdev = to_platform_device(subsys->dev);
	ctx = platform_get_drvdata(pdev);

	if (!ctx) {
		pr_err("%s: no context\n", __func__);
		return;
	}

	if (!ctx->recovery_in_progress)
		(void)msm_11ad_ssr_copy_ramdump(ctx);
}

static void msm_11ad_ssr_deinit(struct msm11ad_ctx *ctx)
{
	if (ctx->ramdump_dev) {
		destroy_ramdump_device(ctx->ramdump_dev);
		ctx->ramdump_dev = NULL;
	}

	kfree(ctx->ramdump_addr);
	ctx->ramdump_addr = NULL;

	if (ctx->subsys_handle) {
		subsystem_put(ctx->subsys_handle);
		ctx->subsys_handle = NULL;
	}

	if (ctx->subsys) {
		subsys_unregister(ctx->subsys);
		ctx->subsys = NULL;
	}
}

static int msm_11ad_ssr_init(struct msm11ad_ctx *ctx)
{
	int rc;
	struct msm_dump_entry dump_entry;

	ctx->subsysdesc.name = "WIGIG";
	ctx->subsysdesc.owner = THIS_MODULE;
	ctx->subsysdesc.shutdown = msm_11ad_ssr_shutdown;
	ctx->subsysdesc.powerup = msm_11ad_ssr_powerup;
	ctx->subsysdesc.ramdump = msm_11ad_ssr_ramdump;
	ctx->subsysdesc.crash_shutdown = msm_11ad_ssr_crash_shutdown;
	ctx->subsysdesc.dev = ctx->dev;
	ctx->subsys = subsys_register(&ctx->subsysdesc);
	if (IS_ERR(ctx->subsys)) {
		rc = PTR_ERR(ctx->subsys);
		dev_err(ctx->dev, "subsys_register failed :%d\n", rc);
		goto out_rc;
	}

	/* register ramdump area */
	ctx->ramdump_addr = kmalloc(ctx->ramdump_size, GFP_KERNEL);
	if (!ctx->ramdump_addr) {
		rc = -ENOMEM;
		goto out_rc;
	}

	ctx->dump_data.addr = virt_to_phys(ctx->ramdump_addr);
	ctx->dump_data.len = ctx->ramdump_size;
	dump_entry.id = MSM_DUMP_DATA_WIGIG;
	dump_entry.addr = virt_to_phys(&ctx->dump_data);

	rc = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
	if (rc) {
		dev_err(ctx->dev, "Dump table setup failed: %d\n", rc);
		goto out_rc;
	}

	ctx->ramdump_dev = create_ramdump_device(ctx->subsysdesc.name,
						 ctx->subsysdesc.dev);
	if (!ctx->ramdump_dev) {
		dev_err(ctx->dev, "Create ramdump device failed: %d\n", rc);
		rc = -ENOMEM;
		goto out_rc;
	}

	return 0;

out_rc:
	msm_11ad_ssr_deinit(ctx);
	return rc;
}

static void msm_11ad_init_cpu_boost(struct msm11ad_ctx *ctx)
{
	unsigned int cpu0freq, freq;
	int i, boost_cpu = 0;

	cpu0freq = cpufreq_quick_get_max(0);
	for_each_possible_cpu(i) {
		freq = cpufreq_quick_get_max(i);
		if (freq > cpu0freq) {
			boost_cpu = i;
			break;
		}
	}

	if (boost_cpu) {
		/*
		 * use first 2 big cores for boost, to be compatible with WLAN
		 * which assigns big cores from the last index
		 */
		ctx->use_cpu_boost = true;
		cpumask_clear(&ctx->boost_cpu_0);
		cpumask_clear(&ctx->boost_cpu_1);
		cpumask_set_cpu(boost_cpu, &ctx->boost_cpu_0);
		if (boost_cpu < (nr_cpu_ids - 1)) {
			cpumask_set_cpu(boost_cpu + 1, &ctx->boost_cpu_1);
			dev_info(ctx->dev, "CPU boost: will use cores %d - %d\n",
				 boost_cpu, boost_cpu + 1);
		} else {
			cpumask_set_cpu(boost_cpu, &ctx->boost_cpu_1);
			dev_info(ctx->dev, "CPU boost: will use core %d\n",
				 boost_cpu);
		}
	} else {
		ctx->use_cpu_boost = false;
		dev_info(ctx->dev, "CPU boost disabled, uniform topology\n");
	}
}

static void msm_11ad_pci_event_cb(struct msm_pcie_notify *notify)
{
	struct pci_dev *pcidev = notify->user;
	struct msm11ad_ctx *ctx = pcidev2ctx(pcidev);

	if (!ctx)
		return;

	if (!ctx->rops.notify || !ctx->wil_handle) {
		dev_info(ctx->dev,
			 "no registered notif CB, cannot hadle pci notifications\n");
		return;
	}

	switch (notify->event) {
	case MSM_PCIE_EVENT_LINKDOWN:
		dev_err(ctx->dev, "PCIe linkdown\n");
		ctx->rops.notify(ctx->wil_handle,
				 WIL_PLATFORM_NOTIF_PCI_LINKDOWN);
		break;
	default:
		break;
	}

}

static int msm_11ad_probe(struct platform_device *pdev)
{
	struct msm11ad_ctx *ctx;
	struct device *dev = &pdev->dev;
	struct device_node *of_node = dev->of_node;
	struct device_node *rc_node;
	struct pci_dev *pcidev = NULL;
	int rc, i;
	bool pcidev_found = false;
	struct msm_pcie_register_event *pci_event;

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		rc = -ENOMEM;
		goto out_module;
	}

	ctx->dev = dev;

	/*== parse ==*/

	/* Information pieces:
	 * - of_node stands for "wil6210":
	 *	wil6210: qcom,wil6210 {
	 *	compatible = "qcom,wil6210";
	 *	qcom,pcie-parent = <&pcie1>;
	 *	qcom,wigig-en = <&tlmm 94 0>; (ctx->gpio_en)
	 *	qcom,wigig-dc = <&tlmm 81 0>; (ctx->gpio_dc)
	 *	qcom,sleep-clk-en = <&pm8994_gpios 18 0>; (ctx->sleep_clk_en)
	 *	qcom,msm-bus,name = "wil6210";
	 *	qcom,msm-bus,num-cases = <2>;
	 *	qcom,msm-bus,num-paths = <1>;
	 *	qcom,msm-bus,vectors-KBps =
	 *		<100 512 0 0>,
	 *		<100 512 600000 800000>;
	 *	qcom,use-ap-power-save; (ctx->use_ap_ps)
	 *};
	 * rc_node stands for "qcom,pcie", selected entries:
	 * cell-index = <1>; (ctx->rc_index)
	 * iommus = <&anoc0_smmu>;
	 * qcom,smmu-exist;
	 */

	/* wigig-en and wigig-dc are optional properties */
	ctx->gpio_dc = of_get_named_gpio(of_node, gpio_dc_name, 0);
	if (ctx->gpio_dc < 0)
		dev_warn(ctx->dev, "GPIO <%s> not found, dc GPIO not used\n",
			 gpio_dc_name);
	ctx->gpio_en = of_get_named_gpio(of_node, gpio_en_name, 0);
	if (ctx->gpio_en < 0)
		dev_warn(ctx->dev, "GPIO <%s> not found, enable GPIO not used\n",
			gpio_en_name);
	ctx->sleep_clk_en = of_get_named_gpio(of_node, sleep_clk_en_name, 0);
	if (ctx->sleep_clk_en < 0)
		dev_warn(ctx->dev, "GPIO <%s> not found, sleep clock not used\n",
			 sleep_clk_en_name);
	rc_node = of_parse_phandle(of_node, "qcom,pcie-parent", 0);
	if (!rc_node) {
		dev_err(ctx->dev, "Parent PCIE device not found\n");
		rc = -EINVAL;
		goto out_module;
	}
	rc = of_property_read_u32(rc_node, "cell-index", &ctx->rc_index);
	if (rc < 0) {
		dev_err(ctx->dev, "Parent PCIE device index not found\n");
		rc = -EINVAL;
		goto out_module;
	}
	ctx->keep_radio_on_during_sleep = of_property_read_bool(of_node,
		"qcom,keep-radio-on-during-sleep");
	ctx->bus_scale = msm_bus_cl_get_pdata(pdev);
	if (!ctx->bus_scale) {
		dev_err(ctx->dev, "Unable to read bus-scaling from DT\n");
		rc = -EINVAL;
		goto out_module;
	}
	ctx->use_ap_ps = of_property_read_bool(of_node,
					       "qcom,use-ap-power-save");

	/*== execute ==*/
	/* turn device on */
	rc = msm_11ad_init_vregs(ctx);
	if (rc) {
		dev_err(ctx->dev, "msm_11ad_init_vregs failed: %d\n", rc);
		goto out_module;
	}
	rc = msm_11ad_enable_vregs(ctx);
	if (rc) {
		dev_err(ctx->dev, "msm_11ad_enable_vregs failed: %d\n", rc);
		goto out_vreg_clk;
	}

	rc = msm_11ad_init_clocks(ctx);
	if (rc) {
		dev_err(ctx->dev, "msm_11ad_init_clocks failed: %d\n", rc);
		goto out_vreg_clk;
	}

	rc = msm_11ad_enable_clocks(ctx);
	if (rc) {
		dev_err(ctx->dev, "msm_11ad_enable_clocks failed: %d\n", rc);
		goto out_vreg_clk;
	}

	if (ctx->gpio_dc >= 0) {
		rc = gpio_request(ctx->gpio_dc, gpio_dc_name);
		if (rc < 0) {
			dev_err(ctx->dev, "failed to request GPIO %d <%s>\n",
				ctx->gpio_dc, gpio_dc_name);
			goto out_req_dc;
		}
		rc = gpio_direction_output(ctx->gpio_dc, 1);
		if (rc < 0) {
			dev_err(ctx->dev, "failed to set GPIO %d <%s>\n",
				ctx->gpio_dc, gpio_dc_name);
			goto out_set_dc;
		}
		msleep(WIGIG_ENABLE_DELAY);
	}

	if (ctx->gpio_en >= 0) {
		rc = gpio_request(ctx->gpio_en, gpio_en_name);
		if (rc < 0) {
			dev_err(ctx->dev, "failed to request GPIO %d <%s>\n",
				ctx->gpio_en, gpio_en_name);
			goto out_req;
		}
		rc = gpio_direction_output(ctx->gpio_en, 1);
		if (rc < 0) {
			dev_err(ctx->dev, "failed to set GPIO %d <%s>\n",
				ctx->gpio_en, gpio_en_name);
			goto out_set;
		}
		msleep(WIGIG_ENABLE_DELAY);
	}

	/* enumerate it on PCIE */
	rc = msm_pcie_enumerate(ctx->rc_index);
	if (rc < 0) {
		dev_err(ctx->dev, "Parent PCIE enumeration failed\n");
		goto out_rc;
	}
	/* search for PCIE device in our domain */
	for (i = 0; i < ARRAY_SIZE(wigig_pci_tbl); ++i) {
		do {
			pcidev = pci_get_device(wigig_pci_tbl[i].pci_dev.vendor,
						wigig_pci_tbl[i].pci_dev.device,
						pcidev);
			if (!pcidev)
				break;

			if (pci_domain_nr(pcidev->bus) == ctx->rc_index) {
				ctx->ramdump_size = wigig_pci_tbl[i].ramdump_sz;
				pcidev_found = true;
				break;
			}
		} while (true);

		if (pcidev_found)
			break;
	}
	if (!pcidev_found) {
		rc = -ENODEV;
		dev_err(ctx->dev, "Wigig device not found\n");
		goto out_rc;
	}
	ctx->pcidev = pcidev;
	dev_dbg(ctx->dev, "Wigig device %4x:%4x found\n",
		ctx->pcidev->vendor, ctx->pcidev->device);

	rc = msm_pcie_pm_control(MSM_PCIE_RESUME, pcidev->bus->number,
				 pcidev, NULL, 0);
	if (rc) {
		dev_err(ctx->dev, "msm_pcie_pm_control(RESUME) failed:%d\n",
			rc);
		goto out_rc;
	}

	pci_set_power_state(pcidev, PCI_D0);

	pci_restore_state(ctx->pcidev);

	if (ctx->sleep_clk_en >= 0) {
		rc = gpio_request(ctx->sleep_clk_en, "msm_11ad");
		if (rc < 0) {
			dev_err(ctx->dev,
				"failed to request GPIO %d <%s>, sleep clock disabled\n",
				ctx->sleep_clk_en, sleep_clk_en_name);
			ctx->sleep_clk_en = -EINVAL;
		} else {
			gpio_direction_output(ctx->sleep_clk_en, 0);
		}
	}

	/* register for subsystem restart */
	rc = msm_11ad_ssr_init(ctx);
	if (rc) {
		dev_err(ctx->dev, "msm_11ad_ssr_init failed: %d\n", rc);
		goto out_suspend;
	}

	msm_11ad_init_cpu_boost(ctx);

	/* report */
	dev_info(ctx->dev, "msm_11ad discovered. %pK {\n"
		 "  gpio_en = %d\n"
		 "  gpio_dc = %d\n"
		 "  sleep_clk_en = %d\n"
		 "  rc_index = %d\n"
		 "  pcidev = %pK\n"
		 "}\n", ctx, ctx->gpio_en, ctx->gpio_dc, ctx->sleep_clk_en,
		 ctx->rc_index, ctx->pcidev);

	platform_set_drvdata(pdev, ctx);
	device_disable_async_suspend(&pcidev->dev);

	/* Save golden config space for pci linkdown recovery */
	rc = pci_save_state(pcidev);
	if (rc) {
		dev_err(ctx->dev, "pci_save_state failed :%d\n", rc);
		goto out_suspend;
	}
	ctx->golden_state = pci_store_saved_state(pcidev);

	pci_event = &ctx->pci_event;
	pci_event->events = MSM_PCIE_EVENT_LINKDOWN;
	pci_event->user = ctx->pcidev;
	pci_event->mode = MSM_PCIE_TRIGGER_CALLBACK;
	pci_event->options = MSM_PCIE_CONFIG_NO_RECOVERY;
	pci_event->callback = msm_11ad_pci_event_cb;

	rc = msm_pcie_register_event(pci_event);
	if (rc) {
		dev_err(ctx->dev, "failed to register msm pcie event: %d\n",
			rc);
		kfree(ctx->golden_state);
		goto out_suspend;
	}

	list_add_tail(&ctx->list, &dev_list);
	msm_11ad_suspend_power_off(ctx);

	return 0;
out_suspend:
	msm_pcie_pm_control(MSM_PCIE_SUSPEND, pcidev->bus->number,
			    pcidev, NULL, 0);
out_rc:
	if (ctx->gpio_en >= 0)
		gpio_direction_output(ctx->gpio_en, 0);
out_set:
	if (ctx->gpio_en >= 0)
		gpio_free(ctx->gpio_en);
out_req:
	ctx->gpio_en = -EINVAL;
	if (ctx->gpio_dc >= 0)
		gpio_direction_output(ctx->gpio_dc, 0);
out_set_dc:
	if (ctx->gpio_dc >= 0)
		gpio_free(ctx->gpio_dc);
out_req_dc:
	ctx->gpio_dc = -EINVAL;
out_vreg_clk:
	msm_11ad_disable_clocks(ctx);
	msm_11ad_release_clocks(ctx);
	msm_11ad_disable_vregs(ctx);
	msm_11ad_release_vregs(ctx);

out_module:
	module_put(THIS_MODULE);

	return rc;
}

static int msm_11ad_remove(struct platform_device *pdev)
{
	struct msm11ad_ctx *ctx = platform_get_drvdata(pdev);

	msm_pcie_deregister_event(&ctx->pci_event);
	msm_11ad_ssr_deinit(ctx);
	list_del(&ctx->list);
	dev_info(ctx->dev, "%s: pdev %pK pcidev %pK\n", __func__, pdev,
		 ctx->pcidev);
	kfree(ctx->pristine_state);
	kfree(ctx->golden_state);

	pci_dev_put(ctx->pcidev);
	if (ctx->gpio_en >= 0) {
		gpio_direction_output(ctx->gpio_en, 0);
		gpio_free(ctx->gpio_en);
	}
	if (ctx->gpio_dc >= 0) {
		gpio_direction_output(ctx->gpio_dc, 0);
		gpio_free(ctx->gpio_dc);
	}
	if (ctx->sleep_clk_en >= 0)
		gpio_free(ctx->sleep_clk_en);

	msm_11ad_disable_clocks(ctx);
	msm_11ad_release_clocks(ctx);
	msm_11ad_disable_vregs(ctx);
	msm_11ad_release_vregs(ctx);

	module_put(THIS_MODULE);

	return 0;
}

static const struct of_device_id msm_11ad_of_match[] = {
	{ .compatible = "qcom,wil6210", },
	{},
};

static struct platform_driver msm_11ad_driver = {
	.driver = {
		.name = "msm_11ad",
		.of_match_table = msm_11ad_of_match,
	},
	.probe = msm_11ad_probe,
	.remove = msm_11ad_remove,
};
module_platform_driver(msm_11ad_driver);

static void msm_11ad_set_affinity_hint(struct msm11ad_ctx *ctx, uint irq,
				       struct cpumask *boost_cpu)
{
	/*
	 * There is a very small window where user space can change the
	 * affinity after we changed it here and before setting the
	 * NO_BALANCING flag. Retry this several times as a workaround.
	 */
	int retries = 5, rc;
	struct irq_desc *desc;

	while (retries > 0) {
		irq_modify_status(irq, IRQ_NO_BALANCING, 0);
		rc = irq_set_affinity_hint(irq, boost_cpu);
		if (rc)
			dev_warn(ctx->dev,
				"Failed set affinity, rc=%d\n", rc);
		irq_modify_status(irq, 0, IRQ_NO_BALANCING);
		desc = irq_to_desc(irq);
		if (cpumask_equal(desc->irq_common_data.affinity, boost_cpu))
			break;
		retries--;
	}

	if (!retries)
		dev_warn(ctx->dev, "failed to set CPU boost affinity\n");
}

static void msm_11ad_set_boost_affinity(struct msm11ad_ctx *ctx)
{
	msm_11ad_set_affinity_hint(ctx, ctx->pcidev->irq, &ctx->boost_cpu_0);
	/* boost rx and tx interrupts */
	if (ctx->features & BIT(WIL_PLATFORM_FEATURE_TRIPLE_MSI))
		msm_11ad_set_affinity_hint(ctx, ctx->pcidev->irq + 1,
					  &ctx->boost_cpu_1);
}

static void msm_11ad_clear_affinity_hint(struct msm11ad_ctx *ctx, uint irq)
{
	int rc;

	irq_modify_status(irq, IRQ_NO_BALANCING, 0);
	rc = irq_set_affinity_hint(irq, NULL);
	if (rc)
		dev_warn(ctx->dev, "Failed clear affinity, rc=%d\n", rc);
}

static void msm_11ad_clear_boost_affinity(struct msm11ad_ctx *ctx)
{
	msm_11ad_clear_affinity_hint(ctx, ctx->pcidev->irq);
	if (ctx->features & BIT(WIL_PLATFORM_FEATURE_TRIPLE_MSI))
		msm_11ad_clear_affinity_hint(ctx, ctx->pcidev->irq + 1);
}

/* hooks for the wil6210 driver */
static int ops_bus_request(void *handle, u32 kbps /* KBytes/Sec */)
{
	struct msm11ad_ctx *ctx = (struct msm11ad_ctx *)handle;
	int rc, i;
	int vote = 0; /* vote 0 in case requested kbps cannot be satisfied */
	struct msm_bus_paths *usecase;
	u32 usecase_kbps;
	u32 min_kbps = ~0;

	/* find the lowest usecase that is bigger than requested kbps */
	for (i = 0; i < ctx->bus_scale->num_usecases; i++) {
		usecase = &ctx->bus_scale->usecase[i];
		/*
		 * assume we have single path (vectors[0]). If we ever
		 * have multiple paths, need to define the behavior
		 */
		usecase_kbps = div64_u64(usecase->vectors[0].ab, 1000);
		if (usecase_kbps >= kbps && usecase_kbps < min_kbps) {
			min_kbps = usecase_kbps;
			vote = i;
		}
	}

	rc = msm_bus_scale_client_update_request(ctx->msm_bus_handle, vote);
	if (rc)
		dev_err(ctx->dev,
			"Failed msm_bus voting. kbps=%d vote=%d, rc=%d\n",
			kbps, vote, rc);

	if (ctx->use_cpu_boost) {
		bool was_boosted = ctx->is_cpu_boosted;
		bool needs_boost = (kbps >= WIGIG_MIN_CPU_BOOST_KBPS);

		if (was_boosted != needs_boost) {
			if (needs_boost) {
				rc = core_ctl_set_boost(true);
				if (rc) {
					dev_err(ctx->dev,
						"Failed enable boost rc=%d\n",
						rc);
					goto out;
				}
				msm_11ad_set_boost_affinity(ctx);
				dev_dbg(ctx->dev, "CPU boost enabled\n");
			} else {
				rc = core_ctl_set_boost(false);
				if (rc)
					dev_err(ctx->dev,
						"Failed disable boost rc=%d\n",
						rc);
				msm_11ad_clear_boost_affinity(ctx);
				dev_dbg(ctx->dev, "CPU boost disabled\n");
			}
			ctx->is_cpu_boosted = needs_boost;
		}
	}
out:
	return rc;
}

static void ops_uninit(void *handle)
{
	struct msm11ad_ctx *ctx = (struct msm11ad_ctx *)handle;

	if (ctx->msm_bus_handle) {
		msm_bus_scale_unregister_client(ctx->msm_bus_handle);
		ctx->msm_bus_handle = 0;
	}

	memset(&ctx->rops, 0, sizeof(ctx->rops));
	ctx->wil_handle = NULL;

	msm_11ad_suspend_power_off(ctx);
}

static int msm_11ad_notify_crash(struct msm11ad_ctx *ctx)
{
	int rc;

	if (ctx->subsys) {
		dev_info(ctx->dev, "SSR requested\n");
		(void)msm_11ad_ssr_copy_ramdump(ctx);
		ctx->recovery_in_progress = true;
		subsys_set_crash_status(ctx->subsys, CRASH_STATUS_ERR_FATAL);
		rc = subsystem_restart_dev(ctx->subsys);
		if (rc) {
			dev_err(ctx->dev,
				"subsystem_restart_dev fail: %d\n", rc);
			ctx->recovery_in_progress = false;
		}
	}

	return 0;
}

static int ops_notify(void *handle, enum wil_platform_event evt)
{
	struct msm11ad_ctx *ctx = (struct msm11ad_ctx *)handle;
	int rc = 0;
	struct pci_dev *pcidev = ctx->pcidev;

	switch (evt) {
	case WIL_PLATFORM_EVT_FW_CRASH:
		rc = msm_11ad_notify_crash(ctx);
		break;
	case WIL_PLATFORM_EVT_PRE_RESET:
		/*
		 * Enable rf_clk clock before resetting the device to ensure
		 * stable ref clock during the device reset
		 */
		if (ctx->features &
		    BIT(WIL_PLATFORM_FEATURE_FW_EXT_CLK_CONTROL)) {
			rc = msm_11ad_enable_clk(ctx, &ctx->rf_clk);
			if (rc) {
				dev_err(ctx->dev,
					"failed to enable clk, rc %d\n", rc);
				break;
			}
		}
		break;
	case WIL_PLATFORM_EVT_FW_RDY:
		/*
		 * Disable rf_clk clock after the device is up to allow
		 * the device to control it via its GPIO for power saving
		 */
		if (ctx->features &
		    BIT(WIL_PLATFORM_FEATURE_FW_EXT_CLK_CONTROL))
			msm_11ad_disable_clk(ctx, &ctx->rf_clk);

		/*
		 * Save golden config space for pci linkdown recovery.
		 * golden_state is also saved after enumeration, free the old
		 * saved state before reallocating
		 */
		rc = pci_save_state(pcidev);
		if (rc) {
			dev_err(ctx->dev, "pci_save_state failed :%d\n", rc);
			return rc;
		}
		kfree(ctx->golden_state);
		ctx->golden_state = pci_store_saved_state(pcidev);
		break;
	default:
		pr_debug("%s: Unhandled event %d\n", __func__, evt);
		break;
	}

	return rc;
}

static int ops_get_capa(void *handle)
{
	struct msm11ad_ctx *ctx = (struct msm11ad_ctx *)handle;
	int capa;

	pr_debug("%s: keep radio on during sleep is %s\n", __func__,
		 ctx->keep_radio_on_during_sleep ? "allowed" : "not allowed");

	capa = (ctx->keep_radio_on_during_sleep ?
			BIT(WIL_PLATFORM_CAPA_RADIO_ON_IN_SUSPEND) : 0) |
		BIT(WIL_PLATFORM_CAPA_T_PWR_ON_0) |
		BIT(WIL_PLATFORM_CAPA_EXT_CLK);
	if (!ctx->smmu_s1_bypass)
		capa |= BIT(WIL_PLATFORM_CAPA_SMMU);

	pr_debug("%s: use AP power save is %s\n", __func__, ctx->use_ap_ps ?
		 "allowed" : "not allowed");

	if (ctx->use_ap_ps)
		capa |= BIT(WIL_PLATFORM_CAPA_AP_PS);

	return capa;
}

static void ops_set_features(void *handle, int features)
{
	struct msm11ad_ctx *ctx = (struct msm11ad_ctx *)handle;

	pr_debug("%s: features 0x%x\n", __func__, features);
	ctx->features = features;
}

void *msm_11ad_dev_init(struct device *dev, struct wil_platform_ops *ops,
			const struct wil_platform_rops *rops, void *wil_handle)
{
	struct pci_dev *pcidev = to_pci_dev(dev);
	struct msm11ad_ctx *ctx = pcidev2ctx(pcidev);
	struct iommu_domain *domain;
	int bypass = 0;
	int fastmap = 0;
	int coherent = 0;

	if (!ctx) {
		pr_err("Context not found for pcidev %pK\n", pcidev);
		return NULL;
	}

	/* bus scale */
	ctx->msm_bus_handle =
		msm_bus_scale_register_client(ctx->bus_scale);
	if (!ctx->msm_bus_handle) {
		dev_err(ctx->dev, "Failed msm_bus registration\n");
		return NULL;
	}
	dev_info(ctx->dev, "msm_bus handle 0x%x\n", ctx->msm_bus_handle);

	domain = iommu_get_domain_for_dev(&pcidev->dev);
	if (domain) {
		iommu_domain_get_attr(domain, DOMAIN_ATTR_S1_BYPASS, &bypass);
		iommu_domain_get_attr(domain, DOMAIN_ATTR_FAST, &fastmap);
		iommu_domain_get_attr(domain,
				      DOMAIN_ATTR_PAGE_TABLE_IS_COHERENT,
				      &coherent);

		dev_info(ctx->dev, "SMMU initialized, bypass=%d, fastmap=%d, coherent=%d\n",
			 bypass, fastmap, coherent);
	} else {
		dev_warn(ctx->dev, "Unable to get iommu domain\n");
	}
	ctx->smmu_s1_bypass = bypass;

	/* subsystem restart */
	if (rops) {
		ctx->rops = *rops;
		ctx->wil_handle = wil_handle;
	}

	/* fill ops */
	memset(ops, 0, sizeof(*ops));
	ops->bus_request = ops_bus_request;
	ops->suspend = ops_suspend;
	ops->resume = ops_resume;
	ops->uninit = ops_uninit;
	ops->notify = ops_notify;
	ops->get_capa = ops_get_capa;
	ops->set_features = ops_set_features;
	ops->pci_linkdown_recovery = ops_pci_linkdown_recovery;

	return ctx;
}
EXPORT_SYMBOL(msm_11ad_dev_init);

int msm_11ad_modinit(void)
{
	struct msm11ad_ctx *ctx = list_first_entry_or_null(&dev_list,
							   struct msm11ad_ctx,
							   list);

	if (!ctx) {
		pr_err("Context not found\n");
		return -EINVAL;
	}

	ctx->subsys_handle = subsystem_get(ctx->subsysdesc.name);

	return msm_11ad_resume_power_on(ctx);
}
EXPORT_SYMBOL(msm_11ad_modinit);

void msm_11ad_modexit(void)
{
	struct msm11ad_ctx *ctx = list_first_entry_or_null(&dev_list,
							   struct msm11ad_ctx,
							   list);

	if (!ctx) {
		pr_err("Context not found\n");
		return;
	}

	if (ctx->subsys_handle) {
		subsystem_put(ctx->subsys_handle);
		ctx->subsys_handle = NULL;
	}
}
EXPORT_SYMBOL(msm_11ad_modexit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for Qualcomm Technologies, Inc. 11ad card");

