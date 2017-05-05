/*
 *  stmvl53l0_module-cci.c - Linux kernel modules for STM VL53L0 FlightSense TOF
 *							sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
/*
 * power specific includes
 */
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
/*
 * API includes
 */
#include "vl53l0_api.h"
#include "vl53l0_def.h"
#include "vl53l0_platform.h"
#include "stmvl53l0-cci.h"
#include "stmvl53l0-i2c.h"
#include "stmvl53l0.h"

#ifdef CAMERA_CCI
/*
 * Global data
 */
static struct v4l2_file_operations msm_tof_v4l2_subdev_fops;
static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
	    msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll = msm_camera_cci_i2c_poll,
};

static int stmvl53l0_get_dt_data(struct device *dev, struct cci_data *data);

/*
 * QCOM specific functions
 */
static int stmvl53l0_get_dt_data(struct device *dev, struct cci_data *data)
{
	int rc = 0;

	vl53l0_dbgmsg("Enter\n");

	if (dev->of_node) {
		struct device_node *of_node = dev->of_node;
		struct msm_tof_vreg *vreg_cfg;

		if (!of_node) {
			vl53l0_errmsg("failed %d\n", __LINE__);
			return -EINVAL;
		}

		rc = of_property_read_u32(of_node,
					  "cell-index", &data->pdev->id);
		if (rc < 0) {
			vl53l0_errmsg("failed %d\n", __LINE__);
			return rc;
		}
		vl53l0_dbgmsg("cell-index: %d\n", data->pdev->id);
		rc = of_property_read_u32(of_node, "qcom,cci-master",
					  &data->cci_master);
		if (rc < 0) {
			vl53l0_errmsg("failed %d\n", __LINE__);
			/* Set default master 0 */
			data->cci_master = MASTER_0;
			rc = 0;
		}
		vl53l0_dbgmsg("cci_master: %d\n", data->cci_master);
		if (of_find_property(of_node, "qcom,cam-vreg-name", NULL)) {
			vreg_cfg = &data->vreg_cfg;
			rc = msm_camera_get_dt_vreg_data(of_node,
							 &vreg_cfg->cam_vreg,
							 &vreg_cfg->num_vreg);
			if (rc < 0) {
				vl53l0_errmsg("failed %d\n", __LINE__);
				return rc;
			}
		}
		vl53l0_dbgmsg("vreg-name: %s min_volt: %d max_volt: %d",
			      vreg_cfg->cam_vreg->reg_name,
			      vreg_cfg->cam_vreg->min_voltage,
			      vreg_cfg->cam_vreg->max_voltage);

		rc = msm_sensor_driver_get_gpio_data(&(data->gconf), of_node);
		if ((rc < 0) || (data->gconf == NULL)) {
			vl53l0_errmsg
			    ("No Laser Sensor GPIOs to be configured!\n");
		}

	}
	vl53l0_dbgmsg("End rc =%d\n", rc);

	return rc;
}

static int32_t stmvl53l0_vreg_control(struct cci_data *data, int config)
{
	int rc = 0, i, cnt;
	struct msm_tof_vreg *vreg_cfg;

	vl53l0_dbgmsg("Enter\n");

	vreg_cfg = &data->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	vl53l0_dbgmsg("num_vreg: %d\n", cnt);
	if (!cnt) {
		vl53l0_errmsg("failed %d\n", __LINE__);
		return 0;
	}

	if (cnt >= MSM_TOF_MAX_VREGS) {
		vl53l0_errmsg("failed %d cnt %d\n", __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = msm_camera_config_single_vreg(&(data->pdev->dev),
						   &vreg_cfg->cam_vreg[i],
						   (struct regulator **)
						   &vreg_cfg->data[i], config);
	}

	vl53l0_dbgmsg("EXIT rc =%d\n", rc);
	return rc;
}

static int msm_tof_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int rc = 0;
/*
 *	struct msm_tof_ctrl_t *tof_ctrl =  v4l2_get_subdevdata(sd);
 *	if (!tof_ctrl) {
 *		pr_err("failed\n");
 *		return -EINVAL;
 *	}
 *	if (tof_ctrl->tof_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
 *		rc = tof_ctrl->i2c_client.i2c_func_tbl->i2c_util(
 *			&tof_ctrl->i2c_client, MSM_CCI_RELEASE);
 *		if (rc < 0)
 *			pr_err("cci_init failed\n");
 *	}
 *    tof_ctrl->i2c_state = TOF_I2C_RELEASE;
 */
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_tof_internal_ops = {
	.close = msm_tof_close,
};

static long msm_tof_subdev_ioctl(struct v4l2_subdev *sd,
				 unsigned int cmd, void *arg)
{
	struct cci_data *cci_object = NULL;
	int32_t rc = 0;

	cci_object = v4l2_get_subdevdata(sd);
	if (cmd == MSM_SD_SHUTDOWN)
		cci_object->power_up = 0;

	vl53l0_dbgmsg("cmd = %d power_up = %d", cmd, cci_object->power_up);
	return rc;
}

static int32_t msm_tof_power(struct v4l2_subdev *sd, int on)
{
	vl53l0_dbgmsg("TOF power called\n");
	return 0;
}

static struct v4l2_subdev_core_ops msm_tof_subdev_core_ops = {
	.ioctl = msm_tof_subdev_ioctl,
	.s_power = msm_tof_power,
};

static struct v4l2_subdev_ops msm_tof_subdev_ops = {
	.core = &msm_tof_subdev_core_ops,
};

static int stmvl53l0_cci_init(struct cci_data *data)
{
	int rc = 0;
	struct msm_camera_cci_client *cci_client = data->client->cci_client;

	if (data->subdev_initialized == FALSE) {
		data->client->i2c_func_tbl = &msm_sensor_cci_func_tbl;
		data->client->cci_client =
		    kzalloc(sizeof(struct msm_camera_cci_client), GFP_KERNEL);
		if (!data->client->cci_client) {
			vl53l0_errmsg("%d, failed no memory\n", __LINE__);
			return -ENOMEM;
		}
		cci_client = data->client->cci_client;
		cci_client->cci_subdev = msm_cci_get_subdev();
		if (cci_client->cci_subdev == NULL) {
			vl53l0_errmsg("CCI subdev is not initialized!!\n");
			return -ENODEV;
		}
		cci_client->cci_i2c_master = data->cci_master;
		v4l2_subdev_init(&data->msm_sd.sd, data->subdev_ops);
		v4l2_set_subdevdata(&data->msm_sd.sd, data);
		data->msm_sd.sd.internal_ops = &msm_tof_internal_ops;
		data->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		snprintf(data->msm_sd.sd.name,
			 ARRAY_SIZE(data->msm_sd.sd.name), "msm_tof");
		media_entity_init(&data->msm_sd.sd.entity, 0, NULL, 0);
		data->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
		data->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_TOF;
		data->msm_sd.close_seq =
		    MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
		msm_sd_register(&data->msm_sd);
		msm_tof_v4l2_subdev_fops = v4l2_subdev_fops;
		data->msm_sd.sd.devnode->fops =
		    &msm_tof_v4l2_subdev_fops;
		data->subdev_initialized = TRUE;
	}

	cci_client->sid = 0x29;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = data->cci_master;
	cci_client->i2c_freq_mode = I2C_FAST_MODE;
	rc = data->client->i2c_func_tbl->i2c_util(data->client, MSM_CCI_INIT);
	if (rc < 0) {
		vl53l0_errmsg("%d: CCI Init failed\n", __LINE__);
		return rc;
	}
	vl53l0_dbgmsg("CCI Init Succeeded\n");

	data->client->addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

	return 0;
}

static int32_t stmvl53l0_platform_probe(struct platform_device *pdev)
{
	struct stmvl53l0_data *vl53l0_data = NULL;
	struct cci_data *cci_object = NULL;
	int32_t rc = 0;

	vl53l0_dbgmsg("Enter\n");

	if (!pdev->dev.of_node) {
		vl53l0_errmsg("of_node NULL\n");
		return -EINVAL;
	}

	vl53l0_data = kzalloc(sizeof(struct stmvl53l0_data), GFP_KERNEL);
	if (!vl53l0_data) {
		rc = -ENOMEM;
		return rc;
	}
	if (vl53l0_data) {
		vl53l0_data->client_object =
		    kzalloc(sizeof(struct cci_data), GFP_KERNEL);
		cci_object = (struct cci_data *)vl53l0_data->client_object;
	}
	cci_object->client =
	    (struct msm_camera_i2c_client *)&cci_object->g_client;

	/* setup bus type */
	vl53l0_data->bus_type = CCI_BUS;

	/* Set platform device handle */
	cci_object->subdev_ops = &msm_tof_subdev_ops;
	cci_object->pdev = pdev;
	rc = stmvl53l0_get_dt_data(&pdev->dev, cci_object);
	if (rc < 0) {
		vl53l0_errmsg("%d, failed rc %d\n", __LINE__, rc);
		kfree(vl53l0_data->client_object);
		kfree(vl53l0_data);
		return rc;
	}
	vl53l0_data->irq_gpio = of_get_named_gpio_flags(pdev->dev.of_node,
		"stm,irq-gpio", 0, NULL);

	if (!gpio_is_valid(vl53l0_data->irq_gpio)) {
		vl53l0_errmsg("%d failed get irq gpio", __LINE__);
		kfree(vl53l0_data->client_object);
		kfree(vl53l0_data);
		return -EINVAL;
	}

	cci_object->subdev_id = pdev->id;

	/* Set device type as platform device */
	cci_object->device_type = MSM_CAMERA_PLATFORM_DEVICE;
	cci_object->subdev_initialized = FALSE;

	/* setup device name */
	vl53l0_data->dev_name = dev_name(&pdev->dev);

	/* setup device data */
	dev_set_drvdata(&pdev->dev, vl53l0_data);

	/* setup other stuff */
	rc = stmvl53l0_setup(vl53l0_data);

	/* init default value */
	cci_object->power_up = 0;

	vl53l0_dbgmsg("End\n");

	return rc;

}

static int32_t stmvl53l0_platform_remove(struct platform_device *pdev)
{
	struct stmvl53l0_data *vl53l0_data = platform_get_drvdata(pdev);

	stmvl53l0_cleanup(vl53l0_data);
	platform_set_drvdata(pdev, NULL);

	kfree(vl53l0_data->client_object);
	kfree(vl53l0_data);

	return 0;
}

static const struct of_device_id st_stmvl53l0_dt_match[] = {
	{.compatible = "st,stmvl53l0",},
	{},
};

static struct platform_driver stmvl53l0_platform_driver = {
	.probe = stmvl53l0_platform_probe,
	.remove = stmvl53l0_platform_remove,
	.driver = {
		   .name = STMVL53L0_DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = st_stmvl53l0_dt_match,
		   },
};

int stmvl53l0_power_up_cci(void *cci_object, unsigned int *preset_flag)
{
	int ret = 0;
	struct cci_data *data = (struct cci_data *)cci_object;
	struct gpio *gpio_tbl = NULL;
	uint8_t gpio_tbl_size = 0;
	int i = 0;

	vl53l0_dbgmsg("Enter");

	/* need to init cci first */
	ret = stmvl53l0_cci_init(data);
	if (ret) {
		vl53l0_errmsg("stmvl53l0_cci_init failed %d\n", __LINE__);
		return ret;
	}

	/* Check if GPIO needs to be enabled for chip select */
	vl53l0_dbgmsg("Get gpio table!size: %d\n",
		      data->gconf->cam_gpio_req_tbl_size);
	gpio_tbl = data->gconf->cam_gpio_req_tbl;
	gpio_tbl_size = data->gconf->cam_gpio_req_tbl_size;
	if (gpio_tbl_size > 0) {
		ret = msm_camera_pinctrl_init(&(data->pinctrl_info),
					      &(data->pdev->dev));
		if (ret < 0) {
			vl53l0_errmsg("Initialization of pinctrl failed\n");
			data->cam_pinctrl_status = 0;
		} else {
			data->cam_pinctrl_status = 1;
		}

		for (i = 0; i < gpio_tbl_size; i++) {
			ret = gpio_request_one(gpio_tbl[i].gpio,
					       gpio_tbl[i].flags,
					       gpio_tbl[i].label);
			if (ret < 0) {
				vl53l0_errmsg
				    ("Request for GPIO %d failed! Err: %d\n",
				     gpio_tbl[i].gpio, ret);
			} else {
				if (data->cam_pinctrl_status) {
					ret =
					  pinctrl_select_state(
					     data->pinctrl_info.pinctrl,
					 data->pinctrl_info.gpio_state_active);
					if (ret < 0) {
						vl53l0_errmsg(
							"%s: Cannot set pin to active state!\n",
						     __func__);
					}
				}
				vl53l0_dbgmsg("Set pin %d value to 1!\n",
					      gpio_tbl[i].gpio);
				gpio_set_value_cansleep(gpio_tbl[i].gpio, 1);
			}
		}
	}

	/* actual power up */
	if (data && data->device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		ret = stmvl53l0_vreg_control(data, 1);
		if (ret < 0) {
			vl53l0_errmsg("stmvl53l0_vreg_control failed %d\n",
				      __LINE__);
			return ret;
		}
	}
	data->power_up = 1;
	usleep_range(3000, 3500);
	*preset_flag = 1;
	vl53l0_dbgmsg("End\n");

	return ret;
}

int stmvl53l0_power_down_cci(void *cci_object)
{
	int ret = 0;
	struct cci_data *data = (struct cci_data *)cci_object;
	int i = 0;
	struct gpio *gpio_tbl = NULL;
	uint8_t gpio_tbl_size = 0;

	vl53l0_dbgmsg("Enter\n");
	if (data->power_up) {
		/* need to release cci first */
		ret = data->client->i2c_func_tbl->i2c_util(data->client,
							   MSM_CCI_RELEASE);
		if (ret < 0)
			vl53l0_errmsg("CCI Release failed rc %d\n", ret);

		/* actual power down */
		if (data->device_type == MSM_CAMERA_PLATFORM_DEVICE) {
			ret = stmvl53l0_vreg_control(data, 0);
			if (ret < 0) {
				vl53l0_errmsg
				    ("stmvl53l0_vreg_control failed %d\n",
				     __LINE__);
				return ret;
			}
		}

		/* reset GPIO pins */
		gpio_tbl = data->gconf->cam_gpio_req_tbl;
		gpio_tbl_size = data->gconf->cam_gpio_req_tbl_size;
		if (gpio_tbl_size > 0) {
			for (i = 0; i < gpio_tbl_size; i++)
				gpio_set_value_cansleep(gpio_tbl[i].gpio, 0);
			if (data->cam_pinctrl_status) {
				ret =
				    pinctrl_select_state(data->pinctrl_info.
							 pinctrl,
							 data->pinctrl_info.
							 gpio_state_suspend);
				if (ret < 0) {
					vl53l0_errmsg(
							"Error setting gpio pin to supsend state!\n");
				}

				devm_pinctrl_put(data->pinctrl_info.pinctrl);
				data->cam_pinctrl_status = 0;
				gpio_free_array(gpio_tbl, gpio_tbl_size);
			}
		}
	}
	data->power_up = 0;
	vl53l0_dbgmsg("End\n");
	return ret;
}

int stmvl53l0_cci_power_status(void *cci_object)
{
	struct cci_data *data = (struct cci_data *)cci_object;

	return data->power_up;
}

int stmvl53l0_init_cci(void)
{
	int ret = 0;

	vl53l0_dbgmsg("Enter\n");

	/* register as a platform device */
	ret = platform_driver_register(&stmvl53l0_platform_driver);
	if (ret)
		vl53l0_errmsg("%d, error ret:%d\n", __LINE__, ret);

	vl53l0_dbgmsg("End\n");

	return ret;
}

void stmvl53l0_exit_cci(void *cci_object)
{
	struct cci_data *data = (struct cci_data *)cci_object;

	vl53l0_dbgmsg("Enter\n");

	if (data && data->client->cci_client)
		kfree(data->client->cci_client);

	vl53l0_dbgmsg("End\n");
}
#endif				/* end of CAMERA_CCI */
