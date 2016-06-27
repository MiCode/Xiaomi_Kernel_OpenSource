/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/of_gpio.h>
#include "msm_ir_cut.h"
#include "msm_camera_dt_util.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

DEFINE_MSM_MUTEX(msm_ir_cut_mutex);

static struct v4l2_file_operations msm_ir_cut_v4l2_subdev_fops;

static const struct of_device_id msm_ir_cut_dt_match[] = {
	{.compatible = "qcom,ir-cut", .data = NULL},
	{}
};

static struct msm_ir_cut_table msm_gpio_ir_cut_table;

static struct msm_ir_cut_table *ir_cut_table[] = {
	&msm_gpio_ir_cut_table,
};

static int32_t msm_ir_cut_get_subdev_id(
	struct msm_ir_cut_ctrl_t *ir_cut_ctrl, void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;

	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (ir_cut_ctrl->ir_cut_device_type != MSM_CAMERA_PLATFORM_DEVICE) {
		pr_err("failed\n");
		return -EINVAL;
	}

	*subdev_id = ir_cut_ctrl->pdev->id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_ir_cut_init(
	struct msm_ir_cut_ctrl_t *ir_cut_ctrl,
	struct msm_ir_cut_cfg_data_t *ir_cut_data)
{
	int32_t rc = 0;

	CDBG("Enter");

	rc = ir_cut_ctrl->func_tbl->camera_ir_cut_on(ir_cut_ctrl, ir_cut_data);

	CDBG("Exit");
	return rc;
}

static int32_t msm_ir_cut_release(
	struct msm_ir_cut_ctrl_t *ir_cut_ctrl)
{
	int32_t rc = 0;

	if (ir_cut_ctrl->ir_cut_state == MSM_CAMERA_IR_CUT_RELEASE) {
		pr_err("%s:%d Invalid ir_cut state = %d",
			__func__, __LINE__, ir_cut_ctrl->ir_cut_state);
		return 0;
	}

	rc = ir_cut_ctrl->func_tbl->camera_ir_cut_on(ir_cut_ctrl, NULL);
	if (rc < 0) {
		pr_err("%s:%d camera_ir_cut_on failed rc = %d",
			__func__, __LINE__, rc);
		return rc;
	}
	ir_cut_ctrl->ir_cut_state = MSM_CAMERA_IR_CUT_RELEASE;
	return 0;
}

static int32_t msm_ir_cut_off(struct msm_ir_cut_ctrl_t *ir_cut_ctrl,
	struct msm_ir_cut_cfg_data_t *ir_cut_data)
{
	int rc = 0;

	CDBG("Enter cut off\n");

	if (ir_cut_ctrl->gconf) {
		rc = msm_camera_request_gpio_table(
			ir_cut_ctrl->gconf->cam_gpio_req_tbl,
			ir_cut_ctrl->gconf->cam_gpio_req_tbl_size, 1);

		if (rc < 0) {
			pr_err("ERR:%s:Failed in selecting state: %d\n",
				__func__, rc);

			return rc;
		}
	} else {
		pr_err("%s: No IR CUT GPIOs\n", __func__);
		return 0;
	}

	if (ir_cut_ctrl->cam_pinctrl_status) {
		rc = pinctrl_select_state(
			ir_cut_ctrl->pinctrl_info.pinctrl,
			ir_cut_ctrl->pinctrl_info.gpio_state_active);

		if (rc < 0)
			pr_err("ERR:%s:%d cannot set pin to active state: %d",
				__func__, __LINE__, rc);
	}

	CDBG("ERR:%s:gpio_conf->gpio_num_info->gpio_num[0] = %d",
		__func__,
		ir_cut_ctrl->gconf->gpio_num_info->
			gpio_num[IR_CUT_FILTER_GPIO_P]);

	CDBG("ERR:%s:gpio_conf->gpio_num_info->gpio_num[1] = %d",
		__func__,
		ir_cut_ctrl->gconf->gpio_num_info->
			gpio_num[IR_CUT_FILTER_GPIO_M]);

	gpio_set_value_cansleep(
		ir_cut_ctrl->gconf->gpio_num_info->
			gpio_num[IR_CUT_FILTER_GPIO_P],
		0);

	gpio_set_value_cansleep(
		ir_cut_ctrl->gconf->gpio_num_info->
			gpio_num[IR_CUT_FILTER_GPIO_M],
		1);

	if (ir_cut_ctrl->gconf) {
		rc = msm_camera_request_gpio_table(
			ir_cut_ctrl->gconf->cam_gpio_req_tbl,
			ir_cut_ctrl->gconf->cam_gpio_req_tbl_size, 0);

		if (rc < 0) {
			pr_err("ERR:%s:Failed in selecting state: %d\n",
				__func__, rc);

			return rc;
		}
	} else {
		pr_err("%s: No IR CUT GPIOs\n", __func__);
		return 0;
	}

	CDBG("Exit\n");
	return 0;
}

static int32_t msm_ir_cut_on(
	struct msm_ir_cut_ctrl_t *ir_cut_ctrl,
	struct msm_ir_cut_cfg_data_t *ir_cut_data)
{
	int rc = 0;

	CDBG("Enter ir cut on\n");

	if (ir_cut_ctrl->gconf) {
		rc = msm_camera_request_gpio_table(
			ir_cut_ctrl->gconf->cam_gpio_req_tbl,
			ir_cut_ctrl->gconf->cam_gpio_req_tbl_size, 1);

		if (rc < 0) {
			pr_err("ERR:%s:Failed in selecting state: %d\n",
				__func__, rc);

			return rc;
		}
	} else {
		pr_err("%s: No IR CUT GPIOs\n", __func__);
		return 0;
	}

	if (ir_cut_ctrl->cam_pinctrl_status) {
		rc = pinctrl_select_state(
			ir_cut_ctrl->pinctrl_info.pinctrl,
			ir_cut_ctrl->pinctrl_info.gpio_state_active);

		if (rc < 0)
			pr_err("ERR:%s:%d cannot set pin to active state: %d",
				__func__, __LINE__, rc);
	}

	CDBG("ERR:%s: gpio_conf->gpio_num_info->gpio_num[0] = %d",
		__func__,
		ir_cut_ctrl->gconf->gpio_num_info->
			gpio_num[IR_CUT_FILTER_GPIO_P]);

	CDBG("ERR:%s: gpio_conf->gpio_num_info->gpio_num[1] = %d",
		__func__,
		ir_cut_ctrl->gconf->gpio_num_info->
			gpio_num[IR_CUT_FILTER_GPIO_M]);

	gpio_set_value_cansleep(
		ir_cut_ctrl->gconf->gpio_num_info->
			gpio_num[IR_CUT_FILTER_GPIO_P],
		1);

	gpio_set_value_cansleep(
		ir_cut_ctrl->gconf->
			gpio_num_info->gpio_num[IR_CUT_FILTER_GPIO_M],
		1);

	if (ir_cut_ctrl->gconf) {
		rc = msm_camera_request_gpio_table(
			ir_cut_ctrl->gconf->cam_gpio_req_tbl,
			ir_cut_ctrl->gconf->cam_gpio_req_tbl_size, 0);

		if (rc < 0) {
			pr_err("ERR:%s:Failed in selecting state: %d\n",
				__func__, rc);

			return rc;
		}
	} else {
		pr_err("%s: No IR CUT GPIOs\n", __func__);
		return 0;
	}
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_ir_cut_handle_init(
	struct msm_ir_cut_ctrl_t *ir_cut_ctrl,
	struct msm_ir_cut_cfg_data_t *ir_cut_data)
{
	uint32_t i = 0;
	int32_t rc = -EFAULT;
	enum msm_ir_cut_driver_type ir_cut_driver_type =
		ir_cut_ctrl->ir_cut_driver_type;

	CDBG("Enter");

	if (ir_cut_ctrl->ir_cut_state == MSM_CAMERA_IR_CUT_INIT) {
		pr_err("%s:%d Invalid ir_cut state = %d",
			__func__, __LINE__, ir_cut_ctrl->ir_cut_state);
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(ir_cut_table); i++) {
		if (ir_cut_driver_type == ir_cut_table[i]->ir_cut_driver_type) {
			ir_cut_ctrl->func_tbl = &ir_cut_table[i]->func_tbl;
			rc = 0;
			break;
		}
	}

	if (rc < 0) {
		pr_err("%s:%d failed invalid ir_cut_driver_type %d\n",
			__func__, __LINE__, ir_cut_driver_type);
		return -EINVAL;
	}

	rc = ir_cut_ctrl->func_tbl->camera_ir_cut_init(
			ir_cut_ctrl, ir_cut_data);
	if (rc < 0) {
		pr_err("%s:%d camera_ir_cut_init failed rc = %d",
			__func__, __LINE__, rc);
		return rc;
	}

	ir_cut_ctrl->ir_cut_state = MSM_CAMERA_IR_CUT_INIT;

	CDBG("Exit");
	return 0;
}

static int32_t msm_ir_cut_config(struct msm_ir_cut_ctrl_t *ir_cut_ctrl,
	void __user *argp)
{
	int32_t rc = -EINVAL;
	struct msm_ir_cut_cfg_data_t *ir_cut_data =
		(struct msm_ir_cut_cfg_data_t *) argp;

	mutex_lock(ir_cut_ctrl->ir_cut_mutex);

	CDBG("Enter %s type %d\n", __func__, ir_cut_data->cfg_type);

	switch (ir_cut_data->cfg_type) {
	case CFG_IR_CUT_INIT:
		rc = msm_ir_cut_handle_init(ir_cut_ctrl, ir_cut_data);
		break;
	case CFG_IR_CUT_RELEASE:
		if (ir_cut_ctrl->ir_cut_state == MSM_CAMERA_IR_CUT_INIT)
			rc = ir_cut_ctrl->func_tbl->camera_ir_cut_release(
				ir_cut_ctrl);
		break;
	case CFG_IR_CUT_OFF:
		if (ir_cut_ctrl->ir_cut_state == MSM_CAMERA_IR_CUT_INIT)
			rc = ir_cut_ctrl->func_tbl->camera_ir_cut_off(
				ir_cut_ctrl, ir_cut_data);
		break;
	case CFG_IR_CUT_ON:
		if (ir_cut_ctrl->ir_cut_state == MSM_CAMERA_IR_CUT_INIT)
			rc = ir_cut_ctrl->func_tbl->camera_ir_cut_on(
				ir_cut_ctrl, ir_cut_data);
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(ir_cut_ctrl->ir_cut_mutex);

	CDBG("Exit %s type %d\n", __func__, ir_cut_data->cfg_type);

	return rc;
}

static long msm_ir_cut_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct msm_ir_cut_ctrl_t *fctrl = NULL;
	void __user *argp = (void __user *)arg;

	CDBG("Enter\n");

	if (!sd) {
		pr_err("sd NULL\n");
		return -EINVAL;
	}
	fctrl = v4l2_get_subdevdata(sd);
	if (!fctrl) {
		pr_err("fctrl NULL\n");
		return -EINVAL;
	}
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_ir_cut_get_subdev_id(fctrl, argp);
	case VIDIOC_MSM_IR_CUT_CFG:
		return msm_ir_cut_config(fctrl, argp);
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_SHUTDOWN:
		if (!fctrl->func_tbl) {
			pr_err("fctrl->func_tbl NULL\n");
			return -EINVAL;
		} else {
			return fctrl->func_tbl->camera_ir_cut_release(fctrl);
		}
	default:
		pr_err_ratelimited("invalid cmd %d\n", cmd);
		return -ENOIOCTLCMD;
	}
	CDBG("Exit\n");
}

static struct v4l2_subdev_core_ops msm_ir_cut_subdev_core_ops = {
	.ioctl = msm_ir_cut_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_ir_cut_subdev_ops = {
	.core = &msm_ir_cut_subdev_core_ops,
};
static int msm_ir_cut_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {

	int rc = 0;
	struct msm_ir_cut_ctrl_t *ir_cut_ctrl = v4l2_get_subdevdata(sd);

	CDBG("Enter\n");

	if (!ir_cut_ctrl) {
		pr_err("%s: failed\n", __func__);
		return -EINVAL;
	}

	if (ir_cut_ctrl->ir_cut_state == MSM_CAMERA_IR_CUT_INIT)
		rc = ir_cut_ctrl->func_tbl->camera_ir_cut_release(
			ir_cut_ctrl);

	CDBG("Exit\n");

	return rc;
};

static const struct v4l2_subdev_internal_ops msm_ir_cut_internal_ops = {
	.close = msm_ir_cut_close,
};

static int32_t msm_ir_cut_get_gpio_dt_data(struct device_node *of_node,
		struct msm_ir_cut_ctrl_t *fctrl)
{
	int32_t rc = 0, i = 0;
	uint16_t *gpio_array = NULL;
	int16_t gpio_array_size = 0;
	struct msm_camera_gpio_conf *gconf = NULL;

	gpio_array_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, gpio_array_size);

	if (gpio_array_size > 0) {
		fctrl->power_info.gpio_conf =
			 kzalloc(sizeof(struct msm_camera_gpio_conf),
				 GFP_KERNEL);
		if (!fctrl->power_info.gpio_conf) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			return rc;
		}
		gconf = fctrl->power_info.gpio_conf;

		gpio_array = kcalloc(gpio_array_size, sizeof(uint16_t),
			GFP_KERNEL);
		if (!gpio_array)
			return -ENOMEM;
		for (i = 0; i < gpio_array_size; i++) {
			gpio_array[i] = of_get_gpio(of_node, i);
			if (((int16_t)gpio_array[i]) < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				rc = -EINVAL;
				goto free_gpio_array;
			}
			CDBG("%s gpio_array[%d] = %d\n", __func__, i,
				gpio_array[i]);
		}

		rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto free_gpio_array;
		}
		kfree(gpio_array);

		if (fctrl->ir_cut_driver_type == IR_CUT_DRIVER_DEFAULT)
			fctrl->ir_cut_driver_type = IR_CUT_DRIVER_GPIO;
		CDBG("%s:%d fctrl->ir_cut_driver_type = %d", __func__, __LINE__,
			fctrl->ir_cut_driver_type);
	}

	return rc;

free_gpio_array:
	kfree(gpio_array);
	return rc;
}

static int32_t msm_ir_cut_get_dt_data(struct device_node *of_node,
	struct msm_ir_cut_ctrl_t *fctrl)
{
	int32_t rc = 0;

	CDBG("called\n");

	if (!of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	/* Read the sub device */
	rc = of_property_read_u32(of_node, "cell-index", &fctrl->pdev->id);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	fctrl->ir_cut_driver_type = IR_CUT_DRIVER_DEFAULT;

	/* Read the gpio information from device tree */
	rc = msm_ir_cut_get_gpio_dt_data(of_node, fctrl);
	if (rc < 0) {
		pr_err("%s:%d msm_ir_cut_get_gpio_dt_data failed rc %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_ir_cut_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	int32_t rc = 0;
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct msm_ir_cut_cfg_data_t32 *u32 =
		(struct msm_ir_cut_cfg_data_t32 *)arg;
	struct msm_ir_cut_cfg_data_t ir_cut_data;

	CDBG("Enter");
	ir_cut_data.cfg_type = u32->cfg_type;

	switch (cmd) {
	case VIDIOC_MSM_IR_CUT_CFG32:
		cmd = VIDIOC_MSM_IR_CUT_CFG;
		break;
	default:
		return msm_ir_cut_subdev_ioctl(sd, cmd, arg);
	}

	rc = msm_ir_cut_subdev_ioctl(sd, cmd, &ir_cut_data);

	CDBG("Exit");
	return rc;
}

static long msm_ir_cut_subdev_fops_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_ir_cut_subdev_do_ioctl);
}
#endif

static int32_t msm_ir_cut_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0, i = 0;
	struct msm_ir_cut_ctrl_t *ir_cut_ctrl = NULL;

	CDBG("Enter");
	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	ir_cut_ctrl = kzalloc(sizeof(struct msm_ir_cut_ctrl_t), GFP_KERNEL);
	if (!ir_cut_ctrl)
		return -ENOMEM;

	memset(ir_cut_ctrl, 0, sizeof(struct msm_ir_cut_ctrl_t));

	ir_cut_ctrl->pdev = pdev;

	rc = msm_ir_cut_get_dt_data(pdev->dev.of_node, ir_cut_ctrl);

	if (rc < 0) {
		pr_err("%s:%d msm_ir_cut_get_dt_data failed\n",
			__func__, __LINE__);
		kfree(ir_cut_ctrl);
		return -EINVAL;
	}

	rc = msm_sensor_driver_get_gpio_data(&(ir_cut_ctrl->gconf),
		(&pdev->dev)->of_node);

	if ((rc < 0) || (ir_cut_ctrl->gconf == NULL)) {
		pr_err("%s: No IR CUT GPIOs\n", __func__);

		kfree(ir_cut_ctrl);
		return -EINVAL;
	}

	CDBG("%s: gpio_request_table_size = %d\n",
		__func__,
		ir_cut_ctrl->gconf->cam_gpio_req_tbl_size);

	for (i = 0;
		i < ir_cut_ctrl->gconf->cam_gpio_req_tbl_size; i++) {
		CDBG("%s: gpio = %d\n", __func__,
			ir_cut_ctrl->gconf->cam_gpio_req_tbl[i].gpio);
		CDBG("%s: gpio-flags = %lu\n", __func__,
			ir_cut_ctrl->gconf->cam_gpio_req_tbl[i].flags);
		CDBG("%s: gconf->gpio_num_info->gpio_num[%d] = %d\n",
			__func__, i,
			ir_cut_ctrl->gconf->gpio_num_info->gpio_num[i]);
	}

	ir_cut_ctrl->cam_pinctrl_status = 1;

	rc = msm_camera_pinctrl_init(
		&(ir_cut_ctrl->pinctrl_info), &(pdev->dev));

	if (rc < 0) {
		pr_err("ERR:%s: Error in reading IR CUT pinctrl\n",
			__func__);
		ir_cut_ctrl->cam_pinctrl_status = 0;
	}

	ir_cut_ctrl->ir_cut_state = MSM_CAMERA_IR_CUT_RELEASE;
	ir_cut_ctrl->power_info.dev = &ir_cut_ctrl->pdev->dev;
	ir_cut_ctrl->ir_cut_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	ir_cut_ctrl->ir_cut_mutex = &msm_ir_cut_mutex;

	/* Initialize sub device */
	v4l2_subdev_init(&ir_cut_ctrl->msm_sd.sd, &msm_ir_cut_subdev_ops);
	v4l2_set_subdevdata(&ir_cut_ctrl->msm_sd.sd, ir_cut_ctrl);

	ir_cut_ctrl->msm_sd.sd.internal_ops = &msm_ir_cut_internal_ops;
	ir_cut_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(ir_cut_ctrl->msm_sd.sd.name,
		ARRAY_SIZE(ir_cut_ctrl->msm_sd.sd.name),
		"msm_camera_ir_cut");
	media_entity_init(&ir_cut_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	ir_cut_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	ir_cut_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_IR_CUT;
	ir_cut_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x1;
	msm_sd_register(&ir_cut_ctrl->msm_sd);

	CDBG("%s:%d ir_cut sd name = %s", __func__, __LINE__,
		ir_cut_ctrl->msm_sd.sd.entity.name);
	msm_ir_cut_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
	msm_ir_cut_v4l2_subdev_fops.compat_ioctl32 =
		msm_ir_cut_subdev_fops_ioctl;
#endif
	ir_cut_ctrl->msm_sd.sd.devnode->fops = &msm_ir_cut_v4l2_subdev_fops;

	CDBG("probe success\n");
	return rc;
}

MODULE_DEVICE_TABLE(of, msm_ir_cut_dt_match);

static struct platform_driver msm_ir_cut_platform_driver = {
	.probe = msm_ir_cut_platform_probe,
	.driver = {
		.name = "qcom,ir-cut",
		.owner = THIS_MODULE,
		.of_match_table = msm_ir_cut_dt_match,
	},
};

static int __init msm_ir_cut_init_module(void)
{
	int32_t rc = 0;

	CDBG("Enter\n");
	rc = platform_driver_register(&msm_ir_cut_platform_driver);
	if (!rc)
		return rc;

	pr_err("platform probe for ir_cut failed");

	return rc;
}

static void __exit msm_ir_cut_exit_module(void)
{
	platform_driver_unregister(&msm_ir_cut_platform_driver);
}

static struct msm_ir_cut_table msm_gpio_ir_cut_table = {
	.ir_cut_driver_type = IR_CUT_DRIVER_GPIO,
	.func_tbl = {
		.camera_ir_cut_init = msm_ir_cut_init,
		.camera_ir_cut_release = msm_ir_cut_release,
		.camera_ir_cut_off = msm_ir_cut_off,
		.camera_ir_cut_on = msm_ir_cut_on,
	},
};

module_init(msm_ir_cut_init_module);
module_exit(msm_ir_cut_exit_module);
MODULE_DESCRIPTION("MSM IR CUT");
MODULE_LICENSE("GPL v2");
