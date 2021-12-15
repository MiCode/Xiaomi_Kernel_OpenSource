// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <soc/snd_event.h>

struct snd_event_client {
	struct list_head node;

	struct device *dev;
	const struct snd_event_ops *ops;
	void *data;

	bool attached;
	bool state;
};

struct snd_event_client_array {
	struct device *dev;
	struct snd_event_client *clnt;
	void *data;
	int (*compare)(struct device *, void *);
};

struct snd_event_clients {
	size_t num_clients;
	struct snd_event_client_array *cl_arr;
};

struct snd_master {
	struct device *dev;
	const struct snd_event_ops *ops;
	void *data;

	bool state;
	bool fwk_state;
	bool clients_found;
	struct snd_event_clients *clients;
};

static DEFINE_MUTEX(snd_event_mutex);
static LIST_HEAD(snd_event_client_list);
static struct snd_master *master;

static struct snd_event_client *find_snd_event_client(struct device *dev)
{
	struct snd_event_client *c;

	list_for_each_entry(c, &snd_event_client_list, node)
		if ((c->dev == dev) && c->ops)
			return c;

	return NULL;
}

static int check_and_update_fwk_state(void)
{
	bool new_fwk_state = true;
	struct snd_event_client *c;
	int ret = 0;
	int i = 0;

	for (i = 0; i < master->clients->num_clients; i++) {
		c = master->clients->cl_arr[i].clnt;
		new_fwk_state &= c->state;
	}
	new_fwk_state &= master->state;

	if (master->fwk_state ^ new_fwk_state) {
		if (new_fwk_state) {
			for (i = 0; i < master->clients->num_clients; i++) {
				c = master->clients->cl_arr[i].clnt;
				if (c->ops->enable) {
					ret = c->ops->enable(c->dev, c->data);
					if (ret) {
						dev_err(c->dev,
							"%s: enable failed\n",
							__func__);
						goto dev_en_failed;
					}
				}
			}
			if (master->ops->enable) {
				ret = master->ops->enable(master->dev,
							  master->data);
				if (ret) {
					dev_err(master->dev,
						"%s: enable failed\n",
						__func__);
					goto mstr_en_failed;
				}
			}
		} else {
			if (master->ops->disable)
				master->ops->disable(master->dev,
						     master->data);
			for (i = 0; i < master->clients->num_clients; i++) {
				c = master->clients->cl_arr[i].clnt;
				if (c->ops->disable)
					c->ops->disable(c->dev, c->data);
			}
		}
		master->fwk_state = new_fwk_state;
	}
	goto exit;

mstr_en_failed:
	i = master->clients->num_clients;
dev_en_failed:
	for (; i > 0; i--) {
		c = master->clients->cl_arr[i - 1].clnt;
		if (c->ops->disable)
			c->ops->disable(c->dev, c->data);
	}
exit:
	return ret;
}

static int snd_event_find_clients(struct snd_master *master)
{
	struct snd_event_clients *clients = master->clients;
	int i = 0;
	int ret = 0;

	for (i = 0; i < clients->num_clients; i++) {
		struct snd_event_client_array *c_arr = &clients->cl_arr[i];
		struct snd_event_client *c;

		if (c_arr->dev) {
			pr_err("%s: client already present dev=%pK\n",
				 __func__, c_arr->dev);
			continue;
		}

		list_for_each_entry(c, &snd_event_client_list, node) {
			if (c->attached)
				continue;

			if (c_arr->compare(c->dev, c_arr->data)) {
				dev_dbg(master->dev,
					"%s: found client, dev=%pK\n",
					__func__, c->dev);
				c_arr->dev = c->dev;
				c_arr->clnt = c;
				c->attached = true;
				break;
			}
		}
		if (!c_arr->dev) {
			dev_dbg(master->dev,
				"%s: failed to find some client\n",
				__func__);
			ret = -ENXIO;
			break;
		}
	}

	return ret;
}

/*
 * snd_event_client_register - Register a client with the SND event FW
 *
 * @dev: Pointer to the "struct device" associated with the client
 * @snd_ev_ops: Pointer to the snd_event_ops struct for the client containing
 *              callback functions
 * @data: Pointer to any additional data that the caller wants to get back
 *        with callback functions
 *
 * Returns 0 on success or error on failure.
 */
int snd_event_client_register(struct device *dev,
			      const struct snd_event_ops *snd_ev_ops,
			      void *data)
{
	struct snd_event_client *c;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return -ENOMEM;

	c->dev = dev;
	c->ops = snd_ev_ops;
	c->data = data;

	dev_dbg(dev, "%s: adding client to SND event FW (ops %pK)\n",
		__func__, snd_ev_ops);

	mutex_lock(&snd_event_mutex);
	list_add_tail(&c->node, &snd_event_client_list);

	if (master && !master->clients_found) {
		if (snd_event_find_clients(master)) {
			dev_dbg(dev, "%s: Failed to find all clients\n",
				__func__);
			goto exit;
		}
		master->clients_found = true;
	}

exit:
	mutex_unlock(&snd_event_mutex);
	return 0;
}
EXPORT_SYMBOL(snd_event_client_register);

/*
 * snd_event_client_deregister - Remove a client from the SND event FW
 *
 * @dev: Pointer to the "struct device" associated with the client
 *
 * Returns 0 on success or error on failure.
 */
int snd_event_client_deregister(struct device *dev)
{
	struct snd_event_client *c;
	int ret = 0;
	int i = 0;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&snd_event_mutex);
	if (list_empty(&snd_event_client_list)) {
		dev_dbg(dev, "%s: No SND client registered\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	c = find_snd_event_client(dev);
	if (!c || (c->dev != dev)) {
		dev_dbg(dev, "%s: No matching snd dev found\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	c->state = false;

	if (master && master->clients_found) {
		struct snd_event_client *d;
		bool dev_found = false;

		for (i = 0; i < master->clients->num_clients; i++) {
			d = master->clients->cl_arr[i].clnt;
			if (c->dev == d->dev) {
				dev_found = true;
				break;
			}
		}
		if (dev_found) {
			ret = check_and_update_fwk_state();
			master->clients_found = false;
		}
	}

	list_del(&c->node);
	kfree(c);
exit:
	mutex_unlock(&snd_event_mutex);
	return ret;
}
EXPORT_SYMBOL(snd_event_client_deregister);

/*
 * snd_event_mstr_add_client - Add a client to the master's list of clients
 *
 * @snd_clients: list of clients associated with this master
 * @compare: Pointer to the compare callback function that master will use to
 *           confirm the clients
 * @data: Address to any additional data that the master wants to get back with
 *        compare callback functions
 */
void snd_event_mstr_add_client(struct snd_event_clients **snd_clients,
			       int (*compare)(struct device *, void *),
			       void *data)
{
	struct snd_event_clients *client = *snd_clients;

	if (IS_ERR(client)) {
		pr_err("%s: snd_clients is invalid\n", __func__);
		return;
	}

	if (!client) {
		client = kzalloc(sizeof(*client), GFP_KERNEL);
		if (!client) {
			*snd_clients = ERR_PTR(-ENOMEM);
			return;
		}
		client->cl_arr = kzalloc(sizeof(struct snd_event_client_array),
					 GFP_KERNEL);
		if (!client->cl_arr) {
			*snd_clients = ERR_PTR(-ENOMEM);
			return;
		}
		*snd_clients = client;
	} else {
		struct snd_event_client_array *new;

		new = krealloc(client->cl_arr,
			       (client->num_clients + 1) * sizeof(*new),
			       GFP_KERNEL | __GFP_ZERO);
		if (!new) {
			*snd_clients = ERR_PTR(-ENOMEM);
			return;
		}
		client->cl_arr = new;
	}

	client->cl_arr[client->num_clients].dev = NULL;
	client->cl_arr[client->num_clients].data = data;
	client->cl_arr[client->num_clients].compare = compare;
	client->num_clients++;
}
EXPORT_SYMBOL(snd_event_mstr_add_client);

/*
 * snd_event_master_register - Register a master with the SND event FW
 *
 * @dev: Pointer to the "struct device" associated with the master
 * @ops: Pointer to the snd_event_ops struct for the master containing
 *       callback functions
 * @clients: List of clients for the master
 * @data: Pointer to any additional data that the caller wants to get back
 *        with callback functions
 *
 * Returns 0 on success or error on failure.
 *
 * Prerequisite:
 *  clients list must not be empty.
 *  All clients for the master must have to be registered by calling
 *  snd_event_mstr_add_client() before calling this API to register a
 *  master with SND event fwk.
 */
int snd_event_master_register(struct device *dev,
			      const struct snd_event_ops *ops,
			      struct snd_event_clients *clients,
			      void *data)
{
	struct snd_master *new_master;
	int ret = 0;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&snd_event_mutex);
	if (master) {
		dev_err(dev, "%s: master already allocated with %pK\n",
			__func__, master->dev);
		ret = -EALREADY;
		goto exit;
	}
	mutex_unlock(&snd_event_mutex);

	if (!clients || IS_ERR(clients)) {
		dev_err(dev, "%s: Invalid clients ptr\n", __func__);
		return -EINVAL;
	}

	new_master = kzalloc(sizeof(*new_master), GFP_KERNEL);
	if (!new_master)
		return -ENOMEM;

	new_master->dev = dev;
	new_master->ops = ops;
	new_master->data = data;
	new_master->clients = clients;

	dev_dbg(dev, "adding master to SND event FW (ops %pK)\n", ops);

	mutex_lock(&snd_event_mutex);

	master = new_master;

	ret = snd_event_find_clients(master);
	if (ret) {
		dev_dbg(dev, "%s: Failed to find all clients\n", __func__);
		ret = 0;
		goto exit;
	}
	master->clients_found = true;

exit:
	mutex_unlock(&snd_event_mutex);
	return ret;
}
EXPORT_SYMBOL(snd_event_master_register);

/*
 * snd_event_master_deregister - Remove a master from the SND event FW
 *
 * @dev: Pointer to the "struct device" associated with the master
 *
 * Returns 0 on success or error on failure.
 */
int snd_event_master_deregister(struct device *dev)
{
	int ret = 0;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&snd_event_mutex);
	if (!master) {
		dev_dbg(dev, "%s: No master found\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	if (master->dev != dev) {
		dev_dbg(dev, "%s: device is not a Master\n", __func__);
		ret = -ENXIO;
		goto exit;
	}

	master->state = false;

	if (master && master->clients_found)
		ret = check_and_update_fwk_state();

	kfree(master->clients->cl_arr);
	kfree(master->clients);
	kfree(master);
	master = NULL;
exit:
	mutex_unlock(&snd_event_mutex);
	return ret;
}
EXPORT_SYMBOL(snd_event_master_deregister);

/*
 * snd_event_notify - Update the state of the Master/client in the SND event FW
 *
 * @dev: Pointer to the "struct device" associated with the master/client
 * @state: UP/DOWN state of the caller (master/client)
 *
 * Returns 0 on success or error on failure.
 */
int snd_event_notify(struct device *dev, unsigned int state)
{
	struct snd_event_client *c;
	int ret = 0;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&snd_event_mutex);
	if (list_empty(&snd_event_client_list) && !master) {
		dev_err(dev, "%s: No device registered\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	c = find_snd_event_client(dev);
	if (!c && (!master || (master->dev != dev))) {
		dev_err(dev, "%s: No snd dev entry found\n", __func__);
		ret = -ENXIO;
		goto exit;
	}

	if (c)
		c->state = !!state;
	else
		master->state = !!state;

	if (master && master->clients_found)
		ret = check_and_update_fwk_state();

exit:
	mutex_unlock(&snd_event_mutex);
	return ret;
}
EXPORT_SYMBOL(snd_event_notify);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SND event module");
