/*
 * Copyright (C) 2011-2014 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "<sensor_event> " fmt

#include "sensor_event.h"
#include "hwmsensor.h"
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

struct sensor_event_client {
	spinlock_t buffer_lock;
	unsigned int head;
	unsigned int tail;
	unsigned int bufsize;
	unsigned int buffull;
	struct sensor_event *buffer;
	wait_queue_head_t wait;
};
struct sensor_event_obj {
	struct sensor_event_client client[ID_SENSOR_MAX_HANDLE + 1];
};
static struct sensor_event_obj *event_obj;
static struct lock_class_key buffer_lock_key[ID_SENSOR_MAX_HANDLE + 1];
/*
 * sensor_input_event only support process context.
 */
int sensor_input_event(unsigned char handle, const struct sensor_event *event)
{
	struct sensor_event_client *client = &event_obj->client[handle];
	unsigned int dummy = 0;

	/* spin_lock safe, this function don't support interrupt context */
	spin_lock(&client->buffer_lock);
	/*
	 * Reserve below log if need debug LockProve
	 * SE_ERR("[Lomen]sensor_input_event: printf key handle ID=%d, key
	 * addr=%p\n",
	 * handle, (struct
	 * lock_class_key*)client->buffer_lock.rlock.dep_map.key);
	 */
	if (unlikely(client->buffull == true)) {
		pr_err_ratelimited(
			"input buffull, handle:%d, head:%d, tail:%d\n", handle,
			client->head, client->tail);
		spin_unlock(&client->buffer_lock);
		wake_up_interruptible(&client->wait);
		return -1;
	}
	client->buffer[client->head++] = *event;
	client->head &= client->bufsize - 1;
	/* remain 1 count */
	dummy = client->head + 1;
	dummy &= client->bufsize - 1;
	if (unlikely(dummy == client->tail))
		client->buffull = true;
	spin_unlock(&client->buffer_lock);

	wake_up_interruptible(&client->wait);
	return 0;
}

static int sensor_event_fetch_next(struct sensor_event_client *client,
				   struct sensor_event *event)
{
	int have_event;
	/*
	 * spin_lock safe, sensor_input_event always in process context.
	 */
	spin_lock(&client->buffer_lock);

	have_event = client->head != client->tail;
	if (have_event) {
		*event = client->buffer[client->tail++];
		client->tail &= client->bufsize - 1;
		client->buffull = false;
	}

	spin_unlock(&client->buffer_lock);

	return have_event;
}

ssize_t sensor_event_read(unsigned char handle, struct file *file,
			  char __user *buffer, size_t count, loff_t *ppos)
{
	struct sensor_event_client *client = &event_obj->client[handle];
	struct sensor_event event;
	size_t read = 0;

	if (count != 0 && count < sizeof(struct sensor_event)) {
		pr_err("sensor_event_read handle: %d err count(%d)\n",
			  handle, (int)count);
		return -EINVAL;
	}

	for (;;) {
		if (client->head == client->tail)
			return 0;
		if (count == 0) {
			pr_debug("sensor_event_read count: %d\n", (int)count);
			break;
		}

		while (read + sizeof(struct sensor_event) <= count &&
		       sensor_event_fetch_next(client, &event)) {

			if (copy_to_user(buffer + read, &event,
					 sizeof(struct sensor_event)))
				return -EFAULT;

			read += sizeof(struct sensor_event);
		}

		if (read)
			break;
	}

	return read;
}

unsigned int sensor_event_poll(unsigned char handle, struct file *file,
			       poll_table *wait)
{
	struct sensor_event_client *client = &event_obj->client[handle];
	unsigned int mask = 0;

	poll_wait(file, &client->wait, wait);

	if (client->head != client->tail) {
		/* pr_err("sensor_event_poll handle:%d\n", handle); */
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}
unsigned int sensor_event_register(unsigned char handle)
{
	struct sensor_event_obj *obj = event_obj;

	switch (handle) {
	/* google continues sensor ringbuffer is 2048 for batch flow */
	case ID_ACCELEROMETER:
	case ID_MAGNETIC:
	case ID_MAGNETIC_UNCALIBRATED:
	case ID_GYROSCOPE:
	case ID_GYROSCOPE_UNCALIBRATED:
	case ID_PRESSURE:
	case ID_ORIENTATION:
	case ID_ROTATION_VECTOR:
	case ID_GAME_ROTATION_VECTOR:
	case ID_GEOMAGNETIC_ROTATION_VECTOR:
	case ID_GRAVITY:
	case ID_LINEAR_ACCELERATION:
	/* mtk add sensor pedometer and activity ringbuffer is 2048 for batch
	 * flow
	 */
	case ID_PEDOMETER:
	case ID_ACTIVITY:
		spin_lock_init(&obj->client[handle].buffer_lock);
		lockdep_set_class(&obj->client[handle].buffer_lock,
				  &buffer_lock_key[handle]);

		obj->client[handle].head = 0;
		obj->client[handle].tail = 0;
		obj->client[handle].bufsize = CONTINUE_SENSOR_BUF_SIZE;
		obj->client[handle].buffull = false;
		obj->client[handle].buffer =
			vzalloc(obj->client[handle].bufsize *
				sizeof(struct sensor_event));
		if (!obj->client[handle].buffer) {
			pr_err("Alloc ringbuffer error!\n");
			return -1;
		}
		init_waitqueue_head(&obj->client[handle].wait);
		break;
	case ID_EKG:
	case ID_PPG1:
	case ID_PPG2:
		spin_lock_init(&obj->client[handle].buffer_lock);
		lockdep_set_class(&obj->client[handle].buffer_lock,
				  &buffer_lock_key[handle]);
		obj->client[handle].head = 0;
		obj->client[handle].tail = 0;
		obj->client[handle].bufsize = BIO_SENSOR_BUF_SIZE;
		obj->client[handle].buffull = false;
		obj->client[handle].buffer =
			vzalloc(obj->client[handle].bufsize *
				sizeof(struct sensor_event));
		if (!obj->client[handle].buffer) {
			pr_err("Alloc ringbuffer error!\n");
			return -1;
		}
		init_waitqueue_head(&obj->client[handle].wait);
		break;
	/*
	 * other sensor like google onchange and oneshot sensor or mtk add
	 * oneshot onchange sensor
	 * ringbuffer is 16 due to no batch flow
	 */
	default:
		spin_lock_init(&obj->client[handle].buffer_lock);
		lockdep_set_class(&obj->client[handle].buffer_lock,
				  &buffer_lock_key[handle]);

		obj->client[handle].head = 0;
		obj->client[handle].tail = 0;
		obj->client[handle].bufsize = OTHER_SENSOR_BUF_SIZE;
		obj->client[handle].buffull = false;
		obj->client[handle].buffer =
			vzalloc(obj->client[handle].bufsize *
				sizeof(struct sensor_event));
		if (!obj->client[handle].buffer) {
			pr_err("Alloc ringbuffer error!\n");
			return -1;
		}
		init_waitqueue_head(&obj->client[handle].wait);
		break;
	}
	return 0;
}
unsigned int sensor_event_deregister(unsigned char handle)
{
	struct sensor_event_obj *obj = event_obj;

	kfree(obj->client[handle].buffer);
	return 0;
}

static int __init sensor_event_entry(void)
{
	struct sensor_event_obj *obj =
		kzalloc(sizeof(struct sensor_event_obj), GFP_KERNEL);

	event_obj = obj;
	return 0;
}
subsys_initcall(sensor_event_entry);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sensor event driver");
MODULE_AUTHOR("Mediatek");
