/*
 * mi_cld_sysfs.h
 *
 *  Created on: 2020年10月23日
 *      Author: shane
 */

#ifndef DRIVERS_SCSI_UFS_MI_CLD_SYSFS_H_
#define DRIVERS_SCSI_UFS_MI_CLD_SYSFS_H_


 int ufscld_create_sysfs(struct ufscld_dev *cld);
 void ufscld_remove_sysfs(struct ufscld_dev *cld);

#endif /* DRIVERS_SCSI_UFS_MI_CLD_SYSFS_H_ */
