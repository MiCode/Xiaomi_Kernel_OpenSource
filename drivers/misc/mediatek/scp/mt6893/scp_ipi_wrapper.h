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

#ifndef _SCP_IPI_WRAPPER_H_
#define _SCP_IPI_WRAPPER_H_

#include "scp_ipi_pin.h"
#include "scp_mbox_layout.h"

/* retry times * 1000 = 0x7FFF_FFFF, mbox wait maximium */
#define SCP_IPI_LEGACY_WAIT 0x20C49B

struct scp_ipi_desc {
	void (*handler)(int id, void *data, unsigned int len);
};

/* the order of ipi_id should be consistent with IPI_LEGACY_GROUP */
enum ipi_id {
	IPI_MPOOL,
	IPI_CHRE,
	IPI_CHREX,
	IPI_SENSOR,
	SCP_NR_IPI,
};

#endif
