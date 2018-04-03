/* Copyright (c) 2009-2018, The Linux Foundation. All rights reserved.
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
#include <linux/leds-qpnp-flash.h>
#include "msm_flash.h"
#include "msm_camera_dt_util.h"
#include "msm_cci.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

DEFINE_MSM_MUTEX(msm_flash_mutex);

static struct v4l2_file_operations msm_flash_v4l2_subdev_fops;
static struct led_trigger *torch_trigger;

static const struct of_device_id msm_flash_dt_match[] = {
	{.compatible = "qcom,camera-flash", .data = NULL},
	{}
};

static struct msm_flash_table msm_i2c_flash_table;
static struct msm_flash_table msm_gpio_flash_table;
static struct msm_flash_table msm_pmic_flash_table;

static struct msm_flash_table *flash_table[] = {
	&msm_i2c_flash_table,
	&msm_gpio_flash_table,
	&msm_pmic_flash_table
};

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

static void msm_torch_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	if (!torch_trigger) {
		pr_err("No torch trigger found, can't set brightness\n");
		return;
	}

	led_trigger_event(torch_trigger, value);
};

static struct led_classdev msm_torch_led[MAX_LED_TRIGGERS] = {
	{
		.name		= "torch-light0",
		.brightness_set	= msm_torch_brightness_set,
		.brightness	= LED_OFF,
	},
	{
		.name		= "torch-light1",
		.brightness_set	= msm_torch_brightness_set,
		.brightness	= LED_OFF,
	},
	{
		.name		= "torch-light2",
		.brightness_set	= msm_torch_brightness_set,
		.brightness	= LED_OFF,
	},
};

static int32_t msm_torch_create_classdev(struct platform_device *pdev,
				void *data)
{
	int32_t rc = 0;
	int32_t i = 0;
	struct msm_flash_ctrl_t *fctrl =
		(struct msm_flash_ctrl_t *)data;

	if (!fctrl) {
		pr_err("Invalid fctrl\n");
		return -EINVAL;
	}

	for (i = 0; i < fctrl->torch_num_sources; i++) {
		if (fctrl->torch_trigger[i]) {
			torch_trigger = fctrl->torch_trigger[i];
			CDBG("%s:%d msm_torch_brightness_set for torch %d",
				__func__, __LINE__, i);
			msm_torch_brightness_set(&msm_torch_led[i],
				LED_OFF);

			rc = led_classdev_register(&pdev->dev,
				&msm_torch_led[i]);
			if (rc) {
				pr_err("Failed to register %d led dev. rc = %d\n",
						i, rc);
				return rc;
			}
		} else {
			pr_err("Invalid fctrl->torch_trigger[%d]\n", i);
			return -EINVAL;
		}
	}

	return 0;
};

static int32_t msm_flash_get_subdev_id(
	struct msm_flash_ctrl_t *flash_ctrl, void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;

	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (flash_ctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = flash_ctrl->pdev->id;
	else
		*subdev_id = flash_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_flash_i2c_write_table(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_camera_i2c_reg_setting_array *settings)
{
	struct msm_camera_i2c_reg_setting conf_array;

	conf_array.addr_type = settings->addr_type;
	conf_array.data_type = settings->data_type;
	conf_array.delay = settings->delay;
	conf_array.reg_setting = settings->reg_setting_a;
	conf_array.size = settings->size;

	/* Validate the settings size */
	if ((!conf_array.size) || (conf_array.size > MAX_I2C_REG_SET)) {
		pr_err("failed: invalid size %d", conf_array.size);
		return -EINVAL;
	}

	return flash_ctrl->flash_i2c_client.i2c_func_tbl->i2c_write_table(
		&flash_ctrl->flash_i2c_client, &conf_array);
}

#ifdef CONFIG_COMPAT
static void msm_flash_copy_power_settings_compat(
	struct msm_sensor_power_setting *ps,
	struct msm_sensor_power_setting32 *ps32, uint32_t size)
{
	uint16_t i = 0;

	for (i = 0; i < size; i++) {
		ps[i].config_val = ps32[i].config_val;
		ps[i].delay = ps32[i].delay;
		ps[i].seq_type = ps32[i].seq_type;
		ps[i].seq_val = ps32[i].seq_val;
	}
}
#endif

static int32_t msm_flash_i2c_init(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t rc = 0;
	struct msm_flash_init_info_t *flash_init_info =
		flash_data->cfg.flash_init_info;
	struct msm_camera_i2c_reg_setting_array *settings = NULL;
	struct msm_camera_cci_client *cci_client = NULL;
#ifdef CONFIG_COMPAT
	struct msm_sensor_power_setting_array32 *power_setting_array32 = NULL;
#endif
	if (!flash_init_info || !flash_init_info->power_setting_array) {
		pr_err("%s:%d failed: Null pointer\n", __func__, __LINE__);
		return -EFAULT;
	}

#ifdef CONFIG_COMPAT
	if (is_compat_task()) {
		power_setting_array32 = kzalloc(
			sizeof(struct msm_sensor_power_setting_array32),
			GFP_KERNEL);
		if (!power_setting_array32) {
			pr_err("%s mem allocation failed %d\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		if (copy_from_user(power_setting_array32,
			(void __user *)flash_init_info->power_setting_array,
			sizeof(struct msm_sensor_power_setting_array32))) {
			pr_err("%s copy_from_user failed %d\n",
				__func__, __LINE__);
			kfree(power_setting_array32);
			return -EFAULT;
		}

		flash_ctrl->power_setting_array.size =
			power_setting_array32->size;
		flash_ctrl->power_setting_array.size_down =
			power_setting_array32->size_down;

		flash_ctrl->power_setting_array.power_down_setting =
			kzalloc(sizeof(struct msm_sensor_power_setting)*
			flash_ctrl->power_setting_array.size_down, GFP_KERNEL);
		if (!flash_ctrl->power_setting_array.power_down_setting)
			return -ENOMEM;
		if (copy_from_user(
			flash_ctrl->power_setting_array.power_down_setting,
			(void __user *)
			compat_ptr(power_setting_array32->power_down_setting),
			sizeof(struct msm_sensor_power_setting)*
			flash_ctrl->power_setting_array.size_down)) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			return -EFAULT;
		}

		flash_ctrl->power_setting_array.power_setting =
			kzalloc(sizeof(struct msm_sensor_power_setting)*
			flash_ctrl->power_setting_array.size, GFP_KERNEL);
		if (!flash_ctrl->power_setting_array.power_setting)
			return -ENOMEM;
		if (copy_from_user(
			flash_ctrl->power_setting_array.power_setting,
			(void __user *)
			compat_ptr(power_setting_array32->power_setting),
			sizeof(struct msm_sensor_power_setting)*
			flash_ctrl->power_setting_array.size)) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			return -EFAULT;
		}

		/* Validate power_up array size and power_down array size */
		if ((!flash_ctrl->power_setting_array.size) ||
			(flash_ctrl->power_setting_array.size >
			MAX_POWER_CONFIG) ||
			(!flash_ctrl->power_setting_array.size_down) ||
			(flash_ctrl->power_setting_array.size_down >
			MAX_POWER_CONFIG)) {

			pr_err("failed: invalid size %d, size_down %d",
				flash_ctrl->power_setting_array.size,
				flash_ctrl->power_setting_array.size_down);
			kfree(power_setting_array32);
			power_setting_array32 = NULL;
			return -EINVAL;
		}
		/* Copy the settings from compat struct to regular struct */
		msm_flash_copy_power_settings_compat(
			flash_ctrl->power_setting_array.power_setting_a,
			power_setting_array32->power_setting_a,
			flash_ctrl->power_setting_array.size);

		msm_flash_copy_power_settings_compat(
			flash_ctrl->power_setting_array.power_down_setting_a,
			power_setting_array32->power_down_setting_a,
			flash_ctrl->power_setting_array.size_down);
	} else
#endif
	if (copy_from_user(&flash_ctrl->power_setting_array,
		(void __user *)flash_init_info->power_setting_array,
		sizeof(struct msm_sensor_power_setting_array))) {
		pr_err("%s copy_from_user failed %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (flash_ctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = flash_ctrl->flash_i2c_client.cci_client;
		cci_client->sid = flash_init_info->slave_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->i2c_freq_mode = flash_init_info->i2c_freq_mode;
	}

	flash_ctrl->power_info.power_setting =
		flash_ctrl->power_setting_array.power_setting_a;
	flash_ctrl->power_info.power_down_setting =
		flash_ctrl->power_setting_array.power_down_setting_a;
	flash_ctrl->power_info.power_setting_size =
		flash_ctrl->power_setting_array.size;
	flash_ctrl->power_info.power_down_setting_size =
		flash_ctrl->power_setting_array.size_down;

	if ((flash_ctrl->power_info.power_setting_size > MAX_POWER_CONFIG) ||
	(flash_ctrl->power_info.power_down_setting_size > MAX_POWER_CONFIG)) {
		pr_err("%s:%d invalid power setting size=%d size_down=%d\n",
			__func__, __LINE__,
			flash_ctrl->power_info.power_setting_size,
			flash_ctrl->power_info.power_down_setting_size);
		rc = -EINVAL;
		goto msm_flash_i2c_init_fail;
	}

	rc = msm_camera_power_up(&flash_ctrl->power_info,
		flash_ctrl->flash_device_type,
		&flash_ctrl->flash_i2c_client);
	if (rc < 0) {
		pr_err("%s msm_camera_power_up failed %d\n",
			__func__, __LINE__);
		goto msm_flash_i2c_init_fail;
	}

	if (flash_data->cfg.flash_init_info->settings) {
		settings = kzalloc(sizeof(
			struct msm_camera_i2c_reg_setting_array), GFP_KERNEL);
		if (!settings)
			return -ENOMEM;

		if (copy_from_user(settings, (void __user *)
			flash_init_info->settings,
			sizeof(struct msm_camera_i2c_reg_setting_array))) {
			kfree(settings);
			pr_err("%s copy_from_user failed %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		rc = msm_flash_i2c_write_table(flash_ctrl, settings);
		kfree(settings);

		if (rc < 0) {
			pr_err("%s:%d msm_flash_i2c_write_table rc %d failed\n",
				__func__, __LINE__, rc);
		}
	}

	return 0;

msm_flash_i2c_init_fail:
	return rc;
}

static int32_t msm_flash_gpio_init(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t i = 0;
	int32_t rc = 0;

	CDBG("Enter");
	for (i = 0; i < flash_ctrl->flash_num_sources; i++)
		flash_ctrl->flash_op_current[i] = LED_FULL;

	for (i = 0; i < flash_ctrl->torch_num_sources; i++)
		flash_ctrl->torch_op_current[i] = LED_HALF;

	for (i = 0; i < flash_ctrl->torch_num_sources; i++) {
		if (!flash_ctrl->torch_trigger[i]) {
			if (i < flash_ctrl->flash_num_sources)
				flash_ctrl->torch_trigger[i] =
					flash_ctrl->flash_trigger[i];
			else
				flash_ctrl->torch_trigger[i] =
					flash_ctrl->flash_trigger[
					flash_ctrl->flash_num_sources - 1];
		}
	}

	rc = flash_ctrl->func_tbl->camera_flash_off(flash_ctrl, flash_data);

	CDBG("Exit");
	return rc;
}

static int32_t msm_flash_i2c_release(
	struct msm_flash_ctrl_t *flash_ctrl)
{
	int32_t rc = 0;

	if (!(&flash_ctrl->power_info) || !(&flash_ctrl->flash_i2c_client)) {
		pr_err("%s:%d failed: %pK %pK\n",
			__func__, __LINE__, &flash_ctrl->power_info,
			&flash_ctrl->flash_i2c_client);
		return -EINVAL;
	}

	rc = msm_camera_power_down(&flash_ctrl->power_info,
		flash_ctrl->flash_device_type,
		&flash_ctrl->flash_i2c_client);
	if (rc < 0) {
		pr_err("%s msm_camera_power_down failed %d\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

static int32_t msm_flash_off(struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t i = 0;

	CDBG("Enter\n");

	for (i = 0; i < flash_ctrl->flash_num_sources; i++)
		if (flash_ctrl->flash_trigger[i])
			led_trigger_event(flash_ctrl->flash_trigger[i], 0);

	for (i = 0; i < flash_ctrl->torch_num_sources; i++)
		if (flash_ctrl->torch_trigger[i])
			led_trigger_event(flash_ctrl->torch_trigger[i], 0);
	if (flash_ctrl->switch_trigger)
		led_trigger_event(flash_ctrl->switch_trigger, 0);

	CDBG("Exit\n");
	return 0;
}

static int32_t msm_flash_i2c_write_setting_array(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t rc = 0;
	struct msm_camera_i2c_reg_setting_array *settings = NULL;

	if (!flash_data->cfg.settings) {
		pr_err("%s:%d failed: Null pointer\n", __func__, __LINE__);
		return -EFAULT;
	}

	settings = kzalloc(sizeof(struct msm_camera_i2c_reg_setting_array),
		GFP_KERNEL);
	if (!settings)
		return -ENOMEM;

	if (copy_from_user(settings, (void __user *)flash_data->cfg.settings,
		sizeof(struct msm_camera_i2c_reg_setting_array))) {
		kfree(settings);
		pr_err("%s copy_from_user failed %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	rc = msm_flash_i2c_write_table(flash_ctrl, settings);
	kfree(settings);

	if (rc < 0) {
		pr_err("%s:%d msm_flash_i2c_write_table rc = %d failed\n",
			__func__, __LINE__, rc);
	}
	return rc;
}

static int32_t msm_flash_init(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	uint32_t i = 0;
	int32_t rc = -EFAULT;
	enum msm_flash_driver_type flash_driver_type = FLASH_DRIVER_DEFAULT;

	CDBG("Enter");

	if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT) {
		pr_err("%s:%d Invalid flash state = %d",
			__func__, __LINE__, flash_ctrl->flash_state);
		return 0;
	}

	if (flash_data->cfg.flash_init_info->flash_driver_type ==
		FLASH_DRIVER_DEFAULT) {
		flash_driver_type = flash_ctrl->flash_driver_type;
		for (i = 0; i < MAX_LED_TRIGGERS; i++) {
			flash_data->flash_current[i] =
				flash_ctrl->flash_max_current[i];
			flash_data->flash_duration[i] =
				flash_ctrl->flash_max_duration[i];
		}
	} else if (flash_data->cfg.flash_init_info->flash_driver_type ==
		flash_ctrl->flash_driver_type) {
		flash_driver_type = flash_ctrl->flash_driver_type;
		for (i = 0; i < MAX_LED_TRIGGERS; i++) {
			flash_ctrl->flash_max_current[i] =
				flash_data->flash_current[i];
			flash_ctrl->flash_max_duration[i] =
					flash_data->flash_duration[i];
		}
	}

	if (flash_driver_type == FLASH_DRIVER_DEFAULT) {
		pr_err("%s:%d invalid flash_driver_type", __func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(flash_table); i++) {
		if (flash_driver_type == flash_table[i]->flash_driver_type) {
			flash_ctrl->func_tbl = &flash_table[i]->func_tbl;
			rc = 0;
		}
	}

	if (rc < 0) {
		pr_err("%s:%d failed invalid flash_driver_type %d\n",
			__func__, __LINE__,
			flash_data->cfg.flash_init_info->flash_driver_type);
	}

	if (flash_ctrl->func_tbl->camera_flash_init) {
		rc = flash_ctrl->func_tbl->camera_flash_init(
				flash_ctrl, flash_data);
		if (rc < 0) {
			pr_err("%s:%d camera_flash_init failed rc = %d",
				__func__, __LINE__, rc);
			return rc;
		}
	}

	flash_ctrl->flash_state = MSM_CAMERA_FLASH_INIT;

	CDBG("Exit");
	return 0;
}

static int32_t msm_flash_init_prepare(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
#ifdef CONFIG_COMPAT
	struct msm_flash_cfg_data_t flash_data_k;
	struct msm_flash_init_info_t flash_init_info;
	int32_t i = 0;

	if (!is_compat_task()) {
		/*for 64-bit usecase,it need copy the data to local memory*/
		flash_data_k.cfg_type = flash_data->cfg_type;
		for (i = 0; i < MAX_LED_TRIGGERS; i++) {
			flash_data_k.flash_current[i] =
				flash_data->flash_current[i];
			flash_data_k.flash_duration[i] =
				flash_data->flash_duration[i];
		}

		flash_data_k.cfg.flash_init_info = &flash_init_info;
		if (copy_from_user(&flash_init_info,
			(void __user *)(flash_data->cfg.flash_init_info),
			sizeof(struct msm_flash_init_info_t))) {
			pr_err("%s copy_from_user failed %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		return msm_flash_init(flash_ctrl, &flash_data_k);
	}
	/*
	 * for 32-bit usecase,it already copy the userspace
	 * data to local memory in msm_flash_subdev_do_ioctl()
	 * so here do not need copy from user
	 */
	return msm_flash_init(flash_ctrl, flash_data);
#else
	struct msm_flash_cfg_data_t flash_data_k;
	struct msm_flash_init_info_t flash_init_info;
	int32_t i = 0;

	flash_data_k.cfg_type = flash_data->cfg_type;
	for (i = 0; i < MAX_LED_TRIGGERS; i++) {
		flash_data_k.flash_current[i] =
			flash_data->flash_current[i];
		flash_data_k.flash_duration[i] =
			flash_data->flash_duration[i];
	}

	flash_data_k.cfg.flash_init_info = &flash_init_info;
	if (copy_from_user(&flash_init_info,
		(void __user *)(flash_data->cfg.flash_init_info),
		sizeof(struct msm_flash_init_info_t))) {
		pr_err("%s copy_from_user failed %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	return msm_flash_init(flash_ctrl, &flash_data_k);
#endif
}

static int32_t msm_flash_prepare(
	struct msm_flash_ctrl_t *flash_ctrl)
{
	int32_t ret = 0;

	CDBG("%s:%d: State : %d\n",
		__func__, __LINE__, flash_ctrl->flash_state);

	if (flash_ctrl->switch_trigger == NULL) {
		pr_err("%s:%d Invalid argument\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT &&
		flash_ctrl->is_regulator_enabled == 0) {
		ret = qpnp_flash_led_prepare(flash_ctrl->switch_trigger,
				ENABLE_REGULATOR, NULL);
		if (ret < 0) {
			pr_err("%s:%d regulator enable failed ret = %d\n",
				__func__, __LINE__, ret);
			return ret;
		}
		flash_ctrl->is_regulator_enabled = 1;
	} else if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_RELEASE &&
		flash_ctrl->is_regulator_enabled) {
		ret = qpnp_flash_led_prepare(flash_ctrl->switch_trigger,
				DISABLE_REGULATOR, NULL);
		if (ret < 0) {
			pr_err("%s:%d regulator disable failed ret = %d\n",
				__func__, __LINE__, ret);
			return ret;
		}
		flash_ctrl->is_regulator_enabled = 0;
	}
	CDBG("%s:%d:Exit\n", __func__, __LINE__);
	return ret;
}

static int32_t msm_flash_low(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	uint32_t curr = 0, max_current = 0;
	int32_t i = 0;

	CDBG("Enter\n");
	/* Turn off flash triggers */
	for (i = 0; i < flash_ctrl->flash_num_sources; i++)
		if (flash_ctrl->flash_trigger[i])
			led_trigger_event(flash_ctrl->flash_trigger[i], 0);

	/* Turn on flash triggers */
	for (i = 0; i < flash_ctrl->torch_num_sources; i++) {
		if (flash_ctrl->torch_trigger[i]) {
			max_current = flash_ctrl->torch_max_current[i];
			if (flash_data->flash_current[i] >= 0 &&
				flash_data->flash_current[i] <
				max_current) {
				curr = flash_data->flash_current[i];
			} else {
				curr = flash_ctrl->torch_op_current[i];
				pr_debug("LED current clamped to %d\n",
					curr);
			}
			CDBG("low_flash_current[%d] = %d", i, curr);
			led_trigger_event(flash_ctrl->torch_trigger[i],
				curr);
		}
	}
	if (flash_ctrl->switch_trigger)
		led_trigger_event(flash_ctrl->switch_trigger, 1);
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_flash_high(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t curr = 0;
	int32_t max_current = 0;
	int32_t i = 0;

	/* Turn off torch triggers */
	for (i = 0; i < flash_ctrl->torch_num_sources; i++)
		if (flash_ctrl->torch_trigger[i])
			led_trigger_event(flash_ctrl->torch_trigger[i], 0);

	/* Turn on flash triggers */
	for (i = 0; i < flash_ctrl->flash_num_sources; i++) {
		if (flash_ctrl->flash_trigger[i]) {
			max_current = flash_ctrl->flash_max_current[i];
			if (flash_data->flash_current[i] >= 0 &&
				flash_data->flash_current[i] <
				max_current) {
				curr = flash_data->flash_current[i];
			} else {
				curr = flash_ctrl->flash_op_current[i];
				pr_debug("LED flash_current[%d] clamped %d\n",
					i, curr);
			}
			CDBG("high_flash_current[%d] = %d", i, curr);
			led_trigger_event(flash_ctrl->flash_trigger[i],
				curr);
		}
	}
	if (flash_ctrl->switch_trigger)
		led_trigger_event(flash_ctrl->switch_trigger, 1);
	return 0;
}

static int32_t msm_flash_query_current(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_query_data_t *flash_query_data)
{
	int32_t ret = -EINVAL;
	int32_t max_current = -EINVAL;

	if (flash_ctrl->switch_trigger) {
		ret = qpnp_flash_led_prepare(flash_ctrl->switch_trigger,
					QUERY_MAX_CURRENT, &max_current);
		if (ret < 0) {
			pr_err("%s:%d Query max_avail_curr failed ret = %d\n",
				__func__, __LINE__, ret);
			return ret;
		}
	}

	flash_query_data->max_avail_curr = max_current;
	CDBG("%s: %d: max_avail_curr : %d\n", __func__, __LINE__,
			flash_query_data->max_avail_curr);
	return 0;
}

static int32_t msm_flash_release(
	struct msm_flash_ctrl_t *flash_ctrl)
{
	int32_t rc = 0;

	rc = flash_ctrl->func_tbl->camera_flash_off(flash_ctrl, NULL);
	if (rc < 0) {
		pr_err("%s:%d camera_flash_init failed rc = %d",
			__func__, __LINE__, rc);
		return rc;
	}
	flash_ctrl->flash_state = MSM_CAMERA_FLASH_RELEASE;
	return 0;
}

static int32_t msm_flash_config(struct msm_flash_ctrl_t *flash_ctrl,
	void *argp)
{
	int32_t rc = 0;
	struct msm_flash_cfg_data_t *flash_data =
		(struct msm_flash_cfg_data_t *) argp;

	mutex_lock(flash_ctrl->flash_mutex);

	CDBG("Enter %s type %d\n", __func__, flash_data->cfg_type);

	switch (flash_data->cfg_type) {
	case CFG_FLASH_INIT:
		rc = msm_flash_init_prepare(flash_ctrl, flash_data);
		break;
	case CFG_FLASH_RELEASE:
		if (flash_ctrl->flash_state != MSM_CAMERA_FLASH_RELEASE) {
			rc = flash_ctrl->func_tbl->camera_flash_release(
				flash_ctrl);
		} else {
			CDBG(pr_fmt("Invalid state : %d\n"),
				flash_ctrl->flash_state);
		}
		break;
	case CFG_FLASH_OFF:
		if ((flash_ctrl->flash_state != MSM_CAMERA_FLASH_RELEASE) &&
			(flash_ctrl->flash_state != MSM_CAMERA_FLASH_OFF)) {
			rc = flash_ctrl->func_tbl->camera_flash_off(
				flash_ctrl, flash_data);
			if (!rc)
				flash_ctrl->flash_state = MSM_CAMERA_FLASH_OFF;
		} else {
			CDBG(pr_fmt("Invalid state : %d\n"),
				flash_ctrl->flash_state);
		}
		break;
	case CFG_FLASH_LOW:
		if ((flash_ctrl->flash_state == MSM_CAMERA_FLASH_OFF) ||
			(flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)) {
			rc = flash_ctrl->func_tbl->camera_flash_low(
				flash_ctrl, flash_data);
			if (!rc)
				flash_ctrl->flash_state = MSM_CAMERA_FLASH_LOW;
		} else {
			CDBG(pr_fmt("Invalid state : %d\n"),
				flash_ctrl->flash_state);
		}
		break;
	case CFG_FLASH_HIGH:
		if ((flash_ctrl->flash_state == MSM_CAMERA_FLASH_OFF) ||
			(flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)) {
			rc = flash_ctrl->func_tbl->camera_flash_high(
				flash_ctrl, flash_data);
			if (!rc)
				flash_ctrl->flash_state = MSM_CAMERA_FLASH_HIGH;
		} else {
			CDBG(pr_fmt("Invalid state : %d\n"),
				flash_ctrl->flash_state);
		}
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(flash_ctrl->flash_mutex);

	rc = msm_flash_prepare(flash_ctrl);
	if (rc < 0) {
		pr_err("%s:%d Enable/Disable Regulator failed ret = %d",
			__func__, __LINE__, rc);
		return rc;
	}

	CDBG("Exit %s type %d\n", __func__, flash_data->cfg_type);

	return rc;
}

static int32_t msm_flash_query_data(struct msm_flash_ctrl_t *flash_ctrl,
	void *argp)
{
	int32_t rc = -EINVAL, i = 0;
	struct msm_flash_query_data_t *flash_query =
		(struct msm_flash_query_data_t *) argp;

	CDBG("Enter %s type %d\n", __func__, flash_query->query_type);

	switch (flash_query->query_type) {
	case FLASH_QUERY_CURRENT:
		if (flash_ctrl->func_tbl && flash_ctrl->func_tbl->
				camera_flash_query_current != NULL)
			rc = flash_ctrl->func_tbl->
				camera_flash_query_current(
				flash_ctrl, flash_query);
		else {
			flash_query->max_avail_curr = 0;
			for (i = 0; i < flash_ctrl->flash_num_sources; i++) {
				flash_query->max_avail_curr +=
					flash_ctrl->flash_op_current[i];
			}
			rc = 0;
			CDBG("%s: max_avail_curr: %d\n", __func__,
				flash_query->max_avail_curr);
		}
		break;
	default:
		rc = -EFAULT;
		break;
	}

	CDBG("Exit %s type %d\n", __func__, flash_query->query_type);

	return rc;
}

static long msm_flash_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct msm_flash_ctrl_t *fctrl = NULL;
	void *argp = (void *)arg;

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
		return msm_flash_get_subdev_id(fctrl, argp);
	case VIDIOC_MSM_FLASH_CFG:
		return msm_flash_config(fctrl, argp);
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_UNNOTIFY_FREEZE:
		return 0;
	case MSM_SD_SHUTDOWN:
		if (!fctrl->func_tbl) {
			pr_err("fctrl->func_tbl NULL\n");
		} else {
			fctrl->func_tbl->camera_flash_release(fctrl);
			return msm_flash_prepare(fctrl);
		}
		return -EINVAL;
	case VIDIOC_MSM_FLASH_QUERY_DATA:
		return msm_flash_query_data(fctrl, argp);
	default:
		pr_err_ratelimited("invalid cmd %d\n", cmd);
		return -ENOIOCTLCMD;
	}
	CDBG("Exit\n");
}

static struct v4l2_subdev_core_ops msm_flash_subdev_core_ops = {
	.ioctl = msm_flash_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_flash_subdev_ops = {
	.core = &msm_flash_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_flash_internal_ops;

static int32_t msm_flash_get_pmic_source_info(
	struct device_node *of_node,
	struct msm_flash_ctrl_t *fctrl)
{
	int32_t rc = 0;
	uint32_t count = 0, i = 0;
	struct device_node *flash_src_node = NULL;
	struct device_node *torch_src_node = NULL;
	struct device_node *switch_src_node = NULL;

	switch_src_node = of_parse_phandle(of_node, "qcom,switch-source", 0);
	if (!switch_src_node) {
		CDBG("%s:%d switch_src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_string(switch_src_node,
			"qcom,default-led-trigger",
			&fctrl->switch_trigger_name);
		if (rc < 0) {
			rc = of_property_read_string(switch_src_node,
				"linux,default-trigger",
				&fctrl->switch_trigger_name);
			if (rc < 0)
				pr_err("default-trigger read failed\n");
		}
		of_node_put(switch_src_node);
		switch_src_node = NULL;
		if (!rc) {
			CDBG("switch trigger %s\n",
				fctrl->switch_trigger_name);
			led_trigger_register_simple(
				fctrl->switch_trigger_name,
				&fctrl->switch_trigger);
		}
	}

	if (of_get_property(of_node, "qcom,flash-source", &count)) {
		count /= sizeof(uint32_t);
		CDBG("count %d\n", count);
		if (count > MAX_LED_TRIGGERS) {
			pr_err("invalid count\n");
			return -EINVAL;
		}
		fctrl->flash_num_sources = count;
		CDBG("%s:%d flash_num_sources = %d",
			__func__, __LINE__, fctrl->flash_num_sources);
		for (i = 0; i < count; i++) {
			flash_src_node = of_parse_phandle(of_node,
				"qcom,flash-source", i);
			if (!flash_src_node) {
				pr_err("flash_src_node NULL\n");
				continue;
			}

			rc = of_property_read_string(flash_src_node,
				"qcom,default-led-trigger",
				&fctrl->flash_trigger_name[i]);
			if (rc < 0) {
				rc = of_property_read_string(flash_src_node,
					"linux,default-trigger",
					&fctrl->flash_trigger_name[i]);
				if (rc < 0) {
					pr_err("default-trigger read failed\n");
					of_node_put(flash_src_node);
					continue;
				}
			}

			CDBG("default trigger %s\n",
				fctrl->flash_trigger_name[i]);

			/* Read operational-current */
			rc = of_property_read_u32(flash_src_node,
				"qcom,current",
				&fctrl->flash_op_current[i]);
			if (rc < 0) {
				rc = of_property_read_u32(flash_src_node,
					"qcom,current-ma",
					&fctrl->flash_op_current[i]);
				if (rc < 0) {
					pr_err("current: read failed\n");
					of_node_put(flash_src_node);
					continue;
				}
			}

			/* Read max-current */
			rc = of_property_read_u32(flash_src_node,
				"qcom,max-current",
				&fctrl->flash_max_current[i]);
			if (rc < 0) {
				pr_err("current: read failed\n");
				of_node_put(flash_src_node);
				continue;
			}

			/* Read max-duration */
			rc = of_property_read_u32(flash_src_node,
				"qcom,duration",
				&fctrl->flash_max_duration[i]);
			if (rc < 0) {
				rc = of_property_read_u32(flash_src_node,
					"qcom,duration-ms",
					&fctrl->flash_max_duration[i]);
				if (rc < 0) {
					pr_err("duration: read failed\n");
					of_node_put(flash_src_node);
				}
				/* Non-fatal; this property is optional */
			}

			of_node_put(flash_src_node);

			CDBG("max_current[%d] %d\n",
				i, fctrl->flash_op_current[i]);

			led_trigger_register_simple(
				fctrl->flash_trigger_name[i],
				&fctrl->flash_trigger[i]);
		}
		if (fctrl->flash_driver_type == FLASH_DRIVER_DEFAULT)
			fctrl->flash_driver_type = FLASH_DRIVER_PMIC;
		CDBG("%s:%d fctrl->flash_driver_type = %d", __func__, __LINE__,
			fctrl->flash_driver_type);
	}

	if (of_get_property(of_node, "qcom,torch-source", &count)) {
		count /= sizeof(uint32_t);
		CDBG("count %d\n", count);
		if (count > MAX_LED_TRIGGERS) {
			pr_err("invalid count\n");
			return -EINVAL;
		}
		fctrl->torch_num_sources = count;
		CDBG("%s:%d torch_num_sources = %d",
			__func__, __LINE__, fctrl->torch_num_sources);
		for (i = 0; i < count; i++) {
			torch_src_node = of_parse_phandle(of_node,
				"qcom,torch-source", i);
			if (!torch_src_node) {
				pr_err("torch_src_node NULL\n");
				continue;
			}

			rc = of_property_read_string(torch_src_node,
				"qcom,default-led-trigger",
				&fctrl->torch_trigger_name[i]);
			if (rc < 0) {
				rc = of_property_read_string(torch_src_node,
					"linux,default-trigger",
					&fctrl->torch_trigger_name[i]);
				if (rc < 0) {
					pr_err("default-trigger read failed\n");
					of_node_put(torch_src_node);
					continue;
				}
			}

			CDBG("default trigger %s\n",
				fctrl->torch_trigger_name[i]);

			/* Read operational-current */
			rc = of_property_read_u32(torch_src_node,
				"qcom,current",
				&fctrl->torch_op_current[i]);
			if (rc < 0) {
				rc = of_property_read_u32(torch_src_node,
					"qcom,current-ma",
					&fctrl->torch_op_current[i]);
				if (rc < 0) {
					pr_err("current: read failed\n");
					of_node_put(torch_src_node);
					continue;
				}
			}

			/* Read max-current */
			rc = of_property_read_u32(torch_src_node,
				"qcom,max-current",
				&fctrl->torch_max_current[i]);
			if (rc < 0) {
				pr_err("current: read failed\n");
				of_node_put(torch_src_node);
				continue;
			}

			of_node_put(torch_src_node);

			CDBG("max_current[%d] %d\n",
				i, fctrl->torch_op_current[i]);

			led_trigger_register_simple(
				fctrl->torch_trigger_name[i],
				&fctrl->torch_trigger[i]);
		}
		if (fctrl->flash_driver_type == FLASH_DRIVER_DEFAULT)
			fctrl->flash_driver_type = FLASH_DRIVER_PMIC;
		CDBG("%s:%d fctrl->flash_driver_type = %d", __func__, __LINE__,
			fctrl->flash_driver_type);
	}

	return 0;
}

static int32_t msm_flash_get_dt_data(struct device_node *of_node,
	struct msm_flash_ctrl_t *fctrl)
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

	CDBG("subdev id %d\n", fctrl->subdev_id);

	fctrl->flash_driver_type = FLASH_DRIVER_DEFAULT;

	/* Read the CCI master. Use M0 if not available in the node */
	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&fctrl->cci_i2c_master);
	CDBG("%s qcom,cci-master %d, rc %d\n", __func__, fctrl->cci_i2c_master,
		rc);
	if (rc < 0) {
		/* Set default master 0 */
		fctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	} else {
		fctrl->flash_driver_type = FLASH_DRIVER_I2C;
	}

	/* Read the flash and torch source info from device tree node */
	rc = msm_flash_get_pmic_source_info(of_node, fctrl);
	if (rc < 0) {
		pr_err("%s:%d msm_flash_get_pmic_source_info failed rc %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	/* Read the gpio information from device tree */
	rc = msm_sensor_driver_get_gpio_data(
		&(fctrl->power_info.gpio_conf), of_node);
	if (rc < 0) {
		pr_err("%s:%d msm_sensor_driver_get_gpio_data failed rc %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	if (fctrl->flash_driver_type == FLASH_DRIVER_DEFAULT)
		fctrl->flash_driver_type = FLASH_DRIVER_GPIO;
	CDBG("%s:%d fctrl->flash_driver_type = %d", __func__, __LINE__,
		fctrl->flash_driver_type);

	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_flash_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	int32_t i = 0;
	int32_t rc = 0;
	struct video_device *vdev;
	struct v4l2_subdev *sd;
	struct msm_flash_cfg_data_t32 *u32;
	struct msm_flash_cfg_data_t flash_data;
	struct msm_flash_init_info_t32 flash_init_info32;
	struct msm_flash_init_info_t flash_init_info;

	CDBG("Enter");

	if (!file || !arg) {
		pr_err("%s:failed NULL parameter\n", __func__);
		return -EINVAL;
	}
	vdev = video_devdata(file);
	sd = vdev_to_v4l2_subdev(vdev);
	u32 = (struct msm_flash_cfg_data_t32 *)arg;

	switch (cmd) {
	case VIDIOC_MSM_FLASH_CFG32:
		flash_data.cfg_type = u32->cfg_type;
		for (i = 0; i < MAX_LED_TRIGGERS; i++) {
			flash_data.flash_current[i] = u32->flash_current[i];
			flash_data.flash_duration[i] = u32->flash_duration[i];
		}
		cmd = VIDIOC_MSM_FLASH_CFG;
		switch (flash_data.cfg_type) {
		case CFG_FLASH_OFF:
		case CFG_FLASH_LOW:
		case CFG_FLASH_HIGH:
			flash_data.cfg.settings = compat_ptr(u32->cfg.settings);
			break;
		case CFG_FLASH_INIT:
			flash_data.cfg.flash_init_info = &flash_init_info;
			if (copy_from_user(&flash_init_info32,
				(void __user *)
				compat_ptr(u32->cfg.flash_init_info),
				sizeof(struct msm_flash_init_info_t32))) {
				pr_err("%s copy_from_user failed %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
			flash_init_info.flash_driver_type =
				flash_init_info32.flash_driver_type;
			flash_init_info.slave_addr =
				flash_init_info32.slave_addr;
			flash_init_info.i2c_freq_mode =
				flash_init_info32.i2c_freq_mode;
			flash_init_info.settings =
				compat_ptr(flash_init_info32.settings);
			flash_init_info.power_setting_array =
				compat_ptr(
				flash_init_info32.power_setting_array);
			break;
		default:
			break;
		}
		break;
	case VIDIOC_MSM_FLASH_CFG:
		pr_err("invalid cmd 0x%x received\n", cmd);
		return -EINVAL;
	default:
		return msm_flash_subdev_ioctl(sd, cmd, arg);
	}

	rc =  msm_flash_subdev_ioctl(sd, cmd, &flash_data);
	for (i = 0; i < MAX_LED_TRIGGERS; i++) {
		u32->flash_current[i] = flash_data.flash_current[i];
		u32->flash_duration[i] = flash_data.flash_duration[i];
	}
	CDBG("Exit");
	return rc;
}

static long msm_flash_subdev_fops_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_flash_subdev_do_ioctl);
}
#endif
static int32_t msm_flash_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_flash_ctrl_t *flash_ctrl = NULL;
	struct msm_camera_cci_client *cci_client = NULL;

	CDBG("Enter");
	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	flash_ctrl = kzalloc(sizeof(struct msm_flash_ctrl_t), GFP_KERNEL);
	if (!flash_ctrl)
		return -ENOMEM;

	memset(flash_ctrl, 0, sizeof(struct msm_flash_ctrl_t));

	flash_ctrl->pdev = pdev;

	rc = msm_flash_get_dt_data(pdev->dev.of_node, flash_ctrl);
	if (rc < 0) {
		pr_err("%s:%d msm_flash_get_dt_data failed\n",
			__func__, __LINE__);
		kfree(flash_ctrl);
		return -EINVAL;
	}

	flash_ctrl->flash_state = MSM_CAMERA_FLASH_RELEASE;
	flash_ctrl->power_info.dev = &flash_ctrl->pdev->dev;
	flash_ctrl->flash_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	flash_ctrl->flash_mutex = &msm_flash_mutex;
	flash_ctrl->flash_i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	flash_ctrl->flash_i2c_client.cci_client = kzalloc(
		sizeof(struct msm_camera_cci_client), GFP_KERNEL);
	if (!flash_ctrl->flash_i2c_client.cci_client) {
		kfree(flash_ctrl);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	cci_client = flash_ctrl->flash_i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = flash_ctrl->cci_i2c_master;

	/* Initialize sub device */
	v4l2_subdev_init(&flash_ctrl->msm_sd.sd, &msm_flash_subdev_ops);
	v4l2_set_subdevdata(&flash_ctrl->msm_sd.sd, flash_ctrl);

	flash_ctrl->msm_sd.sd.internal_ops = &msm_flash_internal_ops;
	flash_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(flash_ctrl->msm_sd.sd.name,
		ARRAY_SIZE(flash_ctrl->msm_sd.sd.name),
		"msm_camera_flash");
	media_entity_pads_init(&flash_ctrl->msm_sd.sd.entity, 0, NULL);
	flash_ctrl->msm_sd.sd.entity.function = MSM_CAMERA_SUBDEV_FLASH;
	flash_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x1;
	msm_sd_register(&flash_ctrl->msm_sd);

	CDBG("%s:%d flash sd name = %s", __func__, __LINE__,
		flash_ctrl->msm_sd.sd.entity.name);
	msm_cam_copy_v4l2_subdev_fops(&msm_flash_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_flash_v4l2_subdev_fops.compat_ioctl32 =
		msm_flash_subdev_fops_ioctl;
#endif
	flash_ctrl->msm_sd.sd.devnode->fops = &msm_flash_v4l2_subdev_fops;

	if (flash_ctrl->flash_driver_type == FLASH_DRIVER_PMIC)
		rc = msm_torch_create_classdev(pdev, flash_ctrl);

	CDBG("probe success\n");
	return rc;
}

MODULE_DEVICE_TABLE(of, msm_flash_dt_match);

static struct platform_driver msm_flash_platform_driver = {
	.probe = msm_flash_platform_probe,
	.driver = {
		.name = "qcom,camera-flash",
		.owner = THIS_MODULE,
		.of_match_table = msm_flash_dt_match,
	},
};

static int __init msm_flash_init_module(void)
{
	int32_t rc = 0;

	CDBG("Enter\n");
	rc = platform_driver_register(&msm_flash_platform_driver);
	if (rc)
		pr_err("platform probe for flash failed");

	return rc;
}

static void __exit msm_flash_exit_module(void)
{
	platform_driver_unregister(&msm_flash_platform_driver);
}

static struct msm_flash_table msm_pmic_flash_table = {
	.flash_driver_type = FLASH_DRIVER_PMIC,
	.func_tbl = {
		.camera_flash_init = NULL,
		.camera_flash_release = msm_flash_release,
		.camera_flash_off = msm_flash_off,
		.camera_flash_low = msm_flash_low,
		.camera_flash_high = msm_flash_high,
		.camera_flash_query_current = msm_flash_query_current,
	},
};

static struct msm_flash_table msm_gpio_flash_table = {
	.flash_driver_type = FLASH_DRIVER_GPIO,
	.func_tbl = {
		.camera_flash_init = msm_flash_gpio_init,
		.camera_flash_release = msm_flash_release,
		.camera_flash_off = msm_flash_off,
		.camera_flash_low = msm_flash_low,
		.camera_flash_high = msm_flash_high,
		.camera_flash_query_current = NULL,
	},
};

static struct msm_flash_table msm_i2c_flash_table = {
	.flash_driver_type = FLASH_DRIVER_I2C,
	.func_tbl = {
		.camera_flash_init = msm_flash_i2c_init,
		.camera_flash_release = msm_flash_i2c_release,
		.camera_flash_off = msm_flash_i2c_write_setting_array,
		.camera_flash_low = msm_flash_i2c_write_setting_array,
		.camera_flash_high = msm_flash_i2c_write_setting_array,
		.camera_flash_query_current = NULL,
	},
};

module_init(msm_flash_init_module);
module_exit(msm_flash_exit_module);
MODULE_DESCRIPTION("MSM FLASH");
MODULE_LICENSE("GPL v2");
