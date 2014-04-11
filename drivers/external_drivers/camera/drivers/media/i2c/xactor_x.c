/*
 * Support for mipi CSI data generator.
 *
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
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

#include <linux/atomisp_platform.h>
#include <linux/delay.h>
#include <linux/module.h>

#include "xactor_x.h"

#define MODE_DEFAULT 1
#define MODE_PIXTER 1
#define MODE_XACTOR 2
unsigned int mode = MODE_DEFAULT;
module_param(mode, uint, 0644);
MODULE_PARM_DESC(mode,
		"Control the mode how xactor driver operates default = 1. (1=Pixter, 2=SLE csi xactor)");

#define to_csi_xactor_dev(sd) container_of(sd, struct csi_xactor_device, sd)

static int csi_xactor_s_config(struct v4l2_subdev *sd,
			    int irq, void *pdata)
{
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (pdata == NULL)
		return -ENODEV;

	dev->platform_data = pdata;

	mutex_lock(&dev->input_lock);

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	mutex_unlock(&dev->input_lock);

	return 0;

fail_csi_cfg:
	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int csi_xactor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);

	media_entity_cleanup(&dev->sd.entity);
	v4l2_ctrl_handler_free(&dev->ctrl_handler);
	dev->platform_data->csi_cfg(sd, 0);
	v4l2_device_unregister_subdev(sd);
	kfree(dev);

	return 0;
}

static enum xactor_contexts xactor_get_context(struct v4l2_subdev *sd)
{
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	return dev->cur_context;
}

static int csi_xactor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (mode == MODE_XACTOR) {
		/* This is the CSI xactor test code for SLE */
		void __iomem *base;
		unsigned int xactor_on;

#define CSI_XACTOR_UNPAUSE_REG_ADDR    0x100000
		/* lane can only be 4, 1, 2*/
#define CSI_XACTOR_PAUSE(lane, pause_en) \
		(0xc5180000 | (lane<<16) | (((pause_en) ? 7 : 0)<<8))

		if (strcmp(client->name, CSI_XACTOR_A_NAME) == 0) {
			dev_dbg(&client->dev, "set stream on to %d port a\n", enable);
			xactor_on = CSI_XACTOR_PAUSE(1, !enable);
		} else if (strcmp(client->name, CSI_XACTOR_B_NAME) == 0) {
			dev_dbg(&client->dev, "set stream on to %d port b\n", enable);
			xactor_on = CSI_XACTOR_PAUSE(2, !enable);
		} else if (strcmp(client->name, CSI_XACTOR_C_NAME) == 0) {
			dev_dbg(&client->dev, "set stream on to %d port c\n", enable);
			xactor_on = CSI_XACTOR_PAUSE(4, !enable);
		} else {
			dev_err(&client->dev, "xactor driver doesn't match!\n");
			return -EINVAL;
		}

		base = phys_to_virt(CSI_XACTOR_UNPAUSE_REG_ADDR);
		if (!base) {
			dev_dbg(&client->dev, "Failed to phys_to_virt(CSI_XACTOR_UNPAUSE_REG_ADDR)\n");
			return -EINVAL;
		}

		if (enable) {
			dev_dbg(&client->dev, "waiting  for sensor to start sending data\n");
			usleep_range(40000000, 55000000);
		}

		/* only for SLE to stop MIPI xactor */
		dev_dbg(&client->dev, "%s_stream: 0x%x\n", __func__, xactor_on);
		*(s32 __force *)((unsigned int)base) = xactor_on;

		dev_dbg(&client->dev, "vir: 0x%x, phy: 0x%x, data: 0x%x\n",
				(unsigned int)base,
				(unsigned int)virt_to_phys(base),
				*(s32 __force *)((unsigned int)base));
	} else {
		dev_dbg(&client->dev, "stream on for pixter\n");
	}

	return 0;
}

static int get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info)
{
	struct atomisp_sensor_mode_data *buf = &info->data;

	if (info == NULL)
		return -EINVAL;

	/* Dummy values for tuning. */
	buf->crop_horizontal_start = 0;
	buf->crop_vertical_start = 0;

	buf->crop_horizontal_end = 1000;

	buf->crop_vertical_end = 1000;

	buf->output_width = 1000;

	buf->output_height = 1000;

	buf->vt_pix_clk_freq_mhz = 19000000;
	buf->coarse_integration_time_min = 0;
	buf->coarse_integration_time_max_margin = 0;

	buf->fine_integration_time_min = 1;
	buf->fine_integration_time_max_margin = 1000;
	buf->fine_integration_time_def = 1;
	buf->frame_length_lines = 100000;
	buf->line_length_pck = 10000;
	buf->read_mode = 0;

	buf->binning_factor_x = 1;
	buf->binning_factor_y = 1;

	return 0;
}

static int csi_xactor_g_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *interval)
{
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *info = v4l2_get_subdev_hostdata(sd);
	if (info == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);

	/* Return the currently selected settings' maximum frame interval */

	get_intg_factor(client, info);
	interval->interval.numerator = 1;
	interval->interval.denominator = 15;

	mutex_unlock(&dev->input_lock);

	return 0;
}

static int csi_xactor_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	*code = dev->cntx_config[0].mbus_fmt;
	return 0;
}

static int csi_xactor_try_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	return 0;
}

static int csi_xactor_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	struct atomisp_input_stream_info *stream_info =
		(struct atomisp_input_stream_info*)fmt->reserved;

	if (!fmt)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fmt->width = dev->cntx_config[stream_info->stream].width;
	fmt->height = dev->cntx_config[stream_info->stream].height;
	fmt->code = dev->cntx_config[stream_info->stream].mbus_fmt;
	mutex_unlock(&dev->input_lock);

	dev_dbg(&client->dev, "%s w:%d h:%d code: 0x%x stream: %d\n", __func__,
			fmt->width, fmt->height, fmt->code,
			stream_info->stream);

	return 0;
}

static enum xactor_contexts xactor_cntx_mapping[] = {
	CONTEXT_PREVIEW,	/* Invalid atomisp run mode */
	CONTEXT_VIDEO,		/* ATOMISP_RUN_MODE_VIDEO */
	CONTEXT_SNAPSHOT,	/* ATOMISP_RUN_MODE_STILL_CAPTURE */
	CONTEXT_SNAPSHOT,	/* ATOMISP_RUN_MODE_CONTINUOUS_CAPTURE */
	CONTEXT_PREVIEW,	/* ATOMISP_RUN_MODE_PREVIEW */
};

static enum xactor_contexts stream_to_context[] = {
	CONTEXT_SNAPSHOT,
	CONTEXT_PREVIEW,
	CONTEXT_PREVIEW,
	CONTEXT_VIDEO
};

static int csi_xactor_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *info = v4l2_get_subdev_hostdata(sd);
	struct atomisp_input_stream_info *stream_info =
		(struct atomisp_input_stream_info*)fmt->reserved;

	if (!fmt)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	dev->cntx_config[stream_info->stream].width = fmt->width;
	dev->cntx_config[stream_info->stream].height = fmt->height;
	dev->cntx_config[stream_info->stream].mbus_fmt = fmt->code;
	get_intg_factor(client, info);
	stream_info->ch_id = stream_to_context[stream_info->stream];
	mutex_unlock(&dev->input_lock);
	dev_dbg(&client->dev, "%s w:%d h:%d code: 0x%x stream: %d\n", __func__,
			fmt->width, fmt->height, fmt->code,
			stream_info->stream);

	return 0;
}

static int csi_xactor_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static long csi_xactor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return 0;
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

static int
csi_xactor_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_mbus_code_enum *code)
{
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	if (code->index >= 1)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	code->code = dev->format.code;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int
csi_xactor_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	enum xactor_contexts context = xactor_get_context(sd);

	mutex_lock(&dev->input_lock);
	if (index >= 1) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fse->min_width = dev->cntx_config[context].width;
	fse->min_height = dev->cntx_config[context].height;
	fse->max_width = dev->cntx_config[context].width;
	fse->max_height = dev->cntx_config[context].height;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static struct v4l2_mbus_framefmt *
__csi_xactor_get_pad_format(struct csi_xactor_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &sensor->format;
	default:
		return NULL;
	}
}

static int
csi_xactor_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	return 0;
}

static int
csi_xactor_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	struct v4l2_mbus_framefmt *format =
			__csi_xactor_get_pad_format(dev, fh, fmt->pad, fmt->which);

	fmt->format = *format;

	return 0;
}

static int csi_xactor_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	enum xactor_contexts context = xactor_get_context(sd);

	mutex_lock(&dev->input_lock);
	if (index >= 1) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = dev->cntx_config[context].width;
	fsize->discrete.height = dev->cntx_config[context].height;
	fsize->reserved[0] = 1;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int csi_xactor_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	struct csi_xactor_device *dev = to_csi_xactor_dev(sd);
	enum xactor_contexts context = xactor_get_context(sd);

	mutex_lock(&dev->input_lock);
	/* since the isp will donwscale the resolution to the right size,
	  * find the nearest one that will allow the isp to do so
	  * important to ensure that the resolution requested is padded
	  * correctly by the requester, which is the atomisp driver in
	  * this case.
	  */

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = dev->cntx_config[context].width;
	fival->height = dev->cntx_config[context].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = 15;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_subdev_core_ops xactor_core_ops = {
	.queryctrl = v4l2_subdev_queryctrl,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.s_power = csi_xactor_s_power,
	.ioctl = csi_xactor_ioctl,
};

static const struct v4l2_subdev_video_ops xactor_video_ops = {
	.s_stream = csi_xactor_s_stream,
	.enum_framesizes = csi_xactor_enum_framesizes,
	.enum_frameintervals = csi_xactor_enum_frameintervals,
	.enum_mbus_fmt = csi_xactor_enum_mbus_fmt,
	.try_mbus_fmt = csi_xactor_try_mbus_fmt,
	.g_mbus_fmt = csi_xactor_g_mbus_fmt,
	.s_mbus_fmt = csi_xactor_s_mbus_fmt,
	.g_frame_interval = csi_xactor_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops csi_xactor_pad_ops = {
	.enum_mbus_code = csi_xactor_enum_mbus_code,
	.enum_frame_size = csi_xactor_enum_frame_size,
	.get_fmt = csi_xactor_get_pad_format,
	.set_fmt = csi_xactor_set_pad_format,
};

static const struct v4l2_subdev_ops xactor_ops = {
	.core = &xactor_core_ops,
	.video = &xactor_video_ops,
	.pad = &csi_xactor_pad_ops,
};

static int csi_xactor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct csi_xactor_device *dev = container_of(
		ctrl->handler, struct csi_xactor_device, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_RUN_MODE:
		dev->cur_context = xactor_cntx_mapping[ctrl->val];
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = csi_xactor_s_ctrl,
};

static const char * const ctrl_run_mode_menu[] = {
	NULL,
	"Video",
	"Still capture",
	"Continuous capture",
	"Preview",
};

static const struct v4l2_ctrl_config ctrl_run_mode = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_RUN_MODE,
	.name = "run mode",
	.type = V4L2_CTRL_TYPE_MENU,
	.min = 1,
	.def = 4,
	.max = 4,
	.qmenu = ctrl_run_mode_menu,
};

static const struct media_entity_operations csi_xactor_entity_ops = {
	.link_setup = NULL,
};

static int csi_xactor_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct csi_xactor_device *dev;
	struct camera_mipi_info *csi;
	char name[2] = {0};
	int ret;


	/* allocate sensor device & init sub device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	mutex_init(&dev->input_lock);

	v4l2_i2c_subdev_init(&dev->sd, client, &xactor_ops);

	if (client->dev.platform_data) {
		ret = csi_xactor_s_config(&dev->sd, client->irq,
				      client->dev.platform_data);
		if (ret)
			goto out_free;
	}

	switch(mode) {
	case MODE_PIXTER:
		dev_info(&client->dev, "Driver in Pixter mode\n");
		break;
	case MODE_XACTOR:
		dev_info(&client->dev, "Driver in SLE CSI xactor mode\n");
		break;
	default:
		dev_err(&client->dev, "Mode %d is not supported setting to default mode.\n",
				mode);
		mode = MODE_DEFAULT;
		break;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	dev->format.code = V4L2_MBUS_FMT_SBGGR10_1X10;

	ret = v4l2_ctrl_handler_init(&dev->ctrl_handler, 1);
	if (ret) {
		csi_xactor_remove(client);
		return ret;
	}

	dev->run_mode = v4l2_ctrl_new_custom(&dev->ctrl_handler,
					     &ctrl_run_mode, NULL);
	if (dev->ctrl_handler.error) {
		csi_xactor_remove(client);
		return dev->ctrl_handler.error;
	}

	/* Use same lock for controls as for everything else. */
	dev->ctrl_handler.lock = &dev->input_lock;
	dev->sd.ctrl_handler = &dev->ctrl_handler;
	v4l2_ctrl_handler_setup(&dev->ctrl_handler);

	/*
	 * sd->name is updated with sensor driver name by the v4l2.
	 * change it to sensor name in this case.
	 */
	csi = v4l2_get_subdev_hostdata(&dev->sd);

	if (csi->port == ATOMISP_CAMERA_PORT_PRIMARY)
	    name[0] = 'a';
	else if(csi->port == ATOMISP_CAMERA_PORT_SECONDARY)
	    name[0] = 'b';
	else
	    name[0] = 'c';

	snprintf(dev->sd.name, sizeof(dev->sd.name), "%s%s %d-%04x",
		"xactor", name,
		i2c_adapter_id(client->adapter), client->addr);

        dev_info(&client->dev, "%s dev->sd.name: %s\n", __func__, dev->sd.name);

	dev->sd.entity.ops = &csi_xactor_entity_ops;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		csi_xactor_remove(client);
		return ret;
	}

	return 0;

out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

static const struct i2c_device_id csi_xactor_ids[] = {
	{CSI_XACTOR_A_NAME, 0},
	{CSI_XACTOR_B_NAME, 0},
	{CSI_XACTOR_C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, csi_xactor_ids);

static struct i2c_driver csi_xactor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = CSI_XACTOR_NAME,
	},
	.probe = csi_xactor_probe,
	.remove = csi_xactor_remove,
	.id_table = csi_xactor_ids,
};

static __init int csi_xactor_init_mod(void)
{
	int r = i2c_add_driver(&csi_xactor_driver);

	return r;
}

static __exit void csi_xactor_exit_mod(void)
{
	i2c_del_driver(&csi_xactor_driver);
}

module_init(csi_xactor_init_mod);
module_exit(csi_xactor_exit_mod);

MODULE_DESCRIPTION("A dummy sensor driver for csi data generators");
MODULE_AUTHOR("Jukka Kaartinen <jukka.o.kaartinen@intel.com>");
MODULE_LICENSE("GPL");
