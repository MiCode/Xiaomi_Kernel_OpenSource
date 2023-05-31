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

#define DRIVER_NAME "bu64253gwz"
#define BU64253GWZ_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define BU64253GWZ_NAME				"bu64253gwz"
#define BU64253GWZ_MAX_FOCUS_POS			1023
#define BU64253GWZ_ORIGIN_FOCUS_POS			512
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define BU64253GWZ_FOCUS_STEPS			1
#define BU64253GWZ_SET_POSITION_ADDR		0x00

#define BU64253GWZ_CMD_DELAY			0xff
#define BU64253GWZ_CTRL_DELAY_US			10000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define BU64253GWZ_MOVE_STEPS				100
#define BU64253GWZ_MOVE_DELAY_US			5000

/* bu64253gwz device structure */
struct bu64253gwz_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

static inline struct bu64253gwz_device *to_bu64253gwz_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct bu64253gwz_device, ctrls);
}

static inline struct bu64253gwz_device *sd_to_bu64253gwz_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct bu64253gwz_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int bu64253gwz_set_position(struct bu64253gwz_device *bu64253gwz, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&bu64253gwz->sd);
	char puSendCmd[2] = {(char)(((val >> 8) & 0x03) | 0xC4),
			     (char)(val & 0xFF)};

	return i2c_master_send(client, puSendCmd, 2);
}

static int bu64253gwz_release(struct bu64253gwz_device *bu64253gwz)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	char puSendCmd[2];
	struct i2c_client *client = v4l2_get_subdevdata(&bu64253gwz->sd);

	diff_dac = BU64253GWZ_ORIGIN_FOCUS_POS - bu64253gwz->focus->val;

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		BU64253GWZ_MOVE_STEPS;

	val = bu64253gwz->focus->val;

	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (BU64253GWZ_MOVE_STEPS*(-1)) : BU64253GWZ_MOVE_STEPS);

		ret = bu64253gwz_set_position(bu64253gwz, val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(BU64253GWZ_MOVE_DELAY_US,
			     BU64253GWZ_MOVE_DELAY_US + 1000);
	}

	// last step to origin
	ret = bu64253gwz_set_position(bu64253gwz, BU64253GWZ_ORIGIN_FOCUS_POS);
	if (ret < 0) {
		LOG_INF("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}

	puSendCmd[0] = (char)(0x00);
	puSendCmd[1] = (char)(0x00);
	i2c_master_send(client, puSendCmd, 2);

	LOG_INF("-\n");

	return 0;
}

static int bu64253gwz_init(struct bu64253gwz_device *bu64253gwz)
{
	struct i2c_client *client = v4l2_get_subdevdata(&bu64253gwz->sd);
	char puSendCmd[2];
	int ret = 0;

	LOG_INF("+\n");

	client->addr = BU64253GWZ_I2C_SLAVE_ADDR >> 1;

	LOG_INF("Enable ISRC\n");
	puSendCmd[0] = (char)(0xC2);
	puSendCmd[1] = (char)(0x00);
	ret = i2c_master_send(client, puSendCmd, 2);
	if (ret < 0) {
		LOG_INF("I2C write failed!!\n");
		return -1;
	}

	puSendCmd[0] = (char)(0xC8);
	puSendCmd[1] = (char)(0x01);
	ret = i2c_master_send(client, puSendCmd, 2);
	if (ret < 0) {
		LOG_INF("I2C write failed!!\n");
		return -1;
	}

	puSendCmd[0] = (char)(0xD0);
	puSendCmd[1] = (char)(0x42);
	ret = i2c_master_send(client, puSendCmd, 2);
	if (ret < 0) {
		LOG_INF("I2C write failed!!\n");
		return -1;
	}

	LOG_INF("-\n");

	return 0;
}

/* Power handling */
static int bu64253gwz_power_off(struct bu64253gwz_device *bu64253gwz)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = bu64253gwz_release(bu64253gwz);
	if (ret)
		LOG_INF("bu64253gwz release failed!\n");

	ret = regulator_disable(bu64253gwz->vin);
	if (ret)
		return ret;

	ret = regulator_disable(bu64253gwz->vdd);
	if (ret)
		return ret;

	if (bu64253gwz->vcamaf_pinctrl && bu64253gwz->vcamaf_off)
		ret = pinctrl_select_state(bu64253gwz->vcamaf_pinctrl,
					bu64253gwz->vcamaf_off);

	return ret;
}

static int bu64253gwz_power_on(struct bu64253gwz_device *bu64253gwz)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(bu64253gwz->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(bu64253gwz->vdd);
	if (ret < 0)
		return ret;

	if (bu64253gwz->vcamaf_pinctrl && bu64253gwz->vcamaf_on)
		ret = pinctrl_select_state(bu64253gwz->vcamaf_pinctrl,
					bu64253gwz->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(BU64253GWZ_CTRL_DELAY_US, BU64253GWZ_CTRL_DELAY_US + 100);

	ret = bu64253gwz_init(bu64253gwz);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(bu64253gwz->vin);
	regulator_disable(bu64253gwz->vdd);
	if (bu64253gwz->vcamaf_pinctrl && bu64253gwz->vcamaf_off) {
		pinctrl_select_state(bu64253gwz->vcamaf_pinctrl,
				bu64253gwz->vcamaf_off);
	}

	return ret;
}

static int bu64253gwz_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct bu64253gwz_device *bu64253gwz = to_bu64253gwz_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = bu64253gwz_set_position(bu64253gwz, ctrl->val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops bu64253gwz_vcm_ctrl_ops = {
	.s_ctrl = bu64253gwz_set_ctrl,
};

static int bu64253gwz_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct bu64253gwz_device *bu64253gwz = sd_to_bu64253gwz_vcm(sd);

	LOG_INF("%s\n", __func__);

	ret = bu64253gwz_power_on(bu64253gwz);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int bu64253gwz_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct bu64253gwz_device *bu64253gwz = sd_to_bu64253gwz_vcm(sd);

	LOG_INF("%s\n", __func__);

	bu64253gwz_power_off(bu64253gwz);

	return 0;
}

static const struct v4l2_subdev_internal_ops bu64253gwz_int_ops = {
	.open = bu64253gwz_open,
	.close = bu64253gwz_close,
};

static const struct v4l2_subdev_ops bu64253gwz_ops = { };

static void bu64253gwz_subdev_cleanup(struct bu64253gwz_device *bu64253gwz)
{
	v4l2_async_unregister_subdev(&bu64253gwz->sd);
	v4l2_ctrl_handler_free(&bu64253gwz->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&bu64253gwz->sd.entity);
#endif
}

static int bu64253gwz_init_controls(struct bu64253gwz_device *bu64253gwz)
{
	struct v4l2_ctrl_handler *hdl = &bu64253gwz->ctrls;
	const struct v4l2_ctrl_ops *ops = &bu64253gwz_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	bu64253gwz->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, BU64253GWZ_MAX_FOCUS_POS, BU64253GWZ_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	bu64253gwz->sd.ctrl_handler = hdl;

	return 0;
}

static int bu64253gwz_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct bu64253gwz_device *bu64253gwz;
	int ret;

	LOG_INF("%s\n", __func__);

	bu64253gwz = devm_kzalloc(dev, sizeof(*bu64253gwz), GFP_KERNEL);
	if (!bu64253gwz)
		return -ENOMEM;

	bu64253gwz->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(bu64253gwz->vin)) {
		ret = PTR_ERR(bu64253gwz->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	bu64253gwz->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(bu64253gwz->vdd)) {
		ret = PTR_ERR(bu64253gwz->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	bu64253gwz->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(bu64253gwz->vcamaf_pinctrl)) {
		ret = PTR_ERR(bu64253gwz->vcamaf_pinctrl);
		bu64253gwz->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		bu64253gwz->vcamaf_on = pinctrl_lookup_state(
			bu64253gwz->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(bu64253gwz->vcamaf_on)) {
			ret = PTR_ERR(bu64253gwz->vcamaf_on);
			bu64253gwz->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		bu64253gwz->vcamaf_off = pinctrl_lookup_state(
			bu64253gwz->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(bu64253gwz->vcamaf_off)) {
			ret = PTR_ERR(bu64253gwz->vcamaf_off);
			bu64253gwz->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&bu64253gwz->sd, client, &bu64253gwz_ops);
	bu64253gwz->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	bu64253gwz->sd.internal_ops = &bu64253gwz_int_ops;

	ret = bu64253gwz_init_controls(bu64253gwz);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&bu64253gwz->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	bu64253gwz->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&bu64253gwz->sd);
	if (ret < 0)
		goto err_cleanup;

	return 0;

err_cleanup:
	bu64253gwz_subdev_cleanup(bu64253gwz);
	return ret;
}

static int bu64253gwz_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bu64253gwz_device *bu64253gwz = sd_to_bu64253gwz_vcm(sd);

	LOG_INF("%s\n", __func__);

	bu64253gwz_subdev_cleanup(bu64253gwz);

	return 0;
}

static const struct i2c_device_id bu64253gwz_id_table[] = {
	{ BU64253GWZ_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bu64253gwz_id_table);

static const struct of_device_id bu64253gwz_of_table[] = {
	{ .compatible = "mediatek,bu64253gwz" },
	{ },
};
MODULE_DEVICE_TABLE(of, bu64253gwz_of_table);

static struct i2c_driver bu64253gwz_i2c_driver = {
	.driver = {
		.name = BU64253GWZ_NAME,
		.of_match_table = bu64253gwz_of_table,
	},
	.probe_new  = bu64253gwz_probe,
	.remove = bu64253gwz_remove,
	.id_table = bu64253gwz_id_table,
};

module_i2c_driver(bu64253gwz_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("AK7377A VCM driver");
MODULE_LICENSE("GPL v2");
