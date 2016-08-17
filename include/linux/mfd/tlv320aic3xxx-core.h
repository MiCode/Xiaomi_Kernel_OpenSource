/*
 * MFD driver for aic3262
 *
 * Author:      Mukund Navada <navada@ti.com>
 *              Mehar Bajwa <mehar.bajwa@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __MFD_AIC3262_CORE_H__
#define __MFD_AIC3262_CORE_H__

#include <linux/interrupt.h>
#include <linux/mfd/core.h>
enum aic3xxx_type {
	TLV320AIC3262 = 0,
	TLV320AIC3266 = 1,
	TLV320AIC3285 = 2,
};

#define AIC3262_IRQ_HEADSET_DETECT	0
#define AIC3262_IRQ_BUTTON_PRESS	1
#define AIC3262_IRQ_DAC_DRC		2
#define AIC3262_IRQ_AGC_NOISE		3
#define AIC3262_IRQ_OVER_CURRENT	4
#define AIC3262_IRQ_OVERFLOW_EVENT	5
#define AIC3262_IRQ_SPEAKER_OVER_TEMP	6

#define AIC3262_GPIO1			7
#define AIC3262_GPIO2			8
#define AIC3262_GPI1			9
#define AIC3262_GPI2			10
#define AIC3262_GPO1			11
#define AIC3285_GPIO3			9
#define AIC3285_GPIO4			10
#define AIC3285_GPIO5			11
#define AIC3285_GPIO6			12
#define AIC3285_GPIO7			13
#define AIC3285_GPIO8			14
#define AIC3285_GPIO9			15
#define AIC3285_GPIO10			16
#define AIC3285_GPIO11			17
#define AIC3285_GPIO12			18
#define AIC3285_GPO1			19

struct aic3262_codec_data {
	u16 hs_left_step;
	u16 hs_right_step;
	u16 hf_left_step;
	u16 hf_right_step;
};

struct aic3262_platform_data {
	int audpwron_gpio;	/* audio power-on gpio */
	unsigned int irq_base;

	struct aic3262_codec_data *codec;
};

union aic3xxx_reg_union {
	struct aic3xxx_reg {
		u8 offset;
		u8 page;
		u8 book;
		u8 reserved;
	} aic3xxx_register;
	unsigned int aic3xxx_register_int;
};

/****************************             ************************************/

/*
 *****************************************************************************
 * Structures Definitions
 *****************************************************************************
 */
/*
 *----------------------------------------------------------------------------
 * @struct  aic3262_setup_data |
 *          i2c specific data setup for AIC3262.
 * @field   unsigned short |i2c_address |
 *          Unsigned short for i2c address.
 *----------------------------------------------------------------------------
 */
struct aic3262_setup_data {
	unsigned short i2c_address;
};

/* GPIO API */
#define AIC3262_NUM_GPIO 5	/* include 2 GPI and 1 GPO pins */
enum {
	AIC3262_GPIO1_FUNC_DISABLED =		0,
	AIC3262_GPIO1_FUNC_INPUT =		1,
	AIC3262_GPIO1_FUNC_OUTPUT =		3,
	AIC3262_GPIO1_FUNC_CLOCK_OUTPUT =	4,
	AIC3262_GPIO1_FUNC_INT1_OUTPUT =	5,
	AIC3262_GPIO1_FUNC_INT2_OUTPUT =	6,
	AIC3285_GPIO_FUNC_DSD_CHAN1_OUTPUT =	7,
	AIC3285_GPIO_FUNC_DSD_CHAN2_OUTPUT =	8,
	AIC3285_GPIO_FUNC_DAC_MOD_CLK_OUTPUT =	9,
	AIC3262_GPIO1_FUNC_ADC_MOD_CLK_OUTPUT =	10,
	AIC3262_GPIO1_FUNC_SAR_ADC_INTERRUPT =	12,
	AIC3262_GPIO1_FUNC_ASI1_DATA_OUTPUT =	15,
	AIC3262_GPIO1_FUNC_ASI1_WCLK =		16,
	AIC3262_GPIO1_FUNC_ASI1_BCLK =		17,
	AIC3262_GPIO1_FUNC_ASI2_WCLK =		18,
	AIC3262_GPIO1_FUNC_ASI2_BCLK =		19,
	AIC3262_GPIO1_FUNC_ASI3_WCLK =		20,
	AIC3262_GPIO1_FUNC_ASI3_BCLK =		21,
	AIC3285_GPIO_I2C_MASTER_SCL =		30,
	AIC3285_GPIO_I2C_MASTER_SDA =		30,
};

enum {
	AIC3262_GPIO2_FUNC_DISABLED =		0,
	AIC3262_GPIO2_FUNC_INPUT =		1,
	AIC3262_GPIO2_FUNC_OUTPUT =		3,
	AIC3262_GPIO2_FUNC_CLOCK_OUTPUT =	4,
	AIC3262_GPIO2_FUNC_INT1_OUTPUT =	5,
	AIC3262_GPIO2_FUNC_INT2_OUTPUT =	6,
	AIC3262_GPIO2_FUNC_ADC_MOD_CLK_OUTPUT = 10,
	AIC3262_GPIO2_FUNC_SAR_ADC_INTERRUPT =	12,
	AIC3262_GPIO2_FUNC_ASI1_DATA_OUTPUT =	15,
	AIC3262_GPIO2_FUNC_ASI1_WCLK =		16,
	AIC3262_GPIO2_FUNC_ASI1_BCLK =		17,
	AIC3262_GPIO2_FUNC_ASI2_WCLK =		18,
	AIC3262_GPIO2_FUNC_ASI2_BCLK =		19,
	AIC3262_GPIO2_FUNC_ASI3_WCLK =		20,
	AIC3262_GPIO2_FUNC_ASI3_BCLK =		21
};
enum {
	AIC3262_GPO1_FUNC_DISABLED =		0,
	AIC3262_GPO1_FUNC_MSO_OUTPUT_FOR_SPI =	1,
	AIC3262_GPO1_FUNC_GENERAL_PURPOSE_OUTPUT = 2,
	AIC3262_GPO1_FUNC_CLOCK_OUTPUT =	3,
	AIC3262_GPO1_FUNC_INT1_OUTPUT =	4,
	AIC3262_GPO1_FUNC_INT2_OUTPUT =	5,
	AIC3262_GPO1_FUNC_ADC_MOD_CLK_OUTPUT =	7,
	AIC3262_GPO1_FUNC_SAR_ADC_INTERRUPT =	12,
	AIC3262_GPO1_FUNC_ASI1_DATA_OUTPUT =	15,
};
/*
 *----------------------------------------------------------------------------
 * @struct  aic3262_configs |
 *          AIC3262 initialization data which has register offset and register
 *          value.
 * @field   u8 | book_no |
 *          AIC3262 Book Number Offsets required for initialization..
 * @field   u16 | reg_offset |
 *          AIC3262 Register offsets required for initialization..
 * @field   u8 | reg_val |
 *          value to set the AIC3262 register to initialize the AIC3262.
 *---------------------------------------------------------------------------
 */
struct aic3xxx_configs {
	u8 book_no;
	u16 reg_offset;
	u8 reg_val;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic3262_rate_divs |
 *          Setting up the values to get different freqencies
 *
 * @field   u32 | mclk |
 *          Master clock
 * @field   u32 | rate |
 *          sample rate
 * @field   u8 | p_val |
 *          value of p in PLL
 * @field   u32 | pll_j |
 *          value for pll_j
 * @field   u32 | pll_d |
 *          value for pll_d
 * @field   u32 | dosr |
 *          value to store dosr
 * @field   u32 | ndac |
 *          value for ndac
 * @field   u32 | mdac |
 *          value for mdac
 * @field   u32 | aosr |
 *          value for aosr
 * @field   u32 | nadc |
 *          value for nadc
 * @field   u32 | madc |
 *          value for madc
 * @field   u32 | blck_N |
 *          value for block N
 */
struct aic3xxx {
	struct mutex io_lock;
	struct mutex irq_lock;
	enum aic3xxx_type type;
	struct device *dev;
	struct regmap *regmap;
	void *control_data;
	unsigned int irq;
	unsigned int irq_base;
	u8 irq_masks_cur;
	u8 irq_masks_cache;
	/* Used over suspend/resume */
	bool suspended;
	u8 book_no;
	u8 page_no;
	int shutdown_complete;
};

struct aic3262_gpio_setup {
	u8 used;		/* GPIO, GPI and GPO is used in the board, */
				/* used = 1 else 0 */
	u8 in;			/* GPIO is used as input, in = 1 else in = 0 */
				/* GPI in = 1, GPO in = 0 */
	unsigned int in_reg;	/* if GPIO is input,
					register to write the mask. */
	u8 in_reg_bitmask;	/* bitmask for 'value' to be
					written into in_reg */
	u8 in_reg_shift;	/* bits to shift to write 'value'
					into in_reg */
	u8 value;		/* value to be written
					gpio_control_reg if GPIO */
				/* is output, in_reg if its input */
};

struct aic3xxx_pdata {
	unsigned int audio_mclk1;
	unsigned int audio_mclk2;
	unsigned int gpio_irq;	/* whether AIC3262 interrupts the host AP on */
				/* a GPIO pin of AP */
	unsigned int gpio_reset;/* is the codec being reset by a gpio*/
				/* [host] pin, if yes provide the number. */
	struct aic3262_gpio_setup *gpio;/* all gpio configuration */
	int naudint_irq;	/* audio interrupt */
	unsigned int irq_base;
};

static inline int aic3xxx_request_irq(struct aic3xxx *aic3xxx, int irq,
				      irq_handler_t handler,
				      unsigned long irqflags, const char *name,
				      void *data)
{
	if (!aic3xxx->irq_base)
		return -EINVAL;

	return request_threaded_irq(aic3xxx->irq_base + irq, NULL, handler,
				    irqflags, name, data);
}

static inline int aic3xxx_free_irq(struct aic3xxx *aic3xxx, int irq, void *data)
{
	if (!aic3xxx->irq_base)
		return -EINVAL;

	free_irq(aic3xxx->irq_base + irq, data);
	return 0;
}

/* Device I/O API */
int aic3xxx_reg_read(struct aic3xxx *aic3xxx, unsigned int reg);
int aic3xxx_reg_write(struct aic3xxx *aic3xxx, unsigned int reg,
		      unsigned char val);
int aic3xxx_set_bits(struct aic3xxx *aic3xxx, unsigned int reg,
		     unsigned char mask, unsigned char val);
int aic3xxx_bulk_read(struct aic3xxx *aic3xxx, unsigned int reg,
		      int count, u8 *buf);
int aic3xxx_bulk_write(struct aic3xxx *aic3xxx, unsigned int reg,
		       int count, const u8 *buf);
int aic3xxx_wait_bits(struct aic3xxx *aic3xxx, unsigned int reg,
		      unsigned char mask, unsigned char val, int delay,
		      int counter);

int aic3xxx_irq_init(struct aic3xxx *aic3xxx);
void aic3xxx_irq_exit(struct aic3xxx *aic3xxx);
int aic3xxx_device_init(struct aic3xxx *aic3xxx, int irq);
void aic3xxx_device_exit(struct aic3xxx *aic3xxx);

#endif /* End of __MFD_AIC3262_CORE_H__ */
