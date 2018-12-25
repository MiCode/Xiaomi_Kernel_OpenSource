/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Liang-Yen Wang <liang-yen.wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/phy/phy.h>


/* PCI CfgWr/CfgRd registers */
#define CFG_HEADER_0			0x460
#define CFG_HEADER_1			0x464
#define CFG_HEADER_2			0x468
#define CFG_HEADER_3			0x46c
#define CFG_WDATA			0x470
#define APP_TLP_REQ			0x488
#define APP_CFG_REQ			BIT(0)
#define APP_MSG_REQ			BIT(1)
#define APP_CPL_STATUS			GENMASK(7, 5)
#define CFG_RDATA			0x48c

/* PCI Interrupt registers */
#define INT_MASK			0x420
#define INTA_MASK			BIT(16)
#define INTB_MASK			BIT(17)
#define INTC_MASK			BIT(18)
#define	INTD_MASK			BIT(19)
#define INTX_MASK			(INTA_MASK | INTB_MASK | \
					INTC_MASK | INTD_MASK)
#define MTK_PCIE_INTX_SHIFT		16
#define	MSI_MASK			BIT(23)
#define PCIE_L2_ENTRY_WAKE_MASK		BIT(30)
#define INT_STATUS			0x424
#define INTA_STATUS			BIT(16)
#define INTB_STATUS			BIT(17)
#define INTC_STATUS			BIT(18)
#define INTD_STATUS			BIT(19)
#define PM_HP_EVENT_STATUS		BIT(21)
#define AER_EVENT_STATUS		BIT(22)
#define MSI_STATUS			BIT(23)
#define PCIE_L2_ENTRY_WAKE_STATUS	BIT(30)
#define IMSI_STATUS			0x42c

/* IP Configuration registers */
#define K_GBL_1				0x000
#define K_CONF_FUNC0_0			0x100
#define K_CONF_FUNC0_1			0x104
#define K_CONF_FUNC0_2			0x108

#define MTK_ROOT_PORT			((4 << 12) & 0xf000)
#define MTK_VEND_ID			(0x14c3)
#define MTK_DEV_ID			(0x5396)
#define MTK_SS_VEND_ID			(0x14c3)
#define MTK_SS_DEV_ID			(0x5396)

#define K_CONF_IDS(vend, dev)		((vend & 0xffff) | \
					((dev << 16) & 0xffff0000))

/* PCI MAC registers */
#define PCI_RSTCR			0x510
#define PCI_PHY_RSTB			BIT(0)
#define PCI_PIPE_SRSTB			BIT(1)
#define PCI_MAC_SRSTB			BIT(2)
#define PCI_CRSTB			BIT(3)
#define PCI_PERSTB			BIT(8)
#define PCI_PIPE_RST_EN			BIT(13)
#define PCI_MAC_RST_EN			BIT(14)
#define PCI_CONF_RST_EN			BIT(15)
#define PCI_LINKDOWN_RST_EN		(PCI_PIPE_RST_EN | PCI_MAC_RST_EN | \
					PCI_CONF_RST_EN)
#define PCI_WAKE_CONTROL		0x52c
#define CLKREQ_N_ENABLE			BIT(0)
#define PCI_LINK_STATUS			0x804
#define PCI_LINKUP			BIT(10)
#define IMSI_ADDR			0x430
#define MSI_VECTOR			0x0c0
#define MSI_IRQS			32
#define PCIE_PORT_MSI_BIT		32

/* 32 msi irqs for endpoint(b[31:0]) & 1 msi irq for pcie port(b32) */
#define MAX_MSI_IRQS			(MSI_IRQS + 1)

#define INTX_IRQ_NUM			5
#define AHB2PCIE_BASE0_L		0x438
#define AHB2PCIE_BASE0_H		0x43c
#define AHB2PCIE_BASE1_L		0x440
#define AHB2PCIE_BASE1_H		0x444
#define PCIE2AXI_WIN			0x448
#define WIN_NOPREFETCH			BIT(6)
#define WIN_ENABLE			BIT(7)

#define AHB2PCIE_BASEL(base)		(base & 0xffffffff)
#define AHB2PCIE_BASEH(base)		(base >> 32)
#define NOPREFETCH(flag)		((flag & IORESOURCE_PREFETCH)?0:1 << 6)
#define BASE_SIZE(sz)			(sz & 0x1f)

/* Translated AXI base address */
#define PCIE2AXI_SIZE			0x1ffffffff

/* PCI Configuration Transaction Header */
#define CFG_DW0_LENGTH(length)		(length & 0x3ff)
#define CFG_DW0_ATTR(attr)		((attr << 12) & 0x3000)
#define CFG_DW0_EP(ep)		        ((ep << 14) & 0x4000)
#define CFG_DW0_TD(td)			((td << 15) & 0x8000)
#define CFG_DW0_TC(tc)			((tc << 20) & 0x700000)
#define CFG_DW0_TYPE(type)		((type << 24) & 0x1f000000)
#define CFG_DW0_FMT(fmt)		((fmt << 29) & 0xe0000000)
#define CFG_DW1_FBE(fbe)		(fbe & 0x0f)
#define CFG_DW1_LBE(lbe)		((lbe << 4) & 0xf0)
#define CFG_DW1_TAG(tag)		((tag << 8) & 0xff00)
#define CFG_DW1_RID(rid)		((rid << 16) & 0xffff0000)
#define CFG_DW2_REGN(regn)		((((regn >> 2) & 0x3f) << 2) | \
					(((regn >> 8) & 0x0f) << 8))
#define CFG_DW2_FUN(fun)		((fun << 16) & 0x070000)
#define CFG_DW2_DEV(dev)		((dev << 19) & 0xf80000)
#define CFG_DW2_BUS(bus)		((bus << 24) & 0xff000000)

#define CFG_HEADER_DW0(ep, td, type, fmt) \
	(CFG_DW0_LENGTH(1) | CFG_DW0_ATTR(0) | CFG_DW0_EP(ep) | \
	 CFG_DW0_TD(td) | CFG_DW0_TC(0) | CFG_DW0_TYPE(type) | CFG_DW0_FMT(fmt))

#define CFG_HEADER_DW1(fbe, tag, rid) \
	(CFG_DW1_FBE(fbe) | CFG_DW1_LBE(0) | \
	 CFG_DW1_TAG(tag) | CFG_DW1_RID(rid))

#define CFG_HEADER_DW2(regn, fun, dev, bus) \
	(CFG_DW2_REGN(regn) | CFG_DW2_FUN(fun) | \
	 CFG_DW2_DEV(dev) | CFG_DW2_BUS(bus))

struct mtk_pcie_suspend_resume_reg {
	bool linkup;
};

/**
 * struct mtk_pcie_port - PCIe port information
 * @name: PCIe port name
 * @base: IO Mapped Register Base
 * @irq: Interrupt number
 * @port: Port number
 * @lane: Lane count
 * @devfn: Device/Function number
 * @pcie: PCIe host info pointer
 * @irq_domain:  IRQ domain pointer
 * @msi_irq_domain:  MSI IRQ domain pointer
 */
struct mtk_pcie_port {
	char *name;
	void __iomem *base;
	u32 irq;
	u32 port;
	u32 lane;
	int devfn;
	struct phy *phy;
	struct clk	*aux, *obff, *ahb, *axi, *mac, *pipe;
	struct device *dev;
	struct mtk_pcie *pcie;
	struct mtk_pcie_suspend_resume_reg reg;
	struct irq_domain *irq_domain;
	struct irq_domain *msi_irq_domain;
	DECLARE_BITMAP(msi_irq_in_use, MSI_IRQS);
};

/**
 * struct mtk_pcie - PCIe host information
 * @ports: PCIe port information pointer
 * @dev: Device pointer
 * @bus_range: Bus range
 * @pref_mem: Prefetchable Memory
 * @npref_mem: Non-Prefetchable Memory
 * @resources: Bus Resources
 * @nports: Number of PCIe ports
 */
struct mtk_pcie {
	struct mtk_pcie_port *ports;
	struct device *dev;
	struct resource bus_range;
	struct resource pref_mem;
	struct resource npref_mem;
	struct list_head resources;
	void __iomem *base;
	int nports;
};

struct mtk_pcie_plat {
	bool ltssm;
	bool io_coherent;
	/* for mt7622 may have two xtal, this indicate 7622 have 40M xtal*/
	bool xtal;
};

static inline u32 pcie_read(struct mtk_pcie_port *port, u32 reg)
{
	return readl(port->base + reg);
}

static inline void pcie_write(struct mtk_pcie_port *port, u32 val, u32 reg)
{
	writel(val, port->base + reg);
}

/*
 * Looks up the list of register addresses encoded into the reg =
 * <...> property . Once found, maps it.
 */
static void __iomem *mtk_pcie_map_registers(struct platform_device *pdev,
					      u32 port)
{
	struct resource *regs;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, port);
	return devm_ioremap_resource(&pdev->dev, regs);
}

static int mtk_pcie_clk_get(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;

	/*
	 * mt2712 only have ahb and mac clock, other clocks is for mt7622.
	 * ahb clock, for access pcie mac control register.
	 */
	port->ahb = devm_clk_get(dev, "ahb");
	if (IS_ERR(port->ahb))
		return PTR_ERR(port->ahb);

	/* mac clock, for pcie work clock. */
	port->mac = devm_clk_get(dev, "mac");
	if (IS_ERR(port->mac))
		return PTR_ERR(port->mac);

	port->aux = devm_clk_get(dev, "aux");
	if (IS_ERR(port->aux))
		return PTR_ERR(port->aux);

	port->obff = devm_clk_get(dev, "obff");
	if (IS_ERR(port->obff))
		return PTR_ERR(port->obff);

	port->axi = devm_clk_get(dev, "axi");
	if (IS_ERR(port->axi))
		return PTR_ERR(port->axi);

	port->pipe = devm_clk_get(dev, "pipe");
	if (IS_ERR(port->pipe))
		return PTR_ERR(port->pipe);

	return 0;
}

static int mtk_pcie_port_enable(struct mtk_pcie_port *port)
{
	int ret;
	struct device *dev = port->dev;

	/* mt2712 only have ahb and sel clock */
	if (!IS_ERR(port->ahb)) {
		ret = clk_prepare_enable(port->ahb);
		if (ret)
			goto err_put_pm;
	}

	if (!IS_ERR(port->mac)) {
		ret = clk_prepare_enable(port->mac);
		if (ret)
			goto err_put_pm;
	}

	if (!IS_ERR(port->aux)) {
		ret = clk_prepare_enable(port->aux);
		if (ret)
			goto err_put_pm;
	}

	/* just handle power error for now.*/
	if (!IS_ERR(port->obff)) {
		ret = clk_prepare_enable(port->obff);
		if (ret)
			goto err_put_pm;
	}

	if (!IS_ERR(port->axi)) {
		ret = clk_prepare_enable(port->axi);
		if (ret)
			goto err_put_pm;
	}

	if (!IS_ERR(port->pipe)) {
		ret = clk_prepare_enable(port->pipe);
		if (ret)
			goto err_put_pm;
	}

	return 0;

err_put_pm:
	if (dev->pm_domain)
		pm_runtime_put_sync(dev);

	return ret;
}

/*
 * PCIe clock and power could not be disabled, but we add this for debug use,
 * we may need to add debug interface for userspace to disable pcie clock and
 * power for measuring power consume. keep it for now.
 */
static void __maybe_unused mtk_pcie_port_disable(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;

	/* mt2712 does not enable ccf for now.*/
	if (!IS_ERR(port->pipe))
		clk_disable_unprepare(port->pipe);
	if (!IS_ERR(port->mac))
		clk_disable_unprepare(port->mac);
	if (!IS_ERR(port->axi))
		clk_disable_unprepare(port->axi);
	if (!IS_ERR(port->ahb))
		clk_disable_unprepare(port->ahb);
	if (!IS_ERR(port->obff))
		clk_disable_unprepare(port->obff);
	if (!IS_ERR(port->aux))
		clk_disable_unprepare(port->aux);

	/* mt2712 does not enable power domain for now.*/
	if (dev->pm_domain)
		pm_runtime_put_sync(dev);
}

static struct mtk_pcie_port *mtk_pcie_find_port(struct mtk_pcie *pcie,
						    struct pci_bus *bus,
						    int devfn)
{
	struct pci_dev *dev;
	struct pci_bus *pbus;
	int i;

	for (i = 0; i < pcie->nports; i++) {
		struct mtk_pcie_port *port = &pcie->ports[i];

		if (bus->number == 0 && port->devfn == devfn) {
			return port;
		} else if (bus->number != 0) {
			pbus = bus;
			do {
				dev = pbus->self;
				if (port->devfn == dev->devfn)
					return port;
				pbus = dev->bus;
			} while (dev->bus->number != 0);
		}
	}

	return NULL;
}

static inline bool mtk_pcie_link_is_up(struct mtk_pcie_port *port)
{
	return (pcie_read(port, PCI_LINK_STATUS) &
		PCI_LINKUP) ? 1 : 0;
}

static int mtk_pcie_mapping_sz(u64 size)
{
	int i = 0;

	while (size) {
		size >>= 1;
		i++;
	}
	return i;
}

/**
 * mtk_pcie_check_size - Check if cfg size is valid
 * @szie: PCI cfgrd/cfgwr size
 * @where: PCI cfgrd/cfgwr register number
 * @ByteEn: byte enable field in PCI cfgrd/cfgwr header
 *
 * Return: 'true' on success and 'false' if cfg size is invalid
 */
static bool mtk_pcie_check_size(u32 size, u32 where, u32 *ByteEn)
{
	int i = 0;

	/* cfgrd/cfgwr must be on a DWORD boundary */
	if (size + (where & 3) > 4)
		return false;

	/* find the value for the 1st DW Byte Enables */
	*ByteEn = 0;
	while (i < size) {
		*ByteEn |= 1 << i;
		i++;
	}
	*ByteEn <<= (where & 0x3);

	return true;
}

static int mtk_pcie_check_cfg_cpld(struct mtk_pcie_port *port)
{
	int count = 10;

	while (1) {
		/* 000: Success
		 * 001: UR
		 * 010: CRS
		 * 100: CA
		 * 111: Completion Timeout
		 */
		if (!(pcie_read(port, APP_TLP_REQ) & APP_CFG_REQ)) {
			if (pcie_read(port, APP_TLP_REQ) & APP_CPL_STATUS)
				return PCIBIOS_SET_FAILED;
			return PCIBIOS_SUCCESSFUL;
		}
		if (!count--)
			return PCIBIOS_SET_FAILED;
		mdelay(10);
	}
}

static int mtk_pcie_hw_rd_cfg(struct mtk_pcie_port *port, u32 bus, u32 devfn,
	int where, int size, u32 *val)
{
	int byte_enable = 0, reg, shift = 8 * (where & 3);

	*val = 0;
	/* check if cfgwr data is on a DWORD boundary */
	if (!mtk_pcie_check_size(size, where, &byte_enable))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	/* write PCI Configuration Transaction Header for cfgrd */
	pcie_write(port, CFG_HEADER_DW0(0, 0, 4, 0), CFG_HEADER_0);
	pcie_write(port, CFG_HEADER_DW1(byte_enable, 0, 0), CFG_HEADER_1);
	pcie_write(port, CFG_HEADER_DW2(where, PCI_FUNC(devfn),
		PCI_SLOT(devfn), bus), CFG_HEADER_2);
	/* triget h/w to transmit cfgrd TLP */
	reg = pcie_read(port, APP_TLP_REQ);
	pcie_write(port, reg | APP_CFG_REQ, APP_TLP_REQ);
	/* Check complete condition */
	if (mtk_pcie_check_cfg_cpld(port))
		return PCIBIOS_SET_FAILED;
	/* read cpld payload of cfgrd */
	*val = pcie_read(port, CFG_RDATA);

	if (size == 1)
		*val = (*val >> shift) & 0xff;
	else if (size == 2)
		*val = (*val >> shift) & 0xffff;
	else if (size == 3)
		*val = (*val >> shift) & 0xffffff;

	return PCIBIOS_SUCCESSFUL;
}

static int mtk_pcie_hw_wr_cfg(struct mtk_pcie_port *port, u32 bus, u32 devfn,
	int where, int size, u32 val)

{
	int byte_enable, reg, _val, shift = 8 * (where & 3);

	/* check if cfgwr data is on a DWORD boundary */
	if (!mtk_pcie_check_size(size, where, &byte_enable))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	/* write PCI Configuration Transaction Header for cfgwr */
	pcie_write(port, CFG_HEADER_DW0(0, 0, 4, 2), CFG_HEADER_0);
	pcie_write(port, CFG_HEADER_DW1(byte_enable, 0, 0), CFG_HEADER_1);
	pcie_write(port, CFG_HEADER_DW2(where, PCI_FUNC(devfn),
		PCI_SLOT(devfn), bus), CFG_HEADER_2);
	/* write cfgwr data */
	_val = val << shift;
	pcie_write(port, _val, CFG_WDATA);
	/* triget h/w to transmit cfgwr TLP */
	reg = pcie_read(port, APP_TLP_REQ);
	pcie_write(port, reg | APP_CFG_REQ, APP_TLP_REQ);
	/* Check complete condition */
	return mtk_pcie_check_cfg_cpld(port);
}

/**
 * mtk_pcie_read_config - Read configuration space
 * @bus: PCI Bus structure
 * @devfn: Device/function
 * @where: Offset from base
 * @size: Byte/word/dword
 * @val: Value to be read
 *
 * Return: PCIBIOS_SUCCESSFUL on success
 *	   PCIBIOS_DEVICE_NOT_FOUND on failure
 */
static int mtk_pcie_read_config(struct pci_bus *bus, unsigned int devfn,
				   int where, int size, u32 *val)
{
	u32 bn = bus->number;
	struct mtk_pcie *pcie = bus->sysdata;
	struct mtk_pcie_port *port;
	int ret;

	port = mtk_pcie_find_port(pcie, bus, devfn);
	if (!port) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	ret = mtk_pcie_hw_rd_cfg(port, bn, devfn, where, size,  val);
	if (ret)
		*val = 0xffffffff;

	return ret;
}

/**
 * mtk_pcie_write_config - Write configuration space
 * @bus: PCI Bus structure
 * @devfn: Device/function
 * @where: Offset from base
 * @size: Byte/word/dword
 * @val: Value to be written to device
 *
 * Return: PCIBIOS_SUCCESSFUL on success
 *	   PCIBIOS_DEVICE_NOT_FOUND on failure
 */
static int mtk_pcie_write_config(struct pci_bus *bus, unsigned int devfn,
				    int where, int size, u32 val)
{
	u32 bn = bus->number;
	struct mtk_pcie *pcie = bus->sysdata;
	struct mtk_pcie_port *port;

	port = mtk_pcie_find_port(pcie, bus, devfn);
	if (!port)
		return PCIBIOS_DEVICE_NOT_FOUND;
	return mtk_pcie_hw_wr_cfg(port, bn, devfn, where, size,  val);
}


/* PCIe operations */
static struct pci_ops mtk_pcie_ops = {
	.read  = mtk_pcie_read_config,
	.write = mtk_pcie_write_config,
};

/**
 * mtk_pcie_assign_msi - Allocate MSI number
 * @port: PCIe port structure
 *
 * Return: A valid IRQ on success and error value on failure.
 */
static int mtk_pcie_assign_msi(struct mtk_pcie_port *port)
{
	int pos;

	pos = find_first_zero_bit(port->msi_irq_in_use, MSI_IRQS);
	if (pos < MSI_IRQS)
		set_bit(pos, port->msi_irq_in_use);
	else
		return -ENOSPC;

	return pos;
}

/**
 * mtk_msi_teardown_irq - Destroy the MSI
 * @chip: MSI Chip descriptor
 * @irq: MSI IRQ to destroy
 */
static void mtk_msi_teardown_irq(struct msi_controller *chip, unsigned int irq)
{
	struct pci_dev *pdev = to_pci_dev(chip->dev);
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	struct mtk_pcie *pcie;
	struct mtk_pcie_port *port;

	pcie = pdev->bus->sysdata;
	port = mtk_pcie_find_port(pcie, pdev->bus, pdev->devfn);
	if (!port)
		dev_err(pcie->dev, "RC port is not found\n");

	if (!test_bit(hwirq, port->msi_irq_in_use))
		dev_err(pcie->dev, "Trying to free unused MSI#%d\n", irq);
	else
		clear_bit(hwirq, port->msi_irq_in_use);
}

/**
 * mtk_pcie_msi_setup_irq - Setup MSI request
 * @chip: MSI chip pointer
 * @pdev: PCIe device pointer
 * @desc: MSI descriptor pointer
 *
 * Return: '0' on success and error value on failure
 */
static int mtk_pcie_msi_setup_irq(struct msi_controller *chip,
				     struct pci_dev *pdev,
				     struct msi_desc *desc)
{
	struct mtk_pcie *pcie = pdev->bus->sysdata;
	struct mtk_pcie_port *port;
	u32 irq, data;
	int hwirq;
	struct msi_msg msg;

	port = mtk_pcie_find_port(pcie, pdev->bus, pdev->devfn);
	if (!port)
		return -EINVAL;

	chip->dev = &pdev->dev;
	if (pdev->bus->number == 0 &&
			port->devfn == pdev->devfn) {
		hwirq = PCIE_PORT_MSI_BIT;
		data = 0;
	} else
		data = hwirq = mtk_pcie_assign_msi(port);
	if (hwirq < 0)
		return hwirq;

	irq = irq_create_mapping(port->msi_irq_domain, hwirq);
	if (!irq)
		return -EINVAL;

	irq_set_msi_desc(irq, desc);
	msg.address_hi = 0;
	msg.address_lo = (u32)(u64)(port->base + MSI_VECTOR);
	msg.data = data;
	write_msi_msg(irq, &msg);

	return 0;
}

/* MSI Chip Descriptor */
static struct msi_controller mtk_pcie_msi_chip = {
	.setup_irq = mtk_pcie_msi_setup_irq,
	.teardown_irq = mtk_msi_teardown_irq,
};

/* HW Interrupt Chip Descriptor */
static struct irq_chip mtk_msi_irq_chip = {
	.name = "MTK PCIe MSI",
	.irq_enable = unmask_msi_irq,
	.irq_disable = mask_msi_irq,
	.irq_mask = mask_msi_irq,
	.irq_unmask = unmask_msi_irq,
};

/**
 * mtk_pcie_msi_map - Set the handler for the MSI and mark IRQ as valid
 * @domain: IRQ domain
 * @irq: Virtual IRQ number
 * @hwirq: HW interrupt number
 *
 * Return: Always returns 0.
 */
static int mtk_pcie_msi_map(struct irq_domain *domain, unsigned int irq,
			       irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &mtk_msi_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);

	return 0;
}

/* IRQ Domain operations */
static const struct irq_domain_ops msi_domain_ops = {
	.map = mtk_pcie_msi_map,
};

/**
 * mtk_pcie_enable_msi - Enable MSI support
 * @port: PCIe port information
 */
static void mtk_pcie_enable_msi(struct mtk_pcie_port *port)
{
	u32 mask;

	pcie_write(port, (u32)(u64)(port->base + MSI_VECTOR), IMSI_ADDR);
	mask = pcie_read(port, INT_MASK);
	mask &= ~MSI_MASK;
	pcie_write(port, mask, INT_MASK);
}

/**
 * mtk_pcie_add_bus - Add MSI chip info to PCIe bus
 * @bus: PCIe bus
 */
static void mtk_pcie_add_bus(struct pci_bus *bus)
{
	if (IS_ENABLED(CONFIG_PCI_MSI))
		bus->msi = &mtk_pcie_msi_chip;
}

/**
 * mtk_pcie_intx_map - Set the handler for the INTx and mark IRQ as valid
 * @domain: IRQ domain
 * @irq: Virtual IRQ number
 * @hwirq: HW interrupt number
 *
 * Return: Always returns 0.
 */
static int mtk_pcie_intx_map(struct irq_domain *domain, unsigned int irq,
				irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &dummy_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);

	return 0;
}

/* INTx IRQ Domain operations */
static const struct irq_domain_ops intx_domain_ops = {
	.map = mtk_pcie_intx_map,
};

/**
 * mtk_pcie_intr_handler - Interrupt Service Handler
 * @irq: IRQ number
 * @data: PCIe port information
 *
 * Return: IRQ_HANDLED on success and IRQ_NONE on failure
 */
static irqreturn_t mtk_pcie_intr_handler(int irq, void *data)
{
	struct mtk_pcie_port *port = (struct mtk_pcie_port *)data;
	u32 mask, status;
	int pos, msi, index = 0;
	unsigned long val;

	/* read interrupt decode and mask registers */
	status = pcie_read(port, INT_STATUS);
	mask = pcie_read(port, INT_MASK);

	if (!(~mask & status))
		return IRQ_NONE;

	if (status & INTX_MASK) {
		/* handle INTx Interrupt */
		val = ((status & INTX_MASK) >>
			MTK_PCIE_INTX_SHIFT);
		while (val) {
			val >>= 1;
			index++;
		}
		generic_handle_irq(irq_find_mapping(port->irq_domain, index));
	}
	if (status & MSI_STATUS) {
		val = pcie_read(port, IMSI_STATUS);
		while (val > 0) {
			pos = find_first_bit(&val, 32);
			msi = irq_find_mapping(port->msi_irq_domain, pos);
			/* write back to clear bit (w1c)*/
			pcie_write(port, 1 << pos, IMSI_STATUS);
			val &= ~(1 << pos);
			generic_handle_irq(msi);
		}
	}
	/* PCIe port uses the same irq for PME, AER and HP*/
	if ((status & PM_HP_EVENT_STATUS) ||
				(status & AER_EVENT_STATUS)) {
		msi = irq_find_mapping(port->msi_irq_domain, PCIE_PORT_MSI_BIT);
		generic_handle_irq(msi);
	}
	if (status & PCIE_L2_ENTRY_WAKE_STATUS) {
		/* de-assert pe reset */
		if (!(pcie_read(port, PCI_RSTCR) & PCI_PERSTB))
			pcie_write(port, pcie_read(port, PCI_RSTCR) |
				PCI_PERSTB, PCI_RSTCR);
		/* wake up system from L2 state */
		val = pcie_read(port, PCI_WAKE_CONTROL);
		pcie_write(port, val | CLKREQ_N_ENABLE, PCI_WAKE_CONTROL);
		pcie_write(port, val & ~CLKREQ_N_ENABLE, PCI_WAKE_CONTROL);
	}
	/* clear the Interrupt Decode register */
	pcie_write(port, status, INT_STATUS);

	return IRQ_HANDLED;
}

/* enable LTSSM for mt7622. */
static int mtk_pcie_enable_ltssm(struct mtk_pcie *pcie)
{
	unsigned int val;
	void __iomem *reg;
	int offset;
	static int initted;

	if (initted)
		return 0;

	/* enable LTSSM. */
	reg = pcie->base;
	if (IS_ERR(reg))
		return -EINVAL;
	offset = 0;
	val = readl(reg + offset);
	val = val | BIT(0) | BIT(8);
	writel(val, reg + offset);

	val = readl(reg + offset);
	val |= BIT(1) | BIT(9);
	writel(val, reg + offset);

	val = readl(reg + offset);
	val |= BIT(2) | BIT(10);
	writel(val, reg + offset);

	initted = 1;

	return 0;
}

static const struct of_device_id mtk_pcie_of_match[];

/**
 * mtk_pcie_init_hw - Initialize mtk PCIe host hardware
 * @port: PCIe port information
 * @res: Bus Resources
 *
 * Return: '0' on success and error value on failure
 */
static int mtk_pcie_init_hw(struct mtk_pcie_port *port)
{
	int size, count = 0;
	struct device *dev = port->pcie->dev;
	struct resource *pref = &port->pcie->pref_mem;
	struct resource *npref = &port->pcie->npref_mem;
	struct mtk_pcie *pcie = port->pcie;
	struct device_node *np = dev->of_node;
	const struct of_device_id *node;
	const struct mtk_pcie_plat *plat;

	mtk_pcie_port_enable(port);
	node = of_match_node(mtk_pcie_of_match, np);
	plat = node->data;
	if (plat->ltssm) {
		int ret;

		ret = mtk_pcie_enable_ltssm(pcie);
		if (ret) {
			dev_err(dev, "enable ltssm failed\n");
			return ret;
		}
	}

	/* assert all reset  signals */
	pcie_write(port, 0, PCI_RSTCR);

	/* enable rc internal reset.
	 * the reset will work when the link is from link up to link down.
	 */
	pcie_write(port, PCI_LINKDOWN_RST_EN, PCI_RSTCR);
	/* de-assert phy reset  */
	pcie_write(port, pcie_read(port, PCI_RSTCR) |
		PCI_PHY_RSTB, PCI_RSTCR);

	mdelay(100);
	/* de-assert pe reset */
	pcie_write(port, pcie_read(port, PCI_RSTCR) |
		PCI_PERSTB, PCI_RSTCR);
	/* de-assert pipe & mac & configuration reset */
	pcie_write(port, pcie_read(port, PCI_RSTCR) |
		PCI_PIPE_SRSTB | PCI_MAC_SRSTB | PCI_CRSTB, PCI_RSTCR);

	/* check if the link is up or not */
	while (!mtk_pcie_link_is_up(port)) {
		mdelay(1);
		count++;
		if (count >= 50)
			return -EINVAL;
	}

	dev_info(dev, "PCIe link suc\n");
/* MT2712 does not need this */
#ifndef CONFIG_MT2712_FPGA
/* ====================bringup only======================== */
	pcie_write(port, pcie_read(port, K_GBL_1) |
		MTK_ROOT_PORT, K_GBL_1);
	/* set IDs */
	pcie_write(port, K_CONF_IDS(MTK_VEND_ID,
		MTK_DEV_ID), K_CONF_FUNC0_0);
	pcie_write(port, ((PCI_CLASS_BRIDGE_PCI << 16) & 0xffffff00),
		K_CONF_FUNC0_1);
	pcie_write(port, K_CONF_IDS(MTK_SS_VEND_ID,
		MTK_SS_DEV_ID), K_CONF_FUNC0_2);
/* =================================================== */
#endif
	/* set INT mask */
	pcie_write(port, pcie_read(port, INT_MASK) & ~(INTX_MASK |
		PCIE_L2_ENTRY_WAKE_MASK), INT_MASK);
	/* set AHB to PCIe translation windows for prefetchable
	 * and non-prefetchable memory
	 */
	if (pref->flags & IORESOURCE_MEM) {
		size = pref->end - pref->start;
		pcie_write(port,
			(AHB2PCIE_BASEL(pref->start) |
			 NOPREFETCH(IORESOURCE_PREFETCH) |
			 BASE_SIZE(mtk_pcie_mapping_sz(size))),
			AHB2PCIE_BASE1_L);
		pcie_write(port, AHB2PCIE_BASEH(pref->start), AHB2PCIE_BASE1_H);
	}
	if (npref->flags & IORESOURCE_MEM) {
		size = npref->end - npref->start;
		pcie_write(port, (AHB2PCIE_BASEL(npref->start) |
			NOPREFETCH(0) | BASE_SIZE(mtk_pcie_mapping_sz(size))),
		AHB2PCIE_BASE0_L);
		pcie_write(port,
			AHB2PCIE_BASEH(npref->start),
			AHB2PCIE_BASE0_H);
	}
	/* set PCIe to axi translation memory space.*/
	pcie_write(port,
		mtk_pcie_mapping_sz(PCIE2AXI_SIZE) | WIN_ENABLE, PCIE2AXI_WIN);

	return 0;
}

/**
 * mtk_pcie_free_irq_domain - Free IRQ domain
 * @port: PCIe port information
 * @irq_domain: IRQ domain structure
 */
static void mtk_free_irq_domain(u32 num_irqs, struct irq_domain *irq_domain)
{
	int i;
	u32 irq;

	for (i = 0; i < num_irqs; i++) {
		irq = irq_find_mapping(irq_domain, i);
		if (irq > 0)
			irq_dispose_mapping(irq);
	}
	irq_domain_remove(irq_domain);
}

/**
 * mtk_pcie_free_irq_domain - Free IRQ domain
 * @port: PCIe port information
 */
static void mtk_pcie_free_irq_domain(struct mtk_pcie_port *port)
{
	/* free msi irq domain */
	if (IS_ENABLED(CONFIG_PCI_MSI))
		mtk_free_irq_domain(MAX_MSI_IRQS, port->msi_irq_domain);
	/* free int irq domain */
	mtk_free_irq_domain(INTX_IRQ_NUM, port->irq_domain);
}

/**
 * mtk_pcie_init_irq_domain - Initialize IRQ domain
 * @port: PCIe port information
 * @node: Device node
 *
 * Return: '0' on success and error value on failure
 */
static int mtk_pcie_init_irq_domain(struct mtk_pcie_port *port,
		struct device_node *node)
{
	struct device *dev = port->pcie->dev;
	struct device_node *pcie_intc_node;

	/* setup intx */
	pcie_intc_node = of_get_next_child(node, NULL);
	if (!pcie_intc_node) {
		dev_err(dev, "No PCIe Intc node found\n");
		return PTR_ERR(pcie_intc_node);
	}

	port->irq_domain = irq_domain_add_linear(pcie_intc_node, INTX_IRQ_NUM,
						 &intx_domain_ops,
						 port);
	if (!port->irq_domain) {
		dev_err(dev, "Failed to get a INTx IRQ domain\n");
		return PTR_ERR(port->irq_domain);
	}
	/* setup msi */
	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		port->msi_irq_domain = irq_domain_add_linear(node,
							 MAX_MSI_IRQS,
							 &msi_domain_ops,
							 &mtk_pcie_msi_chip);
		if (!port->msi_irq_domain) {
			dev_err(dev, "Failed to get a MSI IRQ domain\n");
			return PTR_ERR(port->msi_irq_domain);
		}
		mtk_pcie_enable_msi(port);
	}

	return 0;
}

/**
 * mtk_add_pcie_port - Scan PCIe bus to add PCIe port and device.
 * @pcie: PCIe host information
 * @pdev: Platform device pointer
 *
 * Return: '0' on success and error value on failure
 */
static int  mtk_add_pcie_port(struct mtk_pcie *pcie,
				   struct platform_device *pdev)
{
	struct pci_bus *bus, *child;

	bus = pci_scan_root_bus(&pdev->dev, 0, &mtk_pcie_ops,
				pcie, &pcie->resources);

	if (!bus)
		return -ENOMEM;

	mtk_pcie_add_bus(bus);
	pci_fixup_irqs(pci_common_swizzle, of_irq_parse_and_map_pci);
	if (!pci_has_flag(PCI_PROBE_ONLY)) {
		pci_bus_size_bridges(bus);
		pci_bus_assign_resources(bus);

		list_for_each_entry(child, &bus->children, node) {
			mtk_pcie_add_bus(child);
			pcie_bus_configure_settings(child);
		}
	}
	pci_bus_add_devices(bus);

	return 0;
}

/* Enable IO coherent for PCIe RC and device.*/
static int mtk_pcie_enable_ioc(struct platform_device *pdev)
{
	struct resource *regs;
	void __iomem *addr;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	addr = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(addr))
		return -EINVAL;
	writel(0x3, addr);

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	addr = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(addr))
		return -EINVAL;
	writel(0x3, addr + 0x8);

	return 0;
}

static int mtk_pcie_switch_xtal(struct platform_device *pdev)
{
	struct resource *regs;
	void __iomem *addr;
	u32 val;

	/* change setting for rc0 phy. */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 6);
	addr = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(addr))
		return -EINVAL;

	val = readl(addr + 0x200);
	val |= BIT(2);
	writel(val, addr + 0x200);

	val = readl(addr + 0xc3c);
	val &= 0xffff;
	val |= 0x380000;
	writel(val, addr + 0xc3c);

	val = readl(addr + 0xc38);
	val &= 0xffff;
	val |= 0x380000;
	writel(val, addr + 0xc38);

	val = readl(addr + 0xc48);
	val &= 0xffff0000;
	val |= 0x36;
	writel(val, addr + 0xc48);

	val = readl(addr + 0xc44);
	val &= 0xffff0000;
	val |= 0x36;
	writel(val, addr + 0xc44);

	val = readl(addr + 0xb14);
	val &= 0xffff0000;
	val |= 0x140;
	writel(val, addr + 0xb14);

	val = readl(addr + 0xb0c);
	val &= BIT(0);
	val |= 0x3e800000 << 1;
	writel(val, addr + 0xb0c);

	val = readl(addr + 0xb10);
	val |= BIT(24);
	writel(val, addr + 0xb10);

	val = readl(addr + 0xb04);
	val &= ~BIT(9);
	val |= BIT(8);
	writel(val, addr + 0xb04);

	val = readl(addr + 0x310);
	val |= BIT(17);
	writel(val, addr + 0x310);

	val = readl(addr + 0x200);
	val &= (~BIT(2));
	writel(val, addr + 0x200);

	/* change phy setting for rc1 */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 7);
	addr = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(addr))
		return -EINVAL;

	val = readl(addr + 0x30c);
	val |= BIT(31);
	writel(val, addr + 0x30c);

	val = readl(addr + 0xc3c);
	val &= 0xffff;
	val |= 0x380000;
	writel(val, addr + 0xc3c);

	val = readl(addr + 0xc38);
	val &= 0xffff;
	val |= 0x380000;
	writel(val, addr + 0xc38);

	val = readl(addr + 0xc48);
	val &= 0xffff0000;
	val |= 0x36;
	writel(val, addr + 0xc48);

	val = readl(addr + 0xc44);
	val &= 0xffff0000;
	val |= 0x36;
	writel(val, addr + 0xc44);

	val = readl(addr + 0xb14);
	val &= 0xffff0000;
	val |= 0x140;
	writel(val, addr + 0xb14);

	val = readl(addr + 0xb0c);
	val &= BIT(0);
	val |= 0x3e800000 << 1;
	writel(val, addr + 0xb0c);

	val = readl(addr + 0xb10);
	val |= BIT(24);
	writel(val, addr + 0xb10);

	val = readl(addr + 0xb04);
	val &= ~BIT(9);
	val |= BIT(8);
	writel(val, addr + 0xb04);

	val = readl(addr + 0x310);
	val |= BIT(17);
	writel(val, addr + 0x310);

	val = readl(addr + 0x30c);
	val &= (~BIT(31));
	writel(val, addr + 0x30c);
	return 0;

}

/**
 * mtk_pcie_parse_and_add_res - Add resources by parsing ranges
 * @pcie: PCIe host information
 *
 * Return: '0' on success and error value on failure
 */
static int mtk_pcie_parse_and_add_res(struct mtk_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct platform_device *pdev = to_platform_device(pcie->dev);
	struct device_node *node = dev->of_node, *child;
	const struct of_device_id *np;
	struct resource *mem;
	resource_size_t offset;
	struct of_pci_range_parser parser;
	struct of_pci_range range;
	struct resource_entry *win;
	struct phy *phy;
	char name[10];
	const struct mtk_pcie_plat *plat;
	int err = 0, i;

	/* enable pcie LTSSM. */
	np = of_match_node(mtk_pcie_of_match, node);
	plat = np->data;
	if (plat->ltssm) {
		pcie->base = mtk_pcie_map_registers(pdev, 2);
		if (IS_ERR(pcie->base)) {
			dev_err(dev, "miss pcie base\n");
			return -EINVAL;
		}
	}

	if (plat->io_coherent && of_dma_is_coherent(node)) {
		err = mtk_pcie_enable_ioc(pdev);
		if (err) {
			dev_err(dev, "enable ioc failed\n");
			return err;
		}
	}

	if (plat->xtal) {
		bool found = false;
		struct device_node *pnode = of_node_get(node);

		while (pnode) {
			if (of_property_read_bool(pnode, "change-xtal")) {
				found = true;
				break;
			}
			pnode = of_get_next_parent(pnode);
		}
		of_node_put(pnode);

		if (found)
			mtk_pcie_switch_xtal(pdev);
	}

	/* get the ranges */
	if (of_pci_range_parser_init(&parser, node)) {
		dev_err(dev, "missing \"ranges\" property\n");
		return -EINVAL;
	}
	/* parse the ranges and add the resources found to the list */
	for_each_of_pci_range(&parser, &range) {

		mem = devm_kmalloc(dev, sizeof(*mem), GFP_KERNEL);
		if (!mem) {
			err = -ENOMEM;
			goto free_resources;
		}

		of_pci_range_to_resource(&range, node, mem);

		switch (mem->flags & IORESOURCE_TYPE_BITS) {
		case IORESOURCE_MEM:
			offset = range.cpu_addr - range.pci_addr;
			if (mem->flags & IORESOURCE_PREFETCH) {
				mem->name = "prefetchable memory";
				memcpy(&pcie->pref_mem, mem, sizeof(*mem));
			} else {
				mem->name = "non-prefetchable memory";
				memcpy(&pcie->npref_mem, mem, sizeof(*mem));
			}

			break;
		default:
			err = -EINVAL;
			break;
		}

		if (err < 0) {
			dev_warn(dev, "Invalid resource found %pR\n", mem);
			continue;
		}
		err = request_resource(&iomem_resource, mem);
		if (err)
			goto free_resources;

		pci_add_resource_offset(&pcie->resources, mem, offset);
	}
	/* get the bus range */
	err = of_pci_parse_bus_range(node, &pcie->bus_range);
	if (err < 0) {
		dev_err(dev, "failed to parse ranges property: %d\n",
			err);
		pcie->bus_range = (struct resource) {
			.name	= node->name,
			.start	= 0,
			.end	= 0xff,
			.flags	= IORESOURCE_BUS,
		};
	}
	/* register bus resource */
	pci_add_resource(&pcie->resources, &pcie->bus_range);

	/* add root ports*/
	i = 0;
	for_each_child_of_node(node, child) {
		if (!of_device_is_available(child))
			continue;
		i++;
	}
	pcie->ports = devm_kzalloc(dev, i *
				   sizeof(struct mtk_pcie_port),
				   GFP_KERNEL);
	if (!pcie->ports)
		return -ENOMEM;

	i = 0;
	for_each_child_of_node(node, child) {
		struct mtk_pcie_port *port = &pcie->ports[i];
		struct platform_device *plat_dev;

		if (!of_device_is_available(child))
			continue;

		port->pcie = pcie;
		if (of_property_read_u32(child, "pcie-port",
					 &port->port)) {
			dev_warn(dev,
				 "ignoring PCIe DT node, missing pcie-port property\n");
			continue;
		}

		if (of_property_read_u32(child, "num-lanes",
						 &port->lane)) {
			dev_warn(dev,
				"ignoring PCIe DT node, missing num-lanes property\n");
			continue;
		}
		port->name = kasprintf(GFP_KERNEL, "pcie%d.%d",
				       port->port, port->lane);
		port->devfn = of_pci_get_devfn(child);
		if (port->devfn < 0)
			continue;
		port->base = mtk_pcie_map_registers(pdev, port->port);
		if (IS_ERR(port->base)) {
			dev_err(dev, "PCIe%d.%d: cannot map registers\n",
				port->port, port->lane);
			port->base = NULL;
			continue;
		}
		/* request interrupt */
		port->irq = platform_get_irq(pdev, port->port);
		if (!port->irq) {
			dev_err(dev, "failed to get irq\n");
			return -ENODEV;
		}
		err = request_irq(port->irq, mtk_pcie_intr_handler,
				IRQF_SHARED, "PCIe", port);
		if (err) {
			dev_err(dev, "failed to register IRQ: %d\n", err);
			return -ENODEV;
		}
		/* initialize pcie phy */
		snprintf(name, sizeof(name), "pcie-phy%d", i);
		phy = devm_of_phy_get(dev, child, name);
		if (!IS_ERR(phy)) {
			port->phy = phy;
			err = phy_init(phy);
			if (err) {
				dev_err(dev, "%s: failed to initialize phy\n",
					name);
				return -ENODEV;
			}
			err = phy_power_on(phy);
			if (err) {
				dev_err(dev, "%s: failed to power up\n", name);
				return -ENODEV;
			}
		}

		plat_dev = of_find_device_by_node(child);
		if (!plat_dev) {
			plat_dev = of_platform_device_create(
						child, NULL,
						platform_bus_type.dev_root);
			if (!plat_dev)
				return -EPROBE_DEFER;
		}
		port->dev = &plat_dev->dev;
		mtk_pcie_clk_get(port);
		/* initialize pcie rc controller */
		err = mtk_pcie_init_hw(port);
		if (err) {
			phy_power_off(port->phy);
			dev_err(dev, "failed to init pcie rc %d h/w, check the connection\n", port->port);
			continue;
		}
		err = mtk_pcie_init_irq_domain(port, child);
		if (err) {
			dev_err(dev, "failed to init pcie h/w\n");
			return -ENODEV;
		}
		pcie->nports = ++i;
	}

	return 0;

free_resources:
	release_child_resources(&iomem_resource);
	list_for_each_entry(win, &pcie->resources, node)
		devm_kfree(dev, win->res);
	pci_free_resource_list(&pcie->resources);

	return err;
}

static const struct mtk_pcie_plat mt2712_pcie = {
	.ltssm = false,
	.io_coherent = false,
	.xtal = false,
};

static const struct mtk_pcie_plat mt7622_pcie = {
	.ltssm = true,
	.io_coherent = true,
	.xtal = true,
};

static const struct of_device_id mtk_pcie_of_match[] = {
	{ .compatible = "mediatek,pcie-mt2712", .data = &mt2712_pcie },
	{ .compatible = "mediatek,pcie-mt7622", .data = &mt7622_pcie },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_pcie_of_match);
/**
 * mtk_pcie_probe - Probe function
 * @pdev: Platform device pointer
 *
 * Return: '0' on success and error value on failure
 */
static int mtk_pcie_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct mtk_pcie *pcie;
	int err;

	match = of_match_device(mtk_pcie_of_match, &pdev->dev);
	if (!match)
		return -ENODEV;

	pcie = devm_kzalloc(&pdev->dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pcie->dev = &pdev->dev;

	/* mt2712 does not enable power domain for now.*/
	if (pcie->dev->pm_domain) {
		/* enable pcie power domain for mt7622. */
		pm_runtime_enable(pcie->dev);
		pm_runtime_get_sync(pcie->dev);
	}

	/*
	 * parse PCI ranges, configuration bus range and
	 * request their resources
	 */
	INIT_LIST_HEAD(&pcie->resources);
	err = mtk_pcie_parse_and_add_res(pcie);
	if (err) {
		dev_err(&pdev->dev, "failed to parse and add resources: %d\n",
			err);
		return err;
	}
	platform_set_drvdata(pdev, pcie);

	/* register the device */
	err = mtk_add_pcie_port(pcie, pdev);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to add pcie port: %d\n", err);
		return err;
	}

	dev_err(&pdev->dev, "pcie probe done\n");
	return 0;
}

/**
 * mtk_pcie_remove - Remove function
 * @pdev: Platform device pointer
 *
 * Return: '0' always
 */
static int mtk_pcie_remove(struct platform_device *pdev)
{
	struct mtk_pcie *pcie = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < pcie->nports; i++) {
		struct mtk_pcie_port *port = &pcie->ports[i];

		mtk_pcie_free_irq_domain(port);
	}

	return 0;
}

static int __maybe_unused mtk_pcie_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev;
	struct mtk_pcie *pcie;
	struct mtk_pcie_port *port;
	struct mtk_pcie_suspend_resume_reg *reg;
	int i;

	pdev = to_platform_device(dev);
	pcie = platform_get_drvdata(pdev);

	for (i = 0; i < pcie->nports; i++) {
		port = &pcie->ports[i];
		reg = &port->reg;

		reg->linkup = mtk_pcie_link_is_up(port);
		if (reg->linkup) {
			if (!IS_ERR(port->ahb))
				clk_disable_unprepare(port->ahb);
			if (!IS_ERR(port->mac))
				clk_disable_unprepare(port->mac);
			phy_power_off(port->phy);
		}
	}

	return 0;
}

static int __maybe_unused mtk_pcie_resume_noirq(struct device *dev)
{
	struct platform_device *pdev;
	struct mtk_pcie *pcie;
	struct mtk_pcie_port *port;
	struct mtk_pcie_suspend_resume_reg *reg;
	int i;

	pdev = to_platform_device(dev);
	pcie = platform_get_drvdata(pdev);

	for (i = 0; i < pcie->nports; i++) {
		port = &pcie->ports[i];
		reg = &port->reg;

		if (reg->linkup) {
			phy_power_on(port->phy);
			mtk_pcie_init_hw(port);
			if (IS_ENABLED(CONFIG_PCI_MSI))
				mtk_pcie_enable_msi(port);
		}
	}

	return 0;
}


const struct dev_pm_ops mtk_pcie_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_pcie_suspend_noirq,
				      mtk_pcie_resume_noirq)
};

static struct platform_driver mtk_pcie_driver = {
	.driver = {
		.name = "mtk-pcie",
		.owner = THIS_MODULE,
		.of_match_table = mtk_pcie_of_match,
		.pm = &mtk_pcie_pm_ops,
	},
	.probe = mtk_pcie_probe,
	.remove = mtk_pcie_remove,
};

builtin_platform_driver(mtk_pcie_driver);

MODULE_AUTHOR("Liang-Yen Wang <liang-yen.wang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek PCIe host controller driver");
MODULE_LICENSE("GPL v2");
