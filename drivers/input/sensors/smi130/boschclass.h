/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * (C) Modification Copyright 2018 Robert Bosch Kft  All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename boschcalss.h
 * @date     2015/11/17 13:44
 * @Modification Date 2018/06/21 15:03
 * @id       "836294d"
 * @version  1.5.9
 *
 * @brief  
 */

#ifndef _BSTCLASS_H
#define _BSTCLASS_H

#ifdef __KERNEL__
#include <linux/time.h>
#include <linux/list.h>
#else
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/types.h>
#endif

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mod_devicetable.h>

struct bosch_dev {
	const char *name;

	int (*open)(struct bosch_dev *dev);
	void (*close)(struct bosch_dev *dev);
	struct mutex mutex;
	struct device dev;
	struct list_head node;
};

#define to_bosch_dev(d) container_of(d, struct bosch_dev, dev)

struct bosch_dev *bosch_allocate_device(void);
void bosch_free_device(struct bosch_dev *dev);

static inline struct bosch_dev *bosch_get_device(struct bosch_dev *dev)
{
	return dev ? to_bosch_dev(get_device(&dev->dev)) : NULL;
}

static inline void bosch_put_device(struct bosch_dev *dev)
{
	if (dev)
		put_device(&dev->dev);
}

static inline void *bosch_get_drvdata(struct bosch_dev *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void bosch_set_drvdata(struct bosch_dev *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

int __must_check bosch_register_device(struct bosch_dev *);
void bosch_unregister_device(struct bosch_dev *);

void bosch_reset_device(struct bosch_dev *);


extern struct class bosch_class;

#endif
