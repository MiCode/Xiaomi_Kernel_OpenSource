/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SF_USER_H__
#define __SF_USER_H__

#include "sf_def.h"

//-----------------------------------------------------------------------------

// platform select
#define SF_PLATFORM_SEL             SF_REE_MTK
// compatible select
#define SF_COMPATIBLE_SEL           SF_COMPATIBLE_REE //SF_COMPATIBLE_NOF
// power mode select
#define SF_POWER_MODE_SEL           PWR_MODE_NOF
// debug log select
#define SF_LOG_ENABLE               1
//-----------------------------------------------------------------------------

/* Dts node. */
#define COMPATIBLE_SW_FP            "mediatek,mt6753-fingerprint"

// for not mediatek
#define COMPATIBLE_RESET_GPIO       "sunwave,gpio_reset"
#define COMPATIBLE_IRQ_GPIO         "sunwave,gpio_irq"
#define COMPATIBLE_PWR_GPIO         "sunwave,gpio_pwr"

//for mediatek pinctl system
#define FINGER_POWER_ON             "finger_power_high"
#define FINGER_POWER_OFF            "finger_power_low"
#define FINGER_RESET_LOW            "fingerprint_reset_low"
#define FINGER_RESET_HIGH           "fingerprint_reset_high"
#define FINGER_INT_SET              "fingerprint_pin_irq"
// spi dts config enable
#define SF_SPI_DTS_CS               0
#define SF_SPI_DTS_Ck               0
#define SF_SPI_DTS_MI               0
#define SF_SPI_DTS_MO               0
#define SF_SPI_DTS_MI_PU            0
#define SF_SPI_DTS_MI_PD            0
#define SF_SPI_DTS_MO_PU            0
#define SF_SPI_DTS_MO_PD            0
#define FINGER_CS_SET               "finger_mode_as_cs"
#define FINGER_CK_SET               "finger_mode_as_ck"
#define FINGER_MI_SET               "finger_mode_as_mi"
#define FINGER_MO_SET               "finger_mode_as_mo"
#define FINGER_MI_PU                "miso_pull_up"
#define FINGER_MI_PD                "miso_pull_down"
#define FINGER_MO_PU                "mosi_pull_up"
#define FINGER_MO_PD                "mosi_pull_down"

/* regulator VDD select */
#define SF_VDD_NAME                 "vfingerprint"
#define SF_VDD_MIN_UV               2800000
#define SF_VDD_MAX_UV               2800000
//-----------------------------------------------------------------------------

#define ANDROID_WAKELOCK            1
#define SF_INT_TRIG_HIGH            0
//-----------------------------------------------------------------------------

//for mtk6739 kernel4.4 spi speed mode
#ifdef CONFIG_MACH_MT6739
#define MTK_6739_SPEED_MODE         1
#else
#define MTK_6739_SPEED_MODE         1
#endif
//-----------------------------------------------------------------------------

//for qualcomm ree spi deassert wait time
#define QUALCOMM_REE_DEASSERT       0

//-----------------------------------------------------------------------------

#if (SF_PLATFORM_SEL == SF_REE_MTK_L5_X)
//android mtk androidL 5.x none dts config file
#define MTK_L5_X_POWER_ON           1
#define MTK_L5_X_IRQ_SET            0

//power GPIO
#if MTK_L5_X_POWER_ON
#define  GPIO_SW_PWR_PIN            GPIO_FIGERPRINT_PWR_EN_PIN
#define  GPIO_SW_PWR_M_GPIO         GPIO_MODE_00
#endif

//interrupt pin
#define  GPIO_SW_IRQ_NUM            CUST_EINT_FIGERPRINT_INT_NUM
#define  GPIO_SW_INT_PIN            GPIO_FIGERPRINT_INT

//reset pin
#define  GPIO_SW_RST_PIN            GPIO_FIGERPRINT_RST
#define  GPIO_SW_RST_PIN_M_GPIO     GPIO_MODE_00

#define  GPIO_SW_RST_M_DAIPCMOUT    GPIO_MODE_01

//interrupt mode
#if MTK_L5_X_IRQ_SET
#define  GPIO_SW_EINT_PIN_M_GPIO    GPIO_FIGERPRINT_INT_M_GPIO //GPIO_MODE_00
#define  GPIO_SW_EINT_PIN_M_EINT    GPIO_FIGERPRINT_INT_M_EINT //GPIO_MODE_00
#endif

#endif
//-----------------------------------------------------------------------------
#include "sf_auto.h"

#endif
