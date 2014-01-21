/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __CLK_VCAP_8092_H__
#define __CLK_VCAP_8092_H__

#include <linux/clk.h>

void vcap_clk_ctrl_pre_init(struct clk *ahb_clk, struct clk *vp_clk);

struct hdmirx_tmds_clk {
	struct clk c;
};

struct afe_pixel_clk {
	struct clk c;
};

extern struct hdmirx_tmds_clk vcap_tmds_clk_src;
extern struct afe_pixel_clk vcap_afe_pixel_clk_src;

#endif /* __CLK_VCAP_8092_H__ */
