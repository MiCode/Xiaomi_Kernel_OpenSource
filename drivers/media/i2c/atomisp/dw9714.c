/*
 * Support for DW9714 VCM.
 *
 * Copyright (c) 2015 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-device.h>
#include <asm/intel-mid.h>
#include <linux/acpi.h>
#include <linux/atomisp_gmin_platform.h>

#include "dw9714.h"

static int dw9714_i2c_write(struct i2c_client *client, u16 data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;
	u16 val;

	val = cpu_to_be16(data);
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = DW9714_16BIT;
	msg.buf = (u8 *)&val;

	ret = i2c_transfer(client->adapter, &msg, 1);
	return ret == num_msg ? 0 : -EIO;
}

static int dw9714_vcm_power_up(struct v4l2_subdev *sd,
			struct camera_vcm_control *vcm)
{
	int ret;
	struct dw9714_device *dev =
		container_of(vcm, struct dw9714_device, vcm_ctrl);

	ret = dev->platform_data->power_ctrl(sd, 1);

	if (dev->vcm_mode != DW9714_DIRECT)
		dev->vcm_settings.update = true;

	/* waiting time requested by DW9714A(vcm) */
	usleep_range(12000, 12500);
	return ret;
}

static int dw9714_vcm_power_down(struct v4l2_subdev *sd,
				struct camera_vcm_control *vcm)
{
	struct dw9714_device *dev =
		container_of(vcm, struct dw9714_device, vcm_ctrl);

	return dev->platform_data->power_ctrl(sd, 0);
}

static int dw9714_t_focus_vcm(struct v4l2_subdev *sd, u16 val,
				struct dw9714_device *dev)
{
	struct i2c_client *client = dev->client;
	int ret = -EINVAL;
	u8 s;

	val &= DW9714_MAX_FOCUS_POS;
	dev->vcm_settings.code = val;

	if (dev->set_vcm_mode) {
		ret = dev->set_vcm_mode(dev);
		if (ret)
			return ret;
	}

	switch (dev->vcm_mode) {
	case DW9714_DIRECT:
		ret = dw9714_i2c_write(client, vcm_val(val, VCM_DEFAULT_S));
		break;
	case DW9714_LSC:
		s = vcm_step_s(dev->vcm_settings.step_setting);
		ret = dw9714_i2c_write(client, vcm_val(val, s));
		break;
	case DW9714_DLC:
		ret = dw9714_i2c_write(client, vcm_val(val, VCM_DEFAULT_S));
		break;
	}

	return ret;
}

static int dw9714_set_vcm_mode(struct dw9714_device *dev)
{
	struct i2c_client *client = dev->client;
	int ret = -EINVAL;
	u8 mclk = vcm_step_mclk(dev->vcm_settings.step_setting);

	/*
	 * For different mode, VCM_PROTECTION_OFF/ON required by the
	 * control procedure. For DW9714_DIRECT/DLC mode, slew value is
	 * VCM_DEFAULT_S(0).
	 */
	switch (dev->vcm_mode) {
	case DW9714_DIRECT:
		ret = dw9714_i2c_write(client, VCM_PROTECTION_OFF);
		if (ret)
			return ret;
		ret = dw9714_i2c_write(client, DIRECT_VCM);
		if (ret)
			return ret;
		ret = dw9714_i2c_write(client, VCM_PROTECTION_ON);
		if (ret)
			return ret;
		break;
	case DW9714_LSC:
		ret = dw9714_i2c_write(client, VCM_PROTECTION_OFF);
		if (ret)
			return ret;
		ret = dw9714_i2c_write(client,
				vcm_dlc_mclk(DLC_DISABLE, mclk));
		if (ret)
			return ret;
		ret = dw9714_i2c_write(client,
				vcm_tsrc(dev->vcm_settings.t_src));
		if (ret)
			return ret;
		ret = dw9714_i2c_write(client, VCM_PROTECTION_ON);
		if (ret)
			return ret;
		break;
	case DW9714_DLC:
		ret = dw9714_i2c_write(client, VCM_PROTECTION_OFF);
		if (ret)
			return ret;
		ret = dw9714_i2c_write(client,
				vcm_dlc_mclk(DLC_ENABLE, mclk));
		if (ret)
			return ret;
		ret = dw9714_i2c_write(client,
				vcm_tsrc(dev->vcm_settings.t_src));
		if (ret)
			return ret;
		ret = dw9714_i2c_write(client, VCM_PROTECTION_ON);
		if (ret)
			return ret;
		break;
	}

	dev->vcm_settings.update = false;
	return ret;
}

static int dw9714_t_focus_abs(struct v4l2_subdev *sd, s32 value,
			struct dw9714_device *dev)
{
	int ret;

	value = min(value, DW9714_MAX_FOCUS_POS);
	ret = dw9714_t_focus_vcm(sd, DW9714_MAX_FOCUS_POS - value, dev);
	if (!ret) {
		dev->number_of_steps = value - dev->focus;
		dev->focus = value;
		getnstimeofday(&dev->timestamp_t_focus_abs);
	}

	return ret;
}

static int dw9714_t_focus_rel(struct v4l2_subdev *sd, s32 value,
			struct dw9714_device *dev)
{
	return dw9714_t_focus_abs(sd, dev->focus + value, dev);
}

static int dw9714_q_focus_status(struct v4l2_subdev *sd, s32 *value,
			struct dw9714_device *dev)
{
	u32 status = 0;
	struct timespec temptime;
	const struct timespec timedelay = {
		0,
		min_t(u32, abs(dev->number_of_steps)*DELAY_PER_STEP_NS,
			DELAY_MAX_PER_STEP_NS),
	};

	ktime_get_ts(&temptime);

	temptime = timespec_sub(temptime, (dev->timestamp_t_focus_abs));

	if (timespec_compare(&temptime, &timedelay) <= 0) {
		status |= ATOMISP_FOCUS_STATUS_MOVING;
		status |= ATOMISP_FOCUS_HP_IN_PROGRESS;
	} else {
		status |= ATOMISP_FOCUS_STATUS_ACCEPTS_NEW_MOVE;
		status |= ATOMISP_FOCUS_HP_COMPLETE;
	}

	*value = status;
	return 0;
}

static int dw9714_q_focus_abs(struct v4l2_subdev *sd, s32 *value,
			struct dw9714_device *dev)
{
	s32 val;

	dw9714_q_focus_status(sd, &val, dev);
	if (val & ATOMISP_FOCUS_STATUS_MOVING)
		*value  = dev->focus - dev->number_of_steps;
	else
		*value  = dev->focus;

	return 0;
}

static int dw9714_t_vcm_slew(struct v4l2_subdev *sd, s32 value,
			struct dw9714_device *dev)
{
	dev->vcm_settings.step_setting = value;
	return 0;
}

static int dw9714_t_vcm_timing(struct v4l2_subdev *sd, s32 value,
			struct dw9714_device *dev)
{
	dev->vcm_settings.t_src = value;
	return 0;
}

struct dw9714_control dw9714_controls[] = {
	{
		.qc = {
			.id = V4L2_CID_FOCUS_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move absolute",
			.minimum = 0,
			.maximum = DW9714_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = dw9714_t_focus_abs,
		.query = dw9714_q_focus_abs,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_RELATIVE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move relative",
			.minimum = DW9714_MAX_FOCUS_NEG,
			.maximum = DW9714_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = dw9714_t_focus_rel,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_STATUS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus status",
			.minimum = 0,
			.maximum = 100, /* allow enum to grow in the future */
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = dw9714_q_focus_status,
	},
	{
		.qc = {
			.id = V4L2_CID_VCM_SLEW,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vcm slew",
			.minimum = 0,
			.maximum = DW9714_SLEW_STEP_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = dw9714_t_vcm_slew,
	},
	{
		.qc = {
			.id = V4L2_CID_VCM_TIMEING,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vcm step time",
			.minimum = 0,
			.maximum = DW9714_SLEW_TIME_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = dw9714_t_vcm_timing,
	},
};
#define N_CONTROLS (ARRAY_SIZE(dw9714_controls))

static struct dw9714_control *dw9714_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (dw9714_controls[i].qc.id == id)
			return &dw9714_controls[i];
	return NULL;
}

static int dw9714_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc,
				struct camera_vcm_control *vcm)
{
	struct dw9714_control *ctrl = dw9714_find_control(qc->id);
	struct dw9714_device *dev =
		container_of(vcm, struct dw9714_device, vcm_ctrl);

	if (!ctrl)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int dw9714_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl,
				struct camera_vcm_control *vcm)
{
	struct dw9714_control *s_ctrl;
	struct dw9714_device *dev =
		container_of(vcm, struct dw9714_device, vcm_ctrl);
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = dw9714_find_control(ctrl->id);
	if (!s_ctrl || !s_ctrl->query)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value, dev);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int dw9714_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl,
				struct camera_vcm_control *vcm)
{
	struct dw9714_control *octrl = dw9714_find_control(ctrl->id);
	struct dw9714_device *dev =
		container_of(vcm, struct dw9714_device, vcm_ctrl);
	int ret;

	if (!octrl || !octrl->tweak)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value, dev);
	mutex_unlock(&dev->input_lock);

	return ret;
}

struct camera_vcm_ops dw9714_vcm_ops = {
	.power_up = dw9714_vcm_power_up,
	.power_down = dw9714_vcm_power_down,
	.queryctrl = dw9714_queryctrl,
	.g_ctrl = dw9714_g_ctrl,
	.s_ctrl = dw9714_s_ctrl,
};

static const struct dw9714_chip_features chips[] = {
	{
		.name = "FMDW9714",
		.supported_modes = DW9714_DIRECT | DW9714_LSC | DW9714_DLC,
		.set_vcm_mode = &dw9714_set_vcm_mode,
	},
	{
		.name = "XXVM149C",
		.supported_modes = DW9714_DIRECT,
	},
};

static const struct dw9714_chip_features *
dw9714_detect(struct i2c_client *client, struct dw9714_device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(chips); i++) {
		if (!strncmp(chips[i].name, client->name,
				strlen(chips[i].name))) {
			dev_info(&client->dev, "dw9714: Found %s\n",
				chips[i].name);
			return &chips[i];
		}
	}

	dev_err(&client->dev, "dw9714: Found non-supported chip (%s)\n",
		client->name);
	return NULL;
}

static void dw9714_verify_and_set_mode(struct dw9714_device *dev,
				const struct dw9714_chip_features *chip)
{
	switch (dev->vcm_mode) {
	case DW9714_DIRECT:
	case DW9714_LSC:
	case DW9714_DLC:
		if (dev->vcm_mode & chip->supported_modes)
			break;

		/* Fall back to default setting */
	default:
		dev->vcm_mode = DW9714_DIRECT;
	}

	dev->set_vcm_mode = chip->set_vcm_mode;
}

static int dw9714_vcm_init(struct i2c_client *client, struct dw9714_device *dev)
{
	const struct dw9714_chip_features *chip;

	chip = dw9714_detect(client, dev);
	if (!chip)
		return -ENODEV;

	dw9714_verify_and_set_mode(dev, chip);
	dev->vcm_ctrl.ops = &dw9714_vcm_ops;
	dev->platform_data = camera_get_af_platform_data();

	return dev->platform_data ? 0 : -ENODEV;
}

static int dw9714_remove(struct i2c_client *client)
{
	return 0;
}

static int dw9714_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct dw9714_device *dev;
	int ret;
	size_t len = sizeof(dev->vcm_ctrl.camera_module);

	dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	if (gmin_get_config_var(&client->dev, "CameraModule",
				dev->vcm_ctrl.camera_module, &len)) {
		dev_err(&client->dev,
			"VCM device is not bound to any camera sensor\n");
		return -EINVAL;
	}

	/* If no mode is found, use Direct Mode as default */
	dev->vcm_mode = gmin_get_var_int(&client->dev, "VcmMode",
					DW9714_DIRECT);

	mutex_init(&dev->input_lock);
	dev->client = client;

	if (dw9714_vcm_init(client, dev))
		return -ENODEV;

	ret = atomisp_gmin_register_vcm_control(&dev->vcm_ctrl);
	if (ret)
		pr_err("register vcm platform data failed\n");

	return ret;
}

static const struct i2c_device_id dw9714_id[] = {
	{DW9714_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, dw9714_id);

static struct acpi_device_id dw9714_acpi_id[] = {
	{"FMDW9714"},
	{"XXVM149C"},
	{},
};
MODULE_DEVICE_TABLE(acpi, dw9714_acpi_id);

static struct i2c_driver dw9714_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DW9714_NAME,
		.acpi_match_table = ACPI_PTR(dw9714_acpi_id),
	},
	.probe = dw9714_probe,
	.remove = dw9714_remove,
	.id_table = dw9714_id,
};

module_i2c_driver(dw9714_driver);

MODULE_DESCRIPTION("VCM Driver IC dw9714");
MODULE_LICENSE("GPL");

