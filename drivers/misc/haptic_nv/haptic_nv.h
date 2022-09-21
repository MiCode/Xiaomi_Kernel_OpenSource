#ifndef _HAPTIC_NV_H_
#define _HAPTIC_NV_H_

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/syscalls.h>

/*********************************************************
 *
 * kernel marco
 *
 ********************************************************/
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 4, 1)
#define TIMED_OUTPUT
#endif

#ifdef TIMED_OUTPUT
#include <../../../drivers/staging/android/timed_output.h>
typedef struct timed_output_dev cdev_t;
#else
typedef struct led_classdev cdev_t;
#endif

/*********************************************************
 *
 * normal marco
 *
 ********************************************************/
#define HAPTIC_NV_I2C_NAME		("awinic_haptic_l")
#define HAPTIC_NV_AWRW_SIZE		(220)
#define HAPTIC_NV_CHIPID_RETRIES	(5)
#define HAPTIC_NV_I2C_RETRIES		(2)
#define HAPTIC_NV_REG_ID		(0x00)
#define AW8624_CHIP_ID			(0x24)
#define AW8622X_CHIP_ID			(0x00)
#define AW86214_CHIP_ID			(0x01)
#define AW862XX_REG_EFRD9		(0x64)
#define REG_NONE_ACCESS			(0)
#define REG_RD_ACCESS			(1 << 0)
#define REG_WR_ACCESS			(1 << 1)
#define AW_REG_MAX			(0xFF)
#define AW_RAMDATA_RD_BUFFER_SIZE	(1024)
#define AW_RAMDATA_WR_BUFFER_SIZE	(2048)
#define AW_GUN_TYPE_DEF_VAL		(0xFF)
#define AW_BULLET_NR_DEF_VAL		(0)
#define AW_I2C_READ_MSG_NUM		(2)
#define AW_I2C_BYTE_ONE			(1)
#define AW_I2C_BYTE_TWO			(2)
#define AW_I2C_BYTE_THREE		(3)
#define AW_I2C_BYTE_FOUR		(4)
#define AW_I2C_BYTE_FIVE		(5)
#define AW_I2C_BYTE_SIX			(6)
#define AW_I2C_BYTE_SEVEN		(7)
#define AW_I2C_BYTE_EIGHT		(8)
#define AWRW_CMD_UNIT			(5)
#define AW_SET_RAMADDR_H(addr)		((addr) >> 8)
#define AW_SET_RAMADDR_L(addr)		((addr) & 0x00FF)
#define AW_SET_BASEADDR_H(addr)		((addr) >> 8)
#define AW_SET_BASEADDR_L(addr)		((addr) & 0x00FF)
/*********************************************************
 *
 * macro control
 *
 ********************************************************/
#define ENABLE_PIN_CONTROL
#define AW_CHECK_RAM_DATA
#define AW_READ_BIN_FLEXBALLY
#define AW_ENABLE_RTP_PRINT_LOG
/* #define AW8624_MUL_GET_F0 */

#define aw_dev_err(format, ...) \
			pr_err("[haptic_nv_l]" format, ##__VA_ARGS__)

#define aw_dev_info(format, ...) \
			pr_info("[haptic_nv_l]" format, ##__VA_ARGS__)

#define aw_dev_dbg(format, ...) \
			pr_debug("[haptic_nv_l]" format, ##__VA_ARGS__)
/*********************************************************
 *
 * enum
 *
 ********************************************************/
enum haptic_nv_chip_name {
	AW_CHIP_NULL = 0,
	AW86223 = 1,
	AW86224_5 = 2,
	AW86214 = 3,
	AW8624 = 4,
};

enum haptic_nv_read_chip_type {
	HAPTIC_NV_FIRST_TRY = 0,
	HAPTIC_NV_LAST_TRY = 1,
};

enum aw862xx_ef_id {
	AW86223_EF_ID = 0x01,
	AW86224_5_EF_ID = 0x00,
	AW86214_EF_ID = 0x41,
};
/*********************************************************
 *
 * struct
 *
 ********************************************************/
struct awinic {
	bool IsUsedIRQ;
	unsigned char name;

	unsigned int aw862xx_i2c_addr;
	int reset_gpio;
	int pdlcen_gpio;
	int irq_gpio;
	int reset_gpio_ret;
	int irq_gpio_ret;
    int enable_pin_control;
	struct i2c_client *i2c;
	struct device *dev;
	struct aw8624 *aw8624;
	struct aw8622x *aw8622x;
	struct aw86214 *aw86214;
#ifdef ENABLE_PIN_CONTROL
	struct pinctrl *awinic_pinctrl;
	struct pinctrl_state *pinctrl_state[5];
#endif
};

struct ram {
	unsigned int len;
	unsigned int check_sum;
	unsigned int base_addr;
	unsigned char version;
	unsigned char ram_shift;
	unsigned char baseaddr_shift;
	unsigned char ram_num;
};

struct haptic_ctr {
	unsigned char cnt;
	unsigned char cmd;
	unsigned char play;
	unsigned char wavseq;
	unsigned char loop;
	unsigned char gain;
	struct list_head list;
};

struct haptic_audio {
	struct mutex lock;
	struct hrtimer timer;
	struct work_struct work;
	int delay_val;
	int timer_val;
	struct haptic_ctr ctr;
	struct list_head ctr_list;
};

struct aw_i2c_package {
	unsigned char flag;
	unsigned char reg_num;
	unsigned char first_addr;
	unsigned char reg_data[HAPTIC_NV_AWRW_SIZE];
};
#endif
