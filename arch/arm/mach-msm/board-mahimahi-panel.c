/* linux/arch/arm/mach-msm/board-mahimahi-panel.c
 *
 * Copyright (c) 2009 Google Inc.
 * Author: Dima Zavin <dima@android.com>
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#include <asm/io.h>
#include <asm/mach-types.h>

#include <mach/msm_fb.h>
#include <mach/msm_iomap.h>
#include <mach/vreg.h>
#include "proc_comm.h"

#include "board-mahimahi.h"
#include "devices.h"


#define SPI_CONFIG              (0x00000000)
#define SPI_IO_CONTROL          (0x00000004)
#define SPI_OPERATIONAL         (0x00000030)
#define SPI_ERROR_FLAGS_EN      (0x00000038)
#define SPI_ERROR_FLAGS         (0x00000038)
#define SPI_OUTPUT_FIFO         (0x00000100)

static void __iomem *spi_base;
static struct clk *spi_clk ;
static struct vreg *vreg_lcm_rftx_2v6;
static struct vreg *vreg_lcm_aux_2v6;

static int qspi_send(uint32_t id, uint8_t data)
{
	uint32_t err;

	/* bit-5: OUTPUT_FIFO_NOT_EMPTY */
	while (readl(spi_base + SPI_OPERATIONAL) & (1<<5)) {
		if ((err = readl(spi_base + SPI_ERROR_FLAGS))) {
			pr_err("%s: ERROR: SPI_ERROR_FLAGS=0x%08x\n", __func__,
			       err);
			return -EIO;
		}
	}
	writel((0x7000 | (id << 9) | data) << 16, spi_base + SPI_OUTPUT_FIFO);
	udelay(100);

	return 0;
}

static int qspi_send_9bit(uint32_t id, uint8_t data)
{
	uint32_t err;

	while (readl(spi_base + SPI_OPERATIONAL) & (1<<5)) {
		err = readl(spi_base + SPI_ERROR_FLAGS);
		if (err) {
			pr_err("%s: ERROR: SPI_ERROR_FLAGS=0x%08x\n", __func__,
			       err);
			return -EIO;
		}
	}
	writel(((id << 8) | data) << 23, spi_base + SPI_OUTPUT_FIFO);
	udelay(100);

	return 0;
}

static int lcm_writeb(uint8_t reg, uint8_t val)
{
	qspi_send(0x0, reg);
	qspi_send(0x1, val);
	return 0;
}

static int lcm_writew(uint8_t reg, uint16_t val)
{
	qspi_send(0x0, reg);
	qspi_send(0x1, val >> 8);
	qspi_send(0x1, val & 0xff);
	return 0;
}

static struct resource resources_msm_fb[] = {
	{
		.start = MSM_FB_BASE,
		.end = MSM_FB_BASE + MSM_FB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct lcm_tbl {
	uint8_t		reg;
	uint8_t		val;
};

static struct lcm_tbl samsung_oled_rgb565_init_table[] = {
	{ 0x31, 0x08 },
	{ 0x32, 0x14 },
	{ 0x30, 0x2 },
	{ 0x27, 0x1 },
	{ 0x12, 0x8 },
	{ 0x13, 0x8 },
	{ 0x15, 0x0 },
	{ 0x16, 0x02 },
	{ 0x39, 0x24 },
	{ 0x17, 0x22 },
	{ 0x18, 0x33 },
	{ 0x19, 0x3 },
	{ 0x1A, 0x1 },
	{ 0x22, 0xA4 },
	{ 0x23, 0x0 },
	{ 0x26, 0xA0 },
};

static struct lcm_tbl samsung_oled_rgb666_init_table[] = {
	{ 0x31, 0x08 },
	{ 0x32, 0x14 },
	{ 0x30, 0x2  },
	{ 0x27, 0x1  },
	{ 0x12, 0x8  },
	{ 0x13, 0x8  },
	{ 0x15, 0x0  },
	{ 0x16, 0x01 },
	{ 0x39, 0x24 },
	{ 0x17, 0x22 },
	{ 0x18, 0x33 },
	{ 0x19, 0x3  },
	{ 0x1A, 0x1  },
	{ 0x22, 0xA4 },
	{ 0x23, 0x0  },
	{ 0x26, 0xA0 },
};

static struct lcm_tbl *init_tablep = samsung_oled_rgb565_init_table;
static size_t init_table_sz = ARRAY_SIZE(samsung_oled_rgb565_init_table);

#define OLED_GAMMA_TABLE_SIZE		(7 * 3)
static struct lcm_tbl samsung_oled_gamma_table[][OLED_GAMMA_TABLE_SIZE] = {
	/* level 10 */
	{
		/* Gamma-R */
		{ 0x40, 0x0 },
		{ 0x41, 0x3f },
		{ 0x42, 0x3f },
		{ 0x43, 0x35 },
		{ 0x44, 0x30 },
		{ 0x45, 0x2c },
		{ 0x46, 0x13 },
		/* Gamma -G */
		{ 0x50, 0x0 },
		{ 0x51, 0x0 },
		{ 0x52, 0x0 },
		{ 0x53, 0x0 },
		{ 0x54, 0x27 },
		{ 0x55, 0x2b },
		{ 0x56, 0x12 },
		/* Gamma -B */
		{ 0x60, 0x0 },
		{ 0x61, 0x3f },
		{ 0x62, 0x3f },
		{ 0x63, 0x34 },
		{ 0x64, 0x2f },
		{ 0x65, 0x2b },
		{ 0x66, 0x1b },
	},

	/* level 40 */
	{
		/* Gamma -R */
		{ 0x40, 0x0 },
		{ 0x41, 0x3f },
		{ 0x42, 0x3e },
		{ 0x43, 0x2e },
		{ 0x44, 0x2d },
		{ 0x45, 0x28 },
		{ 0x46, 0x21 },
		/* Gamma -G */
		{ 0x50, 0x0 },
		{ 0x51, 0x0 },
		{ 0x52, 0x0 },
		{ 0x53, 0x21 },
		{ 0x54, 0x2a },
		{ 0x55, 0x28 },
		{ 0x56, 0x20 },
		/* Gamma -B */
		{ 0x60, 0x0 },
		{ 0x61, 0x3f },
		{ 0x62, 0x3e },
		{ 0x63, 0x2d },
		{ 0x64, 0x2b },
		{ 0x65, 0x26 },
		{ 0x66, 0x2d },
	},

	/* level 70 */
	{
		/* Gamma -R */
		{ 0x40, 0x0 },
		{ 0x41, 0x3f },
		{ 0x42, 0x35 },
		{ 0x43, 0x2c },
		{ 0x44, 0x2b },
		{ 0x45, 0x26 },
		{ 0x46, 0x29 },
		/* Gamma -G */
		{ 0x50, 0x0 },
		{ 0x51, 0x0 },
		{ 0x52, 0x0 },
		{ 0x53, 0x25 },
		{ 0x54, 0x29 },
		{ 0x55, 0x26 },
		{ 0x56, 0x28 },
		/* Gamma -B */
		{ 0x60, 0x0 },
		{ 0x61, 0x3f },
		{ 0x62, 0x34 },
		{ 0x63, 0x2b },
		{ 0x64, 0x2a },
		{ 0x65, 0x23 },
		{ 0x66, 0x37 },
	},

	/* level 100 */
	{
		/* Gamma -R */
		{ 0x40, 0x0 },
		{ 0x41, 0x3f },
		{ 0x42, 0x30 },
		{ 0x43, 0x2a },
		{ 0x44, 0x2b },
		{ 0x45, 0x24 },
		{ 0x46, 0x2f },
		/* Gamma -G */
		{ 0x50, 0x0 },
		{ 0x51, 0x0 },
		{ 0x52, 0x0 },
		{ 0x53, 0x25 },
		{ 0x54, 0x29 },
		{ 0x55, 0x24 },
		{ 0x56, 0x2e },
		/* Gamma -B */
		{ 0x60, 0x0 },
		{ 0x61, 0x3f },
		{ 0x62, 0x2f },
		{ 0x63, 0x29 },
		{ 0x64, 0x29 },
		{ 0x65, 0x21 },
		{ 0x66, 0x3f },
	},

	/* level 130 */
	{
		/* Gamma -R */
		{ 0x40, 0x0 },
		{ 0x41, 0x3f },
		{ 0x42, 0x2e },
		{ 0x43, 0x29 },
		{ 0x44, 0x2a },
		{ 0x45, 0x23 },
		{ 0x46, 0x34 },
		/* Gamma -G */
		{ 0x50, 0x0 },
		{ 0x51, 0x0 },
		{ 0x52, 0xa },
		{ 0x53, 0x25 },
		{ 0x54, 0x28 },
		{ 0x55, 0x23 },
		{ 0x56, 0x33 },
		/* Gamma -B */
		{ 0x60, 0x0 },
		{ 0x61, 0x3f },
		{ 0x62, 0x2d },
		{ 0x63, 0x28 },
		{ 0x64, 0x27 },
		{ 0x65, 0x20 },
		{ 0x66, 0x46 },
	},

	/* level 160 */
	{
		/* Gamma -R */
		{ 0x40, 0x0 },
		{ 0x41, 0x3f },
		{ 0x42, 0x2b },
		{ 0x43, 0x29 },
		{ 0x44, 0x28 },
		{ 0x45, 0x23 },
		{ 0x46, 0x38 },
		/* Gamma -G */
		{ 0x50, 0x0 },
		{ 0x51, 0x0 },
		{ 0x52, 0xb },
		{ 0x53, 0x25 },
		{ 0x54, 0x27 },
		{ 0x55, 0x23 },
		{ 0x56, 0x37 },
		/* Gamma -B */
		{ 0x60, 0x0 },
		{ 0x61, 0x3f },
		{ 0x62, 0x29 },
		{ 0x63, 0x28 },
		{ 0x64, 0x25 },
		{ 0x65, 0x20 },
		{ 0x66, 0x4b },
	},

	/* level 190 */
	{
		/* Gamma -R */
		{ 0x40, 0x0 },
		{ 0x41, 0x3f },
		{ 0x42, 0x29 },
		{ 0x43, 0x29 },
		{ 0x44, 0x27 },
		{ 0x45, 0x22 },
		{ 0x46, 0x3c },
		/* Gamma -G */
		{ 0x50, 0x0 },
		{ 0x51, 0x0 },
		{ 0x52, 0x10 },
		{ 0x53, 0x26 },
		{ 0x54, 0x26 },
		{ 0x55, 0x22 },
		{ 0x56, 0x3b },
		/* Gamma -B */
		{ 0x60, 0x0 },
		{ 0x61, 0x3f },
		{ 0x62, 0x28 },
		{ 0x63, 0x28 },
		{ 0x64, 0x24 },
		{ 0x65, 0x1f },
		{ 0x66, 0x50 },
	},

	/* level 220 */
	{
		/* Gamma -R */
		{ 0x40, 0x0 },
		{ 0x41, 0x3f },
		{ 0x42, 0x28 },
		{ 0x43, 0x28 },
		{ 0x44, 0x28 },
		{ 0x45, 0x20 },
		{ 0x46, 0x40 },
		/* Gamma -G */
		{ 0x50, 0x0 },
		{ 0x51, 0x0 },
		{ 0x52, 0x11 },
		{ 0x53, 0x25 },
		{ 0x54, 0x27 },
		{ 0x55, 0x20 },
		{ 0x56, 0x3f },
		/* Gamma -B */
		{ 0x60, 0x0 },
		{ 0x61, 0x3f },
		{ 0x62, 0x27 },
		{ 0x63, 0x26 },
		{ 0x64, 0x26 },
		{ 0x65, 0x1c },
		{ 0x66, 0x56 },
	},

	/* level 250 */
	{
		/* Gamma -R */
		{ 0x40, 0x0 },
		{ 0x41, 0x3f },
		{ 0x42, 0x2a },
		{ 0x43, 0x27 },
		{ 0x44, 0x27 },
		{ 0x45, 0x1f },
		{ 0x46, 0x44 },
		/* Gamma -G */
		{ 0x50, 0x0 },
		{ 0x51, 0x0 },
		{ 0x52, 0x17 },
		{ 0x53, 0x24 },
		{ 0x54, 0x26 },
		{ 0x55, 0x1f },
		{ 0x56, 0x43 },
		/* Gamma -B */
		{ 0x60, 0x0 },
		{ 0x61, 0x3f },
		{ 0x62, 0x2a },
		{ 0x63, 0x25 },
		{ 0x64, 0x24 },
		{ 0x65, 0x1b },
		{ 0x66, 0x5c },
	},
};
#define SAMSUNG_OLED_NUM_LEVELS		ARRAY_SIZE(samsung_oled_gamma_table)

#define SAMSUNG_OLED_MIN_VAL		10
#define SAMSUNG_OLED_MAX_VAL		250
#define SAMSUNG_OLED_DEFAULT_VAL	(SAMSUNG_OLED_MIN_VAL +		\
					 (SAMSUNG_OLED_MAX_VAL -	\
					  SAMSUNG_OLED_MIN_VAL) / 2)

#define SAMSUNG_OLED_LEVEL_STEP		((SAMSUNG_OLED_MAX_VAL -	\
					  SAMSUNG_OLED_MIN_VAL) /	\
					 (SAMSUNG_OLED_NUM_LEVELS - 1))


#define SONY_TFT_DEF_USER_VAL         102
#define SONY_TFT_MIN_USER_VAL         30
#define SONY_TFT_MAX_USER_VAL         255
#define SONY_TFT_DEF_PANEL_VAL        155
#define SONY_TFT_MIN_PANEL_VAL        26
#define SONY_TFT_MAX_PANEL_VAL        255


static DEFINE_MUTEX(panel_lock);
static struct work_struct brightness_delayed_work;
static DEFINE_SPINLOCK(brightness_lock);
static uint8_t new_val = SAMSUNG_OLED_DEFAULT_VAL;
static uint8_t last_val = SAMSUNG_OLED_DEFAULT_VAL;
static uint8_t table_sel_vals[] = { 0x43, 0x34 };
static int table_sel_idx = 0;
static uint8_t tft_panel_on;

static void gamma_table_bank_select(void)
{
	lcm_writeb(0x39, table_sel_vals[table_sel_idx]);
	table_sel_idx ^= 1;
}

static void samsung_oled_set_gamma_val(int val)
{
	int i;
	int level;
	int frac;

	val = clamp(val, SAMSUNG_OLED_MIN_VAL, SAMSUNG_OLED_MAX_VAL);
	val = (val / 2) * 2;

	level = (val - SAMSUNG_OLED_MIN_VAL) / SAMSUNG_OLED_LEVEL_STEP;
	frac = (val - SAMSUNG_OLED_MIN_VAL) % SAMSUNG_OLED_LEVEL_STEP;

	clk_enable(spi_clk);

	for (i = 0; i < OLED_GAMMA_TABLE_SIZE; ++i) {
		unsigned int v1;
		unsigned int v2 = 0;
		u8 v;
		if (frac == 0) {
			v = samsung_oled_gamma_table[level][i].val;
		} else {

			v1 = samsung_oled_gamma_table[level][i].val;
			v2 = samsung_oled_gamma_table[level+1][i].val;
			v = (v1 * (SAMSUNG_OLED_LEVEL_STEP - frac) +
			     v2 * frac) / SAMSUNG_OLED_LEVEL_STEP;
		}
		lcm_writeb(samsung_oled_gamma_table[level][i].reg, v);
	}

	gamma_table_bank_select();
	clk_disable(spi_clk);
	last_val = val;
}

static int samsung_oled_panel_init(struct msm_lcdc_panel_ops *ops)
{
	pr_info("%s: +()\n", __func__);
	mutex_lock(&panel_lock);

	clk_enable(spi_clk);
	/* Set the gamma write target to 4, leave the current gamma set at 2 */
	lcm_writeb(0x39, 0x24);
	clk_disable(spi_clk);

	mutex_unlock(&panel_lock);
	pr_info("%s: -()\n", __func__);
	return 0;
}

static int samsung_oled_panel_unblank(struct msm_lcdc_panel_ops *ops)
{
	int i;

	pr_info("%s: +()\n", __func__);

	mutex_lock(&panel_lock);

	gpio_set_value(MAHIMAHI_GPIO_LCD_RST_N, 1);
	udelay(50);
	gpio_set_value(MAHIMAHI_GPIO_LCD_RST_N, 0);
	udelay(20);
	gpio_set_value(MAHIMAHI_GPIO_LCD_RST_N, 1);
	msleep(20);

	clk_enable(spi_clk);

	for (i = 0; i < init_table_sz; i++)
		lcm_writeb(init_tablep[i].reg, init_tablep[i].val);

	lcm_writew(0xef, 0xd0e8);
	lcm_writeb(0x1d, 0xa0);
	table_sel_idx = 0;
	gamma_table_bank_select();
	samsung_oled_set_gamma_val(last_val);
	msleep(250);
	lcm_writeb(0x14, 0x03);
	clk_disable(spi_clk);

	mutex_unlock(&panel_lock);

	pr_info("%s: -()\n", __func__);
	return 0;
}

static int samsung_oled_panel_blank(struct msm_lcdc_panel_ops *ops)
{
	pr_info("%s: +()\n", __func__);
	mutex_lock(&panel_lock);

	clk_enable(spi_clk);
	lcm_writeb(0x14, 0x0);
	mdelay(1);
	lcm_writeb(0x1d, 0xa1);
	clk_disable(spi_clk);
	msleep(200);

	gpio_set_value(MAHIMAHI_GPIO_LCD_RST_N, 0);

	mutex_unlock(&panel_lock);
	pr_info("%s: -()\n", __func__);
	return 0;
}

struct lcm_cmd {
	int reg;
	uint32_t val;
	unsigned delay;
};

#define LCM_GPIO_CFG(gpio, func, str) \
		PCOM_GPIO_CFG(gpio, func, GPIO_OUTPUT, GPIO_NO_PULL, str)

static uint32_t sony_tft_display_on_gpio_table[] = {
	LCM_GPIO_CFG(MAHIMAHI_LCD_R1, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_R2, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_R3, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_R4, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_R5, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G0, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G1, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G2, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G3, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G4, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G5, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B1, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B2, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B3, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B4, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B5, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_PCLK, 1, GPIO_4MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_VSYNC, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_HSYNC, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_DE, 1, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_SPI_CLK, 1, GPIO_4MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_SPI_DO, 1, GPIO_4MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_SPI_CSz, 1, GPIO_4MA),
};

static uint32_t sony_tft_display_off_gpio_table[] = {
	LCM_GPIO_CFG(MAHIMAHI_LCD_R1, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_R2, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_R3, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_R4, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_R5, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G0, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G1, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G2, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G3, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G4, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_G5, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B1, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B2, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B3, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B4, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_B5, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_PCLK, 0, GPIO_4MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_VSYNC, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_HSYNC, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_DE, 0, GPIO_8MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_SPI_CLK, 0, GPIO_4MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_SPI_DO, 0, GPIO_4MA),
	LCM_GPIO_CFG(MAHIMAHI_LCD_SPI_CSz, 0, GPIO_4MA),
};

#undef LCM_GPIO_CFG

#define SONY_TFT_DEF_PANEL_DELTA \
		(SONY_TFT_DEF_PANEL_VAL - SONY_TFT_MIN_PANEL_VAL)
#define SONY_TFT_DEF_USER_DELTA \
		(SONY_TFT_DEF_USER_VAL - SONY_TFT_MIN_USER_VAL)

static void sony_tft_set_pwm_val(int val)
{
	pr_info("%s: %d\n", __func__, val);

	last_val = val;

	if (!tft_panel_on)
		return;

	if (val <= SONY_TFT_DEF_USER_VAL) {
		if (val <= SONY_TFT_MIN_USER_VAL)
			val = SONY_TFT_MIN_PANEL_VAL;
		else
			val = SONY_TFT_DEF_PANEL_DELTA *
				(val - SONY_TFT_MIN_USER_VAL) /
				SONY_TFT_DEF_USER_DELTA +
				SONY_TFT_MIN_PANEL_VAL;
	} else
		val = (SONY_TFT_MAX_PANEL_VAL - SONY_TFT_DEF_PANEL_VAL) *
			(val - SONY_TFT_DEF_USER_VAL) /
			(SONY_TFT_MAX_USER_VAL - SONY_TFT_DEF_USER_VAL) +
			SONY_TFT_DEF_PANEL_VAL;

	clk_enable(spi_clk);
	qspi_send_9bit(0x0, 0x51);
	qspi_send_9bit(0x1, val);
	qspi_send_9bit(0x0, 0x53);
	qspi_send_9bit(0x1, 0x24);
	clk_disable(spi_clk);
}

#undef SONY_TFT_DEF_PANEL_DELTA
#undef SONY_TFT_DEF_USER_DELTA

static void sony_tft_panel_config_gpio_table(uint32_t *table, int len)
{
	int n;
	unsigned id;
	for (n = 0; n < len; n++) {
		id = table[n];
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	}
}


static int sony_tft_panel_power(int on)
{
	unsigned id, on_off;

	if (on) {
		on_off = 0;

		vreg_enable(vreg_lcm_aux_2v6);
		vreg_enable(vreg_lcm_rftx_2v6);

		id = PM_VREG_PDOWN_AUX_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on_off, &id);

		id = PM_VREG_PDOWN_RFTX_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on_off, &id);
		mdelay(10);
		gpio_set_value(MAHIMAHI_GPIO_LCD_RST_N, 1);
		mdelay(10);
		gpio_set_value(MAHIMAHI_GPIO_LCD_RST_N, 0);
		udelay(500);
		gpio_set_value(MAHIMAHI_GPIO_LCD_RST_N, 1);
		mdelay(10);
		sony_tft_panel_config_gpio_table(
			sony_tft_display_on_gpio_table,
			ARRAY_SIZE(sony_tft_display_on_gpio_table));
	} else {
		on_off = 1;

		gpio_set_value(MAHIMAHI_GPIO_LCD_RST_N, 0);

		mdelay(120);

		vreg_disable(vreg_lcm_rftx_2v6);
		vreg_disable(vreg_lcm_aux_2v6);

		id = PM_VREG_PDOWN_RFTX_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on_off, &id);

		id = PM_VREG_PDOWN_AUX_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on_off, &id);
		sony_tft_panel_config_gpio_table(
			sony_tft_display_off_gpio_table,
			ARRAY_SIZE(sony_tft_display_off_gpio_table));
	}
	return 0;
}

static int sony_tft_panel_init(struct msm_lcdc_panel_ops *ops)
{
	return 0;
}

static int sony_tft_panel_unblank(struct msm_lcdc_panel_ops *ops)
{
	pr_info("%s: +()\n", __func__);

	mutex_lock(&panel_lock);

	if (tft_panel_on) {
		pr_info("%s: -() already unblanked\n", __func__);
		goto done;
	}

	sony_tft_panel_power(1);
	msleep(45);

	clk_enable(spi_clk);
	qspi_send_9bit(0x0, 0x11);
	msleep(5);
	qspi_send_9bit(0x0, 0x3a);
	qspi_send_9bit(0x1, 0x05);
	msleep(100);
	qspi_send_9bit(0x0, 0x29);
	/* unlock register page for pwm setting */
	qspi_send_9bit(0x0, 0xf0);
	qspi_send_9bit(0x1, 0x5a);
	qspi_send_9bit(0x1, 0x5a);
	qspi_send_9bit(0x0, 0xf1);
	qspi_send_9bit(0x1, 0x5a);
	qspi_send_9bit(0x1, 0x5a);
	qspi_send_9bit(0x0, 0xd0);
	qspi_send_9bit(0x1, 0x5a);
	qspi_send_9bit(0x1, 0x5a);

	qspi_send_9bit(0x0, 0xc2);
	qspi_send_9bit(0x1, 0x53);
	qspi_send_9bit(0x1, 0x12);
	clk_disable(spi_clk);
	msleep(100);
	tft_panel_on = 1;
	sony_tft_set_pwm_val(last_val);

	pr_info("%s: -()\n", __func__);
done:
	mutex_unlock(&panel_lock);
	return 0;
}

static int sony_tft_panel_blank(struct msm_lcdc_panel_ops *ops)
{
	pr_info("%s: +()\n", __func__);

	mutex_lock(&panel_lock);

	clk_enable(spi_clk);
	qspi_send_9bit(0x0, 0x28);
	qspi_send_9bit(0x0, 0x10);
	clk_disable(spi_clk);

	msleep(40);
	sony_tft_panel_power(0);
	tft_panel_on = 0;

	mutex_unlock(&panel_lock);

	pr_info("%s: -()\n", __func__);
	return 0;
}

static struct msm_lcdc_panel_ops mahimahi_lcdc_amoled_panel_ops = {
	.init		= samsung_oled_panel_init,
	.blank		= samsung_oled_panel_blank,
	.unblank	= samsung_oled_panel_unblank,
};

static struct msm_lcdc_panel_ops mahimahi_lcdc_tft_panel_ops = {
	.init		= sony_tft_panel_init,
	.blank		= sony_tft_panel_blank,
	.unblank	= sony_tft_panel_unblank,
};


static struct msm_lcdc_timing mahimahi_lcdc_amoled_timing = {
		.clk_rate		= 24576000,
		.hsync_pulse_width	= 4,
		.hsync_back_porch	= 8,
		.hsync_front_porch	= 8,
		.hsync_skew		= 0,
		.vsync_pulse_width	= 2,
		.vsync_back_porch	= 8,
		.vsync_front_porch	= 8,
		.vsync_act_low		= 1,
		.hsync_act_low		= 1,
		.den_act_low		= 1,
};

static struct msm_lcdc_timing mahimahi_lcdc_tft_timing = {
		.clk_rate		= 24576000,
		.hsync_pulse_width	= 2,
		.hsync_back_porch	= 20,
		.hsync_front_porch	= 20,
		.hsync_skew		= 0,
		.vsync_pulse_width	= 2,
		.vsync_back_porch	= 6,
		.vsync_front_porch	= 4,
		.vsync_act_low		= 1,
		.hsync_act_low		= 1,
		.den_act_low		= 0,
};

static struct msm_fb_data mahimahi_lcdc_fb_data = {
		.xres		= 480,
		.yres		= 800,
		.width		= 48,
		.height		= 80,
		.output_format	= MSM_MDP_OUT_IF_FMT_RGB565,
};

static struct msm_lcdc_platform_data mahimahi_lcdc_amoled_platform_data = {
	.panel_ops	= &mahimahi_lcdc_amoled_panel_ops,
	.timing		= &mahimahi_lcdc_amoled_timing,
	.fb_id		= 0,
	.fb_data	= &mahimahi_lcdc_fb_data,
	.fb_resource	= &resources_msm_fb[0],
};

static struct msm_lcdc_platform_data mahimahi_lcdc_tft_platform_data = {
	.panel_ops	= &mahimahi_lcdc_tft_panel_ops,
	.timing		= &mahimahi_lcdc_tft_timing,
	.fb_id		= 0,
	.fb_data	= &mahimahi_lcdc_fb_data,
	.fb_resource	= &resources_msm_fb[0],
};

static struct platform_device mahimahi_lcdc_amoled_device = {
	.name	= "msm_mdp_lcdc",
	.id	= -1,
	.dev	= {
		.platform_data = &mahimahi_lcdc_amoled_platform_data,
	},
};

static struct platform_device mahimahi_lcdc_tft_device = {
	.name	= "msm_mdp_lcdc",
	.id	= -1,
	.dev	= {
		.platform_data = &mahimahi_lcdc_tft_platform_data,
	},
};

static int mahimahi_init_spi_hack(void)
{
	int ret;

	spi_base = ioremap(MSM_SPI_PHYS, MSM_SPI_SIZE);
	if (!spi_base)
		return -1;

	spi_clk = clk_get(&msm_device_spi.dev, "spi_clk");
	if (IS_ERR(spi_clk)) {
		pr_err("%s: unable to get spi_clk\n", __func__);
		ret = PTR_ERR(spi_clk);
		goto err_clk_get;
	}

	clk_enable(spi_clk);

	printk("spi: SPI_CONFIG=%x\n", readl(spi_base + SPI_CONFIG));
	printk("spi: SPI_IO_CONTROL=%x\n", readl(spi_base + SPI_IO_CONTROL));
	printk("spi: SPI_OPERATIONAL=%x\n", readl(spi_base + SPI_OPERATIONAL));
	printk("spi: SPI_ERROR_FLAGS_EN=%x\n",
	       readl(spi_base + SPI_ERROR_FLAGS_EN));
	printk("spi: SPI_ERROR_FLAGS=%x\n", readl(spi_base + SPI_ERROR_FLAGS));
	printk("-%s()\n", __FUNCTION__);
	clk_disable(spi_clk);

	return 0;

err_clk_get:
	iounmap(spi_base);
	return ret;
}

static void mahimahi_brightness_set(struct led_classdev *led_cdev,
				    enum led_brightness val)
{
	unsigned long flags;
	led_cdev->brightness = val;

	spin_lock_irqsave(&brightness_lock, flags);
	new_val = val;
	spin_unlock_irqrestore(&brightness_lock, flags);

	schedule_work(&brightness_delayed_work);
}

static void mahimahi_brightness_amoled_set_work(struct work_struct *work_ptr)
{
	unsigned long flags;
	uint8_t val;

	spin_lock_irqsave(&brightness_lock, flags);
	val = new_val;
	spin_unlock_irqrestore(&brightness_lock, flags);

	mutex_lock(&panel_lock);
	samsung_oled_set_gamma_val(val);
	mutex_unlock(&panel_lock);
}

static void mahimahi_brightness_tft_set_work(struct work_struct *work_ptr)
{
	unsigned long flags;
	uint8_t val;

	spin_lock_irqsave(&brightness_lock, flags);
	val = new_val;
	spin_unlock_irqrestore(&brightness_lock, flags);

	mutex_lock(&panel_lock);
	sony_tft_set_pwm_val(val);
	mutex_unlock(&panel_lock);
}

static struct led_classdev mahimahi_brightness_led = {
	.name = "lcd-backlight",
	.brightness = LED_FULL,
	.brightness_set = mahimahi_brightness_set,
};

int __init mahimahi_init_panel(void)
{
	int ret;

	if (!machine_is_mahimahi())
		return 0;

	if (system_rev > 0xC0) {
		/* CDMA version (except for EVT1) supports RGB666 */
		init_tablep = samsung_oled_rgb666_init_table;
		init_table_sz = ARRAY_SIZE(samsung_oled_rgb666_init_table);
		mahimahi_lcdc_fb_data.output_format = MSM_MDP_OUT_IF_FMT_RGB666;
	}

	ret = platform_device_register(&msm_device_mdp);
	if (ret != 0)
		return ret;

	ret = mahimahi_init_spi_hack();
	if (ret != 0)
		return ret;

	if (gpio_get_value(MAHIMAHI_GPIO_LCD_ID0)) {
		pr_info("%s: tft panel\n", __func__);
		vreg_lcm_rftx_2v6 = vreg_get(0, "rftx");
		if (IS_ERR(vreg_lcm_rftx_2v6))
			return PTR_ERR(vreg_lcm_rftx_2v6);
		vreg_set_level(vreg_lcm_rftx_2v6, 2600);

		vreg_lcm_aux_2v6 = vreg_get(0, "gp4");
		if (IS_ERR(vreg_lcm_aux_2v6))
			return PTR_ERR(vreg_lcm_aux_2v6);

		if (gpio_get_value(MAHIMAHI_GPIO_LCD_RST_N))
			tft_panel_on = 1;
		ret = platform_device_register(&mahimahi_lcdc_tft_device);
		INIT_WORK(&brightness_delayed_work, mahimahi_brightness_tft_set_work);
	} else {
		pr_info("%s: amoled panel\n", __func__);
		ret = platform_device_register(&mahimahi_lcdc_amoled_device);
		INIT_WORK(&brightness_delayed_work, mahimahi_brightness_amoled_set_work);
	}

	if (ret != 0)
		return ret;

	ret = led_classdev_register(NULL, &mahimahi_brightness_led);
	if (ret != 0) {
		pr_err("%s: Cannot register brightness led\n", __func__);
		return ret;
	}

	return 0;
}

device_initcall(mahimahi_init_panel);
