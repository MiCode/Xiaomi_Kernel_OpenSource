/*
 * Copyright (C) 2019 MediaTek Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pinctrl/pinctrl.h>
#include "ufs.h"
#include "ufshcd.h"
#include "ufs_quirks.h"
#include "unipro.h"
#include "ufs-mtk.h"
#include "ufs-mtk-platform.h"
#include "mtk_idle.h"
#include "mtk_spm_resource_req.h"
#include "mtk_secure_api.h"

#ifdef MTK_UFS_HQA
#include <mtk_reboot.h>
#endif

#ifdef UPMU_READY
#include <mt-plat/upmu_common.h>
#endif

static void __iomem *ufs_mtk_mmio_base_gpio;
static void __iomem *ufs_mtk_mmio_base_topckgen;
static void __iomem *ufs_mtk_mmio_base_infracfg_ao;
static void __iomem *ufs_mtk_mmio_base_apmixed;
static void __iomem *ufs_mtk_mmio_base_ufs_mphy;

#ifdef CONFIG_MACH_MT6833
#define APMIXEDSYS_DTS_NAME "mediatek,mt6833-apmixedsys"
#else
#define APMIXEDSYS_DTS_NAME "mediatek,mt6853-apmixedsys"
#endif

/**
 * ufs_mtk_pltfrm_pwr_change_final_gear - change pwr mode fianl gear value.
 */
void ufs_mtk_pltfrm_pwr_change_final_gear(struct ufs_hba *hba,
	struct ufs_pa_layer_attr *final)
{
	/* Change final gear if necessary */
}

#ifdef MTK_UFS_HQA

/* UFS write is performance 200MB/s, 4KB will take around 20us */
/* Set delay before reset to 100us to cover 4KB & >4KB write */

#define UFS_SPOH_USDELAY_BEFORE_RESET (100)

void random_delay(struct ufs_hba *hba)
{
	u32 time = (sched_clock()%UFS_SPOH_USDELAY_BEFORE_RESET);

	udelay(time);
}
void wdt_pmic_full_reset(struct ufs_hba *hba)
{
	/* disable irq first to prevent some unpredict interrupts */
	ufshcd_disable_intr(hba, hba->intr_mask);

	/* power off device VCC */
	pmic_set_register_value_nolock(PMIC_RG_LDO_VEMC_EN, 0);

	/* power off device VCCQ2 1.8V */
	pmic_set_register_value_nolock(PMIC_RG_LDO_VUFS_EN, 0);

	/* Disable VIO18 is to power off device VCCQ 1.2V, but
	 * since this is an important power source. This will
	 * cause PMIC shutdown. At this cause all of the power
	 * will be shutdown instantly.
	 *
	 * Press power key or plug in USB to power up again.
	 */
	/* mark the code since we do not use EXT LDO for VCCQ 1.2V */
	/* pmic_set_register_value_nolock(PMIC_RG_LDO_VIO18_EN, 0); */

	/* PMIC cold reset */
	pmic_set_register_value_nolock(PMIC_RG_CRST, 1);
}
#endif

#define PMIC_REG_MASK (0xFFFF)
#define PMIC_REG_SHIFT (0)
void ufs_mtk_pltfrm_gpio_trigger_and_debugInfo_dump(struct ufs_hba *hba)
{
#ifdef UPMU_READY
	int vcc_enabled, vcc_0_value, vcc_1_value;
	int vccq2_enabled, va12_enabled;
	u32 val_0, val_1;

	/* check ufs debug register */
	ufshcd_writel(hba, 0x20, REG_UFS_MTK_DEBUG_SEL);
	val_0 = ufshcd_readl(hba, REG_UFS_MTK_PROBE);
	dev_info(hba->dev, "REG_UFS_MTK_PROBE: 0x%x\n", val_0);

	/* check power status */
	vcc_enabled = pmic_get_register_value(PMIC_RG_LDO_VEMC_EN);
	vcc_0_value = pmic_get_register_value(PMIC_RG_VEMC_VOSEL_0);
	vcc_1_value = pmic_get_register_value(PMIC_RG_VEMC_VOSEL_1);
	vccq2_enabled = pmic_get_register_value(PMIC_DA_EXT_PMIC_EN1);
	va12_enabled = pmic_get_register_value(PMIC_RG_LDO_VA12_EN);
	/* dump vcc, vccq2 info */
	dev_info(hba->dev,
		"vcc_en:%d, vcc_0:%d, vcc_1:%d, vccq2_en:%d, va12_en:%d\n",
		vcc_enabled, vcc_0_value, vcc_1_value,
		vccq2_enabled, va12_enabled);

	if (ufs_mtk_mmio_base_infracfg_ao) {
		val_0 = readl(ufs_mtk_mmio_base_infracfg_ao + CLK_CG_2_STA);
		val_1 = readl(ufs_mtk_mmio_base_infracfg_ao + CLK_CG_3_STA);
		dev_info(hba->dev,
			"infracfg_ao: CLK_CG_2_STA = 0x%x, CLK_CG_3_STA = 0x%x\n",
			val_0, val_1);
		dev_info(hba->dev,
			"UNIPRO_SYSCLK_CG(%d) UNIPRO_TICK_CG(%d) UFS_MP_SAP_BCLK_CG(%d)\n",
			!!(val_0 & INFRACFG_AO_UNIPRO_SYSCLK_CG),
			!!(val_0 & INFRACFG_AO_UNIPRO_TICK_CG),
			!!(val_0 & INFRACFG_AO_UFS_MP_SAP_BCLK_CG));
		dev_info(hba->dev,
			"UFS_CG(%d) AES_UFSFDE_CG(%d) UFS_TICK_CG(%d)\n",
			!!(val_0 & INFRACFG_AO_UFS_CG),
			!!(val_0 & INFRACFG_AO_AES_UFSFDE_CG),
			!!(val_0 & INFRACFG_AO_UFS_TICK_CG));
		dev_info(hba->dev,
			"UFS_AXI_CG(%d)\n",
			!!(val_1 & INFRACFG_AO_UFS_AXI_CG));
	}

	if (ufs_mtk_mmio_base_apmixed) {
		val_0 = readl(ufs_mtk_mmio_base_apmixed + PLL_MAINPLL);
		val_1 = readl(ufs_mtk_mmio_base_apmixed + PLL_MSDCPLL);
		dev_info(hba->dev, "apmixed: PLL_MAINPLL = 0x%x, PLL_MSDCPLL = 0x%x\n",
			val_0, val_1);
	}

	if (ufs_mtk_mmio_base_topckgen) {
		val_0 = readl(ufs_mtk_mmio_base_topckgen + CLK_CFG_12);
		dev_info(hba->dev, "topckgen: CLK_CFG_12 = 0x%x\n",
			val_0);
	}
#endif
}

#ifdef CONFIG_MTK_UFS_DEGUG_GPIO_TRIGGER
#include <mt-plat/sync_write.h>

struct device_node *msdc_gpio_node;
void __iomem *msdc_gpio_base;
static struct regulator *vmc;

#define MSDC1_GPIO_MODE17           (msdc_gpio_base + 0x410)
#define MSDC1_GPIO_DIR4             (msdc_gpio_base + 0x40)
#define MSDC1_GPIO_DOUT4            (msdc_gpio_base + 0x140)

#define MSDC1_GPIO_DOUT             (MSDC1_GPIO_DOUT4)
#define MSDC1_GPIO_DOUT_DIR_FIELD   (0x00000F00)
#define MSDC1_GPIO_DOUT_DIR_VAL_SET (0x0000000F)
#define MSDC1_GPIO_DOUT_DIR_VAL_CLR (0x00000000)

#define MSDC_READ32(reg)          __raw_readl(reg)
#define MSDC_WRITE32(reg, val)    mt_reg_sync_writel(val, reg)
static inline unsigned int uffs(unsigned int x)
{
	unsigned int r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}
	return r;
}
#define MSDC_SET_FIELD(reg, field, val) \
	do { \
		unsigned int tv = MSDC_READ32(reg); \
		tv &= ~(field); \
		tv |= ((val) << (uffs((unsigned int)field) - 1)); \
		MSDC_WRITE32(reg, tv); \
	} while (0)

void ufs_mtk_pltfrm_gpio_trigger_init(struct ufs_hba *hba)
{
	/* Need porting for UFS Debug*/
	/* Set gpio mode */
	/* Set gpio output */
	/* Set gpio default low */
	dev_info(hba->dev, "%s: trigger_gpio_init!!!\n",
				__func__);

	if (msdc_gpio_node == NULL) {
		msdc_gpio_node = of_find_compatible_node(NULL, NULL,
			"mediatek,GPIO");
		msdc_gpio_base = of_iomap(msdc_gpio_node, 0);
		pr_notice("of_iomap for gpio base @ 0x%p\n", msdc_gpio_base);
	};

	/*
	 * Power on
	 * Be sure add dts in ufs node
	 * vqmmc-supply = <&mt_pmic_vmc_ldo_reg>;
	 */
	vmc = regulator_get(hba->dev, "vqmmc");
	if (!IS_ERR(vmc)) {
		regulator_set_voltage(vmc, VOL_3000 * 1000, VOL_3000 * 1000);
		regulator_enable(vmc);
	}

	/*
	 * Set gpio to gpio mode. (dat0/dat1/dat2/dat3 in GPIO)
	 * Be sure remove msdc_set_pin_mode in msdc_cust.c set mode
	 * back to msdc.
	 */
	MSDC_SET_FIELD(MSDC1_GPIO_MODE17, 0x0000FFFF, 0x0);

	/*
	 * Set gpio dir output. (dat0/dat1/dat2/dat3 in GPIO)
	 */
	MSDC_SET_FIELD(MSDC1_GPIO_DIR4,
		MSDC1_GPIO_DOUT_DIR_FIELD,
		MSDC1_GPIO_DOUT_DIR_VAL_SET);

	/*
	 * Set gpio output 0. (dat0/dat1/dat2/dat3 in GPIO)
	 */
	MSDC_SET_FIELD(MSDC1_GPIO_DOUT,
		MSDC1_GPIO_DOUT_DIR_FIELD,
		MSDC1_GPIO_DOUT_DIR_VAL_CLR);
}

void ufs_mtk_pltfrm_gpio_trigger(int value)
{
	if (-EPROBE_DEFER == PTR_ERR(vmc)) {
		vmc = regulator_get(ufs_mtk_hba->dev, "vqmmc");
		if (!IS_ERR(vmc)) {
			regulator_set_voltage(vmc,
				VOL_3000 * 1000, VOL_3000 * 1000);
			regulator_enable(vmc);
		}
	}

	if (value == 1) {
		MSDC_SET_FIELD(MSDC1_GPIO_DOUT,
			MSDC1_GPIO_DOUT_DIR_FIELD,
			MSDC1_GPIO_DOUT_DIR_VAL_SET);
	} else {
		MSDC_SET_FIELD(MSDC1_GPIO_DOUT,
			MSDC1_GPIO_DOUT_DIR_FIELD,
			MSDC1_GPIO_DOUT_DIR_VAL_CLR);
	}

	pr_notice("error trigger, %d\n", value);

	pr_info("%s vmc=%d uv\n", __func__, regulator_get_voltage(vmc));

	pr_info("%s mode=0x%x, dir=0x%x, out=0x%x\n", __func__,
		MSDC_READ32(MSDC1_GPIO_MODE17),
		MSDC_READ32(MSDC1_GPIO_DIR4),
		MSDC_READ32(MSDC1_GPIO_DOUT));
}
#endif

int ufs_mtk_pltfrm_xo_ufs_req(struct ufs_hba *hba, bool on)
{
	u32 value;
	int retry;

	if (!hba->card) {
		/*
		 * In case card is not init, just ignore it.
		 * Because clock is default on.
		 * After card init done the clk control
		 * will be normal.
		 */
		dev_info(hba->dev, "%s: card not init skip\n",
			__func__);
		return 0;
	}

	/* inform ATF clock is on */
	if (on)
		mt_secure_call(MTK_SIP_KERNEL_UFS_CTL, 8, 1, 0, 0);

	/*
	 * Delay before disable ref-clk: H8 -> delay A -> disable ref-clk
	 *		delayA
	 * Hynix	30us
	 * Samsung	1us
	 * Toshiba	100us
	 */
	if (!on) {
		switch (hba->card->wmanufacturerid) {
		case UFS_VENDOR_TOSHIBA:
			udelay(100);
			break;
		case UFS_VENDOR_SKHYNIX:
			udelay(30);
			break;
		case UFS_VENDOR_SAMSUNG:
			udelay(1);
			break;
		default:
			udelay(30);
			break;
		}
	}

	/*
	 * REG_UFS_ADDR_XOUFS_ST[0] is xoufs_req_s
	 * REG_UFS_ADDR_XOUFS_ST[1] is xoufs_ack_s
	 * xoufs_req_s is used for XOUFS Clock request to SPI
	 * SW set xoufs_ack_s to trigger Clock Request for XOUFS, and
	 * check xoufs_ack_s set for clock avialable.
	 * SW clear xoufs_ack_s to trigger Clock Release for XOUFS, and
	 * check xoufs_ack_s clear for clock off.
	 */

	if (on)
		ufshcd_writel(hba, 1, REG_UFS_ADDR_XOUFS_ST);
	else
		ufshcd_writel(hba, 0, REG_UFS_ADDR_XOUFS_ST);

	retry = 48; /* 2.4ms wosrt case */
	do {
		value = ufshcd_readl(hba, REG_UFS_ADDR_XOUFS_ST);

		if ((value == 0x3) || (value == 0))
			break;

		udelay(50);
		if (retry) {
			retry--;
		} else {
			dev_notice(hba->dev, "XO_UFS ack failed\n");
			return -EIO;
		}
	} while (1);


	/* Delay after enable ref-clk: enable ref-clk -> delay B -> leave H8
	 *		delayB
	 * Hynix	30us
	 * Samsung	max(1us,32us)
	 * Toshiba	32us
	 */
	if (on) {
		switch (hba->card->wmanufacturerid) {
		case UFS_VENDOR_TOSHIBA:
			udelay(32);
			break;
		case UFS_VENDOR_SKHYNIX:
			udelay(30);
			break;
		case UFS_VENDOR_SAMSUNG:
			udelay(32);
			break;
		default:
			udelay(30);
			break;
		}
	}

	/* inform ATF clock is off */
	if (!on)
		mt_secure_call(MTK_SIP_KERNEL_UFS_CTL, 8, 0, 0, 0);

	return 0;
}

int ufs_mtk_pltfrm_ufs_device_reset(struct ufs_hba *hba)
{
	mt_secure_call(MTK_SIP_KERNEL_UFS_CTL, 2, 0, 0, 0);

	/*
	 * The reset signal is active low.
	 * The UFS device shall detect more than or equal to 1us of positive
	 * or negative RST_n pulse width.
	 * To be on safe side, keep the reset low for at least 10us.
	 */
	usleep_range(10, 15);

	mt_secure_call(MTK_SIP_KERNEL_UFS_CTL, 2, 1, 0, 0);

	/* some device may need time to respond to rst_n */
	usleep_range(10000, 15000);

	dev_info(hba->dev, "%s: UFS device reset done\n", __func__);

	return 0;
}

/*
 * In early-porting stage, because of no bootrom,
 * something finished by bootrom shall be finished here instead.
 * Returns:
 *  0: Successful.
 *  Non-zero: Failed.
 */
int ufs_mtk_pltfrm_bootrom_deputy(struct ufs_hba *hba)
{
#ifdef CONFIG_FPGA_EARLY_PORTING
	mt_secure_call(MTK_SIP_KERNEL_UFS_CTL, 2, 1, 0, 0);
#endif
#ifdef CONFIG_MTK_UFS_DEGUG_GPIO_TRIGGER
	ufs_mtk_pltfrm_gpio_trigger_init(hba);
#endif

	return 0;
}

int ufs_mtk_pltfrm_ref_clk_ctrl(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret = 0;
	u32 val = 0;

	if (on) {
		/* Host need turn on clock by itself */
		ret = ufs_mtk_pltfrm_xo_ufs_req(hba, true);
		if (ret)
			goto out;
	} else {
		val = VENDOR_POWERSTATE_HIBERNATE;
		ufs_mtk_wait_link_state(hba, &val,
			hba->clk_gating.delay_ms);

		if (val == VENDOR_POWERSTATE_HIBERNATE) {
			/* Host need turn off clock by itself */
			ret = ufs_mtk_pltfrm_xo_ufs_req(hba, false);
			if (ret)
				goto out;
		} else {
			dev_info(hba->dev, "%s: power state (%d) clk not off\n",
				__func__, val);
			dev_info(hba->dev, "%s: ah8_en(%d), ah_reg = 0x%x\n",
				__func__,
				ufs_mtk_auto_hibern8_enabled,
				ufshcd_readl(hba,
					REG_AUTO_HIBERNATE_IDLE_TIMER));
		}
	}

out:
	return ret;
}

/**
 * ufs_mtk_deepidle_hibern8_check - callback function for Deepidle & SODI.
 * Release all resources: DRAM/26M clk/Main PLL and dsiable 26M ref clk if
 * in H8.
 *
 * @return: 0 for success, negative/postive error code otherwise
 */
int ufs_mtk_pltfrm_deepidle_check_h8(void)
{
	/* No Need */
	return 0;
}


/**
 * ufs_mtk_deepidle_leave - callback function for leaving Deepidle & SODI.
 */
void ufs_mtk_pltfrm_deepidle_leave(void)
{
	/* No Need */
}

/**
 * ufs_mtk_pltfrm_deepidle_lock - Deepidle & SODI lock.
 * @hba: per-adapter instance
 * @lock: lock or unlock deepidle & SODI entrance.
 */
void ufs_mtk_pltfrm_deepidle_lock(struct ufs_hba *hba, bool lock)
{
	/* No Need */
}

int ufs_mtk_pltfrm_host_sw_rst(struct ufs_hba *hba, u32 target)
{
	u32 reg;

	if (!ufs_mtk_mmio_base_infracfg_ao) {
		dev_info(hba->dev,
			"ufs_mtk_host_sw_rst: failed, null ufs_mtk_mmio_base_infracfg_ao.\n");
		return 1;
	}

	dev_info(hba->dev, "ufs_mtk_host_sw_rst: 0x%x\n", target);

	ufshcd_update_reg_hist(&hba->ufs_stats.sw_reset, (u32)target);

	if (target & SW_RST_TARGET_UFSHCI) {
		/* reset HCI */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UFSHCI_SW_RST_SET);
		reg = reg | (1 << REG_UFSHCI_SW_RST_SET_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UFSHCI_SW_RST_SET);
	}

	if (target & SW_RST_TARGET_UFSCPT) {
		/* reset AES */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UFSCPT_SW_RST_SET);
		reg = reg | (1 << REG_UFSCPT_SW_RST_SET_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UFSCPT_SW_RST_SET);
	}

	if (target & SW_RST_TARGET_UNIPRO) {
		/* reset UniPro */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UNIPRO_SW_RST_SET);
		reg = reg | (1 << REG_UNIPRO_SW_RST_SET_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UNIPRO_SW_RST_SET);
	}

	if (target & SW_RST_TARGET_MPHY) {
		/* reset Mphy */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UFSPHY_SW_RST_SET);
		reg = reg | (1 << REG_UFSPHY_SW_RST_SET_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UFSPHY_SW_RST_SET);
	}

	udelay(100);

	if (target & SW_RST_TARGET_MPHY) {
		/* clear Mphy reset */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UFSPHY_SW_RST_CLR);
		reg = reg | (1 << REG_UFSPHY_SW_RST_CLR_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UFSPHY_SW_RST_CLR);
	}

	if (target & SW_RST_TARGET_UNIPRO) {
		/* clear UniPro reset */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UNIPRO_SW_RST_CLR);
		reg = reg | (1 << REG_UNIPRO_SW_RST_CLR_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UNIPRO_SW_RST_CLR);
	}

	if (target & SW_RST_TARGET_UFSCPT) {
		/* clear AES reset */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UFSCPT_SW_RST_CLR);
		reg = reg | (1 << REG_UFSCPT_SW_RST_CLR_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UFSCPT_SW_RST_CLR);
	}

	if (target & SW_RST_TARGET_UFSHCI) {
		/* clear HCI reset */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UFSHCI_SW_RST_CLR);
		reg = reg | (1 << REG_UFSHCI_SW_RST_CLR_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UFSHCI_SW_RST_CLR);
	}

	udelay(100);

	return 0;
}

int ufs_mtk_pltfrm_init(void)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(ufs_mtk_hba);

	ufs_mtk_hba->caps |= UFSHCD_CAP_CLK_GATING;
	host->vreg_lpm_supported = true;

	return 0;
}

int ufs_mtk_pltfrm_parse_dt(struct ufs_hba *hba)
{
	struct device_node *node_gpio;
	struct device_node *node_ufs_mphy;
	struct device_node *node_infracfg_ao;
	struct device_node *node_apmixed;
	struct device_node *node_topckgen;

	int err = 0;

	/* get ufs_mtk_gpio */
	node_gpio = of_find_compatible_node(NULL, NULL, "mediatek,gpio");

	if (node_gpio) {
		ufs_mtk_mmio_base_gpio = of_iomap(node_gpio, 0);

		if (IS_ERR(*(void **)&ufs_mtk_mmio_base_gpio)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_gpio);
			dev_notice(hba->dev, "error: ufs_mtk_mmio_base_gpio init fail\n");
			ufs_mtk_mmio_base_gpio = NULL;
		}
	} else
		dev_notice(hba->dev, "error: node_gpio init fail\n");

	/* get ufs_mtk_mmio_base_ufs_mphy */

	node_ufs_mphy =
		of_find_compatible_node(NULL, NULL, "mediatek,ufs_mphy");

	if (node_ufs_mphy) {
		ufs_mtk_mmio_base_ufs_mphy = of_iomap(node_ufs_mphy, 0);

		if (IS_ERR(*(void **)&ufs_mtk_mmio_base_ufs_mphy)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_ufs_mphy);
			dev_notice(hba->dev, "error: ufs_mtk_mmio_base_ufs_mphy init fail\n");
			ufs_mtk_mmio_base_ufs_mphy = NULL;
		}
	} else
		dev_notice(hba->dev, "error: ufs_mtk_mmio_base_ufs_mphy init fail\n");


	/* get ufs_mtk_mmio_base_infracfg_ao */
	node_infracfg_ao =
		of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	if (node_infracfg_ao) {
		ufs_mtk_mmio_base_infracfg_ao = of_iomap(node_infracfg_ao, 0);

		if (IS_ERR(*(void **)&ufs_mtk_mmio_base_infracfg_ao)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_infracfg_ao);
			dev_notice(hba->dev, "error: ufs_mtk_mmio_base_infracfg_ao init fail\n");
			ufs_mtk_mmio_base_infracfg_ao = NULL;
		}
	} else
		dev_notice(hba->dev, "error: ufs_mtk_mmio_base_infracfg_ao init fail\n");

	/* get ufs_mtk_mmio_base_apmixed */
	node_apmixed =
		of_find_compatible_node(NULL, NULL,
				APMIXEDSYS_DTS_NAME);
	if (node_apmixed) {
		ufs_mtk_mmio_base_apmixed = of_iomap(node_apmixed, 0);

		if (IS_ERR(*(void **)&ufs_mtk_mmio_base_apmixed)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_apmixed);
			dev_notice(hba->dev, "error: ufs_mtk_mmio_base_apmixed init fail\n");
			ufs_mtk_mmio_base_apmixed = NULL;
		}
	} else
		dev_notice(hba->dev, "error: ufs_mtk_mmio_base_apmixed init fail\n");

	/* get ufs_mtk_mmio_base_topckgen */
	node_topckgen =
		of_find_compatible_node(NULL, NULL, "mediatek,topckgen");
	if (node_topckgen) {
		ufs_mtk_mmio_base_topckgen = of_iomap(node_topckgen, 0);

		if (IS_ERR(*(void **)&ufs_mtk_mmio_base_topckgen)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_topckgen);
			dev_notice(hba->dev, "error: ufs_mtk_mmio_base_topckgen init fail\n");
			ufs_mtk_mmio_base_topckgen = NULL;
		}
	} else
		dev_notice(hba->dev, "error: ufs_mtk_mmio_base_topckgen init fail\n");

	return err;
}

int ufs_mtk_pltfrm_resume(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	/*
	 * If the UFSHCD_CAP_CLK_GATING is not set,
	 * platform pm needs to handle the ref_clk itself.
	 */
	if (!ufshcd_is_clkgating_allowed(hba)) {
		ret = ufs_mtk_pltfrm_xo_ufs_req(hba, true);
		if (ret)
			goto out;
		ret = ufs_mtk_perf_setup(host, true);
		if (ret)
			goto out;
	}
out:
	return ret;
}

int ufs_mtk_pltfrm_suspend(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	/*
	 * If the UFSHCD_CAP_CLK_GATING is not set,
	 * platform pm needs to handle the ref_clk itself.
	 */
	if (!ufshcd_is_clkgating_allowed(hba)) {
		ret = ufs_mtk_perf_setup(host, false);
		if (ret)
			goto out;
		ret = ufs_mtk_pltfrm_xo_ufs_req(hba, false);
		if (ret)
			goto out;
	}
#if 0
	/* TEST ONLY: emulate UFSHCI power off by HCI SW reset */
	ufs_mtk_pltfrm_host_sw_rst(hba, SW_RST_TARGET_UFSHCI);
#endif
out:
	return ret;
}


