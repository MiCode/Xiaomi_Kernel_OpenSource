#ifndef AW2028_LED
#define AW2028_LED

#include <linux/platform_device.h>
#include <linux/leds.h>

#define AW2028_I2C_NAME		"AW2028_LED"
#define AW2028_I2C_BUS		0
#define AW2028_I2C_ADDR		0x65

struct aw2028_led_info {
	struct led_classdev led;
	uint32_t magic_code;
};

struct aw2028_rgbled_info {
	struct aw2028_led_info l_info; /* most be the first member */
	struct device *dev;
	int index;
};
enum {
	AW2028_LED_1,
	AW2028_LED_2,
	AW2028_LED_3,
	AW2028_LED_MAX,
};

enum {
	MT_LED_CC_MODE,
	MT_LED_PWM_MODE,
	MT_LED_BREATH_MODE,
	MT_LED_MODE_MAX,
};

struct aw2028_led_platform_data {
	const char *led_name[AW2028_LED_MAX];
	const char *led_default_trigger[AW2028_LED_MAX];
};

struct aw2028_val_prop {
	const char *name;
	size_t offset;
};

struct i2c_client *AW2028_i2c_client;

static ssize_t AW2028_get_reg(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t AW2028_set_reg(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t AW2028_set_debug(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t AW2028_get_debug(struct device* cd, struct device_attribute *attr, char* buf);
static void aw2028_led_bright_set(struct led_classdev *led, enum led_brightness bright);
static enum led_brightness aw2028_led_bright_get(struct led_classdev *led);
static int aw2028_led_blink_set(struct led_classdev *led, unsigned long *delay_on, unsigned long *delay_off);

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, AW2028_get_reg,  AW2028_set_reg);
static DEVICE_ATTR(debug, S_IWUSR | S_IRUGO, AW2028_get_debug,  AW2028_set_debug);

#endif
