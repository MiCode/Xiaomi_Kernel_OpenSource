/* Goodix's GF316M/GF318M/GF3118M/GF518M/GF5118M/GF516M/GF816M/GF3208/GF5216
 *  fingerprint sensor linux driver for TEE
 *
 * 2010 - 2015 Goodix Technology.
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
 */


#ifndef __GF_SPI_TEE_H
#define __GF_SPI_TEE_H

#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/cdev.h>
#include "mt_spi.h"

/**************************debug******************************/
#define ERR_LOG  (0)
#define INFO_LOG (1)
#define DEBUG_LOG (2)


#define gf_debug(level, fmt, args...) do { \
			if (g_debug_level >= level) {\
				pr_warn("[gf] " fmt, ##args); \
			} \
		} while (0)

#define FUNC_ENTRY()  gf_debug(DEBUG_LOG, "%s, %d, enter\n", __func__, __LINE__)
#define FUNC_EXIT()  gf_debug(DEBUG_LOG, "%s, %d, exit\n", __func__, __LINE__)

/**********************IO Magic**********************/
#define GF_IOC_MAGIC	'g'

typedef enum gf_key_event {
	GF_KEY_NONE = 0,
	GF_KEY_HOME,
	GF_KEY_POWER,
	GF_KEY_MENU,
	GF_KEY_BACK,
	GF_KEY_CAPTURE,
	GF_KEY_UP,
	GF_KEY_DOWN,
	GF_KEY_RIGHT,
	GF_KEY_LEFT,
	GF_KEY_TAP,
	GF_KEY_HEAVY
} gf_key_event_t;

struct gf_key {
	enum gf_key_event key;
	uint32_t value;   /* key down = 1, key up = 0 */
};

enum gf_netlink_cmd {
	GF_NETLINK_TEST = 0,
	GF_NETLINK_IRQ = 1,
	GF_NETLINK_SCREEN_OFF,
	GF_NETLINK_SCREEN_ON
};

struct gf_ioc_transfer {
	u8 cmd;    /* spi read = 0, spi  write = 1 */
	u8 reserved;
	u16 addr;
	u32 len;
	u8 *buf;
};

struct gf_ioc_chip_info {
	u8 vendor_id;
	u8 mode;
	u8 operation;
	u8 reserved[5];
};

/* define commands */
#define GF_IOC_INIT			_IOR(GF_IOC_MAGIC, 0, u8)
#define GF_IOC_EXIT			_IO(GF_IOC_MAGIC, 1)
#define GF_IOC_RESET			_IO(GF_IOC_MAGIC, 2)

#define GF_IOC_ENABLE_IRQ		_IO(GF_IOC_MAGIC, 3)
#define GF_IOC_DISABLE_IRQ		_IO(GF_IOC_MAGIC, 4)

#define GF_IOC_ENABLE_SPI_CLK		_IO(GF_IOC_MAGIC, 5)
#define GF_IOC_DISABLE_SPI_CLK		_IO(GF_IOC_MAGIC, 6)

#define GF_IOC_ENABLE_POWER		_IO(GF_IOC_MAGIC, 7)
#define GF_IOC_DISABLE_POWER		_IO(GF_IOC_MAGIC, 8)

#define GF_IOC_INPUT_KEY_EVENT		_IOW(GF_IOC_MAGIC, 9, struct gf_key)

/* fp sensor has change to sleep mode while screen off */
#define GF_IOC_ENTER_SLEEP_MODE		_IO(GF_IOC_MAGIC, 10)
#define GF_IOC_GET_FW_INFO		_IOR(GF_IOC_MAGIC, 11, u8)
#define GF_IOC_REMOVE		_IO(GF_IOC_MAGIC, 12)
#define GF_IOC_CHIP_INFO	_IOR(GF_IOC_MAGIC, 13, struct gf_ioc_chip_info)
/*goodix SMT*/
#define GF_IOC_FTM		_IOR(GF_IOC_MAGIC, 14, u8)


/* for SPI REE transfer */
#define GF_IOC_TRANSFER_CMD		_IOWR(GF_IOC_MAGIC, 15, struct gf_ioc_transfer)
#define  GF_IOC_MAXNR    16  /* THIS MACRO IS NOT USED NOW... */

struct gf_device {
	dev_t devno;
	struct cdev cdev;
	struct device *device;
	struct class *class;
	struct spi_device *spi;
	int device_count;
	struct mt_chip_conf spi_mcc;

	spinlock_t spi_lock;
	struct list_head device_entry;

	struct input_dev *input;

	u8 *spi_buffer;  /* only used for SPI transfer internal */
	struct mutex buf_lock;
	u8 buf_status;

	/* for netlink use */
	struct sock *nl_sk;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#else
	struct notifier_block notifier;
#endif

	u8 probe_finish;
	u8 irq_count;

	/* bit24-bit32 of signal count */
	/* bit16-bit23 of event type, 1: key down; 2: key up; 3: fp data ready; 4: home key */
	/* bit0-bit15 of event type, buffer status register */
	u32 event_type;
	u8 sig_count;
	u8 is_sleep_mode;
	u8 system_status;

	u32 cs_gpio;
	u32 reset_gpio;
	u32 irq_gpio;
	u32 irq_num;
	u8  need_update;
	u32 irq;

#ifdef CONFIG_OF
	struct pinctrl *pinctrl_gpios;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_miso_spi, *pins_miso_pullhigh, *pins_miso_pulllow;
	struct pinctrl_state *pins_reset_high, *pins_reset_low;
#endif
};

/**************************REE SPI******************************/

#ifndef SUPPORT_REE_SPI
#define SUPPORT_REE_SPI
#endif

#ifdef SUPPORT_REE_SPI

#define HIGH_SPEED 6
#define LOW_SPEED  1

#define ERR_NO_SENSOR    111
#define ERR_FW_DESTROY   112
#define ERR_PREPARE_FAIL 113

/**********************function defination**********************/
void gf_spi_setup_conf_ree(struct gf_device *gf_dev, u32 speed, enum spi_transfer_mode mode);
int gf_spi_read_bytes_ree(struct gf_device *gf_dev, u16 addr, u32 data_len, u8 *rx_buf);
int gf_spi_write_bytes_ree(struct gf_device *gf_dev, u16 addr, u32 data_len, u8 *tx_buf);
int gf_spi_read_byte_ree(struct gf_device *gf_dev, u16 addr, u8 *value);
int gf_spi_write_byte_ree(struct gf_device *gf_dev, u16 addr, u8 value);

#endif

#endif	/* __GF_SPI_TEE_H */
