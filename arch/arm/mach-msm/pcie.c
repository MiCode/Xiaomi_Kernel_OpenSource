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
#define PCIE20_PARF_AXI_MSTR_WR_ADDR_HALT   0x178
#define PCIE20_PARF_Q2A_FLUSH          0x1AC

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
#define MAX_RC_NAME_LEN 15

/* Config Space Offsets */
#define BDF_OFFSET(bus, device, function) \
	((bus << 24) | (device << 15) | (function << 8))

/* debug mask sys interface */
static int msm_pcie_debug_mask;
module_param_named(debug_mask, msm_pcie_debug_mask,
			    int, S_IRUGO | S_IWUSR | S_IWGRP);

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

bool msm_pcie_confirm_linkup(struct msm_pcie_dev_t *dev,
						bool check_sw_stts,
						bool check_ep)
{
	u32 val;

	if (check_sw_stts && (dev->link_status != MSM_PCIE_LINK_ENABLED)) {
		PCIE_DBG(dev, "PCIe: The link of RC %d is not enabled.\n",
			dev->rc_idx);
		return false;
	}

	if (!(readl_relaxed(dev->dm_core + 0x80) & BIT(29))) {
		PCIE_DBG(dev, "PCIe: The link of RC %d is not up.\n",
			dev->rc_idx);
		return false;
	}

	val = readl_relaxed(dev->dm_core);
	PCIE_DBG(dev, "PCIe: device ID and vender ID of RC %d are 0x%x.\n",
		dev->rc_idx, val);
	if (val == PCIE_LINK_DOWN) {
		PCIE_ERR(dev,
			"PCIe: The link of RC %d is not really up; device ID and vender ID of RC %d are 0x%x.\n",
			dev->rc_idx, dev->rc_idx, val);
		return false;
	}

	if (check_ep) {
		val = readl_relaxed(dev->conf);
		PCIE_DBG(dev,
			"PCIe: device ID and vender ID of EP of RC %d are 0x%x.\n",
			dev->rc_idx, val);
		if (val == PCIE_LINK_DOWN) {
			PCIE_ERR(dev,
				"PCIe: The link of RC %d is not really up; device ID and vender ID of EP of RC %d are 0x%x.\n",
				dev->rc_idx, dev->rc_idx, val);
			return false;
		}
	}

	return true;
}

void msm_pcie_cfg_recover(struct msm_pcie_dev_t *dev, bool rc)
{
	int i;
	u32 val = 0;
	u32 *shadow;
	void *cfg;

	if (rc) {
		shadow = dev->rc_shadow;
		cfg = dev->dm_core;
	} else {
		shadow = dev->ep_shadow;
		cfg = dev->conf;
	}

	for (i = PCIE_CONF_SPACE_DW - 1; i >= 0; i--) {
		val = readl_relaxed(shadow + i);
		if (val != PCIE_CLEAR) {
			PCIE_DBG(dev, "PCIe: before recovery:cfg 0x%x:0x%x\n",
				i * 4, readl_relaxed(cfg + i * 4));
			PCIE_DBG(dev, "PCIe: shadow_dw[%d]:cfg 0x%x:0x%x\n",
				i, i * 4, val);
			writel_relaxed(val, cfg + i * 4);
			wmb();
			PCIE_DBG(dev, "PCIe: after recovery:cfg 0x%x:0x%x\n\n",
				i * 4, readl_relaxed(cfg + i * 4));
		}
	}
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
		PCIE_ERR(dev, "PCIe: unsupported bus number:%d\n", bus->number);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	if ((bus->number > MAX_BUS_NUM) || (devfn != 0)) {
		PCIE_DBG(dev, "RC%d invalid %s - bus %d devfn %d\n", rc_idx,
			 (oper == RD) ? "rd" : "wr", bus->number, devfn);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	spin_lock_irqsave(&dev->cfg_lock, dev->irqsave_flags);

	if (!dev->cfg_access) {
		PCIE_DBG(dev, "Access denied for RC%d %d:0x%02x + 0x%04x[%d]\n",
			rc_idx, bus->number, devfn, where, size);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	if (dev->link_status != MSM_PCIE_LINK_ENABLED) {
		PCIE_DBG(dev,
			"Access to RC%d %d:0x%02x + 0x%04x[%d] is denied because link is down\n",
			rc_idx, bus->number, devfn, where, size);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	/* check if the link is up for endpoint */
	if (!rc && !msm_pcie_is_link_up(dev)) {
		PCIE_ERR(dev,
			"PCIe: RC%d %s fail, link down - bus %d devfn %d\n",
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
		PCIE_DBG(dev,
			"RC%d %d:0x%02x + 0x%04x[%d] -> 0x%08x; rd 0x%08x\n",
			rc_idx, bus->number, devfn, where, size, *val, rd_val);
	} else {
		wr_val = (rd_val & ~mask) |
				((*val << (8 * byte_offset)) & mask);
		writel_relaxed(wr_val, config_base + word_offset);
		wmb(); /* ensure config data is written to hardware register */

		if (rd_val == PCIE_LINK_DOWN) {
			PCIE_ERR(dev,
				"Read of RC%d %d:0x%02x + 0x%04x[%d] is all FFs\n",
				rc_idx, bus->number, devfn, where, size);
		} else if (dev->shadow_en) {
			if (rc)
				dev->rc_shadow[word_offset / 4] = wr_val;
			else
				dev->ep_shadow[word_offset / 4] = wr_val;
		}

		PCIE_DBG(dev,
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
		pr_debug("change class for RC:0x%x\n", *val);
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

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < dev->gpio_n; i++) {
		info = &dev->gpio[i];

		if (!info->num)
			continue;

		rc = gpio_request(info->num, info->name);
		if (rc) {
			PCIE_ERR(dev, "PCIe: RC%d can't get gpio %s; %d\n",
				dev->rc_idx, info->name, rc);
			break;
		}

		if (info->out)
			rc = gpio_direction_output(info->num, info->init);
		else
			rc = gpio_direction_input(info->num);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: RC%d can't set direction for GPIO %s:%d\n",
				dev->rc_idx, info->name, rc);
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

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < dev->gpio_n; i++)
		gpio_free(dev->gpio[i].num);
}

int msm_pcie_vreg_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct regulator *vreg;
	struct msm_pcie_vreg_info_t *info;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_VREG; i++) {
		info = &dev->vreg[i];
		vreg = info->hdl;

		if (!vreg)
			continue;

		PCIE_DBG(dev, "RC%d Vreg %s is being enabled\n",
			dev->rc_idx, info->name);
		if (info->max_v) {
			rc = regulator_set_voltage(vreg,
						   info->min_v, info->max_v);
			if (rc) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set voltage for %s: %d\n",
					dev->rc_idx, info->name, rc);
				break;
			}
		}

		if (info->opt_mode) {
			rc = regulator_set_optimum_mode(vreg, info->opt_mode);
			if (rc < 0) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set mode for %s: %d\n",
					dev->rc_idx, info->name, rc);
				break;
			}
		}

		rc = regulator_enable(vreg);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: RC%d can't enable regulator %s: %d\n",
				dev->rc_idx, info->name, rc);
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

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = MSM_PCIE_MAX_VREG - 1; i >= 0; i--) {
		if (dev->vreg[i].hdl) {
			PCIE_DBG(dev, "Vreg %s is being disabled\n",
				dev->vreg[i].name);
			regulator_disable(dev->vreg[i].hdl);
		}
	}
}

static int msm_pcie_clk_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	rc = regulator_enable(dev->gdsc);

	if (rc) {
		PCIE_ERR(dev, "PCIe: fail to enable GDSC for RC%d (%s)\n",
			dev->rc_idx, dev->pdev->name);
		return rc;
	}

	if (dev->bus_client) {
		rc = msm_bus_scale_client_update_request(dev->bus_client, 1);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: fail to set bus bandwidth for RC%d:%d.\n",
				dev->rc_idx, rc);
			return rc;
		} else {
			PCIE_DBG(dev,
				"PCIe: set bus bandwidth for RC%d.\n",
				dev->rc_idx);
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		info = &dev->clk[i];

		if (!info->hdl)
			continue;

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set rate for clk %s: %d.\n",
					dev->rc_idx, info->name, rc);
				break;
			} else {
				PCIE_DBG(dev,
					"PCIe: RC%d set rate for clk %s.\n",
					dev->rc_idx, info->name);
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			PCIE_ERR(dev, "PCIe: RC%d failed to enable clk %s\n",
				dev->rc_idx, info->name);
		else
			PCIE_DBG(dev, "enable clk %s for RC%d.\n",
				info->name, dev->rc_idx);
	}

	if (rc) {
		PCIE_DBG(dev, "RC%d disable clocks for error handling.\n",
			dev->rc_idx);
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

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++)
		if (dev->clk[i].hdl)
			clk_disable_unprepare(dev->clk[i].hdl);

	if (dev->bus_client) {
		rc = msm_bus_scale_client_update_request(dev->bus_client, 0);
		if (rc)
			PCIE_ERR(dev,
				"PCIe: fail to relinquish bus bandwidth for RC%d:%d.\n",
				dev->rc_idx, rc);
		else
			PCIE_DBG(dev,
				"PCIe: relinquish bus bandwidth for RC%d.\n",
				dev->rc_idx);
	}

	regulator_disable(dev->gdsc);
}

static int msm_pcie_pipe_clk_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		info = &dev->pipeclk[i];

		if (!info->hdl)
			continue;

		clk_reset(info->hdl, CLK_RESET_DEASSERT);

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set rate for clk %s: %d.\n",
					dev->rc_idx, info->name, rc);
				break;
			} else {
				PCIE_DBG(dev,
					"PCIe: RC%d set rate for clk %s: %d.\n",
					dev->rc_idx, info->name, rc);
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			PCIE_ERR(dev, "PCIe: RC%d failed to enable clk %s.\n",
				dev->rc_idx, info->name);
		else
			PCIE_DBG(dev, "RC%d enabled pipe clk %s.\n",
				dev->rc_idx, info->name);
	}

	if (rc) {
		PCIE_DBG(dev, "RC%d disable pipe clocks for error handling.\n",
			dev->rc_idx);
		while (i--)
			if (dev->pipeclk[i].hdl)
				clk_disable_unprepare(dev->pipeclk[i].hdl);
	}

	return rc;
}

static void msm_pcie_pipe_clk_deinit(struct msm_pcie_dev_t *dev)
{
	int i;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++)
		if (dev->pipeclk[i].hdl)
			clk_disable_unprepare(
				dev->pipeclk[i].hdl);
}


static void msm_pcie_config_controller(struct msm_pcie_dev_t *dev)
{
	struct resource *axi_conf = dev->res[MSM_PCIE_RES_CONF].resource;
	u32 dev_conf, upper, lower, limit;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

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

	if (dev->shadow_en) {
		writel_relaxed(0, dev->rc_shadow +
				PCIE20_PLR_IATU_VIEWPORT / 4);
		writel_relaxed(4, dev->rc_shadow +
				PCIE20_PLR_IATU_CTRL1 / 4);
		writel_relaxed(lower, dev->rc_shadow +
				PCIE20_PLR_IATU_LBAR / 4);
		writel_relaxed(upper, dev->rc_shadow +
				PCIE20_PLR_IATU_UBAR / 4);
		writel_relaxed(limit, dev->rc_shadow + PCIE20_PLR_IATU_LAR / 4);
		writel_relaxed(dev_conf, dev->rc_shadow +
				PCIE20_PLR_IATU_LTAR / 4);
		writel_relaxed(0, dev->rc_shadow + PCIE20_PLR_IATU_UTAR / 4);
		writel_relaxed(BIT(31), dev->rc_shadow +
				PCIE20_PLR_IATU_CTRL2 / 4);
	}

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
	PCIE_DBG(dev, "PCIE20_PLR_IATU_VIEWPORT:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_VIEWPORT));
	PCIE_DBG(dev, "PCIE20_PLR_IATU_CTRL1:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL1));
	PCIE_DBG(dev, "PCIE20_PLR_IATU_LBAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LBAR));
	PCIE_DBG(dev, "PCIE20_PLR_IATU_UBAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UBAR));
	PCIE_DBG(dev, "PCIE20_PLR_IATU_LAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LAR));
	PCIE_DBG(dev, "PCIE20_PLR_IATU_LTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LTAR));
	PCIE_DBG(dev, "PCIE20_PLR_IATU_UTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UTAR));
	PCIE_DBG(dev, "PCIE20_PLR_IATU_CTRL2:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL2));

	/* configure N_FTS */
	PCIE_DBG(dev, "Original PCIE20_ACK_F_ASPM_CTRL_REG:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG));
	if (!dev->n_fts)
		msm_pcie_write_mask(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG,
					0, BIT(15));
	else
		msm_pcie_write_mask(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG,
					PCIE20_ACK_N_FTS,
					dev->n_fts << 8);

	if (dev->shadow_en) {
		if (!dev->n_fts)
			msm_pcie_write_mask(dev->rc_shadow +
				PCIE20_ACK_F_ASPM_CTRL_REG / 4, 0, BIT(15));
		else
			msm_pcie_write_mask(dev->rc_shadow +
				PCIE20_ACK_F_ASPM_CTRL_REG / 4,
				PCIE20_ACK_N_FTS, dev->n_fts << 8);
	}

	PCIE_DBG(dev, "Updated PCIE20_ACK_F_ASPM_CTRL_REG:0x%x\n",
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
	if (dev->shadow_en) {
		msm_pcie_write_mask(dev->rc_shadow +
			PCIE20_CAP_LINKCTRLSTATUS / 4, 0, BIT(1)|BIT(0));
		msm_pcie_write_mask(dev->rc_shadow + PCIE20_L1SUB_CONTROL1 / 4,
			0, BIT(3)|BIT(2)|BIT(1)|BIT(0));
		msm_pcie_write_mask(dev->rc_shadow +
			PCIE20_DEVICE_CONTROL2_STATUS2 / 4, 0, BIT(10));
	}
	PCIE_DBG(dev, "RC's CAP_LINKCTRLSTATUS:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_CAP_LINKCTRLSTATUS));
	PCIE_DBG(dev, "RC's L1SUB_CONTROL1:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_L1SUB_CONTROL1));
	PCIE_DBG(dev, "RC's DEVICE_CONTROL2_STATUS2:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_DEVICE_CONTROL2_STATUS2));

	/* Enable L1SS on EP */
	msm_pcie_write_mask(dev->conf + PCIE20_CAP_LINKCTRLSTATUS, 0,
					BIT(1)|BIT(0));
	msm_pcie_write_mask(dev->conf + PCIE20_L1SUB_CONTROL1 +
					offset, 0,
					BIT(3)|BIT(2)|BIT(1)|BIT(0));
	msm_pcie_write_mask(dev->conf + PCIE20_DEVICE_CONTROL2_STATUS2, 0,
					BIT(10));
	if (dev->shadow_en) {
		msm_pcie_write_mask(dev->ep_shadow +
			PCIE20_CAP_LINKCTRLSTATUS / 4, 0, BIT(1)|BIT(0));
		msm_pcie_write_mask(dev->ep_shadow + PCIE20_L1SUB_CONTROL1 / 4 +
					PCIE20_EP_L1SUB_CTL1_OFFSET / 4, 0,
					BIT(3)|BIT(2)|BIT(1)|BIT(0));
		msm_pcie_write_mask(dev->ep_shadow +
				PCIE20_DEVICE_CONTROL2_STATUS2 / 4, 0, BIT(10));
	}
	PCIE_DBG(dev, "EP's CAP_LINKCTRLSTATUS:0x%x\n",
		readl_relaxed(dev->conf + PCIE20_CAP_LINKCTRLSTATUS));
	PCIE_DBG(dev, "EP's L1SUB_CONTROL1:0x%x\n",
		readl_relaxed(dev->conf + PCIE20_L1SUB_CONTROL1 +
					offset));
	PCIE_DBG(dev, "EP's DEVICE_CONTROL2_STATUS2:0x%x\n",
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
			PCIE_ERR(dev, "PCIe: memory alloc failed for RC%d\n",
					dev->rc_idx);
			return -ENOMEM;
		}
		ret = of_property_read_u32_array(
			(&pdev->dev)->of_node,
			"max-clock-frequency-hz", clkfreq, cnt);
		if (ret) {
			PCIE_ERR(dev,
				"PCIe: invalid max-clock-frequency-hz property for RC%d:%d\n",
				dev->rc_idx, ret);
			goto out;
		}
	}

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_VREG; i++) {
		vreg_info = &dev->vreg[i];
		vreg_info->hdl =
				devm_regulator_get(&pdev->dev, vreg_info->name);

		if (PTR_ERR(vreg_info->hdl) == -EPROBE_DEFER) {
			PCIE_DBG(dev, "EPROBE_DEFER for VReg:%s\n",
				vreg_info->name);
			ret = PTR_ERR(vreg_info->hdl);
			goto out;
		}

		if (IS_ERR(vreg_info->hdl)) {
			if (vreg_info->required) {
				PCIE_DBG(dev, "Vreg %s doesn't exist\n",
					vreg_info->name);
				ret = PTR_ERR(vreg_info->hdl);
				goto out;
			} else {
				PCIE_DBG(dev,
					"Optional Vreg %s doesn't exist\n",
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
				PCIE_DBG(dev, "%s %s property\n",
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
		PCIE_ERR(dev, "PCIe: RC%d Failed to get %s GDSC:%ld\n",
			dev->rc_idx, dev->pdev->name, PTR_ERR(dev->gdsc));
		if (PTR_ERR(dev->gdsc) == -EPROBE_DEFER)
			PCIE_DBG(dev, "PCIe: EPROBE_DEFER for %s GDSC\n",
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
			PCIE_DBG(dev, "GPIO num for %s is %d\n",
				gpio_info->name, gpio_info->num);
		} else {
			goto out;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		clk_info = &dev->clk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				PCIE_DBG(dev, "Clock %s isn't available:%ld\n",
				clk_info->name, PTR_ERR(clk_info->hdl));
				ret = PTR_ERR(clk_info->hdl);
				goto out;
			} else {
				PCIE_DBG(dev, "Ignoring Clock %s\n",
					clk_info->name);
				clk_info->hdl = NULL;
			}
		} else {
			if (clkfreq != NULL) {
				clk_info->freq = clkfreq[i +
					MSM_PCIE_MAX_PIPE_CLK];
				PCIE_DBG(dev, "Freq of Clock %s is:%d\n",
					clk_info->name, clk_info->freq);
			}
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		clk_info = &dev->pipeclk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				PCIE_DBG(dev, "Clock %s isn't available:%ld\n",
				clk_info->name, PTR_ERR(clk_info->hdl));
				ret = PTR_ERR(clk_info->hdl);
				goto out;
			} else {
				PCIE_DBG(dev, "Ignoring Clock %s\n",
					clk_info->name);
				clk_info->hdl = NULL;
			}
		} else {
			if (clkfreq != NULL) {
				clk_info->freq = clkfreq[i];
				PCIE_DBG(dev, "Freq of Clock %s is:%d\n",
					clk_info->name, clk_info->freq);
			}
		}
	}


	dev->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (!dev->bus_scale_table) {
		PCIE_DBG(dev, "PCIe: No bus scale table for RC%d (%s)\n",
			dev->rc_idx, dev->pdev->name);
		dev->bus_client = 0;
	} else {
		dev->bus_client =
			msm_bus_scale_register_client(dev->bus_scale_table);
		if (!dev->bus_client) {
			PCIE_ERR(dev,
				"PCIe: Failed to register bus client for RC%d (%s)\n",
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
			PCIE_ERR(dev, "PCIe: RC%d can't get %s resource.\n",
				dev->rc_idx, res_info->name);
			ret = -ENOMEM;
			goto out;
		} else
			PCIE_DBG(dev, "start addr for %s is %pa.\n",
				res_info->name,	&res->start);

		res_info->base = devm_ioremap(&pdev->dev,
						res->start, resource_size(res));
		if (!res_info->base) {
			PCIE_ERR(dev, "PCIe: RC%d can't remap %s.\n",
				dev->rc_idx, res_info->name);
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
			PCIE_ERR(dev, "PCIe: RC%d can't find IRQ # for %s.\n",
				dev->rc_idx, irq_info->name);
			ret = -ENODEV;
			goto out;
		} else {
			irq_info->num = res->start;
			PCIE_DBG(dev, "IRQ # for %s is %d.\n", irq_info->name,
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

int msm_pcie_enable(struct msm_pcie_dev_t *dev, u32 options)
{
	int ret = 0;
	uint32_t val;
	long int retries = 0;
	int link_check_count = 0;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	mutex_lock(&dev->setup_lock);

	if (dev->link_status == MSM_PCIE_LINK_ENABLED) {
		PCIE_ERR(dev, "PCIe: the link of RC%d is already enabled\n",
			dev->rc_idx);
		goto out;
	}

	/* assert PCIe reset link to keep EP in reset */

	PCIE_INFO(dev, "PCIe: Assert the reset of endpoint of RC%d.\n",
		dev->rc_idx);
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

	writel_relaxed(0x3656, dev->parf + PCIE20_PARF_SYS_CTRL);

	writel_relaxed(0, dev->parf + PCIE20_PARF_Q2A_FLUSH);

	if (dev->use_msi) {
		PCIE_DBG(dev, "RC%d: enable WR halt.\n", dev->rc_idx);
		msm_pcie_write_mask(dev->parf +
			PCIE20_PARF_AXI_MSTR_WR_ADDR_HALT, 0, BIT(31));
	}

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

	PCIE_DBG(dev, "RC%d: waiting for phy ready...\n", dev->rc_idx);

	do {
		if (pcie_phy_is_ready(dev))
			break;
		retries++;
		usleep_range(REFCLK_STABILIZATION_DELAY_US_MIN,
					 REFCLK_STABILIZATION_DELAY_US_MAX);
	} while (retries < PHY_READY_TIMEOUT_COUNT);

	PCIE_DBG(dev, "RC%d: number of PHY retries:%ld.\n",
		dev->rc_idx, retries);

	if (pcie_phy_is_ready(dev))
		PCIE_INFO(dev, "PCIe RC%d PHY is ready!\n", dev->rc_idx);
	else {
		PCIE_ERR(dev, "PCIe PHY RC%d failed to come up!\n",
			dev->rc_idx);
		ret = -ENODEV;
		goto link_fail;
	}

	if (dev->ep_latency)
		msleep(dev->ep_latency);

	/* de-assert PCIe reset link to bring EP out of reset */

	PCIE_INFO(dev, "PCIe: Release the reset of endpoint of RC%d.\n",
		dev->rc_idx);
	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				1 - dev->gpio[MSM_PCIE_GPIO_PERST].on);
	usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
				 PERST_PROPAGATION_DELAY_US_MAX);

	/* enable link training */
	msm_pcie_write_mask(dev->elbi + PCIE20_ELBI_SYS_CTRL, 0, BIT(0));

	PCIE_DBG(dev, "%s", "check if link is up\n");

	if (dev->rc_idx == 1) {
		PCIE_DBG(dev, "optimized link training for RC%d\n",
			dev->rc_idx);
		/* Wait for up to 100ms for the link to come up */
		do {
			usleep_range(LINK_UP_TIMEOUT_US_MIN,
					LINK_UP_TIMEOUT_US_MAX);
			val =  readl_relaxed(dev->elbi + PCIE20_ELBI_SYS_STTS);
		} while ((!(val & XMLH_LINK_UP) ||
			!msm_pcie_confirm_linkup(dev, false, false))
			&& (link_check_count++ < LINK_UP_CHECK_MAX_COUNT));

		if ((val & XMLH_LINK_UP) &&
			msm_pcie_confirm_linkup(dev, false, false))
			PCIE_DBG(dev, "Link is up after %d checkings\n",
				link_check_count);
		else
			PCIE_DBG(dev, "Initial link training failed for RC%d\n",
				dev->rc_idx);
	} else {
		PCIE_DBG(dev, "non-optimized link training for RC%d\n",
			dev->rc_idx);
		usleep_range(LINK_RETRY_TIMEOUT_US_MIN * 5 ,
			LINK_RETRY_TIMEOUT_US_MAX * 5);
		val =  readl_relaxed(dev->elbi + PCIE20_ELBI_SYS_STTS);
	}

	retries = 0;

	while ((!(val & XMLH_LINK_UP) ||
		!msm_pcie_confirm_linkup(dev, false, false))
		&& (retries < MAX_LINK_RETRIES)) {
		PCIE_ERR(dev, "RC%d:No. %ld:LTSSM_STATE:0x%x\n", dev->rc_idx,
			retries + 1, (val >> 0xC) & 0x1f);
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

	PCIE_DBG(dev, "number of link training retries: %ld\n", retries);

	if ((val & XMLH_LINK_UP) &&
		msm_pcie_confirm_linkup(dev, false, false)) {
		PCIE_INFO(dev, "PCIe RC%d link initialized\n", dev->rc_idx);
	} else {
		PCIE_INFO(dev, "PCIe: Assert the reset of endpoint of RC%d.\n",
			dev->rc_idx);
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
			dev->gpio[MSM_PCIE_GPIO_PERST].on);
		PCIE_ERR(dev, "PCIe RC%d link initialization failed\n",
			dev->rc_idx);
		ret = -1;
		goto link_fail;
	}

	msm_pcie_config_controller(dev);

	if (!dev->msi_gicm_addr)
		msm_pcie_config_msi_controller(dev);

	if (dev->l1ss_supported)
		msm_pcie_config_l1ss(dev);

	dev->link_status = MSM_PCIE_LINK_ENABLED;
	dev->power_on = true;
	dev->suspending = false;
	goto out;

link_fail:
	msm_pcie_clk_deinit(dev);
clk_fail:
	msm_pcie_vreg_deinit(dev);
	msm_pcie_pipe_clk_deinit(dev);
out:
	mutex_unlock(&dev->setup_lock);

	return ret;
}


void msm_pcie_disable(struct msm_pcie_dev_t *dev, u32 options)
{
	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	mutex_lock(&dev->setup_lock);

	if (!dev->power_on) {
		PCIE_DBG(dev,
			"PCIe: the link of RC%d is already power down.\n",
			dev->rc_idx);
		mutex_unlock(&dev->setup_lock);
		return;
	}

	dev->link_status = MSM_PCIE_LINK_DISABLED;
	dev->power_on = false;

	PCIE_INFO(dev, "PCIe: Assert the reset of endpoint of RC%d.\n",
		dev->rc_idx);

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

	mutex_unlock(&dev->setup_lock);
}

static int msm_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct msm_pcie_dev_t *dev =
			(struct msm_pcie_dev_t *)(sys->private_data);

	PCIE_DBG(dev, "bus %d\n", nr);
	/*
	 * specify linux PCI framework to allocate device memory (BARs)
	 * from msm_pcie_dev.dev_mem_res resource.
	 */
	sys->mem_offset = 0;
	sys->io_offset = 0;

	pci_add_resource(&sys->resources, dev->dev_io_res);
	pci_add_resource(&sys->resources, dev->dev_mem_res);
	return 1;
}

static struct pci_bus *msm_pcie_scan_bus(int nr,
						struct pci_sys_data *sys)
{
	struct pci_bus *bus = NULL;
	struct msm_pcie_dev_t *dev =
			(struct msm_pcie_dev_t *)(sys->private_data);

	PCIE_DBG(dev, "bus %d\n", nr);

	bus = pci_scan_root_bus(NULL, sys->busnr, &msm_pcie_ops, sys,
					&sys->resources);

	return bus;
}

static int msm_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);
	int ret = 0;

	PCIE_DBG(pcie_dev, "rc %s slot %d pin %d\n", pcie_dev->pdev->name,
		slot, pin);

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
		PCIE_ERR(pcie_dev, "PCIe: RC%d: unsupported pin number.\n",
			pcie_dev->rc_idx);
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

	PCIE_DBG(dev, "Enumerate RC%d\n", rc_idx);

	if (!dev->enumerated) {
		ret = msm_pcie_enable(dev, PM_ALL);

		/* kick start ARM PCI configuration framework */
		if (!ret) {
			struct pci_dev *pcidev = NULL;
			bool found = false;
			u32 ids = readl_relaxed(msm_pcie_dev[rc_idx].dm_core);
			u32 vendor_id = ids & 0xffff;
			u32 device_id = (ids & 0xffff0000) >> 16;

			PCIE_DBG(dev, "vendor-id:0x%x device_id:0x%x\n",
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
					PCIE_DBG(&msm_pcie_dev[rc_idx],
						"PCI device is found for RC%d\n",
						rc_idx);
				}
			} while (!found && pcidev);

			if (!pcidev) {
				PCIE_ERR(dev,
					"PCIe: Did not find PCI device for RC%d.\n",
					dev->rc_idx);
				return -ENODEV;
			}
		} else {
			PCIE_ERR(dev, "PCIe: failed to enable RC%d.\n",
				dev->rc_idx);
		}
	} else {
		PCIE_ERR(dev, "PCIe: RC%d has already been enumerated.\n",
			dev->rc_idx);
	}

	return ret;
}

static int msm_pcie_probe(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx = -1;
	int i;

	pr_debug("%s\n", __func__);

	mutex_lock(&pcie_drv.drv_lock);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ctrl-amt", &pcie_drv.rc_expected);
	if (ret) {
		pr_err("PCIe: does not find controller amount.\n");
		goto out;
	} else {
		if (pcie_drv.rc_expected > MAX_RC_NUM) {
			pr_debug("Expected number of devices %d\n",
							pcie_drv.rc_expected);
			pr_debug("Exceeded max supported devices %d\n",
							MAX_RC_NUM);
			goto out;
		}
		pr_debug("Target has %d RC(s).\n", pcie_drv.rc_expected);
	}

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &rc_idx);
	if (ret) {
		pr_debug("Did not find RC index.\n");
		goto out;
	} else {
		if (rc_idx >= MAX_RC_NUM) {
			pr_err(
				"PCIe: Invalid RC Index %d (max supported = %d)\n",
				rc_idx, MAX_RC_NUM);
			goto out;
		}
		pcie_drv.rc_num++;
		PCIE_DBG(&msm_pcie_dev[rc_idx], "PCIe: RC index is %d.\n",
			rc_idx);
	}

	msm_pcie_dev[rc_idx].l1ss_supported =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,l1ss-supported");
	PCIE_DBG(&msm_pcie_dev[rc_idx], "L1ss is %s supported.\n",
		msm_pcie_dev[rc_idx].l1ss_supported ? "" : "not");
	msm_pcie_dev[rc_idx].aux_clk_sync =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,aux-clk-sync");
	PCIE_DBG(&msm_pcie_dev[rc_idx],
		"AUX clock is %s synchronous to Core clock.\n",
		msm_pcie_dev[rc_idx].aux_clk_sync ? "" : "not");

	msm_pcie_dev[rc_idx].ep_wakeirq =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,ep-wakeirq");
	PCIE_DBG(&msm_pcie_dev[rc_idx],
		"PCIe: EP of RC%d does %s assert wake when it is up.\n",
		rc_idx, msm_pcie_dev[rc_idx].ep_wakeirq ? "" : "not");

	msm_pcie_dev[rc_idx].n_fts = 0;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,n-fts",
				&msm_pcie_dev[rc_idx].n_fts);

	if (ret)
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"n-fts does not exist. ret=%d\n", ret);
	else
		PCIE_DBG(&msm_pcie_dev[rc_idx], "n-fts: 0x%x.\n",
				msm_pcie_dev[rc_idx].n_fts);

	msm_pcie_dev[rc_idx].ext_ref_clk =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,ext-ref-clk");
	PCIE_DBG(&msm_pcie_dev[rc_idx], "ref clk is %s.\n",
		msm_pcie_dev[rc_idx].ext_ref_clk ? "external" : "internal");

	msm_pcie_dev[rc_idx].ep_latency = 0;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ep-latency",
				&msm_pcie_dev[rc_idx].ep_latency);
	if (ret)
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"RC%d: ep-latency does not exist.\n",
			rc_idx);
	else
		PCIE_DBG(&msm_pcie_dev[rc_idx], "RC%d: ep-latency: 0x%x.\n",
			rc_idx, msm_pcie_dev[rc_idx].ep_latency);

	msm_pcie_dev[rc_idx].msi_gicm_addr = 0;
	msm_pcie_dev[rc_idx].msi_gicm_base = 0;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,msi-gicm-addr",
				&msm_pcie_dev[rc_idx].msi_gicm_addr);

	if (ret) {
		PCIE_DBG(&msm_pcie_dev[rc_idx], "%s",
			"msi-gicm-addr does not exist.\n");
	} else {
		PCIE_DBG(&msm_pcie_dev[rc_idx], "msi-gicm-addr: 0x%x.\n",
				msm_pcie_dev[rc_idx].msi_gicm_addr);

		ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,msi-gicm-base",
				&msm_pcie_dev[rc_idx].msi_gicm_base);

		if (ret) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: msi-gicm-base does not exist.\n",
				rc_idx);
			goto decrease_rc_num;
		} else {
			PCIE_DBG(&msm_pcie_dev[rc_idx], "msi-gicm-base: 0x%x\n",
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
	msm_pcie_dev[rc_idx].linkdown_counter = 0;
	msm_pcie_dev[rc_idx].suspending = false;
	msm_pcie_dev[rc_idx].wake_counter = 0;
	msm_pcie_dev[rc_idx].power_on = false;
	msm_pcie_dev[rc_idx].use_msi = false;
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
	msm_pcie_dev[rc_idx].shadow_en = true;
	for (i = 0; i < PCIE_CONF_SPACE_DW; i++) {
		msm_pcie_dev[rc_idx].rc_shadow[i] = PCIE_CLEAR;
		msm_pcie_dev[rc_idx].ep_shadow[i] = PCIE_CLEAR;
	}

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

	if (msm_pcie_dev[rc_idx].ep_wakeirq) {
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"PCIe: RC%d will be enumerated upon WAKE signal from Endpoint.\n",
			rc_idx);
		mutex_unlock(&pcie_drv.drv_lock);
		return 0;
	}

	ret = msm_pcie_enumerate(rc_idx);

	if (ret)
		PCIE_ERR(&msm_pcie_dev[rc_idx],
			"PCIe: RC%d is not enabled during bootup; it will be enumerated upon WAKE signal.\n",
			rc_idx);
	else
		PCIE_ERR(&msm_pcie_dev[rc_idx], "RC%d is enabled in bootup\n",
			rc_idx);

	PCIE_DBG(&msm_pcie_dev[rc_idx], "PCIE probed %s\n",
		dev_name(&(pdev->dev)));
	mutex_unlock(&pcie_drv.drv_lock);
	return 0;

decrease_rc_num:
	pcie_drv.rc_num--;
out:
	PCIE_ERR(&msm_pcie_dev[rc_idx],
		"PCIe: Driver probe failed for RC%d:%d\n",
		rc_idx, ret);
	mutex_unlock(&pcie_drv.drv_lock);

	return ret;
}

static int __exit msm_pcie_remove(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx;

	pr_debug("PCIe:%s.\n", __func__);

	mutex_lock(&pcie_drv.drv_lock);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &rc_idx);
	if (ret) {
		pr_err("%s: Did not find RC index.\n", __func__);
		goto out;
	} else {
		pcie_drv.rc_num--;
		pr_debug("%s: RC index is 0x%x.", __func__, rc_idx);
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
	char rc_name[MAX_RC_NAME_LEN];

	pr_debug("pcie:%s.\n", __func__);

	pcie_drv.rc_num = 0;
	pcie_drv.rc_expected = 0;
	mutex_init(&pcie_drv.drv_lock);

	for (i = 0; i < MAX_RC_NUM; i++) {
		snprintf(rc_name, MAX_RC_NAME_LEN, "pcie%d", i);
		msm_pcie_dev[i].ipc_log =
			ipc_log_context_create(PCIE_LOG_PAGES, rc_name, 0);
		if (msm_pcie_dev[i].ipc_log == NULL)
			pr_err("%s: unable to create IPC log context for %s\n",
				__func__, rc_name);
		else
			PCIE_DBG(&msm_pcie_dev[i],
				"PCIe IPC logging is enable for RC%d\n",
				i);
		spin_lock_init(&msm_pcie_dev[i].cfg_lock);
		msm_pcie_dev[i].cfg_access = true;
		mutex_init(&msm_pcie_dev[i].setup_lock);
		mutex_init(&msm_pcie_dev[i].recovery_lock);
		spin_lock_init(&msm_pcie_dev[i].linkdown_lock);
		spin_lock_init(&msm_pcie_dev[i].wakeup_lock);
	}

	ret = platform_driver_register(&msm_pcie_driver);

	return ret;
}

static void __exit pcie_exit(void)
{
	pr_debug("pcie:%s.\n", __func__);

	platform_driver_unregister(&msm_pcie_driver);
}

subsys_initcall_sync(pcie_init);
module_exit(pcie_exit);


/* RC do not represent the right class; set it to PCI_CLASS_BRIDGE_PCI */
static void msm_pcie_fixup_early(struct pci_dev *dev)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);
	PCIE_DBG(pcie_dev, "hdr_type %d\n", dev->hdr_type);
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

	pcie_dev->suspending = true;
	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if (!pcie_dev->power_on) {
		PCIE_DBG(pcie_dev,
			"PCIe: power of RC%d has been turned off.\n",
			pcie_dev->rc_idx);
		return ret;
	}

	if (dev && !(options & MSM_PCIE_CONFIG_NO_CFG_RESTORE)
		&& msm_pcie_confirm_linkup(pcie_dev, true, true)) {
		ret = pci_save_state(dev);
		pcie_dev->saved_state =	pci_store_saved_state(dev);
	}
	if (ret) {
		PCIE_ERR(pcie_dev, "PCIe: fail to save state of RC%d:%d.\n",
			pcie_dev->rc_idx, ret);
		pcie_dev->suspending = false;
		return ret;
	}

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	pcie_dev->cfg_access = false;
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	msm_pcie_write_mask(pcie_dev->elbi + PCIE20_ELBI_SYS_CTRL, 0,
				BIT(4));

	PCIE_DBG(pcie_dev, "RC%d: PME_TURNOFF_MSG is sent out\n",
		pcie_dev->rc_idx);

	ret_l23 = readl_poll_timeout((pcie_dev->parf
		+ PCIE20_PARF_PM_STTS), val, (val & BIT(6)), 10000, 100000);

	/* check L23_Ready */
	if (!ret_l23)
		PCIE_DBG(pcie_dev, "RC%d: PM_Enter_L23 is received\n",
			pcie_dev->rc_idx);
	else
		PCIE_DBG(pcie_dev, "RC%d: PM_Enter_L23 is NOT received\n",
			pcie_dev->rc_idx);

	msm_pcie_disable(pcie_dev, PM_PIPE_CLK | PM_CLK | PM_VREG);

	return ret;
}

static void msm_pcie_fixup_suspend(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if (pcie_dev->link_status != MSM_PCIE_LINK_ENABLED)
		return;

	mutex_lock(&pcie_dev->recovery_lock);

	ret = msm_pcie_pm_suspend(dev, NULL, NULL, 0);
	if (ret)
		PCIE_ERR(pcie_dev, "PCIe: RC%d got failure in suspend:%d.\n",
			pcie_dev->rc_idx, ret);

	mutex_unlock(&pcie_dev->recovery_lock);
}
DECLARE_PCI_FIXUP_SUSPEND(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
			  msm_pcie_fixup_suspend);

/* Resume the PCIe link */
static int msm_pcie_pm_resume(struct pci_dev *dev,
			void *user, void *data, u32 options)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	pcie_dev->cfg_access = true;
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	ret = msm_pcie_enable(pcie_dev, PM_PIPE_CLK | PM_CLK | PM_VREG);
	if (ret) {
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d fail to enable PCIe link in resume.\n",
			pcie_dev->rc_idx);
		return ret;
	} else {
		pcie_dev->suspending = false;
		PCIE_DBG(pcie_dev,
			"dev->bus->number = %d dev->bus->primary = %d\n",
			 dev->bus->number, dev->bus->primary);

		if (!(options & MSM_PCIE_CONFIG_NO_CFG_RESTORE)) {
			pci_load_and_free_saved_state(dev,
					&pcie_dev->saved_state);
			pci_restore_state(dev);
		}
	}

	return ret;
}

void msm_pcie_fixup_resume(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if ((pcie_dev->link_status != MSM_PCIE_LINK_DISABLED) ||
		pcie_dev->user_suspend)
		return;

	mutex_lock(&pcie_dev->recovery_lock);
	ret = msm_pcie_pm_resume(dev, NULL, NULL, 0);
	if (ret)
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d got failure in fixup resume:%d.\n",
			pcie_dev->rc_idx, ret);

	mutex_unlock(&pcie_dev->recovery_lock);
}
DECLARE_PCI_FIXUP_RESUME(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
				 msm_pcie_fixup_resume);

void msm_pcie_fixup_resume_early(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if ((pcie_dev->link_status != MSM_PCIE_LINK_DISABLED) ||
		pcie_dev->user_suspend)
		return;

	mutex_lock(&pcie_dev->recovery_lock);
	ret = msm_pcie_pm_resume(dev, NULL, NULL, 0);
	if (ret)
		PCIE_ERR(pcie_dev, "PCIe: RC%d got failure in resume:%d.\n",
			pcie_dev->rc_idx, ret);

	mutex_unlock(&pcie_dev->recovery_lock);
}
DECLARE_PCI_FIXUP_RESUME_EARLY(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
				 msm_pcie_fixup_resume_early);

static void msm_pcie_fixup_final(struct pci_dev *dev)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);
	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);
	pcie_drv.current_rc++;
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, msm_pcie_fixup_final);

int msm_pcie_pm_control(enum msm_pcie_pm_opt pm_opt, u32 busnr, void *user,
			void *data, u32 options)
{
	int ret = 0;
	struct pci_dev *dev;
	u32 rc_idx = 0;

	pr_debug("PCIe: pm_opt:%d;busnr:%d;options:%d\n",
		pm_opt, busnr, options);

	switch (busnr) {
	case 1:
		if (user) {
			struct msm_pcie_dev_t *pcie_dev
				= PCIE_BUS_PRIV_DATA(((struct pci_dev *)user));

			if (pcie_dev) {
				rc_idx = pcie_dev->rc_idx;
				PCIE_DBG(pcie_dev,
					"PCIe: RC%d: pm_opt:%d;busnr:%d;options:%d\n",
					rc_idx, pm_opt, busnr, options);
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
		if (msm_pcie_dev[rc_idx].link_status != MSM_PCIE_LINK_ENABLED)
			PCIE_DBG(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: requested to suspend when link is not enabled:%d.\n",
				rc_idx, msm_pcie_dev[rc_idx].link_status);

		if (!msm_pcie_dev[rc_idx].power_on) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: requested to suspend when link is powered down:%d.\n",
				rc_idx, msm_pcie_dev[rc_idx].link_status);
			break;
		}

		msm_pcie_dev[rc_idx].user_suspend = true;

		mutex_lock(&msm_pcie_dev[rc_idx].recovery_lock);

		ret = msm_pcie_pm_suspend(dev, user, data, options);
		if (ret) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: user failed to suspend the link.\n",
				rc_idx);
			msm_pcie_dev[rc_idx].user_suspend = false;
		}

		mutex_unlock(&msm_pcie_dev[rc_idx].recovery_lock);
		break;
	case MSM_PCIE_RESUME:
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"User of RC%d requests to resume the link\n", rc_idx);
		if (msm_pcie_dev[rc_idx].link_status !=
					MSM_PCIE_LINK_DISABLED) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: requested to resume when link is not disabled:%d.\n",
				rc_idx, msm_pcie_dev[rc_idx].link_status);
			break;
		}

		mutex_lock(&msm_pcie_dev[rc_idx].recovery_lock);
		ret = msm_pcie_pm_resume(dev, user, data, options);
		if (ret) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: user failed to resume the link.\n",
				rc_idx);
		} else {
			PCIE_DBG(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: user succeeded to resume the link.\n",
				rc_idx);

			msm_pcie_dev[rc_idx].user_suspend = false;
		}

		mutex_unlock(&msm_pcie_dev[rc_idx].recovery_lock);
		break;
	default:
		PCIE_ERR(&msm_pcie_dev[rc_idx],
			"PCIe: RC%d: unsupported pm operation:%d.\n",
			rc_idx, pm_opt);
		ret = -ENODEV;
		goto out;
	}

out:
	return ret;
}
EXPORT_SYMBOL(msm_pcie_pm_control);

int msm_pcie_register_event(struct msm_pcie_register_event *reg)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (!reg) {
		pr_err("PCIe: Event registration is NULL\n");
		return -ENODEV;
	}

	if (!reg->user) {
		pr_err("PCIe: User of event registration is NULL\n");
		return -ENODEV;
	}

	pcie_dev = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));

	if (pcie_dev) {
		pcie_dev->event_reg = reg;
		PCIE_DBG(pcie_dev,
			"Event 0x%x is registered for RC %d\n", reg->events,
			pcie_dev->rc_idx);
	} else {
		PCIE_ERR(pcie_dev,
			"PCIe: did not find RC for pci endpoint device 0x%x.\n",
			(u32)reg->user);
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_register_event);

int msm_pcie_deregister_event(struct msm_pcie_register_event *reg)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (!reg) {
		pr_err("PCIe: Event deregistration is NULL\n");
		return -ENODEV;
	}

	if (!reg->user) {
		pr_err("PCIe: User of event deregistration is NULL\n");
		return -ENODEV;
	}

	pcie_dev = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));

	if (pcie_dev) {
		pcie_dev->event_reg = NULL;
		PCIE_DBG(pcie_dev, "Event is deregistered for RC %d\n",
				pcie_dev->rc_idx);
	} else {
		PCIE_ERR(pcie_dev,
			"PCIe: did not find RC for pci endpoint device 0x%x.\n",
			(u32)reg->user);
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_deregister_event);

int msm_pcie_recover_config(struct pci_dev *dev)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (dev) {
		pcie_dev = PCIE_BUS_PRIV_DATA(dev);
		PCIE_DBG(pcie_dev,
			"Recovery for the link of RC%d\n", pcie_dev->rc_idx);
	} else {
		pr_err("PCIe: the input pci dev is NULL.\n");
		return -ENODEV;
	}

	if (msm_pcie_confirm_linkup(pcie_dev, true, true)) {
		PCIE_DBG(pcie_dev,
			"Recover config space of RC%d and its EP\n",
			pcie_dev->rc_idx);
		pcie_dev->shadow_en = false;
		PCIE_DBG(pcie_dev, "Recover RC%d\n", pcie_dev->rc_idx);
		msm_pcie_cfg_recover(pcie_dev, true);
		PCIE_DBG(pcie_dev, "Recover EP of RC%d\n", pcie_dev->rc_idx);
		msm_pcie_cfg_recover(pcie_dev, false);
		PCIE_DBG(pcie_dev,
			"Refreshing the saved config space in PCI framework for RC%d and its EP\n",
			pcie_dev->rc_idx);
		pci_save_state(pcie_dev->dev);
		pci_save_state(dev);
		pcie_dev->shadow_en = true;
		PCIE_DBG(pcie_dev, "Turn on shadow for RC%d\n",
			pcie_dev->rc_idx);
	} else {
		PCIE_ERR(pcie_dev,
			"PCIe: the link of RC%d is not up yet; can't recover config space.\n",
			pcie_dev->rc_idx);
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_recover_config);

int msm_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (dev) {
		pcie_dev = PCIE_BUS_PRIV_DATA(dev);
		PCIE_DBG(pcie_dev,
			"Recovery for the link of RC%d\n", pcie_dev->rc_idx);
	} else {
		pr_err("PCIe: the input pci dev is NULL.\n");
		return -ENODEV;
	}

	PCIE_DBG(pcie_dev,
		"The shadowing of RC%d is %s enabled currently.\n",
		pcie_dev->rc_idx, pcie_dev->shadow_en ? "" : "not");

	pcie_dev->shadow_en = enable;

	PCIE_DBG(pcie_dev,
		"Shadowing of RC%d is turned %s upon user's request.\n",
		pcie_dev->rc_idx, enable ? "on" : "off");

	return ret;
}
EXPORT_SYMBOL(msm_pcie_shadow_control);
