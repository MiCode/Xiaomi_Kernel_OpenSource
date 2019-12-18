// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"PMIC_GLINK: %s: " fmt, __func__

#include <linux/device.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/soc/qcom/pmic_glink.h>

/**
 * struct pmic_glink_dev - Top level data structure for pmic_glink device
 * @rpdev:		rpmsg device from rpmsg framework
 * @dev:		pmic_glink parent device for all child devices
 * @channel_name:	Glink channel name used by rpmsg device
 * @client_idr:		idr list for the clients
 * @client_lock:	mutex lock when idr APIs are used on client_idr
 * @rx_lock:		spinlock to be used when rx_list is modified
 * @rx_work:		worker for handling rx messages
 * @init_work:		worker to instantiate child devices under pdev
 * @rx_wq:		workqueue for rx messages
 * @rx_list:		list for rx messages
 * @dev_list:		list for pmic_glink_dev_list
 * @state:		indicates when remote subsystem is up/down
 * @child_probed:	indicates when the children are probed
 */
struct pmic_glink_dev {
	struct rpmsg_device	*rpdev;
	struct device		*dev;
	const char		*channel_name;
	struct idr		client_idr;
	struct mutex		client_lock;
	spinlock_t		rx_lock;
	struct work_struct	rx_work;
	struct work_struct	init_work;
	struct workqueue_struct	*rx_wq;
	struct list_head	rx_list;
	struct list_head	dev_list;
	atomic_t		state;
	bool			child_probed;
};

/**
 * struct pmic_glink_client - pmic_glink client device
 * @pgdev:	pmic_glink device for the client device
 * @name:	Client name
 * @id:		Unique id for client for communication
 * @lock:	lock for sending data
 * @priv:	private data for client
 * @callback:	callback function for client
 */
struct pmic_glink_client {
	struct pmic_glink_dev	*pgdev;
	const char		*name;
	u32			id;
	struct mutex		lock;
	void			*priv;
	int			(*callback)(void *priv, void *data, size_t len);
};

struct pmic_glink_buf {
	struct list_head	node;
	size_t			len;
	u8			buf[];
};

static LIST_HEAD(pmic_glink_dev_list);
static DEFINE_MUTEX(pmic_glink_dev_lock);

static struct pmic_glink_dev *get_pmic_glink_from_dev(struct device *dev)
{
	struct pmic_glink_dev *tmp, *pos;

	mutex_lock(&pmic_glink_dev_lock);
	list_for_each_entry_safe(pos, tmp, &pmic_glink_dev_list, dev_list) {
		if (pos->dev == dev) {
			mutex_unlock(&pmic_glink_dev_lock);
			return pos;
		}
	}
	mutex_unlock(&pmic_glink_dev_lock);

	return NULL;
}

static struct pmic_glink_dev *get_pmic_glink_from_rpdev(
						struct rpmsg_device *rpdev)
{
	struct pmic_glink_dev *tmp, *pos;

	mutex_lock(&pmic_glink_dev_lock);
	list_for_each_entry_safe(pos, tmp, &pmic_glink_dev_list, dev_list) {
		if (!strcmp(rpdev->id.name, pos->channel_name)) {
			mutex_unlock(&pmic_glink_dev_lock);
			return pos;
		}
	}
	mutex_unlock(&pmic_glink_dev_lock);

	return NULL;
}

/**
 * pmic_glink_write() - Send data from client to remote subsystem
 *
 * @client: Client device pointer that is registered already
 * @data: Pointer to data that needs to be sent
 * @len: Length of data
 *
 * Return: 0 if success, negative on error.
 */
int pmic_glink_write(struct pmic_glink_client *client, void *data,
			size_t len)
{
	int rc;

	if (!client || !client->pgdev || !client->name)
		return -ENODEV;

	if (!client->pgdev->rpdev || !atomic_read(&client->pgdev->state)) {
		pr_err("Error in sending data for client %s\n", client->name);
		return -ENOTCONN;
	}

	mutex_lock(&client->lock);
	rc = rpmsg_trysend(client->pgdev->rpdev->ept, data, len);
	mutex_unlock(&client->lock);

	return rc;
}
EXPORT_SYMBOL(pmic_glink_write);

/**
 * pmic_glink_register_client() - Register a PMIC Glink client
 *
 * @dev: Device pointer of child device
 * @client_data: Client device data pointer
 *
 * Return: Valid client pointer upon success or ERR_PTR(-ERRNO)
 *
 * This function should be called by a client with a unique id, name and
 * callback function so that the pmic_glink driver can route the messages
 * to the client.
 */
struct pmic_glink_client *pmic_glink_register_client(struct device *dev,
			const struct pmic_glink_client_data *client_data)
{
	int rc;
	struct pmic_glink_dev *pgdev;
	struct pmic_glink_client *client;

	if (!dev || !dev->parent)
		return ERR_PTR(-ENODEV);

	if (!client_data->id || !client_data->callback || !client_data->name)
		return ERR_PTR(-EINVAL);

	pgdev = get_pmic_glink_from_dev(dev->parent);
	if (!pgdev) {
		pr_err("Failed to get pmic_glink_dev for %s\n",
			client_data->name);
		return ERR_PTR(-ENODEV);
	}

	if (!atomic_read(&pgdev->state)) {
		pr_err("pmic_glink is not up\n");
		return ERR_PTR(-EPROBE_DEFER);
	}

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->name = kstrdup(client_data->name, GFP_KERNEL);
	if (!client->name) {
		kfree(client);
		return ERR_PTR(-ENOMEM);
	}

	mutex_init(&client->lock);
	client->id = client_data->id;
	client->callback = client_data->callback;
	client->priv = client_data->priv;
	client->pgdev = pgdev;

	mutex_lock(&pgdev->client_lock);
	rc = idr_alloc(&pgdev->client_idr, client, client->id, client->id + 1,
			GFP_KERNEL);
	if (rc < 0) {
		pr_err("Error in allocating idr for client %s, rc=%d\n",
			client->name, rc);
		mutex_unlock(&pgdev->client_lock);
		kfree(client->name);
		kfree(client);
		return ERR_PTR(rc);
	}

	mutex_unlock(&pgdev->client_lock);

	return client;
}
EXPORT_SYMBOL(pmic_glink_register_client);

/**
 * pmic_glink_unregister_client() - Unregister a PMIC Glink client
 *
 * @client: Client device pointer that is registered already
 *
 * Return: 0 if success, negative on error.
 *
 * This function should be called by a client when it wants to unregister from
 * pmic_glink driver. Messages will not be routed to client after this is done.
 */
int pmic_glink_unregister_client(struct pmic_glink_client *client)
{
	if (!client || !client->pgdev)
		return -ENODEV;

	mutex_lock(&client->pgdev->client_lock);
	idr_remove(&client->pgdev->client_idr, client->id);
	mutex_unlock(&client->pgdev->client_lock);

	kfree(client->name);
	kfree(client);
	return 0;
}
EXPORT_SYMBOL(pmic_glink_unregister_client);

static void pmic_glink_rx_callback(struct pmic_glink_dev *pgdev,
					struct pmic_glink_buf *pbuf)
{
	struct pmic_glink_client *client;
	struct pmic_glink_hdr *hdr;

	hdr = (struct pmic_glink_hdr *)pbuf->buf;

	mutex_lock(&pgdev->client_lock);
	client = idr_find(&pgdev->client_idr, hdr->owner);
	mutex_unlock(&pgdev->client_lock);

	if (!client || !client->callback) {
		pr_err("No client present for %u\n", hdr->owner);
		return;
	}

	client->callback(client->priv, pbuf->buf, pbuf->len);
}

static void pmic_glink_rx_work(struct work_struct *work)
{
	struct pmic_glink_dev *pdev = container_of(work, struct pmic_glink_dev,
						rx_work);
	struct pmic_glink_buf *pbuf, *tmp;
	unsigned long flags;

	if (!list_empty(&pdev->rx_list)) {
		list_for_each_entry_safe(pbuf, tmp, &pdev->rx_list, node) {
			pmic_glink_rx_callback(pdev, pbuf);
			spin_lock_irqsave(&pdev->rx_lock, flags);
			list_del(&pbuf->node);
			spin_unlock_irqrestore(&pdev->rx_lock, flags);
			kfree(pbuf);
		}
	}
}

static int pmic_glink_rpmsg_callback(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 addr)
{
	struct pmic_glink_dev *pdev = dev_get_drvdata(&rpdev->dev);
	struct pmic_glink_buf *pbuf;
	unsigned long flags;

	pbuf = kzalloc(sizeof(*pbuf) + len, GFP_ATOMIC);
	if (!pbuf)
		return -ENOMEM;

	pbuf->len = len;
	memcpy(pbuf->buf, data, len);

	spin_lock_irqsave(&pdev->rx_lock, flags);
	list_add_tail(&pbuf->node, &pdev->rx_list);
	spin_unlock_irqrestore(&pdev->rx_lock, flags);

	queue_work(pdev->rx_wq, &pdev->rx_work);
	return 0;
}

static void pmic_glink_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct pmic_glink_dev *pgdev = NULL;

	pgdev = get_pmic_glink_from_rpdev(rpdev);
	if (!pgdev) {
		pr_err("Failed to get pmic_glink_dev for %s\n", rpdev->id.name);
		return;
	}

	atomic_set(&pgdev->state, 0);
	pgdev->rpdev = NULL;
	pr_debug("%s removed\n", rpdev->id.name);
}

static int pmic_glink_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct pmic_glink_dev *pgdev = NULL;

	pgdev = get_pmic_glink_from_rpdev(rpdev);
	if (!pgdev) {
		pr_err("Failed to get pmic_glink_dev for %s\n", rpdev->id.name);
		return -EPROBE_DEFER;
	}

	dev_set_drvdata(&rpdev->dev, pgdev);
	pgdev->rpdev = rpdev;
	atomic_set(&pgdev->state, 1);
	schedule_work(&pgdev->init_work);
	pr_debug("%s probed\n", rpdev->id.name);

	return 0;
}

static const struct rpmsg_device_id pmic_glink_rpmsg_match[] = {
	{ "PMIC_RTR_ADSP_APPS" },
	{ "PMIC_LOGS_ADSP_APPS" },
	{}
};

static struct rpmsg_driver pmic_glink_rpmsg_driver = {
	.id_table = pmic_glink_rpmsg_match,
	.probe = pmic_glink_rpmsg_probe,
	.remove = pmic_glink_rpmsg_remove,
	.callback = pmic_glink_rpmsg_callback,
	.drv = {
		.name = "pmic_glink_rpmsg",
	},
};

static void pmic_glink_init_work(struct work_struct *work)
{
	struct pmic_glink_dev *pgdev = container_of(work, struct pmic_glink_dev,
					init_work);
	struct device *dev = pgdev->dev;
	int rc;

	if (pgdev->child_probed)
		return;

	rc = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (rc < 0)
		pr_err("Failed to create devices rc=%d\n", rc);
	else
		pgdev->child_probed = true;
}

static void pmic_glink_dev_add(struct pmic_glink_dev *pgdev)
{
	mutex_lock(&pmic_glink_dev_lock);
	list_add(&pgdev->dev_list, &pmic_glink_dev_list);
	mutex_unlock(&pmic_glink_dev_lock);
}

static void pmic_glink_dev_remove(struct pmic_glink_dev *pgdev)
{
	struct pmic_glink_dev *pos, *tmp;

	mutex_lock(&pmic_glink_dev_lock);
	list_for_each_entry_safe(pos, tmp, &pmic_glink_dev_list, dev_list) {
		if (pos == pgdev)
			list_del(&pgdev->dev_list);
	}
	mutex_unlock(&pmic_glink_dev_lock);
}

static int pmic_glink_probe(struct platform_device *pdev)
{
	struct pmic_glink_dev *pgdev;
	struct device *dev = &pdev->dev;
	int rc;

	pgdev = devm_kzalloc(dev, sizeof(*pgdev), GFP_KERNEL);
	if (!pgdev)
		return -ENOMEM;

	rc = of_property_read_string(dev->of_node, "qcom,pmic-glink-channel",
					&pgdev->channel_name);
	if (rc < 0) {
		pr_err("Error in reading qcom,pmic-glink-channel rc=%d\n", rc);
		return rc;
	}

	if (strlen(pgdev->channel_name) > RPMSG_NAME_SIZE) {
		pr_err("pmic glink channel name %s exceeds length\n",
			pgdev->channel_name);
		return -EINVAL;
	}

	pgdev->rx_wq = create_singlethread_workqueue("pmic_glink_rx");
	if (!pgdev->rx_wq) {
		pr_err("Failed to create pmic_glink_rx wq\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, pgdev);

	INIT_WORK(&pgdev->rx_work, pmic_glink_rx_work);
	INIT_WORK(&pgdev->init_work, pmic_glink_init_work);
	INIT_LIST_HEAD(&pgdev->rx_list);
	INIT_LIST_HEAD(&pgdev->dev_list);
	spin_lock_init(&pgdev->rx_lock);
	mutex_init(&pgdev->client_lock);
	idr_init(&pgdev->client_idr);
	pgdev->dev = dev;

	pmic_glink_dev_add(pgdev);

	pr_debug("%s probed successfully\n", pgdev->channel_name);
	return 0;
}

static int pmic_glink_remove(struct platform_device *pdev)
{
	struct pmic_glink_dev *pgdev = dev_get_drvdata(&pdev->dev);

	flush_workqueue(pgdev->rx_wq);
	destroy_workqueue(pgdev->rx_wq);
	idr_destroy(&pgdev->client_idr);
	of_platform_depopulate(&pdev->dev);
	pgdev->child_probed = false;
	pmic_glink_dev_remove(pgdev);

	return 0;
}

static const struct of_device_id pmic_glink_of_match[] = {
	{ .compatible = "qcom,pmic-glink" },
	{}
};
MODULE_DEVICE_TABLE(of, pmic_glink_of_match);

static struct platform_driver pmic_glink_driver = {
	.probe = pmic_glink_probe,
	.remove = pmic_glink_remove,
	.driver = {
		.name = "pmic_glink",
		.of_match_table = pmic_glink_of_match,
	},
};

static int __init pmic_glink_init(void)
{
	int rc;

	rc = platform_driver_register(&pmic_glink_driver);
	if (rc < 0)
		return rc;

	return register_rpmsg_driver(&pmic_glink_rpmsg_driver);
}
module_init(pmic_glink_init);

static void __exit pmic_glink_exit(void)
{
	unregister_rpmsg_driver(&pmic_glink_rpmsg_driver);
	platform_driver_unregister(&pmic_glink_driver);
}
module_exit(pmic_glink_exit);

MODULE_DESCRIPTION("QTI PMIC Glink driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("qcom,pmic-glink");
