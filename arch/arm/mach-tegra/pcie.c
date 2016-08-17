/*
 * arch/arm/mach-tegra/pcie.c
 *
 * PCIe host controller driver for TEGRA SOCs
 *
 * Copyright (c) 2010, CompuLab, Ltd.
 * Author: Mike Rapoport <mike@compulab.co.il>
 *
 * Based on NVIDIA PCIe driver
 * Copyright (c) 2008-2013, NVIDIA Corporation.
 *
 * Bits taken from arch/arm/mach-dove/pcie.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/msi.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/async.h>

#include <asm/sizes.h>
#include <asm/mach/pci.h>

#include <mach/iomap.h>
#include <mach/clk.h>
#include <mach/powergate.h>
#include <mach/pci.h>

#include "board.h"

#define MSELECT_CONFIG_0_ENABLE_PCIE_APERTURE				5

/* register definitions */
#define AFI_OFFSET							0x3800
#define PADS_OFFSET							0x3000
#define RP_OFFSET							0x1000

#define AFI_AXI_BAR0_SZ							0x00
#define AFI_AXI_BAR1_SZ							0x04
#define AFI_AXI_BAR2_SZ							0x08
#define AFI_AXI_BAR3_SZ							0x0c
#define AFI_AXI_BAR4_SZ							0x10
#define AFI_AXI_BAR5_SZ							0x14

#define AFI_AXI_BAR0_START						0x18
#define AFI_AXI_BAR1_START						0x1c
#define AFI_AXI_BAR2_START						0x20
#define AFI_AXI_BAR3_START						0x24
#define AFI_AXI_BAR4_START						0x28
#define AFI_AXI_BAR5_START						0x2c

#define AFI_FPCI_BAR0							0x30
#define AFI_FPCI_BAR1							0x34
#define AFI_FPCI_BAR2							0x38
#define AFI_FPCI_BAR3							0x3c
#define AFI_FPCI_BAR4							0x40
#define AFI_FPCI_BAR5							0x44

#define AFI_CACHE_BAR0_SZ						0x48
#define AFI_CACHE_BAR0_ST						0x4c
#define AFI_CACHE_BAR1_SZ						0x50
#define AFI_CACHE_BAR1_ST						0x54

#define AFI_MSI_BAR_SZ							0x60
#define AFI_MSI_FPCI_BAR_ST						0x64
#define AFI_MSI_AXI_BAR_ST						0x68

#define AFI_MSI_VEC0_0							0x6c
#define AFI_MSI_VEC1_0							0x70
#define AFI_MSI_VEC2_0							0x74
#define AFI_MSI_VEC3_0							0x78
#define AFI_MSI_VEC4_0							0x7c
#define AFI_MSI_VEC5_0							0x80
#define AFI_MSI_VEC6_0							0x84
#define AFI_MSI_VEC7_0							0x88

#define AFI_MSI_EN_VEC0_0						0x8c
#define AFI_MSI_EN_VEC1_0						0x90
#define AFI_MSI_EN_VEC2_0						0x94
#define AFI_MSI_EN_VEC3_0						0x98
#define AFI_MSI_EN_VEC4_0						0x9c
#define AFI_MSI_EN_VEC5_0						0xa0
#define AFI_MSI_EN_VEC6_0						0xa4
#define AFI_MSI_EN_VEC7_0						0xa8

#define AFI_CONFIGURATION						0xac
#define AFI_CONFIGURATION_EN_FPCI				(1 << 0)
#define AFI_CONFIGURATION_DFPCI_RSPPASSPW			(1 << 2)

#define AFI_FPCI_ERROR_MASKS						0xb0

#define AFI_INTR_MASK							0xb4
#define AFI_INTR_MASK_INT_MASK					(1 << 0)
#define AFI_INTR_MASK_MSI_MASK					(1 << 8)

#define AFI_INTR_CODE							0xb8
#define AFI_INTR_CODE_MASK						0xf
#define AFI_INTR_MASTER_ABORT						4
#define AFI_INTR_LEGACY						6

#define AFI_INTR_SIGNATURE						0xbc
#define AFI_SM_INTR_ENABLE						0xc4

#define AFI_AFI_INTR_ENABLE						0xc8
#define AFI_INTR_EN_INI_SLVERR						(1 << 0)
#define AFI_INTR_EN_INI_DECERR						(1 << 1)
#define AFI_INTR_EN_TGT_SLVERR						(1 << 2)
#define AFI_INTR_EN_TGT_DECERR						(1 << 3)
#define AFI_INTR_EN_TGT_WRERR						(1 << 4)
#define AFI_INTR_EN_DFPCI_DECERR					(1 << 5)
#define AFI_INTR_EN_AXI_DECERR						(1 << 6)
#define AFI_INTR_EN_FPCI_TIMEOUT					(1 << 7)
#define AFI_INTR_EN_PRSNT_SENSE					(1 << 8)

#define AFI_PCIE_CONFIG							0x0f8
#define AFI_PCIE_CONFIG_PCIEC0_DISABLE_DEVICE			(1 << 1)
#define AFI_PCIE_CONFIG_PCIEC1_DISABLE_DEVICE			(1 << 2)
#define AFI_PCIE_CONFIG_PCIEC2_DISABLE_DEVICE			(1 << 3)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK		(0xf << 20)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_SINGLE		(0x0 << 20)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_DUAL		(0x1 << 20)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_411			(0x2 << 20)

#define AFI_FUSE							0x104
#define AFI_FUSE_PCIE_T0_GEN2_DIS				(1 << 2)

#define AFI_PEX0_CTRL							0x110
#define AFI_PEX1_CTRL							0x118
#define AFI_PEX2_CTRL							0x128
#define AFI_PEX_CTRL_RST					(1 << 0)
#define AFI_PEX_CTRL_CLKREQ_EN					(1 << 1)
#define AFI_PEX_CTRL_REFCLK_EN					(1 << 3)

#define AFI_PEXBIAS_CTRL_0					0x168

#define RP_VEND_XP						0x00000F00
#define RP_VEND_XP_DL_UP					(1 << 30)

#define  RP_TXBA1						0x00000E1C
#define  RP_TXBA1_CM_OVER_PW_BURST_MASK			(0xF << 4)
#define  RP_TXBA1_CM_OVER_PW_BURST_INIT_VAL			(0x4 << 4)
#define  RP_TXBA1_PW_OVER_CM_BURST_MASK			(0xF)
#define  RP_TXBA1_PW_OVER_CM_BURST_INIT_VAL			(0x4)

#define RP_LINK_CONTROL_STATUS					0x00000090
#define RP_LINK_CONTROL_STATUS_LINKSTAT_MASK			0x3fff0000

#define PADS_CTL_SEL						0x0000009C

#define PADS_CTL						0x000000A0
#define PADS_CTL_IDDQ_1L					(1 << 0)
#define PADS_CTL_TX_DATA_EN_1L					(1 << 6)
#define PADS_CTL_RX_DATA_EN_1L					(1 << 10)

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define  PADS_PLL_CTL						0x000000B8
#else
#define  PADS_PLL_CTL						0x000000B4
#endif
#define  PADS_PLL_CTL_RST_B4SM					(1 << 1)
#define  PADS_PLL_CTL_LOCKDET					(1 << 8)
#define  PADS_PLL_CTL_REFCLK_MASK				(0x3 << 16)
#define  PADS_PLL_CTL_REFCLK_INTERNAL_CML			(0 << 16)
#define  PADS_PLL_CTL_REFCLK_INTERNAL_CMOS			(1 << 16)
#define  PADS_PLL_CTL_REFCLK_EXTERNAL				(2 << 16)
#define  PADS_PLL_CTL_TXCLKREF_MASK				(0x1 << 20)
#define  PADS_PLL_CTL_TXCLKREF_BUF_EN				(1 << 22)
#define  PADS_PLL_CTL_TXCLKREF_DIV10				(0 << 20)
#define  PADS_PLL_CTL_TXCLKREF_DIV5				(1 << 20)

#define  PADS_REFCLK_CFG1					0x000000CC

/* PMC access is required for PCIE xclk (un)clamping */
#define PMC_SCRATCH42						0x144
#define PMC_SCRATCH42_PCX_CLAMP				(1 << 0)

#define NV_PCIE2_RP_PRIV_MISC					0x00000FE0
#define PCIE2_RP_PRIV_MISC_CTLR_CLK_CLAMP_ENABLE		1 << 23
#define PCIE2_RP_PRIV_MISC_TMS_CLK_CLAMP_ENABLE		1 << 31

#define NV_PCIE2_RP_VEND_XP1					0x00000F04
#define NV_PCIE2_RP_VEND_XP1_LINK_PVT_CTL_L1_ASPM_SUPPORT_ENABLE	1 << 21

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
/*
 * Tegra2 defines 1GB in the AXI address map for PCIe.
 *
 * That address space is split into different regions, with sizes and
 * offsets as follows:
 *
 * 0x80000000 - 0x80003fff - PCI controller registers
 * 0x80004000 - 0x80103fff - PCI configuration space
 * 0x80104000 - 0x80203fff - PCI extended configuration space
 * 0x80203fff - 0x803fffff - unused
 * 0x80400000 - 0x8040ffff - downstream IO
 * 0x80410000 - 0x8fffffff - unused
 * 0x90000000 - 0x9fffffff - non-prefetchable memory
 * 0xa0000000 - 0xbfffffff - prefetchable memory
 */
#define TEGRA_PCIE_BASE		0x80000000

#define PCIE_REGS_SZ		SZ_16K
#define PCIE_CFG_OFF		PCIE_REGS_SZ
#define PCIE_CFG_SZ		SZ_1M
#define PCIE_EXT_CFG_OFF	(PCIE_CFG_SZ + PCIE_CFG_OFF)
#define PCIE_EXT_CFG_SZ		SZ_1M
#define PCIE_IOMAP_SZ		(PCIE_REGS_SZ + PCIE_CFG_SZ + PCIE_EXT_CFG_SZ)

#define MMIO_BASE		(TEGRA_PCIE_BASE + SZ_4M)
#define MMIO_SIZE		SZ_64K
#define MEM_BASE_0		(TEGRA_PCIE_BASE + SZ_256M)
#define MEM_SIZE_0		SZ_256M
#define PREFETCH_MEM_BASE_0	(MEM_BASE_0 + MEM_SIZE_0)
#define PREFETCH_MEM_SIZE_0	SZ_512M

#else

/*
 * AXI address map for the PCIe aperture , defines 1GB in the AXI
 *  address map for PCIe.
 *
 *  That address space is split into different regions, with sizes and
 *  offsets as follows. Exepct for the Register space, SW is free to slice the
 *  regions as it chooces.
 *
 *  The split below seems to work fine for now.
 *
 *  0x0000_0000 to 0x00ff_ffff - Register space          16MB.
 *  0x0100_0000 to 0x01ff_ffff - Config space            16MB.
 *  0x0200_0000 to 0x02ff_ffff - Extended config space   16MB.
 *  0x0300_0000 to 0x03ff_ffff - Downstream IO space
 *   ... Will be filled with other BARS like MSI/upstream IO etc.
 *  0x1000_0000 to 0x1fff_ffff - non-prefetchable memory aperture
 *  0x2000_0000 to 0x3fff_ffff - Prefetchable memory aperture
 *
 *  Config and Extended config sizes are choosen to support
 *  maximum of 256 devices,
 *  which is good enough for all the current use cases.
 *
 */
#define TEGRA_PCIE_BASE	0x00000000

#define PCIE_REGS_SZ		SZ_16M
#define PCIE_CFG_OFF		PCIE_REGS_SZ
#define PCIE_CFG_SZ		SZ_16M
#define PCIE_EXT_CFG_OFF	(PCIE_CFG_SZ + PCIE_CFG_OFF)
#define PCIE_EXT_CFG_SZ		SZ_16M
/* During the boot only registers/config and extended config apertures are
 * mapped. Rest are mapped on demand by the PCI device drivers.
 */
#define PCIE_IOMAP_SZ		(PCIE_REGS_SZ + PCIE_CFG_SZ + PCIE_EXT_CFG_SZ)

#define MMIO_BASE				(TEGRA_PCIE_BASE + PCIE_IOMAP_SZ)
#define MMIO_SIZE							SZ_1M
#define MEM_BASE_0				(TEGRA_PCIE_BASE + SZ_256M)
#define MEM_SIZE_0		SZ_256M
#define PREFETCH_MEM_BASE_0	(MEM_BASE_0 + MEM_SIZE_0)
#define PREFETCH_MEM_SIZE_0	SZ_512M
#endif

#define  PCIE_CONF_BUS(b)					((b) << 16)
#define  PCIE_CONF_DEV(d)					((d) << 11)
#define  PCIE_CONF_FUNC(f)					((f) << 8)
#define  PCIE_CONF_REG(r)	\
	(((r) & ~0x3) | (((r) < 256) ? PCIE_CFG_OFF : PCIE_EXT_CFG_OFF))

#define PCIE_CTRL_REGS		7
#define COMBINE_PCIE_PCIX_SPACE	2

struct tegra_pcie_port {
	int			index;
	u8			root_bus_nr;
	void __iomem		*base;

	bool			link_up;

	char			io_space_name[16];
	char			mem_space_name[16];
	char			prefetch_space_name[20];
	struct resource		res[3];
	struct pci_bus*		bus;
};

struct tegra_pcie_info {
	struct tegra_pcie_port	port[MAX_PCIE_SUPPORTED_PORTS];
	int			num_ports;

	void __iomem		*reg_clk_base;
	void __iomem		*regs;
	struct resource		*res_mmio;
	int			power_rails_enabled;
	int			pcie_power_enabled;
	struct work_struct 	hotplug_detect;

	struct regulator	*regulator_hvdd;
	struct regulator	*regulator_pexio;
	struct regulator	*regulator_avdd_plle;
	struct clk		*pcie_xclk;
	struct clk		*pll_e;
	struct device		*dev;
	struct tegra_pci_platform_data *plat_data;
}tegra_pcie;

struct resource tegra_pcie_res_mmio = {
	.name = "PCI IO",
	.start = MMIO_BASE,
	.end = MMIO_BASE + MMIO_SIZE - 1,
	.flags = IORESOURCE_MEM,
};

static struct resource pcie_io_space;
static struct resource pcie_mem_space;
static struct resource pcie_prefetch_mem_space;
/* disable read write while noirq operation
 * is performed since pcie is powered off */
static bool is_pcie_noirq_op = false;
/* enable and init msi once during boot or resume */
static bool msi_enable;
/* this flag is used for enumeration by hotplug */
/* when dock is not connected while system boot */
static bool is_dock_conn_at_boot = true;

void __iomem *tegra_pcie_io_base;
EXPORT_SYMBOL(tegra_pcie_io_base);

static inline void afi_writel(u32 value, unsigned long offset)
{
	writel(value, offset + AFI_OFFSET + tegra_pcie.regs);
}

static inline u32 afi_readl(unsigned long offset)
{
	return readl(offset + AFI_OFFSET + tegra_pcie.regs);
}

static inline void pads_writel(u32 value, unsigned long offset)
{
	writel(value, offset + PADS_OFFSET + tegra_pcie.regs);
}

static inline u32 pads_readl(unsigned long offset)
{
	return readl(offset + PADS_OFFSET + tegra_pcie.regs);
}

static inline void rp_writel(u32 value, unsigned long offset, int rp)
{
	BUG_ON(rp != 0 && rp != 1 && rp != 2);
	offset += rp * (0x1UL << (rp - 1)) * RP_OFFSET;
	writel(value, offset + tegra_pcie.regs);
}

static inline unsigned int rp_readl(unsigned long offset, int rp)
{
	BUG_ON(rp != 0 && rp != 1 && rp != 2);
	offset += rp * (0x1UL << (rp - 1)) * RP_OFFSET;
	return readl(offset + tegra_pcie.regs);
}

static struct tegra_pcie_port *bus_to_port(int bus)
{
	int i;

	for (i = tegra_pcie.num_ports - 1; i >= 0; i--) {
		int rbus = tegra_pcie.port[i].root_bus_nr;
		if (rbus != -1 && rbus == bus)
			break;
	}

	return i >= 0 ? tegra_pcie.port + i : NULL;
}

static int tegra_pcie_read_conf(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	struct tegra_pcie_port *pp = bus_to_port(bus->number);
	void __iomem *addr;

	/* read reg is disabled without intr to avoid hang in suspend noirq */
	if (is_pcie_noirq_op)
		return 0;

	if (pp) {
		if (devfn != 0) {
			*val = 0xffffffff;
			return PCIBIOS_DEVICE_NOT_FOUND;
		}

		addr = pp->base + (where & ~0x3);
	} else {
		addr = tegra_pcie.regs + (PCIE_CONF_BUS(bus->number) +
					  PCIE_CONF_DEV(PCI_SLOT(devfn)) +
					  PCIE_CONF_FUNC(PCI_FUNC(devfn)) +
					  PCIE_CONF_REG(where));
	}

	*val = readl(addr);

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int tegra_pcie_write_conf(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	struct tegra_pcie_port *pp = bus_to_port(bus->number);
	void __iomem *addr;

	u32 mask;
	u32 tmp;

	/* write reg is disabled without intr to avoid hang in resume noirq */
	if (is_pcie_noirq_op)
		return 0;
	/* pcie core is supposed to enable bus mastering and io/mem responses
	 * if its not setting then enable corresponding bits in pci_command
	 */
	if (where == PCI_COMMAND) {
		if (!(val & PCI_COMMAND_IO))
			val |= PCI_COMMAND_IO;
		if (!(val & PCI_COMMAND_MEMORY))
			val |= PCI_COMMAND_MEMORY;
		if (!(val & PCI_COMMAND_MASTER))
			val |= PCI_COMMAND_MASTER;
		if (!(val & PCI_COMMAND_SERR))
			val |= PCI_COMMAND_SERR;
	}

	if (pp) {
		if (devfn != 0)
			return PCIBIOS_DEVICE_NOT_FOUND;

		addr = pp->base + (where & ~0x3);
	} else {
		addr = tegra_pcie.regs + (PCIE_CONF_BUS(bus->number) +
					  PCIE_CONF_DEV(PCI_SLOT(devfn)) +
					  PCIE_CONF_FUNC(PCI_FUNC(devfn)) +
					  PCIE_CONF_REG(where));
	}

	if (size == 4) {
		writel(val, addr);
		return PCIBIOS_SUCCESSFUL;
	}

	if (size == 2)
		mask = ~(0xffff << ((where & 0x3) * 8));
	else if (size == 1)
		mask = ~(0xff << ((where & 0x3) * 8));
	else
		return PCIBIOS_BAD_REGISTER_NUMBER;

	tmp = readl(addr) & mask;
	tmp |= val << ((where & 0x3) * 8);
	writel(tmp, addr);

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops tegra_pcie_ops = {
	.read	= tegra_pcie_read_conf,
	.write	= tegra_pcie_write_conf,
};

static void __devinit tegra_pcie_fixup_bridge(struct pci_dev *dev)
{
	u16 reg;

	if ((dev->class >> 16) == PCI_BASE_CLASS_BRIDGE) {
		pci_read_config_word(dev, PCI_COMMAND, &reg);
		reg |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
			PCI_COMMAND_MASTER | PCI_COMMAND_SERR);
		pci_write_config_word(dev, PCI_COMMAND, reg);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, tegra_pcie_fixup_bridge);

/* Tegra PCIE root complex wrongly reports device class */
static void __devinit tegra_pcie_fixup_class(struct pci_dev *dev)
{
	dev->class = PCI_CLASS_BRIDGE_PCI << 8;
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0bf0, tegra_pcie_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0bf1, tegra_pcie_fixup_class);
#else
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1c, tegra_pcie_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1d, tegra_pcie_fixup_class);
#endif

/* Tegra PCIE requires relaxed ordering */
static void __devinit tegra_pcie_relax_enable(struct pci_dev *dev)
{
	u16 val16;
	int pos = pci_find_capability(dev, PCI_CAP_ID_EXP);

	if (pos <= 0) {
		dev_err(&dev->dev, "skipping relaxed ordering fixup\n");
		return;
	}

	pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &val16);
	val16 |= PCI_EXP_DEVCTL_RELAX_EN;
	pci_write_config_word(dev, pos + PCI_EXP_DEVCTL, val16);
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, tegra_pcie_relax_enable);

static void tegra_pcie_preinit(void)
{
	pcie_io_space.name = "PCIe I/O Space";
	pcie_io_space.start = PCIBIOS_MIN_IO;
	pcie_io_space.end = IO_SPACE_LIMIT;
	pcie_io_space.flags = IORESOURCE_IO;
	if (request_resource(&ioport_resource, &pcie_io_space))
		panic("can't allocate PCIe I/O space");

	pcie_mem_space.name = "PCIe MEM Space";
	pcie_mem_space.start = MEM_BASE_0;
	pcie_mem_space.end = MEM_BASE_0 + MEM_SIZE_0 - 1;
	pcie_mem_space.flags = IORESOURCE_MEM;
	if (request_resource(&iomem_resource, &pcie_mem_space))
		panic("can't allocate PCIe MEM space");

	pcie_prefetch_mem_space.name = "PCIe PREFETCH MEM Space";
	pcie_prefetch_mem_space.start = PREFETCH_MEM_BASE_0;
	pcie_prefetch_mem_space.end = PREFETCH_MEM_BASE_0 + PREFETCH_MEM_SIZE_0 - 1;
	pcie_prefetch_mem_space.flags = IORESOURCE_MEM | IORESOURCE_PREFETCH;
	if (request_resource(&iomem_resource, &pcie_prefetch_mem_space))
		panic("can't allocate PCIe PREFETCH MEM space");

}

static int tegra_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct tegra_pcie_port *pp;

	if (nr >= tegra_pcie.num_ports)
		return 0;

	pp = tegra_pcie.port + nr;
	pp->root_bus_nr = sys->busnr;

	pci_add_resource_offset(&sys->resources, &pcie_io_space, sys->io_offset);
	pci_add_resource_offset(&sys->resources, &pcie_mem_space, sys->mem_offset);
	pci_add_resource_offset(&sys->resources, &pcie_prefetch_mem_space, sys->mem_offset);

	return 1;
}

static int tegra_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return INT_PCIE_INTR;
}

static struct pci_bus *tegra_pcie_scan_bus(int nr,
						  struct pci_sys_data *sys)
{
	struct tegra_pcie_port *pp;

	if (nr >= tegra_pcie.num_ports)
		return NULL;

	pp = tegra_pcie.port + nr;
	pp->root_bus_nr = sys->busnr;

	return pci_scan_root_bus(NULL, sys->busnr, &tegra_pcie_ops, sys,
				 &sys->resources);
}

static struct hw_pci tegra_pcie_hw = {
	.nr_controllers	= MAX_PCIE_SUPPORTED_PORTS,
	.preinit	= tegra_pcie_preinit,
	.setup		= tegra_pcie_setup,
	.scan		= tegra_pcie_scan_bus,
	.swizzle	= pci_std_swizzle,
	.map_irq	= tegra_pcie_map_irq,
};

#ifdef CONFIG_PM
static int tegra_pcie_suspend(struct device *dev);
static int tegra_pcie_resume(struct device *dev);

/* It enumerates the devices when dock is connected after system boot */
/* this is similar to pcibios_init_hw in bios32.c */
static void tegra_pcie_hotplug_init(void)
{
	struct pci_sys_data *sys = NULL;
	int ret, nr;

	if (is_dock_conn_at_boot)
		return;

	tegra_pcie_preinit();
	for (nr = 0; nr < tegra_pcie_hw.nr_controllers; nr++) {
		sys = kzalloc(sizeof(struct pci_sys_data), GFP_KERNEL);
		if (!sys)
			panic("PCI: unable to allocate sys data!");

#ifdef CONFIG_PCI_DOMAINS
		sys->domain  = tegra_pcie_hw.domain;
#endif
		sys->hw      = &tegra_pcie_hw;
		sys->busnr   = nr;
		sys->swizzle = tegra_pcie_hw.swizzle;
		sys->map_irq = tegra_pcie_hw.map_irq;
		INIT_LIST_HEAD(&sys->resources);

		ret = tegra_pcie_setup(nr, sys);
		if (ret > 0) {
			if (list_empty(&sys->resources)) {
				pci_add_resource_offset(&sys->resources,
					 &ioport_resource, sys->io_offset);
				pci_add_resource_offset(&sys->resources,
					 &iomem_resource, sys->mem_offset);
			}
			pci_create_root_bus(NULL, nr, &tegra_pcie_ops, sys, &sys->resources);
		}
	}
	is_dock_conn_at_boot = true;
}
#endif

static void tegra_pcie_attach(void)
{
#ifdef CONFIG_PM
	tegra_pcie_resume(NULL);
#endif
}

static void tegra_pcie_detach(void)
{
#ifdef CONFIG_PM
	tegra_pcie_suspend(NULL);
#endif
}

static void work_hotplug_handler(struct work_struct *work)
{
	struct tegra_pcie_info *pcie_driver =
		container_of(work, struct tegra_pcie_info, hotplug_detect);
	int val;

	if (pcie_driver->plat_data->gpio == -1)
		return;
	val = gpio_get_value(pcie_driver->plat_data->gpio);
	if (val == 0) {
		pr_info("Pcie Dock Connected\n");
		tegra_pcie_attach();
	} else {
		pr_info("Pcie Dock DisConnected\n");
		tegra_pcie_detach();
	}
}

static irqreturn_t gpio_pcie_detect_isr(int irq, void *arg)
{
	schedule_work(&tegra_pcie.hotplug_detect);
	return IRQ_HANDLED;
}

static irqreturn_t tegra_pcie_isr(int irq, void *arg)
{
	const char *err_msg[] = {
		"Unknown",
		"AXI slave error",
		"AXI decode error",
		"Target abort",
		"Master abort",
		"Invalid write",
		""
		"Response decoding error",
		"AXI response decoding error",
		"Transcation timeout",
	};

	u32 code, signature;

	code = afi_readl(AFI_INTR_CODE) & AFI_INTR_CODE_MASK;
	signature = afi_readl(AFI_INTR_SIGNATURE);
	afi_writel(0, AFI_INTR_CODE);

	if (code == AFI_INTR_LEGACY)
		return IRQ_NONE;

	if (code >= ARRAY_SIZE(err_msg))
		code = 0;

	/*
	 * do not pollute kernel log with master abort reports since they
	 * happen a lot during enumeration
	 */
	if (code == AFI_INTR_MASTER_ABORT)
		pr_debug("PCIE: %s, signature: %08x\n",
				err_msg[code], signature);
	else
		pr_err("PCIE: %s, signature: %08x\n", err_msg[code], signature);

	return IRQ_HANDLED;
}

/*
 *  PCIe support functions
 */
static void tegra_pcie_setup_translations(void)
{
	u32 fpci_bar;
	u32 size;
	u32 axi_address;

	/* Bar 0: config Bar */
	fpci_bar = ((u32)0xfdff << 16);
	size = PCIE_CFG_SZ;
	axi_address = TEGRA_PCIE_BASE + PCIE_CFG_OFF;
	afi_writel(axi_address, AFI_AXI_BAR0_START);
	afi_writel(size >> 12, AFI_AXI_BAR0_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR0);

	/* Bar 1: extended config Bar */
	fpci_bar = ((u32)0xfe1 << 20);
	size = PCIE_EXT_CFG_SZ;
	axi_address = TEGRA_PCIE_BASE + PCIE_EXT_CFG_OFF;
	afi_writel(axi_address, AFI_AXI_BAR1_START);
	afi_writel(size >> 12, AFI_AXI_BAR1_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR1);

	/* Bar 2: downstream IO bar */
	fpci_bar = ((__u32)0xfdfc << 16);
	size = MMIO_SIZE;
	axi_address = MMIO_BASE;
	afi_writel(axi_address, AFI_AXI_BAR2_START);
	afi_writel(size >> 12, AFI_AXI_BAR2_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR2);

	/* Bar 3: prefetchable memory BAR */
	fpci_bar = (((PREFETCH_MEM_BASE_0 >> 12) & 0x0fffffff) << 4) | 0x1;
	size =  PREFETCH_MEM_SIZE_0;
	axi_address = PREFETCH_MEM_BASE_0;
	afi_writel(axi_address, AFI_AXI_BAR3_START);
	afi_writel(size >> 12, AFI_AXI_BAR3_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR3);

	/* Bar 4: non prefetchable memory BAR */
	fpci_bar = (((MEM_BASE_0 >> 12)	& 0x0FFFFFFF) << 4) | 0x1;
	size = MEM_SIZE_0;
	axi_address = MEM_BASE_0;
	afi_writel(axi_address, AFI_AXI_BAR4_START);
	afi_writel(size >> 12, AFI_AXI_BAR4_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR4);

	/* Bar 5: NULL out the remaining BAR as it is not used */
	fpci_bar = 0;
	size = 0;
	axi_address = 0;
	afi_writel(axi_address, AFI_AXI_BAR5_START);
	afi_writel(size >> 12, AFI_AXI_BAR5_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR5);

	/* map all upstream transactions as uncached */
	afi_writel(PHYS_OFFSET, AFI_CACHE_BAR0_ST);
	afi_writel(0, AFI_CACHE_BAR0_SZ);
	afi_writel(0, AFI_CACHE_BAR1_ST);
	afi_writel(0, AFI_CACHE_BAR1_SZ);

	/* No MSI */
	afi_writel(0, AFI_MSI_FPCI_BAR_ST);
	afi_writel(0, AFI_MSI_BAR_SZ);
	afi_writel(0, AFI_MSI_AXI_BAR_ST);
	afi_writel(0, AFI_MSI_BAR_SZ);
}

static int tegra_pcie_enable_controller(void)
{
	u32 val, reg;
	int i, timeout;
	void __iomem *reg_mselect_base;

	reg_mselect_base = IO_ADDRESS(TEGRA_MSELECT_BASE);
	/* select the PCIE APERTURE in MSELECT config */
	reg = readl(reg_mselect_base);
	reg |= 1 << MSELECT_CONFIG_0_ENABLE_PCIE_APERTURE;
	writel(reg, reg_mselect_base);

	/* Enable slot clock and pulse the reset signals */
	for (i = 0, reg = AFI_PEX0_CTRL; i < MAX_PCIE_SUPPORTED_PORTS;
			i++, reg += (i*8)) {
		val = afi_readl(reg) | AFI_PEX_CTRL_CLKREQ_EN | AFI_PEX_CTRL_REFCLK_EN;
		afi_writel(val, reg);
		val &= ~AFI_PEX_CTRL_RST;
		afi_writel(val, reg);

		val = afi_readl(reg) | AFI_PEX_CTRL_RST;
		afi_writel(val, reg);
	}
	afi_writel(0, AFI_PEXBIAS_CTRL_0);

	/* Enable all PCIE controller and */
	/* system management configuration of PCIE crossbar */
	val = afi_readl(AFI_PCIE_CONFIG);
	val &= ~(AFI_PCIE_CONFIG_PCIEC0_DISABLE_DEVICE |
		 AFI_PCIE_CONFIG_PCIEC1_DISABLE_DEVICE |
		 AFI_PCIE_CONFIG_PCIEC2_DISABLE_DEVICE |
		 AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	val |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_DUAL;
#else
	val |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_411;
#endif
	afi_writel(val, AFI_PCIE_CONFIG);

	/* Disable Gen 2 capability of PCIE */
	val = afi_readl(AFI_FUSE) & ~AFI_FUSE_PCIE_T0_GEN2_DIS;
	afi_writel(val, AFI_FUSE);

	/* Initialze internal PHY, enable up to 16 PCIE lanes */
	pads_writel(0x0, PADS_CTL_SEL);

	/* override IDDQ to 1 on all 4 lanes */
	val = pads_readl(PADS_CTL) | PADS_CTL_IDDQ_1L;
	pads_writel(val, PADS_CTL);

	/*
	 * set up PHY PLL inputs select PLLE output as refclock,
	 * set pll TX clock ref to div10 (not div5)
	 * set pll ref clock buf to enable.
	 */
	val = pads_readl(PADS_PLL_CTL);
	val &= ~(PADS_PLL_CTL_REFCLK_MASK | PADS_PLL_CTL_TXCLKREF_MASK);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	val |= (PADS_PLL_CTL_REFCLK_INTERNAL_CML | PADS_PLL_CTL_TXCLKREF_DIV10);
#else
	val |= (PADS_PLL_CTL_REFCLK_INTERNAL_CML | PADS_PLL_CTL_TXCLKREF_BUF_EN);
#endif
	pads_writel(val, PADS_PLL_CTL);

	/* take PLL out of reset  */
	val = pads_readl(PADS_PLL_CTL) | PADS_PLL_CTL_RST_B4SM;
	pads_writel(val, PADS_PLL_CTL);

	/*
	 * Hack, set the clock voltage to the DEFAULT provided by hw folks.
	 * This doesn't exist in the documentation
	 */
	pads_writel(0xfa5cfa5c, 0xc8);
	pads_writel(0x0000FA5C, PADS_REFCLK_CFG1);

	/* Wait for the PLL to lock */
	timeout = 300;
	do {
		val = pads_readl(PADS_PLL_CTL);
		usleep_range(1000, 1000);
		if (--timeout == 0) {
			pr_err("Tegra PCIe error: timeout waiting for PLL\n");
			return -EBUSY;
		}
	} while (!(val & PADS_PLL_CTL_LOCKDET));

	/* turn off IDDQ override */
	val = pads_readl(PADS_CTL) & ~PADS_CTL_IDDQ_1L;
	pads_writel(val, PADS_CTL);

	/* enable TX/RX data */
	val = pads_readl(PADS_CTL);
	val |= (PADS_CTL_TX_DATA_EN_1L | PADS_CTL_RX_DATA_EN_1L);
	pads_writel(val, PADS_CTL);

	/* Take the PCIe interface module out of reset */
	tegra_periph_reset_deassert(tegra_pcie.pcie_xclk);

	/* WAR avoid hang on CPU read/write while gpu transfers in progress */
	val = afi_readl(AFI_CONFIGURATION) | AFI_CONFIGURATION_DFPCI_RSPPASSPW;

	/* Finally enable PCIe */
	val |=  AFI_CONFIGURATION_EN_FPCI;
	afi_writel(val, AFI_CONFIGURATION);

	val = (AFI_INTR_EN_INI_SLVERR | AFI_INTR_EN_INI_DECERR |
	       AFI_INTR_EN_TGT_SLVERR | AFI_INTR_EN_TGT_DECERR |
	       AFI_INTR_EN_TGT_WRERR | AFI_INTR_EN_DFPCI_DECERR |
	       AFI_INTR_EN_PRSNT_SENSE);
	afi_writel(val, AFI_AFI_INTR_ENABLE);
	afi_writel(0xffffffff, AFI_SM_INTR_ENABLE);

	/* FIXME: No MSI for now, only INT */
	afi_writel(AFI_INTR_MASK_INT_MASK, AFI_INTR_MASK);

	/* Disable all execptions */
	afi_writel(0, AFI_FPCI_ERROR_MASKS);

	return 0;
}

static int tegra_pcie_enable_regulators(void)
{
	if (tegra_pcie.power_rails_enabled) {
		pr_debug("PCIE: Already power rails enabled");
		return 0;
	}
	tegra_pcie.power_rails_enabled = 1;

	if (tegra_pcie.regulator_hvdd == NULL) {
		printk(KERN_INFO "PCIE.C: %s : regulator hvdd_pex\n",
					__func__);
		tegra_pcie.regulator_hvdd =
			regulator_get(tegra_pcie.dev, "hvdd_pex");
		if (IS_ERR_OR_NULL(tegra_pcie.regulator_hvdd)) {
			pr_err("%s: unable to get hvdd_pex regulator\n",
					__func__);
			tegra_pcie.regulator_hvdd = 0;
		}
	}

	if (tegra_pcie.regulator_pexio == NULL) {
		printk(KERN_INFO "PCIE.C: %s : regulator pexio\n", __func__);
		tegra_pcie.regulator_pexio =
			regulator_get(tegra_pcie.dev, "vdd_pexb");
		if (IS_ERR_OR_NULL(tegra_pcie.regulator_pexio)) {
			pr_err("%s: unable to get pexio regulator\n", __func__);
			tegra_pcie.regulator_pexio = 0;
		}
	}

	/*SATA and PCIE use same PLLE, In default configuration,
	* and we set default AVDD_PLLE with SATA.
	* So if use default board, you have to turn on (LDO2) AVDD_PLLE.
	 */
	if (tegra_pcie.regulator_avdd_plle == NULL) {
		printk(KERN_INFO "PCIE.C: %s : regulator avdd_plle\n",
				__func__);
		tegra_pcie.regulator_avdd_plle = regulator_get(tegra_pcie.dev,
						"avdd_plle");
		if (IS_ERR_OR_NULL(tegra_pcie.regulator_avdd_plle)) {
			pr_err("%s: unable to get avdd_plle regulator\n",
				__func__);
			tegra_pcie.regulator_avdd_plle = 0;
		}
	}
	if (tegra_pcie.regulator_hvdd)
		regulator_enable(tegra_pcie.regulator_hvdd);
	if (tegra_pcie.regulator_pexio)
		regulator_enable(tegra_pcie.regulator_pexio);
	if (tegra_pcie.regulator_avdd_plle)
		regulator_enable(tegra_pcie.regulator_avdd_plle);

	return 0;
}

static int tegra_pcie_disable_regulators(void)
{
	int err = 0;

	if (tegra_pcie.power_rails_enabled == 0) {
		pr_debug("PCIE: Already power rails disabled");
		goto err_exit;
	}
	if (tegra_pcie.regulator_hvdd)
		err = regulator_disable(tegra_pcie.regulator_hvdd);
	if (err)
		goto err_exit;
	if (tegra_pcie.regulator_pexio)
		err = regulator_disable(tegra_pcie.regulator_pexio);
	if (err)
		goto err_exit;
	if (tegra_pcie.regulator_avdd_plle)
		err = regulator_disable(tegra_pcie.regulator_avdd_plle);
	tegra_pcie.power_rails_enabled = 0;
err_exit:
	return err;
}

static int tegra_pcie_power_regate(void)
{
	int err;
	err = tegra_unpowergate_partition_with_clk_on(TEGRA_POWERGATE_PCIE);
	if (err) {
		pr_err("PCIE: powerup sequence failed: %d\n", err);
		return err;
	}
	tegra_periph_reset_assert(tegra_pcie.pcie_xclk);
	return clk_prepare_enable(tegra_pcie.pll_e);
}

static int tegra_pcie_map_resources(void)
{
	int err;

	/* Allocate config space virtual memory */
	tegra_pcie.regs = ioremap_nocache(TEGRA_PCIE_BASE, PCIE_IOMAP_SZ);
	if (tegra_pcie.regs == NULL) {
		pr_err("PCIE: Failed to map PCI/AFI registers\n");
		return -ENOMEM;
	}

	err = request_resource(&iomem_resource, &tegra_pcie_res_mmio);
	if (err) {
		pr_err("PCIE: Failed to request resources: %d\n", err);
		return err;
	}
	tegra_pcie.res_mmio = &tegra_pcie_res_mmio;

	/* Allocate downstream IO virtual memory */
	tegra_pcie_io_base = ioremap_nocache(tegra_pcie_res_mmio.start,
			resource_size(&tegra_pcie_res_mmio));
	if (tegra_pcie_io_base == NULL) {
		pr_err("PCIE: Failed to map IO\n");
		return -ENOMEM;
	}
	return err;
}

void tegra_pcie_unmap_resources(void)
{
	if (tegra_pcie_io_base) {
		iounmap(tegra_pcie_io_base);
		tegra_pcie_io_base = 0;
	}
	if (tegra_pcie.res_mmio) {
		release_resource(tegra_pcie.res_mmio);
		tegra_pcie.res_mmio = 0;
	}
	if (tegra_pcie.regs) {
		iounmap(tegra_pcie.regs);
		tegra_pcie.regs = 0;
	}
}
static int tegra_pcie_power_off(void);

static int tegra_pcie_power_on(void)
{
	int err = 0;

	if (tegra_pcie.pcie_power_enabled) {
		pr_debug("PCIE: Already powered on");
		goto err_exit;
	}
	tegra_pcie.pcie_power_enabled = 1;

	err = tegra_pcie_enable_regulators();
	if (err) {
		pr_err("PCIE: Failed to enable regulators\n");
		goto err_exit;
	}
	err = tegra_pcie_power_regate();
	if (err) {
		pr_err("PCIE: Failed to power regate\n");
		goto err_exit;
	}
	err = tegra_pcie_map_resources();
	if (err) {
		pr_err("PCIE: Failed to map resources\n");
		goto err_exit;
	}

err_exit:
	if (err)
		tegra_pcie_power_off();
	return err;
}

static int tegra_pcie_power_off(void)
{
	int err = 0;

	if (tegra_pcie.pcie_power_enabled == 0) {
		pr_debug("PCIE: Already powered off");
		goto err_exit;
	}
	tegra_pcie_unmap_resources();
	if (tegra_pcie.pll_e)
		clk_disable_unprepare(tegra_pcie.pll_e);

	err = tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_PCIE);
	if (err)
		goto err_exit;

	err = tegra_pcie_disable_regulators();

	tegra_pcie.pcie_power_enabled = 0;
err_exit:
	return err;
}

static int tegra_pcie_clocks_get(void)
{
	/* reset the PCIEXCLK */
	tegra_pcie.pcie_xclk = clk_get_sys("tegra_pcie", "pciex");
	if (IS_ERR_OR_NULL(tegra_pcie.pcie_xclk)) {
		pr_err("%s: unable to get PCIE Xclock\n", __func__);
		goto error_exit;
	}
	tegra_pcie.pll_e = clk_get_sys(NULL, "pll_e");
	if (IS_ERR_OR_NULL(tegra_pcie.pll_e)) {
		pr_err("%s: unable to get PLLE\n", __func__);
		goto error_exit;
	}
	return 0;
error_exit:
	if (tegra_pcie.pcie_xclk)
		clk_put(tegra_pcie.pcie_xclk);
	if (tegra_pcie.pll_e)
		clk_put(tegra_pcie.pll_e);
	return -EINVAL;
}

static void tegra_pcie_clocks_put(void)
{
	if (tegra_pcie.pll_e)
		clk_put(tegra_pcie.pll_e);
	if (tegra_pcie.pcie_xclk)
		clk_put(tegra_pcie.pcie_xclk);
}

static int tegra_pcie_get_resources(void)
{
	int err;

	tegra_pcie.power_rails_enabled = 0;
	tegra_pcie.pcie_power_enabled = 0;

	err = tegra_pcie_clocks_get();
	if (err) {
		pr_err("PCIE: failed to get clocks: %d\n", err);
		goto err_clk_get;
	}
	err = tegra_pcie_power_on();
	if (err) {
		pr_err("PCIE: Failed to power on: %d\n", err);
		goto err_pwr_on;
	}
	err = request_irq(INT_PCIE_INTR, tegra_pcie_isr,
			IRQF_SHARED, "PCIE", &tegra_pcie);
	if (err) {
		pr_err("PCIE: Failed to register IRQ: %d\n", err);
		goto err_pwr_on;
	}
	set_irq_flags(INT_PCIE_INTR, IRQF_VALID);
	return 0;

err_pwr_on:
	tegra_pcie_power_off();
err_clk_get:
	tegra_pcie_clocks_put();
	return err;
}

/*
 * FIXME: If there are no PCIe cards attached, then calling this function
 * can result in the increase of the bootup time as there are big timeout
 * loops.
 */
#define TEGRA_PCIE_LINKUP_TIMEOUT	200	/* up to 1.2 seconds */
static bool tegra_pcie_check_link(struct tegra_pcie_port *pp, int idx,
				  u32 reset_reg)
{
	u32 reg;
	int retries = 3;
	int timeout;

	do {
		timeout = TEGRA_PCIE_LINKUP_TIMEOUT;
		while (timeout) {
			reg = readl(pp->base + RP_VEND_XP);

			if (reg & RP_VEND_XP_DL_UP)
				break;

			mdelay(1);
			timeout--;
		}

		if (!timeout)  {
			pr_err("PCIE: port %d: link down, retrying\n", idx);
			goto retry;
		}

		timeout = TEGRA_PCIE_LINKUP_TIMEOUT;
		while (timeout) {
			reg = readl(pp->base + RP_LINK_CONTROL_STATUS);

			if (reg & 0x20000000)
				return true;

			mdelay(1);
			timeout--;
		}

retry:
		if (--retries) {
			/* Pulse the PEX reset */
			reg = afi_readl(reset_reg) & ~AFI_PEX_CTRL_RST;
			afi_writel(reg, reset_reg);
			reg = afi_readl(reset_reg) | AFI_PEX_CTRL_RST;
			afi_writel(reg, reset_reg);
		}

	} while (retries);

	return false;
}

static void tegra_pcie_enable_clock_clamp(int index)
{
	unsigned int data;

	/* Power mangagement settings */
	/* Enable clock clamping by default */
	data = rp_readl(NV_PCIE2_RP_PRIV_MISC, index);
	data |= (PCIE2_RP_PRIV_MISC_CTLR_CLK_CLAMP_ENABLE) |
		(PCIE2_RP_PRIV_MISC_TMS_CLK_CLAMP_ENABLE);
	rp_writel(data, NV_PCIE2_RP_PRIV_MISC, index);
}

static void tegra_pcie_enable_aspm_l1_support(int index)
{
	unsigned int data;

	/* Enable ASPM - L1 state support by default */
	data = rp_readl(NV_PCIE2_RP_VEND_XP1, index);
	data |= (NV_PCIE2_RP_VEND_XP1_LINK_PVT_CTL_L1_ASPM_SUPPORT_ENABLE);
	rp_writel(data, NV_PCIE2_RP_VEND_XP1, index);
}

static void tegra_pcie_add_port(int index, u32 offset, u32 reset_reg)
{
	struct tegra_pcie_port *pp;
	unsigned int data;

	pp = tegra_pcie.port + tegra_pcie.num_ports;

	pp->index = -1;
	pp->base = tegra_pcie.regs + offset;
	pp->link_up = tegra_pcie_check_link(pp, index, reset_reg);

	if (!pp->link_up) {
		pp->base = NULL;
		printk(KERN_INFO "PCIE: port %d: link down, ignoring\n", index);
		return;
	}
	tegra_pcie_enable_clock_clamp(index);
	tegra_pcie_enable_aspm_l1_support(index);

	/*
	 * Initialize TXBA1 register to fix the unfair arbitration
	 * between downstream reads and completions to upstream reads
	 */
	data = rp_readl(RP_TXBA1, index);
	data &= ~(RP_TXBA1_PW_OVER_CM_BURST_MASK);
	data |= RP_TXBA1_PW_OVER_CM_BURST_INIT_VAL;
	data &= ~(RP_TXBA1_CM_OVER_PW_BURST_MASK);
	data |= RP_TXBA1_CM_OVER_PW_BURST_INIT_VAL;
	rp_writel(data, RP_TXBA1, index);

	tegra_pcie.num_ports++;
	pp->index = index;
	memset(pp->res, 0, sizeof(pp->res));
}

static int tegra_pcie_init(void)
{
	int err = 0;
	int port;
	int rp_offset = 0;
	int ctrl_offset = AFI_PEX0_CTRL;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	pcibios_min_mem = 0x1000;
	pcibios_min_io = 0;
#else
	pcibios_min_mem = 0x03000000ul;
	pcibios_min_io = 0x10000000ul;
#endif

	INIT_WORK(&tegra_pcie.hotplug_detect, work_hotplug_handler);
	err = tegra_pcie_get_resources();
	if (err)
		return err;

	err = tegra_pcie_enable_controller();
	if (err)
		return err;

	/* setup the AFI address translations */
	tegra_pcie_setup_translations();
	for (port = 0; port < MAX_PCIE_SUPPORTED_PORTS; port++) {
		ctrl_offset += (port * 8);
		rp_offset = (rp_offset + RP_OFFSET) * port;
		if (tegra_pcie.plat_data->port_status[port])
			tegra_pcie_add_port(port, rp_offset, ctrl_offset);
	}

	if (tegra_pcie.plat_data->use_dock_detect) {
		int irq;

		pr_info("acquiring dock_detect = %d\n",
				tegra_pcie.plat_data->gpio);
		gpio_request(tegra_pcie.plat_data->gpio, "pcie_dock_detect");
		gpio_direction_input(tegra_pcie.plat_data->gpio);
		irq = gpio_to_irq(tegra_pcie.plat_data->gpio);
		if (irq < 0) {
			pr_err("Unable to get irq number for dock_detect\n");
			goto err_irq;
		}
		err = request_irq((unsigned int)irq,
				gpio_pcie_detect_isr,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"pcie_dock_detect",
				(void *)tegra_pcie.plat_data);
		if (err < 0) {
			pr_err("Unable to claim irq number for dock_detect\n");
			goto err_irq;
		}
	}

	if (tegra_pcie.num_ports)
		pci_common_init(&tegra_pcie_hw);
	else {
		/* no dock is connected, hotplug will occur after boot */
		err = tegra_pcie_power_off();
		is_dock_conn_at_boot = false;
	}

err_irq:

	return err;
}

static int tegra_pcie_probe(struct platform_device *pdev)
{
	int ret;

	tegra_pcie.dev = &pdev->dev;
	tegra_pcie.plat_data = pdev->dev.platform_data;
	dev_dbg(&pdev->dev, "PCIE.C: %s : _port_status[0] %d\n",
		__func__, tegra_pcie.plat_data->port_status[0]);
	dev_dbg(&pdev->dev, "PCIE.C: %s : _port_status[1] %d\n",
		__func__, tegra_pcie.plat_data->port_status[1]);
	dev_dbg(&pdev->dev, "PCIE.C: %s : _port_status[2] %d\n",
		__func__, tegra_pcie.plat_data->port_status[2]);
	ret = tegra_pcie_init();

	return ret;
}

#ifdef CONFIG_PM
static int tegra_pcie_suspend(struct device *dev)
{
	struct pci_dev *pdev = NULL;

	async_synchronize_full();

	for_each_pci_dev(pdev) {
		pci_stop_and_remove_bus_device(pdev);
		break;
	}

	/* disable read/write registers before powering off */
	is_pcie_noirq_op = true;
	/* reset number of ports since fresh initialization occurs in resume */
	tegra_pcie.num_ports = 0;

	return tegra_pcie_power_off();
}

static void tegra_pcie_set_irq(struct pci_bus *bus)
{
	struct pci_bus *b;
	struct pci_dev *pdev;

	list_for_each_entry(pdev, &bus->devices, bus_list) {
		b = pdev->subordinate;
		if (!b) {
			pdev->irq = tegra_pcie_map_irq(pdev,0,0);
			pci_write_config_byte(pdev, PCI_INTERRUPT_LINE, pdev->irq);
			continue;
		}
		tegra_pcie_set_irq(b);
		pdev->irq = tegra_pcie_map_irq(pdev,0,0);
		pci_write_config_byte(pdev, PCI_INTERRUPT_LINE, pdev->irq);
	}
}

static int tegra_pcie_resume(struct device *dev)
{
	int ret = 0;
	struct pci_bus *bus = NULL;
	int port, rp_offset = 0;
	int ctrl_offset = AFI_PEX0_CTRL;

	/* return w/o resume if cardhu dock is not connected */
	if (gpio_get_value(tegra_pcie.plat_data->gpio))
		goto exit;

	ret = tegra_pcie_power_on();
	if (ret) {
		pr_err("PCIE: Failed to power on: %d\n", ret);
		return ret;
	}
	/* enable read/write registers after powering on */
	is_pcie_noirq_op = false;
	tegra_pcie_enable_controller();
	tegra_pcie_setup_translations();
	msi_enable = false;

	for (port = 0; port < MAX_PCIE_SUPPORTED_PORTS; port++) {
		ctrl_offset += (port * 8);
		rp_offset = (rp_offset + 0x1000) * port;
		if (tegra_pcie.plat_data->port_status[port])
			tegra_pcie_add_port(port, rp_offset, ctrl_offset);
	}
	if (!tegra_pcie.num_ports) {
		tegra_pcie_power_off();
		goto exit;
	}

	tegra_pcie_hotplug_init();
	while ((bus = pci_find_next_bus(bus)) != NULL) {
		struct pci_dev *dev;

		pci_scan_child_bus(bus);

		list_for_each_entry(dev, &bus->devices, bus_list)
		if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE ||
			dev->hdr_type == PCI_HEADER_TYPE_CARDBUS)
			if (dev->subordinate)
				pci_bus_size_bridges(dev->subordinate);

		/* set irq for all devices */
		tegra_pcie_set_irq(bus);
		pci_bus_assign_resources(bus);
		pci_enable_bridges(bus);
		pci_bus_add_devices(bus);
	}
exit:
	return 0;
}
#endif

static int tegra_pcie_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops tegra_pcie_pm_ops = {
	.suspend = tegra_pcie_suspend,
	.resume = tegra_pcie_resume,
	};
#endif

static struct platform_driver tegra_pcie_driver = {
	.probe   = tegra_pcie_probe,
	.remove  = tegra_pcie_remove,
	.driver  = {
		.name  = "tegra-pcie",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm    = &tegra_pcie_pm_ops,
#endif
	},
};

static int __init tegra_pcie_init_driver(void)
{
	return platform_driver_register(&tegra_pcie_driver);
}

static void __exit tegra_pcie_exit_driver(void)
{
	platform_driver_unregister(&tegra_pcie_driver);
}

module_init(tegra_pcie_init_driver);
module_exit(tegra_pcie_exit_driver);

static struct irq_chip tegra_irq_chip_msi_pcie = {
	.name = "PCIe-MSI",
	.irq_mask = mask_msi_irq,
	.irq_unmask = unmask_msi_irq,
	.irq_enable = unmask_msi_irq,
	.irq_disable = mask_msi_irq,
};

/* 1:1 matching of these to the MSI vectors, 1 per bit */
/* and each mapping matches one of the available interrupts */
/*   irq should equal INT_PCI_MSI_BASE + index */
struct msi_map_entry {
	bool used;
	u8 index;
	int irq;
};

/* hardware supports 256 max*/
#if (INT_PCI_MSI_NR > 256)
#error "INT_PCI_MSI_NR too big"
#endif

#define MSI_MAP_SIZE  (INT_PCI_MSI_NR)
static struct msi_map_entry msi_map[MSI_MAP_SIZE];

static void msi_map_init(void)
{
	int i;

	for (i = 0; i < MSI_MAP_SIZE; i++) {
		msi_map[i].used = false;
		msi_map[i].index = i;
		msi_map[i].irq = 0;
	}
}

/* returns an index into the map*/
static struct msi_map_entry *msi_map_get(void)
{
	struct msi_map_entry *retval = NULL;
	int i;

	for (i = 0; i < MSI_MAP_SIZE; i++) {
		if (!msi_map[i].used) {
			retval = msi_map + i;
			retval->irq = INT_PCI_MSI_BASE + i;
			retval->used = true;
			break;
		}
	}

	return retval;
}

void msi_map_release(struct msi_map_entry *entry)
{
	if (entry) {
		entry->used = false;
		entry->irq = 0;
	}
}

static irqreturn_t tegra_pcie_msi_isr(int irq, void *arg)
{
	int i;
	int offset;
	int index;
	u32 reg;

	for (i = 0; i < 8; i++) {
		reg = afi_readl(AFI_MSI_VEC0_0 + i * 4);
		while (reg != 0x00000000) {
			offset = find_first_bit((unsigned long int *)&reg, 32);
			index = i * 32 + offset;
			/* clear the interrupt */
			afi_writel(1ul << index, AFI_MSI_VEC0_0 + i * 4);
			if (index < MSI_MAP_SIZE) {
				if (msi_map[index].used)
					generic_handle_irq(msi_map[index].irq);
				else
					printk(KERN_INFO "unexpected MSI (1)\n");
			} else {
				/* that's weird who triggered this?*/
				/* just clear it*/
				printk(KERN_INFO "unexpected MSI (2)\n");
			}
			/* see if there's any more pending in this vector */
			reg = afi_readl(AFI_MSI_VEC0_0 + i * 4);
		}
	}

	return IRQ_HANDLED;
}

static bool tegra_pcie_enable_msi(void)
{
	bool retval = false;
	u32 reg;
	u32 msi_base = 0;
	u32 msi_aligned = 0;

	/* this only happens once. */
	if (msi_enable) {
		retval = true;
		goto exit;
	}
	msi_map_init();

	/* enables MSI interrupts.  */
	if (request_irq(INT_PCIE_MSI, tegra_pcie_msi_isr,
		IRQF_SHARED, "PCIe-MSI",
		tegra_pcie_msi_isr)) {
			pr_err("%s: Cannot register IRQ %u\n",
				__func__, INT_PCIE_MSI);
			goto exit;
	}
	/* setup AFI/FPCI range */
	/* FIXME do this better! should be based on PAGE_SIZE */
	msi_base = __get_free_pages(GFP_KERNEL, 3);
	msi_aligned = ((msi_base + ((1<<12) - 1)) & ~((1<<12) - 1));
	msi_aligned = virt_to_phys((void *)msi_aligned);

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	afi_writel(msi_aligned, AFI_MSI_FPCI_BAR_ST);
#else
	/* different from T20!*/
	afi_writel(msi_aligned>>8, AFI_MSI_FPCI_BAR_ST);
#endif
	afi_writel(msi_aligned, AFI_MSI_AXI_BAR_ST);
	/* this register is in 4K increments */
	afi_writel(1, AFI_MSI_BAR_SZ);

	/* enable all MSI vectors */
	afi_writel(0xffffffff, AFI_MSI_EN_VEC0_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC1_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC2_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC3_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC4_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC5_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC6_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC7_0);

	/* and unmask the MSI interrupt */
	reg = 0;
	reg |= (AFI_INTR_MASK_INT_MASK | AFI_INTR_MASK_MSI_MASK);
	afi_writel(reg, AFI_INTR_MASK);

	set_irq_flags(INT_PCIE_MSI, IRQF_VALID);

	msi_enable = true;
	retval = true;
exit:
	if (!retval) {
		if (msi_base)
			free_pages(msi_base, 3);
	}
	return retval;
}


/* called by arch_setup_msi_irqs in drivers/pci/msi.c */
int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	int retval = -EINVAL;
	struct msi_msg msg;
	struct msi_map_entry *map_entry = NULL;

	if (!tegra_pcie_enable_msi())
		goto exit;

	map_entry = msi_map_get();
	if (map_entry == NULL)
		goto exit;

	retval = irq_alloc_desc(map_entry->irq);
	if (retval < 0)
		goto exit;
	irq_set_chip_and_handler(map_entry->irq,
				&tegra_irq_chip_msi_pcie,
				handle_simple_irq);

	retval = irq_set_msi_desc(map_entry->irq, desc);
	if (retval < 0)
		goto exit;
	set_irq_flags(map_entry->irq, IRQF_VALID);

	msg.address_lo = afi_readl(AFI_MSI_AXI_BAR_ST);
	/* 32 bit address only */
	msg.address_hi = 0;
	msg.data = map_entry->index;

	write_msi_msg(map_entry->irq, &msg);

	retval = 0;
exit:
	if (retval != 0) {
		if (map_entry) {
			irq_free_desc(map_entry->irq);
			msi_map_release(map_entry);
		}
	}

	return retval;
}

void arch_teardown_msi_irq(unsigned int irq)
{
	int i;
	for (i = 0; i < MSI_MAP_SIZE; i++) {
		if ((msi_map[i].used) && (msi_map[i].irq == irq)) {
			irq_free_desc(msi_map[i].irq);
			msi_map_release(msi_map + i);
			break;
		}
	}
}
