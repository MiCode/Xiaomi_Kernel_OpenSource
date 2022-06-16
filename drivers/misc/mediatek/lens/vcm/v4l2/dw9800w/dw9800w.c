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

#define DRIVER_NAME "dw9800w"
#define DW9800W_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define DW9800W_NAME				"dw9800w"
#define DW9800W_MAX_FOCUS_POS			1023
#define DW9800W_ORIGIN_FOCUS_POS		512
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define DW9800W_FOCUS_STEPS			1
#define DW9800W_SET_POSITION_ADDR		0x03

#define DW9800W_CMD_DELAY			0xff
#define DW9800W_CTRL_DELAY_US			10000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define DW9800W_MOVE_STEPS			100
#define DW9800W_MOVE_DELAY_US			5000

/* dw9800w device structure */
struct dw9800w_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

static inline struct dw9800w_device *to_dw9800w_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct dw9800w_device, ctrls);
}

static inline struct dw9800w_device *sd_to_dw9800w_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct dw9800w_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int dw9800w_set_position(struct dw9800w_device *dw9800w, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9800w->sd);

	return i2c_smbus_write_word_data(client, DW9800W_SET_POSITION_ADDR,
					 swab16(val));
}

static int dw9800w_release(struct dw9800w_device *dw9800w)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9800w->sd);

	diff_dac = DW9800W_ORIGIN_FOCUS_POS - dw9800w->focus->val;

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		DW9800W_MOVE_STEPS;

	val = dw9800w->focus->val;

	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (DW9800W_MOVE_STEPS*(-1)) : DW9800W_MOVE_STEPS);

		ret = dw9800w_set_position(dw9800w, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(DW9800W_MOVE_DELAY_US,
			     DW9800W_MOVE_DELAY_US + 1000);
	}

	// last step to origin
	ret = dw9800w_set_position(dw9800w, DW9800W_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}

	i2c_smbus_write_byte_data(client, 0x02, 0x20);

	LOG_INF("-\n");

	return 0;
}

static int dw9800w_init(struct dw9800w_device *dw9800w)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9800w->sd);
	int ret = 0;
	char puSendCmdArray[7][2] = {
	{0x02, 0x01}, {0x02, 0x00}, {0xFE, 0xFE},
	{0x02, 0x02}, {0x06, 0x40}, {0x07, 0x60}, {0xFE, 0xFE},
	};
	unsigned char cmd_number;

	client->addr = DW9800W_I2C_SLAVE_ADDR >> 1;
	//ret = i2c_smbus_read_byte_data(client, 0x02);

	LOG_INF("Check HW version: %x\n", ret);

	for (cmd_number = 0; cmd_number < 7; cmd_number++) {
		if (puSendCmdArray[cmd_number][0] != 0xFE) {
			ret = i2c_smbus_write_byte_data(client,
					puSendCmdArray[cmd_number][0],
					puSendCmdArray[cmd_number][1]);

			if (ret < 0)
				return -1;
		} else {
			udelay(100);
		}
	}

	return ret;
}

/* Power handling */
static int dw9800w_power_off(struct dw9800w_device *dw9800w)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = dw9800w_release(dw9800w);
	if (ret)
		LOG_INF("dw9800w release failed!\n");

	ret = regulator_disable(dw9800w->vin);
	if (ret)
		return ret;

	ret = regulator_disable(dw9800w->vdd);
	if (ret)
		return ret;

	if (dw9800w->vcamaf_pinctrl && dw9800w->vcamaf_off)
		ret = pinctrl_select_state(dw9800w->vcamaf_pinctrl,
					dw9800w->vcamaf_off);

	return ret;
}

static int dw9800w_power_on(struct dw9800w_device *dw9800w)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(dw9800w->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(dw9800w->vdd);
	if (ret < 0)
		return ret;

	if (dw9800w->vcamaf_pinctrl && dw9800w->vcamaf_on)
		ret = pinctrl_select_state(dw9800w->vcamaf_pinctrl,
					dw9800w->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(DW9800W_CTRL_DELAY_US, DW9800W_CTRL_DELAY_US + 100);

	ret = dw9800w_init(dw9800w);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(dw9800w->vin);
	regulator_disable(dw9800w->vdd);
	if (dw9800w->vcamaf_pinctrl && dw9800w->vcamaf_off) {
		pinctrl_select_state(dw9800w->vcamaf_pinctrl,
				dw9800w->vcamaf_off);
	}

	return ret;
}

static int dw9800w_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct dw9800w_device *dw9800w = to_dw9800w_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = dw9800w_set_position(dw9800w, ctrl->val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops dw9800w_vcm_ctrl_ops = {
	.s_ctrl = dw9800w_set_ctrl,
};

static int dw9800w_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct dw9800w_device *dw9800w = sd_to_dw9800w_vcm(sd);

	LOG_INF("%s\n", __func__);

	ret = dw9800w_power_on(dw9800w);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int dw9800w_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct dw9800w_device *dw9800w = sd_to_dw9800w_vcm(sd);

	LOG_INF("%s\n", __func__);

	dw9800w_power_off(dw9800w);

	return 0;
}

static const struct v4l2_subdev_internal_ops dw9800w_int_ops = {
	.open = dw9800w_open,
	.close = dw9800w_close,
};

static const struct v4l2_subdev_ops dw9800w_ops = { };

static void dw9800w_subdev_cleanup(struct dw9800w_device *dw9800w)
{
	v4l2_async_unregister_subdev(&dw9800w->sd);
	v4l2_ctrl_handler_free(&dw9800w->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&dw9800w->sd.entity);
#endif
}

static int dw9800w_init_controls(struct dw9800w_device *dw9800w)
{
	struct v4l2_ctrl_handler *hdl = &dw9800w->ctrls;
	const struct v4l2_ctrl_ops *ops = &dw9800w_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	dw9800w->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, DW9800W_MAX_FOCUS_POS, DW9800W_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	dw9800w->sd.ctrl_handler = hdl;

	return 0;
}

static int dw9800w_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct dw9800w_device *dw9800w;
	int ret;

	LOG_INF("%s\n", __func__);

	dw9800w = devm_kzalloc(dev, sizeof(*dw9800w), GFP_KERNEL);
	if (!dw9800w)
		return -ENOMEM;

	dw9800w->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(dw9800w->vin)) {
		ret = PTR_ERR(dw9800w->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	dw9800w->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(dw9800w->vdd)) {
		ret = PTR_ERR(dw9800w->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	dw9800w->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(dw9800w->vcamaf_pinctrl)) {
		ret = PTR_ERR(dw9800w->vcamaf_pinctrl);
		dw9800w->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		dw9800w->vcamaf_on = pinctrl_lookup_state(
			dw9800w->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(dw9800w->vcamaf_on)) {
			ret = PTR_ERR(dw9800w->vcamaf_on);
			dw9800w->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		dw9800w->vcamaf_off = pinctrl_lookup_state(
			dw9800w->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(dw9800w->vcamaf_off)) {
			ret = PTR_ERR(dw9800w->vcamaf_off);
			dw9800w->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&dw9800w->sd, client, &dw9800w_ops);
	dw9800w->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dw9800w->sd.internal_ops = &dw9800w_int_ops;

	ret = dw9800w_init_controls(dw9800w);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&dw9800w->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	dw9800w->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&dw9800w->sd);
	if (ret < 0)
		goto err_cleanup;

	return 0;

err_cleanup:
	dw9800w_subdev_cleanup(dw9800w);
	return ret;
}

static int dw9800w_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9800w_device *dw9800w = sd_to_dw9800w_vcm(sd);

	LOG_INF("%s\n", __func__);

	dw9800w_subdev_cleanup(dw9800w);

	return 0;
}

static const struct i2c_device_id dw9800w_id_table[] = {
	{ DW9800W_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, dw9800w_id_table);

static const struct of_device_id dw9800w_of_table[] = {
	{ .compatible = "mediatek,dw9800w" },
	{ },
};
MODULE_DEVICE_TABLE(of, dw9800w_of_table);

static struct i2c_driver dw9800w_i2c_driver = {
	.driver = {
		.name = DW9800W_NAME,
		.of_match_table = dw9800w_of_table,
	},
	.probe_new  = dw9800w_probe,
	.remove = dw9800w_remove,
	.id_table = dw9800w_id_table,
};

module_i2c_driver(dw9800w_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("DW9800W VCM driver");
MODULE_LICENSE("GPL v2");
