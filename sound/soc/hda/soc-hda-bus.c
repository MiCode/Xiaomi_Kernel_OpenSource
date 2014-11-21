/*
 *  soc_hda_bus.c - HDA bus Inteface for HDA devices
 *
 *  Copyright (C) 2014 Intel Corp
 *  Author: Jeeja KP<jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/string.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/pci.h>
#include <sound/core.h>
#include <sound/soc-hda-bus.h>

#define HDA_RW_NO_RESPONSE_FALLBACK (1 << 0)

static void soc_hda_bus_dev_release(struct device *dev)
{
	return;
}

struct device snd_soc_hda_bus_dev = {
	.init_name      = "hda",
	.release	= soc_hda_bus_dev_release,
};

struct snd_soc_hda_object {
	struct snd_soc_hda_device pdev;
	char name[1];
};

struct bus_type snd_soc_hda_bus_type;

/**
 * hda_device_put - destroy a hda device
 * @pdev: hda device to free
 *
 * Free all memory associated with a hda device.  This function must
 * _only_ be externally called in error cases.  All other usage is a bug.
 */
void snd_soc_hda_device_put(struct snd_soc_hda_device *pdev)
{
	if (pdev)
		put_device(&pdev->dev);
}
EXPORT_SYMBOL_GPL(snd_soc_hda_device_put);

static void soc_hda_device_release(struct device *dev)
{
	struct snd_soc_hda_object *hda = container_of(dev,
			struct snd_soc_hda_object, pdev.dev);

	kfree(hda->pdev.dev.platform_data);
	kfree(hda);
}

/**
 * snd_soc_hda_device_alloc - create a hda device
 * @name: base name of the device we're adding
 * @id: instance id
 *
 * Create a hda device object which can have other objects attached
 * to it, and which will have attached objects freed when it is released.
 */
struct snd_soc_hda_device *snd_soc_hda_device_alloc(const char *name,
		int addr, unsigned int id)
{
	struct snd_soc_hda_object *hda;

	hda = kzalloc(sizeof(struct snd_soc_hda_object) + strlen(name), GFP_KERNEL);
	if (hda) {
		strcpy(hda->name, name);
		hda->pdev.name = hda->name;
		hda->pdev.id = id;
		hda->pdev.addr = addr;
		device_initialize(&hda->pdev.dev);
		hda->pdev.dev.release = soc_hda_device_release;
	}

	return hda ? &hda->pdev : NULL;
}
EXPORT_SYMBOL_GPL(snd_soc_hda_device_alloc);

/**
 * snd_soc_hda_device_add_data - add platform-specific data to a hda device
 * @pdev: hda device allocated by platform_device_alloc to add resources to
 * @data: hda specific data for this hda device
 * @size: size of platform specific data
 *
 */
int snd_soc_hda_device_add_data(struct snd_soc_hda_device *pdev, const void *data,
			     size_t size)
{
	void *d = NULL;

	if (data) {
		d = kmemdup(data, size, GFP_KERNEL);
		if (!d)
			return -ENOMEM;
	}

	kfree(pdev->dev.platform_data);
	pdev->dev.platform_data = d;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_hda_device_add_data);

/**
 * snd_soc_hda_device_add - add a hda device to device hierarchy
 * @pdev: hda device we're adding
 *
 */
int snd_soc_hda_device_add(struct snd_soc_hda_device *pdev)
{
	int ret;

	if (!pdev)
		return -EINVAL;

	if (!pdev->dev.parent)
		pdev->dev.parent = &snd_soc_hda_bus_dev;

	pdev->dev.bus = &snd_soc_hda_bus_type;

	if (pdev->id < 0)
		dev_set_name(&pdev->dev, "%s", pdev->name);
	else
		dev_set_name(&pdev->dev, "%s.%d", pdev->name,  pdev->addr);

	dev_info(&pdev->dev, "Registering hda device '%s'. Parent at %s\n",
		 dev_name(&pdev->dev), dev_name(pdev->dev.parent));

	ret = device_add(&pdev->dev);

	return ret;
}
EXPORT_SYMBOL_GPL(snd_soc_hda_device_add);

/**
 * snd_soc_hda_device_del - remove a hda device
 * @pdev: hda device we're removing
 *
 */
void snd_soc_hda_device_del(struct snd_soc_hda_device *pdev)
{
	if (pdev)
		device_del(&pdev->dev);
}
EXPORT_SYMBOL_GPL(snd_soc_hda_device_del);

/**
 * hda_device_register - add a hda device
 * @pdev: hda device we're adding
 */
int snd_soc_hda_device_register(struct snd_soc_hda_device *pdev)
{
	device_initialize(&pdev->dev);
	return snd_soc_hda_device_add(pdev);
}
EXPORT_SYMBOL_GPL(snd_soc_hda_device_register);

/**
 * hda_device_unregister - unregister a hda device
 * @pdev: hda device we're unregistering
 *
 * Unregistration is done in 2 steps. First we release all resources
 * and remove it from the subsystem, then we drop reference count by
 * calling hda_device_put().
 */
void snd_soc_hda_device_unregister(struct snd_soc_hda_device *pdev)
{
	snd_soc_hda_device_del(pdev);
	snd_soc_hda_device_put(pdev);
}
EXPORT_SYMBOL_GPL(snd_soc_hda_device_unregister);

static int soc_hda_drv_probe(struct device *dev)
{
	struct snd_soc_hda_driver *drv = to_soc_hda_driver(dev->driver);

	return drv->probe(to_soc_hda_device(dev));
}


static int soc_hda_drv_remove(struct device *dev)
{
	struct snd_soc_hda_driver *drv = to_soc_hda_driver(dev->driver);

	return drv->remove(to_soc_hda_device(dev));
}

static void soc_hda_drv_shutdown(struct device *dev)
{
	struct snd_soc_hda_driver *drv = to_soc_hda_driver(dev->driver);

	drv->shutdown(to_soc_hda_device(dev));
}

static void soc_hda_drv_unsol_event(struct device *_dev,  unsigned int res)
{
	struct snd_soc_hda_driver *drv = to_soc_hda_driver(_dev->driver);
	struct snd_soc_hda_device *dev = to_soc_hda_device(_dev);

	if (drv->unsol_event)
		drv->unsol_event(dev, res);
}

/**
 * hda_driver_register - register a driver for hda devices
 * @drv: hda driver structure
 */
int snd_soc_hda_driver_register(struct snd_soc_hda_driver *drv)
{
	drv->driver.bus = &snd_soc_hda_bus_type;
	if (drv->probe)
		drv->driver.probe = soc_hda_drv_probe;
	if (drv->remove)
		drv->driver.remove = soc_hda_drv_remove;
	if (drv->shutdown)
		drv->driver.shutdown = soc_hda_drv_shutdown;

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL_GPL(snd_soc_hda_driver_register);

/**
 * hda_driver_unregister - unregister a driver for hda devices
 * @drv: hda driver structure
 */
void snd_soc_hda_driver_unregister(struct snd_soc_hda_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL_GPL(snd_soc_hda_driver_unregister);

static ssize_t modalias_show(struct device *dev, struct device_attribute *a,
			     char *buf)
{
	struct snd_soc_hda_device	*hdev = to_soc_hda_device(dev);

	return sprintf(buf, "%s%s\n", HDA_MODULE_PREFIX, hdev->name);
}

static struct device_attribute soc_hda_dev_attrs[] = {
	__ATTR_RO(modalias),
	__ATTR_NULL,
};

static int soc_hda_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct snd_soc_hda_device	*hdev = to_soc_hda_device(dev);

	add_uevent_var(env, "MODALIAS=%s%s", HDA_MODULE_PREFIX,
			hdev->name);
	return 0;
}

static const struct snd_soc_hda_device_id *soc_hda_match_id(
			const struct snd_soc_hda_device_id *id,
			struct snd_soc_hda_device *pdev)
{
	while (id->name[0]) {
		if (pdev->id == id->id) {
			pdev->id_entry = id;
			return id;
		} else if (strcmp(pdev->name, id->name) == 0) {
				pdev->id_entry = id;
				return id;
		}
		id++;
	}
	return NULL;
}

/**
 * hda_match - bind hda device to hda driver.
 * @dev: device.
 * @drv: driver.
 *
 */
static int soc_hda_match(struct device *dev, struct device_driver *drv)
{
	struct snd_soc_hda_device *pdev = to_soc_hda_device(dev);
	struct snd_soc_hda_driver *pdrv = to_soc_hda_driver(drv);

	/* Then try to match against the id table */
	if (pdrv->id_table)
		return soc_hda_match_id(pdrv->id_table, pdev) != NULL;

	/* fall-back to driver name match */
	return (strcmp(pdev->name, drv->name) == 0);
}

#ifdef CONFIG_PM_SLEEP

static int soc_hda_legacy_suspend(struct device *dev, pm_message_t mesg)
{
	struct snd_soc_hda_driver *pdrv = to_soc_hda_driver(dev->driver);
	struct snd_soc_hda_device *pdev = to_soc_hda_device(dev);

	if (pdrv) {
		if (pdrv->suspend)
			return pdrv->suspend(pdev, mesg);
	}
	return 0;
}

static int soc_hda_legacy_resume(struct device *dev)
{
	struct snd_soc_hda_driver *pdrv = to_soc_hda_driver(dev->driver);
	struct snd_soc_hda_device *pdev = to_soc_hda_device(dev);

	if (pdrv && pdrv->resume)
		return pdrv->resume(pdev);
	return 0;
}

int snd_soc_hda_pm_suspend(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_suspend(dev);
	else
		return soc_hda_legacy_suspend(dev, PMSG_SUSPEND);
}

int snd_soc_hda_pm_resume(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_resume(dev);
	else
		return soc_hda_legacy_resume(dev);
}

int snd_soc_hda_pm_freeze(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_freeze(dev);
	else
		return soc_hda_legacy_suspend(dev, PMSG_FREEZE);
}

int snd_soc_hda_pm_thaw(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_thaw(dev);
	else
		return soc_hda_legacy_resume(dev);
}

int snd_soc_hda_pm_poweroff(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_poweroff(dev);
	else
		return soc_hda_legacy_suspend(dev, PMSG_HIBERNATE);
}

int snd_soc_hda_pm_restore(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_restore(dev);
	else
		return soc_hda_legacy_resume(dev);
}

#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops soc_hda_dev_pm_ops = {
	.runtime_suspend = pm_generic_runtime_suspend,
	.runtime_resume = pm_generic_runtime_resume,
	USE_HDA_PM_SLEEP_OPS
};

struct bus_type snd_soc_hda_bus_type = {
	.name		= "hda",
	.dev_attrs	= soc_hda_dev_attrs,
	.match		= soc_hda_match,
	.uevent		= soc_hda_uevent,
	.pm		= &soc_hda_dev_pm_ops,
};
EXPORT_SYMBOL_GPL(snd_soc_hda_bus_type);

static int match_addr(struct device *dev, void *data)
{
	unsigned int addr = *((unsigned int *)data);
	struct snd_soc_hda_device *pdev = to_soc_hda_device(dev);
	return (pdev->addr == addr) ? 1 : 0;
}

static struct device *bus_find_device_by_addr(unsigned int addr)
{
	return bus_find_device(&snd_soc_hda_bus_type, NULL, &addr, match_addr);

}

/*
 * process queued unsolicited events
 */
static void snd_soc_process_unsol_events(struct work_struct *work)
{
	struct hda_bus_unsolicited *unsol =
		container_of(work, struct hda_bus_unsolicited, work);
	unsigned int rp, caddr, res, addr;
	struct device *dev;

	while (unsol->rp != unsol->wp) {
		rp = (unsol->rp + 1) % HDA_UNSOL_QUEUE_SIZE;
		unsol->rp = rp;
		rp <<= 1;
		res = unsol->queue[rp];
		caddr = unsol->queue[rp + 1];
		if (!(caddr & (1 << 4))) /* no unsolicited event? */
			continue;
		addr = caddr & 0x0f;
		dev = bus_find_device_by_addr(addr);
		if (dev)
			soc_hda_drv_unsol_event(dev, res);
		else
			dev_err(dev, "no hda device found for addr=%d", addr);
	}
}

/*
 * initialize unsolicited queue
 */
static int init_unsol_queue(struct hda_bus *bus)
{
	struct hda_bus_unsolicited *unsol;

	if (bus->unsol) /* already initialized */
		return 0;

	unsol = kzalloc(sizeof(*unsol), GFP_KERNEL);
	if (!unsol) {
		pr_err("can't allocate unsolicited queue\n");
		return -ENOMEM;
	}
	INIT_WORK(&unsol->work, snd_soc_process_unsol_events);
	unsol->bus = bus;
	bus->unsol = unsol;
	return 0;
}

int snd_soc_hda_bus_init(struct pci_dev *pci, void *data,
	struct snd_soc_hda_bus_ops ops, struct snd_soc_hda_bus **busp)
{
	int ret;
	struct snd_soc_hda_bus *sbus;
	struct hda_bus *bus;

	sbus = kzalloc(sizeof(*sbus), GFP_KERNEL);
	if (sbus == NULL) {
		pr_err("can't allocate struct hda_bus\n");
		return -ENOMEM;
	}

	bus = &sbus->bus;
	sbus->ops = ops;
	bus->private_data = data;
	bus->pci = pci;

	mutex_init(&bus->cmd_mutex);
	mutex_init(&bus->prepare_mutex);

	snprintf(bus->workq_name, sizeof(bus->workq_name),
		 "hd-audio");
	bus->workq = create_singlethread_workqueue(bus->workq_name);
	if (!bus->workq) {
		pr_err(" cannot create workqueue %s\n",
			   bus->workq_name);
		kfree(sbus);
		return -ENOMEM;
	}

	ret = init_unsol_queue(bus);
	if (ret < 0)
		goto err;

	if (busp)
		*busp = sbus;

	snd_soc_hda_bus_dev.parent = &pci->dev;

	ret = device_register(&snd_soc_hda_bus_dev);
	if (ret)
		goto err;

	dev_set_drvdata(&snd_soc_hda_bus_dev, sbus);

	ret =  bus_register(&snd_soc_hda_bus_type);
	if (ret) {
		device_unregister(&snd_soc_hda_bus_dev);
		goto err;
	}

	return ret;
err:
	kfree(sbus);
	return ret;
}
EXPORT_SYMBOL_GPL(snd_soc_hda_bus_init);

static int soc_hda_bus_free(struct snd_soc_hda_bus *sbus)
{
	struct hda_bus *bus;

	if (!sbus)
		return 0;

	bus = &sbus->bus;

	if (bus->workq)
		flush_workqueue(bus->workq);

	kfree(bus->unsol);

	if (bus->workq)
		destroy_workqueue(bus->workq);
	kfree(sbus);
	return 0;
}

static int soc_hda_release_device(struct device *dev, void *data)
{
	struct snd_soc_hda_device *pdev = to_soc_hda_device(dev);
	snd_soc_hda_device_unregister(pdev);
	return 0;
}

void snd_soc_hda_bus_release(void)
{
	struct snd_soc_hda_bus *bus;

	bus = dev_get_drvdata(&snd_soc_hda_bus_dev);

	/*unregister all devices on bus */
	bus_for_each_dev(&snd_soc_hda_bus_type, NULL, NULL,
			soc_hda_release_device);

	soc_hda_bus_free(bus);
	bus_unregister(&snd_soc_hda_bus_type);
	device_unregister(&snd_soc_hda_bus_dev);
}
EXPORT_SYMBOL_GPL(snd_soc_hda_bus_release);

/*
 * Send and receive a verb
 */
int snd_soc_codec_exec_verb(struct snd_soc_hda_device *dev, unsigned int cmd,
			  int flags,  unsigned int *res)
{
	struct snd_soc_hda_bus *sbus = dev_get_drvdata(&snd_soc_hda_bus_dev);
	struct hda_bus *bus = &sbus->bus;
	int err;

	if (!bus)
		return -1;

	if (cmd == ~0)
		return -1;

	if (bus->sync_write)
		*res = -1;

	mutex_lock(&bus->cmd_mutex);
	if (flags & HDA_RW_NO_RESPONSE_FALLBACK)
		bus->no_response_fallback = 1;
	for (;;) {
		/*FIXME
			trace_hda_send_cmd(codec, cmd); */
		err = sbus->ops.command(bus, cmd);
		if (err != -EAGAIN)
			break;
		/* process pending verbs */
		sbus->ops.get_response(bus, dev->addr);
	}
	if (!err && res) {
		*res = sbus->ops.get_response(bus, dev->addr);
		/*FIXME
		trace_hda_get_response(codec, *res);*/
	}
	bus->no_response_fallback = 0;
	mutex_unlock(&bus->cmd_mutex);
	/*FIXME commenting it for test
	if (res && *res == -1 && bus->rirb_error) {
		if (bus->response_reset) {
			pr_err("resetting BUS due to "
				   "fatal communication error\n");
			//trace_hda_bus_reset(bus);
			bus->ops.bus_reset(bus);
		}
		goto again;
	} */

	/* clear reset-flag when the communication gets recovered */
	if (!err)
		bus->response_reset = 0;
	return err;
}
EXPORT_SYMBOL_GPL(snd_soc_codec_exec_verb);

const struct snd_pci_quirk *snd_soc_hda_quirk_lookup(struct snd_soc_hda_device *dev,
		const struct snd_pci_quirk *list)
{
	struct snd_soc_hda_bus *sbus = dev_get_drvdata(&snd_soc_hda_bus_dev);
	struct hda_bus *bus = &sbus->bus;

	return snd_pci_quirk_lookup(bus->pci, list);

}
EXPORT_SYMBOL_GPL(snd_soc_hda_quirk_lookup);

MODULE_DESCRIPTION("ASOC HDA bus core");
MODULE_LICENSE("GPL v2");

