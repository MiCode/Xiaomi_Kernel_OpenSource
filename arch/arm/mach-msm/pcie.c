/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

/*
 * MSM PCIe controller driver.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/clk/msm-clk.h>
#include <asm/mach/pci.h>
#include <mach/gpiomux.h>
#include <mach/hardware.h>
#include <mach/msm_iomap.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>

#include "pcie.h"

/* Root Complex Port vendor/device IDs */
#define PCIE_VENDOR_ID_RCP             0x17cb
#ifdef CONFIG_ARCH_MDM9630
#define PCIE_DEVICE_ID_RCP             0x300
#else
#define PCIE_DEVICE_ID_RCP             0x0101
#endif

#define PCIE20_PARF_SYS_CTRL           0x00
#define PCIE20_PARF_PM_STTS            0x24
#define PCIE20_PARF_PCS_DEEMPH         0x34
#define PCIE20_PARF_PCS_SWING          0x38
#define PCIE20_PARF_PHY_CTRL           0x40
#define PCIE20_PARF_PHY_REFCLK         0x4C
#define PCIE20_PARF_CONFIG_BITS        0x50
#define PCIE20_PARF_DBI_BASE_ADDR      0x168

#define PCIE20_ELBI_VERSION            0x00
#define PCIE20_ELBI_SYS_CTRL           0x04
#define PCIE20_ELBI_SYS_STTS	       0x08

#define PCIE20_CAP                     0x70
#define PCIE20_CAP_LINKCTRLSTATUS      (PCIE20_CAP + 0x10)

#define PCIE20_COMMAND_STATUS          0x04
#define PCIE20_BUSNUMBERS              0x18
#define PCIE20_MEMORY_BASE_LIMIT       0x20
#define PCIE20_L1SUB_CONTROL1          0x158
#define PCIE20_EP_L1SUB_CTL1_OFFSET    0x30
#define PCIE20_DEVICE_CONTROL2_STATUS2 0x98

#define PCIE20_ACK_F_ASPM_CTRL_REG     0x70C
#define PCIE20_ACK_N_FTS               0xff00

#define PCIE20_PLR_IATU_VIEWPORT       0x900
#define PCIE20_PLR_IATU_CTRL1          0x904
#define PCIE20_PLR_IATU_CTRL2          0x908
#define PCIE20_PLR_IATU_LBAR           0x90C
#define PCIE20_PLR_IATU_UBAR           0x910
#define PCIE20_PLR_IATU_LAR            0x914
#define PCIE20_PLR_IATU_LTAR           0x918
#define PCIE20_PLR_IATU_UTAR           0x91c

#define RD 0
#define WR 1

/* PM control options */
#define PM_IRQ                   0x1
#define PM_CLK                   0x2
#define PM_GPIO                  0x4
#define PM_VREG                  0x8
#define PM_PIPE_CLK              0x10
#define PM_ALL (PM_IRQ | PM_CLK | PM_GPIO | PM_VREG | PM_PIPE_CLK)

/* Timing Delays */
#define PERST_PROPAGATION_DELAY_US_MIN        10000
#define PERST_PROPAGATION_DELAY_US_MAX        15000
#define REFCLK_STABILIZATION_DELAY_US_MIN     1000
#define REFCLK_STABILIZATION_DELAY_US_MAX     1500
#define LINK_RETRY_TIMEOUT_US_MIN             20000
#define LINK_RETRY_TIMEOUT_US_MAX             25000
#define LINK_UP_TIMEOUT_US_MIN                5000
#define LINK_UP_TIMEOUT_US_MAX                5100
#define LINK_UP_CHECK_MAX_COUNT               20
#define PHY_STABILIZATION_DELAY_US_MIN        995
#define PHY_STABILIZATION_DELAY_US_MAX        1005

#define PHY_READY_TIMEOUT_COUNT               10
#define XMLH_LINK_UP                          0x400
#define MAX_LINK_RETRIES 5
#define MAX_BUS_NUM 3
#define MAX_PROP_SIZE 32

/* Config Space Offsets */
#define BDF_OFFSET(bus, device, function) \
	((bus << 24) | (device << 15) | (function << 8))

/* debug mask sys interface */
static int msm_pcie_debug_mask;
module_param_named(debug_mask, msm_pcie_debug_mask,
			    int, S_IRUGO | S_IWUSR | S_IWGRP);

struct mutex setup_lock;

/**
 *  PCIe driver state
 */
struct pcie_drv_sta {
	u32 rc_num;
	u32 rc_expected;
	u32 current_rc;
	bool vreg_on;
	struct mutex drv_lock;
} pcie_drv;

/* msm pcie device data */
static struct msm_pcie_dev_t msm_pcie_dev[MAX_RC_NUM];

/* regulators */
static struct msm_pcie_vreg_info_t msm_pcie_vreg_info[MSM_PCIE_MAX_VREG] = {
	{NULL, "vreg-3.3", 0, 0, 0, false},
	{NULL, "vreg-1.8", 1800000, 1800000, 1000, true},
	{NULL, "vreg-0.9", 1000000, 1000000, 24000, true}
};

/* GPIOs */
static struct msm_pcie_gpio_info_t msm_pcie_gpio_info[MSM_PCIE_MAX_GPIO] = {
	{"perst-gpio",      0, 1, 0, 0},
	{"wake-gpio",       0, 0, 0, 0},
	{"clkreq-gpio",     0, 0, 0, 0}
};

/* clocks */
static struct msm_pcie_clk_info_t
	msm_pcie_clk_info[MAX_RC_NUM][MSM_PCIE_MAX_CLK] = {
	{
	{NULL, "pcie_0_ref_clk_src", 0, false},
	{NULL, "pcie_0_aux_clk", 1010000, true},
	{NULL, "pcie_0_cfg_ahb_clk", 0, true},
	{NULL, "pcie_0_mstr_axi_clk", 0, true},
	{NULL, "pcie_0_slv_axi_clk", 0, true},
	{NULL, "pcie_0_ldo", 0, true}
	},
	{
	{NULL, "pcie_1_ref_clk_src", 0, false},
	{NULL, "pcie_1_aux_clk", 1010000, true},
	{NULL, "pcie_1_cfg_ahb_clk", 0, true},
	{NULL, "pcie_1_mstr_axi_clk", 0, true},
	{NULL, "pcie_1_slv_axi_clk", 0, true},
	{NULL, "pcie_1_ldo", 0, true}
	}
};

/* Pipe Clocks */
static struct msm_pcie_clk_info_t
	msm_pcie_pipe_clk_info[MAX_RC_NUM][MSM_PCIE_MAX_PIPE_CLK] = {
	{
	{NULL, "pcie_0_pipe_clk", 125000000, true},
	},
	{
	{NULL, "pcie_1_pipe_clk", 125000000, true},
	}
};

/* resources */
static const struct msm_pcie_res_info_t msm_pcie_res_info[MSM_PCIE_MAX_RES] = {
	{"parf",	0, 0},
	{"phy",     0, 0},
	{"dm_core",	0, 0},
	{"elbi",	0, 0},
	{"conf",	0, 0},
	{"io",		0, 0},
	{"bars",	0, 0}
};

/* irqs */
static const struct msm_pcie_irq_info_t msm_pcie_irq_info[MSM_PCIE_MAX_IRQ] = {
	{"int_msi",	0},
	{"int_a",	0},
	{"int_b",	0},
	{"int_c",	0},
	{"int_d",	0},
	{"int_pls_pme",		0},
	{"int_pme_legacy",	0},
	{"int_pls_err",		0},
	{"int_aer_legacy",	0},
	{"int_pls_link_up",	0},
	{"int_pls_link_down",	0},
	{"int_bridge_flush_n",	0},
	{"int_wake",	0}
};

int msm_pcie_get_debug_mask(void)
{
	return msm_pcie_debug_mask;
}

static void msm_pcie_write_mask(void __iomem *addr,
				uint32_t clear_mask, uint32_t set_mask)
{
	uint32_t val;

	val = (readl_relaxed(addr) & ~clear_mask) | set_mask;
	writel_relaxed(val, addr);
	wmb();  /* ensure data is written to hardware register */
}

static int msm_pcie_is_link_up(struct msm_pcie_dev_t *dev)
{
	return readl_relaxed(dev->dm_core +
			PCIE20_CAP_LINKCTRLSTATUS) & BIT(29);
}

static inline int msm_pcie_oper_conf(struct pci_bus *bus, u32 devfn, int oper,
				     int where, int size, u32 *val)
{
	uint32_t word_offset, byte_offset, mask;
	uint32_t rd_val, wr_val;
	struct msm_pcie_dev_t *dev;
	void __iomem *config_base;
	bool rc = false;
	u32 rc_idx;
	int rv = 0;

	dev = ((struct msm_pcie_dev_t *)
		(((struct pci_sys_data *)bus->sysdata)->private_data));

	if (!dev) {
		pr_err("PCIe: No device found for this bus.\n");
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	/* Do the bus->number based access control since we don't support
	   ECAM mechanism */

	switch (bus->number) {
	case 0:
		rc = true;
	case 1:
		rc_idx = dev->rc_idx;
		break;
	default:
		pr_err("PCIe: unsupported bus number.\n");
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	if ((bus->number > MAX_BUS_NUM) || (devfn != 0)) {
		PCIE_DBG("RC%d invalid %s - bus %d devfn %d\n", rc_idx,
			 (oper == RD) ? "rd" : "wr", bus->number, devfn);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	spin_lock_irqsave(&dev->cfg_lock, dev->irqsave_flags);

	if (!dev->cfg_access) {
		PCIE_DBG("Access denied for RC%d %d:0x%02x + 0x%04x[%d]\n",
			rc_idx, bus->number, devfn, where, size);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	if (dev->link_status != MSM_PCIE_LINK_ENABLED) {
		PCIE_DBG(
			"Access to RC%d %d:0x%02x + 0x%04x[%d] is denied because link is down\n",
			rc_idx, bus->number, devfn, where, size);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	/* check if the link is up for endpoint */
	if (!rc && !msm_pcie_is_link_up(dev)) {
			pr_err("RC%d %s fail, link down - bus %d devfn %d\n",
				rc_idx, (oper == RD) ? "rd" : "wr",
				bus->number, devfn);
			*val = ~0;
			rv = PCIBIOS_DEVICE_NOT_FOUND;
			goto unlock;
	}

	word_offset = where & ~0x3;
	byte_offset = where & 0x3;
	mask = (~0 >> (8 * (4 - size))) << (8 * byte_offset);

	config_base = rc ? dev->dm_core : dev->conf;
	rd_val = readl_relaxed(config_base + word_offset);

	if (oper == RD) {
		*val = ((rd_val & mask) >> (8 * byte_offset));
		PCIE_DBG("RC%d %d:0x%02x + 0x%04x[%d] -> 0x%08x; rd 0x%08x\n",
			rc_idx, bus->number, devfn, where, size, *val, rd_val);
	} else {
		wr_val = (rd_val & ~mask) |
				((*val << (8 * byte_offset)) & mask);
		writel_relaxed(wr_val, config_base + word_offset);
		wmb(); /* ensure config data is written to hardware register */

		PCIE_DBG(
			"RC%d %d:0x%02x + 0x%04x[%d] <- 0x%08x; rd 0x%08x val 0x%08x\n",
			rc_idx, bus->number, devfn, where, size,
			wr_val, rd_val, *val);
	}

unlock:
	spin_unlock_irqrestore(&dev->cfg_lock, dev->irqsave_flags);
out:
	return rv;
}

static int msm_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			    int size, u32 *val)
{
	int ret = msm_pcie_oper_conf(bus, devfn, RD, where, size, val);

	if ((bus->number == 0) && (where == PCI_CLASS_REVISION)) {
		*val = (*val & 0xff) | (PCI_CLASS_BRIDGE_PCI << 16);
		PCIE_DBG("change class for RC:0x%x\n", *val);
	}

	return ret;
}

static int msm_pcie_wr_conf(struct pci_bus *bus, u32 devfn,
			    int where, int size, u32 val)
{
	return msm_pcie_oper_conf(bus, devfn, WR, where, size, &val);
}

static struct pci_ops msm_pcie_ops = {
	.read = msm_pcie_rd_conf,
	.write = msm_pcie_wr_conf,
};

static int msm_pcie_gpio_init(struct msm_pcie_dev_t *dev)
{
	int rc, i;
	struct msm_pcie_gpio_info_t *info;

	PCIE_DBG("\n");

	for (i = 0; i < dev->gpio_n; i++) {
		info = &dev->gpio[i];

		if (!info->num)
			continue;

		rc = gpio_request(info->num, info->name);
		if (rc) {
			pr_err("can't get gpio %s; %d\n", info->name, rc);
			break;
		}

		if (info->out)
			rc = gpio_direction_output(info->num, info->init);
		else
			rc = gpio_direction_input(info->num);
		if (rc) {
			pr_err("can't set gpio direction %s; %d\n",
			       info->name, rc);
			gpio_free(info->num);
			break;
		}
	}

	if (rc)
		while (i--)
			gpio_free(dev->gpio[i].num);

	return rc;
}

static void msm_pcie_gpio_deinit(struct msm_pcie_dev_t *dev)
{
	int i;

	for (i = 0; i < dev->gpio_n; i++)
		gpio_free(dev->gpio[i].num);
}

int msm_pcie_vreg_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct regulator *vreg;
	struct msm_pcie_vreg_info_t *info;

	PCIE_DBG("\n");

	for (i = 0; i < MSM_PCIE_MAX_VREG; i++) {
		info = &dev->vreg[i];
		vreg = info->hdl;

		if (!vreg)
			continue;

		PCIE_DBG("Vreg %s is being enabled\n", info->name);
		if (info->max_v) {
			rc = regulator_set_voltage(vreg,
						   info->min_v, info->max_v);
			if (rc) {
				pr_err("can't set voltage %s; %d\n",
				       info->name, rc);
				break;
			}
		}

		if (info->opt_mode) {
			rc = regulator_set_optimum_mode(vreg, info->opt_mode);
			if (rc < 0) {
				pr_err("can't set mode %s; %d\n",
				       info->name, rc);
				break;
			}
		}

		rc = regulator_enable(vreg);
		if (rc) {
			pr_err("can't enable %s, %d\n", info->name, rc);
			break;
		}
	}

	if (rc)
		while (i--) {
			struct regulator *hdl = dev->vreg[i].hdl;
			if (hdl)
				regulator_disable(hdl);
		}

	return rc;
}

static void msm_pcie_vreg_deinit(struct msm_pcie_dev_t *dev)
{
	int i;

	for (i = MSM_PCIE_MAX_VREG - 1; i >= 0; i--) {
		if (dev->vreg[i].hdl) {
			PCIE_DBG("Vreg %s is being disabled\n",
				dev->vreg[i].name);
			regulator_disable(dev->vreg[i].hdl);
		}
	}
}

static int msm_pcie_clk_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;

	rc = regulator_enable(dev->gdsc);

	if (rc) {
		pr_err("PCIe: fail to enable GDSC for RC %s\n",
			dev->pdev->name);
		return rc;
	}

	if (dev->bus_client) {
		rc = msm_bus_scale_client_update_request(dev->bus_client, 1);
		if (rc) {
			pr_err(
				"PCIe:%s:fail to set bus bandwidth for RC %d:%d\n",
				__func__, dev->rc_idx, rc);
			return rc;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		info = &dev->clk[i];

		if (!info->hdl)
			continue;

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				pr_err("PCIe: can't set rate for clk %s: %d\n",
					info->name, rc);
				break;
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			pr_err("failed to enable clk %s\n", info->name);
		else
			PCIE_DBG("enable clk %s\n", info->name);
	}

	if (rc) {
		PCIE_DBG("disable clocks for error handling\n");
		while (i--) {
			struct clk *hdl = dev->clk[i].hdl;
			if (hdl)
				clk_disable_unprepare(hdl);
		}

		regulator_disable(dev->gdsc);
	}

	return rc;
}

static void msm_pcie_clk_deinit(struct msm_pcie_dev_t *dev)
{
	int i;
	int rc;

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++)
		if (dev->clk[i].hdl)
			clk_disable_unprepare(dev->clk[i].hdl);

	if (dev->bus_client) {
		rc = msm_bus_scale_client_update_request(dev->bus_client, 0);
		if (rc)
			pr_err(
				"PCIe:%s:fail to set bus bandwidth for RC %d:%d\n",
				__func__, dev->rc_idx, rc);
	}

	regulator_disable(dev->gdsc);
}

static int msm_pcie_pipe_clk_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		info = &dev->pipeclk[i];

		if (!info->hdl)
			continue;

		clk_reset(info->hdl, CLK_RESET_DEASSERT);

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				pr_err("PCIe: can't set rate for clk %s: %d\n",
					info->name, rc);
				break;
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			pr_err("failed to enable clk %s\n", info->name);
		else
			PCIE_DBG("enabled pipe clk %s\n", info->name);
	}

	if (rc) {
		PCIE_DBG("disable pipe clocks for error handling\n");
		while (i--)
			if (dev->pipeclk[i].hdl)
				clk_disable_unprepare(dev->pipeclk[i].hdl);
	}

	return rc;
}

static void msm_pcie_pipe_clk_deinit(struct msm_pcie_dev_t *dev)
{
	int i;

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++)
		if (dev->pipeclk[i].hdl)
			clk_disable_unprepare(
				dev->pipeclk[i].hdl);
}


static void msm_pcie_config_controller(struct msm_pcie_dev_t *dev)
{
	struct resource *axi_conf = dev->res[MSM_PCIE_RES_CONF].resource;
	u32 dev_conf, upper, lower, limit;

	PCIE_DBG("\n");

	if (IS_ENABLED(CONFIG_ARM_LPAE)) {
		lower = PCIE_LOWER_ADDR(axi_conf->start);
		upper = PCIE_UPPER_ADDR(axi_conf->start);
		limit = PCIE_LOWER_ADDR(axi_conf->end);
	} else {
		lower = axi_conf->start;
		upper = 0;
		limit = axi_conf->end;
	}

	dev_conf = BDF_OFFSET(1, 0, 0);

	/*
	 * program and enable address translation region 0 (device config
	 * address space); region type config;
	 * axi config address range to device config address range
	 */
	writel_relaxed(0, dev->dm_core + PCIE20_PLR_IATU_VIEWPORT);
	/* ensure that hardware locks the region before programming it */
	wmb();

	writel_relaxed(4, dev->dm_core + PCIE20_PLR_IATU_CTRL1);
	writel_relaxed(lower, dev->dm_core + PCIE20_PLR_IATU_LBAR);
	writel_relaxed(upper, dev->dm_core + PCIE20_PLR_IATU_UBAR);
	writel_relaxed(limit, dev->dm_core + PCIE20_PLR_IATU_LAR);
	writel_relaxed(dev_conf, dev->dm_core + PCIE20_PLR_IATU_LTAR);
	writel_relaxed(0, dev->dm_core + PCIE20_PLR_IATU_UTAR);
	writel_relaxed(BIT(31), dev->dm_core + PCIE20_PLR_IATU_CTRL2);
	/* ensure that hardware registers the configuration */
	wmb();
	PCIE_DBG("PCIE20_PLR_IATU_VIEWPORT:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_VIEWPORT));
	PCIE_DBG("PCIE20_PLR_IATU_CTRL1:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL1));
	PCIE_DBG("PCIE20_PLR_IATU_LBAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LBAR));
	PCIE_DBG("PCIE20_PLR_IATU_UBAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UBAR));
	PCIE_DBG("PCIE20_PLR_IATU_LAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LAR));
	PCIE_DBG("PCIE20_PLR_IATU_LTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LTAR));
	PCIE_DBG("PCIE20_PLR_IATU_UTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UTAR));
	PCIE_DBG("PCIE20_PLR_IATU_CTRL2:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL2));

	/* configure N_FTS */
	PCIE_DBG("Original PCIE20_ACK_F_ASPM_CTRL_REG:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG));
	if (!dev->n_fts)
		msm_pcie_write_mask(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG,
					0, BIT(15));
	else
		msm_pcie_write_mask(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG,
					PCIE20_ACK_N_FTS,
					dev->n_fts << 8);
	PCIE_DBG("Updated PCIE20_ACK_F_ASPM_CTRL_REG:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG));
}

static void msm_pcie_config_l1ss(struct msm_pcie_dev_t *dev)
{
	u32 offset = 0;

	if (!dev->rc_idx)
		offset = PCIE20_EP_L1SUB_CTL1_OFFSET;

	/* Enable the AUX Clock and the Core Clk to be synchronous for L1SS*/
	if (!dev->aux_clk_sync)
		msm_pcie_write_mask(dev->parf +
				PCIE20_PARF_SYS_CTRL, BIT(3), 0);

	/* Enable L1SS on RC */
	msm_pcie_write_mask(dev->dm_core + PCIE20_CAP_LINKCTRLSTATUS, 0,
					BIT(1)|BIT(0));
	msm_pcie_write_mask(dev->dm_core + PCIE20_L1SUB_CONTROL1, 0,
					BIT(3)|BIT(2)|BIT(1)|BIT(0));
	msm_pcie_write_mask(dev->dm_core + PCIE20_DEVICE_CONTROL2_STATUS2, 0,
					BIT(10));
	PCIE_DBG("RC's CAP_LINKCTRLSTATUS:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_CAP_LINKCTRLSTATUS));
	PCIE_DBG("RC's L1SUB_CONTROL1:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_L1SUB_CONTROL1));
	PCIE_DBG("RC's DEVICE_CONTROL2_STATUS2:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_DEVICE_CONTROL2_STATUS2));

	/* Enable L1SS on EP */
	msm_pcie_write_mask(dev->conf + PCIE20_CAP_LINKCTRLSTATUS, 0,
					BIT(1)|BIT(0));
	msm_pcie_write_mask(dev->conf + PCIE20_L1SUB_CONTROL1 +
					offset, 0,
					BIT(3)|BIT(2)|BIT(1)|BIT(0));
	msm_pcie_write_mask(dev->conf + PCIE20_DEVICE_CONTROL2_STATUS2, 0,
					BIT(10));
	PCIE_DBG("EP's CAP_LINKCTRLSTATUS:0x%x\n",
		readl_relaxed(dev->conf + PCIE20_CAP_LINKCTRLSTATUS));
	PCIE_DBG("EP's L1SUB_CONTROL1:0x%x\n",
		readl_relaxed(dev->conf + PCIE20_L1SUB_CONTROL1 +
					offset));
	PCIE_DBG("EP's DEVICE_CONTROL2_STATUS2:0x%x\n",
		readl_relaxed(dev->conf + PCIE20_DEVICE_CONTROL2_STATUS2));
}

static int msm_pcie_get_resources(struct msm_pcie_dev_t *dev,
					struct platform_device *pdev)
{
	int i, len, cnt, ret = 0;
	struct msm_pcie_vreg_info_t *vreg_info;
	struct msm_pcie_gpio_info_t *gpio_info;
	struct msm_pcie_clk_info_t  *clk_info;
	struct resource *res;
	struct msm_pcie_res_info_t *res_info;
	struct msm_pcie_irq_info_t *irq_info;
	char prop_name[MAX_PROP_SIZE];
	const __be32 *prop;
	u32 *clkfreq = NULL;

	cnt = of_property_count_strings((&pdev->dev)->of_node,
			"clock-names");
	if (cnt > 0) {
		clkfreq = kzalloc(cnt * sizeof(*clkfreq),
					GFP_KERNEL);
		if (!clkfreq) {
			pr_err("%s: memory alloc failed\n",
					__func__);
			return -ENOMEM;
		}
		ret = of_property_read_u32_array(
			(&pdev->dev)->of_node,
			"max-clock-frequency-hz", clkfreq, cnt);
		if (ret) {
			pr_err(
				"%s: invalid max-clock-frequency-hz property, %d\n",
				__func__, ret);
			goto out;
		}
	}

	PCIE_DBG("\n");

	for (i = 0; i < MSM_PCIE_MAX_VREG; i++) {
		vreg_info = &dev->vreg[i];
		vreg_info->hdl =
				devm_regulator_get(&pdev->dev, vreg_info->name);

		if (PTR_ERR(vreg_info->hdl) == -EPROBE_DEFER) {
			PCIE_DBG("EPROBE_DEFER for VReg:%s\n",
				vreg_info->name);
			ret = PTR_ERR(vreg_info->hdl);
			goto out;
		}

		if (IS_ERR(vreg_info->hdl)) {
			if (vreg_info->required) {
				PCIE_DBG("Vreg %s doesn't exist\n",
					vreg_info->name);
				ret = PTR_ERR(vreg_info->hdl);
				goto out;
			} else {
				PCIE_DBG("Optional Vreg %s doesn't exist\n",
					vreg_info->name);
				vreg_info->hdl = NULL;
			}
		} else {
			dev->vreg_n++;
			snprintf(prop_name, MAX_PROP_SIZE,
				"qcom,%s-voltage-level", vreg_info->name);
			prop = of_get_property((&pdev->dev)->of_node,
						prop_name, &len);
			if (!prop || (len != (3 * sizeof(__be32)))) {
				PCIE_DBG("%s %s property\n",
					prop ? "invalid format" :
					"no", prop_name);
			} else {
				vreg_info->max_v = be32_to_cpup(&prop[0]);
				vreg_info->min_v = be32_to_cpup(&prop[1]);
				vreg_info->opt_mode =
					be32_to_cpup(&prop[2]);
			}
		}
	}

	dev->gdsc = devm_regulator_get(&pdev->dev, "gdsc-vdd");

	if (IS_ERR(dev->gdsc)) {
		pr_err("PCIe: Failed to get %s GDSC:%ld\n",
			dev->pdev->name, PTR_ERR(dev->gdsc));
		if (PTR_ERR(dev->gdsc) == -EPROBE_DEFER)
			PCIE_DBG("PCIe: EPROBE_DEFER for %s GDSC\n",
					dev->pdev->name);
		ret = PTR_ERR(dev->gdsc);
		goto out;
	}

	dev->gpio_n = 0;
	for (i = 0; i < MSM_PCIE_MAX_GPIO; i++) {
		gpio_info = &dev->gpio[i];
		ret = of_get_named_gpio((&pdev->dev)->of_node,
					gpio_info->name, 0);
		if (ret >= 0) {
			gpio_info->num = ret;
			ret = 0;
			dev->gpio_n++;
			PCIE_DBG("GPIO num for %s is %d\n", gpio_info->name,
							gpio_info->num);
		} else {
			goto out;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		clk_info = &dev->clk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				PCIE_DBG("Clock %s isn't available:%ld\n",
				clk_info->name, PTR_ERR(clk_info->hdl));
				ret = PTR_ERR(clk_info->hdl);
				goto out;
			} else {
				PCIE_DBG("Ignoring Clock %s\n", clk_info->name);
				clk_info->hdl = NULL;
			}
		} else {
			if (clkfreq != NULL) {
				clk_info->freq = clkfreq[i +
					MSM_PCIE_MAX_PIPE_CLK];
				PCIE_DBG("Freq of Clock %s is:%d\n",
					clk_info->name, clk_info->freq);
			}
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		clk_info = &dev->pipeclk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				PCIE_DBG("Clock %s isn't available:%ld\n",
				clk_info->name, PTR_ERR(clk_info->hdl));
				ret = PTR_ERR(clk_info->hdl);
				goto out;
			} else {
				PCIE_DBG("Ignoring Clock %s\n", clk_info->name);
				clk_info->hdl = NULL;
			}
		} else {
			if (clkfreq != NULL) {
				clk_info->freq = clkfreq[i];
				PCIE_DBG("Freq of Clock %s is:%d\n",
					clk_info->name, clk_info->freq);
			}
		}
	}


	dev->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (!dev->bus_scale_table) {
		PCIE_DBG("PCIe: No bus scale table for RC %d (%s)\n",
			dev->rc_idx, dev->pdev->name);
		dev->bus_client = 0;
	} else {
		dev->bus_client =
			msm_bus_scale_register_client(dev->bus_scale_table);
		if (!dev->bus_client) {
			pr_err(
				"PCIe: Failed to register bus client for RC %d (%s)\n",
				dev->rc_idx, dev->pdev->name);
			msm_bus_cl_clear_pdata(dev->bus_scale_table);
			ret = -ENODEV;
			goto out;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_RES; i++) {
		res_info = &dev->res[i];

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							   res_info->name);

		if (!res) {
			pr_err("pcie:can't get %s resource.\n", res_info->name);
			ret = -ENOMEM;
			goto out;
		} else
			PCIE_DBG("start addr for %s is %pa.\n", res_info->name,
					&res->start);

		res_info->base = devm_ioremap(&pdev->dev,
						res->start, resource_size(res));
		if (!res_info->base) {
			pr_err("pcie: can't remap %s.\n", res_info->name);
			ret = -ENOMEM;
			goto out;
		}
		res_info->resource = res;
	}

	for (i = 0; i < MSM_PCIE_MAX_IRQ; i++) {
		irq_info = &dev->irq[i];

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							   irq_info->name);

		if (!res) {
			int j;
			for (j = 0; j < MSM_PCIE_MAX_RES; j++) {
				iounmap(dev->res[j].base);
				dev->res[j].base = NULL;
			}
			pr_err("pcie: can't find IRQ # for %s.\n",
				irq_info->name);
			ret = -ENODEV;
			goto out;
		} else {
			irq_info->num = res->start;
			PCIE_DBG("IRQ # for %s is %d.\n", irq_info->name,
					irq_info->num);
		}
	}

	/* All allocations succeeded */

	dev->wake_n = dev->irq[MSM_PCIE_INT_WAKE].num;

	dev->parf = dev->res[MSM_PCIE_RES_PARF].base;
	dev->phy = dev->res[MSM_PCIE_RES_PHY].base;
	dev->elbi = dev->res[MSM_PCIE_RES_ELBI].base;
	dev->dm_core = dev->res[MSM_PCIE_RES_DM_CORE].base;
	dev->conf = dev->res[MSM_PCIE_RES_CONF].base;
	dev->bars = dev->res[MSM_PCIE_RES_BARS].base;
	dev->dev_mem_res = dev->res[MSM_PCIE_RES_BARS].resource;
	dev->dev_io_res = dev->res[MSM_PCIE_RES_IO].resource;
	dev->dev_io_res->flags = IORESOURCE_IO;

out:
	kfree(clkfreq);
	return ret;
}

static void msm_pcie_release_resources(struct msm_pcie_dev_t *dev)
{
	dev->parf = NULL;
	dev->elbi = NULL;
	dev->dm_core = NULL;
	dev->conf = NULL;
	dev->bars = NULL;
	dev->dev_mem_res = NULL;
	dev->dev_io_res = NULL;
}

static int msm_pcie_enable(struct msm_pcie_dev_t *dev, u32 options)
{
	int ret;
	uint32_t val;
	long int retries = 0;
	int link_check_count = 0;

	PCIE_DBG("\n");

	mutex_lock(&setup_lock);

	/* assert PCIe reset link to keep EP in reset */

	pr_info("PCIe: Assert the reset of endpoint\n");
	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);
	usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
				 PERST_PROPAGATION_DELAY_US_MAX);

	/* enable power */

	if (options & PM_VREG) {
		ret = msm_pcie_vreg_init(dev);
		if (ret)
			goto out;
	}

	/* enable clocks */
	if (options & PM_CLK) {
		ret = msm_pcie_clk_init(dev);
		wmb();
		if (ret)
			goto clk_fail;
	}

	/* enable PCIe clocks and resets */
	msm_pcie_write_mask(dev->parf + PCIE20_PARF_PHY_CTRL, BIT(0), 0);

	/* change DBI base address */
	writel_relaxed(0, dev->parf + PCIE20_PARF_DBI_BASE_ADDR);

	/* init PCIe PHY */
	pcie_phy_init(dev);

	if (options & PM_PIPE_CLK) {
		usleep_range(PHY_STABILIZATION_DELAY_US_MIN,
					 PHY_STABILIZATION_DELAY_US_MAX);
		/* Enable the pipe clock */
		ret = msm_pcie_pipe_clk_init(dev);
		wmb();
		if (ret)
			goto link_fail;
	}

	PCIE_DBG("waiting for phy ready...\n");

	do {
		if (pcie_phy_is_ready(dev))
			break;
		retries++;
		usleep_range(REFCLK_STABILIZATION_DELAY_US_MIN,
					 REFCLK_STABILIZATION_DELAY_US_MAX);
	} while (retries < PHY_READY_TIMEOUT_COUNT);

	PCIE_DBG("number of PHY retries: %ld\n", retries);

	if (pcie_phy_is_ready(dev))
		pr_info("PCIe RC %d PHY is ready!\n", dev->rc_idx);
	else {
		pr_err("PCIe PHY RC %d failed to come up!\n", dev->rc_idx);
		ret = -ENODEV;
		goto link_fail;
	}

	if (dev->ep_latency)
		msleep(dev->ep_latency);

	/* de-assert PCIe reset link to bring EP out of reset */

	pr_info("PCIe: Release the reset of endpoint\n");
	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				1 - dev->gpio[MSM_PCIE_GPIO_PERST].on);
	usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
				 PERST_PROPAGATION_DELAY_US_MAX);

	/* enable link training */
	msm_pcie_write_mask(dev->elbi + PCIE20_ELBI_SYS_CTRL, 0, BIT(0));

	PCIE_DBG("check if link is up\n");

	/* Wait for up to 100ms for the link to come up */
	do {
		usleep_range(LINK_UP_TIMEOUT_US_MIN, LINK_UP_TIMEOUT_US_MAX);
		val =  readl_relaxed(dev->elbi + PCIE20_ELBI_SYS_STTS);
	} while (!(val & XMLH_LINK_UP) &&
		(link_check_count++ < LINK_UP_CHECK_MAX_COUNT));

	if (val & XMLH_LINK_UP)
		PCIE_DBG("Link is up after %d checkings\n", link_check_count);
	else
		PCIE_DBG("Initial link training failed\n");

	retries = 0;

	while (!(val & XMLH_LINK_UP) && (retries < MAX_LINK_RETRIES)) {
		PCIE_DBG("LTSSM_STATE:0x%x\n", (val >> 0xC) & 0x1f);
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);
		usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
					 PERST_PROPAGATION_DELAY_US_MAX);
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				1 - dev->gpio[MSM_PCIE_GPIO_PERST].on);
		usleep_range(LINK_RETRY_TIMEOUT_US_MIN,
				LINK_RETRY_TIMEOUT_US_MAX);
		retries++;
		val =  readl_relaxed(dev->elbi + PCIE20_ELBI_SYS_STTS);
	}

	PCIE_DBG("number of link training retries: %ld\n", retries);

	if (val & XMLH_LINK_UP) {
		pr_info("PCIe RC %d link initialized\n", dev->rc_idx);
	} else {
		pr_info("PCIe: Assert the reset of endpoint\n");
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
			dev->gpio[MSM_PCIE_GPIO_PERST].on);
		pr_err("PCIe RC %d link initialization failed\n", dev->rc_idx);
		ret = -1;
		goto link_fail;
	}

	msm_pcie_config_controller(dev);

	if (!dev->msi_gicm_addr)
		msm_pcie_config_msi_controller(dev);

	if (dev->l1ss_supported)
		msm_pcie_config_l1ss(dev);

	dev->link_status = MSM_PCIE_LINK_ENABLED;
	goto out;

link_fail:
	msm_pcie_clk_deinit(dev);
clk_fail:
	msm_pcie_vreg_deinit(dev);
	msm_pcie_pipe_clk_deinit(dev);
out:
	mutex_unlock(&setup_lock);

	return ret;
}


void msm_pcie_disable(struct msm_pcie_dev_t *dev, u32 options)
{
	PCIE_DBG("RC %d\n", dev->rc_idx);

	pr_info("PCIe: Assert the reset of endpoint\n");
	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);

	if (options & PM_CLK) {
		msm_pcie_write_mask(dev->parf + PCIE20_PARF_PHY_CTRL, 0,
					BIT(0));
		msm_pcie_clk_deinit(dev);
	}

	if (options & PM_VREG)
		msm_pcie_vreg_deinit(dev);

	if (options & PM_PIPE_CLK)
		msm_pcie_pipe_clk_deinit(dev);

	dev->link_status = MSM_PCIE_LINK_DISABLED;
}

static int msm_pcie_setup(int nr, struct pci_sys_data *sys)
{
	PCIE_DBG("bus %d\n", nr);

	/*
	 * specify linux PCI framework to allocate device memory (BARs)
	 * from msm_pcie_dev.dev_mem_res resource.
	 */
	sys->mem_offset = 0;
	sys->io_offset = 0;

	pci_add_resource(&sys->resources,
		((struct msm_pcie_dev_t *)(sys->private_data))->dev_io_res);
	pci_add_resource(&sys->resources,
		((struct msm_pcie_dev_t *)(sys->private_data))->dev_mem_res);
	return 1;
}

static struct pci_bus *msm_pcie_scan_bus(int nr,
						struct pci_sys_data *sys)
{
	struct pci_bus *bus = NULL;

	PCIE_DBG("bus %d\n", nr);

	bus = pci_scan_root_bus(NULL, sys->busnr, &msm_pcie_ops, sys,
					&sys->resources);

	return bus;
}

static int msm_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);
	int ret = 0;

	PCIE_DBG("rc %s slot %d pin %d\n", pcie_dev->pdev->name, slot, pin);

	switch (pin) {
	case 1:
		ret = pcie_dev->irq[MSM_PCIE_INT_A].num;
		break;
	case 2:
		ret = pcie_dev->irq[MSM_PCIE_INT_B].num;
		break;
	case 3:
		ret = pcie_dev->irq[MSM_PCIE_INT_C].num;
		break;
	case 4:
		ret = pcie_dev->irq[MSM_PCIE_INT_D].num;
		break;
	default:
		pr_err("PCIe: unsupported pin number.\n");
	}

	return ret;
}


static struct hw_pci msm_pci[MAX_RC_NUM] = {
	{
	.domain = 0,
	.nr_controllers	= 1,
	.swizzle	= pci_common_swizzle,
	.setup		= msm_pcie_setup,
	.scan		= msm_pcie_scan_bus,
	.map_irq	= msm_pcie_map_irq,
	},
	{
	.domain = 1,
	.nr_controllers	= 1,
	.swizzle	= pci_common_swizzle,
	.setup		= msm_pcie_setup,
	.scan		= msm_pcie_scan_bus,
	.map_irq	= msm_pcie_map_irq,
	},
};

int msm_pcie_enumerate(u32 rc_idx)
{
	int ret = 0;
	struct msm_pcie_dev_t *dev = &msm_pcie_dev[rc_idx];

	PCIE_DBG("Enumerate RC %d\n", rc_idx);

	if (!dev->enumerated) {
		ret = msm_pcie_enable(dev, PM_ALL);

		/* kick start ARM PCI configuration framework */
		if (!ret) {
			struct pci_dev *pcidev = NULL;
			bool found = false;
			u32 ids = readl_relaxed(msm_pcie_dev[rc_idx].dm_core);
			u32 vendor_id = ids & 0xffff;
			u32 device_id = (ids & 0xffff0000) >> 16;

			PCIE_DBG("vendor-id:0x%x device_id:0x%x\n",
					vendor_id, device_id);

			msm_pci[rc_idx].private_data = (void **)&dev;
			pci_common_init(&msm_pci[rc_idx]);
			/* This has to happen only once */
			dev->enumerated = true;

			do {
				pcidev = pci_get_device(vendor_id,
					device_id, pcidev);
				if (pcidev && (&msm_pcie_dev[rc_idx] ==
					(struct msm_pcie_dev_t *)
					PCIE_BUS_PRIV_DATA(pcidev))) {
					msm_pcie_dev[rc_idx].dev = pcidev;
					found = true;
					PCIE_DBG(
						"PCI device is found for RC %d\n",
						rc_idx);
				}
			} while (!found && pcidev);

			if (!pcidev) {
				pr_err(
					"PCIe: %s: Did not find PCI device for RC %d.\n",
					__func__, dev->rc_idx);
				return -ENODEV;
			}
		} else {
			pr_err("PCIe: %s: failed to enable RC %d.\n",
				__func__, dev->rc_idx);
		}
	} else {
		pr_err("PCIe: %s: RC %d has already been enumerated.\n",
			__func__, dev->rc_idx);
	}

	return ret;
}

static int msm_pcie_probe(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx;

	PCIE_DBG("\n");

	mutex_lock(&pcie_drv.drv_lock);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ctrl-amt", &pcie_drv.rc_expected);
	if (ret) {
		pr_err("PCIe: does not find controller amount.\n");
		goto out;
	} else {
		if (pcie_drv.rc_expected > MAX_RC_NUM) {
			PCIE_DBG("Expected number of devices %d\n",
							pcie_drv.rc_expected);
			PCIE_DBG("Exceeded max supported devices %d\n",
							MAX_RC_NUM);
			goto out;
		}
		PCIE_DBG("Target has %d RC(s).\n", pcie_drv.rc_expected);
	}

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &rc_idx);
	if (ret) {
		PCIE_DBG("Did not find RC index.\n");
		goto out;
	} else {
		if (rc_idx >= MAX_RC_NUM) {
			PCIE_DBG("Invalid RC Index %d (max supported = %d)\n",
							rc_idx, MAX_RC_NUM);
			goto out;
		}
		pcie_drv.rc_num++;
		PCIE_DBG("RC index is %d.", rc_idx);
	}

	msm_pcie_dev[rc_idx].l1ss_supported =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,l1ss-supported");
	PCIE_DBG("L1ss is %s supported.\n",
		msm_pcie_dev[rc_idx].l1ss_supported ? "" : "not");
	msm_pcie_dev[rc_idx].aux_clk_sync =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,aux-clk-sync");
	PCIE_DBG("AUX clock is %s synchronous to Core clock.\n",
		msm_pcie_dev[rc_idx].aux_clk_sync ? "" : "not");

	msm_pcie_dev[rc_idx].n_fts = 0;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,n-fts",
				&msm_pcie_dev[rc_idx].n_fts);

	if (ret)
		PCIE_DBG("n-fts does not exist.\n");
	else
		PCIE_DBG("n-fts: 0x%x.\n",
				msm_pcie_dev[rc_idx].n_fts);

	msm_pcie_dev[rc_idx].ext_ref_clk =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,ext-ref-clk");
	PCIE_DBG("ref clk is %s.\n",
		msm_pcie_dev[rc_idx].ext_ref_clk ? "external" : "internal");

	msm_pcie_dev[rc_idx].ep_latency = 0;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ep-latency",
				&msm_pcie_dev[rc_idx].ep_latency);
	if (ret)
		PCIE_DBG("ep-latency does not exist.\n");
	else
		PCIE_DBG("ep-latency: 0x%x.\n",
				msm_pcie_dev[rc_idx].ep_latency);

	msm_pcie_dev[rc_idx].msi_gicm_addr = 0;
	msm_pcie_dev[rc_idx].msi_gicm_base = 0;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,msi-gicm-addr",
				&msm_pcie_dev[rc_idx].msi_gicm_addr);

	if (ret) {
		PCIE_DBG("msi-gicm-addr does not exist.\n");
	} else {
		PCIE_DBG("msi-gicm-addr: 0x%x.\n",
				msm_pcie_dev[rc_idx].msi_gicm_addr);

		ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,msi-gicm-base",
				&msm_pcie_dev[rc_idx].msi_gicm_base);

		if (ret) {
			pr_err("msi-gicm-base does not exist.\n");
			goto decrease_rc_num;
		} else {
			PCIE_DBG("msi-gicm-base: 0x%x.\n",
					msm_pcie_dev[rc_idx].msi_gicm_base);
		}
	}

	msm_pcie_dev[rc_idx].rc_idx = rc_idx;
	msm_pcie_dev[rc_idx].pdev = pdev;
	msm_pcie_dev[rc_idx].vreg_n = 0;
	msm_pcie_dev[rc_idx].gpio_n = 0;
	msm_pcie_dev[rc_idx].parf_deemph = 0;
	msm_pcie_dev[rc_idx].parf_swing = 0;
	msm_pcie_dev[rc_idx].link_status = MSM_PCIE_LINK_DEINIT;
	msm_pcie_dev[rc_idx].user_suspend = false;
	msm_pcie_dev[rc_idx].saved_state = NULL;
	msm_pcie_dev[rc_idx].enumerated = false;
	memcpy(msm_pcie_dev[rc_idx].vreg, msm_pcie_vreg_info,
				sizeof(msm_pcie_vreg_info));
	memcpy(msm_pcie_dev[rc_idx].gpio, msm_pcie_gpio_info,
				sizeof(msm_pcie_gpio_info));
	memcpy(msm_pcie_dev[rc_idx].clk, msm_pcie_clk_info[rc_idx],
				sizeof(msm_pcie_clk_info));
	memcpy(msm_pcie_dev[rc_idx].pipeclk, msm_pcie_pipe_clk_info[rc_idx],
				sizeof(msm_pcie_pipe_clk_info));
	memcpy(msm_pcie_dev[rc_idx].res, msm_pcie_res_info,
				sizeof(msm_pcie_res_info));
	memcpy(msm_pcie_dev[rc_idx].irq, msm_pcie_irq_info,
				sizeof(msm_pcie_irq_info));

	ret = msm_pcie_get_resources(&msm_pcie_dev[rc_idx],
				msm_pcie_dev[rc_idx].pdev);

	if (ret)
		goto decrease_rc_num;

	ret = msm_pcie_gpio_init(&msm_pcie_dev[rc_idx]);
	if (ret) {
		msm_pcie_release_resources(&msm_pcie_dev[rc_idx]);
		goto decrease_rc_num;
	}

	ret = msm_pcie_irq_init(&msm_pcie_dev[rc_idx]);
	if (ret) {
		msm_pcie_release_resources(&msm_pcie_dev[rc_idx]);
		msm_pcie_gpio_deinit(&msm_pcie_dev[rc_idx]);
		goto decrease_rc_num;
	}

	ret = msm_pcie_enumerate(rc_idx);

	if (ret)
		pr_err(
			"PCIe:failed to enable RC %d in bootup; it will be enumerated upon WAKE signal.\n",
			rc_idx);
	else
		PCIE_DBG("RC %d is enabled in bootup\n", rc_idx);

	PCIE_DBG("PCIE probed %s\n", dev_name(&(pdev->dev)));
	mutex_unlock(&pcie_drv.drv_lock);
	return 0;

decrease_rc_num:
	pcie_drv.rc_num--;
out:
	pr_err("Driver Failed ret=%d\n", ret);
	mutex_unlock(&pcie_drv.drv_lock);

	return ret;
}

static int __exit msm_pcie_remove(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx;

	mutex_lock(&pcie_drv.drv_lock);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &rc_idx);
	if (ret) {
		PCIE_DBG("Did not find RC index.\n");
		goto out;
	} else {
		pcie_drv.rc_num--;
		PCIE_DBG("RC index is 0x%x.", rc_idx);
	}

	msm_pcie_irq_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_vreg_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_clk_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_gpio_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_release_resources(&msm_pcie_dev[rc_idx]);

out:
	mutex_unlock(&pcie_drv.drv_lock);

	return ret;
}

static struct of_device_id msm_pcie_match[] = {
	{	.compatible = "qcom,msm_pcie",
	},
	{}
};

static struct platform_driver msm_pcie_driver = {
	.probe	= msm_pcie_probe,
	.remove	= msm_pcie_remove,
	.driver	= {
		.name		= "msm_pcie",
		.owner		= THIS_MODULE,
		.of_match_table	= msm_pcie_match,
	},
};

static int __init pcie_init(void)
{
	int ret = 0, i;

	PCIE_DBG("pcie:%s.\n", __func__);

	pcie_drv.rc_num = 0;
	pcie_drv.rc_expected = 0;
	mutex_init(&pcie_drv.drv_lock);
	mutex_init(&setup_lock);

	for (i = 0; i < MAX_RC_NUM; i++) {
		spin_lock_init(&msm_pcie_dev[i].cfg_lock);
		msm_pcie_dev[i].cfg_access = true;
	}

	ret = platform_driver_register(&msm_pcie_driver);

	return ret;
}

static void __exit pcie_exit(void)
{
	PCIE_DBG("pcie:%s.\n", __func__);

	platform_driver_unregister(&msm_pcie_driver);
}

subsys_initcall_sync(pcie_init);
module_exit(pcie_exit);


/* RC do not represent the right class; set it to PCI_CLASS_BRIDGE_PCI */
static void msm_pcie_fixup_early(struct pci_dev *dev)
{
	PCIE_DBG("hdr_type %d\n", dev->hdr_type);
	if (dev->hdr_type == 1)
		dev->class = (dev->class & 0xff) | (PCI_CLASS_BRIDGE_PCI << 8);
}
DECLARE_PCI_FIXUP_EARLY(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
			msm_pcie_fixup_early);

/* Suspend the PCIe link */
static int msm_pcie_pm_suspend(struct pci_dev *dev,
			void *user, void *data, u32 options)
{
	int ret = 0;
	u32 val = 0;
	int ret_l23;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG("RC %d\n", pcie_dev->rc_idx);

	if (dev) {
		ret = pci_save_state(dev);
		pcie_dev->saved_state =	pci_store_saved_state(dev);
	}
	if (ret) {
		pr_err("PCIe: fail to save state of RC 0x%p:%d.\n",
				dev, ret);
		return ret;
	}

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	pcie_dev->cfg_access = false;
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	msm_pcie_write_mask(pcie_dev->elbi + PCIE20_ELBI_SYS_CTRL, 0,
				BIT(4));

	PCIE_DBG("PME_TURNOFF_MSG is sent out\n");

	ret_l23 = readl_poll_timeout((pcie_dev->parf
		+ PCIE20_PARF_PM_STTS), val, (val & BIT(6)), 10000, 100000);

	/* check L23_Ready */
	if (!ret_l23)
		PCIE_DBG("PM_Enter_L23 is received\n");
	else
		PCIE_DBG("PM_Enter_L23 is NOT received\n");

	msm_pcie_disable(pcie_dev, PM_PIPE_CLK | PM_CLK | PM_VREG);

	return ret;
}

static void msm_pcie_fixup_suspend(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG("RC %d\n", pcie_dev->rc_idx);

	if (pcie_dev->link_status != MSM_PCIE_LINK_ENABLED)
		return;

	ret = msm_pcie_pm_suspend(dev, NULL, NULL, 0);
	if (ret)
		pr_err("PCIe: got failure in suspend:%d.\n", ret);
}
DECLARE_PCI_FIXUP_SUSPEND(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
			  msm_pcie_fixup_suspend);

/* Resume the PCIe link */
static int msm_pcie_pm_resume(struct pci_dev *dev,
			void *user, void *data, u32 options)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG("RC %d\n", pcie_dev->rc_idx);

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	pcie_dev->cfg_access = true;
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	ret = msm_pcie_enable(pcie_dev, PM_PIPE_CLK | PM_CLK | PM_VREG);
	if (ret) {
		pr_err("PCIe:fail to enable PCIe link in resume\n");
		return ret;
	} else {
		PCIE_DBG("dev->bus->number = %d dev->bus->primary = %d\n",
			 dev->bus->number, dev->bus->primary);

		pci_load_and_free_saved_state(dev, &pcie_dev->saved_state);

		pci_restore_state(dev);
	}

	return ret;
}

void msm_pcie_fixup_resume(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG("RC %d\n", pcie_dev->rc_idx);

	if ((pcie_dev->link_status != MSM_PCIE_LINK_DISABLED) ||
		pcie_dev->user_suspend)
		return;

	ret = msm_pcie_pm_resume(dev, NULL, NULL, 0);
	if (ret)
		pr_err("PCIe: got failure in fixup resume:%d.\n", ret);
}
DECLARE_PCI_FIXUP_RESUME(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
				 msm_pcie_fixup_resume);

void msm_pcie_fixup_resume_early(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG("RC %d\n", pcie_dev->rc_idx);

	if ((pcie_dev->link_status != MSM_PCIE_LINK_DISABLED) ||
		pcie_dev->user_suspend)
		return;

	ret = msm_pcie_pm_resume(dev, NULL, NULL, 0);
	if (ret)
		pr_err("PCIe: got failure in resume:%d.\n", ret);
}
DECLARE_PCI_FIXUP_RESUME_EARLY(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
				 msm_pcie_fixup_resume_early);

static void msm_pcie_fixup_final(struct pci_dev *dev)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);
	PCIE_DBG("RC %d\n", pcie_dev->rc_idx);
	pcie_drv.current_rc++;
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, msm_pcie_fixup_final);

int msm_pcie_pm_control(enum msm_pcie_pm_opt pm_opt, u32 busnr, void *user,
			void *data, u32 options)
{
	int ret = 0;
	struct pci_dev *dev;
	u32 rc_idx = 0;

	PCIE_DBG("pm_opt:%d;busnr:%d;options:%d\n", pm_opt, busnr, options);

	switch (busnr) {
	case 1:
		if (user) {
			struct msm_pcie_dev_t *pcie_dev
				= PCIE_BUS_PRIV_DATA(((struct pci_dev *)user));

			if (pcie_dev) {
				rc_idx = pcie_dev->rc_idx;
				PCIE_DBG("RC %d\n", pcie_dev->rc_idx);
			} else {
				pr_err(
					"PCIe: did not find RC for pci endpoint device 0x%x.\n",
					(u32)user);
				ret = -ENODEV;
				goto out;
			}
		}
		break;
	default:
		pr_err("PCIe: unsupported bus number.\n");
		ret = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	dev = msm_pcie_dev[rc_idx].dev;

	switch (pm_opt) {
	case MSM_PCIE_SUSPEND:
		if (msm_pcie_dev[rc_idx].link_status != MSM_PCIE_LINK_ENABLED) {
			pr_err(
				"PCIe: requested to suspend when link is not enabled:%d.\n",
				msm_pcie_dev[rc_idx].link_status);
			break;
		}
		msm_pcie_dev[rc_idx].user_suspend = true;
		ret = msm_pcie_pm_suspend(dev, user, data, options);
		if (ret) {
			pr_err("PCIe: user failed to suspend the link.\n");
			msm_pcie_dev[rc_idx].user_suspend = false;
		}
		break;
	case MSM_PCIE_RESUME:
		if (msm_pcie_dev[rc_idx].link_status !=
					MSM_PCIE_LINK_DISABLED) {
			pr_err(
				"PCIe: requested to resume when link is not disabled:%d.\n",
				msm_pcie_dev[rc_idx].link_status);
			break;
		}
		ret = msm_pcie_pm_resume(dev, user, data, options);
		if (ret)
			pr_err("PCIe: user failed to resume the link.\n");
		else
			msm_pcie_dev[rc_idx].user_suspend = false;
		break;
	default:
		pr_err("PCIe: unsupported pm operation:%d.\n", pm_opt);
		ret = -ENODEV;
		goto out;
	}

out:
	return ret;
}
EXPORT_SYMBOL(msm_pcie_pm_control);
