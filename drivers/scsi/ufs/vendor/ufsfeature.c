// SPDX-License-Identifier: GPL-2.0
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
#include "ufs-mediatek.h"

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
	u8 desc_buf[UFSF_QUERY_DESC_DEVICE_MAX_SIZE];
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
		  desc_buf[DEVICE_DESC_PARAM_UFS_FEAT],
		  desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+2],
		  desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+3]);

	INFO_MSG("Driver Feature Version : (%.6X%s)", UFSFEATURE_DD_VER,
		 UFSFEATURE_DD_VER_POST);

#if defined(CONFIG_UFSSHPB)
	ufsshpb_get_dev_info(ufsf, desc_buf);
#endif
#if defined(CONFIG_UFSTW)
	ufstw_get_dev_info(ufsf, desc_buf);
#endif
#if defined(CONFIG_UFSHID)
	ufshid_get_dev_info(ufsf, desc_buf);
#endif
#if defined(CONFIG_UFSRINGBUF)
	ufsringbuf_get_dev_info(ufsf, desc_buf);
#endif
	return 0;
}

static int ufsf_read_geo_desc(struct ufsf_feature *ufsf, u8 selector)
{
	u8 geo_buf[UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE];
	int ret;

	ret = ufsf_read_desc(ufsf->hba, QUERY_DESC_IDN_GEOMETRY, 0, selector,
			     geo_buf, UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE);
	if (ret)
		return ret;

#if defined(CONFIG_UFSSHPB)
	if (ufsshpb_get_state(ufsf) == HPB_NEED_INIT)
		ufsshpb_get_geo_info(ufsf, geo_buf);
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_NEED_INIT)
		ufstw_get_geo_info(ufsf, geo_buf);
#endif
#if defined(CONFIG_UFSRINGBUF)
	if (ufsringbuf_get_state(ufsf) == RINGBUF_NEED_INIT)
		ufsringbuf_get_geo_info(ufsf, geo_buf);
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

#if defined(CONFIG_UFSSHPB)
	if (ufsshpb_get_state(ufsf) == HPB_NEED_INIT)
		ufsshpb_get_lu_info(ufsf, lun, unit_buf);
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_NEED_INIT)
		ufstw_alloc_lu(ufsf, lun, unit_buf);
#endif
out:
	return;
}

void ufsf_device_check(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);
	int ret, lun;
	u32 status;

	ufshcd_query_attr(ufsf->hba, UPIU_QUERY_OPCODE_READ_ATTR,
			  QUERY_ATTR_IDN_SUP_VENDOR_OPTIONS, 0, 0, &status);
	INFO_MSG("UFS FEATURE SELECTOR Dev %d - D/D %d", status,
		  UFSFEATURE_SELECTOR);

	ret = ufsf_read_dev_desc(ufsf, UFSFEATURE_SELECTOR);
	if (ret)
		return;

	ret = ufsf_read_geo_desc(ufsf, UFSFEATURE_SELECTOR);
	if (ret)
		return;

	seq_scan_lu(lun)
		ufsf_read_unit_desc(ufsf, lun, UFSFEATURE_SELECTOR);
}

static int ufsf_execute_dev_ctx_req(struct ufsf_feature *ufsf,
				    int lun, unsigned char *cdb,
				    void *buf, int len)
{
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdev;
	int ret = 0;

	sdev = ufsf->sdev_ufs_lu[lun];
	if (!sdev) {
		WARN_MSG("cannot find scsi_device");
		return -ENODEV;
	}

	ufsf->issue_ioctl = true;
	ret = scsi_execute(sdev, cdb, DMA_FROM_DEVICE, buf, len, NULL, &sshdr,
			   msecs_to_jiffies(30000), 3, 0, 0, NULL);
	ufsf->issue_ioctl = false;

	return ret;
}

static inline void ufsf_set_read_dev_ctx(unsigned char *cdb, int lba, int len)
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

int ufsf_issue_req_dev_ctx(struct ufsf_feature *ufsf, int lun,
			   unsigned char *buf, int buf_len)
{
	unsigned char cdb[10] = { 0 };
	int cmd_len = buf_len >> OS_PAGE_SHIFT;
	int ret = 0;

	ufsf_set_read_dev_ctx(cdb, READ10_DEBUG_LBA, cmd_len);

	ret = ufsf_execute_dev_ctx_req(ufsf, lun, cdb, buf, buf_len);

	if (ret < 0)
		ERR_MSG("failed with err %d", ret);

	return ret;
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

int ufsf_query_ioctl(struct ufsf_feature *ufsf, int lun, void __user *buffer,
		     struct ufs_ioctl_query_data *ioctl_data, u8 selector)
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
				     sizeof(struct ufs_ioctl_query_data),
				     ioctl_data->buf_size);
		INFO_MSG("buf size %d", ioctl_data->buf_size);
		ufsf_print_query_buf(kernel_buf, ioctl_data->buf_size);
		if (err)
			goto out_release_mem;
		break;

	case UPIU_QUERY_OPCODE_READ_DESC:
		switch (ioctl_data->idn) {
		case QUERY_DESC_IDN_UNIT:
			if (!ufs_is_valid_unit_desc_lun(&ufsf->hba->dev_info, lun, 0)) {
				ERR_MSG("No unit descriptor for lun 0x%x", lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			index = lun;
			INFO_MSG("read lu desc lun: %d", index);
			break;

		case QUERY_DESC_IDN_STRING:
			if (!ufs_is_valid_unit_desc_lun(&ufsf->hba->dev_info, lun, 0)) {
				ERR_MSG("No unit descriptor for lun 0x%x", lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			err = ufsf_issue_req_dev_ctx(ufsf, lun, kernel_buf,
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
				   sizeof(struct ufs_ioctl_query_data));
		if (err)
			ERR_MSG("Failed copying back to user.");

		err = copy_to_user(buffer + sizeof(struct ufs_ioctl_query_data),
				   kernel_buf, ioctl_data->buf_size);
		if (err)
			ERR_MSG("Fail: copy rsp_buffer to user space.");
	}
out_release_mem:
	kfree(kernel_buf);
out:
	return err;
}

bool ufsf_upiu_check_for_ccd(struct ufshcd_lrb *lrbp)
{
	unsigned char *cdb = lrbp->cmd->cmnd;
	int data_seg_len, sense_data_len;

	if (cdb[0] != VENDOR_OP || cdb[1] != VENDOR_CCD)
		return false;

	data_seg_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
				       MASK_RSP_UPIU_DATA_SEG_LEN;
	sense_data_len = be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len);

	if (data_seg_len != CCD_DATA_SEG_LEN ||
	    sense_data_len != CCD_SENSE_DATA_LEN) {
		WARN_MSG("CCD info is wrong. so check it.");
		WARN_MSG("CCD data_seg_len = %d, sense_data_len %d",
			 data_seg_len, sense_data_len);
	} else {
		INFO_MSG("CCD info is correct!!");
	}

	/*
	 * sense_len will be not set as Descriptor Type isn't 0x70
	 * if not set sense_len, sense will not be able to copy
	 * in sg_scsi_ioctl()
	 */
	scsi_req(lrbp->cmd->request)->sense_len = CCD_SENSE_DATA_LEN;

	return true;
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
	struct ufs_hba *hba = shost_priv(sdev->host);

	ufsf->hba = hba;
	if (ufsf_is_valid_lun(sdev->lun)) {
		ufsf->sdev_ufs_lu[sdev->lun] = sdev;
		ufsf->slave_conf_cnt++;
		INFO_MSG("lun[%d] sdev(%p) q(%p) slave_conf_cnt(%d)",
			 (int)sdev->lun, sdev, sdev->request_queue,
			 ufsf->slave_conf_cnt);

	}
	schedule_work(&ufsf->device_check_work);
}

inline int ufsf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	int ret = 0;

#if defined(CONFIG_UFSSHPB)
	if (ufsshpb_get_state(ufsf) == HPB_PRESENT &&
	    ufsf->issue_ioctl == false) {
		ret = ufsshpb_prep_fn(ufsf, lrbp);
		if (ret == -EAGAIN)
			return ret;
	}
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_PRESENT)
		ufstw_prep_fn(ufsf, lrbp);
#endif
#if defined(CONFIG_UFSRINGBUF)
	if (ufsringbuf_get_state(ufsf) == RINGBUF_PRESENT ||
	    ufsringbuf_get_state(ufsf) == RINGBUF_RESET)
		ufsringbuf_prep_fn(ufsf, lrbp);
#endif

	return ret;
}

inline void ufsf_prepare_reset_lu(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSTW)
	INFO_MSG("run reset_lu.. tw_state(%d) -> TW_PREPARE_RESET",
		 ufstw_get_state(ufsf));
	ufstw_set_state(ufsf, TW_PREPARE_RESET);
#endif
}

inline void ufsf_reset_lu(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSTW)
	ufsf->reset_lu_pos = ufsf->hba->ufs_stats.event[UFS_EVT_DEV_RESET].pos;
	schedule_work(&ufsf->reset_lu_work);
#endif
}

inline void ufsf_reset_lu_handler(struct work_struct *work)
{
#if defined(CONFIG_UFSTW)
	struct ufsf_feature *ufsf;
	int retry;

	ufsf = container_of(work, struct ufsf_feature, reset_lu_work);

	retry = ufsf->hba->nutrs;

	if (ufstw_get_state(ufsf) != TW_PREPARE_RESET)
		return;

	// check pos diff
	while (ufsf->reset_lu_pos == ufsf->hba->ufs_stats.event[UFS_EVT_DEV_RESET].pos) {
		if (--retry < 0)
			break;

		// In ufshcd_clear_cmd(), it waits 1s for each doorbell.
		msleep(1000);
	}

	if ((ufsf->reset_lu_pos == ufsf->hba->ufs_stats.event[UFS_EVT_DEV_RESET].pos) ||
	    (ufsf->hba->ufs_stats.event[UFS_EVT_DEV_RESET].val[ufsf->reset_lu_pos] != 0)) {
		/*
		 * TW is not reset in this LU
		 * We just restore its state as PRESENT
		*/
		ufstw_set_state(ufsf, TW_PRESENT);
		return;
	}

	INFO_MSG("run reset_lu.. tw_state(%d) -> TW_RESET",
		 ufstw_get_state(ufsf));
	ufstw_set_state(ufsf, TW_RESET);
	ufstw_reset(ufsf);
#endif
}

/*
 * called by ufshcd_vops_device_reset()
 */
inline void ufsf_reset_host(struct ufsf_feature *ufsf)
{
	struct ufs_hba *hba = ufsf->hba;
	struct Scsi_Host *host = hba->host;
	unsigned long flags;
	u32 eh_flags;

	if (!ufsf->check_init)
		return;

	/*
	 * Check if it is error handling(eh) context.
	 *
	 * In the following cases, we can enter here even though it is not in eh
	 * context.
	 *  - when ufshcd_is_link_off() is true in ufshcd_resume()
	 *  - when ufshcd_vops_suspend() fails in ufshcd_suspend()
	 */
	spin_lock_irqsave(host->host_lock, flags);
	eh_flags = ufshcd_eh_in_progress(hba);
	spin_unlock_irqrestore(host->host_lock, flags);
	if (!eh_flags)
		return;

#if defined(CONFIG_UFSSHPB)
	INFO_MSG("run reset_host.. hpb_state(%d) -> HPB_RESET",
		 ufsshpb_get_state(ufsf));
	if (ufsshpb_get_state(ufsf) == HPB_PRESENT)
		ufsshpb_reset_host(ufsf);
#endif

#if defined(CONFIG_UFSTW)
	INFO_MSG("run reset_host.. tw_state(%d) -> TW_RESET",
		 ufstw_get_state(ufsf));
	if (ufstw_get_state(ufsf) == TW_PRESENT)
		ufstw_reset_host(ufsf);
#endif
#if defined(CONFIG_UFSHID)
	INFO_MSG("run reset_host.. hid_state(%d) -> HID_RESET",
		 ufshid_get_state(ufsf));
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_reset_host(ufsf);
#endif
#if defined(CONFIG_UFSRINGBUF)
	INFO_MSG("run reset_host.. ringbuf_state(%d) -> RINGBUF_RESET",
		 ufsringbuf_get_state(ufsf));
	if (ufsringbuf_get_state(ufsf) == RINGBUF_PRESENT)
		ufsringbuf_reset_host(ufsf);
#endif

	schedule_work(&ufsf->reset_wait_work);
}

inline void ufsf_init(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSSHPB)
	if (ufsshpb_get_state(ufsf) == HPB_NEED_INIT) {
		INFO_MSG("init start.. hpb_state (%d)", HPB_NEED_INIT);
		schedule_work(&ufsf->hpb_init_work);
	}
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_NEED_INIT)
		ufstw_init(ufsf);
#endif
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_NEED_INIT)
		ufshid_init(ufsf);
#endif
#if defined(CONFIG_UFSRINGBUF)
	if (ufsringbuf_get_state(ufsf) == RINGBUF_NEED_INIT)
		ufsringbuf_init(ufsf);
#endif

	ufsf->check_init = true;
}

inline void ufsf_reset(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSSHPB)
	if (ufsshpb_get_state(ufsf) == HPB_RESET) {
		INFO_MSG("reset start.. hpb_state %d", HPB_RESET);
		ufsshpb_reset(ufsf);
	}
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_RESET) {
		INFO_MSG("reset start.. tw_state %d",
			 ufstw_get_state(ufsf));
		ufstw_reset(ufsf);
	}
#endif
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_RESET)
		ufshid_reset(ufsf);
#endif
#if defined(CONFIG_UFSRINGBUF)
	if (ufsringbuf_get_state(ufsf) == RINGBUF_RESET)
		ufsringbuf_reset(ufsf);
#endif
}

inline void ufsf_remove(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSSHPB)
	if (ufsshpb_get_state(ufsf) == HPB_PRESENT)
		ufsshpb_remove(ufsf, HPB_NEED_INIT);
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_PRESENT)
		ufstw_remove(ufsf);
#endif
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_remove(ufsf);
#endif
#if defined(CONFIG_UFSRINGBUF)
	if (ufsringbuf_get_state(ufsf) == RINGBUF_PRESENT)
		ufsringbuf_remove(ufsf);
#endif
}

static void ufsf_device_check_work_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(work, struct ufsf_feature, device_check_work);

	mutex_lock(&ufsf->device_check_lock);
	if (!ufsf->check_init) {
		ufsf_device_check(ufsf->hba);
		ufsf_init(ufsf);
	}

#if defined(CONFIG_UFSSHPB)
	if (ufsf->check_init && ufsf->num_lu == ufsf->slave_conf_cnt) {
		if (ufsshpb_get_state(ufsf) == HPB_WAIT_INIT) {
			INFO_MSG("wakeup ufsshpb_init_handler");
			wake_up(&ufsf->hpb_wait);
		}
	}
#endif
	mutex_unlock(&ufsf->device_check_lock);

}
/*
 * worker to change the feature state to present after processing the error handler.
 */
static void ufsf_reset_wait_work_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf;
	struct ufs_hba *hba;
	struct Scsi_Host *host;
	u32 ufshcd_state;
	unsigned long flags;

	ufsf = container_of(work, struct ufsf_feature, reset_wait_work);
	hba = ufsf->hba;
	host = hba->host;

	/*
	 * Wait completion of hba->eh_work.
	 *
	 * reset_wait_work is scheduled at ufsf_reset_host(),
	 * so it can be waken up before eh_work is completed.
	 *
	 * ufsf_reset must be called after eh_work has completed.
	 */
	flush_work(&hba->eh_work);

	spin_lock_irqsave(host->host_lock, flags);
	ufshcd_state = hba->ufshcd_state;
	spin_unlock_irqrestore(host->host_lock, flags);

	if (ufshcd_state == UFSHCD_STATE_OPERATIONAL)
		ufsf_reset(ufsf);
}

#if defined(CONFIG_UFSHID)
static void ufsf_on_idle(struct work_struct *work)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(work, struct ufsf_feature, on_idle_work);
	if (ufshid_get_state(ufsf) == HID_PRESENT &&
	    !ufsf->hba->outstanding_reqs)
		ufshid_on_idle(ufsf);
}
#endif

inline void ufsf_set_init_state(struct ufsf_feature *ufsf)
{
	ufsf->slave_conf_cnt = 0;
	ufsf->issue_ioctl = false;

	mutex_init(&ufsf->device_check_lock);
	INIT_WORK(&ufsf->device_check_work, ufsf_device_check_work_handler);
	INIT_WORK(&ufsf->reset_wait_work, ufsf_reset_wait_work_handler);
	INIT_WORK(&ufsf->reset_lu_work, ufsf_reset_lu_handler);

#if defined(CONFIG_UFSSHPB)
	ufsshpb_set_state(ufsf, HPB_NEED_INIT);
	INIT_WORK(&ufsf->hpb_init_work, ufsshpb_init_handler);
	init_waitqueue_head(&ufsf->hpb_wait);
#endif
#if defined(CONFIG_UFSTW)
	ufstw_set_state(ufsf, TW_NEED_INIT);
#endif
#if defined(CONFIG_UFSHID)
	INIT_WORK(&ufsf->on_idle_work, ufsf_on_idle);
	ufshid_set_state(ufsf, HID_NEED_INIT);
#endif
#if defined(CONFIG_UFSRINGBUF)
	ufsringbuf_set_state(ufsf, RINGBUF_NEED_INIT);
#endif
}

inline void ufsf_suspend(struct ufsf_feature *ufsf)
{
	/*
	 * Wait completion of reset_wait_work.
	 *
	 * When suspend occurrs immediately after reset
	 * and reset_wait_work is executed late,
	 * we can enter here before ufsf_reset() cleans up the feature's reset sequence.
	 */
	flush_work(&ufsf->reset_wait_work);

#if defined(CONFIG_UFSSHPB)
	/*
	 * if suspend failed, pm could call the suspend function again,
	 * in this case, ufsshpb state already had been changed to SUSPEND state.
	 * so, we will not call ufsshpb_suspend.
	 */
	if (ufsshpb_get_state(ufsf) == HPB_PRESENT)
		ufsshpb_suspend(ufsf);
#endif
#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_PRESENT)
		ufstw_suspend(ufsf);
#endif
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_suspend(ufsf);
#endif
}

inline void ufsf_resume(struct ufsf_feature *ufsf, bool is_link_off)
{
#if defined(CONFIG_UFSSHPB)
	if (ufsshpb_get_state(ufsf) == HPB_SUSPEND ||
	    ufsshpb_get_state(ufsf) == HPB_PRESENT) {
		if (ufsshpb_get_state(ufsf) == HPB_PRESENT)
			WARN_MSG("warning.. hpb state PRESENT in resuming");
		ufsshpb_resume(ufsf);
	}
#endif

#if defined(CONFIG_UFSTW)
	if (ufstw_get_state(ufsf) == TW_SUSPEND)
		ufstw_resume(ufsf, is_link_off);
#endif
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_SUSPEND)
		ufshid_resume(ufsf);
#endif
#if defined(CONFIG_UFSRINGBUF)
	if (ufsringbuf_get_state(ufsf) == RINGBUF_PRESENT)
		ufsringbuf_resume(ufsf);
#endif
}

inline void ufsf_change_lun(struct ufsf_feature *ufsf,
			    struct ufshcd_lrb *lrbp)
{
	int ctx_lba = LI_EN_32(lrbp->cmd->cmnd + 2);

	if (unlikely(ufsf->issue_ioctl == true &&
	    ctx_lba == READ10_DEBUG_LBA)) {
		lrbp->lun = READ10_DEBUG_LUN;
		INFO_MSG("lun 0x%X lba 0x%X", lrbp->lun, ctx_lba);
	}
}

/*
 * Wrapper functions for ufsshpb.
 */
#if defined(CONFIG_UFSSHPB)
inline void ufsf_hpb_noti_rb(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	if (ufsshpb_get_state(ufsf) == HPB_PRESENT)
		ufsshpb_rsp_upiu(ufsf, lrbp);
}

#else
inline void ufsf_hpb_noti_rb(struct ufsf_feature *ufsf,
			     struct ufshcd_lrb *lrbp) {}
#endif

/*
 * Wrapper functions for ufshid.
 */
#if defined(CONFIG_UFSHID) && defined(CONFIG_UFSHID_DEBUG)
inline void ufsf_hid_acc_io_stat(struct ufsf_feature *ufsf,
				 struct ufshcd_lrb *lrbp)
{
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_acc_io_stat_during_trigger(ufsf, lrbp);
}
#else
inline void ufsf_hid_acc_io_stat(struct ufsf_feature *ufsf,
				 struct ufshcd_lrb *lrbp) {}
#endif

MODULE_LICENSE("GPL v2");
