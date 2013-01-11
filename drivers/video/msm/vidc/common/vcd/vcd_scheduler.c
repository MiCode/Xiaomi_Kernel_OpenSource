/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#include <media/msm/vidc_type.h>
#include "vcd.h"

#define NORMALIZATION_FACTOR 3600
#define ADJUST_CLIENT_ROUNDS(client, round_adjustment) \
do {\
	if ((client)->rounds < round_adjustment) {\
		(client)->rounds = 0;\
		VCD_MSG_HIGH("%s(): WARNING: Scheduler list unsorted",\
			__func__);\
	} else\
		(client)->rounds -= round_adjustment;\
} while (0)

u32 vcd_sched_create(struct list_head *sched_list)
{
	u32 rc = VCD_S_SUCCESS;
	if (!sched_list) {
		VCD_MSG_ERROR("%s(): Invalid parameter", __func__);
		rc = VCD_ERR_ILLEGAL_PARM;
	} else
		INIT_LIST_HEAD(sched_list);
	return rc;
}

void vcd_sched_destroy(struct list_head *sched_clnt_list)
{
	struct vcd_sched_clnt_ctx *sched_clnt, *sched_clnt_next;
	if (sched_clnt_list)
		list_for_each_entry_safe(sched_clnt,
			sched_clnt_next, sched_clnt_list, list) {
			list_del_init(&sched_clnt->list);
			sched_clnt->clnt_active = false;
		}
}

void insert_client_in_list(struct list_head *sched_clnt_list,
	struct vcd_sched_clnt_ctx *sched_new_clnt, bool tail)
{
	struct vcd_sched_clnt_ctx *sched_clnt;
	if (!list_empty(sched_clnt_list)) {
		if (tail)
			sched_clnt = list_entry(sched_clnt_list->prev,
				struct vcd_sched_clnt_ctx, list);
		else
			sched_clnt = list_first_entry(sched_clnt_list,
				struct vcd_sched_clnt_ctx, list);
		sched_new_clnt->rounds = sched_clnt->rounds;
	} else
		sched_new_clnt->rounds = 0;
	if (tail)
		list_add_tail(&sched_new_clnt->list, sched_clnt_list);
	else
		list_add(&sched_new_clnt->list, sched_clnt_list);
}

u32 vcd_sched_add_client(struct vcd_clnt_ctxt *cctxt)
{
	struct vcd_property_hdr prop_hdr;
	struct vcd_sched_clnt_ctx *sched_cctxt;
	u32 rc = VCD_S_SUCCESS;
	if (!cctxt) {
		VCD_MSG_ERROR("%s(): Invalid parameter", __func__);
		rc = VCD_ERR_ILLEGAL_PARM;
	} else if (cctxt->sched_clnt_hdl)
		VCD_MSG_HIGH(
			"%s(): Scheduler client already exists!", __func__);
	else {
		sched_cctxt = (struct vcd_sched_clnt_ctx *)
			kmalloc(sizeof(struct vcd_sched_clnt_ctx),
					GFP_KERNEL);
		if (sched_cctxt) {

			prop_hdr.prop_id = DDL_I_FRAME_PROC_UNITS;
			prop_hdr.sz = sizeof(cctxt->frm_p_units);
			rc = ddl_get_property(cctxt->ddl_handle, &prop_hdr,
						  &cctxt->frm_p_units);
			VCD_FAILED_RETURN(rc,
				"Failed: Get DDL_I_FRAME_PROC_UNITS");
			if (cctxt->decoding) {
				cctxt->frm_rate.fps_numerator =
					VCD_DEC_INITIAL_FRAME_RATE;
				cctxt->frm_rate.fps_denominator = 1;
			} else {
				prop_hdr.prop_id = VCD_I_FRAME_RATE;
				prop_hdr.sz = sizeof(cctxt->frm_rate);
				rc = ddl_get_property(cctxt->ddl_handle,
						&prop_hdr, &cctxt->frm_rate);
				VCD_FAILED_RETURN(rc,
					"Failed: Get VCD_I_FRAME_RATE");
			}
			if (!cctxt->perf_set_by_client)
				cctxt->reqd_perf_lvl = cctxt->frm_p_units *
					cctxt->frm_rate.fps_numerator /
					cctxt->frm_rate.fps_denominator;

			cctxt->sched_clnt_hdl = sched_cctxt;
			memset(sched_cctxt, 0,
				sizeof(struct vcd_sched_clnt_ctx));
			sched_cctxt->tkns = 0;
			sched_cctxt->round_perfrm = NORMALIZATION_FACTOR *
				cctxt->frm_rate.fps_denominator /
				cctxt->frm_rate.fps_numerator;
			sched_cctxt->clnt_active = true;
			sched_cctxt->clnt_data = cctxt;
			INIT_LIST_HEAD(&sched_cctxt->ip_frm_list);

			insert_client_in_list(
				&cctxt->dev_ctxt->sched_clnt_list,
				sched_cctxt, false);
		}
	}
	return rc;
}

u32 vcd_sched_remove_client(struct vcd_sched_clnt_ctx *sched_cctxt)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_clnt_ctxt *cctxt;
	if (!sched_cctxt) {
		VCD_MSG_ERROR("%s(): Invalid handle ptr", __func__);
		rc = VCD_ERR_ILLEGAL_PARM;
	} else if (!list_empty(&sched_cctxt->ip_frm_list)) {
		VCD_MSG_ERROR(
			"%s(): Cannot remove client, queue no empty", __func__);
		rc = VCD_ERR_ILLEGAL_OP;
	} else {
		cctxt = sched_cctxt->clnt_data;
		list_del(&sched_cctxt->list);
		memset(sched_cctxt, 0,
			sizeof(struct vcd_sched_clnt_ctx));
		kfree(sched_cctxt);
	}
	return rc;
}

u32 vcd_sched_update_config(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc = VCD_S_SUCCESS;
	if (!cctxt || !cctxt->sched_clnt_hdl) {
		VCD_MSG_ERROR("%s(): Invalid parameter", __func__);
		rc = VCD_ERR_ILLEGAL_PARM;
	} else {
		cctxt->sched_clnt_hdl->rounds /=
			cctxt->sched_clnt_hdl->round_perfrm;
		cctxt->sched_clnt_hdl->round_perfrm =
			NORMALIZATION_FACTOR *
			cctxt->frm_rate.fps_denominator /
			cctxt->frm_rate.fps_numerator;
		cctxt->sched_clnt_hdl->rounds *=
			cctxt->sched_clnt_hdl->round_perfrm;
	}
	return rc;
}

u32 vcd_sched_queue_buffer(
	struct vcd_sched_clnt_ctx *sched_cctxt,
	struct vcd_buffer_entry *buffer, u32 tail)
{
	u32 rc = VCD_S_SUCCESS;
	if (!sched_cctxt || !buffer) {
		VCD_MSG_ERROR("%s(): Invalid parameter", __func__);
		rc = VCD_ERR_ILLEGAL_PARM;
	} else if (tail)
		list_add_tail(&buffer->sched_list,
				&sched_cctxt->ip_frm_list);
	else
		list_add(&buffer->sched_list, &sched_cctxt->ip_frm_list);
	return rc;
}

u32 vcd_sched_dequeue_buffer(
	struct vcd_sched_clnt_ctx *sched_cctxt,
	struct vcd_buffer_entry **buffer)
{
	u32 rc = VCD_ERR_QEMPTY;
	if (!sched_cctxt || !buffer) {
		VCD_MSG_ERROR("%s(): Invalid parameter", __func__);
		rc = VCD_ERR_ILLEGAL_PARM;
	} else {
		*buffer = NULL;
		if (!list_empty(&sched_cctxt->ip_frm_list)) {
			*buffer = list_first_entry(
					&sched_cctxt->ip_frm_list,
					struct vcd_buffer_entry,
					sched_list);
			list_del(&(*buffer)->sched_list);
			rc = VCD_S_SUCCESS;
		}
	}
	return rc;
}

u32 vcd_sched_mark_client_eof(struct vcd_sched_clnt_ctx *sched_cctxt)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_buffer_entry *buffer = NULL;
	if (!sched_cctxt) {
		VCD_MSG_ERROR("%s(): Invalid parameter", __func__);
		rc = VCD_ERR_ILLEGAL_PARM;
	} else if (!list_empty(&sched_cctxt->ip_frm_list)) {
		buffer = list_entry(sched_cctxt->ip_frm_list.prev,
			struct vcd_buffer_entry, sched_list);
		buffer->frame.flags |= VCD_FRAME_FLAG_EOS;
	} else
		rc = VCD_ERR_QEMPTY;
	return rc;
}

u32 vcd_sched_suspend_resume_clnt(
	struct vcd_clnt_ctxt *cctxt, u32 state)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_sched_clnt_ctx *sched_cctxt;
	if (!cctxt || !cctxt->sched_clnt_hdl) {
		VCD_MSG_ERROR("%s(): Invalid parameter", __func__);
		rc = VCD_ERR_ILLEGAL_PARM;
	} else {
		sched_cctxt = cctxt->sched_clnt_hdl;
		if (state != sched_cctxt->clnt_active) {
			sched_cctxt->clnt_active = state;
			if (state)
				insert_client_in_list(&cctxt->dev_ctxt->\
					sched_clnt_list, sched_cctxt, false);
			else
				list_del_init(&sched_cctxt->list);
		}
	}
	return rc;
}

u32 vcd_sched_get_client_frame(struct list_head *sched_clnt_list,
	struct vcd_clnt_ctxt **cctxt,
	struct vcd_buffer_entry **buffer)
{
	u32 rc = VCD_ERR_QEMPTY, round_adjustment = 0;
	struct vcd_sched_clnt_ctx *sched_clnt, *clnt_nxt;
	if (!sched_clnt_list || !cctxt || !buffer) {
		VCD_MSG_ERROR("%s(): Invalid parameter", __func__);
		rc = VCD_ERR_ILLEGAL_PARM;
	} else if (!list_empty(sched_clnt_list)) {
		*cctxt = NULL;
		*buffer = NULL;
		list_for_each_entry_safe(sched_clnt,
			clnt_nxt, sched_clnt_list, list) {
			if (&sched_clnt->list == sched_clnt_list->next)
				round_adjustment = sched_clnt->rounds;
			if (*cctxt) {
				if ((*cctxt)->sched_clnt_hdl->rounds >=
					sched_clnt->rounds)
					list_move(&(*cctxt)->sched_clnt_hdl\
						->list, &sched_clnt->list);
				ADJUST_CLIENT_ROUNDS(sched_clnt,
					round_adjustment);
			} else if (sched_clnt->tkns &&
				!list_empty(&sched_clnt->ip_frm_list)) {
				*cctxt = sched_clnt->clnt_data;
				sched_clnt->rounds += sched_clnt->round_perfrm;
			} else
				ADJUST_CLIENT_ROUNDS(sched_clnt,
						round_adjustment);
		}
		if (*cctxt) {
			rc = vcd_sched_dequeue_buffer(
				(*cctxt)->sched_clnt_hdl, buffer);
			if (rc == VCD_S_SUCCESS) {
				(*cctxt)->sched_clnt_hdl->tkns--;
				ADJUST_CLIENT_ROUNDS((*cctxt)->\
					sched_clnt_hdl, round_adjustment);
			}
		}
	}
	return rc;
}
