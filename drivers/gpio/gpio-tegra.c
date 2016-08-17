/*
 * arch/arm/mach-tegra/gpio.c
 *
 * Copyright (c) 2010 Google, Inc
 *
 * Author:
 *	Erik Gilling <konkers@google.com>
 *
 * Copyright (c) 2011-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/syscore_ops.h>
#include <linux/irqdomain.h>

#include <asm/mach/irq.h>

#include <mach/gpio-tegra.h>
#include <mach/iomap.h>
#include <mach/legacy_irq.h>
#include <mach/pinmux.h>

#include "../../arch/arm/mach-tegra/pm-irq.h"

#define GPIO_BANK(x)		((x) >> 5)
#define GPIO_PORT(x)		(((x) >> 3) & 0x3)
#define GPIO_BIT(x)		((x) & 0x7)

#define GPIO_REG(x)		(GPIO_BANK(x) * tegra_gpio_bank_stride + \
					GPIO_PORT(x) * 4)

#define GPIO_CNF(x)		(GPIO_REG(x) + 0x00)
#define GPIO_OE(x)		(GPIO_REG(x) + 0x10)
#define GPIO_OUT(x)		(GPIO_REG(x) + 0X20)
#define GPIO_IN(x)		(GPIO_REG(x) + 0x30)
#define GPIO_INT_STA(x)		(GPIO_REG(x) + 0x40)
#define GPIO_INT_ENB(x)		(GPIO_REG(x) + 0x50)
#define GPIO_INT_LVL(x)		(GPIO_REG(x) + 0x60)
#define GPIO_INT_CLR(x)		(GPIO_REG(x) + 0x70)

#define GPIO_MSK_CNF(x)		(GPIO_REG(x) + tegra_gpio_upper_offset + 0x00)
#define GPIO_MSK_OE(x)		(GPIO_REG(x) + tegra_gpio_upper_offset + 0x10)
#define GPIO_MSK_OUT(x)		(GPIO_REG(x) + tegra_gpio_upper_offset + 0X20)
#define GPIO_MSK_INT_STA(x)	(GPIO_REG(x) + tegra_gpio_upper_offset + 0x40)
#define GPIO_MSK_INT_ENB(x)	(GPIO_REG(x) + tegra_gpio_upper_offset + 0x50)
#define GPIO_MSK_INT_LVL(x)	(GPIO_REG(x) + tegra_gpio_upper_offset + 0x60)

#define GPIO_INT_LVL_MASK		0x010101
#define GPIO_INT_LVL_EDGE_RISING	0x000101
#define GPIO_INT_LVL_EDGE_FALLING	0x000100
#define GPIO_INT_LVL_EDGE_BOTH		0x010100
#define GPIO_INT_LVL_LEVEL_HIGH		0x000001
#define GPIO_INT_LVL_LEVEL_LOW		0x000000

struct tegra_gpio_bank {
	int bank;
	int irq;
	spinlock_t lvl_lock[4];
#ifdef CONFIG_PM_SLEEP
	u32 cnf[4];
	u32 out[4];
	u32 oe[4];
	u32 int_enb[4];
	u32 int_lvl[4];
	u32 wake_enb[4];
#endif
};

static struct irq_domain *irq_domain;
static void __iomem *regs;

static u32 tegra_gpio_bank_count;
static u32 tegra_gpio_bank_stride;
static u32 tegra_gpio_upper_offset;
static struct tegra_gpio_bank *tegra_gpio_banks;

static inline void tegra_gpio_writel(u32 val, u32 reg)
{
	__raw_writel(val, regs + reg);
}

static inline u32 tegra_gpio_readl(u32 reg)
{
	return __raw_readl(regs + reg);
}

static int tegra_gpio_compose(int bank, int port, int bit)
{
	return (bank << 5) | ((port & 0x3) << 3) | (bit & 0x7);
}

void tegra_gpio_set_tristate(int gpio_nr, enum tegra_tristate ts)
{
	int pin_group  =  tegra_pinmux_get_pingroup(gpio_nr);
	tegra_pinmux_set_tristate(pin_group, ts);
}

static void tegra_gpio_mask_write(u32 reg, int gpio, int value)
{
	u32 val;

	val = 0x100 << GPIO_BIT(gpio);
	if (value)
		val |= 1 << GPIO_BIT(gpio);
	tegra_gpio_writel(val, reg);
}

int tegra_gpio_get_bank_int_nr(int gpio)
{
	int bank;
	int irq;
	if (gpio >= TEGRA_NR_GPIOS) {
		pr_warn("%s : Invalid gpio ID - %d\n", __func__, gpio);
		return -EINVAL;
	}
	bank = gpio >> 5;
	irq = tegra_gpio_banks[bank].irq;
	return irq;
}

static void tegra_gpio_enable(int gpio)
{
	if (gpio >= TEGRA_NR_GPIOS) {
		pr_warn("%s : Invalid gpio ID - %d\n", __func__, gpio);
		return;
	}
	tegra_gpio_mask_write(GPIO_MSK_CNF(gpio), gpio, 1);
}

int tegra_is_gpio(int gpio)
{
	if (gpio >= TEGRA_NR_GPIOS) {
		pr_warn("%s : Invalid gpio ID - %d\n", __func__, gpio);
		return 0;
	}
	return (tegra_gpio_readl(GPIO_CNF(gpio)) >> GPIO_BIT(gpio)) & 0x1;
}
EXPORT_SYMBOL(tegra_is_gpio);


static void tegra_gpio_disable(int gpio)
{
	if (gpio >= TEGRA_NR_GPIOS) {
		pr_warn("%s : Invalid gpio ID - %d\n", __func__, gpio);
		return;
	}
	tegra_gpio_mask_write(GPIO_MSK_CNF(gpio), gpio, 0);
}

void tegra_gpio_init_configure(unsigned gpio, bool is_input, int value)
{
	if (gpio >= TEGRA_NR_GPIOS) {
		pr_warn("%s : Invalid gpio ID - %d\n", __func__, gpio);
		return;
	}
	if (is_input) {
		tegra_gpio_mask_write(GPIO_MSK_OE(gpio), gpio, 0);
	} else {
		tegra_gpio_mask_write(GPIO_MSK_OUT(gpio), gpio, value);
		tegra_gpio_mask_write(GPIO_MSK_OE(gpio), gpio, 1);
	}
	tegra_gpio_mask_write(GPIO_MSK_CNF(gpio), gpio, 1);
}

static void tegra_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	tegra_gpio_mask_write(GPIO_MSK_OUT(offset), offset, value);
}

static int tegra_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	if ((tegra_gpio_readl(GPIO_OE(offset)) >> GPIO_BIT(offset)) & 0x1)
		return (tegra_gpio_readl(GPIO_OUT(offset)) >>
			GPIO_BIT(offset)) & 0x1;
	return (tegra_gpio_readl(GPIO_IN(offset)) >> GPIO_BIT(offset)) & 0x1;
}

static int tegra_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	tegra_gpio_mask_write(GPIO_MSK_OE(offset), offset, 0);
	tegra_gpio_enable(offset);
	return 0;
}

static int tegra_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
					int value)
{
	tegra_gpio_set(chip, offset, value);
	tegra_gpio_mask_write(GPIO_MSK_OE(offset), offset, 1);
	tegra_gpio_enable(offset);
	return 0;
}

static int tegra_gpio_set_debounce(struct gpio_chip *chip, unsigned offset,
				unsigned debounce)
{
	return -ENOSYS;
}

static int tegra_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	return irq_find_mapping(irq_domain, offset);
}

static int tegra_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	return 0;
}

static void tegra_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	tegra_gpio_disable(offset);
}

static struct gpio_chip tegra_gpio_chip = {
	.label			= "tegra-gpio",
	.request		= tegra_gpio_request,
	.free			= tegra_gpio_free,
	.direction_input	= tegra_gpio_direction_input,
	.get			= tegra_gpio_get,
	.direction_output	= tegra_gpio_direction_output,
	.set			= tegra_gpio_set,
	.set_debounce		= tegra_gpio_set_debounce,
	.to_irq			= tegra_gpio_to_irq,
	.base			= 0,
	.ngpio			= TEGRA_NR_GPIOS,
};

static void tegra_gpio_irq_ack(struct irq_data *d)
{
	int gpio = d->hwirq;

	tegra_gpio_writel(1 << GPIO_BIT(gpio), GPIO_INT_CLR(gpio));

#ifdef CONFIG_TEGRA_FPGA_PLATFORM
	/* FPGA platforms have a serializer between the GPIO
	   block and interrupt controller. Allow time for
	   clearing of the GPIO interrupt to propagate to the
	   interrupt controller before re-enabling the IRQ
	   to prevent double interrupts. */
	udelay(15);
#endif
}

static void tegra_gpio_irq_mask(struct irq_data *d)
{
	int gpio = d->hwirq;

	tegra_gpio_mask_write(GPIO_MSK_INT_ENB(gpio), gpio, 0);
}

static void tegra_gpio_irq_unmask(struct irq_data *d)
{
	int gpio = d->hwirq;

	tegra_gpio_mask_write(GPIO_MSK_INT_ENB(gpio), gpio, 1);
}

static int tegra_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	int gpio = d->hwirq;
	struct tegra_gpio_bank *bank = irq_data_get_irq_chip_data(d);
	int port = GPIO_PORT(gpio);
	int lvl_type;
	int val;
	unsigned long flags;
	int wake = tegra_gpio_to_wake(d->hwirq);

	switch (type & IRQ_TYPE_SENSE_MASK) {
	case IRQ_TYPE_EDGE_RISING:
		lvl_type = GPIO_INT_LVL_EDGE_RISING;
		break;

	case IRQ_TYPE_EDGE_FALLING:
		lvl_type = GPIO_INT_LVL_EDGE_FALLING;
		break;

	case IRQ_TYPE_EDGE_BOTH:
		lvl_type = GPIO_INT_LVL_EDGE_BOTH;
		break;

	case IRQ_TYPE_LEVEL_HIGH:
		lvl_type = GPIO_INT_LVL_LEVEL_HIGH;
		break;

	case IRQ_TYPE_LEVEL_LOW:
		lvl_type = GPIO_INT_LVL_LEVEL_LOW;
		break;

	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&bank->lvl_lock[port], flags);

	val = tegra_gpio_readl(GPIO_INT_LVL(gpio));
	val &= ~(GPIO_INT_LVL_MASK << GPIO_BIT(gpio));
	val |= lvl_type << GPIO_BIT(gpio);
	tegra_gpio_writel(val, GPIO_INT_LVL(gpio));

	spin_unlock_irqrestore(&bank->lvl_lock[port], flags);

	tegra_gpio_mask_write(GPIO_MSK_OE(gpio), gpio, 0);
	tegra_gpio_enable(gpio);

	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH))
		__irq_set_handler_locked(d->irq, handle_level_irq);
	else if (type & (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING))
		__irq_set_handler_locked(d->irq, handle_edge_irq);

	tegra_pm_irq_set_wake_type(wake, type);

	return 0;
}

static void tegra_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	struct tegra_gpio_bank *bank;
	int port;
	int pin;
	struct irq_chip *chip = irq_desc_get_chip(desc);

	chained_irq_enter(chip, desc);

	bank = irq_get_handler_data(irq);

	for (port = 0; port < 4; port++) {
		int gpio = tegra_gpio_compose(bank->bank, port, 0);
		unsigned long sta = tegra_gpio_readl(GPIO_INT_STA(gpio)) &
			tegra_gpio_readl(GPIO_INT_ENB(gpio));

		for_each_set_bit(pin, &sta, 8)
			generic_handle_irq(gpio_to_irq(gpio + pin));
	}

	chained_irq_exit(chip, desc);

}

#ifdef CONFIG_PM_SLEEP
void tegra_gpio_resume(void)
{
	unsigned long flags;
	int b;
	int p;

	local_irq_save(flags);

	for (b = 0; b < tegra_gpio_bank_count; b++) {
		struct tegra_gpio_bank *bank = &tegra_gpio_banks[b];

		for (p = 0; p < ARRAY_SIZE(bank->oe); p++) {
			unsigned int gpio = (b<<5) | (p<<3);
			tegra_gpio_writel(bank->cnf[p], GPIO_CNF(gpio));
			tegra_gpio_writel(bank->out[p], GPIO_OUT(gpio));
			tegra_gpio_writel(bank->oe[p], GPIO_OE(gpio));
			tegra_gpio_writel(bank->int_lvl[p], GPIO_INT_LVL(gpio));
			tegra_gpio_writel(bank->int_enb[p], GPIO_INT_ENB(gpio));
		}
	}

	local_irq_restore(flags);
}

int tegra_gpio_suspend(void)
{
	unsigned long flags;
	int b;
	int p;

	local_irq_save(flags);
	for (b = 0; b < tegra_gpio_bank_count; b++) {
		struct tegra_gpio_bank *bank = &tegra_gpio_banks[b];

		for (p = 0; p < ARRAY_SIZE(bank->oe); p++) {
			unsigned int gpio = (b<<5) | (p<<3);
			bank->cnf[p] = tegra_gpio_readl(GPIO_CNF(gpio));
			bank->out[p] = tegra_gpio_readl(GPIO_OUT(gpio));
			bank->oe[p] = tegra_gpio_readl(GPIO_OE(gpio));
			bank->int_enb[p] = tegra_gpio_readl(GPIO_INT_ENB(gpio));
			bank->int_lvl[p] = tegra_gpio_readl(GPIO_INT_LVL(gpio));

			/* disable gpio interrupts that are not wake sources */
			tegra_gpio_writel(bank->wake_enb[p], GPIO_INT_ENB(gpio));
		}
	}
	local_irq_restore(flags);

	return 0;
}

static int tegra_update_lp1_gpio_wake(struct irq_data *d, bool enable)
{
#ifdef CONFIG_PM_SLEEP
	struct tegra_gpio_bank *bank = irq_data_get_irq_chip_data(d);
	u8 mask;
	u8 port_index;
	u8 pin_index_in_bank;
	u8 pin_in_port;
	int gpio = d->hwirq;

	if (gpio < 0)
		return -EIO;
	pin_index_in_bank = (gpio & 0x1F);
	port_index = pin_index_in_bank >> 3;
	pin_in_port = (pin_index_in_bank & 0x7);
	mask = BIT(pin_in_port);
	if (enable)
		bank->wake_enb[port_index] |= mask;
	else
		bank->wake_enb[port_index] &= ~mask;
#endif

	return 0;
}

static int tegra_gpio_irq_set_wake(struct irq_data *d, unsigned int enable)
{
	struct tegra_gpio_bank *bank = irq_data_get_irq_chip_data(d);
	int ret = 0;
	int wake = tegra_gpio_to_wake(d->hwirq);

	/*
	 * update LP1 mask for gpio port/pin interrupt
	 * LP1 enable independent of LP0 wake support
	 */
	ret = tegra_update_lp1_gpio_wake(d, enable);
	if (ret) {
		pr_err("Failed gpio lp1 %s for irq=%d, error=%d\n",
			(enable ? "enable" : "disable"), d->irq, ret);
		goto fail;
	}

	/* LP1 enable for bank interrupt */
	ret = tegra_update_lp1_irq_wake(bank->irq, enable);
	if (ret)
		pr_err("Failed gpio lp1 %s for irq=%d, error=%d\n",
			(enable ? "enable" : "disable"), bank->irq, ret);

	if (wake < 0)
		pr_err("Warning: enabling a non-LP0 wake source %lu\n",
			d->hwirq);
	else {
		ret = tegra_pm_irq_set_wake(wake, enable);
		if (ret)
			pr_err("Failed gpio lp0 %s for irq=%d, error=%d\n",
				(enable ? "enable" : "disable"), d->irq, ret);
	}

fail:
	return ret;
}
#else
#define tegra_gpio_irq_set_wake NULL
#define tegra_update_lp1_gpio_wake NULL
#define tegra_gpio_suspend NULL
#define tegra_gpio_resume NULL
#endif

static struct syscore_ops tegra_gpio_syscore_ops = {
	.suspend = tegra_gpio_suspend,
	.resume = tegra_gpio_resume,
};

int tegra_gpio_resume_init(void)
{
	register_syscore_ops(&tegra_gpio_syscore_ops);
	return 0;
}

static struct irq_chip tegra_gpio_irq_chip = {
	.name		= "GPIO",
	.irq_ack	= tegra_gpio_irq_ack,
	.irq_mask	= tegra_gpio_irq_mask,
	.irq_unmask	= tegra_gpio_irq_unmask,
	.irq_set_type	= tegra_gpio_irq_set_type,
	.irq_set_wake	= tegra_gpio_irq_set_wake,
	.flags		= IRQCHIP_MASK_ON_SUSPEND,
};

struct tegra_gpio_soc_config {
	u32 bank_stride;
	u32 upper_offset;
};

static struct tegra_gpio_soc_config tegra20_gpio_config = {
	.bank_stride = 0x80,
	.upper_offset = 0x800,
};

static struct tegra_gpio_soc_config tegra30_gpio_config = {
	.bank_stride = 0x100,
	.upper_offset = 0x80,
};

static struct of_device_id tegra_gpio_of_match[] __devinitdata = {
	{ .compatible = "nvidia,tegra30-gpio", .data = &tegra30_gpio_config },
	{ .compatible = "nvidia,tegra20-gpio", .data = &tegra20_gpio_config },
	{ },
};

/* This lock class tells lockdep that GPIO irqs are in a different
 * category than their parents, so it won't report false recursion.
 */
static struct lock_class_key gpio_lock_class;

static int __devinit tegra_gpio_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct tegra_gpio_soc_config *config;
	int irq_base;
	struct resource *res;
	struct tegra_gpio_bank *bank;
	int gpio;
	int i;
	int j;

	match = of_match_device(tegra_gpio_of_match, &pdev->dev);
	if (match)
		config = (struct tegra_gpio_soc_config *)match->data;
	else
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		config = &tegra20_gpio_config;
#else
		config = &tegra30_gpio_config;
#endif

	tegra_gpio_bank_stride = config->bank_stride;
	tegra_gpio_upper_offset = config->upper_offset;

	for (;;) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, tegra_gpio_bank_count);
		if (!res)
			break;
		tegra_gpio_bank_count++;
	}
	if (!tegra_gpio_bank_count) {
		dev_err(&pdev->dev, "Missing IRQ resource\n");
		return -ENODEV;
	}

	tegra_gpio_chip.ngpio = tegra_gpio_bank_count * 32;

	tegra_gpio_banks = devm_kzalloc(&pdev->dev,
			tegra_gpio_bank_count * sizeof(*tegra_gpio_banks),
			GFP_KERNEL);
	if (!tegra_gpio_banks) {
		dev_err(&pdev->dev, "Couldn't allocate bank structure\n");
		return -ENODEV;
	}

	irq_base = irq_alloc_descs(-1, 0, tegra_gpio_chip.ngpio, 0);
	if (irq_base < 0) {
		dev_err(&pdev->dev, "Couldn't allocate IRQ numbers\n");
		return -ENODEV;
	}

	irq_domain = irq_domain_add_legacy(pdev->dev.of_node,
					   tegra_gpio_chip.ngpio, irq_base, 0,
					   &irq_domain_simple_ops, NULL);

	for (i = 0; i < tegra_gpio_bank_count; i++) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res) {
			dev_err(&pdev->dev, "Missing IRQ resource\n");
			return -ENODEV;
		}

		bank = &tegra_gpio_banks[i];
		bank->bank = i;
		bank->irq = res->start;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Missing MEM resource\n");
		return -ENODEV;
	}

	regs = devm_request_and_ioremap(&pdev->dev, res);
	if (!regs) {
		dev_err(&pdev->dev, "Couldn't ioremap regs\n");
		return -ENODEV;
	}

	for (i = 0; i < tegra_gpio_bank_count; i++) {
		for (j = 0; j < 4; j++) {
			int gpio = tegra_gpio_compose(i, j, 0);
			tegra_gpio_writel(0x00, GPIO_INT_ENB(gpio));
			tegra_gpio_writel(0x00, GPIO_INT_STA(gpio));
		}
	}

#ifdef CONFIG_OF_GPIO
	tegra_gpio_chip.of_node = pdev->dev.of_node;
#endif

	gpiochip_add(&tegra_gpio_chip);

	for (gpio = 0; gpio < tegra_gpio_chip.ngpio; gpio++) {
		int irq = irq_find_mapping(irq_domain, gpio);
		/* No validity check; all Tegra GPIOs are valid IRQs */

		bank = &tegra_gpio_banks[GPIO_BANK(gpio)];

		irq_set_lockdep_class(irq, &gpio_lock_class);
		irq_set_chip_data(irq, bank);
		irq_set_chip_and_handler(irq, &tegra_gpio_irq_chip,
					 handle_simple_irq);
		set_irq_flags(irq, IRQF_VALID);
	}

	for (i = 0; i < tegra_gpio_bank_count; i++) {
		bank = &tegra_gpio_banks[i];

		for (j = 0; j < 4; j++)
			spin_lock_init(&bank->lvl_lock[j]);

		irq_set_handler_data(bank->irq, bank);
		irq_set_chained_handler(bank->irq, tegra_gpio_irq_handler);

	}

	return 0;
}

static struct platform_driver tegra_gpio_driver = {
	.driver		= {
		.name	= "tegra-gpio",
		.owner	= THIS_MODULE,
		.of_match_table = tegra_gpio_of_match,
	},
	.probe		= tegra_gpio_probe,
};

static int __init tegra_gpio_init(void)
{
	return platform_driver_register(&tegra_gpio_driver);
}
postcore_initcall(tegra_gpio_init);

void tegra_gpio_config(struct tegra_gpio_table *table, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		int gpio = table[i].gpio;

		if (table[i].enable)
			tegra_gpio_enable(gpio);
		else
			tegra_gpio_disable(gpio);
	}
}

#ifdef	CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static int dbg_gpio_show(struct seq_file *s, void *unused)
{
	int i;
	int j;

	seq_printf(s, "Bank:Port CNF OE OUT IN INT_STA INT_ENB INT_LVL\n");
	for (i = 0; i < tegra_gpio_bank_count; i++) {
		for (j = 0; j < 4; j++) {
			int gpio = tegra_gpio_compose(i, j, 0);
			seq_printf(s,
				"%d:%d %02x %02x %02x %02x %02x %02x %06x\n",
				i, j,
				tegra_gpio_readl(GPIO_CNF(gpio)),
				tegra_gpio_readl(GPIO_OE(gpio)),
				tegra_gpio_readl(GPIO_OUT(gpio)),
				tegra_gpio_readl(GPIO_IN(gpio)),
				tegra_gpio_readl(GPIO_INT_STA(gpio)),
				tegra_gpio_readl(GPIO_INT_ENB(gpio)),
				tegra_gpio_readl(GPIO_INT_LVL(gpio)));
		}
	}
	return 0;
}

static int dbg_gpio_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_gpio_show, &inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= dbg_gpio_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_gpio_debuginit(void)
{
	(void) debugfs_create_file("tegra_gpio", S_IRUGO,
					NULL, NULL, &debug_fops);
	return 0;
}
late_initcall(tegra_gpio_debuginit);
#endif
