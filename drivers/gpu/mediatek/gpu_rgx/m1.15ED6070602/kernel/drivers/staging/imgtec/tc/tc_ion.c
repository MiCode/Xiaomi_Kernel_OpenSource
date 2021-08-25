/*
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this file
 * under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
#include <linux/module.h>
#endif

#include "tc_ion.h"
#include "tc_drv_internal.h"

#include "ion_lma_heap.h"

#if defined(SUPPORT_RGX) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))

#include "ion_fbcdc_clear.h"

static long tc_ion_custom_ioctl(struct ion_client *client,
				unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case ION_IOC_FBCDC_ALLOC:
		return ion_custom_fbcdc_alloc(client, arg);
	default:
		break;
	}
	return -ENOTTY;
}

#else /* defined(SUPPORT_RGX) */

#define tc_ion_custom_ioctl NULL

#endif /* defined(SUPPORT_RGX) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)) */

int tc_ion_init(struct tc_device *tc, int mem_bar)
{
	int i, err = 0;
	struct ion_platform_heap ion_heap_data[TC_ION_HEAP_COUNT] = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
		{
			.type = ION_HEAP_TYPE_SYSTEM,
			.id = ION_HEAP_TYPE_SYSTEM,
			.name = "system",
		},
#else
		/* The system heap is installed by the kernel */
		{ 0 },
#endif
		{
			.type = ION_HEAP_TYPE_CUSTOM,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
			.id = ION_HEAP_TC_PDP,
#endif
			.size = tc->pdp_heap_mem_size,
			.base = tc->pdp_heap_mem_base,
			.name = "tc-pdp",
			.priv = (void *)tc->tc_mem.base, /* offset */
		},
#if defined(SUPPORT_RGX) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
		{
			.type = ION_HEAP_TYPE_CUSTOM,
			.id = ION_HEAP_TC_ROGUE,
			.size = tc->ext_heap_mem_size,
			.base = tc->ext_heap_mem_base,
			.name = "tc-rogue",
		},
#endif
#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
		{
			.type = ION_HEAP_TYPE_CUSTOM,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
			.id = ION_HEAP_TC_SECURE,
#endif
			.size = tc->secure_heap_mem_size,
			.base = tc->secure_heap_mem_base,
			.name = "tc-secure",
			.priv = (void *)tc->tc_mem.base, /* offset */
		},
#endif /* defined(SUPPORT_FAKE_SECURE_ION_HEAP) */
	};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	tc->ion_device = ion_device_create(tc_ion_custom_ioctl);
	if (IS_ERR_OR_NULL(tc->ion_device)) {
		err = PTR_ERR(tc->ion_device);
		goto err_out;
	}
#else
	/*
	 * There is no way to remove heaps, so take a reference on the
	 * module to prevent it being unloaded.
	 */
	dev_info(&tc->pdev->dev,
		 "Adding custom ION heaps. This module cannot be unloaded.\n");

	if (!try_module_get(THIS_MODULE)) {
		dev_err(&tc->pdev->dev,
			"Failed to take module reference\n");
		err = -EBUSY;
		goto err_out;
	}
#endif
	err = request_pci_io_addr(tc->pdev, mem_bar, 0,
		tc->tc_mem.size);
	if (err) {
		dev_err(&tc->pdev->dev,
			"Failed to request tc memory (%d)\n", err);
		goto err_free_device;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	tc->ion_heaps[0] = ion_heap_create(&ion_heap_data[0]);
	if (IS_ERR_OR_NULL(tc->ion_heaps[0])) {
		err = PTR_ERR(tc->ion_heaps[0]);
		tc->ion_heaps[0] = NULL;
		goto err_free_device;
	}
	ion_device_add_heap(tc->ion_device, tc->ion_heaps[0]);
#endif

	for (i = 1; i < TC_ION_HEAP_COUNT; i++) {
		bool allow_cpu_map = true;
#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
		if (ion_heap_data[i].id == ION_HEAP_TC_SECURE)
#else
		if (!strcmp(ion_heap_data[i].name, "tc-secure"))
#endif
			allow_cpu_map = false;
#endif
		tc->ion_heaps[i] = ion_lma_heap_create(&ion_heap_data[i],
			allow_cpu_map);
		if (IS_ERR_OR_NULL(tc->ion_heaps[i])) {
			err = PTR_ERR(tc->ion_heaps[i]);
			tc->ion_heaps[i] = NULL;
			goto err_free_heaps;
		}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
		ion_device_add_heap(tc->ion_device, tc->ion_heaps[i]);
#else
		ion_device_add_heap(tc->ion_heaps[i]);
#endif
	}

	return 0;

err_free_heaps:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	ion_heap_destroy(tc->ion_heaps[0]);
#endif

	for (i = 1; i < TC_ION_HEAP_COUNT; i++) {
		if (!tc->ion_heaps[i])
			break;
		ion_lma_heap_destroy(tc->ion_heaps[i]);
	}

	release_pci_io_addr(tc->pdev, mem_bar,
		tc->tc_mem.base, tc->tc_mem.size);
err_free_device:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	ion_device_destroy(tc->ion_device);
#else
	module_put(THIS_MODULE);
#endif
err_out:
	/* If the ptr was NULL, it is possible that err is 0 in the err path */
	if (err == 0)
		err = -ENOMEM;
	return err;
}

void tc_ion_deinit(struct tc_device *tc, int mem_bar)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	int i = 0;

	ion_device_destroy(tc->ion_device);
	ion_heap_destroy(tc->ion_heaps[0]);
	for (i = 1; i < TC_ION_HEAP_COUNT; i++)
		ion_lma_heap_destroy(tc->ion_heaps[i]);
	release_pci_io_addr(tc->pdev, mem_bar,
		tc->tc_mem.base, tc->tc_mem.size);
#else
	/*
	 * The module reference taken in tc_ion_init should prevent us
	 * getting here.
	 */
	BUG();
#endif
}
