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

#define DRIVER_NAME "vl53l0"

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define VL53L0_NAME				"laser"

#define VL53L0_CTRL_DELAY_US			5000

/* vl53l0 device structure */
struct vl53l0_device {
	int driver_init;
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

#define NUMBER_OF_MAX_TOF_DATA 64
struct TofInformation {
	int32_t is_tof_supported;
	int32_t num_of_rows; /* Max : 8 */
	int32_t num_of_cols; /* Max : 8 */
	int32_t ranging_distance[NUMBER_OF_MAX_TOF_DATA];
	int32_t dmax_distance[NUMBER_OF_MAX_TOF_DATA];
	int32_t error_status[NUMBER_OF_MAX_TOF_DATA];
	int32_t maximal_distance; /* Operating Range Distance */
	int64_t timestamp;
};

struct mtk_tof_info {
	struct TofInformation *p_tof_info;
};

/* Control commnad */
#define VIDIOC_MTK_G_TOF_INFO _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_tof_info)


static inline struct vl53l0_device *to_vl53l0_ois(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct vl53l0_device, ctrls);
}

static inline struct vl53l0_device *sd_to_vl53l0_ois(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vl53l0_device, sd);
}

static int vl53l0_release(struct vl53l0_device *vl53l0)
{
	return 0;
}

static int vl53l0_init(struct vl53l0_device *vl53l0)
{
	struct i2c_client *client = v4l2_get_subdevdata(&vl53l0->sd);

	LOG_INF("[%s] %p\n", __func__, client);

	return 0;
}

/* Power handling */
static int vl53l0_power_off(struct vl53l0_device *vl53l0)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = vl53l0_release(vl53l0);
	if (ret)
		LOG_INF("vl53l0 release failed!\n");

	ret = regulator_disable(vl53l0->vin);
	if (ret)
		return ret;

	ret = regulator_disable(vl53l0->vdd);
	if (ret)
		return ret;

	if (vl53l0->vcamaf_pinctrl && vl53l0->vcamaf_off)
		ret = pinctrl_select_state(vl53l0->vcamaf_pinctrl,
					vl53l0->vcamaf_off);

	return ret;
}

static int vl53l0_power_on(struct vl53l0_device *vl53l0)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(vl53l0->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(vl53l0->vdd);
	if (ret < 0)
		return ret;

	if (vl53l0->vcamaf_pinctrl && vl53l0->vcamaf_on)
		ret = pinctrl_select_state(vl53l0->vcamaf_pinctrl,
					vl53l0->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	/*
	 * Execute driver initialization in the first time getting
	 * TOF info to avoid increase preview launch waiting time
	 */
	vl53l0->driver_init = 1;
	/*
	usleep_range(VL53L0_CTRL_DELAY_US, VL53L0_CTRL_DELAY_US + 100);

	ret = vl53l0_init(vl53l0);
	if (ret < 0)
		goto fail;
	 */

	return 0;
/*
fail:
	regulator_disable(vl53l0->vin);
	regulator_disable(vl53l0->vdd);
	if (vl53l0->vcamaf_pinctrl && vl53l0->vcamaf_off) {
		pinctrl_select_state(vl53l0->vcamaf_pinctrl,
				vl53l0->vcamaf_off);
	}

	return ret;
*/
}

static int vl53l0_set_ctrl(struct v4l2_ctrl *ctrl)
{
	/* struct vl53l0_device *vl53l0 = to_vl53l0_ois(ctrl); */

	return 0;
}

static const struct v4l2_ctrl_ops vl53l0_ois_ctrl_ops = {
	.s_ctrl = vl53l0_set_ctrl,
};

static int vl53l0_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
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

static int vl53l0_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	LOG_INF("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static long vl53l0_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct vl53l0_device *vl53l0 = sd_to_vl53l0_ois(sd);

	switch (cmd) {

	case VIDIOC_MTK_G_TOF_INFO:
	{
		struct mtk_tof_info *info = arg;
		struct TofInformation tof_info;

		/*
		 * Execute driver initialization in the first time getting
		 * TOF info to avoid increase preview launch waiting time
		 */
		if (vl53l0->driver_init == 1) {
			ret = vl53l0_init(vl53l0);
			if (ret < 0)
				return ret;

			vl53l0->driver_init = 0;
		}

		memset(&tof_info, 0, sizeof(struct TofInformation));

		LOG_INF("%s\n", __func__);
		tof_info.is_tof_supported = 1;
		tof_info.num_of_rows = 1;
		tof_info.num_of_cols = 1;
		tof_info.maximal_distance = 1500; /* Unit : mm */

		/* To Do */
		LOG_INF("[%s] (%d, %d), dist(%d, %d, %d), err(%d)\n",
			__func__, tof_info.num_of_rows, tof_info.num_of_cols,
			tof_info.ranging_distance[0], tof_info.dmax_distance[0],
			tof_info.maximal_distance, tof_info.error_status[0]);

		if (copy_to_user((void *)info->p_tof_info, &tof_info, sizeof(tof_info)))
			ret = -EFAULT;
	}
	break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_internal_ops vl53l0_int_ops = {
	.open = vl53l0_open,
	.close = vl53l0_close,
};

static struct v4l2_subdev_core_ops vl53l0_ops_core = {
	.ioctl = vl53l0_ops_core_ioctl,
};

static const struct v4l2_subdev_ops vl53l0_ops = {
	.core = &vl53l0_ops_core,
};

static void vl53l0_subdev_cleanup(struct vl53l0_device *vl53l0)
{
	v4l2_async_unregister_subdev(&vl53l0->sd);
	v4l2_ctrl_handler_free(&vl53l0->ctrls);
#if IS_ENABLE(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&vl53l0->sd.entity);
#endif
}

static int vl53l0_init_controls(struct vl53l0_device *vl53l0)
{
	struct v4l2_ctrl_handler *hdl = &vl53l0->ctrls;
	/* const struct v4l2_ctrl_ops *ops = &vl53l0_ois_ctrl_ops; */

	v4l2_ctrl_handler_init(hdl, 1);

	if (hdl->error)
		return hdl->error;

	vl53l0->sd.ctrl_handler = hdl;

	return 0;
}

static int vl53l0_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct vl53l0_device *vl53l0;
	int ret;

	LOG_INF("%s\n", __func__);

	vl53l0 = devm_kzalloc(dev, sizeof(*vl53l0), GFP_KERNEL);
	if (!vl53l0)
		return -ENOMEM;

	vl53l0->vin = devm_regulator_get(dev, "camera_tof_vin");
	if (IS_ERR(vl53l0->vin)) {
		ret = PTR_ERR(vl53l0->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	vl53l0->vdd = devm_regulator_get(dev, "camera_tof_vdd");
	if (IS_ERR(vl53l0->vdd)) {
		ret = PTR_ERR(vl53l0->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	vl53l0->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(vl53l0->vcamaf_pinctrl)) {
		ret = PTR_ERR(vl53l0->vcamaf_pinctrl);
		vl53l0->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		vl53l0->vcamaf_on = pinctrl_lookup_state(
			vl53l0->vcamaf_pinctrl, "camera_tof_en_on");

		if (IS_ERR(vl53l0->vcamaf_on)) {
			ret = PTR_ERR(vl53l0->vcamaf_on);
			vl53l0->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		vl53l0->vcamaf_off = pinctrl_lookup_state(
			vl53l0->vcamaf_pinctrl, "camera_tof_en_off");

		if (IS_ERR(vl53l0->vcamaf_off)) {
			ret = PTR_ERR(vl53l0->vcamaf_off);
			vl53l0->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&vl53l0->sd, client, &vl53l0_ops);
	vl53l0->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	vl53l0->sd.internal_ops = &vl53l0_int_ops;

	ret = vl53l0_init_controls(vl53l0);
	if (ret)
		goto err_cleanup;

#if IS_ENABLE(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&vl53l0->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	vl53l0->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&vl53l0->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	vl53l0_subdev_cleanup(vl53l0);
	return ret;
}

static int vl53l0_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct vl53l0_device *vl53l0 = sd_to_vl53l0_ois(sd);

	LOG_INF("%s\n", __func__);

	vl53l0_subdev_cleanup(vl53l0);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		vl53l0_power_off(vl53l0);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int __maybe_unused vl53l0_ois_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct vl53l0_device *vl53l0 = sd_to_vl53l0_ois(sd);

	return vl53l0_power_off(vl53l0);
}

static int __maybe_unused vl53l0_ois_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct vl53l0_device *vl53l0 = sd_to_vl53l0_ois(sd);

	return vl53l0_power_on(vl53l0);
}

static const struct i2c_device_id vl53l0_id_table[] = {
	{ VL53L0_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, vl53l0_id_table);

static const struct of_device_id vl53l0_of_table[] = {
	{ .compatible = "mediatek,vl53l0" },
	{ },
};
MODULE_DEVICE_TABLE(of, vl53l0_of_table);

static const struct dev_pm_ops vl53l0_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(vl53l0_ois_suspend, vl53l0_ois_resume, NULL)
};

static struct i2c_driver vl53l0_i2c_driver = {
	.driver = {
		.name = VL53L0_NAME,
		.pm = &vl53l0_pm_ops,
		.of_match_table = vl53l0_of_table,
	},
	.probe_new  = vl53l0_probe,
	.remove = vl53l0_remove,
	.id_table = vl53l0_id_table,
};

module_i2c_driver(vl53l0_i2c_driver);

MODULE_AUTHOR("Sam Hung <Sam.Hung@mediatek.com>");
MODULE_DESCRIPTION("VL53L0 TOF driver");
MODULE_LICENSE("GPL v2");
