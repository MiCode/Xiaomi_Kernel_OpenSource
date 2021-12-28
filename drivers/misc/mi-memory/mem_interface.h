#ifndef __MEM_INTERFACE_H__
#define __MEM_INTERFACE_H__

#include <scsi/scsi_device.h>
#include <scsi/scsi_common.h>

#include "../../scsi/ufs/ufshcd.h"

#define SD_ASCII_STD true
#define SD_RAW false

/*
 * customer debug interface
 */
struct UFS_ERR_STATE_DEBUG {
	u64 err_occurred; /*if happend err*/
	char err_reason[10][32]; /*err reason*/
};

struct UFS_DATA {
	struct UFS_ERR_STATE_DEBUG ufs_err_state;
};

struct UFS_DATA *get_ufs_data(void);

int ufs_read_desc_param(struct ufs_hba *hba, enum desc_idn desc_id, u8 desc_index, u8 param_offset, void* buf, u8 param_size);

#endif
