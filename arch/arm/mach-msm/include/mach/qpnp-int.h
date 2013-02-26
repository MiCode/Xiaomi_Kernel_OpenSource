/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef QPNPINT_H
#define QPNPINT_H

#include <linux/spmi.h>

struct qpnp_irq_spec {
	uint8_t slave; /* 0-15 */
	uint8_t per; /* 0-255 */
	uint8_t irq; /* 0-7 */
};

struct qpnp_local_int {
	 /* mask - Invoke PMIC Arbiter local mask handler */
	int (*mask)(struct spmi_controller *spmi_ctrl,
		    struct qpnp_irq_spec *spec,
		    uint32_t priv_d);
	 /* unmask - Invoke PMIC Arbiter local unmask handler */
	int (*unmask)(struct spmi_controller *spmi_ctrl,
		      struct qpnp_irq_spec *spec,
		      uint32_t priv_d);
	/* register_priv_data - Return per irq priv data */
	int (*register_priv_data)(struct spmi_controller *spmi_ctrl,
				  struct qpnp_irq_spec *spec,
				  uint32_t *priv_d);
};

#ifdef CONFIG_MSM_QPNP_INT
/**
 * qpnpint_of_init() - Device Tree irq initialization
 *
 * Standard Device Tree init routine to be called from
 * of_irq_init().
 */
int __init qpnpint_of_init(struct device_node *node,
			   struct device_node *parent);

/**
 * qpnpint_register_controller() - Register local interrupt callbacks
 *
 * Used by the PMIC Arbiter driver or equivalent to register
 * callbacks for interrupt events.
 */
int qpnpint_register_controller(struct device_node *node,
				struct spmi_controller *ctrl,
				struct qpnp_local_int *li_cb);

/**
 * qpnpint_unregister_controller() - Unregister local interrupt callbacks
 *
 * Used by the PMIC Arbiter driver or equivalent to unregister
 * callbacks for interrupt events.
 */
int qpnpint_unregister_controller(struct device_node *node);

/**
 * qpnpint_handle_irq - Main interrupt handling routine
 *
 * Pass a PMIC Arbiter interrupt to Linux.
 */
int qpnpint_handle_irq(struct spmi_controller *spmi_ctrl,
		       struct qpnp_irq_spec *spec);
#else
static inline int __init qpnpint_of_init(struct device_node *node,
				  struct device_node *parent)
{
	return -ENXIO;
}

static inline int qpnpint_register_controller(struct device_node *node,
					      struct spmi_controller *ctrl,
					      struct qpnp_local_int *li_cb)

{
	return -ENXIO;
}

static inline int qpnpint_unregister_controller(struct device_node *node)

{
	return -ENXIO;
}

static inline int qpnpint_handle_irq(struct spmi_controller *spmi_ctrl,
		       struct qpnp_irq_spec *spec)
{
	return -ENXIO;
}
#endif /* CONFIG_MSM_QPNP_INT */
#endif /* QPNPINT_H */
