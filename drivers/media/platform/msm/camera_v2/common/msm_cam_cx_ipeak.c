/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <linux/of.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <soc/qcom/cx_ipeak.h>
#include "msm_cam_cx_ipeak.h"

static struct cx_ipeak_client *cam_cx_ipeak;
static int cx_default_ipeak_mask;
static int cx_current_ipeak_mask;
static int cam_cx_client_cnt;

int cam_cx_ipeak_register_cx_ipeak(struct cx_ipeak_client *p_cam_cx_ipeak,
	int *cam_cx_ipeak_bit)
{
	int rc = 0;

	*cam_cx_ipeak_bit = 1 << cam_cx_client_cnt++;
	cx_default_ipeak_mask |= *cam_cx_ipeak_bit;

	if (cam_cx_ipeak)
		goto exit;

	cam_cx_ipeak = p_cam_cx_ipeak;

	if (!cam_cx_ipeak)
		rc = -EINVAL;

exit:
	pr_debug("%s: client_cnt %d client mask = %x default_mask = %x\n",
		__func__, cam_cx_client_cnt, *cam_cx_ipeak_bit,
		cx_default_ipeak_mask);
	return rc;
}

int cam_cx_ipeak_update_vote_cx_ipeak(int cam_cx_ipeak_bit)
{
	int32_t soc_cx_ipeak_bit = cam_cx_ipeak_bit;
	int ret = 0;

	pr_debug("%s: E: current_mask = %x default_mask = %x new bit = %x\n",
		__func__, cx_current_ipeak_mask,
		cx_default_ipeak_mask, soc_cx_ipeak_bit);

	cx_current_ipeak_mask |= soc_cx_ipeak_bit;
	pr_debug("%s: current_mask = %x\n", __func__, cx_current_ipeak_mask);

	if (cx_current_ipeak_mask == cx_default_ipeak_mask) {
		if (cam_cx_ipeak) {
			ret = cx_ipeak_update(cam_cx_ipeak, true);
			if (ret)
				goto exit;
			pr_debug("%s: X: All client VOTE\n", __func__);
		}
	}
exit:
	return ret;
}

int cam_cx_ipeak_unvote_cx_ipeak(int cam_cx_ipeak_bit)
{
	int32_t soc_cx_ipeak_bit = cam_cx_ipeak_bit;

	pr_debug("%s: current_mask = %x soc_cx_ipeak_bit = %x\n", __func__,
		cx_current_ipeak_mask, soc_cx_ipeak_bit);
	if (cx_current_ipeak_mask == cx_default_ipeak_mask) {
		if (cam_cx_ipeak)
			cx_ipeak_update(cam_cx_ipeak, false);
		pr_debug("%s: One client requested UNVOTE\n", __func__);
	}
	cx_current_ipeak_mask &= (~soc_cx_ipeak_bit);
	pr_debug("%s:X:updated cx_current_ipeak_mask = %x\n", __func__,
		cx_current_ipeak_mask);

	return 0;
}
