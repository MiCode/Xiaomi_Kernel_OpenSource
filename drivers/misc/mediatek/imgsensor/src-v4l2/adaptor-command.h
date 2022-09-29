/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2022 MediaTek Inc. */

#ifndef __ADAPTOR_COMMAND_H__
#define __ADAPTOR_COMMAND_H__

long adaptor_command(struct v4l2_subdev *sd, unsigned int cmd, void *arg);

#endif
