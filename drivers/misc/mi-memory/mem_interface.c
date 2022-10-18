#include <linux/export.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/pm_runtime.h>
#include <linux/nls.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <asm/unaligned.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_request.h>
#include "mem_interface.h"

/* Query request retries */
#define QUERY_REQ_RETRIES 3

/**
 * struct uc_string_id - unicode string
 *
 * @len: size of this descriptor inclusive
 * @type: descriptor type
 * @uc: unicode string character
 */
struct uc_string_id {
	u8 len;
	u8 type;
	wchar_t uc[];
} __packed;

static struct UFS_DATA ufs_data;

struct UFS_DATA *get_ufs_data(void)
{
	return &ufs_data;
}

int ufs_read_desc_param(struct ufs_hba *hba, enum desc_idn desc_id, u8 desc_index, u8 param_offset, void* buf, u8 param_size)
{
	u8 desc_buf[8] = {0};
	int ret;

	if (param_size > 8)
		return -EINVAL;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, desc_id, desc_index,
				param_offset, desc_buf, param_size);
	pm_runtime_put_sync(hba->dev);

	if (ret)
		return -EINVAL;
	switch (param_size) {
	case 1:
		*(u8*)buf = *desc_buf;
		break;
	case 2:
		*(u16*)buf = get_unaligned_be16(desc_buf);
		break;
	case 4:
		*(u32*)buf =  get_unaligned_be32(desc_buf);
		break;
	case 8:
		*(u64*)buf= get_unaligned_be64(desc_buf);
		break;
	default:
		*(u8*)buf = *desc_buf;
		break;
	}

	return ret;
}