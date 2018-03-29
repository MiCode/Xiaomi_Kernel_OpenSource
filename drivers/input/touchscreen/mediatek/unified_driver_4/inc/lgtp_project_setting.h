/***************************************************************************
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
 *    File  : lgtp_project_setting.h
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/

#if !defined(_LGTP_PROJECT_SETTING_H_)
#define _LGTP_PROJECT_SETTING_H_


/****************************************************************************
* Project Setting ( Model )
****************************************************************************/
#if defined(CONFIG_TOUCHSCREEN_LGE_MIT300)
#define TOUCH_MODEL_M1V
#elif defined(CONFIG_TOUCHSCREEN_LGE_MIT300_M4)
#define TOUCH_MODEL_M4
#elif defined(CONFIG_TOUCHSCREEN_LGE_FT8707)
#define TOUCH_MODEL_K7
#endif

/****************************************************************************
* Available Feature supported by Unified Driver
* If you want to use it, define it inside of model feature
****************************************************************************/
/* #define ENABLE_HOVER_DETECTION */
/* #define ENABLE_TOUCH_AT_OFF_CHARGING */

/****************************************************************************
* Project Setting ( AP Solution / AP Chipset / Touch Device )
****************************************************************************/
#if defined(TOUCH_MODEL_Y30)

/* AP Solution */
#define TOUCH_PLATFORM_QCT

/* AP Chipset */
#define TOUCH_PLATFORM_MSM8210

/* Touch Device */
#define TOUCH_DEVICE_LU201X
#define TOUCH_DEVICE_LU202X
#define TOUCH_DEVICE_FT6X36
#define TOUCH_DEVICE_DUMMY

/* Driver Feature */
#define ENABLE_HOVER_DETECTION

/* IC Type */
#define TOUCH_TYPE_ONCELL
#elif defined(TOUCH_MODEL_C30)

/* AP Solution */
#define TOUCH_PLATFORM_QCT

/* AP Chipset */
#define TOUCH_PLATFORM_MSM8916

/* Touch Device */
#define TOUCH_DEVICE_LU202X
/* #define TOUCH_DEVICE_DUMMY */

#elif defined(TOUCH_MODEL_C70)

/* AP Solution */
#define TOUCH_PLATFORM_QCT

/* AP Chipset */
#define TOUCH_PLATFORM_MSM8916

/* Touch Device */
#define TOUCH_DEVICE_S3320

/* Swipe mode */
#define ENABLE_SWIPE_MODE

/*JDI In-cell*/
#define TOUCH_TYPE_INCELL

#elif defined(TOUCH_MODEL_YG)

/* AP Solution */
#define TOUCH_PLATFORM_QCT

/* AP Chipset */
#define TOUCH_PLATFORM_MSM8916

/* Touch Device */
#define TOUCH_DEVICE_S3320

/*JDI In-cell*/
#define TOUCH_TYPE_INCELL

/* Swipe mode */
#define DISABLE_SWIPE_MODE

#elif defined(TOUCH_MODEL_C90NAS)

/* AP Solution */
#define TOUCH_PLATFORM_QCT

/* AP Chipset */
#define TOUCH_PLATFORM_MSM8916

/* Touch Device */
#define TOUCH_DEVICE_S3320

/*JDI In-cell*/
#define TOUCH_TYPE_INCELL

/* Swipe mode */
#define ENABLE_SWIPE_MODE

#elif defined(TOUCH_MODEL_P1C)

/* AP Solution */
#define TOUCH_PLATFORM_QCT

/* AP Chipset */
#define TOUCH_PLATFORM_MSM8916

/* Touch Device */
#define TOUCH_DEVICE_S3320

/* Driver Feature */
#define ENABLE_TOUCH_AT_OFF_CHARGING

/* Swipe mode */
#define ENABLE_SWIPE_MODE

#elif defined(TOUCH_MODEL_Y90)

/* AP Solution */
#define TOUCH_PLATFORM_MTK

/* AP Chipset */
#define TOUCH_PLATFORM_MT6582

/* Touch Device */
#define TOUCH_DEVICE_S3320

/*JDI In-cell*/
#define TOUCH_TYPE_INCELL

#elif defined(TOUCH_MODEL_Y70)

/* AP Solution */
#define TOUCH_PLATFORM_MTK

/* AP Chipset */
#define TOUCH_PLATFORM_MT6582

/* Touch Device */
#define TOUCH_DEVICE_S3320

/* Swipe mode */
#define ENABLE_SWIPE_MODE

/*JDI In-cell*/
#define TOUCH_TYPE_INCELL

#elif defined(TOUCH_MODEL_Y50)

/* AP Solution */
#define TOUCH_PLATFORM_MTK

/* AP Chipset */
#define TOUCH_PLATFORM_MT6582

/* Touch Device */
#define TOUCH_DEVICE_MIT200

/*LGD In-cell*/
#define TOUCH_TYPE_INCELL

#elif defined(TOUCH_MODEL_C90)

/* AP Solution */
#define TOUCH_PLATFORM_MTK

/* AP Chipset */
#define TOUCH_PLATFORM_MT6732

/* Touch Device */
#define TOUCH_DEVICE_S3320

/*JDI In-cell*/
#define TOUCH_TYPE_INCELL

#elif defined(TOUCH_MODEL_C50)

/* AP Solution */
#define TOUCH_PLATFORM_QCT

/* AP Chipset */
#define TOUCH_PLATFORM_MSM8916

/* Touch Device */
#define TOUCH_DEVICE_MIT200

/*LGD In-cell*/
#define TOUCH_LGD_PHASE2

/* Swipe mode */
#define ENABLE_SWIPE_MODE

#elif defined(TOUCH_MODEL_LION_3G)

/* AP Solution */
#define TOUCH_PLATFORM_MTK

/* AP Chipset */
#define TOUCH_PLATFORM_MT6582

/* Touch Device */
#define TOUCH_DEVICE_TD4191

/* Swipe mode */
#define ENABLE_SWIPE_MODE

/*TD4191 In-cell*/
#define TOUCH_TYPE_INCELL

/* LCD notify function use for suspend/resume */
/* #define CONFIG_USE_LCD_NOTIFY_FUNCTION */

#elif defined(TOUCH_MODEL_P1B)
/* AP Solution */
#define TOUCH_PLATFORM_QCT

/* AP Chipset */
#define TOUCH_PLATFORM_MSM8939

/* Touch Device */
#define TOUCH_DEVICE_S3320

/*JDI In-cell*/
#define TOUCH_TYPE_INCELL

/* Swipe mode */
#define ENABLE_SWIPE_MODE

#elif defined(TOUCH_MODEL_M1V) || defined(TOUCH_MODEL_M4)
/* AP Solution */
#define TOUCH_PLATFORM_MTK

/* AP Chipset */
#define TOUCH_PLATFORM_MT6735P

/* Touch Device */
#define TOUCH_DEVICE_MIT300

/*JDI In-cell*/
#define TOUCH_TYPE_INCELL

#elif defined(TOUCH_MODEL_K7)

#define TOUCH_PLATFORM_MTK

#define TOUCH_PLATFORM_MT6735P

#define TOUCH_DEVICE_FT8707

#define TOUCH_TYPE_INCELL
#else
#error "Model should be defined"
#endif

#endif				/* _LGTP_PROJECT_SETTING_H_ */

/* End Of File */
