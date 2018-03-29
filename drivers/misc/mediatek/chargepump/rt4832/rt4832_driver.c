/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file rt4832.c
   brief This file contains all function implementations for the rt4832 in linux
   this source file refer to MT6572 platform
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <mach/gpio_const.h>
#include <mt_gpio.h>

#include <linux/platform_device.h>

#include <linux/leds.h>

#define LCD_LED_MAX 0x7F
#define LCD_LED_MIN 0

#define DEFAULT_BRIGHTNESS 0x73
#define RT4832_MIN_VALUE_SETTINGS 10	/* value leds_brightness_set */
#define RT4832_MAX_VALUE_SETTINGS 255	/* value leds_brightness_set */
#define MIN_MAX_SCALE(x) (((x) < RT4832_MIN_VALUE_SETTINGS) ? RT4832_MIN_VALUE_SETTINGS :\
(((x) > RT4832_MAX_VALUE_SETTINGS) ? RT4832_MAX_VALUE_SETTINGS:(x)))

#define BACKLIHGT_NAME "charge-pump"

#define RT4832_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define RT4832_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define RT4832_DEV_NAME "rt4832"

#define CPD_TAG                  "[ChargePump] "
#define CPD_FUN(f)               pr_debug(CPD_TAG"%s\n", __func__)
#define CPD_ERR(fmt, args...)    pr_err(CPD_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    pr_debug(CPD_TAG fmt, ##args)

/* I2C variable */
static struct i2c_client *new_client;
static const struct i2c_device_id rt4832_i2c_id[] = { {RT4832_DEV_NAME, 0}, {} };
static struct i2c_board_info i2c_rt4832 __initdata = { I2C_BOARD_INFO(RT4832_DEV_NAME, 0x11) };

static int rt4832_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

#ifdef CONFIG_OF
static const struct of_device_id rt4832_of_match[] = {
	/* { .compatible = "rt4832", }, */
	{.compatible = "mediatek,backlight",},
	{},
};

MODULE_DEVICE_TABLE(of, rt4832_of_match);
#endif

static struct i2c_driver rt4832_driver = {
	.driver = {
		   .name = "rt4832",
#ifdef CONFIG_OF
		   .of_match_table = rt4832_of_match,
#endif
		   },
	.probe = rt4832_driver_probe,
	.id_table = rt4832_i2c_id,
};

/* Flash control */
unsigned char strobe_ctrl;
unsigned char flash_ctrl;
unsigned char flash_status;

#ifndef GPIO_LCD_BL_EN
#define GPIO_LCD_BL_EN         (GPIO65 | 0x80000000)
#define GPIO_LCD_BL_EN_M_GPIO   GPIO_MODE_00
#endif

/* Gamma 2.2 Table */
unsigned char bright_arr[] = {
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12,
	13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 19, 19, 20, 21, 21, 22, 23, 24,
	24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 38, 39, 40, 41, 43, 44, 45,
	47, 48, 49, 51, 52, 54, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 73, 74, 76, 78,
	80, 82, 84, 86, 89, 91, 93, 95, 97, 100, 102, 104, 106, 109, 111, 114, 116, 119, 121, 124,
	    127
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static unsigned char current_brightness;
#endif
static unsigned char is_suspend;

struct semaphore rt4832_lock;

/* generic */
#define RT4832_MAX_RETRY_I2C_XFER (100)
#define RT4832_I2C_WRITE_DELAY_TIME 1

typedef struct {
	bool bat_exist;
	bool bat_full;
	bool bat_low;
	s32 bat_charging_state;
	s32 bat_vol;
	bool charger_exist;
	s32 pre_charging_current;
	s32 charging_current;
	s32 charger_vol;
	s32 charger_protect_status;
	s32 ISENSE;
	s32 ICharging;
	s32 temperature;
	s32 total_charging_time;
	s32 PRE_charging_time;
	s32 CC_charging_time;
	s32 TOPOFF_charging_time;
	s32 POSTFULL_charging_time;
	s32 charger_type;
	s32 PWR_SRC;
	s32 SOC;
	s32 ADC_BAT_SENSE;
	s32 ADC_I_SENSE;
} PMU_ChargerStruct;

/* i2c read routine for API*/
static char rt4832_i2c_read(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
#if !defined BMA_USE_BASIC_I2C_FUNC
	s32 dummy;

	if (NULL == client)
		return -1;

	while (0 != len--) {
#ifdef BMA_SMBUS
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy < 0) {
			CPD_ERR("i2c bus read error");
			return -1;
		}
		*data = (u8) (dummy & 0xff);
#else
		dummy = i2c_master_send(client, (char *)&reg_addr, 1);
		if (dummy < 0) {
			CPD_ERR("send dummy is %d", dummy);
			return -1;
		}

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0) {
			CPD_ERR("recv dummy is %d", dummy);
			return -1;
		}
#endif
		reg_addr++;
		data++;
	}
	return 0;
#else
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		 },

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < RT4832_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		mdelay(RT4832_I2C_WRITE_DELAY_TIME);
	}

	if (RT4832_MAX_RETRY_I2C_XFER <= retry) {
		CPD_ERR("I2C xfer error");
		return -EIO;
	}

	return 0;
#endif
}

/* i2c write routine for */
static char rt4832_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
#if !defined BMA_USE_BASIC_I2C_FUNC
	s32 dummy;
#ifndef BMA_SMBUS
	/* u8 buffer[2]; */
#endif

	if (NULL == client)
		return -1;

	while (0 != len--) {
#if 1
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif

		reg_addr++;
		data++;
		if (dummy < 0)
			return -1;
	}

#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buffer,
		 },
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < RT4832_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
				break;
			mdelay(RT4832_I2C_WRITE_DELAY_TIME);
		}
		if (RT4832_MAX_RETRY_I2C_XFER <= retry)
			return -EIO;
		reg_addr++;
		data++;
	}
#endif

	return 0;
}

static int rt4832_smbus_read_byte(struct i2c_client *client,
				  unsigned char reg_addr, unsigned char *data)
{
	return rt4832_i2c_read(client, reg_addr, data, 1);
}

static int rt4832_smbus_write_byte(struct i2c_client *client,
				   unsigned char reg_addr, unsigned char *data)
{
	int ret_val = 0;
	int i = 0;

	ret_val = rt4832_i2c_write(client, reg_addr, data, 1);

	for (i = 0; i < 5; i++) {
		if (ret_val != 0)
			rt4832_i2c_write(client, reg_addr, data, 1);
		else
			return ret_val;
	}
	return ret_val;
}

#if 0				/* sangcheol.seo 150728 bring up */
static int rt4832_smbus_read_byte_block(struct i2c_client *client,
					unsigned char reg_addr, unsigned char *data,
					unsigned char len)
{
	return rt4832_i2c_read(client, reg_addr, data, len);
}
#endif

void rt4832_dsv_ctrl(int enable)
{
	unsigned char data = 0;

	if (enable == 1) {

		data = 0x28;
		rt4832_smbus_write_byte(new_client, 0x0D, &data);
		data = 0x24;
		rt4832_smbus_write_byte(new_client, 0x0E, &data);
		rt4832_smbus_write_byte(new_client, 0x0F, &data);

		data = 0x0A;
		rt4832_smbus_write_byte(new_client, 0x09, &data);
		data = 0x48;
		rt4832_smbus_write_byte(new_client, 0x0C, &data);
	} else {

		data = 0x0C;
		rt4832_smbus_write_byte(new_client, 0x08, &data);
		data = 0x0A;
		rt4832_smbus_write_byte(new_client, 0x09, &data);
		data = 0x37;
		rt4832_smbus_write_byte(new_client, 0x0C, &data);
	}
}

void rt4832_dsv_toggle_ctrl(void)
{
	unsigned char data = 0;

	data = 0x68;
	rt4832_smbus_write_byte(new_client, 0x0D, &data);

	data = 0x24;
	rt4832_smbus_write_byte(new_client, 0x0E, &data);
	rt4832_smbus_write_byte(new_client, 0x0F, &data);

	data = 0x01;
	rt4832_smbus_write_byte(new_client, 0x0C, &data);
	data = 0x8C;		/* periodic mode */
	rt4832_smbus_write_byte(new_client, 0x08, &data);
	data = 0x2A;
	rt4832_smbus_write_byte(new_client, 0x09, &data);
}


bool check_charger_pump_vendor(void)
{
	int err = 0;
	unsigned char data = 0;

	err = rt4832_smbus_read_byte(new_client, 0x01, &data);

	if (err < 0)
		CPD_ERR("read charge-pump vendor id fail\n");

/* CPD_ERR("vendor is 0x%x\n"); */

	if ((data & 0x03) == 0x03)	/* Richtek */
		return 0;
	else
		return 1;
}


int chargepump_set_backlight_level(unsigned int level)
{
	unsigned char data = 0;
	unsigned char data1 = 0;
	unsigned int bright_per = 0;
	unsigned int results = 0;	/* sangcheol.seo@lge.com */
	int ret = 0;
	unsigned char lsb_data = 0;
	unsigned char msb_data = 0;

	if (level == 0) {
		results = down_interruptible(&rt4832_lock);
		data1 = 0x04;
		rt4832_smbus_write_byte(new_client, 0x04, &data1);
		data1 = 0x00;	/* backlight2 brightness 0 */
		rt4832_smbus_write_byte(new_client, 0x05, &data1);

		rt4832_smbus_read_byte(new_client, 0x0A, &data1);
		data1 &= 0xE6;

		rt4832_smbus_write_byte(new_client, 0x0A, &data1);
		up(&rt4832_lock);
		if (flash_status == 0) {
#ifdef USING_LCM_BL_EN
			/* mt_set_gpio_out(GPIO_LCM_BL_EN,GPIO_OUT_ZERO); */
#else
			/* mt_set_gpio_out(GPIO_LCD_BL_EN,GPIO_OUT_ZERO); */
#endif
		}
		is_suspend = 1;
	} else {
		level = MIN_MAX_SCALE(level);

		/* Gamma 2.2 Table adapted */
		bright_per = (level - (unsigned int)10) * (unsigned int)100 / (unsigned int)245;
		data = bright_arr[bright_per];

/* data = 0x70;//force the backlight on if level > 0 <==Add by Minrui for backlight debug */

		if (is_suspend == 1) {
			is_suspend = 0;
#ifdef USING_LCM_BL_EN
			/* mt_set_gpio_out(GPIO_LCM_BL_EN,GPIO_OUT_ONE); */
#else
			/* mt_set_gpio_out(GPIO_LCD_BL_EN,GPIO_OUT_ONE); */
#endif
			mdelay(10);
			results = down_interruptible(&rt4832_lock);
			if (check_charger_pump_vendor() == 0) {
				data1 = 0x54;	/* 0x37; */
				rt4832_smbus_write_byte(new_client, 0x02, &data1);
				CPD_LOG("[ChargePump]-Richtek\n");
			} else {
				data1 = 0x70;	/* 0x57; */
				rt4832_smbus_write_byte(new_client, 0x02, &data1);
				CPD_LOG("[RT4832]-TI\n");
			}
			/* TO DO */
			/* 0x03 0bit have to be set backlight brightness set by LSB or LSB and MSB */
			data1 = 0x00;	/* 11bit / */
			rt4832_smbus_write_byte(new_client, 0x03, &data1);

			msb_data = (data >> 6) | 0x04;
			lsb_data = (data << 2) | 0x03;

			rt4832_smbus_write_byte(new_client, 0x04, &msb_data);
			rt4832_smbus_write_byte(new_client, 0x05, &lsb_data);

			CPD_LOG("[RT4832]-wake up I2C check = %d\n", ret);
			CPD_LOG("[RT4832]-backlight brightness Setting[reg0x04][MSB:0x%x]\n",
				msb_data);
			CPD_LOG("[RT4832]-backlight brightness Setting[reg0x05][LSB:0x%x]\n",
				lsb_data);

			rt4832_smbus_read_byte(new_client, 0x0A, &data1);
			data1 |= 0x19;

			rt4832_smbus_write_byte(new_client, 0x0A, &data1);
			up(&rt4832_lock);
		}

		results = down_interruptible(&rt4832_lock);
		{
			unsigned char read_data = 0;

			rt4832_smbus_read_byte(new_client, 0x02, &read_data);
			CPD_LOG("[RT4832]-OVP[0x%x]\n", read_data);
		}
		msb_data = (data >> 6) | 0x04;
		lsb_data = (data << 2) | 0x03;

		CPD_LOG("[RT4832]-backlight brightness Setting[reg0x04][MSB:0x%x]\n", msb_data);
		CPD_LOG("[RT4832]-backlight brightness Setting[reg0x05][LSB:0x%x]\n", lsb_data);
		ret |= rt4832_smbus_write_byte(new_client, 0x04, &msb_data);
		ret |= rt4832_smbus_write_byte(new_client, 0x05, &lsb_data);

		CPD_LOG("[RT4832]-I2C check = %d\n", ret);
		up(&rt4832_lock);
	}
	return 0;
}

unsigned char get_rt4832_backlight_level(void)
{
	unsigned char rt4832_msb = 0;
	unsigned char rt4832_lsb = 0;
	unsigned char rt4832_level = 0;

	rt4832_smbus_read_byte(new_client, 0x04, &rt4832_msb);
	rt4832_smbus_read_byte(new_client, 0x05, &rt4832_lsb);

	rt4832_level |= ((rt4832_msb & 0x3) << 6);
	rt4832_level |= ((rt4832_lsb & 0xFC) >> 2);

	return rt4832_level;

}

void set_rt4832_backlight_level(unsigned char level)
{
	unsigned char rt4832_msb = 0;
	unsigned char rt4832_lsb = 0;
	unsigned char data = 0;

	if (level == 0)
		chargepump_set_backlight_level(level);
	else {
		if (is_suspend == 1) {
			is_suspend = 0;

			data = 0x70;	/* 0x57; */
			rt4832_smbus_write_byte(new_client, 0x02, &data);
			data = 0x00;	/* 11bit / */
			rt4832_smbus_write_byte(new_client, 0x03, &data);

			rt4832_msb = (level >> 6) | 0x04;
			rt4832_lsb = (level << 2) | 0x03;

			rt4832_smbus_write_byte(new_client, 0x04, &rt4832_msb);
			rt4832_smbus_write_byte(new_client, 0x05, &rt4832_lsb);

			rt4832_smbus_read_byte(new_client, 0x0A, &data);
			data |= 0x19;

			rt4832_smbus_write_byte(new_client, 0x0A, &data);
		} else {
			rt4832_msb = (level >> 6) | 0x04;
			rt4832_lsb = (level << 2) | 0x03;

			rt4832_smbus_write_byte(new_client, 0x04, &rt4832_msb);
			rt4832_smbus_write_byte(new_client, 0x05, &rt4832_lsb);
		}
	}
}


static int rt4832_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	CPD_FUN();

	new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);

	memset(new_client, 0, sizeof(struct i2c_client));

	new_client = client;

	return 0;
}

static int rt4832_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	new_client = client;

	CPD_FUN();
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CPD_LOG("i2c_check_functionality error\n");
		return -1;
	}

	if (client == NULL)
		CPD_ERR("%s client is NULL\n", __func__);
	else
		CPD_LOG("%s %p %x %x\n", __func__, client->adapter, client->addr, client->flags);
	return 0;
}


static int rt4832_i2c_remove(struct i2c_client *client)
{
	CPD_FUN();
	new_client = NULL;
	return 0;
}


static int
__attribute__ ((unused)) rt4832_detect(struct i2c_client *client, int kind,
					   struct i2c_board_info *info)
{
	CPD_FUN();
	return 0;
}

static struct i2c_driver rt4832_i2c_driver = {
	.driver.name = RT4832_DEV_NAME,
	.probe = rt4832_i2c_probe,
	.remove = rt4832_i2c_remove,
	.id_table = rt4832_i2c_id,
};

static int rt4832_pd_probe(struct platform_device *pdev)
{
	CPD_FUN();

	/* i2c number 1(0~2) control */
	i2c_register_board_info(2, &i2c_rt4832, 1);

#ifdef USING_LCM_BL_EN
	mt_set_gpio_mode(GPIO_LCM_BL_EN, GPIO_LCM_BL_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_BL_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_BL_EN, GPIO_DIR_OUT);
#else
	mt_set_gpio_mode(GPIO_LCD_BL_EN, GPIO_LCD_BL_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCD_BL_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCD_BL_EN, GPIO_DIR_OUT);
#endif
/* i2c_add_driver(&rt4832_i2c_driver); */
	if (i2c_add_driver(&rt4832_driver) != 0)
		CPD_ERR("Failed to register rt4832 driver");
	return 0;
}

static int __attribute__ ((unused)) rt4832_pd_remove(struct platform_device *pdev)
{
	CPD_FUN();
	i2c_del_driver(&rt4832_i2c_driver);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void rt4832_early_suspend(struct early_suspend *h)
{
	int err = 0;
	unsigned char data;

	CPD_FUN();

	down_interruptible(&rt4832_lock);
	data = 0x00;		/* backlight2 brightness 0 */
	err = rt4832_smbus_write_byte(new_client, 0x05, &data);

	err = rt4832_smbus_read_byte(new_client, 0x0A, &data);
	data &= 0xE6;

	err = rt4832_smbus_write_byte(new_client, 0x0A, &data);
	up(&rt4832_lock);
	CPD_LOG("[RT4832] rt4832_early_suspend  [%d]\n", data);
#ifdef USING_LCM_BL_EN
	mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ZERO);
#else
	mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);
#endif
}

void rt4832_flash_strobe_prepare(char OnOff, char ActiveHigh)
{
	int err = 0;

	CPD_FUN();

	down_interruptible(&rt4832_lock);

	err = rt4832_smbus_read_byte(new_client, 0x09, &strobe_ctrl);

	err = rt4832_smbus_read_byte(new_client, 0x0A, &flash_ctrl);

	strobe_ctrl &= 0xF3;

	if (ActiveHigh)
		strobe_ctrl |= 0x20;
	else
		strobe_ctrl &= 0xDF;

	if (OnOff == 1) {
		CPD_LOG("Strobe mode On\n");
		strobe_ctrl |= 0x10;
		flash_ctrl |= 0x66;
		flash_status = 1;
		CPD_LOG("[RT4832][Strobe] flash_status = %d\n", flash_status);
	} else if (OnOff == 2) {
		CPD_LOG("Torch mode On\n");
		strobe_ctrl |= 0x10;
		flash_ctrl |= 0x62;
		flash_status = 1;
		CPD_LOG("[RT4832][Torch] flash_status = %d\n", flash_status);
	} else {
		CPD_LOG("Flash Off\n");
		strobe_ctrl &= 0xEF;
		flash_ctrl &= 0x99;
		flash_status = 0;
		CPD_LOG("[RT4832][off] flash_status = %d\n", flash_status);
	}

	err = rt4832_smbus_write_byte(new_client, 0x09, &strobe_ctrl);

	up(&rt4832_lock);
}
EXPORT_SYMBOL(rt4832_flash_strobe_prepare);

/* strobe enable */
void rt4832_flash_strobe_en(void)
{
	int err = 0;

	CPD_FUN();
	down_interruptible(&rt4832_lock);
	err = rt4832_smbus_write_byte(new_client, 0x0A, &flash_ctrl);
	up(&rt4832_lock);
}
EXPORT_SYMBOL(rt4832_flash_strobe_en);


/* strobe level */
void rt4832_flash_strobe_level(char level)
{
	int err = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;
	unsigned char torch_level;
	unsigned char strobe_timeout = 0x1F;

	CPD_FUN();

	down_interruptible(&rt4832_lock);
#if 0
	if (level == 1)
		torch_level = 0x20;
	else
		torch_level = 0x50;

	err = rt4832_smbus_read_byte(new_client, 0x06, &data1);

	if (31 < level) {
		data1 = torch_level | 0x0A;
		strobe_timeout = 0x0F;
	} else if (level < 0) {
		data1 = torch_level;
	} else {
		data1 = torch_level | level;
	}

#else
	torch_level = 0x50;

	err = rt4832_smbus_read_byte(new_client, 0x06, &data1);

	strobe_timeout = 0x0F;
	if (level < 0)
		data1 = torch_level;
	else if (level == 1)
		data1 = torch_level | 0x03;
	else if (level == 2)
		data1 = torch_level | 0x05;
	else if (level == 3)
		data1 = torch_level | 0x08;
	else if (level == 4)
		data1 = torch_level | 0x0A;
	else
		data1 = torch_level | level;

#endif

	CPD_LOG("Flash Level =0x%x\n", data1);
	err = rt4832_smbus_write_byte(new_client, 0x06, &data1);

	data2 = 0x40 | strobe_timeout;
	CPD_LOG("Storbe Timeout =0x%x\n", data2);
	err |= rt4832_smbus_write_byte(new_client, 0x07, &data2);
	up(&rt4832_lock);
}
EXPORT_SYMBOL(rt4832_flash_strobe_level);


static void rt4832_late_resume(struct early_suspend *h)
{
	int err = 0;
	unsigned char data1;

	CPD_FUN();

#ifdef USING_LCM_BL_EN
	mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ONE);
#else
	mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
#endif
	mdelay(50);
	down_interruptible(&rt4832_lock);
	err = rt4832_smbus_write_byte(new_client, 0x05, &current_brightness);

	err = rt4832_smbus_read_byte(new_client, 0x0A, &data1);
	data1 |= 0x19;		/* backlight enable */

	err = rt4832_smbus_write_byte(new_client, 0x0A, &data1);
	up(&rt4832_lock);
	CPD_LOG("[RT4832] rt4832_late_resume  [%d]\n", data1);
}

static struct early_suspend __attribute__ ((unused)) rt4832_early_suspend_desc = {
.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN, .suspend = rt4832_early_suspend, .resume =
	    rt4832_late_resume,};
#endif

static struct platform_driver rt4832_backlight_driver = {
	.remove = rt4832_pd_remove,
	.probe = rt4832_pd_probe,
	.driver = {
		   .name = BACKLIHGT_NAME,
		   .owner = THIS_MODULE,
		   },
};

#if 0
#ifdef CONFIG_OF
static struct platform_device mtk_backlight_dev = {
	.name = BACKLIHGT_NAME,
	.id = -1,
};
#endif
#endif
static int __init rt4832_init(void)
{
	CPD_FUN();
	sema_init(&rt4832_lock, 1);

#if 0
#ifdef CONFIG_OF
	if (platform_device_register(&mtk_backlight_dev)) {
		CPD_ERR("failed to register device");
		return -1;
	}
#endif

#ifndef	CONFIG_MTK_LEDS
	register_early_suspend(&rt4832_early_suspend_desc);
#endif

	if (platform_driver_register(&rt4832_backlight_driver)) {
		CPD_ERR("failed to register driver");
		return -1;
	}
#else

	/* i2c number 1(0~2) control */
	i2c_register_board_info(2, &i2c_rt4832, 1);

	/*if(platform_driver_register(&rt4832_backlight_driver))
	   {
	   CPD_ERR("failed to register driver");
	   return -1;
	   } */
#ifdef USING_LCM_BL_EN
	mt_set_gpio_mode(GPIO_LCM_BL_EN, GPIO_LCM_BL_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_BL_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_BL_EN, GPIO_DIR_OUT);
#else
	mt_set_gpio_mode(GPIO_LCD_BL_EN, GPIO_LCD_BL_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCD_BL_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCD_BL_EN, GPIO_DIR_OUT);
#endif

/* i2c_add_driver(&rt4832_i2c_driver); */
	if (i2c_add_driver(&rt4832_driver) != 0)
		CPD_ERR("Failed to register rt4832 driver");
#endif

	return 0;
}

static void __exit rt4832_exit(void)
{
	platform_driver_unregister(&rt4832_backlight_driver);
}

MODULE_AUTHOR("Albert Zhang <xu.zhang@bosch-sensortec.com>");
MODULE_DESCRIPTION("rt4832 driver");
MODULE_LICENSE("GPL");

late_initcall(rt4832_init);
module_exit(rt4832_exit);
