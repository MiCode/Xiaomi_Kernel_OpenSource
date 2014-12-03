/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __NFC_NCI_H
#define __NFC_NCI_H

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/version.h>

#include <linux/semaphore.h>
#include <linux/completion.h>

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>

struct nfc_device {
	struct cdev cdev;
	struct class *char_class;
};

enum ehandler_mode {
	UNSOLICITED_MODE = 0,
	SOLICITED_MODE,
	UNSOLICITED_FTM_RAW_MODE,
	SOLICITED_FTM_RAW_MODE
};

enum ekernel_logging_mode {
	LEVEL_0 = 0,	/* For Basic Comms, such asNCI TX/TX to NFCC */
	LEVEL_1,	/* Other Debug e.g. Notifications, ISR hit, etc ..*/
	LEVEL_2,
	LEVEL_3,
	LEVEL_4,
	LEVEL_5
};

struct devicemode {
	enum ehandler_mode	handle_flavour;
} tdevicemode;

#define NFC_DRIVER_NAME		"nfc-nci"
#define NFC_I2C_DRIVER_NAME		"NCI NFC I2C Interface",

#define NCI_I2C_SLAVE	(0x2C)
#define NFC_I2C_BUS	3	/* 6, 10, 4, 5 */
#define NFC_SET_PWR	_IOW(0xE9, 0x01, unsigned int)
#define NFCC_MODE	_IOW(0xE9, 0x02, unsigned int)
#define NFC_KERNEL_LOGGING_MODE		_IOW(0xE9, 0x03, unsigned int)
#define SET_RX_BLOCK	_IOW(0xE9, 0x04, unsigned int)
#define SET_EMULATOR_TEST_POINT		_IOW(0xE9, 0x05, unsigned int)
#define NFCC_VERSION				_IOW(0xE9, 0x08, unsigned int)
#define NFC_GET_EFUSE				_IOW(0xE9, 0x09, unsigned int)
#define NFCC_INITIAL_CORE_RESET_NTF		_IOW(0xE9, 0x10, unsigned int)

#define NFC_MAX_I2C_TRANSFER	(0x0400)
#define NFC_MSG_MAX_SIZE	(0x21)

#define NFC_RX_BUFFER_CNT_START		(0x0)

#define NFC_RX_BUFFER_BLOCK_SIZE	(0x120)	/* Bytes per Block */
#define NFC_RX_BUFFER_PAGE_SIZE		(0x1000)	/* Page size Bytes */
#define NFC_RX_BUFFER_PAGES		(0x8)
#define NFC_RX_ORDER_FREE_PAGES	(0x3)	/* Free 8 Pages */

/* The total no. of Blocks */
#define NFC_RX_BUFFER_CNT_LIMIT		(unsigned short)(	\
						(		\
						((NFC_RX_BUFFER_PAGE_SIZE) *\
						(NFC_RX_BUFFER_PAGES))/\
						(NFC_RX_BUFFER_BLOCK_SIZE)\
						)		\
						)		\

#define PAYLOAD_HEADER_LENGTH		(0x3)
#define PAYLOAD_LENGTH_MAX		(256)
#define BYTE				(0x8)
#define NCI_IDENTIFIER			(0x10)

/** Power Management Related **/

#define NFCC_WAKE				(0x01)
#define NFCC_SLEEP				(0x00)

#define XTAL_CLOCK				(0X00)
#define REFERENCE_CLOCK			(0X01)

/* LDO Trim Settings */
#define IPTAT_TRIM			(0x1F)
#define V1P1_TRIM			(0x0F)
#define V1P8_TRIM			(0x0F)
#define VBATT_OK_THRESHOLD		(0x07)

#define PWR_EN		(0x08)		/* Enable 1.1V LDO Regulator */
#define LS_EN		(0x04)		/* Enable 1.1V->1.8V Level Shifters */

/* Write '1' to cause wake event to NFCC. If set NFCC will not go to SLEEP */
#define NCI_WAKE	(0x02)

#define NCI_ENA		(0x01)		/* Write '1' to enable PLL */
#define FREQ_SEL	(0x00)		/* XO Frequency Select */
#define FREQ_SEL_13	(0x00)		/* XO Frequency Select = 13.56MHz */
#define FREQ_SEL_19	(0x01)		/* XO Frequency Select = 19.20 MHz */
#define FREQ_SEL_26	(0x02)		/* XO Frequency Select = 26.00 MHz */
#define FREQ_SEL_27	(0x03)		/* XO Frequency Select = 27.12 MHz */
#define FREQ_SEL_37	(0x04)		/* XO Frequency Select = 37.40 MHz */
#define FREQ_SEL_38	(0x05)		/* XO Frequency Select = 38.40 MHz */
#define FREQ_SEL_40	(0x06)		/* XO Frequency Select = 40.00 MHz */
#define FREQ_SEL_48	(0x07)		/* XO Frequency Select = 48.00 MHz */
#define FREQ_SEL_27	(0x03)		/* XO Frequency Select */


#define QUALIFY_REFCLK	(0x80)
#define QUALIFY_OSC	(0x40)
#define LOCALBIASXTAL	(0x20)
#define BIAS2X_FORCE	(0x10)
#define BIAS2X		(0x08)
#define LBIAS2X	(0x04)
#define SMALLRF	(0x02)
#define SMALLRBIAS	(0x01)

/* Select as appropriate */
#define CRYSTAL_OSC	((QUALIFY_REFCLK) | (QUALIFY_OSC) |	\
			(LOCALBIASXTAL) | (BIAS2X_FORCE) |	\
			(BIAS2X) | (LBIAS2X) | (SMALLRF) | (SMALLRBIAS))

#define CDACIN		(0x3F)	/* Tuning range for load capacitor at X1*/
#define CDACOUT		(0x3F)	/* Tuning range for load capacitor at X2*/

#define RAW(reg, value)		(raw_##reg[1] = value)

/* Logging macro with threshold control */
#define PRINTK(LEVEL, THRESHOLD, pString, ...)		(	\
		if (LEVEL > THRESHOLD) {			\
			pr_info(pString, ##__VA_ARGS__);		\
		}						\
							)

/* board config */
struct nfc_platform_data {
	int (*request_resources) (struct i2c_client *client);
	void (*free_resources) (void);
	void (*enable) (int fw);
	int (*test) (void);
	void (*disable) (void);
};
/*
 * Internal NFCC Hardware states. At present these may not be possible to
 * detect in software as possibly no power when
 * in monitor state! Also, need to detect DISABLE control GPIO from PMIC.
 */
enum nfcc_hardware_state {
	NFCC_STATE_MONITOR,	/* VBAT < h/w Critcal Voltage */
	/* VBAT > H/W Critical Voltage;
	Lowest Power Mode - DISABLE = 1; only
	possible when phone is ON */
	NFCC_STATE_HPD,
	/* VBAT > H/W Critical Voltage; DISABLE = 0;
	Only possible when phone is ON */
	NFCC_STSTE_ULPM,
	/* VBAT > H/W Critical Voltage; DISABLE = 0;
	Powered by PMIC & VBAT; 1.8V I/O supply on; VDDPX available, boot is
	initiated by host over I2C */
	NFCC_STATE_NORMAL_REGION1,
	/* VBAT > H/W Critical Voltage; DISABLE = 0;
	Powered by VBAT; 1.8V I/O supply on; VDDPX available, boot is initiated
	by host over I2C */
	NFCC_STATE_NORMAL_REGION2,
};

/* We assume here that VBATT > h/w Critical Voltage */
enum nfcc_state {
	/* Assume In ULPM state, ready for initialisation, cannot detect for
	Monitor or HPD states */
	NFCC_STATE_COLD,
	/* (VDDPX==1) && (Following I2C initialisation). In Region 1 or Region2
	state WAKE */
	NFCC_STATE_NORMAL_WAKE,
	/* (VDDPX==1) && (Following I2C initialisation). In Region 1 or Region2
	state SLEEP */
	NFCC_STATE_NORMAL_SLEEP,
};


enum nfcc_irq {
	NFCC_NO_INT,
	NFCC_INT,
};

enum nfcc_initial_core_reset_ntf {
	TIMEDOUT_INITIAL_CORE_RESET_NTF = 0, /* 0*/
	ARRIVED_INITIAL_CORE_RESET_NTF, /* 1 */
	DEFAULT_INITIAL_CORE_RESET_NTF, /*2*/
};

struct nfc_info {
	struct	miscdevice			miscdev;
	struct	i2c_client			*i2c_dev;
	struct	regulator_bulk_data		regs[3];
	enum	nfcc_state			state;
	wait_queue_head_t			read_wait;
	loff_t					read_offset;
	struct	mutex				read_mutex;
	struct	mutex				mutex;
	u8					*buf;
	size_t					buflen;
	spinlock_t				irq_enabled_lock;
	unsigned int				count_irq;
	enum	nfcc_irq			read_irq;
};


struct nfc_i2c_platform_data {
	unsigned int	nfc_irq_gpio;
	unsigned int	nfc_clk_en_gpio;
	unsigned int	dis_gpio;
	unsigned int	irq_gpio;
	unsigned int	ven_gpio;
	unsigned int	firm_gpio;
	unsigned int	reg;
};
#endif
