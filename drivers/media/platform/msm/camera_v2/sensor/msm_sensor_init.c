/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "MSM-SENSOR-INIT %s:%d " fmt "\n", __func__, __LINE__

/* Header files */
#include "msm_sensor_init.h"
#include "msm_sensor_driver.h"
#include "msm_sensor.h"
#include "msm_sd.h"

/* Logging macro */
#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

static struct msm_sensor_init_t *s_init;
static struct v4l2_file_operations msm_sensor_init_v4l2_subdev_fops;
/* Static function declaration */
static long msm_sensor_init_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg);

/* Static structure declaration */
static struct v4l2_subdev_core_ops msm_sensor_init_subdev_core_ops = {
	.ioctl = msm_sensor_init_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_sensor_init_subdev_ops = {
	.core = &msm_sensor_init_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_sensor_init_internal_ops;

static int msm_sensor_wait_for_probe_done(struct msm_sensor_init_t *s_init)
{
	int rc;
	int tm = 20000;
	if (s_init->module_init_status == 1) {
		CDBG("msm_cam_get_module_init_status -2\n");
		return 0;
	}
	rc = wait_event_timeout(s_init->state_wait,
		(s_init->module_init_status == 1), msecs_to_jiffies(tm));
	if (rc == 0)
		pr_err("%s:%d wait timeout\n", __func__, __LINE__);

	return rc;
}

/* Static function definition */
static int32_t msm_sensor_driver_cmd(struct msm_sensor_init_t *s_init,
	void *arg)
{
	int32_t                      rc = 0;
	struct sensor_init_cfg_data *cfg = (struct sensor_init_cfg_data *)arg;

	/* Validate input parameters */
	if (!s_init || !cfg) {
		pr_err("failed: s_init %pK cfg %pK", s_init, cfg);
		return -EINVAL;
	}

	switch (cfg->cfgtype) {
	case CFG_SINIT_PROBE:
		mutex_lock(&s_init->imutex);
		s_init->module_init_status = 0;
		rc = msm_sensor_driver_probe(cfg->cfg.setting,
			&cfg->probed_info,
			cfg->entity_name);
		mutex_unlock(&s_init->imutex);
		if (rc < 0)
			pr_err("%s failed (non-fatal) rc %d", __func__, rc);
		break;

	case CFG_SINIT_PROBE_DONE:
		s_init->module_init_status = 1;
		wake_up(&s_init->state_wait);
		break;

	case CFG_SINIT_PROBE_WAIT_DONE:
		msm_sensor_wait_for_probe_done(s_init);
		break;

	default:
		pr_err("default");
		break;
	}

	return rc;
}

static long msm_sensor_init_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	long rc = 0;
	struct msm_sensor_init_t *s_init = v4l2_get_subdevdata(sd);
	CDBG("Enter");

	/* Validate input parameters */
	if (!s_init) {
		pr_err("failed: s_init %pK", s_init);
		return -EINVAL;
	}

	switch (cmd) {
	case VIDIOC_MSM_SENSOR_INIT_CFG:
		rc = msm_sensor_driver_cmd(s_init, arg);
		break;

	default:
		pr_err_ratelimited("default\n");
		break;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_sensor_init_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	int32_t             rc = 0;
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct sensor_init_cfg_data32 *u32 =
		(struct sensor_init_cfg_data32 *)arg;
	struct sensor_init_cfg_data sensor_init_data;

	switch (cmd) {
	case VIDIOC_MSM_SENSOR_INIT_CFG32:
		memset(&sensor_init_data, 0, sizeof(sensor_init_data));
		sensor_init_data.cfgtype = u32->cfgtype;
		sensor_init_data.cfg.setting = compat_ptr(u32->cfg.setting);
		cmd = VIDIOC_MSM_SENSOR_INIT_CFG;
		rc = msm_sensor_init_subdev_ioctl(sd, cmd, &sensor_init_data);
		if (rc < 0) {
			pr_err("%s:%d VIDIOC_MSM_SENSOR_INIT_CFG failed (non-fatal)",
				__func__, __LINE__);
			return rc;
		}
		u32->probed_info = sensor_init_data.probed_info;
		strlcpy(u32->entity_name, sensor_init_data.entity_name,
			sizeof(sensor_init_data.entity_name));
		return 0;
	default:
		return msm_sensor_init_subdev_ioctl(sd, cmd, arg);
	}
}

static long msm_sensor_init_subdev_fops_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_sensor_init_subdev_do_ioctl);
}
#endif

static int __init msm_sensor_init_module(void)
{
	int ret = 0;
	/* Allocate memory for msm_sensor_init control structure */
	s_init = kzalloc(sizeof(struct msm_sensor_init_t), GFP_KERNEL);
	if (!s_init)
		return -ENOMEM;

	CDBG("MSM_SENSOR_INIT_MODULE %pK", NULL);

	/* Initialize mutex */
	mutex_init(&s_init->imutex);

	/* Create /dev/v4l-subdevX for msm_sensor_init */
	v4l2_subdev_init(&s_init->msm_sd.sd, &msm_sensor_init_subdev_ops);
	snprintf(s_init->msm_sd.sd.name, sizeof(s_init->msm_sd.sd.name), "%s",
		"msm_sensor_init");
	v4l2_set_subdevdata(&s_init->msm_sd.sd, s_init);
	s_init->msm_sd.sd.internal_ops = &msm_sensor_init_internal_ops;
	s_init->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_init->msm_sd.sd.entity, 0, NULL, 0);
	s_init->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_init->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR_INIT;
	s_init->msm_sd.sd.entity.name = s_init->msm_sd.sd.name;
	s_init->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x6;
	ret = msm_sd_register(&s_init->msm_sd);
	if (ret) {
		CDBG("%s: msm_sd_register error = %d\n", __func__, ret);
		goto error;
	}
	msm_cam_copy_v4l2_subdev_fops(&msm_sensor_init_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_sensor_init_v4l2_subdev_fops.compat_ioctl32 =
		msm_sensor_init_subdev_fops_ioctl;
#endif
	s_init->msm_sd.sd.devnode->fops =
		&msm_sensor_init_v4l2_subdev_fops;

	init_waitqueue_head(&s_init->state_wait);

	return 0;
error:
	mutex_destroy(&s_init->imutex);
	kfree(s_init);
	return ret;
}

static void __exit msm_sensor_exit_module(void)
{
	msm_sd_unregister(&s_init->msm_sd);
	mutex_destroy(&s_init->imutex);
	kfree(s_init);
	return;
}

module_init(msm_sensor_init_module);
module_exit(msm_sensor_exit_module);
MODULE_DESCRIPTION("msm_sensor_init");
MODULE_LICENSE("GPL v2");
