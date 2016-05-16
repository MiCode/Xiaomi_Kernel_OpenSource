/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 * @file	mstar_drv_common.h
 *
 * @brief   This file defines the interface of touch screen
 *
*
 */

#ifndef __MSTAR_DRV_COMMON_H__
#define __MSTAR_DRV_COMMON_H__

/*--------------------------------------------------------------------------*/
/* INCLUDE FILE															 */
/*--------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/miscdevice.h>
#include <linux/time.h>
#include <linux/input/mt.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>

/*--------------------------------------------------------------------------*/
/* TOUCH DEVICE DRIVER RELEASE VERSION									  */
/*--------------------------------------------------------------------------*/

#define DEVICE_DRIVER_RELEASE_VERSION   ("3.3.0.0")


/*--------------------------------------------------------------------------*/
/* COMPILE OPTION DEFINITION												*/
/*--------------------------------------------------------------------------*/

/*
 * Note.
 * The below compile option is used to enable the specific device driver code handling for distinct smart phone developer platform.
 * For running on Spreadtrum platform, please define the compile option CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM.
 * For running on Qualcomm platform, please define the compile option CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM.
 * For running on MediaTek platform, please define the compile option CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM.
 */

#define CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM


/*
 * Note.
 * The below compile option is used to enable the specific device driver code handling for mutual-capacitive ic or self-capacitive ic.
 * For a mutual-capacitive ic project(MSG26xxM/MSG28xx), please define the compile option CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC.
 * For a self-capacitive ic project(MSG21xxA/MSG22xx), please define the compile option CONFIG_ENABLE_TOUCH_DRIVER_FOR_SELF_IC.
 */
#define CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC



/*
 * Note.
 * The below compile option is used to enable the specific device driver code handling to make sure main board can supply power to touch ic for some specific BB chip of MTK(EX. MT6582)/SPRD(EX. SC7715)/QCOM(EX. MSM8610).
 * By default, this compile option is disabled.
 */


/*
 * Note.
 * The below compile option is used to distinguish different workqueue scheduling mechanism when firmware report finger touch to device driver by IRQ interrupt.
 * For MTK platform, there are two type of workqueue scheduling mechanism available.
 * Please refer to the related code which is enclosed by compile option CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM in mstar_drv_platform_porting_layer.c for more detail.
 * This compile option is used for MTK platform only.
 * By default, this compile option is disabled.
 */



/*
 * Note.
 * The below compile option is used to apply DMA mode for read/write data between device driver and firmware by I2C.
 * The DMA mode is used to reslove I2C read/write 8 bytes limitation for specific MTK BB chip(EX. MT6589/MT6572/...)
 * This compile option is used for MTK platform only.
 * By default, this compile option is disabled.
 */



/*
 * Note.
 * The below compile option is used to enable the specific device driver code handling when touch panel support virtual key(EX. Menu, Home, Back, Search).
 * If this compile option is not defined, the function for virtual key handling will be disabled.
 * By default, this compile option is enabled.
 */
#define CONFIG_TP_HAVE_KEY

/*
 * Note.
 * Since specific MTK BB chip report virtual key touch by using coordinate instead of key code, the below compile option is used to enable the code handling for reporting key with coordinate.
 * This compile option is used for MTK platform only.
 * By default, this compile option is disabled.
 */


/*
 * Note.
 * The below flag is used to enable debug mode data log for firmware.
 * Please make sure firmware support debug mode data log firstly, then you can enable this flag.
 * By default, this flag is enabled.
 */
#define CONFIG_ENABLE_FIRMWARE_DATA_LOG (1)

/*
 * Note.
 * The below compile option is used to enable segment read debug mode finger touch data for MSG26XXM/MSG28XX only.
 * Since I2C transaction length limitation for some specific MTK BB chip(EX. MT6589/MT6572/...) or QCOM BB chip, the debug mode finger touch data of MSG26XXM/MSG28XX can not be retrieved by one time I2C read operation.
 * So we need to retrieve the complete finger touch data by segment read.
 * By default, this compile option is enabled.
 */
#define CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA

/*
 * Note.
 * The below compile option is used to enable gesture wakeup.
 * By default, this compile option is disabled.
 */



#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

/*
 * Note.
 * The below compile option is used to enable device driver to support at most 64 types of gesture wakeup mode.
 * If the below compile option is not enabled, device driver can only support at most 16 types of gesture wakeup mode.
 * By the way, 64 types of gesture wakeup mode is ready for MSG22XX/MSG28XX only.
 * But, 64 types of gesture wakeup mode for MSG21XXA/MSG26XXM is not supported.
 * By default, this compile option is disabled.
 */


/*
 * Note.
 * The below compile option is used to enable gesture debug mode.
 * By default, this compile option is disabled.
 */


/*
 * Note.
 * The below compile option is used to enable gesture information mode.
 * By default, this compile option is disabled.
 */


#endif



/*
 * Note.
 * The below compile option is used to enable phone level MP test handling.
 * By the way, phone level MP test is ready for MSG21XXA/MSG22XX/MSG26XXM.
 * But, phone level MP test for MSG28XX is not ready yet.
 * By default, this compile option is disabled.
 */



#ifdef CONFIG_ENABLE_ITO_MP_TEST

/*
 * Note.
 * The below compile option is used to enable the specific short test item of 2R triangle pattern for self-capacitive touch ic.
 * This compile option is used for MSG21XXA/MSG22XX only.
 * Please enable the compile option if the ITO pattern is 2R triangle pattern for MSG21XXA/MSG22XX.
 * Please disable the compile option if the ITO pattern is H(horizontal) triangle pattern for MSG21XXA/MSG22XX.
 * By default, this compile option is enabled.
 */
#define CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE

#endif



/*
 * Note.
 * If this compile option is not defined, the SW ID mechanism for updating firmware will be disabled.
 * By default, this compile option is disabled.
 */



#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID

/*
 * Note.
 * If this compile option is defined, the update firmware bin file shall be stored in a two dimensional array format.
 * Else, the update firmware bin file shall be stored in an one dimensional array format.
 * Be careful, MSG22XX only support storing update firmware bin file in an one dimensional array format, it does not support two dimensional array format.
 * By default, this compile option is enabled.
 */
#define CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY

#endif


/*
 * Note.
 * The below compile option is used to enable hotknot and only supports MSG28XX.
 * By default, this compile option is disabled.
 */



/*
 * Note.
 * The below compile option is used to enable proximity detection.
 * By default, this compile option is disabled.
 */


/*
 * Note.
 * The below compile option is used to enable notifier feedback handling for QCOM platform.
 * By default, this compile option is disabled.
 */
#define CONFIG_ENABLE_NOTIFIER_FB


/*
 * Note.
 * The below compile option is used to enable report rate calculation.
 * By default, this compile option is enabled.
 */
#define CONFIG_ENABLE_COUNT_REPORT_RATE


/*
 * Note.
 * The below compile option is used to enable glove mode and only supports MSG28XX.
 * By default, this compile option is disabled.
 */



/*
 * Note.
 * The below compile option is used to enable jni interface.
 * By default, this compile option is enabled.
 */
#define CONFIG_ENABLE_JNI_INTERFACE


/*
 * Note.
 * The below compile option is used to enable charger detection for notifying the charger plug in/plug out status to touch firmware.
 * By default, this compile option is disabled.
 */



/*
 * Note.
 * The below compile option is used to enable the debug code for clarifying some issues. For example, to debug the delay time issue for IC hardware reset.
 * By the way, this feature is supported for MSG28XX only.
 * By default, this compile option is disabled.
 */
#define CONFIG_ENABLE_CODE_FOR_DEBUG


/*
 * Note.
 * The below compile option is used to enable/disable Type A/Type B multi-touch protocol for reporting touch point/key to Linux input sub-system.
 * If this compile option is defined, Type B protocol is enabled.
 * Else, Type A protocol is enabled.
 * By the way, Type B protocol is supported for MSG26XXM/MSG28XX only, but is not supported for MSG21XXA/MSG22XX yet.
 * By default, this compile option is disabled.
 */



/*--------------------------------------------------------------------------*/
/* PREPROCESSOR CONSTANT DEFINITION										 */
/*--------------------------------------------------------------------------*/

#define u8   unsigned char
#define u16  unsigned short
#define u32  unsigned int
#define s8   signed char
#define s16  signed short
#define s32  signed int
#define s64  int64_t
#define u64  uint64_t


#define CHIP_TYPE_MSG21XX   (0x01)
#define CHIP_TYPE_MSG21XXA  (0x02)
#define CHIP_TYPE_MSG26XXM  (0x03)
#define CHIP_TYPE_MSG22XX   (0x7A)
#define CHIP_TYPE_MSG28XX   (0x85)

#define PACKET_TYPE_TOOTH_PATTERN   (0x20)
#define PACKET_TYPE_GESTURE_WAKEUP  (0x50)
#define PACKET_TYPE_GESTURE_DEBUG  		(0x51)
#define PACKET_TYPE_GESTURE_INFORMATION	(0x52)

#define TOUCH_SCREEN_X_MIN   (0)
#define TOUCH_SCREEN_Y_MIN   (0)
/*
 * Note.
 * Please change the below touch screen resolution according to the touch panel that you are using.
 */
#define TOUCH_SCREEN_X_MAX   (720)
#define TOUCH_SCREEN_Y_MAX   (1280)
/*
 * Note.
 * Please do not change the below setting.
 */
#define TPD_WIDTH   (2048)
#define TPD_HEIGHT  (2048)


#define BIT0  (1<<0)
#define BIT1  (1<<1)
#define BIT2  (1<<2)
#define BIT3  (1<<3)
#define BIT4  (1<<4)
#define BIT5  (1<<5)
#define BIT6  (1<<6)
#define BIT7  (1<<7)
#define BIT8  (1<<8)
#define BIT9  (1<<9)
#define BIT10 (1<<10)
#define BIT11 (1<<11)
#define BIT12 (1<<12)
#define BIT13 (1<<13)
#define BIT14 (1<<14)
#define BIT15 (1<<15)


#define MAX_DEBUG_REGISTER_NUM	 (10)
#define MAX_DEBUG_COMMAND_ARGUMENT_NUM	  (4)

#define MAX_UPDATE_FIRMWARE_BUFFER_SIZE	(130)

#define MAX_I2C_TRANSACTION_LENGTH_LIMIT	  (250)
#define MAX_TOUCH_IC_REGISTER_BANK_SIZE	   (256)


#define PROCFS_AUTHORITY (0666)


#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#define GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG	 0x00000001
#define GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG		0x00000002
#define GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG	  0x00000004
#define GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG	  0x00000008
#define GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG	 0x00000010
#define GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG	  0x00000020
#define GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG	  0x00000040
#define GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG	  0x00000080
#define GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG	  0x00000100
#define GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG	  0x00000200
#define GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG	  0x00000400
#define GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG	  0x00000800
#define GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG	  0x00001000
#define GESTURE_WAKEUP_MODE_RESERVE1_FLAG		 0x00002000
#define GESTURE_WAKEUP_MODE_RESERVE2_FLAG		 0x00004000
#define GESTURE_WAKEUP_MODE_RESERVE3_FLAG		 0x00008000

#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
#define GESTURE_WAKEUP_MODE_RESERVE4_FLAG		 0x00010000
#define GESTURE_WAKEUP_MODE_RESERVE5_FLAG		 0x00020000
#define GESTURE_WAKEUP_MODE_RESERVE6_FLAG		 0x00040000
#define GESTURE_WAKEUP_MODE_RESERVE7_FLAG		 0x00080000
#define GESTURE_WAKEUP_MODE_RESERVE8_FLAG		 0x00100000
#define GESTURE_WAKEUP_MODE_RESERVE9_FLAG		 0x00200000
#define GESTURE_WAKEUP_MODE_RESERVE10_FLAG		0x00400000
#define GESTURE_WAKEUP_MODE_RESERVE11_FLAG		0x00800000
#define GESTURE_WAKEUP_MODE_RESERVE12_FLAG		0x01000000
#define GESTURE_WAKEUP_MODE_RESERVE13_FLAG		0x02000000
#define GESTURE_WAKEUP_MODE_RESERVE14_FLAG		0x04000000
#define GESTURE_WAKEUP_MODE_RESERVE15_FLAG		0x08000000
#define GESTURE_WAKEUP_MODE_RESERVE16_FLAG		0x10000000
#define GESTURE_WAKEUP_MODE_RESERVE17_FLAG		0x20000000
#define GESTURE_WAKEUP_MODE_RESERVE18_FLAG		0x40000000
#define GESTURE_WAKEUP_MODE_RESERVE19_FLAG		0x80000000

#define GESTURE_WAKEUP_MODE_RESERVE20_FLAG		0x00000001
#define GESTURE_WAKEUP_MODE_RESERVE21_FLAG		0x00000002
#define GESTURE_WAKEUP_MODE_RESERVE22_FLAG		0x00000004
#define GESTURE_WAKEUP_MODE_RESERVE23_FLAG		0x00000008
#define GESTURE_WAKEUP_MODE_RESERVE24_FLAG		0x00000010
#define GESTURE_WAKEUP_MODE_RESERVE25_FLAG		0x00000020
#define GESTURE_WAKEUP_MODE_RESERVE26_FLAG		0x00000040
#define GESTURE_WAKEUP_MODE_RESERVE27_FLAG		0x00000080
#define GESTURE_WAKEUP_MODE_RESERVE28_FLAG		0x00000100
#define GESTURE_WAKEUP_MODE_RESERVE29_FLAG		0x00000200
#define GESTURE_WAKEUP_MODE_RESERVE30_FLAG		0x00000400
#define GESTURE_WAKEUP_MODE_RESERVE31_FLAG		0x00000800
#define GESTURE_WAKEUP_MODE_RESERVE32_FLAG		0x00001000
#define GESTURE_WAKEUP_MODE_RESERVE33_FLAG		0x00002000
#define GESTURE_WAKEUP_MODE_RESERVE34_FLAG		0x00004000
#define GESTURE_WAKEUP_MODE_RESERVE35_FLAG		0x00008000
#define GESTURE_WAKEUP_MODE_RESERVE36_FLAG		0x00010000
#define GESTURE_WAKEUP_MODE_RESERVE37_FLAG		0x00020000
#define GESTURE_WAKEUP_MODE_RESERVE38_FLAG		0x00040000
#define GESTURE_WAKEUP_MODE_RESERVE39_FLAG		0x00080000
#define GESTURE_WAKEUP_MODE_RESERVE40_FLAG		0x00100000
#define GESTURE_WAKEUP_MODE_RESERVE41_FLAG		0x00200000
#define GESTURE_WAKEUP_MODE_RESERVE42_FLAG		0x00400000
#define GESTURE_WAKEUP_MODE_RESERVE43_FLAG		0x00800000
#define GESTURE_WAKEUP_MODE_RESERVE44_FLAG		0x01000000
#define GESTURE_WAKEUP_MODE_RESERVE45_FLAG		0x02000000
#define GESTURE_WAKEUP_MODE_RESERVE46_FLAG		0x04000000
#define GESTURE_WAKEUP_MODE_RESERVE47_FLAG		0x08000000
#define GESTURE_WAKEUP_MODE_RESERVE48_FLAG		0x10000000
#define GESTURE_WAKEUP_MODE_RESERVE49_FLAG		0x20000000
#define GESTURE_WAKEUP_MODE_RESERVE50_FLAG		0x40000000
#define GESTURE_WAKEUP_MODE_RESERVE51_FLAG		0x80000000
#endif

#define GESTURE_WAKEUP_PACKET_LENGTH	(6)

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
#define GESTURE_DEBUG_MODE_PACKET_LENGTH	(128)
#endif

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
#define GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH	(128)
#endif

#endif

#define FEATURE_GESTURE_WAKEUP_MODE		 0x0001
#define FEATURE_GESTURE_DEBUG_MODE		  0x0002
#define FEATURE_GESTURE_INFORMATION_MODE	0x0003

#define FEATURE_TOUCH_DRIVER_DEBUG_LOG	  0x0010
#define FEATURE_FIRMWARE_DATA_LOG		   0x0011
#define FEATURE_FORCE_TO_UPDATE_FIRMWARE	0x0012


#define I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE   (20)

#define FIRMWARE_FILE_PATH_ON_SD_CARD	  "/mnt/sdcard/msctp_update.bin"


/*
 * Note.
 * The below flag is used to enable the output log mechanism while touch device driver is running.
 * If this flag is not defined, the function for output log will be disabled.
 * By default, this flag is enabled.
 */
#define CONFIG_ENABLE_TOUCH_DRIVER_DEBUG_LOG (1)

/*=============================================================*/

/*=============================================================*/

extern u8 IS_TOUCH_DRIVER_DEBUG_LOG_ENABLED;

/*--------------------------------------------------------------------------*/
/* PREPROCESSOR MACRO DEFINITION											*/
/*--------------------------------------------------------------------------*/

#define DBG(fmt, arg...) do {\
							   if (IS_TOUCH_DRIVER_DEBUG_LOG_ENABLED == 1)\
								   printk(fmt, ##arg);\
						} while (0)


/*--------------------------------------------------------------------------*/
/* DATA TYPE DEFINITION													 */
/*--------------------------------------------------------------------------*/

typedef enum {
	EMEM_ALL = 0,
	EMEM_MAIN,
	EMEM_INFO
} EmemType_e;

typedef enum {
	ITO_TEST_MODE_OPEN_TEST = 1,
	ITO_TEST_MODE_SHORT_TEST = 2,
	ITO_TEST_MODE_WATERPROOF_TEST = 3
} ItoTestMode_e;

typedef enum {
	ITO_TEST_OK = 0,
	ITO_TEST_FAIL,
	ITO_TEST_GET_TP_TYPE_ERROR,
	ITO_TEST_UNDEFINED_ERROR,
	ITO_TEST_UNDER_TESTING

} ItoTestResult_e;

typedef enum {
	ADDRESS_MODE_8BIT = 0,
	ADDRESS_MODE_16BIT = 1
} AddressMode_e;

typedef enum {
	MUTUAL = 0,
	SELF,
	WATERPROOF,
	MUTUAL_SINGLE_DRIVE
} ItoTestFwMode_e;

/*--------------------------------------------------------------------------*/
/* GLOBAL VARIABLE DEFINITION											   */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* GLOBAL FUNCTION DECLARATION											  */
/*--------------------------------------------------------------------------*/

extern u8 DrvCommonCalculateCheckSum(u8 *pMsg, u32 nLength);
extern u32 DrvCommonConvertCharToHexDigit(char *pCh, u32 nLength);
extern u32 DrvCommonCrcDoReflect(u32 nRef, s8 nCh);
extern u32 DrvCommonCrcGetValue(u32 nText, u32 nPrevCRC);
extern void DrvCommonCrcInitTable(void);
extern void DrvCommonReadFile(char *pFilePath, u8 *pBuf, u16 nLength);

#endif  /* __MSTAR_DRV_COMMON_H__ */
