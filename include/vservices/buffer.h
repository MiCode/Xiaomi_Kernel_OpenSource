/*
 * include/vservices/buffer.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file defines simple wrapper types for strings and variable-size buffers
 * that are stored inside Virtual Services message buffers.
 */

#ifndef _VSERVICES_BUFFER_H_
#define _VSERVICES_BUFFER_H_

#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>

struct vs_mbuf;

/**
 * struct vs_string - Virtual Services fixed sized string type
 * @ptr: String pointer
 * @max_size: Maximum length of the string in bytes
 *
 * A handle to a possibly NUL-terminated string stored in a message buffer. If
 * the size of the string equals to max_size, the string is not NUL-terminated.
 * If the protocol does not specify an encoding, the encoding is assumed to be
 * UTF-8. Wide character encodings are not supported by this type; use struct
 * vs_pbuf for wide character strings.
 */
struct vs_string {
	char *ptr;
	size_t max_size;
};

/**
 * vs_string_copyout - Copy a Virtual Services string to a C string buffer.
 * @dest: C string to copy to
 * @src: Virtual Services string to copy from
 * @max_size: Size of the destination buffer, including the NUL terminator.
 *
 * The behaviour is similar to strlcpy(): that is, the copied string
 * is guaranteed not to exceed the specified size (including the NUL
 * terminator byte), and is guaranteed to be NUL-terminated as long as
 * the size is nonzero (unlike strncpy()).
 *
 * The return value is the size of the input string (even if the output was
 * truncated); this is to make truncation easy to detect.
 */
static inline size_t
vs_string_copyout(char *dest, const struct vs_string *src, size_t max_size)
{
	size_t src_len = strnlen(src->ptr, src->max_size);

	if (max_size) {
		size_t dest_len = min(src_len, max_size - 1);

		memcpy(dest, src->ptr, dest_len);
		dest[dest_len] = '\0';
	}
	return src_len;
}

/**
 * vs_string_copyin_len - Copy a C string, up to a given length, into a Virtual
 *                        Services string.
 * @dest: Virtual Services string to copy to
 * @src: C string to copy from
 * @max_size: Maximum number of bytes to copy
 *
 * Returns the number of bytes copied, which may be less than the input
 * string's length.
 */
static inline size_t
vs_string_copyin_len(struct vs_string *dest, const char *src, size_t max_size)
{
	strncpy(dest->ptr, src, min(max_size, dest->max_size));

	return strnlen(dest->ptr, dest->max_size);
}

/**
 * vs_string_copyin - Copy a C string into a Virtual Services string.
 * @dest: Virtual Services string to copy to
 * @src: C string to copy from
 *
 * Returns the number of bytes copied, which may be less than the input
 * string's length.
 */
static inline size_t
vs_string_copyin(struct vs_string *dest, const char *src)
{
	return vs_string_copyin_len(dest, src, dest->max_size);
}

/**
 * vs_string_length - Return the size of the string stored in a Virtual Services
 *                    string.
 * @str: Virtual Service string to get the length of
 */
static inline size_t
vs_string_length(struct vs_string *str)
{
	return strnlen(str->ptr, str->max_size);
}

/**
 * vs_string_dup - Allocate a C string buffer and copy a Virtual Services string
 *                 into it.
 * @str: Virtual Services string to duplicate
 */
static inline char *
vs_string_dup(struct vs_string *str, gfp_t gfp)
{
	size_t len;
	char *ret;

	len = strnlen(str->ptr, str->max_size) + 1;
	ret = kmalloc(len, gfp);
	if (ret)
		vs_string_copyout(ret, str, len);
	return ret;
}

/**
 * vs_string_max_size - Return the maximum size of a Virtual Services string,
 *                      not including the NUL terminator if the lenght of the
 *                      string is equal to max_size.
 *
 * @str Virtual Services string to return the maximum size of.
 *
 * @return The maximum size of the string.
 */
static inline size_t
vs_string_max_size(struct vs_string *str)
{
	return str->max_size;
}

/**
 * struct vs_pbuf - Handle to a variable-size buffered payload.
 * @data: Data buffer
 * @size: Current size of the buffer
 * @max_size: Maximum size of the buffer
 *
 * This is similar to struct vs_string, except that has an explicitly
 * stored size rather than being null-terminated. The functions that
 * return ssize_t all return the new size of the modified buffer, and
 * will return a negative size if the buffer overflows.
 */
struct vs_pbuf {
	void *data;
	size_t size, max_size;
};

/**
 * vs_pbuf_size - Get the size of a pbuf
 * @pbuf: pbuf to get the size of
 */
static inline size_t vs_pbuf_size(const struct vs_pbuf *pbuf)
{
	return pbuf->size;
}

/**
 * vs_pbuf_data - Get the data pointer for a a pbuf
 * @pbuf: pbuf to get the data pointer for
 */
static inline const void *vs_pbuf_data(const struct vs_pbuf *pbuf)
{
	return pbuf->data;
}

/**
 * vs_pbuf_resize - Resize a pbuf
 * @pbuf: pbuf to resize
 * @size: New size
 */
static inline ssize_t vs_pbuf_resize(struct vs_pbuf *pbuf, size_t size)
{
	if (size > pbuf->max_size)
		return -EOVERFLOW;

	pbuf->size = size;
	return size;
}

/**
 * vs_pbuf_copyin - Copy data into a pbuf
 * @pbuf: pbuf to copy data into
 * @offset: Offset to copy data to
 * @data: Pointer to data to copy into the pbuf
 * @nbytes: Number of bytes to copy into the pbuf
 */
static inline ssize_t vs_pbuf_copyin(struct vs_pbuf *pbuf, off_t offset,
		const void *data, size_t nbytes)
{
	if (offset + nbytes > pbuf->size)
		return -EOVERFLOW;

	memcpy(pbuf->data + offset, data, nbytes);

	return nbytes;
}

/**
 * vs_pbuf_append - Append data to a pbuf
 * @pbuf: pbuf to append to
 * @data: Pointer to data to append to the pbuf
 * @nbytes: Number of bytes to append
 */
static inline ssize_t vs_pbuf_append(struct vs_pbuf *pbuf,
		const void *data, size_t nbytes)
{
	if (pbuf->size + nbytes > pbuf->max_size)
		return -EOVERFLOW;

	memcpy(pbuf->data + pbuf->size, data, nbytes);
	pbuf->size += nbytes;

	return pbuf->size;
}

/**
 * vs_pbuf_dup_string - Duplicate the contents of a pbuf as a C string. The
 * string is allocated and must be freed using kfree.
 * @pbuf: pbuf to convert
 * @gfp_flags: GFP flags for the string allocation
 */
static inline char *vs_pbuf_dup_string(struct vs_pbuf *pbuf, gfp_t gfp_flags)
{
	return kstrndup(pbuf->data, pbuf->size, gfp_flags);
}

#endif /* _VSERVICES_BUFFER_H_ */
