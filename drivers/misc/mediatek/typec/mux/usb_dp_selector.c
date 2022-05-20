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

/* MT6983 uds_V1 offset = [11]
 * MT6985 uds_V2 offset = [18] dp 4-lanes offset = [19]
 * Reserved for future platform.
 */

enum usb_dp_sw_vers {
	uds_V1 = 1,
	uds_V2,
};

struct usb_dp_selector {
	struct device *dev;
	struct typec_switch *sw;
	struct typec_mux *mux;
	struct mutex lock;
	void __iomem *selector_reg_address;
	int uds_ver;
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
		switch (uds->uds_ver) {
		case uds_V1:
			uds_setbits(uds->selector_reg_address, (1 << 11));
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
			mtk_dp_aux_swap_enable(false);
#endif
			break;
		case uds_V2:
			uds_setbits(uds->selector_reg_address, (1 << 18));
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
			mtk_dp_aux_swap_enable(false);
#endif
			break;
		default:
			break;
		}
		break;
	case TYPEC_ORIENTATION_REVERSE:
		/* USB FLIP TX2, DP TX1 */
		switch (uds->uds_ver) {
		case uds_V1:
			uds_clrbits(uds->selector_reg_address, (1 << 11));
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
			mtk_dp_aux_swap_enable(true);
#endif
			break;
		case uds_V2:
			uds_clrbits(uds->selector_reg_address, (1 << 18));
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
			mtk_dp_aux_swap_enable(true);
#endif
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return 0;
}

/*
 * case
 *  4 Pin Assignment C 4-lans
 * 16 Pin Assignment E 4-lans
 *  8 Pin Assignment D 2-lans
 * 32 Pin Assignment F 2-lans
 */

static int usb_dp_selector_mux_set(struct typec_mux *mux,
				struct typec_mux_state *state)
{
	struct usb_dp_selector *uds = typec_mux_get_drvdata(mux);
	struct tcp_notify *data = state->data;
	int ret = 0;

	/*dev_info(uds->dev, "usb_dp_selector_mux_set\n");
	 *dev_info(uds->dev, "state->mode : %d\n", state->mode);
	 *dev_info(uds->dev, "data-> polarity : %d\n", data->ama_dp_state.polarity);
	 *dev_info(uds->dev, "data-> signal : %d\n", data->ama_dp_state.signal);
	 *dev_info(uds->dev, "data-> pin_assignment : %d\n", data->ama_dp_state.pin_assignment);
	 *dev_info(uds->dev, "data-> active : %d\n", data->ama_dp_state.active);
	 */

	if (state->mode == TCP_NOTIFY_AMA_DP_STATE) {
		switch (data->ama_dp_state.pin_assignment) {
		/* 4-lanes */
		case 4:
		case 16:
			switch (uds->uds_ver) {
			case uds_V1:
				break;
			case uds_V2:
				uds_setbits(uds->selector_reg_address, (1 << 19));
				break;
			default:
				break;
			}
			break;
		/* 2-lanes */
		case 8:
		case 32:
			switch (uds->uds_ver) {
			case uds_V1:
				break;
			case uds_V2:
				uds_clrbits(uds->selector_reg_address, (1 << 19));
				break;
			default:
				break;
			}
			break;
		default:
			dev_info(uds->dev, "%s Pin Assignment not support\n", __func__);
			break;
		}
	} else if (state->mode == TCP_NOTIFY_AMA_DP_HPD_STATE) {
		uint8_t irq = data->ama_dp_hpd_state.irq;
		uint8_t state = data->ama_dp_hpd_state.state;

		dev_info(uds->dev, "TCP_NOTIFY_AMA_DP_HPD_STATE irq:%x state:%x\n",
			irq, state);
		/* Call DP API */
		dev_info(uds->dev, "[%s][%d]\n", __func__, __LINE__);
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
		if (state) {
			if (irq)
				mtk_dp_SWInterruptSet(0x8);
			else
				mtk_dp_SWInterruptSet(0x4);
		} else {
			mtk_dp_SWInterruptSet(0x2);
		}
#endif
	} else if (state->mode == TCP_NOTIFY_TYPEC_STATE) {
		if ((data->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			data->typec_state.old_state == TYPEC_ATTACHED_SNK) &&
			data->typec_state.new_state == TYPEC_UNATTACHED) {
			/* Call DP Event API Ready */
			dev_info(uds->dev, "Plug Out, Disconnect HPD\n");
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
			mtk_dp_SWInterruptSet(0x2);
#endif
		}
	}

	return ret;
}

static int usb_dp_selector_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct usb_dp_selector *uds;
	struct typec_switch_desc sw_desc = { };
	struct typec_mux_desc mux_desc = { };
	int ret;

	uds = devm_kzalloc(&pdev->dev, sizeof(*uds), GFP_KERNEL);
	if (!uds)
		return -ENOMEM;

	uds->dev = dev;

	uds->selector_reg_address = devm_platform_ioremap_resource_byname(pdev, "usb_dp_reg");
	if (IS_ERR(uds->selector_reg_address))
		return PTR_ERR(uds->selector_reg_address);

	ret = device_property_read_u32(dev, "mediatek,uds-ver",
			&uds->uds_ver);

	if (ret) {
		uds->uds_ver = 1;
		dev_info(dev, "used default usb_dp selector reg version = %d\n", uds->uds_ver);
	} else {
		dev_info(dev, "uds-ver = %d\n", uds->uds_ver);
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

	/* Setting MUX callback */
	mux_desc.drvdata = uds;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = usb_dp_selector_mux_set;
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
	uds->mux = mtk_typec_mux_register(dev, &mux_desc);
#else
	uds->mux = typec_switch_register(dev, &mux_desc);
#endif
	if (IS_ERR(uds->mux)) {
		dev_info(dev, "error registering typec mux: %ld\n",
			PTR_ERR(uds->mux));
		return PTR_ERR(uds->mux);
	}

	platform_set_drvdata(pdev, uds);

	dev_info(dev, "probe done\n");
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
