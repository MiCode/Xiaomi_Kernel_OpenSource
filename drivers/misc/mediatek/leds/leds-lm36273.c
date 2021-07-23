#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
//#include <../lcm/panel_set_disp_param.h>


#include "leds-lm36273.h"
#ifdef CONFIG_MACH_MT6785
#include <../lcm/panel_set_disp_param.h>
#else
#include "dsi_panel_mi.h"
#endif

static struct LCM_led_i2c_read_write lcm_led_i2c_read_write = {0};

static struct lm36273_led g_lm36273_led;
static struct lm36273_reg lm36273_regs_conf[] = {
	{ LP36273_DISP_BC1, 0x60 },/* disable pwm*/
	{ LP36273_DISP_BC2, 0x85 },/* disable dimming*/
	{ LP36273_DISP_BIAS_BOOST, 0x24 },/* set LCM_OUT voltage*/
	{ LP36273_DISP_BIAS_VPOS, 0x1e },/* set vsp to +5.5V*/
	{ LP36273_DISP_BIAS_VNEG, 0x1e },/* set vsn to -5.5V*/
};


int lm36273_reg_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = -EINVAL;
	char write_data[2] = { 0 };

	pr_debug("%s addr:0x%x, value: 0x%x\n", __func__, addr, value);

	if (g_lm36273_led.client == NULL) {
		pr_err("ERROR!! lm36273 i2c client is null\n");
		return ret;
	}

	if (addr < LP36273_DISP_REV || addr > LP36273_DISP_PTD_MSB) {
		pr_err("ERROR!! lm36273 addr overflow\n");
		return ret;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(g_lm36273_led.client, write_data, 2);
	if (ret < 0)
		pr_err("lm36273 write data fail !!\n");

	return ret;
}
EXPORT_SYMBOL(lm36273_reg_write_bytes);

int lm36273_reg_read_bytes(char addr, char *buf)
{
	int ret = -EINVAL;
	char puReadCmd[1] = {addr};

	pr_debug("%s addr:0x%x", __func__, addr);
	if (g_lm36273_led.client == NULL && buf == NULL) {
		pr_err("ERROR!! lm36273 i2c client or buffer is null\n");
		return ret;
	}

	if (addr < LP36273_DISP_REV || addr > LP36273_DISP_PTD_MSB) {
		pr_err("ERROR!! lm36273 addr overflow\n");
		return ret;
	}

	ret = i2c_master_send(g_lm36273_led.client, puReadCmd, 1);
	if (ret < 0) {
		pr_err("ERROR!! lm36273 write failed!!\n");
		return ret;
	}

	ret = i2c_master_recv(g_lm36273_led.client, buf, 1);
	if (ret < 0) {
		pr_err("ERROR!! lm36273 read failed!!\n");
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL(lm36273_reg_read_bytes);

int lm36273_bl_bias_conf(void)
{
	int ret, i, reg_count;

	pr_debug("lm36273_bl_bias_conf backlight and bias setting\n");
	mutex_lock(&g_lm36273_led.lock);

	reg_count = ARRAY_SIZE(lm36273_regs_conf) / sizeof(lm36273_regs_conf[0]);
	for (i = 0; i < reg_count; i++)
		ret = lm36273_reg_write_bytes(lm36273_regs_conf[i].reg, lm36273_regs_conf[i].value);

	mutex_unlock(&g_lm36273_led.lock);
	return ret;
}
EXPORT_SYMBOL(lm36273_bl_bias_conf);

int lm36273_bias_enable(int enable, int delayMs)
{
	mutex_lock(&g_lm36273_led.lock);

	if (delayMs > 100)
		delayMs = 100;

	if (enable) {
		/* enable LCD bias VPOS */
		lm36273_reg_write_bytes(LP36273_DISP_BIAS_CONF1, 0x9c);
		mdelay(delayMs);
		/* enable LCD bias VNEG */
		lm36273_reg_write_bytes(LP36273_DISP_BIAS_CONF1, 0x9e);
	} else {
		/* disable LCD bias VNEG */
		lm36273_reg_write_bytes(LP36273_DISP_BIAS_CONF1, 0x9c);
		mdelay(delayMs);
		/* disable LCD bias VPOS */
		lm36273_reg_write_bytes(LP36273_DISP_BIAS_CONF1, 0x98);
		/* bias supply off */
		lm36273_reg_write_bytes(LP36273_DISP_BIAS_CONF1, 0x18);
	}

	mutex_unlock(&g_lm36273_led.lock);
	return 0;
}
EXPORT_SYMBOL(lm36273_bias_enable);

int lm36273_brightness_set(int level)
{
	int tmp_bl = 0;

	if (level < 0 || level > BL_LEVEL_MAX || level == g_lm36273_led.level)
		return 0;

	tmp_bl = bl_level_remap[level];
	mutex_lock(&g_lm36273_led.lock);
	pr_debug("%s lsb:0x%x, msb:0x%x\n", __func__, tmp_bl & 0x7, tmp_bl >> 3);
	lm36273_reg_write_bytes(LP36273_DISP_BB_LSB, tmp_bl & 0x7);
	lm36273_reg_write_bytes(LP36273_DISP_BB_MSB, tmp_bl >> 3);

	if (level == 0 && g_lm36273_led.level != 0) {
		/* disable BL and current sink*/
		lm36273_reg_write_bytes(LP36273_DISP_BL_ENABLE, 0x0);
		g_lm36273_led.hbm_on = 0;
		pr_debug("lm36273_brightness_set, close\n");
	} else if (level > 0 && g_lm36273_led.level == 0) {
		/* enable BL and current sink*/
		lm36273_reg_write_bytes(LP36273_DISP_BL_ENABLE, 0x17);
		lm36273_reg_write_bytes(LP36273_DISP_BC2, 0xcd);
		pr_debug("lm36273_brightness_set, enable level:%d\n", level);
	}

	g_lm36273_led.level = level;
	mutex_unlock(&g_lm36273_led.lock);
	return 0;
}
EXPORT_SYMBOL(lm36273_brightness_set);


int hbm_brightness_set(int level)
{
	int tmp_bl = 0;
	mutex_lock(&g_lm36273_led.lock);

	if (g_lm36273_led.level == BL_LEVEL_MAX) {
		switch (level) {
		case DISPPARAM_LCD_HBM_L1_ON:
			lm36273_reg_write_bytes(LP36273_DISP_BB_LSB, BL_HBM_L1 & 0x7);
			lm36273_reg_write_bytes(LP36273_DISP_BB_MSB, BL_HBM_L1 >> 3);
			g_lm36273_led.hbm_on = 1;
			break;
		case DISPPARAM_LCD_HBM_L2_ON:
			lm36273_reg_write_bytes(LP36273_DISP_BB_LSB, BL_HBM_L2 & 0x7);
			lm36273_reg_write_bytes(LP36273_DISP_BB_MSB, BL_HBM_L2 >> 3);
			g_lm36273_led.hbm_on = 1;
			break;
		case DISPPARAM_LCD_HBM_L3_ON:
			lm36273_reg_write_bytes(LP36273_DISP_BB_LSB, BL_HBM_L3 & 0x7);
			lm36273_reg_write_bytes(LP36273_DISP_BB_MSB, BL_HBM_L3 >> 3);
			g_lm36273_led.hbm_on = 1;
			break;
		case DISPPARAM_LCD_HBM_OFF:
			if (g_lm36273_led.level > 0 && g_lm36273_led.level <= BL_LEVEL_MAX) {
				tmp_bl = bl_level_remap[g_lm36273_led.level];
				lm36273_reg_write_bytes(LP36273_DISP_BB_LSB, tmp_bl & 0x7);
				lm36273_reg_write_bytes(LP36273_DISP_BB_MSB, tmp_bl >> 3);
			}
			g_lm36273_led.hbm_on = 0;
			break;
		default:
			break;
		}
	}

	mutex_unlock(&g_lm36273_led.lock);
	return 0;
}
EXPORT_SYMBOL(hbm_brightness_set);

static int led_i2c_reg_op(char *buffer, int op, int count)
{
	int i, ret = -EINVAL;
	char reg_addr = *buffer;
	char *reg_val = buffer;

	if (reg_val == NULL) {
		pr_err("%s,buffer is null\n", __func__);
		return ret;
	}

	if (op == LM36273_REG_READ) {
		for (i = 0; i < count; i++) {
			ret = lm36273_reg_read_bytes(reg_addr, reg_val);
			if (ret <= 0)
				break;

			reg_addr++;
			reg_val++;
		}
	} else if (op == LM36273_REG_WRITE) {
		ret = lm36273_reg_write_bytes(reg_addr, *(reg_val + 1));
	}

	return ret;
}

static char string_to_hex(const char *str)
{
	char val_l = 0;
	char val_h = 0;

	if (str[0] >= '0' && str[0] <= '9')
		val_h = str[0] - '0';
	else if (str[0] <= 'f' && str[0] >= 'a')
		val_h = 10 + str[0] - 'a';
	else if (str[0] <= 'F' && str[0] >= 'A')
		val_h = 10 + str[0] - 'A';

	if (str[1] >= '0' && str[1] <= '9')
		val_l = str[1]-'0';
	else if (str[1] <= 'f' && str[1] >= 'a')
		val_l = 10 + str[1] - 'a';
	else if (str[1] <= 'F' && str[1] >= 'A')
		val_l = 10 + str[1] - 'A';

	return (val_h << 4) | val_l;
}

static long led_i2c_reg_write(char *buf, unsigned long  count)
{
	int retval = -EINVAL;
	unsigned int read_enable = 0;
	unsigned int packet_count = 0;
	char register_addr = 0;
	char *input = NULL;
	unsigned char pbuf[3] = {0};

	pr_info("[%s], count  = %ld, buf = %s ", __func__, count, buf);

	if (count < 9 || buf == NULL) {
		/* 01 01 01      -- read 0x01 register, len:1*/
		/* 00 01 08 17 -- write 0x17 to 0x08 register,*/
		pr_err("[%s], command is invalid, count  = %ld,buf = %s ", __func__, count, buf);
		return retval;
	}


	input = buf;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	retval = kstrtou32(pbuf, 10, &read_enable);
	if (retval)
		return retval;
	lcm_led_i2c_read_write.read_enable = !!read_enable;
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	packet_count = (unsigned int)string_to_hex(pbuf);
	if (lcm_led_i2c_read_write.read_enable && !packet_count) {
		retval = -EINVAL;
		return retval;
	}
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	register_addr = string_to_hex(pbuf);
	if (lcm_led_i2c_read_write.read_enable) {
		lcm_led_i2c_read_write.read_count = packet_count;
		memset(lcm_led_i2c_read_write.buffer, 0, sizeof(lcm_led_i2c_read_write.buffer));
		lcm_led_i2c_read_write.buffer[0] = (unsigned char)register_addr;

		retval = led_i2c_reg_op(lcm_led_i2c_read_write.buffer, LM36273_REG_READ, lcm_led_i2c_read_write.read_count);
	} else {
		if (count < 12)
			return retval;

		memset(lcm_led_i2c_read_write.buffer, 0, sizeof(lcm_led_i2c_read_write.buffer));
		lcm_led_i2c_read_write.buffer[0] = (unsigned char)register_addr;
		input = input + 3;
		memcpy(pbuf, input, 2);
		pbuf[2] = '\0';
		lcm_led_i2c_read_write.buffer[1] = (unsigned char)string_to_hex(pbuf);

		retval = led_i2c_reg_op(lcm_led_i2c_read_write.buffer, LM36273_REG_WRITE, 0);

	}

	return retval;
}

static long led_i2c_reg_read(char *buf)
{
	int i = 0;
	ssize_t count = 0;

	if (lcm_led_i2c_read_write.read_enable) {
		for (i = 0; i < lcm_led_i2c_read_write.read_count; i++) {
			if (i ==  lcm_led_i2c_read_write.read_count - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x\n",
				     lcm_led_i2c_read_write.buffer[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x ",
				     lcm_led_i2c_read_write.buffer[i]);
			}
		}
	}
	return count;
}

static ssize_t led_i2c_reg_show(struct device *device,
			    struct device_attribute *attr,
			   char *buf)
{
	return led_i2c_reg_read(buf);
}

static ssize_t led_i2c_reg_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int rc = 0;
	rc = led_i2c_reg_write((char *)buf, count);
	return rc;
}

static DEVICE_ATTR_RW(led_i2c_reg);

static struct attribute *lm36273_attrs[] = {
	&dev_attr_led_i2c_reg.attr,
	NULL
};

static const struct attribute_group lm36273_attr_group = {
	.attrs = lm36273_attrs,
};

static int lm36273_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	pr_debug("lm36273_probe: %s\n", client->name);
	g_lm36273_led.client = client;
	mutex_init(&g_lm36273_led.lock);
	g_lm36273_led.hbm_on = 0;
	
	pr_debug("lm36273_sysfs create: %s\n", client->name);
	ret = sysfs_create_group(&client->dev.kobj, &lm36273_attr_group);
	if (ret < 0) {
		pr_err("%s sysfs_create_group failed, ret: %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int lm36273_remove(struct i2c_client *client)
{
	i2c_unregister_device(client);

	return 0;
}

static const struct i2c_device_id lm36273_id[] = {
	{ "I2C_LCD_BIAS", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm36273_id);

static const struct of_device_id of_lm36273_i2c_match[] = {
	{ .compatible = "mediatek,I2C_LCD_BIAS", },
	{},
};
MODULE_DEVICE_TABLE(of, of_lm36273_i2c_match);

static struct i2c_driver lm36273_i2c_driver = {
	.driver = {
		.name	= "I2C_LCD_BIAS",
		.of_match_table = of_match_ptr(of_lm36273_i2c_match),
	},
	.probe		= lm36273_probe,
	.remove		= lm36273_remove,
	.id_table	= lm36273_id,
};
module_i2c_driver(lm36273_i2c_driver);
