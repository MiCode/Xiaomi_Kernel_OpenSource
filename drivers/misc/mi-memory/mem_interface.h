#ifndef __MEM_INTERFACE_H__
#define __MEM_INTERFACE_H__

#include "../../scsi/ufs/ufshcd.h"

#define SD_ASCII_STD true
#define SD_RAW false

/*
 * customer debug interface
 */
struct ufs_err_state_debug {
	u64 err_occurred; /*if happend err*/
	char err_reason[10][32]; /*err reason*/
};

struct ufs_data {
	struct ufs_hba *hba;
	struct scsi_device *sdev;
	struct ufs_err_state_debug ufs_err_state;
};

u8 memblock_mem_size_in_gb(void);

struct ufs_hba *get_ufs_hba_data(void);

struct ufs_data *get_ufs_data(void);

struct scsi_device *get_ufs_sdev_data(void);

int ufs_get_string_desc(struct ufs_hba *hba, void* buf, int size, enum device_desc_param pname, bool ascii_std);

int ufs_read_desc_param(struct ufs_hba *hba, enum desc_idn desc_id, u8 desc_index, u8 param_offset, void* buf, u8 param_size);

int ufshcd_read_desc(struct ufs_hba *hba, enum desc_idn desc_id, int desc_index, void *buf, u32 size);

#endif