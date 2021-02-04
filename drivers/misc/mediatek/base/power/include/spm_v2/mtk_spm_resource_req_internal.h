/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_SPM_RESOURCE_REQ_INTERNAL_H__
#define __MTK_SPM_RESOURCE_REQ_INTERNAL_H__

/* SPM resource request APIs: for internal use */

unsigned int spm_get_resource_usage(void);
void spm_resource_req_dump(void);
bool spm_resource_req_init(void);
void spm_resource_req_debugfs_init(struct dentry *spm_dir);

#endif /* __MTK_SPM_RESOURCE_REQ_INTERNAL_H__ */
