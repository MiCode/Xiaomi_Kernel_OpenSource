/*
 *
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <linux/gfp.h>
#include <linux/memory_group_manager.h>

#include <mali_kbase.h>
#include <mali_kbase_native_mgm.h>

/**
 * kbase_native_mgm_alloc - Native physical memory allocation method
 *
 * Delegates all memory allocation requests to the kernel's alloc_pages
 * function.
 *
 * @mgm_dev:  The memory group manager the request is being made through.
 * @group_id: A physical memory group ID, which must be valid but is not used.
 *            Its valid range is 0 .. MEMORY_GROUP_MANAGER_NR_GROUPS-1.
 * @gfp_mask: Bitmask of Get Free Page flags affecting allocator behavior.
 * @order:    Page order for physical page size (order=0 means 4 KiB,
 *            order=9 means 2 MiB).
 *
 * Return: Pointer to allocated page, or NULL if allocation failed.
 */
static struct page *kbase_native_mgm_alloc(
	struct memory_group_manager_device *mgm_dev, int group_id,
	gfp_t gfp_mask, unsigned int order)
{
	CSTD_UNUSED(mgm_dev);
	WARN_ON(group_id < 0);
	WARN_ON(group_id >= MEMORY_GROUP_MANAGER_NR_GROUPS);

	return alloc_pages(gfp_mask, order);
}

/**
 * kbase_native_mgm_free - Native physical memory freeing method
 *
 * Delegates all memory freeing requests to the kernel's __free_pages function.
 *
 * @mgm_dev:  The memory group manager the request is being made through.
 * @group_id: A physical memory group ID, which must be valid but is not used.
 *            Its valid range is 0 .. MEMORY_GROUP_MANAGER_NR_GROUPS-1.
 * @page:     Address of the struct associated with a page of physical
 *            memory that was allocated by calling the alloc method of
 *            the same memory pool with the same argument values.
 * @order:    Page order for physical page size (order=0 means 4 KiB,
 *            order=9 means 2 MiB).
 */
static void kbase_native_mgm_free(struct memory_group_manager_device *mgm_dev,
	int group_id, struct page *page, unsigned int order)
{
	CSTD_UNUSED(mgm_dev);
	WARN_ON(group_id < 0);
	WARN_ON(group_id >= MEMORY_GROUP_MANAGER_NR_GROUPS);

	__free_pages(page, order);
}

struct memory_group_manager_device kbase_native_mgm_dev = {
	.ops = {
		.mgm_alloc_page = kbase_native_mgm_alloc,
		.mgm_free_page = kbase_native_mgm_free
	},
	.data = NULL
};
