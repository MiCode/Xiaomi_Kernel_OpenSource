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

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
#include "ion_fbcdc_clear.h"

#if defined(SUPPORT_RGX)
#include "rgxdefs_km.h"
#endif /* defined(SUPPORT_RGX) */

#include <linux/uaccess.h>
#include <linux/err.h>

#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
#define ION_HEAP_TC_SECURE   (ION_HEAP_TYPE_CUSTOM+3)
#endif

long ion_custom_fbcdc_alloc(struct ion_client *client, unsigned long arg)
{
	struct ion_fbcdc_alloc_data data;
	struct ion_handle *handle;
	long err = 0;

	if (copy_from_user(&data, (void __user *)arg,
					   sizeof(data)))
		return -EFAULT;

	/* Note: The underlying ion allocation should not clear the memory.
	 * Otherwise this will be done twice which is a waste of processing power. */
	handle = ion_alloc(client, data.len, data.align, data.heap_id_mask, data.flags);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
	if (data.heap_id_mask != ION_HEAP_TC_SECURE)
#endif /* defined(SUPPORT_FAKE_SECURE_ION_HEAP) */
	{
		void *paddr = ion_map_kernel(client, handle);

		if (IS_ERR(handle)) {
			err = PTR_ERR(paddr);
			goto err_free;
		}

#if defined(SUPPORT_RGX) && defined(RGX_FEATURE_FBCDC_ARCHITECTURE)
		if (data.tiles > 0) {
#if (RGX_FEATURE_FBCDC_ARCHITECTURE == 1)
			/* Note: This only works in direct mode (32 bit header) */
			int j;
			u32 *pu32addr = paddr;
			size_t tiles = (data.tiles + 31) & ~31;
			/* Write the header first */
			for (j = 0; j < tiles; j++)
				pu32addr[j] = 0x04104101;
			/* Clear the data region to black */
			memset(&pu32addr[tiles], 0, data.len - tiles * sizeof(*pu32addr));
#elif (RGX_FEATURE_FBCDC_ARCHITECTURE == 2)
			/* Note: This only works in direct mode (8 bit header) */
			u8 *pu8addr = paddr;
			size_t tiles = (data.tiles + 127) & ~127;
			/* Write the header first */
			memset(pu8addr, 0xc7, tiles * sizeof(*pu8addr));
			/* Clear the data region to black */
			memset(&pu8addr[tiles], 0, data.len - tiles * sizeof(*pu8addr));
#else
#warning "Clearing of buffers not implemented for given FBCDC architecture"
			memset(paddr, 0, data.len);
#endif
		} else
#endif /* defined(SUPPORT_RGX) && defined(RGX_FEATURE_FBCDC_ARCHITECTURE) */
			memset(paddr, 0, data.len);

		ion_unmap_kernel(client, handle);
	}

	/* We don't have access to the id member of handle, so return a shared
	 * dma_buf which gets reimported to ion by userspace. */
	data.handle = ion_share_dma_buf_fd(client, handle);
	if (data.handle < 0) {
		err = data.handle;
		goto err_free;
	}

	if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
		err = -EFAULT;
		goto err_free;
	}

err_free:
	ion_free(client, handle);
	return err;
}
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)) */
