/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __SPM_MTK_SPM_RESOURCE_REQ_H__
#define __SPM_MTK_SPM_RESOURCE_REQ_H__

/* SPM resource request APIs: public */

enum {
	SPM_RESOURCE_RELEASE = 0,
	SPM_RESOURCE_MAINPLL = 1 << 0,
	SPM_RESOURCE_DRAM    = 1 << 1,
	SPM_RESOURCE_CK_26M  = 1 << 2,
	SPM_RESOURCE_AXI_BUS = 1 << 3,
	SPM_RESOURCE_CPU     = 1 << 4,
	NF_SPM_RESOURCE = 5,

	SPM_RESOURCE_ALL = (1 << NF_SPM_RESOURCE) - 1
};

enum {
	SPM_RESOURCE_USER_SPM = 0,
	SPM_RESOURCE_USER_UFS,
	SPM_RESOURCE_USER_SSUSB,
	SPM_RESOURCE_USER_AUDIO,
	SPM_RESOURCE_USER_UART,
	SPM_RESOURCE_USER_CONN,
	SPM_RESOURCE_USER_MSDC,
	SPM_RESOURCE_USER_SCP,
	NF_SPM_RESOURCE_USER
};

bool spm_resource_req(unsigned int user, unsigned int req_mask);

#endif /* __SPM_MTK_SPM_RESOURCE_REQ_H__ */
