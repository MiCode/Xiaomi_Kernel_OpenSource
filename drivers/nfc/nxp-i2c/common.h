/******************************************************************************
 * Copyright (C) 2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019-2021 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/
#ifndef _COMMON_H_
#define _COMMON_H_

#include <linux/cdev.h>

#include "i2c_drv.h"

/* Max device count for this driver */
#define DEV_COUNT			1
/* i2c device class */
#define CLASS_NAME			"nqnfc"

/* NFC character device name, this will be in /dev/ */
#define NFC_CHAR_DEV_NAME		"nq-nci"

/* NCI packet details */
#define NCI_CMD				(0x20)
#define NCI_RSP				(0x40)
#define NCI_HDR_LEN			(3)
#define NCI_HDR_IDX			(0)
#define NCI_HDR_OID_IDX			(1)
#define NCI_PAYLOAD_IDX			(3)
#define NCI_PAYLOAD_LEN_IDX		(2)

/* FW DNLD packet details */
#define DL_HDR_LEN			(2)
#define DL_CRC_LEN			(2)

#define MAX_NCI_PAYLOAD_LEN		(255)
#define MAX_NCI_BUFFER_SIZE		(NCI_HDR_LEN + MAX_NCI_PAYLOAD_LEN)
#define MAX_DL_PAYLOAD_LEN		(550)
#define MAX_DL_BUFFER_SIZE		(DL_HDR_LEN + DL_CRC_LEN + \
					MAX_DL_PAYLOAD_LEN)


/* Retry count for normal write */
#define NO_RETRY			(1)
/* Maximum retry count for standby writes */
#define MAX_RETRY_COUNT			(3)
#define MAX_WRITE_IRQ_COUNT		(5)
#define MAX_IRQ_WAIT_TIME		(90)
#define WAKEUP_SRC_TIMEOUT		(2000)

/* command response timeout */
#define NCI_CMD_RSP_TIMEOUT_MS		(2000)
/* Time to wait for NFCC to be ready again after any change in the GPIO */
#define NFC_GPIO_SET_WAIT_TIME_US	(10000)
/* Time to wait for IRQ low during write 5*3ms */
#define NFC_WRITE_IRQ_WAIT_TIME_US	(3000)
/* Time to wait before retrying i2c/I3C writes */
#define WRITE_RETRY_WAIT_TIME_US	(1000)
/* Time to wait before retrying read for some specific usecases */
#define READ_RETRY_WAIT_TIME_US		(3500)
#define NFC_MAGIC			(0xE9)

/* Ioctls */
/* The type should be aligned with MW HAL definitions */
#define NFC_SET_PWR			_IOW(NFC_MAGIC, 0x01, unsigned int)
#define ESE_SET_PWR			_IOW(NFC_MAGIC, 0x02, unsigned int)
#define ESE_GET_PWR			_IOR(NFC_MAGIC, 0x03, unsigned int)

#define DTS_IRQ_GPIO_STR		"qcom,nq-irq"
#define DTS_VEN_GPIO_STR		"qcom,nq-ven"
#define DTS_FWDN_GPIO_STR		"qcom,nq-firm"

enum nfcc_ioctl_request {
	/* NFC disable request with VEN LOW */
	NFC_POWER_OFF = 0,
	/* NFC enable request with VEN Toggle */
	NFC_POWER_ON,
	/* firmware download request with VEN Toggle */
	NFC_FW_DWL_VEN_TOGGLE,
	/* ISO reset request */
	NFC_ISO_RESET,
	/* request for firmware download gpio HIGH */
	NFC_FW_DWL_HIGH,
	/* VEN hard reset request */
	NFC_VEN_FORCED_HARD_RESET,
	/* request for firmware download gpio LOW */
	NFC_FW_DWL_LOW,
};

/* nfc platform interface type */
enum interface_flags {
	/* I2C physical IF for NFCC */
	PLATFORM_IF_I2C = 0,
};

/* nfc state flags */
enum nfc_state_flags {
	/* nfc in unknown state */
	NFC_STATE_UNKNOWN = 0,
	/* nfc in download mode */
	NFC_STATE_FW_DWL = 0x1,
	/* nfc booted in NCI mode */
	NFC_STATE_NCI = 0x2,
	/* nfc booted in Fw teared mode */
	NFC_STATE_FW_TEARED = 0x4,
};
/*
 * Power state for IBI handing, mainly needed to defer the IBI handling
 *  for the IBI received in suspend state to do it later in resume call
 */
enum pm_state_flags {
	PM_STATE_NORMAL = 0,
	PM_STATE_SUSPEND,
	PM_STATE_IBI_BEFORE_RESUME,
};

/* Enum for GPIO values */
enum gpio_values {
	GPIO_INPUT = 0x0,
	GPIO_OUTPUT = 0x1,
	GPIO_HIGH = 0x2,
	GPIO_OUTPUT_HIGH = 0x3,
	GPIO_IRQ = 0x4,
};

/* NFC GPIO variables */
struct platform_gpio {
	unsigned int irq;
	unsigned int ven;
	unsigned int dwl_req;
};

/* NFC Struct to get all the required configs from DTS */
struct platform_configs {
	struct platform_gpio gpio;
};

/* cold reset Features specific Parameters */
struct cold_reset {
	bool rsp_pending;	/* cmd rsp pending status */
	bool in_progress;	/* for cold reset when gurad timer in progress */
	bool reset_protection;	/* reset protection enabled/disabled */
	uint8_t status;		/* status from response buffer */
	uint8_t rst_prot_src;	/* reset protection source (SPI, NFC) */
	struct timer_list timer;
	wait_queue_head_t read_wq;
};

/* Device specific structure */
struct nfc_dev {
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct mutex write_mutex;
	uint8_t *read_kbuf;
	uint8_t *write_kbuf;
	struct mutex dev_ref_mutex;
	unsigned int dev_ref_count;
	struct class *nfc_class;
	struct device *nfc_device;
	struct cdev c_dev;
	dev_t devno;
	/* Interface flag */
	uint8_t interface;
	/* nfc state flags */
	uint8_t nfc_state;
	/* NFC VEN pin state */
	bool nfc_ven_enabled;
	bool release_read;
	union {
		struct i2c_dev i2c_dev;
	};
	struct platform_configs configs;
	struct cold_reset cold_reset;

	/* function pointers for the common i2c functionality */
	int (*nfc_read)(struct nfc_dev *dev, char *buf, size_t count,
			int timeout);
	int (*nfc_write)(struct nfc_dev *dev, const char *buf,
			 const size_t count, int max_retry_cnt);
	int (*nfc_enable_intr)(struct nfc_dev *dev);
	int (*nfc_disable_intr)(struct nfc_dev *dev);
};

int nfc_dev_open(struct inode *inode, struct file *filp);
int nfc_dev_flush(struct file *pfile, fl_owner_t id);
int nfc_dev_close(struct inode *inode, struct file *filp);
long nfc_dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg);
int nfc_parse_dt(struct device *dev, struct platform_configs *nfc_configs,
		 uint8_t interface);
int nfc_misc_register(struct nfc_dev *nfc_dev,
		      const struct file_operations *nfc_fops, int count,
		      char *devname, char *classname);
void nfc_misc_unregister(struct nfc_dev *nfc_dev, int count);
int configure_gpio(unsigned int gpio, int flag);
void gpio_set_ven(struct nfc_dev *nfc_dev, int value);
void gpio_free_all(struct nfc_dev *nfc_dev);
int validate_nfc_state_nci(struct nfc_dev *nfc_dev);
#endif /* _COMMON_H_ */
