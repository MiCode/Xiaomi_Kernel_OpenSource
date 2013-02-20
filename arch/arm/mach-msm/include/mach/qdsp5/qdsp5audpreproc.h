/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#ifndef QDSP5AUDPREPROC_H
#define _QDSP5AUDPREPROC_H

#include <mach/qdsp5/qdsp5audpreproccmdi.h>
#include <mach/qdsp5/qdsp5audpreprocmsg.h>

#define MSM_AUD_ENC_MODE_TUNNEL  0x00000100
#define MSM_AUD_ENC_MODE_NONTUNNEL  0x00000200

#define AUDPREPROC_CODEC_MASK 0x00FF
#define AUDPREPROC_MODE_MASK 0xFF00

#define MSM_ADSP_ENC_MODE_TUNNEL 24
#define MSM_ADSP_ENC_MODE_NON_TUNNEL 25

/* Exported common api's from audpreproc layer */
int audpreproc_aenc_alloc(unsigned enc_type, const char **module_name,
		unsigned *queue_id);
void audpreproc_aenc_free(int enc_id);

#endif /* QDSP5AUDPREPROC_H */
