/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include "msm_laser_led.h"
#include "msm_camera_dt_util.h"
#include "msm_sd.h"
#include "msm_cci.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

DEFINE_MSM_MUTEX(msm_laser_led_mutex);

static struct v4l2_file_operations msm_laser_led_v4l2_subdev_fops;

static const struct of_device_id msm_laser_led_dt_match[] = {
	{.compatible = "qcom,laser-led", .data = NULL},
	{}
};

static long msm_laser_led_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg);

static int32_t msm_laser_led_get_subdev_id(
	struct msm_laser_led_ctrl_t *laser_led_ctrl, void __user *arg)
{
	int32_t __user *subdev_id = (int32_t __user *)arg;

	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("subdevice ID is not valid\n");
		return -EINVAL;
	}

	if (laser_led_ctrl->laser_led_device_type !=
		MSM_CAMERA_PLATFORM_DEVICE) {
		pr_err("device type is not matching\n");
		return -EINVAL;
	}

	if (copy_to_user(arg, &laser_led_ctrl->pdev->id,
		sizeof(int32_t))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	CDBG("Exit: subdev_id %d\n", laser_led_ctrl->pdev->id);
	return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};
#ifdef CONFIG_COMPAT
static int32_t msm_laser_led_init(
	struct msm_laser_led_ctrl_t *laser_led_ctrl,
	struct msm_laser_led_cfg_data_t32 __user *laser_led_data)
#else
static int32_t msm_laser_led_init(
	struct msm_laser_led_ctrl_t *laser_led_ctrl,
	struct msm_laser_led_cfg_data_t __user *laser_led_data)
#endif
{
	int32_t rc = -EFAULT;
	struct msm_camera_cci_client *cci_client = NULL;

	CDBG("Enter\n");

	if (laser_led_ctrl->laser_led_state == MSM_CAMERA_LASER_LED_INIT) {
		pr_err("Invalid laser_led state = %d\n",
				laser_led_ctrl->laser_led_state);
		return 0;
	}

	rc = laser_led_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&laser_led_ctrl->i2c_client, MSM_CCI_INIT);
	if (rc < 0)
		pr_err("cci_init failed\n");

	cci_client = laser_led_ctrl->i2c_client.cci_client;

	if (copy_from_user(&(cci_client->sid),
		&(laser_led_data->i2c_addr),
		sizeof(uint16_t))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}
	cci_client->sid = cci_client->sid >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;

	if (copy_from_user(&(cci_client->i2c_freq_mode),
		&(laser_led_data->i2c_freq_mode),
		sizeof(enum i2c_freq_mode_t))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	laser_led_ctrl->laser_led_state = MSM_CAMERA_LASER_LED_INIT;

	CDBG("Exit\n");
	return 0;
}

static int msm_laser_led_close(struct v4l2_subdev *sd,
			struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_laser_led_ctrl_t *l_ctrl =  v4l2_get_subdevdata(sd);

	CDBG("Enter\n");
	if (!l_ctrl) {
		pr_err("failed: subdev data is null\n");
		return -EINVAL;
	}
	mutex_lock(l_ctrl->laser_led_mutex);
	if (l_ctrl->laser_led_device_type == MSM_CAMERA_PLATFORM_DEVICE &&
		l_ctrl->laser_led_state != MSM_CAMERA_LASER_LED_RELEASE) {
		rc = l_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&l_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed: %d\n", rc);
	}
	l_ctrl->laser_led_state = MSM_CAMERA_LASER_LED_RELEASE;
	mutex_unlock(l_ctrl->laser_led_mutex);
	CDBG("Exit\n");
	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_laser_led_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	int32_t rc = 0;
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	CDBG("Enter\n");
	switch (cmd) {
	case VIDIOC_MSM_LASER_LED_CFG32:
		cmd = VIDIOC_MSM_LASER_LED_CFG;
	default:
		rc =  msm_laser_led_subdev_ioctl(sd, cmd, arg);
	}

	CDBG("Exit\n");
	return rc;
}

static long msm_laser_led_subdev_fops_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return msm_laser_led_subdev_do_ioctl(file, cmd, (void *)arg);
}

static int32_t msm_laser_led_control32(
	struct msm_laser_led_ctrl_t *laser_led_ctrl,
	void __user *argp)
{
	struct msm_camera_i2c_reg_setting32 conf_array32;
	struct msm_camera_i2c_reg_setting conf_array;
	int32_t rc = 0;
	struct msm_laser_led_cfg_data_t32 laser_led_data;
	uint32_t *debug_reg;
	int i;
	uint16_t local_data;

	if (laser_led_ctrl->laser_led_state != MSM_CAMERA_LASER_LED_INIT) {
		pr_err("%s:%d failed: invalid state %d\n", __func__,
			__LINE__, laser_led_ctrl->laser_led_state);
		return -EFAULT;
	}

	if (copy_from_user(&laser_led_data,
		argp,
		sizeof(struct msm_laser_led_cfg_data_t32))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(&conf_array32,
		(compat_ptr)(laser_led_data.setting),
		sizeof(struct msm_camera_i2c_reg_setting32))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	conf_array.addr_type = conf_array32.addr_type;
	conf_array.data_type = conf_array32.data_type;
	conf_array.delay = conf_array32.delay;
	conf_array.size = conf_array32.size;

	if (!conf_array.size ||
		conf_array.size > I2C_REG_DATA_MAX) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	conf_array.reg_setting = kzalloc(conf_array.size *
		(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
	if (!conf_array.reg_setting)
		return -ENOMEM;

	if (copy_from_user(conf_array.reg_setting,
		(compat_ptr)(conf_array32.reg_setting),
		conf_array.size *
		sizeof(struct msm_camera_i2c_reg_array))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		kfree(conf_array.reg_setting);
		return -EFAULT;
	}

	debug_reg = kzalloc(laser_led_data.debug_reg_size *
		(sizeof(uint32_t)), GFP_KERNEL);
	if (!debug_reg) {
		kfree(conf_array.reg_setting);
		return -ENOMEM;
	}

	if (copy_from_user(debug_reg,
		(void __user *)compat_ptr(laser_led_data.debug_reg),
		laser_led_data.debug_reg_size *
		sizeof(uint32_t))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		kfree(conf_array.reg_setting);
		kfree(debug_reg);
		return -EFAULT;
	}

	laser_led_ctrl->i2c_client.addr_type = conf_array.addr_type;

	rc = laser_led_ctrl->i2c_client.i2c_func_tbl->
		i2c_write_table(&(laser_led_ctrl->i2c_client),
		&conf_array);

	for (i = 0; i < laser_led_data.debug_reg_size; i++) {
		rc = laser_led_ctrl->i2c_client.i2c_func_tbl->i2c_read(
			&(laser_led_ctrl->i2c_client),
			debug_reg[i],
			&local_data, conf_array.data_type);
	}

	kfree(conf_array.reg_setting);
	kfree(debug_reg);

	return rc;
}
#endif

static int32_t msm_laser_led_control(
	struct msm_laser_led_ctrl_t *laser_led_ctrl,
	void __user *argp)
{
	struct msm_camera_i2c_reg_setting conf_array;
	struct msm_laser_led_cfg_data_t laser_led_data;

	uint32_t *debug_reg;
	int i;
	uint16_t local_data;
	int32_t rc = 0;

	if (laser_led_ctrl->laser_led_state != MSM_CAMERA_LASER_LED_INIT) {
		pr_err("%s:%d failed: invalid state %d\n", __func__,
			__LINE__, laser_led_ctrl->laser_led_state);
		return -EFAULT;
	}

	if (copy_from_user(&laser_led_data,
		argp,
		sizeof(struct msm_laser_led_cfg_data_t))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(&conf_array,
		(laser_led_data.setting),
		sizeof(struct msm_camera_i2c_reg_setting))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (!conf_array.size ||
		conf_array.size > I2C_REG_DATA_MAX) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	conf_array.reg_setting = kzalloc(conf_array.size *
		(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
	if (!conf_array.reg_setting)
		return -ENOMEM;

	if (copy_from_user(conf_array.reg_setting, (void __user *)(
		conf_array.reg_setting),
		conf_array.size *
		sizeof(struct msm_camera_i2c_reg_array))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		kfree(conf_array.reg_setting);
		return -EFAULT;
	}

	debug_reg = kzalloc(laser_led_data.debug_reg_size *
		(sizeof(uint32_t)), GFP_KERNEL);
	if (!debug_reg) {
		kfree(conf_array.reg_setting);
		return -ENOMEM;
	}

	if (copy_from_user(debug_reg,
		(laser_led_data.debug_reg),
		laser_led_data.debug_reg_size *
		sizeof(uint32_t))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		kfree(debug_reg);
		kfree(conf_array.reg_setting);
		return -EFAULT;
	}

	laser_led_ctrl->i2c_client.addr_type = conf_array.addr_type;

	rc = laser_led_ctrl->i2c_client.i2c_func_tbl->
		i2c_write_table(&(laser_led_ctrl->i2c_client),
		&conf_array);

	for (i = 0; i < laser_led_data.debug_reg_size; i++) {
		rc = laser_led_ctrl->i2c_client.i2c_func_tbl->i2c_read(
			&(laser_led_ctrl->i2c_client),
			debug_reg[i],
			&local_data, conf_array.data_type);
	}

	kfree(conf_array.reg_setting);
	kfree(debug_reg);

	return rc;
}

static int32_t msm_laser_led_config(struct msm_laser_led_ctrl_t *laser_led_ctrl,
	void __user *argp)
{
	int32_t rc = -EINVAL;
	enum msm_laser_led_cfg_type_t cfg_type;

#ifdef CONFIG_COMPAT
	struct msm_laser_led_cfg_data_t32 __user *laser_led_data =
		(struct msm_laser_led_cfg_data_t32 __user *) argp;
#else
	struct msm_laser_led_cfg_data_t __user *laser_led_data =
		(struct msm_laser_led_cfg_data_t __user *) argp;
#endif

	mutex_lock(laser_led_ctrl->laser_led_mutex);

	if (copy_from_user(&(cfg_type),
		&(laser_led_data->cfg_type),
		sizeof(enum msm_laser_led_cfg_type_t))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		mutex_unlock(laser_led_ctrl->laser_led_mutex);
		return -EFAULT;
	}

	CDBG("type %d\n", cfg_type);

	switch (cfg_type) {
	case CFG_LASER_LED_INIT:
		rc = msm_laser_led_init(laser_led_ctrl, laser_led_data);
		break;
	case CFG_LASER_LED_CONTROL:
#ifdef CONFIG_COMPAT
		if (is_compat_task())
			rc = msm_laser_led_control32(laser_led_ctrl, argp);
		else
#endif
			rc = msm_laser_led_control(laser_led_ctrl, argp);
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(laser_led_ctrl->laser_led_mutex);

	CDBG("Exit: type %d\n", cfg_type);

	return rc;
}

static long msm_laser_led_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct msm_laser_led_ctrl_t *lctrl = NULL;
	void __user *argp = (void __user *)arg;

	CDBG("Enter\n");

	if (!sd) {
		pr_err(" v4l2 ir led subdevice is NULL\n");
		return -EINVAL;
	}
	lctrl = v4l2_get_subdevdata(sd);
	if (!lctrl) {
		pr_err("lctrl NULL\n");
		return -EINVAL;
	}
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_laser_led_get_subdev_id(lctrl, argp);
	case VIDIOC_MSM_LASER_LED_CFG:
		return msm_laser_led_config(lctrl, argp);
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_SHUTDOWN:
		if (!lctrl->i2c_client.i2c_func_tbl) {
			pr_err("a_ctrl->i2c_client.i2c_func_tbl NULL\n");
			return -EINVAL;
		}
		return msm_laser_led_close(sd, NULL);

	default:
		pr_err("invalid cmd %d\n", cmd);
		return -ENOIOCTLCMD;
	}
	CDBG("Exit\n");
}

static struct v4l2_subdev_core_ops msm_laser_led_subdev_core_ops = {
	.ioctl = msm_laser_led_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_laser_led_subdev_ops = {
	.core = &msm_laser_led_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_laser_led_internal_ops = {
	.close = msm_laser_led_close,
};

static int32_t msm_laser_led_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_laser_led_ctrl_t *laser_led_ctrl = NULL;
	struct msm_camera_cci_client *cci_client = NULL;

	CDBG("Enter\n");
	if (!pdev->dev.of_node) {
		pr_err("IR LED device node is not present in device tree\n");
		return -EINVAL;
	}

	laser_led_ctrl = devm_kzalloc(&pdev->dev,
		sizeof(struct msm_laser_led_ctrl_t), GFP_KERNEL);
	if (!laser_led_ctrl)
		return -ENOMEM;

	laser_led_ctrl->pdev = pdev;

	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		kfree(laser_led_ctrl);
		pr_err("reading cell index failed: rc %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&laser_led_ctrl->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", laser_led_ctrl->cci_master, rc);
	if (rc < 0 || laser_led_ctrl->cci_master >= MASTER_MAX) {
		kfree(laser_led_ctrl);
		pr_err("invalid cci master info: rc %d\n", rc);
		return rc;
	}

	laser_led_ctrl->laser_led_state = MSM_CAMERA_LASER_LED_RELEASE;
	laser_led_ctrl->power_info.dev = &laser_led_ctrl->pdev->dev;
	laser_led_ctrl->laser_led_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	laser_led_ctrl->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	laser_led_ctrl->laser_led_mutex = &msm_laser_led_mutex;

	laser_led_ctrl->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!laser_led_ctrl->i2c_client.cci_client)
		return -ENOMEM;

	cci_client = laser_led_ctrl->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = laser_led_ctrl->cci_master;

	/* Initialize sub device */
	v4l2_subdev_init(&laser_led_ctrl->msm_sd.sd, &msm_laser_led_subdev_ops);
	v4l2_set_subdevdata(&laser_led_ctrl->msm_sd.sd, laser_led_ctrl);

	laser_led_ctrl->msm_sd.sd.internal_ops = &msm_laser_led_internal_ops;
	laser_led_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(laser_led_ctrl->msm_sd.sd.name,
		ARRAY_SIZE(laser_led_ctrl->msm_sd.sd.name),
		"msm_camera_laser_led");
	media_entity_init(&laser_led_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	laser_led_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	laser_led_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_LASER_LED;
	laser_led_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x1;
	msm_sd_register(&laser_led_ctrl->msm_sd);

	laser_led_ctrl->laser_led_state = MSM_CAMERA_LASER_LED_RELEASE;

	CDBG("laser_led sd name = %s\n",
		laser_led_ctrl->msm_sd.sd.entity.name);
	msm_laser_led_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
	msm_laser_led_v4l2_subdev_fops.compat_ioctl32 =
		msm_laser_led_subdev_fops_ioctl;
#endif
	laser_led_ctrl->msm_sd.sd.devnode->fops =
		&msm_laser_led_v4l2_subdev_fops;

	CDBG("probe success\n");
	return rc;
}

MODULE_DEVICE_TABLE(of, msm_laser_led_dt_match);

static struct platform_driver msm_laser_led_platform_driver = {
	.probe = msm_laser_led_platform_probe,
	.driver = {
		.name = "qcom,laser-led",
		.owner = THIS_MODULE,
		.of_match_table = msm_laser_led_dt_match,
	},
};

static int __init msm_laser_led_init_module(void)
{
	int32_t rc;

	CDBG("Enter\n");
	rc = platform_driver_register(&msm_laser_led_platform_driver);
	if (!rc) {
		CDBG("Exit\n");
		return rc;
	}
	pr_err("laser-led driver register failed: %d\n", rc);

	return rc;
}

static void __exit msm_laser_led_exit_module(void)
{
	platform_driver_unregister(&msm_laser_led_platform_driver);
}

module_init(msm_laser_led_init_module);
module_exit(msm_laser_led_exit_module);
MODULE_DESCRIPTION("MSM IR LED");
MODULE_LICENSE("GPL v2");
