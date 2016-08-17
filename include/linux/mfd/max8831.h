#include <linux/regmap.h>

#ifndef __LINUX_MFD_MAX8831
#define __LINUX_MFD_MAX8831
/*LED MAX8831 Registers */
#define MAX8831_CTRL			0x00

#define MAX8831_CTRL_LED1_ENB		1
#define MAX8831_CTRL_LED2_ENB		(1<<1)
#define MAX8831_CTRL_LED3_ENB		(1<<2)
#define MAX8831_CTRL_LED4_ENB		(1<<3)
#define MAX8831_CTRL_LED5_ENB		(1<<4)

#define MAX8831_RAMP_CTRL_LED1		0x03
#define MAX8831_RAMP_CTRL_LED2		0x04
#define MAX8831_RAMP_CTRL_LED3		0x05
#define MAX8831_RAMP_CTRL_LED4		0x06
#define MAX8831_RAMP_CTRL_LED5		0x07

#define MAX8831_CURRENT_CTRL_LED1	0x0B
#define MAX8831_CURRENT_CTRL_LED2	0x0C
#define MAX8831_CURRENT_CTRL_LED3	0x0D
#define MAX8831_CURRENT_CTRL_LED4	0x0E
#define MAX8831_CURRENT_CTRL_LED5	0x0F

#define MAX8831_BL_LEDS_MAX_CURR	0x7F
#define MAX8831_KEY_LEDS_MAX_CURR	0x1F

#define MAX8831_BLINK_CTRL_LED3		0x17
#define MAX8831_BLINK_CTRL_LED4		0x18
#define MAX8831_BLINK_CTRL_LED5		0x19

#define MAX8831_BLINK_ENB		(1<<6)
#define MAX8831_BLINK_OFF_TIMER_SHIFT	3
#define MAX8831_BLINK_ON_TIMER_SHIFT	0

#define MAX8831_BOOST_CTRL		0x1D
#define MAX8831_BOOST_CTRL_LED5		(1<<4)
#define MAX8831_BOOST_CTRL_LED4		(1<<3)
#define MAX8831_BOOST_CTRL_LED3		(1<<2)

#define MAX8831_LEDS_STAT1		0x2D
#define MAX8831_STAT1_LED1_FAULT	1
#define MAX8831_STAT1_LED2_FAULT	(1<<1)
#define MAX8831_STAT1_LED3_FAULT	(1<<2)
#define MAX8831_STAT1_LED4_FAULT	(1<<3)
#define MAX8831_STAT1_LED5_FAULT	(1<<4)

#define MAX8831_LEDS_STAT2		0x2E
#define MAX8831_STAT2_OSOD		(1<<2)
#define MAX8831_STAT2_TSD		(1<<1)
#define MAX8831_STAT2_OVP		1

/* IDs for each of the LEDs */
enum max8831_led_ids {
	MAX8831_ID_LED1,
	MAX8831_ID_LED2,
	MAX8831_ID_LED3,
	MAX8831_ID_LED4,
	MAX8831_ID_LED5,
	MAX8831_BL_LEDS,/* Refers to LED1 and LED2 together for video bl */
};

struct max8831_subdev_info {
	int		id;
	const char	*name;
	void		*platform_data;
	size_t		pdata_size;
};

struct max8831_platform_data {
	int				num_subdevs;
	struct max8831_subdev_info	*subdevs;
};

struct max8831_chip {
	struct i2c_client	*client;
	struct device		*dev;
	struct regmap		*regmap;

};

static inline int max8831_write(struct device *dev,
	unsigned int reg, unsigned int val)
{
	struct max8831_chip *chip = dev_get_drvdata(dev);
	return regmap_write(chip->regmap, reg, val);
}

static inline int max8831_read(struct device *dev,
	unsigned int reg, unsigned int *val)
{
	struct max8831_chip *chip = dev_get_drvdata(dev);
	return regmap_read(chip->regmap, reg, (unsigned int *) val);
}

static inline int max8831_update_bits(struct device *dev,
	unsigned int reg,
	unsigned int bit_mask,
	unsigned int val)
{
	struct max8831_chip *chip = dev_get_drvdata(dev);
	return regmap_update_bits(chip->regmap, reg, bit_mask, val);
}

#endif
