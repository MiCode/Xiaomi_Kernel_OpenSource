/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_device.h>
#include <uapi/media/msmb_qca.h>
#include <linux/spinlock_types.h>
#include "msm_qca402.h"

#define DEVICE_NAME "msm_qca402char"
#define DEVICE_CLASS "msm_qca402"
#define MAX_DEVICE_NUM (1)
#define QCA402_START_ENQ_BUFFS (8)
#define QCA402_MAX_EVENTS_IN_MSG (16)
#undef  pr_fmt
#define pr_fmt(fmt) "[qca402x]: %s: " fmt, __func__

static int msm_qca402_major;
static struct class *msm_qca402_class;
struct msm_qca402_device_t {
	struct msm_qca402_dev_data_t *dev;
	struct task_struct *user_task;
	struct task_struct *recv_task;
};

static struct msm_qca402_device_t msm_qca402_device[MAX_DEVICE_NUM];

static const struct of_device_id msm_qca402_dt_match[] = {
	{
		.compatible = "qcom,qca402",
	},
	{}
};
MODULE_DEVICE_TABLE(of, msm_qca402_dt_match);

static int msm_qca402_open(struct inode *ip, struct file *fp)
{
	struct msm_qca402_dev_data_t *dev_data = container_of(ip->i_cdev,
				struct msm_qca402_dev_data_t, cdev);
	struct msm_qca402_file_data_t *file_data;
	struct file_data_list_t *f_entry;

	pr_debug("called for device %d\n", dev_data->dev_idx);

	file_data = kzalloc(sizeof(*file_data), GFP_KERNEL);
	if (!file_data)
		return -EINVAL;
	pr_debug("Out q %pK", &file_data->out_queue);
	init_waitqueue_head(&file_data->out_queue);
	/* initialize channel list here */
	file_data->dev_data = dev_data;
	INIT_LIST_HEAD(&file_data->evt_list);
	INIT_LIST_HEAD(&file_data->ready_ch_list);
	mutex_init(&file_data->file_mutex);
	f_entry = kzalloc(sizeof(*f_entry), GFP_KERNEL);
	if (!f_entry) {
		mutex_destroy(&file_data->file_mutex);
		kfree(file_data);
		return -EINVAL;
	}
	f_entry->file_data = file_data;
	list_add_tail(&f_entry->list, &dev_data->file_list);
	fp->private_data = file_data;
	return 0;
}

static void msm_qca402_free_file_data(struct msm_qca402_file_data_t *file_data)
{
	struct msm_qca402_evt_list_t *cur_evt, *next_evt;
	struct msm_qca402_ready_list_t *cur_rch, *next_rch;

	list_for_each_entry_safe(cur_evt, next_evt, &file_data->evt_list,
				 list) {
		list_del_init(&cur_evt->list);
		kfree(cur_evt);
		break;
	}
	list_for_each_entry_safe(cur_rch, next_rch, &file_data->ready_ch_list,
			list) {
		list_del_init(&cur_rch->list);
		kfree(cur_rch);
		break;
	}
	mutex_destroy(&file_data->file_mutex);
	kfree(file_data);
}

static void msm_qca402_release_file(struct msm_qca402_dev_data_t *dev_data,
		struct msm_qca402_file_data_t *file_data)
{
	struct file_data_list_t *f_entry;
	struct file_data_list_t *next;

	list_for_each_entry_safe(f_entry, next, &dev_data->file_list, list) {
		if (!file_data || f_entry->file_data == file_data) {
			list_del_init(&f_entry->list);
			msm_qca402_free_file_data(f_entry->file_data);
			kfree(f_entry);
			break;
		}
	}
}

static int msm_qca402_release(struct inode *ip, struct file *fp)
{
	struct msm_qca402_file_data_t *file_data =
		(struct msm_qca402_file_data_t *)fp->private_data;
	struct msm_qca402_dev_data_t *dev_data = container_of(ip->i_cdev,
				struct msm_qca402_dev_data_t, cdev);

	pr_debug("called\n");

	msm_qca402_release_file(dev_data, file_data);

	return 0;
}

static struct msm_qca402_channel_list_t *msm_qca402_init_channel(
		struct msm_qca402_dev_data_t *dev_data,
		__u32 channel_id)
{
	struct msm_qca402_channel_list_t *channel = NULL;
	struct msm_qca402_channel_list_t *new_channel = NULL;

	list_for_each_entry(channel, &dev_data->channel_list, list) {
		if (channel->channel_id == channel_id) {
			channel->ref_cnt++;
			pr_debug("channel with %d exists\n", channel_id);
			return channel;
		}
	}
	new_channel = kzalloc(sizeof(*new_channel), GFP_KERNEL);
	if (!new_channel)
		return NULL;
	new_channel->channel_id = channel_id;
	INIT_LIST_HEAD(&new_channel->enqueued_list);
	INIT_LIST_HEAD(&new_channel->ready_list);
	INIT_LIST_HEAD(&new_channel->dequeued_list);
	list_add_tail(&new_channel->list, &dev_data->channel_list);
	new_channel->ref_cnt = 1;
	return new_channel;
}

static void msm_qca402_free_buffer_list(struct msm_qca402_dev_data_t *dev_data,
					struct list_head  *buffer_list)
{
	struct msm_qca402_buffer_list_t *buffer;
	struct msm_qca402_buffer_list_t *n_buffer;

	list_for_each_entry_safe(buffer, n_buffer, buffer_list, list) {
		if (buffer->ih)
			ion_unmap_kernel(dev_data->ion_client, buffer->ih);
		list_del_init(&buffer->list);
		kfree(buffer);
	}
}

static int msm_qca402_deinit_channel(
		struct msm_qca402_dev_data_t *dev_data,
		__u32 channel_id)
{
	struct msm_qca402_channel_list_t *channel;
	struct msm_qca402_channel_list_t *c_channel = NULL;

	list_for_each_entry(channel, &dev_data->channel_list, list) {
		if (channel->channel_id == channel_id) {
			c_channel = channel;
			break;
		}
	}
	if (!c_channel) {
		pr_debug("No channel %d\n", channel_id);
		return -EINVAL;
	}
	c_channel->ref_cnt--;
	if (channel->ref_cnt)
		return 0;
	msm_qca402_free_buffer_list(dev_data, &channel->enqueued_list);
	msm_qca402_free_buffer_list(dev_data, &channel->ready_list);
	msm_qca402_free_buffer_list(dev_data, &channel->dequeued_list);
	list_del_init(&channel->list);
	kfree(channel);
	return 0;
}

static void msm_qca402_unregister_events(
		struct msm_qca402_file_data_t *file_data, int num_events,
		struct msm_qca_event_type *evts)
{
	int i;
	struct msm_qca402_evt_list_t *c_evt_entry;
	struct msm_qca402_evt_list_t *next;

	for (i = 0; i < num_events; i++) {
		list_for_each_entry_safe(c_evt_entry, next,
						&file_data->evt_list, list) {
			if (c_evt_entry->channel_id == evts[i].channel_id &&
				c_evt_entry->cmd == evts[i].cmd) {
				pr_debug("%pK Unregistering %d %d\n", file_data,
					c_evt_entry->channel_id,
					c_evt_entry->cmd);
				msm_qca402_deinit_channel(file_data->dev_data,
						evts[i].channel_id);
				list_del_init(&c_evt_entry->list);
				kfree(c_evt_entry);
				break;
			}
		}
	}
}

static int msm_qca402_register_events(struct msm_qca402_file_data_t *file_data,
				int num_events,
				struct msm_qca_event_type *evts)
{
	int i;
	int ret = 0;
	struct msm_qca402_evt_list_t *c_evt_entry;

	for (i = 0; i < num_events; i++) {
		c_evt_entry = kzalloc(sizeof(*c_evt_entry), GFP_KERNEL);
		if (!c_evt_entry) {
			ret = -ENOMEM;
			break;
		}
		c_evt_entry->channel_id = evts[i].channel_id;
		c_evt_entry->cmd = evts[i].cmd;
		pr_debug("%pK Registering %d %d\n", file_data,
			c_evt_entry->channel_id, c_evt_entry->cmd);
		list_add_tail(&c_evt_entry->list, &file_data->evt_list);
		msm_qca402_init_channel(file_data->dev_data,
			evts[i].channel_id);
	}

	if (ret)
		msm_qca402_unregister_events(file_data, num_events, evts);

	return ret;
}

static void msm_qca402_free_workbuffs(struct msm_qca402_dev_data_t *dev_data)
{
	struct msm_qca402_workbuff_list_t *wb;
	struct msm_qca402_workbuff_list_t *next;

	list_for_each_entry_safe(wb, next, &dev_data->work_list, list) {
		list_del_init(&wb->list);
		kfree(wb);
	}
}

static int msm_qca402_alloc_add_workbuffs(
	struct msm_qca402_dev_data_t *dev_data,
	__u32 num_buffs)
{
	int ret = 0;
	int i;
	struct msm_qca402_workbuff_list_t *wb;

	for (i = 0; i < num_buffs; i++) {
		wb = kzalloc(sizeof(*wb), GFP_KERNEL);
		if (!wb) {
			ret = -ENOMEM;
			msm_qca402_free_workbuffs(dev_data);
			break;
		}
		list_add_tail(&wb->list, &dev_data->work_list);
	}
	return ret;
}

static struct msm_qca402_workbuff_list_t *msm_qca402_get_workbuff(
	struct msm_qca402_dev_data_t *dev_data)
{
	int ret;
	struct msm_qca402_workbuff_list_t *wb;

	if (list_empty(&dev_data->work_list)) {
		ret = msm_qca402_alloc_add_workbuffs(dev_data, 1);
		if (ret)
			return NULL;
	}
	wb = list_first_entry(&dev_data->work_list,
		struct msm_qca402_workbuff_list_t, list);
	list_del_init(&wb->list);
	return wb;
}

static void msm_qca402_propagate_event(struct msm_qca402_dev_data_t *dev_data,
				struct msm_qca_event_type *event,
				struct msm_qca402_channel_list_t *channel)
{
	struct msm_qca402_file_data_t *file_data;
	struct file_data_list_t *f_entry;
	struct msm_qca402_evt_list_t *e_entry;
	struct msm_qca402_ready_list_t *r_entry;

	list_for_each_entry(f_entry, &dev_data->file_list, list) {
		file_data = f_entry->file_data;
		list_for_each_entry(e_entry, &file_data->evt_list, list) {
			if (e_entry->channel_id == event->channel_id &&
				e_entry->cmd == event->cmd) {
				/**
				 * put event into queue
				 * unblock poll
				 */
				pr_debug("%d %d\n", event->channel_id,
						e_entry->cmd);
				r_entry = kzalloc(sizeof(*r_entry), GFP_KERNEL);
				if (!r_entry)
					return;

				channel->ref_cnt += 1;
				r_entry->ch_data = channel;
				r_entry->cmd = e_entry->cmd;
				list_add_tail(&r_entry->list,
						&file_data->ready_ch_list);
				pr_debug("called\n");
				wake_up_interruptible(&file_data->out_queue);
			}
		}
	}
}

static int msm_qca402_copy_data(struct msm_qca402_htca_message_t *htca_msg,
		struct msm_qca402_buffer_list_t *buffer,
		__u32 size)
{

	__u32 c_payload_size = 0;
	__u32 c_meta_size = 0;

	if (size < (QCA_MSG_HEADER_SIZE + htca_msg->header.meta_size))
		return -EINVAL;
	c_payload_size = size - htca_msg->header.meta_size -
		QCA_MSG_HEADER_SIZE;


	switch (htca_msg->header.msg_flags & LPCHTCA_PACKET_MASK) {
	case LPCHTCA_FULL_PACKET:
	case LPCHTCA_START_PACKET: {
		if (buffer->valid_size > 0)
			buffer->valid_size = 0;
		if ((htca_msg->header.meta_size >
					buffer->qca_msg.header_size) ||
			(c_payload_size > buffer->qca_msg.data_size))
			return -EINVAL;
		c_meta_size = htca_msg->header.meta_size;
	}
		break;

	case LPCHTCA_END_PACKET:
	case LPCHTCA_MID_PACKET:
		if (c_payload_size + buffer->valid_size >
			buffer->qca_msg.data_size) {
			/* indicate ERROR here */
			return -EINVAL;
		}
		break;
	case LPCHTCA_UNSPEC_PACKET:
		if (c_payload_size + buffer->valid_size >
			buffer->qca_msg.data_size) {
			if (buffer->valid_size)
				/* continue in next buffer */
				return -ENOMEM;
			else
				return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (c_meta_size)
		if (copy_to_user((void __user *)
					(uintptr_t)buffer->qca_msg.header,
				htca_msg->payload,
				c_meta_size))
			return -EFAULT;

	if (c_payload_size) {
		__u8 *ptr;

		if (buffer->vaddr) {
			/* ion buffer */
			ptr = (__u8 *)buffer->vaddr;
			ptr += buffer->valid_size;
			memcpy((void *)ptr,
				&htca_msg->payload[htca_msg->header.meta_size],
				c_payload_size);
		} else {
			ptr = (__u8 *)((uintptr_t)buffer->qca_msg.buff_addr);
			ptr += buffer->valid_size;
			if (copy_to_user((void __user *)ptr,
				&htca_msg->payload[htca_msg->header.meta_size],
				c_payload_size))
				return -EFAULT;
		}
		buffer->valid_size += c_payload_size;
	}

	switch (htca_msg->header.msg_flags & LPCHTCA_PACKET_MASK) {
	case LPCHTCA_START_PACKET: {
		buffer->qca_msg.header_size = c_meta_size;
	}
		break;

	case LPCHTCA_FULL_PACKET: {
		buffer->qca_msg.header_size = c_meta_size;
		buffer->qca_msg.data_size = buffer->valid_size;
	}
		break;

	case LPCHTCA_END_PACKET: {
		buffer->qca_msg.data_size = buffer->valid_size;
	}
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int msm_qca402_move_userbuff_to_(struct list_head *target_list,
		struct msm_qca402_channel_list_t *ch_data)
{
	struct msm_qca402_buffer_list_t *c_entry;

	c_entry = ch_data->current_entry;
	if (c_entry) {
		list_del_init(&c_entry->list);
		list_add_tail(&c_entry->list, target_list);
	}
	ch_data->current_entry = NULL;
	if (!list_empty(&ch_data->enqueued_list)) {
		ch_data->current_entry = list_first_entry(
			&ch_data->enqueued_list,
			struct msm_qca402_buffer_list_t, list);
	}
	return 0;
}

static int msm_qca402_process_htca_message(
		struct msm_qca402_dev_data_t *dev_data,
		struct msm_qca402_workbuff_list_t *wb)
{
	int ret = 0;
	int keep = 0;
	__u8 pkt_flags;
	struct msm_qca402_htca_message_t *htca_msg = &wb->htca_msg;
	struct msm_qca402_channel_list_t *ch_data_cur;
	struct msm_qca402_channel_list_t *ch_data = NULL;

	pr_debug("wb %pK %d\n", htca_msg,
			htca_msg->header.channel_id);
	list_for_each_entry(ch_data_cur, &dev_data->channel_list, list) {
		if (ch_data_cur->channel_id == htca_msg->header.channel_id) {
			ch_data = ch_data_cur;
			break;
		}
	}
	if (!ch_data && !(htca_msg->header.msg_flags & LPCHTCA_ASYNC_MESSAGE)) {
		/* No registered handler and not ASYNC message, drop packet */
		goto return_wb;
	} else if (!ch_data) {
		/* if ASYNC message with no registered handler, add it */
		ch_data = msm_qca402_init_channel(dev_data,
				htca_msg->header.channel_id);
		if (!ch_data)
			return -EINVAL;
	}
	if (!ch_data->has_buff &&
			!(htca_msg->header.msg_flags & LPCHTCA_FULL_PACKET)) {
		/* data packet but no free enqueued buffers, drop packet */
		/* indicate ERROR here */
		goto return_wb;
	} else if (ch_data->has_buff) {
		if (!ch_data->current_entry) {
			pr_debug("NO CURRENT ENTRY\n");
			return -EINVAL;
		}
		/* data packet and there is enqueued buffer start filling it */
		ret = msm_qca402_copy_data(htca_msg, ch_data->current_entry,
				wb->size);
		pkt_flags = htca_msg->header.msg_flags & LPCHTCA_PACKET_MASK;
		if ((pkt_flags == LPCHTCA_UNSPEC_PACKET && ret == -ENOMEM) ||
			(pkt_flags == LPCHTCA_FULL_PACKET) ||
			(pkt_flags == LPCHTCA_END_PACKET) ||
			(htca_msg->header.msg_flags &
					LPCHTCA_END_TRANSMISSON)) {
			struct msm_qca_event_type event;

			event.cmd = MSM_QCA_EVENT_ENQ_BUF;
			event.channel_id = ch_data->channel_id;
			msm_qca402_move_userbuff_to_(&ch_data->ready_list,
					ch_data);
			msm_qca402_propagate_event(dev_data, &event, ch_data);
		}
		if (ret == -ENOMEM) {
			ret = msm_qca402_copy_data(htca_msg,
				ch_data->current_entry, wb->size);
		}
	} else {
		struct msm_qca_event_type event;

		/* control message, save for future receiving */
		keep = 1;
		ch_data->wbout = wb;
		event.cmd = MSM_QCA_EVENT_RECV_MSG;
		event.channel_id = ch_data->channel_id;
		msm_qca402_propagate_event(dev_data, &event, ch_data);
	}

return_wb:
	if (!keep)
		list_add_tail(&wb->list, &dev_data->work_list);
	return ret;
}

static struct msm_qca402_channel_list_t *msm_qca402_find_channel(
		struct msm_qca402_dev_data_t *dev_data,
		__u32 channel_id)
{
	struct msm_qca402_channel_list_t *ch_data;

	list_for_each_entry(ch_data, &dev_data->channel_list, list) {
		if (ch_data->channel_id == channel_id)
			return ch_data;
	}

	return NULL;
}

static int msm_qca402_copy_header(struct msm_qca402_dev_data_t *dev_data,
		struct msm_qca402_buffer_list_t *buff,
		struct msm_qca_message_type *data)
{
		buff->wbb = msm_qca402_get_workbuff(dev_data);
		if (!buff->wbb)
			return -ENOMEM;

		if (data->header_size <= QCA_MSG_PAYLOAD_SIZE) {
			if (copy_from_user(
				(void *)&buff->wbb->htca_msg.payload[0],
				(void __user *)(uintptr_t)data->header,
				data->header_size))
				return -EFAULT;
			buff->wbb->htca_msg.header.meta_size =
				data->header_size;
		} else
			return -EINVAL;
		return 0;
}

static struct msm_qca402_buffer_list_t *msm_qca402_deq_map_buff(
		struct msm_qca402_dev_data_t *dev_data,
		struct msm_qca402_channel_list_t *ch_data,
		struct msm_qca_message_type *data)
{
	int ret;
	struct msm_qca402_buffer_list_t *buff;

	list_for_each_entry(buff, &ch_data->dequeued_list, list) {
		if (buff->qca_msg.fd == data->fd) {
			list_del_init(&buff->list);
			if (!ch_data->has_buff) {
				ret = msm_qca402_copy_header(dev_data, buff,
						data);
				if (ret < 0)
					return NULL;
			}
			buff->qca_msg = *data;
			return buff;
		}
	}
	buff = kzalloc(sizeof(*buff), GFP_KERNEL);
	if (!buff)
		return NULL;
	if (!ch_data->has_buff) {
		ret = msm_qca402_copy_header(dev_data, buff, data);
		if (ret < 0)
			return NULL;
	}
	buff->qca_msg = *data;
	if (data->is_ion_data) {
		buff->ih = ion_import_dma_buf_fd(dev_data->ion_client,
				data->fd);
		buff->vaddr = ion_map_kernel(dev_data->ion_client, buff->ih);
	}

	return buff;
}

static unsigned int msm_qca402_recv_message(
		struct msm_qca402_file_data_t *file,
		struct msm_qca_message_type *data)
{
	struct msm_qca402_channel_list_t *channel;
	struct msm_qca402_ready_list_t *rq_entry;
	struct msm_qca402_buffer_list_t *buffer;
	int ret = 0;

	if (list_empty(&file->ready_ch_list))
		return -ENODATA;
	rq_entry = list_first_entry(&file->ready_ch_list,
		struct msm_qca402_ready_list_t, list);
	channel = rq_entry->ch_data;
	list_del_init(&rq_entry->list);
	kfree(rq_entry);

	if (!list_empty(&channel->ready_list)) {
		buffer = list_first_entry(&channel->ready_list,
			struct msm_qca402_buffer_list_t, list);
		*data = buffer->qca_msg;
		if (channel->has_buff)
			data->cmd = MSM_QCA_EVENT_ENQ_BUF;
		else
			data->cmd = MSM_QCA_EVENT_SEND_MSG;
		list_del_init(&buffer->list);
		buffer->valid_size = 0;
		channel->ref_cnt--;
		list_add_tail(&buffer->list, &channel->dequeued_list);
	} else if (channel->wbout) {
		data->channel_id = channel->channel_id;
		data->is_ion_data = 0;
		data->data_size = 0;
		if (data->header_size >=
			channel->wbout->htca_msg.header.meta_size) {
			data->header_size =
				channel->wbout->htca_msg.header.meta_size;
			data->cmd = MSM_QCA_EVENT_RECV_MSG;
			if (copy_to_user((void __user *)(uintptr_t)data->header,
				channel->wbout->htca_msg.payload,
				channel->wbout->htca_msg.header.meta_size)) {
				list_add_tail(&channel->wbout->list,
						&file->dev_data->work_list);
				return -EFAULT;
			}
			channel->ref_cnt--;
			list_add_tail(&channel->wbout->list,
					&file->dev_data->work_list);
		} else {
			ret = -EINVAL;
		}
	}

	return ret;
}

static __u8 msm_qca402_fill_wb(
		struct msm_qca402_buffer_list_t *buff,
		struct msm_qca402_workbuff_list_t *wb)
{
	__u32 total_size = buff->qca_msg.header_size +
		buff->qca_msg.data_size;
	__u32 write_size = 0;
	__u8 *ptr;

	wb->htca_msg.header.msg_flags = 0;

	wb->htca_msg.header.channel_id = buff->qca_msg.channel_id;

	pr_debug("valid size %d total %d\n", buff->valid_size, total_size);
	if (buff->valid_size == 0) {
		if (total_size <= QCA_MSG_PAYLOAD_SIZE)
			wb->htca_msg.header.msg_flags = LPCHTCA_FULL_PACKET;
		else
			wb->htca_msg.header.msg_flags = LPCHTCA_START_PACKET;
		write_size = min(QCA_MSG_PAYLOAD_SIZE -
					wb->htca_msg.header.meta_size,
				buff->qca_msg.data_size);
	} else {
		wb->htca_msg.header.meta_size = 0;
		if (total_size - buff->valid_size <= QCA_MSG_PAYLOAD_SIZE)
			wb->htca_msg.header.msg_flags = LPCHTCA_END_PACKET;
		else
			wb->htca_msg.header.msg_flags = LPCHTCA_MID_PACKET;
		write_size = min(QCA_MSG_PAYLOAD_SIZE,
				buff->qca_msg.data_size - buff->valid_size);
	}
	if (buff->vaddr) {
		ptr = (__u8 *)buff->vaddr + buff->valid_size;
		memcpy(&wb->htca_msg.payload[wb->htca_msg.header.meta_size],
				(void *)ptr, write_size);
	} else {
		ptr = (__u8 *)((uintptr_t)buff->qca_msg.buff_addr) +
				buff->valid_size;
		if (copy_from_user(
			&wb->htca_msg.payload[wb->htca_msg.header.meta_size],
			(void __user *)ptr, write_size))
			return -EFAULT;
	}
	buff->valid_size += write_size;
	wb->size = QCA_MSG_HEADER_SIZE + wb->htca_msg.header.meta_size +
		write_size;
	if (buff->qca_msg.last_data &&
		((wb->htca_msg.header.msg_flags == LPCHTCA_FULL_PACKET) ||
		(wb->htca_msg.header.msg_flags == LPCHTCA_END_PACKET)))
		wb->htca_msg.header.msg_flags |= LPCHTCA_END_TRANSMISSON;

	pr_debug("flags %x\n", wb->htca_msg.header.msg_flags);
	return wb->htca_msg.header.msg_flags;
}
static unsigned int msm_qca402_process_wb(
	struct msm_qca402_dev_data_t *dev_data,
	struct msm_qca402_channel_list_t *ch_data,
	struct msm_qca402_buffer_list_t *buff,
	struct msm_qca402_workbuff_list_t **wb)
{
	__u8 flags;
	struct msm_qca_event_type event;

	if (!*wb)
		*wb = msm_qca402_get_workbuff(dev_data);

	flags = msm_qca402_fill_wb(buff, *wb);

	if ((flags & LPCHTCA_PACKET_MASK) == LPCHTCA_FULL_PACKET ||
		(flags & LPCHTCA_PACKET_MASK) == LPCHTCA_END_PACKET) {
		msm_qca402_move_userbuff_to_(&ch_data->ready_list,
				ch_data);
		event.cmd = MSM_QCA_EVENT_SEND_MSG;
		event.channel_id = ch_data->channel_id;
		msm_qca402_propagate_event(dev_data, &event, ch_data);
		return 1;
	}

	return 0;
}

static unsigned int msm_qca402_send_htca_message(
	struct msm_qca402_dev_data_t *dev_data,
	struct msm_qca402_channel_list_t *ch_data)
{
	int status;
	int ret;
	struct msm_qca402_buffer_list_t *buff = ch_data->current_entry;
	struct msm_qca402_workbuff_list_t *wb = NULL;


	wb = ch_data->wbin;
	if (!wb) {
		if (!buff)
			return -EINVAL;
		wb = buff->wbb;
		if (!wb) {
			pr_err("Incorrect header error\n");
			return -EINVAL;
		}
		ret = msm_qca402_process_wb(dev_data, ch_data, buff,
				&wb);
		if (!wb)
			return -EINVAL;
		buff->wbb = NULL;
	}

	pr_debug("send %d\n", wb->size);
	dev_data->sending++;
	status = htca_buffer_send(dev_data->htca_target, 0,
			(void *)&wb->htca_msg.header, wb->size, wb);
	ch_data->wbin = NULL;
	if (status)
		return -EINVAL;
	if (((wb->htca_msg.header.msg_flags & LPCHTCA_PACKET_MASK) ==
				LPCHTCA_FULL_PACKET) ||
		((wb->htca_msg.header.msg_flags & LPCHTCA_PACKET_MASK) ==
				LPCHTCA_END_PACKET)) {
		return 0;
	}

	if (!buff)
		return -EINVAL;
	ret = msm_qca402_process_wb(dev_data, ch_data, buff, &ch_data->wbin);

	return -EAGAIN;
}


static unsigned int msm_qca402_poll(struct file *fp, poll_table *wait)
{
	struct msm_qca402_file_data_t *file_data =
		(struct msm_qca402_file_data_t *)fp->private_data;
	unsigned int mask = 0;

	pr_debug("called %pK\n", &file_data->out_queue);
	poll_wait(fp, &file_data->out_queue, wait);
	if (!list_empty(&file_data->ready_ch_list))
		mask = POLLIN | POLLRDNORM;
	return mask;
}

static struct msm_qca_event_type *msm_qca402_alloc_get_evts(
		struct msm_qca_event_list_type *evt_list)
{
	int len = sizeof(struct msm_qca_event_type) *
				evt_list->num_events;
	struct msm_qca_event_type *evts;

	if (evt_list->num_events > QCA402_MAX_EVENTS_IN_MSG)
		return NULL;
	evts = kzalloc(len, GFP_KERNEL);
	if (!evts)
		return NULL;
	if (copy_from_user((void *)evts,
			(void __user *)(uintptr_t)evt_list->events,
			len)) {
		kfree(evts);
		return NULL;
	}
	return evts;
}

static int msm_qca402_abort(struct msm_qca402_file_data_t *file_data,
	int num_events, struct msm_qca_event_type *evts) {
	return 0;
}

static int msm_qca402_flush(struct msm_qca402_file_data_t *file_data,
	int num_events, struct msm_qca_event_type *evts) {
	return 0;
}

static long msm_qca402_ioctl(struct file *file, unsigned int cmd,
		void *arg)
{
	struct msm_qca402_file_data_t *file_data =
		(struct msm_qca402_file_data_t *)file->private_data;
	long ret = 0;

	switch (cmd) {
	case MSM_QCA402X_ENQUEUE_BUFFER: {
		struct msm_qca_message_type *data =
			(struct msm_qca_message_type *)arg;
		struct msm_qca402_buffer_list_t *buff;
		struct msm_qca402_channel_list_t *ch_data;

		pr_debug("MSM_QCA402X_ENQUEUE_BUFFER\n");
		mutex_lock(&file_data->dev_data->dev_mutex);
		ch_data = msm_qca402_find_channel(file_data->dev_data,
				data->channel_id);
		if (!ch_data) {
			mutex_unlock(&file_data->dev_data->dev_mutex);
			return -EINVAL;
		}
		ch_data->has_buff = 1;
		buff = msm_qca402_deq_map_buff(file_data->dev_data, ch_data,
				data);
		if (!buff) {
			mutex_unlock(&file_data->dev_data->dev_mutex);
			return -ENOMEM;
		}
		list_add_tail(&buff->list, &ch_data->enqueued_list);
		if (!ch_data->current_entry)
			ch_data->current_entry = list_first_entry(
				&ch_data->enqueued_list,
				struct msm_qca402_buffer_list_t, list);
		mutex_unlock(&file_data->dev_data->dev_mutex);
	}
		break;
	case MSM_QCA402X_SEND_MESSAGE: {
		struct msm_qca_message_type *data =
			(struct msm_qca_message_type *)arg;
		struct msm_qca402_buffer_list_t *buff;
		struct msm_qca402_channel_list_t *ch_data;
		struct msm_qca402_ready_list_t *r_entry;

		pr_debug("MSM_QCA402X_SEND_MESSAGE\n");
		mutex_lock(&file_data->dev_data->dev_mutex);
		ch_data = msm_qca402_find_channel(file_data->dev_data,
				data->channel_id);
		if (!ch_data) {
			mutex_unlock(&file_data->dev_data->dev_mutex);
			return -EINVAL;
		}
		ch_data->has_buff = 0;
		buff = msm_qca402_deq_map_buff(file_data->dev_data, ch_data,
				data);
		if (!buff) {
			mutex_unlock(&file_data->dev_data->dev_mutex);
			return -ENOMEM;
		}
		r_entry = kzalloc(sizeof(*r_entry), GFP_KERNEL);
		if (!r_entry) {
			list_add_tail(&buff->list, &ch_data->dequeued_list);
			mutex_unlock(&file_data->dev_data->dev_mutex);
			return -ENOMEM;
		}
		ch_data->ref_cnt += 1;
		list_add_tail(&buff->list, &ch_data->enqueued_list);
		r_entry->ch_data = ch_data;
		list_add_tail(&r_entry->list, &file_data->dev_data->in_list);
		if (!ch_data->current_entry)
			ch_data->current_entry = list_first_entry(
				&ch_data->enqueued_list,
				struct msm_qca402_buffer_list_t, list);
		mutex_unlock(&file_data->dev_data->dev_mutex);

		wake_up_interruptible(&file_data->dev_data->in_queue);
	}
		break;
	case MSM_QCA402X_RECEIVE_MESSAGE: {
		struct msm_qca_message_type *data =
			(struct msm_qca_message_type *)arg;
		pr_debug("MSM_QCA402X_RECEIVE_MESSAGE\n");
		mutex_lock(&file_data->dev_data->dev_mutex);
		ret = msm_qca402_recv_message(file_data, data);
		mutex_unlock(&file_data->dev_data->dev_mutex);
	}
		break;
	case MSM_QCA402X_REGISTER_EVENT: {
		struct msm_qca_event_list_type *evt_list =
			(struct msm_qca_event_list_type *)arg;
		struct msm_qca_event_type *evts;

		pr_debug("MSM_QCA402X_REGISTER_EVENT\n");

		evts = msm_qca402_alloc_get_evts(evt_list);
		if (!evts)
			return -ENOMEM;
		mutex_lock(&file_data->dev_data->dev_mutex);
		ret = msm_qca402_register_events(file_data,
						 evt_list->num_events,
						 evts);
		mutex_unlock(&file_data->dev_data->dev_mutex);
		kfree(evts);
	}
		break;
	case MSM_QCA402X_UNREGISTER_EVENT: {
		struct msm_qca_event_list_type *evt_list =
			(struct msm_qca_event_list_type *)arg;
		struct msm_qca_event_type *evts;

		pr_debug("MSM_QCA402X_UNREGISTER_EVENT\n");

		evts = msm_qca402_alloc_get_evts(evt_list);
		if (!evts)
			return -ENOMEM;
		mutex_lock(&file_data->dev_data->dev_mutex);
		msm_qca402_unregister_events(file_data, evt_list->num_events,
						evts);
		mutex_unlock(&file_data->dev_data->dev_mutex);
		kfree(evts);
	}
		break;
	case MSM_QCA402X_FLUSH_BUFFERS: {
		struct msm_qca_event_list_type *evt_list =
			(struct msm_qca_event_list_type *)arg;
		struct msm_qca_event_type *evts;

		pr_debug("MSM_QCA402X_UNREGISTER_EVENT\n");

		evts = msm_qca402_alloc_get_evts(evt_list);
		if (!evts)
			return -ENOMEM;
		mutex_lock(&file_data->dev_data->dev_mutex);
		msm_qca402_flush(file_data, evt_list->num_events, evts);
		mutex_unlock(&file_data->dev_data->dev_mutex);
		kfree(evts);
	}
		break;

	case MSM_QCA402X_ABORT_MESSAGE: {
		struct msm_qca_event_list_type *evt_list =
			(struct msm_qca_event_list_type *)arg;
		struct msm_qca_event_type *evts;

		pr_debug("MSM_QCA402X_UNREGISTER_EVENT\n");

		evts = msm_qca402_alloc_get_evts(evt_list);
		if (!evts)
			return -ENOMEM;
		mutex_lock(&file_data->dev_data->dev_mutex);
		msm_qca402_abort(file_data, evt_list->num_events, evts);
		mutex_unlock(&file_data->dev_data->dev_mutex);
		kfree(evts);
	}
		break;
	default:
		pr_debug("unhandled ioctl command: %d\n", cmd);
	}
	return ret;
}
static long msm_qca402_unlocked_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	long ret = 0;
	__u32 len = 0;

	switch (cmd) {
	case MSM_QCA402X_ENQUEUE_BUFFER:
	case MSM_QCA402X_SEND_MESSAGE:
	case MSM_QCA402X_RECEIVE_MESSAGE: {
		struct msm_qca_message_type data;

		len = sizeof(struct msm_qca_message_type);
		if (copy_from_user(&data, (void __user *)arg, len))
			return -EFAULT;
		ret = msm_qca402_ioctl(file, cmd, (void *)&data);
		if (copy_to_user((void __user *)arg, &data, len))
			return -EFAULT;
	}
		break;
	case MSM_QCA402X_FLUSH_BUFFERS:
	case MSM_QCA402X_ABORT_MESSAGE:
	case MSM_QCA402X_REGISTER_EVENT:
	case MSM_QCA402X_UNREGISTER_EVENT: {
		struct msm_qca_event_list_type data;

		len = sizeof(struct msm_qca_event_list_type);
		if (copy_from_user(&data, (void __user *)arg, len))
			return -EFAULT;
		ret = msm_qca402_ioctl(file, cmd, (void *)&data);
		if (copy_to_user((void __user *)arg, &data, len))
			return -EFAULT;
	}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
#ifdef CONFIG_COMPAT
struct msm_qca_message_type_32 {
	__u64 header;
	__u64 buff_addr;
	__u32 header_size;
	__u32 data_size;
	int fd;
	__u8 cmd;
	__u8 channel_id;
	__u8 is_ion_data;
	__u8 last_data;
};

struct msm_qca_event_list_type_32 {
	__u64 events;
	__u32 num_events;
};

#define MSM_QCA402X_ENQUEUE_BUFFER_COMPAT \
	_IOWR(0xdd, 1, struct msm_qca_message_type_32)
#define MSM_QCA402X_SEND_MESSAGE_COMPAT \
	_IOWR(0xdd, 2, struct msm_qca_message_type_32)
#define MSM_QCA402X_RECEIVE_MESSAGE_COMPAT \
	_IOWR(0xdd, 3, struct msm_qca_message_type_32)
#define MSM_QCA402X_FLUSH_BUFFERS_COMPAT \
	_IOWR(0xdd, 4, struct msm_qca_event_list_type_32)
#define MSM_QCA402X_ABORT_MESSAGE_COMPAT \
	_IOWR(0xdd, 5, struct msm_qca_event_list_type_32)
#define MSM_QCA402X_REGISTER_EVENT_COMPAT \
	_IOWR(0xdd, 6, struct msm_qca_event_list_type_32)
#define MSM_QCA402X_UNREGISTER_EVENT_COMPAT \
	_IOWR(0xdd, 7, struct msm_qca_event_list_type_32)

static long msm_qca402_compat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	long ret = 0;

	pr_debug("called\n");

	switch (cmd) {
	case MSM_QCA402X_ENQUEUE_BUFFER_COMPAT:
	case MSM_QCA402X_SEND_MESSAGE_COMPAT:
	case MSM_QCA402X_RECEIVE_MESSAGE_COMPAT: {
		struct msm_qca_message_type_32 data32;
		struct msm_qca_message_type data;
		__u32 len = 0;

		len = sizeof(struct msm_qca_message_type_32);
		if (copy_from_user(&data32, (void __user *)arg, len))
			return -EFAULT;
		data.channel_id = data32.channel_id;
		data.is_ion_data = data32.is_ion_data;
		data.header_size = data32.header_size;
		data.fd = data32.fd;
		data.data_size = data32.data_size;
		data.cmd = data32.cmd;
		data.last_data = data32.last_data;
		data.header = compat_ptr(data32.header);
		data.buff_addr = compat_ptr(data32.buff_addr);

		ret = msm_qca402_ioctl(file, cmd, (void *)&data);

		data32.header_size = data.header_size;
		data32.fd = data.fd;
		data32.data_size = data.data_size;
		if (copy_to_user((void __user *)arg, (void *)&data32, len))
			return -EFAULT;
	}
		break;
	case MSM_QCA402X_FLUSH_BUFFERS_COMPAT:
	case MSM_QCA402X_ABORT_MESSAGE_COMPAT:
	case MSM_QCA402X_REGISTER_EVENT_COMPAT:
	case MSM_QCA402X_UNREGISTER_EVENT_COMPAT: {
		struct msm_qca_event_list_type_32 data32;
		struct msm_qca_event_list_type data;
		int i;

		len = sizeof(struct msm_qca_event_list_type_32);
		if (copy_from_user(&data32, (void __user *)arg, len))
			return -EFAULT;
		data.num_events = data32.num_events;
		for (i = 0; i < data.num_events; i++)
			data.events[i] = data32.events[i];
		ret = msm_qca402_ioctl(file, cmd, (void *)&data);
	}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
#endif

static void msm_qca402_send_msg_hndl(void *target, __u8 epid,
	__u8 eventid, struct htca_event_info *event_info,
	void *context)
{
	struct msm_qca402_dev_data_t *dev_data =
		(struct msm_qca402_dev_data_t *)context;
	struct msm_qca402_workbuff_list_t *wb;

	wb = (struct msm_qca402_workbuff_list_t *)event_info->cookie;
	if ((!wb) || (!dev_data))
		return;

	wake_up_interruptible(&dev_data->in_queue);

	mutex_lock(&dev_data->dev_mutex);
	dev_data->sending--;
	list_add_tail(&wb->list, &dev_data->work_list);
	mutex_unlock(&dev_data->dev_mutex);

}

static void msm_qca402_recv_msg_hndl(void *target, __u8 epid,
	__u8 eventid, struct htca_event_info *event_info,
	void *context)
{
	struct msm_qca402_dev_data_t *dev_data =
		(struct msm_qca402_dev_data_t *)context;
	struct msm_qca402_workbuff_list_t *wb;

	wb = (struct msm_qca402_workbuff_list_t *)event_info->cookie;
	if ((!wb) || (!dev_data))
		return;

	mutex_lock(&dev_data->dev_mutex);
	dev_data->receiving--;
	wb->size = event_info->actual_length;
	list_add_tail(&wb->list, &dev_data->recv_list);
	mutex_unlock(&dev_data->dev_mutex);
	if (dev_data->sending < dev_data->receiving)
		wake_up_interruptible(&dev_data->in_queue);
	wake_up_interruptible(&dev_data->recv_queue);
}

static void msm_qca402_data_avl_hndl(void *target, __u8 epid,
	__u8 eventid, struct htca_event_info *event_info,
	void *context)
{
	pr_debug("started\n");
}

static int msm_qca402_htca_recv_thread(void *hdl)
{

	int wait;
	struct msm_qca402_dev_data_t *dev_data =
		(struct msm_qca402_dev_data_t *)hdl;
	struct msm_qca402_workbuff_list_t *wb_prev, *wb_curr;
	int status;

	pr_debug("start waiting\n");

	while (!kthread_should_stop()) {
		wait = wait_event_interruptible(dev_data->recv_queue,
			!list_empty(&dev_data->recv_list) ||
			kthread_should_stop());

		if (kthread_should_stop())
			break;

		pr_debug("rcvd\n");
		mutex_lock(&dev_data->dev_mutex);
		wb_prev = list_first_entry(&dev_data->recv_list,
				struct msm_qca402_workbuff_list_t, list);
		list_del_init(&wb_prev->list);

		wb_curr = msm_qca402_get_workbuff(dev_data);
		status = htca_buffer_receive(dev_data->htca_target, 0,
			(void *)&wb_curr->htca_msg, HTCA_MAX_BUFF_SIZE,
			wb_curr);
		dev_data->receiving++;

		if (status)
			pr_err("Receive error\n");

		msm_qca402_process_htca_message(dev_data, wb_prev);
		mutex_unlock(&dev_data->dev_mutex);

	}

	return 0;
}


static int msm_qca402_htca_user_thread(void *hdl)
{
	int ret, wait, i;
	struct msm_qca402_dev_data_t *dev_data =
		(struct msm_qca402_dev_data_t *)hdl;
	struct msm_qca402_ready_list_t *r_entry;
	struct msm_qca402_channel_list_t *ch_data = NULL;
	struct msm_qca402_workbuff_list_t *wb;
	int status;

	pr_debug("started EP\n");
	ret = htca_start(dev_data->htca_target);
	WARN_ON(ret);
	htca_event_reg(dev_data->htca_target, 0, HTCA_EVENT_BUFFER_SENT,
		msm_qca402_send_msg_hndl, dev_data);
	htca_event_reg(dev_data->htca_target, 0, HTCA_EVENT_BUFFER_RECEIVED,
		msm_qca402_recv_msg_hndl, dev_data);
	htca_event_reg(dev_data->htca_target, 0, HTCA_EVENT_DATA_AVAILABLE,
		msm_qca402_data_avl_hndl, dev_data);
	pr_debug("start waiting\n");


	for (i = 0; i < QCA402_START_ENQ_BUFFS; i++) {
		wb = msm_qca402_get_workbuff(dev_data);
		status = htca_buffer_receive(dev_data->htca_target, 0,
			(void *)&wb->htca_msg, HTCA_MAX_BUFF_SIZE, wb);
	}

	dev_data->receiving += QCA402_START_ENQ_BUFFS;

	while (!kthread_should_stop()) {
		wait = wait_event_interruptible(dev_data->in_queue,
			((!list_empty(&dev_data->in_list) || ch_data) &&
			(dev_data->sending < dev_data->receiving)) ||
			kthread_should_stop());

		if (kthread_should_stop())
			break;

		mutex_lock(&dev_data->dev_mutex);
		if (!ch_data) {
			r_entry = list_first_entry(&dev_data->in_list,
					struct msm_qca402_ready_list_t, list);

			ch_data = r_entry->ch_data;
			list_del_init(&r_entry->list);
			kfree(r_entry);
		}
		ret = msm_qca402_send_htca_message(dev_data, ch_data);
		if (ret == -EINVAL) {
			pr_err("Error during message send\n");
			ch_data = NULL;
		} else if (ret == 0) {
			pr_debug("Message sent\n");
			ch_data->ref_cnt--;
			ch_data = NULL;
		}
		mutex_unlock(&dev_data->dev_mutex);
	}

	return 0;
}

static const struct file_operations msm_qca402_ops = {
	.open = msm_qca402_open,
	.poll = msm_qca402_poll,
	.release = msm_qca402_release,
	.unlocked_ioctl = msm_qca402_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = msm_qca402_compat_ioctl,
#endif
};

static int msm_qca402_create_device(struct msm_qca402_dev_data_t **dv,
		int minor)
{
	int ret;
	char name[20];

	*dv = kzalloc(sizeof(**dv), GFP_KERNEL);
	if (!*dv)
		return -ENOMEM;
	snprintf(name, sizeof(name), "%s%d", DEVICE_NAME, minor);
	pr_debug("called for device %s\n", name);
	(*dv)->dev = device_create(msm_qca402_class, NULL,
			MKDEV(msm_qca402_major, minor), NULL, name);
	if (IS_ERR((*dv)->dev)) {
		ret = PTR_ERR((*dv)->dev);
		kfree(*dv);
		*dv = NULL;
		return ret;
	}
	cdev_init(&(*dv)->cdev, &msm_qca402_ops);
	ret = cdev_add(&(*dv)->cdev, MKDEV(msm_qca402_major, minor), 1);
	if (ret < 0) {
		device_unregister((*dv)->dev);
		kfree(*dv);
		*dv = NULL;
	}

	return ret;
}

static void msm_qca402_destroy_device(struct msm_qca402_dev_data_t *dv)
{
	pr_debug("called\n");
	cdev_del(&dv->cdev);
	device_unregister(dv->dev);
	kfree(dv);
}

static void msm_qca402_trgt_avail_hndl(void *target, __u8 epid,
	__u8 eventid, struct htca_event_info *event_info,
	void *context)
{
	struct msm_qca402_dev_data_t *dev_data;
	dev_t devid;
	int ret;
	int i;

	pr_debug("started\n");
	/* get the actual number from dts */
	ret = alloc_chrdev_region(&devid, 0, MAX_DEVICE_NUM, DEVICE_NAME);
	if (ret < 0)
		goto register_error;
	msm_qca402_major = MAJOR(devid);
	msm_qca402_class = class_create(THIS_MODULE, DEVICE_CLASS);
	if (IS_ERR(msm_qca402_class)) {
		ret = PTR_ERR(msm_qca402_class);
		goto class_error;
	}
	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		ret = msm_qca402_create_device(&msm_qca402_device[i].dev,
				i + MINOR(devid));
		if (ret < 0)
			goto device_error;
		dev_data = msm_qca402_device[i].dev;
		dev_data->dev_idx = i;
		dev_data->ion_client =
			msm_ion_client_create("qca_ion_client");
		if (IS_ERR_OR_NULL(dev_data->ion_client)) {
			msm_qca402_destroy_device(dev_data);
			msm_qca402_device[i].dev = NULL;
			goto device_error;
		}

		init_waitqueue_head(&dev_data->in_queue);
		init_waitqueue_head(&dev_data->recv_queue);

		pr_debug("ion done\n");
		INIT_LIST_HEAD(&dev_data->file_list);
		INIT_LIST_HEAD(&dev_data->channel_list);
		INIT_LIST_HEAD(&dev_data->work_list);
		INIT_LIST_HEAD(&dev_data->recv_list);
		INIT_LIST_HEAD(&dev_data->in_list);
		ret = msm_qca402_alloc_add_workbuffs(dev_data,
				QCA402_MIN_NUM_WORKBUFS);
		if (ret < 0)
			goto device_error;
		pr_debug("lists done\n");
		mutex_init(&dev_data->dev_mutex);
		spin_lock_init(&dev_data->lock);
		pr_debug("device[%d] %pK\n", i, dev_data);
		dev_data->htca_target = target;
	}

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		msm_qca402_device[i].user_task = kthread_create(
			msm_qca402_htca_user_thread, dev_data, "qca402_user");
		msm_qca402_device[i].recv_task = kthread_create(
			msm_qca402_htca_recv_thread, dev_data, "qca402_recv");
		wake_up_process(msm_qca402_device[i].user_task);
		wake_up_process(msm_qca402_device[i].recv_task);
	}
	return;

device_error:
	for (--i; i >= 0; i--) {
		if (msm_qca402_device[i].dev) {
			mutex_destroy(&msm_qca402_device[i].dev->dev_mutex);
			msm_qca402_free_workbuffs(msm_qca402_device[i].dev);
			ion_client_destroy(
				msm_qca402_device[i].dev->ion_client);
			msm_qca402_destroy_device(msm_qca402_device[i].dev);
			msm_qca402_device[i].dev = NULL;
		}
	}
	class_destroy(msm_qca402_class);
class_error:
	unregister_chrdev(msm_qca402_major, DEVICE_NAME);
register_error:
	pr_err("error: %d\n", ret);
}

static void msm_qca402_trgt_unavail_hndl(void *target, __u8 epid,
	__u8 eventid, struct htca_event_info *event_info,
	void *context)
{
	int i;

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (msm_qca402_device[i].dev) {
			kthread_stop(msm_qca402_device[i].user_task);
			kthread_stop(msm_qca402_device[i].recv_task);
			htca_stop(msm_qca402_device[i].dev->htca_target);
			mutex_destroy(&msm_qca402_device[i].dev->dev_mutex);
			msm_qca402_free_workbuffs(msm_qca402_device[i].dev);
			ion_client_destroy(
				msm_qca402_device[i].dev->ion_client);
			msm_qca402_destroy_device(msm_qca402_device[i].dev);
			msm_qca402_device[i].dev = NULL;
		}
	}
	class_unregister(msm_qca402_class);
	class_destroy(msm_qca402_class);
	unregister_chrdev(msm_qca402_major, DEVICE_NAME);
}

static int msm_qca402_probe(struct platform_device *pdev)
{
	const struct of_device_id *match_dev;
	int status;

	if (pdev->dev.of_node) {
		pr_debug("device found\n");
		match_dev = of_match_device(pdev->dev.driver->of_match_table,
			&pdev->dev);
	}

	pr_debug("called\n");
	if (htca_init())
		return -EINVAL;

	pr_debug("register\n");
	status = htca_event_reg(NULL, 0, HTCA_EVENT_TARGET_AVAILABLE,
		msm_qca402_trgt_avail_hndl, NULL);
	if (status != HTCA_OK)
		return -EINVAL;
	status = htca_event_reg(NULL, 0, HTCA_EVENT_TARGET_UNAVAILABLE,
		msm_qca402_trgt_unavail_hndl, NULL);
	if (status != HTCA_OK)
		return -EINVAL;
	pr_debug("register_end\n");

	return 0;
}

static int msm_qca402_remove(struct platform_device *pdev)
{
	int i, stop_flag = 0;

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (msm_qca402_device[i].dev) {
			stop_flag = 1;
			kthread_stop(msm_qca402_device[i].user_task);
			kthread_stop(msm_qca402_device[i].recv_task);
			htca_stop(msm_qca402_device[i].dev->htca_target);
			mutex_destroy(&msm_qca402_device[i].dev->dev_mutex);
			msm_qca402_free_workbuffs(msm_qca402_device[i].dev);
			ion_client_destroy(
				msm_qca402_device[i].dev->ion_client);
			msm_qca402_destroy_device(msm_qca402_device[i].dev);
			msm_qca402_device[i].dev = NULL;
		}
	}
	if (stop_flag) {
		class_unregister(msm_qca402_class);
		class_destroy(msm_qca402_class);
		unregister_chrdev(msm_qca402_major, DEVICE_NAME);
	}
	htca_shutdown();
	return 0;
}

static struct platform_driver msm_qca402_driver = {
	.probe = msm_qca402_probe,
	.remove = msm_qca402_remove,
	.driver = {
		.name = "msm_qca402",
		.owner = THIS_MODULE,
		.of_match_table = msm_qca402_dt_match,
	},
};

static int __init msm_qca402_init(void)
{
	platform_driver_register(&msm_qca402_driver);
	return 0;
}

static void __exit msm_qca402_exit(void)
{
}

module_init(msm_qca402_init);
module_exit(msm_qca402_exit);

MODULE_DESCRIPTION("Driver for QCA402x HTCA High level communication");
MODULE_LICENSE("GPL v2");
