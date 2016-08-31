/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <linux/err.h>
#include <linux/nvshm_stats.h>
#include "nvshm_priv.h"
#include "nvshm_iobuf.h"

#define B2A(h, x) ((void *)(x) + ((int)(h)->mb_base_virt) - NVSHM_IPC_BB_BASE)

struct nvshm_stats_desc {
	/** The type of data. */
	char type; /* enums are chars on the BB side*/
	/** The name of the data field. */
	char name[64];
	char pad[3]; /* modem needs pointers aligned */
	/** A pointer to the decode information if this is a sub
	* structure entry (type = NVSHM_STATS_SUB). */
	const struct nvshm_stats_desc *sub;
	/** The offset of this entry from the start of the current
	* structure. */
	unsigned int offset;
	/** The number of elements. */
	int size;
	/** size of each element. 0 if non applicable */
	int elem_size;
};

/** The stats data header structure.
 * This data structure is automatically added to the head of every
 * top level stats data structure by the STATS_TOP_STRUCTURE_START()
 * macro. It is used to hold stats system specific data (currently
 * only the enabled flag). */
struct data_header {
	/** Indicates whether this stats data is enabled or not. */
	unsigned int enabled;
};

/** Structure used to hold a stats entry in the stats system. */
struct table_entry {
	/** Pointer to header within the stats data.
	 * This will be NULL until the stats entry has been installed.
	 * This is assumed to be the start of the stats data and offsets
	 * to stats fields can be applied to this address. */
	struct data_header *data;
	/** Total size of stats data pointed to by data. */
	unsigned int size;
	/** Pointer to decode entries array. */
	const struct nvshm_stats_desc *desc;

};

struct nvshm_stats_private {
	const struct nvshm_handle *handle;
	void *address;
	int records_no;
};

/* the notifier list remains valid for the life of the kernel */
static RAW_NOTIFIER_HEAD(notifier_list);

/* priv gets reset at modem [re-]boot */
static struct nvshm_stats_private priv;

void nvshm_stats_init(struct nvshm_handle *handle)
{
	priv.handle = handle;
	priv.address = handle->stats_base_virt;
	priv.records_no = 0;
	raw_notifier_call_chain(&notifier_list, NVSHM_STATS_MODEM_UP, NULL);
}

void nvshm_stats_cleanup(void)
{
	raw_notifier_call_chain(&notifier_list, NVSHM_STATS_MODEM_DOWN, NULL);
}

const u32 *nvshm_stats_top(const char *top_name,
			   struct nvshm_stats_iter *it)
{
	const struct table_entry *entry;
	const u32 *rc = ERR_PTR(-ENOENT);
	unsigned int i, total_no;

	if (!priv.handle->stats_size)
		return ERR_PTR(-ENODATA);

	total_no = *(const unsigned int *) priv.address;
	entry = priv.address + sizeof(unsigned int);
	for (i = 0; i < total_no; i++, entry++) {
		const struct nvshm_stats_desc *desc;

		if (!entry->desc || !entry->data)
			continue;

		desc = B2A(priv.handle, entry->desc);
		if (!strcmp(desc->name, top_name)) {
			it->desc = desc;
			it->data = B2A(priv.handle, entry->data);
			rc = (const u32 *) it->data;
			it->data += sizeof(*rc);
			break;
		}
	}

	return rc;
}
EXPORT_SYMBOL_GPL(nvshm_stats_top);

int nvshm_stats_sub(const struct nvshm_stats_iter *it,
		    int index,
		    struct nvshm_stats_iter *sub_it)
{
	if (it->desc->type != NVSHM_STATS_SUB)
		return -EINVAL;

	if (index >= it->desc->size)
		return -ERANGE;

	sub_it->desc = B2A(priv.handle, it->desc->sub);
	sub_it->data = it->data + index * it->desc->elem_size;
	return 0;
}
EXPORT_SYMBOL_GPL(nvshm_stats_sub);

int nvshm_stats_next(struct nvshm_stats_iter *it)
{
	if ((it->desc->type != NVSHM_STATS_START) &&
	    (it->desc->type != NVSHM_STATS_END))
		it->data += it->desc->size * it->desc->elem_size;

	it->desc++;
	return 0;
}
EXPORT_SYMBOL_GPL(nvshm_stats_next);

const char *nvshm_stats_name(const struct nvshm_stats_iter *it)
{
	return it->desc->name;
}
EXPORT_SYMBOL_GPL(nvshm_stats_name);

enum nvshm_stats_type nvshm_stats_type(const struct nvshm_stats_iter *it)
{
	return it->desc->type;
}
EXPORT_SYMBOL_GPL(nvshm_stats_type);

int nvshm_stats_elems(const struct nvshm_stats_iter *it)
{
	return it->desc->size;
}
EXPORT_SYMBOL_GPL(nvshm_stats_elems);

static inline int check_type_index(const struct nvshm_stats_iter *it,
				   enum nvshm_stats_type type,
				   int index)
{
	if (it->desc->type != type)
		return -EINVAL;

	if (index >= it->desc->size)
		return -ERANGE;

	return 0;
}

u32 *nvshm_stats_valueptr_uint32(const struct nvshm_stats_iter *it,
				 int index)
{
	u32 *array;
	int rc;

	rc = check_type_index(it, NVSHM_STATS_UINT32, index);
	if (rc)
		return ERR_PTR(rc);

	array = (u32 *) it->data;
	return &array[index];
}
EXPORT_SYMBOL_GPL(nvshm_stats_valueptr_uint32);

s32 *nvshm_stats_valueptr_sint32(const struct nvshm_stats_iter *it,
				 int index)
{
	s32 *array;
	int rc;

	rc = check_type_index(it, NVSHM_STATS_SINT32, index);
	if (rc)
		return ERR_PTR(rc);

	array = (s32 *) it->data;
	return &array[index];
}
EXPORT_SYMBOL_GPL(nvshm_stats_valueptr_sint32);

u64 *nvshm_stats_valueptr_uint64(const struct nvshm_stats_iter *it,
				int index)
{
	u64 *array;
	int rc;

	rc = check_type_index(it, NVSHM_STATS_UINT64, index);
	if (rc)
		return ERR_PTR(rc);

	array = (u64 *) it->data;
	return &array[index];
}
EXPORT_SYMBOL_GPL(nvshm_stats_valueptr_uint64);

void nvshm_stats_register(struct notifier_block *nb)
{
	raw_notifier_chain_register(&notifier_list, nb);
}
EXPORT_SYMBOL_GPL(nvshm_stats_register);

void nvshm_stats_unregister(struct notifier_block *nb)
{
	raw_notifier_chain_unregister(&notifier_list, nb);
}
EXPORT_SYMBOL_GPL(nvshm_stats_unregister);
