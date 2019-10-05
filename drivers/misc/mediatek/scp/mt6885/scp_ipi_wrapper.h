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

static char msg_legacy_ipi_chre[PIN_IN_SIZE_CHRE_0 * MBOX_SLOT_SIZE];
static char msg_legacy_ipi_sensor[PIN_IN_SIZE_SENSOR_0 * MBOX_SLOT_SIZE];
static char msg_legacy_ipi_mpool_0[PIN_IN_SIZE_SCP_MPOOL * MBOX_SLOT_SIZE];
static char msg_legacy_ipi_mpool_1[PIN_IN_SIZE_SCP_MPOOL * MBOX_SLOT_SIZE];

/* the order of ipi_id should be consistent with IPI_LEGACY_GROUP */
enum ipi_id {
	IPI_CHRE,
	IPI_CHREX,
	IPI_SENSOR,
	IPI_MPOOL,
	SCP_NR_IPI,
};

#define SCP_IPI_LEGACY_GROUP				  \
{							  \
	{	.out_id_0 = IPI_OUT_CHRE_0,		  \
		.in_id_0 = IPI_IN_CHRE_0,		  \
		.out_size = PIN_OUT_SIZE_CHRE_0,	  \
		.in_size = PIN_IN_SIZE_CHRE_0,		  \
		.msg_0 = msg_legacy_ipi_chre,		  \
	},						  \
	{	.out_id_0 = IPI_OUT_CHREX_0,		  \
		.out_size = PIN_OUT_SIZE_CHREX_0,	  \
	},						  \
	{	.out_id_0 = IPI_OUT_SENSOR_0,		  \
		.in_id_0 = IPI_IN_SENSOR_0,		  \
		.out_size = PIN_OUT_SIZE_SENSOR_0,	  \
		.in_size = PIN_IN_SIZE_SENSOR_0,	  \
		.msg_0 = msg_legacy_ipi_sensor,		  \
	},						  \
	{	.out_id_0 = IPI_OUT_SCP_MPOOL_0,	  \
		.out_id_1 = IPI_OUT_SCP_MPOOL_1,	  \
		.in_id_0 = IPI_IN_SCP_MPOOL_0,		  \
		.in_id_1 = IPI_IN_SCP_MPOOL_1,		  \
		.out_size = PIN_OUT_SIZE_SCP_MPOOL,	  \
		.in_size = PIN_IN_SIZE_SCP_MPOOL,	  \
		.msg_0 = msg_legacy_ipi_mpool_0,	  \
		.msg_1 = msg_legacy_ipi_mpool_1,	  \
	},						  \
}

#endif
