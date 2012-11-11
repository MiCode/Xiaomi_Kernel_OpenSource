/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/export.h>
#include <linux/of.h>
#include <mach/pmic.h>
#include <mach/camera.h>
#include <mach/gpio.h>
#include "msm_flash.h"
#include "msm.h"

static struct timer_list timer_flash;

enum msm_cam_flash_stat {
	MSM_CAM_FLASH_OFF,
	MSM_CAM_FLASH_ON,
};

static int config_flash_gpio_table(enum msm_cam_flash_stat stat,
			struct msm_camera_sensor_strobe_flash_data *sfdata)
{
	int rc = 0, i = 0;
	int msm_cam_flash_gpio_tbl[][2] = {
		{sfdata->flash_trigger, 1},
		{sfdata->flash_charge, 1},
		{sfdata->flash_charge_done, 0}
	};

	if (stat == MSM_CAM_FLASH_ON) {
		for (i = 0; i < ARRAY_SIZE(msm_cam_flash_gpio_tbl); i++) {
			rc = gpio_request(msm_cam_flash_gpio_tbl[i][0],
							  "CAM_FLASH_GPIO");
			if (unlikely(rc < 0)) {
				pr_err("%s not able to get gpio\n", __func__);
				for (i--; i >= 0; i--)
					gpio_free(msm_cam_flash_gpio_tbl[i][0]);
				break;
			}
			if (msm_cam_flash_gpio_tbl[i][1])
				gpio_direction_output(
					msm_cam_flash_gpio_tbl[i][0], 0);
			else
				gpio_direction_input(
					msm_cam_flash_gpio_tbl[i][0]);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(msm_cam_flash_gpio_tbl); i++) {
			gpio_direction_input(msm_cam_flash_gpio_tbl[i][0]);
			gpio_free(msm_cam_flash_gpio_tbl[i][0]);
		}
	}
	return rc;
}

static int msm_strobe_flash_xenon_charge(int32_t flash_charge,
		int32_t charge_enable, uint32_t flash_recharge_duration)
{
	gpio_set_value_cansleep(flash_charge, charge_enable);
	if (charge_enable) {
		timer_flash.expires = jiffies +
			msecs_to_jiffies(flash_recharge_duration);
		/* add timer for the recharge */
		if (!timer_pending(&timer_flash))
			add_timer(&timer_flash);
	} else
		del_timer_sync(&timer_flash);
	return 0;
}

static void strobe_flash_xenon_recharge_handler(unsigned long data)
{
	unsigned long flags;
	struct msm_camera_sensor_strobe_flash_data *sfdata =
		(struct msm_camera_sensor_strobe_flash_data *)data;

	spin_lock_irqsave(&sfdata->timer_lock, flags);
	msm_strobe_flash_xenon_charge(sfdata->flash_charge, 1,
		sfdata->flash_recharge_duration);
	spin_unlock_irqrestore(&sfdata->timer_lock, flags);

	return;
}

static irqreturn_t strobe_flash_charge_ready_irq(int irq_num, void *data)
{
	struct msm_camera_sensor_strobe_flash_data *sfdata =
		(struct msm_camera_sensor_strobe_flash_data *)data;

	/* put the charge signal to low */
	gpio_set_value_cansleep(sfdata->flash_charge, 0);

	return IRQ_HANDLED;
}

static int msm_strobe_flash_xenon_init(
	struct msm_camera_sensor_strobe_flash_data *sfdata)
{
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&sfdata->spin_lock, flags);
	if (!sfdata->state) {

		rc = config_flash_gpio_table(MSM_CAM_FLASH_ON, sfdata);
		if (rc < 0) {
			pr_err("%s: gpio_request failed\n", __func__);
			goto go_out;
		}
		rc = request_irq(sfdata->irq, strobe_flash_charge_ready_irq,
			IRQF_TRIGGER_RISING, "charge_ready", sfdata);
		if (rc < 0) {
			pr_err("%s: request_irq failed %d\n", __func__, rc);
			goto go_out;
		}

		spin_lock_init(&sfdata->timer_lock);
		/* setup timer */
		init_timer(&timer_flash);
		timer_flash.function = strobe_flash_xenon_recharge_handler;
		timer_flash.data = (unsigned long)sfdata;
	}
	sfdata->state++;
go_out:
	spin_unlock_irqrestore(&sfdata->spin_lock, flags);

	return rc;
}

static int msm_strobe_flash_xenon_release
(struct msm_camera_sensor_strobe_flash_data *sfdata, int32_t final_release)
{
	unsigned long flags;

	spin_lock_irqsave(&sfdata->spin_lock, flags);
	if (sfdata->state > 0) {
		if (final_release)
			sfdata->state = 0;
		else
			sfdata->state--;

		if (!sfdata->state) {
			free_irq(sfdata->irq, sfdata);
			config_flash_gpio_table(MSM_CAM_FLASH_OFF, sfdata);
			if (timer_pending(&timer_flash))
				del_timer_sync(&timer_flash);
		}
	}
	spin_unlock_irqrestore(&sfdata->spin_lock, flags);
	return 0;
}

static int msm_strobe_flash_ctrl(
	struct msm_camera_sensor_strobe_flash_data *sfdata,
	struct strobe_flash_ctrl_data *strobe_ctrl)
{
	int rc = 0;
	switch (strobe_ctrl->type) {
	case STROBE_FLASH_CTRL_INIT:
		if (!sfdata)
			return -ENODEV;
		rc = msm_strobe_flash_xenon_init(sfdata);
		break;
	case STROBE_FLASH_CTRL_CHARGE:
		rc = msm_strobe_flash_xenon_charge(sfdata->flash_charge,
			strobe_ctrl->charge_en,
			sfdata->flash_recharge_duration);
		break;
	case STROBE_FLASH_CTRL_RELEASE:
		if (sfdata)
			rc = msm_strobe_flash_xenon_release(sfdata, 0);
		break;
	default:
		pr_err("Invalid Strobe Flash State\n");
		rc = -EINVAL;
	}
	return rc;
}

int msm_flash_led_init(struct msm_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_flash_external *external = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	external = &fctrl->flash_data->flash_src->_fsrc.ext_driver_src;
	if (external->expander_info && !fctrl->expander_client) {
		struct i2c_adapter *adapter =
		i2c_get_adapter(external->expander_info->bus_id);
		if (adapter)
			fctrl->expander_client = i2c_new_device(adapter,
				external->expander_info->board_info);
		if (!fctrl->expander_client || !adapter) {
			pr_err("fctrl->expander_client is not available\n");
			rc = -ENOTSUPP;
			return rc;
		}
		i2c_put_adapter(adapter);
	}
	rc = msm_camera_init_gpio_table(
		fctrl->flash_data->flash_src->init_gpio_tbl,
		fctrl->flash_data->flash_src->init_gpio_tbl_size, 1);
	if (rc < 0)
		pr_err("%s:%d failed\n", __func__, __LINE__);
	return rc;
}

int msm_flash_led_release(struct msm_flash_ctrl_t *fctrl)
{
	struct msm_camera_sensor_flash_external *external = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	external = &fctrl->flash_data->flash_src->_fsrc.ext_driver_src;
	msm_camera_set_gpio_table(
		fctrl->flash_data->flash_src->set_gpio_tbl,
		fctrl->flash_data->flash_src->set_gpio_tbl_size, 0);
	msm_camera_init_gpio_table(
		fctrl->flash_data->flash_src->init_gpio_tbl,
		fctrl->flash_data->flash_src->init_gpio_tbl_size, 0);
	if (external->expander_info && fctrl->expander_client) {
		i2c_unregister_device(fctrl->expander_client);
		fctrl->expander_client = NULL;
	}
	return 0;
}

int msm_flash_led_off(struct msm_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_flash_external *external = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	external = &fctrl->flash_data->flash_src->_fsrc.ext_driver_src;
	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = msm_camera_i2c_write_tbl(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->off_setting,
			fctrl->reg_setting->off_setting_size,
			fctrl->reg_setting->default_data_type);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	msm_camera_set_gpio_table(
		fctrl->flash_data->flash_src->set_gpio_tbl,
		fctrl->flash_data->flash_src->set_gpio_tbl_size, 0);

	return rc;
}

int msm_flash_led_low(struct msm_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_flash_external *external = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	external = &fctrl->flash_data->flash_src->_fsrc.ext_driver_src;
	msm_camera_set_gpio_table(
		fctrl->flash_data->flash_src->set_gpio_tbl,
		fctrl->flash_data->flash_src->set_gpio_tbl_size, 1);
	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = msm_camera_i2c_write_tbl(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->low_setting,
			fctrl->reg_setting->low_setting_size,
			fctrl->reg_setting->default_data_type);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	return rc;
}

int msm_flash_led_high(struct msm_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_flash_external *external = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	external = &fctrl->flash_data->flash_src->_fsrc.ext_driver_src;
	msm_camera_set_gpio_table(
		fctrl->flash_data->flash_src->set_gpio_tbl,
		fctrl->flash_data->flash_src->set_gpio_tbl_size, 1);
	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = msm_camera_i2c_write_tbl(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->high_setting,
			fctrl->reg_setting->high_setting_size,
			fctrl->reg_setting->default_data_type);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	return rc;
}

int msm_camera_flash_led_config(struct msm_flash_ctrl_t *fctrl,
	uint8_t led_state)
{
	int rc = 0;

	CDBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl->func_tbl) {
		pr_err("%s flash func tbl NULL\n", __func__);
		return 0;
	}
	switch (led_state) {
	case MSM_CAMERA_LED_INIT:
		if (fctrl->func_tbl->flash_led_init)
			rc = fctrl->func_tbl->flash_led_init(fctrl);
		break;

	case MSM_CAMERA_LED_RELEASE:
		if (fctrl->func_tbl->flash_led_release)
			rc = fctrl->func_tbl->
				flash_led_release(fctrl);
		break;

	case MSM_CAMERA_LED_OFF:
		if (fctrl->func_tbl->flash_led_off)
			rc = fctrl->func_tbl->flash_led_off(fctrl);
		break;

	case MSM_CAMERA_LED_LOW:
		if (fctrl->func_tbl->flash_led_low)
			rc = fctrl->func_tbl->flash_led_low(fctrl);
		break;

	case MSM_CAMERA_LED_HIGH:
		if (fctrl->func_tbl->flash_led_high)
			rc = fctrl->func_tbl->flash_led_high(fctrl);
		break;

	default:
		rc = -EFAULT;
		break;
	}
	return rc;
}

static struct msm_flash_ctrl_t *get_fctrl(struct v4l2_subdev *sd)
{
	return container_of(sd, struct msm_flash_ctrl_t, v4l2_sdev);
}

static long msm_flash_config(struct msm_flash_ctrl_t *fctrl, void __user *argp)
{
	long rc = 0;
	struct flash_ctrl_data flash_info;
	if (!argp) {
		pr_err("%s argp NULL\n", __func__);
		return -EINVAL;
	}
	if (copy_from_user(&flash_info, argp, sizeof(flash_info))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EFAULT;
	}
	switch (flash_info.flashtype) {
	case LED_FLASH:
		if (fctrl->func_tbl->flash_led_config)
			rc = fctrl->func_tbl->flash_led_config(fctrl,
				flash_info.ctrl_data.led_state);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
		break;
	case STROBE_FLASH:
		rc = msm_strobe_flash_ctrl(fctrl->strobe_flash_data,
			&(flash_info.ctrl_data.strobe_ctrl));
		break;
	default:
		pr_err("Invalid Flash MODE\n");
		rc = -EINVAL;
	}
	return rc;
}

static long msm_flash_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_flash_ctrl_t *fctrl = NULL;
	void __user *argp = (void __user *)arg;
	if (!sd) {
		pr_err("%s:%d sd NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	fctrl = get_fctrl(sd);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
	case VIDIOC_MSM_FLASH_LED_DATA_CFG:
		fctrl->flash_data = (struct msm_camera_sensor_flash_data *)argp;
		return 0;
	case VIDIOC_MSM_FLASH_STROBE_DATA_CFG:
		fctrl->strobe_flash_data =
			(struct msm_camera_sensor_strobe_flash_data *)argp;
		return 0;
	case VIDIOC_MSM_FLASH_CFG:
		return msm_flash_config(fctrl, argp);
	default:
		return -ENOIOCTLCMD;
	}
}

static struct v4l2_subdev_core_ops msm_flash_subdev_core_ops = {
	.ioctl = msm_flash_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_flash_subdev_ops = {
	.core = &msm_flash_subdev_core_ops,
};

int msm_flash_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_flash_ctrl_t *fctrl = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	fctrl = (struct msm_flash_ctrl_t *)(id->driver_data);
	if (fctrl->flash_i2c_client)
		fctrl->flash_i2c_client->client = client;

	/* Assign name for sub device */
	snprintf(fctrl->v4l2_sdev.name, sizeof(fctrl->v4l2_sdev.name),
		"%s", id->name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&fctrl->v4l2_sdev, client, &msm_flash_subdev_ops);

	CDBG("%s:%d probe success\n", __func__, __LINE__);
	return 0;

probe_failure:
	CDBG("%s:%d probe failed\n", __func__, __LINE__);
	return rc;
}

int msm_flash_platform_probe(struct platform_device *pdev, void *data)
{
	struct msm_flash_ctrl_t *fctrl = (struct msm_flash_ctrl_t *)data;
	struct msm_cam_subdev_info sd_info;
	CDBG("%s:%d called\n", __func__, __LINE__);

	if (!fctrl) {
		pr_err("%s fctrl NULL\n", __func__);
		return -EINVAL;
	}

	/* Initialize sub device */
	v4l2_subdev_init(&fctrl->v4l2_sdev, &msm_flash_subdev_ops);

	/* Assign name for sub device */
	snprintf(fctrl->v4l2_sdev.name, sizeof(fctrl->v4l2_sdev.name),
		"%s", "msm_flash");

	fctrl->pdev = pdev;
	sd_info.sdev_type = FLASH_DEV;
	sd_info.sd_index = pdev->id;
	msm_cam_register_subdev_node(&fctrl->v4l2_sdev, &sd_info);

	CDBG("%s:%d probe success\n", __func__, __LINE__);
	return 0;
}

int msm_flash_create_v4l2_subdev(void *data, uint8_t sd_index)
{
	struct msm_flash_ctrl_t *fctrl = (struct msm_flash_ctrl_t *)data;
	struct msm_cam_subdev_info sd_info;
	CDBG("%s:%d called\n", __func__, __LINE__);

	/* Initialize sub device */
	v4l2_subdev_init(&fctrl->v4l2_sdev, &msm_flash_subdev_ops);

	/* Assign name for sub device */
	snprintf(fctrl->v4l2_sdev.name, sizeof(fctrl->v4l2_sdev.name),
		"%s", "msm_flash");

	sd_info.sdev_type = FLASH_DEV;
	sd_info.sd_index = sd_index;
	msm_cam_register_subdev_node(&fctrl->v4l2_sdev, &sd_info);

	CDBG("%s:%d probe success\n", __func__, __LINE__);
	return 0;
}
