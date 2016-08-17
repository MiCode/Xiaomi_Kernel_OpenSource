/*
 * linux/arch/arm/mach-tegra/pinmux-t11-tables.c
 *
 * Common pinmux configurations for Tegra11x SoCs
 *
 * Copyright (C) 2011-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/syscore_ops.h>
#include <linux/bug.h>
#include <linux/bitops.h>

#include <mach/iomap.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t11.h>

#include "gpio-names.h"

#define PINGROUP_REG_A	0x868
#define MUXCTL_REG_A	0x3000

#define SET_DRIVE_PINGROUP(pg_name, r, drv_down_offset, drv_down_mask,	\
	drv_up_offset, drv_up_mask, slew_rise_offset, slew_rise_mask,	\
	slew_fall_offset, slew_fall_mask, drv_type_valid,		\
	drv_type_offset, drv_type_mask, _dev_id)			\
	[TEGRA_DRIVE_PINGROUP_ ## pg_name] = {			\
		.name = #pg_name,				\
		.reg_bank = 0,					\
		.reg = ((r) - PINGROUP_REG_A),			\
		.drvup_offset = drv_up_offset,			\
		.drvup_mask = drv_up_mask,			\
		.drvdown_offset = drv_down_offset,		\
		.drvdown_mask = drv_down_mask,			\
		.slewrise_offset = slew_rise_offset,		\
		.slewrise_mask = slew_rise_mask,		\
		.slewfall_offset = slew_fall_offset,		\
		.slewfall_mask = slew_fall_mask,		\
		.drvtype_valid = drv_type_valid,		\
		.drvtype_offset = drv_type_offset,		\
		.drvtype_mask = drv_type_mask,			\
		.dev_id = _dev_id,				\
	}

#define DEFAULT_DRIVE_PINGROUP(pg_name, r)		\
	[TEGRA_DRIVE_PINGROUP_ ## pg_name] = {		\
		.name = #pg_name,			\
		.reg_bank = 0,				\
		.reg = ((r) - PINGROUP_REG_A),		\
		.drvup_offset = 20,			\
		.drvup_mask = 0x1f,			\
		.drvdown_offset = 12,			\
		.drvdown_mask = 0x1f,			\
		.slewrise_offset = 28,			\
		.slewrise_mask = 0x3,			\
		.slewfall_offset = 30,			\
		.slewfall_mask = 0x3,			\
		.drvtype_valid = 0,			\
		.drvtype_offset = 6,			\
		.drvtype_mask = 0x3,			\
		.dev_id = NULL				\
	}

const struct tegra_drive_pingroup_desc tegra_soc_drive_pingroups[TEGRA_MAX_DRIVE_PINGROUP] = {
	DEFAULT_DRIVE_PINGROUP(AO1,		0x868),
	DEFAULT_DRIVE_PINGROUP(AO2,		0x86c),
	DEFAULT_DRIVE_PINGROUP(AT1,		0x870),
	DEFAULT_DRIVE_PINGROUP(AT2,		0x874),
	DEFAULT_DRIVE_PINGROUP(AT3,		0x878),
	DEFAULT_DRIVE_PINGROUP(AT4,		0x87c),
	DEFAULT_DRIVE_PINGROUP(AT5,		0x880),
	DEFAULT_DRIVE_PINGROUP(CDEV1,		0x884),
	DEFAULT_DRIVE_PINGROUP(CDEV2,		0x888),
	DEFAULT_DRIVE_PINGROUP(CSUS,		0x88c),
	DEFAULT_DRIVE_PINGROUP(DAP1,		0x890),
	DEFAULT_DRIVE_PINGROUP(DAP2,		0x894),
	DEFAULT_DRIVE_PINGROUP(DAP3,		0x898),
	DEFAULT_DRIVE_PINGROUP(DAP4,		0x89c),
	DEFAULT_DRIVE_PINGROUP(DBG,		0x8a0),
	SET_DRIVE_PINGROUP(SDIO3,		0x8b0,	12,	0x7F,	20,
		0x7F,	28,	0x3,	30,	0x3,	0,	0,	0,
		"sdhci-tegra.2"),
	DEFAULT_DRIVE_PINGROUP(SPI,		0x8b4),
	DEFAULT_DRIVE_PINGROUP(UAA,		0x8b8),
	DEFAULT_DRIVE_PINGROUP(UAB,		0x8bc),
	DEFAULT_DRIVE_PINGROUP(UART2,		0x8c0),
	DEFAULT_DRIVE_PINGROUP(UART3,		0x8c4),
	SET_DRIVE_PINGROUP(SDIO1,		0x8ec,	12,	0x7F,	20,
		0x7F,	28,	0x3,	30,	0x3,	0,	0,	0,
		"sdhci-tegra.0"),
	DEFAULT_DRIVE_PINGROUP(CRT,		0x8f8),
	DEFAULT_DRIVE_PINGROUP(DDC,		0x8fc),
	SET_DRIVE_PINGROUP(GMA,			0x900,	14,	0x1F,	20,
		0x1F,	28,	0x3,	30,	0x3,	1,	6,	0x3,
		"sdhci-tegra.3"),
	DEFAULT_DRIVE_PINGROUP(GME,		0x910),
	DEFAULT_DRIVE_PINGROUP(GMF,		0x914),
	DEFAULT_DRIVE_PINGROUP(GMG,		0x918),
	DEFAULT_DRIVE_PINGROUP(GMH,		0x91c),
	DEFAULT_DRIVE_PINGROUP(OWR,		0x920),
	DEFAULT_DRIVE_PINGROUP(UAD,		0x924),
	DEFAULT_DRIVE_PINGROUP(GPV,		0x928),
	DEFAULT_DRIVE_PINGROUP(DEV3,		0x92c),
	DEFAULT_DRIVE_PINGROUP(CEC,		0x938),
	DEFAULT_DRIVE_PINGROUP(AT6,		0x994),
	DEFAULT_DRIVE_PINGROUP(DAP5,		0x998),
	DEFAULT_DRIVE_PINGROUP(VBUS,		0x99C),
};

#define PINGROUP(pg_name, gpio_nr, vdd, f0, f1, f2, f3, fs, iod, reg)	\
	[TEGRA_PINGROUP_ ## pg_name] = {			\
		.name = #pg_name,				\
		.vddio = TEGRA_VDDIO_ ## vdd,			\
		.funcs = {					\
			TEGRA_MUX_ ## f0,			\
			TEGRA_MUX_ ## f1,			\
			TEGRA_MUX_ ## f2,			\
			TEGRA_MUX_ ## f3,			\
		},						\
		.gpionr = TEGRA_GPIO_ ## gpio_nr,		\
		.func_safe = TEGRA_MUX_ ## fs,			\
		.tri_bank = 1,					\
		.tri_reg = ((reg) - MUXCTL_REG_A),		\
		.tri_bit = 4,					\
		.mux_bank = 1,					\
		.mux_reg = ((reg) - MUXCTL_REG_A),		\
		.mux_bit = 0,					\
		.pupd_bank = 1,					\
		.pupd_reg = ((reg) - MUXCTL_REG_A),		\
		.pupd_bit = 2,					\
		.io_default = TEGRA_PIN_ ## iod,		\
		.od_bit = 6,					\
		.lock_bit = 7,					\
		.ioreset_bit = 8,				\
		.rcv_sel_bit = 9,				\
	}

/* !!!FIXME!!! FILL IN fSafe COLUMN IN TABLE ....... */
#define PINGROUPS	\
	/*       NAME		  GPIO		VDD	    f0		f1          f2          f3           fSafe       io	reg */\
	PINGROUP(ULPI_DATA0,	  PO1,		BB,	    SPI3,	HSI,	    UARTA,	ULPI,	     ULPI,	INPUT,	0x3000),\
	PINGROUP(ULPI_DATA1,	  PO2,		BB,	    SPI3,	HSI,	    UARTA,	ULPI,	     ULPI,	INPUT,	0x3004),\
	PINGROUP(ULPI_DATA2,	  PO3,		BB,	    SPI3,	HSI,	    UARTA,	ULPI,	     ULPI,	INPUT,	0x3008),\
	PINGROUP(ULPI_DATA3,	  PO4,		BB,	    SPI3,	HSI,	    UARTA,	ULPI,	     ULPI,	INPUT,	0x300c),\
	PINGROUP(ULPI_DATA4,	  PO5,		BB,	    SPI2,	HSI,	    UARTA,	ULPI,	     ULPI,	INPUT,	0x3010),\
	PINGROUP(ULPI_DATA5,	  PO6,		BB,	    SPI2,	HSI,	    UARTA,	ULPI,	     ULPI,	INPUT,	0x3014),\
	PINGROUP(ULPI_DATA6,	  PO7,		BB,	    SPI2,	HSI,	    UARTA,	ULPI,	     ULPI,	INPUT,	0x3018),\
	PINGROUP(ULPI_DATA7,	  PO0,		BB,	    SPI2,	HSI,	    UARTA,	ULPI,	     ULPI,	INPUT,	0x301c),\
	PINGROUP(ULPI_CLK,	  PY0,		BB,	    SPI1,	SPI5,	    UARTD,	ULPI,	     ULPI,	INPUT,	0x3020),\
	PINGROUP(ULPI_DIR,	  PY1,		BB,	    SPI1,	SPI5,	    UARTD,	ULPI,	     ULPI,	INPUT,	0x3024),\
	PINGROUP(ULPI_NXT,	  PY2,		BB,	    SPI1,	SPI5,	    UARTD,	ULPI,	     ULPI,	INPUT,	0x3028),\
	PINGROUP(ULPI_STP,	  PY3,		BB,	    SPI1,	SPI5,	    UARTD,	ULPI,	     ULPI,	INPUT,	0x302c),\
	PINGROUP(DAP3_FS,	  PP0,		BB,	    I2S2,	SPI5,	    DISPLAYA,	DISPLAYB,       I2S2,	INPUT,	0x3030),\
	PINGROUP(DAP3_DIN,	  PP1,		BB,	    I2S2,	SPI5,	    DISPLAYA,	DISPLAYB,       I2S2,	INPUT,	0x3034),\
	PINGROUP(DAP3_DOUT,	  PP2,		BB,	    I2S2,	SPI5,	    DISPLAYA,	DISPLAYB,       I2S2,	INPUT,	0x3038),\
	PINGROUP(DAP3_SCLK,	  PP3,		BB,	    I2S2,	SPI5,	    DISPLAYA,	DISPLAYB,       I2S2,	INPUT,	0x303c),\
	PINGROUP(GPIO_PV0,	  PV0,		BB,	    USB,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3040),\
	PINGROUP(GPIO_PV1,	  PV1,		BB,	    RSVD0,	RSVD1,	    RSVD2,	RSVD3,	     RSVD0,	INPUT,	0x3044),\
	PINGROUP(SDMMC1_CLK,	  PZ0,		SDMMC1,     SDMMC1,	CLK12,	    RSVD2,	RSVD3,       RSVD2,	INPUT,	0x3048),\
	PINGROUP(SDMMC1_CMD,	  PZ1,		SDMMC1,     SDMMC1,	SPDIF,	    SPI4,	UARTA,       SDMMC1,	INPUT,	0x304c),\
	PINGROUP(SDMMC1_DAT3,	  PY4,		SDMMC1,     SDMMC1,	SPDIF,	    SPI4,	UARTA,       SDMMC1,	INPUT,	0x3050),\
	PINGROUP(SDMMC1_DAT2,	  PY5,		SDMMC1,     SDMMC1,	PWM0,	    SPI4,	UARTA,       SDMMC1,	INPUT,	0x3054),\
	PINGROUP(SDMMC1_DAT1,	  PY6,		SDMMC1,     SDMMC1,	PWM1,	    SPI4,	UARTA,       SDMMC1,	INPUT,	0x3058),\
	PINGROUP(SDMMC1_DAT0,	  PY7,		SDMMC1,     SDMMC1,	RSVD1,	    SPI4,	UARTA,       RSVD1,	INPUT,	0x305c),\
	PINGROUP(CLK2_OUT,	  PW5,		SDMMC1,     EXTPERIPH2,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3068),\
	PINGROUP(CLK2_REQ,	  PCC5,		SDMMC1,     DAP,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x306c),\
	PINGROUP(HDMI_INT,	  PN7,		LCD,	    RSVD0,	RSVD1,	    RSVD2,	RSVD3,	     RSVD0,	INPUT,	0x3110),\
	PINGROUP(DDC_SCL,	  PV4,		LCD,	    I2C4,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3114),\
	PINGROUP(DDC_SDA,	  PV5,		LCD,	    I2C4,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3118),\
	PINGROUP(UART2_RXD,	  PC3,		UART,	    IRDA,	SPDIF,	    UARTA,	SPI4,	     IRDA,	INPUT,	0x3164),\
	PINGROUP(UART2_TXD,	  PC2,		UART,	    IRDA,	SPDIF,	    UARTA,	SPI4,	     IRDA,	INPUT,	0x3168),\
	PINGROUP(UART2_RTS_N,	  PJ6,		UART,	    UARTA,	UARTB,	    RSVD2,	SPI4,	     RSVD2,	INPUT,	0x316c),\
	PINGROUP(UART2_CTS_N,	  PJ5,		UART,	    UARTA,	UARTB,	    RSVD2,	SPI4,	     RSVD2,	INPUT,	0x3170),\
	PINGROUP(UART3_TXD,	  PW6,		UART,	    UARTC,	RSVD1,	    RSVD2,	SPI4,	     RSVD1,	INPUT,	0x3174),\
	PINGROUP(UART3_RXD,	  PW7,		UART,	    UARTC,	RSVD1,	    RSVD2,	SPI4,	     RSVD1,	INPUT,	0x3178),\
	PINGROUP(UART3_CTS_N,	  PA1,		UART,	    UARTC,	SDMMC1,	    DTV,	SPI4,	     UARTC,	INPUT,	0x317c),\
	PINGROUP(UART3_RTS_N,	  PC0,          UART,	    UARTC,	PWM0,	    DTV,	DISPLAYA,    UARTC,	INPUT,	0x3180),\
	PINGROUP(GPIO_PU0,	  PU0,          UART,	    OWR,	UARTA,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3184),\
	PINGROUP(GPIO_PU1,	  PU1,          UART,	    RSVD0,	UARTA,	    RSVD2,	RSVD3,	     RSVD0,	INPUT,	0x3188),\
	PINGROUP(GPIO_PU2,	  PU2,          UART,	    RSVD0,	UARTA,	    RSVD2,	RSVD3,	     RSVD0,	INPUT,	0x318c),\
	PINGROUP(GPIO_PU3,	  PU3,          UART,	    PWM0,	UARTA,	    DISPLAYA,	DISPLAYB,       PWM0,	INPUT,	0x3190),\
	PINGROUP(GPIO_PU4,	  PU4,          UART,	    PWM1,	UARTA,	    DISPLAYA,	DISPLAYB,       PWM1,	INPUT,	0x3194),\
	PINGROUP(GPIO_PU5,	  PU5,          UART,	    PWM2,	UARTA,	    DISPLAYA,	DISPLAYB,       PWM2,	INPUT,	0x3198),\
	PINGROUP(GPIO_PU6,	  PU6,          UART,	    PWM3,	UARTA,	    USB,	DISPLAYB,       PWM3,	INPUT,	0x319c),\
	PINGROUP(GEN1_I2C_SDA,	  PC5,          UART,	    I2C1,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x31a0),\
	PINGROUP(GEN1_I2C_SCL,	  PC4,          UART,	    I2C1,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x31a4),\
	PINGROUP(DAP4_FS,	  PP4,          UART,	    I2S3,	RSVD1,	    DTV,	RSVD3,	     RSVD1,	INPUT,	0x31a8),\
	PINGROUP(DAP4_DIN,	  PP5,          UART,	    I2S3,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x31ac),\
	PINGROUP(DAP4_DOUT,	  PP6,          UART,	    I2S3,	RSVD1,	    DTV,	RSVD3,	     RSVD1,	INPUT,	0x31b0),\
	PINGROUP(DAP4_SCLK,	  PP7,          UART,	    I2S3,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x31b4),\
	PINGROUP(CLK3_OUT,	  PEE0,		UART,	    EXTPERIPH3,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x31b8),\
	PINGROUP(CLK3_REQ,	  PEE1,		UART,	    DEV3,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x31bc),\
	PINGROUP(GMI_WP_N,	  PC7,		GMI,	    RSVD0,	NAND,	    GMI,	GMI_ALT,     RSVD0,	INPUT,	0x31c0),\
	PINGROUP(GMI_IORDY,	  PI5,		GMI,	    SDMMC2,	RSVD1,	    GMI,	TRACE,	     RSVD1,	INPUT,	0x31c4),\
	PINGROUP(GMI_WAIT,	  PI7,		GMI,	    SPI4,	NAND,	    GMI,	DTV,	     NAND,	INPUT,	0x31c8),\
	PINGROUP(GMI_ADV_N,	  PK0,		GMI,	    RSVD0,	NAND,	    GMI,	TRACE,	     RSVD0,	INPUT,	0x31cc),\
	PINGROUP(GMI_CLK,	  PK1,		GMI,	    SDMMC2,	NAND,	    GMI,	TRACE,	     GMI,	INPUT,	0x31d0),\
	PINGROUP(GMI_CS0_N,	  PJ0,		GMI,	    RSVD0,	NAND,	    GMI,	USB,	     RSVD0,	INPUT,	0x31d4),\
	PINGROUP(GMI_CS1_N,	  PJ2,		GMI,	    RSVD0,	NAND,	    GMI,	SOC,	     RSVD0,	INPUT,	0x31d8),\
	PINGROUP(GMI_CS2_N,	  PK3,		GMI,	    SDMMC2,	NAND,	    GMI,	TRACE,	     GMI,	INPUT,	0x31dc),\
	PINGROUP(GMI_CS3_N,	  PK4,		GMI,	    SDMMC2,	NAND,	    GMI,	GMI_ALT,     GMI,	INPUT,	0x31e0),\
	PINGROUP(GMI_CS4_N,	  PK2,		GMI,	    USB,	NAND,	    GMI,	TRACE,	     GMI,	INPUT,	0x31e4),\
	PINGROUP(GMI_CS6_N,	  PI3,		GMI,	    NAND,	NAND_ALT,   GMI,	SPI4,	     NAND,	INPUT,	0x31e8),\
	PINGROUP(GMI_CS7_N,	  PI6,		GMI,	    NAND,	NAND_ALT,   GMI,	SDMMC2,	     NAND,	INPUT,	0x31ec),\
	PINGROUP(GMI_AD0,	  PG0,		GMI,	    RSVD0,	NAND,	    GMI,	RSVD3,	     RSVD0,	INPUT,	0x31f0),\
	PINGROUP(GMI_AD1,	  PG1,		GMI,	    RSVD0,	NAND,	    GMI,	RSVD3,	     RSVD0,	INPUT,	0x31f4),\
	PINGROUP(GMI_AD2,	  PG2,		GMI,	    RSVD0,	NAND,	    GMI,	RSVD3,	     RSVD0,	INPUT,	0x31f8),\
	PINGROUP(GMI_AD3,	  PG3,		GMI,	    RSVD0,	NAND,	    GMI,	RSVD3,	     RSVD0,	INPUT,	0x31fc),\
	PINGROUP(GMI_AD4,	  PG4,		GMI,	    RSVD0,	NAND,	    GMI,	RSVD3,	     RSVD0,	INPUT,	0x3200),\
	PINGROUP(GMI_AD5,	  PG5,		GMI,	    RSVD0,	NAND,	    GMI,	SPI4,	     RSVD0,	INPUT,	0x3204),\
	PINGROUP(GMI_AD6,	  PG6,		GMI,	    RSVD0,	NAND,	    GMI,	SPI4,	     RSVD0,	INPUT,	0x3208),\
	PINGROUP(GMI_AD7,	  PG7,		GMI,	    RSVD0,	NAND,	    GMI,	SPI4,	     RSVD0,	INPUT,	0x320c),\
	PINGROUP(GMI_AD8,	  PH0,		GMI,	    PWM0,	NAND,	    GMI,	DTV,	     GMI,	INPUT,	0x3210),\
	PINGROUP(GMI_AD9,	  PH1,		GMI,	    PWM1,	NAND,	    GMI,	CLDVFS,	     GMI,	INPUT,	0x3214),\
	PINGROUP(GMI_AD10,	  PH2,		GMI,	    PWM2,	NAND,	    GMI,	CLDVFS,	     GMI,	INPUT,	0x3218),\
	PINGROUP(GMI_AD11,	  PH3,		GMI,	    PWM3,	NAND,	    GMI,	USB,	     GMI,	INPUT,	0x321c),\
	PINGROUP(GMI_AD12,	  PH4,		GMI,	    SDMMC2,	NAND,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3220),\
	PINGROUP(GMI_AD13,	  PH5,		GMI,	    SDMMC2,	NAND,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3224),\
	PINGROUP(GMI_AD14,	  PH6,		GMI,	    SDMMC2,	NAND,	    GMI,	DTV,	     GMI,	INPUT,	0x3228),\
	PINGROUP(GMI_AD15,	  PH7,		GMI,	    SDMMC2,	NAND,	    GMI,	DTV,	     GMI,	INPUT,	0x322c),\
	PINGROUP(GMI_A16,	  PJ7,		GMI,	    UARTD,	TRACE,	    GMI,	GMI_ALT,     GMI,	INPUT,	0x3230),\
	PINGROUP(GMI_A17,	  PB0,		GMI,	    UARTD,	RSVD1,	    GMI,	TRACE,	     RSVD1,	INPUT,	0x3234),\
	PINGROUP(GMI_A18,	  PB1,		GMI,	    UARTD,	RSVD1,	    GMI,	TRACE,	     RSVD1,	INPUT,	0x3238),\
	PINGROUP(GMI_A19,	  PK7,		GMI,	    UARTD,	SPI4,	    GMI,	TRACE,	     GMI,	INPUT,	0x323c),\
	PINGROUP(GMI_WR_N,	  PI0,		GMI,	    RSVD0,	NAND,	    GMI,	SPI4,	     RSVD0,	INPUT,	0x3240),\
	PINGROUP(GMI_OE_N,	  PI1,		GMI,	    RSVD0,	NAND,	    GMI,	SOC,	     RSVD0,	INPUT,	0x3244),\
	PINGROUP(GMI_DQS_P,	  PJ3,		GMI,	    SDMMC2,	NAND,	    GMI,	TRACE,	     NAND,	INPUT,	0x3248),\
	PINGROUP(GMI_RST_N,	  PI4,		GMI,	    NAND,	NAND_ALT,   GMI,	RSVD3,	     RSVD3,	INPUT,	0x324c),\
	PINGROUP(GEN2_I2C_SCL,	  PT5,		GMI,	    I2C2,	RSVD1,      GMI,	RSVD3,	     RSVD1,	INPUT,	0x3250),\
	PINGROUP(GEN2_I2C_SDA,	  PT6,		GMI,	    I2C2,	RSVD1,      GMI,	RSVD3,	     RSVD1,	INPUT,	0x3254),\
	PINGROUP(SDMMC4_CLK,	  PCC4,		SDMMC4,     SDMMC4,	RSVD1,	    GMI,	RSVD3,	     RSVD1,	INPUT,	0x3258),\
	PINGROUP(SDMMC4_CMD,	  PT7,		SDMMC4,     SDMMC4,	RSVD1,	    GMI,	RSVD3,	     RSVD1,	INPUT,	0x325c),\
	PINGROUP(SDMMC4_DAT0,	  PAA0,		SDMMC4,     SDMMC4,	SPI3,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3260),\
	PINGROUP(SDMMC4_DAT1,	  PAA1,		SDMMC4,     SDMMC4,	SPI3,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3264),\
	PINGROUP(SDMMC4_DAT2,	  PAA2,		SDMMC4,     SDMMC4,	SPI3,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3268),\
	PINGROUP(SDMMC4_DAT3,	  PAA3,		SDMMC4,     SDMMC4,	SPI3,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x326c),\
	PINGROUP(SDMMC4_DAT4,	  PAA4,		SDMMC4,     SDMMC4,	SPI3,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3270),\
	PINGROUP(SDMMC4_DAT5,	  PAA5,		SDMMC4,     SDMMC4,	SPI3,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3274),\
	PINGROUP(SDMMC4_DAT6,	  PAA6,		SDMMC4,     SDMMC4,	SPI3,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3278),\
	PINGROUP(SDMMC4_DAT7,	  PAA7,		SDMMC4,     SDMMC4,	RSVD1,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x327c),\
	PINGROUP(CAM_MCLK,	  PCC0,		CAM,	    VI,		VI_ALT1,    VI_ALT3,	RSVD3,	     RSVD3,	INPUT,	0x3284),\
	PINGROUP(GPIO_PCC1,	  PCC1,		CAM,	    I2S4,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3288),\
	PINGROUP(GPIO_PBB0,	  PBB0,		CAM,	    I2S4,	VI,	    VI_ALT1,	VI_ALT3,     I2S4,	INPUT,	0x328c),\
	PINGROUP(CAM_I2C_SCL,	  PBB1,		CAM,	    VGP1,	I2C3,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3290),\
	PINGROUP(CAM_I2C_SDA,	  PBB2,		CAM,	    VGP2,	I2C3,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3294),\
	PINGROUP(GPIO_PBB3,	  PBB3,		CAM,	    VGP3,	DISPLAYA,      DISPLAYB,	RSVD3,	     RSVD3,	INPUT,	0x3298),\
	PINGROUP(GPIO_PBB4,	  PBB4,		CAM,	    VGP4,	DISPLAYA,      DISPLAYB,	RSVD3,	     RSVD3,	INPUT,	0x329c),\
	PINGROUP(GPIO_PBB5,	  PBB5,		CAM,	    VGP5,	DISPLAYA,      DISPLAYB,	RSVD3,	     RSVD3,	INPUT,	0x32a0),\
	PINGROUP(GPIO_PBB6,	  PBB6,		CAM,	    VGP6,	DISPLAYA,      DISPLAYB,	RSVD3,	     RSVD3,	INPUT,	0x32a4),\
	PINGROUP(GPIO_PBB7,	  PBB7,		CAM,	    I2S4,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x32a8),\
	PINGROUP(GPIO_PCC2,	  PCC2,		CAM,	    I2S4,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x32ac),\
	PINGROUP(JTAG_RTCK,	  INVALID,	SYS,	    RTCK,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x32b0),\
	PINGROUP(PWR_I2C_SCL,	  PZ6,		SYS,	    I2CPWR,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x32b4),\
	PINGROUP(PWR_I2C_SDA,	  PZ7,		SYS,	    I2CPWR,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x32b8),\
	PINGROUP(KB_ROW0,	  PR0,		SYS,	    KBC,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x32bc),\
	PINGROUP(KB_ROW1,	  PR1,		SYS,	    KBC,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x32c0),\
	PINGROUP(KB_ROW2,	  PR2,		SYS,	    KBC,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x32c4),\
	PINGROUP(KB_ROW3,	  PR3,		SYS,	    KBC,	DISPLAYA,      RSVD2,	DISPLAYB,       RSVD2,	INPUT,	0x32c8),\
	PINGROUP(KB_ROW4,	  PR4,		SYS,	    KBC,	DISPLAYA,      SPI2,	DISPLAYB,       KBC,	INPUT,	0x32cc),\
	PINGROUP(KB_ROW5,	  PR5,		SYS,	    KBC,	DISPLAYA,      SPI2,	DISPLAYB,       KBC,	INPUT,	0x32d0),\
	PINGROUP(KB_ROW6,	  PR6,		SYS,	    KBC,	DISPLAYA,      DISPLAYA_ALT,	DISPLAYB,       KBC,	INPUT,	0x32d4),\
	PINGROUP(KB_ROW7,	  PR7,		SYS,	    KBC,	RSVD1,	    CLDVFS,	UARTA,	     RSVD1,	INPUT,	0x32d8),\
	PINGROUP(KB_ROW8,	  PS0,		SYS,	    KBC,	RSVD1,	    CLDVFS,	UARTA,	     RSVD1,	INPUT,	0x32dc),\
	PINGROUP(KB_ROW9,	  PS1,		SYS,	    KBC,	RSVD1,	    RSVD2,	UARTA,	     RSVD1,	INPUT,	0x32e0),\
	PINGROUP(KB_ROW10,	  PS2,		SYS,	    KBC,	RSVD1,	    RSVD2,	UARTA,	     RSVD1,	INPUT,	0x32e4),\
	PINGROUP(KB_COL0,	  PQ0,	 	SYS,	    KBC,	USB,	    SPI2,	EMC_DLL,       KBC,	INPUT,	0x32fc),\
	PINGROUP(KB_COL1,	  PQ1,		SYS,	    KBC,	RSVD1,	    SPI2,	EMC_DLL,       RSVD1,	INPUT,	0x3300),\
	PINGROUP(KB_COL2,	  PQ2,		SYS,	    KBC,	RSVD1,	    SPI2,	RSVD3,	     RSVD1,	INPUT,	0x3304),\
	PINGROUP(KB_COL3,	  PQ3,		SYS,	    KBC,	DISPLAYA,      PWM2,	UARTA,	     KBC,	INPUT,	0x3308),\
	PINGROUP(KB_COL4,	  PQ4,		SYS,	    KBC,	OWR,	    SDMMC3,	UARTA,	     KBC,	INPUT,	0x330c),\
	PINGROUP(KB_COL5,	  PQ5,		SYS,	    KBC,	RSVD1,	    SDMMC1,	RSVD3,	     RSVD1,	INPUT,	0x3310),\
	PINGROUP(KB_COL6,	  PQ6,		SYS,	    KBC,	RSVD1,	    SPI2,	RSVD3,	     RSVD1,	INPUT,	0x3314),\
	PINGROUP(KB_COL7,	  PQ7,		SYS,	    KBC,	RSVD1,	    SPI2,	RSVD3,	     RSVD1,	INPUT,	0x3318),\
	PINGROUP(CLK_32K_OUT,	  PA0,		SYS,	    BLINK,	SOC,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x331c),\
	PINGROUP(SYS_CLK_REQ,	  PZ5,		SYS,	    SYSCLK,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3320),\
	PINGROUP(CORE_PWR_REQ,	  INVALID,	SYS,	    PWRON,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3324),\
	PINGROUP(CPU_PWR_REQ,	  INVALID,	SYS,	    CPU,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3328),\
	PINGROUP(PWR_INT_N,	  INVALID,	SYS,	    PMI,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x332c),\
	PINGROUP(CLK_32K_IN,	  INVALID,	SYS,	    CLK,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3330),\
	PINGROUP(OWR,		  INVALID,	SYS,	    OWR,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3334),\
	PINGROUP(DAP1_FS,	  PN0,		AUDIO,      I2S0,	HDA,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3338),\
	PINGROUP(DAP1_DIN,	  PN1,		AUDIO,      I2S0,	HDA,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x333c),\
	PINGROUP(DAP1_DOUT,	  PN2,		AUDIO,      I2S0,	HDA,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3340),\
	PINGROUP(DAP1_SCLK,	  PN3,		AUDIO,      I2S0,	HDA,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3344),\
	PINGROUP(CLK1_REQ,	  PEE2,		AUDIO,      DAP,	DAP1,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3348),\
	PINGROUP(CLK1_OUT,	  PW4,		AUDIO,      EXTPERIPH1,	DAP2,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x334c),\
	PINGROUP(SPDIF_IN,	  PK6,		AUDIO,      SPDIF,	USB,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3350),\
	PINGROUP(SPDIF_OUT,	  PK5,		AUDIO,      SPDIF,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3354),\
	PINGROUP(DAP2_FS,	  PA2,		AUDIO,      I2S1,	HDA,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3358),\
	PINGROUP(DAP2_DIN,	  PA4,		AUDIO,      I2S1,	HDA,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x335c),\
	PINGROUP(DAP2_DOUT,	  PA5,		AUDIO,      I2S1,	HDA,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3360),\
	PINGROUP(DAP2_SCLK,	  PA3,		AUDIO,      I2S1,	HDA,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3364),\
	PINGROUP(DVFS_PWM,	  PX0,		AUDIO,      SPI6,	CLDVFS,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3368),\
	PINGROUP(GPIO_X1_AUD,	  PX1,		AUDIO,      SPI6,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x336c),\
	PINGROUP(GPIO_X3_AUD,	  PX3,		AUDIO,      SPI6,	SPI1,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3370),\
	PINGROUP(DVFS_CLK,	  PX2,		AUDIO,      SPI6,	CLDVFS,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x3374),\
	PINGROUP(GPIO_X4_AUD,	  PX4,		AUDIO,      RSVD0,	SPI1,	    SPI2,	DAP2,	     RSVD0,	INPUT,	0x3378),\
	PINGROUP(GPIO_X5_AUD,	  PX5,		AUDIO,      RSVD0,	SPI1,	    SPI2,	RSVD3,	     RSVD0,	INPUT,	0x337c),\
	PINGROUP(GPIO_X6_AUD,	  PX6,		AUDIO,      SPI6,	SPI1,	    SPI2,	RSVD3,	     RSVD3,	INPUT,	0x3380),\
	PINGROUP(GPIO_X7_AUD,	  PX7,		AUDIO,      RSVD0,	SPI1,	    SPI2,	RSVD3,	     RSVD0,	INPUT,	0x3384),\
	PINGROUP(SDMMC3_CLK,	  PA6,		SDMMC3,     SDMMC3,	RSVD1,	    RSVD2,	SPI3,	     RSVD1,	INPUT,	0x3390),\
	PINGROUP(SDMMC3_CMD,	  PA7,		SDMMC3,     SDMMC3,	PWM3,	    UARTA,	SPI3,	     SDMMC3,	INPUT,	0x3394),\
	PINGROUP(SDMMC3_DAT0,	  PB7,		SDMMC3,     SDMMC3,	RSVD1,	    RSVD2,	SPI3,	     RSVD1,	INPUT,	0x3398),\
	PINGROUP(SDMMC3_DAT1,	  PB6,		SDMMC3,     SDMMC3,	PWM2,	    UARTA,	SPI3,	     SDMMC3,	INPUT,	0x339c),\
	PINGROUP(SDMMC3_DAT2,	  PB5,		SDMMC3,     SDMMC3,	PWM1,	    DISPLAYA,	SPI3,	     SDMMC3,	INPUT,	0x33a0),\
	PINGROUP(SDMMC3_DAT3,	  PB4,		SDMMC3,     SDMMC3,	PWM0,	    DISPLAYB,	SPI3,	     SDMMC3,	INPUT,	0x33a4),\
	PINGROUP(HDMI_CEC, 	  PEE3,		SYS,        CEC,	SDMMC3,	    RSVD2,	SOC,	     RSVD2,	INPUT,	0x33e0),\
	PINGROUP(SDMMC1_WP_N,	  PV3,		SDMMC1,     SDMMC1,	CLK12,	    SPI4,	UARTA,	     SDMMC1,	INPUT,	0x33e4),\
	PINGROUP(SDMMC3_CD_N,	  PV2,		SYS,        SDMMC3,	OWR,	    RSVD2,	RSVD3,	     RSVD2,	INPUT,	0x33e8),\
	PINGROUP(GPIO_W2_AUD,	  PW2,		AUDIO,      SPI6,	RSVD1,	    SPI2,	I2C1,	     RSVD1,	INPUT,	0x33ec),\
	PINGROUP(GPIO_W3_AUD,	  PW3,		AUDIO,      SPI6,	SPI1,	    SPI2,	I2C1,	     SPI6,	INPUT,	0x33f0),\
	PINGROUP(USB_VBUS_EN0,	  PN4,		LCD,        USB,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x33f4),\
	PINGROUP(USB_VBUS_EN1,	  PN5,		LCD,        USB,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x33f8),\
	PINGROUP(SDMMC3_CLK_LB_IN,	  PEE5,		SDMMC3,        SDMMC3,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x33fc),\
	PINGROUP(SDMMC3_CLK_LB_OUT,	  PEE4,		SDMMC3,        SDMMC3,	RSVD1,	    RSVD2,	RSVD3,	     RSVD1,	INPUT,	0x3400),\
	PINGROUP(NAND_GMI_CLK_LB,	  INVALID,		GMI,        SDMMC2,	NAND,	    GMI,	RSVD3,	     RSVD3,	INPUT,	0x3404),\
	PINGROUP(RESET_OUT_N,	  INVALID,	SYS,        RSVD0,	RSVD1,	    RSVD2,	RESET_OUT_N, RSVD0,	OUTPUT,	0x3408),

const struct tegra_pingroup_desc tegra_soc_pingroups[TEGRA_MAX_PINGROUP] = {
	PINGROUPS
};

#undef PINGROUP

#undef TEGRA_GPIO_INVALID
#define TEGRA_GPIO_INVALID	TEGRA_MAX_GPIO

#define PINGROUP(pg_name, gpio_nr, vdd, f0, f1, f2, f3, fs, iod, reg)	\
	[TEGRA_GPIO_##gpio_nr] =  TEGRA_PINGROUP_ ##pg_name\

const int gpio_to_pingroup[TEGRA_MAX_GPIO + 1] = {
	PINGROUPS

};

#define SET_DRIVE(_name, _hsm, _schmitt, _drive, _pulldn_drive, _pullup_drive, _pulldn_slew, _pullup_slew) \
	{							\
		.pingroup = TEGRA_DRIVE_PINGROUP_##_name,	\
		.hsm = TEGRA_HSM_##_hsm,			\
		.schmitt = TEGRA_SCHMITT_##_schmitt,		\
		.drive = TEGRA_DRIVE_##_drive,			\
		.pull_down = TEGRA_PULL_##_pulldn_drive,	\
		.pull_up = TEGRA_PULL_##_pullup_drive,		\
		.slew_rising = TEGRA_SLEW_##_pulldn_slew,	\
		.slew_falling = TEGRA_SLEW_##_pullup_slew,	\
	}

static __initdata struct tegra_drive_pingroup_config t11x_def_drive_pinmux[] = {
	SET_DRIVE(AT2, DISABLE, DISABLE, DIV_8, 48, 55, FASTEST, FASTEST),
};

#ifdef CONFIG_PM_SLEEP

static struct tegra_pingroup_config *sleep_pinmux;
static int sleep_pinmux_size;

void tegra11x_set_sleep_pinmux(struct tegra_pingroup_config *config, int size)
{
	sleep_pinmux = config;
	sleep_pinmux_size = size;
}

static u32 pinmux_reg[TEGRA_MAX_PINGROUP + ARRAY_SIZE(tegra_soc_drive_pingroups)];

static int tegra11x_pinmux_suspend(void)
{
	unsigned int i;
	u32 *ctx = pinmux_reg;

	for (i = 0; i < TEGRA_MAX_PINGROUP; i++)
		*ctx++ = pg_readl(tegra_soc_pingroups[i].mux_bank,
				tegra_soc_pingroups[i].mux_reg);

	for (i = 0; i < ARRAY_SIZE(tegra_soc_drive_pingroups); i++)
		*ctx++ = pg_readl(tegra_soc_drive_pingroups[i].reg_bank,
				tegra_soc_drive_pingroups[i].reg);

	/* change to sleep pinmux settings */
	if (sleep_pinmux)
		tegra_pinmux_config_table(sleep_pinmux, sleep_pinmux_size);
	return 0;
}

#define PMC_IO_DPD_REQ		0x1B8
#define PMC_IO_DPD2_REQ		0x1C0

static void tegra11x_pinmux_resume(void)
{
	unsigned int i;
	u32 *ctx = pinmux_reg;

	for (i = 0; i < TEGRA_MAX_PINGROUP; i++)
		pg_writel(*ctx++, tegra_soc_pingroups[i].mux_bank,
			tegra_soc_pingroups[i].mux_reg);

	for (i = 0; i < ARRAY_SIZE(tegra_soc_drive_pingroups); i++)
		pg_writel(*ctx++, tegra_soc_drive_pingroups[i].reg_bank,
			tegra_soc_drive_pingroups[i].reg);
}

static struct syscore_ops tegra_pinmux_syscore_ops = {
	.suspend = tegra11x_pinmux_suspend,
	.resume = tegra11x_pinmux_resume,
};
#endif

void __devinit tegra11x_pinmux_init(const struct tegra_pingroup_desc **pg,
		int *pg_max, const struct tegra_drive_pingroup_desc **pgdrive,
		int *pgdrive_max, const int **gpiomap, int *gpiomap_max)
{
	*pg = tegra_soc_pingroups;
	*pg_max = TEGRA_MAX_PINGROUP;
	*pgdrive = tegra_soc_drive_pingroups;
	*pgdrive_max = TEGRA_MAX_DRIVE_PINGROUP;
	*gpiomap = gpio_to_pingroup;
	*gpiomap_max = TEGRA_MAX_GPIO;

#ifdef CONFIG_PM_SLEEP
	register_syscore_ops(&tegra_pinmux_syscore_ops);
#endif
}

void tegra11x_default_pinmux(void)
{
	tegra_drive_pinmux_config_table(t11x_def_drive_pinmux,
					ARRAY_SIZE(t11x_def_drive_pinmux));
}
