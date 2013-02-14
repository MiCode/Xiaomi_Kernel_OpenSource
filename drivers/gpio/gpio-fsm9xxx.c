/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/bitops.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/module.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>

/* see 80-VA736-2 Rev C pp 695-751
**
** These are actually the *shadow* gpio registers, since the
** real ones (which allow full access) are only available to the
** ARM9 side of the world.
**
** Since the _BASE need to be page-aligned when we're mapping them
** to virtual addresses, adjust for the additional offset in these
** macros.
*/

#if defined(CONFIG_ARCH_FSM9XXX)
#define MSM_GPIO1_REG(off) (MSM_TLMM_BASE + (off))
#endif

#if defined(CONFIG_ARCH_FSM9XXX)

/* output value */
#define MSM_GPIO_OUT_G(group)  MSM_GPIO1_REG(0x00 + (group) * 4)
#define MSM_GPIO_OUT_N(gpio)   MSM_GPIO_OUT_G((gpio) / 32)
#define MSM_GPIO_OUT_0         MSM_GPIO_OUT_G(0)   /* gpio  31-0   */
#define MSM_GPIO_OUT_1         MSM_GPIO_OUT_G(1)   /* gpio  63-32  */
#define MSM_GPIO_OUT_2         MSM_GPIO_OUT_G(2)   /* gpio  95-64  */
#define MSM_GPIO_OUT_3         MSM_GPIO_OUT_G(3)   /* gpio 127-96  */
#define MSM_GPIO_OUT_4         MSM_GPIO_OUT_G(4)   /* gpio 159-128 */
#define MSM_GPIO_OUT_5         MSM_GPIO_OUT_G(5)   /* gpio 167-160 */

/* same pin map as above, output enable */
#define MSM_GPIO_OE_G(group)   MSM_GPIO1_REG(0x20 + (group) * 4)
#define MSM_GPIO_OE_N(gpio)    MSM_GPIO_OE_G((gpio) / 32)
#define MSM_GPIO_OE_0          MSM_GPIO_OE_G(0)
#define MSM_GPIO_OE_1          MSM_GPIO_OE_G(1)
#define MSM_GPIO_OE_2          MSM_GPIO_OE_G(2)
#define MSM_GPIO_OE_3          MSM_GPIO_OE_G(3)
#define MSM_GPIO_OE_4          MSM_GPIO_OE_G(4)
#define MSM_GPIO_OE_5          MSM_GPIO_OE_G(5)

/* same pin map as above, input read */
#define MSM_GPIO_IN_G(group)   MSM_GPIO1_REG(0x48 + (group) * 4)
#define MSM_GPIO_IN_N(gpio)    MSM_GPIO_IN_G((gpio) / 32)
#define MSM_GPIO_IN_0          MSM_GPIO_IN_G(0)
#define MSM_GPIO_IN_1          MSM_GPIO_IN_G(1)
#define MSM_GPIO_IN_2          MSM_GPIO_IN_G(2)
#define MSM_GPIO_IN_3          MSM_GPIO_IN_G(3)
#define MSM_GPIO_IN_4          MSM_GPIO_IN_G(4)
#define MSM_GPIO_IN_5          MSM_GPIO_IN_G(5)

/* configuration */
#define MSM_GPIO_PAGE          MSM_GPIO1_REG(0x40)
#define MSM_GPIO_CONFIG        MSM_GPIO1_REG(0x44)

#endif /* CONFIG_ARCH_FSM9XXX */

#define MSM_GPIO_BANK(bank, first, last)				\
	{								\
		.regs = {						\
			.out =         MSM_GPIO_OUT_##bank,		\
			.in =          MSM_GPIO_IN_##bank,		\
			.oe =          MSM_GPIO_OE_##bank,		\
		},							\
		.chip = {						\
			.base = (first),				\
			.ngpio = (last) - (first) + 1,			\
			.get = msm_gpio_get,				\
			.set = msm_gpio_set,				\
			.direction_input = msm_gpio_direction_input,	\
			.direction_output = msm_gpio_direction_output,	\
			.request = msm_gpio_request,			\
			.free = msm_gpio_free,				\
		}							\
	}

struct msm_gpio_regs {
	void __iomem *out;
	void __iomem *in;
	void __iomem *oe;
};

struct msm_gpio_chip {
	spinlock_t		lock;
	struct gpio_chip	chip;
	struct msm_gpio_regs	regs;
};

static int msm_gpio_write(struct msm_gpio_chip *msm_chip,
			  unsigned offset, unsigned on)
{
	unsigned mask = BIT(offset);
	unsigned val;

	val = __raw_readl(msm_chip->regs.out);
	if (on)
		__raw_writel(val | mask, msm_chip->regs.out);
	else
		__raw_writel(val & ~mask, msm_chip->regs.out);
	return 0;
}

static int msm_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct msm_gpio_chip *msm_chip;
	unsigned long irq_flags;

	msm_chip = container_of(chip, struct msm_gpio_chip, chip);
	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	__raw_writel(__raw_readl(msm_chip->regs.oe) & ~BIT(offset),
		msm_chip->regs.oe);
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
	return 0;
}

static int
msm_gpio_direction_output(struct gpio_chip *chip, unsigned offset, int value)
{
	struct msm_gpio_chip *msm_chip;
	unsigned long irq_flags;

	msm_chip = container_of(chip, struct msm_gpio_chip, chip);
	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	msm_gpio_write(msm_chip, offset, value);
	__raw_writel(__raw_readl(msm_chip->regs.oe) | BIT(offset),
		msm_chip->regs.oe);
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
	return 0;
}

static int msm_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct msm_gpio_chip *msm_chip;

	msm_chip = container_of(chip, struct msm_gpio_chip, chip);
	return (__raw_readl(msm_chip->regs.in) & (1U << offset)) ? 1 : 0;
}

static void msm_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct msm_gpio_chip *msm_chip;
	unsigned long irq_flags;

	msm_chip = container_of(chip, struct msm_gpio_chip, chip);
	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	msm_gpio_write(msm_chip, offset, value);
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
}

#ifdef CONFIG_MSM_GPIOMUX
static int msm_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	return msm_gpiomux_get(chip->base + offset);
}

static void msm_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	msm_gpiomux_put(chip->base + offset);
}
#else
#define msm_gpio_request NULL
#define msm_gpio_free NULL
#endif

struct msm_gpio_chip msm_gpio_chips[] = {
	MSM_GPIO_BANK(0,   0,  31),
	MSM_GPIO_BANK(1,  32,  63),
	MSM_GPIO_BANK(2,  64,  95),
	MSM_GPIO_BANK(3,  96, 127),
	MSM_GPIO_BANK(4, 128, 159),
	MSM_GPIO_BANK(5, 160, 167),
};

void msm_gpio_enter_sleep(int from_idle)
{
	return;
}

void msm_gpio_exit_sleep(void)
{
	return;
}

static int __init msm_init_gpio(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(msm_gpio_chips); i++) {
		spin_lock_init(&msm_gpio_chips[i].lock);
		gpiochip_add(&msm_gpio_chips[i].chip);
	}

	return 0;
}

postcore_initcall(msm_init_gpio);

int gpio_tlmm_config(unsigned config, unsigned disable)
{
	uint32_t flags;
	unsigned gpio = GPIO_PIN(config);

	if (gpio > NR_MSM_GPIOS)
		return -EINVAL;
	flags = ((GPIO_DRVSTR(config) << 6) & (0x7 << 6)) |
		((GPIO_FUNC(config) << 2) & (0xf << 2)) |
		((GPIO_PULL(config) & 0x3));
	dsb();
	__raw_writel(gpio, MSM_GPIO_PAGE);
	dsb();
	__raw_writel(flags, MSM_GPIO_CONFIG);

	return 0;
}
EXPORT_SYMBOL(gpio_tlmm_config);

int msm_gpios_request_enable(const struct msm_gpio *table, int size)
{
	int rc = msm_gpios_request(table, size);
	if (rc)
		return rc;
	rc = msm_gpios_enable(table, size);
	if (rc)
		msm_gpios_free(table, size);
	return rc;
}
EXPORT_SYMBOL(msm_gpios_request_enable);

void msm_gpios_disable_free(const struct msm_gpio *table, int size)
{
	msm_gpios_disable(table, size);
	msm_gpios_free(table, size);
}
EXPORT_SYMBOL(msm_gpios_disable_free);

int msm_gpios_request(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_request(GPIO_PIN(g->gpio_cfg), g->label);
		if (rc) {
			pr_err("gpio_request(%d) <%s> failed: %d\n",
			       GPIO_PIN(g->gpio_cfg), g->label ?: "?", rc);
			goto err;
		}
	}
	return 0;
err:
	msm_gpios_free(table, i);
	return rc;
}
EXPORT_SYMBOL(msm_gpios_request);

void msm_gpios_free(const struct msm_gpio *table, int size)
{
	int i;
	const struct msm_gpio *g;
	for (i = size-1; i >= 0; i--) {
		g = table + i;
		gpio_free(GPIO_PIN(g->gpio_cfg));
	}
}
EXPORT_SYMBOL(msm_gpios_free);

int msm_gpios_enable(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("gpio_tlmm_config(0x%08x, GPIO_CFG_ENABLE)"
			       " <%s> failed: %d\n",
			       g->gpio_cfg, g->label ?: "?", rc);
			pr_err("pin %d func %d dir %d pull %d drvstr %d\n",
			       GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg),
			       GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg),
			       GPIO_DRVSTR(g->gpio_cfg));
			goto err;
		}
	}
	return 0;
err:
	msm_gpios_disable(table, i);
	return rc;
}
EXPORT_SYMBOL(msm_gpios_enable);

int msm_gpios_disable(const struct msm_gpio *table, int size)
{
	int rc = 0;
	int i;
	const struct msm_gpio *g;
	for (i = size-1; i >= 0; i--) {
		int tmp;
		g = table + i;
		tmp = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_DISABLE);
		if (tmp) {
			pr_err("gpio_tlmm_config(0x%08x, GPIO_CFG_DISABLE)"
			       " <%s> failed: %d\n",
			       g->gpio_cfg, g->label ?: "?", rc);
			pr_err("pin %d func %d dir %d pull %d drvstr %d\n",
			       GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg),
			       GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg),
			       GPIO_DRVSTR(g->gpio_cfg));
			if (!rc)
				rc = tmp;
		}
	}

	return rc;
}
EXPORT_SYMBOL(msm_gpios_disable);

