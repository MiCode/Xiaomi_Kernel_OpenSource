/*
 * driver/mfd/tps6591x.c
 *
 * Core driver for TI TPS6591x PMIC family
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>

#include <linux/mfd/core.h>
#include <linux/mfd/tps6591x.h>

/* device control registers */
#define TPS6591X_DEVCTRL	0x3F
#define DEVCTRL_PWR_OFF_SEQ	(1 << 7)
#define DEVCTRL_DEV_ON		(1 << 2)
#define DEVCTRL_DEV_SLP		(1 << 1)
#define TPS6591X_DEVCTRL2	0x40

/* device sleep on registers */
#define TPS6591X_SLEEP_KEEP_ON	0x42
#define SLEEP_KEEP_ON_THERM	(1 << 7)
#define SLEEP_KEEP_ON_CLKOUT32K	(1 << 6)
#define SLEEP_KEEP_ON_VRTC	(1 << 5)
#define SLEEP_KEEP_ON_I2CHS	(1 << 4)

/* interrupt status registers */
#define TPS6591X_INT_STS	0x50
#define TPS6591X_INT_STS2	0x52
#define TPS6591X_INT_STS3	0x54

/* interrupt mask registers */
#define TPS6591X_INT_MSK	0x51
#define TPS6591X_INT_MSK2	0x53
#define TPS6591X_INT_MSK3	0x55

/* GPIO register base address */
#define TPS6591X_GPIO_BASE_ADDR	0x60

/* silicon version number */
#define TPS6591X_VERNUM		0x80

#define TPS6591X_GPIO_SLEEP	7
#define TPS6591X_GPIO_PDEN	3
#define TPS6591X_GPIO_DIR	2

/* pullup register address */
#define TPS6591X_PUADEN_REG_ADDR	0x1C

#define TPS6591X_PIN_MAP(_pin_id, _bit_num)	[_pin_id] = _bit_num

enum irq_type {
	EVENT,
	GPIO,
};

struct tps6591x_irq_data {
	u8		mask_reg;
	u8		mask_pos;
	enum irq_type	type;
};

#define TPS6591X_IRQ(_reg, _mask_pos, _type)	\
	{					\
		.mask_reg	= (_reg),	\
		.mask_pos	= (_mask_pos),	\
		.type		= (_type),	\
	}

static const struct tps6591x_irq_data tps6591x_irqs[] = {
	[TPS6591X_INT_PWRHOLD_F]	= TPS6591X_IRQ(0, 0, EVENT),
	[TPS6591X_INT_VMBHI]		= TPS6591X_IRQ(0, 1, EVENT),
	[TPS6591X_INT_PWRON]		= TPS6591X_IRQ(0, 2, EVENT),
	[TPS6591X_INT_PWRON_LP]		= TPS6591X_IRQ(0, 3, EVENT),
	[TPS6591X_INT_PWRHOLD_R]	= TPS6591X_IRQ(0, 4, EVENT),
	[TPS6591X_INT_HOTDIE]		= TPS6591X_IRQ(0, 5, EVENT),
	[TPS6591X_INT_RTC_ALARM]	= TPS6591X_IRQ(0, 6, EVENT),
	[TPS6591X_INT_RTC_PERIOD]	= TPS6591X_IRQ(0, 7, EVENT),
	[TPS6591X_INT_GPIO0]		= TPS6591X_IRQ(1, 0, GPIO),
	[TPS6591X_INT_GPIO1]		= TPS6591X_IRQ(1, 2, GPIO),
	[TPS6591X_INT_GPIO2]		= TPS6591X_IRQ(1, 4, GPIO),
	[TPS6591X_INT_GPIO3]		= TPS6591X_IRQ(1, 6, GPIO),
	[TPS6591X_INT_GPIO4]		= TPS6591X_IRQ(2, 0, GPIO),
	[TPS6591X_INT_GPIO5]		= TPS6591X_IRQ(2, 2, GPIO),
	[TPS6591X_INT_WTCHDG]		= TPS6591X_IRQ(2, 4, EVENT),
	[TPS6591X_INT_VMBCH2_H]		= TPS6591X_IRQ(2, 5, EVENT),
	[TPS6591X_INT_VMBCH2_L]		= TPS6591X_IRQ(2, 6, EVENT),
	[TPS6591X_INT_PWRDN]		= TPS6591X_IRQ(2, 7, EVENT),
};

static u8 tps6591x_pin_map[] = {
	TPS6591X_PIN_MAP(TPS6591X_PUP_NRESPWRON2P, 0),
	TPS6591X_PIN_MAP(TPS6591X_PUP_HDRSTP, 1),
	TPS6591X_PIN_MAP(TPS6591X_PUP_PWRHOLDP, 2),
	TPS6591X_PIN_MAP(TPS6591X_PUP_SLEEPP, 3),
	TPS6591X_PIN_MAP(TPS6591X_PUP_PWRONP, 4),
	TPS6591X_PIN_MAP(TPS6591X_PUP_I2CSRP, 5),
	TPS6591X_PIN_MAP(TPS6591X_PUP_I2CCTLP, 6),
};

struct tps6591x {
	struct mutex		lock;
	struct device		*dev;
	struct i2c_client	*client;

	struct gpio_chip	gpio;
	struct irq_chip		irq_chip;
	struct mutex		irq_lock;
	int			irq_base;
	int			irq_main;
	u32			irq_en;
	u8			mask_cache[3];
	u8			mask_reg[3];
};

static inline int __tps6591x_read(struct i2c_client *client,
				  int reg, uint8_t *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "failed reading at 0x%02x\n", reg);
		return ret;
	}

	*val = (uint8_t)ret;

	return 0;
}

static inline int __tps6591x_reads(struct i2c_client *client, int reg,
				int len, uint8_t *val)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, len, val);
	if (ret < 0) {
		dev_err(&client->dev, "failed reading from 0x%02x\n", reg);
		return ret;
	}

	return 0;
}

static inline int __tps6591x_write(struct i2c_client *client,
				 int reg, uint8_t val)
{
	int ret;
	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		dev_err(&client->dev, "failed writing 0x%02x to 0x%02x\n",
				val, reg);
		return ret;
	}

	return 0;
}

static inline int __tps6591x_writes(struct i2c_client *client, int reg,
				  int len, uint8_t *val)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, len, val);
	if (ret < 0) {
		dev_err(&client->dev, "failed writings to 0x%02x\n", reg);
		return ret;
	}

	return 0;
}

int tps6591x_write(struct device *dev, int reg, uint8_t val)
{
	struct tps6591x *tps6591x = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&tps6591x->lock);
	ret = __tps6591x_write(to_i2c_client(dev), reg, val);
	mutex_unlock(&tps6591x->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(tps6591x_write);

int tps6591x_writes(struct device *dev, int reg, int len, uint8_t *val)
{
	struct tps6591x *tps6591x = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&tps6591x->lock);
	ret = __tps6591x_writes(to_i2c_client(dev), reg, len, val);
	mutex_unlock(&tps6591x->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(tps6591x_writes);

int tps6591x_read(struct device *dev, int reg, uint8_t *val)
{
	return __tps6591x_read(to_i2c_client(dev), reg, val);
}
EXPORT_SYMBOL_GPL(tps6591x_read);

int tps6591x_reads(struct device *dev, int reg, int len, uint8_t *val)
{
	return __tps6591x_reads(to_i2c_client(dev), reg, len, val);
}
EXPORT_SYMBOL_GPL(tps6591x_reads);

int tps6591x_set_bits(struct device *dev, int reg, uint8_t bit_mask)
{
	struct tps6591x *tps6591x = dev_get_drvdata(dev);
	uint8_t reg_val;
	int ret = 0;

	mutex_lock(&tps6591x->lock);

	ret = __tps6591x_read(to_i2c_client(dev), reg, &reg_val);
	if (ret)
		goto out;

	if ((reg_val & bit_mask) != bit_mask) {
		reg_val |= bit_mask;
		ret = __tps6591x_write(to_i2c_client(dev), reg, reg_val);
	}
out:
	mutex_unlock(&tps6591x->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(tps6591x_set_bits);

int tps6591x_clr_bits(struct device *dev, int reg, uint8_t bit_mask)
{
	struct tps6591x *tps6591x = dev_get_drvdata(dev);
	uint8_t reg_val;
	int ret = 0;

	mutex_lock(&tps6591x->lock);

	ret = __tps6591x_read(to_i2c_client(dev), reg, &reg_val);
	if (ret)
		goto out;

	if (reg_val & bit_mask) {
		reg_val &= ~bit_mask;
		ret = __tps6591x_write(to_i2c_client(dev), reg, reg_val);
	}
out:
	mutex_unlock(&tps6591x->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(tps6591x_clr_bits);

int tps6591x_update(struct device *dev, int reg, uint8_t val, uint8_t mask)
{
	struct tps6591x *tps6591x = dev_get_drvdata(dev);
	uint8_t reg_val;
	int ret = 0;

	mutex_lock(&tps6591x->lock);

	ret = __tps6591x_read(tps6591x->client, reg, &reg_val);
	if (ret)
		goto out;

	if ((reg_val & mask) != val) {
		reg_val = (reg_val & ~mask) | (val & mask);
		ret = __tps6591x_write(tps6591x->client, reg, reg_val);
	}
out:
	mutex_unlock(&tps6591x->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(tps6591x_update);

static struct i2c_client *tps6591x_i2c_client;
static void tps6591x_power_off(void)
{
	struct device *dev = NULL;

	if (!tps6591x_i2c_client)
		return;

	dev = &tps6591x_i2c_client->dev;

	if (tps6591x_set_bits(dev, TPS6591X_DEVCTRL, DEVCTRL_PWR_OFF_SEQ) < 0)
		return;

	tps6591x_clr_bits(dev, TPS6591X_DEVCTRL, DEVCTRL_DEV_ON);
}

static int tps6591x_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	struct tps6591x *tps6591x = container_of(gc, struct tps6591x, gpio);
	uint8_t val;
	int ret;

	ret = __tps6591x_read(tps6591x->client, TPS6591X_GPIO_BASE_ADDR +
			offset,	&val);
	if (ret)
		return ret;

	if (val & 0x4)
		return val & 0x1;
	else
		return (val & 0x2) ? 1 : 0;
}

static void tps6591x_gpio_set(struct gpio_chip *chip, unsigned offset,
			int value)
{

	struct tps6591x *tps6591x = container_of(chip, struct tps6591x, gpio);

	tps6591x_update(tps6591x->dev, TPS6591X_GPIO_BASE_ADDR + offset,
			value, 0x1);
}

static int tps6591x_gpio_input(struct gpio_chip *gc, unsigned offset)
{
	struct tps6591x *tps6591x = container_of(gc, struct tps6591x, gpio);
	uint8_t reg_val;
	int ret;

	ret = __tps6591x_read(tps6591x->client, TPS6591X_GPIO_BASE_ADDR +
			offset,	&reg_val);
	if (ret)
		return ret;

	reg_val &= ~0x4;
	return __tps6591x_write(tps6591x->client, TPS6591X_GPIO_BASE_ADDR +
			offset,	reg_val);
}

static int tps6591x_gpio_output(struct gpio_chip *gc, unsigned offset,
				int value)
{
	struct tps6591x *tps6591x = container_of(gc, struct tps6591x, gpio);
	uint8_t reg_val, val;
	int ret;

	ret = __tps6591x_read(tps6591x->client, TPS6591X_GPIO_BASE_ADDR +
			offset,	&reg_val);
	if (ret)
		return ret;

	reg_val &= ~0x1;
	val = (value & 0x1) | 0x4;
	reg_val = reg_val | val;
	return __tps6591x_write(tps6591x->client, TPS6591X_GPIO_BASE_ADDR +
			offset,	reg_val);
}

static int tps6591x_gpio_to_irq(struct gpio_chip *gc, unsigned off)
{
	struct tps6591x *tps6591x;
	tps6591x = container_of(gc, struct tps6591x, gpio);

	if ((off >= 0) && (off <= TPS6591X_INT_GPIO5 - TPS6591X_INT_GPIO0))
		return tps6591x->irq_base + TPS6591X_INT_GPIO0 + off;

	return -EIO;
}

static void tps6591x_gpio_init(struct tps6591x *tps6591x,
			struct tps6591x_platform_data *pdata)
{
	int ret;
	int gpio_base = pdata->gpio_base;
	int i;
	u8 gpio_reg;
	struct tps6591x_gpio_init_data *ginit;

	if (gpio_base <= 0)
		return;

	for (i = 0; i < pdata->num_gpioinit_data; ++i) {
		ginit = &pdata->gpio_init_data[i];
		if (!ginit->init_apply)
			continue;
		gpio_reg = (ginit->sleep_en << TPS6591X_GPIO_SLEEP) |
				(ginit->pulldn_en << TPS6591X_GPIO_PDEN) |
				(ginit->output_mode_en << TPS6591X_GPIO_DIR);

		if (ginit->output_mode_en)
			gpio_reg |= ginit->output_val;

		ret =  __tps6591x_write(tps6591x->client,
				TPS6591X_GPIO_BASE_ADDR + i, gpio_reg);
		if (ret < 0)
			dev_err(&tps6591x->client->dev, "Gpio %d init "
				"configuration failed: %d\n", i, ret);
	}

	tps6591x->gpio.owner		= THIS_MODULE;
	tps6591x->gpio.label		= tps6591x->client->name;
	tps6591x->gpio.dev		= tps6591x->dev;
	tps6591x->gpio.base		= gpio_base;
	tps6591x->gpio.ngpio		= TPS6591X_GPIO_NR;
	tps6591x->gpio.can_sleep	= 1;

	tps6591x->gpio.direction_input	= tps6591x_gpio_input;
	tps6591x->gpio.direction_output	= tps6591x_gpio_output;
	tps6591x->gpio.set		= tps6591x_gpio_set;
	tps6591x->gpio.get		= tps6591x_gpio_get;
	tps6591x->gpio.to_irq		= tps6591x_gpio_to_irq;

	ret = gpiochip_add(&tps6591x->gpio);
	if (ret)
		dev_warn(tps6591x->dev, "GPIO registration failed: %d\n", ret);
}

static int __remove_subdev(struct device *dev, void *unused)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static int tps6591x_remove_subdevs(struct tps6591x *tps6591x)
{
	return device_for_each_child(tps6591x->dev, NULL, __remove_subdev);
}

static void tps6591x_irq_lock(struct irq_data *data)
{
	struct tps6591x *tps6591x = irq_data_get_irq_chip_data(data);

	mutex_lock(&tps6591x->irq_lock);
}

static void tps6591x_irq_mask(struct irq_data *irq_data)
{
	struct tps6591x *tps6591x = irq_data_get_irq_chip_data(irq_data);
	unsigned int __irq = irq_data->irq - tps6591x->irq_base;
	const struct tps6591x_irq_data *data = &tps6591x_irqs[__irq];

	if (data->type == EVENT)
		tps6591x->mask_reg[data->mask_reg] |= (1 << data->mask_pos);
	else
		tps6591x->mask_reg[data->mask_reg] |= (3 << data->mask_pos);

	tps6591x->irq_en &= ~(1 << __irq);
}

static void tps6591x_irq_unmask(struct irq_data *irq_data)
{
	struct tps6591x *tps6591x = irq_data_get_irq_chip_data(irq_data);

	unsigned int __irq = irq_data->irq - tps6591x->irq_base;
	const struct tps6591x_irq_data *data = &tps6591x_irqs[__irq];

	if (data->type == EVENT) {
		tps6591x->mask_reg[data->mask_reg] &= ~(1 << data->mask_pos);
		tps6591x->irq_en |= (1 << __irq);
	}
}

static void tps6591x_irq_sync_unlock(struct irq_data *data)
{
	struct tps6591x *tps6591x = irq_data_get_irq_chip_data(data);
	int i;

	for (i = 0; i < ARRAY_SIZE(tps6591x->mask_reg); i++) {
		if (tps6591x->mask_reg[i] != tps6591x->mask_cache[i]) {
			if (!WARN_ON(tps6591x_write(tps6591x->dev,
						TPS6591X_INT_MSK + 2*i,
						tps6591x->mask_reg[i])))
				tps6591x->mask_cache[i] = tps6591x->mask_reg[i];
		}
	}

	mutex_unlock(&tps6591x->irq_lock);
}

static int tps6591x_irq_set_type(struct irq_data *irq_data, unsigned int type)
{
	struct tps6591x *tps6591x = irq_data_get_irq_chip_data(irq_data);

	unsigned int __irq = irq_data->irq - tps6591x->irq_base;
	const struct tps6591x_irq_data *data = &tps6591x_irqs[__irq];

	if (data->type == GPIO) {
		if (type & IRQ_TYPE_EDGE_FALLING)
			tps6591x->mask_reg[data->mask_reg]
				&= ~(1 << data->mask_pos);
		else
			tps6591x->mask_reg[data->mask_reg]
				|= (1 << data->mask_pos);

		if (type & IRQ_TYPE_EDGE_RISING)
			tps6591x->mask_reg[data->mask_reg]
				&= ~(2 << data->mask_pos);
		else
			tps6591x->mask_reg[data->mask_reg]
				|= (2 << data->mask_pos);

		tps6591x->irq_en |= (1 << __irq);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tps6591x_irq_set_wake(struct irq_data *irq_data, unsigned int on)
{
	struct tps6591x *tps6591x = irq_data_get_irq_chip_data(irq_data);
	return irq_set_irq_wake(tps6591x->irq_main, on);
}
#else
#define tps6591x_irq_set_wake NULL
#endif


static irqreturn_t tps6591x_irq(int irq, void *data)
{
	struct tps6591x *tps6591x = data;
	int ret = 0;
	u8 tmp[3];
	u8 int_ack;
	u32 acks, mask = 0;
	int i;

	for (i = 0; i < 3; i++) {
		ret = tps6591x_read(tps6591x->dev, TPS6591X_INT_STS + 2*i,
				&tmp[i]);
		if (ret < 0) {
			dev_err(tps6591x->dev,
				"failed to read interrupt status\n");
			return IRQ_NONE;
		}
		if (tmp[i]) {
			/* Ack only those interrupts which are enabled */
			int_ack = tmp[i] & (~(tps6591x->mask_cache[i]));
			ret = tps6591x_write(tps6591x->dev,
					TPS6591X_INT_STS + 2*i,	int_ack);
			if (ret < 0) {
				dev_err(tps6591x->dev,
					"failed to write interrupt status\n");
				return IRQ_NONE;
			}
		}
	}

	acks = (tmp[2] << 16) | (tmp[1] << 8) | tmp[0];

	for (i = 0; i < ARRAY_SIZE(tps6591x_irqs); i++) {
		if (tps6591x_irqs[i].type == GPIO)
			mask = (3 << (tps6591x_irqs[i].mask_pos
					+ tps6591x_irqs[i].mask_reg*8));
		else if (tps6591x_irqs[i].type == EVENT)
			mask = (1 << (tps6591x_irqs[i].mask_pos
					+ tps6591x_irqs[i].mask_reg*8));

		if ((acks & mask) && (tps6591x->irq_en & (1 << i)))
			handle_nested_irq(tps6591x->irq_base + i);
	}
	return IRQ_HANDLED;
}

static int __devinit tps6591x_irq_init(struct tps6591x *tps6591x, int irq,
				int irq_base)
{
	int i, ret;

	if (!irq_base) {
		dev_warn(tps6591x->dev, "No interrupt support on IRQ base\n");
		return -EINVAL;
	}

	mutex_init(&tps6591x->irq_lock);

	tps6591x->mask_reg[0] = 0xFF;
	tps6591x->mask_reg[1] = 0xFF;
	tps6591x->mask_reg[2] = 0xFF;
	for (i = 0; i < 3; i++) {
		tps6591x->mask_cache[i] = tps6591x->mask_reg[i];
		tps6591x_write(tps6591x->dev, TPS6591X_INT_MSK + 2*i,
				 tps6591x->mask_cache[i]);
	}

	for (i = 0; i < 3; i++)
		tps6591x_write(tps6591x->dev, TPS6591X_INT_STS + 2*i, 0xff);

	tps6591x->irq_base = irq_base;
	tps6591x->irq_main = irq;

	tps6591x->irq_chip.name = "tps6591x";
	tps6591x->irq_chip.irq_mask = tps6591x_irq_mask;
	tps6591x->irq_chip.irq_unmask = tps6591x_irq_unmask;
	tps6591x->irq_chip.irq_bus_lock = tps6591x_irq_lock;
	tps6591x->irq_chip.irq_bus_sync_unlock = tps6591x_irq_sync_unlock;
	tps6591x->irq_chip.irq_set_type = tps6591x_irq_set_type;
	tps6591x->irq_chip.irq_set_wake = tps6591x_irq_set_wake;

	for (i = 0; i < ARRAY_SIZE(tps6591x_irqs); i++) {
		int __irq = i + tps6591x->irq_base;
		irq_set_chip_data(__irq, tps6591x);
		irq_set_chip_and_handler(__irq, &tps6591x->irq_chip,
					 handle_simple_irq);
		irq_set_nested_thread(__irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(__irq, IRQF_VALID);
#endif
	}

	ret = request_threaded_irq(irq, NULL, tps6591x_irq, IRQF_ONESHOT,
				"tps6591x", tps6591x);
	if (!ret) {
		device_init_wakeup(tps6591x->dev, 1);
		enable_irq_wake(irq);
	}

	return ret;
}

static int __devinit tps6591x_add_subdevs(struct tps6591x *tps6591x,
					  struct tps6591x_platform_data *pdata)
{
	struct tps6591x_subdev_info *subdev;
	struct platform_device *pdev;
	int i, ret = 0;

	for (i = 0; i < pdata->num_subdevs; i++) {
		subdev = &pdata->subdevs[i];

		pdev = platform_device_alloc(subdev->name, subdev->id);

		pdev->dev.parent = tps6591x->dev;
		pdev->dev.platform_data = subdev->platform_data;

		ret = platform_device_add(pdev);
		if (ret)
			goto failed;
	}
	return 0;

failed:
	tps6591x_remove_subdevs(tps6591x);
	return ret;
}
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
static void print_regs(const char *header, struct seq_file *s,
		struct i2c_client *client, int start_offset,
		int end_offset)
{
	uint8_t reg_val;
	int i;
	int ret;

	seq_printf(s, "%s\n", header);
	for (i = start_offset; i <= end_offset; ++i) {
		ret = __tps6591x_read(client, i, &reg_val);
		if (ret >= 0)
			seq_printf(s, "Reg 0x%02x Value 0x%02x\n", i, reg_val);
	}
	seq_printf(s, "------------------\n");
}

static int dbg_tps_show(struct seq_file *s, void *unused)
{
	struct tps6591x *tps = s->private;
	struct i2c_client *client = tps->client;

	seq_printf(s, "TPS6591x Registers\n");
	seq_printf(s, "------------------\n");

	print_regs("Timing Regs",    s, client, 0x0, 0x6);
	print_regs("Alarm Regs",     s, client, 0x8, 0xD);
	print_regs("RTC Regs",       s, client, 0x10, 0x16);
	print_regs("BCK Regs",       s, client, 0x17, 0x1B);
	print_regs("PUADEN Regs",    s, client, 0x18, 0x18);
	print_regs("REF Regs",       s, client, 0x1D, 0x1D);
	print_regs("VDD Regs",       s, client, 0x1E, 0x29);
	print_regs("LDO Regs",       s, client, 0x30, 0x37);
	print_regs("THERM Regs",     s, client, 0x38, 0x38);
	print_regs("BBCH Regs",      s, client, 0x39, 0x39);
	print_regs("DCDCCNTRL Regs", s, client, 0x3E, 0x3E);
	print_regs("DEV_CNTRL Regs", s, client, 0x3F, 0x40);
	print_regs("SLEEP Regs",     s, client, 0x41, 0x44);
	print_regs("EN1 Regs",       s, client, 0x45, 0x48);
	print_regs("INT Regs",       s, client, 0x50, 0x55);
	print_regs("GPIO Regs",      s, client, 0x60, 0x68);
	print_regs("WATCHDOG Regs",  s, client, 0x69, 0x69);
	print_regs("VMBCH Regs",     s, client, 0x6A, 0x6B);
	print_regs("LED_CTRL Regs",  s, client, 0x6c, 0x6D);
	print_regs("PWM_CTRL Regs",  s, client, 0x6E, 0x6F);
	print_regs("SPARE Regs",     s, client, 0x70, 0x70);
	print_regs("VERNUM Regs",    s, client, 0x80, 0x80);
	return 0;
}

static int dbg_tps_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_tps_show, inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= dbg_tps_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void __init tps6591x_debuginit(struct tps6591x *tps)
{
	(void)debugfs_create_file("tps6591x", S_IRUGO, NULL,
			tps, &debug_fops);
}
#else
static void __init tps6591x_debuginit(struct tps6591x *tpsi)
{
	return;
}
#endif

static int __devinit tps6591x_sleepinit(struct tps6591x *tpsi,
					struct tps6591x_platform_data *pdata)
{
	struct device *dev = NULL;
	int ret = 0;

	dev = tpsi->dev;

	if (!pdata->dev_slp_en)
		goto no_err_return;

	/* pmu dev_slp_en is set. Make sure slp_keepon is available before
	 * allowing SLEEP device state */
	if (!pdata->slp_keepon) {
		dev_err(dev, "slp_keepon_data required for slp_en\n");
		goto err_sleep_init;
	}

	/* enabling SLEEP device state */
	ret = tps6591x_set_bits(dev, TPS6591X_DEVCTRL, DEVCTRL_DEV_SLP);
	if (ret < 0) {
		dev_err(dev, "set dev_slp failed: %d\n", ret);
		goto err_sleep_init;
	}

	if (pdata->slp_keepon->therm_keepon) {
		ret = tps6591x_set_bits(dev, TPS6591X_SLEEP_KEEP_ON,
						SLEEP_KEEP_ON_THERM);
		if (ret < 0) {
			dev_err(dev, "set therm_keepon failed: %d\n", ret);
			goto disable_dev_slp;
		}
	}

	if (pdata->slp_keepon->clkout32k_keepon) {
		ret = tps6591x_set_bits(dev, TPS6591X_SLEEP_KEEP_ON,
						SLEEP_KEEP_ON_CLKOUT32K);
		if (ret < 0) {
			dev_err(dev, "set clkout32k_keepon failed: %d\n", ret);
			goto disable_dev_slp;
		}
	}


	if (pdata->slp_keepon->vrtc_keepon) {
		ret = tps6591x_set_bits(dev, TPS6591X_SLEEP_KEEP_ON,
						SLEEP_KEEP_ON_VRTC);
		if (ret < 0) {
			dev_err(dev, "set vrtc_keepon failed: %d\n", ret);
			goto disable_dev_slp;
		}
	}

	if (pdata->slp_keepon->i2chs_keepon) {
		ret = tps6591x_set_bits(dev, TPS6591X_SLEEP_KEEP_ON,
						SLEEP_KEEP_ON_I2CHS);
		if (ret < 0) {
			dev_err(dev, "set i2chs_keepon failed: %d\n", ret);
			goto disable_dev_slp;
		}
	}

no_err_return:
	return 0;

disable_dev_slp:
	tps6591x_clr_bits(dev, TPS6591X_DEVCTRL, DEVCTRL_DEV_SLP);

err_sleep_init:
	return ret;
}

static int __devinit tps6591x_pup_init(struct i2c_client *client,
				struct tps6591x_platform_data *pdata)
{
	int i, ret, pin_id;
	u8 reg_val;

	ret = __tps6591x_read(client, TPS6591X_PUADEN_REG_ADDR, &reg_val);
	if (ret < 0) {
		dev_err(&client->dev, "unable to read pull up register\n");
		return ret;
	}

	for (i = 0; i < pdata->num_pins; i++) {
		pin_id = pdata->pup_data[i].pin_id;
		if (pdata->pup_data[i].pup_val == TPS6591X_PUP_DEFAULT)
			continue;
		else if (pdata->pup_data[i].pup_val == TPS6591X_PUP_EN)
			reg_val |= (1 << tps6591x_pin_map[pin_id]);
		else if (pdata->pup_data[i].pup_val == TPS6591X_PUP_DIS)
			reg_val &= ~(1 << tps6591x_pin_map[pin_id]);
	}

	ret = __tps6591x_write(client, TPS6591X_PUADEN_REG_ADDR, reg_val);
	if (ret < 0) {
		dev_err(&client->dev, "unable to write to pull up register\n");
		return ret;
	}
	return 0;
}

static int __devinit tps6591x_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct tps6591x_platform_data *pdata = client->dev.platform_data;
	struct tps6591x *tps6591x;
	int ret;

	if (!pdata) {
		dev_err(&client->dev, "tps6591x requires platform data\n");
		return -ENOTSUPP;
	}

	ret = i2c_smbus_read_byte_data(client, TPS6591X_VERNUM);
	if (ret < 0) {
		dev_err(&client->dev, "Silicon version number read"
				" failed: %d\n", ret);
		return -EIO;
	}

	dev_info(&client->dev, "VERNUM is %02x\n", ret);

	tps6591x = kzalloc(sizeof(struct tps6591x), GFP_KERNEL);
	if (tps6591x == NULL)
		return -ENOMEM;

	tps6591x->client = client;
	tps6591x->dev = &client->dev;
	i2c_set_clientdata(client, tps6591x);

	mutex_init(&tps6591x->lock);

	if (client->irq) {
		ret = tps6591x_irq_init(tps6591x, client->irq,
					pdata->irq_base);
		if (ret) {
			dev_err(&client->dev, "IRQ init failed: %d\n", ret);
			goto err_irq_init;
		}
	}

	ret = tps6591x_add_subdevs(tps6591x, pdata);
	if (ret) {
		dev_err(&client->dev, "add devices failed: %d\n", ret);
		goto err_add_devs;
	}

	tps6591x_gpio_init(tps6591x, pdata);

	tps6591x_debuginit(tps6591x);

	tps6591x_sleepinit(tps6591x, pdata);

	tps6591x_pup_init(client, pdata);

	if (pdata->use_power_off && !pm_power_off)
		pm_power_off = tps6591x_power_off;

	tps6591x_i2c_client = client;

	return 0;

err_add_devs:
	if (client->irq)
		free_irq(client->irq, tps6591x);
err_irq_init:
	kfree(tps6591x);
	return ret;
}

static int __devexit tps6591x_i2c_remove(struct i2c_client *client)
{
	struct tps6591x *tps6591x = i2c_get_clientdata(client);

	if (client->irq)
		free_irq(client->irq, tps6591x);

	if (gpiochip_remove(&tps6591x->gpio) < 0)
		dev_err(&client->dev, "Error in removing the gpio driver\n");

	kfree(tps6591x);
	return 0;
}
#ifdef CONFIG_PM
static int tps6591x_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	if (client->irq)
		disable_irq(client->irq);
	return 0;
}

static int tps6591x_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	if (client->irq)
		enable_irq(client->irq);
	return 0;
}

static const struct dev_pm_ops tps6591x_pm_ops = {
	.suspend = tps6591x_i2c_suspend,
	.resume = tps6591x_i2c_resume,
};

#endif
static const struct i2c_device_id tps6591x_id_table[] = {
	{ "tps6591x", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, tps6591x_id_table);

static struct i2c_driver tps6591x_driver = {
	.driver	= {
		.name	= "tps6591x",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm 	= &tps6591x_pm_ops,
#endif
	},
	.probe		= tps6591x_i2c_probe,
	.remove		= __devexit_p(tps6591x_i2c_remove),
	.id_table	= tps6591x_id_table,
};

static int __init tps6591x_init(void)
{
	return i2c_add_driver(&tps6591x_driver);
}
subsys_initcall(tps6591x_init);

static void __exit tps6591x_exit(void)
{
	i2c_del_driver(&tps6591x_driver);
}
module_exit(tps6591x_exit);

MODULE_DESCRIPTION("TPS6591X core driver");
MODULE_LICENSE("GPL");
