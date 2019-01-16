/*
 * kernel/power/tuxonice_storage.c
 *
 * Copyright (C) 2005-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * Routines for talking to a userspace program that manages storage.
 *
 * The kernel side:
 * - starts the userspace program;
 * - sends messages telling it when to open and close the connection;
 * - tells it when to quit;
 *
 * The user space side:
 * - passes messages regarding status;
 *
 */

#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/freezer.h>

#include "tuxonice_sysfs.h"
#include "tuxonice_modules.h"
#include "tuxonice_netlink.h"
#include "tuxonice_storage.h"
#include "tuxonice_ui.h"

static struct user_helper_data usm_helper_data;
static struct toi_module_ops usm_ops;
static int message_received, usm_prepare_count;
static int storage_manager_last_action, storage_manager_action;

static int usm_user_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh)
{
	int type;
	int *data;

	type = nlh->nlmsg_type;

	/* A control message: ignore them */
	if (type < NETLINK_MSG_BASE)
		return 0;

	/* Unknown message: reply with EINVAL */
	if (type >= USM_MSG_MAX)
		return -EINVAL;

	/* All operations require privileges, even GET */
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* Only allow one task to receive NOFREEZE privileges */
	if (type == NETLINK_MSG_NOFREEZE_ME && usm_helper_data.pid != -1)
		return -EBUSY;

	data = (int *)NLMSG_DATA(nlh);

	switch (type) {
	case USM_MSG_SUCCESS:
	case USM_MSG_FAILED:
		message_received = type;
		complete(&usm_helper_data.wait_for_process);
		break;
	default:
		printk(KERN_INFO "Storage manager doesn't recognise " "message %d.\n", type);
	}

	return 1;
}

#ifdef CONFIG_NET
static int activations;

int toi_activate_storage(int force)
{
	int tries = 1;

	if (usm_helper_data.pid == -1 || !usm_ops.enabled)
		return 0;

	message_received = 0;
	activations++;

	if (activations > 1 && !force)
		return 0;

	while ((!message_received || message_received == USM_MSG_FAILED) && tries < 2) {
		toi_prepare_status(DONT_CLEAR_BAR, "Activate storage attempt " "%d.\n", tries);

		init_completion(&usm_helper_data.wait_for_process);

		toi_send_netlink_message(&usm_helper_data, USM_MSG_CONNECT, NULL, 0);

		/* Wait 2 seconds for the userspace process to make contact */
		wait_for_completion_timeout(&usm_helper_data.wait_for_process, 2 * HZ);

		tries++;
	}

	return 0;
}

int toi_deactivate_storage(int force)
{
	if (usm_helper_data.pid == -1 || !usm_ops.enabled)
		return 0;

	message_received = 0;
	activations--;

	if (activations && !force)
		return 0;

	init_completion(&usm_helper_data.wait_for_process);

	toi_send_netlink_message(&usm_helper_data, USM_MSG_DISCONNECT, NULL, 0);

	wait_for_completion_timeout(&usm_helper_data.wait_for_process, 2 * HZ);

	if (!message_received || message_received == USM_MSG_FAILED) {
		printk(KERN_INFO "Returning failure disconnecting storage.\n");
		return 1;
	}

	return 0;
}
#endif

static void storage_manager_simulate(void)
{
	printk(KERN_INFO "--- Storage manager simulate ---\n");
	toi_prepare_usm();
	schedule();
	printk(KERN_INFO "--- Activate storage 1 ---\n");
	toi_activate_storage(1);
	schedule();
	printk(KERN_INFO "--- Deactivate storage 1 ---\n");
	toi_deactivate_storage(1);
	schedule();
	printk(KERN_INFO "--- Cleanup usm ---\n");
	toi_cleanup_usm();
	schedule();
	printk(KERN_INFO "--- Storage manager simulate ends ---\n");
}

static int usm_storage_needed(void)
{
	return sizeof(int) + strlen(usm_helper_data.program) + 1;
}

static int usm_save_config_info(char *buf)
{
	int len = strlen(usm_helper_data.program);
	memcpy(buf, usm_helper_data.program, len + 1);
	return sizeof(int) + len + 1;
}

static void usm_load_config_info(char *buf, int size)
{
	/* Don't load the saved path if one has already been set */
	if (usm_helper_data.program[0])
		return;

	memcpy(usm_helper_data.program, buf + sizeof(int), *((int *)buf));
}

static int usm_memory_needed(void)
{
	/* ball park figure of 32 pages */
	return 32 * PAGE_SIZE;
}

/* toi_prepare_usm
 */
int toi_prepare_usm(void)
{
	usm_prepare_count++;

	if (usm_prepare_count > 1 || !usm_ops.enabled)
		return 0;

	usm_helper_data.pid = -1;

	if (!*usm_helper_data.program)
		return 0;

	toi_netlink_setup(&usm_helper_data);

	if (usm_helper_data.pid == -1)
		printk(KERN_INFO "TuxOnIce Storage Manager wanted, but couldn't" " start it.\n");

	toi_activate_storage(0);

	return usm_helper_data.pid != -1;
}

void toi_cleanup_usm(void)
{
	usm_prepare_count--;

	if (usm_helper_data.pid > -1 && !usm_prepare_count) {
		toi_deactivate_storage(0);
		toi_netlink_close(&usm_helper_data);
	}
}

static void storage_manager_activate(void)
{
	if (storage_manager_action == storage_manager_last_action)
		return;

	if (storage_manager_action)
		toi_prepare_usm();
	else
		toi_cleanup_usm();

	storage_manager_last_action = storage_manager_action;
}

/*
 * User interface specific /sys/power/tuxonice entries.
 */

static struct toi_sysfs_data sysfs_params[] = {
	SYSFS_NONE("simulate_atomic_copy", storage_manager_simulate),
	SYSFS_INT("enabled", SYSFS_RW, &usm_ops.enabled, 0, 1, 0, NULL),
	SYSFS_STRING("program", SYSFS_RW, usm_helper_data.program, 254, 0,
		     NULL),
	SYSFS_INT("activate_storage", SYSFS_RW, &storage_manager_action, 0, 1,
		  0, storage_manager_activate)
};

static struct toi_module_ops usm_ops = {
	.type = MISC_MODULE,
	.name = "usm",
	.directory = "storage_manager",
	.module = THIS_MODULE,
	.storage_needed = usm_storage_needed,
	.save_config_info = usm_save_config_info,
	.load_config_info = usm_load_config_info,
	.memory_needed = usm_memory_needed,

	.sysfs_data = sysfs_params,
	.num_sysfs_entries = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data),
};

/* toi_usm_sysfs_init
 * Description: Boot time initialisation for user interface.
 */
int toi_usm_init(void)
{
	usm_helper_data.nl = NULL;
	usm_helper_data.program[0] = '\0';
	usm_helper_data.pid = -1;
	usm_helper_data.skb_size = 0;
	usm_helper_data.pool_limit = 6;
	usm_helper_data.netlink_id = NETLINK_TOI_USM;
	usm_helper_data.name = "userspace storage manager";
	usm_helper_data.rcv_msg = usm_user_rcv_msg;
	usm_helper_data.interface_version = 2;
	usm_helper_data.must_init = 0;
	init_completion(&usm_helper_data.wait_for_process);

	return toi_register_module(&usm_ops);
}

void toi_usm_exit(void)
{
	toi_netlink_close_complete(&usm_helper_data);
	toi_unregister_module(&usm_ops);
}
