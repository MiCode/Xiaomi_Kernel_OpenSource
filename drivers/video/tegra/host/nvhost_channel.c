/*
 * drivers/video/tegra/host/nvhost_channel.c
 *
 * Tegra Graphics Host Channel
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nvhost_channel.h"
#include "dev.h"
#include "nvhost_acm.h"
#include "nvhost_job.h"
#include "chip_support.h"

#include <trace/events/nvhost.h>
#include <linux/nvhost_ioctl.h>
#include <linux/slab.h>

#define NVHOST_CHANNEL_LOW_PRIO_MAX_WAIT 50

int nvhost_channel_init(struct nvhost_channel *ch,
		struct nvhost_master *dev, int index)
{
	int err;
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);

	/* Link platform_device to nvhost_channel */
	err = channel_op().init(ch, dev, index);
	if (err < 0) {
		dev_err(&dev->dev->dev, "failed to init channel %d\n",
				index);
		return err;
	}
	pdata->channel = ch;

	return 0;
}

int nvhost_channel_submit(struct nvhost_job *job)
{
	/*
	 * Check if queue has higher priority jobs running. If so, wait until
	 * queue is empty. Ignores result from nvhost_cdma_flush, as we submit
	 * either when push buffer is empty or when we reach the timeout.
	 */
	int higher_count = 0;

	switch (job->priority) {
	case NVHOST_PRIORITY_HIGH:
		higher_count = 0;
		break;
	case NVHOST_PRIORITY_MEDIUM:
		higher_count = job->ch->cdma.high_prio_count;
		break;
	case NVHOST_PRIORITY_LOW:
		higher_count = job->ch->cdma.high_prio_count
			+ job->ch->cdma.med_prio_count;
		break;
	}
	if (higher_count > 0)
		(void)nvhost_cdma_flush(&job->ch->cdma,
				NVHOST_CHANNEL_LOW_PRIO_MAX_WAIT);

	return channel_op().submit(job);
}

struct nvhost_channel *nvhost_getchannel(struct nvhost_channel *ch)
{
	int err = 0;
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);

	mutex_lock(&ch->reflock);
	if (ch->refcount == 0) {
		if (pdata->init)
			pdata->init(ch->dev);
		err = nvhost_cdma_init(&ch->cdma);
	} else if (pdata->exclusive) {
		err = -EBUSY;
	}
	if (!err)
		ch->refcount++;

	mutex_unlock(&ch->reflock);

	/* Keep alive modules that needs to be when a channel is open */
	if (!err && pdata->keepalive)
		nvhost_module_busy(ch->dev);

	return err ? NULL : ch;
}

void nvhost_putchannel(struct nvhost_channel *ch, struct nvhost_hwctx *ctx)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	BUG_ON(!channel_cdma_op().stop);

	if (ctx) {
		mutex_lock(&ch->submitlock);
		if (ch->cur_ctx == ctx)
			ch->cur_ctx = NULL;
		mutex_unlock(&ch->submitlock);
	}

	/* Allow keep-alive'd module to be turned off */
	if (pdata->keepalive)
		nvhost_module_idle(ch->dev);

	mutex_lock(&ch->reflock);
	if (ch->refcount == 1) {
		channel_cdma_op().stop(&ch->cdma);
		nvhost_cdma_deinit(&ch->cdma);
		if (pdata->deinit)
			pdata->deinit(ch->dev);
	}
	ch->refcount--;
	mutex_unlock(&ch->reflock);
}

int nvhost_channel_suspend(struct nvhost_channel *ch)
{
	int ret = 0;

	mutex_lock(&ch->reflock);
	BUG_ON(!channel_cdma_op().stop);

	if (ch->refcount) {
		ret = nvhost_module_suspend(ch->dev);
		if (!ret)
			channel_cdma_op().stop(&ch->cdma);
	}
	mutex_unlock(&ch->reflock);

	return ret;
}

struct nvhost_channel *nvhost_alloc_channel_internal(int chindex,
	int max_channels, int *current_channel_count)
{
	struct nvhost_channel *ch = NULL;

	if ( (chindex > max_channels) ||
	     ( (*current_channel_count + 1) > max_channels) )
		return NULL;
	else {
		ch = kzalloc(sizeof(*ch), GFP_KERNEL);
		if (ch == NULL)
			return NULL;
		else {
			(*current_channel_count)++;
			return ch;
		}
	}
}

void nvhost_free_channel_internal(struct nvhost_channel *ch,
	int *current_channel_count)
{
	kfree(ch);
	(*current_channel_count)--;
}

int nvhost_channel_save_context(struct nvhost_channel *ch)
{
	struct nvhost_hwctx *cur_ctx = ch->cur_ctx;
	int err = 0;
	if (cur_ctx)
		err = channel_op().save_context(ch);

	return err;

}

int nvhost_channel_read_reg(struct nvhost_channel *ch,
	struct nvhost_hwctx *hwctx,
	u32 offset, u32 *value)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	if (!pdata->read_reg)
		return -EINVAL;

	return pdata->read_reg(ch->dev, ch, hwctx, offset, value);
}

int nvhost_channel_drain_read_fifo(struct nvhost_channel *ch,
	u32 *ptr, unsigned int count, unsigned int *pending)
{
	return channel_op().drain_read_fifo(ch, ptr, count, pending);
}
