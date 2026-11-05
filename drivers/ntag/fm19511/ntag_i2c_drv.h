/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2028, The Linux Foundation. All rights reserved.
 */

#ifndef _NTAG_I2C_DRV_H_
#define _NTAG_I2C_DRV_H_
#include <linux/i2c.h>

#define NTAG_I2C_DRV_STR	"fm,fm19511"	/*kept same as dts */

struct ntag_dev;

//Interface specific parameters
struct i2c_dev {
	struct i2c_client *client;
	/*IRQ parameters */
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
	/* NTAG_IRQ wake-up state */
	bool irq_wake_up;
};
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
int ntag_i2c_dev_probe(struct i2c_client *client);
#else
int ntag_i2c_dev_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
void ntag_i2c_dev_remove(struct i2c_client *client);
#else
int ntag_i2c_dev_remove(struct i2c_client *client);
#endif
int ntag_i2c_dev_suspend(struct device *device);
int ntag_i2c_dev_resume(struct device *device);

int i2c_enable_irq(struct ntag_dev *dev);
int i2c_disable_irq(struct ntag_dev *dev);
int i2c_read(struct ntag_dev *dev, char *buf, size_t count);
int i2c_write(struct ntag_dev *dev, const char *buf, size_t count);


#endif //_NTAG_I2C_DRV_H_
