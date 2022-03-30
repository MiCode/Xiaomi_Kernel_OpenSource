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

#define DW9718_NAME				"dw9718"
#define DW9718_MAX_FOCUS_POS			1023
#define DW9718_ORIGIN_FOCUS_POS			0
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
#define DW9718_MOVE_STEPS			100
#define DW9718_MOVE_DELAY_US			5000

/* dw9718 device structure */
struct dw9718_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
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
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9718->sd);

	diff_dac = DW9718_ORIGIN_FOCUS_POS - dw9718->focus->val;

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		DW9718_MOVE_STEPS;

	val = dw9718->focus->val;

	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (DW9718_MOVE_STEPS*(-1)) : DW9718_MOVE_STEPS);

		ret = dw9718_set_position(dw9718, val);
		if (ret) {
			pr_info("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(DW9718_MOVE_DELAY_US,
			     DW9718_MOVE_DELAY_US + 1000);
	}

	// last step to origin
	ret = dw9718_set_position(dw9718, DW9718_ORIGIN_FOCUS_POS);
	if (ret) {
		pr_info("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}

	ret = i2c_smbus_write_byte_data(client, DW9718_CONTROL_REG,
					DW9718_CONTROL_POWER_DOWN);
	if (ret)
		return ret;

	pr_info("%s -\n", __func__);

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

	ret = regulator_disable(dw9718->vdd);
	if (ret)
		return ret;

	if (dw9718->vcamaf_pinctrl && dw9718->vcamaf_off)
		ret = pinctrl_select_state(dw9718->vcamaf_pinctrl,
					dw9718->vcamaf_off);

	return ret;
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

	if (dw9718->vcamaf_pinctrl && dw9718->vcamaf_on)
		ret = pinctrl_select_state(dw9718->vcamaf_pinctrl,
					dw9718->vcamaf_on);

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
	if (dw9718->vcamaf_pinctrl && dw9718->vcamaf_off) {
		pinctrl_select_state(dw9718->vcamaf_pinctrl,
				dw9718->vcamaf_off);
	}

	return ret;
}

static int dw9718_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct dw9718_device *dw9718 = to_dw9718_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		ret = dw9718_set_position(dw9718, ctrl->val);
		if (ret) {
			pr_info("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops dw9718_vcm_ctrl_ops = {
	.s_ctrl = dw9718_set_ctrl,
};

static int dw9718_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct dw9718_device *dw9718 = sd_to_dw9718_vcm(sd);

	pr_info("%s\n", __func__);

	ret = dw9718_power_on(dw9718);
	if (ret < 0) {
		pr_info("%s power on fail, ret = %d",
			__func__, ret);
		return ret;
	}

	return 0;
}

static int dw9718_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct dw9718_device *dw9718 = sd_to_dw9718_vcm(sd);

	pr_info("%s\n", __func__);

	dw9718_power_off(dw9718);

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

	dw9718->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(dw9718->vcamaf_pinctrl)) {
		ret = PTR_ERR(dw9718->vcamaf_pinctrl);
		dw9718->vcamaf_pinctrl = NULL;
		pr_info("cannot get pinctrl\n");
	} else {
		dw9718->vcamaf_on = pinctrl_lookup_state(
			dw9718->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(dw9718->vcamaf_on)) {
			ret = PTR_ERR(dw9718->vcamaf_on);
			dw9718->vcamaf_on = NULL;
			pr_info("cannot get vcamaf_on pinctrl\n");
		}

		dw9718->vcamaf_off = pinctrl_lookup_state(
			dw9718->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(dw9718->vcamaf_off)) {
			ret = PTR_ERR(dw9718->vcamaf_off);
			dw9718->vcamaf_off = NULL;
			pr_info("cannot get vcamaf_off pinctrl\n");
		}
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

	return 0;
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

static struct i2c_driver dw9718_i2c_driver = {
	.driver = {
		.name = DW9718_NAME,
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
