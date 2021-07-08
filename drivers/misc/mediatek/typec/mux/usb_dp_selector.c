// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>
#include "tcpm.h"

#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
#include "mux_switch.h"
#endif

struct usb_dp_selector {
	struct device *dev;
	struct typec_switch *sw;
	struct mutex lock;
	void __iomem *selector_reg_address;
};

static inline void uds_setbits(void __iomem *base, u32 bits)
{
	void __iomem *addr = base;
	u32 tmp = readl(addr);

	writel((tmp | (bits)), addr);
}

static inline void uds_clrbits(void __iomem *base, u32 bits)
{
	void __iomem *addr = base;
	u32 tmp = readl(addr);

	writel((tmp & ~(bits)), addr);
}

static int usb_dp_selector_switch_set(struct typec_switch *sw,
			      enum typec_orientation orientation)
{
	struct usb_dp_selector *uds = typec_switch_get_drvdata(sw);

	dev_info(uds->dev, "%s %d\n", __func__, orientation);

	switch (orientation) {
	case TYPEC_ORIENTATION_NONE:
		/* Nothing */
		break;
	case TYPEC_ORIENTATION_NORMAL:
		/* USB NORMAL TX1, DP TX2 */
		uds_clrbits(uds->selector_reg_address, 11);
		break;
	case TYPEC_ORIENTATION_REVERSE:
		/* USB FLIP TX2, DP TX1 */
		uds_setbits(uds->selector_reg_address, 11);
		break;
	default:
		break;
	}

	return 0;
}

static int usb_dp_selector_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct usb_dp_selector *uds;
	struct typec_switch_desc sw_desc;
	u32 usb_dp_reg;
	int ret;

	uds = devm_kzalloc(&pdev->dev, sizeof(*uds), GFP_KERNEL);
	if (!uds)
		return -ENOMEM;

	uds->dev = dev;

	ret = of_property_read_u32(np, "mediatek,usb_dp_reg", &usb_dp_reg);
	if (ret) {
		dev_info(dev, "Failed to parse usb_dp reg value\n");
		return ret;
	}
	uds->selector_reg_address = ioremap(usb_dp_reg, 0x1);

	if (!uds->selector_reg_address) {
		dev_info(dev, "could not ioremap usb_dp reg\n");
		return -ENOMEM;
	}

	/* Setting Switch callback */
	sw_desc.drvdata = uds;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = usb_dp_selector_switch_set;

#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
	uds->sw = mtk_typec_switch_register(dev, &sw_desc);
#else
	uds->sw = typec_switch_register(dev, &sw_desc);
#endif
	if (IS_ERR(uds->sw)) {
		dev_info(dev, "error registering typec switch: %ld\n",
			PTR_ERR(uds->sw));
		return PTR_ERR(uds->sw);
	}

	platform_set_drvdata(pdev, uds);

	dev_info(dev, "%s done\n", __func__);
	return 0;
}

static int usb_dp_selector_remove(struct platform_device *pdev)
{
	struct usb_dp_selector *uds = platform_get_drvdata(pdev);

	mtk_typec_switch_unregister(uds->sw);
	return 0;
}

static const struct of_device_id usb_dp_selector_ids[] = {
	{.compatible = "mediatek,usb_dp_selector",},
	{},
};

static struct platform_driver usb_dp_selector_driver = {
	.driver = {
		.name = "usb_dp_selector",
		.of_match_table = usb_dp_selector_ids,
	},
	.probe = usb_dp_selector_probe,
	.remove = usb_dp_selector_remove,
};

module_platform_driver(usb_dp_selector_driver);

MODULE_DESCRIPTION("Type-C DP/USB selector");
MODULE_LICENSE("GPL v2");
