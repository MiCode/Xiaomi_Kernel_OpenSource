/*
 *  core.c - compress offload core
 *
 *  Copyright (C) 2011 Intel Corporation
 *  Authors:	Vinod Koul <vinod.koul@linux.intel.com>
 *		Pierre-Louis Bossart <pierre-louis.bossart@linux.intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/uio.h>
#include <linux/uaccess.h>
#include <sound/snd_compress_params.h>
#include <sound/compress_offload.h>
#include <sound/compress_driver.h>

/* TODO:
 * - Integrate with alsa, compressed devices should register as alsa devices
 *	as /dev/snd_compr_xxx
 * - Integrate with ASoC:
 *	Opening compressed path should also start the codec dai
 *   TBD how the cpu dai will be viewed and started.
 *	ASoC should always be optional part
 *	(we should be able to use this framework in non asoc systems
 * - Multiple node representation
 *	driver should be able to register multiple nodes
 * - Version numbering for API
 */

static DEFINE_MUTEX(device_mutex);
static LIST_HEAD(device_list);
static LIST_HEAD(misc_list);

/*
 * currently we are using misc device for registration and exposing ioctls
 * this is temporary and will be moved to snd
 * the device should be registered as /dev/snd_compr.....
 */

struct snd_compr_misc {
	struct miscdevice misc;
	struct list_head list;
	struct snd_compr *compr;
};

struct snd_ioctl_data {
	struct snd_compr_misc *misc;
	unsigned long caps;
	unsigned int minor;
	struct snd_compr_stream stream;
};

static struct snd_compr_misc *snd_compr_get_device(unsigned int minor)
{
	struct snd_compr_misc *misc;

	list_for_each_entry(misc, &misc_list, list) {
		if (minor == misc->misc.minor)
			return misc;
	}
	return NULL;
}

static int snd_compr_open(struct inode *inode, struct file *f)
{
	unsigned int minor = iminor(inode);
	struct snd_compr_misc *misc = snd_compr_get_device(minor);
	struct snd_ioctl_data *data;
	struct snd_compr_runtime *runtime;
	unsigned int direction;
	int ret;

	mutex_lock(&device_mutex);
	if (f->f_flags & O_WRONLY)
		direction = SNDRV_PCM_STREAM_PLAYBACK;
	else {
		ret = -ENXIO;
		goto out;
	}
	/* curently only encoded playback is supported, above needs to be
	 * removed once we have recording support */

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto out;
	}
	data->misc = misc;
	data->minor = minor;
	data->stream.ops = misc->compr->ops;
	data->stream.direction = direction;
	data->stream.private_data = misc->compr->private_data;
	data->stream.device = misc->compr;
	runtime = kzalloc(sizeof(*runtime), GFP_KERNEL);
	if (!runtime) {
		ret = -ENOMEM;
		kfree(data);
		goto out;
	}
	runtime->state = SNDRV_PCM_STATE_OPEN;
	init_waitqueue_head(&runtime->sleep);
	data->stream.runtime = runtime;
	f->private_data = (void *)data;
	ret = misc->compr->ops->open(&data->stream);
	if (ret) {
		kfree(runtime);
		kfree(data);
		goto out;
	}
out:
	mutex_unlock(&device_mutex);
	return ret;
}

static int snd_compr_free(struct inode *inode, struct file *f)
{
	struct snd_ioctl_data *data = f->private_data;
	mutex_lock(&device_mutex);
	data->stream.ops->free(&data->stream);
	kfree(data->stream.runtime->buffer);
	kfree(data->stream.runtime);
	kfree(data);
	mutex_unlock(&device_mutex);
	return 0;
}

static void snd_compr_update_tstamp(struct snd_compr_stream *stream,
		struct snd_compr_tstamp *tstamp)
{
	stream->ops->pointer(stream, tstamp);
	stream->runtime->hw_pointer = tstamp->copied_bytes;
}

static size_t snd_compr_calc_avail(struct snd_compr_stream *stream,
		struct snd_compr_avail *avail)
{
	size_t avail_calc;

	snd_compr_update_tstamp(stream, &avail->tstamp);
	avail_calc = stream->runtime->app_pointer - stream->runtime->hw_pointer;
	if (avail_calc < 0)
		avail_calc = stream->runtime->buffer_size + avail_calc;
	avail->avail = avail_calc;
	return avail_calc;
}

static size_t snd_compr_get_avail(struct snd_compr_stream *stream)
{
	struct snd_compr_avail avail;

	return snd_compr_calc_avail(stream, &avail);
}

static int
snd_compr_ioctl_avail(struct snd_compr_stream *stream, unsigned long arg)
{
	struct snd_compr_avail ioctl_avail;

	snd_compr_calc_avail(stream, &ioctl_avail);

	if (copy_to_user((unsigned long __user *)arg, &ioctl_avail, sizeof(ioctl_avail)))
		return -EFAULT;
	return 0;
}

static int snd_compr_write_data(struct snd_compr_stream *stream,
	       const char __user *buf, size_t count)
{
	void *dstn;
	size_t copy;

	dstn = stream->runtime->buffer + stream->runtime->app_pointer;
	if (count < stream->runtime->buffer_size - stream->runtime->app_pointer) {
		if (copy_from_user(dstn, buf, count))
			return -EFAULT;
		stream->runtime->app_pointer += count;
	} else {
		copy = stream->runtime->buffer_size - stream->runtime->app_pointer;
		if (copy_from_user(dstn, buf, copy))
			return -EFAULT;
		if (copy_from_user(stream->runtime->buffer, buf + copy, count - copy))
			return -EFAULT;
		stream->runtime->app_pointer = count - copy;
	}
	/* if DSP cares, let it know data has been written */
	if (stream->ops->ack)
		stream->ops->ack(stream);
	return count;
}

static ssize_t snd_compr_write(struct file *f, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct snd_ioctl_data *data = f->private_data;
	struct snd_compr_stream *stream;
	size_t avail;
	int retval;

	BUG_ON(!data);
	stream = &data->stream;
	mutex_lock(&stream->device->lock);
	/* write is allowed when stream is running or has been steup */
	if (stream->runtime->state != SNDRV_PCM_STATE_SETUP &&
			stream->runtime->state != SNDRV_PCM_STATE_RUNNING) {
		mutex_unlock(&stream->device->lock);
		return -EPERM;
	}

	avail = snd_compr_get_avail(stream);
	/* calculate how much we can write to buffer */
	if (avail > count)
		avail = count;

	if (stream->ops->copy)
		retval = stream->ops->copy(stream, buf, avail);
	else
		retval = snd_compr_write_data(stream, buf, avail);

	/* while initiating the stream, write should be called before START
	 * call, so in setup move state */
	if (stream->runtime->state == SNDRV_PCM_STATE_SETUP)
		stream->runtime->state = SNDRV_PCM_STATE_PREPARED;

	mutex_unlock(&stream->device->lock);
	return retval;
}


static ssize_t snd_compr_read(struct file *f, char __user *buf,
		size_t count, loff_t *offset)
{
	return -ENXIO;
}

static int snd_compr_mmap(struct file *f, struct vm_area_struct *vma)
{
	return -ENXIO;
}

unsigned int snd_compr_poll(struct file *f, poll_table *wait)
{
	struct snd_ioctl_data *data = f->private_data;
	struct snd_compr_stream *stream;
	int retval = 0;

	BUG_ON(!data);
	stream = &data->stream;

	mutex_lock(&stream->device->lock);
	if (stream->runtime->state != SNDRV_PCM_STATE_RUNNING) {
		retval = -ENXIO;
		goto out;
	}
	poll_wait(f, &stream->runtime->sleep, wait);

	/* this would change after read is implemented, we would need to
	 * check for direction here */
	if (stream->runtime->state != SNDRV_PCM_STATE_RUNNING)
		retval = POLLOUT | POLLWRNORM;
out:
	mutex_unlock(&stream->device->lock);
	return retval;
}

void snd_compr_fragment_elapsed(struct snd_compr_stream *stream)
{
	size_t avail;

	if (stream->direction !=  SNDRV_PCM_STREAM_PLAYBACK)
		return;
	avail = snd_compr_get_avail(stream);
	if (avail >= stream->runtime->fragment_size)
		wake_up(&stream->runtime->sleep);
}
EXPORT_SYMBOL_GPL(snd_compr_fragment_elapsed);

void snd_compr_frame_elapsed(struct snd_compr_stream *stream)
{
	size_t avail;

	if (stream->direction !=  SNDRV_PCM_STREAM_CAPTURE)
		return;
	avail = snd_compr_get_avail(stream);
	if (avail)
		wake_up(&stream->runtime->sleep);
}
EXPORT_SYMBOL_GPL(snd_compr_frame_elapsed);

static int snd_compr_get_caps(struct snd_compr_stream *stream, unsigned long arg)
{
	int retval;
	struct snd_compr_caps caps;

	if (!stream->ops->get_caps)
		return -ENXIO;

	retval = stream->ops->get_caps(stream, &caps);
	if (retval)
		goto out;
	if (copy_to_user((void __user *)arg, &caps, sizeof(caps)))
		retval = -EFAULT;
out:
	return retval;
}

static int snd_compr_get_codec_caps(struct snd_compr_stream *stream, unsigned long arg)
{
	int retval;
	struct snd_compr_codec_caps *caps;

	if (!stream->ops->get_codec_caps)
		return -ENXIO;

	caps = kmalloc(sizeof(*caps), GFP_KERNEL);
	if (!caps)
		return -ENOMEM;

	retval = stream->ops->get_codec_caps(stream, caps);
	if (retval)
		goto out;
	if (copy_to_user((void __user *)arg, caps, sizeof(*caps)))
		retval = -EFAULT;

out:
	kfree(caps);
	return retval;
}

/* revisit this with snd_pcm_preallocate_xxx */
static int snd_compr_allocate_buffer(struct snd_compr_stream *stream,
		struct snd_compr_params *params)
{
	unsigned int buffer_size;
	void *buffer;

	buffer_size = params->buffer.fragment_size * params->buffer.fragments;
	if (stream->ops->copy) {
		buffer = NULL;
		/* if copy is defined the driver will be required to copy
		 * the data from core
		 */
	} else {
		buffer = kmalloc(buffer_size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
	}
	stream->runtime->fragment_size = params->buffer.fragment_size;
	stream->runtime->fragments = params->buffer.fragments;
	stream->runtime->buffer = buffer;
	stream->runtime->buffer_size = buffer_size;
	return 0;
}

static int snd_compr_set_params(struct snd_compr_stream *stream, unsigned long arg)
{
	struct snd_compr_params *params;
	int retval;

	if (stream->runtime->state == SNDRV_PCM_STATE_OPEN) {
		/*
		 * we should allow parameter change only when stream has been
		 * opened not in other cases
		 */
		params = kmalloc(sizeof(*params), GFP_KERNEL);
		if (!params)
			return -ENOMEM;
		if (copy_from_user(params, (void __user *)arg, sizeof(*params)))
			return -EFAULT;
		retval = snd_compr_allocate_buffer(stream, params);
		if (retval) {
			kfree(params);
			return -ENOMEM;
		}
		retval = stream->ops->set_params(stream, params);
		if (retval)
			goto out;
		stream->runtime->state = SNDRV_PCM_STATE_SETUP;
	} else
		return -EPERM;
out:
	kfree(params);
	return retval;
}

static int snd_compr_get_params(struct snd_compr_stream *stream, unsigned long arg)
{
	struct snd_compr_params *params;
	int retval;

	if (!stream->ops->get_params)
		return -ENXIO;

	params = kmalloc(sizeof(*params), GFP_KERNEL);
	if (!params)
		return -ENOMEM;
	retval = stream->ops->get_params(stream, params);
	if (retval)
		goto out;
	if (copy_to_user((char __user *)arg, params, sizeof(*params)))
		retval = -EFAULT;

out:
	kfree(params);
	return retval;
}

static int snd_compr_tstamp(struct snd_compr_stream *stream, unsigned long arg)
{
	struct snd_compr_tstamp tstamp;

	snd_compr_update_tstamp(stream, &tstamp);
	if (copy_to_user((struct snd_compr_tstamp __user *)arg, &tstamp, sizeof(tstamp)))
		return -EFAULT;
	return 0;
}

static int snd_compr_pause(struct snd_compr_stream *stream)
{
	int retval;

	if (stream->runtime->state == SNDRV_PCM_STATE_PAUSED)
		return 0;
	retval = stream->ops->trigger(stream, SNDRV_PCM_TRIGGER_PAUSE_PUSH);
	if (!retval) {
		stream->runtime->state = SNDRV_PCM_STATE_PAUSED;
		wake_up(&stream->runtime->sleep);
	}
	return retval;
}

static int snd_compr_resume(struct snd_compr_stream *stream)
{
	int retval;

	if (stream->runtime->state != SNDRV_PCM_STATE_PAUSED)
		return -EPERM;
	retval = stream->ops->trigger(stream, SNDRV_PCM_TRIGGER_PAUSE_RELEASE);
	if (!retval)
		stream->runtime->state = SNDRV_PCM_STATE_RUNNING;
	return retval;
}

static int snd_compr_start(struct snd_compr_stream *stream)
{
	int retval;

	if (stream->runtime->state != SNDRV_PCM_STATE_PREPARED)
		return -EPERM;
	retval = stream->ops->trigger(stream, SNDRV_PCM_TRIGGER_START);
	if (!retval)
		stream->runtime->state = SNDRV_PCM_STATE_RUNNING;
	return retval;
}

static int snd_compr_stop(struct snd_compr_stream *stream)
{
	int retval;

	if (stream->runtime->state != SNDRV_PCM_STATE_PREPARED)
		return -EPERM;
	retval = stream->ops->trigger(stream, SNDRV_PCM_TRIGGER_STOP);
	if (!retval) {
		stream->runtime->state = SNDRV_PCM_STATE_SETUP;
		wake_up(&stream->runtime->sleep);
	}
	return retval;
}

static int snd_compr_drain(struct snd_compr_stream *stream)
{
	int retval;

	if (stream->runtime->state != SNDRV_PCM_STATE_PREPARED ||
			stream->runtime->state != SNDRV_PCM_STATE_PAUSED)
		return -EPERM;
	retval = stream->ops->trigger(stream, SND_COMPR_TRIGGER_DRAIN);
	if (!retval) {
		stream->runtime->state = SNDRV_PCM_STATE_SETUP;
		wake_up(&stream->runtime->sleep);
	}
	return retval;
}

static long snd_compr_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct snd_ioctl_data *data = f->private_data;
	struct snd_compr_stream *stream;
	int retval = -ENOTTY;

	BUG_ON(!data);
	stream = &data->stream;
	mutex_lock(&stream->device->lock);
	switch (_IOC_NR(cmd)) {
	case _IOC_NR(SNDRV_COMPRESS_GET_CAPS):
		retval = snd_compr_get_caps(stream, arg);
		break;
	case _IOC_NR(SNDRV_COMPRESS_GET_CODEC_CAPS):
		retval = snd_compr_get_codec_caps(stream, arg);
		break;
	case _IOC_NR(SNDRV_COMPRESS_SET_PARAMS):
		retval = snd_compr_set_params(stream, arg);
		break;
	case _IOC_NR(SNDRV_COMPRESS_GET_PARAMS):
		retval = snd_compr_get_params(stream, arg);
		break;
	case _IOC_NR(SNDRV_COMPRESS_TSTAMP):
		retval = snd_compr_tstamp(stream, arg);
		break;
	case _IOC_NR(SNDRV_COMPRESS_AVAIL):
		retval = snd_compr_ioctl_avail(stream, arg);
	case _IOC_NR(SNDRV_COMPRESS_PAUSE):
		retval = snd_compr_pause(stream);
		break;
	case _IOC_NR(SNDRV_COMPRESS_RESUME):
		retval = snd_compr_resume(stream);
		break;
	case _IOC_NR(SNDRV_COMPRESS_START):
		retval = snd_compr_start(stream);
		break;
	case _IOC_NR(SNDRV_COMPRESS_STOP):
		retval = snd_compr_stop(stream);
		break;
	case _IOC_NR(SNDRV_COMPRESS_DRAIN):
		cmd = SND_COMPR_TRIGGER_DRAIN;
		retval = snd_compr_drain(stream);
		break;
	}
	mutex_unlock(&stream->device->lock);
	return retval;
}

static const struct file_operations snd_comp_file = {
	.owner =	THIS_MODULE,
	.open =		snd_compr_open,
	.release =	snd_compr_free,
	.read =		snd_compr_read,
	.write =	snd_compr_write,
	.unlocked_ioctl = snd_compr_ioctl,
	.mmap =		snd_compr_mmap,
	.poll =		snd_compr_poll,
};

static int snd_compress_add_device(struct snd_compr *device)
{
	int ret;

	struct snd_compr_misc *misc = kzalloc(sizeof(*misc), GFP_KERNEL);

	misc->misc.name = device->name;
	misc->misc.fops = &snd_comp_file;
	misc->misc.minor = MISC_DYNAMIC_MINOR;
	misc->compr = device;
	ret = misc_register(&misc->misc);
	if (ret) {
		pr_err("couldn't register misc device\n");
		kfree(misc);
	} else {
		pr_debug("Got minor %d\n", misc->misc.minor);
		list_add_tail(&misc->list, &misc_list);
	}
	return ret;
}

static int snd_compress_remove_device(struct snd_compr *device)
{
	struct snd_compr_misc *misc, *__misc;

	list_for_each_entry_safe(misc, __misc, &misc_list, list) {
		if (device == misc->compr) {
			misc_deregister(&misc->misc);
			list_del(&device->list);
			kfree(misc);
		}
	}
	return 0;
}
/**
 * snd_compress_register - register compressed device
 *
 * @device: compressed device to register
 */
int snd_compress_register(struct snd_compr *device)
{
	int retval;

	if (device->name == NULL || device->dev == NULL || device->ops == NULL)
		return -EINVAL;
	BUG_ON(!device->ops->open);
	BUG_ON(!device->ops->free);
	BUG_ON(!device->ops->set_params);
	BUG_ON(!device->ops->get_params);
	BUG_ON(!device->ops->trigger);
	BUG_ON(!device->ops->pointer);
	BUG_ON(!device->ops->get_caps);
	BUG_ON(!device->ops->get_codec_caps);

	INIT_LIST_HEAD(&device->list);
	/* todo register the compressed streams */
	/* todo integrate with asoc */

	/* register a compressed card  TBD if this needs change */

	pr_debug("Registering compressed device %s\n", device->name);
	mutex_lock(&device_mutex);
	/*  register a msic device for now */
	retval = snd_compress_add_device(device);
	if (!retval)
		list_add_tail(&device->list, &device_list);
	mutex_unlock(&device_mutex);
	return retval;
}
EXPORT_SYMBOL_GPL(snd_compress_register);

int snd_compress_deregister(struct snd_compr *device)
{
	pr_debug("Removing compressed device %s\n", device->name);
	mutex_lock(&device_mutex);
	snd_compress_remove_device(device);
	list_del(&device->list);
	mutex_unlock(&device_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_compress_deregister);

static int __init snd_compress_init(void)
{
	return 0;
}

static void __exit snd_compress_exit(void)
{
}

module_init(snd_compress_init);
module_exit(snd_compress_exit);
