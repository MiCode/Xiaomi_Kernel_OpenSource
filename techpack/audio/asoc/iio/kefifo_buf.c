// SPDX-License-Identifier: GPL-2.0-only
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>
#include <linux/mutex.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/buffer_impl.h>
#include <linux/sched.h>
#include <linux/poll.h>

struct iio_kefifo {
	struct iio_buffer buffer;
	struct kfifo kf;
	struct mutex user_lock;
	int update_needed;
};

#define iio_to_kefifo(r) container_of(r, struct iio_kefifo, buffer)

static inline int __iio_allocate_kefifo(struct iio_kefifo *buf,
			size_t bytes_per_datum, unsigned int length)
{
	if ((length == 0) || (bytes_per_datum == 0))
		return -EINVAL;

	/*
	 * Make sure we don't overflow an unsigned int after kfifo rounds up to
	 * the next power of 2.
	 */
	if (roundup_pow_of_two(length) > UINT_MAX / bytes_per_datum)
		return -EINVAL;

	return __kfifo_alloc((struct __kfifo *)&buf->kf, length,
			     bytes_per_datum, GFP_KERNEL);
}

static int iio_request_update_kefifo(struct iio_buffer *r)
{
	int ret = 0;
	struct iio_kefifo *buf = iio_to_kefifo(r);

	mutex_lock(&buf->user_lock);
	if (buf->update_needed) {
		kfifo_free(&buf->kf);
		ret = __iio_allocate_kefifo(buf, buf->buffer.bytes_per_datum,
				   buf->buffer.length);
		if (ret >= 0)
			buf->update_needed = false;
	} else {
		kfifo_reset_out(&buf->kf);
	}
	mutex_unlock(&buf->user_lock);

	return ret;
}

static int iio_mark_update_needed_kefifo(struct iio_buffer *r)
{
	struct iio_kefifo *kf = iio_to_kefifo(r);
	kf->update_needed = true;
	return 0;
}

static int iio_set_bytes_per_datum_kefifo(struct iio_buffer *r, size_t bpd)
{
	if (r->bytes_per_datum != bpd) {
		r->bytes_per_datum = bpd;
		iio_mark_update_needed_kefifo(r);
	}
	return 0;
}

static int iio_set_length_kefifo(struct iio_buffer *r, unsigned int length)
{
	/* Avoid an invalid state */
	if (length < 2)
		length = 2;
	if (r->length != length) {
		r->length = length;
		iio_mark_update_needed_kefifo(r);
	}
	return 0;
}

static int iio_store_to_kefifo(struct iio_buffer *r,
			      const void *data)
{
	int ret;
	struct iio_kefifo *kf = iio_to_kefifo(r);
	ret = kfifo_in(&kf->kf, data, 1);
	if (ret != 1)
		return -EBUSY;
	return 0;
}

static int iio_read_first_n_kefifo(struct iio_buffer *r,
			   size_t n, char __user *buf)
{
	int ret, copied;
	struct iio_kefifo *kf = iio_to_kefifo(r);

	if (mutex_lock_interruptible(&kf->user_lock))
		return -ERESTARTSYS;

	if (!kfifo_initialized(&kf->kf) || n < kfifo_esize(&kf->kf))
		ret = -EINVAL;
	else
		ret = kfifo_to_user(&kf->kf, buf, n, &copied);
	mutex_unlock(&kf->user_lock);
	if (ret < 0)
		return ret;

	return copied;
}

static size_t iio_kefifo_buf_data_available(struct iio_buffer *r)
{
	struct iio_kefifo *kf = iio_to_kefifo(r);
	size_t samples;

	mutex_lock(&kf->user_lock);
	samples = kfifo_len(&kf->kf);
	mutex_unlock(&kf->user_lock);

	return samples;
}

static void iio_kefifo_buffer_release(struct iio_buffer *buffer)
{
	struct iio_kefifo *kf = iio_to_kefifo(buffer);

	mutex_destroy(&kf->user_lock);
	kfifo_free(&kf->kf);
	kfree(kf);
}

static const struct iio_buffer_access_funcs kefifo_access_funcs = {
	.store_to = &iio_store_to_kefifo,
	.read_first_n = &iio_read_first_n_kefifo,
	.data_available = iio_kefifo_buf_data_available,
	.request_update = &iio_request_update_kefifo,
	.set_bytes_per_datum = &iio_set_bytes_per_datum_kefifo,
	.set_length = &iio_set_length_kefifo,
	.release = &iio_kefifo_buffer_release,

	.modes = INDIO_BUFFER_SOFTWARE | INDIO_BUFFER_TRIGGERED,
};

struct iio_buffer *iio_kefifo_allocate(void)
{
	struct iio_kefifo *kf;

	kf = kzalloc(sizeof(*kf), GFP_KERNEL);
	if (!kf)
		return NULL;

	kf->update_needed = true;
	iio_buffer_init(&kf->buffer);
	kf->buffer.access = &kefifo_access_funcs;
	kf->buffer.length = 2;
	mutex_init(&kf->user_lock);

	return &kf->buffer;
}

void iio_kefifo_free(struct iio_buffer *r)
{
	iio_buffer_put(r);
}



MODULE_LICENSE("GPL");
