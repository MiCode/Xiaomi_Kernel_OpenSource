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

#define DRIVER_NAME "gt9768"
#define GT9768_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

#define GT9768_NAME				"gt9768"
#define GT9768_MAX_FOCUS_POS			1023
#define GT9768_ORIGIN_FOCUS_POS		0
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define GT9768_FOCUS_STEPS			1
#define GT9768_SET_POSITION_ADDR		0x03

#define GT9768_CMD_DELAY			0xff
#define GT9768_CTRL_DELAY_US			5000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define GT9768_MOVE_STEPS			100
#define GT9768_MOVE_DELAY_US			5000

/* gt9768 device structure */
struct gt9768_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

static inline struct gt9768_device *to_gt9768_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct gt9768_device, ctrls);
}

static inline struct gt9768_device *sd_to_gt9768_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct gt9768_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int gt9768_set_position(struct gt9768_device *gt9768, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&gt9768->sd);

	return i2c_smbus_write_word_data(client, GT9768_SET_POSITION_ADDR,
					 swab16(val));
}

static int gt9768_release(struct gt9768_device *gt9768)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;

	diff_dac = GT9768_ORIGIN_FOCUS_POS - gt9768->focus->val;

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		GT9768_MOVE_STEPS;

	val = gt9768->focus->val;

	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (GT9768_MOVE_STEPS*(-1)) : GT9768_MOVE_STEPS);

		ret = gt9768_set_position(gt9768, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(GT9768_MOVE_DELAY_US,
			     GT9768_MOVE_DELAY_US + 1000);
	}

	// last step to origin
	ret = gt9768_set_position(gt9768, GT9768_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}

	LOG_INF("-\n");

	return 0;
}

static int gt9768_init(struct gt9768_device *gt9768)
{
	struct i2c_client *client = v4l2_get_subdevdata(&gt9768->sd);
	int ret;

	pr_debug("chengming +\n");

	client->addr  = GT9768_I2C_SLAVE_ADDR >> 1;
	ret = i2c_smbus_read_byte_data(client, 0x00);

	pr_debug("chengming Check HW version: %x\n", ret);




	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00);

	pr_debug("chengming -Advance on  \n",ret);

	ret = i2c_smbus_write_byte_data(client, 0x06, 0x09);

	pr_debug("chengming -AAC3 mode  \n",ret);

	ret = i2c_smbus_write_byte_data(client, 0x07, 0x01);

	pr_debug("chengming -0x07 write success  \n",ret);
	ret = i2c_smbus_write_byte_data(client, 0x08, 0x56);

	pr_debug("chengming -0x08 write success  \n",ret);

 	usleep_range(GT9768_MOVE_DELAY_US,GT9768_MOVE_DELAY_US );
	pr_debug("chengming -\n");

	return 0;
}

/* Power handling */
static int gt9768_power_off(struct gt9768_device *gt9768)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = gt9768_release(gt9768);
	if (ret)
		LOG_INF("gt9768 release failed!\n");

	ret = regulator_disable(gt9768->vin);
	if (ret)
		return ret;

	ret = regulator_disable(gt9768->vdd);
	if (ret)
		return ret;

	if (gt9768->vcamaf_pinctrl && gt9768->vcamaf_off)
		ret = pinctrl_select_state(gt9768->vcamaf_pinctrl,
					gt9768->vcamaf_off);

	return ret;
}

static int gt9768_power_on(struct gt9768_device *gt9768)
{
	int ret;

	pr_debug("chengming gt9768_power_on %s\n", __func__);

	ret = regulator_enable(gt9768->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(gt9768->vdd);
	if (ret < 0)
		return ret;

	if (gt9768->vcamaf_pinctrl && gt9768->vcamaf_on)
		ret = pinctrl_select_state(gt9768->vcamaf_pinctrl,
					gt9768->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(GT9768_CTRL_DELAY_US, GT9768_CTRL_DELAY_US + 100);

	ret = gt9768_init(gt9768);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(gt9768->vin);
	regulator_disable(gt9768->vdd);
	if (gt9768->vcamaf_pinctrl && gt9768->vcamaf_off) {
		pinctrl_select_state(gt9768->vcamaf_pinctrl,
				gt9768->vcamaf_off);
	}

	return ret;
}

static int gt9768_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct gt9768_device *gt9768 = to_gt9768_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = gt9768_set_position(gt9768, ctrl->val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops gt9768_vcm_ctrl_ops = {
	.s_ctrl = gt9768_set_ctrl,
};

static int gt9768_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct gt9768_device *gt9768 = sd_to_gt9768_vcm(sd);

	LOG_INF("%s\n", __func__);

	ret = gt9768_power_on(gt9768);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int gt9768_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gt9768_device *gt9768 = sd_to_gt9768_vcm(sd);

	LOG_INF("%s\n", __func__);

	gt9768_power_off(gt9768);

	return 0;
}

static const struct v4l2_subdev_internal_ops gt9768_int_ops = {
	.open = gt9768_open,
	.close = gt9768_close,
};

static const struct v4l2_subdev_ops gt9768_ops = { };

static void gt9768_subdev_cleanup(struct gt9768_device *gt9768)
{
	v4l2_async_unregister_subdev(&gt9768->sd);
	v4l2_ctrl_handler_free(&gt9768->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&gt9768->sd.entity);
#endif
}

static int gt9768_init_controls(struct gt9768_device *gt9768)
{
	struct v4l2_ctrl_handler *hdl = &gt9768->ctrls;
	const struct v4l2_ctrl_ops *ops = &gt9768_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	gt9768->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, GT9768_MAX_FOCUS_POS, GT9768_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	gt9768->sd.ctrl_handler = hdl;

	return 0;
}

static int gt9768_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct gt9768_device *gt9768;
	int ret;

	pr_debug("chengming %s\n", __func__);

	gt9768 = devm_kzalloc(dev, sizeof(*gt9768), GFP_KERNEL);
	if (!gt9768)
		return -ENOMEM;

	gt9768->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(gt9768->vin)) {
		ret = PTR_ERR(gt9768->vin);
		if (ret != -EPROBE_DEFER)
			pr_debug("cannot get vin regulator\n");
		return ret;
	}

	gt9768->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(gt9768->vdd)) {
		ret = PTR_ERR(gt9768->vdd);
		if (ret != -EPROBE_DEFER)
			pr_debug("cannot get vdd regulator\n");
		return ret;
	}

	gt9768->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(gt9768->vcamaf_pinctrl)) {
		ret = PTR_ERR(gt9768->vcamaf_pinctrl);
		gt9768->vcamaf_pinctrl = NULL;
		pr_debug("chengming cannot get pinctrl\n");
	} else {
		gt9768->vcamaf_on = pinctrl_lookup_state(
			gt9768->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(gt9768->vcamaf_on)) {
			ret = PTR_ERR(gt9768->vcamaf_on);
			gt9768->vcamaf_on = NULL;
			pr_debug("chengming cannot get vcamaf_on pinctrl\n");
		}

		gt9768->vcamaf_off = pinctrl_lookup_state(
			gt9768->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(gt9768->vcamaf_off)) {
			ret = PTR_ERR(gt9768->vcamaf_off);
			gt9768->vcamaf_off = NULL;
			pr_debug("chengming cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&gt9768->sd, client, &gt9768_ops);
	gt9768->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	gt9768->sd.internal_ops = &gt9768_int_ops;

	ret = gt9768_init_controls(gt9768);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&gt9768->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	gt9768->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&gt9768->sd);
	if (ret < 0)
		goto err_cleanup;

	return 0;

err_cleanup:
	gt9768_subdev_cleanup(gt9768);
	return ret;
}

static int gt9768_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gt9768_device *gt9768 = sd_to_gt9768_vcm(sd);

	LOG_INF("%s\n", __func__);

	gt9768_subdev_cleanup(gt9768);

	return 0;
}

static const struct i2c_device_id gt9768_id_table[] = {
	{ GT9768_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, gt9768_id_table);

static const struct of_device_id gt9768_of_table[] = {
	{ .compatible = "mediatek,gt9768" },
	{ },
};
MODULE_DEVICE_TABLE(of, gt9768_of_table);

static struct i2c_driver gt9768_i2c_driver = {
	.driver = {
		.name = GT9768_NAME,
		.of_match_table = gt9768_of_table,
	},
	.probe_new  = gt9768_probe,
	.remove = gt9768_remove,
	.id_table = gt9768_id_table,
};

module_i2c_driver(gt9768_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("GT9768 VCM driver");
MODULE_LICENSE("GPL v2");
