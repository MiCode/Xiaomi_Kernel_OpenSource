/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * SATA init module.
 * To be used with SATA interface on MSM targets.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/regulator/consumer.h>
#include <linux/ahci_platform.h>
#include <mach/clk.h>

/* PHY registers */
#define UNIPHY_PLL_REFCLK_CFG		0x000
#define UNIPHY_PLL_POSTDIV1_CFG		0x004
#define UNIPHY_PLL_CHGPUMP_CFG		0x008
#define UNIPHY_PLL_VCOLPF_CFG		0x00C
#define UNIPHY_PLL_VREG_CFG		0x010
#define UNIPHY_PLL_PWRGEN_CFG		0x014
#define UNIPHY_PLL_DMUX_CFG		0x018
#define UNIPHY_PLL_AMUX_CFG		0x01C
#define UNIPHY_PLL_GLB_CFG		0x020
#define UNIPHY_PLL_POSTDIV2_CFG		0x024
#define UNIPHY_PLL_POSTDIV3_CFG		0x028
#define UNIPHY_PLL_LPFR_CFG		0x02C
#define UNIPHY_PLL_LPFC1_CFG		0x030
#define UNIPHY_PLL_LPFC2_CFG		0x034
#define UNIPHY_PLL_SDM_CFG0		0x038
#define UNIPHY_PLL_SDM_CFG1		0x03C
#define UNIPHY_PLL_SDM_CFG2		0x040
#define UNIPHY_PLL_SDM_CFG3		0x044
#define UNIPHY_PLL_SDM_CFG4		0x048
#define UNIPHY_PLL_SSC_CFG0		0x04C
#define UNIPHY_PLL_SSC_CFG1		0x050
#define UNIPHY_PLL_SSC_CFG2		0x054
#define UNIPHY_PLL_SSC_CFG3		0x058
#define UNIPHY_PLL_LKDET_CFG0		0x05C
#define UNIPHY_PLL_LKDET_CFG1		0x060
#define UNIPHY_PLL_LKDET_CFG2		0x064
#define UNIPHY_PLL_TEST_CFG		0x068
#define UNIPHY_PLL_CAL_CFG0		0x06C
#define UNIPHY_PLL_CAL_CFG1		0x070
#define UNIPHY_PLL_CAL_CFG2		0x074
#define UNIPHY_PLL_CAL_CFG3		0x078
#define UNIPHY_PLL_CAL_CFG4		0x07C
#define UNIPHY_PLL_CAL_CFG5		0x080
#define UNIPHY_PLL_CAL_CFG6		0x084
#define UNIPHY_PLL_CAL_CFG7		0x088
#define UNIPHY_PLL_CAL_CFG8		0x08C
#define UNIPHY_PLL_CAL_CFG9		0x090
#define UNIPHY_PLL_CAL_CFG10		0x094
#define UNIPHY_PLL_CAL_CFG11		0x098
#define UNIPHY_PLL_EFUSE_CFG		0x09C
#define UNIPHY_PLL_DEBUG_BUS_SEL	0x0A0
#define UNIPHY_PLL_CTRL_42		0x0A4
#define UNIPHY_PLL_CTRL_43		0x0A8
#define UNIPHY_PLL_CTRL_44		0x0AC
#define UNIPHY_PLL_CTRL_45		0x0B0
#define UNIPHY_PLL_CTRL_46		0x0B4
#define UNIPHY_PLL_CTRL_47		0x0B8
#define UNIPHY_PLL_CTRL_48		0x0BC
#define UNIPHY_PLL_STATUS		0x0C0
#define UNIPHY_PLL_DEBUG_BUS0		0x0C4
#define UNIPHY_PLL_DEBUG_BUS1		0x0C8
#define UNIPHY_PLL_DEBUG_BUS2		0x0CC
#define UNIPHY_PLL_DEBUG_BUS3		0x0D0
#define UNIPHY_PLL_CTRL_54		0x0D4

#define SATA_PHY_SER_CTRL		0x100
#define SATA_PHY_TX_DRIV_CTRL0		0x104
#define SATA_PHY_TX_DRIV_CTRL1		0x108
#define SATA_PHY_TX_DRIV_CTRL2		0x10C
#define SATA_PHY_TX_DRIV_CTRL3		0x110
#define SATA_PHY_TX_RESV0		0x114
#define SATA_PHY_TX_RESV1		0x118
#define SATA_PHY_TX_IMCAL0		0x11C
#define SATA_PHY_TX_IMCAL1		0x120
#define SATA_PHY_TX_IMCAL2		0x124
#define SATA_PHY_RX_IMCAL0		0x128
#define SATA_PHY_RX_IMCAL1		0x12C
#define SATA_PHY_RX_IMCAL2		0x130
#define SATA_PHY_RX_TERM		0x134
#define SATA_PHY_RX_TERM_RESV		0x138
#define SATA_PHY_EQUAL			0x13C
#define SATA_PHY_EQUAL_RESV		0x140
#define SATA_PHY_OOB_TERM		0x144
#define SATA_PHY_CDR_CTRL0		0x148
#define SATA_PHY_CDR_CTRL1		0x14C
#define SATA_PHY_CDR_CTRL2		0x150
#define SATA_PHY_CDR_CTRL3		0x154
#define SATA_PHY_CDR_CTRL4		0x158
#define SATA_PHY_FA_LOAD0		0x15C
#define SATA_PHY_FA_LOAD1		0x160
#define SATA_PHY_CDR_CTRL_RESV		0x164
#define SATA_PHY_PI_CTRL0		0x168
#define SATA_PHY_PI_CTRL1		0x16C
#define SATA_PHY_DESER_RESV		0x170
#define SATA_PHY_RX_RESV0		0x174
#define SATA_PHY_AD_TPA_CTRL		0x178
#define SATA_PHY_REFCLK_CTRL		0x17C
#define SATA_PHY_POW_DWN_CTRL0		0x180
#define SATA_PHY_POW_DWN_CTRL1		0x184
#define SATA_PHY_TX_DATA_CTRL		0x188
#define SATA_PHY_BIST_GEN0		0x18C
#define SATA_PHY_BIST_GEN1		0x190
#define SATA_PHY_BIST_GEN2		0x194
#define SATA_PHY_BIST_GEN3		0x198
#define SATA_PHY_LBK_CTRL		0x19C
#define SATA_PHY_TEST_DEBUG_CTRL	0x1A0
#define SATA_PHY_ALIGNP			0x1A4
#define SATA_PHY_PRBS_CFG0		0x1A8
#define SATA_PHY_PRBS_CFG1		0x1AC
#define SATA_PHY_PRBS_CFG2		0x1B0
#define SATA_PHY_PRBS_CFG3		0x1B4
#define SATA_PHY_CHAN_COMP_CHK_CNT	0x1B8
#define SATA_PHY_RESET_CTRL		0x1BC
#define SATA_PHY_RX_CLR			0x1C0
#define SATA_PHY_RX_EBUF_CTRL		0x1C4
#define SATA_PHY_ID0			0x1C8
#define SATA_PHY_ID1			0x1CC
#define SATA_PHY_ID2			0x1D0
#define SATA_PHY_ID3			0x1D4
#define SATA_PHY_RX_CHK_ERR_CNT0	0x1D8
#define SATA_PHY_RX_CHK_ERR_CNT1	0x1DC
#define SATA_PHY_RX_CHK_STAT		0x1E0
#define SATA_PHY_TX_IMCAL_STAT		0x1E4
#define SATA_PHY_RX_IMCAL_STAT		0x1E8
#define SATA_PHY_RX_EBUF_STAT		0x1EC
#define SATA_PHY_DEBUG_BUS_STAT0	0x1F0
#define SATA_PHY_DEBUG_BUS_STAT1	0x1F4
#define SATA_PHY_DEBUG_BUS_STAT2	0x1F8
#define SATA_PHY_DEBUG_BUS_STAT3	0x1FC

#define AHCI_HOST_CAP		0x00
#define AHCI_HOST_CAP_MASK	0x1F
#define AHCI_HOST_CAP_PMP	(1 << 17)

struct msm_sata_hba {
	struct platform_device *ahci_pdev;
	struct clk *slave_iface_clk;
	struct clk *bus_clk;
	struct clk *iface_clk;
	struct clk *src_clk;
	struct clk *rxoob_clk;
	struct clk *pmalive_clk;
	struct clk *cfg_clk;
	struct regulator *clk_pwr;
	struct regulator *pmp_pwr;
	void __iomem *phy_base;
	void __iomem *ahci_base;
};

static inline void msm_sata_delay_us(unsigned int delay)
{
	/* sleep for max. 50us more to combine processor wakeups */
	usleep_range(delay, delay + 50);
}

static int msm_sata_clk_get_prepare_enable_set_rate(struct device *dev,
		const char *name, struct clk **out_clk, int rate)
{
	int ret = 0;
	struct clk *clk;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		dev_err(dev, "failed to get clk: %s err = %d\n", name, ret);
		goto out;
	}

	if (rate >= 0) {
		ret = clk_set_rate(clk, rate);
		if (ret) {
			dev_err(dev, "failed to set rate: %d clk: %s err = %d\n",
					rate, name, ret);
			goto out;
		}
	}

	ret = clk_prepare_enable(clk);
	if (ret)
		dev_err(dev, "failed to enable clk: %s err = %d\n", name, ret);
out:
	if (!ret)
		*out_clk = clk;

	return ret;
}

static int msm_sata_clk_get_prepare_enable(struct device *dev,
		const char *name, struct clk **out_clk)
{
	return msm_sata_clk_get_prepare_enable_set_rate(dev, name, out_clk, -1);
}

static void msm_sata_clk_put_unprepare_disable(struct clk **clk)
{
	if (*clk) {
		clk_disable_unprepare(*clk);
		clk_put(*clk);
		*clk = NULL;
	}
}

static int msm_sata_hard_reset(struct device *dev)
{
	int ret;
	struct msm_sata_hba *hba = dev_get_drvdata(dev);

	ret = clk_reset(hba->iface_clk, CLK_RESET_ASSERT);
	if (ret) {
		dev_err(dev, "iface_clk assert failed %d\n", ret);
		goto out;
	}

	ret = clk_reset(hba->iface_clk, CLK_RESET_DEASSERT);
	if (ret) {
		dev_err(dev, "iface_clk de-assert failed %d\n", ret);
		goto out;
	}
out:
	return ret;
}

static int msm_sata_clk_init(struct device *dev)
{
	int ret = 0;
	struct msm_sata_hba *hba = dev_get_drvdata(dev);

	/* Enable AHB clock for system fabric slave port connected to SATA */
	ret = msm_sata_clk_get_prepare_enable(dev,
			"slave_iface_clk", &hba->slave_iface_clk);
	if (ret)
		goto out;

	/* Enable AHB clock for system fabric and SATA core interface */
	ret = msm_sata_clk_get_prepare_enable(dev,
			"iface_clk", &hba->iface_clk);
	if (ret)
		goto put_dis_slave_iface_clk;

	/* Enable AXI clock for SATA AXI master and slave interfaces */
	ret = msm_sata_clk_get_prepare_enable(dev,
			"bus_clk", &hba->bus_clk);
	if (ret)
		goto put_dis_iface_clk;

	/* Enable the source clock for pmalive, rxoob and phy ref clocks */
	ret = msm_sata_clk_get_prepare_enable_set_rate(dev,
			"src_clk", &hba->src_clk, 100000000);
	if (ret)
		goto put_dis_bus_clk;

	/*
	 * Enable RX OOB detection clock. The clock rate is
	 * same as PHY reference clock (100MHz).
	 */
	ret = msm_sata_clk_get_prepare_enable(dev,
			"core_rxoob_clk", &hba->rxoob_clk);
	if (ret)
		goto put_dis_src_clk;

	/*
	 * Enable power management always-on clock. The clock rate
	 * is same as PHY reference clock (100MHz).
	 */
	ret = msm_sata_clk_get_prepare_enable(dev,
			"core_pmalive_clk", &hba->pmalive_clk);
	if (ret)
		goto put_dis_rxoob_clk;

	/* Enable PHY configuration AHB clock, fixed 64MHz clock */
	ret = msm_sata_clk_get_prepare_enable(dev,
			"cfg_clk", &hba->cfg_clk);
	if (ret)
		goto put_dis_pmalive_clk;

	return ret;

put_dis_pmalive_clk:
	msm_sata_clk_put_unprepare_disable(&hba->pmalive_clk);
put_dis_rxoob_clk:
	msm_sata_clk_put_unprepare_disable(&hba->rxoob_clk);
put_dis_src_clk:
	msm_sata_clk_put_unprepare_disable(&hba->src_clk);
put_dis_bus_clk:
	msm_sata_clk_put_unprepare_disable(&hba->bus_clk);
put_dis_iface_clk:
	msm_sata_clk_put_unprepare_disable(&hba->iface_clk);
put_dis_slave_iface_clk:
	msm_sata_clk_put_unprepare_disable(&hba->slave_iface_clk);
out:
	return ret;
}

static void msm_sata_clk_deinit(struct device *dev)
{
	struct msm_sata_hba *hba = dev_get_drvdata(dev);

	msm_sata_clk_put_unprepare_disable(&hba->cfg_clk);
	msm_sata_clk_put_unprepare_disable(&hba->pmalive_clk);
	msm_sata_clk_put_unprepare_disable(&hba->rxoob_clk);
	msm_sata_clk_put_unprepare_disable(&hba->src_clk);
	msm_sata_clk_put_unprepare_disable(&hba->bus_clk);
	msm_sata_clk_put_unprepare_disable(&hba->iface_clk);
	msm_sata_clk_put_unprepare_disable(&hba->slave_iface_clk);
}

static int msm_sata_vreg_get_enable_set_vdd(struct device *dev,
			const char *name, struct regulator **out_vreg,
			int min_uV, int max_uV, int hpm_uA)
{
	int ret = 0;
	struct regulator *vreg;

	vreg = devm_regulator_get(dev, name);
	if (IS_ERR(vreg)) {
		ret = PTR_ERR(vreg);
		dev_err(dev, "Regulator: %s get failed, err=%d\n", name, ret);
		goto out;
	}

	if (regulator_count_voltages(vreg) > 0) {
		ret = regulator_set_voltage(vreg, min_uV, max_uV);
		if (ret) {
			dev_err(dev, "Regulator: %s set voltage failed, err=%d\n",
					name, ret);
			goto err;
		}

		ret = regulator_set_optimum_mode(vreg, hpm_uA);
		if (ret < 0) {
			dev_err(dev, "Regulator: %s set optimum mode(uA_load=%d) failed, err=%d\n",
					name, hpm_uA, ret);
			goto err;
		} else {
			/*
			 * regulator_set_optimum_mode() can return non zero
			 * value even for success case.
			 */
			ret = 0;
		}
	}

	ret = regulator_enable(vreg);
	if (ret)
		dev_err(dev, "Regulator: %s enable failed, err=%d\n",
				name, ret);
err:
	if (!ret)
		*out_vreg = vreg;
	else
		devm_regulator_put(vreg);
out:
	return ret;
}

static int msm_sata_vreg_put_disable(struct device *dev,
		struct regulator *reg, const char *name, int max_uV)
{
	int ret;

	if (!reg)
		return 0;

	ret = regulator_disable(reg);
	if (ret) {
		dev_err(dev, "Regulator: %s disable failed err=%d\n",
				name, ret);
		goto err;
	}

	if (regulator_count_voltages(reg) > 0) {
		ret = regulator_set_voltage(reg, 0, max_uV);
		if (ret < 0) {
			dev_err(dev, "Regulator: %s set voltage to 0 failed, err=%d\n",
					name, ret);
			goto err;
		}

		ret = regulator_set_optimum_mode(reg, 0);
		if (ret < 0) {
			dev_err(dev, "Regulator: %s set optimum mode(uA_load = 0) failed, err=%d\n",
					name, ret);
			goto err;
		} else {
			/*
			 * regulator_set_optimum_mode() can return non zero
			 * value even for success case.
			 */
			ret = 0;
		}
	}

err:
	devm_regulator_put(reg);
	return ret;
}

static int msm_sata_vreg_init(struct device *dev)
{
	int ret = 0;
	struct msm_sata_hba *hba = dev_get_drvdata(dev);

	/*
	 * The SATA clock generator needs 3.3V supply and can consume
	 * max. 850mA during functional mode.
	 */
	ret = msm_sata_vreg_get_enable_set_vdd(dev, "sata_ext_3p3v",
				&hba->clk_pwr, 3300000, 3300000, 850000);
	if (ret)
		goto out;

	/* Add 1ms regulator ramp-up delay */
	msm_sata_delay_us(1000);

	/* Read AHCI capability register to check if PMP is supported.*/
	if (readl_relaxed(hba->ahci_base +
				AHCI_HOST_CAP) & AHCI_HOST_CAP_PMP) {
		/* Power up port-multiplier */
		ret = msm_sata_vreg_get_enable_set_vdd(dev, "sata_pmp_pwr",
				&hba->pmp_pwr, 1800000, 1800000, 200000);
		if (ret) {
			msm_sata_vreg_put_disable(dev, hba->clk_pwr,
					"sata_ext_3p3v", 3300000);
			goto out;
		}

		/* Add 1ms regulator ramp-up delay */
		msm_sata_delay_us(1000);
	}

out:
	return ret;
}

static void msm_sata_vreg_deinit(struct device *dev)
{
	struct msm_sata_hba *hba = dev_get_drvdata(dev);

	msm_sata_vreg_put_disable(dev, hba->clk_pwr,
			"sata_ext_3p3v", 3300000);

	if (hba->pmp_pwr)
		msm_sata_vreg_put_disable(dev, hba->pmp_pwr,
				"sata_pmp_pwr", 1800000);
}

static void msm_sata_phy_deinit(struct device *dev)
{
	struct msm_sata_hba *hba = dev_get_drvdata(dev);

	/* Power down PHY */
	writel_relaxed(0xF8, hba->phy_base + SATA_PHY_POW_DWN_CTRL0);
	writel_relaxed(0xFE, hba->phy_base + SATA_PHY_POW_DWN_CTRL1);

	/* Power down PLL block */
	writel_relaxed(0x00, hba->phy_base + UNIPHY_PLL_GLB_CFG);
	mb();

	devm_iounmap(dev, hba->phy_base);
}

static int msm_sata_phy_init(struct device *dev)
{
	int ret = 0;
	u32 reg = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_sata_hba *hba = dev_get_drvdata(dev);
	struct resource *mem;

	mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy_mem");
	if (!mem) {
		dev_err(dev, "no mmio space\n");
		return -EINVAL;
	}

	hba->phy_base = devm_ioremap(dev, mem->start, resource_size(mem));
	if (!hba->phy_base) {
		dev_err(dev, "failed to allocate memory for SATA PHY\n");
		return -ENOMEM;
	}

	/* SATA phy initialization */

	writel_relaxed(0x01, hba->phy_base + SATA_PHY_SER_CTRL);

	writel_relaxed(0xB1, hba->phy_base + SATA_PHY_POW_DWN_CTRL0);
	mb();
	msm_sata_delay_us(10);

	writel_relaxed(0x01, hba->phy_base + SATA_PHY_POW_DWN_CTRL0);
	writel_relaxed(0x3E, hba->phy_base + SATA_PHY_POW_DWN_CTRL1);
	writel_relaxed(0x01, hba->phy_base + SATA_PHY_RX_IMCAL0);
	writel_relaxed(0x01, hba->phy_base + SATA_PHY_TX_IMCAL0);
	writel_relaxed(0x02, hba->phy_base + SATA_PHY_TX_IMCAL2);

	/* Write UNIPHYPLL registers to configure PLL */
	writel_relaxed(0x04, hba->phy_base + UNIPHY_PLL_REFCLK_CFG);
	writel_relaxed(0x00, hba->phy_base + UNIPHY_PLL_PWRGEN_CFG);

	writel_relaxed(0x0A, hba->phy_base + UNIPHY_PLL_CAL_CFG0);
	writel_relaxed(0xF3, hba->phy_base + UNIPHY_PLL_CAL_CFG8);
	writel_relaxed(0x01, hba->phy_base + UNIPHY_PLL_CAL_CFG9);
	writel_relaxed(0xED, hba->phy_base + UNIPHY_PLL_CAL_CFG10);
	writel_relaxed(0x02, hba->phy_base + UNIPHY_PLL_CAL_CFG11);

	writel_relaxed(0x36, hba->phy_base + UNIPHY_PLL_SDM_CFG0);
	writel_relaxed(0x0D, hba->phy_base + UNIPHY_PLL_SDM_CFG1);
	writel_relaxed(0xA3, hba->phy_base + UNIPHY_PLL_SDM_CFG2);
	writel_relaxed(0xF0, hba->phy_base + UNIPHY_PLL_SDM_CFG3);
	writel_relaxed(0x00, hba->phy_base + UNIPHY_PLL_SDM_CFG4);

	writel_relaxed(0x19, hba->phy_base + UNIPHY_PLL_SSC_CFG0);
	writel_relaxed(0xE1, hba->phy_base + UNIPHY_PLL_SSC_CFG1);
	writel_relaxed(0x00, hba->phy_base + UNIPHY_PLL_SSC_CFG2);
	writel_relaxed(0x11, hba->phy_base + UNIPHY_PLL_SSC_CFG3);

	writel_relaxed(0x04, hba->phy_base + UNIPHY_PLL_LKDET_CFG0);
	writel_relaxed(0xFF, hba->phy_base + UNIPHY_PLL_LKDET_CFG1);

	writel_relaxed(0x02, hba->phy_base + UNIPHY_PLL_GLB_CFG);
	mb();
	msm_sata_delay_us(40);

	writel_relaxed(0x03, hba->phy_base + UNIPHY_PLL_GLB_CFG);
	mb();
	msm_sata_delay_us(400);

	writel_relaxed(0x05, hba->phy_base + UNIPHY_PLL_LKDET_CFG2);
	mb();

	/* poll for ready status, timeout after 1 sec */
	ret = readl_poll_timeout(hba->phy_base + UNIPHY_PLL_STATUS, reg,
			(reg & 1 << 0), 100, 1000000);
	if (ret) {
		dev_err(dev, "poll timeout UNIPHY_PLL_STATUS\n");
		goto out;
	}

	ret = readl_poll_timeout(hba->phy_base + SATA_PHY_TX_IMCAL_STAT, reg,
			(reg & 1 << 0), 100, 1000000);
	if (ret) {
		dev_err(dev, "poll timeout SATA_PHY_TX_IMCAL_STAT\n");
		goto out;
	}

	ret = readl_poll_timeout(hba->phy_base + SATA_PHY_RX_IMCAL_STAT, reg,
			(reg & 1 << 0), 100, 1000000);
	if (ret) {
		dev_err(dev, "poll timeout SATA_PHY_RX_IMCAL_STAT\n");
		goto out;
	}

	/* SATA phy calibrated succesfully, power up to functional mode */
	writel_relaxed(0x3E, hba->phy_base + SATA_PHY_POW_DWN_CTRL1);
	writel_relaxed(0x01, hba->phy_base + SATA_PHY_RX_IMCAL0);
	writel_relaxed(0x01, hba->phy_base + SATA_PHY_TX_IMCAL0);

	writel_relaxed(0x00, hba->phy_base + SATA_PHY_POW_DWN_CTRL1);
	writel_relaxed(0x59, hba->phy_base + SATA_PHY_CDR_CTRL0);
	writel_relaxed(0x04, hba->phy_base + SATA_PHY_CDR_CTRL1);
	writel_relaxed(0x00, hba->phy_base + SATA_PHY_CDR_CTRL2);
	writel_relaxed(0x00, hba->phy_base + SATA_PHY_PI_CTRL0);
	writel_relaxed(0x00, hba->phy_base + SATA_PHY_CDR_CTRL3);
	writel_relaxed(0x01, hba->phy_base + SATA_PHY_POW_DWN_CTRL0);

	writel_relaxed(0x11, hba->phy_base + SATA_PHY_TX_DATA_CTRL);
	writel_relaxed(0x43, hba->phy_base + SATA_PHY_ALIGNP);
	writel_relaxed(0x04, hba->phy_base + SATA_PHY_OOB_TERM);

	writel_relaxed(0x01, hba->phy_base + SATA_PHY_EQUAL);
	writel_relaxed(0x09, hba->phy_base + SATA_PHY_TX_DRIV_CTRL0);
	writel_relaxed(0x09, hba->phy_base + SATA_PHY_TX_DRIV_CTRL1);
	mb();

	dev_dbg(dev, "SATA PHY powered up in functional mode\n");

out:
	/* power down PHY in case of failure */
	if (ret)
		msm_sata_phy_deinit(dev);

	return ret;
}

int msm_sata_init(struct device *ahci_dev, void __iomem *mmio)
{
	int ret;
	struct device *dev = ahci_dev->parent;
	struct msm_sata_hba *hba = dev_get_drvdata(dev);

	/* Save ahci mmio to access vendor specific registers */
	hba->ahci_base = mmio;

	ret = msm_sata_clk_init(dev);
	if (ret) {
		dev_err(dev, "SATA clk init failed with err=%d\n", ret);
		goto out;
	}

	ret = msm_sata_vreg_init(dev);
	if (ret) {
		dev_err(dev, "SATA vreg init failed with err=%d\n", ret);
		msm_sata_clk_deinit(dev);
		goto out;
	}

	ret = msm_sata_phy_init(dev);
	if (ret) {
		dev_err(dev, "SATA PHY init failed with err=%d\n", ret);
		msm_sata_vreg_deinit(dev);
		msm_sata_clk_deinit(dev);
		goto out;
	}

out:
	return ret;
}

void msm_sata_deinit(struct device *ahci_dev)
{
	struct device *dev = ahci_dev->parent;

	msm_sata_phy_deinit(dev);
	msm_sata_vreg_deinit(dev);
	msm_sata_clk_deinit(dev);
}

static int msm_sata_suspend(struct device *ahci_dev)
{
	msm_sata_deinit(ahci_dev);

	return 0;
}

static int msm_sata_resume(struct device *ahci_dev)
{
	int ret;
	struct device *dev = ahci_dev->parent;

	ret = msm_sata_clk_init(dev);
	if (ret) {
		dev_err(dev, "SATA clk init failed with err=%d\n", ret);
		/*
		 * If clock initialization failed, that means ahci driver
		 * cannot access any register going further. Since there is
		 * no check within ahci driver to check for clock failures,
		 * panic here instead of making an unclocked register access.
		 */
		BUG();
	}

	/* Issue asynchronous reset to reset PHY */
	ret = msm_sata_hard_reset(dev);
	if (ret)
		goto out;

	ret = msm_sata_vreg_init(dev);
	if (ret) {
		dev_err(dev, "SATA vreg init failed with err=%d\n", ret);
		/* Do not turn off clks, AHCI driver might do register access */
		goto out;
	}

	ret = msm_sata_phy_init(dev);
	if (ret) {
		dev_err(dev, "SATA PHY init failed with err=%d\n", ret);
		/* Do not turn off clks, AHCI driver might do register access */
		msm_sata_vreg_deinit(dev);
		goto out;
	}
out:
	return ret;
}

static struct ahci_platform_data msm_ahci_pdata = {
	.init = msm_sata_init,
	.exit = msm_sata_deinit,
	.suspend = msm_sata_suspend,
	.resume = msm_sata_resume,
};

static int __devinit msm_sata_probe(struct platform_device *pdev)
{
	struct platform_device *ahci;
	struct msm_sata_hba *hba;
	int ret = 0;

	hba = devm_kzalloc(&pdev->dev, sizeof(struct msm_sata_hba), GFP_KERNEL);
	if (!hba) {
		dev_err(&pdev->dev, "no memory\n");
		ret = -ENOMEM;
		goto err;
	}

	platform_set_drvdata(pdev, hba);

	ahci = platform_device_alloc("ahci", pdev->id);
	if (!ahci) {
		dev_err(&pdev->dev, "couldn't allocate ahci device\n");
		ret = -ENOMEM;
		goto err_free;
	}

	dma_set_coherent_mask(&ahci->dev, pdev->dev.coherent_dma_mask);

	ahci->dev.parent = &pdev->dev;
	ahci->dev.dma_mask = pdev->dev.dma_mask;
	ahci->dev.dma_parms = pdev->dev.dma_parms;
	hba->ahci_pdev = ahci;

	ret = platform_device_add_resources(ahci, pdev->resource,
			pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "couldn't add resources to ahci device\n");
		goto err_put_device;
	}

	ahci->dev.platform_data = &msm_ahci_pdata;
	ret = platform_device_add(ahci);
	if (ret) {
		dev_err(&pdev->dev, "failed to register ahci device\n");
		goto err_put_device;
	}

	return 0;

err_put_device:
	platform_device_put(ahci);
err_free:
	devm_kfree(&pdev->dev, hba);
err:
	return ret;
}

static int __devexit msm_sata_remove(struct platform_device *pdev)
{
	struct msm_sata_hba *hba = platform_get_drvdata(pdev);

	platform_device_unregister(hba->ahci_pdev);

	return 0;
}

static struct platform_driver msm_sata_driver = {
	.probe		= msm_sata_probe,
	.remove		= __devexit_p(msm_sata_remove),
	.driver		= {
		.name	= "msm_sata",
	},
};

module_platform_driver(msm_sata_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("AHCI platform MSM Glue Layer");
