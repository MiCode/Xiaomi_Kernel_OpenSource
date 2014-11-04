/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
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

#include "sh_css_refcount.h"
#include "memory_access/memory_access.h"
#include "sh_css_defs.h"

#include "platform_support.h"

#include "assert_support.h"

#include "sh_css_debug.h"

/* TODO: enable for other memory aswell
	 now only for hrt_vaddress */
struct sh_css_refcount_entry {
	uint32_t count;
	hrt_vaddress data;
	size_t size;
	int32_t id;
};

struct sh_css_refcount_list {
	uint32_t size;
	struct sh_css_refcount_entry *items;
};

static struct sh_css_refcount_list myrefcount;

int sh_css_refcount_used(void)
{
	uint32_t i;
	int used = 0;
	for (i = 0; i < myrefcount.size; i++) {
		if (myrefcount.items[i].data != mmgr_NULL)
			++used;
	}
	return used;
}

static struct sh_css_refcount_entry *find_entry(hrt_vaddress ptr)
{
	uint32_t i;

assert(ptr != 0);
assert(myrefcount.items != NULL);
	if (myrefcount.items == NULL)
		return NULL;

	for (i = 0; i < myrefcount.size; i++) {
		if (myrefcount.items[i].data == ptr) {
			/* found entry */
			return &myrefcount.items[i];
		}
	}
	return NULL;
}

static struct sh_css_refcount_entry *find_free_entry(hrt_vaddress ptr)
{
	uint32_t i;

assert(ptr != 0);
assert(myrefcount.items != NULL);
	if (myrefcount.items == NULL)
		return NULL;

	for (i = 0; i < myrefcount.size; i++) {
		if (myrefcount.items[i].data == 0)
			return &myrefcount.items[i];
	}
	return NULL;
}


enum sh_css_err sh_css_refcount_init(void)
{
	enum sh_css_err err = sh_css_success;
	int size = 2000;

assert(myrefcount.items == NULL);

	myrefcount.items =
		sh_css_malloc(sizeof(struct sh_css_refcount_entry)*size);
	if (!myrefcount.items)
		err = sh_css_err_cannot_allocate_memory;
	if (err == sh_css_success) {
		memset(myrefcount.items, 0,
			   sizeof(struct sh_css_refcount_entry)*size);
		myrefcount.size = size;
	}
	return err;
}

void sh_css_refcount_uninit(void)
{
	struct sh_css_refcount_entry *entry;
	uint32_t i;
	for (i = 0; i < myrefcount.size; i++) {
		entry = &myrefcount.items[i];
		if (entry->data != mmgr_NULL) {
/*			sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
				"sh_css_refcount_uninit: freeing (%x)\n",
				entry->data);*/
			mmgr_free(entry->data);
			entry->data = mmgr_NULL;
			entry->size = 0;
			entry->count = 0;
			entry->id = 0;
		}
	}
	sh_css_free(myrefcount.items);
	myrefcount.items = NULL;
	myrefcount.size = 0;
}

hrt_vaddress sh_css_refcount_alloc(
	int32_t id, const size_t size, const uint16_t attribute)
{
	hrt_vaddress ptr;
	struct sh_css_refcount_entry *entry = NULL;
	uint32_t i;

	assert(size > 0);
	assert(id != FREE_BUF_CACHE);

	for (i = 0; i < myrefcount.size; i++) {
		entry = &myrefcount.items[i];
		if ((entry->id == FREE_BUF_CACHE) && (entry->size == size)) {
			entry->id = id;
			assert(entry->count == 0);
			entry->count = 1;
			assert(entry->data != mmgr_NULL);
			if (attribute & MMGR_ATTRIBUTE_CLEARED)
				mmgr_clear(entry->data, size);
			sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
				"sh_css_refcount_alloc(%x) 0x%x "
				"reused from cache, refcnt %d\n",
				id, entry->data, entry->count);

			return entry->data;
		}
	}

	ptr = mmgr_alloc_attr(size, attribute);
	assert(ptr != mmgr_NULL);

	/* This address should not exist in the administration yet */
	assert(!find_entry(ptr));
	entry = find_free_entry(ptr);

	assert(entry != NULL);
	if (entry == NULL)
		return mmgr_NULL;
	assert(entry->data == mmgr_NULL);
	

	entry->id = id;
	entry->data = ptr;
	entry->size = size;
	entry->count = 1;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_refcount_alloc(%x) 0x%x "
		"new alloc refcnt %d\n",
		id, ptr, entry->count);


	return ptr;
}

hrt_vaddress sh_css_refcount_retain(int32_t id, hrt_vaddress ptr)
{
	struct sh_css_refcount_entry *entry;

	assert(id != FREE_BUF_CACHE);

	assert(ptr != mmgr_NULL);
	entry = find_entry(ptr);

	assert(entry != NULL);
	if (entry == NULL)
		return mmgr_NULL;
	assert(entry->id == id);
	assert(entry->data == ptr);

	entry->count += 1;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_refcount_retain(%x) 0x%x new refcnt %d\n",
		id, ptr, entry->count );
	return ptr;
}

bool sh_css_refcount_release(int32_t id, hrt_vaddress ptr)
{
	struct sh_css_refcount_entry *entry;

	assert(id != FREE_BUF_CACHE);

	if (ptr == mmgr_NULL)
		return false;

	entry = find_entry(ptr);

	if (entry) {
		assert(entry->id == id);
		if (entry->count > 0) {
			entry->count -= 1;
			if (entry->count == 0) {
/*				sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
					"sh_css_refcount_release: freeing\n");*/
				entry->id = FREE_BUF_CACHE;
				sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
					"sh_css_refcount_release(%x) 0x%x "
					"new refcnt 0, returned to cache\n",
					id, ptr);
			} else {
				sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
					"sh_css_refcount_release(%x) 0x%x "
					"new refcnt %d\n",
					id, ptr, entry->count );
			}
			return true;
		}
	}

/* SHOULD NOT HAPPEN: ptr not managed by refcount, or not valid anymore */
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_refcount_release(%x) 0x%x ERROR not managed\n", id, ptr);
	assert(false);

	return false;
}

bool sh_css_refcount_is_single(hrt_vaddress ptr)
{
	struct sh_css_refcount_entry *entry;

	if (ptr == mmgr_NULL)
		return false;

	entry = find_entry(ptr);

	if (entry)
		return (entry->count == 1);

	return true;
}

int32_t sh_css_refcount_get_id(hrt_vaddress ptr)
{
	struct sh_css_refcount_entry *entry;
	assert(ptr != mmgr_NULL);
	entry = find_entry(ptr);
	assert(entry != NULL);
	if (entry == NULL)
		return mmgr_NULL;
	return entry->id;
}

void sh_css_refcount_clear(int32_t id, void (*clear_func)(hrt_vaddress ptr))
{
	struct sh_css_refcount_entry *entry;
	uint32_t i;
	uint32_t count = 0;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_refcount_clear(%x)\n", id);
	for (i = 0; i < myrefcount.size; i++) {
		entry = &myrefcount.items[i];
		if ((entry->data != mmgr_NULL) && (entry->id == id)) {
			sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
				"sh_css_refcount_clear: %x: 0x%x refcnt %d\n",
				id, entry->data, entry->count);
			if (clear_func) {
				/* clear using provided function */
				/* This function will update the entry */
				/* administration (we should not do that)  */
				clear_func(entry->data);
				assert(entry->count == 0);
			}
			else {
				sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
						"sh_css_refcount_clear: "
						"using default mmgr_free\n");
				mmgr_free(entry->data);

				assert(entry->count == 0);
				entry->data = mmgr_NULL;
				entry->size = 0;
				entry->count = 0;
				entry->id = 0;
			}
			count++;
		}
	}
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_refcount_clear(%x): cleared %d\n", id, count);
}

