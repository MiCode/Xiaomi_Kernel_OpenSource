// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include "cam_flash_dev.h"
#include "cam_flash_soc.h"
#include "cam_flash_core.h"
#include "cam_common_util.h"
#include "camera_main.h"

#ifdef CONFIG_CAMERA_I2CFLASH
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/gpio.h>
#endif

#ifdef CONFIG_CAMERA_I2CFLASH
#define I2C_FLASH_NUM 4
#define I2C_FLASH_SLOT_INDEX 7
#define I2C_FLASH_AUX_SLOT_INDEX 6
#define I2C_FLASH_ADDR 0xc6
#define I2C_FLASH_MAX_TORCH_CURRENT 1500
#define I2C_FLASH_MIN_TORCH_CURRENT 0
#define I2C_FLASH_GPIO_EN 356
#define I2C_FLASH_LED1_REG_ADDR 0x05
#define I2C_FLASH_LED2_REG_ADDR 0x06

static struct kobject *i2c_flash_kobj;
static int i2c_flash_brightness[I2C_FLASH_NUM];
static int i2c_flash_switch;
static int i2c_flash_status;

struct cam_flash_ctrl *fctrl_flash;
struct cam_flash_ctrl *fctrl_flash_aux;

static int cam_flash_i2c_driver_powerup()
{
	int ret;

	ret = gpio_request_one(I2C_FLASH_GPIO_EN, 0, "flash_en");
	if (ret < 0) {
		CAM_ERR(CAM_FLASH, "i2c flash kobj gpio_request_one failed ");
		return ret;
	}
	gpio_direction_output(I2C_FLASH_GPIO_EN,1);
	usleep_range(1000, 2000);

	ret = camera_io_init(&(fctrl_flash->io_master_info));
	if (ret) {
		CAM_ERR(CAM_FLASH, "i2c flash kobj fctrl_flash cci_init failed: rc: %d", ret);
		goto release_gpio;
	}
	ret = camera_io_init(&(fctrl_flash_aux->io_master_info));
	if (ret) {
		CAM_ERR(CAM_FLASH, "i2c flash kobj fctrl_flash_aux cci_init failed: rc: %d", ret);
		goto release_i2c;
	}

	CAM_INFO(CAM_FLASH, "i2c flash kobj powerup %d", ret);

	usleep_range(1000, 2000);
	return ret;

release_i2c:
	camera_io_release(&(fctrl_flash->io_master_info));
release_gpio:
	gpio_direction_output(I2C_FLASH_GPIO_EN,0);
	gpio_free(I2C_FLASH_GPIO_EN);
	return ret;
}

static int cam_flash_i2c_driver_powerdown()
{
	int ret = 0;

	camera_io_release(&(fctrl_flash->io_master_info));
	camera_io_release(&(fctrl_flash_aux->io_master_info));
	usleep_range(1000, 2000);
	gpio_direction_output(I2C_FLASH_GPIO_EN,0);
	gpio_free(I2C_FLASH_GPIO_EN);
	usleep_range(1000, 2000);

	CAM_INFO(CAM_FLASH, "i2c flash kobj powerdown %d", ret);

	return ret;
}

static int cam_flash_i2c_driver_set_torch(int* led_brightness, int led_switch)
{
	int ret;

	struct cam_sensor_i2c_reg_array i2c_flash_reg_setting[3] = {
		{0x05,0x23,0,0},
		{0x06,0x23,0,0},
		{0x01,0x00,0,0},
	};
	struct cam_sensor_i2c_reg_setting i2c_flash_settings = {
		.reg_setting = i2c_flash_reg_setting,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.size = 3,
		.delay = 0,
	};

	i2c_flash_reg_setting[0].reg_data = led_brightness[0];
	i2c_flash_reg_setting[1].reg_data = led_brightness[1];
	i2c_flash_reg_setting[2].reg_data = (led_switch & 0x03) | 0x08;
	ret = camera_io_dev_write((&(fctrl_flash->io_master_info)),
		&(i2c_flash_settings));
	if (ret < 0) {
		CAM_ERR(CAM_FLASH, "i2c flash kobj Failed to random write I2C settings: %d", ret);
		return ret;
	}

	i2c_flash_reg_setting[0].reg_data = led_brightness[2];
	i2c_flash_reg_setting[1].reg_data = led_brightness[3];
	i2c_flash_reg_setting[2].reg_data = ((led_switch & 0x0C) >> 2) | 0x08;
	ret = camera_io_dev_write((&(fctrl_flash_aux->io_master_info)),
		&(i2c_flash_settings));
	if (ret < 0) {
		CAM_ERR(CAM_FLASH, "i2c flash kobj Failed to random write I2C settings: %d", ret);
		return ret;
	}

	CAM_INFO(CAM_FLASH, "i2c flash kobj set_torch %d %d", led_brightness, led_switch);

	usleep_range(1000, 2000);
	return ret;
}


static ssize_t led_brightness_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d:%d:%d:%d\n", i2c_flash_brightness[0], i2c_flash_brightness[1],
		i2c_flash_brightness[2], i2c_flash_brightness[3]);
}

static ssize_t led_brightness_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int ret;
	int i;
	int brightness[I2C_FLASH_NUM];

	CAM_INFO(CAM_FLASH, "i2c flash kobj echo buf %s", buf);
	ret = sscanf(buf, "%d:%d:%d:%d", &brightness[0], &brightness[1],
		&brightness[2], &brightness[3]);
	CAM_INFO(CAM_FLASH, "i2c flash kobj sscanf ret %d", ret);

	if (ret == I2C_FLASH_NUM) {
		i2c_flash_brightness[0] = brightness[0];
		i2c_flash_brightness[1] = brightness[1];
		i2c_flash_brightness[2] = brightness[2];
		i2c_flash_brightness[3] = brightness[3];
	}

	for (i = 0;i < I2C_FLASH_NUM; i++) {
		if (I2C_FLASH_MAX_TORCH_CURRENT < i2c_flash_brightness[i]) {
			i2c_flash_brightness[i] = I2C_FLASH_MAX_TORCH_CURRENT;
		}
		if (I2C_FLASH_MIN_TORCH_CURRENT > i2c_flash_brightness[i]) {
			i2c_flash_brightness[i] = I2C_FLASH_MIN_TORCH_CURRENT;
		}
		CAM_INFO(CAM_FLASH, "i2c flash kobj set brightness[%d] to %d", i, i2c_flash_brightness[i]);
	}

	return count;
}

static struct kobj_attribute led_brightness_attribute =
	__ATTR(led_brightness, 0664, led_brightness_show, led_brightness_store);

static ssize_t led_switch_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%d\n", i2c_flash_switch);
}

static ssize_t led_switch_store(struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 10, &i2c_flash_switch);
	if (ret < 0) {
		return ret;
	}

	CAM_INFO(CAM_FLASH, "i2c flash kobj set switch to %d", i2c_flash_switch);

	if ((NULL != fctrl_flash) && (NULL != fctrl_flash_aux)) {

		fctrl_flash->io_master_info.client->addr = I2C_FLASH_ADDR;
		fctrl_flash_aux->io_master_info.client->addr = I2C_FLASH_ADDR;


		if ((0x01 <= i2c_flash_switch) && ( 0x0F >= i2c_flash_switch) && (0 == i2c_flash_status)) {

			ret = cam_flash_i2c_driver_powerup();
			if (ret < 0) {
				CAM_ERR(CAM_FLASH, "i2c flash kobj Failed to powerup: %d", ret);
				return ret;
			}

			ret = cam_flash_i2c_driver_set_torch(i2c_flash_brightness, i2c_flash_switch);
			if (ret < 0) {
				CAM_ERR(CAM_FLASH, "i2c flash kobj Failed to set_torch: %d", ret);
				return ret;
			}

			i2c_flash_status = 1;
		}else if ((0 == i2c_flash_switch) && (1 == i2c_flash_status)) {

			ret = cam_flash_i2c_driver_set_torch(i2c_flash_brightness, i2c_flash_switch);
			if (ret < 0) {
				CAM_ERR(CAM_FLASH, "i2c flash kobj Failed to set_torch: %d", ret);
				return ret;
			}

			ret = cam_flash_i2c_driver_powerdown();
			if (ret < 0) {
				CAM_ERR(CAM_FLASH, "i2c flash kobj Failed to powerdown: %d", ret);
				return ret;
			}

			i2c_flash_status = 0;
		}else {
			CAM_ERR(CAM_FLASH, "i2c flash kobj error cmd i2c_flash_switch %d ", i2c_flash_switch);
		}

	}

	return count;
}

static struct kobj_attribute led_switch_attribute =
	__ATTR(led_switch, 0664, led_switch_show, led_switch_store);

static struct attribute *attrs[] = {
	&led_brightness_attribute.attr,
	&led_switch_attribute.attr,
	NULL,   /* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int cam_flash_i2c_driver_init_kobject(struct cam_flash_ctrl *fctrl)
{
	int retval = 0;

	if (NULL != fctrl) {
		CAM_INFO(CAM_FLASH, "i2c flash kobj init flash:%s index %d i2c_flash_kobj %p",
			fctrl->io_master_info.client->name,
			fctrl->soc_info.index,
			i2c_flash_kobj);

		if (I2C_FLASH_SLOT_INDEX == fctrl->soc_info.index) {
			fctrl_flash = fctrl;
		}else if (I2C_FLASH_AUX_SLOT_INDEX == fctrl->soc_info.index) {
			fctrl_flash_aux = fctrl;
		}
	}

	if (NULL != i2c_flash_kobj) {
		return retval;
	}

	i2c_flash_kobj = kobject_create_and_add(FLASH_DRIVER_I2C, kernel_kobj);
	if (!i2c_flash_kobj) {
		return -ENOMEM;
	}

	retval = sysfs_create_group(i2c_flash_kobj, &attr_group);
	if (retval) {
	   kobject_put(i2c_flash_kobj);
	}

	CAM_INFO(CAM_FLASH, "i2c flash kobj init rc %d  i2c_flash_kobj %p",retval, i2c_flash_kobj);

	return retval;
}

static void cam_flash_i2c_driver_deinit_kobject()
{
	CAM_INFO(CAM_FLASH, "i2c flash kobj deinit i2c_flash_kobj %p",i2c_flash_kobj);
	if (NULL != i2c_flash_kobj) {
		kobject_put(i2c_flash_kobj);
	}
}
void update_i2c_flash_brightness(struct i2c_settings_list *i2c_list)
{
	int i = 0;
	for (i = 0; i < i2c_list->i2c_settings.size; i++) {
		if (I2C_FLASH_LED1_REG_ADDR == i2c_list->i2c_settings.reg_setting[i].reg_addr) {
			i2c_flash_brightness[0] = i2c_list->i2c_settings.reg_setting[i].reg_data;
			i2c_flash_brightness[2] = i2c_list->i2c_settings.reg_setting[i].reg_data;
		}
		else if (I2C_FLASH_LED2_REG_ADDR == i2c_list->i2c_settings.reg_setting[i].reg_addr) {
			i2c_flash_brightness[1] = i2c_list->i2c_settings.reg_setting[i].reg_data;
			i2c_flash_brightness[3] = i2c_list->i2c_settings.reg_setting[i].reg_data;
		}
	}
	CAM_INFO(CAM_FLASH, "%d:%d:%d:%d\n", i2c_flash_brightness[0], i2c_flash_brightness[1],
		i2c_flash_brightness[2], i2c_flash_brightness[3]);
}
#endif

static int32_t cam_flash_driver_cmd(struct cam_flash_ctrl *fctrl,
		void *arg, struct cam_flash_private_soc *soc_private)
{
	int rc = 0;
	int i = 0;
	struct cam_control *cmd = (struct cam_control *)arg;

	if (!fctrl || !arg) {
		CAM_ERR(CAM_FLASH, "fctrl/arg is NULL with arg:%pK fctrl%pK",
			fctrl, arg);
		return -EINVAL;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_FLASH, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	mutex_lock(&(fctrl->flash_mutex));
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev flash_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if (fctrl->flash_state != CAM_FLASH_STATE_INIT) {
			CAM_ERR(CAM_FLASH,
				"Cannot apply Acquire dev: Prev state: %d",
				fctrl->flash_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		if (fctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_FLASH, "Device is already acquired");
			rc = -EINVAL;
			goto release_mutex;
		}

		rc = copy_from_user(&flash_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(flash_acq_dev));
		if (rc) {
			CAM_ERR(CAM_FLASH, "Failed Copying from User");
			goto release_mutex;
		}

#if IS_ENABLED(CONFIG_ISPV3)
		if (flash_acq_dev.reserved)
			fctrl->trigger_source = CAM_REQ_MGR_TRIG_SRC_EXTERNAL;
		else
			fctrl->trigger_source = CAM_REQ_MGR_TRIG_SRC_INTERNAL;
#endif

		bridge_params.session_hdl = flash_acq_dev.session_handle;
		bridge_params.ops = &fctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = fctrl;
		bridge_params.dev_id = CAM_FLASH;

		flash_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		if (flash_acq_dev.device_handle <= 0) {
			rc = -EFAULT;
			CAM_ERR(CAM_FLASH, "Can not create device handle");
			goto release_mutex;
		}
		fctrl->bridge_intf.device_hdl =
			flash_acq_dev.device_handle;
		fctrl->bridge_intf.session_hdl =
			flash_acq_dev.session_handle;
		fctrl->apply_streamoff = false;

#if IS_ENABLED(CONFIG_ISPV3)
		CAM_DBG(CAM_FLASH, "Device Handle: %d trigger_source: %s",
			flash_acq_dev.device_handle,
			(fctrl->trigger_source == CAM_REQ_MGR_TRIG_SRC_INTERNAL) ?
			"internal" : "external");
#endif

		rc = copy_to_user(u64_to_user_ptr(cmd->handle),
			&flash_acq_dev,
			sizeof(struct cam_sensor_acquire_dev));
		if (rc) {
			CAM_ERR(CAM_FLASH, "Failed Copy to User with rc = %d",
				rc);
			rc = -EFAULT;
			goto release_mutex;
		}
		fctrl->flash_state = CAM_FLASH_STATE_ACQUIRE;

		CAM_INFO(CAM_FLASH, "CAM_ACQUIRE_DEV for dev_hdl: 0x%x",
			fctrl->bridge_intf.device_hdl);
		break;
	}
	case CAM_RELEASE_DEV: {
		CAM_INFO(CAM_FLASH, "CAM_RELEASE_DEV for dev_hdl: 0x%x",
			fctrl->bridge_intf.device_hdl);
		if ((fctrl->flash_state == CAM_FLASH_STATE_INIT) ||
			(fctrl->flash_state == CAM_FLASH_STATE_START)) {
			CAM_WARN(CAM_FLASH,
				"Wrong state for Release dev: Prev state:%d",
				fctrl->flash_state);
		}

		if (fctrl->bridge_intf.device_hdl == -1 &&
			fctrl->flash_state == CAM_FLASH_STATE_ACQUIRE) {
			CAM_ERR(CAM_FLASH,
				"Invalid Handle: Link Hdl: %d device hdl: %d",
				fctrl->bridge_intf.device_hdl,
				fctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}

		if (fctrl->bridge_intf.link_hdl != -1) {
			CAM_ERR(CAM_FLASH,
				"Device [%d] still active on link 0x%x",
				fctrl->flash_state,
				fctrl->bridge_intf.link_hdl);
			rc = -EAGAIN;
			goto release_mutex;
		}

		if ((fctrl->flash_state == CAM_FLASH_STATE_CONFIG) ||
			(fctrl->flash_state == CAM_FLASH_STATE_START))
			fctrl->func_tbl.flush_req(fctrl, FLUSH_ALL, 0);

		if (cam_flash_release_dev(fctrl))
			CAM_WARN(CAM_FLASH,
				"Failed in destroying the device Handle");

		if (fctrl->func_tbl.power_ops) {
			if (fctrl->func_tbl.power_ops(fctrl, false))
				CAM_WARN(CAM_FLASH, "Power Down Failed");
		}

		fctrl->streamoff_count = 0;
		fctrl->flash_state = CAM_FLASH_STATE_INIT;
		break;
	}
	case CAM_QUERY_CAP: {
		struct cam_flash_query_cap_info flash_cap = {0};

		CAM_DBG(CAM_FLASH, "CAM_QUERY_CAP");
		flash_cap.slot_info = fctrl->soc_info.index;
		for (i = 0; i < fctrl->flash_num_sources; i++) {
			flash_cap.max_current_flash[i] =
				soc_private->flash_max_current[i];
			flash_cap.max_duration_flash[i] =
				soc_private->flash_max_duration[i];
		}

		for (i = 0; i < fctrl->torch_num_sources; i++)
			flash_cap.max_current_torch[i] =
				soc_private->torch_max_current[i];

		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&flash_cap, sizeof(struct cam_flash_query_cap_info))) {
			CAM_ERR(CAM_FLASH, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	}
	case CAM_START_DEV: {
		CAM_INFO(CAM_FLASH, "CAM_START_DEV for dev_hdl: 0x%x",
			fctrl->bridge_intf.device_hdl);
		if ((fctrl->flash_state == CAM_FLASH_STATE_INIT) ||
			(fctrl->flash_state == CAM_FLASH_STATE_START)) {
			CAM_WARN(CAM_FLASH,
				"Cannot apply Start Dev: Prev state: %d",
				fctrl->flash_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		fctrl->apply_streamoff = false;
		fctrl->flash_state = CAM_FLASH_STATE_START;
		break;
	}
	case CAM_STOP_DEV: {
		CAM_INFO(CAM_FLASH, "CAM_STOP_DEV ENTER for dev_hdl: 0x%x",
			fctrl->bridge_intf.device_hdl);
		if (fctrl->flash_state != CAM_FLASH_STATE_START) {
			CAM_WARN(CAM_FLASH,
				"Cannot apply Stop dev: Prev state is: %d",
				fctrl->flash_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		fctrl->func_tbl.flush_req(fctrl, FLUSH_ALL, 0);
		fctrl->last_flush_req = 0;
		cam_flash_off(fctrl);
		fctrl->flash_state = CAM_FLASH_STATE_ACQUIRE;
		break;
	}
	case CAM_CONFIG_DEV: {
		CAM_DBG(CAM_FLASH, "CAM_CONFIG_DEV");
		rc = fctrl->func_tbl.parser(fctrl, arg);
		if (rc) {
			CAM_ERR(CAM_FLASH, "Failed Flash Config: rc=%d\n", rc);
			goto release_mutex;
		}
		break;
	}
	default:
		CAM_ERR(CAM_FLASH, "Invalid Opcode: %d", cmd->op_code);
		rc = -EINVAL;
	}

release_mutex:
	mutex_unlock(&(fctrl->flash_mutex));
	return rc;
}

static int32_t cam_flash_init_default_params(struct cam_flash_ctrl *fctrl)
{
	/* Validate input parameters */
	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "failed: invalid params fctrl %pK",
			fctrl);
		return -EINVAL;
	}

	CAM_DBG(CAM_FLASH,
		"master_type: %d", fctrl->io_master_info.master_type);
	/* Initialize cci_client */
	if (fctrl->io_master_info.master_type == CCI_MASTER) {
		fctrl->io_master_info.cci_client = kzalloc(sizeof(
			struct cam_sensor_cci_client), GFP_KERNEL);
		if (!(fctrl->io_master_info.cci_client))
			return -ENOMEM;
	} else if (fctrl->io_master_info.master_type == I2C_MASTER) {
		if (!(fctrl->io_master_info.client))
			return -EINVAL;
	} else {
		CAM_ERR(CAM_FLASH,
			"Invalid master / Master type Not supported");
		return -EINVAL;
	}

	return 0;
}

static const struct of_device_id cam_flash_dt_match[] = {
	{.compatible = "qcom,camera-flash", .data = NULL},
	{}
};

static int cam_flash_subdev_close_internal(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_flash_ctrl *fctrl =
		v4l2_get_subdevdata(sd);

	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "Flash ctrl ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&fctrl->flash_mutex);
	cam_flash_shutdown(fctrl);
	mutex_unlock(&fctrl->flash_mutex);

	return 0;
}

static int cam_flash_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	bool crm_active = cam_req_mgr_is_open();

	if (crm_active) {
		CAM_DBG(CAM_FLASH, "CRM is ACTIVE, close should be from CRM");
		return 0;
	}

	return cam_flash_subdev_close_internal(sd, fh);
}

static long cam_flash_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	struct cam_flash_ctrl *fctrl = NULL;
	struct cam_flash_private_soc *soc_private = NULL;

	CAM_DBG(CAM_FLASH, "Enter");

	fctrl = v4l2_get_subdevdata(sd);
	soc_private = fctrl->soc_info.soc_private;

	switch (cmd) {
	case VIDIOC_CAM_CONTROL: {
		rc = cam_flash_driver_cmd(fctrl, arg,
			soc_private);
		if (rc)
			CAM_ERR(CAM_FLASH,
				"Failed in driver cmd: %d", rc);
		break;
	}
	case CAM_SD_SHUTDOWN:
		if (!cam_req_mgr_is_shutdown()) {
			CAM_ERR(CAM_CORE, "SD shouldn't come from user space");
			return 0;
		}

		rc = cam_flash_subdev_close_internal(sd, NULL);
		break;
	default:
		CAM_ERR(CAM_FLASH, "Invalid ioctl cmd type");
		rc = -ENOIOCTLCMD;
		break;
	}

	CAM_DBG(CAM_FLASH, "Exit");
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_flash_subdev_do_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_FLASH,
			"Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL: {
		rc = cam_flash_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc)
			CAM_ERR(CAM_FLASH, "cam_flash_ioctl failed");
		break;
	}
	default:
		CAM_ERR(CAM_FLASH, "Invalid compat ioctl cmd_type:%d",
			cmd);
		rc = -ENOIOCTLCMD;
		break;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_FLASH,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
}
#endif

static struct v4l2_subdev_core_ops cam_flash_subdev_core_ops = {
	.ioctl = cam_flash_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_flash_subdev_do_ioctl
#endif
};

static struct v4l2_subdev_ops cam_flash_subdev_ops = {
	.core = &cam_flash_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cam_flash_internal_ops = {
	.close = cam_flash_subdev_close,
};

static int cam_flash_init_subdev(struct cam_flash_ctrl *fctrl)
{
	int rc = 0;

	strlcpy(fctrl->device_name, CAM_FLASH_NAME,
		sizeof(fctrl->device_name));
	fctrl->v4l2_dev_str.internal_ops =
		&cam_flash_internal_ops;
	fctrl->v4l2_dev_str.ops = &cam_flash_subdev_ops;
	fctrl->v4l2_dev_str.name = CAMX_FLASH_DEV_NAME;
	fctrl->v4l2_dev_str.sd_flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	fctrl->v4l2_dev_str.ent_function = CAM_FLASH_DEVICE_TYPE;
	fctrl->v4l2_dev_str.token = fctrl;
	fctrl->v4l2_dev_str.close_seq_prior = CAM_SD_CLOSE_MEDIUM_PRIORITY;

	rc = cam_register_subdev(&(fctrl->v4l2_dev_str));
	if (rc)
		CAM_ERR(CAM_FLASH, "Fail to create subdev with %d", rc);

	return rc;
}

static int cam_flash_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int32_t rc = 0, i = 0;
	struct cam_flash_ctrl *fctrl = NULL;
	struct device_node *of_parent = NULL;
	struct platform_device *pdev = to_platform_device(dev);

	CAM_DBG(CAM_FLASH, "Binding flash component");
	if (!pdev->dev.of_node) {
		CAM_ERR(CAM_FLASH, "of_node NULL");
		return -EINVAL;
	}

	fctrl = kzalloc(sizeof(struct cam_flash_ctrl), GFP_KERNEL);
	if (!fctrl)
		return -ENOMEM;

	fctrl->pdev = pdev;
	fctrl->of_node = pdev->dev.of_node;
	fctrl->soc_info.pdev = pdev;
	fctrl->soc_info.dev = &pdev->dev;
	fctrl->soc_info.dev_name = pdev->name;

	platform_set_drvdata(pdev, fctrl);

	rc = cam_flash_get_dt_data(fctrl, &fctrl->soc_info);
	if (rc) {
		CAM_ERR(CAM_FLASH, "cam_flash_get_dt_data failed with %d", rc);
		kfree(fctrl);
		return -EINVAL;
	}

	if (of_find_property(pdev->dev.of_node, "cci-master", NULL)) {
		/* Get CCI master */
		rc = of_property_read_u32(pdev->dev.of_node, "cci-master",
			&fctrl->cci_i2c_master);
		CAM_DBG(CAM_FLASH, "cci-master %d, rc %d",
			fctrl->cci_i2c_master, rc);
		if (rc < 0) {
			/* Set default master 0 */
			fctrl->cci_i2c_master = MASTER_0;
			rc = 0;
		}

		fctrl->io_master_info.master_type = CCI_MASTER;
		rc = cam_flash_init_default_params(fctrl);
		if (rc) {
			CAM_ERR(CAM_FLASH,
				"failed: cam_flash_init_default_params rc %d",
				rc);
			return rc;
		}

		of_parent = of_get_parent(pdev->dev.of_node);
		if (of_property_read_u32(of_parent, "cell-index",
				&fctrl->cci_num) < 0)
		/* Set default master 0 */
			fctrl->cci_num = CCI_DEVICE_0;

		fctrl->io_master_info.cci_client->cci_device = fctrl->cci_num;
		CAM_DBG(CAM_FLASH, "cci-index %d", fctrl->cci_num, rc);

		fctrl->i2c_data.per_frame =
			kzalloc(sizeof(struct i2c_settings_array) *
			MAX_PER_FRAME_ARRAY, GFP_KERNEL);
		if (fctrl->i2c_data.per_frame == NULL) {
			CAM_ERR(CAM_FLASH, "No Memory");
			rc = -ENOMEM;
			goto free_cci_resource;
		}

		INIT_LIST_HEAD(&(fctrl->i2c_data.init_settings.list_head));
		INIT_LIST_HEAD(&(fctrl->i2c_data.config_settings.list_head));
		INIT_LIST_HEAD(&(fctrl->i2c_data.streamoff_settings.list_head));
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
			INIT_LIST_HEAD(
				&(fctrl->i2c_data.per_frame[i].list_head));

		fctrl->func_tbl.parser = cam_flash_i2c_pkt_parser;
		fctrl->func_tbl.apply_setting = cam_flash_i2c_apply_setting;
		fctrl->func_tbl.power_ops = cam_flash_i2c_power_ops;
		fctrl->func_tbl.flush_req = cam_flash_i2c_flush_request;
	} else {
		/* PMIC Flash */
		fctrl->func_tbl.parser = cam_flash_pmic_pkt_parser;
		fctrl->func_tbl.apply_setting = cam_flash_pmic_apply_setting;
		fctrl->func_tbl.power_ops = NULL;
		fctrl->func_tbl.flush_req = cam_flash_pmic_flush_request;
	}

	rc = cam_flash_init_subdev(fctrl);
	if (rc) {
		if (fctrl->io_master_info.cci_client != NULL)
			goto free_cci_resource;
		else
			goto free_resource;
	}

	fctrl->bridge_intf.device_hdl = -1;
	fctrl->bridge_intf.link_hdl = -1;
	fctrl->bridge_intf.ops.get_dev_info = cam_flash_publish_dev_info;
	fctrl->bridge_intf.ops.link_setup = cam_flash_establish_link;
	fctrl->bridge_intf.ops.apply_req = cam_flash_apply_request;
	fctrl->bridge_intf.ops.flush_req = cam_flash_flush_request;
	fctrl->last_flush_req = 0;

	mutex_init(&(fctrl->flash_mutex));

	fctrl->flash_state = CAM_FLASH_STATE_INIT;
	CAM_DBG(CAM_FLASH, "Component bound successfully");
	return rc;

free_cci_resource:
	kfree(fctrl->io_master_info.cci_client);
	fctrl->io_master_info.cci_client = NULL;
free_resource:
	kfree(fctrl->i2c_data.per_frame);
	kfree(fctrl->soc_info.soc_private);
	cam_soc_util_release_platform_resource(&fctrl->soc_info);
	fctrl->i2c_data.per_frame = NULL;
	fctrl->soc_info.soc_private = NULL;
	kfree(fctrl);
	fctrl = NULL;
	return rc;
}

static void cam_flash_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_flash_ctrl *fctrl;
	struct platform_device *pdev = to_platform_device(dev);

	fctrl = platform_get_drvdata(pdev);
	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "Flash device is NULL");
		return;
	}

	mutex_lock(&fctrl->flash_mutex);
	cam_flash_shutdown(fctrl);
	mutex_unlock(&fctrl->flash_mutex);
	cam_unregister_subdev(&(fctrl->v4l2_dev_str));
	cam_flash_put_source_node_data(fctrl);
	platform_set_drvdata(pdev, NULL);
	v4l2_set_subdevdata(&fctrl->v4l2_dev_str.sd, NULL);
	kfree(fctrl);
	CAM_INFO(CAM_FLASH, "Flash Sensor component unbind");
}

const static struct component_ops cam_flash_component_ops = {
	.bind = cam_flash_component_bind,
	.unbind = cam_flash_component_unbind,
};

static int cam_flash_platform_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_flash_component_ops);
	return 0;
}

static int32_t cam_flash_platform_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_FLASH, "Adding Flash Sensor component");
	rc = component_add(&pdev->dev, &cam_flash_component_ops);
	if (rc)
		CAM_ERR(CAM_FLASH, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_flash_i2c_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int32_t rc = 0, i = 0;
	struct i2c_client      *client = NULL;
	struct cam_flash_ctrl  *fctrl = NULL;
	struct cam_hw_soc_info *soc_info = NULL;

	client = container_of(dev, struct i2c_client, dev);
	CAM_INFO(CAM_FLASH, "cam_flash_i2c_driver_probe E rc: %d", rc);

	if (client == NULL) {
		CAM_ERR(CAM_FLASH, "Invalid Args client: %pK",
			client);
		return -EINVAL;
	}

	/* Create sensor control structure */
	fctrl = kzalloc(sizeof(*fctrl), GFP_KERNEL);
	if (!fctrl)
		return -ENOMEM;

	client->dev.driver_data = fctrl;
	fctrl->io_master_info.client = client;
	fctrl->of_node = client->dev.of_node;
	fctrl->soc_info.dev = &client->dev;
	fctrl->soc_info.dev_name = client->name;
	fctrl->io_master_info.master_type = I2C_MASTER;

	rc = cam_flash_get_dt_data(fctrl, &fctrl->soc_info);
	if (rc) {
		CAM_ERR(CAM_FLASH, "failed: cam_sensor_parse_dt rc %d", rc);
		goto free_ctrl;
	}

	rc = cam_flash_init_default_params(fctrl);
	if (rc) {
		CAM_ERR(CAM_FLASH,
				"failed: cam_flash_init_default_params rc %d",
				rc);
		goto free_ctrl;
	}

	soc_info = &fctrl->soc_info;

	/* Initalize regulators to default parameters */
	for (i = 0; i < soc_info->num_rgltr; i++) {
		soc_info->rgltr[i] = devm_regulator_get(soc_info->dev,
			soc_info->rgltr_name[i]);
		if (IS_ERR_OR_NULL(soc_info->rgltr[i])) {
			rc = PTR_ERR(soc_info->rgltr[i]);
			rc  = rc ? rc : -EINVAL;
			CAM_ERR(CAM_FLASH, "get failed for regulator %s %d",
				soc_info->rgltr_name[i], rc);
			goto free_ctrl;
		}
		CAM_DBG(CAM_FLASH, "get for regulator %s",
			soc_info->rgltr_name[i]);
	}

	if (!soc_info->gpio_data) {
		CAM_DBG(CAM_FLASH, "No GPIO found");
	} else {
		if (!soc_info->gpio_data->cam_gpio_common_tbl_size) {
			CAM_DBG(CAM_FLASH, "No GPIO found");
			rc = -EINVAL;
			goto free_ctrl;
		}

		rc = cam_sensor_util_init_gpio_pin_tbl(soc_info,
			&fctrl->power_info.gpio_num_info);
		if ((rc < 0) || (!fctrl->power_info.gpio_num_info)) {
			CAM_ERR(CAM_FLASH, "No/Error Flash GPIOs");
			rc = -EINVAL;
			goto free_ctrl;
		}
	}

	rc = cam_flash_init_subdev(fctrl);
	if (rc)
		goto free_ctrl;

	fctrl->i2c_data.per_frame =
		kzalloc(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (fctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto unreg_subdev;
	}

	INIT_LIST_HEAD(&(fctrl->i2c_data.init_settings.list_head));
	INIT_LIST_HEAD(&(fctrl->i2c_data.config_settings.list_head));
	INIT_LIST_HEAD(&(fctrl->i2c_data.streamoff_settings.list_head));
	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
		INIT_LIST_HEAD(&(fctrl->i2c_data.per_frame[i].list_head));

	fctrl->func_tbl.parser = cam_flash_i2c_pkt_parser;
	fctrl->func_tbl.apply_setting = cam_flash_i2c_apply_setting;
	fctrl->func_tbl.power_ops = cam_flash_i2c_power_ops;
	fctrl->func_tbl.flush_req = cam_flash_i2c_flush_request;

	fctrl->bridge_intf.device_hdl = -1;
	fctrl->bridge_intf.link_hdl = -1;
	fctrl->bridge_intf.ops.get_dev_info = cam_flash_publish_dev_info;
	fctrl->bridge_intf.ops.link_setup = cam_flash_establish_link;
	fctrl->bridge_intf.ops.apply_req = cam_flash_apply_request;
	fctrl->bridge_intf.ops.flush_req = cam_flash_flush_request;
	fctrl->last_flush_req = 0;

	mutex_init(&(fctrl->flash_mutex));
	fctrl->flash_state = CAM_FLASH_STATE_INIT;

#ifdef CONFIG_CAMERA_I2CFLASH
	cam_flash_i2c_driver_init_kobject(fctrl);
#endif

	CAM_INFO(CAM_FLASH, "cam_flash_i2c_driver_probe X rc: %d", rc);

	return rc;

unreg_subdev:
	cam_unregister_subdev(&(fctrl->v4l2_dev_str));
free_ctrl:
	kfree(fctrl);
	fctrl = NULL;
	return rc;
}

static void cam_flash_i2c_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct i2c_client     *client = NULL;
	struct cam_flash_ctrl *fctrl = NULL;

	client = container_of(dev, struct i2c_client, dev);
	if (!client) {
		CAM_ERR(CAM_FLASH,
			"Failed to get i2c client");
		return;
	}

	fctrl = i2c_get_clientdata(client);

#ifdef CONFIG_CAMERA_I2CFLASH
	cam_flash_i2c_driver_deinit_kobject();
#endif
	/* Handle I2C Devices */
	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "Flash device is NULL");
		return;
	}

	CAM_INFO(CAM_FLASH, "i2c driver remove invoked");
	/*Free Allocated Mem */
	kfree(fctrl->i2c_data.per_frame);
	fctrl->i2c_data.per_frame = NULL;
	kfree(fctrl);
}

const static struct component_ops cam_flash_i2c_component_ops = {
	.bind = cam_flash_i2c_component_bind,
	.unbind = cam_flash_i2c_component_unbind,
};

static int32_t cam_flash_i2c_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;

	if (client == NULL || id == NULL) {
		CAM_ERR(CAM_FLASH, "Invalid Args client: %pK id: %pK",
			client, id);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CAM_ERR(CAM_FLASH, "%s :: i2c_check_functionality failed",
			client->name);
		return -EFAULT;
	}

	CAM_INFO(CAM_FLASH, "Adding sensor flash component");
	rc = component_add(&client->dev, &cam_flash_i2c_component_ops);
	if (rc)
		CAM_ERR(CAM_FLASH, "failed to add component rc: %d", rc);

	return rc;
}

static int32_t cam_flash_i2c_driver_remove(struct i2c_client *client)
{
	component_del(&client->dev, &cam_flash_i2c_component_ops);

	return 0;
}

MODULE_DEVICE_TABLE(of, cam_flash_dt_match);

struct platform_driver cam_flash_platform_driver = {
	.probe = cam_flash_platform_probe,
	.remove = cam_flash_platform_remove,
	.driver = {
		.name = "CAM-FLASH-DRIVER",
		.owner = THIS_MODULE,
		.of_match_table = cam_flash_dt_match,
		.suppress_bind_attrs = true,
	},
};

static const struct of_device_id cam_flash_i2c_dt_match[] = {
	{.compatible = "qcom,cam-i2c-flash", .data = NULL},
	{}
};
MODULE_DEVICE_TABLE(of, cam_flash_i2c_dt_match);

static const struct i2c_device_id i2c_id[] = {
	{FLASH_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

struct i2c_driver cam_flash_i2c_driver = {
	.id_table = i2c_id,
	.probe  = cam_flash_i2c_driver_probe,
	.remove = cam_flash_i2c_driver_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = FLASH_DRIVER_I2C,
		.of_match_table = cam_flash_i2c_dt_match,
		.suppress_bind_attrs = true,
	},
};

int32_t cam_flash_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_register(&cam_flash_platform_driver);
	if (rc < 0) {
		CAM_ERR(CAM_FLASH, "platform probe failed rc: %d", rc);
		return rc;
	}

	CAM_INFO(CAM_FLASH, "platform_driver_register X rc: %d", rc);

	rc = i2c_add_driver(&cam_flash_i2c_driver);
	if (rc < 0)
		CAM_ERR(CAM_FLASH, "i2c_add_driver failed rc: %d", rc);

	CAM_INFO(CAM_FLASH, "i2c_add_driver X rc: %d", rc);

	return rc;
}

void cam_flash_exit_module(void)
{
	platform_driver_unregister(&cam_flash_platform_driver);
	i2c_del_driver(&cam_flash_i2c_driver);
}

MODULE_DESCRIPTION("CAM FLASH");
MODULE_LICENSE("GPL v2");
