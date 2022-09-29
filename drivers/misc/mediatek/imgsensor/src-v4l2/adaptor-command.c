// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.


#include "mtk_camera-v4l2-controls.h"
#include "adaptor.h"
#include "adaptor-fsync-ctrls.h"

#include "adaptor-command.h"


#define sd_to_ctx(__sd) container_of(__sd, struct adaptor_ctx, sd)


/*---------------------------------------------------------------------------*/
// functions that called by in-kernel drivers.
/*---------------------------------------------------------------------------*/
/* GET */

/* SET */
static int s_cmd_fsync_sync_frame_start_end(struct adaptor_ctx *ctx, void *arg)
{
	int *p_flag = NULL;
	int ret = 0;

	/* error handling (unexpected case) */
	if (unlikely(arg == NULL)) {
		ret = -ENOIOCTLCMD;
		adaptor_logi(ctx,
			"ERROR: V4L2_CMD_FSYNC_SYNC_FRAME_START_END, idx:%d, input arg is nullptr, return:%d\n",
			ctx->idx);
		return ret;
	}

	/* casting arg to int-pointer for using */
	p_flag = arg;

	adaptor_logd(ctx,
		"V4L2_CMD_FSYNC_SYNC_FRAME_START_END, idx:%d, flag:%d\n",
		ctx->idx, *p_flag);

	notify_fsync_mgr_sync_frame(ctx, *p_flag);

	return ret;
}


/*---------------------------------------------------------------------------*/
// adaptor command framework/entry
/*---------------------------------------------------------------------------*/

struct command_entry {
	unsigned int cmd;
	int (*func)(struct adaptor_ctx *ctx, void *arg);
};

static const struct command_entry command_list[] = {
	/* GET */

	/* SET */
	{V4L2_CMD_FSYNC_SYNC_FRAME_START_END, s_cmd_fsync_sync_frame_start_end},
};

long adaptor_command(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct adaptor_ctx *ctx = NULL;
	int i, ret = -ENOIOCTLCMD;

	/* error handling (unexpected case) */
	if (unlikely(sd == NULL)) {
		ret = -ENOIOCTLCMD;
		pr_info(
			"[%s] ERROR: get nullptr of v4l2_subdev (sd), return:%d   [cmd id:%#x]\n",
			__func__, ret, cmd);
		return ret;
	}

	ctx = sd_to_ctx(sd);

	adaptor_logd(ctx,
		"dispatch command request, idx:%d, cmd id:%#x\n",
		ctx->idx, cmd);

	/* dispatch command request */
	for (i = 0; i < ARRAY_SIZE(command_list); i++) {
		if (command_list[i].cmd == cmd) {
			ret = command_list[i].func(ctx, arg);
			break;
		}
	}

	return ret;
}

