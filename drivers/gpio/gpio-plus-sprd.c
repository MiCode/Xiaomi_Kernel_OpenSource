// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 */

#include <linux/bitops.h>
#include <linux/gpio/driver.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

/*
 * Registers definitions for GPIO plus controller, gpio plus
 * support 256 pin, 32 interrupt which can route to 8 subsys
 * freely.
 */

/* GPIO_CTL: 256 gpio data conctrol register. */
#define GPIO0_CRL		0x0
#define GPIO_CRL(n)		(GPIO0_CRL + (n) * 0x4)
/* GPIO_CTL bit definitions */
#define BIT_GPIO_DATA		BIT(0)
#define BIT_GPIO_ODATA		BIT(1)
#define BIT_GPIO_ENABLE		BIT(2)
#define BIT_GPIO_DIR		BIT(3)
#define BIT_GPIO_SEC_STATUS		BIT(4)

/* INT_CTL: 32 interrupt control register */
#define INT0_CRL		0x600
#define INT_CRL(n)		(INT0_CRL + (n) * 0x4)
/* bit definitions */
#define BIT_INT_LEVEL		BIT(0)
#define BIT_SLEEP_INT_MODE	BIT(1)
#define BIT_INT_EDG_DET_MODE_MASK	GENMASK(3, 2)
#define BIT_INT_MODE_MASK		GENMASK(5, 4)
#define BIT_DBC_TRG		BIT(6)
#define BIT_DBC_CYCLE_MASK	GENMASK(27, 16)

/* INT_ROUT: 32 interrupt route to which subsys */
#define INT_ROUT0		0xc00
#define INT_ROUT(n)		(INT_ROUT0 + (n) * 0x4)
/* bit definitions */
#define BIT_INT_ROUT_SEL_MASK	GENMASK(2, 0)

/* NT_SOURCE_SEL: 32 interrupt choose which gpio as interrupt source */
#define INT_SOURCE_SEL0		0xd00
#define INT_SOURCE_SEL(n)	(INT_SOURCE_SEL0 + (n) * 0x4)
/* bit definitions */
#define BIT_INT_SOURCE_SEL_MASK	GENMASK(7, 0)

/*8 subsys config interrupt */
#define INT_SYSIF0_EN		0x700
#define INT_SYSIF0_RAW		0x800
#define INT_SYSIF0_MSK		0x900
#define INT_SYSIF0_CLR		0xa00
#define INT_SYSIF0_STATUS	0xb00
#define INT_SYSIF_EN(n)		(INT_SYSIF0_EN + (n) * 0x4)
#define INT_SYSIF_RAW(n)	(INT_SYSIF0_RAW + (n) * 0x4)
#define INT_SYSIF_MSK(n)	(INT_SYSIF0_MSK + (n) * 0x4)
#define INT_SYSIF_CLR(n)	(INT_SYSIF0_CLR + (n) * 0x4)
#define INT_SYSIF_STATUS(n)	(INT_SYSIF0_STATUS + (n) * 0x4)
/* bit definitions */
#define BIT_INT_SYSIF_MASK	GENMASK(31, 0)

/* gpio and interface secure mode control */
#define GPIO_SEC_CTRL		0xe00
#define GPIO_CTRL_SEC0		0xe10
#define GPIO_CTRL_SEC(n)	(GPIO_CTRL_SEC0 + (n) * 0x4)
#define INT_SYSIF_SEC		0xe40

#define SPRD_GPIO_PLUS_NR	256

enum sprd_gpio_plus_type {
	SPRD_GPIO_PLUS_DEBOUNCE = 0,
	SPRD_GPIO_PLUS_EDGE,
	SPRD_GPIO_PLUS_LATCH,
	SPRD_GPIO_PLUS_LEVEL,
	SPRD_GPIO_PLUS_MAX,
};

enum sprd_gpio_plus_subsys {
	SPRD_SYSIF_AP = 0,
	SPRD_SYSIF_AP_SECURE,
	SPRD_SYSIF_CM4,
	SPRD_SYSIF_PUBCP,
	SPRD_SYSIF_WTLCP,
	SPRD_SYSIF_RSV0,
	SPRD_SYSIF_RSV1,
	SPRD_SYSIF_RSV2,
	SPRD_SYSIF_MAX,
};

/* channel base allocation for subsys */
enum sprd_channel_base {
	SPRD_CHANNEL_AP = 0,
	SPRD_CHANNEL_AP_SECURE = 12,
	SPRD_CHANNEL_CM4 = 20,
	SPRD_CHANNEL_PUBCP = 28,
	SPRD_CHANNEL_MAX = 32,
};
#define SPRD_CHANNEL_OUT_RANGE	SPRD_CHANNEL_MAX

/* subsys channel/route configuration, must be modify in different subsys */
#define SPRD_CHANNEL_START	SPRD_CHANNEL_AP
#define SPRD_CHANNEL_END	(SPRD_CHANNEL_AP_SECURE - 1)
#define SPRD_SYSIF_CURRENT	SPRD_SYSIF_AP

/*
 * @irq_active: store attached interrupt channels
 **/
struct sprd_gpio_plus {
	struct gpio_chip chip;
	void __iomem *base;
	spinlock_t lock;
	int irq;
	int irq_active;
};

static void sprd_gpio_plus_update(struct gpio_chip *chip, unsigned int reg,
		       unsigned int mask, unsigned int val)
{
	struct sprd_gpio_plus *sprd_gpio_plus = gpiochip_get_data(chip);
	void __iomem *base = sprd_gpio_plus->base;
	u32 shift = __ffs(mask);
	unsigned long flags;
	u32 tmp;

	spin_lock_irqsave(&sprd_gpio_plus->lock, flags);
	tmp = readl_relaxed(base + reg);

	tmp &= ~mask;
	tmp |= (val << shift) & mask;

	writel_relaxed(tmp, base + reg);
	spin_unlock_irqrestore(&sprd_gpio_plus->lock, flags);
}

static int sprd_gpio_plus_read(struct gpio_chip *chip, unsigned int reg,
		       unsigned int mask)
{
	struct sprd_gpio_plus *sprd_gpio_plus = gpiochip_get_data(chip);
	void __iomem *base = sprd_gpio_plus->base;
	u32 shift = __ffs(mask);
	u32 value;

	value = readl_relaxed(base + reg);
	value &= mask;

	return value >>= shift;
}

static int sprd_gpio_plus_request(struct gpio_chip *chip, unsigned int offset)
{
	sprd_gpio_plus_update(chip, GPIO_CRL(offset), BIT_GPIO_ENABLE, 1);

	return 0;
}

static void sprd_gpio_plus_free(struct gpio_chip *chip, unsigned int offset)
{
	sprd_gpio_plus_update(chip, GPIO_CRL(offset), BIT_GPIO_ENABLE, 0);
}

static int sprd_gpio_plus_direction_input(struct gpio_chip *chip,
				     unsigned int offset)
{
	sprd_gpio_plus_update(chip, GPIO_CRL(offset), BIT_GPIO_DIR, 0);

	return 0;
}

static int sprd_gpio_plus_direction_output(struct gpio_chip *chip,
				      unsigned int offset, int value)
{
	sprd_gpio_plus_update(chip, GPIO_CRL(offset), BIT_GPIO_DIR, 1);
	sprd_gpio_plus_update(chip, GPIO_CRL(offset), BIT_GPIO_DATA, value);

	return 0;
}

static int sprd_gpio_plus_get(struct gpio_chip *chip, unsigned int offset)
{
	return sprd_gpio_plus_read(chip, GPIO_CRL(offset), BIT_GPIO_DATA);
}

static void sprd_gpio_plus_set(struct gpio_chip *chip, unsigned int offset,
			  int value)
{
	sprd_gpio_plus_update(chip, GPIO_CRL(offset), BIT_GPIO_DATA, value);
}

/*
 * find interrupt channel the gpio attached,
 * if gpio is not attached, attach it to a unused interrupt channel.
 **/
static u32 sprd_gpio_plus_to_channel(struct gpio_chip *chip, u32 offset)
{
	int i;
	struct sprd_gpio_plus *sprd_gpio_plus = gpiochip_get_data(chip);
	u32 value;
	unsigned long flags;

	spin_lock_irqsave(&sprd_gpio_plus->lock, flags);
	for (i = SPRD_CHANNEL_START;
		i < SPRD_CHANNEL_START + sprd_gpio_plus->irq_active;
		i++) {
		value = sprd_gpio_plus_read(chip, INT_SOURCE_SEL(i),
			BIT_INT_SOURCE_SEL_MASK);
		if (value == offset) {
			spin_unlock_irqrestore(&sprd_gpio_plus->lock,
				flags);
			return i;
		}
	}

	if (i > SPRD_CHANNEL_END) {
		spin_unlock_irqrestore(&sprd_gpio_plus->lock, flags);
		return SPRD_CHANNEL_OUT_RANGE;
	}

	sprd_gpio_plus->irq_active++;
	spin_unlock_irqrestore(&sprd_gpio_plus->lock, flags);
	sprd_gpio_plus_update(chip, INT_SOURCE_SEL(i),
		BIT_INT_SOURCE_SEL_MASK, offset);
	sprd_gpio_plus_update(chip, INT_ROUT(i),
		BIT_INT_ROUT_SEL_MASK, SPRD_SYSIF_CURRENT);

	return i;
}

static u32 sprd_channel_to_gpio_plus(struct gpio_chip *chip, u32 channel)
{

	return sprd_gpio_plus_read(chip, INT_SOURCE_SEL(channel),
				BIT_INT_SOURCE_SEL_MASK);
}

static int sprd_gpio_plus_set_debounce(struct gpio_chip *chip,
				      unsigned int offset,
				      unsigned int debounce)
{
	u32 channel = sprd_gpio_plus_to_channel(chip, offset);
	u32 value;

	if (channel >= SPRD_CHANNEL_OUT_RANGE) {
		dev_err(chip->parent, "channel:%d exceed max num:%d",
			channel, SPRD_CHANNEL_OUT_RANGE);
		return -EIO;
	}

	value = debounce / 1000;
	sprd_gpio_plus_update(chip, INT_CRL(channel),
				BIT_DBC_CYCLE_MASK, value);
	if (value)
		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_MODE_MASK, SPRD_GPIO_PLUS_DEBOUNCE);
	else
		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_MODE_MASK, SPRD_GPIO_PLUS_LEVEL);

	return 0;
}

static int sprd_gpio_plus_set_config(struct gpio_chip *chip,
					unsigned int offset,
					unsigned long config)
{
	unsigned long param = pinconf_to_config_param(config);
	u32 arg = pinconf_to_config_argument(config);

	if (param == PIN_CONFIG_INPUT_DEBOUNCE)
		return sprd_gpio_plus_set_debounce(chip, offset, arg);

	return -ENOTSUPP;
}

static void sprd_gpio_plus_irq_mask(struct irq_data *data)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	u32 offset = irqd_to_hwirq(data);
	u32 channel = sprd_gpio_plus_to_channel(chip, offset);

	if (channel >= SPRD_CHANNEL_OUT_RANGE) {
		dev_err(chip->parent, "channel:%d exceed max num:%d",
			channel, SPRD_CHANNEL_OUT_RANGE);
		return;
	}
	sprd_gpio_plus_update(chip, INT_SYSIF_EN(SPRD_SYSIF_CURRENT),
		BIT(channel), 0);
}

static void sprd_gpio_plus_irq_ack(struct irq_data *data)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	u32 offset = irqd_to_hwirq(data);
	u32 channel = sprd_gpio_plus_to_channel(chip, offset);

	if (channel >= SPRD_CHANNEL_OUT_RANGE) {
		dev_err(chip->parent, "channel:%d exceed max num:%d",
			channel, SPRD_CHANNEL_OUT_RANGE);
		return;
	}
	sprd_gpio_plus_update(chip, INT_SYSIF_CLR(SPRD_SYSIF_CURRENT),
		BIT(channel), 1);
}

static void sprd_gpio_plus_irq_unmask(struct irq_data *data)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	u32 offset = irqd_to_hwirq(data);
	u32 channel = sprd_gpio_plus_to_channel(chip, offset);
	u32 value;

	if (channel >= SPRD_CHANNEL_OUT_RANGE) {
		dev_err(chip->parent, "channel:%d exceed max num:%d",
			channel, SPRD_CHANNEL_OUT_RANGE);
		return;
	}
	sprd_gpio_plus_update(chip, INT_SYSIF_EN(SPRD_SYSIF_CURRENT),
		BIT(channel), 1);

	value = sprd_gpio_plus_read(chip, INT_CRL(channel),
				BIT_DBC_CYCLE_MASK);
	if (value)
		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_DBC_TRG, 1);
}

static int sprd_gpio_plus_irq_set_type(struct irq_data *data,
				  unsigned int flow_type)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	u32 offset = irqd_to_hwirq(data);
	u32 channel = sprd_gpio_plus_to_channel(chip, offset);
	u32 value, mode;

	switch (flow_type) {
	case IRQ_TYPE_EDGE_RISING:
		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_MODE_MASK, SPRD_GPIO_PLUS_EDGE);

		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_EDG_DET_MODE_MASK, 1);
		irq_set_handler_locked(data, handle_edge_irq);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_MODE_MASK, SPRD_GPIO_PLUS_EDGE);

		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_EDG_DET_MODE_MASK, 0);
		irq_set_handler_locked(data, handle_edge_irq);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_MODE_MASK, SPRD_GPIO_PLUS_EDGE);

		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_EDG_DET_MODE_MASK, 2);
		irq_set_handler_locked(data, handle_edge_irq);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		mode = sprd_gpio_plus_read(chip, INT_CRL(channel),
					BIT_INT_MODE_MASK);
		value = sprd_gpio_plus_read(chip, INT_CRL(channel),
					BIT_DBC_CYCLE_MASK);
		if ((mode != SPRD_GPIO_PLUS_LEVEL) && (value == 0)) {
			sprd_gpio_plus_update(chip, INT_CRL(channel),
				BIT_INT_MODE_MASK, SPRD_GPIO_PLUS_LEVEL);
		} else if ((mode != SPRD_GPIO_PLUS_DEBOUNCE) && (value != 0)) {
			sprd_gpio_plus_update(chip, INT_CRL(channel),
				BIT_INT_MODE_MASK, SPRD_GPIO_PLUS_DEBOUNCE);
		}
		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_LEVEL, 1);
		irq_set_handler_locked(data, handle_level_irq);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		mode = sprd_gpio_plus_read(chip, INT_CRL(channel),
					BIT_INT_MODE_MASK);
		value = sprd_gpio_plus_read(chip, INT_CRL(channel),
					BIT_DBC_CYCLE_MASK);
		if ((mode != SPRD_GPIO_PLUS_LEVEL) && (value == 0)) {
			sprd_gpio_plus_update(chip, INT_CRL(channel),
				BIT_INT_MODE_MASK, SPRD_GPIO_PLUS_LEVEL);
		} else if ((mode != SPRD_GPIO_PLUS_DEBOUNCE) && (value != 0)) {
			sprd_gpio_plus_update(chip, INT_CRL(channel),
				BIT_INT_MODE_MASK, SPRD_GPIO_PLUS_DEBOUNCE);
		}
		sprd_gpio_plus_update(chip, INT_CRL(channel),
			BIT_INT_LEVEL, 0);
		irq_set_handler_locked(data, handle_level_irq);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void sprd_gpio_plus_irq_handler(struct irq_desc *desc)
{
	struct gpio_chip *chip = irq_desc_get_handler_data(desc);
	struct irq_chip *ic = irq_desc_get_chip(desc);
	u32 n, girq, gpio;
	unsigned long reg;

	chained_irq_enter(ic, desc);

	reg = sprd_gpio_plus_read(chip, INT_SYSIF_MSK(SPRD_SYSIF_CURRENT),
		BIT_INT_SYSIF_MASK);
	n = SPRD_CHANNEL_START;
	for_each_set_bit_from(n, &reg, SPRD_CHANNEL_END) {
		gpio = sprd_channel_to_gpio_plus(chip, n);
		girq = irq_find_mapping(chip->irq.domain, gpio);
		generic_handle_irq(girq);
	}

	chained_irq_exit(ic, desc);
}

static struct irq_chip sprd_gpio_plus_irqchip = {
	.name = "sprd-gpio-plus",
	.irq_ack = sprd_gpio_plus_irq_ack,
	.irq_mask = sprd_gpio_plus_irq_mask,
	.irq_unmask = sprd_gpio_plus_irq_unmask,
	.irq_set_type = sprd_gpio_plus_irq_set_type,
	.flags = IRQCHIP_SKIP_SET_WAKE,
};

static int sprd_gpio_plus_probe(struct platform_device *pdev)
{
	struct gpio_irq_chip *irq;
	struct sprd_gpio_plus *sprd_gpio_plus;
	struct resource *res;
	int ret;

	sprd_gpio_plus = devm_kzalloc(&pdev->dev,
				sizeof(*sprd_gpio_plus), GFP_KERNEL);
	if (!sprd_gpio_plus)
		return -ENOMEM;

	sprd_gpio_plus->irq = platform_get_irq(pdev, 0);
	if (sprd_gpio_plus->irq < 0) {
		dev_err(&pdev->dev, "Failed to get GPIO interrupt.\n");
		return sprd_gpio_plus->irq;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sprd_gpio_plus->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sprd_gpio_plus->base))
		return PTR_ERR(sprd_gpio_plus->base);

	spin_lock_init(&sprd_gpio_plus->lock);

	sprd_gpio_plus->chip.label = dev_name(&pdev->dev);
	sprd_gpio_plus->chip.ngpio = SPRD_GPIO_PLUS_NR;
	sprd_gpio_plus->chip.base = -1;
	sprd_gpio_plus->chip.parent = &pdev->dev;
	sprd_gpio_plus->chip.of_node = pdev->dev.of_node;
	sprd_gpio_plus->chip.request = sprd_gpio_plus_request;
	sprd_gpio_plus->chip.set_config = sprd_gpio_plus_set_config;
	sprd_gpio_plus->chip.free = sprd_gpio_plus_free;
	sprd_gpio_plus->chip.get = sprd_gpio_plus_get;
	sprd_gpio_plus->chip.set = sprd_gpio_plus_set;
	sprd_gpio_plus->chip.direction_input = sprd_gpio_plus_direction_input;
	sprd_gpio_plus->chip.direction_output = sprd_gpio_plus_direction_output;

	irq = &sprd_gpio_plus->chip.irq;
	irq->chip = &sprd_gpio_plus_irqchip;
	irq->handler = handle_bad_irq;
	irq->default_type = IRQ_TYPE_NONE;
	irq->parent_handler = sprd_gpio_plus_irq_handler;
	irq->parent_handler_data = sprd_gpio_plus;
	irq->num_parents = 1;
	irq->parents = &sprd_gpio_plus->irq;

	ret = devm_gpiochip_add_data(&pdev->dev,
				&sprd_gpio_plus->chip, sprd_gpio_plus);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, sprd_gpio_plus);

	return 0;
}

static const struct of_device_id sprd_gpio_plus_of_match[] = {
	{ .compatible = "sprd,sharkl3-gpio-plus", },
	{ /* end of list */ }
};
MODULE_DEVICE_TABLE(of, sprd_gpio_plus_of_match);

static struct platform_driver sprd_gpio_plus_driver = {
	.probe = sprd_gpio_plus_probe,
	.driver = {
		.name = "sprd-gpio-plus",
		.of_match_table	= sprd_gpio_plus_of_match,
	},
};

module_platform_driver_probe(sprd_gpio_plus_driver, sprd_gpio_plus_probe);

MODULE_DESCRIPTION("Spreadtrum GPIO PLUS driver");
MODULE_LICENSE("GPL v2");
