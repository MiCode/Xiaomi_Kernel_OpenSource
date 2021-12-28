// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#include "dsi_parser.h"
#include "dsi_defs.h"

#define DSI_PARSER_MAX_NODES 20

enum dsi_parser_prop_type {
	DSI_PROP_TYPE_STR,
	DSI_PROP_TYPE_STR_ARRAY,
	DSI_PROP_TYPE_INT_SET,
	DSI_PROP_TYPE_INT_SET_ARRAY,
	DSI_PROP_TYPE_INT_ARRAY,
};

struct dsi_parser_prop {
	char *name;
	char *raw;
	char *value;
	char **items;
	enum dsi_parser_prop_type type;
	int len;
};

struct dsi_parser_node {
	char *name;
	char *data;

	struct dsi_parser_prop *prop;
	int prop_count;

	struct dsi_parser_node *child[DSI_PARSER_MAX_NODES];
	int children_count;
};

struct dsi_parser {
	const struct firmware *fw;
	struct dsi_parser_node *head_node;
	struct dsi_parser_node *current_node;
	struct device *dev;
	char *buf;
	char file_name[SZ_32];
};

static int dsi_parser_count(char *buf, int item)
{
	int count = 0;

	do {
		buf = strnchr(buf, strlen(buf), item);
		if (buf)
			count++;
	} while (buf++);

	return count;
}

static char *dsi_parser_clear_tail(char *buf)
{
	int size = strlen(buf);
	char *end;

	if (!size)
		goto exit;

	end = buf + size - 1;
	while (end >= buf && *end == '*')
		end--;

	*(end + 1) = '\0';
exit:
	return buf;
}

static char *dsi_parser_strim(char *buf)
{
	strreplace(buf, '*', ' ');

	return strim(buf);
}

static char *dsi_parser_get_data(char *start, char *end, char *str)
{
	strsep(&str, start);
	if (str)
		return dsi_parser_clear_tail(strsep(&str, end));

	return NULL;
}

static bool dsi_parser_get_tuples_data(
		struct dsi_parser_prop *prop, char *str)
{
	bool middle_of_tx = false;

	if (!str) {
		DSI_ERR("Invalid input\n");
		return middle_of_tx;
	}

	while (str) {
		char *out = strsep(&str, " ");

		if (str || middle_of_tx) {
			middle_of_tx = true;

			prop->items[prop->len++] = dsi_parser_strim(out);
		}
	}

	return middle_of_tx;
}

static bool dsi_parser_get_strings(struct device *dev,
			struct dsi_parser_prop *prop, char *str)
{
	bool middle_of_tx = false;
	int i = 0;
	int count = 0;

	if (!str) {
		DSI_ERR("Invalid input\n");
		goto end;
	}

	if (!dsi_parser_count(str, '"'))
		goto end;

	count = dsi_parser_count(str, ',');
	DSI_DEBUG("count=%d\n", count);

	if (!count) {
		prop->value = dsi_parser_get_data("\"", "\"", str);
		prop->type = DSI_PROP_TYPE_STR;
		middle_of_tx = prop->value ? true : false;

		goto end;
	}

	/* number of items are 1 more than separator */
	count++;
	prop->items = devm_kzalloc(dev, count, GFP_KERNEL);
	if (!prop->items)
		goto end;

	prop->type = DSI_PROP_TYPE_STR_ARRAY;

	while (str) {
		char *out = strsep(&str, ",");

		if ((str || middle_of_tx) && (i < count)) {
			prop->items[i++] =
				dsi_parser_get_data("\"", "\"", out);
			prop->len++;

			middle_of_tx = true;
		}
	}
end:
	return middle_of_tx;
}

static bool dsi_parser_get_tuples(struct device *dev,
			struct dsi_parser_prop *prop, char *str)
{
	bool middle_of_tx = false;
	char *data = NULL;

	if (!str) {
		DSI_ERR("Invalid input\n");
		return middle_of_tx;
	}

	while (str) {
		char *out = strsep(&str, ",");

		if (str || middle_of_tx) {
			data = dsi_parser_get_data("<", ">", out);
			middle_of_tx = true;

			dsi_parser_get_tuples_data(prop, data);
		}
	}

	return middle_of_tx;
}

static void dsi_parser_get_int_value(struct dsi_parser_prop *prop,
					int forced_base)
{
	int i;

	for (i = 0; i < prop->len; i++) {
		int base, val;
		char *to_int, *tmp;
		char item[SZ_128];

		strlcpy(item, prop->items[i], SZ_128);

		tmp = item;

		if (forced_base) {
			base = forced_base;
		} else {
			to_int = strsep(&tmp, "x");

			if (!tmp) {
				tmp = to_int;
				base = 10;
			} else {
				base = 16;
			}
		}

		if (kstrtoint(tmp, base, &val)) {
			DSI_ERR("error converting %s at %d\n",
				tmp, i);

			continue;
		}

		prop->value[i] = val & 0xFF;
	}
}

static bool dsi_parser_parse_prop(struct device *dev,
		struct dsi_parser_prop *prop, char *buf)
{
	bool found = false;
	char *out = strsep(&buf, "=");
	size_t buf_len;

	if (!out || !buf)
		goto end;

	buf_len = strlen(buf);

	prop->raw = devm_kzalloc(dev, buf_len + 1, GFP_KERNEL);
	if (!prop->raw)
		goto end;

	strlcpy(prop->raw, buf, buf_len + 1);

	found = true;

	prop->name = dsi_parser_strim(out);
	DSI_DEBUG("RAW: %s: %s\n", prop->name, prop->raw);

	prop->len = 0;

	if (dsi_parser_get_strings(dev, prop, buf))
		goto end;

	prop->items = devm_kzalloc(dev, strlen(buf) * 2, GFP_KERNEL);
	if (!prop->items)
		goto end;

	if (dsi_parser_get_tuples(dev, prop, buf)) {
		prop->value = devm_kzalloc(dev, prop->len, GFP_KERNEL);
		if (prop->value) {
			prop->type = DSI_PROP_TYPE_INT_SET_ARRAY;
			dsi_parser_get_int_value(prop, 0);
		}
		goto end;
	}

	prop->value = dsi_parser_get_data("<", ">", buf);
	if (prop->value) {
		if (dsi_parser_get_tuples_data(prop, prop->value)) {
			prop->value = devm_kzalloc(dev, prop->len, GFP_KERNEL);
			if (prop->value) {
				prop->type = DSI_PROP_TYPE_INT_SET;
				dsi_parser_get_int_value(prop, 0);
			}
			goto end;
		} else {
			prop->items[prop->len++] = prop->value;
		}

		goto end;
	}

	prop->value = dsi_parser_get_data("[", "]", buf);
	if (prop->value) {
		char *out5;

		if (!prop->items)
			goto end;

		out5 = prop->value;
		while (out5 && strlen(out5)) {
			char *out6 = strsep(&out5, " ");

			out6 = dsi_parser_strim(out6);
			if (out6 && strlen(out6))
				prop->items[prop->len++] = out6;
		}

		prop->value = devm_kzalloc(dev, prop->len, GFP_KERNEL);
		if (prop->value) {
			prop->type = DSI_PROP_TYPE_INT_ARRAY;

			dsi_parser_get_int_value(prop, 16);
		}
	} else {
		found = false;
	}
end:
	return found;
}

static char *dsi_parser_clean_name(char *name)
{
	char *clean_name = name;

	if (!name) {
		DSI_ERR("Invalid input\n");
		return NULL;
	}

	while (name)
		clean_name = strsep(&name, ";");

	return dsi_parser_strim(clean_name);
}

static char *dsi_parser_get_blob(char **buf, bool *has_child)
{
	char *data = NULL;
	char *start = *buf;

	data = strpbrk(*buf, "{}");
	if (!data)
		goto end;

	if (*data == '{')
		*has_child = true;

	if (*has_child) {
		while (data != *buf) {
			data--;
			if (*data == ';') {
				data++;
				*data = '\0';
				*buf = data + 1;
				break;
			}
		}
	} else {
		*data = '\0';
		*buf = data + 1;
	}
end:
	return start;
}

static struct dsi_parser_node *dsi_parser_find_nodes(struct device *dev,
							char **buf)
{
	struct dsi_parser_node *node = NULL, *cnode = NULL;
	char *name, *data;
	bool has_child = false;

	if (!buf || !*buf)
		goto end;

	data = strpbrk(*buf, "{}");
	if (!data) {
		DSI_DEBUG("{} not found\n");
		goto end;
	}

	if (*data == '}') {
		*buf = data + 1;
		goto end;
	}

	name = strsep(buf, "{");

	if (*buf && name) {
		node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
		if (!node)
			goto end;

		node->name = dsi_parser_clean_name(name);
		node->data = dsi_parser_get_blob(buf, &has_child);

		if (!has_child)
			goto end;

		do {
			cnode = dsi_parser_find_nodes(dev, buf);
			if (cnode &&
			    (node->children_count < DSI_PARSER_MAX_NODES))
				node->child[node->children_count++] = cnode;
		} while (cnode);
	}
end:
	return node;
}

static void dsi_parser_count_properties(struct dsi_parser_node *node)
{
	int count;

	if (node && strlen(node->data)) {
		node->prop_count = dsi_parser_count(node->data, ';');

		for (count = 0; count < node->children_count; count++)
			dsi_parser_count_properties(node->child[count]);
	}
}

static void dsi_parser_get_properties(struct device *dev,
		struct dsi_parser_node *node)
{
	int count;

	if (!node)
		return;

	if (node->prop_count) {
		int i = 0;
		char *buf = node->data;

		node->prop = devm_kcalloc(dev, node->prop_count,
				sizeof(struct dsi_parser_prop),
				GFP_KERNEL);
		if (!node->prop)
			return;

		for (i = 0; i < node->prop_count; i++) {
			char *out = strsep(&buf, ";");
			struct dsi_parser_prop *prop = &node->prop[i];

			if (!out || !prop)
				continue;

			if (!dsi_parser_parse_prop(dev, prop, out)) {
				char *out1 = strsep(&out, "}");

				if (!out1)
					continue;

				out1 = dsi_parser_strim(out1);

				if (!out && strlen(out1)) {
					prop->name = out1;
					prop->value = "1";
				}
			}
		}
	}

	for (count = 0; count < node->children_count; count++)
		dsi_parser_get_properties(dev, node->child[count]);
}

static struct dsi_parser_prop *dsi_parser_search_property(
			struct dsi_parser_node *node,
			const char *name)
{
	int i = 0;
	struct dsi_parser_prop *prop = node->prop;

	for (i = 0; i < node->prop_count; i++) {
		if (prop[i].name && !strcmp(prop[i].name, name))
			return &prop[i];
	}

	return NULL;
}

/* APIs for the clients */
struct property *dsi_parser_find_property(const struct device_node *np,
				  const char *name,
				  int *lenp)
{
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_prop *prop = NULL;

	if (!node || !name || !lenp)
		goto end;

	prop = dsi_parser_search_property(node, name);
	if (!prop) {
		DSI_DEBUG("%s not found\n", name);
		goto end;
	}

	if (lenp) {
		if (prop->type == DSI_PROP_TYPE_INT_ARRAY)
			*lenp = prop->len;
		else if (prop->type == DSI_PROP_TYPE_INT_SET_ARRAY ||
			 prop->type == DSI_PROP_TYPE_INT_SET)
			*lenp = prop->len * sizeof(u32);
		else
			*lenp = strlen(prop->raw) + 1;

		DSI_DEBUG("%s len=%d\n", name, *lenp);
	}
end:
	return (struct property *)prop;
}

bool dsi_parser_read_bool(const struct device_node *np,
			const char *propname)
{
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	bool prop_set;

	prop_set = dsi_parser_search_property(node, propname) ? true : false;

	DSI_DEBUG("%s=%s\n", propname, prop_set ? "set" : "not set");

	return prop_set;
}

int dsi_parser_read_string(const struct device_node *np,
			const char *propname, const char **out_string)
{
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_prop *prop;
	char *property = NULL;
	int rc = 0;

	prop = dsi_parser_search_property(node, propname);
	if (!prop) {
		DSI_DEBUG("%s not found\n", propname);
		rc = -EINVAL;
	} else {
		property = prop->value;
	}

	*out_string = property;

	DSI_DEBUG("%s=%s\n", propname, *out_string);
	return rc;
}

int dsi_parser_read_u64(const struct device_node *np, const char *propname,
			 u64 *out_value)
{
	return -EINVAL;
}

int dsi_parser_read_u32(const struct device_node *np,
			const char *propname, u32 *out_value)
{
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_prop *prop;
	char *property, *to_int, item[SZ_128];
	int rc = 0, base;

	prop = dsi_parser_search_property(node, propname);
	if (!prop) {
		DSI_DEBUG("%s not found\n", propname);
		rc = -EINVAL;
		goto end;
	}

	if (!prop->value)
		goto end;

	strlcpy(item, prop->value, SZ_128);
	property = item;
	to_int = strsep(&property, "x");

	if (!property) {
		property = to_int;
		base = 10;
	} else {
		base = 16;
	}

	rc = kstrtoint(property, base, out_value);
	if (rc) {
		DSI_ERR("prop=%s error(%d) converting %s, base=%d\n",
			propname, rc, property, base);
		goto end;
	}

	DSI_DEBUG("%s=%d\n", propname, *out_value);
end:
	return rc;
}

int dsi_parser_read_u32_index(const struct device_node *np,
			const char *propname, u32 index, u32 *out_value)
{
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_prop *prop;
	char *property, *to_int, item[SZ_128];
	int rc = 0, base;

	prop = dsi_parser_search_property(node, propname);
	if (!prop) {
		DSI_DEBUG("%s not found\n", propname);
		rc = -EINVAL;
		goto end;
	}

	if (index >= prop->len) {
		rc = -EINVAL;
		goto end;
	}

	strlcpy(item, prop->items[index], SZ_128);
	property = item;
	to_int = strsep(&property, "x");

	if (!property) {
		property = to_int;
		base = 10;
	} else {
		base = 16;
	}

	rc = kstrtoint(property, base, out_value);
	if (rc) {
		DSI_ERR("prop=%s error(%d) converting %s, base=%d\n",
			propname, rc, property, base);
		goto end;
	}

	DSI_DEBUG("%s=%d\n", propname, *out_value);
end:
	return rc;
}

int dsi_parser_read_u32_array(const struct device_node *np,
			      const char *propname,
			      u32 *out_values, size_t sz)
{
	int i, rc = 0;
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_prop *prop;

	prop = dsi_parser_search_property(node, propname);
	if (!prop) {
		DSI_DEBUG("%s not found\n", propname);
		rc = -EINVAL;
		goto end;
	}

	for (i = 0; i < prop->len; i++) {
		int base, val;
		char item[SZ_128];
		char *to_int, *tmp;

		strlcpy(item, prop->items[i], SZ_128);

		tmp = item;

		to_int = strsep(&tmp, "x");

		if (!tmp) {
			tmp = to_int;
			base = 10;
		} else {
			base = 16;
		}

		rc = kstrtoint(tmp, base, &val);
		if (rc) {
			DSI_ERR("prop=%s error(%d) converting %s(%d), base=%d\n",
				propname, rc, tmp, i, base);
			continue;
		}

		*out_values++ = val;

		DSI_DEBUG("%s: [%d]=%d\n", propname, i, *(out_values - 1));
	}
end:
	return rc;
}

const void *dsi_parser_get_property(const struct device_node *np,
			const char *name, int *lenp)
{
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_prop *prop;
	char *property = NULL;

	prop = dsi_parser_search_property(node, name);
	if (!prop) {
		DSI_DEBUG("%s not found\n", name);
		goto end;
	}

	property = prop->value;

	if (prop->type == DSI_PROP_TYPE_STR)
		DSI_DEBUG("%s=%s\n", name, property);

	if (lenp) {
		if (prop->type == DSI_PROP_TYPE_INT_ARRAY)
			*lenp = prop->len;
		else if (prop->type == DSI_PROP_TYPE_INT_SET_ARRAY ||
			 prop->type == DSI_PROP_TYPE_INT_SET)
			*lenp = prop->len * sizeof(u32);
		else
			*lenp = strlen(prop->raw) + 1;

		DSI_DEBUG("%s len=%d\n", name, *lenp);
	}
end:
	return property;
}

struct device_node *dsi_parser_get_child_by_name(const struct device_node *np,
				const char *name)
{
	int index = 0;
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_node *matched_node = NULL;

	if (!node || !node->children_count)
		goto end;

	do {
		struct dsi_parser_node *child_node = node->child[index++];

		if (!child_node)
			goto end;

		if (!strcmp(child_node->name, name)) {
			matched_node = child_node;
			break;
		}
	} while (index < node->children_count);
end:
	DSI_DEBUG("%s: %s\n", name, matched_node ? "found" : "not found");

	return (struct device_node *)matched_node;
}

struct dsi_parser_node *dsi_parser_get_node_by_name(
				struct dsi_parser_node *node,
				char *name)
{
	int count = 0;
	struct dsi_parser_node *matched_node = NULL;

	if (!node) {
		DSI_ERR("node is null\n");
		goto end;
	}

	if (!strcmp(node->name, name)) {
		matched_node = node;
		goto end;
	}

	for (count = 0; count < node->children_count; count++) {
		matched_node = dsi_parser_get_node_by_name(
				node->child[count], name);
		if (matched_node)
			break;
	}
end:
	DSI_DEBUG("%s: %s\n", name, matched_node ? "found" : "not found");

	return matched_node;
}

int dsi_parser_get_child_count(const struct device_node *np)
{
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	int count = 0;

	if (node) {
		count = node->children_count;
		DSI_DEBUG("node %s child count=%d\n", node->name, count);
	}

	return count;
}

struct device_node *dsi_parser_get_next_child(const struct device_node *np,
	struct device_node *prev)
{
	int index = 0;
	struct dsi_parser_node *parent = (struct dsi_parser_node *)np;
	struct dsi_parser_node *prev_child = (struct dsi_parser_node *)prev;
	struct dsi_parser_node *matched_node = NULL;

	if (!parent || !parent->children_count)
		goto end;

	do {
		struct dsi_parser_node *child_node = parent->child[index++];

		if (!child_node)
			goto end;

		if (!prev) {
			matched_node = child_node;
			goto end;
		}

		if (!strcmp(child_node->name, prev_child->name)) {
			if (index < parent->children_count)
				matched_node = parent->child[index];
			break;
		}
	} while (index < parent->children_count);
end:
	if (matched_node)
		DSI_DEBUG("next child: %s\n", matched_node->name);

	return (struct device_node *)matched_node;
}

int dsi_parser_count_u32_elems(const struct device_node *np,
				const char *propname)
{
	int count = 0;
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_prop *prop;

	prop = dsi_parser_search_property(node, propname);
	if (!prop) {
		DSI_DEBUG("%s not found\n", propname);
		goto end;
	}

	count = prop->len;

	DSI_DEBUG("prop %s has %d items\n", prop->name, count);
end:
	return count;
}

int dsi_parser_count_strings(const struct device_node *np,
			    const char *propname)
{
	int count = 0;
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_prop *prop;

	prop = dsi_parser_search_property(node, propname);
	if (!prop) {
		DSI_DEBUG("%s not found\n", propname);
		goto end;
	}

	if (prop->type == DSI_PROP_TYPE_STR_ARRAY)
		count = prop->len;
	else if (prop->type == DSI_PROP_TYPE_STR)
		count = 1;

	DSI_DEBUG("prop %s has %d items\n", prop->name, count);
end:
	return count;
}

int dsi_parser_read_string_index(const struct device_node *np,
				const char *propname,
				int index, const char **output)
{
	struct dsi_parser_node *node = (struct dsi_parser_node *)np;
	struct dsi_parser_prop *prop;

	prop = dsi_parser_search_property(node, propname);
	if (!prop) {
		DSI_DEBUG("%s not found\n", propname);
		goto end;
	}

	if (prop->type != DSI_PROP_TYPE_STR_ARRAY) {
		DSI_ERR("not a string array property\n");
		goto end;
	}

	if (index >= prop->len) {
		DSI_ERR("out of bond index %d\n", index);
		goto end;
	}

	*output = prop->items[index];

	return 0;
end:
	return -EINVAL;
}

int dsi_parser_get_named_gpio(struct device_node *np,
				const char *propname, int index)
{
	int gpio = -EINVAL;

	dsi_parser_read_u32(np, propname, &gpio);

	return gpio;
}

void *dsi_parser_get_head_node(void *in,
				const u8 *data, u32 size)
{
	struct dsi_parser *parser = in;
	char *buf;

	if (!parser || !data || !size) {
		DSI_ERR("invalid input\n");
		goto err;
	}

	parser->buf = devm_kzalloc(parser->dev, size, GFP_KERNEL);
	if (!parser->buf)
		goto err;

	buf = parser->buf;

	memcpy(buf, data, size);

	strreplace(buf, '\n', ' ');
	strreplace(buf, '\t', '*');

	parser->head_node = dsi_parser_find_nodes(parser->dev, &buf);
	if (!parser->head_node) {
		DSI_ERR("could not get head node\n");
		devm_kfree(parser->dev, parser->buf);
		goto err;
	}

	dsi_parser_count_properties(parser->head_node);
	dsi_parser_get_properties(parser->dev, parser->head_node);

	parser->current_node = parser->head_node;

	return parser->head_node;
err:
	return NULL;
}

static int dsi_parser_read_file(struct dsi_parser *parser,
				const u8 **buf, u32 *size)
{
	int rc = 0;

	release_firmware(parser->fw);

	rc = request_firmware(&parser->fw, parser->file_name, parser->dev);
	if (rc || !parser->fw) {
		DSI_ERR("couldn't read firmware\n");
		goto end;
	}

	*buf = parser->fw->data;
	*size = parser->fw->size;

	DSI_DEBUG("file %s: size %zd\n",
		parser->file_name, parser->fw->size);
end:
	return rc;
}

static void dsi_parser_free_mem(struct device *dev,
				struct dsi_parser_node *node)
{
	int i = 0;

	if (!node)
		return;

	DSI_DEBUG("node=%s, prop_count=%d\n", node->name, node->prop_count);

	for (i = 0; i < node->prop_count; i++) {
		struct dsi_parser_prop *prop = &node->prop[i];

		if (!prop)
			continue;

		DSI_DEBUG("deleting prop=%s\n", prop->name);

		if (prop->items)
			devm_kfree(dev, prop->items);

		if (prop->raw)
			devm_kfree(dev, prop->raw);

		if ((prop->type == DSI_PROP_TYPE_INT_SET_ARRAY ||
		     prop->type == DSI_PROP_TYPE_INT_SET ||
		     prop->type == DSI_PROP_TYPE_INT_ARRAY) && prop->value)
			devm_kfree(dev, prop->value);
	}

	if (node->prop)
		devm_kfree(dev, node->prop);

	for (i = 0; i < node->children_count; i++)
		dsi_parser_free_mem(dev, node->child[i]);

	devm_kfree(dev, node);
}

static ssize_t dsi_parser_write_init(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dsi_parser *parser = file->private_data;
	const u8 *data = NULL;
	u32 size = 0;
	char buf[SZ_32];
	size_t len = 0;

	if (!parser)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_32 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (sscanf(buf, "%31s", parser->file_name) != 1) {
		DSI_ERR("failed to get val\n");
		goto end;
	}

	if (dsi_parser_read_file(parser, &data, &size)) {
		DSI_ERR("failed to read file\n");
		goto end;
	}

	dsi_parser_free_mem(parser->dev, parser->head_node);

	if (parser->buf) {
		devm_kfree(parser->dev, parser->buf);
		parser->buf = NULL;
	}

	parser->head_node = dsi_parser_get_head_node(parser, data, size);
	if (!parser->head_node) {
		DSI_ERR("failed to parse data\n");
		goto end;
	}
end:
	return len;
}

static ssize_t dsi_parser_read_node(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int i, j, len = 0, max_size = SZ_4K;
	struct dsi_parser *parser = file->private_data;
	struct dsi_parser_node *node;
	struct dsi_parser_prop *prop;

	if (!parser)
		return -ENODEV;

	if (*ppos)
		return len;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	node = parser->current_node;
	if (!node) {
		len = -EINVAL;
		goto error;
	}

	prop = node->prop;

	len += scnprintf(buf + len, max_size - len, "node name=%s\n",
		node->name);
	if (len == max_size)
		goto buffer_overflow;

	len += scnprintf(buf + len, max_size - len, "children count=%d\n",
		node->children_count);
	if (len == max_size)
		goto buffer_overflow;

	for (i = 0; i < node->children_count; i++) {
		len += scnprintf(buf + len, max_size - len, "child[%d]=%s\n",
			i, node->child[i]->name);
		if (len == max_size)
			goto buffer_overflow;
	}

	for (i = 0; i < node->prop_count; i++) {
		if (!prop[i].name)
			continue;

		len += scnprintf(buf + len, max_size - len,
			"property=%s\n", prop[i].name);
		if (len == max_size)
			goto buffer_overflow;

		if (prop[i].value) {
			if (prop[i].type == DSI_PROP_TYPE_STR) {
				len += scnprintf(buf + len, max_size - len,
					"value=%s\n", prop[i].value);
				if (len == max_size)
					goto buffer_overflow;
			} else {
				for (j = 0; j < prop[i].len; j++) {
					len += scnprintf(buf + len,
						max_size - len,
						"%x", prop[i].value[j]);
					if (len == max_size)
						goto buffer_overflow;
				}

				len += scnprintf(buf + len, max_size - len,
						"\n");
				if (len == max_size)
					goto buffer_overflow;

			}
		}

		if (prop[i].len) {
			len += scnprintf(buf + len, max_size - len, "items:\n");
			if (len == max_size)
				goto buffer_overflow;
		}

		for (j = 0; j < prop[i].len; j++) {
			char delim;

			if (j && !(j % 10))
				delim = '\n';
			else
				delim = ' ';

			len += scnprintf(buf + len, max_size - len, "%s%c",
				prop[i].items[j], delim);
			if (len == max_size)
				goto buffer_overflow;
		}

		len += scnprintf(buf + len, max_size - len, "\n\n");
		if (len == max_size)
			goto buffer_overflow;
	}
buffer_overflow:
	if (simple_read_from_buffer(user_buff, count, ppos, buf, len)) {
		len = -EFAULT;
		goto error;
	}
error:
	kfree(buf);

	return len;
}

static ssize_t dsi_parser_write_node(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dsi_parser *parser = file->private_data;
	char buf[SZ_512];
	size_t len = 0;

	if (!parser)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_512 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	strreplace(buf, '\n', ' ');

	if (!strcmp(strim(buf), "head_node"))
		parser->current_node = parser->head_node;
	else
		parser->current_node = dsi_parser_get_node_by_name(
					parser->head_node, strim(buf));
end:
	return len;
}

static const struct file_operations dsi_parser_init_fops = {
	.open = simple_open,
	.write = dsi_parser_write_init,
};

static const struct file_operations dsi_parser_node_fops = {
	.open = simple_open,
	.read = dsi_parser_read_node,
	.write = dsi_parser_write_node,
};

int dsi_parser_dbg_init(void *parser, struct dentry *parent_dir)
{
	int rc = 0;
	struct dentry *dir, *file;

	if (!parser || !parent_dir) {
		DSI_ERR("invalid input\n");
		goto end;
	}

	dir = debugfs_create_dir("parser", parent_dir);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);

		DSI_ERR("failed to create parser debugfs\n");
		goto end;
	}

	file = debugfs_create_file("init", 0644, dir,
		parser, &dsi_parser_init_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);

		DSI_ERR("failed to create init debugfs\n");
		goto dbg;
	}

	file = debugfs_create_file("node", 0644, dir,
		parser, &dsi_parser_node_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);

		DSI_ERR("failed to create init debugfs\n");
		goto dbg;
	}

	DSI_DEBUG("success\n");
	return 0;
dbg:
	debugfs_remove_recursive(dir);
end:
	return rc;
}

void *dsi_parser_get(struct device *dev)
{
	int rc = 0;
	struct dsi_parser *parser = NULL;

	if (!dev) {
		DSI_ERR("invalid data\n");
		rc = -EINVAL;
		goto end;
	}

	parser = devm_kzalloc(dev, sizeof(*parser), GFP_KERNEL);
	if (!parser) {
		rc = -ENOMEM;
		goto end;
	}

	parser->dev = dev;

	strlcpy(parser->file_name, "dsi_prop", sizeof(parser->file_name));

	return parser;
end:
	return ERR_PTR(rc);
}

void dsi_parser_put(void *data)
{
	struct dsi_parser *parser = data;

	if (!parser)
		return;

	dsi_parser_free_mem(parser->dev, parser->head_node);

	devm_kfree(parser->dev, parser->buf);
	devm_kfree(parser->dev, parser);
}

