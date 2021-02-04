/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>

#include "mtu3.h"
#include "mtu3_dr.h"
#include "mtu3_hal.h"
#ifdef CONFIG_MTK_BOOT
#include <mt-plat/mtk_boot_common.h>
#endif

u32 mtu3_debug_level = K_EMERG | K_ALET | K_CRIT;

module_param(mtu3_debug_level, int, 0644);
MODULE_PARM_DESC(mtu3_debug_level, "Debug Print Log Lvl");

/*
 * USB Speed Mode
 * 0: High Speed
 * 1: Super Speed
 */
u32 mtu3_speed;
static int set_musb_speed(const char *val, const struct kernel_param *kp)
{
	int ret;
	u32 u3_en;

	ret = kstrtou32(val, 10, &u3_en);
	if (ret)
		return ret;

	if (u3_en != 0 && u3_en != 1)
		return -EINVAL;

	mtu3_speed = u3_en;

	return 0;
}
static struct kernel_param_ops musb_speed_param_ops = {
	.set = set_musb_speed,
	.get = param_get_int,
};
module_param_cb(speed, &musb_speed_param_ops, &mtu3_speed, 0644);
MODULE_PARM_DESC(debug, "USB speed configuration. default = 1, spuper speed.");


#ifdef CONFIG_SYSFS
const char *const mtu3_mode_str[CABLE_MODE_MAX] = { "CHRG_ONLY",
	"NORMAL", "HOST_ONLY", "FORCE_ON" };
unsigned int mtu3_cable_mode = CABLE_MODE_NORMAL;

#if !defined(CONFIG_USB_MU3D_DRV)
const struct attribute_group mtu3_attr_group;

ssize_t musb_cmode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!dev) {
		pr_info("dev is null!!\n");
		return 0;
	}
	return sprintf(buf, "%d\n", mtu3_cable_mode);
}

ssize_t musb_cmode_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int cmode;
	struct ssusb_mtk *ssusb;
	struct extcon_dev *edev;

	if (!dev) {
		pr_info("dev is null!!\n");
		return count;
	}

	ssusb = dev_to_ssusb(dev);

	if (!ssusb) {
		pr_info("ssusb is null!!\n");
		return count;
	}

	edev = (&ssusb->otg_switch)->edev;

	if (sscanf(buf, "%ud", &cmode) == 1) {
		if (cmode >= CABLE_MODE_MAX)
			cmode = CABLE_MODE_NORMAL;

		if (mtu3_cable_mode != cmode) {
			pr_info("%s %s --> %s\n", __func__,
				mtu3_mode_str[mtu3_cable_mode],
				mtu3_mode_str[cmode]);
			mtu3_cable_mode = cmode;

			/* IPO shutdown, disable USB */
			if (cmode == CABLE_MODE_CHRG_ONLY) {
				ssusb_set_mailbox(&ssusb->otg_switch,
					MTU3_VBUS_OFF);
			} else if (cmode == CABLE_MODE_HOST_ONLY) {
				ssusb_set_mailbox(&ssusb->otg_switch,
					MTU3_VBUS_OFF);
			} else if (cmode == CABLE_MODE_FORCEON) {
				ssusb_set_mailbox(&ssusb->otg_switch,
					MTU3_VBUS_VALID);
			} else {	/* IPO bootup, enable USB */
				if (extcon_get_state(edev, EXTCON_USB_HOST))
					ssusb_set_mailbox(&ssusb->otg_switch,
						   MTU3_ID_GROUND);
				else
					ssusb_set_mailbox(&ssusb->otg_switch,
						   MTU3_CMODE_VBUS_VALID);
			}
			msleep(200);
		}
	}
	return count;
}

static bool saving_mode;
ssize_t musb_saving_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!dev) {
		pr_info("dev is null!!\n");
		return 0;
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", saving_mode);
}

ssize_t musb_saving_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int saving;
	long tmp_val;

	if (!dev) {
		pr_info("dev is null!!\n");
		return count;
	/* } else if (1 == sscanf(buf, "%d", &saving)) { */
	} else if (kstrtol(buf, 10, (long *)&tmp_val) == 0) {
		saving = tmp_val;
		pr_info("old=%d new=%d\n", saving, saving_mode);
		if (saving_mode == (!saving))
			saving_mode = !saving_mode;
	}
	return count;
}

bool is_saving_mode(void)
{
	pr_info("saving_mode : %d\n", saving_mode);
	return saving_mode;
}


DEVICE_ATTR(cmode, 0664, musb_cmode_show, musb_cmode_store);
DEVICE_ATTR(saving, 0664, musb_saving_mode_show, musb_saving_mode_store);

#ifdef CONFIG_MTK_UART_USB_SWITCH
#include <linux/phy/mediatek/mtk_usb_phy.h>

ssize_t musb_portmode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssusb_mtk *ssusb;
	bool portmode;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return 0;
	}

	ssusb  = dev_to_ssusb(dev);
	portmode = usb_mtkphy_check_in_uart_mode(ssusb->phys[0]);

	if (portmode == PORT_MODE_USB)
		pr_debug("\nUSB Port mode -> USB\n");
	else if (portmode == PORT_MODE_UART)
		pr_debug("\nUSB Port mode -> UART\n");

	usb_mtkphy_dump_usb2uart_reg(ssusb->phys[0]);

	return scnprintf(buf, PAGE_SIZE, "%d\n", portmode);
}

ssize_t musb_portmode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb;
	unsigned short portmode_new, portmode_old;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return count;
	} else if (sscanf(buf, "%ud", &portmode_new) == 1) {

		ssusb = dev_to_ssusb(dev);
		portmode_old = usb_mtkphy_check_in_uart_mode(ssusb->phys[0]);

		pr_debug("\nPortmode: %d=>%d\n", portmode_old, portmode_new);
		if (portmode_new >= PORT_MODE_MAX)
			portmode_new = PORT_MODE_USB;

		if (portmode_old != portmode_new) {
			if (portmode_new == PORT_MODE_USB) {
				/* Changing to USB Mode */
				pr_debug("USB Port mode -> USB\n");
				usb_mtkphy_switch_to_usb(ssusb->phys[0]);
			} else if (portmode_new == PORT_MODE_UART) {
				/* Changing to UART Mode */
				pr_debug("USB Port mode -> UART\n");
				usb_mtkphy_switch_to_uart(ssusb->phys[0]);
			}
			usb_mtkphy_dump_usb2uart_reg(ssusb->phys[0]);
			portmode_old = portmode_new;
		}
	}
	return count;
}

DEVICE_ATTR(portmode, 0664, musb_portmode_show, musb_portmode_store);
#endif


#ifdef CONFIG_MTK_SIB_USB_SWITCH
#include <linux/phy/mediatek/mtk_usb_phy.h>

ssize_t musb_sib_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct ssusb_mtk *ssusb;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return 0;
	}

	ssusb = dev_to_ssusb(dev);

	ret = usb_mtkphy_sib_enable_switch_status(ssusb->phys[0]);
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

ssize_t musb_sib_enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned int mode;
	struct ssusb_mtk *ssusb;

	if (!dev) {
		pr_debug("dev is null!!\n");
		return count;
	}

	ssusb = dev_to_ssusb(dev);

	if (!kstrtouint(buf, 0, &mode)) {
		pr_debug("USB sib_enable: %d\n", mode);
		usb_mtkphy_sib_enable_switch(ssusb->phys[0], mode);
	}
	return count;
}

DEVICE_ATTR(sib_enable, 0664, musb_sib_enable_show, musb_sib_enable_store);
#endif


struct attribute *mtu3_attributes[] = {
	&dev_attr_cmode.attr,
	&dev_attr_saving.attr,
#ifdef CONFIG_MTK_UART_USB_SWITCH
	&dev_attr_portmode.attr,
#endif
#ifdef CONFIG_MTK_SIB_USB_SWITCH
	&dev_attr_sib_enable.attr,
#endif
	NULL
};

const struct attribute_group mtu3_attr_group = {
	.attrs = mtu3_attributes,
};
#endif
#endif

/* u2-port0 should be powered on and enabled; */
int ssusb_check_clocks(struct ssusb_mtk *ssusb, u32 ex_clks)
{
	void __iomem *ibase = ssusb->ippc_base;
	u32 value, check_val;
	int ret;

	check_val = ex_clks | SSUSB_SYS125_RST_B_STS;

	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS1, value,
			(check_val == (value & check_val)), 100, 20000);
	if (ret) {
		dev_err(ssusb->dev, "clks of sts1 are not stable!\n");
	}

	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS2, value,
			(value & SSUSB_U2_MAC_SYS_RST_B_STS), 100, 10000);
	if (ret) {
		dev_err(ssusb->dev, "mac2 clock is not stable\n");
	}

	return 0;
}

#if !defined(CONFIG_USB_MU3D_DRV)
static int ssusb_phy_init(struct ssusb_mtk *ssusb)
{
	int i;
	int ret;

	for (i = 0; i < ssusb->num_phys; i++) {
		ret = phy_init(ssusb->phys[i]);
		if (ret)
			goto exit_phy;
	}
	return 0;

exit_phy:
	for (; i > 0; i--)
		phy_exit(ssusb->phys[i - 1]);

	return ret;
}
#endif

static int ssusb_phy_exit(struct ssusb_mtk *ssusb)
{
	int i;

	for (i = 0; i < ssusb->num_phys; i++)
		phy_exit(ssusb->phys[i]);

	return 0;
}

#if !defined(CONFIG_USB_MU3D_DRV)
static int ssusb_phy_power_on(struct ssusb_mtk *ssusb)
{
	int i;
	int ret;

	for (i = 0; i < ssusb->num_phys; i++) {
		ret = phy_power_on(ssusb->phys[i]);
		if (ret)
			goto power_off_phy;
	}
	return 0;

power_off_phy:
	for (; i > 0; i--)
		phy_power_off(ssusb->phys[i - 1]);

	return ret;
}
#endif

static void ssusb_phy_power_off(struct ssusb_mtk *ssusb)
{
	unsigned int i;

	for (i = 0; i < ssusb->num_phys; i++)
		phy_power_off(ssusb->phys[i]);
}

#if !defined(CONFIG_USB_MU3D_DRV)
static int ssusb_rscs_init(struct ssusb_mtk *ssusb)
{
	int ret = 0;

	ret = regulator_enable(ssusb->vusb33);
	if (ret) {
		dev_err(ssusb->dev, "failed to enable vusb33\n");
		goto vusb33_err;
	}

	ret = ssusb_ext_pwr_on(ssusb, ssusb->is_host);
	if (ret)
		dev_info(ssusb->dev, "failed to enable vusb10\n");

	ret = ssusb_clk_on(ssusb, ssusb->is_host);
	if (ret) {
		dev_err(ssusb->dev, "failed to enable sys_clk\n");
		goto sys_clk_err;
	}
	ret = ssusb_phy_init(ssusb);
	if (ret) {
		dev_err(ssusb->dev, "failed to init phy\n");
		goto phy_init_err;
	}

	ret = ssusb_phy_power_on(ssusb);
	if (ret) {
		dev_err(ssusb->dev, "failed to power on phy\n");
		goto phy_err;
	}

	return 0;

phy_err:
	ssusb_phy_exit(ssusb);
phy_init_err:
	clk_disable_unprepare(ssusb->sys_clk);
sys_clk_err:
	regulator_disable(ssusb->vusb33);
vusb33_err:

	return ret;
}
#endif

static void ssusb_rscs_exit(struct ssusb_mtk *ssusb)
{
	regulator_disable(ssusb->vusb33);
	ssusb_ext_pwr_off(ssusb, ssusb->is_host);
	ssusb_phy_power_off(ssusb);
	ssusb_phy_exit(ssusb);
}

#if !defined(CONFIG_USB_MU3D_DRV)
static void ssusb_ip_sw_reset(struct ssusb_mtk *ssusb)
{
	/* reset whole ip (xhci & u3d) */
	mtu3_setbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	udelay(1);
	mtu3_clrbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
}
#endif

static int get_ssusb_rscs(struct platform_device *pdev, struct ssusb_mtk *ssusb)
{
	struct device_node *node = pdev->dev.of_node;
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int i;

	ssusb->vusb33 = devm_regulator_get(&pdev->dev, "vusb");
	if (IS_ERR(ssusb->vusb33)) {
		dev_err(dev, "failed to get vusb33\n");
		return PTR_ERR(ssusb->vusb33);
	}

	ssusb->sys_clk = devm_clk_get(dev, "sys_ck");
	if (IS_ERR(ssusb->sys_clk)) {
		dev_err(dev, "failed to get sys clock\n");
		return PTR_ERR(ssusb->sys_clk);
	}

	ssusb->ref_clk = devm_clk_get(dev, "rel_clk");
	if (IS_ERR(ssusb->ref_clk)) {
		dev_info(dev, "failed to get ref clock\n");
		return PTR_ERR(ssusb->ref_clk);
	}

	ssusb->num_phys = of_count_phandle_with_args(node,
			"phys", "#phy-cells");

	if (ssusb->num_phys > 0) {
		ssusb->phys = devm_kcalloc(dev, ssusb->num_phys,
					sizeof(*ssusb->phys), GFP_KERNEL);
		if (!ssusb->phys)
			return -ENOMEM;
	} else {
		ssusb->num_phys = 0;
	}

	for (i = 0; i < ssusb->num_phys; i++) {
		ssusb->phys[i] = devm_of_phy_get_by_index(dev, node, i);

		if (IS_ERR(ssusb->phys[i])) {
			dev_err(dev, "failed to get phy-%d\n", i);
			return PTR_ERR(ssusb->phys[i]);
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ippc");
	ssusb->ippc_base = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(ssusb->ippc_base)) {
		dev_err(dev, "failed to map memory for ippc\n");
		return PTR_ERR(ssusb->ippc_base);
	}

	ssusb->dr_mode = usb_get_dr_mode(dev);
	if (ssusb->dr_mode == USB_DR_MODE_UNKNOWN) {
		dev_err(dev, "dr_mode is error\n");
		return -EINVAL;
	}

	ssusb->force_vbus =
		of_property_read_bool(node, "mediatek,force_vbus_det");

	if (ssusb->dr_mode == USB_DR_MODE_PERIPHERAL)
		return 0;

	if (ssusb->dr_mode != USB_DR_MODE_OTG)
		return 0;

	otg_sx->is_u3_drd = of_property_read_bool(node,
				"mediatek,usb3-drd");
	mtu3_speed = otg_sx->is_u3_drd;
	otg_sx->is_u3h_drd = of_property_read_bool(node,
				"mediatek,usb3h-drd");

	if (of_property_read_bool(node, "extcon")) {
		otg_sx->edev = extcon_get_edev_by_phandle(ssusb->dev, 0);
		if (IS_ERR(otg_sx->edev)) {
			dev_err(ssusb->dev, "couldn't get extcon device\n");
			return -EPROBE_DEFER;
		}
	}

	dev_info(dev, "dr_mode: %d, is_u3_dr: %d, is_u3h_dr: %d\n",
		ssusb->dr_mode, otg_sx->is_u3_drd, otg_sx->is_u3h_drd);

	return 0;
}

static int mtu3_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct ssusb_mtk *ssusb;
	int ret = -ENOMEM;

	/* all elements are set to ZERO as default value */
	ssusb = devm_kzalloc(dev, sizeof(*ssusb), GFP_KERNEL);
	if (!ssusb)
		return -ENOMEM;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dev, "No suitable DMA config available\n");
		return -ENOTSUPP;
	}

	platform_set_drvdata(pdev, ssusb);
	ssusb->dev = dev;

	dev_set_name(dev, "musb-hdrc");

	ret = get_ssusb_rscs(pdev, ssusb);
	if (ret)
		return ret;

	ret = get_ssusb_ext_rscs(ssusb);
	if (ret)
		return ret;

	/* enable power domain */
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	device_enable_async_suspend(dev);

	if (IS_ENABLED(CONFIG_USB_MTU3_HOST))
		ssusb->dr_mode = USB_DR_MODE_HOST;
	else if (IS_ENABLED(CONFIG_USB_MTU3_GADGET))
		ssusb->dr_mode = USB_DR_MODE_PERIPHERAL;
#if !defined(CONFIG_USB_MU3D_DRV)
	ret = ssusb_rscs_init(ssusb);
	if (ret)
		goto comm_init_err;

	ssusb_ip_sw_reset(ssusb);
#endif
	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		ret = ssusb_gadget_init(ssusb);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			goto comm_exit;
		}
		break;
	case USB_DR_MODE_HOST:
		ret = ssusb_host_init(ssusb, node);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			goto comm_exit;
		}
		break;
	case USB_DR_MODE_OTG:
#if !defined(CONFIG_USB_MU3D_DRV)
		ret = ssusb_gadget_init(ssusb);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			goto comm_exit;
		}
#endif

		ret = ssusb_host_init(ssusb, node);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			goto gadget_exit;
		}
		phy_hal_init(ssusb->phys[0]);
		ssusb_otg_switch_init(ssusb);

		break;
	default:
		dev_err(dev, "unsupported mode: %d\n", ssusb->dr_mode);
		ret = -EINVAL;
		goto comm_exit;
	}

#ifdef CONFIG_SYSFS
#if !defined(CONFIG_USB_MU3D_DRV)
	ret = sysfs_create_group(&dev->kobj, &mtu3_attr_group);
	if (ret)
		dev_info(dev, "failed to create group\n");
#endif
#endif

#ifdef CONFIG_MTK_BOOT
	if (get_boot_mode() == META_BOOT) {
		dev_info(dev, "in special mode %d\n", get_boot_mode());
		/*mtu3_cable_mode = CABLE_MODE_FORCEON;*/
	}
#endif

	return 0;

gadget_exit:
	ssusb_gadget_exit(ssusb);
comm_exit:
	ssusb_rscs_exit(ssusb);
#if !defined(CONFIG_USB_MU3D_DRV)
comm_init_err:
#endif
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);

	return ret;
}

static int mtu3_remove(struct platform_device *pdev)
{
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		ssusb_gadget_exit(ssusb);
		break;
	case USB_DR_MODE_HOST:
		ssusb_host_exit(ssusb);
		break;
	case USB_DR_MODE_OTG:
		ssusb_otg_switch_exit(ssusb);
		phy_hal_exit(ssusb->phys[0]);
		ssusb_gadget_exit(ssusb);
		ssusb_host_exit(ssusb);
		break;
	default:
		return -EINVAL;
	}

	ssusb_rscs_exit(ssusb);
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

/*
 * when support dual-role mode, we reject suspend when
 * it works as device mode;
 */
static int __maybe_unused mtu3_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	dev_info(dev, "%s\n", __func__);

	/* REVISIT: disconnect it for only device mode? */
	if (!ssusb->is_host)
		return 0;

	ssusb_host_disable(ssusb, ssusb->is_host);
	/* ssusb_phy_power_off(ssusb); */
	ssusb_clk_off(ssusb, ssusb->is_host);
	usb_wakeup_enable(ssusb);
	return 0;
}

static int __maybe_unused mtu3_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	dev_info(dev, "%s\n", __func__);

	if (!ssusb->is_host)
		return 0;

	usb_wakeup_disable(ssusb);
	ssusb_clk_on(ssusb, ssusb->is_host);
	/* ssusb_phy_power_on(ssusb); */
	ssusb_host_enable(ssusb);
	return 0;
}

static const struct dev_pm_ops mtu3_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtu3_suspend, mtu3_resume)
};

#define DEV_PM_OPS (IS_ENABLED(CONFIG_PM) ? &mtu3_pm_ops : NULL)

#ifdef CONFIG_OF

static const struct of_device_id mtu3_of_match[] = {
	{.compatible = "mediatek,mt6758-mtu3",},
	{.compatible = "mediatek,mt3967-mtu3",},
	{.compatible = "mediatek,mt6779-mtu3",},
	{},
};

MODULE_DEVICE_TABLE(of, mtu3_of_match);

#endif

static struct platform_driver mtu3_driver = {
	.probe = mtu3_probe,
	.remove = mtu3_remove,
	.driver = {
		.name = "mtu3_phone",
		.pm = DEV_PM_OPS,
		.of_match_table = of_match_ptr(mtu3_of_match),
	},
};

static int __init mtu3_driver_init(void)
{
	int ret;

	ret =  platform_driver_register(&mtu3_driver);

	return ret;
}

late_initcall(mtu3_driver_init);

MODULE_AUTHOR("Chunfeng Yun <chunfeng.yun@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek USB3 DRD Controller Driver");
