/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2013-2015 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __MALI_MEMORY_H__
#define __MALI_MEMORY_H__

#include "mali_osk.h"
#include "mali_session.h"

#include <linux/list.h>
#include <linux/mm.h>

#include "mali_memory_types.h"
#include "mali_memory_os_alloc.h"

_mali_osk_errcode_t mali_memory_initialize(void);
void mali_memory_terminate(void);

/** @brief Allocate a page table page
 *
 * Allocate a page for use as a page directory or page table. The page is
 * mapped into kernel space.
 *
 * @return _MALI_OSK_ERR_OK on success, otherwise an error code
 * @param table_page GPU pointer to the allocated page
 * @param mapping CPU pointer to the mapping of the allocated page
 */
MALI_STATIC_INLINE _mali_osk_errcode_t
mali_mmu_get_table_page(mali_dma_addr *table_page, mali_io_address *mapping)
{
	return mali_mem_os_get_table_page(table_page, mapping);
}

/** @brief Release a page table page
 *
 * Release a page table page allocated through \a mali_mmu_get_table_page
 *
 * @param pa the GPU address of the page to release
 */
MALI_STATIC_INLINE void
mali_mmu_release_table_page(mali_dma_addr phys, void *virt)
{
	mali_mem_os_release_table_page(phys, virt);
}

/** @brief mmap function
 *
 * mmap syscalls on the Mali device node will end up here.
 *
 * This function allocates Mali memory and maps it on CPU and Mali.
 */
int mali_mmap(struct file *filp, struct vm_area_struct *vma);

/** @brief Start a new memory session
 *
 * Called when a process opens the Mali device node.
 *
 * @param session Pointer to session to initialize
 */
_mali_osk_errcode_t mali_memory_session_begin(struct mali_session_data *session);

/** @brief Close a memory session
 *
 * Called when a process closes the Mali device node.
 *
 * Memory allocated by the session will be freed
 *
 * @param session Pointer to the session to terminate
 */
void mali_memory_session_end(struct mali_session_data *session);

/** @brief Prepare Mali page tables for mapping
 *
 * This function will prepare the Mali page tables for mapping the memory
 * described by \a descriptor.
 *
 * Page tables will be reference counted and allocated, if not yet present.
 *
 * @param descriptor Pointer to the memory descriptor to the mapping
 */
_mali_osk_errcode_t mali_mem_mali_map_prepare(mali_mem_allocation *descriptor);

/** @brief Free Mali page tables for mapping
 *
 * This function will unmap pages from Mali memory and free the page tables
 * that are now unused.
 *
 * The updated pages in the Mali L2 cache will be invalidated, and the MMU TLBs will be zapped if necessary.
 *
 * @param descriptor Pointer to the memory descriptor to unmap
 */
void mali_mem_mali_map_free(struct mali_session_data *session, u32 size, mali_address_t vaddr, u32 flags);

/** @brief Parse resource and prepare the OS memory allocator
 *
 * @param size Maximum size to allocate for Mali GPU.
 * @return _MALI_OSK_ERR_OK on success, otherwise failure.
 */
_mali_osk_errcode_t mali_memory_core_resource_os_memory(u32 size);

/** @brief Parse resource and prepare the dedicated memory allocator
 *
 * @param start Physical start address of dedicated Mali GPU memory.
 * @param size Size of dedicated Mali GPU memory.
 * @return _MALI_OSK_ERR_OK on success, otherwise failure.
 */
_mali_osk_errcode_t mali_memory_core_resource_dedicated_memory(u32 start, u32 size);


struct mali_page_node *_mali_page_node_allocate(mali_page_node_type type);

void _mali_page_node_ref(struct mali_page_node *node);
void _mali_page_node_unref(struct mali_page_node *node);
void _mali_page_node_add_page(struct mali_page_node *node, struct page *page);

void _mali_page_node_add_block_item(struct mali_page_node *node, mali_block_item *item);

int _mali_page_node_get_ref_count(struct mali_page_node *node);
dma_addr_t _mali_page_node_get_phy_addr(struct mali_page_node *node);
unsigned long _mali_page_node_get_pfn(struct mali_page_node *node);

#endif /* __MALI_MEMORY_H__ */
