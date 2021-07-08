// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define DRIVER_NAME "gt9772"
#define GT9772_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define GT9772_NAME				"gt9772"
#define GT9772_MAX_FOCUS_POS			1023
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define GT9772_FOCUS_STEPS			1
#define GT9772_SET_POSITION_ADDR		0x03

#define GT9772_CMD_DELAY			0xff
#define GT9772_CTRL_DELAY_US			5000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define GT9772_MOVE_STEPS			16
#define GT9772_MOVE_DELAY_US			8400
#define GT9772_STABLE_TIME_US			20000

/* gt9772 device structure */
struct gt9772_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

static inline struct gt9772_device *to_gt9772_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct gt9772_device, ctrls);
}

static inline struct gt9772_device *sd_to_gt9772_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct gt9772_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int gt9772_set_position(struct gt9772_device *gt9772, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&gt9772->sd);

	return i2c_smbus_write_word_data(client, GT9772_SET_POSITION_ADDR,
					 swab16(val));
}

static int gt9772_release(struct gt9772_device *gt9772)
{
	int ret, val;

	for (val = round_down(gt9772->focus->val, GT9772_MOVE_STEPS);
	     val >= 0; val -= GT9772_MOVE_STEPS) {
		ret = gt9772_set_position(gt9772, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(GT9772_MOVE_DELAY_US,
			     GT9772_MOVE_DELAY_US + 1000);
	}

	/*
	 * Wait for the motor to stabilize after the last movement
	 * to prevent the motor from shaking.
	 */
	usleep_range(GT9772_STABLE_TIME_US - GT9772_MOVE_DELAY_US,
		     GT9772_STABLE_TIME_US - GT9772_MOVE_DELAY_US + 1000);

	return 0;
}

static int gt9772_init(struct gt9772_device *gt9772)
{
	struct i2c_client *client = v4l2_get_subdevdata(&gt9772->sd);
	int ret;

	LOG_INF("+\n");

	client->addr  = GT9772_I2C_SLAVE_ADDR >> 1;
	ret = i2c_smbus_read_byte_data(client, 0x00);

	LOG_INF("Check HW version: %x\n", ret);

	ret = i2c_smbus_write_byte_data(client, 0xed, 0xab);

	LOG_INF("-\n");

	return 0;
}

/* Power handling */
static int gt9772_power_off(struct gt9772_device *gt9772)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = gt9772_release(gt9772);
	if (ret)
		LOG_INF("gt9772 release failed!\n");

	ret = regulator_disable(gt9772->vin);
	if (ret)
		return ret;

	ret = regulator_disable(gt9772->vdd);
	if (ret)
		return ret;

	if (gt9772->vcamaf_pinctrl && gt9772->vcamaf_off)
		ret = pinctrl_select_state(gt9772->vcamaf_pinctrl,
					gt9772->vcamaf_off);

	return ret;
}

static int gt9772_power_on(struct gt9772_device *gt9772)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(gt9772->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(gt9772->vdd);
	if (ret < 0)
		return ret;

	if (gt9772->vcamaf_pinctrl && gt9772->vcamaf_on)
		ret = pinctrl_select_state(gt9772->vcamaf_pinctrl,
					gt9772->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(GT9772_CTRL_DELAY_US, GT9772_CTRL_DELAY_US + 100);

	ret = gt9772_init(gt9772);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(gt9772->vin);
	regulator_disable(gt9772->vdd);
	if (gt9772->vcamaf_pinctrl && gt9772->vcamaf_off) {
		pinctrl_select_state(gt9772->vcamaf_pinctrl,
				gt9772->vcamaf_off);
	}

	return ret;
}

static int gt9772_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gt9772_device *gt9772 = to_gt9772_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		return gt9772_set_position(gt9772, ctrl->val);
	}
	return 0;
}

static const struct v4l2_ctrl_ops gt9772_vcm_ctrl_ops = {
	.s_ctrl = gt9772_set_ctrl,
};

static int gt9772_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = pm_runtime_get_sync(sd->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(sd->dev);
		return ret;
	}

	return 0;
}

static int gt9772_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	LOG_INF("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops gt9772_int_ops = {
	.open = gt9772_open,
	.close = gt9772_close,
};

static const struct v4l2_subdev_ops gt9772_ops = { };

static void gt9772_subdev_cleanup(struct gt9772_device *gt9772)
{
	v4l2_async_unregister_subdev(&gt9772->sd);
	v4l2_ctrl_handler_free(&gt9772->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&gt9772->sd.entity);
#endif
}

static int gt9772_init_controls(struct gt9772_device *gt9772)
{
	struct v4l2_ctrl_handler *hdl = &gt9772->ctrls;
	const struct v4l2_ctrl_ops *ops = &gt9772_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	gt9772->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, GT9772_MAX_FOCUS_POS, GT9772_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	gt9772->sd.ctrl_handler = hdl;

	return 0;
}

static int gt9772_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct gt9772_device *gt9772;
	int ret;

	LOG_INF("%s\n", __func__);

	gt9772 = devm_kzalloc(dev, sizeof(*gt9772), GFP_KERNEL);
	if (!gt9772)
		return -ENOMEM;

	gt9772->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(gt9772->vin)) {
		ret = PTR_ERR(gt9772->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	gt9772->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(gt9772->vdd)) {
		ret = PTR_ERR(gt9772->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	gt9772->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(gt9772->vcamaf_pinctrl)) {
		ret = PTR_ERR(gt9772->vcamaf_pinctrl);
		gt9772->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		gt9772->vcamaf_on = pinctrl_lookup_state(
			gt9772->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(gt9772->vcamaf_on)) {
			ret = PTR_ERR(gt9772->vcamaf_on);
			gt9772->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		gt9772->vcamaf_off = pinctrl_lookup_state(
			gt9772->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(gt9772->vcamaf_off)) {
			ret = PTR_ERR(gt9772->vcamaf_off);
			gt9772->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&gt9772->sd, client, &gt9772_ops);
	gt9772->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	gt9772->sd.internal_ops = &gt9772_int_ops;

	ret = gt9772_init_controls(gt9772);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&gt9772->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	gt9772->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&gt9772->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	gt9772_subdev_cleanup(gt9772);
	return ret;
}

static int gt9772_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gt9772_device *gt9772 = sd_to_gt9772_vcm(sd);

	LOG_INF("%s\n", __func__);

	gt9772_subdev_cleanup(gt9772);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		gt9772_power_off(gt9772);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int __maybe_unused gt9772_vcm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gt9772_device *gt9772 = sd_to_gt9772_vcm(sd);

	return gt9772_power_off(gt9772);
}

static int __maybe_unused gt9772_vcm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gt9772_device *gt9772 = sd_to_gt9772_vcm(sd);

	return gt9772_power_on(gt9772);
}

static const struct i2c_device_id gt9772_id_table[] = {
	{ GT9772_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, gt9772_id_table);

static const struct of_device_id gt9772_of_table[] = {
	{ .compatible = "mediatek,gt9772" },
	{ },
};
MODULE_DEVICE_TABLE(of, gt9772_of_table);

static const struct dev_pm_ops gt9772_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(gt9772_vcm_suspend, gt9772_vcm_resume, NULL)
};

static struct i2c_driver gt9772_i2c_driver = {
	.driver = {
		.name = GT9772_NAME,
		.pm = &gt9772_pm_ops,
		.of_match_table = gt9772_of_table,
	},
	.probe_new  = gt9772_probe,
	.remove = gt9772_remove,
	.id_table = gt9772_id_table,
};

module_i2c_driver(gt9772_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("GT9772 VCM driver");
MODULE_LICENSE("GPL v2");
