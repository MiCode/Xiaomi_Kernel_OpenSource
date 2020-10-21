/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _NFC_COMMON_H_
#define _NFC_COMMON_H_

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
#include "nfc_i2c_drv.h"
#include "nfc_i3c_drv.h"

// Max device count for this driver
#define DEV_COUNT            1

// NFC device class
#define CLASS_NAME           "nfc"

//  NFC character device name, this will be in /dev/
#define NFC_CHAR_DEV_NAME	 "nq-nci"

// HDR length of NCI packet
#define NCI_HDR_LEN				3
#define NCI_PAYLOAD_IDX			3
#define NCI_PAYLOAD_LEN_IDX		2

#define NCI_RESET_CMD_LEN			(4)
#define NCI_RESET_RSP_LEN			(4)
#define NCI_RESET_NTF_LEN			(13)
#define NCI_GET_VERSION_CMD_LEN		(8)
#define NCI_GET_VERSION_RSP_LEN		(12)

// Below offsets should be subtracted from core reset ntf len

#define NFC_CHIP_TYPE_OFF		(3)
#define NFC_ROM_VERSION_OFF		(2)
#define NFC_FW_MAJOR_OFF		(1)

#define COLD_RESET_CMD_LEN		3
#define COLD_RESET_RSP_LEN		4
#define COLD_RESET_CMD_GID		0x2F
#define COLD_RESET_CMD_PAYLOAD_LEN	0x00
#define COLD_RESET_RSP_GID		0x4F
#define COLD_RESET_OID			0x1E

#define MAX_NCI_PAYLOAD_LEN		(255)
/*
 * From MW 11.04 buffer size increased to support
 * frame size of 554 in FW download mode
 * Frame len(2) + Frame Header(6) + DATA(512) + HASH(32) + CRC(2) + RFU(4)
 */
#define MAX_BUFFER_SIZE			(558)

// Maximum retry count for standby writes
#define MAX_RETRY_COUNT			(3)

// Retry count for normal write
#define NO_RETRY				(1)
#define MAX_IRQ_WAIT_TIME		(90)
#define WAKEUP_SRC_TIMEOUT		(2000)

#define NFC_MAGIC 0xE9

// Ioctls
// The type should be aligned with MW HAL definitions

#define NFC_SET_PWR		_IOW(NFC_MAGIC, 0x01, unsigned int)
#define ESE_SET_PWR		_IOW(NFC_MAGIC, 0x02, unsigned int)
#define ESE_GET_PWR		_IOR(NFC_MAGIC, 0x03, unsigned int)
#define NFC_GET_PLATFORM_TYPE	_IO(NFC_MAGIC, 0x04)

#define DTS_IRQ_GPIO_STR	"qcom,sn-irq"
#define DTS_VEN_GPIO_STR	"qcom,sn-ven"
#define DTS_FWDN_GPIO_STR	"qcom,sn-firm"
#define DTS_CLKREQ_GPIO_STR	"qcom,sn-clkreq"
#define DTS_CLKSRC_GPIO_STR	"qcom,clk-src"
#define NFC_LDO_SUPPLY_DT_NAME	"qcom,sn-vdd-1p8"
#define NFC_LDO_SUPPLY_NAME	"qcom,sn-vdd-1p8-supply"
#define NFC_LDO_VOL_DT_NAME	"qcom,sn-vdd-1p8-voltage"
#define NFC_LDO_CUR_DT_NAME	"qcom,sn-vdd-1p8-current"

//as per SN1x0 datasheet
#define NFC_VDDIO_MIN		1650000 //in uV
#define NFC_VDDIO_MAX		1950000 //in uV
#define NFC_CURRENT_MAX		157000 //in uA


#define NUM_OF_IPC_LOG_PAGES	(2)
#define PKT_MAX_LEN		(4) // no of max bytes to print for cmd/resp

#define GET_IPCLOG_MAX_PKT_LEN(c)	((c > PKT_MAX_LEN) ? PKT_MAX_LEN : c)

#define NFCLOG_IPC(nfc_dev, log_to_dmesg, x...)	\
do { \
	ipc_log_string(nfc_dev->ipcl, x); \
	if (log_to_dmesg) { \
		if (nfc_dev->nfc_device) \
			dev_err((nfc_dev->nfc_device), x); \
		else \
			pr_err(x); \
	} \
} while (0)

enum ese_ioctl_request {
	/* eSE POWER ON */
	ESE_POWER_ON = 0,
	/* eSE POWER OFF */
	ESE_POWER_OFF,
	/* eSE COLD RESET */
	ESE_COLD_RESET,
	/* eSE POWER STATE */
	ESE_POWER_STATE
};

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
	/* NFC enable without VEN gpio modification */
	NFC_ENABLE,
	/* NFC disable without VEN gpio modification */
	NFC_DISABLE,
	/*for HDR size change in FW mode */
	NFC_FW_HDR_LEN,
};

/*nfc platform interface type*/
enum interface_flags {
	/*I2C physical IF for NFCC */
	PLATFORM_IF_I2C = 0,
	/*I3C physical IF for NFCC */
	PLATFORM_IF_I3C,
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

/* Enum for GPIO values*/
enum gpio_values {
	GPIO_INPUT = 0x0,
	GPIO_OUTPUT = 0x1,
	GPIO_HIGH = 0x2,
	GPIO_OUTPUT_HIGH = 0x3,
	GPIO_IRQ = 0x4,
};

enum nfcc_chip_variant {
	NFCC_SN100_A = 0xa3,	    /**< NFCC SN100_A */
	NFCC_SN100_B = 0xa4,	    /**< NFCC SN100_B */
	NFCC_NOT_SUPPORTED = 0xFF		/**< NFCC is not supported */
};

// NFC GPIO variables
struct platform_gpio {
	unsigned int irq;
	unsigned int ven;
	unsigned int clkreq;
	unsigned int dwl_req;
};

// NFC LDO entries from DT
struct platform_ldo {
	int vdd_levels[2];
	int max_current;
};

//Features specific Parameters
struct cold_reset {
	wait_queue_head_t read_wq;
	bool rsp_pending;
	uint8_t status;
	/* Is NFC enabled from UI */
	bool is_nfc_enabled;
};

/* Device specific structure */
struct nfc_dev {
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct mutex dev_ref_mutex;
	unsigned int dev_ref_count;
	struct class *nfc_class;
	struct device *nfc_device;
	struct cdev c_dev;
	dev_t devno;
	/* Interface flag */
	uint8_t interface;
	/* NFC VEN pin state */
	bool nfc_ven_enabled;
	bool is_vreg_enabled;
	bool is_ese_session_active;
	union {
		struct i2c_dev i2c_dev;
		struct i3c_dev i3c_dev;
	};
	struct platform_gpio gpio;
	struct platform_ldo ldo;
	struct cold_reset cold_reset;
	struct regulator *reg;

	/* read buffer*/
	size_t kbuflen;
	u8 *kbuf;

	union nqx_uinfo nqx_info;

	void *ipcl;

	int (*nfc_read)(struct nfc_dev *dev,
					char *buf, size_t count);
	int (*nfc_write)(struct nfc_dev *dev,
			const char *buf, const size_t count, int max_retry_cnt);
	int (*nfc_enable_intr)(struct nfc_dev *dev);
	int (*nfc_disable_intr)(struct nfc_dev *dev);
};

int nfc_dev_open(struct inode *inode, struct file *filp);
int nfc_dev_close(struct inode *inode, struct file *filp);
long nfc_dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg);
int nfc_parse_dt(struct device *dev, struct platform_gpio *nfc_gpio,
		 struct platform_ldo *ldo, uint8_t interface);
int nfc_misc_probe(struct nfc_dev *nfc_dev,
		      const struct file_operations *nfc_fops, int count,
		      char *devname, char *classname);
void nfc_misc_remove(struct nfc_dev *nfc_dev, int count);
int configure_gpio(unsigned int gpio, int flag);
void read_cold_reset_rsp(struct nfc_dev *nfc_dev, char *header);
void gpio_set_ven(struct nfc_dev *nfc_dev, int value);
int nfcc_hw_check(struct nfc_dev *nfc_dev);
int nfc_ldo_config(struct device *dev, struct nfc_dev *nfc_dev);
int nfc_ldo_vote(struct nfc_dev *nfc_dev);
int nfc_ldo_unvote(struct nfc_dev *nfc_dev);

#endif //_NFC_COMMON_H_
