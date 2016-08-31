/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/debugfs.h>

#include "nvshm_types.h"
#include "nvshm_if.h"
#include "nvshm_priv.h"
#include "nvshm_ipc.h"
#include "nvshm_queue.h"
#include "nvshm_iobuf.h"

struct nvshm_channel *nvshm_open_channel(int chan,
					 struct nvshm_if_operations *ops,
					 void *interface_data)
{
	struct nvshm_handle *handle = nvshm_get_handle();

	pr_debug("%s(%d)\n", __func__, chan);
	spin_lock(&handle->lock);
	if (handle->chan[chan].ops) {
		pr_err("%s: already registered on chan %d\n", __func__, chan);
		return NULL;
	}

	handle->chan[chan].ops = ops;
	handle->chan[chan].data = interface_data;
	spin_unlock(&handle->lock);
	return &handle->chan[chan];
}

void nvshm_close_channel(struct nvshm_channel *handle)
{
	struct nvshm_handle *priv = nvshm_get_handle();

	/* we cannot flush the work queue here as the call to
	   nvshm_close_channel() is made from cleanup_interfaces(),
	   which executes from the context of the work queue

	   additionally, flushing the work queue is unnecessary here
	   as the main work queue handler always checks the state of
	   the IPC */

	spin_lock(&priv->lock);
	priv->chan[handle->index].ops = NULL;
	priv->chan[handle->index].data = NULL;
	spin_unlock(&priv->lock);
}

int nvshm_write(struct nvshm_channel *handle, struct nvshm_iobuf *iob)
{
	struct nvshm_handle *priv = nvshm_get_handle();
	struct nvshm_iobuf *list, *leaf;
	int count = 0, ret = 0;

	spin_lock_bh(&priv->lock);
	if (!priv->chan[handle->index].ops) {
		pr_err("%s: channel not mapped\n", __func__);
		spin_unlock_bh(&priv->lock);
		return -EINVAL;
	}

	list = iob;
	while (list) {
		count++;
		leaf = list->sg_next;
		while (leaf) {
			count++;
			leaf = NVSHM_B2A(priv, leaf);
			leaf = leaf->sg_next;
		}
		list = list->next;
		if (list)
			list = NVSHM_B2A(priv, list);
	}
	priv->chan[handle->index].rate_counter -= count;
	if (priv->chan[handle->index].rate_counter < 0) {
		priv->chan[handle->index].xoff = 1;
		pr_warn("%s: rate limit hit on chan %d\n", __func__,
			handle->index);
		ret = 1;
	}

	iob->chan = handle->index;
	iob->qnext = NULL;
	nvshm_queue_put(priv, iob);
	nvshm_generate_ipc(priv);
	spin_unlock_bh(&priv->lock);
	return ret;
}

/* Defered to nvshm_wq because it can be called from atomic context */
void nvshm_start_tx(struct nvshm_channel *handle)
{
	struct nvshm_handle *priv = nvshm_get_handle();
	queue_work(priv->nvshm_wq, &handle->start_tx_work);
}
