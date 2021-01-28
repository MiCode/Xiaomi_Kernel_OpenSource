// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"
#include <linux/usb/typec.h>

static int tcpc_dual_role_set_prop_pr(const struct typec_capability *cap,
				      enum typec_role trole)
{
	int ret;
	uint8_t val, role;
	struct tcpc_device *tcpc = container_of(cap,
						struct tcpc_device, typec_caps);

	switch (trole) {
	case TYPEC_SOURCE:
		val = DUAL_ROLE_PROP_PR_SRC;
		role = PD_ROLE_SOURCE;
		break;
	case TYPEC_SINK:
		val = DUAL_ROLE_PROP_PR_SNK;
		role = PD_ROLE_SINK;
		break;
	default:
		return 0;
	}

	if (val == tcpc->dual_role_pr) {
		pr_info("%s wrong role (%d->%d)\n",
			__func__, tcpc->dual_role_pr, val);
		return 0;
	}

	ret = tcpm_dpm_pd_power_swap(tcpc, role, NULL);
	pr_info("%s power role swap (%d->%d): %d\n",
		__func__, tcpc->dual_role_pr, val, ret);

	if (ret == TCPM_ERROR_NO_PD_CONNECTED) {
		ret = tcpm_typec_role_swap(tcpc);
		pr_info("%s typec role swap (%d->%d): %d\n",
			__func__, tcpc->dual_role_pr, val, ret);
	}

	typec_set_pwr_role(tcpc->typec_port, trole);
	return ret;
}

static int tcpc_dual_role_set_prop_dr(const struct typec_capability *cap,
				      enum typec_data_role data)
{
	int ret;
	uint8_t val, role;
	struct tcpc_device *tcpc = container_of(cap,
						struct tcpc_device, typec_caps);

	switch (data) {
	case TYPEC_HOST:
		val = DUAL_ROLE_PROP_DR_HOST;
		role = PD_ROLE_DFP;
		break;
	case TYPEC_DEVICE:
		val = DUAL_ROLE_PROP_DR_DEVICE;
		role = PD_ROLE_UFP;
		break;
	default:
		return 0;
	}

	if (val == tcpc->dual_role_dr) {
		pr_info("%s wrong role (%d->%d)\n",
			__func__, tcpc->dual_role_dr, val);
		return 0;
	}

	ret = tcpm_dpm_pd_data_swap(tcpc, role, NULL);
	pr_info("%s data role swap (%d->%d): %d\n",
		__func__, tcpc->dual_role_dr, val, ret);

	typec_set_data_role(tcpc->typec_port, data);

	return ret;
}

static int tcpc_dual_role_set_prop_vconn(const struct typec_capability *cap,
					 enum typec_role trole)
{
	int ret;
	uint8_t val, role;
	struct tcpc_device *tcpc = container_of(cap,
						struct tcpc_device, typec_caps);

	switch (trole) {
	case TYPEC_SINK:
		val = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
		role = PD_ROLE_VCONN_OFF;
		break;
	case TYPEC_SOURCE:
		val = DUAL_ROLE_PROP_VCONN_SUPPLY_YES;
		role = PD_ROLE_VCONN_ON;
		break;
	default:
		return 0;
	}

	if (val == tcpc->dual_role_vconn) {
		pr_info("%s wrong role (%d->%d)\n",
			__func__, tcpc->dual_role_vconn, val);
		return 0;
	}

	ret = tcpm_dpm_pd_vconn_swap(tcpc, role, NULL);
	pr_info("%s vconn swap (%d->%d): %d\n",
		__func__, tcpc->dual_role_vconn, val, ret);

	typec_set_vconn_role(tcpc->typec_port, trole);

	return ret;
}

static int tcpm_port_type_set(const struct typec_capability *cap,
			      enum typec_port_type type)
{
	uint8_t role;
	struct tcpc_device *tcpc = container_of(cap,
						struct tcpc_device, typec_caps);

	switch (type) {
	case TYPEC_PORT_SNK:
		role = TYPEC_ROLE_SNK;
		break;
	case TYPEC_PORT_SRC:
		role = TYPEC_ROLE_SRC;
		break;
	case TYPEC_PORT_DRP:
		role = TYPEC_ROLE_DRP;
		break;
	default:
		return 0;
	}

	return tcpm_typec_change_role(tcpc, role);
}

static int tcpm_try_role(const struct typec_capability *cap, int role)
{
	struct tcpc_device *tcpc = container_of(cap,
						struct tcpc_device, typec_caps);

	if (role != TYPEC_ROLE_TRY_SRC && role != TYPEC_ROLE_TRY_SNK)
		return 0;

	return tcpm_typec_change_role(tcpc, role);
}

int tcpc_dual_role_phy_init(struct tcpc_device *tcpc)
{
	struct device_node *switch_np;
	int err;

	tcpc->typec_caps.revision = 0x0120;	/* Type-C spec release 1.2 */
	tcpc->typec_caps.pd_revision = 0x0300;	/* USB-PD spec release 3.0 */
	tcpc->typec_caps.dr_set = tcpc_dual_role_set_prop_dr;
	tcpc->typec_caps.pr_set = tcpc_dual_role_set_prop_pr;
	tcpc->typec_caps.vconn_set = tcpc_dual_role_set_prop_vconn;
	tcpc->typec_caps.try_role = tcpm_try_role;
	tcpc->typec_caps.port_type_set = tcpm_port_type_set;
	tcpc->typec_caps.type = TYPEC_PORT_DRP;
	tcpc->typec_caps.data = TYPEC_PORT_DRD;
	tcpc->typec_caps.prefer_role = TYPEC_SINK;

	switch_np = of_parse_phandle(tcpc->dev.parent->of_node, "switch", 0);
	if (switch_np) {
		tcpc->dev_conn.endpoint[0] = kasprintf(GFP_KERNEL,
				"%s-switch", switch_np->name);
		/* 0 is typec port id */
		tcpc->dev_conn.endpoint[1] = kasprintf(GFP_KERNEL,
				"port%d", 0);
		tcpc->dev_conn.id = "orientation-switch";
		device_connection_add(&tcpc->dev_conn);
		dev_info(&tcpc->dev, "add %s\n", tcpc->dev_conn.endpoint[0]);
	} else
		dev_info(&tcpc->dev, "can't find switch\n");

	tcpc->typec_port = typec_register_port(&tcpc->dev, &tcpc->typec_caps);
	if (IS_ERR(tcpc->typec_port)) {
		err = PTR_ERR(tcpc->typec_port);
		return -EINVAL;
	}

	return 0;
}
