/*
 * Tegra Wakeups for NVIDIA SoCs Tegra
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/irqchip/tegra.h>
#include <linux/system-wakeup.h>

#include <mach/irqs.h>
#include <mach/gpio-tegra.h>

int *tegra_gpio_wake_table;
EXPORT_SYMBOL_GPL(tegra_gpio_wake_table);

int *tegra_irq_wake_table;
EXPORT_SYMBOL_GPL(tegra_irq_wake_table);

int tegra_wake_table_len;
EXPORT_SYMBOL_GPL(tegra_wake_table_len);

static int last_gpio = -1;

int tegra_set_wake_gpio(unsigned int wake, int gpio)
{
	if (wake >= tegra_wake_table_len)
		return -EINVAL;

	tegra_irq_wake_table[wake] = -EAGAIN;
	tegra_gpio_wake_table[wake] = gpio;

	return 0;
}

int tegra_set_wake_irq(unsigned int wake, int irq)
{
	if (wake >= tegra_wake_table_len)
		return -EINVAL;

	tegra_irq_wake_table[wake] = irq;

	return 0;
}

int tegra_gpio_to_wake(int gpio)
{
	int i;

	for (i = 0; i < tegra_wake_table_len; i++) {
		if (tegra_gpio_wake_table[i] == gpio) {
			pr_info("gpio wake%d for gpio=%d\n", i, gpio);
			last_gpio = i;
			return i;
		}
	}

	return -EINVAL;
}

void tegra_irq_to_wake(int irq, int *wak_list, int *wak_size)
{
	int i;
	int bank_irq;

	*wak_size = 0;
	for (i = 0; i < tegra_wake_table_len; i++) {
		if (tegra_irq_wake_table[i] == irq) {
			pr_info("Wake%d for irq=%d\n", i, irq);
			wak_list[*wak_size] = i;
			*wak_size = *wak_size + 1;
		}
	}
	if (*wak_size)
		goto out;

	/* The gpio set_wake code bubbles the set_wake call up to the irq
	 * set_wake code. This insures that the nested irq set_wake call
	 * succeeds, even though it doesn't have to do any pm setup for the
	 * bank.
	 *
	 * This is very fragile - there's no locking, so two callers could
	 * cause issues with this.
	 */
	if (last_gpio < 0)
		goto out;

	bank_irq = tegra_gpio_get_bank_int_nr(tegra_gpio_wake_table[last_gpio]);
	if (bank_irq == irq) {
		pr_info("gpio bank wake found: wake%d for irq=%d\n", i, irq);
		wak_list[*wak_size] = last_gpio;
		*wak_size = 1;
	}

out:
	return;
}

int tegra_wake_to_irq(int wake)
{
	int ret;

	if (wake < 0)
		return -EINVAL;

	if (wake >= tegra_wake_table_len)
		return -EINVAL;

	ret = tegra_irq_wake_table[wake];
	if (ret == -EAGAIN) {
		ret = tegra_gpio_wake_table[wake];
		if (ret != -EINVAL)
			ret = gpio_to_irq(ret);
	}

	return ret;
}

int tegra_set_wake_source(int wake, int irq)
{
	if (wake < 0)
		return -EINVAL;

	if (wake >= tegra_wake_table_len)
		return -EINVAL;

	tegra_irq_wake_table[wake] = irq;
	return 0;
}

int tegra_disable_wake_source(int wake)
{
	return tegra_set_wake_source(wake, -EINVAL);
}

int get_wakeup_reason_irq(void)
{
	unsigned long long wake_status = tegra_read_pmc_wake_status();
	unsigned long long mask_ll = 1ULL;
	int irq;
	struct irq_desc *desc;
	int i;

	for (i = 0; i < tegra_wake_table_len; ++i) {
		if (mask_ll & wake_status) {
			irq = tegra_wake_to_irq(i);
			if (!irq)
				continue;
			desc = irq_to_desc(irq);
			if (!desc || !desc->action || !desc->action->name)
				continue;
			return irq;
		}
		mask_ll <<= 1;
	}
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(get_wakeup_reason_irq);
