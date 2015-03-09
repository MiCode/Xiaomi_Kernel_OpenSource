/*
 * hsu_plat.c: driver for Intel High Speed UART device
 *
 * (C) Copyright 2013 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>
#include <linux/clk.h>

#include "hsu.h"

#define CHT_HSU_CLOCK	0x0800
#define CHT_HSU_RESET	0x0804
#define CHT_GENERAL_REG 0x0808
#define CHT_HSU_OVF_IRQ	0x0820	/* Overflow interrupt related */

#define CHT_GENERAL_DIS_RTS_N_OVERRIDE (1 << 3)
enum {
	hsu_chv_0,
	hsu_chv_1,
};

void cht_hsu_reset(void __iomem *addr)
{
	writel(0, addr + CHT_HSU_RESET);
	writel(3, addr + CHT_HSU_RESET);

	/* Disable the tx overflow IRQ */
	writel(2, addr + CHT_HSU_OVF_IRQ);
}

void cht_hsu_set_clk(unsigned int m, unsigned int n,
				void __iomem *addr)
{
	unsigned int param, update_bit;

	update_bit = 1 << 31;
	param = (m << 1) | (n << 16) | 0x1;

	writel(param, addr + CHT_HSU_CLOCK);
	writel((param | update_bit), addr + CHT_HSU_CLOCK);
	writel(param, addr + CHT_HSU_CLOCK);
}

static void hsu_set_termios(struct uart_port *p, struct ktermios *termios,
				struct ktermios *old)
{
	u32 reg;

	/*
	 * If auto-handshake mechanism is enabled,
	 * disable rts_n override
	 */
	reg = readl(p->membase + CHT_GENERAL_REG);
	reg &= ~CHT_GENERAL_DIS_RTS_N_OVERRIDE;
	if (!(termios->c_cflag & CRTSCTS))
		reg |= CHT_GENERAL_DIS_RTS_N_OVERRIDE;
	writel(reg, p->membase + CHT_GENERAL_REG);

	serial_hsu_do_set_termios(p, termios, old);
}

static void hsu_serial_setup(struct uart_hsu_port *up)
{
	struct uart_port *p = &up->port;

	p->set_termios = hsu_set_termios;
}

unsigned int cht_hsu_get_uartclk(struct uart_hsu_port *up)
{
	struct clk *clk;
	unsigned int uartclk = 0;

	clk = devm_clk_get(up->dev, NULL);
	if (!IS_ERR(clk)) {
		clk_prepare_enable(clk);
		uartclk = clk_get_rate(clk);
	}

	return uartclk;
}

static struct hsu_port_cfg hsu_port_cfgs[] = {
	[hsu_chv_0] = {
		.hw_ip = hsu_dw,
		.idle = 100,
		.hw_reset = cht_hsu_reset,
		.get_uartclk = cht_hsu_get_uartclk,
		.set_clk = cht_hsu_set_clk,
		.hw_context_save = 1,
	},
	[hsu_chv_1] = {
		.hw_ip = hsu_dw,
		.idle = 100,
		.hw_reset = cht_hsu_reset,
		.get_uartclk = cht_hsu_get_uartclk,
		.set_clk = cht_hsu_set_clk,
		.hw_context_save = 1,
	},
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id hsu_acpi_ids[] = {
	{ "8086228A", hsu_chv_0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, hsu_acpi_ids);
#endif

#ifdef CONFIG_PM_SLEEP
static int serial_hsu_plat_suspend(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);
	int ret = 0;

	if (up)
		ret = serial_hsu_do_suspend(up);

	return ret;
}

static int serial_hsu_plat_resume(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);
	int ret = 0;

	if (up)
		ret = serial_hsu_do_resume(up);

	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int serial_hsu_plat_runtime_idle(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);

	return serial_hsu_do_runtime_idle(up);
}

static int serial_hsu_plat_runtime_suspend(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);

	return serial_hsu_do_suspend(up);
}

static int serial_hsu_plat_runtime_resume(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);

	return serial_hsu_do_resume(up);
}
#endif

static const struct dev_pm_ops serial_hsu_plat_pm_ops = {

	SET_SYSTEM_SLEEP_PM_OPS(serial_hsu_plat_suspend,
				serial_hsu_plat_resume)
	SET_RUNTIME_PM_OPS(serial_hsu_plat_runtime_suspend,
				serial_hsu_plat_runtime_resume,
				serial_hsu_plat_runtime_idle)
};

static int serial_hsu_plat_port_probe(struct platform_device *pdev)
{
	const struct acpi_device_id *id;
	struct acpi_device *adev;
	struct uart_hsu_port *up;
	int port = pdev->id, irq;
	struct resource *mem, *ioarea;
	resource_size_t start, len;
	struct hsu_port_cfg *cfg = NULL;

#ifdef CONFIG_ACPI
	if (!ACPI_HANDLE(&pdev->dev) ||
		acpi_bus_get_device(ACPI_HANDLE(&pdev->dev), &adev))
		return -ENODEV;

	id = acpi_match_device(pdev->dev.driver->acpi_match_table, &pdev->dev);
	if (!id)
		return -ENODEV;

	if (kstrtoint(adev->pnp.unique_id, 0, &port))
		return -ENODEV;
	port--;

	cfg = &hsu_port_cfgs[id->driver_data + port];
	cfg->dev = &pdev->dev;
#endif

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		return -EINVAL;
	}
	start = mem->start;
	len = resource_size(mem);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return -ENXIO;
	}

	ioarea = request_mem_region(mem->start, resource_size(mem),
			pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "HSU region already claimed\n");
		return -EBUSY;
	}

	up = serial_hsu_port_setup(&pdev->dev, port, start, len,
			irq, cfg);
	if (IS_ERR(up)) {
		release_mem_region(mem->start, resource_size(mem));
		dev_err(&pdev->dev, "failed to setup HSU\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, up);
	hsu_serial_setup(up);

	if (!pdev->dev.dma_mask) {
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	}
	dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	return 0;
}

static int serial_hsu_plat_port_remove(struct platform_device *pdev)
{
	struct uart_hsu_port *up = platform_get_drvdata(pdev);
	struct resource *mem;

	pm_runtime_forbid(&pdev->dev);
	serial_hsu_port_free(up);
	platform_set_drvdata(pdev, NULL);
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem)
		release_mem_region(mem->start, resource_size(mem));

	return 0;
}

static void serial_hsu_plat_port_shutdown(struct platform_device *pdev)
{
	struct uart_hsu_port *up = platform_get_drvdata(pdev);

	if (!up)
		return;

	serial_hsu_port_shutdown(up);
}

static struct platform_driver hsu_plat_driver = {
	.probe		= serial_hsu_plat_port_probe,
	.remove		= serial_hsu_plat_port_remove,
	.shutdown	= serial_hsu_plat_port_shutdown,
	.driver		= {
		.name	= "HSU serial",
		.owner	= THIS_MODULE,
/* Disable PM only when kgdb(poll mode uart) is enabled */
#if defined(CONFIG_PM) && !defined(CONFIG_CONSOLE_POLL)
		.pm	 = &serial_hsu_plat_pm_ops,
#endif
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(hsu_acpi_ids),
#endif
	},
};

static int __init hsu_plat_init(void)
{
	return platform_driver_register(&hsu_plat_driver);
}

static void __exit hsu_plat_exit(void)
{
	platform_driver_unregister(&hsu_plat_driver);
}

module_init(hsu_plat_init);
module_exit(hsu_plat_exit);

MODULE_AUTHOR("Jason Chen <jason.cj.chen@intel.com>");
MODULE_LICENSE("GPL v2");
