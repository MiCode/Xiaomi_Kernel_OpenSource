/***************************************************************************************
 * als_camera.c, source file of imx268 ALS feature.
 *
 * It's a file node, implement get als_data from imx268 sensor and some tuning interface
 * implement, so user space can tuning through /sys/class/als_camera/als.
 *
 * Copyright (c) 2016 Xiaomi Technologies, Inc.  All rights reserved.
 **************************************************************************************/
#include "als_camera.h"

static dev_t als_devno = MKDEV(255, 0);
static struct cdev *als_cdev;
static struct class *als_class;
static struct device *als_dev;

struct camera_als_ctrl_t *als_ctrl = NULL;
static struct timer_list als_timer;
static struct workqueue_struct *als_workqueue;
static struct delayed_work als_delay_work;

static DECLARE_WAIT_QUEUE_HEAD(als_poll_wait_queue);

static int als_data;
#ifdef CAMERA_ALS_TEST
static int debug_cmd;
static int debug_data;
static int debug_store_flag;
static bool test_flag;
static int samp_time = 2;
static int samp_freq = 33;
static int samp_data_accur = 40960;
#endif /* CAMERA_ALS_TEST */
static int open_sensor_flag;
#ifdef CONFIG_COMPAT
static uint32_t open_sensor_flag_compat;
#endif /* CONFIG_COMPAT */
unsigned long als_id = 0xeeeeffff;

static void msm_sensor_get_als_data(struct work_struct *work);
static int als_init_sensor(void);

static struct msm_camera_i2c_fn_t camera_als_cci_func_table = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_util = msm_sensor_cci_i2c_util,
};

static struct msm_cam_clk_info camera_als_clk_info[] = {
	[SENSOR_CAM_MCLK] = {"cam_src_clk", 24000000},
	[SENSOR_CAM_CLK] = {"cam_clk", 0},
};


/* function implement */
static int debug_timer(int time)
{
#ifdef CAMERA_ALS_TEST
	samp_time = time;
#endif /* CAMERA_ALS_TEST */

	return 0;
}

static int debug_samp_area(int data)
{
	int ret = 0;

	if ((data < 0) || (data > 2)) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3200, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	if (ret < 0) {
		ret = -ENOMEM;
		goto exit;
	}
	ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3201, data, MSM_CAMERA_I2C_BYTE_DATA);
	if (ret < 0) {
		ret = -ENOMEM;
		goto exit;
	}
	ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3200, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	if (ret < 0) {
		ret = -ENOMEM;
	}

exit:
	return ret;
}

static int debug_samp_freq(int freq_time)
{
	int ret = 0;
	int s_time = 0;
	uint16_t time_high = 0;
	uint16_t time_low = 0;

	s_time = freq_time;
	if (s_time) {
		if (s_time > 65535) {
			s_time = 65535;
			ret = -ENOMEM;
			goto exit;
		} else if (s_time < 33) {
			s_time = 33;
			ret = -ENOMEM;
			goto exit;
		}

		time_low = (s_time & 0xff);
		time_high = ((s_time >> 8) & 0xff);

		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3200, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3202, time_high, MSM_CAMERA_I2C_BYTE_DATA);
		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3203, time_low, MSM_CAMERA_I2C_BYTE_DATA);
		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3200, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	}

exit:
	return ret;
}

static int debug_samp_accur(int data)
{
	int ret = 0;
	int user_data = 0;
	uint16_t data_high = 0;
	uint16_t data_low = 0;

	user_data = data;
	if (user_data > 3) {
		if (user_data >= 65535) {
			user_data = 65535;
			ret = -ENOMEM;
			goto exit;
		}
		data_low = (user_data & 0xff);
		data_high = ((user_data >> 8) & 0xff);

		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3200, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3290, data_high, MSM_CAMERA_I2C_BYTE_DATA);
		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3291, data_low, MSM_CAMERA_I2C_BYTE_DATA);
		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3200, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	} else if ((user_data >= (-4)) && (user_data <= 3)) {
		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3200, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3292, user_data, MSM_CAMERA_I2C_BYTE_DATA);
		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3200, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	} else {
		ret = -ENOMEM;
	}

exit:
	return ret;
}

static void als_timer_handle(unsigned long arg)
{
	queue_delayed_work(als_workqueue, &als_delay_work, 0);

	return;
}

static int als_open(struct inode *node, struct file *fops)
{
	ALS_PRI_D("%s:%s: open\n", TAG_LOG, __func__);

	return 0;
}

static ssize_t als_read(struct file *fops, char *buf, size_t len, loff_t *fseek)
{
	ALS_PRI_D("%s:%s: read\n", TAG_LOG, __func__);

	if (copy_to_user(buf, &als_data, sizeof(als_data)) != 0) {
			ALS_PRI_E("%s:%s: copy_to_user failed\n", TAG_LOG, __func__);
	}
	als_data = 0;

	return 0;
}

static ssize_t als_write(struct file *fops, const char *buf, size_t len, loff_t *fseek)
{
	int write_data = 0;

	ALS_PRI_D("%s:%s: write\n", TAG_LOG, __func__);

	if (copy_from_user(&write_data, (void __user *)buf, sizeof(write_data))) {
			ALS_PRI_E("%s:%s: copy_from_user failed\n", TAG_LOG, __func__);
	}

	return 0;
}

static long als_ioctl(struct file *fops, unsigned int cmd, unsigned long params)
{
	int data = 0;
	int ret = 0;
	int user_flag = 0;

	ALS_PRI_D("%s:%s: enter\n", TAG_LOG, __func__);

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(&data, (void __user *)params, sizeof(data))) {
			ALS_PRI_E("%s:%s: copy_from_user failed\n", TAG_LOG, __func__);
			return -ENOMEM;
		}
	}

	switch (cmd) {
	case CAMERA_ALS_IOCTL_CMD_OPEN:
		ret = als_init_sensor();
		if (ret < 0) {
			ALS_PRI_E("%s:%s: power up sensor failed\n", TAG_LOG, __func__);
			open_sensor_flag = 0;
		} else {
			open_sensor_flag = 1;

			/* add timer again */
			als_timer.function = &als_timer_handle;
			als_timer.data = als_id;
			als_timer.expires = jiffies + TIME_OUT;
			add_timer(&als_timer);
		}
		user_flag = open_sensor_flag;
		break;
	case CAMERA_ALS_IOCTL_CMD_CLOSE:
		ret = msm_camera_power_down(&als_ctrl->als_board_info->als_power_info, als_ctrl->als_device_type, &als_ctrl->als_i2c_client);
		if (ret < 0) {
			ALS_PRI_E("%s:%s: power down sensor failed\n", TAG_LOG, __func__);
			open_sensor_flag = 1;
		} else {
			open_sensor_flag = 0;
			cancel_delayed_work(&als_delay_work);
			del_timer(&als_timer);
		}
		user_flag = open_sensor_flag;
		break;
	case CAMERA_ALS_IOCTL_CMD_DEBUG_TIMER:
		ret = debug_timer(data);
		user_flag = 1;
		break;
	case CAMERA_ALS_IOCTL_CMD_DEBUG_SAMP_AREA:
		ret = debug_samp_area(data);
		if (ret < 0) {
			user_flag = 0;
		} else {
			user_flag = 1;
		}
		break;
	case CAMERA_ALS_IOCTL_CMD_DEBUG_SAMP_FREQ:
		ret = debug_samp_freq(data);
		if (ret < 0) {
			user_flag = 0;
		} else {
			user_flag = 1;
		}
		break;
	case CAMERA_ALS_IOCTL_CMD_DEBUG_SAMP_ACCUR:
		ret = debug_samp_accur(data);
		if (ret < 0) {
			user_flag = 0;
		} else {
			user_flag = 1;
		}
		break;
	default:
		ALS_PRI_E("%s:%s: default\n", TAG_LOG, __func__);
		break;
	}

	if (copy_to_user((void __user *)params, &user_flag, 1) != 0) {
		ALS_PRI_E("%s:%s: CAMERA_ALS_IOCTL_CMD_READ_ALSCMD copy_to_user failed\n", TAG_LOG, __func__);
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long als_compat_ioctl(struct file *fops, unsigned int cmd, unsigned long params)
{
	int data = 0;
	int ret = 0;
	uint32_t user_flag = 0;

	ALS_PRI_D("%s:%s: enter\n", TAG_LOG, __func__);

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(&data, (void __user *)params, sizeof(data))) {
			ALS_PRI_E("%s:%s: copy_from_user failed\n", TAG_LOG, __func__);
			return -ENOMEM;
		}
	}

	switch (cmd) {
	case CAMERA_ALS_IOCTL_CMD_OPEN:
		ret = als_init_sensor();
		if (ret < 0) {
			ALS_PRI_E("%s:%s: power up sensor failed\n", TAG_LOG, __func__);
			open_sensor_flag_compat = 0;
		} else {
			open_sensor_flag_compat = 1;

			/* add timer again */
			als_timer.function = &als_timer_handle;
			als_timer.data = als_id;
			als_timer.expires = jiffies + TIME_OUT;
			add_timer(&als_timer);
		}
		user_flag = open_sensor_flag_compat;
		break;
	case CAMERA_ALS_IOCTL_CMD_CLOSE:
		ret = msm_camera_power_down(&als_ctrl->als_board_info->als_power_info, als_ctrl->als_device_type, &als_ctrl->als_i2c_client);
		if (ret < 0) {
			ALS_PRI_E("%s:%s: power down sensor failed\n", TAG_LOG, __func__);
			open_sensor_flag_compat = 1;
		} else {
			open_sensor_flag_compat = 0;
			cancel_delayed_work(&als_delay_work);
			del_timer(&als_timer);
		}
		user_flag = open_sensor_flag_compat;
		break;
	case CAMERA_ALS_IOCTL_CMD_DEBUG_TIMER:
		ret = debug_timer(data);
		user_flag = 1;
		break;
	case CAMERA_ALS_IOCTL_CMD_DEBUG_SAMP_AREA:
		ret = debug_samp_area(data);
		if (ret < 0) {
			user_flag = 0;
		} else {
			user_flag = 1;
		}
		break;
	case CAMERA_ALS_IOCTL_CMD_DEBUG_SAMP_FREQ:
		ret = debug_samp_freq(data);
		if (ret < 0) {
			user_flag = 0;
		} else {
			user_flag = 1;
		}
		break;
	case CAMERA_ALS_IOCTL_CMD_DEBUG_SAMP_ACCUR:
		ret = debug_samp_accur(data);
		if (ret < 0) {
			user_flag = 0;
		} else {
			user_flag = 1;
		}
		break;
	default:
		ALS_PRI_E("%s:%s: default\n", TAG_LOG, __func__);
		break;
	}

	if (copy_to_user((void __user *)params, &user_flag, 1) != 0) {
		ALS_PRI_E("%s:%s: CAMERA_ALS_IOCTL_CMD_READ_ALSCMD copy_to_user failed\n", TAG_LOG, __func__);
	}

	return 0;
}
#endif /* CONFIG_COMPAT */

static unsigned int als_poll(struct file *fops, poll_table *wait)
{
	unsigned int mask = 0;

	ALS_PRI_D("%s:%s: enter\n", TAG_LOG, __func__);

	poll_wait(fops, &als_poll_wait_queue, wait);
	if (als_data) {
		mask = POLLIN | POLLRDNORM;
	}

	return mask;
}

/* define and fill file_operations struct */
static const struct file_operations als_fops = {
	.owner = THIS_MODULE,
	.open = als_open,
	.read = als_read,
	.write = als_write,
	.unlocked_ioctl = als_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = als_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.poll = als_poll,
};

static void msm_sensor_get_als_data(struct work_struct *work)
{
	int rc = 0;
	uint16_t high_reg = 0;
	uint16_t low_reg = 0;
	uint16_t reg = 0;
#ifdef CAMERA_ALS_TEST
	int i = 0;
	uint16_t debug_reg = 0;
	int range = 0;
#endif /* CAMERA_ALS_TEST */

#ifdef CAMERA_ALS_TEST
	if (!test_flag) {
#endif /* CAMERA_ALS_TEST */
		if ((!open_sensor_flag) && (!open_sensor_flag_compat)) {
			ALS_PRI_D("%s:%s: user didn't open sensor\n", TAG_LOG, __func__);
			goto exit;
		} else {
#ifdef CAMERA_ALS_TEST
			als_timer.expires = jiffies + (HZ/samp_time);
#else
			als_timer.expires = jiffies + TIME_OUT;
#endif /* CAMERA_ALS_TEST */
			add_timer(&als_timer);
			ALS_PRI_D("%s:%s: add timer\n", TAG_LOG, __func__);
		}
#ifdef CAMERA_ALS_TEST
	}
#endif /* CAMERA_ALS_TEST */

	rc = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3204, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		ALS_PRI_D("%s:%s: write 0x3204,0x01 failed\n", TAG_LOG, __func__);
		goto exit;
	}
#ifdef CAMERA_ALS_TEST
	switch (debug_store_flag) {
	case 22:
		range = 2;
		break;
	case 23:
		range = 8;
		break;
	case 24:
		range = 32;
		break;
	default:
		range = 2;
		break;
	}

	for (i = 0; i < range; i++) {
		rc = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_read(&(als_ctrl->als_i2c_client), (0x3210+i), &debug_reg, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			ALS_PRI_E("%s:%s: read data %d failed\n", TAG_LOG, __func__, i);
		}
		if (i % 2) {
			reg = ((reg << 8) + debug_reg);
#ifdef CAMERA_ALS_TEST
			ALS_PRI_E("%s:%s: debug data area-%d = %d\n", TAG_LOG, __func__, (i/2), reg);
#else
			ALS_PRI_D("%s:%s: debug data area-%d = %d\n", TAG_LOG, __func__, (i/2), reg);
#endif /* CAMERA_ALS_TEST */
			reg = 0;
		} else {
			reg += debug_reg;
		}
	} /* for (i = 0;i < range; i++) */
	reg = 0;
#endif /* CAMERA_ALS_TEST */

	rc = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_read(&(als_ctrl->als_i2c_client), (0x3210+00), &high_reg, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		ALS_PRI_D("%s:%s: read data high failed\n", TAG_LOG, __func__);
		goto exit;
	}
	rc = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_read(&(als_ctrl->als_i2c_client), (0x3210+01), &low_reg, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		ALS_PRI_D("%s:%s: read low high failed\n", TAG_LOG, __func__);
		goto exit;
	}

	reg = ((high_reg << 8) | low_reg);
	ALS_PRI_D("%s:%s data = 0x%x\n", TAG_LOG, __func__, reg);

	rc = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write(&(als_ctrl->als_i2c_client), 0x3204, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		ALS_PRI_D("%s:%s: write 0x3204,0x00 failed\n", TAG_LOG, __func__);
		goto exit;
	}

	if (reg > 0) {
		als_data = reg;
		wake_up(&als_poll_wait_queue);  /* wake up poll wait */
	}

exit:
	return;
}

#ifdef CAMERA_ALS_TEST
static ssize_t als_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int data = 0;

	if (debug_store_flag) {
		data = debug_data;
		debug_data = 0;
	} else {
		 data = als_data;
	}

	return sprintf(buf, "%d\n", data);
}

static ssize_t als_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	sscanf(buf, "%d", &debug_cmd);

	switch (debug_cmd) {
	case 11: /* power up sensor */
		ret = als_init_sensor();
		if (ret < 0)
			debug_data = 99;
		debug_store_flag = debug_cmd;
		test_flag = true;
		break;
	case 12: /* read als data */
		msm_sensor_get_als_data(NULL);
		break;
	case 13: /* power down sensor */
		ret = msm_camera_power_down(&als_ctrl->als_board_info->als_power_info, als_ctrl->als_device_type, &als_ctrl->als_i2c_client);
		if (ret < 0)
			debug_data = 99;
		debug_store_flag = debug_cmd;
		test_flag = true;
		break;
	case 22: /* change sampling area, 1*/
		ret = debug_samp_area(0x00);
		if (ret < 0)
			debug_data = 99;
		debug_store_flag = debug_cmd;
		break;
	case 23: /* change sampling area, 4*/
		ret = debug_samp_area(0x01);
		if (ret < 0)
			debug_data = 99;
		debug_store_flag = debug_cmd;
		break;
	case 24: /* change sampling area, 16*/
		ret = debug_samp_area(0x02);
		if (ret < 0)
			debug_data = 99;
		debug_store_flag = debug_cmd;
		break;
	default:
		break;
	}

	return count;
}
static DEVICE_ATTR_RW(als);

static ssize_t time_als_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", samp_time);
}

static ssize_t time_als_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int als_time = 0;
	int ret = 0;

	sscanf(buf, "%d", &als_time);

	if (!als_time) {
		als_time = 0;
	}
	ret = debug_timer(als_time);

	return count;
}
static DEVICE_ATTR_RW(time_als);

static ssize_t sample_als_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", samp_freq);
}

static ssize_t sample_als_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	sscanf(buf, "%d", &samp_freq);

	ret = debug_samp_freq(samp_freq);

	return count;
}
static DEVICE_ATTR_RW(sample_als);

static ssize_t debug_als_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", samp_data_accur);
}

static ssize_t debug_als_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int user_data = 0;

	sscanf(buf, "%d", &samp_data_accur);
	user_data = samp_data_accur;

	if ((user_data >= (-4)) && (user_data <= 3)) {
		samp_data_accur = 99;
	}

	ret = debug_samp_accur(user_data);

	return count;
}
static DEVICE_ATTR_RW(debug_als);

static struct device_attribute *als_list[] = {
	&dev_attr_als,
	&dev_attr_time_als,
	&dev_attr_sample_als,
	&dev_attr_debug_als,
};
#endif /* CAMERA_ALS_TEST */

static int als_init_sensor(void)
{
	int ret = 0;
	struct device_node *ds_node = NULL;

	ds_node = als_ctrl->pdev->dev.of_node;
	if (!ds_node) {
		ALS_PRI_E("%s:%s: pdev->dev.of_node is NULL\n", TAG_LOG, __func__);
		goto init_error;
	}

	ret = msm_camera_power_up(&als_ctrl->als_board_info->als_power_info, als_ctrl->als_device_type, &als_ctrl->als_i2c_client);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: test power up sensor failed\n", TAG_LOG, __func__);
		goto init_error;
	} else if (0 == ret) {
		/* fill init setting */
		als_init_reg_setting.reg_setting = &als_init_setting_array[0];
		als_init_reg_setting.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
		als_init_reg_setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
		als_init_reg_setting.delay = 0;
		als_init_reg_setting.size = (sizeof(als_init_setting_array) / sizeof(struct msm_camera_i2c_reg_array));
		ALS_PRI_D("%s:%s: size = %d\n", TAG_LOG, __func__, als_init_reg_setting.size);

		ret = als_ctrl->als_i2c_client.i2c_func_tbl->i2c_write_table(&(als_ctrl->als_i2c_client), &als_init_reg_setting);
		if (ret < 0) {
			ALS_PRI_E("%s:%s: test i2c_write_table failed\n", TAG_LOG, __func__);
			goto init_error;
		}
	}

	return 0;

init_error:
	return -ENOMEM;
}

static int get_power_down_setting(struct device_node *ds_node, struct camera_vreg_t *cam_vreg, int num_vreg, struct msm_camera_power_ctrl_t *als_power_info)
{
	struct msm_sensor_power_setting *ps;
	uint16_t count = 0;
	const char *seq_name = NULL;
	uint32_t *array = NULL;
	int i = 0, rc = 0, j = 0;

	if (!ds_node) {
		ALS_PRI_E("%s:%s: ds_node is NULL\n", TAG_LOG, __func__);
		rc = -ENOMEM;
		goto exit;
	}

	count = of_property_count_strings(ds_node, "qcom,cam-power-down-seq-type");
	if (count <= 0) {
		ALS_PRI_E("%s:%s: read seq-type count failed\n", TAG_LOG, __func__);
		rc = -ENOMEM;
		goto exit;
	}

	ps = kzalloc((sizeof(*ps) * count), GFP_KERNEL);
	als_power_info->power_down_setting = ps;
	als_power_info->power_down_setting_size = count;

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(ds_node, "qcom,cam-power-down-seq-type", i, &seq_name);
		ALS_PRI_D("%s:%s: seq_name[%d] = %s\n", TAG_LOG, __func__, i, seq_name);
		if (rc < 0) {
			ALS_PRI_E("%s:%s: read seq-type index failed\n", TAG_LOG, __func__);
			goto exit;
		}
		if (!strcmp(seq_name, "sensor_vreg")) {
			ps[i].seq_type = SENSOR_VREG;
		} else if (!strcmp(seq_name, "sensor_gpio")) {
			ps[i].seq_type = SENSOR_GPIO;
		} else if (!strcmp(seq_name, "sensor_clk")) {
			ps[i].seq_type = SENSOR_CLK;
		} else {
			ALS_PRI_D("%s:%s: unrecognized seq-type\n", TAG_LOG, __func__);
			rc = -ENOMEM;
			goto exit;
		}
		ALS_PRI_D("%s:%s: seq_type[%d] %d\n", TAG_LOG, __func__, i, ps[i].seq_type);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(ds_node, "qcom,cam-power-down-seq-val", i, &seq_name);
		ALS_PRI_D("%s:%s: seq_name[%d] = %s\n", TAG_LOG, __func__, i, seq_name);
		if (rc < 0) {
			ALS_PRI_E("%s:%s: read seq-val index failed\n", TAG_LOG, __func__);
			goto exit;
		}
		switch (ps[i].seq_type) {
		case SENSOR_VREG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(seq_name, cam_vreg[j].reg_name))
					break;
			}
			if (j < num_vreg)
				ps[i].seq_val = j;
			else
				rc = -ENOMEM;
			break;
		case SENSOR_GPIO:
			if (!strcmp(seq_name, "sensor_gpio_reset"))
				ps[i].seq_val = SENSOR_GPIO_RESET;
			else if (!strcmp(seq_name, "sensor_gpio_standby"))
				ps[i].seq_val = SENSOR_GPIO_STANDBY;
			else if (!strcmp(seq_name, "sensor_gpio_vdig"))
				ps[i].seq_val = SENSOR_GPIO_VDIG;
			else if (!strcmp(seq_name, "sensor_gpio_vana"))
				ps[i].seq_val = SENSOR_GPIO_VANA;
			else if (!strcmp(seq_name, "sensor_gpio_vaf"))
				ps[i].seq_val = SENSOR_GPIO_VAF;
			else if (!strcmp(seq_name, "sensor_gpio_vio"))
				ps[i].seq_val = SENSOR_GPIO_VIO;
			else
				rc = -ENOMEM;
			break;
		case SENSOR_CLK:
			if (!strcmp(seq_name, "sensor_cam_mclk"))
				ps[i].seq_val = SENSOR_CAM_MCLK;
			else if (!strcmp(seq_name, "sensor_cam_clk"))
				ps[i].seq_val = SENSOR_CAM_CLK;
			else
				rc = -ENOMEM;
			break;
		default:
			rc = -ENOMEM;
			break;
		}
		if (rc < 0) {
			ALS_PRI_E("%s:%s: unrecognized seq-val\n", TAG_LOG, __func__);
			goto exit;
		}
	}

	array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!array) {
		ALS_PRI_E("%s:%s: alloc array failed\n", TAG_LOG, __func__);
		rc = -ENOMEM;
		goto exit;
	}

	rc = of_property_read_u32_array(ds_node, "qcom,cam-power-down-seq-cfg-val", array, count);
	if (rc < 0) {
		ALS_PRI_E("%s:%s: read cfg-val failed\n", TAG_LOG, __func__);
		rc = -ENOMEM;
		goto free;
	}
	for (i = 0; i < count; i++) {
		if (ps[i].seq_type == SENSOR_GPIO) {
			if (array[i] == 0)
				ps[i].config_val = GPIO_OUT_LOW;
			else if (array[i] == 1)
				ps[i].config_val = GPIO_OUT_HIGH;
		} else {
			ps[i].config_val = array[i];
		}
		ALS_PRI_D("%s:%s: power_down_setting[%d].config_val = %ld\n", TAG_LOG, __func__, i, ps[i].config_val);
	}

	rc = of_property_read_u32_array(ds_node, "qcom,cam-power-down-seq-delay", array, count);
	if (rc < 0) {
		ALS_PRI_E("%s:%s: read seq-delay failed\n", TAG_LOG, __func__);
		rc = -ENOMEM;
		goto free;
	}
	for (i = 0; i < count; i++) {
		ps[i].delay = array[i];
		ALS_PRI_D("%s:%s: power_down_setting[%d].delay = %d\n", TAG_LOG, __func__, i, ps[i].delay);
	}

free:
	kfree(array);

exit:
	return rc;
}

static int camera_als_get_power_info(void)
{
	int ret = 0;
	int i = 0;
	int8_t gpio_array_size = 0;
	struct als_board_info_t *ab_info = NULL;
	struct msm_camera_power_ctrl_t *als_power_info = NULL;
	struct device_node *ds_node = NULL;
	struct msm_camera_gpio_conf *als_gconf = NULL;
	uint16_t *als_gpio_array = NULL;

	ds_node = als_ctrl->pdev->dev.of_node;
	if (!ds_node) {
		ALS_PRI_E("%s:%s: pdev->dev.of_node is NULL\n", TAG_LOG, __func__);
		ret = -ENOMEM;
		goto exit;
	}

	ab_info = als_ctrl->als_board_info;
	als_power_info = &als_ctrl->als_board_info->als_power_info;
	ret = msm_camera_get_dt_vreg_data(ds_node, &als_power_info->cam_vreg, &als_power_info->num_vreg);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: read num_vreg failed\n", TAG_LOG, __func__);
		ret = -ENOMEM;
		goto exit;
	}
	ALS_PRI_D("%s:%s: als_power_info->num_vreg = %d\n", TAG_LOG, __func__, als_power_info->num_vreg);

	ret = msm_camera_get_dt_power_setting_data(ds_node, als_power_info->cam_vreg, als_power_info->num_vreg, als_power_info);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: read power setting failed\n", TAG_LOG, __func__);
		ret = -ENOMEM;
		goto exit;
	}

	ret = get_power_down_setting(ds_node, als_power_info->cam_vreg, als_power_info->num_vreg, als_power_info);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: read power down setting failed\n", TAG_LOG, __func__);
		ret = -ENOMEM;
		goto exit;
	}

	als_power_info->gpio_conf = kzalloc(sizeof(struct msm_camera_gpio_conf), GFP_KERNEL);
	if (!als_power_info->gpio_conf) {
		ret = -ENOMEM;
		goto exit;
	}
	als_gconf = als_power_info->gpio_conf;
	gpio_array_size = of_gpio_count(ds_node);

	if (gpio_array_size > 0) {
		als_gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size, GFP_KERNEL);
		if (!als_gpio_array) {
			ALS_PRI_E("%s:%s: alloc gpio_array ailed\n", TAG_LOG, __func__);
			ret = -ENOMEM;
			goto free_gpio_conf;
		}
		for (i = 0; i < gpio_array_size; i++) {
			als_gpio_array[i] = of_get_gpio(ds_node, i);
			ALS_PRI_D("%s:%s: gpio_array[%d] = %d\n", TAG_LOG, __func__, i, als_gpio_array[i]);
		}

		ret = msm_camera_get_dt_gpio_req_tbl(ds_node, als_gconf, als_gpio_array, gpio_array_size);
		if (ret < 0) {
			ALS_PRI_E("%s:%s: get gpio table from dts failed\n", TAG_LOG, __func__);
			ret = -ENOMEM;
			goto free_gpio_array;
		}

		ret = msm_camera_init_gpio_pin_tbl(ds_node, als_gconf, als_gpio_array, gpio_array_size);
		if (ret < 0) {
			ALS_PRI_E("%s:%s: get gpio table from dts failed\n", TAG_LOG, __func__);
			ret = -ENOMEM;
			goto free_gpio_array;
		}
		kfree(als_gpio_array);
	}

	return ret;

free_gpio_array:
	kfree(als_gpio_array);

free_gpio_conf:
	kfree(als_power_info->gpio_conf);

exit:
	return ret;
}

static int camera_als_parse_dts(void)
{
	int ret = 0;
	uint32_t temp = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct als_board_info_t *ab_info = NULL;
	struct msm_camera_power_ctrl_t *als_power_info = NULL;
	struct device_node *ds_node = NULL;

	ds_node = als_ctrl->pdev->dev.of_node;
	if (!ds_node) {
		ALS_PRI_E("%s:%s: pdev->dev.of_node is NULL\n", TAG_LOG, __func__);
		return -ENOMEM;
	}

	als_ctrl->als_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	als_ctrl->als_i2c_client.i2c_func_tbl = &camera_als_cci_func_table;
	als_ctrl->als_i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	als_ctrl->als_i2c_client.cci_client = kzalloc(sizeof(struct msm_camera_cci_client), GFP_KERNEL);
	if (!als_ctrl->als_i2c_client.cci_client) {
		ALS_PRI_E("%s:%s: alloc cci_client fail\n", TAG_LOG, __func__);
		goto alloc_cci_error;
	}
	cci_client = als_ctrl->als_i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->retries = 3;
	cci_client->id_map = 0;

	als_ctrl->als_board_info = kzalloc(sizeof(struct als_board_info_t), GFP_KERNEL);
	if (!als_ctrl->als_board_info) {
		ALS_PRI_E("%s:%s: alloc board info failed\n", TAG_LOG, __func__);
		goto alloc_boardInfo_error;
	}

	ab_info = als_ctrl->als_board_info;
	als_power_info = &ab_info->als_power_info;
	als_power_info->clk_info = camera_als_clk_info;
	als_power_info->clk_info_size = ARRAY_SIZE(camera_als_clk_info);
	als_power_info->dev = &als_ctrl->pdev->dev;

	ret = of_property_read_string(ds_node, "qcom,camera-als-name", &ab_info->als_name);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: read als-name failed\n", TAG_LOG, __func__);
		goto alloc_boardInfo_error;
	}
	ALS_PRI_D("%s:%s: qcom,camera-als-name = %s\n", TAG_LOG, __func__, ab_info->als_name);

	ret = of_property_read_u32(ds_node, "cell-index", &als_ctrl->pdev->id);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: read cell-index failed\n", TAG_LOG, __func__);
		goto alloc_boardInfo_error;
	}
	als_ctrl->subdev_id = als_ctrl->pdev->id;
	ALS_PRI_D("%s:%s: cell-index = %d\n", TAG_LOG, __func__, als_ctrl->pdev->id);

	ret = of_property_read_u32(ds_node, "qcom,cci-master", &als_ctrl->als_i2c_master);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: read cci-master failed\n", TAG_LOG, __func__);
		goto alloc_boardInfo_error;
	}
	cci_client->cci_i2c_master = als_ctrl->als_i2c_master;
	ALS_PRI_D("%s:%s: qcom,cci-master = %d\n", TAG_LOG, __func__, als_ctrl->als_i2c_master);

	ret = of_property_read_u32(ds_node, "qcom,slave-addr", &temp);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: read i2c_addr failed\n", TAG_LOG, __func__);
		ab_info->i2c_addr = 0x20;
	}

	ab_info->i2c_addr = temp;
	cci_client->sid = (ab_info->i2c_addr >> 1);
	ALS_PRI_D("%s:%s: qcom,slave-addr = %d\n", TAG_LOG, __func__, temp);

	ret = of_property_read_u32(ds_node, "qcom,i2c-freq-mode", &cci_client->i2c_freq_mode);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: read i2c_freq_mode failed\n", TAG_LOG, __func__);
		 cci_client->i2c_freq_mode = 1;
	}
	ALS_PRI_D("%s:%s: qcom,i2c-freq-mode = %d\n", TAG_LOG, __func__, cci_client->i2c_freq_mode);

	ret = camera_als_get_power_info();
	if (ret < 0) {
		ALS_PRI_E("%s:%s: get_power_info failed\n", TAG_LOG, __func__);
		goto alloc_error;
	}

	return 0;

alloc_error:
	kfree(als_ctrl->als_board_info);

alloc_boardInfo_error:
	kfree(als_ctrl->als_i2c_client.cci_client);

alloc_cci_error:
	return -ENOMEM;
}

static int als_create_chardev(void)
{
	if (register_chrdev_region(als_devno, 1, "als_camera")) {
		ALS_PRI_E("%s:%s: register_chrdev_region failed\n", TAG_LOG, __func__);
		goto error;
	}

	if (NULL == (als_cdev = cdev_alloc())) {
		unregister_chrdev_region(als_devno, 1);
		ALS_PRI_E("%s:%s: cdev_alloc failed\n", TAG_LOG, __func__);
		goto error;
	}

	als_cdev->owner = THIS_MODULE;
	als_cdev->ops = &als_fops;

	if (cdev_add(als_cdev, als_devno, 1)) {
		unregister_chrdev_region(als_devno, 1);
		ALS_PRI_E("%s:%s: cdev_sdd failed\n", TAG_LOG, __func__);
		goto error;
	}

	return 0;

error:
	return -ENOMEM;
}

static int als_create_device(void)
{
	int err = 0;
#ifdef CAMERA_ALS_TEST
	int attr_count = 0;
	int i = 0;
#endif /* CAMERA_ALS_TEST */

	if (als_create_chardev() != 0) {
		err = -ENOMEM;
		goto exit;
	}

	als_class = class_create(THIS_MODULE, "als_camera");
	if (IS_ERR(als_class)) {
		ALS_PRI_E("%s:%s: class_create failed\n", TAG_LOG, __func__);
		err = -ENOMEM;
		goto create_exit;
	}

	als_dev = device_create(als_class, NULL, als_devno, NULL, "als_camera");
	if (IS_ERR(als_dev)) {
		ALS_PRI_E("%s:%s: device_create failed\n", TAG_LOG, __func__);
		err = -ENOMEM;
		goto create_exit;
	}

#ifdef CAMERA_ALS_TEST
	attr_count = (sizeof(als_list) / sizeof(als_list[0]));

	for (i = 0; i < attr_count; i++) {
		if (device_create_file(als_dev, als_list[i]) < 0) {
			ALS_PRI_E("%s:%s: create attribute file fail\n", TAG_LOG, __func__);
			err = -ENOMEM;
		}
	}
#endif /* CAMERA_ALS_TEST */

	return err;

create_exit:
	cdev_del(als_cdev);
	unregister_chrdev_region(als_devno, 1);

exit:
	return err;
}

static int camera_als_platform_probe(struct platform_device *pdev)
{
	int ret = 0;

	ALS_PRI_D("%s:%s: stqcom,als_camera starting\n", TAG_LOG, __func__);

	if (pdev->dev.of_node == NULL) {
		ALS_PRI_E("%s:%s: of_node is NULL\n", TAG_LOG, __func__);
		return -ENOMEM;
	}

	als_ctrl = kzalloc(sizeof(struct camera_als_ctrl_t), GFP_KERNEL);
	if (!als_ctrl) {
		ALS_PRI_E("%s:%s: alloc als_ctrl failed\n", TAG_LOG, __func__);
		return -ENOMEM;
	}

	als_ctrl->pdev = pdev;

	ret = camera_als_parse_dts();
	if (ret < 0) {
		ALS_PRI_E("%s:%s: parse dts failed\n", TAG_LOG, __func__);
		goto als_error;
	}

#ifdef CAMERA_ALS_TEST
	ret = als_init_sensor();
	if (ret < 0) {
		ALS_PRI_E("%s:%s: test als_init_sensor failed\n", TAG_LOG, __func__);
		goto als_error;
	}

	ALS_PRI_D("%s:%s: test read data begin\n", TAG_LOG, __func__);
	msleep(10);
	test_flag = true;
	msm_sensor_get_als_data(NULL);
	test_flag = false;
	ALS_PRI_D("%s:%s: test read data end\n", TAG_LOG, __func__);

	ret = msm_camera_power_down(&als_ctrl->als_board_info->als_power_info, als_ctrl->als_device_type, &als_ctrl->als_i2c_client);
	if (ret < 0) {
		ALS_PRI_E("%s:%s: test power down sensor failed\n", TAG_LOG, __func__);
	}
#endif /* CAMERA_ALS_TEST*/

	init_timer(&als_timer); /* init timer */

	als_workqueue = create_workqueue("als_workqueue");
	if (!als_workqueue) {
		ALS_PRI_E("%s:%s: create work queue fail\n", TAG_LOG, __func__);
	}
	INIT_DELAYED_WORK(&als_delay_work, msm_sensor_get_als_data);

	ret = als_create_device();
	if (ret < 0) {
		ALS_PRI_E("%s:%s: create device failed\n", TAG_LOG, __func__);
		goto als_error;
	}

	ALS_PRI_D("%s:%s: end\n", TAG_LOG, __func__);

	return 0;

als_error:
	kfree(als_ctrl);
	return -ENOMEM;
}

static int camera_als_platform_remove(struct platform_device *pdev)
{
	cancel_delayed_work(&als_delay_work);
	if (als_workqueue) {
		flush_workqueue(als_workqueue);
		destroy_workqueue(als_workqueue);
	}
	del_timer(&als_timer);
	cdev_del(als_cdev);
	if (als_dev) {
		device_destroy(als_class, als_devno);
	}
	if (als_class) {
		class_destroy(als_class);
	}
	unregister_chrdev_region(als_devno, 1);

	return 0;
}

static const struct of_device_id als_device_id_match[] = {
	{ .compatible = "qcom,camera-als" },
	{}
};
MODULE_DEVICE_TABLE(of, als_device_id_match);

static struct platform_driver camera_als_platform_driver = {
	.probe = camera_als_platform_probe,
	.remove = camera_als_platform_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "qcom,camera-als",
		.of_match_table = als_device_id_match,
	},
};

static int __init als_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&camera_als_platform_driver);
	if (ret < 0) {
		ALS_PRI_E("%s:%s:platform driver register failed!\n", TAG_LOG, __func__);
	}

	return 0;
}

static void __exit als_exit(void)
{
	platform_driver_unregister(&camera_als_platform_driver);

	return;
}

module_init(als_init);
module_exit(als_exit);
MODULE_DESCRIPTION("als_camera_driver");
MODULE_AUTHOR("Xiaomi Technologies, Inc");
MODULE_LICENSE("GPL v2");
