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

#define DRIVER_NAME "bu63169"

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define BU63169_NAME				"bu63169"

#define BU63169_CTRL_DELAY_US			5000

/* bu63169 device structure */
struct bu63169_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};


#define OIS_DATA_NUMBER 32
struct OisInfo {
	int32_t is_ois_supported;
	int32_t data_mode;  /* ON/OFF */
	int32_t samples;
	int32_t x_shifts[OIS_DATA_NUMBER];
	int32_t y_shifts[OIS_DATA_NUMBER];
	int64_t timestamps[OIS_DATA_NUMBER];
};

struct mtk_ois_pos_info {
	struct OisInfo *p_ois_info;
};

/* Control commnad */
#define VIDIOC_MTK_S_OIS_MODE _IOW('V', BASE_VIDIOC_PRIVATE + 2, int32_t)

#define VIDIOC_MTK_G_OIS_POS_INFO _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_ois_pos_info)


static inline struct bu63169_device *to_bu63169_ois(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct bu63169_device, ctrls);
}

static inline struct bu63169_device *sd_to_bu63169_ois(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct bu63169_device, sd);
}

static int bu63169_release(struct bu63169_device *bu63169)
{
	return 0;
}

static int bu63169_init(struct bu63169_device *bu63169)
{
	/* struct i2c_client *client = v4l2_get_subdevdata(&bu63169->sd); */
	return 0;
}

/* Power handling */
static int bu63169_power_off(struct bu63169_device *bu63169)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = bu63169_release(bu63169);
	if (ret)
		LOG_INF("bu63169 release failed!\n");

	ret = regulator_disable(bu63169->vin);
	if (ret)
		return ret;

	ret = regulator_disable(bu63169->vdd);
	if (ret)
		return ret;

	if (bu63169->vcamaf_pinctrl && bu63169->vcamaf_off)
		ret = pinctrl_select_state(bu63169->vcamaf_pinctrl,
					bu63169->vcamaf_off);

	return ret;
}

static int bu63169_power_on(struct bu63169_device *bu63169)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(bu63169->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(bu63169->vdd);
	if (ret < 0)
		return ret;

	if (bu63169->vcamaf_pinctrl && bu63169->vcamaf_on)
		ret = pinctrl_select_state(bu63169->vcamaf_pinctrl,
					bu63169->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(BU63169_CTRL_DELAY_US, BU63169_CTRL_DELAY_US + 100);

	ret = bu63169_init(bu63169);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(bu63169->vin);
	regulator_disable(bu63169->vdd);
	if (bu63169->vcamaf_pinctrl && bu63169->vcamaf_off) {
		pinctrl_select_state(bu63169->vcamaf_pinctrl,
				bu63169->vcamaf_off);
	}

	return ret;
}

static int bu63169_set_ctrl(struct v4l2_ctrl *ctrl)
{
	/* struct bu63169_device *bu63169 = to_bu63169_ois(ctrl); */

	return 0;
}

static const struct v4l2_ctrl_ops bu63169_ois_ctrl_ops = {
	.s_ctrl = bu63169_set_ctrl,
};

static int bu63169_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
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

static int bu63169_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	LOG_INF("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static long bu63169_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;

	switch (cmd) {

	case VIDIOC_MTK_S_OIS_MODE:
	{
		int *ois_mode = arg;

		if (*ois_mode)
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Enable\n");
		else
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Disable\n");
	}
	break;

	case VIDIOC_MTK_G_OIS_POS_INFO:
	{
		struct mtk_ois_pos_info *info = arg;
		struct OisInfo pos_info;

		memset(&pos_info, 0, sizeof(struct OisInfo));

		/* To Do */

		if (copy_to_user((void *)info->p_ois_info, &pos_info, sizeof(pos_info)))
			ret = -EFAULT;
	}
	break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_internal_ops bu63169_int_ops = {
	.open = bu63169_open,
	.close = bu63169_close,
};

static struct v4l2_subdev_core_ops bu63169_ops_core = {
	.ioctl = bu63169_ops_core_ioctl,
};

static const struct v4l2_subdev_ops bu63169_ops = {
	.core = &bu63169_ops_core,
};

static void bu63169_subdev_cleanup(struct bu63169_device *bu63169)
{
	v4l2_async_unregister_subdev(&bu63169->sd);
	v4l2_ctrl_handler_free(&bu63169->ctrls);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&bu63169->sd.entity);
#endif
}

static int bu63169_init_controls(struct bu63169_device *bu63169)
{
	struct v4l2_ctrl_handler *hdl = &bu63169->ctrls;
	/* const struct v4l2_ctrl_ops *ops = &bu63169_ois_ctrl_ops; */

	v4l2_ctrl_handler_init(hdl, 1);

	if (hdl->error)
		return hdl->error;

	bu63169->sd.ctrl_handler = hdl;

	return 0;
}

static int bu63169_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct bu63169_device *bu63169;
	int ret;

	LOG_INF("%s\n", __func__);

	bu63169 = devm_kzalloc(dev, sizeof(*bu63169), GFP_KERNEL);
	if (!bu63169)
		return -ENOMEM;

	bu63169->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(bu63169->vin)) {
		ret = PTR_ERR(bu63169->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	bu63169->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(bu63169->vdd)) {
		ret = PTR_ERR(bu63169->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	bu63169->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(bu63169->vcamaf_pinctrl)) {
		ret = PTR_ERR(bu63169->vcamaf_pinctrl);
		bu63169->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		bu63169->vcamaf_on = pinctrl_lookup_state(
			bu63169->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(bu63169->vcamaf_on)) {
			ret = PTR_ERR(bu63169->vcamaf_on);
			bu63169->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		bu63169->vcamaf_off = pinctrl_lookup_state(
			bu63169->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(bu63169->vcamaf_off)) {
			ret = PTR_ERR(bu63169->vcamaf_off);
			bu63169->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&bu63169->sd, client, &bu63169_ops);
	bu63169->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	bu63169->sd.internal_ops = &bu63169_int_ops;

	ret = bu63169_init_controls(bu63169);
	if (ret)
		goto err_cleanup;

#if defined(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&bu63169->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	bu63169->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&bu63169->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	bu63169_subdev_cleanup(bu63169);
	return ret;
}

static int bu63169_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bu63169_device *bu63169 = sd_to_bu63169_ois(sd);

	LOG_INF("%s\n", __func__);

	bu63169_subdev_cleanup(bu63169);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		bu63169_power_off(bu63169);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int __maybe_unused bu63169_ois_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bu63169_device *bu63169 = sd_to_bu63169_ois(sd);

	return bu63169_power_off(bu63169);
}

static int __maybe_unused bu63169_ois_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bu63169_device *bu63169 = sd_to_bu63169_ois(sd);

	return bu63169_power_on(bu63169);
}

static const struct i2c_device_id bu63169_id_table[] = {
	{ BU63169_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bu63169_id_table);

static const struct of_device_id bu63169_of_table[] = {
	{ .compatible = "mediatek,bu63169" },
	{ },
};
MODULE_DEVICE_TABLE(of, bu63169_of_table);

static const struct dev_pm_ops bu63169_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(bu63169_ois_suspend, bu63169_ois_resume, NULL)
};

static struct i2c_driver bu63169_i2c_driver = {
	.driver = {
		.name = BU63169_NAME,
		.pm = &bu63169_pm_ops,
		.of_match_table = bu63169_of_table,
	},
	.probe_new  = bu63169_probe,
	.remove = bu63169_remove,
	.id_table = bu63169_id_table,
};

module_i2c_driver(bu63169_i2c_driver);

MODULE_AUTHOR("Sam Hung <Sam.Hung@mediatek.com>");
MODULE_DESCRIPTION("BU63169 VCM driver");
MODULE_LICENSE("GPL v2");
