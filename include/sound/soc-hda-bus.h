/*
 *  soc-hda-bus.h - Generic HDA bus/device model
 *
 *  Copyright (c) 2014 Intel Corporation
 *  Author: Jeeja KP <jeeja.kp@intel.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 */

#ifndef _SOC_HDA_BUS_H_
#define _SOC_HDA_BUS_H_

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <sound/hda_bus.h>

struct snd_soc_hda_device {
	const char	*name;
	unsigned int	id;
	unsigned int	addr;
	struct device	dev;
	const struct snd_soc_hda_device_id *id_entry;
};

#define soc_hda_get_device_id(pdev)    ((pdev)->id_entry)

#define to_soc_hda_device(x) container_of((x), struct snd_soc_hda_device, dev)

int snd_soc_hda_device_register(struct snd_soc_hda_device *);
void snd_soc_hda_device_unregister(struct snd_soc_hda_device *);

int snd_soc_hda_add_devices(struct snd_soc_hda_device **, int);
struct snd_soc_hda_device *snd_soc_hda_device_alloc(const char *name,
					int addr, unsigned int id);

int snd_soc_hda_device_add_data(struct snd_soc_hda_device *pdev,
				const void *data, size_t size);
int snd_soc_hda_device_add(struct snd_soc_hda_device *pdev);
void snd_soc_hda_device_del(struct snd_soc_hda_device *pdev);
void snd_soc_hda_device_put(struct snd_soc_hda_device *pdev);

struct snd_soc_hda_driver {
	const struct snd_soc_hda_device_id *id_table;
	int	(*probe)(struct snd_soc_hda_device *hda);
	int	(*remove)(struct snd_soc_hda_device *hda);
	void	(*shutdown)(struct snd_soc_hda_device *hda);
	int	(*suspend)(struct snd_soc_hda_device *hda, pm_message_t mesg);
	int	(*resume)(struct snd_soc_hda_device *hda);
	void	(*unsol_event)(struct snd_soc_hda_device *hda ,
					unsigned int res);
	struct device_driver	driver;
};

#define to_soc_hda_driver(drv) (container_of((drv), struct snd_soc_hda_driver, \
				driver))

int snd_soc_hda_driver_register(struct snd_soc_hda_driver *);
void snd_soc_hda_driver_unregister(struct snd_soc_hda_driver *);

static inline void *snd_soc_hda_get_drvdata(
			const struct snd_soc_hda_device *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}

static inline void snd_soc_hda_set_drvdata(struct snd_soc_hda_device *pdev,
						void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}

#ifdef CONFIG_PM_SLEEP
int snd_soc_hda_pm_suspend(struct device *dev);
int snd_soc_hda_pm_resume(struct device *dev);
int snd_soc_hda_pm_freeze(struct device *dev);
int snd_soc_hda_pm_thaw(struct device *dev);
int snd_soc_hda_pm_poweroff(struct device *dev);
int snd_soc_hda_pm_restore(struct device *dev);
#else
#define snd_soc_hda_pm_suspend             NULL
#define snd_soc_hda_pm_resume              NULL
#define snd_soc_hda_pm_freeze              NULL
#define snd_soc_hda_pm_thaw                NULL
#define snd_soc_hda_pm_poweroff            NULL
#define snd_soc_hda_pm_restore             NULL
#endif

#ifdef CONFIG_PM_SLEEP
#define USE_HDA_PM_SLEEP_OPS \
	.suspend = snd_soc_hda_pm_suspend, \
	.resume = snd_soc_hda_pm_resume, \
	.freeze = snd_soc_hda_pm_freeze, \
	.thaw = snd_soc_hda_pm_thaw, \
	.poweroff = snd_soc_hda_pm_poweroff, \
	.restore = snd_soc_hda_pm_restore,
#else
#define USE_HDA_PM_SLEEP_OPS
#endif

struct snd_soc_hda_bus_ops {
	/* send a single command */
	int (*command)(struct hda_bus *bus, unsigned int cmd);
	/* get a response from the last command */
	unsigned int (*get_response)(struct hda_bus *bus, unsigned int addr);
	/* reset bus for retry verb */
	void (*bus_reset)(struct hda_bus *bus);
};

struct snd_soc_hda_bus {
	struct hda_bus bus;
	struct snd_soc_hda_bus_ops ops;
	/* unsolicited event queue */
	struct snd_soc_hda_bus_unsolicited *unsol;
};

/*
 * unsolicited event handler
 */

#define HDA_UNSOL_QUEUE_SIZE    64

struct snd_soc_hda_bus_unsolicited {
	struct hda_bus_unsolicited hda_bus_unsolicited;
	struct snd_soc_hda_bus *bus;
};

int snd_soc_codec_exec_verb(struct snd_soc_hda_device *dev, unsigned int cmd,
			   int flags, unsigned int *res);
const struct snd_pci_quirk *snd_soc_hda_quirk_lookup(
				struct snd_soc_hda_device *dev,
				const struct snd_pci_quirk *list);
int snd_soc_hda_bus_init(struct pci_dev *pci, void *data,
			struct snd_soc_hda_bus_ops ops,
			struct snd_soc_hda_bus **busp);
void snd_soc_hda_bus_release(void);

#endif /* _SOC_HDA_BUS_H_ */
