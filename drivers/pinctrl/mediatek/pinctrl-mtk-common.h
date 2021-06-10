/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Hongzhou.Yang <hongzhou.yang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PINCTRL_MTK_COMMON_H
#define __PINCTRL_MTK_COMMON_H

#include <linux/pinctrl/pinctrl.h>
#include <linux/regmap.h>
#include <linux/pinctrl/pinconf-generic.h>

#define NO_EINT_SUPPORT    255
#define MT_EDGE_SENSITIVE           0
#define MT_LEVEL_SENSITIVE          1
#define EINT_DBNC_SET_DBNC_BITS     4
#define EINT_DBNC_RST_BIT           (0x1 << 1)
#define EINT_DBNC_SET_EN            (0x1 << 0)
#define MAX_IP_BASE                     10
#define MTK_PINCTRL_NOT_SUPPORT	(0xffff)

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
#define MTK_PUPD_BIT_PD             (1 << 0)
#define MTK_PUPD_BIT_PU             (1 << 1)
#define MTK_PUPD_R1R0_BIT_PUPD      (1 << 0)
#define MTK_PUPD_R1R0_BIT_R0        (1 << 1)
#define MTK_PUPD_R1R0_BIT_R1        (1 << 2)
#define MTK_PUPD_R1R0_BIT_SUPPORT   (1 << 3)
#define MTK_PUPD_R1R0_GET_PUPD(val)     (val & MTK_PUPD_R1R0_BIT_PUPD)
#define MTK_PUPD_R1R0_GET_PULLEN(val)	\
		((val & (MTK_PUPD_R1R0_BIT_R0 | MTK_PUPD_R1R0_BIT_R1)) >> 1)
#define MTK_PUPD_R1R0_GET_SUPPORT(val)	\
		(val & MTK_PUPD_R1R0_BIT_SUPPORT)

enum {
	GPIO_PULL_UNSUPPORTED = -1,
	GPIO_PULL_DOWN = 0,
	GPIO_PULL_UP = 1,
	GPIO_NO_PULL = 2,
};

enum {
	GPIO_PULL_EN_UNSUPPORTED = -1,
	GPIO_NOPULLUP           = -4,
	GPIO_NOPULLDOWN         = -5,
	GPIO_PULL_DISABLE = 0,
	GPIO_PULL_ENABLE  = 1,
	GPIO_PULL_ENABLE_R0 = 2,
	GPIO_PULL_ENABLE_R1 = 3,
	GPIO_PULL_ENABLE_R0R1 = 4,
	GPIO_PULL_EN_MAX,
	GPIO_PULL_EN_DEFAULT = GPIO_PULL_ENABLE,
};
#endif

struct mtk_desc_function {
	const char *name;
	unsigned char muxval;
};

struct mtk_desc_eint {
	unsigned char eintmux;
	unsigned char eintnum;
};

struct mtk_desc_pin {
	struct pinctrl_pin_desc	pin;
#ifdef CONFIG_MTK_EINT_MULTI_TRIGGER_DESIGN
	struct mtk_desc_eint eint;
#else
	const struct mtk_desc_eint eint;
#endif
	const struct mtk_desc_function	*functions;
};

#define MTK_PIN(_pin, _pad, _chip, _eint, ...)		\
	{							\
		.pin = _pin,					\
		.eint = _eint,					\
		.functions = (struct mtk_desc_function[]){	\
			__VA_ARGS__, { } },			\
	}

#define MTK_EINT_FUNCTION(_eintmux, _eintnum)				\
	{							\
		.eintmux = _eintmux,					\
		.eintnum = _eintnum,					\
	}

#define MTK_FUNCTION(_val, _name)				\
	{							\
		.muxval = _val,					\
		.name = _name,					\
	}

#define SET_ADDR(x, y)  (x + (y->devdata->port_align))
#define CLR_ADDR(x, y)  (x + (y->devdata->port_align << 1))

struct mtk_pinctrl_group {
	const char	*name;
	unsigned long	config;
	unsigned	pin;
};

/**
 * struct mtk_drv_group_desc - Provide driving group data.
 * @max_drv: The maximum current of this group.
 * @min_drv: The minimum current of this group.
 * @low_bit: The lowest bit of this group.
 * @high_bit: The highest bit of this group.
 * @step: The step current of this group.
 */
struct mtk_drv_group_desc {
	unsigned char min_drv;
	unsigned char max_drv;
	unsigned char low_bit;
	unsigned char high_bit;
	unsigned char step;
};

#define MTK_DRV_GRP(_min, _max, _low, _high, _step)	\
	{	\
		.min_drv = _min,	\
		.max_drv = _max,	\
		.low_bit = _low,	\
		.high_bit = _high,	\
		.step = _step,		\
	}

/**
 * struct mtk_pin_drv_grp - Provide each pin driving info.
 * @pin: The pin number.
 * @offset: The offset of driving register for this pin.
 * @bit: The bit of driving register for this pin.
 * @grp: The group for this pin belongs to.
 */
struct mtk_pin_drv_grp {
	unsigned short pin;
	unsigned short offset;
	unsigned char bit;
	unsigned char grp;
};

#define MTK_PIN_DRV_GRP(_pin, _offset, _bit, _grp)	\
	{	\
		.pin = _pin,	\
		.offset = _offset,	\
		.bit = _bit,	\
		.grp = _grp,	\
	}

/**
 * struct mtk_pin_spec_pupd_set_samereg
 * - For special pins' pull up/down setting which resides in same register
 * @pin: The pin number.
 * @offset: The offset of special pull up/down setting register.
 * @pupd_bit: The pull up/down bit in this register.
 * @r0_bit: The r0 bit of pull resistor.
 * @r1_bit: The r1 bit of pull resistor.
 */
struct mtk_pin_spec_pupd_set_samereg {
	unsigned short pin;
	unsigned short offset;
	unsigned char pupd_bit;
	unsigned char r1_bit;
	unsigned char r0_bit;
	unsigned char ip_num;
};

#define MTK_PIN_PUPD_SPEC_SR(_pin, _offset, _pupd, _r1, _r0, _ip_num)	\
	{	\
		.pin = _pin,	\
		.offset = _offset,	\
		.pupd_bit = _pupd,	\
		.r1_bit = _r1,		\
		.r0_bit = _r0,		\
		.ip_num = _ip_num,\
	}

/**
 * struct mtk_pin_ies_set - For special pins' ies and smt setting.
 * @start: The start pin number of those special pins.
 * @end: The end pin number of those special pins.
 * @offset: The offset of special setting register.
 * @bit: The bit of special setting register.
 */
struct mtk_pin_ies_smt_set {
	unsigned short start;
	unsigned short end;
	unsigned short offset;
	unsigned char bit;
};

#define MTK_PIN_IES_SMT_SPEC(_start, _end, _offset, _bit)	\
	{	\
		.start = _start,	\
		.end = _end,	\
		.bit = _bit,	\
		.offset = _offset,	\
	}

struct mtk_eint_offsets {
	const char *name;
	unsigned int  stat;
	unsigned int  ack;
	unsigned int  mask;
	unsigned int  mask_set;
	unsigned int  mask_clr;
	unsigned int  sens;
	unsigned int  sens_set;
	unsigned int  sens_clr;
	unsigned int  soft;
	unsigned int  soft_set;
	unsigned int  soft_clr;
	unsigned int  pol;
	unsigned int  pol_set;
	unsigned int  pol_clr;
	unsigned int  dom_en;
	unsigned int  dbnc_ctrl;
	unsigned int  dbnc_set;
	unsigned int  dbnc_clr;
	u8  port_mask;
	u8  ports;
};

/**
 * struct mt_pin_info - For all pins' setting
 * @pin: The pin number
 * @offset: The address offset of pin setting register
 * @bit: The bit shift at setting register
 * @width: The bit width at setting register
 * @ip_num: The IOConfiguration base index
 */
struct mtk_pin_info {
	unsigned int pin;
	unsigned int offset;
	unsigned char bit;
	unsigned char width;
	unsigned char ip_num;
};

#define MTK_PIN_INFO(_pin, _offset, _bit, _width, _ip_num)	\
	{	\
		.pin = _pin,	\
		.offset = _offset,	\
		.bit = _bit, \
		.width = _width, \
		.ip_num = _ip_num,\
	}

struct mtk_pinctrl {
	struct regmap	*regmap1;
	struct regmap	*regmap2;
	struct regmap	        *regmap[MAX_IP_BASE];
	struct pinctrl_desc pctl_desc;
	struct device           *dev;
	struct gpio_chip	*chip;
	struct mtk_pinctrl_group	*groups;
	struct timer_list	*eint_timers;
	unsigned int		ngroups;
	const char          **grp_names;
	struct pinctrl_dev      *pctl_dev;
	const struct mtk_pinctrl_devdata  *devdata;
	void __iomem		*eint_reg_base;
	struct irq_domain	*domain;
	int			*eint_dual_edges;
	u32 *wake_mask;
	u32 *cur_mask;
	int		*eint_sw_debounce_en;
	u32		*eint_sw_debounce;
#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	int		dbg_start;
#endif
};

/**
 * struct mtk_pinctrl_devdata - Provide HW GPIO related data.
 * @pins: An array describing all pins the pin controller affects.
 * @npins: The number of entries in @pins.
 *
 * @grp_desc: The driving group info.
 * @pin_drv_grp: The driving group for all pins.
 * @spec_pull_set: Each SoC may have special pins for pull up/down setting,
 *  these pins' pull setting are very different, they have separate pull
 *  up/down bit, R0 and R1 resistor bit, so they need special pull setting.
 *  If special setting is success, this should return 0, otherwise it should
 *  return non-zero value.
 * @spec_ies_smt_set: Some pins are irregular, their input enable and smt
 * control register are discontinuous, but they are mapping together. That
 * means when user set smt, input enable is set at the same time. So they
 * also need special control. If special control is success, this should
 * return 0, otherwise return non-zero value.
 * @spec_pinmux_set: In some cases, there are two pinmux functions share
 * the same value in the same segment of pinmux control register. If user
 * want to use one of the two functions, they need an extra bit setting to
 * select the right one.
 * @spec_dir_set: In very few SoCs, direction control registers are not
 * arranged continuously, they may be cut to parts. So they need special
 * dir setting.

 * @dir_offset: The direction register offset.
 * @pullen_offset: The pull-up/pull-down enable register offset.
 * @pinmux_offset: The pinmux register offset.
 *
 * @type1_start: Some chips have two base addresses for pull select register,
 *  that means some pins use the first address and others use the second. This
 *  member record the start of pin number to use the second address.
 * @type1_end: The end of pin number to use the second address.
 *
 * @port_shf: The shift between two registers.
 * @port_mask: The mask of register.
 * @port_align: Provide clear register and set register step.
 * @regmap_num:chip regmap number.
 */
struct mtk_pinctrl_devdata {
#ifdef CONFIG_MTK_EINT_MULTI_TRIGGER_DESIGN
	struct mtk_desc_pin		*pins;
#else
	const struct mtk_desc_pin	*pins;
#endif
	unsigned int				npins;
	const struct mtk_drv_group_desc	*grp_desc;
	unsigned int	n_grp_cls;
	const struct mtk_pin_drv_grp	*pin_drv_grp;
	unsigned int	n_pin_drv_grps;
	const struct mtk_pin_info	*pin_drv_grps;
	unsigned int	                n_pin_drv;
	const struct mtk_pin_info	*pin_mode_grps;
	unsigned int	                n_pin_mode;
	const struct mtk_pin_info	*pin_ies_grps;
	unsigned int	                n_pin_ies;
	const struct mtk_pin_info	*pin_smt_grps;
	unsigned int	                n_pin_smt;
	const struct mtk_pin_info	*pin_dout_grps;
	unsigned int	                n_pin_dout;
	const struct mtk_pin_info	*pin_din_grps;
	unsigned int	                n_pin_din;
	const struct mtk_pin_info	*pin_dir_grps;
	unsigned int	                n_pin_dir;
	const struct mtk_pin_info	*pin_pullen_grps;
	unsigned int	                n_pin_pullen;
	const struct mtk_pin_info	*pin_pullsel_grps;
	unsigned int	                n_pin_pullsel;
	const struct mtk_pin_info	*pin_pupd_r1r0_grps;
	unsigned int	                n_pin_pupd_r1r0;
#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	const struct mtk_pin_info		*pin_pu_grps;
	unsigned int			n_pin_pu;
	const struct mtk_pin_info		*pin_pd_grps;
	unsigned int			n_pin_pd;
	const struct mtk_pin_info		*pin_pupd_grps;
	unsigned int			n_pin_pupd;
	const struct mtk_pin_info		*pin_r0_grps;
	unsigned int			n_pin_r0;
	const struct mtk_pin_info		*pin_r1_grps;
	unsigned int			n_pin_r1;
	const struct mtk_pin_info		*pin_drve4_grps;
	unsigned int			n_pin_drve4;
	const struct mtk_pin_info		*pin_drve8_grps;
	unsigned int			n_pin_drve8;
#endif
	int (*spec_pull_set)(struct mtk_pinctrl *pctl,
			struct regmap *reg, unsigned int pin,
			unsigned char align, bool isup, unsigned int arg);
	int (*spec_ies_smt_set)(struct mtk_pinctrl *pctl,
			struct regmap *reg, unsigned int pin,
			unsigned char align, int value, enum pin_config_param arg);
	void (*spec_pinmux_set)(struct regmap *reg, unsigned int pin,
			unsigned int mode);
	void (*spec_dir_set)(unsigned int *reg_addr, unsigned int pin);
	int (*spec_dir_get)(struct mtk_pinctrl *pctl, unsigned int *reg_addr,
			unsigned int pin, unsigned int *reg_val);
	unsigned int (*spec_debounce_select)(unsigned int debounce);
#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	int (*spec_pull_get)(struct regmap *reg, unsigned int pin);
	int (*spec_ies_get)(struct regmap *reg, unsigned int pin);
	int (*spec_smt_get)(struct regmap *reg, unsigned int pin);
	int (*mtk_pctl_set_pull_sel)(struct mtk_pinctrl *pctl,
		unsigned int pin, bool enable, bool isup, unsigned int arg);
	int (*mtk_pctl_get_pull_sel)(struct mtk_pinctrl *pctl,
		unsigned int pin);
	int (*mtk_pctl_set_gpio_drv)(struct mtk_pinctrl *pctl,
		unsigned int pin, unsigned char drv);
	int (*mtk_pctl_get_gpio_drv)(struct mtk_pinctrl *pctl,
		unsigned int pin);
	int (*mtk_pctl_get_pull_en)(struct mtk_pinctrl *pctl, unsigned int pin);
#endif
	unsigned int dir_offset;
	unsigned int ies_offset;
	unsigned int smt_offset;
	unsigned int pullen_offset;
	unsigned int pullsel_offset;
	unsigned int drv_offset;
	unsigned int dout_offset;
	unsigned int din_offset;
	unsigned int pinmux_offset;
	unsigned short type1_start;
	unsigned short type1_end;
	unsigned char  port_shf;
	unsigned char  port_mask;
	unsigned char  port_align;
	unsigned char  port_pin_shf;
	unsigned int    regmap_num;
	struct mtk_eint_offsets eint_offsets;
	unsigned int	ap_num;
	unsigned int	db_cnt;
#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	const struct irq_domain_ops *mtk_irq_domain_ops;
#endif
};

int mtk_pctrl_init(struct platform_device *pdev,
		const struct mtk_pinctrl_devdata *data,
		struct regmap *regmap);

int mtk_pctrl_spec_pull_set_samereg(struct mtk_pinctrl *pctl,
		struct regmap *regmap,
		const struct mtk_pin_spec_pupd_set_samereg *pupd_infos,
		unsigned int info_num, unsigned int pin,
		unsigned char align, bool isup, unsigned int r1r0);

int mtk_spec_pull_get_samereg(struct regmap *regmap,
		const struct mtk_pin_spec_pupd_set_samereg *pupd_infos,
		unsigned int info_num, unsigned int pin);

int mtk_pconf_spec_set_ies_smt_range(struct regmap *regmap,
		const struct mtk_pin_ies_smt_set *ies_smt_infos, unsigned int info_num,
		unsigned int pin, unsigned char align, int value);

int mtk_pinctrl_get_gpio_value(struct mtk_pinctrl *pctl,
	int pin, int size, const struct mtk_pin_info pin_info[]);

int mtk_pinctrl_update_gpio_value(struct mtk_pinctrl *pctl, int pin,
	unsigned char value, int size, const struct mtk_pin_info pin_info[]);

int mtk_pinctrl_set_gpio_value(struct mtk_pinctrl *pctl, int pin,
	bool value, int size, const struct mtk_pin_info pin_info[]);

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
int mtk_pctrl_get_gpio_chip_base(void);

extern struct mtk_pinctrl *pctl_alt;
#endif

unsigned int mtk_gpio_debounce_select(const unsigned int *dbnc_infos,
	int dbnc_infos_num, unsigned int debounce);

extern const struct dev_pm_ops mtk_eint_pm_ops;

#endif /* __PINCTRL_MTK_COMMON_H */
