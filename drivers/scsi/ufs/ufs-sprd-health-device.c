// SPDX-License-Identifier: GPL-2.0-only
/*
 * spreadtrum create in 2021/11/19
 *
 * ufs health report for vendor
 *
 * Base on YMTC
 */

#include "ufshcd.h"
#include "ufs-sprd.h"
#include "ufs-sysfs.h"
#include "ufs-sprd-health-device.h"

#define WRITE_BUFFER_LEN	0x2c
#define READ_BUFFER_LEN	0x1000

struct _ufshealth_data {
	u8 buf[4096];
};

static struct _ufshealth_data ufshealth_data;

static u32 get_sprd_u32_buf(int data)
{
	u8 temp_0 = *((u8 *)(&ufshealth_data.buf[data]));
	u8 temp_1 = *((u8 *)(&ufshealth_data.buf[data+1]));
	u8 temp_2 = *((u8 *)(&ufshealth_data.buf[data+2]));
	u8 temp_3 = *((u8 *)(&ufshealth_data.buf[data+3]));
	u32 real_val = 0;

	real_val = temp_3 | (temp_2 << 8) |
			(temp_1 << 16) | (temp_0 << 24);

	return real_val;
}

static void get_ufshealth_data(u8 *data, int buffer_len)
{
	memcpy(&ufshealth_data.buf[0], data, buffer_len);
}

#define UFS_HEALTH_DEVICE(name)	\
static int sprd_##name##_show(int data)	\
{	\
	return  get_sprd_u32_buf(data);	\
}

UFS_HEALTH_DEVICE(fbbc);
UFS_HEALTH_DEVICE(rb_num);
UFS_HEALTH_DEVICE(rtbb_esf);
UFS_HEALTH_DEVICE(rtbb_psf);
UFS_HEALTH_DEVICE(rtbb_uecc);
UFS_HEALTH_DEVICE(uecc_count);
UFS_HEALTH_DEVICE(read_reclaim_slc);
UFS_HEALTH_DEVICE(read_reclaim_tlc);
UFS_HEALTH_DEVICE(vdt_vccq);
UFS_HEALTH_DEVICE(vdt_vcc);
UFS_HEALTH_DEVICE(sudden_power_off_recovery_success);
UFS_HEALTH_DEVICE(sudden_power_off_recovery_fail);
UFS_HEALTH_DEVICE(min_ec_num_slc);
UFS_HEALTH_DEVICE(max_ec_num_slc);
UFS_HEALTH_DEVICE(ave_ec_num_slc);
UFS_HEALTH_DEVICE(min_ec_num_tlc);
UFS_HEALTH_DEVICE(max_ec_num_tlc);
UFS_HEALTH_DEVICE(ave_ec_num_tlc);
UFS_HEALTH_DEVICE(cumulative_host_read);
UFS_HEALTH_DEVICE(cumulative_host_write);
UFS_HEALTH_DEVICE(cumulative_initialization_count);
UFS_HEALTH_DEVICE(waf_total);
UFS_HEALTH_DEVICE(history_min_nand_temp);
UFS_HEALTH_DEVICE(history_max_nand_temp);
UFS_HEALTH_DEVICE(slc_used_life);
UFS_HEALTH_DEVICE(tlc_used_life);
UFS_HEALTH_DEVICE(ffu_success_cnt);
UFS_HEALTH_DEVICE(ffu_fail_cnt);
UFS_HEALTH_DEVICE(spare_slc_block_num);
UFS_HEALTH_DEVICE(spare_tlc_block_num);
UFS_HEALTH_DEVICE(max_temperature_counter_over_85c);
UFS_HEALTH_DEVICE(max_temperature_counter_over_125c);
UFS_HEALTH_DEVICE(rtbb_slc);
UFS_HEALTH_DEVICE(rtbb_tlc);
UFS_HEALTH_DEVICE(health_data);

int ufs_get_health_report(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct scsi_device *sdp;
	unsigned long flags;
	int ret;
	unsigned char cmd0[10] = {0};
	unsigned char cmd1[10] = {0};
	struct scsi_sense_hdr sshdr;
	uint8_t *read_buffer_data;
	unsigned char write_buffer_data[WRITE_BUFFER_LEN] = {0};

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdp = host->sdev_ufs_lu[0];
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

	read_buffer_data = kzalloc(READ_BUFFER_LEN, GFP_KERNEL);
	if (!read_buffer_data) {
		scsi_device_put(sdp);
		return -ENOMEM;
	}

	write_buffer_data[0] = 0xBB;
	write_buffer_data[1] = 0x40;
	write_buffer_data[2] = 0x10;

	cmd0[0] = WRITE_BUFFER;	/* 0x3B: WRITE_BUFFER */
	cmd0[1] = 0xE1;			/* MODE */
	cmd0[2] = 0x0;
	cmd0[3] = 0x0;
	cmd0[4] = 0x0;
	cmd0[5] = 0x0;
	cmd0[6] = 0x0;
	cmd0[7] = 0x0;
	cmd0[8] = 0x2c;			/* PARAMETER LIST LENGTH  */
	cmd0[9] = 0x0;

	ret = scsi_execute(sdp, cmd0, DMA_TO_DEVICE,
				write_buffer_data, WRITE_BUFFER_LEN, NULL, &sshdr,
				msecs_to_jiffies(1000), 3, 0, RQF_PM, NULL);
	if (ret) {
		sdev_printk(KERN_WARNING, sdp, "WRITE_BUFFER fail.\n");
		goto out;
	}

	cmd1[0] = READ_BUFFER;
	cmd1[1] = 0xC1;
	cmd1[2] = 0x0;
	cmd1[3] = 0x0;
	cmd1[4] = 0x0;
	cmd1[5] = 0x0;
	cmd1[6] = 0x0;
	cmd1[7] = 0x10;
	cmd1[8] = 0x0;
	cmd1[9] = 0x0;

	ret = scsi_execute(sdp, cmd1, DMA_FROM_DEVICE,
				read_buffer_data, READ_BUFFER_LEN, NULL, &sshdr,
				msecs_to_jiffies(1000), 3, 0, RQF_PM, NULL);
	if (ret) {
		sdev_printk(KERN_WARNING, sdp, "READ_BUFFER fail.\n");
		goto out;
	}

	get_ufshealth_data(read_buffer_data, 0x1000);

out:
	scsi_device_put(sdp);
	kfree(read_buffer_data);
	return ret;
}

static ssize_t factory_bad_block_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_fbbc_show(0);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t reserved_block_num_slc_tlc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}


	data = sprd_rb_num_show(4);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t rtbb_esf_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_rtbb_esf_show(12);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t rtbb_psf_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_rtbb_psf_show(16);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t rtbb_uecc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_rtbb_uecc_show(20);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t uecc_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err = -1;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_uecc_count_show(24);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t read_reclaim_slc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_read_reclaim_slc_show(36);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t read_reclaim_tlc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_read_reclaim_tlc_show(40);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t vdt_vccq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_vdt_vccq_show(44);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t vdt_vcc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_vdt_vcc_show(48);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t sudden_power_off_recovery_success_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_sudden_power_off_recovery_success_show(56);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t sudden_power_off_recovery_fail_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_sudden_power_off_recovery_fail_show(60);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t min_ec_num_slc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_min_ec_num_slc_show(68);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t max_ec_num_slc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_max_ec_num_slc_show(72);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t ave_ec_num_slc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_ave_ec_num_slc_show(76);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t min_ec_num_tlc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_min_ec_num_tlc_show(80);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t max_ec_num_tlc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_max_ec_num_tlc_show(84);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t ave_ec_num_tlc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_ave_ec_num_tlc_show(88);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t cumulative_host_read_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_cumulative_host_read_show(92);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t cumulative_host_write_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_cumulative_host_write_show(96);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t cumulative_initialization_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_cumulative_initialization_count_show(100);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t waf_tatal_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_waf_total_show(104);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t history_min_nand_temp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_history_min_nand_temp_show(144);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t history_max_nand_temp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_history_max_nand_temp_show(148);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t slc_used_life_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_slc_used_life_show(152);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t tlc_used_life_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_tlc_used_life_show(156);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t ffu_success_cnt_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_ffu_success_cnt_show(160);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t ffu_fail_cnt_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_ffu_fail_cnt_show(164);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t spare_slc_block_num_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_spare_slc_block_num_show(176);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t spare_tlc_block_num_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_spare_tlc_block_num_show(180);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t max_temperature_counter_over_85c_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_max_temperature_counter_over_85c_show(448);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t max_temperature_counter_over_125c_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_max_temperature_counter_over_125c_show(452);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t rtbb_slc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_rtbb_slc_show(456);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t rtbb_tlc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 data = 0;
	int err;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	data = sprd_rtbb_tlc_show(460);
	return snprintf(buf, PAGE_SIZE, "0x%x\n", data);
}

static ssize_t health_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	int err;
	u32 data;
	int i;
	int count = 0;

	err = ufs_get_health_report(hba);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting ufs health report. err = %d.\n",
			__func__, err);
		return err;
	}

	count += snprintf(buf + count, PAGE_SIZE,
			"All health report(4 byte size):\n");
	for (i = 0; i < 1000; i = i + 4) {
		data = sprd_health_data_show(i);
		count += snprintf(buf + count, PAGE_SIZE, "%08x ", data);
	}
	count += snprintf(buf + count, PAGE_SIZE, "\n");

	return count;
}

static DEVICE_ATTR_RO(factory_bad_block);
static DEVICE_ATTR_RO(reserved_block_num_slc_tlc);
static DEVICE_ATTR_RO(rtbb_esf);
static DEVICE_ATTR_RO(rtbb_psf);
static DEVICE_ATTR_RO(rtbb_uecc);
static DEVICE_ATTR_RO(uecc_count);
static DEVICE_ATTR_RO(read_reclaim_slc);
static DEVICE_ATTR_RO(read_reclaim_tlc);
static DEVICE_ATTR_RO(vdt_vccq);
static DEVICE_ATTR_RO(vdt_vcc);
static DEVICE_ATTR_RO(sudden_power_off_recovery_success);
static DEVICE_ATTR_RO(sudden_power_off_recovery_fail);
static DEVICE_ATTR_RO(min_ec_num_slc);
static DEVICE_ATTR_RO(max_ec_num_slc);
static DEVICE_ATTR_RO(ave_ec_num_slc);
static DEVICE_ATTR_RO(min_ec_num_tlc);
static DEVICE_ATTR_RO(max_ec_num_tlc);
static DEVICE_ATTR_RO(ave_ec_num_tlc);
static DEVICE_ATTR_RO(cumulative_host_read);
static DEVICE_ATTR_RO(cumulative_host_write);
static DEVICE_ATTR_RO(cumulative_initialization_count);
static DEVICE_ATTR_RO(waf_tatal);
static DEVICE_ATTR_RO(history_min_nand_temp);
static DEVICE_ATTR_RO(history_max_nand_temp);
static DEVICE_ATTR_RO(slc_used_life);
static DEVICE_ATTR_RO(tlc_used_life);
static DEVICE_ATTR_RO(ffu_success_cnt);
static DEVICE_ATTR_RO(ffu_fail_cnt);
static DEVICE_ATTR_RO(spare_slc_block_num);
static DEVICE_ATTR_RO(spare_tlc_block_num);
static DEVICE_ATTR_RO(max_temperature_counter_over_85c);
static DEVICE_ATTR_RO(max_temperature_counter_over_125c);
static DEVICE_ATTR_RO(rtbb_slc);
static DEVICE_ATTR_RO(rtbb_tlc);
static DEVICE_ATTR_RO(health_data);

static struct attribute *ufs_sysfs_health_report[] = {
	&dev_attr_factory_bad_block.attr,
	&dev_attr_reserved_block_num_slc_tlc.attr,
	&dev_attr_rtbb_esf.attr,
	&dev_attr_rtbb_psf.attr,
	&dev_attr_rtbb_uecc.attr,
	&dev_attr_uecc_count.attr,
	&dev_attr_read_reclaim_slc.attr,
	&dev_attr_read_reclaim_tlc.attr,
	&dev_attr_vdt_vccq.attr,
	&dev_attr_vdt_vcc.attr,
	&dev_attr_sudden_power_off_recovery_success.attr,
	&dev_attr_sudden_power_off_recovery_fail.attr,
	&dev_attr_min_ec_num_slc.attr,
	&dev_attr_max_ec_num_slc.attr,
	&dev_attr_ave_ec_num_slc.attr,
	&dev_attr_min_ec_num_tlc.attr,
	&dev_attr_max_ec_num_tlc.attr,
	&dev_attr_ave_ec_num_tlc.attr,
	&dev_attr_cumulative_host_read.attr,
	&dev_attr_cumulative_host_write.attr,
	&dev_attr_cumulative_initialization_count.attr,
	&dev_attr_waf_tatal.attr,
	&dev_attr_history_min_nand_temp.attr,
	&dev_attr_history_max_nand_temp.attr,
	&dev_attr_slc_used_life.attr,
	&dev_attr_tlc_used_life.attr,
	&dev_attr_ffu_success_cnt.attr,
	&dev_attr_ffu_fail_cnt.attr,
	&dev_attr_spare_slc_block_num.attr,
	&dev_attr_spare_tlc_block_num.attr,
	&dev_attr_max_temperature_counter_over_85c.attr,
	&dev_attr_max_temperature_counter_over_125c.attr,
	&dev_attr_rtbb_slc.attr,
	&dev_attr_rtbb_tlc.attr,
	&dev_attr_health_data.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_health_report_group = {
	.name = "health_report",
	.attrs = ufs_sysfs_health_report,
};

static const struct attribute_group *ufs_sysfs_health_device_group[] = {
	&ufs_sysfs_health_report_group,
	NULL,
};

void ufs_sprd_sysfs_add_health_device_nodes(struct ufs_hba *hba)
{
	int ret;

	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_YMTC) {
		ret = sysfs_create_groups(&hba->dev->kobj, ufs_sysfs_health_device_group);
		if (ret)
			dev_err(hba->dev, "%s: sprd sysfs health device groups creation failed(err = %d)\n",
				__func__, ret);
	}
}
EXPORT_SYMBOL_GPL(ufs_sprd_sysfs_add_health_device_nodes);
