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
/*************************************************************************/ /*!
 @Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 @License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef ION_FBCDC_ALLOC_H
#define ION_FBCDC_ALLOC_H

#define ION_IOC_FBCDC_ALLOC 1

struct ion_fbcdc_alloc_data {
	/* in */
	size_t len;
	size_t align;
	unsigned int heap_id_mask;
	unsigned int flags;
	size_t tiles;
	/* out */
	int handle;
};

#if !defined(__KERNEL__)
static
int ion_fbcdc_alloc(int fd, size_t len, size_t align, unsigned int heap_mask,
					unsigned int flags, size_t tiles, int *handlefd) __attribute__((unused));

static
int ion_fbcdc_alloc(int fd, size_t len, size_t align, unsigned int heap_mask,
					unsigned int flags, size_t tiles, int *handlefd)
{
	int err;
	struct ion_fbcdc_alloc_data payload = {
		.len = len,
		.align = align,
		.heap_id_mask = heap_mask,
		.flags = flags,
		.tiles = tiles,
	};
	struct ion_custom_data data = {
		.cmd = ION_IOC_FBCDC_ALLOC,
		.arg = (unsigned long)&payload,
	};

	if (handlefd == NULL)
		return -EINVAL;

	err = ioctl(fd, ION_IOC_CUSTOM, &data);
	if (err < 0)
		return err;

	/* The handle returned is a shared dma_buf.*/ 
	*handlefd = payload.handle;

	return err;
}

static int ion_custom_alloc(int fd, size_t len, size_t align, unsigned int heap_mask,
							unsigned int flags, size_t tiles, int *handlefd)
{

#if defined(PVR_ANDROID_ION_FBCDC_ALLOC)
	return ion_fbcdc_alloc(fd, len, align, heap_mask, flags, tiles, handlefd);
#else /* defined(PVR_ANDROID_ION_FBCDC_ALLOC) */
	PVR_UNREFERENCED_PARAMETER(tiles);
	return ion_alloc_fd(fd, len, align, heap_mask, flags, handlefd);
#endif /* defined(PVR_ANDROID_ION_FBCDC_ALLOC) */
}
#endif /* !defined(__KERNEL__) */

#endif /* ION_FBCDC_ALLOC_H */
