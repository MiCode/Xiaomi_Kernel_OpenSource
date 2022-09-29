// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek PCIe host controller driver.
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Jianjun Wang <jianjun.wang@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <trace/hooks/traps.h>

#include "../pci.h"
#include "../../misc/mediatek/clkbuf/v1/inc/mtk_clkbuf_ctl.h"

/* pextp register, CG,HW mode */
#define PCIE_PEXTP_CG_0			0x14
#define PEXTP_PWRCTL_0			0x40
#define PCIE_HW_MTCMOS_EN_P0		BIT(0)
#define PEXTP_PWRCTL_1			0x44
#define PCIE_HW_MTCMOS_EN_P1		BIT(0)
#define PEXTP_RSV_0			0x60
#define PCIE_HW_MTCMOS_EN_MD_P0		BIT(0)
#define PEXTP_RSV_1			0x64
#define PCIE_HW_MTCMOS_EN_MD_P1		BIT(0)

#define PEXTP_SW_RST			0x4
#define PEXTP_SW_RST_SET_OFFSET		0x8
#define PEXTP_SW_RST_CLR_OFFSET		0xc
#define PEXTP_SW_RST_MAC0_BIT		BIT(0)
#define PEXTP_SW_RST_PHY0_BIT		BIT(1)
#define PEXTP_SW_MAC0_PHY0_BIT \
	(PEXTP_SW_RST_MAC0_BIT | PEXTP_SW_RST_PHY0_BIT)
#define PEXTP_SW_RST_MAC1_BIT		BIT(8)
#define PEXTP_SW_RST_PHY1_BIT		BIT(9)
#define PEXTP_SW_MAC1_PHY1_BIT \
	(PEXTP_SW_RST_MAC1_BIT | PEXTP_SW_RST_PHY1_BIT)

#define PCIE_BASIC_STATUS		0x18

#define PCIE_SETTING_REG		0x80
#define PCIE_PCI_IDS_1			0x9c
#define PCI_CLASS(class)		(class << 8)
#define PCIE_RC_MODE			BIT(0)

#define PCIE_CFGNUM_REG			0x140
#define PCIE_CFG_DEVFN(devfn)		((devfn) & GENMASK(7, 0))
#define PCIE_CFG_BUS(bus)		(((bus) << 8) & GENMASK(15, 8))
#define PCIE_CFG_BYTE_EN(bytes)		(((bytes) << 16) & GENMASK(19, 16))
#define PCIE_CFG_FORCE_BYTE_EN		BIT(20)
#define PCIE_CFG_OFFSET_ADDR		0x1000
#define PCIE_CFG_HEADER(bus, devfn) \
	(PCIE_CFG_BUS(bus) | PCIE_CFG_DEVFN(devfn))

#define PCIE_RST_CTRL_REG		0x148
#define PCIE_MAC_RSTB			BIT(0)
#define PCIE_PHY_RSTB			BIT(1)
#define PCIE_BRG_RSTB			BIT(2)
#define PCIE_PE_RSTB			BIT(3)

#define PCIE_LTSSM_STATUS_REG		0x150
#define PCIE_LTSSM_STATE_MASK		GENMASK(28, 24)
#define PCIE_LTSSM_STATE(val)		((val & PCIE_LTSSM_STATE_MASK) >> 24)
#define PCIE_LTSSM_STATE_L2_IDLE	0x14

#define PCIE_LINK_STATUS_REG		0x154
#define PCIE_PORT_LINKUP		BIT(8)

#define PCIE_ASPM_CTRL			0x15c
#define PCIE_P2_EXIT_BY_CLKREQ		BIT(17)
#define PCIE_P2_IDLE_TIME_MASK		GENMASK(27, 24)
#define PCIE_P2_IDLE_TIME(x)		((x << 24) & PCIE_P2_IDLE_TIME_MASK)

#define PCIE_MSI_SET_NUM		8
#define PCIE_MSI_IRQS_PER_SET		32
#define PCIE_MSI_IRQS_NUM \
	(PCIE_MSI_IRQS_PER_SET * PCIE_MSI_SET_NUM)

#define PCIE_INT_ENABLE_REG		0x180
#define PCIE_MSI_ENABLE			GENMASK(PCIE_MSI_SET_NUM + 8 - 1, 8)
#define PCIE_MSI_SHIFT			8
#define PCIE_INTX_SHIFT			24
#define PCIE_INTX_ENABLE \
	GENMASK(PCIE_INTX_SHIFT + PCI_NUM_INTX - 1, PCIE_INTX_SHIFT)

#define PCIE_INT_STATUS_REG		0x184
#define PCIE_AXIERR_COMPL_TIMEOUT	BIT(18)
#define PCIE_MSI_SET_ENABLE_REG		0x190
#define PCIE_MSI_SET_ENABLE		GENMASK(PCIE_MSI_SET_NUM - 1, 0)

#define PCIE_MSI_SET_BASE_REG		0xc00
#define PCIE_MSI_SET_OFFSET		0x10
#define PCIE_MSI_SET_STATUS_OFFSET	0x04
#define PCIE_MSI_SET_ENABLE_OFFSET	0x08
#define PCIE_MSI_SET_ENABLE_GRP1_OFFSET	0x0c

#define PCIE_MSI_SET_ADDR_HI_BASE	0xc80
#define PCIE_MSI_SET_ADDR_HI_OFFSET	0x04

#define PCIE_MSI_GRP2_SET_OFFSET	0xDC0
#define PCIE_MSI_GRPX_PER_SET_OFFSET	4
#define PCIE_MSI_GRP3_SET_OFFSET	0xDE0

#define PCIE_AXI0_ERR_ADDR_L		0xe00
#define PCIE_AXI0_ERR_INFO		0xe08

#define PCIE_ICMD_PM_REG		0x198
#define PCIE_TURN_OFF_LINK		BIT(4)

#define PCIE_ISTATUS_PM			0x19C
#define PCIE_L1PM_SM			GENMASK(10, 8)

#define PCIE_MISC_CTRL_REG		0x348
#define PCIE_DVFS_REQ_FORCE_ON		BIT(1)
#define PCIE_MAC_SLP_DIS		BIT(7)
#define PCIE_DVFS_REQ_FORCE_OFF		BIT(12)

#define PCIE_TRANS_TABLE_BASE_REG	0x800
#define PCIE_ATR_SRC_ADDR_MSB_OFFSET	0x4
#define PCIE_ATR_TRSL_ADDR_LSB_OFFSET	0x8
#define PCIE_ATR_TRSL_ADDR_MSB_OFFSET	0xc
#define PCIE_ATR_TRSL_PARAM_OFFSET	0x10
#define PCIE_ATR_TLB_SET_OFFSET		0x20

#define PCIE_MAX_TRANS_TABLES		8
#define PCIE_ATR_EN			BIT(0)
#define PCIE_ATR_SIZE(size) \
	(((((size) - 1) << 1) & GENMASK(6, 1)) | PCIE_ATR_EN)
#define PCIE_ATR_ID(id)			((id) & GENMASK(3, 0))
#define PCIE_ATR_TYPE_MEM		PCIE_ATR_ID(0)
#define PCIE_ATR_TYPE_IO		PCIE_ATR_ID(1)
#define PCIE_ATR_TLP_TYPE(type)		(((type) << 16) & GENMASK(18, 16))
#define PCIE_ATR_TLP_TYPE_MEM		PCIE_ATR_TLP_TYPE(0)
#define PCIE_ATR_TLP_TYPE_IO		PCIE_ATR_TLP_TYPE(2)

/* pcie read completion timeout */
#define PCIE_CONF_DEV2_CTL_STS		0x10a8
#define PCIE_DCR2_CPL_TO		GENMASK(3, 0)
#define PCIE_CPL_TIMEOUT_4MS		0x2

/* PHY sif register */
#define PCIE_PHY_SIF			0x11100000
#define PEXTP_DIG_GLB_28		0x28
#define RG_XTP_PHY_CLKREQ_N_IN		GENMASK(13, 12)
#define PEXTP_DIG_GLB_50		0x50
#define RG_XTP_CKM_EN_L1S0		BIT(13)

/* PHY ckm register */
#define PCIE_PHY_CKM			0x11110000
#define XTP_CKM_DA_REG_3C		0x3C
#define RG_CKM_PADCK_REQ		GENMASK(13, 12)

/* vlpcfg register */
#define PCIE_VLPCFG_BASE		0x1C00C000
#define PCIE_VLP_AXI_PROTECT_STA	0x240
#define PCIE_MAC0_SLP_READY_MASK	BIT(11)

enum mtk_pcie_suspend_link_state {
	LINK_STATE_L12 = 0,
	LINK_STATE_L2,
};

/**
 * struct mtk_msi_set - MSI information for each set
 * @base: IO mapped register base
 * @msg_addr: MSI message address
 * @saved_irq_state: IRQ enable state saved at suspend time
 */
struct mtk_msi_set {
	void __iomem *base;
	phys_addr_t msg_addr;
	u32 saved_irq_state;
};

/**
 * struct mtk_pcie_port - PCIe port information
 * @dev: pointer to PCIe device
 * @base: IO mapped register base
 * @pextpcfg: pextpcfg_ao(pcie HW MTCMOS) IO mapped register base
 * @vlpcfg_base: vlpcfg(bus protect ready) IO mapped register base
 * @reg_base: physical register base
 * @mac_reset: MAC reset control
 * @phy_reset: PHY reset control
 * @phy: PHY controller block
 * @clks: PCIe clocks
 * @num_clks: PCIe clocks count for this port
 * @port_num: serial number of pcie port
 * @suspend_mode: pcie enter low poer mode when the system enter suspend
 * @dvfs_req_en: pcie wait request to reply ack when pcie exit from P2 state
 * @peri_reset_en: clear peri pcie reset to open pcie phy & mac
 * @irq: PCIe controller interrupt number
 * @saved_irq_state: IRQ enable state saved at suspend time
 * @irq_lock: lock protecting IRQ register access
 * @intx_domain: legacy INTx IRQ domain
 * @msi_domain: MSI IRQ domain
 * @msi_bottom_domain: MSI IRQ bottom domain
 * @msi_sets: MSI sets information
 * @lock: lock protecting IRQ bit map
 * @vote_lock: lock protecting vote HW control mode
 * @ep_hw_mode_en: flag of ep control hw mode
 * @rc_hw_mode_en: flag of rc control hw mode
 * @msi_irq_in_use: bit map for assigned MSI IRQ
 */
struct mtk_pcie_port {
	struct device *dev;
	void __iomem *base;
	void __iomem *pextpcfg;
	void __iomem *vlpcfg_base;
	phys_addr_t reg_base;
	struct reset_control *mac_reset;
	struct reset_control *phy_reset;
	struct phy *phy;
	struct device *genpd_mac;
	struct device *genpd_phy;
	struct clk_bulk_data *clks;
	int num_clks;

	int port_num;
	u32 suspend_mode;
	bool dvfs_req_en;
	bool peri_reset_en;
	int irq;
	u32 saved_irq_state;
	raw_spinlock_t irq_lock;
	struct irq_domain *intx_domain;
	struct irq_domain *msi_domain;
	struct irq_domain *msi_bottom_domain;
	struct mtk_msi_set msi_sets[PCIE_MSI_SET_NUM];
	struct mutex lock;
	struct mutex vote_lock;
	bool ep_hw_mode_en;
	bool rc_hw_mode_en;
	DECLARE_BITMAP(msi_irq_in_use, PCIE_MSI_IRQS_NUM);
};

/**
 * mtk_pcie_config_tlp_header() - Configure a configuration TLP header
 * @bus: PCI bus to query
 * @devfn: device/function number
 * @where: offset in config space
 * @size: data size in TLP header
 *
 * Set byte enable field and device information in configuration TLP header.
 */
static void mtk_pcie_config_tlp_header(struct pci_bus *bus, unsigned int devfn,
					int where, int size)
{
	struct mtk_pcie_port *port = bus->sysdata;
	int bytes;
	u32 val;

	bytes = (GENMASK(size - 1, 0) & 0xf) << (where & 0x3);

	val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(bytes) |
	      PCIE_CFG_HEADER(bus->number, devfn);

	writel_relaxed(val, port->base + PCIE_CFGNUM_REG);
}

static void __iomem *mtk_pcie_map_bus(struct pci_bus *bus, unsigned int devfn,
				      int where)
{
	struct mtk_pcie_port *port = bus->sysdata;

	return port->base + PCIE_CFG_OFFSET_ADDR + where;
}

static int mtk_pcie_config_read(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	mtk_pcie_config_tlp_header(bus, devfn, where, size);

	return pci_generic_config_read32(bus, devfn, where, size, val);
}

static int mtk_pcie_config_write(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	mtk_pcie_config_tlp_header(bus, devfn, where, size);

	if (size <= 2)
		val <<= (where & 0x3) * 8;

	return pci_generic_config_write32(bus, devfn, where, 4, val);
}

static struct pci_ops mtk_pcie_ops = {
	.map_bus = mtk_pcie_map_bus,
	.read  = mtk_pcie_config_read,
	.write = mtk_pcie_config_write,
};

static int mtk_pcie_set_trans_table(struct mtk_pcie_port *port,
				    resource_size_t cpu_addr,
				    resource_size_t pci_addr,
				    resource_size_t size,
				    unsigned long type, int num)
{
	void __iomem *table;
	u32 val;

	if (num >= PCIE_MAX_TRANS_TABLES) {
		dev_err(port->dev, "not enough translate table for addr: %#llx, limited to [%d]\n",
			(unsigned long long)cpu_addr, PCIE_MAX_TRANS_TABLES);
		return -ENODEV;
	}

	table = port->base + PCIE_TRANS_TABLE_BASE_REG +
		num * PCIE_ATR_TLB_SET_OFFSET;

	writel_relaxed(lower_32_bits(cpu_addr) | PCIE_ATR_SIZE(fls(size) - 1),
		       table);
	writel_relaxed(upper_32_bits(cpu_addr),
		       table + PCIE_ATR_SRC_ADDR_MSB_OFFSET);
	writel_relaxed(lower_32_bits(pci_addr),
		       table + PCIE_ATR_TRSL_ADDR_LSB_OFFSET);
	writel_relaxed(upper_32_bits(pci_addr),
		       table + PCIE_ATR_TRSL_ADDR_MSB_OFFSET);

	if (type == IORESOURCE_IO)
		val = PCIE_ATR_TYPE_IO | PCIE_ATR_TLP_TYPE_IO;
	else
		val = PCIE_ATR_TYPE_MEM | PCIE_ATR_TLP_TYPE_MEM;

	writel_relaxed(val, table + PCIE_ATR_TRSL_PARAM_OFFSET);

	return 0;
}

static void mtk_pcie_enable_msi(struct mtk_pcie_port *port)
{
	int i;
	u32 val;

	for (i = 0; i < PCIE_MSI_SET_NUM; i++) {
		struct mtk_msi_set *msi_set = &port->msi_sets[i];

		msi_set->base = port->base + PCIE_MSI_SET_BASE_REG +
				i * PCIE_MSI_SET_OFFSET;
		msi_set->msg_addr = port->reg_base + PCIE_MSI_SET_BASE_REG +
				    i * PCIE_MSI_SET_OFFSET;

		/* Configure the MSI capture address */
		writel_relaxed(lower_32_bits(msi_set->msg_addr), msi_set->base);
		writel_relaxed(upper_32_bits(msi_set->msg_addr),
			       port->base + PCIE_MSI_SET_ADDR_HI_BASE +
			       i * PCIE_MSI_SET_ADDR_HI_OFFSET);
	}

	val = readl_relaxed(port->base + PCIE_MSI_SET_ENABLE_REG);
	val |= PCIE_MSI_SET_ENABLE;
	writel_relaxed(val, port->base + PCIE_MSI_SET_ENABLE_REG);

	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val |= PCIE_MSI_ENABLE;
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);
}

static void __maybe_unused mtk_pcie_mt6985_fixup(void)
{
	void __iomem *pcie_phy_sif;
	void __iomem *pcie_phy_ckm;
	u32 val;

	pcie_phy_sif = ioremap(PCIE_PHY_SIF, 0x100);
	pcie_phy_ckm = ioremap(PCIE_PHY_CKM, 0x100);

	val = readl(pcie_phy_sif + PEXTP_DIG_GLB_28);
	val |= RG_XTP_PHY_CLKREQ_N_IN;
	writel(val, pcie_phy_sif + PEXTP_DIG_GLB_28);

	val = readl(pcie_phy_sif + PEXTP_DIG_GLB_50);
	val &= ~RG_XTP_CKM_EN_L1S0;
	writel(val, pcie_phy_sif + PEXTP_DIG_GLB_50);

	val = readl(pcie_phy_ckm + XTP_CKM_DA_REG_3C);
	val |= RG_CKM_PADCK_REQ;
	writel(val, pcie_phy_ckm + XTP_CKM_DA_REG_3C);

	pr_info("PHY GLB_28=%#x, GLB_50=%#x, CKM_3C=%#x\n",
		readl(pcie_phy_sif + PEXTP_DIG_GLB_28),
		readl(pcie_phy_sif + PEXTP_DIG_GLB_50),
		readl(pcie_phy_ckm + XTP_CKM_DA_REG_3C));

	iounmap(pcie_phy_sif);
	iounmap(pcie_phy_ckm);
}

static int mtk_pcie_startup_port(struct mtk_pcie_port *port)
{
	struct resource_entry *entry;
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	unsigned int table_index = 0;
	int err;
	u32 val;

	/* Set as RC mode */
	val = readl_relaxed(port->base + PCIE_SETTING_REG);
	val |= PCIE_RC_MODE;
	writel_relaxed(val, port->base + PCIE_SETTING_REG);

	/* Set class code */
	val = readl_relaxed(port->base + PCIE_PCI_IDS_1);
	val &= ~GENMASK(31, 8);
	val |= PCI_CLASS(PCI_CLASS_BRIDGE_PCI << 8);
	writel_relaxed(val, port->base + PCIE_PCI_IDS_1);

	if (port->pextpcfg) {
		mutex_init(&port->vote_lock);

		port->vlpcfg_base = ioremap(PCIE_VLPCFG_BASE, 0x1000);
		port->ep_hw_mode_en = false;
		port->rc_hw_mode_en = false;

		val = readl_relaxed(port->base + PCIE_ASPM_CTRL);
		val &= ~PCIE_P2_IDLE_TIME_MASK;
		val |= PCIE_P2_EXIT_BY_CLKREQ | PCIE_P2_IDLE_TIME(8);
		writel_relaxed(val, port->base + PCIE_ASPM_CTRL);

		mtk_pcie_mt6985_fixup();

		/* Software enable BBCK2 */
		clk_buf_voter_ctrl_by_id(7, SW_FPM);
	}

	/* Mask all INTx interrupts */
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val &= ~PCIE_INTX_ENABLE;
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);

	/* DVFSRC voltage request state */
	val = readl_relaxed(port->base + PCIE_MISC_CTRL_REG);
	val |= PCIE_DVFS_REQ_FORCE_ON;
	if (!port->dvfs_req_en) {
		val &= ~PCIE_DVFS_REQ_FORCE_ON;
		val |= PCIE_DVFS_REQ_FORCE_OFF;
	}
	writel_relaxed(val, port->base + PCIE_MISC_CTRL_REG);

	/* Assert all reset signals */
	val = readl_relaxed(port->base + PCIE_RST_CTRL_REG);
	val |= PCIE_MAC_RSTB | PCIE_PHY_RSTB | PCIE_BRG_RSTB | PCIE_PE_RSTB;
	writel_relaxed(val, port->base + PCIE_RST_CTRL_REG);

	/*
	 * Described in PCIe CEM specification setctions 2.2 (PERST# Signal)
	 * and 2.2.1 (Initial Power-Up (G3 to S0)).
	 * The deassertion of PERST# should be delayed 100ms (TPVPERL)
	 * for the power and clock to become stable.
	 */
	msleep(100);

	/* De-assert reset signals */
	val &= ~(PCIE_MAC_RSTB | PCIE_PHY_RSTB | PCIE_BRG_RSTB | PCIE_PE_RSTB);
	writel_relaxed(val, port->base + PCIE_RST_CTRL_REG);

	/* Check if the link is up or not */
	err = readl_poll_timeout(port->base + PCIE_LINK_STATUS_REG, val,
				 !!(val & PCIE_PORT_LINKUP), 20,
				 PCI_PM_D3COLD_WAIT * USEC_PER_MSEC);
	if (err) {
		val = readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG);
		dev_err(port->dev, "PCIe link down, ltssm reg val: %#x\n", val);
		return err;
	}

	mtk_pcie_enable_msi(port);

	if (port->pextpcfg) {
		/* PCIe read completion timeout is adjusted to 4ms */
		val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(0xf) |
		      PCIE_CFG_HEADER(0, 0);
		writel_relaxed(val, port->base + PCIE_CFGNUM_REG);
		val = readl_relaxed(port->base + PCIE_CONF_DEV2_CTL_STS);
		val &= ~PCIE_DCR2_CPL_TO;
		val |= PCIE_CPL_TIMEOUT_4MS;
		writel_relaxed(val, port->base + PCIE_CONF_DEV2_CTL_STS);
		pr_info("PCIe RC control 2 register=%#x",
			readl_relaxed(port->base + PCIE_CONF_DEV2_CTL_STS));
	}

	/* Set PCIe translation windows */
	resource_list_for_each_entry(entry, &host->windows) {
		struct resource *res = entry->res;
		unsigned long type = resource_type(res);
		resource_size_t cpu_addr;
		resource_size_t pci_addr;
		resource_size_t size;
		const char *range_type;

		if (type == IORESOURCE_IO) {
			cpu_addr = pci_pio_to_address(res->start);
			range_type = "IO";
		} else if (type == IORESOURCE_MEM) {
			cpu_addr = res->start;
			range_type = "MEM";
		} else {
			continue;
		}

		pci_addr = res->start - entry->offset;
		size = resource_size(res);
		err = mtk_pcie_set_trans_table(port, cpu_addr, pci_addr, size,
					       type, table_index);
		if (err)
			return err;

		dev_dbg(port->dev, "set %s trans window[%d]: cpu_addr = %#llx, pci_addr = %#llx, size = %#llx\n",
			range_type, table_index, (unsigned long long)cpu_addr,
			(unsigned long long)pci_addr, (unsigned long long)size);

		table_index++;
	}

	return 0;
}

static int mtk_pcie_set_affinity(struct irq_data *data,
				 const struct cpumask *mask, bool force)
{
	struct mtk_pcie_port *port = data->domain->host_data;
	struct irq_data *port_data = irq_get_irq_data(port->irq);
	struct irq_chip *port_chip = irq_data_get_irq_chip(port_data);
	int ret;

	if (!port_chip || !port_chip->irq_set_affinity)
		return -EINVAL;

	ret = port_chip->irq_set_affinity(port_data, mask, force);

	irq_data_update_effective_affinity(data, mask);

	return ret;
}

static void mtk_pcie_msi_irq_mask(struct irq_data *data)
{
	pci_msi_mask_irq(data);
	irq_chip_mask_parent(data);
}

static void mtk_pcie_msi_irq_unmask(struct irq_data *data)
{
	pci_msi_unmask_irq(data);
	irq_chip_unmask_parent(data);
}

static struct irq_chip mtk_msi_irq_chip = {
	.irq_ack = irq_chip_ack_parent,
	.irq_mask = mtk_pcie_msi_irq_mask,
	.irq_unmask = mtk_pcie_msi_irq_unmask,
	.name = "MSI",
};

static struct msi_domain_info mtk_msi_domain_info = {
	.flags	= (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		   MSI_FLAG_PCI_MSIX | MSI_FLAG_MULTI_PCI_MSI),
	.chip	= &mtk_msi_irq_chip,
};

static void mtk_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_port *port = data->domain->host_data;
	unsigned long hwirq;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	msg->address_hi = upper_32_bits(msi_set->msg_addr);
	msg->address_lo = lower_32_bits(msi_set->msg_addr);
	msg->data = hwirq;
	dev_dbg(port->dev, "msi#%#lx address_hi %#x address_lo %#x data %d\n",
		hwirq, msg->address_hi, msg->address_lo, msg->data);
}

static void mtk_msi_bottom_irq_ack(struct irq_data *data)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	unsigned long hwirq;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	writel_relaxed(BIT(hwirq), msi_set->base + PCIE_MSI_SET_STATUS_OFFSET);
}

static void mtk_msi_bottom_irq_mask(struct irq_data *data)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_port *port = data->domain->host_data;
	unsigned long hwirq, flags;
	u32 val;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	val &= ~BIT(hwirq);
	writel_relaxed(val, msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static void mtk_msi_bottom_irq_unmask(struct irq_data *data)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_port *port = data->domain->host_data;
	unsigned long hwirq, flags;
	u32 val;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	val |= BIT(hwirq);
	writel_relaxed(val, msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static struct irq_chip mtk_msi_bottom_irq_chip = {
	.irq_ack		= mtk_msi_bottom_irq_ack,
	.irq_mask		= mtk_msi_bottom_irq_mask,
	.irq_unmask		= mtk_msi_bottom_irq_unmask,
	.irq_compose_msi_msg	= mtk_compose_msi_msg,
	.irq_set_affinity	= mtk_pcie_set_affinity,
	.name			= "MSI",
};

static int mtk_msi_bottom_domain_alloc(struct irq_domain *domain,
				       unsigned int virq, unsigned int nr_irqs,
				       void *arg)
{
	struct mtk_pcie_port *port = domain->host_data;
	struct mtk_msi_set *msi_set;
	int i, hwirq, set_idx;

	mutex_lock(&port->lock);

	hwirq = bitmap_find_free_region(port->msi_irq_in_use, PCIE_MSI_IRQS_NUM,
					order_base_2(nr_irqs));

	mutex_unlock(&port->lock);

	if (hwirq < 0)
		return -ENOSPC;

	set_idx = hwirq / PCIE_MSI_IRQS_PER_SET;
	msi_set = &port->msi_sets[set_idx];

	for (i = 0; i < nr_irqs; i++)
		irq_domain_set_info(domain, virq + i, hwirq + i,
				    &mtk_msi_bottom_irq_chip, msi_set,
				    handle_edge_irq, NULL, NULL);

	return 0;
}

static void mtk_msi_bottom_domain_free(struct irq_domain *domain,
				       unsigned int virq, unsigned int nr_irqs)
{
	struct mtk_pcie_port *port = domain->host_data;
	struct irq_data *data = irq_domain_get_irq_data(domain, virq);

	mutex_lock(&port->lock);

	bitmap_release_region(port->msi_irq_in_use, data->hwirq,
			      order_base_2(nr_irqs));

	mutex_unlock(&port->lock);

	irq_domain_free_irqs_common(domain, virq, nr_irqs);
}

static const struct irq_domain_ops mtk_msi_bottom_domain_ops = {
	.alloc = mtk_msi_bottom_domain_alloc,
	.free = mtk_msi_bottom_domain_free,
};

static void mtk_intx_mask(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val &= ~BIT(data->hwirq + PCIE_INTX_SHIFT);
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static void mtk_intx_unmask(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val |= BIT(data->hwirq + PCIE_INTX_SHIFT);
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

/**
 * mtk_intx_eoi() - Clear INTx IRQ status at the end of interrupt
 * @data: pointer to chip specific data
 *
 * As an emulated level IRQ, its interrupt status will remain
 * until the corresponding de-assert message is received; hence that
 * the status can only be cleared when the interrupt has been serviced.
 */
static void mtk_intx_eoi(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	unsigned long hwirq;

	hwirq = data->hwirq + PCIE_INTX_SHIFT;
	writel_relaxed(BIT(hwirq), port->base + PCIE_INT_STATUS_REG);
}

static struct irq_chip mtk_intx_irq_chip = {
	.irq_mask		= mtk_intx_mask,
	.irq_unmask		= mtk_intx_unmask,
	.irq_eoi		= mtk_intx_eoi,
	.irq_set_affinity	= mtk_pcie_set_affinity,
	.name			= "INTx",
};

static int mtk_pcie_intx_map(struct irq_domain *domain, unsigned int irq,
			     irq_hw_number_t hwirq)
{
	irq_set_chip_data(irq, domain->host_data);
	irq_set_chip_and_handler_name(irq, &mtk_intx_irq_chip,
				      handle_fasteoi_irq, "INTx");
	return 0;
}

static const struct irq_domain_ops intx_domain_ops = {
	.map = mtk_pcie_intx_map,
};

static int mtk_pcie_init_irq_domains(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct device_node *intc_node, *node = dev->of_node;
	int ret;

	raw_spin_lock_init(&port->irq_lock);

	/* Setup INTx */
	intc_node = of_get_child_by_name(node, "interrupt-controller");
	if (!intc_node) {
		dev_err(dev, "missing interrupt-controller node\n");
		return -ENODEV;
	}

	port->intx_domain = irq_domain_add_linear(intc_node, PCI_NUM_INTX,
						  &intx_domain_ops, port);
	if (!port->intx_domain) {
		dev_err(dev, "failed to create INTx IRQ domain\n");
		return -ENODEV;
	}

	/* Setup MSI */
	mutex_init(&port->lock);

	port->msi_bottom_domain = irq_domain_add_linear(node, PCIE_MSI_IRQS_NUM,
				  &mtk_msi_bottom_domain_ops, port);
	if (!port->msi_bottom_domain) {
		dev_err(dev, "failed to create MSI bottom domain\n");
		ret = -ENODEV;
		goto err_msi_bottom_domain;
	}

	port->msi_domain = pci_msi_create_irq_domain(dev->fwnode,
						     &mtk_msi_domain_info,
						     port->msi_bottom_domain);
	if (!port->msi_domain) {
		dev_err(dev, "failed to create MSI domain\n");
		ret = -ENODEV;
		goto err_msi_domain;
	}

	return 0;

err_msi_domain:
	irq_domain_remove(port->msi_bottom_domain);
err_msi_bottom_domain:
	irq_domain_remove(port->intx_domain);

	return ret;
}

static void mtk_pcie_irq_teardown(struct mtk_pcie_port *port)
{
	irq_set_chained_handler_and_data(port->irq, NULL, NULL);

	if (port->intx_domain) {
		int virq, i;

		for (i = 0; i < PCI_NUM_INTX; i++) {
			virq = irq_find_mapping(port->intx_domain, i);
			if (virq > 0)
				irq_dispose_mapping(virq);
		}
		irq_domain_remove(port->intx_domain);
	}

	if (port->msi_domain)
		irq_domain_remove(port->msi_domain);

	if (port->msi_bottom_domain)
		irq_domain_remove(port->msi_bottom_domain);

	irq_dispose_mapping(port->irq);
}

static void mtk_pcie_msi_handler(struct mtk_pcie_port *port, int set_idx)
{
	struct mtk_msi_set *msi_set = &port->msi_sets[set_idx];
	unsigned long msi_enable, msi_status;
	irq_hw_number_t bit, hwirq;

	msi_enable = readl_relaxed(msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);

	do {
		msi_status = readl_relaxed(msi_set->base +
					   PCIE_MSI_SET_STATUS_OFFSET);
		msi_status &= msi_enable;
		if (!msi_status)
			break;

		for_each_set_bit(bit, &msi_status, PCIE_MSI_IRQS_PER_SET) {
			hwirq = bit + set_idx * PCIE_MSI_IRQS_PER_SET;
			generic_handle_domain_irq(port->msi_bottom_domain, hwirq);
		}
	} while (true);
}

static void mtk_pcie_irq_handler(struct irq_desc *desc)
{
	struct mtk_pcie_port *port = irq_desc_get_handler_data(desc);
	struct irq_chip *irqchip = irq_desc_get_chip(desc);
	unsigned long status;
	irq_hw_number_t irq_bit = PCIE_INTX_SHIFT;

	chained_irq_enter(irqchip, desc);

	status = readl_relaxed(port->base + PCIE_INT_STATUS_REG);
	for_each_set_bit_from(irq_bit, &status, PCI_NUM_INTX +
			      PCIE_INTX_SHIFT)
		generic_handle_domain_irq(port->intx_domain,
					  irq_bit - PCIE_INTX_SHIFT);

	irq_bit = PCIE_MSI_SHIFT;
	for_each_set_bit_from(irq_bit, &status, PCIE_MSI_SET_NUM +
			      PCIE_MSI_SHIFT) {
		mtk_pcie_msi_handler(port, irq_bit - PCIE_MSI_SHIFT);

		writel_relaxed(BIT(irq_bit), port->base + PCIE_INT_STATUS_REG);
	}

	chained_irq_exit(irqchip, desc);
}

static int mtk_pcie_setup_irq(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct platform_device *pdev = to_platform_device(dev);
	int err;

	err = mtk_pcie_init_irq_domains(port);
	if (err)
		return err;

	port->irq = platform_get_irq(pdev, 0);
	if (port->irq < 0)
		return port->irq;

	irq_set_chained_handler_and_data(port->irq, mtk_pcie_irq_handler, port);

	return 0;
}

static int mtk_pcie_parse_port(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *regs;
	struct device_node *pextp_node;
	int ret;

	regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie-mac");
	if (!regs)
		return -EINVAL;
	port->base = devm_ioremap_resource(dev, regs);
	if (IS_ERR(port->base)) {
		dev_err(dev, "failed to map register base\n");
		return PTR_ERR(port->base);
	}

	port->reg_base = regs->start;

	port->port_num = of_get_pci_domain_nr(dev->of_node);
	if (port->port_num < 0) {
		dev_info(dev, "failed to get domain number\n");
		return port->port_num;
	}

	port->dvfs_req_en = true;
	ret = of_property_read_bool(dev->of_node, "mediatek,dvfs-req-dis");
	if (ret)
		port->dvfs_req_en = false;

	port->peri_reset_en = true;
	ret = of_property_read_bool(dev->of_node, "mediatek,peri-reset-dis");
	if (ret)
		port->peri_reset_en = false;

	pextp_node = of_find_compatible_node(NULL, NULL,
					     "mediatek,mt6985-pextpcfg_ao");
	if (pextp_node) {
		port->pextpcfg = of_iomap(pextp_node, 0);
		if (IS_ERR(port->pextpcfg))
			return PTR_ERR(port->pextpcfg);
	}

	port->suspend_mode = LINK_STATE_L2;
	ret = of_property_read_bool(dev->of_node, "mediatek,suspend-mode-l12");
	if (ret)
		port->suspend_mode = LINK_STATE_L12;

	port->phy_reset = devm_reset_control_get_optional_exclusive(dev, "phy");
	if (IS_ERR(port->phy_reset)) {
		ret = PTR_ERR(port->phy_reset);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get PHY reset\n");

		return ret;
	}

	port->mac_reset = devm_reset_control_get_optional_exclusive(dev, "mac");
	if (IS_ERR(port->mac_reset)) {
		ret = PTR_ERR(port->mac_reset);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get MAC reset\n");

		return ret;
	}

	port->phy = devm_phy_optional_get(dev, "pcie-phy");
	if (IS_ERR(port->phy)) {
		ret = PTR_ERR(port->phy);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get PHY\n");

		return ret;
	}

	port->num_clks = devm_clk_bulk_get_all(dev, &port->clks);
	if (port->num_clks < 0) {
		dev_err(dev, "failed to get clocks\n");
		return port->num_clks;
	}

	port->genpd_mac = dev_pm_domain_attach_by_name(dev, "pd_mac");
	if (IS_ERR(port->genpd_mac)) {
		ret = PTR_ERR(port->genpd_mac);
		if (ret != -EPROBE_DEFER)
			dev_info(dev, "failed to attach MAC genpd\n");

		return ret;
	}

	port->genpd_phy = dev_pm_domain_attach_by_name(dev, "pd_phy");
	if (IS_ERR(port->genpd_phy)) {
		ret = PTR_ERR(port->genpd_phy);
		if (ret != -EPROBE_DEFER)
			dev_info(dev, "failed to attach PHY genpd\n");

		return ret;
	}

	return 0;
}

static int mtk_pcie_peri_reset(struct mtk_pcie_port *port, bool enable)
{
	struct arm_smccc_res res;
	struct device *dev = port->dev;

	arm_smccc_smc(MTK_SIP_KERNEL_PCIE_CONTROL, port->port_num, enable,
		      0, 0, 0, 0, 0, &res);

	if (res.a0)
		dev_info(dev, "Can't %s sw reset through SMC call\n",
			 enable ? "set" : "clear");

	return res.a0;
}

static int mtk_pcie_power_up(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	int err;

	/* Clear PCIe pextp sw reset bit */
	if (port->pextpcfg) {
		writel_relaxed(PEXTP_SW_MAC0_PHY0_BIT,
			       port->pextpcfg + PEXTP_SW_RST_CLR_OFFSET);
		writel_relaxed(PEXTP_SW_MAC1_PHY1_BIT,
			       port->pextpcfg + PEXTP_SW_RST_CLR_OFFSET);
	}

	/* Clear PCIe sw reset bit */
	if (port->peri_reset_en) {
		err = mtk_pcie_peri_reset(port, false);
		if (err) {
			dev_info(dev, "failed to clear PERI reset control bit\n");
			return err;
		}
	}

	/* PHY power on and enable pipe clock */
	reset_control_deassert(port->phy_reset);

	err = phy_init(port->phy);
	if (err) {
		dev_err(dev, "failed to initialize PHY\n");
		goto err_phy_init;
	}

	err = phy_power_on(port->phy);
	if (err) {
		dev_err(dev, "failed to power on PHY\n");
		goto err_phy_on;
	}

	/* MAC power on and enable transaction layer clocks */
	reset_control_deassert(port->mac_reset);

	if (port->genpd_phy)
		pm_runtime_get_sync(port->genpd_phy);

	if (port->genpd_mac)
		pm_runtime_get_sync(port->genpd_mac);

	err = clk_bulk_prepare_enable(port->num_clks, port->clks);
	if (err) {
		dev_err(dev, "failed to enable clocks\n");
		goto err_clk_init;
	}

	return 0;

err_clk_init:
	if (port->genpd_mac)
		pm_runtime_put_sync(port->genpd_mac);
	if (port->genpd_phy)
		pm_runtime_put_sync(port->genpd_phy);
	reset_control_assert(port->mac_reset);
	phy_power_off(port->phy);
err_phy_on:
	phy_exit(port->phy);
err_phy_init:
	reset_control_assert(port->phy_reset);

	return err;
}

static void mtk_pcie_power_down(struct mtk_pcie_port *port)
{
	clk_bulk_disable_unprepare(port->num_clks, port->clks);

	if (port->genpd_mac) {
		pm_runtime_put_sync(port->genpd_mac);
		dev_pm_domain_detach(port->genpd_mac, true);
	}

	if (port->genpd_phy) {
		pm_runtime_put_sync(port->genpd_phy);
		dev_pm_domain_detach(port->genpd_phy, true);
	}

	reset_control_assert(port->mac_reset);

	phy_power_off(port->phy);
	phy_exit(port->phy);
	reset_control_assert(port->phy_reset);

	/* Set PCIe sw reset bit */
	if (port->peri_reset_en)
		mtk_pcie_peri_reset(port, true);

	/* Set PCIe pextp sw reset bit */
	if (port->pextpcfg) {
		writel_relaxed(PEXTP_SW_MAC0_PHY0_BIT,
			       port->pextpcfg + PEXTP_SW_RST_SET_OFFSET);
		writel_relaxed(PEXTP_SW_MAC1_PHY1_BIT,
			       port->pextpcfg + PEXTP_SW_RST_SET_OFFSET);
	}

	/* BBCK2 is controlled by itself hardware mode */
	clk_buf_voter_ctrl_by_id(7, HW);
}

static int mtk_pcie_setup(struct mtk_pcie_port *port)
{
	int err;

	err = mtk_pcie_parse_port(port);
	if (err)
		return err;

	/* Don't touch the hardware registers before power up */
	err = mtk_pcie_power_up(port);
	if (err)
		return err;

	/* Try link up */
	err = mtk_pcie_startup_port(port);
	if (err)
		goto err_setup;

	err = mtk_pcie_setup_irq(port);
	if (err)
		goto err_setup;

	return 0;

err_setup:
	mtk_pcie_power_down(port);

	return err;
}

static int mtk_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_pcie_port *port;
	struct pci_host_bridge *host;
	int err;

	host = devm_pci_alloc_host_bridge(dev, sizeof(*port));
	if (!host)
		return -ENOMEM;

	port = pci_host_bridge_priv(host);

	port->dev = dev;
	platform_set_drvdata(pdev, port);

	err = mtk_pcie_setup(port);
	if (err)
		goto err_probe;

	host->ops = &mtk_pcie_ops;
	host->sysdata = port;

	err = pci_host_probe(host);
	if (err) {
		mtk_pcie_irq_teardown(port);
		mtk_pcie_power_down(port);
		goto err_probe;
	}

	return 0;

err_probe:
	pinctrl_pm_select_sleep_state(&pdev->dev);

	return err;
}

static int mtk_pcie_remove(struct platform_device *pdev)
{
	struct mtk_pcie_port *port = platform_get_drvdata(pdev);
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	int err = 0;

	pci_lock_rescan_remove();
	pci_stop_root_bus(host->bus);
	pci_remove_root_bus(host->bus);
	pci_unlock_rescan_remove();

	mtk_pcie_irq_teardown(port);
	mtk_pcie_power_down(port);

	err = pinctrl_pm_select_sleep_state(&pdev->dev);
	if (err) {
		dev_info(&pdev->dev, "Failed to set PCIe pins sleep state\n");
		return err;
	}

	return 0;
}

static struct device_node *mtk_pcie_find_node_by_port(int port)
{
	struct device_node *pcie_node = NULL;

	do {
		pcie_node = of_find_node_by_name(pcie_node, "pcie");
		if (port == of_get_pci_domain_nr(pcie_node))
			return pcie_node;
	} while (pcie_node);

	pr_info("pcie device node not found!\n");

	return NULL;
}

int mtk_pcie_probe_port(int port)
{
	struct device_node *pcie_node;
	struct platform_device *pdev;

	pcie_node = mtk_pcie_find_node_by_port(port);
	if (!pcie_node)
		return -ENODEV;

	pdev = of_find_device_by_node(pcie_node);
	if (!pdev) {
		pr_info("pcie platform device not found!\n");
		return -ENODEV;
	}

	if (device_attach(&pdev->dev) < 0) {
		device_release_driver(&pdev->dev);
		pr_info("%s: pcie probe fail!\n", __func__);
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_probe_port);

int mtk_pcie_remove_port(int port)
{
	struct device_node *pcie_node;
	struct platform_device *pdev;

	pcie_node = mtk_pcie_find_node_by_port(port);
	if (!pcie_node)
		return -ENODEV;

	pdev = of_find_device_by_node(pcie_node);
	if (!pdev) {
		pr_info("pcie platform device not found!\n");
		return -ENODEV;
	}

	device_release_driver(&pdev->dev);

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_remove_port);

#if IS_ENABLED(CONFIG_ANDROID_FIX_PCIE_SLAVE_ERROR)
static void pcie_android_rvh_do_serror(void *data, struct pt_regs *regs,
				       unsigned int esr, int *ret)
{
	struct device_node *pcie_node;
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	u32 val;

	pcie_node = mtk_pcie_find_node_by_port(0);
	if (!pcie_node) {
		pr_info("PCIe device node not found!\n");
		return;
	}

	pdev = of_find_device_by_node(pcie_node);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port) {
		pr_info("PCIe port not found!\n");
		return;
	}

	val = readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG);
	if (val & PCIE_AXIERR_COMPL_TIMEOUT) {
		*ret = 1;
		writel_relaxed(PCIE_AXIERR_COMPL_TIMEOUT,
			       pcie_port->base + PCIE_INT_STATUS_REG);
	}

	pr_info("ltssm reg: %#x, PCIe interrupt status=%#x, AXI0 ERROR address=%#x, AXI0 ERROR status=%#x\n",
		readl_relaxed(pcie_port->base + PCIE_LTSSM_STATUS_REG),
		readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG),
		readl_relaxed(pcie_port->base + PCIE_AXI0_ERR_ADDR_L),
		readl_relaxed(pcie_port->base + PCIE_AXI0_ERR_INFO));

	dump_stack();
}
#endif

/**
 * mtk_pcie_dump_link_info() - Dump PCIe RC information
 * @port: The port number which EP use
 * @ret_val: bit[4:0]: LTSSM state (PCIe MAC offset 0x150 bit[28:24])
 *           bit[5]: DL_UP state (PCIe MAC offset 0x154 bit[8])
 *           bit[6]: Completion timeout status (PCIe MAC offset 0x184 bit[18])
 */
u32 mtk_pcie_dump_link_info(int port)
{
	struct device_node *pcie_node;
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	u32 val, ret_val = 0;

	pcie_node = mtk_pcie_find_node_by_port(port);
	if (!pcie_node) {
		pr_info("PCIe device node not found!\n");
		return 0;
	}

	pdev = of_find_device_by_node(pcie_node);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return 0;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port) {
		pr_info("PCIe port not found!\n");
		return 0;
	}

	/* Check the sleep protect ready */
	val = readl_relaxed(pcie_port->vlpcfg_base + PCIE_VLP_AXI_PROTECT_STA);
	val &= PCIE_MAC0_SLP_READY_MASK;
	if (val)
		return 0;

	val = readl_relaxed(pcie_port->base + PCIE_LTSSM_STATUS_REG);
	ret_val |= PCIE_LTSSM_STATE(val);
	val = readl_relaxed(pcie_port->base + PCIE_LINK_STATUS_REG);
	ret_val |= (val >> 3) & BIT(5);
	val = readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG);
	ret_val |= (val >> 12) & BIT(6);

	pr_info("ltssm reg:%#x, link sta:%#x, power sta:%#x, IP basic sta:%#x, int sta:%#x, axi err add:%#x, axi err info:%#x\n",
		readl_relaxed(pcie_port->base + PCIE_LTSSM_STATUS_REG),
		readl_relaxed(pcie_port->base + PCIE_LINK_STATUS_REG),
		readl_relaxed(pcie_port->base + PCIE_ISTATUS_PM),
		readl_relaxed(pcie_port->base + PCIE_BASIC_STATUS),
		readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG),
		readl_relaxed(pcie_port->base + PCIE_AXI0_ERR_ADDR_L),
		readl_relaxed(pcie_port->base + PCIE_AXI0_ERR_INFO));
	pr_info("clock gate:%#x, PCIe HW MODE BIT:%#x, Modem HW MODE BIT:%#x\n",
		readl_relaxed(pcie_port->pextpcfg + PCIE_PEXTP_CG_0),
		readl_relaxed(pcie_port->pextpcfg + PEXTP_PWRCTL_0),
		readl_relaxed(pcie_port->pextpcfg + PEXTP_RSV_0));

	return ret_val;
}
EXPORT_SYMBOL(mtk_pcie_dump_link_info);

/**
 * mtk_msi_unmask_to_other_mcu() - Unmask msi dispatch to other mcu
 * @data: The irq_data of virq
 * @group: MSI will dispatch to which group number
 */
int mtk_msi_unmask_to_other_mcu(struct irq_data *data, u32 group)
{
	struct irq_data *parent_data = data->parent_data;
	struct mtk_msi_set *msi_set;
	struct mtk_pcie_port *port;
	void __iomem *dest_addr;
	unsigned long hwirq;
	u32 val, set_num;

	if (!parent_data)
		return -EINVAL;

	msi_set = irq_data_get_irq_chip_data(parent_data);
	if (!msi_set)
		return -ENODEV;

	port = parent_data->domain->host_data;
	hwirq = parent_data->hwirq % PCIE_MSI_IRQS_PER_SET;
	set_num = parent_data->hwirq / PCIE_MSI_IRQS_PER_SET;

	switch (group) {
	case 1:
		dest_addr = msi_set->base + PCIE_MSI_SET_ENABLE_GRP1_OFFSET;
		break;
	case 2:
		dest_addr = port->base + PCIE_MSI_GRP2_SET_OFFSET +
			    PCIE_MSI_GRPX_PER_SET_OFFSET * set_num;
		break;
	case 3:
		dest_addr = port->base + PCIE_MSI_GRP3_SET_OFFSET +
			    PCIE_MSI_GRPX_PER_SET_OFFSET * set_num;
		break;
	default:
		pr_info("Group %d out of max range\n", group);

		return -EINVAL;
	}

	val = readl_relaxed(dest_addr);
	val |= BIT(hwirq);
	writel_relaxed(val, dest_addr);

	pr_info("group=%d, hwirq=%ld, SET num=%d, Enable status=%#x\n",
		group, hwirq, set_num, readl_relaxed(dest_addr));

	return 0;
}
EXPORT_SYMBOL(mtk_msi_unmask_to_other_mcu);

/**
 * mtk_pcie_mask_msi_to_ap() - Disable msi dispatch to ap
 * @port: The port number which EP use
 * @msi_addr: EP message address register for msi
 * @mask: EP msi dispatch to modem, [like:0xFF00]
 */
int mtk_pcie_mask_msi_to_ap(int port, u32 msi_addr, u32 mask)
{
	struct device_node *pcie_node;
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	u32 offset, val;

	pcie_node = mtk_pcie_find_node_by_port(port);
	if (!pcie_node)
		return -ENODEV;

	pdev = of_find_device_by_node(pcie_node);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port)
		return -ENODEV;

	offset = msi_addr - pcie_port->reg_base;
	if (offset < PCIE_MSI_SET_BASE_REG || offset > PCIE_MSI_SET_ADDR_HI_BASE) {
		pr_info("Wrong MSI address: %#x\n", msi_addr);
		return -EINVAL;
	}

	val = readl_relaxed(pcie_port->base + offset + PCIE_MSI_SET_ENABLE_OFFSET);
	val &= ~mask;
	writel_relaxed(val, pcie_port->base + offset + PCIE_MSI_SET_ENABLE_OFFSET);

	pr_info("port=%d, MSI address=%#x, mask=%#x, Enable status=%#x\n",
		port, msi_addr, mask,
		readl_relaxed(pcie_port->base + offset + PCIE_MSI_SET_ENABLE_OFFSET));

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_mask_msi_to_ap);

static void __maybe_unused mtk_pcie_irq_save(struct mtk_pcie_port *port)
{
	int i;

	raw_spin_lock(&port->irq_lock);

	port->saved_irq_state = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);

	for (i = 0; i < PCIE_MSI_SET_NUM; i++) {
		struct mtk_msi_set *msi_set = &port->msi_sets[i];

		msi_set->saved_irq_state = readl_relaxed(msi_set->base +
					   PCIE_MSI_SET_ENABLE_OFFSET);
	}

	raw_spin_unlock(&port->irq_lock);
}

static void __maybe_unused mtk_pcie_irq_restore(struct mtk_pcie_port *port)
{
	int i;

	raw_spin_lock(&port->irq_lock);

	writel_relaxed(port->saved_irq_state, port->base + PCIE_INT_ENABLE_REG);

	for (i = 0; i < PCIE_MSI_SET_NUM; i++) {
		struct mtk_msi_set *msi_set = &port->msi_sets[i];

		writel_relaxed(msi_set->saved_irq_state,
			       msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	}

	raw_spin_unlock(&port->irq_lock);
}

static int __maybe_unused mtk_pcie_turn_off_link(struct mtk_pcie_port *port)
{
	u32 val;

	val = readl_relaxed(port->base + PCIE_ICMD_PM_REG);
	val |= PCIE_TURN_OFF_LINK;
	writel_relaxed(val, port->base + PCIE_ICMD_PM_REG);

	/* Check the link is L2 */
	return readl_poll_timeout(port->base + PCIE_LTSSM_STATUS_REG, val,
				  (PCIE_LTSSM_STATE(val) ==
				   PCIE_LTSSM_STATE_L2_IDLE), 20,
				   50 * USEC_PER_MSEC);
}

static void mtk_pcie_enable_hw_control(struct mtk_pcie_port *port, bool enable)
{
	u32 val;

	val = readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_0);
	if (enable)
		val |= PCIE_HW_MTCMOS_EN_P0;
	else
		val &= ~PCIE_HW_MTCMOS_EN_P0;

	writel_relaxed(val, port->pextpcfg + PEXTP_PWRCTL_0);

	if (enable)
		dev_info(port->dev, "PCIe HW MODE BIT=%#x\n",
			 readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_0));
}

/*
 * mtk_pcie_hw_control_vote() - Vote mechanism
 * @port: The port number which EP use
 * @hw_mode_en: vote mechanism, true: agree open hw mode;
 *        false: disagree open hw mode
 * @who: 0 is rc, 1 is wifi
 */
int mtk_pcie_hw_control_vote(int port, bool hw_mode_en, u8 who)
{
	struct device_node *pcie_node;
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	bool vote_hw_mode_en;
	int err = 0;
	u32 val;

	pcie_node = mtk_pcie_find_node_by_port(port);
	if (!pcie_node)
		return -ENODEV;

	pdev = of_find_device_by_node(pcie_node);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port)
		return -ENODEV;

	mutex_lock(&pcie_port->vote_lock);

	if (who) {
		if (hw_mode_en)
			pcie_port->ep_hw_mode_en = true;
		else
			pcie_port->ep_hw_mode_en = false;
	} else {
		if (hw_mode_en)
			pcie_port->rc_hw_mode_en = true;
		else
			pcie_port->rc_hw_mode_en = false;
	}

	vote_hw_mode_en = (pcie_port->ep_hw_mode_en && pcie_port->rc_hw_mode_en)
			   ? true : false;
	mtk_pcie_enable_hw_control(pcie_port, vote_hw_mode_en);

	mutex_unlock(&pcie_port->vote_lock);

	if (!vote_hw_mode_en) {
		/* Check the sleep protect ready */
		err = readl_poll_timeout(pcie_port->vlpcfg_base +
					 PCIE_VLP_AXI_PROTECT_STA, val,
					 !(val & PCIE_MAC0_SLP_READY_MASK),
					 20, 50 * USEC_PER_MSEC);
		if (err)
			dev_info(pcie_port->dev, "PCIe sleep protect not ready, %#x\n",
				 readl_relaxed(pcie_port->vlpcfg_base +
				 PCIE_VLP_AXI_PROTECT_STA));
	}

	return err;
}
EXPORT_SYMBOL(mtk_pcie_hw_control_vote);

static int __maybe_unused mtk_pcie_suspend_noirq(struct device *dev)
{
	struct mtk_pcie_port *port = dev_get_drvdata(dev);
	int err;
	u32 val;

	if (port->suspend_mode == LINK_STATE_L12) {
		val = readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG);
		dev_info(port->dev, "pcie LTSSM=%#x\n", val);
		val = readl_relaxed(port->base + PCIE_ISTATUS_PM);
		dev_info(port->dev, "pcie L1SS_pm=%#x\n", val);

		if (port->port_num == 0) {
			err = mtk_pcie_hw_control_vote(0, true, 0);
			if (err)
				return err;

			dev_info(port->dev, "Modem HW MODE BIT=%#x\n",
				 readl_relaxed(port->pextpcfg + PEXTP_RSV_0));
		} else if (port->port_num == 1) {
			val = readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_1);
			val |= PCIE_HW_MTCMOS_EN_P1;
			writel_relaxed(val, port->pextpcfg + PEXTP_PWRCTL_1);
		}

		/* BBCK2 is controlled by itself hardware mode */
		clk_buf_voter_ctrl_by_id(7, HW);
	} else {
		/* Trigger link to L2 state */
		err = mtk_pcie_turn_off_link(port);
		if (err) {
			dev_info(port->dev, "cannot enter L2 state\n");
			return err;
		}

		/* Pull down the PERST# pin */
		val = readl_relaxed(port->base + PCIE_RST_CTRL_REG);
		val |= PCIE_PE_RSTB;
		writel_relaxed(val, port->base + PCIE_RST_CTRL_REG);

		dev_dbg(port->dev, "entered L2 states successfully");

		mtk_pcie_irq_save(port);
		mtk_pcie_power_down(port);
	}

	return 0;
}

static int __maybe_unused mtk_pcie_resume_noirq(struct device *dev)
{
	struct mtk_pcie_port *port = dev_get_drvdata(dev);
	int err;
	u32 val;

	if (port->suspend_mode == LINK_STATE_L12) {
		/* Software enable BBCK2 */
		clk_buf_voter_ctrl_by_id(7, SW_FPM);

		if (port->port_num == 0) {
			err = mtk_pcie_hw_control_vote(0, false, 0);
			if (err)
				return err;

			dev_info(port->dev, "Modem HW MODE BIT=%#x\n",
				 readl_relaxed(port->pextpcfg + PEXTP_RSV_0));
		} else if (port->port_num == 1) {
			val = readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_1);
			val &= ~PCIE_HW_MTCMOS_EN_P1;
			writel_relaxed(val, port->pextpcfg + PEXTP_PWRCTL_1);
		}
	} else {
		err = mtk_pcie_power_up(port);
		if (err)
			return err;

		err = mtk_pcie_startup_port(port);
		if (err) {
			mtk_pcie_power_down(port);
			return err;
		}

		mtk_pcie_irq_restore(port);
	}

	return 0;
}

static const struct dev_pm_ops mtk_pcie_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_pcie_suspend_noirq,
				      mtk_pcie_resume_noirq)
};

static const struct of_device_id mtk_pcie_of_match[] = {
	{ .compatible = "mediatek,mt8192-pcie" },
	{ .compatible = "mediatek,mt6985-pcie" },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_pcie_of_match);

static struct platform_driver mtk_pcie_driver = {
	.probe = mtk_pcie_probe,
	.remove = mtk_pcie_remove,
	.driver = {
		.name = "mtk-pcie",
		.of_match_table = mtk_pcie_of_match,
		.pm = &mtk_pcie_pm_ops,
	},
};

static int mtk_pcie_init_func(void *pvdev)
{
#if IS_ENABLED(CONFIG_ANDROID_FIX_PCIE_SLAVE_ERROR)
	int err = 0;

	err = register_trace_android_rvh_do_serror(
			pcie_android_rvh_do_serror, NULL);
	if (err)
		pr_info("register pcie android_rvh_do_serror failed!\n");
#endif

	return platform_driver_register(&mtk_pcie_driver);
}

static int __init mtk_pcie_init(void)
{
	struct task_struct *driver_thread_handle;

	driver_thread_handle = kthread_run(mtk_pcie_init_func,
					   NULL, "pcie_thread");

	if (IS_ERR(driver_thread_handle))
		return PTR_ERR(driver_thread_handle);

	return 0;
}

static void __exit mtk_pcie_exit(void)
{
	platform_driver_unregister(&mtk_pcie_driver);
}

module_init(mtk_pcie_init);
module_exit(mtk_pcie_exit);
MODULE_LICENSE("GPL v2");
