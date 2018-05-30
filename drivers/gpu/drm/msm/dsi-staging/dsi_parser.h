/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _DSI_PARSER_H_
#define _DSI_PARSER_H_

#include <linux/of.h>
#include <linux/of_gpio.h>

#ifdef CONFIG_DSI_PARSER
void *dsi_parser_get(struct device *dev);
void dsi_parser_put(void *data);
int dsi_parser_dbg_init(void *parser, struct dentry *dir);
void *dsi_parser_get_head_node(void *parser,
		const u8 *data, u32 size);

const void *dsi_parser_get_property(const struct device_node *np,
			const char *name, int *lenp);
bool dsi_parser_read_bool(const struct device_node *np,
			const char *propname);
int dsi_parser_read_u64(const struct device_node *np, const char *propname,
			 u64 *out_value);
int dsi_parser_read_u32(const struct device_node *np,
			const char *propname, u32 *out_value);
int dsi_parser_read_u32_array(const struct device_node *np,
			const char *propname,
			u32 *out_values, size_t sz);
int dsi_parser_read_string(const struct device_node *np,
			const char *propname, const char **out_string);
struct device_node *dsi_parser_get_child_by_name(const struct device_node *node,
				const char *name);
int dsi_parser_get_child_count(const struct device_node *np);
struct property *dsi_parser_find_property(const struct device_node *np,
			const char *name, int *lenp);
struct device_node *dsi_parser_get_next_child(const struct device_node *np,
	struct device_node *prev);
int dsi_parser_count_u32_elems(const struct device_node *np,
				const char *propname);
int dsi_parser_count_strings(const struct device_node *np,
			    const char *propname);
int dsi_parser_read_string_index(const struct device_node *np,
				const char *propname,
				int index, const char **output);
int dsi_parser_get_named_gpio(struct device_node *np,
				const char *propname, int index);
#else /* CONFIG_DSI_PARSER */
static inline void *dsi_parser_get(struct device *dev)
{
	return NULL;
}

static inline void dsi_parser_put(void *data)
{
}

static inline int dsi_parser_dbg_init(void *parser, struct dentry *dir)
{
	return -ENODEV;
}

static inline void *dsi_parser_get_head_node(void *parser,
		const u8 *data, u32 size)
{
	return NULL;
}

static inline const void *dsi_parser_get_property(const struct device_node *np,
			const char *name, int *lenp)
{
	return NULL;
}

static inline bool dsi_parser_read_bool(const struct device_node *np,
			const char *propname)
{
	return false;
}

static inline int dsi_parser_read_u64(const struct device_node *np,
			const char *propname, u64 *out_value)
{
	return -ENODEV;
}

static inline int dsi_parser_read_u32(const struct device_node *np,
			const char *propname, u32 *out_value)
{
	return -ENODEV;
}

static inline int dsi_parser_read_u32_array(const struct device_node *np,
			const char *propname, u32 *out_values, size_t sz)
{
	return -ENODEV;
}

static inline int dsi_parser_read_string(const struct device_node *np,
			const char *propname, const char **out_string)
{
	return -ENODEV;
}

static inline struct device_node *dsi_parser_get_child_by_name(
				const struct device_node *node,
				const char *name)
{
	return NULL;
}

static inline int dsi_parser_get_child_count(const struct device_node *np)
{
	return -ENODEV;
}

static inline struct property *dsi_parser_find_property(
			const struct device_node *np,
			const char *name, int *lenp)
{
	return NULL;
}

static inline struct device_node *dsi_parser_get_next_child(
				const struct device_node *np,
				struct device_node *prev)
{
	return NULL;
}

static inline int dsi_parser_count_u32_elems(const struct device_node *np,
				const char *propname)
{
	return -ENODEV;
}

static inline int dsi_parser_count_strings(const struct device_node *np,
			    const char *propname)
{
	return -ENODEV;
}

static inline int dsi_parser_read_string_index(const struct device_node *np,
				const char *propname,
				int index, const char **output)
{
	return -ENODEV;
}

static inline int dsi_parser_get_named_gpio(struct device_node *np,
				const char *propname, int index)
{
	return -ENODEV;
}

#endif /* CONFIG_DSI_PARSER */

#define dsi_for_each_child_node(parent, child) \
	for (child = utils->get_next_child(parent, NULL); \
	     child != NULL; \
	     child = utils->get_next_child(parent, child))

struct dsi_parser_utils {
	void *data;
	struct device_node *node;

	const void *(*get_property)(const struct device_node *np,
			const char *name, int *lenp);
	int (*read_u64)(const struct device_node *np,
			const char *propname, u64 *out_value);
	int (*read_u32)(const struct device_node *np,
			const char *propname, u32 *out_value);
	bool (*read_bool)(const struct device_node *np,
			 const char *propname);
	int (*read_u32_array)(const struct device_node *np,
			const char *propname, u32 *out_values, size_t sz);
	int (*read_string)(const struct device_node *np, const char *propname,
				const char **out_string);
	struct device_node *(*get_child_by_name)(
				const struct device_node *node,
				const char *name);
	int (*get_child_count)(const struct device_node *np);
	struct property *(*find_property)(const struct device_node *np,
			const char *name, int *lenp);
	struct device_node *(*get_next_child)(const struct device_node *np,
		struct device_node *prev);
	int (*count_u32_elems)(const struct device_node *np,
		const char *propname);
	int (*get_named_gpio)(struct device_node *np,
				const char *propname, int index);
	int (*get_available_child_count)(const struct device_node *np);
};

static inline struct dsi_parser_utils *dsi_parser_get_of_utils(void)
{
	static struct dsi_parser_utils of_utils = {
		.get_property = of_get_property,
		.read_bool = of_property_read_bool,
		.read_u64 = of_property_read_u64,
		.read_u32 = of_property_read_u32,
		.read_u32_array = of_property_read_u32_array,
		.read_string = of_property_read_string,
		.get_child_by_name = of_get_child_by_name,
		.get_child_count = of_get_child_count,
		.get_available_child_count = of_get_available_child_count,
		.find_property = of_find_property,
		.get_next_child = of_get_next_child,
		.count_u32_elems = of_property_count_u32_elems,
		.get_named_gpio = of_get_named_gpio,
	};

	return &of_utils;
}

static inline struct dsi_parser_utils *dsi_parser_get_parser_utils(void)
{
	static struct dsi_parser_utils parser_utils = {
		.get_property = dsi_parser_get_property,
		.read_bool = dsi_parser_read_bool,
		.read_u64 = dsi_parser_read_u64,
		.read_u32 = dsi_parser_read_u32,
		.read_u32_array = dsi_parser_read_u32_array,
		.read_string = dsi_parser_read_string,
		.get_child_by_name = dsi_parser_get_child_by_name,
		.get_child_count = dsi_parser_get_child_count,
		.get_available_child_count = dsi_parser_get_child_count,
		.find_property = dsi_parser_find_property,
		.get_next_child = dsi_parser_get_next_child,
		.count_u32_elems = dsi_parser_count_u32_elems,
		.get_named_gpio = dsi_parser_get_named_gpio,
	};

	return &parser_utils;
}
#endif
