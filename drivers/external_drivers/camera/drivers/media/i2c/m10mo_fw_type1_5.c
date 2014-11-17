/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
 * Partially based on m-5mols kernel driver,
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * Partially based on jc_v4l2 kernel driver from http://opensource.samsung.com
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/atomisp_platform.h>
#include <media/m10mo_atomisp.h>
#include <linux/module.h>
#include "m10mo.h"

static int m10mo_set_high_speed(struct v4l2_subdev *sd)
{
	struct m10mo_device *dev = to_m10mo_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	dev_dbg(&client->dev, "%s: enter\n", __func__);

	if (dev->mode != M10MO_PARAM_SETTING_MODE &&
		dev->mode != M10MO_PARAMETER_MODE) {
		/*
		 * We should switch to param mode first and
		 * reset all the parameters.
		 */
		ret = __m10mo_param_mode_set(sd);
		if (ret)
			goto out;
	}

	ret = m10mo_writeb(sd, CATEGORY_CAPTURE_CTRL, CAPTURE_MODE,
				CAP_MODE_MOVIE);
	if (ret)
		goto out;

	/* 1080P@60FPS */
	ret = m10mo_writeb(sd, CATEGORY_PARAM, PARAM_MON_SIZE,
			MON_SIZE_FHD_60FPS);
	if (ret)
		goto out;
	/* NO meta data */
	ret = m10mo_writeb(sd, CATEGORY_PARAM,
			MON_METADATA_SUPPORT_CTRL,
			MON_METADATA_SUPPORT_CTRL_DIS);
	if (ret)
		goto out;
	/* Select format NV12 */
	ret = m10mo_writeb(sd, CATEGORY_PARAM, CHOOSE_NV12NV21_FMT,
			CHOOSE_NV12NV21_FMT_NV12);
	if (ret)
		goto out;
	/* Enable interrupt signal */
	ret = m10mo_writeb(sd, CATEGORY_SYSTEM,
			SYSTEM_INT_ENABLE, 0x01);
	if (ret)
		goto out;
	/* Go to Monitor mode and output YUV Data */
	ret = m10mo_request_mode_change(sd,
			M10MO_MONITOR_MODE_HIGH_SPEED);
	if (ret)
		goto out;

	ret = m10mo_wait_mode_change(sd, M10MO_MONITOR_MODE_HIGH_SPEED,
			M10MO_INIT_TIMEOUT);
	if (ret < 0)
		goto out;

	return 0;
out:
	dev_err(&client->dev, "%s:streaming failed %d\n", __func__, ret);
	return ret;
}

int m10mo_set_run_mode_fw_type1_5(struct v4l2_subdev *sd)
{
	struct m10mo_device *dev = to_m10mo_sensor(sd);
	int ret;

	/*
	 * Handle RAW capture mode separately irrespective of the run mode
	 * being configured. Start the RAW capture right away.
	 */
	if (dev->capture_mode == M10MO_CAPTURE_MODE_ZSL_RAW) {
		/*
		 * As RAW capture is done from a command line tool, we are not
		 * restarting the preview after the RAW capture. So it is ok
		 * to reset the RAW capture mode here because the next RAW
		 * capture has to start from the Set format onwards.
		 */
		dev->capture_mode = M10MO_CAPTURE_MODE_ZSL_NORMAL;
		return m10mo_set_zsl_raw_capture(sd);
	}

	switch (dev->run_mode) {
	case CI_MODE_STILL_CAPTURE:
		ret = m10mo_set_still_capture(sd);
		break;
	default:
		/* TODO: Revisit this logic on switching to panorama */
		if (dev->curr_res_table[dev->fmt_idx].command == 0x43)
			ret = m10mo_set_panorama_monitor(sd);
		else if (dev->fps == M10MO_HIGH_SPEED_FPS)
			ret = m10mo_set_high_speed(sd);
		else
			ret = m10mo_set_zsl_monitor(sd);
	}
	return ret;

}

const struct m10mo_fw_ops fw_type1_5_ops = {
	.set_run_mode           = m10mo_set_run_mode_fw_type1_5,
	.set_burst_mode         = m10mo_set_burst_mode,
	.stream_off             = m10mo_streamoff,
	.single_capture_process = m10mo_single_capture_process,
	.try_mbus_fmt           =  __m10mo_try_mbus_fmt,
	.set_mbus_fmt           =  __m10mo_set_mbus_fmt,
	.test_pattern           = m10mo_test_pattern,
};
