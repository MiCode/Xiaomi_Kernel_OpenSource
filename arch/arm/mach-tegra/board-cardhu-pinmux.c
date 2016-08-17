/*
 * arch/arm/mach-tegra/board-cardhu-pinmux.c
 *
 * Copyright (C) 2011-2012, NVIDIA Corporation
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <mach/pinmux.h>
#include <mach/pinmux-tegra30.h>
#include <mach/gpio-tegra.h>
#include "board.h"
#include "board-cardhu.h"
#include "devices.h"
#include "gpio-names.h"

#define DEFAULT_DRIVE(_name)					\
	{							\
		.pingroup = TEGRA_DRIVE_PINGROUP_##_name,	\
		.hsm = TEGRA_HSM_DISABLE,			\
		.schmitt = TEGRA_SCHMITT_ENABLE,		\
		.drive = TEGRA_DRIVE_DIV_1,			\
		.pull_down = TEGRA_PULL_31,			\
		.pull_up = TEGRA_PULL_31,			\
		.slew_rising = TEGRA_SLEW_SLOWEST,		\
		.slew_falling = TEGRA_SLEW_SLOWEST,		\
	}
/* Setting the drive strength of pins
 * hsm: Enable High speed mode (ENABLE/DISABLE)
 * Schimit: Enable/disable schimit (ENABLE/DISABLE)
 * drive: low power mode (DIV_1, DIV_2, DIV_4, DIV_8)
 * pulldn_drive - drive down (falling edge) - Driver Output Pull-Down drive
 *                strength code. Value from 0 to 31.
 * pullup_drive - drive up (rising edge)  - Driver Output Pull-Up drive
 *                strength code. Value from 0 to 31.
 * pulldn_slew -  Driver Output Pull-Up slew control code  - 2bit code
 *                code 11 is least slewing of signal. code 00 is highest
 *                slewing of the signal.
 *                Value - FASTEST, FAST, SLOW, SLOWEST
 * pullup_slew -  Driver Output Pull-Down slew control code -
 *                code 11 is least slewing of signal. code 00 is highest
 *                slewing of the signal.
 *                Value - FASTEST, FAST, SLOW, SLOWEST
 */
#define SET_DRIVE(_name, _hsm, _schmitt, _drive, _pulldn_drive, _pullup_drive, _pulldn_slew, _pullup_slew) \
	{                                               \
		.pingroup = TEGRA_DRIVE_PINGROUP_##_name,   \
		.hsm = TEGRA_HSM_##_hsm,                    \
		.schmitt = TEGRA_SCHMITT_##_schmitt,        \
		.drive = TEGRA_DRIVE_##_drive,              \
		.pull_down = TEGRA_PULL_##_pulldn_drive,    \
		.pull_up = TEGRA_PULL_##_pullup_drive,		\
		.slew_rising = TEGRA_SLEW_##_pulldn_slew,   \
		.slew_falling = TEGRA_SLEW_##_pullup_slew,	\
	}

/* !!!FIXME!!!! POPULATE THIS TABLE */
static __initdata struct tegra_drive_pingroup_config cardhu_drive_pinmux[] = {
	/* DEFAULT_DRIVE(<pin_group>), */
	/* SET_DRIVE(ATA, DISABLE, DISABLE, DIV_1, 31, 31, FAST, FAST) */
	SET_DRIVE(DAP2, 	DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),
	SET_DRIVE(DAP1, 	DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

	/* All I2C pins are driven to maximum drive strength */
	/* GEN1 I2C */
	SET_DRIVE(DBG,		DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

	/* GEN2 I2C */
	SET_DRIVE(AT5,		DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

	/* CAM I2C */
	SET_DRIVE(GME,		DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

	/* DDC I2C */
	SET_DRIVE(DDC,		DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

	/* PWR_I2C */
	SET_DRIVE(AO1,		DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

	/* UART3 */
	SET_DRIVE(UART3,	DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

	/* SDMMC1 */
	SET_DRIVE(SDIO1,	DISABLE, DISABLE, DIV_1, 46, 42, FAST, FAST),

	/* SDMMC3 */
	SET_DRIVE(SDIO3,	DISABLE, DISABLE, DIV_1, 46, 42, FAST, FAST),

	/* SDMMC4 */
	SET_DRIVE(GMA,		DISABLE, DISABLE, DIV_1, 9, 9, SLOWEST, SLOWEST),
	SET_DRIVE(GMB,		DISABLE, DISABLE, DIV_1, 9, 9, SLOWEST, SLOWEST),
	SET_DRIVE(GMC,		DISABLE, DISABLE, DIV_1, 9, 9, SLOWEST, SLOWEST),
	SET_DRIVE(GMD,		DISABLE, DISABLE, DIV_1, 9, 9, SLOWEST, SLOWEST),

};

#define DEFAULT_PINMUX(_pingroup, _mux, _pupd, _tri, _io)	\
	{							\
		.pingroup	= TEGRA_PINGROUP_##_pingroup,	\
		.func		= TEGRA_MUX_##_mux,		\
		.pupd		= TEGRA_PUPD_##_pupd,		\
		.tristate	= TEGRA_TRI_##_tri,		\
		.io		= TEGRA_PIN_##_io,		\
		.lock		= TEGRA_PIN_LOCK_DEFAULT,	\
		.od		= TEGRA_PIN_OD_DEFAULT,		\
		.ioreset	= TEGRA_PIN_IO_RESET_DEFAULT,	\
	}

#define I2C_PINMUX(_pingroup, _mux, _pupd, _tri, _io, _lock, _od) \
	{							\
		.pingroup	= TEGRA_PINGROUP_##_pingroup,	\
		.func		= TEGRA_MUX_##_mux,		\
		.pupd		= TEGRA_PUPD_##_pupd,		\
		.tristate	= TEGRA_TRI_##_tri,		\
		.io		= TEGRA_PIN_##_io,		\
		.lock		= TEGRA_PIN_LOCK_##_lock,	\
		.od		= TEGRA_PIN_OD_##_od,		\
		.ioreset	= TEGRA_PIN_IO_RESET_DEFAULT,	\
	}

#define VI_PINMUX(_pingroup, _mux, _pupd, _tri, _io, _lock, _ioreset) \
	{							\
		.pingroup	= TEGRA_PINGROUP_##_pingroup,	\
		.func		= TEGRA_MUX_##_mux,		\
		.pupd		= TEGRA_PUPD_##_pupd,		\
		.tristate	= TEGRA_TRI_##_tri,		\
		.io		= TEGRA_PIN_##_io,		\
		.lock		= TEGRA_PIN_LOCK_##_lock,	\
		.od		= TEGRA_PIN_OD_DEFAULT,		\
		.ioreset	= TEGRA_PIN_IO_RESET_##_ioreset	\
	}
#define CEC_PINMUX(_pingroup, _mux, _pupd, _tri, _io, _lock, _od) \
	{                                                       \
		.pingroup       = TEGRA_PINGROUP_##_pingroup,   \
			.func           = TEGRA_MUX_##_mux,             \
			.pupd           = TEGRA_PUPD_##_pupd,           \
			.tristate       = TEGRA_TRI_##_tri,             \
			.io             = TEGRA_PIN_##_io,              \
			.lock           = TEGRA_PIN_LOCK_##_lock,       \
			.od             = TEGRA_PIN_OD_##_od,           \
			.ioreset        = TEGRA_PIN_IO_RESET_DEFAULT,   \
	}

static __initdata struct tegra_pingroup_config cardhu_pinmux_common[] = {
	/* SDMMC1 pinmux */
	DEFAULT_PINMUX(SDMMC1_CLK,      SDMMC1,          NORMAL,     NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_CMD,      SDMMC1,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT3,     SDMMC1,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT2,     SDMMC1,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT1,     SDMMC1,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT0,     SDMMC1,          PULL_UP,    NORMAL,     INPUT),

	/* SDMMC3 pinmux */
	DEFAULT_PINMUX(SDMMC3_CLK,      SDMMC3,          NORMAL,     NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_CMD,      SDMMC3,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT0,     SDMMC3,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT1,     SDMMC3,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT2,     SDMMC3,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT3,     SDMMC3,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT6,     SDMMC3,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT7,     SDMMC3,          PULL_UP,    NORMAL,     INPUT),

	/* SDMMC4 pinmux */
	DEFAULT_PINMUX(SDMMC4_CLK,      SDMMC4,          NORMAL,     NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_CMD,      SDMMC4,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT0,     SDMMC4,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT1,     SDMMC4,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT2,     SDMMC4,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT3,     SDMMC4,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT4,     SDMMC4,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT5,     SDMMC4,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT6,     SDMMC4,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT7,     SDMMC4,          PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_RST_N,    RSVD1,           PULL_DOWN,  NORMAL,     INPUT),

	/* I2C1 pinmux */
	I2C_PINMUX(GEN1_I2C_SCL,	I2C1,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),
	I2C_PINMUX(GEN1_I2C_SDA,	I2C1,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),

	/* I2C2 pinmux */
	I2C_PINMUX(GEN2_I2C_SCL,	I2C2,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),
	I2C_PINMUX(GEN2_I2C_SDA,	I2C2,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),

	/* I2C3 pinmux */
	I2C_PINMUX(CAM_I2C_SCL,		I2C3,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),
	I2C_PINMUX(CAM_I2C_SDA,		I2C3,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),

	/* I2C4 pinmux */
	I2C_PINMUX(DDC_SCL,		I2C4,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),
	I2C_PINMUX(DDC_SDA,		I2C4,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),

	/* Power I2C pinmux */
	I2C_PINMUX(PWR_I2C_SCL,		I2CPWR,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),
	I2C_PINMUX(PWR_I2C_SDA,		I2CPWR,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),

	/* HDMI-CEC  pinmux */
	CEC_PINMUX(HDMI_CEC,    CEC,    NORMAL,        NORMAL, INPUT,  DISABLE,        ENABLE),

	DEFAULT_PINMUX(ULPI_DATA0,      UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(ULPI_DATA1,      UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA2,      UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA3,      UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA4,      UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA5,      UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA6,      UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA7,      UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(ULPI_CLK,        UARTD,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(ULPI_DIR,        UARTD,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_NXT,        UARTD,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_STP,        UARTD,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(DAP3_FS,         I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_DIN,        I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_DOUT,       I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_SCLK,       I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PV2,        RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PV3,        RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(CLK2_OUT,        EXTPERIPH2,      NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK2_REQ,        DAP,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_PWR1,        DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_PWR2,        DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_SDIN,        DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_SDOUT,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_WR_N,        DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_DC0,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_PWR0,        DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_PCLK,        DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_DE,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_HSYNC,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_VSYNC,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D0,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D1,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D2,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D3,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D4,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D5,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D6,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D7,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D8,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D9,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D10,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D11,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D12,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D13,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D14,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D15,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D16,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D17,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D18,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D19,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D20,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D21,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D22,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D23,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_DC1,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CRT_HSYNC,       CRT,             NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(CRT_VSYNC,       CRT,             NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(VI_D0,           RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D1,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D2,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D3,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D4,           VI,              NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(VI_D5,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D7,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D10,          RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_MCLK,         VI,              PULL_UP,   NORMAL,     INPUT),

	DEFAULT_PINMUX(UART2_RXD,       IRDA,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART2_TXD,       IRDA,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART2_RTS_N,     UARTB,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART2_CTS_N,     UARTB,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART3_TXD,       UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART3_RXD,       UARTC,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART3_CTS_N,     UARTC,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART3_RTS_N,     UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU0,        RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU1,        RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU2,        RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU3,        RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU4,        RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU5,        PWM2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU6,        RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_FS,         I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DIN,        I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DOUT,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_SCLK,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK3_OUT,        EXTPERIPH3,      NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(CLK3_REQ,        DEV3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_WP_N,        GMI,             NORMAL,    NORMAL,     INPUT),

	DEFAULT_PINMUX(KB_ROW5,         OWR,             NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(KB_ROW12,        KBC,             NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(KB_ROW14,        KBC,             NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(KB_ROW15,        KBC,             NORMAL,    NORMAL,     OUTPUT),

#if 0 /* for testing on Verbier */
	DEFAULT_PINMUX(GMI_WAIT,        NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_ADV_N,       NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_CLK,         NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS0_N,       NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS1_N,       NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS3_N,       NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS4_N,       NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS6_N,       NAND_ALT,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS7_N,       NAND_ALT,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD0,         NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD1,         NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD2,         NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD3,         NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD4,         NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD5,         NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD6,         NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD7,         NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD8,         NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD9,         NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD10,        NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD11,        NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD12,        NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD13,        NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD14,        NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD15,        NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_WR_N,        NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_OE_N,        NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_DQS,         NAND,            NORMAL,    NORMAL,     INPUT),
#else
	DEFAULT_PINMUX(GMI_AD8,         PWM0,            NORMAL,    NORMAL,     OUTPUT), /* LCD1_BL_PWM */
#endif
	DEFAULT_PINMUX(GMI_A16,         SPI4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_A17,         SPI4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_A18,         SPI4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_A19,         SPI4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CAM_MCLK,        VI_ALT2,         PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PCC1,       RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB0,       RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB3,       VGP3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB5,       VGP5,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB6,       VGP6,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB7,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PCC2,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(JTAG_RTCK,       RTCK,            NORMAL,    NORMAL,     OUTPUT),

	/*  KBC keys */
	DEFAULT_PINMUX(KB_ROW0,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW1,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW2,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW3,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL0,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL1,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL2,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL3,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL4,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL5,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PV0,        RSVD,            PULL_UP,   NORMAL,     INPUT),

	DEFAULT_PINMUX(CLK_32K_OUT,     BLINK,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(SYS_CLK_REQ,     SYSCLK,          NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(OWR,             OWR,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_FS,         I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DIN,        I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DOUT,       I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_SCLK,       I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK1_REQ,        DAP,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK1_OUT,        EXTPERIPH1,      NORMAL,    NORMAL,     INPUT),
#if 0 /* For HDA realtek Codec */
	DEFAULT_PINMUX(SPDIF_IN,        DAP2,            PULL_DOWN, NORMAL,     INPUT),
#else
	DEFAULT_PINMUX(SPDIF_IN,        SPDIF,           NORMAL,    NORMAL,     INPUT),
#endif
	DEFAULT_PINMUX(SPDIF_OUT,       SPDIF,           NORMAL,    NORMAL,     OUTPUT),
#if 0 /* For HDA realtek Codec */
	DEFAULT_PINMUX(DAP2_FS,         HDA,             PULL_DOWN, NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DIN,        HDA,             PULL_DOWN, NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DOUT,       HDA,             PULL_DOWN, NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_SCLK,       HDA,             PULL_DOWN, NORMAL,     INPUT),
#else
	DEFAULT_PINMUX(DAP2_FS,         I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DIN,        I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DOUT,       I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_SCLK,       I2S1,            NORMAL,    NORMAL,     INPUT),
#endif
	DEFAULT_PINMUX(SPI2_CS1_N,      SPI2,            PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_MOSI,       SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_SCK,        SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_CS0_N,      SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_MISO,       SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L0_PRSNT_N,  PCIE,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L0_RST_N,    PCIE,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(PEX_L0_CLKREQ_N, PCIE,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_WAKE_N,      PCIE,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L1_PRSNT_N,  PCIE,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L1_RST_N,    PCIE,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(PEX_L1_CLKREQ_N, PCIE,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L2_PRSNT_N,  PCIE,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L2_RST_N,    PCIE,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(PEX_L2_CLKREQ_N, PCIE,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(HDMI_CEC,        CEC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(HDMI_INT,        RSVD0,           NORMAL,    TRISTATE,   INPUT),

	/* Gpios */
	/* SDMMC1 CD gpio */
	DEFAULT_PINMUX(GMI_IORDY,       RSVD1,           PULL_UP,   NORMAL,     INPUT),
	/* SDMMC1 WP gpio */
	DEFAULT_PINMUX(VI_D11,          RSVD1,           PULL_UP,   NORMAL,     INPUT),
	/* Touch panel GPIO */
	/* Touch IRQ */
	DEFAULT_PINMUX(GMI_AD12,        NAND,            PULL_UP,   NORMAL,     INPUT),

	/* Touch RESET */
	DEFAULT_PINMUX(GMI_AD14,        NAND,            NORMAL,    NORMAL,     OUTPUT),


	/* Power rails GPIO */
	DEFAULT_PINMUX(SPI2_SCK,        GMI,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB4,       VGP4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW8,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT5,     SDMMC3,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT4,     SDMMC3,          PULL_UP,   NORMAL,     INPUT),

	VI_PINMUX(VI_D6,           VI,              NORMAL,    NORMAL,     OUTPUT, DISABLE, DISABLE),
	VI_PINMUX(VI_D8,           SDMMC2,          NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_D9,           SDMMC2,          NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_PCLK,         RSVD1,           PULL_UP,   TRISTATE,   INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_HSYNC,        RSVD1,           NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_VSYNC,        RSVD1,           NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
};

static __initdata struct tegra_pingroup_config cardhu_pinmux_e118x[] = {
	/* Power rails GPIO */
	DEFAULT_PINMUX(SPI2_SCK,        SPI2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_RST_N,       RSVD3,           PULL_UP,   TRISTATE,   INPUT),
	DEFAULT_PINMUX(GMI_AD15,        NAND,            PULL_UP,   TRISTATE,   INPUT),
};

static __initdata struct tegra_pingroup_config cardhu_pinmux_pm311[] = {
	/* Power rails GPIO */
	DEFAULT_PINMUX(SPI2_SCK,        SPI2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L2_RST_N,    PCIE,            PULL_UP,   TRISTATE,   INPUT),
	DEFAULT_PINMUX(PEX_L2_CLKREQ_N, PCIE,            PULL_UP,   TRISTATE,   INPUT),
};

static __initdata struct tegra_pingroup_config cardhu_pinmux_cardhu[] = {
	DEFAULT_PINMUX(LCD_CS0_N,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_SCK,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_CS1_N,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_M1,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),

	DEFAULT_PINMUX(GMI_CS2_N,       RSVD1,           PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD8,         PWM0,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD10,        NAND,            NORMAL,    NORMAL,     OUTPUT),

	/* Power rails GPIO */
	DEFAULT_PINMUX(GMI_CS2_N,       NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_RST_N,       RSVD3,           PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD15,        NAND,            PULL_UP,   NORMAL,     INPUT),

	DEFAULT_PINMUX(GMI_CS0_N,       GMI,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_CS1_N,       GMI,             PULL_UP,   TRISTATE,   INPUT),
	/*TP_IRQ*/
	DEFAULT_PINMUX(GMI_CS4_N,       GMI,             PULL_UP,   NORMAL,     INPUT),
	/*PCIE dock detect*/
	DEFAULT_PINMUX(GPIO_PU4,        RSVD1,           PULL_UP,   NORMAL,     INPUT),
};

static __initdata struct tegra_pingroup_config cardhu_pinmux_cardhu_a03[] = {
	DEFAULT_PINMUX(LCD_CS0_N,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_SCK,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_CS1_N,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_M1,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),

	DEFAULT_PINMUX(GMI_CS2_N,       RSVD1,           PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD8,         PWM0,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD10,        NAND,            NORMAL,    NORMAL,     OUTPUT),

	/* Power rails GPIO */
	DEFAULT_PINMUX(PEX_L0_PRSNT_N,  PCIE,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L0_CLKREQ_N, PCIE,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L1_CLKREQ_N, RSVD3,           PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(PEX_L1_PRSNT_N,  RSVD3,           PULL_UP,   NORMAL,     INPUT),

	/*PCIE dock detect*/
	DEFAULT_PINMUX(GPIO_PU4,        RSVD1,           PULL_UP,   NORMAL,     INPUT),
};

static __initdata struct tegra_pingroup_config cardhu_pinmux_e1291_a04[] = {
	DEFAULT_PINMUX(GMI_AD15,        NAND,            PULL_DOWN,   NORMAL,   OUTPUT),
	DEFAULT_PINMUX(ULPI_DATA6,      UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(SPI2_MOSI,       SPI6,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_SCLK,       RSVD1,           NORMAL,    NORMAL,     OUTPUT),

	/*PCIE dock detect*/
	DEFAULT_PINMUX(GPIO_PU4,        RSVD1,           PULL_UP,   NORMAL,     INPUT),
};

static __initdata struct tegra_pingroup_config cardhu_pinmux_pm315[] = {
	DEFAULT_PINMUX(GMI_AD15,        NAND,            PULL_DOWN,   NORMAL,   OUTPUT),
	DEFAULT_PINMUX(ULPI_DATA6,      UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(SPI2_MOSI,       SPI6,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_SCLK,       RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	/* PCIE dock detect */
	DEFAULT_PINMUX(GPIO_PU4,        RSVD1,           PULL_UP,   NORMAL,     INPUT),
	/* CDC enable for realtek RTL5640 */
	DEFAULT_PINMUX(SPI2_SCK,        SPI2,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(SPI2_CS1_N,      SPI2,            NORMAL,    NORMAL,     INPUT),
	/* Power up for USB1, USB3 */
	DEFAULT_PINMUX(GMI_AD13,    	NAND,            PULL_UP,    NORMAL,    INPUT),
};

static __initdata struct tegra_pingroup_config cardhu_pinmux_e1198[] = {
	DEFAULT_PINMUX(LCD_CS0_N,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_SCK,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_CS1_N,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_M1,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),

	DEFAULT_PINMUX(GMI_CS2_N,       RSVD1,           PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD8,         PWM0,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD10,        NAND,            NORMAL,    NORMAL,     OUTPUT),

	/* SPI2 */
	DEFAULT_PINMUX(SPI2_SCK,        SPI2,            PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI2_MOSI,       SPI2,            PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI2_MISO,       SPI2,            PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI2_CS0_N,      SPI2,            PULL_UP,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI2_CS2_N,      SPI2,            PULL_UP,    NORMAL,     INPUT),
};

static __initdata struct tegra_pingroup_config cardhu_pinmux_pm269_e1506[] = {
	DEFAULT_PINMUX(LCD_M1,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_DC1,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
};

static __initdata struct tegra_pingroup_config unused_pins_lowpower[] = {
	DEFAULT_PINMUX(GMI_WAIT,        NAND,           PULL_UP,    TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_ADV_N,       NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_CLK,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS3_N,       NAND,           NORMAL,     NORMAL,       OUTPUT),
	DEFAULT_PINMUX(GMI_CS6_N,       SATA,           NORMAL,     NORMAL,       OUTPUT),
	DEFAULT_PINMUX(GMI_CS7_N,       NAND,           PULL_UP,    NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_AD0,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD1,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD2,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD3,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD4,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD5,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD6,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD7,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD11,        NAND,           NORMAL,     NORMAL,       OUTPUT),
	DEFAULT_PINMUX(GMI_AD13,        NAND,           PULL_UP,    NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_WR_N,        NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_OE_N,        NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_DQS,         NAND,           NORMAL,     TRISTATE,     OUTPUT),
};

static __initdata struct tegra_pingroup_config unused_pins_lowpower_e1506[] = {
	DEFAULT_PINMUX(LCD_D0,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D1,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D2,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D3,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D4,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D5,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D6,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D7,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D8,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D9,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D10,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D11,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D12,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D13,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D14,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D15,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D16,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D17,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D18,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D19,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D20,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D21,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D22,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D23,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),

	DEFAULT_PINMUX(LCD_DC0,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_PWR0,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_PWR1,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_PWR2,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_CS0_N,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_CS1_N,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),

	DEFAULT_PINMUX(LCD_PCLK,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_WR_N,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_HSYNC,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_VSYNC,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_SCK,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_SDOUT,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_SDIN,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
};

static __initdata struct tegra_pingroup_config gmi_pins_269[] = {
	/* Continuation of table unused_pins_lowpower only for PM269 */
	DEFAULT_PINMUX(GMI_CS0_N,       NAND,           PULL_UP,    NORMAL,       OUTPUT),
	DEFAULT_PINMUX(GMI_CS1_N,       NAND,           PULL_UP,    TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS2_N,       RSVD1,          NORMAL,     NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_CS3_N,       NAND,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS4_N,       NAND,           PULL_UP,    NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_CS6_N,       SATA,           NORMAL,     TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_CS7_N,       NAND,           PULL_UP,    NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_AD8,         PWM0,           NORMAL,     NORMAL,       OUTPUT),
	DEFAULT_PINMUX(GMI_AD10,        NAND,           NORMAL,     NORMAL,       OUTPUT),
	DEFAULT_PINMUX(GMI_AD11,        NAND,           NORMAL,     NORMAL,       OUTPUT),
	DEFAULT_PINMUX(GMI_AD13,        NAND,           PULL_UP,    TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD15,        NAND,           PULL_UP,    TRISTATE,     INPUT),
	DEFAULT_PINMUX(GMI_A16,         SPI4,           NORMAL,     NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_A17,         SPI4,           NORMAL,     NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_A18,         SPI4,           PULL_UP,    NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_A19,         SPI4,           NORMAL,     NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_RST_N,       NAND,           PULL_UP,    NORMAL,       INPUT),
	DEFAULT_PINMUX(GMI_WP_N,        NAND,           NORMAL,     NORMAL,       INPUT),
};

static void __init cardhu_wm8903_audio_init(void)
{
	int ret = gpio_request(TEGRA_GPIO_CDC_IRQ, "wm8903");
	if (ret < 0) {
		pr_err("%s() Error in gpio_request() for gpio %d\n",
					__func__, ret);
	}
	ret = gpio_direction_input(TEGRA_GPIO_CDC_IRQ);
	if (ret < 0) {
		pr_err("%s() Error in setting gpio %d to in/out\n",
					__func__, ret);
		gpio_free(TEGRA_GPIO_CDC_IRQ);
	}
}

static void __init beaver_rt5640_audio_init(void)
{
	int ret = gpio_request(TEGRA_GPIO_RTL_CDC_IRQ, "rt5640");
	if (ret < 0)
		pr_err("%s() Error in gpio_request() for gpio %d\n",
					__func__, ret);
	ret = gpio_direction_input(TEGRA_GPIO_RTL_CDC_IRQ);
	if (ret < 0) {
		pr_err("%s() Error in setting gpio %d to in/out\n",
					__func__, ret);
		gpio_free(TEGRA_GPIO_RTL_CDC_IRQ);
	}

}

#define GPIO_INIT_PIN_MODE(_gpio, _is_input, _value)	\
	{					\
		.gpio_nr	= _gpio,	\
		.is_input	= _is_input,	\
		.value		= _value,	\
	}


/* E1198-A01/E1291 specific  fab < A03 */
static struct gpio_init_pin_info init_gpio_mode_e1291_a02[] = {
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH7, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PI4, false, 0),
};

/* E1198-A02/E1291 specific  fab = A03 */
static struct gpio_init_pin_info init_gpio_mode_e1291_a03[] = {
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PDD6, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PDD4, false, 0),
};

/* E1198-A02/E1291 specific  fab >= A04 */
static struct gpio_init_pin_info init_gpio_mode_e1291_a04[] = {
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PDD6, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PDD4, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PR2, false, 0),
};

static struct gpio_init_pin_info init_gpio_mode_pm315[] = {
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PDD6, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PDD4, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH5, false, 1),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PR2, false, 0),
};

static void __init cardhu_gpio_init_configure(void)
{
	struct board_info board_info;
	int len;
	int i;
	struct gpio_init_pin_info *pins_info;

	tegra_get_board_info(&board_info);

	switch (board_info.board_id) {
	case BOARD_E1198:
		if (board_info.fab < BOARD_FAB_A02) {
			len = ARRAY_SIZE(init_gpio_mode_e1291_a02);
			pins_info = init_gpio_mode_e1291_a02;
		} else {
			len = ARRAY_SIZE(init_gpio_mode_e1291_a03);
			pins_info = init_gpio_mode_e1291_a03;
		}
		break;
	case BOARD_E1291:
		if (board_info.fab < BOARD_FAB_A03) {
			len = ARRAY_SIZE(init_gpio_mode_e1291_a02);
			pins_info = init_gpio_mode_e1291_a02;
		} else if (board_info.fab == BOARD_FAB_A03) {
			len = ARRAY_SIZE(init_gpio_mode_e1291_a03);
			pins_info = init_gpio_mode_e1291_a03;
		} else {
			len = ARRAY_SIZE(init_gpio_mode_e1291_a04);
			pins_info = init_gpio_mode_e1291_a04;
		}
		break;
	case BOARD_PM315:
		len = ARRAY_SIZE(init_gpio_mode_pm315);
		pins_info = init_gpio_mode_pm315;
		break;
	default:
		return;
	}

	for (i = 0; i < len; ++i) {
		tegra_gpio_init_configure(pins_info->gpio_nr,
			pins_info->is_input, pins_info->value);
		pins_info++;
	}
}

int __init cardhu_gpio_init(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);
	cardhu_gpio_init_configure();
	if (board_info.board_id == BOARD_PM315)
		beaver_rt5640_audio_init();
	else
		cardhu_wm8903_audio_init();

	return 0;
}

int __init cardhu_pinmux_init(void)
{
	struct board_info board_info;
	struct board_info display_board_info;

	tegra30_default_pinmux();

	tegra_pinmux_config_table(cardhu_pinmux_common, ARRAY_SIZE(cardhu_pinmux_common));
	tegra_drive_pinmux_config_table(cardhu_drive_pinmux,
					ARRAY_SIZE(cardhu_drive_pinmux));

	tegra_get_board_info(&board_info);
	tegra_get_display_board_info(&display_board_info);
	switch (board_info.board_id) {
	case BOARD_E1198:
		tegra_pinmux_config_table(cardhu_pinmux_e1198,
					ARRAY_SIZE(cardhu_pinmux_e1198));
		tegra_pinmux_config_table(unused_pins_lowpower,
					ARRAY_SIZE(unused_pins_lowpower));
		if (board_info.fab >= BOARD_FAB_A02)
			tegra_pinmux_config_table(cardhu_pinmux_cardhu_a03,
					ARRAY_SIZE(cardhu_pinmux_cardhu_a03));
		break;
	case BOARD_E1291:
		if (board_info.fab < BOARD_FAB_A03) {
			tegra_pinmux_config_table(cardhu_pinmux_cardhu,
					ARRAY_SIZE(cardhu_pinmux_cardhu));
			tegra_pinmux_config_table(unused_pins_lowpower,
					ARRAY_SIZE(unused_pins_lowpower));
		} else {
			tegra_pinmux_config_table(cardhu_pinmux_cardhu_a03,
					ARRAY_SIZE(cardhu_pinmux_cardhu_a03));
		}
		if (board_info.fab >= BOARD_FAB_A04)
			tegra_pinmux_config_table(cardhu_pinmux_e1291_a04,
					ARRAY_SIZE(cardhu_pinmux_e1291_a04));
		break;
	case BOARD_PM315:
		tegra_pinmux_config_table(cardhu_pinmux_cardhu_a03,
				ARRAY_SIZE(cardhu_pinmux_cardhu_a03));
		tegra_pinmux_config_table(cardhu_pinmux_pm315,
				ARRAY_SIZE(cardhu_pinmux_pm315));
		break;
	case BOARD_PM269:
	case BOARD_PM305:
	case BOARD_PM311:
	case BOARD_E1257:
		if (board_info.board_id == BOARD_PM311 || board_info.board_id == BOARD_PM305) {
			tegra_pinmux_config_table(cardhu_pinmux_pm311,
					ARRAY_SIZE(cardhu_pinmux_pm311));
		} else {
			tegra_pinmux_config_table(cardhu_pinmux_e118x,
					ARRAY_SIZE(cardhu_pinmux_e118x));
		}

		if (display_board_info.board_id == BOARD_DISPLAY_E1506) {
			tegra_pinmux_config_table(cardhu_pinmux_pm269_e1506,
					ARRAY_SIZE(cardhu_pinmux_pm269_e1506));
			tegra_pinmux_config_table(unused_pins_lowpower_e1506,
					ARRAY_SIZE(unused_pins_lowpower_e1506));
		}

		tegra_pinmux_config_table(unused_pins_lowpower,
					ARRAY_SIZE(unused_pins_lowpower));
		tegra_pinmux_config_table(gmi_pins_269,
					ARRAY_SIZE(gmi_pins_269));
		break;
	default:
		tegra_pinmux_config_table(cardhu_pinmux_e118x,
					ARRAY_SIZE(cardhu_pinmux_e118x));
		break;
	}

	return 0;
}

#define PIN_GPIO_LPM(_name, _gpio, _is_input, _value)	\
	{					\
		.name		= _name,	\
		.gpio_nr	= _gpio,	\
		.is_gpio	= true,		\
		.is_input	= _is_input,	\
		.value		= _value,	\
	}

struct gpio_init_pin_info pin_lpm_cardhu_common[] = {
	PIN_GPIO_LPM("GMI_CS3_N", TEGRA_GPIO_PK4, 0, 0),
	PIN_GPIO_LPM("GMI_CS4_N", TEGRA_GPIO_PK2, 1, 0),
	PIN_GPIO_LPM("GMI_CS7",   TEGRA_GPIO_PI6, 1, 0),
	PIN_GPIO_LPM("GMI_CS0",   TEGRA_GPIO_PJ0, 1, 0),
	PIN_GPIO_LPM("GMI_CS1",   TEGRA_GPIO_PJ2, 1, 0),
	PIN_GPIO_LPM("GMI_WP_N",  TEGRA_GPIO_PC7, 1, 0),
};

/* E1198 without PM313 display board */
struct gpio_init_pin_info pin_lpm_cardhu_common_wo_pm313[] = {
	PIN_GPIO_LPM("GMI_AD11",  TEGRA_GPIO_PH3, 0, 0),
};

struct gpio_init_pin_info vddio_gmi_pins_pm269[] = {
	PIN_GPIO_LPM("GMI_CS3_N", TEGRA_GPIO_PK4, 0, 0),
	PIN_GPIO_LPM("GMI_CS4_N", TEGRA_GPIO_PK2, 1, 0),
	PIN_GPIO_LPM("GMI_CS7",   TEGRA_GPIO_PI6, 1, 0),
	PIN_GPIO_LPM("GMI_CS0",   TEGRA_GPIO_PJ0, 1, 0),
	PIN_GPIO_LPM("GMI_CS1",   TEGRA_GPIO_PJ2, 1, 0),
	PIN_GPIO_LPM("GMI_WP_N",  TEGRA_GPIO_PC7, 1, 0),
	PIN_GPIO_LPM("GMI_A16",   TEGRA_GPIO_PJ7, 0, 0),
	PIN_GPIO_LPM("GMI_A17",   TEGRA_GPIO_PB0, 0, 0),
	PIN_GPIO_LPM("GMI_A18",   TEGRA_GPIO_PB1, 1, 0),
	PIN_GPIO_LPM("GMI_A19",   TEGRA_GPIO_PK7, 0, 0),
};

/* PM269 without PM313 display board */
struct gpio_init_pin_info vddio_gmi_pins_pm269_wo_pm313[] = {
	PIN_GPIO_LPM("GMI_CS2",   TEGRA_GPIO_PK3, 1, 0),
};

struct gpio_init_pin_info vddio_gmi_pins_pm269_e1506[] = {
	PIN_GPIO_LPM("GMI_CS2",   TEGRA_GPIO_PK3, 1, 0),
};

static struct gpio_init_pin_info cardhu_unused_gpio_pins_e1506[] = {
	PIN_GPIO_LPM("LCD_D0",     TEGRA_GPIO_PE0,  0, 0),
	PIN_GPIO_LPM("LCD_D1",     TEGRA_GPIO_PE1,  0, 0),
	PIN_GPIO_LPM("LCD_D2",     TEGRA_GPIO_PE2,  0, 0),
	PIN_GPIO_LPM("LCD_D3",     TEGRA_GPIO_PE3,  0, 0),
	PIN_GPIO_LPM("LCD_D4",     TEGRA_GPIO_PE4,  0, 0),
	PIN_GPIO_LPM("LCD_D5",     TEGRA_GPIO_PE5,  0, 0),
	PIN_GPIO_LPM("LCD_D6",     TEGRA_GPIO_PE6,  0, 0),
	PIN_GPIO_LPM("LCD_D7",     TEGRA_GPIO_PE7,  0, 0),
	PIN_GPIO_LPM("LCD_D8",     TEGRA_GPIO_PF0,  0, 0),
	PIN_GPIO_LPM("LCD_D9",     TEGRA_GPIO_PF1,  0, 0),
	PIN_GPIO_LPM("LCD_D10",    TEGRA_GPIO_PF2,  0, 0),
	PIN_GPIO_LPM("LCD_D11",    TEGRA_GPIO_PF3,  0, 0),
	PIN_GPIO_LPM("LCD_D12",    TEGRA_GPIO_PF4,  0, 0),
	PIN_GPIO_LPM("LCD_D13",    TEGRA_GPIO_PF5,  0, 0),
	PIN_GPIO_LPM("LCD_D14",    TEGRA_GPIO_PF6,  0, 0),
	PIN_GPIO_LPM("LCD_D15",    TEGRA_GPIO_PF7,  0, 0),
	PIN_GPIO_LPM("LCD_D16",    TEGRA_GPIO_PM0,  0, 0),
	PIN_GPIO_LPM("LCD_D17",    TEGRA_GPIO_PM1,  0, 0),
	PIN_GPIO_LPM("LCD_D18",    TEGRA_GPIO_PM2,  0, 0),
	PIN_GPIO_LPM("LCD_D19",    TEGRA_GPIO_PM3,  0, 0),
	PIN_GPIO_LPM("LCD_D20",    TEGRA_GPIO_PM4,  0, 0),
	PIN_GPIO_LPM("LCD_D21",    TEGRA_GPIO_PM5,  0, 0),
	PIN_GPIO_LPM("LCD_D22",    TEGRA_GPIO_PM6,  0, 0),
	PIN_GPIO_LPM("LCD_D23",    TEGRA_GPIO_PM7,  0, 0),

	PIN_GPIO_LPM("LCD_DC0",     TEGRA_GPIO_PN6,  0, 0),
	PIN_GPIO_LPM("LCD_PWR0",    TEGRA_GPIO_PB2,  0, 0),
	PIN_GPIO_LPM("LCD_PWR1",    TEGRA_GPIO_PC1,  0, 0),
	PIN_GPIO_LPM("LCD_PWR2",    TEGRA_GPIO_PC6,  0, 0),
	PIN_GPIO_LPM("LCD_CS0_N",   TEGRA_GPIO_PN4,  0, 0),
	PIN_GPIO_LPM("LCD_CS1_N",   TEGRA_GPIO_PW0,  0, 0),
	PIN_GPIO_LPM("LCD_PCLK",    TEGRA_GPIO_PB3,  0, 0),
	PIN_GPIO_LPM("LCD_WR_N",    TEGRA_GPIO_PZ3,  0, 0),
	PIN_GPIO_LPM("LCD_HSYNC",   TEGRA_GPIO_PJ3,  0, 0),
	PIN_GPIO_LPM("LCD_VSYNC",   TEGRA_GPIO_PJ4,  0, 0),
	PIN_GPIO_LPM("LCD_SCK",     TEGRA_GPIO_PZ4,  0, 0),
	PIN_GPIO_LPM("LCD_SDOUT",   TEGRA_GPIO_PN5,  0, 0),
	PIN_GPIO_LPM("LCD_SDIN",    TEGRA_GPIO_PZ2,  0, 0),
};

static void set_unused_pin_gpio(struct gpio_init_pin_info *lpm_pin_info,
		int list_count)
{
	int i;
	struct gpio_init_pin_info *pin_info;
	int ret;

	for (i = 0; i < list_count; ++i) {
		pin_info = (struct gpio_init_pin_info *)(lpm_pin_info + i);
		if (!pin_info->is_gpio)
			continue;

		ret = gpio_request(pin_info->gpio_nr, pin_info->name);
		if (ret < 0) {
			pr_err("%s() Error in gpio_request() for gpio %d\n",
					__func__, pin_info->gpio_nr);
			continue;
		}
		if (pin_info->is_input)
			ret = gpio_direction_input(pin_info->gpio_nr);
		else
			ret = gpio_direction_output(pin_info->gpio_nr,
							pin_info->value);
		if (ret < 0) {
			pr_err("%s() Error in setting gpio %d to in/out\n",
				__func__, pin_info->gpio_nr);
			gpio_free(pin_info->gpio_nr);
			continue;
		}
	}
}

/* Initialize the pins to desired state as per power/asic/system-eng
 * recomendation */
int __init cardhu_pins_state_init(void)
{
	struct board_info board_info;
	struct board_info display_board_info;

	tegra_get_board_info(&board_info);
	tegra_get_display_board_info(&display_board_info);
	if ((board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_E1198)) {
			set_unused_pin_gpio(&pin_lpm_cardhu_common[0],
					ARRAY_SIZE(pin_lpm_cardhu_common));

			if (display_board_info.board_id != BOARD_DISPLAY_PM313) {
				set_unused_pin_gpio(&pin_lpm_cardhu_common_wo_pm313[0],
						ARRAY_SIZE(pin_lpm_cardhu_common_wo_pm313));
			}
	}

	if ((board_info.board_id == BOARD_PM269) ||
		(board_info.board_id == BOARD_E1257) ||
		(board_info.board_id == BOARD_PM305) ||
		(board_info.board_id == BOARD_PM311)) {
			set_unused_pin_gpio(&vddio_gmi_pins_pm269[0],
				ARRAY_SIZE(vddio_gmi_pins_pm269));

			if (display_board_info.board_id == BOARD_DISPLAY_E1506) {
				set_unused_pin_gpio(&vddio_gmi_pins_pm269_e1506[0],
						ARRAY_SIZE(vddio_gmi_pins_pm269_e1506));
				set_unused_pin_gpio(cardhu_unused_gpio_pins_e1506,
						ARRAY_SIZE(cardhu_unused_gpio_pins_e1506));
			} else if (display_board_info.board_id != BOARD_DISPLAY_PM313) {
				set_unused_pin_gpio(&vddio_gmi_pins_pm269_wo_pm313[0],
						ARRAY_SIZE(vddio_gmi_pins_pm269_wo_pm313));
			}
	}

	return 0;
}
