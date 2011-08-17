/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include "mt9d113.h"

/* Micron MT9D113 Registers and their values */
#define  REG_MT9D113_MODEL_ID	0x0000
#define  MT9D113_MODEL_ID		0x2580
#define Q8						0x00000100

struct mt9d113_work {
	struct work_struct work;
};

static struct  mt9d113_work *mt9d113_sensorw;
static struct  i2c_client *mt9d113_client;

struct mt9d113_ctrl {
	const struct msm_camera_sensor_info *sensordata;
	uint32_t sensormode;
	uint32_t fps_divider;/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;/* init to 1 * 0x00000400 */
	uint16_t fps;
	uint16_t curr_step_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint16_t total_lines_per_frame;
	uint16_t config_csi;
	enum mt9d113_resolution_t prev_res;
	enum mt9d113_resolution_t pict_res;
	enum mt9d113_resolution_t curr_res;
	enum mt9d113_test_mode_t  set_test;
};

static struct mt9d113_ctrl *mt9d113_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(mt9d113_wait_queue);
DEFINE_MUTEX(mt9d113_mut);

static int mt9d113_i2c_rxdata(unsigned short saddr,
				unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr   = saddr,
			.flags = 0,
			.len   = 2,
			.buf   = rxdata,
		},
		{
			.addr   = saddr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxdata,
		},
	};
	if (i2c_transfer(mt9d113_client->adapter, msgs, 2) < 0) {
		CDBG("mt9d113_i2c_rxdata failed!\n");
		return -EIO;
	}
	return 0;
}

static int32_t mt9d113_i2c_read(unsigned short   saddr,
				unsigned short raddr,
				unsigned short *rdata,
				enum mt9d113_width width)
{
	int32_t rc = 0;
	unsigned char buf[4];
	if (!rdata)
		return -EIO;
	memset(buf, 0, sizeof(buf));
	switch (width) {
	case WORD_LEN: {
			buf[0] = (raddr & 0xFF00)>>8;
			buf[1] = (raddr & 0x00FF);
			rc = mt9d113_i2c_rxdata(saddr, buf, 2);
			if (rc < 0)
				return rc;
			*rdata = buf[0] << 8 | buf[1];
		}
		break;
	default:
		break;
	}
	if (rc < 0)
		CDBG("mt9d113_i2c_read failed !\n");
	return rc;
}

static int32_t mt9d113_i2c_txdata(unsigned short saddr,
				unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};
	if (i2c_transfer(mt9d113_client->adapter, msg, 1) < 0) {
		CDBG("mt9d113_i2c_txdata failed\n");
		return -EIO;
	}
	return 0;
}

static int32_t mt9d113_i2c_write(unsigned short saddr,
				unsigned short waddr,
				unsigned short wdata,
				enum mt9d113_width width)
{
	int32_t rc = -EIO;
	unsigned char buf[4];
	memset(buf, 0, sizeof(buf));
	switch (width) {
	case WORD_LEN: {
			buf[0] = (waddr & 0xFF00)>>8;
			buf[1] = (waddr & 0x00FF);
			buf[2] = (wdata & 0xFF00)>>8;
			buf[3] = (wdata & 0x00FF);
			rc = mt9d113_i2c_txdata(saddr, buf, 4);
		}
		break;
	case BYTE_LEN: {
			buf[0] = waddr;
			buf[1] = wdata;
			rc = mt9d113_i2c_txdata(saddr, buf, 2);
		}
		break;
	default:
		break;
	}
	if (rc < 0)
		printk(KERN_ERR
			"i2c_write failed, addr = 0x%x, val = 0x%x!\n",
			waddr, wdata);
	return rc;
}

static int32_t mt9d113_i2c_write_table(
				struct mt9d113_i2c_reg_conf
				const *reg_conf_tbl,
				int num_of_items_in_table)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9d113_i2c_write(mt9d113_client->addr,
				reg_conf_tbl->waddr, reg_conf_tbl->wdata,
				WORD_LEN);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

static long mt9d113_reg_init(void)
{
	uint16_t data = 0;
	int32_t rc = 0;
	int count = 0;
	struct msm_camera_csi_params mt9d113_csi_params;
	if (!mt9d113_ctrl->config_csi) {
		mt9d113_csi_params.lane_cnt = 1;
		mt9d113_csi_params.data_format = CSI_8BIT;
		mt9d113_csi_params.lane_assign = 0xe4;
		mt9d113_csi_params.dpcm_scheme = 0;
		mt9d113_csi_params.settle_cnt = 0x14;
		rc = msm_camio_csi_config(&mt9d113_csi_params);
		mt9d113_ctrl->config_csi = 1;
		msleep(50);
	}
	/* Disable parallel and enable mipi*/
	rc = mt9d113_i2c_write(mt9d113_client->addr,
				0x001A,
				0x0051, WORD_LEN);
	rc = mt9d113_i2c_write(mt9d113_client->addr,
				0x001A,
				0x0050,
				WORD_LEN);
	msleep(20);
	rc = mt9d113_i2c_write(mt9d113_client->addr,
				0x001A,
				0x0058,
				WORD_LEN);

	/* Preset pll settings begin*/
	rc = mt9d113_i2c_write_table(&mt9d113_regs.pll_tbl[0],
				mt9d113_regs.pll_tbl_size);
	if (rc < 0)
		return rc;
	rc = mt9d113_i2c_read(mt9d113_client->addr,
				0x0014, &data, WORD_LEN);
	data = data&0x8000;
	/* Poll*/
	while (data == 0x0000) {
		data = 0;
		rc = mt9d113_i2c_read(mt9d113_client->addr,
				0x0014, &data, WORD_LEN);
		data = data & 0x8000;
		usleep_range(11000, 12000);
		count++;
		if (count == 100) {
			CDBG(" Timeout:1\n");
			break;
		}
	}
	rc = mt9d113_i2c_write(mt9d113_client->addr,
				0x0014,
				0x20FA,
				WORD_LEN);

	/*Preset pll Ends*/
	mt9d113_i2c_write(mt9d113_client->addr,
				0x0018,
				0x402D,
				WORD_LEN);

	mt9d113_i2c_write(mt9d113_client->addr,
				0x0018,
				0x402C,
				WORD_LEN);
	/*POLL_REG=0x0018,0x4000,!=0x0000,DELAY=10,TIMEOUT=100*/
	data = 0;
	rc = mt9d113_i2c_read(mt9d113_client->addr,
		0x0018, &data, WORD_LEN);
	data = data & 0x4000;
	count = 0;
	while (data != 0x0000) {
		rc = mt9d113_i2c_read(mt9d113_client->addr,
			0x0018, &data, WORD_LEN);
		data = data & 0x4000;
		CDBG(" data is %d\n" , data);
		usleep_range(11000, 12000);
		count++;
		if (count == 100) {
			CDBG(" Loop2 timeout: MT9D113\n");
			break;
		}
		CDBG(" Not streaming\n");
	}
	CDBG("MT9D113: Start stream\n");
	/*Preset Register Wizard Conf*/
	rc = mt9d113_i2c_write_table(&mt9d113_regs.register_tbl[0],
				mt9d113_regs.register_tbl_size);
	if (rc < 0)
		return rc;
	rc = mt9d113_i2c_write_table(&mt9d113_regs.err_tbl[0],
				mt9d113_regs.err_tbl_size);
	if (rc < 0)
		return rc;
	rc = mt9d113_i2c_write_table(&mt9d113_regs.eeprom_tbl[0],
				mt9d113_regs.eeprom_tbl_size);
	if (rc < 0)
		return rc;

	rc = mt9d113_i2c_write_table(&mt9d113_regs.low_light_tbl[0],
				mt9d113_regs.low_light_tbl_size);
	if (rc < 0)
		return rc;

	rc = mt9d113_i2c_write_table(&mt9d113_regs.awb_tbl[0],
				mt9d113_regs.awb_tbl_size);
	if (rc < 0)
		return rc;

	rc = mt9d113_i2c_write_table(&mt9d113_regs.patch_tbl[0],
				mt9d113_regs.patch_tbl_size);
	if (rc < 0)
		return rc;

	/*check patch load*/
	mt9d113_i2c_write(mt9d113_client->addr,
				0x098C,
				0xA024,
				WORD_LEN);
	count = 0;
	/*To check if patch is loaded properly
	poll the register 0x990 till the condition is
	met or till the timeout*/
	data = 0;
	rc = mt9d113_i2c_read(mt9d113_client->addr,
				0x0990, &data, WORD_LEN);
	while (data == 0) {
		data = 0;
		rc = mt9d113_i2c_read(mt9d113_client->addr,
				0x0990, &data, WORD_LEN);
		usleep_range(11000, 12000);
		count++;
		if (count == 100) {
			CDBG("Timeout in patch loading\n");
			break;
		}
	}
		/*BITFIELD=0x0018, 0x0004, 0*/
	/*Preset continue begin */
	rc = mt9d113_i2c_write(mt9d113_client->addr, 0x0018, 0x0028,
				WORD_LEN);
	CDBG(" mt9d113 wait for seq done\n");
	/* syncronize the FW with the sensor
	MCU_ADDRESS [SEQ_CMD]*/
	rc = mt9d113_i2c_write(mt9d113_client->addr,
				0x098C, 0xA103, WORD_LEN);
	rc = mt9d113_i2c_write(mt9d113_client->addr,
				0x0990, 0x0006, WORD_LEN);
		/*mt9d113 wait for seq done
	 syncronize the FW with the sensor */
	msleep(20);
	/*Preset continue end */
	CDBG(" MT9D113: Preset continue end\n");
	rc = mt9d113_i2c_write(mt9d113_client->addr,
				0x0012,
				0x00F5,
				WORD_LEN);
	/*continue begin */
	CDBG(" MT9D113: Preset continue begin\n");
	rc = mt9d113_i2c_write(mt9d113_client->addr, 0x0018, 0x0028 ,
				WORD_LEN);
	/*mt9d113 wait for seq done
	 syncronize the FW with the sensor
	MCU_ADDRESS [SEQ_CMD]*/
	msleep(20);
	rc = mt9d113_i2c_write(mt9d113_client->addr,
				0x098C, 0xA103, WORD_LEN);
	/* MCU DATA */
	rc = mt9d113_i2c_write(mt9d113_client->addr, 0x0990,
				0x0006, WORD_LEN);
	/*mt9d113 wait for seq done
	syncronize the FW with the sensor */
	/* MCU_ADDRESS [SEQ_CMD]*/
	msleep(20);
	/*Preset continue end*/
	return rc;

}

static long mt9d113_set_sensor_mode(int mode)
{
	long rc = 0;
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = mt9d113_reg_init();
		CDBG("MT9D113: configure to preview begin\n");
		rc =
		mt9d113_i2c_write(mt9d113_client->addr,
						0x098C, 0xA115, WORD_LEN);
		if (rc < 0)
			return rc;
		rc =
		mt9d113_i2c_write(mt9d113_client->addr,
						0x0990, 0x0000, WORD_LEN);
		if (rc < 0)
			return rc;
		rc =
		mt9d113_i2c_write(mt9d113_client->addr,
						0x098C, 0xA103, WORD_LEN);
		if (rc < 0)
			return rc;
		rc =
		mt9d113_i2c_write(mt9d113_client->addr,
						0x098C, 0x0001, WORD_LEN);
		if (rc < 0)
			return rc;
		break;
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		rc =
		mt9d113_i2c_write(mt9d113_client->addr,
						0x098C, 0xA115, WORD_LEN);
		rc =
		mt9d113_i2c_write(mt9d113_client->addr,
						0x098C, 0x0002, WORD_LEN);
		rc =
		mt9d113_i2c_write(mt9d113_client->addr,
						0x098C, 0xA103, WORD_LEN);
		rc =
		mt9d113_i2c_write(mt9d113_client->addr,
						0x098C, 0x0002, WORD_LEN);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int mt9d113_sensor_init_probe(const struct
				msm_camera_sensor_info * data)
{
	uint16_t model_id = 0;
	int rc = 0;
	/* Read the Model ID of the sensor */
	rc = mt9d113_i2c_read(mt9d113_client->addr,
						REG_MT9D113_MODEL_ID,
						&model_id, WORD_LEN);
	if (rc < 0)
		goto init_probe_fail;
	/* Check if it matches it with the value in Datasheet */
	if (model_id != MT9D113_MODEL_ID)
		printk(KERN_INFO "mt9d113 model_id = 0x%x\n", model_id);
	if (rc < 0)
		goto init_probe_fail;
	return rc;
init_probe_fail:
	printk(KERN_INFO "probe fail\n");
	return rc;
}

static int mt9d113_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9d113_wait_queue);
	return 0;
}

int mt9d113_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long rc = 0;

	if (copy_from_user(&cfg_data,
					(void *)argp,
					(sizeof(struct sensor_cfg_data))))
		return -EFAULT;
	mutex_lock(&mt9d113_mut);
	CDBG("mt9d113_ioctl, cfgtype = %d, mode = %d\n",
		 cfg_data.cfgtype, cfg_data.mode);
	switch (cfg_data.cfgtype) {
	case CFG_SET_MODE:
		rc = mt9d113_set_sensor_mode(
						cfg_data.mode);
		break;
	case CFG_SET_EFFECT:
		return rc;
	case CFG_GET_AF_MAX_STEPS:
	default:
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&mt9d113_mut);
	return rc;
}

int mt9d113_sensor_release(void)
{
	int rc = 0;

	mutex_lock(&mt9d113_mut);
	gpio_set_value_cansleep(mt9d113_ctrl->sensordata->sensor_reset, 0);
	msleep(20);
	gpio_free(mt9d113_ctrl->sensordata->sensor_reset);
	kfree(mt9d113_ctrl);
	mutex_unlock(&mt9d113_mut);

	return rc;
}

static int mt9d113_probe_init_done(const struct msm_camera_sensor_info
				*data)
{
	gpio_free(data->sensor_reset);
	return 0;
}

static int mt9d113_probe_init_sensor(const struct msm_camera_sensor_info
				*data)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = gpio_request(data->sensor_pwd, "mt9d113");
	if (!rc) {
		printk(KERN_INFO "sensor_reset = %d\n", rc);
		gpio_direction_output(data->sensor_pwd, 0);
		usleep_range(11000, 12000);
	} else {
		goto init_probe_done;
	}
	msleep(20);
	rc = gpio_request(data->sensor_reset, "mt9d113");
	printk(KERN_INFO " mt9d113_probe_init_sensor\n");
	if (!rc) {
		printk(KERN_INFO "sensor_reset = %d\n", rc);
		gpio_direction_output(data->sensor_reset, 0);
		usleep_range(11000, 12000);
		gpio_set_value_cansleep(data->sensor_reset, 1);
		usleep_range(11000, 12000);
	} else
		goto init_probe_done;
	printk(KERN_INFO " mt9d113_probe_init_sensor called\n");
	rc = mt9d113_i2c_read(mt9d113_client->addr, REG_MT9D113_MODEL_ID,
						&chipid, 2);
	if (rc < 0)
		goto init_probe_fail;
	/*Compare sensor ID to MT9D113 ID: */
	if (chipid != MT9D113_MODEL_ID) {
		printk(KERN_INFO "mt9d113_probe_init_sensor chip idis%d\n",
			chipid);
	}
	CDBG("mt9d113_probe_init_sensor Success\n");
	goto init_probe_done;
init_probe_fail:
	CDBG(" ov2720_probe_init_sensor fails\n");
	gpio_set_value_cansleep(data->sensor_reset, 0);
	mt9d113_probe_init_done(data);
init_probe_done:
	printk(KERN_INFO " mt9d113_probe_init_sensor finishes\n");
	return rc;
}

static int mt9d113_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}
	mt9d113_sensorw =
	kzalloc(sizeof(struct mt9d113_work), GFP_KERNEL);
	if (!mt9d113_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}
	i2c_set_clientdata(client, mt9d113_sensorw);
	mt9d113_init_client(client);
	mt9d113_client = client;
	CDBG("mt9d113_probe succeeded!\n");
	return 0;
probe_failure:
	kfree(mt9d113_sensorw);
	mt9d113_sensorw = NULL;
	CDBG("mt9d113_probe failed!\n");
	return rc;
}

static const struct i2c_device_id mt9d113_i2c_id[] = {
	{ "mt9d113", 0},
	{},
};

static struct i2c_driver mt9d113_i2c_driver = {
	.id_table = mt9d113_i2c_id,
	.probe  = mt9d113_i2c_probe,
	.remove = __exit_p(mt9d113_i2c_remove),
			  .driver = {
		.name = "mt9d113",
	},
};

int mt9d113_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	mt9d113_ctrl = kzalloc(sizeof(struct mt9d113_ctrl), GFP_KERNEL);
	if (!mt9d113_ctrl) {
		printk(KERN_INFO "mt9d113_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	mt9d113_ctrl->fps_divider = 1 * 0x00000400;
	mt9d113_ctrl->pict_fps_divider = 1 * 0x00000400;
	mt9d113_ctrl->set_test = TEST_OFF;
	mt9d113_ctrl->config_csi = 0;
	mt9d113_ctrl->prev_res = QTR_SIZE;
	mt9d113_ctrl->pict_res = FULL_SIZE;
	mt9d113_ctrl->curr_res = INVALID_SIZE;
	if (data)
		mt9d113_ctrl->sensordata = data;
	if (rc < 0) {
		printk(KERN_INFO "mt9d113_sensor_open_init fail\n");
		return rc;
	}
		/* enable mclk first */
		msm_camio_clk_rate_set(24000000);
		msleep(20);
		rc = mt9d113_probe_init_sensor(data);
		if (rc < 0)
			goto init_fail;
		mt9d113_ctrl->fps = 30*Q8;
		rc = mt9d113_sensor_init_probe(data);
		if (rc < 0) {
			gpio_set_value_cansleep(data->sensor_reset, 0);
			goto init_fail;
		} else
			printk(KERN_ERR "%s: %d\n", __func__, __LINE__);
		goto init_done;
init_fail:
		printk(KERN_INFO "init_fail\n");
		mt9d113_probe_init_done(data);
init_done:
		CDBG("init_done\n");
		return rc;
}

static int mt9d113_sensor_probe(const struct msm_camera_sensor_info
				*info,
				struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = i2c_add_driver(&mt9d113_i2c_driver);
	if (rc < 0 || mt9d113_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_fail;
	}
	msm_camio_clk_rate_set(24000000);
	usleep_range(5000, 6000);
	rc = mt9d113_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;
	s->s_init = mt9d113_sensor_open_init;
	s->s_release = mt9d113_sensor_release;
	s->s_config  = mt9d113_sensor_config;
	s->s_camera_type = FRONT_CAMERA_2D;
	s->s_mount_angle  = 0;
	gpio_set_value_cansleep(info->sensor_reset, 0);
	mt9d113_probe_init_done(info);
	return rc;
probe_fail:
	printk(KERN_INFO "mt9d113_sensor_probe: SENSOR PROBE FAILS!\n");
	return rc;
}

static int __mt9d113_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, mt9d113_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9d113_probe,
	.driver = {
		.name = "msm_camera_mt9d113",
		.owner = THIS_MODULE,
	},
};

static int __init mt9d113_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(mt9d113_init);

MODULE_DESCRIPTION("Micron 2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
