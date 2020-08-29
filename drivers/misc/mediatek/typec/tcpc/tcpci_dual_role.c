// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/usb/typec.h>
#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

struct typec_port {
	unsigned int			id;
	struct device			dev;
	struct ida			mode_ids;

	int				prefer_role;
	enum typec_data_role		data_role;
	enum typec_role			pwr_role;
	enum typec_role			vconn_role;
	enum typec_pwr_opmode		pwr_opmode;
	enum typec_port_type		port_type;
	struct mutex			port_type_lock;

	enum typec_orientation		orientation;
	struct typec_switch		*sw;
	struct typec_mux		*mux;

	const struct typec_capability	*cap;
	const struct typec_operations   *ops;
};

static int tcpc_dual_role_set_prop_pr(struct typec_port *port,
				      enum typec_role trole)
{
	int ret;
	uint8_t val, role;
	struct tcpc_device *tcpc = container_of(port->cap,
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

	return ret;
}

static int tcpc_dual_role_set_prop_dr(struct typec_port *port,
				      enum typec_data_role data)
{
	int ret;
	uint8_t val, role;
	struct tcpc_device *tcpc = container_of(port->cap,
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

	return ret;
}

static int tcpc_dual_role_set_prop_vconn(struct typec_port *port,
					 enum typec_role trole)
{
	int ret;
	uint8_t val, role;
	struct tcpc_device *tcpc = container_of(port->cap,
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

	return ret;
}

static int tcpm_port_type_set(struct typec_port *port,
			      enum typec_port_type type)
{
	uint8_t role;
	struct tcpc_device *tcpc = container_of(port->cap,
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

static int tcpm_try_role(struct typec_port *port, int role)
{
	struct tcpc_device *tcpc = container_of(port->cap,
						struct tcpc_device, typec_caps);

	if (role != TYPEC_ROLE_TRY_SRC && role != TYPEC_ROLE_TRY_SNK)
		return 0;

	return tcpm_typec_change_role(tcpc, role);
}

static const struct typec_operations mtk_ops = {
	.dr_set = tcpc_dual_role_set_prop_dr,
	.pr_set = tcpc_dual_role_set_prop_pr,
	.vconn_set = tcpc_dual_role_set_prop_vconn,
	.try_role = tcpm_try_role,
	.port_type_set = tcpm_port_type_set,
};

int tcpc_dual_role_phy_init(struct tcpc_device *tcpc)
{
	tcpc->typec_caps.revision = 0x0120;	/* Type-C spec release 1.2 */
	tcpc->typec_caps.pd_revision = 0x0300;	/* USB-PD spec release 3.0 */
	tcpc->typec_caps.ops = &mtk_ops;
	tcpc->typec_caps.type = TYPEC_PORT_DRP;
	tcpc->typec_caps.data = TYPEC_PORT_DRD;
	tcpc->typec_caps.prefer_role = TYPEC_SINK;
	tcpc->typec_port = typec_register_port(&tcpc->dev, &tcpc->typec_caps);

	return PTR_ERR_OR_ZERO(tcpc->typec_port);
}
