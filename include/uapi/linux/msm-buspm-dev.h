/* Copyright (c) 2011,2014-15 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_BUSPM_DEV_H__
#define __MSM_BUSPM_DEV_H__

#include <linux/ioctl.h>

struct msm_buspm_map_dev {
	void            *vaddr;
	unsigned long   paddr;
	size_t          buflen;
};

/* Read/write data into kernel buffer */
struct buspm_xfer_req {
	unsigned int  size;		/* Size of this request, in bytes */
	void *data;		/* Data buffer to transfer data to/from */
};

struct buspm_alloc_params {
	size_t  size;
};

enum msm_buspm_ioc_cmds {
	MSM_BUSPM_SPDM_CLK_DIS = 0,
	MSM_BUSPM_SPDM_CLK_EN,
};

#define MSM_BUSPM_IOC_MAGIC	'p'

#define MSM_BUSPM_IOC_FREE	\
	_IOW(MSM_BUSPM_IOC_MAGIC, 0, void *)

#define MSM_BUSPM_IOC_ALLOC	\
	_IOW(MSM_BUSPM_IOC_MAGIC, 1, uint32_t)

#define MSM_BUSPM_IOC_RDBUF	\
	_IOW(MSM_BUSPM_IOC_MAGIC, 2, struct buspm_xfer_req)

#define MSM_BUSPM_IOC_WRBUF	\
	_IOW(MSM_BUSPM_IOC_MAGIC, 3, struct buspm_xfer_req)

#define MSM_BUSPM_IOC_RD_PHYS_ADDR	\
	_IOR(MSM_BUSPM_IOC_MAGIC, 4, uint32_t)

#define MSM_BUSPM_IOC_CMD	\
	_IOR(MSM_BUSPM_IOC_MAGIC, 5, uint32_t)
#endif
