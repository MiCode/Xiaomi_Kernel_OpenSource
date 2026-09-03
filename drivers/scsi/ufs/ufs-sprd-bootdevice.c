// SPDX-License-Identifier: GPL-2.0-only
/*
 * Proc_bootdevice node for extend UFS info
 *
 * Copyright (C) 2022 Unisoc, Inc.
 */

#include <linux/proc_fs.h>

#include "ufs.h"
#include "ufshcd.h"
#include "ufs-sprd.h"
#include "ufs-sprd-bootdevice.h"

struct ufs_bootdevice bootdevice;
struct ufs_device_identification ufs_uid;

static void set_ufs_bootdevice_desc_info(u8 *desc)
{
	memcpy(bootdevice.device_desc, desc, sizeof(bootdevice.device_desc));
}

static void set_ufs_device_uid(struct ufs_hba *hba, const uint8_t *desc_buf)
{
	int i, idx;
	u8 sn_buf[UFS_MAX_SN_LEN + 1] = {0};

	switch (ufs_uid.manufacturer_id) {
	case UFS_VENDOR_SAMSUNG:
		/*
		 * Samsung V4\SANDISK
		 * UFS requires a 24-byte Unicode representation of SN,
		 * and here we need to convert Unicode to 12-byte display
		 */
		for (i = 0; i < UFS_MAX_SN_LEN; i++) {
			idx = QUERY_DESC_HDR_SIZE + i * 2 + 1;
			sn_buf[i] = desc_buf[idx];
		}
		break;
	case UFS_VENDOR_SKHYNIX:
		/* hynix only have 6Byte, add a 0x00 before every byte */
		for (i = 0; i < 6; i++) {
			sn_buf[i * 2] = 0x0;
			sn_buf[i * 2 + 1] = desc_buf[QUERY_DESC_HDR_SIZE + i];
		}
		break;
	case UFS_VENDOR_TOSHIBA:
		/*
		 * toshiba: 20Byte, every two byte has a prefix of 0x00, skip
		 * and add two 0x00 to the end
		 */
		for (i = 0; i < 10; i++) {
			idx = QUERY_DESC_HDR_SIZE + i * 2 + 1;
			sn_buf[i] = desc_buf[idx];
		}
		break;
	case UFS_VENDOR_MICRON:
		/* Micron need 4 bytes */
		memcpy(sn_buf, desc_buf + QUERY_DESC_HDR_SIZE, 4);
		break;
	case UFS_VENDOR_WDC:
		for (i = 0; i < UFS_MAX_SN_LEN; i++) {
			idx = QUERY_DESC_HDR_SIZE + i * 2 + 1;
			sn_buf[i] = desc_buf[idx];
		}
		break;
	default:
		dev_err(hba->dev, "unknown ufs manufacturer id: 0x%04x\n",
			ufs_uid.manufacturer_id);
		break;
	}

	memcpy(ufs_uid.serial_number, sn_buf, UFS_MAX_SN_LEN);
}

int ufshcd_decode_ufs_uid(struct ufs_hba *hba)
{
	int err;
	u8 device_desc[QUERY_DESC_MAX_SIZE] = {0};
	uint8_t *uc_str;

	uc_str = kzalloc(QUERY_DESC_MAX_SIZE + 1, GFP_KERNEL);
	if (!uc_str)
		return -ENOMEM;

	memset(&ufs_uid, 0, sizeof(struct ufs_device_identification));
	/* Device Descriptor */
	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_DEVICE, 0, 0,
				     device_desc, QUERY_DESC_MAX_SIZE);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading Device Desc. err = %d\n",
			__func__, err);
		goto out;
	}
	ufs_uid.manufacturer_id = hba->dev_info.wmanufacturerid;
	ufs_uid.manufacturer_date =
		device_desc[DEVICE_DESC_PARAM_MANF_DATE] << 8 |
		device_desc[DEVICE_DESC_PARAM_MANF_DATE + 1];

	/* Serial Number String Descriptor */
	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_STRING,
				     (int)device_desc[DEVICE_DESC_PARAM_SN],
				     0,
				     (u8 *)uc_str, QUERY_DESC_MAX_SIZE);
	if (err < 0) {
		dev_err(hba->dev, "Reading String Desc failed after %d retries. err = %d\n",
			3, err);
		goto out;
	}

	uc_str[QUERY_DESC_MAX_SIZE] = '\0';
	set_ufs_device_uid(hba, (uint8_t *)uc_str);
	set_ufs_bootdevice_desc_info(device_desc);
out:
	kfree(uc_str);
	return err;
}

static int ufs_cid_show(struct seq_file *m, void *v)
{
	int i;

	memcpy(bootdevice.cid, (u32 *)&ufs_uid, sizeof(bootdevice.cid));
	for (i = 0; i < UFS_CID_LEN - 1; i++)
		bootdevice.cid[i] = be32_to_cpu(bootdevice.cid[i]);

	bootdevice.cid[UFS_CID_LEN - 1] =
		((bootdevice.cid[UFS_CID_LEN - 1] & 0xffff) << 16) |
		((bootdevice.cid[UFS_CID_LEN - 1] >> 16) & 0xffff);

	seq_printf(m, "%08x%08x%08x%08x\n", bootdevice.cid[0],
		   bootdevice.cid[1], bootdevice.cid[2], bootdevice.cid[3]);
	return 0;
}

static int ufs_cid_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_cid_show, PDE_DATA(inode));
}

static const struct proc_ops cid_fops = {
	.proc_open = ufs_cid_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_life_time_est_typ_a_show(struct seq_file *m, void *v)
{
	int err;
	struct ufs_hba *hba = dev_get_drvdata((struct device *)m->private);
	u8 hlth_desc_buf[QUERY_DESC_MAX_SIZE];

	/* Device Health Descriptor */
	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_HEALTH, 0, 0,
				     hlth_desc_buf,
				     hba->desc_size[QUERY_DESC_IDN_HEALTH]);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health Desc. err = %d\n",
			__func__, err);
		return err;
	}

	bootdevice.life_time_est_typ_a =
		hlth_desc_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A];
	seq_printf(m, "0x%02x\n", bootdevice.life_time_est_typ_a);
	return err;
}

static int sprd_life_time_est_typ_a_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_life_time_est_typ_a_show, PDE_DATA(inode));
}

static const struct proc_ops life_time_est_typ_a_fops = {
	.proc_open = sprd_life_time_est_typ_a_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_life_time_est_typ_b_show(struct seq_file *m, void *v)
{
	int err;
	struct ufs_hba *hba = dev_get_drvdata((struct device *)m->private);
	u8 hlth_desc_buf[QUERY_DESC_MAX_SIZE];

	/* Device Health Descriptor */
	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_HEALTH, 0, 0,
				     hlth_desc_buf,
				     hba->desc_size[QUERY_DESC_IDN_HEALTH]);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health Desc. err = %d\n",
			__func__, err);
		return err;
	}

	bootdevice.life_time_est_typ_b =
		hlth_desc_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_B];
	seq_printf(m, "0x%02x\n", bootdevice.life_time_est_typ_b);
	return err;
}

static int sprd_life_time_est_typ_b_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_life_time_est_typ_b_show, PDE_DATA(inode));
}

static const struct proc_ops life_time_est_typ_b_fops = {
	.proc_open = sprd_life_time_est_typ_b_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_manid_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%06x\n", ufs_uid.manufacturer_id);
	return 0;
}

static int sprd_manid_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_manid_show, PDE_DATA(inode));
}

static const struct proc_ops manfid_fops = {
	.proc_open = sprd_manid_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_name_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", "sprd-platform");
	return 0;
}

static int sprd_name_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_name_show, PDE_DATA(inode));
}

static const struct proc_ops name_fops = {
	.proc_open = sprd_name_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int pre_eol_info_show(struct seq_file *m, void *v)
{
	int err;
	struct ufs_hba *hba = dev_get_drvdata((struct device *)m->private);
	u8 hlth_desc_buf[QUERY_DESC_MAX_SIZE];

	/* Device Health Descriptor */
	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_HEALTH, 0, 0,
				     hlth_desc_buf, hba->desc_size[QUERY_DESC_IDN_HEALTH]);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health Desc. err = %d\n",
			__func__, err);
		return err;
	}

	bootdevice.pre_eol_info = hlth_desc_buf[HEALTH_DESC_PARAM_EOL_INFO];
	seq_printf(m, "0x%02x\n", bootdevice.pre_eol_info);
	return err;
}

static int sprd_pre_eol_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, pre_eol_info_show, PDE_DATA(inode));
}

static const struct proc_ops pre_eol_info_fops = {
	.proc_open = sprd_pre_eol_info_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int prod_name_show(struct seq_file *m, void *v)
{
	struct ufs_hba *hba = dev_get_drvdata((struct device *)m->private);

	if (NULL != hba->dev_info.model)
		seq_printf(m, "%s\n", hba->dev_info.model);
	return 0;
}

static int sprd_prod_name_open(struct inode *inode, struct file *file)
{
	return single_open(file, prod_name_show, PDE_DATA(inode));
}

static const struct proc_ops prod_name_fops = {
	.proc_open = sprd_prod_name_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int prod_rev_show(struct seq_file *m, void *v)
{
	struct ufs_hba *hba = dev_get_drvdata((struct device *)m->private);

	seq_printf(m, "%s\n", hba->sdev_ufs_device->rev);
	return 0;
}

static int sprd_rev_open(struct inode *inode, struct file *file)
{
	return single_open(file, prod_rev_show, PDE_DATA(inode));
}

static const struct proc_ops rev_fops = {
	.proc_open = sprd_rev_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_size_show(struct seq_file *m, void *v)
{
	int err;
	u64 total_raw_device_capacity;
	struct ufs_hba *hba = dev_get_drvdata((struct device *)m->private);
	u8 geo_desc_buf[QUERY_DESC_MAX_SIZE];

	/* Device GEOMETRY Descriptor */
	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0, 0,
				     geo_desc_buf,
				     hba->desc_size[QUERY_DESC_IDN_GEOMETRY]);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health Desc. err = %d\n",
			__func__, err);
		return err;
	}

	total_raw_device_capacity =
		(u64)geo_desc_buf[GEOMETRY_DESC_PARAM_DEV_CAP + 0] << 56 |
		(u64)geo_desc_buf[GEOMETRY_DESC_PARAM_DEV_CAP + 1] << 48 |
		(u64)geo_desc_buf[GEOMETRY_DESC_PARAM_DEV_CAP + 2] << 40 |
		(u64)geo_desc_buf[GEOMETRY_DESC_PARAM_DEV_CAP + 3] << 32 |
		(u64)geo_desc_buf[GEOMETRY_DESC_PARAM_DEV_CAP + 4] << 24 |
		(u64)geo_desc_buf[GEOMETRY_DESC_PARAM_DEV_CAP + 5] << 16 |
		(u64)geo_desc_buf[GEOMETRY_DESC_PARAM_DEV_CAP + 6] << 8 |
		(u64)geo_desc_buf[GEOMETRY_DESC_PARAM_DEV_CAP + 7] << 0;

	seq_printf(m, "%lld\n", total_raw_device_capacity);
	return err;
}

static int sprd_size_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_size_show, PDE_DATA(inode));
}

static const struct proc_ops size_fops = {
	.proc_open = sprd_size_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int boot_type_show(struct seq_file *m, void *v)
{
	bootdevice.type = UFS_TYPE;

	seq_printf(m, "%s\n", "UFS");
	return 0;
}

static int sprd_boot_type_open(struct inode *inode, struct file *file)
{
	return single_open(file, boot_type_show, PDE_DATA(inode));
}

static const struct proc_ops type_fops = {
	.proc_open = sprd_boot_type_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_specversion_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%03x\n",
		bootdevice.device_desc[DEVICE_DESC_PARAM_SPEC_VER] << 8 |
		bootdevice.device_desc[DEVICE_DESC_PARAM_SPEC_VER + 1]);
	return 0;
}

static int sprd_specversion_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_specversion_show, PDE_DATA(inode));
}

static const struct proc_ops specversion_fops = {
	.proc_open = sprd_specversion_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_wb_status_show(struct seq_file *m, void *v)
{
	struct ufs_hba *hba = dev_get_drvdata((struct device *)m->private);

	seq_printf(m, "%d\n", hba->dev_info.wb_enabled);
	return 0;
}

static int sprd_wb_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_wb_status_show, PDE_DATA(inode));
}

static const struct proc_ops wb_fops = {
	.proc_open = sprd_wb_status_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static char *const sprd_ufs_node_info[] = {
	"cid",
	"life_time_est_typ_a",
	"life_time_est_typ_b",
	"manfid",
	"name",
	"pre_eol_info",
	"product_name",
	"rev",
	"size",
	"type",
	"specversion",
	"wb_enable"
};

static const struct proc_ops *proc_fops_list[] = {
	&cid_fops,
	&life_time_est_typ_a_fops,
	&life_time_est_typ_b_fops,
	&manfid_fops,
	&name_fops,
	&pre_eol_info_fops,
	&prod_name_fops,
	&rev_fops,
	&size_fops,
	&type_fops,
	&specversion_fops,
	&wb_fops,
};

int sprd_ufs_proc_init(struct ufs_hba *hba)
{
	struct proc_dir_entry *bootdevice_dir;
	struct proc_dir_entry *prEntry;
	struct device *dev = hba->dev;
	int i, node, err;
	u8 hlth_desc_buf[QUERY_DESC_MAX_SIZE];

	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	bootdevice_dir = proc_mkdir("bootdevice", NULL);
	if (!bootdevice_dir) {
		pr_err("%s: failed to create /proc/bootdevice\n",
			__func__);
		return -1;
	}
	node = ARRAY_SIZE(sprd_ufs_node_info);

	for (i = 0; i < node; i++) {
		prEntry = proc_create_data(sprd_ufs_node_info[i], 0444,
					   bootdevice_dir, proc_fops_list[i], dev);
		if (!prEntry) {
			pr_err("%s,failed to create node: /proc/bootdevice/%s\n",
				__func__, sprd_ufs_node_info[i]);
			return -1;
		}
	}

	/* Device Health Descriptor */
	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_HEALTH, 0, 0,
				     hlth_desc_buf,
				     hba->desc_size[QUERY_DESC_IDN_HEALTH]);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health Desc. err = %d\n",
			__func__, err);
		return err;
	}

	host->pre_eol_info =
		hlth_desc_buf[HEALTH_DESC_PARAM_EOL_INFO];
	host->life_time_est_typ_a =
		hlth_desc_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A];
	host->life_time_est_typ_b =
		hlth_desc_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_B];

	dev_info(hba->dev,
		 "elo_info:%x, life_time_estimation_a:%x, life_time_estimation_b:%x\n",
		 host->pre_eol_info,
		 host->life_time_est_typ_a,
		 host->life_time_est_typ_b);

	return 0;
}

void sprd_ufs_proc_exit(void)
{
	remove_proc_subtree("bootdevice", NULL);
}
