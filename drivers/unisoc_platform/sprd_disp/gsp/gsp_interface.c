// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include "gsp_core.h"
#include "gsp_dev.h"
#include "gsp_debug.h"
#include "gsp_interface.h"
#include "gsp_interface/gsp_interface_sharkl5pro.h"
#include "gsp_interface/gsp_interface_qogirn6pro.h"

static struct gsp_interface_ops gsp_interface_sharkl5pro_ops = {
	.parse_dt = gsp_interface_sharkl5pro_parse_dt,
	.init = gsp_interface_sharkl5pro_init,
	.deinit = gsp_interface_sharkl5pro_deinit,
	.prepare = gsp_interface_sharkl5pro_prepare,
	.unprepare = gsp_interface_sharkl5pro_unprepare,
	.reset = gsp_interface_sharkl5pro_reset,
	.dump = gsp_interface_sharkl5pro_dump,
};

static struct gsp_interface_ops gsp_interface_qogirn6pro_ops = {
	.parse_dt = gsp_interface_qogirn6pro_parse_dt,
	.init = gsp_interface_qogirn6pro_init,
	.deinit = gsp_interface_qogirn6pro_deinit,
	.prepare = gsp_interface_qogirn6pro_prepare,
	.unprepare = gsp_interface_qogirn6pro_unprepare,
	.reset = gsp_interface_qogirn6pro_reset,
	.dump = gsp_interface_qogirn6pro_dump,
};

int gsp_interface_is_attached(struct gsp_interface *interface)
{
	return interface->attached == true ? 1 : 0;
}

char *gsp_interface_to_name(struct gsp_interface *interface)
{
	return interface->name;
}

void gsp_interface_copy_name(const char *orig, char *dst)
{
	int i = 0;

	for (i = 0; i < 2; i++) {
		while (*orig != '-')
			orig++;
		orig++;
	}

	strcpy(dst, orig);
}

int gsp_interface_attach(struct gsp_interface **interface, struct gsp_dev *gsp)
{
	int ret = -1;
	const char *tmp = NULL;
	char name[32];
	struct device *dev = NULL;
	struct gsp_core *core = NULL;

	dev = gsp_dev_to_device(gsp);
	ret = of_property_read_string(dev->of_node, "compatible", &tmp);
	if (ret) {
		GSP_ERR("read compatible name failed\n");
		return ret;
	}
	gsp_interface_copy_name(tmp, name);
	GSP_INFO("gsp interface name: %s\n", name);

	if (strcmp(GSP_SHARKL5PRO, name) == 0) {
		*interface = kzalloc(sizeof(struct gsp_interface_sharkl5pro),
				     GFP_KERNEL);
		if (IS_ERR_OR_NULL(*interface)) {
			GSP_ERR("alloc interface[%s] failed\n", name);
			goto error;
		}
		memset(*interface, 0, sizeof(struct gsp_interface_sharkl5pro));
		(*interface)->ops = &gsp_interface_sharkl5pro_ops;
	} else if (strcmp(GSP_QOGIRN6PRO, name) == 0) {
		*interface = kzalloc(sizeof(struct gsp_interface_qogirn6pro),
					GFP_KERNEL);
		if (IS_ERR_OR_NULL(*interface)) {
			GSP_ERR("alloc interface[%s] failed\n", name);
			goto error;
		}
		memset(*interface, 0, sizeof(struct gsp_interface_qogirn6pro));
		(*interface)->ops = &gsp_interface_qogirn6pro_ops;
	} else {/* can add other interface with "else if" */
		GSP_WARN("no match interface for gsp\n");
		goto error;
	}
	strlcpy((*interface)->name, name, sizeof((*interface)->name));

	/* here get core[0] device node as default */
	core = gsp_dev_to_core(gsp, 0);
	dev = gsp_core_to_device(core);
	ret = (*interface)->ops->parse_dt(*interface, dev->of_node);
	if (ret) {
		GSP_ERR("interface[%s] parse dt failed\n",
			gsp_interface_to_name(*interface));
		goto free;
	}

	(*interface)->attached = true;
	(*interface)->attached_dev = gsp;
	return ret;

free:
	kfree((*interface));
	(*interface) = NULL;
error:
	ret = -1;
	return ret;
}

void gsp_interface_detach(struct gsp_interface *interface)
{
	if (IS_ERR_OR_NULL(interface)) {
		GSP_WARN("interface may been detached before\n");
		return;
	}

	kfree(interface);
}

int gsp_interface_prepare(struct gsp_interface *interface)
{
	if (IS_ERR_OR_NULL(interface)) {
		GSP_ERR("interface params error\n");
		return -1;
	}

	if (interface->ops == NULL || interface->ops->prepare == NULL) {
		GSP_ERR("interface[%s] has not register prepare function\n",
			gsp_interface_to_name(interface));
		return -1;
	}

	return interface->ops->prepare(interface);
}

int gsp_interface_unprepare(struct gsp_interface *interface)
{
	if (interface == NULL) {
		GSP_ERR("interface params error");
		return -1;
	}

	if (interface->ops == NULL || interface->ops->unprepare == NULL) {
		GSP_ERR("interface[%s] has not register unprepare function\n",
			gsp_interface_to_name(interface));
		return -1;
	}

	return interface->ops->unprepare(interface);
}

int gsp_interface_reset(struct gsp_interface *interface)
{
	if (interface == NULL) {
		GSP_ERR("interface params error");
		return -1;
	}

	if (interface->ops == NULL || interface->ops->reset == NULL) {
		GSP_ERR("interface[%s] has not register reset function\n",
			gsp_interface_to_name(interface));
		return -1;
	}

	return interface->ops->reset(interface);
}

int gsp_interface_init(struct gsp_interface *interface)
{
	if (IS_ERR_OR_NULL(interface)) {
		GSP_ERR("interface params error\n");
		return -1;
	}

	if (interface->ops == NULL || interface->ops->init == NULL) {
		GSP_ERR("interface[%s] has not register init function\n",
			gsp_interface_to_name(interface));
		return -1;
	}

	return interface->ops->init(interface);
}

int gsp_interface_deinit(struct gsp_interface *interface)
{
	if (interface == NULL) {
		GSP_ERR("interface params error");
		return -1;
	}

	if (interface->ops == NULL || interface->ops->deinit == NULL) {
		GSP_ERR("interface[%s] has not register deinit function\n",
			gsp_interface_to_name(interface));
		return -1;
	}

	return interface->ops->deinit(interface);
}
