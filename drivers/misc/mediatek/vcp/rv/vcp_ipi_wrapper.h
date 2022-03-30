/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _VCP_IPI_WRAPPER_H_
#define _VCP_IPI_WRAPPER_H_

#include "vcp_ipi_pin.h"
#include "vcp_mbox_layout.h"

/* retry times * 1000 = 0x7FFF_FFFF, mbox wait maximum */
#define VCP_IPI_LEGACY_WAIT 0x20C49B

struct vcp_ipi_desc {
	void (*handler)(int id, void *data, unsigned int len);
};

#endif
