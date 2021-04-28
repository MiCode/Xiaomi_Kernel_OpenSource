/**************************************************************************
*  AW2028_LED.c
*
*  Create Date :
*
*  Modify Date :
*
*  Create by   : AWINIC Technology CO., LTD
*
*  Version     : 1.0.0 , 2020/12/22
**************************************************************************/

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/module.h>

#include <linux/leds.h>
#include "aw2028_led.h"

//////////////////////////////////////////////////////
// i2c write and read
//////////////////////////////////////////////////////
#ifdef CONFIG_CUSTOM_KERNEL_ALS_MULTI_CALI_SUPPORT
#define BACKLED_STATE_NOTIFY
#endif
unsigned char I2C_write_reg(unsigned char addr, unsigned char reg_data)
{
	char ret;
	u8 wdbuf[512] = {0};

	struct i2c_msg msgs[] = {
		{
			.flags	= 0,
			.len	= 2,
			.buf	= wdbuf,
		},
	};

	if(NULL == AW2028_i2c_client)
	{
		pr_err("AW2028_i2c_client is NULL\n");
		return -1;
	}

	msgs[0].addr = AW2028_i2c_client->addr,
	wdbuf[0] = addr;
	wdbuf[1] = reg_data;

	ret = i2c_transfer(AW2028_i2c_client->adapter, msgs, 1);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

    return ret;
}

unsigned char I2C_read_reg(unsigned char addr)
{
	unsigned char ret;
	u8 rdbuf[512] = {0};

	struct i2c_msg msgs[] = {
		{
			.flags	= 0,
			.len	= 1,
			.buf	= rdbuf,
		},
		{
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= rdbuf,
		},
	};

	if(NULL == AW2028_i2c_client)
	{
		pr_err("AW2028_i2c_client is NULL\n");
		return -1;
	}

	msgs[0].addr = AW2028_i2c_client->addr,
	msgs[1].addr = AW2028_i2c_client->addr,
	rdbuf[0] = addr;

	ret = i2c_transfer(AW2028_i2c_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

    return rdbuf[0];
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AW2028 LED
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned char ms2timer(unsigned int time)
{
	unsigned char i;
	unsigned int ref[16] = {4, 128, 256, 384, 512, 762, 1024, 1524, 2048, 2560, 3072, 4096, 5120, 6144, 7168, 8192};

	for(i=0; i<15; i++)
	{
		if(time <= ref[0])
		{
			return 0;
		}
		else if(time > ref[15])
		{
			return 15;
		}
		else if((time>ref[i]) && (time<=ref[i+1]))
		{
			if((time-ref[i]) <= (ref[i+1]-time))
			{
				return i;
			}
			else
			{
				return (i+1);
			}
		}
	}
	return 0;
}


unsigned char AW2028_LED_ON(unsigned char r, unsigned char g, unsigned char b)
{
	I2C_write_reg(0x00, 0x55);		// software reset

	I2C_write_reg(0x01, 0x01);		// GCR
	I2C_write_reg(0x03, 0x02);		// IMAX
	I2C_write_reg(0x04, 0x00);		// LCFG1
	I2C_write_reg(0x05, 0x00);		// LCFG2
	I2C_write_reg(0x06, 0x00);		// LCFG3
	I2C_write_reg(0x07, 0x07);		// LEDEN

	I2C_write_reg(0x10, r);		// ILED1
	I2C_write_reg(0x11, g);		// ILED2
	I2C_write_reg(0x12, b);		// ILED3
	I2C_write_reg(0x1C, 0xFF);		// PWM1
	I2C_write_reg(0x1D, 0xFF);		// PWM2
	I2C_write_reg(0x1E, 0xFF);		// PWM3	

	return 0;
}

unsigned char AW2028_LED_ON_COMMON(uint8_t reg_addr, enum led_brightness bright)
{
	I2C_write_reg(0x01, 0x01);		// GCR
	I2C_write_reg(0x03, 0x02);		// IMAX
	I2C_write_reg(0x04, 0x00);		// LCFG1
	I2C_write_reg(0x05, 0x00);		// LCFG2
	I2C_write_reg(0x06, 0x00);		// LCFG3
	I2C_write_reg(0x07, 0x07);		// LEDEN
	I2C_write_reg(0x08, 0x00);		// LEDCTR

	I2C_write_reg(reg_addr, bright);		// IDLE1/2/3

	I2C_write_reg(0x1C, 0xFF);		// PWM1
	I2C_write_reg(0x1D, 0xFF);		// PWM2
	I2C_write_reg(0x1E, 0xFF);		// PWM3

	return 0;
}

unsigned char AW2028_LED_OFF(void)
{
	I2C_write_reg(0x00, 0x55);		// software reset
	return 0;
}

unsigned char AW2028_LED_Blink(unsigned char r, unsigned char g, unsigned char b, unsigned int trise_ms, unsigned int ton_ms, unsigned int tfall_ms, unsigned int toff_ms)
{
	unsigned char trise, ton, tfall, toff;

	trise = ms2timer(trise_ms);
	ton   = ms2timer(ton_ms);
	tfall = ms2timer(tfall_ms);
	toff  = ms2timer(toff_ms);

	I2C_write_reg(0x00, 0x55);		// software reset

	I2C_write_reg(0x01, 0x01);		// GCR
	I2C_write_reg(0x03, 0x02);		// IMAX
	I2C_write_reg(0x04, 0x01);		// LCFG1
	I2C_write_reg(0x05, 0x01);		// LCFG2
	I2C_write_reg(0x06, 0x01);		// LCFG3
	I2C_write_reg(0x07, 0x07);		// LEDEN
	I2C_write_reg(0x08, 0x08);		// LEDCTR

	I2C_write_reg(0x10, r);		// ILED1
	I2C_write_reg(0x11, g);		// ILED2
	I2C_write_reg(0x12, b);		// ILED3
	I2C_write_reg(0x1C, 0xFF);		// PWM1
	I2C_write_reg(0x1D, 0xFF);		// PWM2
	I2C_write_reg(0x1E, 0xFF);		// PWM3	

	I2C_write_reg(0x30, (trise<<4)|ton);		// PAT_T1		Trise & Ton
	I2C_write_reg(0x31, (tfall<<4)|toff);		// PAT_T2		Tfall & Toff
	I2C_write_reg(0x32, 0x00);		// PAT_T3				Tdelay
	I2C_write_reg(0x33, 0x00);		// PAT_T4 	  PAT_CTR & Color
	I2C_write_reg(0x34, 0x00);		// PAT_T5		    Timer

	I2C_write_reg(0x09, 0x07);		// PAT_RIN
	return 0;
}

unsigned char AW2028_LED_Blink_ON(unsigned int trise_ms,unsigned int tfall_ms)
{
	unsigned char trise, ton, tfall, toff;

	ton = trise = ms2timer(trise_ms);
	toff = tfall = ms2timer(tfall_ms);

	I2C_write_reg(0x04, 0x01);		// LCFG1
	I2C_write_reg(0x05, 0x01);		// LCFG2
	I2C_write_reg(0x06, 0x01);		// LCFG3
	I2C_write_reg(0x07, 0x07);		// LEDEN
	I2C_write_reg(0x08, 0x08);		// LEDCTR

	I2C_write_reg(0x30, (trise<<4)|ton);		// PAT_T1		Trise & Ton
	I2C_write_reg(0x31, (tfall<<4)|toff);		// PAT_T2		Tfall & Toff

	I2C_write_reg(0x09, 0x07);		// PAT_RIN
	return 0;
}

unsigned char AW2028_LED_Blink_OFF(unsigned int trise_ms,unsigned int tfall_ms)
{
	unsigned char trise, ton, tfall, toff;

	ton = trise = ms2timer(trise_ms);
	toff = tfall = ms2timer(tfall_ms);

	I2C_write_reg(0x04, 0x00);		// LCFG1
	I2C_write_reg(0x05, 0x00);		// LCFG2
	I2C_write_reg(0x06, 0x00);		// LCFG3
	I2C_write_reg(0x07, 0x07);		// LEDEN
	I2C_write_reg(0x08, 0x00);		// LEDCTR

	I2C_write_reg(0x30, (trise<<4)|ton);		// PAT_T1		Trise & Ton
	I2C_write_reg(0x31, (tfall<<4)|toff);		// PAT_T2		Tfall & Toff

	I2C_write_reg(0x09, 0x00);		// PAT_RIN
	return 0;
}

unsigned char AW2028_Audio_Corss_Zero(void)
{
	I2C_write_reg(0x00, 0x55);		// software reset

	I2C_write_reg(0x01, 0x01);		// GCR
	I2C_write_reg(0x03, 0x01);		// IMAX
	I2C_write_reg(0x07, 0x07);		// LEDEN
	I2C_write_reg(0x10, 0xFF);		// ILED1
	I2C_write_reg(0x11, 0xFF);		// ILED2
	I2C_write_reg(0x12, 0xFF);		// ILED3
	I2C_write_reg(0x1C, 0xFF);		// PWM1
	I2C_write_reg(0x1D, 0xFF);		// PWM2
	I2C_write_reg(0x1E, 0xFF);		// PWM3

	I2C_write_reg(0x40, 0x11);		// AUDIO_CTR
	I2C_write_reg(0x41, 0x07);		// AUDIO_LEDEN
	I2C_write_reg(0x42, 0x00);		// AUDIO_FLT
	I2C_write_reg(0x43, 0x1A);		// AGC_GAIN
	I2C_write_reg(0x44, 0x1F);		// GAIN_MAX
	I2C_write_reg(0x45, 0x3D);		// AGC_CFG
	I2C_write_reg(0x46, 0x14);		// ATTH
	I2C_write_reg(0x47, 0x0A);		// RLTH
	I2C_write_reg(0x48, 0x00);		// NOISE
	I2C_write_reg(0x49, 0x02);		// TIMER
	I2C_write_reg(0x40, 0x13);		// AUDIO_CTR

	return 0;
}

unsigned char AW2028_Audio_Timer(void)
{
	I2C_write_reg(0x00, 0x55);		// software reset

	I2C_write_reg(0x01, 0x01);		// GCR
	I2C_write_reg(0x03, 0x01);		// IMAX
	I2C_write_reg(0x07, 0x07);		// LEDEN
	I2C_write_reg(0x10, 0xFF);		// ILED1
	I2C_write_reg(0x11, 0xFF);		// ILED2
	I2C_write_reg(0x12, 0xFF);		// ILED3
	I2C_write_reg(0x1C, 0xFF);		// PWM1
	I2C_write_reg(0x1D, 0xFF);		// PWM2
	I2C_write_reg(0x1E, 0xFF);		// PWM3

	I2C_write_reg(0x40, 0x11);		// AUDIO_CTR
	I2C_write_reg(0x41, 0x07);		// AUDIO_LEDEN
	I2C_write_reg(0x42, 0x00);		// AUDIO_FLT
	I2C_write_reg(0x43, 0x1A);		// AGC_GAIN
	I2C_write_reg(0x44, 0x1F);		// GAIN_MAX
	I2C_write_reg(0x45, 0x3D);		// AGC_CFG
	I2C_write_reg(0x46, 0x14);		// ATTH
	I2C_write_reg(0x47, 0x0A);		// RLTH
	I2C_write_reg(0x48, 0x00);		// NOISE
	I2C_write_reg(0x49, 0x00);		// TIMER
	I2C_write_reg(0x40, 0x0B);		// AUDIO_CTR

	return 0;
}


unsigned char AW2028_Audio(unsigned char mode)
{
	if(mode > 5)
	{
		mode = 0;
	}
	I2C_write_reg(0x00, 0x55);		// software reset

	I2C_write_reg(0x01, 0x01);		// GCR
	I2C_write_reg(0x03, 0x01);		// IMAX
	I2C_write_reg(0x07, 0x07);		// LEDEN
	I2C_write_reg(0x10, 0xFF);		// ILED1
	I2C_write_reg(0x11, 0xFF);		// ILED2
	I2C_write_reg(0x12, 0xFF);		// ILED3
	I2C_write_reg(0x1C, 0xFF);		// PWM1
	I2C_write_reg(0x1D, 0xFF);		// PWM2
	I2C_write_reg(0x1E, 0xFF);		// PWM3

	I2C_write_reg(0x40, (mode<<3)|0x01);		// AUDIO_CTR
	I2C_write_reg(0x41, 0x07);		// AUDIO_LEDEN
	I2C_write_reg(0x42, 0x00);		// AUDIO_FLT
	I2C_write_reg(0x43, 0x1A);		// AGC_GAIN
	I2C_write_reg(0x44, 0x1F);		// GAIN_MAX
	I2C_write_reg(0x45, 0x3D);		// AGC_CFG
	I2C_write_reg(0x46, 0x14);		// ATTH
	I2C_write_reg(0x47, 0x0A);		// RLTH
	I2C_write_reg(0x48, 0x00);		// NOISE
	I2C_write_reg(0x49, 0x00);		// TIMER
	I2C_write_reg(0x40, (mode<<3)|0x03);		// AUDIO_CTR

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t AW2028_get_reg(struct device* cd,struct device_attribute *attr, char* buf)
{
	unsigned char reg_val;
	unsigned char i;
	ssize_t len = 0;
	for(i=0;i<0x4B;i++)
	{
		reg_val = I2C_read_reg(i);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg%2X = 0x%2X, ", i,reg_val);
	}

	return len;
}

static ssize_t AW2028_set_reg(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	unsigned int databuf[2];
	if(2 == sscanf(buf,"%x %x",&databuf[0], &databuf[1]))
	{
		I2C_write_reg((unsigned char)databuf[0],databuf[1]);
	}
	return len;
}

static ssize_t AW2028_get_debug(struct device* cd,struct device_attribute *attr, char* buf)
{
	ssize_t len = 0;
	len += snprintf(buf+len, PAGE_SIZE-len, "AW2028_LED_OFF(void)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 0 > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "AW2028_LED_ON(r, g, b)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 1  r   g   b  > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 1 255 255 255 > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "AW2028_LED_Blink(r, g, b, trise, ton, tfall, tfall)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 2  r   g   b  trise ton tfall toff > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 2 255 255 255 1000    0 1000  1000 > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "AW2028_LED_Audio(mode)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 3 mode > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 3   1  > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");

	return len;
}

#ifdef BACKLED_STATE_NOTIFY
extern void cabc_backled_data_notification(int backlight_value);
#endif
static ssize_t AW2028_set_debug(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	unsigned int databuf[16];
	bool led_state = false;
	sscanf(buf,"%d",&databuf[0]);
	pr_info("led_state buf %d %d %d %d\n", __func__, databuf[0],databuf[1],databuf[2],databuf[3]);
	if(databuf[0] == 0) {		// OFF
		led_state = false;
		AW2028_LED_OFF();
	} else if(databuf[0] == 1) {	//ON
		sscanf(&buf[1], "%d %d %d", &databuf[1], &databuf[2], &databuf[3]);
		if ((databuf[1] == 0 && databuf[2] == 0 && databuf[3] == 0) || databuf[0] == 0) {
			led_state = false;
		} else {
			led_state = true;
	}
		AW2028_LED_ON(databuf[1], databuf[2], databuf[3]);
	} else if(databuf[0] == 2) {	//Blink
		sscanf(&buf[1], "%d %d %d %d %d %d %d", &databuf[1], &databuf[2], &databuf[3], &databuf[4], &databuf[5], &databuf[6], &databuf[7]);
		AW2028_LED_Blink(databuf[1], databuf[2], databuf[3], databuf[4], databuf[5], databuf[6], databuf[7]);
	} else if(databuf[0] == 3) {	//Audio
		sscanf(&buf[1], "%d", &databuf[1]);
		AW2028_Audio(databuf[1]);
	}
#ifdef BACKLED_STATE_NOTIFY
	//we report backled_state to lux_b sensor here for cabc feature is changing backled_state
	if (led_state == false) {
		pr_info("led_state debug set off\n");
		cabc_backled_data_notification(0);
	}
	else if (led_state == true) {
		pr_info("led_state debug set on\n");
		cabc_backled_data_notification(1);
	}
#endif
	return len;
}

EXPORT_SYMBOL(AW2028_set_debug);

static int AW2028_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);
	pr_info("%s device->name = %s\n", __func__, dev->init_name);
	err = device_create_file(dev, &dev_attr_reg);
	err = device_create_file(dev, &dev_attr_debug);
	return err;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int AW2028_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	unsigned char reg_value;
	unsigned char cnt = 5;
	int err = 0;
	pr_info("%s start\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	AW2028_i2c_client = client;

	while(cnt>0)
	{
		reg_value = I2C_read_reg(0x00);
		pr_info("AW2028 CHIPID=0x%2x\n", reg_value);
		if(reg_value == 0xB1)
		{
			break;
		}
		msleep(10);
		cnt--;
	}
	if(!cnt)
	{
		err = -ENODEV;
		goto exit_create_singlethread;
	}

	AW2028_create_sysfs(client);

	return 0;

exit_create_singlethread:
	AW2028_i2c_client = NULL;
exit_check_functionality_failed:
	return err;
}

static int AW2028_i2c_remove(struct i2c_client *client)
{
//	AW2028_i2c_client = NULL;
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id AW2028_i2c_id[] = {
	{ AW2028_I2C_NAME, 0 },
	{ }
};

static const struct of_device_id extled_of_match[] = {
	{.compatible = "awinic, aw2028_i2c"},
	{},
};


static struct i2c_driver AW2028_i2c_driver = {
    .driver = {
            .owner  = THIS_MODULE,
            .name   = AW2028_I2C_NAME,
		.of_match_table = extled_of_match,

    },

    .probe          = AW2028_i2c_probe,
    .remove         = AW2028_i2c_remove,
    .id_table       = AW2028_i2c_id,
};

static int AW2028_led_remove(struct platform_device *pdev)
{
	pr_info("AW2028 remove\n");
	i2c_del_driver(&AW2028_i2c_driver);
	return 0;
}

static struct aw2028_rgbled_info aw2028_rgbled_info[AW2028_LED_MAX] = {
	{
		.l_info = {
			.led = {
				//.max_brightness = 12,
				.brightness_set = aw2028_led_bright_set,
				.brightness_get = aw2028_led_bright_get,
				.blink_set = aw2028_led_blink_set,
			},
			.magic_code = 12121212,
		},
		.index = AW2028_LED_1,
	},
	{
		.l_info = {
			.led = {
				//.max_brightness = 12,
				.brightness_set = aw2028_led_bright_set,
				.brightness_get = aw2028_led_bright_get,
				.blink_set = aw2028_led_blink_set,
			},
			.magic_code = 12121212,
		},
		.index = AW2028_LED_2,
	},
	{
		.l_info = {
			.led = {
				//.max_brightness = 12,
				.brightness_set = aw2028_led_bright_set,
				.brightness_get = aw2028_led_bright_get,
				.blink_set = aw2028_led_blink_set,
			},
			.magic_code = 12121212,
		},
		.index = AW2028_LED_3,
	},
};

static const struct aw2028_led_platform_data def_platform_data = {
};

static const struct aw2028_val_prop aw2028_val_props[] = {
};

static inline int aw2028_led_get_index(struct led_classdev *led)
{
	struct aw2028_rgbled_info *led_info =
		(struct aw2028_rgbled_info *)led;

	return led_info->index;
}

enum led_brightness bright_rgb[AW2028_LED_MAX] = {0};
static void aw2028_led_bright_set(
	struct led_classdev *led, enum led_brightness bright)
{
	int led_index = aw2028_led_get_index(led);
	uint8_t reg_addr = 0;

	switch (led_index) {
	case AW2028_LED_1:
		reg_addr = 0x10;
		break;
	case AW2028_LED_2:
		reg_addr = 0x11;
		break;
	case AW2028_LED_3:
		reg_addr = 0x12;
		break;
	}

	bright_rgb[led_index] = bright;

	AW2028_LED_ON_COMMON(reg_addr, bright);
#ifdef BACKLED_STATE_NOTIFY
	//we report backled_state to lux_b sensor here for cabc feature is changing backled_state
	pr_info("led_state buf %d %d %d %d\n", __func__, bright_rgb[AW2028_LED_1],bright_rgb[AW2028_LED_2],bright_rgb[AW2028_LED_3]);
	if (bright_rgb[AW2028_LED_1] == 0 && bright_rgb[AW2028_LED_2] == 0 && bright_rgb[AW2028_LED_3] == 0) {
		pr_info("led_state set off\n");
		cabc_backled_data_notification(0);
	}
	else {
		pr_info("led_state set on\n");
		cabc_backled_data_notification(1);
	}
#endif
}

static enum led_brightness aw2028_led_bright_get(struct led_classdev *led)
{
	int led_index = aw2028_led_get_index(led);
	return bright_rgb[led_index];
}

static int aw2028_led_blink_set(struct led_classdev *led,
	unsigned long *delay_on, unsigned long *delay_off)
{
	int ret = 0;
	pr_info("%s start, delay_on = %d\n", __func__, *delay_on);
	if (*delay_on) {
		AW2028_LED_Blink_ON(*delay_on, *delay_off);
	} else {
		AW2028_LED_Blink_OFF(*delay_on, *delay_off);
	}

	return ret;
}

static inline void AW2028_dt_parser_helper(struct device_node *np, void *data,
					   const struct aw2028_val_prop *props,
					   int prop_cnt)
{
	int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		of_property_read_u32(np, props[i].name, data + props[i].offset);
	}
}
static int AW2028_rgbled_parse_dt_data(struct device *dev,
				      struct aw2028_led_platform_data *pdata)
{
	int ret = 0;
	int name_cnt = 0;
	struct device_node *np = dev->of_node;

	dev_dbg(dev, "%s ++\n", __func__);
	memcpy(pdata, &def_platform_data, sizeof(*pdata));
	AW2028_dt_parser_helper(np, (void *)pdata,
		aw2028_val_props, ARRAY_SIZE(aw2028_val_props));

	while (true) {
		const char *name = NULL;

		ret = of_property_read_string_index(np, "mt,led_name",
			name_cnt, &name);
		if (ret < 0)
			break;
		pdata->led_name[name_cnt] = name;
		name_cnt++;
	}

	dev_dbg(dev, "%s --\n", __func__);
	return 0;
}

static int AW2028_led_probe(struct platform_device *pdev)
{
	struct aw2028_led_platform_data *pdata =
			dev_get_platdata(&pdev->dev);
	struct aw2028_rgbled_info *mpri;
	bool use_dt = pdev->dev.of_node;
	int ret = 0, i = 0;
	pr_info("%s start\n", __func__);

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (use_dt) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = AW2028_rgbled_parse_dt_data(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "parse dt fail\n");
			return ret;
		}
		pdev->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data specified\n");
		return -EINVAL;
	}
	mpri = devm_kzalloc(&pdev->dev, sizeof(*mpri), GFP_KERNEL);
	if (!mpri)
		return -ENOMEM;
	mpri->dev = &pdev->dev;
	platform_set_drvdata(pdev, mpri);

	for (i = 0; i < ARRAY_SIZE(aw2028_rgbled_info); i++) {
		aw2028_rgbled_info[i].l_info.led.name = pdata->led_name[i];
		ret = led_classdev_register(&pdev->dev,
				&aw2028_rgbled_info[i].l_info.led);
		if (ret < 0) {
			dev_err(&pdev->dev, "register led %d fail\n", i);
			goto out_led_register;
		}
	}
	pr_info("%s successfully probed\n", __func__);

	dev_info(&pdev->dev, "%s: successfully probed\n", __func__);
	return 0;

out_led_register:
	while (--i >= 0)
		led_classdev_unregister(&aw2028_rgbled_info[i].l_info.led);

	return -EINVAL;
}

static const struct of_device_id aw2028plt_of_match[] = {
	{.compatible = "mediatek,mt6360_pmu_rgbled"},
	{},
};

static struct platform_driver AW2028_led_driver = {
	.probe	 = AW2028_led_probe,
	.remove	 = AW2028_led_remove,
	.driver = {
		.name  = "aw2028_led",
		.of_match_table = aw2028plt_of_match,
	}
};

static int __init AW2028_LED_init(void) {
	int ret;
	pr_info("%s start\n", __func__);

	ret = platform_driver_register(&AW2028_led_driver);
	if (ret) {
		pr_info("****[%s] Unable to register driver (%d)\n", __func__, ret);
		return ret;
	}

	ret = i2c_add_driver(&AW2028_i2c_driver);
	if (ret != 0) {
		pr_info("[%s] failed to register AW2028 i2c driver.\n", __func__);
		return ret;
	} else {
		pr_info("[%s] Success to register AW2028 i2c driver.\n", __func__);
	}
	return 0;
}

static void __exit AW2028_LED_exit(void) {
	pr_info("%s exit\n", __func__);
	platform_driver_unregister(&AW2028_led_driver);
}

module_init(AW2028_LED_init);
module_exit(AW2028_LED_exit);
