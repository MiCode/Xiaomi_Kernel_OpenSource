/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/gfp.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/msm_ipc.h>
#include <linux/rwsem.h>
#include <linux/uaccess.h>

#include <net/sock.h>
#include "ipc_router_private.h"
#include "ipc_router_security.h"

#define IRSC_COMPLETION_TIMEOUT_MS 30000
#define SEC_RULES_HASH_SZ 32

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

struct security_rule {
	struct list_head list;
	uint32_t service_id;
	uint32_t instance_id;
	unsigned reserved;
	int num_group_info;
	kgid_t *group_id;
};

static DECLARE_RWSEM(security_rules_lock_lha4);
static struct list_head security_rules[SEC_RULES_HASH_SZ];
static DECLARE_COMPLETION(irsc_completion);

/**
 * wait_for_irsc_completion() - Wait for IPC Router Security Configuration
 *                              (IRSC) to complete
 */
void wait_for_irsc_completion(void)
{
	unsigned long rem_jiffies;
	do {
		rem_jiffies = wait_for_completion_timeout(&irsc_completion,
				msecs_to_jiffies(IRSC_COMPLETION_TIMEOUT_MS));
		if (rem_jiffies)
			return;
		pr_err("%s: waiting for IPC Security Conf.\n", __func__);
	} while (1);
}

/**
 * signal_irsc_completion() - Signal the completion of IRSC
 */
void signal_irsc_completion(void)
{
	complete_all(&irsc_completion);
}

/**
 * check_permisions() - Check whether the process has permissions to
 *                      create an interface handle with IPC Router
 *
 * @return: true if the process has permissions, else false.
 */
int check_permissions(void)
{
	int rc = 0;
	if (uid_eq(current_euid(), GLOBAL_ROOT_UID) ||
	    capable(CAP_NET_RAW) || capable(CAP_NET_BIND_SERVICE))
		rc = 1;
	return rc;
}
EXPORT_SYMBOL(check_permissions);

/**
 * msm_ipc_config_sec_rules() - Add a security rule to the database
 * @arg: Pointer to the buffer containing the rule.
 *
 * @return: 0 if successfully added, < 0 for error.
 *
 * A security rule is defined using <Service_ID: Group_ID> tuple. The rule
 * implies that a user-space process in order to send a QMI message to
 * service Service_ID should belong to the Linux group Group_ID.
 */
int msm_ipc_config_sec_rules(void *arg)
{
	struct config_sec_rules_args sec_rules_arg;
	struct security_rule *rule, *temp_rule;
	int key;
	size_t kgroup_info_sz;
	int ret;
	size_t group_info_sz;
	gid_t *group_id = NULL;
	int loop;

	if (!uid_eq(current_euid(), GLOBAL_ROOT_UID))
		return -EPERM;

	ret = copy_from_user(&sec_rules_arg, (void *)arg,
			     sizeof(sec_rules_arg));
	if (ret)
		return -EFAULT;

	if (sec_rules_arg.num_group_info <= 0)
		return -EINVAL;

	if (sec_rules_arg.num_group_info > (SIZE_MAX / sizeof(gid_t))) {
		pr_err("%s: Integer Overflow %zu * %d\n", __func__,
			sizeof(gid_t), sec_rules_arg.num_group_info);
		return -EINVAL;
	}
	group_info_sz = sec_rules_arg.num_group_info * sizeof(gid_t);

	if (sec_rules_arg.num_group_info > (SIZE_MAX / sizeof(kgid_t))) {
		pr_err("%s: Integer Overflow %zu * %d\n", __func__,
			sizeof(kgid_t), sec_rules_arg.num_group_info);
		return -EINVAL;
	}
	kgroup_info_sz = sec_rules_arg.num_group_info * sizeof(kgid_t);

	rule = kzalloc(sizeof(struct security_rule), GFP_KERNEL);
	if (!rule) {
		pr_err("%s: security_rule alloc failed\n", __func__);
		return -ENOMEM;
	}

	rule->group_id = kzalloc(kgroup_info_sz, GFP_KERNEL);
	if (!rule->group_id) {
		pr_err("%s: kgroup_id alloc failed\n", __func__);
		kfree(rule);
		return -ENOMEM;
	}

	group_id = kzalloc(group_info_sz, GFP_KERNEL);
	if (!group_id) {
		pr_err("%s: group_id alloc failed\n", __func__);
		kfree(rule->group_id);
		kfree(rule);
		return -ENOMEM;
	}

	rule->service_id = sec_rules_arg.service_id;
	rule->instance_id = sec_rules_arg.instance_id;
	rule->reserved = sec_rules_arg.reserved;
	rule->num_group_info = sec_rules_arg.num_group_info;
	ret = copy_from_user(group_id, ((void *)(arg + sizeof(sec_rules_arg))),
			     group_info_sz);
	if (ret) {
		kfree(group_id);
		kfree(rule->group_id);
		kfree(rule);
		return -EFAULT;
	}
	for (loop = 0; loop < rule->num_group_info; loop++)
		rule->group_id[loop] = KGIDT_INIT(group_id[loop]);
	kfree(group_id);

	key = rule->service_id & (SEC_RULES_HASH_SZ - 1);
	down_write(&security_rules_lock_lha4);
	if (rule->service_id == ALL_SERVICE) {
		temp_rule = list_first_entry(&security_rules[key],
					     struct security_rule, list);
		list_del(&temp_rule->list);
		kfree(temp_rule->group_id);
		kfree(temp_rule);
	}
	list_add_tail(&rule->list, &security_rules[key]);
	up_write(&security_rules_lock_lha4);

	if (rule->service_id == ALL_SERVICE)
		msm_ipc_sync_default_sec_rule((void *)rule);
	else
		msm_ipc_sync_sec_rule(rule->service_id, rule->instance_id,
				      (void *)rule);

	return 0;
}
EXPORT_SYMBOL(msm_ipc_config_sec_rules);

/**
 * msm_ipc_add_default_rule() - Add default security rule
 *
 * @return: 0 on success, < 0 on error/
 *
 * This function is used to ensure the basic security, if there is no
 * security rule defined for a service. It can be overwritten by the
 * default security rule from user-space script.
 */
static int msm_ipc_add_default_rule(void)
{
	struct security_rule *rule;
	int key;

	rule = kzalloc(sizeof(struct security_rule), GFP_KERNEL);
	if (!rule) {
		pr_err("%s: security_rule alloc failed\n", __func__);
		return -ENOMEM;
	}

	rule->group_id = kzalloc(sizeof(*(rule->group_id)), GFP_KERNEL);
	if (!rule->group_id) {
		pr_err("%s: group_id alloc failed\n", __func__);
		kfree(rule);
		return -ENOMEM;
	}

	rule->service_id = ALL_SERVICE;
	rule->instance_id = ALL_INSTANCE;
	rule->num_group_info = 1;
	*(rule->group_id) = KGIDT_INIT(AID_NET_RAW);
	down_write(&security_rules_lock_lha4);
	key = (ALL_SERVICE & (SEC_RULES_HASH_SZ - 1));
	list_add_tail(&rule->list, &security_rules[key]);
	up_write(&security_rules_lock_lha4);
	return 0;
}

/**
 * msm_ipc_get_security_rule() - Get the security rule corresponding to a
 *                               service
 * @service_id: Service ID for which the rule has to be got.
 * @instance_id: Instance ID for which the rule has to be got.
 *
 * @return: Returns the rule info on success, NULL on error.
 *
 * This function is used when the service comes up and gets registered with
 * the IPC Router.
 */
void *msm_ipc_get_security_rule(uint32_t service_id, uint32_t instance_id)
{
	int key;
	struct security_rule *rule;

	key = (service_id & (SEC_RULES_HASH_SZ - 1));
	down_read(&security_rules_lock_lha4);
	/* Return the rule for a specific <service:instance>, if found. */
	list_for_each_entry(rule, &security_rules[key], list) {
		if ((rule->service_id == service_id) &&
		    (rule->instance_id == instance_id)) {
			up_read(&security_rules_lock_lha4);
			return (void *)rule;
		}
	}

	/* Return the rule for a specific service, if found. */
	list_for_each_entry(rule, &security_rules[key], list) {
		if ((rule->service_id == service_id) &&
		    (rule->instance_id == ALL_INSTANCE)) {
			up_read(&security_rules_lock_lha4);
			return (void *)rule;
		}
	}

	/* Return the default rule, if no rule defined for a service. */
	key = (ALL_SERVICE & (SEC_RULES_HASH_SZ - 1));
	list_for_each_entry(rule, &security_rules[key], list) {
		if ((rule->service_id == ALL_SERVICE) &&
		    (rule->instance_id == ALL_INSTANCE)) {
			up_read(&security_rules_lock_lha4);
			return (void *)rule;
		}
	}
	up_read(&security_rules_lock_lha4);
	return NULL;
}
EXPORT_SYMBOL(msm_ipc_get_security_rule);

/**
 * msm_ipc_check_send_permissions() - Check if the sendng process has
 *                                    permissions specified as per the rule
 * @data: Security rule to be checked.
 *
 * @return: true if the process has permissions, else false.
 *
 * This function is used to check if the current executing process has
 * permissions to send message to the remote entity. The security rule
 * corresponding to the remote entity is specified by "data" parameter
 */
int msm_ipc_check_send_permissions(void *data)
{
	int i;
	struct security_rule *rule = (struct security_rule *)data;

	/* Source/Sender is Root user */
	if (uid_eq(current_euid(), GLOBAL_ROOT_UID))
		return 1;

	/* Destination has no rules defined, possibly a client. */
	if (!rule)
		return 1;

	for (i = 0; i < rule->num_group_info; i++) {
		if (in_egroup_p(rule->group_id[i]))
			return 1;
	}
	return 0;
}
EXPORT_SYMBOL(msm_ipc_check_send_permissions);

/**
 * msm_ipc_router_security_init() - Initialize the security rule database
 *
 * @return: 0 if successful, < 0 for error.
 */
int msm_ipc_router_security_init(void)
{
	int i;

	for (i = 0; i < SEC_RULES_HASH_SZ; i++)
		INIT_LIST_HEAD(&security_rules[i]);

	msm_ipc_add_default_rule();
	return 0;
}
EXPORT_SYMBOL(msm_ipc_router_security_init);
