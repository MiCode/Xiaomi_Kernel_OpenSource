/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_IPC_ROUTER_SECURITY_H
#define _MSM_IPC_ROUTER_SECURITY_H

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/errno.h>

#ifdef CONFIG_MSM_IPC_ROUTER_SECURITY
#include <linux/android_aid.h>

/**
 * check_permisions() - Check whether the process has permissions to
 *                      create an interface handle with IPC Router
 *
 * @return: true if the process has permissions, else false.
 */
int check_permissions(void);

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
int msm_ipc_config_sec_rules(void *arg);

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
void *msm_ipc_get_security_rule(uint32_t service_id, uint32_t instance_id);

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
int msm_ipc_check_send_permissions(void *data);

/**
 * msm_ipc_router_security_init() - Initialize the security rule database
 *
 * @return: 0 if successful, < 0 for error.
 */
int msm_ipc_router_security_init(void);

/**
 * wait_for_irsc_completion() - Wait for IPC Router Security Configuration
 *                              (IRSC) to complete
 */
void wait_for_irsc_completion(void);

/**
 * signal_irsc_completion() - Signal the completion of IRSC
 */
void signal_irsc_completion(void);

#else

static inline int check_permissions(void)
{
	return 1;
}

static inline int msm_ipc_config_sec_rules(void *arg)
{
	return -ENODEV;
}

static inline void *msm_ipc_get_security_rule(uint32_t service_id,
					      uint32_t instance_id)
{
	return NULL;
}

static inline int msm_ipc_check_send_permissions(void *data)
{
	return 1;
}

static inline int msm_ipc_router_security_init(void)
{
	return 0;
}

static inline void wait_for_irsc_completion(void) { }

static inline void signal_irsc_completion(void) { }

#endif
#endif
