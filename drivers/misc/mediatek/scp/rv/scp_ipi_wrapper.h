/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _SCP_IPI_WRAPPER_H_
#define _SCP_IPI_WRAPPER_H_

#include "scp_ipi_pin.h"
#include "scp_mbox_layout.h"

/* retry times * 1000 = 0x7FFF_FFFF, mbox wait maximum */
#define SCP_IPI_LEGACY_WAIT 0x20C49B

struct scp_ipi_desc {
	void (*handler)(int id, void *data, unsigned int len);
};

#endif
