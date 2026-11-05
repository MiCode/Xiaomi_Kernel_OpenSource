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
#include <linux/kfifo.h>


/* #define XIAOMI_DRM_INTERFACE_WA */
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
#include <linux/pinctrl/consumer.h>

#include <linux/of_address.h>

#include  <linux/regulator/consumer.h>

#ifndef XIAOMI_DRM_INTERFACE_WA
#include <linux/workqueue.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#endif
#include <linux/poll.h>

#ifndef CONFIG_SIDE_FINGERPRINT
#ifndef CONFIG_FOD_FINGERPRINT
#define FP_ULTRA_QCOM
#endif
#endif

#ifdef FP_ULTRA_QCOM
#include "fp_ultra.h"
#endif

#ifdef CONFIG_ULTRA_FINGERPRINT_P11
#define FP_ULTRA_QCOM_P11
#endif

#ifdef CONFIG_ULTRA_AP_CTRL_INTR2
#define FP_INTR2_AP_PER_LOW
#define FP_ULTRA_QCOM_P11
#endif

#define DTS_VOlT_REGULATER_3V3          "fp_3v3_vreg"
#define DTS_VOlT_REGULATER_3V1          "fp_3v1_vreg"
#define DTS_VOlT_REGULATER_1V8          "fp_1v8_vreg"
#define DTS_VOlT_REGULATER_GPIO         "xiaomi,gpio_pwr"

#define POWER_LDO_3V3   "vreg3v3"
#define POWER_LDO_1V8   "vreg1v8"
#define POWER_VDDIO     "pwr_gpio"

#define FP_RESET_DEFAULT_TIME   10

#define FP_KEY_INPUT_HOME		KEY_HOME
#define FP_KEY_INPUT_MENU		KEY_MENU
#define FP_KEY_INPUT_BACK		KEY_BACK
#define FP_KEY_INPUT_POWER		KEY_POWER
#define FP_KEY_INPUT_CAMERA		KEY_CAMERA
#define FP_KEY_INPUT_KPENTER    KEY_KPENTER
#define FP_KEY_DOUBLE_CLICK     BTN_C

enum fp_cdev_minor {
	MINOR_NUM_MIFP = 0,
	MINOR_NUM_MIFP_ID = 1,
	MINOR_NUM_IPC = 2,
#ifndef FP_ULTRA_QCOM_P11
	MINOR_NUM_FD = 3,
#endif
};

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
	FP_NETLINK_SCREEN_ON,
};

struct fp_ioc_chip_info {
	u8 vendor_id;
	u8 mode;
	u8 operation;
	u8 reserved[5];
};


/**********************IO Magic**********************/
#define FP_IOC_MAGIC	         'g'
#define FP_IOC_INIT			     _IOR(FP_IOC_MAGIC, 0, u8)
#define FP_IOC_EXIT			     _IO(FP_IOC_MAGIC, 1)
#define FP_IOC_RESET			 _IO(FP_IOC_MAGIC, 2)

#define FP_IOC_ENABLE_IRQ		 _IO(FP_IOC_MAGIC, 3)
#define FP_IOC_DISABLE_IRQ		 _IO(FP_IOC_MAGIC, 4)

#define FP_IOC_ENABLE_SPI_CLK    _IOW(FP_IOC_MAGIC, 5, uint32_t)
#define FP_IOC_DISABLE_SPI_CLK	 _IO(FP_IOC_MAGIC, 6)

#define FP_IOC_ENABLE_POWER		 _IOW(FP_IOC_MAGIC, 7, char)
#define FP_IOC_DISABLE_POWER	 _IOW(FP_IOC_MAGIC, 8, char)

#define FP_IOC_INPUT_KEY_EVENT	 _IOW(FP_IOC_MAGIC, 9, struct fp_key)

/* fp sensor has change to sleep mode while screen off */
#define FP_IOC_ENTER_SLEEP_MODE	 _IO(FP_IOC_MAGIC, 10)
#define FP_IOC_GET_FW_INFO		 _IOR(FP_IOC_MAGIC, 11, u8)
#define FP_IOC_REMOVE			 _IO(FP_IOC_MAGIC, 12)
#define FP_IOC_CHIP_INFO		 _IOW(FP_IOC_MAGIC, 13, struct fp_ioc_chip_info)

#define FP_IOC_MAXNR    		 19	/* THIS MACRO IS NOT USED NOW... */

#ifndef FP_ULTRA_QCOM_P11
#define QBT_IS_WUHB_CONNECTED	 _IO(FP_IOC_MAGIC, 20)
#endif
#define QBT_SEND_KEY_EVENT	 _IO(FP_IOC_MAGIC, 21)
#define QBT_ENABLE_IPC	 _IO(FP_IOC_MAGIC, 22)
#define QBT_DISABLE_IPC	 _IO(FP_IOC_MAGIC, 23)
#ifndef FP_ULTRA_QCOM_P11
#define QBT_ENABLE_FD	 _IO(FP_IOC_MAGIC, 24)
#define QBT_DISABLE_FD	 _IO(FP_IOC_MAGIC, 25)
#endif
#define QBT_CONFIGURE_TOUCH_FD	 _IO(FP_IOC_MAGIC, 26)
#define QBT_ACQUIRE_WAKELOCK	 _IO(FP_IOC_MAGIC, 27)
#define QBT_RELEASE_WAKELOCK	 _IO(FP_IOC_MAGIC, 28)

#define FP_IOC_REQUEST_IPC           _IO(FP_IOC_MAGIC, 32)
#define FP_IOC_FREE_IPC              _IO(FP_IOC_MAGIC, 33)
#ifndef FP_ULTRA_QCOM_P11
#define FP_IOC_REQUEST_WUHB          _IO(FP_IOC_MAGIC, 34)
#define FP_IOC_FREE_WUHB             _IO(FP_IOC_MAGIC, 35)
#endif
#define FP_IOC_DISABLE_INTR2         _IO(FP_IOC_MAGIC, 36)
#define FP_IOC_REQUEST_RESOURCE      _IO(FP_IOC_MAGIC, 37)
#define FP_IOC_RELEASE_RESOURCE      _IO(FP_IOC_MAGIC, 38)
#define FP_IOC_REQUEST_INTR2         _IO(FP_IOC_MAGIC, 39)
#define FP_IOC_DEV_INFO              _IOR(FP_IOC_MAGIC, 40, char)
#define FP_IOC_RESET_OUT_LOW         _IO(FP_IOC_MAGIC, 41)
#define FP_IOC_REGULATOR_ENABLE_PWM     _IO(FP_IOC_MAGIC, 42)
#define FP_IOC_REGULATOR_DISABLE_PWM    _IO(FP_IOC_MAGIC, 43)
#define FP_IOC_RESET_TIME_MS            _IOW(FP_IOC_MAGIC, 44, uint32_t)
#define FP_IOC_INTR2_SIDE_CONFIG        _IOR(FP_IOC_MAGIC, 45, uint32_t)


#define DRIVER_COMPATIBLE "xiaomi,xiaomi-fp"


struct fp_position {
	bool enable;
	bool reqIntr2;
	int x1,x2,y1,y2;
	int TRACKING_ID[32];
	int SLOT_CUR;
	u_int32_t SLOT_MAP;
	unsigned int location[5];
	int touch_x[32];
	int touch_y[32];
};

struct fp_vreg {
	bool enable;
	bool IsGpio;
	struct regulator *mRegulator;
	signed mPwrGpio;
};

struct fp_device {
	dev_t devt;
	struct cdev cdev;
	struct cdev fpid_cdev;  //mfpca open device, read chip-id ioctl
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
	struct wakeup_source *wakesrc;
	struct input_dev *input;
	struct notifier_block notifier;
	const char *vendor_names;
	char device_available;
	char fb_black;
	char wait_finger_down;
	u_int32_t  fp_netlink_num;
	int  fp_netlink_enabled;
	int  fp_poll_have_data;
	int  fingerdown;
	wait_queue_head_t fp_wait_queue;
	u_int32_t rst_high_delay;
	u_int32_t rst_low_delay;
	u_int32_t rst_pull_flag[5];
	u_int32_t intr2_side_config;
#ifdef FP_INTR2_AP_PER_LOW
	bool intr2_AP_pin_enable;
	u_int8_t intr2_ctrl_enable;	// 0:disable 1:enable to control intr2 (set low or high), default is 1
#endif
#ifdef FP_ULTRA_QCOM_P11
	bool reset_pin_enable;
#endif
/**************************Pin******************************/
	signed irq_gpio;
	int irq_num;
	int irq_enabled;
	bool irq_request;
	struct fp_vreg vreg_3v3;
	struct fp_vreg vreg_1v8;
	struct fp_vreg vreg_gpio;
	struct pinctrl *pinctrl;
#ifdef FP_ULTRA_QCOM_P11
	struct pinctrl_state *pins_int_default;
	struct pinctrl_state *pins_int_pulldown;
#else
	struct pinctrl_state *pins_eint_default;
	struct pinctrl_state *pins_eint_pulldown;
#endif
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
/**************************CONFIG_QCOM_ULTRA******************************/
#ifdef FP_ULTRA_QCOM
#ifdef FP_ULTRA_QCOM_P11
	atomic_t	ipc_available;
	atomic_t    wakelock_acquired;
	struct fw_ipc_info	fw_ipc;
	bool intr2_enable;
	uint32_t intr2_gpio;
	struct cdev qbt_ipc_cdev;
	struct mutex	mutex;
	struct mutex	ipc_events_mutex;
	spinlock_t	intr2_events_lock;
	DECLARE_KFIFO(ipc_events, struct ipc_event, MAX_FW_EVENTS);
	wait_queue_head_t read_wait_queue_ipc;
	struct fp_position position;
#else
	atomic_t	fd_available;
	atomic_t	ipc_available;
	atomic_t    wakelock_acquired;
	struct fw_ipc_info	fw_ipc;
	struct finger_detect_gpio fd_gpio;
	uint32_t intr2_gpio;
	bool is_wuhb_connected;
	struct cdev qbt_fd_cdev;
	struct cdev qbt_ipc_cdev;
	struct mutex	mutex;
	struct mutex	fd_events_mutex;
	struct mutex	ipc_events_mutex;
	DECLARE_KFIFO(fd_events, struct fd_event, MAX_FW_EVENTS);
	DECLARE_KFIFO(ipc_events, struct ipc_event, MAX_FW_EVENTS);
	wait_queue_head_t read_wait_queue_fd;
	wait_queue_head_t read_wait_queue_ipc;
	struct fd_userspace_buf scrath_buf;
	struct fp_position position;
	struct work_struct intr2_work;
#endif
#endif
/**************************CONFIG_QCOM_ULTRA******************************/
};
/**********************function defination**********************/

/*fp_netlink function*/
/*#define FP_NETLINK_ROUTE 29	for GF test temporary, need defined in include/uapi/linux/netlink.h */

void fp_netlink_send(struct fp_device *fp_dev, const int command);
void fp_netlink_recv(struct sk_buff *__skb);
int fp_netlink_init(struct fp_device *fp_dev);
int fp_netlink_destroy(struct fp_device *fp_dev);

/*fp_platform function refererce */
int fp_parse_dts(struct fp_device *fp_dev);
int fp_power_on(struct fp_vreg *vreg);
int fp_power_off(struct fp_vreg *vreg);
void fp_power_config(struct fp_device *fp_dev);
void fp_hw_reset(struct fp_device *fp_dev);
void fp_hw_reset_delay(struct fp_device *fp_dev, u_int32_t rst_low_delay, u_int32_t rst_high_delay);
void fp_enable_irq(struct fp_device *fp_dev);
void fp_disable_irq(struct fp_device *fp_dev);
void fp_kernel_key_input(struct fp_device *fp_dev, struct fp_key *fp_key);
void fp_local_time_printk(const char *format, ...);
void fp_acquire_stay_wake_time_ms(struct fp_device *fp_dev, unsigned int msec);
void fp_acquire_stay_wake(struct fp_device *fp_dev);
void fp_release_stay_wake(struct fp_device *fp_dev);

#ifdef FP_ULTRA_QCOM
/*fp_input function refererce */
int xiaomifp_evdev_init(struct fp_device *fp_dev);
void xiaomifp_evdev_remove(void);
#endif

#endif /* FP_DRIVER_H */
