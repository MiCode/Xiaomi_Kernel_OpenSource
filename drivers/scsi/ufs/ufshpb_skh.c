// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
 * Modified work Copyright (C) 2018, Google, Inc.
 * Modified work Copyright (C) 2019 SK hynix
 */

#include <linux/slab.h>
#include <linux/blkdev.h>
#include <scsi/scsi.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>

#include "../../../block/blk.h"

#include "ufs.h"
#include "ufshcd.h"
#include "ufshpb_skh.h"
#include <asm/unaligned.h>

u32 skhpb_debug_mask = SKHPB_LOG_ERR | SKHPB_LOG_INFO;
//u32 skhpb_debug_mask = SKHPB_LOG_ERR | SKHPB_LOG_INFO | SKHPB_LOG_DEBUG | SKHPB_LOG_HEX;
int debug_map_req = SKHPB_MAP_RSP_DISABLE;

/*
 * debug variables
 */
static int skhpb_alloc_mctx;

/*
 * define global constants
 */
static int skhpb_sects_per_blk_shift;
static int skhpb_bits_per_dword_shift;
static int skhpb_bits_per_dword_mask;

static int skhpb_create_sysfs(struct ufs_hba *hba, struct skhpb_lu *hpb);
static int skhpb_check_lru_evict(struct skhpb_lu *hpb, struct skhpb_region *cb);
static void skhpb_error_handler(struct work_struct *work);
static void skhpb_evict_region(struct skhpb_lu *hpb,
						struct skhpb_region *cb);
static void skhpb_purge_active_block(struct skhpb_lu *hpb);
static int skhpb_set_map_req(struct skhpb_lu *hpb,
		int region, int subregion, struct skhpb_map_ctx *mctx,
		struct skhpb_rsp_info *rsp_info,
		enum SKHPB_BUFFER_MODE flag);
static int skhpb_rsp_map_cmd_req(struct skhpb_lu *hpb,
		struct skhpb_rsp_info *rsp_info);
static void skhpb_map_loading_trigger(struct skhpb_lu *hpb,
		bool only_pinned, bool do_work_handler);

static inline void skhpb_purge_active_page(struct skhpb_lu *hpb,
		struct skhpb_subregion *cp, int state);

static void skhpb_hit_lru_info(struct skhpb_victim_select_info *lru_info,
		struct skhpb_region *cb);

static inline void skhpb_get_bit_offset(
		struct skhpb_lu *hpb, int subregion_offset,
		int *dword, int *offset)
{
	*dword = subregion_offset >> skhpb_bits_per_dword_shift;
	*offset = subregion_offset & skhpb_bits_per_dword_mask;
}

/* called with hpb_lock (irq) */
static bool skhpb_ppn_dirty_check(struct skhpb_lu *hpb,
		struct skhpb_subregion *cp, int subregion_offset)
{
	bool is_dirty = false;
	unsigned int bit_dword, bit_offset;

	if (!cp->mctx->ppn_dirty)
		return true;

	skhpb_get_bit_offset(hpb, subregion_offset,
			&bit_dword, &bit_offset);
	is_dirty = cp->mctx->ppn_dirty[bit_dword] & (1 << bit_offset) ? true : false;

	return is_dirty;
}

static void skhpb_ppn_prep(struct skhpb_lu *hpb,
		struct ufshcd_lrb *lrbp, skhpb_t ppn,
		unsigned int sector_len)
{
	unsigned char *cmd = lrbp->cmd->cmnd;

	if (hpb->hpb_ver < 0x0200) {
		cmd[0] = READ_16;
		cmd[1] = 0x0;
		cmd[15] = 0x01; //NAND V5,V6 : Use Control field for HPB_READ
	} else {
		cmd[0] = READ_16;
		cmd[1] = 0x5; // NAND V7 : Use reseved fields for HPB_READ
		cmd[15] = 0x0;
	}
	put_unaligned(ppn, (u64 *)&cmd[6]);
	cmd[14] = (u8)(sector_len >> skhpb_sects_per_blk_shift); //Transfer length
	lrbp->cmd->cmd_len = MAX_CDB_SIZE;
	//To verify the values within READ command
	/* SKHPB_DRIVER_HEXDUMP("[HPB] HPB READ ", 16, 1, cmd, sizeof(cmd), 1); */
}

static inline void skhpb_set_dirty_bits(struct skhpb_lu *hpb,
		struct skhpb_region *cb, struct skhpb_subregion *cp,
		int dword, int offset, unsigned int count)
{
	const unsigned long mask = ((1UL << count) - 1) & 0xffffffff;

	if (cb->region_state == SKHPB_REGION_INACTIVE)
		return;

	BUG_ON(!cp->mctx);
	cp->mctx->ppn_dirty[dword] |= (mask << offset);
}

/* called with hpb_lock (irq) */
static void skhpb_set_dirty(struct skhpb_lu *hpb,
			struct ufshcd_lrb *lrbp, int region,
			int subregion, int subregion_offset)
{
	struct skhpb_region *cb;
	struct skhpb_subregion *cp;
	int count;
	int bit_count, bit_dword, bit_offset;

	count = blk_rq_sectors(lrbp->cmd->request) >> skhpb_sects_per_blk_shift;
	skhpb_get_bit_offset(hpb, subregion_offset,
			&bit_dword, &bit_offset);

	do {
		bit_count = min(count, SKHPB_BITS_PER_DWORD - bit_offset);

		cb = hpb->region_tbl + region;
		cp = cb->subregion_tbl + subregion;

		skhpb_set_dirty_bits(hpb, cb, cp,
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
}

#if 0
static inline bool skhpb_is_encrypted_lrbp(struct ufshcd_lrb *lrbp)
{
	return (lrbp->utr_descriptor_ptr->header.dword_0 & UTRD_CRYPTO_ENABLE);
}
#endif

static inline enum SKHPB_CMD skhpb_get_cmd(struct ufshcd_lrb *lrbp)
{
	unsigned char cmd = lrbp->cmd->cmnd[0];

	if (cmd == READ_10 || cmd == READ_16)
		return SKHPB_CMD_READ;
	if (cmd == WRITE_10 || cmd == WRITE_16)
		return SKHPB_CMD_WRITE;
	if (cmd == UNMAP)
		return SKHPB_CMD_DISCARD;
	return SKHPB_CMD_OTHERS;
}

static inline void skhpb_get_pos_from_lpn(struct skhpb_lu *hpb,
		unsigned int lpn, int *region, int *subregion, int *offset)
{
	int region_offset;

	*region = lpn >> hpb->entries_per_region_shift;
	region_offset = lpn & hpb->entries_per_region_mask;
	*subregion = region_offset >> hpb->entries_per_subregion_shift;
	*offset = region_offset & hpb->entries_per_subregion_mask;
}

static inline bool skhpb_check_region_subregion_validity(struct skhpb_lu *hpb,
		int region, int subregion)
{
	struct skhpb_region *cb;

	if (region >= hpb->regions_per_lu) {
		SKHPB_DRIVER_E("[HCM] Out of REGION range - region[%d]:MAX[%d]\n",
					   region, hpb->regions_per_lu);
		return false;
	}
	cb = hpb->region_tbl + region;
	if (subregion >= cb->subregion_count) {
		SKHPB_DRIVER_E("[HCM] Out of SUBREGION range - subregion[i]:MAX[%d]\n",
					   subregion, cb->subregion_count);
		return false;
	}
	return true;
}


static inline skhpb_t skhpb_get_ppn(struct skhpb_map_ctx *mctx, int pos)
{
	skhpb_t *ppn_table;
	int index, offset;

	index = pos / SKHPB_ENTREIS_PER_OS_PAGE;
	offset = pos % SKHPB_ENTREIS_PER_OS_PAGE;

	ppn_table = page_address(mctx->m_page[index]);
	return ppn_table[offset];
}


#if defined(SKHPB_READ_LARGE_CHUNK_SUPPORT)
static bool skhpb_subregion_dirty_check(struct skhpb_lu *hpb, struct skhpb_subregion *cp,
		int subregion_offset, int reqBlkCnt)
{
	unsigned int bit_dword, bit_offset;
	unsigned int tmp;
	int checkCnt;

	if (!cp->mctx)
		return true;

	if (!cp->mctx->ppn_dirty)
		return true;

	skhpb_get_bit_offset(hpb, subregion_offset,
			&bit_dword, &bit_offset);

	while (true) {
		checkCnt = SKHPB_BITS_PER_DWORD - bit_offset;
		if (cp->mctx->ppn_dirty[bit_dword]) {
			tmp = cp->mctx->ppn_dirty[bit_dword] << bit_offset;
			if (SKHPB_BITS_PER_DWORD - reqBlkCnt > 0)
				tmp = tmp >> (SKHPB_BITS_PER_DWORD - reqBlkCnt);
			if (tmp)
				return true;
		}
		reqBlkCnt -= checkCnt;
		if (reqBlkCnt <= 0)
			break;
		bit_dword++;
		if (bit_dword >= hpb->ppn_dirties_per_subregion)
			break;
		bit_offset = 0;
	}
	return false;
}

static bool skhpb_lc_dirty_check(struct skhpb_lu *hpb, unsigned int lpn, unsigned int rq_sectors)
{
	int reg;
	int subReg;
	struct skhpb_region *cb;
	struct skhpb_subregion *cp;
	unsigned long cur_lpn = lpn;
	int subRegOffset;
	int reqBlkCnt = rq_sectors >> skhpb_sects_per_blk_shift;

	do {
		skhpb_get_pos_from_lpn(hpb, cur_lpn, &reg, &subReg, &subRegOffset);
		if (!skhpb_check_region_subregion_validity(hpb, reg, subReg))
			return true;
		cb = hpb->region_tbl + reg;
		cp = cb->subregion_tbl + subReg;

		if (cb->region_state == SKHPB_REGION_INACTIVE ||
				cp->subregion_state != SKHPB_SUBREGION_CLEAN) {
			atomic64_inc(&hpb->lc_reg_subreg_miss);
			return true;
		}

		if (skhpb_subregion_dirty_check(hpb, cp, subRegOffset, reqBlkCnt)) {
			//SKHPB_DRIVER_D("[NORMAL READ] DIRTY: Region(%d), SubRegion(%d) \n", reg, subReg);
			atomic64_inc(&hpb->lc_entry_dirty_miss);
			return true;
		}

		if (hpb->entries_per_subregion < subRegOffset + reqBlkCnt) {
			reqBlkCnt -= (hpb->entries_per_subregion - subRegOffset);
		} else {
			reqBlkCnt = 0;
		}

		cur_lpn += reqBlkCnt;
	} while (reqBlkCnt);

	return false;
}
#endif

void skhpb_prep_fn(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct skhpb_lu *hpb;
	struct skhpb_region *cb;
	struct skhpb_subregion *cp;
	unsigned int lpn;
	skhpb_t ppn = 0;
	int region, subregion, subregion_offset;
	const struct request *rq;
	unsigned long long rq_pos;
	unsigned int rq_sectors;
	unsigned char cmd;
	unsigned long flags;

	/* WKLU could not be HPB-LU */
	if (lrbp->lun >= UFS_UPIU_MAX_GENERAL_LUN)
		return;

	hpb = hba->skhpb_lup[lrbp->lun];
	if (!hpb || !hpb->lu_hpb_enable)
		return;

	if (hpb->force_hpb_read_disable)
		return;

	cmd = skhpb_get_cmd(lrbp);
	if (cmd == SKHPB_CMD_OTHERS)
		return;
	if (blk_rq_is_scsi(lrbp->cmd->request))
		return;
	/*
	 * TODO: check if ICE is not supported or not.
	 *
	 * if (cmd==SKHPB_CMD_READ && skhpb_is_encrypted_lrbp(lrbp))
	 *	return;
	 */
	rq 		= lrbp->cmd->request;
	rq_pos 		= blk_rq_pos(rq);
	rq_sectors 	= blk_rq_sectors(rq);

	lpn = rq_pos / SKHPB_SECTORS_PER_BLOCK;
	skhpb_get_pos_from_lpn(
		hpb, lpn, &region, &subregion, &subregion_offset);
	if (!skhpb_check_region_subregion_validity(hpb, region, subregion))
		return;
	cb = hpb->region_tbl + region;
	cp = cb->subregion_tbl + subregion;

	if (cmd == SKHPB_CMD_WRITE ||
		cmd == SKHPB_CMD_DISCARD) {
		if (cb->region_state == SKHPB_REGION_INACTIVE) {
			return;
		}
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		skhpb_set_dirty(hpb, lrbp, region, subregion,
						subregion_offset);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
		return;
	}

#if defined(SKHPB_READ_LARGE_CHUNK_SUPPORT)
	if (((rq_sectors & (SKHPB_SECTORS_PER_BLOCK - 1)) != 0) ||
		 rq_sectors >
		 (SKHPB_READ_LARGE_CHUNK_MAX_BLOCK_COUNT << skhpb_sects_per_blk_shift)) {
#else
	if (rq_sectors != SKHPB_SECTORS_PER_BLOCK) {
#endif
		atomic64_inc(&hpb->size_miss);
		return;
	}

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	if (cb->region_state == SKHPB_REGION_INACTIVE) {
		atomic64_inc(&hpb->region_miss);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
		return;
	} else if (cp->subregion_state != SKHPB_SUBREGION_CLEAN) {
		atomic64_inc(&hpb->subregion_miss);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
		return;
	}
	if (rq_sectors <= SKHPB_SECTORS_PER_BLOCK) {
		if (skhpb_ppn_dirty_check(hpb, cp, subregion_offset)) {
			atomic64_inc(&hpb->entry_dirty_miss);
			spin_unlock_irqrestore(&hpb->hpb_lock, flags);
			return;
		}
	}
#if defined(SKHPB_READ_LARGE_CHUNK_SUPPORT)
	else {
		if (skhpb_lc_dirty_check(hpb, lpn, rq_sectors)) {
			spin_unlock_irqrestore(&hpb->hpb_lock, flags);
			return;
		}
		atomic64_inc(&hpb->lc_hit);
	}
#endif
	ppn = skhpb_get_ppn(cp->mctx, subregion_offset);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);

	//SKHPB_DRIVER_D("XXX ufs ppn %016llx, lba %u\n", (unsigned long long)ppn, lpn);
	skhpb_ppn_prep(hpb, lrbp, ppn, rq_sectors);
	atomic64_inc(&hpb->hit);
	return;
}

static int skhpb_clean_dirty_bitmap(
		struct skhpb_lu *hpb, struct skhpb_subregion *cp)
{
	struct skhpb_region *cb;

	cb = hpb->region_tbl + cp->region;

	/* if mctx is null, active block had been evicted out */
	if (cb->region_state == SKHPB_REGION_INACTIVE || !cp->mctx) {
		SKHPB_DRIVER_D("%d - %d error already evicted\n",
				cp->region, cp->subregion);
		return -EINVAL;
	}

	memset(cp->mctx->ppn_dirty, 0x00,
			hpb->entries_per_subregion / BITS_PER_BYTE);
	return 0;
}

static void skhpb_clean_active_subregion(
		struct skhpb_lu *hpb, struct skhpb_subregion *cp)
{
	struct skhpb_region *cb;

	cb = hpb->region_tbl + cp->region;

	/* if mctx is null, active block had been evicted out */
	if (cb->region_state == SKHPB_REGION_INACTIVE || !cp->mctx) {
		SKHPB_DRIVER_D("%d - %d clean already evicted\n",
				cp->region, cp->subregion);
		return;
	}
	cp->subregion_state = SKHPB_SUBREGION_CLEAN;
}

static void skhpb_error_active_subregion(
		struct skhpb_lu *hpb, struct skhpb_subregion *cp)
{
	struct skhpb_region *cb;

	cb = hpb->region_tbl + cp->region;

	/* if mctx is null, active block had been evicted out */
	if (cb->region_state == SKHPB_REGION_INACTIVE || !cp->mctx) {
		SKHPB_DRIVER_E("%d - %d evicted\n", cp->region, cp->subregion);
		return;
	}
	cp->subregion_state = SKHPB_SUBREGION_DIRTY;
}

static void skhpb_map_compl_process(struct skhpb_lu *hpb,
		struct skhpb_map_req *map_req)
{
	unsigned long flags;

	SKHPB_MAP_REQ_TIME(map_req, map_req->RSP_end, 1);
	SKHPB_DRIVER_D("SKHPB RB COMPL BUFFER %d - %d\n", map_req->region, map_req->subregion);

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	skhpb_clean_active_subregion(hpb,
			hpb->region_tbl[map_req->region].subregion_tbl +
							map_req->subregion);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
}

static void skhpb_map_inactive_compl_process(struct skhpb_lu *hpb,
				     struct skhpb_map_req *map_req)
{
	SKHPB_DRIVER_D("SKHPB WB COMPL BUFFER %d\n", map_req->region);
	SKHPB_MAP_REQ_TIME(map_req, map_req->RSP_end, 1);
}

/*
 * Must held rsp_list_lock before enter this function
 */
static struct skhpb_rsp_info *skhpb_get_req_info(struct skhpb_lu *hpb)
{
	struct skhpb_rsp_info *rsp_info =
		list_first_entry_or_null(&hpb->lh_rsp_info_free,
				struct skhpb_rsp_info,
				list_rsp_info);
	if (!rsp_info) {
		SKHPB_DRIVER_E("there is no rsp_info");
		return NULL;
	}
	list_del(&rsp_info->list_rsp_info);
	memset(rsp_info, 0x00, sizeof(struct skhpb_rsp_info));

	INIT_LIST_HEAD(&rsp_info->list_rsp_info);

	return rsp_info;
}

static void skhpb_map_req_compl_fn(struct request *req, blk_status_t error)
{
	struct skhpb_map_req *map_req = req->end_io_data;
	struct ufs_hba *hba;
	struct skhpb_lu *hpb;
	struct scsi_sense_hdr sshdr = {0};
	struct skhpb_region *cb;
	struct scsi_request *scsireq = scsi_req(req);
	unsigned long flags;

	/* shut up "bio leak" warning */
	memcpy(map_req->sense, scsireq->sense, SCSI_SENSE_BUFFERSIZE);
	req->bio = NULL;
	__blk_put_request(req->q, req);

	hpb = map_req->hpb;
	hba = hpb->hba;
	cb = hpb->region_tbl + map_req->region;

	if (hba->skhpb_state != SKHPB_PRESENT)
		goto free_map_req;

	if (!error) {
		skhpb_map_compl_process(hpb, map_req);
		goto free_map_req;
	}

	SKHPB_DRIVER_E("error number %d ( %d - %d )\n",
		error, map_req->region, map_req->subregion);
	scsi_normalize_sense(map_req->sense,
				SCSI_SENSE_BUFFERSIZE, &sshdr);
	SKHPB_DRIVER_E("code %x sense_key %x asc %x ascq %x\n",
				sshdr.response_code,
				sshdr.sense_key, sshdr.asc, sshdr.ascq);
	SKHPB_DRIVER_E("byte4 %x byte5 %x byte6 %x additional_len %x\n",
				sshdr.byte4, sshdr.byte5,
				sshdr.byte6, sshdr.additional_length);
	atomic64_inc(&hpb->rb_fail);

	if (sshdr.sense_key == ILLEGAL_REQUEST) {
		if (sshdr.asc == 0x00 && sshdr.ascq == 0x16) {
			/* OPERATION IN PROGRESS */
			SKHPB_DRIVER_E("retry rb %d - %d",
					map_req->region, map_req->subregion);

			spin_lock_irqsave(&hpb->map_list_lock, flags);
			INIT_LIST_HEAD(&map_req->list_map_req);
			list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_retry);
			spin_unlock_irqrestore(&hpb->map_list_lock, flags);

			schedule_delayed_work(&hpb->skhpb_map_req_retry_work,
								  msecs_to_jiffies(5000));
			return;
		}
	}
	// Only change subregion status at here.
	// Do not put map_ctx, it will re-use when it is activated again.
	spin_lock_irqsave(&hpb->hpb_lock, flags);
	skhpb_error_active_subregion(hpb, cb->subregion_tbl + map_req->subregion);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
free_map_req:
	spin_lock_irqsave(&hpb->map_list_lock, flags);
	INIT_LIST_HEAD(&map_req->list_map_req);
	list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_free);
	spin_unlock_irqrestore(&hpb->map_list_lock, flags);
	atomic64_dec(&hpb->alloc_map_req_cnt);
}

static void skhpb_map_inactive_req_compl_fn(
	struct request *req, blk_status_t error)
{
	struct skhpb_map_req *map_req = req->end_io_data;
	struct ufs_hba *hba;
	struct skhpb_lu *hpb;
	struct scsi_sense_hdr sshdr;
	struct skhpb_region *cb;
	struct scsi_request *scsireq = scsi_req(req);
	unsigned long flags;

	/* shut up "bio leak" warning */
	memcpy(map_req->sense, scsireq->sense, SCSI_SENSE_BUFFERSIZE);
	req->bio = NULL;
	__blk_put_request(req->q, req);

	hpb = map_req->hpb;
	hba = hpb->hba;
	cb = hpb->region_tbl + map_req->region;

	if (hba->skhpb_state != SKHPB_PRESENT)
		goto free_map_req;

	if (!error) {
		skhpb_map_inactive_compl_process(hpb, map_req);
		goto free_map_req;
	}

	SKHPB_DRIVER_E("error number %d ( %d - %d )",
			error, map_req->region, map_req->subregion);
	scsi_normalize_sense(map_req->sense, SCSI_SENSE_BUFFERSIZE,
						 &sshdr);
	SKHPB_DRIVER_E("code %x sense_key %x asc %x ascq %x",
			sshdr.response_code,
			sshdr.sense_key, sshdr.asc, sshdr.ascq);
	SKHPB_DRIVER_E("byte4 %x byte5 %x byte6 %x additional_len %x",
			sshdr.byte4, sshdr.byte5,
			sshdr.byte6, sshdr.additional_length);
	atomic64_inc(&hpb->rb_fail);

	if (sshdr.sense_key == ILLEGAL_REQUEST) {
		if (cb->is_pinned) {
			SKHPB_DRIVER_E("WRITE_BUFFER is not allowed on pinned area: region#%d",
					cb->region);
		} else {
			if (sshdr.asc == 0x00 && sshdr.ascq == 0x16) {
				/* OPERATION IN PROGRESS */
				SKHPB_DRIVER_E("retry wb %d", map_req->region);

				spin_lock(&hpb->map_list_lock);
				INIT_LIST_HEAD(&map_req->list_map_req);
				list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_retry);
				spin_unlock(&hpb->map_list_lock);

				schedule_delayed_work(&hpb->skhpb_map_req_retry_work,
									  msecs_to_jiffies(5000));
				return;
			}
		}
	}
	// Only change subregion status at here.
	// Do not put map_ctx, it will re-use when it is activated again.
	spin_lock_irqsave(&hpb->hpb_lock, flags);
	skhpb_error_active_subregion(hpb, cb->subregion_tbl + map_req->subregion);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
free_map_req:
	spin_lock(&hpb->map_list_lock);
	INIT_LIST_HEAD(&map_req->list_map_req);
	list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_free);
	spin_unlock(&hpb->map_list_lock);
	atomic64_dec(&hpb->alloc_map_req_cnt);
}

static int skhpb_execute_req_dev_ctx(struct skhpb_lu *hpb,
				unsigned char *cmd, void *buf, int length)
{
	unsigned long flags;
	struct scsi_sense_hdr sshdr = {0};
	struct scsi_device *sdp;
	struct ufs_hba *hba = hpb->hba;
	int ret = 0;

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdp = hba->sdev_ufs_lu[hpb->lun];
	if (sdp) {
		ret = scsi_device_get(sdp);
		if (!ret && !scsi_device_online(sdp)) {
			ret = -ENODEV;
			scsi_device_put(sdp);
		} else if (!ret) {
			hba->issue_ioctl = true;
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret)
		return ret;

	ret = scsi_execute_req(sdp, cmd, DMA_FROM_DEVICE,
				buf, length, &sshdr,
				msecs_to_jiffies(30000), 3, NULL);
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->issue_ioctl = false;
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	scsi_device_put(sdp);
	return ret;
}

static inline void skhpb_set_read_dev_ctx_cmd(unsigned char *cmd, int lba,
					       int length)
{
	cmd[0] = READ_10;
	cmd[1] = 0x02;
	cmd[2] = SKHPB_GET_BYTE_3(lba);
	cmd[3] = SKHPB_GET_BYTE_2(lba);
	cmd[4] = SKHPB_GET_BYTE_1(lba);
	cmd[5] = SKHPB_GET_BYTE_0(lba);
	cmd[6] = SKHPB_GET_BYTE_2(length);
	cmd[7] = SKHPB_GET_BYTE_1(length);
	cmd[8] = SKHPB_GET_BYTE_0(length);
}

int skhpb_issue_req_dev_ctx(struct skhpb_lu *hpb, unsigned char *buf,
			      int buf_length)
{
	unsigned char cmd[10] = { 0 };
	int cmd_len = buf_length >> PAGE_SHIFT;
	int ret = 0;

	skhpb_set_read_dev_ctx_cmd(cmd, 0x48504230, cmd_len);

	ret = skhpb_execute_req_dev_ctx(hpb, cmd, buf, buf_length);
	if (ret < 0)
		SKHPB_DRIVER_E("failed with err %d\n", ret);
	return ret;
}

static inline void skhpb_set_write_buf_cmd(unsigned char *cmd, int region)
{
	cmd[0] = SKHPB_WRITE_BUFFER;
	cmd[1] = 0x01;
	cmd[2] = SKHPB_GET_BYTE_1(region);
	cmd[3] = SKHPB_GET_BYTE_0(region);
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;
	cmd[7] = 0x00;
	cmd[8] = 0x00;
	cmd[9] = 0x00;

	//To verify the values within WRITE_BUFFER command
	SKHPB_DRIVER_HEXDUMP("[HPB] WRITE BUFFER ", 16, 1, cmd, 10, 1);
}

static inline void skhpb_set_read_buf_cmd(unsigned char *cmd,
		int region, int subregion, int subregion_mem_size)
{
	cmd[0] = SKHPB_READ_BUFFER;
	cmd[1] = 0x01;
	cmd[2] = SKHPB_GET_BYTE_1(region);
	cmd[3] = SKHPB_GET_BYTE_0(region);
	cmd[4] = SKHPB_GET_BYTE_1(subregion);
	cmd[5] = SKHPB_GET_BYTE_0(subregion);
	cmd[6] = SKHPB_GET_BYTE_2(subregion_mem_size);
	cmd[7] = SKHPB_GET_BYTE_1(subregion_mem_size);
	cmd[8] = SKHPB_GET_BYTE_0(subregion_mem_size);
	cmd[9] = 0x00;

	//To verify the values within READ_BUFFER command
	SKHPB_DRIVER_HEXDUMP("[HPB] READ BUFFER ", 16, 1, cmd, 10, 1);
}

static int skhpb_add_bio_page(struct skhpb_lu *hpb,
		struct request_queue *q, struct bio *bio, struct bio_vec *bvec,
		struct skhpb_map_ctx *mctx)
{
	struct page *page = NULL;
	int i, ret;

	bio_init(bio, bvec, hpb->mpages_per_subregion);

	for (i = 0; i < hpb->mpages_per_subregion; i++) {
		page = mctx->m_page[i];
		if (!page)
			return -ENOMEM;

		ret = bio_add_pc_page(q, bio, page, hpb->mpage_bytes, 0);
		if (ret != hpb->mpage_bytes) {
			SKHPB_DRIVER_E("error ret %d\n", ret);
			return -EINVAL;
		}
	}
	return 0;
}

static int skhpb_map_req_issue(
	struct skhpb_lu *hpb, struct skhpb_map_req *map_req)
{
	struct request_queue *q = hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue;
	struct request *req;
	struct scsi_request *scsireq;
	unsigned char cmd[10] = { 0 };
	int ret;
	unsigned long flags;

	if (map_req->rwbuffer_flag == W_BUFFER)
		skhpb_set_write_buf_cmd(cmd, map_req->region);
	else
		skhpb_set_read_buf_cmd(cmd, map_req->region, map_req->subregion,
								map_req->subregion_mem_size);
	if (map_req->rwbuffer_flag == W_BUFFER)
		req = blk_get_request(q, REQ_OP_SCSI_OUT, __GFP_RECLAIM);
	else
		req = blk_get_request(q, REQ_OP_SCSI_IN, __GFP_RECLAIM);
	if (IS_ERR(req)) {
		int rv = PTR_ERR(req);

		if (map_req->rwbuffer_flag == W_BUFFER)
			SKHPB_DRIVER_E("blk_get_request errno %d, \
					retry #%d, WRITE BUFFER %d\n",
					rv, map_req->retry_cnt, map_req->region);
		else
			SKHPB_DRIVER_E("blk_get_request errno %d, \
					retry #%d, READ BUFFER %d:%d\n",
					rv, map_req->retry_cnt,
					map_req->region, map_req->subregion);

		if (map_req->retry_cnt == 10) {
			/* give up */
			return rv;
		}

		spin_lock_irqsave(&hpb->map_list_lock, flags);
		list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_retry);
		spin_unlock_irqrestore(&hpb->map_list_lock, flags);

		schedule_delayed_work(&hpb->skhpb_map_req_retry_work, msecs_to_jiffies(10));

		return 0;
	}
	scsireq = scsi_req(req);

	scsireq->cmd_len = COMMAND_SIZE(cmd[0]);
	BUG_ON(scsireq->cmd_len > sizeof(scsireq->__cmd));
	scsireq->cmd = scsireq->__cmd;
	memcpy(scsireq->cmd, cmd, scsireq->cmd_len);

	req->rq_flags |= RQF_QUIET | RQF_PREEMPT;
	req->timeout = msecs_to_jiffies(30000);
	req->end_io_data = map_req;

	if (map_req->rwbuffer_flag == R_BUFFER) {
		ret = skhpb_add_bio_page(
			hpb, q, &map_req->bio, map_req->bvec, map_req->mctx);
		if (ret) {
			SKHPB_DRIVER_E("skhpb_add_bio_page_error %d\n", ret);
			goto out_put_request;
		}
		map_req->pbio = &map_req->bio;
		blk_rq_append_bio(req, &map_req->pbio);
	}
	SKHPB_DRIVER_D("issue map_request: %d - %d\n",
			map_req->region, map_req->subregion);

	SKHPB_MAP_REQ_TIME(map_req, map_req->RSP_issue, 0);
	if (hpb->hpb_control_mode == HOST_CTRL_MODE) {
		if (map_req->rwbuffer_flag == W_BUFFER)
			blk_execute_rq_nowait(
				q, NULL, req, 0, skhpb_map_inactive_req_compl_fn);
		else
			blk_execute_rq_nowait(q, NULL, req, 0, skhpb_map_req_compl_fn);
	} else
		blk_execute_rq_nowait(q, NULL, req, 1, skhpb_map_req_compl_fn);

	if (map_req->rwbuffer_flag == W_BUFFER)
		atomic64_inc(&hpb->w_map_req_cnt);
	else
		atomic64_inc(&hpb->map_req_cnt);

	return 0;

out_put_request:
	blk_put_request(req);
	return ret;
}

static int skhpb_set_map_req(struct skhpb_lu *hpb,
		int region, int subregion, struct skhpb_map_ctx *mctx,
		struct skhpb_rsp_info *rsp_info,
		enum SKHPB_BUFFER_MODE flag)
{
	bool last = hpb->region_tbl[region].subregion_tbl[subregion].last;
	struct skhpb_map_req *map_req;
	unsigned long flags;

	spin_lock_irqsave(&hpb->map_list_lock, flags);
	map_req = list_first_entry_or_null(&hpb->lh_map_req_free,
					    struct skhpb_map_req,
					    list_map_req);
	if (!map_req) {
		SKHPB_DRIVER_D("There is no map_req\n");
		spin_unlock_irqrestore(&hpb->map_list_lock, flags);
		return -EAGAIN;
	}
	list_del(&map_req->list_map_req);
	spin_unlock_irqrestore(&hpb->map_list_lock, flags);
	atomic64_inc(&hpb->alloc_map_req_cnt);

	memset(map_req, 0x00, sizeof(struct skhpb_map_req));

	map_req->hpb = hpb;
	map_req->region = region;
	map_req->subregion = subregion;
	map_req->subregion_mem_size =
		last ? hpb->last_subregion_mem_size : hpb->subregion_mem_size;
	map_req->mctx = mctx;
	map_req->lun = hpb->lun;
	map_req->RSP_start = rsp_info->RSP_start;
	if (flag == W_BUFFER) {
		map_req->rwbuffer_flag = W_BUFFER;
	} else
		map_req->rwbuffer_flag = R_BUFFER;

	if (skhpb_map_req_issue(hpb, map_req)) {
		SKHPB_DRIVER_E("issue Failed!!!\n");
		return -ENOMEM;
	}
	return 0;
}

static struct skhpb_map_ctx *skhpb_get_map_ctx(struct skhpb_lu *hpb)
{
	struct skhpb_map_ctx *mctx;

	mctx = list_first_entry_or_null(&hpb->lh_map_ctx,
			struct skhpb_map_ctx, list_table);
	if (mctx) {
		list_del_init(&mctx->list_table);
		hpb->debug_free_table--;
		return mctx;
	}
	return ERR_PTR(-ENOMEM);
}

static inline void skhpb_add_lru_info(struct skhpb_victim_select_info *lru_info,
				       struct skhpb_region *cb)
{
	cb->region_state = SKHPB_REGION_ACTIVE;
	list_add_tail(&cb->list_region, &lru_info->lru);
	atomic64_inc(&lru_info->active_count);
}

static inline int skhpb_add_region(struct skhpb_lu *hpb,
					struct skhpb_region *cb)
{
	struct skhpb_victim_select_info *lru_info;
	int subregion;
	int err = 0;

	lru_info = &hpb->lru_info;

	//SKHPB_DRIVER_D("E->active region: %d", cb->region);

	for (subregion = 0; subregion < cb->subregion_count; subregion++) {
		struct skhpb_subregion *cp;

		cp = cb->subregion_tbl + subregion;
		cp->mctx = skhpb_get_map_ctx(hpb);
		if (IS_ERR(cp->mctx)) {
			err = PTR_ERR(cp->mctx);
			goto out;
		}
		cp->subregion_state = SKHPB_SUBREGION_DIRTY;
	}
	if (!cb->is_pinned)
		skhpb_add_lru_info(lru_info, cb);

	atomic64_inc(&hpb->region_add);
out:
	if (err)
		SKHPB_DRIVER_E("get mctx failed. err %d subregion %d free_table %d\n",
			err, subregion, hpb->debug_free_table);
	return err;
}

static inline void skhpb_put_map_ctx(
		struct skhpb_lu *hpb, struct skhpb_map_ctx *mctx)
{
	list_add(&mctx->list_table, &hpb->lh_map_ctx);
	hpb->debug_free_table++;
}

static inline void skhpb_purge_active_page(struct skhpb_lu *hpb,
		struct skhpb_subregion *cp, int state)
{
	if (state == SKHPB_SUBREGION_UNUSED) {
		skhpb_put_map_ctx(hpb, cp->mctx);
		cp->mctx = NULL;
	}
	cp->subregion_state = state;
}

static inline void skhpb_cleanup_lru_info(
	struct skhpb_victim_select_info *lru_info,
	struct skhpb_region *cb)
{
	list_del_init(&cb->list_region);
	cb->region_state = SKHPB_REGION_INACTIVE;
	cb->hit_count = 0;
	atomic64_dec(&lru_info->active_count);
}

static inline void skhpb_evict_region(struct skhpb_lu *hpb,
				     struct skhpb_region *cb)
{
	struct skhpb_victim_select_info *lru_info;
	struct skhpb_subregion *cp;
	int subregion;

	// If the maximum value is exceeded at the time of region addition,
	// it may have already been processed.
	if (cb->region_state == SKHPB_REGION_INACTIVE) {
		SKHPB_DRIVER_D("Region:%d was already inactivated.\n",
				cb->region);
		return;
	}
	lru_info = &hpb->lru_info;

	//SKHPB_DRIVER_D("C->EVICT region: %d\n", cb->region);

	skhpb_cleanup_lru_info(lru_info, cb);
	atomic64_inc(&hpb->region_evict);
	for (subregion = 0; subregion < cb->subregion_count; subregion++) {
		cp = cb->subregion_tbl + subregion;

		skhpb_purge_active_page(hpb, cp, SKHPB_SUBREGION_UNUSED);
	}
}

static void skhpb_hit_lru_info(struct skhpb_victim_select_info *lru_info,
				       struct skhpb_region *cb)
{
	switch (lru_info->selection_type) {
	case TYPE_LRU:
		list_move_tail(&cb->list_region, &lru_info->lru);
		break;
	case TYPE_LFU:
		if (cb->hit_count != 0xffffffff)
			cb->hit_count++;

		list_move_tail(&cb->list_region, &lru_info->lru);
		break;
	default:
		break;
	}
}

static struct skhpb_region *skhpb_victim_lru_info(
				struct skhpb_victim_select_info *lru_info)
{
	struct skhpb_region *cb;
	struct skhpb_region *victim_cb = NULL;
	u32 hit_count = 0xffffffff;

	switch (lru_info->selection_type) {
	case TYPE_LRU:
		victim_cb = list_first_entry(&lru_info->lru,
				struct skhpb_region, list_region);
		break;
	case TYPE_LFU:
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

static int skhpb_check_lru_evict(struct skhpb_lu *hpb, struct skhpb_region *cb)
{
	struct skhpb_victim_select_info *lru_info = &hpb->lru_info;
	struct skhpb_region *victim_cb;
	unsigned long flags;

	if (cb->is_pinned)
		return 0;

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	if (!list_empty(&cb->list_region)) {
		skhpb_hit_lru_info(lru_info, cb);
		goto out;
	}

	if (cb->region_state != SKHPB_REGION_INACTIVE)
		goto out;

	if (atomic64_read(&lru_info->active_count) == lru_info->max_lru_active_count) {

		victim_cb = skhpb_victim_lru_info(lru_info);

		if (!victim_cb) {
			SKHPB_DRIVER_E("SKHPB victim_cb is NULL\n");
			goto unlock_error;
		}
		SKHPB_DRIVER_D("max lru case. victim : %d\n", victim_cb->region);
		skhpb_evict_region(hpb, victim_cb);
	}
	if (skhpb_add_region(hpb, cb)) {
		SKHPB_DRIVER_E("SKHPB memory allocation failed\n");
		goto unlock_error;
	}


out:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	return 0;
unlock_error:
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	return -ENOMEM;
}

static int skhpb_evict_load_region(struct skhpb_lu *hpb,
				struct skhpb_rsp_info *rsp_info)
{
	struct skhpb_region *cb;
	int region, iter;
	unsigned long flags;

	for (iter = 0; iter < rsp_info->inactive_cnt; iter++) {
		region = rsp_info->inactive_list.region[iter];
		cb = hpb->region_tbl + region;

		if (cb->is_pinned) {
			/*
			 * Pinned active-block should not drop-out.
			 * But if so, it would treat error as critical,
			 * and it will run skhpb_eh_work
			 */
			SKHPB_DRIVER_E("SKHPB pinned active-block drop-out error\n");
			return -ENOMEM;
		}
		if (list_empty(&cb->list_region))
			continue;

		spin_lock_irqsave(&hpb->hpb_lock, flags);
		skhpb_evict_region(hpb, cb);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	}

	for (iter = 0; iter < rsp_info->active_cnt; iter++) {
		region = rsp_info->active_list.region[iter];
		cb = hpb->region_tbl + region;
		if (skhpb_check_lru_evict(hpb, cb))
			return -ENOMEM;
	}
	return 0;
}

static inline struct skhpb_rsp_field *skhpb_get_hpb_rsp(
		struct ufshcd_lrb *lrbp)
{
	return (struct skhpb_rsp_field *)&lrbp->ucd_rsp_ptr->sr.sense_data_len;
}

static int skhpb_rsp_map_cmd_req(struct skhpb_lu *hpb,
		struct skhpb_rsp_info *rsp_info)
{
	struct skhpb_region *cb;
	struct skhpb_subregion *cp;
	int region, subregion;
	int iter;
	int ret;
	unsigned long flags;

		/*
		 *  Before Issue read buffer CMD for active active block,
		 *  prepare the memory from memory pool.
		 */
		ret = skhpb_evict_load_region(hpb, rsp_info);
		if (ret) {
			SKHPB_DRIVER_E("region evict/load failed. ret %d\n", ret);
			goto wakeup_ee_worker;
		}
		for (iter = 0; iter < rsp_info->active_cnt; iter++) {
			region = rsp_info->active_list.region[iter];
			subregion = rsp_info->active_list.subregion[iter];
			cb = hpb->region_tbl + region;

			if (region >= hpb->regions_per_lu ||
				subregion >= cb->subregion_count) {
				SKHPB_DRIVER_E("skhpb issue-map %d - %d range error\n",
							   region, subregion);
				goto wakeup_ee_worker;
			}

			cp = cb->subregion_tbl + subregion;

			/*
			 * if subregion_state set SKHPB_SUBREGION_ISSUED,
			 * active_page has already been added to list,
			 * so it just ends function.
			 */
			spin_lock_irqsave(&hpb->hpb_lock, flags);
			if (cp->subregion_state == SKHPB_SUBREGION_ISSUED) {
				spin_unlock_irqrestore(&hpb->hpb_lock, flags);
				continue;
			}

			cp->subregion_state = SKHPB_SUBREGION_ISSUED;

			ret = skhpb_clean_dirty_bitmap(hpb, cp);

			spin_unlock_irqrestore(&hpb->hpb_lock, flags);

			if (ret)
				continue;

			if (!hpb->hba->sdev_ufs_lu[hpb->lun] ||
				!hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue)
				return -ENODEV;
			ret = skhpb_set_map_req(hpb, region, subregion,
									cp->mctx, rsp_info, R_BUFFER);
			SKHPB_DRIVER_D("SEND READ_BUFFER - Region:%d, SubRegion:%d\n",
						   region, subregion);
			if (ret) {
				if (ret == -EAGAIN) {
					spin_lock_irqsave(&hpb->hpb_lock, flags);
					cp->subregion_state = SKHPB_SUBREGION_DIRTY;
					spin_unlock_irqrestore(&hpb->hpb_lock, flags);
					rsp_info->inactive_cnt = 0;
					if (iter) {
						rsp_info->active_list.region[0]  = region;
						rsp_info->active_list.subregion[0] = subregion;
						rsp_info->active_cnt = 1;
					}
					return ret;
				}
				SKHPB_DRIVER_E("skhpb_set_map_req error %d\n", ret);
				goto wakeup_ee_worker;
			}
		}
	return 0;

wakeup_ee_worker:
	hpb->hba->skhpb_state = SKHPB_FAILED;
	schedule_work(&hpb->hba->skhpb_eh_work);
	return -ENODEV;
}

/* routine : isr (ufs) */
void skhpb_rsp_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct skhpb_lu *hpb;
	struct skhpb_rsp_field *rsp_field;
	struct skhpb_rsp_field sense_data;
	struct skhpb_rsp_info *rsp_info;
	int data_seg_len, num, blk_idx, update_alert;

	update_alert = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2)
		& MASK_RSP_UPIU_HPB_UPDATE_ALERT;
	data_seg_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2)
		& MASK_RSP_UPIU_DATA_SEG_LEN;
	if (!update_alert || !data_seg_len) {
		bool do_tasklet = false;

		if (lrbp->lun >= UFS_UPIU_MAX_GENERAL_LUN)
			return;

		hpb = hba->skhpb_lup[lrbp->lun];
		if (!hpb)
			return;

		spin_lock(&hpb->rsp_list_lock);
		do_tasklet = !list_empty(&hpb->lh_rsp_info);
		spin_unlock(&hpb->rsp_list_lock);

		if (do_tasklet)
			schedule_work(&hpb->skhpb_rsp_work);
		return;
	}

	memcpy(&sense_data, &lrbp->ucd_rsp_ptr->sr.sense_data_len,
		sizeof(struct skhpb_rsp_field));
	rsp_field = &sense_data;
	if ((get_unaligned_be16(rsp_field->sense_data_len + 0)
		 != SKHPB_DEV_SENSE_SEG_LEN) ||
			rsp_field->desc_type != SKHPB_DEV_DES_TYPE ||
			rsp_field->additional_len != SKHPB_DEV_ADDITIONAL_LEN ||
			rsp_field->hpb_type == SKHPB_RSP_NONE ||
			rsp_field->active_region_cnt > SKHPB_MAX_ACTIVE_NUM ||
			rsp_field->inactive_region_cnt > SKHPB_MAX_INACTIVE_NUM)
		return;

	if (rsp_field->lun >= UFS_UPIU_MAX_GENERAL_LUN) {
		SKHPB_DRIVER_E("lun is not general = %d", rsp_field->lun);
		return;
	}
	hpb = hba->skhpb_lup[rsp_field->lun];
	if (!hpb) {
		SKHPB_DRIVER_E("UFS-LU%d is not SKHPB LU\n", rsp_field->lun);
		return;
	}
	if (hpb->force_map_req_disable)
		return;
	SKHPB_DRIVER_D("HPB-Info Noti: %d LUN: %d Seg-Len %d, Req_type = %d\n",
		rsp_field->hpb_type, rsp_field->lun,
		data_seg_len, rsp_field->hpb_type);
	if (!hpb->lu_hpb_enable) {
		SKHPB_DRIVER_E("LU(%d) not HPB-LU\n", rsp_field->lun);
		return;
	}
	//To verify the values within RESPONSE UPIU.
	SKHPB_DRIVER_HEXDUMP("[HPB] RESP UPIU ", 16, 1,
			lrbp->ucd_rsp_ptr, sizeof(struct utp_upiu_rsp), 1);
	//If wLUMaxActiveHPBRegions == wNumHPBPinnedRegions
	if (hpb->lru_info.max_lru_active_count == 0) {
		SKHPB_DRIVER_D("max_lru_active_count is 0");
		return;
	}

	switch (rsp_field->hpb_type) {
	case SKHPB_RSP_REQ_REGION_UPDATE:
		atomic64_inc(&hpb->rb_noti_cnt);
		WARN_ON(data_seg_len != SKHPB_DEV_DATA_SEG_LEN);

		spin_lock(&hpb->rsp_list_lock);
		rsp_info = skhpb_get_req_info(hpb);
		spin_unlock(&hpb->rsp_list_lock);
		if (!rsp_info)
			return;
		rsp_info->type = SKHPB_RSP_REQ_REGION_UPDATE;

		for (num = 0; num < rsp_field->active_region_cnt; num++) {
			blk_idx = num * SKHPB_PER_ACTIVE_INFO_BYTES;
			rsp_info->active_list.region[num] =
				get_unaligned_be16(rsp_field->hpb_active_field + blk_idx);
			rsp_info->active_list.subregion[num] =
				get_unaligned_be16(rsp_field->hpb_active_field
								+ blk_idx + 2);
			SKHPB_DRIVER_D("active num: %d, #block: %d, page#: %d\n",
				num + 1,
				rsp_info->active_list.region[num],
				rsp_info->active_list.subregion[num]);
		}
		rsp_info->active_cnt = num;

		for (num = 0; num < rsp_field->inactive_region_cnt; num++) {
			blk_idx = num * SKHPB_PER_INACTIVE_INFO_BYTES;
			rsp_info->inactive_list.region[num] =
				get_unaligned_be16(rsp_field->hpb_inactive_field + blk_idx);
			SKHPB_DRIVER_D("inactive num: %d, #block: %d\n",
				  num + 1, rsp_info->inactive_list.region[num]);
		}
		rsp_info->inactive_cnt = num;

		SKHPB_DRIVER_D("active cnt: %d, inactive cnt: %d\n",
			  rsp_info->active_cnt, rsp_info->inactive_cnt);
		SKHPB_DRIVER_D("add_list %p -> %p\n",
					rsp_info, &hpb->lh_rsp_info);

		spin_lock(&hpb->rsp_list_lock);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info);
		spin_unlock(&hpb->rsp_list_lock);

		schedule_work(&hpb->skhpb_rsp_work);
		break;

	case SKHPB_RSP_HPB_RESET:
		for (num = 0 ; num < UFS_UPIU_MAX_GENERAL_LUN ; num++) {
			hpb = hba->skhpb_lup[num];
			if (!hpb || !hpb->lu_hpb_enable)
				continue;
			atomic64_inc(&hpb->reset_noti_cnt);

			spin_lock(&hpb->rsp_list_lock);
			rsp_info = skhpb_get_req_info(hpb);
			spin_unlock(&hpb->rsp_list_lock);
			if (!rsp_info)
				return;

			rsp_info->type = SKHPB_RSP_HPB_RESET;

			spin_lock(&hpb->rsp_list_lock);
			list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info);
			spin_unlock(&hpb->rsp_list_lock);

			schedule_work(&hpb->skhpb_rsp_work);
		}
		break;

	default:
		SKHPB_DRIVER_E("hpb_type is not available : %d\n",
					rsp_field->hpb_type);
		break;
	}
}

static int skhpb_read_desc(struct ufs_hba *hba,
		u8 desc_id, u8 desc_index, u8 *desc_buf, u32 size)
{
	int err = 0;

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
				desc_id, desc_index, 0, desc_buf, &size);
	if (err) {
		SKHPB_DRIVER_E("reading Device Desc failed. err = %d\n", err);
	}
	return err;
}

static int skhpb_read_device_desc(
	struct ufs_hba *hba, u8 *desc_buf, u32 size)
{
	return skhpb_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, desc_buf, size);
}

static int skhpb_read_geo_desc(struct ufs_hba *hba, u8 *desc_buf, u32 size)
{
	return skhpb_read_desc(hba, QUERY_DESC_IDN_GEOMETRY, 0,
						desc_buf, size);
}

static int skhpb_read_unit_desc(struct ufs_hba *hba, int lun,
						u8 *desc_buf, u32 size)
{
	return skhpb_read_desc(hba, QUERY_DESC_IDN_UNIT,
						lun, desc_buf, size);
}

static inline void skhpb_add_subregion_to_req_list(struct skhpb_lu *hpb,
		struct skhpb_subregion *cp)
{
	list_add_tail(&cp->list_subregion, &hpb->lh_subregion_req);
	cp->subregion_state = SKHPB_SUBREGION_DIRTY;
}

static int skhpb_execute_req(struct skhpb_lu *hpb, unsigned char *cmd,
		struct skhpb_subregion *cp)
{
	struct ufs_hba *hba = hpb->hba;
	struct scsi_device *sdp;
	struct request_queue *q;
	struct request *req;
	struct scsi_request *scsireq;
	struct bio bio;
	struct bio *pbio = &bio;
	struct scsi_sense_hdr sshdr = {0};
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdp = hba->sdev_ufs_lu[hpb->lun];
	if (sdp) {
		ret = scsi_device_get(sdp);
		if (!ret && !scsi_device_online(sdp)) {
			ret = -ENODEV;
			scsi_device_put(sdp);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret)
		return ret;

	q = sdp->request_queue;

	req = blk_get_request(q, REQ_OP_SCSI_IN, __GFP_RECLAIM);
	if (IS_ERR(req)) {
		ret = PTR_ERR(req);
		goto out_put;
	}
	scsireq = scsi_req(req);

	scsireq->cmd_len = COMMAND_SIZE(cmd[0]);
	if (scsireq->cmd_len > sizeof(scsireq->__cmd)) {
		scsireq->cmd = kmalloc(scsireq->cmd_len, __GFP_RECLAIM);
		if (!scsireq->cmd) {
			ret = -ENOMEM;
			goto out_put_request;
		}
	} else {
		scsireq->cmd = scsireq->__cmd;
	}
	memcpy(scsireq->cmd, cmd, scsireq->cmd_len);

	scsireq->retries = 3;
	req->timeout = msecs_to_jiffies(10000);
	req->rq_flags |= RQF_QUIET | RQF_PREEMPT;

	ret = skhpb_add_bio_page(hpb, q, &bio, hpb->bvec, cp->mctx);
	if (ret)
		goto out_put_scsi_req;
	blk_rq_append_bio(req, &pbio);

	blk_execute_rq(q, NULL, req, 1);

	if (scsireq->result) {
		scsi_normalize_sense(scsireq->sense, SCSI_SENSE_BUFFERSIZE, &sshdr);
		SKHPB_DRIVER_E("code %x sense_key %x asc %x ascq %x",
				sshdr.response_code, sshdr.sense_key, sshdr.asc,
				sshdr.ascq);
		SKHPB_DRIVER_E("byte4 %x byte5 %x byte6 %x additional_len %x",
				sshdr.byte4, sshdr.byte5, sshdr.byte6,
				sshdr.additional_length);
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		skhpb_error_active_subregion(hpb, cp);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
		ret = -EIO;
	} else {
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		ret = skhpb_clean_dirty_bitmap(hpb, cp);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
		if (ret) {
			SKHPB_DRIVER_E("skhpb_clean_dirty_bitmap error %d", ret);
		}
		ret = 0;
	}

out_put_scsi_req:
	scsi_req_free_cmd(scsireq);
out_put_request:
	blk_put_request(req);
out_put:
	scsi_device_put(sdp);
	return ret;
}

static int skhpb_issue_map_req_from_list(struct skhpb_lu *hpb)
{
	struct skhpb_subregion *cp, *next_cp;
	int ret;
	unsigned long flags;

	LIST_HEAD(req_list);

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	list_splice_init(&hpb->lh_subregion_req, &req_list);
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);

	list_for_each_entry_safe(cp, next_cp, &req_list, list_subregion) {
		int subregion_mem_size =
			cp->last ? hpb->last_subregion_mem_size : hpb->subregion_mem_size;
		unsigned char cmd[10] = { 0 };

		skhpb_set_read_buf_cmd(cmd, cp->region, cp->subregion,
					subregion_mem_size);

		SKHPB_DRIVER_D("issue map_request: %d - %d/%d\n",
				cp->region, cp->subregion, subregion_mem_size);

		ret = skhpb_execute_req(hpb, cmd, cp);
		if (ret < 0) {
			SKHPB_DRIVER_E("region %d sub %d failed with err %d",
					cp->region, cp->subregion, ret);
			continue;
		}

		spin_lock_irqsave(&hpb->hpb_lock, flags);
		skhpb_clean_active_subregion(hpb, cp);
		list_del_init(&cp->list_subregion);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	}

	return 0;
}

static void skhpb_pinned_work_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct skhpb_lu *hpb =
		container_of(dwork, struct skhpb_lu, skhpb_pinned_work);
	int ret;

	SKHPB_DRIVER_D("worker start\n");

	if (!list_empty(&hpb->lh_subregion_req)) {
		pm_runtime_get_sync(SKHPB_DEV(hpb));
		ret = skhpb_issue_map_req_from_list(hpb);
		/*
		 * if its function failed at init time,
		 * skhpb-device will request map-req,
		 * so it is not critical-error, and just finish work-handler
		 */
		if (ret)
			SKHPB_DRIVER_E("failed map-issue. ret %d\n", ret);
		pm_runtime_mark_last_busy(SKHPB_DEV(hpb));
		pm_runtime_put_noidle(SKHPB_DEV(hpb));
	}

	SKHPB_DRIVER_D("worker end\n");
}

static void skhpb_map_req_retry_work_handler(struct work_struct *work)
{
	struct skhpb_lu *hpb;
	struct delayed_work *dwork = to_delayed_work(work);
	struct skhpb_map_req *map_req;
	int ret = 0;
	unsigned long flags;

	LIST_HEAD(retry_list);

	hpb = container_of(dwork, struct skhpb_lu, skhpb_map_req_retry_work);
	SKHPB_DRIVER_D("retry worker start");

	spin_lock_irqsave(&hpb->map_list_lock, flags);
	list_splice_init(&hpb->lh_map_req_retry, &retry_list);
	spin_unlock_irqrestore(&hpb->map_list_lock, flags);

	while (1) {
		map_req = list_first_entry_or_null(&retry_list,
				struct skhpb_map_req, list_map_req);
		if (!map_req) {
			SKHPB_DRIVER_D("There is no map_req");
			break;
		}
		list_del(&map_req->list_map_req);

		map_req->retry_cnt++;

		ret = skhpb_map_req_issue(hpb, map_req);
		if (ret) {
			SKHPB_DRIVER_E("skhpb_map_req_issue error %d", ret);
			goto wakeup_ee_worker;
		}
	}
	SKHPB_DRIVER_D("worker end");
	return;

wakeup_ee_worker:
	hpb->hba->skhpb_state = SKHPB_FAILED;
	schedule_work(&hpb->hba->skhpb_eh_work);
}

static void skhpb_delayed_rsp_work_handler(struct work_struct *work)
{
	struct skhpb_lu *hpb = container_of(work, struct skhpb_lu, skhpb_rsp_work);
	struct skhpb_rsp_info *rsp_info;
	unsigned long flags;
	int ret = 0;

	SKHPB_DRIVER_D("rsp_work enter");

	while (!hpb->hba->clk_gating.is_suspended) {
		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		rsp_info = list_first_entry_or_null(&hpb->lh_rsp_info,
				struct skhpb_rsp_info, list_rsp_info);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
		if (!rsp_info) {
			break;
		}
		SKHPB_RSP_TIME(rsp_info->RSP_start);

		switch (rsp_info->type) {
		case SKHPB_RSP_REQ_REGION_UPDATE:
			ret = skhpb_rsp_map_cmd_req(hpb, rsp_info);
			if (ret)
				return;
			break;

		case SKHPB_RSP_HPB_RESET:
			skhpb_purge_active_block(hpb);
			skhpb_map_loading_trigger(hpb, true, false);
			break;

		default:
			break;
		}

		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		list_del_init(&rsp_info->list_rsp_info);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info_free);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
	}
	SKHPB_DRIVER_D("rsp_work end");
}

static void skhpb_init_constant(void)
{
	skhpb_sects_per_blk_shift = ffs(SKHPB_BLOCK) - ffs(SKHPB_SECTOR);
	SKHPB_DRIVER_D("skhpb_sects_per_blk_shift: %u %u\n",
		  skhpb_sects_per_blk_shift, ffs(SKHPB_SECTORS_PER_BLOCK) - 1);

	skhpb_bits_per_dword_shift = ffs(SKHPB_BITS_PER_DWORD) - 1;
	skhpb_bits_per_dword_mask = SKHPB_BITS_PER_DWORD - 1;
	SKHPB_DRIVER_D("bits_per_dword %u shift %u mask 0x%X\n",
		  SKHPB_BITS_PER_DWORD, skhpb_bits_per_dword_shift, skhpb_bits_per_dword_mask);
}

static void skhpb_table_mempool_remove(struct skhpb_lu *hpb)
{
	struct skhpb_map_ctx *mctx, *next;
	int i;

	/*
	 * the mctx in the lh_map_ctx has been allocated completely.
	 */
	list_for_each_entry_safe(mctx, next, &hpb->lh_map_ctx, list_table) {
		list_del(&mctx->list_table);

		for (i = 0; i < hpb->mpages_per_subregion; i++)
			__free_page(mctx->m_page[i]);

		kvfree(mctx->ppn_dirty);
		kfree(mctx->m_page);
		kfree(mctx);
		skhpb_alloc_mctx--;
	}
}

static int skhpb_init_pinned_active_block(struct skhpb_lu *hpb,
		struct skhpb_region *cb)
{
	struct skhpb_subregion *cp;
	int subregion, j;
	int err = 0;
	unsigned long flags;

	for (subregion = 0 ; subregion < cb->subregion_count ; subregion++) {
		cp = cb->subregion_tbl + subregion;

		cp->mctx = skhpb_get_map_ctx(hpb);
		if (IS_ERR(cp->mctx)) {
			err = PTR_ERR(cp->mctx);
			goto release;
		}
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		skhpb_add_subregion_to_req_list(hpb, cp);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	}
	return 0;

release:
	for (j = 0 ; j < subregion ; j++) {
		cp = cb->subregion_tbl + j;
		skhpb_put_map_ctx(hpb, cp->mctx);
	}
	return err;
}

static inline bool skhpb_is_pinned(
		struct skhpb_lu_desc *lu_desc, int region)
{
	if (lu_desc->lu_hpb_pinned_end_offset != -1 &&
			region >= lu_desc->hpb_pinned_region_startidx &&
				region <= lu_desc->lu_hpb_pinned_end_offset)
		return true;

	return false;
}

static inline void skhpb_init_jobs(struct skhpb_lu *hpb)
{
	INIT_DELAYED_WORK(&hpb->skhpb_pinned_work, skhpb_pinned_work_handler);
	INIT_DELAYED_WORK(&hpb->skhpb_map_req_retry_work, skhpb_map_req_retry_work_handler);
	INIT_WORK(&hpb->skhpb_rsp_work, skhpb_delayed_rsp_work_handler);
}

static inline void skhpb_cancel_jobs(struct skhpb_lu *hpb)
{
	cancel_delayed_work_sync(&hpb->skhpb_pinned_work);
	cancel_work_sync(&hpb->skhpb_rsp_work);
	cancel_delayed_work_sync(&hpb->skhpb_map_req_retry_work);
}

static void skhpb_init_subregion_tbl(struct skhpb_lu *hpb,
		struct skhpb_region *cb, bool last_region)
{
	int subregion;

	for (subregion = 0 ; subregion < cb->subregion_count ; subregion++) {
		struct skhpb_subregion *cp = cb->subregion_tbl + subregion;

		cp->region = cb->region;
		cp->subregion = subregion;
		cp->subregion_state = SKHPB_SUBREGION_UNUSED;
		cp->last = (last_region && subregion == cb->subregion_count - 1);
	}
}

static inline int skhpb_alloc_subregion_tbl(struct skhpb_lu *hpb,
		struct skhpb_region *cb, int subregion_count)
{
	cb->subregion_tbl = kzalloc(sizeof(struct skhpb_subregion) * subregion_count,
			GFP_KERNEL);
	if (!cb->subregion_tbl)
		return -ENOMEM;

	cb->subregion_count = subregion_count;

	return 0;
}

static int skhpb_table_mempool_init(struct skhpb_lu *hpb,
		int num_regions, int subregions_per_region,
		int entry_count, int entry_byte)
{
	int i, j;
	struct skhpb_map_ctx *mctx = NULL;

	for (i = 0 ; i < num_regions * subregions_per_region ; i++) {
		mctx = kzalloc(sizeof(struct skhpb_map_ctx), GFP_KERNEL);
		if (!mctx)
			goto release_mem;

		mctx->m_page = kzalloc(sizeof(struct page *) *
					hpb->mpages_per_subregion, GFP_KERNEL);
		if (!mctx->m_page)
			goto release_mem;

		mctx->ppn_dirty = kvzalloc(entry_count / BITS_PER_BYTE, GFP_KERNEL);
		if (!mctx->ppn_dirty)
			goto release_mem;

		for (j = 0; j < hpb->mpages_per_subregion; j++) {
			mctx->m_page[j] = alloc_page(GFP_KERNEL | __GFP_ZERO);
			if (!mctx->m_page[j])
				goto release_mem;
		}
		/* SKSKHPB_DRIVER_D("[%d] mctx->m_page %p get_order %d\n", i,
			  mctx->m_page, get_order(hpb->mpages_per_subregion)); */

		INIT_LIST_HEAD(&mctx->list_table);
		list_add(&mctx->list_table, &hpb->lh_map_ctx);

		hpb->debug_free_table++;
	}

	skhpb_alloc_mctx = num_regions * subregions_per_region;
	/* SKSKHPB_DRIVER_D("number of mctx %d %d %d. debug_free_table %d\n",
		  num_regions * subregions_per_region, num_regions,
		  subregions_per_region, hpb->debug_free_table); */
	return 0;

release_mem:
	/*
	 * mctxs already added in lh_map_ctx will be removed
	 * in the caller function.
	 */
	if (!mctx)
		goto out;

	if (mctx->m_page) {
		for (j = 0; j < hpb->mpages_per_subregion; j++)
			if (mctx->m_page[j])
				__free_page(mctx->m_page[j]);
		kfree(mctx->m_page);
	}
	kvfree(mctx->ppn_dirty);
	kfree(mctx);
out:
	return -ENOMEM;
}

static int skhpb_req_mempool_init(struct ufs_hba *hba,
				struct skhpb_lu *hpb, int qd)
{
	struct skhpb_rsp_info *rsp_info = NULL;
	struct skhpb_map_req *map_req = NULL;
	int i, rec_qd;

	if (!qd) {
		qd = hba->nutrs;
		SKHPB_DRIVER_D("hba->nutrs = %d\n", hba->nutrs);
		}
	if (hpb->hpb_ver >= 0x0200)
		rec_qd = hpb->lu_max_active_regions;
	else
		rec_qd = qd;

	INIT_LIST_HEAD(&hpb->lh_rsp_info_free);
	INIT_LIST_HEAD(&hpb->lh_map_req_free);
	INIT_LIST_HEAD(&hpb->lh_map_req_retry);

	hpb->rsp_info = vzalloc(rec_qd * sizeof(struct skhpb_rsp_info));
	if (!hpb->rsp_info)
		goto release_mem;

	hpb->map_req = vzalloc(qd * sizeof(struct skhpb_map_req));
	if (!hpb->map_req)
		goto release_mem;

	for (i = 0; i < rec_qd; i++) {
		rsp_info = hpb->rsp_info + i;
		INIT_LIST_HEAD(&rsp_info->list_rsp_info);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info_free);
	}

	for (i = 0; i < qd; i++) {
		map_req = hpb->map_req + i;
		INIT_LIST_HEAD(&map_req->list_map_req);
		list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_free);
	}

	return 0;

release_mem:
	return -ENOMEM;
}

static void skhpb_init_lu_constant(struct skhpb_lu *hpb,
		struct skhpb_lu_desc *lu_desc,
		struct skhpb_func_desc *func_desc)
{
	unsigned long long region_unit_size, region_mem_size;
	unsigned long long last_subregion_unit_size;
	int entries_per_region;

	/*	From descriptors	*/
	region_unit_size = SKHPB_SECTOR * (1ULL << func_desc->hpb_region_size);
	region_mem_size = region_unit_size / SKHPB_BLOCK * SKHPB_ENTRY_SIZE;

	hpb->subregion_unit_size = SKHPB_SECTOR * (1ULL << func_desc->hpb_subregion_size);
	hpb->subregion_mem_size = hpb->subregion_unit_size /
						SKHPB_BLOCK * SKHPB_ENTRY_SIZE;
	if (func_desc->hpb_region_size == func_desc->hpb_subregion_size)
		hpb->identical_size = true;
	else
		hpb->identical_size = false;

	last_subregion_unit_size =
		lu_desc->lu_logblk_cnt *
		(1ULL << lu_desc->lu_logblk_size) % hpb->subregion_unit_size;
	hpb->last_subregion_mem_size =
		last_subregion_unit_size / SKHPB_BLOCK * SKHPB_ENTRY_SIZE;
	if (hpb->last_subregion_mem_size == 0)
		hpb->last_subregion_mem_size = hpb->subregion_mem_size;
	hpb->hpb_ver = func_desc->hpb_ver;
	hpb->lu_max_active_regions = lu_desc->lu_max_active_hpb_regions;
	hpb->lru_info.max_lru_active_count =
					lu_desc->lu_max_active_hpb_regions
					- lu_desc->lu_num_hpb_pinned_regions;

	/*	relation : lu <-> region <-> sub region <-> entry	 */
	hpb->lu_num_blocks = lu_desc->lu_logblk_cnt;
	entries_per_region = region_mem_size / SKHPB_ENTRY_SIZE;
	hpb->entries_per_subregion = hpb->subregion_mem_size / SKHPB_ENTRY_SIZE;
#if BITS_PER_LONG == 32
	hpb->subregions_per_region = div_u64(region_mem_size, hpb->subregion_mem_size);
#else
	hpb->subregions_per_region = region_mem_size / hpb->subregion_mem_size;
#endif

	hpb->hpb_control_mode = func_desc->hpb_control_mode;
#if defined(SKHPB_READ_LARGE_CHUNK_SUPPORT)
	hpb->ppn_dirties_per_subregion =
		hpb->entries_per_subregion/BITS_PER_PPN_DIRTY;
	/* SKSKHPB_DRIVER_D("ppn_dirties_per_subregion:%d, %d \n",
			hpb->ppn_dirties_per_subregion, BITS_PER_PPN_DIRTY); */
#endif
	/*
	 * 1. regions_per_lu
	 *		= (lu_num_blocks * 4096) / region_unit_size
	 *		= (lu_num_blocks * SKHPB_ENTRY_SIZE) / region_mem_size
	 *		= lu_num_blocks / (region_mem_size / SKHPB_ENTRY_SIZE)
	 *
	 * 2. regions_per_lu = lu_num_blocks / subregion_mem_size (is trik...)
	 *    if SKHPB_ENTRY_SIZE != subregions_per_region, it is error.
	 */
#if BITS_PER_LONG == 32
	hpb->regions_per_lu = div_u64((hpb->lu_num_blocks
			+ (region_mem_size / SKHPB_ENTRY_SIZE) - 1),
			(region_mem_size / SKHPB_ENTRY_SIZE));
	hpb->subregions_per_lu = div_u64((hpb->lu_num_blocks
			+ (hpb->subregion_mem_size / SKHPB_ENTRY_SIZE) - 1),
			(hpb->subregion_mem_size / SKHPB_ENTRY_SIZE));
#else
	hpb->regions_per_lu = (hpb->lu_num_blocks
			+ (region_mem_size / SKHPB_ENTRY_SIZE) - 1)
			/ (region_mem_size / SKHPB_ENTRY_SIZE);
	hpb->subregions_per_lu = (hpb->lu_num_blocks
			+ (hpb->subregion_mem_size / SKHPB_ENTRY_SIZE) - 1)
			/ (hpb->subregion_mem_size / SKHPB_ENTRY_SIZE);
#endif

	/*	mempool info	*/
	hpb->mpage_bytes = PAGE_SIZE;
	hpb->mpages_per_subregion = hpb->subregion_mem_size / hpb->mpage_bytes;

	/*	Bitmask Info.	 */
	hpb->dwords_per_subregion = hpb->entries_per_subregion / SKHPB_BITS_PER_DWORD;
	hpb->entries_per_region_shift = ffs(entries_per_region) - 1;
	hpb->entries_per_region_mask = entries_per_region - 1;
	hpb->entries_per_subregion_shift = ffs(hpb->entries_per_subregion) - 1;
	hpb->entries_per_subregion_mask = hpb->entries_per_subregion - 1;

	SKHPB_DRIVER_I("===== Device Descriptor =====\n");
	SKHPB_DRIVER_I("hpb_region_size = %d, hpb_subregion_size = %d\n",
			func_desc->hpb_region_size,
			func_desc->hpb_subregion_size);
	SKHPB_DRIVER_I("=====   Constant Values   =====\n");
	SKHPB_DRIVER_I("region_unit_size = %llu, region_mem_size %llu\n",
			region_unit_size, region_mem_size);
	SKHPB_DRIVER_I("subregion_unit_size = %llu, subregion_mem_size %d\n",
			hpb->subregion_unit_size, hpb->subregion_mem_size);
	SKHPB_DRIVER_I("last_subregion_mem_size = %d\n", hpb->last_subregion_mem_size);
	SKHPB_DRIVER_I("lu_num_blks = %llu, reg_per_lu = %d, subreg_per_lu = %d\n",
			hpb->lu_num_blocks, hpb->regions_per_lu,
			hpb->subregions_per_lu);
	SKHPB_DRIVER_I("subregions_per_region = %d\n",
			hpb->subregions_per_region);
	SKHPB_DRIVER_I("entries_per_region %u shift %u mask 0x%X\n",
			entries_per_region, hpb->entries_per_region_shift,
			hpb->entries_per_region_mask);
	SKHPB_DRIVER_I("entries_per_subregion %u shift %u mask 0x%X\n",
			hpb->entries_per_subregion,
			hpb->entries_per_subregion_shift,
			hpb->entries_per_subregion_mask);
	SKHPB_DRIVER_I("mpages_per_subregion : %d\n",
			hpb->mpages_per_subregion);
	SKHPB_DRIVER_I("===================================\n");
}

static int skhpb_lu_hpb_init(struct ufs_hba *hba, struct skhpb_lu *hpb,
		struct skhpb_func_desc *func_desc,
		struct skhpb_lu_desc *lu_desc, u8 lun,
		bool *do_work_lun)
{
	struct skhpb_region *cb;
	struct skhpb_subregion *cp;
	int region, subregion;
	int total_subregion_count, subregion_count;
	int ret, j;

	*do_work_lun = false;

	spin_lock_init(&hpb->hpb_lock);
	spin_lock_init(&hpb->rsp_list_lock);
	spin_lock_init(&hpb->map_list_lock);

	/* init lru information */
	INIT_LIST_HEAD(&hpb->lru_info.lru);
	hpb->lru_info.selection_type = TYPE_LRU;

	INIT_LIST_HEAD(&hpb->lh_subregion_req);
	INIT_LIST_HEAD(&hpb->lh_rsp_info);
	INIT_LIST_HEAD(&hpb->lh_map_ctx);

	hpb->lu_hpb_enable = true;

	skhpb_init_lu_constant(hpb, lu_desc, func_desc);

	hpb->region_tbl = vzalloc(sizeof(struct skhpb_region) *	hpb->regions_per_lu);
	if (!hpb->region_tbl)
		return -ENOMEM;

	SKHPB_DRIVER_D("active_block_table bytes: %lu\n",
		(sizeof(struct skhpb_region) * hpb->regions_per_lu));

	ret = skhpb_table_mempool_init(hpb,
			lu_desc->lu_max_active_hpb_regions,
			hpb->subregions_per_region,
			hpb->entries_per_subregion, SKHPB_ENTRY_SIZE);
	if (ret) {
		SKHPB_DRIVER_E("ppn table mempool init fail!\n");
		goto release_mempool;
	}

	ret = skhpb_req_mempool_init(hba, hpb, lu_desc->lu_queue_depth);
	if (ret) {
		SKHPB_DRIVER_E("rsp_info_mempool init fail!\n");
		goto release_mempool;
	}

	total_subregion_count = hpb->subregions_per_lu;

	skhpb_init_jobs(hpb);

	SKHPB_DRIVER_D("total_subregion_count: %d\n", total_subregion_count);
	for (region = 0, subregion_count = 0,
			total_subregion_count = hpb->subregions_per_lu;
			region < hpb->regions_per_lu;
			region++, total_subregion_count -= subregion_count) {
		cb = hpb->region_tbl + region;
		cb->region = region;

		/* init lru region information*/
		INIT_LIST_HEAD(&cb->list_region);
		cb->hit_count = 0;

		subregion_count = min(total_subregion_count,
				hpb->subregions_per_region);
		/* SKSKHPB_DRIVER_D("total: %d subregion_count: %d\n",
				total_subregion_count, subregion_count); */

		ret = skhpb_alloc_subregion_tbl(hpb, cb, subregion_count);
		if (ret)
			goto release_region_cp;
		skhpb_init_subregion_tbl(hpb, cb, region == hpb->regions_per_lu - 1);

		if (skhpb_is_pinned(lu_desc, region)) {
			SKHPB_DRIVER_D("region: %d PINNED %d ~ %d\n",
				region, lu_desc->hpb_pinned_region_startidx,
				lu_desc->lu_hpb_pinned_end_offset);
			ret = skhpb_init_pinned_active_block(hpb, cb);
			if (ret)
				goto release_region_cp;
			*do_work_lun = true;
			cb->is_pinned = true;
			cb->region_state = SKHPB_REGION_ACTIVE;
		} else {
			/* SKSKHPB_DRIVER_D("region: %d inactive\n", cb->region); */
			cb->is_pinned = false;
			cb->region_state = SKHPB_REGION_INACTIVE;
		}
	}
	if (total_subregion_count != 0) {
		SKHPB_DRIVER_E("error total_subregion_count: %d\n",
			total_subregion_count);
		goto release_region_cp;
	}
	hpb->hba = hba;
	hpb->lun = lun;
	/*
	 * even if creating sysfs failed, skhpb could run normally.
	 * so we don't deal with error handling
	 */
	skhpb_create_sysfs(hba, hpb);
	return 0;

release_region_cp:
	for (j = 0 ; j < region ; j++) {
		cb = hpb->region_tbl + j;
		if (cb->subregion_tbl) {
			for (subregion = 0; subregion < cb->subregion_count;
								subregion++) {
				cp = cb->subregion_tbl + subregion;

				if (cp->mctx)
					skhpb_put_map_ctx(hpb, cp->mctx);
			}
			kfree(cb->subregion_tbl);
		}
	}

release_mempool:
	skhpb_table_mempool_remove(hpb);
	*do_work_lun = false;
	return ret;
}

static int skhpb_get_hpb_lu_desc(struct ufs_hba *hba,
		struct skhpb_lu_desc *lu_desc, int lun)
{
	int ret;
	u8 logical_buf[SKHPB_QUERY_DESC_UNIT_MAX_SIZE] = { 0 };

	ret = skhpb_read_unit_desc(hba, lun, logical_buf,
				SKHPB_QUERY_DESC_UNIT_MAX_SIZE);
	if (ret) {
		SKHPB_DRIVER_E("read unit desc failed. ret %d\n", ret);
		return ret;
	}

	lu_desc->lu_queue_depth = logical_buf[UNIT_DESC_PARAM_LU_Q_DEPTH];

	// 2^log, ex) 0x0C = 4KB
	lu_desc->lu_logblk_size = logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_SIZE];
	lu_desc->lu_logblk_cnt =
		get_unaligned_be64(&logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT]);

	if (logical_buf[UNIT_DESC_PARAM_LU_ENABLE] == LU_HPB_ENABLE)
		lu_desc->lu_hpb_enable = true;
	else
		lu_desc->lu_hpb_enable = false;

	lu_desc->lu_max_active_hpb_regions =
		get_unaligned_be16(logical_buf +
						   UNIT_DESC_HPB_LU_MAX_ACTIVE_REGIONS);
	lu_desc->hpb_pinned_region_startidx =
		get_unaligned_be16(logical_buf +
						   UNIT_DESC_HPB_LU_PIN_REGION_START_OFFSET);
	lu_desc->lu_num_hpb_pinned_regions =
		get_unaligned_be16(logical_buf +
						   UNIT_DESC_HPB_LU_NUM_PIN_REGIONS);

	if (lu_desc->lu_hpb_enable) {
		SKHPB_DRIVER_D("LUN(%d) [0A] bLogicalBlockSize %d\n",
				lun, lu_desc->lu_logblk_size);
		SKHPB_DRIVER_D("LUN(%d) [0B] qLogicalBlockCount %llu\n",
				lun, lu_desc->lu_logblk_cnt);
		SKHPB_DRIVER_D("LUN(%d) [03] bLuEnable %d\n",
				lun, logical_buf[UNIT_DESC_PARAM_LU_ENABLE]);
		SKHPB_DRIVER_D("LUN(%d) [06] bLuQueueDepth %d\n",
				lun, lu_desc->lu_queue_depth);
		SKHPB_DRIVER_D("LUN(%d) [23:24] wLUMaxActiveHPBRegions %d\n",
				lun, lu_desc->lu_max_active_hpb_regions);
		SKHPB_DRIVER_D("LUN(%d) [25:26] wHPBPinnedRegionStartIdx %d\n",
				lun, lu_desc->hpb_pinned_region_startidx);
		SKHPB_DRIVER_D("LUN(%d) [27:28] wNumHPBPinnedRegions %d\n",
				lun, lu_desc->lu_num_hpb_pinned_regions);
	}

	if (lu_desc->lu_num_hpb_pinned_regions > 0) {
		lu_desc->lu_hpb_pinned_end_offset =
			lu_desc->hpb_pinned_region_startidx +
			lu_desc->lu_num_hpb_pinned_regions - 1;
	} else
		lu_desc->lu_hpb_pinned_end_offset = -1;

	if (lu_desc->lu_hpb_enable)
		SKHPB_DRIVER_I("Enable, LU: %d, MAX_REGION: %d, PIN: %d - %d\n",
			lun,
			lu_desc->lu_max_active_hpb_regions,
			lu_desc->hpb_pinned_region_startidx,
			lu_desc->lu_num_hpb_pinned_regions);
	return 0;
}

static void skhpb_quirk_setup(struct ufs_hba *hba,
							  struct skhpb_func_desc *desc)
{
	if (hba->dev_quirks & SKHPB_QUIRK_PURGE_HINT_INFO_WHEN_SLEEP) {
		hba->skhpb_quirk |= SKHPB_QUIRK_PURGE_HINT_INFO_WHEN_SLEEP;
		SKHPB_DRIVER_I("QUIRK set PURGE_HINT_INFO_WHEN_SLEEP\n");
	}
}

static int skhpb_read_dev_desc_support(struct ufs_hba *hba,
		struct skhpb_func_desc *desc)
{
	u8 desc_buf[SKHPB_QUERY_DESC_DEVICE_MAX_SIZE];
	int err;

	err = skhpb_read_device_desc(hba, desc_buf,
			SKHPB_QUERY_DESC_DEVICE_MAX_SIZE);
	if (err)
		return err;

	if (desc_buf[DEVICE_DESC_PARAM_UFS_FEAT] &
			SKHPB_UFS_FEATURE_SUPPORT_HPB_BIT) {
		hba->skhpb_feat |= SKHPB_UFS_FEATURE_SUPPORT_HPB_BIT;
		SKHPB_DRIVER_I("FeaturesSupport= support\n");
	} else {
		SKHPB_DRIVER_I("FeaturesSupport= not support\n");
		return -ENODEV;
	}

	desc->lu_cnt = desc_buf[DEVICE_DESC_PARAM_NUM_LU];
	SKHPB_DRIVER_D("Dev LU count= %d\n", desc->lu_cnt);

	desc->spec_ver =
		(u16)SKHPB_SHIFT_BYTE_1(desc_buf[DEVICE_DESC_PARAM_SPEC_VER]) |
		(u16)SKHPB_SHIFT_BYTE_0(desc_buf[DEVICE_DESC_PARAM_SPEC_VER + 1]);
	SKHPB_DRIVER_I("Dev Spec Ver= %x.%x\n",
				   SKHPB_GET_BYTE_1(desc->spec_ver),
				   SKHPB_GET_BYTE_0(desc->spec_ver));

	desc->hpb_ver =
		(u16)SKHPB_SHIFT_BYTE_1(desc_buf[DEVICE_DESC_PARAM_HPB_VER]) |
		(u16)SKHPB_SHIFT_BYTE_0(desc_buf[DEVICE_DESC_PARAM_HPB_VER + 1]);
	SKHPB_DRIVER_I("Dev Ver= %x.%x.%x, DD Ver= %x.%x.%x\n",
			(desc->hpb_ver >> 8) & 0xf,
			(desc->hpb_ver >> 4) & 0xf,
			(desc->hpb_ver >> 0) & 0xf,
			SKHPB_GET_BYTE_2(SKHPB_DD_VER),
			SKHPB_GET_BYTE_1(SKHPB_DD_VER),
			SKHPB_GET_BYTE_0(SKHPB_DD_VER));

	skhpb_quirk_setup(hba, desc);

	if (hba->skhpb_quirk & SKHPB_QUIRK_ALWAYS_DEVICE_CONTROL_MODE)
		desc->hpb_control_mode = DEV_CTRL_MODE;
	else
		desc->hpb_control_mode = (u8)desc_buf[DEVICE_DESC_PARAM_HPB_CONTROL];


	SKHPB_DRIVER_I("HPB Control Mode = %s",
			(desc->hpb_control_mode)?"DEV MODE":"HOST MODE");
	if (desc->hpb_control_mode == HOST_CTRL_MODE) {
		SKHPB_DRIVER_E("Driver does not support Host Control Mode");
		return -ENODEV;
	}
	hba->hpb_control_mode = desc->hpb_control_mode;
	return 0;
}

static int skhpb_read_geo_desc_support(struct ufs_hba *hba,
		struct skhpb_func_desc *desc)
{
	int err;
	u8 geometry_buf[SKHPB_QUERY_DESC_GEOMETRY_MAX_SIZE];

	err = skhpb_read_geo_desc(hba, geometry_buf,
				SKHPB_QUERY_DESC_GEOMETRY_MAX_SIZE);
	if (err)
		return err;

	desc->hpb_region_size = geometry_buf[GEOMETRY_DESC_HPB_REGION_SIZE];
	desc->hpb_number_lu = geometry_buf[GEOMETRY_DESC_HPB_NUMBER_LU];
	desc->hpb_subregion_size =
			geometry_buf[GEOMETRY_DESC_HPB_SUBREGION_SIZE];
	desc->hpb_device_max_active_regions =
			get_unaligned_be16(geometry_buf
				+ GEOMETRY_DESC_HPB_DEVICE_MAX_ACTIVE_REGIONS);

	SKHPB_DRIVER_D("[48] bHPBRegionSize %u\n", desc->hpb_region_size);
	SKHPB_DRIVER_D("[49] bHPBNumberLU %u\n", desc->hpb_number_lu);
	SKHPB_DRIVER_D("[4A] bHPBSubRegionSize %u\n", desc->hpb_subregion_size);
	SKHPB_DRIVER_D("[4B:4C] wDeviceMaxActiveHPBRegions %u\n",
			desc->hpb_device_max_active_regions);

	if (desc->hpb_number_lu == 0) {
		SKHPB_DRIVER_E("HPB is not supported\n");
		return -ENODEV;
	}
	/* for activation */
	hba->skhpb_max_regions = desc->hpb_device_max_active_regions;
	return 0;
}

int skhpb_control_validation(struct ufs_hba *hba,
				struct skhpb_config_desc *config)
{
	unsigned int num_regions = 0;
	int lun;

	if (!(hba->skhpb_feat & SKHPB_UFS_FEATURE_SUPPORT_HPB_BIT))
		return -ENOTSUPP;

	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		unsigned char *unit = config->unit[lun];

		if (unit[SKHPB_CONF_LU_ENABLE] >= LU_SET_MAX)
			return -EINVAL;

		/* total should not exceed max_active_regions */
		num_regions += unit[SKHPB_CONF_ACTIVE_REGIONS] << 8;
		num_regions += unit[SKHPB_CONF_ACTIVE_REGIONS + 1];
		if (num_regions > hba->skhpb_max_regions)
			return -EINVAL;
	}
	return 0;
}

static int skhpb_init(struct ufs_hba *hba)
{
	struct skhpb_func_desc func_desc;
	int ret, retries;
	u8 lun;
	int hpb_dev = 0;
	bool do_work;

	pm_runtime_get_sync(hba->dev);
	ret = skhpb_read_dev_desc_support(hba, &func_desc);
	if (ret)
		goto out_state;

	ret = skhpb_read_geo_desc_support(hba, &func_desc);
	if (ret)
		goto out_state;

	for (retries = 0; retries < 20; retries++) {
		if (!hba->lrb_in_use) {
			ret = ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_SET_FLAG,
				QUERY_FLAG_IDN_HPB_RESET, NULL);
			if (!ret) {
				SKHPB_DRIVER_I("Query fHPBReset is successfully sent retries = %d\n", retries);
				break;
			}
		} else
			msleep(200);
	}
	if (ret == 0) {
		bool fHPBReset = true;

		ufshcd_query_flag_retry(hba,
				UPIU_QUERY_OPCODE_READ_FLAG,
				QUERY_FLAG_IDN_HPB_RESET,
				&fHPBReset);
		if (fHPBReset)
			SKHPB_DRIVER_I("fHPBReset still set\n");
	}

	skhpb_init_constant();

	do_work = false;
	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		struct skhpb_lu_desc lu_desc = {0};
		bool do_work_lun = false;

		ret = skhpb_get_hpb_lu_desc(hba, &lu_desc, lun);
		if (ret)
			goto out_state;

		if (lu_desc.lu_hpb_enable == false)
			continue;

		hba->skhpb_lup[lun] = kzalloc(sizeof(struct skhpb_lu),
								GFP_KERNEL);
		if (!hba->skhpb_lup[lun]) {
			ret = -ENOMEM;
			goto out_free_mem;
		}

		ret = skhpb_lu_hpb_init(hba, hba->skhpb_lup[lun],
				&func_desc, &lu_desc, lun, &do_work_lun);
		if (ret) {
			if (ret == -ENODEV)
				continue;
			else
				goto out_free_mem;
		}
		do_work |= do_work_lun;
		hba->skhpb_quicklist_lu_enable[hpb_dev] = lun;
		SKHPB_DRIVER_D("skhpb_quicklist_lu_enable[%d] = %d\n", hpb_dev, lun);
		hpb_dev++;
	}

	if (hpb_dev)
		goto done;

	goto out_free_mem;

done:
	INIT_WORK(&hba->skhpb_eh_work, skhpb_error_handler);
	hba->skhpb_state = SKHPB_PRESENT;
	hba->issue_ioctl = false;
	pm_runtime_mark_last_busy(hba->dev);
	pm_runtime_put_noidle(hba->dev);

	if (do_work) {
		for (lun = 0; lun < UFS_UPIU_MAX_GENERAL_LUN; lun++) {
			struct skhpb_lu *hpb = hba->skhpb_lup[lun];

			if (hpb)
				schedule_delayed_work(&hpb->skhpb_pinned_work, 0);
		}
	}

	return 0;

out_free_mem:
	skhpb_release(hba, SKHPB_NOT_SUPPORTED);
out_state:
	hba->skhpb_state = SKHPB_NOT_SUPPORTED;
	pm_runtime_mark_last_busy(hba->dev);
	pm_runtime_put_noidle(hba->dev);
	return ret;
}

static void skhpb_map_loading_trigger(struct skhpb_lu *hpb,
		bool only_pinned, bool do_work_handler)
{
	int region, subregion;
	unsigned long flags;

	if (do_work_handler)
		goto work_out;
	flush_delayed_work(&hpb->skhpb_pinned_work);
	for (region = 0 ; region < hpb->regions_per_lu ; region++) {
		struct skhpb_region *cb;

		cb = hpb->region_tbl + region;

		if (cb->region_state != SKHPB_REGION_ACTIVE &&
				!cb->is_pinned)
			continue;

		if ((only_pinned && cb->is_pinned) ||
				!only_pinned) {
			spin_lock_irqsave(&hpb->hpb_lock, flags);
			for (subregion = 0; subregion < cb->subregion_count;
								subregion++)
				skhpb_add_subregion_to_req_list(hpb,
						cb->subregion_tbl + subregion);
			spin_unlock_irqrestore(&hpb->hpb_lock, flags);
			do_work_handler = true;
		}
	}
work_out:
	if (do_work_handler)
		schedule_delayed_work(&hpb->skhpb_pinned_work, 0);
}

static void skhpb_purge_active_block(struct skhpb_lu *hpb)
{
	int region, subregion;
	int state;
	struct skhpb_region *cb;
	struct skhpb_subregion *cp;
	unsigned long flags;

	spin_lock_irqsave(&hpb->hpb_lock, flags);
	for (region = 0 ; region < hpb->regions_per_lu ; region++) {
		cb = hpb->region_tbl + region;

		if (cb->region_state == SKHPB_REGION_INACTIVE) {
			continue;
		}

		if (cb->is_pinned) {
			state = SKHPB_SUBREGION_DIRTY;
		} else if (cb->region_state == SKHPB_REGION_ACTIVE) {
			state = SKHPB_SUBREGION_UNUSED;
			skhpb_cleanup_lru_info(&hpb->lru_info, cb);
		} else {
			SKHPB_DRIVER_E("Unsupported state of region\n");
			continue;
		}

		SKHPB_DRIVER_D("region %d state %d dft %d\n",
				region, state,
				hpb->debug_free_table);
		for (subregion = 0 ; subregion < cb->subregion_count;
							subregion++) {
			cp = cb->subregion_tbl + subregion;

			skhpb_purge_active_page(hpb, cp, state);
		}
		SKHPB_DRIVER_D("region %d state %d dft %d\n",
				region, state, hpb->debug_free_table);
	}
	spin_unlock_irqrestore(&hpb->hpb_lock, flags);
}

static void skhpb_retrieve_rsp_info(struct skhpb_lu *hpb)
{
	struct skhpb_rsp_info *rsp_info;
	unsigned long flags;

	while (1) {
		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		rsp_info = list_first_entry_or_null(&hpb->lh_rsp_info,
				struct skhpb_rsp_info, list_rsp_info);
		if (!rsp_info) {
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
			break;
		}
		list_move_tail(&rsp_info->list_rsp_info,
				&hpb->lh_rsp_info_free);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
		SKHPB_DRIVER_D("add_list %p -> %p",
				&hpb->lh_rsp_info_free, rsp_info);
	}
}

static void skhpb_probe(struct ufs_hba *hba)
{
	struct skhpb_lu *hpb;
	int lu;

	for (lu = 0 ; lu < UFS_UPIU_MAX_GENERAL_LUN ; lu++) {
		hpb = hba->skhpb_lup[lu];

		if (hpb && hpb->lu_hpb_enable) {
			skhpb_cancel_jobs(hpb);
			skhpb_retrieve_rsp_info(hpb);
			skhpb_purge_active_block(hpb);
			SKHPB_DRIVER_I("SKHPB lun %d reset\n", lu);
//			tasklet_init(&hpb->hpb_work_handler,
//				skhpb_work_handler_fn, (unsigned long)hpb);
		}
	}
	hba->skhpb_state = SKHPB_PRESENT;
}

static void skhpb_destroy_subregion_tbl(struct skhpb_lu *hpb,
		struct skhpb_region *cb)
{
	int subregion;

	for (subregion = 0 ; subregion < cb->subregion_count ; subregion++) {
		struct skhpb_subregion *cp;

		cp = cb->subregion_tbl + subregion;
		cp->subregion_state = SKHPB_SUBREGION_UNUSED;
		skhpb_put_map_ctx(hpb, cp->mctx);
	}

	kfree(cb->subregion_tbl);
}

static void skhpb_destroy_region_tbl(struct skhpb_lu *hpb)
{
	int region;

	if (!hpb->region_tbl)
		return;

	for (region = 0 ; region < hpb->regions_per_lu ; region++) {
		struct skhpb_region *cb;

		cb = hpb->region_tbl + region;
		if (cb->region_state == SKHPB_REGION_ACTIVE) {
			cb->region_state = SKHPB_REGION_INACTIVE;

			skhpb_destroy_subregion_tbl(hpb, cb);
		}
	}
	vfree(hpb->region_tbl);
}

void skhpb_suspend(struct ufs_hba *hba)
{
	int lun;
	unsigned long flags;
	struct skhpb_lu *hpb;
	struct skhpb_rsp_info *rsp_info;
	struct skhpb_map_req *map_req;

	if (hba->skhpb_quirk & SKHPB_QUIRK_PURGE_HINT_INFO_WHEN_SLEEP) {
		for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
			hpb = hba->skhpb_lup[lun];
			if (!hpb)
				continue;

			while (1) {
				spin_lock_irqsave(&hpb->rsp_list_lock, flags);
				/* break if lh_rsp_info list_head not init yet. */
				if (!hpb->lh_rsp_info.next) {
					spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
					break;
				}
				rsp_info = list_first_entry_or_null(&hpb->lh_rsp_info,
													struct skhpb_rsp_info, list_rsp_info);
				if (!rsp_info) {
					spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
					break;
				}
				list_del_init(&rsp_info->list_rsp_info);
				list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info_free);
				atomic64_inc(&hpb->canceled_resp);
				spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
			}
			while (1) {
				spin_lock_irqsave(&hpb->map_list_lock, flags);
				/* break if lh_map_req_retry list_head not init yet. */
				if (!hpb->lh_map_req_retry.next) {
					spin_unlock_irqrestore(&hpb->map_list_lock, flags);
					break;
				}
				map_req = list_first_entry_or_null(&hpb->lh_map_req_retry,
												   struct skhpb_map_req, list_map_req);
				if (!map_req) {
					spin_unlock_irqrestore(&hpb->map_list_lock, flags);
					break;
				}
				list_del_init(&map_req->list_map_req);
				list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_free);
				atomic64_inc(&hpb->canceled_map_req);
				spin_unlock_irqrestore(&hpb->map_list_lock, flags);
			}
		}
	}
	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		hpb = hba->skhpb_lup[lun];
		if (!hpb)
			continue;
		skhpb_cancel_jobs(hpb);
	}
}

void skhpb_resume(struct ufs_hba *hba)
{
	int lun;
	struct skhpb_lu *hpb;
	unsigned long flags;

	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		hpb = hba->skhpb_lup[lun];
		if (!hpb)
			continue;
		spin_lock_irqsave(&hpb->map_list_lock, flags);
		if (!list_empty(&hpb->lh_map_req_retry))
			schedule_delayed_work(&hpb->skhpb_map_req_retry_work, msecs_to_jiffies(1000));
		spin_unlock_irqrestore(&hpb->map_list_lock, flags);
	}
}

void skhpb_release(struct ufs_hba *hba, int state)
{
	int lun;

	hba->skhpb_state = SKHPB_FAILED;

	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		struct skhpb_lu *hpb = hba->skhpb_lup[lun];

		if (!hpb)
			continue;

		hba->skhpb_lup[lun] = NULL;

		if (!hpb->lu_hpb_enable)
			continue;

		hpb->lu_hpb_enable = false;

		skhpb_cancel_jobs(hpb);

		skhpb_destroy_region_tbl(hpb);
		skhpb_table_mempool_remove(hpb);

		vfree(hpb->rsp_info);
		vfree(hpb->map_req);

		kobject_uevent(&hpb->kobj, KOBJ_REMOVE);
		kobject_del(&hpb->kobj); // TODO --count & del?

		kfree(hpb);
	}

	if (skhpb_alloc_mctx != 0)
		SKHPB_DRIVER_E("warning: skhpb_alloc_mctx %d", skhpb_alloc_mctx);

	hba->skhpb_state = state;
}

void skhpb_init_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ufs_hba *hba =
		container_of(dwork, struct ufs_hba, skhpb_init_work);
	struct Scsi_Host *shost = hba->host;
	bool async_scan;

	if (hba->skhpb_state == SKHPB_NOT_SUPPORTED)
		return;

	spin_lock_irq(shost->host_lock);
	async_scan = shost->async_scan;
	spin_unlock_irq(shost->host_lock);
	if (async_scan) {
		schedule_delayed_work(dwork, msecs_to_jiffies(100));
		return;
	}

	if (hba->skhpb_state == SKHPB_NEED_INIT) {
		int err = skhpb_init(hba);

		if (hba->skhpb_state == SKHPB_NOT_SUPPORTED)
			SKHPB_DRIVER_E("Run without HPB - err=%d\n", err);
	} else if (hba->skhpb_state == SKHPB_RESET) {
		skhpb_probe(hba);
	}
}

void ufshcd_init_hpb(struct ufs_hba *hba)
{
	int lun;

	hba->skhpb_feat = 0;
	hba->skhpb_quirk = 0;
	hba->skhpb_state = SKHPB_NEED_INIT;
	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		hba->skhpb_lup[lun] = NULL;
		hba->sdev_ufs_lu[lun] = NULL;
		hba->skhpb_quicklist_lu_enable[lun] = SKHPB_U8_MAX;
	}

	INIT_DELAYED_WORK(&hba->skhpb_init_work, skhpb_init_handler);
}

static void skhpb_error_handler(struct work_struct *work)
{
	struct ufs_hba *hba;

	hba = container_of(work, struct ufs_hba, skhpb_eh_work);

	SKHPB_DRIVER_E("SKHPB driver runs without SKHPB\n");
	SKHPB_DRIVER_E("SKHPB will be removed from the kernel\n");

	skhpb_release(hba, SKHPB_FAILED);
}

static void skhpb_stat_init(struct skhpb_lu *hpb)
{
	atomic64_set(&hpb->hit, 0);
	atomic64_set(&hpb->size_miss, 0);
	atomic64_set(&hpb->region_miss, 0);
	atomic64_set(&hpb->subregion_miss, 0);
	atomic64_set(&hpb->entry_dirty_miss, 0);
	atomic64_set(&hpb->w_map_req_cnt, 0);
#if defined(SKHPB_READ_LARGE_CHUNK_SUPPORT)
	atomic64_set(&hpb->lc_entry_dirty_miss, 0);
	atomic64_set(&hpb->lc_reg_subreg_miss, 0);
	atomic64_set(&hpb->lc_hit, 0);
#endif
	atomic64_set(&hpb->rb_noti_cnt, 0);
	atomic64_set(&hpb->reset_noti_cnt, 0);
	atomic64_set(&hpb->map_req_cnt, 0);
	atomic64_set(&hpb->region_evict, 0);
	atomic64_set(&hpb->region_add, 0);
	atomic64_set(&hpb->rb_fail, 0);
	atomic64_set(&hpb->canceled_resp, 0);
	atomic64_set(&hpb->canceled_map_req, 0);
	atomic64_set(&hpb->alloc_map_req_cnt, 0);
}


static ssize_t skhpb_sysfs_info_from_region_store(struct skhpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long long value = 0;
	int region, subregion;
	struct skhpb_region *cb;
	struct skhpb_subregion *cp;

	if (kstrtoull(buf, 0, &value)) {
		SKHPB_DRIVER_E("kstrtoul error\n");
		return -EINVAL;
	}

	if (value >= hpb->regions_per_lu) {
		SKHPB_DRIVER_E("value %llu >= regions_per_lu %d error\n",
			value, hpb->regions_per_lu);
		return -EINVAL;
	}

	region = (int)value;
	cb = hpb->region_tbl + region;

	SKHPB_DRIVER_I("get_info_from_region[%d]=", region);
	SKHPB_DRIVER_I("region %u state %s", region,
			   ((cb->region_state == SKHPB_REGION_INACTIVE) ? "INACTIVE" :
				((cb->region_state == SKHPB_REGION_ACTIVE) ? "ACTIVE" : "INVALID"))
			   );
	for (subregion = 0; subregion < cb->subregion_count; subregion++) {
		cp = cb->subregion_tbl + subregion;
		SKHPB_DRIVER_I("subregion %u state %s", subregion,
				   ((cp->subregion_state == SKHPB_SUBREGION_UNUSED) ? "UNUSED" :
					((cp->subregion_state == SKHPB_SUBREGION_DIRTY) ? "DIRTY" :
					 ((cp->subregion_state == SKHPB_SUBREGION_CLEAN) ? "CLEAN" :
					  (cp->subregion_state == SKHPB_SUBREGION_ISSUED) ?
					  "ISSUED" : "INVALID")))
				  );
	}
	return count;
}

static ssize_t skhpb_sysfs_info_from_lba_store(struct skhpb_lu *hpb,
		const char *buf, size_t count)
{
	skhpb_t ppn;
	unsigned long long value = 0;
	unsigned int lpn;
	int region, subregion, subregion_offset;
	struct skhpb_region *cb;
	struct skhpb_subregion *cp;
	unsigned long flags;
	int dirty;

	if (kstrtoull(buf, 0, &value)) {
		SKHPB_DRIVER_E("kstrtoul error\n");
		return -EINVAL;
	}

	if (value > hpb->lu_num_blocks * SKHPB_SECTORS_PER_BLOCK) {
		SKHPB_DRIVER_E("value %llu > lu_num_blocks %llu error\n",
			value, hpb->lu_num_blocks);
		return -EINVAL;
	}
	lpn = value / SKHPB_SECTORS_PER_BLOCK;

	skhpb_get_pos_from_lpn(hpb, lpn, &region, &subregion,
						&subregion_offset);
	if (!skhpb_check_region_subregion_validity(hpb, region, subregion))
		return -EINVAL;
	cb = hpb->region_tbl + region;
	cp = cb->subregion_tbl + subregion;

	if (cb->region_state != SKHPB_REGION_INACTIVE) {
		ppn = skhpb_get_ppn(cp->mctx, subregion_offset);
		spin_lock_irqsave(&hpb->hpb_lock, flags);
		dirty = skhpb_ppn_dirty_check(hpb, cp, subregion_offset);
		spin_unlock_irqrestore(&hpb->hpb_lock, flags);
	} else {
		ppn = 0;
		dirty = -1;
	}
	SKHPB_DRIVER_I("get_info_from_lba[%llu]=", value);
	SKHPB_DRIVER_I("sector %llu region %d state %s subregion %d state %s",
			   value, region,
			   ((cb->region_state == SKHPB_REGION_INACTIVE) ? "INACTIVE" :
				((cb->region_state == SKHPB_REGION_ACTIVE) ? "ACTIVE" : "INVALID")),
			   subregion,
			   ((cp->subregion_state == SKHPB_SUBREGION_UNUSED) ? "UNUSED" :
				((cp->subregion_state == SKHPB_SUBREGION_DIRTY) ? "DIRTY" :
				 ((cp->subregion_state == SKHPB_SUBREGION_CLEAN) ? "CLEAN" :
				  (cp->subregion_state == SKHPB_SUBREGION_ISSUED) ?
				  "ISSUED":"INVALID")))
			  );
	SKHPB_DRIVER_I("sector %llu lpn %u ppn %llx dirty %d",
			value, lpn, ppn, dirty);
	return count;
}

static ssize_t skhpb_sysfs_map_req_show(struct skhpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
					"map_req_count[RB_NOTI RESET_NOTI MAP_REQ]= %lld %lld %lld\n",
			(long long)atomic64_read(&hpb->rb_noti_cnt),
			(long long)atomic64_read(&hpb->reset_noti_cnt),
			(long long)atomic64_read(&hpb->map_req_cnt));
}

static ssize_t skhpb_sysfs_count_reset_store(struct skhpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long debug;

	if (kstrtoul(buf, 0, &debug))
		return -EINVAL;

	skhpb_stat_init(hpb);

	return count;
}

static ssize_t skhpb_sysfs_add_evict_show(struct skhpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "add_evict_count[ADD EVICT]= %lld %lld\n",
			(long long)atomic64_read(&hpb->region_add),
			(long long)atomic64_read(&hpb->region_evict));
}

static ssize_t skhpb_sysfs_statistics_show(struct skhpb_lu *hpb, char *buf)
{
	long long size_miss, region_miss, subregion_miss, entry_dirty_miss,
		 hit, miss_all, rb_fail, canceled_resp, canceled_map_req;
#if defined(SKHPB_READ_LARGE_CHUNK_SUPPORT)
	long long lc_dirty_miss = 0, lc_state_miss = 0, lc_hit = 0;
#endif
	int count = 0;

	hit = atomic64_read(&hpb->hit);
	size_miss = atomic64_read(&hpb->size_miss);
	region_miss = atomic64_read(&hpb->region_miss);
	subregion_miss = atomic64_read(&hpb->subregion_miss);
	entry_dirty_miss = atomic64_read(&hpb->entry_dirty_miss);
	rb_fail = atomic64_read(&hpb->rb_fail);
	canceled_resp = atomic64_read(&hpb->canceled_resp);
	canceled_map_req = atomic64_read(&hpb->canceled_map_req);

#if defined(SKHPB_READ_LARGE_CHUNK_SUPPORT)
	lc_dirty_miss = atomic64_read(&hpb->lc_entry_dirty_miss);
	lc_state_miss = atomic64_read(&hpb->lc_reg_subreg_miss);
	lc_hit = atomic64_read(&hpb->lc_hit);
	entry_dirty_miss += lc_dirty_miss;
	subregion_miss += lc_state_miss;
#endif
	miss_all = size_miss + region_miss + subregion_miss + entry_dirty_miss;
	count += snprintf(buf + count, PAGE_SIZE,
			"Total: %lld\nHit_Counts: %lld\n",
			hit + miss_all, hit);
	count += snprintf(buf + count, PAGE_SIZE,
			"Miss_Counts[ALL SIZE REG SUBREG DIRTY]= %lld %lld %lld %lld %lld\n",
			miss_all, size_miss, region_miss, subregion_miss, entry_dirty_miss);

#if defined(SKHPB_READ_LARGE_CHUNK_SUPPORT)
	count += snprintf(buf + count, PAGE_SIZE,
			"LARG_CHNK  Hit_Count: %lld, miss_count[ALL DIRTY REG_SUBREG]= %lld %lld %lld\n",
			lc_hit, lc_dirty_miss + lc_state_miss, lc_dirty_miss, lc_state_miss);
#endif
	if (hpb->hba->skhpb_quirk & SKHPB_QUIRK_PURGE_HINT_INFO_WHEN_SLEEP)
		count += snprintf(buf + count, PAGE_SIZE,
				"READ_BUFFER miss_count[RB_FAIL CANCEL_RESP CANCEL_MAP]= %lld %lld %lld\n",
				rb_fail, canceled_resp, canceled_map_req);
	else
		count += snprintf(buf + count, PAGE_SIZE,
				"READ_BUFFER miss_count[RB_FAIL]= %lld\n", rb_fail);
	return count;
}

static ssize_t skhpb_sysfs_version_show(struct skhpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"HPBversion[HPB DD]= %x.%x.%x %x.%x.%x\n",
			(hpb->hpb_ver >> 8) & 0xf,
			(hpb->hpb_ver >> 4) & 0xf,
			(hpb->hpb_ver >> 0) & 0xf,
			SKHPB_GET_BYTE_2(SKHPB_DD_VER),
			SKHPB_GET_BYTE_1(SKHPB_DD_VER),
			SKHPB_GET_BYTE_0(SKHPB_DD_VER));
}

static ssize_t skhpb_sysfs_active_count_show(
	struct skhpb_lu *hpb, char *buf)
{
	struct skhpb_region *cb;
	int region;
	int pinned_cnt = 0;

	for (region = 0 ; region < hpb->regions_per_lu ; region++) {
		cb = hpb->region_tbl + region;

		if (cb->is_pinned && cb->region_state == SKHPB_REGION_ACTIVE)
			pinned_cnt++;
	}
	return snprintf(buf, PAGE_SIZE,
					"active_count[ACTIVE_CNT PINNED_CNT]= %ld %d\n",
					 atomic64_read(&hpb->lru_info.active_count), pinned_cnt);
}

static ssize_t skhpb_sysfs_hpb_disable_show(
	struct skhpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"[0] BOTH_enabled= %d\
		\n[1] ONLY_HPB_BUFFER_disabled= %d\
		\n[2] ONLY_HPB_READ_disabled= %d\
		\n[3] BOTH_disabled= %d\n",
		(hpb->force_map_req_disable == 0 &&
		 hpb->force_hpb_read_disable == 0)?1:0,
		hpb->force_map_req_disable,
		hpb->force_hpb_read_disable,
		(hpb->force_map_req_disable == 1 &&
		 hpb->force_hpb_read_disable == 1)?1:0);
}

static ssize_t skhpb_sysfs_hpb_disable_store(struct skhpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value = 0;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 3) {
		SKHPB_DRIVER_E("Error, Only [0-3] is valid:\
			 \n[0] BOTH_enable\
			 \n[1] ONLY_HPB_BUFFER_disable\
			 \n[2] ONLY_HPB_READ_disable\
			 \n[3] BOTH_disabie\n");
		return -EINVAL;
	}

	hpb->force_map_req_disable = value & 0x1;
	hpb->force_hpb_read_disable = value & 0x2;
	return count;
}

static ssize_t skhpb_sysfs_hpb_reset_store(struct skhpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value = 0;
	unsigned int doorbel;
	struct skhpb_rsp_info *rsp_info;
	unsigned long flags;
	int ret = 0, retries, lun;
	struct skhpb_lu *hpb_lu;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;
	if (!value)
		return count;

	for (retries = 1; retries <= 20; retries++) {
		pm_runtime_get_sync(SKHPB_DEV(hpb));
		ufshcd_hold(hpb->hba, false);
		doorbel = ufshcd_readl(hpb->hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
		ufshcd_release(hpb->hba);
		pm_runtime_mark_last_busy(SKHPB_DEV(hpb));
		pm_runtime_put_noidle(SKHPB_DEV(hpb));

		if (!doorbel) {
			pm_runtime_get_sync(SKHPB_DEV(hpb));
			ret = ufshcd_query_flag_retry(hpb->hba,
					UPIU_QUERY_OPCODE_SET_FLAG,
					QUERY_FLAG_IDN_HPB_RESET,
					NULL);
			pm_runtime_mark_last_busy(SKHPB_DEV(hpb));
			pm_runtime_put_noidle(SKHPB_DEV(hpb));
			if (!ret) {
				SKHPB_DRIVER_I("Query fHPBReset is successfully sent\n");
				break;
			}
		}
		SKHPB_DRIVER_I("fHPBReset failed and will retry[%d] after some time, DOORBELL: 0x%x\n",
				 retries, doorbel);
		msleep(200);
	}
	if (ret == 0) {
		bool fHPBReset = true;
		pm_runtime_get_sync(SKHPB_DEV(hpb));
		ufshcd_query_flag_retry(hpb->hba,
				UPIU_QUERY_OPCODE_READ_FLAG,
				QUERY_FLAG_IDN_HPB_RESET,
				&fHPBReset);
		 pm_runtime_mark_last_busy(SKHPB_DEV(hpb));
		 pm_runtime_put_noidle(SKHPB_DEV(hpb));
		if (fHPBReset)
			SKHPB_DRIVER_E("fHPBReset is still in progress at device, but keep going\n");
	} else {
		SKHPB_DRIVER_E("Fail to set fHPBReset flag\n");
		goto out;
	}
	flush_delayed_work(&hpb->skhpb_pinned_work);
	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		hpb_lu = hpb->hba->skhpb_lup[lun];
		if (!hpb_lu || !hpb_lu->lu_hpb_enable)
			continue;

		skhpb_stat_init(hpb_lu);

		spin_lock_irqsave(&hpb_lu->rsp_list_lock, flags);
		rsp_info = skhpb_get_req_info(hpb_lu);
		spin_unlock_irqrestore(&hpb_lu->rsp_list_lock, flags);
		if (!rsp_info)
			goto out;
		rsp_info->type = SKHPB_RSP_HPB_RESET;
		SKHPB_RSP_TIME(rsp_info->RSP_start);
		spin_lock_irqsave(&hpb_lu->rsp_list_lock, flags);
		list_add_tail(&rsp_info->list_rsp_info, &hpb_lu->lh_rsp_info);
		spin_unlock_irqrestore(&hpb_lu->rsp_list_lock, flags);
		SKHPB_DRIVER_I("Host HPB reset start LU%d - fHPBReset\n", lun);
		schedule_work(&hpb_lu->skhpb_rsp_work);
	}
out:
	return count;
}

static ssize_t skhpb_sysfs_debug_log_show(
	struct skhpb_lu *hpb, char *buf)
{
	int value = skhpb_debug_mask;

	if (value == (SKHPB_LOG_OFF)) {
		value = SKHPB_LOG_LEVEL_OFF;
	} else if (value == (SKHPB_LOG_ERR)) {
		value = SKHPB_LOG_LEVEL_ERR;
	} else if (value == (SKHPB_LOG_ERR | SKHPB_LOG_INFO)) {
		value = SKHPB_LOG_LEVEL_INFO;
	} else if (value == (SKHPB_LOG_ERR | SKHPB_LOG_INFO | SKHPB_LOG_DEBUG)) {
		value = SKHPB_LOG_LEVEL_DEBUG;
	} else if (value == (SKHPB_LOG_ERR | SKHPB_LOG_INFO | SKHPB_LOG_DEBUG | SKHPB_LOG_HEX)) {
		value = SKHPB_LOG_LEVEL_HEX;
	}

	return snprintf(buf, PAGE_SIZE, "[0] : LOG_LEVEL_OFF\
			\n[1] : LOG_LEVEL_ERR\
			\n[2] : LOG_LEVEL_INFO\
			\n[3] : LOG_LEVEL_DEBUG\
			\n[4] : LOG_LEVEL_HEX\
			\n-----------------------\
			\nLog-Level = %d\n", value);
}

static ssize_t skhpb_sysfs_debug_log_store(struct skhpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value = 0;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > SKHPB_LOG_LEVEL_HEX) {
		SKHPB_DRIVER_E("Error, Only [0-4] is valid:\
			 \n[0] : LOG_LEVEL_OFF\
			 \n[1] : LOG_LEVEL_ERR\
			 \n[2] : LOG_LEVEL_INFO\
			 \n[3] : LOG_LEVEL_DEBUG\
			 \n[4] : LOG_LEVEL_HEX\n");
		return -EINVAL;
	}

	if (value == SKHPB_LOG_LEVEL_OFF) {
		skhpb_debug_mask = SKHPB_LOG_OFF;
	} else if (value == SKHPB_LOG_LEVEL_ERR) {
		skhpb_debug_mask = SKHPB_LOG_ERR;
	} else if (value == SKHPB_LOG_LEVEL_INFO) {
		skhpb_debug_mask = SKHPB_LOG_ERR | SKHPB_LOG_INFO;
	} else if (value == SKHPB_LOG_LEVEL_DEBUG) {
		skhpb_debug_mask = SKHPB_LOG_ERR | SKHPB_LOG_INFO | SKHPB_LOG_DEBUG;
	} else if (value == SKHPB_LOG_LEVEL_HEX) {
		skhpb_debug_mask = SKHPB_LOG_ERR | SKHPB_LOG_INFO | SKHPB_LOG_DEBUG | SKHPB_LOG_HEX;
	}

	return count;
}

static ssize_t skhpb_sysfs_rsp_time_show(struct skhpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"map_req_time: %s\n", (debug_map_req ? "Enable":"Disable"));
}

static ssize_t skhpb_sysfs_rsp_time_store(struct skhpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value = 0;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 1)
		return count;

	debug_map_req = value;
	return count;
}

static struct skhpb_sysfs_entry skhpb_sysfs_entries[] = {
	__ATTR(hpb_disable, 0644,
			skhpb_sysfs_hpb_disable_show,
			skhpb_sysfs_hpb_disable_store),
	__ATTR(HPBVersion, 0444, skhpb_sysfs_version_show, NULL),
	__ATTR(statistics, 0444, skhpb_sysfs_statistics_show, NULL),
	__ATTR(active_count, 0444, skhpb_sysfs_active_count_show, NULL),
	__ATTR(add_evict_count, 0444, skhpb_sysfs_add_evict_show, NULL),
	__ATTR(count_reset, 0200, NULL, skhpb_sysfs_count_reset_store),
	__ATTR(map_req_count, 0444, skhpb_sysfs_map_req_show, NULL),
	__ATTR(get_info_from_region, 0200, NULL,
		   skhpb_sysfs_info_from_region_store),
	__ATTR(get_info_from_lba, 0200, NULL, skhpb_sysfs_info_from_lba_store),
	__ATTR(hpb_reset, 0200, NULL, skhpb_sysfs_hpb_reset_store),
	__ATTR(debug_log, 0644,
			skhpb_sysfs_debug_log_show,
			skhpb_sysfs_debug_log_store),
	__ATTR(response_time, 0644,
			skhpb_sysfs_rsp_time_show,
			skhpb_sysfs_rsp_time_store),
	__ATTR_NULL
};

static ssize_t skhpb_attr_show(struct kobject *kobj,
		struct attribute *attr, char *page)
{
	struct skhpb_sysfs_entry *entry;
	struct skhpb_lu *hpb;
	ssize_t error;

	entry = container_of(attr,
			struct skhpb_sysfs_entry, attr);
	hpb = container_of(kobj, struct skhpb_lu, kobj);

	if (!entry->show)
		return -EIO;

	mutex_lock(&hpb->sysfs_lock);
	error = entry->show(hpb, page);
	mutex_unlock(&hpb->sysfs_lock);
	return error;
}

static ssize_t skhpb_attr_store(struct kobject *kobj,
		struct attribute *attr,
		const char *page, size_t length)
{
	struct skhpb_sysfs_entry *entry;
	struct skhpb_lu *hpb;
	ssize_t error;

	entry = container_of(attr, struct skhpb_sysfs_entry, attr);
	hpb = container_of(kobj, struct skhpb_lu, kobj);

	if (!entry->store)
		return -EIO;

	mutex_lock(&hpb->sysfs_lock);
	error = entry->store(hpb, page, length);
	mutex_unlock(&hpb->sysfs_lock);
	return error;
}

static const struct sysfs_ops skhpb_sysfs_ops = {
	.show = skhpb_attr_show,
	.store = skhpb_attr_store,
};

static struct kobj_type skhpb_ktype = {
	.sysfs_ops = &skhpb_sysfs_ops,
	.release = NULL,
};

static int skhpb_create_sysfs(struct ufs_hba *hba,
		struct skhpb_lu *hpb)
{
	struct device *dev = hba->dev;
	struct skhpb_sysfs_entry *entry;
	int err;

	hpb->sysfs_entries = skhpb_sysfs_entries;

	skhpb_stat_init(hpb);

	kobject_init(&hpb->kobj, &skhpb_ktype);
	mutex_init(&hpb->sysfs_lock);

	err = kobject_add(&hpb->kobj, kobject_get(&dev->kobj),
			"ufshpb_lu%d", hpb->lun);
	if (!err) {
		for (entry = hpb->sysfs_entries;
				entry->attr.name != NULL ; entry++) {
			if (sysfs_create_file(&hpb->kobj, &entry->attr))
				break;
		}
		kobject_uevent(&hpb->kobj, KOBJ_ADD);
	}
	return err;
}
