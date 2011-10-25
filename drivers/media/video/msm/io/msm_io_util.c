/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <mach/board.h>
#include <mach/camera.h>

int msm_cam_clk_enable(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable)
{
	int i;
	int rc = 0;
	if (enable) {
		for (i = 0; i < num_clk; i++) {
			clk_ptr[i] = clk_get(dev, clk_info[i].clk_name);
			if (IS_ERR(clk_ptr[i])) {
				pr_err("%s get failed\n", clk_info[i].clk_name);
				rc = PTR_ERR(clk_ptr[i]);
				goto cam_clk_get_err;
			}
			if (clk_info[i].clk_rate >= 0) {
				rc = clk_set_rate(clk_ptr[i],
							clk_info[i].clk_rate);
				if (rc < 0) {
					pr_err("%s set failed\n",
						   clk_info[i].clk_name);
					goto cam_clk_set_err;
				}
			}
			rc = clk_enable(clk_ptr[i]);
			if (rc < 0) {
				pr_err("%s enable failed\n",
					   clk_info[i].clk_name);
				goto cam_clk_set_err;
			}
		}
	} else {
		for (i = num_clk - 1; i >= 0; i--) {
			if (clk_ptr[i] != NULL)
				clk_disable(clk_ptr[i]);
				clk_put(clk_ptr[i]);
		}
	}
	return rc;


cam_clk_set_err:
	clk_put(clk_ptr[i]);
cam_clk_get_err:
	for (i--; i >= 0; i--) {
		if (clk_ptr[i] != NULL) {
			clk_disable(clk_ptr[i]);
			clk_put(clk_ptr[i]);
		}
	}
	return rc;
}

