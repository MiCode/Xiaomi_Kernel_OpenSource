/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MDLA_CMD_H__
#define __APUSYS_MDLA_CMD_H__

int parse_mdla_codebuf_info(struct apusys_subcmd *sc,
	struct apusys_cmd_hnd *hnd);
int set_multimdla_codebuf(struct apusys_subcmd *sc,
	struct apusys_cmd_hnd *hnd, int idx);
int check_multimdla_support(struct apusys_subcmd *sc);

#endif
