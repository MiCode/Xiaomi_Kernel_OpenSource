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

#define DRIVER_NAME "ak7375c"
#define AK7375C_I2C_SLAVE_ADDR 0xE8

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define AK7375C_NAME				"ak7375c"
#define AK7375C_MAX_FOCUS_POS			1023
#define AK7375C_ORIGIN_FOCUS_POS		0
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define AK7375C_FOCUS_STEPS			1
#define AK7375C_SET_POSITION_ADDR		0x00

#define AK7375C_CMD_DELAY			0xff
#define AK7375C_CTRL_DELAY_US			10000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define AK7375C_MOVE_STEPS			100
#define AK7375C_MOVE_DELAY_US			5000

/* ak7375c device structure */
struct ak7375c_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

static inline struct ak7375c_device *to_ak7375c_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct ak7375c_device, ctrls);
}

static inline struct ak7375c_device *sd_to_ak7375c_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct ak7375c_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int ak7375c_set_position(struct ak7375c_device *ak7375c, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7375c->sd);

	return i2c_smbus_write_word_data(client, AK7375C_SET_POSITION_ADDR,
					 swab16(val << 6));
}

static int ak7375c_release(struct ak7375c_device *ak7375c)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7375c->sd);

	diff_dac = AK7375C_ORIGIN_FOCUS_POS - ak7375c->focus->val;

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		AK7375C_MOVE_STEPS;

	val = ak7375c->focus->val;

	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (AK7375C_MOVE_STEPS*(-1)) : AK7375C_MOVE_STEPS);

		ret = ak7375c_set_position(ak7375c, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(AK7375C_MOVE_DELAY_US,
			     AK7375C_MOVE_DELAY_US + 1000);
	}

	// last step to origin
	ret = ak7375c_set_position(ak7375c, AK7375C_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}

	i2c_smbus_write_byte_data(client, 0x02, 0x20);

	LOG_INF("-\n");

	return 0;
}

static int ak7375c_init(struct ak7375c_device *ak7375c)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7375c->sd);
	int ret = 0;

	client->addr = AK7375C_I2C_SLAVE_ADDR >> 1;
	//ret = i2c_smbus_read_byte_data(client, 0x02);

	LOG_INF("Check HW version: %x\n", ret);

	/* 00:active mode , 10:Standby mode , x1:Sleep mode */
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00);

	return 0;
}

/* Power handling */
static int ak7375c_power_off(struct ak7375c_device *ak7375c)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = ak7375c_release(ak7375c);
	if (ret)
		LOG_INF("ak7375c release failed!\n");

	ret = regulator_disable(ak7375c->vin);
	if (ret)
		return ret;

	ret = regulator_disable(ak7375c->vdd);
	if (ret)
		return ret;

	if (ak7375c->vcamaf_pinctrl && ak7375c->vcamaf_off)
		ret = pinctrl_select_state(ak7375c->vcamaf_pinctrl,
					ak7375c->vcamaf_off);

	return ret;
}

static int ak7375c_power_on(struct ak7375c_device *ak7375c)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(ak7375c->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(ak7375c->vdd);
	if (ret < 0)
		return ret;

	if (ak7375c->vcamaf_pinctrl && ak7375c->vcamaf_on)
		ret = pinctrl_select_state(ak7375c->vcamaf_pinctrl,
					ak7375c->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(AK7375C_CTRL_DELAY_US, AK7375C_CTRL_DELAY_US + 100);

	ret = ak7375c_init(ak7375c);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(ak7375c->vin);
	regulator_disable(ak7375c->vdd);
	if (ak7375c->vcamaf_pinctrl && ak7375c->vcamaf_off) {
		pinctrl_select_state(ak7375c->vcamaf_pinctrl,
				ak7375c->vcamaf_off);
	}

	return ret;
}

static int ak7375c_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct ak7375c_device *ak7375c = to_ak7375c_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = ak7375c_set_position(ak7375c, ctrl->val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops ak7375c_vcm_ctrl_ops = {
	.s_ctrl = ak7375c_set_ctrl,
};

static int ak7375c_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct ak7375c_device *ak7375c = sd_to_ak7375c_vcm(sd);

	LOG_INF("%s\n", __func__);

	ret = ak7375c_power_on(ak7375c);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int ak7375c_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ak7375c_device *ak7375c = sd_to_ak7375c_vcm(sd);

	LOG_INF("%s\n", __func__);

	ak7375c_power_off(ak7375c);

	return 0;
}

static const struct v4l2_subdev_internal_ops ak7375c_int_ops = {
	.open = ak7375c_open,
	.close = ak7375c_close,
};

static const struct v4l2_subdev_ops ak7375c_ops = { };

static void ak7375c_subdev_cleanup(struct ak7375c_device *ak7375c)
{
	v4l2_async_unregister_subdev(&ak7375c->sd);
	v4l2_ctrl_handler_free(&ak7375c->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&ak7375c->sd.entity);
#endif
}

static int ak7375c_init_controls(struct ak7375c_device *ak7375c)
{
	struct v4l2_ctrl_handler *hdl = &ak7375c->ctrls;
	const struct v4l2_ctrl_ops *ops = &ak7375c_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	ak7375c->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, AK7375C_MAX_FOCUS_POS, AK7375C_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	ak7375c->sd.ctrl_handler = hdl;

	return 0;
}

static int ak7375c_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ak7375c_device *ak7375c;
	int ret;

	LOG_INF("%s\n", __func__);

	ak7375c = devm_kzalloc(dev, sizeof(*ak7375c), GFP_KERNEL);
	if (!ak7375c)
		return -ENOMEM;

	ak7375c->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(ak7375c->vin)) {
		ret = PTR_ERR(ak7375c->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	ak7375c->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(ak7375c->vdd)) {
		ret = PTR_ERR(ak7375c->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	ak7375c->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ak7375c->vcamaf_pinctrl)) {
		ret = PTR_ERR(ak7375c->vcamaf_pinctrl);
		ak7375c->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		ak7375c->vcamaf_on = pinctrl_lookup_state(
			ak7375c->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(ak7375c->vcamaf_on)) {
			ret = PTR_ERR(ak7375c->vcamaf_on);
			ak7375c->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		ak7375c->vcamaf_off = pinctrl_lookup_state(
			ak7375c->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(ak7375c->vcamaf_off)) {
			ret = PTR_ERR(ak7375c->vcamaf_off);
			ak7375c->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&ak7375c->sd, client, &ak7375c_ops);
	ak7375c->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ak7375c->sd.internal_ops = &ak7375c_int_ops;

	ret = ak7375c_init_controls(ak7375c);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&ak7375c->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	ak7375c->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&ak7375c->sd);
	if (ret < 0)
		goto err_cleanup;

	return 0;

err_cleanup:
	ak7375c_subdev_cleanup(ak7375c);
	return ret;
}

static int ak7375c_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7375c_device *ak7375c = sd_to_ak7375c_vcm(sd);

	LOG_INF("%s\n", __func__);

	ak7375c_subdev_cleanup(ak7375c);

	return 0;
}

static const struct i2c_device_id ak7375c_id_table[] = {
	{ AK7375C_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ak7375c_id_table);

static const struct of_device_id ak7375c_of_table[] = {
	{ .compatible = "mediatek,ak7375c" },
	{ },
};
MODULE_DEVICE_TABLE(of, ak7375c_of_table);

static struct i2c_driver ak7375c_i2c_driver = {
	.driver = {
		.name = AK7375C_NAME,
		.of_match_table = ak7375c_of_table,
	},
	.probe_new  = ak7375c_probe,
	.remove = ak7375c_remove,
	.id_table = ak7375c_id_table,
};

module_i2c_driver(ak7375c_i2c_driver);

MODULE_AUTHOR("XXX");
MODULE_DESCRIPTION("AK7375C VCM driver");
MODULE_LICENSE("GPL v2");
