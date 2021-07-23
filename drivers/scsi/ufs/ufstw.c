/*
 * Universal Flash Storage Turbo Write
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
#include "ufstw.h"
#include "ufs_quirks.h"

#define UFS_MTK_TW_AWAYS_ON

static int ufstw_create_sysfs(struct ufsf_feature *ufsf, struct ufstw_lu *tw);
static int ufstw_clear_lu_flag(struct ufstw_lu *tw, u8 idn, bool *flag_res);
static int ufstw_read_lu_attr(struct ufstw_lu *tw, u8 idn, u32 *attr_val);

static inline void ufstw_lu_get(struct ufstw_lu *tw)
{
	kref_get(&tw->ufsf->tw_kref);
}

static inline void ufstw_lu_put(struct ufstw_lu *tw)
{
	kref_put(&tw->ufsf->tw_kref, ufstw_release);
}

static inline bool ufstw_is_write_lrbp(struct ufshcd_lrb *lrbp)
{
	if (lrbp->cmd->cmnd[0] == WRITE_10 || lrbp->cmd->cmnd[0] == WRITE_16)
		return true;

	return false;
}

static int ufstw_switch_mode(struct ufstw_lu *tw, int tw_mode)
{
	int ret = 0;

	atomic_set(&tw->tw_mode, tw_mode);
	if (tw->tw_enable)
		ret = ufstw_clear_lu_flag(tw, QUERY_FLAG_IDN_TW_EN,
					  &tw->tw_enable);
	return ret;
}

static void ufstw_switch_disable_mode(struct ufstw_lu *tw)
{
	WARNING_MSG("dTurboWriteBUfferLifeTImeEst 0x%X", tw->tw_lifetime_est);
	WARNING_MSG("tw-mode will change to disable-mode..");

	mutex_lock(&tw->mode_lock);
	ufstw_switch_mode(tw, TW_MODE_DISABLED);
	mutex_unlock(&tw->mode_lock);
}

static void ufstw_lifetime_work_fn(struct work_struct *work)
{
	struct ufstw_lu *tw;

	tw = container_of(work, struct ufstw_lu, tw_lifetime_work);

	ufstw_lu_get(tw);

	if (atomic_read(&tw->ufsf->tw_state) != TW_PRESENT) {
		INFO_MSG("tw_state != TW_PRESENT (%d)",
			 atomic_read(&tw->ufsf->tw_state));
		goto out;
	}

	if (ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST,
			       &tw->tw_lifetime_est))
		goto out;

#if defined(CONFIG_UFSTW_IGNORE_GUARANTEE_BIT)
	if (tw->tw_lifetime_est & MASK_UFSTW_LIFETIME_NOT_GUARANTEE) {
		WARNING_MSG("warn: lun %d - dTurboWriteBufferLifeTimeEst[31] == 1", tw->lun);
		WARNING_MSG("Device not guarantee the lifetime of Turbo Write Buffer");
		WARNING_MSG("but we will ignore them for PoC");
	}
#else
	if (tw->tw_lifetime_est & MASK_UFSTW_LIFETIME_NOT_GUARANTEE) {
		WARNING_MSG("warn: lun %d - dTurboWriteBufferLifeTimeEst[31] == 1", tw->lun);
		WARNING_MSG("Device not guarantee the lifetime of Turbo Write Buffer");
		WARNING_MSG("So tw_mode change to disable_mode");
		goto tw_disable;
	}
#endif
	if ((tw->tw_lifetime_est & ~MASK_UFSTW_LIFETIME_NOT_GUARANTEE)
	    < UFSTW_MAX_LIFETIME_VALUE)
		goto out;
	else
		goto tw_disable;
tw_disable:
	ufstw_switch_disable_mode(tw);
out:
	ufstw_lu_put(tw);
}

void ufstw_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	struct ufstw_lu *tw;

	if (!lrbp || !ufsf_is_valid_lun(lrbp->lun))
		return;

	if (!ufstw_is_write_lrbp(lrbp))
		return;

	tw = ufsf->tw_lup[lrbp->lun];
	if (!tw)
		return;

	if (atomic_read(&tw->tw_mode) == TW_MODE_DISABLED)
		return;

	if (!tw->tw_enable)
		return;

	spin_lock_bh(&tw->lifetime_lock);
	tw->stat_write_sec += blk_rq_sectors(lrbp->cmd->request);

	if (tw->stat_write_sec > UFSTW_LIFETIME_SECT) {
		tw->stat_write_sec = 0;
		spin_unlock_bh(&tw->lifetime_lock);
		schedule_work(&tw->tw_lifetime_work);
		return;
	}

	blk_add_trace_msg(tw->ufsf->sdev_ufs_lu[tw->lun]->request_queue,
			  "%s:%d tw_lifetime_work %u",
			  __func__, __LINE__, tw->stat_write_sec);
	spin_unlock_bh(&tw->lifetime_lock);
}

static u8 ufstw_get_query_idx(struct ufstw_lu *tw)
{
	u8 idx;

	/* Share buffer type only use idx 0 */
	if (tw->ufsf->tw_dev_info.tw_buf_type == WB_SINGLE_SHARE_BUFFER_TYPE)
		idx = 0;
	else
		idx = (u8)tw->lun;

	return idx;
}

static int ufstw_read_lu_attr(struct ufstw_lu *tw, u8 idn, u32 *attr_val)
{
	struct ufs_hba *hba = tw->ufsf->hba;
	int err;
	u32 val;
	u8 idx;

	pm_runtime_get_sync(hba->dev);

	ufstw_lu_get(tw);

	idx = ufstw_get_query_idx(tw);
	err = ufsf_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, idn,
				    idx, &val);
	if (err) {
		ERR_MSG("read attr [0x%.2X] failed...err %d", idn, err);
		ufstw_lu_put(tw);
		pm_runtime_put_sync(hba->dev);
		return err;
	}

	*attr_val = val;

	blk_add_trace_msg(tw->ufsf->sdev_ufs_lu[tw->lun]->request_queue,
			  "%s:%d IDN %s (%d)", __func__, __LINE__,
			  idn == QUERY_ATTR_IDN_TW_FLUSH_STATUS ? "TW_FLUSH_STATUS" :
			  idn == QUERY_ATTR_IDN_TW_BUF_SIZE ? "TW_BUF_SIZE" :
			  idn == QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST ? "TW_BUF_LIFETIME_EST" :
			  "UNKNOWN", idn);

	TW_DEBUG(tw->ufsf, "tw_attr LUN(%d) [0x%.2X] %u", tw->lun, idn,
		 *attr_val);

	ufstw_lu_put(tw);
	pm_runtime_put_sync(hba->dev);

	return 0;
}

static int ufstw_set_lu_flag(struct ufstw_lu *tw, u8 idn, bool *flag_res)
{
	struct ufs_hba *hba = tw->ufsf->hba;
	int err;
	u8 idx;

	pm_runtime_get_sync(hba->dev);
	ufstw_lu_get(tw);

	idx = ufstw_get_query_idx(tw);
	err = ufsf_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG, idn,
				    idx, NULL);
	if (err) {
		ERR_MSG("set flag [0x%.2X] failed...err %d", idn, err);
		ufstw_lu_put(tw);
		pm_runtime_put_sync(hba->dev);
		return err;
	}

	*flag_res = true;
	blk_add_trace_msg(tw->ufsf->sdev_ufs_lu[tw->lun]->request_queue,
			  "%s:%d IDN %s (%d)", __func__, __LINE__,
			  idn == QUERY_FLAG_IDN_TW_EN ? "TW_EN" :
			  idn == QUERY_FLAG_IDN_TW_BUF_FLUSH_EN ? "FLUSH_EN" :
			  idn == QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN ?
			  "HIBERN_EN" : "UNKNOWN", idn);

	TW_DEBUG(tw->ufsf, "tw_flag LUN(%d) [0x%.2X] %u", tw->lun, idn,
		 *flag_res);

	ufstw_lu_put(tw);
	pm_runtime_put_sync(hba->dev);

	return 0;
}

static int ufstw_clear_lu_flag(struct ufstw_lu *tw, u8 idn, bool *flag_res)
{
	struct ufs_hba *hba = tw->ufsf->hba;
	int err;
	u8 idx;

	pm_runtime_get_sync(hba->dev);
	ufstw_lu_get(tw);

	idx = ufstw_get_query_idx(tw);
	err = ufsf_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG, idn,
				    idx, NULL);
	if (err) {
		ERR_MSG("clear flag [0x%.2X] failed...err%d", idn, err);
		ufstw_lu_put(tw);
		pm_runtime_put_sync(hba->dev);
		return err;
	}

	*flag_res = false;

	blk_add_trace_msg(tw->ufsf->sdev_ufs_lu[tw->lun]->request_queue,
			  "%s:%d IDN %s (%d)", __func__, __LINE__,
			  idn == QUERY_FLAG_IDN_TW_EN ? "TW_EN" :
			  idn == QUERY_FLAG_IDN_TW_BUF_FLUSH_EN ? "FLUSH_EN" :
			  idn == QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN ? "HIBERN_EN" :
			  "UNKNOWN", idn);

	TW_DEBUG(tw->ufsf, "tw_flag LUN(%d) [0x%.2X] %u", tw->lun, idn,
		 *flag_res);

	ufstw_lu_put(tw);
	pm_runtime_put_sync(hba->dev);
	return 0;
}

static inline int ufstw_read_lu_flag(struct ufstw_lu *tw, u8 idn,
				     bool *flag_res)
{
	struct ufs_hba *hba = tw->ufsf->hba;
	int err;
	bool val = 0;
	u8 idx;

	pm_runtime_get_sync(hba->dev);
	ufstw_lu_get(tw);

	idx = ufstw_get_query_idx(tw);
	err = ufsf_query_flag_retry(hba, UPIU_QUERY_OPCODE_READ_FLAG, idn,
				    idx, &val);
	if (err) {
		ERR_MSG("read flag [0x%.2X] failed...err%d", idn, err);
		ufstw_lu_put(tw);
		pm_runtime_put_sync(hba->dev);
		return err;
	}

	*flag_res = val;

	TW_DEBUG(tw->ufsf, "tw_flag LUN(%d) [0x%.2X] %u", tw->lun, idn,
		 *flag_res);

	ufstw_lu_put(tw);
	pm_runtime_put_sync(hba->dev);
	return 0;

}

/* device level (ufsf) */
static int ufstw_auto_ee(struct ufsf_feature *ufsf)
{
	struct ufs_hba *hba = ufsf->hba;
	u16 mask = MASK_EE_TW;
	u32 val;
	int err = 0;

	pm_runtime_get_sync(hba->dev);

	if (hba->ee_ctrl_mask & mask)
		goto out;

	val = hba->ee_ctrl_mask | mask;
	val &= 0xFFFF; /* 2 bytes */
	err = ufsf_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
				    QUERY_ATTR_IDN_EE_CONTROL, 0, &val);
	if (err) {
		ERR_MSG("failed to enable exception event err%d", err);
		goto out;
	}

	hba->ee_ctrl_mask |= mask;
	ufsf->tw_ee_mode = true;

	TW_DEBUG(ufsf, "turbo_write_exception_event_enable");
out:
	pm_runtime_put_sync(hba->dev);
	return err;
}

/* device level (ufsf) */
static int ufstw_disable_ee(struct ufsf_feature *ufsf)
{
	struct ufs_hba *hba = ufsf->hba;
	u16 mask = MASK_EE_TW;
	int err = 0;
	u32 val;

	pm_runtime_get_sync(hba->dev);

	if (!(hba->ee_ctrl_mask & mask))
		goto out;

	val = hba->ee_ctrl_mask & ~mask;
	val &= 0xFFFF;	/* 2 bytes */
	err = ufsf_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
				    QUERY_ATTR_IDN_EE_CONTROL, 0, &val);
	if (err) {
		ERR_MSG("failed to disable exception event err%d", err);
		goto out;
	}

	hba->ee_ctrl_mask &= ~mask;
	ufsf->tw_ee_mode = false;

	TW_DEBUG(ufsf, "turbo_write_exeception_event_disable");
out:
	pm_runtime_put_sync(hba->dev);
	return err;
}

static void ufstw_flush_work_fn(struct work_struct *dwork)
{
	struct ufs_hba *hba;
	struct ufstw_lu *tw;
	bool need_resched = false;

	tw = container_of(dwork, struct ufstw_lu, tw_flush_work.work);

	TW_DEBUG(tw->ufsf, "start flush worker");

	ufstw_lu_get(tw);
	if (atomic_read(&tw->ufsf->tw_state) != TW_PRESENT) {
		ERR_MSG("tw_state != TW_PRESENT (%d)",
			atomic_read(&tw->ufsf->tw_state));
		ufstw_lu_put(tw);
		return;
	}

	hba = tw->ufsf->hba;
	if (tw->next_q && time_before(jiffies, tw->next_q)) {
		if (schedule_delayed_work(&tw->tw_flush_work,
					  tw->next_q - jiffies))
			pm_runtime_get_noresume(hba->dev);
		ufstw_lu_put(tw);
		return;
	}

	pm_runtime_get_sync(hba->dev);
	if (ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_BUF_SIZE,
			       &tw->tw_available_buffer_size))
		goto error_put;

	mutex_lock(&tw->flush_lock);

	if (tw->tw_flush_during_hibern_enter &&
	    tw->tw_available_buffer_size >= tw->flush_th_max) {
		TW_DEBUG(tw->ufsf, "flush_disable QR (%d, %d)",
			 tw->lun, tw->tw_available_buffer_size);

		if (ufstw_clear_lu_flag(tw,
					QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
					&tw->tw_flush_during_hibern_enter))
			goto error_unlock;
		tw->next_q = 0;
		need_resched = false;
	} else if (tw->tw_available_buffer_size < tw->flush_th_max) {
		if (tw->tw_flush_during_hibern_enter) {
			need_resched = true;
		} else if (tw->tw_available_buffer_size < tw->flush_th_min) {
			TW_DEBUG(tw->ufsf, "flush_enable  QR (%d, %d)",
				 tw->lun, tw->tw_available_buffer_size);
			if (ufstw_set_lu_flag(tw,
					      QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
					      &tw->tw_flush_during_hibern_enter))
				goto error_unlock;
			need_resched = true;
		} else {
			need_resched = false;
		}
	}
	mutex_unlock(&tw->flush_lock);

	pm_runtime_put_noidle(hba->dev);
	pm_runtime_put(hba->dev);

	if (need_resched) {
		tw->next_q =
			jiffies + msecs_to_jiffies(UFSTW_FLUSH_CHECK_PERIOD_MS);
		if (schedule_delayed_work(&tw->tw_flush_work,
					  msecs_to_jiffies(UFSTW_FLUSH_CHECK_PERIOD_MS)))
			pm_runtime_get_noresume(hba->dev);
	}
	ufstw_lu_put(tw);
	return;
error_unlock:
	mutex_unlock(&tw->flush_lock);
error_put:
	pm_runtime_put_noidle(hba->dev);
	pm_runtime_put(hba->dev);

	if (tw->next_q) {
		tw->next_q =
			jiffies + msecs_to_jiffies(UFSTW_FLUSH_CHECK_PERIOD_MS);
		if (schedule_delayed_work(&tw->tw_flush_work,
					  msecs_to_jiffies(UFSTW_FLUSH_CHECK_PERIOD_MS)))
			pm_runtime_get_noresume(hba->dev);
	}
	ufstw_lu_put(tw);
}

void ufstw_error_handler(struct ufsf_feature *ufsf)
{
	ERR_MSG("tw_state : %d -> %d", atomic_read(&ufsf->tw_state), TW_FAILED);
	atomic_set(&ufsf->tw_state, TW_FAILED);
	dump_stack();
	kref_put(&ufsf->tw_kref, ufstw_release);
}

void ufstw_ee_handler(struct ufsf_feature *ufsf)
{
	struct ufs_hba *hba;
	int lun;

	hba = ufsf->hba;

	if (ufsf->tw_debug)
		atomic64_inc(&ufsf->tw_debug_ee_count);

	seq_scan_lu(lun) {
		if (!ufsf->tw_lup[lun])
			continue;

		if (!ufsf->sdev_ufs_lu[lun]) {
			WARNING_MSG("warn: lun %d don't have scsi_device", lun);
			continue;
		}

		ufstw_lu_get(ufsf->tw_lup[lun]);
		if (!delayed_work_pending(&ufsf->tw_lup[lun]->tw_flush_work)) {
			ufsf->tw_lup[lun]->next_q = jiffies;
			if (schedule_delayed_work(&ufsf->tw_lup[lun]->tw_flush_work,
						  msecs_to_jiffies(0)))
				pm_runtime_get_noresume(hba->dev);
		}
		ufstw_lu_put(ufsf->tw_lup[lun]);
	}
}

static void ufstw_flush_h8_work_fn(struct work_struct *dwork)
{
	struct ufs_hba *hba;
	struct ufstw_lu *tw;

	tw = container_of(dwork, struct ufstw_lu, tw_flush_h8_work.work);
	hba = tw->ufsf->hba;

	/* Exit runtime suspend and do flush in hibern 1 sec */
	pm_runtime_get_sync(hba->dev);
	msleep(1000);

	/* Check again, if still need flush reschedule itself */
	if (ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_BUF_SIZE,
			       &tw->tw_available_buffer_size)) {
		ERR_MSG("ERROR: get available tw buffer size error");
		goto out;
	}
	if (tw->tw_available_buffer_size < tw->flush_th_max)
		schedule_delayed_work(&tw->tw_flush_h8_work, 0);

out:
	pm_runtime_put_sync(hba->dev);
}

bool ufstw_need_flush(struct ufsf_feature *ufsf)
{
	struct ufs_hba *hba = ufsf->hba;
	struct ufstw_lu *tw;
	bool need_flush = false;
	u8 idx;

	if (!ufsf->tw_lup[2])
		goto out;

	if (!ufsf->sdev_ufs_lu[2]) {
		WARNING_MSG("warn: lun 2 don't have scsi_device");
		goto out;
	}

	tw = ufsf->tw_lup[2];

	if (atomic_read(&tw->tw_mode) == TW_MODE_DISABLED)
		goto out;

	if (!tw->tw_enable)
		goto out;

	if (atomic_read(&tw->ufsf->tw_state) != TW_PRESENT)
		goto out;

	ufstw_lu_get(tw);

	/*
	 * No need check again, let ufstw_flush_h8_work_fn finish is enough.
	 * Only return need_flush to break runtime/system suspend.
	 */
	if (work_busy(&tw->tw_flush_h8_work.work)) {
		need_flush = true;
		goto out_put;
	}

	idx = ufstw_get_query_idx(tw);
	if (ufsf_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				  QUERY_ATTR_IDN_TW_BUF_SIZE,
				  idx,
				  &tw->tw_available_buffer_size)) {
		ERR_MSG("ERROR: get available tw buffer size error");
		goto out_put;
	}

	/* No need flush */
	if (tw->tw_available_buffer_size >= tw->flush_th_max)
		goto out_put;

	/* Need flush, check device flush method */
	if (hba->dev_quirks & UFS_DEVICE_QUIRK_WRITE_BOOSETER_FLUSH) {
		/* Toshiba device recover WB by toggle fWriteBoosterEn */
		if (ufsf_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG,
					  QUERY_FLAG_IDN_TW_EN, idx, NULL)) {
			ERR_MSG("ERROR: disable tw error");
			goto out_put;
		}

		if (ufsf_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
					  QUERY_FLAG_IDN_TW_EN, idx, NULL)) {
			ERR_MSG("ERROR: enable tw error");
			goto out_put;
		}
	} else {
		/* Other device recover WB by hibernate */
		schedule_delayed_work(&tw->tw_flush_h8_work, 0);
		need_flush = true;
	}

out_put:
	if (need_flush) {
		INFO_MSG("UFS TW available buffer size(%d) < %d0%%",
			tw->tw_available_buffer_size, tw->flush_th_max);
	}

	ufstw_lu_put(tw);
out:
	return need_flush;
}

static inline void ufstw_init_dev_jobs(struct ufsf_feature *ufsf)
{
	INIT_INFO("INIT_WORK(tw_reset_work)");
	INIT_WORK(&ufsf->tw_reset_work, ufstw_reset_work_fn);
}

static inline void ufstw_init_lu_jobs(struct ufstw_lu *tw)
{
	INIT_INFO("INIT_DELAYED_WORK(tw_flush_work) ufstw_lu%d", tw->lun);
	INIT_DELAYED_WORK(&tw->tw_flush_work, ufstw_flush_work_fn);
	INIT_DELAYED_WORK(&tw->tw_flush_h8_work, ufstw_flush_h8_work_fn);
	INIT_INFO("INIT_WORK(tw_lifetime_work)");
	INIT_WORK(&tw->tw_lifetime_work, ufstw_lifetime_work_fn);
}

static inline void ufstw_cancel_lu_jobs(struct ufstw_lu *tw)
{
	int ret;

	ret = cancel_delayed_work_sync(&tw->tw_flush_work);
	ret = cancel_work_sync(&tw->tw_lifetime_work);
}

void ufstw_get_dev_info(struct ufstw_dev_info *tw_dev_info, u8 *desc_buf)
{
	struct ufsf_feature *ufsf;
	struct ufs_hba *hba;

	ufsf = container_of(tw_dev_info, struct ufsf_feature, tw_dev_info);
	hba = ufsf->hba;

	tw_dev_info->tw_device = false;

	if (UFSF_EFS_TURBO_WRITE
	    & LI_EN_32(&desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP]))
		INIT_INFO("bUFSExFeaturesSupport: TW support");
	else {
		INIT_INFO("bUFSExFeaturesSupport: TW not support");
		return;
	}
	tw_dev_info->tw_buf_no_reduct =
		desc_buf[DEVICE_DESC_PARAM_TW_RETURN_TO_USER];
	tw_dev_info->tw_buf_type = desc_buf[DEVICE_DESC_PARAM_TW_BUF_TYPE];

	/* Set TW device if TW support */
	tw_dev_info->tw_device = true;

	INFO_MSG("tw_dev [53] bTurboWriteBufferNoUserSpaceReductionEn %u",
		 tw_dev_info->tw_buf_no_reduct);
	INFO_MSG("tw_dev [54] bTurboWriteBufferType %u",
		 tw_dev_info->tw_buf_type);
}

void ufstw_get_geo_info(struct ufstw_dev_info *tw_dev_info, u8 *geo_buf)
{
	tw_dev_info->tw_number_lu = geo_buf[GEOMETRY_DESC_TW_NUMBER_LU];
	if (tw_dev_info->tw_number_lu == 0) {
		ERR_MSG("Turbo Write is not supported");
		tw_dev_info->tw_device = false;
		return;
	}

	INFO_MSG("tw_geo [4F:52] dTurboWriteBufferMaxNAllocUnits %u",
		 LI_EN_32(&geo_buf[GEOMETRY_DESC_TW_MAX_SIZE]));
	INFO_MSG("tw_geo [53] bDeviceMaxTurboWriteLUs %u",
		 tw_dev_info->tw_number_lu);
	INFO_MSG("tw_geo [54] bTurboWriteBufferCapAdjFac %u",
		 geo_buf[GEOMETRY_DESC_TW_CAP_ADJ_FAC]);
	INFO_MSG("tw_geo [55] bSupportedTurboWriteBufferUserSpaceReductionTypes %u",
		 geo_buf[GEOMETRY_DESC_TW_SUPPORT_USER_REDUCTION_TYPES]);
	INFO_MSG("tw_geo [56] bSupportedTurboWriteBufferTypes %u",
		 geo_buf[GEOMETRY_DESC_TW_SUPPORT_BUF_TYPE]);
}

int ufstw_get_lu_info(struct ufsf_feature *ufsf, unsigned int lun, u8 *lu_buf)
{
	struct ufsf_lu_desc lu_desc;
	struct ufstw_lu *tw;

	lu_desc.tw_lu_buf_size =
		LI_EN_32(&lu_buf[UNIT_DESC_TW_LU_MAX_BUF_SIZE]);

	ufsf->tw_lup[lun] = NULL;

	/* MTK: tw_buf_type = 0(LU base), 1(Single share) */
	if ((lu_desc.tw_lu_buf_size) ||
		((ufsf->tw_dev_info.tw_buf_type == WB_SINGLE_SHARE_BUFFER_TYPE)
		&& (lun == 2))) {
		ufsf->tw_lup[lun] =
			kzalloc(sizeof(struct ufstw_lu), GFP_KERNEL);
		if (!ufsf->tw_lup[lun])
			return -ENOMEM;

		tw = ufsf->tw_lup[lun];
		tw->ufsf = ufsf;
		tw->lun = lun;
		INIT_INFO("tw_lu LUN(%d) [29:2C] dLUNumTurboWriteBufferAllocUnits %u",
			  lun, lu_desc.tw_lu_buf_size);
	} else {
		INIT_INFO("tw_lu LUN(%d) [29:2C] dLUNumTurboWriteBufferAllocUnits %u",
			  lun, lu_desc.tw_lu_buf_size);
		INIT_INFO("===== LUN(%d) is TurboWrite-disabled.", lun);
		return -ENODEV;
	}

	return 0;
}

static inline void ufstw_print_lu_flag_attr(struct ufstw_lu *tw)
{
	INFO_MSG("tw_flag LUN(%d) [%u] fTurboWriteEn %u", tw->lun,
		 QUERY_FLAG_IDN_TW_EN, tw->tw_enable);
	INFO_MSG("tw_flag LUN(%d) [%u] fTurboWriteBufferFlushEn %u", tw->lun,
		 QUERY_FLAG_IDN_TW_BUF_FLUSH_EN, tw->tw_flush_enable);
	INFO_MSG("tw_flag LUN(%d) [%u] fTurboWriteBufferFlushDuringHibernateEnter %u",
		 tw->lun, QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
		 tw->tw_flush_during_hibern_enter);

	INFO_MSG("tw_attr LUN(%d) [%u] flush_status  %u", tw->lun,
		 QUERY_ATTR_IDN_TW_FLUSH_STATUS, tw->tw_flush_status);
	INFO_MSG("tw_attr LUN(%d) [%u] buffer_size  %u", tw->lun,
		 QUERY_ATTR_IDN_TW_BUF_SIZE, tw->tw_available_buffer_size);
	INFO_MSG("tw_attr LUN(%d) [%d] bufffer_lifetime  %u(0x%X)",
		 tw->lun, QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST,
		 tw->tw_lifetime_est, tw->tw_lifetime_est);
}

static inline void ufstw_lu_update(struct ufstw_lu *tw)
{
	ufstw_lu_get(tw);

	/* Flag */
	if (ufstw_read_lu_flag(tw, QUERY_FLAG_IDN_TW_EN, &tw->tw_enable))
		goto error_put;

	if (ufstw_read_lu_flag(tw, QUERY_FLAG_IDN_TW_BUF_FLUSH_EN,
			       &tw->tw_flush_enable))
		goto error_put;

	if (ufstw_read_lu_flag(tw, QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
			       &tw->tw_flush_during_hibern_enter))
		goto error_put;

	/* Attribute */
	if (ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_FLUSH_STATUS,
			       &tw->tw_flush_status))
		goto error_put;

	if (ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_BUF_SIZE,
			       &tw->tw_available_buffer_size))
		goto error_put;

	ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST,
			       &tw->tw_lifetime_est);
error_put:
	ufstw_lu_put(tw);
}

static void ufstw_lu_init(struct ufsf_feature *ufsf, unsigned int lun)
{
	struct ufstw_lu *tw = ufsf->tw_lup[lun];

	ufstw_lu_get(tw);
	tw->ufsf = ufsf;

	mutex_init(&tw->flush_lock);
	mutex_init(&tw->mode_lock);
	spin_lock_init(&tw->lifetime_lock);

	tw->stat_write_sec = 0;
	atomic_set(&tw->active_cnt, 0);

	tw->flush_th_min = UFSTW_FLUSH_WORKER_TH_MIN;
	tw->flush_th_max = UFSTW_FLUSH_WORKER_TH_MAX;

	/* for Debug */
	ufstw_init_lu_jobs(tw);

	if (ufstw_create_sysfs(ufsf, tw))
		INIT_INFO("sysfs init fail. but tw could run normally.");

	/* Read Flag, Attribute */
	ufstw_lu_update(tw);

#if defined(CONFIG_UFSTW_IGNORE_GUARANTEE_BIT)
	if (tw->tw_lifetime_est & MASK_UFSTW_LIFETIME_NOT_GUARANTEE) {
		WARNING_MSG("warn: lun %d - dTurboWriteBufferLifeTimeEst[31] == 1", lun);
		WARNING_MSG("Device not guarantee the lifetime of Turbo Write Buffer");
		WARNING_MSG("but we will ignore them for PoC");
	}
#else
	if (tw->tw_lifetime_est & MASK_UFSTW_LIFETIME_NOT_GUARANTEE) {
		WARNING_MSG("warn: lun %d - dTurboWriteBufferLifeTimeEst[31] == 1", lun);
		WARNING_MSG("Device not guarantee the lifetime of Turbo Write Buffer");
		WARNING_MSG("So tw_mode change to disable_mode");
		goto tw_disable;
	}
#endif
	if ((tw->tw_lifetime_est & ~MASK_UFSTW_LIFETIME_NOT_GUARANTEE)
	    < UFSTW_MAX_LIFETIME_VALUE) {
		atomic_set(&tw->tw_mode, TW_MODE_MANUAL);
		goto out;
	} else
		goto tw_disable;

tw_disable:
	ufstw_switch_disable_mode(tw);
out:
	ufstw_print_lu_flag_attr(tw);
	ufstw_lu_put(tw);
}

void ufstw_init(struct ufsf_feature *ufsf)
{
	unsigned int lun;
	unsigned int tw_enabled_lun = 0;
#ifdef UFS_MTK_TW_AWAYS_ON
	int tw_lun = 0;
	struct ufstw_lu *tw;
#endif

	kref_init(&ufsf->tw_kref);

#ifdef UFS_MTK_TW_AWAYS_ON
	/* MTK: TW only enable in LU2, skip scan */
	lun = 2;
	if (ufsf->tw_lup[lun]) {
		ufstw_lu_init(ufsf, lun);
		tw_lun = lun;
		INIT_INFO("UFSTW LU %d working", lun);
		tw_enabled_lun++;
	}
#else
	seq_scan_lu(lun) {
		if (!ufsf->tw_lup[lun])
			continue;

		if (!ufsf->sdev_ufs_lu[lun]) {
			WARNING_MSG("warn: lun %d don't have scsi_device", lun);
			continue;
		}

		ufstw_lu_init(ufsf, lun);

		ufsf->sdev_ufs_lu[lun]->request_queue->turbo_write_dev = true;

		INIT_INFO("UFSTW LU %d working", lun);
		tw_enabled_lun++;
	}
#endif

	if (tw_enabled_lun == 0) {
		ERR_MSG("ERROR: tw_enabled_lun == 0. So TW disabled.");
		goto out_free_mem;
	}

	if (tw_enabled_lun > ufsf->tw_dev_info.tw_number_lu) {
		ERR_MSG("ERROR: dev_info(bDeviceMaxTurboWriteLUs) mismatch. So TW disabled.");
		goto out;
	}

#ifdef UFS_MTK_TW_AWAYS_ON
	/* MTK: Disable TW if run out lifetime */
	tw = ufsf->tw_lup[tw_lun];
	if (atomic_read(&tw->tw_mode) == TW_MODE_DISABLED)
		goto out;
#endif
	/*
	 * Initialize Device Level...
	 */
	ufstw_disable_ee(ufsf);
	ufstw_init_dev_jobs(ufsf);
	atomic64_set(&ufsf->tw_debug_ee_count, 0);
	ufsf->tw_debug = false;
	atomic_set(&ufsf->tw_state, TW_PRESENT);

#ifdef UFS_MTK_TW_AWAYS_ON
	/* MTK: Enable TW and H8 flush in Manual mode */
	if ((atomic_read(&tw->tw_mode) == TW_MODE_MANUAL) && (tw_lun != 0)) {
		if (ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_EN, &tw->tw_enable))
			goto out;
		if (ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
			&tw->tw_flush_during_hibern_enter))
			goto out;
	}
#endif
	return;
out_free_mem:
	seq_scan_lu(lun) {
		kfree(ufsf->tw_lup[lun]);
		ufsf->tw_lup[lun] = NULL;
	}
out:
	/* MTK: not free because we still need querry */
	ufsf->tw_dev_info.tw_device = false;
	atomic_set(&ufsf->tw_state, TW_NOT_SUPPORTED);
}

static inline int ufstw_probe_lun_done(struct ufsf_feature *ufsf)
{
	return (ufsf->num_lu == ufsf->slave_conf_cnt);
}

void ufstw_init_work_fn(struct work_struct *work)
{
	struct ufsf_feature *ufsf;
#ifndef UFS_MTK_TW_AWAYS_ON
	int ret;
#endif

	ufsf = container_of(work, struct ufsf_feature, tw_init_work);

#ifndef UFS_MTK_TW_AWAYS_ON
	/* MTK: TW only enable in LU2, skip wait */
	init_waitqueue_head(&ufsf->tw_wait);

	ret = wait_event_timeout(ufsf->tw_wait,
				 ufstw_probe_lun_done(ufsf),
				 msecs_to_jiffies(10000));
	if (ret == 0) {
		ERR_MSG("Probing LU is not fully completed.");
		return;
	}
#endif

	INIT_INFO("TW_INIT_START");

	ufstw_init(ufsf);
}

void ufstw_suspend(struct ufsf_feature *ufsf)
{
	struct ufstw_lu *tw;
	int lun;
#if 0
	int ret;

/*
 * MTK: No ned flush work, else deadlock may happen.
 * flush_work ->ufstw_reset_work_fn -> ufstw_reset -> ufstw_set_lu_flag ->
 * pm_runtime_put_sync -> ufstw_suspend -> flush_work
 * Beside, reset work only set tw flag, it can do later after suspend.
 */
	ret = flush_work(&ufsf->tw_reset_work);
	TW_DEBUG(ufsf, "flush_work(tw_reset_work) = %d", ret);
#endif

	seq_scan_lu(lun) {
		tw = ufsf->tw_lup[lun];
		if (!tw)
			continue;

		ufstw_lu_get(tw);
		ufstw_cancel_lu_jobs(tw);
		ufstw_lu_put(tw);
	}
}

void ufstw_resume(struct ufsf_feature *ufsf)
{
	struct ufstw_lu *tw;
	int lun;

	seq_scan_lu(lun) {
		tw = ufsf->tw_lup[lun];
		if (!tw)
			continue;

		ufstw_lu_get(tw);
		TW_DEBUG(ufsf, "ufstw_lu %d resume", lun);
		if (tw->next_q) {
			TW_DEBUG(ufsf,
				 "ufstw_lu %d flush_worker reschedule...", lun);
			if (schedule_delayed_work(&tw->tw_flush_work,
						  (tw->next_q - jiffies)))
				pm_runtime_get_noresume(ufsf->hba->dev);
		}
		ufstw_lu_put(tw);
	}
}

void ufstw_release(struct kref *kref)
{
	struct ufsf_feature *ufsf;
	struct ufstw_lu *tw;
	int lun;
	int ret;

	dump_stack();
	ufsf = container_of(kref, struct ufsf_feature, tw_kref);
	RELEASE_INFO("start release");

	RELEASE_INFO("tw_state : %d -> %d", atomic_read(&ufsf->tw_state),
		     TW_FAILED);
	atomic_set(&ufsf->tw_state, TW_FAILED);

	RELEASE_INFO("kref count %d",
		     atomic_read(&ufsf->tw_kref.refcount.refs));

	ret = cancel_work_sync(&ufsf->tw_reset_work);
	RELEASE_INFO("cancel_work_sync(tw_reset_work) = %d", ret);

	seq_scan_lu(lun) {
		tw = ufsf->tw_lup[lun];

		RELEASE_INFO("ufstw_lu%d %p", lun, tw);

		ufsf->tw_lup[lun] = NULL;

		if (!tw)
			continue;

		ufstw_cancel_lu_jobs(tw);
		tw->next_q = 0;

		ret = kobject_uevent(&tw->kobj, KOBJ_REMOVE);
		RELEASE_INFO("kobject error %d", ret);

		kobject_del(&tw->kobj);

		kfree(tw);
	}

	RELEASE_INFO("end release");
}

static void ufstw_reset(struct ufsf_feature *ufsf)
{
	struct ufstw_lu *tw;
	int lun;
	int ret;

	if (atomic_read(&ufsf->tw_state) == TW_FAILED) {
		ERR_MSG("tw_state == TW_FAILED(%d)",
			atomic_read(&ufsf->tw_state));
		return;
	}

	seq_scan_lu(lun) {
		tw = ufsf->tw_lup[lun];
		TW_DEBUG(ufsf, "reset tw[%d]=%p", lun, tw);
		if (!tw)
			continue;

		INFO_MSG("ufstw_lu%d reset", lun);

		ufstw_lu_get(tw);
		ufstw_cancel_lu_jobs(tw);

		if (atomic_read(&tw->tw_mode) == TW_MODE_MANUAL &&
		    tw->tw_enable) {
			ret = ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_EN,
						&tw->tw_enable);
			if (ret)
				tw->tw_enable = false;
		}

		if (tw->tw_flush_enable) {
			ret = ufstw_set_lu_flag(tw,
						QUERY_FLAG_IDN_TW_BUF_FLUSH_EN,
						&tw->tw_flush_enable);
			if (ret)
				tw->tw_flush_enable = false;
		}

		if (tw->tw_flush_during_hibern_enter) {
			ret = ufstw_set_lu_flag(tw,
						QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
						&tw->tw_flush_during_hibern_enter);
			if (ret)
				tw->tw_flush_during_hibern_enter = false;
		}

		if (tw->next_q) {
			TW_DEBUG(ufsf,
				 "ufstw_lu %d flush_worker reschedule...", lun);
			if (schedule_delayed_work(&tw->tw_flush_work,
						  (tw->next_q - jiffies)))
				pm_runtime_get_noresume(ufsf->hba->dev);
		}
		ufstw_lu_put(tw);
	}

	if (ufsf->tw_ee_mode)
		ufstw_auto_ee(ufsf);

	atomic_set(&ufsf->tw_state, TW_PRESENT);
	INFO_MSG("reset complete.. tw_state %d", atomic_read(&ufsf->tw_state));
}

static inline int ufstw_wait_kref_init_value(struct ufsf_feature *ufsf)
{
	return (atomic_read(&ufsf->tw_kref.refcount.refs) == 1);
}

void ufstw_reset_work_fn(struct work_struct *work)
{
	struct ufsf_feature *ufsf;
	int ret;

	ufsf = container_of(work, struct ufsf_feature, tw_reset_work);
	TW_DEBUG(ufsf, "reset tw_kref.refcount=%d",
		 atomic_read(&ufsf->tw_kref.refcount.refs));

	init_waitqueue_head(&ufsf->tw_wait);

	ret = wait_event_timeout(ufsf->tw_wait,
				 ufstw_wait_kref_init_value(ufsf),
				 msecs_to_jiffies(15000));
	if (ret == 0) {
		ERR_MSG("UFSTW kref is not init_value(=1). kref count = %d ret = %d. So, TW_RESET_FAIL",
			atomic_read(&ufsf->tw_kref.refcount.refs), ret);
		return;
	}

	INIT_INFO("TW_RESET_START");

	ufstw_reset(ufsf);
}

/* protected by mutex mode_lock  */
static void __active_turbo_write(struct ufstw_lu *tw, int do_work)
{
	if (atomic_read(&tw->tw_mode) != TW_MODE_FS)
		return;

	blk_add_trace_msg(tw->ufsf->sdev_ufs_lu[tw->lun]->request_queue,
			  "%s:%d do_work %d active_cnt %d",
			  __func__, __LINE__, do_work,
			  atomic_read(&tw->active_cnt));

	if (do_work == TW_FLAG_ENABLE_SET && !tw->tw_enable)
		ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_EN, &tw->tw_enable);
	else if (do_work == TW_FLAG_ENABLE_CLEAR && tw->tw_enable)
		ufstw_clear_lu_flag(tw, QUERY_FLAG_IDN_TW_EN, &tw->tw_enable);
}

static void ufstw_active_turbo_write(struct request_queue *q, bool on)
{
	struct scsi_device *sdev = q->queuedata;
	struct Scsi_Host *shost;
	struct ufs_hba *hba;
	struct ufstw_lu *tw;
	int do_work = TW_FLAG_ENABLE_NONE;
	u64 lun;

	lun = sdev->lun;
	if (lun >= UFS_UPIU_MAX_GENERAL_LUN)
		return;

	shost = sdev->host;
	hba = shost_priv(shost);
	tw = hba->ufsf.tw_lup[lun];
	if (!tw)
		return;

	ufstw_lu_get(tw);
	if (on) {
		if (atomic_inc_return(&tw->active_cnt) == 1)
			do_work = TW_FLAG_ENABLE_SET;
	} else {
		if (atomic_dec_return(&tw->active_cnt) == 0)
			do_work = TW_FLAG_ENABLE_CLEAR;
	}

	blk_add_trace_msg(q, "%s:%d on %d active cnt %d do_work %d state %d mode %d",
			  __func__, __LINE__, on, atomic_read(&tw->active_cnt),
			  do_work, atomic_read(&tw->ufsf->tw_state),
			  atomic_read(&tw->tw_mode));

	if (!do_work)
		goto out;

	if (atomic_read(&tw->ufsf->tw_state) != TW_PRESENT) {
		WARNING_MSG("tw_state %d.. cannot enable turbo_write..",
			    atomic_read(&tw->ufsf->tw_state));
		goto out;
	}

	if (atomic_read(&tw->tw_mode) != TW_MODE_FS)
		goto out;

	mutex_lock(&tw->mode_lock);
	__active_turbo_write(tw, do_work);
	mutex_unlock(&tw->mode_lock);
out:
	ufstw_lu_put(tw);
}

void bdev_set_turbo_write(struct block_device *bdev)
{
	struct request_queue *q = bdev->bd_queue;

	blk_add_trace_msg(q, "%s:%d turbo_write_dev %d\n",
			  __func__, __LINE__, q->turbo_write_dev);

	if (q->turbo_write_dev)
		ufstw_active_turbo_write(bdev->bd_queue, true);
}

void bdev_clear_turbo_write(struct block_device *bdev)
{
	struct request_queue *q = bdev->bd_queue;

	blk_add_trace_msg(q, "%s:%d turbo_write_dev %d\n",
			  __func__, __LINE__, q->turbo_write_dev);

	if (q->turbo_write_dev)
		ufstw_active_turbo_write(bdev->bd_queue, false);
}

/* sysfs function */
static ssize_t ufstw_sysfs_show_ee_mode(struct ufstw_lu *tw, char *buf)
{
	SYSFS_INFO("TW_ee_mode %d", tw->ufsf->tw_ee_mode);

	return snprintf(buf, PAGE_SIZE, "%d", tw->ufsf->tw_ee_mode);
}

static ssize_t ufstw_sysfs_store_ee_mode(struct ufstw_lu *tw,
					 const char *buf, size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (atomic_read(&tw->ufsf->tw_state) != TW_PRESENT) {
		SYSFS_INFO("ee_mode cannot change, because current state is not TW_PRESENT (%d)..",
			   atomic_read(&tw->ufsf->tw_state));
		return -EINVAL;
	}

	if (val >= TW_EE_MODE_NUM) {
		SYSFS_INFO("wrong input.. your input %lu", val);
		return -EINVAL;
	}

	if (val)
		ufstw_auto_ee(tw->ufsf);
	else
		ufstw_disable_ee(tw->ufsf);

	SYSFS_INFO("TW_ee_mode %d", tw->ufsf->tw_ee_mode);

	return count;
}

static ssize_t ufstw_sysfs_show_flush_during_hibern_enter(struct ufstw_lu *tw,
							  char *buf)
{
	int ret;

	mutex_lock(&tw->flush_lock);
	if (ufstw_read_lu_flag(tw, QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
			       &tw->tw_flush_during_hibern_enter)) {
		mutex_unlock(&tw->flush_lock);
		return -EINVAL;
	}

	SYSFS_INFO("TW_flush_during_hibern_enter %d",
		   tw->tw_flush_during_hibern_enter);
	ret = snprintf(buf, PAGE_SIZE, "%d", tw->tw_flush_during_hibern_enter);

	mutex_unlock(&tw->flush_lock);
	return ret;
}

static ssize_t ufstw_sysfs_store_flush_during_hibern_enter(struct ufstw_lu *tw,
							   const char *buf,
							   size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (atomic_read(&tw->ufsf->tw_state) != TW_PRESENT) {
		SYSFS_INFO("tw_mode cannot change, because current state is not TW_PRESENT (%d)..",
			   atomic_read(&tw->ufsf->tw_state));
		return -EINVAL;
	}

	mutex_lock(&tw->flush_lock);
	if (tw->ufsf->tw_ee_mode == TW_EE_MODE_AUTO) {
		SYSFS_INFO("flush_during_hibern_enable cannot change on auto ee_mode");
		mutex_unlock(&tw->flush_lock);
		return -EINVAL;
	}

	if (val) {
		if (ufstw_set_lu_flag(tw,
				      QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
				      &tw->tw_flush_during_hibern_enter)) {
			mutex_unlock(&tw->flush_lock);
			return -EINVAL;
		}
	} else {
		if (ufstw_clear_lu_flag(tw,
					QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
					&tw->tw_flush_during_hibern_enter)) {
			mutex_unlock(&tw->flush_lock);
			return -EINVAL;
		}
	}

	SYSFS_INFO("TW_flush_during_hibern_enter %d",
		   tw->tw_flush_during_hibern_enter);
	mutex_unlock(&tw->flush_lock);

	return count;
}

static ssize_t ufstw_sysfs_show_flush_enable(struct ufstw_lu *tw, char *buf)
{
	int ret;

	if (ufstw_read_lu_flag(tw, QUERY_FLAG_IDN_TW_BUF_FLUSH_EN,
			       &tw->tw_flush_enable))
		return -EINVAL;

	SYSFS_INFO("TW_flush_enable %d", tw->tw_flush_enable);

	ret = snprintf(buf, PAGE_SIZE, "%d", tw->tw_flush_enable);

	return ret;
}

static ssize_t ufstw_sysfs_store_flush_enable(struct ufstw_lu *tw,
					      const char *buf, size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (atomic_read(&tw->ufsf->tw_state) != TW_PRESENT) {
		SYSFS_INFO("tw_mode cannot change, because current tw-state is not TW_PRESENT..(state:%d)..",
			   atomic_read(&tw->ufsf->tw_state));
		return -EINVAL;
	}

	if (tw->ufsf->tw_ee_mode == TW_EE_MODE_AUTO) {
		SYSFS_INFO("flush_enable cannot change on auto ee_mode");
		return -EINVAL;
	}

	if (val) {
		if (ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_BUF_FLUSH_EN,
				      &tw->tw_flush_enable))
			return -EINVAL;
	} else {
		if (ufstw_clear_lu_flag(tw, QUERY_FLAG_IDN_TW_BUF_FLUSH_EN,
					&tw->tw_flush_enable))
			return -EINVAL;
	}

	SYSFS_INFO("TW_flush_enable %d", tw->tw_flush_enable);

	return count;
}

static ssize_t ufstw_sysfs_show_debug(struct ufstw_lu *tw, char *buf)
{
	SYSFS_INFO("debug %d", tw->ufsf->tw_debug);

	return snprintf(buf, PAGE_SIZE, "%d", tw->ufsf->tw_debug);
}

static ssize_t ufstw_sysfs_store_debug(struct ufstw_lu *tw, const char *buf,
				       size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val)
		tw->ufsf->tw_debug = true;
	else
		tw->ufsf->tw_debug = false;

	SYSFS_INFO("debug %d", tw->ufsf->tw_debug);

	return count;
}

static ssize_t ufstw_sysfs_show_flush_th_min(struct ufstw_lu *tw, char *buf)
{
	SYSFS_INFO("flush_th_min%d", tw->flush_th_min);

	return snprintf(buf, PAGE_SIZE, "%d", tw->flush_th_min);
}

static ssize_t ufstw_sysfs_store_flush_th_min(struct ufstw_lu *tw,
					      const char *buf, size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val < 0 || val > 10) {
		SYSFS_INFO("input value is wrong.. your input %lu", val);
		return -EINVAL;
	}

	if (tw->flush_th_max <= val) {
		SYSFS_INFO("input value could not be greater than flush_th_max..");
		SYSFS_INFO("your input %lu, flush_th_max %u",
			   val, tw->flush_th_max);
		return -EINVAL;
	}

	tw->flush_th_min = val;
	SYSFS_INFO("flush_th_min %u", tw->flush_th_min);

	return count;
}

static ssize_t ufstw_sysfs_show_flush_th_max(struct ufstw_lu *tw, char *buf)
{
	SYSFS_INFO("flush_th_max %d", tw->flush_th_max);

	return snprintf(buf, PAGE_SIZE, "%d", tw->flush_th_max);
}

static ssize_t ufstw_sysfs_store_flush_th_max(struct ufstw_lu *tw,
					      const char *buf, size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val < 0 || val > 10) {
		SYSFS_INFO("input value is wrong.. your input %lu", val);
		return -EINVAL;
	}

	if (tw->flush_th_min >= val) {
		SYSFS_INFO("input value could not be less than flush_th_min..");
		SYSFS_INFO("your input %lu, flush_th_min %u",
			   val, tw->flush_th_min);
		return -EINVAL;
	}

	tw->flush_th_max = val;
	SYSFS_INFO("flush_th_max %u", tw->flush_th_max);

	return count;
}

static ssize_t ufstw_sysfs_show_version(struct ufstw_lu *tw, char *buf)
{
	SYSFS_INFO("TW version %.4X D/D version %.4X",
		   tw->ufsf->tw_dev_info.tw_ver, UFSTW_DD_VER);

	return snprintf(buf, PAGE_SIZE, "TW version %.4X DD version %.4X",
			tw->ufsf->tw_dev_info.tw_ver, UFSTW_DD_VER);
}

static ssize_t ufstw_sysfs_show_debug_active_cnt(struct ufstw_lu *tw, char *buf)
{
	SYSFS_INFO("debug active cnt %d",
		   atomic_read(&tw->active_cnt));

	return snprintf(buf, PAGE_SIZE, "active_cnt %d",
			atomic_read(&tw->active_cnt));
}

/* SYSFS DEFINE */
#define define_sysfs_ro(_name) __ATTR(_name, 0444,\
				      ufstw_sysfs_show_##_name, NULL),
#define define_sysfs_rw(_name) __ATTR(_name, 0644,\
				      ufstw_sysfs_show_##_name, \
				      ufstw_sysfs_store_##_name),

#define define_sysfs_attr_r_function(_name, _IDN) \
static ssize_t ufstw_sysfs_show_##_name(struct ufstw_lu *tw, char *buf) \
{ \
	if (ufstw_read_lu_attr(tw, _IDN, &tw->tw_##_name))\
		return -EINVAL;\
	SYSFS_INFO("TW_"#_name" : %u (0x%X)", tw->tw_##_name, tw->tw_##_name); \
	return snprintf(buf, PAGE_SIZE, "%u", tw->tw_##_name); \
}

/* SYSFS FUNCTION */
define_sysfs_attr_r_function(flush_status, QUERY_ATTR_IDN_TW_FLUSH_STATUS)
define_sysfs_attr_r_function(available_buffer_size, QUERY_ATTR_IDN_TW_BUF_SIZE)
define_sysfs_attr_r_function(current_tw_buffer_size, QUERY_ATTR_CUR_TW_BUF_SIZE)
define_sysfs_attr_r_function(lifetime_est, QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST)

static ssize_t ufstw_sysfs_show_tw_enable(struct ufstw_lu *tw, char *buf)
{
	if (ufstw_read_lu_flag(tw, QUERY_FLAG_IDN_TW_EN, &tw->tw_enable))
		return -EINVAL;

	SYSFS_INFO("TW_enable: %u (0x%X)", tw->tw_enable, tw->tw_enable);
	return snprintf(buf, PAGE_SIZE, "%u", tw->tw_enable);
}

static ssize_t ufstw_sysfs_store_tw_enable(struct ufstw_lu *tw, const char *buf,
					   size_t count)
{
	unsigned long val = 0;
	ssize_t ret = count;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val > 2) {
		SYSFS_INFO("wrong mode number.. your input %lu", val);
		return -EINVAL;
	}

	mutex_lock(&tw->mode_lock);
	if (atomic_read(&tw->tw_mode) == TW_MODE_DISABLED) {
		SYSFS_INFO("all turbo write life time is exhausted..");
		SYSFS_INFO("you could not change this value..");
		goto out;
	}

	if (atomic_read(&tw->tw_mode) != TW_MODE_MANUAL) {
		SYSFS_INFO("cannot set tw_enable.. current %s (%d) mode..",
			   atomic_read(&tw->tw_mode) == TW_MODE_FS ?
			   "TW_MODE_FS" : "UNKNOWN",
			   atomic_read(&tw->tw_mode));
		ret = -EINVAL;
		goto out;
	}

	if (val) {
		if (ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_EN,
				      &tw->tw_enable)) {
			ret = -EINVAL;
			goto out;
		}
	} else {
		if (ufstw_clear_lu_flag(tw, QUERY_FLAG_IDN_TW_EN,
					&tw->tw_enable)) {
			ret = -EINVAL;
			goto out;
		}
	}
out:
	mutex_unlock(&tw->mode_lock);
	SYSFS_INFO("TW_enable : %u (0x%X)", tw->tw_enable, tw->tw_enable);
	return ret;
}

static ssize_t ufstw_sysfs_show_tw_mode(struct ufstw_lu *tw, char *buf)
{
	int tw_mode = atomic_read(&tw->tw_mode);

	SYSFS_INFO("TW_mode %s %d",
		   tw_mode == TW_MODE_MANUAL ? "manual" :
		   tw_mode == TW_MODE_FS ? "fs" : "unknown", tw_mode);
	return snprintf(buf, PAGE_SIZE, "%d", tw_mode);
}

static ssize_t ufstw_sysfs_store_tw_mode(struct ufstw_lu *tw, const char *buf,
					 size_t count)
{
	int tw_mode = 0;

	if (kstrtouint(buf, 0, &tw_mode))
		return -EINVAL;

	if (atomic_read(&tw->ufsf->tw_state) != TW_PRESENT) {
		SYSFS_INFO("tw_mode cannot change, because current state is not TW_PRESENT (%d)..",
			   atomic_read(&tw->ufsf->tw_state));
		return -EINVAL;
	}

	if (tw_mode >= TW_MODE_NUM ||
	    tw_mode == TW_MODE_DISABLED) {
		SYSFS_INFO("wrong mode number.. your input %d", tw_mode);
		return -EINVAL;
	}

	mutex_lock(&tw->mode_lock);
	if (atomic_read(&tw->tw_mode) == TW_MODE_DISABLED) {
		SYSFS_INFO("all turbo write life time is exhausted..");
		SYSFS_INFO("you could not change this value..");
		count = -EINVAL;
		goto out;
	}

	if (tw_mode == atomic_read(&tw->tw_mode))
		goto out;

	count = (ssize_t) ufstw_switch_mode(tw, tw_mode);
out:
	mutex_unlock(&tw->mode_lock);
	SYSFS_INFO("TW_mode: %d", atomic_read(&tw->tw_mode));
	return count;
}

static struct ufstw_sysfs_entry ufstw_sysfs_entries[] = {
	/* tw mode select */
	define_sysfs_rw(tw_mode)

	/* Flag */
	define_sysfs_rw(tw_enable)
	define_sysfs_rw(flush_enable)
	define_sysfs_rw(flush_during_hibern_enter)

	/* Attribute */
	define_sysfs_rw(ee_mode)
	define_sysfs_ro(flush_status)
	define_sysfs_ro(available_buffer_size)
	define_sysfs_ro(current_tw_buffer_size)
	define_sysfs_ro(lifetime_est)

	/* debug */
	define_sysfs_rw(debug)
	define_sysfs_ro(debug_active_cnt)

	/* support */
	define_sysfs_rw(flush_th_max)
	define_sysfs_rw(flush_th_min)

	/* device level */
	define_sysfs_ro(version)
	__ATTR_NULL
};

static ssize_t ufstw_attr_show(struct kobject *kobj, struct attribute *attr,
			       char *page)
{
	struct ufstw_sysfs_entry *entry;
	struct ufstw_lu *tw;
	ssize_t error;

	entry = container_of(attr, struct ufstw_sysfs_entry, attr);
	tw = container_of(kobj, struct ufstw_lu, kobj);
	if (!entry->show)
		return -EIO;

	ufstw_lu_get(tw);
	mutex_lock(&tw->sysfs_lock);
	error = entry->show(tw, page);
	mutex_unlock(&tw->sysfs_lock);
	ufstw_lu_put(tw);
	return error;
}

static ssize_t ufstw_attr_store(struct kobject *kobj, struct attribute *attr,
				const char *page, size_t length)
{
	struct ufstw_sysfs_entry *entry;
	struct ufstw_lu *tw;
	ssize_t error;

	entry = container_of(attr, struct ufstw_sysfs_entry, attr);
	tw = container_of(kobj, struct ufstw_lu, kobj);

	if (!entry->store)
		return -EIO;

	ufstw_lu_get(tw);
	mutex_lock(&tw->sysfs_lock);
	error = entry->store(tw, page, length);
	mutex_unlock(&tw->sysfs_lock);
	ufstw_lu_put(tw);
	return error;
}

static const struct sysfs_ops ufstw_sysfs_ops = {
	.show = ufstw_attr_show,
	.store = ufstw_attr_store,
};

static struct kobj_type ufstw_ktype = {
	.sysfs_ops = &ufstw_sysfs_ops,
	.release = NULL,
};

static int ufstw_create_sysfs(struct ufsf_feature *ufsf, struct ufstw_lu *tw)
{
	struct device *dev = ufsf->hba->dev;
	struct ufstw_sysfs_entry *entry;
	int err;

	ufstw_lu_get(tw);
	tw->sysfs_entries = ufstw_sysfs_entries;

	kobject_init(&tw->kobj, &ufstw_ktype);
	mutex_init(&tw->sysfs_lock);

	INIT_INFO("ufstw creates sysfs ufstw_lu(%d) %p dev->kobj %p",
		  tw->lun, &tw->kobj, &dev->kobj);

	err = kobject_add(&tw->kobj, kobject_get(&dev->kobj),
			  "ufstw_lu%d", tw->lun);
	if (!err) {
		for (entry = tw->sysfs_entries; entry->attr.name != NULL;
		     entry++) {
			INIT_INFO("ufstw_lu%d sysfs attr creates: %s",
				  tw->lun, entry->attr.name);
			if (sysfs_create_file(&tw->kobj, &entry->attr))
				break;
		}
		INIT_INFO("ufstw_lu%d sysfs adds uevent", tw->lun);
		kobject_uevent(&tw->kobj, KOBJ_ADD);
	}
	ufstw_lu_put(tw);
	return err;
}
