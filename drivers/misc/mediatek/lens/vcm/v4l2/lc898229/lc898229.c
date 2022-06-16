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

#define DRIVER_NAME "lc898229"

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define LC898229_NAME				"lc898229"
#define LC898229_MAX_FOCUS_POS			1023
#define LC898229_ORIGIN_FOCUS_POS		0
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define LC898229_FOCUS_STEPS			1
#define LC898229_SET_POSITION_ADDR		0x84

#define LC898229_CMD_DELAY			0xff
#define LC898229_CTRL_DELAY_US			5000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define LC898229_MOVE_STEPS			16
#define LC898229_MOVE_DELAY_US			8400

/* lc898229 device structure */
struct lc898229_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

static inline struct lc898229_device *to_lc898229_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct lc898229_device, ctrls);
}

static inline struct lc898229_device *sd_to_lc898229_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct lc898229_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static int lc898229_set_position(struct lc898229_device *lc898229, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&lc898229->sd);

	return i2c_smbus_write_word_data(client, LC898229_SET_POSITION_ADDR,
					 swab16(val));
}

static int lc898229_release(struct lc898229_device *lc898229)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;

	diff_dac = LC898229_ORIGIN_FOCUS_POS - lc898229->focus->val;

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		LC898229_MOVE_STEPS;

	val = lc898229->focus->val;

	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (LC898229_MOVE_STEPS*(-1)) : LC898229_MOVE_STEPS);

		ret = lc898229_set_position(lc898229, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(LC898229_MOVE_DELAY_US,
			     LC898229_MOVE_DELAY_US + 1000);
	}

	// last step to origin
	ret = lc898229_set_position(lc898229, LC898229_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}

	LOG_INF("-\n");

	return 0;
}

static int lc898229_init(struct lc898229_device *lc898229)
{
	struct i2c_client *client = v4l2_get_subdevdata(&lc898229->sd);
	int ret;

	ret = i2c_smbus_read_byte_data(client, 0xF0);

	LOG_INF("Check HW version: %x\n", ret);

	if (ret == 0xA5) {
		int wait_cnt = 20;

		ret = i2c_smbus_write_byte_data(client, 0xE0, 0x01);

		while (wait_cnt > 0) {
			ret = i2c_smbus_read_byte_data(client, 0xB3);

			if (ret == 0)
				break;

			wait_cnt--;
		}
	}

	return 0;
}

/* Power handling */
static int lc898229_power_off(struct lc898229_device *lc898229)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = lc898229_release(lc898229);
	if (ret)
		LOG_INF("lc898229 release failed!\n");

	ret = regulator_disable(lc898229->vin);
	if (ret)
		return ret;

	ret = regulator_disable(lc898229->vdd);
	if (ret)
		return ret;

	if (lc898229->vcamaf_pinctrl && lc898229->vcamaf_off)
		ret = pinctrl_select_state(lc898229->vcamaf_pinctrl,
					lc898229->vcamaf_off);

	return ret;
}

static int lc898229_power_on(struct lc898229_device *lc898229)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(lc898229->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(lc898229->vdd);
	if (ret < 0)
		return ret;

	if (lc898229->vcamaf_pinctrl && lc898229->vcamaf_on)
		ret = pinctrl_select_state(lc898229->vcamaf_pinctrl,
					lc898229->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(LC898229_CTRL_DELAY_US, LC898229_CTRL_DELAY_US + 100);

	ret = lc898229_init(lc898229);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(lc898229->vin);
	regulator_disable(lc898229->vdd);
	if (lc898229->vcamaf_pinctrl && lc898229->vcamaf_off) {
		pinctrl_select_state(lc898229->vcamaf_pinctrl,
				lc898229->vcamaf_off);
	}

	return ret;
}

static int lc898229_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct lc898229_device *lc898229 = to_lc898229_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = lc898229_set_position(lc898229, ctrl->val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops lc898229_vcm_ctrl_ops = {
	.s_ctrl = lc898229_set_ctrl,
};

static int lc898229_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct lc898229_device *lc898229 = sd_to_lc898229_vcm(sd);

	LOG_INF("%s\n", __func__);

	ret = lc898229_power_on(lc898229);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int lc898229_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct lc898229_device *lc898229 = sd_to_lc898229_vcm(sd);

	LOG_INF("%s\n", __func__);

	lc898229_power_off(lc898229);

	return 0;
}

static const struct v4l2_subdev_internal_ops lc898229_int_ops = {
	.open = lc898229_open,
	.close = lc898229_close,
};

static const struct v4l2_subdev_ops lc898229_ops = { };

static void lc898229_subdev_cleanup(struct lc898229_device *lc898229)
{
	v4l2_async_unregister_subdev(&lc898229->sd);
	v4l2_ctrl_handler_free(&lc898229->ctrls);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&lc898229->sd.entity);
#endif
}

static int lc898229_init_controls(struct lc898229_device *lc898229)
{
	struct v4l2_ctrl_handler *hdl = &lc898229->ctrls;
	const struct v4l2_ctrl_ops *ops = &lc898229_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	lc898229->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, LC898229_MAX_FOCUS_POS, LC898229_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	lc898229->sd.ctrl_handler = hdl;

	return 0;
}

static int lc898229_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct lc898229_device *lc898229;
	int ret;

	LOG_INF("%s\n", __func__);

	lc898229 = devm_kzalloc(dev, sizeof(*lc898229), GFP_KERNEL);
	if (!lc898229)
		return -ENOMEM;

	lc898229->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(lc898229->vin)) {
		ret = PTR_ERR(lc898229->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	lc898229->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(lc898229->vdd)) {
		ret = PTR_ERR(lc898229->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	lc898229->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(lc898229->vcamaf_pinctrl)) {
		ret = PTR_ERR(lc898229->vcamaf_pinctrl);
		lc898229->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		lc898229->vcamaf_on = pinctrl_lookup_state(
			lc898229->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(lc898229->vcamaf_on)) {
			ret = PTR_ERR(lc898229->vcamaf_on);
			lc898229->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		lc898229->vcamaf_off = pinctrl_lookup_state(
			lc898229->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(lc898229->vcamaf_off)) {
			ret = PTR_ERR(lc898229->vcamaf_off);
			lc898229->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&lc898229->sd, client, &lc898229_ops);
	lc898229->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	lc898229->sd.internal_ops = &lc898229_int_ops;

	ret = lc898229_init_controls(lc898229);
	if (ret)
		goto err_cleanup;

#if defined(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&lc898229->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	lc898229->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&lc898229->sd);
	if (ret < 0)
		goto err_cleanup;

	return 0;

err_cleanup:
	lc898229_subdev_cleanup(lc898229);
	return ret;
}

static int lc898229_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lc898229_device *lc898229 = sd_to_lc898229_vcm(sd);

	LOG_INF("%s\n", __func__);

	lc898229_subdev_cleanup(lc898229);

	return 0;
}

static const struct i2c_device_id lc898229_id_table[] = {
	{ LC898229_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, lc898229_id_table);

static const struct of_device_id lc898229_of_table[] = {
	{ .compatible = "mediatek,lc898229" },
	{ },
};
MODULE_DEVICE_TABLE(of, lc898229_of_table);

static struct i2c_driver lc898229_i2c_driver = {
	.driver = {
		.name = LC898229_NAME,
		.of_match_table = lc898229_of_table,
	},
	.probe_new  = lc898229_probe,
	.remove = lc898229_remove,
	.id_table = lc898229_id_table,
};

module_i2c_driver(lc898229_i2c_driver);

MODULE_AUTHOR("Sam Hung <Sam.Hung@mediatek.com>");
MODULE_DESCRIPTION("LC898229 VCM driver");
MODULE_LICENSE("GPL v2");
