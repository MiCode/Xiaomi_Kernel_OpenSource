/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * Author: Akashdeep Sharma <akashdeep.sharma@intel.com>
 * Author: Shashank Sharma <shashank.sharma@intel.com>
 */

#ifndef _VLV_HDMI_PORT_H_
#define _VLV_HDMI_PORT_H_
#include <linux/types.h>
#include <linux/i2c.h>
#include <core/common/hdmi/gen_hdmi_pipe.h>

struct vlv_hdmi_port {
	u32 control_reg;
	u32 dip_stat;
	u32 dip_ctrl;
	u32 dip_data;
	u32 hpd_stat;
	u32 hpd_detect;
	u32 hpd_ctrl;
	struct i2c_adapter *adapter;
	enum port port_id;
};

u32 vlv_hdmi_port_enable(struct vlv_hdmi_port *port);
u32 vlv_hdmi_port_disable(struct vlv_hdmi_port *port);
bool vlv_hdmi_port_init(struct vlv_hdmi_port *port, enum port, enum pipe);
bool vlv_hdmi_port_destroy(struct vlv_hdmi_port *port);
u32 vlv_hdmi_port_prepare(struct vlv_hdmi_port *port, u32 val);

/* Added for HDMI audio */
void vlv_hdmi_port_enable_audio(struct vlv_hdmi_port *port);
void vlv_hdmi_port_disable_audio(struct vlv_hdmi_port *port);
u32 vlv_hdmi_port_write_dip(struct vlv_hdmi_port *port, bool enable);

#endif /* _VLV_HDMI_PORT_H_ */
