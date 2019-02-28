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
					unsigned int flags, size_t tiles, ion_user_handle_t *handle) __attribute__((unused));

static
int ion_fbcdc_alloc(int fd, size_t len, size_t align, unsigned int heap_mask,
					unsigned int flags, size_t tiles, ion_user_handle_t *handle)
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
	struct ion_fd_data fd_data;

	if (handle == NULL)
		return -EINVAL;

	err = ioctl(fd, ION_IOC_CUSTOM, &data);
	if (err < 0)
		return err;

	/* The handle returned is a shared dma_buf. Reimport it to get an ion
	 * handle */
	fd_data.fd = payload.handle;
	err = ioctl(fd, ION_IOC_IMPORT, &fd_data);
	if (err < 0)
		return err;

	close(fd_data.fd);

	*handle = fd_data.handle;

	return err;
}

static int ion_custom_alloc(int fd, size_t len, size_t align, unsigned int heap_mask,
							unsigned int flags, size_t tiles, ion_user_handle_t *handle)
{

#if defined(PVR_ANDROID_ION_FBCDC_ALLOC)
	return ion_fbcdc_alloc(fd, len, align, heap_mask, flags, tiles, handle);
#else /* defined(PVR_ANDROID_ION_FBCDC_ALLOC) */
	PVR_UNREFERENCED_PARAMETER(tiles);
	return ion_alloc(fd, len, align, heap_mask, flags, handle);
#endif /* defined(PVR_ANDROID_ION_FBCDC_ALLOC) */
}
#endif /* !defined(__KERNEL__) */

#endif /* ION_FBCDC_ALLOC_H */
