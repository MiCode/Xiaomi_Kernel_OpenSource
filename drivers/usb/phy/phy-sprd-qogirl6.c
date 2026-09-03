// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#include <linux/io.h>
#include <linux/iio/consumer.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/soc/sprd/sprd_usbpinmux.h>
#include <linux/timer.h>
#include <linux/usb/phy.h>
#include <linux/usb/sprd_commonphy.h>
#include <linux/power/sprd-bc1p2.h>
#include <linux/usb/otg.h>
#include <dt-bindings/soc/sprd,qogirl6-mask.h>
#include <dt-bindings/soc/sprd,qogirl6-regs.h>
#include <uapi/linux/usb/charger.h>
#include <dt-bindings/mfd/sprd,sc2730-mask.h>
#include <dt-bindings/mfd/sprd,sc2730-regs.h>

/*analog2 defined*/
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL			0x0008
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL2			0x000c
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW			0x001c
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1                    0x0004
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING                     0x0010
#define REG_ANLG_PHY_G2_ANALOG_USB20_REG_SEL_CFG_0			0x0020
#define BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_TUNEHSAMP			0x06000000
#define BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_TFREGRES			0x01f80000
#define BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_TUNEEQ			0x7

/*mask defined*/
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S                     0x10
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L                     0x8
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN                   0x1
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT                  0x10000
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8                 0x10000000
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_TUNEHSAMP                   0x6000000
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_TFREGRES                    0x1f80000
#define MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN          0x4
#define MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN          0x2
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DPPULLDOWN                  0x10
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DMPULLDOWN                  0x8
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_RESERVED                    0xffff

#define TUNEHSAMP_SHIFT			25
#define TUNEEQ_SHIFT			0
#define TFREGRES_SHIFT			19

#define DEFAULT_HOST_EYE_PATTERN					0x06a3d1c0
#define DEFAULT_DEVICE_EYE_PATTERN					0x0617d1c0

struct sprd_hsphy {
	struct device		*dev;
	struct usb_phy		phy;
	struct sprd_hsphy_ops		ops;
	struct notifier_block		typec_nb;
	struct regulator	*vdd;
	struct sprd_bc1p2_priv bc1p2_info;
	struct regmap           *hsphy_glb;
	struct regmap           *ana_g2;
	struct regmap           *pmic;
	struct wakeup_source	*wake_lock;
	struct work_struct		work;
	unsigned long event;
	u32			vdd_vol;
	u32			host_eye_pattern;
	u32			device_eye_pattern;
	atomic_t		reset;
	atomic_t		inited;
	atomic_t		high_impedance_stated;
	bool			is_host;
	bool			shutdown;
	struct regulator	*avdd1v8;
	struct regulator	*avdd1v2;
	struct regulator	*dcdcmodem;
	struct regulator	*dcdcgpu;
	struct iio_channel	*dp;
	struct iio_channel	*dm;
};

struct sprd_hsphy *g_phy;

#define FULLSPEED_USB33_TUNE		2700000
#define SC2730_CHARGE_DET_FGU_CTRL      0x3A0
#define SC2730_ADC_OFFSET               0x1800
#define BIT_DP_DM_AUX_EN                BIT(1)
#define BIT_DP_DM_BC_ENB                BIT(0)
#define VOLT_LO_LIMIT                   1200
#define VOLT_HI_LIMIT                   600

#define CHGR_DET_FGU_CTRL		0x1ba0
#define DP_DM_FS_ENB			BIT(14)
#define DP_DM_BC_ENB			BIT(0)
#define WAIT_TIME_1S	1000
#define REBOOT_WAIT_VBUS_TIME (20*WAIT_TIME_1S)

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

static void sprd_hsphy_charger_detect_work(struct work_struct *work)
{
	struct sprd_hsphy *phy = container_of(work, struct sprd_hsphy, work);

	if (phy->shutdown)
		return;

	__pm_stay_awake(phy->wake_lock);
	if (phy->event)
		sprd_usb_changed(&phy->bc1p2_info, USB_CHARGER_PRESENT);
	else
		sprd_usb_changed(&phy->bc1p2_info, USB_CHARGER_ABSENT);
	__pm_relax(phy->wake_lock);
}

static inline void sprd_hsphy_reset_core(struct sprd_hsphy *phy)
{
	u32 reg, msk;

	/* Reset PHY */
	reg = msk = MASK_AON_APB_OTG_PHY_SOFT_RST |
				MASK_AON_APB_OTG_UTMI_SOFT_RST;

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1,
		msk, reg);

	/* USB PHY reset need to delay 300us */
	udelay(300);

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1, msk, 0);

}

static int sprd_hostphy_set(struct usb_phy *x, int on)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;
	int ret = 0;

	if (on) {
		reg = phy->host_eye_pattern;
		regmap_write(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING, reg);

		msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, 0);

		msk = MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, msk);

		reg = 0x200;
		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, reg);
		phy->is_host = true;
	} else {
		reg = phy->device_eye_pattern;
		regmap_write(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING, reg);

		reg = msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, reg);

		msk = MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, 0);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, 0);
		phy->is_host = false;
	}

	return ret;
}

static int sprd_hsphy_set_pmic_pd_en(struct usb_phy *x, bool on)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 ret = 0;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s, phy is already shut down, no need to set pmic pd!\n",
				__func__);
		return -1;
	}

	if (!phy->pmic) {
		dev_dbg(x->dev, "pmic is null, cannot set pmic avdd18/avdd12/dcdcmm/dcdcgpu!\n");
		return -1;
	}

	dev_info(x->dev, "%s, bool on: %d\n", __func__, on);

	if (on) {
		/* Pmic + ufs : ldo set avdd18/avdd12/dcdcmodme/dcdcgpu pd_en off
		 * pmic + emmc: ldo set avdd18/avdd12/dcdcmodme/dcdcgpu pd_en on
		 *
		 * Pmic + emmc board system set avdd18/avdd12/dcdcmodme/dcdcgpu power down
		 * while system enter into suspend.
		 */

		/* Lowpower usbphy rely on avdd18 to remote wakeup.
		 * If avdd1v8 pd_en is on, set avdd1v8 pd_en off.
		 */
		if (phy->avdd1v8) {
			ret = regulator_enable(phy->avdd1v8);
			if (ret)
				pr_err("phy->avdd1v8 regulator_enable failed!\n");
		}

		/* Power on avdd18 cause leakage of electricity,which is ums9620 defect.
		 * To solve the soc problem, power on avdd12/dcdcmodme/dcdcgpu as workaround.
		 */
		/* Set avdd12/dcdcmm/dcdcgpu power on while suspend */
		if (phy->avdd1v2) {
			ret = regulator_enable(phy->avdd1v2);
			if (ret)
				pr_err("phy->avdd1v2 regulator_enable failed!\n");
		}
		if (phy->dcdcmodem) {
			ret = regulator_enable(phy->dcdcmodem);
			if (ret)
				pr_err("phy->dcdcmodem regulator_enable failed!\n");
		}
		if (phy->dcdcgpu) {
			ret = regulator_enable(phy->dcdcgpu);
			if (ret)
				pr_err("phy->dcdcgpu regulator_enable failed!\n");
		}
	} else {
		/*clear avdd18/avdd12/dcdcmodem/dcdcgpu pd_en */
		if (phy->avdd1v8)
			regulator_disable(phy->avdd1v8);

		if (phy->avdd1v2)
			regulator_disable(phy->avdd1v2);

		if (phy->dcdcmodem)
			regulator_disable(phy->dcdcmodem);

		if (phy->dcdcgpu)
			regulator_disable(phy->dcdcgpu);
	}

	return 0;
}

int sprd_hsphy_set_high_impedance_state(void) {
        u32 msk, val;
        int ret = 0;

        if (atomic_read(&g_phy->high_impedance_stated)) {
                dev_info(g_phy->dev, "%s is already high impedance stated\n", __func__);
                return 0;
        }

        /* before detect charge type, make sure usb phy is power on */
        /* Turn On VDD */
        regulator_set_voltage(g_phy->vdd, g_phy->vdd_vol, g_phy->vdd_vol);
        ret = regulator_enable(g_phy->vdd);
        if (ret)
                return ret;

        regmap_read(g_phy->ana_g2, REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, &val);
        dev_info(g_phy->dev, "%s enter, utmi_ctl1=0x%X, line:%d\n", __func__, val, __LINE__);
        regmap_read(g_phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, &val);
        dev_info(g_phy->dev, "%s enter, otg_phy_test=0x%X, line:%d\n", __func__, val, __LINE__);

        ret |= regmap_update_bits(g_phy->ana_g2, REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW,
                        MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN, 0);

	msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
                 MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);
        ret |= regmap_update_bits(g_phy->ana_g2,
                         REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, 0);

        /* usb vbus invalid */
        msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
        regmap_update_bits(g_phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);

	/* set dp&dm highZ*/
        msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
        ret |= regmap_update_bits(g_phy->ana_g2,
                        REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);
	
	atomic_set(&g_phy->high_impedance_stated, 1);
        
	return ret;
}
EXPORT_SYMBOL_GPL(sprd_hsphy_set_high_impedance_state);

int sprd_hsphy_cancel_high_impedance_state(void) {
        u32 val, reg, msk;
        int ret = 0;
	
	regmap_read(g_phy->ana_g2, REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, &val);
        dev_info(g_phy->dev, "%s enter, utmi_ctl1=0x%X, line:%d\n", __func__, val, __LINE__);
        regmap_read(g_phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, &val);
        dev_info(g_phy->dev, "%s enter, otg_phy_test=0x%X, line:%d\n", __func__, val, __LINE__);

	if (!atomic_read(&g_phy->high_impedance_stated)) {
                dev_info(g_phy->dev, "%s already cancel, line:%d\n", __func__, __LINE__);
                return 0;
        }

        regulator_disable(g_phy->vdd);
        
	atomic_set(&g_phy->high_impedance_stated, 0);
        /* usb vbus valid */
        reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
        regmap_update_bits(g_phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, reg);
        
	/* remove dp&dm highZ*/
        reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
        ret |= regmap_update_bits(g_phy->ana_g2,
                        REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);
        
	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8;
        regmap_update_bits(g_phy->ana_g2,
                REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
                msk, reg);
        
	return ret;
}
EXPORT_SYMBOL_GPL(sprd_hsphy_cancel_high_impedance_state);

static int sprd_hsphy_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;
	int ret;

	if (atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already inited!\n", __func__);
		return 0;
	}
	
	if (!(x->flags & SPRD_USB_PHY_IGNORE_RESET)) {
		dev_dbg(x->dev, "%s turn on VDD\n", __func__);
		regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
		ret = regulator_enable(phy->vdd);
	if (ret)
		return ret;
	}

	/* usb enable */
	reg = msk = MASK_AON_APB_OTG_UTMI_EB;
	regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_APB_EB1, msk, reg);

	reg = msk = MASK_AON_APB_CGM_OTG_REF_EN |
		MASK_AON_APB_CGM_DPHY_REF_EN;
	regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_CGM_REG1, msk, reg);

	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW,
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN, 0);

	/* usb phy power */
	msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, 0);

	/* usb vbus valid */
	reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_OTG_PHY_TEST, msk, reg);

	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,	msk, reg);

	/* for SPRD phy utmi_width sel */
	reg = msk = MASK_AON_APB_UTMI_WIDTH_SEL;
	regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_OTG_PHY_CTRL, msk, reg);

	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
		msk, reg);

	if (!atomic_read(&phy->reset)) {
		sprd_hsphy_reset_core(phy);
		atomic_set(&phy->reset, 1);
	}

	atomic_set(&phy->inited, 1);

	return 0;
}

static void sprd_hsphy_shutdown(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already shut down\n", __func__);
		return;
	}

	/* usb vbus */
	msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);
	msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);

	/* usb power down */
	reg = msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, reg);
	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW,
		msk, reg);

	/* usb cgm ref */
	msk = MASK_AON_APB_CGM_OTG_REF_EN |
		MASK_AON_APB_CGM_DPHY_REF_EN;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, 0);
	
	if (!(x->flags & SPRD_USB_PHY_IGNORE_RESET)) {
		regulator_disable(phy->vdd);
	}

	atomic_set(&phy->inited, 0);
	atomic_set(&phy->reset, 0);
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
	&dev_attr_vdd_voltage.attr,
	&dev_attr_hsphy_device_eye_pattern.attr,
	&dev_attr_hsphy_host_eye_pattern.attr,
	NULL
};
ATTRIBUTE_GROUPS(usb_hsphy);

static int sprd_hsphy_vbus_notify(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, vbus_nb);
	struct sprd_hsphy *phy = container_of(usb_phy, struct sprd_hsphy, phy);
	//u32 reg, msk;
	u32 msk;

	if (phy->shutdown)
		return 0;

	if (usb_phy->last_event == USB_EVENT_ID) {
		dev_info(phy->dev, "USB PHY is host mode\n");
		return 0;
	}

	pm_wakeup_event(phy->dev, 400);

	if (event) {
		/* usb vbus valid */
		/*reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, reg);

		reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);

		reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8;
		regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, reg);*/
		dev_info(phy->dev, "don't need to usb vbus valid\n");

	} else {
		/* usb vbus invalid */
		msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);
	}

	phy->event = event;
	queue_work(system_unbound_wq, &phy->work);

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
	phy->device_eye_pattern &= ~BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_TUNEHSAMP;
	phy->device_eye_pattern |= (u32)val[0] << TUNEHSAMP_SHIFT;

	/* host setting */
	phy->host_eye_pattern &= ~BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_TUNEHSAMP;
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
	phy->device_eye_pattern &= ~BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_TUNEEQ;
	phy->device_eye_pattern |= (u32)val[0] << TUNEEQ_SHIFT;

	/* host setting */
	phy->host_eye_pattern &= ~BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_TUNEEQ;
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
	phy->device_eye_pattern &= ~BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_TFREGRES;
	phy->device_eye_pattern |= (u32)val[0] << TFREGRES_SHIFT;

	/* host setting */
	phy->host_eye_pattern &= ~BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_TFREGRES;
	phy->host_eye_pattern |= (u32)val[1] << TFREGRES_SHIFT;

	return 0;
}

static int sprd_eye_pattern_prepared(struct sprd_hsphy *phy, struct device *dev)
{
	int ret = 0;

	/* set default eyepatt */
	phy->device_eye_pattern = DEFAULT_DEVICE_EYE_PATTERN;
	dev_info(dev, "%s: default device eye_pattern: 0x%x\n", __func__,
				phy->device_eye_pattern);

	phy->host_eye_pattern = DEFAULT_HOST_EYE_PATTERN;
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

static enum usb_charger_type sprd_hsphy_retry_charger_detect(struct usb_phy *x);

static enum usb_charger_type sprd_hsphy_charger_detect(struct usb_phy *x)
{
	if (x->flags & CHARGER_2NDDETECT_SELECT)
		return sprd_hsphy_retry_charger_detect(x);

	return sprd_bc1p2_charger_detect(x);
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

static int sc2730_voltage_cali(int voltage)
{
	return voltage*3/2;
}

static enum usb_charger_type sprd_hsphy_retry_charger_detect(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	enum usb_charger_type type = UNKNOWN_TYPE;
	int dm_voltage, dp_voltage;
	int cnt = 0;
	u64 curr;
	static bool reboot;

	if (!phy->dm || !phy->dp || phy->shutdown) {
		dev_err(x->dev, " phy->dp:%p, phy->dm:%p, shutdown:%d\n",
			phy->dp, phy->dm, phy->shutdown);
		return UNKNOWN_TYPE;
	}

	regmap_update_bits(phy->pmic,
		SC2730_ADC_OFFSET | SC2730_CHARGE_DET_FGU_CTRL,
		BIT_DP_DM_AUX_EN | BIT_DP_DM_BC_ENB,
		BIT_DP_DM_AUX_EN);

	if (!reboot) {
		reboot = 1;
		curr = ktime_to_ms(ktime_get());
		dev_info(x->dev, "%s time %llu\n", __func__, curr);
		if (curr < REBOOT_WAIT_VBUS_TIME) {
			for (cnt = 0; cnt < 15; cnt++) {
				iio_read_channel_processed(phy->dp, &dp_voltage);
				dp_voltage = sc2730_voltage_cali(dp_voltage);

				if (dp_voltage > VOLT_LO_LIMIT) {
					dev_info(x->dev, "[%s][%d] dp_voltage:%d\n",
						__func__, cnt, dp_voltage);
					break;
				}
				msleep(20);
			}
		} else {
			msleep(300);
		}
	} else {
		msleep(300);
	}

	cnt = 20;
	iio_read_channel_processed(phy->dp, &dp_voltage);
	dp_voltage = sc2730_voltage_cali(dp_voltage);
	if (dp_voltage > VOLT_LO_LIMIT) {
		do {
			if (phy->shutdown)
				return UNKNOWN_TYPE;

			iio_read_channel_processed(phy->dm, &dm_voltage);
			dm_voltage = sc2730_voltage_cali(dm_voltage);
			if (dm_voltage > VOLT_LO_LIMIT) {
				type = DCP_TYPE;
				break;
			}
			msleep(100);
			cnt--;
			iio_read_channel_processed(phy->dp, &dp_voltage);
			dp_voltage = sc2730_voltage_cali(dp_voltage);
			if (dp_voltage  < VOLT_HI_LIMIT) {
				type = SDP_TYPE;
				break;
			}
		} while ((x->chg_state == USB_CHARGER_PRESENT) && cnt > 0);
	}

	regmap_update_bits(phy->pmic,
		SC2730_ADC_OFFSET | SC2730_CHARGE_DET_FGU_CTRL,
		BIT_DP_DM_AUX_EN | BIT_DP_DM_BC_ENB, 0);

	dev_info(x->dev, "correct type is %x\n", type);
	if (type != UNKNOWN_TYPE) {
		x->chg_type = type;
		if (x->chg_type == SDP_TYPE)
			usb_phy_set_charger_current(x, DEFAULT_CUR_MIN);
		schedule_work(&x->chg_work);
	}
	return type;
}

int sprd_hsphy_cali_mode(void)
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

static void sprd_hsphy_lowpower_pd_init(struct platform_device *pdev, struct sprd_hsphy *phy)
{
	phy->avdd1v8 = devm_regulator_get(&pdev->dev, "usb-avdd1v8-slp");
	if (IS_ERR(phy->avdd1v8)) {
		dev_err(&pdev->dev, "can't get avdd1v8\n");
		phy->avdd1v8 = NULL;
	}

	phy->avdd1v2 = devm_regulator_get(&pdev->dev, "usb-avdd1v2-slp");
	if (IS_ERR(phy->avdd1v2)) {
		dev_err(&pdev->dev, "can't get avdd1v2\n");
		phy->avdd1v2 = NULL;
	}

	phy->dcdcmodem = devm_regulator_get(&pdev->dev, "usb-dcdcmodem-slp");
	if (IS_ERR(phy->dcdcmodem)) {
		dev_err(&pdev->dev, "can't get dcdcmodem\n");
		phy->dcdcmodem = NULL;
	}

	phy->dcdcgpu = devm_regulator_get(&pdev->dev, "usb-dcdcgpu-slp");
	if (IS_ERR(phy->dcdcgpu)) {
		dev_err(&pdev->dev, "can't get dcdcgpu\n");
		phy->dcdcgpu = NULL;
	}
}

static int sprd_hsphy_probe(struct platform_device *pdev)
{
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct sprd_hsphy *phy;
	struct device *dev = &pdev->dev;
	int ret = 0, calimode = 0;
	u32 reg, msk;
	struct usb_otg *otg;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		dev_err(dev, "unable to get syscon platform device\n");
		ret = -ENODEV;
		goto device_node_err;
	}

	phy->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!phy->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		ret = -ENODEV;
		goto platform_device_err;
	}

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				   &phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "unable to read ssphy vdd voltage\n");
		goto platform_device_err;
	}

	calimode = sprd_hsphy_cali_mode();
	if (calimode) {
		phy->vdd_vol = FULLSPEED_USB33_TUNE;
		dev_info(dev, "calimode vdd_vol:%d\n", phy->vdd_vol);
	}

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get ssphy vdd supply\n");
		ret = PTR_ERR(phy->vdd);
		goto platform_device_err;
	}

	/* get lowpower electricity */
	sprd_hsphy_lowpower_pd_init(pdev, phy);

	ret = regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "fail to set ssphy vdd voltage at %dmV\n",
			phy->vdd_vol);
		goto platform_device_err;
	}

	otg = devm_kzalloc(&pdev->dev, sizeof(*otg), GFP_KERNEL);
	if (!otg) {
		ret = -ENOMEM;
		goto platform_device_err;
	}

	phy->ana_g2 = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-anag2");
	if (IS_ERR(phy->ana_g2)) {
		dev_err(&pdev->dev, "ap USB anag2 syscon failed!\n");
		ret = PTR_ERR(phy->ana_g2);
		goto platform_device_err;
	}

	phy->hsphy_glb = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-enable");
	if (IS_ERR(phy->hsphy_glb)) {
		dev_err(&pdev->dev, "ap USB aon apb syscon failed!\n");
		ret = PTR_ERR(phy->hsphy_glb);
		goto platform_device_err;
	}

	ret = sprd_eye_pattern_prepared(phy, dev);
	if (ret < 0)
		dev_warn(dev, "sprd_eye_pattern_prepared failed, ret = %d\n", ret);

	phy->dp = devm_iio_channel_get(dev, "dp");
	phy->dm = devm_iio_channel_get(dev, "dm");
	if (IS_ERR(phy->dp)) {
		phy->dp = NULL;
		dev_warn(dev, "failed to get dp or dm channel\n");
	}
	if (IS_ERR(phy->dm)) {
		phy->dm = NULL;
		dev_warn(dev, "failed to get dp or dm channel\n");
	}

	/* enable usb module */
	reg = msk = (MASK_AON_APB_OTG_UTMI_EB | MASK_AON_APB_ANA_EB);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB1, msk, reg);

	reg = msk = MASK_AON_APB_CGM_OTG_REF_EN | MASK_AON_APB_CGM_DPHY_REF_EN;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, reg);

	/* usb power down */
	if (sprd_usbmux_check_mode() != MUX_MODE) {
		reg = msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
			MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);
		regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, reg);
	}
	phy->dev = dev;
	phy->phy.dev = dev;
	phy->phy.label = "sprd-hsphy";
	phy->phy.otg = otg;
	phy->phy.init = sprd_hsphy_init;
	phy->phy.shutdown = sprd_hsphy_shutdown;
	phy->phy.set_vbus = sprd_hostphy_set;
	phy->phy.type = USB_PHY_TYPE_USB2;
	phy->phy.vbus_nb.notifier_call = sprd_hsphy_vbus_notify;
	phy->phy.charger_detect = sprd_hsphy_charger_detect;
	phy->phy.set_wakeup = sprd_hsphy_set_pmic_pd_en;
	otg->usb_phy = &phy->phy;
	phy->ops.dpdm_switch_to_phy = sprd_hsphy_dpdm_switch_to_phy;
	phy->ops.get_dpdm_from_phy = sprd_hsphy_get_dpdm_from_phy;

	device_init_wakeup(phy->dev, true);

	phy->wake_lock = wakeup_source_register(phy->dev, "sprd-hsphy");
	if (!phy->wake_lock) {
		dev_err(dev, "fail to register wakeup lock.\n");
		goto platform_device_err;
	}

	INIT_WORK(&phy->work, sprd_hsphy_charger_detect_work);
	phy->typec_nb.notifier_call = sprd_hsphy_typec_notifier;
	ret = register_sprd_usbphy_notifier(&phy->typec_nb, SPRD_USBPHY_EVENT_TYPEC);
	if (ret) {
		dev_err(dev, "fail to register_sprd_usbphy_notifier\n");
		goto  platform_device_err;
	}

	platform_set_drvdata(pdev, phy);
	ret = usb_add_bc1p2_init(&phy->bc1p2_info, &phy->phy);
	if (ret) {
		dev_err(dev, "fail to add bc1p2\n");
		return ret;
	}

	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		goto  platform_device_err;
	}
	sc27xx_dpdm_switch_to_phy(phy->pmic, false);

	ret = sysfs_create_groups(&dev->kobj, usb_hsphy_groups);
	if (ret)
		dev_warn(dev, "failed to create usb hsphy attributes\n");

	phy->shutdown = false;
	if (extcon_get_state(phy->phy.edev, EXTCON_USB) > 0)
		sprd_usb_changed(&phy->bc1p2_info, USB_CHARGER_PRESENT);

	g_phy = phy;

	dev_dbg(dev, "sprd usb phy probe ok !\n");

platform_device_err:
device_node_err:
	of_node_put(regmap_np);

	return ret;
}

static int sprd_hsphy_remove(struct platform_device *pdev)
{
	struct sprd_hsphy *phy = platform_get_drvdata(pdev);

	usb_remove_bc1p2(&phy->bc1p2_info);
	sysfs_remove_groups(&pdev->dev.kobj, usb_hsphy_groups);
	usb_remove_phy(&phy->phy);

	regulator_disable(phy->vdd);

	return 0;
}

static void sprd_hsphy_drshutdown(struct platform_device *pdev)
{
	struct sprd_hsphy *phy = platform_get_drvdata(pdev);

	phy->shutdown = true;
	cancel_work_sync(&phy->work);
	usb_shutdown_bc1p2(&phy->bc1p2_info);
	sc27xx_dpdm_switch_to_phy(phy->pmic, false);
}

static const struct of_device_id sprd_hsphy_match[] = {
	{ .compatible = "sprd,qogirl6-phy" },
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
