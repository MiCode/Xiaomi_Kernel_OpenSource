/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#ifndef _TC_DRV_H
#define _TC_DRV_H

/*
 * This contains the hooks for the testchip driver, as used by the Rogue and
 * PDP sub-devices, and the platform data passed to each of their drivers
 */

#include <linux/version.h>
#include <linux/pci.h>
#include <linux/device.h>

/* Valid values for the TC_MEMORY_CONFIG configuration option */
#define TC_MEMORY_LOCAL		1
#define TC_MEMORY_HOST		2
#define TC_MEMORY_HYBRID	3

#if defined(SUPPORT_ION) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))

#include PVR_ANDROID_ION_HEADER

/* NOTE: This should be kept in sync with the user side (in buffer_generic.c) */
#if defined(SUPPORT_RGX)
#define ION_HEAP_TC_ROGUE    (ION_HEAP_TYPE_CUSTOM+1)
#endif
#define ION_HEAP_TC_PDP      (ION_HEAP_TYPE_CUSTOM+2)

#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
#define ION_HEAP_TC_SECURE   (ION_HEAP_TYPE_CUSTOM+3)
#endif

#endif /* defined(SUPPORT_ION) */

#define TC_INTERRUPT_PDP     0
#define TC_INTERRUPT_EXT     1
#define TC_INTERRUPT_TC5_PDP 2
#define TC_INTERRUPT_COUNT   3

int tc_enable(struct device *dev);
void tc_disable(struct device *dev);

int tc_enable_interrupt(struct device *dev, int interrupt_id);
int tc_disable_interrupt(struct device *dev, int interrupt_id);

int tc_set_interrupt_handler(struct device *dev, int interrupt_id,
	void (*handler_function)(void *), void *handler_data);

int tc_sys_info(struct device *dev, u32 *tmp, u32 *pll);
int tc_sys_strings(struct device *dev,
	char *str_fpga_rev, size_t size_fpga_rev, char *str_tcf_core_rev,
	size_t size_tcf_core_rev, char *str_tcf_core_target_build_id,
	size_t size_tcf_core_target_build_id, char *str_pci_ver,
	size_t size_pci_ver, char *str_macro_ver, size_t size_macro_ver);
int tc_core_clock_speed(struct device *dev);

#define APOLLO_DEVICE_NAME_PDP   "apollo_pdp"
#define ODN_DEVICE_NAME_PDP      "odin_pdp"

/* The following structs are initialised and passed down by the parent tc
 * driver to the respective sub-drivers
 */

struct tc_pdp_platform_data {
#if defined(SUPPORT_ION) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	struct ion_device *ion_device;
	int ion_heap_id;
#endif
	resource_size_t memory_base;

	/* The following is used by the drm_pdp driver as it manages the
	 * pdp memory
	 */
	resource_size_t pdp_heap_memory_base;
	resource_size_t pdp_heap_memory_size;

	/* Used to export host address instead of pdp address, depends on the
	 * TC memory mode.
	 *
	 * PDP phys address space is from 0 to end of local device memory,
	 * however if the TC is configured to operate in hybrid mode then the
	 * GPU is configured to match the CPU phys address space view.
	 */
	bool dma_map_export_host_addr;
};

#if defined(SUPPORT_RGX)

#define TC_DEVICE_NAME_ROGUE "tc_rogue"

struct tc_rogue_platform_data {
#if defined(SUPPORT_ION) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	struct ion_device *ion_device;
	int ion_heap_id;
#endif
	/* The testchip memory mode (LOCAL, HOST or HYBRID) */
	int mem_mode;

	/* The base address of the testchip memory (CPU physical address) -
	 * used to convert from CPU-Physical to device-physical addresses
	 */
	resource_size_t tc_memory_base;

	/* The following is used to setup the services heaps that map to the
	 * ion heaps
	 */
	resource_size_t pdp_heap_memory_base;
	resource_size_t pdp_heap_memory_size;
	resource_size_t rogue_heap_memory_base;
	resource_size_t rogue_heap_memory_size;
#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
	resource_size_t secure_heap_memory_base;
	resource_size_t secure_heap_memory_size;
#endif
};

#endif /* defined(SUPPORT_RGX) */

#endif /* _TC_DRV_H */
