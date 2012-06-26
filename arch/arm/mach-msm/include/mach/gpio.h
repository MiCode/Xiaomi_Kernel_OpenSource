/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
 * Author: Mike Lockwood <lockwood@android.com>
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
#ifndef __ASM_ARCH_MSM_GPIO_H
#define __ASM_ARCH_MSM_GPIO_H

#define ARCH_NR_GPIOS 512

#include <linux/interrupt.h>
#include <asm-generic/gpio.h>
#include <mach/irqs.h>

#define FIRST_BOARD_GPIO	NR_GPIO_IRQS

extern struct irq_chip msm_gpio_irq_extn;

/**
 * struct msm_gpio - GPIO pin description
 * @gpio_cfg - configuration bitmap, as per gpio_tlmm_config()
 * @label - textual label
 *
 * Usually, GPIO's are operated by sets.
 * This struct accumulate all GPIO information in single source
 * and facilitete group operations provided by msm_gpios_xxx()
 */
struct msm_gpio {
	u32 gpio_cfg;
	const char *label;
};

/**
 * msm_gpios_request_enable() - request and enable set of GPIOs
 *
 * Request and configure set of GPIO's
 * In case of error, all operations rolled back.
 * Return error code.
 *
 * @table: GPIO table
 * @size:  number of entries in @table
 */
int msm_gpios_request_enable(const struct msm_gpio *table, int size);

/**
 * msm_gpios_disable_free() - disable and free set of GPIOs
 *
 * @table: GPIO table
 * @size:  number of entries in @table
 */
void msm_gpios_disable_free(const struct msm_gpio *table, int size);

/**
 * msm_gpios_request() - request set of GPIOs
 * In case of error, all operations rolled back.
 * Return error code.
 *
 * @table: GPIO table
 * @size:  number of entries in @table
 */
int msm_gpios_request(const struct msm_gpio *table, int size);

/**
 * msm_gpios_free() - free set of GPIOs
 *
 * @table: GPIO table
 * @size:  number of entries in @table
 */
void msm_gpios_free(const struct msm_gpio *table, int size);

/**
 * msm_gpios_enable() - enable set of GPIOs
 * In case of error, all operations rolled back.
 * Return error code.
 *
 * @table: GPIO table
 * @size:  number of entries in @table
 */
int msm_gpios_enable(const struct msm_gpio *table, int size);

/**
 * msm_gpios_disable() - disable set of GPIOs
 *
 * @table: GPIO table
 * @size:  number of entries in @table
 */
int msm_gpios_disable(const struct msm_gpio *table, int size);

/**
 * msm_gpios_show_resume_irq() - show the interrupts that could have triggered
 * resume
 */
void msm_gpio_show_resume_irq(void);

/* GPIO TLMM (Top Level Multiplexing) Definitions */

/* GPIO TLMM: Function -- GPIO specific */

/* GPIO TLMM: Direction */
enum {
	GPIO_CFG_INPUT,
	GPIO_CFG_OUTPUT,
};

/* GPIO TLMM: Pullup/Pulldown */
enum {
	GPIO_CFG_NO_PULL,
	GPIO_CFG_PULL_DOWN,
	GPIO_CFG_KEEPER,
	GPIO_CFG_PULL_UP,
};

/* GPIO TLMM: Drive Strength */
enum {
	GPIO_CFG_2MA,
	GPIO_CFG_4MA,
	GPIO_CFG_6MA,
	GPIO_CFG_8MA,
	GPIO_CFG_10MA,
	GPIO_CFG_12MA,
	GPIO_CFG_14MA,
	GPIO_CFG_16MA,
};

enum {
	GPIO_CFG_ENABLE,
	GPIO_CFG_DISABLE,
};

#define GPIO_CFG(gpio, func, dir, pull, drvstr) \
	((((gpio) & 0x3FF) << 4)        |	  \
	 ((func) & 0xf)                  |	  \
	 (((dir) & 0x1) << 14)           |	  \
	 (((pull) & 0x3) << 15)          |	  \
	 (((drvstr) & 0xF) << 17))

/**
 * extract GPIO pin from bit-field used for gpio_tlmm_config
 */
#define GPIO_PIN(gpio_cfg)    (((gpio_cfg) >>  4) & 0x3ff)
#define GPIO_FUNC(gpio_cfg)   (((gpio_cfg) >>  0) & 0xf)
#define GPIO_DIR(gpio_cfg)    (((gpio_cfg) >> 14) & 0x1)
#define GPIO_PULL(gpio_cfg)   (((gpio_cfg) >> 15) & 0x3)
#define GPIO_DRVSTR(gpio_cfg) (((gpio_cfg) >> 17) & 0xf)

int gpio_tlmm_config(unsigned config, unsigned disable);

enum msm_tlmm_hdrive_tgt {
	TLMM_HDRV_SDC4_CLK = 0,
	TLMM_HDRV_SDC4_CMD,
	TLMM_HDRV_SDC4_DATA,
	TLMM_HDRV_SDC3_CLK,
	TLMM_HDRV_SDC3_CMD,
	TLMM_HDRV_SDC3_DATA,
	TLMM_HDRV_SDC2_CLK,
	TLMM_HDRV_SDC2_CMD,
	TLMM_HDRV_SDC2_DATA,
	TLMM_HDRV_SDC1_CLK,
	TLMM_HDRV_SDC1_CMD,
	TLMM_HDRV_SDC1_DATA,
};

enum msm_tlmm_pull_tgt {
	TLMM_PULL_SDC4_CLK = 0,
	TLMM_PULL_SDC4_CMD,
	TLMM_PULL_SDC4_DATA,
	TLMM_PULL_SDC3_CLK,
	TLMM_PULL_SDC3_CMD,
	TLMM_PULL_SDC3_DATA,
	TLMM_PULL_SDC2_CLK,
	TLMM_PULL_SDC2_CMD,
	TLMM_PULL_SDC2_DATA,
	TLMM_PULL_SDC1_CLK,
	TLMM_PULL_SDC1_CMD,
	TLMM_PULL_SDC1_DATA,
};

#if defined(CONFIG_GPIO_MSM_V2) || defined(CONFIG_GPIO_MSM_V3)
void msm_tlmm_set_hdrive(enum msm_tlmm_hdrive_tgt tgt, int drv_str);
void msm_tlmm_set_pull(enum msm_tlmm_pull_tgt tgt, int pull);

/*
 * A GPIO can be set as a direct-connect IRQ.  This can be used to bypass
 * the normal summary-interrupt mechanism for those GPIO lines deemed to be
 * higher priority or otherwise worthy of special treatment, but resources
 * are limited: only a few DC interrupt lines are available.
 * Care must be taken when usurping a GPIO in this manner, as the summary
 * interrupt controller has no idea that the GPIO has been taken away from it.
 * Clients can still register to receive the summary interrupt assigned
 * to that GPIO, which will uninstall it as a direct connect IRQ with
 * no warning.
 *
 * The irq passed to this function is the DC IRQ number, not the
 * irq number seen by the scorpion when the interrupt triggers.  For example,
 * if 0 is specified, then when DC IRQ 0 triggers, the scorpion will see
 * interrupt TLMM_MSM_DIR_CONN_IRQ_0.
 *
 * input_polarity parameter specifies when the gpio should raise the direct
 * interrupt. A value of 0 means that it is active low, anything else means
 * active high
 *
 */
int msm_gpio_install_direct_irq(unsigned gpio, unsigned irq,
						unsigned int input_polarity);
#else
static inline void msm_tlmm_set_hdrive(enum msm_tlmm_hdrive_tgt tgt,
				       int drv_str) {}
static inline void msm_tlmm_set_pull(enum msm_tlmm_pull_tgt tgt, int pull) {}
static inline int msm_gpio_install_direct_irq(unsigned gpio, unsigned irq,
						unsigned int input_polarity)
{
	return -ENOSYS;
}
#endif

#ifdef CONFIG_OF
int __init msm_gpio_of_init(struct device_node *node,
			    struct device_node *parent);
#endif

#endif /* __ASM_ARCH_MSM_GPIO_H */
