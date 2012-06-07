/* arch/arm/mach-msm/board-mahimahi.h
 *
 * Copyright (C) 2009 HTC Corporation.
 * Author: Haley Teng <Haley_Teng@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __ARCH_ARM_MACH_MSM_BOARD_MAHIMAHI_H
#define __ARCH_ARM_MACH_MSM_BOARD_MAHIMAHI_H

#include <mach/board.h>

#define MSM_SMI_BASE		0x02B00000
#define MSM_SMI_SIZE		0x01500000

#define MSM_RAM_CONSOLE_BASE	0x03A00000
#define MSM_RAM_CONSOLE_SIZE	0x00040000

#define MSM_FB_BASE		0x03B00000
#define MSM_FB_SIZE		0x00465000

#define MSM_EBI1_BANK0_BASE	0x20000000
#define MSM_EBI1_BANK0_SIZE	0x0E000000

#define MSM_GPU_MEM_BASE	0x2DB00000
#define MSM_GPU_MEM_SIZE	0x00500000

#define MSM_EBI1_BANK1_BASE	0x30000000
#define MSM_EBI1_BANK1_SIZE	0x10000000

#define MSM_PMEM_MDP_BASE	0x30000000
#define MSM_PMEM_MDP_SIZE	0x02000000

#define MSM_PMEM_ADSP_BASE	0x32000000
#define MSM_PMEM_ADSP_SIZE	0x02900000

#define MSM_PMEM_CAMERA_BASE	0x34900000
#define MSM_PMEM_CAMERA_SIZE	0x00800000

#define MSM_HIGHMEM_BASE	0x35100000
#define MSM_HIGHMEM_SIZE	0x0AF00000

#define MAHIMAHI_GPIO_PS_HOLD		25

#define MAHIMAHI_GPIO_UP_INT_N		35
#define MAHIMAHI_GPIO_UP_RESET_N	82
#define MAHIMAHI_GPIO_LS_EN_N		119

#define MAHIMAHI_GPIO_TP_INT_N		92
#define MAHIMAHI_GPIO_TP_LS_EN		93
#define MAHIMAHI_GPIO_TP_EN		160

#define MAHIMAHI_GPIO_POWER_KEY		94
#define MAHIMAHI_GPIO_SDMC_CD_REV0_N	153

#define MAHIMAHI_GPIO_WIFI_SHUTDOWN_N	127
#define MAHIMAHI_GPIO_WIFI_IRQ		152

#define MAHIMAHI_GPIO_BALL_UP		38
#define MAHIMAHI_GPIO_BALL_DOWN		37
#define MAHIMAHI_GPIO_BALL_LEFT		145
#define MAHIMAHI_GPIO_BALL_RIGHT	21

#define MAHIMAHI_GPIO_BT_UART1_RTS	43
#define MAHIMAHI_GPIO_BT_UART1_CTS	44
#define MAHIMAHI_GPIO_BT_UART1_RX	45
#define MAHIMAHI_GPIO_BT_UART1_TX	46
#define MAHIMAHI_GPIO_BT_RESET_N	146
#define MAHIMAHI_GPIO_BT_SHUTDOWN_N	128

#define MAHIMAHI_GPIO_BT_WAKE		57
#define MAHIMAHI_GPIO_BT_HOST_WAKE	86

#define MAHIMAHI_GPIO_PROXIMITY_INT_N	90
#define MAHIMAHI_GPIO_PROXIMITY_EN	120

#define MAHIMAHI_GPIO_DS2482_SLP_N	87
#define MAHIMAHI_GPIO_VIBRATOR_ON	89
/* Compass */
#define MAHIMAHI_REV0_GPIO_COMPASS_INT_N	36

#define MAHIMAHI_GPIO_COMPASS_INT_N	153
#define MAHIMAHI_GPIO_COMPASS_RST_N	107
#define MAHIMAHI_PROJECT_NAME          "mahimahi"
#define MAHIMAHI_LAYOUTS { 			   \
	{ {-1,  0, 0}, { 0, -1,  0}, {0, 0,  1} }, \
	{ { 0, -1, 0}, { 1,  0,  0}, {0, 0, -1} }, \
	{ { 0, -1, 0}, { 1,  0,  0}, {0, 0,  1} }, \
	{ {-1,  0, 0}, { 0,  0, -1}, {0, 1,  0} }  \
}

/* Audio */
#define MAHIMAHI_AUD_JACKHP_EN		157
#define MAHIMAHI_AUD_2V5_EN		158
#define MAHIMAHI_AUD_MICPATH_SEL 	111
#define MAHIMAHI_AUD_A1026_INT		112
#define MAHIMAHI_AUD_A1026_WAKEUP 	113
#define MAHIMAHI_AUD_A1026_RESET 	129
#define MAHIMAHI_AUD_A1026_CLK		 -1
#define MAHIMAHI_CDMA_XA_AUD_A1026_CLK	105
/* NOTE: MAHIMAHI_CDMA_XB_AUD_A1026_WAKEUP on CDMA is the same GPIO as
 * MAHIMAHI_GPIO_BATTERY_CHARGER_CURRENT on UMTS.  Also,
 * MAHIMAHI_CDMA_XB_AUD_A1026_RESET is the same as
 * GPIO MAHIMAHI_GPIO_35MM_KEY_INT_SHUTDOWN on UMTS.
 */
#define MAHIMAHI_CDMA_XB_AUD_A1026_WAKEUP	16
#define MAHIMAHI_CDMA_XB_AUD_A1026_RESET	19
#define MAHIMAHI_CDMA_XB_AUD_A1026_CLK	-1

/* Bluetooth PCM */
#define MAHIMAHI_BT_PCM_OUT		68
#define MAHIMAHI_BT_PCM_IN		69
#define MAHIMAHI_BT_PCM_SYNC		70
#define MAHIMAHI_BT_PCM_CLK		71
/* flash light */
#define MAHIMAHI_GPIO_FLASHLIGHT_TORCH	58
#define MAHIMAHI_GPIO_FLASHLIGHT_FLASH	84

#define MAHIMAHI_GPIO_LED_3V3_EN	85
#define MAHIMAHI_GPIO_LCD_RST_N		29
#define MAHIMAHI_GPIO_LCD_ID0		147

/* 3.5mm remote control key interrupt shutdown signal */
#define MAHIMAHI_GPIO_35MM_KEY_INT_SHUTDOWN	19

#define MAHIMAHI_GPIO_DOCK		106

/* speaker amplifier enable pin for mahimahi CDMA version */
#define MAHIMAHI_CDMA_GPIO_AUD_SPK_AMP_EN	104

#define MAHIMAHI_GPIO_BATTERY_DETECTION		39
#define MAHIMAHI_GPIO_BATTERY_CHARGER_EN	22
#define MAHIMAHI_GPIO_BATTERY_CHARGER_CURRENT	16

#define MAHIMAHI_CDMA_GPIO_BT_WAKE		28
#define MAHIMAHI_CDMA_GPIO_FLASHLIGHT_TORCH	26

#define MAHIMAHI_CDMA_SD_2V85_EN		100
#define MAHIMAHI_CDMA_JOG_2V6_EN		150
/* display relative */
#define MAHIMAHI_LCD_SPI_CLK            (17)
#define MAHIMAHI_LCD_SPI_DO             (18)
#define MAHIMAHI_LCD_SPI_CSz            (20)
#define MAHIMAHI_LCD_RSTz               (29)
#define MAHIMAHI_LCD_R1                 (114)
#define MAHIMAHI_LCD_R2                 (115)
#define MAHIMAHI_LCD_R3                 (116)
#define MAHIMAHI_LCD_R4                 (117)
#define MAHIMAHI_LCD_R5                 (118)
#define MAHIMAHI_LCD_G0                 (121)
#define MAHIMAHI_LCD_G1                 (122)
#define MAHIMAHI_LCD_G2                 (123)
#define MAHIMAHI_LCD_G3                 (124)
#define MAHIMAHI_LCD_G4                 (125)
#define MAHIMAHI_LCD_G5                 (126)
#define MAHIMAHI_LCD_B1                 (130)
#define MAHIMAHI_LCD_B2                 (131)
#define MAHIMAHI_LCD_B3                 (132)
#define MAHIMAHI_LCD_B4                 (133)
#define MAHIMAHI_LCD_B5                 (134)
#define MAHIMAHI_LCD_PCLK               (135)
#define MAHIMAHI_LCD_VSYNC              (136)
#define MAHIMAHI_LCD_HSYNC              (137)
#define MAHIMAHI_LCD_DE                 (138)
#define is_cdma_version(rev) (((rev) & 0xF0) == 0xC0)

#endif /* __ARCH_ARM_MACH_MSM_BOARD_MAHIMAHI_H */
