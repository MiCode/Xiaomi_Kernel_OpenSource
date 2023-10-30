
#ifndef _NTAG_I2C_DRV_H_
#define _NTAG_I2C_DRV_H_

#include <linux/i2c.h>

#define NTAG5_I2C_DRV_STR   "nxp,ntag5"	/*kept same as dts */

struct ntag_dev;

//Interface specific parameters
struct i2c_dev {
	struct i2c_client *client;

	/*IRQ parameters */
	bool irq_enabled;

	spinlock_t irq_enabled_lock;
	/* NFC_IRQ wake-up state */
	bool irq_wake_up;
};

int nfc_i2c_dev_probe(struct i2c_client *client,const struct i2c_device_id *id);
int nfc_i2c_dev_remove(struct i2c_client *client);

#if IS_ENABLED(CONFIG_NTAG5_I2C)

int i2c_enable_irq(void);
int i2c_disable_irq(void);
int i2c_read(struct ntag_dev *dev, char *buf, size_t count, int timeout);

#else

static inline int i2c_enable_irq(struct ntag_dev *dev)
{
	return -ENXIO;
}

static inline int i2c_disable_irq(struct ntag_dev *dev)
{
	return -ENXIO;
}

static inline int i2c_read(struct ntag_dev *dev, char *buf, size_t count, int timeout)
{
	return -ENXIO;
}

#endif

#endif //_NFC_I2C_DRV_H_
