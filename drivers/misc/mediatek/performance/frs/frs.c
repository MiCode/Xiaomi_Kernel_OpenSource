// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <net/genetlink.h>
#include <linux/kobject.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/module.h>

#include "frs.h"
#include "fpsgo_common.h"
#include "fstb.h"

#define EARA_MAX_COUNT 10
#define EARA_PROC_NAME_LEN 16
#define TAG "FRS"

static int frs_nl_id = 31;
module_param(frs_nl_id, int, 0644);
struct frs_info frs_data;
struct _EARA_THRM_PACKAGE {
	__s32 type;
	__s32 request;
	__s32 is_camera;
	__s32 pair_pid[EARA_MAX_COUNT];
	__u64 pair_bufid[EARA_MAX_COUNT];
	__s32 pair_tfps[EARA_MAX_COUNT];
	__s32 pair_rfps[EARA_MAX_COUNT];
	__s32 pair_diff[EARA_MAX_COUNT];
	__s32 pair_hwui[EARA_MAX_COUNT];
	char proc_name[EARA_MAX_COUNT][EARA_PROC_NAME_LEN];
};

struct _EARA_THRM_ENABLE {
	__s32 type;
	__s32 enable;
	__s32 pid;
};

static int eara_enable;
static DEFINE_MUTEX(pre_lock);
static struct sock *frs_nl_sk;
static int eara_pid = -1;

static void set_tfps_diff(int max_cnt, int *pid, unsigned long long *buf_id, int *tfps, int *diff)
{
	int i;

	mutex_lock(&pre_lock);

	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return;
	}
	mutex_unlock(&pre_lock);

	for (i = 0; i < max_cnt; i++) {
		if (pid[i] == 0)
			break;
		pr_debug(TAG "Set %d %llu: %d\n", pid[i], buf_id[i], diff[i]);
		eara2fstb_tfps_mdiff(pid[i], buf_id[i], diff[i], tfps[i]);
	}
}

static void switch_eara(int enable, int pid)
{
	pr_debug(TAG "%s enable:%d\n", __func__, enable);
	mutex_lock(&pre_lock);
	eara_enable = enable;
	eara_pid = pid;
	mutex_unlock(&pre_lock);

}

int pre_change_single_event(int pid, unsigned long long bufID,
			int target_fps)
{
	struct _EARA_THRM_PACKAGE change_msg;
	int ret = 0;

	mutex_lock(&pre_lock);
	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return -1;
	}
	mutex_unlock(&pre_lock);

	memset(&change_msg, 0, sizeof(struct _EARA_THRM_PACKAGE));
	change_msg.request = 1;
	change_msg.pair_pid[0] = pid;
	change_msg.pair_bufid[0] = bufID;
	change_msg.pair_tfps[0] = target_fps;
	ret = eara_nl_send_to_user((void *)&change_msg, sizeof(struct _EARA_THRM_PACKAGE));

	return ret;
}

int pre_change_event(void)
{
	struct _EARA_THRM_PACKAGE change_msg;
	int ret = 0;

	pr_debug("eara_enable %d\n", eara_enable);
	mutex_lock(&pre_lock);
	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return -1;
	}
	mutex_unlock(&pre_lock);
	memset(&change_msg, 0, sizeof(struct _EARA_THRM_PACKAGE));
	eara2fstb_get_tfps(EARA_MAX_COUNT, &(change_msg.is_camera), change_msg.pair_pid,
			change_msg.pair_bufid, change_msg.pair_tfps, change_msg.pair_rfps,
			change_msg.pair_hwui, change_msg.proc_name);
	ret = eara_nl_send_to_user((void *)&change_msg, sizeof(struct _EARA_THRM_PACKAGE));

	return ret;
}

int eara_nl_send_to_user(void *buf, int size)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;

	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	if (frs_nl_sk == NULL)
		return -1;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return -1;
	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, size+1, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, buf, size);
	NETLINK_CB(skb).portid = 0; /* from kernel */
	NETLINK_CB(skb).dst_group = 0; /* unicast */

	pr_debug(TAG "Netlink_unicast size=%d\n", size);

	ret = netlink_unicast(frs_nl_sk, skb, eara_pid, MSG_DONTWAIT);
	if (ret < 0) {
		pr_debug(TAG "Send to pid %d failed %d\n", eara_pid, ret);
		return -1;
	}
	pr_debug(TAG "Netlink_unicast- ret=%d\n", ret);
	return 0;

}

static void eara_nl_data_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct _EARA_THRM_PACKAGE *change_msg;
	struct _EARA_THRM_ENABLE *enable_msg;
	//int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	/*tsta_dprintk(
	 *"[ta_nl_data_handler] recv skb from user space uid:%d pid:%d seq:%d\n"
	 * ,uid, pid, seq);
	 */
	data = NLMSG_DATA(nlh);
	change_msg = (struct _EARA_THRM_PACKAGE *) NLMSG_DATA(nlh);
	enable_msg = (struct _EARA_THRM_ENABLE *) NLMSG_DATA(nlh);
	if (change_msg->type == 0)
		set_tfps_diff(EARA_MAX_COUNT, change_msg->pair_pid,
			change_msg->pair_bufid, change_msg->pair_tfps, change_msg->pair_diff);
	else
		switch_eara(enable_msg->enable, enable_msg->pid);
}


int eara_netlink_init(void)
{
	/*add by willcai for the userspace  to kernelspace*/
	struct netlink_kernel_cfg cfg = {
		.input  = eara_nl_data_handler,
	};

	frs_nl_sk = NULL;
	frs_nl_sk = netlink_kernel_create(&init_net, frs_nl_id, &cfg);

	pr_debug(TAG "netlink_kernel_create protol= %d\n", frs_nl_id);

	if (frs_nl_sk == NULL) {
		pr_debug(TAG "netlink_kernel_create fail\n");
		return -1;
	}

	return 0;
}

static ssize_t frs_nl_id_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", frs_nl_id);

	return len;
}

static ssize_t frs_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			frs_data.enable,
			frs_data.activated, frs_data.pid,
			frs_data.target_fps, frs_data.diff,
			frs_data.tpcb, frs_data.tpcb_slope,
			frs_data.ap_headroom, frs_data.n_sec_to_ttpcb);

	return len;
}

static ssize_t frs_info_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int enable, act, target_fps, tpcb, tpcb_slope;
	int ap_headroom, n_sec_to_ttpcb;
	int pid, diff;
	int ret;

	ret = sscanf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &enable, &act, &pid, &target_fps,
				&diff, &tpcb, &tpcb_slope, &ap_headroom, &n_sec_to_ttpcb);
	if (ret == 9) {
		if ((ap_headroom >= -1000) && (ap_headroom <= 1000)) {
			frs_data.ap_headroom = ap_headroom;
		} else {
			pr_info("[%s] invalid ap head room input\n", __func__);
			return -EINVAL;
		}

		frs_data.enable = enable;
		frs_data.activated = act;
		frs_data.tpcb = tpcb;
		frs_data.pid = pid;
		frs_data.target_fps = target_fps;
		frs_data.diff = diff;
		frs_data.tpcb_slope = tpcb_slope;
		frs_data.n_sec_to_ttpcb = n_sec_to_ttpcb;
	} else {
		pr_info("[%s] invalid input\n", __func__);
		return -EINVAL;
	}

	return count;
}

static struct kobj_attribute frs_nl_id_attr = __ATTR_RO(frs_nl_id);
static struct kobj_attribute frs_info_attr = __ATTR_RW(frs_info);
static struct attribute *thermal_attrs[] = {
	&frs_nl_id_attr.attr,
	&frs_info_attr.attr,
	NULL
};
static struct attribute_group thermal_attr_group = {
	.name	= "thermal",
	.attrs	= thermal_attrs,
};

void __exit eara_thrm_pre_exit(void)
{
	eara_pre_change_fp = NULL;
	eara_pre_change_single_fp = NULL;
}

int __init eara_thrm_pre_init(void)
{
	int ret;

	eara_pre_change_fp = pre_change_event;
	eara_pre_change_single_fp =  pre_change_single_event;
	eara_netlink_init();

	ret = sysfs_create_group(kernel_kobj, &thermal_attr_group);
	if (ret) {
		pr_info(TAG, "failed to create thermal sysfs, ret=%d!\n", ret);
		return ret;
	}
	return 0;
}

module_init(eara_thrm_pre_init);
module_exit(eara_thrm_pre_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Frame Rate Smoother");
MODULE_AUTHOR("MediaTek Inc.");

