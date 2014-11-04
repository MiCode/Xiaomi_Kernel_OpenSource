/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "ia_css_refcount.h"
#include "memory_access/memory_access.h"
#include "sh_css_defs.h"

#include "platform_support.h"

#include "assert_support.h"

#include "ia_css_debug.h"

/* TODO: enable for other memory aswell
	 now only for hrt_vaddress */
struct ia_css_refcount_entry {
	uint32_t count;
	hrt_vaddress data;
	int32_t id;
};

struct ia_css_refcount_list {
	uint32_t size;
	struct ia_css_refcount_entry *items;
};

static struct ia_css_refcount_list myrefcount;

static struct ia_css_refcount_entry *refcount_find_entry(hrt_vaddress ptr,
							 bool firstfree)
{
	uint32_t i;

	assert(ptr != 0);
	assert(myrefcount.items != NULL);

	for (i = 0; i < myrefcount.size; i++) {

		if ((&myrefcount.items[i])->data == 0) {
			if (firstfree) {
				/* for new entry */
				return &myrefcount.items[i];
			}
		}
		if ((&myrefcount.items[i])->data == ptr) {
			/* found entry */
			return &myrefcount.items[i];
		}
	}
	return NULL;
}

enum ia_css_err ia_css_refcount_init(uint32_t size)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(size != 0);
	assert(myrefcount.items == NULL);

	myrefcount.items =
	    sh_css_malloc(sizeof(struct ia_css_refcount_entry) * size);
	if (!myrefcount.items)
		err = IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;
	if (err == IA_CSS_SUCCESS) {
		memset(myrefcount.items, 0,
		       sizeof(struct ia_css_refcount_entry) * size);
		myrefcount.size = size;
	}
	return err;
}

void ia_css_refcount_uninit(void)
{
	struct ia_css_refcount_entry *entry;
	uint32_t i;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			    "ia_css_refcount_uninit() entry\n");
	for (i = 0; i < myrefcount.size; i++) {
		/* driver verifier tool has issues with &arr[i]
		   and prefers arr + i; as these are actually equivalent
		   the line below uses + i
		*/
		entry = myrefcount.items + i;
		if (entry->data != mmgr_NULL) {
			/*	ia_css_debug_dtrace(IA_CSS_DBG_TRACE,
				"ia_css_refcount_uninit: freeing (%x)\n",
				entry->data);*/
			mmgr_free(entry->data);
			entry->data = mmgr_NULL;
			entry->count = 0;
			entry->id = 0;
		}
	}
	sh_css_free(myrefcount.items);
	myrefcount.items = NULL;
	myrefcount.size = 0;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			    "ia_css_refcount_uninit() leave\n");
}

hrt_vaddress ia_css_refcount_increment(int32_t id, hrt_vaddress ptr)
{
	struct ia_css_refcount_entry *entry;

	if (ptr == mmgr_NULL)
		return ptr;

	entry = refcount_find_entry(ptr, false);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			    "ia_css_refcount_increment(%x) 0x%x\n", id, ptr);

	if (!entry) {
		entry = refcount_find_entry(ptr, true);
		assert(entry != NULL);
		if (entry == NULL)
			return mmgr_NULL;
		entry->id = id;
	}

	assert(entry->id == id);

	if (entry->data == ptr)
		entry->count += 1;
	else if (entry->data == mmgr_NULL) {
		entry->data = ptr;
		entry->count = 1;
	} else
		return mmgr_NULL;

	return ptr;
}

bool ia_css_refcount_decrement(int32_t id, hrt_vaddress ptr)
{
	struct ia_css_refcount_entry *entry;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			    "ia_css_refcount_decrement(%x) 0x%x\n", id, ptr);

	if (ptr == mmgr_NULL)
		return false;

	entry = refcount_find_entry(ptr, false);

	if (entry) {
		assert(entry->id == id);
		if (entry->count > 0) {
			entry->count -= 1;
			if (entry->count == 0) {
				/* ia_css_debug_dtrace(IA_CSS_DBEUG_TRACE,
				   "ia_css_refcount_decrement: freeing\n");*/
				mmgr_free(ptr);
				entry->data = mmgr_NULL;
				entry->id = 0;
			}
			return true;
		}
	}

	/* SHOULD NOT HAPPEN: ptr not managed by refcount, or not
	   valid anymore */
	if (entry)
		IA_CSS_ERROR("id %x, ptr 0x%x entry %p entry->id %x entry->count %d\n",
			id, ptr, entry, entry->id, entry->count);
	else
		IA_CSS_ERROR("entry NULL\n");
	assert(false);

	return false;
}

bool ia_css_refcount_is_single(hrt_vaddress ptr)
{
	struct ia_css_refcount_entry *entry;

	if (ptr == mmgr_NULL)
		return false;

	entry = refcount_find_entry(ptr, false);

	if (entry)
		return (entry->count == 1);

	return true;
}

void ia_css_refcount_clear(int32_t id, clear_func clear_func_ptr)
{
	struct ia_css_refcount_entry *entry;
	uint32_t i;
	uint32_t count = 0;

	assert(clear_func_ptr != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_refcount_clear(%x)\n",
			    id);

	for (i = 0; i < myrefcount.size; i++) {
		/* driver verifier tool has issues with &arr[i]
		   and prefers arr + i; as these are actually equivalent
		   the line below uses + i
		*/
		entry = myrefcount.items + i;
		if ((entry->data != mmgr_NULL) && (entry->id == id)) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
					    "ia_css_refcount_clear:"
					    " %x: 0x%x\n", id, entry->data);
			if (clear_func_ptr) {
				/* clear using provided function */
				clear_func_ptr(entry->data);
			} else {
				ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
						    "ia_css_refcount_clear: "
						    "using mmgr_free: "
						    "no clear_func\n");
				mmgr_free(entry->data);
			}
			assert(entry->count == 0);
			entry->data = mmgr_NULL;
			entry->count = 0;
			entry->id = 0;
			count++;
		}
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			    "ia_css_refcount_clear(%x): cleared %d\n", id,
			    count);
}
