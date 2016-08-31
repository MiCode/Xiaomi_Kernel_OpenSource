/*
 *  Shared Transport Host wake up driver
 *  	For protocols registered over Shared Transport
 *  Copyright (C) 2011-2012 Texas Instruments
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <linux/ti_wilink_st.h>

#include <linux/regulator/consumer.h>

#define VERSION		"1.2"
#define FLAG_RESET	0x00
#define HOST_WAKE	0x01
#define IRQ_WAKE	0x02

#define CHANNEL_ACL	0x02
#define CHANNEL_EVT	0x04
#define CHANNEL_FM	0x08
#define CHANNEL_GPS	0x09
#define CHANNEL_NFC	0x0C

struct st_host_wake_info {
	unsigned host_wake_irq;
	struct regulator *vdd_3v3;
	struct regulator *vdd_1v8;
	unsigned int supp_proto_reg;
};

static unsigned long flags;
static struct st_host_wake_info *bsi;
static char  dev_id[12] ="st_host_wake";

void st_host_wake_notify(int chan_id, int reg_state)
{
	/* HOST_WAKE to be set after all BT channels including CHANNEL_SCO
	 * is registered
	 */
	if(chan_id == CHANNEL_ACL || chan_id == CHANNEL_EVT)
		return;

#ifndef CONFIG_ST_HOST_WAKE_GPS
	if(chan_id == CHANNEL_GPS) {
		pr_info("CONFIG_ST_HOST_WAKE_GPS not set hence reject");
		return;
	}
#endif

#ifndef CONFIG_ST_HOST_WAKE_FM
	if(chan_id == CHANNEL_FM) {
		pr_info("CONFIG_ST_HOST_WAKE_FM not set hence reject");
		return;
	}
#endif

#ifndef CONFIG_ST_HOST_WAKE_NFC
	if(chan_id == CHANNEL_NFC) {
		pr_info("CONFIG_ST_HOST_WAKE_NFC not set hence reject");
		return;
	}
#endif
	switch(reg_state) {
		case ST_PROTO_REGISTERED:
			pr_info("Channel %d registered", chan_id);
			bsi->supp_proto_reg++;
			set_bit(HOST_WAKE, &flags);
			pr_info("HOST_WAKE set");
			break;

		case ST_PROTO_UNREGISTERED:
			pr_info("Channel %d un-registered", chan_id);
			bsi->supp_proto_reg--;

			if(!bsi->supp_proto_reg) {
				pr_info("All supported protocols un-registered");
				if(bsi && test_bit(IRQ_WAKE, &flags)) {
					pr_info("disabling wake_irq after unregister");
					disable_irq_wake(bsi->host_wake_irq);
					clear_bit(IRQ_WAKE, &flags);
				}

				clear_bit(HOST_WAKE, &flags);
				pr_info("HOST_WAKE cleared");
			}
			break;

		default:
			break;
	}
}
EXPORT_SYMBOL(st_host_wake_notify);

void st_vltg_regulation(int state)
{
	pr_info("%s with state %d", __func__, state);

	if(ST_VLTG_REG_ENABLE == state) {
		if (bsi->vdd_3v3)
			regulator_enable(bsi->vdd_3v3);
		if (bsi->vdd_1v8)
			regulator_enable(bsi->vdd_1v8);

	} else if(ST_VLTG_REG_DISABLE == state) {
		if (bsi->vdd_3v3)
			regulator_disable(bsi->vdd_3v3);
		if (bsi->vdd_1v8)
			regulator_disable(bsi->vdd_1v8);

	} else {
		pr_warn("Unknown voltage regulation state");
	}
}
EXPORT_SYMBOL(st_vltg_regulation);

static irqreturn_t st_host_wake_isr(int irq, void *dev_id)
{
	pr_debug("%s", __func__);

	return IRQ_HANDLED;
}

static int st_host_wake_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res = NULL;

	pr_info("TI Host Wakeup Driver [Ver %s]", VERSION);

	bsi = kzalloc(sizeof(struct st_host_wake_info), GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	bsi->vdd_3v3 = regulator_get(&pdev->dev, "vdd_st_3v3");
	if (IS_ERR_OR_NULL(bsi->vdd_3v3)) {
		pr_warn("%s: regulator vdd_st_3v3 not available\n", __func__);
		bsi->vdd_3v3 = NULL;
	}
	bsi->vdd_1v8 = regulator_get(&pdev->dev, "vddio_st_1v8");
	if (IS_ERR_OR_NULL(bsi->vdd_1v8)) {
		pr_warn("%s: regulator vddio_st_1v8 not available\n", __func__);
		bsi->vdd_1v8 = NULL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						"host_wake");
	if (!res) {
		pr_err("couldn't find host_wake irq\n");
		ret = -ENODEV;
		goto free_bsi;
	}

	bsi->host_wake_irq = res->start;
	clear_bit(IRQ_WAKE, &flags);

	if (bsi->host_wake_irq < 0) {
		pr_err("couldn't find host_wake irq");
		ret = -ENODEV;
		goto free_bsi;
	}

	if (res->flags & IORESOURCE_IRQ_LOWEDGE)
		ret = request_irq(bsi->host_wake_irq, st_host_wake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING,
				"bluetooth hostwake", dev_id);
	else
		ret = request_irq(bsi->host_wake_irq, st_host_wake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_RISING,
				"bluetooth hostwake", dev_id);

	if (ret < 0) {
		pr_err("Couldn't acquire HOST_WAKE IRQ");
		goto free_bsi;
	}

	clear_bit(HOST_WAKE, &flags);
	bsi->supp_proto_reg = 0;

	goto finish;

free_bsi:
	kfree(bsi);
finish:
	return ret;
}

static int st_host_wake_remove(struct platform_device *pdev)
{
	pr_debug("%s", __func__);

	free_irq(bsi->host_wake_irq, dev_id);

	if (bsi->vdd_3v3)
		regulator_put(bsi->vdd_3v3);
	if (bsi->vdd_1v8)
		regulator_put(bsi->vdd_1v8);

	kfree(bsi);

	return 0;
}

static int st_host_wake_resume(struct platform_device *pdev)
{
	pr_info("%s", __func__);

	if (test_bit(HOST_WAKE, &flags) && test_bit(IRQ_WAKE, &flags)) {
		pr_info("disable the host_wake irq");
		disable_irq_wake(bsi->host_wake_irq);
		clear_bit(IRQ_WAKE, &flags);
	}

	return 0;
}

static int st_host_wake_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int retval = 0;

	pr_info("%s", __func__);

	if (test_bit(HOST_WAKE, &flags) && (!test_bit(IRQ_WAKE, &flags))) {
		retval = enable_irq_wake(bsi->host_wake_irq);
		if (retval < 0) {
			pr_err("Couldn't enable HOST_WAKE as wakeup"
					"interrupt retval %d\n", retval);
			goto fail;
		}
		set_bit(IRQ_WAKE, &flags);
		pr_info("enabled the host_wake irq");
	}
fail:
	return retval;
}


static struct platform_driver st_host_wake_driver = {
	.probe = st_host_wake_probe,
	.remove = st_host_wake_remove,
	.suspend = st_host_wake_suspend,
	.resume = st_host_wake_resume,
	.driver = {
		.name = "st_host_wake",
		.owner = THIS_MODULE,
	},
};

static int __init st_host_wake_init(void)
{
	int retval = 0;

	pr_debug("%s", __func__);

	retval = platform_driver_register(&st_host_wake_driver);
	if(retval)
		pr_err("st_host_wake_init failed");

	return retval;
}

static void __exit st_host_wake_exit(void)
{
	pr_debug("%s", __func__);

	if (bsi == NULL)
		return;

	platform_driver_unregister(&st_host_wake_driver);
}

module_init(st_host_wake_init);
module_exit(st_host_wake_exit);

MODULE_DESCRIPTION("TI Host Wakeup Driver [Ver %s]" VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
