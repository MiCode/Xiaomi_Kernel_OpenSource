#include <linux/memblock.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/pm_runtime.h>
#include <asm/unaligned.h>
#include <scsi/scsi_host.h>
#include "mem_interface.h"

static struct ufs_data *ufs_data_ptr;

void set_ufs_hba_data(struct scsi_device *sdev)
{
	if (!ufs_data_ptr) {
		ufs_data_ptr = kzalloc(sizeof(struct ufs_data), GFP_ATOMIC);
		if (!ufs_data_ptr)
			return;
	}

	if (sdev && !ufs_data_ptr->sdev) {
		ufs_data_ptr->sdev = sdev;
		ufs_data_ptr->hba = shost_priv(sdev->host);
	}
}

struct ufs_hba *get_ufs_hba_data(void)
{
	return ufs_data_ptr->hba;
}

EXPORT_SYMBOL(get_ufs_hba_data);

struct scsi_device *get_ufs_sdev_data(void)
{
	return ufs_data_ptr->sdev;
}

EXPORT_SYMBOL(get_ufs_sdev_data);

struct ufs_data *get_ufs_data(void)
{
	return ufs_data_ptr;
}

EXPORT_SYMBOL(get_ufs_data);

void ufsdbg_set_err_state(char *err_reason)
{
	int offset = 9;

	ufs_data_ptr->ufs_err_state.err_occurred++;
#ifdef CONFIG_DEBUG_FS
    // For Compatible with Memory debugfs interface
    if(!ufs_data_ptr->hba->debugfs_files.err_occurred
       && (ufs_data_ptr->ufs_err_state.err_occurred > 0)){
        ufs_data_ptr->hba->debugfs_files.err_occurred = true;
    }
#endif
	while (offset) {
		memcpy(ufs_data_ptr->ufs_err_state.err_reason + offset, ufs_data_ptr->ufs_err_state.err_reason + offset - 1, 32);
		offset--;
	}
	memcpy(ufs_data_ptr->ufs_err_state.err_reason, err_reason, 32);
	dump_stack();
}

/*obtain ddr size*/
u8 memblock_mem_size_in_gb(void)
{
	return (u8)((memblock_phys_mem_size() + memblock_reserved_size()) / 1024 / 1024 / 1024);
}

EXPORT_SYMBOL(memblock_mem_size_in_gb);

int ufshcd_read_desc(struct ufs_hba *hba, enum desc_idn desc_id, int desc_index, void *buf, u32 size)
{
	int ret = 0;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, desc_id, desc_index, 0, buf, size);
	pm_runtime_put_sync(hba->dev);

	return ret;
}

EXPORT_SYMBOL(ufshcd_read_desc);

int ufs_get_string_desc(struct ufs_hba *hba, void* buf, int size, enum device_desc_param pname, bool ascii_std)
{
	u8 index;
	int ret = 0;

	int desc_len = QUERY_DESC_MAX_SIZE;
	u8 *desc_buf;

	desc_buf = kzalloc(QUERY_DESC_MAX_SIZE, GFP_ATOMIC);
	if (!desc_buf)
		return -ENOMEM;
	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_query_descriptor_retry(hba,
		UPIU_QUERY_OPCODE_READ_DESC, QUERY_DESC_IDN_DEVICE,
		0, 0, desc_buf, &desc_len);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}
	index = desc_buf[pname];
	kfree(desc_buf);
	desc_buf = NULL;
	ret = ufshcd_read_string_desc(hba, index, desc_buf, desc_len, ascii_std);
	if (ret < 0)
		goto out;
	memcpy(buf, desc_buf, size);
out:
	pm_runtime_put_sync(hba->dev);
	kfree(desc_buf);
	return ret;
}

EXPORT_SYMBOL(ufs_get_string_desc);

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

EXPORT_SYMBOL(ufs_read_desc_param);
