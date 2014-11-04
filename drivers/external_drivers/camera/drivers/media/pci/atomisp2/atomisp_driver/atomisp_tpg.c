/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <media/v4l2-event.h>
#include <media/v4l2-mediabus.h>
#include "atomisp_internal.h"
#include "atomisp_tpg.h"

static int tpg_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int tpg_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	/*to fake*/
	return 0;
}

static int tpg_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	/*to fake*/
	return 0;
}

static int tpg_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	/*to fake*/
	return 0;
}

static int tpg_enum_frameintervals(struct v4l2_subdev *sd,
				      struct v4l2_frmivalenum *fival)
{
	/*to fake*/
	return 0;
}

static int tpg_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				enum v4l2_mbus_pixelcode *code)
{
	/*to fake*/
	return 0;
}

static int tpg_try_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	/* only raw8 grbg is supported by TPG */
	fmt->code = V4L2_MBUS_FMT_SGRBG8_1X8;
	return 0;
}

static int tpg_g_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	/*to fake*/
	return 0;
}

static int tpg_s_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	/* only raw8 grbg is supported by TPG */
	fmt->code = V4L2_MBUS_FMT_SGRBG8_1X8;
	return 0;
}

#ifndef CONFIG_GMIN_INTEL_MID
static int tpg_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *chip)
{
	if (!chip)
		return -EINVAL;
	return 0;
}
#endif
static int tpg_log_status(struct v4l2_subdev *sd)
{
	/*to fake*/
	return 0;
}

static int tpg_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	/*to fake*/
	return -EINVAL;
}

static int tpg_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	/*to fake*/
	return -EINVAL;
}

static int tpg_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	/*to fake*/
	return 0;
}

static int tpg_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static int tpg_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	/*to fake*/
	return 0;
}

static int tpg_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	/*to fake*/
	return 0;
}

static int tpg_enum_frame_ival(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_frame_interval_enum *fie)
{
	/*to fake*/
	return 0;
}

static const struct v4l2_subdev_video_ops tpg_video_ops = {
	.s_stream = tpg_s_stream,
	.g_parm = tpg_g_parm,
	.s_parm = tpg_s_parm,
	.enum_framesizes = tpg_enum_framesizes,
	.enum_frameintervals = tpg_enum_frameintervals,
	.enum_mbus_fmt = tpg_enum_mbus_fmt,
	.try_mbus_fmt = tpg_try_mbus_fmt,
	.g_mbus_fmt = tpg_g_mbus_fmt,
	.s_mbus_fmt = tpg_s_mbus_fmt,
};

static const struct v4l2_subdev_core_ops tpg_core_ops = {
#ifndef CONFIG_GMIN_INTEL_MID
	.g_chip_ident = tpg_g_chip_ident,
#endif
	.log_status = tpg_log_status,
	.queryctrl = tpg_queryctrl,
	.g_ctrl = tpg_g_ctrl,
	.s_ctrl = tpg_s_ctrl,
	.s_power = tpg_s_power,
};

static const struct v4l2_subdev_pad_ops tpg_pad_ops = {
	.enum_mbus_code = tpg_enum_mbus_code,
	.enum_frame_size = tpg_enum_frame_size,
	.enum_frame_interval = tpg_enum_frame_ival,
};

static const struct v4l2_subdev_ops tpg_ops = {
	.core = &tpg_core_ops,
	.video = &tpg_video_ops,
	.pad = &tpg_pad_ops,
};

void atomisp_tpg_unregister_entities(struct atomisp_tpg_device *tpg)
{
	media_entity_cleanup(&tpg->sd.entity);
	v4l2_device_unregister_subdev(&tpg->sd);
}

int atomisp_tpg_register_entities(struct atomisp_tpg_device *tpg,
			struct v4l2_device *vdev)
{
	int ret;
	/* Register the subdev and video nodes. */
	ret = v4l2_device_register_subdev(vdev, &tpg->sd);
	if (ret < 0)
		goto error;

	return 0;

error:
	atomisp_tpg_unregister_entities(tpg);
	return ret;
}

void atomisp_tpg_cleanup(struct atomisp_device *isp)
{

}

int atomisp_tpg_init(struct atomisp_device *isp)
{
	struct atomisp_tpg_device *tpg = &isp->tpg;
	struct v4l2_subdev *sd = &tpg->sd;
	struct media_pad *pads = tpg->pads;
	struct media_entity *me = &sd->entity;
	int ret;

	tpg->isp = isp;
	v4l2_subdev_init(sd, &tpg_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	strcpy(sd->name, "tpg_subdev");
	v4l2_set_subdevdata(sd, tpg);

	pads[0].flags = MEDIA_PAD_FL_SINK;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV;

	ret = media_entity_init(me, 1, pads, 0);
	if (ret < 0)
		goto fail;
	return 0;
fail:
	atomisp_tpg_cleanup(isp);
	return ret;
}
