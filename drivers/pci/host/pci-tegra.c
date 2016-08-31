/*
 * PCIe host controller driver for TEGRA SOCs
 *
 * Copyright (c) 2010, CompuLab, Ltd.
 * Author: Mike Rapoport <mike@compulab.co.il>
 *
 * Based on NVIDIA PCIe driver
 * Copyright (c) 2008-2014, NVIDIA Corporation. All rights reserved.
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
#include <linux/clk/tegra.h>
#include <linux/msi.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/async.h>
#include <linux/vmalloc.h>
#include <linux/pm_runtime.h>
#include <linux/tegra-powergate.h>
#include <linux/tegra-soc.h>
#include <linux/pci-tegra.h>

#include <asm/sizes.h>
#include <asm/mach/pci.h>

#include <mach/tegra_usb_pad_ctrl.h>
#include <mach/pm_domains.h>
#include <mach/io_dpd.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t12.h>

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

#define AFI_FPCI_ERROR_MASKS						0xb0

#define AFI_INTR_MASK							0xb4
#define AFI_INTR_MASK_INT_MASK					(1 << 0)
#define AFI_INTR_MASK_MSI_MASK					(1 << 8)

#define AFI_INTR_CODE							0xb8
#define AFI_INTR_CODE_MASK						0xf
#define AFI_INTR_MASTER_ABORT						4
#define AFI_INTR_LEGACY						6
#define AFI_INTR_PRSNT_SENSE						10

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

#define AFI_PCIE_PME						0x0f0
#define AFI_PCIE_PME_TURN_OFF					0x101
#define AFI_PCIE_PME_ACK					0x420

#define AFI_PCIE_CONFIG						0x0f8
#define AFI_PCIE_CONFIG_PCIEC0_DISABLE_DEVICE			(1 << 1)
#define AFI_PCIE_CONFIG_PCIEC1_DISABLE_DEVICE			(1 << 2)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK		(0xf << 20)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X2_X1		(0x0 << 20)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X4_X1		(0x1 << 20)

#define AFI_FUSE							0x104
#define AFI_FUSE_PCIE_T0_GEN2_DIS				(1 << 2)

#define AFI_PEX0_CTRL							0x110
#define AFI_PEX1_CTRL							0x118
#define AFI_PEX_CTRL_RST					(1 << 0)
#define AFI_PEX_CTRL_CLKREQ_EN					(1 << 1)
#define AFI_PEX_CTRL_REFCLK_EN					(1 << 3)
#define AFI_PEX_CTRL_OVERRIDE_EN				(1 << 4)

#define AFI_PLLE_CONTROL					0x160
#define AFI_PLLE_CONTROL_BYPASS_PADS2PLLE_CONTROL		(1 << 9)
#define AFI_PLLE_CONTROL_PADS2PLLE_CONTROL_EN			(1 << 1)

#define AFI_PEXBIAS_CTRL_0					0x168
#define AFI_WR_SCRATCH_0					0x120
#define AFI_WR_SCRATCH_0_RESET_VAL				0x00202020
#define AFI_WR_SCRATCH_0_DEFAULT_VAL				0x00000000

#define AFI_MSG_0						0x190
#define AFI_MSG_PM_PME_MASK					0x00100010
#define AFI_MSG_INTX_MASK					0x1f001f00
#define AFI_MSG_PM_PME0						(1 << 4)

#define RP_VEND_XP						0x00000F00
#define RP_VEND_XP_DL_UP					(1 << 30)

#define RP_LINK_CONTROL_STATUS					0x00000090
#define RP_LINK_CONTROL_STATUS_LINKSTAT_MASK			0x3fff0000
#define RP_LINK_CONTROL_STATUS_RETRAIN_LINK			(0x1 << 5)

#define RP_LINK_CONTROL_STATUS_2				0x000000B0
#define RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_MASK		0x0000000F
#define RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN1		(0x1 << 0)
#define RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN2		(0x2 << 0)

#define  PADS_REFCLK_CFG0					0x000000C8
#define  PADS_REFCLK_CFG1					0x000000CC
#define  PADS_REFCLK_BIAS					0x000000D0

#define NV_PCIE2_RP_RSR					0x000000A0
#define NV_PCIE2_RP_RSR_PMESTAT				(1 << 16)

#define NV_PCIE2_RP_PRIV_MISC					0x00000FE0
#define PCIE2_RP_PRIV_MISC_PRSNT_MAP_EP_PRSNT			(0xE << 0)
#define PCIE2_RP_PRIV_MISC_PRSNT_MAP_EP_ABSNT			(0xF << 0)
#define PCIE2_RP_PRIV_MISC_CTLR_CLK_CLAMP_THRESHOLD		(0xF << 16)
#define PCIE2_RP_PRIV_MISC_CTLR_CLK_CLAMP_ENABLE		(1 << 23)
#define PCIE2_RP_PRIV_MISC_TMS_CLK_CLAMP_THRESHOLD		(0xF << 24)
#define PCIE2_RP_PRIV_MISC_TMS_CLK_CLAMP_ENABLE		(1 << 31)

#define NV_PCIE2_RP_VEND_XP1					0x00000F04
#define NV_PCIE2_RP_VEND_XP1_LINK_PVT_CTL_L1_ASPM_SUPPORT	(1 << 21)

#define NV_PCIE2_RP_VEND_CTL1					0x00000F48
#define PCIE2_RP_VEND_CTL1_ERPT				(1 << 13)

#define NV_PCIE2_RP_VEND_XP_BIST				0x00000F4C
#define PCIE2_RP_VEND_XP_BIST_GOTO_L1_L2_AFTER_DLLP_DONE	(1 << 28)

#define NV_PCIE2_RP_ECTL_1_R2					0x00000FD8
#define PCIE2_RP_ECTL_1_R2_TX_DRV_CNTL_1C			(0x3 << 28)

#define BOARD_PM359						0x0167
#define BOARD_PM358						0x0166

/*
 * AXI address map for the PCIe aperture , defines 1GB in the AXI
 *  address map for PCIe.
 *
 *  That address space is split into different regions, with sizes and
 *  offsets as follows. Except for the Register space, SW is free to slice the
 *  regions as it chooces.
 *
 *  The split below seems to work fine for now.
 *
 *  0x0100_0000 to 0x01ff_ffff - Register space           16MB.
 *  0x0200_0000 to 0x11ff_ffff - Config space             256MB.
 *  0x1200_0000 to 0x1200_ffff - Downstream IO space
 *   ... Will be filled with other BARS like MSI/upstream IO etc.
 *  0x1210_0000 to 0x320f_ffff - Prefetchable memory aperture
 *  0x3210_0000 to 0x3fff_ffff - non-prefetchable memory aperture
 */
#define TEGRA_PCIE_BASE	0x01000000

#define PCIE_REGS_SZ		SZ_16M
#define PCIE_CFG_OFF		(TEGRA_PCIE_BASE + PCIE_REGS_SZ)
#define PCIE_CFG_SZ		SZ_256M
/* During the boot only registers/config and extended config apertures are
 * mapped. Rest are mapped on demand by the PCI device drivers.
 */
#define MMIO_BASE		(PCIE_CFG_OFF + PCIE_CFG_SZ)
#define MMIO_SIZE		SZ_64K
#define PREFETCH_MEM_BASE_0	(MMIO_BASE + SZ_1M)
#define PREFETCH_MEM_SIZE_0	SZ_512M
#define MEM_BASE_0		(PREFETCH_MEM_BASE_0 + PREFETCH_MEM_SIZE_0)
#define MEM_SIZE_0		(SZ_1G - MEM_BASE_0)


#define DEBUG 0
#if DEBUG
#define PR_FUNC_LINE	pr_info("PCIE: %s(%d)\n", __func__, __LINE__)
#else
#define PR_FUNC_LINE	do {} while (0)
#endif

struct tegra_pcie_port {
	int			index;
	u8			root_bus_nr;
	void __iomem		*base;
	bool			link_up;
};

struct tegra_pcie_info {
	struct tegra_pcie_port	port[MAX_PCIE_SUPPORTED_PORTS];
	int			num_ports;
	void __iomem		*regs;
	int			power_rails_enabled;
	int			pcie_power_enabled;
	struct work_struct	hotplug_detect;

	struct regulator	*regulator_hvdd;
	struct regulator	*regulator_pexio;
	struct regulator	*regulator_avdd_plle;
	struct clk		*pcie_xclk;
	struct clk		*pcie_mselect;
	struct device		*dev;
	struct tegra_pci_platform_data *plat_data;
	struct list_head busses;
};

struct tegra_pcie_info tegra_pcie;
EXPORT_SYMBOL(tegra_pcie);

struct tegra_pcie_bus {
	struct vm_struct *area;
	struct list_head list;
	unsigned int nr;
};

static struct resource pcie_mem_space;
static struct resource pcie_prefetch_mem_space;

/* this flag enables features required either after boot or resume */
/* also required to enable msi from host both after boot and resume */
static bool resume_path;
/* used to avoid successive hotplug disconnect or connect */
static bool hotplug_event;

static inline void afi_writel(u32 value, unsigned long offset)
{
	writel(value, offset + AFI_OFFSET + tegra_pcie.regs);
}

static inline u32 afi_readl(unsigned long offset)
{
	return readl(offset + AFI_OFFSET + tegra_pcie.regs);
}

/* Array of PCIe Controller Register offsets */
static u32 pex_controller_registers[] = {
	AFI_PEX0_CTRL,
	AFI_PEX1_CTRL,
};

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

/*
 * The configuration space mapping on Tegra is somewhat similar to the ECAM
 * defined by PCIe. However it deviates a bit in how the 4 bits for extended
 * register accesses are mapped:
 *
 *    [27:24] extended register number
 *    [23:16] bus number
 *    [15:11] device number
 *    [10: 8] function number
 *    [ 7: 0] register number
 *
 * Mapping the whole extended configuration space would required 256 MiB of
 * virtual address space, only a small part of which will actually be used.
 * To work around this, a 1 MiB of virtual addresses are allocated per bus
 * when the bus is first accessed. When the physical range is mapped, the
 * the bus number bits are hidden so that the extended register number bits
 * appear as bits [19:16]. Therefore the virtual mapping looks like this:
 *
 *    [19:16] extended register number
 *    [15:11] device number
 *    [10: 8] function number
 *    [ 7: 0] register number
 *
 * This is achieved by stitching together 16 chunks of 64 KiB of physical
 * address space via the MMU.
 */
static unsigned long tegra_pcie_conf_offset(unsigned int devfn, int where)
{

	return ((where & 0xf00) << 8) | (PCI_SLOT(devfn) << 11) |
	       (PCI_FUNC(devfn) << 8) | (where & 0xfc);
}

static struct tegra_pcie_bus *tegra_pcie_bus_alloc(unsigned int busnr)
{
	pgprot_t prot = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY | L_PTE_XN |
			L_PTE_MT_DEV_SHARED | L_PTE_SHARED;
	phys_addr_t cs = (phys_addr_t)PCIE_CFG_OFF;
	struct tegra_pcie_bus *bus;
	unsigned int i;
	int err;

	PR_FUNC_LINE;
	bus = kzalloc(sizeof(*bus), GFP_KERNEL);
	if (!bus)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&bus->list);
	bus->nr = busnr;

	/* allocate 1 MiB of virtual addresses */
	bus->area = get_vm_area(SZ_1M, VM_IOREMAP);
	if (!bus->area) {
		err = -ENOMEM;
		goto free;
	}

	/* map each of the 16 chunks of 64 KiB each.
	 *
	 * Note that each chunk still needs to increment by 16 MiB in
	 * physical space.
	 */
	for (i = 0; i < 16; i++) {
		unsigned long virt = (unsigned long)bus->area->addr +
				     i * SZ_64K;
		phys_addr_t phys = cs + i * SZ_16M + busnr * SZ_64K;

		err = ioremap_page_range(virt, virt + SZ_64K, phys, prot);
		if (err < 0) {
			dev_err(tegra_pcie.dev, "ioremap_page_range() failed: %d\n",
				err);
			goto unmap;
		}
	}

	return bus;
unmap:
	vunmap(bus->area->addr);
free:
	kfree(bus);
	return ERR_PTR(err);
}

/*
 * Look up a virtual address mapping for the specified bus number.
 * If no such mapping existis, try to create one.
 */
static void __iomem *tegra_pcie_bus_map(unsigned int busnr)
{
	struct tegra_pcie_bus *bus;

	list_for_each_entry(bus, &tegra_pcie.busses, list)
		if (bus->nr == busnr)
			return bus->area->addr;

	bus = tegra_pcie_bus_alloc(busnr);
	if (IS_ERR(bus))
		return NULL;

	list_add_tail(&bus->list, &tegra_pcie.busses);

	return bus->area->addr;
}

int tegra_pcie_read_conf(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	struct tegra_pcie_port *pp = bus_to_port(bus->number);
	void __iomem *addr;

	if (pp) {
		if (devfn != 0) {
			*val = 0xffffffff;
			return PCIBIOS_DEVICE_NOT_FOUND;
		}

		addr = pp->base + (where & ~0x3);
	} else {
		addr = tegra_pcie_bus_map(bus->number);
		if (!addr) {
			dev_err(tegra_pcie.dev,
				"failed to map cfg. space for bus %u\n",
				bus->number);
			*val = 0xffffffff;
			return PCIBIOS_DEVICE_NOT_FOUND;
		}
		addr += tegra_pcie_conf_offset(devfn, where);
	}

	*val = readl(addr);

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}
EXPORT_SYMBOL(tegra_pcie_read_conf);

static int tegra_pcie_write_conf(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	struct tegra_pcie_port *pp = bus_to_port(bus->number);
	void __iomem *addr;

	u32 mask;
	u32 tmp;

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
		addr = tegra_pcie_bus_map(bus->number);
		if (!addr) {
			dev_err(tegra_pcie.dev,
				"failed to map cfg. space for bus %u\n",
				bus->number);
			return PCIBIOS_DEVICE_NOT_FOUND;
		}
		addr += tegra_pcie_conf_offset(devfn, where);
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

static void tegra_pcie_fixup_bridge(struct pci_dev *dev)
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
static void tegra_pcie_fixup_class(struct pci_dev *dev)
{
	dev->class = PCI_CLASS_BRIDGE_PCI << 8;
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1c, tegra_pcie_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1d, tegra_pcie_fixup_class);

/* Tegra PCIE requires relaxed ordering */
static void tegra_pcie_relax_enable(struct pci_dev *dev)
{
	pcie_capability_set_word(dev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_RELAX_EN);
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, tegra_pcie_relax_enable);

static void tegra_pcie_preinit(void)
{
	PR_FUNC_LINE;
	pcie_mem_space.name = "PCIe MEM Space";
	pcie_mem_space.start = MEM_BASE_0;
	pcie_mem_space.end = MEM_BASE_0 + MEM_SIZE_0 - 1;
	pcie_mem_space.flags = IORESOURCE_MEM;
	if (request_resource(&iomem_resource, &pcie_mem_space))
		panic("can't allocate PCIe MEM space");

	pcie_prefetch_mem_space.name = "PCIe PREFETCH MEM Space";
	pcie_prefetch_mem_space.start = PREFETCH_MEM_BASE_0;
	pcie_prefetch_mem_space.end = (PREFETCH_MEM_BASE_0
			+ PREFETCH_MEM_SIZE_0 - 1);
	pcie_prefetch_mem_space.flags = IORESOURCE_MEM | IORESOURCE_PREFETCH;
	if (request_resource(&iomem_resource, &pcie_prefetch_mem_space))
		panic("can't allocate PCIe PREFETCH MEM space");

}

static int tegra_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct tegra_pcie_port *pp;

	PR_FUNC_LINE;
	if (nr >= tegra_pcie.num_ports)
		return 0;

	pp = tegra_pcie.port + nr;
	pp->root_bus_nr = sys->busnr;

	pci_ioremap_io(nr * MMIO_SIZE, MMIO_BASE);
	pci_add_resource_offset(
		&sys->resources, &pcie_mem_space, sys->mem_offset);
	pci_add_resource_offset(
		&sys->resources, &pcie_prefetch_mem_space, sys->mem_offset);

	return 1;
}

static int tegra_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return INT_PCIE_INTR;
}

static struct pci_bus *__init tegra_pcie_scan_bus(int nr,
						  struct pci_sys_data *sys)
{
	struct tegra_pcie_port *pp;

	PR_FUNC_LINE;
	if (nr >= tegra_pcie.num_ports)
		return NULL;

	pp = tegra_pcie.port + nr;
	pp->root_bus_nr = sys->busnr;

	return pci_scan_root_bus(NULL, sys->busnr, &tegra_pcie_ops, sys,
				 &sys->resources);
}

static struct hw_pci __initdata tegra_pcie_hw = {
	.nr_controllers	= MAX_PCIE_SUPPORTED_PORTS,
	.preinit	= tegra_pcie_preinit,
	.setup		= tegra_pcie_setup,
	.scan		= tegra_pcie_scan_bus,
	.map_irq	= tegra_pcie_map_irq,
};

#ifdef CONFIG_PM
static int tegra_pcie_suspend_noirq(struct device *dev);
static int tegra_pcie_resume_noirq(struct device *dev);

#ifdef HOTPLUG_ON_SYSTEM_BOOT
/* It enumerates the devices when dock is connected after system boot */
/* this is similar to pcibios_init_hw in bios32.c */
static void __init tegra_pcie_hotplug_init(void)
{
	struct pci_sys_data *sys = NULL;
	int ret, nr;

	if (is_dock_conn_at_boot)
		return;

	PR_FUNC_LINE;
	tegra_pcie_preinit();
	for (nr = 0; nr < tegra_pcie_hw.nr_controllers; nr++) {
		sys = kzalloc(sizeof(struct pci_sys_data), GFP_KERNEL);
		if (!sys)
			panic("PCI: unable to allocate sys data!");

#ifdef CONFIG_PCI_DOMAINS
		sys->domain  = tegra_pcie_hw.domain;
#endif
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
			pci_create_root_bus(NULL, nr, &tegra_pcie_ops,
					sys, &sys->resources);
		}
	}
	is_dock_conn_at_boot = true;
}
#endif
#endif
static int tegra_pcie_attach(void)
{
	int err = 0;

	if (!hotplug_event)
		return err;
#ifdef CONFIG_PM
	err =  tegra_pcie_resume_noirq(NULL);
#endif
	hotplug_event = false;
	return err;
}

static int tegra_pcie_detach(void)
{
	int err = 0;

	if (hotplug_event)
		return err;
#ifdef CONFIG_PM
	err =  tegra_pcie_suspend_noirq(NULL);
#endif
	hotplug_event = true;
	return err;
}

static void tegra_pcie_prsnt_map_override(int index, bool prsnt)
{
	unsigned int data;

	if (hotplug_event)
		return;
	/* currently only hotplug on root port 0 supported */
	PR_FUNC_LINE;
	data = rp_readl(NV_PCIE2_RP_PRIV_MISC, index);
	data &= ~PCIE2_RP_PRIV_MISC_PRSNT_MAP_EP_ABSNT;
	if (prsnt)
		data |= PCIE2_RP_PRIV_MISC_PRSNT_MAP_EP_PRSNT;
	else
		data |= PCIE2_RP_PRIV_MISC_PRSNT_MAP_EP_ABSNT;
	rp_writel(data, NV_PCIE2_RP_PRIV_MISC, index);
}

static void work_hotplug_handler(struct work_struct *work)
{
	struct tegra_pcie_info *pcie_driver =
		container_of(work, struct tegra_pcie_info, hotplug_detect);
	int val;

	PR_FUNC_LINE;
	if (pcie_driver->plat_data->gpio_hot_plug == -1)
		return;
	val = gpio_get_value(pcie_driver->plat_data->gpio_hot_plug);
	if (val == 0) {
		pr_info("PCIE Hotplug: Connected\n");
		tegra_pcie_attach();
	} else {
		pr_info("PCIE Hotplug: DisConnected\n");
		tegra_pcie_detach();
	}
}

static irqreturn_t gpio_pcie_detect_isr(int irq, void *arg)
{
	PR_FUNC_LINE;
	schedule_work(&tegra_pcie.hotplug_detect);
	return IRQ_HANDLED;
}

static void notify_device_isr(u32 mesg)
{
	pr_debug(KERN_INFO "Legacy INTx interrupt occurred %x\n", mesg);
	/* TODO: Need to call pcie device isr instead of ignoring interrupt */
	/* same comment applies to below handler also */
}

static void handle_sb_intr(void)
{
	u32 mesg;

	PR_FUNC_LINE;
	mesg = afi_readl(AFI_MSG_0);

	if (mesg & AFI_MSG_INTX_MASK)
		/* notify device isr for INTx messages from pcie devices */
		notify_device_isr(mesg);
	else if (mesg & AFI_MSG_PM_PME_MASK) {
		u32 idx;
		/* handle PME messages */
		idx = (mesg & AFI_MSG_PM_PME0) ? 0 : 1;
		mesg = rp_readl(NV_PCIE2_RP_RSR, idx);
		mesg |= NV_PCIE2_RP_RSR_PMESTAT;
		rp_writel(mesg, NV_PCIE2_RP_RSR, idx);
	} else
		afi_writel(mesg, AFI_MSG_0);
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
		"",
		"Response decoding error",
		"AXI response decoding error",
		"Transcation timeout",
		"",
	};

	u32 code, signature;

	PR_FUNC_LINE;
	if (!tegra_pcie.regs) {
		pr_info("PCIE: PCI/AFI registers are unmapped\n");
		return IRQ_HANDLED;
	}
	code = afi_readl(AFI_INTR_CODE) & AFI_INTR_CODE_MASK;
	signature = afi_readl(AFI_INTR_SIGNATURE);

	if (code == AFI_INTR_LEGACY)
		handle_sb_intr();

	afi_writel(0, AFI_INTR_CODE);

	if (code >= ARRAY_SIZE(err_msg))
		code = 0;

	/*
	 * do not pollute kernel log with master abort reports since they
	 * happen a lot during enumeration
	 */
	if (code == AFI_INTR_MASTER_ABORT)
		pr_debug("PCIE: %s, signature: %08x\n",
				err_msg[code], signature);
	else if ((code != AFI_INTR_LEGACY) && (code != AFI_INTR_PRSNT_SENSE))
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

	PR_FUNC_LINE;
	/* Bar 0: type 1 extended configuration space */
	fpci_bar = 0xfe100000;
	size = PCIE_CFG_SZ;
	axi_address = PCIE_CFG_OFF;
	afi_writel(axi_address, AFI_AXI_BAR0_START);
	afi_writel(size >> 12, AFI_AXI_BAR0_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR0);

	/* Bar 1: downstream IO bar */
	fpci_bar = 0xfdfc0000;
	size = MMIO_SIZE;
	axi_address = MMIO_BASE;
	afi_writel(axi_address, AFI_AXI_BAR1_START);
	afi_writel(size >> 12, AFI_AXI_BAR1_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR1);

	/* Bar 2: prefetchable memory BAR */
	fpci_bar = (((PREFETCH_MEM_BASE_0 >> 12) & 0xfffff) << 4) | 0x1;
	size =  PREFETCH_MEM_SIZE_0;
	axi_address = PREFETCH_MEM_BASE_0;
	afi_writel(axi_address, AFI_AXI_BAR2_START);
	afi_writel(size >> 12, AFI_AXI_BAR2_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR2);

	/* Bar 3: non prefetchable memory BAR */
	fpci_bar = (((MEM_BASE_0 >> 12) & 0xfffff) << 4) | 0x1;
	size = MEM_SIZE_0;
	axi_address = MEM_BASE_0;
	afi_writel(axi_address, AFI_AXI_BAR3_START);
	afi_writel(size >> 12, AFI_AXI_BAR3_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR3);

	/* NULL out the remaining BAR as it is not used */
	afi_writel(0, AFI_AXI_BAR4_START);
	afi_writel(0, AFI_AXI_BAR4_SZ);
	afi_writel(0, AFI_FPCI_BAR4);

	afi_writel(0, AFI_AXI_BAR5_START);
	afi_writel(0, AFI_AXI_BAR5_SZ);
	afi_writel(0, AFI_FPCI_BAR5);

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

static int tegra_pcie_enable_pads(bool enable)
{
	int err = 0;

	PR_FUNC_LINE;
	if (!tegra_platform_is_fpga()) {
		/* WAR for Eye diagram failure on lanes for T124 platforms */
		pads_writel(0x34ac34ac, PADS_REFCLK_CFG0);
		pads_writel(0x00000028, PADS_REFCLK_BIAS);
		/* T124 PCIe pad programming is moved to XUSB_PADCTL space */
		err = pcie_phy_pad_enable(enable,
				tegra_get_lane_owner_info() >> 1);
		if (err)
			pr_err("%s unable to initalize pads\n", __func__);
	}
	return err;
}

static int tegra_pcie_enable_controller(void)
{
	u32 val, reg;
	int i, ret = 0, lane_owner;

	PR_FUNC_LINE;
	/* Enable slot clock and ensure reset signal is assert */
	for (i = 0; i < ARRAY_SIZE(pex_controller_registers); i++) {
		reg = pex_controller_registers[i];
		val = afi_readl(reg) | AFI_PEX_CTRL_REFCLK_EN |
			AFI_PEX_CTRL_CLKREQ_EN;
		/* Since CLKREQ# pinmux pins may float in some platfoms */
		/* resulting in disappear of refclk specially at higher temp */
		/* overrided CLKREQ to always drive refclk */
		if (!tegra_pcie.plat_data->has_clkreq)
			val |= AFI_PEX_CTRL_OVERRIDE_EN;
		val &= ~AFI_PEX_CTRL_RST;
		afi_writel(val, reg);
	}

	/* Enable PLL power down */
	val = afi_readl(AFI_PLLE_CONTROL);
	val &= ~AFI_PLLE_CONTROL_BYPASS_PADS2PLLE_CONTROL;
	val |= AFI_PLLE_CONTROL_PADS2PLLE_CONTROL_EN;
	afi_writel(val, AFI_PLLE_CONTROL);

	afi_writel(0, AFI_PEXBIAS_CTRL_0);

	lane_owner = 0;
	/* Enable all PCIE controller and */
	/* system management configuration of PCIE crossbar */
	val = afi_readl(AFI_PCIE_CONFIG);
	val &= ~(AFI_PCIE_CONFIG_PCIEC0_DISABLE_DEVICE |
		 AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK);
	if (tegra_platform_is_fpga()) {
		/* FPGA supports only x2_x1 bar config */
		val |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X2_X1;
	} else {
		/* Extract 2 upper bits from odmdata[28:30] and configure */
		/* T124 pcie lanes in X2_X1/X4_X1 config based on them */
		lane_owner = tegra_get_lane_owner_info() >> 1;
		if (lane_owner == PCIE_LANES_X2_X1)
			val |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X2_X1;
		else {
			val |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X4_X1;
			if ((tegra_pcie.plat_data->port_status[1]) &&
				(lane_owner == PCIE_LANES_X4_X1))
				val &= ~AFI_PCIE_CONFIG_PCIEC1_DISABLE_DEVICE;
		}
	}
	afi_writel(val, AFI_PCIE_CONFIG);

	/* Enable Gen 2 capability of PCIE */
	val = afi_readl(AFI_FUSE) & ~AFI_FUSE_PCIE_T0_GEN2_DIS;
	afi_writel(val, AFI_FUSE);

	/* Finally enable PCIe */
	val = afi_readl(AFI_CONFIGURATION);
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
	/* deassert PEX reset signal */
	for (i = 0; i < ARRAY_SIZE(pex_controller_registers); i++) {
		val = afi_readl(pex_controller_registers[i]);
		val |= AFI_PEX_CTRL_RST;
		afi_writel(val, pex_controller_registers[i]);
	}
	return ret;
}

#ifdef USE_REGULATORS
static int tegra_pcie_enable_regulators(void)
{
	PR_FUNC_LINE;
	if (tegra_pcie.power_rails_enabled) {
		pr_debug("PCIE: Already power rails enabled");
		return 0;
	}
	tegra_pcie.power_rails_enabled = 1;

	if (tegra_pcie.regulator_hvdd == NULL) {
		pr_info("PCIE.C: %s : regulator hvdd_pex\n", __func__);
		tegra_pcie.regulator_hvdd =
			regulator_get(tegra_pcie.dev, "hvdd_pex");
		if (IS_ERR(tegra_pcie.regulator_hvdd)) {
			pr_err("%s: unable to get hvdd_pex regulator\n",
					__func__);
			tegra_pcie.regulator_hvdd = 0;
		}
	}

	if (tegra_pcie.regulator_pexio == NULL) {
		pr_info("PCIE.C: %s : regulator pexio\n", __func__);
		tegra_pcie.regulator_pexio =
			regulator_get(tegra_pcie.dev, "avdd_pex_pll");
		if (IS_ERR(tegra_pcie.regulator_pexio)) {
			pr_err("%s: unable to get pexio regulator\n", __func__);
			tegra_pcie.regulator_pexio = 0;
		}
	}

	/*SATA and PCIE use same PLLE, In default configuration,
	* and we set default AVDD_PLLE with SATA.
	* So if use default board, you have to turn on (LDO2) AVDD_PLLE.
	 */
	if (tegra_pcie.regulator_avdd_plle == NULL) {
		pr_info("PCIE.C: %s : regulator avdd_plle\n", __func__);
		tegra_pcie.regulator_avdd_plle = regulator_get(tegra_pcie.dev,
						"avdd_pll_erefe");
		if (IS_ERR(tegra_pcie.regulator_avdd_plle)) {
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

	PR_FUNC_LINE;
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
#endif

static int tegra_pcie_power_ungate(void)
{
	int err;

	PR_FUNC_LINE;
	err = tegra_unpowergate_partition_with_clk_on(TEGRA_POWERGATE_PCIE);
	if (err) {
		pr_err("PCIE: powerup sequence failed: %d\n", err);
		return err;
	}
	err = clk_prepare_enable(tegra_pcie.pcie_mselect);
	if (err) {
		pr_err("PCIE: mselect clk enable failed: %d\n", err);
		return err;
	}
	clk_set_rate(tegra_pcie.pcie_mselect, 408000000);
	/* pciex is reset only but need to be enabled for dvfs support */
	err = clk_enable(tegra_pcie.pcie_xclk);
	if (err) {
		pr_err("PCIE: pciex clk enable failed: %d\n", err);
		return err;
	}
	return 0;
}

static int tegra_pcie_map_resources(void)
{
	PR_FUNC_LINE;
	/* Allocate config space virtual memory */
	tegra_pcie.regs = ioremap_nocache(TEGRA_PCIE_BASE, PCIE_REGS_SZ);
	if (tegra_pcie.regs == NULL) {
		pr_err("PCIE: Failed to map PCI/AFI registers\n");
		return -ENOMEM;
	}

	return 0;
}

void tegra_pcie_unmap_resources(void)
{
	PR_FUNC_LINE;
	if (tegra_pcie.regs) {
		iounmap(tegra_pcie.regs);
		tegra_pcie.regs = 0;
	}
}

static bool tegra_pcie_is_fpga_pcie(void)
{
#define CLK_RST_BOND_OUT_REG		0x60006078
#define CLK_RST_BOND_OUT_REG_PCIE	(1 << 6)
	static int val;

	PR_FUNC_LINE;
	if (!val)
		val = readl(ioremap(CLK_RST_BOND_OUT_REG, 4));
	/* return if current netlist does not contain PCIE */
	if (val & CLK_RST_BOND_OUT_REG_PCIE)
		return false;
	return true;
}

static int tegra_pcie_fpga_phy_init(void)
{
#define FPGA_GEN2_SPEED_SUPPORT		0x90000001

	PR_FUNC_LINE;
	if (!tegra_pcie_is_fpga_pcie())
		return -ENODEV;

	/* Do reset for FPGA pcie phy */
	afi_writel(AFI_WR_SCRATCH_0_RESET_VAL, AFI_WR_SCRATCH_0);
	udelay(10);
	afi_writel(AFI_WR_SCRATCH_0_DEFAULT_VAL, AFI_WR_SCRATCH_0);
	udelay(10);
	afi_writel(AFI_WR_SCRATCH_0_RESET_VAL, AFI_WR_SCRATCH_0);

	/* required for gen2 speed support on FPGA */
	rp_writel(FPGA_GEN2_SPEED_SUPPORT, NV_PCIE2_RP_VEND_XP_BIST, 0);

	return 0;
}

static void tegra_pcie_pme_turnoff(void)
{
	unsigned int data;

	PR_FUNC_LINE;
	if (tegra_platform_is_fpga() && !tegra_pcie_is_fpga_pcie())
		return;
	data = afi_readl(AFI_PCIE_PME);
	data |= AFI_PCIE_PME_TURN_OFF;
	afi_writel(data, AFI_PCIE_PME);
	do {
		data = afi_readl(AFI_PCIE_PME);
	} while (!(data & AFI_PCIE_PME_ACK));

	/* Required for PLL power down */
	data = afi_readl(AFI_PLLE_CONTROL);
	data |= AFI_PLLE_CONTROL_BYPASS_PADS2PLLE_CONTROL;
	afi_writel(data, AFI_PLLE_CONTROL);
}

static struct tegra_io_dpd pexbias_io = {
	.name			= "PEX_BIAS",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 4,
};
static struct tegra_io_dpd pexclk1_io = {
	.name			= "PEX_CLK1",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 5,
};
static struct tegra_io_dpd pexclk2_io = {
	.name			= "PEX_CLK2",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 6,
};
static int tegra_pcie_power_on(void)
{
	int err = 0;

	PR_FUNC_LINE;
	if (tegra_pcie.pcie_power_enabled) {
		pr_debug("PCIE: Already powered on");
		goto err_exit;
	}
	tegra_pcie.pcie_power_enabled = 1;
	pm_runtime_get_sync(tegra_pcie.dev);

	if (!tegra_platform_is_fpga()) {
		/* disable PEX IOs DPD mode to turn on pcie */
		tegra_io_dpd_disable(&pexbias_io);
		tegra_io_dpd_disable(&pexclk1_io);
		tegra_io_dpd_disable(&pexclk2_io);
	}
	err = tegra_pcie_power_ungate();
	if (err) {
		pr_err("PCIE: Failed to power ungate\n");
		goto err_exit;
	}
	err = tegra_pcie_map_resources();
	if (err) {
		pr_err("PCIE: Failed to map resources\n");
		goto err_exit;
	}
	if (tegra_platform_is_fpga()) {
		err = tegra_pcie_fpga_phy_init();
		if (err)
			pr_err("PCIE: Failed to initialize FPGA Phy\n");
	}

err_exit:
	if (err)
		pm_runtime_put(tegra_pcie.dev);
	return err;
}

static int tegra_pcie_power_off(void)
{
	int err = 0;

	PR_FUNC_LINE;
	if (tegra_pcie.pcie_power_enabled == 0) {
		pr_debug("PCIE: Already powered off");
		goto err_exit;
	}
	tegra_pcie_prsnt_map_override(0, false);
	tegra_pcie_pme_turnoff();
	tegra_pcie_enable_pads(false);
	tegra_pcie_unmap_resources();
	if (tegra_pcie.pcie_mselect)
		clk_disable(tegra_pcie.pcie_mselect);
	if (tegra_pcie.pcie_xclk)
		clk_disable(tegra_pcie.pcie_xclk);
	err = tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_PCIE);
	if (err)
		goto err_exit;

	if (!tegra_platform_is_fpga()) {
		/* put PEX pads into DPD mode to save additional power */
		tegra_io_dpd_enable(&pexbias_io);
		tegra_io_dpd_enable(&pexclk1_io);
		tegra_io_dpd_enable(&pexclk2_io);
	}
	pm_runtime_put(tegra_pcie.dev);

	tegra_pcie.pcie_power_enabled = 0;
err_exit:
	return err;
}

static int tegra_pcie_clocks_get(void)
{
	PR_FUNC_LINE;
	/* get the PCIEXCLK */
	tegra_pcie.pcie_xclk = clk_get_sys("tegra_pcie", "pciex");
	if (IS_ERR_OR_NULL(tegra_pcie.pcie_xclk)) {
		pr_err("%s: unable to get PCIE Xclock\n", __func__);
		return -EINVAL;
	}
	tegra_pcie.pcie_mselect = clk_get_sys("tegra_pcie", "mselect");
	if (IS_ERR_OR_NULL(tegra_pcie.pcie_mselect)) {
		pr_err("%s: unable to get PCIE mselect clock\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static void tegra_pcie_clocks_put(void)
{
	PR_FUNC_LINE;
	if (tegra_pcie.pcie_xclk)
		clk_put(tegra_pcie.pcie_xclk);
	if (tegra_pcie.pcie_mselect)
		clk_put(tegra_pcie.pcie_mselect);
}

static int tegra_pcie_get_resources(void)
{
	int err;

	PR_FUNC_LINE;
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

	PR_FUNC_LINE;
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

static void tegra_pcie_apply_sw_war(int index, bool enum_done)
{
	unsigned int data;
	struct pci_dev *pdev = NULL;

	PR_FUNC_LINE;
	if (enum_done) {
		/* disable msi for port driver to avoid panic */
		for_each_pci_dev(pdev)
			if (pci_pcie_type(pdev) == PCI_EXP_TYPE_ROOT_PORT)
				pdev->msi_enabled = 0;
	} else {
		/* WAR for Eye diagram failure on lanes for T124 platforms */
		data = rp_readl(NV_PCIE2_RP_ECTL_1_R2, index);
		data |= PCIE2_RP_ECTL_1_R2_TX_DRV_CNTL_1C;
		rp_writel(data, NV_PCIE2_RP_ECTL_1_R2, index);
	}
}

/* Enable various features of root port */
static void tegra_pcie_enable_rp_features(int index)
{
	unsigned int data;

	PR_FUNC_LINE;
	/* Power mangagement settings */
	/* Enable clock clamping by default and enable card detect */
	data = rp_readl(NV_PCIE2_RP_PRIV_MISC, index);
	data |= PCIE2_RP_PRIV_MISC_CTLR_CLK_CLAMP_THRESHOLD |
		PCIE2_RP_PRIV_MISC_CTLR_CLK_CLAMP_ENABLE |
		PCIE2_RP_PRIV_MISC_TMS_CLK_CLAMP_THRESHOLD |
		PCIE2_RP_PRIV_MISC_TMS_CLK_CLAMP_ENABLE;
	rp_writel(data, NV_PCIE2_RP_PRIV_MISC, index);

	/* Enable ASPM - L1 state support by default */
	data = rp_readl(NV_PCIE2_RP_VEND_XP1, index);
	data |= NV_PCIE2_RP_VEND_XP1_LINK_PVT_CTL_L1_ASPM_SUPPORT;
	rp_writel(data, NV_PCIE2_RP_VEND_XP1, index);

	/* LTSSM wait for DLLP to finish before entering L1 or L2/L3 */
	/* to avoid truncating of PM mesgs resulting in reciever errors */
	data = rp_readl(NV_PCIE2_RP_VEND_XP_BIST, index);
	data |= PCIE2_RP_VEND_XP_BIST_GOTO_L1_L2_AFTER_DLLP_DONE;
	rp_writel(data, NV_PCIE2_RP_VEND_XP_BIST, index);

	/* unhide AER capability */
	data = rp_readl(NV_PCIE2_RP_VEND_CTL1, index);
	data |= PCIE2_RP_VEND_CTL1_ERPT;
	rp_writel(data, NV_PCIE2_RP_VEND_CTL1, index);

	tegra_pcie_apply_sw_war(index, false);
}

static void tegra_pcie_disable_ctlr(int index)
{
	u32 data;

	PR_FUNC_LINE;
	data = afi_readl(AFI_PCIE_CONFIG);
	if (index)
		data |= AFI_PCIE_CONFIG_PCIEC1_DISABLE_DEVICE;
	else
		data |= AFI_PCIE_CONFIG_PCIEC0_DISABLE_DEVICE;
	afi_writel(data, AFI_PCIE_CONFIG);
}

static void tegra_pcie_add_port(int index, u32 offset, u32 reset_reg)
{
	struct tegra_pcie_port *pp;

	PR_FUNC_LINE;
	tegra_pcie_prsnt_map_override(index, true);

	pp = tegra_pcie.port + tegra_pcie.num_ports;
	pp->index = -1;
	pp->base = tegra_pcie.regs + offset;
	pp->link_up = tegra_pcie_check_link(pp, index, reset_reg);

	if (!pp->link_up) {
		pp->base = NULL;
		pr_info("PCIE: port %d: link down, ignoring\n", index);
		tegra_pcie_disable_ctlr(index);
		return;
	}
	tegra_pcie_enable_rp_features(index);

	tegra_pcie.num_ports++;
	pp->index = index;
	/* initialize root bus in boot path only */
	if (!resume_path)
		pp->root_bus_nr = -1;
}

void tegra_pcie_check_ports(void)
{
	int port, rp_offset = 0;
	int ctrl_offset = AFI_PEX0_CTRL;

	PR_FUNC_LINE;
	/* reset number of ports */
	tegra_pcie.num_ports = 0;

	for (port = 0; port < MAX_PCIE_SUPPORTED_PORTS; port++) {
		ctrl_offset += (port * 8);
		rp_offset = (rp_offset + RP_OFFSET) * port;
		if (tegra_pcie.plat_data->port_status[port])
			tegra_pcie_add_port(port, rp_offset, ctrl_offset);
	}
}
EXPORT_SYMBOL(tegra_pcie_check_ports);

static int tegra_pcie_conf_gpios(void)
{
	int irq, err = 0;

	PR_FUNC_LINE;
	if (tegra_pcie.plat_data->gpio_hot_plug != -1) {
		/* configure gpio for hotplug detection */
		pr_info("acquiring hotplug_detect = %d\n",
				tegra_pcie.plat_data->gpio_hot_plug);
		err = gpio_request(tegra_pcie.plat_data->gpio_hot_plug,
					"pcie_hotplug_detect");
		if (err < 0) {
			pr_err("%s: gpio_request failed %d\n", __func__, err);
			return err;
		}
		err = gpio_direction_input(
				tegra_pcie.plat_data->gpio_hot_plug);
		if (err < 0) {
			pr_err("%s: gpio_direction_input failed %d\n",
				__func__, err);
			goto err_hot_plug;
		}
		irq = gpio_to_irq(tegra_pcie.plat_data->gpio_hot_plug);
		if (irq < 0) {
			pr_err("Unable to get irq for hotplug_detect\n");
			goto err_hot_plug;
		}
		err = request_irq((unsigned int)irq,
				gpio_pcie_detect_isr,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"pcie_hotplug_detect",
				(void *)tegra_pcie.plat_data);
		if (err < 0) {
			pr_err("Unable to claim irq for hotplug_detect\n");
			goto err_hot_plug;
		}
	}
	if (tegra_pcie.plat_data->gpio_x1_slot != -1) {
		err = gpio_request(
			tegra_pcie.plat_data->gpio_x1_slot, "pcie_x1_slot");
		if (err < 0) {
			pr_err("%s: pcie_x1_slot gpio_request failed %d\n",
					__func__, err);
			goto err_hot_plug;
		}
		err = gpio_direction_output(
			tegra_pcie.plat_data->gpio_x1_slot, 1);
		if (err < 0) {
			pr_err("%s: pcie_x1_slot gpio_direction_output failed %d\n",
					__func__, err);
			goto err_x1;
		}
		gpio_set_value_cansleep(
			tegra_pcie.plat_data->gpio_x1_slot, 1);
	}
	return 0;

err_x1:
	gpio_free(tegra_pcie.plat_data->gpio_x1_slot);
err_hot_plug:
	if (tegra_pcie.plat_data->gpio_hot_plug != -1)
		gpio_free(tegra_pcie.plat_data->gpio_hot_plug);
	return err;
}

static int tegra_pcie_scale_voltage(bool isGen2)
{
	unsigned long rate;
	int err;

	PR_FUNC_LINE;
	if (isGen2) {
		/* Scale up voltage for Gen2 speed */
		rate = 500000000;
	} else {
		/* Scale down voltage for Gen1 speed */
		rate = 250000000;
	}
	err = clk_set_rate(tegra_pcie.pcie_xclk, rate);
	return err;
}

static bool tegra_pcie_change_link_speed(struct pci_dev *pdev, bool isGen2)
{
	u16 val, link_up_spd, link_dn_spd;
	struct pci_dev *up_dev, *dn_dev;

	PR_FUNC_LINE;
	/* skip if current device is not PCI express capable */
	/* or is either a root port or downstream port */
	if (!pci_is_pcie(pdev))
		goto skip;
	if ((pci_pcie_type(pdev) == PCI_EXP_TYPE_DOWNSTREAM) ||
		(pci_pcie_type(pdev) == PCI_EXP_TYPE_ROOT_PORT))
		goto skip;

	/* initialize upstream/endpoint and downstream/root port device ptr */
	up_dev = pdev;
	dn_dev = pdev->bus->self;

	/* read link status register to find current speed */
	pcie_capability_read_word(up_dev, PCI_EXP_LNKSTA, &link_up_spd);
	link_up_spd &= PCI_EXP_LNKSTA_CLS;
	pcie_capability_read_word(dn_dev, PCI_EXP_LNKSTA, &link_dn_spd);
	link_dn_spd &= PCI_EXP_LNKSTA_CLS;

	/* skip if both devices across the link are already trained to gen2 */
	if (isGen2 &&
		(link_up_spd == RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN2) &&
		(link_dn_spd == RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN2))
		goto skip;
	/* skip if both devices across the link are already trained to gen1 */
	else if (!isGen2 &&
		((link_up_spd == RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN1) ||
		(link_dn_spd == RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN1)))
		goto skip;

	/* read link capability register to find max speed supported */
	pcie_capability_read_word(up_dev, PCI_EXP_LNKCAP, &link_up_spd);
	link_up_spd &= PCI_EXP_LNKCAP_SLS;
	pcie_capability_read_word(dn_dev, PCI_EXP_LNKCAP, &link_dn_spd);
	link_dn_spd &= PCI_EXP_LNKCAP_SLS;

	/* skip if any device across the link is not supporting gen2 speed */
	if (isGen2 &&
		((link_up_spd < RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN2) ||
		(link_dn_spd < RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN2)))
		goto skip;
	/* skip if any device across the link is not supporting gen1 speed */
	else if (!isGen2 &&
		((link_up_spd < RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN1) ||
		(link_dn_spd < RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN1)))
		goto skip;

	/* Set Link Speed */
	pcie_capability_read_word(dn_dev, PCI_EXP_LNKCTL2, &val);
	val &= ~RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_MASK;
	if (isGen2)
		val |= RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN2;
	else
		val |= RP_LINK_CONTROL_STATUS_2_TGT_LINK_SPD_GEN1;
	pcie_capability_write_word(dn_dev, PCI_EXP_LNKCTL2, val);

	/* Retrain the link */
	pcie_capability_read_word(dn_dev, PCI_EXP_LNKCTL, &val);
	val |= RP_LINK_CONTROL_STATUS_RETRAIN_LINK;
	pcie_capability_write_word(dn_dev, PCI_EXP_LNKCTL, val);

	return true;
skip:
	return false;
}

bool tegra_pcie_link_speed(bool isGen2)
{
	struct pci_dev *pdev = NULL;
	bool ret = false;

	PR_FUNC_LINE;
	/* Voltage scaling should happen before any device transition */
	/* to Gen2 or after all devices has transitioned to Gen1 */
	if (isGen2)
		if (tegra_pcie_scale_voltage(isGen2))
			return ret;

	for_each_pci_dev(pdev) {
		if (tegra_pcie_change_link_speed(pdev, isGen2))
			ret = true;
	}
	if (!isGen2)
		if (tegra_pcie_scale_voltage(isGen2))
			ret = false;

	return ret;
}
EXPORT_SYMBOL(tegra_pcie_link_speed);

/* support PLL power down in L1 dynamically based on platform */
static void tegra_pcie_pll_pdn(void)
{
	struct pci_dev *pdev = NULL;

	PR_FUNC_LINE;
	/* CLKREQ# to PD if device connected to RP doesn't have CLKREQ# */
	/* capability(no PLL power down in L1 here) and PU if they have */
	for_each_pci_dev(pdev) {
		if (pci_pcie_type(pdev) == PCI_EXP_TYPE_ROOT_PORT)
			continue;

		if ((pci_pcie_type(pdev->bus->self) ==
			PCI_EXP_TYPE_ROOT_PORT)) {
			u32 val = 0;

			pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &val);
			if (val & PCI_EXP_LNKCAP_CLKPM) {
				tegra_pinmux_set_pullupdown(
					TEGRA_PINGROUP_PEX_L0_CLKREQ_N,
					TEGRA_PUPD_PULL_UP);
				tegra_pinmux_set_pullupdown(
					TEGRA_PINGROUP_PEX_L1_CLKREQ_N,
					TEGRA_PUPD_PULL_UP);
			} else {
				tegra_pinmux_set_pullupdown(
					TEGRA_PINGROUP_PEX_L0_CLKREQ_N,
					TEGRA_PUPD_PULL_DOWN);
				tegra_pinmux_set_pullupdown(
					TEGRA_PINGROUP_PEX_L1_CLKREQ_N,
					TEGRA_PUPD_PULL_DOWN);
			}
			break;
		}
	}
}

/* Enable ASPM support of all devices based on it's capability */
static void tegra_pcie_enable_aspm(void)
{
	struct pci_dev *pdev = NULL;
	u16 val = 0, aspm = 0;

	PR_FUNC_LINE;
	for_each_pci_dev(pdev) {
		/* Find ASPM capability */
		pcie_capability_read_word(pdev, PCI_EXP_LNKCAP, &aspm);
		aspm &= PCI_EXP_LNKCAP_ASPMS;

		/* Enable ASPM support as per capability */
		pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &val);
		val |= aspm >> 10;
		pcie_capability_write_word(pdev, PCI_EXP_LNKCTL, val);
	}
}

static void tegra_pcie_enable_features(void)
{
	PR_FUNC_LINE;

	/* configure all links to gen2 speed by default */
	if (!tegra_pcie_link_speed(true))
		pr_info("PCIE: No Link speed change happened\n");

	tegra_pcie_pll_pdn();
	tegra_pcie_enable_aspm();
	tegra_pcie_apply_sw_war(0, true);
}

static int __init tegra_pcie_init(void)
{
	int err = 0;

	pcibios_min_mem = 0x03000000ul;
	pcibios_min_io = 0x1000ul;

	PR_FUNC_LINE;
	INIT_LIST_HEAD(&tegra_pcie.busses);
	INIT_WORK(&tegra_pcie.hotplug_detect, work_hotplug_handler);
	err = tegra_pcie_get_resources();
	if (err) {
		pr_err("PCIE: get resources failed\n");
		return err;
	}
	err = tegra_pcie_enable_pads(true);
	if (err) {
		pr_err("PCIE: enable pads failed\n");
		return err;
	}
	err = tegra_pcie_enable_controller();
	if (err) {
		pr_err("PCIE: enable controller failed\n");
		return err;
	}
	err = tegra_pcie_conf_gpios();
	if (err) {
		pr_err("PCIE: configuring gpios failed\n");
		return err;
	}
	/* setup the AFI address translations */
	tegra_pcie_setup_translations();
	tegra_pcie_check_ports();

	if (tegra_pcie.num_ports)
		pci_common_init(&tegra_pcie_hw);
	else {
		err = tegra_pcie_power_off();
		if (err < 0) {
			pr_err("Unable to power off pcie\n");
			return err;
		}
	}
	tegra_pcie_enable_features();
	/* register pcie device as wakeup source */
	device_init_wakeup(tegra_pcie.dev, true);

	return 0;
}

static int __init tegra_pcie_probe(struct platform_device *pdev)
{
	int ret;

	PR_FUNC_LINE;
	tegra_pcie.dev = &pdev->dev;
	tegra_pcie.plat_data = pdev->dev.platform_data;
	dev_dbg(&pdev->dev, "PCIE.C: %s : _port_status[0] %d\n",
		__func__, tegra_pcie.plat_data->port_status[0]);
	dev_dbg(&pdev->dev, "PCIE.C: %s : _port_status[1] %d\n",
		__func__, tegra_pcie.plat_data->port_status[1]);

	/* Enable Runtime PM for PCIe, TODO: Need to add PCIe host device */
	pm_runtime_enable(tegra_pcie.dev);

	ret = tegra_pcie_init();
	if (ret)
		tegra_pd_remove_device(tegra_pcie.dev);

	return ret;
}

#ifdef CONFIG_PM
static int tegra_pcie_suspend_noirq(struct device *dev)
{
	int ret = 0;

	PR_FUNC_LINE;
	/* configure PE_WAKE signal as wake sources */
	if ((tegra_pcie.plat_data->gpio_wake != -1) &&
			device_may_wakeup(dev)) {
		ret = enable_irq_wake(gpio_to_irq(
			tegra_pcie.plat_data->gpio_wake));
		if (ret < 0) {
			dev_err(dev,
				"ID wake-up event failed with error %d\n", ret);
			return ret;
		}
	}
	return tegra_pcie_power_off();
}

static int tegra_pcie_resume_noirq(struct device *dev)
{
	int ret = 0;

	PR_FUNC_LINE;
	resume_path = true;

	if ((tegra_pcie.plat_data->gpio_wake != -1) &&
			device_may_wakeup(dev)) {
		ret = disable_irq_wake(gpio_to_irq(
			tegra_pcie.plat_data->gpio_wake));
		if (ret < 0) {
			dev_err(dev,
				"ID wake-up event failed with error %d\n", ret);
			return ret;
		}
	}
	ret = tegra_pcie_power_on();
	if (ret) {
		pr_err("PCIE: Failed to power on: %d\n", ret);
		return ret;
	}
	tegra_pcie_enable_pads(true);
	tegra_pcie_enable_controller();
	tegra_pcie_setup_translations();

	tegra_pcie_check_ports();
	if (!tegra_pcie.num_ports) {
		tegra_pcie_power_off();
		goto exit;
	}
	resume_path = false;

exit:
	return 0;
}

static int tegra_pcie_resume(struct device *dev)
{
	PR_FUNC_LINE;
	tegra_pcie_enable_features();
	return 0;
}
#endif

static int tegra_pcie_remove(struct platform_device *pdev)
{
	struct tegra_pcie_bus *bus;

	PR_FUNC_LINE;
	list_for_each_entry(bus, &tegra_pcie.busses, list) {
		vunmap(bus->area->addr);
		kfree(bus);
	}
	tegra_pcie_detach();
	tegra_pd_remove_device(tegra_pcie.dev);

	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops tegra_pcie_pm_ops = {
	.suspend_noirq  = tegra_pcie_suspend_noirq,
	.resume_noirq = tegra_pcie_resume_noirq,
	.resume = tegra_pcie_resume,
	};
#endif

/* driver data is accessed after init, so use __refdata instead of __initdata */
static struct platform_driver __refdata tegra_pcie_driver = {
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
	if (tegra_cpu_is_asim())
		return 0;
	return platform_driver_register(&tegra_pcie_driver);
}

static void __exit_refok tegra_pcie_exit_driver(void)
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

	PR_FUNC_LINE;
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
					pr_info("unexpected MSI (1)\n");
			} else {
				/* that's weird who triggered this?*/
				/* just clear it*/
				pr_info("unexpected MSI (2)\n");
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

	PR_FUNC_LINE;
	/* this only happens once. */
	if (resume_path) {
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

	afi_writel(msi_aligned>>8, AFI_MSI_FPCI_BAR_ST);
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

	resume_path = true;
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

	PR_FUNC_LINE;
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

	PR_FUNC_LINE;
	for (i = 0; i < MSI_MAP_SIZE; i++) {
		if ((msi_map[i].used) && (msi_map[i].irq == irq)) {
			irq_free_desc(msi_map[i].irq);
			msi_map_release(msi_map + i);
			break;
		}
	}
}
