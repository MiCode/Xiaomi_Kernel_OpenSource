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

#ifndef _MEMORY_GROUP_MANAGER_H_
#define _MEMORY_GROUP_MANAGER_H_

#define MEMORY_GROUP_MANAGER_NR_GROUPS (16)

struct memory_group_manager_device;

/**
 * struct memory_group_manager_ops - Callbacks for memory group manager
 *                                   operations
 *
 * @mgm_alloc_page: Callback to allocate physical memory in a group
 * @mgm_free_page:  Callback to free physical memory in a group
 */
struct memory_group_manager_ops {
	/**
	 * mgm_alloc_page - Allocate a physical memory page in a group
	 *
	 * @mgm_dev:  The memory group manager through which the request is
	 *            being made.
	 * @group_id: A physical memory group ID. The meaning of this is defined
	 *            by the systems integrator. Its valid range is
	 *            0 .. MEMORY_GROUP_MANAGER_NR_GROUPS-1.
	 * @gfp_mask: Bitmask of Get Free Page flags affecting allocator
	 *            behavior.
	 * @order:    Page order for physical page size (order=0 means 4 KiB,
	 *            order=9 means 2 MiB).
	 *
	 * Return: Pointer to allocated page, or NULL if allocation failed.
	 */
	struct page *(*mgm_alloc_page)(
		struct memory_group_manager_device *mgm_dev, int group_id,
		gfp_t gfp_mask, unsigned int order);

	/**
	 * mgm_free_page - Free a physical memory page in a group
	 *
	 * @mgm_dev:  The memory group manager through which the request
	 *            is being made.
	 * @group_id: A physical memory group ID. The meaning of this is
	 *            defined by the systems integrator. Its valid range is
	 *            0 .. MEMORY_GROUP_MANAGER_NR_GROUPS-1.
	 * @page:     Address of the struct associated with a page of physical
	 *            memory that was allocated by calling the mgm_alloc_page
	 *            method of the same memory pool with the same values of
	 *            @group_id and @order.
	 * @order:    Page order for physical page size (order=0 means 4 KiB,
	 *            order=9 means 2 MiB).
	 */
	void (*mgm_free_page)(
		struct memory_group_manager_device *mgm_dev, int group_id,
		struct page *page, unsigned int order);
};

/**
 * struct memory_group_manager_device - Device structure for a memory group
 *                                      manager
 *
 * @ops  - Callbacks associated with this device
 * @data - Pointer to device private data
 *
 * In order for a systems integrator to provide custom behaviors for memory
 * operations performed by the kbase module (controller driver), they must
 * provide a platform-specific driver module which implements this interface.
 *
 * This structure should be registered with the platform device using
 * platform_set_drvdata().
 */
struct memory_group_manager_device {
	struct memory_group_manager_ops ops;
	void *data;
};

#endif /* _MEMORY_GROUP_MANAGER_H_ */
