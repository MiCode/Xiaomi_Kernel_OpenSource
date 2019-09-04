/*
 * Copyright (c) 2014-2018 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fb.h>

#ifndef CONFIG_TRUSTONIC_TRUSTED_UI
#define CONFIG_TRUSTONIC_TRUSTED_UI
#endif
#include <t-base-tui.h>

#include "tui_ioctl.h"
#include "dciTui.h"
#include "tlcTui.h"
#include "tui-hal.h"

#define TUI_MEMPOOL_SIZE 0

struct tui_mempool {
	void *va;
	unsigned long pa;
	size_t size;
};

static struct tui_mempool g_tui_mem_pool;

/* basic implementation of a memory pool for TUI framebuffer.  This
 * implementation is using kmalloc, for the purpose of demonstration only.
 * A real implementation might prefer using more advanced allocator, like ION,
 * in order not to exhaust memory available to kmalloc
 */
static bool allocate_tui_memory_pool(struct tui_mempool *pool, size_t size)
{
	bool ret = false;
	void *tui_mem_pool = NULL;

	pr_info("%s %s:%d\n", __func__, __FILE__, __LINE__);
	if (!size) {
		pr_debug("TUI frame buffer: nothing to allocate.");
		return true;
	}

	tui_mem_pool = kmalloc(size, GFP_KERNEL);
	if (!tui_mem_pool) {
		return ret;
	} else if (ksize(tui_mem_pool) < size) {
		pr_notice("TUI mem pool size too small: req'd=%zu alloc'd=%zu",
		       size, ksize(tui_mem_pool));
		kfree(tui_mem_pool);
	} else {
		pool->va = tui_mem_pool;
		pool->pa = virt_to_phys(tui_mem_pool);
		pool->size = ksize(tui_mem_pool);
		ret = true;
	}
	return ret;
}

static void free_tui_memory_pool(struct tui_mempool *pool)
{
	kfree(pool->va);
	memset(pool, 0, sizeof(*pool));
}

/**
 * hal_tui_init() - integrator specific initialization for kernel module
 *
 * This function is called when the kernel module is initialized, either at
 * boot time, if the module is built statically in the kernel, or when the
 * kernel is dynamically loaded if the module is built as a dynamic kernel
 * module. This function may be used by the integrator, for instance, to get a
 * memory pool that will be used to allocate the secure framebuffer and work
 * buffer for TUI sessions.
 *
 * Return: must return 0 on success, or non-zero on error. If the function
 * returns an error, the module initialization will fail.
 */
u32 hal_tui_init(void)
{
	/* Allocate memory pool for the framebuffer
	 */
	if (!allocate_tui_memory_pool(&g_tui_mem_pool, TUI_MEMPOOL_SIZE))
		return TUI_DCI_ERR_INTERNAL_ERROR;

	return TUI_DCI_OK;
}

/**
 * hal_tui_exit() - integrator specific exit code for kernel module
 *
 * This function is called when the kernel module exit. It is called when the
 * kernel module is unloaded, for a dynamic kernel module, and never called for
 * a module built into the kernel. It can be used to free any resources
 * allocated by hal_tui_init().
 */
void hal_tui_exit(void)
{
	/* delete memory pool if any */
	if (g_tui_mem_pool.va)
		free_tui_memory_pool(&g_tui_mem_pool);
}

/**
 * hal_tui_alloc() - allocator for secure framebuffer and working buffer
 * @allocbuffer:    input parameter that the allocator fills with the physical
 *                  addresses of the allocated buffers
 * @allocsize:      size of the buffer to allocate.  All the buffer are of the
 *                  same size
 * @number:         Number to allocate.
 *
 * This function is called when the module receives a CMD_TUI_SW_OPEN_SESSION
 * message from the secure driver.  The function must allocate 'number'
 * buffer(s) of physically contiguous memory, where the length of each buffer
 * is at least 'allocsize' bytes.  The physical address of each buffer must be
 * stored in the array of structure 'allocbuffer' which is provided as
 * arguments.
 *
 * Physical address of the first buffer must be put in allocate[0].pa , the
 * second one on allocbuffer[1].pa, and so on.  The function must return 0 on
 * success, non-zero on error.  For integrations where the framebuffer is not
 * allocated by the Normal World, this function should do nothing and return
 * success (zero).
 * If the working buffer allocation is different from framebuffers, ensure that
 * the physical address of the working buffer is at index 0 of the allocbuffer
 * table (allocbuffer[0].pa).
 */
u32 hal_tui_alloc(
	struct tui_alloc_buffer_t allocbuffer[MAX_DCI_BUFFER_NUMBER],
	size_t allocsize, u32 number)
{
	u32 ret = TUI_DCI_ERR_INTERNAL_ERROR;

	if (!allocbuffer) {
		pr_debug("%s(%d): allocbuffer is null\n", __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	pr_debug("%s(%d): Requested size=0x%zx x %u chunks\n",
		 __func__, __LINE__, allocsize, number);

	if ((size_t)allocsize == 0) {
		pr_debug("%s(%d): Nothing to allocate\n", __func__, __LINE__);
		return TUI_DCI_OK;
	}

	if (number != 2) {
		pr_debug("%s(%d): Unexpected number of buffers requested\n",
			 __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	if ((size_t)(allocsize * number) <= g_tui_mem_pool.size) {
		/* requested buffer fits in the memory pool */
		allocbuffer[0].pa = (u64)g_tui_mem_pool.pa;
		allocbuffer[1].pa = (u64)(g_tui_mem_pool.pa +
					       g_tui_mem_pool.size / 2);
		pr_debug("%s(%d): allocated at %llx\n", __func__, __LINE__,
			 allocbuffer[0].pa);
		pr_debug("%s(%d): allocated at %llx\n", __func__, __LINE__,
			 allocbuffer[1].pa);
		ret = TUI_DCI_OK;
	} else {
		/*
		 * requested buffer is bigger than the memory pool, return an
		 * error
		 */
		pr_debug("%s(%d): Memory pool too small\n", __func__, __LINE__);
		ret = TUI_DCI_ERR_INTERNAL_ERROR;
	}

	return ret;
}

/**
 * hal_tui_free() - free memory allocated by hal_tui_alloc()
 *
 * This function is called at the end of the TUI session, when the TUI module
 * receives the CMD_TUI_SW_CLOSE_SESSION message. The function should free the
 * buffers allocated by hal_tui_alloc(...).
 */
void hal_tui_free(void)
{
}

/**
 * hal_tui_deactivate() - deactivate Normal World display and input
 *
 * This function should stop the Normal World display and, if necessary, Normal
 * World input. It is called when a TUI session is opening, before the Secure
 * World takes control of display and input.
 *
 * Return: must return 0 on success, non-zero otherwise.
 */
u32 hal_tui_deactivate(void)
{
	/* Set linux TUI flag */
	trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);
	/*
	 * Stop NWd display here.  After this function returns, SWd will take
	 * control of the display and input.  Therefore the NWd should no longer
	 * access it
	 * This can be done by calling the fb_blank(FB_BLANK_POWERDOWN) function
	 * on the appropriate framebuffer device
	 */
	trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED |
			   TRUSTEDUI_MODE_INPUT_SECURED);

	return TUI_DCI_OK;
}

/**
 * hal_tui_activate() - restore Normal World display and input after a TUI
 * session
 *
 * This function should enable Normal World display and, if necessary, Normal
 * World input. It is called after a TUI session, after the Secure World has
 * released the display and input.
 *
 * Return: must return 0 on success, non-zero otherwise.
 */
u32 hal_tui_activate(void)
{
	/* Protect NWd */
	trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED |
			     TRUSTEDUI_MODE_INPUT_SECURED);
	/*
	 * Restart NWd display here.  TUI session has ended, and therefore the
	 * SWd will no longer use display and input.
	 * This can be done by calling the fb_blank(FB_BLANK_UNBLANK) function
	 * on the appropriate framebuffer device
	 */
	/* Clear linux TUI flag */
	trustedui_set_mode(TRUSTEDUI_MODE_OFF);
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
u32 hal_tui_process_cmd(struct tui_hal_cmd_t *cmd, struct tui_hal_rsp_t *rsp)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
u32 hal_tui_notif(void)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
void hal_tui_post_start(struct tlc_tui_response_t *rsp)
{
	pr_info("%s\n", __func__);
}
