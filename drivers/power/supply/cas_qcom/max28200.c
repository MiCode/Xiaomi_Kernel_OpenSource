#define pr_fmt(fmt)     "[max28200] %s: " fmt, __func__
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>

enum print_reason {
	PR_INTERRUPT = BIT(0),
	PR_REGISTER = BIT(1),
	PR_OEM = BIT(2),
	PR_DEBUG = BIT(3),
	PR_FW = BIT(4),
};

static int debug_mask = PR_OEM | PR_FW;

module_param_named(debug_mask, debug_mask, int, 0600);

#define max_dbg(reason, fmt, ...)			\
	do {						\
		if (debug_mask & (reason))		\
		pr_info(fmt, ##__VA_ARGS__);	\
		else					\
		pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

#define DELAY_RESET_MS          1000
#define DELAY_AFTER_RESET_US    400
#define DELAY_MASTERERASE_MS    40000
#define DELAY_PROMPT_US			200
#define DELAY_ERASEPAGE_MS		40000

#define LOADER_PROGRAM_ADDRESS			(0x0000)

#define MAX28200_CMD_NOP				0x00
#define MAX28200_CMD_MASTERERASE       	0x02
#define MAX28200_CMD_GETSTATUS       	0x04
#define MAX28200_CMD_WORDBYTEMODE       0x0A
#define MAX28200_CMD_MULTIPLIER			0x0B
#define MAX28200_CMD_READCOMMAND       	0x20
#define MAX28200_CMD_LOADVERIFY       	0xD0
#define MAX28200_CMD_ERASEBLOCK       	0xE0

#define MAX28200_MAGIC_VALUE			0x55
#define MAX28200_RESP_VALUEPROMPT		0x3E
#define LOADER_EXPECTED_STATUS			0x04

#define MAX28200_PARAM_MULTIPLIER		0x20
#define LOADER_PAYLOAD_LENGTH			(4 * MAX28200_PARAM_MULTIPLIER)

#define STATUS_ERROR      	         (-1)
#define STATUS_I2C_OPEN_ERROR        (-2)
#define STATUS_I2C_CLOSE_ERROR       (-3)
#define STATUS_GPIO_OPEN_ERROR       (-4)
#define STATUS_GPIO_CLOSE_ERROR      (-5)
#define STATUS_READ_ERROR            (-6)
#define STATUS_WRITE_ERROR           (-7)
#define STATUS_SLADDR_ERROR          (-8)
#define STATUS_GPIO_HIGH_ERROR       (-9)
#define STATUS_GPIO_LOW_ERROR        (-10)
#define STATUS_INVALID_PROMPT_ERROR  (-11)
#define STATUS_INVALID_RESULT_ERROR  (-12)
#define STATUS_VERIFY_FAILED_ERROR   (-13)
#define STATUS_INVALID_ADDRESS_ERROR (-14)

#define MAX28200_LOADER_ERROR_NONE		0x00
#define MAX28200_LOADER_VERIFY_FAILED   0x05

#define MAX28200_FLASHPAGESIZE_BYTES	1024

#define MAX28200_FIRMWARE_MAXBYTES	0x0C3C

#define MAX28200_SW_VERSION_REG 0x01
#define MAX28200_SW_VERSION 0x0001

enum max28200_ctrl_cmd {
	MAX28200_CTRL_P0_LOW = 0x00,
	MAX28200_CTRL_P0_HIG = 0x01,
	MAX28200_RESET_WATCHDOG = 0x02,
	MAX28200_STOP_WATCHDOG = 0x03,
	MAX28200_SET_TIME_10S = 0x04,
	MAX28200_SET_TIME_5S = 0x05,
	MAX28200_SET_TIME_2S = 0x06,
	MAX28200_SET_TIME_1S = 0x07,
	MAX28200_SET_TIME_500MS = 0x08,
	MAX28200_SET_TIME_200MS = 0x09,
	MAX28200_SET_TIME_100MS = 0x0A,
	MAX28200_SET_TIME_50MS = 0x0B,
	MAX28200_SET_TIME_20MS = 0x0C,
	MAX28200_SET_TIME_10MS = 0x0D,
	MAX28200_FW_VERSION_CMD = 0x0E,
};

struct max28200_chip {
	struct device *dev;
	struct i2c_client *client;
	struct mutex i2c_rw_lock;
	int use_pin_en;
	int watchdog_int;
	int watchdog_irq;
	int version;
	struct delayed_work firmware_download_work;
	struct delayed_work sw_check_work;
	struct delayed_work monitor_work;
	struct pinctrl *max28200_pinctrl;
	struct pinctrl_state *max_reset_high;
	struct pinctrl_state *max_reset_low;
	struct pinctrl_state *max_pin_en_on;
	struct pinctrl_state *max_pin_en_off;
	bool enabled;
	struct power_supply_desc max28200_psy_d;
	struct power_supply *max28200_psy;
	bool fw_ok;
	bool early_enable;
};

u8 max28200_firmware[MAX28200_FIRMWARE_MAXBYTES] = {
	0x3a, 0xda, 0x00, 0x6b, 0x00, 0x70, 0x00, 0x3b, 0xf7, 0x88, 0x00, 0x0b,
	    0xed, 0x0c, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02, 0x00,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0x8d, 0x8c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0x8d, 0x8c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0x61, 0x0c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0x56, 0x0c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0x8d, 0x8c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0x53, 0x0c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff,
	0x8d, 0x8c, 0x8d, 0x8c, 0x00, 0x2b, 0xff, 0x79, 0x0a, 0x8d, 0x13, 0x8a,
	    0xff, 0x0b, 0x00, 0x1a,
	0x0a, 0x93, 0x0d, 0x8a, 0x00, 0x2b, 0x01, 0x39, 0x00, 0x1b, 0x37, 0x93,
	    0x8d, 0x8c, 0x0a, 0x8d,
	0x79, 0x8a, 0x01, 0x4a, 0x0a, 0xf9, 0x02, 0x78, 0x03, 0x2c, 0x00, 0x79,
	    0x00, 0x2b, 0x01, 0x59,
	0x67, 0xa1, 0x77, 0xa1, 0x0d, 0x8a, 0x8d, 0x8c, 0x8d, 0x8c, 0x0a, 0x8d,
	    0x00, 0x79, 0x19, 0x89,
	0x02, 0x0b, 0x09, 0x3d, 0x05, 0x0a, 0x02, 0x0b, 0x51, 0x3d, 0x02, 0x0b,
	    0x16, 0x3d, 0x02, 0x0b,
	0x33, 0x3d, 0x02, 0x0b, 0x90, 0x3d, 0x0d, 0x8a, 0x0d, 0x8c, 0xff, 0x4b,
	    0xff, 0x03, 0x01, 0x2b,
	0x00, 0x13, 0x02, 0x2b, 0x00, 0x33, 0x00, 0x2b, 0x2a, 0x43, 0x00, 0x13,
	    0x21, 0x03, 0x00, 0x2b,
	0xf7, 0x93, 0x00, 0x1b, 0xb7, 0x93, 0x0d, 0x8c, 0x00, 0x03, 0x00, 0x2b,
	    0x00, 0x43, 0x00, 0x13,
	0x00, 0x2b, 0xf7, 0x93, 0x00, 0x2b, 0xf7, 0xb3, 0x0d, 0x8c, 0x02, 0x0b,
	    0x92, 0x3d, 0x00, 0x1b,
	0x37, 0x93, 0x01, 0x0b, 0xea, 0x3d, 0x0a, 0xb9, 0xd0, 0x3d, 0x0d, 0x8c,
	    0x00, 0x2b, 0xf7, 0xb3,
	0x00, 0x2b, 0xf7, 0x93, 0x00, 0x1b, 0xb7, 0x93, 0x0d, 0x8c, 0x16, 0x0b,
	    0xe3, 0x19, 0x00, 0x4b,
	0x87, 0x80, 0x00, 0x4b, 0x87, 0x80, 0x07, 0x80, 0x00, 0x2b, 0x00, 0x39,
	    0x00, 0x2b, 0x00, 0x49,
	0x00, 0x2b, 0x77, 0x88, 0x3a, 0xda, 0x3a, 0xda, 0x3a, 0xda, 0xfc, 0x4e,
	    0xc6, 0x3d, 0x87, 0xd8,
	0x00, 0x2b, 0x00, 0x19, 0xd1, 0x3d, 0xc1, 0x3d, 0x00, 0x1b, 0xb7, 0x93,
	    0xd9, 0x8a, 0x43, 0x5c,
	0xb9, 0x8a, 0x01, 0x5c, 0xf9, 0x0c, 0x00, 0x2b, 0x00, 0x39, 0xcf, 0x3d,
	    0x39, 0x8a, 0xff, 0x0b,
	0xf0, 0x1a, 0x03, 0x1c, 0xff, 0x0b, 0xf0, 0x39, 0xed, 0x0c, 0x39, 0x8a,
	    0x0f, 0x1a, 0x2a, 0x8a,
	0x01, 0x0b, 0x1b, 0x4a, 0x0a, 0x8c, 0x01, 0x0b, 0x53, 0x0c, 0x01, 0x0b,
	    0x57, 0x0c, 0x01, 0x0b,
	0xd8, 0x0c, 0x01, 0x0b, 0x4b, 0x0c, 0x01, 0x0b, 0x5d, 0x0c, 0x01, 0x0b,
	    0x65, 0x0c, 0x01, 0x0b,
	0x6d, 0x0c, 0x01, 0x0b, 0x75, 0x0c, 0x01, 0x0b, 0x7d, 0x0c, 0x01, 0x0b,
	    0x86, 0x0c, 0x01, 0x0b,
	0x8f, 0x0c, 0x01, 0x0b, 0x98, 0x0c, 0x01, 0x0b, 0xa1, 0x0c, 0x01, 0x0b,
	    0xa9, 0x0c, 0x01, 0x0b,
	0xb1, 0x0c, 0x3a, 0xda, 0x3a, 0xda, 0x3a, 0xda, 0x3a, 0xda, 0x3a, 0xda,
	    0x3a, 0xda, 0x3a, 0xda,
	0x3a, 0xda, 0x3a, 0xda, 0x3a, 0xda, 0x3a, 0xda, 0x3a, 0xda, 0x3a, 0xda,
	    0x3a, 0xda, 0x3a, 0xda,
	0x3a, 0xda, 0x3a, 0xda, 0xb7, 0x0c, 0x00, 0x2b, 0x00, 0x59, 0x02, 0x0b,
	    0x92, 0x3d, 0x00, 0x4b,
	0x87, 0x80, 0x07, 0x80, 0xaf, 0x0c, 0x00, 0x4b, 0x87, 0x80, 0x07, 0x80,
	    0xab, 0x0c, 0x00, 0x0b,
	0xb5, 0x3d, 0x00, 0x4b, 0x87, 0x80, 0x87, 0x80, 0xa5, 0x0c, 0xe4, 0x0b,
	    0xe2, 0x19, 0x00, 0x4b,
	0x87, 0x80, 0x07, 0x80, 0x02, 0x0b, 0x92, 0x3d, 0x9d, 0x0c, 0x72, 0x0b,
	    0x71, 0x19, 0x00, 0x4b,
	0x87, 0x80, 0x07, 0x80, 0x02, 0x0b, 0x92, 0x3d, 0x95, 0x0c, 0x2d, 0x0b,
	    0xc6, 0x19, 0x00, 0x4b,
	0x87, 0x80, 0x07, 0x80, 0x02, 0x0b, 0x92, 0x3d, 0x8d, 0x0c, 0x16, 0x0b,
	    0xe3, 0x19, 0x00, 0x4b,
	0x87, 0x80, 0x07, 0x80, 0x02, 0x0b, 0x92, 0x3d, 0x85, 0x0c, 0x0b, 0x0b,
	    0x71, 0x19, 0x00, 0x4b,
	0x87, 0x80, 0x07, 0x80, 0x02, 0x0b, 0x92, 0x3d, 0x01, 0x0b, 0x02, 0x0c,
	    0x04, 0x0b, 0x93, 0x19,
	0x00, 0x4b, 0x87, 0x80, 0x07, 0x80, 0x02, 0x0b, 0x92, 0x3d, 0x01, 0x0b,
	    0x02, 0x0c, 0x02, 0x0b,
	0x4a, 0x19, 0x00, 0x4b, 0x87, 0x80, 0x07, 0x80, 0x02, 0x0b, 0x92, 0x3d,
	    0x01, 0x0b, 0x02, 0x0c,
	0x01, 0x0b, 0x25, 0x19, 0x00, 0x4b, 0x87, 0x80, 0x07, 0x80, 0x02, 0x0b,
	    0x92, 0x3d, 0x01, 0x0b,
	0x02, 0x0c, 0x75, 0x19, 0x00, 0x4b, 0x87, 0x80, 0x07, 0x80, 0x02, 0x0b,
	    0x92, 0x3d, 0x01, 0x0b,
	0x02, 0x0c, 0x3b, 0x19, 0x00, 0x4b, 0x87, 0x80, 0x07, 0x80, 0x02, 0x0b,
	    0x92, 0x3d, 0x01, 0x0b,
	0x02, 0x0c, 0xc9, 0xde, 0x00, 0x2b, 0xc7, 0xb3, 0xc9, 0xde, 0x01, 0x0a,
	    0x01, 0x0b, 0xde, 0x3d,
	0x00, 0x0a, 0x01, 0x0b, 0xde, 0x3d, 0x00, 0x2b, 0x67, 0xb3, 0x6d, 0x8d,
	    0x00, 0x6d, 0x03, 0x4d,
	0x00, 0x2b, 0x05, 0x29, 0x03, 0x0c, 0x13, 0xb7, 0xfa, 0x6c, 0x37, 0x93,
	    0x0d, 0xed, 0x37, 0x93,
	0x00, 0x6d, 0x00, 0x7d, 0xa3, 0x8a, 0x03, 0x0b, 0x00, 0x1a, 0xfc, 0x5c,
	    0x13, 0x8a, 0xff, 0x0b,
	0x00, 0x1a, 0x0a, 0x93, 0x00, 0x1b, 0xb7, 0x93, 0x00, 0x2b, 0x00, 0x39,
	    0x01, 0x0b, 0x02, 0x0c,
	0x02, 0x0b, 0x92, 0x3d, 0x00, 0x0b, 0xb5, 0x3d, 0x01, 0x0b, 0x02, 0x0c,
	    0x6d, 0x8d, 0x00, 0x6d,
	0x03, 0x4d, 0x00, 0x2b, 0x05, 0x29, 0x04, 0x0c, 0x00, 0x4b, 0x23, 0xb7,
	    0xf9, 0x2c, 0x09, 0xb3,
	0x0d, 0xed, 0x0d, 0x8c, 0x6d, 0x8d, 0x00, 0x6d, 0x00, 0x0a, 0x01, 0x0b,
	    0x00, 0x7d, 0xff, 0x5d,
	0x03, 0x4d, 0x00, 0x2b, 0x05, 0x29, 0x02, 0x0c, 0x13, 0xc7, 0xf7, 0x6c,
	    0x33, 0x8a, 0x0d, 0xed,
	0x0d, 0x8c, 0x13, 0xc7, 0xfe, 0x6c, 0x33, 0x8a, 0x0d, 0x8c, 0x0a, 0x8d,
	    0x58, 0x8a, 0x01, 0x2a,
	0x0a, 0xd8, 0x0d, 0x8a, 0x0d, 0x8c, 0x0a, 0x8d, 0x58, 0x8a, 0xfe, 0x1a,
	    0x0a, 0xd8, 0x0d, 0x8a,
	0x0d, 0x8c, 0x00, 0x21, 0x00, 0x31, 0x00, 0x11, 0x0a, 0x81, 0x0d, 0x8c,
	    0x0a, 0x8d, 0x21, 0x8a,
	0xe7, 0x0b, 0xff, 0x1a, 0x01, 0x2a, 0x0a, 0xa1, 0x0d, 0x8a, 0x0d, 0x8c,
	    0x07, 0xa1, 0x0d, 0x8c,
	0x0a, 0x8d, 0x21, 0x8a, 0xe7, 0x0b, 0xfe, 0x1a, 0x08, 0x0b, 0x00, 0x2a,
	    0x0a, 0xa1, 0x0d, 0x8a,
	0x0d, 0x8c, 0x0a, 0x8d, 0x21, 0x8a, 0xe7, 0x0b, 0xfe, 0x1a, 0x10, 0x0b,
	    0x00, 0x2a, 0x0a, 0xa1,
	0x0d, 0x8a, 0x0d, 0x8c, 0x0a, 0x8d, 0x21, 0x8a, 0xe7, 0x0b, 0xfe, 0x1a,
	    0x18, 0x0b, 0x00, 0x2a,
	0x0a, 0xa1, 0x0d, 0x8a, 0x0d, 0x8c, 0x97, 0xa1, 0x0d, 0x8c, 0x17, 0xa1,
	    0x0d, 0x8c, 0xb7, 0xa1,
	0x0d, 0x8c, 0x37, 0xa1, 0x0d, 0x8c, 0xc7, 0xa1, 0x0d, 0x8c, 0x47, 0xa1,
	    0x0d, 0x8c, 0xd7, 0xa1,
	0x0d, 0x8c, 0x57, 0xa1, 0x0d, 0x8c, 0x0a, 0x8d, 0x21, 0x8a, 0x7f, 0x0b,
	    0xff, 0x1a, 0x0a, 0xa1,
	0x0d, 0x8a, 0x0d, 0x8c, 0x0a, 0x8d, 0x21, 0x8a, 0x80, 0x0b, 0x00, 0x2a,
	    0x0a, 0xa1, 0x0d, 0x8a,
	0x0d, 0x8c, 0x0a, 0x8d, 0x00, 0x78, 0x0c, 0x3c, 0x01, 0x78, 0x10, 0x3c,
	    0x02, 0x78, 0x16, 0x3c,
	0x03, 0x78, 0x1c, 0x3c, 0x04, 0x78, 0x22, 0x3c, 0x05, 0x78, 0x28, 0x3c,
	    0x1a, 0xda, 0x2e, 0x0c,
	0x21, 0x8a, 0xf8, 0x0b, 0xff, 0x1a, 0x0a, 0xa1, 0x0a, 0xda, 0x28, 0x0c,
	    0x21, 0x8a, 0xf8, 0x0b,
	0xff, 0x1a, 0x01, 0x0b, 0x00, 0x2a, 0x0a, 0xa1, 0x0a, 0xda, 0x20, 0x0c,
	    0x21, 0x8a, 0xf8, 0x0b,
	0xff, 0x1a, 0x02, 0x0b, 0x00, 0x2a, 0x0a, 0xa1, 0x0a, 0xda, 0x18, 0x0c,
	    0x21, 0x8a, 0xf8, 0x0b,
	0xff, 0x1a, 0x03, 0x0b, 0x00, 0x2a, 0x0a, 0xa1, 0x0a, 0xda, 0x10, 0x0c,
	    0x21, 0x8a, 0xf8, 0x0b,
	0xff, 0x1a, 0x04, 0x0b, 0x00, 0x2a, 0x0a, 0xa1, 0x0a, 0xda, 0x08, 0x0c,
	    0x21, 0x8a, 0xf8, 0x0b,
	0xff, 0x1a, 0x05, 0x0b, 0x00, 0x2a, 0x0a, 0xa1, 0x0a, 0xda, 0x00, 0x0c,
	    0x0d, 0x8a, 0x0d, 0x8c,
	0xa7, 0xa1, 0x0d, 0x8c, 0x27, 0xa1, 0x0d, 0x8c, 0x0d, 0x8c, 0x0d, 0x8c,
	    0x0d, 0x8c, 0x0d, 0x8c,
	0x0d, 0x8c, 0x0d, 0x8c, 0x0d, 0x8c, 0x3a, 0xda, 0x0d, 0x8c, 0x0d, 0x8c,
	    0x99, 0x06, 0x3a, 0xda,
	0x3a, 0xda, 0x3a, 0xda, 0x0d, 0x8c,
};

/* I2C function */
static int max28200_write(struct max28200_chip *chip, const char *buf,
			  int count)
{
	int ret = 0;
	int retry = 0;

	mutex_lock(&chip->i2c_rw_lock);
retry:
	ret = i2c_master_send(chip->client, buf, count);
	if (ret < 0) {
		max_dbg(PR_OEM, "max28200_write ret=%d, retry:%d\n", ret, retry);
		if (retry < 3) {
			retry++;
			goto retry;
		}
	}
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int max28200_read(struct max28200_chip *chip, char *buf, int count)
{
	int ret = 0;

	mutex_lock(&chip->i2c_rw_lock);
	ret = i2c_master_recv(chip->client, buf, count);
	mutex_unlock(&chip->i2c_rw_lock);
	if (ret < 0)
		max_dbg(PR_OEM, "max28200_read ret=%d\n", ret);

	return ret;
}

/**
 *@brief	Load a image of a size into the MAX28200 flash.
 *         Parameters:
 * 		  image       : pointer to start of image
 *           sizeOfImage : size of image to save in flash
 *           loadAddress : byte address within flash to start flashing
 */
static int max28200_LoadVerify(struct max28200_chip *chip, u8 *image,
			       int sizeOfImage, u16 loadAddress)
{
	int i;
	u8 val;
	int status;
	int index;
	int address;
	int payloadLength;

	index = 0;
	address = loadAddress;
	payloadLength = LOADER_PAYLOAD_LENGTH;
	while (index < sizeOfImage) {

		//
		// Load and Verify Code
		//
		val = MAX28200_CMD_LOADVERIFY;
		status = max28200_write(chip, &val, 1);
		if (status < 0)
			return status;
		// Byte Count
		val = (u8) payloadLength;
		//max_dbg(PR_FW, "byte count %02x ",val);
		status = max28200_write(chip, &val, 1);
		if (status < 0)
			return status;
		// Low Address
		val = (u8) (address & 0xff);
		//max_dbg(PR_FW, "lo %02x ",val);
		status = max28200_write(chip, &val, 1);
		if (status < 0)
			return status;
		// High Address
		val = (u8) ((address >> 8) & 0xff);
		//max_dbg(PR_FW, "hi %02x ",val);
		status = max28200_write(chip, &val, 1);
		if (status < 0)
			return status;

		for (i = 0; i < payloadLength; i++) {
			val = image[index++];
			//max_dbg(PR_FW, "%02x ",val);
			status = max28200_write(chip, &val, 1);
			if (status < 0)
				return status;
		}

		// advance the address
		address += payloadLength;

		// S 55 [3E*] P
		// read the prompt
		status = max28200_read(chip, &val, 1);
		if (status < 0)
			return status;
		if (val != MAX28200_RESP_VALUEPROMPT)
			return STATUS_INVALID_PROMPT_ERROR;

		// Get Status
		// S 54 04 P
		//max_dbg(PR_FW, "get status...\n");
		val = MAX28200_CMD_GETSTATUS;
		status = max28200_write(chip, &val, 1);
		if (status < 0)
			return status;
		// S 55 [04*] P
		status = max28200_read(chip, &val, 1);
		if (status < 0)
			return status;
		if (val != LOADER_EXPECTED_STATUS)
			return STATUS_VERIFY_FAILED_ERROR;
		// S 55 [04*] P
		status = max28200_read(chip, &val, 1);
		if (status < 0)
			return status;
		if (val != MAX28200_LOADER_ERROR_NONE)
			return STATUS_VERIFY_FAILED_ERROR;

		// S 55 [3E*] P
		// read the prompt
		status = max28200_read(chip, &val, 1);
		if (status < 0)
			return status;
		if (val != MAX28200_RESP_VALUEPROMPT)
			return STATUS_INVALID_PROMPT_ERROR;
	}
	return 0;
}

static int max28200_reset(struct max28200_chip *chip)
{
	// assert the reset low
	pinctrl_select_state(chip->max28200_pinctrl, chip->max_reset_low);
	// delay
	usleep_range(DELAY_RESET_MS, DELAY_RESET_MS + 100);

	// assert the reset high
	pinctrl_select_state(chip->max28200_pinctrl, chip->max_reset_high);
	// delay
	usleep_range(DELAY_AFTER_RESET_US, DELAY_AFTER_RESET_US + 10);

	return 0;
}

static int max28200_read_version(struct max28200_chip *chip)
{
	u8 val;
	u8 buf[2];
	int rc;

	val = MAX28200_FW_VERSION_CMD;
	rc = max28200_write(chip, &val, 1);
	if (rc < 0) {
		max_dbg(PR_OEM, "write fw version failed:%d\n", rc);
		return rc;
	}

	rc = max28200_read(chip, buf, 2);
	if (rc < 0) {
		max_dbg(PR_OEM, "read fw version failed:%d\n", rc);
		return rc;
	}
	max_dbg(PR_OEM, "read the fw version:%x\n", buf[1] << 8 | buf[0]);

	chip->version = (buf[1] << 8 | buf[0]);

	return chip->version;
}

static int max28200_set_time(struct max28200_chip *chip, int time)
{
	int rc;
	u8 val;

	val = time;
	rc = max28200_write(chip, &val, 1);
	if (rc < 0) {
		max_dbg(PR_OEM, "set time failed:%d\n", rc);
		return rc;
	}
	max_dbg(PR_OEM, "set time watchdog time to:%d\n", time);

	return rc;
}

static int max28200_cmd_set_en(struct max28200_chip *max, bool enable)
{
	int rc, retry = 0;
	char val;

	if (enable) {
		val = MAX28200_CTRL_P0_HIG;
	} else {
		val = MAX28200_CTRL_P0_LOW;
	}

	rc = max28200_write(max, &val, 1);
	if (rc < 0) {
		max_dbg(PR_OEM, "watchdog enable cp failed:%d retry:%d\n", rc, retry);
		return rc;
	}

	return 0;
}

static int max28200_reset_watchdog(struct max28200_chip *chip)
{
	int rc;
	u8 val;

	val = MAX28200_RESET_WATCHDOG;
	rc = max28200_write(chip, &val, 1);
	if (rc < 0) {
		max_dbg(PR_OEM, "reset watchdog failed:%d\n", rc);
		return rc;
	}
	max_dbg(PR_DEBUG, "reset watchdog time\n");

	return rc;
}

static int max28200_program(struct max28200_chip *chip, u8 *image,
			    int sizeOfImage)
{
	u8 val;
	int status;
	//int simulatedError_PageNumber;

	max28200_reset(chip);
	//
	// Send the Magic Value
	//
	max_dbg(PR_FW, "step1: max28200 send the magic value...\n");
	val = MAX28200_MAGIC_VALUE;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 55 [3E*] P
	// read the prompt
	usleep_range(75, 80);	// it is very importent
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != MAX28200_RESP_VALUEPROMPT)
		return STATUS_INVALID_PROMPT_ERROR;

	//
	// NOP
	// S 54 00 P
	//
	val = MAX28200_CMD_NOP;
	max_dbg(PR_FW, "step2: max28200_program send nop...\n");
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 55 [3E*] P
	// read the prompt
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != MAX28200_RESP_VALUEPROMPT)
		return STATUS_INVALID_PROMPT_ERROR;

	//
	// Master Erase
	// S 54 02 P
	//
	max_dbg(PR_FW, "step3: max28200_program master erase...\n");
	val = MAX28200_CMD_MASTERERASE;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;

	// delay after issuing master erase
	usleep_range(DELAY_MASTERERASE_MS, DELAY_MASTERERASE_MS + 100);

	// S 55 [3E*] P
	// read the prompt
	max_dbg(PR_FW, "step4: max28200_program read prompt...\n");
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != MAX28200_RESP_VALUEPROMPT)
		return STATUS_INVALID_PROMPT_ERROR;

	//
	// Get Status
	// S 54 04 P
	//
	max_dbg(PR_FW, "step5: max28200_program get status...\n");
	val = MAX28200_CMD_GETSTATUS;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 55 [04*] P
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != LOADER_EXPECTED_STATUS)
		return STATUS_INVALID_RESULT_ERROR;
	// S 55 [00*] P
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != 0x00)
		return STATUS_INVALID_RESULT_ERROR;
	// S 55 [3E*] P
	// read the prompt
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != MAX28200_RESP_VALUEPROMPT)
		return STATUS_INVALID_PROMPT_ERROR;

	//
	// Bogus Command
	// S 54 55 P
	//
	max_dbg(PR_FW, "step6: max28200_program bogus command...\n");
	val = 0x55;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 54 00 P
	val = 0x00;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 54 00 P
	val = 0x00;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 54 00 P
	val = 0x00;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 55 [3E*] P
	// read the prompt
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != MAX28200_RESP_VALUEPROMPT)
		return STATUS_INVALID_PROMPT_ERROR;

	//
	// Get Status
	// S 54 04 P
	//
	max_dbg(PR_FW, "step7: max28200_program get status...\n");
	val = MAX28200_CMD_GETSTATUS;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 55 [04*] P
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != LOADER_EXPECTED_STATUS)
		return STATUS_INVALID_RESULT_ERROR;
	//S 55 [01*] P
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != 0x01)
		return STATUS_INVALID_RESULT_ERROR;
	// S 55 [3E*] P
	// read the prompt
	max_dbg(PR_FW, "step8: max28200_program read the prompt...\n");
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != MAX28200_RESP_VALUEPROMPT)
		return STATUS_INVALID_PROMPT_ERROR;

	//
	// Set Multiplier - This value needs to be set such that bytes written multiplied by
	//                  4 gives the actual desired length in the length byte for load command.
	// S 54 0B P
	max_dbg(PR_FW, "step9: max28200_program set multiplier...\n");
	val = MAX28200_CMD_MULTIPLIER;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 54 00 P
	val = 0x00;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 54 04 P
	val = MAX28200_PARAM_MULTIPLIER;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 54 00 P
	val = 0x00;
	status = max28200_write(chip, &val, 1);
	if (status < 0)
		return status;
	// S 55 [3E*] P
	// read the prompt
	status = max28200_read(chip, &val, 1);
	if (status < 0)
		return status;
	if (val != MAX28200_RESP_VALUEPROMPT)
		return STATUS_INVALID_PROMPT_ERROR;
	//
	// load the image
	//
	status =
	    max28200_LoadVerify(chip, image, sizeOfImage,
				LOADER_PROGRAM_ADDRESS);
	if (status < 0)
		return status;
	max_dbg(PR_FW, "step10: max28200_program max28200_LoadVerify ok\n");

	max28200_reset(chip);
	usleep_range(1000000, 1100000);
	val = max28200_read_version(chip);
	if (val == MAX28200_SW_VERSION) {
		max_dbg(PR_OEM, "max28200_program success\n");
		chip->fw_ok = true;
	} else {
		max_dbg(PR_OEM, "max28200_program error val = %x\n", val);
	}

	return 0;
}

static bool max28200_set_watchdog_enable(struct max28200_chip *max, bool enable)
{
	int rc;

	if (max->use_pin_en) {
		if (enable) {
			max_dbg(PR_OEM,
				"pinctrl_select_state to max_pin_en_on\n");
			rc = pinctrl_select_state(max->max28200_pinctrl,
						  max->max_pin_en_on);
			if (rc < 0) {
				dev_err(max->dev,
					"fail to select max_pin_en_on rc=%d\n",
					rc);
				return rc;
			}
		} else {
			max_dbg(PR_OEM,
				"pinctrl_select_state to max_pin_en_off\n");
			rc = pinctrl_select_state(max->max28200_pinctrl,
						  max->max_pin_en_off);
			if (rc < 0) {
				dev_err(max->dev,
					"fail to select max_pin_en_off rc=%d\n",
					rc);
				return rc;
			}
		}
	} else {
		if (!max->fw_ok && enable) {
			max->early_enable = true;
			return 0;
		}

		if (enable)
			max28200_set_time(max, MAX28200_SET_TIME_10S);
		max28200_cmd_set_en(max, enable);
		schedule_delayed_work(&max->monitor_work, msecs_to_jiffies(5000));
		max_dbg(PR_OEM, "set watchdog enable cp:%d\n", enable);

	}
	max->enabled = enable;

	return rc;
}

static int max28200_get_watchdog_enable(struct max28200_chip *max)
{
	return max->enabled;
}

static enum power_supply_property max28200_props[] = {
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_CHIP_OK,
};

static int max28200_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct max28200_chip *max = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		val->intval = max28200_get_watchdog_enable(max);
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		val->intval = (max->version == MAX28200_SW_VERSION) && max->fw_ok;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max28200_set_property(struct power_supply *psy,
				 enum power_supply_property prop,
				 const union power_supply_propval *val)
{
	struct max28200_chip *max = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		max28200_set_watchdog_enable(max, !!val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max28200_prop_is_writeable(struct power_supply *psy,
				      enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int max28200_psy_register(struct max28200_chip *max)
{
	struct power_supply_config max28200_psy_cfg = { };

	max->max28200_psy_d.name = "cp_wdog";
	max->max28200_psy_d.type = POWER_SUPPLY_TYPE_BMS;
	max->max28200_psy_d.properties = max28200_props;
	max->max28200_psy_d.num_properties = ARRAY_SIZE(max28200_props);
	max->max28200_psy_d.get_property = max28200_get_property;
	max->max28200_psy_d.set_property = max28200_set_property;
	max->max28200_psy_d.property_is_writeable = max28200_prop_is_writeable;

	max28200_psy_cfg.drv_data = max;
	max28200_psy_cfg.num_supplicants = 0;
	max->max28200_psy = devm_power_supply_register(max->dev,
						       &max->max28200_psy_d,
						       &max28200_psy_cfg);
	if (IS_ERR(max->max28200_psy)) {
		max_dbg(PR_OEM, "Failed to register max28200_psy");
		return PTR_ERR(max->max28200_psy);
	}

	return 0;
}

static int max28200_pinctrl_init(struct max28200_chip *chip)
{
	int retval = 0;
	/* Get pinctrl if target uses pinctrl */
	chip->max28200_pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->max28200_pinctrl)) {
		retval = PTR_ERR(chip->max28200_pinctrl);
		dev_err(chip->dev, "Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	chip->max_reset_low
	    = pinctrl_lookup_state(chip->max28200_pinctrl, "max_reset_low");
	if (IS_ERR_OR_NULL(chip->max_reset_low)) {
		retval = PTR_ERR(chip->max_reset_low);
		dev_err(chip->dev, "Can not lookup %s pinstate %d\n",
			"max_reset_low", retval);
		goto err_pinctrl_lookup;
	}

	chip->max_reset_high
	    = pinctrl_lookup_state(chip->max28200_pinctrl, "max_reset_high");
	if (IS_ERR_OR_NULL(chip->max_reset_high)) {
		retval = PTR_ERR(chip->max_reset_high);
		dev_err(chip->dev, "Can not lookup %s pinstate %d\n",
			"max_reset_high", retval);
		goto err_pinctrl_lookup;
	}

	if (chip->use_pin_en) {
		chip->max_pin_en_on
		    =
		    pinctrl_lookup_state(chip->max28200_pinctrl,
					 "max_pin_en_on");
		if (IS_ERR_OR_NULL(chip->max_pin_en_on)) {
			retval = PTR_ERR(chip->max_pin_en_on);
			dev_err(chip->dev, "Can not lookup %s pinstate %d\n",
				"max_pin_en_on", retval);
		}
		chip->max_pin_en_off
		    =
		    pinctrl_lookup_state(chip->max28200_pinctrl,
					 "max_pin_en_off");
		if (IS_ERR_OR_NULL(chip->max_pin_en_off)) {
			retval = PTR_ERR(chip->max_pin_en_off);
			dev_err(chip->dev, "Can not lookup %s pinstate %d\n",
				"max_pin_en_off", retval);
		}
	}

	pinctrl_select_state(chip->max28200_pinctrl, chip->max_reset_low);

	return 0;
      err_pinctrl_lookup:
	devm_pinctrl_put(chip->max28200_pinctrl);
      err_pinctrl_get:
	chip->max28200_pinctrl = NULL;
	return retval;
}

static void max28200_monitor_work_fn(struct work_struct *work)
{
	struct max28200_chip *max = container_of(work,
						 struct max28200_chip,
						 monitor_work.work);

	if (max->enabled) {
		max28200_reset_watchdog(max);
		schedule_delayed_work(&max->monitor_work, msecs_to_jiffies(5000));
	}
}

static void max28200_firmware_download_work_fn(struct work_struct *work)
{
	struct max28200_chip *chip =
	    container_of(work, struct max28200_chip,
			 firmware_download_work.work);
	max_dbg(PR_OEM, "max28200_firmware_download_work_fn\n");

	if (max28200_program
	    (chip, max28200_firmware, MAX28200_FIRMWARE_MAXBYTES))
		schedule_delayed_work(&chip->firmware_download_work,
				      msecs_to_jiffies(3000));
}

static void max28200_sw_check_work_fn(struct work_struct *work)
{
	struct max28200_chip *chip =
	    container_of(work, struct max28200_chip, sw_check_work.work);
	u8 val = 0x00;

	max_dbg(PR_OEM, "max28200_sw_check_work_fn\n");

	max28200_reset(chip);
	usleep_range(1000000, 1100000);
	val = max28200_read_version(chip);
	if (val != MAX28200_SW_VERSION) {
		max_dbg(PR_OEM,
			"max28200_probe get sw version failed need download firmware\n");
		schedule_delayed_work(&chip->firmware_download_work,
				      msecs_to_jiffies(3000));
	} else {
		chip->fw_ok = true;;
		if (chip->early_enable) {
			max_dbg(PR_OEM, "enable the watchdog\n");
			max28200_set_watchdog_enable(chip, true);
		}
		max_dbg(PR_OEM, "max28200 probe successfully\n");
	}
}

static irqreturn_t max28200_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static int max28200_parse_dt(struct max28200_chip *chip)
{
	struct device_node *dNode = chip->dev->of_node;
	int ret;

	if (dNode == NULL)
		return -ENODEV;

	chip->use_pin_en = of_property_read_bool(dNode, "max,use_pin_en");
	max_dbg(PR_OEM, "use_pin_en:%d\n", chip->use_pin_en);

	chip->watchdog_int = of_get_named_gpio(dNode, "watchdog_int", 0);
	if (chip->watchdog_int < 0) {
		max_dbg(PR_OEM, "%s - get watchdog_int error\n", __func__);
		return -ENODEV;
	}
	ret = gpio_request(chip->watchdog_int, "watchdog_int");
	chip->watchdog_irq = gpio_to_irq(chip->watchdog_int);
	request_threaded_irq(chip->watchdog_irq,
			     NULL, max28200_irq_handler,
			     IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			     "watchdog_irq", chip);

	return 0;
}

static int max28200_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct max28200_chip *chip;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);

	if (!chip)
		return -ENOMEM;

	chip->dev = &client->dev;
	chip->client = client;

	i2c_set_clientdata(client, chip);

	mutex_init(&chip->i2c_rw_lock);

	max28200_parse_dt(chip);
	max28200_psy_register(chip);
	max28200_pinctrl_init(chip);

	INIT_DELAYED_WORK(&chip->firmware_download_work,
			  max28200_firmware_download_work_fn);
	INIT_DELAYED_WORK(&chip->sw_check_work, max28200_sw_check_work_fn);
	INIT_DELAYED_WORK(&chip->monitor_work, max28200_monitor_work_fn);

	if (!chip->use_pin_en)
		schedule_delayed_work(&chip->sw_check_work, 0);

	max_dbg(PR_OEM, "max28200 probe sucess\n");

	return 0;
}

static struct of_device_id max28200_match_table[] = {
	{.compatible = "maxim,max28200",},
	{},
};

MODULE_DEVICE_TABLE(of, max28200_match_table);

static const struct i2c_device_id max28200_id[] = {
	{"max28200", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, max28200_id);

static const struct dev_pm_ops max28200_pm_ops = {
	.resume = NULL,
	.suspend_noirq = NULL,
	.suspend = NULL,
};

static struct i2c_driver max_driver = {
	.driver = {
		   .name = "max28200",
		   .owner = THIS_MODULE,
		   .of_match_table = max28200_match_table,
		   .pm = &max28200_pm_ops,
		   },
	.id_table = max28200_id,
	.probe = max28200_probe,
	.remove = NULL,
	.shutdown = NULL,

};

module_i2c_driver(max_driver);

MODULE_DESCRIPTION("Maxim max28200 Driver");
MODULE_LICENSE("GPL v2");
