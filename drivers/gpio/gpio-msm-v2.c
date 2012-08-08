/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>

#include <mach/msm_iomap.h>
#include <mach/gpiomux.h>
#include "gpio-msm-common.h"

/* Bits of interest in the GPIO_IN_OUT register.
 */
enum {
	GPIO_IN_BIT  = 0,
	GPIO_OUT_BIT = 1
};

/* Bits of interest in the GPIO_INTR_STATUS register.
 */
enum {
	INTR_STATUS_BIT = 0,
};

/* Bits of interest in the GPIO_CFG register.
 */
enum {
	GPIO_OE_BIT = 9,
};

/* Bits of interest in the GPIO_INTR_CFG register.
 */
enum {
	INTR_ENABLE_BIT        = 0,
	INTR_POL_CTL_BIT       = 1,
	INTR_DECT_CTL_BIT      = 2,
	INTR_RAW_STATUS_EN_BIT = 3,
};

/* Codes of interest in GPIO_INTR_CFG_SU.
 */
enum {
	TARGET_PROC_SCORPION = 4,
	TARGET_PROC_NONE     = 7,
};

/*
 * There is no 'DC_POLARITY_LO' because the GIC is incapable
 * of asserting on falling edge or level-low conditions.  Even though
 * the registers allow for low-polarity inputs, the case can never arise.
 */
enum {
	DC_POLARITY_HI	= BIT(11),
	DC_IRQ_ENABLE	= BIT(3),
};

/*
 * When a GPIO triggers, two separate decisions are made, controlled
 * by two separate flags.
 *
 * - First, INTR_RAW_STATUS_EN controls whether or not the GPIO_INTR_STATUS
 * register for that GPIO will be updated to reflect the triggering of that
 * gpio.  If this bit is 0, this register will not be updated.
 * - Second, INTR_ENABLE controls whether an interrupt is triggered.
 *
 * If INTR_ENABLE is set and INTR_RAW_STATUS_EN is NOT set, an interrupt
 * can be triggered but the status register will not reflect it.
 */
#define INTR_RAW_STATUS_EN BIT(INTR_RAW_STATUS_EN_BIT)
#define INTR_ENABLE        BIT(INTR_ENABLE_BIT)
#define INTR_DECT_CTL_EDGE BIT(INTR_DECT_CTL_BIT)
#define INTR_POL_CTL_HI    BIT(INTR_POL_CTL_BIT)

#define GPIO_INTR_CFG_SU(gpio)    (MSM_TLMM_BASE + 0x0400 + (0x04 * (gpio)))
#define DIR_CONN_INTR_CFG_SU(irq) (MSM_TLMM_BASE + 0x0700 + (0x04 * (irq)))
#define GPIO_CONFIG(gpio)         (MSM_TLMM_BASE + 0x1000 + (0x10 * (gpio)))
#define GPIO_IN_OUT(gpio)         (MSM_TLMM_BASE + 0x1004 + (0x10 * (gpio)))
#define GPIO_INTR_CFG(gpio)       (MSM_TLMM_BASE + 0x1008 + (0x10 * (gpio)))
#define GPIO_INTR_STATUS(gpio)    (MSM_TLMM_BASE + 0x100c + (0x10 * (gpio)))

static inline void set_gpio_bits(unsigned n, void __iomem *reg)
{
	__raw_writel(__raw_readl(reg) | n, reg);
}

static inline void clr_gpio_bits(unsigned n, void __iomem *reg)
{
	__raw_writel(__raw_readl(reg) & ~n, reg);
}

unsigned __msm_gpio_get_inout(unsigned gpio)
{
	return __raw_readl(GPIO_IN_OUT(gpio)) & BIT(GPIO_IN_BIT);
}

void __msm_gpio_set_inout(unsigned gpio, unsigned val)
{
	__raw_writel(val ? BIT(GPIO_OUT_BIT) : 0, GPIO_IN_OUT(gpio));
}

void __msm_gpio_set_config_direction(unsigned gpio, int input, int val)
{
	if (input)
		clr_gpio_bits(BIT(GPIO_OE_BIT), GPIO_CONFIG(gpio));
	else {
		__msm_gpio_set_inout(gpio, val);
		set_gpio_bits(BIT(GPIO_OE_BIT), GPIO_CONFIG(gpio));
	}
}

void __msm_gpio_set_polarity(unsigned gpio, unsigned val)
{
	if (val)
		clr_gpio_bits(INTR_POL_CTL_HI, GPIO_INTR_CFG(gpio));
	else
		set_gpio_bits(INTR_POL_CTL_HI, GPIO_INTR_CFG(gpio));
}

unsigned __msm_gpio_get_intr_status(unsigned gpio)
{
	return __raw_readl(GPIO_INTR_STATUS(gpio)) &
					BIT(INTR_STATUS_BIT);
}

void __msm_gpio_set_intr_status(unsigned gpio)
{
	__raw_writel(BIT(INTR_STATUS_BIT), GPIO_INTR_STATUS(gpio));
}

unsigned __msm_gpio_get_intr_config(unsigned gpio)
{
	return __raw_readl(GPIO_INTR_CFG(gpio));
}

void __msm_gpio_set_intr_cfg_enable(unsigned gpio, unsigned val)
{
	if (val) {
		set_gpio_bits(INTR_ENABLE, GPIO_INTR_CFG(gpio));

	} else {
		clr_gpio_bits(INTR_ENABLE, GPIO_INTR_CFG(gpio));
	}
}

void __msm_gpio_set_intr_cfg_type(unsigned gpio, unsigned type)
{
	unsigned cfg;

	/* RAW_STATUS_EN is left on for all gpio irqs. Due to the
	 * internal circuitry of TLMM, toggling the RAW_STATUS
	 * could cause the INTR_STATUS to be set for EDGE interrupts.
	 */
	cfg  = __msm_gpio_get_intr_config(gpio);
	cfg |= INTR_RAW_STATUS_EN;
	__raw_writel(cfg, GPIO_INTR_CFG(gpio));
	__raw_writel(TARGET_PROC_SCORPION, GPIO_INTR_CFG_SU(gpio));

	cfg  = __msm_gpio_get_intr_config(gpio);
	if (type & IRQ_TYPE_EDGE_BOTH)
		cfg |= INTR_DECT_CTL_EDGE;
	else
		cfg &= ~INTR_DECT_CTL_EDGE;

	if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_LEVEL_HIGH))
		cfg |= INTR_POL_CTL_HI;
	else
		cfg &= ~INTR_POL_CTL_HI;

	__raw_writel(cfg, GPIO_INTR_CFG(gpio));
	/* Sometimes it might take a little while to update
	 * the interrupt status after the RAW_STATUS is enabled
	 * We clear the interrupt status before enabling the
	 * interrupt in the unmask call-back.
	 */
	udelay(5);
}

void __gpio_tlmm_config(unsigned config)
{
	uint32_t flags;
	unsigned gpio = GPIO_PIN(config);

	flags = ((GPIO_DIR(config) << 9) & (0x1 << 9)) |
		((GPIO_DRVSTR(config) << 6) & (0x7 << 6)) |
		((GPIO_FUNC(config) << 2) & (0xf << 2)) |
		((GPIO_PULL(config) & 0x3));
	__raw_writel(flags, GPIO_CONFIG(gpio));
}

void __msm_gpio_install_direct_irq(unsigned gpio, unsigned irq,
					unsigned int input_polarity)
{
	uint32_t bits;

	__raw_writel(__raw_readl(GPIO_CONFIG(gpio)) | BIT(GPIO_OE_BIT),
		GPIO_CONFIG(gpio));
	__raw_writel(__raw_readl(GPIO_INTR_CFG(gpio)) &
		~(INTR_RAW_STATUS_EN | INTR_ENABLE),
		GPIO_INTR_CFG(gpio));
	__raw_writel(DC_IRQ_ENABLE | TARGET_PROC_NONE,
		GPIO_INTR_CFG_SU(gpio));

	bits = TARGET_PROC_SCORPION | (gpio << 3);
	if (input_polarity)
		bits |= DC_POLARITY_HI;
	__raw_writel(bits, DIR_CONN_INTR_CFG_SU(irq));
}
