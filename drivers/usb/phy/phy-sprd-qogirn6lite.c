/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/soc/sprd/sprd_usbpinmux.h>
#include <linux/usb/otg.h>
#include <linux/usb/phy.h>
#include <linux/usb/sprd_commonphy.h>
#include <linux/power/sprd-bc1p2.h>
#include <dt-bindings/soc/sprd,qogirn6lite-mask.h>
#include <dt-bindings/soc/sprd,qogirn6lite-regs.h>
#include <linux/usb/sprd_usbm.h>
#include <dt-bindings/mfd/sprd,ump9620-mask.h>
#include <dt-bindings/mfd/sprd,ump9620-regs.h>

struct phy_reg_info {
	struct regmap		*regmap_ptr;
	u32			args[3];
};

struct sprd_hsphy {
	struct device		*dev;
	struct usb_phy		phy;
	struct sprd_hsphy_ops		ops;
	struct notifier_block		typec_nb;
	struct regulator	*vdd;
	struct sprd_bc1p2_priv bc1p2_info;
	struct regmap           *hsphy_glb;
	struct regmap           *ana_g0;
	struct regmap           *pmic;
	struct phy_reg_info	refclk_cfg;
	u32			host_eye_pattern;
	u32			device_eye_pattern;
	u32			vdd_vol;
	atomic_t		reset;
	atomic_t		inited;
	bool			is_host;
	bool			avdd1v8_pd_en_changed;
	bool			avdd1v2_pd_en_changed;
	bool			dcdcmm_pd_en_changed;
	bool			shutdown;
};

#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_VBUSVALID       0x02000000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TESTCLK         0x01000000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TESTDATAIN      0xff0000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TESTADDR        0xf000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TESTDATAOUTSEL  0x800
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TESTDATAOUT     0x380
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BIST_MODE       0x7c
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_T2RCOMP         0x2
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_LPBK_END        0x1
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_DATABUS16_8     0x10000000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_SUSPENDM        0x08000000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_PORN            0x04000000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_RESET           0x02000000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_RXERROR         0x01000000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BYPASS_DRV_DP   0x00800000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BYPASS_DRV_DM   0x00400000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BYPASS_FS       0x00200000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BYPASS_IN_DP    0x00100000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BYPASS_IN_DM    0x80000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BYPASS_OUT_DP   0x40000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BYPASS_OUT_DM   0x20000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_VBUSVLDEXT      0x10000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_RESERVED        0x0000FFFF
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_REXTENABLE      0x4
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_DMPULLUP        0x2
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_SAMPLER_SEL     0x1
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_DPPULLDOWN      0x10
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_DMPULLDOWN      0x8
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TXBITSTUFFENABLE 0x4
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TXBITSTUFFENABLEH 0x2
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_SLEEPM          0x1
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNEHSAMP       0x06000000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TFREGRES        0x01f80000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TFHSRES         0x7c000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNERISE        0x3000
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNEOTG         0xe00
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNEDSC         0x180
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNESQ          0x78
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNEEQ          0x7
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNEPLLS        0x7800
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_PLL_PFD_DEADZONE 0x300
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_PLL_PFD_DELAY   0xc0
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_PLL_CP_IOFFSET_EN 0x20
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_PLL_CP_IOFFSET  0x1e
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_PLL_REF_DOUBLER_EN 0x1
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BISTRAM_EN      0x2
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_BIST_MODE_EN    0x1
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_ISO_SW_EN       0x1
#define BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_SUSPENDM 0x100
#define BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_PORN    0x80
#define BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_RESET   0x40
#define BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_BYPASS_FS 0x20
#define BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_BYPASS_IN_DM 0x10
#define BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN 0x8
#define BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN 0x4
#define BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_SLEEPM 0x2
#define BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_ISO_SW_EN 0x1


#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_TEST_PIN        0x0000
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1       0x0004
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_BATTER_PLL      0x0008
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL2       0x000C
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_TRIMMING        0x0010
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_PLL_CTRL        0x0014
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_PHY_BIST_TEST   0x0018
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_PHY             0x001C
#define REG_ANLG_PHY_G0L_ANALOG_USB20_REG_SEL_CFG_0         0x0020

#define FULLSPEED_USB33_TUNE			3300000
#define REG_ANA_UMP9621_SLP_DCDC_PD_CTRL	0xa0ac
#define MASK_ANA_UMP9621_SLP_DCDCMODEM_PD_EN	0x4

#define CHGR_DET_FGU_CTRL		0x23a0
#define DP_DM_FS_ENB			BIT(14)
#define DP_DM_BC_ENB			BIT(0)

#define TUNEHSAMP_SHIFT			25
#define TUNEEQ_SHIFT			0
#define TFREGRES_SHIFT			19

#define SPRD_USB_PHY_IGNORE_RESET	BIT(29)

static void sc27xx_dpdm_switch_to_phy(struct regmap *regmap, bool enable)
{
	int ret;
	u32 val;

	pr_info("switch dp/dm to %s\n", enable ? "usb" : "other");
	ret = regmap_read(regmap, CHGR_DET_FGU_CTRL, &val);
	if (ret) {
		pr_err("%s, dp/dm switch reg read failed:%d\n",
				__func__, ret);
		return;
	}

	/*
	 * bit14: 1 switch to USB phy, 0 switch to fast charger
	 * bit0 : 1 switch to USB phy, 0 switch to BC1P2
	 */
	if (enable)
		val = val | DP_DM_FS_ENB | DP_DM_BC_ENB;
	else
		val = val & ~(DP_DM_FS_ENB | DP_DM_BC_ENB);

	ret = regmap_write(regmap, CHGR_DET_FGU_CTRL, val);
	if (ret)
		pr_err("%s, dp/dm switch reg write failed:%d\n",
				__func__, ret);
}

static bool sc27xx_get_dpdm_from_phy(struct regmap *regmap)
{
	int ret;
	u32 reg;
	bool val;

	ret = regmap_read(regmap, CHGR_DET_FGU_CTRL, &reg);
	if (ret) {
		pr_err("%s, dp/dm switch reg read failed:%d\n",
				__func__, ret);
		return false;
	}

	/*
	 * bit14: 1 switch to USB phy, 0 switch to fast charger
	 * bit0 : 1 switch to USB phy, 0 switch to BC1P2
	 */
	if ((reg & DP_DM_FS_ENB) && (reg & DP_DM_BC_ENB))
		val = true;
	else
		val = false;
	pr_info("get dpdm form %s\n", val ? "usb" : "other");

	return val;
}

static int sprd_hsphy_typec_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct sprd_hsphy *phy  = container_of(nb, struct sprd_hsphy, typec_nb);

	if (phy->shutdown)
		return 0;

	pr_info("__func__:%s, event %s\n", __func__, event ? "true" : "false");
	if (event)
		sc27xx_dpdm_switch_to_phy(phy->pmic, true);
	else
		sc27xx_dpdm_switch_to_phy(phy->pmic, false);

	return 0;
}

static int boot_cali;
static int sprd_hsphy_cali_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmdline;
	int ret;
	bool mode;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);

	if (ret) {
		pr_err("Can't not parse bootargs\n");
		return 0;
	}

	mode = (strstr(cmdline, "androidboot.mode=cali") != NULL) ||
	       (strstr(cmdline, "androidboot.mode=autotest") != NULL) ||
	       (strstr(cmdline, "sprdboot.mode=cali") != NULL) ||
	       (strstr(cmdline, "sprdboot.mode=autotest") != NULL);

	if (mode)
		return 1;
	else
		return 0;
}

static int sprd_hsphy_set_wakeup(struct usb_phy *x, bool keep_wakeup)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg = 0;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "phy is already shut down, no need set wakeup\n");
		return -1;
	}

	if (!phy->pmic) {
		dev_dbg(x->dev, "pmic is null, cannot set wakeup\n");
		return -1;
	}

	dev_info(x->dev, "%s, keep_wakeup %d\n", __func__, keep_wakeup);

	if (keep_wakeup) {
		/* AVDD18 & AVDD12 & DCDCMM */
		regmap_read(phy->pmic, REG_ANA_SLP_LDO_PD_CTRL1, &reg);
		if (reg & MASK_ANA_SLP_LDO_AVDD18_PD_EN) {
			phy->avdd1v8_pd_en_changed = true;
			regmap_update_bits(phy->pmic,
					REG_ANA_SLP_LDO_PD_CTRL1, MASK_ANA_SLP_LDO_AVDD18_PD_EN, 0);
		}
		if (reg & MASK_ANA_SLP_LDO_AVDD12_PD_EN) {
			phy->avdd1v2_pd_en_changed = true;
			regmap_update_bits(phy->pmic,
					REG_ANA_SLP_LDO_PD_CTRL1, MASK_ANA_SLP_LDO_AVDD12_PD_EN, 0);
		}

		regmap_read(phy->pmic, REG_ANA_UMP9621_SLP_DCDC_PD_CTRL, &reg);
		if (reg & MASK_ANA_UMP9621_SLP_DCDCMODEM_PD_EN) {
			phy->dcdcmm_pd_en_changed = true;
			regmap_update_bits(phy->pmic,
				REG_ANA_UMP9621_SLP_DCDC_PD_CTRL,
				MASK_ANA_UMP9621_SLP_DCDCMODEM_PD_EN, 0);
		}
	} else {
		if (phy->avdd1v8_pd_en_changed) {
			regmap_update_bits(phy->pmic,
				REG_ANA_SLP_LDO_PD_CTRL1,
				MASK_ANA_SLP_LDO_AVDD18_PD_EN, MASK_ANA_SLP_LDO_AVDD18_PD_EN);
			phy->avdd1v8_pd_en_changed = 0;
		}
		if (phy->avdd1v2_pd_en_changed) {
			regmap_update_bits(phy->pmic,
				REG_ANA_SLP_LDO_PD_CTRL1,
				MASK_ANA_SLP_LDO_AVDD12_PD_EN, MASK_ANA_SLP_LDO_AVDD12_PD_EN);
			phy->avdd1v2_pd_en_changed = 0;
		}
		if (phy->dcdcmm_pd_en_changed) {
			regmap_update_bits(phy->pmic,
				REG_ANA_UMP9621_SLP_DCDC_PD_CTRL,
				MASK_ANA_UMP9621_SLP_DCDCMODEM_PD_EN,
				MASK_ANA_UMP9621_SLP_DCDCMODEM_PD_EN);
			phy->dcdcmm_pd_en_changed = 0;
		}
	}

	return 0;
}

static inline void sprd_hsphy_reset_core(struct sprd_hsphy *phy)
{
	u32 reg, msk;

	/* Reset PHY */
	msk = MASK_AON_APB_OTG_PHY_SOFT_RST |
				MASK_AON_APB_OTG_UTMI_SOFT_RST;
	reg = msk;

	if (!sprd_usbm_ssphy_get_onoff())
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1,
			msk, reg);

	/* USB PHY reset need to delay 300us */
	udelay(300);

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1,
		msk, 0);
}

static int sprd_hostphy_set(struct usb_phy *x, int on)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;
	int ret = 0;

	if (on) {
		reg = phy->host_eye_pattern;
		regmap_write(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_TRIMMING, reg);

		msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, 0);

		msk = BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_DMPULLDOWN |
			BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, msk);

		reg = 0x200;
		msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, reg);

		phy->is_host = true;
	} else {
		reg = phy->device_eye_pattern;
		regmap_write(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_TRIMMING, reg);

		reg = msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, reg);

		msk = BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			BIT_ANLG_PHY_G0L_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_DMPULLDOWN |
			BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, 0);

		msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, 0);
		phy->is_host = false;
	}
	return ret;
}
/*
static void sprd_hsphy_emphasis_set(struct usb_phy *x, bool enabled)
{

}
*/
static int sprd_hsphy_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;
	int ret = 0;

	if (atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already inited!\n", __func__);
		return 0;
	}

	/* Turn On VDD */
	if (!(x->flags & SPRD_USB_PHY_IGNORE_RESET)) {
		regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
		ret = regulator_enable(phy->vdd);
		if (ret)
			return ret;
	}

	sprd_usbm_hsphy_set_onoff(1);

	if (sprd_usbm_event_is_active()) {
		/* select the AON-SYS USB controller */
		msk = MASK_AON_APB_USB20_CTRL_MUX_REG;
		ret |= regmap_update_bits(phy->hsphy_glb, REG_AON_APB_AON_SOC_USB_CTRL,
			msk, 0);
	}

	/* usb enable */
	reg = msk = MASK_AON_APB_AON_USB2_TOP_EB |
		MASK_AON_APB_OTG_PHY_EB;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_APB_EB1, msk, reg);

	reg = msk = MASK_AON_APB_CGM_OTG_REF_EN |
		MASK_AON_APB_CGM_DPHY_REF_EN;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_CGM_REG1, msk, reg);

	/* usb phy power */
	msk = (MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_L |
		MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_S);
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_MIPI_CSI_POWER_CTRL, msk, 0);

	ret |= regmap_update_bits(phy->ana_g0,
		REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_PHY,
		BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_ISO_SW_EN, 0);

	/* REG_AON_APB_MIPI_CSI_POWER_CTRL(offset:0x350) reg PD_L/PD_S bit has two ways to control.
	 * 1. ANLOG_USB20_REG_SEL_CFG_0(offset:0x20) bit0 set 1,
	 *    then set/clr analog USB20_ISO_SW_EN and PD_L/PD_S
	 *
	 * 2. REG_AON_APB_AON_SOC_USB_CTRL(offset:0x190) reg USB20_ISO_SW_EN bit set 0,
	 *    then sel/clr USB20_ISO_SW_EN and PD_L/PD_S.
	 *
	 * 3. default control is 2.
	 */
	/* enable usb20 ISO AVDD1V8_USB */
	msk = MASK_AON_APB_USB20_ISO_SW_EN;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_AON_SOC_USB_CTRL, msk, 0);

	/* usb vbus valid */
	reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_OTG_PHY_TEST, msk, reg);

	reg = msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_VBUSVLDEXT;
	ret |= regmap_update_bits(phy->ana_g0,
		REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1,	msk, reg);

	/* for SPRD phy utmi_width sel */
	reg = msk = MASK_AON_APB_UTMI_WIDTH_SEL;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_OTG_PHY_CTRL, msk, reg);

	reg = msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_DATABUS16_8;
	ret |= regmap_update_bits(phy->ana_g0,
		REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1,
		msk, reg);

	if (phy->refclk_cfg.regmap_ptr) {
		reg = msk = phy->refclk_cfg.args[1] | phy->refclk_cfg.args[2];
		ret |= regmap_update_bits(phy->refclk_cfg.regmap_ptr,
				phy->refclk_cfg.args[0],
				msk, reg);
	}

	if (!atomic_read(&phy->reset)) {
		sprd_hsphy_reset_core(phy);
		atomic_set(&phy->reset, 1);
	}

	atomic_set(&phy->inited, 1);
	return ret;
}

static void sprd_hsphy_shutdown(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already shut down\n", __func__);
		return;
	}

	dev_info(x->dev, "[%s]enter usbm_event_is_active(%d), usbm_ssphy_get_onoff(%d)\n",
		__func__, sprd_usbm_event_is_active(), sprd_usbm_ssphy_get_onoff());

	sprd_usbm_hsphy_set_onoff(0);
	if (!sprd_usbm_ssphy_get_onoff()) {
		/* usb vbus */
		msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);
		msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);

		if (sprd_usbmux_check_mode() != MUX_MODE) {
			/* disable aon apb usb20 ISO_SW_EN */
			reg = msk = MASK_AON_APB_USB20_ISO_SW_EN;
			regmap_update_bits(phy->hsphy_glb, REG_AON_APB_AON_SOC_USB_CTRL, msk, reg);

			reg = msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_ISO_SW_EN;
			regmap_update_bits(phy->ana_g0,
				REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_PHY,
				msk, reg);

			/* usb power down */
			reg = msk = (MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_L |
				MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_S);
			regmap_update_bits(phy->hsphy_glb,
				REG_AON_APB_MIPI_CSI_POWER_CTRL, msk, reg);
		}

		/* usb cgm ref */
		msk = MASK_AON_APB_CGM_OTG_REF_EN |
			MASK_AON_APB_CGM_DPHY_REF_EN;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, 0);

		/*disable analog:0x64900004*/
		msk = MASK_AON_APB_AON_USB2_TOP_EB | MASK_AON_APB_OTG_PHY_EB;;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB1, msk, 0);

		if (phy->refclk_cfg.regmap_ptr) {
			reg = msk = phy->refclk_cfg.args[1] | phy->refclk_cfg.args[2];
			regmap_update_bits(phy->refclk_cfg.regmap_ptr,
					phy->refclk_cfg.args[0],
					msk, ~reg);
		}
	}

	if (!(x->flags & SPRD_USB_PHY_IGNORE_RESET)) {
		if (regulator_is_enabled(phy->vdd))
			regulator_disable(phy->vdd);
	}

	atomic_set(&phy->inited, 0);
	atomic_set(&phy->reset, 0);
}

static int __maybe_unused sprd_hsphy_post_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	if (regulator_is_enabled(phy->vdd))
		regulator_disable(phy->vdd);

	return 0;
}

static ssize_t vdd_voltage_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;

	return sprintf(buf, "%d\n", x->vdd_vol);
}

static ssize_t vdd_voltage_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);
	u32 vol;

	if (!x)
		return -EINVAL;

	if (kstrtouint(buf, 16, &vol) < 0)
		return -EINVAL;

	if (vol < 1200000 || vol > 3750000) {
		dev_err(dev, "Invalid voltage value %d\n", vol);
		return -EINVAL;
	}
	x->vdd_vol = vol;

	return size;
}
static DEVICE_ATTR_RW(vdd_voltage);

static ssize_t hsphy_device_eye_pattern_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;


	return sprintf(buf, "0x%x\n", x->device_eye_pattern);
}

static ssize_t hsphy_device_eye_pattern_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;

	if (kstrtouint(buf, 16, &x->device_eye_pattern) < 0)
		return -EINVAL;

	return size;
}
static DEVICE_ATTR_RW(hsphy_device_eye_pattern);

static ssize_t hsphy_host_eye_pattern_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;


	return sprintf(buf, "0x%x\n", x->host_eye_pattern);
}

static ssize_t hsphy_host_eye_pattern_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;

	if (kstrtouint(buf, 16, &x->host_eye_pattern) < 0)
		return -EINVAL;

	return size;
}

static DEVICE_ATTR_RW(hsphy_host_eye_pattern);

static struct attribute *usb_hsphy_attrs[] = {
	&dev_attr_hsphy_device_eye_pattern.attr,
	&dev_attr_hsphy_host_eye_pattern.attr,
	&dev_attr_vdd_voltage.attr,
	NULL
};
ATTRIBUTE_GROUPS(usb_hsphy);

static enum usb_charger_type sprd_hsphy_charger_detect(struct usb_phy *x)
{
	enum usb_charger_type type = UNKNOWN_TYPE;

	type = sprd_bc1p2_charger_detect(x);

	return type;
}

static void sprd_hsphy_dpdm_switch_to_phy(struct usb_phy *x, bool enable)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	sc27xx_dpdm_switch_to_phy(phy->pmic, enable);
}

static bool sprd_hsphy_get_dpdm_from_phy(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	return sc27xx_get_dpdm_from_phy(phy->pmic);
}

static int sprd_hsphy_vbus_notify(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, vbus_nb);
	struct sprd_hsphy *phy = container_of(usb_phy, struct sprd_hsphy, phy);
	u32 reg, msk;

	if (phy->shutdown)
		return 0;

	if (usb_phy->last_event == USB_EVENT_ID) {
		dev_info(phy->dev, "USB PHY is host mode\n");
		return 0;
	}

	if (event) {
		/* usb vbus valid */
		reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
		regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_TEST, msk, reg);

		reg = msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);
		sprd_usb_changed(&phy->bc1p2_info, USB_CHARGER_PRESENT);
	} else {
		/* usb vbus invalid */
		msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST,
			msk, 0);
		msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g0,
			REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);
		sprd_usb_changed(&phy->bc1p2_info, USB_CHARGER_ABSENT);
	}

	return 0;
}

static int sprd_eyepatt_tunehsamp_set(struct sprd_hsphy *phy, struct device *dev)
{
	int ret = 0;
	u8 val[2];

	ret = of_property_read_u8_array(dev->of_node, "sprd,hsphy-tunehsamp", val, 2);

	if (ret < 0) {
		dev_err(dev, "unable to get hsphy-device-tunehsamp\n");
		return ret;
	}

	/* device setting */
	phy->device_eye_pattern &= ~BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNEHSAMP;
	phy->device_eye_pattern |= (u32)val[0] << TUNEHSAMP_SHIFT;

	/* host setting */
	phy->host_eye_pattern &= ~BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNEHSAMP;
	phy->host_eye_pattern |= (u32)val[1] << TUNEHSAMP_SHIFT;

	return 0;
}

static int sprd_eyepatt_tuneeq_set(struct sprd_hsphy *phy, struct device *dev)
{
	int ret = 0;
	u8 val[2];

	ret = of_property_read_u8_array(dev->of_node, "sprd,hsphy-tuneeq", val, 2);

	if (ret < 0) {
		dev_err(dev, "unable to get hsphy-tuneeq\n");
		return ret;
	}

	/* device setting */
	phy->device_eye_pattern &= ~BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNEEQ;
	phy->device_eye_pattern |= (u32)val[0] << TUNEEQ_SHIFT;

	/* host setting */
	phy->host_eye_pattern &= ~BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TUNEEQ;
	phy->host_eye_pattern |= (u32)val[1] << TUNEEQ_SHIFT;

	return 0;
}

static int sprd_eyepatt_tfregres_set(struct sprd_hsphy *phy, struct device *dev)
{
	int ret = 0;
	u8 val[2];

	ret = of_property_read_u8_array(dev->of_node, "sprd,hsphy-tfregres", val, 2);

	if (ret < 0) {
		dev_err(dev, "unable to get hsphy-tfregres\n");
		return ret;
	}

	/* device setting */
	phy->device_eye_pattern &= ~BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TFREGRES;
	phy->device_eye_pattern |= (u32)val[0] << TFREGRES_SHIFT;

	/* host setting */
	phy->host_eye_pattern &= ~BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_TFREGRES;
	phy->host_eye_pattern |= (u32)val[1] << TFREGRES_SHIFT;

	return 0;
}

static int sprd_eye_pattern_prepared(struct sprd_hsphy *phy, struct device *dev)
{
	int ret = 0;

	/* set default eyepatt */
	regmap_read(phy->ana_g0, REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_TRIMMING,
				&phy->device_eye_pattern);
	dev_info(dev, "%s: default device eye_pattern: 0x%x\n", __func__,
				phy->device_eye_pattern);

	regmap_read(phy->ana_g0, REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_TRIMMING,
				&phy->host_eye_pattern);
	dev_info(dev, "%s: default host eye_pattern: 0x%x\n", __func__,
				phy->host_eye_pattern);

	/* set eyepatt tunehsamp */
	ret |= sprd_eyepatt_tunehsamp_set(phy, dev);

	/* set eyepatt tuneeq */
	ret |= sprd_eyepatt_tuneeq_set(phy, dev);

	/* set eyepatt tfregres */
	ret |= sprd_eyepatt_tfregres_set(phy, dev);

	return ret;
}

static int sprd_hsphy_probe(struct platform_device *pdev)
{
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct sprd_hsphy *phy;
	struct device *dev = &pdev->dev;
	int ret;
	struct usb_otg *otg;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon platform device\n");
		return -ENODEV;
	}

	phy->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!phy->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				   &phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "unable to read hsphy vdd voltage\n");
		return ret;
	}

	boot_cali = sprd_hsphy_cali_mode();

	if (boot_cali) {
		phy->vdd_vol = FULLSPEED_USB33_TUNE;
		dev_info(dev, "calimode vdd_vol:%d\n", phy->vdd_vol);
	}

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get hsphy vdd supply\n");
		return PTR_ERR(phy->vdd);
	}

	ret = regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "fail to set hsphy vdd voltage at %dmV\n",
			phy->vdd_vol);
		return ret;
	}

	otg = devm_kzalloc(&pdev->dev, sizeof(*otg), GFP_KERNEL);
	if (!otg)
		return -ENOMEM;

	phy->ana_g0 = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-anag0");
	if (IS_ERR(phy->ana_g0)) {
		dev_err(&pdev->dev, "ap USB anag0 syscon failed!\n");
		return PTR_ERR(phy->ana_g0);
	}

	phy->hsphy_glb = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-enable");
	if (IS_ERR(phy->hsphy_glb)) {
		dev_err(&pdev->dev, "ap USB aon apb syscon failed!\n");
		return PTR_ERR(phy->hsphy_glb);
	}

	phy->refclk_cfg.regmap_ptr = syscon_regmap_lookup_by_phandle_args(dev->of_node,
			"refclk_cfg", 3, phy->refclk_cfg.args);
	if (IS_ERR(phy->refclk_cfg.regmap_ptr)) {
		dev_warn(&pdev->dev, "failed to get refclk_cfg regmap!\n");
		phy->refclk_cfg.regmap_ptr = NULL;
	}

	ret = sprd_eye_pattern_prepared(phy, dev);
	if (ret < 0)
		dev_warn(dev, "sprd_eye_pattern_prepared failed, ret = %d\n", ret);

	/* musb_set_utmi_60m_flag() is used to control the must i2s clk
	 * configuration, pass parameter "true" if we use 60MHz utmi phy
	 */
	musb_set_utmi_60m_flag(false);

	phy->phy.dev = dev;
	phy->phy.label = "sprd-hsphy";
	phy->phy.otg = otg;
	phy->phy.init = sprd_hsphy_init;
	phy->phy.shutdown = sprd_hsphy_shutdown;
	phy->phy.set_vbus = sprd_hostphy_set;
	phy->phy.type = USB_PHY_TYPE_USB2;
	phy->phy.vbus_nb.notifier_call = sprd_hsphy_vbus_notify;
	phy->phy.charger_detect = sprd_hsphy_charger_detect;
	phy->phy.set_wakeup = sprd_hsphy_set_wakeup;
	otg->usb_phy = &phy->phy;
	phy->ops.dpdm_switch_to_phy = sprd_hsphy_dpdm_switch_to_phy;
	phy->ops.get_dpdm_from_phy = sprd_hsphy_get_dpdm_from_phy;
	platform_set_drvdata(pdev, phy);
	ret = usb_add_bc1p2_init(&phy->bc1p2_info, &phy->phy);
	if (ret) {
		dev_err(dev, "fail to add bc1p2\n");
		return ret;
	}

	phy->typec_nb.notifier_call = sprd_hsphy_typec_notifier;
	ret = register_sprd_usbphy_notifier(&phy->typec_nb, SPRD_USBPHY_EVENT_TYPEC);
	if (ret) {
		dev_err(dev, "fail to register_sprd_usbphy_notifier\n");
		return ret;
	}

	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		return ret;
	}

	sc27xx_dpdm_switch_to_phy(phy->pmic, false);
	ret = sysfs_create_groups(&dev->kobj, usb_hsphy_groups);
	if (ret)
		dev_warn(dev, "failed to create usb hsphy attributes\n");

	phy->shutdown = false;

	if (extcon_get_state(phy->phy.edev, EXTCON_USB) > 0)
		sprd_usb_changed(&phy->bc1p2_info, USB_CHARGER_PRESENT);

	dev_dbg(dev, "sprd usb phy probe ok !\n");

	return 0;
}

static int sprd_hsphy_remove(struct platform_device *pdev)
{
	struct sprd_hsphy *phy = platform_get_drvdata(pdev);

	usb_remove_bc1p2(&phy->bc1p2_info);

	sysfs_remove_groups(&pdev->dev.kobj, usb_hsphy_groups);
	usb_remove_phy(&phy->phy);
	if (regulator_is_enabled(phy->vdd))
		regulator_disable(phy->vdd);

	return 0;
}

static void sprd_hsphy_drshutdown(struct platform_device *pdev)
{
	struct sprd_hsphy *phy = platform_get_drvdata(pdev);

	phy->shutdown = true;
	usb_shutdown_bc1p2(&phy->bc1p2_info);
	sc27xx_dpdm_switch_to_phy(phy->pmic, false);
}

static const struct of_device_id sprd_hsphy_match[] = {
	{ .compatible = "sprd,qogirn6lite-phy" },
	{},
};

MODULE_DEVICE_TABLE(of, sprd_hsphy_match);

static struct platform_driver sprd_hsphy_driver = {
	.probe = sprd_hsphy_probe,
	.remove = sprd_hsphy_remove,
	.shutdown = sprd_hsphy_drshutdown,
	.driver = {
		.name = "sprd-hsphy",
		.of_match_table = sprd_hsphy_match,
	},
};

static int __init sprd_hsphy_driver_init(void)
{
	return platform_driver_register(&sprd_hsphy_driver);
}

static void __exit sprd_hsphy_driver_exit(void)
{
	platform_driver_unregister(&sprd_hsphy_driver);
}

late_initcall(sprd_hsphy_driver_init);
module_exit(sprd_hsphy_driver_exit);

MODULE_DESCRIPTION("UNISOC USB PHY driver");
MODULE_LICENSE("GPL v2");
