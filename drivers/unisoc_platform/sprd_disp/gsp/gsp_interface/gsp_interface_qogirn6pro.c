// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */


#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include "gsp_interface_qogirn6pro.h"
#include "../gsp_debug.h"
#include "../gsp_interface.h"

int gsp_interface_qogirn6pro_parse_dt(struct gsp_interface *intf,
				  struct device_node *node)
{
	int status = 0;

	struct gsp_interface_qogirn6pro *gsp_interface = NULL;

	gsp_interface = (struct gsp_interface_qogirn6pro *)intf;

	gsp_interface->clk_dpu_vsp_eb = of_clk_get_by_name(node,
		QOGIRN6PRO_DPU_VSP_EB_NAME);

	if (IS_ERR_OR_NULL(gsp_interface->clk_dpu_vsp_eb)) {
		GSP_ERR("iread clk_dpu_vsp_eb failed\n");
		status = -1;
	}
      return status;
}

int gsp_interface_qogirn6pro_init(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_qogirn6pro_deinit(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_qogirn6pro_prepare(struct gsp_interface *intf)
{
	int ret = -1;
	struct gsp_interface_qogirn6pro *gsp_interface = NULL;

	if (IS_ERR_OR_NULL(intf)) {
		GSP_ERR("interface params error\n");
		return ret;
	}

	gsp_interface = (struct gsp_interface_qogirn6pro *)intf;


	ret = clk_prepare_enable(gsp_interface->clk_dpu_vsp_eb);
	if (ret) {
		GSP_ERR("enable interface[%s] clk_dpu_vsp_eb failed\n",
			gsp_interface_to_name(intf));
		goto clk_dpu_vsp_eb_disable;
	}

	goto exit;

clk_dpu_vsp_eb_disable:
	clk_disable_unprepare(gsp_interface->clk_dpu_vsp_eb);
	GSP_ERR("interface[%s] prepare ERR !\n",
		gsp_interface_to_name(intf));

exit:
	GSP_DEBUG("interface[%s] prepare success\n",
		gsp_interface_to_name(intf));
	return ret;
}

int gsp_interface_qogirn6pro_unprepare(struct gsp_interface *intf)
{
	struct gsp_interface_qogirn6pro *gsp_interface = NULL;

	if (IS_ERR_OR_NULL(intf)) {
		GSP_ERR("interface params error\n");
		return -1;
	}

	gsp_interface = (struct gsp_interface_qogirn6pro *)intf;

	clk_disable_unprepare(gsp_interface->clk_dpu_vsp_eb);

	GSP_DEBUG("interface[%s] unprepare success\n",
		  gsp_interface_to_name(intf));
	return 0;
}

int gsp_interface_qogirn6pro_reset(struct gsp_interface *intf)
{
	return 0;
}

void gsp_interface_qogirn6pro_dump(struct gsp_interface *intf)
{

}
