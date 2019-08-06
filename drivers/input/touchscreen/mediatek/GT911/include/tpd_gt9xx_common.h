/*
 *  Driver for Goodix Touchscreens
 *
 *  Copyright (c) 2014 Red Hat Inc.
 *  Copyright (c) 2015 K. Merker <merker@debian.org>
 *
 *  This code is based on gt9xx.c authored by andrew@goodix.com:
 *
 *  2010 - 2012 Goodix Technology.
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2 of the License.
 */

#ifndef TPD_CUSTOM_GT9XX_H__
#define TPD_CUSTOM_GT9XX_H__

#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
/* #include <linux/io.h> */

#include <linux/bitops.h>
#include <linux/byteorder/generic.h>
#include <linux/delay.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/interrupt.h>
#include <linux/time.h>

#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

extern struct tpd_device *tpd;
extern u16 show_len;
extern u16 total_len;
extern u8 gtp_rawdiff_mode;
extern unsigned int tpd_rst_gpio_number;
extern unsigned int tpd_int_gpio_number;
extern unsigned int touch_irq;

extern s32 gtp_send_cfg(struct i2c_client *client);
extern void gtp_reset_guitar(struct i2c_client *client, s32 ms);
extern void gtp_int_sync(void);
extern u8 gup_init_update_proc(struct i2c_client *client);
extern u8 gup_init_fw_proc(struct i2c_client *client);
extern s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len);
extern s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len);
extern int i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf,
			   int len);
extern int i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf,
			  int len);

/* ***************************PART1:ON/OFF define*************************/

/*
 *****************************************
 * Notice : all customizable feature shuold import
 * to defconfig/Kconfig/dts file.
 * Do not config it here
 *******************************************
 */
/*#define GTP_CUSTOM_CFG        0*/ /*config in defconfig*/
/* driver send config to TP on intilization (for no config built in TP flash) */
/*#define GTP_DRIVER_SEND_CFG   1*/ /*config in defconfig*/
/*#define GTP_HAVE_TOUCH_KEY    0*/ /*config in defconfig*/
/*#define GTP_POWER_CTRL_SLEEP  1*/
/*config in defconfig*/ /* turn off power on suspend */
/*#define GTP_AUTO_UPDATE       1*/
/*config in defconfig*/ /* update FW to TP FLASH */
/*#define GTP_CHANGE_X2Y        0*/ /*config in defconfig*/ /* set for se1 */
/* auto updated fw by gtp_default_FW in gt9xx_firmware.h, */
/* function together with */
/* GTP_AUTO_UPDATE */
/*#define GTP_HEADER_FW_UPDATE  1*/ /*config in defconfig*/
/* auto update config by .cfg file, function together with GTP_AUTO_UPDATE */
/*Now GT911 do not use this option.If use please add it to*/
/* Kconfig and config in */
/* defconfig*/
/*#define GTP_AUTO_UPDATE_CFG   0*/
/* if gt9xxf, better enable it if hardware platform supported */
/*Now GT911 do not use this option.If use please add it to*/
/* Kconfig and config in */
/* defconfig*/
/*#define GTP_SUPPORT_I2C_DMA   1*/
/*#define GTP_COMPATIBLE_MODE   1*/
/*config in defconfig*/ /* compatible with GT9XXF */

/*#define GTP_ESD_PROTECT       0*/
/*config in defconfig*/ /* esd protection with a cycle of 2 seconds */
/*#define GTP_CREATE_WR_NODE    1*/ /*config in defconfig*/
/*Now GT911 do not use this option.If use please add it to Kconfig */
/* and config in */
/* defconfig*/
/*#define GUP_USE_HEADER_FILE   0*/
/*#define GTP_FW_DOWNLOAD       0*/
/*config in defconfig*/ /* update FW to TP SRAM */
/* #define GTP_CHARGER_DETECT */

#define GTP_CONFIG_MIN_LENGTH 186
#define GTP_CONFIG_MAX_LENGTH 240
/* #define GTP_CHARGER_SWITCH    0*/
/*config in defconfig*/ /* charger plugin & plugout detect */
/*Now GT911 do not use this option.If use please add it to*/
/* Kconfig and config in */
/* defconfig*/
/*#define GTP_WITH_PEN          0*/
/*Now GT911 do not use this option.If use please add it to */
/*Kconfig and config in */
/* defconfig*/
/*#define GTP_SLIDE_WAKEUP      0*/
/* double-click wakup, function together with GTP_SLIDE_WAKEUP */
/*Now GT911 do not use this option.If use please add it to */
/*Kconfig and config in */
/* defconfig*/
/*#define GTP_DBL_CLK_WAKEUP    0*/
/*#define HOTKNOT_BLOCK_RW      0*/ /*config in defconfig*/

/* #define GTP_PROXIMITY */ /*config in defconfig*/
/* #define TPD_HAVE_BUTTON*/
/*report key as coordinate,Vibration feedback */ /*config in  dts file */
/* #define TPD_WARP_X */			 /*config in defconfig*/
/* #define TPD_WARP_Y */			 /*config in defconfig*/

#define GTP_INFO_ON 0
#define GTP_DEBUG_ON 0
#define GTP_DEBUG_ARRAY_ON 0
#define GTP_DEBUG_FUNC_ON 0

#define CFG_GROUP_LEN(p_cfg_grp) ARRAY_SIZE(p_cfg_grp)
#define FLASHLESS_FLASH_WORKROUND 0

#ifdef CONFIG_GTP_COMPATIBLE_MODE
extern void force_reset_guitar(void);
#endif

/* STEP_3(optional):Custom set some config by themself,if need. */
#ifdef CONFIG_GTP_CUSTOM_CFG
#define GTP_MAX_HEIGHT 800
#define GTP_MAX_WIDTH 480
#define GTP_INT_TRIGGER 0 /* 0:Rising 1:Falling */
#else
#define GTP_MAX_HEIGHT 4096
#define GTP_MAX_WIDTH 4096
#define GTP_INT_TRIGGER 1
#endif
#define GTP_MAX_TOUCH 5

#define VELOCITY_CUSTOM
#define TPD_VELOCITY_CUSTOM_X 15
#define TPD_VELOCITY_CUSTOM_Y 15
#ifdef CONFIG_GTP_CREATE_WR_NODE
extern s32 init_wr_node(struct i2c_client *client);
extern void uninit_wr_node(void);
#endif
#ifdef CONFIG_GTP_CHARGER_DETECT
extern bool upmu_get_pchr_chrdet(void);
#endif

#ifdef CONFIG_GTP_ESD_PROTECT
extern void gtp_esd_switch(struct i2c_client *client, s32 on);
#endif

#define UPDATE_FUNCTIONS
#ifdef UPDATE_FUNCTIONS
extern s32 gup_enter_update_mode(struct i2c_client *client);
extern void gup_leave_update_mode(void);
extern s32 gup_update_proc(void *dir);
#endif
extern struct i2c_client *i2c_client_point;
#ifdef VELOCITY_CUSTOM
extern int tpd_v_magnify_x;
extern int tpd_v_magnify_y;
#endif

/* STEP_4(optional):If this project have touch key,Set touch key config. */
#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
#define GTP_KEY_TAB                                                            \
	{                                                                      \
		KEY_MENU, KEY_HOME, KEY_BACK, KEY_SEND                         \
	}
#endif

/* ***************************PART3:OTHER */
/* define********************************* */
#define GTP_DRIVER_VERSION "V2.1<2014/01/10>"
#define GTP_I2C_NAME "Goodix-TS"
#define GT91XX_CONFIG_PROC_FILE "gt9xx_config"
#define GTP_POLL_TIME 10
#define GTP_ADDR_LENGTH 2
#define GTP_CONFIG_MIN_LENGTH 186
#define GTP_CONFIG_MAX_LENGTH 240
#define FAIL 0
#define SUCCESS 1
#define SWITCH_OFF 0
#define SWITCH_ON 1

/* ******************** For GT9XXF Start ***********************/
#ifdef CONFIG_GTP_COMPATIBLE_MODE
enum CHIP_TYPE_T {
	CHIP_TYPE_GT9 = 0,
	CHIP_TYPE_GT9F = 1,
};
#endif

#define GTP_REG_MATRIX_DRVNUM 0x8069
#define GTP_REG_MATRIX_SENNUM 0x806A
#define GTP_REG_RQST 0x8043
#define GTP_REG_BAK_REF 0x99D0
#define GTP_REG_MAIN_CLK 0x8020
#define GTP_REG_CHIP_TYPE 0x8000
#define GTP_REG_HAVE_KEY 0x804E
#define GTP_REG_HN_STATE 0xAB10

#define GTP_FL_FW_BURN 0x00
#define GTP_FL_ESD_RECOVERY 0x01
#define GTP_FL_READ_REPAIR 0x02

#define GTP_BAK_REF_SEND 0
#define GTP_BAK_REF_STORE 1
#define CFG_LOC_DRVA_NUM 29
#define CFG_LOC_DRVB_NUM 30
#define CFG_LOC_SENS_NUM 31

#define GTP_CHK_FW_MAX 1000
#define GTP_CHK_FS_MNT_MAX 300
#define GTP_BAK_REF_PATH "/data/gtp_ref.bin"
#define GTP_MAIN_CLK_PATH "/data/gtp_clk.bin"
#define GTP_RQST_CONFIG 0x01
#define GTP_RQST_BAK_REF 0x02
#define GTP_RQST_RESET 0x03
#define GTP_RQST_MAIN_CLOCK 0x04
#define GTP_RQST_HOTKNOT_CODE 0x20
#define GTP_RQST_RESPONDED 0x00
#define GTP_RQST_IDLE 0xFF

#define HN_DEVICE_PAIRED 0x80
#define HN_MASTER_DEPARTED 0x40
#define HN_SLAVE_DEPARTED 0x20
#define HN_MASTER_SEND 0x10
#define HN_SLAVE_RECEIVED 0x08

/* ******************** For GT9XXF End ***********************/

/* Register define */
#define GTP_READ_COOR_ADDR 0x814E
#define GTP_REG_SLEEP 0x8040
#define GTP_REG_SENSOR_ID 0x814A
#define GTP_REG_CONFIG_DATA 0x8047
#define GTP_REG_VERSION 0x8140
#define GTP_REG_HW_INFO 0x4220
#define GTP_REG_REFRESH_RATE 0x8056

#define RESOLUTION_LOC 3
#define TRIGGER_LOC 8

#define GTP_DMA_MAX_TRANSACTION_LENGTH 255 /* for DMA mode */
#define GTP_DMA_MAX_I2C_TRANSFER_SIZE                                          \
	(GTP_DMA_MAX_TRANSACTION_LENGTH - GTP_ADDR_LENGTH)
#define MAX_TRANSACTION_LENGTH 8
#define TPD_I2C_NUMBER 0
#define I2C_MASTER_CLOCK 300
#define MAX_I2C_TRANSFER_SIZE (MAX_TRANSACTION_LENGTH - GTP_ADDR_LENGTH)
#define TPD_MAX_RESET_COUNT 3
/* #define TPD_CALIBRATION_MATRIX        {962,0,0,0,1600,0,0,0} */
#define TPD_CALIBRATION_MATRIX_ROTATION_NORMAL                                 \
	{                                                                      \
		0, 2559, 0, -6557, 0, 5238784, 0, 0                            \
	}
#define TPD_CALIBRATION_MATRIX_ROTATION_FACTORY                                \
	{                                                                      \
		0, 2559, 0, -6557, 0, 5238784, 0, 0                            \
	}

#define TPD_RESET_ISSUE_WORKAROUND
#define TPD_HAVE_CALIBRATION
#define TPD_NO_GPIO
#define TPD_RESET_ISSUE_WORKAROUND

#ifdef TPD_WARP_X
#undef TPD_WARP_X
#define TPD_WARP_X(x_max, x) (x_max - 1 - x)
#else
#define TPD_WARP_X(x_max, x) x
#endif

#ifdef TPD_WARP_Y
#undef TPD_WARP_Y
#define TPD_WARP_Y(y_max, y) (y_max - 1 - y)
#else
#define TPD_WARP_Y(y_max, y) y
#endif

/* Log define */
#define GTP_INFO(fmt, arg...)                                                  \
	do {                                                                   \
		if (GTP_INFO_ON)                                               \
			pr_debug("<<-GTP-INFO->> " fmt "\n", ##arg);           \
	} while (0)
#define GTP_ERROR(fmt, arg...) pr_info("<<-GTP-ERROR->> " fmt "\n", ##arg)
#define GTP_DEBUG(fmt, arg...)                                                 \
	do {                                                                   \
		if (GTP_DEBUG_ON)                                              \
			pr_debug("<<-GTP-DEBUG->> [%d]" fmt "\n", __LINE__,    \
				 ##arg);                                       \
	} while (0)
#define GTP_DEBUG_ARRAY(array, num)                                            \
	do {                                                                   \
		s32 i;                                                         \
		u8 *a = array;                                                 \
		if (GTP_DEBUG_ARRAY_ON) {                                      \
			pr_debug("<<-GTP-DEBUG-ARRAY->>\n");                   \
			for (i = 0; i < (num); i++) {                          \
				pr_debug("%02x   ", (a)[i]);                   \
				if ((i + 1) % 10 == 0)                         \
					pr_debug("\n");                        \
			}                                                      \
			pr_debug("\n");                                        \
		}                                                              \
	} while (0)
#define GTP_DEBUG_FUNC()                                                       \
	do {                                                                   \
		if (GTP_DEBUG_FUNC_ON)                                         \
			pr_debug("<<-GTP-FUNC->> Func:%s@Line:%d\n", __func__, \
				 __LINE__);                                    \
	} while (0)
#define GTP_SWAP(x, y)                                                         \
	do {                                                                   \
		typeof(x) z = x;                                               \
		x = y;                                                         \
		y = z;                                                         \
	} while (0)

/* ****************************PART4:UPDATE */
/* define******************************* */
/* Error no */
#define ERROR_NO_FILE 2      /* ENOENT */
#define ERROR_FILE_READ 23   /* ENFILE */
#define ERROR_FILE_TYPE 21   /* EISDIR */
#define ERROR_GPIO_REQUEST 4 /* EINTR */
#define ERROR_I2C_TRANSFER 5 /* EIO */
#define ERROR_NO_RESPONSE 16 /* EBUSY */
#define ERROR_TIMEOUT 110    /* ETIMEDOUT */

/* *****************************End of Part III********************************
 */

#endif /* TPD_CUSTOM_GT9XX_H__ */
