#ifndef __AW22XXX_H__
#define __AW22XXX_H__

/*********************************************************
 *
 * i2c
 *
 ********************************************************/
#define MAX_I2C_BUFFER_SIZE         65536

#define AW22XXX_FLASH_I2C_WRITES
#define MAX_FLASH_WRITE_BYTE_SIZE   128

#define AWINIC_FW_UPDATE_DELAY
/*********************************************************
 *
 * chip info
 *
 ********************************************************/
#define AW22XXX_SRSTR       0x76
#define AW22XXX_SRSTW       0x55
#define AW22118_CHIPID      0x18
#define AW22127_CHIPID      0x27

#define AW22XXX_RGB_MAX     9

enum aw22xxx_chipids {
	AW22XXX = 0,
	AW22118 = 1,
	AW22127 = 2,
};

enum aw22xxx_fw_flags {
	AW22XXX_FLAG_FW_NONE = 0,
	AW22XXX_FLAG_FW_UPDATE = 1,
	AW22XXX_FLAG_FW_OK = 2,
	AW22XXX_FLAG_FW_FAIL = 3,
};

enum aw22xxx_flags {
	AW22XXX_FLAG_NONE = 0,
	AW22XXX_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw22xxx_reg_page {
	AW22XXX_REG_PAGE0 = 0,
	AW22XXX_REG_PAGE1 = 1,
	AW22XXX_REG_PAGE2 = 2,
	AW22XXX_REG_PAGE3 = 3,
	AW22XXX_REG_PAGE4 = 4,
};

enum aw22xxx_dbgctr {
	AW22XXX_DBGCTR_NORMAL = 0,
	AW22XXX_DBGCTR_IRAM = 1,
	AW22XXX_DBGCTR_SFR = 2,
	AW22XXX_DBGCTR_FLASH = 3,
	AW22XXX_DBGCTR_MAX = 4,
};

enum aw22xxx_imax {
	AW22XXX_IMAX_2mA = 8,
	AW22XXX_IMAX_3mA = 0,
	AW22XXX_IMAX_4mA = 9,
	AW22XXX_IMAX_6mA = 1,
	AW22XXX_IMAX_9mA = 2,
	AW22XXX_IMAX_10mA = 11,
	AW22XXX_IMAX_15mA = 3,
	AW22XXX_IMAX_20mA = 12,
	AW22XXX_IMAX_30mA = 4,
	AW22XXX_IMAX_40mA = 14,
	AW22XXX_IMAX_45mA = 5,
	AW22XXX_IMAX_60mA = 6,
	AW22XXX_IMAX_75mA = 7,
};

/*********************************************************
 *
 * cfg
 *
 ********************************************************/
#define AW22XXX_AUDIO_INITGAIN      0x1F

/*********************************************************
 *
 * struct
 *
 ********************************************************/
struct aw22xxx_container {
	unsigned int len;
	unsigned int version;
	unsigned int bist;
	unsigned int key;
	unsigned char data[];
};

struct aw22xxx {
	struct i2c_client *i2c;
	struct device *dev;
	struct led_classdev cdev;
	struct work_struct brightness_work;
	struct work_struct task_work;
	struct work_struct fw_work;
	struct work_struct cfg_work;
#ifdef AWINIC_FW_UPDATE_DELAY
	struct hrtimer fw_timer;
#endif
	struct mutex cfg_lock;

	int reset_gpio;
	int irq_gpio;

	unsigned char flags;
	unsigned char chipid;
	unsigned char fw_update;
	unsigned char fw_flags;

	unsigned int imax;
	unsigned int fw_version;

	unsigned char task0;
	unsigned char task1;

	unsigned char effect;
	unsigned char cfg;

	unsigned int rgb[AW22XXX_RGB_MAX];
};

#endif

