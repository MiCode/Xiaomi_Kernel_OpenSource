// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/delay.h>

#include "t-base-tui.h"

#include "tui_ioctl.h"
#include "dciTui.h"
#include "tlcTui.h"
#include "tui-hal.h"
#include "tui-hal_mt.h"
#include <memory_ssmr.h>

#define TUI_MEMPOOL_SIZE 0

/* Extrac memory size required for TUI driver */
#define TUI_EXTRA_MEM_SIZE (0x200000)

struct tui_mempool {
	void *va;
	unsigned long pa;
	size_t size;
};

static int g_tbuff_alloc;

static __always_unused struct tui_mempool g_tui_mem_pool;

/* basic implementation of a memory pool for TUI framebuffer.  This
 * implementation is using kmalloc, for the purpose of demonstration only.
 * A real implementation might prefer using more advanced allocator, like ION,
 * in order not to exhaust memory available to kmalloc
 */
static bool __always_unused allocate_tui_memory_pool(
	struct tui_mempool *pool, size_t size)
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

static void __always_unused free_tui_memory_pool(struct tui_mempool *pool)
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
uint32_t hal_tui_init(void)
{
	pr_info("%s\n", __func__);

	/* Allocate memory pool for the framebuffer
	 */
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
uint32_t hal_tui_alloc(
	struct tui_alloc_buffer_t allocbuffer[MAX_DCI_BUFFER_NUMBER],
	size_t allocsize, uint32_t number)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;
	phys_addr_t pa = 0;
	unsigned long size = allocsize * number;

	if (!allocbuffer) {
		pr_notice("%s(%d): allocbuffer is null\n", __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	pr_debug("%s(%d): Requested size=0x%zx x %u chunks\n",
		 __func__, __LINE__, allocsize, number);

	if ((size_t)allocsize == 0) {
		pr_notice("%s(%d): Nothing to allocate\n", __func__, __LINE__);
		return TUI_DCI_OK;
	}

	ret = ssmr_offline(&pa, &size, true, SSMR_FEAT_TUI);

	pr_debug("%s(%d): required size 0x%zx, acquired size=0x%lx\n",
		 __func__, __LINE__, allocsize * number, size);

	if (ret == 0) {
		g_tbuff_alloc = 1;
		allocbuffer[0].pa = (uint64_t) pa;
		allocbuffer[1].pa = (uint64_t) (pa + allocsize);
		allocbuffer[2].pa = (uint64_t) (pa + allocsize*2);
		pr_debug("%s(%d): buf_1 0x%llx, buf_2 0x%llx, buf_3 0x%llx, extra=0x%x\n",
			__func__, __LINE__, allocbuffer[0].pa,
			allocbuffer[1].pa, allocbuffer[2].pa,
			TUI_EXTRA_MEM_SIZE);
	} else {
		pr_notice("%s(%d): tui_region_offline failed!\n",
			 __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	return TUI_DCI_OK;
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
	pr_debug("[TUI-HAL] %s\n", __func__);
	if (g_tbuff_alloc) {
		ssmr_online(SSMR_FEAT_TUI);
		g_tbuff_alloc = 0;
	}
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
uint32_t hal_tui_deactivate(void)
{
	int ret = TUI_DCI_OK;
	int __maybe_unused tmp = 0;

	pr_debug("%s+\n", __func__);
	/* Set linux TUI flag */
	trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);
	/*
	 * Stop NWd display here.  After this function returns, SWd will take
	 * control of the display and input.  Therefore the NWd should no longer
	 * access it
	 * This can be done by calling the fb_blank(FB_BLANK_POWERDOWN) function
	 * on the appropriate framebuffer device
	 */

#ifdef TUI_ENABLE_TOUCH
	tpd_enter_tui();
#endif
#ifdef TUI_LOCK_I2C
	i2c_tui_enable_clock(0);
#endif

#ifdef TUI_ENABLE_DISPLAY
	tmp = display_enter_tui();
	if (tmp) {
		pr_notice("[TUI-HAL] %s() failed because display\n", __func__);
		ret = TUI_DCI_ERR_OUT_OF_DISPLAY;
	}
#endif
	trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED|
			   TRUSTEDUI_MODE_INPUT_SECURED);

	pr_debug("[TUI-HAL] %s()\n", __func__);

	return ret;
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
uint32_t hal_tui_activate(void)
{
	pr_info("[TUI-HAL] %s+\n", __func__);
	/* Protect NWd */
	trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|
			     TRUSTEDUI_MODE_INPUT_SECURED);
	/*
	 * Restart NWd display here.  TUI session has ended, and therefore the
	 * SWd will no longer use display and input.
	 * This can be done by calling the fb_blank(FB_BLANK_UNBLANK) function
	 * on the appropriate framebuffer device
	 */
	/* Clear linux TUI flag */
#ifdef TUI_ENABLE_TOUCH
	tpd_exit_tui();
#endif

#ifdef TUI_LOCK_I2C
	i2c_tui_disable_clock(0);
#endif

#ifdef TUI_ENABLE_DISPLAY
	display_exit_tui();
#endif
	trustedui_set_mode(TRUSTEDUI_MODE_OFF);
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
uint32_t hal_tui_process_cmd(struct tui_hal_cmd_t *cmd,
			     struct tui_hal_rsp_t *rsp)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
uint32_t hal_tui_notif(void)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
void hal_tui_post_start(struct tlc_tui_response_t *rsp)
{
	pr_info("%s\n", __func__);
}

int __weak ssmr_offline(phys_addr_t *pa, unsigned long *size, bool is_64bit,
		unsigned int feat)
{
	return -1;
}
int __weak ssmr_online(unsigned int feat)
{
	return -1;
}

int __weak tpd_reregister_from_tui(void)
{
	return 0;
}

int __weak tpd_enter_tui(void)
{
	return 0;
}

int __weak tpd_exit_tui(void)
{
	return 0;
}

int __weak display_enter_tui(void)
{
	return 0;
}

int __weak display_exit_tui(void)
{
	return 0;
}

int __weak i2c_tui_enable_clock(int id)
{
	return 0;
}

int __weak i2c_tui_disable_clock(int id)
{
	return 0;
}
