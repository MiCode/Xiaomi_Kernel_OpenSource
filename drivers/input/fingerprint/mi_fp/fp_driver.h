/*
 * Copyright (C) 2013-2016, Shenzhen Huiding Technology Co., Ltd.
 * All Rights Reserved.
 */
#ifndef FP_DRIVER_H
#define FP_DRIVER_H

/**************************debug******************************/
#define DEBUG
#define pr_fmt(fmt) "xiaomi-fp %s: " fmt, __func__
#define FUNC_ENTRY() pr_debug(" enter\n")
#define FUNC_EXIT() pr_debug(" exit\n")

#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/cdev.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/atomic.h>
#include <linux/timer.h>
#include <linux/input.h>
#include <linux/version.h>
#include <linux/rtc.h>
#include <linux/time.h>

/*#define XIAOMI_DRM_INTERFACE_WA*/
#include <linux/notifier.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef CONFIG_FP_MTK_PLATFORM
#else
#include <linux/clk.h>
#endif

#include <net/sock.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>


#include <linux/of_address.h>

#include  <linux/regulator/consumer.h>

#ifndef XIAOMI_DRM_INTERFACE_WA
#include <linux/workqueue.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#endif
#include <linux/poll.h>

#define DTS_VOlT_REGULATER_GD			"fp_goodix"
#define DTS_VOlT_REGULATER_FPC			"fp_fpc"


#define FP_KEY_INPUT_HOME		KEY_HOME
#define FP_KEY_INPUT_MENU		KEY_MENU
#define FP_KEY_INPUT_BACK		KEY_BACK
#define FP_KEY_INPUT_POWER		KEY_POWER
#define FP_KEY_INPUT_CAMERA		KEY_CAMERA
#define FP_KEY_INPUT_KPENTER            KEY_KPENTER
#define FP_KEY_DOUBLE_CLICK             BTN_C

typedef enum fp_key_event {
	FP_KEY_NONE = 0,
	FP_KEY_HOME,
	FP_KEY_POWER,
	FP_KEY_MENU,
	FP_KEY_BACK,
	FP_KEY_CAMERA,
	FP_KEY_HOME_DOUBLE_CLICK,
} fp_key_event_t;

struct fp_key {
	enum fp_key_event key;
	uint32_t value;		/* key down = 1, key up = 0 */
};

struct fp_key_map {
	unsigned int type;
	unsigned int code;
};

enum fp_netlink_cmd {
	FP_NETLINK_TEST = 0,
	FP_NETLINK_IRQ = 1,
	FP_NETLINK_SCREEN_OFF,
	FP_NETLINK_SCREEN_ON
};

struct fp_ioc_chip_info {
	u8 vendor_id;
	u8 mode;
	u8 operation;
	u8 reserved[5];
};


/**********************IO Magic**********************/
#define FP_IOC_MAGIC	'g'
#define FP_IOC_INIT			_IOR(FP_IOC_MAGIC, 0, u8)
#define FP_IOC_EXIT			_IO(FP_IOC_MAGIC, 1)
#define FP_IOC_RESET			_IO(FP_IOC_MAGIC, 2)

#define FP_IOC_ENABLE_IRQ		_IO(FP_IOC_MAGIC, 3)
#define FP_IOC_DISABLE_IRQ		_IO(FP_IOC_MAGIC, 4)

#define FP_IOC_ENABLE_SPI_CLK           _IOW(FP_IOC_MAGIC, 5, uint32_t)
#define FP_IOC_DISABLE_SPI_CLK		_IO(FP_IOC_MAGIC, 6)

#define FP_IOC_ENABLE_POWER		_IO(FP_IOC_MAGIC, 7)
#define FP_IOC_DISABLE_POWER		_IO(FP_IOC_MAGIC, 8)

#define FP_IOC_INPUT_KEY_EVENT		_IOW(FP_IOC_MAGIC, 9, struct fp_key)

/* fp sensor has change to sleep mode while screen off */
#define FP_IOC_ENTER_SLEEP_MODE		_IO(FP_IOC_MAGIC, 10)
#define FP_IOC_GET_FW_INFO		_IOR(FP_IOC_MAGIC, 11, u8)
#define FP_IOC_REMOVE			_IO(FP_IOC_MAGIC, 12)
#define FP_IOC_CHIP_INFO		_IOW(FP_IOC_MAGIC, 13, struct fp_ioc_chip_info)

#define FP_IOC_MAXNR    		19	/* THIS MACRO IS NOT USED NOW... */

#define DRIVER_COMPATIBLE "xiaomi,xiaomi-fp"
struct fp_device {
	dev_t devt;
	struct cdev cdev;
	struct device *device;
	struct class *class;
	struct list_head device_entry;
#ifdef CONFIG_FP_MTK_PLATFORM
	struct spi_device *driver_device;
#else
	struct platform_device *driver_device;
#endif
    
        struct workqueue_struct *screen_state_wq;
        struct delayed_work screen_state_dw;
	struct input_dev *input;
	struct notifier_block notifier;
	char device_available;
	char fb_black;
	char wait_finger_down;
	u_int32_t  fp_netlink_num;
	int  fp_netlink_enabled;
	int  fp_poll_have_data;
	int  fingerdown;
	wait_queue_head_t fp_wait_queue;
/**************************Pin******************************/
	signed irq_gpio;
	int irq_num;
	int irq_enabled;
	struct regulator *vreg;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_eint_default;
	struct pinctrl_state *pins_eint_pulldown;
	struct pinctrl_state *pins_spiio_spi_mode;
	struct pinctrl_state *pins_spiio_gpio_mode;
	struct pinctrl_state *pins_reset_high;
	struct pinctrl_state *pins_reset_low;
/**************************Pin******************************/

/**************************CONFIG_MTK_PLATFORM******************************/
#ifdef CONFIG_FP_MTK_PLATFORM
	struct pinctrl_state *pins_miso_spi, *pins_miso_pullhigh,
	    *pins_miso_pulllow;
	struct pinctrl_state *pins_spi_cs;
#ifndef CONFIG_SPI_MT65XX_MODULE
	struct mt_chip_conf spi_mcc;
#endif
	spinlock_t spi_lock;
#endif
/**************************CONFIG_MTK_PLATFORM******************************/
};

/**********************function defination**********************/

/*fp_netlink function*/
/*#define FP_NETLINK_ROUTE 29	for GF test temporary, need defined in include/uapi/linux/netlink.h */

void fp_netlink_send(struct fp_device *fp_dev, const int command);
void fp_netlink_recv(struct sk_buff *__skb);
int fp_netlink_init(struct fp_device *fp_dev);
int fp_netlink_destroy(struct fp_device *fp_dev);

/*fp_platform function refererce */
int  fp_parse_dts(struct fp_device *fp_dev);
int  fp_power_config(struct fp_device *fp_dev);
void fp_power_on(struct fp_device *fp_dev);
void fp_power_off(struct fp_device *fp_dev);
void fp_hw_reset(struct fp_device *fp_dev, u8 delay);
void fp_enable_irq(struct fp_device *fp_dev);
void fp_disable_irq(struct fp_device *fp_dev);
void fp_kernel_key_input(struct fp_device *fp_dev, struct fp_key *fp_key);
void fp_local_time_printk(const char *level, const char *format, ...);

#endif /* FP_DRIVER_H */
