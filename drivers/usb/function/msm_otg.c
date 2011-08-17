/* drivers/usb/otg/msm_otg.c
 *
 * OTG Driver for HighSpeed USB
 *
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <mach/msm_otg.h>
#include <mach/msm_hsusb.h>
#include <mach/msm_hsusb_hw.h>
#include <mach/board.h>

#define MSM_USB_BASE (xceiv->regs)

#define A_HOST 0
#define B_DEVICE 1
#define A_TO_B 0
#define B_TO_A 1

static struct msm_otg_transceiver *xceiv;

struct msm_otg_transceiver *msm_otg_get_transceiver(void)
{
	if (xceiv)
		get_device(xceiv->dev);
	return xceiv;
}
EXPORT_SYMBOL(msm_otg_get_transceiver);

void msm_otg_put_transceiver(struct msm_otg_transceiver *xceiv)
{
	if (xceiv)
		put_device(xceiv->dev);
}
EXPORT_SYMBOL(msm_otg_put_transceiver);

static void msm_otg_set_clk(int on)
{
	if (on) {
		clk_enable(xceiv->clk);
		clk_enable(xceiv->pclk);
	} else {
		clk_disable(xceiv->clk);
		clk_disable(xceiv->pclk);
	}
}

static inline int is_host(void)
{
	int ret;

	ret = (OTGSC_ID & readl(USB_OTGSC)) ? 0 : 1;
	return ret;
}

static void msm_otg_enable(void)
{
	msm_otg_set_clk(1);
	/* Enable ID interrupts */
	writel(readl(USB_OTGSC) | OTGSC_IDIE, USB_OTGSC);

	if (is_host()) {
		pr_info("%s: configuring USB in host mode\n", __func__);
		xceiv->hcd_ops->request(xceiv->hcd_ops->handle, REQUEST_START);
		xceiv->state = A_HOST;
	} else {
		pr_info("%s: configuring USB in device mode\n", __func__);
		xceiv->dcd_ops->request(xceiv->dcd_ops->handle, REQUEST_START);
		xceiv->state = B_DEVICE;
	}
	msm_otg_set_clk(0);
	xceiv->active = 1;
	wake_lock_timeout(&xceiv->wlock, HZ/2);
	enable_irq(xceiv->irq);
}

static void msm_otg_disable(int mode)
{
	unsigned long flags;

	spin_lock_irqsave(&xceiv->lock, flags);
	xceiv->active = 0;
	spin_unlock_irqrestore(&xceiv->lock, flags);

	pr_info("%s: OTG is disabled\n", __func__);

	if (mode != xceiv->state)
		return;
	switch (mode) {
	case A_HOST:
		if (xceiv->state == A_HOST) {
			pr_info("%s: configuring USB in device mode\n",
					__func__);
			xceiv->dcd_ops->request(xceiv->dcd_ops->handle,
							REQUEST_START);
			xceiv->state = B_DEVICE;
		}
		break;
	case B_DEVICE:
		if (xceiv->state == B_DEVICE) {
			pr_info("%s: configuring USB in host mode\n",
					__func__);
			xceiv->hcd_ops->request(xceiv->hcd_ops->handle,
							REQUEST_START);
			xceiv->state = A_HOST;
		}
		break;
	}

}

static void msm_otg_do_work(struct work_struct *w)
{
	switch (xceiv->state) {
	case A_HOST:
		if (xceiv->flags == A_TO_B) {
			xceiv->hcd_ops->request(xceiv->hcd_ops->handle,
							REQUEST_STOP);
			pr_info("%s: configuring USB in device mode\n",
					__func__);
			xceiv->dcd_ops->request(xceiv->dcd_ops->handle,
							REQUEST_START);
			xceiv->state = B_DEVICE;
		}
		break;
	case B_DEVICE:
		if (xceiv->flags == B_TO_A) {
			xceiv->dcd_ops->request(xceiv->dcd_ops->handle,
							REQUEST_STOP);
			pr_info("%s: configuring USB in host mode\n",
					__func__);
			xceiv->hcd_ops->request(xceiv->hcd_ops->handle,
							REQUEST_START);
			xceiv->state = A_HOST;
		}
		break;
	}
	wake_lock_timeout(&xceiv->wlock, HZ/2);
	enable_irq(xceiv->irq);
}

static irqreturn_t msm_otg_irq(int irq, void *data)
{
	u32 otgsc;
	u32 temp;

	if (!xceiv->active)
		return IRQ_HANDLED;

	if (xceiv->in_lpm)
		return IRQ_HANDLED;

	otgsc = readl(USB_OTGSC);
	temp = otgsc & ~OTGSC_INTR_STS_MASK;
	if (otgsc & OTGSC_IDIS) {
		wake_lock(&xceiv->wlock);
		if (is_host()) {
			xceiv->flags = B_TO_A;
			schedule_work(&xceiv->work);
		} else {
			xceiv->flags = A_TO_B;
			schedule_work(&xceiv->work);
		}
		disable_irq(xceiv->irq);
		writel(temp | OTGSC_IDIS, USB_OTGSC);
	}

	return IRQ_HANDLED;

}

static DEFINE_MUTEX(otg_register_lock);

static int msm_otg_set_peripheral(struct msm_otg_transceiver *xceiv,
					struct msm_otg_ops *ops)
{
	int ret = 0;

	mutex_lock(&otg_register_lock);
	if (!xceiv) {
		ret = -EINVAL;
		goto unlock;
	}
	if (!ops) {
		xceiv->dcd_ops = NULL;
		pr_info("%s: Peripheral driver is deregistered with OTG\n",
				__func__);
		msm_otg_disable(B_DEVICE);
		goto unlock;
	}
	if (xceiv->dcd_ops) {
		ret = -EBUSY;
		goto unlock;
	}

	xceiv->dcd_ops = ops;
	xceiv->dcd_ops->request(xceiv->dcd_ops->handle, REQUEST_STOP);
	if (xceiv->hcd_ops)
		msm_otg_enable();
unlock:
	mutex_unlock(&otg_register_lock);
	return ret;
}

static int msm_otg_set_host(struct msm_otg_transceiver *xceiv,
				struct msm_otg_ops *hcd_ops)
{
	int ret = 0;

	mutex_lock(&otg_register_lock);
	if (!xceiv) {
		ret = -EINVAL;
		goto unlock;
	}
	if (!hcd_ops) {
		xceiv->hcd_ops = NULL;
		pr_info("%s: Host driver is deregistered with OTG\n",
				__func__);
		msm_otg_disable(A_HOST);
		goto unlock;
	}
	if (xceiv->hcd_ops) {
		ret = -EBUSY;
		goto unlock;
	}

	xceiv->hcd_ops = hcd_ops;
	xceiv->hcd_ops->request(xceiv->hcd_ops->handle, REQUEST_STOP);
	if (xceiv->dcd_ops)
		msm_otg_enable();

unlock:
	mutex_unlock(&otg_register_lock);
	return ret;
}

static int msm_otg_set_suspend(struct msm_otg_transceiver *otg, int suspend)
{
	unsigned long flags;

	spin_lock_irqsave(&xceiv->lock, flags);
	xceiv->in_lpm = suspend;
	spin_unlock_irqrestore(&xceiv->lock, flags);
	return 0;
}

static int __init msm_otg_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	xceiv = kzalloc(sizeof(struct msm_otg_transceiver), GFP_KERNEL);
	if (!xceiv)
		return -ENOMEM;

	xceiv->clk = clk_get(NULL, "usb_hs_clk");
	if (IS_ERR(xceiv->clk)) {
		ret = PTR_ERR(xceiv->clk);
		goto free_xceiv;
	}
	xceiv->pclk = clk_get(NULL, "usb_hs_pclk");
	if (IS_ERR(xceiv->clk)) {
		ret = PTR_ERR(xceiv->pclk);
		goto put_clk;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto put_pclk;
	}

	xceiv->regs = ioremap(res->start, resource_size(res));
	if (!xceiv->regs) {
		ret = -ENOMEM;
		goto put_pclk;
	}
	xceiv->irq = platform_get_irq(pdev, 0);
	if (!xceiv->irq) {
		ret = -ENODEV;
		goto free_regs;
	}

	/* disable interrupts before requesting irq */
	msm_otg_set_clk(1);
	writel(0, USB_USBINTR);
	writel(readl(USB_OTGSC) & ~OTGSC_INTR_MASK, USB_OTGSC);
	msm_otg_set_clk(0);

	ret = request_irq(xceiv->irq, msm_otg_irq, IRQF_SHARED,
					"msm_otg", pdev);
	if (ret)
		goto free_regs;
	disable_irq(xceiv->irq);

	INIT_WORK(&xceiv->work, msm_otg_do_work);
	spin_lock_init(&xceiv->lock);
	wake_lock_init(&xceiv->wlock, WAKE_LOCK_SUSPEND, "usb_otg");
	wake_lock(&xceiv->wlock);

	xceiv->set_host = msm_otg_set_host;
	xceiv->set_peripheral = msm_otg_set_peripheral;
	xceiv->set_suspend = msm_otg_set_suspend;

	return 0;
free_regs:
	iounmap(xceiv->regs);
put_pclk:
	clk_put(xceiv->pclk);
put_clk:
	clk_put(xceiv->clk);
free_xceiv:
	kfree(xceiv);
	return ret;

}

static int __exit msm_otg_remove(struct platform_device *pdev)
{
	cancel_work_sync(&xceiv->work);
	free_irq(xceiv->irq, pdev);
	iounmap(xceiv->regs);
	clk_put(xceiv->pclk);
	clk_put(xceiv->clk);
	kfree(xceiv);
	return 0;
}

static struct platform_driver msm_otg_driver = {
	.remove = __exit_p(msm_otg_remove),
	.driver = {
		.name = "msm_hsusb_otg",
		.owner = THIS_MODULE,
	},
};

static int __init msm_otg_init(void)
{
	return platform_driver_probe(&msm_otg_driver, msm_otg_probe);
}

static void __exit msm_otg_exit(void)
{
	platform_driver_unregister(&msm_otg_driver);
}

subsys_initcall(msm_otg_init);
module_exit(msm_otg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM USB OTG driver");
MODULE_VERSION("1.00");
