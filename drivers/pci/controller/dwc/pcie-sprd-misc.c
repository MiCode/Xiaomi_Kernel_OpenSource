// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe host controller driver for Unisoc SoCs
 *
 * Copyright (C) 2020 Unisoc Inc.
 * http://www.unisoc.com
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/msi.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pcie-rc-sprd.h>
#include <linux/platform_device.h>

#include "pcie-designware.h"
#include "pcie-sprd.h"

#define	NUM_OF_ARGS 5

static void sprd_pcie_fix_class(struct pci_dev *dev)
{
	struct pcie_port *pp = dev->bus->sysdata;

	if (dev->class != PCI_CLASS_NOT_DEFINED)
		return;

	if (dev->bus == pp->bridge->bus)
		dev->class = 0x0604 << 8;
	else
		dev->class = 0x080d << 8;

	dev_info(&dev->dev,
		 "The class of device %04x:%04x is changed to: 0x%06x\n",
		 dev->device, dev->vendor, dev->class);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SYNOPSYS, 0xabcd, sprd_pcie_fix_class);

static void sprd_pcie_fix_header(struct pci_dev *dev)
{
	u32 val;
	struct pci_dev *bridge;
	struct pcie_port *pp;
	struct dw_pcie *pci;

	/*
	 * Only the RC which connects with marlin3 EP needs to modify
	 * port T_POWER_ON register.
	 */
	if ((dev->vendor == PCI_VENDOR_ID_MARLIN3) &&
	    (dev->device == PCI_DEVICE_ID_MARLIN3)) {
		bridge = dev->bus->self;
		pp = bridge->bus->sysdata;
		pci = to_dw_pcie_from_pp(pp);

		val = dw_pcie_readl_dbi(pci, SPRD_PCIE_GEN2_L1SS_CAP);
		val &= ~(PCIE_T_POWER_ON_SCALE_MASK |
			 PCIE_T_POWER_ON_VALUE_MASK);
		val |= (PCIE_T_POWER_ON_SCALE | PCIE_T_POWER_ON_VALUE);
		dw_pcie_writel_dbi(pci, SPRD_PCIE_GEN2_L1SS_CAP, val);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARLIN3, PCI_DEVICE_ID_MARLIN3,
			 sprd_pcie_fix_header);

int sprd_pcie_syscon_setting(struct platform_device *pdev, char *env)
{
	struct device_node *np = pdev->dev.of_node;
	int i, count, err, j;
	u32 type, delay, reg, mask, val, tmp_val;
	struct of_phandle_args out_args;
	struct regmap *iomap;

	if (!of_find_property(np, env, NULL)) {
		dev_info(&pdev->dev,
			 "there isn't property %s in dts\n", env);
		return 0;
	}

	/* one handle and NUM_OF_ARGS args */
	count = of_property_count_elems_of_size(np, env,
		(NUM_OF_ARGS + 1) * sizeof(u32));
	dev_info(&pdev->dev, "property (%s) reg count is %d :\n", env, count);

	for (i = 0; i < count; i++) {
		err = of_parse_phandle_with_fixed_args(np, env, NUM_OF_ARGS,
				i, &out_args);
		if (err < 0)
			return err;

		type = out_args.args[0];
		delay = out_args.args[1];
		reg = out_args.args[2];
		mask = out_args.args[3];
		val = out_args.args[4];

		iomap = syscon_node_to_regmap(out_args.np);
		switch (type) {
		case 0:
			regmap_update_bits(iomap, reg, mask, val);
			break;

		case 1:
			regmap_read(iomap, reg, &tmp_val);
			tmp_val &= (~mask);
			tmp_val |= (val & mask);
			regmap_write(iomap, reg, tmp_val);
			break;

		case 2:
			j = 0;
			do {
				regmap_read(iomap, reg, &tmp_val);
				/*
				 * Sometimes PCIe/IPA SYS power on very slowly,
				 * it may need 100ms.
				 */
				if (j++ > 2000)
					return -ETIMEDOUT;
				usleep_range(50, 100);
			} while ((tmp_val & mask) != val);
			break;
		}
		if (delay)
			usleep_range(delay, delay + 10);

		regmap_read(iomap, reg, &tmp_val);
		dev_info(&pdev->dev,
			"%2d:reg[0x%8x] mask[0x%8x] val[0x%8x] result[0x%8x]\n",
			i, reg, mask, val, tmp_val);
	}

	return i;
}
EXPORT_SYMBOL(sprd_pcie_syscon_setting);

int sprd_pcie_syscon_cache_setting(struct platform_device *pdev, char *env, bool enable)
{
	struct device_node *np = pdev->dev.of_node;
	int i, count, err;
	struct of_phandle_args out_args;
	struct regmap *iomap;

	if (!of_find_property(np, env, NULL)) {
		dev_info(&pdev->dev,
			 "there isn't property %s in dts\n", env);
		return 0;
	}
	count = of_property_count_elems_of_size(np, env, sizeof(u32));

	for (i = 0; i < count; i++) {
		err = of_parse_phandle_with_fixed_args(np, env, 0,
				i, &out_args);
		if (err < 0)
			return err;
		iomap = syscon_node_to_regmap(out_args.np);
		regcache_cache_only(iomap, enable);
	}

	return err;
}
EXPORT_SYMBOL(sprd_pcie_syscon_cache_setting);

int sprd_pcie_enter_pcipm_l2(struct dw_pcie *pci)
{
	u32 reg;
	int retries;

	reg = dw_pcie_readl_dbi(pci, SPRD_PCIE_PE0_PM_CTRL);
	reg |= SPRD_PCIE_APP_CLK_PM_EN;
	dw_pcie_writel_dbi(pci, SPRD_PCIE_PE0_PM_CTRL, reg);

	reg = dw_pcie_readl_dbi(pci, SPRD_PCIE_PE0_TX_MSG_REG);
	reg |= SPRD_PCIE_PME_TURN_OFF_REQ;
	dw_pcie_writel_dbi(pci, SPRD_PCIE_PE0_TX_MSG_REG, reg);

	for (retries = 0; retries < ENTER_L2_MAX_RETRIES; retries++) {
		reg = dw_pcie_readl_dbi(pci, PCIE_PORT_DEBUG0);
		if ((reg & PORT_LOGIC_LTSSM_STATE_MASK) == LTSSM_STATE_L2_IDLE) {
			dev_info(pci->dev,
				 "enter L2, LTSSM state: 0x%x\n", reg);
			return 0;
		}
		/* TODO: now we can't sure the delay time */
		usleep_range(1000, 2000);
	}
	dev_err(pci->dev,
		"can't enter L2, [0x%x]: 0x%x, [0x%x]: 0x%x, [0x%x]: 0x%x\n",
		SPRD_PCIE_PE0_PM_CTRL,
		dw_pcie_readl_dbi(pci, SPRD_PCIE_PE0_PM_CTRL),
		SPRD_PCIE_PE0_PM_STS,
		dw_pcie_readl_dbi(pci, SPRD_PCIE_PE0_PM_STS),
		PCIE_PORT_DEBUG0,
		dw_pcie_readl_dbi(pci, PCIE_PORT_DEBUG0));

	return -ETIMEDOUT;
}
EXPORT_SYMBOL(sprd_pcie_enter_pcipm_l2);

/*
 * WORKAROUND:
 * Clear unhandled msi irqs before pcie power off.
 * if an endpoint asserts a msi irq and then free the irq before
 * it is handled, the irq line may be always active.
 */
void sprd_pcie_clear_unhandled_msi(struct dw_pcie *pci)
{
	u32 val, i;

	for (i = 0; i < MAX_MSI_CTRLS; i++) {
		val = dw_pcie_readl_dbi(pci, PCIE_MSI_INTR0_STATUS + i * 12);
		if (val) {
			dev_warn(pci->dev,
				 "clear unhandled 0x%x in MSI INTR%d",
				 val, i);
			dw_pcie_writel_dbi(pci,
					   PCIE_MSI_INTR0_ENABLE + i * 12, val);
		}
	}
}
EXPORT_SYMBOL(sprd_pcie_clear_unhandled_msi);

void sprd_pcie_save_msi_ctrls(struct dw_pcie *pci)
{
	int i;
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	for (i = 0; i < MAX_MSI_CTRLS; i++) {
		ctrl->save_msi_ctrls[i][0] =
			dw_pcie_readl_dbi(pci, PCIE_MSI_INTR0_ENABLE + i * 12);
		ctrl->save_msi_ctrls[i][1] =
			dw_pcie_readl_dbi(pci, PCIE_MSI_INTR0_MASK + i * 12);
		ctrl->save_msi_ctrls[i][2] =
			dw_pcie_readl_dbi(pci, PCIE_MSI_INTR0_STATUS + i * 12);
	}
}

void sprd_pcie_save_dwc_reg(struct dw_pcie *pci)
{
	sprd_pcie_save_msi_ctrls(pci);
}
EXPORT_SYMBOL(sprd_pcie_save_dwc_reg);

void sprd_pcie_restore_msi_ctrls(struct dw_pcie *pci)
{
	int i;
	u64 msi_target;
	struct pcie_port *pp = &pci->pp;
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	msi_target = (u64)pp->msi_data;
	dw_pcie_writel_dbi(pci, PCIE_MSI_ADDR_LO,
			   (u32)(msi_target & 0xffffffff));
	dw_pcie_writel_dbi(pci, PCIE_MSI_ADDR_HI,
			   (u32)(msi_target >> 32 & 0xfffffff));

	for (i = 0; i < MAX_MSI_CTRLS; i++) {
		dw_pcie_writel_dbi(pci, PCIE_MSI_INTR0_ENABLE + i * 12,
				   ctrl->save_msi_ctrls[i][0]);
		dw_pcie_writel_dbi(pci, PCIE_MSI_INTR0_MASK + i * 12,
				   ctrl->save_msi_ctrls[i][1]);
		dw_pcie_writel_dbi(pci, PCIE_MSI_INTR0_STATUS + i * 12,
				   ctrl->save_msi_ctrls[i][2]);
	}
}

void sprd_pcie_restore_dwc_reg(struct dw_pcie *pci)
{
	sprd_pcie_restore_msi_ctrls(pci);
}
EXPORT_SYMBOL(sprd_pcie_restore_dwc_reg);

/*
 *  Orca pci asic design for normal interrupt mode is not compatible with
 *  kernel PCIE framework, the 0-3 msi irqs are handled directly by hardware.
 *  This workaround is used to disable the corresponding bits of these four irqs
 *  on the msi register so that AP GIC will not receive these interrupts.
 */
void sprd_pcie_teardown_msi_irq(unsigned int irq)
{
	unsigned int msi_ctrls, bit, val;
	struct irq_data *data = irq_get_irq_data(irq);
	struct msi_desc *msi = irq_data_get_msi_desc(data);
	struct pcie_port *pp = (struct pcie_port *)msi_desc_to_pci_sysdata(msi);
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);

	msi_ctrls = (data->hwirq / 32) * 12;
	bit = data->hwirq % 32;
	val = dw_pcie_readl_dbi(pci, PCIE_MSI_INTR0_ENABLE + msi_ctrls);
	val &= ~(1 << bit);
	dw_pcie_writel_dbi(pci,  PCIE_MSI_INTR0_ENABLE + msi_ctrls, val);
}
EXPORT_SYMBOL(sprd_pcie_teardown_msi_irq);

#ifdef CONFIG_SPRD_PCIE_AER
void sprd_pcie_alloc_irq_vectors(struct pci_dev *dev, int *irqs, int services)
{
	struct pcie_port *pp = dev->bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	/*
	 * Unisoc PCIe RC only supports the following port service type
	 * (please see pcieport_if.h):
	 * -1. Power Management Event (PME)
	 * -2. Advanced Error Reporting (AER)
	 *  However, only AER irq is used right now.
	 */
	irqs[1] = ctrl->aer_irq;
}
EXPORT_SYMBOL(sprd_pcie_alloc_irq_vectors);
#endif

void sprd_pcie_dump_rc_regs(struct platform_device *pdev)
{
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;
	u32 index, offset;

	dev_err(&pdev->dev,
		  "LTSSM [0xe64]: 0x%x, [0x728]: 0x%x, [0xe04]: 0x%x\n",
		  dw_pcie_readl_dbi(pci, SPRD_PCIE_PE0_PM_STS),
		  dw_pcie_readl_dbi(pci, PCIE_PORT_DEBUG0),
		  dw_pcie_readl_dbi(pci, PCIE_SS_REG_BASE+APB_CLKFREQ_TIMEOUT));

	print_hex_dump(KERN_ERR, "PCIe RC reg: ", DUMP_PREFIX_ADDRESS,
		       16, 4, pci->dbi_base, 0xc0, 0);
	print_hex_dump(KERN_ERR, "PCIe RC reg: ", DUMP_PREFIX_ADDRESS,
		       16, 4, pci->dbi_base + 0x100, 0x80, 0);
	print_hex_dump(KERN_ERR, "PCIe RC reg: ", DUMP_PREFIX_ADDRESS,
		       16, 4, pci->dbi_base + PCIE_MSI_ADDR_LO, 0x20, 0);
	for (index = 0; index <= 2; index++) {
		offset = PCIE_GET_ATU_OUTB_UNR_REG_OFFSET(index);
		print_hex_dump(KERN_ERR, "PCIe RC reg: ", DUMP_PREFIX_ADDRESS,
			       16, 4, pci->dbi_base + offset, 0x20, 0);
	}
}
EXPORT_SYMBOL(sprd_pcie_dump_rc_regs);

/* Check pci vendor id before modify its own config registers */
int sprd_pcie_check_vendor_id(struct dw_pcie *pci)
{
	u16 val;
	int retries;

	for (retries = 0; retries < 100; retries++) {
		val = dw_pcie_readw_dbi(pci, PCI_VENDOR_ID);
		if (val == PCI_VENDOR_ID_SYNOPSYS)
			return 0;
		usleep_range(50, 100);
	}

	dev_err(pci->dev, "the vendor id is:0x%8x, retries:%d", val, retries);

	return -ETIMEDOUT;
}
EXPORT_SYMBOL(sprd_pcie_check_vendor_id);

/* Check PCIE_ATU_VIEWPORT before dw_pcie_iatu_detect() */
void sprd_pcie_check_atu_viewport(struct dw_pcie *pci)
{
	u32 val, retries;

	for (retries = 0; retries < 1000; retries++) {
		val = dw_pcie_readl_dbi(pci, PCIE_ATU_VIEWPORT);
		if (val == 0xFFFFFFFF)
			return;
		usleep_range(50, 100);
	}

	dev_err(pci->dev,
		"the PCIE_ATU_VIEWPORT is:0x%8x, retries:%d", val, retries);
}
EXPORT_SYMBOL(sprd_pcie_check_atu_viewport);

/*
 * 1. First configure your own configuration space register, and then establish
 * the link to avoid the register exception caused by PCI instability during
 * the link process.
 * 2. Clear the ltssm_en bit before power down the pci to avoid establishing
 * the link immediately after the next power up.
 */
void sprd_pcie_ltssm_enable(struct dw_pcie *pci, bool enable)
{
	u32 val;

	val = dw_pcie_readl_dbi(pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3);
	if (enable)
		dw_pcie_writel_dbi(pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3,
				   val | LTSSM_EN);
	else
		dw_pcie_writel_dbi(pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3,
				   val &  ~LTSSM_EN);
}
EXPORT_SYMBOL(sprd_pcie_ltssm_enable);

MODULE_DESCRIPTION("Unisoc PCIe host controller driver");
MODULE_LICENSE("GPL");
