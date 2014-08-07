/*
 * Virtual GPIO controller driver
 * Copyright (c) 2014, Intel Corporation.
 *
 * Author: Dyut Kumar Sil <dyut.k.sil@intel.com>
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>


#define VIRTUAL_GPIO_DRIVER_VERSION	"1.1.0"
#define VIRTUAL_GPIO_DRIVER_NAME	"virtual_gpio"

#define GPE0A_PME_STS_BIT               0x2000
#define GPE0A_PME_EN_BIT                0x2000
#define GPE0A_STS_PORT			0x420
#define GPE0A_EN_PORT			0x428
#define BYT_MODEL				0x37

struct workqueue_struct *vgpio_wq;

struct virtual_gpio_data {
	int irq;
	struct platform_device	*pdev;
	struct work_struct periodic_work;
	struct mutex vg_mutex;
};
static irqreturn_t virtual_gpio_irq_handler(int irq, void *data)
{
	struct virtual_gpio_data *gd;
	gd = data;
	queue_work(vgpio_wq, &gd->periodic_work);
	return IRQ_HANDLED;
}
void virtual_gpio_irq_handler_isr(struct work_struct *work)
{
	u32 gpe_sts_reg = inl(GPE0A_STS_PORT);
	u32 gpe_en_reg = inl(GPE0A_EN_PORT), temp = 0;
	acpi_status status;
	u64 tmp;
	acpi_handle handle;
		struct virtual_gpio_data *gd =
		container_of(work, struct virtual_gpio_data, periodic_work);
	struct device *dev = &gd->pdev->dev;

	handle = ACPI_HANDLE(dev);
	if (!handle)
		return;
	mutex_lock(&gd->vg_mutex);
	if (gpe_en_reg & GPE0A_PME_EN_BIT) {
		/* Clear the STS Bit */
		temp = gpe_en_reg&(~GPE0A_PME_EN_BIT);
		outl(temp, GPE0A_EN_PORT);
	}
	if (boot_cpu_data.x86_model != BYT_MODEL) {
		status = acpi_evaluate_object(handle,
						"_E02", NULL, NULL);
		if (ACPI_FAILURE(status))
			dev_err(dev, "_E02 call failed in virtual gpio\n");
	}

	if (gpe_sts_reg & GPE0A_PME_STS_BIT)
			outl(GPE0A_PME_STS_BIT, GPE0A_STS_PORT);

	if (gpe_en_reg & GPE0A_PME_EN_BIT)
		outl(gpe_en_reg, GPE0A_EN_PORT);
	mutex_unlock(&gd->vg_mutex);
	return;
}


static void acpi_virtual_gpio_request_interrupts(struct virtual_gpio_data *gd)
{
	struct device *dev = &gd->pdev->dev;
	struct acpi_buffer buf = {ACPI_ALLOCATE_BUFFER, NULL};
	struct acpi_resource *res;
	acpi_handle handle, ev_handle;
	acpi_status status;
	unsigned int pin;
	int ret;
	char ev_name[5];
	unsigned long irq_flags;
	handle = ACPI_HANDLE(dev);
	if (!handle)
		return;

	status = acpi_get_event_resources(handle, &buf);
	if (ACPI_FAILURE(status)) {
		if (buf.pointer)
			ACPI_FREE(buf.pointer);
		return;
	}

	/*
	 * If a GPIO interrupt has an ACPI event handler method, set
	 * up an interrupt handler that calls the ACPI event handler.
	 */
	for (res = buf.pointer;
	     res && (res->type != ACPI_RESOURCE_TYPE_END_TAG);
	     res = ACPI_NEXT_RESOURCE(res)) {

		if (res->type != ACPI_RESOURCE_TYPE_GPIO ||
		    res->data.gpio.connection_type !=
		    ACPI_RESOURCE_GPIO_TYPE_INT)
			continue;

		pin = res->data.gpio.pin_table[0];
		dev_info(dev, "virtual gpio pin = %d\n", pin);
		if (pin > 255)
			continue;

		sprintf(ev_name, "_%c%02X",
			res->data.gpio.triggering ? 'E' : 'L', pin);
		status = acpi_get_handle(handle, ev_name, &ev_handle);
		if (ACPI_FAILURE(status))
			continue;

		irq_flags = IRQF_SHARED | IRQF_NO_SUSPEND;
		if (acpi_gbl_reduced_hardware)
			irq_flags = 0;

			ret = devm_request_irq(dev, gd->irq,
						virtual_gpio_irq_handler,
						irq_flags,
						VIRTUAL_GPIO_DRIVER_NAME,
						gd);

		if (ret)
			dev_err(dev,
				"Failed to request IRQ %d ACPI event handler\n",
				gd->irq);
	}
	if (buf.pointer)
		ACPI_FREE(buf.pointer);
}

static int virtual_gpio_probe(struct platform_device *pdev)
{
	struct virtual_gpio_data *gd;
	struct device *dev = &pdev->dev;
	u32 gpe_en_reg = inl(GPE0A_EN_PORT);
	u32 ret = 0;

	gd = devm_kzalloc(dev, sizeof(struct virtual_gpio_data), GFP_KERNEL);
	if (!gd) {
		dev_err(dev, "can't allocate memory for virtual_gpio\n");
		return -ENOMEM;
	}

	gd->pdev = pdev;
	platform_set_drvdata(pdev, gd);

	/* set up interrupt */
	gd->irq = platform_get_irq(pdev, 0);

	INIT_WORK(&gd->periodic_work, virtual_gpio_irq_handler_isr);
	vgpio_wq = create_singlethread_workqueue(dev_name(dev));
	if (!vgpio_wq) {
		dev_err(dev, "virtual gpio worker thread create failed\n");
		ret = -ENOMEM;
		return ret;
	}
	acpi_virtual_gpio_request_interrupts(gd);
	mutex_init(&gd->vg_mutex);
	pm_runtime_enable(dev);
	gpe_en_reg |= GPE0A_PME_EN_BIT;
	outl(gpe_en_reg, GPE0A_EN_PORT);

	return ret;
}

static int virtual_gpio_runtime_suspend(struct device *dev)
{
	return 0;
}

static int virtual_gpio_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops virtual_gpio_pm_ops = {
	.runtime_suspend = virtual_gpio_runtime_suspend,
	.runtime_resume = virtual_gpio_runtime_resume,
};

static const struct acpi_device_id virtual_gpio_acpi_ids[] = {
	{ "INT0002", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, virtual_gpio_acpi_ids);

static int virtual_gpio_remove(struct platform_device *pdev)
{
	destroy_workqueue(vgpio_wq);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void virtual_gpio_shutdown(struct platform_device *pdev)
{
}

static struct platform_driver virtual_gpio_driver = {
	.driver = {
		.name			= VIRTUAL_GPIO_DRIVER_NAME,
		.owner			= THIS_MODULE,
		.pm			= &virtual_gpio_pm_ops,
		.acpi_match_table	= ACPI_PTR(virtual_gpio_acpi_ids),
	},
	.probe	= virtual_gpio_probe,
	.remove	= virtual_gpio_remove,
	.shutdown = virtual_gpio_shutdown,
};

module_platform_driver(virtual_gpio_driver);

MODULE_AUTHOR("Dyut Kumar Sil <dyut.k.sil@intel.com>");
MODULE_DESCRIPTION("Intel (R) Virtual GPIO Controller Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VIRTUAL_GPIO_DRIVER_VERSION);
