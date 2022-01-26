/*
 * Filename: lcm_cust_common.c
 * date:20201209
 * Description: cust common source file
 * Author:samir.liu
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include "lcm_cust_common.h"
#include "linux/hardware_info.h"

#define LOG_TAG "LCM_COMMON"

#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)

#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"

extern char Lcm_name[HARDWARE_MAX_ITEM_LONGTH];

static struct i2c_client *_lcm_i2c_client;
struct lm36273_reg {
	uint8_t reg;
	uint8_t value;
};

struct lm36273_led {
	struct mutex lock;
	int level;
	int hbm_status;
	int cur_level;
};
static struct lm36273_led g_lm36273_led;

int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_err("[ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

struct lm36273_reg lm36273_regs_conf[] = {
	{ LP36273_DISP_BC1, 0x38 },/* disable pwm*/
	{ LP36273_DISP_BC2, 0x85},
	//{ LP36273_DISP_FULL_CURRENT, 0xa0},
	{ LP36273_DISP_BIAS_VPOS, 0x1e },/* set vsp to +5.5V*/
	{ LP36273_DISP_BIAS_VNEG, 0x1e },/* set vsp to +5.5V*/
};
int lm36273_bl_bias_conf(void)
{
	int ret, i, reg_count;

	LCM_LOGI("lm36273_bl_bias_conf backlight and bias setting\n");

	//lcm_set_gpio_output(LCM_BIAS_ENABLE,1);
	mdelay(2);
	reg_count = ARRAY_SIZE(lm36273_regs_conf) / sizeof(lm36273_regs_conf[0]);
	for (i = 0; i < reg_count; i++)
		ret = _lcm_i2c_write_bytes(lm36273_regs_conf[i].reg, lm36273_regs_conf[i].value);

	return ret;
}

int lm36273_bias_enable(int enable, int delayMs)
{
	if (delayMs > 100)
		delayMs = 100;

	if (enable) {
		/* enable LCD bias VPOS */
		_lcm_i2c_write_bytes(LP36273_DISP_BIAS_CONF1, 0x9c);
		mdelay(delayMs);
		/* enable LCD bias VNEG */
		_lcm_i2c_write_bytes(LP36273_DISP_BIAS_CONF1, 0x9e);
	} else {
		/* disable LCD bias VNEG */
		_lcm_i2c_write_bytes(LP36273_DISP_BIAS_CONF1, 0x9c);
		mdelay(delayMs);
		/* disable LCD bias VPOS */
		_lcm_i2c_write_bytes(LP36273_DISP_BIAS_CONF1, 0x98);
		/* bias supply off */
		_lcm_i2c_write_bytes(LP36273_DISP_BIAS_CONF1, 0x18);
		mdelay(delayMs);
		//lcm_set_gpio_output(LCM_BIAS_ENABLE,0);
	}

	return 0;
}

int hbm_brightness_set(int level)
{
	mutex_lock(&g_lm36273_led.lock);

	switch (level) {
	case DISPPARAM_LCD_HBM_L1_ON:
		_lcm_i2c_write_bytes(LP36273_DISP_BB_LSB, BL_HBM_L1 & 0x7);
		_lcm_i2c_write_bytes(LP36273_DISP_BB_MSB, BL_HBM_L1 >> 3);
		g_lm36273_led.hbm_status = 1;
		break;
	case DISPPARAM_LCD_HBM_L2_ON:
		_lcm_i2c_write_bytes(LP36273_DISP_BB_LSB, BL_HBM_L2 & 0x7);
		_lcm_i2c_write_bytes(LP36273_DISP_BB_MSB, BL_HBM_L2 >> 3);
		g_lm36273_led.hbm_status = 2;
		break;
	case DISPPARAM_LCD_HBM_L3_ON:
		if (!strncmp(Lcm_name, "k16a_36_02_0a_vdo", 17)) {
			_lcm_i2c_write_bytes(LP36273_DISP_BB_LSB, BL_HBM_L3_2 & 0x7);
			_lcm_i2c_write_bytes(LP36273_DISP_BB_MSB, BL_HBM_L3_2 >> 3);
		} else {
			_lcm_i2c_write_bytes(LP36273_DISP_BB_LSB, BL_HBM_L3 & 0x7);
			_lcm_i2c_write_bytes(LP36273_DISP_BB_MSB, BL_HBM_L3 >> 3);
		}
		g_lm36273_led.hbm_status = 3;
		break;
	case DISPPARAM_LCD_HBM_OFF:
		if (g_lm36273_led.level > 0 && g_lm36273_led.level <= BL_LEVEL_MAX) {
			_lcm_i2c_write_bytes(LP36273_DISP_BB_LSB, g_lm36273_led.level & 0x7);
			_lcm_i2c_write_bytes(LP36273_DISP_BB_MSB, g_lm36273_led.level >> 3);
		}
		g_lm36273_led.hbm_status = 0;
		break;
	default:
		break;
	}

	mutex_unlock(&g_lm36273_led.lock);
	return 0;
}
EXPORT_SYMBOL(hbm_brightness_set);

int lm36273_brightness_set(int level)
{
	int temp_level, LSB_tmp, MSB_tmp;
	if (level > 2047)
	    level = 2047;

	if (g_lm36273_led.hbm_status && level) {
          	if (g_lm36273_led.level == 0) {
                  	hbm_brightness_set(g_lm36273_led.hbm_status);
          		_lcm_i2c_write_bytes(LP36273_DISP_BL_ENABLE, 0x17);
          		_lcm_i2c_write_bytes(LP36273_DISP_BC2, 0xcd);
                }
		return 0;
	}

	temp_level = level * 180 / 255;
	LSB_tmp = temp_level & 0x7;
	MSB_tmp = (temp_level >> 3) & 0xFF;

	mutex_lock(&g_lm36273_led.lock);
	_lcm_i2c_write_bytes(LP36273_DISP_BB_LSB, LSB_tmp);
	_lcm_i2c_write_bytes(LP36273_DISP_BB_MSB, MSB_tmp);
	if (level == 0 && g_lm36273_led.level != 0) {
		_lcm_i2c_write_bytes(LP36273_DISP_BL_ENABLE, 0x0);
	} else if (level > 0 && g_lm36273_led.level == 0) {
		_lcm_i2c_write_bytes(LP36273_DISP_BL_ENABLE, 0x17);
		_lcm_i2c_write_bytes(LP36273_DISP_BC2, 0xcd);
	}

	pr_info("%s backlight = %d, temp_level = %d\n", __func__, level, temp_level);
	g_lm36273_led.level = temp_level;
	g_lm36273_led.cur_level = level;
	mutex_unlock(&g_lm36273_led.lock);
	return 0;
}
EXPORT_SYMBOL(lm36273_brightness_set);

int hbm_brightness_get(void)
{
	int hbm_status;
	hbm_status = g_lm36273_led.hbm_status;
	return hbm_status;
}
EXPORT_SYMBOL(hbm_brightness_get);

int lcm_get_cur_level(void)
{
	int cur_level;
	cur_level = g_lm36273_led.cur_level;
	return cur_level;
}
EXPORT_SYMBOL(lcm_get_cur_level);

unsigned int panel_white_point_x;
unsigned int panel_white_point_y;
unsigned int panel_white_point_lv;

#define WHITE_POINT_BASE_X 172
#define WHITE_POINT_BASE_Y 192

static int __init disp_get_white_point_x(char *arg)
{
	unsigned int x;
	kstrtouint(arg, 10, &x);
	panel_white_point_x = x + WHITE_POINT_BASE_X;
}

static int __init disp_get_white_point_y(char *arg)
{
	int y;
	kstrtouint(arg, 10, &y);
	panel_white_point_y = y + WHITE_POINT_BASE_Y;
}

static int __init disp_get_white_point_lv(char *arg)
{
	int lv;
	kstrtouint(arg, 10, &lv);
	panel_white_point_lv = lv;
}

static int _lcm_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	LCM_LOGD("%s\n", __func__);
	LCM_LOGD("info==>name=%s addr=0x%x\n", client->name, client->addr);
	_lcm_i2c_client = client;
	mutex_init(&g_lm36273_led.lock);
	g_lm36273_led.hbm_status = 0;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	LCM_LOGD("%s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}


static const struct of_device_id _lcm_i2c_of_match[] = {
	{
	    .compatible = "mediatek,i2c_lcd_bias",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { { LCM_I2C_ID_NAME, 0 },
						    {} };

static struct i2c_driver lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

static int __init lcm_i2c_init(void)
{
	LCM_LOGD("%s\n", __func__);
	i2c_add_driver(&lcm_i2c_driver);
	LCM_LOGD("%s success\n", __func__);
	return 0;
}

static void __exit lcm_i2c_exit(void)
{
	LCM_LOGD("%s\n", __func__);
	i2c_del_driver(&lcm_i2c_driver);
}

early_param("androidboot.lcm_white_ponit_x", disp_get_white_point_x);
early_param("androidboot.lcm_white_ponit_y", disp_get_white_point_y);
early_param("androidboot.lcm_white_ponit_lv", disp_get_white_point_lv);

module_init(lcm_i2c_init);
module_exit(lcm_i2c_exit);

MODULE_DESCRIPTION("lcm cust common Ctrl Funtion Driver");
MODULE_LICENSE("GPL v2");

