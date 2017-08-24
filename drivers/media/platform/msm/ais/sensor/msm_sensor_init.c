/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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
#include "msm_sensor_driver.h"
#include "msm_sensor.h"
#include "msm_sd.h"
#include "msm_camera_io_util.h"
#include "msm_early_cam.h"

/* Logging macro */
#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

#define EARLY_CAMERA_SIGNAL_DONE 0xa5a5a5a5
#define EARLY_CAMERA_SIGNAL_DISABLED 0

static bool early_camera_clock_off;
static struct msm_sensor_init_t *s_init;

static int msm_sensor_wait_for_probe_done(struct msm_sensor_init_t *s_init)
{
	int rc;
	int tm = 10000;

	if (s_init->module_init_status == 1) {
		CDBG("msm_cam_get_module_init_status -2\n");
		return 0;
	}
	rc = wait_event_timeout(s_init->state_wait,
		(s_init->module_init_status == 1), msecs_to_jiffies(tm));
	if (rc == 0) {
		pr_err("%s:%d wait timeout\n", __func__, __LINE__);
		rc = -1;
	}

	return rc;
}

#define MMSS_A_VFE_0_SPARE 0xC84

/* Static function definition */
int32_t msm_sensor_driver_cmd(struct msm_sensor_init_t *s_init, void *arg)
{
	int32_t                      rc = 0;
	u32 val = 0;
	void __iomem *base;
	struct sensor_init_cfg_data *cfg = (struct sensor_init_cfg_data *)arg;

	/* Validate input parameters */
	if (!s_init || !cfg) {
		pr_err("failed: s_init %pK cfg %pK", s_init, cfg);
		return -EINVAL;
	}

	pr_debug("%s : %d", __func__, cfg->cfgtype);
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
		if (early_camera_clock_off == false) {
			base = ioremap(0x00A10000, 0x1000);
			val = msm_camera_io_r_mb(base + MMSS_A_VFE_0_SPARE);
			while (val != EARLY_CAMERA_SIGNAL_DONE) {
				if (val == EARLY_CAMERA_SIGNAL_DISABLED)
					break;
				msleep(1000);
				val = msm_camera_io_r_mb(
					base + MMSS_A_VFE_0_SPARE);
				pr_err("Waiting for signal from LK val = %u\n",
					val);
			}
			rc = msm_early_cam_disable_clocks();
			if (rc < 0) {
				pr_err("Failed to disable early camera :%d\n",
					rc);
			} else {
				early_camera_clock_off = true;
				pr_debug("Voted OFF early camera clocks\n");
			}
		}

		s_init->module_init_status = 1;
		wake_up(&s_init->state_wait);
		break;

	case CFG_SINIT_PROBE_WAIT_DONE:
		rc = msm_sensor_wait_for_probe_done(s_init);
		break;

	default:
		pr_err("default");
		break;
	}

	return rc;
}

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

	init_waitqueue_head(&s_init->state_wait);
	early_camera_clock_off = false;
	return ret;
}

static void __exit msm_sensor_exit_module(void)
{
	mutex_destroy(&s_init->imutex);
	kfree(s_init);
}

module_init(msm_sensor_init_module);
module_exit(msm_sensor_exit_module);
MODULE_DESCRIPTION("msm_sensor_init");
MODULE_LICENSE("GPL v2");
