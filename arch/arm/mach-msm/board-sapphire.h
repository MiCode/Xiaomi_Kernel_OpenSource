/* linux/arch/arm/mach-msm/board-sapphire.h
 * Copyright (C) 2007-2009 HTC Corporation.
 * Author: Thomas Tsai <thomas_tsai@htc.com>
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
#ifndef __ARCH_ARM_MACH_MSM_BOARD_SAPPHIRE_H
#define __ARCH_ARM_MACH_MSM_BOARD_SAPPHIRE_H

#include <mach/board.h>

#define MSM_SMI_BASE		0x00000000
#define MSM_SMI_SIZE		0x00800000

#define MSM_EBI_BASE		0x10000000
#define MSM_EBI_SIZE		0x07100000

#define MSM_PMEM_GPU0_BASE	0x00000000
#define MSM_PMEM_GPU0_SIZE	0x00700000

#define SMI64_MSM_PMEM_MDP_BASE	0x15900000
#define SMI64_MSM_PMEM_MDP_SIZE	0x00800000

#define SMI64_MSM_PMEM_ADSP_BASE	0x16100000
#define SMI64_MSM_PMEM_ADSP_SIZE	0x00800000

#define SMI64_MSM_PMEM_CAMERA_BASE	0x15400000
#define SMI64_MSM_PMEM_CAMERA_SIZE	0x00500000

#define SMI64_MSM_FB_BASE		0x00700000
#define SMI64_MSM_FB_SIZE		0x00100000

#define SMI64_MSM_LINUX_BASE		MSM_EBI_BASE
#define SMI64_MSM_LINUX_SIZE		0x068e0000

#define SMI64_MSM_LINUX_BASE_1		0x02000000
#define SMI64_MSM_LINUX_SIZE_1		0x02000000

#define SMI64_MSM_LINUX_BASE_2		MSM_EBI_BASE
#define SMI64_MSM_LINUX_SIZE_2		0x05400000

#define SMI32_MSM_LINUX_BASE		MSM_EBI_BASE
#define SMI32_MSM_LINUX_SIZE		0x5400000

#define SMI32_MSM_PMEM_MDP_BASE	SMI32_MSM_LINUX_BASE + SMI32_MSM_LINUX_SIZE
#define SMI32_MSM_PMEM_MDP_SIZE	0x800000

#define SMI32_MSM_PMEM_ADSP_BASE	SMI32_MSM_PMEM_MDP_BASE + SMI32_MSM_PMEM_MDP_SIZE
#define SMI32_MSM_PMEM_ADSP_SIZE	0x800000

#define SMI32_MSM_FB_BASE		SMI32_MSM_PMEM_ADSP_BASE + SMI32_MSM_PMEM_ADSP_SIZE
#define SMI32_MSM_FB_SIZE		0x9b000


#define MSM_PMEM_GPU1_SIZE	0x800000
#define MSM_PMEM_GPU1_BASE     (MSM_RAM_CONSOLE_BASE + MSM_RAM_CONSOLE_SIZE)

#define MSM_RAM_CONSOLE_BASE	0x169E0000
#define MSM_RAM_CONSOLE_SIZE	128 * SZ_1K

#if (SMI32_MSM_FB_BASE + SMI32_MSM_FB_SIZE) >= (MSM_PMEM_GPU1_BASE)
#error invalid memory map
#endif

#if (SMI64_MSM_FB_BASE + SMI64_MSM_FB_SIZE) >= (MSM_PMEM_GPU1_BASE)
#error invalid memory map
#endif

#define DECLARE_MSM_IOMAP
#include <mach/msm_iomap.h>

/*
** SOC GPIO
*/
#define SAPPHIRE_BALL_UP_0     94
#define SAPPHIRE_BALL_LEFT_0   18
#define SAPPHIRE_BALL_DOWN_0   49
#define SAPPHIRE_BALL_RIGHT_0  19

#define SAPPHIRE_POWER_KEY     20
#define SAPPHIRE_VOLUME_UP     36
#define SAPPHIRE_VOLUME_DOWN   39

#define SAPPHIRE_GPIO_PS_HOLD   (25)
#define SAPPHIRE_MDDI_1V5_EN	(28)
#define SAPPHIRE_BL_PWM			(27)
#define SAPPHIRE_TP_LS_EN    	(1)
#define SAPPHIRE20_TP_LS_EN			(88)

/* H2W */
#define SAPPHIRE_GPIO_CABLE_IN1		(83)
#define SAPPHIRE_GPIO_CABLE_IN2		(37)
#define SAPPHIRE_GPIO_UART3_RX		(86)
#define SAPPHIRE_GPIO_UART3_TX		(87)
#define SAPPHIRE_GPIO_H2W_DATA		(86)
#define SAPPHIRE_GPIO_H2W_CLK		(87)

#define SAPPHIRE_GPIO_UART1_RTS		(43)
#define SAPPHIRE_GPIO_UART1_CTS		(44)

/*
** CPLD GPIO
**
** Sapphire Altera CPLD can keep the registers value and
** doesn't need a shadow to backup.
**/
#define SAPPHIRE_CPLD_BASE   0xFA000000	/* VA */
#define SAPPHIRE_CPLD_START  0x98000000	/* PA */
#define SAPPHIRE_CPLD_SIZE   SZ_4K

#define SAPPHIRE_GPIO_START (128)				/* Pseudo GPIO number */

/* Sapphire has one INT BANK only. */
#define SAPPHIRE_GPIO_INT_B0_MASK_REG           (0x0c)	/*INT3 MASK*/
#define SAPPHIRE_GPIO_INT_B0_STAT_REG           (0x0e)	/*INT1 STATUS*/

/* LED control register */
#define SAPPHIRE_CPLD_LED_BASE									(SAPPHIRE_CPLD_BASE + 0x10)		/* VA */
#define SAPPHIRE_CPLD_LED_START									(SAPPHIRE_CPLD_START + 0x10)	/* PA */
#define SAPPHIRE_CPLD_LED_SIZE									0x08

/* MISCn: GPO pin to Enable/Disable some functions. */
#define SAPPHIRE_GPIO_MISC1_BASE               	(SAPPHIRE_GPIO_START + 0x00)
#define SAPPHIRE_GPIO_MISC2_BASE               	(SAPPHIRE_GPIO_START + 0x08)
#define SAPPHIRE_GPIO_MISC3_BASE               	(SAPPHIRE_GPIO_START + 0x10)
#define SAPPHIRE_GPIO_MISC4_BASE               	(SAPPHIRE_GPIO_START + 0x18)
#define SAPPHIRE_GPIO_MISC5_BASE               	(SAPPHIRE_GPIO_START + 0x20)

/* INT BANK0: INT1: int status, INT2: int level, INT3: int Mask */
#define SAPPHIRE_GPIO_INT_B0_BASE              	(SAPPHIRE_GPIO_START + 0x28)

/* MISCn GPIO: */
#define SAPPHIRE_GPIO_CPLD128_VER_0            	(SAPPHIRE_GPIO_MISC1_BASE + 4)
#define SAPPHIRE_GPIO_CPLD128_VER_1            	(SAPPHIRE_GPIO_MISC1_BASE + 5)
#define SAPPHIRE_GPIO_CPLD128_VER_2            	(SAPPHIRE_GPIO_MISC1_BASE + 6)
#define SAPPHIRE_GPIO_CPLD128_VER_3            	(SAPPHIRE_GPIO_MISC1_BASE + 7)

#define SAPPHIRE_GPIO_H2W_DAT_DIR              	(SAPPHIRE_GPIO_MISC2_BASE + 2)
#define SAPPHIRE_GPIO_H2W_CLK_DIR              	(SAPPHIRE_GPIO_MISC2_BASE + 3)
#define SAPPHIRE_GPIO_H2W_SEL0                 	(SAPPHIRE_GPIO_MISC2_BASE + 6)
#define SAPPHIRE_GPIO_H2W_SEL1                 	(SAPPHIRE_GPIO_MISC2_BASE + 7)

#define SAPPHIRE_GPIO_I2C_PULL                 	(SAPPHIRE_GPIO_MISC3_BASE + 2)
#define SAPPHIRE_GPIO_TP_EN                    	(SAPPHIRE_GPIO_MISC3_BASE + 4)
#define SAPPHIRE_GPIO_JOG_EN                   	(SAPPHIRE_GPIO_MISC3_BASE + 5)
#define SAPPHIRE_GPIO_JOG_LED_EN               	(SAPPHIRE_GPIO_MISC3_BASE + 6)
#define SAPPHIRE_GPIO_APKEY_LED_EN             	(SAPPHIRE_GPIO_MISC3_BASE + 7)

#define SAPPHIRE_GPIO_VCM_PWDN                 	(SAPPHIRE_GPIO_MISC4_BASE + 0)
#define SAPPHIRE_GPIO_USB_H2W_SW               	(SAPPHIRE_GPIO_MISC4_BASE + 1)
#define SAPPHIRE_GPIO_COMPASS_RST_N            	(SAPPHIRE_GPIO_MISC4_BASE + 2)
#define SAPPHIRE_GPIO_USB_PHY_RST_N            	(SAPPHIRE_GPIO_MISC4_BASE + 5)
#define SAPPHIRE_GPIO_WIFI_PA_RESETX           	(SAPPHIRE_GPIO_MISC4_BASE + 6)
#define SAPPHIRE_GPIO_WIFI_EN                  	(SAPPHIRE_GPIO_MISC4_BASE + 7)

#define SAPPHIRE_GPIO_BT_32K_EN                	(SAPPHIRE_GPIO_MISC5_BASE + 0)
#define SAPPHIRE_GPIO_MAC_32K_EN               	(SAPPHIRE_GPIO_MISC5_BASE + 1)
#define SAPPHIRE_GPIO_MDDI_32K_EN              	(SAPPHIRE_GPIO_MISC5_BASE + 2)
#define SAPPHIRE_GPIO_COMPASS_32K_EN           	(SAPPHIRE_GPIO_MISC5_BASE + 3)

/* INT STATUS/LEVEL/MASK : INT GPIO should be the last. */
#define SAPPHIRE_GPIO_NAVI_ACT_N           		(SAPPHIRE_GPIO_INT_B0_BASE + 0)
#define SAPPHIRE_GPIO_COMPASS_IRQ         		(SAPPHIRE_GPIO_INT_B0_BASE + 1)
#define SAPPHIRE_GPIO_SEARCH_ACT_N			(SAPPHIRE_GPIO_INT_B0_BASE + 2)
#define SAPPHIRE_GPIO_AUD_HSMIC_DET_N      		(SAPPHIRE_GPIO_INT_B0_BASE + 3)
#define SAPPHIRE_GPIO_SDMC_CD_N      			(SAPPHIRE_GPIO_INT_B0_BASE + 4)
#define SAPPHIRE_GPIO_CAM_BTN_STEP1_N          	(SAPPHIRE_GPIO_INT_B0_BASE + 5)
#define SAPPHIRE_GPIO_CAM_BTN_STEP2_N          	(SAPPHIRE_GPIO_INT_B0_BASE + 6)
#define SAPPHIRE_GPIO_TP_ATT_N            		(SAPPHIRE_GPIO_INT_B0_BASE + 7)

#define	SAPPHIRE_GPIO_END						SAPPHIRE_GPIO_TP_ATT_N
#define	SAPPHIRE_GPIO_LAST_INT					(SAPPHIRE_GPIO_TP_ATT_N)

/* Bit position in the CPLD MISCn by the CPLD GPIOn: only bit0-7 is used. */
#define	CPLD_GPIO_BIT_POS_MASK(n)		(1U << ((n) & 7))
#define	CPLD_GPIO_REG_OFFSET(n)			_g_CPLD_MISCn_Offset[((n)-SAPPHIRE_GPIO_START) >> 3]
#define	CPLD_GPIO_REG(n)				(CPLD_GPIO_REG_OFFSET(n) + SAPPHIRE_CPLD_BASE)

/*
** CPLD INT Start
*/
#define SAPPHIRE_INT_START 					(NR_MSM_IRQS + NR_GPIO_IRQS)	/* pseudo number for CPLD INT */
/* Using INT status/Bank0 for GPIO to INT */
#define	SAPPHIRE_GPIO_TO_INT(n)				((n-SAPPHIRE_GPIO_INT_B0_BASE) + SAPPHIRE_INT_START)
#define SAPPHIRE_INT_END 					(SAPPHIRE_GPIO_TO_INT(SAPPHIRE_GPIO_END))

/* get the INT reg by GPIO number */
#define	CPLD_INT_GPIO_TO_BANK(n)			(((n)-SAPPHIRE_GPIO_INT_B0_BASE) >> 3)
#define	CPLD_INT_STATUS_REG_OFFSET_G(n)		_g_INT_BANK_Offset[CPLD_INT_GPIO_TO_BANK(n)][0]
#define	CPLD_INT_LEVEL_REG_OFFSET_G(n)		_g_INT_BANK_Offset[CPLD_INT_GPIO_TO_BANK(n)][1]
#define	CPLD_INT_MASK_REG_OFFSET_G(n)		_g_INT_BANK_Offset[CPLD_INT_GPIO_TO_BANK(n)][2]
#define	CPLD_INT_STATUS_REG_G(n)			(SAPPHIRE_CPLD_BASE + CPLD_INT_STATUS_REG_OFFSET_G(n))
#define	CPLD_INT_LEVEL_REG_G(n)				(SAPPHIRE_CPLD_BASE + CPLD_INT_LEVEL_REG_OFFSET_G(n))
#define	CPLD_INT_MASK_REG_G(n)				(SAPPHIRE_CPLD_BASE + CPLD_INT_MASK_REG_OFFSET_G(n))

/* get the INT reg by INT number */
#define	CPLD_INT_TO_BANK(i)					((i-SAPPHIRE_INT_START) >> 3)
#define	CPLD_INT_STATUS_REG_OFFSET(i)		_g_INT_BANK_Offset[CPLD_INT_TO_BANK(i)][0]
#define	CPLD_INT_LEVEL_REG_OFFSET(i)		_g_INT_BANK_Offset[CPLD_INT_TO_BANK(i)][1]
#define	CPLD_INT_MASK_REG_OFFSET(i)			_g_INT_BANK_Offset[CPLD_INT_TO_BANK(i)][2]
#define	CPLD_INT_STATUS_REG(i)				(SAPPHIRE_CPLD_BASE + CPLD_INT_STATUS_REG_OFFSET(i))
#define	CPLD_INT_LEVEL_REG(i)				(SAPPHIRE_CPLD_BASE + CPLD_INT_LEVEL_REG_OFFSET(i))
#define	CPLD_INT_MASK_REG(i)				(SAPPHIRE_CPLD_BASE + CPLD_INT_MASK_REG_OFFSET(i) )

/* return the bit mask by INT number */
#define SAPPHIRE_INT_BIT_MASK(i) 			(1U << ((i - SAPPHIRE_INT_START) & 7))

void config_sapphire_camera_on_gpios(void);
void config_sapphire_camera_off_gpios(void);
int sapphire_get_smi_size(void);
unsigned int sapphire_get_hwid(void);
unsigned int sapphire_get_skuid(void);
unsigned int is_12pin_camera(void);
int sapphire_is_5M_camera(void);
int sapphire_gpio_write(struct gpio_chip *chip, unsigned n, unsigned on);

#endif /* GUARD */
