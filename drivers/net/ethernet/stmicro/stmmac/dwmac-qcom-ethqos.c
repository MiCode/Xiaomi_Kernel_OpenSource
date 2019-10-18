// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2018-19, Linaro Limited

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mii.h>
#include <linux/of_mdio.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/debugfs.h>
#include <asm/dma-iommu.h>
#include <linux/iommu.h>


#include "stmmac.h"
#include "stmmac_platform.h"
#include "dwmac-qcom-ethqos.h"
#include "stmmac_ptp.h"

#define RGMII_IO_MACRO_DEBUG1		0x20
#define EMAC_SYSTEM_LOW_POWER_DEBUG	0x28

/* RGMII_IO_MACRO_CONFIG fields */
#define RGMII_CONFIG_FUNC_CLK_EN		BIT(30)
#define RGMII_CONFIG_POS_NEG_DATA_SEL		BIT(23)
#define RGMII_CONFIG_GPIO_CFG_RX_INT		GENMASK(21, 20)
#define RGMII_CONFIG_GPIO_CFG_TX_INT		GENMASK(19, 17)
#define RGMII_CONFIG_MAX_SPD_PRG_9		GENMASK(16, 8)
#define RGMII_CONFIG_MAX_SPD_PRG_2		GENMASK(7, 6)
#define RGMII_CONFIG_INTF_SEL			GENMASK(5, 4)
#define RGMII_CONFIG_BYPASS_TX_ID_EN		BIT(3)
#define RGMII_CONFIG_LOOPBACK_EN		BIT(2)
#define RGMII_CONFIG_PROG_SWAP			BIT(1)
#define RGMII_CONFIG_DDR_MODE			BIT(0)

/* SDCC_HC_REG_DLL_CONFIG fields */
#define SDCC_DLL_CONFIG_DLL_RST			BIT(30)
#define SDCC_DLL_CONFIG_PDN			BIT(29)
#define SDCC_DLL_CONFIG_MCLK_FREQ		GENMASK(26, 24)
#define SDCC_DLL_CONFIG_CDR_SELEXT		GENMASK(23, 20)
#define SDCC_DLL_CONFIG_CDR_EXT_EN		BIT(19)
#define SDCC_DLL_CONFIG_CK_OUT_EN		BIT(18)
#define SDCC_DLL_CONFIG_CDR_EN			BIT(17)
#define SDCC_DLL_CONFIG_DLL_EN			BIT(16)
#define SDCC_DLL_MCLK_GATING_EN			BIT(5)
#define SDCC_DLL_CDR_FINE_PHASE			GENMASK(3, 2)

/* SDCC_HC_REG_DDR_CONFIG fields */
#define SDCC_DDR_CONFIG_PRG_DLY_EN		BIT(31)
#define SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY	GENMASK(26, 21)
#define SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE	GENMASK(29, 27)
#define SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN	BIT(30)
#define SDCC_DDR_CONFIG_PRG_RCLK_DLY		GENMASK(8, 0)

/* SDCC_HC_REG_DLL_CONFIG2 fields */
#define SDCC_DLL_CONFIG2_DLL_CLOCK_DIS		BIT(21)
#define SDCC_DLL_CONFIG2_MCLK_FREQ_CALC		GENMASK(17, 10)
#define SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SEL	GENMASK(3, 2)
#define SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SW	BIT(1)
#define SDCC_DLL_CONFIG2_DDR_CAL_EN		BIT(0)

/* SDC4_STATUS bits */
#define SDC4_STATUS_DLL_LOCK			BIT(7)

/* RGMII_IO_MACRO_CONFIG2 fields */
#define RGMII_CONFIG2_RSVD_CONFIG15		GENMASK(31, 17)
#define RGMII_CONFIG2_RGMII_CLK_SEL_CFG		BIT(16)
#define RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN	BIT(13)
#define RGMII_CONFIG2_CLK_DIVIDE_SEL		BIT(12)
#define RGMII_CONFIG2_RX_PROG_SWAP		BIT(7)
#define RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL	BIT(6)
#define RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN	BIT(5)

#define EMAC_I0_EMAC_CORE_HW_VERSION_RGOFFADDR 0x00000070
#define EMAC_HW_v2_3_2_RG 0x20030002

#define MII_BUSY 0x00000001
#define MII_WRITE 0x00000002

/* GMAC4 defines */
#define MII_GMAC4_GOC_SHIFT		2
#define MII_GMAC4_WRITE			BIT(MII_GMAC4_GOC_SHIFT)
#define MII_GMAC4_READ			(3 << MII_GMAC4_GOC_SHIFT)

#define MII_BUSY 0x00000001
#define MII_WRITE 0x00000002

#define DWC_ETH_QOS_PHY_INTR_STATUS     0x0013

#define LINK_UP 1
#define LINK_DOWN 0

#define LINK_DOWN_STATE 0x800
#define LINK_UP_STATE 0x400

bool phy_intr_en;

struct qcom_ethqos *pethqos;
struct emac_emb_smmu_cb_ctx emac_emb_smmu_ctx = {0};

void *ipc_emac_log_ctxt;

static int rgmii_readl(struct qcom_ethqos *ethqos, unsigned int offset)
{
	return readl(ethqos->rgmii_base + offset);
}

static void rgmii_writel(struct qcom_ethqos *ethqos,
			 int value, unsigned int offset)
{
	writel(value, ethqos->rgmii_base + offset);
}

static void rgmii_updatel(struct qcom_ethqos *ethqos,
			  int mask, int val, unsigned int offset)
{
	unsigned int temp;

	temp =  rgmii_readl(ethqos, offset);
	temp = (temp & ~(mask)) | val;
	rgmii_writel(ethqos, temp, offset);
}

static void rgmii_dump(struct qcom_ethqos *ethqos)
{
	dev_dbg(&ethqos->pdev->dev, "Rgmii register dump\n");
	dev_dbg(&ethqos->pdev->dev, "RGMII_IO_MACRO_CONFIG: %x\n",
		rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG));
	dev_dbg(&ethqos->pdev->dev, "SDCC_HC_REG_DLL_CONFIG: %x\n",
		rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG));
	dev_dbg(&ethqos->pdev->dev, "SDCC_HC_REG_DDR_CONFIG: %x\n",
		rgmii_readl(ethqos, SDCC_HC_REG_DDR_CONFIG));
	dev_dbg(&ethqos->pdev->dev, "SDCC_HC_REG_DLL_CONFIG2: %x\n",
		rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG2));
	dev_dbg(&ethqos->pdev->dev, "SDC4_STATUS: %x\n",
		rgmii_readl(ethqos, SDC4_STATUS));
	dev_dbg(&ethqos->pdev->dev, "SDCC_USR_CTL: %x\n",
		rgmii_readl(ethqos, SDCC_USR_CTL));
	dev_dbg(&ethqos->pdev->dev, "RGMII_IO_MACRO_CONFIG2: %x\n",
		rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG2));
	dev_dbg(&ethqos->pdev->dev, "RGMII_IO_MACRO_DEBUG1: %x\n",
		rgmii_readl(ethqos, RGMII_IO_MACRO_DEBUG1));
	dev_dbg(&ethqos->pdev->dev, "EMAC_SYSTEM_LOW_POWER_DEBUG: %x\n",
		rgmii_readl(ethqos, EMAC_SYSTEM_LOW_POWER_DEBUG));
}

/* Clock rates */
#define RGMII_1000_NOM_CLK_FREQ			(250 * 1000 * 1000UL)
#define RGMII_ID_MODE_100_LOW_SVS_CLK_FREQ	 (50 * 1000 * 1000UL)
#define RGMII_ID_MODE_10_LOW_SVS_CLK_FREQ	  (5 * 1000 * 1000UL)

static void
ethqos_update_rgmii_clk(struct qcom_ethqos *ethqos, unsigned int speed)
{
	switch (speed) {
	case SPEED_1000:
		ethqos->rgmii_clk_rate =  RGMII_1000_NOM_CLK_FREQ;
		break;

	case SPEED_100:
		ethqos->rgmii_clk_rate =  RGMII_ID_MODE_100_LOW_SVS_CLK_FREQ;
		break;

	case SPEED_10:
		ethqos->rgmii_clk_rate =  RGMII_ID_MODE_10_LOW_SVS_CLK_FREQ;
		break;
	}

	clk_set_rate(ethqos->rgmii_clk, ethqos->rgmii_clk_rate);
}

static void ethqos_set_func_clk_en(struct qcom_ethqos *ethqos)
{
	rgmii_updatel(ethqos, RGMII_CONFIG_FUNC_CLK_EN,
		      RGMII_CONFIG_FUNC_CLK_EN, RGMII_IO_MACRO_CONFIG);
}

static int ethqos_dll_configure(struct qcom_ethqos *ethqos)
{
	unsigned int val;
	int retry = 1000;

	/* Set CDR_EN */
	if (ethqos->emac_ver == EMAC_HW_v2_3_2_RG)
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CDR_EN,
			      0, SDCC_HC_REG_DLL_CONFIG);
	else
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CDR_EN,
			      SDCC_DLL_CONFIG_CDR_EN, SDCC_HC_REG_DLL_CONFIG);

	/* Set CDR_EXT_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CDR_EXT_EN,
		      SDCC_DLL_CONFIG_CDR_EXT_EN, SDCC_HC_REG_DLL_CONFIG);

	/* Clear CK_OUT_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
		      0, SDCC_HC_REG_DLL_CONFIG);

	/* Set DLL_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_EN,
		      SDCC_DLL_CONFIG_DLL_EN, SDCC_HC_REG_DLL_CONFIG);

	if (ethqos->emac_ver != EMAC_HW_v2_3_2_RG) {
		rgmii_updatel(ethqos, SDCC_DLL_MCLK_GATING_EN,
			      0, SDCC_HC_REG_DLL_CONFIG);

		rgmii_updatel(ethqos, SDCC_DLL_CDR_FINE_PHASE,
			      0, SDCC_HC_REG_DLL_CONFIG);
	}
	/* Wait for CK_OUT_EN clear */
	do {
		val = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
		val &= SDCC_DLL_CONFIG_CK_OUT_EN;
		if (!val)
			break;
		mdelay(1);
		retry--;
	} while (retry > 0);
	if (!retry)
		dev_err(&ethqos->pdev->dev, "Clear CK_OUT_EN timedout\n");

	/* Set CK_OUT_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
		      SDCC_DLL_CONFIG_CK_OUT_EN, SDCC_HC_REG_DLL_CONFIG);

	/* Wait for CK_OUT_EN set */
	retry = 1000;
	do {
		val = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
		val &= SDCC_DLL_CONFIG_CK_OUT_EN;
		if (val)
			break;
		mdelay(1);
		retry--;
	} while (retry > 0);
	if (!retry)
		dev_err(&ethqos->pdev->dev, "Set CK_OUT_EN timedout\n");

	/* Set DDR_CAL_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_CAL_EN,
		      SDCC_DLL_CONFIG2_DDR_CAL_EN, SDCC_HC_REG_DLL_CONFIG2);

	if (ethqos->emac_ver != EMAC_HW_v2_3_2_RG) {
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DLL_CLOCK_DIS,
			      0, SDCC_HC_REG_DLL_CONFIG2);

		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_MCLK_FREQ_CALC,
			      0x1A << 10, SDCC_HC_REG_DLL_CONFIG2);

		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SEL,
			      BIT(2), SDCC_HC_REG_DLL_CONFIG2);

		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SW,
			      SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SW,
			      SDCC_HC_REG_DLL_CONFIG2);
	}

	return 0;
}

static int ethqos_rgmii_macro_init(struct qcom_ethqos *ethqos)
{
	/* Disable loopback mode */
	rgmii_updatel(ethqos, RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
		      0, RGMII_IO_MACRO_CONFIG2);

	/* Select RGMII, write 0 to interface select */
	rgmii_updatel(ethqos, RGMII_CONFIG_INTF_SEL,
		      0, RGMII_IO_MACRO_CONFIG);

	switch (ethqos->speed) {
	case SPEED_1000:
		rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
			      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
			      RGMII_CONFIG_POS_NEG_DATA_SEL,
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
			      RGMII_CONFIG_PROG_SWAP, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
			      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
			      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_IO_MACRO_CONFIG2);

		/* Set PRG_RCLK_DLY to 57 for 1.8 ns delay */
		if (ethqos->emac_ver == EMAC_HW_v2_3_2_RG)
			rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_RCLK_DLY,
				      69, SDCC_HC_REG_DDR_CONFIG);
		else
			rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_RCLK_DLY,
				      57, SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_DLY_EN,
			      SDCC_DDR_CONFIG_PRG_DLY_EN,
			      SDCC_HC_REG_DDR_CONFIG);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2_RG)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
		break;

	case SPEED_100:
		rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
			      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
			      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG_MAX_SPD_PRG_2,
			      BIT(6), RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
			      0, RGMII_IO_MACRO_CONFIG2);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2_RG)
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      0, RGMII_IO_MACRO_CONFIG2);
		/* Write 0x5 to PRG_RCLK_DLY_CODE */
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE,
			      (BIT(29) | BIT(27)), SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_HC_REG_DDR_CONFIG);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2_RG)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
		break;

	case SPEED_10:
		rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
			      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
			      0, RGMII_IO_MACRO_CONFIG2);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2_RG)
			rgmii_updatel(ethqos,
				      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
				      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
				      RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos,
				      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
				      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG_MAX_SPD_PRG_9,
			      BIT(12) | GENMASK(9, 8),
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
			      0, RGMII_IO_MACRO_CONFIG2);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2_RG)
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      0, RGMII_IO_MACRO_CONFIG2);
		/* Write 0x5 to PRG_RCLK_DLY_CODE */
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE,
			      (BIT(29) | BIT(27)), SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_HC_REG_DDR_CONFIG);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2_RG)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
		break;
	default:
		dev_err(&ethqos->pdev->dev,
			"Invalid speed %d\n", ethqos->speed);
		return -EINVAL;
	}

	return 0;
}

static int ethqos_configure(struct qcom_ethqos *ethqos)
{
	volatile unsigned int dll_lock;
	unsigned int i, retry = 1000;

	/* Reset to POR values and enable clk */
	for (i = 0; i < ethqos->num_por; i++)
		rgmii_writel(ethqos, ethqos->por[i].value,
			     ethqos->por[i].offset);
	ethqos_set_func_clk_en(ethqos);

	/* Initialize the DLL first */

	/* Set DLL_RST */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST,
		      SDCC_DLL_CONFIG_DLL_RST, SDCC_HC_REG_DLL_CONFIG);

	/* Set PDN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN,
		      SDCC_DLL_CONFIG_PDN, SDCC_HC_REG_DLL_CONFIG);

	/* Clear DLL_RST */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST, 0,
		      SDCC_HC_REG_DLL_CONFIG);

	/* Clear PDN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN, 0,
		      SDCC_HC_REG_DLL_CONFIG);

	if (ethqos->speed != SPEED_100 && ethqos->speed != SPEED_10) {
		/* Set DLL_EN */
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_EN,
			      SDCC_DLL_CONFIG_DLL_EN, SDCC_HC_REG_DLL_CONFIG);

		/* Set CK_OUT_EN */
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
			      SDCC_DLL_CONFIG_CK_OUT_EN,
			      SDCC_HC_REG_DLL_CONFIG);

		/* Set USR_CTL bit 26 with mask of 3 bits */
		rgmii_updatel(ethqos, GENMASK(26, 24), BIT(26), SDCC_USR_CTL);

		/* wait for DLL LOCK */
		do {
			mdelay(1);
			dll_lock = rgmii_readl(ethqos, SDC4_STATUS);
			if (dll_lock & SDC4_STATUS_DLL_LOCK)
				break;
		} while (retry > 0);
		if (!retry)
			dev_err(&ethqos->pdev->dev,
				"Timeout while waiting for DLL lock\n");
	}

	if (ethqos->speed == SPEED_1000)
		ethqos_dll_configure(ethqos);

	ethqos_rgmii_macro_init(ethqos);

	return 0;
}

static void ethqos_fix_mac_speed(void *priv, unsigned int speed)
{
	struct qcom_ethqos *ethqos = priv;

	ethqos->speed = speed;
	ethqos_update_rgmii_clk(ethqos, speed);
	ethqos_configure(ethqos);
}

static int ethqos_mdio_read(struct stmmac_priv  *priv, int phyaddr, int phyreg)
{
	unsigned int mii_address = priv->hw->mii.addr;
	unsigned int mii_data = priv->hw->mii.data;
	u32 v;
	int data;
	u32 value = MII_BUSY;

	value |= (phyaddr << priv->hw->mii.addr_shift)
		& priv->hw->mii.addr_mask;
	value |= (phyreg << priv->hw->mii.reg_shift) & priv->hw->mii.reg_mask;
	value |= (priv->clk_csr << priv->hw->mii.clk_csr_shift)
		& priv->hw->mii.clk_csr_mask;
	if (priv->plat->has_gmac4)
		value |= MII_GMAC4_READ;

	if (readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
			       100, 10000))
		return -EBUSY;

	writel_relaxed(value, priv->ioaddr + mii_address);

	if (readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
			       100, 10000))
		return -EBUSY;

	/* Read the data from the MII data register */
	data = (int)readl_relaxed(priv->ioaddr + mii_data);

	return data;
}

static int ethqos_phy_intr_config(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	ethqos->phy_intr = platform_get_irq_byname(ethqos->pdev, "phy-intr");

	if (ethqos->phy_intr < 0) {
		if (ethqos->phy_intr != -EPROBE_DEFER) {
			dev_err(&ethqos->pdev->dev,
				"PHY IRQ configuration information not found\n");
		}
		ret = 1;
	}

	return ret;
}

static void ethqos_handle_phy_interrupt(struct qcom_ethqos *ethqos)
{
	int phy_intr_status = 0;
	struct platform_device *pdev = ethqos->pdev;

	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	phy_intr_status = ethqos_mdio_read(priv, priv->plat->phy_addr,
					   DWC_ETH_QOS_PHY_INTR_STATUS);

	if (dev->phydev && (phy_intr_status & LINK_UP_STATE))
		phy_mac_interrupt(dev->phydev, LINK_UP);
	else if (dev->phydev && (phy_intr_status & LINK_DOWN_STATE))
		phy_mac_interrupt(dev->phydev, LINK_DOWN);
}

static void ethqos_defer_phy_isr_work(struct work_struct *work)
{
	struct qcom_ethqos *ethqos =
		container_of(work, struct qcom_ethqos, emac_phy_work);

	ethqos_handle_phy_interrupt(ethqos);
}

static irqreturn_t ETHQOS_PHY_ISR(int irq, void *dev_data)
{
	struct qcom_ethqos *ethqos = (struct qcom_ethqos *)dev_data;

	queue_work(system_wq, &ethqos->emac_phy_work);
}

static int ethqos_phy_intr_enable(struct qcom_ethqos *ethqos)
{
	int ret = 0;
	struct net_device *dev = platform_get_drvdata(ethqos->pdev);

	INIT_WORK(&ethqos->emac_phy_work, ethqos_defer_phy_isr_work);
	ret = request_irq(ethqos->phy_intr, ETHQOS_PHY_ISR,
			  IRQF_SHARED, "stmmac", ethqos);
	if (ret) {
		ETHQOSERR("Unable to register PHY IRQ %d\n",
			  ethqos->phy_intr);
		return ret;
	}
	phy_intr_en = true;
	return ret;
}

static void ethqos_pps_irq_config(struct qcom_ethqos *ethqos)
{
	ethqos->pps_class_a_irq =
	platform_get_irq_byname(ethqos->pdev, "ptp_pps_irq_0");
	if (ethqos->pps_class_a_irq < 0) {
		if (ethqos->pps_class_a_irq != -EPROBE_DEFER)
			ETHQOSERR("class_a_irq config info not found\n");
	}
	ethqos->pps_class_b_irq =
	platform_get_irq_byname(ethqos->pdev, "ptp_pps_irq_1");
	if (ethqos->pps_class_b_irq < 0) {
		if (ethqos->pps_class_b_irq != -EPROBE_DEFER)
			ETHQOSERR("class_b_irq config info not found\n");
	}
}

static const struct of_device_id qcom_ethqos_match[] = {
	{ .compatible = "qcom,sdxprairie-ethqos", .data = &emac_v2_3_2_por},
	{ .compatible = "qcom,emac-smmu-embedded", },
	{ }
};

static ssize_t read_phy_reg_dump(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct qcom_ethqos *ethqos = file->private_data;
	unsigned int len = 0, buf_len = 2000;
	char *buf;
	ssize_t ret_cnt;
	int phydata = 0;
	int i = 0;

	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	if (!ethqos || !dev->phydev) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len,
					 "\n************* PHY Reg dump *************\n");

	for (i = 0; i < 32; i++) {
		phydata = ethqos_mdio_read(priv, priv->plat->phy_addr, i);
		len += scnprintf(buf + len, buf_len - len,
					 "MII Register (%#x) = %#x\n",
					 i, phydata);
	}

	if (len > buf_len) {
		ETHQOSERR("(len > buf_len) buffer not sufficient\n");
		len = buf_len;
	}

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static ssize_t read_rgmii_reg_dump(struct file *file,
				   char __user *user_buf, size_t count,
				   loff_t *ppos)
{
	struct qcom_ethqos *ethqos = file->private_data;
	unsigned int len = 0, buf_len = 2000;
	char *buf;
	ssize_t ret_cnt;
	int rgmii_data = 0;
	struct platform_device *pdev = ethqos->pdev;

	struct net_device *dev = platform_get_drvdata(pdev);

	if (!ethqos || !dev->phydev) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len,
					 "\n************* RGMII Reg dump *************\n");
	rgmii_data = rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG);
	len += scnprintf(buf + len, buf_len - len,
					 "RGMII_IO_MACRO_CONFIG Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_HC_REG_DLL_CONFIG Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_HC_REG_DDR_CONFIG);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_HC_REG_DDR_CONFIG Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG2);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_HC_REG_DLL_CONFIG2 Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDC4_STATUS);
	len += scnprintf(buf + len, buf_len - len,
					 "SDC4_STATUS Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_USR_CTL);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_USR_CTL Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG2);
	len += scnprintf(buf + len, buf_len - len,
					 "RGMII_IO_MACRO_CONFIG2 Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, RGMII_IO_MACRO_DEBUG1);
	len += scnprintf(buf + len, buf_len - len,
					 "RGMII_IO_MACRO_DEBUG1 Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, EMAC_SYSTEM_LOW_POWER_DEBUG);
	len += scnprintf(buf + len, buf_len - len,
					 "EMAC_SYSTEM_LOW_POWER_DEBUG Register = %#x\n",
					 rgmii_data);

	if (len > buf_len) {
		ETHQOSERR("(len > buf_len) buffer not sufficient\n");
		len = buf_len;
	}

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static const struct file_operations fops_phy_reg_dump = {
	.read = read_phy_reg_dump,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_rgmii_reg_dump = {
	.read = read_rgmii_reg_dump,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ethqos_create_debugfs(struct qcom_ethqos        *ethqos)
{
	static struct dentry *phy_reg_dump;
	static struct dentry *rgmii_reg_dump;

	if (!ethqos) {
		ETHQOSERR("Null Param %s\n", __func__);
		return -ENOMEM;
	}

	ethqos->debugfs_dir = debugfs_create_dir("eth", NULL);

	if (!ethqos->debugfs_dir || IS_ERR(ethqos->debugfs_dir)) {
		ETHQOSERR("Can't create debugfs dir\n");
		return -ENOMEM;
	}

	phy_reg_dump = debugfs_create_file("phy_reg_dump", 0400,
					   ethqos->debugfs_dir, ethqos,
					   &fops_phy_reg_dump);
	if (!phy_reg_dump || IS_ERR(phy_reg_dump)) {
		ETHQOSERR("Can't create phy_dump %d\n", (int)phy_reg_dump);
		goto fail;
	}

	rgmii_reg_dump = debugfs_create_file("rgmii_reg_dump", 0400,
					     ethqos->debugfs_dir, ethqos,
					     &fops_rgmii_reg_dump);
	if (!rgmii_reg_dump || IS_ERR(rgmii_reg_dump)) {
		ETHQOSERR("Can't create rgmii_dump %d\n", (int)rgmii_reg_dump);
		goto fail;
	}
	return 0;

fail:
	debugfs_remove_recursive(ethqos->debugfs_dir);
	return -ENOMEM;
}

static void emac_emb_smmu_exit(void)
{
	if (emac_emb_smmu_ctx.valid) {
		if (emac_emb_smmu_ctx.smmu_pdev)
			arm_iommu_detach_device
			(&emac_emb_smmu_ctx.smmu_pdev->dev);
		if (emac_emb_smmu_ctx.mapping)
			arm_iommu_release_mapping(emac_emb_smmu_ctx.mapping);
		emac_emb_smmu_ctx.valid = false;
		emac_emb_smmu_ctx.mapping = NULL;
		emac_emb_smmu_ctx.pdev_master = NULL;
		emac_emb_smmu_ctx.smmu_pdev = NULL;
	}
}

static int emac_emb_smmu_cb_probe(struct platform_device *pdev)
{
	int result;
	u32 iova_ap_mapping[2];
	struct device *dev = &pdev->dev;
	int atomic_ctx = 1;
	int fast = 1;
	int bypass = 1;

	ETHQOSDBG("EMAC EMB SMMU CB probe: smmu pdev=%p\n", pdev);

	result = of_property_read_u32_array(dev->of_node, "qcom,iova-mapping",
					    iova_ap_mapping, 2);
	if (result) {
		ETHQOSERR("Failed to read EMB start/size iova addresses\n");
		return result;
	}
	emac_emb_smmu_ctx.va_start = iova_ap_mapping[0];
	emac_emb_smmu_ctx.va_size = iova_ap_mapping[1];
	emac_emb_smmu_ctx.va_end = emac_emb_smmu_ctx.va_start +
				   emac_emb_smmu_ctx.va_size;

	emac_emb_smmu_ctx.smmu_pdev = pdev;

	if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
	    dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		ETHQOSERR("DMA set 32bit mask failed\n");
		return -EOPNOTSUPP;
	}

	emac_emb_smmu_ctx.mapping = arm_iommu_create_mapping
	(dev->bus, emac_emb_smmu_ctx.va_start, emac_emb_smmu_ctx.va_size);
	if (IS_ERR_OR_NULL(emac_emb_smmu_ctx.mapping)) {
		ETHQOSDBG("Fail to create mapping\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}
	ETHQOSDBG("Successfully Created SMMU mapping\n");
	emac_emb_smmu_ctx.valid = true;

	if (of_property_read_bool(dev->of_node, "qcom,smmu-s1-bypass")) {
		if (iommu_domain_set_attr(emac_emb_smmu_ctx.mapping->domain,
					  DOMAIN_ATTR_S1_BYPASS,
					  &bypass)) {
			ETHQOSERR("Couldn't set SMMU S1 bypass\n");
			result = -EIO;
			goto err_smmu_probe;
		}
		ETHQOSDBG("SMMU S1 BYPASS set\n");
	} else {
		if (iommu_domain_set_attr(emac_emb_smmu_ctx.mapping->domain,
					  DOMAIN_ATTR_ATOMIC,
					  &atomic_ctx)) {
			ETHQOSERR("Couldn't set SMMU domain as atomic\n");
			result = -EIO;
			goto err_smmu_probe;
		}
		ETHQOSDBG("SMMU atomic set\n");
		if (iommu_domain_set_attr(emac_emb_smmu_ctx.mapping->domain,
					  DOMAIN_ATTR_FAST,
					  &fast)) {
			ETHQOSERR("Couldn't set FAST SMMU\n");
			result = -EIO;
			goto err_smmu_probe;
		}
		ETHQOSDBG("SMMU fast map set\n");
	}

	result = arm_iommu_attach_device(&emac_emb_smmu_ctx.smmu_pdev->dev,
					 emac_emb_smmu_ctx.mapping);
	if (result) {
		ETHQOSERR("couldn't attach to IOMMU ret=%d\n", result);
		goto err_smmu_probe;
	}

	emac_emb_smmu_ctx.iommu_domain =
		iommu_get_domain_for_dev(&emac_emb_smmu_ctx.smmu_pdev->dev);

	ETHQOSDBG("Successfully attached to IOMMU\n");
	if (emac_emb_smmu_ctx.pdev_master)
		goto smmu_probe_done;

err_smmu_probe:
	if (emac_emb_smmu_ctx.mapping)
		arm_iommu_release_mapping(emac_emb_smmu_ctx.mapping);
	emac_emb_smmu_ctx.valid = false;

smmu_probe_done:
	emac_emb_smmu_ctx.ret = result;
	return result;
}

static int qcom_ethqos_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct qcom_ethqos *ethqos;
	struct resource *res;
	int ret;

	ipc_emac_log_ctxt = ipc_log_context_create(IPCLOG_STATE_PAGES,
						   "emac", 0);
	if (!ipc_emac_log_ctxt)
		ETHQOSERR("Error creating logging context for emac\n");
	else
		ETHQOSDBG("IPC logging has been enabled for emac\n");
	if (of_device_is_compatible(pdev->dev.of_node,
				    "qcom,emac-smmu-embedded"))
		return emac_emb_smmu_cb_probe(pdev);
	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	ethqos = devm_kzalloc(&pdev->dev, sizeof(*ethqos), GFP_KERNEL);
	if (!ethqos) {
		ret = -ENOMEM;
		goto err_mem;
	}
	ethqos->pdev = pdev;

	ethqos_init_reqgulators(ethqos);
	ethqos_init_gpio(ethqos);

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat)) {
		dev_err(&pdev->dev, "dt configuration failed\n");
		return PTR_ERR(plat_dat);
	}


	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rgmii");
	ethqos->rgmii_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ethqos->rgmii_base)) {
		dev_err(&pdev->dev, "Can't get rgmii base\n");
		ret = PTR_ERR(ethqos->rgmii_base);
		goto err_mem;
	}

	ethqos->por = of_device_get_match_data(&pdev->dev);

	ethqos->rgmii_clk = devm_clk_get(&pdev->dev, "rgmii");
	if (!ethqos->rgmii_clk) {
		ret = -ENOMEM;
		goto err_mem;
	}

	ret = clk_prepare_enable(ethqos->rgmii_clk);
	if (ret)
		goto err_mem;

	ethqos->speed = SPEED_1000;
	ethqos_update_rgmii_clk(ethqos, SPEED_1000);
	ethqos_set_func_clk_en(ethqos);

	plat_dat->bsp_priv = ethqos;
	plat_dat->fix_mac_speed = ethqos_fix_mac_speed;
	plat_dat->has_gmac4 = 1;
	plat_dat->pmt = 1;
	plat_dat->tso_en = of_property_read_bool(np, "snps,tso");

	if (of_property_read_bool(pdev->dev.of_node, "qcom,arm-smmu")) {
		emac_emb_smmu_ctx.pdev_master = pdev;
		ret = of_platform_populate(pdev->dev.of_node,
					   qcom_ethqos_match, NULL, &pdev->dev);
		if (ret)
			ETHQOSERR("Failed to populate EMAC platform\n");
		if (emac_emb_smmu_ctx.ret) {
			ETHQOSERR("smmu probe failed\n");
			of_platform_depopulate(&pdev->dev);
			ret = emac_emb_smmu_ctx.ret;
			emac_emb_smmu_ctx.ret = 0;
		}
	}

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_clk;

	ethqos->emac_ver = rgmii_readl(ethqos,
				       EMAC_I0_EMAC_CORE_HW_VERSION_RGOFFADDR);

	if (!ethqos_phy_intr_config(ethqos))
		ethqos_phy_intr_enable(ethqos);
	else
		ETHQOSERR("Phy interrupt configuration failed");
	rgmii_dump(ethqos);

	if (ethqos->emac_ver == EMAC_HW_v2_3_2_RG) {
		ethqos_pps_irq_config(ethqos);
		create_pps_interrupt_device_node(&ethqos->avb_class_a_dev_t,
						 &ethqos->avb_class_a_cdev,
						 &ethqos->avb_class_a_class,
						 AVB_CLASS_A_POLL_DEV_NODE);

		create_pps_interrupt_device_node(&ethqos->avb_class_b_dev_t,
						 &ethqos->avb_class_b_cdev,
						 &ethqos->avb_class_b_class,
						 AVB_CLASS_B_POLL_DEV_NODE);
	}

	pethqos = ethqos;
	ethqos_create_debugfs(ethqos);
	return ret;

err_clk:
	clk_disable_unprepare(ethqos->rgmii_clk);

err_mem:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static int qcom_ethqos_remove(struct platform_device *pdev)
{
	struct qcom_ethqos *ethqos;
	int ret;

	ethqos = get_stmmac_bsp_priv(&pdev->dev);
	if (!ethqos)
		return -ENODEV;

	ret = stmmac_pltfr_remove(pdev);
	clk_disable_unprepare(ethqos->rgmii_clk);

	if (phy_intr_en)
		free_irq(ethqos->phy_intr, ethqos);
	emac_emb_smmu_exit();
	ethqos_disable_regulators(ethqos);

	return ret;
}

MODULE_DEVICE_TABLE(of, qcom_ethqos_match);

static struct platform_driver qcom_ethqos_driver = {
	.probe  = qcom_ethqos_probe,
	.remove = qcom_ethqos_remove,
	.driver = {
		.name           = "qcom-ethqos",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = of_match_ptr(qcom_ethqos_match),
	},
};
module_platform_driver(qcom_ethqos_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. ETHQOS driver");
MODULE_LICENSE("GPL v2");
