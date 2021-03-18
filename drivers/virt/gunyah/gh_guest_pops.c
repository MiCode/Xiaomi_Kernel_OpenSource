// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/notifier.h>

#include <linux/gunyah/gh_vm.h>
#include <linux/gunyah/gh_rm_drv.h>

#define GH_GUEST_POPS_POFF_BUTTON_HOLD_DELAY_MS	1000

static struct input_dev *gh_vm_poff_input;
static struct notifier_block rm_nb;

static int gh_guest_pops_handle_stop_shutdown(void)
{
	/* Emulate a KEY_POWER event to notify user-space of a shutdown */
	pr_info("Sending KEY_POWER event\n");

	input_report_key(gh_vm_poff_input, KEY_POWER, 1);
	input_sync(gh_vm_poff_input);

	msleep(GH_GUEST_POPS_POFF_BUTTON_HOLD_DELAY_MS);

	input_report_key(gh_vm_poff_input, KEY_POWER, 0);
	input_sync(gh_vm_poff_input);

	return 0;
}

static int gh_guest_pops_handle_stop_crash(void)
{
	panic("Panic requested by Primary-VM\n");
	return 0;
}

static int
gh_guest_pops_vm_shutdown(struct gh_rm_notif_vm_shutdown_payload *vm_shutdown)
{
	switch (vm_shutdown->stop_reason) {
	case GH_VM_STOP_SHUTDOWN:
		return gh_guest_pops_handle_stop_shutdown();
	case GH_VM_STOP_CRASH:
		return gh_guest_pops_handle_stop_crash();
	};

	return 0;
}

static int gh_guest_pops_rm_notifer_fn(struct notifier_block *nb,
					unsigned long cmd, void *data)
{
	switch (cmd) {
	case GH_RM_NOTIF_VM_SHUTDOWN:
		return gh_guest_pops_vm_shutdown(data);
	}

	return NOTIFY_DONE;
}

static int __init gh_guest_pops_init_poff(void)
{
	int ret;

	gh_vm_poff_input = input_allocate_device();
	if (!gh_vm_poff_input)
		return -ENOMEM;

	input_set_capability(gh_vm_poff_input, EV_KEY, KEY_POWER);

	ret = input_register_device(gh_vm_poff_input);
	if (ret)
		goto fail_register;

	rm_nb.notifier_call = gh_guest_pops_rm_notifer_fn;
	ret = gh_rm_register_notifier(&rm_nb);
	if (ret)
		goto fail_init;

	return 0;

fail_init:
	input_unregister_device(gh_vm_poff_input);
fail_register:
	input_free_device(gh_vm_poff_input);
	return ret;
}

static void gh_guest_pops_exit_poff(void)
{
	gh_rm_unregister_notifier(&rm_nb);

	input_unregister_device(gh_vm_poff_input);
	input_free_device(gh_vm_poff_input);
}

static int __init gh_guest_pops_init(void)
{
	int ret;

	ret = gh_guest_pops_init_poff();
	if (ret)
		return ret;

	ret = gh_rm_vm_set_os_status(GH_RM_OS_STATUS_BOOT);
	if (ret) {
		pr_err("Failed to set the OS status\n");
		gh_guest_pops_exit_poff();
		return ret;
	}

	return 0;
}
module_init(gh_guest_pops_init);

static void __exit gh_guest_pops_exit(void)
{
	gh_guest_pops_exit_poff();
}
module_exit(gh_guest_pops_exit);

MODULE_LICENSE("GPL v2");
