/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <soc/qcom/rmnet_ctl.h>
#include "rmnet_ctl_client.h"

struct rmnet_ctl_client {
	struct rmnet_ctl_client_hooks hooks;
};

struct rmnet_ctl_endpoint {
	struct rmnet_ctl_dev __rcu *dev;
	struct rmnet_ctl_client __rcu *client;
};

static DEFINE_SPINLOCK(client_lock);
static struct rmnet_ctl_endpoint ctl_ep;

void rmnet_ctl_endpoint_setdev(const struct rmnet_ctl_dev *dev)
{
	rcu_assign_pointer(ctl_ep.dev, dev);
}

void rmnet_ctl_endpoint_post(const void *data, size_t len)
{
	struct rmnet_ctl_client *client;
	struct sk_buff *skb;

	if (unlikely(!data || !len))
		return;

	rcu_read_lock();

	client = rcu_dereference(ctl_ep.client);

	if (client && client->hooks.ctl_dl_client_hook) {
		skb = alloc_skb(len, GFP_ATOMIC);
		if (skb) {
			skb_put_data(skb, data, len);
			skb->protocol = htons(ETH_P_MAP);
			client->hooks.ctl_dl_client_hook(skb);
		}
	}

	rcu_read_unlock();
}

void *rmnet_ctl_register_client(struct rmnet_ctl_client_hooks *hook)
{
	struct rmnet_ctl_client *client;

	if (!hook)
		return NULL;

	client = (struct rmnet_ctl_client *)
			kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return NULL;
	client->hooks = *hook;

	spin_lock(&client_lock);

	/* Only support one client for now */
	if (rcu_dereference(ctl_ep.client)) {
		spin_unlock(&client_lock);
		kfree(client);
		return NULL;
	}

	rcu_assign_pointer(ctl_ep.client, client);

	spin_unlock(&client_lock);

	return client;
}
EXPORT_SYMBOL(rmnet_ctl_register_client);

int rmnet_ctl_unregister_client(void *handle)
{
	struct rmnet_ctl_client *client = (struct rmnet_ctl_client *)handle;

	spin_lock(&client_lock);

	if (rcu_dereference(ctl_ep.client) != client) {
		spin_unlock(&client_lock);
		return -EINVAL;
	}

	RCU_INIT_POINTER(ctl_ep.client, NULL);

	spin_unlock(&client_lock);

	synchronize_rcu();
	kfree(client);

	return 0;
}
EXPORT_SYMBOL(rmnet_ctl_unregister_client);

int rmnet_ctl_send_client(void *handle, struct sk_buff *skb)
{
	struct rmnet_ctl_client *client = (struct rmnet_ctl_client *)handle;
	struct rmnet_ctl_dev *dev;
	int rc = -EINVAL;

	if (client != rcu_dereference(ctl_ep.client))
		return rc;

	rcu_read_lock();

	dev = rcu_dereference(ctl_ep.dev);
	if (dev && dev->xmit)
		rc = dev->xmit(dev, skb);

	rcu_read_unlock();

	return rc;
}
EXPORT_SYMBOL(rmnet_ctl_send_client);
