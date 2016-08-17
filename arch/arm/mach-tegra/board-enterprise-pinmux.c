/*
 * arch/arm/mach-tegra/board-enterprise-pinmux.c
 *
 * Copyright (C) 2011-2012, NVIDIA CORPORATION. All rights reserved.
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
#include "board-enterprise.h"
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
static __initdata struct tegra_drive_pingroup_config enterprise_drive_pinmux[] = {
	/* DEFAULT_DRIVE(<pin_group>), */
	/* SET_DRIVE(ATA, DISABLE, DISABLE, DIV_1, 31, 31, FAST, FAST) */

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

#define CEC_PINMUX(_pingroup, _mux, _pupd, _tri, _io, _lock, _od) \
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

static __initdata struct tegra_pingroup_config enterprise_pinmux_common[] = {
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

	/* SDMMC4 pinmux */
	DEFAULT_PINMUX(SDMMC4_CLK,      SDMMC4,          NORMAL,     NORMAL,     INPUT),
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
	I2C_PINMUX(DDC_SCL,		I2C4,		PULL_UP,NORMAL,	INPUT,	DISABLE,	DISABLE),
	I2C_PINMUX(DDC_SDA,		I2C4,		PULL_UP,NORMAL,	INPUT,	DISABLE,	DISABLE),

	/* Power I2C pinmux */
	I2C_PINMUX(PWR_I2C_SCL,		I2CPWR,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),
	I2C_PINMUX(PWR_I2C_SDA,		I2CPWR,		NORMAL,	NORMAL,	INPUT,	DISABLE,	ENABLE),

	DEFAULT_PINMUX(ULPI_DATA0,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA1,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA2,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA3,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA4,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA5,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA6,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA7,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PV2,        RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(VI_D0,           RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D1,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D2,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D3,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D4,           VI,              NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(VI_D5,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D7,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D10,          RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_MCLK,         VI,              PULL_UP,   NORMAL,     INPUT),

	DEFAULT_PINMUX(GMI_AD8,         PWM0,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD9,         NAND,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_AD10,        NAND,            NORMAL,    NORMAL,     OUTPUT),
#if IS_EXTERNAL_PWM
	DEFAULT_PINMUX(GMI_AD11,        PWM3,            NORMAL,    NORMAL,	OUTPUT),
#endif
	DEFAULT_PINMUX(JTAG_RTCK,       RTCK,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(KB_ROW0,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW1,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW2,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW3,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL0,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL1,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL2,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL4,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL5,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK_32K_OUT,     BLINK,           PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(SYS_CLK_REQ,     SYSCLK,          NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(OWR,             OWR,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK1_REQ,        DAP,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI2_CS1_N,      SPI2,            PULL_UP,   NORMAL,     INPUT),
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
	CEC_PINMUX(HDMI_CEC,            CEC,             NORMAL,    TRISTATE,   OUTPUT, DEFAULT, DISABLE),
	DEFAULT_PINMUX(HDMI_INT,        RSVD0,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(VI_D11,          RSVD1,           PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD15,        NAND,            PULL_UP,   TRISTATE,   INPUT),
	VI_PINMUX(VI_D6,           VI,              NORMAL,    NORMAL,     OUTPUT, DISABLE, DISABLE),
	VI_PINMUX(VI_D8,           SDMMC2,          NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_D9,           SDMMC2,          NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_PCLK,         RSVD1,           PULL_UP,   TRISTATE,   INPUT,  DISABLE, ENABLE),
	VI_PINMUX(VI_HSYNC,        RSVD1,           NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_VSYNC,        RSVD1,           NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
};

static __initdata struct tegra_pingroup_config enterprise_pinmux_a03[] = {
	DEFAULT_PINMUX(LCD_PWR0,        DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D10,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(SDMMC4_CMD,      SDMMC4,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_CLK,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DIR,        ULPI,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(ULPI_NXT,        ULPI,            NORMAL,    TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(ULPI_STP,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_FS,         I2S2,            NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP3_DIN,        I2S2,            NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP3_DOUT,       I2S2,            NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP3_SCLK,       I2S2,            NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(GPIO_PV3,        RSVD1,           PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(LCD_PWR1,        DISPLAYA,        NORMAL,    TRISTATE,     OUTPUT),
	DEFAULT_PINMUX(LCD_PWR2,        DISPLAYA,        NORMAL,    TRISTATE,     INPUT),
	DEFAULT_PINMUX(LCD_CS0_N,       DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_DC0,         DISPLAYA,        PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(LCD_DE,          DISPLAYA,        NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(LCD_D0,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D1,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D2,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D3,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D4,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D5,          DISPLAYA,        PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(LCD_D6,          RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D7,          RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D8,          DISPLAYA,        NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(LCD_D9,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D11,         DISPLAYA,        PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(LCD_D12,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D13,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D14,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D15,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D16,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D17,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D18,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D19,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D20,         DISPLAYA,        PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(LCD_D21,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D22,         RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D23,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_CS1_N,       DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_M1,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_DC1,         DISPLAYA,        PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(UART2_RXD,       IRDA,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART2_TXD,       IRDA,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART2_RTS_N,     UARTB,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART2_CTS_N,     UARTB,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART3_TXD,       UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART3_RXD,       UARTC,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART3_CTS_N,     UARTC,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART3_RTS_N,     UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU0,        UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU1,        UARTA,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GPIO_PU2,        UARTA,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GPIO_PU3,        UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU4,        PWM1,            PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(GPIO_PU5,        PWM2,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GPIO_PU6,        PWM3,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(DAP4_FS,         I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DIN,        I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DOUT,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_SCLK,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_A16,         UARTD,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_A17,         UARTD,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GMI_A18,         UARTD,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GMI_A19,         UARTD,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(CAM_MCLK,        VI_ALT2,         NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PCC1,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB0,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB3,       VGP3,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB7,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PCC2,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW10,        KBC,             NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(KB_ROW12,        KBC,             NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(KB_COL3,         KBC,             PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(GPIO_PV0,        RSVD,            PULL_UP,   TRISTATE,   INPUT),
	DEFAULT_PINMUX(DAP1_FS,         I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DIN,        I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DOUT,       I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_SCLK,       I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK1_OUT,        EXTPERIPH1,      NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(SPDIF_IN,        SPDIF,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(DAP2_FS,         I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DIN,        I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DOUT,       I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_SCLK,       I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_MOSI,       SPI1,            PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(SPI1_SCK,        SPI1,            PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(SPI1_MISO,       SPI1,            PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(GMI_IORDY,       RSVD1,           PULL_UP,   TRISTATE,   INPUT),
	DEFAULT_PINMUX(GMI_AD12,        NAND,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GMI_AD14,        NAND,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(KB_ROW8,         KBC,             PULL_UP,   TRISTATE,   INPUT),
};

static __initdata struct tegra_pingroup_config enterprise_unused_pinmux_common[] = {
	DEFAULT_PINMUX(CLK2_OUT,       EXTPERIPH2,       PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CLK2_REQ,       DAP,              PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CLK3_OUT,       EXTPERIPH3,       PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CLK3_REQ,       DEV3,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CLK_32K_OUT,     BLINK,           PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB4,      VGP4,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB5,      VGP5,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB6,      VGP6,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD0,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD1,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD2,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD3,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD4,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD5,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD6,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD7,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
#if !(IS_EXTERNAL_PWM)
	DEFAULT_PINMUX(GMI_AD11,        GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
#endif
	DEFAULT_PINMUX(GMI_CS0_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CS2_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CS3_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CS6_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CS7_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CLK,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_DQS,         RSVD3,           PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_RST_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_WAIT,        GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_WP_N,        GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW6,         KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW7,         KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW9,         KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW11,        KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW13,        KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW14,        KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW15,        KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_PCLK,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_WR_N,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_HSYNC,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_VSYNC,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_SCK,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_SDOUT,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_SDIN,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CRT_HSYNC,       CRT,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CRT_VSYNC,       CRT,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SDMMC3_DAT4,     SDMMC3,          PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SDMMC3_DAT5,     SDMMC3,          PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SDMMC3_DAT6,     SDMMC3,          PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SDMMC3_DAT7,     SDMMC3,          PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPDIF_OUT,       SPDIF,           PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPI1_CS0_N,      SPI1,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPI2_SCK,        SPI2,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPI2_CS0_N,      SPI2,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPI2_MOSI,       SPI2,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPI2_MISO,       SPI2,            PULL_DOWN,    TRISTATE,  OUTPUT),
};

static __initdata struct tegra_pingroup_config enterprise_pinmux_a02[] = {
	DEFAULT_PINMUX(LCD_D10,         DISPLAYA,        PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(LCD_PWR0,        DISPLAYA,        PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(SDMMC4_CMD,      SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_CLK,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DIR,        ULPI,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(ULPI_NXT,        ULPI,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(ULPI_STP,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_FS,         I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_DIN,        I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_DOUT,       I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_SCLK,       I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PV3,        RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_PWR1,        DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_PWR2,        DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_CS0_N,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_DC0,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_DE,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D0,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D1,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D2,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D3,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D4,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D5,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D6,          RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D7,          RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D8,          DISPLAYA,        NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(LCD_D9,          DISPLAYA,        NORMAL,    NORMAL,     INPUT),
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
	DEFAULT_PINMUX(LCD_D22,         RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_D23,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_CS1_N,       DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_M1,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_DC1,         DISPLAYA,        NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART2_RXD,       IRDA,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART2_TXD,       IRDA,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART2_RTS_N,     UARTB,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART2_CTS_N,     UARTB,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART3_TXD,       UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART3_RXD,       UARTC,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART3_CTS_N,     UARTC,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART3_RTS_N,     UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU0,        UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU1,        UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU2,        UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU3,        UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU4,        PWM1,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU5,        PWM2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU6,        PWM3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_FS,         I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DIN,        I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DOUT,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_SCLK,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_A16,         UARTD,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_A17,         UARTD,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_A18,         UARTD,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_A19,         UARTD,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(CAM_MCLK,        VI_ALT2,         NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PCC1,       RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB0,       RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB3,       VGP3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB7,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PCC2,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW10,        KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW12,        KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL3,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PV0,        RSVD,            PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_FS,         I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DIN,        I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DOUT,       I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_SCLK,       I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK1_OUT,        EXTPERIPH1,      NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPDIF_IN,        SPDIF,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_FS,         I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DIN,        I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DOUT,       I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_SCLK,       I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_MOSI,       SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_SCK,        SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_MISO,       SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_IORDY,       RSVD1,           PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD12,        NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD14,        NAND,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW8,         KBC,             PULL_UP,   NORMAL,     INPUT),
};

static struct tegra_gpio_table gpio_table[] = {
	{ .gpio = TEGRA_GPIO_HP_DET,		.enable = true	},
};
static struct tegra_gpio_table tai_gpio_table[] = {
	{ .gpio = TEGRA_GPIO_CODEC_RST,		.enable = true	},
};
struct pin_info_low_power_mode {
	char name[16];
	int gpio_nr;
	bool is_gpio;
	bool is_input;
	int value; /* Value if it is output*/
};

#define PIN_GPIO_LPM(_name, _gpio, _is_input, _value)	\
	{					\
		.name		= _name,	\
		.gpio_nr	= _gpio,	\
		.is_gpio	= true,		\
		.is_input	= _is_input,	\
		.value		= _value,	\
	}
static __initdata struct pin_info_low_power_mode enterprise_unused_gpio_pins_common[] = {
	PIN_GPIO_LPM("CLK2_OUT",     TEGRA_GPIO_PW5,   0, 0),
	PIN_GPIO_LPM("CLK2_REQ",     TEGRA_GPIO_PCC5,  0, 0),
	PIN_GPIO_LPM("CLK3_OUT",     TEGRA_GPIO_PEE0,  0, 0),
	PIN_GPIO_LPM("CLK3_REQ",     TEGRA_GPIO_PEE1,  0, 0),
	PIN_GPIO_LPM("CLK_32K_OUT",  TEGRA_GPIO_PA0,   0, 0),
	PIN_GPIO_LPM("GPIO_PBB4",    TEGRA_GPIO_PBB4,  0, 0),
	PIN_GPIO_LPM("GPIO_PBB5",    TEGRA_GPIO_PBB5,  0, 0),
	PIN_GPIO_LPM("GPIO_PBB6",    TEGRA_GPIO_PBB6,  0, 0),
	PIN_GPIO_LPM("GMI_AD0",      TEGRA_GPIO_PG0,  0, 0),
	PIN_GPIO_LPM("GMI_AD1",      TEGRA_GPIO_PG1,  0, 0),
	PIN_GPIO_LPM("GMI_AD2",      TEGRA_GPIO_PG2,  0, 0),
	PIN_GPIO_LPM("GMI_AD3",      TEGRA_GPIO_PG3,  0, 0),
	PIN_GPIO_LPM("GMI_AD4",      TEGRA_GPIO_PG4,  0, 0),
	PIN_GPIO_LPM("GMI_AD5",      TEGRA_GPIO_PG5,  0, 0),
	PIN_GPIO_LPM("GMI_AD6",      TEGRA_GPIO_PG6,  0, 0),
	PIN_GPIO_LPM("GMI_AD7",      TEGRA_GPIO_PG7,  0, 0),
#if !(IS_EXTERNAL_PWM)
	PIN_GPIO_LPM("GMI_AD11",     TEGRA_GPIO_PH3,  0, 0),
#endif
	PIN_GPIO_LPM("GMI_CS0_N",    TEGRA_GPIO_PJ0,  0, 0),
	PIN_GPIO_LPM("GMI_CS2_N",    TEGRA_GPIO_PK3,  0, 0),
	PIN_GPIO_LPM("GMI_CS3_N",    TEGRA_GPIO_PK4,  0, 0),
	PIN_GPIO_LPM("GMI_CS6_N",    TEGRA_GPIO_PI3,  0, 0),
	PIN_GPIO_LPM("GMI_CS7_N",    TEGRA_GPIO_PI6,  0, 0),
	PIN_GPIO_LPM("GMI_ADV",      TEGRA_GPIO_PK0,  0, 0),
	PIN_GPIO_LPM("GMI_CLK",      TEGRA_GPIO_PK1,  0, 0),
	PIN_GPIO_LPM("GMI_DQS",      TEGRA_GPIO_PI2,  0, 0),
	PIN_GPIO_LPM("GMI_RST_N",    TEGRA_GPIO_PI4,  0, 0),
	PIN_GPIO_LPM("GMI_WAIT",     TEGRA_GPIO_PI7,  0, 0),
	PIN_GPIO_LPM("GMI_WP_N",     TEGRA_GPIO_PC7,  0, 0),
	PIN_GPIO_LPM("KB_ROW6",      TEGRA_GPIO_PR6,  0, 0),
	PIN_GPIO_LPM("KB_ROW7",      TEGRA_GPIO_PR7,  0, 0),
	PIN_GPIO_LPM("KB_ROW9",      TEGRA_GPIO_PS1,  0, 0),
	PIN_GPIO_LPM("KB_ROW11",     TEGRA_GPIO_PS3,  0, 0),
	PIN_GPIO_LPM("KB_ROW13",     TEGRA_GPIO_PS5,  0, 0),
	PIN_GPIO_LPM("KB_ROW14",     TEGRA_GPIO_PS6,  0, 0),
	PIN_GPIO_LPM("KB_ROW15",     TEGRA_GPIO_PS7,  0, 0),
	PIN_GPIO_LPM("LCD_PCLK",     TEGRA_GPIO_PB3,  0, 0),
	PIN_GPIO_LPM("LCD_WR_N",     TEGRA_GPIO_PZ3,  0, 0),
	PIN_GPIO_LPM("LCD_HSYNC",    TEGRA_GPIO_PJ3,  0, 0),
	PIN_GPIO_LPM("LCD_VSYNC",    TEGRA_GPIO_PJ4,  0, 0),
	PIN_GPIO_LPM("LCD_SCK",      TEGRA_GPIO_PZ4,  0, 0),
	PIN_GPIO_LPM("LCD_SDOUT",    TEGRA_GPIO_PN5,  0, 0),
	PIN_GPIO_LPM("LCD_SDIN",     TEGRA_GPIO_PZ2,  0, 0),
	PIN_GPIO_LPM("CRT_HSYNC",    TEGRA_GPIO_PV6,  0, 0),
	PIN_GPIO_LPM("CRT_VSYNC",    TEGRA_GPIO_PV7,  0, 0),
	PIN_GPIO_LPM("SDMMC3_DAT4",  TEGRA_GPIO_PD1,  0, 0),
	PIN_GPIO_LPM("SDMMC3_DAT5",  TEGRA_GPIO_PD0,  0, 0),
	PIN_GPIO_LPM("SDMMC3_DAT6",  TEGRA_GPIO_PD3,  0, 0),
	PIN_GPIO_LPM("SDMMC3_DAT7",  TEGRA_GPIO_PD4,  0, 0),
	PIN_GPIO_LPM("SPDIF_OUT",    TEGRA_GPIO_PK5,  0, 0),
	PIN_GPIO_LPM("SPI1_CS0_N",   TEGRA_GPIO_PX6,  0, 0),
	PIN_GPIO_LPM("SPI2_SCK",     TEGRA_GPIO_PX2,  0, 0),
	PIN_GPIO_LPM("SPI2_CS0_N",   TEGRA_GPIO_PX3,  0, 0),
	PIN_GPIO_LPM("SPI2_MOSI",    TEGRA_GPIO_PX0,  0, 0),
	PIN_GPIO_LPM("SPI2_MISO",    TEGRA_GPIO_PX1,  0, 0),
};

static __initdata struct pin_info_low_power_mode enterprise_unused_gpio_pins_a02[] = {
	PIN_GPIO_LPM("LCD_D10",      TEGRA_GPIO_PF2,  0, 0),
	PIN_GPIO_LPM("LCD_PWR0",     TEGRA_GPIO_PB2,  0, 0),
};

static __initdata struct pin_info_low_power_mode enterprise_gpio_pins_a03[] = {
	PIN_GPIO_LPM("GPIO_PV3",      TEGRA_GPIO_PV3,  0, 0),
	PIN_GPIO_LPM("LCD_DC0",       TEGRA_GPIO_PN6,  0, 0),
	PIN_GPIO_LPM("LCD_D5",        TEGRA_GPIO_PE5,  0, 0),
	PIN_GPIO_LPM("LCD_D20",       TEGRA_GPIO_PM4,  0, 0),
	PIN_GPIO_LPM("LCD_DC1",       TEGRA_GPIO_PD2,  0, 0),
	PIN_GPIO_LPM("GPIO_PU4",      TEGRA_GPIO_PU4,  0, 0),
	PIN_GPIO_LPM("KB_COL3",       TEGRA_GPIO_PQ3,  0, 0),
	PIN_GPIO_LPM("SPI1_MOSI",     TEGRA_GPIO_PX4,  0, 0),
	PIN_GPIO_LPM("SPI1_MISO",     TEGRA_GPIO_PX7,  0, 0),
	PIN_GPIO_LPM("SPI1_SCK",      TEGRA_GPIO_PX5,  0, 0),
};

/*----------------------------- TAI Pinmux--------------------------------- */
static __initdata struct tegra_pingroup_config tai_pinmux_common[] = {
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

	/* SDMMC4 pinmux */
	DEFAULT_PINMUX(SDMMC4_CLK,      SDMMC4,          NORMAL,     NORMAL,     INPUT),
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
	I2C_PINMUX(GEN1_I2C_SCL,        I2C1,            NORMAL,      NORMAL,    INPUT,  DISABLE,  ENABLE),
	I2C_PINMUX(GEN1_I2C_SDA,        I2C1,            NORMAL,      NORMAL,    INPUT,  DISABLE,  ENABLE),

	/* I2C2 pinmux */
	I2C_PINMUX(GEN2_I2C_SCL,        I2C2,            NORMAL,      NORMAL,    INPUT,  DISABLE,  ENABLE),
	I2C_PINMUX(GEN2_I2C_SDA,        I2C2,            NORMAL,      NORMAL,    INPUT,  DISABLE,  ENABLE),

	/* I2C3 pinmux */
	I2C_PINMUX(CAM_I2C_SCL,         I2C3,            NORMAL,      NORMAL,    INPUT,  DISABLE,  ENABLE),
	I2C_PINMUX(CAM_I2C_SDA,         I2C3,            NORMAL,      NORMAL,    INPUT,  DISABLE,  ENABLE),

	/* I2C4 pinmux */
	I2C_PINMUX(DDC_SCL,             I2C4,            PULL_UP,     NORMAL,    INPUT,  DISABLE, DISABLE),
	I2C_PINMUX(DDC_SDA,             I2C4,            PULL_UP,     NORMAL,    INPUT,  DISABLE, DISABLE),

	/* Power I2C pinmux */
	I2C_PINMUX(PWR_I2C_SCL,         I2CPWR,          NORMAL,      NORMAL,    INPUT,  DISABLE,  ENABLE),
	I2C_PINMUX(PWR_I2C_SDA,         I2CPWR,          NORMAL,      NORMAL,    INPUT,  DISABLE,  ENABLE),

	DEFAULT_PINMUX(ULPI_DATA0,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA1,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA2,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA3,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA4,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA5,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA6,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA7,      ULPI,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PV2,        RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(VI_D0,           RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D1,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D2,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D3,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D4,           VI,              NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(VI_D5,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D7,           SDMMC2,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_D10,          RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(VI_MCLK,         VI,              PULL_UP,   NORMAL,     INPUT),

	DEFAULT_PINMUX(GMI_AD11,        PWM3,            NORMAL,    NORMAL,	OUTPUT),

	DEFAULT_PINMUX(JTAG_RTCK,       RTCK,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(KB_ROW0,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW1,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW2,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW3,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL0,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL1,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL2,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL4,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL5,         KBC,             PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK_32K_OUT,     BLINK,           PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(SYS_CLK_REQ,     SYSCLK,          NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(OWR,             OWR,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK1_REQ,        DAP,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI2_CS1_N,      SPI2,            PULL_UP,   NORMAL,     INPUT),
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
	CEC_PINMUX(HDMI_CEC,            CEC,             NORMAL,    TRISTATE,   OUTPUT, DEFAULT, DISABLE),
	DEFAULT_PINMUX(HDMI_INT,        RSVD0,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(VI_D11,          RSVD1,           PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_AD15,        NAND,            PULL_UP,   TRISTATE,   INPUT),
	VI_PINMUX(VI_D6,           VI,              NORMAL,    NORMAL,     OUTPUT, DISABLE, DISABLE),
	VI_PINMUX(VI_D8,           SDMMC2,          NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_D9,           SDMMC2,          NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_PCLK,         RSVD1,           PULL_UP,   TRISTATE,   INPUT,  DISABLE, ENABLE),
	VI_PINMUX(VI_HSYNC,        RSVD1,           NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),
	VI_PINMUX(VI_VSYNC,        RSVD1,           NORMAL,    NORMAL,     INPUT,  DISABLE, DISABLE),

	DEFAULT_PINMUX(LCD_PWR0,        DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D10,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(SDMMC4_CMD,      SDMMC4,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_CLK,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DIR,        ULPI,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(ULPI_NXT,        ULPI,            NORMAL,    TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(ULPI_STP,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_FS,         I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_DIN,        I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_DOUT,       I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_SCLK,       I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PV3,        RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_PWR1,        DISPLAYA,        NORMAL,    TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(LCD_PWR2,        DISPLAYA,        NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(LCD_CS0_N,       DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_DC0,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_DE,          DISPLAYA,        NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(LCD_D0,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D1,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D2,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D3,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D4,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D5,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D6,          RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D7,          RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D8,          DISPLAYA,        NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(LCD_D9,          DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D11,         DISPLAYA,        PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(LCD_D12,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D13,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D14,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D15,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D16,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D17,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D18,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D20,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D21,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D22,         RSVD1,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_D23,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_CS1_N,       DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_DC1,         DISPLAYA,        NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART2_RXD,       IRDA,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART2_TXD,       IRDA,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART2_RTS_N,     UARTB,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART2_CTS_N,     UARTB,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART3_TXD,       UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART3_RXD,       UARTC,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART3_CTS_N,     UARTC,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART3_RTS_N,     UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU0,        UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU1,        UARTA,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GPIO_PU2,        UARTA,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GPIO_PU3,        UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU4,        PWM1,            NORMAL,    NORMAL,   OUTPUT),
	DEFAULT_PINMUX(GPIO_PU5,        PWM2,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GPIO_PU6,        PWM3,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(DAP4_FS,         I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DIN,        I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DOUT,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_SCLK,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_A16,         UARTD,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GMI_A17,         UARTD,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GMI_A18,         UARTD,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GMI_A19,         UARTD,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(CAM_MCLK,        VI_ALT2,         NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PCC1,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB0,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB3,       VGP3,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB7,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PCC2,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW10,        KBC,             NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(KB_ROW12,        KBC,             NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(KB_COL3,         KBC,             PULL_DOWN, TRISTATE,   OUTPUT),
	DEFAULT_PINMUX(GPIO_PV0,        RSVD,            PULL_UP,   TRISTATE,   INPUT),
	DEFAULT_PINMUX(DAP1_FS,         I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DIN,        I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DOUT,       I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_SCLK,       I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK1_OUT,        EXTPERIPH1,      NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(SPDIF_IN,        SPDIF,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(DAP2_DIN,        I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DOUT,       I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_SCLK,       I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_FS,         I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GMI_IORDY,       RSVD1,           PULL_UP,   TRISTATE,   INPUT),
	DEFAULT_PINMUX(GMI_AD14,        NAND,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(KB_ROW8,         KBC,             PULL_UP,   TRISTATE,   INPUT),
	DEFAULT_PINMUX(GMI_CS7_N,       NAND_ALT,        PULL_UP,   TRISTATE,   INPUT),
	DEFAULT_PINMUX(SPI2_MOSI,       SPI6,            PULL_UP,   NORMAL,     OUTPUT),
	DEFAULT_PINMUX(SPI1_MOSI,       SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_SCK,        SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_MISO,       SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPI1_CS0_N,      SPI1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK3_OUT,        EXTPERIPH3,      NORMAL,    NORMAL,     OUTPUT),
	/*DEFAULT_PINMUX(KB_ROW9,       KBC,             PULL_DOWN, TRISTATE,   OUTPUT), //Tai, APCPU_DVS*/

};

static __initdata struct tegra_pingroup_config tai_pinmux_a02[] = {
	DEFAULT_PINMUX(LCD_PWR2,       DISPLAYA,         NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(LCD_PWR1,       DISPLAYA,         NORMAL,    NORMAL,     INPUT),
};

static __initdata struct tegra_pingroup_config tai_pinmux_a03[] = {
	DEFAULT_PINMUX(LCD_PWR2,       DISPLAYA,         NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(LCD_PWR1,       DISPLAYA,         NORMAL,    NORMAL,     OUTPUT),
};

static __initdata struct tegra_pingroup_config tai_unused_pinmux_common[] = {
	DEFAULT_PINMUX(CLK2_OUT,       EXTPERIPH2,       PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CLK2_REQ,       DAP,              PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CLK3_REQ,       DEV3,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CLK_32K_OUT,     BLINK,           PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB4,      VGP4,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB5,      VGP5,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB6,      VGP6,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD0,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD1,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD2,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD3,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD4,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD5,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD6,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD7,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CS0_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CS2_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CS3_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CS6_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_CLK,         GMI,             NORMAL,       TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_DQS,         RSVD3,           PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_RST_N,       GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_WAIT,        GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_WP_N,        GMI,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW6,         KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW7,         KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW11,        KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW13,        KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW14,        KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(KB_ROW15,        KBC,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_PCLK,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_WR_N,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_HSYNC,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_VSYNC,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_SCK,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_SDOUT,       DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_SDIN,        DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CRT_HSYNC,       CRT,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(CRT_VSYNC,       CRT,             PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SDMMC3_DAT4,     SDMMC3,          PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SDMMC3_DAT5,     SDMMC3,          PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SDMMC3_DAT6,     SDMMC3,          PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SDMMC3_DAT7,     SDMMC3,          PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPDIF_OUT,       SPDIF,           PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPI2_SCK,        SPI2,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPI2_CS0_N,      SPI2,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(SPI2_MISO,       SPI2,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_D19,         DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(LCD_M1,          DISPLAYA,        PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD8,         PWM0,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD9,         PWM1,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD10,        PWM2,            PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD12,        RSVD1,           PULL_DOWN,    TRISTATE,  OUTPUT),
	DEFAULT_PINMUX(GMI_AD13,        RSVD1,           PULL_DOWN,    TRISTATE,  OUTPUT),

};

static __initdata struct pin_info_low_power_mode tai_unused_gpio_pins_common[] = {
	PIN_GPIO_LPM("CLK2_OUT",     TEGRA_GPIO_PW5,   0, 0),
	PIN_GPIO_LPM("CLK2_REQ",     TEGRA_GPIO_PCC5,  0, 0),
	PIN_GPIO_LPM("CLK3_REQ",     TEGRA_GPIO_PEE1,  0, 0),
	PIN_GPIO_LPM("CLK_32K_OUT",  TEGRA_GPIO_PA0,   0, 0),
	PIN_GPIO_LPM("GPIO_PBB4",    TEGRA_GPIO_PBB4,  0, 0),
	PIN_GPIO_LPM("GPIO_PBB5",    TEGRA_GPIO_PBB5,  0, 0),
	PIN_GPIO_LPM("GPIO_PBB6",    TEGRA_GPIO_PBB6,  0, 0),
	PIN_GPIO_LPM("GMI_AD0",      TEGRA_GPIO_PG0,  0, 0),
	PIN_GPIO_LPM("GMI_AD1",      TEGRA_GPIO_PG1,  0, 0),
	PIN_GPIO_LPM("GMI_AD2",      TEGRA_GPIO_PG2,  0, 0),
	PIN_GPIO_LPM("GMI_AD3",      TEGRA_GPIO_PG3,  0, 0),
	PIN_GPIO_LPM("GMI_AD4",      TEGRA_GPIO_PG4,  0, 0),
	PIN_GPIO_LPM("GMI_AD5",      TEGRA_GPIO_PG5,  0, 0),
	PIN_GPIO_LPM("GMI_AD6",      TEGRA_GPIO_PG6,  0, 0),
	PIN_GPIO_LPM("GMI_AD7",      TEGRA_GPIO_PG7,  0, 0),
	PIN_GPIO_LPM("GMI_CS0_N",    TEGRA_GPIO_PJ0,  0, 0),
	PIN_GPIO_LPM("GMI_CS2_N",    TEGRA_GPIO_PK3,  0, 0),
	PIN_GPIO_LPM("GMI_CS3_N",    TEGRA_GPIO_PK4,  0, 0),
	PIN_GPIO_LPM("GMI_CS6_N",    TEGRA_GPIO_PI3,  0, 0),
	PIN_GPIO_LPM("GMI_ADV",      TEGRA_GPIO_PK0,  0, 0),
	PIN_GPIO_LPM("GMI_CLK",      TEGRA_GPIO_PK1,  0, 0),
	PIN_GPIO_LPM("GMI_DQS",      TEGRA_GPIO_PI2,  0, 0),
	PIN_GPIO_LPM("GMI_RST_N",    TEGRA_GPIO_PI4,  0, 0),
	PIN_GPIO_LPM("GMI_WAIT",     TEGRA_GPIO_PI7,  0, 0),
	PIN_GPIO_LPM("GMI_WP_N",     TEGRA_GPIO_PC7,  0, 0),
	PIN_GPIO_LPM("KB_ROW6",      TEGRA_GPIO_PR6,  0, 0),
	PIN_GPIO_LPM("KB_ROW7",      TEGRA_GPIO_PR7,  0, 0),
	PIN_GPIO_LPM("KB_ROW11",     TEGRA_GPIO_PS3,  0, 0),
	PIN_GPIO_LPM("KB_ROW13",     TEGRA_GPIO_PS5,  0, 0),
	PIN_GPIO_LPM("KB_ROW14",     TEGRA_GPIO_PS6,  0, 0),
	PIN_GPIO_LPM("KB_ROW15",     TEGRA_GPIO_PS7,  0, 0),
	PIN_GPIO_LPM("LCD_PCLK",     TEGRA_GPIO_PB3,  0, 0),
	PIN_GPIO_LPM("LCD_WR_N",     TEGRA_GPIO_PZ3,  0, 0),
	PIN_GPIO_LPM("LCD_HSYNC",    TEGRA_GPIO_PJ3,  0, 0),
	PIN_GPIO_LPM("LCD_VSYNC",    TEGRA_GPIO_PJ4,  0, 0),
	PIN_GPIO_LPM("LCD_SCK",      TEGRA_GPIO_PZ4,  0, 0),
	PIN_GPIO_LPM("LCD_SDOUT",    TEGRA_GPIO_PN5,  0, 0),
	PIN_GPIO_LPM("LCD_SDIN",     TEGRA_GPIO_PZ2,  0, 0),
	PIN_GPIO_LPM("CRT_HSYNC",    TEGRA_GPIO_PV6,  0, 0),
	PIN_GPIO_LPM("CRT_VSYNC",    TEGRA_GPIO_PV7,  0, 0),
	PIN_GPIO_LPM("SDMMC3_DAT4",  TEGRA_GPIO_PD1,  0, 0),
	PIN_GPIO_LPM("SDMMC3_DAT5",  TEGRA_GPIO_PD0,  0, 0),
	PIN_GPIO_LPM("SDMMC3_DAT6",  TEGRA_GPIO_PD3,  0, 0),
	PIN_GPIO_LPM("SDMMC3_DAT7",  TEGRA_GPIO_PD4,  0, 0),
	PIN_GPIO_LPM("SPDIF_OUT",    TEGRA_GPIO_PK5,  0, 0),
	PIN_GPIO_LPM("SPI2_SCK",     TEGRA_GPIO_PX2,  0, 0),
	PIN_GPIO_LPM("SPI2_CS0_N",   TEGRA_GPIO_PX3,  0, 0),
	PIN_GPIO_LPM("SPI2_MISO",    TEGRA_GPIO_PX1,  0, 0),
	PIN_GPIO_LPM("KB_COL3",       TEGRA_GPIO_PQ3,  0, 0),
	PIN_GPIO_LPM("LCD_D19",      TEGRA_GPIO_PM3,  0, 0),
	PIN_GPIO_LPM("LCD_M1",       TEGRA_GPIO_PW1,  0, 0),
	PIN_GPIO_LPM("GMI_AD8",      TEGRA_GPIO_PH0,  0, 0),
	PIN_GPIO_LPM("GMI_AD9",      TEGRA_GPIO_PH1,  0, 0),
	PIN_GPIO_LPM("GMI_AD10",     TEGRA_GPIO_PH2,  0, 0),
	PIN_GPIO_LPM("GMI_AD12",     TEGRA_GPIO_PH4,  0, 0),
	PIN_GPIO_LPM("GMI_AD13",     TEGRA_GPIO_PH5,  0, 0),
};

static void enterprise_set_unused_pin_gpio(struct pin_info_low_power_mode *lpm_pin_info,
		int list_count)
{
	int i;
	struct pin_info_low_power_mode *pin_info;
	int ret;

	for (i = 0; i < list_count; ++i) {
		pin_info = (struct pin_info_low_power_mode *)(lpm_pin_info + i);
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

int __init enterprise_pinmux_init(void)
{
	struct board_info board_info;
	tegra_get_board_info(&board_info);

	tegra30_default_pinmux();

	if (board_info.board_id != BOARD_E1239) {
		tegra_pinmux_config_table(enterprise_pinmux_common,
				  ARRAY_SIZE(enterprise_pinmux_common));
		tegra_drive_pinmux_config_table(enterprise_drive_pinmux,
				ARRAY_SIZE(enterprise_drive_pinmux));
		tegra_pinmux_config_table(enterprise_unused_pinmux_common,
				ARRAY_SIZE(enterprise_unused_pinmux_common));

		tegra_gpio_config(gpio_table, ARRAY_SIZE(gpio_table));
		enterprise_set_unused_pin_gpio(
				enterprise_unused_gpio_pins_common,
				ARRAY_SIZE(enterprise_unused_gpio_pins_common));

		if (board_info.fab < BOARD_FAB_A03) {
			tegra_pinmux_config_table(enterprise_pinmux_a02,
					  ARRAY_SIZE(enterprise_pinmux_a02));
			enterprise_set_unused_pin_gpio(
				enterprise_unused_gpio_pins_a02,
				ARRAY_SIZE(enterprise_unused_gpio_pins_a02));
		} else {
			tegra_pinmux_config_table(enterprise_pinmux_a03,
					  ARRAY_SIZE(enterprise_pinmux_a03));
			enterprise_set_unused_pin_gpio(enterprise_gpio_pins_a03,
				       ARRAY_SIZE(enterprise_gpio_pins_a03));
		}
	} else {
		tegra_pinmux_config_table(tai_pinmux_common,
					  ARRAY_SIZE(tai_pinmux_common));
		if (board_info.fab <= BOARD_FAB_A02) {
			tegra_pinmux_config_table(tai_pinmux_a02,
						ARRAY_SIZE(tai_pinmux_a02));
		} else {
			tegra_pinmux_config_table(tai_pinmux_a03,
						ARRAY_SIZE(tai_pinmux_a03));
		}
		tegra_drive_pinmux_config_table(enterprise_drive_pinmux,
					ARRAY_SIZE(enterprise_drive_pinmux));
		tegra_pinmux_config_table(tai_unused_pinmux_common,
					ARRAY_SIZE(tai_unused_pinmux_common));

		tegra_gpio_config(gpio_table, ARRAY_SIZE(gpio_table));
		tegra_gpio_config(tai_gpio_table, ARRAY_SIZE(tai_gpio_table));
		enterprise_set_unused_pin_gpio(tai_unused_gpio_pins_common,
				ARRAY_SIZE(tai_unused_gpio_pins_common));

	}

	return 0;
}
