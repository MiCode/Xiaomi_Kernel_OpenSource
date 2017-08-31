/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "hab.h"

struct virtual_channel *
hab_vchan_alloc(struct uhab_context *ctx, struct physical_channel *pchan)
{
	int id;
	struct virtual_channel *vchan;

	if (!pchan || !ctx)
		return NULL;

	vchan = kzalloc(sizeof(*vchan), GFP_KERNEL);
	if (!vchan)
		return NULL;

	/* This should be the first thing we do in this function */
	idr_preload(GFP_KERNEL);
	spin_lock_bh(&pchan->vid_lock);
	id = idr_alloc(&pchan->vchan_idr, vchan, 1, 256, GFP_NOWAIT);
	spin_unlock_bh(&pchan->vid_lock);
	idr_preload_end();

	if (id < 0) {
		kfree(vchan);
		return NULL;
	}
	mb(); /* id must be generated done before pchan_get */

	hab_pchan_get(pchan);
	vchan->pchan = pchan;
	vchan->id = ((id << HAB_VCID_ID_SHIFT) & HAB_VCID_ID_MASK) |
		((pchan->habdev->id << HAB_VCID_MMID_SHIFT) &
			HAB_VCID_MMID_MASK) |
		((pchan->dom_id << HAB_VCID_DOMID_SHIFT) &
			HAB_VCID_DOMID_MASK);
	spin_lock_init(&vchan->rx_lock);
	INIT_LIST_HEAD(&vchan->rx_list);
	init_waitqueue_head(&vchan->rx_queue);

	kref_init(&vchan->refcount);
	kref_init(&vchan->usagecnt);
	vchan->otherend_closed = pchan->closed;

	hab_ctx_get(ctx);
	vchan->ctx = ctx;

	return vchan;
}

static void
hab_vchan_free(struct kref *ref)
{
	int found;
	struct virtual_channel *vchan =
		container_of(ref, struct virtual_channel, refcount);
	struct hab_message *message, *msg_tmp;
	struct export_desc *exp;
	struct physical_channel *pchan = vchan->pchan;
	struct uhab_context *ctx = vchan->ctx;

	list_for_each_entry_safe(message, msg_tmp, &vchan->rx_list, node) {
		list_del(&message->node);
		hab_msg_free(message);
	}

	do {
		found = 0;
		write_lock(&ctx->exp_lock);
		list_for_each_entry(exp, &ctx->exp_whse, node) {
			if (exp->vcid_local == vchan->id) {
				list_del(&exp->node);
				found = 1;
				break;
			}
		}
		write_unlock(&ctx->exp_lock);
		if (found) {
			habmem_hyp_revoke(exp->payload, exp->payload_count);
			habmem_remove_export(exp);
		}
	} while (found);

	do {
		found = 0;
		spin_lock_bh(&ctx->imp_lock);
		list_for_each_entry(exp, &ctx->imp_whse, node) {
			if (exp->vcid_remote == vchan->id) {
				list_del(&exp->node);
				found = 1;
				break;
			}
		}
		spin_unlock_bh(&ctx->imp_lock);
		if (found) {
			habmm_imp_hyp_unmap(ctx->import_ctx,
				exp->import_index,
				exp->payload_count,
				ctx->kernel);
			ctx->import_total--;
			kfree(exp);
		}
	} while (found);

	spin_lock_bh(&pchan->vid_lock);
	idr_remove(&pchan->vchan_idr, HAB_VCID_GET_ID(vchan->id));
	spin_unlock_bh(&pchan->vid_lock);

	hab_pchan_put(pchan);
	hab_ctx_put(ctx);

	kfree(vchan);
}

struct virtual_channel*
hab_vchan_get(struct physical_channel *pchan, uint32_t vchan_id)
{
	struct virtual_channel *vchan;

	spin_lock_bh(&pchan->vid_lock);
	vchan = idr_find(&pchan->vchan_idr, HAB_VCID_GET_ID(vchan_id));
	if (vchan)
		if (!kref_get_unless_zero(&vchan->refcount))
			vchan = NULL;
	spin_unlock_bh(&pchan->vid_lock);

	return vchan;
}

void hab_vchan_stop(struct virtual_channel *vchan)
{
	if (vchan) {
		vchan->otherend_closed = 1;
		wake_up_interruptible(&vchan->rx_queue);
	}
}

void hab_vchan_stop_notify(struct virtual_channel *vchan)
{
	hab_send_close_msg(vchan);
	hab_vchan_stop(vchan);
}


int hab_vchan_find_domid(struct virtual_channel *vchan)
{
	return vchan ? vchan->pchan->dom_id : -1;
}

static void
hab_vchan_free_deferred(struct work_struct *work)
{
	struct virtual_channel *vchan =
		container_of(work, struct virtual_channel, work);

	hab_vchan_free(&vchan->refcount);
}

static void
hab_vchan_schedule_free(struct kref *ref)
{
	struct virtual_channel *vchan =
		container_of(ref, struct virtual_channel, refcount);

	INIT_WORK(&vchan->work, hab_vchan_free_deferred);
	schedule_work(&vchan->work);
}

void hab_vchan_put(struct virtual_channel *vchan)
{
	if (vchan)
		kref_put(&vchan->refcount, hab_vchan_schedule_free);
}
