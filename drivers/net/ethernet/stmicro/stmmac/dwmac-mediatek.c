// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */
#include <linux/bitfield.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/stmmac.h>

#include "stmmac.h"
#include "stmmac_platform.h"

/* Peri Configuration register for mt2712 */
#define PERI_ETH_PHY_INTF_SEL	0x418
#define PHY_INTF_MII		0
#define PHY_INTF_RGMII		1
#define PHY_INTF_RMII		4
#define RMII_CLK_SRC_RXC	BIT(4)
#define RMII_CLK_SRC_INTERNAL	BIT(5)

#define PERI_ETH_DLY	0x428
#define ETH_DLY_GTXC_INV	BIT(6)
#define ETH_DLY_GTXC_ENABLE	BIT(5)
#define ETH_DLY_GTXC_STAGES	GENMASK(4, 0)
#define ETH_DLY_TXC_INV		BIT(20)
#define ETH_DLY_TXC_ENABLE	BIT(19)
#define ETH_DLY_TXC_STAGES	GENMASK(18, 14)
#define ETH_DLY_RXC_INV		BIT(13)
#define ETH_DLY_RXC_ENABLE	BIT(12)
#define ETH_DLY_RXC_STAGES	GENMASK(11, 7)

#define PERI_ETH_DLY_FINE	0x800
#define ETH_RMII_DLY_TX_INV	BIT(2)
#define ETH_FINE_DLY_GTXC	BIT(1)
#define ETH_FINE_DLY_RXC	BIT(0)

struct mac_delay_struct {
	u32 tx_delay;
	u32 rx_delay;
	bool tx_inv;
	bool rx_inv;
};

struct mediatek_dwmac_plat_data {
	const struct mediatek_dwmac_variant *variant;
	struct mac_delay_struct mac_delay;
	struct clk_bulk_data *clks;
	struct device_node *np;
	struct regmap *peri_regmap;
	struct device *dev;
	int phy_mode;
	bool rmii_rxc;
};

struct mediatek_dwmac_variant {
	int (*dwmac_set_phy_interface)(struct mediatek_dwmac_plat_data *plat);
	int (*dwmac_set_delay)(struct mediatek_dwmac_plat_data *plat);

	/* clock ids to be requested */
	const char * const *clk_list;
	int num_clks;

	u32 dma_bit_mask;
	u32 rx_delay_max;
	u32 tx_delay_max;
};

/* list of clocks required for mac */
static const char * const mt2712_dwmac_clk_l[] = {
	"axi", "apb", "mac_main", "ptp_ref"
};

#ifdef CONFIG_DEBUG_FS

static int eth_smt_result;

#define MTK_ETH_FRAME_LEN (ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)

static int FRAME_PATTERN_CH[8] = {
	0x11111111,
	0x22222222,
	0x33333333,
	0x44444444,
	0x55555555,
	0x66666666,
	0x77777777,
	0x88888888,
};

static int frame_hdrs[8][4] = {
	/* for channel 0 : Non tagged header
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x800
	 */
	{0xFFFFFFFF, 0x5500FFFF, 0xF77DB57B, 0x00000081},

	/* for channel 1 : VLAN tagged header with priority 1
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0xFFFFFFFF, 0x5500FFFF, 0xF77DB57B, 0x64200081},

	/* for channel 2 : VLAN tagged header with priority 2
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0xFFFFFFFF, 0x5500FFFF, 0xF77DB57B, 0x64400081},

	/* for channel 3 : VLAN tagged header with priority 3
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64600081},

	/* for channel 4 : VLAN tagged header with priority 4
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64800081},

	/* for channel 5 : VLAN tagged header with priority 5
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64A00081},

	/* for channel 6 : VLAN tagged header with priority 6
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64C00081},

	/* for channel 7 : VLAN tagged header with priority 7
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64E00081},
};

static int send_frame(struct stmmac_priv *priv, int num)
{
	struct sk_buff *skb = NULL;
	unsigned int *skb_data = NULL;
	int payload_cnt = 0;
	int i, j, ret, cpu;
	unsigned int frame_size = 1500;
	unsigned int tx_octetcount_gb, tx_framecount_gb;
	unsigned int rx_framecount_gb, rx_octetcount_gb, rx_octetcount_g;
	const struct net_device_ops *netdev_ops = priv->dev->netdev_ops;

	usleep_range(40000, 50000);

	priv->mmc.mmc_tx_octetcount_gb += readl(priv->mmcaddr + 0x14);
	priv->mmc.mmc_tx_framecount_gb += readl(priv->mmcaddr + 0x18);
	priv->mmc.mmc_rx_framecount_gb += readl(priv->mmcaddr + 0x80);
	priv->mmc.mmc_rx_octetcount_gb += readl(priv->mmcaddr + 0x84);
	priv->mmc.mmc_rx_octetcount_g += readl(priv->mmcaddr + 0x88);
	tx_octetcount_gb = priv->mmc.mmc_tx_octetcount_gb;
	tx_framecount_gb = priv->mmc.mmc_tx_framecount_gb;
	rx_framecount_gb = priv->mmc.mmc_rx_framecount_gb;
	rx_octetcount_gb = priv->mmc.mmc_rx_octetcount_gb;
	rx_octetcount_g = priv->mmc.mmc_rx_octetcount_g;
	usleep_range(1000000, 2000000);
	for (j = 0; j < priv->plat->tx_queues_to_use; j++) {
		for (i = 0; i < num; i++) {
			struct netdev_queue *txq =
					netdev_get_tx_queue(priv->dev, j);

			payload_cnt = 0;
			skb = dev_alloc_skb(MTK_ETH_FRAME_LEN);
			if (!skb) {
				pr_err("Failed to allocate tx skb\n");
				return 1;
			}

			skb_set_queue_mapping(skb, j); /*  map skb to queue0 */
			skb_data = (unsigned int *)skb->data;
			/* Add Ethernet header */
			*skb_data++ = frame_hdrs[j][0];
			*skb_data++ = frame_hdrs[j][1];
			*skb_data++ = frame_hdrs[j][2];
			*skb_data++ = frame_hdrs[j][3];
			/* Add payload */
			for (payload_cnt = 0; payload_cnt < frame_size;) {
				*skb_data++ = FRAME_PATTERN_CH[j];
				/* increment by 4 since we are writing
				 * one dword at a time
				 */
				payload_cnt += 4;
			}
			skb->len = frame_size;
			cpu = smp_processor_id();
			do {
				/* Disable soft irqs for various locks below.
				 * Also stops preemption for RCU.
				 * avoid dql bug-on issue.
				 */
				rcu_read_lock_bh();
				HARD_TX_LOCK(priv->dev, txq, cpu);
				ret = netdev_ops->ndo_start_xmit(skb,
								 priv->dev);
				if (ret == NETDEV_TX_OK)
					txq_trans_update(txq);
				HARD_TX_UNLOCK(priv->dev, txq);
				rcu_read_unlock_bh();
			} while (ret != NETDEV_TX_OK);
		}
	}

	usleep_range(1000000, 2000000);
	priv->mmc.mmc_tx_octetcount_gb += readl(priv->mmcaddr + 0x14);
	priv->mmc.mmc_tx_framecount_gb += readl(priv->mmcaddr + 0x18);
	priv->mmc.mmc_rx_framecount_gb += readl(priv->mmcaddr + 0x80);
	priv->mmc.mmc_rx_octetcount_gb += readl(priv->mmcaddr + 0x84);
	priv->mmc.mmc_rx_octetcount_g += readl(priv->mmcaddr + 0x88);
	tx_octetcount_gb = priv->mmc.mmc_tx_octetcount_gb - tx_octetcount_gb;
	tx_framecount_gb = priv->mmc.mmc_tx_framecount_gb - tx_framecount_gb;
	rx_framecount_gb = priv->mmc.mmc_rx_framecount_gb - rx_framecount_gb;
	rx_octetcount_gb = priv->mmc.mmc_rx_octetcount_gb - rx_octetcount_gb;
	rx_octetcount_g = priv->mmc.mmc_rx_octetcount_g - rx_octetcount_g;
	usleep_range(40000, 50000);

	if (tx_framecount_gb == rx_framecount_gb &&
	    tx_octetcount_gb == rx_octetcount_gb &&
	    rx_octetcount_gb == rx_octetcount_g &&
	    tx_framecount_gb == (priv->plat->tx_queues_to_use * num)) {
		pr_err("loop back success:\n");
		ret = 0;
	} else {
		pr_err("loop back fail:\n");
		ret = 1;
	}

	pr_err("tx_queues:%d\t pkt_num_per_queue:%d\n",
	       priv->plat->tx_queues_to_use, num);
	pr_err("tx_framecount_gb:%u\t tx_octetcount_gb:%u\n",
	       tx_framecount_gb, tx_octetcount_gb);
	pr_err("rx_framecount_gb:%u\t rx_octetcount_gb:%u, rx_octetcount_g:%u\n",
	       rx_framecount_gb, rx_octetcount_gb, rx_octetcount_g);

	return ret;
}

static ssize_t stmmac_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *netdev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(netdev);
	int len = 0;
	int buf_len = (int)PAGE_SIZE;
	struct phy_device *phy_dev = of_phy_find_device(priv->plat->phy_node);

	len += snprintf(buf + len, buf_len - len,
			"stmmac debug commands: in hexadecimal notation\n");
	len += snprintf(buf + len, buf_len - len,
			"mac read: echo er reg_start reg_range > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"mac write: echo ew reg_addr value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause22 read: echo cl22r phy_reg > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause22 write: echo cl22w phy_reg value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause45 read: echo cl45r dev_id reg_addr > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause45 write: echo cl45w dev_id reg_addr value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"reg read: echo rr reg_addr > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"reg write: echo wr reg_addr value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"phy addr:%d\n", phy_dev->mdio.addr);
	len += snprintf(buf + len, buf_len - len,
			"eth smt result:%d\n", eth_smt_result);

	return len;
}

static ssize_t stmmac_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf,
			    size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *netdev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(netdev);
	void __iomem *tmp_addr;
	int reg, data, devid, origin, i, reg_addr;
	struct phy_device *phy_dev = of_phy_find_device(priv->plat->phy_node);

	if (!strncmp(buf, "er", 2) &&
	    (sscanf(buf + 2, "%x %x", &reg, &data) == 2)) {
		for (i = 0; i < data / 0x10 + 1; i++) {
			dev_info(dev,
				 "%08x:\t%08x\t%08x\t%08x\t%08x\t\n",
				 reg + i * 16,
				 readl(priv->ioaddr + reg + i * 0x10),
				 readl(priv->ioaddr + reg + i * 0x10 + 0x4),
				 readl(priv->ioaddr + reg + i * 0x10 + 0x8),
				 readl(priv->ioaddr + reg + i * 0x10 + 0xc));
		}
	} else if (!strncmp(buf, "ew", 2) &&
		   (sscanf(buf + 2, "%x %x", &reg, &data) == 2)) {
		origin = readl(priv->ioaddr + reg);
		writel(data, priv->ioaddr + reg);
		dev_info(dev, "mac reg%#x, value:%#x -> %#x\n",
			 reg, origin, readl(priv->ioaddr + reg));
	} else if (!strncmp(buf, "cl22r", 5) &&
		   (sscanf(buf + 5, "%x", &reg) == 1)) {
		dev_info(dev, "cl22 reg%#x, value:%#x\n",
			 reg, priv->mii->read(priv->mii,
			 phy_dev->mdio.addr, reg));
	} else if (!strncmp(buf, "cl22w", 5) &&
		   (sscanf(buf + 5, "%x %x", &reg, &data) == 2)) {
		origin = priv->mii->read(priv->mii, phy_dev->mdio.addr, reg);
		priv->mii->write(priv->mii, phy_dev->mdio.addr, reg, data);
		dev_info(dev, "cl22 reg%#x, %#x -> %#x\n",
			 reg, origin, priv->mii->read(priv->mii,
			 phy_dev->mdio.addr, reg));
	} else if (!strncmp(buf, "cl45r", 5) &&
		   (sscanf(buf + 5, "%x %x", &devid, &reg) == 2)) {
		reg_addr = MII_ADDR_C45 |
			   ((devid & 0x1f) << 16) |
			   (reg & 0xffff);
		dev_info(dev, "cl45 reg:%#x-%#x, %#x\n", devid, reg,
			 priv->mii->read(priv->mii, phy_dev->mdio.addr,
					 reg_addr));
	} else if (!strncmp(buf, "cl45w", 5) &&
		   (sscanf(buf + 5, "%x %x %x", &devid, &reg, &data) == 3)) {
		reg_addr = MII_ADDR_C45 |
			   ((devid & 0x1f) << 16) |
			   (reg & 0xffff);
		origin = priv->mii->read(priv->mii,
					 phy_dev->mdio.addr,
					 reg_addr);
		priv->mii->write(priv->mii, phy_dev->mdio.addr,
				 reg_addr, data);
		dev_info(dev, "cl45 reg:%#x-%#x, %#x -> %#x\n",
			 devid, reg, origin,
			 priv->mii->read(priv->mii,
					 phy_dev->mdio.addr,
					 reg_addr));
	} else if (!strncmp(buf, "rr", 2) &&
		   (sscanf(buf + 2, "%x", &reg) == 1)) {
		tmp_addr = ioremap_nocache(reg, 32);
		data = readl(tmp_addr);
		dev_info(dev, "rr reg%#x, value:%#x\n", reg, data);
	} else if (!strncmp(buf, "wr", 2) &&
		   (sscanf(buf + 2, "%x %x", &reg, &data) == 2)) {
		tmp_addr = ioremap_nocache(reg, 32);
		origin = readl(tmp_addr);
		writel(data, tmp_addr);
		dev_info(dev, "reg%#x, value:%#x -> %#x\n",
			 reg, origin, readl(tmp_addr));
	} else if (!strncmp(buf, "dump_mac", 8)) {
		for (i = 0; i < 0x1300 / 0x10 + 1; i++) {
			pr_info("%08x:\t%08x\t%08x\t%08x\t%08x\t\n",
				reg + i * 16,
				readl(priv->ioaddr + i * 0x10),
				readl(priv->ioaddr + i * 0x10 + 0x4),
				readl(priv->ioaddr + i * 0x10 + 0x8),
				readl(priv->ioaddr + i * 0x10 + 0xc));
		}
	} else if (!strncmp(buf, "tx", 2) &&
		   (sscanf(buf + 2, "%d", &reg) == 1)) {
		eth_smt_result = 0;
		if (!send_frame(priv, reg))
			eth_smt_result = 1;
	} else if (!strncmp(buf, "carrier_on", 10)) {
		netif_carrier_on(priv->dev);
	} else if (!strncmp(buf, "carrier_off", 11)) {
		netif_carrier_off(priv->dev);
	} else {
		dev_info(dev, "Error: command not support\n");
	}

	return count;
}

static DEVICE_ATTR_RW(stmmac);

static int eth_create_attr(struct device *dev)
{
	int err = 0;

	if (!dev)
		return -EINVAL;

	err = device_create_file(dev, &dev_attr_stmmac);
	if (err)
		pr_err("create debug file fail:%s\n", __func__);

	return err;
}

static void eth_remove_attr(struct device *dev)
{
	device_remove_file(dev, &dev_attr_stmmac);
}
#endif

static int mt2712_set_interface(struct mediatek_dwmac_plat_data *plat)
{
	int rmii_rxc = plat->rmii_rxc ? RMII_CLK_SRC_RXC : 0;
	u32 intf_val = 0;

	/* select phy interface in top control domain */
	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		intf_val |= PHY_INTF_MII;
		break;
	case PHY_INTERFACE_MODE_RMII:
		intf_val |= (PHY_INTF_RMII | rmii_rxc);
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		intf_val |= PHY_INTF_RGMII;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}

	regmap_write(plat->peri_regmap, PERI_ETH_PHY_INTF_SEL, intf_val);

	return 0;
}

static void mt2712_delay_ps2stage(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_RMII:
		/* 550ps per stage for MII/RMII */
		mac_delay->tx_delay /= 550;
		mac_delay->rx_delay /= 550;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		/* 170ps per stage for RGMII */
		mac_delay->tx_delay /= 170;
		mac_delay->rx_delay /= 170;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		break;
	}
}

static int mt2712_set_delay(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;
	u32 delay_val = 0, fine_val = 0;

	mt2712_delay_ps2stage(plat);

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		delay_val |= FIELD_PREP(ETH_DLY_TXC_ENABLE, !!mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_TXC_STAGES, mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_TXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(ETH_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	case PHY_INTERFACE_MODE_RMII:
		/* the rmii reference clock is from external phy,
		 * and the property "rmii_rxc" indicates which pin(TXC/RXC)
		 * the reference clk is connected to. The reference clock is a
		 * received signal, so rx_delay/rx_inv are used to indicate
		 * the reference clock timing adjustment
		 */
		if (plat->rmii_rxc) {
			/* the rmii reference clock from outside is connected
			 * to RXC pin, the reference clock will be adjusted
			 * by RXC delay macro circuit.
			 */
			delay_val |= FIELD_PREP(ETH_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_RXC_STAGES, mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_RXC_INV, mac_delay->rx_inv);
		} else {
			/* the rmii reference clock from outside is connected
			 * to TXC pin, the reference clock will be adjusted
			 * by TXC delay macro circuit.
			 */
			delay_val |= FIELD_PREP(ETH_DLY_TXC_ENABLE, !!mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_TXC_STAGES, mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_TXC_INV, mac_delay->rx_inv);
		}
		/* tx_inv will inverse the tx clock inside mac relateive to
		 * reference clock from external phy,
		 * and this bit is located in the same register with fine-tune
		 */
		if (mac_delay->tx_inv)
			fine_val = ETH_RMII_DLY_TX_INV;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		fine_val = ETH_FINE_DLY_GTXC | ETH_FINE_DLY_RXC;

		delay_val |= FIELD_PREP(ETH_DLY_GTXC_ENABLE, !!mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_GTXC_STAGES, mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_GTXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(ETH_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}
	regmap_write(plat->peri_regmap, PERI_ETH_DLY, delay_val);
	regmap_write(plat->peri_regmap, PERI_ETH_DLY_FINE, fine_val);

	return 0;
}

static const struct mediatek_dwmac_variant mt2712_gmac_variant = {
		.dwmac_set_phy_interface = mt2712_set_interface,
		.dwmac_set_delay = mt2712_set_delay,
		.clk_list = mt2712_dwmac_clk_l,
		.num_clks = ARRAY_SIZE(mt2712_dwmac_clk_l),
		.dma_bit_mask = 33,
		.rx_delay_max = 17600,
		.tx_delay_max = 17600,
};

static int mediatek_dwmac_config_dt(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;
	u32 tx_delay_ps, rx_delay_ps;

	plat->peri_regmap = syscon_regmap_lookup_by_phandle(plat->np, "mediatek,pericfg");
	if (IS_ERR(plat->peri_regmap)) {
		dev_err(plat->dev, "Failed to get pericfg syscon\n");
		return PTR_ERR(plat->peri_regmap);
	}

	plat->phy_mode = of_get_phy_mode(plat->np);
	if (plat->phy_mode < 0) {
		dev_err(plat->dev, "not find phy-mode\n");
		return -EINVAL;
	}

	if (!of_property_read_u32(plat->np, "mediatek,tx-delay-ps", &tx_delay_ps)) {
		if (tx_delay_ps < plat->variant->tx_delay_max) {
			mac_delay->tx_delay = tx_delay_ps;
		} else {
			dev_err(plat->dev, "Invalid TX clock delay: %dps\n", tx_delay_ps);
			return -EINVAL;
		}
	}

	if (!of_property_read_u32(plat->np, "mediatek,rx-delay-ps", &rx_delay_ps)) {
		if (rx_delay_ps < plat->variant->rx_delay_max) {
			mac_delay->rx_delay = rx_delay_ps;
		} else {
			dev_err(plat->dev, "Invalid RX clock delay: %dps\n", rx_delay_ps);
			return -EINVAL;
		}
	}

	mac_delay->tx_inv = of_property_read_bool(plat->np, "mediatek,txc-inverse");
	mac_delay->rx_inv = of_property_read_bool(plat->np, "mediatek,rxc-inverse");
	plat->rmii_rxc = of_property_read_bool(plat->np, "mediatek,rmii-rxc");

	return 0;
}

static int mediatek_dwmac_clk_init(struct mediatek_dwmac_plat_data *plat)
{
	const struct mediatek_dwmac_variant *variant = plat->variant;
	int i, num = variant->num_clks;

	plat->clks = devm_kcalloc(plat->dev, num, sizeof(*plat->clks), GFP_KERNEL);
	if (!plat->clks)
		return -ENOMEM;

	for (i = 0; i < num; i++)
		plat->clks[i].id = variant->clk_list[i];

	return devm_clk_bulk_get(plat->dev, num, plat->clks);
}

static int mediatek_dwmac_init(struct platform_device *pdev, void *priv)
{
	struct mediatek_dwmac_plat_data *plat = priv;
	const struct mediatek_dwmac_variant *variant = plat->variant;
	int ret;

	ret = dma_set_mask_and_coherent(plat->dev, DMA_BIT_MASK(variant->dma_bit_mask));
	if (ret) {
		dev_err(plat->dev, "No suitable DMA available, err = %d\n", ret);
		return ret;
	}

	ret = variant->dwmac_set_phy_interface(plat);
	if (ret) {
		dev_err(plat->dev, "failed to set phy interface, err = %d\n", ret);
		return ret;
	}

	ret = variant->dwmac_set_delay(plat);
	if (ret) {
		dev_err(plat->dev, "failed to set delay value, err = %d\n", ret);
		return ret;
	}

	ret = clk_bulk_prepare_enable(variant->num_clks, plat->clks);
	if (ret) {
		dev_err(plat->dev, "failed to enable clks, err = %d\n", ret);
		return ret;
	}

	return 0;
}

static void mediatek_dwmac_exit(struct platform_device *pdev, void *priv)
{
	struct mediatek_dwmac_plat_data *plat = priv;
	const struct mediatek_dwmac_variant *variant = plat->variant;

	clk_bulk_disable_unprepare(variant->num_clks, plat->clks);
}

static int mediatek_dwmac_probe(struct platform_device *pdev)
{
	struct mediatek_dwmac_plat_data *priv_plat;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	int ret;

	priv_plat = devm_kzalloc(&pdev->dev, sizeof(*priv_plat), GFP_KERNEL);
	if (!priv_plat)
		return -ENOMEM;

	priv_plat->variant = of_device_get_match_data(&pdev->dev);
	if (!priv_plat->variant) {
		dev_err(&pdev->dev, "Missing dwmac-mediatek variant\n");
		return -EINVAL;
	}

	priv_plat->dev = &pdev->dev;
	priv_plat->np = pdev->dev.of_node;

	ret = mediatek_dwmac_config_dt(priv_plat);
	if (ret)
		return ret;

	ret = mediatek_dwmac_clk_init(priv_plat);
	if (ret)
		return ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	plat_dat->interface = priv_plat->phy_mode;
	plat_dat->has_gmac4 = 1;
	plat_dat->has_gmac = 0;
	plat_dat->pmt = 0;
	plat_dat->riwt_off = 1;
	plat_dat->maxmtu = ETH_DATA_LEN;
	plat_dat->bsp_priv = priv_plat;
	plat_dat->init = mediatek_dwmac_init;
	plat_dat->exit = mediatek_dwmac_exit;
	mediatek_dwmac_init(pdev, priv_plat);

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret) {
		stmmac_remove_config_dt(pdev, plat_dat);
		return ret;
	}

#ifdef CONFIG_DEBUG_FS
	eth_create_attr(&pdev->dev);
#endif
	return 0;
}

static int mediatek_dwmac_remove(struct platform_device *pdev)
{
	int ret;

#ifdef CONFIG_DEBUG_FS
	eth_remove_attr(&pdev->dev);
#endif
	stmmac_pltfr_remove(pdev);

	ret = pm_runtime_put_sync(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "put power fail, ret=%d\n", ret);
		return ret;
	}
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id mediatek_dwmac_match[] = {
	{ .compatible = "mediatek,mt2712-gmac",
	  .data = &mt2712_gmac_variant },
	{ }
};

MODULE_DEVICE_TABLE(of, mediatek_dwmac_match);

static struct platform_driver mediatek_dwmac_driver = {
	.probe  = mediatek_dwmac_probe,
	.remove = mediatek_dwmac_remove,
	.driver = {
		.name           = "dwmac-mediatek",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = mediatek_dwmac_match,
	},
};
module_platform_driver(mediatek_dwmac_driver);

MODULE_AUTHOR("Biao Huang <biao.huang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek DWMAC specific glue layer");
MODULE_LICENSE("GPL v2");
