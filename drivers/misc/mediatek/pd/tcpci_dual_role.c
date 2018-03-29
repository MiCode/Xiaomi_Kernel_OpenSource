/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/tcpci_dual_role.c
 *
 * Author: TH <tsunghan_tasi@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

#ifdef CONFIG_DUAL_ROLE_USB_INTF
static enum dual_role_property tcpc_dual_role_props[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
	DUAL_ROLE_PROP_VCONN_SUPPLY,
};

static int tcpc_dual_role_get_prop(struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop, unsigned int *val)
{
	struct tcpc_device *tcpc = dev_get_drvdata(dual_role->dev.parent);
	int ret = 0;

	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		*val = tcpc->dual_role_mode;
		break;
	case DUAL_ROLE_PROP_PR:
		*val = tcpc->dual_role_pr;
		break;
	case DUAL_ROLE_PROP_DR:
		*val = tcpc->dual_role_dr;
		break;
	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		*val = tcpc->dual_role_vconn;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static	int tcpc_dual_role_prop_is_writeable(
	struct dual_role_phy_instance *dual_role, enum dual_role_property prop)
{
	int retval = -ENOSYS;
	struct tcpc_device *tcpc = dev_get_drvdata(dual_role->dev.parent);

	switch (prop) {
	case DUAL_ROLE_PROP_PR:
	case DUAL_ROLE_PROP_DR:
	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		if (tcpc->dual_role_supported_modes ==
			DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP)
			retval = 1;
		break;
	default:
		break;
	}
	return retval;
}

static int tcpc_dual_role_set_prop(struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop, const unsigned int *val)
{
	struct tcpc_device *tcpc = dev_get_drvdata(dual_role->dev.parent);

	switch (prop) {
	#ifdef CONFIG_USB_POWER_DELIVERY
	case DUAL_ROLE_PROP_PR:
		if (*val != tcpc->dual_role_pr) {
			pr_info("%s power role swap (%d->%d)\n",
				__func__, tcpc->dual_role_pr, *val);
			tcpm_power_role_swap(tcpc);
		} else
			pr_info("%s Same Power Role\n", __func__);
		break;
	case DUAL_ROLE_PROP_DR:
		if (*val != tcpc->dual_role_dr) {
			pr_info("%s data role swap (%d->%d)\n",
				__func__, tcpc->dual_role_dr, *val);
			tcpm_data_role_swap(tcpc);
		} else
			pr_info("%s Same Data Role\n", __func__);
		break;
	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		if (*val != tcpc->dual_role_vconn) {
			pr_info("%s vconn swap (%d->%d)\n",
				__func__, tcpc->dual_role_vconn, *val);
			tcpm_vconn_swap(tcpc);
		} else
			pr_info("%s Same Vconn\n", __func__);
		break;
	default:
		break;
	#else /* TypeC Only */
	case DUAL_ROLE_PROP_PR:
	case DUAL_ROLE_PROP_DR:
	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		tcpc_typec_swap_role(tcpc);
	default:
		break;
	#endif /* CONFIG_USB_POWER_DELIVERY */
	}
	return 0;
}

static void tcpc_get_dual_desc(
		struct dual_role_phy_desc *desc, struct device_node *np)
{
	u32 val;

	if (of_property_read_u32(np, "rt-dual,supported_modes", &val) >= 0) {
		if (val > DUAL_ROLE_PROP_SUPPORTED_MODES_TOTAL)
			desc->supported_modes =
					DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
		else
			desc->supported_modes = val;
	}
}

int tcpc_dual_role_phy_init(
			struct tcpc_device *tcpc)
{
	struct dual_role_phy_desc *dual_desc;
	int len;
	char *str_name;
	struct device_node *np = of_find_node_by_name(NULL, tcpc->desc.name);

	if (!np) {
		pr_err("%s not device node %s\n", __func__, tcpc->desc.name);
		return -EINVAL;
	}

	tcpc->dr_usb = devm_kzalloc(&tcpc->dev,
				sizeof(tcpc->dr_usb), GFP_KERNEL);

	dual_desc = devm_kzalloc(&tcpc->dev, sizeof(*dual_desc), GFP_KERNEL);
	if (!dual_desc)
		return -ENOMEM;

	tcpc_get_dual_desc(dual_desc, np);

	len = strlen(tcpc->desc.name);
	str_name = devm_kzalloc(&tcpc->dev, len+11, GFP_KERNEL);
	sprintf(str_name, "dual-role-%s", tcpc->desc.name);
	dual_desc->name = str_name;

	dual_desc->properties = tcpc_dual_role_props;
	dual_desc->num_properties = ARRAY_SIZE(tcpc_dual_role_props);
	dual_desc->get_property = tcpc_dual_role_get_prop;
	dual_desc->set_property = tcpc_dual_role_set_prop;
	dual_desc->property_is_writeable = tcpc_dual_role_prop_is_writeable;

	tcpc->dr_usb = devm_dual_role_instance_register(&tcpc->dev, dual_desc);
	if (IS_ERR(tcpc->dr_usb)) {
		dev_err(&tcpc->dev, "tcpc fail to register dual role usb\n");
		return -EINVAL;
	}
	/* init dual role phy instance property */
	tcpc->dual_role_pr = DUAL_ROLE_PROP_PR_NONE;
	tcpc->dual_role_dr = DUAL_ROLE_PROP_DR_NONE;
	tcpc->dual_role_mode = DUAL_ROLE_PROP_MODE_NONE;
	tcpc->dual_role_vconn = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
	return 0;
}
EXPORT_SYMBOL(tcpc_dual_role_phy_init);
#endif /* CONFIG_DUAL_ROLE_USB_INTF */
