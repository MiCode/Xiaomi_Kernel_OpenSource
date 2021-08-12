// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "share_mem " fmt

#include <linux/err.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/io.h>
#include <linux/slab.h>

#include "sensor_comm.h"
#include "share_memory.h"
#include "hf_sensor_type.h"


struct share_mem_config_handle {
	int (*handler)(struct share_mem_config *cfg, void *private_data);
	void *private_data;
};

struct share_mem_usage {
	uint8_t payload_type;
	bool init_status;
	int id;
};

static struct share_mem_config_handle shm_handle[MAX_SHARE_MEM_PAYLOAD_TYPE];

static struct share_mem_usage shm_usage_table[] = {
	{
		.payload_type = SHARE_MEM_DATA_PAYLOAD_TYPE,
		.id = SENS_MEM_ID,
	},
	{
		.payload_type = SHARE_MEM_SUPER_DATA_PAYLOAD_TYPE,
		.id = SENS_SUPER_MEM_ID,
	},
	{
		.payload_type = SHARE_MEM_LIST_PAYLOAD_TYPE,
		.id = SENS_LIST_MEM_ID,
	},
	{
		.payload_type = SHARE_MEM_DEBUG_PAYLOAD_TYPE,
		.id = SENS_DEBUG_MEM_ID,
	},
	{
		.payload_type = SHARE_MEM_CUSTOM_W_PAYLOAD_TYPE,
		.id = SENS_CUSTOM_W_MEM_ID,
	},
	{
		.payload_type = SHARE_MEM_CUSTOM_R_PAYLOAD_TYPE,
		.id = SENS_CUSTOM_R_MEM_ID,
	},
};

static int share_mem_notify(struct share_mem *shm,
		struct share_mem_notify *notify)
{
	int ret = 0;
	struct sensor_comm_notify n;

	if (shm->write_position == shm->last_write_position)
		return 0;

	n.sequence = notify->sequence;
	n.sensor_type = notify->sensor_type;
	n.command = notify->notify_cmd;
	n.value[0] = shm->write_position;
	n.length = sizeof(n.value[0]);
	ret = sensor_comm_notify(&n);
	if (ret < 0)
		return ret;
	shm->last_write_position = shm->write_position;
	return ret;
}

static void share_mem_buffer_full_detect(struct share_mem *shm,
		uint32_t curr_written)
{
	int ret = 0;
	uint32_t rp = 0, wp = 0, buffer_size = 0;
	struct share_mem_notify notify;

	shm->buffer_full_written += curr_written;
	if (shm->buffer_full_written < shm->buffer_full_threshold)
		return;

	rp = shm->base->rp;
	wp = shm->base->wp;
	buffer_size = shm->base->buffer_size;

	shm->buffer_full_written = (wp > rp) ?
		(wp - rp) : (buffer_size - rp + wp);
	if (shm->buffer_full_written >= shm->buffer_full_threshold) {
		notify.sequence = 0;
		notify.sensor_type = SENSOR_TYPE_INVALID;
		notify.notify_cmd = shm->buffer_full_cmd;
		ret = share_mem_notify(shm, &notify);
		if (ret < 0)
			pr_err("%s buffer full notify fail %d\n",
				shm->name, ret);
		else
			shm->buffer_full_written = 0;
	}
}

int share_mem_seek(struct share_mem *shm, uint32_t write_position)
{
	if (!shm->base)
		return -EINVAL;

	mutex_lock(&shm->lock);
	shm->write_position = write_position;
	mutex_unlock(&shm->lock);
	return 0;
}

int share_mem_read_reset(struct share_mem *shm)
{
	if (!shm->base)
		return -EINVAL;

	mutex_lock(&shm->lock);
	shm->base->rp = 0;
	shm->write_position = 0;
	mutex_unlock(&shm->lock);
	return 0;
}

int share_mem_write_reset(struct share_mem *shm)
{
	if (!shm->base)
		return -EINVAL;

	mutex_lock(&shm->lock);
	shm->base->wp = 0;
	shm->write_position = 0;
	shm->last_write_position = 0;
	shm->buffer_full_written = 0;
	mutex_unlock(&shm->lock);
	return 0;
}

static int share_mem_read_dram(struct share_mem *shm,
		void *buf, uint32_t count)
{
	uint32_t rp = 0, wp = 0, buffer_size = 0, item_size = 0;
	uint8_t *src = NULL, *dst = buf;
	uint32_t first = 0, second = 0, read = 0;

	if (!shm->item_size || count % shm->item_size)
		return -EINVAL;

	wp = shm->write_position;
	rp = shm->base->rp;
	buffer_size = shm->base->buffer_size;
	item_size = shm->base->item_size;
	src = (uint8_t *)shm->base + offsetof(struct share_mem_base, data);

	if (item_size != shm->item_size)
		return -EIO;

	if (wp == rp)
		return 0;

	if (wp > rp) {
		first = wp - rp;
		second = 0;
	} else {
		first = buffer_size - rp;
		second = wp;
	}

	first = min(first, count);
	second = min(count - first, second);

	memcpy_fromio(dst, src + rp, first);
	memcpy_fromio(dst + first, src, second);
	read = first + second;
	rp += read;
	rp %= buffer_size;

	/*
	 * make sure that the data is copied before
	 * incrementing the rp index counter
	 */
	smp_wmb();
	shm->base->rp = rp;

	return read;
}

int share_mem_read(struct share_mem *shm, void *buf, uint32_t count)
{
	int ret = 0;

	if (!shm->base || !buf || !count)
		return -EINVAL;

	mutex_lock(&shm->lock);
	ret = share_mem_read_dram(shm, buf, count);
	mutex_unlock(&shm->lock);
	return ret;
}

static int share_mem_write_dram(struct share_mem *shm,
		void *buf, uint32_t count)
{
	uint32_t rp = 0, wp = 0, buffer_size = 0, item_size = 0, write = 0;
	uint8_t *src = buf, *dst = NULL;

	if (!shm->item_size || count % shm->item_size)
		return -EINVAL;

	rp = shm->base->rp;
	wp = shm->base->wp;
	buffer_size = shm->base->buffer_size;
	item_size = shm->base->item_size;
	dst = (uint8_t *)shm->base + offsetof(struct share_mem_base, data);

	if (item_size != shm->item_size)
		return -EIO;

	/* remain 1 count */
	while ((write < count) && ((wp + item_size) % buffer_size != rp)) {
		memcpy_toio(dst + wp, src + write, item_size);
		write += item_size;
		wp += item_size;
		wp %= buffer_size;
	}

	if (!write)
		return 0;

	/*
	 * make sure that the data is copied before
	 * incrementing the wp index counter
	 */
	smp_wmb();
	shm->base->wp = wp;
	shm->write_position = wp;

	if (shm->buffer_full_detect)
		share_mem_buffer_full_detect(shm, write);

	return write;
}

int share_mem_write(struct share_mem *shm, void *buf, uint32_t count)
{
	int ret = 0;

	if (!shm->base || !buf || !count)
		return -EINVAL;

	mutex_lock(&shm->lock);
	ret = share_mem_write_dram(shm, buf, count);
	mutex_unlock(&shm->lock);
	return ret;
}

int share_mem_flush(struct share_mem *shm, struct share_mem_notify *notify)
{
	int ret = 0;

	if (!shm->base)
		return -EINVAL;

	mutex_lock(&shm->lock);
	ret = share_mem_notify(shm, notify);
	mutex_unlock(&shm->lock);
	return ret;
}

int share_mem_init(struct share_mem *shm, struct share_mem_config *cfg)
{
	int ret = 0;

	if (!shm->name || !shm->item_size)
		return -EINVAL;

	mutex_init(&shm->lock);

	cfg->base->wp = 0;
	cfg->base->rp = 0;
	cfg->base->item_size = shm->item_size;
	cfg->base->buffer_size =
		(((long)cfg->buffer_size -
		offsetof(struct share_mem_base, data)) /
		shm->item_size) * shm->item_size;

	shm->write_position = 0;
	shm->last_write_position = 0;

	if (shm->buffer_full_detect) {
		shm->buffer_full_written = 0;
		shm->buffer_full_threshold =
			((uint32_t)(((cfg->base->buffer_size - shm->item_size) /
			shm->item_size) * 8 / 10)) *
			shm->item_size;
		if (shm->buffer_full_threshold <= shm->item_size) {
			ret = -EINVAL;
			goto exit;
		}
	}

	shm->base = cfg->base;
exit:
	return ret;
}

static int share_mem_send_config(void)
{
	int ret = 0;
	uint32_t i = 0, index = 0;
	struct share_mem_usage *usage = NULL;
	struct sensor_comm_ctrl *ctrl = NULL;
	struct sensor_comm_share_mem *comm_shm = NULL;

	ctrl = kzalloc(sizeof(*ctrl) + sizeof(*comm_shm), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	ctrl->sensor_type = 0;
	ctrl->command = SENS_COMM_CTRL_SHARE_MEMORY_CMD;
	ctrl->length = sizeof(*comm_shm);
	comm_shm = (struct sensor_comm_share_mem *)ctrl->data;

	memset(comm_shm, 0, sizeof(*comm_shm));
	for (i = 0; i < ARRAY_SIZE(shm_usage_table); i++) {
		usage = &shm_usage_table[i];
		if (usage->payload_type >= MAX_SHARE_MEM_PAYLOAD_TYPE)
			continue;
		/* host init share mem ready we can send share mem to scp */
		if (usage->init_status) {
			comm_shm->base_info[index].payload_type =
				usage->payload_type;
			comm_shm->base_info[index].payload_base =
				(uint32_t)scp_get_reserve_mem_phys(usage->id);
			WARN_ON(!comm_shm->base_info[index].payload_base);
			comm_shm->available_num = ++index;
		}
		if (index == ARRAY_SIZE(comm_shm->base_info) ||
		    (i == (ARRAY_SIZE(shm_usage_table) - 1) && index)) {
			ret = sensor_comm_ctrl_send(ctrl,
				sizeof(*ctrl) + ctrl->length);
			if (ret < 0)
				break;
			index = 0;
			memset(comm_shm, 0, sizeof(*comm_shm));
		}
	}
	kfree(ctrl);
	return ret;
}

int share_mem_config(void)
{
	int ret = 0;
	uint32_t i = 0;
	struct share_mem_config cfg;
	struct share_mem_config_handle *handle = NULL;
	struct share_mem_usage *usage = NULL;

	for (i = 0; i < ARRAY_SIZE(shm_usage_table); i++) {
		usage = &shm_usage_table[i];
		/* must reset init_status to false scp reset each times */
		usage->init_status = false;
		if (usage->payload_type >= MAX_SHARE_MEM_PAYLOAD_TYPE) {
			pr_err("payload type %u invalid index %u\n",
				usage->payload_type, i);
			BUG_ON(1);
		}
		handle = &shm_handle[usage->payload_type];
		if (!handle->handler) {
			pr_err("payload type %u handler NULL index %u\n",
				usage->payload_type, i);
			BUG_ON(1);
		}
		memset(&cfg, 0, sizeof(cfg));
		cfg.payload_type = usage->payload_type;
		cfg.base =
			(void *)(long)scp_get_reserve_mem_virt(usage->id);
		cfg.buffer_size =
			(uint32_t)scp_get_reserve_mem_size(usage->id);
		BUG_ON(!cfg.base);
		ret = handle->handler(&cfg, handle->private_data);
		if (ret < 0)
			continue;
		usage->init_status = true;
	}

	return share_mem_send_config();
}

void share_mem_config_handler_register(uint8_t payload_type,
		int (*f)(struct share_mem_config *cfg, void *private_data),
		void *private_data)
{
	if (payload_type >= MAX_SHARE_MEM_PAYLOAD_TYPE)
		return;

	shm_handle[payload_type].private_data = private_data;
	shm_handle[payload_type].handler = f;
}

void share_mem_config_handler_unregister(uint8_t payload_type)
{
	if (payload_type >= MAX_SHARE_MEM_PAYLOAD_TYPE)
		return;

	shm_handle[payload_type].handler = NULL;
	shm_handle[payload_type].private_data = NULL;
}
