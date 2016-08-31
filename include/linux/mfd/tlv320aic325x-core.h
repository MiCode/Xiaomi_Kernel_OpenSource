/*
 * MFD driver for aic
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

#ifndef __MFD_AIC3XXX_CORE_H__
#define __MFD_AIC3XXX_CORE_H__

#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/irqdomain.h>

enum aic325x_type {
	TLV320AIC3262 = 0,
	TLV320AIC3266 = 1,
	TLV320AIC3256 = 2,
};

#define AIC3XXX_IRQ_HEADSET_DETECT	0
#define AIC3XXX_IRQ_BUTTON_PRESS	1
#define AIC3XXX_IRQ_DAC_DRC		2
#define AIC3XXX_IRQ_AGC_NOISE		3
#define AIC3XXX_IRQ_OVER_CURRENT	4
#define AIC3XXX_IRQ_OVERFLOW_EVENT	5
#define AIC3XXX_IRQ_SPEAKER_OVER_TEMP	6


union aic325x_reg_union {
	struct aic325x_reg {
		u8 offset;
		u8 page;
		u8 book;
		u8 reserved;
	} aic325x_register;
	unsigned int aic325x_register_int;
};

/****************************             ************************************/

/**
 * Platform data for aic325x family device.
 *
 * @audio_mclk1: MCLK1 frequency in Hz
 * @audio_mclk2: MCLK2 frequency in Hz
 * @gpio_irq: whether AIC3262 interrupts the host AP on a GPIO pin
 *		of AP
 * @gpio_reset: is the codec being reset by a gpio [host] pin,
 *		if yes provide the number.
 * @num_gpios: number of gpio pins on this device
 * @gpio_defaults: all gpio configuration
 * @naudint_irq: audio interrupt number
 * @irq_base: base of chained interrupt handler
 */
struct aic325x_pdata {
	unsigned int audio_mclk1;
	unsigned int audio_mclk2;
	unsigned int gpio_irq;	/* whether AIC3256 interrupts the host AP on */
				/* a GPIO pin of AP */
	unsigned int gpio_reset;/* is the codec being reset by a gpio*/
				/* [host] pin, if yes provide the number. */
	struct aic325x_gpio_setup *gpio_defaults;/* all gpio configuration */
	int naudint_irq;	/* audio interrupt */
	int num_gpios;
	unsigned int irq_base;
};

struct aic325x {
	struct mutex io_lock;
	struct mutex irq_lock;
	enum aic325x_type type;
	struct device *dev;
	struct regmap *regmap;
	struct aic325x_pdata pdata;
	void *control_data;
	unsigned int irq;
	unsigned int irq_base;
	struct irq_domain *domain;
	u8 irq_masks_cur;
	u8 irq_masks_cache;
	/* Used over suspend/resume */
	bool suspended;
	u8 book_no;
	u8 page_no;
};

struct aic325x_gpio_setup {
	unsigned int reg;	/* if GPIO is input,
					register to write the mask. */
	u8 value;		/* value to be written
					gpio_control_reg if GPIO */
				/* is output, in_reg if its input */
};


static inline int aic325x_request_irq(struct aic325x *aic325x, int irq,
				      irq_handler_t handler,
				      unsigned long irqflags, const char *name,
				      void *data)
{
	irq = irq_create_mapping(aic325x->domain, irq);
	if (irq < 0) {
		dev_err(aic325x->dev,
			"Mapping hardware interrupt failed %d\n", irq);
		return irq;
	}

	return request_threaded_irq(irq, NULL, handler,
				    irqflags, name, data);
}

static inline int aic325x_free_irq(struct aic325x *aic325x, int irq, void *data)
{
	if (!aic325x->irq_base)
		return -EINVAL;

	free_irq(aic325x->irq_base + irq, data);
	return 0;
}

/* Device I/O API */
int aic325x_reg_read(struct aic325x *aic325x, unsigned int reg);
int aic325x_reg_write(struct aic325x *aic325x, unsigned int reg,
		      unsigned char val);
int aic325x_set_bits(struct aic325x *aic325x, unsigned int reg,
		     unsigned char mask, unsigned char val);
int aic325x_bulk_read(struct aic325x *aic325x, unsigned int reg,
		      int count, u8 *buf);
int aic325x_bulk_write(struct aic325x *aic325x, unsigned int reg,
		       int count, const u8 *buf);
int aic325x_wait_bits(struct aic325x *aic325x, unsigned int reg,
		      unsigned char mask, unsigned char val, int delay,
		      int counter);

int aic325x_irq_init(struct aic325x *aic325x);
void aic325x_irq_exit(struct aic325x *aic325x);
int aic325x_device_init(struct aic325x *aic325x);
void aic325x_device_exit(struct aic325x *aic325x);

#endif /* End of __MFD_AIC3XXX_CORE_H__ */
