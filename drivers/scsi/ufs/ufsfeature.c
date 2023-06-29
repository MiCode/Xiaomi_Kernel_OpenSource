/*
 * Universal Flash Storage Feature Support
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


#include "ufsfeature.h"
#include "ufshcd.h"
#include "ufs_quirks.h"

#if defined(CONFIG_UFSHPB)
#include "ufshpb.h"
#endif

#if defined(CONFIG_UFS_CHECK) && defined(CONFIG_FACTORY_BUILD)
#include <asm/unaligned.h>
#include "ufs-check.h"
#endif

#define QUERY_REQ_TIMEOUT				1500 /* msec */

static inline void ufsf_init_query(struct ufs_hba *hba,
				   struct ufs_query_req **request,
				   struct ufs_query_res **response,
				   enum query_opcode opcode, u8 idn,
				   u8 index, u8 selector)
{
	*request = &hba->dev_cmd.query.request;
	*response = &hba->dev_cmd.query.response;
	memset(*request, 0, sizeof(struct ufs_query_req));
	memset(*response, 0, sizeof(struct ufs_query_res));
	(*request)->upiu_req.opcode = opcode;
	(*request)->upiu_req.idn = idn;
	(*request)->upiu_req.index = index;
	(*request)->upiu_req.selector = selector;
}

/*
 * ufs feature common functions.
 */
int ufsf_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
		    enum flag_idn idn, u8 index, bool *flag_res)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	u8 selector;
	int err;

	BUG_ON(!hba);

	ufshcd_hold(hba, false);
	mutex_lock(&hba->dev_cmd.lock);

	if (hba->card->wmanufacturerid == UFS_VENDOR_SAMSUNG ||
		hba->card->wmanufacturerid == UFS_VENDOR_MICRON)
		selector = UFSFEATURE_SELECTOR;
	else
		selector = 0;

	/*
	 * Init the query response and request parameters
	 */
	ufsf_init_query(hba, &request, &response, opcode, idn, index,
			selector);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_SET_FLAG:
	case UPIU_QUERY_OPCODE_CLEAR_FLAG:
	case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		if (!flag_res) {
			/* No dummy reads */
			dev_err(hba->dev, "%s: Invalid argument for read request\n",
					__func__);
			err = -EINVAL;
			goto out_unlock;
		}
		break;
	default:
		dev_err(hba->dev,
			"%s: Expected query flag opcode but got = %d\n",
			__func__, opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	/* Send query request */
	err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, QUERY_REQ_TIMEOUT);
	if (err) {
		dev_err(hba->dev,
			"%s: Sending flag query for idn %d failed, err = %d\n",
			__func__, idn, err);
		goto out_unlock;
	}

	if (flag_res)
		*flag_res = (be32_to_cpu(response->upiu_res.value) &
				MASK_QUERY_UPIU_FLAG_LOC) & 0x1;

out_unlock:
	mutex_unlock(&hba->dev_cmd.lock);
	ufshcd_release(hba);
	return err;
}

int ufsf_query_flag_retry(struct ufs_hba *hba, enum query_opcode opcode,
			  enum flag_idn idn, u8 idx, bool *flag_res)
{
	int ret;
	int retries;

	BUG_ON(idx > 7);

	for (retries = 0; retries < UFSF_QUERY_REQ_RETRIES; retries++) {
		ret = ufsf_query_flag(hba, opcode, idn, idx, flag_res);
		if (ret)
			dev_dbg(hba->dev,
				"%s: failed with error %d, retries %d\n",
				__func__, ret, retries);
		else
			break;
	}
	if (ret)
		dev_err(hba->dev,
			"%s: query flag, opcode %d, idn %d, failed with error %d after %d retires\n",
			__func__, opcode, idn, ret, retries);
	return ret;
}

int ufsf_query_attr_retry(struct ufs_hba *hba, enum query_opcode opcode,
			  enum attr_idn idn, u8 idx, u32 *attr_val)
{
	int ret;
	int retries;
	u8 selector;

	if (hba->card->wmanufacturerid == UFS_VENDOR_SAMSUNG ||
		hba->card->wmanufacturerid == UFS_VENDOR_MICRON)
		selector = UFSFEATURE_SELECTOR;
	else
		selector = 0;

	for (retries = 0; retries < UFSF_QUERY_REQ_RETRIES; retries++) {
		ret = ufshcd_query_attr(hba, opcode, idn, idx,
					selector, attr_val);
		if (ret)
			dev_dbg(hba->dev,
				"%s: failed with error %d, retries %d\n",
				__func__, ret, retries);
		else
			break;
	}
	if (ret)
		dev_err(hba->dev,
			"%s: query attr, opcode %d, idn %d, failed with error %d after %d retires\n",
			__func__, opcode, idn, ret, retries);
	return ret;
}

static int ufsf_read_desc(struct ufs_hba *hba, u8 desc_id, u8 desc_index,
			  u8 selector, u8 *desc_buf, u32 size)
{
	int err = 0;

	pm_runtime_get_sync(hba->dev);

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    desc_id, desc_index,
					    selector,
					    desc_buf, &size);
	if (err)
		ERR_MSG("reading Device Desc failed. err = %d", err);

	pm_runtime_put_sync(hba->dev);

	return err;
}

static int ufsf_read_dev_desc(struct ufsf_feature *ufsf, u8 selector)
{
	u8 desc_buf[UFSF_QUERY_DESC_DEVICE_MAX_SIZE] = {0};
	int ret;

	ret = ufsf_read_desc(ufsf->hba, QUERY_DESC_IDN_DEVICE, 0, selector,
			     desc_buf, UFSF_QUERY_DESC_DEVICE_MAX_SIZE);
	if (ret)
		return ret;

	ufsf->num_lu = desc_buf[DEVICE_DESC_PARAM_NUM_LU];
	INFO_MSG("device lu count %d", ufsf->num_lu);

	INFO_MSG("sel=%u length=%u(0x%x) bSupport=0x%.2x, extend=0x%.2x_%.2x",
		 selector, desc_buf[DEVICE_DESC_PARAM_LEN],
		 desc_buf[DEVICE_DESC_PARAM_LEN],
		 desc_buf[DEVICE_DESC_PARAM_FEAT_SUP],
		 desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+2],
		 desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+3]);

	INFO_MSG("Driver Feature Version : (%.6X%s)", UFSFEATURE_DD_VER,
		 UFSFEATURE_DD_VER_POST);

#if defined(CONFIG_UFSHPB)
	ufshpb_get_dev_info(ufsf, desc_buf);
#endif

#if defined(CONFIG_UFSTW)
	ufstw_get_dev_info(ufsf, desc_buf);
#endif
	return 0;
}

static int ufsf_read_geo_desc(struct ufsf_feature *ufsf, u8 selector)
{
	u8 geo_buf[UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE];
	int ret;
	u64 total_size = 0;

	ret = ufsf_read_desc(ufsf->hba, QUERY_DESC_IDN_GEOMETRY, 0, selector,
			     geo_buf, UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE);
	if (ret)
		return ret;
#if defined(CONFIG_UFS_CHECK) && defined(CONFIG_FACTORY_BUILD)
	total_size = get_unaligned_be64(&geo_buf[0x04]);
	fill_total_gb(ufsf->hba, total_size);
#endif
#if defined(CONFIG_UFSHPB)
	if (ufshpb_get_state(ufsf) == HPB_NEED_INIT)
		ufshpb_get_geo_info(ufsf, geo_buf);
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_NEED_INIT)
		ufstw_get_geo_info(ufsf, geo_buf);
#endif
	return 0;
}

static void ufsf_read_unit_desc(struct ufsf_feature *ufsf, int lun, u8 selector)
{
	u8 unit_buf[UFSF_QUERY_DESC_UNIT_MAX_SIZE];
	int lu_enable, ret = 0;

	ret = ufsf_read_desc(ufsf->hba, QUERY_DESC_IDN_UNIT, lun, selector,
			     unit_buf, UFSF_QUERY_DESC_UNIT_MAX_SIZE);
	if (ret) {
		ERR_MSG("read unit desc failed. ret (%d)", ret);
		goto out;
	}

	lu_enable = unit_buf[UNIT_DESC_PARAM_LU_ENABLE];
	if (!lu_enable)
		return;
#if defined(CONFIG_UFS_CHECK) && defined(CONFIG_FACTORY_BUILD)
	else if (lun == 2 && lu_enable != 2)
		check_hpb_and_tw_provsion(ufsf->hba);
#endif
#if defined(CONFIG_UFSHPB)
	if (ufshpb_get_state(ufsf) == HPB_NEED_INIT)
		ufshpb_get_lu_info(ufsf, lun, unit_buf);
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_NEED_INIT)
		ufstw_alloc_lu(ufsf, lun, unit_buf);
#endif
#if defined(CONFIG_UFS_CHECK) && defined(CONFIG_FACTORY_BUILD)
	if (lun == 2 && ufsf->hba->card->wmanufacturerid != UFS_VENDOR_SKHYNIX) {
		if (check_wb_hpb_size(ufsf->hba) == -1)
			check_hpb_and_tw_provsion(ufsf->hba);
	}
#endif
out:
	return;
}

void ufsf_device_check(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = &hba->ufsf;
	int ret, lun;
	u32 status;
	u8 selector = 0;

	ufsf->hba = hba;

	if (hba->card->wmanufacturerid == UFS_VENDOR_SAMSUNG ||
	    hba->card->wmanufacturerid == UFS_VENDOR_MICRON) {
		selector = UFSFEATURE_SELECTOR;
		if (hba->card->wmanufacturerid == UFS_VENDOR_SAMSUNG) {
			ufshcd_query_attr(ufsf->hba, UPIU_QUERY_OPCODE_READ_ATTR,
					QUERY_ATTR_IDN_SUP_VENDOR_OPTIONS, 0, 0, &status);
			INFO_MSG("UFS FEATURE SELECTOR Dev %d - D/D %d", status,
						UFSFEATURE_SELECTOR);
			}
	    }

	ret = ufsf_read_dev_desc(ufsf, selector);
	if (ret)
		return;

	ret = ufsf_read_geo_desc(ufsf, selector);
	if (ret)
		return;

	seq_scan_lu(lun)
		ufsf_read_unit_desc(ufsf, lun, selector);
}

static void ufsf_print_query_buf(unsigned char *field, int size)
{
	unsigned char buf[255];
	int count = 0;
	int i;

	count += snprintf(buf, 8, "(0x00):");

	for (i = 0; i < size; i++) {
		count += snprintf(buf + count, 4, " %.2X", field[i]);

		if ((i + 1) % 16 == 0) {
			buf[count] = '\n';
			buf[count + 1] = '\0';
			printk(buf);
			count = 0;
			count += snprintf(buf, 8, "(0x%.2X):", i + 1);
		} else if ((i + 1) % 4 == 0)
			count += snprintf(buf + count, 3, " :");
	}
	buf[count] = '\n';
	buf[count + 1] = '\0';
	printk(buf);
}

inline int ufsf_check_query(__u32 opcode)
{
	return (opcode & 0xffff0000) >> 16 == UFSFEATURE_QUERY_OPCODE;
}

static inline void ufsf_set_read10_debug_cmd(unsigned char *cdb, int lba,
					     int len)
{
	cdb[0] = READ_10;
	cdb[1] = 0x02;
	cdb[2] = GET_BYTE_3(lba);
	cdb[3] = GET_BYTE_2(lba);
	cdb[4] = GET_BYTE_1(lba);
	cdb[5] = GET_BYTE_0(lba);
	cdb[6] = GET_BYTE_2(len);
	cdb[7] = GET_BYTE_1(len);
	cdb[8] = GET_BYTE_0(len);
}

static int ufsf_execute_read10_debug(struct ufsf_feature *ufsf, int lun,
				     unsigned char *cdb, void *buf, int len)
{
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdev;
	int ret = 0;

	sdev = ufsf->sdev_ufs_lu[lun];
	if (!sdev) {
		ERR_MSG("cannot find scsi_device");
		return -ENODEV;
	}

	ret = ufsf_get_scsi_device(ufsf->hba, sdev);
	if (ret)
		return ret;

	ufsf->issue_read10_debug = true;

	ret = scsi_execute(sdev, cdb, DMA_FROM_DEVICE, buf, len, NULL, &sshdr,
			   msecs_to_jiffies(30000), 3, 0, 0, NULL);

	ufsf->issue_read10_debug = false;

	scsi_device_put(sdev);

	return ret;
}

int ufsf_issue_read10_debug(struct ufsf_feature *ufsf, int lun,
			    unsigned char *buf, int buf_len)
{
	unsigned char cdb[10] = { 0 };
	int cmd_len = buf_len >> OS_PAGE_SHIFT;
	int ret = 0;

	ufsf_set_read10_debug_cmd(cdb, READ10_DEBUG_LBA, cmd_len);

	ret = ufsf_execute_read10_debug(ufsf, lun, cdb, buf, buf_len);

	if (ret < 0)
		ERR_MSG("failed with err %d", ret);

	return ret;
}

int ufsf_query_ioctl(struct ufsf_feature *ufsf, unsigned int lun,
		     void __user *buffer,
		     struct ufs_ioctl_query_data_hpb *ioctl_data, u8 selector)
{
	unsigned char *kernel_buf;
	int opcode;
	int err = 0;
	int index = 0;
	int length = 0;
	int buf_len = 0;

	opcode = ioctl_data->opcode & 0xffff;

	INFO_MSG("op %u idn %u sel %u size %u(0x%X)", opcode, ioctl_data->idn,
		 selector, ioctl_data->buf_size, ioctl_data->buf_size);

	buf_len = (ioctl_data->idn == QUERY_DESC_IDN_STRING) ?
		IOCTL_DEV_CTX_MAX_SIZE : QUERY_DESC_MAX_SIZE;

	kernel_buf = kzalloc(buf_len, GFP_KERNEL);
	if (!kernel_buf) {
		err = -ENOMEM;
		goto out;
	}

	switch (opcode) {
	case UPIU_QUERY_OPCODE_WRITE_DESC:
		err = copy_from_user(kernel_buf, buffer +
				     sizeof(struct ufs_ioctl_query_data_hpb),
				     ioctl_data->buf_size);
		INFO_MSG("buf size %d", ioctl_data->buf_size);
		ufsf_print_query_buf(kernel_buf, ioctl_data->buf_size);
		if (err)
			goto out_release_mem;
		break;

	case UPIU_QUERY_OPCODE_READ_DESC:
		switch (ioctl_data->idn) {
		case QUERY_DESC_IDN_UNIT:
			if (!ufs_is_valid_unit_desc_lun(lun)) {
				ERR_MSG("No unit descriptor for lun 0x%x", lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			index = lun;
			INFO_MSG("read lu desc lun: %d", index);
			break;

		case QUERY_DESC_IDN_STRING:
			if (!ufs_is_valid_unit_desc_lun(lun)) {
				ERR_MSG("No unit descriptor for lun 0x%x", lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			err = ufsf_issue_read10_debug(ufsf, lun, kernel_buf,
						      ioctl_data->buf_size);
			if (err < 0)
				goto out_release_mem;

			goto copy_buffer;
		case QUERY_DESC_IDN_DEVICE:
		case QUERY_DESC_IDN_GEOMETRY:
		case QUERY_DESC_IDN_CONFIGURATION:
			break;

		default:
			ERR_MSG("invalid idn %d", ioctl_data->idn);
			err = -EINVAL;
			goto out_release_mem;
		}
		break;
	default:
		ERR_MSG("invalid opcode %d", opcode);
		err = -EINVAL;
		goto out_release_mem;
	}

	length = ioctl_data->buf_size;

	err = ufshcd_query_descriptor_retry(ufsf->hba, opcode, ioctl_data->idn,
					    index, selector, kernel_buf,
					    &length);
	if (err)
		goto out_release_mem;

copy_buffer:
	if (opcode == UPIU_QUERY_OPCODE_READ_DESC) {
		err = copy_to_user(buffer, ioctl_data,
				   sizeof(struct ufs_ioctl_query_data_hpb));
		if (err)
			ERR_MSG("Failed copying back to user.");

		err = copy_to_user(buffer + sizeof(struct ufs_ioctl_query_data_hpb),
				   kernel_buf, ioctl_data->buf_size);
		if (err)
			ERR_MSG("Fail: copy rsp_buffer to user space.");
	}
out_release_mem:
	kfree(kernel_buf);
out:
	return err;
}

inline int ufsf_get_scsi_device(struct ufs_hba *hba, struct scsi_device *sdev)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		scsi_device_put(sdev);
		ERR_MSG("scsi_device_get failed.(%d)", ret);
		return -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return ret;
}

inline bool ufsf_is_valid_lun(int lun)
{
	return lun < UFS_UPIU_MAX_GENERAL_LUN;
}

inline void ufsf_slave_configure(struct ufsf_feature *ufsf,
				 struct scsi_device *sdev)
{
	if (ufsf_is_valid_lun(sdev->lun)) {
		ufsf->sdev_ufs_lu[sdev->lun] = sdev;
		ufsf->slave_conf_cnt++;
		INFO_MSG("lun[%d] sdev(%p) q(%p) slave_conf_cnt(%d/%d)",
			 (int)sdev->lun, sdev, sdev->request_queue,
			 ufsf->slave_conf_cnt, ufsf->num_lu);

#if defined(CONFIG_UFSHPB)
		if (ufsf->num_lu == ufsf->slave_conf_cnt) {
			if (ufshpb_get_state(ufsf) == HPB_NEED_INIT) {
				INFO_MSG("wakeup ufshpb_init_handler");
				wake_up(&ufsf->hpb_wait);
			}
		}
#endif
	}
}

inline void ufsf_change_read10_debug_lun(struct ufsf_feature *ufsf,
					 struct ufshcd_lrb *lrbp)
{
	int ctx_lba = LI_EN_32(lrbp->cmd->cmnd + 2);

	if (ufsf->issue_read10_debug == true && ctx_lba == READ10_DEBUG_LBA) {
		lrbp->lun = READ10_DEBUG_LUN;
		INFO_MSG("lun 0x%X lba 0x%X", lrbp->lun, ctx_lba);
	}
}


inline void ufsf_prep_fn(struct ufsf_feature *ufsf,
			 struct ufshcd_lrb *lrbp)
{
#if defined(CONFIG_UFSHPB)
	if (ufshpb_get_state(ufsf) == HPB_PRESENT &&
	    ufsf->issue_read10_debug == false)
		ufshpb_prep_fn(ufsf, lrbp);
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_PRESENT)
		ufstw_prep_fn(ufsf, lrbp);
#endif
}

inline void ufsf_reset_lu(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSTW)
	INFO_MSG("run reset_lu.. tw_state(%d) -> TW_RESET",
		 ufstw_get_state(ufsf));
	ufstw_set_state(ufsf, TW_RESET);
	ufstw_reset(ufsf, false);
#endif
}

inline void ufsf_reset_host(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHPB)
	INFO_MSG("run reset_host.. hpb_state(%d) -> HPB_RESET",
		 ufshpb_get_state(ufsf));
	if (ufshpb_get_state(ufsf) == HPB_PRESENT)
		ufshpb_reset_host(ufsf);
#endif

#if defined(CONFIG_UFSTW)
	INFO_MSG("run reset_host.. tw_state(%d) -> TW_RESET",
		 ufstw_get_state(ufsf));
	if (ufstw_get_state(ufsf) == TW_PRESENT)
		ufstw_reset_host(ufsf);
#endif
}

inline void ufsf_init(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHPB)
	if (ufshpb_get_state(ufsf) == HPB_NEED_INIT) {
		INFO_MSG("init start.. hpb_state (%d)", HPB_NEED_INIT);
		schedule_work(&ufsf->hpb_init_work);
	}
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_NEED_INIT)
		ufstw_init(ufsf);
#endif
}

inline void ufsf_reset(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHPB)
	if (ufshpb_get_state(ufsf) == HPB_RESET) {
		INFO_MSG("reset start.. hpb_state %d", HPB_RESET);
		ufshpb_reset(ufsf);
	}
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_RESET &&
	    !ufsf->hba->pm_op_in_progress) {
		INFO_MSG("reset start.. tw_state %d",
			 ufstw_get_state(ufsf));
		ufstw_reset(ufsf, false);
	}
#endif
}

inline void ufsf_remove(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHPB)
	if (ufshpb_get_state(ufsf) == HPB_PRESENT)
		ufshpb_remove(ufsf, HPB_NEED_INIT);
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_PRESENT)
		ufstw_remove(ufsf);
#endif
}

inline void ufsf_set_init_state(struct ufsf_feature *ufsf)
{
	ufsf->slave_conf_cnt = 0;
	ufsf->issue_read10_debug = false;
#if defined(CONFIG_UFSHPB)
	ufshpb_set_state(ufsf, HPB_NEED_INIT);
	INIT_WORK(&ufsf->hpb_init_work, ufshpb_init_handler);
	init_waitqueue_head(&ufsf->hpb_wait);
#endif

#if defined(CONFIG_UFSW)
	ufstw_set_state(ufsf, TW_NEED_INIT);
#endif
}

inline void ufsf_suspend(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHPB)
	/*
	 * if suspend failed, pm could call the suspend function again,
	 * in this case, ufshpb state already had been changed to SUSPEND state.
	 * so, we will not call ufshpb_suspend.
	 */
	if (ufshpb_get_state(ufsf) == HPB_PRESENT)
		ufshpb_suspend(ufsf);
#endif
}

inline void ufsf_resume(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHPB)
	if (ufshpb_get_state(ufsf) == HPB_SUSPEND ||
	    ufshpb_get_state(ufsf) == HPB_PRESENT) {
		if (ufshpb_get_state(ufsf) == HPB_PRESENT)
			WARN_MSG("warning.. hpb state PRESENT in resuming");
		ufshpb_resume(ufsf);
	}
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == HPB_RESET)
		ufstw_reset(ufsf, true);
#endif
}

inline void ufsf_on_idle(struct ufsf_feature *ufsf, bool scsi_req)
{}

/*
 * Wrapper functions for ufshpb.
 */
#if defined(CONFIG_UFSHPB)
inline int ufsf_hpb_prepare_pre_req(struct ufsf_feature *ufsf,
				    struct scsi_cmnd *cmd, int lun)
{
	if (ufshpb_get_state(ufsf) == HPB_PRESENT)
		return ufshpb_prepare_pre_req(ufsf, cmd, lun);
	return -ENODEV;
}

inline int ufsf_hpb_prepare_add_lrbp(struct ufsf_feature *ufsf, int add_tag)
{
	if (ufshpb_get_state(ufsf) == HPB_PRESENT)
		return ufshpb_prepare_add_lrbp(ufsf, add_tag);
	return -ENODEV;
}

inline void ufsf_hpb_end_pre_req(struct ufsf_feature *ufsf,
				 struct request *req)
{
	ufshpb_end_pre_req(ufsf, req);
}

inline void ufsf_hpb_noti_rb(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	if (ufshpb_get_state(ufsf) == HPB_PRESENT)
		ufshpb_rsp_upiu(ufsf, lrbp);
}

#else
inline int ufsf_hpb_prepare_pre_req(struct ufsf_feature *ufsf,
				    struct scsi_cmnd *cmd, int lun)
{
	return 0;
}

inline int ufsf_hpb_prepare_add_lrbp(struct ufsf_feature *ufsf, int add_tag)
{
	return 0;
}

inline void ufsf_hpb_end_pre_req(struct ufsf_feature *ufsf,
				 struct request *req) {}
inline void ufsf_hpb_noti_rb(struct ufsf_feature *ufsf,
			     struct ufshcd_lrb *lrbp) {}
#endif
