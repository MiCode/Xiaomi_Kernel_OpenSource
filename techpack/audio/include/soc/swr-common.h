/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015, 2017-2020 The Linux Foundation. All rights reserved.
 */

#ifndef _LINUX_SWR_COMMON_H
#define _LINUX_SWR_COMMON_H
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/bitops.h>

enum {
	SWR_UC0 = 0,
	SWR_UC1,
	SWR_UC2,
	SWR_UC_MAX,
};

struct port_params {
	u16 si;
	u8 off1;
	u8 off2;
	u8 hstart;/* head start */
	u8 hstop; /* head stop */
	u8 wd_len;/* word length */
	u8 bp_mode; /* block pack mode */
	u8 bgp_ctrl;/* block group control */
	u8 lane_ctrl;/* lane to be used */
	u8 dir; /* master port dir: OUT = 0 IN = 1 */
	u8 stream_type; /* PDM = 0, PCM = 1 */
};

struct swrm_port_config {
	u32 size;
	u32 uc;
	void *params;
};

struct swr_mstr_port_map {
	u32 id;
	u32 uc;
	struct port_params *swr_port_params;
};

#define SWR_MSTR_PORT_LEN      8 /* Number of master ports */

#endif /* _LINUX_SWR_COMMON_H */
