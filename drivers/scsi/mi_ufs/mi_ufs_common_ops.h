/*
 * mi_ufs_common_ops.h
 *
 *  Created on: 2020年10月22日
 *      Author: shane
 */

#ifndef DRIVERS_SCSI_UFS_MI_UFS_COMMON_OPS_H_
#define DRIVERS_SCSI_UFS_MI_UFS_COMMON_OPS_H_

#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>
#include <linux/blkdev.h>
#include <linux/bitfield.h>
#include <scsi/scsi_cmnd.h>

#include "../../block/blk.h"

int mi_ufs_read_desc(struct ufs_hba *hba,
		   enum desc_idn desc_id,   int desc_index,  u8 selector,  u8 param_offset,   u8 *param_read_buf,   u8 param_size);


int mi_ufs_query_flag(struct ufs_hba *hba,
	enum query_opcode opcode, enum flag_idn idn, u8 index, u8 selector, bool *flag_res);


int mi_ufs_query_attr(struct ufs_hba *hba, enum query_opcode opcode, u8 idn, u8 idx, u8 selector, u32 *attr_val);

#endif /* DRIVERS_SCSI_UFS_MI_UFS_COMMON_OPS_H_ */
