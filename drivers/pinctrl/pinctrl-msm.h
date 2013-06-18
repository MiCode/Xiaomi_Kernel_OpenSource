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

#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/machine.h>

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
 * struct msm_pintype_info: represent a pin type supported by the TLMM.
 * @prg_cfg: helper to program a given config for a pintype.
 * @prg_func: helper to program a given func for a pintype.
 * @pack_cfg: helper to pack a parsed config as per a pintype.
 * @set_reg_base: helper to set the register base address for a pintype.
 * @reg_data: register base for a pintype.
 * @prop_name: DT property name for a pintype.
 * @name: name of pintype.
 * @num_pins: number of pins of given pintype.
 * @pin_start: starting pin number for the given pintype within pinctroller.
 * @pin_end: ending pin number for the given pintype within pinctroller.
 * @node: device node for the pintype.
 */
struct msm_pintype_info {
	int (*prg_cfg)(uint pin_no, unsigned long *config, void *reg_data,
								bool rw);
	void (*prg_func)(uint pin_no, u32 func, void *reg_data, bool enable);
	void (*set_reg_base)(void __iomem **ptype_base,
						void __iomem *tlmm_base);
	void __iomem *reg_base;
	const char *prop_name;
	const char *name;
	u32 num_pins;
	int pin_start;
	int pin_end;
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

/* TLMM version specific data */
extern struct msm_tlmm_pintype tlmm_v3_pintypes;
#endif

