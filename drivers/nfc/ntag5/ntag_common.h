
#ifndef _NTAG_COMMON_H_
#define _NTAG_COMMON_H_


#include <linux/types.h>
#include <linux/version.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/nfcinfo.h>
#include <linux/regulator/consumer.h>
#include <linux/ipc_logging.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include "ntag_i2c_drv.h"

// Max device count for this driver
#define DEV_COUNT            1

// NFC device class
#define CLASS_NAME           "ntag5-nfc"

//  NFC character device name, this will be in /dev/
#define NFC_CHAR_DEV_NAME	 "ntag5-nci"

#define NTAG5_I2C_DRV_STR   "nxp,ntag5"	/*kept same as dts */

#define DTS_IRQ_GPIO_STR	"qcom,ntag-pu"
#define DTS_HPD_GPIO_STR	"qcom,ntag-hpd"

#define MAX_BUFFER_SIZE  (255)
#define MAX_DL_BUFFER_SIZE (560)

#define NTAG5_BLOCK_SIZE          4
#define STATUS_BUFFER_SIZE_NOT_SUPPORT    247		// BUF溢出错误

#define NFCLOG_IPC(nfc_dev, log_to_dmesg, x...)	\
do { \
	if (log_to_dmesg) { \
		if (nfc_dev->nfc_device) \
			dev_err((nfc_dev->nfc_device), x); \
		else \
			pr_err(x); \
	} \
} while (0)

/*nfc state flags*/
enum nfc_state_flags {

	/**NTAG5 default state*/
	NTAG5_DEFAULT_STATE = 0,

	/**NTAG5 PU/ED  event detection state */
	NTAG5_PU_STATE = 0x1,

	/**NTAG5 hard power down state*/
	NTAG5_HPD_STATE = 0x2,

};

/* Enum for GPIO values*/
enum gpio_values {
	GPIO_INPUT = 0x0,
	GPIO_OUTPUT = 0x1,
	GPIO_OUTPUT_HIGH = 0x3,
	GPIO_OUTPUT_LOW = 0x5,
	GPIO_IRQ = 0x6,
	GPIO_HPD = 0x7,
};

// NFC GPIO variables
struct platform_gpio {
	unsigned int pu;
	unsigned int hpd;
};

// NFC Struct to get all the required configs from DTS
struct platform_configs {
	struct platform_gpio gpio;
};

/* Device specific structure */
struct ntag_dev {

	struct i2c_dev i2c_dev;

	struct platform_configs configs;

	struct cdev c_dev;

	dev_t devno;
	wait_queue_head_t read_wq;

	uint8_t nfc_state;

	struct pinctrl              *ntag5_pinctrl;
	struct pinctrl_state        *ntag5_pu_default;
	struct pinctrl_state        *ntag5_pu_suspend;
	struct pinctrl_state        *ntag5_hpd_default;
	struct pinctrl_state        *ntag5_hpd_suspend;

	struct class *nfc_class;
	struct device *nfc_device;

	struct fasync_struct *fasync_queue; //异步通知队列

	int (*nfc_read)(struct ntag_dev *dev, char *buf, size_t count, int timeout);

	int (*nfc_enable_intr)(void);
	int (*nfc_disable_intr)(void);
};

int nfc_misc_register(struct ntag_dev *ntag_dev,
		const struct file_operations *nfc_fops, int count, char *devname,
		char *classname);

void nfc_misc_unregister(struct ntag_dev *ntag_dev, int count);

int configure_gpio(unsigned int gpio, int flag);
int nfc_parse_dt(struct device *dev, struct platform_configs *nfc_configs);
int ntag5_gpio_init(struct ntag_dev *ntag_dev);

#endif
