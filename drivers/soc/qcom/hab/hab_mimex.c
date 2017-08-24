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
#include "hab_grantable.h"

/*
 * use physical channel to send export parcel

 * local                      remote
 * send(export)        -->    IRQ store to export warehouse
 * wait(export ack)   <--     send(export ack)

 * the actual data consists the following 3 parts listed in order
 * 1. header (uint32_t) vcid|type|size
 * 2. export parcel (full struct)
 * 3. full contents in export->pdata
 */


static int hab_export_ack_find(struct uhab_context *ctx,
	struct hab_export_ack *expect_ack)
{
	int ret = 0;
	struct hab_export_ack_recvd *ack_recvd;

	spin_lock_bh(&ctx->expq_lock);

	list_for_each_entry(ack_recvd, &ctx->exp_rxq, node) {
		if (ack_recvd->ack.export_id == expect_ack->export_id &&
			ack_recvd->ack.vcid_local == expect_ack->vcid_local &&
			ack_recvd->ack.vcid_remote == expect_ack->vcid_remote) {
			list_del(&ack_recvd->node);
			kfree(ack_recvd);
			ret = 1;
			break;
		}
		ack_recvd->age++;
		if (ack_recvd->age > Q_AGE_THRESHOLD) {
			list_del(&ack_recvd->node);
			kfree(ack_recvd);
		}
	}

	spin_unlock_bh(&ctx->expq_lock);

	return ret;
}

static int hab_export_ack_wait(struct uhab_context *ctx,
		struct hab_export_ack *expect_ack)
{
	int ret;

	ret = wait_event_interruptible_timeout(ctx->exp_wq,
		hab_export_ack_find(ctx, expect_ack),
		HZ);
	if (!ret || (ret == -ERESTARTSYS))
		ret = -EAGAIN;
	else if (ret > 0)
		ret = 0;
	return ret;
}

/*
 * Get id from free list first. if not available, new id is generated.
 * Once generated it will not be erased
 * assumptions: no handshake or memory map/unmap in this helper function
 */
static struct export_desc *habmem_add_export(struct virtual_channel *vchan,
		int sizebytes,
		uint32_t flags)
{
	struct uhab_context *ctx;
	struct export_desc *exp;

	if (!vchan || !sizebytes)
		return NULL;

	exp = vmalloc(sizebytes);
	if (!exp)
		return NULL;

	idr_preload(GFP_KERNEL);
	spin_lock(&vchan->pchan->expid_lock);
	exp->export_id =
		idr_alloc(&vchan->pchan->expid_idr, exp, 1, 0, GFP_NOWAIT);
	spin_unlock(&vchan->pchan->expid_lock);
	idr_preload_end();

	exp->readonly = flags;
	exp->vchan = vchan;
	exp->vcid_local = vchan->id;
	exp->vcid_remote = vchan->otherend_id;
	exp->domid_local = -1; /* dom id, provided on the importer */
	exp->domid_remote = vchan->pchan->dom_id;

	ctx = vchan->ctx;
	write_lock(&ctx->exp_lock);
	ctx->export_total++;
	list_add_tail(&exp->node, &ctx->exp_whse);
	write_unlock(&ctx->exp_lock);

	return exp;
}

void habmem_remove_export(struct export_desc *exp)
{
	struct physical_channel *pchan;
	struct uhab_context *ctx;

	if (!exp || !exp->vchan || !exp->vchan->ctx || !exp->vchan->pchan)
		return;

	ctx = exp->vchan->ctx;
	ctx->export_total--;

	pchan = exp->vchan->pchan;

	spin_lock(&pchan->expid_lock);
	idr_remove(&pchan->expid_idr, exp->export_id);
	spin_unlock(&pchan->expid_lock);

	vfree(exp);
}

static int compress_pfns(void **pfns, int npages, unsigned int *data_size)
{
	int i, j = 0;
	struct grantable *item = (struct grantable *)*pfns;
	int region_size = 1;
	struct compressed_pfns *new_table =
		vmalloc(sizeof(struct compressed_pfns) +
			npages * sizeof(struct region));

	if (!new_table)
		return -ENOMEM;

	new_table->first_pfn = item[0].pfn;
	for (i = 1; i < npages; i++) {
		if (item[i].pfn-1 == item[i-1].pfn) {
			region_size++;
		} else {
			new_table->region[j].size  = region_size;
			new_table->region[j].space = item[i].pfn -
							item[i-1].pfn - 1;
			j++;
			region_size = 1;
		}
	}
	new_table->region[j].size = region_size;
	new_table->region[j].space = 0;
	new_table->nregions = j+1;
	vfree(*pfns);

	*data_size = sizeof(struct compressed_pfns) +
		sizeof(struct region)*new_table->nregions;
	*pfns = new_table;
	return 0;
}

/*
 * store the parcel to the warehouse, then send the parcel to remote side
 * both exporter composed export descriptor and the grantrefids are sent
 * as one msg to the importer side
 */
static int habmem_export_vchan(struct uhab_context *ctx,
		struct virtual_channel *vchan,
		void *pdata,
		int payload_size,
		int nunits,
		uint32_t flags,
		uint32_t *export_id) {
	int ret;
	struct export_desc *exp;
	uint32_t sizebytes = sizeof(*exp) + payload_size;
	struct hab_export_ack expected_ack = {0};
	struct hab_header header = HAB_HEADER_INITIALIZER;

	exp = habmem_add_export(vchan, sizebytes, flags);
	if (!exp)
		return -ENOMEM;

	 /* append the pdata to the export descriptor */
	exp->payload_count = nunits;
	memcpy(exp->payload, pdata, payload_size);

	HAB_HEADER_SET_SIZE(header, sizebytes);
	HAB_HEADER_SET_TYPE(header, HAB_PAYLOAD_TYPE_EXPORT);
	HAB_HEADER_SET_ID(header, vchan->otherend_id);
	ret = physical_channel_send(vchan->pchan, &header, exp);

	if (ret != 0) {
		pr_err("failed to export payload to the remote %d\n", ret);
		return ret;
	}

	expected_ack.export_id = exp->export_id;
	expected_ack.vcid_local = exp->vcid_local;
	expected_ack.vcid_remote = exp->vcid_remote;
	ret = hab_export_ack_wait(ctx, &expected_ack);

	*export_id = exp->export_id;

	return ret;
}

int hab_mem_export(struct uhab_context *ctx,
		struct hab_export *param,
		int kernel)
{
	int ret = 0;
	void *pdata_exp = NULL;
	unsigned int pdata_size = 0;
	uint32_t export_id = 0;
	struct virtual_channel *vchan;
	int page_count;

	if (!ctx || !param || param->sizebytes > HAB_MAX_EXPORT_SIZE)
		return -EINVAL;

	vchan = hab_get_vchan_fromvcid(param->vcid, ctx);
	if (!vchan || !vchan->pchan) {
		ret = -ENODEV;
		goto err;
	}

	page_count = param->sizebytes/PAGE_SIZE;
	pdata_exp = habmm_hyp_allocate_grantable(page_count, &pdata_size);
	if (!pdata_exp) {
		ret = -ENOMEM;
		goto err;
	}

	if (kernel) {
		ret = habmem_hyp_grant((unsigned long)param->buffer,
			page_count,
			param->flags,
			vchan->pchan->dom_id,
			pdata_exp);
	} else {
		ret = habmem_hyp_grant_user((unsigned long)param->buffer,
			page_count,
			param->flags,
			vchan->pchan->dom_id,
			pdata_exp);
	}
	if (ret < 0) {
		pr_err("habmem_hyp_grant failed size=%d ret=%d\n",
			pdata_size, ret);
		goto err;
	}

	compress_pfns(&pdata_exp, page_count, &pdata_size);

	ret = habmem_export_vchan(ctx,
		vchan,
		pdata_exp,
		pdata_size,
		page_count,
		param->flags,
		&export_id);

	param->exportid = export_id;
err:
	vfree(pdata_exp);
	if (vchan)
		hab_vchan_put(vchan);
	return ret;
}

int hab_mem_unexport(struct uhab_context *ctx,
		struct hab_unexport *param,
		int kernel)
{
	int ret = 0, found = 0;
	struct export_desc *exp, *tmp;

	if (!ctx || !param)
		return -EINVAL;

	write_lock(&ctx->exp_lock);
	list_for_each_entry_safe(exp, tmp, &ctx->exp_whse, node) {
		if ((param->exportid == exp->export_id) &&
			(param->vcid == exp->vcid_local)) {
			list_del(&exp->node);
			found = 1;
			break;
		}
	}
	write_unlock(&ctx->exp_lock);

	if (!found)
		return -EINVAL;

	ret = habmem_hyp_revoke(exp->payload, exp->payload_count);

	habmem_remove_export(exp);
	return ret;
}

int hab_mem_import(struct uhab_context *ctx,
		struct hab_import *param,
		int kernel)
{
	int ret = 0, found = 0;
	struct export_desc *exp = NULL;

	if (!ctx || !param)
		return -EINVAL;

	spin_lock_bh(&ctx->imp_lock);
	list_for_each_entry(exp, &ctx->imp_whse, node) {
		if ((exp->export_id == param->exportid) &&
			(param->vcid == exp->vcid_remote)) {
			found = 1;
			break;
		}
	}
	spin_unlock_bh(&ctx->imp_lock);

	if (!found) {
		pr_err("Fail to get export descriptor from export id %d\n",
			param->exportid);
		ret = -ENODEV;
		return ret;
	}

	ret = habmem_imp_hyp_map(ctx->import_ctx,
		exp->payload,
		exp->payload_count,
		exp->domid_local,
		&exp->import_index,
		&exp->kva,
		kernel,
		param->flags);
	if (ret) {
		pr_err("Import fail ret:%d pcnt:%d rem:%d 1st_ref:0x%X\n",
			ret, exp->payload_count,
			exp->domid_local, *((uint32_t *)exp->payload));
		return ret;
	}

	param->index = exp->import_index;
	param->kva = (uint64_t)exp->kva;

	return ret;
}

int hab_mem_unimport(struct uhab_context *ctx,
		struct hab_unimport *param,
		int kernel)
{
	int ret = 0, found = 0;
	struct export_desc *exp = NULL, *exp_tmp;

	if (!ctx || !param)
		return -EINVAL;

	spin_lock_bh(&ctx->imp_lock);
	list_for_each_entry_safe(exp, exp_tmp, &ctx->imp_whse, node) {
		if ((exp->export_id == param->exportid) &&
			(param->vcid == exp->vcid_remote)) {
			list_del(&exp->node);
			ctx->import_total--;
			found = 1;
			break;
		}
	}
	spin_unlock_bh(&ctx->imp_lock);

	if (!found)
		ret = -EINVAL;
	else {
		ret = habmm_imp_hyp_unmap(ctx->import_ctx,
			exp->import_index,
			exp->payload_count,
			kernel);

		param->kva = (uint64_t)exp->kva;
		kfree(exp);
	}

	return ret;
}
