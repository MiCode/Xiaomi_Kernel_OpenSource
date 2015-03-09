/*
 * hsu_pci.c: driver for Intel High Speed UART device
 *
 * Refer pxa.c, 8250.c and some other drivers in drivers/serial/
 *
 * (C) Copyright 2010 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>

#include "hsu.h"

#ifdef CONFIG_PM_SLEEP
static int serial_hsu_pci_suspend(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);
	int ret = 0;

	if (up)
		ret = serial_hsu_do_suspend(up);

	return ret;
}

static int serial_hsu_pci_resume(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);
	int ret = 0;

	if (up)
		ret = serial_hsu_do_resume(up);

	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int serial_hsu_pci_runtime_idle(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);

	return serial_hsu_do_runtime_idle(up);
}

static int serial_hsu_pci_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);

	return serial_hsu_do_suspend(up);
}

static int serial_hsu_pci_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);

	return serial_hsu_do_resume(up);
}
#endif

static const struct dev_pm_ops serial_hsu_pci_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(serial_hsu_pci_suspend,
				serial_hsu_pci_resume)
	SET_RUNTIME_PM_OPS(serial_hsu_pci_runtime_suspend,
				serial_hsu_pci_runtime_resume,
				serial_hsu_pci_runtime_idle)
};

enum hsu_pci_id_t {
	medfield_hsu,
};

static struct hsu_port_cfg hsu_pci_cfgs[] = {
	[medfield_hsu] = {
		.hw_ip = hsu_intel,
	},
};

static const struct pci_device_id hsuart_port_pci_ids[] = {
	{ PCI_VDEVICE(INTEL, 0x081B), medfield_hsu },
	{ PCI_VDEVICE(INTEL, 0x081C), medfield_hsu },
	{ PCI_VDEVICE(INTEL, 0x081D), medfield_hsu },
	{},
};
MODULE_DEVICE_TABLE(pci, hsuart_port_pci_ids);

static const struct pci_device_id hsuart_dma_pci_ids[] = {
	{ PCI_VDEVICE(INTEL, 0x081E), 0 },
	{},
};
MODULE_DEVICE_TABLE(pci, hsuart_dma_pci_ids);

static int serial_hsu_pci_port_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	struct uart_hsu_port *up;
	int ret, index;
	resource_size_t start, len;
	struct hsu_port_cfg *hsu_cfg;

	start = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);

	if (ent->driver_data >= ARRAY_SIZE(hsu_pci_cfgs)) {
		dev_err(&pdev->dev, "%s: invalid driver data %ld\n", __func__,
			ent->driver_data);
		return -EINVAL;
	}

	dev_info(&pdev->dev,
		"FUNC: %d driver: %ld addr:%lx len:%lx\n",
		PCI_FUNC(pdev->devfn), ent->driver_data,
		(ulong) start, (ulong) len);

	switch (pdev->device) {
	case 0x081B:
		index = 0;
		break;
	case 0x081C:
		index = 1;
		break;
	case 0x081D:
		index = 2;
		break;
	default:
		dev_err(&pdev->dev, "HSU: out of index!");
		return -ENODEV;
	}

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	ret = pci_request_region(pdev, 0, "hsu");
	if (ret)
		goto err;

	hsu_cfg = &hsu_pci_cfgs[ent->driver_data];

	up = serial_hsu_port_setup(&pdev->dev, index, start, len,
			pdev->irq, hsu_cfg);
	if (IS_ERR(up))
		goto err;

	pci_set_drvdata(pdev, up);

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);
	return 0;
err:
	pci_disable_device(pdev);
	return ret;
}

static void serial_hsu_pci_port_remove(struct pci_dev *pdev)
{
	struct uart_hsu_port *up = pci_get_drvdata(pdev);

	pm_runtime_forbid(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	serial_hsu_port_free(up);
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
}

static void serial_hsu_pci_port_shutdown(struct pci_dev *pdev)
{
	struct uart_hsu_port *up = pci_get_drvdata(pdev);

	if (!up)
		return;

	serial_hsu_port_shutdown(up);
}

static struct pci_driver hsu_port_pci_driver = {
	.name =		"HSU serial",
	.id_table =	hsuart_port_pci_ids,
	.probe =	serial_hsu_pci_port_probe,
	.remove =	serial_hsu_pci_port_remove,
	.shutdown =	serial_hsu_pci_port_shutdown,
/* Disable PM only when kgdb(poll mode uart) is enabled */
#if defined(CONFIG_PM) && !defined(CONFIG_CONSOLE_POLL)
	.driver = {
		.pm = &serial_hsu_pci_pm_ops,
	},
#endif
};

static int serial_hsu_pci_dma_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	int ret;
	resource_size_t start, len;

	start = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);

	dev_info(&pdev->dev,
		"FUNC: %d driver: %ld addr:%lx len:%lx\n",
		PCI_FUNC(pdev->devfn), ent->driver_data,
		(ulong) pci_resource_start(pdev, 0),
		(ulong) pci_resource_len(pdev, 0));

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	ret = pci_request_region(pdev, 0, "hsu dma");
	if (ret)
		goto err;

	ret = serial_hsu_dma_setup(&pdev->dev, start, len, pdev->irq);
	if (ret)
		goto err;

	return 0;
err:
	pci_disable_device(pdev);
	return ret;
}

static void serial_hsu_pci_dma_remove(struct pci_dev *pdev)
{
	serial_hsu_dma_free();
	pci_disable_device(pdev);
	pci_unregister_driver(&hsu_port_pci_driver);
}

static struct pci_driver hsu_dma_pci_driver = {
	.name =		"HSU DMA",
	.id_table =	hsuart_dma_pci_ids,
	.probe =	serial_hsu_pci_dma_probe,
	.remove =	serial_hsu_pci_dma_remove,
};

static int __init hsu_pci_init(void)
{
	int ret;

	ret = pci_register_driver(&hsu_dma_pci_driver);
	if (!ret) {
		ret = pci_register_driver(&hsu_port_pci_driver);
		if (ret)
			pci_unregister_driver(&hsu_dma_pci_driver);
	}

	return ret;
}

static void __exit hsu_pci_exit(void)
{
	pci_unregister_driver(&hsu_port_pci_driver);
	pci_unregister_driver(&hsu_dma_pci_driver);
}

module_init(hsu_pci_init);
module_exit(hsu_pci_exit);

MODULE_AUTHOR("Yang Bin <bin.yang@intel.com>");
MODULE_LICENSE("GPL v2");
