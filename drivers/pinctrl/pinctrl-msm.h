/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __PINCTRL_MSM_H__
#define __PINCTRL_MSM_H__

#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/machine.h>
#include <linux/platform_device.h>

/**
 * struct msm_pin_group: group of pins having the same pinmux function.
 * @name: name of the pin group.
 * @pins: the pins included in this group.
 * @num_pins: number of pins included in this group.
 * @func: the function number to be programmed when selected.
 */
struct msm_pin_grps {
	const char *name;
	unsigned int *pins;
	unsigned num_pins;
	u32 func;
};

/**
 * struct msm_pmx_funcs: represent a pin function.
 * @name: name of the pin function.
 * @gps: one or more names of pin groups that provide this function.
 * @num_grps: number of groups included in @groups.
 */
struct msm_pmx_funcs {
	const char *name;
	const char **gps;
	unsigned num_grps;
};

/**
 * struct msm_tlmm_irq_chip: represents interrupt controller descriptor
 * @irq: irq number for tlmm summary interrupt.
 * @chip_base: base register for TLMM.
 * @num_irqs: number of pins that can be used as irq lines.
 * @apps_id: id assigned to the apps processor.
 * @enabled_irqs: bitmask of pins enabled as interrupts.
 * @wake_irqs: bitmask of pins enabled as wake up interrupts.
 * @irq_lock: protect against concurrent access.
 * @domain: irq domain of given interrupt controller
 * @irq_chip: irq chip operations.
 * @irq_chip_extn: extension of irq chip operations.
 * @dev: TLMM device.
 * @device_node: device tree node of interrupt controller.
 * @pinfo: pintype information.
 * @handler: irq handler for given pintype interrupt controller
 */
struct msm_tlmm_irq_chip {
	int irq;
	void *__iomem chip_base;
	unsigned int num_irqs;
	unsigned int apps_id;
	unsigned long *enabled_irqs;
	unsigned long *dual_edge_irqs;
	unsigned long *wake_irqs;
	spinlock_t irq_lock;
	struct irq_domain *domain;
	const struct irq_domain_ops *domain_ops;
	struct irq_chip chip;
	struct irq_chip *irq_chip_extn;
	struct device *dev;
	struct device_node *node;
	void *pinfo;
	irqreturn_t (*handler)(int irq, struct msm_tlmm_irq_chip *ic);
};

/**
 * struct msm_pintype_info: represent a pin type supported by the TLMM.
 * @prg_cfg: helper to program a given config for a pintype.
 * @prg_func: helper to program a given func for a pintype.
 * @pack_cfg: helper to pack a parsed config as per a pintype.
 * @set_reg_base: helper to set the register base address for a pintype.
 * @init_irq: helper to initialize any irq functionality.
 * @reg_data: register base for a pintype.
 * @prop_name: DT property name for a pintype.
 * @name: name of pintype.
 * @num_pins: number of pins of given pintype.
 * @pin_start: starting pin number for the given pintype within pinctroller.
 * @pin_end: ending pin number for the given pintype within pinctroller.
 * @gc: gpio chip implementation for pin type.
 * @irq_chip: interrupt controller support for given pintype.
 * @supports_gpio: pintype supports gpio function.
 * @grange: pins that map to gpios.
 * @node: device node for the pintype.
 */
struct msm_pintype_info {
	int (*prg_cfg)(uint pin_no, unsigned long *config, void *reg_data,
								bool rw);
	void (*prg_func)(uint pin_no, u32 func, void *reg_data, bool enable);
	void (*set_reg_base)(void __iomem **ptype_base,
						void __iomem *tlmm_base);
	int (*init_irq)(int irq, struct msm_pintype_info *pinfo,
						struct device *tlmm_dev);
	void __iomem *reg_base;
	const char *prop_name;
	const char *name;
	u32 num_pins;
	int pin_start;
	int pin_end;
	struct gpio_chip gc;
	struct msm_tlmm_irq_chip *irq_chip;
	bool supports_gpio;
	struct pinctrl_gpio_range grange;
	struct device_node *node;
};

/**
 * struct msm_tlmm_pintype: represents all the TLMM pintypes for a given TLMM
 * version.
 * @num_entries: number of pintypes.
 * @pintype_info: descriptor for the pintypes. One for each present.
 */
struct msm_tlmm_pintype {
	const uint num_entries;
	struct msm_pintype_info *pintype_info;
};

/**
 * struct msm_pindesc: descriptor for all pins maintained by pinctrl driver
 * @pin_info: pintype for a given pin.
 * @name: name of the pin.
 */
struct msm_pindesc {
	struct msm_pintype_info *pin_info;
	char name[20];
};

/**
 * struct msm_tlmm_desc: descriptor for the TLMM hardware block
 * @base: base address of tlmm desc.
 * @irq: summary irq number for tlmm block. Must be > 0 if present.
 * @num_pintypes: Number of pintypes on the tlmm block for a given SOC.
 * @pintypes: pintypes supported on a given TLMM block for a given SOC.
 */
struct msm_tlmm_desc {
	void __iomem *base;
	int irq;
	unsigned int num_pintypes;
	struct msm_pintype_info *pintypes;
};

/* Common probe for all TLMM */
int msm_pinctrl_probe(struct platform_device *pdev,
					struct msm_tlmm_desc *tlmm_info);
#endif

