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


/*****************************************************************************
 * Header Files
 *****************************************************************************/
#include "audio_ipi_platform.h"
#include "scp_ipi.h"

/*****************************************************************************
 * Function
 ****************************************************************************/
unsigned int audio_ipi_check_scp_status(void)
{
	return is_scp_ready(SCP_A_ID);
}

unsigned int get_audio_ipi_scp_location(void)
{
	return SCP_A_ID;
}

