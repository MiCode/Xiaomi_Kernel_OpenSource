/*
 * drivers/misc/tegra-profiler/comm.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/bitops.h>
#include <linux/err.h>

#include <asm/uaccess.h>

#include <linux/tegra_profiler.h>

#include "comm.h"
#include "version.h"

struct quadd_comm_ctx comm_ctx;

static inline void *rb_alloc(unsigned long size)
{
	return vmalloc(size);
}

static inline void rb_free(void *addr)
{
	vfree(addr);
}

static void rb_reset(struct quadd_ring_buffer *rb)
{
	rb->pos_read = 0;
	rb->pos_write = 0;
	rb->fill_count = 0;
	rb->max_fill_count = 0;
}

static int rb_init(struct quadd_ring_buffer *rb, size_t size)
{
	spin_lock_init(&rb->lock);

	rb->size = size;
	rb->buf = NULL;

	rb->buf = (char *) rb_alloc(rb->size);
	if (!rb->buf) {
		pr_err("Ring buffer alloc error\n");
		return -ENOMEM;
	}
	pr_info("rb: data buffer size: %u\n", (unsigned int)rb->size);

	rb_reset(rb);

	return 0;
}

static void rb_deinit(struct quadd_ring_buffer *rb)
{
	unsigned long flags;

	spin_lock_irqsave(&rb->lock, flags);
	if (rb->buf) {
		rb_reset(rb);

		rb_free(rb->buf);
		rb->buf = NULL;
	}
	spin_unlock_irqrestore(&rb->lock, flags);
}

static __attribute__((unused)) int rb_is_full(struct quadd_ring_buffer *rb)
{
	return rb->fill_count == rb->size;
}

static int rb_is_empty(struct quadd_ring_buffer *rb)
{
	return rb->fill_count == 0;
}

static int rb_is_empty_lock(struct quadd_ring_buffer *rb)
{
	int res;
	unsigned long flags;

	spin_lock_irqsave(&rb->lock, flags);
	res = rb->fill_count == 0;
	spin_unlock_irqrestore(&rb->lock, flags);

	return res;
}

static size_t
rb_get_free_space(struct quadd_ring_buffer *rb)
{
	return rb->size - rb->fill_count;
}

static size_t
rb_write(struct quadd_ring_buffer *rb, char *data, size_t length)
{
	size_t new_pos_write, chunk1;

	if (length > rb_get_free_space(rb))
		return 0;

	new_pos_write = (rb->pos_write + length) % rb->size;

	if (new_pos_write < rb->pos_write) {
		chunk1 = rb->size - rb->pos_write;
		memcpy(rb->buf + rb->pos_write, data, chunk1);
		if (new_pos_write > 0)
			memcpy(rb->buf, data + chunk1, new_pos_write);
	} else {
		memcpy(rb->buf + rb->pos_write, data, length);
	}

	rb->pos_write = new_pos_write;
	rb->fill_count += length;

	return length;
}

static ssize_t rb_read_undo(struct quadd_ring_buffer *rb, size_t length)
{
	if (rb_get_free_space(rb) < length)
		return -EIO;

	if (rb->pos_read > length)
		rb->pos_read -= length;
	else
		rb->pos_read += rb->size - length;

	rb->fill_count += sizeof(struct quadd_record_data);
	return length;
}

static size_t rb_read(struct quadd_ring_buffer *rb, char *data, size_t length)
{
	unsigned int new_pos_read, chunk1;

	if (length > rb->fill_count)
		return 0;

	new_pos_read = (rb->pos_read + length) % rb->size;

	if (new_pos_read < rb->pos_read) {
		chunk1 = rb->size - rb->pos_read;
		memcpy(data, rb->buf + rb->pos_read, chunk1);
		if (new_pos_read > 0)
			memcpy(data + chunk1, rb->buf, new_pos_read);
	} else {
		memcpy(data, rb->buf + rb->pos_read, length);
	}

	rb->pos_read = new_pos_read;
	rb->fill_count -= length;

	return length;
}

static ssize_t
rb_read_user(struct quadd_ring_buffer *rb, char __user *data, size_t length)
{
	size_t new_pos_read, chunk1;

	if (length > rb->fill_count)
		return 0;

	new_pos_read = (rb->pos_read + length) % rb->size;

	if (new_pos_read < rb->pos_read) {
		chunk1 = rb->size - rb->pos_read;
		if (copy_to_user(data, rb->buf + rb->pos_read, chunk1))
			return -EFAULT;

		if (new_pos_read > 0) {
			if (copy_to_user(data + chunk1, rb->buf,
					 new_pos_read))
				return -EFAULT;
		}
	} else {
		if (copy_to_user(data, rb->buf + rb->pos_read, length))
			return -EFAULT;
	}

	rb->pos_read = new_pos_read;
	rb->fill_count -= length;

	return length;
}

static void
write_sample(struct quadd_record_data *sample,
	     struct quadd_iovec *vec, int vec_count)
{
	int i;
	unsigned long flags;
	struct quadd_ring_buffer *rb = &comm_ctx.rb;
	size_t length_sample;

	length_sample = sizeof(struct quadd_record_data);
	for (i = 0; i < vec_count; i++)
		length_sample += vec[i].len;

	spin_lock_irqsave(&rb->lock, flags);

	if (length_sample > rb_get_free_space(rb)) {
		pr_err_once("Error: Buffer has been overflowed\n");
		spin_unlock_irqrestore(&rb->lock, flags);
		return;
	}

	if (!rb_write(rb, (char *)sample, sizeof(struct quadd_record_data))) {
		spin_unlock_irqrestore(&rb->lock, flags);
		return;
	}

	for (i = 0; i < vec_count; i++) {
		if (!rb_write(rb, vec[i].base, vec[i].len)) {
			spin_unlock_irqrestore(&rb->lock, flags);
			pr_err_once("%s: error: ring buffer\n", __func__);
			return;
		}
	}

	if (rb->fill_count > rb->max_fill_count)
		rb->max_fill_count = rb->fill_count;

	spin_unlock_irqrestore(&rb->lock, flags);

	wake_up_interruptible(&comm_ctx.read_wait);
}

static ssize_t read_sample(char __user *buffer, size_t max_length)
{
	int retval = -EIO;
	unsigned long flags;
	struct quadd_ring_buffer *rb = &comm_ctx.rb;
	struct quadd_record_data record;
	size_t length_extra = 0, nr_events;
	struct quadd_sample_data *sample;

	spin_lock_irqsave(&rb->lock, flags);

	if (rb_is_empty(rb)) {
		retval = 0;
		goto out;
	}

	if (rb->fill_count < sizeof(record))
		goto out;

	if (!rb_read(rb, (char *)&record, sizeof(record)))
		goto out;

	switch (record.record_type) {
	case QUADD_RECORD_TYPE_SAMPLE:
		sample = &record.sample;
		length_extra = sample->callchain_nr * sizeof(quadd_bt_addr_t);

		nr_events = __sw_hweight32(sample->events_flags);
		length_extra += nr_events * sizeof(u32);

		length_extra += sample->state ? sizeof(u32) : 0;
		break;

	case QUADD_RECORD_TYPE_MMAP:
		length_extra = sizeof(u64);

		if (record.mmap.filename_length > 0) {
			length_extra += record.mmap.filename_length;
		} else {
			pr_err("Error: filename is empty\n");
			goto out;
		}
		break;

	case QUADD_RECORD_TYPE_HEADER:
		length_extra = record.hdr.nr_events * sizeof(u32);
		break;

	case QUADD_RECORD_TYPE_DEBUG:
		length_extra = record.debug.extra_length;
		break;

	case QUADD_RECORD_TYPE_MA:
		length_extra = 0;
		break;

	case QUADD_RECORD_TYPE_POWER_RATE:
		length_extra = record.power_rate.nr_cpus * sizeof(u32);
		break;

	case QUADD_RECORD_TYPE_ADDITIONAL_SAMPLE:
		length_extra = record.additional_sample.extra_length;
		break;

	default:
		goto out;
	}

	if (sizeof(record) + length_extra > max_length) {
		retval = rb_read_undo(rb, sizeof(record));
		if (retval < 0)
			goto out;

		retval = 0;
		goto out;
	}

	if (length_extra > rb->fill_count)
		goto out;

	if (copy_to_user(buffer, &record, sizeof(record)))
		goto out_fault_error;

	if (length_extra > 0) {
		retval = rb_read_user(rb, buffer + sizeof(record),
				      length_extra);
		if (retval <= 0)
			goto out;
	}

	spin_unlock_irqrestore(&rb->lock, flags);
	return sizeof(record) + length_extra;

out_fault_error:
	retval = -EFAULT;

out:
	spin_unlock_irqrestore(&rb->lock, flags);
	return retval;
}

static void put_sample(struct quadd_record_data *data,
		       struct quadd_iovec *vec, int vec_count)
{
	if (!atomic_read(&comm_ctx.active))
		return;

	write_sample(data, vec, vec_count);
}

static void comm_reset(void)
{
	unsigned long flags;

	pr_debug("Comm reset\n");
	spin_lock_irqsave(&comm_ctx.rb.lock, flags);
	rb_reset(&comm_ctx.rb);
	spin_unlock_irqrestore(&comm_ctx.rb.lock, flags);
}

static int is_active(void)
{
	return atomic_read(&comm_ctx.active) != 0;
}

static struct quadd_comm_data_interface comm_data = {
	.put_sample = put_sample,
	.reset = comm_reset,
	.is_active = is_active,
};

static int check_access_permission(void)
{
	struct task_struct *task;

	if (capable(CAP_SYS_ADMIN))
		return 0;

	if (!comm_ctx.params_ok || comm_ctx.process_pid == 0)
		return -EACCES;

	rcu_read_lock();
	task = pid_task(find_vpid(comm_ctx.process_pid), PIDTYPE_PID);
	rcu_read_unlock();
	if (!task)
		return -EACCES;

	if (current_fsuid() != task_uid(task) &&
	    task_uid(task) != comm_ctx.debug_app_uid) {
		pr_err("Permission denied, owner/task uids: %u/%u\n",
			   current_fsuid(), task_uid(task));
		return -EACCES;
	}
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	mutex_lock(&comm_ctx.io_mutex);
	comm_ctx.nr_users++;
	mutex_unlock(&comm_ctx.io_mutex);
	return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
	mutex_lock(&comm_ctx.io_mutex);
	comm_ctx.nr_users--;

	if (comm_ctx.nr_users == 0) {
		if (atomic_cmpxchg(&comm_ctx.active, 1, 0)) {
			comm_ctx.control->stop();
			pr_info("Stop profiling: daemon is closed\n");
		}
	}
	mutex_unlock(&comm_ctx.io_mutex);

	return 0;
}

static unsigned int
device_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct quadd_ring_buffer *rb = &comm_ctx.rb;

	poll_wait(file, &comm_ctx.read_wait, wait);

	if (!rb_is_empty_lock(rb))
		mask |= POLLIN | POLLRDNORM;

	if (!atomic_read(&comm_ctx.active))
		mask |= POLLHUP;

	return mask;
}

static ssize_t
device_read(struct file *filp,
	    char __user *buffer,
	    size_t length,
	    loff_t *offset)
{
	int err;
	size_t was_read = 0, res, samples_counter = 0;

	err = check_access_permission();
	if (err)
		return err;

	mutex_lock(&comm_ctx.io_mutex);

	if (!atomic_read(&comm_ctx.active)) {
		mutex_unlock(&comm_ctx.io_mutex);
		return -EPIPE;
	}

	while (was_read + sizeof(struct quadd_record_data) < length) {
		res = read_sample(buffer + was_read, length - was_read);
		if (res < 0) {
			mutex_unlock(&comm_ctx.io_mutex);
			pr_err("Error: data is corrupted\n");
			return res;
		}

		if (res == 0)
			break;

		was_read += res;
		samples_counter++;

		if (!atomic_read(&comm_ctx.active))
			break;
	}

	mutex_unlock(&comm_ctx.io_mutex);
	return was_read;
}

static long
device_ioctl(struct file *file,
	     unsigned int ioctl_num,
	     unsigned long ioctl_param)
{
	int err = 0;
	struct quadd_parameters *user_params;
	struct quadd_comm_cap cap;
	struct quadd_module_state state;
	struct quadd_module_version versions;
	struct quadd_extables extabs;
	unsigned long flags;
	struct quadd_ring_buffer *rb = &comm_ctx.rb;

	if (ioctl_num != IOCTL_SETUP &&
	    ioctl_num != IOCTL_GET_CAP &&
	    ioctl_num != IOCTL_GET_STATE &&
	    ioctl_num != IOCTL_GET_VERSION) {
		err = check_access_permission();
		if (err)
			return err;
	}

	mutex_lock(&comm_ctx.io_mutex);

	switch (ioctl_num) {
	case IOCTL_SETUP:
		if (atomic_read(&comm_ctx.active)) {
			pr_err("error: tegra profiler is active\n");
			err = -EBUSY;
			goto error_out;
		}

		user_params = vmalloc(sizeof(*user_params));
		if (!user_params) {
			err = -ENOMEM;
			goto error_out;
		}

		if (copy_from_user(user_params, (void __user *)ioctl_param,
				   sizeof(struct quadd_parameters))) {
			pr_err("setup failed\n");
			vfree(user_params);
			err = -EFAULT;
			goto error_out;
		}

		err = comm_ctx.control->set_parameters(user_params,
						       &comm_ctx.debug_app_uid);
		if (err) {
			pr_err("error: setup failed\n");
			vfree(user_params);
			goto error_out;
		}
		comm_ctx.params_ok = 1;
		comm_ctx.process_pid = user_params->pids[0];

		if (user_params->reserved[QUADD_PARAM_IDX_SIZE_OF_RB] == 0) {
			pr_err("error: too old version of daemon\n");
			vfree(user_params);
			err = -EINVAL;
			goto error_out;
		}
		comm_ctx.rb_size = user_params->reserved[0];

		pr_info("setup success: freq/mafreq: %u/%u, backtrace: %d, pid: %d\n",
			user_params->freq,
			user_params->ma_freq,
			user_params->backtrace,
			user_params->pids[0]);

		vfree(user_params);
		break;

	case IOCTL_GET_CAP:
		comm_ctx.control->get_capabilities(&cap);
		if (copy_to_user((void __user *)ioctl_param, &cap,
				 sizeof(struct quadd_comm_cap))) {
			pr_err("error: get_capabilities failed\n");
			err = -EFAULT;
			goto error_out;
		}
		break;

	case IOCTL_GET_VERSION:
		strcpy(versions.branch, QUADD_MODULE_BRANCH);
		strcpy(versions.version, QUADD_MODULE_VERSION);

		versions.samples_version = QUADD_SAMPLES_VERSION;
		versions.io_version = QUADD_IO_VERSION;

		if (copy_to_user((void __user *)ioctl_param, &versions,
				 sizeof(struct quadd_module_version))) {
			pr_err("error: get version failed\n");
			err = -EFAULT;
			goto error_out;
		}
		break;

	case IOCTL_GET_STATE:
		comm_ctx.control->get_state(&state);

		state.buffer_size = comm_ctx.rb_size;

		spin_lock_irqsave(&rb->lock, flags);
		state.buffer_fill_size =
			comm_ctx.rb_size - rb_get_free_space(rb);
		state.reserved[QUADD_MOD_STATE_IDX_RB_MAX_FILL_COUNT] =
			rb->max_fill_count;
		spin_unlock_irqrestore(&rb->lock, flags);

		if (copy_to_user((void __user *)ioctl_param, &state,
				 sizeof(struct quadd_module_state))) {
			pr_err("error: get_state failed\n");
			err = -EFAULT;
			goto error_out;
		}
		break;

	case IOCTL_START:
		if (!atomic_cmpxchg(&comm_ctx.active, 0, 1)) {
			if (!comm_ctx.params_ok) {
				pr_err("error: params failed\n");
				atomic_set(&comm_ctx.active, 0);
				err = -EFAULT;
				goto error_out;
			}

			err = rb_init(rb, comm_ctx.rb_size);
			if (err) {
				pr_err("error: rb_init failed\n");
				atomic_set(&comm_ctx.active, 0);
				goto error_out;
			}

			err = comm_ctx.control->start();
			if (err) {
				pr_err("error: start failed\n");
				atomic_set(&comm_ctx.active, 0);
				goto error_out;
			}
			pr_info("Start profiling success\n");
		}
		break;

	case IOCTL_STOP:
		if (atomic_cmpxchg(&comm_ctx.active, 1, 0)) {
			comm_ctx.control->stop();
			wake_up_interruptible(&comm_ctx.read_wait);
			rb_deinit(&comm_ctx.rb);
			pr_info("Stop profiling success\n");
		}
		break;

	case IOCTL_SET_EXTAB:
		if (copy_from_user(&extabs, (void __user *)ioctl_param,
				   sizeof(extabs))) {
			pr_err("error: set_extab failed\n");
			err = -EFAULT;
			goto error_out;
		}

		err = comm_ctx.control->set_extab(&extabs);
		if (err) {
			pr_err("error: set_extab\n");
			goto error_out;
		}
		break;

	default:
		pr_err("error: ioctl %u is unsupported in this version of module\n",
		       ioctl_num);
		err = -EFAULT;
	}

error_out:
	mutex_unlock(&comm_ctx.io_mutex);
	return err;
}

static void unregister(void)
{
	misc_deregister(comm_ctx.misc_dev);
	kfree(comm_ctx.misc_dev);
}

static void free_ctx(void)
{
	rb_deinit(&comm_ctx.rb);
}

static const struct file_operations qm_fops = {
	.read		= device_read,
	.poll		= device_poll,
	.open		= device_open,
	.release	= device_release,
	.unlocked_ioctl	= device_ioctl
};

static int comm_init(void)
{
	int res;
	struct miscdevice *misc_dev;

	misc_dev = kzalloc(sizeof(*misc_dev), GFP_KERNEL);
	if (!misc_dev) {
		pr_err("Error: alloc error\n");
		return -ENOMEM;
	}

	misc_dev->minor = MISC_DYNAMIC_MINOR;
	misc_dev->name = QUADD_DEVICE_NAME;
	misc_dev->fops = &qm_fops;

	res = misc_register(misc_dev);
	if (res < 0) {
		pr_err("Error: misc_register: %d\n", res);
		return res;
	}
	comm_ctx.misc_dev = misc_dev;

	mutex_init(&comm_ctx.io_mutex);
	atomic_set(&comm_ctx.active, 0);

	comm_ctx.params_ok = 0;
	comm_ctx.process_pid = 0;
	comm_ctx.nr_users = 0;

	init_waitqueue_head(&comm_ctx.read_wait);

	return 0;
}

struct quadd_comm_data_interface *
quadd_comm_events_init(struct quadd_comm_control_interface *control)
{
	int err;

	err = comm_init();
	if (err < 0)
		return ERR_PTR(err);

	comm_ctx.control = control;
	return &comm_data;
}

void quadd_comm_events_exit(void)
{
	mutex_lock(&comm_ctx.io_mutex);
	unregister();
	free_ctx();
	mutex_unlock(&comm_ctx.io_mutex);
}
