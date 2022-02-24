/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>


#include <mtu3.h>
#include <mtu3_hal.h>
#include <mtk_spm_resource_req.h>
#include <mtk_idle.h>
#include "mtu3_priv.h"

static int dpidle_status = USB_DPIDLE_ALLOWED;
static DEFINE_SPINLOCK(usb_hal_dpidle_lock);
static void issue_dpidle_timer(void);
static void init_usb_audio_idle(void);

static void __iomem *infra_ao_base;

#define DPIDLE_TIMER_INTERVAL_MS 30

int get_ssusb_ext_rscs(struct ssusb_mtk *ssusb)
{
	struct device *dev = ssusb->dev;
	struct ssusb_priv *priv;

	/* all elements are set to ZERO as default value */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->vusb10 = devm_regulator_get(dev, "va09");
	if (IS_ERR(priv->vusb10)) {
		dev_info(dev, "failed to get vusb10\n");
		return PTR_ERR(priv->vusb10);
	}

	/* private mode setting */
	ssusb->force_vbus = true;
	ssusb->u1u2_disable = true;
	ssusb->u3_loopb_support = true;


	ssusb->priv_data = priv;

	init_usb_audio_idle();

	return 0;
}

static int ssusb_host_clk_on(struct ssusb_mtk *ssusb)
{
	return 0;
}

static int ssusb_host_clk_off(struct ssusb_mtk *ssusb)
{
	return 0;
}

static int ssusb_sysclk_on(struct ssusb_mtk *ssusb)
{
	int ret = 0;

	ret = clk_prepare_enable(ssusb->sys_clk);
	if (ret)
		dev_info(ssusb->dev, "failed to enable sys_clk\n");

	ret = clk_prepare_enable(ssusb->ref_clk);
	if (ret)
		dev_info(ssusb->dev, "failed to enable ref_clk\n");

	return ret;
}

static void ssusb_sysclk_off(struct ssusb_mtk *ssusb)
{
	clk_disable_unprepare(ssusb->sys_clk);
	clk_disable_unprepare(ssusb->ref_clk);
}

int ssusb_clk_on(struct ssusb_mtk *ssusb, int host_mode)
{
	ssusb_dpidle_request(USB_DPIDLE_FORBIDDEN);
	if (host_mode) {
		ssusb_sysclk_on(ssusb);
		ssusb_host_clk_on(ssusb);
	} else {
		ssusb_sysclk_on(ssusb);
	}
	return 0;
}

int ssusb_clk_off(struct ssusb_mtk *ssusb, int host_mode)
{
	if (host_mode) {
		ssusb_host_clk_off(ssusb);
		ssusb_sysclk_off(ssusb);
	} else {
		ssusb_sysclk_off(ssusb);
	}
	ssusb_dpidle_request(USB_DPIDLE_ALLOWED);
	return 0;
}

int ssusb_ext_pwr_on(struct ssusb_mtk *ssusb, int mode)
{
	int ret = 0;
	struct ssusb_priv *priv;

	priv = ssusb->priv_data;
	ret = regulator_enable(priv->vusb10);
	if (ret)
		dev_info(ssusb->dev, "failed to enable vusb10\n");
	return ret;
}

int ssusb_ext_pwr_off(struct ssusb_mtk *ssusb, int mode)
{
	int ret = 0;
	struct ssusb_priv *priv;

	priv = ssusb->priv_data;
	ret = regulator_disable(priv->vusb10);
	if (ret)
		dev_info(ssusb->dev, "failed to disable vusb10\n");
	return ret;
}

static void dpidle_timer_wakeup_func(unsigned long data)
{
	struct timer_list *timer = (struct timer_list *)data;

	if (dpidle_status == USB_DPIDLE_TIMER)
		issue_dpidle_timer();
	kfree(timer);
}

static void issue_dpidle_timer(void)
{
	struct timer_list *timer;

	timer = kzalloc(sizeof(struct timer_list), GFP_ATOMIC);
	if (!timer)
		return;

	init_timer(timer);
	timer->function = dpidle_timer_wakeup_func;
	timer->data = (unsigned long)timer;
	timer->expires = jiffies + msecs_to_jiffies(DPIDLE_TIMER_INTERVAL_MS);
	add_timer(timer);
}

void ssusb_dpidle_request(int mode)
{
	unsigned long flags;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);
	static int skip_cnt;

	spin_lock_irqsave(&usb_hal_dpidle_lock, flags);

	/* update dpidle_status */
	dpidle_status = mode;
	/*usb_audio_req(false);*/

	switch (mode) {

	case USB_DPIDLE_ALLOWED:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, 0);
		mtu3_printk(K_NOTICE, "DPIDLE_ALLOWED\n");
		break;
	case USB_DPIDLE_FORBIDDEN:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, SPM_RESOURCE_ALL);

		if (__ratelimit(&ratelimit))
			mtu3_printk(K_NOTICE,
				"DPIDLE_FORBIDDEN\n");
		break;
	case USB_DPIDLE_SRAM:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB,
				SPM_RESOURCE_CK_26M | SPM_RESOURCE_MAINPLL);

		if (__ratelimit(&ratelimit)) {
			mtu3_printk(K_NOTICE,
				"DPIDLE_SRAM, skip<%d>\n", skip_cnt);
			skip_cnt = 0;
		} else
			skip_cnt++;
		break;
	case USB_DPIDLE_AUDIO:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, 0);
		/*usb_audio_req(true);*/

		if (__ratelimit(&ratelimit)) {
			mtu3_printk(K_NOTICE,
				"DPIDLE_AUDIO, skip<%d>\n", skip_cnt);
			skip_cnt = 0;
		} else
			skip_cnt++;
		break;
	case USB_DPIDLE_TIMER:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, SPM_RESOURCE_CK_26M);
		mtu3_printk(K_NOTICE, "DPIDLE_TIMER\n");
		issue_dpidle_timer();
		break;
	case USB_DPIDLE_SUSPEND:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB,
				SPM_RESOURCE_MAINPLL | SPM_RESOURCE_CK_26M |
				SPM_RESOURCE_AXI_BUS);
		mtu3_printk(K_NOTICE, "DPIDLE_SUSPEND\n");
		break;
	case USB_DPIDLE_RESUME:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, 0);
		mtu3_printk(K_NOTICE, "DPIDLE_RESUME\n");
		break;
	default:
		mtu3_printk(K_WARNIN, "[ERROR] Are you kidding!?!?\n");
		break;
	}

	spin_unlock_irqrestore(&usb_hal_dpidle_lock, flags);
}

static int usbaudio_idle_notify_call(struct notifier_block *nfb,
				unsigned long id,
				void *arg)
{
	switch (id) {
	case NOTIFY_DPIDLE_ENTER:
	case NOTIFY_SOIDLE_ENTER:
	case NOTIFY_DPIDLE_LEAVE:
	case NOTIFY_SOIDLE_LEAVE:
	case NOTIFY_SOIDLE3_ENTER:
	case NOTIFY_SOIDLE3_LEAVE:
	default:
		/* do nothing */
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block usbaudio_idle_nfb = {
	.notifier_call = usbaudio_idle_notify_call,
};

static void init_usb_audio_idle(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	if (node) {
		infra_ao_base = of_iomap(node, 0);
		if (infra_ao_base)
			mtk_idle_notifier_register(&usbaudio_idle_nfb);
	}
}



