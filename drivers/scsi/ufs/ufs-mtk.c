/*
 * Copyright (C) 2018 MediaTek Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "ufs.h"
#include <linux/bitfield.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/hie.h>
#include <linux/nls.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/reboot.h>
#include <linux/rpmb.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "unipro.h"
#include "ufs_quirks.h"
#include "ufs-mtk.h"
#include "ufs-mtk-block.h"
#include "ufs-mtk-platform.h"
#include "ufs-mtk-dbg.h"

#ifdef CONFIG_HIE
#include <mt-plat/keyhint.h>
#endif
#include <mt-plat/mtk_partition.h>
#include <mt-plat/mtk_secure_api.h>
#include <mt-plat/mtk_boot.h>
#include <scsi/ufs/ufs-mtk-ioctl.h>
#ifdef CONFIG_MTK_UFS_LBA_CRC16_CHECK
#include <linux/crc16.h>
#endif

#include "mtk_spm_resource_req.h"

/* Query request retries */
#define QUERY_REQ_RETRIES 10

/* refer to ufs_mtk_init() for default value of these globals */
int  ufs_mtk_rpm_autosuspend_delay;    /* runtime PM: auto suspend delay */
bool ufs_mtk_rpm_enabled;              /* runtime PM: on/off */
bool ufs_mtk_auto_hibern8_enabled;
bool ufs_mtk_host_deep_stall_enable;
bool ufs_mtk_host_scramble_enable;
struct ufs_hba *ufs_mtk_hba;

static bool ufs_mtk_is_data_cmd(char cmd_op);
static bool ufs_mtk_is_unmap_cmd(char cmd_op);

#ifdef CONFIG_MTK_UFS_LBA_CRC16_CHECK
/*
 * Sometimes a lba may use encrypted r/w sometime
 * and use raw r/w at other times.
 * Use two CRC16 Arrays to record Encrypted and NoEncrypted data
 */
/* Self init Encryption and No Encryption array */
static u8 di_init;
/* Logical block count */
static u64 di_blkcnt;
/* CRC value of each 4KB block */
static u16 *di_crc;
/* private data */
static u8 *di_priv;
/* only do crc to first 32 byte of a 4KB block to reduce cpu overhead */
#define DI_CRC_DATA_SIZE (32)

static u8 ufs_mtk_di_get_priv(struct scsi_cmnd *cmd)
{
	u8 priv;
	const unsigned char *key = NULL;

	hie_key_payload(&cmd->request->bio->bi_crypt_ctx, &key);
	priv = key[0];
	if (!priv)
		priv++;

	return priv;
}

/* Init Encryption and No Encryption array and others */
void ufs_mtk_di_init(struct ufs_hba *hba)
{
	u8 ud_buf[UNIT_DESC_PARAM_ERASE_BLK_SIZE] = { 0 };
	int len = UNIT_DESC_PARAM_ERASE_BLK_SIZE;

	/* Already init */
	if (di_init != 0)
		return;

	/* Read LU2 unit descriptor */
	ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
		QUERY_DESC_IDN_UNIT, 0x2, 0, ud_buf, &len);

	di_blkcnt = (
		((u64)ud_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT] << 56) |
		((u64)ud_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+1] << 48) |
		((u64)ud_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+2] << 40) |
		((u64)ud_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+3] << 32) |
		((u64)ud_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+4] << 24) |
		((u64)ud_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+5] << 16) |
		((u64)ud_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+6] << 8) |
		((u64)ud_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+7]));

	di_crc = vzalloc(di_blkcnt * sizeof(u16));
	if (!di_crc)
		return;

	di_priv = vzalloc(di_blkcnt * sizeof(u8));
	if (!di_priv) {
		vfree(di_crc);
		di_crc = NULL;
		return;
	}

	dev_info(hba->dev, "%s: need %llu MB memory for total lba %llu(0x%llx)\n",
		__func__, di_blkcnt * (sizeof(u16) + sizeof(u8)) / 1024 / 1024
		, di_blkcnt, di_blkcnt);

	di_init = 1;
}

static void ufs_mtk_di_reset(struct ufs_hba *hba)
{
	if (di_crc)
		memset(di_crc, 0, di_blkcnt * sizeof(u16));

	if (di_priv)
		memset(di_priv, 0, di_blkcnt * sizeof(u8));

	dev_info(hba->dev, "%s: Reset di %llu MB memory done!",
		__func__,
		di_blkcnt * (sizeof(u16) + sizeof(u8)) / 1024 / 1024);
}

/* Clear record for UNMAP command */
static int ufs_mtk_di_clr(struct scsi_cmnd *cmd)
{
	u32 lba, lba_cnt, i;

	/*
	 * Discard request has only one continuous sector range,
	 * so we can assume UNMAP command has only one UNMAP block
	 * descriptor.
	 */

	/* __sector: 512-byte based */
	lba = cmd->request->__sector >> 3;

	/* __data_len: bytes */
	lba_cnt = cmd->request->__data_len >> 12;

	/* clear both encrypted and non-encrypted records */
	for (i = 0; i < lba_cnt; i++) {
		di_crc[lba + i] =
		di_priv[lba + i] = 0;
	}

	return 0;
}

static int ufs_mtk_di_cmp_read(struct ufs_hba *hba,
			       u32 lba, int mode,
			       u16 crc, u8 priv)
{
	/*
	 * For read, update CRC16 value
	 * if corresponding CRC16[lba] is 0 (not set before),
	 * Otherwise, compare current calculated crc16
	 * value to CRC16[lba].
	 */
	if (mode == UFS_CRYPTO_HW_FDE) {
		if (di_crc[lba] == 0 || di_priv[lba] == 0) {
			di_crc[lba] = crc;
			di_priv[lba] = priv;
		} else {
			if (di_crc[lba] != crc) {
				dev_info(hba->dev,
				"%s: crc err! existed: 0x%x, read: 0x%x, lba: %u\n",
				__func__, di_crc[lba],
				crc, lba);
				WARN_ON(1);
				return -EIO;
			}
		}
	} else if (mode == UFS_CRYPTO_HW_FBE) {
		if (di_crc[lba] == 0 || di_priv[lba] == 0) {
			di_crc[lba] = crc;
			di_priv[lba] = priv;
		} else {
			if (priv != di_priv[lba]) {
				dev_info(hba->dev,
				"%s: key err (fde)! existed: 0x%x, read: 0x%x, lba: %u\n",
				__func__,
				di_priv[lba],
				priv, lba);
				return 0;
			} else if (crc != di_crc[lba]) {
				dev_info(hba->dev,
				 "%s: crc err (fbe)! existed: 0x%x, read: 0x%x, lba: %u\n",
				 __func__,
				 di_crc[lba],
				 crc, lba);
				return 0;
			}
		}
	} else {
		if (di_crc[lba] == 0 || di_priv[lba] != 0) {
			di_crc[lba] = crc;
			di_priv[lba] = priv;
		} else {
			if (di_crc[lba] != crc) {
				dev_info(hba->dev,
				"%s: crc err (ne)! existed: 0x%x, read: 0x%x, lba: %u\n",
				__func__,
				di_crc[lba],
				crc, lba);
				WARN_ON(1);
				return -EIO;
			}
		}
	}

	return 0;
}

static int ufs_mtk_di_cmp(struct ufs_hba *hba, struct scsi_cmnd *cmd)
{
	char *buffer;
	int i, len, err = 0;
	struct scatterlist *sg;
	u32 lba, blk_cnt, end_lba;
	u16 crc = 0;
	int mode = 0;
	u8 priv = 0;

	sg = scsi_sglist(cmd);

	lba = cmd->cmnd[5] | (cmd->cmnd[4] << 8) | (cmd->cmnd[3] << 16) |
		(cmd->cmnd[2] << 24);

	if ((u64)lba >= di_blkcnt) {
		dev_info(hba->dev,
			"%s: lba err! expected: di_blkcnt: 0x%llx, LBA: 0x%x\n",
			__func__, di_blkcnt, lba);
		WARN_ON(1);
		return -EIO;
	}

	/* HPB use READ_16, Transfer_len in cmd[15]*/
	if (cmd->cmnd[0] == READ_16) {
		if (hba->card->wmanufacturerid == UFS_VENDOR_SAMSUNG)
			blk_cnt = cmd->cmnd[15];
		else
			blk_cnt = cmd->cmnd[14];  /* JEDEC version */
#if defined(CONFIG_SCSI_SKHPB)
	} else if (cmd->cmnd[0] == SKHPB_READ) {  /* JEDEC version */
		blk_cnt = cmd->cmnd[14];
#endif
	} else {
		blk_cnt = cmd->cmnd[8] | (cmd->cmnd[7] << 8);
	}
	end_lba = lba + blk_cnt;

	/* Only data commands in LU2 need to check crc */
	if (ufshcd_scsi_to_upiu_lun(cmd->device->lun) != 0x2)
		return 0;

	if (!scsi_sg_count(cmd))
		return 0;

	if (hie_request_crypted(cmd->request)) {
		mode = UFS_CRYPTO_HW_FBE;
		priv = ufs_mtk_di_get_priv(cmd);
#ifdef CONFIG_MTK_HW_FDE
	} else if (cmd->request->bio && cmd->request->bio->bi_hw_fde) {
		mode = UFS_CRYPTO_HW_FDE;
		priv = 1;
#endif
	}

	for (i = 0; i < scsi_sg_count(cmd); i++) {
		buffer = (char *)sg_virt(sg);
		for (len = 0; len < sg->length; len = len + 0x1000, lba++) {
			/*
			 * Use value 0 as empty slot in crc array
			 *
			 * if calculated crc value is 0, use crc + 1
			 * instead to avoid conflict
			 */
			crc = crc16(0x0, &buffer[len], DI_CRC_DATA_SIZE);
			if (crc == 0)
				crc++;

			if (ufs_mtk_is_data_write_cmd(cmd->cmnd[0])) {
				/* For write, update crc value */
				di_crc[lba] = crc;
				di_priv[lba] = priv;
			} else {
				err = ufs_mtk_di_cmp_read(hba,
					lba, mode, crc, priv);
				if (err)
					return err;
			}
		}
		sg = sg_next(sg);
	}
	/*
	 * Check lba # traverse from
	 * scatter is the same as end_lba
	 */
	if (end_lba != lba) {
		dev_info(hba->dev,
		"expect end_lba is 0x%x, but 0x%x, cmd=0x%x, blk_cnt=0x%x\n",
		 end_lba, lba, cmd->cmnd[0], blk_cnt);
	}

	return 0;
}

int ufs_mtk_di_inspect(struct ufs_hba *hba, struct scsi_cmnd *cmd)
{
	if (!di_crc)
		return 0;

	/* do inspection in LU2 (user LU) only */
	if (ufshcd_scsi_to_upiu_lun(cmd->device->lun) != 0x2)
		return -ENODEV;

	if (ufs_mtk_is_data_cmd(cmd->cmnd[0]))
		return ufs_mtk_di_cmp(hba, cmd);

	if (ufs_mtk_is_unmap_cmd(cmd->cmnd[0]))
		return ufs_mtk_di_clr(cmd);

	return -ENODEV;
}
#endif

static int ufs_mtk_query_desc(struct ufs_hba *hba, enum query_opcode opcode,
			enum desc_idn idn, u8 index, void *desc, int len)
{
	return ufshcd_query_descriptor_retry(hba,
		opcode, idn, index, 0, desc, &len);
}

#ifdef CONFIG_HIE

static struct kh_dev ufs_mtk_kh;

struct kh_dev *ufs_mtk_get_kh(void)
{
	return &ufs_mtk_kh;
}

#if defined(HIE_CHANGE_KEY_IN_NORMAL_WORLD)
static int ufs_crypto_hie_get_cap(struct ufs_hba *hba, unsigned int hie_cap)
{
	dev_dbg(hba->dev, "hie_cap: 0x%x\n", hie_cap);

	if (hie_cap & BC_AES_128_XTS)
		return 0;
	else if (hie_cap & BC_AES_256_XTS)
		return 1;
	else
		return -1;
}
#endif

/* configure request for HIE */
static int ufs_mtk_hie_cfg_request(unsigned int mode,
	const char *key, int len, struct request *req, void *priv)
{
	struct ufs_crypt_info *info = (struct ufs_crypt_info *)priv;
	struct ufshcd_lrb *lrbp;
	struct ufs_hba *hba = info->hba;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 i;
	u32 *key_ptr;
	unsigned long flags;
	u64 iv, lba;
	u32 dunl, dunu;
	struct scsi_cmnd *cmd;
	int need_update = 1;
	int key_idx;
#if defined(HIE_CHANGE_KEY_IN_NORMAL_WORLD)
	union ufs_cpt_cap cpt_cap;
	union ufs_cap_cfg cpt_cfg;
	u32 addr;
#else
	u32 hie_para;
#endif

	spin_lock_irqsave(info->hba->host->host_lock, flags);

	if (!(hba->crypto_feature & UFS_CRYPTO_HW_FBE_ENCRYPTED)) {
		mt_secure_call(MTK_SIP_KERNEL_CRYPTO_HIE_INIT, 0, 0, 0, 0);
		hba->crypto_feature |= UFS_CRYPTO_HW_FBE_ENCRYPTED;
	}

	if (host->passthrough_keyhint) {
		/* always use cfg slot with slot id the same as task tag */
		key_idx = req->tag;
		need_update = 1;
	} else {
		/*
		 * get key index from key hint, or install new
		 * key to key hint
		 */
		key_idx = kh_get_hint(ufs_mtk_get_kh(), key, &need_update);
	}

	/*
	 * Return error if free key slots are unavailable (-ENOMEM).
	 *
	 * Bypass error by unsuccesful keyhint registration (-ENODEV)
	 * because ufs host driver shall work fine in this case.
	 */
	if ((key_idx < 0) && (key_idx != -ENODEV))
		return key_idx;

	spin_unlock_irqrestore(info->hba->host->host_lock, flags);

	if (need_update || (key_idx < 0)) {

		/*
		 * Shall happen only if keyhint is not registered
		 * successfully. Use tag as key index directly and
		 * always update key.
		 *
		 * Note. In this case, queue depth shall align to
		 * number of key slots.
		 */
		if (key_idx < 0)
			key_idx = req->tag;

		key_ptr = (u32 *)key;

#if defined(HIE_CHANGE_KEY_IN_NORMAL_WORLD)
		/* init crypto cfg */
		memset(&cpt_cfg, 0, sizeof(cpt_cfg));

		/* init key */
		len = len >> 2;
		if (len > 16) {
			dev_info(info->hba->dev,
				"Key size is over %d bits\n", len * 32);
			len = 16;
		}

		for (i = 0; i < len; i++)
			cpt_cfg.cfgx.key[i] = key_ptr[i];

		/* enable this cfg */
		cpt_cfg.cfgx.cfg_en = 1;

		/* init capability id */
		mode = ufs_crypto_hie_get_cap(info->hba, mode);
		cpt_cfg.cfgx.cap_id = (u8) mode;

		/* init data unit size: fixed as 4 KB (512 * 2^3) for UFS */
		cpt_cfg.cfgx.du_size = (1 << 3);

		/*
		 * Get address of cfg[cfg_id], this is also
		 * address of key in cfg[cfg_id].
		 */
		cpt_cap.cap_raw = ufshcd_readl(info->hba,
			UFS_REG_CRYPTO_CAPABILITY);
		addr = (cpt_cap.cap.cfg_ptr << 8) + (u32)(key_idx << 7);

		spin_lock_irqsave(info->hba->host->host_lock, flags);

		/* write configuration only to register */
		for (i = 0; i < 18; i++) {
			ufshcd_writel(info->hba, cpt_cfg.cfgx_raw[i],
				(addr + i * 4));
			dev_dbg(info->hba->dev, "[%d] 0x%x=0x%x\n",
				key_idx,
				(addr + i * 4), cpt_cfg.cfgx_raw[i]);
		}

		spin_unlock_irqrestore(info->hba->host->host_lock, flags);
#else
		hie_para = ((key_idx & 0xFF) << UFS_HIE_PARAM_OFS_CFG_ID) |
			((mode & 0xFF) << UFS_HIE_PARAM_OFS_MODE) |
			((len & 0xFF) << UFS_HIE_PARAM_OFS_KEY_TOTAL_BYTE);

		spin_lock_irqsave(info->hba->host->host_lock, flags);

		/* init ufs crypto IP for HIE and program key by first 8B */
		mt_secure_call(MTK_SIP_KERNEL_CRYPTO_HIE_CFG_REQUEST,
			hie_para, key_ptr[0], key_ptr[1], 0);

		/* program remaining key */
		for (i = 2; i < len / sizeof(u32); i += 3) {
			mt_secure_call(MTK_SIP_KERNEL_CRYPTO_HIE_CFG_REQUEST,
				key_ptr[i], key_ptr[i + 1], key_ptr[i + 2], 0);
		}

		spin_unlock_irqrestore(info->hba->host->host_lock, flags);
#endif
	}

	cmd = info->cmd;

	lba = blk_rq_pos(req) >> 3;

	/* Get iv from hie */
	iv = hie_get_iv(req);

	/* If hie not assign iv, then use lba as iv */
	if (!iv)
		iv = lba;

	ufs_mtk_crypto_cal_dun(UFS_CRYPTO_ALGO_AES_XTS, iv, &dunl, &dunu);

	/* setup LRB for UTPRD's crypto fields */
	lrbp = &info->hba->lrb[req->tag];
	lrbp->crypto_en = 1;
	lrbp->crypto_cfgid = key_idx;

	/* remember to give "tweak" for AES-XTS */
	lrbp->crypto_dunl = dunl;
	lrbp->crypto_dunu = dunu;

	return 0;
}

struct hie_dev ufs_hie_dev = {
	.name = "ufs",
	.mode = (BC_AES_256_XTS | BC_AES_128_XTS),
	.encrypt = ufs_mtk_hie_cfg_request,
	.decrypt = ufs_mtk_hie_cfg_request,
	.priv = NULL,
};

struct hie_dev *ufs_mtk_hie_get_dev(void)
{
	return &ufs_hie_dev;
}

int ufs_mtk_hie_req_done(struct ufs_hba *hba,
	struct ufshcd_lrb *lrbp)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	if (lrbp->crypto_en && !host->passthrough_keyhint) {
		ret = kh_release_hint(
			ufs_mtk_get_kh(), lrbp->crypto_cfgid);
	}

	return ret;
}
#endif

static int ufs_mtk_send_uic_command(struct ufs_hba *hba, u32 cmd,
	u32 arg1, u32 arg2, u32 *arg3, u8 *err_code)
{
	int result;
	struct uic_command uic_cmd = {
		.command = cmd,
		.argument1 = arg1,
		.argument2 = arg2,
		.argument3 = *arg3,
	};

	result = ufshcd_send_uic_cmd(hba, &uic_cmd);

	if (err_code)
		*err_code = uic_cmd.argument2 & MASK_UIC_COMMAND_RESULT;

	if (result) {
		dev_err(hba->dev, "UIC command error: %#x\n", result);
		return -EIO;
	}
	if (cmd == UIC_CMD_DME_GET || cmd == UIC_CMD_DME_PEER_GET)
		*arg3 = uic_cmd.argument3;

	return 0;
}

int ufs_mtk_run_batch_uic_cmd(struct ufs_hba *hba,
	struct uic_command *cmds, int ncmds)
{
	int i;
	int err = 0;

	for (i = 0; i < ncmds; i++) {

		err = ufshcd_send_uic_cmd(hba, &cmds[i]);

		if (err) {
			dev_err(hba->dev, "%s fail, cmd: %x, arg1: %x\n",
				__func__, cmds->command, cmds->argument1);
			/* return err; */
		}
	}

	return err;
}

int ufs_mtk_cfg_unipro_cg(struct ufs_hba *hba, bool enable)
{
	u32 tmp = 0;

	if (enable) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), &tmp);
		tmp = tmp | (1 << RX_SYMBOL_CLK_GATE_EN) |
		      (1 << SYS_CLK_GATE_EN) |
		      (1 << TX_CLK_GATE_EN);
		ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp & ~(1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_DEBUGCLOCKENABLE), tmp);
	} else {
		ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), &tmp);
		tmp = tmp & ~((1 << RX_SYMBOL_CLK_GATE_EN) |
			      (1 << SYS_CLK_GATE_EN) |
			      (1 << TX_CLK_GATE_EN));
		ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp | (1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_DEBUGCLOCKENABLE), tmp);
	}

	return 0;
}

/**
 * ufs_mtk_advertise_quirks - advertise the known mtk UFS controller quirks
 * @hba: host controller instance
 *
 * mtk UFS host controller might have some non standard behaviours (quirks)
 * than what is specified by UFSHCI specification. Advertise all such
 * quirks to standard UFS host controller driver so standard takes them into
 * account.
 */
static void ufs_mtk_advertise_hci_quirks(struct ufs_hba *hba)
{
#if defined(CONFIG_MTK_HW_FDE)
#if defined(UFS_MTK_PLATFORM_UFS_HCI_PERF_HEURISTIC)
	hba->quirks |= UFSHCD_QUIRK_UFS_HCI_PERF_HEURISTIC;
#endif
#endif

#if defined(UFS_MTK_PLATFORM_UFS_HCI_RST_DEV_FOR_LINKUP_FAIL)
	hba->quirks |= UFSHCD_QUIRK_UFS_HCI_DEV_RST_FOR_LINKUP_FAIL;
#endif

#if defined(UFS_MTK_PLATFORM_UFS_HCI_VENDOR_HOST_RST)
	hba->quirks |= UFSHCD_QUIRK_UFS_HCI_VENDOR_HOST_RST;
#endif

#if defined(UFS_MTK_PLATFORM_UFS_HCI_MANUALLY_DISABLE_AH8_BEFORE_RING_DOORBELL)
	hba->quirks |= UFSHCD_QUIRK_UFS_HCI_DISABLE_AH8_BEFORE_RDB;
#endif

	dev_info(hba->dev, "hci quirks: %#x\n", hba->quirks);
}

#ifdef CONFIG_MTK_HW_FDE
/**
 * ufs_mtk_hwfde_cfg_cmd - configure command for hw fde
 * @hba: host controller instance
 * @cmd: scsi command instance
 *
 * HW FDE key may be changed by updating master key by end-user's behavior.
 * Update new key to crypto IP if necessary.
 *
 * Host lock must be held during key update in atf.
 */
void ufs_mtk_hwfde_cfg_cmd(struct ufs_hba *hba, struct scsi_cmnd *cmd)
{
	u64 lba;
	u32 dunl, dunu;
	unsigned long flags;
	int hwfde_key_idx_old;
	struct ufshcd_lrb *lrbp;

	lrbp = &hba->lrb[cmd->request->tag];

	/* for r/w request only */
	if (cmd->request->bio && cmd->request->bio->bi_hw_fde) {

		/* in case hw fde is enabled and key index is updated by
		 * dm-crypt
		 */
		if (cmd->request->bio->bi_key_idx !=
			hba->crypto_hwfde_key_idx) {

			/* acquire host lock to ensure atomicity during key
			 * change in atf
			 */
			spin_lock_irqsave(hba->host->host_lock, flags);

			/* do key change */
			mt_secure_call(MTK_SIP_KERNEL_HW_FDE_UFS_CTL, (1 << 3),
				0, 0, 0);

			hwfde_key_idx_old = hba->crypto_hwfde_key_idx;
			hba->crypto_hwfde_key_idx =
				cmd->request->bio->bi_key_idx;

			spin_unlock_irqrestore(hba->host->host_lock, flags);

			dev_info(hba->dev, "update hw-fde key, ver %d->%d\n",
				hwfde_key_idx_old,
				hba->crypto_hwfde_key_idx);
		}

		lba = blk_rq_pos(cmd->request) >> 3;

		ufs_mtk_crypto_cal_dun(UFS_CRYPTO_ALGO_ESSIV_AES_CBC,
			lba, &dunl, &dunu);

		lrbp->crypto_cfgid = 0;
		lrbp->crypto_dunl = dunl;
		lrbp->crypto_dunu = dunu;
		lrbp->crypto_en = 1;

		/* mark data has ever gone through encryption/decryption path */
		hba->crypto_feature |= UFS_CRYPTO_HW_FDE_ENCRYPTED;
	}
}

#else
void ufs_mtk_hwfde_cfg_cmd(struct ufs_hba *hba, struct scsi_cmnd *cmd)
{
};
#endif

static enum ufs_pm_level
ufs_mtk_get_desired_pm_lvl_for_dev_link_state(enum ufs_dev_pwr_mode dev_state,
					enum uic_link_state link_state)
{
	enum ufs_pm_level lvl;

	for (lvl = UFS_PM_LVL_0; lvl < UFS_PM_LVL_MAX; lvl++) {
		if ((ufs_pm_lvl_states[lvl].dev_state == dev_state) &&
			(ufs_pm_lvl_states[lvl].link_state == link_state))
			return lvl;
	}

	/* if no match found, return the level 0 */
	return UFS_PM_LVL_0;
}

static bool ufs_mtk_is_valid_pm_lvl(int lvl)
{
	if (lvl >= 0 && lvl < ufs_pm_lvl_states_size)
		return true;
	else
		return false;
}

static int ufs_mtk_host_clk_get(struct device *dev, const char *name,
				struct clk **clk_out)
{
	struct clk *clk;
	int err = 0;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk))
		err = PTR_ERR(clk);
	else
		*clk_out = clk;

	return err;
}

bool ufs_mtk_perf_is_supported(struct ufs_mtk_host *host)
{
	if (!host->crypto_clk_mux ||
	    !host->crypto_parent_clk_normal ||
	    !host->crypto_parent_clk_perf ||
	    !host->req_vcore ||
	    host->crypto_vcore_opp < 0)
		return false;
	else
		return true;
}

int ufs_mtk_perf_setup_req(struct ufs_mtk_host *host, bool perf)
{
	int err = 0;

	err = clk_prepare_enable(host->crypto_clk_mux);
	if (err) {
		dev_info(host->hba->dev, "%s: clk_prepare_enable(): %d\n",
			 __func__, err);
		goto out;
	}

	if (perf) {
		pm_qos_update_request(host->req_vcore,
				      host->crypto_vcore_opp);
		err = clk_set_parent(host->crypto_clk_mux,
				     host->crypto_parent_clk_perf);
	} else {
		err = clk_set_parent(host->crypto_clk_mux,
				     host->crypto_parent_clk_normal);
		pm_qos_update_request(host->req_vcore,
				      PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	}

	if (err)
		dev_info(host->hba->dev, "%s: clk_set_parent(): %d\n",
			 __func__, err);

	clk_disable_unprepare(host->crypto_clk_mux);

out:
	ufs_mtk_dbg_add_trace(host->hba, UFS_TRACE_PERF_MODE,
		perf, 0, (u32)err,
		0, 0, 0, 0, 0, 0);

	return err;
}

int ufs_mtk_perf_setup_crypto_clk(struct ufs_mtk_host *host, bool perf)
{
	int err = 0;
	bool rpm_resumed = false;
	bool clk_prepared = false;

	if (!ufs_mtk_perf_is_supported(host)) {
		dev_info(host->hba->dev, "%s: perf mode is unsupported\n",
			 __func__);
		err = -ENOTSUPP;
		goto out;
	}

	/* runtime resume shall be prior to blocking requests */
	pm_runtime_get_sync(host->hba->dev);
	rpm_resumed = true;

	/*
	 * reuse clk scaling preparation function to wait until all
	 * on-going commands are done, and then block future commands
	 */
	err = ufshcd_clock_scaling_prepare(host->hba);
	if (err) {
		dev_info(host->hba->dev,
			 "%s: ufshcd_clock_scaling_prepare(): %d\n",
			 __func__, err);
		goto out;
	}

	clk_prepared = true;

	err = ufs_mtk_perf_setup_req(host, perf);
out:
	/*
	 * add event before any possible incoming commands
	 * by unblocking requests in ufshcd_clock_scaling_unprepare()
	 */
	dev_info(host->hba->dev, "perf mode: request %s %s\n",
		 perf ? "on" : "off",
		 err ? "failed" : "ok");

	if (clk_prepared)
		ufshcd_clock_scaling_unprepare(host->hba);

	if (rpm_resumed)
		pm_runtime_put_sync(host->hba->dev);

	if (!err)
		host->perf_en = perf;

	return err;
}

int ufs_mtk_perf_setup(struct ufs_mtk_host *host,
	 bool perf)
{
	int err = 0;

	if (!ufs_mtk_perf_is_supported(host) ||
		(host->perf_mode != PERF_AUTO)) {
		/* return without error */
		return 0;
	}

	err = ufs_mtk_perf_setup_req(host, perf);
	if (!err)
		host->perf_en = perf;
	else
		dev_info(host->hba->dev, "%s: %s fail %d\n",
		 __func__, perf ? "en":"dis", err);

	return err;
}

static int ufs_mtk_perf_init_crypto(struct ufs_hba *hba)
{
	int err = 0;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct device_node *np = hba->dev->of_node;

	err = ufs_mtk_host_clk_get(hba->dev,
				   "ufs-vendor-crypto-clk-mux",
				   &host->crypto_clk_mux);
	if (err) {
		dev_info(hba->dev,
			"%s: failed to get ufs-vendor-crypto-clk-mux, err: %d",
			__func__, err);
		goto out;
	}

	err = ufs_mtk_host_clk_get(hba->dev,
				   "ufs-vendor-crypto-normal-parent-clk",
				   &host->crypto_parent_clk_normal);
	if (err) {
		dev_info(hba->dev,
			"%s: failed to get ufs-vendor-crypto-normal-parent-clk, err: %d",
			__func__, err);
		goto out;
	}

	err = ufs_mtk_host_clk_get(hba->dev,
				   "ufs-vendor-crypto-perf-parent-clk",
				   &host->crypto_parent_clk_perf);
	if (err) {
		dev_info(hba->dev,
			"%s: failed to get ufs-vendor-crypto-perf-parent-clk, err: %d",
			__func__, err);
		goto out;
	}

	err = of_property_read_s32(np, "mediatek,perf-crypto-vcore",
				   &host->crypto_vcore_opp);
	if (err) {
		dev_info(hba->dev,
			"%s: failed to get mediatek,perf-crypto-vcore",
			__func__);
		host->crypto_vcore_opp = -1;
		goto out;
	}

	/* init VCORE QOS */
	host->req_vcore = devm_kzalloc(hba->dev, sizeof(*host->req_vcore),
				       GFP_KERNEL);
	if (!host->req_vcore) {
		err = -ENOMEM;
		goto out;
	}

	pm_qos_add_request(host->req_vcore, PM_QOS_VCORE_OPP,
			   PM_QOS_VCORE_OPP_DEFAULT_VALUE);
out:
#ifdef CONFIG_UFSTW
	if (!err)
		host->perf_mode = PERF_AUTO;
	else
#endif
		host->perf_mode = PERF_FORCE_DISABLE;

	return err;
}

static int ufs_mtk_setup_clocks(struct ufs_hba *hba, bool on,
	enum ufs_notify_change_status stage)
{
	int ret = 0;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	switch (stage) {
	case PRE_CHANGE:
		if (!on) {
			ret = ufs_mtk_pltfrm_ref_clk_ctrl(hba, false);
			if (host) {
				pm_qos_update_request(
					&host->req_cpu_dma_latency,
					PM_QOS_DEFAULT_VALUE);
			}
		}
		break;
	case POST_CHANGE:
		if (on) {
			ret = ufs_mtk_pltfrm_ref_clk_ctrl(hba, true);
			if (host) {
				pm_qos_update_request(
					&host->req_cpu_dma_latency, 0);
			}
		}
		break;
	default:
		break;
	}

	return ret;
}

static void ufs_mtk_set_caps(struct ufs_hba *hba)
{
	hba->caps |= UFSHCD_CAP_AUTO_BKOPS_SUSPEND;
}

/**
 * ufs_mtk_init - find other essential mmio bases
 * @hba: host controller instance
 */
static int ufs_mtk_init(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host;
	int err = 0;

	host = devm_kzalloc(hba->dev, sizeof(*host), GFP_KERNEL);
	if (!host) {
		err = -ENOMEM;
		dev_info(hba->dev,
			 "%s: no memory for mtk ufs host\n", __func__);
		goto out;
	}

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	/* initialize globals */
	ufs_mtk_rpm_autosuspend_delay = -1;
	ufs_mtk_rpm_enabled = false;
	ufs_mtk_auto_hibern8_enabled = false;
	ufs_mtk_host_deep_stall_enable = 0;
	ufs_mtk_host_scramble_enable = 0;
	ufs_mtk_hba = hba;
	hba->crypto_hwfde_key_idx = -1;

	/*
	 * Rename device to unify device path for booting storage device.
	 *
	 * Device rename shall be prior to any pinctrl operation to avoid
	 * known kernel panic issue which can be triggered by dumping pin
	 * information, for example,
	 *
	 * "cat /sys/kernel/debug/pinctrl/10005000.pinctrl/pinmux-pins".
	 *
	 * The panic is because create_pinctrl() will keep the original
	 * device name string instance in kobject. However, old name string
	 * instance will be freed during device_rename() but NOT awared by
	 * pinctrl.
	 *
	 * Please also remove default pin state in device tree and related
	 * code because create_pinctrl() will be activated before device
	 * probing if default pin state is declared.
	 */
	device_rename(hba->dev, "bootdevice");

	ufs_mtk_pltfrm_init();

	ufs_mtk_pltfrm_parse_dt(hba);

	ufs_mtk_set_caps(hba);
	ufs_mtk_advertise_hci_quirks(hba);

	ufs_mtk_parse_dt(hba);

	/*
	 * If rpm_lvl and and spm_lvl are not already set to valid levels,
	 * set the default power management level for UFS runtime and system
	 * suspend. Default power saving mode selected is keeping UFS link in
	 * Hibern8 state and UFS device in sleep.
	 */
	if (!ufs_mtk_is_valid_pm_lvl(hba->rpm_lvl))
		hba->rpm_lvl = ufs_mtk_get_desired_pm_lvl_for_dev_link_state(
							UFS_SLEEP_PWR_MODE,
							UIC_LINK_HIBERN8_STATE);
	if (!ufs_mtk_is_valid_pm_lvl(hba->spm_lvl))
		hba->spm_lvl = ufs_mtk_get_desired_pm_lvl_for_dev_link_state(
							UFS_SLEEP_PWR_MODE,
							UIC_LINK_HIBERN8_STATE);

	/* Get auto-hibern8 timeout from device tree */
	ufs_mtk_parse_auto_hibern8_timer(hba);

	ufs_mtk_perf_init_crypto(hba);
out:
	return err;
}

static int ufs_mtk_pre_pwr_change(struct ufs_hba *hba,
			      struct ufs_pa_layer_attr *desired,
			      struct ufs_pa_layer_attr *final)
{
	int err = 0;

	struct ufs_descriptor desc;

	/* get manu ID for vendor specific configuration in the future */
	if (hba->manu_id == 0) {

		/* read device descriptor */
		desc.descriptor_idn = 0;
		desc.index = 0;

		err = ufs_mtk_query_desc(hba,
			UPIU_QUERY_FUNC_STANDARD_READ_REQUEST,
			desc.descriptor_idn, desc.index,
			desc.descriptor, sizeof(desc.descriptor));
		if (err)
			return err;

		/* get wManufacturerID */
		hba->manu_id = desc.descriptor[0x18] << 8 |
			desc.descriptor[0x19];
		dev_dbg(hba->dev, "wManufacturerID: 0x%x\n", hba->manu_id);
	}

/* HSG3B as default power mode, only use HSG1B at FPGA */
#ifndef CONFIG_FPGA_EARLY_PORTING
	final->gear_rx = 3;
	final->gear_tx = 3;
#else
	final->gear_rx = 1;
	final->gear_tx = 1;
#endif
	/* Change by dts setting */
	if (hba->lanes_per_direction == 2) {
		final->lane_rx = 2;
		final->lane_tx = 2;
	} else {
		final->lane_rx = 1;
		final->lane_tx = 1;
	}
	final->hs_rate = PA_HS_MODE_B;
	final->pwr_rx = FAST_MODE;
	final->pwr_tx = FAST_MODE;

	ufs_mtk_pltfrm_pwr_change_final_gear(hba, final);

	/* Set PAPowerModeUserData[0~5] = 0xffff, default is 0 */
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA0), 0x1fff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA1), 0xffff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA2), 0x7fff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA3), 0x1fff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA4), 0xffff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA5), 0x7fff);

	return err;
}

static int ufs_mtk_pwr_change_notify(struct ufs_hba *hba,
			      enum ufs_notify_change_status stage,
			      struct ufs_pa_layer_attr *desired,
			      struct ufs_pa_layer_attr *final)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_pwr_change(hba, desired, final);
		break;
	case POST_CHANGE:
		break;
	default:
		break;
	}

	return ret;
}

static int ufs_mtk_init_mphy(struct ufs_hba *hba)
{
	return 0;
}

static int ufs_mtk_enable_crypto(struct ufs_hba *hba)
{
	/* restore vendor crypto setting by re-using resume operation */
	mt_secure_call(MTK_SIP_KERNEL_HW_FDE_UFS_CTL, (1 << 2), 0, 0, 0);

	return 0;
}

static int ufs_mtk_probe_crypto(struct ufs_hba *hba)
{
#ifdef CONFIG_HIE
	int ret;
	union ufs_cpt_cap cpt_cap;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ret = hie_register_device(&ufs_hie_dev);
	if (ret)
		return ret;

	cpt_cap.cap_raw = ufshcd_readl(hba, UFS_REG_CRYPTO_CAPABILITY);
	if (hba->nutrs <= cpt_cap.cap.cfg_cnt + 1) {
		host->passthrough_keyhint = true;
	} else {
		/*
		 * enable hie key hint feature
		 *
		 * key_bits = 512 bits for all possible FBE crypto algorithms
		 * key_slot = crypto configuration slots
		 */
		ret = kh_register(ufs_mtk_get_kh(), 512,
			cpt_cap.cap.cfg_cnt + 1);
		if (ret)
			return ret;
		host->passthrough_keyhint = false;
	}
	dev_info(hba->dev, "keyhint enabled: %d\n",
		!host->passthrough_keyhint);

	hba->crypto_feature |= UFS_CRYPTO_HW_FBE;
#endif

#if defined(CONFIG_MTK_HW_FDE)
	hba->crypto_feature |= UFS_CRYPTO_HW_FDE;
#endif

	return 0;
}

static int ufs_mtk_reset_device(struct ufs_hba *hba)
{
	dev_info(hba->dev, "reset device\n");

	/* do device hw reset */
	mt_secure_call(MTK_SIP_KERNEL_HW_FDE_UFS_CTL, (1 << 5), 0, 0, 0);

	return 0;
}

int ufs_mtk_linkup_fail_handler(struct ufs_hba *hba, int left_retry)
{
	if (!(hba->quirks & UFSHCD_QUIRK_UFS_HCI_DEV_RST_FOR_LINKUP_FAIL))
		return 0;

	if (left_retry <= 1)
		ufs_mtk_reset_device(hba);

	return 0;
}

int ufs_mtk_check_powerctl(struct ufs_hba *hba)
{
	int err = 0;
	u32 val = 0;

	/* check if host in power saving */
	err = ufshcd_dme_get(hba,
		UIC_ARG_MIB(VENDOR_UNIPROPOWERDOWNCONTROL), &val);
	if (!err && val == 0x1) {
		err = ufshcd_dme_set(hba,
			UIC_ARG_MIB(VENDOR_UNIPROPOWERDOWNCONTROL), 0);
		dev_info(hba->dev, "get dme 0x%x = %d, set 0 (%d)\n",
			VENDOR_UNIPROPOWERDOWNCONTROL, val, err);
	}

	return err;
}

static int ufs_mtk_hce_enable_notify(struct ufs_hba *hba,
	enum ufs_notify_change_status stage)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		break;
	case POST_CHANGE:
		ret = ufs_mtk_enable_crypto(hba);
		/*
		 * After HCE enable, need disable xoufs_req_s in ufshci
		 * when xoufs hw solution is not ready.
		 */
#ifndef UFS_REF_CLK_CTRL_BY_UFSHCI
		/*
		 * Old chip not use this, disable it always after HCE enable
		 * to deactivate UFS_SRCCLKENA/UFS_INFRA_REQ/UFS_VRF18_REQ
		 * after ufs_mtk_pltfrm_resume.
		 */
		ufshcd_writel(hba, 0, REG_UFS_ADDR_XOUFS_ST);
#endif
		break;
	default:
		break;
	}

	return ret;
}

static int ufs_mtk_pre_link(struct ufs_hba *hba)
{
	int ret = 0;
	u32 tmp;

	ufs_mtk_pltfrm_bootrom_deputy(hba);

	ufs_mtk_init_mphy(hba);

	/* ensure auto-hibern8 is disabled during hba probing */
	ufshcd_vops_auto_hibern8(hba, false);

	/* powerup unipro if unipro powerdown */
	ret = ufs_mtk_check_powerctl(hba);
	if (ret)
		return ret;

	/* configure deep stall */
	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), &tmp);
	if (ret)
		return ret;

	if (ufs_mtk_host_deep_stall_enable)   /* enable deep stall */
		tmp |= (1 << 6);
	else
		tmp &= ~(1 << 6);   /* disable deep stall */

	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), tmp);
	if (ret)
		return ret;

	/* configure scrambling */
	if (ufs_mtk_host_scramble_enable)
		ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_SCRAMBLING), 1);
	else
		ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_SCRAMBLING), 0);

	return ret;
}

static int ufs_mtk_post_link(struct ufs_hba *hba)
{
	int ret = 0;
	u32 arg = 0;
	u32 ah_ms;

	/* disable device LCC */
	ret = ufs_mtk_send_uic_command(hba, UIC_CMD_DME_SET,
		UIC_ARG_MIB(PA_LOCALTXLCCENABLE), 0, &arg, NULL);

	if (ret) {
		dev_err(hba->dev, "dme_setting_after_link fail\n");
		ret = 0;	/* skip error */
	}

	/* enable unipro clock gating feature */
	ufs_mtk_cfg_unipro_cg(hba, true);

	/* configure clk gating delay */
	if (ufshcd_is_clkgating_allowed(hba)) {
		if (ufshcd_is_auto_hibern8_supported(hba) && hba->ahit)
			ah_ms = FIELD_GET(UFSHCI_AHIBERN8_TIMER_MASK,
					  hba->ahit);
		else
			ah_ms = 10;
		hba->clk_gating.delay_ms = ah_ms + 5;
	}

	return ret;

}

static int ufs_mtk_link_startup_notify(struct ufs_hba *hba,
	enum ufs_notify_change_status stage)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_link(hba);
		break;
	case POST_CHANGE:
		ret = ufs_mtk_post_link(hba);
		break;
	default:
		break;
	}

	return ret;
}

static int ufs_mtk_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	if (ufshcd_is_link_hibern8(hba)) {
		/*
		 * HCI power-down flow with link in hibern8
		 * Enter vendor-specific power down mode to keep UniPro state
		 */
		ret = ufshcd_dme_set(hba,
			UIC_ARG_MIB_SEL(VENDOR_UNIPROPOWERDOWNCONTROL, 0), 1);
		if (ret) {
			/* dump ufs debug Info like XO_UFS/VEMC/VUFS18 */
			ufs_mtk_pltfrm_gpio_trigger_and_debugInfo_dump(hba);

			/*
			 * Power down fail leave vendor-specific power down mode
			 * to resume UniPro state
			 */
			(void)ufshcd_dme_set(hba,
				UIC_ARG_MIB_SEL(VENDOR_UNIPROPOWERDOWNCONTROL,
				0), 0);
			ret = -EAGAIN;

			return ret;
		}

		ufs_mtk_pltfrm_suspend(hba);

		/* vendor-specific crypto suspend */
		mt_secure_call(MTK_SIP_KERNEL_HW_FDE_UFS_CTL, (1 << 1),
			0, 0, 0);
#ifdef CONFIG_HIE
		if (!host->passthrough_keyhint)
			kh_suspend(ufs_mtk_get_kh());
#endif
	}

	return ret;
}

static void ufs_mtk_dbg_register_dump(struct ufs_hba *hba)
{
	u32 val;

	/* read debugging register REG_UFS_MTK_PROBE */

	/*
	 * configure REG_UFS_MTK_DEBUG_SEL to direct debugging information
	 * to REG_UFS_MTK_PROBE.
	 *
	 * REG_UFS_MTK_DEBUG_SEL is not required to set back
	 * as 0x0 (default value) after dump.
	 */
	ufshcd_writel(hba, 0x20, REG_UFS_MTK_DEBUG_SEL);

	val = ufshcd_readl(hba, REG_UFS_MTK_PROBE);

	dev_info(hba->dev, "REG_UFS_MTK_PROBE: 0x%x\n", val);
}

static int ufs_mtk_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int ret = 0;

	if (ufshcd_is_link_hibern8(hba)) {

		ufs_mtk_pltfrm_resume(hba);

		/*
		 * HCI power-on flow with link in hibern8
		 *
		 * Enable UFSHCI
		 * NOTE: interrupt mask UFSHCD_UIC_MASK
		 *       (UIC_COMMAND_COMPL | UFSHCD_UIC_PWR_MASK)
		 *       will be enabled inside.
		 */
		ret = ufshcd_hba_enable(hba);
		if (ret) {
			dev_err(hba->dev, "%s: hba_enable failed. ret = %d\n",
				__func__, ret);
			return ret;
		}

		/* Leave vendor-specific power down mode to resume
		 * UniPro state
		 */
		ret = ufshcd_dme_set(hba,
			UIC_ARG_MIB_SEL(VENDOR_UNIPROPOWERDOWNCONTROL, 0), 0);
		if (ret) {
			dev_err(hba->dev, "%s: UniProPowerDownControl failed. ret = %d\n",
				__func__, ret);
			return ret;
		}

		/*
		 * Leave hibern8 state
		 * NOTE: ufshcd_make_hba_operational will be ok only if link
		 * is in active state.
		 *
		 *       The reason is: ufshcd_make_hba_operational
		 *       will check if REG_CONTROLLER_STATUS
		 *       is OK. In REG_CONTROLLER_STATUS, UTMRLRDY
		 *       and UTRLRDY will be ready only if
		 *       DP is set. DP will be set only if link
		 *       is in active state in MTK design.
		 */
		ret = ufshcd_uic_hibern8_exit(hba);
		if (!ret)
			ufshcd_set_link_active(hba);

		/* Re-start hba */
		ret = ufshcd_make_hba_operational(hba);
		if (ret) {
			dev_err(hba->dev, "%s: make_hba_operational failed. ret = %d\n",
				__func__, ret);
			return ret;
		}

		/* vendor-specific crypto resume */
		mt_secure_call(MTK_SIP_KERNEL_HW_FDE_UFS_CTL, (1 << 2),
			0, 0, 0);
	}

	return ret;
}

void ufs_mtk_parse_dt(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 val;

	if (np) {
		/* system PM */
		if (of_property_read_u32(np, "mediatek,spm-level",
			&hba->spm_lvl))
			hba->spm_lvl = -1;

		/* runtime PM */
		if (of_property_read_u32(np, "mediatek,rpm-level",
			&hba->rpm_lvl))
			hba->rpm_lvl = -1;

		if (!of_property_read_u32(np, "mediatek,rpm-enable", &val)) {
			if (val)
				ufs_mtk_rpm_enabled = true;
		}

		if (of_property_read_u32(np, "mediatek,rpm-autosuspend-delay",
			&ufs_mtk_rpm_autosuspend_delay))
			ufs_mtk_rpm_autosuspend_delay = -1;

		if (of_property_read_bool(np, "mediatek,spm_sw_mode"))
			host->spm_sw_mode = true;
		else
			host->spm_sw_mode = false;
	}
}

void ufs_mtk_parse_auto_hibern8_timer(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	unsigned long flags;
	u32 ahit;
	u32 ah_ms = 0;

	if (np) {
		if (of_property_read_u32(np, "mediatek,auto-hibern8-timer",
			&ah_ms))
			ah_ms = 0;
	}

	dev_info(hba->dev, "auto-hibern8 timer %d ms\n", ah_ms);

	if (!ah_ms)
		return;

	/* update auto-hibern8 timer */
	ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, ah_ms) |
	       FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3);
	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->ahit == ahit)
		goto out_unlock;
	hba->ahit = ahit;
out_unlock:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

/**
 * ufs_mtk_ffu_send_cmd - sends WRITE BUFFER command to do FFU
 * @hba: per adapter instance
 * @idata: ioctl data for ffu
 *
 * Returns 0 if ffu operation is sccessfull
 * Returns non-zero if failed to do ffu
 */
static int ufs_mtk_ffu_send_cmd(struct scsi_device *dev,
	struct ufs_ioctl_ffu_data *idata)
{
	struct ufs_hba *hba;
	unsigned char cmd[10];
	struct scsi_sense_hdr sshdr;
	unsigned long flags;
	int ret;

	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;

	spin_lock_irqsave(hba->host->host_lock, flags);

	ret = scsi_device_get(dev);
	if (!ret && !scsi_device_online(dev)) {
		ret = -ENODEV;
		scsi_device_put(dev);
	}


	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	/*
	 * If scsi commands fail, the scsi mid-layer schedules scsi error-
	 * handling, which would wait for host to be resumed. Since we know
	 * we are functional while we are here, skip host resume in error
	 * handling context.
	 */
	hba->host->eh_noresume = 1;

	cmd[0] = WRITE_BUFFER;                   /* Opcode */
	cmd[1] = 0xE;                            /* 0xE: Download firmware */
	cmd[2] = 0;                              /* Buffer ID = 0 */
	cmd[3] = 0;                              /* Buffer Offset[23:16] = 0 */
	cmd[4] = 0;                              /* Buffer Offset[15:08] = 0 */
	cmd[5] = 0;                              /* Buffer Offset[07:00] = 0 */
	cmd[6] = (idata->buf_byte >> 16) & 0xff; /* Length[23:16] */
	cmd[7] = (idata->buf_byte >> 8) & 0xff;  /* Length[15:08] */
	cmd[8] = (idata->buf_byte) & 0xff;       /* Length[07:00] */
	cmd[9] = 0x0;                            /* Control = 0 */

	/*
	 * Current function would be generally called from the power management
	 * callbacks hence set the RQF_PM flag so that it doesn't resume the
	 * already suspended children.
	 */
	ret = scsi_execute(dev, cmd, DMA_TO_DEVICE,
				idata->buf_ptr, idata->buf_byte, NULL, &sshdr,
				msecs_to_jiffies(1000), 0, 0, RQF_PM, NULL);

	if (ret) {
		sdev_printk(KERN_ERR, dev,
			  "WRITE BUFFER failed for firmware upgrade\n");
	}

	scsi_device_put(dev);
	hba->host->eh_noresume = 0;
	return ret;
}

/**
 * ufs_mtk_ioctl_get_fw_ver - perform user request: query fw ver
 * @hba: per-adapter instance
 * @buffer: user space buffer for ffu ioctl data
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_ffu_data.
 * It will read the buffer information of new firmware.
 */
int ufs_mtk_ioctl_get_fw_ver(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba;
	struct ufs_ioctl_query_fw_ver_data *idata = NULL;
	int err;

	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;

	/* check scsi device instance */
	if (!dev->rev) {
		dev_err(hba->dev, "%s: scsi_device or rev is NULL\n", __func__);
		err = -ENOENT;
		goto out;
	}

	idata = kzalloc(sizeof(struct ufs_ioctl_query_fw_ver_data), GFP_KERNEL);

	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(idata, buf_user,
			sizeof(struct ufs_ioctl_query_fw_ver_data));

	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

	idata->buf_byte = min_t(int, UFS_IOCTL_FFU_MAX_FW_VER_BYTES,
				idata->buf_byte);

	/*
	 * Copy firmware version string to user buffer
	 *
	 * We get firmware version from scsi_device->rev,
	 * which is ready in scsi_add_lun()
	 * during SCSI device probe process.
	 *
	 * If probe failed, rev will be NULL.
	 * We checked it in the beginning of this function.
	 */
	err = copy_to_user(idata->buf_ptr, dev->rev, idata->buf_byte);

	if (err) {
		dev_err(hba->dev, "%s: err %d copying back to user.\n",
				__func__, err);
		goto out_release_mem;
	}

out_release_mem:
	kfree(idata);
out:
	return 0;
}

void ufs_mtk_device_quiesce(struct ufs_hba *hba)
{
	struct scsi_device *scsi_d;
	int i;

	/*
	 * Wait all cmds done & block user issue cmds to
	 * general LUs, wlun device, wlun rpmb and wlun boot.
	 * To avoid new cmds coming after device has been
	 * stopped by SSU cmd in ufshcd_suspend().
	 */
	for (i = 0; i < UFS_UPIU_MAX_GENERAL_LUN; i++) {
		scsi_d = scsi_device_lookup(hba->host, 0, 0, i);
		if (scsi_d)
			scsi_device_quiesce(scsi_d);
	}

	scsi_d = scsi_device_lookup(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_BOOT_WLUN));
	if (scsi_d)
		scsi_device_quiesce(scsi_d);

	if (hba->sdev_ufs_device)
		scsi_device_quiesce(hba->sdev_ufs_device);
	if (hba->sdev_ufs_rpmb)
		scsi_device_quiesce(hba->sdev_ufs_rpmb);
}

void ufs_mtk_device_resume(struct ufs_hba *hba)
{
	struct scsi_device *scsi_d;
	int i;

	for (i = 0; i < UFS_UPIU_MAX_GENERAL_LUN; i++) {
		scsi_d = scsi_device_lookup(hba->host, 0, 0, i);
		if (scsi_d)
			scsi_device_resume(scsi_d);
	}

	scsi_d = scsi_device_lookup(hba->host, 0, 0,
	ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_BOOT_WLUN));
	if (scsi_d)
		scsi_device_resume(scsi_d);

	if (hba->sdev_ufs_device)
		scsi_device_resume(hba->sdev_ufs_device);
	if (hba->sdev_ufs_rpmb)
		scsi_device_resume(hba->sdev_ufs_rpmb);
}

/**
 * ufs_mtk_ioctl_ffu - perform user ffu
 * @hba: per-adapter instance
 * @buffer: user space buffer for ffu ioctl data
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_ffu_data.
 * It will read the buffer information of new firmware.
 */
int ufs_mtk_ioctl_ffu(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba = shost_priv(dev->host);
	struct ufs_ioctl_ffu_data *idata = NULL;
	struct ufs_ioctl_ffu_data *idata_user = NULL;
	int err = 0;
	u32 attr = 0;

	idata = kzalloc(sizeof(struct ufs_ioctl_ffu_data), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	idata_user = kzalloc(sizeof(struct ufs_ioctl_ffu_data), GFP_KERNEL);
	if (!idata_user) {
		kfree(idata);
		err = -ENOMEM;
		goto out;
	}

	/* extract struct idata from user buffer */
	err = copy_from_user(idata_user, buf_user,
		sizeof(struct ufs_ioctl_ffu_data));

	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

	memcpy(idata, idata_user, sizeof(struct ufs_ioctl_ffu_data));

	/* extract firmware from user buffer */
	if (idata->buf_byte > (u32)UFS_IOCTL_FFU_MAX_FW_SIZE_BYTES) {
		dev_err(hba->dev, "%s: idata->buf_byte:0x%x > max 0x%x bytes\n",
				__func__, idata->buf_byte,
				(u32)UFS_IOCTL_FFU_MAX_FW_SIZE_BYTES);
		err = -ENOMEM;
		goto out_release_mem;
	}
	idata->buf_ptr = kzalloc(idata->buf_byte, GFP_KERNEL);

	if (!idata->buf_ptr) {
		err = -ENOMEM;
		goto out_release_mem;
	}

	if (copy_from_user(idata->buf_ptr,
		(void __user *)idata_user->buf_ptr, idata->buf_byte)) {
		err = -EFAULT;
		goto out_release_mem;
	}

	ufs_mtk_device_quiesce(hba);

	/* do FFU */
	err = ufs_mtk_ffu_send_cmd(dev, idata);

	if (err) {
		dev_err(hba->dev, "%s: ffu failed, err %d\n", __func__, err);
		ufs_mtk_device_resume(hba);

	} else {
		dev_info(hba->dev, "%s: ffu ok\n", __func__);
	}

	/*
	 * Check bDeviceFFUStatus attribute
	 *
	 * For reference only since UFS spec. said the status is valid after
	 * device power cycle.
	 */
	err = ufshcd_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
		QUERY_ATTR_IDN_DEVICE_FFU_STATUS, 0, 0, &attr);

	if (err) {
		dev_err(hba->dev, "%s: query bDeviceFFUStatus failed, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

	if (attr > UFS_FFU_STATUS_OK)
		dev_err(hba->dev, "%s: bDeviceFFUStatus shows fail %d (ref only)\n",
			__func__, attr);

out_release_mem:
	kfree(idata->buf_ptr);
	kfree(idata);
	kfree(idata_user);
out:
	/*
	 * UFS might not be used normally after FFU.
	 * Just reboot system (including device) to avoid following
	 * false alarm. For example, I/O errors.
	 */
	emergency_restart();

	return err;
}

/**
 * ufs_mtk_ioctl_query - perform user read queries
 * @hba: per-adapter instance
 * @lun: used for lun specific queries
 * @buffer: user space buffer for reading and submitting query data and params
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_query_data.
 * It will read the opcode, idn and buf_length parameters, and, put the
 * response in the buffer field while updating the used size in buf_length.
 */
#if defined(CONFIG_UFSFEATURE)
int ufsf_query_ioctl(struct ufsf_feature *ufsf, int lun, void __user *buffer,
		     struct ufs_ioctl_query_data_hpb *ioctl_data, u8 selector);
#endif
int ufs_mtk_ioctl_query(struct ufs_hba *hba, u8 lun, void __user *buf_user)
{
	struct ufs_ioctl_query_data *idata;
	void __user *user_buf_ptr;
	int err = 0;
	int length = 0;
	void *data_ptr;
	bool flag;
	u32 att = 0;
	u8 *desc = NULL;
	u8 read_desc, read_attr, write_attr, read_flag;
#if defined(CONFIG_UFSFEATURE)
	u8 selector;
#endif
	idata = kzalloc(sizeof(struct ufs_ioctl_query_data), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(idata, buf_user,
			sizeof(struct ufs_ioctl_query_data));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

#if defined(CONFIG_UFSFEATURE)
	if (hba->card->wmanufacturerid == UFS_VENDOR_SAMSUNG ||
		hba->card->wmanufacturerid == UFS_VENDOR_MICRON)
		selector = UFSFEATURE_SELECTOR;
	else
		selector = 0;

	if (ufsf_check_query(idata->opcode)) {
		err = ufsf_query_ioctl(&hba->ufsf, lun, buf_user,
				(struct ufs_ioctl_query_data_hpb *)idata,
				selector);
		goto out_release_mem;
	}
#endif

	user_buf_ptr = idata->buf_ptr;

	/*
	 * save idata->idn to specific local variable to avoid
	 * confusion by comapring idata->idn with different enums below.
	 */
	switch (idata->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		read_desc = idata->idn;
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		read_attr = idata->idn;
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		write_attr = idata->idn;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		read_flag = idata->idn;
		break;
	default:
		goto out_einval;
	}

	/* verify legal parameters & send query */
	switch (idata->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		switch (read_desc) {
		case QUERY_DESC_IDN_DEVICE:
		case QUERY_DESC_IDN_STRING:
			break;
		default:
			goto out_einval;
		}

		length = min_t(int, QUERY_DESC_MAX_SIZE,
				idata->buf_byte);

		desc = kzalloc(length, GFP_KERNEL);

		if (!desc) {
			err = -ENOMEM;
			goto out_release_mem;
		}

		err = ufshcd_query_descriptor_retry(hba, idata->opcode,
				read_desc, idata->idx, 0, desc, &length);
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		switch (read_attr) {
		case QUERY_ATTR_IDN_BOOT_LUN_EN:
			break;
		case QUERY_ATTR_IDN_DEVICE_FFU_STATUS:
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_attr(hba, idata->opcode,
					read_attr, idata->idx, 0, &att);
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		switch (write_attr) {
		case QUERY_ATTR_IDN_BOOT_LUN_EN:
			break;
		default:
			goto out_einval;
		}

		length = min_t(int, sizeof(int), idata->buf_byte);

		if (copy_from_user(&att, (void __user *)(unsigned long)
					idata->buf_ptr, length)) {
			err = -EFAULT;
			goto out_release_mem;
		}

		err = ufshcd_query_attr(hba, idata->opcode,
					write_attr, idata->idx, 0, &att);
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		switch (read_flag) {
		case QUERY_FLAG_IDN_PERMANENTLY_DISABLE_FW_UPDATE:
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_flag(hba, idata->opcode,
					read_flag, &flag);
		break;
	default:
		goto out_einval;
	}

	if (err) {
		dev_err(hba->dev, "%s: query for idn %d failed\n", __func__,
					idata->idn);
		goto out_release_mem;
	}

	/*
	 * copy response data
	 * As we might end up reading less data then what is specified in
	 * "ioct_data->buf_byte". So we are updating "ioct_data->
	 * buf_byte" to what exactly we have read.
	 */
	switch (idata->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		idata->buf_byte = min_t(int, idata->buf_byte, length);
		data_ptr = desc;
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		idata->buf_byte = sizeof(att);
		data_ptr = &att;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		idata->buf_byte = 1;
		data_ptr = &flag;
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		/* write attribute does not require coping response data */
		goto out_release_mem;
	default:
		goto out_einval;
	}

	/* copy to user */
	err = copy_to_user(buf_user, idata,
			sizeof(struct ufs_ioctl_query_data));
	if (err)
		dev_err(hba->dev, "%s: failed copying back to user.\n",
			__func__);

	err = copy_to_user(user_buf_ptr, data_ptr, idata->buf_byte);
	if (err)
		dev_err(hba->dev, "%s: err %d copying back to user.\n",
				__func__, err);
	goto out_release_mem;

out_einval:
	dev_err(hba->dev,
		"%s: illegal ufs query ioctl data, opcode 0x%x, idn 0x%x\n",
		__func__, idata->opcode, (unsigned int)idata->idn);
	err = -EINVAL;
out_release_mem:
	kfree(idata);
	kfree(desc);
out:
	return err;
}

/**
 * ufs_mtk_ioctl_rpmb - perform user rpmb read/write request
 * @hba: per-adapter instance
 * @buf_user: user space buffer for ioctl rpmb_cmd data
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct rpmb_cmd.
 * It will read/write data to rpmb
 */
int ufs_mtk_ioctl_rpmb(struct ufs_hba *hba, void __user *buf_user)
{
	struct rpmb_cmd cmd[3];
	struct rpmb_frame *frame_buf = NULL;
	struct rpmb_frame *frames = NULL;
	int size = 0;
	int nframes = 0;
	unsigned long flags;
	struct scsi_device *sdev;
	int ret;
	int i;

	/* Get scsi device */
	spin_lock_irqsave(hba->host->host_lock, flags);
	sdev = hba->sdev_ufs_rpmb;
	if (sdev) {
		ret = scsi_device_get(sdev);
		if (!ret && !scsi_device_online(sdev)) {
			ret = -ENODEV;
			scsi_device_put(sdev);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret) {
		dev_info(hba->dev,
			"%s: failed get rpmb device, ret %d\n",
			__func__, ret);
		goto out;
	}

	/* Get cmd params from user buffer */
	ret = copy_from_user((void *) cmd,
		buf_user, sizeof(struct rpmb_cmd) * 3);
	if (ret) {
		dev_info(hba->dev,
			"%s: failed copying cmd buffer from user, ret %d\n",
			__func__, ret);
		goto out_put;
	}

	/* Check number of rpmb frames */
	for (i = 0; i < 3; i++) {
		ret = (int)rpmb_get_rw_size(ufs_mtk_rpmb_get_raw_dev());
		if (cmd[i].nframes > ret) {
			dev_info(hba->dev,
				"%s: number of rpmb frames %u exceeds limit %d\n",
				__func__, cmd[i].nframes, ret);
			ret = -EINVAL;
			goto out_put;
		}
	}

	/* Prepaer frame buffer */
	for (i = 0; i < 3; i++)
		nframes += cmd[i].nframes;
	frame_buf = kcalloc(nframes, sizeof(struct rpmb_frame), GFP_KERNEL);
	if (!frame_buf) {
		ret = -ENOMEM;
		goto out_put;
	}
	frames = frame_buf;

	/*
	 * Send all command one by one.
	 * Use rpmb lock to prevent other rpmb read/write threads cut in line.
	 * Use mutex not spin lock because in/out function might sleep.
	 */
	mutex_lock(&hba->rpmb_lock);
	for (i = 0; i < 3; i++) {
		if (cmd[i].nframes == 0)
			break;

		/* Get frames from user buffer */
		size = sizeof(struct rpmb_frame) * cmd[i].nframes;
		ret = copy_from_user((void *) frames, cmd[i].frames, size);
		if (ret) {
			dev_err(hba->dev,
				"%s: failed from user, ret %d\n",
				__func__, ret);
			break;
		}

		/* Do rpmb in out */
		if (cmd[i].flags & RPMB_F_WRITE) {
			ret = ufshcd_rpmb_security_out(sdev, frames,
						       cmd[i].nframes);
			if (ret) {
				dev_err(hba->dev,
					"%s: failed rpmb out, err %d\n",
					__func__, ret);
				break;
			}

		} else {
			ret = ufshcd_rpmb_security_in(sdev, frames,
						      cmd[i].nframes);
			if (ret) {
				dev_err(hba->dev,
					"%s: failed rpmb in, err %d\n",
					__func__, ret);
				break;
			}

			/* Copy frames to user buffer */
			ret = copy_to_user((void *) cmd[i].frames,
				frames, size);
			if (ret) {
				dev_err(hba->dev,
					"%s: failed to user, err %d\n",
					__func__, ret);
				break;
			}
		}

		frames += cmd[i].nframes;
	}
	mutex_unlock(&hba->rpmb_lock);

	kfree(frame_buf);

out_put:
	scsi_device_put(sdev);
out:
	return ret;
}

static int ufs_mtk_scsi_dev_cfg(struct scsi_device *sdev,
	enum ufs_scsi_dev_cfg op)
{
	if (op == UFS_SCSI_DEV_SLAVE_CONFIGURE) {
		if (ufs_mtk_rpm_enabled) {
			sdev->use_rpm_auto = 1;
			sdev->autosuspend_delay = ufs_mtk_rpm_autosuspend_delay;
		} else {
			sdev->use_rpm_auto = 0;
			sdev->autosuspend_delay = -1;
		}
	}

	return 0;
}

void ufs_mtk_runtime_pm_init(struct scsi_device *sdev)
{
	/*
	 * If runtime PM is enabled for UFS device, use auto-suspend mechanism
	 * if assigned.
	 *
	 * This is only for those SCSI devices which runtime PM is not managed
	 * by block layer. Thus autosuspend of this device is not configured by
	 * sd_probe_async.
	 */
	if (ufs_mtk_rpm_enabled && sdev->autosuspend_delay >= 0) {
		pm_runtime_set_autosuspend_delay(&sdev->sdev_gendev,
			sdev->autosuspend_delay);
		pm_runtime_use_autosuspend(&sdev->sdev_gendev);
	}
}

static void ufs_mtk_device_reset(struct ufs_hba *hba)
{
	(void)ufs_mtk_pltfrm_ufs_device_reset(hba);

#ifdef CONFIG_MTK_UFS_LBA_CRC16_CHECK
	/* Clear di memory to avoid false alarm */
	ufs_mtk_di_reset(hba);
#endif
}

static void ufs_mtk_auto_hibern8(struct ufs_hba *hba, bool enable)
{
	/* if auto-hibern8 is not enabled by device tree, return */
	if (!hba->ahit)
		return;

	dev_dbg(hba->dev, "ah8: %d, ahit: 0x%x\n", enable, hba->ahit);

	/* if already enabled or disabled, return */
	if (!(ufs_mtk_auto_hibern8_enabled ^ enable))
		return;

	if (enable) {
		ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);

		ufs_mtk_auto_hibern8_enabled = true;
	} else {
		/* disable auto-hibern8 */
		ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);

		ufs_mtk_auto_hibern8_enabled = false;
	}
}

int ufs_mtk_auto_hiber8_quirk_handler(struct ufs_hba *hba, bool enable)
{
	/*
	 * do not toggle ah8 during suspend/resume callback
	 * since ah8 policy is always fixed
	 * as below,
	 *
	 * 1. suspend: ah8 is disabled before suspend flow starts.
	 * 2. resume: ah8 is enabled after resume flow is finished.
	 */
	if (hba->quirks & UFSHCD_QUIRK_UFS_HCI_DISABLE_AH8_BEFORE_RDB &&
		!hba->outstanding_reqs &&
		!hba->outstanding_tasks &&
		!hba->pm_op_in_progress) {

		ufshcd_vops_auto_hibern8(hba, enable);
	}

	return 0;
}

int ufs_mtk_wait_link_state(struct ufs_hba *hba, u32 *state,
			    unsigned long retry_ms)
{
	unsigned long timeout;
	u32 val;

	timeout = jiffies + msecs_to_jiffies(retry_ms);
	do {
		ufshcd_writel(hba, 0x20, REG_UFS_MTK_DEBUG_SEL);
		val = ufshcd_readl(hba, REG_UFS_MTK_PROBE);
		val = val >> 28;

		if (val == *state)
			break;

		/* sleep for max. 200us */
		usleep_range(100, 200);
	} while (time_before(jiffies, timeout));

	if (val == *state)
		return 0;

	*state = val;
	return -ETIMEDOUT;
}

int ufs_mtk_generic_read_dme_no_check(u32 uic_cmd, u16 mib_attribute,
	u16 gen_select_index, u32 *value, unsigned long retry_ms)
{
	struct ufs_hba *hba = ufs_mtk_hba;
	struct uic_command ucmd = {0};
	u32 val;
	int ret = 0;
	unsigned long elapsed_us = 0;
	bool reenable_intr = false;

	val = ufshcd_readl(hba, REG_CONTROLLER_STATUS);
	if (!(val & UIC_COMMAND_READY) ||
		hba->active_uic_cmd) {
		dev_info(hba->dev,
			"uic not rdy, host sta(0x%x), uic cmd(%d)\n",
			val,
			hba->active_uic_cmd ?
			hba->active_uic_cmd->command : -1);
		return -EIO;
	}

	if (ufshcd_readl(hba, REG_INTERRUPT_ENABLE)
			& UIC_COMMAND_COMPL) {
		ufshcd_disable_intr(hba, UIC_COMMAND_COMPL);
		/*
		 * Make sure UIC command completion interrupt is disabled before
		 * issuing UIC command.
		 */
		wmb();
		reenable_intr = true;
	}

	ucmd.command = uic_cmd;
	ucmd.argument1 = UIC_ARG_MIB_SEL(
		(u32)mib_attribute, (u32)gen_select_index);
	ucmd.argument2 = 0;
	ucmd.argument3 = 0;

	ufshcd_writel(hba, ucmd.argument1, REG_UIC_COMMAND_ARG_1);
	ufshcd_writel(hba, ucmd.argument2, REG_UIC_COMMAND_ARG_2);
	ufshcd_writel(hba, ucmd.argument3, REG_UIC_COMMAND_ARG_3);
	ufs_mtk_dme_cmd_log(hba, &ucmd, UFS_TRACE_UIC_SEND);

	ufshcd_writel(hba,
		ucmd.command & COMMAND_OPCODE_MASK, REG_UIC_COMMAND);

	while ((ufshcd_readl(hba,
		REG_INTERRUPT_STATUS) & UIC_COMMAND_COMPL)
		!= UIC_COMMAND_COMPL) {
		/* busy waiting 1us */
		udelay(1);
		elapsed_us += 1;
		if (elapsed_us > (retry_ms * 1000)) {
			ret = -ETIMEDOUT;
			goto out;
		}
	}
	ufshcd_writel(hba, UIC_COMMAND_COMPL, REG_INTERRUPT_STATUS);

	ufs_mtk_dme_cmd_log(hba, &ucmd, UFS_TRACE_UIC_CMPL_GENERAL);

	val = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2);
	if (val & MASK_UIC_COMMAND_RESULT) {
		ret = val;
		goto out;
	}

	*value = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3);
out:
	if (reenable_intr)
		ufshcd_enable_intr(hba, UIC_COMMAND_COMPL);

	return ret;
}

/* Notice: this function must be called in automic context */
/* Because it is not protected by ufs spin_lock or mutex */
/* it access ufs host directly. */
int ufs_mtk_generic_read_dme(u32 uic_cmd, u16 mib_attribute,
	u16 gen_select_index, u32 *value, unsigned long retry_ms)
{
	if (ufs_mtk_hba->outstanding_reqs || ufs_mtk_hba->outstanding_tasks
		|| ufs_mtk_hba->active_uic_cmd ||
		ufs_mtk_hba->pm_op_in_progress) {
		dev_dbg(ufs_mtk_hba->dev, "req: %lx, task %lx, pm %x\n",
			ufs_mtk_hba->outstanding_reqs
			, ufs_mtk_hba->outstanding_tasks,
			ufs_mtk_hba->pm_op_in_progress);
		if (ufs_mtk_hba->active_uic_cmd != NULL)
			dev_dbg(ufs_mtk_hba->dev, "uic not null\n");
		return -1;
	}

	return ufs_mtk_generic_read_dme_no_check(uic_cmd, mib_attribute,
		gen_select_index, value, retry_ms);
}

/**
 * ufs_mtk_deepidle_hibern8_check - callback function for Deepidle & SODI.
 * Release all resources: DRAM/26M clk/Main PLL and dsiable 26M ref clk if in H8
 *
 * @return: 0 for success, negative/postive error code otherwise
 */
int ufs_mtk_deepidle_hibern8_check(void)
{
	return ufs_mtk_pltfrm_deepidle_check_h8();
}

/**
 * ufs_mtk_deepidle_leave - callback function for leaving Deepidle & SODI.
 */
void ufs_mtk_deepidle_leave(void)
{
	ufs_mtk_pltfrm_deepidle_leave();
}

struct rpmb_dev *ufs_mtk_rpmb_get_raw_dev(void)
{
	return ufs_mtk_hba->rawdev_ufs_rpmb;
}

#ifdef CONFIG_MTK_UFS_DEBUG
void ufs_mtk_dump_asc_ascq(struct ufs_hba *hba, u8 asc, u8 ascq)
{
	dev_info(hba->dev, "Sense Data: ASC=%#04x, ASCQ=%#04x\n", asc, ascq);

	if (asc == 0x25) {
		if (ascq == 0x00)
			dev_err(hba->dev, "Logical unit not supported!\n");
	} else if (asc == 0x29) {
		if (ascq == 0x00)
			dev_err(hba->dev,
				"Power on, reset, or bus device reset occupied\n");
	}
}
#endif

void ufs_mtk_crypto_cal_dun(u32 alg_id, u64 iv, u32 *dunl, u32 *dunu)
{
	/* bitlocker dun use byte address */
	if (alg_id == UFS_CRYPTO_ALGO_BITLOCKER_AES_CBC)
		iv = iv << 12;

	*dunl = iv & 0xffffffff;
	*dunu = (iv >> 32) & 0xffffffff;
}

bool ufs_mtk_is_data_write_cmd(char cmd_op)
{
	if (cmd_op == WRITE_10 || cmd_op == WRITE_16 || cmd_op == WRITE_6)
		return true;

	return false;
}

static inline bool ufs_mtk_is_unmap_cmd(char cmd_op)
{
	if (cmd_op == UNMAP)
		return true;

	return false;
}

static bool ufs_mtk_is_data_cmd(char cmd_op)
{
	if (cmd_op == WRITE_10 || cmd_op == READ_10 ||
	    cmd_op == WRITE_16 || cmd_op == READ_16 ||
	    cmd_op == WRITE_6 || cmd_op == READ_6)
		return true;

	return false;
}

static struct ufs_cmd_str_struct ufs_cmd_str_tbl[] = {
	{"TEST_UNIT_READY",        0x00},
	{"REQUEST_SENSE",          0x03},
	{"FORMAT_UNIT",            0x04},
	{"READ_BLOCK_LIMITS",      0x05},
	{"INQUIRY",                0x12},
	{"RECOVER_BUFFERED_DATA",  0x14},
	{"MODE_SENSE",             0x1a},
	{"START_STOP",             0x1b},
	{"SEND_DIAGNOSTIC",        0x1d},
	{"READ_FORMAT_CAPACITIES", 0x23},
	{"READ_CAPACITY",          0x25},
	{"READ_10",                0x28},
	{"WRITE_10",               0x2a},
	{"PRE_FETCH",              0x34},
	{"SYNCHRONIZE_CACHE",      0x35},
	{"WRITE_BUFFER",           0x3b},
	{"READ_BUFFER",            0x3c},
	{"UNMAP",                  0x42},
	{"MODE_SELECT_10",         0x55},
	{"MODE_SENSE_10",          0x5a},
	{"REPORT_LUNS",            0xa0},
	{"READ_CAPACITY_16",       0x9e},
	{"SECURITY_PROTOCOL_IN",   0xa2},
	{"MAINTENANCE_IN",         0xa3},
	{"MAINTENANCE_OUT",        0xa4},
	{"SECURITY_PROTOCOL_OUT",  0xb5},
	{"UNKNOWN",                0xFF}
};

static int ufs_mtk_get_cmd_str_idx(char cmd)
{
	int i;

	for (i = 0; ufs_cmd_str_tbl[i].cmd != 0xFF; i++) {
		if (ufs_cmd_str_tbl[i].cmd == cmd)
			return i;
	}

	return i;
}

void ufs_mtk_dbg_dump_scsi_cmd(struct ufs_hba *hba,
	struct scsi_cmnd *cmd, u32 flag)
{
#ifdef CONFIG_MTK_UFS_DEBUG
	u32 lba = 0;
	u32 blk_cnt;
	u32 fua, flush;
	char str[32];

	strncpy(str,
		ufs_cmd_str_tbl[ufs_mtk_get_cmd_str_idx(cmd->cmnd[0])].str,
		32 - 1);

	if (ufs_mtk_is_data_cmd(cmd->cmnd[0])) {
		lba = cmd->cmnd[5] | (cmd->cmnd[4] << 8) |
			(cmd->cmnd[3] << 16) | (cmd->cmnd[2] << 24);
		blk_cnt = cmd->cmnd[8] | (cmd->cmnd[7] << 8);
		fua = (cmd->cmnd[1] & 0x8) ? 1 : 0;
		flush = (cmd->request->cmd_flags & REQ_FUA) ? 1 : 0;

#ifdef CONFIG_MTK_HW_FDE
		if (cmd->request->bio && cmd->request->bio->bi_hw_fde) {
			dev_dbg(hba->dev,
				"QCMD(C),L:%x,T:%d,0x%x,%s,LBA:%d,BCNT:%d,FUA:%d,FLUSH:%d\n",
				ufshcd_scsi_to_upiu_lun(cmd->device->lun),
				cmd->request->tag, cmd->cmnd[0],
				str,
				lba, blk_cnt, fua, flush);

		} else
#endif
		{
			#ifdef CONFIG_HIE
			if (hie_request_crypted(cmd->request)) {
				dev_dbg(hba->dev,
				"QCMD(H),L:%x,T:%d,0x%x,%s,LBA:%d,BCNT:%d,FUA:%d,FLUSH:%d\n",
				ufshcd_scsi_to_upiu_lun(cmd->device->lun),
					cmd->request->tag, cmd->cmnd[0],
					str,
					lba, blk_cnt, fua, flush);
			} else
			#endif
			{
				dev_dbg(hba->dev,
				"QCMD,L:%x,T:%d,0x%x,%s,LBA:%d,BCNT:%d,FUA:%d,FLUSH:%d\n",
				ufshcd_scsi_to_upiu_lun(cmd->device->lun),
					cmd->request->tag, cmd->cmnd[0],
					str,
					lba, blk_cnt, fua, flush);
			}
		}
	} else {
		dev_dbg(hba->dev, "QCMD,L:%x,T:%d,0x%x,%s\n",
			ufshcd_scsi_to_upiu_lun(cmd->device->lun),
			cmd->request->tag, cmd->cmnd[0],
			str);
	}
#endif
}

#ifdef SPM_READY
void ufs_mtk_res_ctrl(struct ufs_hba *hba, unsigned int op)
{
	int res_type = 0;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (!host->spm_sw_mode)
		return;

	if (!hba->outstanding_tasks && !hba->outstanding_reqs) {
		if (op == UFS_RESCTL_CMD_SEND) {
			if (hba->active_uic_cmd)
				res_type = 1;
			else
				res_type = 2;
		} else if (op == UFS_RESCTL_CMD_COMP)
			res_type = 1;
	}

	if (res_type == 1)
		spm_resource_req(SPM_RESOURCE_USER_UFS,
				 SPM_RESOURCE_MAINPLL | SPM_RESOURCE_CK_26M);
	else if (res_type == 2)
		spm_resource_req(SPM_RESOURCE_USER_UFS, SPM_RESOURCE_ALL);
}
#else
#define ufs_mtk_res_ctrl	NULL
#endif

/**
 * struct ufs_hba_mtk_vops - UFS MTK specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static struct ufs_hba_variant_ops ufs_hba_mtk_vops = {
	"mediatek.ufshci",  /* name */
	ufs_mtk_init,    /* init */
	NULL,            /* exit */
	NULL,            /* get_ufs_hci_version */
	NULL,            /* clk_scale_notify */
	ufs_mtk_setup_clocks,            /* setup_clocks */
	NULL,            /* setup_regulators */
	ufs_mtk_hce_enable_notify,    /* hce_enable_notify */
	ufs_mtk_link_startup_notify,  /* link_startup_notify */
	ufs_mtk_pwr_change_notify,    /* pwr_change_notify */
	NULL,		 /* setup_xfer_req */
	NULL,		 /* setup_task_mgmt */
	NULL,		 /* hibern8_notify */
	NULL,            /* apply_dev_quirks */
	ufs_mtk_suspend,              /* suspend */
	ufs_mtk_resume,               /* resume */
	ufs_mtk_dbg_register_dump,    /* dbg_register_dump */
	NULL,			 /* phy_initialization */
	ufs_mtk_device_reset,         /* device_reset */
	ufs_mtk_auto_hibern8,         /* auto_hibern8 */
	ufs_mtk_res_ctrl,             /* res_ctrl */
	ufs_mtk_pltfrm_deepidle_lock, /* deepidle_lock */
	ufs_mtk_scsi_dev_cfg          /* scsi_dev_cfg */
};

/**
 * ufs_mtk_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_mtk_probe(struct platform_device *pdev)
{
	int err;
	struct ufs_hba *hba;
	struct ufs_mtk_host *host;
	struct device *dev = &pdev->dev;
	int boot_type;
	void __iomem *ufs_base;

	/*
	 * Disable xoufs_req_s in ufshci to deactivate
	 * 1. UFS_SRCCLKENA
	 * 2. UFS_INFRA_REQ
	 * 3. UFS_VRF18_REQ
	 * Which block 26M off
	 */
	ufs_base = of_iomap(pdev->dev.of_node, 0);
	if (!ufs_base) {
		dev_err(dev, "ufs iomap failed\n");
		return -ENODEV;
	};
#ifndef UFS_REF_CLK_CTRL_BY_UFSHCI
	/* Old chip not use this, disable it always */
	writel(0, ufs_base + REG_UFS_ADDR_XOUFS_ST);
#endif
	/* Add get_boot_type check and return ENODEV if not ufs boot */
	boot_type = get_boot_type();
	if (boot_type != BOOTDEV_UFS) {
#ifdef UFS_REF_CLK_CTRL_BY_UFSHCI
		/* New chip disable it when eMMC boot */
		writel(0, ufs_base + REG_UFS_ADDR_XOUFS_ST);
#endif
		return -ENODEV;
	}

	ufs_mtk_biolog_init();

	/* perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_mtk_vops);
	if (err) {
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);
		goto out;
	}

	hba = platform_get_drvdata(pdev);

	host = ufshcd_get_variant(hba);

	pm_qos_add_request(&host->req_cpu_dma_latency, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);

	err = ufs_mtk_probe_crypto(hba);
	if (err)
		dev_info(dev, "ufs_mtk_probe_crypto() failed %d\n", err);

out:
	return err;
}

/**
 * ufs_mtk_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always return 0
 */
static int ufs_mtk_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	pm_runtime_get_sync(&(pdev)->dev);
	ufshcd_remove(hba);
	ufs_mtk_biolog_exit();
	if (host)
		pm_qos_remove_request(&host->req_cpu_dma_latency);

	return 0;
}

const struct of_device_id ufs_mtk_of_match[] = {
	{ .compatible = "mediatek,ufshci"},
	{},
};

static const struct dev_pm_ops ufs_mtk_pm_ops = {
	.suspend	= ufshcd_pltfrm_suspend,
	.resume		= ufshcd_pltfrm_resume,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume  = ufshcd_pltfrm_runtime_resume,
	.runtime_idle    = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_mtk_pltform = {
	.probe	= ufs_mtk_probe,
	.remove	= ufs_mtk_remove,
	.shutdown = ufshcd_pltfrm_shutdown,
	.driver	= {
		.name	= "ufshcd",
		.owner	= THIS_MODULE,
		.pm	= &ufs_mtk_pm_ops,
		.of_match_table = ufs_mtk_of_match,
	},
};

module_platform_driver(ufs_mtk_pltform);

