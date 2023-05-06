#ifndef __GH_COMMON_H
#define __GH_COMMON_H

#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/cdev.h>
#include <linux/input.h>


#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/i2c.h>

#include "gh_customer.h"

/**************************debug******************************/
#define ERR_LOG  (0)
#define INFO_LOG (1)
#define DEBUG_LOG (2)


extern u8 g_debug_level;

#define gh_debug(level, fmt, args...) do { \
			if (g_debug_level >= level) {\
				pr_warn("[gh]: " fmt, ##args); \
			} \
		} while (0)

#define FUNC_ENTRY()  gh_debug(DEBUG_LOG, "%s, %d, enter\n", __func__, __LINE__)
#define FUNC_EXIT()  gh_debug(DEBUG_LOG, "%s, %d, exit\n", __func__, __LINE__)

/**********************IO Magic**********************/
#define GH_IOC_MAGIC	'g'

enum gh_netlink_cmd {
	GH_NETLINK_TEST = 0,
	GH_NETLINK_IRQ = 1,
	GH_NETLINK_SCREEN_OFF,
	GH_NETLINK_SCREEN_ON
};


struct gh_ioc_transfer_raw {
	u32 len;
	u8 *read_buf;
	u8 *write_buf;
	uint32_t high_time;
	uint32_t low_time;
};

struct gh_ioc_chip_info {
	u8 vendor_id;
	u8 mode;
	u8 operation;
	u8 reserved[5];
};

/* define for gh_spi_cfg_t->speed_hz */
#define GH_SPI_SPEED_LOW 1
#define GH_SPI_SPEED_MEDIUM 6
#define GH_SPI_SPEED_HIGH 9

enum gh_spi_cpol {
	GH_SPI_CPOL_0,
	GH_SPI_CPOL_1
};

enum gh_spi_cpha {
	GH_SPI_CPHA_0,
	GH_SPI_CPHA_1
};

typedef struct {
	unsigned int cs_setuptime;
	unsigned int cs_holdtime;
	unsigned int cs_idletime;
	unsigned int speed_hz; //spi clock rate
	unsigned int duty_cycle; //The time ratio of active level in a period. Default value is 50. that means high time and low time is same.
	enum gh_spi_cpol cpol;
	enum gh_spi_cpol cpha;
} gh_spi_cfg_t;


 struct gh_i2c_rdwr_ioctl_data {
	struct i2c_msg *msgs;   /* pointers to i2c_msgs */
	__u32 nmsgs;            /* number of i2c_msgs */
 };

/* define commands */
#define GH_IOC_INIT			_IOR(GH_IOC_MAGIC, 0, u8)
#define GH_IOC_EXIT			_IO(GH_IOC_MAGIC, 1)
#define GH_IOC_RESET			_IO(GH_IOC_MAGIC, 2)

#define GH_IOC_ENABLE_IRQ		_IO(GH_IOC_MAGIC, 3)
#define GH_IOC_DISABLE_IRQ		_IO(GH_IOC_MAGIC, 4)

#define GH_IOC_ENABLE_SPI_CLK           _IOW(GH_IOC_MAGIC, 5, uint32_t)
#define GH_IOC_DISABLE_SPI_CLK		_IO(GH_IOC_MAGIC, 6)

#define GH_IOC_ENABLE_POWER		_IO(GH_IOC_MAGIC, 7)
#define GH_IOC_DISABLE_POWER		_IO(GH_IOC_MAGIC, 8)

#define GH_IOC_INPUT_KEY_EVENT		_IOW(GH_IOC_MAGIC, 9, struct gh_key)

/* fp sensor has change to sleep mode while screen off */
#define GH_IOC_ENTER_SLEEP_MODE		_IO(GH_IOC_MAGIC, 10)
#define GH_IOC_GET_FW_INFO		_IOR(GH_IOC_MAGIC, 11, u8)
#define GH_IOC_REMOVE		_IO(GH_IOC_MAGIC, 12)
#define GH_IOC_CHIP_INFO	_IOR(GH_IOC_MAGIC, 13, struct gh_ioc_chip_info)

#define GH_IOC_NAV_EVENT	_IOW(GH_IOC_MAGIC, 14, gh_nav_event_t)

/* for SPI REE transfer */
#define GH_IOC_TRANSFER_CMD		_IOWR(GH_IOC_MAGIC, 15, struct gh_ioc_transfer)
#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
#define GH_IOC_TRANSFER_RAW_CMD	_IOWR(GH_IOC_MAGIC, 16, struct gh_ioc_transfer_raw)
#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)
#define GH_IOC_TRANSFER_RAW_CMD	_IOWR(GH_IOC_MAGIC, 16, struct gh_i2c_rdwr_ioctl_data)
#endif
#define GH_IOC_SPI_INIT_CFG_CMD	_IOW(GH_IOC_MAGIC, 17, gh_spi_cfg_t)
#define GH_IOC_SET_RESET_VALUE    _IOW(GH_IOC_MAGIC, 18, uint32_t)

#define  GH_IOC_MAXNR    19  /* THIS MACRO IS NOT USED NOW... */

struct gh_device {
	dev_t devno;
	struct cdev cdev;
	struct device *device;
	struct class *class;
#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
	struct spi_device *spi;
#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)
	struct i2c_client *client;
#endif
	int device_count;
	//struct mt_chip_conf spi_mcc;

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
	/* bit16-bit23 of event type, 1: key down; 2: key up; 3: fp data ready; 4: home key */
	/* bit0-bit15 of event type, buffer status register */
	u32 event_type;
	u8 sig_count;
	u8 is_sleep_mode;
	u8 system_status;
	u32 boost_en_gpio;
	u32 cs_gpio;
	u32 reset_gpio;
	u32 irq_gpio;
	u32 irq_num;
	u8  need_update;
	u32 irq;

#ifdef CONFIG_OF
	struct pinctrl *pinctrl_gpios;
	struct pinctrl_state *pins_irq;
	struct pinctrl_state *pins_miso_spi, *pins_miso_pullhigh, *pins_miso_pulllow;
	struct pinctrl_state *pins_reset_high, *pins_reset_low;
#endif
};

/**********************platform defination**********************/
int gh_get_gpio_dts_info(struct gh_device *gh_dev);
void gh_cleanup_info(struct gh_device *gh_dev);
void gh_hw_power_enable(struct gh_device *gh_dev, u8 onoff);
void gh_irq_gpio_cfg(struct gh_device *gh_dev);
void gh_hw_reset(struct gh_device *gh_dev, u8 delay);
void gh_hw_set_reset_value(struct gh_device *gh_dev, u8 value);
/**************************REE SPI******************************/

#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
/**********************function defination**********************/
void gh_spi_setup_conf(struct gh_device *gh_dev, u32 speed);
int gh_init_transfer_buffer(void);
int gh_free_transfer_buffer(void);
#endif

int gh_ioctl_transfer_raw_cmd(struct gh_device *gh_dev, unsigned long arg, unsigned int bufsiz);

#endif	/* __GH_COMMON_H */
