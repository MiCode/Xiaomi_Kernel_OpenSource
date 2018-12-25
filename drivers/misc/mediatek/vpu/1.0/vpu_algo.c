/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/string.h>
#include "vpu_algo.h"

/* list of vlist_type(struct vpu_algo) */
static struct list_head vpu_algo_pool;
static uint32_t prop_info_data_length;

const size_t g_vpu_prop_type_size[VPU_NUM_PROP_TYPES] = {
	[VPU_PROP_TYPE_CHAR]     = sizeof(char),
	[VPU_PROP_TYPE_INT32]    = sizeof(int32_t),
	[VPU_PROP_TYPE_FLOAT]    = sizeof(int32_t),
	[VPU_PROP_TYPE_INT64]    = sizeof(int64_t),
	[VPU_PROP_TYPE_DOUBLE]   = sizeof(int64_t)
};

const char *g_vpu_prop_type_names[VPU_NUM_PROP_TYPES] = {
	[VPU_PROP_TYPE_CHAR]     = "char",
	[VPU_PROP_TYPE_INT32]    = "int32",
	[VPU_PROP_TYPE_FLOAT]    = "float",
	[VPU_PROP_TYPE_INT64]    = "int64",
	[VPU_PROP_TYPE_DOUBLE]   = "double"
};

const char *g_vpu_port_usage_names[VPU_NUM_PORT_USAGES] = {
	[VPU_PORT_USAGE_IMAGE]     = "image",
	[VPU_PORT_USAGE_DATA]      = "data",
};

const char *g_vpu_port_dir_names[VPU_NUM_PORT_DIRS] = {
	[VPU_PORT_DIR_IN]       = "in",
	[VPU_PORT_DIR_OUT]      = "out",
	[VPU_PORT_DIR_IN_OUT]   = "in-out",
};

struct vpu_prop_desc g_vpu_prop_descs[VPU_NUM_PROPS] = {
#define INS_PROP(id, name, type, count, access) \
	{ VPU_PROP_ ## id, VPU_PROP_TYPE_ ## type, \
	  VPU_PROP_ACCESS_ ## access, 0, count, name }

	INS_PROP(RESERVED, "reserved", INT32, 256, RDONLY),
#undef INS_PROP
};

int vpu_init_algo(struct vpu_device *vpu_device)
{
	uint16_t i = 0;
	uint32_t offset = 0;
	uint32_t prop_data_length;
	struct vpu_prop_desc *prop_desc;

	INIT_LIST_HEAD(&vpu_algo_pool);

	for (i = 0; i < VPU_NUM_PROPS; i++) {
		prop_desc = &g_vpu_prop_descs[i];
		prop_desc->offset = offset;
		prop_data_length =
		    prop_desc->count * g_vpu_prop_type_size[prop_desc->type];
		offset += prop_data_length;
	}
	/* the total length = last offset + last data length */
	prop_info_data_length = offset;

	return 0;
}

int vpu_add_algo_to_pool(struct vpu_algo *algo)
{
	list_add_tail(vlist_link(algo, struct vpu_algo), &vpu_algo_pool);
	return 0;
}

int vpu_find_algo_by_id(vpu_id_t id, struct vpu_algo **ralgo)
{
	struct vpu_algo *algo;
	struct list_head *head;
	char *name;

	if (id < 1)
		goto err;

	list_for_each(head, &vpu_algo_pool)
	{
		algo = vlist_node_of(head, struct vpu_algo);
		if (algo->id == id) {
			*ralgo = algo;
			return 0;
		}
	}

	if (vpu_get_name_of_algo(id, &name))
		goto err;

	if (vpu_create_algo(name, &algo) == 0) {
		vpu_add_algo_to_pool(algo);
		*ralgo = algo;
		return 0;
	}

err:
	*ralgo = NULL;
	return -ENOENT;
}

int vpu_find_algo_by_name(char *name, struct vpu_algo **ralgo)
{
	struct vpu_algo *algo;
	struct list_head *head;

	if (name == NULL)
		goto err;

	list_for_each(head, &vpu_algo_pool)
	{
		algo = vlist_node_of(head, struct vpu_algo);
		if (!strcmp(name, algo->name)) {
			*ralgo = algo;
			return 0;
		}
	}


	if (vpu_create_algo(name, &algo) == 0) {
		vpu_add_algo_to_pool(algo);
		*ralgo = algo;
		return 0;
	}

err:
	*ralgo = NULL;
	return -ENOENT;
}

static int vpu_calc_prop_offset(struct vpu_prop_desc *descs, uint32_t count,
				uint32_t *length)
{

	struct vpu_prop_desc *prop_desc;
	uint32_t offset = 0;
	uint32_t alignment = 1;
	uint32_t i, tmp;
	size_t type_size;

	/* get the alignment of struct packing */
	for (i = 0; i < count; i++) {
		prop_desc = &descs[i];
		type_size = g_vpu_prop_type_size[prop_desc->type];

		if (alignment < type_size)
			alignment = type_size;
	}

	/* calculate every prop's offset  */
	for (i = 0; i < count; i++) {
		prop_desc = &descs[i];
		type_size = g_vpu_prop_type_size[prop_desc->type];

		/* align offset with next data type */
		tmp = offset % type_size;
		if (tmp)
			offset += type_size - tmp;

		/* padding if the remainder is not enough */
		tmp = alignment - offset % alignment;
		if (tmp < type_size)
			offset += tmp;

		prop_desc->offset = offset;
		offset += prop_desc->count * type_size;
	}
	*length = offset;

	return 0;
}

int vpu_create_algo(char *name, struct vpu_algo **ralgo)
{
	int ret, id, mva, length;
	struct vpu_algo *algo = NULL;

	ret = vpu_get_entry_of_algo(name, &id, &mva, &length);
	CHECK_RET("algo(%s) is not existed in image files!\n", name);

	ret = vpu_alloc_algo(&algo);
	CHECK_RET("vpu_alloc_algo failed!\n");

	strlcpy(algo->name, name, sizeof(char[VPU_NAME_SIZE]));
	algo->id = id;
	algo->bin_ptr = mva;
	algo->bin_length = length;

	ret = vpu_hw_load_algo(algo);
	CHECK_RET("vpu_hw_load_algo failed!\n");
	ret = vpu_hw_get_algo_info(algo);
	CHECK_RET("vpu_hw_get_algo_info failed!\n");
	ret = vpu_calc_prop_offset(algo->info_descs, algo->info_desc_count,
				   &algo->info_length);
	CHECK_RET("vpu_calc_prop_offset[info] failed!\n");
	ret = vpu_calc_prop_offset(algo->sett_descs, algo->sett_desc_count,
				   &algo->sett_length);
	CHECK_RET("vpu_calc_prop_offset[sett] failed!\n");

	*ralgo = algo;
	return 0;

out:
	*ralgo = NULL;
	if (algo != NULL)
		vpu_free_algo(algo);
	return ret;
}

int vpu_alloc_algo(struct vpu_algo **ralgo)
{
	struct vpu_algo *algo;

	algo = kzalloc(sizeof(vlist_type(struct vpu_algo)) +
					prop_info_data_length, GFP_KERNEL);
	if (algo == NULL) {
		LOG_ERR("vpu_alloc_algo(), algo=0x%p\n", algo);
		return -ENOMEM;
	}
	algo->info_ptr = (uint64_t) algo + sizeof(vlist_type(struct vpu_algo));
	algo->info_length = prop_info_data_length;

	INIT_LIST_HEAD(vlist_link(algo, struct vpu_algo));
	*ralgo = algo;
	return 0;
}


int vpu_free_algo(struct vpu_algo *algo)
{
	if (algo == NULL)
		return 0;

	kfree(algo);
	return 0;
}

int vpu_alloc_request(struct vpu_request **rreq)
{
	struct vpu_request *req;

	req = kzalloc(sizeof(vlist_type(struct vpu_request)), GFP_KERNEL);
	if (req == NULL) {
		LOG_ERR("vpu_alloc_request(), node=0x%p\n", req);
		return -ENOMEM;
	}

	*rreq = req;

	return 0;
}

int vpu_free_request(struct vpu_request *req)
{
	if (req != NULL)
		kfree(req);
	return 0;
}

int vpu_dump_algo(struct seq_file *s)
{
	struct vpu_algo *algo;
	struct vpu_prop_desc *prop_desc;
	struct vpu_port *port;
	struct list_head *head;
	uint32_t i, j;
	uint32_t data_length;
	char line_buffer[24 + 1] = {0};
	unsigned char *info_data;

	list_for_each(head, &vpu_algo_pool)
	{
		algo = vlist_node_of(head, struct vpu_algo);
		vpu_print_seq(s,
					"[Algo: id=%d name=%s, address=0x%llx, length=%d]\n",
					  algo->id, algo->name,
					  algo->bin_ptr,
					  algo->bin_length);

#define LINE_BAR "  +-----+---------------+-------+-------+\n"
		vpu_print_seq(s, LINE_BAR);
		vpu_print_seq(s, "  |%-5s|%-15s|%-7s|%-7s|\n", "Port", "Name",
			      "Dir", "Usage");
		vpu_print_seq(s, LINE_BAR);

		for (i = 0; i < algo->port_count; i++) {
			port = &algo->ports[i];
			vpu_print_seq(s, "  |%-5d|%-15s|%-7s|%-7s|\n", port->id,
				      port->name,
				      g_vpu_port_dir_names[port->dir],
				      g_vpu_port_usage_names[port->usage]);
		}
		vpu_print_seq(s, LINE_BAR);
		vpu_print_seq(s, "\n");
#undef LINE_BAR

#define LINE_BAR "  +-----+---------------+-------+-------+------------------------------+\n"
		if (algo->info_desc_count) {
			vpu_print_seq(s, LINE_BAR);
			vpu_print_seq(s, "  |%-5s|%-15s|%-7s|%-7s|%-30s|\n",
				      "Info", "Name", "Type", "Count", "Value");
			vpu_print_seq(s, LINE_BAR);
		}

		for (i = 0; i < algo->info_desc_count; i++) {
			prop_desc = &algo->info_descs[i];
			data_length = prop_desc->count *
				      g_vpu_prop_type_size[prop_desc->type];

			vpu_print_seq(s, "  |%-5d|%-15s|%-7s|%-7d|%04XH ",
				      prop_desc->id, prop_desc->name,
				      g_vpu_prop_type_names[prop_desc->type],
				      prop_desc->count, 0);

			info_data = (unsigned char *)(algo->info_ptr +
						      prop_desc->offset);
			memset(line_buffer, ' ', 24);
			for (j = 0; j < data_length; j++, info_data++) {
				int pos = j % 8;

				if (j && pos == 0) {
					vpu_print_seq(
						s,
						"  |%-5s|%-15s|%-7s|%-7s|%04XH ",
						"", "", "", "", j);
				}
				sprintf(line_buffer + pos * 3, "%02X",
					*info_data);
				line_buffer[pos * 3 + 2] = ' ';
				if (pos == 7 || j + 1 == data_length)
					vpu_print_seq(s, "%s|\n", line_buffer);
			}
		}

		if (algo->info_desc_count) {
			vpu_print_seq(s, LINE_BAR);
			vpu_print_seq(s, "\n");
		}
#undef LINE_BAR

#define LINE_BAR "  +-----+---------------+-------+-------+-------+\n"
		if (algo->sett_desc_count < 1)
			continue;

		vpu_print_seq(s, LINE_BAR);
		vpu_print_seq(s, "  |%-5s|%-15s|%-7s|%-7s|%-7s|\n", "Sett",
			      "Name", "Offset", "Type", "Count");
		vpu_print_seq(s, LINE_BAR);
		for (i = 0; i < algo->sett_desc_count; i++) {
			prop_desc = &algo->sett_descs[i];
			data_length = prop_desc->count *
				      g_vpu_prop_type_size[prop_desc->type];

			vpu_print_seq(s, "  |%-5d|%-15s|%-7d|%-7s|%-7d|\n",
				      prop_desc->id, prop_desc->name,
				      prop_desc->offset,
				      g_vpu_prop_type_names[prop_desc->type],
				      prop_desc->count);
		}
		vpu_print_seq(s, LINE_BAR);
		vpu_print_seq(s, "\n");
#undef LINE_BAR

	}
	return 0;
}
