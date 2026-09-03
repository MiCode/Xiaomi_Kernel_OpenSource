// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/iio/consumer.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/usb/phy.h>
#include <linux/power/sprd-bc1p2.h>
#include <linux/usb/otg.h>
#include <uapi/linux/usb/charger.h>
#include <dt-bindings/soc/sprd,sharkle-mask.h>
#include <dt-bindings/soc/sprd,sharkle-regs.h>

#define BYPASS_FSLS_DISCONNECTED	BIT(9)

struct sprd_hsphy {
	struct device		*dev;
	struct usb_phy		phy;
	void __iomem		*base;
	struct regulator	*vdd;
	struct sprd_bc1p2_priv bc1p2_info;
	struct regmap		*hsphy_glb;
	struct regmap		*pmic;
	struct wakeup_source	*wake_lock;
	struct work_struct	work;
	unsigned long		event;
	u32			host_otg_ctrl0;
	u32			device_otg_ctrl0;
	u32			host_otg_ctrl1;
	u32			device_otg_ctrl1;
	u32			vdd_vol;
	u32			phy_tune;
	atomic_t		reset;
	atomic_t		inited;
	bool			is_host;
	bool			shutdown;
	struct iio_channel	*dp;
	struct iio_channel	*dm;
};

#define FULLSPEED_USB33_TUNE		2700000
#define SC2721_CHARGE_DET_FGU_CTRL	0xecc
#define BIT_DP_DM_AUX_EN			BIT(1)
#define BIT_DP_DM_BC_ENB			BIT(0)
#define VOLT_LO_LIMIT				1200
#define VOLT_HI_LIMIT				600

#define TUNEHSAMP_SHIFT			30
#define TUNEEQ_SHIFT			0
#define TFREGRES_SHIFT			8

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

static inline void __reset_core(void __iomem *addr)
{
	u32 reg;

	/* Reset PHY */
	reg = readl_relaxed(addr + REG_AP_AHB_AHB_RST);
	reg = MASK_AP_AHB_OTG_PHY_SOFT_RST | MASK_AP_AHB_OTG_UTMI_SOFT_RST |
			MASK_AP_AHB_OTG_SOFT_RST;
	/* set  offset : 0x1000 */
	writel_relaxed(reg, addr + REG_AP_AHB_AHB_RST + 0x1000);
	/* USB PHY reset need to delay 20ms~30ms */
	usleep_range(20000, 30000);
	/* clear offset: 0x2000 */
	writel_relaxed(reg, addr + REG_AP_AHB_AHB_RST + 0x2000);
}

static int sprd_hostphy_set(struct usb_phy *x, int on)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg1;

	if (on) {
		reg1 = phy->host_otg_ctrl0;
		writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_CTRL0);

		reg1 = phy->host_otg_ctrl1;
		writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_CTRL1);

		reg1 = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_CTRL);
		reg1 &= ~MASK_AP_AHB_USB2_PHY_IDDIG;
		reg1 |= MASK_AP_AHB_OTG_DPPULLDOWN | MASK_AP_AHB_OTG_DMPULLDOWN;
		writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_PHY_CTRL);

		reg1 = readl_relaxed(phy->base + REG_AP_AHB_OTG_CTRL0);
		reg1 |= BYPASS_FSLS_DISCONNECTED;
		writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_CTRL0);
		phy->is_host = true;
	} else {
		reg1 = phy->device_otg_ctrl0;
		writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_CTRL0);

		reg1 = phy->device_otg_ctrl1;
		writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_CTRL1);

		reg1 = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_CTRL);
		reg1 |= MASK_AP_AHB_USB2_PHY_IDDIG;
		reg1 &= ~(MASK_AP_AHB_OTG_DPPULLDOWN |
			MASK_AP_AHB_OTG_DMPULLDOWN);
		writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_PHY_CTRL);

		reg1 = readl_relaxed(phy->base + REG_AP_AHB_OTG_CTRL0);
		reg1 &= ~BYPASS_FSLS_DISCONNECTED;
		writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_CTRL0);
		phy->is_host = false;
	}
	return 0;
}

static int sprd_hsphy_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, reg1;
	int ret;

	if (atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already inited!\n", __func__);
		return 0;
	}

	/* Turn On VDD */
	regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);

	ret = regulator_enable(phy->vdd);
	if (ret)
		return ret;

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_L, 0);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_S, 0);
	/* Restore PHY tunes */
	writel_relaxed(phy->phy_tune, phy->base + REG_AP_AHB_OTG_PHY_TUNE);

	/* USB PHY write register need to delay 2ms~3ms */
	usleep_range(2000, 3000);

	/* usb vbus valid */
	reg = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_TEST);
	reg |= (MASK_AP_AHB_OTG_VBUS_VALID_EXT |
		 MASK_AP_AHB_OTG_VBUS_VALID_PHYREG);
	writel_relaxed(reg, phy->base + REG_AP_AHB_OTG_PHY_TEST);

	/* for SPRD phy utmi_width sel */
	reg1 = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_CTRL);
	reg1 &= ~(MASK_AP_AHB_UTMI_WIDTH_SEL|MASK_AP_AHB_USB2_DATABUS16_8);
	writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_PHY_CTRL);

	/* for SPRD phy sampler sel */
	reg1 = readl_relaxed(phy->base + REG_AP_AHB_OTG_CTRL1);
	reg1 |= MASK_AP_AHB_USB20_SAMPLER_SEL;
	writel_relaxed(reg1, phy->base + REG_AP_AHB_OTG_CTRL1);

	if (!atomic_read(&phy->reset)) {
		/* USB PHY write register need to delay 2ms~3ms */
		usleep_range(2000, 3000);
		__reset_core(phy->base);
		atomic_set(&phy->reset, 1);
	}

	atomic_set(&phy->inited, 1);

	return 0;
}

static void sprd_hsphy_shutdown(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already shut down\n", __func__);
		return;
	}

	reg = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_TEST);
	reg &= ~(MASK_AP_AHB_OTG_VBUS_VALID_PHYREG |
		MASK_AP_AHB_OTG_VBUS_VALID_EXT);
	writel_relaxed(reg, phy->base + REG_AP_AHB_OTG_PHY_TEST);

	/* Backup PHY Tune value */
	phy->phy_tune = readl(phy->base + REG_AP_AHB_OTG_PHY_TUNE);

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_L, MASK_AON_APB_USB_PHY_PD_L);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_S, MASK_AON_APB_USB_PHY_PD_S);

	regulator_disable(phy->vdd);

	atomic_set(&phy->inited, 0);
	atomic_set(&phy->reset, 0);
}

static ssize_t phy_tune_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_hsphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_hsphy, phy);

	return sprintf(buf, "0x%x\n", phy->phy_tune);
}

static ssize_t phy_tune_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_hsphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_hsphy, phy);
	if (kstrtouint(buf, 16, &phy->phy_tune) < 0)
		return -EINVAL;

	return size;
}
static DEVICE_ATTR_RW(phy_tune);

static ssize_t vdd_voltage_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_hsphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_hsphy, phy);

	return sprintf(buf, "%d\n", phy->vdd_vol);
}

static ssize_t vdd_voltage_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_hsphy *phy;
	unsigned int vol;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_hsphy, phy);
	if (kstrtouint(buf, 16, &vol) < 0)
		return -EINVAL;

	if (vol < 1200000 || vol > 3750000) {
		dev_err(dev, "Invalid voltage value %d\n", vol);
		return -EINVAL;
	}
	phy->vdd_vol = vol;

	return size;
}
static DEVICE_ATTR_RW(vdd_voltage);

static struct attribute *usb_hsphy_attrs[] = {
	&dev_attr_phy_tune.attr,
	&dev_attr_vdd_voltage.attr,
	NULL
};
ATTRIBUTE_GROUPS(usb_hsphy);

static const struct of_device_id sprd_hsphy_match[] = {
	{ .compatible = "sprd,sharkle-phy" },
	{},
};

static int sprd_hsphy_vbus_notify(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, vbus_nb);
	struct sprd_hsphy *phy = container_of(usb_phy, struct sprd_hsphy, phy);
	u32 reg;

	if (phy->shutdown)
		return 0;

	if (usb_phy->last_event == USB_EVENT_ID) {
		dev_info(phy->dev, "USB PHY is host mode\n");
		return 0;
	}

	pm_wakeup_event(phy->dev, 400);

	if (event) {
		/* usb vbus valid */
		reg = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_TEST);
		reg |= (MASK_AP_AHB_OTG_VBUS_VALID_EXT |
			 MASK_AP_AHB_OTG_VBUS_VALID_PHYREG);
		writel_relaxed(reg, phy->base + REG_AP_AHB_OTG_PHY_TEST);
	} else {
		/* usb vbus invalid */
		reg = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_TEST);
		reg &= ~(MASK_AP_AHB_OTG_VBUS_VALID_PHYREG |
			MASK_AP_AHB_OTG_VBUS_VALID_EXT);
		writel_relaxed(reg, phy->base + REG_AP_AHB_OTG_PHY_TEST);
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
	phy->device_otg_ctrl0 &= ~MASK_AP_AHB_USB20_TUNEHSAMP;
	phy->device_otg_ctrl0 |= (u32)val[0] << TUNEHSAMP_SHIFT;

	/* host setting */
	phy->host_otg_ctrl0 &= ~MASK_AP_AHB_USB20_TUNEHSAMP;
	phy->host_otg_ctrl0 |= (u32)val[1] << TUNEHSAMP_SHIFT;

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
	phy->device_otg_ctrl1 &= ~MASK_AP_AHB_USB20_TUNEEQ;
	phy->device_otg_ctrl1 |= (u32)val[0] << TUNEEQ_SHIFT;

	/* host setting */
	phy->host_otg_ctrl1 &= ~MASK_AP_AHB_USB20_TUNEEQ;
	phy->host_otg_ctrl1 |= (u32)val[1] << TUNEEQ_SHIFT;

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
	phy->device_otg_ctrl1 &= ~MASK_AP_AHB_USB20_TFREGRES;
	phy->device_otg_ctrl1 |= (u32)val[0] << TFREGRES_SHIFT;

	/* host setting */
	phy->host_otg_ctrl1 &= ~MASK_AP_AHB_USB20_TFREGRES;
	phy->host_otg_ctrl1 |= (u32)val[1] << TFREGRES_SHIFT;

	return 0;
}

static int sprd_eye_pattern_prepared(struct sprd_hsphy *phy, struct device *dev)
{
	int ret = 0;

	/* set default OTG_CTRL */
	phy->device_otg_ctrl0 = readl_relaxed(phy->base + REG_AP_AHB_OTG_CTRL0);
	phy->device_otg_ctrl1 = readl_relaxed(phy->base + REG_AP_AHB_OTG_CTRL1);
	dev_info(dev, "%s: default device REG_AP_AHB_OTG_CTRL0: 0x%x\n",
		__func__, phy->device_otg_ctrl0);
	dev_info(dev, "%s: default device REG_AP_AHB_OTG_CTRL1: 0x%x\n",
		__func__, phy->device_otg_ctrl1);

	phy->host_otg_ctrl0 = readl_relaxed(phy->base + REG_AP_AHB_OTG_CTRL0);
	phy->host_otg_ctrl1 = readl_relaxed(phy->base + REG_AP_AHB_OTG_CTRL1);
	dev_info(dev, "%s: default host REG_AP_AHB_OTG_CTRL0: 0x%x\n", __func__,
		phy->host_otg_ctrl0);
	dev_info(dev, "%s: default host REG_AP_AHB_OTG_CTRL1: 0x%x\n", __func__,
		phy->host_otg_ctrl1);

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

static int sc2721_voltage_cali(int voltage)
{
	return voltage*3/2;
}

static enum usb_charger_type sprd_hsphy_retry_charger_detect(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	enum usb_charger_type type = UNKNOWN_TYPE;
	int dm_voltage, dp_voltage;
	int cnt = 20;

	if (!phy->dm || !phy->dp || phy->shutdown) {
		dev_err(x->dev, " phy->dp:%p, phy->dm:%p, shutdown:%d\n",
			phy->dp, phy->dm, phy->shutdown);
		return UNKNOWN_TYPE;
	}

	regmap_update_bits(phy->pmic,
		SC2721_CHARGE_DET_FGU_CTRL,
		BIT_DP_DM_AUX_EN | BIT_DP_DM_BC_ENB,
		BIT_DP_DM_AUX_EN);

	msleep(300);
	iio_read_channel_processed(phy->dp, &dp_voltage);
	dp_voltage = sc2721_voltage_cali(dp_voltage);
	if (dp_voltage > VOLT_LO_LIMIT) {
		do {
			if (phy->shutdown)
				return UNKNOWN_TYPE;

			iio_read_channel_processed(phy->dm, &dm_voltage);
			dm_voltage = sc2721_voltage_cali(dm_voltage);
			if (dm_voltage > VOLT_LO_LIMIT) {
				type = DCP_TYPE;
				break;
			}
			msleep(100);
			cnt--;
			iio_read_channel_processed(phy->dp, &dp_voltage);
			dp_voltage = sc2721_voltage_cali(dp_voltage);
			if (dp_voltage  < VOLT_HI_LIMIT) {
				type = SDP_TYPE;
				break;
			}
		} while ((x->chg_state == USB_CHARGER_PRESENT) && cnt > 0);
	}

	regmap_update_bits(phy->pmic,
		SC2721_CHARGE_DET_FGU_CTRL,
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

static int sprd_hsphy_probe(struct platform_device *pdev)
{
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct sprd_hsphy *phy;
	struct device *dev = &pdev->dev;
	struct regmap *hsphy_glb;
	struct resource *res;
	int ret = 0, calimode = 0;
	struct usb_otg *otg;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "phy_glb_regs");
	if (!res) {
		dev_err(dev, "missing memory resource\n");
		return -ENODEV;
	}

	phy->base = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(phy->base))
		return PTR_ERR(phy->base);

	ret = of_property_read_u32(dev->of_node, "sprd,tune-value",
				   &phy->phy_tune);
	if (ret < 0) {
		dev_err(dev, "unable to read hsphy usb phy tune\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_warn(dev, "unable to get syscon node\n");
	} else {
		regmap_pdev = of_find_device_by_node(regmap_np);
		if (!regmap_pdev) {
			of_node_put(regmap_np);
			dev_warn(dev, "unable to get syscon platform device\n");
			phy->pmic = NULL;
		} else {
			phy->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
			if (!phy->pmic)
				dev_warn(dev, "unable to get pmic regmap device\n");
		}
	}

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				   &phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "unable to read ssphy vdd voltage\n");
		return ret;
	}

	calimode = sprd_hsphy_cali_mode();
	if (calimode) {
		phy->vdd_vol = FULLSPEED_USB33_TUNE;
		dev_info(dev, "calimode vdd_vol:%d\n", phy->vdd_vol);
	}

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get ssphy vdd supply\n");
		return PTR_ERR(phy->vdd);
	}

	ret = regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "fail to set ssphy vdd voltage at %dmV\n",
			phy->vdd_vol);
		return ret;
	}

	otg = devm_kzalloc(&pdev->dev, sizeof(*otg), GFP_KERNEL);
	if (!otg)
		return -ENOMEM;

	hsphy_glb = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-enable");
	if (IS_ERR(hsphy_glb)) {
		dev_err(&pdev->dev, "ap USB PHY syscon failed!\n");
		return PTR_ERR(hsphy_glb);
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

	regmap_update_bits(hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_L, MASK_AON_APB_USB_PHY_PD_L);
	regmap_update_bits(hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_S, MASK_AON_APB_USB_PHY_PD_S);
	phy->hsphy_glb = hsphy_glb;

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
	otg->usb_phy = &phy->phy;

	device_init_wakeup(phy->dev, true);

	phy->wake_lock = wakeup_source_register(phy->dev, "sprd-hsphy");
	if (!phy->wake_lock) {
		dev_err(dev, "fail to register wakeup lock.\n");
		return -ENOMEM;
	}

	INIT_WORK(&phy->work, sprd_hsphy_charger_detect_work);

	platform_set_drvdata(pdev, phy);
	ret = usb_add_bc1p2_init(&phy->bc1p2_info, &phy->phy);
	if (ret) {
		dev_err(dev, "fail to add bc1p2\n");
		return ret;
	}

	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		return ret;
	}

	ret = sysfs_create_groups(&dev->kobj, usb_hsphy_groups);
	if (ret)
		dev_warn(dev, "failed to create usb hsphy attributes\n");

	phy->shutdown = false;

	if (extcon_get_state(phy->phy.edev, EXTCON_USB) > 0)
		sprd_usb_changed(&phy->bc1p2_info, USB_CHARGER_PRESENT);

	return 0;
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
}

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
