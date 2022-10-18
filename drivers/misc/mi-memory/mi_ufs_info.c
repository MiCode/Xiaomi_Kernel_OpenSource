#include <soc/qcom/socinfo.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_common.h>
#include <asm/unaligned.h>
#include "mi_memory_sysfs.h"
#include "mem_interface.h"
#include "../../scsi/ufs/ufs-qcom.h"

#define SCSI_LUN 		0

static struct scsi_device *sdev = NULL;

enum field_width {
	BYTE	= 1,
	WORD	= 2,
	DWORD   = 4,
};

struct desc_field_offset {
	char *name;
	int offset;
	enum field_width width_byte;
};

static void look_up_scsi_device(int lun)
{
	struct Scsi_Host *shost;

	shost = scsi_host_lookup(0);
	if (!shost)
		return;
	sdev = scsi_device_lookup(shost, 0, 0, lun);

	pr_info("[mi_ufs_info] scsi device proc name is %s\n", sdev->host->hostt->proc_name);

	scsi_device_put(sdev);

	scsi_host_put(shost);
}

struct ufs_hba *get_ufs_hba_data(void)
{
	if(!sdev)
		look_up_scsi_device(SCSI_LUN);

	return shost_priv(sdev->host);
}

static int ufshcd_read_desc(struct ufs_hba *hba, enum desc_idn desc_id, int desc_index, void *buf, u32 size)
{
	int ret = 0;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, desc_id, desc_index, 0, buf, size);
	pm_runtime_put_sync(hba->dev);

	return ret;
}

static ssize_t dump_health_desc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 value = 0;
	int count = 0, i = 0;
	u8 buff_len = 0;
	u8 *desc_buf = NULL;

	struct ufs_hba *hba = get_ufs_hba_data();

	struct desc_field_offset health_desc_field_name[] = {
		{"bLength", 			HEALTH_DESC_PARAM_LEN, 				BYTE},
		{"bDescriptorType", 	HEALTH_DESC_PARAM_TYPE, 			BYTE},
		{"bPreEOLInfo", 		HEALTH_DESC_PARAM_EOL_INFO, 		BYTE},
		{"bDeviceLifeTimeEstA", HEALTH_DESC_PARAM_LIFE_TIME_EST_A, 	BYTE},
		{"bDeviceLifeTimeEstB", HEALTH_DESC_PARAM_LIFE_TIME_EST_B, 	BYTE},
	};

	struct desc_field_offset *tmp = NULL;

	ufs_read_desc_param(hba, QUERY_DESC_IDN_HEALTH, 0, HEALTH_DESC_PARAM_LEN, &buff_len, BYTE);

	desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!desc_buf) {
		count += snprintf((buf + count), PAGE_SIZE, "get health info fail\n");
		return count;
	}

	ufshcd_read_desc(hba, QUERY_DESC_IDN_HEALTH, 0, desc_buf, buff_len);

	for (i = 0; i < ARRAY_SIZE(health_desc_field_name); ++i) {
		u8 *ptr = NULL;

		tmp = &health_desc_field_name[i];

		ptr = desc_buf + tmp->offset;

		switch (tmp->width_byte) {
			case BYTE:
				value = (u16)(*ptr);
				break;
			case WORD:
				value = *(u16 *)(ptr);
				break;
			default:
				value = (u16)(*ptr);
				break;
		}

		count += snprintf((buf + count), PAGE_SIZE, "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
			tmp->offset, tmp->name, value);
	}

	kfree(desc_buf);

	return count;

}

static DEVICE_ATTR_RO(dump_health_desc);

static struct attribute *ufshcd_sysfs[] = {
	&dev_attr_dump_health_desc.attr,
	NULL,
};

const struct attribute_group ufshcd_sysfs_group = {
	.name = "ufshcd0",
	.attrs = ufshcd_sysfs,
};
