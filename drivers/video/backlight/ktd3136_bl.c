#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/backlight.h>
#include <linux/pwm.h>
#include <linux/leds.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/bitops.h>
#include <linux/mfd/ktd3136.h>
#include <linux/device.h>
#include <linux/platform_device.h>

struct ktd3137_chip *bkl_chip;

#define KTD_DEBUG

 #ifdef KTD_DEBUG
#define LOG_DBG(fmt, args...) printk(KERN_INFO "[ktd]"fmt"\n", ##args)
#endif

int ktd3137_brightness_table_reg4[256] = {0x01, 0x02, 0x04, 0x04, 0x07,
	0x02, 0x00, 0x06, 0x04, 0x02, 0x03, 0x04, 0x05, 0x06, 0x02,
	0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x04, 0x05,
	0x06, 0x05, 0x03, 0x00, 0x05, 0x02, 0x06, 0x02, 0x06, 0x02,
	0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x06, 0x02,
	0x06, 0x01, 0x04, 0x07, 0x02, 0x05, 0x00, 0x03, 0x06, 0x01,
	0x04, 0x07, 0x02, 0x05, 0x00, 0x03, 0x05, 0x07, 0x01, 0x03,
	0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02,
	0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x07,
	0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x07, 0x05, 0x03, 0x01,
	0x07, 0x05, 0x03, 0x01, 0x07, 0x05, 0x03, 0x01, 0x07, 0x05,
	0x03, 0x00, 0x05, 0x02, 0x07, 0x04, 0x01, 0x06, 0x03, 0x00,
	0x05, 0x02, 0x07, 0x04, 0x01, 0x06, 0x03, 0x00, 0x05, 0x02,
	0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03,
	0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03,
	0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x06, 0x01,
	0x04, 0x07, 0x02, 0x05, 0x00, 0x03, 0x06, 0x01, 0x04, 0x07,
	0x02, 0x05, 0x00, 0x03, 0x06, 0x01, 0x04, 0x07, 0x02, 0x05,
	0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01,
	0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05,
	0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01,
	0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05,
	0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x04, 0x05, 0x06,
	0x07};
int ktd3137_brightness_table_reg5[256] = {0x00, 0x06, 0x0C, 0x11, 0x15,
	0x1A, 0x1E, 0x21, 0x25, 0x29, 0x2C, 0x2F, 0x32, 0x35, 0x38, 0x3A,
	0x3D, 0x3F, 0x42, 0x44, 0x47, 0x49, 0x4C, 0x4E, 0x50, 0x52, 0x54,
	0x56, 0x58, 0x59, 0x5B, 0x5C, 0x5E, 0x5F, 0x61, 0x62, 0x64, 0x65,
	0x67, 0x68, 0x6A, 0x6B, 0x6D, 0x6E, 0x70, 0x71, 0x73, 0x74, 0x75,
	0x77, 0x78, 0x7A, 0x7B, 0x7C, 0x7E, 0x7F, 0x80, 0x82, 0x83, 0x85,
	0x86, 0x87, 0x88, 0x8A, 0x8B, 0x8C, 0x8D, 0x8F, 0x90, 0x91, 0x92,
	0x94, 0x95, 0x96, 0x97, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xAA, 0xAB, 0xAC,
	0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
	0xB8, 0xB9, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC0,
	0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC6, 0xC7, 0xC8, 0xC9, 0xC9,
	0xCA, 0xCB, 0xCC, 0xCC, 0xCD, 0xCE, 0xCF, 0xCF, 0xD0, 0xD1, 0xD2,
	0xD2, 0xD3, 0xD3, 0xD4, 0xD5, 0xD5, 0xD6, 0xD7, 0xD7, 0xD8, 0xD8,
	0xD9, 0xDA, 0xDA, 0xDB, 0xDC, 0xDC, 0xDD, 0xDD, 0xDE, 0xDE, 0xDF,
	0xDF, 0xE0, 0xE0, 0xE1, 0xE1, 0xE2, 0xE2, 0xE3, 0xE3, 0xE4, 0xE4, 0xE5,
	0xE5, 0xE6, 0xE6, 0xE7, 0xE7, 0xE8, 0xE8, 0xE9, 0xE9, 0xEA, 0xEA, 0xEB,
	0xEB, 0xEC, 0xEC, 0xEC, 0xED, 0xED, 0xEE, 0xEE, 0xEE, 0xEF, 0xEF, 0xEF,
	0xF0, 0xF0, 0xF1, 0xF1, 0xF1, 0xF2, 0xF2, 0xF2, 0xF3, 0xF3, 0xF3, 0xF4,
	0xF4, 0xF4, 0xF4, 0xF5, 0xF5, 0xF5, 0xF5, 0xF6, 0xF6, 0xF6, 0xF6, 0xF7,
	0xF7, 0xF7, 0xF7, 0xF8, 0xF8, 0xF8, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xFA,
	0xFA, 0xFA, 0xFA, 0xFB, 0xFB, 0xFB, 0xFB, 0xFC, 0xFC, 0xFC, 0xFC, 0xFD,
	0xFD, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF};

static int ktd3137_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int ktd3137_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	*val = ret;

	//LOG_DBG("Reading 0x%02x=0x%02x\n", reg, *val);
	return ret;
}

static int ktd3137_masked_write(struct i2c_client *client,
					int reg, u8 mask, u8 val)
{
	int rc;
	u8 temp = 0;

	rc = ktd3137_read_reg(client, reg, &temp);
	if (rc < 0) {
		dev_err(&client->dev, "failed to read reg\n");
	} else {
		temp &= ~mask;
		temp |= val & mask;
		rc = ktd3137_write_reg(client, reg, temp);
		if (rc < 0)
			dev_err(&client->dev, "failed to write masked data\n");
	}

	ktd3137_read_reg(client, reg, &temp);
	return rc;
}

static int ktd_find_bit(int x)
{
	int i = 0;

	while ((x = x >> 1))
		i++;

	return i+1;
}

static void ktd_parse_dt(struct device *dev, struct ktd3137_chip *chip)
{
	struct device_node *np = dev->of_node;
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	int rc = 0;
	u32 bl_channel, temp;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return;// -ENOMEM;

	pdata->hwen_gpio = of_get_named_gpio(np, "ktd,hwen-gpio", 0);
	LOG_DBG("hwen --<%d>\n", pdata->hwen_gpio);

	pdata->pwm_mode = of_property_read_bool(np, "ktd,pwm-mode");
	LOG_DBG("pwmmode --<%d>\n", pdata->pwm_mode);

	pdata->using_lsb = of_property_read_bool(np, "ktd,using-lsb");
	LOG_DBG("using_lsb --<%d>\n", pdata->using_lsb);

	if (pdata->using_lsb) {
		pdata->default_brightness = 0x7ff;
		pdata->max_brightness = 2047;
	} else {
		pdata->default_brightness = 0xff;
		pdata->max_brightness = 255;
	}
	rc = of_property_read_u32(np, "ktd,pwm-frequency", &temp);
	if (rc) {
		pr_err("Invalid pwm-frequency!\n");
	} else {
		pdata->pwm_period = temp;
		LOG_DBG("pwm-frequency --<%d>\n", pdata->pwm_period);
	}

	rc = of_property_read_u32(np, "ktd,bl-fscal-led", &temp);
	if (rc) {
		pr_err("Invalid backlight full-scale led current!\n");
	} else {
		pdata->full_scale_led = temp;
		LOG_DBG("full-scale led current --<%d mA>\n",
					pdata->full_scale_led);
	}

	rc = of_property_read_u32(np, "ktd,turn-on-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,,turnon!\n");
	} else {
		pdata->ramp_on_time = temp;
		LOG_DBG("ramp on time --<%d ms>\n", pdata->ramp_on_time);
	}

	rc = of_property_read_u32(np, "ktd,turn-off-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,,turnoff!\n");
	} else {
		pdata->ramp_off_time = temp;
		LOG_DBG("ramp off time --<%d ms>\n", pdata->ramp_off_time);
	}

	rc = of_property_read_u32(np, "ktd,pwm-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid pwm-tarns-dim value!\n");
	} else {
		pdata->pwm_trans_dim = temp;
		LOG_DBG("pwm trnasition dimming  --<%d ms>\n",
					pdata->pwm_trans_dim);
	}

	rc = of_property_read_u32(np, "ktd,i2c-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid i2c-trans-dim value !\n");
	} else {
		pdata->i2c_trans_dim = temp;
		LOG_DBG("i2c transition dimming --<%d ms>\n",
					pdata->i2c_trans_dim);
	}

	rc = of_property_read_u32(np, "ktd,bl-channel", &bl_channel);
	if (rc) {
		pr_err("Invalid channel setup\n");
	} else {
		pdata->channel = bl_channel;
		LOG_DBG("bl-channel --<%x>\n", pdata->channel);
	}

	rc = of_property_read_u32(np, "ktd,ovp-level", &temp);
	if (!rc) {
		pdata->ovp_level = temp;
		LOG_DBG("ovp-level --<%d> --temp <%d>\n",
					pdata->ovp_level, temp);
	} else
		pr_err("Invalid OVP level!\n");

	rc = of_property_read_u32(np, "ktd,switching-frequency", &temp);
	if (!rc) {
		pdata->frequency = temp;
		LOG_DBG("switching frequency --<%d>\n", pdata->frequency);
	} else {
		pr_err("Invalid Frequency value!\n");
	}

	rc = of_property_read_u32(np, "ktd,inductor-current", &temp);
	if (!rc) {
		pdata->induct_current = temp;
		LOG_DBG("inductor current limit --<%d>\n",
					pdata->induct_current);
	} else
		pr_err("invalid induct_current limit\n");

	rc = of_property_read_u32(np, "ktd,flash-timeout", &temp);
	if (!rc) {
		pdata->flash_timeout = temp;
		LOG_DBG("flash timeout --<%d>\n", pdata->flash_timeout);
	} else {
		pr_err("invalid flash-time value!\n");
	}

	rc = of_property_read_u32(np, "ktd,linear_ramp", &temp);
	if (!rc) {
		pdata->linear_ramp = temp;
		LOG_DBG("linear_ramp --<%d>\n", pdata->linear_ramp);
	} else {
		pr_err("invalid linear_ramp value!\n");
	}

	rc = of_property_read_u32(np, "ktd,linear_backlight", &temp);
	if (!rc) {
		pdata->linear_backlight = temp;
		LOG_DBG("linear_backlight --<%d>\n", pdata->linear_backlight);
	} else {
		pr_err("invalid linear_backlight value!\n");
	}

	rc = of_property_read_u32(np, "ktd,flash-current", &temp);
	if (!rc) {
		pdata->flash_current = temp;
		LOG_DBG("flash current --<0x%x>\n", pdata->flash_current);
	} else {
		pr_err("invalid flash current value!\n");
	}

	dev->platform_data = pdata;
}

static int ktd3137_bl_enable_channel(struct ktd3137_chip *chip)
{
	int ret;
	struct ktd3137_bl_pdata *pdata = chip->pdata;

	if (pdata->channel == 0) {
		//default value for mode Register, all channel disabled.
		LOG_DBG("all channels are going to be disabled\n");
		ret = ktd3137_write_reg(chip->client, REG_PWM, 0x18);
	} else if (pdata->channel == 3) {
		LOG_DBG("turn all channel on!\n");
		ret = ktd3137_masked_write(chip->client, REG_PWM, 0x9F, 0x9F);
	} else if (pdata->channel == 2) {
		ret = ktd3137_masked_write(chip->client, REG_PWM, 0x9B, 0x1B);
	}

	return ret;
}

static void ktd3137_pwm_mode_enable(struct ktd3137_chip *chip, bool en)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	u8 value;

	if (en) {
		if (pdata->pwm_mode)
			LOG_DBG("already activated!\n");
		else
			pdata->pwm_mode = en;
		ktd3137_masked_write(chip->client, REG_PWM, 0x80, 0x80);
	} else {
		if (pdata->pwm_mode)
			pdata->pwm_mode = en;
		ktd3137_masked_write(chip->client, REG_PWM, 0x9B, 0x1B);
	}

	ktd3137_read_reg(chip->client, REG_PWM, &value);
	LOG_DBG("register pwm<0x06> current value is --<%x>\n", value);
}

static void ktd3137_get_deviceid(struct ktd3137_chip *chip)
{
	u8 value;

	ktd3137_read_reg(chip->client, REG_DEV_ID, &value);
	LOG_DBG("Device ID is --<0x0%x>\n", (value >> 3));
}

static void ktd3137_ramp_setting(struct ktd3137_chip *chip)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	unsigned int max_time = 16384;
	int temp = 0;

	if (pdata->ramp_on_time == 0) {//512us
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0xf0, 0x00);
		LOG_DBG("rampon time is 0\n");
	} else if (pdata->ramp_on_time > max_time) {
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0xf0, 0xf0);
		LOG_DBG("rampon time is max\n");
	} else {
		temp = ktd_find_bit(pdata->ramp_on_time);
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0xf0, temp<<4);
		LOG_DBG("temp is %d\n", temp);
	}

	if (pdata->ramp_off_time == 0) {//512us
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0x0f, 0x00);
		LOG_DBG("rampoff time is 0\n");
	} else if (pdata->ramp_off_time > max_time) {
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0x0f, 0x0f);
		LOG_DBG("rampoff time is max\n");
	} else {
		temp = ktd_find_bit(pdata->ramp_off_time);
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0x0f, temp);
		LOG_DBG("temp is %d\n", temp);
	}

}

static void ktd3137_transition_ramp(struct ktd3137_chip *chip)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	int reg_i2c, reg_pwm, temp;

	if (pdata->i2c_trans_dim >= 1024) {
		reg_i2c = 0xf;
	} else if (pdata->i2c_trans_dim < 128) {
		reg_i2c = 0x0;
	} else {
		temp = pdata->i2c_trans_dim/64;
		reg_i2c = temp-1;
		LOG_DBG("reg_i2c is --<0x%x>\n", reg_i2c);
	}

	if (pdata->pwm_trans_dim >= 256) {
		reg_pwm = 0x7;
	} else if (pdata->pwm_trans_dim < 4) {
		reg_pwm = 0x0;
	} else {
		temp = ktd_find_bit(pdata->pwm_trans_dim);
		reg_pwm = temp - 2;
		LOG_DBG("temp is %d\n", temp);
	}

	ktd3137_masked_write(chip->client, REG_TRANS_RAMP, 0x70, reg_pwm);
	ktd3137_masked_write(chip->client, REG_TRANS_RAMP, 0x0f, reg_i2c);

}

static void ktd3137_flash_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct ktd3137_chip *chip;
	u8 reg;

	chip = container_of(cdev, struct ktd3137_chip, cdev_flash);

	cancel_delayed_work_sync(&chip->work);
	if (!brightness) // flash off
		return;
	else if (brightness > 15)
		brightness = 0x0f;

	if (chip->pdata->flash_timeout < 100)
		reg = 0x00;
	else if (chip->pdata->flash_timeout > 1500)
		reg = 0x0f;
	else
		reg = (chip->pdata->flash_timeout/100);

	reg = (reg << 4) | brightness;
	LOG_DBG("update register value --<0x%x>\n", reg);
	ktd3137_write_reg(chip->client, REG_FLASH_SETTING, reg);

	ktd3137_masked_write(chip->client, REG_MODE, 0x02, 0x02);

	schedule_delayed_work(&chip->work, chip->pdata->flash_timeout);
}

static int ktd3137_flashled_init(struct i2c_client *client,
				struct ktd3137_chip *chip)
{
	//struct ktd3137_bl_pdata *pdata = chip->pdata;
	int ret;

	chip->cdev_flash.name = "ktd3137_flash";
	chip->cdev_flash.max_brightness = 16;
	chip->cdev_flash.brightness_set = ktd3137_flash_brightness_set;

	ret = led_classdev_register((struct device *) &client->dev,
						&chip->cdev_flash);
	if (ret < 0) {
		dev_err(&client->dev, "failed to register ktd3137_flash\n");
		return ret;
	}

	return 0;
}

static void ktd3137_backlight_init(struct ktd3137_chip *chip)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	u8 value;
	u8 update_value;
	ktd3137_get_deviceid(chip);
	update_value = (pdata->ovp_level == 32) ? 0x20 : 0x00;
	(pdata->induct_current == 2600) ? update_value |= 0x08 : update_value;
	(pdata->frequency == 1000) ? update_value |= 0x40 : update_value;
	(pdata->linear_ramp == 1) ? update_value |= 0x04 : update_value;
	(pdata->linear_backlight == 1) ? update_value |= 0x02 : update_value;
	ktd3137_write_reg(chip->client, REG_CONTROL, update_value);
	ktd3137_bl_enable_channel(chip);

	if (pdata->pwm_mode) {
		ktd3137_pwm_mode_enable(chip, true);
	} else {
		ktd3137_pwm_mode_enable(chip, false);
	}

	ktd3137_ramp_setting(chip);
	ktd3137_transition_ramp(chip);
	ktd3137_read_reg(chip->client, REG_CONTROL, &value);
	ktd3137_masked_write(chip->client, REG_MODE, 0xf8,
					pdata->full_scale_led);

	LOG_DBG("read control register -before--<0x%x> -after--<0x%x>\n",
					update_value, value);
}

static void ktd3137_hwen_pin_ctrl(struct ktd3137_chip *chip, int en)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;

	if (en) {
		LOG_DBG("hwen pin is going to be high!---<%d>\n", en);
		gpio_set_value(pdata->hwen_gpio, true);
	} else {
		LOG_DBG("hwen pin is going to be low!---<%d>\n", en);
		gpio_set_value(pdata->hwen_gpio, false);
	}
}

static void ktd3137_check_status(struct ktd3137_chip *chip)
{
	u8 value = 0;

	ktd3137_read_reg(chip->client, REG_STATUS, &value);
	if (value) {
		LOG_DBG("status bit has been change! <%x>", value);

		if (value & RESET_CONDITION_BITS) {
			ktd3137_hwen_pin_ctrl(chip, 0);
			ktd3137_hwen_pin_ctrl(chip, 1);
			ktd3137_backlight_init(chip);
		}

	}
	return ;//value;
}

static int ktd3137_gpio_init(struct ktd3137_chip *chip)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	int ret;

	if (gpio_is_valid(pdata->hwen_gpio)) {
		ret = gpio_request(pdata->hwen_gpio, "ktd_hwen_gpio");
		if (ret < 0) {
			pr_err("failed to request gpio\n");
			return  -ENOMEM;
		}
		ret = gpio_direction_output(pdata->hwen_gpio, 0);
		if (ret < 0) {
			pr_err("failed to set output");
			gpio_free(pdata->hwen_gpio);
			return ret;
		}
		LOG_DBG("gpio is valid!\n");
		ktd3137_hwen_pin_ctrl(chip, 1);
	}

	return 0;
}

static void ktd3137_pwm_control(struct ktd3137_chip *chip, int brightness)
{
	struct pwm_device *pwm = NULL;
	unsigned int duty, period;

	if (!chip->pwm) {
		pwm = devm_pwm_get(chip->dev, DEFAULT_PWM_NAME);

		if (IS_ERR(pwm)) {
			dev_err(chip->dev, "can't get pwm device\n");
			return;
		}
	}

	if (brightness > chip->pdata->max_brightness)
		brightness = chip->pdata->max_brightness;

	chip->pwm = pwm;
	period = chip->pdata->pwm_period;
	duty = brightness * period / chip->pdata->max_brightness;
	pwm_config(chip->pwm, duty, period);

	if (duty)
		pwm_enable(chip->pwm);
	else
		pwm_disable(chip->pwm);
}

void ktd3137_brightness_set_workfunc(struct ktd3137_chip *chip, int brightness)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	u8 value;

	if (brightness == 0) {
		ktd3137_write_reg(chip->client, 0x07, 0x44);
		ktd3137_write_reg(chip->client, REG_MODE, 0x98);
		mdelay(10);
	} else {
		ktd3137_write_reg(chip->client, REG_MODE, 0x99);
	}

	if (pdata->linear_backlight == 1) {
		ktd3137_masked_write(chip->client, REG_CONTROL, 0x02, 0x02);// set linear mode
	}

	if (pdata->pwm_mode) {
		LOG_DBG("pwm_ctrl is needed\n");
		ktd3137_pwm_control(chip, brightness);
	} else {
		if (brightness > pdata->max_brightness)
			brightness = pdata->max_brightness;
		if (pdata->using_lsb) {
			ktd3137_masked_write(chip->client, REG_RATIO_LSB,
							0x07, brightness);
			ktd3137_masked_write(chip->client, REG_RATIO_MSB,
							0xff, brightness>>3);
		} else {
			ktd3137_masked_write(chip->client, REG_RATIO_LSB, 0x07,
				ktd3137_brightness_table_reg4[brightness]);
			ktd3137_masked_write(chip->client, REG_RATIO_MSB, 0xff,
				ktd3137_brightness_table_reg5[brightness]);
		}
	}

	ktd3137_read_reg(chip->client, 0x02, &value);
	ktd3137_read_reg(chip->client, 0x03, &value);
	ktd3137_read_reg(chip->client, 0x04, &value);
	ktd3137_read_reg(chip->client, 0x05, &value);
	ktd3137_read_reg(chip->client, 0x06, &value);
	ktd3137_read_reg(chip->client, 0x08, &value);
}

int ktd_hbm_set(enum backlight_hbm_mode hbm_mode)
{
	u8 value = 0;
	LOG_DBG("%s enter\n", __func__);

	switch (hbm_mode) {
	case HBM_MODE_DEFAULT:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0x99);
		LOG_DBG("This is hbm mode 1\n");
		break;
	case HBM_MODE_LEVEL1:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xB1);
		LOG_DBG("This is hbm mode 2\n");
		break;
	case HBM_MODE_LEVEL2:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xC9);
		LOG_DBG("This is hbm mode 3\n");
		break;
	default:
		LOG_DBG("This isn't hbm mode\n");
		break;
	 }
	ktd3137_read_reg(bkl_chip->client, 0x02, &value);
	LOG_DBG("[bkl]%s hbm_mode=%d, value = %d\n", __func__, hbm_mode, value);
	return 0;
}

#if defined(PROJECT_OLIVE) || defined(PROJECT_OLIVELITE) || defined(PROJECT_OLIVEWOOD)
#define LOWEST_BRIGHTNESS          8
#endif

int ktd3137_brightness_set(int brightness)
{
	LOG_DBG("%s brightness = %d\n", __func__, brightness);
#if defined(PROJECT_OLIVE) || defined(PROJECT_OLIVELITE) || defined(PROJECT_OLIVEWOOD)
	if ((brightness > 0) && (brightness <= LOWEST_BRIGHTNESS)) {
		brightness = LOWEST_BRIGHTNESS;
	};
	switch (brightness) {
	case LOWEST_BRIGHTNESS:
		brightness = brightness - 1;
		LOG_DBG("%s The lowest brightness = %d\n", __func__, brightness);
		break;
	}
#endif
	ktd3137_brightness_set_workfunc(bkl_chip, brightness);
	return brightness;
}

static int ktd3137_update_brightness(struct backlight_device *bl)
{
	struct ktd3137_chip *chip = bl_get_data(bl);
	int brightness = bl->props.brightness;

	LOG_DBG("current brightness is --<%d>\n", bl->props.brightness);

	cancel_delayed_work_sync(&chip->work);

	if (bl->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
		brightness = 0;

	if (brightness > 0)
		ktd3137_masked_write(chip->client, REG_MODE, 0x01, 0x01);
	else
		ktd3137_masked_write(chip->client, REG_MODE, 0x01, 0x00);

	ktd3137_brightness_set_workfunc(chip, brightness);
	schedule_delayed_work(&chip->work, 100);

	return 0;
}

static const struct backlight_ops ktd3137_backlight_ops = {
	.options    = BL_CORE_SUSPENDRESUME,
	.update_status = ktd3137_update_brightness,
};

static ssize_t ktd3137_bl_chip_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_DEV_ID, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_mode_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_MODE, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_mode_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_MODE, value);

	return count;
}

static ssize_t ktd3137_bl_ctrl_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_CONTROL, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_ctrl_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_CONTROL, value);

	return count;

}

static ssize_t ktd3137_bl_brightness_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint16_t reg_val = 0;
	u8 temp = 0;

	ktd3137_read_reg(chip->client, REG_RATIO_LSB, &temp);
	reg_val = temp << 8;
	ktd3137_read_reg(chip->client, REG_RATIO_MSB, &temp);
	reg_val |= temp;

	return snprintf(buf, 1024, "0x%x\n", reg_val);

}

static ssize_t ktd3137_bl_brightness_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}


	if (chip->pdata->using_lsb) {
		ktd3137_masked_write(chip->client, REG_RATIO_LSB,
							0x07, value);
		ktd3137_masked_write(chip->client, REG_RATIO_MSB,
							0xff, value >> 3);
	} else {
		ktd3137_write_reg(chip->client, REG_RATIO_MSB, value);
	}

	return count;
}

static ssize_t ktd3137_bl_pwm_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_PWM, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_pwm_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_PWM, value);

	return count;
}

static ssize_t ktd3137_bl_ramp_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_RAMP_ON, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_ramp_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_RAMP_ON, value);

	return count;

}

static ssize_t ktd3137_bl_trans_ramp_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_TRANS_RAMP, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_trans_ramp_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_TRANS_RAMP, value);

	return count;

}

static ssize_t ktd3137_bl_flash_setting_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_FLASH_SETTING, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_flash_setting_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_FLASH_SETTING, value);

	return count;
}

static ssize_t ktd3137_bl_status_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_STATUS, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static DEVICE_ATTR(ktd_chip_id, 0444, ktd3137_bl_chip_id_show, NULL);
static DEVICE_ATTR(ktd_mode_reg, 0664, ktd3137_bl_mode_reg_show,
				ktd3137_bl_mode_reg_store);
static DEVICE_ATTR(ktd_ctrl_reg, 0664, ktd3137_bl_ctrl_reg_show,
				ktd3137_bl_ctrl_reg_store);
static DEVICE_ATTR(ktd_brightness_reg, 0664, ktd3137_bl_brightness_reg_show,
				ktd3137_bl_brightness_reg_store);
static DEVICE_ATTR(ktd_pwm_reg, 0664, ktd3137_bl_pwm_reg_show,
				ktd3137_bl_pwm_reg_store);
static DEVICE_ATTR(ktd_ramp_reg, 0664, ktd3137_bl_ramp_reg_show,
				ktd3137_bl_ramp_reg_store);
static DEVICE_ATTR(ktd_trans_ramp_reg, 0664, ktd3137_bl_trans_ramp_reg_show,
				ktd3137_bl_trans_ramp_reg_store);
static DEVICE_ATTR(ktd_flash_setting_reg, 0664,
				ktd3137_bl_flash_setting_reg_show,
				ktd3137_bl_flash_setting_reg_store);
static DEVICE_ATTR(ktd_status_reg, 0444, ktd3137_bl_status_reg_show, NULL);

static struct attribute *ktd3137_bl_attribute[] = {
	&dev_attr_ktd_chip_id.attr,
	&dev_attr_ktd_mode_reg.attr,
	&dev_attr_ktd_ctrl_reg.attr,
	&dev_attr_ktd_brightness_reg.attr,
	&dev_attr_ktd_pwm_reg.attr,
	&dev_attr_ktd_ramp_reg.attr,
	&dev_attr_ktd_trans_ramp_reg.attr,
	&dev_attr_ktd_flash_setting_reg.attr,
	&dev_attr_ktd_status_reg.attr,
	NULL
};

static const struct attribute_group ktd3137_bl_attr_group = {
	.attrs = ktd3137_bl_attribute,
};

static void ktd3137_sync_backlight_work(struct work_struct *work)
{
	struct ktd3137_chip *chip;
	u8 value;

	chip = container_of(work, struct ktd3137_chip, work.work);

	ktd3137_read_reg(chip->client, REG_FLASH_SETTING, &value);
	//LOG_DBG("flash setting register --<0x%x>\n", value);

	ktd3137_read_reg(chip->client, REG_MODE, &value);
	//LOG_DBG("mode register --<0x%x>\n", value);

	ktd3137_check_status(chip);
}

/*static int ktd3137_backlight_add_device(struct i2c_client *client,
				struct ktd3137_chip *chip)
{
	struct backlight_device *bl;
	struct backlight_properties props;
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	int ret;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.brightness = pdata->default_brightness;
	props.max_brightness = pdata->max_brightness;

	bl = devm_backlight_device_register(&client->dev,
			dev_driver_string(&client->dev),
			&client->dev, chip, &ktd3137_backlight_ops, &props);

	if (IS_ERR(bl)) {
		dev_err(&client->dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}

	chip->bl = bl;

	ret = sysfs_create_group(&chip->bl->dev.kobj, &ktd3137_bl_attr_group);
	if (ret) {
		dev_err(&client->dev, "failed to create sysfs group\n");
		return ret;
	}

	return 0;
}*/

static struct class *ktd3137_class;
static atomic_t ktd_dev;
static struct device *ktd3137_dev;

struct device *ktd3137_device_create(void *drvdata, const char *fmt)
{
	struct device *dev;
	if (IS_ERR(ktd3137_class)) {
		pr_err("Failed to create class %ld\n", PTR_ERR(ktd3137_class));
	}

	dev = device_create(ktd3137_class, NULL, atomic_inc_return(&ktd_dev), drvdata, fmt);
	if (IS_ERR(dev)) {
		pr_err("Failed to create device %s %ld\n", fmt, PTR_ERR(dev));
	} else {
		pr_debug("%s : %s : %d\n", __func__, fmt, dev->devt);
	}

	return dev;
}


static int ktd3137_probe(struct i2c_client *client,
			const struct i2c_device_id *id) {

	int err = 0;
	u8 value;

	struct ktd3137_bl_pdata *pdata = dev_get_drvdata(&client->dev);
	struct ktd3137_chip *chip;
	//struct device_node *np = client->dev.of_node;
	extern char *saved_command_line;
	int bkl_id = 0;
	char *bkl_ptr = (char *)strnstr(saved_command_line, ":bklic=", strlen(saved_command_line));
	bkl_ptr += strlen(":bklic=");
	bkl_id = simple_strtol(bkl_ptr, NULL, 10);
	if (bkl_id != 24) {
		return -ENODEV;
	}
	client->addr = 0x36;
	LOG_DBG("probe start!\n");
	if (!pdata) {
		ktd_parse_dt(&client->dev, chip);
		pdata = dev_get_platdata(&client->dev);
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "check_functionality failed.\n");
		err = -ENODEV;
		goto exit0;
	}

	//ktd3137_client = client;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto exit0;
	}

	chip->client = client;
	chip->pdata = pdata;
	chip->dev = &client->dev;

	ktd3137_dev = ktd3137_device_create(chip, "ktd");
	if (IS_ERR(ktd3137_dev)) {
		dev_err(&client->dev, "failed_to create device for ktd");
	}

	err = sysfs_create_group(&ktd3137_dev->kobj, &ktd3137_bl_attr_group);
	if (err) {
		dev_err(&client->dev, "failed to create sysfs group\n");

	}

	i2c_set_clientdata(client, chip);

	/*err = ktd3137_backlight_add_device(client, chip);
	if (err)
		goto exit0;*/

	ktd3137_gpio_init(chip);
	ktd3137_backlight_init(chip);
	INIT_DELAYED_WORK(&chip->work, ktd3137_sync_backlight_work);
	ktd3137_flashled_init(client, chip);
	ktd3137_check_status(chip);
	ktd3137_read_reg(chip->client, 0x02, &value);
	ktd3137_read_reg(chip->client, 0x03, &value);
	ktd3137_read_reg(chip->client, 0x06, &value);
	ktd3137_read_reg(chip->client, 0x07, &value);
	ktd3137_read_reg(chip->client, 0x08, &value);
	ktd3137_read_reg(chip->client, 0x0A, &value);
	//backlight_update_status(chip->bl);
	bkl_chip = chip;
exit0:
	return err;
}

static int ktd3137_remove(struct i2c_client *client)
{
	struct ktd3137_chip *chip = i2c_get_clientdata(client);

	chip->bl->props.brightness = 0;

	backlight_update_status(chip->bl);
	cancel_delayed_work_sync(&chip->work);

	ktd3137_hwen_pin_ctrl(chip, 0);

	sysfs_remove_group(&chip->bl->dev.kobj, &ktd3137_bl_attr_group);
	gpio_free(chip->pdata->hwen_gpio);

	return 0;
}

static const struct i2c_device_id ktd3137_id[] = {
	{KTD_I2C_NAME, 0},
	{ }
};


static const struct of_device_id ktd3137_match_table[] = {
	{ .compatible = "ktd,ktd3137",},
	{ },
};

static struct i2c_driver ktd3137_driver = {
	.driver = {
		.name	= KTD_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ktd3137_match_table,
	},
	.probe = ktd3137_probe,
	.remove = ktd3137_remove,
	.id_table = ktd3137_id,
};

static int __init ktd3137_init(void)
{
	int err;

	ktd3137_class = class_create(THIS_MODULE, "ktd3137");
	if (IS_ERR(ktd3137_class)) {
		pr_err("unable to create ktd3137 class; errno = %ld\n", PTR_ERR(ktd3137_class));
		ktd3137_class = NULL;
	}

	err = i2c_add_driver(&ktd3137_driver);
	if (err) {
		LOG_DBG("ktd3137 driver failed,(errno = %d)\n", err);
	} else {
		LOG_DBG("Successfully added driver %s\n",
			ktd3137_driver.driver.name);
	}
	return err;
}

static void __exit ktd3137_exit(void)
{
	i2c_del_driver(&ktd3137_driver);
}

module_init(ktd3137_init);
module_exit(ktd3137_exit);

MODULE_AUTHOR("kinet-ic.com");
MODULE_LICENSE("GPL");
