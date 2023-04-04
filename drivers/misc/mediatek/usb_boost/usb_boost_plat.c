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
#include "mtk_ppm_api.h"
#include "fbt_cpu_ctrl.h"

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

#if IS_ENABLED(CONFIG_MTK_PPM_V3)

#define CPU_KIR_USB 6
#define CPU_MAX_KIR 12
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define LOG_BUF_SIZE (128)
static struct ppm_limit_data *freq_set[CPU_MAX_KIR];
static struct ppm_limit_data *current_freq;
static unsigned long *policy_mask;
static struct cpu_ctrl_data *freq_to_set;
static int cluster_num;
static struct mutex boost_freq;
#define for_each_perfmgr_clusters(i)	\
	for (i = 0; i < cluster_num; i++)

static int arch_get_nr_clusters(void)
{
	int __arch_nr_clusters = -1;
	int max_id = 0;
	unsigned int cpu;

	/* assume socket id is monotonic increasing without gap. */
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->package_id > max_id)
			max_id = cpu_topo->package_id;
	}
	__arch_nr_clusters = max_id + 1;
	return __arch_nr_clusters;
}

static int update_userlimit_cpu_freq_usb(int kicker, int num_cluster
		, struct cpu_ctrl_data *freq_limit)
{
	struct ppm_limit_data *final_freq;
	int retval = 0;
	int i, j, len = 0, len1 = 0;
	char msg[128];
	char msg1[128];

	mutex_lock(&boost_freq);

	final_freq = kcalloc(num_cluster
			, sizeof(struct ppm_limit_data), GFP_KERNEL);
	if (!final_freq) {
		retval = -1;
		//perfmgr_trace_printk("cpu_ctrl", "!final_freq\n");
		goto ret_update;
	}
	//if (num_cluster != perfmgr_clusters) {
	//	pr_debug(
	//			"num_cluster : %d perfmgr_clusters: %d, doesn't match\n",
	//			num_cluster, perfmgr_clusters);
	//	retval = -1;
	//	//perfmgr_trace_printk("cpu_ctrl","num_cluster != perfmgr_clusters\n");
	//	goto ret_update;
	//}

	if (kicker < 0 || kicker >= CPU_MAX_KIR) {
		pr_debug("kicker:%d, error\n", kicker);
		retval = -1;
		goto ret_update;
	}

	for_each_perfmgr_clusters(i) {
		final_freq[i].min = -1;
		final_freq[i].max = -1;
	}

	len += snprintf(msg + len, sizeof(msg) - len, "[%d] ", kicker);
	if (len < 0) {
		//perfmgr_trace_printk("cpu_ctrl", "return -EIO 1\n");
		mutex_unlock(&boost_freq);
		return -EIO;
	}
	for_each_perfmgr_clusters(i) {
		freq_set[kicker][i].min = freq_limit[i].min >= -1 ?
			freq_limit[i].min : -1;
		freq_set[kicker][i].max = freq_limit[i].max >= -1 ?
			freq_limit[i].max : -1;

		len += snprintf(msg + len, sizeof(msg) - len, "(%d)(%d) ",
		freq_set[kicker][i].min, freq_set[kicker][i].max);
		if (len < 0) {
			//perfmgr_trace_printk("cpu_ctrl", "return -EIO 2\n");
			mutex_unlock(&boost_freq);
			return -EIO;
		}

		if (freq_set[kicker][i].min == -1 &&
				freq_set[kicker][i].max == -1)
			clear_bit(kicker, &policy_mask[i]);
		else
			set_bit(kicker, &policy_mask[i]);

		len1 += snprintf(msg1 + len1, sizeof(msg1) - len1,
				"[0x %lx] ", policy_mask[i]);
		if (len1 < 0) {
			//perfmgr_trace_printk("cpu_ctrl", "return -EIO 3\n");
			mutex_unlock(&boost_freq);
			return -EIO;
		}
	}

	for (i = 0; i < CPU_MAX_KIR; i++) {
		for_each_perfmgr_clusters(j) {
			final_freq[j].min
				= MAX(freq_set[i][j].min, final_freq[j].min);
// #if IS_ENABLED(CONFIG_MTK_CPU_CTRL_CFP)
			final_freq[j].max
				= final_freq[j].max != -1 &&
				freq_set[i][j].max != -1 ?
				MIN(freq_set[i][j].max, final_freq[j].max) :
				MAX(freq_set[i][j].max, final_freq[j].max);
// #else
//			final_freq[j].max
//			= MAX(freq_set[i][j].max, final_freq[j].max);
// #endif
			if (final_freq[j].min > final_freq[j].max &&
					final_freq[j].max != -1)
				final_freq[j].max = final_freq[j].min;
		}
	}

	for_each_perfmgr_clusters(i) {
		current_freq[i].min = final_freq[i].min;
		current_freq[i].max = final_freq[i].max;
		len += snprintf(msg + len, sizeof(msg) - len, "{%d}{%d} ",
				current_freq[i].min, current_freq[i].max);
		if (len < 0) {
			//perfmgr_trace_printk("cpu_ctrl", "return -EIO 4\n");
			mutex_unlock(&boost_freq);
			return -EIO;
		}
	}

	if (len >= 0 && len < LOG_BUF_SIZE) {
		len1 = LOG_BUF_SIZE - len - 1;
		if (len1 > 0)
			strncat(msg, msg1, len1);
	}
	// if (log_enable)
	pr_debug("%s", msg);

// #if IS_ENABLED(CONFIG_TRACING)
//	perfmgr_trace_printk("cpu_ctrl", msg);
// #endif


// #if IS_ENABLED(CONFIG_MTK_CPU_CTRL_CFP)
//	if (!cfp_init_ret)
//		cpu_ctrl_cfp(final_freq);
//	else
//		mt_ppm_userlimit_cpu_freq(perfmgr_clusters, final_freq);
// #else
	mt_ppm_userlimit_cpu_freq(num_cluster, final_freq);
// #endif

ret_update:
	kfree(final_freq);
	mutex_unlock(&boost_freq);
	return retval;

	return 0;
}
static int freq_hold_no_qos(struct act_arg_obj *arg)
{
		int i;

		USB_BOOST_DBG("\n");

		for (i = 0; i < cluster_num; i++) {
			freq_to_set[i].min = arg->arg1;
			freq_to_set[i].max = -1;
		}
		update_userlimit_cpu_freq_usb(CPU_KIR_USB, cluster_num, freq_to_set);
		return 0;
}

static int freq_release_no_qos(struct act_arg_obj *arg)
{
	int i;

	USB_BOOST_DBG("\n");

	for (i = 0; i < cluster_num; i++) {
		freq_to_set[i].min = -1;
		freq_to_set[i].max = -1;
	}

	update_userlimit_cpu_freq_usb(CPU_KIR_USB, cluster_num, freq_to_set);
	return 0;
}

#else
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
#endif

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
#if IS_ENABLED(CONFIG_MTK_PPM_V3)
	unsigned int i, j = 0;
#endif
	USB_BOOST_NOTICE("\n");

	/* mandatory, related resource inited*/
	usb_boost_init();

	/* mandatory, hook callback depends on platform */
#if IS_ENABLED(CONFIG_MTK_PPM_V3)
	/* init freq ppm data */

	cluster_num = arch_get_nr_clusters();
	USB_BOOST_DBG("cluster_num:%d\n", cluster_num);

	mutex_init(&boost_freq);
	current_freq = kcalloc(cluster_num, sizeof(struct ppm_limit_data),
		GFP_KERNEL);

	policy_mask = kcalloc(cluster_num, sizeof(unsigned long),
			GFP_KERNEL);

	for (i = 0; i < CPU_MAX_KIR; i++)
		freq_set[i] = kcalloc(cluster_num
			, sizeof(struct ppm_limit_data)
			, GFP_KERNEL);

	for_each_perfmgr_clusters(i) {
		current_freq[i].min = -1;
		current_freq[i].max = -1;
		policy_mask[i] = 0;
	}

	for (i = 0; i < CPU_MAX_KIR; i++) {
		for_each_perfmgr_clusters(j) {
			freq_set[i][j].min = -1;
			freq_set[i][j].max = -1;
		}
	}

	USB_BOOST_DBG("cluster_num=%d\n", cluster_num);
	freq_to_set = kcalloc(cluster_num,
		sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	if (!freq_to_set) {
		USB_BOOST_DBG("kcalloc freq_to_set fail\n");
		return -1;
	}
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_HOLD, freq_hold_no_qos);
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_RELEASE, freq_release_no_qos);
#else
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_HOLD, freq_hold);
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_RELEASE, freq_release);
#endif
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
#if IS_ENABLED(CONFIG_MTK_PPM_V3)
	unsigned int i = 0;

	kfree(freq_to_set);
	kfree(current_freq);
	kfree(policy_mask);
	for (i = 0; i < CPU_MAX_KIR; i++)
		kfree(freq_set[i]);
#endif
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
		of_device_is_compatible(np, "mediatek,mt6789-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6833-usb_boost")) {
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
		of_device_is_compatible(np, "mediatek,mt6789-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6768-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6833-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6761-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6739-usb_boost")) {
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
		of_device_is_compatible(np, "mediatek,mt6789-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6768-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6833-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6761-usb_boost") ||
		of_device_is_compatible(np, "mediatek,mt6739-usb_boost")) {
		USB_BOOST_NOTICE("\n");
		cpu_latency_qos_update_request(&pm_qos_req,
			PM_QOS_DEFAULT_VALUE);
	}

	return 0;
}
