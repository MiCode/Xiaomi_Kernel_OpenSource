/**
 *
 * @file	mstar_drv_platform_porting_layer.h
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

#ifndef __MSTAR_DRV_PLATFORM_PORTING_LAYER_H__
#define __MSTAR_DRV_PLATFORM_PORTING_LAYER_H__

/*--------------------------------------------------------------------------*/
/* INCLUDE FILE															 */
/*--------------------------------------------------------------------------*/

#include "mstar_drv_common.h"

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM)

#include <mach/board.h>
#include <mach/gpio.h>

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
#include <mach/regulator.h>
#include <linux/regulator/consumer.h>
#endif

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#include <linux/input/vir_ps.h>
#endif

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
#include <linux/regulator/consumer.h>
#endif

#ifdef CONFIG_ENABLE_NOTIFIER_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#include <linux/input/vir_ps.h>
#endif

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/hwmsen_helper.h>

#include <linux/namei.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#endif

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpio.h>

#include <cust_eint.h>
#include "tpd.h"
#include "cust_gpio_usage.h"
#include <pmic_drv.h>

#endif

/*--------------------------------------------------------------------------*/
/* PREPROCESSOR CONSTANT DEFINITION										 */
/*--------------------------------------------------------------------------*/

/*
 * Note.
 * Please change the below GPIO pin setting to follow the platform that you are using(EX. MediaTek, Spreadtrum, Qualcomm).
 */
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM)


#define MS_TS_MSG_IC_GPIO_RST   GPIO_TOUCH_RESET
#define MS_TS_MSG_IC_GPIO_INT   GPIO_TOUCH_IRQ

#ifdef CONFIG_TP_HAVE_KEY
#define TOUCH_KEY_MENU (139)
#define TOUCH_KEY_HOME (172)
#define TOUCH_KEY_BACK (158)
#define TOUCH_KEY_SEARCH (217)

#define MAX_KEY_NUM (4)
#endif

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)


#define MS_TS_MSG_IC_GPIO_RST   914
#define MS_TS_MSG_IC_GPIO_INT   915

#ifdef CONFIG_TP_HAVE_KEY
#define TOUCH_KEY_MENU (139)
#define TOUCH_KEY_HOME (172)
#define TOUCH_KEY_BACK (158)
#define TOUCH_KEY_SEARCH (217)

#define MAX_KEY_NUM (4)
#endif

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)

#define MS_TS_MSG_IC_GPIO_RST   (GPIO_CTP_RST_PIN)
#define MS_TS_MSG_IC_GPIO_INT   (GPIO_CTP_EINT_PIN)

#ifdef CONFIG_TP_HAVE_KEY
#define TOUCH_KEY_MENU	KEY_MENU
#define TOUCH_KEY_HOME	KEY_HOMEPAGE
#define TOUCH_KEY_BACK	KEY_BACK
#define TOUCH_KEY_SEARCH  KEY_SEARCH

#define MAX_KEY_NUM (4)
#endif

#endif

/*--------------------------------------------------------------------------*/
/* GLOBAL FUNCTION DECLARATION											  */
/*--------------------------------------------------------------------------*/

extern void DrvPlatformLyrDisableFingerTouchReport(void);
extern void DrvPlatformLyrEnableFingerTouchReport(void);
extern void DrvPlatformLyrFingerTouchPressed(s32 nX, s32 nY, s32 nPressure, s32 nId);
extern void DrvPlatformLyrFingerTouchReleased(s32 nX, s32 nY, s32 nId);
extern s32 DrvPlatformLyrInputDeviceInitialize(struct i2c_client *pClient);
extern void DrvPlatformLyrSetIicDataRate(struct i2c_client *pClient, u32 nIicDataRate);
extern void DrvPlatformLyrTouchDevicePowerOff(void);
extern void DrvPlatformLyrTouchDevicePowerOn(void);
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
extern void DrvPlatformLyrTouchDeviceRegulatorPowerOn(void);
#endif
extern void DrvPlatformLyrTouchDeviceRegisterEarlySuspend(void);
extern s32 DrvPlatformLyrTouchDeviceRegisterFingerTouchInterruptHandler(void);
extern s32 DrvPlatformLyrTouchDeviceRemove(struct i2c_client *pClient);
extern s32 DrvPlatformLyrTouchDeviceRequestGPIO(void);
extern void DrvPlatformLyrTouchDeviceResetHw(void);
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
extern int DrvPlatformLyrGetTpPsData(void);
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
extern void DrvPlatformLyrTpPsEnable(int nEnable);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
extern int DrvPlatformLyrTpPsOperate(void *pSelf, u32 nCommand, void *pBuffIn, int nSizeIn, void *pBuffOut, int nSizeOut, int *pActualOut);
#endif
#endif

#endif  /* __MSTAR_DRV_PLATFORM_PORTING_LAYER_H__ */
