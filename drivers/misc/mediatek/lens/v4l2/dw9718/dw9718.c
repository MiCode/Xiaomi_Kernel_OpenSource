// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define DW9718_NAME				"dw9718"
#define DW9718_MAX_FOCUS_POS			1023
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define DW9718_FOCUS_STEPS			1
#define DW9718_CONTROL_REG			0x00
#define DW9718_SET_POSITION_ADDR		0x02
#define DW9718_CONTROL_POWER_DOWN		BIT(0)
#define DW9718_AAC_MODE_EN			BIT(1)

#define DW9718_CMD_DELAY			0xff
#define DW9718_CTRL_DELAY_US			5000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define DW9718_MOVE_STEPS			16
#define DW9718_MOVE_DELAY_US			8400
#define DW9718_STABLE_TIME_US			20000

/* dw9718 device structure */
struct dw9718_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
};

static inline struct dw9718_device *to_dw9718_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct dw9718_device, ctrls);
}

static inline struct dw9718_device *sd_to_dw9718_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct dw9718_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list dw9718_init_regs[] = {
	{DW9718_CMD_DELAY, DW9718_CMD_DELAY},
	{DW9718_CMD_DELAY, DW9718_CMD_DELAY},
};

static int dw9718_write_smbus(struct dw9718_device *dw9718, unsigned char reg,
			      unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9718->sd);
	int ret = 0;

	if (reg == DW9718_CMD_DELAY  && value == DW9718_CMD_DELAY)
		usleep_range(DW9718_CTRL_DELAY_US,
			     DW9718_CTRL_DELAY_US + 100);
	else
		ret = i2c_smbus_write_byte_data(client, reg, value);
	return ret;
}

static int dw9718_write_array(struct dw9718_device *dw9718,
			      struct regval_list *vals, u32 len)
{
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = dw9718_write_smbus(dw9718, vals[i].reg_num,
					 vals[i].value);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int dw9718_set_position(struct dw9718_device *dw9718, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9718->sd);

	return i2c_smbus_write_word_data(client, DW9718_SET_POSITION_ADDR,
					 swab16(val));
}

static int dw9718_release(struct dw9718_device *dw9718)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9718->sd);
	int ret, val;

	for (val = round_down(dw9718->focus->val, DW9718_MOVE_STEPS);
	     val >= 0; val -= DW9718_MOVE_STEPS) {
		ret = dw9718_set_position(dw9718, val);
		if (ret) {
			pr_info("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(DW9718_MOVE_DELAY_US,
			     DW9718_MOVE_DELAY_US + 1000);
	}

	/*
	 * Wait for the motor to stabilize after the last movement
	 * to prevent the motor from shaking.
	 */
	usleep_range(DW9718_STABLE_TIME_US - DW9718_MOVE_DELAY_US,
		     DW9718_STABLE_TIME_US - DW9718_MOVE_DELAY_US + 1000);

	ret = i2c_smbus_write_byte_data(client, DW9718_CONTROL_REG,
					DW9718_CONTROL_POWER_DOWN);
	if (ret)
		return ret;

	usleep_range(DW9718_CTRL_DELAY_US, DW9718_CTRL_DELAY_US + 100);

	return 0;
}

static int dw9718_init(struct dw9718_device *dw9718)
{
	int ret, val;

	pr_info("%s\n", __func__);

	ret = dw9718_write_array(dw9718, dw9718_init_regs,
				 ARRAY_SIZE(dw9718_init_regs));
	if (ret)
		return ret;

	for (val = dw9718->focus->val % DW9718_MOVE_STEPS;
	     val <= dw9718->focus->val;
	     val += DW9718_MOVE_STEPS) {
		ret = dw9718_set_position(dw9718, val);
		if (ret) {
			pr_info("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(DW9718_MOVE_DELAY_US,
			     DW9718_MOVE_DELAY_US + 1000);
	}

	return 0;
}

/* Power handling */
static int dw9718_power_off(struct dw9718_device *dw9718)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = dw9718_release(dw9718);
	if (ret)
		pr_info("dw9718 release failed!\n");

	ret = regulator_disable(dw9718->vin);
	if (ret)
		return ret;

	return regulator_disable(dw9718->vdd);
}

static int dw9718_power_on(struct dw9718_device *dw9718)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = regulator_enable(dw9718->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(dw9718->vdd);
	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(DW9718_CTRL_DELAY_US, DW9718_CTRL_DELAY_US + 100);

	ret = dw9718_init(dw9718);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(dw9718->vin);
	regulator_disable(dw9718->vdd);

	return ret;
}

static int dw9718_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct dw9718_device *dw9718 = to_dw9718_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE)
		return dw9718_set_position(dw9718, ctrl->val);

	return 0;
}

static const struct v4l2_ctrl_ops dw9718_vcm_ctrl_ops = {
	.s_ctrl = dw9718_set_ctrl,
};

static int dw9718_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = pm_runtime_get_sync(sd->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(sd->dev);
		return ret;
	}

	return 0;
}

static int dw9718_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pr_info("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops dw9718_int_ops = {
	.open = dw9718_open,
	.close = dw9718_close,
};

static const struct v4l2_subdev_ops dw9718_ops = { };

static void dw9718_subdev_cleanup(struct dw9718_device *dw9718)
{
	v4l2_async_unregister_subdev(&dw9718->sd);
	v4l2_ctrl_handler_free(&dw9718->ctrls);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&dw9718->sd.entity);
#endif
}

static int dw9718_init_controls(struct dw9718_device *dw9718)
{
	struct v4l2_ctrl_handler *hdl = &dw9718->ctrls;
	const struct v4l2_ctrl_ops *ops = &dw9718_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	dw9718->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, DW9718_MAX_FOCUS_POS, DW9718_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	dw9718->sd.ctrl_handler = hdl;

	return 0;
}

static int dw9718_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct dw9718_device *dw9718;
	int ret;

	pr_info("%s\n", __func__);

	dw9718 = devm_kzalloc(dev, sizeof(*dw9718), GFP_KERNEL);
	if (!dw9718)
		return -ENOMEM;

	dw9718->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(dw9718->vin)) {
		ret = PTR_ERR(dw9718->vin);
		if (ret != -EPROBE_DEFER)
			pr_info("cannot get vin regulator\n");
		return ret;
	}

	dw9718->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(dw9718->vdd)) {
		ret = PTR_ERR(dw9718->vdd);
		if (ret != -EPROBE_DEFER)
			pr_info("cannot get vdd regulator\n");
		return ret;
	}

	v4l2_i2c_subdev_init(&dw9718->sd, client, &dw9718_ops);
	dw9718->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dw9718->sd.internal_ops = &dw9718_int_ops;

	ret = dw9718_init_controls(dw9718);
	if (ret)
		goto err_cleanup;

#if defined(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&dw9718->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	dw9718->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&dw9718->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	dw9718_subdev_cleanup(dw9718);
	return ret;
}

static int dw9718_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9718_device *dw9718 = sd_to_dw9718_vcm(sd);

	pr_info("%s\n", __func__);

	dw9718_subdev_cleanup(dw9718);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		dw9718_power_off(dw9718);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int __maybe_unused dw9718_vcm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9718_device *dw9718 = sd_to_dw9718_vcm(sd);

	return dw9718_power_off(dw9718);
}

static int __maybe_unused dw9718_vcm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9718_device *dw9718 = sd_to_dw9718_vcm(sd);

	return dw9718_power_on(dw9718);
}

static const struct i2c_device_id dw9718_id_table[] = {
	{ DW9718_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, dw9718_id_table);

static const struct of_device_id dw9718_of_table[] = {
	{ .compatible = "mediatek,dw9718" },
	{ },
};
MODULE_DEVICE_TABLE(of, dw9718_of_table);

static const struct dev_pm_ops dw9718_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(dw9718_vcm_suspend, dw9718_vcm_resume, NULL)
};

static struct i2c_driver dw9718_i2c_driver = {
	.driver = {
		.name = DW9718_NAME,
		.pm = &dw9718_pm_ops,
		.of_match_table = dw9718_of_table,
	},
	.probe_new  = dw9718_probe,
	.remove = dw9718_remove,
	.id_table = dw9718_id_table,
};

module_i2c_driver(dw9718_i2c_driver);

MODULE_AUTHOR("Dongchun Zhu <dongchun.zhu@mediatek.com>");
MODULE_DESCRIPTION("DW9718 VCM driver");
MODULE_LICENSE("GPL v2");
