// SPDX-License-Identifier: GPL-2.0
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
#include "ufsfeature.h"
#include "ufstw.h"

static int ufstw_create_sysfs(struct ufsf_feature *ufsf, struct ufstw_lu *tw);

inline int ufstw_get_state(struct ufsf_feature *ufsf)
{
	return atomic_read(&ufsf->tw_state);
}

inline void ufstw_set_state(struct ufsf_feature *ufsf, int state)
{
	atomic_set(&ufsf->tw_state, state);
}

static int ufstw_is_not_present(struct ufsf_feature *ufsf)
{
	enum UFSTW_STATE cur_state = ufstw_get_state(ufsf);

	if (cur_state != TW_PRESENT) {
		INFO_MSG("tw_state != TW_PRESENT (%d)", cur_state);
		return -ENODEV;
	}
	return 0;
}

#define FLAG_IDN_NAME(idn)						\
	(idn == QUERY_FLAG_IDN_TW_EN ? "tw_enable" :			\
	 idn == QUERY_FLAG_IDN_TW_BUF_FLUSH_EN ? "flush_enable" :	\
	 idn == QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN ?			\
	 "flush_hibern" : "unknown")

#define ATTR_IDN_NAME(idn)						\
	(idn == QUERY_ATTR_IDN_TW_FLUSH_STATUS ? "flush_status" :	\
	 idn == QUERY_ATTR_IDN_TW_AVAIL_BUF_SIZE ? "avail_buffer_size" :\
	 idn == QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST ? "lifetime_est" :	\
	 idn == QUERY_ATTR_IDN_TW_CURR_BUF_SIZE ? "current_buf_size" :	\
	 "unknown")

static int ufstw_read_lu_attr(struct ufstw_lu *tw, u8 idn, u32 *attr_val)
{
	struct ufs_hba *hba = tw->ufsf->hba;
	int err = 0, lun;
	u32 val;

	lun = (tw->lun == TW_LU_SHARED) ? 0 : tw->lun;
	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, idn,
				      (u8)lun, UFSFEATURE_SELECTOR, &val);
	if (err) {
		ERR_MSG("read attr [0x%.2X](%s) failed. (%d)", idn,
			ATTR_IDN_NAME(idn), err);
		goto out;
	}

	*attr_val = val;

	INFO_MSG("read attr LUN(%d) [0x%.2X](%s) success (%u)",
		 lun, idn, ATTR_IDN_NAME(idn), *attr_val);
out:
	return err;
}

static int ufstw_set_lu_flag(struct ufstw_lu *tw, u8 idn, bool *flag_res)
{
	struct ufs_hba *hba = tw->ufsf->hba;
	int err = 0, lun;

	lun = (tw->lun == TW_LU_SHARED) ? 0 : tw->lun;
	err = ufsf_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG, idn,
				    (u8)lun, UFSFEATURE_SELECTOR, NULL);
	if (err) {
		ERR_MSG("set flag [0x%.2X](%s) failed. (%d)", idn,
			FLAG_IDN_NAME(idn), err);
		goto out;
	}

	*flag_res = true;

	INFO_MSG("set flag LUN(%d) [0x%.2X](%s) success. (%u)",
		 lun, idn, FLAG_IDN_NAME(idn), *flag_res);
out:
	return err;
}

static int ufstw_clear_lu_flag(struct ufstw_lu *tw, u8 idn, bool *flag_res)
{
	struct ufs_hba *hba = tw->ufsf->hba;
	int err = 0, lun;

	lun = (tw->lun == TW_LU_SHARED) ? 0 : tw->lun;
	err = ufsf_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG, idn,
				    (u8)lun, UFSFEATURE_SELECTOR, NULL);
	if (err) {
		ERR_MSG("clear flag [0x%.2X](%s) failed. (%d)", idn,
			FLAG_IDN_NAME(idn), err);
		goto out;
	}

	*flag_res = false;

	INFO_MSG("clear flag LUN(%d) [0x%.2X](%s) success. (%u)",
		 lun, idn, FLAG_IDN_NAME(idn), *flag_res);
out:
	return err;
}

static int ufstw_read_lu_flag(struct ufstw_lu *tw, u8 idn, bool *flag_res)
{
	struct ufs_hba *hba = tw->ufsf->hba;
	int err = 0, lun;
	bool val;

	lun = (tw->lun == TW_LU_SHARED) ? 0 : tw->lun;
	err = ufsf_query_flag_retry(hba, UPIU_QUERY_OPCODE_READ_FLAG, idn,
				    (u8)lun, UFSFEATURE_SELECTOR, &val);
	if (err) {
		ERR_MSG("read flag [0x%.2X](%s) failed. (%d)", idn,
			FLAG_IDN_NAME(idn), err);
		goto out;
	}

	*flag_res = val;

	INFO_MSG("read flag LUN(%d) [0x%.2X](%s) success. (%u)",
		 lun, idn, FLAG_IDN_NAME(idn), *flag_res);
out:
	return err;
}

static inline bool ufstw_is_write_lrbp(struct ufshcd_lrb *lrbp)
{
	if (lrbp->cmd->cmnd[0] == WRITE_10 || lrbp->cmd->cmnd[0] == WRITE_16)
		return true;

	return false;
}

static void ufstw_switch_disable_state(struct ufstw_lu *tw)
{
	int err = 0;

	WARN_MSG("dTurboWriteBUfferLifeTImeEst (0x%.2X)", tw->lifetime_est);
	WARN_MSG("tw-mode will change to disable-mode");

	mutex_lock(&tw->sysfs_lock);
	ufstw_set_state(tw->ufsf, TW_FAILED);
	mutex_unlock(&tw->sysfs_lock);

	if (tw->tw_enable) {
		pm_runtime_get_sync(tw->ufsf->hba->dev);
		err = ufstw_clear_lu_flag(tw, QUERY_FLAG_IDN_TW_EN,
					  &tw->tw_enable);
		pm_runtime_put_sync(tw->ufsf->hba->dev);
		if (err)
			WARN_MSG("tw_enable flag clear failed");
	}
}

static int ufstw_check_lifetime_not_guarantee(struct ufstw_lu *tw)
{
	bool disable_flag = false;

	if (tw->lifetime_est & MASK_UFSTW_LIFETIME_NOT_GUARANTEE) {
		if (tw->lun == TW_LU_SHARED)
			WARN_MSG("lun-shared lifetime_est[31] (1)");
		else
			WARN_MSG("lun %d lifetime_est[31] (1)",
				    tw->lun);

		WARN_MSG("Device not guarantee the lifetime of TW Buffer");
#if defined(CONFIG_UFSTW_IGNORE_GUARANTEE_BIT)
		WARN_MSG("but we will ignore them for PoC");
#else
		disable_flag = true;
#endif
	}

	if (disable_flag ||
	    (tw->lifetime_est & ~MASK_UFSTW_LIFETIME_NOT_GUARANTEE) >=
	    UFSTW_MAX_LIFETIME_VALUE) {
		ufstw_switch_disable_state(tw);
		return -ENODEV;
	}

	return 0;
}

static void ufstw_lifetime_work_fn(struct work_struct *work)
{
	struct ufstw_lu *tw;
	int ret;

	tw = container_of(work, struct ufstw_lu, tw_lifetime_work);

	if (ufstw_is_not_present(tw->ufsf))
		return;

	pm_runtime_get_sync(tw->ufsf->hba->dev);
	ret = ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST,
				 &tw->lifetime_est);
	pm_runtime_put_sync(tw->ufsf->hba->dev);
	if (ret)
		return;

	ufstw_check_lifetime_not_guarantee(tw);
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
	spin_unlock_bh(&tw->lifetime_lock);

	TMSG(tw->ufsf, lrbp->lun, "%s:%d tw_lifetime_work %u",
	     __func__, __LINE__, tw->stat_write_sec);
}

static inline void ufstw_init_lu_jobs(struct ufstw_lu *tw)
{
	INIT_WORK(&tw->tw_lifetime_work, ufstw_lifetime_work_fn);
}

static inline void ufstw_cancel_lu_jobs(struct ufstw_lu *tw)
{
	int ret;

	ret = cancel_work_sync(&tw->tw_lifetime_work);
	INFO_MSG("cancel_work_sync(tw_lifetime_work) ufstw_lu[%d] (%d)",
		 tw->lun, ret);
}

static inline int ufstw_version_check(struct ufstw_dev_info *tw_dev_info)
{
	INFO_MSG("Support TW Spec : Driver = %.4X, Device = %.4X",
		 UFSTW_VER, tw_dev_info->tw_ver);

	INFO_MSG("TW Driver Version : %.6X%s", UFSTW_DD_VER,
		 UFSTW_DD_VER_POST);

	if (tw_dev_info->tw_ver != UFSTW_VER)
		return -ENODEV;

	return 0;
}

void ufstw_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf)
{
	struct ufstw_dev_info *tw_dev_info = &ufsf->tw_dev_info;

	if (LI_EN_32(&desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP]) &
		     UFS_FEATURE_SUPPORT_TW_BIT) {
		INFO_MSG("bUFSExFeaturesSupport: TW is set");
	} else {
		ERR_MSG("bUFSExFeaturesSupport: TW not support");
		ufstw_set_state(ufsf, TW_FAILED);
		return;
	}
	tw_dev_info->tw_buf_no_reduct =
		desc_buf[DEVICE_DESC_PARAM_TW_RETURN_TO_USER];
	tw_dev_info->tw_buf_type = desc_buf[DEVICE_DESC_PARAM_TW_BUF_TYPE];
	tw_dev_info->tw_shared_buf_alloc_units =
	   LI_EN_32(&desc_buf[DEVICE_DESC_PARAM_TW_SHARED_BUF_ALLOC_UNITS]);
	tw_dev_info->tw_ver = LI_EN_16(&desc_buf[DEVICE_DESC_PARAM_TW_VER]);

	if (ufstw_version_check(tw_dev_info)) {
		ERR_MSG("TW Spec Version mismatch. TW disabled");
		ufstw_set_state(ufsf, TW_FAILED);
		return;
	}

	INFO_MSG("tw_dev [53] bTurboWriteBufferNoUserSpaceReductionEn (%u)",
		 tw_dev_info->tw_buf_no_reduct);
	INFO_MSG("tw_dev [54] bTurboWriteBufferType (%u)",
		 tw_dev_info->tw_buf_type);
	INFO_MSG("tw_dev [55] dNumSharedTUrboWriteBufferAllocUnits (%u)",
		 tw_dev_info->tw_shared_buf_alloc_units);

	if (tw_dev_info->tw_buf_type == TW_BUF_TYPE_SHARED &&
	    tw_dev_info->tw_shared_buf_alloc_units == 0) {
		ERR_MSG("TW use shared buffer. But alloc unit is (0)");
		ufstw_set_state(ufsf, TW_FAILED);
		return;
	}
}

void ufstw_get_geo_info(struct ufsf_feature *ufsf, u8 *geo_buf)
{
	struct ufstw_dev_info *tw_dev_info = &ufsf->tw_dev_info;

	tw_dev_info->tw_number_lu = geo_buf[GEOMETRY_DESC_TW_NUMBER_LU];
	if (tw_dev_info->tw_number_lu == 0) {
		ERR_MSG("Turbo Write is not supported");
		ufstw_set_state(ufsf, TW_FAILED);
		return;
	}

	INFO_MSG("tw_geo [4F:52] dTurboWriteBufferMaxNAllocUnits (%u)",
		 LI_EN_32(&geo_buf[GEOMETRY_DESC_TW_MAX_SIZE]));
	INFO_MSG("tw_geo [53] bDeviceMaxTurboWriteLUs (%u)",
		 tw_dev_info->tw_number_lu);
	INFO_MSG("tw_geo [54] bTurboWriteBufferCapAdjFac (%u)",
		 geo_buf[GEOMETRY_DESC_TW_CAP_ADJ_FAC]);
	INFO_MSG("tw_geo [55] bSupportedTWBufferUserSpaceReductionTypes (%u)",
		 geo_buf[GEOMETRY_DESC_TW_SUPPORT_USER_REDUCTION_TYPES]);
	INFO_MSG("tw_geo [56] bSupportedTurboWriteBufferTypes (%u)",
		 geo_buf[GEOMETRY_DESC_TW_SUPPORT_BUF_TYPE]);
}

static void ufstw_alloc_shared_lu(struct ufsf_feature *ufsf)
{
	struct ufstw_lu *tw;

	tw = kzalloc(sizeof(struct ufstw_lu), GFP_KERNEL);
	if (!tw) {
		ERR_MSG("ufstw_lu[shared] memory alloc failed");
		return;
	}

	tw->lun = TW_LU_SHARED;
	tw->ufsf = ufsf;
	ufsf->tw_lup[0] = tw;

	INFO_MSG("ufstw_lu[shared] is TurboWrite-Enabled");
}

static void ufstw_get_lu_info(struct ufsf_feature *ufsf, int lun, u8 *lu_buf)
{
	struct ufsf_lu_desc lu_desc;
	struct ufstw_lu *tw;

	lu_desc.tw_lu_buf_size =
		LI_EN_32(&lu_buf[UNIT_DESC_TW_LU_WRITE_BUFFER_ALLOC_UNIT]);

	ufsf->tw_lup[lun] = NULL;

	if (lu_desc.tw_lu_buf_size) {
		ufsf->tw_lup[lun] =
			kzalloc(sizeof(struct ufstw_lu), GFP_KERNEL);
		if (!ufsf->tw_lup[lun]) {
			ERR_MSG("ufstw_lu[%d] memory alloc faild", lun);
			return;
		}

		tw = ufsf->tw_lup[lun];
		tw->ufsf = ufsf;
		tw->lun = lun;
		INFO_MSG("ufstw_lu[%d] [29:2C] dLUNumTWBufferAllocUnits (%u)",
			 lun, lu_desc.tw_lu_buf_size);
		INFO_MSG("ufstw_lu[%d] is TurboWrite-Enabled.", lun);
	} else {
		INFO_MSG("ufstw_lu[%d] [29:2C] dLUNumTWBufferAllocUnits (%u)",
			 lun, lu_desc.tw_lu_buf_size);
		INFO_MSG("ufstw_lu[%d] is TurboWrite-disabled", lun);
	}
}

inline void ufstw_alloc_lu(struct ufsf_feature *ufsf,
				  int lun, u8 *lu_buf)
{
	if (ufsf->tw_dev_info.tw_buf_type == TW_BUF_TYPE_SHARED &&
	    !ufsf->tw_lup[0])
		ufstw_alloc_shared_lu(ufsf);
	else if (ufsf->tw_dev_info.tw_buf_type == TW_BUF_TYPE_LU)
		ufstw_get_lu_info(ufsf, lun, lu_buf);
}

static inline void ufstw_print_lu_flag_attr(struct ufstw_lu *tw)
{
	char lun_str[20] = { 0 };

	if (tw->lun == TW_LU_SHARED)
		snprintf(lun_str, 7, "shared");
	else
		snprintf(lun_str, 2, "%d", tw->lun);

	INFO_MSG("tw_flag ufstw_lu[%s] IDN (0x%.2X) tw_enable (%d)",
		 lun_str, QUERY_FLAG_IDN_TW_EN, tw->tw_enable);
	INFO_MSG("tw_flag ufstw_lu[%s] IDN (0x%.2X) flush_enable (%d)",
		 lun_str, QUERY_FLAG_IDN_TW_BUF_FLUSH_EN,
		 tw->flush_enable);
	INFO_MSG("tw_flag ufstw_lu[%s] IDN (0x%.2X) flush_hibern (%d)",
		 lun_str, QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
		 tw->flush_during_hibern_enter);

	INFO_MSG("tw_attr ufstw_lu[%s] IDN (0x%.2X) flush_status (%u)",
		 lun_str, QUERY_ATTR_IDN_TW_FLUSH_STATUS, tw->flush_status);
	INFO_MSG("tw_attr ufstw_lu[%s] IDN (0x%.2X) buffer_size (%u)",
		 lun_str, QUERY_ATTR_IDN_TW_AVAIL_BUF_SIZE,
		 tw->available_buffer_size);
	INFO_MSG("tw_attr ufstw_lu[%s] IDN (0x%.2X) buffer_lifetime (0x%.2X)",
		 lun_str, QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST,
		 tw->lifetime_est);
}

static inline void ufstw_lu_update(struct ufstw_lu *tw)
{
	/* Flag */
	pm_runtime_get_sync(tw->ufsf->hba->dev);
	if (ufstw_read_lu_flag(tw, QUERY_FLAG_IDN_TW_EN, &tw->tw_enable))
		goto error_put;

	if (ufstw_read_lu_flag(tw, QUERY_FLAG_IDN_TW_BUF_FLUSH_EN,
			       &tw->flush_enable))
		goto error_put;

	if (ufstw_read_lu_flag(tw, QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
			       &tw->flush_during_hibern_enter))
		goto error_put;

	/* Attribute */
	if (ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_FLUSH_STATUS,
			       &tw->flush_status))
		goto error_put;

	if (ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_AVAIL_BUF_SIZE,
			       &tw->available_buffer_size))
		goto error_put;

	ufstw_read_lu_attr(tw, QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST,
			       &tw->lifetime_est);
error_put:
	pm_runtime_put_sync(tw->ufsf->hba->dev);
}

static int ufstw_lu_init(struct ufsf_feature *ufsf, int lun)
{
	struct ufstw_lu *tw;
	int ret = 0;

	if (lun == TW_LU_SHARED)
		tw = ufsf->tw_lup[0];
	else
		tw = ufsf->tw_lup[lun];

	tw->ufsf = ufsf;
	spin_lock_init(&tw->lifetime_lock);

	ufstw_lu_update(tw);

	ret = ufstw_check_lifetime_not_guarantee(tw);
	if (ret)
		goto err_out;

	ufstw_print_lu_flag_attr(tw);

	tw->stat_write_sec = 0;

	ufstw_init_lu_jobs(tw);

#if defined(CONFIG_UFSTW_BOOT_ENABLED)
	pm_runtime_get_sync(ufsf->hba->dev);
	ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_EN, &tw->tw_enable);
	ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
			  &tw->flush_during_hibern_enter);
	pm_runtime_put_sync(ufsf->hba->dev);
#endif
	ret = ufstw_create_sysfs(ufsf, tw);
	if (ret)
		ERR_MSG("create sysfs failed");
err_out:
	return ret;
}

void ufstw_init(struct ufsf_feature *ufsf)
{
	int lun, ret = 0;
	unsigned int tw_enabled_lun = 0;

	INFO_MSG("init start.. tw_state (%d)", ufstw_get_state(ufsf));

	if (ufsf->tw_dev_info.tw_buf_type == TW_BUF_TYPE_SHARED) {
		if (!ufsf->tw_lup[0]) {
			ERR_MSG("tw_lup memory allocation failed");
			goto out;
		}
		BUG_ON(ufsf->tw_lup[0]->lun != TW_LU_SHARED);

		ret = ufstw_lu_init(ufsf, TW_LU_SHARED);
		if (ret)
			goto out_free_mem;

		INFO_MSG("ufstw_lu[shared] working");
		tw_enabled_lun++;
	} else {
		seq_scan_lu(lun) {
			if (!ufsf->tw_lup[lun])
				continue;

			ret = ufstw_lu_init(ufsf, lun);
			if (ret)
				goto out_free_mem;

			INFO_MSG("ufstw_lu[%d] working", lun);
			tw_enabled_lun++;
		}

		if (tw_enabled_lun > ufsf->tw_dev_info.tw_number_lu) {
			ERR_MSG("lu count mismatched");
			goto out_free_mem;
		}
	}

	if (tw_enabled_lun == 0) {
		ERR_MSG("tw_enabled_lun count zero");
		goto out_free_mem;
	}

	ufstw_set_state(ufsf, TW_PRESENT);
	return;
out_free_mem:
	seq_scan_lu(lun) {
		kfree(ufsf->tw_lup[lun]);
		ufsf->tw_lup[lun] = NULL;
	}
out:
	ERR_MSG("Turbo write initialization failed");

	ufstw_set_state(ufsf, TW_FAILED);
}

static inline void ufstw_remove_sysfs(struct ufstw_lu *tw)
{
	int ret;

	ret = kobject_uevent(&tw->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject removed (%d)", ret);
	kobject_del(&tw->kobj);
}

void ufstw_remove(struct ufsf_feature *ufsf)
{
	struct ufstw_lu *tw;
	int lun;

	dump_stack();
	INFO_MSG("start release");

	ufstw_set_state(ufsf, TW_FAILED);

	if (ufsf->tw_dev_info.tw_buf_type == TW_BUF_TYPE_SHARED) {
		tw = ufsf->tw_lup[0];
		INFO_MSG("ufstw_lu[shared] %p", tw);
		ufsf->tw_lup[0] = NULL;
		ufstw_cancel_lu_jobs(tw);
		ufstw_remove_sysfs(tw);
		kfree(tw);
	} else {
		seq_scan_lu(lun) {
			tw = ufsf->tw_lup[lun];
			INFO_MSG("ufstw_lu[%d] %p", lun, tw);

			if (!tw)
				continue;
			ufsf->tw_lup[lun] = NULL;
			ufstw_cancel_lu_jobs(tw);
			ufstw_remove_sysfs(tw);
			kfree(tw);
		}
	}

	INFO_MSG("end release");
}

static void ufstw_reset_query_handling(struct ufstw_lu *tw)
{
	int ret;

	pm_runtime_get(tw->ufsf->hba->dev);
	if (tw->tw_enable) {
		ret = ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_EN,
					&tw->tw_enable);
		if (ret)
			tw->tw_enable = false;
	}

	if (tw->flush_enable) {
		ret = ufstw_set_lu_flag(tw, QUERY_FLAG_IDN_TW_BUF_FLUSH_EN,
					&tw->flush_enable);
		if (ret)
			tw->flush_enable = false;
	}

	if (tw->flush_during_hibern_enter) {
		ret = ufstw_set_lu_flag(tw,
					QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN,
					&tw->flush_during_hibern_enter);
		if (ret)
			tw->flush_during_hibern_enter = false;
	}

	pm_runtime_put_noidle(tw->ufsf->hba->dev);
}

static void ufstw_restore_flags(struct ufsf_feature *ufsf)
{
	struct ufstw_lu *tw;
	int lun;

	if (ufsf->tw_dev_info.tw_buf_type == TW_BUF_TYPE_SHARED) {
		tw = ufsf->tw_lup[0];

		INFO_MSG("ufstw_lu[shared] restore");
		ufstw_reset_query_handling(tw);
	} else {
		seq_scan_lu(lun) {
			tw = ufsf->tw_lup[lun];
			if (!tw)
				continue;

			INFO_MSG("ufstw_lu[%d] restore", lun);
			ufstw_reset_query_handling(tw);
		}
	}

	INFO_MSG("ufstw reset finish");
}

void ufstw_suspend(struct ufsf_feature *ufsf)
{
	ufstw_set_state(ufsf, TW_SUSPEND);
}

void ufstw_resume(struct ufsf_feature *ufsf, bool is_link_off)
{
	INFO_MSG("ufstw resume start.");
	ufstw_set_state(ufsf, TW_PRESENT);

	if (is_link_off)
		ufstw_restore_flags(ufsf);
}

void ufstw_reset_host(struct ufsf_feature *ufsf)
{
	struct ufstw_lu *tw;
	int lun;

	if (ufstw_is_not_present(ufsf))
		return;

	ufstw_set_state(ufsf, TW_RESET);
	if (ufsf->tw_dev_info.tw_buf_type == TW_BUF_TYPE_SHARED) {
		tw = ufsf->tw_lup[0];

		INFO_MSG("ufstw_lu[shared] cancel jobs");
		ufstw_cancel_lu_jobs(tw);
	} else {
		seq_scan_lu(lun) {
			tw = ufsf->tw_lup[lun];
			if (!tw)
				continue;

			INFO_MSG("ufstw_lu[%d] cancel jobs", lun);
			ufstw_cancel_lu_jobs(tw);
		}
	}
}

void ufstw_reset(struct ufsf_feature *ufsf)
{
	INFO_MSG("ufstw reset start.");
	ufstw_set_state(ufsf, TW_PRESENT);

	ufstw_restore_flags(ufsf);
}

#define ufstw_sysfs_attr_show_func(_query, _name, _IDN, hex)		\
static ssize_t ufstw_sysfs_show_##_name(struct ufstw_lu *tw, char *buf)	\
{									\
	int ret;							\
	enum UFSTW_STATE cur_state = ufstw_get_state(tw->ufsf);		\
									\
	if (cur_state != TW_PRESENT && cur_state != TW_SUSPEND)		\
		return -ENODEV;						\
									\
	pm_runtime_get_sync(tw->ufsf->hba->dev);			\
	ret = ufstw_read_lu_##_query(tw, _IDN, &tw->_name);		\
	pm_runtime_put_sync(tw->ufsf->hba->dev);			\
	if (ret)							\
		return -ENODEV;						\
									\
	INFO_MSG("read "#_query" "#_name" %u (0x%X)",			\
		 tw->_name, tw->_name);					\
	if (hex)							\
		return snprintf(buf, PAGE_SIZE, "0x%.2X\n", tw->_name);	\
	return snprintf(buf, PAGE_SIZE, "%u\n", tw->_name);		\
}

#define ufstw_sysfs_attr_store_func(_name, _IDN)			\
static ssize_t ufstw_sysfs_store_##_name(struct ufstw_lu *tw,		\
					 const char *buf,		\
					 size_t count)			\
{									\
	unsigned long val;						\
	ssize_t ret =  count;						\
	enum UFSTW_STATE cur_state = ufstw_get_state(tw->ufsf);		\
									\
	if (kstrtoul(buf, 0, &val))					\
		return -EINVAL;						\
									\
	if (!(val == 0  || val == 1))					\
		return -EINVAL;						\
									\
	INFO_MSG("val %lu", val);					\
	if (cur_state != TW_PRESENT && cur_state != TW_SUSPEND)		\
		return -ENODEV;						\
									\
	pm_runtime_get_sync(tw->ufsf->hba->dev);			\
	if (val) {							\
		if (ufstw_set_lu_flag(tw, _IDN, &tw->_name))		\
			ret = -ENODEV;					\
	} else {							\
		if (ufstw_clear_lu_flag(tw, _IDN, &tw->_name))		\
			ret = -ENODEV;					\
	}								\
	pm_runtime_put_sync(tw->ufsf->hba->dev);			\
									\
	INFO_MSG(#_name " query success");				\
	return ret;							\
}

ufstw_sysfs_attr_show_func(flag, tw_enable, QUERY_FLAG_IDN_TW_EN, 0);
ufstw_sysfs_attr_store_func(tw_enable, QUERY_FLAG_IDN_TW_EN);
ufstw_sysfs_attr_show_func(flag, flush_enable,
			   QUERY_FLAG_IDN_TW_BUF_FLUSH_EN, 0);
ufstw_sysfs_attr_store_func(flush_enable, QUERY_FLAG_IDN_TW_BUF_FLUSH_EN);
ufstw_sysfs_attr_show_func(flag, flush_during_hibern_enter,
			   QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN, 0);
ufstw_sysfs_attr_store_func(flush_during_hibern_enter,
			    QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN);

ufstw_sysfs_attr_show_func(attr, flush_status,
			   QUERY_ATTR_IDN_TW_FLUSH_STATUS, 0);
ufstw_sysfs_attr_show_func(attr, available_buffer_size,
			   QUERY_ATTR_IDN_TW_AVAIL_BUF_SIZE, 0);
ufstw_sysfs_attr_show_func(attr, lifetime_est,
			   QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST, 1);
ufstw_sysfs_attr_show_func(attr, curr_buffer_size,
			   QUERY_ATTR_IDN_TW_CURR_BUF_SIZE, 0);

#define ufstw_sysfs_attr_ro(_name) __ATTR(_name, 0444,\
				      ufstw_sysfs_show_##_name, NULL)
#define ufstw_sysfs_attr_rw(_name) __ATTR(_name, 0644,\
				      ufstw_sysfs_show_##_name, \
				      ufstw_sysfs_store_##_name)

static struct ufstw_sysfs_entry ufstw_sysfs_entries[] = {
	/* Flag */
	ufstw_sysfs_attr_rw(tw_enable),
	ufstw_sysfs_attr_rw(flush_enable),
	ufstw_sysfs_attr_rw(flush_during_hibern_enter),
	/* Attribute */
	ufstw_sysfs_attr_ro(flush_status),
	ufstw_sysfs_attr_ro(available_buffer_size),
	ufstw_sysfs_attr_ro(lifetime_est),
	ufstw_sysfs_attr_ro(curr_buffer_size),
	__ATTR_NULL
};

static ssize_t ufstw_attr_show(struct kobject *kobj, struct attribute *attr,
			       char *page)
{
	struct ufstw_sysfs_entry *entry;
	struct ufstw_lu *tw;
	ssize_t error;

	entry = container_of(attr, struct ufstw_sysfs_entry, attr);
	if (!entry->show)
		return -EIO;

	tw = container_of(kobj, struct ufstw_lu, kobj);

	mutex_lock(&tw->sysfs_lock);
	error = entry->show(tw, page);
	mutex_unlock(&tw->sysfs_lock);
	return error;
}

static ssize_t ufstw_attr_store(struct kobject *kobj, struct attribute *attr,
				const char *page, size_t length)
{
	struct ufstw_sysfs_entry *entry;
	struct ufstw_lu *tw;
	ssize_t error;

	entry = container_of(attr, struct ufstw_sysfs_entry, attr);
	if (!entry->store)
		return -EIO;

	tw = container_of(kobj, struct ufstw_lu, kobj);

	mutex_lock(&tw->sysfs_lock);
	error = entry->store(tw, page, length);
	mutex_unlock(&tw->sysfs_lock);
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
	char lun_str[20] = { 0 };

	tw->sysfs_entries = ufstw_sysfs_entries;

	kobject_init(&tw->kobj, &ufstw_ktype);
	mutex_init(&tw->sysfs_lock);

	if (tw->lun == TW_LU_SHARED) {
		snprintf(lun_str, 6, "ufstw");
		INFO_MSG("ufstw creates sysfs ufstw-shared");
	} else {
		snprintf(lun_str, 10, "ufstw_lu%d", tw->lun);
		INFO_MSG("ufstw creates sysfs ufstw_lu%d", tw->lun);
	}

	err = kobject_add(&tw->kobj, kobject_get(&dev->kobj), lun_str);
	if (!err) {
		for (entry = tw->sysfs_entries; entry->attr.name != NULL;
		     entry++) {
			if (tw->lun == TW_LU_SHARED)
				INFO_MSG("ufstw-shared sysfs attr creates: %s",
					 entry->attr.name);
			else
				INFO_MSG("ufstw_lu(%d) sysfs attr creates: %s",
					 tw->lun, entry->attr.name);

			err = sysfs_create_file(&tw->kobj, &entry->attr);
			if (err) {
				ERR_MSG("create entry(%s) failed",
					entry->attr.name);
				goto kobj_del;
			}
		}
		kobject_uevent(&tw->kobj, KOBJ_ADD);
	} else {
		ERR_MSG("kobject_add failed");
	}

	return err;
kobj_del:
	err = kobject_uevent(&tw->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject removed (%d)", err);
	kobject_del(&tw->kobj);
	return -EINVAL;
}

MODULE_LICENSE("GPL v2");
