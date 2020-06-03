/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
 */

#ifndef __SOC_COM_CX_IPEAK_H
#define __SOC_COM_CX_IPEAK_H

typedef void (*cx_ipeak_victim_fn)(void *data, u32 freq_limit);

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

static inline int cx_ipeak_victim_register(struct cx_ipeak_client *client,
		cx_ipeak_victim_fn victim_cb, void *data)
{
	return 0;
}

static inline void cx_ipeak_victim_unregister(struct cx_ipeak_client *client)
{
}
#else

struct cx_ipeak_client *cx_ipeak_register(struct device_node *dev_node,
		const char *client_name);
void cx_ipeak_unregister(struct cx_ipeak_client *client);
int cx_ipeak_update(struct cx_ipeak_client *ipeak_client, bool vote);
int cx_ipeak_victim_register(struct cx_ipeak_client *client,
		cx_ipeak_victim_fn victim_cb, void *data);
void cx_ipeak_victim_unregister(struct cx_ipeak_client *client);

#endif

#endif /*__SOC_COM_CX_IPEAK_H*/
