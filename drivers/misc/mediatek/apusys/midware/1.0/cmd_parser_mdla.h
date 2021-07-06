/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __APUSYS_MDLA_CMD_H__
#define __APUSYS_MDLA_CMD_H__

int parse_mdla_codebuf_info(struct apusys_subcmd *sc,
	struct apusys_cmd_hnd *hnd);
int set_multimdla_codebuf(struct apusys_subcmd *sc,
	struct apusys_cmd_hnd *hnd, int idx);
int check_multimdla_support(struct apusys_subcmd *sc);

#endif
