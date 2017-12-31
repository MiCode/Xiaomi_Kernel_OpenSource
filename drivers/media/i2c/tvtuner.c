/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/media.h>
#include <media/v4l2-ioctl.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#include <linux/platform_device.h>
#include <linux/of_platform.h>

#include <media/msm_ba.h>

#include "tvtuner.h"

#define DRIVER_NAME "tv-tuner"

struct Tvtuner_state {
	struct device *dev;

	/* V4L2 Data */
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler ctrl_hdl;
	struct v4l2_dv_timings timings;

	/* media entity controls */
	struct media_pad pad;

	struct mutex		mutex;
};


/* Initialize Tvtuner I2C Settings */
static int Tvtuner_dev_init(struct Tvtuner_state *state)
{
	int ret = 0;

	TUNER_DEBUG("tv_tuner dev init is started\n");
	return ret;
}

/* Initialize Tvtuner hardware */
static int Tvtuner_hw_init(struct Tvtuner_state *state)
{
	int ret = 0;

	TUNER_DEBUG("tv_tuner hw init is started\n");
	return ret;
}

static int Tvtuner_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;

	TUNER_DEBUG("tv_tuner set control is started id = 0x%x\n", ctrl->id);
	return ret;
}

static int Tvtuner_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	int ret = 0;
	struct v4l2_mbus_framefmt *fmt = &format->format;

	fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
	fmt->width = 1280;
	fmt->height = 720;
	fmt->colorspace = V4L2_COLORSPACE_SMPTE170M;

	TUNER_DEBUG("tv_tuner get mbus format is started\n");
	return ret;
}

static int Tvtuner_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *interval)
{
	int ret = 0;

	TUNER_DEBUG("tv_tuner get frame interval is started\n");
	return ret;
}

static int Tvtuner_s_routing(struct v4l2_subdev *sd, u32 input,
				u32 output, u32 config)
{
	int ret = 0;

	TUNER_DEBUG("tv_tuner s_routing is started\n");
	return ret;
}

static int Tvtuner_query_dv_timings(struct v4l2_subdev *sd,
			struct v4l2_dv_timings *timings)
{
	int ret = 0;

	TUNER_DEBUG("tv_tuner query dv timings is started\n");
	return ret;
}

static int Tvtuner_query_sd_std(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	int ret = 0;

	TUNER_DEBUG("tv_tuner query SD input is started\n");
	return ret;
}

static int Tvtuner_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	int ret = 0;
	*status = 1;

	TUNER_DEBUG("tv_tuner get input status is started\n");
	return ret;
}

static int Tvtuner_s_stream(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

	TUNER_DEBUG("tv_tuner start stream is started\n");
	return ret;
}

static const struct v4l2_subdev_video_ops Tvtuner_video_ops = {
	.s_routing = Tvtuner_s_routing,
	.g_frame_interval = Tvtuner_g_frame_interval,
	.querystd = Tvtuner_query_sd_std,
	.g_dv_timings = Tvtuner_query_dv_timings,
	.g_input_status = Tvtuner_g_input_status,
	.s_stream = Tvtuner_s_stream,
};


static const struct v4l2_ctrl_ops Tvtuner_ctrl_ops = {
	.s_ctrl = Tvtuner_s_ctrl,
};

static const struct v4l2_subdev_pad_ops Tvtuner_pad_ops = {
	.get_fmt = Tvtuner_get_fmt,
};

static const struct v4l2_subdev_ops Tvtuner_ops = {
	.video = &Tvtuner_video_ops,
	.pad = &Tvtuner_pad_ops,
};

static int Tvtuner_init_v4l2_controls(struct Tvtuner_state *state)
{
	int ret = 0;

	TUNER_DEBUG("%s: Exit with ret: %d\n", __func__, ret);
	return ret;
}

static int Tvtuner_parse_dt(struct platform_device *pdev,
		struct Tvtuner_state *state)
{

	int ret = 0;

	TUNER_DEBUG("%s: tvtuner parse dt called\n", __func__);
	return ret;
}

static const struct of_device_id Tvtuner_id[] = {
	{ .compatible = "qcom,tv-tuner", },
	{},
};

MODULE_DEVICE_TABLE(of, Tvtuner_id);

static int Tvtuner_probe(struct platform_device *pdev)
{
	struct Tvtuner_state *state;
	const struct of_device_id *device_id;
	struct v4l2_subdev *sd;
	int ret;

	device_id = of_match_device(Tvtuner_id, &pdev->dev);
	if (!device_id) {
		TUNER_DEBUG("%s: device_id is NULL\n", __func__);
		ret = -ENODEV;
		goto err;
	}

	/* Create Tvtuner State */
	state = devm_kzalloc(&pdev->dev,
			sizeof(struct Tvtuner_state), GFP_KERNEL);
	if (state == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	platform_set_drvdata(pdev, state);
	state->dev = &pdev->dev;

	mutex_init(&state->mutex);
	ret = Tvtuner_parse_dt(pdev, state);
	if (ret < 0) {
		TUNER_ERROR("Error parsing dt tree\n");
		goto err_mem_free;
	}

	/* Configure and Register V4L2 Sub-device */
	sd = &state->sd;
	v4l2_subdev_init(sd, &Tvtuner_ops);
	sd->owner = pdev->dev.driver->owner;
	v4l2_set_subdevdata(sd, state);
	strlcpy(sd->name, DRIVER_NAME, sizeof(sd->name));
	state->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;

	/* Register as Media Entity */
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	state->sd.entity.flags |= (unsigned long)MEDIA_ENT_T_V4L2_SUBDEV;
	ret = media_entity_init(&state->sd.entity, 1, &state->pad, 0);
	if (ret) {
		ret = -EIO;
		TUNER_ERROR("%s(%d): Media entity init failed\n",
			__func__, __LINE__);
		goto err_media_entity;
	}

	/* Initialize HW Config */
	ret = Tvtuner_hw_init(state);
	if (ret) {
		ret = -EIO;
		TUNER_ERROR("%s: HW Initialisation Failed\n", __func__);
		goto err_media_entity;
	}

	ret = Tvtuner_init_v4l2_controls(state);
	if (ret) {
		TUNER_ERROR("%s: V4L2 Controls Initialisation Failed %d\n",
			__func__, ret);
	}

	/* Initialize SW Init Settings and I2C sub maps */
	ret = Tvtuner_dev_init(state);
	if (ret) {
		ret = -EIO;
		TUNER_ERROR("%s(%d): SW Initialisation Failed\n",
			__func__, __LINE__);
		goto err_media_entity;
	}

	/* BA registration */
	TUNER_DEBUG(" register msm-ba driver to tv_tuner");
	ret = msm_ba_register_subdev_node(sd);
	if (ret) {
		ret = -EIO;
		TUNER_DEBUG("%s: BA init failed\n", __func__);
		goto err_media_entity;
	}
	TUNER_DEBUG("Probe of tvtuner successful!\n");

	return ret;

err_media_entity:
	media_entity_cleanup(&sd->entity);

err_mem_free:
	devm_kfree(&pdev->dev, state);

err:
	return ret;
}

static int Tvtuner_remove(struct platform_device *pdev)
{
	struct Tvtuner_state *state = platform_get_drvdata(pdev);

	msm_ba_unregister_subdev_node(&state->sd);
	v4l2_device_unregister_subdev(&state->sd);
	media_entity_cleanup(&state->sd.entity);

	v4l2_ctrl_handler_free(&state->ctrl_hdl);

	mutex_destroy(&state->mutex);
	devm_kfree(&pdev->dev, state);

	return 0;
}

static int Tvtuner_suspend(struct device *dev)
{
	TUNER_DEBUG("tv_tuner driver is in suspend state\n");
	return 0;
}

static int Tvtuner_resume(struct device *dev)
{
	TUNER_DEBUG("tv_tuner driver is in resume state\n");
	return 0;
}

static SIMPLE_DEV_PM_OPS(Tvtuner_pm_ops, Tvtuner_suspend, Tvtuner_resume);
#define TVTUNER_PM_OPS (&Tvtuner_pm_ops)

static struct platform_driver Tvtuner_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "tv-tuner",
		.of_match_table = Tvtuner_id,
		.pm = TVTUNER_PM_OPS,
	},
	.probe = Tvtuner_probe,
	.remove = Tvtuner_remove,
};

module_driver(Tvtuner_driver, platform_driver_register,
		platform_driver_unregister);

MODULE_DESCRIPTION(" TV TUNER Test driver");
MODULE_LICENSE("GPL v2");
