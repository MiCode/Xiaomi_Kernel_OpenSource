// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include "usb_boost.h"
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/interconnect.h>
#include <linux/cpufreq.h>
#include "usb_boost.h"
#include "dvfsrc-exp.h"

static struct pm_qos_request pm_qos_req;
static LIST_HEAD(usb_policy_list);
struct usb_policy {
	struct freq_qos_request    qos_req;
	struct cpufreq_policy      *policy;
	struct list_head           list;
};

static struct icc_path *usb_icc_path;
unsigned int peak_bw;
struct device *gdev;

static int freq_hold(struct act_arg_obj *arg)
{

	struct usb_policy *req_policy;
	struct cpufreq_policy *policy;
	int cpu, ret;

	USB_BOOST_DBG("\n");

	if (list_empty(&usb_policy_list)) {
		for_each_possible_cpu(cpu) {
			policy = cpufreq_cpu_get(cpu);
			if (!policy)
				continue;

			USB_BOOST_DBG("%s, policy: first:%d, min:%d, max:%d",
				__func__, policy->cpu, policy->min, policy->max);

			req_policy = kzalloc(sizeof(*req_policy), GFP_KERNEL);
			if (!req_policy)
				return -ENOMEM;

			req_policy->policy = policy;

			ret = freq_qos_add_request(&policy->constraints, &req_policy->qos_req, FREQ_QOS_MIN, 0);
			if (ret < 0) {
				USB_BOOST_NOTICE("%s: fail to add freq constraint (%d)\n",
					__func__, ret);
				return ret;
			}
			list_add_tail(&req_policy->list, &usb_policy_list);
		}
	}

	list_for_each_entry(req_policy, &usb_policy_list, list) {
		USB_BOOST_NOTICE("%s: update request cpu(%x)\n", __func__, req_policy->policy->cpu);
		freq_qos_update_request(&req_policy->qos_req, req_policy->policy->max);
	}

	return 0;
}

static int freq_release(struct act_arg_obj *arg)
{
	struct usb_policy *req_policy;

	USB_BOOST_DBG("\n");

	list_for_each_entry(req_policy, &usb_policy_list, list) {
		freq_qos_update_request(&req_policy->qos_req, 0);
	}
	return 0;
}

static int core_hold(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");

	/*Disable MCDI to save around 100us
	 *"Power ON CPU -> CPU context restore"
	 */

	cpu_latency_qos_update_request(&pm_qos_req, 50);

	return 0;
}

static int core_release(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");

	/*Enable MCDI*/
	cpu_latency_qos_update_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);

	return 0;
}

static int vcorefs_hold(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");

	if (usb_icc_path)
		icc_set_bw(usb_icc_path, 0, peak_bw);

	return 0;
}

static int vcorefs_release(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");

	if (usb_icc_path)
		icc_set_bw(usb_icc_path, 0, 0);

	return 0;
}

static int usb_boost_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	bool audio_boost;

	USB_BOOST_NOTICE("\n");

	/* mandatory, related resource inited*/
	usb_boost_init();

	/* mandatory, hook callback depends on platform */
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_HOLD, freq_hold);
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_RELEASE, freq_release);
	register_usb_boost_act(TYPE_CPU_CORE, ACT_HOLD, core_hold);
	register_usb_boost_act(TYPE_CPU_CORE, ACT_RELEASE, core_release);
	register_usb_boost_act(TYPE_DRAM_VCORE, ACT_HOLD, vcorefs_hold);
	register_usb_boost_act(TYPE_DRAM_VCORE, ACT_RELEASE, vcorefs_release);

	cpu_latency_qos_add_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);

	usb_icc_path = of_icc_get(&pdev->dev, "icc-bw");
	if (!usb_icc_path) {
		USB_BOOST_NOTICE("%s: fail to get icc path\n", __func__);
		return -1;
	}

	peak_bw = dvfsrc_get_required_opp_peak_bw(node, 0);
	USB_BOOST_NOTICE("%s: peak_bw(%x)\n", __func__, peak_bw);

	audio_boost = of_property_read_bool(node, "usb-audio");
	gdev = &pdev->dev;
	usb_audio_boost(audio_boost);

	return 0;
}

static int usb_boost_remove(struct platform_device *pdev)
{
	struct usb_policy *req_policy, *tmp;

	list_for_each_entry_safe(req_policy, tmp, &usb_policy_list, list) {
		freq_qos_remove_request(&req_policy->qos_req);
		list_del(&req_policy->list);
		kfree(req_policy);
	}

	return 0;
}

static const struct of_device_id usb_boost_of_match[] = {
	{.compatible = "mediatek,usb_boost"},
	{},
};

MODULE_DEVICE_TABLE(of, usb_boost_of_match);
static struct platform_driver usb_boost_driver = {
	.remove = usb_boost_remove,
	.probe = usb_boost_probe,
	.driver = {
		   .name = "mediatek,usb_boost",
		   .of_match_table = usb_boost_of_match,
		   },
};

static int __init usbboost(void)
{
	USB_BOOST_NOTICE("\n");

	platform_driver_register(&usb_boost_driver);

	return 0;
}
module_init(usbboost);

static void __exit clean(void)
{
}
module_exit(clean);
MODULE_LICENSE("GPL v2");

int audio_freq_hold(void)
{
	struct device_node *np = gdev->of_node;
	int cpu_freq_audio[3];
	struct usb_policy *req_policy;
	struct cpufreq_policy *policy;
	int cpu, ret;

	USB_BOOST_NOTICE("\n");

	if (list_empty(&usb_policy_list)) {
		for_each_possible_cpu(cpu) {
			policy = cpufreq_cpu_get(cpu);
			if (!policy)
				continue;

			USB_BOOST_DBG("%s, policy: first:%d, min:%d, max:%d",
				__func__, policy->cpu, policy->min, policy->max);

			req_policy = kzalloc(sizeof(*req_policy), GFP_KERNEL);
			if (!req_policy)
				return -ENOMEM;

			req_policy->policy = policy;

			ret = freq_qos_add_request(&policy->constraints, &req_policy->qos_req,
				FREQ_QOS_MIN, 0);
			if (ret < 0) {
				USB_BOOST_NOTICE("%s: fail to add freq constraint (%d)\n",
					__func__, ret);
				return ret;
			}
			list_add_tail(&req_policy->list, &usb_policy_list);
		}
	}

	if (of_device_is_compatible(np, "mediatek,mt6983-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6895-usb_boost")) {
		device_property_read_u32(gdev, "small-core", &(cpu_freq_audio[0]));
		device_property_read_u32(gdev, "medium-core", &(cpu_freq_audio[1]));
		device_property_read_u32(gdev, "big-core", &(cpu_freq_audio[2]));

		USB_BOOST_NOTICE("%s: request cpu freq(%d) (%d) (%d)\n", __func__,
			cpu_freq_audio[0], cpu_freq_audio[1], cpu_freq_audio[2]);

		list_for_each_entry(req_policy, &usb_policy_list, list) {
			if (req_policy->policy->cpu == 0 && cpu_freq_audio[0] > 0)
				freq_qos_update_request(&req_policy->qos_req, cpu_freq_audio[0]);

			if (req_policy->policy->cpu == 4 && cpu_freq_audio[1] > 0)
				freq_qos_update_request(&req_policy->qos_req, cpu_freq_audio[1]);

			if (req_policy->policy->cpu == 7 && cpu_freq_audio[2] > 0)
				freq_qos_update_request(&req_policy->qos_req, cpu_freq_audio[2]);
		}
	}

	if (of_device_is_compatible(np, "mediatek,mt6855-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6789-usb_boost")) {
		device_property_read_u32(gdev, "small-core", &(cpu_freq_audio[0]));
		device_property_read_u32(gdev, "big-core", &(cpu_freq_audio[1]));

		USB_BOOST_NOTICE("%s: request cpu freq(%d) (%d)\n", __func__,
			cpu_freq_audio[0], cpu_freq_audio[1]);

		list_for_each_entry(req_policy, &usb_policy_list, list) {
			if (req_policy->policy->cpu == 0 && cpu_freq_audio[0] > 0)
				ret = freq_qos_update_request(&req_policy->qos_req,
							      cpu_freq_audio[0]);
			if (!ret)
				USB_BOOST_NOTICE("%s: fail to update freq constraint (policy:%d)\n",
					__func__, req_policy->policy->cpu);

			if (req_policy->policy->cpu == 6 && cpu_freq_audio[1] > 0)
				ret = freq_qos_update_request(&req_policy->qos_req,
							      cpu_freq_audio[1]);
			if (!ret)
				USB_BOOST_NOTICE("%s: fail to update freq constraint (policy:%d)\n",
					__func__, req_policy->policy->cpu);
		}
	}

	return 0;
}

int audio_freq_release(void)
{
	struct usb_policy *req_policy;

	USB_BOOST_DBG("\n");

	list_for_each_entry(req_policy, &usb_policy_list, list) {
		freq_qos_update_request(&req_policy->qos_req, 0);
	}
	return 0;
}


int audio_core_hold(void)
{
	struct device_node *np = gdev->of_node;

	/*Disable MCDI to save around 100us
	 *"Power ON CPU -> CPU context restore"
	 */
	if (of_device_is_compatible(np, "mediatek,mt6983-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6895-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6855-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6789-usb_boost")) {
		USB_BOOST_NOTICE("\n");
		cpu_latency_qos_update_request(&pm_qos_req, 50);
	}

	return 0;
}

int audio_core_release(void)
{
	struct device_node *np = gdev->of_node;

	/*Enable MCDI*/
	if (of_device_is_compatible(np, "mediatek,mt6983-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6895-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6855-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6789-usb_boost")) {
		USB_BOOST_NOTICE("\n");
		cpu_latency_qos_update_request(&pm_qos_req,
			PM_QOS_DEFAULT_VALUE);
	}

	return 0;
}
