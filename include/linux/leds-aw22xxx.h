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
#define AW22XXX_SRSTR              0x76
#define AW22XXX_SRSTW              0x55
#define AW22118_CHIPID             0x18
#define AW22127_CHIPID             0x27

#define AW22XXX_RGB_MAX            9

#define AW22XXX_RED                0x02
#define AW22XXX_GREEN              0x03
#define AW22XXX_BLUE               0x01

#define AW22XXX_RGB_INDEX          31
#define AW22XXX_BRIGHTNESS_INDEX   27

#define AW22XXX_DELAY_ON_LOW       31
#define AW22XXX_DELAY_ON_HIGH      29
#define AW22XXX_DELAY_OFF_LOW      25
#define AW22XXX_DELAY_OFF_HIGH     23

#define AW22XXX_IMAX_LIMIT         7  //LIMIT 20MA

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
	struct led_classdev red_cdev;
	struct led_classdev green_cdev;
	struct led_classdev blue_cdev;
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

	unsigned char frq;

	unsigned int rgb[AW22XXX_RGB_MAX];
};

static unsigned char aw22xxx_init_code[]={
	0xff,0x00,//page0
	0x05,0x00,//task0
	0x04,0x01,//mcuctr
	0x02,0x00,//gcr
	0x02,0x01,//gcr
	0x04,0x01,//mcuctr
	0x0c,0x00,//audctr
	0xff,0x01,//page1
	0x00,0x00,//mode
	0x01,0x00,//breath[0]
	0x02,0x00,//breath[1]
	0x03,0x00,//breath[2]
	0x05,0x03,//com
	0x06,0x09,//channel
	0x0b,0x00,//linectrl
	0x2c,0x00,//pwm_max
	0x10,0x00,//iled[0]
	0x11,0x00,//iled[1]
	0x12,0x00,//iled[2]
	0x13,0x00,//iled[3]
	0x14,0x00,//iled[4]
	0x15,0x00,//iled[5]
	0x16,0x00,//iled[6]
	0x17,0x00,//iled[7]
	0x18,0x00,//iled[8]
	0x19,0x00,//iled[6]
	0x1A,0x00,//iled[7]
	0x1B,0x00,//iled[8]
	0x1c,0x00,//iled[12]
	0x1d,0x00,//iled[13]
	0x1e,0x00,//iled[14]
	0x1f,0x00,//iled[15]
	0x20,0x00,//iled[16]
	0x21,0x00,//iled[17]
	0x22,0x00,//iled[15]
	0x23,0x00,//iled[16]
	0x24,0x00,//iled[17]
	0x25,0x00,//iled[15]
	0x26,0x00,//iled[16]
	0x27,0x00,//iled[17]
	0x28,0x00,//iled[15]
	0x29,0x00,//iled[16]
	0x2a,0x00,//iled[17]
	0xff,0x00,//page0
	0x05,0x02,//task0
	0x04,0x03,//mcuctr
};

static unsigned char aw22xxx_led_rgb_code[] = {
	0xff,0x00,//page0
	0x05,0x02,
	0x02,0x01,//gcr
	0xff,0x01,//page1
	0x05,0x03,//com
	0x06,0x09,//channel
	0x0b,0x00,//linectrl
	0x2c,0xff,//pwm_max
	0xff,0x00,//page0
	0x05,0x02,//task0
	0x04,0x03,//mcuctr
	0x06,0x06,
	0x04,0x07,
	0x06,0x3f,//brightness
	0x04,0x07,
	0x06,0x01,//blue
	0x04,0x07,
};

static unsigned char aw22xxx_led_blink_code[] ={
	0xff,0x00,
	0x05,0x00,
	0x04,0x01,
	0x02,0x00,
	0x02,0x01,
	0x04,0x01,
	0x0c,0x00,
	0xff,0x01,
	0x00,0x04,
	0x05,0x03,
	0x06,0x09,
	0x07,0x00,//off high
	0x08,0x00,//off low
	0x0b,0x01,
	0x2b,0x00,//on high
	0x2c,0x8f,//on low
	0xff,0x00,
	0x05,0x02,
	0x04,0x03,
};

#endif
