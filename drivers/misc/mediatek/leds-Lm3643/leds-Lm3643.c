/* 
* drivers/leds/leds-lm3643.c 
* General device driver for TI LM3643, FLASH LED Driver 
* 
* Copyright (C) 2014 Texas Instruments 
* Copyright (C) 2018 XiaoMi, Inc.
* 
* Contact: Daniel Jeong <gshark.jeong@gmail.com> 
*			Ldd-Mlp <ldd-mlp@list.ti.com> 
* 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * version 2 as published by the Free Software Foundation. 
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * General Public License for more details. 
 */ 
 
#include <linux/module.h> 
#include <linux/i2c.h> 
#include <linux/gpio.h> 
#include <linux/leds.h> 
#include <linux/slab.h> 
#include <linux/platform_device.h> 
#include <linux/fs.h> 
#include <linux/regmap.h> 
#include <linux/workqueue.h> 
#include <linux/delay.h>
//#include <linux/platform_data/leds-lm3643.h> 
#include <mach/mt_gpio.h> 
#include <linux/dev_info.h>
/* registers definitions */ 
#define REG_ENABLE		0x01 
#define REG_FLASH_LED0_BR	0x03 
#define REG_FLASH_LED1_BR	0x04 
#define REG_TORCH_LED0_BR	0x05 
#define REG_TORCH_LED1_BR	0x06 
#define REG_FLASH_TOUT		0x08 
#define REG_FLAG0		0x0a 
#define REG_FLAG1		0x0b 
 
enum lm3643_devid { 
	ID_FLASH0 = 0x0, 
	ID_FLASH1, 
	ID_TORCH0, 
	ID_TORCH1, 
	ID_MAX 
}; 
 
enum lm3643_mode { 
	MODE_STDBY = 0x0, 
	MODE_IR, 
	MODE_TORCH, 
	MODE_FLASH, 
	MODE_MAX 
}; 
 
enum lm3643_devfile { 
	DFILE_FLASH0_ENABLE = 0, 
	DFILE_FLASH0_ONOFF, 
	DFILE_FLASH0_SOURCE, 
	DFILE_FLASH0_TIMEOUT, 
	DFILE_FLASH1_ENABLE, 
	DFILE_FLASH1_ONOFF, 
	DFILE_TORCH0_ENABLE, 
	DFILE_TORCH0_ONOFF, 
	DFILE_TORCH0_SOURCE, 
	DFILE_TORCH1_ENABLE, 
	DFILE_TORCH1_ONOFF, 
	DFILE_MAX 
}; 
 
#define to_lm3643(_ctrl, _no) container_of(_ctrl, struct lm3643, cdev[_no]) 
#define LM3643_NAME "lm3643"

static struct i2c_board_info __initdata lm3643_dev={ I2C_BOARD_INFO(LM3643_NAME, 0x63)};

struct lm3643 { 
	struct device *dev; 
 
	u8 brightness[ID_MAX]; 
	struct work_struct work[ID_MAX]; 
	struct led_classdev cdev[ID_MAX]; 
 
	struct lm3643_platform_data *pdata; 
	struct regmap *regmap; 
	struct mutex lock; 
}; 
 
static void lm3643_read_flag(struct lm3643 *pchip) 
{ 
 
	int rval; 
	unsigned int flag0, flag1; 
 
	rval = regmap_read(pchip->regmap, REG_FLAG0, &flag0); 
	rval |= regmap_read(pchip->regmap, REG_FLAG1, &flag1); 
 
	if (rval < 0) 
		dev_err(pchip->dev, "i2c access fail.\n"); 
 
	dev_info(pchip->dev, "[flag1] 0x%x, [flag0] 0x%x\n", 
		 flag1 & 0x1f, flag0); 
} 
 
/* torch0 brightness control */ 
static void lm3643_deferred_torch0_brightness_set(struct work_struct *work) 
{ 
	struct lm3643 *pchip = container_of(work, 
					    struct lm3643, work[ID_TORCH0]); 
 
	if (regmap_update_bits(pchip->regmap, 
			       REG_TORCH_LED0_BR, 0x7f, 
			       pchip->brightness[ID_TORCH0])) 
		dev_err(pchip->dev, "i2c access fail.\n"); 
	lm3643_read_flag(pchip); 
} 
 
static void lm3643_torch0_brightness_set(struct led_classdev *cdev, 
					 enum led_brightness brightness) 
{ 
	struct lm3643 *pchip = 
	    container_of(cdev, struct lm3643, cdev[ID_TORCH0]); 
 
	pchip->brightness[ID_TORCH0] = brightness; 
	schedule_work(&pchip->work[ID_TORCH0]); 
} 
 
/* torch1 brightness control */ 
static void lm3643_deferred_torch1_brightness_set(struct work_struct *work) 
{ 
	struct lm3643 *pchip = container_of(work, 
					    struct lm3643, work[ID_TORCH1]); 
 
	if (regmap_update_bits(pchip->regmap, 
			       REG_TORCH_LED1_BR, 0x7f, 
			       pchip->brightness[ID_TORCH1])) 
		dev_err(pchip->dev, "i2c access fail.\n"); 
	lm3643_read_flag(pchip); 
} 
 
static void lm3643_torch1_brightness_set(struct led_classdev *cdev, 
					 enum led_brightness brightness) 
{ 
	struct lm3643 *pchip = 
	    container_of(cdev, struct lm3643, cdev[ID_TORCH1]); 
 
	pchip->brightness[ID_TORCH1] = brightness; 
	schedule_work(&pchip->work[ID_TORCH1]); 
} 
 
/* flash0 brightness control */ 
static void lm3643_deferred_flash0_brightness_set(struct work_struct *work) 
{ 
	struct lm3643 *pchip = container_of(work, 
					    struct lm3643, work[ID_FLASH0]); 
 
	if (regmap_update_bits(pchip->regmap, 
			       REG_FLASH_LED0_BR, 0x7f, 
			       pchip->brightness[ID_FLASH0])) 
		dev_err(pchip->dev, "i2c access fail.\n"); 
	lm3643_read_flag(pchip); 
} 
 
static void lm3643_flash0_brightness_set(struct led_classdev *cdev, 
					 enum led_brightness brightness) 
{ 
	struct lm3643 *pchip = 
	    container_of(cdev, struct lm3643, cdev[ID_FLASH0]); 
 
	pchip->brightness[ID_FLASH0] = brightness; 
	schedule_work(&pchip->work[ID_FLASH0]); 
} 
 
/* flash1 brightness control */ 
static void lm3643_deferred_flash1_brightness_set(struct work_struct *work) 
{ 
	struct lm3643 *pchip = container_of(work, 
					    struct lm3643, work[ID_FLASH1]); 
 
	if (regmap_update_bits(pchip->regmap, 
			       REG_FLASH_LED1_BR, 0x7f, 
			       pchip->brightness[ID_FLASH1])) 
		dev_err(pchip->dev, "i2c access fail.\n"); 
	lm3643_read_flag(pchip); 
} 
 
static void lm3643_flash1_brightness_set(struct led_classdev *cdev, 
					 enum led_brightness brightness) 
{ 
	struct lm3643 *pchip = 
	    container_of(cdev, struct lm3643, cdev[ID_FLASH1]); 
 
	pchip->brightness[ID_FLASH1] = brightness; 
	schedule_work(&pchip->work[ID_FLASH1]); 
} 
 
struct lm3643_devices { 
	struct led_classdev cdev; 
	work_func_t func; 
}; 
 
static struct lm3643_devices lm3643_leds[ID_MAX] = { 
	[ID_FLASH0] = { 
		       .cdev.name = "flash0", 
		       .cdev.brightness = 0, 
		       .cdev.max_brightness = 0x7f, 
		       .cdev.brightness_set = lm3643_flash0_brightness_set, 
		       .cdev.default_trigger = "flash0", 
		       .func = lm3643_deferred_flash0_brightness_set}, 
	[ID_FLASH1] = { 
		       .cdev.name = "flash1", 
		       .cdev.brightness = 0, 
		       .cdev.max_brightness = 0x7f, 
		       .cdev.brightness_set = lm3643_flash1_brightness_set, 
		       .cdev.default_trigger = "flash1", 
		       .func = lm3643_deferred_flash1_brightness_set}, 
	[ID_TORCH0] = { 
		       .cdev.name = "torch0", 
		       .cdev.brightness = 0, 
		       .cdev.max_brightness = 0x7f, 
		       .cdev.brightness_set = lm3643_torch0_brightness_set, 
		       .cdev.default_trigger = "torch0", 
		       .func = lm3643_deferred_torch0_brightness_set}, 
	[ID_TORCH1] = { 
		       .cdev.name = "torch1", 
		       .cdev.brightness = 0, 
		       .cdev.max_brightness = 0x7f, 
		       .cdev.brightness_set = lm3643_torch1_brightness_set, 
		       .cdev.default_trigger = "torch1", 
		       .func = lm3643_deferred_torch1_brightness_set}, 
}; 
 
static void lm3643_led_unregister(struct lm3643 *pchip, enum lm3643_devid id) 
{ 
	int icnt; 
 
	for (icnt = id; icnt > 0; icnt--) 
		led_classdev_unregister(&pchip->cdev[icnt - 1]); 
} 
 
static int lm3643_led_register(struct lm3643 *pchip) 
{ 
	int icnt, rval; 
 
	for (icnt = 0; icnt < ID_MAX; icnt++) { 
		INIT_WORK(&pchip->work[icnt], lm3643_leds[icnt].func); 
		pchip->cdev[icnt].name = lm3643_leds[icnt].cdev.name; 
		pchip->cdev[icnt].max_brightness = 
		    lm3643_leds[icnt].cdev.max_brightness; 
		pchip->cdev[icnt].brightness = 
		    lm3643_leds[icnt].cdev.brightness; 
		pchip->cdev[icnt].brightness_set = 
		    lm3643_leds[icnt].cdev.brightness_set; 
		pchip->cdev[icnt].default_trigger = 
		    lm3643_leds[icnt].cdev.default_trigger; 
		rval = led_classdev_register((struct device *) 
					     pchip->dev, &pchip->cdev[icnt]); 
		if (rval < 0) { 
			lm3643_led_unregister(pchip, icnt); 
			return rval; 
		} 
	} 
	return 0; 
} 
 
/* device files to control registers */ 
struct lm3643_commands { 
	char *str; 
	int size; 
}; 
 
enum lm3643_cmd_id { 
	CMD_ENABLE = 0, 
	CMD_DISABLE, 
	CMD_ON, 
	CMD_OFF, 
	CMD_IRMODE, 
	CMD_OVERRIDE, 
	CMD_MAX 
}; 
 
struct lm3643_commands cmds[CMD_MAX] = { 
	[CMD_ENABLE] = {"enable", 6}, 
	[CMD_DISABLE] = {"disable", 7}, 
	[CMD_ON] = {"on", 2}, 
	[CMD_OFF] = {"off", 3}, 
	[CMD_IRMODE] = {"irmode", 6}, 
	[CMD_OVERRIDE] = {"override", 8}, 
}; 
 
struct lm3643_files { 
	enum lm3643_devid id; 
	struct device_attribute attr; 
}; 
 
static size_t lm3643_ctrl(struct device *dev, 
			  const char *buf, enum lm3643_devid id, 
			  enum lm3643_devfile dfid, size_t size) 
{ 
	struct led_classdev *led_cdev = dev_get_drvdata(dev); 
	struct lm3643 *pchip = to_lm3643(led_cdev, id); 
	enum lm3643_cmd_id icnt; 
	int tout, rval; 
 
	mutex_lock(&pchip->lock); 
	for (icnt = 0; icnt < CMD_MAX; icnt++) { 
		if (strncmp(buf, cmds[icnt].str, cmds[icnt].size) == 0) 
			break; 
	} 
 
	switch (dfid) { 
		/* led 0 enable */ 
	case DFILE_FLASH0_ENABLE: 
	case DFILE_TORCH0_ENABLE: 
		if (icnt == CMD_ENABLE) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x1, 
					       0x1); 
		else if (icnt == CMD_DISABLE) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x1, 
					       0x0); 
		break; 
		/* led 1 enable, flash override */ 
	case DFILE_FLASH1_ENABLE: 
		if (icnt == CMD_ENABLE) { 
			rval = regmap_update_bits(pchip->regmap, 
						  REG_FLASH_LED0_BR, 0x80, 0x0); 
			rval |= 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
					       0x2); 
		} else if (icnt == CMD_DISABLE) { 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
					       0x0); 
		} else if (icnt == CMD_OVERRIDE) { 
			rval = regmap_update_bits(pchip->regmap, 
						  REG_FLASH_LED0_BR, 0x80, 
						  0x80); 
			rval |= 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
					       0x2); 
		} 
		break; 
		/* led 1 enable, torch override */ 
	case DFILE_TORCH1_ENABLE: 
		if (icnt == CMD_ENABLE) { 
			rval = regmap_update_bits(pchip->regmap, 
						  REG_TORCH_LED0_BR, 0x80, 0x0); 
			rval |= 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
					       0x2); 
		} else if (icnt == CMD_DISABLE) { 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
					       0x0); 
		} else if (icnt == CMD_OVERRIDE) { 
			rval = regmap_update_bits(pchip->regmap, 
						  REG_TORCH_LED0_BR, 0x80, 
						  0x80); 
			rval |= 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
					       0x2); 
		} 
		break; 
		/* mode control flash/ir */ 
	case DFILE_FLASH0_ONOFF: 
	case DFILE_FLASH1_ONOFF: 
		if (icnt == CMD_ON) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
					       0xc); 
		else if (icnt == CMD_OFF) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
					       0x0); 
		else if (icnt == CMD_IRMODE) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
					       0x4); 
		break; 
		/* mode control torch */ 
	case DFILE_TORCH0_ONOFF: 
	case DFILE_TORCH1_ONOFF: 
		if (icnt == CMD_ON) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
					       0x8); 
		else if (icnt == CMD_OFF) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
					       0x0); 
		break; 
		/* strobe pin control */ 
	case DFILE_FLASH0_SOURCE: 
		if (icnt == CMD_ON) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x20, 
					       0x20); 
		else if (icnt == CMD_OFF) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x20, 
					       0x0); 
		break; 
	case DFILE_TORCH0_SOURCE: 
		if (icnt == CMD_ON) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x10, 
					       0x10); 
		else if (icnt == CMD_OFF) 
			rval = 
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x10, 
					       0x0); 
		break; 
		/* flash time out */ 
	case DFILE_FLASH0_TIMEOUT: 
		rval = kstrtouint((const char *)buf, 10, &tout); 
		if (rval < 0) 
			break; 
		rval = regmap_update_bits(pchip->regmap, 
					  REG_FLASH_TOUT, 0x0f, tout); 
		break; 
	default: 
		dev_err(pchip->dev, "error : undefined dev file\n"); 
		break; 
	} 
	lm3643_read_flag(pchip); 
	mutex_unlock(&pchip->lock); 
	return size; 
} 
 
/* flash enable control */ 
static ssize_t lm3643_flash0_enable_store(struct device *dev, 
					  struct device_attribute *devAttr, 
					  const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_ENABLE, size); 
} 
 
static ssize_t lm3643_flash1_enable_store(struct device *dev, 
					  struct device_attribute *devAttr, 
					  const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_FLASH1, DFILE_FLASH1_ENABLE, size); 
} 
 
/* flash onoff control */ 
static ssize_t lm3643_flash0_onoff_store(struct device *dev, 
					 struct device_attribute *devAttr, 
					 const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_ONOFF, size); 
} 
 
static ssize_t lm3643_flash1_onoff_store(struct device *dev, 
					 struct device_attribute *devAttr, 
					 const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_FLASH1, DFILE_FLASH1_ONOFF, size); 
} 
 
/* flash timeout control */ 
static ssize_t lm3643_flash0_timeout_store(struct device *dev, 
					   struct device_attribute *devAttr, 
					   const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_TIMEOUT, size); 
} 
 
/* flash source control */ 
static ssize_t lm3643_flash0_source_store(struct device *dev, 
					  struct device_attribute *devAttr, 
					  const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_SOURCE, size); 
} 
 
/* torch enable control */ 
static ssize_t lm3643_torch0_enable_store(struct device *dev, 
					  struct device_attribute *devAttr, 
					  const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_TORCH0_ENABLE, size); 
} 
 
static ssize_t lm3643_torch1_enable_store(struct device *dev, 
					  struct device_attribute *devAttr, 
					  const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_TORCH1, DFILE_TORCH1_ENABLE, size); 
} 
 
/* torch onoff control */ 
static ssize_t lm3643_torch0_onoff_store(struct device *dev, 
					 struct device_attribute *devAttr, 
					 const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_TORCH0, DFILE_TORCH0_ONOFF, size); 
} 
 
static ssize_t lm3643_torch1_onoff_store(struct device *dev, 
					 struct device_attribute *devAttr, 
					 const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_TORCH1, DFILE_TORCH1_ONOFF, size); 
} 
 
/* torch source control */ 
static ssize_t lm3643_torch0_source_store(struct device *dev, 
					  struct device_attribute *devAttr, 
					  const char *buf, size_t size) 
{ 
	return lm3643_ctrl(dev, buf, ID_TORCH0, DFILE_TORCH0_SOURCE, size); 
} 



#define lm3643_attr(_name, _show, _store)\ 
{\ 
	.attr = {\ 
		.name = _name,\ 
		.mode = 0200,\ 
	},\ 
	.show = _show,\ 
	.store = _store,\ 
} 
 
static struct lm3643_files lm3643_devfiles[DFILE_MAX] = { 
	[DFILE_FLASH0_ENABLE] = { 
				 .id = ID_FLASH0, 
				 .attr = 
				 lm3643_attr("enable", NULL, 
					     lm3643_flash0_enable_store), 
				 }, 
	[DFILE_FLASH0_ONOFF] = { 
				.id = ID_FLASH0, 
				.attr = 
				lm3643_attr("onoff", NULL, 
					    lm3643_flash0_onoff_store), 
				}, 
	[DFILE_FLASH0_SOURCE] = { 
				 .id = ID_FLASH0, 
				 .attr = 
				 lm3643_attr("source", NULL, 
					     lm3643_flash0_source_store), 
				 }, 
	[DFILE_FLASH0_TIMEOUT] = { 
				  .id = ID_FLASH0, 
				  .attr = 
				  lm3643_attr("timeout", NULL, 
					      lm3643_flash0_timeout_store), 
				  }, 
	[DFILE_FLASH1_ENABLE] = { 
				 .id = ID_FLASH1, 
				 .attr = 
				 lm3643_attr("enable", NULL, 
					     lm3643_flash1_enable_store), 
				 }, 
	[DFILE_FLASH1_ONOFF] = { 
				.id = ID_FLASH1, 
				.attr = 
				lm3643_attr("onoff", NULL, 
					    lm3643_flash1_onoff_store), 
				}, 
	[DFILE_TORCH0_ENABLE] = { 
				 .id = ID_TORCH0, 
				 .attr = 
				 lm3643_attr("enable", NULL, 
					     lm3643_torch0_enable_store), 
				 }, 
	[DFILE_TORCH0_ONOFF] = { 
				.id = ID_TORCH0, 
				.attr = 
				lm3643_attr("onoff", NULL, 
					    lm3643_torch0_onoff_store), 
				}, 
	[DFILE_TORCH0_SOURCE] = { 
				 .id = ID_TORCH0, 
				 .attr = 
				 lm3643_attr("source", NULL, 
					     lm3643_torch0_source_store), 
				 }, 
	[DFILE_TORCH1_ENABLE] = { 
				 .id = ID_TORCH1, 
				 .attr = 
				 lm3643_attr("enable", NULL, 
					     lm3643_torch1_enable_store), 
				 }, 
	[DFILE_TORCH1_ONOFF] = { 
				.id = ID_TORCH1, 
				.attr = 
				lm3643_attr("onoff", NULL, 
					    lm3643_torch1_onoff_store), 
				} 
}; 
 
static void lm3643_df_remove(struct lm3643 *pchip, enum lm3643_devfile dfid) 
{ 
	enum lm3643_devfile icnt; 
 
	for (icnt = dfid; icnt > 0; icnt--) 
		device_remove_file(pchip->cdev[lm3643_devfiles[icnt - 1].id]. 
				   dev, &lm3643_devfiles[icnt - 1].attr); 
} 
 
static int lm3643_df_create(struct lm3643 *pchip) 
{ 
	enum lm3643_devfile icnt; 
	int rval; 
 
	for (icnt = 0; icnt < DFILE_MAX; icnt++) { 
		rval = 
		    device_create_file(pchip->cdev[lm3643_devfiles[icnt].id]. 
				       dev, &lm3643_devfiles[icnt].attr); 
		if (rval < 0) { 
			lm3643_df_remove(pchip, icnt); 
			return rval; 
		} 
	} 
	return 0; 
} 
 
static const struct regmap_config lm3643_regmap = { 
	.reg_bits = 8, 
	.val_bits = 8, 
	.max_register = 0xff, 
}; 


int iReadRegI2C_lm(struct i2c_client *client, u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId)
{
    int  i4RetValue = 0;

	//spin_lock(&kdsensor_drv_lock);
	client->addr = i2cId;
	client->ext_flag = (client->ext_flag)&(~I2C_DMA_FLAG);

	/* Remove i2c ack error log during search sensor */
	/* PK_ERR("client->ext_flag: %d", g_IsSearchSensor); */
	//if (g_IsSearchSensor == 1)
	 //   client->ext_flag = (client->ext_flag) | I2C_A_FILTER_MSG;
	//else
	    //client->ext_flag = (client->ext_flag)&(~I2C_A_FILTER_MSG);

	//spin_unlock(&kdsensor_drv_lock);
	/*  */
	i4RetValue = i2c_master_send(client, a_pSendData, a_sizeSendData);
	if (i4RetValue != a_sizeSendData) {
	    printk("[CAMERA SENSOR] I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
	    return -1;
	}

	i4RetValue = i2c_master_recv(client, (char *)a_pRecvData, a_sizeRecvData);
	if (i4RetValue != a_sizeRecvData) {
	    printk("[CAMERA SENSOR] I2C read failed!!\n");
	    return -1;
	}
    
    return 0;
} 

kal_uint16 read_data_lm3643(struct i2c_client *client, kal_uint8 addr)
{
    //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    kal_uint8 get_byte=0;
    char pusendcmd[2] = {addr & 0xFF, 0};
    iReadRegI2C_lm(client, pusendcmd , 1, &get_byte, 1, 0x63);
    return ((get_byte)&0x00ff);
}

EXPORT_SYMBOL(read_data_lm3643);

kal_uint16 write_data_lm3643(struct i2c_client *client, kal_uint8 addr, kal_uint8 data)
{
    //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    kal_uint8 get_byte=0;
    int  i4RetValue = 0;
    char pusendcmd[2] = {addr, data};
    client->addr = 0x63;
	client->ext_flag = (client->ext_flag)&(~I2C_DMA_FLAG);
	printk("<%s:%d>reg[%d][%d]\n", __FUNCTION__, __LINE__, addr, data);
	i4RetValue = i2c_master_send(client, pusendcmd, 2);
	
	// pusendcmd[0] = data;
	// i4RetValue = i2c_master_send(client, pusendcmd, 1);
	// if (i4RetValue != 1) {
	//     printk("[CAMERA SENSOR] I2C send failed!!, Addr = 0x%x\n", pusendcmd[0]);
	//     return -1;
	// }
    //iReadRegI2C_lm(client, pusendcmd , 1, &get_byte, 1, 0x63);
    return ((get_byte)&0x00ff);
}
EXPORT_SYMBOL(write_data_lm3643);
int g_is_ktd2684;
EXPORT_SYMBOL(g_is_ktd2684);
extern struct i2c_client *g_lm3643_i2c_client;

static ssize_t lm3643_mode_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	printk("<%s:%d>g_is_ktd2684[%d]\n", __func__, __LINE__, g_is_ktd2684);
	return (sprintf(buf, "%d\n", g_is_ktd2684));
}

static DEVICE_ATTR(mode, S_IRUGO, lm3643_mode_show, NULL);

static struct attribute *lm3643_debug_attrs[] = {
	&dev_attr_mode.attr,
	NULL
};

static const struct attribute_group lm3643_debug_attr_group = {
	.attrs = lm3643_debug_attrs,
	.name = "debug"
};


static int lm3643_probe(struct i2c_client *client, 
			const struct i2c_device_id *id) 
{ 
	struct lm3643 *pchip; 
	int rval; 
 	struct devinfo_struct *dev_led;
	/* i2c check */ 
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) { 
		dev_err(&client->dev, "i2c functionality check fail.\n"); 
		return -EOPNOTSUPP; 
	} 
 
	pchip = devm_kzalloc(&client->dev, sizeof(struct lm3643), GFP_KERNEL); 
	if (!pchip) 
		return -ENOMEM; 
 	mt_set_gpio_mode(GPIO_CAMERA_FLASH_EXT1_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CAMERA_FLASH_EXT1_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CAMERA_FLASH_EXT1_PIN, 1);
	mt_set_gpio_mode(GPIO_CAMERA_FLASH_EN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CAMERA_FLASH_EN_PIN, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO_CAMERA_FLASH_EN_PIN, 1);
	mt_set_gpio_mode(GPIO_CAMERA_FLASH_MODE_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CAMERA_FLASH_MODE_PIN, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO_CAMERA_FLASH_MODE_PIN, 1);
	pchip->dev = &client->dev; 
	g_lm3643_i2c_client = client;
	//printk("<%s:%d>data[%x][%x][%x]\n", __func__, __LINE__, read_data_lm3643(client, 0x1), read_data_lm3643(client, 0xA), read_data_lm3643(client, 0xc));
	//write_data_lm3643(client, 0x01, 0x8a);
	//mdelay(200);
	//printk("<%s:%d>data[%x][%x][%x]\n", __func__, __LINE__, read_data_lm3643(client, 0x1), read_data_lm3643(client, 0xA), read_data_lm3643(client, 0xc));
	//write_data_lm3643(client, 0x01, 0x8a);
	
	dev_led = (struct devinfo_struct*)kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);
    if (read_data_lm3643(client, 0xC) == 0x8)
	{
		g_is_ktd2684 = 1;
		
    	dev_led->device_vendor = "KTD";
    	dev_led->device_ic = "KTD2684";
	}
	else
	{
		g_is_ktd2684 = 0;
		//dev_led->device_type = "Fingerprint";
    	dev_led->device_vendor = "Ti";
    	dev_led->device_ic = "lm3644";
	}
    dev_led->device_type = "Flash_LED_DRIVER";
    dev_led->device_version = DEVINFO_NULL;
    dev_led->device_module = DEVINFO_NULL;
    dev_led->device_info = DEVINFO_NULL;
    //LOG_INF("<%s:%d>devinfo_add[%d]dev[%x]\n", __func__, __LINE__, devinfo_add, dev_ofilm);
	dev_led->device_used = DEVINFO_USED;
	DEVINFO_CHECK_ADD_DEVICE(dev_led);
	mdelay(200);
	//write_data_lm3643(client, 0x01, 0x80);
	//printk("<%s:%d>data[%x][%x][%x]\n", __func__, __LINE__, read_data_lm3643(client, 0x1), read_data_lm3643(client, 0xA), read_data_lm3643(client, 0xc));
	pchip->regmap = devm_regmap_init_i2c(client, &lm3643_regmap); 
	if (IS_ERR(pchip->regmap)) { 
		rval = PTR_ERR(pchip->regmap); 
		dev_err(&client->dev, "Failed to allocate register map: %d\n", 
			rval); 
		return rval; 
	} 
	mutex_init(&pchip->lock); 
	i2c_set_clientdata(client, pchip); 
 
 	rval = sysfs_create_group(&client->dev.kobj,&lm3643_debug_attr_group);
	if(rval){
			dev_err(&client->dev,"%s, Failed to create sysfs file: %d\n", __func__, rval);
			return rval;
	}
	// /* led class register */ 
	// rval = lm3643_led_register(pchip); 
	// if (rval < 0) 
	// 	return rval; 
 
	//  create dev files  
	// rval = lm3643_df_create(pchip); 
	// if (rval < 0) { 
	// 	lm3643_led_unregister(pchip, ID_MAX); 
	// 	return rval; 
	// } 
 
	dev_info(pchip->dev, "lm3643 leds initialized\n"); 
	return 0; 
} 
 
static int lm3643_remove(struct i2c_client *client) 
{ 
	struct lm3643 *pchip = i2c_get_clientdata(client); 
 
	lm3643_df_remove(pchip, DFILE_MAX); 
	lm3643_led_unregister(pchip, ID_MAX); 
 
	return 0; 
} 
 
static const struct i2c_device_id lm3643_id[] = { 
	{LM3643_NAME, 0}, 
	{} 
}; 
 
//MODULE_DEVICE_TABLE(i2c, lm3643_id); 
 
static struct i2c_driver lm3643_i2c_driver = { 
	.driver = { 
		   .name = LM3643_NAME, 
		   .owner = THIS_MODULE, 
		   //.pm = NULL, 
		   }, 
	.probe = lm3643_probe, 
	.remove = lm3643_remove, 
	.id_table = lm3643_id, 
}; 
 
//module_i2c_driver(lm3643_i2c_driver); 
static struct platform_device lm3643_i2c_device = {
    .name = LM3643_NAME,
    .id = 0,
    .dev = {}
};

static int __init lm3643_i2C_init(void)
{
    i2c_register_board_info(3, &lm3643_dev, 1);


    if(i2c_add_driver(&lm3643_i2c_driver)){
        printk("Failed to register lm3643_i2c_driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit lm3643_i2C_exit(void)
{
    platform_driver_unregister(&lm3643_i2c_driver);
}

module_init(lm3643_i2C_init);
module_exit(lm3643_i2C_exit);

MODULE_DESCRIPTION("Texas Instruments Flash Lighting driver for LM3643"); 
MODULE_AUTHOR("Daniel Jeong <daniel.jeong@ti.com>"); 
MODULE_LICENSE("GPL v2"); 
