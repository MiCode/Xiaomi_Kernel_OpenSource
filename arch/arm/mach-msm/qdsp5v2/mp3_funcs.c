/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <linux/msm_audio.h>

#include <mach/qdsp5v2/qdsp5audppmsg.h>
#include <mach/qdsp5v2/qdsp5audplaycmdi.h>
#include <mach/qdsp5v2/qdsp5audplaymsg.h>
#include <mach/qdsp5v2/audpp.h>
#include <mach/qdsp5v2/codec_utils.h>
#include <mach/qdsp5v2/mp3_funcs.h>
#include <mach/debug_mm.h>

long mp3_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	MM_DBG("mp3_ioctl() cmd = %d\b", cmd);

	return -EINVAL;
}

void audpp_cmd_cfg_mp3_params(struct audio *audio)
{
	struct audpp_cmd_cfg_adec_params_mp3 cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_CMD_CFG_ADEC_PARAMS;
	cmd.common.length = AUDPP_CMD_CFG_ADEC_PARAMS_MP3_LEN;
	cmd.common.dec_id = audio->dec_id;
	cmd.common.input_sampling_frequency = audio->out_sample_rate;

	audpp_send_queue2(&cmd, sizeof(cmd));
}
