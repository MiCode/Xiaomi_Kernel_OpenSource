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

#include <linux/slab.h>
#include <linux/blkdev.h>
#include <scsi/scsi.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>

#include "../../../block/blk.h"

#include "ufs.h"
#include "ufshcd.h"
#include "ufshpb.h"

/*
 * UFSHPB DEBUG
 */
#define INFO_MSG(msg, args...)		pr_info("%s:%d " msg "\n", \
						__func__, __LINE__, ##args)
#define INIT_INFO(msg, args...)		INFO_MSG(msg, ##args)
#define RELEASE_INFO(msg, args...)	INFO_MSG(msg, ##args)
#define SYSFS_INFO(msg, args...)	INFO_MSG(msg, ##args)
#define ERR_MSG(msg, args...)		pr_err("%s:%d " msg "\n", \
						__func__, __LINE__, ##args)
#define WARNING_MSG(msg, args...)	pr_warn("%s:%d " msg "\n", \
						__func__, __LINE__, ##args)

#define HPB_DEBUG(hpb, msg, args...)			\
	do { if (hpb->debug)				\
		pr_notice("%s:%d " msg "\n",		\
		       __func__, __LINE__, ##args);	\
	} while (0)

#define TMSG(hpb, args...)						\
	do { if (hpb->hba->sdev_ufs_lu[hpb->lun] &&			\
		 hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue)	\
			blk_add_trace_msg(				\
			hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue,	\
				##args);				\
	} while (0)

/*
 * debug variables
 */
int alloc_mctx;

/*
 * define global constants
 */
static int sects_per_blk_shift;
static int bits_per_dword_shift;
static int bits_per_dword_mask;
static int bits_per_byte_shift;

static int ufshpb_create_sysfs(struct ufs_hba *hba,
		struct ufshpb_lu *hpb);
static void ufshpb_error_handler(struct work_struct *work);
static void ufshpb_evict_region(struct ufshpb_lu *hpb,
				     struct ufshpb_region *cb);

static inline void ufshpb_get_bit_offset(
		struct ufshpb_lu *hpb, int subregion_offset,
		int *dword, int *offset)
{
	*dword = subregion_offset >> bits_per_dword_shift;
	*offset = subregion_offset & bits_per_dword_mask;
}

/* called with hpb_lock (irq) */
static bool ufshpb_ppn_dirty_check(struct ufshpb_lu *hpb,
		struct ufshpb_subregion *cp, int subregion_offset)
{
	bool is_dirty;
	unsigned int bit_dword, bit_offset;

	ufshpb_get_bit_offset(hpb, subregion_offset,
			&bit_dword, &bit_offset);

	if (!cp->mctx->ppn_dirty)
		return false;

	is_dirty = cp->mctx->ppn_dirty[bit_dword] &
		(1 << bit_offset) ? true : false;

	return is_dirty;
}

static void ufshpb_ppn_prep(struct ufshpb_lu *hpb,
		struct ufshcd_lrb *lrbp, unsigned long long ppn)
{
	unsigned char cmd[16] = { 0 };
#ifdef UFSHPB_ERROR_INJECT
	static struct timeval tm_s = { 0 }, tm_e = { 0 };
	int err_bit;

	if (hpb->err_inject) {
		if (tm_s.tv_sec == 0)
			do_gettimeofday(&tm_s);

		do_gettimeofday(&tm_e);

		if ((tm_e.tv_sec - tm_s.tv_sec) >= 5) {
			err_bit = jiffies & 0x3F;

			SYSFS_INFO("hpb err_inject %ld, %ld, err bit:%d\n",
				tm_e.tv_sec, tm_s.tv_sec, err_bit);
			do_gettimeofday(&tm_s);

			ppn ^= (1 << err_bit);
		}
	}
#endif

	cmd[0] = READ_16;
	cmd[2] = lrbp->cmd->cmnd[2];
	cmd[3] = lrbp->cmd->cmnd[3];
	cmd[4] = lrbp->cmd->cmnd[4];
	cmd[5] = lrbp->cmd->cmnd[5];
	cmd[6] = GET_BYTE_7(ppn);
	cmd[7] = GET_BYTE_6(ppn);
	cmd[8] = GET_BYTE_5(ppn);
	cmd[9] = GET_BYTE_4(ppn);
	cmd[10] = GET_BYTE_3(ppn);
	cmd[11] = GET_BYTE_2(ppn);
	cmd[12] = GET_BYTE_1(ppn);
	cmd[13] = GET_BYTE_0(ppn);
	cmd[14] = 0x11; // Group Number
	cmd[15] = 0x01; // Transfer_len = 0x01 (FW defined)

	memcpy(lrbp->cmd->cmnd, cmd, MAX_CDB_SIZE);
	memcpy(lrbp->ucd_req_ptr->sc.cdb, cmd, MAX_CDB_SIZE);
}

/* called with hpb_lock (irq) */
static inline void ufshpb_set_dirty_bits(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb, struct ufshpb_subregion *cp,
		int dword, int offset, unsigned int count)
{
	const unsigned long mask = ((1UL << count) - 1) & 0xffffffff;

	if (cb->region_state == HPBREGION_INACTIVE)
		return;

	WARN_ON(!cp->mctx);
	cp->mctx->ppn_dirty[dword] |= (mask << offset);
}

static void ufshpb_set_dirty(struct ufshpb_lu *hpb,
		struct ufshcd_lrb *lrbp, int region,
		int subregion, int subregion_offset)
{
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;
	int count;
	int bit_count, bit_dword, bit_offset;

	count = blk_rq_sectors(lrbp->cmd->request) >> sects_per_blk_shift;
	ufshpb_get_bit_offset(hpb, subregion_offset,
			&bit_dword, &bit_offset);

	do {
		bit_count = min(count, BITS_PER_DWORD - bit_offset);

		cb = hpb->region_tbl + region;
		cp = cb->subregion_tbl + subregion;

		ufshpb_set_dirty_bits(hpb, cb, cp,
				bit_dword, bit_offset, bit_count);

		bit_offset = 0;
		bit_dword++;

		if (bit_dword == hpb->dwords_per_subregion) {
			bit_dword = 0;
			subregion++;

			if (subregion == hpb->subregions_per_region) {
				subregion = 0;
				region++;
			}
		}

		count -= bit_count;
	} while (count);

	WARN_ON(count < 0);
}

static inline bool ufshpb_is_read_lrbp(struct ufshcd_lrb *lrbp)
{
	if (lrbp->cmd->cmnd[0] == READ_10 || lrbp->cmd->cmnd[0] == READ_16)
		return true;

	return false;
}

static inline bool ufshpb_is_write_discard_lrbp(struct ufshcd_lrb *lrbp)
{
	if (lrbp->cmd->cmnd[0] == WRITE_10 || lrbp->cmd->cmnd[0] == WRITE_16
			|| lrbp->cmd->cmnd[0] == UNMAP)
		return true;

	return false;
}

static inline void ufshpb_get_pos_from_lpn(struct ufshpb_lu *hpb,
		unsigned int lpn, int *region, int *subregion,
		int *offset)
{
	int region_offset;

	*region = lpn >> hpb->entries_per_region_shift;
	region_offset = lpn & hpb->entries_per_region_mask;
	*subregion = region_offset >> hpb->entries_per_subregion_shift;
	*offset = region_offset & hpb->entries_per_subregion_mask;
}

static unsigned long long ufshpb_get_ppn(struct ufshpb_map_ctx *mctx, int pos)
{
	unsigned long long *ppn_table;
	struct page *page;
	int index, offset;

	index = pos / HPB_ENTREIS_PER_OS_PAGE;
	offset = pos % HPB_ENTREIS_PER_OS_PAGE;

	page = mctx->m_page[index];
	WARN_ON(!page);

	ppn_table = page_address(page);
	WARN_ON(!ppn_table);

	return ppn_table[offset];
}

void ufshpb_prep_fn(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufshpb_lu *hpb;
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;
	unsigned int lpn;
	unsigned long long ppn = 0;
	int region, subregion, subregion_offset;

	/* WKLU could not be HPB-LU */
	if (lrbp->lun >= UFS_UPIU_MAX_GENERAL_LUN)
		return;

	hpb = hba->ufshpb_lup[lrbp->lun];
	if (!hpb || !hpb->lu_hpb_enable) {
		if (ufshpb_is_read_lrbp(lrbp))
			goto read_10;
		return;
	}

	if (hpb->force_disable) {
		if (ufshpb_is_read_lrbp(lrbp))
			goto read_10;
		return;
	}

	lpn = blk_rq_pos(lrbp->cmd->request) / SECTORS_PER_BLOCK;
	ufshpb_get_pos_from_lpn(hpb, lpn, &region,
			&subregion, &subregion_offset);
	cb = hpb->region_tbl + region;

	if (ufshpb_is_write_discard_lrbp(lrbp)) {
		spin_lock_bh(&hpb->hpb_lock);

		if (cb->region_state == HPBREGION_INACTIVE) {
			spin_unlock_bh(&hpb->hpb_lock);
			return;
		}
		ufshpb_set_dirty(hpb, lrbp, region, subregion,
			subregion_offset);
		spin_unlock_bh(&hpb->hpb_lock);
		return;
	}

	if (!ufshpb_is_read_lrbp(lrbp))
		return;

	if (blk_rq_sectors(lrbp->cmd->request) != SECTORS_PER_BLOCK) {
		TMSG(hpb, "%llu + %u READ_10 many_blocks %d - %d",
			(unsigned long long) blk_rq_pos(lrbp->cmd->request),
			(unsigned int) blk_rq_sectors(lrbp->cmd->request),
			region, subregion);
		return;
	}

	cp = cb->subregion_tbl + subregion;

	spin_lock_bh(&hpb->hpb_lock);
	if (cb->region_state == HPBREGION_INACTIVE ||
			cp->subregion_state != HPBSUBREGION_CLEAN) {
		if (cb->region_state == HPBREGION_INACTIVE) {
			atomic64_inc(&hpb->region_miss);
			TMSG(hpb, "%llu + %u READ_10 RG_INACT %d - %d",
			(unsigned long long) blk_rq_pos(lrbp->cmd->request),
			(unsigned int) blk_rq_sectors(lrbp->cmd->request),
			region, subregion);
		} else if (cp->subregion_state == HPBSUBREGION_DIRTY
				|| cp->subregion_state == HPBSUBREGION_ISSUED) {
			atomic64_inc(&hpb->subregion_miss);
			TMSG(hpb, "%llu + %u READ_10 SRG_D %d - %d",
			(unsigned long long) blk_rq_pos(lrbp->cmd->request),
			(unsigned int) blk_rq_sectors(lrbp->cmd->request),
			region, subregion);
		} else
			TMSG(hpb, "%llu + %u READ_10 ( %d %d ) %d - %d",
			(unsigned long long) blk_rq_pos(lrbp->cmd->request),
			(unsigned int) blk_rq_sectors(lrbp->cmd->request),
			cb->region_state, cp->subregion_state,
			region, subregion);
		spin_unlock_bh(&hpb->hpb_lock);
		return;
	}

	if (ufshpb_ppn_dirty_check(hpb, cp, subregion_offset)) {
		atomic64_inc(&hpb->entry_dirty_miss);
		TMSG(hpb, "%llu + %u READ_10 E_D %d - %d",
			(unsigned long long) blk_rq_pos(lrbp->cmd->request),
			(unsigned int) blk_rq_sectors(lrbp->cmd->request),
			region, subregion);
		spin_unlock_bh(&hpb->hpb_lock);
		return;
	}

	ppn = ufshpb_get_ppn(cp->mctx, subregion_offset);
	spin_unlock_bh(&hpb->hpb_lock);

	ufshpb_ppn_prep(hpb, lrbp, ppn);
	TMSG(hpb, "%llu + %u READ_16 %d - %d",
		(unsigned long long) blk_rq_pos(lrbp->cmd->request),
		(unsigned int) blk_rq_sectors(lrbp->cmd->request),
		region, subregion);
	atomic64_inc(&hpb->hit);
	return;
read_10:
	if (!hpb || !lrbp)
		return;
	TMSG(hpb, "%llu + %u READ_10",
			(unsigned long long) blk_rq_pos(lrbp->cmd->request),
			(unsigned int) blk_rq_sectors(lrbp->cmd->request));
	atomic64_inc(&hpb->miss);
}

static int ufshpb_clean_dirty_bitmap(
		struct ufshpb_lu *hpb, struct ufshpb_subregion *cp)
{
	struct ufshpb_region *cb;

	cb = hpb->region_tbl + cp->region;

	/* if mctx is null, active block had been evicted out */
	if (cb->region_state == HPBREGION_INACTIVE || !cp->mctx) {
		HPB_DEBUG(hpb, "%d - %d evicted", cp->region, cp->subregion);
		return -EINVAL;
	}

	memset(cp->mctx->ppn_dirty, 0x00,
			hpb->entries_per_subregion >> bits_per_byte_shift);

	return 0;
}

static void ufshpb_clean_active_subregion(
		struct ufshpb_lu *hpb, struct ufshpb_subregion *cp)
{
	struct ufshpb_region *cb;

	cb = hpb->region_tbl + cp->region;

	/* if mctx is null, active block had been evicted out */
	if (cb->region_state == HPBREGION_INACTIVE || !cp->mctx) {
		HPB_DEBUG(hpb, "%d - %d evicted", cp->region, cp->subregion);
		return;
	}
	cp->subregion_state = HPBSUBREGION_CLEAN;
}

static void ufshpb_error_active_subregion(
		struct ufshpb_lu *hpb, struct ufshpb_subregion *cp)
{
	struct ufshpb_region *cb;

	cb = hpb->region_tbl + cp->region;

	/* if mctx is null, active block had been evicted out */
	if (cb->region_state == HPBREGION_INACTIVE || !cp->mctx) {
		ERR_MSG("%d - %d evicted", cp->region, cp->subregion);
		return;
	}
	cp->subregion_state = HPBSUBREGION_DIRTY;
}


static void ufshpb_map_compl_process(struct ufshpb_lu *hpb,
		struct ufshpb_map_req *map_req)
{
	unsigned long long debug_ppn_0, debug_ppn_65535;

	map_req->RSP_end = ktime_to_ns(ktime_get());

	debug_ppn_0 = ufshpb_get_ppn(map_req->mctx, 0);
	debug_ppn_65535 = ufshpb_get_ppn(map_req->mctx, 65535);

	TMSG(hpb, "Noti: C RB %d - %d", map_req->region, map_req->subregion);
	HPB_DEBUG(hpb, "UFSHPB COMPL READ BUFFER %d - %d ( %llx ~ %llx )",
			map_req->region, map_req->subregion,
			debug_ppn_0, debug_ppn_65535);
	HPB_DEBUG(hpb, "Profiling: start~tasklet1 = %lld, tasklet1~issue =%lld",
			map_req->RSP_tasklet_enter1 - map_req->RSP_start,
			map_req->RSP_issue - map_req->RSP_tasklet_enter1);

	HPB_DEBUG(hpb, "Profiling: issue~endio = %lld, endio~end = %lld",
			map_req->RSP_endio - map_req->RSP_issue,
			map_req->RSP_end - map_req->RSP_endio);

	spin_lock(&hpb->hpb_lock);
	ufshpb_clean_active_subregion(hpb,
			hpb->region_tbl[map_req->region].subregion_tbl +
			map_req->subregion);
	spin_unlock(&hpb->hpb_lock);
}

/*
 * Must held rsp_list_lock before enter this function
 */
static struct ufshpb_rsp_info *ufshpb_get_req_info(struct ufshpb_lu *hpb)
{
	struct ufshpb_rsp_info *rsp_info =
		list_first_entry_or_null(&hpb->lh_rsp_info_free,
					struct ufshpb_rsp_info,
					list_rsp_info);
	if (!rsp_info) {
		HPB_DEBUG(hpb, "there is no rsp_info");
		return NULL;
	}
	list_del(&rsp_info->list_rsp_info);
	memset(rsp_info, 0x00, sizeof(struct ufshpb_rsp_info));

	INIT_LIST_HEAD(&rsp_info->list_rsp_info);

	return rsp_info;
}

static void ufshpb_map_req_compl_fn(struct request *req, int error)
{
	struct ufshpb_map_req *map_req =
		(struct ufshpb_map_req *) req->end_io_data;
	struct ufs_hba *hba;
	struct ufshpb_lu *hpb;
	struct scsi_sense_hdr sshdr;
	struct ufshpb_region *cb;
	struct ufshpb_rsp_info *rsp_info;
	unsigned long flags;

	hpb = map_req->hpb;
	hba = hpb->hba;
	cb = hpb->region_tbl + map_req->region;
	map_req->RSP_endio = ktime_to_ns(ktime_get());

	if (hba->ufshpb_state != HPB_PRESENT)
		goto free_map_req;

	if (!error) {
		ufshpb_map_compl_process(hpb, map_req);
		goto free_map_req;
	}

	ERR_MSG("error number %d ( %d - %d )",
		error, map_req->region, map_req->subregion);
	scsi_normalize_sense(map_req->sense, SCSI_SENSE_BUFFERSIZE,
		&sshdr);
	ERR_MSG("code %x sense_key %x asc %x ascq %x",
		sshdr.response_code,
			sshdr.sense_key, sshdr.asc, sshdr.ascq);
	ERR_MSG("byte4 %x byte5 %x byte6 %x additional_len %x",
			sshdr.byte4, sshdr.byte5,
			sshdr.byte6, sshdr.additional_length);
	atomic64_inc(&hpb->rb_fail);

	if (sshdr.sense_key == ILLEGAL_REQUEST) {
		spin_lock(&hpb->hpb_lock);
		if (cb->region_state == HPBREGION_PINNED) {
			if (sshdr.asc == 0x06 && sshdr.ascq == 0x01) {
				HPB_DEBUG(hpb, "retry pinned rb %d - %d",
					map_req->region,
					map_req->subregion);
				INIT_LIST_HEAD(&map_req->list_map_req);
				list_add_tail(&map_req->list_map_req,
					&hpb->lh_map_req_retry);
				spin_unlock(&hpb->hpb_lock);

				schedule_delayed_work(&hpb->ufshpb_retry_work,
					msecs_to_jiffies(5000));
				return;

			} else {
				HPB_DEBUG(hpb, "pinned rb %d - %d(dirty)",
					map_req->region,
					map_req->subregion);

				ufshpb_error_active_subregion(hpb,
					cb->subregion_tbl + map_req->subregion);
				spin_unlock(&hpb->hpb_lock);
			}
		} else {
			spin_unlock(&hpb->hpb_lock);

			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			rsp_info = ufshpb_get_req_info(hpb);
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
			if (!rsp_info) {
				dev_warn(hba->dev, "%s:%d No rsp_info\n",
					__func__, __LINE__);
				goto free_map_req;
			}

			rsp_info->type = HPB_RSP_REQ_REGION_UPDATE;
			rsp_info->RSP_start = ktime_to_ns(ktime_get());
			rsp_info->active_cnt = 0;
			rsp_info->inactive_cnt = 1;
			rsp_info->inactive_list.region[0] = map_req->region;
			HPB_DEBUG(hpb,
				"Non-pinned rb %d is added to rsp_info_list",
				map_req->region);

			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			list_add_tail(&rsp_info->list_rsp_info,
				&hpb->lh_rsp_info);
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

			tasklet_schedule(&hpb->ufshpb_tasklet);
		}
	}

free_map_req:
	INIT_LIST_HEAD(&map_req->list_map_req);
	spin_lock(&hpb->hpb_lock);
	list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_free);
	spin_unlock(&hpb->hpb_lock);
}

static int ufshpb_execute_req_dev_ctx(struct ufshpb_lu *hpb,
				      unsigned char *cmd,
				      void *buf, int length)
{
	unsigned long flags;
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdp;
	struct ufs_hba *hba;
	int ret = 0;

	hba = hpb->hba;

	if (!hba->sdev_ufs_lu[hpb->lun]) {
		dev_warn(hba->dev, "(%s) UFSHPB cannot find scsi_device\n",
			__func__);
		return -ENODEV;
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdp = hba->sdev_ufs_lu[hpb->lun];
	if (!sdp)
		return -ENODEV;

	ret = scsi_device_get(sdp);
	if (!ret && !scsi_device_online(sdp)) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		ret = -ENODEV;
		scsi_device_put(sdp);
		return ret;
	}
	hba->issue_ioctl = true;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	ret = scsi_execute_req_flags(sdp, cmd, DMA_FROM_DEVICE,
				      buf, length, &sshdr,
				      msecs_to_jiffies(30000), 3, NULL, 0);
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->issue_ioctl = false;
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	scsi_device_put(sdp);

	return ret;
}

static void print_buf(unsigned char *buf, int length)
{
	int i;
	int line_max = 32;

	pr_info("%s:%d Device Context Print Start!!\n", __func__, __LINE__);
	for (i = 0; i < length; i++) {
		if (i % line_max == 0)
			pr_info("(0x%.2x) :", i);
		pr_info(" %.2x", buf[i]);

		if ((i + 1) % line_max == 0)
			pr_info("\n");
	}
	pr_info("\n");
}

static inline void ufshpb_set_read_dev_ctx_cmd(unsigned char *cmd, int lba,
					       int length)
{
	cmd[0] = READ_10;
	cmd[1] = 0x02;
	cmd[2] = GET_BYTE_3(lba);
	cmd[3] = GET_BYTE_2(lba);
	cmd[4] = GET_BYTE_1(lba);
	cmd[5] = GET_BYTE_0(lba);
	cmd[6] = GET_BYTE_2(length);
	cmd[7] = GET_BYTE_1(length);
	cmd[8] = GET_BYTE_0(length);
}

int ufshpb_issue_req_dev_ctx(struct ufshpb_lu *hpb, unsigned char *buf,
			      int buf_length)
{
	unsigned char cmd[10] = { 0 };
	int cmd_len = buf_length >> OS_PAGE_SHIFT;
	int ret = 0;

	ufshpb_set_read_dev_ctx_cmd(cmd, 0x48504230, cmd_len);

	ret = ufshpb_execute_req_dev_ctx(hpb, cmd, buf, buf_length);

	if (ret < 0)
		HPB_DEBUG(hpb, "failed with err %d", ret);

	print_buf(buf, buf_length);

	return ret;
}

static inline void ufshpb_set_read_buf_cmd(unsigned char *cmd,
		int region, int subregion, int subregion_mem_size)
{
	cmd[0] = UFSHPB_READ_BUFFER;
	cmd[1] = 0x01;
	cmd[2] = GET_BYTE_1(region);
	cmd[3] = GET_BYTE_0(region);
	cmd[4] = GET_BYTE_1(subregion);
	cmd[5] = GET_BYTE_0(subregion);
	cmd[6] = GET_BYTE_2(subregion_mem_size);
	cmd[7] = GET_BYTE_1(subregion_mem_size);
	cmd[8] = GET_BYTE_0(subregion_mem_size);
	cmd[9] = 0x00;
}

static void ufshpb_bio_init(struct bio *bio, struct bio_vec *table,
		int max_vecs)
{
	bio_init(bio);

	bio->bi_io_vec = table;
	bio->bi_max_vecs = max_vecs;
}

static int ufshpb_add_bio_page(struct ufshpb_lu *hpb,
		struct request_queue *q, struct bio *bio, struct bio_vec *bvec,
		struct ufshpb_map_ctx *mctx)
{
	struct page *page = NULL;
	int i, ret = 0;

	ufshpb_bio_init(bio, bvec, hpb->mpages_per_subregion);

	for (i = 0; i < hpb->mpages_per_subregion; i++) {
		/* virt_to_page(p + (OS_PAGE_SIZE * i)); */
		page = mctx->m_page[i];
		if (!page)
			return -ENOMEM;

		ret = bio_add_pc_page(q, bio, page, hpb->mpage_bytes, 0);

		if (ret != hpb->mpage_bytes) {
			ERR_MSG("error ret %d", ret);
			return -EINVAL;
		}
	}

	return 0;
}

static inline void ufshpb_issue_map_req(struct request_queue *q,
		struct request *req)
{
	unsigned long flags;

	spin_lock_irqsave(q->queue_lock, flags);
	list_add(&req->queuelist, &q->queue_head);
	spin_unlock_irqrestore(q->queue_lock, flags);
}

static int ufshpb_map_req_issue(struct ufshpb_lu *hpb,
		struct request_queue *q, struct ufshpb_map_req *map_req)
{
	struct request *req;
	struct bio *bio;
	unsigned char cmd[16] = { 0 };
	unsigned long long debug_ppn_0, debug_ppn_65535;
	int ret;

	bio = &map_req->bio;

	ret = ufshpb_add_bio_page(hpb, q, bio, map_req->bvec, map_req->mctx);
	if (ret) {
		HPB_DEBUG(hpb, "ufshpb_add_bio_page_error %d", ret);
		return ret;
	}

	ufshpb_set_read_buf_cmd(cmd, map_req->region, map_req->subregion,
				hpb->subregion_mem_size);

	req = &map_req->req;
	blk_rq_init(q, req);
	blk_rq_append_bio(req, bio);

	req->cmd_len = COMMAND_SIZE(cmd[0]);
	memcpy(req->cmd, cmd, req->cmd_len);

	req->cmd_type = REQ_TYPE_BLOCK_PC;
	req->cmd_flags = READ | REQ_SOFTBARRIER | REQ_QUIET | REQ_PREEMPT;
	req->timeout = msecs_to_jiffies(30000);
	req->end_io = ufshpb_map_req_compl_fn;
	req->end_io_data = (void *)map_req;
	req->sense = map_req->sense;
	req->sense_len = 0;

	debug_ppn_0 = ufshpb_get_ppn(map_req->mctx, 0);
	debug_ppn_65535 = ufshpb_get_ppn(map_req->mctx, 65535);

	HPB_DEBUG(hpb, "ISSUE READ_BUFFER : %d - %d ( %llx ~ %llx ) retry = %d",
		  map_req->region, map_req->subregion,
		  debug_ppn_0, debug_ppn_65535, map_req->retry_cnt);
	TMSG(hpb, "Noti: I RB %d - %d", map_req->region, map_req->subregion);

	/* this sequence already has spin_lock_irqsave(queue_lock) */
	ufshpb_issue_map_req(q, req);
	map_req->RSP_issue = ktime_to_ns(ktime_get());

	atomic64_inc(&hpb->map_req_cnt);

	return 0;
}

static int ufshpb_set_map_req(struct ufshpb_lu *hpb,
		int region, int subregion, struct ufshpb_map_ctx *mctx,
		struct ufshpb_rsp_info *rsp_info)
{
	struct ufshpb_map_req *map_req;

	spin_lock(&hpb->hpb_lock);
	map_req = list_first_entry_or_null(&hpb->lh_map_req_free,
					    struct ufshpb_map_req,
					    list_map_req);
	if (!map_req) {
		HPB_DEBUG(hpb, "There is no map_req");
		spin_unlock(&hpb->hpb_lock);
		return -ENOMEM;
	}
	list_del(&map_req->list_map_req);
	memset(map_req, 0x00, sizeof(struct ufshpb_map_req));

	spin_unlock(&hpb->hpb_lock);

	map_req->hpb = hpb;
	map_req->region = region;
	map_req->subregion = subregion;
	map_req->mctx = mctx;
	map_req->lun = hpb->lun;
	map_req->RSP_start = rsp_info->RSP_start;
	map_req->RSP_tasklet_enter1 = rsp_info->RSP_tasklet_enter;

	return ufshpb_map_req_issue(hpb,
		hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue, map_req);
}

static struct ufshpb_map_ctx *ufshpb_get_map_ctx(
		struct ufshpb_lu *hpb, int *err)
{
	struct ufshpb_map_ctx *mctx;

	mctx = list_first_entry_or_null(&hpb->lh_map_ctx,
			struct ufshpb_map_ctx, list_table);
	if (mctx) {
		list_del_init(&mctx->list_table);
		hpb->debug_free_table--;
		return mctx;
	}
	*err = -ENOMEM;
	return NULL;
}

static inline void ufshpb_add_lru_info(struct victim_select_info *lru_info,
				       struct ufshpb_region *cb)
{
	cb->region_state = HPBREGION_ACTIVE;
	list_add_tail(&cb->list_region, &lru_info->lru);
	atomic64_inc(&lru_info->active_cnt);
}

static inline int ufshpb_add_region(struct ufshpb_lu *hpb,
	struct ufshpb_region *cb)
{
	struct victim_select_info *lru_info;
	int subregion;
	int err = 0;

	lru_info = &hpb->lru_info;

	HPB_DEBUG(hpb, "\x1b[44m\x1b[32m E->active region: %d \x1b[0m",
		cb->region);
	TMSG(hpb, "Noti: ACT RG: %d", cb->region);

	for (subregion = 0; subregion < cb->subregion_count; subregion++) {
		struct ufshpb_subregion *cp;

		cp = cb->subregion_tbl + subregion;

		cp->mctx = ufshpb_get_map_ctx(hpb, &err);
		if (!cp->mctx) {
			HPB_DEBUG(hpb,
			"get mctx failed. err %d subregion %d free_table %d",
				err, subregion, hpb->debug_free_table);
			goto out;
		}

		cp->subregion_state = HPBSUBREGION_DIRTY;
	}
	ufshpb_add_lru_info(lru_info, cb);

	atomic64_inc(&hpb->region_add);
out:
	return err;
}

static inline void ufshpb_put_map_ctx(
		struct ufshpb_lu *hpb, struct ufshpb_map_ctx *mctx)
{
	list_add(&mctx->list_table, &hpb->lh_map_ctx);
	hpb->debug_free_table++;
}

static inline void ufshpb_purge_active_subregion(struct ufshpb_lu *hpb,
		struct ufshpb_subregion *cp, int state)
{
	if (state == HPBSUBREGION_UNUSED) {
		ufshpb_put_map_ctx(hpb, cp->mctx);
		cp->mctx = NULL;
	}

	cp->subregion_state = state;
}

static inline void ufshpb_cleanup_lru_info(struct victim_select_info *lru_info,
					   struct ufshpb_region *cb)
{
	list_del_init(&cb->list_region);
	cb->region_state = HPBREGION_INACTIVE;
	cb->hit_count = 0;
	atomic64_dec(&lru_info->active_cnt);
}

static inline void ufshpb_evict_region(struct ufshpb_lu *hpb,
				     struct ufshpb_region *cb)
{
	struct victim_select_info *lru_info;
	struct ufshpb_subregion *cp;
	int subregion;

	lru_info = &hpb->lru_info;

	HPB_DEBUG(hpb, "\x1b[41m\x1b[33m C->EVICT region: %d \x1b[0m",
		  cb->region);
	TMSG(hpb, "Noti: EVIC RG: %d", cb->region);

	ufshpb_cleanup_lru_info(lru_info, cb);
	atomic64_inc(&hpb->region_evict);
	for (subregion = 0; subregion < cb->subregion_count; subregion++) {
		cp = cb->subregion_tbl + subregion;

		ufshpb_purge_active_subregion(hpb, cp, HPBSUBREGION_UNUSED);
	}
}

static void ufshpb_hit_lru_info(struct victim_select_info *lru_info,
				       struct ufshpb_region *cb)
{
	switch (lru_info->selection_type) {
	case LRU:
		list_move_tail(&cb->list_region, &lru_info->lru);
		break;
	case LFU:
		if (cb->hit_count != 0xffffffff)
			cb->hit_count++;

		list_move_tail(&cb->list_region, &lru_info->lru);
		break;
	default:
		break;
	}
}

static struct ufshpb_region *ufshpb_victim_lru_info(
	struct victim_select_info *lru_info)
{
	struct ufshpb_region *cb;
	struct ufshpb_region *victim_cb = NULL;
	u32 hit_count = 0xffffffff;

	switch (lru_info->selection_type) {
	case LRU:
		victim_cb = list_first_entry(&lru_info->lru,
			struct ufshpb_region, list_region);
		break;
	case LFU:
		list_for_each_entry(cb, &lru_info->lru, list_region) {
			if (hit_count > cb->hit_count) {
				hit_count = cb->hit_count;
				victim_cb = cb;
			}
		}
		break;
	default:
		break;
	}

	return victim_cb;
}

static int ufshpb_evict_load_region(struct ufshpb_lu *hpb,
		struct ufshpb_rsp_info *rsp_info)
{
	struct ufshpb_region *cb;
	struct ufshpb_region *victim_cb;
	struct victim_select_info *lru_info;
	int region, ret;
	int iter;

	lru_info = &hpb->lru_info;
	HPB_DEBUG(hpb, "active_cnt :%ld", atomic64_read(&lru_info->active_cnt));

	for (iter = 0; iter < rsp_info->inactive_cnt; iter++) {
		region = rsp_info->inactive_list.region[iter];
		cb = hpb->region_tbl + region;

		spin_lock(&hpb->hpb_lock);
		if (cb->region_state == HPBREGION_PINNED) {
			/*
			 * Pinned active-block should not drop-out.
			 * But if so, it would treat error as critical,
			 * and it will run ufshpb_eh_work
			 */
			dev_warn(hpb->hba->dev,
				 "UFSHPB pinned active-block drop-out error\n");
			spin_unlock(&hpb->hpb_lock);
			goto error;
		}

		if (list_empty(&cb->list_region)) {
			spin_unlock(&hpb->hpb_lock);
			continue;
		}

		ufshpb_evict_region(hpb, cb);
		spin_unlock(&hpb->hpb_lock);
	}

	for (iter = 0; iter < rsp_info->active_cnt; iter++) {
		region = rsp_info->active_list.region[iter];
		cb = hpb->region_tbl + region;

		/*
		 * if already region is added to lru_list,
		 * just initiate the information of lru.
		 * because the region already has the map ctx.
		 * (!list_empty(&cb->list_region) == region->state=active...)
		 */

		spin_lock(&hpb->hpb_lock);
		if (!list_empty(&cb->list_region)) {
			ufshpb_hit_lru_info(lru_info, cb);
			spin_unlock(&hpb->hpb_lock);
			continue;
		}

		if (cb->region_state == HPBREGION_INACTIVE) {
			if (atomic64_read(&lru_info->active_cnt)
			    == lru_info->max_lru_active_cnt) {
				victim_cb = ufshpb_victim_lru_info(lru_info);
				if (!victim_cb) {
					dev_warn(hpb->hba->dev,
					 "UFSHPB victim_cb is NULL\n");
					spin_unlock(&hpb->hpb_lock);
					goto error;
				}
				TMSG(hpb, "Noti: VT RG %d", victim_cb->region);
				HPB_DEBUG(hpb,
					"max lru case. victim choose: %d",
					 victim_cb->region);

				ufshpb_evict_region(hpb, victim_cb);
			}

			ret = ufshpb_add_region(hpb, cb);
			if (ret) {
				dev_warn(hpb->hba->dev,
					 "UFSHPB memory allocation failed\n");
				spin_unlock(&hpb->hpb_lock);
				goto error;
			}
		}
		spin_unlock(&hpb->hpb_lock);
	}
	return 0;
error:
	return -ENOMEM;
}

static inline struct ufshpb_rsp_field *ufshpb_get_hpb_rsp(
		struct ufshcd_lrb *lrbp)
{
	return (struct ufshpb_rsp_field *)&lrbp->ucd_rsp_ptr->sr.sense_data_len;
}

static void ufshpb_rsp_map_cmd_req(struct ufshpb_lu *hpb,
		struct ufshpb_rsp_info *rsp_info)
{
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;
	int region, subregion;
	int iter;
	int ret;

	/*
	 *  Before Issue read buffer CMD for active active block,
	 *  prepare the memory from memory pool.
	 */
	ret = ufshpb_evict_load_region(hpb, rsp_info);
	if (ret) {
		HPB_DEBUG(hpb, "region evict/load failed. ret %d", ret);
		goto wakeup_ee_worker;
	}

	/*
	 * If there is active region req, read buffer cmd issue.
	 */
	for (iter = 0; iter < rsp_info->active_cnt; iter++) {
		region = rsp_info->active_list.region[iter];
		subregion = rsp_info->active_list.subregion[iter];
		cb = hpb->region_tbl + region;

		if (region >= hpb->regions_per_lu ||
		    subregion >= cb->subregion_count) {
			HPB_DEBUG(hpb, "ufshpb issue-map %d - %d range error",
				  region, subregion);
			goto wakeup_ee_worker;
		}

		cp = cb->subregion_tbl + subregion;

		/* if subregion_state set HPBSUBREGION_ISSUED,
		 * active_page has already been added to list,
		 * so it just ends function.
		 */

		spin_lock(&hpb->hpb_lock);
		if (cp->subregion_state == HPBSUBREGION_ISSUED) {
			spin_unlock(&hpb->hpb_lock);
			continue;
		}

		cp->subregion_state = HPBSUBREGION_ISSUED;

		ret = ufshpb_clean_dirty_bitmap(hpb, cp);

		spin_unlock(&hpb->hpb_lock);

		if (ret)
			continue;

		if (hpb->force_map_req_disable) {
			HPB_DEBUG(hpb, "map disable - return");
			return;
		}

		ret = ufshpb_set_map_req(hpb, region, subregion, cp->mctx,
			rsp_info);
		if (ret) {
			HPB_DEBUG(hpb, "ufshpb_set_map_req error %d", ret);
			goto wakeup_ee_worker;
		}
	}
	return;

wakeup_ee_worker:
	hpb->hba->ufshpb_state = HPB_FAILED;
	schedule_work(&hpb->hba->ufshpb_eh_work);
}

/* routine : isr (ufs) */
void ufshpb_rsp_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufshpb_lu *hpb;
	struct ufshpb_rsp_field *rsp_field;
	struct ufshpb_rsp_info *rsp_info;
	struct ufshpb_region *region;
	int data_seg_len, num, blk_idx;

	data_seg_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2)
		& MASK_RSP_UPIU_DATA_SEG_LEN;

	if (!data_seg_len) {
		bool do_tasklet = false;

		if (lrbp->lun >= UFS_UPIU_MAX_GENERAL_LUN)
			return;

		hpb = hba->ufshpb_lup[lrbp->lun];
		if (!hpb)
			return;

		spin_lock(&hpb->rsp_list_lock);
		do_tasklet = !list_empty(&hpb->lh_rsp_info);
		spin_unlock(&hpb->rsp_list_lock);

		if (do_tasklet)
			tasklet_schedule(&hpb->ufshpb_tasklet);
		return;
	}

	rsp_field = ufshpb_get_hpb_rsp(lrbp);

	if ((SHIFT_BYTE_1(rsp_field->sense_data_len[0]) |
	     SHIFT_BYTE_0(rsp_field->sense_data_len[1])) != DEV_SENSE_SEG_LEN ||
	    rsp_field->desc_type != DEV_DES_TYPE ||
	    rsp_field->additional_len != DEV_ADDITIONAL_LEN ||
	    rsp_field->hpb_type == HPB_RSP_NONE ||
	    rsp_field->active_region_cnt > MAX_ACTIVE_NUM ||
	    rsp_field->inactive_region_cnt > MAX_INACTIVE_NUM)
		return;

	if (lrbp->lun >= UFS_UPIU_MAX_GENERAL_LUN) {
		dev_warn(hba->dev, "lun is not general = %d", lrbp->lun);
		return;
	}

	hpb = hba->ufshpb_lup[lrbp->lun];
	if (!hpb) {
		dev_warn(hba->dev, "%s:%d UFS-LU%d is not UFSHPB LU\n",
			__func__,
			__LINE__, lrbp->lun);
		return;
	}

	WARN_ON(!rsp_field);
	WARN_ON(!lrbp);
	WARN_ON(!lrbp->ucd_rsp_ptr);

	HPB_DEBUG(hpb, "HPB-Info Noti: %d LUN: %d Seg-Len %d, Req_type = %d",
		  rsp_field->hpb_type, lrbp->lun,
		  be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
		  MASK_RSP_UPIU_DATA_SEG_LEN, rsp_field->reserved);
	atomic64_inc(&hpb->rb_noti_cnt);

	if (!hpb->lu_hpb_enable) {
		dev_warn(hba->dev, "UFSHPB(%s) LU(%d) is disabled.\n",
				__func__, lrbp->lun);
		return;
	}

	spin_lock(&hpb->rsp_list_lock);
	rsp_info = ufshpb_get_req_info(hpb);
	spin_unlock(&hpb->rsp_list_lock);
	if (!rsp_info)
		return;

	switch (rsp_field->hpb_type) {
	case HPB_RSP_REQ_REGION_UPDATE:
		WARN_ON(data_seg_len != DEV_DATA_SEG_LEN);
		rsp_info->type = HPB_RSP_REQ_REGION_UPDATE;

		rsp_info->RSP_start = ktime_to_ns(ktime_get());

		for (num = 0; num < rsp_field->active_region_cnt; num++) {
			blk_idx = num * PER_ACTIVE_INFO_BYTES;
			rsp_info->active_list.region[num] =
				SHIFT_BYTE_1(
				rsp_field->hpb_active_field[blk_idx + 0]) |
				SHIFT_BYTE_0(
				rsp_field->hpb_active_field[blk_idx + 1]);
			rsp_info->active_list.subregion[num] =
				SHIFT_BYTE_1(
				rsp_field->hpb_active_field[blk_idx + 2]) |
				SHIFT_BYTE_0(
				rsp_field->hpb_active_field[blk_idx + 3]);
			HPB_DEBUG(hpb,
				"active num: %d, region: %d, subregion: %d",
				num + 1,
				rsp_info->active_list.region[num],
				rsp_info->active_list.subregion[num]);
		}
		rsp_info->active_cnt = num;

		for (num = 0; num < rsp_field->inactive_region_cnt; num++) {
			blk_idx = num * PER_INACTIVE_INFO_BYTES;
			rsp_info->inactive_list.region[num] =
				SHIFT_BYTE_1(
				rsp_field->hpb_inactive_field[blk_idx + 0]) |
				SHIFT_BYTE_0(
				rsp_field->hpb_inactive_field[blk_idx + 1]);
			HPB_DEBUG(hpb, "inactive num: %d, region: %d",
				  num + 1, rsp_info->inactive_list.region[num]);
		}
		rsp_info->inactive_cnt = num;

		TMSG(hpb, "Noti: #ACTIVE %d, #INACTIVE %d",
		     rsp_info->active_cnt, rsp_info->inactive_cnt);

		HPB_DEBUG(hpb, "active cnt: %d, inactive cnt: %d",
			  rsp_info->active_cnt, rsp_info->inactive_cnt);

		if (hpb->debug) {
			list_for_each_entry(region, &hpb->lru_info.lru,
				list_region) {
				HPB_DEBUG(hpb, "active list : %d (cnt: %d)",
					  region->region, region->hit_count);
			}
		}

		HPB_DEBUG(hpb, "add_list %p -> %p",
			  rsp_info, &hpb->lh_rsp_info);
		spin_lock(&hpb->rsp_list_lock);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info);
		spin_unlock(&hpb->rsp_list_lock);

		tasklet_schedule(&hpb->ufshpb_tasklet);
		break;

	default:
		HPB_DEBUG(hpb, "hpb_type is not available : %d",
			  rsp_field->hpb_type);
		break;
	}
}

static int ufshpb_read_desc(struct ufs_hba *hba,
		u8 desc_id, u8 desc_index, u8 *desc_buf, u32 size)
{
	int retries, err = 0;
	u8 selector = 1;

	for (retries = 3; retries > 0; retries--) {
		err = ufshcd_query_descriptor_retry(hba,
			UPIU_QUERY_OPCODE_READ_DESC,
			desc_id, desc_index, selector, desc_buf, &size);
		if (!err)
			break;

		dev_dbg(hba->dev,
			"%s:%d reading Device Desc failed. err = %d\n",
			__func__, __LINE__, err);
	}
	return err;
}

static int ufshpb_read_device_desc(struct ufs_hba *hba, u8 *desc_buf, u32 size)
{
	return ufshpb_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, desc_buf, size);
}

static int ufshpb_read_geo_desc(struct ufs_hba *hba, u8 *desc_buf, u32 size)
{
	return ufshpb_read_desc(hba, QUERY_DESC_IDN_GEOMETRY, 0, desc_buf,
		size);
}

static int ufshpb_read_unit_desc(struct ufs_hba *hba, int lun, u8 *desc_buf,
	u32 size)
{
	return ufshpb_read_desc(hba, QUERY_DESC_IDN_UNIT, lun, desc_buf, size);
}

static inline void ufshpb_add_subregion_to_req_list(struct ufshpb_lu *hpb,
		struct ufshpb_subregion *cp)
{
	list_add_tail(&cp->list_subregion, &hpb->lh_subregion_req);
	cp->subregion_state = HPBSUBREGION_ISSUED;
}

static int ufshpb_execute_req(struct ufshpb_lu *hpb, unsigned char *cmd,
		struct ufshpb_subregion *cp)
{
	unsigned long flags;
	struct request_queue *q;
	char sense[SCSI_SENSE_BUFFERSIZE];
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdp;
	struct ufs_hba *hba;
	struct request req;
	struct bio bio;
	struct bio_vec *bvec;
	int ret = 0;

	hba = hpb->hba;

	bvec = kmalloc_array(hpb->mpages_per_subregion, sizeof(struct bio_vec),
		GFP_KERNEL);
	if (!bvec)
		return -ENOMEM;

	if (!hba->sdev_ufs_lu[hpb->lun]) {
		dev_warn(hba->dev, "(%s) UFSHPB cannot find scsi_device\n",
			__func__);
		ret = -ENODEV;
		goto mem_free_out;
	}

	sdp = hba->sdev_ufs_lu[hpb->lun];
	if (!sdp) {
		dev_warn(hba->dev, "(%s) UFSHPB cannot find scsi_device\n",
			__func__);
		ret = -ENODEV;
		goto mem_free_out;
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdp);
	if (!ret && !scsi_device_online(sdp)) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		ret = -ENODEV;
		scsi_device_put(sdp);
		goto mem_free_out;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	q = sdp->request_queue;

	ret = ufshpb_add_bio_page(hpb, q, &bio, bvec, cp->mctx);
	if (ret) {
		scsi_device_put(sdp);
		goto mem_free_out;
	}

	blk_rq_init(q, &req);
	blk_rq_append_bio(&req, &bio);

	req.cmd_len = COMMAND_SIZE(cmd[0]);
	memcpy(req.cmd, cmd, req.cmd_len);
	req.sense = sense;
	req.sense_len = 0;
	req.retries = 3;
	req.timeout = msecs_to_jiffies(10000);
	req.cmd_type = REQ_TYPE_BLOCK_PC;
	req.cmd_flags = REQ_QUIET | REQ_PREEMPT;

	blk_execute_rq(q, NULL, &req, 1);
	if (req.errors) {
		ret = -EIO;
		scsi_normalize_sense(req.sense, SCSI_SENSE_BUFFERSIZE, &sshdr);
		ERR_MSG("code %x sense_key %x asc %x ascq %x",
			sshdr.response_code, sshdr.sense_key, sshdr.asc,
			sshdr.ascq);
		ERR_MSG("byte4 %x byte5 %x byte6 %x additional_len %x",
			sshdr.byte4, sshdr.byte5, sshdr.byte6,
			sshdr.additional_length);
		spin_lock_bh(&hpb->hpb_lock);
		ufshpb_error_active_subregion(hpb, cp);
		spin_unlock_bh(&hpb->hpb_lock);
	} else {
		ret = 0;
		spin_lock_bh(&hpb->hpb_lock);
		ufshpb_clean_dirty_bitmap(hpb, cp);
		spin_unlock_bh(&hpb->hpb_lock);
	}
	scsi_device_put(sdp);
mem_free_out:
	kfree(bvec);
	return ret;
}

static int ufshpb_issue_map_req_from_list(struct ufshpb_lu *hpb)
{
	struct ufshpb_subregion *cp, *next_cp;
	int ret;

	LIST_HEAD(req_list);

	spin_lock_bh(&hpb->hpb_lock);
	list_splice_init(&hpb->lh_subregion_req,
			&req_list);
	spin_unlock_bh(&hpb->hpb_lock);

	list_for_each_entry_safe(cp, next_cp, &req_list, list_subregion) {
		unsigned char cmd[10] = { 0 };
		unsigned long long debug_ppn_0, debug_ppn_65535;

		ufshpb_set_read_buf_cmd(cmd, cp->region, cp->subregion,
					hpb->subregion_mem_size);

		debug_ppn_0 = ufshpb_get_ppn(cp->mctx, 0);
		debug_ppn_65535 = ufshpb_get_ppn(cp->mctx, 65535);

		HPB_DEBUG(hpb, "ISSUE READ_BUFFER : %d - %d ( %llx ~ %llx )",
			  cp->region, cp->subregion,
			  debug_ppn_0, debug_ppn_65535);

		TMSG(hpb, "Noti: I RB %d - %d",	cp->region, cp->subregion);

		ret = ufshpb_execute_req(hpb, cmd, cp);
		if (ret < 0) {
			ERR_MSG("region %d sub %d failed with err %d",
				cp->region, cp->subregion, ret);
			continue;
		}

		debug_ppn_0 = ufshpb_get_ppn(cp->mctx, 0);
		debug_ppn_65535 = ufshpb_get_ppn(cp->mctx, 65535);

		TMSG(hpb, "Noti: C RB %d - %d",	cp->region, cp->subregion);

		HPB_DEBUG(hpb, "COMPL READ_BUFFER : %d - %d ( %llx ~ %llx )",
			  cp->region, cp->subregion,
			  debug_ppn_0, debug_ppn_65535);

		spin_lock_bh(&hpb->hpb_lock);
		ufshpb_clean_active_subregion(hpb, cp);
		list_del_init(&cp->list_subregion);
		spin_unlock_bh(&hpb->hpb_lock);
	}

	return 0;
}

static void ufshpb_work_handler(struct work_struct *work)
{
	struct ufshpb_lu *hpb;
	int ret;

	hpb = container_of(work, struct ufshpb_lu, ufshpb_work);
	HPB_DEBUG(hpb, "worker start");

	if (!list_empty(&hpb->lh_subregion_req)) {
		ret = ufshpb_issue_map_req_from_list(hpb);
		/*
		 * if its function failed at init time,
		 * ufshpb-device will request map-req,
		 * so it is not critical-error, and just finish work-handler
		 */
		if (ret)
			HPB_DEBUG(hpb, "failed map-issue. ret %d", ret);
	}

	HPB_DEBUG(hpb, "worker end");
}

static void ufshpb_retry_work_handler(struct work_struct *work)
{
	struct ufshpb_lu *hpb;
	struct delayed_work *dwork = to_delayed_work(work);
	struct ufshpb_map_req *map_req;
	int ret = 0;

	LIST_HEAD(retry_list);

	hpb = container_of(dwork, struct ufshpb_lu, ufshpb_retry_work);
	HPB_DEBUG(hpb, "retry worker start");


	spin_lock_bh(&hpb->hpb_lock);
	list_splice_init(&hpb->lh_map_req_retry, &retry_list);
	spin_unlock_bh(&hpb->hpb_lock);

	while (1) {
		map_req = list_first_entry_or_null(&retry_list,
				struct ufshpb_map_req, list_map_req);
		if (!map_req) {
			HPB_DEBUG(hpb, "There is no map_req");
			break;
		}
		list_del(&map_req->list_map_req);

		map_req->retry_cnt++;

		ret = ufshpb_map_req_issue(hpb,
			hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue,
			map_req);
		if (ret) {
			HPB_DEBUG(hpb, "ufshpb_set_map_req error %d", ret);
			goto wakeup_ee_worker;
		}
	}
	HPB_DEBUG(hpb, "worker end");
	return;

wakeup_ee_worker:
	hpb->hba->ufshpb_state = HPB_FAILED;
	schedule_work(&hpb->hba->ufshpb_eh_work);
}

static void ufshpb_tasklet_fn(unsigned long private)
{
	struct ufshpb_lu *hpb = (struct ufshpb_lu *)private;
	struct ufshpb_rsp_info *rsp_info;
	unsigned long flags;

	while (1) {
		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		rsp_info = list_first_entry_or_null(&hpb->lh_rsp_info,
				struct ufshpb_rsp_info, list_rsp_info);
		if (!rsp_info) {
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
			break;
		}

		rsp_info->RSP_tasklet_enter = ktime_to_ns(ktime_get());

		list_del_init(&rsp_info->list_rsp_info);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

		switch (rsp_info->type) {
		case HPB_RSP_REQ_REGION_UPDATE:
			ufshpb_rsp_map_cmd_req(hpb, rsp_info);
			break;
		default:
			break;
		}

		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		HPB_DEBUG(hpb, "add_list %p -> %p",
			  &hpb->lh_rsp_info_free, rsp_info);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info_free);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
	}
}

static void ufshpb_init_constant(void)
{
	sects_per_blk_shift = ffs(BLOCK) - ffs(SECTOR);
	INIT_INFO("sects_per_blk_shift: %u %u",
		  sects_per_blk_shift, ffs(SECTORS_PER_BLOCK) - 1);

	bits_per_dword_shift = ffs(BITS_PER_DWORD) - 1;
	bits_per_dword_mask = BITS_PER_DWORD - 1;
	INIT_INFO("bits_per_dword %u shift %u mask 0x%X",
		  BITS_PER_DWORD, bits_per_dword_shift, bits_per_dword_mask);

	bits_per_byte_shift = ffs(BITS_PER_BYTE) - 1;
	INIT_INFO("bits_per_byte %u shift %u",
		  BITS_PER_BYTE, bits_per_byte_shift);
}

static inline void ufshpb_req_mempool_remove(struct ufshpb_lu *hpb)
{
	kfree(hpb->rsp_info);
	kfree(hpb->map_req);
}


static void ufshpb_table_mempool_remove(struct ufshpb_lu *hpb)
{
	struct ufshpb_map_ctx *mctx, *next;
	int i;

	/*
	 * the mctx in the lh_map_ctx has been allocated completely.
	 */
	list_for_each_entry_safe(mctx, next, &hpb->lh_map_ctx, list_table) {
		for (i = 0; i < hpb->mpages_per_subregion; i++)
			__free_page(mctx->m_page[i]);

		vfree(mctx->ppn_dirty);
		kfree(mctx->m_page);
		kfree(mctx);
		alloc_mctx--;
	}
}

static int ufshpb_init_pinned_active_block(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb)
{
	struct ufshpb_subregion *cp;
	int subregion, j;
	int err = 0;

	for (subregion = 0; subregion < cb->subregion_count; subregion++) {
		cp = cb->subregion_tbl + subregion;

		cp->mctx = ufshpb_get_map_ctx(hpb, &err);
		if (err) {
			ERR_MSG(
			"get mctx failed. err %d subregion %d free_table %d",
				err, subregion, hpb->debug_free_table);
			goto release;
		}
		spin_lock_bh(&hpb->hpb_lock);
		ufshpb_add_subregion_to_req_list(hpb, cp);
		spin_unlock_bh(&hpb->hpb_lock);
	}

	cb->region_state = HPBREGION_PINNED;

	return 0;

release:
	for (j = 0; j < subregion; j++) {
		cp = cb->subregion_tbl + j;
		ufshpb_put_map_ctx(hpb, cp->mctx);
	}

	return err;
}

static inline bool ufshpb_is_HPBREGION_PINNED(
		struct ufshpb_lu_desc *lu_desc, int region)
{
	if (lu_desc->lu_hpb_pinned_end_offset != -1 &&
			region >= lu_desc->hpb_pinned_region_startidx &&
				region <= lu_desc->lu_hpb_pinned_end_offset)
		return true;

	return false;
}

static inline void ufshpb_init_jobs(struct ufshpb_lu *hpb)
{
	INIT_WORK(&hpb->ufshpb_work, ufshpb_work_handler);
	INIT_DELAYED_WORK(&hpb->ufshpb_retry_work, ufshpb_retry_work_handler);
	tasklet_init(&hpb->ufshpb_tasklet, ufshpb_tasklet_fn,
		(unsigned long)hpb);
}

static inline void ufshpb_cancel_jobs(struct ufshpb_lu *hpb)
{
	cancel_work_sync(&hpb->ufshpb_work);
	cancel_delayed_work_sync(&hpb->ufshpb_retry_work);
	tasklet_kill(&hpb->ufshpb_tasklet);
}

static void ufshpb_init_subregion_tbl(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb)
{
	int subregion;

	for (subregion = 0; subregion < cb->subregion_count; subregion++) {
		struct ufshpb_subregion *cp = cb->subregion_tbl + subregion;

		cp->region = cb->region;
		cp->subregion = subregion;
		cp->subregion_state = HPBSUBREGION_UNUSED;
	}
}

static inline int ufshpb_alloc_subregion_tbl(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb, int subregion_count)
{
	cb->subregion_tbl = kzalloc(
			sizeof(struct ufshpb_subregion) * subregion_count,
			GFP_KERNEL);
	if (!cb->subregion_tbl)
		return -ENOMEM;

	cb->subregion_count = subregion_count;
	INIT_INFO("region %d subregion_count %d active_page_table bytes %lu",
		cb->region, subregion_count,
		sizeof(struct ufshpb_subregion *) *
		hpb->subregions_per_region);

	return 0;
}

static int ufshpb_table_mempool_init(struct ufshpb_lu *hpb,
		int num_regions, int subregions_per_region,
		int entry_count, int entry_byte)
{
	int i, j, k;
	struct ufshpb_map_ctx *mctx = NULL;

	INIT_LIST_HEAD(&hpb->lh_map_ctx);

	for (i = 0; i < num_regions * subregions_per_region; i++) {
		mctx = kmalloc(sizeof(struct ufshpb_map_ctx), GFP_KERNEL);
		if (!mctx)
			goto release_mem;

		mctx->m_page = kzalloc(sizeof(struct page *) *
			hpb->mpages_per_subregion, GFP_KERNEL);
		if (!mctx->m_page)
			goto release_mem;

		mctx->ppn_dirty = (unsigned int *)
			vzalloc(entry_count >> bits_per_byte_shift);
		if (!mctx->ppn_dirty)
			goto release_mem;

		for (j = 0; j < hpb->mpages_per_subregion; j++) {
			mctx->m_page[j] = alloc_page(GFP_KERNEL | __GFP_ZERO);
			if (!mctx->m_page[j]) {
				for (k = 0; k < j; k++)
					kfree(mctx->m_page[k]);
				goto release_mem;
			}
		}
		INIT_INFO("[%d] mctx->m_page %p get_order %d", i,
			  mctx->m_page, get_order(hpb->mpages_per_subregion));

		INIT_LIST_HEAD(&mctx->list_table);
		list_add(&mctx->list_table, &hpb->lh_map_ctx);

		hpb->debug_free_table++;
	}

	alloc_mctx = num_regions * subregions_per_region;
	INIT_INFO("number of mctx %d %d %d. debug_free_table %d",
		  num_regions * subregions_per_region, num_regions,
		  subregions_per_region, hpb->debug_free_table);
	return 0;

release_mem:
	/*
	 * mctxs already added in lh_map_ctx will be removed
	 * in the caller function.
	 */
	if (mctx) {
		kfree(mctx->m_page);
		if (mctx->ppn_dirty)
			vfree(mctx->ppn_dirty);
		kfree(mctx);
	}
	return -ENOMEM;
}

static int ufshpb_req_mempool_init(struct ufshpb_lu *hpb, int num_pinned_rgn,
				   int queue_depth)
{
	struct ufs_hba *hba;
	struct ufshpb_rsp_info *rsp_info = NULL;
	struct ufshpb_map_req *map_req = NULL;
	int i, map_req_cnt = 0;

	hba = hpb->hba;

	if (!queue_depth) {
		queue_depth = hba->nutrs;
		INIT_INFO("lu_queue_depth is 0. we use device's queue info.");
		INIT_INFO("hba->nutrs = %d", hba->nutrs);
	}

	INIT_LIST_HEAD(&hpb->lh_rsp_info_free);
	INIT_LIST_HEAD(&hpb->lh_map_req_free);
	INIT_LIST_HEAD(&hpb->lh_map_req_retry);

	hpb->rsp_info = kcalloc(queue_depth, sizeof(struct ufshpb_rsp_info),
		GFP_KERNEL);
	if (!hpb->rsp_info)
		goto release_mem;

	map_req_cnt = max((num_pinned_rgn * hpb->subregions_per_region * 2),
			  queue_depth);

	hpb->map_req = kcalloc(map_req_cnt, sizeof(struct ufshpb_map_req),
		GFP_KERNEL);
	if (!hpb->map_req)
		goto release_mem;

	for (i = 0; i < queue_depth; i++) {
		rsp_info = hpb->rsp_info + i;
		INIT_LIST_HEAD(&rsp_info->list_rsp_info);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info_free);
	}

	for (i = 0; i < map_req_cnt; i++) {
		map_req = hpb->map_req + i;
		INIT_LIST_HEAD(&map_req->list_map_req);
		list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_free);
	}

	return 0;

release_mem:
	kfree(hpb->rsp_info);
	return -ENOMEM;
}

static void ufshpb_init_lu_constant(struct ufshpb_lu *hpb,
		struct ufshpb_lu_desc *lu_desc,
		struct ufshpb_func_desc *func_desc)
{
	unsigned long long region_unit_size, region_mem_size;
	int entries_per_region;

	/*	From descriptors	*/
	region_unit_size = (unsigned long long)
		SECTOR * (0x01 << func_desc->hpb_region_size);
	region_mem_size = region_unit_size / BLOCK * HPB_ENTRY_SIZE;

	hpb->subregion_unit_size = (unsigned long long)
		SECTOR * (0x01 << func_desc->hpb_subregion_size);
	hpb->subregion_mem_size = hpb->subregion_unit_size /
		BLOCK * HPB_ENTRY_SIZE;

	hpb->hpb_ver = func_desc->hpb_ver;
	hpb->lu_max_active_regions = lu_desc->lu_max_active_hpb_regions;
	hpb->lru_info.max_lru_active_cnt =
					lu_desc->lu_max_active_hpb_regions
					- lu_desc->lu_num_hpb_pinned_regions;

	/*	relation : lu <-> region <-> sub region <-> entry	 */
	hpb->lu_num_blocks = lu_desc->lu_logblk_cnt;
	entries_per_region = region_mem_size / HPB_ENTRY_SIZE;
	hpb->entries_per_subregion = hpb->subregion_mem_size / HPB_ENTRY_SIZE;
	hpb->subregions_per_region = region_mem_size / hpb->subregion_mem_size;

	/*
	 * 1. regions_per_lu = (lu_num_blocks * 4096) / region_unit_size
	 *          = (lu_num_blocks * HPB_ENTRY_SIZE) / region_mem_size
	 *          = lu_num_blocks / (region_mem_size / HPB_ENTRY_SIZE)
	 *
	 * 2. regions_per_lu = lu_num_blocks / subregion_mem_size (is trik...)
	 *    if HPB_ENTRY_SIZE != subregions_per_region, it is error.
	 */
	hpb->regions_per_lu = ((unsigned long long)hpb->lu_num_blocks
			       + (region_mem_size / HPB_ENTRY_SIZE) - 1)
				/ (region_mem_size / HPB_ENTRY_SIZE);
	hpb->subregions_per_lu = ((unsigned long long)hpb->lu_num_blocks
				  + (hpb->subregion_mem_size / HPB_ENTRY_SIZE)
				  - 1)
				/ (hpb->subregion_mem_size / HPB_ENTRY_SIZE);

	/* mempool info */
	hpb->mpage_bytes = OS_PAGE_SIZE;
	hpb->mpages_per_subregion = hpb->subregion_mem_size / hpb->mpage_bytes;

	/* Bitmask Info */
	hpb->dwords_per_subregion = hpb->entries_per_subregion / BITS_PER_DWORD;
	hpb->entries_per_region_shift = ffs(entries_per_region) - 1;
	hpb->entries_per_region_mask = entries_per_region - 1;
	hpb->entries_per_subregion_shift = ffs(hpb->entries_per_subregion) - 1;
	hpb->entries_per_subregion_mask = hpb->entries_per_subregion - 1;

	INIT_INFO("===== From Device Descriptor! =====");
	INIT_INFO("hpb_region_size = %d, hpb_subregion_size = %d",
		  func_desc->hpb_region_size, func_desc->hpb_subregion_size);
	INIT_INFO("=====   Constant Values(LU)   =====");
	INIT_INFO("region_unit_size = %lld, region_mem_size %lld",
		  region_unit_size, region_mem_size);
	INIT_INFO("subregion_unit_size = %lld, subregion_mem_size %d",
		  hpb->subregion_unit_size, hpb->subregion_mem_size);

	INIT_INFO("lu_num_blocks=%d, regions_per_lu=%d, subregions_per_lu = %d",
		  hpb->lu_num_blocks, hpb->regions_per_lu,
		  hpb->subregions_per_lu);

	INIT_INFO("subregions_per_region = %d", hpb->subregions_per_region);
	INIT_INFO("entries_per_region %u shift %u mask 0x%X",
		  entries_per_region,
		  hpb->entries_per_region_shift, hpb->entries_per_region_mask);
	INIT_INFO("entries_per_subregion %u shift %u mask 0x%X",
		  hpb->entries_per_subregion, hpb->entries_per_subregion_shift,
		  hpb->entries_per_subregion_mask);
	INIT_INFO("mpages_per_subregion : %d", hpb->mpages_per_subregion);
	INIT_INFO("===================================\n");
}

static int ufshpb_lu_hpb_init(struct ufs_hba *hba, struct ufshpb_lu *hpb,
		struct ufshpb_func_desc *func_desc,
		struct ufshpb_lu_desc *lu_desc, int lun)
{
	struct ufshpb_region *region_table, *cb;
	struct ufshpb_subregion *cp;
	int region, subregion;
	int total_subregion_count, subregion_count;
	bool do_work_handler;
	int ret, j;

	hpb->hba = hba;
	hpb->lun = lun;
	hpb->debug = false;
	hpb->lu_hpb_enable = true;

	ufshpb_init_lu_constant(hpb, lu_desc, func_desc);

	region_table = kzalloc(
		sizeof(struct ufshpb_region) * hpb->regions_per_lu,
		GFP_KERNEL);
	if (!region_table) {
		ret = -ENOMEM;
		goto out;
	}

	INIT_INFO("active_block_table bytes: %lu",
		  (sizeof(struct ufshpb_region) * hpb->regions_per_lu));

	hpb->region_tbl = region_table;

	spin_lock_init(&hpb->hpb_lock);
	spin_lock_init(&hpb->rsp_list_lock);

	/* init lru information*/
	INIT_LIST_HEAD(&hpb->lru_info.lru);
	hpb->lru_info.selection_type = LRU;

	INIT_LIST_HEAD(&hpb->lh_subregion_req);
	INIT_LIST_HEAD(&hpb->lh_rsp_info);
	INIT_LIST_HEAD(&hpb->lh_map_ctx);

	ret = ufshpb_table_mempool_init(hpb,
			lu_desc->lu_max_active_hpb_regions,
			hpb->subregions_per_region,
			hpb->entries_per_subregion, HPB_ENTRY_SIZE);
	if (ret) {
		ERR_MSG("ppn table mempool init fail!");
		goto release_mempool;
	}

	ret = ufshpb_req_mempool_init(hpb, lu_desc->lu_num_hpb_pinned_regions,
				      lu_desc->lu_queue_depth);
	if (ret) {
		ERR_MSG("rsp_info_mempool init fail!");
		goto release_mempool;
	}

	total_subregion_count = hpb->subregions_per_lu;

	ufshpb_init_jobs(hpb);

	INIT_INFO("total_subregion_count: %d", total_subregion_count);
	for (region = 0, subregion_count = 0,
			total_subregion_count = hpb->subregions_per_lu
			; region < hpb->regions_per_lu;
			region++, total_subregion_count -= subregion_count) {
		cb = region_table + region;
		cb->region = region;

		/* init lru region information*/
		INIT_LIST_HEAD(&cb->list_region);
		cb->hit_count = 0;

		subregion_count = min(total_subregion_count,
			hpb->subregions_per_region);
		INIT_INFO("total: %d subregion_count: %d",
			  total_subregion_count, subregion_count);

		ret = ufshpb_alloc_subregion_tbl(hpb, cb, subregion_count);
		if (ret)
			goto release_region_cp;
		ufshpb_init_subregion_tbl(hpb, cb);

		if (ufshpb_is_HPBREGION_PINNED(lu_desc, region)) {
			INIT_INFO("region: %d PINNED %d ~ %d",
				  region, lu_desc->hpb_pinned_region_startidx,
				  lu_desc->lu_hpb_pinned_end_offset);
			ret = ufshpb_init_pinned_active_block(hpb, cb);
			if (ret)
				goto release_region_cp;

			do_work_handler = true;
		} else {
			INIT_INFO("region: %d inactive", cb->region);
			cb->region_state = HPBREGION_INACTIVE;
		}
	}

	if (total_subregion_count != 0) {
		ERR_MSG("error total_subregion_count: %d",
			total_subregion_count);
		goto release_region_cp;
	}

	if (do_work_handler)
		schedule_work(&hpb->ufshpb_work);

	/*
	 * even if creating sysfs failed, ufshpb could run normally.
	 * so we don't deal with error handling
	 */
	ufshpb_create_sysfs(hba, hpb);
	return 0;

release_region_cp:
	for (j = 0; j < region; j++) {
		cb = region_table + j;
		if (cb->subregion_tbl) {
			for (subregion = 0;
				subregion < cb->subregion_count;
				subregion++) {
				cp = cb->subregion_tbl + subregion;

				if (cp->mctx)
					ufshpb_put_map_ctx(hpb, cp->mctx);
			}
			kfree(cb->subregion_tbl);
		}
	}

release_mempool:
	ufshpb_req_mempool_remove(hpb);

	ufshpb_table_mempool_remove(hpb);

	kfree(region_table);
out:
	hpb->lu_hpb_enable = false;
	return ret;
}

static int ufshpb_get_hpb_lu_desc(struct ufs_hba *hba,
		struct ufshpb_lu_desc *lu_desc, int lun)
{
	int ret;
	u8 logical_buf[UFSHPB_QUERY_DESC_UNIT_MAX_SIZE] = { 0 };

	ret = ufshpb_read_unit_desc(hba, lun, logical_buf,
				UFSHPB_QUERY_DESC_UNIT_MAX_SIZE);
	if (ret) {
		ERR_MSG("read unit desc failed. ret %d", ret);
		return ret;
	}

	lu_desc->lu_queue_depth = logical_buf[UNIT_DESC_PARAM_LU_Q_DEPTH];

	/* 2^log, ex) 0x0C = 4KB */
	lu_desc->lu_logblk_size =
		logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_SIZE];
	lu_desc->lu_logblk_cnt =
		SHIFT_BYTE_7(
			(u64)logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT]) |
		SHIFT_BYTE_6(
			(u64)logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 1])
			|
		SHIFT_BYTE_5(
			(u64)logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 2])
			|
		SHIFT_BYTE_4(
			(u64)logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 3])
			|
		SHIFT_BYTE_3(
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 4]) |
		SHIFT_BYTE_2(
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 5]) |
		SHIFT_BYTE_1(
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 6]) |
		SHIFT_BYTE_0(
		logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 7]);

	if (logical_buf[UNIT_DESC_PARAM_LU_ENABLE] == 0x02)
		lu_desc->lu_hpb_enable = true;
	else
		lu_desc->lu_hpb_enable = false;

	INIT_INFO("LUN(%d) [02] bUnitIndex %d",
		  lun, logical_buf[UNIT_DESC_PARAM_UNIT_INDEX]);
	INIT_INFO("LUN(%d) [03] bLUEnable %d",
		  lun, logical_buf[UNIT_DESC_PARAM_LU_ENABLE]);

	lu_desc->lu_max_active_hpb_regions =
		SHIFT_BYTE_1(logical_buf[UNIT_DESC_HPB_LU_MAX_ACTIVE_REGIONS]) |
		SHIFT_BYTE_0(
			logical_buf[UNIT_DESC_HPB_LU_MAX_ACTIVE_REGIONS + 1]);
	lu_desc->hpb_pinned_region_startidx =
		SHIFT_BYTE_1(
			logical_buf[UNIT_DESC_HPB_LU_PIN_REGION_START_OFFSET]) |
		SHIFT_BYTE_0(
		logical_buf[UNIT_DESC_HPB_LU_PIN_REGION_START_OFFSET + 1]);
	lu_desc->lu_num_hpb_pinned_regions =
		SHIFT_BYTE_1(logical_buf[UNIT_DESC_HPB_LU_NUM_PIN_REGIONS]) |
		SHIFT_BYTE_0(logical_buf[UNIT_DESC_HPB_LU_NUM_PIN_REGIONS + 1]);

	INIT_INFO("LUN(%d) [0A] bLogicalBlockSize %d",
		  lun, lu_desc->lu_logblk_size);
	INIT_INFO("LUN(%d) [0B] qLogicalBlockCount %llu",
		  lun, lu_desc->lu_logblk_cnt);
	INIT_INFO("LUN(%d) [03] bLuEnable %d", lun, lu_desc->lu_hpb_enable);
	INIT_INFO("LUN(%d) [06] bLuQueueDepth %d",
		  lun, lu_desc->lu_queue_depth);
	INIT_INFO("LUN(%d) [23:24] wLUMaxActiveHPBRegions %d",
		  lun, lu_desc->lu_max_active_hpb_regions);
	INIT_INFO("LUN(%d) [25:26] wHPBPinnedRegionStartIdx %d",
		  lun, lu_desc->hpb_pinned_region_startidx);
	INIT_INFO("LUN(%d) [27:28] wNumHPBPinnedRegions %d",
		  lun, lu_desc->lu_num_hpb_pinned_regions);

	if (lu_desc->lu_num_hpb_pinned_regions > 0) {
		lu_desc->lu_hpb_pinned_end_offset =
			lu_desc->hpb_pinned_region_startidx +
			lu_desc->lu_num_hpb_pinned_regions - 1;
	} else
		lu_desc->lu_hpb_pinned_end_offset = -1;

	INIT_INFO("LUN(%d) PINNED Start %d End %d",
		  lun, lu_desc->hpb_pinned_region_startidx,
		  lu_desc->lu_hpb_pinned_end_offset);

	return 0;
}

static int ufshpb_read_dev_desc_support(struct ufs_hba *hba,
		struct ufshpb_func_desc *desc)
{
	u8 desc_buf[UFSHPB_QUERY_DESC_DEVICE_MAX_SIZE];
	int err;

	err = ufshpb_read_device_desc(hba, desc_buf,
			UFSHPB_QUERY_DESC_DEVICE_MAX_SIZE);
	if (err)
		return err;

	if (desc_buf[DEVICE_DESC_PARAM_FEAT_SUP] &
			UFS_FEATURE_SUPPORT_HPB_BIT) {
		INIT_INFO("bUFSFeaturesSupport: HPB is set");
	} else {
		INIT_INFO("bUFSFeaturesSupport: HPB not support");
		return -ENODEV;
	}

	desc->lu_cnt = desc_buf[DEVICE_DESC_PARAM_NUM_LU];
	INIT_INFO("device lu count %d", desc->lu_cnt);

	desc->hpb_ver =
		(u16) SHIFT_BYTE_1(desc_buf[DEVICE_DESC_PARAM_HPB_VER]) |
		(u16) SHIFT_BYTE_0(desc_buf[DEVICE_DESC_PARAM_HPB_VER + 1]);

	INIT_INFO("HPB Version = %.2x %.2x",
		  GET_BYTE_1(desc->hpb_ver), GET_BYTE_0(desc->hpb_ver));
	return 0;
}

static int ufshpb_read_geo_desc_support(struct ufs_hba *hba,
		struct ufshpb_func_desc *desc)
{
	int err;
	u8 geometry_buf[UFSHPB_QUERY_DESC_GEOMETRY_MAX_SIZE];

	err = ufshpb_read_geo_desc(hba, geometry_buf,
				UFSHPB_QUERY_DESC_GEOMETRY_MAX_SIZE);
	if (err)
		return err;

	desc->hpb_region_size = geometry_buf[GEOMETRY_DESC_HPB_REGION_SIZE];
	desc->hpb_number_lu = geometry_buf[GEOMETRY_DESC_HPB_NUMBER_LU];
	desc->hpb_subregion_size =
		geometry_buf[GEOMETRY_DESC_HPB_SUBREGION_SIZE];
	desc->hpb_device_max_active_regions =
		(u16) SHIFT_BYTE_1(
		geometry_buf[GEOMETRY_DESC_HPB_DEVICE_MAX_ACTIVE_REGIONS]) |
		(u16) SHIFT_BYTE_0(
		geometry_buf[GEOMETRY_DESC_HPB_DEVICE_MAX_ACTIVE_REGIONS + 1]);

	INIT_INFO("[48] bHPBRegionSiz %u", desc->hpb_region_size);
	INIT_INFO("[49] bHPBNumberLU %u", desc->hpb_number_lu);
	INIT_INFO("[4A] bHPBSubRegionSize %u", desc->hpb_subregion_size);
	INIT_INFO("[4B:4C] wDeviceMaxActiveHPBRegions %u",
		  desc->hpb_device_max_active_regions);

	if (desc->hpb_number_lu == 0) {
		dev_warn(hba->dev, "UFSHPB) HPB is not supported\n");
		return -ENODEV;
	}

	return 0;
}

static int ufshpb_init(struct ufs_hba *hba)
{
	struct ufshpb_func_desc func_desc;
	int lun, ret, i;
	int hpb_dev = 0;

	ret = ufshpb_read_dev_desc_support(hba, &func_desc);
	if (ret || func_desc.hpb_ver != UFSHPB_VER) {
		INIT_INFO("Driver = %.2x %.2x, Device = %.2x %.2x",
			  GET_BYTE_1(UFSHPB_VER), GET_BYTE_0(UFSHPB_VER),
			  GET_BYTE_1(func_desc.hpb_ver),
			  GET_BYTE_0(func_desc.hpb_ver));
		goto out_state;
	}

	ret = ufshpb_read_geo_desc_support(hba, &func_desc);
	if (ret)
		goto out_state;

	ufshpb_init_constant();

	for (lun = 0; lun < UFS_UPIU_MAX_GENERAL_LUN; lun++) {
		struct ufshpb_lu_desc lu_desc;
		int ret;

		hba->ufshpb_lup[lun] = NULL;

		ret = ufshpb_get_hpb_lu_desc(hba, &lu_desc, lun);
		if (ret)
			goto out_state;

		if (lu_desc.lu_hpb_enable == false) {
			INIT_INFO("===== LU %d is hpb-disabled.", lun);
			continue;
		}

		hba->ufshpb_lup[lun] = kzalloc(sizeof(struct ufshpb_lu),
			GFP_KERNEL);
		if (!hba->ufshpb_lup[lun])
			goto out_free_mem;

		ret = ufshpb_lu_hpb_init(hba, hba->ufshpb_lup[lun],
				&func_desc, &lu_desc, lun);
		if (ret) {
			if (ret == -ENODEV)
				continue;
			else
				goto out_free_mem;
		}
		hpb_dev++;
	}

	if (hpb_dev == 0) {
		dev_warn(hba->dev, "No UFSHPB LU to init\n");
		ret = -ENODEV;
		goto out_free_mem;
	}

	INIT_WORK(&hba->ufshpb_eh_work, ufshpb_error_handler);
	hba->ufshpb_state = HPB_PRESENT;
	hba->issue_ioctl = false;

	for (lun = 0; lun < UFS_UPIU_MAX_GENERAL_LUN; lun++)
		if (hba->ufshpb_lup[lun])
			dev_info(hba->dev, "UFSHPB LU %d working\n", lun);

	return 0;
out_free_mem:
	for (i = 0; i < UFS_UPIU_MAX_GENERAL_LUN; i++)
		kfree(hba->ufshpb_lup[i]);
out_state:
	hba->ufshpb_state = HPB_NOT_SUPPORTED;
	return ret;
}

static void ufshpb_map_loading_trigger(struct ufshpb_lu *hpb,
		bool dirty, bool only_pinned)
{
	int region, subregion;
	bool do_work_handler = false;

	for (region = 0; region < hpb->regions_per_lu; region++) {
		struct ufshpb_region *cb;

		cb = hpb->region_tbl + region;

		if (cb->region_state == HPBREGION_ACTIVE ||
			cb->region_state == HPBREGION_PINNED) {
			SYSFS_INFO("add active block number %d state %d",
					   region, cb->region_state);
			if ((only_pinned && cb->region_state ==
				HPBREGION_PINNED) ||
				!only_pinned) {
				spin_lock_bh(&hpb->hpb_lock);
				for (subregion = 0;
					subregion < cb->subregion_count;
					subregion++) {
					ufshpb_add_subregion_to_req_list(hpb,
						cb->subregion_tbl + subregion);
				}
				spin_unlock_bh(&hpb->hpb_lock);
				do_work_handler = true;
			}

			if (dirty) {
				for (subregion = 0;
				subregion < cb->subregion_count;
				subregion++)
				cb->subregion_tbl[subregion].subregion_state =
					HPBSUBREGION_DIRTY;
			}

		}
	}

	if (do_work_handler)
		schedule_work(&hpb->ufshpb_work);
}

static void ufshpb_purge_active_block(struct ufshpb_lu *hpb)
{
	int region, subregion;
	int state;
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;

	spin_lock_bh(&hpb->hpb_lock);
	for (region = 0; region < hpb->regions_per_lu; region++) {
		cb = hpb->region_tbl + region;

		if (cb->region_state == HPBREGION_INACTIVE) {
			HPB_DEBUG(hpb, "region %d inactive", region);
			continue;
		}

		if (cb->region_state == HPBREGION_PINNED) {
			state = HPBSUBREGION_DIRTY;
		} else if (cb->region_state == HPBREGION_ACTIVE) {
			state = HPBSUBREGION_UNUSED;
			ufshpb_cleanup_lru_info(&hpb->lru_info, cb);
		} else {
			HPB_DEBUG(hpb, "Unsupported state of region");
			continue;
		}


		HPB_DEBUG(hpb, "region %d state %d dft %d", region, state,
			  hpb->debug_free_table);
		for (subregion = 0;
			subregion < cb->subregion_count;
			subregion++) {
			cp = cb->subregion_tbl + subregion;

			ufshpb_purge_active_subregion(hpb, cp, state);
		}
		HPB_DEBUG(hpb, "region %d state %d dft %d", region, state,
			  hpb->debug_free_table);
	}
	spin_unlock_bh(&hpb->hpb_lock);
}

static void ufshpb_retrieve_rsp_info(struct ufshpb_lu *hpb)
{
	struct ufshpb_rsp_info *rsp_info;
	unsigned long flags;

	while (1) {
		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		rsp_info = list_first_entry_or_null(&hpb->lh_rsp_info,
				struct ufshpb_rsp_info, list_rsp_info);
		if (!rsp_info) {
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
			break;
		}

		HPB_DEBUG(hpb, "add_list %p -> %p", &hpb->lh_rsp_info_free,
			  rsp_info);
		list_move_tail(&rsp_info->list_rsp_info,
			       &hpb->lh_rsp_info_free);

		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
	}
}

static void ufshpb_probe(struct ufs_hba *hba)
{
	struct ufshpb_lu *hpb;
	int lu;

	for (lu = 0; lu < UFS_UPIU_MAX_GENERAL_LUN; lu++) {
		hpb = hba->ufshpb_lup[lu];
		if (hpb && hpb->lu_hpb_enable) {
			dev_info(hba->dev, "UFSHPB lun %d reset\n", lu);

			ufshpb_cancel_jobs(hpb);

			ufshpb_retrieve_rsp_info(hpb);

			ufshpb_purge_active_block(hpb);

			tasklet_init(&hpb->ufshpb_tasklet, ufshpb_tasklet_fn,
				(unsigned long)hpb);
		}
	}

	hba->ufshpb_state = HPB_PRESENT;
}

static void ufshpb_destroy_subregion_tbl(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb)
{
	int subregion;

	for (subregion = 0; subregion < cb->subregion_count; subregion++) {
		struct ufshpb_subregion *cp;

		cp = cb->subregion_tbl + subregion;

		RELEASE_INFO("cp %d %p state %d mctx %p",
			     subregion, cp, cp->subregion_state, cp->mctx);

		cp->subregion_state = HPBSUBREGION_UNUSED;

		ufshpb_put_map_ctx(hpb, cp->mctx);
	}

	kfree(cb->subregion_tbl);
}

static void ufshpb_destroy_region_tbl(struct ufshpb_lu *hpb)
{
	int region;

	for (region = 0; region < hpb->regions_per_lu; region++) {
		struct ufshpb_region *cb;

		cb = hpb->region_tbl + region;
		RELEASE_INFO("region %d %p state %d", region, cb,
			     cb->region_state);

		if (cb->region_state == HPBREGION_PINNED ||
				cb->region_state == HPBREGION_ACTIVE) {
			cb->region_state = HPBREGION_INACTIVE;

			ufshpb_destroy_subregion_tbl(hpb, cb);
		}
	}

	ufshpb_table_mempool_remove(hpb);
	kfree(hpb->region_tbl);
}

void ufshpb_release(struct ufs_hba *hba, int state)
{
	int lun;

	RELEASE_INFO("start release");
	hba->ufshpb_state = HPB_FAILED;

	for (lun = 0; lun < UFS_UPIU_MAX_GENERAL_LUN; lun++) {
		struct ufshpb_lu *hpb = hba->ufshpb_lup[lun];

		RELEASE_INFO("lun %d %p", lun, hpb);

		hba->ufshpb_lup[lun] = NULL;

		if (!hpb)
			continue;

		if (!hpb->lu_hpb_enable)
			continue;

		hpb->lu_hpb_enable = false;

		ufshpb_cancel_jobs(hpb);

		ufshpb_destroy_region_tbl(hpb);

		ufshpb_req_mempool_remove(hpb);

		kobject_uevent(&hpb->kobj, KOBJ_REMOVE);
		kobject_del(&hpb->kobj); // TODO count ?®ì?ê³?del?

		kfree(hpb);
	}

	if (alloc_mctx != 0)
		WARNING_MSG("warning: alloc_mctx %d", alloc_mctx);

	hba->ufshpb_state = state;
}

void ufshpb_init_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	struct delayed_work *dwork = to_delayed_work(work);
	int err;
#if defined(CONFIG_SCSI_SCAN_ASYNC)
	unsigned long flags;
#endif

	INIT_INFO("INIT Handler called");
	hba = container_of(dwork, struct ufs_hba, ufshpb_init_work);

#if defined(CONFIG_SCSI_SCAN_ASYNC)
	mutex_lock(&hba->host->scan_mutex);
	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->host->async_scan == 1) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		mutex_unlock(&hba->host->scan_mutex);
		INIT_INFO("Not set scsi-device-info. So re-sched.");
		schedule_delayed_work(&hba->ufshpb_init_work,
			msecs_to_jiffies(100));
		return;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	mutex_unlock(&hba->host->scan_mutex);
#endif

	if (hba->ufshpb_state == HPB_NEED_INIT) {
		pr_info("%s:%d HPB_INIT_START\n", __func__, __LINE__);
		err = ufshpb_init(hba);
		if (err) {
			dev_warn(hba->dev,
"UFSHPB driver initialization failed - UFSHCD will run without UFSHPB\n");
			dev_warn(hba->dev, "UFSHPB error num : %d\n", err);
		}
	} else if (hba->ufshpb_state == HPB_RESET) {
		ufshpb_probe(hba);
	}
}

static void ufshpb_error_handler(struct work_struct *work)
{
	struct ufs_hba *hba;

	hba = container_of(work, struct ufs_hba, ufshpb_eh_work);

	dev_warn(hba->dev,
	"UFSHPB driver has failed - but UFSHCD can run without UFSHPB\n");
	dev_warn(hba->dev, "UFSHPB will be removed from the kernel\n");

	ufshpb_release(hba, HPB_FAILED);
}

static void ufshpb_stat_init(struct ufshpb_lu *hpb)
{
	atomic64_set(&hpb->hit, 0);
	atomic64_set(&hpb->miss, 0);
	atomic64_set(&hpb->region_miss, 0);
	atomic64_set(&hpb->subregion_miss, 0);
	atomic64_set(&hpb->entry_dirty_miss, 0);
	atomic64_set(&hpb->rb_noti_cnt, 0);
	atomic64_set(&hpb->map_req_cnt, 0);
	atomic64_set(&hpb->region_evict, 0);
	atomic64_set(&hpb->region_add, 0);
	atomic64_set(&hpb->rb_fail, 0);
}

static ssize_t ufshpb_sysfs_debug_release_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value;

	SYSFS_INFO("start release function");

	if (kstrtoul(buf, 0, &value)) {
		ERR_MSG("kstrtoul error");
		return -EINVAL;
	}

	if (value == 0xab) {
		SYSFS_INFO("magic number %lu release start", value);
		goto err_out;
	} else {
		SYSFS_INFO("wrong magic number %lu", value);
	}

	return count;
err_out:
	hpb->hba->ufshpb_state = HPB_FAILED;
	schedule_work(&hpb->hba->ufshpb_eh_work);
	return count;
}

static ssize_t ufshpb_sysfs_info_lba_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long long ppn;
	unsigned long value;
	unsigned int lpn;
	int region, subregion, subregion_offset;
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;
	int dirty;

	if (kstrtoul(buf, 0, &value)) {
		ERR_MSG("kstrtoul error");
		return -EINVAL;
	}

	if (value > hpb->lu_num_blocks * SECTORS_PER_BLOCK) {
		ERR_MSG("value %lu > lu_num_blocks %d error",
			value, hpb->lu_num_blocks);
		return -EINVAL;
	}
	lpn = value / SECTORS_PER_BLOCK;

	ufshpb_get_pos_from_lpn(hpb, lpn, &region, &subregion,
		&subregion_offset);

	cb = hpb->region_tbl + region;
	cp = cb->subregion_tbl + subregion;

	if (cb->region_state != HPBREGION_INACTIVE) {
		ppn = ufshpb_get_ppn(cp->mctx, subregion_offset);
		spin_lock_bh(&hpb->hpb_lock);
		dirty = ufshpb_ppn_dirty_check(hpb, cp, subregion_offset);
		spin_unlock_bh(&hpb->hpb_lock);
	} else {
		ppn = 0;
		dirty = -1;
	}

	SYSFS_INFO("sector %lu region %d state %d subregion %d state %d",
		   value, region, cb->region_state, subregion,
		   cp->subregion_state);
	SYSFS_INFO("sector %lu lpn %u ppn %llx dirty %d",
		   value, lpn, ppn, dirty);
	TMSG(hpb, "%s:%d sector %lu lpn %u ppn %llx dirty %d",
			__func__, __LINE__, value, lpn, ppn, dirty);
	return count;
}

static ssize_t ufshpb_sysfs_map_req_show(struct ufshpb_lu *hpb, char *buf)
{
	long long rb_noti_cnt, map_req_cnt;

	rb_noti_cnt = atomic64_read(&hpb->rb_noti_cnt);
	map_req_cnt = atomic64_read(&hpb->map_req_cnt);

	SYSFS_INFO("rb_noti conunt %lld map_req count %lld",
			   rb_noti_cnt, map_req_cnt);

	return snprintf(buf, PAGE_SIZE,
		"rb_noti conunt %lld map_req count %lld\n",
		rb_noti_cnt, map_req_cnt);
}

static ssize_t ufshpb_sysfs_count_reset_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long debug;

	if (kstrtoul(buf, 0, &debug))
		return -EINVAL;

	ufshpb_stat_init(hpb);

	return count;
}

static ssize_t ufshpb_sysfs_add_evict_show(struct ufshpb_lu *hpb, char *buf)
{
	long long add, evict;

	add = atomic64_read(&hpb->region_add);
	evict = atomic64_read(&hpb->region_evict);

	SYSFS_INFO("add %lld evict %lld", add, evict);

	return snprintf(buf, PAGE_SIZE, "add %lld evict %lld\n", add, evict);
}

static ssize_t ufshpb_sysfs_hit_show(struct ufshpb_lu *hpb, char *buf)
{
	long long hit;

	hit = atomic64_read(&hpb->hit);

	SYSFS_INFO("hit %lld", hit);

	return snprintf(buf, PAGE_SIZE, "%lld\n", hit);
}

static ssize_t ufshpb_sysfs_miss_show(struct ufshpb_lu *hpb, char *buf)
{
	long long region_miss, subregion_miss, entry_dirty_miss, rb_fail;

	region_miss = atomic64_read(&hpb->region_miss);
	subregion_miss = atomic64_read(&hpb->subregion_miss);
	entry_dirty_miss = atomic64_read(&hpb->entry_dirty_miss);
	rb_fail = atomic64_read(&hpb->rb_fail);

	SYSFS_INFO("Total : %lld, region_miss %lld, subregion_miss %lld",
		   region_miss + subregion_miss + entry_dirty_miss,
		   region_miss, subregion_miss);

	SYSFS_INFO("entry_dirty_miss %lld, rb_fail %lld",
		   entry_dirty_miss, rb_fail);

	return snprintf(buf, PAGE_SIZE,
	"Total: %lld, region %lld subregion %lld entry %lld rb_fail %lld\n",
			region_miss + subregion_miss + entry_dirty_miss,
			region_miss, subregion_miss, entry_dirty_miss, rb_fail);
}

static ssize_t ufshpb_sysfs_version_show(struct ufshpb_lu *hpb, char *buf)
{
	SYSFS_INFO("HPB version %.2x %.2x, D/D version %.2x %.2x\n",
		   GET_BYTE_1(hpb->hpb_ver), GET_BYTE_0(hpb->hpb_ver),
		   GET_BYTE_1(UFSHPB_DD_VER), GET_BYTE_0(UFSHPB_DD_VER));

	return snprintf(buf, PAGE_SIZE,
			"HPB version %.2x %.2x, D/D version %.2x %.2x\n",
			GET_BYTE_1(hpb->hpb_ver), GET_BYTE_0(hpb->hpb_ver),
			GET_BYTE_1(UFSHPB_DD_VER), GET_BYTE_0(UFSHPB_DD_VER));
}

static ssize_t ufshpb_sysfs_active_list_show(struct ufshpb_lu *hpb, char *buf)
{
	int ret = 0, count = 0;
	struct ufshpb_region *region;

	list_for_each_entry(region, &hpb->lru_info.lru, list_region) {
		ret = sprintf(buf + count, "%d(cnt=%d) ",
			      region->region, region->hit_count);
		count += ret;
	}
	ret = sprintf(buf + count, "\n");
	count += ret;

	return count;
}

static ssize_t ufshpb_sysfs_active_block_status_show(struct ufshpb_lu *hpb,
	char *buf)
{
	int ret = 0, count = 0, region;

	ret = sprintf(buf, "PINNED=%d ACTIVE=%d INACTIVE=%d\n",
			HPBREGION_PINNED, HPBREGION_ACTIVE, HPBREGION_INACTIVE);
	count = ret;

	for (region = 0; region < hpb->regions_per_lu; region++) {
		ret = sprintf(buf + count, "%d:%d ", region,
				hpb->region_tbl[region].region_state);
		count += ret;
	}

	ret = sprintf(buf + count, "\n");
	count += ret;

	return count;
}

static ssize_t ufshpb_sysfs_subregion_status_show(
			  struct ufshpb_lu *hpb, char *buf)
{
	int ret = 0, count = 0, region, sub;

	ret = sprintf(buf, "PINNED=%d ACTIVE=%d INACTIVE=%d\n",
		      HPBREGION_PINNED, HPBREGION_ACTIVE, HPBREGION_INACTIVE);
	count = ret;

	for (region = 0; region < hpb->regions_per_lu; region++) {
		ret = sprintf(buf + count, "%d:%d ", region,
			      hpb->region_tbl[region].region_state);
		pr_info("\nregion %d state %d\n", region,
		       hpb->region_tbl[region].region_state);

		for (sub = 0; sub < hpb->region_tbl[region].subregion_count;
		     sub++) {
			pr_info("region %d subregion %d state %d\n",
			       region, hpb->region_tbl[region].region_state,
			       hpb->region_tbl[region].subregion_tbl[sub].
			       subregion_state);
		}
		count += ret;
	}

	ret = sprintf(buf + count, "\n");
	count += ret;

	return count;
}

static ssize_t ufshpb_sysfs_debug_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long debug;

	if (kstrtoul(buf, 0, &debug))
		return -EINVAL;

	if (debug >= 1)
		hpb->debug = 1;
	else
		hpb->debug = 0;

	SYSFS_INFO("debug %d", hpb->debug);
	return count;
}

static ssize_t ufshpb_sysfs_debug_show(struct ufshpb_lu *hpb, char *buf)
{
	SYSFS_INFO("debug %d", hpb->debug);

	return snprintf(buf, PAGE_SIZE, "%d\n",	hpb->debug);
}

static ssize_t ufshpb_sysfs_map_loading_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value;

	SYSFS_INFO("");

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 1)
		return -EINVAL;

	SYSFS_INFO("value %lu", value);

	if (value == 1)
		ufshpb_map_loading_trigger(hpb, false, false);

	return count;

}

static ssize_t ufshpb_sysfs_map_disable_show(struct ufshpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			">> force_map_req_disable: %d\n",
			hpb->force_map_req_disable);
}

static ssize_t ufshpb_sysfs_map_disable_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 1)
		value = 1;

	if (value == 1)
		hpb->force_map_req_disable = true;
	else if (value == 0)
		hpb->force_map_req_disable = false;
	else
		ERR_MSG("error value: %lu", value);

	SYSFS_INFO("force_map_req_disable: %d", hpb->force_map_req_disable);

	return count;
}

static ssize_t ufshpb_sysfs_disable_show(struct ufshpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			">> force_disable: %d\n", hpb->force_disable);
}

static ssize_t ufshpb_sysfs_disable_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 1)
		value = 1;

	if (value == 1)
		hpb->force_disable = true;
	else if (value == 0)
		hpb->force_disable = false;
	else
		ERR_MSG("error value: %lu", value);

	SYSFS_INFO("force_disable: %d", hpb->force_disable);

	return count;
}

static int global_region;

static inline bool is_region_active(struct ufshpb_lu *hpb, int region)
{
	if (hpb->region_tbl[region].region_state == HPBREGION_ACTIVE ||
		hpb->region_tbl[region].region_state == HPBREGION_PINNED)
		return true;

	return false;
}

static ssize_t ufshpb_sysfs_active_group_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long block;
	int region;

	if (kstrtoul(buf, 0, &block))
		return -EINVAL;

	region = block >> hpb->entries_per_region_shift;
	if (region >= hpb->regions_per_lu) {
		ERR_MSG("error region %d max %d", region, hpb->regions_per_lu);
		region = hpb->regions_per_lu - 1;
	}

	global_region = region;

	SYSFS_INFO("block %lu region %d active %d",
		   block, region, is_region_active(hpb, region));

	return count;
}

static ssize_t ufshpb_sysfs_active_group_show(struct ufshpb_lu *hpb, char *buf)
{
	SYSFS_INFO("region %d active %d",
		   global_region, is_region_active(hpb, global_region));

	return snprintf(buf, PAGE_SIZE,
		"%d\n",	is_region_active(hpb, global_region));
}


#ifdef UFSHPB_ERROR_INJECT
static ssize_t ufshpb_sysfs_err_inject_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long debug;

	if (kstrtoul(buf, 0, &debug))
		return -EINVAL;

	if (debug >= 1)
		hpb->err_inject = 1;
	else
		hpb->err_inject = 0;

	SYSFS_INFO("err_inject %d", hpb->err_inject);
	return count;
}

static ssize_t ufshpb_sysfs_err_inject_show(struct ufshpb_lu *hpb, char *buf)
{
	SYSFS_INFO("err_inject %d", hpb->err_inject);

	return snprintf(buf, PAGE_SIZE, "%d\n",	hpb->err_inject);
}
#endif

static struct ufshpb_sysfs_entry ufshpb_sysfs_entries[] = {
	__ATTR(is_active_group, 0444 | 0200,
	       ufshpb_sysfs_active_group_show, ufshpb_sysfs_active_group_store),
	__ATTR(read_16_disable, 0444 | 0200,
	       ufshpb_sysfs_disable_show, ufshpb_sysfs_disable_store),
	__ATTR(map_cmd_disable, 0444 | 0200,
	       ufshpb_sysfs_map_disable_show, ufshpb_sysfs_map_disable_store),
	__ATTR(map_loading, 0200, NULL, ufshpb_sysfs_map_loading_store),
	__ATTR(debug, 0444 | 0200,
	       ufshpb_sysfs_debug_show, ufshpb_sysfs_debug_store),
	__ATTR(active_block_status, 0444,
	       ufshpb_sysfs_active_block_status_show, NULL),
	__ATTR(subregion_status, 0444,
	       ufshpb_sysfs_subregion_status_show, NULL),
	__ATTR(HPBVersion, 0444, ufshpb_sysfs_version_show, NULL),
	__ATTR(hit_count, 0444, ufshpb_sysfs_hit_show, NULL),
	__ATTR(miss_count, 0444, ufshpb_sysfs_miss_show, NULL),
	__ATTR(active_list, 0444, ufshpb_sysfs_active_list_show, NULL),
	__ATTR(add_evict_count, 0444, ufshpb_sysfs_add_evict_show, NULL),
	__ATTR(count_reset, 0200, NULL, ufshpb_sysfs_count_reset_store),
	__ATTR(map_req_count, 0444, ufshpb_sysfs_map_req_show, NULL),
	__ATTR(get_info_from_lba, 0200, NULL, ufshpb_sysfs_info_lba_store),
	__ATTR(release, 0200, NULL, ufshpb_sysfs_debug_release_store),
#ifdef UFSHPB_ERROR_INJECT
	__ATTR(err_inject, 0444 | 0200,
	       ufshpb_sysfs_err_inject_show, ufshpb_sysfs_err_inject_store),
#endif
	__ATTR_NULL
};

static ssize_t
ufshpb_attr_show(struct kobject *kobj,
		struct attribute *attr, char *page)
{
	struct ufshpb_sysfs_entry *entry;
	struct ufshpb_lu *hpb;
	ssize_t error;

	entry = container_of(attr,
			struct ufshpb_sysfs_entry, attr);
	hpb = container_of(kobj, struct ufshpb_lu, kobj);

	if (!entry->show)
		return -EIO;

	mutex_lock(&hpb->sysfs_lock);
	error = entry->show(hpb, page);
	mutex_unlock(&hpb->sysfs_lock);
	return error;
}

static ssize_t
ufshpb_attr_store(struct kobject *kobj,
		struct attribute *attr,
		const char *page, size_t length)
{
	struct ufshpb_sysfs_entry *entry;
	struct ufshpb_lu *hpb;
	ssize_t error;

	entry = container_of(attr,
			struct ufshpb_sysfs_entry, attr);
	hpb = container_of(kobj,
			struct ufshpb_lu, kobj);

	if (!entry->store)
		return -EIO;

	mutex_lock(&hpb->sysfs_lock);
	error = entry->store(hpb, page, length);
	mutex_unlock(&hpb->sysfs_lock);
	return error;
}

static const struct sysfs_ops ufshpb_sysfs_ops = {
	.show = ufshpb_attr_show,
	.store = ufshpb_attr_store,
};

static struct kobj_type ufshpb_ktype = {
	.sysfs_ops = &ufshpb_sysfs_ops,
	.release = NULL,
};

static int ufshpb_create_sysfs(struct ufs_hba *hba,
		struct ufshpb_lu *hpb)
{
	struct device *dev = hba->dev;
	struct ufshpb_sysfs_entry *entry;
	int err;

	hpb->sysfs_entries = ufshpb_sysfs_entries;

	ufshpb_stat_init(hpb);

	kobject_init(&hpb->kobj, &ufshpb_ktype);
	mutex_init(&hpb->sysfs_lock);

	INIT_INFO("ufshpb creates sysfs ufshpb_lu %d %p dev->kobj %p",
		  hpb->lun, &hpb->kobj, &dev->kobj);

	err = kobject_add(&hpb->kobj, kobject_get(&dev->kobj),
			"ufshpb_lu%d", hpb->lun);
	if (!err) {
		for (entry = hpb->sysfs_entries;
				entry->attr.name != NULL; entry++) {
			INIT_INFO("ufshpb_lu%d sysfs attr creates: %s",
				  hpb->lun, entry->attr.name);
			if (sysfs_create_file(&hpb->kobj, &entry->attr))
				break;
		}
		INIT_INFO("ufshpb_lu%d sysfs adds uevent", hpb->lun);
		kobject_uevent(&hpb->kobj, KOBJ_ADD);
	}

	return err;
}
