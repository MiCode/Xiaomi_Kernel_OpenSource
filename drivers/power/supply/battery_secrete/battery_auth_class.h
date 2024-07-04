#ifndef __BATT_AUTH_CLASS__
#define __BATT_AUTH_CLASS__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>

struct auth_device {
	const char *name;
	const struct auth_ops *ops;
	raw_spinlock_t io_lock;

	struct device dev;

	void *drv_data;

	struct gpio_desc *gpiod;
};

#define to_auth_device(obj) container_of(obj, struct auth_device, dev)

struct auth_ops {
	int (*auth_battery) (struct auth_device * auth_dev);
	int (*get_battery_id) (struct auth_device * auth_dev, u8 * id);
};

int auth_device_start_auth(struct auth_device *auth_dev);
int auth_device_get_batt_id(struct auth_device *auth_dev, u8 * id);
struct auth_device *auth_device_register(const char *name,
					 struct device *parent,
					 void *devdata,
					 const struct auth_ops *ops);
void auth_device_unregister(struct auth_device *auth_dev);
struct auth_device *get_batt_auth_by_name(const char *name);
#endif				/* __BATT_AUTH_CLASS__ */