/*
 * arch/arm/mach-tegra/baseband-xmm-power2.c
 *
 * Copyright (C) 2011-2013, NVIDIA Corporation. All Rights Reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <mach/usb_phy.h>
#include "baseband-xmm-power.h"
#include "board.h"
#include "devices.h"

static unsigned long XYZ = 1000 * 1000000 + 800 * 1000 + 500;

module_param(modem_ver, ulong, 0644);
MODULE_PARM_DESC(modem_ver,
	"baseband xmm power2 - modem software version");
module_param(modem_flash, ulong, 0644);
MODULE_PARM_DESC(modem_flash,
	"baseband xmm power2 - modem flash (1 = flash, 0 = flashless)");
module_param(modem_pm, ulong, 0644);
MODULE_PARM_DESC(modem_pm,
	"baseband xmm power2 - modem power management (1 = pm, 0 = no pm)");
module_param(XYZ, ulong, 0644);
MODULE_PARM_DESC(XYZ,
	"baseband xmm power2 - timing parameters X/Y/Z delay in ms");

static struct workqueue_struct *workqueue;
static bool free_ipc_ap_wake_irq;
static enum ipc_ap_wake_state_t ipc_ap_wake_state;

static irqreturn_t xmm_power2_ipc_ap_wake_irq(int irq, void *dev_id)
{
	int value;
	struct xmm_power_data *data = dev_id;
	struct baseband_power_platform_data *pdata = data->pdata;

	/* check for platform data */
	if (!pdata)
		return IRQ_HANDLED;

	value = gpio_get_value(pdata->modem.xmm.ipc_ap_wake);

	/* IPC_AP_WAKE state machine */
	if (unlikely(ipc_ap_wake_state < IPC_AP_WAKE_IRQ_READY))
		pr_err("%s - spurious irq\n", __func__);
	else if (ipc_ap_wake_state == IPC_AP_WAKE_IRQ_READY) {
		if (!value) {
			pr_debug("%s: IPC_AP_WAKE_IRQ_READY got falling edge\n",
						__func__);
			/* go to IPC_AP_WAKE_INIT2 state */
			ipc_ap_wake_state = IPC_AP_WAKE_INIT2;
			/* queue work */
			data->state =
				BBXMM_WORK_INIT_FLASHLESS_PM_STEP2;
			queue_work(workqueue, &data->work);
		} else
			pr_debug("%s: IPC_AP_WAKE_IRQ_READY"
				" wait for falling edge\n", __func__);
	} else {
		if (!value) {
			pr_debug("%s - falling\n", __func__);
			ipc_ap_wake_state = IPC_AP_WAKE_L;
		} else {
			pr_debug("%s - rising\n", __func__);
			ipc_ap_wake_state = IPC_AP_WAKE_H;
		}
		return xmm_power_ipc_ap_wake_irq(value);
	}

	return IRQ_HANDLED;
}

static void xmm_power2_step1(struct work_struct *work)
{
	struct xmm_power_data *data =
			container_of(work, struct xmm_power_data, work);
	struct baseband_power_platform_data *pdata = data->pdata;
	int X = XYZ / 1000000;

	pr_info("%s {\n", __func__);

	/* check for platform data */
	if (!pdata)
		return;

	/* unregister usb host controller */
	if (pdata->hsic_unregister)
		pdata->hsic_unregister(&data->hsic_device);
	else
		pr_err("%s: hsic_unregister is missing\n", __func__);

	/* wait X ms */
	msleep(X);

	/* set IPC_HSIC_ACTIVE low */
	gpio_set_value(pdata->modem.xmm.ipc_hsic_active, 0);

	pr_info("%s }\n", __func__);
}

static void xmm_power2_step2(struct work_struct *work)
{
	struct xmm_power_data *data =
			container_of(work, struct xmm_power_data, work);
	struct baseband_power_platform_data *pdata = data->pdata;
	int X = XYZ / 1000000;
	int Y = XYZ / 1000 - X * 1000;
	int Z = XYZ % 1000;

	pr_info("%s {\n", __func__);

	/* wait Y ms */
	msleep(Y);

	/* register usb host controller */
	if (pdata->hsic_register)
		data->hsic_device = pdata->hsic_register(pdata->ehci_device);
	else
		pr_err("%s: hsic_register is missing\n", __func__);

	/* wait Z ms */
	msleep(Z);

	/* set IPC_HSIC_ACTIVE high */
	gpio_set_value(pdata->modem.xmm.ipc_hsic_active, 1);

	/* queue work function to check if enumeration succeeded */
	data->state = BBXMM_WORK_INIT_FLASHLESS_PM_STEP3;
	queue_work(workqueue, &data->work);

	pr_info("%s }\n", __func__);
}

static void xmm_power2_step3(struct work_struct *work)
{
	struct xmm_power_data *data =
			container_of(work, struct xmm_power_data, work);
	struct baseband_power_platform_data *pdata = data->pdata;
	int enum_success = 0;
	mm_segment_t oldfs;
	struct file *filp;

	pr_info("%s {\n", __func__);

	/* wait 1 sec */
	msleep(1000);

	/* check if enumeration succeeded */
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open("/dev/ttyACM0", O_RDONLY, 0);
	if (IS_ERR(filp) || (filp == NULL))
		pr_err("failed to open /dev/ttyACM0 %ld\n", PTR_ERR(filp));
	else {
		filp_close(filp, NULL);
		enum_success = 1;
	}
	set_fs(oldfs);

	/* if enumeration failed, attempt recovery pulse */
	if (!enum_success) {
		pr_info("attempting recovery pulse...\n");
		/* wait 20 ms */
		msleep(20);
		/* set IPC_HSIC_ACTIVE low */
		gpio_set_value(pdata->modem.xmm.ipc_hsic_active, 0);
		/* wait 20 ms */
		msleep(20);
		/* set IPC_HSIC_ACTIVE high */
		gpio_set_value(pdata->modem.xmm.ipc_hsic_active, 1);
		/* check if recovery pulse worked */
		data->state = BBXMM_WORK_INIT_FLASHLESS_PM_STEP4;
		queue_work(workqueue, &data->work);
	}

	pr_info("%s }\n", __func__);
}

static void xmm_power2_step4(struct work_struct *work)
{
	struct xmm_power_data *data =
			container_of(work, struct xmm_power_data, work);
	mm_segment_t oldfs;
	struct file *filp;
	int enum_success = 0;

	pr_info("%s {\n", __func__);

	/* check for platform data */
	if (!data)
		return;

	/* wait 500 ms */
	msleep(500);

	/* check if enumeration succeeded */
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open("/dev/ttyACM0", O_RDONLY, 0);
	if (IS_ERR(filp) || (filp == NULL))
		pr_err("failed to open /dev/ttyACM0 %ld\n", PTR_ERR(filp));
	else {
		filp_close(filp, NULL);
		enum_success = 1;
	}
	set_fs(oldfs);

	/* if recovery pulse did not fix enumeration, retry from beginning */
	if (!enum_success) {
		static int retry = 3;
		if (!retry) {
			pr_info("failed to enumerate modem software"
				" - too many retry attempts\n");
		} else {
			pr_info("recovery pulse failed to fix modem"
				" enumeration..."
				" restarting from beginning"
				" - attempt #%d\n",
				retry);
			--retry;
			ipc_ap_wake_state = IPC_AP_WAKE_IRQ_READY;
			data->state = BBXMM_WORK_INIT_FLASHLESS_PM_STEP1;
			queue_work(workqueue, &data->work);
		}
	}

	pr_info("%s }\n", __func__);
}

static void xmm_power2_work_func(struct work_struct *work)
{
	struct xmm_power_data *data =
			container_of(work, struct xmm_power_data, work);
	struct baseband_power_platform_data *pdata;
	int err;

	if (!data || !data->pdata)
		return;
	pdata = data->pdata;

	pr_debug("%s pdata->state=%d\n", __func__, data->state);

	switch (data->state) {
	case BBXMM_WORK_UNINIT:
		pr_debug("BBXMM_WORK_UNINIT\n");
		/* free baseband irq(s) */
		if (free_ipc_ap_wake_irq) {
			free_irq(gpio_to_irq(pdata->modem.xmm.ipc_ap_wake),
					data);
			free_ipc_ap_wake_irq = false;
		}
		break;
	case BBXMM_WORK_INIT:
		pr_debug("BBXMM_WORK_INIT\n");
		/* request baseband irq(s) */
		ipc_ap_wake_state = IPC_AP_WAKE_UNINIT;
		err = request_threaded_irq(
				gpio_to_irq(pdata->modem.xmm.ipc_ap_wake),
				NULL, xmm_power2_ipc_ap_wake_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"xmm_power2_ipc_ap_wake_irq", data);
		if (err < 0) {
			pr_err("%s - request irq IPC_AP_WAKE_IRQ failed\n",
				__func__);
			return;
		}
		free_ipc_ap_wake_irq = true;
		ipc_ap_wake_state = IPC_AP_WAKE_IRQ_READY;

		/* go to next state */
		data->state = (modem_flash && !modem_pm)
			? BBXMM_WORK_INIT_FLASH_STEP1
			: (modem_flash && modem_pm)
			? BBXMM_WORK_INIT_FLASH_PM_STEP1
			: (!modem_flash && modem_pm)
			? BBXMM_WORK_INIT_FLASHLESS_PM_STEP1
			: BBXMM_WORK_UNINIT;
		queue_work(workqueue, work);
		break;
	case BBXMM_WORK_INIT_FLASH_STEP1:
		pr_debug("BBXMM_WORK_INIT_FLASH_STEP1\n");
		pr_info("%s: flashed modem is not supported here\n", __func__);
		break;
	case BBXMM_WORK_INIT_FLASH_PM_STEP1:
		pr_debug("BBXMM_WORK_INIT_FLASH_PM_STEP1\n");
		pr_info("%s: flashed modem is not supported here\n", __func__);
		break;
	case BBXMM_WORK_INIT_FLASHLESS_PM_STEP1:
		/* start flashless modem enum process */
		pr_debug("BBXMM_WORK_INIT_FLASHLESS_PM_STEP1\n");
		xmm_power2_step1(work);
		break;
	case BBXMM_WORK_INIT_FLASHLESS_PM_STEP2:
		pr_debug("BBXMM_WORK_INIT_FLASHLESS_PM_STEP2\n");
		xmm_power2_step2(work);
		break;
	case BBXMM_WORK_INIT_FLASHLESS_PM_STEP3:
		pr_debug("BBXMM_WORK_INIT_FLASHLESS_PM_STEP3\n");
		xmm_power2_step3(work);
		break;
	case BBXMM_WORK_INIT_FLASHLESS_PM_STEP4:
		pr_debug("BBXMM_WORK_INIT_FLASHLESS_PM_STEP4\n");
		xmm_power2_step4(work);
		break;
	default:
		break;
	}
}

static int xmm_power2_probe(struct platform_device *device)
{
	pr_debug("%s\n", __func__);
	if (!device->dev.platform_data) {
		pr_err("%s: no platform data found\n", __func__);
		return -ENOMEM;
	}

	xmm_power_drv_data.pdata = device->dev.platform_data;

	/* create workqueue */
	pr_debug("%s: init work queue\n", __func__);
	workqueue = create_singlethread_workqueue("xmm_power2_wq");
	if (unlikely(!workqueue)) {
		pr_err("%s: cannot create workqueue\n", __func__);
		return -ENOMEM;
	}

	/* init work */
	pr_debug("%s: BBXMM_WORK_INIT\n", __func__);
	INIT_WORK(&xmm_power_drv_data.work, xmm_power2_work_func);
	xmm_power_drv_data.state = BBXMM_WORK_INIT;
	queue_work(workqueue, &xmm_power_drv_data.work);

	return 0;
}

static int xmm_power2_remove(struct platform_device *device)
{
	struct baseband_power_platform_data *pdata =
			device->dev.platform_data;
	struct xmm_power_data *data = &xmm_power_drv_data;

	pr_debug("%s\n", __func__);

	/* check for platform data */
	if (!data)
		return -ENODEV;

	/* free work queue */
	if (workqueue) {
		cancel_work_sync(&data->work);
		destroy_workqueue(workqueue);
	}

	/* free irq */
	if (free_ipc_ap_wake_irq) {
		free_irq(gpio_to_irq(pdata->modem.xmm.ipc_ap_wake), data);
		free_ipc_ap_wake_irq = false;
	}

	return 0;
}

#ifdef CONFIG_PM
static int xmm_power2_suspend(struct platform_device *device,
	pm_message_t state)
{
	struct baseband_power_platform_data *data =
			device->dev.platform_data;

	pr_debug("%s - nop\n", __func__);

	/* check for platform data */
	if (!data)
		return 0;

	return 0;
}

static int xmm_power2_resume(struct platform_device *device)
{
	struct baseband_power_platform_data *data =
			device->dev.platform_data;

	pr_debug("%s - nop\n", __func__);

	/* check for platform data */
	if (!data)
		return 0;

	return 0;
}
#endif

static struct platform_driver baseband_power2_driver = {
	.probe = xmm_power2_probe,
	.remove = xmm_power2_remove,
#ifdef CONFIG_PM
	.suspend = xmm_power2_suspend,
	.resume = xmm_power2_resume,
#endif
	.driver = {
		.name = "baseband_xmm_power2",
	},
};

static int __init xmm_power2_init(void)
{
	pr_debug("%s\n", __func__);

	return platform_driver_register(&baseband_power2_driver);
}

static void __exit xmm_power2_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&baseband_power2_driver);
}

module_init(xmm_power2_init)
module_exit(xmm_power2_exit)

MODULE_LICENSE("GPL");
