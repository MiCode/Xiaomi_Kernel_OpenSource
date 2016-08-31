/*
 * arch/arm/mach-tegra/board-bonaire-pinmux.c
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation
 * Copyright (c) 2013 NVIDIA CORPORATION. All rights reserved.
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
#include <linux/bug.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t12.h>

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


/* !!!FIXME!!!! POPULATE THIS TABLE */
static __initdata struct tegra_drive_pingroup_config bonaire_drive_pinmux[] = {
	/* DEFAULT_DRIVE(<pin_group>), */
};

#define DEFAULT_PINMUX(_pingroup, _mux, _pupd, _tri, _io)	\
	{							\
		.pingroup	= TEGRA_PINGROUP_##_pingroup,	\
		.func		= TEGRA_MUX_##_mux,		\
		.pupd		= TEGRA_PUPD_##_pupd,		\
		.tristate	= TEGRA_TRI_##_tri,		\
		.io		= TEGRA_PIN_##_io,		\
	}

/* !!!FIXME!!!! POPULATE THIS TABLE */
static __initdata struct tegra_pingroup_config bonaire_pinmux[] = {
	DEFAULT_PINMUX(ULPI_DATA0,      SPI3,            NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA1,      SPI3,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA2,      ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA3,      ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA4,      ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA5,      ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA6,      ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DATA7,      ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_CLK,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_DIR,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_NXT,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(ULPI_STP,        ULPI,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_FS,         I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_DIN,        I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_DOUT,       I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP3_SCLK,       I2S2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_CLK,      CLK12,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_CMD,      SPDIF,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT3,     SPDIF,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT2,     SDMMC1,          NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT1,     SDMMC1,          NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT0,     RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK2_OUT,        EXTPERIPH2,      NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK2_REQ,        DAP,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DDC_SCL,         I2C4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DDC_SDA,         I2C4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART2_RXD,       SPI4,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART2_TXD,       SPI4,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART2_RTS_N,     SPI4,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART2_CTS_N,     SPI4,            NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(UART3_TXD,       UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(UART3_RXD,       UARTC,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART3_CTS_N,     UARTC,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(UART3_RTS_N,     UARTC,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(GPIO_PU0,        OWR,             NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GPIO_PU1,        RSVD0,           NORMAL,    TRISTATE,   INPUT),
	DEFAULT_PINMUX(GPIO_PU2,        RSVD0,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU3,        PWM0,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU4,        PWM1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU5,        PWM2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PU6,        RSVD2,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GEN1_I2C_SDA,    I2C1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GEN1_I2C_SCL,    I2C1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_FS,         I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DIN,        I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_DOUT,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP4_SCLK,       I2S3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK3_OUT,        EXTPERIPH3,      NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(CLK3_REQ,        DEV3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GEN2_I2C_SCL,    I2C2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GEN2_I2C_SDA,    I2C2,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_CLK,      SDMMC4,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_CMD,      SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT0,     SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT1,     SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT2,     SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT3,     SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT4,     SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT5,     SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT6,     SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT7,     SDMMC4,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(CAM_MCLK,        VI,              NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PCC1,       RSVD1,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB0,       VGP6,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CAM_I2C_SCL,     I2C3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CAM_I2C_SDA,     I2C3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB3,       VGP3,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB4,       VGP4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB5,       VGP5,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB6,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PBB7,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PCC2,       I2S4,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(JTAG_RTCK,       RTCK,            NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(PWR_I2C_SCL,     I2CPWR,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(PWR_I2C_SDA,     I2CPWR,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW0,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW1,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW2,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW3,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW4,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW5,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW6,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW7,         UARTA,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(KB_ROW8,         UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW9,         UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_ROW10,        UARTA,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL0,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL1,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL2,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL3,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL4,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL5,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL6,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(KB_COL7,         KBC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK_32K_OUT,     BLINK,           NORMAL,    NORMAL,     OUTPUT),
#ifdef CONFIG_SND_HDA_TEGRA
	DEFAULT_PINMUX(DAP1_FS,         HDA,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DIN,        HDA,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DOUT,       HDA,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_SCLK,       HDA,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(CLK1_REQ,        DAP1,            NORMAL,    NORMAL,     INPUT),
#else
	DEFAULT_PINMUX(DAP1_FS,         I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DIN,        I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_DOUT,       I2S0,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP1_SCLK,       I2S0,            NORMAL,    NORMAL,     INPUT),
#endif
	DEFAULT_PINMUX(SPDIF_IN,        SPDIF,           NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SPDIF_OUT,       SPDIF,           NORMAL,    NORMAL,     OUTPUT),
	DEFAULT_PINMUX(DAP2_FS,         I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DIN,        I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_DOUT,       I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(DAP2_SCLK,       I2S1,            NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_CLK,      SDMMC3,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_CMD,      SDMMC3,          PULL_UP,   NORMAL,	INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT0,     SDMMC3,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT1,     SDMMC3,          PULL_UP,   NORMAL,	INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT2,     SDMMC3,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT3,     SDMMC3,          PULL_UP,   NORMAL,     INPUT),
	DEFAULT_PINMUX(HDMI_CEC,        CEC,             NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(SDMMC1_WP_N,     SDMMC1,          NORMAL,    NORMAL,     INPUT),
	DEFAULT_PINMUX(GPIO_PV1,        RSVD0,           PULL_UP,   NORMAL,     OUTPUT),
};

void __init bonaire_pinmux_init(void)
{
	tegra12x_default_pinmux();
	tegra_pinmux_config_table(bonaire_pinmux, ARRAY_SIZE(bonaire_pinmux));
	tegra_drive_pinmux_config_table(bonaire_drive_pinmux,
					ARRAY_SIZE(bonaire_drive_pinmux));
}
