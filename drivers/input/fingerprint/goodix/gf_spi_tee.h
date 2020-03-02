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
#include <linux/input.h>
#ifndef CONFIG_SPI_MT65XX
#include "mtk_spi.h"
#endif
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
#include <mach/mtk_6306_gpio.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/notifier.h>
#endif

/**************************debug******************************/
#define ERR_LOG  (0)
#define INFO_LOG (1)
#define DEBUG_LOG (2)

extern u8 g_debug_level;
#define gf_debug(level, fmt, args...) do { \
			if (g_debug_level >= level) {\
				pr_info("[gf] " fmt, ##args); \
			} \
		} while (0)

#define FUNC_ENTRY()  \
	gf_debug(DEBUG_LOG, "%s, %d, enter\n", __func__, __LINE__)
#define FUNC_EXIT()  \
	gf_debug(DEBUG_LOG, "%s, %d, exit\n", __func__, __LINE__)

/**********************IO Magic**********************/
#define GF_IOC_MAGIC	'g'

#define GF_NAV_INPUT_UP			KEY_UP
#define GF_NAV_INPUT_DOWN		KEY_DOWN
#define GF_NAV_INPUT_LEFT		KEY_LEFT
#define GF_NAV_INPUT_RIGHT		KEY_RIGHT
#define GF_NAV_INPUT_CLICK		KEY_VOLUMEDOWN
#define GF_NAV_INPUT_DOUBLE_CLICK	KEY_VOLUMEUP
#define GF_NAV_INPUT_LONG_PRESS		KEY_SEARCH
#define GF_NAV_INPUT_HEAVY		KEY_CHAT

#define GF_KEY_INPUT_HOME		KEY_HOMEPAGE
#define GF_KEY_INPUT_MENU		KEY_MENU
#define GF_KEY_INPUT_BACK		KEY_BACK
#define GF_KEY_INPUT_POWER		KEY_POWER
#define GF_KEY_INPUT_CAMERA		KEY_CAMERA

enum gf_nav_event_t {
	GF_NAV_NONE = 0,
	GF_NAV_FINGER_UP,
	GF_NAV_FINGER_DOWN,
	GF_NAV_UP,
	GF_NAV_DOWN,
	GF_NAV_LEFT,
	GF_NAV_RIGHT,
	GF_NAV_CLICK,
	GF_NAV_HEAVY,
	GF_NAV_LONG_PRESS,
	GF_NAV_DOUBLE_CLICK,
};

enum gf_key_event_t {
	GF_KEY_NONE = 0,
	GF_KEY_HOME,
	GF_KEY_POWER,
	GF_KEY_MENU,
	GF_KEY_BACK,
	GF_KEY_CAMERA,
};

struct gf_key {
	enum gf_key_event_t key;
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

struct gf_ioc_transfer_raw {
	u32 len;
	u8 *read_buf;
	u8 *write_buf;
	uint32_t high_time;
	uint32_t low_time;
};

struct gf_ioc_chip_info {
	u8 vendor_id;
	u8 mode;
	u8 operation;
	u8 reserved[5];
};

/* define for gf_spi_cfg_t->speed_hz */
#define GF_SPI_SPEED_LOW 1
#define GF_SPI_SPEED_MEDIUM 6
#define GF_SPI_SPEED_HIGH 9

enum gf_spi_cpol {
	GF_SPI_CPOL_0,
	GF_SPI_CPOL_1
};

enum gf_spi_cpha {
	GF_SPI_CPHA_0,
	GF_SPI_CPHA_1
};

struct gf_spi_cfg_t {
	unsigned int cs_setuptime;
	unsigned int cs_holdtime;
	unsigned int cs_idletime;
	unsigned int speed_hz; /* spi clock rate */
	/* The time ratio of active level in a period. Default value is 50.
	 * that means high time and low time is same.
	 */
	unsigned int duty_cycle;
	enum gf_spi_cpol cpol;
	enum gf_spi_cpol cpha;
};

/* define commands */
#define GF_IOC_INIT			_IOR(GF_IOC_MAGIC, 0, u8)
#define GF_IOC_EXIT			_IO(GF_IOC_MAGIC, 1)
#define GF_IOC_RESET			_IO(GF_IOC_MAGIC, 2)

#define GF_IOC_ENABLE_IRQ		_IO(GF_IOC_MAGIC, 3)
#define GF_IOC_DISABLE_IRQ		_IO(GF_IOC_MAGIC, 4)

#define GF_IOC_ENABLE_SPI_CLK           _IOW(GF_IOC_MAGIC, 5, uint32_t)
#define GF_IOC_DISABLE_SPI_CLK		_IO(GF_IOC_MAGIC, 6)

#define GF_IOC_ENABLE_POWER		_IO(GF_IOC_MAGIC, 7)
#define GF_IOC_DISABLE_POWER		_IO(GF_IOC_MAGIC, 8)

#define GF_IOC_INPUT_KEY_EVENT	_IOW(GF_IOC_MAGIC, 9, struct gf_key)

/* fp sensor has change to sleep mode while screen off */
#define GF_IOC_ENTER_SLEEP_MODE		_IO(GF_IOC_MAGIC, 10)
#define GF_IOC_GET_FW_INFO		_IOR(GF_IOC_MAGIC, 11, u8)
#define GF_IOC_REMOVE		_IO(GF_IOC_MAGIC, 12)
#define GF_IOC_CHIP_INFO	_IOW(GF_IOC_MAGIC, 13, struct gf_ioc_chip_info)

#define GF_IOC_NAV_EVENT	_IOW(GF_IOC_MAGIC, 14, enum gf_nav_event_t)

/* for SPI REE transfer */
#define GF_IOC_TRANSFER_CMD \
	_IOWR(GF_IOC_MAGIC, 15, struct gf_ioc_transfer)
#define GF_IOC_TRANSFER_RAW_CMD \
	_IOWR(GF_IOC_MAGIC, 16, struct gf_ioc_transfer_raw)
#define GF_IOC_SPI_INIT_CFG_CMD \
	_IOW(GF_IOC_MAGIC, 17, struct gf_spi_cfg_t)

#define  GF_IOC_MAXNR    18  /* THIS MACRO IS NOT USED NOW... */

/* SMT backup solution (just to get chip ID) */
#define GF_IOC_FTM		_IOR(GF_IOC_MAGIC, 20, u8)

struct gf_device {
	dev_t devno;
	struct cdev cdev;
	struct device *device;
	struct class *class;
	struct spi_device *spi;
	int device_count;
#ifndef CONFIG_SPI_MT65XX
	struct mt_chip_conf spi_mcc;
#endif

	spinlock_t spi_lock;
	struct list_head device_entry;

	struct input_dev *input;

	u8 *spi_buffer;  /* only used for SPI transfer internal */
	struct mutex buf_lock;
	struct mutex release_lock;
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
	/*
	 * bit16-bit23 of event type:
	 * 1: key down;
	 * 2: key up;
	 * 3: fp data ready;
	 * 4: home key
	 */
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
	struct pinctrl *pinctrl_gpio;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_miso_spi;
	struct pinctrl_state *miso_pullhigh;
	struct pinctrl_state *miso_pulllow;
	struct pinctrl_state *rst_high;
	struct pinctrl_state *rst_low;
	//struct pinctrl_state *pins_reset_high, *pins_reset_low;
#endif
};

/**************************REE SPI******************************/

#ifndef SUPPORT_REE_SPI
#define SUPPORT_REE_SPI
#endif

#ifdef SUPPORT_REE_SPI

#define SUPPORT_REE_MILAN_A

#define HIGH_SPEED 6
#define LOW_SPEED  1

#define ERR_NO_SENSOR    111
#define ERR_FW_DESTROY   112
#define ERR_PREPARE_FAIL 113

/**********************function defination**********************/
#ifndef CONFIG_SPI_MT65XX
void gf_spi_setup_conf_ree(struct gf_device *gf_dev, u32 speed,
					enum spi_transfer_mode mode);
#endif
int gf_spi_read_bytes_ree(struct gf_device *gf_dev, u16 addr,
u32				 data_len, u8 *rx_buf);
int gf_spi_write_bytes_ree(struct gf_device *gf_dev, u16 addr,
				u32 data_len, u8 *tx_buf);
int gf_spi_read_byte_ree(struct gf_device *gf_dev, u16 addr, u8 *value);
int gf_spi_write_byte_ree(struct gf_device *gf_dev, u16 addr, u8 value);
int gf_ioctl_transfer_raw_cmd(struct gf_device *gf_dev, unsigned long arg,
					unsigned int bufsiz);
#ifndef CONFIG_SPI_MT65XX
int  gf_ioctl_spi_init_cfg_cmd(struct mt_chip_conf *mcc, unsigned long arg);
#endif
#ifdef CONFIG_SPI_MT65XX
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);
#endif

#ifdef SUPPORT_REE_MILAN_A
#ifndef CONFIG_TRUSTONIC_TEE_SUPPORT
int gf_spi_read_bytes_ree_new(struct gf_device *gf_dev, u16 addr,
					u32 data_len, u8 *buf);
int gf_spi_write_bytes_ree_new(struct gf_device *gf_dev, u16 addr,
					u32 data_len, u8 *buf);
int gf_milan_a_series_init_process(struct gf_device *gf_dev);
#endif
#endif

#endif

#endif	/* __GF_SPI_TEE_H */
