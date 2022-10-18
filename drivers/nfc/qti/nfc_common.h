/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2019-2021 NXP
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
#include "ese_cold_reset.h"

// Max device count for this driver
#define DEV_COUNT            1

// NFC device class
#define CLASS_NAME           "qti-nfc"

//  NFC character device name, this will be in /dev/
#define NFC_CHAR_DEV_NAME	 "nq-nci"

// NCI packet details
#define NCI_MSG_CMD                 0x20
#define NCI_MSG_RSP                 0x40
#define NCI_MSG_NTF                 0x60
#define DL_CMD			0x00
#define DL_PAYLOAD_BYTE_ZERO		0x00
#define NCI_HDR_LEN                 3
#define NCI_PAYLOAD_IDX             3
#define NCI_PAYLOAD_LEN_IDX         2

/*Time to wait for first NCI rest response*/
#define NCI_RESET_RESP_READ_DELAY  (10000) // 10ms
#define NCI_RESET_RESP_TIMEOUT     (500)  // 500ms

// FW DNLD packet details
#define FW_MSG_CMD_RSP              0x00
#define FW_HDR_LEN                  2
#define FW_PAYLOAD_LEN_IDX          1
#define FW_CRC_LEN                  2

#define NCI_RSP_PKT_TYPE		(0x40)
#define FW_MIN_PAYLOAD_LEN          4
#define MIN_NFC_DL_FRAME_SIZE       3

#define NCI_RESET_CMD_LEN			(4)
#define NCI_RESET_RSP_LEN			(4)
#define NCI_CORE_RESET_CMD_OID		(0x0)
#define NCI_CORE_RESET_CMD_PAYLOAD_LEN	(0x1)
#define NCI_CORE_RESET_KEEP_CONFIG	(0x0)
#define NCI_RESET_NTF_LEN			(13)
#define SN1XX_ROM_VER               0x01
#define SN1xx_MAJOR_VER             0x10
#define SN220_ROM_VER               0x01
#define SN220_MAJOR_VER             0x01
#define FW_ROM_CODE_VER_OFFSET      4
#define FW_MAJOR_VER_OFFSET         7
#define GET_VERSION_RSP_CHIP_TYPE_OFFSET	3
#define GET_VERSION_RSP_MINOR_VERSION_OFFSET	6
#define DL_GET_VERSION_CMD_LEN      (8)
#define DL_GET_VERSION_RSP_LEN_1    (12)	/* SN110 */
#define DL_GET_VERSION_RSP_LEN_2    (20)	/* SN220 */
#define DL_GET_VERSION_CMD_PAYLOAD_LEN      (4)
#define DL_GET_VERSION_CMD_ID		(0xF1)
#define DL_GET_VERSION_CMD_CRC_1	(0x6E)
#define DL_GET_VERSION_CMD_CRC_2	(0xEF)

#define DL_RESET_CMD_LEN                (8)
#define DL_GET_SESSION_STATE_CMD_LEN    (8)
#define DL_GET_SESSION_STATE_RSP_LEN    (8)
#define DL_GET_SESSION_STATE_CMD_PAYLOAD_LEN    (4)
#define DL_GET_SESSION_CMD_ID			(0xF2)
#define DL_GET_SESSION_CMD_CRC_1		(0xF5)
#define DL_GET_SESSION_CMD_CRC_2		(0x33)
#define GET_SESSION_STS_OFF             (3)
#define NFCC_SESSION_STS_CLOSED         (0x0)

/* Below offsets should be subtracted from NCI header length + payload length */

#define NFC_CHIP_TYPE_OFF		(4)
#define NFC_FW_MINOR_OFF		(1)

#define GET_VERSION_CMD_LEN			8
#define GET_SESSION_STATE_CMD_LEN			8
#define MAX_NCI_PAYLOAD_LEN		(255)
#define MAX_BUFFER_SIZE                 (NCI_HDR_LEN + MAX_NCI_PAYLOAD_LEN)
/*
 * From MW 11.04 buffer size increased to support
 * frame size of 554 in FW download mode
 * Frame len(2) + Frame Header(6) + DATA(512) + HASH(32) + CRC(2) + RFU(4)
 */
#define MAX_DL_PAYLOAD_LEN              (550)
#define MAX_DL_BUFFER_SIZE              (FW_HDR_LEN + FW_CRC_LEN + MAX_DL_PAYLOAD_LEN)
// Maximum retry count for standby writes
#define MAX_RETRY_COUNT			(3)

// Retry count for normal write
#define NO_RETRY				(1)
#define MAX_IRQ_WAIT_TIME		(90)
#define WAKEUP_SRC_TIMEOUT		(2000)

/*command response timeout*/
#define NCI_CMD_RSP_TIMEOUT             (2000)	//2s
/*Time to wait for NFCC to be ready again after any change in the GPIO*/
#define NFC_GPIO_SET_WAIT_TIME_USEC     (10000)
/*Time to wait after soft reset via any NCI/DL cmd*/
#define NFC_SOFT_RESET_WAIT_TIME_USEC   (5000)
/*Time to wait before retrying i2c writes*/
#define WRITE_RETRY_WAIT_TIME_USEC      (1000)
/*Time to wait before retrying read for some specific usecases*/
#define READ_RETRY_WAIT_TIME_USEC       (3500)
#define NFC_MAGIC 0xE9

// Ioctls
// The type should be aligned with MW HAL definitions

#define NFC_SET_PWR		_IOW(NFC_MAGIC, 0x01, unsigned int)
#define ESE_SET_PWR		_IOW(NFC_MAGIC, 0x02, unsigned int)
#define ESE_GET_PWR		_IOR(NFC_MAGIC, 0x03, unsigned int)
#define NFC_GET_PLATFORM_TYPE	_IO(NFC_MAGIC, 0x04)

/* NFC HAL can call this ioctl to get the current IRQ state */
#define NFC_GET_IRQ_STATE      _IO(NFC_MAGIC, 0x06)

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
};

/*nfc platform interface type*/
enum interface_flags {
	/*I2C physical IF for NFCC */
	PLATFORM_IF_I2C = 0,
};

/*nfc state flags*/
enum nfc_state_flags {
	/*nfc in unknown state */
	NFC_STATE_UNKNOWN = 0,
	/*nfc in download mode */
	NFC_STATE_FW_DWL = 0x1,
	/*nfc booted in NCI mode */
	NFC_STATE_NCI = 0x2,
	/*nfc booted in Fw teared mode */
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

/* Enum for GPIO values*/
enum gpio_values {
	GPIO_INPUT = 0x0,
	GPIO_OUTPUT = 0x1,
	GPIO_HIGH = 0x2,
	GPIO_OUTPUT_HIGH = 0x3,
	GPIO_IRQ = 0x4,
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

// NFC Struct to get all the required configs from DTS
struct platform_configs {
	struct platform_gpio gpio;
	struct platform_ldo ldo;
};

enum chip_types {
	CHIP_SN1XX = 0x01,
	CHIP_SN220 = 0x02,
	CHIP_UNKNOWN = 0xFF,
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
	/* current firmware major version */
	uint8_t fw_major_version;
	bool is_vreg_enabled;
	bool is_ese_session_active;
	struct i2c_dev i2c_dev;
	struct platform_configs configs;
	struct cold_reset cold_reset;
	struct regulator *reg;

	/* read buffer*/
	size_t kbuflen;
	u8 *kbuf;

	union nqx_uinfo nqx_info;

	void *ipcl;

	int (*nfc_read)(struct nfc_dev *dev, char *buf, size_t count, int timeout);
	int (*nfc_write)(struct nfc_dev *dev, const char *buf, const size_t count,
				int max_retry_cnt);
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
		const struct file_operations *nfc_fops, int count, char *devname,
		char *classname);
void nfc_misc_unregister(struct nfc_dev *nfc_dev, int count);
int configure_gpio(unsigned int gpio, int flag);
void gpio_set_ven(struct nfc_dev *nfc_dev, int value);
int nfcc_hw_check(struct nfc_dev *nfc_dev);
int nfc_ldo_config(struct device *dev, struct nfc_dev *nfc_dev);
int nfc_ldo_vote(struct nfc_dev *nfc_dev);
int nfc_ldo_unvote(struct nfc_dev *nfc_dev);
int is_nfc_data_available_for_read(struct nfc_dev *nfc_dev);
int validate_nfc_state_nci(struct nfc_dev *nfc_dev);
void set_nfcc_state_from_rsp(struct nfc_dev *dev, const char *buf,
				const int count);
void enable_dwnld_mode(struct nfc_dev *nfc_dev, bool value);
#endif //_NFC_COMMON_H_
