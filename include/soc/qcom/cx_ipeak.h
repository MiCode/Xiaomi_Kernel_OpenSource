/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#ifndef __SOC_COM_CX_IPEAK_H
#define __SOC_COM_CX_IPEAK_H

struct device_node;
struct cx_ipeak_client;

#ifndef CONFIG_QCOM_CX_IPEAK

static inline struct cx_ipeak_client *cx_ipeak_register(
		struct device_node *dev_node,
		const char *client_name)
{
	return NULL;
}

static inline void cx_ipeak_unregister(struct cx_ipeak_client *client)
{
}

static inline int cx_ipeak_update(struct cx_ipeak_client *ipeak_client,
			bool vote)
{
	return 0;
}
#else

struct cx_ipeak_client *cx_ipeak_register(struct device_node *dev_node,
		const char *client_name);
void cx_ipeak_unregister(struct cx_ipeak_client *client);
int cx_ipeak_update(struct cx_ipeak_client *ipeak_client, bool vote);

#endif

#endif /*__SOC_COM_CX_IPEAK_H*/
