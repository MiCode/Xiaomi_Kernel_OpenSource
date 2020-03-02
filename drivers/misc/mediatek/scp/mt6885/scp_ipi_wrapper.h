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

/* define group id in bitwise which length is various */
/* SCP_IN_WRAPPER_VAR, IPI_IN_XXXX must < 32, otherwise it'll overflow */
#define SCP_IN_WRAPPER_VAR ((1 << IPI_IN_CHRE_0) | (1 << IPI_IN_SENSOR_0))
#define SCP_OUT_WRAPPER_VAR ((1 << IPI_CHRE) | (1 << IPI_SENSOR) \
			     | (1 << IPI_CHREX))
/* retry times * 1000 = 0x7FFF_FFFF, mbox wait maximium */
#define SCP_IPI_LEGACY_WAIT 0x20C49B

static char *msg_legacy_ipi_chre[PIN_IN_SIZE_CHRE_0 * MBOX_SLOT_SIZE];
static char *msg_legacy_ipi_sensor[PIN_IN_SIZE_SENSOR_0 * MBOX_SLOT_SIZE];
static char *msg_legacy_ipi_error_info[PIN_IN_SIZE_SCP_ERROR_INFO_0
				       * MBOX_SLOT_SIZE];
static char *msg_legacy_ipi_apccci[PIN_IN_SIZE_APCCCI_0 * MBOX_SLOT_SIZE];
static char *msg_legacy_ipi_audio_vow[PIN_IN_SIZE_AUDIO_VOW_1 * MBOX_SLOT_SIZE];

/* the order of ipi_id should be consistent with IPI_LEGACY_GROUP */
enum ipi_id {
	IPI_CHRE,
	IPI_CHREX,
	IPI_SENSOR,
	IPI_SCP_ERROR_INFO,
	IPI_APCCCI,
	IPI_DVFS_SLEEP,
	IPI_DVFS_SET_FREQ,
	IPI_AUDIO,
	SCP_NR_IPI,
};

#define SCP_IPI_LEGACY_GROUP				  \
{							  \
	{	.out_id_0 = IPI_OUT_CHRE_0,		  \
		.in_id_0 = IPI_IN_CHRE_0,		  \
		.out_size = PIN_OUT_SIZE_CHRE_0,	  \
		.in_size = PIN_IN_SIZE_CHRE_0,		  \
		.msg = msg_legacy_ipi_chre,		  \
	},						  \
	{	.out_id_0 = IPI_OUT_CHREX_0,		  \
		.out_size = PIN_OUT_SIZE_CHREX_0,	  \
	},						  \
	{	.out_id_0 = IPI_OUT_SENSOR_0,		  \
		.in_id_0 = IPI_IN_SENSOR_0,		  \
		.out_size = PIN_OUT_SIZE_SENSOR_0,	  \
		.in_size = PIN_IN_SIZE_SENSOR_0,	  \
		.msg = msg_legacy_ipi_sensor,		  \
	},						  \
	{	.in_id_0 = IPI_IN_SCP_ERROR_INFO_0,	  \
		.in_size = PIN_IN_SIZE_SCP_ERROR_INFO_0,  \
		.msg = msg_legacy_ipi_error_info,	  \
	},						  \
	{	.out_id_0 = IPI_OUT_APCCCI_0,		  \
		.in_id_0 = IPI_IN_APCCCI_0,		  \
		.out_size = PIN_OUT_SIZE_APCCCI_0,	  \
		.in_size = PIN_IN_SIZE_APCCCI_0,	  \
		.msg = msg_legacy_ipi_apccci,		  \
	},						  \
	{	.out_id_0 = IPI_OUT_DVFS_SLEEP_0,	  \
		.out_id_1 = IPI_OUT_DVFS_SLEEP_1,	  \
		.out_size = PIN_OUT_SIZE_DVFS_SLEEP_0,	  \
	},						  \
	{	.out_id_0 = IPI_OUT_DVFS_SET_FREQ_0,	  \
		.out_id_1 = IPI_OUT_DVFS_SET_FREQ_1,	  \
		.out_size = PIN_OUT_SIZE_DVFS_SET_FREQ_0, \
	},						  \
	{	.out_id_1 = IPI_OUT_AUDIO_VOW_1,	  \
		.in_id_1 = IPI_IN_AUDIO_VOW_1,		  \
		.out_size = PIN_OUT_SIZE_AUDIO_VOW_1,	  \
		.in_size = PIN_IN_SIZE_AUDIO_VOW_1,	  \
		.msg = msg_legacy_ipi_audio_vow,	  \
	},						  \
}

#endif
