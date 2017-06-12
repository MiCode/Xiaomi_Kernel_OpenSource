/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#define WIGIG_VENDOR (0x1ae9)
#define WIGIG_DEVICE (0x0310)

#define SMMU_BASE	0x20000000 /* Device address range base */
#define SMMU_SIZE	((SZ_1G * 4ULL) - SMMU_BASE)

#define WIGIG_ENABLE_DELAY	50

#define WIGIG_SUBSYS_NAME	"WIGIG"
#define WIGIG_RAMDUMP_SIZE    0x200000 /* maximum ramdump size */
#define WIGIG_DUMP_FORMAT_VER   0x1
#define WIGIG_DUMP_MAGIC_VER_V1 0x57474947
#define VDD_MIN_UV	1028000
#define VDD_MAX_UV	1028000
#define VDD_MAX_UA	575000
#define VDDIO_MIN_UV	1950000
#define VDDIO_MAX_UV	2040000
#define VDDIO_MAX_UA	70300

#define PCIE20_CAP_LINKCTRLSTATUS 0x80

#define WIGIG_MIN_CPU_BOOST_KBPS	150000

struct device;

static const char * const gpio_en_name = "qcom,wigig-en";
static const char * const sleep_clk_en_name = "qcom,sleep-clk-en";

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
	int sleep_clk_en; /* sleep clock enable for low PM management */

	/* pci device */
	u32 rc_index; /* PCIE root complex index */
	struct pci_dev *pcidev;
	struct pci_saved_state *pristine_state;
	bool l1_enabled_in_enum;

	/* SMMU */
	bool use_smmu; /* have SMMU enabled? */
	int smmu_s1_en;
	int smmu_fast_map;
	int smmu_coherent;
	struct dma_iommu_mapping *mapping;
	u32 smmu_base;
	u32 smmu_size;

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

	/* external vregs and clocks */
	struct msm11ad_vreg vdd;
	struct msm11ad_vreg vddio;
	struct msm11ad_clk rf_clk3;
	struct msm11ad_clk rf_clk3_pin;

	/* cpu boost support */
	bool use_cpu_boost;
	bool is_cpu_boosted;
	struct cpumask boost_cpu;

	bool keep_radio_on_during_sleep;
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

	dev_info(dev, "%s: %s initialized successfully\n", __func__, name);

out:
	return rc;
}

static int msm_11ad_release_vreg(struct device *dev, struct msm11ad_vreg *vreg)
{
	if (!vreg || !vreg->reg)
		return 0;

	dev_info(dev, "%s: %s released\n", __func__, vreg->name);

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
		dev_err(dev, "%s: failed to get %s rc %d",
				__func__, name, rc);
		kfree(clk->name);
		clk->clk = NULL;
		goto out;
	}

	dev_info(dev, "%s: %s initialized successfully\n", __func__, name);

out:
	return rc;
}

static int msm_11ad_release_clk(struct device *dev, struct msm11ad_clk *clk)
{
	if (!clk || !clk->clk)
		return 0;

	dev_info(dev, "%s: %s released\n", __func__, clk->name);

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

	return rc;

vddio_fail:
	msm_11ad_release_vreg(dev, &ctx->vdd);
out:
	return rc;
}

static void msm_11ad_release_vregs(struct msm11ad_ctx *ctx)
{
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

	dev_info(dev, "%s: %s enabled\n", __func__, vreg->name);

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

	dev_info(dev, "%s: %s disabled\n", __func__, vreg->name);

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

	return rc;

vddio_fail:
	msm_11ad_disable_vreg(ctx, &ctx->vdd);
out:
	return rc;
}

static int msm_11ad_disable_vregs(struct msm11ad_ctx *ctx)
{
	if (!ctx->vdd.reg && !ctx->vddio.reg)
		goto out;

	/* ignore errors on disable vreg */
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

	rc = msm_11ad_enable_clk(ctx, &ctx->rf_clk3);
	if (rc)
		return rc;

	rc = msm_11ad_enable_clk(ctx, &ctx->rf_clk3_pin);
	if (rc)
		msm_11ad_disable_clk(ctx, &ctx->rf_clk3);

	return rc;
}

static int msm_11ad_init_clocks(struct msm11ad_ctx *ctx)
{
	int rc;
	struct device *dev = ctx->dev;

	if (!of_property_read_bool(dev->of_node, "qcom,use-ext-clocks"))
		return 0;

	rc = msm_11ad_init_clk(dev, &ctx->rf_clk3, "rf_clk3_clk");
	if (rc)
		return rc;

	rc = msm_11ad_init_clk(dev, &ctx->rf_clk3_pin, "rf_clk3_pin_clk");
	if (rc)
		msm_11ad_release_clk(ctx->dev, &ctx->rf_clk3);

	return rc;
}

static void msm_11ad_release_clocks(struct msm11ad_ctx *ctx)
{
	msm_11ad_release_clk(ctx->dev, &ctx->rf_clk3_pin);
	msm_11ad_release_clk(ctx->dev, &ctx->rf_clk3);
}

static void msm_11ad_disable_clocks(struct msm11ad_ctx *ctx)
{
	msm_11ad_disable_clk(ctx, &ctx->rf_clk3_pin);
	msm_11ad_disable_clk(ctx, &ctx->rf_clk3);
}

int msm_11ad_ctrl_aspm_l1(struct msm11ad_ctx *ctx, bool enable)
{
	int rc;
	u32 val;
	struct pci_dev *pdev = ctx->pcidev;
	bool l1_enabled;

	/* Read current state */
	rc = pci_read_config_dword(pdev,
				   PCIE20_CAP_LINKCTRLSTATUS, &val);
	if (rc) {
		dev_err(ctx->dev,
			"reading PCIE20_CAP_LINKCTRLSTATUS failed:%d\n", rc);
		return rc;
	}
	dev_dbg(ctx->dev, "PCIE20_CAP_LINKCTRLSTATUS read returns 0x%x\n", val);

	l1_enabled = val & PCI_EXP_LNKCTL_ASPM_L1;
	if (l1_enabled == enable) {
		dev_dbg(ctx->dev, "ASPM_L1 is already %s\n",
			l1_enabled ? "enabled" : "disabled");
		return 0;
	}

	if (enable)
		val |= PCI_EXP_LNKCTL_ASPM_L1; /* enable bit 1 */
	else
		val &= ~PCI_EXP_LNKCTL_ASPM_L1; /* disable bit 1 */

	dev_dbg(ctx->dev, "writing PCIE20_CAP_LINKCTRLSTATUS (val 0x%x)\n",
		val);
	rc = pci_write_config_dword(pdev,
				    PCIE20_CAP_LINKCTRLSTATUS, val);
	if (rc)
		dev_err(ctx->dev,
			"writing PCIE20_CAP_LINKCTRLSTATUS (val 0x%x) failed:%d\n",
			val, rc);

	return rc;
}

static int msm_11ad_turn_device_power_off(struct msm11ad_ctx *ctx)
{
	if (ctx->gpio_en >= 0)
		gpio_direction_output(ctx->gpio_en, 0);

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

	msm_pcie_shadow_control(ctx->pcidev, 0);

	rc = pci_save_state(pcidev);
	if (rc) {
		dev_err(ctx->dev, "pci_save_state failed :%d\n", rc);
		goto out;
	}
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

	msm_pcie_shadow_control(pcidev, 0);

	dev_dbg(ctx->dev, "disable device and save config\n");
	pci_disable_device(pcidev);
	pci_save_state(pcidev);
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

	msm_pcie_shadow_control(ctx->pcidev, 1);

	/* Disable L1, in case it is enabled */
	if (ctx->l1_enabled_in_enum) {
		rc = msm_11ad_ctrl_aspm_l1(ctx, false);
		if (rc) {
			dev_err(ctx->dev,
				"failed to disable L1, rc %d\n", rc);
			goto err_suspend_rc;
		}
	}

	return 0;

err_suspend_rc:
	msm_pcie_pm_control(MSM_PCIE_SUSPEND, pcidev->bus->number,
			    pcidev, NULL, 0);
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

	msm_pcie_shadow_control(pcidev, 1);

	dev_dbg(ctx->dev, "pci set master\n");
	pci_set_master(pcidev);

out:
	return rc;
}

static int msm_11ad_smmu_init(struct msm11ad_ctx *ctx)
{
	int atomic_ctx = 1;
	int rc;
	int force_pt_coherent = 1;
	int smmu_bypass = !ctx->smmu_s1_en;
	dma_addr_t iova_base = 0;
	dma_addr_t iova_end =  ctx->smmu_base + ctx->smmu_size - 1;
	struct iommu_domain_geometry geometry;

	if (!ctx->use_smmu)
		return 0;

	dev_info(ctx->dev, "Initialize SMMU, bypass=%d, fastmap=%d, coherent=%d\n",
		 smmu_bypass, ctx->smmu_fast_map, ctx->smmu_coherent);

	ctx->mapping = arm_iommu_create_mapping(&platform_bus_type,
						ctx->smmu_base, ctx->smmu_size);
	if (IS_ERR_OR_NULL(ctx->mapping)) {
		rc = PTR_ERR(ctx->mapping) ?: -ENODEV;
		dev_err(ctx->dev, "Failed to create IOMMU mapping (%d)\n", rc);
		return rc;
	}

	rc = iommu_domain_set_attr(ctx->mapping->domain,
				   DOMAIN_ATTR_ATOMIC,
				   &atomic_ctx);
	if (rc) {
		dev_err(ctx->dev, "Set atomic attribute to SMMU failed (%d)\n",
			rc);
		goto release_mapping;
	}

	if (smmu_bypass) {
		rc = iommu_domain_set_attr(ctx->mapping->domain,
					   DOMAIN_ATTR_S1_BYPASS,
					   &smmu_bypass);
		if (rc) {
			dev_err(ctx->dev, "Set bypass attribute to SMMU failed (%d)\n",
				rc);
			goto release_mapping;
		}
	} else {
		/* Set dma-coherent and page table coherency */
		if (ctx->smmu_coherent) {
			arch_setup_dma_ops(&ctx->pcidev->dev, 0, 0, NULL, true);
			rc = iommu_domain_set_attr(ctx->mapping->domain,
				   DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
				   &force_pt_coherent);
			if (rc) {
				dev_err(ctx->dev,
					"Set SMMU PAGE_TABLE_FORCE_COHERENT attr failed (%d)\n",
					rc);
				goto release_mapping;
			}
		}

		if (ctx->smmu_fast_map) {
			rc = iommu_domain_set_attr(ctx->mapping->domain,
						   DOMAIN_ATTR_FAST,
						   &ctx->smmu_fast_map);
			if (rc) {
				dev_err(ctx->dev, "Set fast attribute to SMMU failed (%d)\n",
					rc);
				goto release_mapping;
			}
			memset(&geometry, 0, sizeof(geometry));
			geometry.aperture_start = iova_base;
			geometry.aperture_end = iova_end;
			rc = iommu_domain_set_attr(ctx->mapping->domain,
						   DOMAIN_ATTR_GEOMETRY,
						   &geometry);
			if (rc) {
				dev_err(ctx->dev, "Set geometry attribute to SMMU failed (%d)\n",
					rc);
				goto release_mapping;
			}
		}
	}

	rc = arm_iommu_attach_device(&ctx->pcidev->dev, ctx->mapping);
	if (rc) {
		dev_err(ctx->dev, "arm_iommu_attach_device failed (%d)\n", rc);
		goto release_mapping;
	}
	dev_info(ctx->dev, "attached to IOMMU\n");

	return 0;
release_mapping:
	arm_iommu_release_mapping(ctx->mapping);
	ctx->mapping = NULL;
	return rc;
}

static int msm_11ad_ssr_shutdown(const struct subsys_desc *subsys,
				 bool force_stop)
{
	pr_info("%s(%p,%d)\n", __func__, subsys, force_stop);
	/* nothing is done in shutdown. We do full recovery in powerup */
	return 0;
}

static int msm_11ad_ssr_powerup(const struct subsys_desc *subsys)
{
	int rc = 0;
	struct platform_device *pdev;
	struct msm11ad_ctx *ctx;

	pr_info("%s(%p)\n", __func__, subsys);

	pdev = to_platform_device(subsys->dev);
	ctx = platform_get_drvdata(pdev);

	if (!ctx)
		return -ENODEV;

	if (ctx->recovery_in_progress) {
		if (ctx->rops.fw_recovery && ctx->wil_handle) {
			dev_info(ctx->dev, "requesting FW recovery\n");
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
					   WIGIG_RAMDUMP_SIZE);
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
	segment.size = WIGIG_RAMDUMP_SIZE;

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

	ctx->ramdump_dev = create_ramdump_device(ctx->subsysdesc.name,
						 ctx->subsysdesc.dev);
	if (!ctx->ramdump_dev) {
		dev_err(ctx->dev, "Create ramdump device failed\n");
		rc = -ENOMEM;
		goto out_rc;
	}

	/* register ramdump area */
	ctx->ramdump_addr = kmalloc(WIGIG_RAMDUMP_SIZE, GFP_KERNEL);
	if (!ctx->ramdump_addr) {
		rc = -ENOMEM;
		goto out_rc;
	}

	ctx->dump_data.addr = virt_to_phys(ctx->ramdump_addr);
	ctx->dump_data.len = WIGIG_RAMDUMP_SIZE;
	strlcpy(ctx->dump_data.name, "KWIGIG",
			 sizeof(ctx->dump_data.name));
	dump_entry.id = MSM_DUMP_DATA_WIGIG;
	dump_entry.addr = virt_to_phys(&ctx->dump_data);

	rc = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
	if (rc) {
		dev_err(ctx->dev, "Dump table setup failed: %d\n", rc);
		kfree(ctx->ramdump_addr);
		goto out_rc;
	}

	return 0;
out_rc:
	msm_11ad_ssr_deinit(ctx);
	return rc;
}

static void msm_11ad_init_cpu_boost(struct msm11ad_ctx *ctx)
{
	unsigned int minfreq = 0, maxfreq = 0, freq;
	int i, boost_cpu = 0;

	for_each_possible_cpu(i) {
		freq = cpufreq_quick_get_max(i);
		if (freq > maxfreq) {
			maxfreq = freq;
			boost_cpu = i;
		}
		if (!minfreq || freq < minfreq)
			minfreq = freq;
	}

	if (minfreq != maxfreq) {
		/*
		 * use first big core for boost, to be compatible with WLAN
		 * which assigns big cores from the last index
		 */
		ctx->use_cpu_boost = true;
		cpumask_clear(&ctx->boost_cpu);
		cpumask_set_cpu(boost_cpu, &ctx->boost_cpu);
		dev_info(ctx->dev, "CPU boost: will use core %d\n", boost_cpu);
	} else {
		ctx->use_cpu_boost = false;
		dev_info(ctx->dev, "CPU boost disabled, uniform topology\n");
	}
}

static int msm_11ad_probe(struct platform_device *pdev)
{
	struct msm11ad_ctx *ctx;
	struct device *dev = &pdev->dev;
	struct device_node *of_node = dev->of_node;
	struct device_node *rc_node;
	struct pci_dev *pcidev = NULL;
	u32 smmu_mapping[2];
	int rc;
	u32 val;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;

	/*== parse ==*/

	/* Information pieces:
	 * - of_node stands for "wil6210":
	 *	wil6210: qcom,wil6210 {
	 *	compatible = "qcom,wil6210";
	 *	qcom,pcie-parent = <&pcie1>;
	 *	qcom,wigig-en = <&tlmm 94 0>; (ctx->gpio_en)
	 *	qcom,sleep-clk-en = <&pm8994_gpios 18 0>; (ctx->sleep_clk_en)
	 *	qcom,msm-bus,name = "wil6210";
	 *	qcom,msm-bus,num-cases = <2>;
	 *	qcom,msm-bus,num-paths = <1>;
	 *	qcom,msm-bus,vectors-KBps =
	 *		<100 512 0 0>,
	 *		<100 512 600000 800000>;
	 *	qcom,smmu-support;
	 *};
	 * rc_node stands for "qcom,pcie", selected entries:
	 * cell-index = <1>; (ctx->rc_index)
	 * iommus = <&anoc0_smmu>;
	 * qcom,smmu-exist;
	 */

	/* wigig-en is optional property */
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
		return -EINVAL;
	}
	rc = of_property_read_u32(rc_node, "cell-index", &ctx->rc_index);
	if (rc < 0) {
		dev_err(ctx->dev, "Parent PCIE device index not found\n");
		return -EINVAL;
	}
	ctx->use_smmu = of_property_read_bool(of_node, "qcom,smmu-support");
	ctx->keep_radio_on_during_sleep = of_property_read_bool(of_node,
		"qcom,keep-radio-on-during-sleep");
	ctx->bus_scale = msm_bus_cl_get_pdata(pdev);

	ctx->smmu_s1_en = of_property_read_bool(of_node, "qcom,smmu-s1-en");
	if (ctx->smmu_s1_en) {
		ctx->smmu_fast_map = of_property_read_bool(
						of_node, "qcom,smmu-fast-map");
		ctx->smmu_coherent = of_property_read_bool(
						of_node, "qcom,smmu-coherent");
	}
	rc = of_property_read_u32_array(dev->of_node, "qcom,smmu-mapping",
			smmu_mapping, 2);
	if (rc) {
		dev_err(ctx->dev,
			"Failed to read base/size smmu addresses %d, fallback to default\n",
			rc);
		ctx->smmu_base = SMMU_BASE;
		ctx->smmu_size = SMMU_SIZE;
	} else {
		ctx->smmu_base = smmu_mapping[0];
		ctx->smmu_size = smmu_mapping[1];
	}
	dev_dbg(ctx->dev, "smmu_base=0x%x smmu_sise=0x%x\n",
		ctx->smmu_base, ctx->smmu_size);

	/*== execute ==*/
	/* turn device on */
	rc = msm_11ad_init_vregs(ctx);
	if (rc) {
		dev_err(ctx->dev, "msm_11ad_init_vregs failed: %d\n", rc);
		return rc;
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
	do {
		pcidev = pci_get_device(WIGIG_VENDOR, WIGIG_DEVICE, pcidev);
		if (!pcidev)
			break;

		if (pci_domain_nr(pcidev->bus) == ctx->rc_index)
			break;
	} while (true);
	if (!pcidev) {
		rc = -ENODEV;
		dev_err(ctx->dev, "Wigig device %4x:%4x not found\n",
			WIGIG_VENDOR, WIGIG_DEVICE);
		goto out_rc;
	}
	ctx->pcidev = pcidev;

	/* Read current state */
	rc = pci_read_config_dword(pcidev,
				   PCIE20_CAP_LINKCTRLSTATUS, &val);
	if (rc) {
		dev_err(ctx->dev,
			"reading PCIE20_CAP_LINKCTRLSTATUS failed:%d\n",
			rc);
		goto out_rc;
	}

	ctx->l1_enabled_in_enum = val & PCI_EXP_LNKCTL_ASPM_L1;
	dev_dbg(ctx->dev, "L1 is %s in enumeration\n",
		ctx->l1_enabled_in_enum ? "enabled" : "disabled");

	/* Disable L1, in case it is enabled */
	if (ctx->l1_enabled_in_enum) {
		rc = msm_11ad_ctrl_aspm_l1(ctx, false);
		if (rc) {
			dev_err(ctx->dev,
				"failed to disable L1, rc %d\n", rc);
			goto out_rc;
		}
	}

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
		goto out_rc;
	}

	msm_11ad_init_cpu_boost(ctx);

	/* report */
	dev_info(ctx->dev, "msm_11ad discovered. %p {\n"
		 "  gpio_en = %d\n"
		 "  sleep_clk_en = %d\n"
		 "  rc_index = %d\n"
		 "  use_smmu = %d\n"
		 "  pcidev = %p\n"
		 "}\n", ctx, ctx->gpio_en, ctx->sleep_clk_en, ctx->rc_index,
		 ctx->use_smmu, ctx->pcidev);

	platform_set_drvdata(pdev, ctx);
	device_disable_async_suspend(&pcidev->dev);

	list_add_tail(&ctx->list, &dev_list);
	msm_11ad_suspend_power_off(ctx);

	return 0;
out_rc:
	if (ctx->gpio_en >= 0)
		gpio_direction_output(ctx->gpio_en, 0);
out_set:
	if (ctx->gpio_en >= 0)
		gpio_free(ctx->gpio_en);
out_req:
	ctx->gpio_en = -EINVAL;
out_vreg_clk:
	msm_11ad_disable_clocks(ctx);
	msm_11ad_release_clocks(ctx);
	msm_11ad_disable_vregs(ctx);
	msm_11ad_release_vregs(ctx);

	return rc;
}

static int msm_11ad_remove(struct platform_device *pdev)
{
	struct msm11ad_ctx *ctx = platform_get_drvdata(pdev);

	msm_11ad_ssr_deinit(ctx);
	list_del(&ctx->list);
	dev_info(ctx->dev, "%s: pdev %p pcidev %p\n", __func__, pdev,
		 ctx->pcidev);
	kfree(ctx->pristine_state);

	msm_bus_cl_clear_pdata(ctx->bus_scale);
	pci_dev_put(ctx->pcidev);
	if (ctx->gpio_en >= 0) {
		gpio_direction_output(ctx->gpio_en, 0);
		gpio_free(ctx->gpio_en);
	}
	if (ctx->sleep_clk_en >= 0)
		gpio_free(ctx->sleep_clk_en);

	msm_11ad_disable_clocks(ctx);
	msm_11ad_release_clocks(ctx);
	msm_11ad_disable_vregs(ctx);
	msm_11ad_release_vregs(ctx);

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

static void msm_11ad_set_boost_affinity(struct msm11ad_ctx *ctx)
{
	/*
	 * There is a very small window where user space can change the
	 * affinity after we changed it here and before setting the
	 * NO_BALANCING flag. Retry this several times as a workaround.
	 */
	int retries = 5, rc;
	struct irq_desc *desc;

	while (retries > 0) {
		irq_modify_status(ctx->pcidev->irq, IRQ_NO_BALANCING, 0);
		rc = irq_set_affinity_hint(ctx->pcidev->irq, &ctx->boost_cpu);
		if (rc)
			dev_warn(ctx->dev,
				"Failed set affinity, rc=%d\n", rc);
		irq_modify_status(ctx->pcidev->irq, 0, IRQ_NO_BALANCING);
		desc = irq_to_desc(ctx->pcidev->irq);
		if (cpumask_equal(desc->irq_common_data.affinity,
				  &ctx->boost_cpu))
			break;
		retries--;
	}

	if (!retries)
		dev_warn(ctx->dev, "failed to set CPU boost affinity\n");
}

static void msm_11ad_clear_boost_affinity(struct msm11ad_ctx *ctx)
{
	int rc;

	irq_modify_status(ctx->pcidev->irq, IRQ_NO_BALANCING, 0);
	rc = irq_set_affinity_hint(ctx->pcidev->irq, NULL);
	if (rc)
		dev_warn(ctx->dev,
			 "Failed clear affinity, rc=%d\n", rc);
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
		/* assume we have single path (vectors[0]). If we ever
		 * have multiple paths, need to define the behavior */
		usecase_kbps = div64_u64(usecase->vectors[0].ib, 1000);
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

	if (ctx->use_smmu) {
		arm_iommu_detach_device(&ctx->pcidev->dev);
		arm_iommu_release_mapping(ctx->mapping);
		ctx->mapping = NULL;
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

	switch (evt) {
	case WIL_PLATFORM_EVT_FW_CRASH:
		rc = msm_11ad_notify_crash(ctx);
		break;
	case WIL_PLATFORM_EVT_PRE_RESET:
		/*
		 * TODO: Enable rf_clk3 clock before resetting the device to
		 * ensure stable ref clock during the device reset
		 */
		/* Re-enable L1 in case it was enabled in enumeration */
		if (ctx->l1_enabled_in_enum) {
			rc = msm_11ad_ctrl_aspm_l1(ctx, true);
			if (rc)
				dev_err(ctx->dev,
					"failed to enable L1, rc %d\n", rc);
		}
		break;
	case WIL_PLATFORM_EVT_FW_RDY:
		/*
		 * TODO: Disable rf_clk3 clock after the device is up to allow
		 * the device to control it via its GPIO for power saving
		 */
		break;
	default:
		pr_debug("%s: Unhandled event %d\n", __func__, evt);
		break;
	}

	return rc;
}

static bool ops_keep_radio_on_during_sleep(void *handle)
{
	struct msm11ad_ctx *ctx = (struct msm11ad_ctx *)handle;

	pr_debug("%s: keep radio on during sleep is %s\n", __func__,
		 ctx->keep_radio_on_during_sleep ? "allowed" : "not allowed");

	return ctx->keep_radio_on_during_sleep;
}

void *msm_11ad_dev_init(struct device *dev, struct wil_platform_ops *ops,
			const struct wil_platform_rops *rops, void *wil_handle)
{
	struct pci_dev *pcidev = to_pci_dev(dev);
	struct msm11ad_ctx *ctx = pcidev2ctx(pcidev);

	if (!ctx) {
		pr_err("Context not found for pcidev %p\n", pcidev);
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
	/* smmu */
	if (msm_11ad_smmu_init(ctx)) {
		msm_bus_scale_unregister_client(ctx->msm_bus_handle);
		ctx->msm_bus_handle = 0;
		return NULL;
	}

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
	ops->keep_radio_on_during_sleep = ops_keep_radio_on_during_sleep;

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
MODULE_DESCRIPTION("Platform driver for qcom 11ad card");

