// SPDX-License-Identifier: GPL-2.0
/*
 * Universal Flash Storage Host Performance Booster
 *
 * Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Yongmyung Lee <ymhungry.lee@samsung.com>
 *	Jinyoung Choi <j-young.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 *
 * The Linux Foundation chooses to take subject only to the GPLv2
 * license terms, and distributes only under these terms.
 */

#include "ufshcd.h"
#include "ufsfeature.h"
#include "ufsshpb.h"

#define UFSHCD_REQ_SENSE_SIZE	18

static int ufsshpb_create_sysfs(struct ufsf_feature *ufsf,
			       struct ufsshpb_lu *hpb);
static void ufsshpb_remove_sysfs(struct ufsshpb_lu *hpb);


static inline bool ufsshpb_is_triggered_by_fs(struct ufsshpb_region *rgn)
{
	return atomic_read(&rgn->reason) == HPB_UPDATE_FROM_FS;
}

static inline void
ufsshpb_get_pos_from_lpn(struct ufsshpb_lu *hpb, unsigned long lpn, int *rgn_idx,
			int *srgn_idx, int *offset)
{
	int rgn_offset;

	*rgn_idx = lpn >> hpb->entries_per_rgn_shift;
	rgn_offset = lpn & hpb->entries_per_rgn_mask;
	*srgn_idx = rgn_offset >> hpb->entries_per_srgn_shift;
	*offset = rgn_offset & hpb->entries_per_srgn_mask;
}

inline int ufsshpb_valid_srgn(struct ufsshpb_region *rgn,
			     struct ufsshpb_subregion *srgn)
{
	return rgn->rgn_state != HPB_RGN_INACTIVE &&
		srgn->srgn_state == HPB_SRGN_CLEAN;
}

/* Must be held hpb_lock  */
static bool ufsshpb_ppn_dirty_check(struct ufsshpb_lu *hpb, unsigned long lpn,
				   int transfer_len)
{
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	unsigned long cur_lpn = lpn;
	int rgn_idx, srgn_idx, srgn_offset, find_size;
	int scan_cnt = transfer_len;

	do {
		ufsshpb_get_pos_from_lpn(hpb, cur_lpn, &rgn_idx, &srgn_idx,
					&srgn_offset);
		rgn = hpb->rgn_tbl + rgn_idx;
		srgn = rgn->srgn_tbl + srgn_idx;

		if (!ufsshpb_valid_srgn(rgn, srgn))
			return true;

		if (unlikely(!srgn->mctx || !srgn->mctx->ppn_dirty))
			return true;

		if (hpb->entries_per_srgn < srgn_offset + scan_cnt) {
			int cnt = hpb->entries_per_srgn - srgn_offset;

			find_size = hpb->entries_per_srgn;
			scan_cnt -= cnt;
			cur_lpn += cnt;
		} else {
			find_size = srgn_offset + scan_cnt;
			scan_cnt = 0;
		}

		srgn_offset =
			find_next_bit((unsigned long *)srgn->mctx->ppn_dirty,
				      hpb->entries_per_srgn, srgn_offset);

		if (srgn_offset < hpb->entries_per_srgn)
			return srgn_offset < find_size;

	} while (scan_cnt);

	return false;
}

static void ufsshpb_set_read16_cmd(struct ufsshpb_lu *hpb, struct scsi_cmnd *cmd,
				  u64 ppn, unsigned int transfer_len,
				  int hpb_ctx_id)
{
	unsigned char *cdb = cmd->cmnd;

	cdb[0] = READ_16;
	cdb[2] = cmd->cmnd[2];
	cdb[3] = cmd->cmnd[3];
	cdb[4] = cmd->cmnd[4];
	cdb[5] = cmd->cmnd[5];
	cdb[6] = GET_BYTE_7(ppn);
	cdb[7] = GET_BYTE_6(ppn);
	cdb[8] = GET_BYTE_5(ppn);
	cdb[9] = GET_BYTE_4(ppn);
	cdb[10] = GET_BYTE_3(ppn);
	cdb[11] = GET_BYTE_2(ppn);
	cdb[12] = GET_BYTE_1(ppn);
	cdb[13] = GET_BYTE_0(ppn);

	if (hpb_ctx_id < MAX_HPB_CONTEXT_ID)
		cdb[14] = (1 << 7) | hpb_ctx_id;
	else
		cdb[14] = UFSSHPB_GROUP_NUMBER;

	cdb[15] = transfer_len;

	cmd->cmd_len = UFS_CDB_SIZE;
}

/* called with hpb_lock (irq) */
static inline void
ufsshpb_set_dirty_bits(struct ufsshpb_lu *hpb, struct ufsshpb_region *rgn,
		      struct ufsshpb_subregion *srgn, int dword, int offset,
		      unsigned int cnt)
{
	const unsigned long mask = ((1UL << cnt) - 1) & 0xffffffff;

	if (rgn->rgn_state == HPB_RGN_INACTIVE)
		return;

	BUG_ON(!srgn->mctx);
	srgn->mctx->ppn_dirty[dword] |= (mask << offset);
}

static inline void ufsshpb_get_bit_offset(struct ufsshpb_lu *hpb, int srgn_offset,
					 int *dword, int *offset)
{
	*dword = srgn_offset >> bits_per_dword_shift;
	*offset = srgn_offset & bits_per_dword_mask;
}

static void ufsshpb_set_dirty(struct ufsshpb_lu *hpb, struct ufshcd_lrb *lrbp,
			     int rgn_idx, int srgn_idx, int srgn_offset)
{
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	int cnt, bit_cnt, bit_dword, bit_offset;

	cnt = blk_rq_sectors(lrbp->cmd->request) >> sects_per_blk_shift;
	ufsshpb_get_bit_offset(hpb, srgn_offset, &bit_dword, &bit_offset);

	do {
		bit_cnt = min(cnt, BITS_PER_DWORD - bit_offset);

		rgn = hpb->rgn_tbl + rgn_idx;
		srgn = rgn->srgn_tbl + srgn_idx;

		ufsshpb_set_dirty_bits(hpb, rgn, srgn, bit_dword, bit_offset,
				      bit_cnt);

		bit_offset = 0;
		bit_dword++;

		if (bit_dword == hpb->dwords_per_srgn) {
			bit_dword = 0;
			srgn_idx++;

			if (srgn_idx == hpb->srgns_per_rgn) {
				srgn_idx = 0;
				rgn_idx++;
			}
		}
		cnt -= bit_cnt;
	} while (cnt);

	BUG_ON(cnt < 0);
}

static inline bool ufsshpb_is_read_cmd(struct scsi_cmnd *cmd)
{
	return (cmd->cmnd[0] == READ_10 || cmd->cmnd[0] == READ_16);
}

static inline bool ufsshpb_is_write_cmd(struct scsi_cmnd *cmd)
{
	return (cmd->cmnd[0] == WRITE_10 || cmd->cmnd[0] == WRITE_16);
}

static inline bool ufsshpb_is_discard_cmd(struct scsi_cmnd *cmd)
{
	return cmd->cmnd[0] == UNMAP;
}

static u64 ufsshpb_get_ppn(struct ufsshpb_map_ctx *mctx, int pos, int *error)
{
	u64 *ppn_table;
	struct page *page = NULL;
	int index, offset;

	index = pos / HPB_ENTREIS_PER_OS_PAGE;
	offset = pos % HPB_ENTREIS_PER_OS_PAGE;

	page = mctx->m_page[index];
	if (unlikely(!page)) {
		*error = -ENOMEM;
		ERR_MSG("mctx %p cannot get m_page", mctx);
		return 0;
	}

	ppn_table = page_address(page);
	if (unlikely(!ppn_table)) {
		*error = -ENOMEM;
		ERR_MSG("mctx %p cannot get ppn_table vm", mctx);
		return 0;
	}

	return ppn_table[offset];
}

#if defined(CONFIG_HPB_ERR_INJECTION)
static u64 ufsshpb_get_bitflip_ppn(struct ufsshpb_lu *hpb, u64 ppn)
{
	return (ppn ^ hpb->err_injection_bitflip);
}

static u64 ufsshpb_get_offset_ppn(struct ufsshpb_lu *hpb, u64 ppn)
{
	return (ppn + hpb->err_injection_offset);
}

static u64 ufsshpb_get_random_ppn(struct ufsshpb_lu *hpb, u64 ppn)
{
	u64 random_ppn = 0;

	if (!hpb->err_injection_random)
		return ppn;

	get_random_bytes(&random_ppn, sizeof(random_ppn));

	return random_ppn;
}

static u64 ufsshpb_get_err_injection_ppn(struct ufsshpb_lu *hpb, u64 ppn)
{
	u64 new_ppn = 0;

	if (hpb->err_injection_select == HPB_ERR_INJECTION_BITFLIP)
		new_ppn = ufsshpb_get_bitflip_ppn(hpb, ppn);
	else if (hpb->err_injection_select == HPB_ERR_INJECTION_OFFSET)
		new_ppn = ufsshpb_get_offset_ppn(hpb, ppn);
	else if (hpb->err_injection_select == HPB_ERR_INJECTION_RANDOM)
		new_ppn = ufsshpb_get_random_ppn(hpb, ppn);

	return new_ppn;
}
#endif

inline int ufsshpb_get_state(struct ufsf_feature *ufsf)
{
	return atomic_read(&ufsf->hpb_state);
}

inline void ufsshpb_set_state(struct ufsf_feature *ufsf, int state)
{
	atomic_set(&ufsf->hpb_state, state);
}

static inline int ufsshpb_lu_get(struct ufsshpb_lu *hpb)
{
	if (!hpb || ufsshpb_get_state(hpb->ufsf) != HPB_PRESENT)
		return -ENODEV;

	kref_get(&hpb->ufsf->hpb_kref);
	return 0;
}

static inline void ufsshpb_schedule_error_handler(struct kref *kref)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(kref, struct ufsf_feature, hpb_kref);
	schedule_work(&ufsf->hpb_eh_work);
}

static inline void ufsshpb_lu_put(struct ufsshpb_lu *hpb)
{
	kref_put(&hpb->ufsf->hpb_kref, ufsshpb_schedule_error_handler);
}

static void ufsshpb_failed(struct ufsshpb_lu *hpb, const char *f)
{
	ERR_MSG("ufsshpb_driver failed. function (%s)", f);
	ufsshpb_set_state(hpb->ufsf, HPB_FAILED);
	ufsshpb_lu_put(hpb);
}

/*
 * end_io() is called with held the queue lock.
 * so, blk_put_request is not available directly.
 */
static void ufsshpb_put_blk_request(struct request *req)
{
	struct request_queue *q = req->q;

	if (q->mq_ops) {
		blk_mq_free_request(req);
	} else {
		lockdep_assert_held(&q->queue_lock);
		blk_put_request(req); // modify
	}
}

static inline void ufsshpb_put_pre_req(struct ufsshpb_lu *hpb,
				      struct ufsshpb_req *pre_req)
{
	pre_req->req = NULL;
	pre_req->bio = NULL;
	list_add_tail(&pre_req->list_req, &hpb->lh_pre_req_free);
	hpb->num_inflight_pre_req--;
}

static struct ufsshpb_req *ufsshpb_get_pre_req(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_req *pre_req;

	if (hpb->num_inflight_pre_req >= hpb->throttle_pre_req) {
#if defined(CONFIG_HPB_DEBUG)
		HPB_DEBUG(hpb, "pre_req throttle. inflight %d throttle %d",
			  hpb->num_inflight_pre_req, hpb->throttle_pre_req);
#endif
		return NULL;
	}

	pre_req = list_first_entry_or_null(&hpb->lh_pre_req_free,
					   struct ufsshpb_req, list_req);
	if (!pre_req) {
#if defined(CONFIG_HPB_DEBUG)
		HPB_DEBUG(hpb, "There is no pre_req");
#endif
		return NULL;
	}

	list_del_init(&pre_req->list_req);
	hpb->num_inflight_pre_req++;

	return pre_req;
}

static void ufsshpb_pre_req_compl_fn(struct request *req, blk_status_t error)
{
	struct ufsshpb_req *pre_req = (struct ufsshpb_req *)req->end_io_data;
	struct ufsshpb_lu *hpb = pre_req->hpb;
	unsigned long flags;
	struct scsi_sense_hdr sshdr;

	if (error) {
		ERR_MSG("block status %d", error);
		scsi_normalize_sense(pre_req->sense, SCSI_SENSE_BUFFERSIZE,
				     &sshdr);
		ERR_MSG("code %x sense_key %x asc %x ascq %x",
			sshdr.response_code,
			sshdr.sense_key, sshdr.asc, sshdr.ascq);
		ERR_MSG("byte4 %x byte5 %x byte6 %x additional_len %x",
			sshdr.byte4, sshdr.byte5,
			sshdr.byte6, sshdr.additional_length);
	}

	bio_put(pre_req->bio);
	ufsshpb_put_blk_request(pre_req->req);
	spin_lock_irqsave(&hpb->hpb_lock, flags);
	ufsshpb_put_pre_req(pre_req->hpb, pre_req);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);

	ufsshpb_lu_put(pre_req->hpb);
}

static int ufsshpb_prep_entry(struct ufsshpb_req *pre_req, struct page *page)
{
	struct ufsshpb_lu *hpb = pre_req->hpb;
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	u64 *addr;
	u64 entry_ppn = 0;
	unsigned long lpn = pre_req->wb.lpn;
	int rgn_idx, srgn_idx, srgn_offset;
	int i, error = 0;
	unsigned long flags;

	addr = page_address(page);

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	for (i = 0; i < pre_req->wb.len; i++, lpn++) {
		ufsshpb_get_pos_from_lpn(hpb, lpn, &rgn_idx, &srgn_idx,
					&srgn_offset);

		rgn = hpb->rgn_tbl + rgn_idx;
		srgn = rgn->srgn_tbl + srgn_idx;

		if (!ufsshpb_valid_srgn(rgn, srgn))
			goto mctx_error;

		BUG_ON(!srgn->mctx);

		entry_ppn = ufsshpb_get_ppn(srgn->mctx, srgn_offset, &error);
		if (error)
			goto mctx_error;

#if defined(CONFIG_HPB_ERR_INJECTION)
		if (hpb->err_injection_select != HPB_ERR_INJECTION_DISABLE)
			entry_ppn =
				ufsshpb_get_err_injection_ppn(hpb, entry_ppn);
#endif

		addr[i] = entry_ppn;
	}
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	return 0;
mctx_error:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	return -ENOMEM;
}

static int ufsshpb_pre_req_add_bio_page(struct request_queue *q,
				       struct ufsshpb_req *pre_req)
{
	struct page *page = pre_req->wb.m_page;
	struct bio *bio = pre_req->bio;
	int entries_bytes, ret;

	BUG_ON(!page);

	ret = ufsshpb_prep_entry(pre_req, page);
	if (ret)
		return ret;

	entries_bytes = pre_req->wb.len * sizeof(u64);

	ret = bio_add_pc_page(q, bio, page, entries_bytes, 0);
	if (ret != entries_bytes) {
		ERR_MSG("bio_add_pc_page fail: %d", ret);
		return -ENOMEM;
	}

	return 0;
}

static inline unsigned long ufsshpb_get_lpn(struct request *rq)
{
	return blk_rq_pos(rq) / SECTORS_PER_BLOCK;
}

static inline unsigned int ufsshpb_get_len(struct request *rq)
{
	return blk_rq_sectors(rq) / SECTORS_PER_BLOCK;
}

static inline unsigned int ufsshpb_is_unaligned(struct request *rq)
{
	return blk_rq_sectors(rq) % SECTORS_PER_BLOCK;
}

static inline int ufsshpb_issue_ctx_id_ticket(struct ufsshpb_lu *hpb)
{
	int hpb_ctx_id;

	hpb->ctx_id_ticket++;
	if (hpb->ctx_id_ticket >= MAX_HPB_CONTEXT_ID)
		hpb->ctx_id_ticket = 0;
	hpb_ctx_id = hpb->ctx_id_ticket;

	return hpb_ctx_id;
}

static inline void ufsshpb_set_prefetch_cmd(unsigned char *cdb,
					   unsigned long lpn, unsigned int len,
					   int hpb_ctx_id)
{
	int len_byte = len * HPB_ENTRY_SIZE;

	cdb[0] = UFSSHPB_WRITE_BUFFER;
	cdb[1] = UFSSHPB_WB_ID_PREFETCH;
	cdb[2] = GET_BYTE_3(lpn);
	cdb[3] = GET_BYTE_2(lpn);
	cdb[4] = GET_BYTE_1(lpn);
	cdb[5] = GET_BYTE_0(lpn);
	cdb[6] = (1 << 7) | hpb_ctx_id;
	cdb[7] = GET_BYTE_1(len_byte);
	cdb[8] = GET_BYTE_0(len_byte);
	cdb[9] = 0x00;	/* Control = 0x00 */
}

static int ufsshpb_execute_pre_req(struct ufsshpb_lu *hpb, struct scsi_cmnd *cmd,
				  struct ufsshpb_req *pre_req, int hpb_ctx_id)
{
	struct scsi_device *sdev = cmd->device;
	struct request_queue *q = sdev->request_queue;
	struct request *req;
	struct blk_mq_hw_ctx *hctx;
	struct scsi_request *rq;
	struct bio *bio = pre_req->bio;
	int ret = 0;

	pre_req->hpb = hpb;
	pre_req->wb.lpn = ufsshpb_get_lpn(cmd->request);
	pre_req->wb.len = ufsshpb_get_len(cmd->request);
	ret = ufsshpb_pre_req_add_bio_page(q, pre_req);
	if (ret)
		return ret;

	req = pre_req->req;
	hctx = blk_mq_map_queue(req->q, REQ_OP_WRITE, req->mq_ctx);

	/* 1. request setup */
	blk_rq_append_bio(req, &bio);
	req->rq_flags |= RQF_QUIET;
	req->timeout = 30 * HZ;
	req->end_io_data = (void *)pre_req;

	/* 2. scsi_request setup */
	rq = scsi_req(req);
	rq->retries = 1;
	ufsshpb_set_prefetch_cmd(rq->cmd, pre_req->wb.lpn, pre_req->wb.len,
				hpb_ctx_id);
	rq->cmd_len = scsi_command_size(rq->cmd);

	ret = ufsf_blk_execute_rq_nowait_mimic(req, hctx, ufsshpb_pre_req_compl_fn);

	if (!ret)
		atomic64_inc(&hpb->pre_req_cnt);

	return ret;
}

static int ufsshpb_issue_pre_req(struct ufsshpb_lu *hpb, struct scsi_cmnd *cmd,
				int *hpb_ctx_id)
{
	struct ufsshpb_req *pre_req;
	struct request *req = NULL;
	struct bio *bio = NULL;
	unsigned long flags;
	int ctx_id;
	int ret = 0;

	req = blk_get_request(cmd->device->request_queue,
			      REQ_OP_SCSI_OUT | REQ_SYNC,
			      BLK_MQ_REQ_PM | BLK_MQ_REQ_NOWAIT);
	if (IS_ERR(req))
		return -EAGAIN;

	bio = bio_alloc(GFP_ATOMIC, 1);
	if (!bio) {
		blk_put_request(req);
		return -EAGAIN;
	}

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	pre_req = ufsshpb_get_pre_req(hpb);
	if (!pre_req) {
		ret = -EAGAIN;
		goto unlock_out;
	}
	ctx_id = ufsshpb_issue_ctx_id_ticket(hpb);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);

	pre_req->req = req;
	pre_req->bio = bio;

#if defined(CONFIG_HPB_DEBUG)
	{
	unsigned int start_lpn, end_lpn;
	int s_rgn_idx, s_srgn_idx, s_offset;
	int e_rgn_idx, e_srgn_idx, e_offset;
		start_lpn = ufsshpb_get_lpn(cmd->request);
		end_lpn = start_lpn + ufsshpb_get_len(cmd->request) - 1;

		ufsshpb_get_pos_from_lpn(hpb, start_lpn, &s_rgn_idx, &s_srgn_idx,
					&s_offset);
		ufsshpb_get_pos_from_lpn(hpb, end_lpn, &e_rgn_idx, &e_srgn_idx,
					&e_offset);
		trace_printk("[wb.set]\t rgn %04d (%d) - rgn %04d (%d)\n",
			     s_rgn_idx, hpb->rgn_tbl[s_rgn_idx].rgn_state,
			     e_rgn_idx, hpb->rgn_tbl[e_rgn_idx].rgn_state);
	}
#endif

	ret = ufsshpb_lu_get(hpb);
	if (unlikely(ret)) {
		ERR_MSG("ufsshpb_lu_get failed. (%d)", ret);
		goto free_pre_req;
	}

	ret = ufsshpb_execute_pre_req(hpb, cmd, pre_req, ctx_id);
	if (ret) {
		ufsshpb_lu_put(hpb);
		goto free_pre_req;
	}

	*hpb_ctx_id = ctx_id;

	return ret;
free_pre_req:
	spin_lock_irqsave(&hpb->hpb_lock, flags);
	ufsshpb_put_pre_req(hpb, pre_req);
unlock_out:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	bio_put(bio);
	blk_put_request(req);
	return ret;
}

static inline bool ufsshpb_is_support_chunk(int transfer_len)
{
	return transfer_len <= HPB_MULTI_CHUNK_HIGH;
}

static inline void ufsshpb_put_map_req(struct ufsshpb_lu *hpb,
				      struct ufsshpb_req *map_req)
{
	map_req->req = NULL;
	map_req->bio = NULL;
	list_add_tail(&map_req->list_req, &hpb->lh_map_req_free);
	hpb->num_inflight_map_req--;
	hpb->num_inflight_hcm_req--;
}

static struct ufsshpb_req *ufsshpb_get_map_req(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_req *map_req;

	if (hpb->num_inflight_map_req >= hpb->throttle_map_req ||
	    hpb->num_inflight_hcm_req >= hpb->throttle_hcm_req) {
#if defined(CONFIG_HPB_DEBUG)
		HPB_DEBUG(hpb, "map_req throttle. inflight %d throttle %d",
			  hpb->num_inflight_map_req, hpb->throttle_map_req);
		HPB_DEBUG(hpb, "hcm_req throttle. hcm inflight %d hcm throttle %d",
			  hpb->num_inflight_hcm_req, hpb->throttle_hcm_req);
#endif
		return NULL;
	}

	map_req = list_first_entry_or_null(&hpb->lh_map_req_free,
					   struct ufsshpb_req, list_req);
	if (!map_req) {
#if defined(CONFIG_HPB_DEBUG)
		HPB_DEBUG(hpb, "There is no map_req");
#endif
		return NULL;
	}

	list_del_init(&map_req->list_req);
	hpb->num_inflight_map_req++;
	hpb->num_inflight_hcm_req++;

	return map_req;
}

static int ufsshpb_clean_dirty_bitmap(struct ufsshpb_lu *hpb,
				     struct ufsshpb_subregion *srgn)
{
	struct ufsshpb_region *rgn;

	BUG_ON(!srgn->mctx);

	rgn = hpb->rgn_tbl + srgn->rgn_idx;

	if (rgn->rgn_state == HPB_RGN_INACTIVE) {
		HPB_DEBUG(hpb, "%d - %d evicted", srgn->rgn_idx,
			  srgn->srgn_idx);
		return -EINVAL;
	}

	memset(srgn->mctx->ppn_dirty, 0x00,
	       hpb->entries_per_srgn >> bits_per_byte_shift);

	return 0;
}

static void ufsshpb_clean_active_subregion(struct ufsshpb_lu *hpb,
					  struct ufsshpb_subregion *srgn)
{
	struct ufsshpb_region *rgn;

	BUG_ON(!srgn->mctx);

	rgn = hpb->rgn_tbl + srgn->rgn_idx;

	if (rgn->rgn_state == HPB_RGN_INACTIVE) {
		HPB_DEBUG(hpb, "%d - %d evicted", srgn->rgn_idx,
			  srgn->srgn_idx);
		return;
	}
	srgn->srgn_state = HPB_SRGN_CLEAN;
}

static void ufsshpb_error_active_subregion(struct ufsshpb_lu *hpb,
					  struct ufsshpb_subregion *srgn)
{
	struct ufsshpb_region *rgn;

	BUG_ON(!srgn->mctx);

	rgn = hpb->rgn_tbl + srgn->rgn_idx;

	if (rgn->rgn_state == HPB_RGN_INACTIVE) {
		ERR_MSG("%d - %d evicted", srgn->rgn_idx, srgn->srgn_idx);
		return;
	}
	srgn->srgn_state = HPB_SRGN_DIRTY;
}

#if defined(CONFIG_HPB_DEBUG)
static void ufsshpb_check_ppn(struct ufsshpb_lu *hpb, int rgn_idx, int srgn_idx,
			     struct ufsshpb_map_ctx *mctx, const char *str)
{
	int error = 0;
	u64 val[2];

	BUG_ON(!mctx);

	val[0] = ufsshpb_get_ppn(mctx, 0, &error);
	if (!error)
		val[1] = ufsshpb_get_ppn(mctx, hpb->entries_per_srgn - 1,
					&error);
	if (error)
		val[0] = val[1] = 0;

	HPB_DEBUG(hpb, "%s READ BUFFER %d - %d ( %llx ~ %llx )", str, rgn_idx,
		  srgn_idx, val[0], val[1]);
}
#endif

static bool is_clear_reason(struct ufsshpb_lu *hpb, struct ufsshpb_region *rgn)
{
	struct ufsshpb_subregion *srgn;
	unsigned long flags;
	bool ret = true;
	int srgn_idx;

	spin_lock_irqsave(&hpb->rsp_list_lock, flags);
	if (!list_empty(&rgn->list_inact_rgn))
		ret = false;
	else {
		for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
			srgn = rgn->srgn_tbl + srgn_idx;

			if (!list_empty(&srgn->list_act_srgn)) {
				ret = false;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

	return ret;
}

static void ufsshpb_map_compl_process(struct ufsshpb_req *map_req)
{
	struct ufsshpb_lu *hpb = map_req->hpb;
	struct ufsshpb_region *rgn = hpb->rgn_tbl + map_req->rb.rgn_idx;
	struct ufsshpb_subregion *srgn = rgn->srgn_tbl + map_req->rb.srgn_idx;
	unsigned long flags;

	atomic64_inc(&hpb->map_compl_cnt);

#if defined(CONFIG_HPB_DEBUG)
	if (hpb->debug)
		ufsshpb_check_ppn(hpb, srgn->rgn_idx, srgn->srgn_idx, srgn->mctx,
				 "COMPL");

	TMSG(hpb->ufsf, hpb->lun, "Noti: C RB %d - %d", map_req->rb.rgn_idx,
	     map_req->rb.srgn_idx);
	trace_printk("[rb.cpl]\t rgn %04d (%d)\n", map_req->rb.rgn_idx, rgn->rgn_state);
#endif

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	ufsshpb_clean_active_subregion(hpb, srgn);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);

	if (is_clear_reason(hpb, rgn))
		atomic_set(&rgn->reason, HPB_UPDATE_NONE);
}

static inline bool ufsshpb_is_hpb_flag(struct request *rq)
{
	return false;
}

static inline void ufsshpb_act_rsp_list_add(struct ufsshpb_lu *hpb,
					   struct ufsshpb_subregion *srgn)
{
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	atomic64_inc(&hpb->act_rsp_list_cnt);
#endif
	list_add_tail(&srgn->list_act_srgn, &hpb->lh_act_srgn);
#if defined(CONFIG_HPB_DEBUG)
	trace_printk("[al.add]\t rgn %04d (%d) srgn %d\n",
		     srgn->rgn_idx, hpb->rgn_tbl[srgn->rgn_idx].rgn_state,
		     srgn->srgn_idx);
#endif
}

static inline void ufsshpb_act_rsp_list_del(struct ufsshpb_lu *hpb,
					   struct ufsshpb_subregion *srgn)
{
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	atomic64_dec(&hpb->act_rsp_list_cnt);
#endif
	list_del_init(&srgn->list_act_srgn);
#if defined(CONFIG_HPB_DEBUG)
	trace_printk("[al.del]\t rgn %04d (%d)\n",
		     srgn->rgn_idx, hpb->rgn_tbl[srgn->rgn_idx].rgn_state);
#endif
}

static inline void ufsshpb_inact_rsp_list_add(struct ufsshpb_lu *hpb,
					     struct ufsshpb_region *rgn)
{
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	atomic64_inc(&hpb->inact_rsp_list_cnt);
#endif
	list_add_tail(&rgn->list_inact_rgn, &hpb->lh_inact_rgn);
#if defined(CONFIG_HPB_DEBUG)
	trace_printk("[il.add]\t rgn %04d (%d)\n",
		     rgn->rgn_idx, hpb->rgn_tbl[rgn->rgn_idx].rgn_state);
#endif
}

static inline void ufsshpb_inact_rsp_list_del(struct ufsshpb_lu *hpb,
					     struct ufsshpb_region *rgn)
{
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	atomic64_dec(&hpb->inact_rsp_list_cnt);
#endif
	list_del_init(&rgn->list_inact_rgn);
#if defined(CONFIG_HPB_DEBUG)
	trace_printk("[il.del]\t rgn %04d (%d)\n",
		     rgn->rgn_idx, hpb->rgn_tbl[rgn->rgn_idx].rgn_state);
#endif
}

static void ufsshpb_update_active_info(struct ufsshpb_lu *hpb, int rgn_idx,
				      int srgn_idx, enum HPB_UPDATE_INFO info)
{
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;

	rgn = hpb->rgn_tbl + rgn_idx;
	srgn = rgn->srgn_tbl + srgn_idx;

	if (!list_empty(&rgn->list_inact_rgn))
		ufsshpb_inact_rsp_list_del(hpb, rgn);

	if (list_empty(&srgn->list_act_srgn)) {
		ufsshpb_act_rsp_list_add(hpb, srgn);
#if defined(CONFIG_HPB_DEBUG)
		if (info == HPB_UPDATE_FROM_FS) {
			HPB_DEBUG(hpb, "*** Noti [HOST]: HCM SET %d - %d",
				  rgn_idx, srgn_idx);
			TMSG(hpb->ufsf, hpb->lun, "*** Noti [HOST]: HCM SET %d - %d",
			     rgn_idx, srgn_idx);
			trace_printk("[act.fs]\t HCM SET rgn %04d (%d)\n",
				     rgn_idx, hpb->rgn_tbl[rgn_idx].rgn_state);
		} else if (info == HPB_UPDATE_FROM_DEV) {
			HPB_DEBUG(hpb, "*** Noti [DEV]: ACT %d - %d",
				  rgn_idx, srgn_idx);
			TMSG(hpb->ufsf, hpb->lun, "*** Noti [DEV]: ACT %d - %d",
			     rgn_idx, srgn_idx);
			trace_printk("[act.dv]\t ACT SET rgn %04d (%d)\n",
				     rgn_idx, hpb->rgn_tbl[rgn_idx].rgn_state);
		}
#endif
	}
}

static void ufsshpb_update_inactive_info(struct ufsshpb_lu *hpb, int rgn_idx,
					enum HPB_UPDATE_INFO info)
{
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	int srgn_idx;

	rgn = hpb->rgn_tbl + rgn_idx;

	for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
		srgn = rgn->srgn_tbl + srgn_idx;

		if (!list_empty(&srgn->list_act_srgn))
			ufsshpb_act_rsp_list_del(hpb, srgn);
	}

	if (list_empty(&rgn->list_inact_rgn)) {
		ufsshpb_inact_rsp_list_add(hpb, rgn);
#if defined(CONFIG_HPB_DEBUG)
		if (info == HPB_UPDATE_FROM_FS) {
			HPB_DEBUG(hpb, "*** Noti [HOST]: HCM UNSET %d", rgn_idx);
			TMSG(hpb->ufsf, hpb->lun, "*** Noti [HOST]: HCM UNSET %d",
			     rgn_idx);
			trace_printk("[ina.fs]\t HCM UNSET rgn %04d (%d)\n",
				     rgn_idx, hpb->rgn_tbl[rgn_idx].rgn_state);
		} else if (info == HPB_UPDATE_FROM_DEV) {
			HPB_DEBUG(hpb, "*** Noti [DEV]: INACT %d", rgn_idx);
			TMSG(hpb->ufsf, hpb->lun, "*** Noti [DEV]: INACT %d",
			     rgn_idx);
			trace_printk("[ina.dv]\t INACT rgn %04d (%d)\n",
				     rgn_idx, hpb->rgn_tbl[rgn_idx].rgn_state);
		} else {
			HPB_DEBUG(hpb, "*** ERR : INACT %d", rgn_idx);
			TMSG(hpb->ufsf, hpb->lun, "*** ERR [DEV]: INACT %d",
			     rgn_idx);
			trace_printk("[ina.--]\t ERR INACT rgn %04d (%d)\n",
				     rgn_idx, hpb->rgn_tbl[rgn_idx].rgn_state);
		}
#endif
	}
}

static void ufsshpb_update_active_info_by_flag(struct ufsshpb_lu *hpb,
					      struct scsi_cmnd *cmd)
{
	struct ufsshpb_region *rgn;
	struct request *rq = cmd->request;
	int s_rgn_idx, e_rgn_idx, rgn_idx, srgn_idx;
	bool is_update = false;
	unsigned int start_lpn, end_lpn;
	unsigned long flags;

	start_lpn = ufsshpb_get_lpn(rq);
	end_lpn = start_lpn + ufsshpb_get_len(rq) - 1;

	s_rgn_idx = start_lpn >> hpb->entries_per_rgn_shift;
	e_rgn_idx = end_lpn >> hpb->entries_per_rgn_shift;

	for (rgn_idx = s_rgn_idx; rgn_idx <= e_rgn_idx; rgn_idx++) {
		is_update = false;
		rgn = hpb->rgn_tbl + rgn_idx;

		spin_lock_irqsave(&hpb->hpb_lock, flags);
		if (rgn->rgn_state == HPB_RGN_INACTIVE ||
		    rgn->rgn_state == HPB_RGN_ACTIVE) {
			is_update = true;
			atomic_set(&rgn->reason, HPB_UPDATE_FROM_FS);
		} else if (rgn->rgn_state == HPB_RGN_HCM) {
			if (ufsshpb_is_write_cmd(cmd))
				is_update = true;
		}
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);

		if (is_update) {
#if defined(CONFIG_HPB_DEBUG_SYSFS)
			atomic64_inc(&hpb->fs_set_hcm_cnt);
#endif
#if defined(CONFIG_HPB_DEBUG)
			trace_printk("[fs.add]\t rgn %04d (%d)\n",
				     rgn_idx, rgn->rgn_state);

			TMSG(hpb->ufsf, hpb->lun, "%llu + %u set hcm %d",
			     (unsigned long long) blk_rq_pos(rq),
			     (unsigned int) blk_rq_sectors(rq), rgn_idx);
#endif
			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++)
				ufsshpb_update_active_info(hpb, rgn_idx,
							  srgn_idx,
							  HPB_UPDATE_FROM_FS);
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
		}
	}
}

static void ufsshpb_update_inactive_info_by_flag(struct ufsshpb_lu *hpb,
						struct scsi_cmnd *cmd)
{
	struct ufsshpb_region *rgn;
	struct request *rq = cmd->request;
	int s_rgn_idx, s_srgn_idx, s_offset;
	int e_rgn_idx, e_srgn_idx, e_offset;
	int rgn_idx;
	bool is_update = false;
	unsigned int start_lpn, end_lpn;
	unsigned long flags;

	start_lpn = ufsshpb_get_lpn(rq);
	end_lpn = start_lpn + ufsshpb_get_len(rq) - 1;

	ufsshpb_get_pos_from_lpn(hpb, start_lpn, &s_rgn_idx, &s_srgn_idx,
				&s_offset);
	ufsshpb_get_pos_from_lpn(hpb, end_lpn, &e_rgn_idx, &e_srgn_idx,
				&e_offset);

	if (ufsshpb_is_read_cmd(cmd) && !ufsshpb_is_hpb_flag(rq)) {
		if (s_srgn_idx != 0 || s_offset != 0)
			s_rgn_idx++;

		if (e_srgn_idx != (hpb->srgns_per_rgn - 1) ||
		    e_offset != (hpb->entries_per_srgn - 1))
			e_rgn_idx--;
	}

	for (rgn_idx = s_rgn_idx; rgn_idx <= e_rgn_idx; rgn_idx++) {
		is_update = false;
		rgn = hpb->rgn_tbl + rgn_idx;

		spin_lock_irqsave(&hpb->hpb_lock, flags);
		if (rgn->rgn_state == HPB_RGN_HCM) {
			is_update = true;
			atomic_set(&rgn->reason, HPB_UPDATE_FROM_FS);
		}
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);

		if (is_update) {
#if defined(CONFIG_HPB_DEBUG_SYSFS)
			atomic64_inc(&hpb->fs_unset_hcm_cnt);
#endif
#if defined(CONFIG_HPB_DEBUG)
			trace_printk("[fs.del]\t rgn %04d (%d)\n",
				     rgn_idx, rgn->rgn_state);
			TMSG(hpb->ufsf, hpb->lun, "%llu + %u unset hcm %d",
			     (unsigned long long) blk_rq_pos(rq),
			     (unsigned int) blk_rq_sectors(rq), rgn_idx);
#endif
			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			ufsshpb_update_inactive_info(hpb, rgn_idx,
						    HPB_UPDATE_FROM_FS);
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
		}
	}
}

static void ufsshpb_hit_lru_info_by_read(struct victim_select_info *lru_info,
					struct ufsshpb_region *rgn);

#if defined(CONFIG_HPB_DEBUG_SYSFS)
static void ufsshpb_increase_hit_count(struct ufsshpb_lu *hpb,
				      struct ufsshpb_region *rgn,
				      int transfer_len)
{
	if (rgn->rgn_state == HPB_RGN_PINNED ||
	    rgn->rgn_state == HPB_RGN_HCM) {
		if (transfer_len < HPB_MULTI_CHUNK_LOW) {
			atomic64_inc(&hpb->pinned_low_hit);
			atomic64_inc(&rgn->rgn_pinned_low_hit);
		} else {
			atomic64_inc(&hpb->pinned_high_hit);
			atomic64_inc(&rgn->rgn_pinned_high_hit);
		}
	} else if (rgn->rgn_state == HPB_RGN_ACTIVE) {
		if (transfer_len < HPB_MULTI_CHUNK_LOW) {
			atomic64_inc(&hpb->active_low_hit);
			atomic64_inc(&rgn->rgn_active_low_hit);
		} else {
			atomic64_inc(&hpb->active_high_hit);
			atomic64_inc(&rgn->rgn_active_high_hit);
		}
	}
}

static void ufsshpb_increase_miss_count(struct ufsshpb_lu *hpb,
				       struct ufsshpb_region *rgn,
				       int transfer_len)
{
	if (rgn->rgn_state == HPB_RGN_PINNED ||
	    rgn->rgn_state == HPB_RGN_HCM) {
		if (transfer_len < HPB_MULTI_CHUNK_LOW) {
			atomic64_inc(&hpb->pinned_low_miss);
			atomic64_inc(&rgn->rgn_pinned_low_miss);
		} else {
			atomic64_inc(&hpb->pinned_high_miss);
			atomic64_inc(&rgn->rgn_pinned_high_miss);
		}
	} else if (rgn->rgn_state == HPB_RGN_ACTIVE) {
		if (transfer_len < HPB_MULTI_CHUNK_LOW) {
			atomic64_inc(&hpb->active_low_miss);
			atomic64_inc(&rgn->rgn_active_low_miss);
		} else {
			atomic64_inc(&hpb->active_high_miss);
			atomic64_inc(&rgn->rgn_active_high_miss);
		}
	} else if (rgn->rgn_state == HPB_RGN_INACTIVE) {
		if (transfer_len < HPB_MULTI_CHUNK_LOW) {
			atomic64_inc(&hpb->inactive_low_read);
			atomic64_inc(&rgn->rgn_inactive_low_read);
		} else {
			atomic64_inc(&hpb->inactive_high_read);
			atomic64_inc(&rgn->rgn_inactive_high_read);
		}
	}
}

static void ufsshpb_increase_rb_count(struct ufsshpb_lu *hpb,
				     struct ufsshpb_region *rgn)
{
	if (rgn->rgn_state == HPB_RGN_PINNED ||
	    rgn->rgn_state == HPB_RGN_HCM) {
		atomic64_inc(&hpb->pinned_rb_cnt);
		atomic64_inc(&rgn->rgn_pinned_rb_cnt);
	} else if (rgn->rgn_state == HPB_RGN_ACTIVE) {
		atomic64_inc(&hpb->active_rb_cnt);
		atomic64_inc(&rgn->rgn_active_rb_cnt);
	}
}

static void ufsshpb_debug_sys_init(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_region *rgn;
	int rgn_idx;

	atomic64_set(&hpb->pinned_low_hit, 0);
	atomic64_set(&hpb->pinned_high_hit, 0);
	atomic64_set(&hpb->pinned_low_miss, 0);
	atomic64_set(&hpb->pinned_high_miss, 0);
	atomic64_set(&hpb->active_low_hit, 0);
	atomic64_set(&hpb->active_high_hit, 0);
	atomic64_set(&hpb->active_low_miss, 0);
	atomic64_set(&hpb->active_high_miss, 0);
	atomic64_set(&hpb->inactive_low_read, 0);
	atomic64_set(&hpb->inactive_high_read, 0);
	atomic64_set(&hpb->pinned_rb_cnt, 0);
	atomic64_set(&hpb->active_rb_cnt, 0);
	atomic64_set(&hpb->fs_set_hcm_cnt, 0);
	atomic64_set(&hpb->fs_unset_hcm_cnt, 0);
	atomic64_set(&hpb->unset_hcm_victim, 0);

	for (rgn_idx = 0; rgn_idx < hpb->rgns_per_lu; rgn_idx++) {
		rgn = hpb->rgn_tbl + rgn_idx;
		atomic64_set(&rgn->rgn_pinned_low_hit, 0);
		atomic64_set(&rgn->rgn_pinned_high_hit, 0);
		atomic64_set(&rgn->rgn_pinned_low_miss, 0);
		atomic64_set(&rgn->rgn_pinned_high_miss, 0);
		atomic64_set(&rgn->rgn_active_low_hit, 0);
		atomic64_set(&rgn->rgn_active_high_hit, 0);
		atomic64_set(&rgn->rgn_active_low_miss, 0);
		atomic64_set(&rgn->rgn_active_high_miss, 0);
		atomic64_set(&rgn->rgn_inactive_low_read, 0);
		atomic64_set(&rgn->rgn_inactive_high_read, 0);
		atomic64_set(&rgn->rgn_pinned_rb_cnt, 0);
		atomic64_set(&rgn->rgn_active_rb_cnt, 0);
	}
}
#endif

#if defined(CONFIG_HPB_DEBUG)
static void ufsshpb_blk_fill_rwbs(char *rwbs, unsigned int op)
{
	int i = 0;

	if (op & REQ_PREFLUSH)
		rwbs[i++] = 'F';

	switch (op & REQ_OP_MASK) {
	case REQ_OP_WRITE:
	case REQ_OP_WRITE_SAME:
		rwbs[i++] = 'W';
		break;
	case REQ_OP_DISCARD:
		rwbs[i++] = 'D';
		break;
	case REQ_OP_SECURE_ERASE:
		rwbs[i++] = 'D';
		rwbs[i++] = 'E';
		break;
	case REQ_OP_FLUSH:
		rwbs[i++] = 'F';
		break;
	case REQ_OP_READ:
		rwbs[i++] = 'R';
		break;
	default:
		rwbs[i++] = 'N';
	}

	if (op & REQ_FUA)
		rwbs[i++] = 'F';
	if (op & REQ_RAHEAD)
		rwbs[i++] = 'A';
	if (op & REQ_SYNC)
		rwbs[i++] = 'S';
	if (op & REQ_META)
		rwbs[i++] = 'M';

	rwbs[i] = '\0';
}
#endif

static int ufsshpb_fill_passthrough_rq(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;
	struct scsi_device *sdp = cmd->device;
	struct bio *bio = rq->bio;
	sector_t sector = 0;
	unsigned int data_len = 0;
	u8 *p = NULL;

	/*
	 * UFS is 4K block device.
	 */
	if (unlikely(sdp->sector_size != BLOCK)) {
		ERR_MSG("this device is not 4K block device");
		return -EACCES;
	}

	switch (cmd->cmnd[0]) {
	case UNMAP:
		/*
		 * UNMAP command has just a block descriptor(=a phys_segment).
		 */
		if (unlikely(!bio) ||
		    unlikely(bio->bi_vcnt != 1)) {
		    //unlikely(bio->bi_phys_segments != 1)) {
			ERR_MSG("UNMAP bio info is incorrect");
			return -EINVAL;
		}

		p = page_address(bio->bi_io_vec->bv_page);
		if (unlikely(!p)) {
			ERR_MSG("can't get bio page address");
			return -ENOMEM;
		}

		sector = LI_EN_64(p + 8);
		data_len = LI_EN_32(p + 16);
		break;
	case READ_10:
	case WRITE_10:
		sector = LI_EN_32(cmd->cmnd + 2);
		data_len = LI_EN_16(cmd->cmnd + 7);
		break;
	case READ_16:
	case WRITE_16:
		sector = LI_EN_64(cmd->cmnd + 2);
		data_len = LI_EN_32(cmd->cmnd + 10);
		break;
	default:
		return 0;
	};

	rq->__sector = sector << sects_per_blk_shift;
	rq->__data_len = data_len << sects_per_blk_shift << SECTOR_SHIFT;

	return 0;
}

/* routine : READ10 -> HPB_READ  */
int ufsshpb_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	struct ufsshpb_lu *hpb;
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	struct request *rq;
	struct scsi_cmnd *cmd = lrbp->cmd;
	u64 ppn = 0;
	unsigned long lpn, flags;
	int hpb_ctx_id = MAX_HPB_CONTEXT_ID;
	int transfer_len = TRANSFER_LEN;
	int rgn_idx, srgn_idx, srgn_offset, ret, error = 0;
#if defined(CONFIG_HPB_DEBUG)
	char hint;
	char rwbs[8];
#endif

	/* WKLU could not be HPB-LU */
	if (!lrbp || !ufsf_is_valid_lun(lrbp->lun))
		return 0;

	if (!ufsshpb_is_read_cmd(cmd) && !ufsshpb_is_write_cmd(cmd) &&
	    !ufsshpb_is_discard_cmd(cmd))
		return 0;

	rq = cmd->request;
	if (unlikely(blk_rq_is_passthrough(rq) &&
		     rq->__sector == (sector_t)-1)) {
		ret = ufsshpb_fill_passthrough_rq(cmd);
		if (ret)
			return 0;
	}

	hpb = ufsf->hpb_lup[lrbp->lun];
	ret = ufsshpb_lu_get(hpb);
	if (unlikely(ret))
		return 0;

	lpn = ufsshpb_get_lpn(rq);
	ufsshpb_get_pos_from_lpn(hpb, lpn, &rgn_idx, &srgn_idx, &srgn_offset);
	rgn = hpb->rgn_tbl + rgn_idx;
	srgn = rgn->srgn_tbl + srgn_idx;

	if (hpb->force_disable) {
#if defined(CONFIG_HPB_DEBUG)
		if (ufsshpb_is_read_cmd(cmd)) {
			TMSG(ufsf, hpb->lun, "%llu + %u READ_10",
			     (unsigned long long) blk_rq_pos(rq),
			     (unsigned int) blk_rq_sectors(rq));
			trace_printk("[rd.nor]\t %llu + %u READ_10 (force_disable)\n",
			     (unsigned long long) blk_rq_pos(rq),
			     (unsigned int) blk_rq_sectors(rq));
		}
#endif
		goto put_hpb;
	}

#if defined(CONFIG_HPB_DEBUG)
	if (cmd->request->bio == NULL)
		hint = '-';
	else if (cmd->request->bio->bi_write_hint == WRITE_LIFE_EXTREME)
		hint = 'E';
	else if (cmd->request->bio->bi_write_hint == WRITE_LIFE_MEDIUM)
		hint = 'M';
	else if (cmd->request->bio->bi_write_hint == WRITE_LIFE_LONG)
		hint = 'L';
	else if (cmd->request->bio->bi_write_hint == WRITE_LIFE_SHORT)
		hint = 'S';
	else if (cmd->request->bio->bi_write_hint == WRITE_LIFE_NONE)
		hint = 'N';
	else if (cmd->request->bio->bi_write_hint == WRITE_LIFE_NOT_SET)
		hint = '-';
	ufsshpb_blk_fill_rwbs(rwbs, cmd->request->cmd_flags);
	if (rwbs[0] != 'N')
		trace_printk("[%s]\t%llu\t+%u\t<%c> rgn %05d (%d)\n",
			     rwbs, (u64)blk_rq_pos(cmd->request)>>3,
			     blk_rq_sectors(cmd->request)>>3, hint,
			     rgn_idx, rgn->rgn_state);
#endif

	/*
	 * If cmd type is WRITE, bitmap set to dirty.
	 */
	if (ufsshpb_is_write_cmd(cmd) || ufsshpb_is_discard_cmd(cmd)) {
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		if (rgn->rgn_state != HPB_RGN_INACTIVE) {
			ufsshpb_set_dirty(hpb, lrbp, rgn_idx, srgn_idx,
					 srgn_offset);
		}
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	}

	if (ufsshpb_is_read_cmd(cmd)) {
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		if (!list_empty(&rgn->list_lru_rgn))
			ufsshpb_hit_lru_info_by_read(&hpb->lru_info, rgn);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	}

	if (hpb->hcm_disable != true) {
		if (ufsshpb_is_read_cmd(cmd) && ufsshpb_is_hpb_flag(rq))
			ufsshpb_update_active_info_by_flag(hpb, cmd);
		else if (ufsshpb_is_discard_cmd(cmd) ||
			 (ufsshpb_is_read_cmd(cmd) && !ufsshpb_is_hpb_flag(rq)))
			ufsshpb_update_inactive_info_by_flag(hpb, cmd);
	}

	if (!ufsshpb_is_read_cmd(cmd))
		goto put_hpb;

	if (unlikely(ufsshpb_is_unaligned(rq))) {
#if defined(CONFIG_HPB_DEBUG)
		TMSG_CMD(hpb, "READ_10 not aligned 4KB", rq, rgn_idx, srgn_idx);
		trace_printk("[rd.nor]\t %llu + %u READ_10 rgn %04d (%d) (not aligned)\n",
		     (unsigned long long) blk_rq_pos(rq),
		     (unsigned int) blk_rq_sectors(rq),
		     rgn_idx, rgn->rgn_state);
#endif
		goto put_hpb;
	}

	transfer_len = ufsshpb_get_len(rq);
	if (unlikely(!transfer_len))
		goto put_hpb;

	if (!ufsshpb_is_support_chunk(transfer_len)) {
#if defined(CONFIG_HPB_DEBUG)
		TMSG_CMD(hpb, "READ_10 doesn't support chunk size",
			 rq, rgn_idx, srgn_idx);
		trace_printk("[rd.nor]\t %llu + %u READ_10 rgn %04d (%d) (not support chunk)\n",
		     (unsigned long long) blk_rq_pos(rq),
		     (unsigned int) blk_rq_sectors(rq),
		     rgn_idx, rgn->rgn_state);
#endif
		goto put_hpb;
	}

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	if (ufsshpb_ppn_dirty_check(hpb, lpn, transfer_len)) {
		atomic64_inc(&hpb->miss);
#if defined(CONFIG_HPB_DEBUG_SYSFS)
		ufsshpb_increase_miss_count(hpb, rgn, transfer_len);
#endif
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
#if defined(CONFIG_HPB_DEBUG)
		TMSG_CMD(hpb, "READ_10 E_D", rq, rgn_idx, srgn_idx);
		trace_printk("[rd.nor]\t %llu + %u READ_10 rgn %04d (%d) (hpb map invalid)\n",
		     (unsigned long long) blk_rq_pos(rq),
		     (unsigned int) blk_rq_sectors(rq),
		     rgn_idx, rgn->rgn_state);
#endif
		goto put_hpb;
	}

	ppn = ufsshpb_get_ppn(srgn->mctx, srgn_offset, &error);
#if defined(CONFIG_HPB_ERR_INJECTION)
	if (hpb->err_injection_select != HPB_ERR_INJECTION_DISABLE)
		ppn = ufsshpb_get_err_injection_ppn(hpb, ppn);
#endif
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	if (unlikely(error)) {
		ERR_MSG("get_ppn failed.. err %d region %d subregion %d",
			error, rgn_idx, srgn_idx);
		ufsshpb_lu_put(hpb);
		goto wakeup_ee_worker;
	}

	/*
	 * WRITE_BUFFER CMD support 36K (len=9) ~ 512K (len=128) default.
	 * it is possible to change range of transfer_len through sysfs.
	 */
	if (transfer_len >= hpb->pre_req_min_tr_len &&
	    transfer_len <= hpb->pre_req_max_tr_len) {
		ret = ufsshpb_issue_pre_req(hpb, cmd, &hpb_ctx_id);
		if (ret) {
			if (ret == -EAGAIN) {
				unsigned long timeout;

				timeout = cmd->jiffies_at_alloc +
				     msecs_to_jiffies(hpb->requeue_timeout_ms);

				if (time_before(jiffies, timeout))
					goto requeue;
			}

			atomic64_inc(&hpb->miss);
			goto put_hpb;
		}
	}

	ufsshpb_set_read16_cmd(hpb, cmd, ppn, transfer_len, hpb_ctx_id);
#if defined(CONFIG_HPB_DEBUG)
	trace_printk("[rd.hpb]\t %llu + %u READ_HPB rgn %04d (%d)- context_id %d\n",
	     (unsigned long long) blk_rq_pos(rq),
	     (unsigned int) blk_rq_sectors(rq),
	     rgn_idx, rgn->rgn_state, hpb_ctx_id);
	TMSG(ufsf, hpb->lun, "%llu + %u HPB_READ %d - %d context_id %d",
	     (unsigned long long) blk_rq_pos(rq),
	     (unsigned int) blk_rq_sectors(rq), rgn_idx,
	     srgn_idx, hpb_ctx_id);
#endif

	atomic64_inc(&hpb->hit);
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	ufsshpb_increase_hit_count(hpb, rgn, transfer_len);
#endif
put_hpb:
	ufsshpb_lu_put(hpb);
	return 0;
wakeup_ee_worker:
	ufsshpb_failed(hpb, __func__);
	return 0;
requeue:
	ufsshpb_lu_put(hpb);
	return ret;
}

static int ufsshpb_map_req_error(struct ufsshpb_req *map_req)
{
	struct ufsshpb_lu *hpb = map_req->hpb;
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	struct scsi_sense_hdr sshdr;
	unsigned long flags;

	rgn = hpb->rgn_tbl + map_req->rb.rgn_idx;
	srgn = rgn->srgn_tbl + map_req->rb.srgn_idx;

	scsi_normalize_sense(map_req->sense, SCSI_SENSE_BUFFERSIZE, &sshdr);

	ERR_MSG("code %x sense_key %x asc %x ascq %x", sshdr.response_code,
		sshdr.sense_key, sshdr.asc, sshdr.ascq);
	ERR_MSG("byte4 %x byte5 %x byte6 %x additional_len %x", sshdr.byte4,
		sshdr.byte5, sshdr.byte6, sshdr.additional_length);

	if (sshdr.sense_key != ILLEGAL_REQUEST)
		return 0;

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	if (rgn->rgn_state == HPB_RGN_PINNED) {
		if (sshdr.asc == 0x06 && sshdr.ascq == 0x01) {
#if defined(CONFIG_HPB_DEBUG)
			HPB_DEBUG(hpb, "retry pinned rb %d - %d",
				  map_req->rb.rgn_idx, map_req->rb.srgn_idx);
#endif
			spin_unlock_irqrestore(&hpb->hpb_lock, flags);
			spin_lock(&hpb->retry_list_lock);
			list_add_tail(&map_req->list_req,
				      &hpb->lh_map_req_retry);
			spin_unlock(&hpb->retry_list_lock);

			schedule_delayed_work(&hpb->retry_work,
					      msecs_to_jiffies(RETRY_DELAY_MS));
			return -EAGAIN;
		}
#if defined(CONFIG_HPB_DEBUG)
		HPB_DEBUG(hpb, "pinned rb %d - %d(dirty)",
			  map_req->rb.rgn_idx, map_req->rb.srgn_idx);
#endif
		ufsshpb_error_active_subregion(hpb, srgn);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	} else {
		ufsshpb_error_active_subregion(hpb, srgn);

		spin_unlock_irqrestore(&hpb->hpb_lock, flags);

		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		ufsshpb_update_inactive_info(hpb, map_req->rb.rgn_idx,
					    HPB_UPDATE_NONE);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
#if defined(CONFIG_HPB_DEBUG)
		HPB_DEBUG(hpb, "Non-pinned rb %d will be inactive",
			  map_req->rb.rgn_idx);
#endif
		schedule_work(&hpb->task_work);
	}

	return 0;
}

/* routine : map_req compl */
static void ufsshpb_map_req_compl_fn(struct request *req, blk_status_t error)
{
	struct ufsshpb_req *map_req = (struct ufsshpb_req *) req->end_io_data;
	struct ufsshpb_lu *hpb = map_req->hpb;
	unsigned long flags;
	int ret;

	if (ufsshpb_get_state(hpb->ufsf) == HPB_FAILED)
		goto free_map_req;

	if (error) {
		ERR_MSG("COMP_NOTI: RB number %d ( %d - %d )", error,
			map_req->rb.rgn_idx, map_req->rb.srgn_idx);

		ret = ufsshpb_map_req_error(map_req);
		if (ret)
			goto retry_map_req;
	} else
		ufsshpb_map_compl_process(map_req);

free_map_req:
	bio_put(map_req->bio);
	ufsshpb_put_blk_request(map_req->req);
	spin_lock_irqsave(&hpb->hpb_lock, flags);
	ufsshpb_put_map_req(map_req->hpb, map_req);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
retry_map_req:
	scsi_device_put(hpb->ufsf->sdev_ufs_lu[hpb->lun]);
	ufsshpb_lu_put(hpb);
}

static inline void ufsshpb_set_read_buf_cmd(unsigned char *cdb, int rgn_idx,
					   int srgn_idx, int srgn_mem_size)
{
	cdb[0] = UFSSHPB_READ_BUFFER;
	cdb[1] = UFSSHPB_RB_ID_READ;
	cdb[2] = GET_BYTE_1(rgn_idx);
	cdb[3] = GET_BYTE_0(rgn_idx);
	cdb[4] = GET_BYTE_1(srgn_idx);
	cdb[5] = GET_BYTE_0(srgn_idx);
	cdb[6] = GET_BYTE_2(srgn_mem_size);
	cdb[7] = GET_BYTE_1(srgn_mem_size);
	cdb[8] = GET_BYTE_0(srgn_mem_size);
	cdb[9] = 0x00;
}

static inline void ufsshpb_set_hcm_cmd(unsigned char *cdb, int rgn_idx,
				      int srgn_idx, int srgn_mem_size)
{
	cdb[0] = UFSSHPB_READ_BUFFER;
	cdb[1] = UFSSHPB_RB_ID_SET_HCM;
	cdb[2] = GET_BYTE_1(rgn_idx);
	cdb[3] = GET_BYTE_0(rgn_idx);
	cdb[4] = GET_BYTE_1(srgn_idx);
	cdb[5] = GET_BYTE_0(srgn_idx);
	cdb[6] = GET_BYTE_2(srgn_mem_size);
	cdb[7] = GET_BYTE_1(srgn_mem_size);
	cdb[8] = GET_BYTE_0(srgn_mem_size);
	cdb[9] = 0x00;
}

static int ufsshpb_map_req_add_bio_page(struct ufsshpb_lu *hpb,
				       struct request_queue *q, struct bio *bio,
				       struct ufsshpb_map_ctx *mctx)
{
	struct page *page = NULL;
	int i, ret = 0;

	bio_reset(bio);

	for (i = 0; i < hpb->mpages_per_srgn; i++) {
		/* virt_to_page(p + (OS_PAGE_SIZE * i)); */
		page = mctx->m_page[i];
		if (!page)
			return -ENOMEM;

		ret = bio_add_pc_page(q, bio, page, hpb->mpage_bytes, 0);

		if (ret != hpb->mpage_bytes) {
			ERR_MSG("bio_add_pc_page fail: %d", ret);
			return -ENOMEM;
		}
	}

	return 0;
}

static int ufsshpb_execute_map_req(struct ufsshpb_lu *hpb,
				  struct ufsshpb_req *map_req)
{
	struct scsi_device *sdev;
	struct request_queue *q;
	struct request *req;
	struct bio *bio = map_req->bio;
	struct scsi_request *rq;
	struct ufsshpb_region *rgn = hpb->rgn_tbl + map_req->rb.rgn_idx;
	int ret = 0;

	sdev = hpb->ufsf->sdev_ufs_lu[hpb->lun];
	if (!sdev) {
		ERR_MSG("cannot find scsi_device");
		return -ENODEV;
	}

	ret = ufsf_get_scsi_device(hpb->ufsf->hba, sdev);
	if (ret)
		return ret;

	q = sdev->request_queue;

	ret = ufsshpb_map_req_add_bio_page(hpb, q, bio, map_req->rb.mctx);
	if (ret) {
		scsi_device_put(sdev);
		return ret;
	}

	req = map_req->req;

	/* 1. request setup */
	blk_rq_append_bio(req, &bio); /* req->__data_len is setted */
	req->rq_flags |= RQF_QUIET;
	req->timeout = 30 * HZ;
	req->end_io_data = (void *)map_req;

	/* 2. scsi_request setup */
	rq = scsi_req(req);
	rq->retries = 3;

	if (ufsshpb_is_triggered_by_fs(rgn))
		ufsshpb_set_hcm_cmd(rq->cmd, map_req->rb.rgn_idx,
				   map_req->rb.srgn_idx, hpb->srgn_mem_size);
	else
		ufsshpb_set_read_buf_cmd(rq->cmd, map_req->rb.rgn_idx,
					map_req->rb.srgn_idx, hpb->srgn_mem_size);

	rq->cmd_len = scsi_command_size(rq->cmd);

#if defined(CONFIG_HPB_DEBUG)
	if (hpb->debug)
		ufsshpb_check_ppn(hpb, map_req->rb.rgn_idx, map_req->rb.srgn_idx,
				 map_req->rb.mctx, "ISSUE");

	TMSG(hpb->ufsf, hpb->lun, "Noti: I RB %d - %d", map_req->rb.rgn_idx,
	     map_req->rb.srgn_idx);
	trace_printk("[rb.iss]\t rgn %04d (%d) (reason %d) act_rsp_list_cnt %lld\n",
		     rgn->rgn_idx, rgn->rgn_state, atomic_read(&rgn->reason),
		     atomic64_read(&hpb->act_rsp_list_cnt));
#endif

	blk_execute_rq_nowait(q, NULL, req, 1, ufsshpb_map_req_compl_fn);

	if (ufsshpb_is_triggered_by_fs(rgn))
		atomic64_inc(&hpb->set_hcm_req_cnt);
	else
		atomic64_inc(&hpb->map_req_cnt);

#if defined(CONFIG_HPB_DEBUG_SYSFS)
	ufsshpb_increase_rb_count(hpb, rgn);
#endif
	return 0;
}

static struct ufsshpb_map_ctx *ufsshpb_get_map_ctx(struct ufsshpb_lu *hpb,
						 int *err)
{
	struct ufsshpb_map_ctx *mctx;

	mctx = list_first_entry_or_null(&hpb->lh_map_ctx_free,
					struct ufsshpb_map_ctx, list_table);
	if (mctx) {
		list_del_init(&mctx->list_table);
		hpb->debug_free_table--;
		return mctx;
	}
	*err = -ENOMEM;
	return NULL;
}

static inline void ufsshpb_add_lru_info(struct victim_select_info *lru_info,
				       struct ufsshpb_region *rgn)
{
	if (ufsshpb_is_triggered_by_fs(rgn)) {
		rgn->rgn_state = HPB_RGN_HCM;
		list_add_tail(&rgn->list_lru_rgn,
			      &lru_info->lh_lru_hcm_region);
	} else {
		rgn->rgn_state = HPB_RGN_ACTIVE;
		list_add_tail(&rgn->list_lru_rgn, &lru_info->lh_lru_rgn);
	}

	atomic64_inc(&lru_info->active_cnt);
}

static inline int ufsshpb_add_region(struct ufsshpb_lu *hpb,
				    struct ufsshpb_region *rgn)
{
	struct victim_select_info *lru_info;
	int srgn_idx;
	int err = 0;

	lru_info = &hpb->lru_info;

	for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
		struct ufsshpb_subregion *srgn;

		srgn = rgn->srgn_tbl + srgn_idx;

		srgn->mctx = ufsshpb_get_map_ctx(hpb, &err);
		if (!srgn->mctx) {
			HPB_DEBUG(hpb, "get mctx err %d srgn %d free_table %d",
				  err, srgn_idx, hpb->debug_free_table);
			goto out;
		}

		srgn->srgn_state = HPB_SRGN_DIRTY;
	}

#if defined(CONFIG_HPB_DEBUG)
	HPB_DEBUG(hpb, "\x1b[44m\x1b[32m E->active region: %d \x1b[0m",
		  rgn->rgn_idx);
	TMSG(hpb->ufsf, hpb->lun, "Noti: ACT RG: %d", rgn->rgn_idx);
	trace_printk("[rg.add]\t rgn %04d (%d)\n", rgn->rgn_idx, rgn->rgn_state);
#endif

	ufsshpb_add_lru_info(lru_info, rgn);
out:
	return err;
}

static inline void ufsshpb_put_map_ctx(struct ufsshpb_lu *hpb,
				      struct ufsshpb_map_ctx *mctx)
{
	list_add(&mctx->list_table, &hpb->lh_map_ctx_free);
	hpb->debug_free_table++;
}

static inline void ufsshpb_purge_active_subregion(struct ufsshpb_lu *hpb,
						 struct ufsshpb_subregion *srgn,
						 int state)
{
	if (state == HPB_SRGN_UNUSED) {
		ufsshpb_put_map_ctx(hpb, srgn->mctx);
		srgn->mctx = NULL;
	}

	srgn->srgn_state = state;
}

static inline void ufsshpb_cleanup_lru_info(struct victim_select_info *lru_info,
					   struct ufsshpb_region *rgn)
{
	list_del_init(&rgn->list_lru_rgn);
	rgn->rgn_state = HPB_RGN_INACTIVE;
	atomic64_dec(&lru_info->active_cnt);
}

static void __ufsshpb_evict_region(struct ufsshpb_lu *hpb,
				  struct ufsshpb_region *rgn)
{
	struct victim_select_info *lru_info;
	struct ufsshpb_subregion *srgn;
	int srgn_idx;

	lru_info = &hpb->lru_info;

#if defined(CONFIG_HPB_DEBUG)
	HPB_DEBUG(hpb, "\x1b[41m\x1b[33m C->EVICT region: %d \x1b[0m",
		  rgn->rgn_idx);
	TMSG(hpb->ufsf, hpb->lun, "Noti: EVIC RG: %d", rgn->rgn_idx);
	trace_printk("[rg.evt]\t rgn %04d (%d)\n", rgn->rgn_idx, rgn->rgn_state);
#endif

	ufsshpb_cleanup_lru_info(lru_info, rgn);

	for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
		srgn = rgn->srgn_tbl + srgn_idx;

		ufsshpb_purge_active_subregion(hpb, srgn, HPB_SRGN_UNUSED);
	}
}

static inline void ufsshpb_update_lru_list(enum selection_type selection_type,
					  struct list_head *target_lh,
					  struct ufsshpb_region *rgn)
{
	switch (selection_type) {
	case LRU:
		list_move_tail(&rgn->list_lru_rgn, target_lh);
		break;
	default:
		break;
	}
}

static void ufsshpb_promote_lru_info(struct victim_select_info *lru_info,
				    struct ufsshpb_region *rgn)
{
	struct list_head *target_lh = NULL;

	if (rgn->rgn_state == HPB_RGN_ACTIVE) {
		if (ufsshpb_is_triggered_by_fs(rgn)) {
			rgn->rgn_state = HPB_RGN_HCM;
			target_lh = &lru_info->lh_lru_hcm_region;
#if defined(CONFIG_HPB_DEBUG)
			trace_printk("[fs.prm]\t rgn %04d (%d)\n",
				     rgn->rgn_idx, rgn->rgn_state);
#endif
		}
	}

	if (!target_lh)
		return;

	ufsshpb_update_lru_list(lru_info->selection_type, target_lh, rgn);
}

static void ufsshpb_hit_lru_info_by_read(struct victim_select_info *lru_info,
					struct ufsshpb_region *rgn)
{
	struct list_head *target_lh = NULL;

	if (rgn->rgn_state == HPB_RGN_ACTIVE)
		target_lh = &lru_info->lh_lru_rgn;
	else if (rgn->rgn_state == HPB_RGN_HCM)
		target_lh = &lru_info->lh_lru_hcm_region;

	if (!target_lh)
		return;

	ufsshpb_update_lru_list(lru_info->selection_type, target_lh, rgn);
}

/*
 *  Must be held hpb_lock before call this func.
 */
static int ufsshpb_check_issue_state_srgns(struct ufsshpb_lu *hpb,
					  struct ufsshpb_region *rgn)
{
	struct ufsshpb_subregion *srgn;
	int srgn_idx;

	for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
		srgn  = rgn->srgn_tbl + srgn_idx;

		if (srgn->srgn_state == HPB_SRGN_ISSUED) {
#if defined(CONFIG_HPB_DEBUG)
			trace_printk("[check]\t rgn %04d (%d)\n",
				     rgn->rgn_idx, rgn->rgn_state);
#endif
			return -EPERM;
		}
	}
	return 0;
}

static struct ufsshpb_region *ufsshpb_victim_lru_info(struct ufsshpb_lu *hpb,
						    struct ufsshpb_region *load_rgn)
{
	struct victim_select_info *lru_info = &hpb->lru_info;
	struct ufsshpb_region *rgn;
	struct ufsshpb_region *victim_rgn = NULL;

	switch (lru_info->selection_type) {
	case LRU:
		list_for_each_entry(rgn, &lru_info->lh_lru_rgn, list_lru_rgn) {
			if (!rgn)
				break;

			if (ufsshpb_check_issue_state_srgns(hpb, rgn))
				continue;

			victim_rgn = rgn;
			break;
		}

		if (victim_rgn || !ufsshpb_is_triggered_by_fs(load_rgn))
			break;

		list_for_each_entry(rgn, &lru_info->lh_lru_hcm_region,
				    list_lru_rgn) {
			if (!rgn)
				break;

			if (ufsshpb_check_issue_state_srgns(hpb, rgn))
				continue;

			victim_rgn = rgn;
			break;

		}
		break;
	default:
		break;
	}

	return victim_rgn;
}

/* routine : unset_hcm compl */
static void ufsshpb_unset_hcm_req_compl_fn(struct request *req,
					  blk_status_t error)
{
	struct ufsshpb_req *pre_req = (struct ufsshpb_req *) req->end_io_data;
	struct ufsshpb_lu *hpb = pre_req->hpb;
	struct ufsshpb_region *rgn = hpb->rgn_tbl + pre_req->rb.rgn_idx;
	unsigned long flags;

	if (ufsshpb_get_state(hpb->ufsf) != HPB_PRESENT) {
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		goto free_pre_req;
	}

	if (error) {
		ERR_MSG("COMPL UNSET_HCM err %d rgn %d", error, pre_req->rb.rgn_idx);
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		goto free_pre_req;
	}

#if defined(CONFIG_HPB_DEBUG)
	HPB_DEBUG(hpb, "COMPL UNSET_HCM %d", pre_req->rb.rgn_idx);
	TMSG(hpb->ufsf, hpb->lun, "COMPL UNSET_HCM %d", pre_req->rb.rgn_idx);
	trace_printk("[us.cpl]\t COMPL UNSET_HCM rgn %04d (%d)\n",
		     pre_req->rb.rgn_idx, rgn->rgn_state);
#endif

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	if (rgn->rgn_state != HPB_RGN_INACTIVE)
		rgn->rgn_state = HPB_RGN_ACTIVE;
free_pre_req:
	ufsshpb_put_pre_req(pre_req->hpb, pre_req);
	hpb->num_inflight_hcm_req--;
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	ufsshpb_put_blk_request(req);

	if (is_clear_reason(hpb, rgn))
		atomic_set(&rgn->reason, HPB_UPDATE_NONE);

	scsi_device_put(hpb->ufsf->sdev_ufs_lu[hpb->lun]);
	ufsshpb_lu_put(hpb);
}

static inline void ufsshpb_set_unset_hcm_cmd(unsigned char *cdb, u16 rgn_idx)
{
	cdb[0] = UFSSHPB_WRITE_BUFFER;
	cdb[1] = UFSSHPB_WB_ID_UNSET_HCM;
	cdb[2] = GET_BYTE_1(rgn_idx);
	cdb[3] = GET_BYTE_0(rgn_idx);
	cdb[9] = 0x00;	/* Control = 0x00 */
}

static int ufsshpb_execute_unset_hcm_req(struct ufsshpb_lu *hpb,
					struct scsi_device *sdev,
					struct ufsshpb_req *pre_req)
{
	struct request_queue *q = sdev->request_queue;
	struct request *req;
	struct scsi_request *rq;
	int ret;

	ret = ufsf_get_scsi_device(hpb->ufsf->hba, sdev);
	if (ret) {
		ERR_MSG("issue unset_hcm_req failed. rgn %d err %d",
			pre_req->rb.rgn_idx, ret);
		return ret;
	}

	req = pre_req->req;

	/* 1. request setup */
	req->rq_flags |= RQF_QUIET;
	req->timeout = 30 * HZ;
	req->end_io_data = (void *)pre_req;

	/* 2. scsi_request setup */
	rq = scsi_req(req);
	ufsshpb_set_unset_hcm_cmd(rq->cmd, pre_req->rb.rgn_idx);
	rq->cmd_len = scsi_command_size(rq->cmd);

#if defined(CONFIG_HPB_DEBUG)
	HPB_DEBUG(hpb, "ISSUE UNSET_HCM %d", pre_req->rb.rgn_idx);
	TMSG(hpb->ufsf, hpb->lun, "ISSUE UNSET_HCM %d", pre_req->rb.rgn_idx);
	trace_printk("[us.iss]\t rgn %04d (%d) inact_rsp_list_cnt %lld\n",
		     pre_req->rb.rgn_idx, hpb->rgn_tbl[pre_req->rb.rgn_idx].rgn_state,
		     atomic64_read(&hpb->inact_rsp_list_cnt));
#endif

	blk_execute_rq_nowait(q, NULL, req, 1, ufsshpb_unset_hcm_req_compl_fn);

	atomic64_inc(&hpb->unset_hcm_req_cnt);

	return 0;
}

static int ufsshpb_issue_unset_hcm_req(struct ufsshpb_lu *hpb,
				      struct ufsshpb_region *rgn)
{
	struct ufsshpb_req *pre_req;
	struct request *req = NULL;
	unsigned long flags;
	struct scsi_device *sdev;
	struct ufsf_feature *ufsf = hpb->ufsf;
	int ret = 0;

	sdev = ufsf->sdev_ufs_lu[hpb->lun];
	if (!sdev) {
		ERR_MSG("cannot find scsi_device");
		goto wakeup_ee_worker;
	}

	req = blk_get_request(sdev->request_queue,
			      REQ_OP_SCSI_OUT, BLK_MQ_REQ_PM);
	if (IS_ERR(req))
		return -EAGAIN;

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	if (hpb->num_inflight_hcm_req >= hpb->throttle_hcm_req) {
#if defined(CONFIG_HPB_DEBUG)
		HPB_DEBUG(hpb, "hcm_req throttle. hcm inflight %d hcm throttle %d rgn (%04d)",
			  hpb->num_inflight_hcm_req, hpb->throttle_hcm_req,
			  rgn->rgn_idx);
#endif
		ret = -ENOMEM;
		goto unlock_out;
	}
	pre_req = ufsshpb_get_pre_req(hpb);
	if (!pre_req) {
		ret = -ENOMEM;
		goto unlock_out;
	}
	hpb->num_inflight_hcm_req++;
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);

	pre_req->hpb = hpb;
	pre_req->rb.rgn_idx = rgn->rgn_idx;
	pre_req->req = req;

	ret = ufsshpb_lu_get(hpb);
	if (ret) {
		WARN_MSG("warning: ufsshpb_lu_get failed.. %d", ret);
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		ufsshpb_put_pre_req(hpb, pre_req);
		hpb->num_inflight_hcm_req--;
		goto unlock_out;
	}

	ret = ufsshpb_execute_unset_hcm_req(hpb, sdev, pre_req);
	if (ret) {
		ERR_MSG("issue unset_hcm_req failed. rgn %d err %d",
			rgn->rgn_idx, ret);
		ufsshpb_lu_put(hpb);
		goto wakeup_ee_worker;
	}

	return ret;
unlock_out:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	blk_put_request(req);
	return ret;
wakeup_ee_worker:
	ufsshpb_failed(hpb, __func__);
	return ret;
}

static int ufsshpb_evict_region(struct ufsshpb_lu *hpb, struct ufsshpb_region *rgn)
{
	unsigned long flags, flags2;
	int ret;
	int srgn_idx;

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	if (rgn->rgn_state == HPB_RGN_PINNED) {
		/*
		 * Pinned region should not drop-out.
		 * But if so, it would treat error as critical,
		 * and it will run hpb_eh_work
		 */
		ERR_MSG("pinned region drop-out error");
		goto out;
	} else if (rgn->rgn_state == HPB_RGN_HCM) {
		if (atomic_read(&rgn->reason) == HPB_UPDATE_FROM_DEV) {
			atomic64_inc(&hpb->dev_inact_hcm_cnt);
			/*
			 * Device may issue inactive noti for the HCM active
			 * region. We issues RB again to keep valid HPB entries
			 * for HCM active region.
			 */
			spin_lock_irqsave(&hpb->rsp_list_lock, flags2);
			for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++)
				ufsshpb_update_active_info(hpb, rgn->rgn_idx,
							  srgn_idx,
							  HPB_UPDATE_FROM_DEV);
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags2);
			goto out;
		} else if (ufsshpb_is_triggered_by_fs(rgn)) {
			if (ufsshpb_check_issue_state_srgns(hpb, rgn))
				goto evict_fail;

			spin_unlock_irqrestore(&hpb->hpb_lock, flags);
			ret = ufsshpb_issue_unset_hcm_req(hpb, rgn);
			spin_lock_irqsave(&hpb->hpb_lock, flags);
			if (ret)
				goto evict_fail;
		}
	}

	if (!list_empty(&rgn->list_lru_rgn)) {
		if (ufsshpb_check_issue_state_srgns(hpb, rgn))
			goto evict_fail;

		__ufsshpb_evict_region(hpb, rgn);
	}

out:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	return 0;
evict_fail:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	return -EPERM;
}

static inline struct
ufsshpb_rsp_field *ufsshpb_get_hpb_rsp(struct ufshcd_lrb *lrbp)
{
	return (struct ufsshpb_rsp_field *)&lrbp->ucd_rsp_ptr->sr.sense_data_len;
}

static int ufsshpb_issue_map_req(struct ufsshpb_lu *hpb,
				struct ufsshpb_subregion *srgn)
{
	struct scsi_device *sdev = hpb->ufsf->sdev_ufs_lu[hpb->lun];
	struct request *req = NULL;
	struct bio *bio = NULL;
	struct ufsshpb_req *map_req = NULL;
	unsigned long flags;
	int ret = 0;

	req = blk_get_request(sdev->request_queue, REQ_OP_SCSI_IN,
			      BLK_MQ_REQ_PM);
	if (IS_ERR(req))
		return -EAGAIN;

	bio = bio_alloc(GFP_KERNEL, hpb->mpages_per_srgn);
	if (!bio) {
		blk_put_request(req);
		return -EAGAIN;
	}

	spin_lock_irqsave(&hpb->hpb_lock, flags);

	if (srgn->srgn_state == HPB_SRGN_ISSUED) {
		ret = -EAGAIN;
		goto unlock_out;
	}

	ret = ufsshpb_clean_dirty_bitmap(hpb, srgn);
	if (ret) {
		ret = -EAGAIN;
		goto unlock_out;
	}

	map_req = ufsshpb_get_map_req(hpb);
	if (!map_req) {
		ret = -ENOMEM;
		goto unlock_out;
	}

	srgn->srgn_state = HPB_SRGN_ISSUED;

	spin_unlock_irqrestore(&hpb->hpb_lock, flags);

	map_req->req = req;
	map_req->bio = bio;
	map_req->hpb = hpb;
	map_req->rb.rgn_idx = srgn->rgn_idx;
	map_req->rb.srgn_idx = srgn->srgn_idx;
	map_req->rb.mctx = srgn->mctx;
	map_req->rb.lun = hpb->lun;

	ret = ufsshpb_lu_get(hpb);
	if (unlikely(ret)) {
		ERR_MSG("ufsshpb_lu_get failed. (%d)", ret);
		goto free_map_req;
	}

	ret = ufsshpb_execute_map_req(hpb, map_req);
	if (ret) {
		ERR_MSG("issue map_req failed. [%d-%d] err %d",
			srgn->rgn_idx, srgn->srgn_idx, ret);
		ufsshpb_lu_put(hpb);
		goto wakeup_ee_worker;
	}
	return ret;
free_map_req:
	spin_lock_irqsave(&hpb->hpb_lock, flags);
	srgn->srgn_state = HPB_SRGN_DIRTY;
	ufsshpb_put_map_req(hpb, map_req);
unlock_out:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	bio_put(bio);
	blk_put_request(req);
	return ret;
wakeup_ee_worker:
	ufsshpb_failed(hpb, __func__);
	return ret;
}

static int ufsshpb_load_region(struct ufsshpb_lu *hpb, struct ufsshpb_region *rgn)
{
	struct ufsshpb_region *victim_rgn;
	struct victim_select_info *lru_info = &hpb->lru_info;
	unsigned long flags;
	int ret = 0;

	/*
	 * if already region is added to lru_list,
	 * just initiate the information of lru.
	 * because the region already has the map ctx.
	 * (!list_empty(&rgn->list_region) == region->state=active...)
	 */
	spin_lock_irqsave(&hpb->hpb_lock, flags);
	if (!list_empty(&rgn->list_lru_rgn)) {
		ufsshpb_promote_lru_info(lru_info, rgn);
		goto out;
	}

	if (rgn->rgn_state == HPB_RGN_INACTIVE) {
		if (atomic64_read(&lru_info->active_cnt)
		    == lru_info->max_lru_active_cnt) {
			victim_rgn = ufsshpb_victim_lru_info(hpb, rgn);
			if (!victim_rgn) {
#if defined(CONFIG_HPB_DEBUG)
				HPB_DEBUG(hpb, "UFSSHPB victim_rgn is NULL");
#endif
				if (list_empty(&lru_info->lh_lru_rgn) &&
				    !ufsshpb_is_triggered_by_fs(rgn))
					ret = -EACCES;
				else
					ret = -ENOMEM;
				goto out;
			}

			if (victim_rgn->rgn_state == HPB_RGN_HCM) {
				spin_unlock_irqrestore(&hpb->hpb_lock, flags);
				ret = ufsshpb_issue_unset_hcm_req(hpb, victim_rgn);
#if defined(CONFIG_HPB_DEBUG_SYSFS)
				atomic64_inc(&hpb->unset_hcm_victim);
#endif
				spin_lock_irqsave(&hpb->hpb_lock, flags);
				if (ret) {
					ERR_MSG("issue unset_hcm failed. [%d] err %d",
						rgn->rgn_idx, ret);
					goto out;
				}
			}

#if defined(CONFIG_HPB_DEBUG)
			TMSG(hpb->ufsf, hpb->lun, "Noti: VT RG %d state %d",
			     victim_rgn->rgn_idx, victim_rgn->rgn_state);
			HPB_DEBUG(hpb,
				  "LRU MAX(=%lld). victim choose %d state %d",
				  (long long)atomic64_read(&lru_info->active_cnt),
				  victim_rgn->rgn_idx, victim_rgn->rgn_state);
			trace_printk("[victim]\t rgn %04d (%d)\n",
				     victim_rgn->rgn_idx, victim_rgn->rgn_state);
#endif

			__ufsshpb_evict_region(hpb, victim_rgn);
		}

		ret = ufsshpb_add_region(hpb, rgn);
		if (ret) {
			ERR_MSG("UFSSHPB memory allocation failed (%d)", ret);
			spin_unlock_irqrestore(&hpb->hpb_lock, flags);
			goto wakeup_ee_worker;
		}
	}
out:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	return ret;
wakeup_ee_worker:
	ufsshpb_failed(hpb, __func__);
	return ret;
}

static void ufsshpb_rsp_req_region_update(struct ufsshpb_lu *hpb,
					 struct ufsshpb_rsp_field *rsp_field)
{
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	int num, rgn_idx, srgn_idx;

	/*
	 * If active rgn = inactive rgn, choose inactive rgn.
	 * So, active process -> inactive process
	 */
	for (num = 0; num < rsp_field->active_rgn_cnt; num++) {
		rgn_idx = be16_to_cpu(rsp_field->hpb_active_field[num].active_rgn);
		srgn_idx = be16_to_cpu(rsp_field->hpb_active_field[num].active_srgn);

		if (unlikely(rgn_idx >= hpb->rgns_per_lu)) {
			ERR_MSG("rgn_idx %d is wrong. (Max %d)",
				rgn_idx, hpb->rgns_per_lu);
			continue;
		}

		rgn = hpb->rgn_tbl + rgn_idx;

		if (unlikely(srgn_idx >= rgn->srgn_cnt)) {
			ERR_MSG("srgn_idx %d is wrong. (Max %d)",
				srgn_idx, rgn->srgn_cnt);
			continue;
		}

		srgn = rgn->srgn_tbl + srgn_idx;

		if (ufsshpb_is_triggered_by_fs(rgn))
			continue;

		HPB_DEBUG(hpb, "act num: %d, region: %d, subregion: %d",
			  num + 1, rgn_idx, srgn_idx);
		spin_lock(&hpb->rsp_list_lock);
		ufsshpb_update_active_info(hpb, rgn_idx, srgn_idx,
					  HPB_UPDATE_FROM_DEV);
		spin_unlock(&hpb->rsp_list_lock);

		/* It is just blocking HPB_READ */
		spin_lock(&hpb->hpb_lock);
		if (srgn->srgn_state == HPB_SRGN_CLEAN)
			srgn->srgn_state = HPB_SRGN_DIRTY;
		spin_unlock(&hpb->hpb_lock);

		atomic64_inc(&hpb->rb_active_cnt);
	}

	for (num = 0; num < rsp_field->inactive_rgn_cnt; num++) {
		rgn_idx = be16_to_cpu(rsp_field->hpb_inactive_field[num]);

		if (unlikely(rgn_idx >= hpb->rgns_per_lu)) {
			ERR_MSG("rgn_idx %d is wrong. (Max %d)",
				rgn_idx, hpb->rgns_per_lu);
			continue;
		}

		rgn = hpb->rgn_tbl + rgn_idx;

		if (ufsshpb_is_triggered_by_fs(rgn))
			continue;


		/*
		 * to block HPB_READ
		 */
		spin_lock(&hpb->hpb_lock);
		if (rgn->rgn_state != HPB_RGN_INACTIVE) {
			for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
				srgn = rgn->srgn_tbl + srgn_idx;
				if (srgn->srgn_state == HPB_SRGN_CLEAN)
					srgn->srgn_state = HPB_SRGN_DIRTY;
			}
		}
		spin_unlock(&hpb->hpb_lock);

		HPB_DEBUG(hpb, "inact num: %d, region: %d", num + 1, rgn_idx);
		atomic_set(&rgn->reason, HPB_UPDATE_FROM_DEV);
		spin_lock(&hpb->rsp_list_lock);
		ufsshpb_update_inactive_info(hpb, rgn_idx,
					    HPB_UPDATE_FROM_DEV);
		spin_unlock(&hpb->rsp_list_lock);

		atomic64_inc(&hpb->rb_inactive_cnt);
	}

	TMSG(hpb->ufsf, hpb->lun, "Noti: #ACT %u, #INACT %u",
	     rsp_field->active_rgn_cnt, rsp_field->inactive_rgn_cnt);
	trace_printk("[rspnoti]\t noti ACT %d INACT %d\n",
		     rsp_field->active_rgn_cnt, rsp_field->inactive_rgn_cnt);
	schedule_work(&hpb->task_work);
}

static inline int ufsshpb_may_field_valid(struct ufshcd_lrb *lrbp,
					 struct ufsshpb_rsp_field *rsp_field)
{
	if (be16_to_cpu(rsp_field->sense_data_len) != DEV_SENSE_SEG_LEN ||
	    rsp_field->desc_type != DEV_DES_TYPE ||
	    rsp_field->additional_len != DEV_ADDITIONAL_LEN ||
	    rsp_field->hpb_type == HPB_RSP_NONE ||
	    rsp_field->active_rgn_cnt > MAX_ACTIVE_NUM ||
	    rsp_field->inactive_rgn_cnt > MAX_INACTIVE_NUM ||
	    (!rsp_field->active_rgn_cnt && !rsp_field->inactive_rgn_cnt))
		return -EINVAL;

	if (!ufsf_is_valid_lun(lrbp->lun)) {
		ERR_MSG("LU(%d) is not supported", lrbp->lun);
		return -EINVAL;
	}

	return 0;
}

static bool ufsshpb_is_empty_rsp_lists(struct ufsshpb_lu *hpb)
{
	bool ret = true;
	unsigned long flags;

	spin_lock_irqsave(&hpb->rsp_list_lock, flags);
	if (!list_empty(&hpb->lh_inact_rgn) || !list_empty(&hpb->lh_act_srgn))
		ret = false;
	spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

	return ret;
}

/* routine : isr (ufs) */
void ufsshpb_rsp_upiu(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	struct ufsshpb_lu *hpb;
	struct ufsshpb_rsp_field *rsp_field;
	int data_seg_len, ret;

	data_seg_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2)
		& MASK_RSP_UPIU_DATA_SEG_LEN;

	if (!data_seg_len) {
		bool do_task_work = false;

		if (!ufsf_is_valid_lun(lrbp->lun))
			return;

		hpb = ufsf->hpb_lup[lrbp->lun];
		ret = ufsshpb_lu_get(hpb);
		if (unlikely(ret))
			return;

		do_task_work = !ufsshpb_is_empty_rsp_lists(hpb);
		if (do_task_work)
			schedule_work(&hpb->task_work);

		goto put_hpb;
	}

	rsp_field = ufsshpb_get_hpb_rsp(lrbp);

	if (ufsshpb_may_field_valid(lrbp, rsp_field)) {
		WARN_ON(rsp_field->additional_len != DEV_ADDITIONAL_LEN);
		return;
	}

	hpb = ufsf->hpb_lup[lrbp->lun];
	ret = ufsshpb_lu_get(hpb);
	if (unlikely(ret)) {
		ERR_MSG("ufsshpb_lu_get failed. (%d)", ret);
		return;
	}

	if (hpb->force_map_req_disable)
		goto put_hpb;

	HPB_DEBUG(hpb, "**** HPB Noti %u LUN %u Seg-Len %u, #ACT %u, #INACT %u",
		  rsp_field->hpb_type, lrbp->lun,
		  be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
		  MASK_RSP_UPIU_DATA_SEG_LEN, rsp_field->active_rgn_cnt,
		  rsp_field->inactive_rgn_cnt);
	atomic64_inc(&hpb->rb_noti_cnt);

	switch (rsp_field->hpb_type) {
	case HPB_RSP_REQ_REGION_UPDATE:
		WARN_ON(data_seg_len != DEV_DATA_SEG_LEN);
		ufsshpb_rsp_req_region_update(hpb, rsp_field);
		goto put_hpb;
	default:
		HPB_DEBUG(hpb, "hpb_type is not available : %d",
			  rsp_field->hpb_type);
		goto put_hpb;
	}

put_hpb:
	ufsshpb_lu_put(hpb);
}

static int ufsshpb_execute_map_req_wait(struct ufsshpb_lu *hpb,
				       unsigned char *cmd,
				       struct ufsshpb_subregion *srgn)
{
	struct ufsf_feature *ufsf = hpb->ufsf;
	struct scsi_device *sdev;
	struct request_queue *q;
	struct request *req;
	struct scsi_request *rq;
	struct bio *bio;
	struct scsi_sense_hdr sshdr;
	unsigned long flags;
	int ret = 0;

	sdev = ufsf->sdev_ufs_lu[hpb->lun];
	if (!sdev) {
		ERR_MSG("cannot find scsi_device");
		return -ENODEV;
	}

	q = sdev->request_queue;

	ret = ufsf_get_scsi_device(ufsf->hba, sdev);
	if (ret)
		return ret;

	req = blk_get_request(q, REQ_OP_SCSI_IN, BLK_MQ_REQ_PM);
	if (IS_ERR(req)) {
		ERR_MSG("cannot get request");
		ret = -EIO;
		goto sdev_put_out;
	}

	bio = bio_alloc(GFP_KERNEL, hpb->mpages_per_srgn);
	if (!bio) {
		ret = -ENOMEM;
		goto req_put_out;
	}

	ret = ufsshpb_map_req_add_bio_page(hpb, q, bio, srgn->mctx);
	if (ret)
		goto mem_free_out;

	/* 1. request setup*/
	blk_rq_append_bio(req, &bio); /* req->__data_len */
	req->rq_flags |= RQF_QUIET;
	req->timeout = 30 * HZ;

	/* 2. scsi_request setup */
	rq = scsi_req(req);
	rq->retries = 3;
	rq->cmd_len = scsi_command_size(cmd);
	memcpy(rq->cmd, cmd, rq->cmd_len);

	blk_execute_rq(q, NULL, req, 1);
	if (rq->result) {
		ret = -EIO;
		scsi_normalize_sense(rq->sense, SCSI_SENSE_BUFFERSIZE, &sshdr);
		ERR_MSG("code %x sense_key %x asc %x ascq %x",
			sshdr.response_code, sshdr.sense_key, sshdr.asc,
			sshdr.ascq);
		ERR_MSG("byte4 %x byte5 %x byte6 %x additional_len %x",
			sshdr.byte4, sshdr.byte5, sshdr.byte6,
			sshdr.additional_length);
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		ufsshpb_error_active_subregion(hpb, srgn);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	}
mem_free_out:
	bio_put(bio);
req_put_out:
	blk_put_request(req);
sdev_put_out:
	scsi_device_put(sdev);
	return ret;
}

static inline void ufsshpb_set_unset_hcm_all_cmd(unsigned char *cdb)
{
	cdb[0] = UFSSHPB_WRITE_BUFFER;
	cdb[1] = UFSSHPB_WB_ID_UNSET_HCM_ALL;
	cdb[9] = 0x00;	/* Control = 0x00 */
}

static int ufsshpb_issue_map_req_from_list(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_subregion *srgn;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&hpb->rsp_list_lock, flags);

	while ((srgn = list_first_entry_or_null(&hpb->lh_pinned_srgn,
						struct ufsshpb_subregion,
						list_act_srgn))) {
		unsigned char cmd[10] = { 0 };

		list_del_init(&srgn->list_act_srgn);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
		ufsshpb_set_read_buf_cmd(cmd, srgn->rgn_idx, srgn->srgn_idx,
					hpb->srgn_mem_size);

#if defined(CONFIG_HPB_DEBUG)
		if (hpb->debug)
			ufsshpb_check_ppn(hpb, srgn->rgn_idx, srgn->srgn_idx,
					 srgn->mctx, "ISSUE");

		TMSG(hpb->ufsf, hpb->lun, "Pinned: I RB %d - %d",
		     srgn->rgn_idx, srgn->srgn_idx);
		trace_printk("[rb.iss]\t PINNED rgn %04d (%d)\n",
			     srgn->rgn_idx, hpb->rgn_tbl[srgn->rgn_idx].rgn_state);
#endif
		ret = ufsshpb_execute_map_req_wait(hpb, cmd, srgn);
		if (ret < 0) {
			ERR_MSG("region %d sub %d failed with err %d",
				srgn->rgn_idx, srgn->srgn_idx, ret);
			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			if (list_empty(&srgn->list_act_srgn))
				list_add(&srgn->list_act_srgn,
					 &hpb->lh_pinned_srgn);
			continue;
		}
#if defined(CONFIG_HPB_DEBUG)
		if (hpb->debug)
			ufsshpb_check_ppn(hpb, srgn->rgn_idx, srgn->srgn_idx,
					 srgn->mctx, "COMPL");
		TMSG(hpb->ufsf, hpb->lun, "Noti: C RB %d - %d",
		     srgn->rgn_idx, srgn->srgn_idx);
		trace_printk("[rb.cpl]\t PINNED rgn %04d (%d)\n",
			     srgn->rgn_idx, hpb->rgn_tbl[srgn->rgn_idx].rgn_state);
#endif
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		ufsshpb_clean_active_subregion(hpb, srgn);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);

		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
	}

	spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

	return 0;
}

static void ufsshpb_pinned_work_handler(struct work_struct *work)
{
	struct ufsshpb_lu *hpb;
	int ret;

	hpb = container_of(work, struct ufsshpb_lu, pinned_work);
#if defined(CONFIG_HPB_DEBUG)
	HPB_DEBUG(hpb, "worker start for pinned region");
#endif
	pm_runtime_get_sync(hpb->ufsf->hba->dev);
	ufsshpb_lu_get(hpb);

	if (!list_empty(&hpb->lh_pinned_srgn)) {
		ret = ufsshpb_issue_map_req_from_list(hpb);
		/*
		 * if its function failed at init time,
		 * ufsshpb-device will request map-req,
		 * so it is not critical-error, and just finish work-handler
		 */
		if (ret)
			HPB_DEBUG(hpb, "failed map-issue. ret %d", ret);
	}
	ufsshpb_lu_put(hpb);
	pm_runtime_put_sync(hpb->ufsf->hba->dev);
	HPB_DEBUG(hpb, "worker end");
}

static int ufsshpb_check_pm(struct ufsshpb_lu *hpb)
{
	struct ufs_hba *hba = hpb->ufsf->hba;

	if (hba->pm_op_in_progress ||
	    hba->curr_dev_pwr_mode != UFS_ACTIVE_PWR_MODE) {
		INFO_MSG("hba current power state %d pm_progress %d",
			 hba->curr_dev_pwr_mode,
			 hba->pm_op_in_progress);
		return -ENODEV;
	}
	return 0;
}

static void ufsshpb_retry_work_handler(struct work_struct *work)
{
	struct ufsshpb_lu *hpb;
	struct delayed_work *dwork = to_delayed_work(work);
	struct ufsshpb_req *map_req, *next;
	unsigned long flags;
	bool do_retry = false;
	int ret = 0;

	LIST_HEAD(retry_list);

	hpb = container_of(dwork, struct ufsshpb_lu, retry_work);

	if (ufsshpb_check_pm(hpb))
		return;

	ret = ufsshpb_lu_get(hpb);
	if (unlikely(ret)) {
		ERR_MSG("ufsshpb_lu_get failed. (%d)", ret);
		return;
	}
#if defined(CONFIG_HPB_DEBUG)
	HPB_DEBUG(hpb, "retry worker start");
#endif
	spin_lock_bh(&hpb->retry_list_lock);
	list_splice_init(&hpb->lh_map_req_retry, &retry_list);
	spin_unlock_bh(&hpb->retry_list_lock);

	list_for_each_entry_safe(map_req, next, &retry_list, list_req) {
		if (ufsshpb_get_state(hpb->ufsf) == HPB_SUSPEND) {
			INFO_MSG("suspend state. Issue READ_BUFFER in the next turn");
			spin_lock_bh(&hpb->retry_list_lock);
			list_splice_init(&retry_list, &hpb->lh_map_req_retry);
			spin_unlock_bh(&hpb->retry_list_lock);
			do_retry = true;
			break;
		}

		list_del_init(&map_req->list_req);

		ret = ufsshpb_lu_get(hpb);
		if (unlikely(ret)) {
			WARN_MSG("ufsshpb_lu_get failed (%d)", ret);
			bio_put(map_req->bio);
			blk_put_request(map_req->req);
			spin_lock_irqsave(&hpb->hpb_lock, flags);
			ufsshpb_put_map_req(hpb, map_req);
			spin_unlock_irqrestore(&hpb->hpb_lock, flags);
			continue;
		}

		ret = ufsshpb_execute_map_req(hpb, map_req);
		if (ret) {
			ERR_MSG("issue map_req failed. [%d-%d] err %d",
				map_req->rb.rgn_idx, map_req->rb.srgn_idx, ret);
			ufsshpb_lu_put(hpb);
			goto wakeup_ee_worker;
		}
	}
	HPB_DEBUG(hpb, "worker end");
	ufsshpb_lu_put(hpb);

	if (do_retry)
		schedule_delayed_work(&hpb->retry_work,
				      msecs_to_jiffies(RETRY_DELAY_MS));
	return;
wakeup_ee_worker:
	ufsshpb_lu_put(hpb);
	ufsshpb_failed(hpb, __func__);
}

static void ufsshpb_add_starved_list(struct ufsshpb_lu *hpb,
				    struct ufsshpb_region *rgn,
				    struct list_head *starved_list)
{
	struct ufsshpb_subregion *srgn;
	int srgn_idx;

	if (!list_empty(&rgn->list_inact_rgn))
		return;

	for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
		srgn = rgn->srgn_tbl + srgn_idx;

		if (!list_empty(&srgn->list_act_srgn))
			return;
	}

	/* It will be added to hpb->lh_inact_rgn */
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	atomic64_inc(&hpb->inact_rsp_list_cnt);
#endif
	list_add_tail(&rgn->list_inact_rgn, starved_list);
#if defined(CONFIG_HPB_DEBUG)
	trace_printk("[sl.add]\t rgn %04d (%d)\n",
		     rgn->rgn_idx, rgn->rgn_state);
#endif
}

static void ufsshpb_run_inactive_region_list(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_region *rgn;
	unsigned long flags;
	int ret;
	LIST_HEAD(starved_list);

	spin_lock_irqsave(&hpb->rsp_list_lock, flags);
	while ((rgn = list_first_entry_or_null(&hpb->lh_inact_rgn,
					       struct ufsshpb_region,
					       list_inact_rgn))) {
		if (ufsshpb_get_state(hpb->ufsf) == HPB_SUSPEND) {
			INFO_MSG("suspend state. Inactivate rgn the next turn");
			break;
		}

#if defined(CONFIG_HPB_DEBUG)
		trace_printk("[tw.ina]\t inact_rgn rgn %04d (%d)\n",
			     rgn->rgn_idx, rgn->rgn_state);
#endif
		ufsshpb_inact_rsp_list_del(hpb, rgn);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

		ret = ufsshpb_evict_region(hpb, rgn);
		if (ret) {
			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			ufsshpb_add_starved_list(hpb, rgn, &starved_list);
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
		}

		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
	}

	list_splice(&starved_list, &hpb->lh_inact_rgn);
	spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
}

static void ufsshpb_add_active_list(struct ufsshpb_lu *hpb,
				   struct ufsshpb_region *rgn,
				   struct ufsshpb_subregion *srgn)
{
	if (!list_empty(&rgn->list_inact_rgn))
		return;

	if (!list_empty(&srgn->list_act_srgn)) {
		list_move(&srgn->list_act_srgn, &hpb->lh_act_srgn);
		return;
	}
#if defined(CONFIG_HPB_DEBUG)
	trace_printk("[al.add]\trgn: %04d (%d)\n", rgn->rgn_idx, rgn->rgn_state);
#endif

	ufsshpb_act_rsp_list_add(hpb, srgn);
}

static void ufsshpb_run_active_subregion_list(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&hpb->rsp_list_lock, flags);
	while ((srgn = list_first_entry_or_null(&hpb->lh_act_srgn,
						struct ufsshpb_subregion,
						list_act_srgn))) {
		if (ufsshpb_get_state(hpb->ufsf) == HPB_SUSPEND) {
			INFO_MSG("suspend state. Issuing READ_BUFFER the next turn");
			break;
		}

		ufsshpb_act_rsp_list_del(hpb, srgn);

		if (hpb->force_map_req_disable) {
#if defined(CONFIG_HPB_DEBUG)
			HPB_DEBUG(hpb, "map_req disabled");
#endif
			continue;
		}

		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

		rgn = hpb->rgn_tbl + srgn->rgn_idx;
#if defined(CONFIG_HPB_DEBUG)
		trace_printk("[tw.act]\t run_act %04d (%d)\n",
			     rgn->rgn_idx, rgn->rgn_state);
#endif

		ret = ufsshpb_load_region(hpb, rgn);
		if (ret) {
			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			if (ret == -EACCES) {
				continue;
			} else {
				ufsshpb_add_active_list(hpb, rgn, srgn);
				break;
			}
		}

		ret = ufsshpb_issue_map_req(hpb, srgn);
		if (ret) {
			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			ufsshpb_add_active_list(hpb, rgn, srgn);
			break;
		}

		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
	}
	spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
}

static void ufsshpb_task_work_handler(struct work_struct *work)
{
	struct ufsshpb_lu *hpb;
	int ret;

	hpb = container_of(work, struct ufsshpb_lu, task_work);
	ret = ufsshpb_lu_get(hpb);
	if (unlikely(ret)) {
		if (hpb)
			WARN_MSG("lu_get failed.. hpb_state %d",
				 ufsshpb_get_state(hpb->ufsf));
		WARN_MSG("warning: ufsshpb_lu_get failed %d..", ret);
		return;
	}

	ufsshpb_run_inactive_region_list(hpb);
	ufsshpb_run_active_subregion_list(hpb);

	ufsshpb_lu_put(hpb);
}

static inline void ufsshpb_map_req_mempool_remove(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_req *map_req;
	int i;

	for (i = 0; i < hpb->qd; i++) {
		map_req = hpb->map_req + i;
		if (map_req->req)
			blk_put_request(map_req->req);
		if (map_req->bio)
			bio_put(map_req->bio);
	}

	kfree(hpb->map_req);
}

static inline void ufsshpb_pre_req_mempool_remove(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_req *pre_req;
	int i;

	for (i = 0; i < hpb->qd; i++) {
		pre_req = hpb->pre_req + i;
		if (pre_req->req)
			blk_put_request(pre_req->req);
		if (pre_req->bio)
			bio_put(pre_req->bio);
		__free_page(pre_req->wb.m_page);
	}

	kfree(hpb->pre_req);
}

static void ufsshpb_table_mempool_remove(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_map_ctx *mctx, *next;
	int i;

	/*
	 * the mctx in the lh_map_ctx_free has been allocated completely.
	 */
	list_for_each_entry_safe(mctx, next, &hpb->lh_map_ctx_free,
				 list_table) {
		for (i = 0; i < hpb->mpages_per_srgn; i++)
			__free_page(mctx->m_page[i]);

		vfree(mctx->ppn_dirty);
		kfree(mctx->m_page);
		kfree(mctx);
		hpb->alloc_mctx--;
	}
}

/*
 * this function doesn't need to hold lock due to be called in init.
 * (hpb_lock, rsp_list_lock, etc..)
 */
static int ufsshpb_init_pinned_active_region(struct ufsshpb_lu *hpb,
					    struct ufsshpb_region *rgn)
{
	struct ufsshpb_subregion *srgn;
	int srgn_idx, j;
	int err = 0;

	for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
		srgn = rgn->srgn_tbl + srgn_idx;

		srgn->mctx = ufsshpb_get_map_ctx(hpb, &err);
		if (err) {
			ERR_MSG("get mctx err %d srgn %d free_table %d",
				err, srgn_idx, hpb->debug_free_table);
			goto release;
		}

		srgn->srgn_state = HPB_SRGN_ISSUED;
		/*
		 * no need to clean ppn_dirty bitmap
		 * because it is vzalloc-ed @ufsshpb_table_mempool_init()
		 */
		list_add_tail(&srgn->list_act_srgn, &hpb->lh_pinned_srgn);
	}

	rgn->rgn_state = HPB_RGN_PINNED;

	return 0;

release:
	for (j = 0; j < srgn_idx; j++) {
		srgn = rgn->srgn_tbl + j;
		ufsshpb_put_map_ctx(hpb, srgn->mctx);
	}

	return err;
}

static inline bool ufsshpb_is_pinned_region(struct ufsshpb_lu *hpb, int rgn_idx)
{
	if (hpb->lu_pinned_end_offset != -1 &&
	    rgn_idx >= hpb->lu_pinned_rgn_startidx &&
	    rgn_idx <= hpb->lu_pinned_end_offset)
		return true;

	return false;
}

static inline void ufsshpb_init_jobs(struct ufsshpb_lu *hpb)
{
	INIT_WORK(&hpb->pinned_work, ufsshpb_pinned_work_handler);
	INIT_DELAYED_WORK(&hpb->retry_work, ufsshpb_retry_work_handler);
	INIT_WORK(&hpb->task_work, ufsshpb_task_work_handler);
}

static inline void ufsshpb_cancel_jobs(struct ufsshpb_lu *hpb)
{
	cancel_work_sync(&hpb->pinned_work);
	cancel_delayed_work_sync(&hpb->retry_work);
	cancel_work_sync(&hpb->task_work);
}

static void ufsshpb_init_subregion_tbl(struct ufsshpb_lu *hpb,
				      struct ufsshpb_region *rgn)
{
	int srgn_idx;

	for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
		struct ufsshpb_subregion *srgn = rgn->srgn_tbl + srgn_idx;

		INIT_LIST_HEAD(&srgn->list_act_srgn);

		srgn->rgn_idx = rgn->rgn_idx;
		srgn->srgn_idx = srgn_idx;
		srgn->srgn_state = HPB_SRGN_UNUSED;
	}
}

static inline int ufsshpb_alloc_subregion_tbl(struct ufsshpb_lu *hpb,
					     struct ufsshpb_region *rgn,
					     int srgn_cnt)
{
	rgn->srgn_tbl =
		kzalloc(sizeof(struct ufsshpb_subregion) * srgn_cnt, GFP_KERNEL);
	if (!rgn->srgn_tbl)
		return -ENOMEM;

	rgn->srgn_cnt = srgn_cnt;
	return 0;
}

static int ufsshpb_table_mempool_init(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_map_ctx *mctx = NULL;
	int i, j, k;

	INIT_LIST_HEAD(&hpb->lh_map_ctx_free);

	hpb->alloc_mctx = hpb->lu_max_active_rgns * hpb->srgns_per_rgn;

	for (i = 0; i < hpb->alloc_mctx; i++) {
		mctx = kmalloc(sizeof(struct ufsshpb_map_ctx), GFP_KERNEL);
		if (!mctx)
			goto release_mem;

		mctx->m_page =
			kzalloc(sizeof(struct page *) * hpb->mpages_per_srgn,
				GFP_KERNEL);
		if (!mctx->m_page)
			goto release_mem;

		mctx->ppn_dirty = vzalloc(hpb->entries_per_srgn >>
					  bits_per_byte_shift);
		if (!mctx->ppn_dirty)
			goto release_mem;

		for (j = 0; j < hpb->mpages_per_srgn; j++) {
			mctx->m_page[j] = alloc_page(GFP_KERNEL | __GFP_ZERO);
			if (!mctx->m_page[j]) {
				for (k = 0; k < j; k++)
					__free_page(mctx->m_page[k]);
				goto release_mem;
			}
		}

		INIT_LIST_HEAD(&mctx->list_table);
		list_add(&mctx->list_table, &hpb->lh_map_ctx_free);

		hpb->debug_free_table++;
	}

	INFO_MSG("The number of mctx = (%d). debug_free_table (%d)",
		 hpb->alloc_mctx, hpb->debug_free_table);
	return 0;
release_mem:
	/*
	 * mctxs already added in lh_map_ctx_free will be removed
	 * in the caller function.
	 */
	if (mctx) {
		kfree(mctx->m_page);
		vfree(mctx->ppn_dirty);
		kfree(mctx);
	}
	return -ENOMEM;
}

static int ufsshpb_map_req_mempool_init(struct ufsshpb_lu *hpb)
{
	struct scsi_device *sdev;
	struct request_queue *q;
	struct ufsshpb_req *map_req = NULL;
	int qd = hpb->qd;
	int i;

	sdev = hpb->ufsf->sdev_ufs_lu[hpb->lun];
	q = sdev->request_queue;

	INIT_LIST_HEAD(&hpb->lh_map_req_free);
	INIT_LIST_HEAD(&hpb->lh_map_req_retry);

	hpb->map_req = kzalloc(sizeof(struct ufsshpb_req) * qd, GFP_KERNEL);
	if (!hpb->map_req)
		goto release_mem;

	for (i = 0; i < qd; i++) {
		map_req = hpb->map_req + i;
		INIT_LIST_HEAD(&map_req->list_req);
		map_req->req = NULL;
		map_req->bio = NULL;

		list_add_tail(&map_req->list_req, &hpb->lh_map_req_free);
	}

	return 0;
release_mem:
	kfree(hpb->map_req);
	return -ENOMEM;
}

static int ufsshpb_pre_req_mempool_init(struct ufsshpb_lu *hpb)
{
	struct scsi_device *sdev;
	struct request_queue *q;
	struct ufsshpb_req *pre_req = NULL;
	int qd = hpb->qd;
	int i, j;

	INIT_LIST_HEAD(&hpb->lh_pre_req_free);

	sdev = hpb->ufsf->sdev_ufs_lu[hpb->lun];
	q = sdev->request_queue;

	hpb->pre_req = kzalloc(sizeof(struct ufsshpb_req) * qd, GFP_KERNEL);
	if (!hpb->pre_req)
		goto release_mem;

	for (i = 0; i < qd; i++) {
		pre_req = hpb->pre_req + i;
		INIT_LIST_HEAD(&pre_req->list_req);
		pre_req->req = NULL;
		pre_req->bio = NULL;

		pre_req->wb.m_page = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!pre_req->wb.m_page) {
			for (j = 0; j < i; j++)
				__free_page(hpb->pre_req[j].wb.m_page);

			goto release_mem;
		}
		list_add_tail(&pre_req->list_req, &hpb->lh_pre_req_free);
	}

	return 0;
release_mem:
	kfree(hpb->pre_req);
	return -ENOMEM;
}

static void ufsshpb_find_lu_qd(struct ufsshpb_lu *hpb)
{
	struct scsi_device *sdev;
	struct ufs_hba *hba;

	sdev = hpb->ufsf->sdev_ufs_lu[hpb->lun];
	hba = hpb->ufsf->hba;

	/*
	 * ufshcd_slave_alloc(sdev) -> ufshcd_set_queue_depth(sdev)
	 * a lu-queue-depth compared with lu_info and hba->nutrs
	 * is selected in ufshcd_set_queue_depth()
	 */
	hpb->qd = sdev->queue_depth;
	INFO_MSG("lu (%d) queue_depth (%d)", hpb->lun, hpb->qd);
	if (!hpb->qd) {
		hpb->qd = hba->nutrs;
		INFO_MSG("lu_queue_depth is 0. we use device's queue info.");
		INFO_MSG("hba->nutrs = (%d)", hba->nutrs);
	}

	hpb->throttle_map_req = hpb->qd;
	hpb->throttle_pre_req = hpb->qd;
	hpb->throttle_hcm_req = hpb->qd;
	hpb->num_inflight_map_req = 0;
	hpb->num_inflight_pre_req = 0;
	hpb->num_inflight_hcm_req = 0;
}

static void ufsshpb_init_lu_constant(struct ufsshpb_dev_info *hpb_dev_info,
				    struct ufsshpb_lu *hpb)
{
	unsigned long long rgn_unit_size, rgn_mem_size;
	int entries_per_rgn;

	hpb->debug = false;

	ufsshpb_find_lu_qd(hpb);

	/* for pre_req */
	hpb->pre_req_min_tr_len = HPB_MULTI_CHUNK_LOW;
	hpb->pre_req_max_tr_len = HPB_MULTI_CHUNK_HIGH;
	hpb->ctx_id_ticket = 0;
	hpb->requeue_timeout_ms = DEFAULT_WB_REQUEUE_TIME_MS;

	/*	From descriptors	*/
	rgn_unit_size = (unsigned long long)
		SECTOR * (0x01 << hpb_dev_info->rgn_size);
	rgn_mem_size = rgn_unit_size / BLOCK * HPB_ENTRY_SIZE;

	hpb->srgn_unit_size = (unsigned long long)
		SECTOR * (0x01 << hpb_dev_info->srgn_size);
	hpb->srgn_mem_size = hpb->srgn_unit_size / BLOCK * HPB_ENTRY_SIZE;

	/* relation : lu <-> region <-> sub region <-> entry */
	entries_per_rgn = rgn_mem_size / HPB_ENTRY_SIZE;
	hpb->entries_per_srgn = hpb->srgn_mem_size / HPB_ENTRY_SIZE;
	hpb->srgns_per_rgn = rgn_mem_size / hpb->srgn_mem_size;

	/*
	 * regions_per_lu = (lu_num_blocks * 4096) / region_unit_size
	 *	          = (lu_num_blocks * HPB_ENTRY_SIZE) / region_mem_size
	 */
	hpb->rgns_per_lu =
		((unsigned long long)hpb->lu_num_blocks
		 + (rgn_mem_size / HPB_ENTRY_SIZE) - 1)
		/ (rgn_mem_size / HPB_ENTRY_SIZE);
	hpb->srgns_per_lu =
		((unsigned long long)hpb->lu_num_blocks
		 + (hpb->srgn_mem_size / HPB_ENTRY_SIZE) - 1)
		/ (hpb->srgn_mem_size / HPB_ENTRY_SIZE);

	/* mempool info */
	hpb->mpage_bytes = OS_PAGE_SIZE;
	hpb->mpages_per_srgn = hpb->srgn_mem_size / hpb->mpage_bytes;

	/* Bitmask Info. */
	hpb->dwords_per_srgn = hpb->entries_per_srgn / BITS_PER_DWORD;
	hpb->entries_per_rgn_shift = ffs(entries_per_rgn) - 1;
	hpb->entries_per_rgn_mask = entries_per_rgn - 1;
	hpb->entries_per_srgn_shift = ffs(hpb->entries_per_srgn) - 1;
	hpb->entries_per_srgn_mask = hpb->entries_per_srgn - 1;

	INFO_MSG("===== From Device Descriptor! =====");
	INFO_MSG("hpb_region_size = (%d), hpb_subregion_size = (%d)",
		 hpb_dev_info->rgn_size, hpb_dev_info->srgn_size);
	INFO_MSG("=====   Constant Values(LU)   =====");
	INFO_MSG("region_unit_size = (%lld), region_mem_size (%lld)",
		 rgn_unit_size, rgn_mem_size);
	INFO_MSG("subregion_unit_size = (%lld), subregion_mem_size (%d)",
		 hpb->srgn_unit_size, hpb->srgn_mem_size);

	INFO_MSG("lu_num_blocks = (%d)", hpb->lu_num_blocks);
	INFO_MSG("regions_per_lu = (%d), subregions_per_lu = (%d)",
		 hpb->rgns_per_lu, hpb->srgns_per_lu);

	INFO_MSG("subregions_per_region = %d", hpb->srgns_per_rgn);
	INFO_MSG("entries_per_region (%u) shift (%u) mask (0x%X)",
		 entries_per_rgn, hpb->entries_per_rgn_shift,
		 hpb->entries_per_rgn_mask);
	INFO_MSG("entries_per_subregion (%u) shift (%u) mask (0x%X)",
		 hpb->entries_per_srgn, hpb->entries_per_srgn_shift,
		 hpb->entries_per_srgn_mask);
	INFO_MSG("mpages_per_subregion : (%d)", hpb->mpages_per_srgn);
	INFO_MSG("===================================");
}

static int ufsshpb_lu_hpb_init(struct ufsf_feature *ufsf, int lun)
{
	struct ufsshpb_lu *hpb = ufsf->hpb_lup[lun];
	struct ufsshpb_region *rgn_table, *rgn;
	struct ufsshpb_subregion *srgn;
	int rgn_idx, srgn_idx, total_srgn_cnt, srgn_cnt, i, ret = 0;

	ufsshpb_init_lu_constant(&ufsf->hpb_dev_info, hpb);

	hpb->hcm_disable = true;
	rgn_table = vzalloc(sizeof(struct ufsshpb_region) * hpb->rgns_per_lu);
	if (!rgn_table) {
		ret = -ENOMEM;
		goto out;
	}

	INFO_MSG("active_region_table bytes: (%lu)",
		 (sizeof(struct ufsshpb_region) * hpb->rgns_per_lu));

	hpb->rgn_tbl = rgn_table;

	spin_lock_init(&hpb->hpb_lock);
	spin_lock_init(&hpb->retry_list_lock);
	spin_lock_init(&hpb->rsp_list_lock);

	/* init lru information */
	INIT_LIST_HEAD(&hpb->lru_info.lh_lru_rgn);
	INIT_LIST_HEAD(&hpb->lru_info.lh_lru_hcm_region);
	hpb->lru_info.selection_type = LRU;

	INIT_LIST_HEAD(&hpb->lh_pinned_srgn);
	INIT_LIST_HEAD(&hpb->lh_act_srgn);
	INIT_LIST_HEAD(&hpb->lh_inact_rgn);

	INIT_LIST_HEAD(&hpb->lh_map_ctx_free);

	ufsshpb_init_jobs(hpb);

	ret = ufsshpb_map_req_mempool_init(hpb);
	if (ret) {
		ERR_MSG("map_req_mempool init fail!");
		goto release_rgn_table;
	}

	ret = ufsshpb_pre_req_mempool_init(hpb);
	if (ret) {
		ERR_MSG("pre_req_mempool init fail!");
		goto release_map_req_mempool;
	}

	ret = ufsshpb_table_mempool_init(hpb);
	if (ret) {
		ERR_MSG("ppn table mempool init fail!");
		ufsshpb_table_mempool_remove(hpb);
		goto release_pre_req_mempool;
	}

	total_srgn_cnt = hpb->srgns_per_lu;
	INFO_MSG("total_subregion_count: (%d)", total_srgn_cnt);
	for (rgn_idx = 0, srgn_cnt = 0; rgn_idx < hpb->rgns_per_lu;
	     rgn_idx++, total_srgn_cnt -= srgn_cnt) {
		rgn = rgn_table + rgn_idx;
		rgn->rgn_idx = rgn_idx;

		atomic_set(&rgn->reason, HPB_UPDATE_NONE);

		INIT_LIST_HEAD(&rgn->list_inact_rgn);

		/* init lru region information*/
		INIT_LIST_HEAD(&rgn->list_lru_rgn);

		srgn_cnt = min(total_srgn_cnt, hpb->srgns_per_rgn);

		ret = ufsshpb_alloc_subregion_tbl(hpb, rgn, srgn_cnt);
		if (ret)
			goto release_srgns;
		ufsshpb_init_subregion_tbl(hpb, rgn);

		if (ufsshpb_is_pinned_region(hpb, rgn_idx)) {
			ret = ufsshpb_init_pinned_active_region(hpb, rgn);
			if (ret)
				goto release_srgns;
		} else {
			rgn->rgn_state = HPB_RGN_INACTIVE;
		}
	}

	if (total_srgn_cnt != 0) {
		ERR_MSG("error total_subregion_count: %d",
			total_srgn_cnt);
		goto release_srgns;
	}

	atomic64_set(&hpb->lru_info.active_cnt, 0);
	/*
	 * even if creating sysfs failed, ufsshpb could run normally.
	 * so we don't deal with error handling
	 */
	ufsshpb_create_sysfs(ufsf, hpb);

	return 0;
release_srgns:
	for (i = 0; i < rgn_idx; i++) {
		rgn = rgn_table + i;
		if (rgn->srgn_tbl) {
			for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt;
			     srgn_idx++) {
				srgn = rgn->srgn_tbl + srgn_idx;
				if (srgn->mctx)
					ufsshpb_put_map_ctx(hpb, srgn->mctx);
			}
			kfree(rgn->srgn_tbl);
		}
	}

	ufsshpb_table_mempool_remove(hpb);
release_pre_req_mempool:
	ufsshpb_pre_req_mempool_remove(hpb);
release_map_req_mempool:
	ufsshpb_map_req_mempool_remove(hpb);
release_rgn_table:
	vfree(rgn_table);
out:
	return ret;
}

static inline int ufsshpb_version_check(struct ufsshpb_dev_info *hpb_dev_info)
{
	INFO_MSG("Support HPB Spec : Driver = (%.4X)  Device = (%.4X)",
		 UFSSHPB_VER, hpb_dev_info->version);

	INFO_MSG("HPB Driver Version : (%.6X%s)", UFSSHPB_DD_VER, UFSSHPB_DD_VER_POST);

	if (hpb_dev_info->version != UFSSHPB_VER) {
		ERR_MSG("ERROR: HPB Spec Version mismatch. So HPB disabled.");
		return -ENODEV;
	}
	return 0;
}

void ufsshpb_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf)
{
	struct ufsshpb_dev_info *hpb_dev_info = &ufsf->hpb_dev_info;
	int ret;

	if (desc_buf[DEVICE_DESC_PARAM_UFS_FEAT] & UFS_FEATURE_SUPPORT_HPB_BIT)
		INFO_MSG("bUFSFeaturesSupport: HPB is set");
	else {
		INFO_MSG("bUFSFeaturesSupport: HPB not support");
		ufsshpb_set_state(ufsf, HPB_FAILED);
		return;
	}

	hpb_dev_info->version = LI_EN_16(desc_buf + DEVICE_DESC_PARAM_HPB_VER);

	ret = ufsshpb_version_check(hpb_dev_info);
	if (ret)
		ufsshpb_set_state(ufsf, HPB_FAILED);
}

void ufsshpb_get_geo_info(struct ufsf_feature *ufsf, u8 *geo_buf)
{
	struct ufsshpb_dev_info *hpb_dev_info = &ufsf->hpb_dev_info;
	int hpb_device_max_active_rgns = 0;

	hpb_dev_info->num_lu = geo_buf[GEOMETRY_DESC_HPB_NUMBER_LU];
	if (hpb_dev_info->num_lu == 0) {
		ERR_MSG("Don't have a lu for hpb.");
		ufsshpb_set_state(ufsf, HPB_FAILED);
		return;
	}

	hpb_dev_info->rgn_size = geo_buf[GEOMETRY_DESC_HPB_REGION_SIZE];
	hpb_dev_info->srgn_size = geo_buf[GEOMETRY_DESC_HPB_SUBREGION_SIZE];
	hpb_device_max_active_rgns =
		LI_EN_16(geo_buf + GEOMETRY_DESC_HPB_DEVICE_MAX_ACTIVE_REGIONS);

	INFO_MSG("[48] bHPBRegionSize (%u)", hpb_dev_info->rgn_size);
	INFO_MSG("[49] bHPBNumberLU (%u)", hpb_dev_info->num_lu);
	INFO_MSG("[4A] bHPBSubRegionSize (%u)", hpb_dev_info->srgn_size);
	INFO_MSG("[4B:4C] wDeviceMaxActiveHPBRegions (%u)",
		 hpb_device_max_active_rgns);

	if (hpb_dev_info->rgn_size == 0 || hpb_dev_info->srgn_size == 0 ||
	    hpb_device_max_active_rgns == 0) {
		ERR_MSG("HPB NOT normally supported by device");
		ufsshpb_set_state(ufsf, HPB_FAILED);
	}
}

void ufsshpb_get_lu_info(struct ufsf_feature *ufsf, int lun, u8 *unit_buf)
{
	struct ufsf_lu_desc lu_desc;
	struct ufsshpb_lu *hpb;

	lu_desc.lu_enable = unit_buf[UNIT_DESC_PARAM_LU_ENABLE];
	lu_desc.lu_queue_depth = unit_buf[UNIT_DESC_PARAM_LU_Q_DEPTH];
	lu_desc.lu_logblk_size = unit_buf[UNIT_DESC_PARAM_LOGICAL_BLK_SIZE];
	lu_desc.lu_logblk_cnt =
		LI_EN_64(unit_buf + UNIT_DESC_PARAM_LOGICAL_BLK_COUNT);
	lu_desc.lu_max_active_hpb_rgns =
		LI_EN_16(unit_buf + UNIT_DESC_HPB_LU_MAX_ACTIVE_REGIONS);
	lu_desc.lu_hpb_pinned_rgn_startidx =
		LI_EN_16(unit_buf + UNIT_DESC_HPB_LU_PIN_REGION_START_OFFSET);
	lu_desc.lu_num_hpb_pinned_rgns =
		LI_EN_16(unit_buf + UNIT_DESC_HPB_LU_NUM_PIN_REGIONS);

	if (lu_desc.lu_num_hpb_pinned_rgns > 0) {
		lu_desc.lu_hpb_pinned_end_offset =
			lu_desc.lu_hpb_pinned_rgn_startidx +
			lu_desc.lu_num_hpb_pinned_rgns - 1;
	} else
		lu_desc.lu_hpb_pinned_end_offset = PINNED_NOT_SET;

	INFO_MSG("LUN(%d) [0A] bLogicalBlockSize (%d)",
		 lun, lu_desc.lu_logblk_size);
	INFO_MSG("LUN(%d) [0B] qLogicalBlockCount (%llu)",
		 lun, lu_desc.lu_logblk_cnt);
	INFO_MSG("LUN(%d) [03] bLuEnable (%d)", lun, lu_desc.lu_enable);
	INFO_MSG("LUN(%d) [06] bLuQueueDepth (%d)", lun, lu_desc.lu_queue_depth);
	INFO_MSG("LUN(%d) [23:24] wLUMaxActiveHPBRegions (%d)",
		 lun, lu_desc.lu_max_active_hpb_rgns);
	INFO_MSG("LUN(%d) [25:26] wHPBPinnedRegionStartIdx (%d)",
		 lun, lu_desc.lu_hpb_pinned_rgn_startidx);
	INFO_MSG("LUN(%d) [27:28] wNumHPBPinnedRegions (%d)",
		 lun, lu_desc.lu_num_hpb_pinned_rgns);
	INFO_MSG("LUN(%d) PINNED Start (%d) End (%d)",
		 lun, lu_desc.lu_hpb_pinned_rgn_startidx,
		 lu_desc.lu_hpb_pinned_end_offset);

	ufsf->hpb_lup[lun] = NULL;

	if (lu_desc.lu_enable == 0x02 && lu_desc.lu_max_active_hpb_rgns) {
		ufsf->hpb_lup[lun] = kzalloc(sizeof(struct ufsshpb_lu),
					     GFP_KERNEL);
		if (!ufsf->hpb_lup[lun]) {
			ERR_MSG("hpb_lup[%d] alloc failed", lun);
			return;
		}

		hpb = ufsf->hpb_lup[lun];
		hpb->ufsf = ufsf;
		hpb->lun = lun;
		hpb->lu_num_blocks = lu_desc.lu_logblk_cnt;
		hpb->lu_max_active_rgns = lu_desc.lu_max_active_hpb_rgns;
		hpb->lru_info.max_lru_active_cnt =
			lu_desc.lu_max_active_hpb_rgns -
			lu_desc.lu_num_hpb_pinned_rgns;
		hpb->lu_pinned_rgn_startidx =
			lu_desc.lu_hpb_pinned_rgn_startidx;
		hpb->lu_pinned_end_offset = lu_desc.lu_hpb_pinned_end_offset;
	} else {
		INFO_MSG("===== LU (%d) is hpb-disabled.", lun);
	}
}

static void ufsshpb_error_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(work, struct ufsf_feature, hpb_eh_work);

	WARN_MSG("driver has failed. but UFSHCD can run without UFSSHPB");
	WARN_MSG("UFSSHPB will be removed from the kernel");

	ufsshpb_remove(ufsf, HPB_FAILED);
}

static int ufsshpb_init(struct ufsf_feature *ufsf)
{
	int lun, ret;
	struct ufsshpb_lu *hpb;
	int hpb_enabled_lun = 0;

	seq_scan_lu(lun) {
		if (!ufsf->hpb_lup[lun])
			continue;

		/*
		 * HPB need info about request queue in order to issue
		 * RB-CMD for pinned region.
		 */
		if (!ufsf->sdev_ufs_lu[lun]) {
			WARN_MSG("lun (%d) don't have scsi_device", lun);
			continue;
		}

		ret = ufsshpb_lu_hpb_init(ufsf, lun);
		if (ret) {
			kfree(ufsf->hpb_lup[lun]);
			continue;
		}
		hpb_enabled_lun++;
	}

	if (hpb_enabled_lun == 0) {
		ERR_MSG("No UFSSHPB LU to init");
		ret = -ENODEV;
		goto hpb_failed;
	}

	INIT_WORK(&ufsf->hpb_eh_work, ufsshpb_error_handler);

	kref_init(&ufsf->hpb_kref);
	ufsshpb_set_state(ufsf, HPB_PRESENT);

	seq_scan_lu(lun)
		if (ufsf->hpb_lup[lun]) {
			hpb = ufsf->hpb_lup[lun];

			INFO_MSG("UFSSHPB LU %d working", lun);
			if (hpb->lu_pinned_end_offset != PINNED_NOT_SET)
				schedule_work(&hpb->pinned_work);
		}

	return 0;
hpb_failed:
	ufsshpb_set_state(ufsf, HPB_FAILED);
	return ret;
}

static void ufsshpb_drop_retry_list(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_req *map_req, *next;
	struct request *req;
	unsigned long flags;
	LIST_HEAD(lh_reqs);

	if (list_empty(&hpb->lh_map_req_retry))
		return;

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	list_for_each_entry_safe(map_req, next, &hpb->lh_map_req_retry,
				 list_req) {
		INFO_MSG("drop map_req %p ( %d - %d )", map_req,
			 map_req->rb.rgn_idx, map_req->rb.srgn_idx);

		list_del_init(&map_req->list_req);

		/*
		 * blk_put_request() will grap the queue_lock.
		 * so requests add to a list.
		 * lock seq : queue_lock -> hpb_lock.
		 */
		list_add_tail(&map_req->req->queuelist, &lh_reqs);

		bio_put(map_req->bio);

		ufsshpb_put_map_req(hpb, map_req);
	}
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);

	list_for_each_entry(req, &lh_reqs, queuelist)
		blk_put_request(req);
}

static void ufsshpb_drop_rsp_lists(struct ufsshpb_lu *hpb)
{
	struct ufsshpb_region *rgn, *next_rgn;
	struct ufsshpb_subregion *srgn, *next_srgn;
	unsigned long flags;

	spin_lock_irqsave(&hpb->rsp_list_lock, flags);
	list_for_each_entry_safe(rgn, next_rgn, &hpb->lh_inact_rgn,
				 list_inact_rgn) {
		list_del_init(&rgn->list_inact_rgn);
	}

	list_for_each_entry_safe(srgn, next_srgn, &hpb->lh_act_srgn,
				 list_act_srgn) {
		list_del_init(&srgn->list_act_srgn);
	}
	spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
}

static void ufsshpb_destroy_subregion_tbl(struct ufsshpb_lu *hpb,
					 struct ufsshpb_region *rgn)
{
	int srgn_idx;

	for (srgn_idx = 0; srgn_idx < rgn->srgn_cnt; srgn_idx++) {
		struct ufsshpb_subregion *srgn;

		srgn = rgn->srgn_tbl + srgn_idx;
		srgn->srgn_state = HPB_SRGN_UNUSED;

		ufsshpb_put_map_ctx(hpb, srgn->mctx);
	}
}

static void ufsshpb_destroy_region_tbl(struct ufsshpb_lu *hpb)
{
	int rgn_idx;

	INFO_MSG("Start");

	for (rgn_idx = 0; rgn_idx < hpb->rgns_per_lu; rgn_idx++) {
		struct ufsshpb_region *rgn;

		rgn = hpb->rgn_tbl + rgn_idx;
		if (rgn->rgn_state != HPB_RGN_INACTIVE) {
			rgn->rgn_state = HPB_RGN_INACTIVE;

			ufsshpb_destroy_subregion_tbl(hpb, rgn);
		}

		kfree(rgn->srgn_tbl);
	}

	ufsshpb_table_mempool_remove(hpb);
	vfree(hpb->rgn_tbl);

	INFO_MSG("End");
}

void ufsshpb_remove(struct ufsf_feature *ufsf, int state)
{
	struct ufsshpb_lu *hpb;
	int lun;

	INFO_MSG("start release");
	ufsshpb_set_state(ufsf, HPB_FAILED);

	INFO_MSG("kref count (%d)",
		 atomic_read(&ufsf->hpb_kref.refcount.refs));

	seq_scan_lu(lun) {
		hpb = ufsf->hpb_lup[lun];

		INFO_MSG("lun (%d) (%p)", lun, hpb);

		ufsf->hpb_lup[lun] = NULL;

		if (!hpb)
			continue;

		ufsshpb_cancel_jobs(hpb);

		ufsshpb_destroy_region_tbl(hpb);
		if (hpb->alloc_mctx != 0)
			WARN_MSG("warning: alloc_mctx (%d)", hpb->alloc_mctx);

		ufsshpb_map_req_mempool_remove(hpb);

		ufsshpb_pre_req_mempool_remove(hpb);

		ufsshpb_remove_sysfs(hpb);

		kfree(hpb);
	}

	ufsshpb_set_state(ufsf, state);

	INFO_MSG("end release");
}

void ufsshpb_reset(struct ufsf_feature *ufsf)
{
	ufsshpb_set_state(ufsf, HPB_PRESENT);
}

void ufsshpb_reset_host(struct ufsf_feature *ufsf)
{
	struct ufsshpb_lu *hpb;
	int lun;

	ufsshpb_set_state(ufsf, HPB_RESET);
	seq_scan_lu(lun) {
		hpb = ufsf->hpb_lup[lun];
		if (hpb) {
			INFO_MSG("UFSSHPB lun %d reset", lun);
			ufsshpb_cancel_jobs(hpb);
			ufsshpb_drop_retry_list(hpb);
			ufsshpb_drop_rsp_lists(hpb);
		}
	}
}

static inline int ufsshpb_probe_lun_done(struct ufsf_feature *ufsf)
{
	return (ufsf->num_lu == ufsf->slave_conf_cnt);
}

void ufsshpb_init_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf;
	int ret = 0;

	ufsf = container_of(work, struct ufsf_feature, hpb_init_work);

	INFO_MSG("probe_done(%d)", ufsshpb_probe_lun_done(ufsf));

	ufsshpb_set_state(ufsf, HPB_WAIT_INIT);

	ret = wait_event_timeout(ufsf->hpb_wait,
				 ufsshpb_probe_lun_done(ufsf),
				 msecs_to_jiffies(10000));
	if (ret == 0)
		ERR_MSG("Probing LU is not fully complete.");

	INFO_MSG("probe_done(%d) ret(%d)", ufsshpb_probe_lun_done(ufsf), ret);
	INFO_MSG("HPB_INIT_START");

	ret = ufsshpb_init(ufsf);
	if (ret)
		ERR_MSG("UFSSHPB driver init failed. (%d)", ret);
}

void ufsshpb_suspend(struct ufsf_feature *ufsf)
{
	struct ufsshpb_lu *hpb;
	int lun;

	seq_scan_lu(lun) {
		hpb = ufsf->hpb_lup[lun];
		if (hpb) {
			INFO_MSG("ufsshpb_lu %d goto suspend", lun);
			INFO_MSG("ufsshpb_lu %d changes suspend state", lun);
			ufsshpb_set_state(ufsf, HPB_SUSPEND);
			ufsshpb_cancel_jobs(hpb);
		}
	}
}

void ufsshpb_resume(struct ufsf_feature *ufsf)
{
	struct ufsshpb_lu *hpb;
	int lun;

	seq_scan_lu(lun) {
		hpb = ufsf->hpb_lup[lun];
		if (hpb) {
			bool do_task_work = false;
			bool do_retry_work = false;

			ufsshpb_set_state(ufsf, HPB_PRESENT);

			do_task_work = !ufsshpb_is_empty_rsp_lists(hpb);
			do_retry_work =
				!list_empty_careful(&hpb->lh_map_req_retry);

			INFO_MSG("ufsshpb_lu %d resume. do_task_work %d retry %d",
				 lun, do_task_work, do_retry_work);

			if (do_task_work)
				schedule_work(&hpb->task_work);
			if (do_retry_work)
				schedule_delayed_work(&hpb->retry_work,
						      msecs_to_jiffies(100));
		}
	}
}

static void ufsshpb_stat_init(struct ufsshpb_lu *hpb)
{
	atomic64_set(&hpb->hit, 0);
	atomic64_set(&hpb->miss, 0);
	atomic64_set(&hpb->rb_noti_cnt, 0);
	atomic64_set(&hpb->rb_active_cnt, 0);
	atomic64_set(&hpb->rb_inactive_cnt, 0);
	atomic64_set(&hpb->dev_inact_hcm_cnt, 0);
	atomic64_set(&hpb->map_req_cnt, 0);
	atomic64_set(&hpb->map_compl_cnt, 0);
	atomic64_set(&hpb->pre_req_cnt, 0);
	atomic64_set(&hpb->set_hcm_req_cnt, 0);
	atomic64_set(&hpb->unset_hcm_req_cnt, 0);
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	ufsshpb_debug_sys_init(hpb);
#endif
}

/* SYSFS functions */
static ssize_t ufsshpb_sysfs_hcm_disable_show(struct ufsshpb_lu *hpb, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "hcm_disable %d\n",
		       hpb->hcm_disable);

	INFO_MSG("hcm_disable %d", hpb->hcm_disable);
	return ret;
}

static ssize_t ufsshpb_sysfs_hcm_disable_store(struct ufsshpb_lu *hpb,
					       const char *buf, size_t cnt)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 1)
		return -EINVAL;

	if (value == 1)
		hpb->hcm_disable = true;
	else if (value == 0)
		hpb->hcm_disable = false;

	INFO_MSG("hcm_disable %d", hpb->hcm_disable);

	return cnt;
}

static ssize_t ufsshpb_sysfs_prep_disable_show(struct ufsshpb_lu *hpb, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "force_hpb_read_disable %d\n",
		       hpb->force_disable);

	INFO_MSG("force_hpb_read_disable %d", hpb->force_disable);
	return ret;
}

static ssize_t ufsshpb_sysfs_prep_disable_store(struct ufsshpb_lu *hpb,
					       const char *buf, size_t cnt)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 1)
		return -EINVAL;

	if (value == 1)
		hpb->force_disable = true;
	else if (value == 0)
		hpb->force_disable = false;

	INFO_MSG("force_hpb_read_disable %d", hpb->force_disable);

	return cnt;
}

static ssize_t ufsshpb_sysfs_map_disable_show(struct ufsshpb_lu *hpb, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "force_map_req_disable %d\n",
		       hpb->force_map_req_disable);

	INFO_MSG("force_map_req_disable %d", hpb->force_map_req_disable);

	return ret;
}

static ssize_t ufsshpb_sysfs_map_disable_store(struct ufsshpb_lu *hpb,
					      const char *buf, size_t cnt)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 1)
		return -EINVAL;

	if (value == 1)
		hpb->force_map_req_disable = true;
	else if (value == 0)
		hpb->force_map_req_disable = false;

	INFO_MSG("force_map_req_disable %d", hpb->force_map_req_disable);

	return cnt;
}

static ssize_t ufsshpb_sysfs_throttle_map_req_show(struct ufsshpb_lu *hpb,
						  char *buf)
{
	int ret;
#if defined(CONFIG_HPB_DEBUG)
	ret = snprintf(buf, PAGE_SIZE, "throttle_map_req %d (inflight: %d)\n",
		       hpb->throttle_map_req, hpb->num_inflight_map_req);
#else
	ret = snprintf(buf, PAGE_SIZE, "throttle_map_req %d\n",
		       hpb->throttle_map_req);
#endif

	INFO_MSG("throttle_map_req %d", hpb->throttle_map_req);

	return ret;
}

static ssize_t ufsshpb_sysfs_throttle_map_req_store(struct ufsshpb_lu *hpb,
						   const char *buf, size_t cnt)
{
	unsigned long throttle_map_req;

	if (kstrtoul(buf, 0, &throttle_map_req))
		return -EINVAL;

	if (throttle_map_req > hpb->qd)
		return -EINVAL;

	hpb->throttle_map_req = (int)throttle_map_req;

	INFO_MSG("throttle_map_req %d", hpb->throttle_map_req);

	return cnt;
}

static ssize_t ufsshpb_sysfs_throttle_pre_req_show(struct ufsshpb_lu *hpb,
						  char *buf)
{
	int ret;

#if defined(CONFIG_HPB_DEBUG)
	ret = snprintf(buf, PAGE_SIZE, "throttle_pre_req %d (inflight: %d)\n",
		       hpb->throttle_pre_req, hpb->num_inflight_pre_req);
#else
	ret = snprintf(buf, PAGE_SIZE, "throttle_pre_req %d\n",
		       hpb->throttle_pre_req);
#endif

	INFO_MSG("throttle_pre_req %d", hpb->throttle_pre_req);

	return ret;
}

static ssize_t ufsshpb_sysfs_throttle_pre_req_store(struct ufsshpb_lu *hpb,
						   const char *buf, size_t cnt)
{
	unsigned long throttle_pre_req;

	if (kstrtoul(buf, 0, &throttle_pre_req))
		return -EINVAL;

	if (throttle_pre_req > hpb->qd)
		return -EINVAL;

	hpb->throttle_pre_req = (int)throttle_pre_req;

	INFO_MSG("throttle_pre_req %d", hpb->throttle_pre_req);

	return cnt;
}

static ssize_t ufsshpb_sysfs_throttle_hcm_req_show(struct ufsshpb_lu *hpb, char *buf)
{
	int ret;

#if defined(CONFIG_HPB_DEBUG)
	ret = snprintf(buf, PAGE_SIZE, "throttle_hcm_req %d (inflight: %d)\n",
		       hpb->throttle_hcm_req, hpb->num_inflight_hcm_req);
#else
	ret = snprintf(buf, PAGE_SIZE, "throttle_hcm_req %d\n",
		       hpb->throttle_hcm_req);
#endif

	INFO_MSG("%s", buf);

	return ret;
}

static ssize_t ufsshpb_sysfs_throttle_hcm_req_store(struct ufsshpb_lu *hpb,
						   const char *buf, size_t cnt)
{
	unsigned long throttle_hcm_req;

	if (kstrtoul(buf, 0, &throttle_hcm_req))
		return -EINVAL;

	if (throttle_hcm_req > hpb->qd)
		return -EINVAL;

	if (throttle_hcm_req > 0 && throttle_hcm_req <= 32)
		hpb->throttle_hcm_req = (int)throttle_hcm_req;
	else
		INFO_MSG("value is wrong. throttle_hcm_req must be in [1~32] ");

	INFO_MSG("throttle_hcm_req %d", hpb->throttle_hcm_req);

	return cnt;
}

static ssize_t ufsshpb_sysfs_pre_req_min_tr_len_show(struct ufsshpb_lu *hpb,
						    char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%d", hpb->pre_req_min_tr_len);
	INFO_MSG("pre_req min transfer len %d", hpb->pre_req_min_tr_len);

	return ret;
}

static ssize_t ufsshpb_sysfs_pre_req_min_tr_len_store(struct ufsshpb_lu *hpb,
						     const char *buf,
						     size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;

	if (hpb->pre_req_max_tr_len < val || val < HPB_MULTI_CHUNK_LOW)
		INFO_MSG("value is wrong. pre_req transfer len %d ~ %d",
			 HPB_MULTI_CHUNK_LOW, hpb->pre_req_max_tr_len);
	else
		hpb->pre_req_min_tr_len = val;

	INFO_MSG("pre_req min transfer len %d", hpb->pre_req_min_tr_len);

	return count;
}

static ssize_t ufsshpb_sysfs_pre_req_max_tr_len_show(struct ufsshpb_lu *hpb,
						    char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%d", hpb->pre_req_max_tr_len);
	INFO_MSG("pre_req max transfer len %d", hpb->pre_req_max_tr_len);

	return ret;
}

static ssize_t ufsshpb_sysfs_pre_req_max_tr_len_store(struct ufsshpb_lu *hpb,
						     const char *buf,
						     size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (hpb->pre_req_min_tr_len > val || val > HPB_MULTI_CHUNK_HIGH)
		INFO_MSG("value is wrong. pre_req transfer len %d ~ %d",
			 hpb->pre_req_min_tr_len, HPB_MULTI_CHUNK_HIGH);
	else
		hpb->pre_req_max_tr_len = val;

	INFO_MSG("pre_req max transfer len %d", hpb->pre_req_max_tr_len);

	return count;
}

static ssize_t ufsshpb_sysfs_pre_req_timeout_ms_show(struct ufsshpb_lu *hpb,
						    char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", hpb->requeue_timeout_ms);
	INFO_MSG("pre_req requeue timeout %u", hpb->requeue_timeout_ms);

	return ret;
}

static ssize_t ufsshpb_sysfs_pre_req_timeout_ms_store(struct ufsshpb_lu *hpb,
						     const char *buf,
						     size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	hpb->requeue_timeout_ms = val;

	return count;
}

#if defined(CONFIG_HPB_DEBUG)
static ssize_t ufsshpb_sysfs_debug_show(struct ufsshpb_lu *hpb, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "debug %d\n", hpb->debug);

	INFO_MSG("debug %d", hpb->debug);

	return ret;
}

static ssize_t ufsshpb_sysfs_debug_store(struct ufsshpb_lu *hpb,
					const char *buf, size_t cnt)
{
	unsigned long debug;

	if (kstrtoul(buf, 0, &debug))
		return -EINVAL;

	if (debug > 1)
		return -EINVAL;

	if (debug == 1)
		hpb->debug = 1;
	else
		hpb->debug = 0;

	INFO_MSG("debug %d", hpb->debug);

	return cnt;
}
#endif

#if defined(CONFIG_HPB_DEBUG_SYSFS)
static ssize_t ufsshpb_sysfs_lba_info_show(struct ufsshpb_lu *hpb, char *buf)
{
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	enum HPB_RGN_STATE rgn_state;
	enum HPB_SRGN_STATE srgn_state;
	unsigned long lpn, flags;
	int rgn_idx, srgn_idx, srgn_offset;
	int ppn_clean_offset, ppn_clean_pinned_cnt = 0, ppn_clean_active_cnt = 0;
	unsigned long long pinned_valid_size = 0, active_valid_size = 0;
	int ret;

	lpn = hpb->lba_rgns_info / SECTORS_PER_BLOCK;

	ufsshpb_get_pos_from_lpn(hpb, lpn, &rgn_idx, &srgn_idx, &srgn_offset);

	rgn = hpb->rgn_tbl + rgn_idx;

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	rgn_state = rgn->rgn_state;
	for (srgn_idx = 0; srgn_idx < hpb->srgns_per_rgn; srgn_idx++) {
		srgn = rgn->srgn_tbl + srgn_idx;
		srgn_state = srgn->srgn_state;

		if (!ufsshpb_valid_srgn(rgn, srgn))
			continue;

		if (!srgn->mctx || !srgn->mctx->ppn_dirty)
			continue;

		ppn_clean_offset = 0;
		while (ppn_clean_offset < hpb->entries_per_srgn) {
			ppn_clean_offset =
				find_next_zero_bit((unsigned long *)srgn->mctx->ppn_dirty,
						   hpb->entries_per_srgn,
						   ppn_clean_offset);

			if (ppn_clean_offset >= hpb->entries_per_srgn)
				break;

			if (rgn_state == HPB_RGN_PINNED ||
			    rgn_state == HPB_RGN_HCM)
				ppn_clean_pinned_cnt++;
			else if (rgn_state == HPB_RGN_ACTIVE)
				ppn_clean_active_cnt++;

			ppn_clean_offset++;
		}
	}
	pinned_valid_size = ppn_clean_pinned_cnt * BLOCK_KB;
	active_valid_size = ppn_clean_active_cnt * BLOCK_KB;

	ret = snprintf(buf, PAGE_SIZE,
		       "[LBA] %d [Region State] %d (0-INACTIVE, 1-ACTIVE, 2-PINNED, 3-HCM)\n"
		       "[Pinned] hit_count(low) %lld hit_count(high) %lld miss_count(low) %lld miss_count(high) %lld\n"
		       "[Active] hit_count(low) %lld hit_count(high) %lld miss_count(low) %lld miss_count(high) %lld\n"
		       "[Inactive] read_count(low) %lld read_count(high) %lld\n"
		       "[Pinned_rb_cnt] %lld [Active_rb_cnt] %lld\n"
		       "[Current_pinned_valid_size] %llu [Current_active_valid_size] %llu\n",
		       hpb->lba_rgns_info, rgn_state,
		       (long long)atomic64_read(&rgn->rgn_pinned_low_hit),
		       (long long)atomic64_read(&rgn->rgn_pinned_high_hit),
		       (long long)atomic64_read(&rgn->rgn_pinned_low_miss),
		       (long long)atomic64_read(&rgn->rgn_pinned_high_miss),
		       (long long)atomic64_read(&rgn->rgn_active_low_hit),
		       (long long)atomic64_read(&rgn->rgn_active_high_hit),
		       (long long)atomic64_read(&rgn->rgn_active_low_miss),
		       (long long)atomic64_read(&rgn->rgn_active_high_miss),
		       (long long)atomic64_read(&rgn->rgn_inactive_low_read),
		       (long long)atomic64_read(&rgn->rgn_inactive_high_read),
		       (long long)atomic64_read(&rgn->rgn_pinned_rb_cnt),
		       (long long)atomic64_read(&rgn->rgn_active_rb_cnt),
		       pinned_valid_size, active_valid_size);

	INFO_MSG("%s", buf);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	return ret;
}

static ssize_t ufsshpb_sysfs_lba_info_store(struct ufsshpb_lu *hpb,
					   const char *buf, size_t cnt)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value)) {
		ERR_MSG("kstrtoul error");
		return -EINVAL;
	}

	if (value > hpb->lu_num_blocks * SECTORS_PER_BLOCK) {
		ERR_MSG("value %lu > lu_num_blocks %d error",
			value, hpb->lu_num_blocks);
		return -EINVAL;
	}

	hpb->lba_rgns_info = (int)value;

	INFO_MSG("lba_info %d", hpb->lba_rgns_info);

	return cnt;
}

static ssize_t ufsshpb_sysfs_active_size_show(struct ufsshpb_lu *hpb, char *buf)
{
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	enum HPB_RGN_STATE rgn_state;
	enum HPB_SRGN_STATE srgn_state;
	int ret, rgn_idx, srgn_idx;
	int ppn_clean_offset, ppn_clean_pinned_cnt = 0, ppn_clean_active_cnt = 0;
	unsigned long flags;
	unsigned long long pinned_valid_size = 0, active_valid_size = 0;

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	for (rgn_idx = 0; rgn_idx < hpb->rgns_per_lu; rgn_idx++) {
		rgn = hpb->rgn_tbl + rgn_idx;
		rgn_state = rgn->rgn_state;

		for (srgn_idx = 0; srgn_idx < hpb->srgns_per_rgn; srgn_idx++) {
			srgn = rgn->srgn_tbl + srgn_idx;
			srgn_state = srgn->srgn_state;

			if (!ufsshpb_valid_srgn(rgn, srgn))
				continue;

			if (!srgn->mctx || !srgn->mctx->ppn_dirty)
				continue;

			ppn_clean_offset = 0;
			while (ppn_clean_offset < hpb->entries_per_srgn) {
				ppn_clean_offset =
					find_next_zero_bit((unsigned long *)srgn->mctx->ppn_dirty,
							   hpb->entries_per_srgn,
							   ppn_clean_offset);

				if (ppn_clean_offset >= hpb->entries_per_srgn)
					break;

				if (rgn_state == HPB_RGN_PINNED ||
				    rgn_state == HPB_RGN_HCM)
					ppn_clean_pinned_cnt++;
				else if (rgn_state == HPB_RGN_ACTIVE)
					ppn_clean_active_cnt++;

				ppn_clean_offset++;
			}
		}
	}
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	pinned_valid_size = ppn_clean_pinned_cnt * BLOCK_KB;
	active_valid_size = ppn_clean_active_cnt * BLOCK_KB;

	ret = snprintf(buf, PAGE_SIZE,
		       "[Pinned] current_valid_size %llu [Active] current_valid_size %llu\n",
		       pinned_valid_size, active_valid_size);

	INFO_MSG("%s", buf);

	return ret;
}

static ssize_t ufsshpb_sysfs_hpb_normal_read_count_show(struct ufsshpb_lu *hpb,
						       char *buf)
{
	long long pinned_low_hit_cnt, pinned_high_hit_cnt;
	long long pinned_low_miss_cnt, pinned_high_miss_cnt;
	long long active_low_hit_cnt, active_high_hit_cnt;
	long long active_low_miss_cnt, active_high_miss_cnt;
	long long inactive_low_read_cnt, inactive_high_read_cnt;
	int ret;

	pinned_low_hit_cnt = atomic64_read(&hpb->pinned_low_hit);
	pinned_high_hit_cnt = atomic64_read(&hpb->pinned_high_hit);
	pinned_low_miss_cnt = atomic64_read(&hpb->pinned_low_miss);
	pinned_high_miss_cnt = atomic64_read(&hpb->pinned_high_miss);
	active_low_hit_cnt = atomic64_read(&hpb->active_low_hit);
	active_high_hit_cnt = atomic64_read(&hpb->active_high_hit);
	active_low_miss_cnt = atomic64_read(&hpb->active_low_miss);
	active_high_miss_cnt = atomic64_read(&hpb->active_high_miss);
	inactive_low_read_cnt = atomic64_read(&hpb->inactive_low_read);
	inactive_high_read_cnt = atomic64_read(&hpb->inactive_high_read);

	ret = snprintf(buf, PAGE_SIZE,
		       "[Pinned] hit_count(low) %lld hit_count(high) %lld miss_count(low) %lld miss_count(high) %lld\n"
		       "[Active] hit_count(low) %lld hit_count(high) %lld miss_count(low) %lld miss_count(high) %lld\n"
		       "[Inactive] read_count(low) %lld read_count(high) %lld\n",
		       pinned_low_hit_cnt, pinned_high_hit_cnt, pinned_low_miss_cnt, pinned_high_miss_cnt,
		       active_low_hit_cnt, active_high_hit_cnt, active_low_miss_cnt, active_high_miss_cnt,
		       inactive_low_read_cnt, inactive_high_read_cnt);

	INFO_MSG("%s", buf);

	return ret;
}

static ssize_t ufsshpb_sysfs_rb_count_show(struct ufsshpb_lu *hpb, char *buf)
{
	long long pinned_rb_cnt, active_rb_cnt;
	int ret;

	pinned_rb_cnt = atomic64_read(&hpb->pinned_rb_cnt);
	active_rb_cnt = atomic64_read(&hpb->active_rb_cnt);

	ret = snprintf(buf, PAGE_SIZE,
		       "[Pinned] rb_count %lld [Active] rb_count %lld\n",
		       pinned_rb_cnt, active_rb_cnt);

	INFO_MSG("%s", buf);

	return ret;
}

static ssize_t ufsshpb_sysfs_rsp_list_cnt_show(struct ufsshpb_lu *hpb, char *buf)
{
	long long act_cnt, inact_cnt;
	int ret;
#if defined(CONFIG_HPB_DEBUG)
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	unsigned long flags;
	int cnt = 0;
#endif

	act_cnt = atomic64_read(&hpb->act_rsp_list_cnt);
	inact_cnt = atomic64_read(&hpb->inact_rsp_list_cnt);

	ret = snprintf(buf, PAGE_SIZE,
		       "act_rsp_list_cnt %lld inact_rsp_list_cnt %lld\n",
		       act_cnt, inact_cnt);

	INFO_MSG("%s", buf);

	if (!hpb->debug)
		return ret;

	spin_lock_irqsave(&hpb->rsp_list_lock, flags);
	if (act_cnt) {
		ret += snprintf(buf+ret, PAGE_SIZE - ret, "act_rsp_list:\n");
		cnt = 0;
		list_for_each_entry(srgn, &hpb->lh_act_srgn, list_act_srgn) {
			cnt++;
			if (cnt % 10 == 1)
				ret += snprintf(buf+ret, PAGE_SIZE - ret, "\t");
			ret += snprintf(buf+ret, PAGE_SIZE - ret, "%05ds%d ",
					srgn->rgn_idx, hpb->rgn_tbl[srgn->rgn_idx].rgn_state);
			if (cnt % 10 == 0)
				ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");

			if (ret > PAGE_SIZE) {
				ret = PAGE_SIZE - 10;
				ret += snprintf(buf+ret, PAGE_SIZE - ret,
						"... ");
				if (cnt % 10 == 0)
					ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");
				break;
			}
		}
		if (cnt % 10 != 0)
			ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");
	}
	if (inact_cnt) {
		ret += snprintf(buf+ret, PAGE_SIZE - ret, "inact_rsp_list:\n");
		cnt = 0;
		list_for_each_entry(rgn, &hpb->lh_inact_rgn, list_inact_rgn) {
			cnt++;
			if (cnt % 10 == 1)
				ret += snprintf(buf+ret, PAGE_SIZE - ret, "\t");
			ret += snprintf(buf+ret, PAGE_SIZE - ret, "%05ds%d ",
					rgn->rgn_idx, rgn->rgn_state);
			if (cnt % 10 == 0)
				ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");

			if (ret > PAGE_SIZE) {
				ret = PAGE_SIZE - 10;
				ret += snprintf(buf+ret, PAGE_SIZE - ret,
						"... ");
				if (cnt % 10 == 0)
					ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");
				break;
			}
		}
		if (cnt % 10 != 0)
			ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");
	}
	spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

	return ret;
}
#endif

#if defined(CONFIG_HPB_ERR_INJECTION)
static ssize_t ufsshpb_sysfs_err_injection_select_show(struct ufsshpb_lu *hpb,
						      char *buf)
{
	char error_injection[8];
	int ret;

	if (hpb->err_injection_select == HPB_ERR_INJECTION_DISABLE)
		strlcpy(error_injection, "disable", 8);
	else if (hpb->err_injection_select == HPB_ERR_INJECTION_BITFLIP)
		strlcpy(error_injection, "bitflip", 8);
	else if (hpb->err_injection_select == HPB_ERR_INJECTION_OFFSET)
		strlcpy(error_injection, "offset", 7);
	else if (hpb->err_injection_select == HPB_ERR_INJECTION_RANDOM)
		strlcpy(error_injection, "random", 7);

	ret = snprintf(buf, PAGE_SIZE, "err_injection_select %s\n",
		       error_injection);

	INFO_MSG("err_injection_select %s", error_injection);

	return ret;
}

static ssize_t ufsshpb_sysfs_err_injection_select_store(struct ufsshpb_lu *hpb,
						       const char *buf,
						       size_t cnt)
{
	char error_injection[8];
	int ret;

	error_injection[7] = '\0';

	ret = sscanf(buf, "%7s", error_injection);
	if (!ret) {
		INFO_MSG("input failed...");
		return cnt;
	}

	if (!strncmp(error_injection, "disable", 7))
		hpb->err_injection_select = HPB_ERR_INJECTION_DISABLE;
	else if (!strncmp(error_injection, "bitflip", 7))
		hpb->err_injection_select = HPB_ERR_INJECTION_BITFLIP;
	else if (!strncmp(error_injection, "offset", 6))
		hpb->err_injection_select = HPB_ERR_INJECTION_OFFSET;
	else if (!strncmp(error_injection, "random", 6))
		hpb->err_injection_select = HPB_ERR_INJECTION_RANDOM;
	else {
		hpb->err_injection_select = HPB_ERR_INJECTION_DISABLE;
		strlcpy(error_injection, "disable", 8);
	}

	INFO_MSG("err_injection_select %s", error_injection);

	return cnt;
}

static ssize_t ufsshpb_sysfs_err_injection_bitflip_show(struct ufsshpb_lu *hpb,
						       char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "err_injection_bitflip %llx\n",
		       hpb->err_injection_bitflip);

	return ret;
}

static ssize_t ufsshpb_sysfs_err_injection_bitflip_store(struct ufsshpb_lu *hpb,
							const char *buf,
							size_t cnt)
{
	unsigned long long err_injection_bitflip;

	if (kstrtoull(buf, 0, &err_injection_bitflip))
		return -EINVAL;

	hpb->err_injection_bitflip = err_injection_bitflip;

	INFO_MSG("err_injection_bitflip %llx", hpb->err_injection_bitflip);

	return cnt;
}

static ssize_t ufsshpb_sysfs_err_injection_offset_show(struct ufsshpb_lu *hpb,
						      char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "err_injection_offset %lld\n",
		       hpb->err_injection_offset);

	INFO_MSG("err_injection_offset %lld", hpb->err_injection_offset);

	return ret;
}

static ssize_t ufsshpb_sysfs_err_injection_offset_store(struct ufsshpb_lu *hpb,
						       const char *buf,
						       size_t cnt)
{
	unsigned long long err_injection_offset;

	if (kstrtoull(buf, 0, &err_injection_offset))
		return -EINVAL;

	hpb->err_injection_offset = err_injection_offset;

	INFO_MSG("err_injection_offset %lld", hpb->err_injection_offset);

	return cnt;
}

static ssize_t ufsshpb_sysfs_err_injection_random_show(struct ufsshpb_lu *hpb,
						      char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "err_injection_random %d\n",
		       hpb->err_injection_random);

	INFO_MSG("err_injection_random %d", hpb->err_injection_random);

	return ret;
}

static ssize_t ufsshpb_sysfs_err_injection_random_store(struct ufsshpb_lu *hpb,
						       const char *buf,
						       size_t cnt)
{
	unsigned long err_injection_random;

	if (kstrtoul(buf, 0, &err_injection_random))
		return -EINVAL;


	if (err_injection_random > 1)
		return -EINVAL;

	hpb->err_injection_random = err_injection_random;

	INFO_MSG("err_injection_random %d", hpb->err_injection_random);

	return cnt;
}
#endif

static ssize_t ufsshpb_sysfs_version_show(struct ufsshpb_lu *hpb, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "HPB version %.4X D/D version %.6X%s\n",
		       hpb->ufsf->hpb_dev_info.version,
		       UFSSHPB_DD_VER, UFSSHPB_DD_VER_POST);

	INFO_MSG("HPB version %.4X D/D version %.6X%s",
		 hpb->ufsf->hpb_dev_info.version,
		 UFSSHPB_DD_VER, UFSSHPB_DD_VER_POST);

	return ret;
}

static ssize_t ufsshpb_sysfs_hit_show(struct ufsshpb_lu *hpb, char *buf)
{
	long long hit_cnt;
	int ret;

	hit_cnt = atomic64_read(&hpb->hit);

	ret = snprintf(buf, PAGE_SIZE, "hit_count %lld\n", hit_cnt);

	INFO_MSG("hit_count %lld", hit_cnt);

	return ret;
}

static ssize_t ufsshpb_sysfs_miss_show(struct ufsshpb_lu *hpb, char *buf)
{
	long long miss_cnt;
	int ret;

	miss_cnt = atomic64_read(&hpb->miss);

	ret = snprintf(buf, PAGE_SIZE, "miss_count %lld\n", miss_cnt);

	INFO_MSG("miss_count %lld", miss_cnt);

	return ret;
}

static ssize_t ufsshpb_sysfs_map_req_show(struct ufsshpb_lu *hpb, char *buf)
{
	long long rb_noti_cnt, rb_active_cnt, rb_inactive_cnt, map_req_cnt;
	long long map_compl_cnt, dev_inact_hcm_cnt;
	int ret;

	rb_noti_cnt = atomic64_read(&hpb->rb_noti_cnt);
	rb_active_cnt = atomic64_read(&hpb->rb_active_cnt);
	rb_inactive_cnt = atomic64_read(&hpb->rb_inactive_cnt);
	map_req_cnt = atomic64_read(&hpb->map_req_cnt);
	map_compl_cnt = atomic64_read(&hpb->map_compl_cnt);
	dev_inact_hcm_cnt = atomic64_read(&hpb->dev_inact_hcm_cnt);

	ret = snprintf(buf, PAGE_SIZE,
		       "rb_noti %lld ACT %lld INACT %lld (hcm: %lld ) map_req_count %lld map_compl_count %lld\n",
		       rb_noti_cnt, rb_active_cnt, rb_inactive_cnt, dev_inact_hcm_cnt,
		       map_req_cnt, map_compl_cnt);

	INFO_MSG("rb_noti %lld ACT %lld INACT %lld map_req_count %lld",
		 rb_noti_cnt, rb_active_cnt, rb_inactive_cnt,
		 map_req_cnt);

	return ret;
}

static ssize_t ufsshpb_sysfs_pre_req_show(struct ufsshpb_lu *hpb, char *buf)
{
	long long pre_req_cnt;
	int ret;

	pre_req_cnt = atomic64_read(&hpb->pre_req_cnt);

	ret = snprintf(buf, PAGE_SIZE, "pre_req_count %lld\n", pre_req_cnt);

	INFO_MSG("pre_req_count %lld", pre_req_cnt);

	return ret;
}

static ssize_t ufsshpb_sysfs_hcm_req_show(struct ufsshpb_lu *hpb, char *buf)
{
	long long set_hcm_cnt, unset_hcm_cnt;
	int ret;

	set_hcm_cnt = atomic64_read(&hpb->set_hcm_req_cnt);
	unset_hcm_cnt = atomic64_read(&hpb->unset_hcm_req_cnt);

#if defined(CONFIG_HPB_DEBUG_SYSFS)
	ret = snprintf(buf, PAGE_SIZE,
		       "set_hcm_count %lld %lld unset_hcm_count %lld %lld v %lld\n",
		       set_hcm_cnt, atomic64_read(&hpb->fs_set_hcm_cnt),
		       unset_hcm_cnt, atomic64_read(&hpb->fs_unset_hcm_cnt),
		       atomic64_read(&hpb->unset_hcm_victim));
#else
	ret = snprintf(buf, PAGE_SIZE,
		       "set_hcm_count %lld unset_hcm_count %lld\n",
		       set_hcm_cnt, unset_hcm_cnt);
#endif

	INFO_MSG("set_hcm_count %lld unset_hcm_count %lld",
		 set_hcm_cnt, unset_hcm_cnt);

	return ret;
}

static ssize_t ufsshpb_sysfs_region_stat_show(struct ufsshpb_lu *hpb, char *buf)
{
	int ret = 0;
	int pin_cnt = 0, hcm_cnt = 0, act_cnt = 0, inact_cnt = 0;
	int rgn_idx;
	enum HPB_RGN_STATE state;
	unsigned long flags;

	for (rgn_idx = 0; rgn_idx < hpb->rgns_per_lu; rgn_idx++) {
		state = hpb->rgn_tbl[rgn_idx].rgn_state;
		if (state == HPB_RGN_PINNED)
			pin_cnt++;
		else if (state == HPB_RGN_HCM)
			hcm_cnt++;
		else if (state == HPB_RGN_ACTIVE)
			act_cnt++;
		else if (state == HPB_RGN_INACTIVE)
			inact_cnt++;
	}

	ret = snprintf(buf, PAGE_SIZE,
		       "Total %d pinned %d hcm %d active %d inactive %d\n",
		       hpb->rgns_per_lu, pin_cnt, hcm_cnt, act_cnt,
		       inact_cnt);

	INFO_MSG("Total %d pinned %d hcm %d active %d inactive %d",
		 hpb->rgns_per_lu, pin_cnt, hcm_cnt, act_cnt, inact_cnt);

	if (!hpb->debug)
		return ret;

	act_cnt = 0, hcm_cnt = 0;
	spin_lock_irqsave(&hpb->hpb_lock, flags);
	for (rgn_idx = 0; rgn_idx < hpb->rgns_per_lu; rgn_idx++) {
		state = hpb->rgn_tbl[rgn_idx].rgn_state;
		if (state != HPB_RGN_ACTIVE)
			continue;

		act_cnt++;
		if (act_cnt == 1)
			ret += snprintf(buf+ret, PAGE_SIZE - ret, "ACT:\n");

		if ((act_cnt % 10) == 1)
			ret += snprintf(buf+ret, PAGE_SIZE - ret, "\t");

		ret += snprintf(buf+ret, PAGE_SIZE - ret, "%05d   ", rgn_idx);

		if ((act_cnt % 10) == 0)
			ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");

		if (ret > PAGE_SIZE) {
			ret = PAGE_SIZE - 10;
			ret += snprintf(buf+ret, PAGE_SIZE - ret, "... ");
			if (hcm_cnt % 10 == 0)
				ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");
			break;
		}
	}
	if (act_cnt % 10)
		ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");

	for (rgn_idx = 0; rgn_idx < hpb->rgns_per_lu; rgn_idx++) {
		state = hpb->rgn_tbl[rgn_idx].rgn_state;
		if (state == HPB_RGN_HCM) {
			hcm_cnt++;
			if (hcm_cnt == 1)
				ret += snprintf(buf+ret, PAGE_SIZE - ret, "HCM:\n");

			if ((hcm_cnt % 10) == 1)
				ret += snprintf(buf+ret, PAGE_SIZE - ret, "\t");

			ret += snprintf(buf+ret, PAGE_SIZE - ret, "%05d   ", rgn_idx);

			if ((hcm_cnt % 10) == 0)
				ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");

			if (ret > PAGE_SIZE) {
				ret = PAGE_SIZE - 10;
				ret += snprintf(buf+ret, PAGE_SIZE - ret, "... ");
				if (hcm_cnt % 10 == 0)
					ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");
				break;
			}
		}
	}

	if (hcm_cnt % 10)
		ret += snprintf(buf+ret, PAGE_SIZE - ret, "\n");

	spin_unlock_irqrestore(&hpb->hpb_lock, flags);

	return ret;
}

static ssize_t ufsshpb_sysfs_count_reset_store(struct ufsshpb_lu *hpb,
					      const char *buf, size_t cnt)
{
	unsigned long debug;

	if (kstrtoul(buf, 0, &debug))
		return -EINVAL;

	if (debug != 1)
		return cnt;

	INFO_MSG("Stat Init");

	ufsshpb_stat_init(hpb);

	return cnt;
}

static ssize_t ufsshpb_sysfs_info_lba_store(struct ufsshpb_lu *hpb,
					   const char *buf, size_t cnt)
{
	struct ufsshpb_region *rgn;
	struct ufsshpb_subregion *srgn;
	u64 ppn = 0;
	unsigned long value, lpn, flags;
	int rgn_idx, srgn_idx, srgn_offset, error = 0;

	if (kstrtoul(buf, 0, &value)) {
		ERR_MSG("kstrtoul error");
		return -EINVAL;
	}

	if (value >= hpb->lu_num_blocks * SECTORS_PER_BLOCK) {
		ERR_MSG("value %lu >= lu_num_blocks %d error",
			value, hpb->lu_num_blocks * SECTORS_PER_BLOCK);
		return -EINVAL;
	}

	lpn = value / SECTORS_PER_BLOCK;

	ufsshpb_get_pos_from_lpn(hpb, lpn, &rgn_idx, &srgn_idx, &srgn_offset);

	rgn = hpb->rgn_tbl + rgn_idx;
	srgn = rgn->srgn_tbl + srgn_idx;

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	INFO_MSG("lba %lu lpn %lu region %d state %d subregion %d state %d",
		 value, lpn, rgn_idx, rgn->rgn_state, srgn_idx,
		 srgn->srgn_state);

	if (!ufsshpb_valid_srgn(rgn, srgn)) {
		INFO_MSG("[region %d subregion %d] has not valid hpb info.",
			 rgn_idx, srgn_idx);
		goto out;
	}

	if (!srgn->mctx) {
		INFO_MSG("mctx is NULL");
		goto out;
	}

	ppn = ufsshpb_get_ppn(srgn->mctx, srgn_offset, &error);
	if (error) {
		INFO_MSG("getting ppn is fail from a page.");
		goto out;
	}

	INFO_MSG("ppn %llx is_dirty %d", ppn,
		 ufsshpb_ppn_dirty_check(hpb, lpn, 1));
out:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	return cnt;
}

static ssize_t ufsshpb_sysfs_info_region_store(struct ufsshpb_lu *hpb,
					      const char *buf, size_t cnt)
{
	unsigned long rgn_idx;
	int srgn_idx;

	if (kstrtoul(buf, 0, &rgn_idx))
		return -EINVAL;

	if (rgn_idx >= hpb->rgns_per_lu)
		ERR_MSG("error region %ld max %d", rgn_idx, hpb->rgns_per_lu);
	else {
		INFO_MSG("(region state : PINNED=%d HCM=%d ACTIVE=%d INACTIVE=%d)",
			 HPB_RGN_PINNED, HPB_RGN_HCM, HPB_RGN_ACTIVE,
			 HPB_RGN_INACTIVE);

		INFO_MSG("region %ld state %d", rgn_idx,
			 hpb->rgn_tbl[rgn_idx].rgn_state);

		for (srgn_idx = 0; srgn_idx < hpb->rgn_tbl[rgn_idx].srgn_cnt;
		     srgn_idx++) {
			INFO_MSG("--- subregion %d state %d", srgn_idx,
				 hpb->rgn_tbl[rgn_idx].srgn_tbl[srgn_idx].srgn_state);
		}
	}

	return cnt;
}

static ssize_t ufsshpb_sysfs_ufsshpb_release_store(struct ufsshpb_lu *hpb,
						 const char *buf, size_t cnt)
{
	unsigned long value;

	INFO_MSG("start release function");

	if (kstrtoul(buf, 0, &value)) {
		ERR_MSG("kstrtoul error");
		return -EINVAL;
	}

	if (value == 0xab) {
		INFO_MSG("magic number %lu release start", value);
		goto err_out;
	} else
		INFO_MSG("wrong magic number %lu", value);

	return cnt;
err_out:
	INFO_MSG("ref_cnt %d",
		 atomic_read(&hpb->ufsf->hpb_kref.refcount.refs));
	ufsshpb_failed(hpb, __func__);

	return cnt;
}

static struct ufsshpb_sysfs_entry ufsshpb_sysfs_entries[] = {
	__ATTR(hcm_disable, 0644,
	       ufsshpb_sysfs_hcm_disable_show, ufsshpb_sysfs_hcm_disable_store),
	__ATTR(hpb_read_disable, 0644,
	       ufsshpb_sysfs_prep_disable_show, ufsshpb_sysfs_prep_disable_store),
	__ATTR(map_cmd_disable, 0644,
	       ufsshpb_sysfs_map_disable_show, ufsshpb_sysfs_map_disable_store),
	__ATTR(throttle_map_req, 0644,
	       ufsshpb_sysfs_throttle_map_req_show,
	       ufsshpb_sysfs_throttle_map_req_store),
	__ATTR(throttle_pre_req, 0644,
	       ufsshpb_sysfs_throttle_pre_req_show,
	       ufsshpb_sysfs_throttle_pre_req_store),
	__ATTR(throttle_hcm_req, 0644,
	       ufsshpb_sysfs_throttle_hcm_req_show,
	       ufsshpb_sysfs_throttle_hcm_req_store),
	__ATTR(pre_req_min_tr_len, 0644,
	       ufsshpb_sysfs_pre_req_min_tr_len_show,
	       ufsshpb_sysfs_pre_req_min_tr_len_store),
	__ATTR(pre_req_max_tr_len, 0644,
	       ufsshpb_sysfs_pre_req_max_tr_len_show,
	       ufsshpb_sysfs_pre_req_max_tr_len_store),
	__ATTR(pre_req_timeout_ms, 0644,
	       ufsshpb_sysfs_pre_req_timeout_ms_show,
	       ufsshpb_sysfs_pre_req_timeout_ms_store),
#if defined(CONFIG_HPB_DEBUG)
	__ATTR(debug, 0644,
	       ufsshpb_sysfs_debug_show, ufsshpb_sysfs_debug_store),
#endif
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	__ATTR(lba_info, 0644,
	       ufsshpb_sysfs_lba_info_show, ufsshpb_sysfs_lba_info_store),
	__ATTR(active_size, 0444, ufsshpb_sysfs_active_size_show, NULL),
	__ATTR(read_count, 0444, ufsshpb_sysfs_hpb_normal_read_count_show, NULL),
	__ATTR(rb_count, 0444, ufsshpb_sysfs_rb_count_show, NULL),
	__ATTR(rsp_list_count, 0444, ufsshpb_sysfs_rsp_list_cnt_show, NULL),
#endif
#if defined(CONFIG_HPB_ERR_INJECTION)
	__ATTR(err_injection_select, 0644,
	       ufsshpb_sysfs_err_injection_select_show,
	       ufsshpb_sysfs_err_injection_select_store),
	__ATTR(err_injection_bitflip, 0644,
	       ufsshpb_sysfs_err_injection_bitflip_show,
	       ufsshpb_sysfs_err_injection_bitflip_store),
	__ATTR(err_injection_offset, 0644,
	       ufsshpb_sysfs_err_injection_offset_show,
	       ufsshpb_sysfs_err_injection_offset_store),
	__ATTR(err_injection_random, 0644,
	       ufsshpb_sysfs_err_injection_random_show,
	       ufsshpb_sysfs_err_injection_random_store),
#endif
	__ATTR(hpb_version, 0444, ufsshpb_sysfs_version_show, NULL),
	__ATTR(hit_count, 0444, ufsshpb_sysfs_hit_show, NULL),
	__ATTR(miss_count, 0444, ufsshpb_sysfs_miss_show, NULL),
	__ATTR(map_req_count, 0444, ufsshpb_sysfs_map_req_show, NULL),
	__ATTR(pre_req_count, 0444, ufsshpb_sysfs_pre_req_show, NULL),
	__ATTR(hcm_req_count, 0444, ufsshpb_sysfs_hcm_req_show, NULL),
	__ATTR(region_stat_count, 0444, ufsshpb_sysfs_region_stat_show, NULL),
	__ATTR(count_reset, 0200, NULL, ufsshpb_sysfs_count_reset_store),
	__ATTR(get_info_from_lba, 0200, NULL, ufsshpb_sysfs_info_lba_store),
	__ATTR(get_info_from_region, 0200, NULL,
	       ufsshpb_sysfs_info_region_store),
	__ATTR(release, 0200, NULL, ufsshpb_sysfs_ufsshpb_release_store),
	__ATTR_NULL
};

static ssize_t ufsshpb_attr_show(struct kobject *kobj, struct attribute *attr,
				char *page)
{
	struct ufsshpb_sysfs_entry *entry;
	struct ufsshpb_lu *hpb;
	ssize_t error;

	entry = container_of(attr, struct ufsshpb_sysfs_entry, attr);
	hpb = container_of(kobj, struct ufsshpb_lu, kobj);

	if (!entry->show)
		return -EIO;

	mutex_lock(&hpb->sysfs_lock);
	error = entry->show(hpb, page);
	mutex_unlock(&hpb->sysfs_lock);
	return error;
}

static ssize_t ufsshpb_attr_store(struct kobject *kobj, struct attribute *attr,
				 const char *page, size_t len)
{
	struct ufsshpb_sysfs_entry *entry;
	struct ufsshpb_lu *hpb;
	ssize_t error;

	entry = container_of(attr, struct ufsshpb_sysfs_entry, attr);
	hpb = container_of(kobj, struct ufsshpb_lu, kobj);

	if (!entry->store)
		return -EIO;

	mutex_lock(&hpb->sysfs_lock);
	error = entry->store(hpb, page, len);
	mutex_unlock(&hpb->sysfs_lock);
	return error;
}

static const struct sysfs_ops ufsshpb_sysfs_ops = {
	.show = ufsshpb_attr_show,
	.store = ufsshpb_attr_store,
};

static struct kobj_type ufsshpb_ktype = {
	.sysfs_ops = &ufsshpb_sysfs_ops,
	.release = NULL,
};

static int ufsshpb_create_sysfs(struct ufsf_feature *ufsf, struct ufsshpb_lu *hpb)
{
	struct device *dev = ufsf->hba->dev;
	struct ufsshpb_sysfs_entry *entry;
	int err;

	hpb->sysfs_entries = ufsshpb_sysfs_entries;

	ufsshpb_stat_init(hpb);
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	atomic64_set(&hpb->act_rsp_list_cnt, 0);
	atomic64_set(&hpb->inact_rsp_list_cnt, 0);
#endif

	kobject_init(&hpb->kobj, &ufsshpb_ktype);
	mutex_init(&hpb->sysfs_lock);

	INFO_MSG("ufsshpb creates sysfs lu %d %p dev->kobj %p", hpb->lun,
		 &hpb->kobj, &dev->kobj);

	err = kobject_add(&hpb->kobj, kobject_get(&dev->kobj),
			  "ufsshpb_lu%d", hpb->lun);
	if (!err) {
		for (entry = hpb->sysfs_entries; entry->attr.name != NULL;
		     entry++) {
			INFO_MSG("ufsshpb_lu%d sysfs attr creates: %s",
				 hpb->lun, entry->attr.name);
			if (sysfs_create_file(&hpb->kobj, &entry->attr))
				break;
		}
		INFO_MSG("ufsshpb_lu%d sysfs adds uevent", hpb->lun);
		kobject_uevent(&hpb->kobj, KOBJ_ADD);
	}

	return err;
}

static inline void ufsshpb_remove_sysfs(struct ufsshpb_lu *hpb)
{
	kobject_uevent(&hpb->kobj, KOBJ_REMOVE);
	INFO_MSG("ufsshpb removes sysfs lu %d %p ", hpb->lun, &hpb->kobj);
	kobject_del(&hpb->kobj);
}

MODULE_LICENSE("GPL v2");
