/*
 * HECI bus driver
 *
 * Copyright (c) 2012-2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include "bus.h"
#include "heci_dev.h"
#include "client.h"
#include <asm/page.h>
#include "hbm.h"
#include "utils.h"

#define to_heci_cl_driver(d) container_of(d, struct heci_cl_driver, driver)
#define to_heci_cl_device(d) container_of(d, struct heci_cl_device, dev)

/**
 * heci_me_cl_by_uuid - locate index of me client
 *
 * @dev: heci device
 * returns me client index or -ENOENT if not found
 */
int heci_me_cl_by_uuid(const struct heci_device *dev, const uuid_le *uuid)
{
	int i, res = -ENOENT;

	for (i = 0; i < dev->me_clients_num; ++i) {
		if (uuid_le_cmp(*uuid, dev->me_clients[i].props.protocol_name)
				== 0) {
			res = i;
			break;
		}
	}
	return res;
}
EXPORT_SYMBOL(heci_me_cl_by_uuid);


/**
 * heci_me_cl_by_id return index to me_clients for client_id
 *
 * @dev: the device structure
 * @client_id: me client id
 *
 * returns index on success, -ENOENT on failure.
 */

int heci_me_cl_by_id(struct heci_device *dev, u8 client_id)
{
	int i;
	for (i = 0; i < dev->me_clients_num; i++)
		if (dev->me_clients[i].client_id == client_id)
			break;
	if (WARN_ON(dev->me_clients[i].client_id != client_id))
		return -ENOENT;

	if (i == dev->me_clients_num)
		return -ENOENT;

	return i;
}

static int heci_cl_device_match(struct device *dev, struct device_driver *drv)
{
	ISH_DBG_PRINT(KERN_ALERT "%s(): +++ returns 1\n", __func__);

	/*
	 * DD -- return true and let driver's probe() routine decide.
	 * If this solution lives up, we can rearrange it
	 * by simply removing match() routine at all
	 */
	return	1;
}

static int heci_cl_device_probe(struct device *dev)
{
	struct heci_cl_device *device = to_heci_cl_device(dev);
	struct heci_cl_driver *driver;
	struct heci_cl_device_id id;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++\n", __func__);
	if (!device)
		return 0;

	/* in many cases here will be NULL */
	driver = to_heci_cl_driver(dev->driver);
	if (!driver || !driver->probe)
		return -ENODEV;

	dev_dbg(dev, "Device probe\n");

	strncpy(id.name, dev_name(dev), HECI_CL_NAME_SIZE-1);
	id.name[HECI_CL_NAME_SIZE-1] = '\0';

	return driver->probe(device, &id);
}

static int heci_cl_device_remove(struct device *dev)
{
	struct heci_cl_device *device = to_heci_cl_device(dev);
	struct heci_cl_driver *driver;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++\n", __func__);
	if (!device || !dev->driver)
		return 0;

	if (device->event_cb) {
		device->event_cb = NULL;
		cancel_work_sync(&device->event_work);
	}

	driver = to_heci_cl_driver(dev->driver);
	if (!driver->remove) {
		dev->driver = NULL;

		return 0;
	}

	return driver->remove(device);
}

static ssize_t modalias_show(struct device *dev, struct device_attribute *a,
	char *buf)
{
	int len;

	len = snprintf(buf, PAGE_SIZE, "heci:%s\n", dev_name(dev));
	return (len >= PAGE_SIZE) ? (PAGE_SIZE - 1) : len;
}

static struct device_attribute heci_cl_dev_attrs[] = {
	__ATTR_RO(modalias),
	__ATTR_NULL,
};

static int heci_cl_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	if (add_uevent_var(env, "MODALIAS=heci:%s", dev_name(dev)))
		return -ENOMEM;

	return 0;
}

static struct bus_type heci_cl_bus_type = {
	.name		= "heci",
	.dev_attrs	= heci_cl_dev_attrs,
	.match		= heci_cl_device_match,
	.probe		= heci_cl_device_probe,
	.remove		= heci_cl_device_remove,
	.uevent		= heci_cl_uevent,
};

static void heci_cl_dev_release(struct device *dev)
{
	ISH_DBG_PRINT(KERN_ALERT "%s():+++\n", __func__);
	kfree(to_heci_cl_device(dev));
	ISH_DBG_PRINT(KERN_ALERT "%s():---\n", __func__);
}

static struct device_type heci_cl_device_type = {
	.release	= heci_cl_dev_release,
};

/*
 * Allocate HECI bus client device, attach it to uuid and register with HECI bus
 */
struct heci_cl_device *heci_bus_add_device(struct heci_device *dev,
	uuid_le uuid, char *name, struct heci_cl_dev_ops *ops)
{
	struct heci_cl_device *device;
	int status;

	device = kzalloc(sizeof(struct heci_cl_device), GFP_KERNEL);
	if (!device)
		return NULL;

	device->ops = ops;

	device->dev.parent = &dev->pdev->dev;
	device->dev.bus = &heci_cl_bus_type;
	device->dev.type = &heci_cl_device_type;
	device->heci_dev = dev;
	device->fw_client =
		&dev->me_clients[dev->me_client_presentation_num - 1];

	dev_set_name(&device->dev, "%s", name);
	list_add_tail(&device->device_link, &dev->device_list);

	status = device_register(&device->dev);
	if (status) {
		list_del(&device->device_link);
		dev_err(&dev->pdev->dev, "Failed to register HECI device\n");
		kfree(device);
		return NULL;
	}

	dev_dbg(&device->dev, "client %s registered\n", name);
	ISH_DBG_PRINT(KERN_ALERT "%s(): Registered HECI device\n", __func__);

	return device;
}
EXPORT_SYMBOL_GPL(heci_bus_add_device);


/*
 * This is a counterpart of heci_bus_add_device.
 * Device is unregistered and its structure is also freed
 */
void heci_bus_remove_device(struct heci_cl_device *device)
{
	device_unregister(&device->dev);
	/*kfree(device);*/
}
EXPORT_SYMBOL_GPL(heci_bus_remove_device);


/*
 * Part of reset flow
 */
void	heci_bus_remove_all_clients(struct heci_device *heci_dev)
{
	struct heci_cl_device *heci_cl_dev;
	struct heci_cl	*cl, *next;
	unsigned long	flags;

	spin_lock_irqsave(&heci_dev->device_lock, flags);
	list_for_each_entry_safe(cl, next, &heci_dev->cl_list, link) {
		list_del(&cl->link);
		spin_unlock_irqrestore(&heci_dev->device_lock, flags);
		heci_cl_dev = cl->device;

		/*
		 * Wake any pending process. The waiter would check dev->state
		 * and determine that it's not enabled already,
		 * and will return error to its caller
		 */
		if (waitqueue_active(&cl->rx_wait))
			wake_up_interruptible(&cl->rx_wait);
		if (waitqueue_active(&cl->wait_ctrl_res))
			wake_up(&cl->wait_ctrl_res);

		/* Disband any pending read/write requests */
		heci_cl_flush_queues(cl);

		if (cl->read_rb) {
			struct heci_cl_rb *rb = NULL;

			rb = heci_cl_find_read_rb(cl);
			/* Remove entry from read list */
			if (rb)
				list_del(&rb->list);

			rb = cl->read_rb;
			cl->read_rb = NULL;

			if (rb) {
				heci_io_rb_free(rb);
				rb = NULL;
			}
		}

		/* Unregister HECI bus client device */
		heci_bus_remove_device(heci_cl_dev);

		/* Free client and HECI bus client device structures */
		kfree(cl);
		spin_lock_irqsave(&heci_dev->device_lock, flags);
	}
	spin_unlock_irqrestore(&heci_dev->device_lock, flags);
#if 0
	if (waitqueue_active(&heci_dev->wait_recvd_msg))
		wake_up(&heci_dev->wait_recvd_msg);
#endif

	/* Free all client structures */
	kfree(heci_dev->me_clients);
	heci_dev->me_clients = NULL;
	heci_dev->me_clients_num = 0;
	heci_dev->me_client_presentation_num  = 0;
	heci_dev->me_client_index = 0;
	bitmap_zero(heci_dev->me_clients_map, HECI_CLIENTS_MAX);
	bitmap_zero(heci_dev->host_clients_map, HECI_CLIENTS_MAX);
	bitmap_set(heci_dev->host_clients_map, 0, 3);
}
EXPORT_SYMBOL_GPL(heci_bus_remove_all_clients);


int __heci_cl_driver_register(struct heci_cl_driver *driver,
	struct module *owner)
{
	int err;

	driver->driver.name = driver->name;
	driver->driver.owner = owner;
	driver->driver.bus = &heci_cl_bus_type;

	err = driver_register(&driver->driver);
	if (err)
		return err;

	ISH_DBG_PRINT(KERN_ALERT "%s(): heci: driver [%s] registered\n",
		__func__, driver->driver.name);
	pr_debug("heci: driver [%s] registered\n", driver->driver.name);
	return 0;
}
EXPORT_SYMBOL_GPL(__heci_cl_driver_register);

void heci_cl_driver_unregister(struct heci_cl_driver *driver)
{
	driver_unregister(&driver->driver);

	pr_debug("heci: driver [%s] unregistered\n", driver->driver.name);
}
EXPORT_SYMBOL_GPL(heci_cl_driver_unregister);


static void heci_bus_event_work(struct work_struct *work)
{
	struct heci_cl_device *device;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++\n", __func__);
	device = container_of(work, struct heci_cl_device, event_work);

	if (device->event_cb)
		device->event_cb(device, device->events, device->event_context);

	/*device->events = 0;*/
}

int heci_register_event_cb(struct heci_cl_device *device,
	void (*event_cb)(struct heci_cl_device *, u32, void *), void *context)
{
	if (device->event_cb)
		return -EALREADY;

	/*device->events = 0;*/
	device->event_cb = event_cb;
	device->event_context = context;
	INIT_WORK(&device->event_work, heci_bus_event_work);

	return 0;
}
EXPORT_SYMBOL_GPL(heci_register_event_cb);

void *heci_cl_get_drvdata(const struct heci_cl_device *device)
{
	return dev_get_drvdata(&device->dev);
}
EXPORT_SYMBOL_GPL(heci_cl_get_drvdata);

void heci_cl_set_drvdata(struct heci_cl_device *device, void *data)
{
	dev_set_drvdata(&device->dev, data);
}
EXPORT_SYMBOL_GPL(heci_cl_set_drvdata);

/* What's this? */
int heci_cl_enable_device(struct heci_cl_device *device)
{
	if (!device->ops || !device->ops->enable)
		return 0;

	return device->ops->enable(device);
}
EXPORT_SYMBOL_GPL(heci_cl_enable_device);

int heci_cl_disable_device(struct heci_cl_device *device)
{
	if (!device->ops || !device->ops->disable)
		return 0;

	return device->ops->disable(device);
}
EXPORT_SYMBOL_GPL(heci_cl_disable_device);
/************************/

void heci_cl_bus_rx_event(struct heci_cl_device *device)
{
	static int	rx_count;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++ [%d]\n", __func__, rx_count++);
	if (!device || !device->event_cb)
		return;

	set_bit(HECI_CL_EVENT_RX, &device->events);

	if (device->event_cb)
		schedule_work(&device->event_work);
}

int __init heci_cl_bus_init(void)
{
	int	rv;

	ISH_DBG_PRINT(KERN_ALERT "%s(): Registering HECI bus\n", __func__);
	rv = bus_register(&heci_cl_bus_type);
	if (!rv)
		heci_cl_alloc_dma_buf();
	return	rv;
}

void __exit heci_cl_bus_exit(void)
{
ISH_DBG_PRINT(KERN_ALERT "%s(): Unregistering HECI bus\n", __func__);
	bus_unregister(&heci_cl_bus_type);
}


ssize_t cl_prop_read(struct device *dev, struct device_attribute *dev_attr,
	char *buf)
{
	ssize_t	rv = -EINVAL;
	struct heci_cl_device	*cl_device = to_heci_cl_device(dev);
	unsigned long	flags;

	if (!strcmp(dev_attr->attr.name, "max_msg_length")) {
		scnprintf(buf, PAGE_SIZE, "%u\n",
			(unsigned)cl_device->fw_client->props.max_msg_length);
		rv = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name, "protocol_version")) {
		scnprintf(buf, PAGE_SIZE, "%u\n",
			(unsigned)cl_device->fw_client->props.protocol_version);
		rv = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name,  "max_number_of_connections")) {
		scnprintf(buf, PAGE_SIZE, "%u\n",
			(unsigned)cl_device->fw_client->props.max_number_of_connections);
		rv = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name,  "fixed_address")) {
		scnprintf(buf, PAGE_SIZE, "%u\n",
			(unsigned)cl_device->fw_client->props.fixed_address);
		rv = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name,  "single_recv_buf")) {
		scnprintf(buf, PAGE_SIZE, "%u\n",
			(unsigned)cl_device->fw_client->props.single_recv_buf);
		rv = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name,  "dma_hdr_len")) {
		scnprintf(buf, PAGE_SIZE, "%u\n",
			(unsigned)cl_device->fw_client->props.dma_hdr_len);
		rv = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name,  "num_active_connections")) {
		struct heci_cl	*cl, *next;
		unsigned	count = 0;

		spin_lock_irqsave(&cl_device->heci_dev->device_lock, flags);
		list_for_each_entry_safe(cl, next,
				&cl_device->heci_dev->cl_list, link) {
			if (cl->state == HECI_CL_CONNECTED &&
					cl->device == cl_device)
				++count;
		}
		spin_unlock_irqrestore(&cl_device->heci_dev->device_lock,
			flags);

		scnprintf(buf, PAGE_SIZE, "%u\n", count);
		rv = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name,  "client_id")) {
		scnprintf(buf, PAGE_SIZE, "%u\n",
			(unsigned)cl_device->fw_client->client_id);
		rv = strlen(buf);
	}

	return	rv;
}

ssize_t	cl_prop_write(struct device *dev, struct device_attribute *dev_attr,
	const char *buf, size_t count)
{
	return	-EINVAL;
}

static struct device_attribute	max_msg_length = {
	.attr = {
		.name = "max_msg_length",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = cl_prop_read,
	.store = cl_prop_write
};

static struct device_attribute	protocol_version = {
	.attr = {
		.name = "protocol_version",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = cl_prop_read,
	.store = cl_prop_write
};

static struct device_attribute	max_number_of_connections = {
	.attr = {
		.name = "max_number_of_connections",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = cl_prop_read,
	.store = cl_prop_write
};

static struct device_attribute	fixed_address = {
	.attr = {
		.name = "fixed_address",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = cl_prop_read,
	.store = cl_prop_write
};

static struct device_attribute	single_recv_buf = {
	.attr = {
		.name = "single_recv_buf",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = cl_prop_read,
	.store = cl_prop_write
};

static struct device_attribute	dma_hdr_len = {
	.attr = {
		.name = "dma_hdr_len",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = cl_prop_read,
	.store = cl_prop_write
};

static struct device_attribute	num_active_connections = {
	.attr = {
		.name = "num_active_connections",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = cl_prop_read,
	.store = cl_prop_write
};

static struct device_attribute	client_id = {
	.attr = {
		.name = "client_id",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = cl_prop_read,
	.store = cl_prop_write
};

/*
 * Enum-completion callback for HECI bus - heci_device has reported its clients
 */
int	heci_bus_new_client(struct heci_device *dev)
{
	int	i;
	char	*dev_name;
	struct heci_cl_device	*cl_device;
	uuid_le	device_uuid;

	/*
	 * For all reported clients, create an unconnected client and add its
	 * device to HECI bus.
	 * If appropriate driver has loaded, this will trigger its probe().
	 * Otherwise, probe() will be called when driver is loaded
	 */
	i = dev->me_client_presentation_num - 1;
	device_uuid = dev->me_clients[i].props.protocol_name;
	dev_name = kasprintf(GFP_ATOMIC,
		"{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		device_uuid.b[3], device_uuid.b[2], device_uuid.b[1],
		device_uuid.b[0], device_uuid.b[5], device_uuid.b[4],
		device_uuid.b[7], device_uuid.b[6], device_uuid.b[8],
		device_uuid.b[9], device_uuid.b[10], device_uuid.b[11],
		device_uuid.b[12], device_uuid.b[13], device_uuid.b[14],
		device_uuid.b[15]);
	if (!dev_name)
		return	-ENOMEM;

	cl_device = heci_bus_add_device(dev, device_uuid, dev_name, NULL);
	if (!cl_device) {
		kfree(dev_name);
		return	-ENOENT;
	}

	/* Export several properties per client device */
	device_create_file(&cl_device->dev, &max_msg_length);
	device_create_file(&cl_device->dev, &protocol_version);
	device_create_file(&cl_device->dev, &max_number_of_connections);
	device_create_file(&cl_device->dev, &fixed_address);
	device_create_file(&cl_device->dev, &single_recv_buf);
	device_create_file(&cl_device->dev, &dma_hdr_len);
	device_create_file(&cl_device->dev, &num_active_connections);
	device_create_file(&cl_device->dev, &client_id);
	kfree(dev_name);

	return	0;
}


static int	does_driver_bind_uuid(struct device *dev, void *id)
{
	uuid_le	*uuid = id;
	struct heci_cl_device	*device;

	if (!dev->driver)
		return	0;

	device = to_heci_cl_device(dev);
	if (!uuid_le_cmp(device->fw_client->props.protocol_name, *uuid))
		return	1;

	return	0;
}


int	heci_can_client_connect(struct heci_device *heci_dev, uuid_le *uuid)
{
	int	rv;

	rv = bus_for_each_dev(&heci_cl_bus_type, NULL, uuid,
		does_driver_bind_uuid);
	return	!rv;
}


/* Binds connected heci_cl to HECI bus device */
int	heci_cl_device_bind(struct heci_cl *cl)
{
	int	rv;
	struct heci_cl_device	*cl_device, *next;

	if (!cl->me_client_id || cl->state != HECI_CL_CONNECTED)
		return	-EFAULT;

	rv = -ENOENT;
	list_for_each_entry_safe(cl_device, next, &cl->dev->device_list,
			device_link) {
		if (cl_device->fw_client->client_id == cl->me_client_id) {
			cl->device = cl_device;
			rv = 0;
			break;
		}
	}

	return	rv;
}

