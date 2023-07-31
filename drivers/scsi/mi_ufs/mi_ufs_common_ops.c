/*
 * mi_ufs_ops.c
 *
 *  Created on: 2020年10月20日
 *      Author: shane
 */

#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>
#include <linux/blkdev.h>
#include <linux/bitfield.h>
#include <scsi/scsi_cmnd.h>

#include "../../block/blk.h"
#include "mi-ufshcd.h"

/*implement mi_ufs_query_desc in the future*/
int mi_ufs_read_desc(struct ufs_hba *hba, enum desc_idn desc_id,   int desc_index,  u8 selector,  u8 param_offset,   void *param_read_buf,   u32 param_size)
{
	return ufshcd_read_desc_param_sel(hba, desc_id, desc_index, selector, param_offset, param_read_buf, param_size);
}


int mi_ufs_query_flag(struct ufs_hba *hba,
	enum query_opcode opcode, enum flag_idn idn, u8 index, u8 selector, bool *flag_res)
{
	int ret;
	int retries;

	for (retries = 0; retries < 3; retries++) {
		ret = ufshcd_query_flag_sel(hba, opcode, idn, index, selector, flag_res);
		if (ret){
			dev_err(hba->dev,
				"%s: failed with error %d, retries %d, opcode %d, idn %d\n",
				__func__, ret, retries, opcode, idn);
		} else {
			dev_err(hba->dev,
				"%s: query attribute, opcode %d, idn %d, success\n",
				__func__, opcode, idn);
			break;
        }
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query attribute, opcode %d, idn %d, failed with error %d after %d retires\n",
			__func__, opcode, idn, ret, retries);
	return ret;
}


int mi_ufs_query_attr(struct ufs_hba *hba, enum query_opcode opcode,  enum attr_idn idn, u8 idx, u8 selector, u32 *attr_val)
{
	int ret = 0;
	int retries;

	pm_runtime_get_sync(hba->dev);
	for (retries = 0; retries < 3; retries++) {
		ret = ufshcd_query_attr(hba, opcode, idn, idx,
				selector, attr_val);
		if (ret)
			dev_dbg(hba->dev,
				"%s: failed with error %d, retries %d\n",
				__func__, ret, retries);
		else
			break;
	}

	if (ret) {
		dev_err(hba->dev,
				"%s: mi query attr, opcode %d, idn %d, failed with error %d after %d retires\n",
				__func__, opcode, idn, ret, retries);
		goto err_out;
	}

err_out:
	pm_runtime_put_sync(hba->dev);
	return ret;
}
