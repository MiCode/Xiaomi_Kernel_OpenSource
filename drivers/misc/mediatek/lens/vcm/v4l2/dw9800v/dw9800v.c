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

#define DRIVER_NAME "dw9800v"
#define DW9800V_I2C_SLAVE_ADDR 0x18
#define AK7314_I2C_SLAVE_ADDR  0x18
#define EEPROM_I2C_SLAVE_ADDR  0xA2


#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define DW9800V_NAME				"dw9800v"
#define DW9800V_MAX_FOCUS_POS			1023
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define DW9800V_FOCUS_STEPS			1
#define DW9800V_SET_POSITION_ADDR		0x03
#define AK7314_SET_POSITION_ADDR		0x00


#define DW9800V_CMD_DELAY			0xff
#define DW9800V_CTRL_DELAY_US			10000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define DW9800V_MOVE_STEPS			16
#define DW9800V_MOVE_DELAY_US			8400
#define DW9800V_STABLE_TIME_US			20000

/* dw9800v device structure */
struct dw9800v_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

static int g_vendor_id;

static int read_vendor_id(struct i2c_client *client, u16 a_u2Addr)
{
	u8 vendorID = 0xFF;
	int i4RetValue = 0;
	char puReadCmd[2] = {(char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF)};

	client->addr = EEPROM_I2C_SLAVE_ADDR;
	client->addr = client->addr >> 1;

	i4RetValue = i2c_master_send(client, puReadCmd, 2);
	if (i4RetValue < 0) {
		LOG_INF(" I2C write failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(client, (char *)&vendorID, 1);
	if (i4RetValue != 1) {
		LOG_INF(" I2C read failed!!\n");
		return -1;
	}

	client->addr = DW9800V_I2C_SLAVE_ADDR;
	client->addr = client->addr >> 1;

	return vendorID;
}


static inline struct dw9800v_device *to_dw9800v_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct dw9800v_device, ctrls);
}

static inline struct dw9800v_device *sd_to_dw9800v_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct dw9800v_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int dw9800v_set_position(struct dw9800v_device *dw9800v, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9800v->sd);
	int dac_addr   = (g_vendor_id == 0x03/*semco*/) ?
		AK7314_SET_POSITION_ADDR : DW9800V_SET_POSITION_ADDR;
	int data_shift = (g_vendor_id == 0x03/*semco*/) ? 6 : 0;

	return i2c_smbus_write_word_data(client, dac_addr, swab16(val << data_shift));
}

static int dw9800v_release(struct dw9800v_device *dw9800v)
{
	int ret, val;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9800v->sd);

	for (val = round_down(dw9800v->focus->val, DW9800V_MOVE_STEPS);
	     val >= 0; val -= DW9800V_MOVE_STEPS) {
		ret = dw9800v_set_position(dw9800v, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(DW9800V_MOVE_DELAY_US,
			     DW9800V_MOVE_DELAY_US + 1000);
	}

	i2c_smbus_write_byte_data(client, 0x02, 0x20);
	msleep(20);

	/*
	 * Wait for the motor to stabilize after the last movement
	 * to prevent the motor from shaking.
	 */
	usleep_range(DW9800V_STABLE_TIME_US - DW9800V_MOVE_DELAY_US,
		     DW9800V_STABLE_TIME_US - DW9800V_MOVE_DELAY_US + 1000);

	return 0;
}

static int dw9800v_init(struct dw9800v_device *dw9800v)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9800v->sd);
	int ret = 0;
	int i = 0;
	unsigned char cmd_number = 7;
#if defined(MATISSE_CAM)
	char puSendCmdArray[8][2] = {
	{0x02, 0x01}, {0x02, 0x00}, {0xFE, 0xFE},
	{0x02, 0x02}, {0x06, 0x40}, {0x07, 0x07}, {0x10, 0x01}, {0xFE, 0xFE},
	};
	cmd_number = 8;
#elif defined(RUBENS_CAM)
	char puSendCmdArray[7][2] = {
	{0x02, 0x01}, {0x02, 0x00}, {0xFE, 0xFE},
	{0x02, 0x02}, {0x06, 0x80}, {0x07, 0x7C}, {0xFE, 0xFE},
	};
#else
	char puSendCmdArray[7][2] = {
	{0x02, 0x01}, {0x02, 0x00}, {0xFE, 0xFE},
	{0x02, 0x02}, {0x06, 0x80}, {0x07, 0x7C}, {0xFE, 0xFE},
	};
#endif

	client->addr = DW9800V_I2C_SLAVE_ADDR >> 1;
	//ret = i2c_smbus_read_byte_data(client, 0x02);

	LOG_INF("Check HW version: %x\n", ret);

	for (i = 0; i < cmd_number; i++) {
		if (puSendCmdArray[i][0] != 0xFE) {
			LOG_INF("0x%02x <= 0x%02x\n", puSendCmdArray[i][0],
				puSendCmdArray[i][1]);
			ret = i2c_smbus_write_byte_data(client,
					puSendCmdArray[i][0],
					puSendCmdArray[i][1]);

			if (ret < 0)
				return -1;
		} else {
			udelay(100);
		}
	}

	return ret;
}

#ifdef MATISSE_CAM
static int ak7314_init(struct dw9800v_device *dw9800v)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9800v->sd);
	int ret = 0;

	LOG_INF("+\n");

	client->addr = AK7314_I2C_SLAVE_ADDR >> 1;
	//ret = i2c_smbus_read_byte_data(client, 0x02);

	LOG_INF("Check HW version: %x\n", ret);

	/* 00:active mode , 10:Standby mode , x1:Sleep mode */
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00);

	LOG_INF("-\n");

	return ret;
}
#endif

/* Power handling */
static int dw9800v_power_off(struct dw9800v_device *dw9800v)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = dw9800v_release(dw9800v);
	if (ret)
		LOG_INF("dw9800v release failed!\n");

	ret = regulator_disable(dw9800v->vin);
	if (ret)
		return ret;

	ret = regulator_disable(dw9800v->vdd);
	if (ret)
		return ret;

	if (dw9800v->vcamaf_pinctrl && dw9800v->vcamaf_off)
		ret = pinctrl_select_state(dw9800v->vcamaf_pinctrl,
					dw9800v->vcamaf_off);

	return ret;
}

static int dw9800v_power_on(struct dw9800v_device *dw9800v)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9800v->sd);

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(dw9800v->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(dw9800v->vdd);
	if (ret < 0)
		return ret;

	if (dw9800v->vcamaf_pinctrl && dw9800v->vcamaf_on)
		ret = pinctrl_select_state(dw9800v->vcamaf_pinctrl,
					dw9800v->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(DW9800V_CTRL_DELAY_US, DW9800V_CTRL_DELAY_US + 100);

	g_vendor_id = read_vendor_id(client, 0x01);
	LOG_INF("vendor id: %x\n", g_vendor_id);

#ifdef MATISSE_CAM
	if (g_vendor_id == 0x03 /* semco */) {
		ret = ak7314_init(dw9800v);
	} else { /* ofilm */
		ret = dw9800v_init(dw9800v);
	}
#else
	ret = dw9800v_init(dw9800v);
#endif
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(dw9800v->vin);
	regulator_disable(dw9800v->vdd);
	if (dw9800v->vcamaf_pinctrl && dw9800v->vcamaf_off) {
		pinctrl_select_state(dw9800v->vcamaf_pinctrl,
				dw9800v->vcamaf_off);
	}

	return ret;
}

static int dw9800v_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct dw9800v_device *dw9800v = to_dw9800v_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		return dw9800v_set_position(dw9800v, ctrl->val);
	}
	return 0;
}

static const struct v4l2_ctrl_ops dw9800v_vcm_ctrl_ops = {
	.s_ctrl = dw9800v_set_ctrl,
};

static int dw9800v_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
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

static int dw9800v_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	LOG_INF("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops dw9800v_int_ops = {
	.open = dw9800v_open,
	.close = dw9800v_close,
};

static const struct v4l2_subdev_ops dw9800v_ops = { };

static void dw9800v_subdev_cleanup(struct dw9800v_device *dw9800v)
{
	v4l2_async_unregister_subdev(&dw9800v->sd);
	v4l2_ctrl_handler_free(&dw9800v->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&dw9800v->sd.entity);
#endif
}

static int dw9800v_init_controls(struct dw9800v_device *dw9800v)
{
	struct v4l2_ctrl_handler *hdl = &dw9800v->ctrls;
	const struct v4l2_ctrl_ops *ops = &dw9800v_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	dw9800v->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, DW9800V_MAX_FOCUS_POS, DW9800V_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	dw9800v->sd.ctrl_handler = hdl;

	return 0;
}

static int dw9800v_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct dw9800v_device *dw9800v;
	int ret;

	LOG_INF("%s\n", __func__);

	dw9800v = devm_kzalloc(dev, sizeof(*dw9800v), GFP_KERNEL);
	if (!dw9800v)
		return -ENOMEM;

	dw9800v->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(dw9800v->vin)) {
		ret = PTR_ERR(dw9800v->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	dw9800v->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(dw9800v->vdd)) {
		ret = PTR_ERR(dw9800v->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	dw9800v->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(dw9800v->vcamaf_pinctrl)) {
		ret = PTR_ERR(dw9800v->vcamaf_pinctrl);
		dw9800v->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		dw9800v->vcamaf_on = pinctrl_lookup_state(
			dw9800v->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(dw9800v->vcamaf_on)) {
			ret = PTR_ERR(dw9800v->vcamaf_on);
			dw9800v->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		dw9800v->vcamaf_off = pinctrl_lookup_state(
			dw9800v->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(dw9800v->vcamaf_off)) {
			ret = PTR_ERR(dw9800v->vcamaf_off);
			dw9800v->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&dw9800v->sd, client, &dw9800v_ops);
	dw9800v->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dw9800v->sd.internal_ops = &dw9800v_int_ops;

	ret = dw9800v_init_controls(dw9800v);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&dw9800v->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	dw9800v->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&dw9800v->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	dw9800v_subdev_cleanup(dw9800v);
	return ret;
}

static int dw9800v_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9800v_device *dw9800v = sd_to_dw9800v_vcm(sd);

	LOG_INF("%s\n", __func__);

	dw9800v_subdev_cleanup(dw9800v);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		dw9800v_power_off(dw9800v);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int __maybe_unused dw9800v_vcm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9800v_device *dw9800v = sd_to_dw9800v_vcm(sd);

	return dw9800v_power_off(dw9800v);
}

static int __maybe_unused dw9800v_vcm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9800v_device *dw9800v = sd_to_dw9800v_vcm(sd);

	return dw9800v_power_on(dw9800v);
}

static const struct i2c_device_id dw9800v_id_table[] = {
	{ DW9800V_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, dw9800v_id_table);

static const struct of_device_id dw9800v_of_table[] = {
	{ .compatible = "mediatek,dw9800v" },
	{ },
};
MODULE_DEVICE_TABLE(of, dw9800v_of_table);

static const struct dev_pm_ops dw9800v_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(dw9800v_vcm_suspend, dw9800v_vcm_resume, NULL)
};

static struct i2c_driver dw9800v_i2c_driver = {
	.driver = {
		.name = DW9800V_NAME,
		.pm = &dw9800v_pm_ops,
		.of_match_table = dw9800v_of_table,
	},
	.probe_new  = dw9800v_probe,
	.remove = dw9800v_remove,
	.id_table = dw9800v_id_table,
};

module_i2c_driver(dw9800v_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("DW9800V VCM driver");
MODULE_LICENSE("GPL v2");
