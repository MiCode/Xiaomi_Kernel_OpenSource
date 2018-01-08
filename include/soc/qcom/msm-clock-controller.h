/*
 * Copyright (c) 2014, 2017, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MSM_CLOCK_CONTROLLER_H
#define __ARCH_ARM_MSM_CLOCK_CONTROLLER_H

#include <linux/list.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define dt_err(np, fmt, ...) \
	pr_err("%s: " fmt, np->name, ##__VA_ARGS__)
#define dt_prop_err(np, str, fmt, ...) \
	dt_err(np, "%s: " fmt, str, ##__VA_ARGS__)

/**
 * struct msmclk_parser
 * @compatible
 *      matches compatible property from devicetree
 * @parsedt
 *      constructs & returns an instance of the appropriate obj based on
 *      the data from devicetree.
 */
struct msmclk_parser {
	struct list_head list;
	char *compatible;
	void * (*parsedt)(struct device *dev, struct device_node *of);
};

#define MSMCLK_PARSER(fn, str, id) \
static struct msmclk_parser _msmclk_##fn##id = {		\
	.list = LIST_HEAD_INIT(_msmclk_##fn##id.list),		\
	.compatible = str,					\
	.parsedt = fn,						\
};								\
static int __init _msmclk_init_##fn##id(void)			\
{								\
	msmclk_parser_register(&_msmclk_##fn##id);		\
	return 0;						\
}								\
early_initcall(_msmclk_init_##fn##id)

/*
 * struct msmclk_data
 * @base
 *      ioremapped region for sub_devices
 * @list
 *	tracks all registered driver instances
 * @htable
 *	tracks all registered child clocks
 * @clk_tbl
 *      array of clk_lookup to be registered with the clock framework
 */
#define HASHTABLE_SIZE 200
struct msmclk_data {
	void __iomem *base;
	struct device *dev;
	struct list_head list;
	struct hlist_head htable[HASHTABLE_SIZE];
	struct clk_lookup *clk_tbl;
	int clk_tbl_size;
	int max_clk_tbl_size;
};

#if defined(CONFIG_MSM_CLK_CONTROLLER_V2)

/* Utility functions */
int of_property_count_phandles(struct device_node *np, char *propname);
int of_property_read_phandle_index(struct device_node *np, char *propname,
					int index, phandle *p);
void *msmclk_generic_clk_init(struct device *dev, struct device_node *np,
				struct clk *c);

/*
 * msmclk_parser_register
 *      Registers a parser which will be matched with a node from dt
 *      according to the compatible string.
 */
void msmclk_parser_register(struct msmclk_parser *p);

/*
 * msmclk_parse_phandle
 *      On hashtable miss, the corresponding entry will be retrieved from
 *      devicetree, and added to the hashtable.
 */
void *msmclk_parse_phandle(struct device *dev, phandle key);
/*
 * msmclk_lookup_phandle
 *	Straightforward hashtable lookup
 */
void *msmclk_lookup_phandle(struct device *dev, phandle key);

int __init msmclk_init(void);
#else

static inline int of_property_count_phandles(struct device_node *np,
			char *propname)
{
	return 0;
}

static inline int of_property_read_phandle_index(struct device_node *np,
			char *propname, int index, phandle *p)
{
	return 0;
}

static inline void *msmclk_generic_clk_init(struct device *dev,
				struct device_node *np, struct clk *c)
{
	return ERR_PTR(-EINVAL);
}

static inline void msmclk_parser_register(struct msmclk_parser *p) {};

static inline void *msmclk_parse_phandle(struct device *dev, phandle key)
{
	return ERR_PTR(-EINVAL);
}

static inline void *msmclk_lookup_phandle(struct device *dev, phandle key)
{
	return ERR_PTR(-EINVAL);
}

static inline int __init msmclk_init(void)
{
	return 0;
}

#endif /* CONFIG_MSM_CLK_CONTROLLER_V2 */
#endif /* __ARCH_ARM_MSM_CLOCK_CONTROLLER_H */
