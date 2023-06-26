/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __USB_TYPEC_MUX
#define __USB_TYPEC_MUX

#include <linux/device.h>

struct typec_mux;
struct typec_switch;

enum typec_orientation {
	TYPEC_ORIENTATION_NONE,
	TYPEC_ORIENTATION_NORMAL,
	TYPEC_ORIENTATION_REVERSE,
};

typedef int (*typec_switch_set_fn_t)(struct typec_switch *sw,
				     enum typec_orientation orientation);
struct typec_switch_desc {
	struct fwnode_handle *fwnode;
	typec_switch_set_fn_t set;
	void *drvdata;
};

struct typec_switch *
typec_switch_register(struct device *parent,
		      const struct typec_switch_desc *desc);
void typec_switch_unregister(struct typec_switch *sw);

void typec_switch_set_drvdata(struct typec_switch *sw, void *data);
void *typec_switch_get_drvdata(struct typec_switch *sw);

typedef int (*typec_mux_set_fn_t)(struct typec_mux *mux, int state);

struct typec_mux_desc {
	struct fwnode_handle *fwnode;
	typec_mux_set_fn_t set;
	void *drvdata;
};

struct typec_mux *
typec_mux_register(struct device *parent, const struct typec_mux_desc *desc);
void typec_mux_unregister(struct typec_mux *mux);

void typec_mux_set_drvdata(struct typec_mux *mux, void *data);
void *typec_mux_get_drvdata(struct typec_mux *mux);

struct typec_switch {
	struct device dev;
	typec_switch_set_fn_t set;
};

struct typec_mux {
	struct device dev;
	typec_mux_set_fn_t set;
};

#define to_typec_switch(_dev_) container_of(_dev_, struct typec_switch, dev)
#define to_typec_mux(_dev_) container_of(_dev_, struct typec_mux, dev)

#endif /* __USB_TYPEC_MUX */

