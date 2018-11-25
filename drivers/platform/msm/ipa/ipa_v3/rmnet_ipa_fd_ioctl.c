/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/rmnet_ipa_fd_ioctl.h>
#include "ipa_qmi_service.h"

#define DRIVER_NAME "wwan_ioctl"

#ifdef CONFIG_COMPAT
#define WAN_IOC_ADD_FLT_RULE32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_FLT_RULE, \
		compat_uptr_t)
#define WAN_IOC_ADD_FLT_RULE_INDEX32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_ADD_FLT_INDEX, \
		compat_uptr_t)
#define WAN_IOC_POLL_TETHERING_STATS32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_POLL_TETHERING_STATS, \
		compat_uptr_t)
#define WAN_IOC_SET_DATA_QUOTA32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_SET_DATA_QUOTA, \
		compat_uptr_t)
#define WAN_IOC_SET_TETHER_CLIENT_PIPE32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_SET_TETHER_CLIENT_PIPE, \
		compat_uptr_t)
#define WAN_IOC_QUERY_TETHER_STATS32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_QUERY_TETHER_STATS, \
		compat_uptr_t)
#define WAN_IOC_RESET_TETHER_STATS32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_RESET_TETHER_STATS, \
		compat_uptr_t)
#define WAN_IOC_QUERY_DL_FILTER_STATS32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_QUERY_DL_FILTER_STATS, \
		compat_uptr_t)
#define WAN_IOC_QUERY_TETHER_STATS_ALL32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_QUERY_TETHER_STATS_ALL, \
		compat_uptr_t)
#define WAN_IOC_NOTIFY_WAN_STATE32 _IOWR(WAN_IOC_MAGIC, \
		WAN_IOCTL_NOTIFY_WAN_STATE, \
		compat_uptr_t)
#define WAN_IOCTL_ENABLE_PER_CLIENT_STATS32 _IOWR(WAN_IOC_MAGIC, \
			WAN_IOCTL_ENABLE_PER_CLIENT_STATS, \
			compat_uptr_t)
#define WAN_IOCTL_QUERY_PER_CLIENT_STATS32 _IOWR(WAN_IOC_MAGIC, \
			WAN_IOCTL_QUERY_PER_CLIENT_STATS, \
			compat_uptr_t)
#define WAN_IOCTL_SET_LAN_CLIENT_INFO32 _IOWR(WAN_IOC_MAGIC, \
			WAN_IOCTL_SET_LAN_CLIENT_INFO, \
			compat_uptr_t)
#endif

static unsigned int dev_num = 1;
static struct cdev ipa3_wan_ioctl_cdev;
static unsigned int ipa3_process_ioctl = 1;
static struct class *class;
static dev_t device;

static long ipa3_wan_ioctl(struct file *filp,
		unsigned int cmd,
		unsigned long arg)
{
	int retval = 0, rc = 0;
	u32 pyld_sz;
	u8 *param = NULL;

	IPAWANDBG("device %s got ioctl events :>>>\n",
		DRIVER_NAME);

	if (!ipa3_process_ioctl) {

		if ((cmd == WAN_IOC_SET_LAN_CLIENT_INFO) ||
			(cmd == WAN_IOC_CLEAR_LAN_CLIENT_INFO)) {
			IPAWANDBG("Modem is in SSR\n");
			IPAWANDBG("Still allow IOCTL for exceptions (%d)\n",
				cmd);
		} else {
			IPAWANERR_RL("Modem is in SSR, ignoring ioctl (%d)\n",
				cmd);
			return -EAGAIN;
		}
	}

	switch (cmd) {
	case WAN_IOC_ADD_FLT_RULE:
		IPAWANDBG("device %s got WAN_IOC_ADD_FLT_RULE :>>>\n",
		DRIVER_NAME);
		pyld_sz = sizeof(struct ipa_install_fltr_rule_req_msg_v01);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_qmi_filter_request_send(
			(struct ipa_install_fltr_rule_req_msg_v01 *)param)) {
			IPAWANDBG("IPACM->Q6 add filter rule failed\n");
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_ADD_FLT_RULE_EX:
		IPAWANDBG("device %s got WAN_IOC_ADD_FLT_RULE_EX :>>>\n",
		DRIVER_NAME);
		pyld_sz = sizeof(struct ipa_install_fltr_rule_req_ex_msg_v01);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_qmi_filter_request_ex_send(
			(struct ipa_install_fltr_rule_req_ex_msg_v01 *)param)) {
			IPAWANDBG("IPACM->Q6 add filter rule failed\n");
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_ADD_UL_FLT_RULE:
		IPAWANDBG("device %s got WAN_IOC_UL_ADD_FLT_RULE :>>>\n",
		DRIVER_NAME);
		pyld_sz =
		sizeof(struct ipa_configure_ul_firewall_rules_req_msg_v01);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_qmi_ul_filter_request_send(
			(struct ipa_configure_ul_firewall_rules_req_msg_v01 *)
			param)) {
			IPAWANDBG("IPACM->Q6 add ul filter rule failed\n");
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_ADD_FLT_RULE_INDEX:
		IPAWANDBG("device %s got WAN_IOC_ADD_FLT_RULE_INDEX :>>>\n",
		DRIVER_NAME);
		pyld_sz = sizeof(struct ipa_fltr_installed_notif_req_msg_v01);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_qmi_filter_notify_send(
		(struct ipa_fltr_installed_notif_req_msg_v01 *)param)) {
			IPAWANDBG("IPACM->Q6 rule index fail\n");
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_VOTE_FOR_BW_MBPS:
		IPAWANDBG("device %s got WAN_IOC_VOTE_FOR_BW_MBPS :>>>\n",
		DRIVER_NAME);
		pyld_sz = sizeof(uint32_t);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_vote_for_bus_bw((uint32_t *)param)) {
			IPAWANERR("Failed to vote for bus BW\n");
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_POLL_TETHERING_STATS:
		IPAWANDBG_LOW("got WAN_IOCTL_POLL_TETHERING_STATS :>>>\n");
		pyld_sz = sizeof(struct wan_ioctl_poll_tethering_stats);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (rmnet_ipa3_poll_tethering_stats(
		(struct wan_ioctl_poll_tethering_stats *)param)) {
			IPAWANERR_RL("WAN_IOCTL_POLL_TETHERING_STATS failed\n");
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_SET_DATA_QUOTA:
		IPAWANDBG_LOW("got WAN_IOCTL_SET_DATA_QUOTA :>>>\n");
		pyld_sz = sizeof(struct wan_ioctl_set_data_quota);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		rc = rmnet_ipa3_set_data_quota(
			(struct wan_ioctl_set_data_quota *)param);
		if (rc != 0) {
			IPAWANERR("WAN_IOC_SET_DATA_QUOTA failed\n");
			if (rc == -ENODEV)
				retval = -ENODEV;
			else
				retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_SET_TETHER_CLIENT_PIPE:
		IPAWANDBG_LOW("got WAN_IOC_SET_TETHER_CLIENT_PIPE :>>>\n");
		pyld_sz = sizeof(struct wan_ioctl_set_tether_client_pipe);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (rmnet_ipa3_set_tether_client_pipe(
			(struct wan_ioctl_set_tether_client_pipe *)param)) {
			IPAWANERR("WAN_IOC_SET_TETHER_CLIENT_PIPE failed\n");
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_QUERY_TETHER_STATS:
		IPAWANDBG_LOW("got WAN_IOC_QUERY_TETHER_STATS :>>>\n");
		pyld_sz = sizeof(struct wan_ioctl_query_tether_stats);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}

		if (rmnet_ipa3_query_tethering_stats(
			(struct wan_ioctl_query_tether_stats *)param, false)) {
			IPAWANERR("WAN_IOC_QUERY_TETHER_STATS failed\n");
			retval = -EFAULT;
			break;
		}

		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_QUERY_TETHER_STATS_ALL:
		IPAWANDBG_LOW("got WAN_IOC_QUERY_TETHER_STATS_ALL :>>>\n");
		pyld_sz = sizeof(struct wan_ioctl_query_tether_stats_all);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}

		if (rmnet_ipa3_query_tethering_stats_all(
			(struct wan_ioctl_query_tether_stats_all *)param)) {
			IPAWANERR("WAN_IOC_QUERY_TETHER_STATS failed\n");
			retval = -EFAULT;
			break;
		}

		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_RESET_TETHER_STATS:
		IPAWANDBG_LOW("device %s got WAN_IOC_RESET_TETHER_STATS :>>>\n",
				DRIVER_NAME);
		pyld_sz = sizeof(struct wan_ioctl_reset_tether_stats);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}

		if (rmnet_ipa3_reset_tethering_stats(
				(struct wan_ioctl_reset_tether_stats *)param)) {
			IPAWANERR("WAN_IOC_RESET_TETHER_STATS failed\n");
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_NOTIFY_WAN_STATE:
		IPAWANDBG_LOW("device %s got WAN_IOC_NOTIFY_WAN_STATE :>>>\n",
			DRIVER_NAME);
		pyld_sz = sizeof(struct wan_ioctl_notify_wan_state);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}

		if (ipa3_wwan_set_modem_state(
			(struct wan_ioctl_notify_wan_state *)param)) {
			IPAWANERR("WAN_IOC_NOTIFY_WAN_STATE failed\n");
			retval = -EFAULT;
			break;
		}

		break;
	case WAN_IOC_ENABLE_PER_CLIENT_STATS:
		IPAWANDBG_LOW("got WAN_IOC_ENABLE_PER_CLIENT_STATS :>>>\n");
		pyld_sz = sizeof(bool);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (rmnet_ipa3_enable_per_client_stats(
			(bool *)param)) {
			IPAWANERR("WAN_IOC_ENABLE_PER_CLIENT_STATS failed\n");
			retval = -EFAULT;
			break;
		}
		break;
	case WAN_IOC_QUERY_PER_CLIENT_STATS:
		IPAWANDBG_LOW("got WAN_IOC_QUERY_PER_CLIENT_STATS :>>>\n");
		pyld_sz = sizeof(struct wan_ioctl_query_per_client_stats);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}

		retval = rmnet_ipa3_query_per_client_stats(
			(struct wan_ioctl_query_per_client_stats *)param);
		if (retval) {
			IPAWANERR("WAN_IOC_QUERY_PER_CLIENT_STATS failed\n");
			break;
		}

		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_SET_LAN_CLIENT_INFO:
		IPAWANDBG_LOW("got WAN_IOC_SET_LAN_CLIENT_INFO :>>>\n");
		pyld_sz = sizeof(struct wan_ioctl_lan_client_info);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (rmnet_ipa3_set_lan_client_info(
			(struct wan_ioctl_lan_client_info *)param)) {
			IPAWANERR("WAN_IOC_SET_LAN_CLIENT_INFO failed\n");
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_CLEAR_LAN_CLIENT_INFO:
		IPAWANDBG_LOW("got WAN_IOC_CLEAR_LAN_CLIENT_INFO :>>>\n");
		pyld_sz = sizeof(struct wan_ioctl_lan_client_info);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (rmnet_ipa3_clear_lan_client_info(
			(struct wan_ioctl_lan_client_info *)param)) {
			IPAWANERR("WAN_IOC_CLEAR_LAN_CLIENT_INFO failed\n");
			retval = -EFAULT;
			break;
		}
		break;


	case WAN_IOC_SEND_LAN_CLIENT_MSG:
		IPAWANDBG_LOW("got WAN_IOC_SEND_LAN_CLIENT_MSG :>>>\n");
		pyld_sz = sizeof(struct wan_ioctl_send_lan_client_msg);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (rmnet_ipa3_send_lan_client_msg(
			(struct wan_ioctl_send_lan_client_msg *)
			param)) {
			IPAWANERR("IOC_SEND_LAN_CLIENT_MSG failed\n");
			retval = -EFAULT;
			break;
		}
		break;

	default:
		retval = -ENOTTY;
	}
	kfree(param);
	return retval;
}

#ifdef CONFIG_COMPAT
long ipa3_compat_wan_ioctl(struct file *file,
		unsigned int cmd,
		unsigned long arg)
{
	switch (cmd) {
	case WAN_IOC_ADD_FLT_RULE32:
		cmd = WAN_IOC_ADD_FLT_RULE;
		break;
	case WAN_IOC_ADD_FLT_RULE_INDEX32:
		cmd = WAN_IOC_ADD_FLT_RULE_INDEX;
		break;
	case WAN_IOC_POLL_TETHERING_STATS32:
		cmd = WAN_IOC_POLL_TETHERING_STATS;
		break;
	case WAN_IOC_SET_DATA_QUOTA32:
		cmd = WAN_IOC_SET_DATA_QUOTA;
		break;
	case WAN_IOC_SET_TETHER_CLIENT_PIPE32:
		cmd = WAN_IOC_SET_TETHER_CLIENT_PIPE;
		break;
	case WAN_IOC_QUERY_TETHER_STATS32:
		cmd = WAN_IOC_QUERY_TETHER_STATS;
		break;
	case WAN_IOC_RESET_TETHER_STATS32:
		cmd = WAN_IOC_RESET_TETHER_STATS;
		break;
	case WAN_IOC_QUERY_DL_FILTER_STATS32:
		cmd = WAN_IOC_QUERY_DL_FILTER_STATS;
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return ipa3_wan_ioctl(file, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static int ipa3_wan_ioctl_open(struct inode *inode, struct file *filp)
{
	IPAWANDBG("\n IPA A7 ipa3_wan_ioctl open OK :>>>> ");
	return 0;
}

const struct file_operations rmnet_ipa3_fops = {
	.owner = THIS_MODULE,
	.open = ipa3_wan_ioctl_open,
	.read = NULL,
	.unlocked_ioctl = ipa3_wan_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ipa3_compat_wan_ioctl,
#endif
};

int ipa3_wan_ioctl_init(void)
{
	unsigned int wan_ioctl_major = 0;
	int ret;
	struct device *dev;

	device = MKDEV(wan_ioctl_major, 0);

	ret = alloc_chrdev_region(&device, 0, dev_num, DRIVER_NAME);
	if (ret) {
		IPAWANERR(":device_alloc err.\n");
		goto dev_alloc_err;
	}
	wan_ioctl_major = MAJOR(device);

	class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(class)) {
		IPAWANERR(":class_create err.\n");
		goto class_err;
	}

	dev = device_create(class, NULL, device,
		NULL, DRIVER_NAME);
	if (IS_ERR(dev)) {
		IPAWANERR(":device_create err.\n");
		goto device_err;
	}

	cdev_init(&ipa3_wan_ioctl_cdev, &rmnet_ipa3_fops);
	ret = cdev_add(&ipa3_wan_ioctl_cdev, device, dev_num);
	if (ret) {
		IPAWANERR(":cdev_add err.\n");
		goto cdev_add_err;
	}

	ipa3_process_ioctl = 1;

	IPAWANDBG("IPA %s major(%d) initial ok :>>>>\n",
	DRIVER_NAME, wan_ioctl_major);
	return 0;

cdev_add_err:
	device_destroy(class, device);
device_err:
	class_destroy(class);
class_err:
	unregister_chrdev_region(device, dev_num);
dev_alloc_err:
	return -ENODEV;
}

void ipa3_wan_ioctl_stop_qmi_messages(void)
{
	ipa3_process_ioctl = 0;
}

void ipa3_wan_ioctl_enable_qmi_messages(void)
{
	ipa3_process_ioctl = 1;
}

void ipa3_wan_ioctl_deinit(void)
{
	cdev_del(&ipa3_wan_ioctl_cdev);
	device_destroy(class, device);
	class_destroy(class);
	unregister_chrdev_region(device, dev_num);
}
