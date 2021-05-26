/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

#include "imgsensor_cfg_table.h"
#include "imgsensor_platform.h"
#include "smartldo.h"

#define LDO_CAMERA_STATUS "ldo_camera_status"
//#define LDO_CAMERA_DEBUG
#define WL2866_LDO_EN_REG  0x0E
#define WL2866_LDO_EN_MASK 0x0F

struct wl2866_data {
	struct mutex lock;
	struct i2c_client *client;
};

typedef enum cam_cell_id {
	WL2866_LDO_DVDD1,
	WL2866_LDO_DVDD2,
	WL2866_LDO_AVDD1,
	WL2866_LDO_AVDD2,
	WL2866_LDO_NUM,
} cam_cell_id_t;

typedef struct wl2866_ldo_ctrl {
	uint32_t status_bit;
	uint8_t  reg_addr;
	bool status;
} wl2866_ldo_ctrl_t;

wl2866_ldo_ctrl_t wl2866_map[WL2866_LDO_NUM] = {
	{ 0x00, 0x03, false}, //dvdd1
	{ 0x00, 0x04, false}, //dvdd2
	{ 0x00, 0x05, false}, //avdd1
	{ 0x00, 0x06, false}  //avdd2
};

typedef struct wl2866_ldo_pin_map {
	enum IMGSENSOR_SENSOR_IDX   sensor_idx;
	enum IMGSENSOR_HW_PIN       pin;
	enum cam_cell_id cell_id;
} wl2866_ldo_pin_map_t;

wl2866_ldo_pin_map_t wl2866_ldo_pin_map[] = {
	{ IMGSENSOR_SENSOR_IDX_MAIN, IMGSENSOR_HW_PIN_AVDD, WL2866_LDO_AVDD1},//2.8V
	{ IMGSENSOR_SENSOR_IDX_MAIN, IMGSENSOR_HW_PIN_DVDD, WL2866_LDO_DVDD1},//1.15V
	{ IMGSENSOR_SENSOR_IDX_SUB, IMGSENSOR_HW_PIN_DVDD, WL2866_LDO_DVDD2}, //1.2V
	{ IMGSENSOR_SENSOR_IDX_SUB, IMGSENSOR_HW_PIN_AVDD, WL2866_LDO_AVDD2}, //2.8V
	{ IMGSENSOR_SENSOR_IDX_MAIN2, IMGSENSOR_HW_PIN_AVDD, WL2866_LDO_AVDD2}, //2.8V, Depth
	{ IMGSENSOR_SENSOR_IDX_SUB2, IMGSENSOR_HW_PIN_AVDD, WL2866_LDO_AVDD2}, //2.8V, Macro
};

typedef struct {
	enum IMGSENSOR_HW_PIN_STATE pin_state;
	uint32_t votage;
} wl2866_ldo_votage_map_t;

wl2866_ldo_votage_map_t wl2866_ldo_avdd_votage_map[] = {
	{ IMGSENSOR_HW_PIN_STATE_LEVEL_2800, 0x80},

};

wl2866_ldo_votage_map_t wl2866_ldo_dvdd_votage_map[] = {
	{ IMGSENSOR_HW_PIN_STATE_LEVEL_1150, 0x5C},
	{ IMGSENSOR_HW_PIN_STATE_LEVEL_1200, 0x64},

};

static struct wl2866_data *wl2866_data;

static int cam_smart_ldo_wl2866_control(enum IMGSENSOR_SENSOR_IDX   sensor_idx, cam_cell_id_t ldo_id, uint32_t value, bool enable);


static struct SMARTLDO ldo_instance;


static enum IMGSENSOR_RETURN smartldo_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN smartldo_release(void *pinstance)
{
	return IMGSENSOR_RETURN_SUCCESS;
}


static enum IMGSENSOR_RETURN smartldo_dump(void *pinstance)
{
	return IMGSENSOR_RETURN_SUCCESS;
}


static cam_cell_id_t smartldo_get_map_ldo_id(enum IMGSENSOR_SENSOR_IDX   sensor_idx, enum IMGSENSOR_HW_PIN       pin)
{
	uint8_t i = 0;
	uint8_t size = sizeof(wl2866_ldo_pin_map)/sizeof(wl2866_ldo_pin_map_t);

	for (i = 0; i < size; i++) {
		if ((sensor_idx == wl2866_ldo_pin_map[i].sensor_idx) && (pin == wl2866_ldo_pin_map[i].pin))
			return wl2866_ldo_pin_map[i].cell_id;
	}

	return WL2866_LDO_NUM;
}



static int32_t smartldo_get_ldo_value(enum IMGSENSOR_HW_PIN_STATE pin_state, cam_cell_id_t cell_id)
{
	uint8_t i = 0;
	uint8_t size = 0;
	wl2866_ldo_votage_map_t *votage_map = NULL;


	if (cell_id == WL2866_LDO_DVDD1 ||
		cell_id == WL2866_LDO_DVDD2) {
			votage_map = wl2866_ldo_dvdd_votage_map;
			size = ARRAY_SIZE(wl2866_ldo_dvdd_votage_map);
	} else if (cell_id == WL2866_LDO_AVDD1 ||
		cell_id == WL2866_LDO_AVDD2) {
			votage_map = wl2866_ldo_avdd_votage_map;
			size = ARRAY_SIZE(wl2866_ldo_avdd_votage_map);
	} else {
		printk("%s:error cell_id = %d", __func__, cell_id);
		return -1;
	}

	for (i = 0; i < size; i++) {
		if (pin_state == votage_map[i].pin_state)
			return votage_map[i].votage;
	}

	return -1;
}


static enum IMGSENSOR_RETURN smartldo_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	cam_cell_id_t ldo_id = WL2866_LDO_NUM;
	uint32_t value = 0;
	bool enable = false;
	enum IMGSENSOR_RETURN ret = IMGSENSOR_RETURN_ERROR;


	if (pin > IMGSENSOR_HW_PIN_DOVDD   ||
	    pin < IMGSENSOR_HW_PIN_AVDD    ||
	    pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	    pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_1150 ||
	    sensor_idx < 0)
		return ret;

	if (wl2866_data == NULL || wl2866_data->client == NULL)
		return ret;
	//mutex
	mutex_lock(&wl2866_data->lock);
	ldo_id = smartldo_get_map_ldo_id(sensor_idx, pin);
	if (ldo_id == WL2866_LDO_NUM) {
		printk("%s: get ldo id failed\n", __func__);
		goto exit;
	} else
		printk("%s: ldo id = %d", __func__, ldo_id);

	if (pin_state == IMGSENSOR_HW_PIN_STATE_LEVEL_0)
		enable = false;
	else {
		enable = true;
		value = smartldo_get_ldo_value(pin_state, ldo_id);
		if (value < 0) {
			printk("%s: get ldo votage failed\n", __func__);
			goto exit;
		} else
			printk("%s: votage value = 0x%xH", __func__, value);
	}

	if (cam_smart_ldo_wl2866_control(sensor_idx, ldo_id, value, enable) == 0)
		ret = IMGSENSOR_RETURN_SUCCESS;

exit:
	//release mutex
	mutex_unlock(&wl2866_data->lock);
	return ret;
}



static struct IMGSENSOR_HW_DEVICE device = {
	.id        = IMGSENSOR_HW_ID_SMARTLDO,
	.pinstance = (void *)&ldo_instance,
	.init      = smartldo_init,
	.set       = smartldo_set,
	.release   = smartldo_release,
	.dump      = smartldo_dump
};

enum IMGSENSOR_RETURN imgsensor_hw_smartldo_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}


//static uint16_t s_enable_reg_state = 0;
static int wl2866_ldo_enable(uint16_t ldo_id, bool enable)
{
	uint16_t val = 0;
	int ret = -1;

	if (wl2866_data == NULL || wl2866_data->client == NULL)
		return -1;
	val = i2c_smbus_read_byte_data(wl2866_data->client, WL2866_LDO_EN_REG);

	if (enable)
		val |= (1<<ldo_id);
	else
		val &= ~(1<<ldo_id);
	printk("%s: need to set enable value = %d\n", __func__, val);
	ret = i2c_smbus_write_byte_data(wl2866_data->client, WL2866_LDO_EN_REG, val);
	if (ret == 0) {
		val = i2c_smbus_read_byte_data(wl2866_data->client, WL2866_LDO_EN_REG);
		printk("%s: read enable value = %d\n", __func__, val);
	}
	return ret;
}


static int wl2866_ldo_set_value(cam_cell_id_t ldo_id, uint32_t value)
{
	int ret = -1;

	if (wl2866_data == NULL || wl2866_data->client == NULL)
		return -1;
	printk("%s: need to write lod_id = %d, value = %d", __func__, ldo_id, value);
	ret = i2c_smbus_write_byte_data(wl2866_data->client, wl2866_map[ldo_id].reg_addr, value);
	if (ret == 0) {
		value = i2c_smbus_read_byte_data(wl2866_data->client, wl2866_map[ldo_id].reg_addr);
		printk("%s: read ldo_id =%d register value = %d\n", __func__, value);
	}
	return ret;
}

static int cam_smart_ldo_wl2866_control(enum IMGSENSOR_SENSOR_IDX sensor_idx, cam_cell_id_t ldo_id, uint32_t value, bool enable)
{
	int ret = 0;
	uint32_t status_bit = 0;

	printk("%s: entry\n", __func__);
	if (ldo_id >= WL2866_LDO_NUM) {
		printk("%s: error ldo_id = %d\n", __func__, ldo_id);
		return -EFAULT;
	}

	if (enable) {
		status_bit = wl2866_map[ldo_id].status_bit | (1<<sensor_idx);
		if (wl2866_map[ldo_id].status == enable) {
			wl2866_map[ldo_id].status_bit = status_bit;
			printk("%s: no need to enable pin=%d again", __func__, ldo_id);
			return 0;
		}
	} else {
		status_bit = wl2866_map[ldo_id].status_bit & (~(1<<sensor_idx));
		if (status_bit > 0) {
			wl2866_map[ldo_id].status_bit = status_bit;
			printk("%s: no need to disable pin=%d again", __func__, ldo_id);
			return 0;
		}
	}
	if (enable)
		ret = wl2866_ldo_set_value(ldo_id, value);

	if (ret < 0) {
		printk("%s: wl2866_ldo_set_value is fail\n", __func__);
		return ret;
	}
	ret = wl2866_ldo_enable(ldo_id, enable);
	if (ret != 0)
		printk("%s: wl2866_ldo_enable is fail, ret = %d\n", __func__, ret);
	else {
		wl2866_map[ldo_id].status = enable;
		wl2866_map[ldo_id].status_bit = status_bit;
		printk("%s: wl2866_ldo_enable is success\n", __func__);
	}
	return ret;

}

#define WRITE_BUFF_SIZE 32


// MAIN AVDD -> enable: 0 3 10   disable: 0 3 0
// MAIN DVDD -> enable: 0 4 3   disable: 0 4 0
static ssize_t ldo_camera_proc_write(struct file *file, const char __user *buf,
			     size_t len, loff_t *ppos)
{
	int res = 0;
	char write_buf[WRITE_BUFF_SIZE] = { 0 };
	enum IMGSENSOR_SENSOR_IDX   sensor_idx;
	enum IMGSENSOR_HW_PIN       pin;
	enum IMGSENSOR_HW_PIN_STATE pin_state;

	if (len >= WRITE_BUFF_SIZE) {
		return -EFAULT;
	}
	if (copy_from_user(write_buf, buf, len)) {
		return -EFAULT;
	}

	res = sscanf(write_buf, "%d %d %d", &sensor_idx, &pin, &pin_state);

	printk("%s:res=%d, sensor_idx=%d,pin=%d,pin_state=%d\n", __func__, res, sensor_idx, pin, pin_state);

	if (res != 3) {
		return -EINVAL;
	}

	if (wl2866_data == NULL || wl2866_data->client == NULL)
		return -EINVAL;

	res = smartldo_set(NULL, sensor_idx, pin, pin_state);
	if (res != IMGSENSOR_RETURN_SUCCESS)
		printk("%s:write ldo failed\n", __func__);
	else
		printk("%s:write ldo success\n", __func__);


	return len;
}



static int ldo_camera_proc_show(struct seq_file *file, void *data)
{
	int i = 0;

	if (wl2866_data == NULL || wl2866_data->client == NULL) {
		seq_puts(file, "wl2866_client_p is NULL,please check device\n");
		return -EINVAL;
	}

	for (i = 0; i < 16; i++) {
		seq_printf(file, "reg[%d]=0x%x\n", i, i2c_smbus_read_byte_data(wl2866_data->client, i));
	}

	return 0;
}

static int ldo_camera_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ldo_camera_proc_show, inode->i_private);
}

static const struct file_operations ldo_camera_proc_fops = {
	.open = ldo_camera_proc_open,
	.read = seq_read,
	.write = ldo_camera_proc_write,
	.release	= single_release,
	.llseek		= seq_lseek,
};


static int wl2866_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	int i = 0, value = 0, ret = 0;

#ifdef LDO_CAMERA_DEBUG
	struct proc_dir_entry *ldo_status = NULL;
#endif

	printk("%s: prob enter", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_BYTE_DATA
				     | I2C_FUNC_SMBUS_READ_BYTE)) {
		printk("%s: i2c_check_functionality error", __func__);
		return -ENODEV;
	}


	wl2866_data = kzalloc(sizeof(struct wl2866_data), GFP_KERNEL);
	if (!wl2866_data) {
		return -ENOMEM;
		printk("%s: kzalloc error", __func__);
	}
	printk("%s: kzalloc success", __func__);
	/* Init real i2c_client */
	i2c_set_clientdata(client, wl2866_data);
	mutex_init(&wl2866_data->lock);


	wl2866_data->client = client;
	ret = i2c_smbus_read_byte_data(client, 0x00);
	if (ret < 0) {
		printk("[%s] error: read chip rev failed\n", __func__);
		goto free_mem;
	} else{
		printk("[%s] read chip rev success\n", __func__);
	}

	/*
		 for  1 supplier of smart ldo,you need soft reset register,
		otherwise may cause smart ldo power on fail
	*/
	value = 0;
	for (i = 1; i <= 0x0F; i++) {
		ret = i2c_smbus_write_byte_data(wl2866_data->client, i, value);
		if (ret != 0) {
			printk("%s: write value(%x) error, ret = %d\n", __func__, value, ret);
		}
		ret = i2c_smbus_read_byte_data(wl2866_data->client, i);
		printk("%s: read register[%x] = %x\n", __func__, i, ret);
	}

#ifdef LDO_CAMERA_DEBUG
	ldo_status = proc_create(LDO_CAMERA_STATUS, 0644, NULL, &ldo_camera_proc_fops);
	if (ldo_status == NULL) {
		printk("[%s] error: create_proc_entry ldo_status failed\n", __func__);
		goto free_mem;
	}
#endif

	return 0;
free_mem:
	if (wl2866_data) {
		kfree(wl2866_data);
		wl2866_data = NULL;
	}
	return -ENODEV;
}

#ifdef CONFIG_OF
static const struct of_device_id wl2866_of_match[] = {
	{ .compatible = "mediatek,camera_smart_ldo" },
	{}
};
MODULE_DEVICE_TABLE(of, wl2866_of_match);
#endif

static const struct i2c_device_id wl2866_i2c_id[] = {
	{ "wl2866", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, wl2866_i2c_id);

static struct i2c_driver wl2866_driver = {
	.driver = {
		.name	= "ldo_wl2866",
		.of_match_table = of_match_ptr(wl2866_of_match),
	},
	.probe = wl2866_probe,
	.id_table = wl2866_i2c_id,
};
module_i2c_driver(wl2866_driver);

MODULE_DESCRIPTION("WL 2866D I2C LDO Driver");
MODULE_AUTHOR("huabinchen@huaqin.com");
MODULE_LICENSE("GPL");



