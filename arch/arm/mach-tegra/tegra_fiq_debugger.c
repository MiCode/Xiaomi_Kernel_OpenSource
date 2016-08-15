/*
 * arch/arm/mach-tegra/fiq_debugger.c
 *
 * Serial Debugger Interface for Tegra
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdarg.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/serial_reg.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <asm/fiq_debugger.h>
#include <mach/tegra_fiq_debugger.h>
#include <mach/fiq.h>

#include <linux/uaccess.h>

struct tegra_fiq_debugger {
	struct fiq_debugger_pdata pdata;
	void __iomem *debug_port_base;
	bool break_seen;
};

static inline void tegra_write(struct tegra_fiq_debugger *t,
	unsigned int val, unsigned int off)
{
	__raw_writeb(val, t->debug_port_base + off * 4);
}

static inline unsigned int tegra_read(struct tegra_fiq_debugger *t,
	unsigned int off)
{
	return __raw_readb(t->debug_port_base + off * 4);
}

static inline unsigned int tegra_read_lsr(struct tegra_fiq_debugger *t)
{
	unsigned int lsr;

	lsr = tegra_read(t, UART_LSR);
	if (lsr & UART_LSR_BI)
		t->break_seen = true;

	return lsr;
}


static int debug_getc(struct platform_device *pdev)
{
	return FIQ_DEBUGGER_NO_CHAR;
}

static void debug_putc(struct platform_device *pdev, unsigned int c)
{
}


static void fiq_enable(struct platform_device *pdev, unsigned int irq, bool on)
{
	if (on)
		tegra_fiq_enable(irq);
	else
		tegra_fiq_disable(irq);
}

static int tegra_fiq_debugger_id;

void tegra_serial_debug_init(unsigned int base, int irq,
			   struct clk *clk, int signal_irq, int wakeup_irq)
{
	struct tegra_fiq_debugger *t;
	struct platform_device *pdev;
	struct resource *res;
	int res_count;

	t = kzalloc(sizeof(struct tegra_fiq_debugger), GFP_KERNEL);
	if (!t) {
		pr_err("Failed to allocate for fiq debugger\n");
		return;
	}
	t->pdata.uart_getc = debug_getc;
	t->pdata.uart_putc = debug_putc;

	t->pdata.fiq_enable = fiq_enable;

	t->debug_port_base = ioremap(base, PAGE_SIZE);
	if (!t->debug_port_base) {
		pr_err("Failed to ioremap for fiq debugger\n");
		goto out1;
	}

	res = kzalloc(sizeof(struct resource) * 3, GFP_KERNEL);
	if (!res) {
		pr_err("Failed to alloc fiq debugger resources\n");
		goto out2;
	}

	pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (!pdev) {
		pr_err("Failed to alloc fiq debugger platform device\n");
		goto out3;
	};

	res[0].flags = IORESOURCE_IRQ;
	res[0].start = irq;
	res[0].end = irq;
	res[0].name = "fiq";

	res[1].flags = IORESOURCE_IRQ;
	res[1].start = signal_irq;
	res[1].end = signal_irq;
	res[1].name = "signal";
	res_count = 2;

	if (wakeup_irq >= 0) {
		res[2].flags = IORESOURCE_IRQ;
		res[2].start = wakeup_irq;
		res[2].end = wakeup_irq;
		res[2].name = "wakeup";
		res_count++;
	}

	pdev->name = "fiq_debugger";
	pdev->id = tegra_fiq_debugger_id++;
	pdev->dev.platform_data = &t->pdata;
	pdev->resource = res;
	pdev->num_resources = res_count;

	if (platform_device_register(pdev)) {
		pr_err("Failed to register fiq debugger\n");
		goto out4;
	}

	return;

out4:
	kfree(pdev);
out3:
	kfree(res);
out2:
	iounmap(t->debug_port_base);
out1:
	kfree(t);
}
