/*
 * Copyright (C) 2017 MediaTek Inc.
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
#ifdef SR_CLKEN_RC_READY
#include "mtk_srclken_rc.h"
#endif

#ifdef MTK_UFS_HQA
#include <mtk_reboot.h>
#endif

#ifdef UPMU_READY
#include <mt-plat/upmu_common.h>
#endif

#ifdef CLKBUF_READY
#include "mtk_clkbuf_ctl.h"
#endif

static void __iomem *ufs_mtk_mmio_base_gpio;
static void __iomem *ufs_mtk_mmio_base_infracfg_ao;
static void __iomem *ufs_mtk_mmio_base_ufs_mphy;
static struct regulator *reg_va09;

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
	/*
	 * Cmd issue to PMIC on MT6771 will take around 20us ~ 30us, in order
	 * to speed up VEMC disable time, we disable VEMC first coz PMIC cold
	 * reset may take longer to disable VEMC in it's reset flow. Can not
	 * use regulator_disable() here because it can not use in  preemption
	 * disabled context. Use pmic raw API without nlock instead.
	 */
	pmic_set_register_value_nolock(PMIC_RG_LDO_VEMC_EN, 0);

	/* Need reset external LDO for VUFS18, UFS needs VEMC&VUFS18 reset at
	 * the same time
	 */
	pmic_set_register_value_nolock(PMIC_RG_STRUP_EXT_PMIC_SEL, 0x1);

	/* VA09 off, may not require */
	/* pmic_set_register_value_nolock(PMIC_RG_STRUP_EXT_PMIC_EN, 0x1); */
	/* pmic_set_register_value_nolock(PMIC_RG_STRUP_EXT_PMIC_SEL, 0x2); */

	/* PMIC cold reset */
	pmic_set_register_value_nolock(PMIC_RG_CRST, 1);
}
#endif

#define PMIC_REG_MASK (0xFFFF)
#define PMIC_REG_SHIFT (0)
void ufs_mtk_pltfrm_gpio_trigger_and_debugInfo_dump(struct ufs_hba *hba)
{
#ifdef UPMU_READY
	int vcc_enabled, vcc_value, vccq2_enabled, va09_enabled;

	vcc_enabled = pmic_get_register_value(PMIC_RG_LDO_VEMC_EN);
	vcc_value = pmic_get_register_value(PMIC_RG_VEMC_VOSEL);
	vccq2_enabled = pmic_get_register_value(PMIC_DA_EXT_PMIC_EN1);
	va09_enabled = pmic_get_register_value(PMIC_DA_EXT_PMIC_EN1);
	/* dump vcc, vccq2 and va09 info */
	dev_info(hba->dev,
		"vcc_enabled:%d, vcc_value:%d, vccq2_enabled:%d, va09:%d!!!\n",
		vcc_enabled, vcc_value, vccq2_enabled, va09_enabled);
	/* dump clock buffer */
	clk_buf_dump_clkbuf_log();
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
#ifdef SR_CLKEN_RC_READY
	u32 value;
	int retry;

	if (srclken_get_stage() == SRCLKEN_FULL_SET) {
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

		retry = 3; /* 2.4ms wosrt case */
		do {
			value = ufshcd_readl(hba, REG_UFS_ADDR_XOUFS_ST);

			if ((value == 0x3) || (value == 0))
				break;

			mdelay(1);
			if (retry) {
				retry--;
			} else {
				dev_err(hba->dev, "XO_UFS ack failed\n");
				return -EIO;
			}
		} while (1);
	} else
		ufshcd_writel(hba, 0, REG_UFS_ADDR_XOUFS_ST);
#else
	ufshcd_writel(hba, 0, REG_UFS_ADDR_XOUFS_ST);
#endif

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

	/* same as assert, wait for at least 10us after deassert */
	usleep_range(10, 15);

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
	/* No Need for MT6785 */
	return 0;
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
#ifdef SPM_READY
	int ret = 0;
	u32 tmp = 0;

	/**
	 * If current device is not active or link is h8, it means it is after
	 * ufshcd_suspend() through
	 * a. runtime or system pm b. ufshcd_shutdown
	 * Both a. and b. will disable 26MHz ref clk(XO_UFS),
	 * so that deepidle/SODI do not need to disable 26MHz ref clk here.
	 */
	if (ufs_mtk_hba->curr_dev_pwr_mode != UFS_ACTIVE_PWR_MODE ||
		ufshcd_is_link_hibern8(ufs_mtk_hba)) {
		spm_resource_req(SPM_RESOURCE_USER_UFS, SPM_RESOURCE_RELEASE);
		return UFS_H8_SUSPEND;
	}

	/* Release all resources if entering H8 mode */
	ret = ufs_mtk_generic_read_dme(UIC_CMD_DME_GET,
		VENDOR_POWERSTATE, 0, &tmp, 100);

	if (ret) {
		/* ret == -1 means there is outstanding
		 * req/task/uic/pm ongoing, not an error
		 */
		if (ret != -1)
			dev_err(ufs_mtk_hba->dev,
				"ufshcd_dme_get 0x%x fail, ret = %d!\n",
				VENDOR_POWERSTATE, ret);
		return ret;
	}

	if (tmp == VENDOR_POWERSTATE_HIBERNATE) {
		/*
		 * Delay before disable XO_UFS: H8 -> delay A -> disable XO_UFS
		 *		delayA
		 * Hynix	30us
		 * Samsung	1us
		 * Toshiba	100us
		 */
		switch (ufs_mtk_hba->card->wmanufacturerid) {
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
			break;
		}
		/*
		 * Disable MPHY 26MHz ref clock in H8 mode
		 * SSPM project will disable MPHY 26MHz ref clock
		 * in SSPM deepidle/SODI IPI handler
		 */
	#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	#ifdef CLKBUF_READY
		clk_buf_ctrl(CLK_BUF_UFS, false);
		ufs_mtk_pltfrm_xo_ufs_req(ufs_mtk_hba, false);
	#endif
	#endif
		spm_resource_req(SPM_RESOURCE_USER_UFS, SPM_RESOURCE_RELEASE);
		return UFS_H8;
	}

	return -1;
#else
	return -1;
#endif
}


/**
 * ufs_mtk_deepidle_leave - callback function for leaving Deepidle & SODI.
 */
void ufs_mtk_pltfrm_deepidle_leave(void)
{
#ifdef CLKBUF_READY
	/* Enable MPHY 26MHz ref clock after leaving deepidle */
	/* SSPM project will enable MPHY 26MHz ref clock in SSPM
	 * deepidle/SODI IPI handler
	 */

#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	/* If current device is not active, it means it is after
	 * ufshcd_suspend() through a. runtime or system pm b. ufshcd_shutdown
	 * And deepidle/SODI can not enter in ufs suspend/resume
	 * callback by idle_lock_by_ufs()
	 * Therefore, it's guranteed that UFS is in H8 now
	 * and 26MHz ref clk is disabled by suspend callback
	 * deepidle/SODI do not need to enable 26MHz ref clk here
	 */
	if (ufs_mtk_hba->curr_dev_pwr_mode != UFS_ACTIVE_PWR_MODE)
		return;

	clk_buf_ctrl(CLK_BUF_UFS, true);
	ufs_mtk_pltfrm_xo_ufs_req(ufs_mtk_hba, true);
#endif
#endif
	/* Delay after enable XO_UFS: enable XO_UFS -> delay B -> leave H8
	 *		delayB
	 * Hynix	30us
	 * Samsung	max(1us,32us)
	 * Toshiba	32us
	 */
	udelay(32);
}

/**
 * ufs_mtk_pltfrm_deepidle_lock - Deepidle & SODI lock.
 * @hba: per-adapter instance
 * @lock: lock or unlock deepidle & SODI entrance.
 */
void ufs_mtk_pltfrm_deepidle_lock(struct ufs_hba *hba, bool lock)
{
#ifdef SPM_READY
	if (lock)
		idle_lock_by_ufs(1);
	else
		idle_lock_by_ufs(0);
#endif
}

int ufs_mtk_pltfrm_host_sw_rst(struct ufs_hba *hba, u32 target)
{
	u32 reg;

	if (!ufs_mtk_mmio_base_infracfg_ao) {
		dev_info(hba->dev,
			"ufs_mtk_host_sw_rst: failed, null ufs_mtk_mmio_base_infracfg_ao.\n");
		return 1;
	}

	dev_dbg(hba->dev, "ufs_mtk_host_sw_rst: 0x%x\n", target);

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

	if (target & SW_RST_TARGET_UFSHCI) {
		/* clear HCI reset */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UFSHCI_SW_RST_CLR);
		reg = reg | (1 << REG_UFSHCI_SW_RST_CLR_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UFSHCI_SW_RST_CLR);
	}

	if (target & SW_RST_TARGET_UFSCPT) {
		/* clear AES reset */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UFSCPT_SW_RST_CLR);
		reg = reg | (1 << REG_UFSCPT_SW_RST_CLR_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UFSCPT_SW_RST_CLR);
	}

	if (target & SW_RST_TARGET_UNIPRO) {
		/* clear UniPro reset */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UNIPRO_SW_RST_CLR);
		reg = reg | (1 << REG_UNIPRO_SW_RST_CLR_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UNIPRO_SW_RST_CLR);
	}

	if (target & SW_RST_TARGET_MPHY) {
		/* clear Mphy reset */
		reg = readl(ufs_mtk_mmio_base_infracfg_ao +
			REG_UFSPHY_SW_RST_CLR);
		reg = reg | (1 << REG_UFSPHY_SW_RST_CLR_BIT);
		writel(reg,
			ufs_mtk_mmio_base_infracfg_ao + REG_UFSPHY_SW_RST_CLR);
	}

	udelay(100);

	return 0;
}

int ufs_mtk_pltfrm_init(void)
{
	return 0;
}

int ufs_mtk_pltfrm_parse_dt(struct ufs_hba *hba)
{
	struct device_node *node_gpio;
	struct device_node *node_ufs_mphy;
	struct device_node *node_infracfg_ao;
	int err = 0;

	/* get ufs_mtk_gpio */
	node_gpio = of_find_compatible_node(NULL, NULL, "mediatek,gpio");

	if (node_gpio) {
		ufs_mtk_mmio_base_gpio = of_iomap(node_gpio, 0);

		if (IS_ERR(*(void **)&ufs_mtk_mmio_base_gpio)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_gpio);
			dev_err(hba->dev, "error: ufs_mtk_mmio_base_gpio init fail\n");
			ufs_mtk_mmio_base_gpio = NULL;
		}
	} else
		dev_err(hba->dev, "error: node_gpio init fail\n");

	/* get ufs_mtk_mmio_base_ufs_mphy */

	node_ufs_mphy =
		of_find_compatible_node(NULL, NULL, "mediatek,ufs_mphy");

	if (node_ufs_mphy) {
		ufs_mtk_mmio_base_ufs_mphy = of_iomap(node_ufs_mphy, 0);

		if (IS_ERR(*(void **)&ufs_mtk_mmio_base_ufs_mphy)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_ufs_mphy);
			dev_err(hba->dev, "error: ufs_mtk_mmio_base_ufs_mphy init fail\n");
			ufs_mtk_mmio_base_ufs_mphy = NULL;
		}
	} else
		dev_err(hba->dev, "error: ufs_mtk_mmio_base_ufs_mphy init fail\n");


	/* get ufs_mtk_mmio_base_infracfg_ao */
	node_infracfg_ao =
		of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	if (node_infracfg_ao) {
		ufs_mtk_mmio_base_infracfg_ao = of_iomap(node_infracfg_ao, 0);

		if (IS_ERR(*(void **)&ufs_mtk_mmio_base_infracfg_ao)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_infracfg_ao);
			dev_err(hba->dev, "error: ufs_mtk_mmio_base_infracfg_ao init fail\n");
			ufs_mtk_mmio_base_infracfg_ao = NULL;
		}
	} else
		dev_err(hba->dev, "error: ufs_mtk_mmio_base_infracfg_ao init fail\n");

	/* get va09 regulator and enable it */
	reg_va09 = regulator_get(hba->dev, "va09");
	if (!reg_va09) {
		dev_info(hba->dev, "%s: get va09 fail!!!\n", __func__);
		return -EINVAL;
	}
	err = regulator_enable(reg_va09);
	if (err < 0) {
		dev_info(hba->dev, "%s: enalbe va09 fail, err = %d\n",
			__func__, err);
		return err;
	}

	return err;
}

int ufs_mtk_pltfrm_resume(struct ufs_hba *hba)
{
	int ret = 0;
	u32 reg;

#ifdef CLKBUF_READY
	/* Enable MPHY 26MHz ref clock */
	clk_buf_ctrl(CLK_BUF_UFS, true);
	ufs_mtk_pltfrm_xo_ufs_req(ufs_mtk_hba, true);
#endif
	/* Set regulator to turn on VA09 LDO */
	ret = regulator_enable(reg_va09);
	if (ret < 0) {
		dev_info(hba->dev,
			"%s: enalbe va09 fail, err = %d\n", __func__, ret);
		return ret;
	}

	/* wait 200 us to stablize VA09 */
	udelay(200);

	/* Step 1: Set RG_VA09_ON to 1 */
	mt_secure_call(MTK_SIP_KERNEL_UFS_CTL, 1, 1, 0, 0);

	/* Step 2: release DA_MP_PLL_PWR_ON */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = reg | (0x1 << 11);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = reg & ~(0x1 << 10);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0x008C);

	/* Step 3: release DA_MP_PLL_ISO_EN */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = reg & ~(0x1 << 9);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = reg & ~(0x1 << 8);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0x008C);

	/* Step 4: release DA_MP_CDR_PWR_ON */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = reg | (0x1 << 18);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = reg & ~(0x1 << 17);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xB044);

	/* Step 5: release DA_MP_CDR_ISO_EN */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = reg & ~(0x1 << 20);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = reg & ~(0x1 << 19);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xB044);

	/* Step 6: release DA_MP_RX0_SQ_EN */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xA0AC);
	reg = reg | (0x1 << 1);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xA0AC);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xA0AC);
	reg = reg & ~(0x1);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xA0AC);

	/* Delay 1us to wait DIFZ stable */
	udelay(1);

	/* Step 7: release DIFZ */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xA09C);
	reg = reg & ~(0x1 << 18);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xA09C);

	return ret;
}

int ufs_mtk_pltfrm_suspend(struct ufs_hba *hba)
{
	int ret = 0;
	u32 reg;

	/* Step 1: force DIFZ */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xA09C);
	reg = reg | (0x1 << 18);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xA09C);

	/* Step 2: force DA_MP_RX0_SQ_EN */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xA0AC);
	reg = reg | (0x1);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xA0AC);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xA0AC);
	reg = reg & ~(0x1 << 1);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xA0AC);

	/* Step 3: force DA_MP_CDR_ISO_EN */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = reg | (0x1 << 19);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = reg | (0x1 << 20);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xB044);

	/* Step 4: force DA_MP_CDR_PWR_ON */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = reg | (0x1 << 17);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0xB044);
	reg = reg & ~(0x1 << 18);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0xB044);

	/* Step 5: force DA_MP_PLL_ISO_EN */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = reg | (0x1 << 8);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = reg | (0x1 << 9);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0x008C);

	/* Step 6: force DA_MP_PLL_PWR_ON */
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = reg | (0x1 << 10);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = readl(ufs_mtk_mmio_base_ufs_mphy + 0x008C);
	reg = reg & ~(0x1 << 11);
	writel(reg, ufs_mtk_mmio_base_ufs_mphy + 0x008C);

	/* Step 7: Set RG_VA09_ON to 0 */
	mt_secure_call(MTK_SIP_KERNEL_UFS_CTL, 1, 0, 0, 0);

	/* delay awhile to satisfy T_HIBERNATE */
	mdelay(15);

#ifdef CLKBUF_READY
	/* Disable MPHY 26MHz ref clock in H8 mode */
	clk_buf_ctrl(CLK_BUF_UFS, false);
	ufs_mtk_pltfrm_xo_ufs_req(ufs_mtk_hba, false);
#endif
	if (ufs_mtk_hba->curr_dev_pwr_mode != UFS_ACTIVE_PWR_MODE)
		spm_resource_req(SPM_RESOURCE_USER_UFS, SPM_RESOURCE_RELEASE);

	/* Set regulator to turn off VA09 LDO */
	ret = regulator_disable(reg_va09);
	if (ret < 0) {
		dev_info(hba->dev,
			"%s: disalbe va09 fail, err = %d\n",
			__func__, ret);
		return ret;
	}
#if 0
	/* TEST ONLY: emulate UFSHCI power off by HCI SW reset */
	ufs_mtk_pltfrm_host_sw_rst(hba, SW_RST_TARGET_UFSHCI);
#endif

	return ret;
}


