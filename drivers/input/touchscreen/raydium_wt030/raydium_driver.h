/* drivers/input/touchscreen/raydium_wt030/raydium_driver.h
 *
 * Raydium TouchScreen driver.
 *
 * Copyright (c) 2010  Raydium tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_RAYDIUM_H
#define __LINUX_RAYDIUM_H
#define RAYDIUM_NAME "raydium_ts"
#define COORDS_ARR_SIZE    4
#define I2C_VTG_MIN_UV		1650000
#define I2C_VTG_MAX_UV		1950000
#define VDD_VTG_MIN_UV		1650000
#define VDD_VTG_MAX_UV		1950000
#define RAD_MAIN_VERSION	0x01
#define RAD_MINOR_VERSION	0x07
#define RAD_CUSTOMER_VERSION	0x0100

#if defined(CONFIG_TOUCHSCREEN_RAYDIUM_CHIPSET)
/* IC timing control arguments */
#define RAYDIUM_POWERON_DELAY_USEC    500
#define RAYDIUM_RESET_INTERVAL_MSEC   5
#define RAYDIUM_RESET_RESTORE_USEC    200
#define RAYDIUM_RESET_DELAY_MSEC      100

/* I2C bus slave address(ID) */
#define RAYDIUM_I2C_EID    (0x5A)
#define RAYDIUM_I2C_NID    (0x39)

/* I2C R/W configuration literal */
#define RAYDIUM_I2C_WRITE       I2C_SMBUS_WRITE
#define RAYDIUM_I2C_READ        I2C_SMBUS_READ
#define SYN_I2C_RETRY_TIMES     2
#define MAX_WRITE_PACKET_SIZE   64
#define MAX_READ_PACKET_SIZE    64

/* PDA address and bit definition*/
#define RAD_READ_FT_DATA_CMD        0x2000019C
/* 1byte, disable:0x00 ; enable:0x20*/
#define RAD_GESTURE_STATE_CMD       0x200005F4
#define RAD_GESTURE_DISABLE         0x00
#define RAD_GESTURE_ENABLE          0x20
/* 4bytes, [0]:ready ; [1]:type ; [2]:direction*/
#define RAD_GESTURE_RESULT_CMD      0x200005F0
#define RAD_CHK_I2C_CMD				0x500009BC
#define RAD_PDA2_CTRL_CMD           0x50000628
#define RAD_ENABLE_PDA2             0x04
#define RAD_ENABLE_SI2				0x02

/* PDA literal */
#define MASK_8BIT    0xFF
#define RAD_I2C_PDA_ADDRESS_LENGTH    4
#define PDA_MODE     1
#define PDA2_MODE    2
#define RAD_I2C_PDA_MODE_DISABLE      0x00
#define RAD_I2C_PDA_MODE_ENABLE       0x80
/* Using byte mode due to data might be not word-aligment */
#define RAD_I2C_PDA_MODE_WORD_MODE    0x40
#define RAD_I2C_PDA_2_MODE_DISABLE    0x20
#define RAD_PALM_DISABLE    0x00
#define RAD_PALM_ENABLE     0x01
#define RAD_WAKE_UP			0x02
#define RAYDIUM_TEST_FW	0x80
#define RAYDIUM_TEST_PARA	0x40
#define RAYDIUM_BOOTLOADER	0x20
#define RAYDIUM_FIRMWARE	0x10
#define RAYDIUM_PARA		0x08
#define RAYDIUM_COMP		0x04
#define RAYDIUM_BASELINE	0x02
#define RAYDIUM_INIT		0x01
#define FAIL          0
#define ERROR        -1
#define SUCCESS       1
#define DISABLE       0
#define ENABLE        1

/* PDA2 setting */
/* Page 0 ~ Page A */
#define MAX_PAGE_AMOUNT    11

/* PDA2 address and setting definition*/
#define RAYDIUM_PDA2_TCH_RPT_STATUS_ADDR    0x00    /* only in Page 0 */
#define RAYDIUM_PDA2_TCH_RPT_ADDR           0x01    /* only in Page 0 */
#define RAYDIUM_PDA2_HOST_CMD_ADDR          0x02    /* only in Page 0 */
#define RAYDIUM_PDA2_PALM_AREA_ADDR         0x03    /* only in Page 0 */
#define RAYDIUM_PDA2_GESTURE_RPT_ADDR       0x04    /* only in Page 0 */
#define RAYDIUM_PDA2_PALM_STATUS_ADDR       0x05    /* only in Page 0 */
#define RAYDIUM_PDA2_FW_VERSION_ADDR        0x06    /* only in Page 0 */
#define RAYDIUM_PDA2_PANEL_VERSION_ADDR     0x07    /* only in Page 0 */
#define RAYDIUM_PDA2_DISPLAY_INFO_ADDR		0x08    /* only in Page 0 */
#define RAYDIUM_PDA2_PDA_CFG_ADDR           0x09    /* only in Page 0 */
#define RAYDIUM_PDA2_RAWDATA_ADDR           0x0B    /* only in Page 0 */
/* Page 0 ~ Page 9 will be directed to Page 0 */
#define RAYDIUM_PDA2_PAGE_ADDR              0x0A
#define RAYDIUM_PDA2_PAGE_0                 0x00
/* temporary switch to PDA once */
#define RAYDIUM_PDA2_ENABLE_PDA             0x0A
/* permanently switch to PDA mode */
#define RAYDIUM_PDA2_2_PDA                  (MAX_PAGE_AMOUNT + 2)

/* Raydium host cmd */
#define RAYDIUM_HOST_CMD_NO_OP              0x00
#define RAYDIUM_HOST_CMD_PWR_SLEEP          0x30
#define RAYDIUM_HOST_CMD_DISPLAY_MODE	0x33
#define RAYDIUM_HOST_CMD_CALIBRATION        0x5C
#define RAYDIUM_HOST_CMD_TP_MODE            0x60
#define RAYDIUM_HOST_CMD_FT_MODE            0x61

/* PDA2 literal */
/* entry byte + target page byte */
#define RAYDIUM_I2C_PDA2_PAGE_LENGTH        2


/* Touch report */
#define MAX_TOUCH_NUM                 2
#define MAX_REPORT_PACKET_SIZE        35
#define MAX_TCH_STATUS_PACKET_SIZE    4
#define PRESS_MAX                     0xFFFF
#define WIDTH_MAX                     0xFFFF
#define BYTE_SHIFT         8

/* FW update literal */
#define RAYDIUM_FW_BIN_PATH_LENGTH    256

#define RAD_BOOT_1X_SIZE		0x800
#define RAD_INIT_1X_SIZE		0x200
#define RAD_FW_1X_SIZE			0x5000
#define RAD_PARA_1X_SIZE		0xE4
#define RAD_TESTFW_1X_SIZE		0x5600

#define RAD_BOOT_2X_SIZE		0x800
#define RAD_INIT_2X_SIZE		0x200
#define RAD_FW_2X_SIZE			0x6200
#define RAD_PARA_2X_SIZE		0x15C
#define RAD_TESTFW_2X_SIZE		(RAD_FW_2X_SIZE + RAD_PARA_2X_SIZE + 4)

#define RAD_CMD_UPDATE_BIN		0x80
#define RAD_CMD_UPDATE_END		0x81
#define RAD_CMD_BURN_FINISH		0x82

/* FT APK literal */
#define RAD_HOST_CMD_POS    0x00
#define RAD_FT_CMD_POS      0x01
#define RAD_FT_CMD_LENGTH   0x02

/* FT APK data type */
#define RAYDIUM_FT_UPDATE    0x01

/*Raydium system flag*/
#define INT_FLAG	0x01
#define ENG_MODE	0x02

/* define display mode */
#define ACTIVE_MODE     0x00
#define AMBIENT_MODE    0x01
#define SLEEP_MODE      0x02

#define RAD_20 0x2209

/* Enable sysfs */
#define CONFIG_RM_SYSFS_DEBUG

/* Gesture switch */
#define GESTURE_EN

/* Enable FW update */
/* #define FW_UPDATE_EN */
/* #define FW_MAPPING_EN */
#define HOST_NOTIFY_EN
#define MSM_NEW_VER

/* enable ESD */
/* #define ESD_SOLUTION_EN */
/* #define ENABLE_DUMP_DATA */
/* #define ENABLE_FLASHLOG_BACKUP */
#ifdef ENABLE_DUMP_DATA
#define DATA_MAP_5_5 0
#endif



#define PINCTRL_STATE_ACTIVE     "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND    "pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE    "pmx_ts_release"

#if defined(CONFIG_TOUCHSCREEN_RM_TS_SELFTEST)
#define RAD_SELFTEST
#endif

struct raydium_ts_data {
	unsigned int irq;
	unsigned int irq_gpio;
	unsigned int rst_gpio;
	unsigned int x_max;
	unsigned int y_max;
#ifdef FILTER_POINTS
	unsigned int x_pos[2];
	unsigned int y_pos[2];
	unsigned int last_x_pos[2];
	unsigned int last_y_pos[2];
#else
	unsigned int x_pos[2];
	unsigned int y_pos[2];
#endif
	unsigned int pressure;
	unsigned int is_suspend;
	unsigned int is_sleep;
#ifdef GESTURE_EN
	unsigned int is_palm;
#endif
	unsigned char u8_max_touchs;

	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mutex lock;
	struct work_struct  work;
	struct workqueue_struct *workqueue;
	struct irq_desc *irq_desc;
	bool irq_enabled;
	bool irq_wake;

#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
	int blank;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif /*end of CONFIG_FB*/

	struct regulator *vdd;
	struct regulator *vcc_i2c;
	unsigned int fw_version;
	unsigned short id;
	char *vcc_name;
#ifdef MSM_NEW_VER
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
#endif /*end of MSM_NEW_VER*/

#ifdef ENABLE_DUMP_DATA
	struct delayed_work dump_work;

#endif /*end of ENABLE_DUMP_DATA*/

};
struct raydium_platform_data {
	char *vdd_name;
	int irq_gpio_number;
	int reset_gpio_number;
	int x_max;
	int y_max;
};

struct raydium_ts_platform_data {
	char *name;
	u32 irqflags;
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 hard_rst_dly;
	u32 soft_rst_dly;
	u32 num_max_touches;
	u32 fw_id;
};

/* TODO: Using struct+memcpy instead of array+offset*/
enum raydium_pt_report_status {
	POS_SEQ = 0,/*1:touch, 0:no touch*/
	POS_PT_AMOUNT,
	POS_GES_STATUS,
	POS_FW_STATE,
};

enum raydium_pt_report_idx {
	POS_PT_ID = 0,
	POS_X_L,
	POS_X_H,
	POS_Y_L,
	POS_Y_H,
	POS_PRESSURE_L,
	POS_PRESSURE_H,
	POS_WX_L,
	POS_WX_H,
	POS_WY_L,
	POS_WY_H,
	LEN_PT = 11
};

extern int raydium_read_touchdata(unsigned char *tp_status,  unsigned char *buf);
extern unsigned char raydium_mem_table_setting(void);
extern int wait_fw_state(struct i2c_client *client, unsigned int u32_addr,
			 unsigned int u32_state, unsigned long u32_delay_us,
			 unsigned short u16_retry);
extern int wait_irq_state(struct i2c_client *client,
				unsigned int u32_retry_time,
				unsigned int u32_delay_us);
extern void raydium_irq_control(bool enable);

extern int raydium_i2c_mode_control(struct i2c_client *client,
				    unsigned char u8_mode);
extern int raydium_i2c_pda_read(struct i2c_client *client,
				unsigned int u32_addr, unsigned char *u8_r_data,
				unsigned short u16_length);
extern int raydium_i2c_pda_write(struct i2c_client *client,
			unsigned int u32_addr, unsigned char *u8_w_data,
			unsigned short u16_length);
extern int raydium_i2c_pda2_read(struct i2c_client *client,
				 unsigned char u8_addr,
				 unsigned char *u8_r_data,
				 unsigned short u16_length);
extern int raydium_i2c_pda2_write(struct i2c_client *client,
				  unsigned char u8_addr,
				  unsigned char *u8_w_data,
				  unsigned short u16_length);
extern int raydium_i2c_pda2_set_page(struct i2c_client *client,
				unsigned int is_suspend,
				unsigned char u8_page);
extern unsigned char raydium_selftest_stop_mcu(struct i2c_client *client);
extern int raydium_burn_comp(struct i2c_client *client);
extern int raydium_burn_fw(struct i2c_client *client);
extern int raydium_fw_upgrade_with_bin_file(struct i2c_client *client,
		char *arguments,
		size_t count,
		struct device *dev);
extern int raydium_load_test_fw(struct i2c_client *client);
extern int raydium_fw_update_check(unsigned short u16_i2c_data);
extern int raydium_i2c_pda_set_address(unsigned int u32_address,
				       unsigned char u8_mode);
extern unsigned char raydium_mem_table_init(unsigned short u16_id);
extern unsigned char raydium_id_init(unsigned char u8_type);

#ifdef RAD_SELFTEST
extern int raydium_do_selftest(void);
#endif
int raydium_esd_check(void);

extern struct attribute *raydium_attributes[];

extern unsigned char g_u8_raydium_flag;
extern unsigned char g_u8_addr;
extern unsigned char g_u8_i2c_mode;
extern unsigned char g_u8_upgrade_type;
extern unsigned char g_u8_raw_data_type;
extern unsigned int g_u32_raw_data_len;    /* 72 bytes*/
extern unsigned int g_u32_length;
extern unsigned long g_u32_addr;
extern unsigned char *g_rad_fw_image, *g_rad_init_image;
extern unsigned char *g_rad_boot_image, *g_rad_para_image;
extern unsigned char *g_rad_testfw_image, *g_rad_testpara_image;
extern unsigned char g_u8_table_setting, g_u8_table_init;
extern unsigned int g_u32_driver_version;
extern unsigned char g_u8_resetflag;
extern struct raydium_ts_data *g_raydium_ts;

#endif
#endif  /*__LINUX_RAYDIUM_H*/

MODULE_AUTHOR("Raydium");
MODULE_DESCRIPTION("Raydium TouchScreen driver");
MODULE_LICENSE("GPL");

