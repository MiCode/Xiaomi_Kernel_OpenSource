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
#include <linux/gpio.h>
#include <linux/delay.h>

#include "hsu.h"

#define CHT_HSU_CLOCK	0x0800
#define CHT_HSU_RESET	0x0804
#define CHT_GENERAL_REG 0x0808
#define CHT_HSU_OVF_IRQ	0x0820	/* Overflow interrupt related */
#define CHT_GENERAL_DIS_RTS_N_OVERRIDE (1 << 3)

enum {
	hsu_chv,
};

static irqreturn_t wakeup_irq(int irq, void *data)
{
	struct uart_hsu_port *up = data;
	struct hsu_port_cfg *cfg = up->port_cfg;
	struct hsu_port_pin_cfg *pin_cfg = &cfg->pin_cfg;

	set_bit(flag_active, &up->flags);
	if (cfg->hw_set_rts && pin_cfg->wake_src == rxd_wake)
		cfg->hw_set_rts(up, 1);
	pm_runtime_get(up->dev);
	pm_runtime_put(up->dev);

	return IRQ_HANDLED;
}

static int cht_hw_set_rts(struct uart_hsu_port *up, int value)
{
	struct hsu_port_pin_cfg *pin_cfg = &up->port_cfg->pin_cfg;
	struct gpio_desc *gpio;

	if (!pin_cfg || pin_cfg->wake_src == no_wake)
		return 0;

	if (value) {
		if (!pin_cfg->rts_gpio) {
			gpio = devm_gpiod_get_index(up->dev, "hsu_rts",
					hsu_rts_idx);
			if (!IS_ERR(gpio))
				pin_cfg->rts_gpio = desc_to_gpio(gpio);
		}

		if (pin_cfg->rts_gpio) {
			gpio_direction_output(pin_cfg->rts_gpio, 1);
			if (!in_interrupt())
				usleep_range(up->byte_delay,
						up->byte_delay + 1);
		}
	} else
		if (pin_cfg->rts_gpio) {
			gpio_free(pin_cfg->rts_gpio);
			pin_cfg->rts_gpio = 0;
		}

	return 0;
}

static int cht_hsu_hw_suspend(struct uart_hsu_port *up)
{
	struct hsu_port_pin_cfg *pin_cfg = &up->port_cfg->pin_cfg;
	struct gpio_desc *gpio;
	int ret;

	if (!pin_cfg || pin_cfg->wake_src == no_wake)
		return 0;

	switch (pin_cfg->wake_src) {
	case rxd_wake:
		if (!pin_cfg->rx_gpio) {
			gpio = devm_gpiod_get_index(up->dev, "hsu_rxd",
					hsu_rxd_idx);
			if (!IS_ERR(gpio))
				pin_cfg->rx_gpio = desc_to_gpio(gpio);
		}
		pin_cfg->wake_gpio = pin_cfg->rx_gpio;
		break;
	case cts_wake:
		if (!pin_cfg->cts_gpio) {
			gpio = devm_gpiod_get_index(up->dev, "hsu_cts",
					hsu_cts_idx);
			if (!IS_ERR(gpio))
				pin_cfg->cts_gpio = desc_to_gpio(gpio);
		}
		pin_cfg->wake_gpio = pin_cfg->cts_gpio;
		break;
	default:
		pin_cfg->wake_gpio = -1;
		break;
	}
	dev_dbg(up->dev, "wake_gpio=%d\n", pin_cfg->wake_gpio);

	if (pin_cfg->wake_gpio != -1) {
		gpio_direction_input(pin_cfg->wake_gpio);
		ret = request_irq(gpio_to_irq(pin_cfg->wake_gpio), wakeup_irq,
				IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING,
				"hsu_wake_irq", up);
		if (ret)
			dev_err(up->dev, "failed to register 'hsu_wake_irq'\n");

		if (pin_cfg->rts_gpio && pin_cfg->wake_src == rxd_wake)
			gpio_direction_output(pin_cfg->rts_gpio, 0);
	}

	return 0;
}

static int cht_hsu_hw_resume(struct uart_hsu_port *up)
{
	struct hsu_port_pin_cfg *pin_cfg = &up->port_cfg->pin_cfg;

	if (!pin_cfg || pin_cfg->wake_src == no_wake)
		return 0;

	if (pin_cfg->wake_gpio != -1) {
		free_irq(gpio_to_irq(pin_cfg->wake_gpio), up);
		pin_cfg->wake_gpio = -1;
	}

	switch (pin_cfg->wake_src) {
	case rxd_wake:
		if (pin_cfg->rx_gpio) {
			gpio_free(pin_cfg->rx_gpio);
			pin_cfg->rx_gpio = 0;
		}
		break;
	case cts_wake:
		if (pin_cfg->cts_gpio) {
			gpio_free(pin_cfg->cts_gpio);
			pin_cfg->cts_gpio = 0;
		}
		break;
	}

	return 0;
}

static void cht_hsu_reset(void __iomem *addr)
{
	writel(0, addr + CHT_HSU_RESET);
	writel(3, addr + CHT_HSU_RESET);

	/* Disable the tx overflow IRQ */
	writel(2, addr + CHT_HSU_OVF_IRQ);
}

static void cht_hsu_set_clk(unsigned int m, unsigned int n,
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

static unsigned int cht_hsu_get_uartclk(struct uart_hsu_port *up)
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
	[hsu_chv] = {
		.hw_ip = hsu_dw,
		.idle = 20,
		.pin_cfg = {
			.wake_src = no_wake,
		},
		.hw_set_rts = cht_hw_set_rts,
		.hw_suspend = cht_hsu_hw_suspend,
		.hw_resume = cht_hsu_hw_resume,
		.hw_reset = cht_hsu_reset,
		.get_uartclk = cht_hsu_get_uartclk,
		.set_clk = cht_hsu_set_clk,
		.hw_context_save = 1,
	},
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id hsu_acpi_ids[] = {
	{ "8086228A", hsu_chv },
	{ }
};
MODULE_DEVICE_TABLE(acpi, hsu_acpi_ids);

/* Manages child driver_data as a 32 bits field. */
#define CHILD_CFG(wake_src, idle) (((wake_src) & 0x3) | ((idle) << 2))
#define CHILD_CFG_WAKE(cfg)       ((cfg) & 0x3)
#define CHILD_CFG_IDLE(cfg)       (((cfg) >> 2) & 0x3FFFFFFF)

static const struct acpi_device_id child_acpi_ids[] = {
	{ "BCM4752" , CHILD_CFG(rxd_wake, 40) },
	{ "LNV4752" , CHILD_CFG(rxd_wake, 40) },
	{ "BCM4752E", CHILD_CFG(rxd_wake, 40) },
	{ "BCM47521", CHILD_CFG(rxd_wake, 40) },
	{ "INT33A2" , CHILD_CFG(cts_wake, 40) },
	{ },
};
MODULE_DEVICE_TABLE(acpi, child_acpi_ids);

static const struct acpi_device_id *match_device_ids(struct acpi_device *device,
					const struct acpi_device_id *ids)
{
	const struct acpi_device_id *id;
	struct acpi_hardware_id *hwid;

	if (!device->status.present)
		return NULL;

	for (id = ids; id->id[0]; id++)
		list_for_each_entry(hwid, &device->pnp.ids, list)
			if (!strcmp((char *) id->id, hwid->id))
				return id;

	return NULL;
}
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
	const struct acpi_device_id *id, *child_id;
	struct acpi_device *adev, *child;
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

	cfg = kmalloc(sizeof(struct hsu_port_cfg), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	*cfg = hsu_port_cfgs[id->driver_data];
	cfg->dev = &pdev->dev;

	/* Sets a particular config if required by device child. */
	list_for_each_entry(child, &adev->children, node) {
		/* child_id = acpi_match_device(child_acpi_ids, &child->dev); */
		child_id = match_device_ids(child, child_acpi_ids);
		if (child_id) {
			pr_warn("uart(%d) device(%s) wake_src(%ld) idle(%ld)\n",
				port, child_id->id,
				CHILD_CFG_WAKE(child_id->driver_data),
				CHILD_CFG_IDLE(child_id->driver_data));

			cfg->pin_cfg.wake_src =
				    CHILD_CFG_WAKE(child_id->driver_data);
			cfg->idle = CHILD_CFG_IDLE(child_id->driver_data);
		}
	}
#endif

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		kfree(cfg);
		return -EINVAL;
	}
	start = mem->start;
	len = resource_size(mem);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		kfree(cfg);
		return -ENXIO;
	}

	ioarea = request_mem_region(mem->start, resource_size(mem),
			pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "HSU region already claimed\n");
		kfree(cfg);
		return -EBUSY;
	}

	up = serial_hsu_port_setup(&pdev->dev, port, start, len,
			irq, cfg);
	if (IS_ERR(up)) {
		release_mem_region(mem->start, resource_size(mem));
		dev_err(&pdev->dev, "failed to setup HSU\n");
		kfree(cfg);
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
	kfree(up->port_cfg);
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
