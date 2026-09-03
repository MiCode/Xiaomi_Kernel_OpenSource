// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Spreadtrum virtual Type-C
 *
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/usb/typec.h>
#include <linux/usb/otg.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock_types.h>
#include <linux/platform_device.h>
#include <linux/extcon.h>

struct virtual_typec {
	struct device *dev;

	struct extcon_dev *edev;
	struct extcon_dev *id_edev;
	struct notifier_block vbus_nb;
	struct notifier_block id_nb;

	enum usb_dr_mode dr_mode;
	bool is_active;
	spinlock_t lock;

	struct typec_port *port;
	struct typec_partner *partner;
	struct typec_capability typec_cap;
	const struct sprd_typec_variant_data *var_data;
};

int virtual_typec_connect(struct virtual_typec *sc, bool flag)
{
	enum typec_data_role data_role;
	enum typec_role power_role;
	struct typec_partner_desc desc;

	dev_info(sc->dev,
		"%s enter in %s mode, line %d\n", __func__, flag ? "Host" : "Device", __LINE__);
	if (sc->partner)
		return 0;

	if (flag) {
		data_role = TYPEC_HOST;
		power_role = TYPEC_SOURCE;
	} else {
		data_role = TYPEC_DEVICE;
		power_role = TYPEC_SINK;
	}

	desc.accessory = TYPEC_ACCESSORY_NONE;
	desc.identity = NULL;

	sc->partner = typec_register_partner(sc->port, &desc);
	if (!sc->partner)
		return -ENODEV;

	typec_set_pwr_role(sc->port, power_role);
	typec_set_data_role(sc->port, data_role);

	return 0;
}

int virtual_typec_disconnect(struct virtual_typec *sc)
{

	dev_info(sc->dev, "%s enter line %d\n", __func__, __LINE__);
	typec_unregister_partner(sc->partner);
	sc->partner = NULL;

	typec_set_pwr_role(sc->port, TYPEC_SINK);
	typec_set_data_role(sc->port, TYPEC_DEVICE);

	return 0;
}

static int virtual_typec_vbus_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct virtual_typec *sc = container_of(nb, struct virtual_typec, vbus_nb);
	unsigned long flags;

	spin_lock_irqsave(&sc->lock, flags);
	if (event) {
		if (sc->is_active || sc->dr_mode == USB_DR_MODE_HOST) {
			spin_unlock_irqrestore(&sc->lock, flags);
			dev_info(sc->dev,
				"ignore micro connect vbus notifier!\n");
			return 0;
		}

		sc->dr_mode = USB_DR_MODE_PERIPHERAL;
		sc->is_active = true;
	} else {
		if (!sc->is_active || sc->dr_mode == USB_DR_MODE_HOST) {
			spin_unlock_irqrestore(&sc->lock, flags);
			dev_info(sc->dev,
				"ignore micro disconnect vbus notifier!\n");
			return 0;
		}

		sc->dr_mode = USB_DR_MODE_UNKNOWN;
		sc->is_active = false;
	}

	if (sc->dr_mode == USB_DR_MODE_PERIPHERAL && sc->is_active == true) {
		spin_unlock_irqrestore(&sc->lock, flags);
		virtual_typec_connect(sc, false);
	} else {
		spin_unlock_irqrestore(&sc->lock, flags);
		virtual_typec_disconnect(sc);
	}

	return 0;
}

static int virtual_typec_id_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct virtual_typec *sc = container_of(nb, struct virtual_typec, id_nb);
	unsigned long flags;

	spin_lock_irqsave(&sc->lock, flags);
	if (event) {
		if (sc->is_active || sc->dr_mode == USB_DR_MODE_PERIPHERAL) {
			spin_unlock_irqrestore(&sc->lock, flags);
			dev_info(sc->dev,
				"ignore micro connect id notifier!\n");
			return 0;
		}

		sc->dr_mode = USB_DR_MODE_HOST;
		sc->is_active = true;
	} else {
		if (!sc->is_active || sc->dr_mode == USB_DR_MODE_PERIPHERAL) {
			spin_unlock_irqrestore(&sc->lock, flags);
			dev_info(sc->dev,
				"ignore micro disconnect id notifier!\n");
			return 0;
		}

		sc->dr_mode = USB_DR_MODE_UNKNOWN;
		sc->is_active = false;
	}

	if (sc->dr_mode == USB_DR_MODE_HOST && sc->is_active == true) {
		spin_unlock_irqrestore(&sc->lock, flags);
		virtual_typec_connect(sc, true);
	} else {
		spin_unlock_irqrestore(&sc->lock, flags);
		virtual_typec_disconnect(sc);
	}

	return 0;
}

static void virtual_typec_detect_cable(struct virtual_typec *sc)
{
	unsigned long flags;

	spin_lock_irqsave(&sc->lock, flags);
	if (extcon_get_state(sc->edev, EXTCON_USB_HOST) == true) {
		if (sc->is_active) {
			spin_unlock_irqrestore(&sc->lock, flags);
			dev_info(sc->dev,
				"ignore micro id connect detect_cable!\n");
			return;
		}

		sc->dr_mode = USB_DR_MODE_HOST;
		sc->is_active = true;
	} else if (extcon_get_state(sc->edev, EXTCON_USB) == true) {
		if (sc->is_active) {
			spin_unlock_irqrestore(&sc->lock, flags);
			dev_info(sc->dev,
				"ignore micro vbus connect detect_cable!\n");
			return;
		}

		sc->dr_mode = USB_DR_MODE_PERIPHERAL;
		sc->is_active = true;
	}

	if (sc->dr_mode == USB_DR_MODE_HOST) {
		spin_unlock_irqrestore(&sc->lock, flags);
		virtual_typec_connect(sc, true);
	} else if (sc->dr_mode == USB_DR_MODE_PERIPHERAL) {
		spin_unlock_irqrestore(&sc->lock, flags);
		virtual_typec_connect(sc, false);
	} else {
		spin_unlock_irqrestore(&sc->lock, flags);
	}

}

int virtual_typec_probe(struct platform_device *pdev)
{
	struct virtual_typec *sc;
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	sc = devm_kzalloc(&pdev->dev, sizeof(*sc), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->typec_cap.type = 2;
	sc->typec_cap.data = TYPEC_PORT_DRD;
	sc->port = typec_register_port(&pdev->dev, &sc->typec_cap);

	if (!sc->port) {
		dev_err(sc->dev, "failed to register port!\n");
		return -ENODEV;
	}

	sc->dev = &pdev->dev;

	/* get micro vbus/id gpios extcon device */
	if (of_property_read_bool(node, "extcon")) {
		sc->edev = extcon_get_edev_by_phandle(sc->dev, 0);
		if (IS_ERR(sc->edev)) {
			ret = PTR_ERR(sc->edev);
			dev_err(dev, "vir_typec get micro extcon failed.\n");
			goto err_extcon_port;
		}
		sc->vbus_nb.notifier_call = virtual_typec_vbus_notifier;
		ret = extcon_register_notifier(sc->edev, EXTCON_USB,
						&sc->vbus_nb);
		if (ret) {
			dev_err(dev,
			"vir_typec register extcon vbus notifier failed.\n");
			goto err_extcon_port;
		}

		sc->id_nb.notifier_call = virtual_typec_id_notifier;
		ret = extcon_register_notifier(sc->edev, EXTCON_USB_HOST,
							&sc->id_nb);
		if (ret) {
			dev_err(dev,
			"vir_typec register extcon host notifier failed.\n");
			goto err_extcon_vbus;
		}
	}
	spin_lock_init(&sc->lock);
	virtual_typec_detect_cable(sc);

	dev_info(sc->dev, "%s probe ok enter line  %d\n", __func__, __LINE__);
	return 0;

err_extcon_vbus:
	if (sc->edev)
		extcon_unregister_notifier(sc->edev, EXTCON_USB,
					&sc->vbus_nb);

err_extcon_port:
	typec_unregister_port(sc->port);
	return ret;
}

static int virtual_typec_remove(struct platform_device *pdev)
{
	struct virtual_typec *sc = platform_get_drvdata(pdev);

	virtual_typec_disconnect(sc);
	typec_unregister_port(sc->port);

	return 0;
}

static const struct of_device_id virtual_typec_match[] = {
	{.compatible = "sprd,virtual-typec", },
	{},
};
MODULE_DEVICE_TABLE(of, virtual_typec_match);

static struct platform_driver virtual_typec_driver = {
	.probe = virtual_typec_probe,
	.remove = virtual_typec_remove,
	.driver = {
		.name = "virtual-typec",
		.of_match_table = virtual_typec_match,
	},
};

static int __init virtual_typec_init(void)
{
	return platform_driver_register(&virtual_typec_driver);
}

static void __exit virtual_typec_exit(void)
{
	platform_driver_unregister(&virtual_typec_driver);
}

module_init(virtual_typec_init);
module_exit(virtual_typec_exit);

MODULE_LICENSE("GPL v2");
