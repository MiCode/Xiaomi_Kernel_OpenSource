// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[share_memory] " fmt

#include <linux/err.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/io.h>
#include <linux/slab.h>

#include "sensor_comm.h"
#include "share_memory.h"
#include "hf_sensor_type.h"

#define SHARE_MEM_FULL_THRESHOLD_SCALE 0.8 // 80% of full buffer size

struct share_mem_config_handle {
	const uint8_t notify_cmd;
	const uint32_t id;
	int (*handler)(struct share_mem_cfg *cfg, void *private_data);
	void *private_data;
	bool done;
};

static struct share_mem_config_handle
share_mem_config_handle[MAX_SENS_COMM_NOTIFY_CMD] = {
	{
		.notify_cmd = SENS_COMM_NOTIFY_DATA_CMD,
		.id = SHARE_MEM_SENSOR_DATA_ID,
	},
	{
		.notify_cmd = SENS_COMM_NOTIFY_FULL_CMD,
		.id = SHARE_MEM_MAX_ID,		//not used, set max
	},
	{
		.notify_cmd = SENS_COMM_NOTIFY_READY_CMD,
		.id = SHARE_MEM_MAX_ID,		//not used, set max
	},
	{
		.notify_cmd = SENS_COMM_NOTIFY_LIST_CMD,
		.id = SHARE_MEM_SENSOR_LIST_ID,
	},
	{
		.notify_cmd = SENS_COMM_NOTIFY_DEBUG_CMD,
		.id = SHARE_MEM_MAX_ID,		//not used, set max
	},
};

static int share_mem_notify_func(struct share_mem *shm,
		struct share_mem_notify *notify)
{
	int ret = 0;
	struct sensor_comm_notify comm_notify;

	if (shm->write_position != shm->last_write_position) {
		comm_notify.sensor_type = notify->sensor_type;
		comm_notify.command = notify->notify_cmd;
		comm_notify.sequence = notify->sequence;
		comm_notify.value[0] = shm->write_position;
		comm_notify.length = sizeof(comm_notify.value[0]);

		ret = sensor_comm_notify(&comm_notify);
		if (ret >= 0)
			shm->last_write_position = shm->write_position;
	}

	return ret;
}

int share_mem_read_reset(struct share_mem *shm)
{
	if (!shm || !shm->base)
		return -EINVAL;

	mutex_lock(&shm->mutex_lock);
	shm->base->rp = 0;
	shm->write_position = 0;
	mutex_unlock(&shm->mutex_lock);

	return 0;
}

int share_mem_seek(struct share_mem *shm, uint32_t wp)
{
	if (!shm || !shm->base)
		return -EINVAL;

	mutex_lock(&shm->mutex_lock);
	shm->write_position = wp;
	mutex_unlock(&shm->mutex_lock);

	return 0;
}

static int share_mem_read_dram(struct share_mem *shm,
		void *buf, uint32_t count)
{
	uint32_t rp = 0;
	uint32_t wp = 0;
	uint32_t buffer_size = 0;
	uint32_t item_size = 0;
	uint8_t *src = NULL;
	uint8_t *dst = buf;
	uint32_t len = 0;
	uint32_t off = 0;
	uint32_t l = 0;

	if (!shm->item_size || (count % shm->item_size))
		return -EINVAL;

	wp = shm->write_position;
	rp = shm->base->rp;
	buffer_size = shm->base->buffer_size;
	item_size = shm->base->item_size;
	src = (uint8_t *)shm->base + offsetof(struct share_mem_tag, data);

	if (item_size != shm->item_size) {
		pr_err("%s invalid item_size:%u shm item_size:%u\n",
			shm->name, item_size, shm->item_size);
		return -EIO;
	}

	if (wp == rp) {
		pr_err_ratelimited("%s empty\n", shm->name);
		return 0;
	}

	len = wp - rp;
	if (len > buffer_size) {
		pr_err("%s invalid wp:%u rp:%u size:%u\n",
			shm->name, wp, rp, buffer_size);
		return -EIO;
	}

	len = min(len, count);
	off = rp % buffer_size;
	l = min(len, buffer_size - off);

	memcpy_fromio(dst, src + off, l);
	memcpy_fromio(dst + l, src, len - l);

	/*
	 * make sure that the data is copied before
	 * incrementing the rp index counter
	 */
	smp_wmb();
	rp += len;
	shm->base->rp = rp;

	return len;
}

int share_mem_read(struct share_mem *shm, void *buf, uint32_t count)
{
	int ret = 0;

	if (!shm || !shm->base || !buf || !count)
		return -EINVAL;

	mutex_lock(&shm->mutex_lock);
	ret = share_mem_read_dram(shm, buf, count);
	mutex_unlock(&shm->mutex_lock);

	return ret;
}

int share_mem_write_reset(struct share_mem *shm)
{
	if (!shm || !shm->base)
		return -EINVAL;

	mutex_lock(&shm->mutex_lock);
	shm->base->wp = 0;
	shm->write_position = 0;
	shm->last_write_position = 0;
	shm->buffer_full_wp = 0;
	mutex_unlock(&shm->mutex_lock);

	return 0;
}

static void share_mem_buffer_full_detect(struct share_mem *shm)
{
	int ret = 0;
	uint32_t rp = 0;
	uint32_t wp = 0;
	struct share_mem_notify notify;

	if (shm->buffer_full_wp < shm->buffer_full_threshold)
		return;

	rp = shm->base->rp;
	wp = shm->base->wp;

	shm->buffer_full_wp = wp - rp;

	if (shm->buffer_full_wp >= shm->buffer_full_threshold) {
		notify.sensor_type = SENSOR_TYPE_INVALID;
		notify.notify_cmd = shm->buffer_full_cmd;
		notify.sequence = 0;
		ret = share_mem_notify_func(shm, &notify);
		if (ret >= 0)
			shm->buffer_full_wp = 0;
		else
			pr_err("%s notify fail! ret:%d\n", __func__, ret);
	}
}

static int share_mem_write_dram(struct share_mem *shm,
		void *buf, uint32_t count)
{
	uint32_t rp = 0;
	uint32_t wp = 0;
	uint32_t buffer_size = 0;
	uint32_t item_size = 0;
	uint8_t *src = buf;
	uint8_t *dst = NULL;
	uint32_t len = 0;
	uint32_t off = 0;
	uint32_t l = 0;

	if (!shm->item_size || (count % shm->item_size))
		return -EINVAL;

	rp = shm->base->rp;
	wp = shm->base->wp;
	buffer_size = shm->base->buffer_size;
	item_size = shm->base->item_size;
	dst = (uint8_t *)shm->base + offsetof(struct share_mem_tag, data);

	if ((item_size != shm->item_size) ||
		(count > buffer_size)) {
		pr_err("%s invalid item_size:%u shm item_size:%u count:%u size:%u\n",
			shm->name, item_size, shm->item_size,
			count, buffer_size);
		return -EIO;
	}

	len = wp - rp;
	if (len > buffer_size) {
		pr_err("%s invalid wp:%u rp:%u size:%u\n",
			shm->name, wp, rp, buffer_size);
		return -EIO;
	}
	len = buffer_size - len;
	if (!len)
		return 0;

	len = min(len, count);
	off = wp % buffer_size;
	l = min(len, buffer_size - off);

	memcpy_toio(dst + off, src, l);
	memcpy_toio(dst, src + l, len - l);

	/*
	 * make sure that the data is copied before
	 * incrementing the wp index counter
	 */
	smp_wmb();
	wp += len;
	shm->base->wp = wp;
	shm->write_position = wp;

	if (shm->buffer_full_detect) {
		shm->buffer_full_wp += len;
		share_mem_buffer_full_detect(shm);
	}

	return len;
}

int share_mem_write(struct share_mem *shm, void *buf, uint32_t count)
{
	int ret = 0;

	if (!shm || !shm->base || !buf || !count)
		return -EINVAL;

	mutex_lock(&shm->mutex_lock);
	ret = share_mem_write_dram(shm, buf, count);
	mutex_unlock(&shm->mutex_lock);

	return ret;
}

int share_mem_flush(struct share_mem *shm, struct share_mem_notify *notify)
{
	int ret = 0;

	if (!shm || !shm->base || !notify)
		return -EINVAL;

	mutex_lock(&shm->mutex_lock);
	ret = share_mem_notify_func(shm, notify);
	mutex_unlock(&shm->mutex_lock);

	return ret;
}

void share_mem_config_handler_register(uint8_t notify_cmd,
		int (*f)(struct share_mem_cfg *cfg, void *private_data),
		void *private_data)
{
	if (notify_cmd >= MAX_SENS_COMM_NOTIFY_CMD)
		return;

	share_mem_config_handle[notify_cmd].private_data = private_data;
	share_mem_config_handle[notify_cmd].handler = f;
}

void share_mem_config_handler_unregister(uint8_t notify_cmd)
{
	if (notify_cmd >= MAX_SENS_COMM_NOTIFY_CMD)
		return;

	share_mem_config_handle[notify_cmd].handler = NULL;
	share_mem_config_handle[notify_cmd].private_data = NULL;
}

static int share_mem_set_config_to_scp(void)
{
	int ret = 0;
	uint32_t i = 0;
	struct share_mem_config_handle *handle = NULL;
	struct sensor_comm_ctrl *ctrl = NULL;
	struct sensor_comm_share_mem *comm_shm = NULL;
	uint32_t info_count = 0;

	ctrl = kzalloc(sizeof(*ctrl) + sizeof(*comm_shm), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	ctrl->sensor_type = 0;
	ctrl->command = SENS_COMM_CTRL_SHARE_MEMORY_CMD;
	ctrl->length = sizeof(*comm_shm);
	comm_shm = (struct sensor_comm_share_mem *)ctrl->data;

	info_count = 0;
	memset(comm_shm, 0x00, sizeof(*comm_shm));

	while (i < MAX_SENS_COMM_NOTIFY_CMD) {
		handle = &share_mem_config_handle[i];

		if ((handle->notify_cmd == i) &&
			(handle->id < SHARE_MEM_MAX_ID) && handle->done) {
			comm_shm->base_info[info_count].notify_cmd =
				handle->notify_cmd;
			comm_shm->base_info[info_count].buffer_base =
				(uint32_t)share_mem_get_phy_addr(handle->id);
			++info_count;
		}

		if (info_count == ARRAY_SIZE(comm_shm->base_info) ||
			((i == (MAX_SENS_COMM_NOTIFY_CMD - 1)) && info_count)) {
			ret = sensor_comm_ctrl_send(ctrl,
				sizeof(*ctrl) + ctrl->length);
			if (ret < 0)
				break;
			info_count = 0;
			memset(comm_shm, 0x00, sizeof(*comm_shm));
		}

		++i;
	}
	kfree(ctrl);
	return ret;
}

void share_mem_config(void)
{
	int ret = 0;
	uint32_t i = 0;
	struct share_mem_cfg cfg;
	struct share_mem_config_handle *handle = NULL;

	/*clear config status firstly for scp reboot*/
	for (i = 0; i < MAX_SENS_COMM_NOTIFY_CMD; i++)
		share_mem_config_handle[i].done = false;

	for (i = 0; i < MAX_SENS_COMM_NOTIFY_CMD; i++) {
		handle = &share_mem_config_handle[i];

		if ((handle->notify_cmd != i) ||
			(handle->id >= SHARE_MEM_MAX_ID)) {
			pr_err("share mem cmd:%d map fail or not used, [%d, %d]\n",
				i, handle->notify_cmd, handle->id);
			continue;
		}

		if (!handle->handler) {
			pr_err("share mem cmd:%d didn't register cfg\n", i);
			continue;
		}

		memset(&cfg, 0x00, sizeof(cfg));
		cfg.notify_cmd = handle->notify_cmd;
		cfg.base = (struct share_mem_tag *)
			(long)share_mem_get_virt_addr(handle->id);
		cfg.buffer_size = (uint32_t)share_mem_get_phy_size(handle->id);

		ret = handle->handler(&cfg, handle->private_data);
		if (ret < 0) {
			pr_err("share mem cmd:%d cfg fail, ret:%d\n", i, ret);
			continue;
		}
		handle->done = true;
	}

	ret = share_mem_set_config_to_scp();
	if (ret < 0)
		pr_err("%s share_mem_set_config_to_scp fail, ret:%d\n",
			__func__, ret);
}

int share_mem_init(struct share_mem *shm, struct share_mem_cfg *cfg)
{
	int ret = 0;

	if (!shm || !shm->name || !shm->item_size || !cfg)
		return -EINVAL;

	if (!cfg->base) {
		WARN_ON(1);
		return -EINVAL;
	}

	if (!shm->base)
		mutex_init(&shm->mutex_lock);

	mutex_lock(&shm->mutex_lock);

	cfg->base->wp = 0;
	cfg->base->rp = 0;
	cfg->base->item_size = shm->item_size;
	cfg->base->buffer_size =
		(((long)cfg->buffer_size -
		offsetof(struct share_mem_tag, data)) /
		shm->item_size) * shm->item_size;

	pr_err("share_mem_data: %s, base:%p, wp:%d, rp:%d, buffer_size:%d, item_size:%d\n",
		shm->name,
		cfg->base,
		cfg->base->wp,
		cfg->base->rp,
		cfg->base->buffer_size,
		cfg->base->item_size);

	if (cfg->base->buffer_size < cfg->base->item_size) {
		ret = -EINVAL;
		goto exit;
	}

	shm->write_position = 0;
	shm->last_write_position = 0;

	if (shm->buffer_full_detect) {
		shm->buffer_full_wp = 0;
		shm->buffer_full_threshold =
			((uint32_t)((cfg->base->buffer_size / shm->item_size) *
			SHARE_MEM_FULL_THRESHOLD_SCALE)) * shm->item_size;
		pr_err("%s %s full cmd:%u threshold:%d\n",
			__func__, shm->name, shm->buffer_full_cmd,
			shm->buffer_full_threshold);

		if (shm->buffer_full_cmd == 0 ||
			shm->buffer_full_threshold <= shm->item_size) {
			ret = -EINVAL;
			goto exit;
		}
	}

	shm->base = cfg->base;
exit:
	mutex_unlock(&shm->mutex_lock);
	return ret;
}
