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

#define DRIVER_NAME "ak7314"
#define AK7314_I2C_SLAVE_ADDR  0x18


#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define AK7314_NAME				"ak7314"
#define AK7314_MAX_FOCUS_POS			1023
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define AK7314_FOCUS_STEPS			1
#define AK7314_SET_POSITION_ADDR		0x00
#define AK7314_SET_POSITION_OFFSET		6

#define AK7314_CMD_DELAY			0xff
#define AK7314_CTRL_DELAY_US			10000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define AK7314_MOVE_STEPS			16
#define AK7314_MOVE_DELAY_US			8400
#define AK7314_STABLE_TIME_US			20000

/* ak7314 device structure */
struct ak7314_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

static inline struct ak7314_device *to_ak7314_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct ak7314_device, ctrls);
}

static inline struct ak7314_device *sd_to_ak7314_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct ak7314_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int ak7314_set_position(struct ak7314_device *ak7314, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7314->sd);
	return i2c_smbus_write_word_data(client, AK7314_SET_POSITION_ADDR,
					swab16(val << AK7314_SET_POSITION_OFFSET));
}

static int ak7314_release(struct ak7314_device *ak7314)
{
	int ret, val;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7314->sd);

	for (val = round_down(ak7314->focus->val, AK7314_MOVE_STEPS);
	     val >= 0; val -= AK7314_MOVE_STEPS) {
		ret = ak7314_set_position(ak7314, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(AK7314_MOVE_DELAY_US,
			     AK7314_MOVE_DELAY_US + 1000);
	}

	i2c_smbus_write_byte_data(client, 0x02, 0x20);
	msleep(20);

	return 0;
}

static int ak7314_init(struct ak7314_device *ak7314)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7314->sd);
	int ret = 0;

	LOG_INF("+\n");

	client->addr = AK7314_I2C_SLAVE_ADDR >> 1;
	ret = i2c_smbus_read_byte_data(client, 0x02);

	LOG_INF("Check HW version: %x\n", ret);

	/* 00:active mode , 10:Standby mode , x1:Sleep mode */
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00);

	LOG_INF("-\n");

	return ret;
}

/* Power handling */
static int ak7314_power_off(struct ak7314_device *ak7314)
{
	int ret;

	LOG_INF("%s +\n", __func__);

	ret = ak7314_release(ak7314);
	if (ret)
		LOG_INF("ak7314 release failed!\n");

	ret = regulator_disable(ak7314->vin);
	if (ret)
		return ret;

	ret = regulator_disable(ak7314->vdd);
	if (ret)
		return ret;

	if (ak7314->vcamaf_pinctrl && ak7314->vcamaf_off)
		ret = pinctrl_select_state(ak7314->vcamaf_pinctrl,
					ak7314->vcamaf_off);
	LOG_INF("%s -\n", __func__);
	return ret;
}

static int ak7314_power_on(struct ak7314_device *ak7314)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(ak7314->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(ak7314->vdd);
	if (ret < 0)
		return ret;

	if (ak7314->vcamaf_pinctrl && ak7314->vcamaf_on)
		ret = pinctrl_select_state(ak7314->vcamaf_pinctrl,
					ak7314->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	//usleep_range(AK7314_CTRL_DELAY_US, AK7314_CTRL_DELAY_US + 100);

	ret = ak7314_init(ak7314);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	LOG_INF("ak7314 power on failed!\n");
	regulator_disable(ak7314->vin);
	regulator_disable(ak7314->vdd);
	if (ak7314->vcamaf_pinctrl && ak7314->vcamaf_off) {
		pinctrl_select_state(ak7314->vcamaf_pinctrl,
				ak7314->vcamaf_off);
	}

	return ret;
}

static int ak7314_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct ak7314_device *ak7314 = to_ak7314_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = ak7314_set_position(ak7314, ctrl->val);
		if (ret) {
			LOG_INF("I2C failure: %d", ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops ak7314_vcm_ctrl_ops = {
	.s_ctrl = ak7314_set_ctrl,
};

static int ak7314_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
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

static int ak7314_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	LOG_INF("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops ak7314_int_ops = {
	.open = ak7314_open,
	.close = ak7314_close,
};

static const struct v4l2_subdev_ops ak7314_ops = { };

static void ak7314_subdev_cleanup(struct ak7314_device *ak7314)
{
	v4l2_async_unregister_subdev(&ak7314->sd);
	v4l2_ctrl_handler_free(&ak7314->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&ak7314->sd.entity);
#endif
}

static int ak7314_init_controls(struct ak7314_device *ak7314)
{
	struct v4l2_ctrl_handler *hdl = &ak7314->ctrls;
	const struct v4l2_ctrl_ops *ops = &ak7314_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	ak7314->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, AK7314_MAX_FOCUS_POS, AK7314_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	ak7314->sd.ctrl_handler = hdl;

	return 0;
}

static int ak7314_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ak7314_device *ak7314;
	int ret;

	LOG_INF("%s\n", __func__);

	ak7314 = devm_kzalloc(dev, sizeof(*ak7314), GFP_KERNEL);
	if (!ak7314)
		return -ENOMEM;

	ak7314->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(ak7314->vin)) {
		ret = PTR_ERR(ak7314->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	ak7314->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(ak7314->vdd)) {
		ret = PTR_ERR(ak7314->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	ak7314->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ak7314->vcamaf_pinctrl)) {
		ret = PTR_ERR(ak7314->vcamaf_pinctrl);
		ak7314->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		ak7314->vcamaf_on = pinctrl_lookup_state(
			ak7314->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(ak7314->vcamaf_on)) {
			ret = PTR_ERR(ak7314->vcamaf_on);
			ak7314->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		ak7314->vcamaf_off = pinctrl_lookup_state(
			ak7314->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(ak7314->vcamaf_off)) {
			ret = PTR_ERR(ak7314->vcamaf_off);
			ak7314->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&ak7314->sd, client, &ak7314_ops);
	ak7314->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ak7314->sd.internal_ops = &ak7314_int_ops;

	ret = ak7314_init_controls(ak7314);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&ak7314->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	ak7314->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&ak7314->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	ak7314_subdev_cleanup(ak7314);
	return ret;
}

static int ak7314_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7314_device *ak7314 = sd_to_ak7314_vcm(sd);

	LOG_INF("%s\n", __func__);

	ak7314_subdev_cleanup(ak7314);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		ak7314_power_off(ak7314);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int __maybe_unused ak7314_vcm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7314_device *ak7314 = sd_to_ak7314_vcm(sd);

	return ak7314_power_off(ak7314);
}

static int __maybe_unused ak7314_vcm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7314_device *ak7314 = sd_to_ak7314_vcm(sd);

	return ak7314_power_on(ak7314);
}

static const struct i2c_device_id ak7314_id_table[] = {
	{ AK7314_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ak7314_id_table);

static const struct of_device_id ak7314_of_table[] = {
	{ .compatible = "mediatek,ak7314" },
	{ },
};
MODULE_DEVICE_TABLE(of, ak7314_of_table);

static const struct dev_pm_ops ak7314_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(ak7314_vcm_suspend, ak7314_vcm_resume, NULL)
};

static struct i2c_driver ak7314_i2c_driver = {
	.driver = {
		.name = AK7314_NAME,
		.pm = &ak7314_pm_ops,
		.of_match_table = ak7314_of_table,
	},
	.probe_new  = ak7314_probe,
	.remove = ak7314_remove,
	.id_table = ak7314_id_table,
};

module_i2c_driver(ak7314_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("AK7314 VCM driver");
MODULE_LICENSE("GPL v2");
